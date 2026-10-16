/**
 * @file nimcp_gradient_checkpoint.cu
 * @brief GPU Gradient Checkpointing Implementation
 *
 * WHAT: Implementation of gradient checkpointing for memory-efficient training
 * WHY:  Enables training larger models by reducing activation memory usage
 * HOW:  Saves activations at checkpoint boundaries, recomputes during backward
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
#include "gpu/training/nimcp_gradient_checkpoint.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

#define LOG_MODULE "CHECKPOINT_GPU"

// Initial pool capacity
#define INITIAL_POOL_CAPACITY 16
#define INITIAL_SEGMENT_CAPACITY 32

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Calculate integer square root (ceiling)
 */
static int int_sqrt_ceil(int n) {
    if (n <= 0) return 0;
    int s = (int)ceil(sqrt((double)n));
    return s;
}

/**
 * @brief Get current time in milliseconds (for profiling)
 */
static double get_time_ms(void) {
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    float ms = 0;
    cudaEventElapsedTime(&ms, start, stop);
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    return (double)ms;
}

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
 * @brief Calculate total size from dimensions
 */
static size_t calc_tensor_bytes(const size_t* dims, uint32_t ndim,
                                 nimcp_gpu_precision_t precision) {
    size_t numel = 1;
    for (uint32_t i = 0; i < ndim; i++) {
        numel *= dims[i];
    }

    size_t elem_size = 4;  // Default FP32
    switch (precision) {
        case NIMCP_GPU_PRECISION_FP16:
        case NIMCP_GPU_PRECISION_BF16:
            elem_size = 2;
            break;
        case NIMCP_GPU_PRECISION_INT8:
            elem_size = 1;
            break;
        default:
            elem_size = 4;
            break;
    }

    return numel * elem_size;
}

/**
 * @brief Build segments based on checkpoint configuration
 */
static bool build_segments(nimcp_checkpoint_ctx_t* ctx) {
    if (!ctx || ctx->total_layers <= 0) return false;

    // Clear existing segments
    if (ctx->segments) {
        for (int i = 0; i < ctx->num_segments; i++) {
            if (ctx->segments[i].saved_input) {
                nimcp_gpu_tensor_destroy(ctx->segments[i].saved_input);
            }
            if (ctx->segments[i].saved_output) {
                nimcp_gpu_tensor_destroy(ctx->segments[i].saved_output);
            }
        }
        nimcp_free(ctx->segments);
        ctx->segments = NULL;
        ctx->num_segments = 0;
    }

    // Determine checkpoint layers based on strategy
    bool* is_checkpoint = (bool*)nimcp_calloc(ctx->total_layers + 1, sizeof(bool));
    if (!is_checkpoint) return false;

    // Layer 0 is always a checkpoint (input)
    is_checkpoint[0] = true;
    // Last layer is always a checkpoint (output)
    is_checkpoint[ctx->total_layers] = true;

    switch (ctx->strategy) {
        case CKPT_STRATEGY_NONE:
            // All layers are checkpoints (no recomputation needed)
            for (int i = 0; i <= ctx->total_layers; i++) {
                is_checkpoint[i] = true;
            }
            break;

        case CKPT_STRATEGY_SQRT: {
            int interval = int_sqrt_ceil(ctx->total_layers);
            for (int i = 0; i <= ctx->total_layers; i += interval) {
                is_checkpoint[i] = true;
            }
            break;
        }

        case CKPT_STRATEGY_EVERY_N: {
            int n = ctx->checkpoint_every_n > 0 ? ctx->checkpoint_every_n : 1;
            for (int i = 0; i <= ctx->total_layers; i += n) {
                is_checkpoint[i] = true;
            }
            break;
        }

        case CKPT_STRATEGY_SELECTIVE:
            for (int i = 0; i < ctx->num_selective_layers; i++) {
                int idx = ctx->selective_layers[i];
                if (idx >= 0 && idx <= ctx->total_layers) {
                    is_checkpoint[idx] = true;
                }
            }
            break;

        case CKPT_STRATEGY_MEMORY_BUDGET:
            // Memory budget configuration handled separately
            // Default to sqrt strategy as fallback
            {
                int interval = int_sqrt_ceil(ctx->total_layers);
                for (int i = 0; i <= ctx->total_layers; i += interval) {
                    is_checkpoint[i] = true;
                }
            }
            break;
    }

    // Update layer info
    for (int i = 0; i <= ctx->total_layers; i++) {
        if (i < ctx->total_layers) {
            ctx->layer_info[i].is_checkpoint_boundary = is_checkpoint[i];
        }
    }

    // Count and create segments
    int segment_count = 0;
    int last_checkpoint = 0;

    for (int i = 1; i <= ctx->total_layers; i++) {
        if (is_checkpoint[i]) {
            segment_count++;
            last_checkpoint = i;
        }
    }

    // Allocate segments
    ctx->segments = (nimcp_checkpoint_segment_t*)nimcp_calloc(segment_count,
                                                         sizeof(nimcp_checkpoint_segment_t));
    if (!ctx->segments) {
        nimcp_free(is_checkpoint);
        return false;
    }
    ctx->max_segments = segment_count;

    // Build segments
    int seg_idx = 0;
    int start = 0;

    for (int i = 1; i <= ctx->total_layers; i++) {
        if (is_checkpoint[i]) {
            ctx->segments[seg_idx].start_layer = start;
            ctx->segments[seg_idx].end_layer = i - 1;
            ctx->segments[seg_idx].saved_input = NULL;
            ctx->segments[seg_idx].saved_output = NULL;
            ctx->segments[seg_idx].needs_recompute = false;
            ctx->segments[seg_idx].input_is_checkpoint = true;
            ctx->segments[seg_idx].output_is_checkpoint = true;
            ctx->segments[seg_idx].forward_fn = NULL;
            ctx->segments[seg_idx].backward_fn = NULL;
            ctx->segments[seg_idx].segment_ctx = NULL;
            ctx->segments[seg_idx].preserve_rng = false;
            ctx->segments[seg_idx].forward_time_ms = 0.0;
            ctx->segments[seg_idx].recompute_time_ms = 0.0;

            // Calculate memory for this segment
            size_t seg_memory = 0;
            for (int j = start; j < i; j++) {
                seg_memory += ctx->layer_info[j].activation_size;
                ctx->layer_info[j].segment_idx = seg_idx;
            }
            ctx->segments[seg_idx].activation_size = seg_memory;
            ctx->segments[seg_idx].memory_saved = 0;

            start = i;
            seg_idx++;
        }
    }

    ctx->num_segments = seg_idx;
    ctx->configured = true;

    nimcp_free(is_checkpoint);

    LOG_DEBUG("Built %d segments for %d layers (strategy=%d)",
              ctx->num_segments, ctx->total_layers, ctx->strategy);

    return true;
}

