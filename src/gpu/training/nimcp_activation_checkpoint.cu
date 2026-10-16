/**
 * @file nimcp_activation_checkpoint.cu
 * @brief High-Level Activation Checkpointing Implementation
 *
 * WHAT: Implementation of high-level activation checkpointing for sequential models
 * WHY:  Provides easy-to-use API for gradient checkpointing in common use cases
 * HOW:  Wraps low-level checkpoint context with layer-aware management
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/training/nimcp_activation_checkpoint.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "ACTIVATION_CKPT"

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Clone a GPU tensor
 */
static nimcp_gpu_tensor_t* clone_tensor(nimcp_gpu_context_t* ctx,
                                         const nimcp_gpu_tensor_t* src) {
    if (!ctx || !src) return NULL;

    nimcp_gpu_tensor_t* dst = nimcp_gpu_tensor_create(ctx, src->dims, src->ndim, src->precision);
    if (!dst) return NULL;

    cudaError_t err = cudaMemcpy(dst->data, src->data,
                                  src->numel * src->elem_size,
                                  cudaMemcpyDeviceToDevice);
    if (err != cudaSuccess) {
        nimcp_gpu_tensor_destroy(dst);
        return NULL;
    }

    return dst;
}

/**
 * @brief Calculate tensor size in bytes
 */
static size_t tensor_size_bytes(const nimcp_gpu_tensor_t* tensor) {
    if (!tensor) return 0;
    return tensor->numel * tensor->elem_size;
}

/**
 * @brief Internal segment forward wrapper for recomputation
 */
typedef struct seq_recompute_ctx {
    nimcp_sequential_checkpoint_t* ckpt;
    nimcp_seq_layer_forward_fn_t layer_forward;
    void* forward_ctx;
    int start_layer;
    int end_layer;
} seq_recompute_ctx_t;

static void seq_segment_forward(void* ctx,
                                 nimcp_gpu_tensor_t* input,
                                 nimcp_gpu_tensor_t* output) {
    seq_recompute_ctx_t* rctx = (seq_recompute_ctx_t*)ctx;
    if (!rctx || !rctx->ckpt || !rctx->layer_forward) return;

    nimcp_sequential_checkpoint_t* ckpt = rctx->ckpt;
    nimcp_gpu_tensor_t* current = input;
    nimcp_gpu_tensor_t* temp_output = NULL;

    // Run forward through each layer in the segment
    for (int i = rctx->start_layer; i <= rctx->end_layer; i++) {
        // Allocate output tensor for this layer
        if (i < rctx->end_layer) {
            // Intermediate layer - allocate temporary
            temp_output = nimcp_gpu_tensor_create(ckpt->gpu_ctx,
                                                   current->dims,
                                                   current->ndim,
                                                   current->precision);
        } else {
            // Final layer - use provided output
            temp_output = output;
        }

        if (!temp_output) {
            LOG_ERROR("Failed to allocate output for layer %d", i);
            return;
        }

        // Run layer forward
        void* layer_ctx = (i < ckpt->num_layers && ckpt->layer_contexts) ?
                          ckpt->layer_contexts[i] : rctx->forward_ctx;
        rctx->layer_forward(i, layer_ctx, current, temp_output);

        // Store activation if this is a checkpoint boundary
        if (nimcp_checkpoint_is_checkpoint_layer(ckpt->ckpt_ctx, i + 1)) {
            if (ckpt->activations[i + 1]) {
                nimcp_gpu_tensor_destroy(ckpt->activations[i + 1]);
            }
            ckpt->activations[i + 1] = clone_tensor(ckpt->gpu_ctx, temp_output);
            ckpt->activation_stored[i + 1] = true;
        }

        // Free previous temp if not input
        if (current != input && i > rctx->start_layer) {
            // Previous temporary was freed when we moved to next layer
        }

        current = temp_output;
    }
}

//=============================================================================
// Configuration API
//=============================================================================

void nimcp_seq_checkpoint_config_init(nimcp_seq_checkpoint_config_t* config) {
    if (!config) return;

    config->strategy = CKPT_STRATEGY_SQRT;
    config->checkpoint_every_n = 0;  // Will be computed
    config->memory_budget = 0;       // Unlimited
    config->preserve_rng = true;
    config->enable_profiling = false;
    config->verbose = false;
    config->auto_configure = true;
}

//=============================================================================
// Lifecycle API
//=============================================================================

nimcp_sequential_checkpoint_t* nimcp_sequential_checkpoint_create(
    nimcp_gpu_context_t* gpu_ctx,
    int num_layers,
    size_t memory_budget)
{
    nimcp_seq_checkpoint_config_t config;
    nimcp_seq_checkpoint_config_init(&config);
    config.memory_budget = memory_budget;

    return nimcp_sequential_checkpoint_create_with_config(gpu_ctx, num_layers, &config);
}