//=============================================================================
// Context Lifecycle API
//=============================================================================

nimcp_checkpoint_ctx_t* nimcp_checkpoint_ctx_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_checkpoint_strategy_t strategy,
    int total_layers,
    size_t memory_budget)
{
    if (!gpu_ctx || total_layers <= 0) {
        LOG_ERROR("Invalid parameters: gpu_ctx=%p, total_layers=%d",
                  (void*)gpu_ctx, total_layers);
        return NULL;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    nimcp_checkpoint_ctx_t* ctx = (nimcp_checkpoint_ctx_t*)nimcp_calloc(1, sizeof(nimcp_checkpoint_ctx_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate checkpoint context");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->strategy = strategy;
    ctx->total_layers = total_layers;
    ctx->memory_budget = memory_budget;
    ctx->checkpoint_every_n = int_sqrt_ceil(total_layers);  // Default

    // Allocate layer info
    ctx->layer_info = (nimcp_layer_activation_info_t*)nimcp_calloc(total_layers,
                                                              sizeof(nimcp_layer_activation_info_t));
    if (!ctx->layer_info) {
        LOG_ERROR("Failed to allocate layer info array");
        nimcp_free(ctx);
        return NULL;
    }

    // Initialize layer info
    for (int i = 0; i < total_layers; i++) {
        ctx->layer_info[i].layer_idx = i;
        ctx->layer_info[i].activation_size = 0;
        ctx->layer_info[i].state = CKPT_STATE_NONE;
        ctx->layer_info[i].activation = NULL;
        ctx->layer_info[i].is_checkpoint_boundary = false;
        ctx->layer_info[i].segment_idx = -1;
    }

    // Allocate tensor pool
    ctx->pool_capacity = INITIAL_POOL_CAPACITY;
    ctx->tensor_pool = (nimcp_gpu_tensor_t**)nimcp_calloc(ctx->pool_capacity,
                                                     sizeof(nimcp_gpu_tensor_t*));
    ctx->pool_in_use = (bool*)nimcp_calloc(ctx->pool_capacity, sizeof(bool));
    if (!ctx->tensor_pool || !ctx->pool_in_use) {
        LOG_ERROR("Failed to allocate tensor pool");
        nimcp_free(ctx->layer_info);
        nimcp_free(ctx->tensor_pool);
        nimcp_free(ctx->pool_in_use);
        nimcp_free(ctx);
        return NULL;
    }
    ctx->pool_size = 0;

    // Initialize statistics
    ctx->current_memory = 0;
    ctx->peak_memory = 0;
    ctx->saved_memory = 0;
    ctx->recompute_count = 0;
    ctx->total_recompute_time_ms = 0.0;
    ctx->total_forward_time_ms = 0.0;
    ctx->forward_pass_count = 0;
    ctx->backward_pass_count = 0;

    // State flags
    ctx->in_forward_pass = false;
    ctx->in_backward_pass = false;
    ctx->configured = false;
    ctx->enable_profiling = false;
    ctx->verbose = false;

    // Build initial segments
    if (!build_segments(ctx)) {
        LOG_WARN("Failed to build initial segments");
    }

    LOG_INFO("Created checkpoint context: layers=%d, strategy=%d, budget=%zu bytes",
             total_layers, strategy, memory_budget);

    return ctx;
}

void nimcp_checkpoint_ctx_destroy(nimcp_checkpoint_ctx_t* ctx) {
    if (!ctx) return;

    // Free segments
    if (ctx->segments) {
        for (int i = 0; i < ctx->num_segments; i++) {
            if (ctx->segments[i].saved_input) {
                nimcp_gpu_tensor_destroy(ctx->segments[i].saved_input);
            }
            if (ctx->segments[i].saved_output) {
                nimcp_gpu_tensor_destroy(ctx->segments[i].saved_output);
            }
        }
        nimcp_free(ctx->segments);
    }

    // Free layer info and activations
    if (ctx->layer_info) {
        for (int i = 0; i < ctx->total_layers; i++) {
            if (ctx->layer_info[i].activation) {
                nimcp_gpu_tensor_destroy(ctx->layer_info[i].activation);
            }
        }
        nimcp_free(ctx->layer_info);
    }

    // Free tensor pool
    if (ctx->tensor_pool) {
        for (int i = 0; i < ctx->pool_size; i++) {
            if (ctx->tensor_pool[i]) {
                nimcp_gpu_tensor_destroy(ctx->tensor_pool[i]);
            }
        }
        nimcp_free(ctx->tensor_pool);
    }
    nimcp_free(ctx->pool_in_use);

    // Free selective layers array
    nimcp_free(ctx->selective_layers);

    LOG_DEBUG("Destroyed checkpoint context");
    nimcp_free(ctx);
}

bool nimcp_checkpoint_ctx_reset(nimcp_checkpoint_ctx_t* ctx) {
    if (!ctx) return false;

    // Clear stored activations
    for (int i = 0; i < ctx->total_layers; i++) {
        if (ctx->layer_info[i].activation) {
            nimcp_gpu_tensor_destroy(ctx->layer_info[i].activation);
            ctx->layer_info[i].activation = NULL;
        }
        ctx->layer_info[i].state = CKPT_STATE_NONE;
    }

    // Clear segment saved tensors
    for (int i = 0; i < ctx->num_segments; i++) {
        if (ctx->segments[i].saved_input) {
            nimcp_gpu_tensor_destroy(ctx->segments[i].saved_input);
            ctx->segments[i].saved_input = NULL;
        }
        if (ctx->segments[i].saved_output) {
            nimcp_gpu_tensor_destroy(ctx->segments[i].saved_output);
            ctx->segments[i].saved_output = NULL;
        }
        ctx->segments[i].needs_recompute = false;
    }

    // Reset pool usage
    for (int i = 0; i < ctx->pool_size; i++) {
        ctx->pool_in_use[i] = false;
    }

    // Reset memory tracking
    ctx->current_memory = 0;

    // Reset state flags
    ctx->in_forward_pass = false;
    ctx->in_backward_pass = false;

    return true;
}

//=============================================================================
// Configuration API
//=============================================================================

bool nimcp_checkpoint_configure(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpoint_strategy_t strategy,
    int checkpoint_every_n)
{
    if (!ctx) return false;

    ctx->strategy = strategy;
    ctx->checkpoint_every_n = checkpoint_every_n > 0 ? checkpoint_every_n : 1;

    return build_segments(ctx);
}

bool nimcp_checkpoint_configure_selective(
    nimcp_checkpoint_ctx_t* ctx,
    const int* layer_indices,
    int num_layers)
{
    if (!ctx || !layer_indices || num_layers <= 0) return false;

    // Free existing selective layers
    nimcp_free(ctx->selective_layers);

    // Copy new selective layers
    ctx->selective_layers = (int*)nimcp_malloc(num_layers * sizeof(int));
    if (!ctx->selective_layers) return false;

    memcpy(ctx->selective_layers, layer_indices, num_layers * sizeof(int));
    ctx->num_selective_layers = num_layers;
    ctx->strategy = CKPT_STRATEGY_SELECTIVE;

    return build_segments(ctx);
}

bool nimcp_checkpoint_auto_configure(
    nimcp_checkpoint_ctx_t* ctx,
    size_t available_memory,
    const size_t* layer_activation_sizes,
    int num_layers)
{
    if (!ctx || !layer_activation_sizes || num_layers <= 0) return false;

    // Update layer sizes
    for (int i = 0; i < num_layers && i < ctx->total_layers; i++) {
        ctx->layer_info[i].activation_size = layer_activation_sizes[i];
    }

    // Calculate total memory without checkpointing
    size_t total_memory = 0;
    for (int i = 0; i < num_layers; i++) {
        total_memory += layer_activation_sizes[i];
    }
    ctx->peak_memory = total_memory;

    if (total_memory <= available_memory) {
        // No checkpointing needed
        ctx->strategy = CKPT_STRATEGY_NONE;
        ctx->memory_budget = available_memory;
        return build_segments(ctx);
    }

    // Try different strategies to find best fit
    ctx->memory_budget = available_memory;

    // Calculate memory for sqrt strategy
    int sqrt_interval = int_sqrt_ceil(num_layers);
    size_t sqrt_memory = 0;
    for (int i = 0; i < num_layers; i += sqrt_interval) {
        sqrt_memory += layer_activation_sizes[i];
    }
    sqrt_memory += sqrt_interval * (total_memory / num_layers);  // One segment's worth

    if (sqrt_memory <= available_memory) {
        ctx->strategy = CKPT_STRATEGY_SQRT;
        return build_segments(ctx);
    }

    // Try progressively smaller intervals
    for (int n = sqrt_interval; n >= 1; n--) {
        size_t est_memory = 0;
        int num_checkpoints = (num_layers + n - 1) / n;

        // Memory for checkpointed activations
        for (int i = 0; i < num_layers; i += n) {
            est_memory += layer_activation_sizes[i];
        }
        // Plus one segment worth of intermediate activations
        size_t segment_memory = 0;
        for (int j = 0; j < n && j < num_layers; j++) {
            segment_memory += layer_activation_sizes[j];
        }
        est_memory += segment_memory;

        if (est_memory <= available_memory) {
            ctx->strategy = CKPT_STRATEGY_EVERY_N;
            ctx->checkpoint_every_n = n;
            return build_segments(ctx);
        }
    }

    // Can't fit in memory budget, use most aggressive checkpointing
    LOG_WARN("Cannot fit within memory budget %zu, using EVERY_N=1", available_memory);
    ctx->strategy = CKPT_STRATEGY_EVERY_N;
    ctx->checkpoint_every_n = 1;

    return build_segments(ctx);
}

bool nimcp_checkpoint_set_layer_size(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx,
    size_t size_bytes)
{
    if (!ctx || layer_idx < 0 || layer_idx >= ctx->total_layers) return false;

    ctx->layer_info[layer_idx].activation_size = size_bytes;
    return true;
}

//=============================================================================
// Checkpoint Operations API
//=============================================================================

bool nimcp_checkpoint_mark_layer(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx,
    nimcp_gpu_tensor_t* activation)
{
    if (!ctx || layer_idx < 0 || layer_idx >= ctx->total_layers || !activation) {
        return false;
    }

    nimcp_layer_activation_info_t* info = &ctx->layer_info[layer_idx];

    // Update activation size if not set
    if (info->activation_size == 0) {
        info->activation_size = activation->numel * activation->elem_size;
    }

    // Check if this layer should be checkpointed
    if (nimcp_checkpoint_should_save(ctx, layer_idx)) {
        // Save activation
        if (info->activation) {
            nimcp_gpu_tensor_destroy(info->activation);
        }

        info->activation = clone_tensor(ctx->gpu_ctx, activation);
        if (!info->activation) {
            LOG_ERROR("Failed to save activation for layer %d", layer_idx);
            return false;
        }

        info->state = CKPT_STATE_SAVED;
        ctx->current_memory += info->activation_size;

        if (ctx->verbose) {
            LOG_DEBUG("Saved activation for layer %d (%zu bytes)",
                      layer_idx, info->activation_size);
        }

        // Also save to segment if this is a segment boundary
        int seg_idx = info->segment_idx;
        if (seg_idx >= 0 && seg_idx < ctx->num_segments) {
            nimcp_checkpoint_segment_t* seg = &ctx->segments[seg_idx];
            if (layer_idx == seg->start_layer) {
                if (seg->saved_input) {
                    nimcp_gpu_tensor_destroy(seg->saved_input);
                }
                seg->saved_input = clone_tensor(ctx->gpu_ctx, activation);
            }
        }
    } else {
        // Mark as freed (will need recomputation)
        info->state = CKPT_STATE_FREED;

        // Track memory savings
        int seg_idx = info->segment_idx;
        if (seg_idx >= 0 && seg_idx < ctx->num_segments) {
            ctx->segments[seg_idx].memory_saved += info->activation_size;
            ctx->segments[seg_idx].needs_recompute = true;
        }
        ctx->saved_memory += info->activation_size;
    }

    return true;
}

bool nimcp_checkpoint_should_save(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx || layer_idx < 0 || layer_idx >= ctx->total_layers) return false;

    return ctx->layer_info[layer_idx].is_checkpoint_boundary;
}

nimcp_gpu_tensor_t* nimcp_checkpoint_get_activation(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx || layer_idx < 0 || layer_idx >= ctx->total_layers) {
        return NULL;
    }

    nimcp_layer_activation_info_t* info = &ctx->layer_info[layer_idx];

    // If activation is saved, return it
    if (info->state == CKPT_STATE_SAVED && info->activation) {
        return info->activation;
    }

    // Need to recompute
    int seg_idx = info->segment_idx;
    if (seg_idx < 0 || seg_idx >= ctx->num_segments) {
        LOG_ERROR("Layer %d has invalid segment index %d", layer_idx, seg_idx);
        return NULL;
    }

    // Trigger recomputation for the segment
    if (!nimcp_checkpoint_recompute_segment(ctx, seg_idx)) {
        LOG_ERROR("Failed to recompute segment %d for layer %d", seg_idx, layer_idx);
        return NULL;
    }

    // Now the activation should be available
    if (info->state == CKPT_STATE_SAVED && info->activation) {
        return info->activation;
    }

    LOG_ERROR("Activation still not available for layer %d after recompute", layer_idx);
    return NULL;
}

bool nimcp_checkpoint_register_forward(
    nimcp_checkpoint_ctx_t* ctx,
    int start_layer,
    int end_layer,
    nimcp_checkpoint_forward_fn_t forward_fn,
    void* fn_ctx)
{
    if (!ctx || !forward_fn) return false;

    // Find the segment covering these layers
    for (int i = 0; i < ctx->num_segments; i++) {
        if (ctx->segments[i].start_layer <= start_layer &&
            ctx->segments[i].end_layer >= end_layer) {
            ctx->segments[i].forward_fn = forward_fn;
            ctx->segments[i].segment_ctx = fn_ctx;
            return true;
        }
    }

    LOG_WARN("No segment found for layers %d-%d", start_layer, end_layer);
    return false;
}

bool nimcp_checkpoint_register_backward(
    nimcp_checkpoint_ctx_t* ctx,
    int start_layer,
    int end_layer,
    nimcp_checkpoint_backward_fn_t backward_fn,
    void* fn_ctx)
{
    if (!ctx || !backward_fn) return false;

    // Find the segment covering these layers
    for (int i = 0; i < ctx->num_segments; i++) {
        if (ctx->segments[i].start_layer <= start_layer &&
            ctx->segments[i].end_layer >= end_layer) {
            ctx->segments[i].backward_fn = backward_fn;
            if (!ctx->segments[i].segment_ctx) {
                ctx->segments[i].segment_ctx = fn_ctx;
            }
            return true;
        }
    }

    LOG_WARN("No segment found for layers %d-%d", start_layer, end_layer);
    return false;
}

bool nimcp_checkpoint_recompute_segment(
    nimcp_checkpoint_ctx_t* ctx,
    int segment_idx)
{
    if (!ctx || segment_idx < 0 || segment_idx >= ctx->num_segments) {
        return false;
    }

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    nimcp_checkpoint_segment_t* seg = &ctx->segments[segment_idx];

    if (!seg->needs_recompute) {
        return true;  // Already computed
    }

    if (!seg->forward_fn) {
        LOG_ERROR("No forward function registered for segment %d", segment_idx);
        return false;
    }

    if (!seg->saved_input) {
        LOG_ERROR("No saved input for segment %d", segment_idx);
        return false;
    }

    cudaEvent_t start_event, stop_event;
    float elapsed_ms = 0;

    if (ctx->enable_profiling) {
        cudaEventCreate(&start_event);
        cudaEventCreate(&stop_event);
        cudaEventRecord(start_event);
    }

    // Mark layers as recomputing
    for (int i = seg->start_layer; i <= seg->end_layer; i++) {
        ctx->layer_info[i].state = CKPT_STATE_RECOMPUTING;
    }

    // Restore RNG state if needed
    // (RNG state restoration would be implementation-specific)

    // Allocate output tensor for segment
    // Using the end layer's expected shape
    int end_idx = seg->end_layer;
    if (end_idx < ctx->total_layers && ctx->layer_info[end_idx].activation) {
        // Use existing activation shape
        nimcp_gpu_tensor_t* existing = ctx->layer_info[end_idx].activation;
        seg->saved_output = nimcp_gpu_tensor_create(ctx->gpu_ctx,
                                                     existing->dims,
                                                     existing->ndim,
                                                     existing->precision);
    } else {
        // Use input shape as fallback
        seg->saved_output = nimcp_gpu_tensor_create(ctx->gpu_ctx,
                                                     seg->saved_input->dims,
                                                     seg->saved_input->ndim,
                                                     seg->saved_input->precision);
    }

    if (!seg->saved_output) {
        LOG_ERROR("Failed to allocate output tensor for segment %d", segment_idx);
        return false;
    }

    // Run forward pass for recomputation
    seg->forward_fn(seg->segment_ctx, seg->saved_input, seg->saved_output);

    // Store recomputed activations
    for (int i = seg->start_layer; i <= seg->end_layer; i++) {
        ctx->layer_info[i].state = CKPT_STATE_SAVED;
        // Note: In a full implementation, intermediate activations would be
        // captured by the forward function. Here we mark them as available.
    }

    seg->needs_recompute = false;
    ctx->recompute_count++;

    if (ctx->enable_profiling) {
        cudaEventRecord(stop_event);
        cudaEventSynchronize(stop_event);
        cudaEventElapsedTime(&elapsed_ms, start_event, stop_event);
        seg->recompute_time_ms += elapsed_ms;
        ctx->total_recompute_time_ms += elapsed_ms;
        cudaEventDestroy(start_event);
        cudaEventDestroy(stop_event);
    }

    if (ctx->verbose) {
        LOG_DEBUG("Recomputed segment %d (layers %d-%d) in %.2f ms",
                  segment_idx, seg->start_layer, seg->end_layer, elapsed_ms);
    }

    return true;
}

bool nimcp_checkpoint_free_activation(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx || layer_idx < 0 || layer_idx >= ctx->total_layers) return false;

    nimcp_layer_activation_info_t* info = &ctx->layer_info[layer_idx];

    if (info->activation) {
        ctx->current_memory -= info->activation_size;
        nimcp_gpu_tensor_destroy(info->activation);
        info->activation = NULL;
    }

    info->state = CKPT_STATE_FREED;
    return true;
}

//=============================================================================
// Checkpointed Function API
//=============================================================================

nimcp_checkpointed_fn_t* nimcp_checkpointed_fn_create(
    nimcp_checkpoint_forward_fn_t forward,
    nimcp_checkpoint_backward_fn_t backward,
    void* fn_ctx,
    bool preserve_rng)
{
    if (!forward) return NULL;

    nimcp_checkpointed_fn_t* fn = (nimcp_checkpointed_fn_t*)nimcp_calloc(1,
                                                                    sizeof(nimcp_checkpointed_fn_t));
    if (!fn) return NULL;

    fn->forward = forward;
    fn->backward = backward;
    fn->ctx = fn_ctx;
    fn->preserve_rng = preserve_rng;
    fn->rng_state = 0;
    fn->saved_input = NULL;
    fn->cached_output = NULL;
    fn->is_checkpointed = true;

    return fn;
}

void nimcp_checkpointed_fn_destroy(nimcp_checkpointed_fn_t* fn) {
    if (!fn) return;

    if (fn->saved_input) {
        nimcp_gpu_tensor_destroy(fn->saved_input);
    }
    if (fn->cached_output) {
        nimcp_gpu_tensor_destroy(fn->cached_output);
    }

    nimcp_free(fn);
}

nimcp_gpu_tensor_t* nimcp_checkpoint_function(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpointed_fn_t* fn,
    nimcp_gpu_tensor_t* input)
{
    if (!ctx || !fn || !input) return NULL;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Save input for backward pass recomputation
    if (fn->saved_input) {
        nimcp_gpu_tensor_destroy(fn->saved_input);
    }
    fn->saved_input = clone_tensor(ctx->gpu_ctx, input);
    if (!fn->saved_input) {
        LOG_ERROR("Failed to save input for checkpointed function");
        return NULL;
    }

    // Save RNG state if needed
    // (RNG state saving would be implementation-specific)

    // Allocate output tensor
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(ctx->gpu_ctx,
                                                          input->dims,
                                                          input->ndim,
                                                          input->precision);
    if (!output) {
        LOG_ERROR("Failed to allocate output for checkpointed function");
        return NULL;
    }

    cudaEvent_t start_event, stop_event;
    float elapsed_ms = 0;

    if (ctx->enable_profiling) {
        cudaEventCreate(&start_event);
        cudaEventCreate(&stop_event);
        cudaEventRecord(start_event);
    }

    // Run forward pass
    fn->forward(fn->ctx, input, output);

    if (ctx->enable_profiling) {
        cudaEventRecord(stop_event);
        cudaEventSynchronize(stop_event);
        cudaEventElapsedTime(&elapsed_ms, start_event, stop_event);
        ctx->total_forward_time_ms += elapsed_ms;
        cudaEventDestroy(start_event);
        cudaEventDestroy(stop_event);
    }

    // Cache output reference (but don't own it)
    fn->cached_output = output;

    ctx->forward_pass_count++;

    return output;
}