nimcp_sequential_checkpoint_t* nimcp_sequential_checkpoint_create_with_config(
    nimcp_gpu_context_t* gpu_ctx,
    int num_layers,
    const nimcp_seq_checkpoint_config_t* config)
{
    if (!gpu_ctx || num_layers <= 0) {
        LOG_ERROR("Invalid parameters: gpu_ctx=%p, num_layers=%d",
                  (void*)gpu_ctx, num_layers);
        return NULL;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    nimcp_sequential_checkpoint_t* ckpt = (nimcp_sequential_checkpoint_t*)nimcp_calloc(1,
        sizeof(nimcp_sequential_checkpoint_t));
    if (!ckpt) {
        LOG_ERROR("Failed to allocate sequential checkpoint context");
        return NULL;
    }

    ckpt->gpu_ctx = gpu_ctx;
    ckpt->num_layers = num_layers;

    // Copy config
    if (config) {
        memcpy(&ckpt->config, config, sizeof(nimcp_seq_checkpoint_config_t));
    } else {
        nimcp_seq_checkpoint_config_init(&ckpt->config);
    }

    // Create underlying checkpoint context
    ckpt->ckpt_ctx = nimcp_checkpoint_ctx_create(gpu_ctx, ckpt->config.strategy,
                                                  num_layers, ckpt->config.memory_budget);
    if (!ckpt->ckpt_ctx) {
        LOG_ERROR("Failed to create underlying checkpoint context");
        nimcp_free(ckpt);
        return NULL;
    }

    // Allocate layer contexts array
    ckpt->layer_contexts = (void**)nimcp_calloc(num_layers, sizeof(void*));
    if (!ckpt->layer_contexts) {
        LOG_ERROR("Failed to allocate layer contexts");
        nimcp_checkpoint_ctx_destroy(ckpt->ckpt_ctx);
        nimcp_free(ckpt);
        return NULL;
    }

    // Allocate layer output sizes array
    ckpt->layer_output_sizes = (size_t*)nimcp_calloc(num_layers, sizeof(size_t));
    if (!ckpt->layer_output_sizes) {
        LOG_ERROR("Failed to allocate layer sizes");
        nimcp_free(ckpt->layer_contexts);
        nimcp_checkpoint_ctx_destroy(ckpt->ckpt_ctx);
        nimcp_free(ckpt);
        return NULL;
    }

    // Allocate activation storage (num_layers + 1 for input)
    int num_activations = num_layers + 1;
    ckpt->activations = (nimcp_gpu_tensor_t**)nimcp_calloc(num_activations,
                                                      sizeof(nimcp_gpu_tensor_t*));
    ckpt->activation_stored = (bool*)nimcp_calloc(num_activations, sizeof(bool));
    if (!ckpt->activations || !ckpt->activation_stored) {
        LOG_ERROR("Failed to allocate activation storage");
        nimcp_free(ckpt->layer_output_sizes);
        nimcp_free(ckpt->layer_contexts);
        nimcp_free(ckpt->activations);
        nimcp_free(ckpt->activation_stored);
        nimcp_checkpoint_ctx_destroy(ckpt->ckpt_ctx);
        nimcp_free(ckpt);
        return NULL;
    }

    // Configure profiling and verbose
    nimcp_checkpoint_set_profiling(ckpt->ckpt_ctx, ckpt->config.enable_profiling);
    nimcp_checkpoint_set_verbose(ckpt->ckpt_ctx, ckpt->config.verbose);

    // Initialize statistics
    ckpt->total_activation_memory = 0;
    ckpt->saved_memory = 0;
    ckpt->recompute_count = 0;
    ckpt->recompute_time_ms = 0.0;

    LOG_INFO("Created sequential checkpoint: layers=%d, strategy=%d, budget=%zu",
             num_layers, ckpt->config.strategy, ckpt->config.memory_budget);

    return ckpt;
}

void nimcp_sequential_checkpoint_destroy(nimcp_sequential_checkpoint_t* ckpt) {
    if (!ckpt) return;

    // Free stored activations
    if (ckpt->activations) {
        int num_activations = ckpt->num_layers + 1;
        for (int i = 0; i < num_activations; i++) {
            if (ckpt->activations[i]) {
                nimcp_gpu_tensor_destroy(ckpt->activations[i]);
            }
        }
        nimcp_free(ckpt->activations);
    }
    nimcp_free(ckpt->activation_stored);

    // Free layer arrays
    nimcp_free(ckpt->layer_contexts);
    nimcp_free(ckpt->layer_output_sizes);

    // Free current input/output
    if (ckpt->current_input) {
        nimcp_gpu_tensor_destroy(ckpt->current_input);
    }
    if (ckpt->current_output) {
        nimcp_gpu_tensor_destroy(ckpt->current_output);
    }

    // Destroy underlying context
    nimcp_checkpoint_ctx_destroy(ckpt->ckpt_ctx);

    LOG_DEBUG("Destroyed sequential checkpoint context");
    nimcp_free(ckpt);
}

bool nimcp_sequential_checkpoint_reset(nimcp_sequential_checkpoint_t* ckpt) {
    if (!ckpt) return false;

    // Reset underlying context
    nimcp_checkpoint_ctx_reset(ckpt->ckpt_ctx);

    // Free stored activations
    int num_activations = ckpt->num_layers + 1;
    for (int i = 0; i < num_activations; i++) {
        if (ckpt->activations[i]) {
            nimcp_gpu_tensor_destroy(ckpt->activations[i]);
            ckpt->activations[i] = NULL;
        }
        ckpt->activation_stored[i] = false;
    }

    // Free current tensors
    if (ckpt->current_input) {
        nimcp_gpu_tensor_destroy(ckpt->current_input);
        ckpt->current_input = NULL;
    }
    if (ckpt->current_output) {
        nimcp_gpu_tensor_destroy(ckpt->current_output);
        ckpt->current_output = NULL;
    }

    // Reset statistics
    ckpt->total_activation_memory = 0;

    return true;
}

//=============================================================================
// Configuration API Implementation
//=============================================================================

bool nimcp_sequential_checkpoint_set_layer_ctx(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    void* layer_ctx)
{
    if (!ckpt || layer_idx < 0 || layer_idx >= ckpt->num_layers) return false;

    ckpt->layer_contexts[layer_idx] = layer_ctx;
    return true;
}

bool nimcp_sequential_checkpoint_set_layer_size(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    size_t output_size)
{
    if (!ckpt || layer_idx < 0 || layer_idx >= ckpt->num_layers) return false;

    ckpt->layer_output_sizes[layer_idx] = output_size;

    // Update underlying checkpoint context
    nimcp_checkpoint_set_layer_size(ckpt->ckpt_ctx, layer_idx, output_size);

    return true;
}

bool nimcp_sequential_checkpoint_set_shape_fn(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_seq_layer_shape_fn_t shape_fn,
    void* ctx)
{
    if (!ckpt) return false;

    ckpt->shape_fn = shape_fn;
    ckpt->shape_ctx = ctx;
    return true;
}

bool nimcp_sequential_checkpoint_configure(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_checkpoint_strategy_t strategy,
    int checkpoint_n)
{
    if (!ckpt) return false;

    ckpt->config.strategy = strategy;
    ckpt->config.checkpoint_every_n = checkpoint_n;

    return nimcp_checkpoint_configure(ckpt->ckpt_ctx, strategy, checkpoint_n);
}

bool nimcp_sequential_checkpoint_auto_configure(nimcp_sequential_checkpoint_t* ckpt) {
    if (!ckpt) return false;

    // Check if we have layer sizes
    bool have_sizes = false;
    for (int i = 0; i < ckpt->num_layers; i++) {
        if (ckpt->layer_output_sizes[i] > 0) {
            have_sizes = true;
            break;
        }
    }

    if (!have_sizes) {
        LOG_WARN("No layer sizes set, using sqrt strategy as default");
        return nimcp_checkpoint_configure(ckpt->ckpt_ctx, CKPT_STRATEGY_SQRT,
                                          nimcp_checkpoint_sqrt_interval(ckpt->num_layers));
    }

    // Get available memory
    size_t free_mem = 0, total_mem = 0;
    cudaMemGetInfo(&free_mem, &total_mem);

    // Use 80% of available memory as budget if not specified
    size_t budget = ckpt->config.memory_budget > 0 ?
                    ckpt->config.memory_budget : (size_t)(free_mem * 0.8);

    return nimcp_checkpoint_auto_configure(ckpt->ckpt_ctx, budget,
                                           ckpt->layer_output_sizes, ckpt->num_layers);
}

//=============================================================================
// Forward Pass API Implementation
//=============================================================================

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_forward(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* input,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    if (!ckpt || !input || !layer_forward) {
        LOG_ERROR("Invalid parameters to sequential forward");
        return NULL;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Begin forward pass
    nimcp_checkpoint_begin_forward(ckpt->ckpt_ctx);

    // Store input as activation[0] (always a checkpoint)
    if (ckpt->activations[0]) {
        nimcp_gpu_tensor_destroy(ckpt->activations[0]);
    }
    ckpt->activations[0] = clone_tensor(ckpt->gpu_ctx, input);
    ckpt->activation_stored[0] = true;
    ckpt->total_activation_memory += tensor_size_bytes(input);

    nimcp_gpu_tensor_t* current = input;
    nimcp_gpu_tensor_t* output = NULL;

    // Track timing
    cudaEvent_t start_event, stop_event;
    float elapsed_ms = 0;

    if (ckpt->config.enable_profiling) {
        cudaEventCreate(&start_event);
        cudaEventCreate(&stop_event);
        cudaEventRecord(start_event);
    }

    // Process each layer
    for (int i = 0; i < ckpt->num_layers; i++) {
        ckpt->current_layer = i;

        // Allocate output tensor
        // Use shape function if available, otherwise use input shape
        size_t output_dims[8];
        uint32_t output_ndim = current->ndim;
        if (ckpt->shape_fn) {
            ckpt->shape_fn(i, ckpt->shape_ctx, current->dims, current->ndim,
                          output_dims, &output_ndim);
        } else {
            // Default: same shape as input
            memcpy(output_dims, current->dims, current->ndim * sizeof(size_t));
        }

        output = nimcp_gpu_tensor_create(ckpt->gpu_ctx, output_dims,
                                          output_ndim, current->precision);
        if (!output) {
            LOG_ERROR("Failed to allocate output tensor for layer %d", i);
            nimcp_checkpoint_end_forward(ckpt->ckpt_ctx);
            return NULL;
        }

        // Run layer forward
        void* layer_ctx = ckpt->layer_contexts[i] ? ckpt->layer_contexts[i] : forward_ctx;
        layer_forward(i, layer_ctx, current, output);

        // Update layer size if not set
        if (ckpt->layer_output_sizes[i] == 0) {
            ckpt->layer_output_sizes[i] = tensor_size_bytes(output);
            nimcp_checkpoint_set_layer_size(ckpt->ckpt_ctx, i, ckpt->layer_output_sizes[i]);
        }

        // Check if this output should be checkpointed (activation i+1)
        bool should_save = nimcp_checkpoint_should_save(ckpt->ckpt_ctx, i) ||
                          (i == ckpt->num_layers - 1);  // Always save final output

        if (should_save) {
            // Store activation
            if (ckpt->activations[i + 1]) {
                nimcp_gpu_tensor_destroy(ckpt->activations[i + 1]);
            }
            ckpt->activations[i + 1] = clone_tensor(ckpt->gpu_ctx, output);
            ckpt->activation_stored[i + 1] = true;
            ckpt->total_activation_memory += tensor_size_bytes(output);

            if (ckpt->config.verbose) {
                LOG_DEBUG("Saved activation %d (%zu bytes)", i + 1,
                          tensor_size_bytes(output));
            }
        } else {
            // Track memory saved
            ckpt->saved_memory += tensor_size_bytes(output);
        }

        // Mark layer in underlying context
        nimcp_checkpoint_mark_layer(ckpt->ckpt_ctx, i, output);

        // Free previous intermediate (if not input and not checkpointed)
        if (current != input && i > 0) {
            int prev_idx = i;  // activation index = layer index + 1, prev = layer index
            if (!ckpt->activation_stored[prev_idx]) {
                // Already handled by not storing
            }
        }

        current = output;
    }

    if (ckpt->config.enable_profiling) {
        cudaEventRecord(stop_event);
        cudaEventSynchronize(stop_event);
        cudaEventElapsedTime(&elapsed_ms, start_event, stop_event);
        cudaEventDestroy(start_event);
        cudaEventDestroy(stop_event);

        if (ckpt->config.verbose) {
            LOG_DEBUG("Sequential forward completed in %.2f ms", elapsed_ms);
        }
    }

    // End forward pass
    nimcp_checkpoint_end_forward(ckpt->ckpt_ctx);

    // Store final output reference
    ckpt->current_output = output;

    return output;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_forward_layer(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* input,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    if (!ckpt || !input || !layer_forward) return NULL;
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) return NULL;

    // Allocate output
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ckpt->gpu_ctx,
                                                          input->dims,
                                                          input->ndim,
                                                          input->precision);
    if (!output) return NULL;

    // Run layer
    void* layer_ctx = ckpt->layer_contexts[layer_idx] ?
                      ckpt->layer_contexts[layer_idx] : forward_ctx;
    layer_forward(layer_idx, layer_ctx, input, output);

    // Handle checkpointing
    if (nimcp_checkpoint_should_save(ckpt->ckpt_ctx, layer_idx)) {
        if (ckpt->activations[layer_idx + 1]) {
            nimcp_gpu_tensor_destroy(ckpt->activations[layer_idx + 1]);
        }
        ckpt->activations[layer_idx + 1] = clone_tensor(ckpt->gpu_ctx, output);
        ckpt->activation_stored[layer_idx + 1] = true;
    }

    return output;
}

//=============================================================================
// Backward Pass API Implementation
//=============================================================================

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* backward_ctx)
{
    // Use default forward function - requires activations to be stored
    // For full recomputation support, use backward_with_recompute
    return nimcp_sequential_checkpoint_backward_with_recompute(
        ckpt, grad_output, NULL, layer_backward, NULL, backward_ctx);
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward_with_recompute(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_forward_fn_t layer_forward,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* forward_ctx,
    void* backward_ctx)
{
    if (!ckpt || !grad_output || !layer_backward) {
        LOG_ERROR("Invalid parameters to sequential backward");
        return NULL;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Begin backward pass
    nimcp_checkpoint_begin_backward(ckpt->ckpt_ctx);

    nimcp_gpu_tensor_t* current_grad = grad_output;
    nimcp_gpu_tensor_t* grad_input = NULL;

    // Track timing
    cudaEvent_t start_event, stop_event;
    float elapsed_ms = 0;

    if (ckpt->config.enable_profiling) {
        cudaEventCreate(&start_event);
        cudaEventCreate(&stop_event);
        cudaEventRecord(start_event);
    }

    // Process layers in reverse order
    for (int i = ckpt->num_layers - 1; i >= 0; i--) {
        ckpt->current_layer = i;

        // Get activation for this layer's input (activation[i])
        nimcp_gpu_tensor_t* layer_input_activation = ckpt->activations[i];

        // Check if we need to recompute
        if (!layer_input_activation && layer_forward) {
            // Need to recompute from nearest checkpoint
            // Find nearest checkpoint before this layer
            int ckpt_layer = i;
            while (ckpt_layer > 0 && !ckpt->activation_stored[ckpt_layer]) {
                ckpt_layer--;
            }

            if (ckpt->activations[ckpt_layer]) {
                // Create recompute context
                seq_recompute_ctx_t rctx;
                rctx.ckpt = ckpt;
                rctx.layer_forward = layer_forward;
                rctx.forward_ctx = forward_ctx;
                rctx.start_layer = ckpt_layer;
                rctx.end_layer = i;

                // Allocate output for recomputation
                nimcp_gpu_tensor_t* recompute_input = ckpt->activations[ckpt_layer];
                nimcp_gpu_tensor_t* recompute_output = nimcp_gpu_tensor_create(
                    ckpt->gpu_ctx, recompute_input->dims,
                    recompute_input->ndim, recompute_input->precision);

                if (!recompute_output) {
                    LOG_ERROR("Failed to allocate recompute output");
                    continue;
                }

                // Run recomputation
                cudaEvent_t recompute_start, recompute_stop;
                float recompute_ms = 0;

                if (ckpt->config.enable_profiling) {
                    cudaEventCreate(&recompute_start);
                    cudaEventCreate(&recompute_stop);
                    cudaEventRecord(recompute_start);
                }

                seq_segment_forward(&rctx, recompute_input, recompute_output);

                if (ckpt->config.enable_profiling) {
                    cudaEventRecord(recompute_stop);
                    cudaEventSynchronize(recompute_stop);
                    cudaEventElapsedTime(&recompute_ms, recompute_start, recompute_stop);
                    ckpt->recompute_time_ms += recompute_ms;
                    cudaEventDestroy(recompute_start);
                    cudaEventDestroy(recompute_stop);
                }

                ckpt->recompute_count++;

                // Now activation[i] should be available
                layer_input_activation = ckpt->activations[i];

                // Clean up recompute output if it's not the activation we need
                if (recompute_output != layer_input_activation) {
                    nimcp_gpu_tensor_destroy(recompute_output);
                }
            } else {
                LOG_ERROR("No checkpoint found for recomputation at layer %d", i);
            }
        }

        if (!layer_input_activation) {
            LOG_ERROR("Missing activation for layer %d backward", i);
            continue;
        }

        // Allocate gradient input tensor
        grad_input = nimcp_gpu_tensor_create(ckpt->gpu_ctx,
                                              layer_input_activation->dims,
                                              layer_input_activation->ndim,
                                              layer_input_activation->precision);
        if (!grad_input) {
            LOG_ERROR("Failed to allocate grad_input for layer %d", i);
            break;
        }

        // Run layer backward
        void* layer_ctx = ckpt->layer_contexts[i] ?
                          ckpt->layer_contexts[i] : backward_ctx;
        layer_backward(i, layer_ctx, current_grad, grad_input);

        // Free previous gradient (if not the original grad_output)
        if (current_grad != grad_output && i < ckpt->num_layers - 1) {
            nimcp_gpu_tensor_destroy(current_grad);
        }

        // Free activation if not a checkpoint (save memory)
        if (!nimcp_checkpoint_is_checkpoint_layer(ckpt->ckpt_ctx, i)) {
            nimcp_sequential_checkpoint_free_activation(ckpt, i);
        }

        current_grad = grad_input;
    }

    if (ckpt->config.enable_profiling) {
        cudaEventRecord(stop_event);
        cudaEventSynchronize(stop_event);
        cudaEventElapsedTime(&elapsed_ms, start_event, stop_event);
        cudaEventDestroy(start_event);
        cudaEventDestroy(stop_event);

        if (ckpt->config.verbose) {
            LOG_DEBUG("Sequential backward completed in %.2f ms (%.2f ms recompute)",
                      elapsed_ms, ckpt->recompute_time_ms);
        }
    }

    // End backward pass
    nimcp_checkpoint_end_backward(ckpt->ckpt_ctx);

    return grad_input;  // Gradient w.r.t. input
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward_layer(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* backward_ctx)
{
    if (!ckpt || !grad_output || !layer_backward) return NULL;
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) return NULL;

    // Get layer input activation
    nimcp_gpu_tensor_t* activation = ckpt->activations[layer_idx];
    if (!activation) {
        LOG_ERROR("No activation for layer %d", layer_idx);
        return NULL;
    }

    // Allocate grad_input
    nimcp_gpu_tensor_t* grad_input = nimcp_gpu_tensor_create(ckpt->gpu_ctx,
                                                              activation->dims,
                                                              activation->ndim,
                                                              activation->precision);
    if (!grad_input) return NULL;

    // Run backward
    void* layer_ctx = ckpt->layer_contexts[layer_idx] ?
                      ckpt->layer_contexts[layer_idx] : backward_ctx;
    layer_backward(layer_idx, layer_ctx, grad_output, grad_input);

    return grad_input;
}

//=============================================================================
// Activation Management API Implementation
//=============================================================================

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_get_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    if (!ckpt) return NULL;
    if (layer_idx < 0 || layer_idx > ckpt->num_layers) return NULL;

    // Return if already stored
    if (ckpt->activation_stored[layer_idx] && ckpt->activations[layer_idx]) {
        return ckpt->activations[layer_idx];
    }

    // Need to recompute
    if (!layer_forward) {
        LOG_ERROR("No forward function provided for recomputation");
        return NULL;
    }

    // Find nearest checkpoint before this layer
    int ckpt_idx = layer_idx - 1;
    while (ckpt_idx >= 0 && !ckpt->activation_stored[ckpt_idx]) {
        ckpt_idx--;
    }

    if (ckpt_idx < 0 || !ckpt->activations[ckpt_idx]) {
        LOG_ERROR("No checkpoint found for recomputation");
        return NULL;
    }

    // Recompute from checkpoint to target layer
    nimcp_gpu_tensor_t* current = ckpt->activations[ckpt_idx];

    for (int i = ckpt_idx; i < layer_idx; i++) {
        nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ckpt->gpu_ctx,
                                                              current->dims,
                                                              current->ndim,
                                                              current->precision);
        if (!output) return NULL;

        void* layer_ctx = ckpt->layer_contexts[i] ?
                          ckpt->layer_contexts[i] : forward_ctx;
        layer_forward(i, layer_ctx, current, output);

        // Free intermediate if not checkpoint
        if (current != ckpt->activations[ckpt_idx]) {
            nimcp_gpu_tensor_destroy(current);
        }

        current = output;
    }

    // Store the computed activation
    ckpt->activations[layer_idx] = current;
    ckpt->activation_stored[layer_idx] = true;
    ckpt->recompute_count++;

    return current;
}