bool nimcp_checkpoint_function_backward(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpointed_fn_t* fn,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    if (!ctx || !fn || !grad_output || !grad_input) return false;

    // Initialize GPU recovery if not already initialized
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!fn->saved_input) {
        LOG_ERROR("No saved input for backward pass - was forward called?");
        return false;
    }

    if (!fn->backward) {
        LOG_ERROR("No backward function registered");
        return false;
    }

    cudaEvent_t start_event, stop_event;
    float elapsed_ms = 0;

    if (ctx->enable_profiling) {
        cudaEventCreate(&start_event);
        cudaEventCreate(&stop_event);
        cudaEventRecord(start_event);
    }

    // Restore RNG state if needed
    // (RNG state restoration would be implementation-specific)

    // Recompute forward pass
    nimcp_gpu_tensor_t* recomputed_output = nimcp_gpu_tensor_create(ctx->gpu_ctx,
                                                                     fn->saved_input->dims,
                                                                     fn->saved_input->ndim,
                                                                     fn->saved_input->precision);
    if (!recomputed_output) {
        LOG_ERROR("Failed to allocate recomputed output");
        return false;
    }

    fn->forward(fn->ctx, fn->saved_input, recomputed_output);
    ctx->recompute_count++;

    // Run backward pass
    fn->backward(fn->ctx, grad_output, grad_input);

    if (ctx->enable_profiling) {
        cudaEventRecord(stop_event);
        cudaEventSynchronize(stop_event);
        cudaEventElapsedTime(&elapsed_ms, start_event, stop_event);
        ctx->total_recompute_time_ms += elapsed_ms;
        cudaEventDestroy(start_event);
        cudaEventDestroy(stop_event);
    }

    // Clean up recomputed output
    nimcp_gpu_tensor_destroy(recomputed_output);

    // Free saved input after backward
    nimcp_gpu_tensor_destroy(fn->saved_input);
    fn->saved_input = NULL;
    fn->cached_output = NULL;

    ctx->backward_pass_count++;

    return true;
}

//=============================================================================
// Forward/Backward Pass Control
//=============================================================================

bool nimcp_checkpoint_begin_forward(nimcp_checkpoint_ctx_t* ctx) {
    if (!ctx) return false;

    if (ctx->in_forward_pass) {
        LOG_WARN("Already in forward pass");
        return false;
    }

    ctx->in_forward_pass = true;
    ctx->in_backward_pass = false;

    // Reset segment needs_recompute flags
    for (int i = 0; i < ctx->num_segments; i++) {
        ctx->segments[i].needs_recompute = false;
    }

    return true;
}

bool nimcp_checkpoint_end_forward(nimcp_checkpoint_ctx_t* ctx) {
    if (!ctx) return false;

    if (!ctx->in_forward_pass) {
        LOG_WARN("Not in forward pass");
        return false;
    }

    ctx->in_forward_pass = false;
    ctx->forward_pass_count++;

    // Mark non-checkpointed layers as needing recompute
    for (int i = 0; i < ctx->total_layers; i++) {
        if (ctx->layer_info[i].state != CKPT_STATE_SAVED) {
            int seg_idx = ctx->layer_info[i].segment_idx;
            if (seg_idx >= 0 && seg_idx < ctx->num_segments) {
                ctx->segments[seg_idx].needs_recompute = true;
            }
        }
    }

    return true;
}