bool nimcp_sequential_checkpoint_has_activation(
    const nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx)
{
    if (!ckpt) return false;
    if (layer_idx < 0 || layer_idx > ckpt->num_layers) return false;

    return ckpt->activation_stored[layer_idx] && ckpt->activations[layer_idx] != NULL;
}

bool nimcp_sequential_checkpoint_store_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* activation)
{
    if (!ckpt || !activation) return false;
    if (layer_idx < 0 || layer_idx > ckpt->num_layers) return false;

    // Free existing
    if (ckpt->activations[layer_idx]) {
        nimcp_gpu_tensor_destroy(ckpt->activations[layer_idx]);
    }

    // Clone and store
    ckpt->activations[layer_idx] = clone_tensor(ckpt->gpu_ctx, activation);
    if (!ckpt->activations[layer_idx]) return false;

    ckpt->activation_stored[layer_idx] = true;
    ckpt->total_activation_memory += tensor_size_bytes(activation);

    return true;
}

bool nimcp_sequential_checkpoint_free_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx)
{
    if (!ckpt) return false;
    if (layer_idx < 0 || layer_idx > ckpt->num_layers) return false;

    if (ckpt->activations[layer_idx]) {
        size_t freed_size = tensor_size_bytes(ckpt->activations[layer_idx]);
        nimcp_gpu_tensor_destroy(ckpt->activations[layer_idx]);
        ckpt->activations[layer_idx] = NULL;
        ckpt->activation_stored[layer_idx] = false;

        if (ckpt->total_activation_memory >= freed_size) {
            ckpt->total_activation_memory -= freed_size;
        }
    }

    return true;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

void nimcp_sequential_checkpoint_get_stats(
    const nimcp_sequential_checkpoint_t* ckpt,
    size_t* total_memory,
    size_t* saved_memory,
    int* recompute_count,
    double* recompute_time_ms)
{
    if (!ckpt) return;

    if (total_memory) *total_memory = ckpt->total_activation_memory;
    if (saved_memory) *saved_memory = ckpt->saved_memory;
    if (recompute_count) *recompute_count = ckpt->recompute_count;
    if (recompute_time_ms) *recompute_time_ms = ckpt->recompute_time_ms;
}

void nimcp_sequential_checkpoint_print_stats(const nimcp_sequential_checkpoint_t* ckpt) {
    if (!ckpt) return;

    printf("=== Sequential Checkpoint Statistics ===\n");
    printf("Number of layers: %d\n", ckpt->num_layers);
    printf("Strategy: %d\n", ckpt->config.strategy);
    printf("Memory budget: %zu bytes (%.2f MB)\n",
           ckpt->config.memory_budget,
           ckpt->config.memory_budget / (1024.0 * 1024.0));
    printf("Total activation memory: %zu bytes (%.2f MB)\n",
           ckpt->total_activation_memory,
           ckpt->total_activation_memory / (1024.0 * 1024.0));
    printf("Memory saved: %zu bytes (%.2f MB)\n",
           ckpt->saved_memory,
           ckpt->saved_memory / (1024.0 * 1024.0));
    printf("Recomputation count: %d\n", ckpt->recompute_count);
    printf("Recomputation time: %.2f ms\n", ckpt->recompute_time_ms);

    // Print stored activations
    int stored_count = 0;
    for (int i = 0; i <= ckpt->num_layers; i++) {
        if (ckpt->activation_stored[i]) stored_count++;
    }
    printf("Stored activations: %d / %d\n", stored_count, ckpt->num_layers + 1);
    printf("========================================\n");
}

int nimcp_sequential_checkpoint_get_info_string(
    const nimcp_sequential_checkpoint_t* ckpt,
    char* buffer,
    size_t size)
{
    if (!ckpt || !buffer || size == 0) return 0;

    return snprintf(buffer, size,
        "SeqCkpt: layers=%d, strategy=%d, memory=%zu/%zu bytes, "
        "saved=%zu bytes, recomputes=%d",
        ckpt->num_layers, ckpt->config.strategy,
        ckpt->total_activation_memory, ckpt->config.memory_budget,
        ckpt->saved_memory, ckpt->recompute_count);
}

//=============================================================================
// Transformer Checkpoint Implementation
//=============================================================================

nimcp_transformer_checkpoint_t* nimcp_transformer_checkpoint_create(
    nimcp_gpu_context_t* gpu_ctx,
    int num_blocks,
    size_t memory_budget)
{
    if (!gpu_ctx || num_blocks <= 0) return NULL;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    nimcp_transformer_checkpoint_t* ckpt = (nimcp_transformer_checkpoint_t*)nimcp_calloc(1,
        sizeof(nimcp_transformer_checkpoint_t));
    if (!ckpt) return NULL;

    ckpt->num_blocks = num_blocks;

    // Each transformer block has ~4 sub-layers: attention, attention_norm, ffn, ffn_norm
    int num_layers = num_blocks * 4;

    ckpt->seq_ckpt = nimcp_sequential_checkpoint_create(gpu_ctx, num_layers, memory_budget);
    if (!ckpt->seq_ckpt) {
        nimcp_free(ckpt);
        return NULL;
    }

    // Allocate block configurations
    ckpt->blocks = (nimcp_transformer_block_t*)nimcp_calloc(num_blocks,
                                                       sizeof(nimcp_transformer_block_t));
    if (!ckpt->blocks) {
        nimcp_sequential_checkpoint_destroy(ckpt->seq_ckpt);
        nimcp_free(ckpt);
        return NULL;
    }

    // Allocate specialized storage
    ckpt->attention_scores = (nimcp_gpu_tensor_t**)nimcp_calloc(num_blocks,
                                                           sizeof(nimcp_gpu_tensor_t*));
    ckpt->ffn_intermediate = (nimcp_gpu_tensor_t**)nimcp_calloc(num_blocks,
                                                           sizeof(nimcp_gpu_tensor_t*));
    if (!ckpt->attention_scores || !ckpt->ffn_intermediate) {
        nimcp_free(ckpt->blocks);
        nimcp_free(ckpt->attention_scores);
        nimcp_free(ckpt->ffn_intermediate);
        nimcp_sequential_checkpoint_destroy(ckpt->seq_ckpt);
        nimcp_free(ckpt);
        return NULL;
    }

    LOG_INFO("Created transformer checkpoint: blocks=%d, memory_budget=%zu",
             num_blocks, memory_budget);

    return ckpt;
}

bool nimcp_transformer_checkpoint_configure_block(
    nimcp_transformer_checkpoint_t* ckpt,
    int block_idx,
    const nimcp_transformer_block_t* block)
{
    if (!ckpt || !block) return false;
    if (block_idx < 0 || block_idx >= ckpt->num_blocks) return false;

    memcpy(&ckpt->blocks[block_idx], block, sizeof(nimcp_transformer_block_t));

    // Calculate expected activation sizes for this block
    // Attention: (batch, seq_len, num_heads, seq_len) for scores
    // FFN: (batch, seq_len, ffn_dim) for intermediate

    return true;
}

void nimcp_transformer_checkpoint_destroy(nimcp_transformer_checkpoint_t* ckpt) {
    if (!ckpt) return;

    // Free attention scores
    if (ckpt->attention_scores) {
        for (int i = 0; i < ckpt->num_blocks; i++) {
            if (ckpt->attention_scores[i]) {
                nimcp_gpu_tensor_destroy(ckpt->attention_scores[i]);
            }
        }
        nimcp_free(ckpt->attention_scores);
    }

    // Free FFN intermediates
    if (ckpt->ffn_intermediate) {
        for (int i = 0; i < ckpt->num_blocks; i++) {
            if (ckpt->ffn_intermediate[i]) {
                nimcp_gpu_tensor_destroy(ckpt->ffn_intermediate[i]);
            }
        }
        nimcp_free(ckpt->ffn_intermediate);
    }

    nimcp_free(ckpt->blocks);
    nimcp_sequential_checkpoint_destroy(ckpt->seq_ckpt);
    nimcp_free(ckpt);
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

bool nimcp_sequential_checkpoint_estimate_memory(
    int num_layers,
    const size_t* layer_sizes,
    size_t* no_ckpt_memory,
    size_t* sqrt_ckpt_memory,
    int* optimal_n)
{
    if (!layer_sizes || num_layers <= 0) return false;

    nimcp_checkpoint_estimate_t estimate;
    if (!nimcp_checkpoint_estimate(layer_sizes, num_layers, &estimate)) {
        return false;
    }

    if (no_ckpt_memory) *no_ckpt_memory = estimate.no_checkpoint_memory;
    if (sqrt_ckpt_memory) *sqrt_ckpt_memory = estimate.sqrt_checkpoint_memory;
    if (optimal_n) *optimal_n = estimate.optimal_n;

    return true;
}

bool nimcp_sequential_checkpoint_recommend(
    int num_layers,
    const size_t* layer_sizes,
    size_t memory_budget,
    nimcp_checkpoint_strategy_t* strategy,
    int* checkpoint_n)
{
    return nimcp_checkpoint_recommend_strategy(layer_sizes, num_layers, memory_budget,
                                               strategy, checkpoint_n);
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

#include "gpu/training/nimcp_activation_checkpoint.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "ACTIVATION_CKPT"

void nimcp_seq_checkpoint_config_init(nimcp_seq_checkpoint_config_t* config) {
    if (config) {
        memset(config, 0, sizeof(nimcp_seq_checkpoint_config_t));
        config->strategy = CKPT_STRATEGY_SQRT;
    }
}

nimcp_sequential_checkpoint_t* nimcp_sequential_checkpoint_create(
    nimcp_gpu_context_t* gpu_ctx,
    int num_layers,
    size_t memory_budget)
{
    LOG_WARN("CUDA not available - sequential checkpointing requires GPU");
    return NULL;
}

nimcp_sequential_checkpoint_t* nimcp_sequential_checkpoint_create_with_config(
    nimcp_gpu_context_t* gpu_ctx,
    int num_layers,
    const nimcp_seq_checkpoint_config_t* config)
{
    return NULL;
}

void nimcp_sequential_checkpoint_destroy(nimcp_sequential_checkpoint_t* ckpt) {
    (void)ckpt;
}

bool nimcp_sequential_checkpoint_reset(nimcp_sequential_checkpoint_t* ckpt) {
    return false;
}

bool nimcp_sequential_checkpoint_set_layer_ctx(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    void* layer_ctx)
{
    return false;
}

bool nimcp_sequential_checkpoint_set_layer_size(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    size_t output_size)
{
    return false;
}

bool nimcp_sequential_checkpoint_set_shape_fn(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_seq_layer_shape_fn_t shape_fn,
    void* ctx)
{
    return false;
}

bool nimcp_sequential_checkpoint_configure(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_checkpoint_strategy_t strategy,
    int checkpoint_n)
{
    return false;
}

bool nimcp_sequential_checkpoint_auto_configure(nimcp_sequential_checkpoint_t* ckpt) {
    return false;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_forward(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* input,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_forward_layer(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* input,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* backward_ctx)
{
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward_with_recompute(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_forward_fn_t layer_forward,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* forward_ctx,
    void* backward_ctx)
{
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward_layer(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* backward_ctx)
{
    return NULL;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_get_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    return NULL;
}

bool nimcp_sequential_checkpoint_has_activation(
    const nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx)
{
    return false;
}

bool nimcp_sequential_checkpoint_store_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* activation)
{
    return false;
}

bool nimcp_sequential_checkpoint_free_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx)
{
    return false;
}

void nimcp_sequential_checkpoint_get_stats(
    const nimcp_sequential_checkpoint_t* ckpt,
    size_t* total_memory,
    size_t* saved_memory,
    int* recompute_count,
    double* recompute_time_ms)
{
    (void)ckpt;
    (void)total_memory;
    (void)saved_memory;
    (void)recompute_count;
    (void)recompute_time_ms;
}

void nimcp_sequential_checkpoint_print_stats(const nimcp_sequential_checkpoint_t* ckpt) {
    (void)ckpt;
}

int nimcp_sequential_checkpoint_get_info_string(
    const nimcp_sequential_checkpoint_t* ckpt,
    char* buffer,
    size_t size)
{
    return 0;
}

nimcp_transformer_checkpoint_t* nimcp_transformer_checkpoint_create(
    nimcp_gpu_context_t* gpu_ctx,
    int num_blocks,
    size_t memory_budget)
{
    return NULL;
}

bool nimcp_transformer_checkpoint_configure_block(
    nimcp_transformer_checkpoint_t* ckpt,
    int block_idx,
    const nimcp_transformer_block_t* block)
{
    return false;
}

void nimcp_transformer_checkpoint_destroy(nimcp_transformer_checkpoint_t* ckpt) {
    (void)ckpt;
}

bool nimcp_sequential_checkpoint_estimate_memory(
    int num_layers,
    const size_t* layer_sizes,
    size_t* no_ckpt_memory,
    size_t* sqrt_ckpt_memory,
    int* optimal_n)
{
    return false;
}

bool nimcp_sequential_checkpoint_recommend(
    int num_layers,
    const size_t* layer_sizes,
    size_t memory_budget,
    nimcp_checkpoint_strategy_t* strategy,
    int* checkpoint_n)
{
    return false;
}

#endif // NIMCP_ENABLE_CUDA