bool nimcp_checkpoint_begin_backward(nimcp_checkpoint_ctx_t* ctx) {
    if (!ctx) return false;

    if (ctx->in_backward_pass) {
        LOG_WARN("Already in backward pass");
        return false;
    }

    ctx->in_backward_pass = true;
    ctx->in_forward_pass = false;

    return true;
}

bool nimcp_checkpoint_end_backward(nimcp_checkpoint_ctx_t* ctx) {
    if (!ctx) return false;

    if (!ctx->in_backward_pass) {
        LOG_WARN("Not in backward pass");
        return false;
    }

    ctx->in_backward_pass = false;
    ctx->backward_pass_count++;

    return true;
}

//=============================================================================
// Memory Estimation API
//=============================================================================

bool nimcp_checkpoint_estimate(
    const size_t* layer_sizes,
    int num_layers,
    nimcp_checkpoint_estimate_t* estimate)
{
    if (!layer_sizes || num_layers <= 0 || !estimate) return false;

    memset(estimate, 0, sizeof(nimcp_checkpoint_estimate_t));

    // Calculate total memory without checkpointing
    size_t total = 0;
    for (int i = 0; i < num_layers; i++) {
        total += layer_sizes[i];
    }
    estimate->no_checkpoint_memory = total;
    estimate->no_checkpoint_overhead = 1.0;

    // Calculate sqrt strategy memory
    int sqrt_interval = int_sqrt_ceil(num_layers);
    size_t sqrt_memory = 0;
    int sqrt_checkpoints = 0;

    for (int i = 0; i < num_layers; i += sqrt_interval) {
        sqrt_memory += layer_sizes[i];
        sqrt_checkpoints++;
    }

    // Add one segment worth of intermediate activations
    size_t max_segment_size = 0;
    for (int seg_start = 0; seg_start < num_layers; seg_start += sqrt_interval) {
        size_t seg_size = 0;
        for (int j = seg_start; j < seg_start + sqrt_interval && j < num_layers; j++) {
            seg_size += layer_sizes[j];
        }
        if (seg_size > max_segment_size) max_segment_size = seg_size;
    }
    sqrt_memory += max_segment_size;

    estimate->sqrt_checkpoint_memory = sqrt_memory;
    estimate->sqrt_recompute_overhead = 1.0 + 1.0 / sqrt_checkpoints;

    // Calculate EVERY_N strategy memory for N=1..10
    for (int n = 1; n <= 10; n++) {
        size_t every_n_memory = 0;
        int checkpoints = 0;

        for (int i = 0; i < num_layers; i += n) {
            every_n_memory += layer_sizes[i];
            checkpoints++;
        }

        // Add max segment size
        max_segment_size = 0;
        for (int seg_start = 0; seg_start < num_layers; seg_start += n) {
            size_t seg_size = 0;
            for (int j = seg_start; j < seg_start + n && j < num_layers; j++) {
                seg_size += layer_sizes[j];
            }
            if (seg_size > max_segment_size) max_segment_size = seg_size;
        }
        every_n_memory += max_segment_size;

        estimate->every_n_memory[n-1] = every_n_memory;
        estimate->every_n_overhead[n-1] = checkpoints > 0 ?
            1.0 + (double)(num_layers - checkpoints) / checkpoints : 1.0;
    }

    // Find optimal N (minimize memory while keeping overhead reasonable)
    estimate->optimal_n = sqrt_interval;
    estimate->optimal_n_memory = sqrt_memory;
    estimate->optimal_n_overhead = estimate->sqrt_recompute_overhead;

    for (int n = 1; n <= 10; n++) {
        if (estimate->every_n_memory[n-1] < estimate->optimal_n_memory &&
            estimate->every_n_overhead[n-1] < 2.0) {  // Max 2x overhead
            estimate->optimal_n = n;
            estimate->optimal_n_memory = estimate->every_n_memory[n-1];
            estimate->optimal_n_overhead = estimate->every_n_overhead[n-1];
        }
    }

    return true;
}

bool nimcp_checkpoint_recommend_strategy(
    const size_t* layer_sizes,
    int num_layers,
    size_t memory_budget,
    nimcp_checkpoint_strategy_t* strategy,
    int* checkpoint_n)
{
    if (!layer_sizes || num_layers <= 0 || !strategy) return false;

    nimcp_checkpoint_estimate_t estimate;
    if (!nimcp_checkpoint_estimate(layer_sizes, num_layers, &estimate)) {
        return false;
    }

    // Check if no checkpointing is needed
    if (estimate.no_checkpoint_memory <= memory_budget) {
        *strategy = CKPT_STRATEGY_NONE;
        if (checkpoint_n) *checkpoint_n = 0;
        return true;
    }

    // Check sqrt strategy
    if (estimate.sqrt_checkpoint_memory <= memory_budget) {
        *strategy = CKPT_STRATEGY_SQRT;
        if (checkpoint_n) *checkpoint_n = int_sqrt_ceil(num_layers);
        return true;
    }

    // Find best EVERY_N that fits
    for (int n = 1; n <= 10; n++) {
        if (estimate.every_n_memory[n-1] <= memory_budget) {
            *strategy = CKPT_STRATEGY_EVERY_N;
            if (checkpoint_n) *checkpoint_n = n;
            return true;
        }
    }

    // Can't fit, use most aggressive
    *strategy = CKPT_STRATEGY_EVERY_N;
    if (checkpoint_n) *checkpoint_n = 1;

    return true;
}

//=============================================================================
// Statistics API
//=============================================================================

void nimcp_checkpoint_get_stats(
    const nimcp_checkpoint_ctx_t* ctx,
    size_t* current_memory,
    size_t* peak_memory,
    size_t* saved_memory,
    int* recompute_count,
    double* recompute_time_ms)
{
    if (!ctx) return;

    if (current_memory) *current_memory = ctx->current_memory;
    if (peak_memory) *peak_memory = ctx->peak_memory;
    if (saved_memory) *saved_memory = ctx->saved_memory;
    if (recompute_count) *recompute_count = ctx->recompute_count;
    if (recompute_time_ms) *recompute_time_ms = ctx->total_recompute_time_ms;
}

void nimcp_checkpoint_print_stats(const nimcp_checkpoint_ctx_t* ctx) {
    if (!ctx) return;

    printf("=== Gradient Checkpointing Statistics ===\n");
    printf("Strategy: %d\n", ctx->strategy);
    printf("Total layers: %d\n", ctx->total_layers);
    printf("Number of segments: %d\n", ctx->num_segments);
    printf("Memory budget: %zu bytes (%.2f MB)\n",
           ctx->memory_budget, ctx->memory_budget / (1024.0 * 1024.0));
    printf("Current memory: %zu bytes (%.2f MB)\n",
           ctx->current_memory, ctx->current_memory / (1024.0 * 1024.0));
    printf("Peak memory (no ckpt): %zu bytes (%.2f MB)\n",
           ctx->peak_memory, ctx->peak_memory / (1024.0 * 1024.0));
    printf("Memory saved: %zu bytes (%.2f MB)\n",
           ctx->saved_memory, ctx->saved_memory / (1024.0 * 1024.0));
    printf("Recomputation count: %d\n", ctx->recompute_count);
    printf("Total recompute time: %.2f ms\n", ctx->total_recompute_time_ms);
    printf("Forward passes: %lu\n", (unsigned long)ctx->forward_pass_count);
    printf("Backward passes: %lu\n", (unsigned long)ctx->backward_pass_count);
    printf("=========================================\n");
}

int nimcp_checkpoint_get_info_string(
    const nimcp_checkpoint_ctx_t* ctx,
    char* buffer,
    size_t size)
{
    if (!ctx || !buffer || size == 0) return 0;

    return snprintf(buffer, size,
        "Checkpoint: strategy=%d, layers=%d, segments=%d, "
        "memory=%zu/%zu bytes, saved=%zu bytes, recomputes=%d",
        ctx->strategy, ctx->total_layers, ctx->num_segments,
        ctx->current_memory, ctx->memory_budget,
        ctx->saved_memory, ctx->recompute_count);
}

void nimcp_checkpoint_set_profiling(nimcp_checkpoint_ctx_t* ctx, bool enable) {
    if (ctx) ctx->enable_profiling = enable;
}

void nimcp_checkpoint_set_verbose(nimcp_checkpoint_ctx_t* ctx, bool verbose) {
    if (ctx) ctx->verbose = verbose;
}

//=============================================================================
// Utility Functions
//=============================================================================

int nimcp_checkpoint_sqrt_interval(int num_layers) {
    return int_sqrt_ceil(num_layers);
}

bool nimcp_checkpoint_is_checkpoint_layer(
    const nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx || layer_idx < 0 || layer_idx >= ctx->total_layers) return false;
    return ctx->layer_info[layer_idx].is_checkpoint_boundary;
}

int nimcp_checkpoint_get_segment_for_layer(
    const nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx || layer_idx < 0 || layer_idx >= ctx->total_layers) return -1;
    return ctx->layer_info[layer_idx].segment_idx;
}

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

#include "gpu/training/nimcp_gradient_checkpoint.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "CHECKPOINT_GPU"

nimcp_checkpoint_ctx_t* nimcp_checkpoint_ctx_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_checkpoint_strategy_t strategy,
    int total_layers,
    size_t memory_budget)
{
    LOG_WARN("CUDA not available - gradient checkpointing requires GPU");
    return NULL;
}

void nimcp_checkpoint_ctx_destroy(nimcp_checkpoint_ctx_t* ctx) {
    (void)ctx;
}

bool nimcp_checkpoint_ctx_reset(nimcp_checkpoint_ctx_t* ctx) {
    return false;
}

bool nimcp_checkpoint_configure(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpoint_strategy_t strategy,
    int checkpoint_every_n)
{
    return false;
}

bool nimcp_checkpoint_configure_selective(
    nimcp_checkpoint_ctx_t* ctx,
    const int* layer_indices,
    int num_layers)
{
    return false;
}

bool nimcp_checkpoint_auto_configure(
    nimcp_checkpoint_ctx_t* ctx,
    size_t available_memory,
    const size_t* layer_activation_sizes,
    int num_layers)
{
    return false;
}

bool nimcp_checkpoint_set_layer_size(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx,
    size_t size_bytes)
{
    return false;
}

bool nimcp_checkpoint_mark_layer(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx,
    nimcp_gpu_tensor_t* activation)
{
    return false;
}

bool nimcp_checkpoint_should_save(nimcp_checkpoint_ctx_t* ctx, int layer_idx) {
    return false;
}

nimcp_gpu_tensor_t* nimcp_checkpoint_get_activation(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    return NULL;
}

bool nimcp_checkpoint_register_forward(
    nimcp_checkpoint_ctx_t* ctx,
    int start_layer,
    int end_layer,
    nimcp_checkpoint_forward_fn_t forward_fn,
    void* fn_ctx)
{
    return false;
}

bool nimcp_checkpoint_register_backward(
    nimcp_checkpoint_ctx_t* ctx,
    int start_layer,
    int end_layer,
    nimcp_checkpoint_backward_fn_t backward_fn,
    void* fn_ctx)
{
    return false;
}

bool nimcp_checkpoint_recompute_segment(nimcp_checkpoint_ctx_t* ctx, int segment_idx) {
    return false;
}

bool nimcp_checkpoint_free_activation(nimcp_checkpoint_ctx_t* ctx, int layer_idx) {
    return false;
}

nimcp_checkpointed_fn_t* nimcp_checkpointed_fn_create(
    nimcp_checkpoint_forward_fn_t forward,
    nimcp_checkpoint_backward_fn_t backward,
    void* fn_ctx,
    bool preserve_rng)
{
    return NULL;
}

void nimcp_checkpointed_fn_destroy(nimcp_checkpointed_fn_t* fn) {
    (void)fn;
}

nimcp_gpu_tensor_t* nimcp_checkpoint_function(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpointed_fn_t* fn,
    nimcp_gpu_tensor_t* input)
{
    return NULL;
}

bool nimcp_checkpoint_function_backward(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpointed_fn_t* fn,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    return false;
}

bool nimcp_checkpoint_begin_forward(nimcp_checkpoint_ctx_t* ctx) {
    return false;
}

bool nimcp_checkpoint_end_forward(nimcp_checkpoint_ctx_t* ctx) {
    return false;
}

bool nimcp_checkpoint_begin_backward(nimcp_checkpoint_ctx_t* ctx) {
    return false;
}

bool nimcp_checkpoint_end_backward(nimcp_checkpoint_ctx_t* ctx) {
    return false;
}

bool nimcp_checkpoint_estimate(
    const size_t* layer_sizes,
    int num_layers,
    nimcp_checkpoint_estimate_t* estimate)
{
    return false;
}

bool nimcp_checkpoint_recommend_strategy(
    const size_t* layer_sizes,
    int num_layers,
    size_t memory_budget,
    nimcp_checkpoint_strategy_t* strategy,
    int* checkpoint_n)
{
    return false;
}

void nimcp_checkpoint_get_stats(
    const nimcp_checkpoint_ctx_t* ctx,
    size_t* current_memory,
    size_t* peak_memory,
    size_t* saved_memory,
    int* recompute_count,
    double* recompute_time_ms)
{
    (void)ctx;
    (void)current_memory;
    (void)peak_memory;
    (void)saved_memory;
    (void)recompute_count;
    (void)recompute_time_ms;
}

void nimcp_checkpoint_print_stats(const nimcp_checkpoint_ctx_t* ctx) {
    (void)ctx;
}

int nimcp_checkpoint_get_info_string(
    const nimcp_checkpoint_ctx_t* ctx,
    char* buffer,
    size_t size)
{
    return 0;
}

void nimcp_checkpoint_set_profiling(nimcp_checkpoint_ctx_t* ctx, bool enable) {
    (void)ctx;
    (void)enable;
}

void nimcp_checkpoint_set_verbose(nimcp_checkpoint_ctx_t* ctx, bool verbose) {
    (void)ctx;
    (void)verbose;
}

int nimcp_checkpoint_sqrt_interval(int num_layers) {
    if (num_layers <= 0) return 0;
    return (int)ceil(sqrt((double)num_layers));
}

bool nimcp_checkpoint_is_checkpoint_layer(
    const nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    return false;
}

int nimcp_checkpoint_get_segment_for_layer(
    const nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    return -1;
}

#endif // NIMCP_ENABLE_CUDA
