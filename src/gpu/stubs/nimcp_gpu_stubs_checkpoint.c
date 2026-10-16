/**
 * @file nimcp_gpu_stubs_checkpoint.c
 * @brief CPU fallback implementations for gradient and activation checkpointing
 *
 * WHAT: CPU implementations for all functions in nimcp_gradient_checkpoint.h
 *       and nimcp_activation_checkpoint.h
 * WHY:  Enables full checkpointing API on CPU-only systems for testing and
 *       API compatibility. On CPU, memory pressure is less critical so
 *       checkpointing stores activation pointers directly.
 * HOW:  Allocates CPU-side structures, stores/retrieves tensor pointers in
 *       layer_info arrays, implements strategy-based checkpoint selection
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include "gpu/training/nimcp_gradient_checkpoint.h"
#include "gpu/training/nimcp_activation_checkpoint.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/*=============================================================================
 * Helper: find segment by layer range
 *=============================================================================*/
static int find_segment(const nimcp_checkpoint_ctx_t* ctx,
                        int start_layer, int end_layer)
{
    if (!ctx) return -1;
    for (int i = 0; i < ctx->num_segments; i++) {
        if (ctx->segments[i].start_layer == start_layer &&
            ctx->segments[i].end_layer == end_layer) {
            return i;
        }
    }
    return -1;
}

/*=============================================================================
 * Helper: ensure segment capacity
 *=============================================================================*/
static bool ensure_segment_capacity(nimcp_checkpoint_ctx_t* ctx)
{
    if (!ctx) return false;
    if (ctx->num_segments < ctx->max_segments) return true;

    int new_cap = ctx->max_segments * 2;
    if (new_cap < 16) new_cap = 16;
    nimcp_checkpoint_segment_t* new_segs = nimcp_calloc(
        (size_t)new_cap, sizeof(nimcp_checkpoint_segment_t));
    if (!new_segs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "ensure_segment_capacity: allocation failed");
        return false;
    }
    if (ctx->segments && ctx->num_segments > 0) {
        memcpy(new_segs, ctx->segments,
               (size_t)ctx->num_segments * sizeof(nimcp_checkpoint_segment_t));
    }
    nimcp_free(ctx->segments);
    ctx->segments = new_segs;
    ctx->max_segments = new_cap;
    return true;
}

/*=============================================================================
 * Gradient Checkpoint - Context Lifecycle API
 *=============================================================================*/

nimcp_checkpoint_ctx_t* nimcp_checkpoint_ctx_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_checkpoint_strategy_t strategy,
    int total_layers,
    size_t memory_budget)
{
    (void)gpu_ctx;  /* Not used in CPU fallback */

    if (total_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_ctx_create: total_layers must be > 0");
        return NULL;
    }

    nimcp_checkpoint_ctx_t* ctx = nimcp_calloc(1, sizeof(nimcp_checkpoint_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_checkpoint_ctx_create: allocation failed");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->strategy = strategy;
    ctx->total_layers = total_layers;
    ctx->memory_budget = memory_budget;

    /* Allocate layer info array */
    ctx->layer_info = nimcp_calloc((size_t)total_layers,
                                   sizeof(nimcp_layer_activation_info_t));
    if (!ctx->layer_info) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_checkpoint_ctx_create: layer_info allocation failed");
        return NULL;
    }

    for (int i = 0; i < total_layers; i++) {
        ctx->layer_info[i].layer_idx = i;
        ctx->layer_info[i].state = CKPT_STATE_NONE;
        ctx->layer_info[i].segment_idx = -1;
    }

    /* Allocate initial segments array */
    ctx->max_segments = 16;
    ctx->segments = nimcp_calloc((size_t)ctx->max_segments,
                                 sizeof(nimcp_checkpoint_segment_t));
    if (!ctx->segments) {
        nimcp_free(ctx->layer_info);
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_checkpoint_ctx_create: segments allocation failed");
        return NULL;
    }

    /* Set default checkpoint_every_n for EVERY_N strategy */
    if (strategy == CKPT_STRATEGY_SQRT) {
        ctx->checkpoint_every_n = nimcp_checkpoint_sqrt_interval(total_layers);
    } else if (strategy == CKPT_STRATEGY_EVERY_N) {
        ctx->checkpoint_every_n = 1;  /* Default, user should call configure */
    }

    ctx->configured = true;

    return ctx;
}

void nimcp_checkpoint_ctx_destroy(nimcp_checkpoint_ctx_t* ctx)
{
    if (!ctx) return;

    nimcp_free(ctx->layer_info);
    nimcp_free(ctx->segments);
    nimcp_free(ctx->selective_layers);

    /* Free tensor pool */
    if (ctx->tensor_pool) {
        nimcp_free(ctx->pool_in_use);
        nimcp_free(ctx->tensor_pool);
    }

    nimcp_free(ctx);
}

bool nimcp_checkpoint_ctx_reset(nimcp_checkpoint_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_ctx_reset: ctx is NULL");
        return false;
    }

    /* Clear all layer activations */
    for (int i = 0; i < ctx->total_layers; i++) {
        ctx->layer_info[i].state = CKPT_STATE_NONE;
        ctx->layer_info[i].activation = NULL;
    }

    /* Reset segments */
    if (ctx->segments) {
        memset(ctx->segments, 0,
               (size_t)ctx->max_segments * sizeof(nimcp_checkpoint_segment_t));
    }
    ctx->num_segments = 0;

    /* Reset statistics */
    ctx->current_memory = 0;
    ctx->saved_memory = 0;
    ctx->recompute_count = 0;
    ctx->total_recompute_time_ms = 0.0;
    ctx->total_forward_time_ms = 0.0;

    /* Reset state flags */
    ctx->in_forward_pass = false;
    ctx->in_backward_pass = false;

    return true;
}

/*=============================================================================
 * Gradient Checkpoint - Configuration API
 *=============================================================================*/

bool nimcp_checkpoint_configure(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpoint_strategy_t strategy,
    int checkpoint_every_n)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_configure: ctx is NULL");
        return false;
    }

    ctx->strategy = strategy;
    ctx->checkpoint_every_n = checkpoint_every_n;

    if (strategy == CKPT_STRATEGY_SQRT) {
        ctx->checkpoint_every_n =
            nimcp_checkpoint_sqrt_interval(ctx->total_layers);
    }

    ctx->configured = true;
    return true;
}

bool nimcp_checkpoint_configure_selective(
    nimcp_checkpoint_ctx_t* ctx,
    const int* layer_indices,
    int num_layers)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_configure_selective: ctx is NULL");
        return false;
    }
    if (!layer_indices || num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_configure_selective: invalid layer_indices or num_layers");
        return false;
    }

    nimcp_free(ctx->selective_layers);
    ctx->selective_layers = nimcp_malloc((size_t)num_layers * sizeof(int));
    if (!ctx->selective_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_checkpoint_configure_selective: allocation failed");
        return false;
    }

    memcpy(ctx->selective_layers, layer_indices,
           (size_t)num_layers * sizeof(int));
    ctx->num_selective_layers = num_layers;
    ctx->strategy = CKPT_STRATEGY_SELECTIVE;
    ctx->configured = true;

    /* Mark selected layers as checkpoint boundaries */
    for (int i = 0; i < num_layers; i++) {
        int idx = layer_indices[i];
        if (idx >= 0 && idx < ctx->total_layers) {
            ctx->layer_info[idx].is_checkpoint_boundary = true;
        }
    }

    return true;
}

bool nimcp_checkpoint_auto_configure(
    nimcp_checkpoint_ctx_t* ctx,
    size_t available_memory,
    const size_t* layer_activation_sizes,
    int num_layers)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_auto_configure: ctx is NULL");
        return false;
    }
    if (!layer_activation_sizes || num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_auto_configure: invalid parameters");
        return false;
    }

    /* Compute total activation memory */
    size_t total_activation = 0;
    for (int i = 0; i < num_layers; i++) {
        if (i < ctx->total_layers) {
            ctx->layer_info[i].activation_size = layer_activation_sizes[i];
        }
        total_activation += layer_activation_sizes[i];
    }

    ctx->peak_memory = total_activation;
    ctx->memory_budget = available_memory;

    if (total_activation <= available_memory) {
        /* Enough memory - no checkpointing needed */
        ctx->strategy = CKPT_STRATEGY_NONE;
    } else {
        /* Use sqrt strategy as default auto-config */
        ctx->strategy = CKPT_STRATEGY_SQRT;
        ctx->checkpoint_every_n =
            nimcp_checkpoint_sqrt_interval(ctx->total_layers);
    }

    ctx->configured = true;
    return true;
}

bool nimcp_checkpoint_set_layer_size(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx,
    size_t size_bytes)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_set_layer_size: ctx is NULL");
        return false;
    }
    if (layer_idx < 0 || layer_idx >= ctx->total_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_set_layer_size: layer_idx out of range");
        return false;
    }

    ctx->layer_info[layer_idx].activation_size = size_bytes;
    return true;
}

/*=============================================================================
 * Gradient Checkpoint - Operations API
 *=============================================================================*/

bool nimcp_checkpoint_mark_layer(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx,
    nimcp_gpu_tensor_t* activation)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_mark_layer: ctx is NULL");
        return false;
    }
    if (layer_idx < 0 || layer_idx >= ctx->total_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_mark_layer: layer_idx out of range");
        return false;
    }

    if (nimcp_checkpoint_should_save(ctx, layer_idx)) {
        /* Store the activation pointer (CPU fallback: store directly) */
        ctx->layer_info[layer_idx].activation = activation;
        ctx->layer_info[layer_idx].state = CKPT_STATE_SAVED;
        ctx->layer_info[layer_idx].is_checkpoint_boundary = true;
        ctx->current_memory += ctx->layer_info[layer_idx].activation_size;
    } else {
        ctx->layer_info[layer_idx].state = CKPT_STATE_FREED;
        ctx->saved_memory += ctx->layer_info[layer_idx].activation_size;
    }

    return true;
}

bool nimcp_checkpoint_should_save(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx) return false;
    if (layer_idx < 0 || layer_idx >= ctx->total_layers) return false;

    switch (ctx->strategy) {
    case CKPT_STRATEGY_NONE:
        return true;  /* Save all activations */

    case CKPT_STRATEGY_SQRT: {
        int interval = nimcp_checkpoint_sqrt_interval(ctx->total_layers);
        if (interval <= 0) interval = 1;
        /* Save at every interval and always save first/last */
        return (layer_idx == 0) ||
               (layer_idx == ctx->total_layers - 1) ||
               (layer_idx % interval == 0);
    }

    case CKPT_STRATEGY_EVERY_N: {
        int n = ctx->checkpoint_every_n;
        if (n <= 0) n = 1;
        /* Save at every Nth layer, plus first and last */
        return (layer_idx == 0) ||
               (layer_idx == ctx->total_layers - 1) ||
               (layer_idx % n == 0);
    }

    case CKPT_STRATEGY_SELECTIVE: {
        /* Check if layer is in the selective list */
        for (int i = 0; i < ctx->num_selective_layers; i++) {
            if (ctx->selective_layers[i] == layer_idx) return true;
        }
        return false;
    }

    case CKPT_STRATEGY_MEMORY_BUDGET:
        /* Budget-based: save if under budget, or if first/last */
        if (layer_idx == 0 || layer_idx == ctx->total_layers - 1) return true;
        return (ctx->current_memory + ctx->layer_info[layer_idx].activation_size
                <= ctx->memory_budget);

    default:
        return true;
    }
}

nimcp_gpu_tensor_t* nimcp_checkpoint_get_activation(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_get_activation: ctx is NULL");
        return NULL;
    }
    if (layer_idx < 0 || layer_idx >= ctx->total_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_get_activation: layer_idx out of range");
        return NULL;
    }

    nimcp_layer_activation_info_t* info = &ctx->layer_info[layer_idx];

    if (info->state == CKPT_STATE_SAVED && info->activation) {
        return info->activation;
    }

    /* Activation not saved - try to recompute from nearest segment */
    if (info->segment_idx >= 0 && info->segment_idx < ctx->num_segments) {
        nimcp_checkpoint_segment_t* seg = &ctx->segments[info->segment_idx];
        if (seg->forward_fn && seg->saved_input) {
            info->state = CKPT_STATE_RECOMPUTING;
            /* Recompute: forward_fn writes into a temporary output.
               On CPU fallback we just call the function. */
            if (seg->saved_output) {
                seg->forward_fn(seg->segment_ctx, seg->saved_input,
                                seg->saved_output);
                info->activation = seg->saved_output;
            }
            info->state = CKPT_STATE_SAVED;
            ctx->recompute_count++;
            return info->activation;
        }
    }

    /* Could not recompute */
    return NULL;
}

bool nimcp_checkpoint_register_forward(
    nimcp_checkpoint_ctx_t* ctx,
    int start_layer,
    int end_layer,
    nimcp_checkpoint_forward_fn_t forward_fn,
    void* fn_ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_register_forward: ctx is NULL");
        return false;
    }
    if (!forward_fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_register_forward: forward_fn is NULL");
        return false;
    }
    if (start_layer < 0 || end_layer >= ctx->total_layers ||
        start_layer > end_layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_register_forward: invalid layer range");
        return false;
    }

    /* Find existing segment or create new one */
    int seg_idx = find_segment(ctx, start_layer, end_layer);
    if (seg_idx < 0) {
        if (!ensure_segment_capacity(ctx)) return false;
        seg_idx = ctx->num_segments++;
        ctx->segments[seg_idx].start_layer = start_layer;
        ctx->segments[seg_idx].end_layer = end_layer;
    }

    ctx->segments[seg_idx].forward_fn = forward_fn;
    ctx->segments[seg_idx].segment_ctx = fn_ctx;

    /* Mark layers as belonging to this segment */
    for (int i = start_layer; i <= end_layer && i < ctx->total_layers; i++) {
        ctx->layer_info[i].segment_idx = seg_idx;
    }

    return true;
}

bool nimcp_checkpoint_register_backward(
    nimcp_checkpoint_ctx_t* ctx,
    int start_layer,
    int end_layer,
    nimcp_checkpoint_backward_fn_t backward_fn,
    void* fn_ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_register_backward: ctx is NULL");
        return false;
    }
    if (!backward_fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_register_backward: backward_fn is NULL");
        return false;
    }
    if (start_layer < 0 || end_layer >= ctx->total_layers ||
        start_layer > end_layer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_register_backward: invalid layer range");
        return false;
    }

    /* Find existing segment or create new one */
    int seg_idx = find_segment(ctx, start_layer, end_layer);
    if (seg_idx < 0) {
        if (!ensure_segment_capacity(ctx)) return false;
        seg_idx = ctx->num_segments++;
        ctx->segments[seg_idx].start_layer = start_layer;
        ctx->segments[seg_idx].end_layer = end_layer;
    }

    ctx->segments[seg_idx].backward_fn = backward_fn;
    if (fn_ctx) {
        ctx->segments[seg_idx].segment_ctx = fn_ctx;
    }

    return true;
}

bool nimcp_checkpoint_recompute_segment(
    nimcp_checkpoint_ctx_t* ctx,
    int segment_idx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_recompute_segment: ctx is NULL");
        return false;
    }
    if (segment_idx < 0 || segment_idx >= ctx->num_segments) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_recompute_segment: segment_idx out of range");
        return false;
    }

    nimcp_checkpoint_segment_t* seg = &ctx->segments[segment_idx];

    if (!seg->forward_fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_recompute_segment: no forward_fn registered");
        return false;
    }

    if (!seg->saved_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_recompute_segment: no saved_input available");
        return false;
    }

    /* Mark layers as recomputing */
    for (int i = seg->start_layer; i <= seg->end_layer &&
         i < ctx->total_layers; i++) {
        ctx->layer_info[i].state = CKPT_STATE_RECOMPUTING;
    }

    /* Re-execute forward function */
    if (seg->saved_output) {
        seg->forward_fn(seg->segment_ctx, seg->saved_input, seg->saved_output);
    }

    /* Mark layers as saved */
    for (int i = seg->start_layer; i <= seg->end_layer &&
         i < ctx->total_layers; i++) {
        ctx->layer_info[i].state = CKPT_STATE_SAVED;
    }

    seg->needs_recompute = false;
    ctx->recompute_count++;

    return true;
}

bool nimcp_checkpoint_free_activation(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_free_activation: ctx is NULL");
        return false;
    }
    if (layer_idx < 0 || layer_idx >= ctx->total_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_free_activation: layer_idx out of range");
        return false;
    }

    nimcp_layer_activation_info_t* info = &ctx->layer_info[layer_idx];
    if (info->state == CKPT_STATE_SAVED && info->activation) {
        /* On CPU fallback, we don't own the activation tensors, just clear ref */
        if (info->activation_size <= ctx->current_memory) {
            ctx->current_memory -= info->activation_size;
        }
        ctx->saved_memory += info->activation_size;
    }

    info->activation = NULL;
    info->state = CKPT_STATE_FREED;

    return true;
}

/*=============================================================================
 * Gradient Checkpoint - Checkpointed Function API
 *=============================================================================*/

nimcp_checkpointed_fn_t* nimcp_checkpointed_fn_create(
    nimcp_checkpoint_forward_fn_t forward,
    nimcp_checkpoint_backward_fn_t backward,
    void* fn_ctx,
    bool preserve_rng)
{
    if (!forward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpointed_fn_create: forward is NULL");
        return NULL;
    }

    nimcp_checkpointed_fn_t* fn = nimcp_calloc(1, sizeof(nimcp_checkpointed_fn_t));
    if (!fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_checkpointed_fn_create: allocation failed");
        return NULL;
    }

    fn->forward = forward;
    fn->backward = backward;
    fn->ctx = fn_ctx;
    fn->preserve_rng = preserve_rng;
    fn->is_checkpointed = true;

    return fn;
}

void nimcp_checkpointed_fn_destroy(nimcp_checkpointed_fn_t* fn)
{
    if (!fn) return;
    /* saved_input and cached_output are not owned - just clear pointers */
    nimcp_free(fn);
}

nimcp_gpu_tensor_t* nimcp_checkpoint_function(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpointed_fn_t* fn,
    nimcp_gpu_tensor_t* input)
{
    (void)ctx;  /* CPU fallback doesn't need GPU context for this */

    if (!fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_function: fn is NULL");
        return NULL;
    }
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_function: input is NULL");
        return NULL;
    }
    if (!fn->forward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_function: fn->forward is NULL");
        return NULL;
    }

    /* Save input for potential recomputation during backward */
    fn->saved_input = input;

    /* Execute forward pass */
    /* On CPU, we run forward and store the output.
       In a real GPU implementation, intermediate activations would be freed. */
    if (fn->cached_output) {
        fn->forward(fn->ctx, input, fn->cached_output);
        return fn->cached_output;
    }

    /* If no cached_output buffer, forward writes in-place conceptually.
       Return input as pass-through for stub behavior. */
    return input;
}

bool nimcp_checkpoint_function_backward(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpointed_fn_t* fn,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    (void)ctx;

    if (!fn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_function_backward: fn is NULL");
        return false;
    }
    if (!grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_function_backward: grad_output or grad_input is NULL");
        return false;
    }

    /* Recompute forward from saved input if we have the forward function */
    if (fn->forward && fn->saved_input && fn->cached_output) {
        fn->forward(fn->ctx, fn->saved_input, fn->cached_output);
    }

    /* Execute backward pass */
    if (fn->backward) {
        fn->backward(fn->ctx, grad_output, grad_input);
    }

    if (ctx) {
        ctx->recompute_count++;
    }

    return true;
}

/*=============================================================================
 * Gradient Checkpoint - Forward/Backward Pass Control
 *=============================================================================*/

bool nimcp_checkpoint_begin_forward(nimcp_checkpoint_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_begin_forward: ctx is NULL");
        return false;
    }

    ctx->in_forward_pass = true;
    ctx->in_backward_pass = false;
    ctx->forward_pass_count++;

    return true;
}

bool nimcp_checkpoint_end_forward(nimcp_checkpoint_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_end_forward: ctx is NULL");
        return false;
    }

    ctx->in_forward_pass = false;
    return true;
}

bool nimcp_checkpoint_begin_backward(nimcp_checkpoint_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_begin_backward: ctx is NULL");
        return false;
    }

    ctx->in_backward_pass = true;
    ctx->in_forward_pass = false;
    ctx->backward_pass_count++;

    return true;
}

bool nimcp_checkpoint_end_backward(nimcp_checkpoint_ctx_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_end_backward: ctx is NULL");
        return false;
    }

    ctx->in_backward_pass = false;
    return true;
}

/*=============================================================================
 * Gradient Checkpoint - Memory Estimation API
 *=============================================================================*/

bool nimcp_checkpoint_estimate(
    const size_t* layer_sizes,
    int num_layers,
    nimcp_checkpoint_estimate_t* estimate)
{
    if (!layer_sizes || !estimate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_estimate: layer_sizes or estimate is NULL");
        return false;
    }
    if (num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_estimate: num_layers must be > 0");
        return false;
    }

    memset(estimate, 0, sizeof(*estimate));

    /* Compute total memory without checkpointing */
    size_t total = 0;
    for (int i = 0; i < num_layers; i++) {
        total += layer_sizes[i];
    }
    estimate->no_checkpoint_memory = total;
    estimate->no_checkpoint_overhead = 1.0;

    /* Sqrt strategy: checkpoint every sqrt(N) layers */
    int sqrt_interval = nimcp_checkpoint_sqrt_interval(num_layers);
    size_t sqrt_mem = 0;
    int sqrt_saved_count = 0;
    for (int i = 0; i < num_layers; i++) {
        if (i == 0 || i == num_layers - 1 ||
            (sqrt_interval > 0 && i % sqrt_interval == 0)) {
            sqrt_mem += layer_sizes[i];
            sqrt_saved_count++;
        }
    }
    estimate->sqrt_checkpoint_memory = sqrt_mem;
    /* Overhead: one extra forward per segment */
    estimate->sqrt_recompute_overhead = (sqrt_saved_count > 0)
        ? 1.0 + (double)(num_layers - sqrt_saved_count) / (double)num_layers
        : 1.0;

    /* Every-N strategies (N=1..10) */
    int best_n = 1;
    size_t best_mem = total;
    double best_overhead = 2.0;

    for (int n = 1; n <= 10; n++) {
        size_t n_mem = 0;
        int n_saved = 0;
        for (int i = 0; i < num_layers; i++) {
            if (i == 0 || i == num_layers - 1 ||
                (i % n == 0)) {
                n_mem += layer_sizes[i];
                n_saved++;
            }
        }
        estimate->every_n_memory[n - 1] = n_mem;
        estimate->every_n_overhead[n - 1] = (n_saved > 0)
            ? 1.0 + (double)(num_layers - n_saved) / (double)num_layers
            : 1.0;

        /* Track optimal: best memory-overhead tradeoff */
        double score = (double)n_mem * estimate->every_n_overhead[n - 1];
        double best_score = (double)best_mem * best_overhead;
        if (score < best_score) {
            best_n = n;
            best_mem = n_mem;
            best_overhead = estimate->every_n_overhead[n - 1];
        }
    }

    estimate->optimal_n = best_n;
    estimate->optimal_n_memory = best_mem;
    estimate->optimal_n_overhead = best_overhead;

    return true;
}

bool nimcp_checkpoint_recommend_strategy(
    const size_t* layer_sizes,
    int num_layers,
    size_t memory_budget,
    nimcp_checkpoint_strategy_t* strategy,
    int* checkpoint_n)
{
    if (!layer_sizes || !strategy || !checkpoint_n) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_checkpoint_recommend_strategy: NULL parameter");
        return false;
    }
    if (num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_checkpoint_recommend_strategy: num_layers must be > 0");
        return false;
    }

    /* Compute total */
    size_t total = 0;
    for (int i = 0; i < num_layers; i++) {
        total += layer_sizes[i];
    }

    if (total <= memory_budget) {
        *strategy = CKPT_STRATEGY_NONE;
        *checkpoint_n = 0;
        return true;
    }

    /* Try sqrt first */
    nimcp_checkpoint_estimate_t est;
    nimcp_checkpoint_estimate(layer_sizes, num_layers, &est);

    if (est.sqrt_checkpoint_memory <= memory_budget) {
        *strategy = CKPT_STRATEGY_SQRT;
        *checkpoint_n = nimcp_checkpoint_sqrt_interval(num_layers);
        return true;
    }

    /* Try EVERY_N from 1..10 */
    for (int n = 1; n <= 10; n++) {
        if (est.every_n_memory[n - 1] <= memory_budget) {
            *strategy = CKPT_STRATEGY_EVERY_N;
            *checkpoint_n = n;
            return true;
        }
    }

    /* Budget is very tight - use every layer (EVERY_N with N=1) */
    *strategy = CKPT_STRATEGY_EVERY_N;
    *checkpoint_n = 1;
    return true;
}

/*=============================================================================
 * Gradient Checkpoint - Statistics API
 *=============================================================================*/

void nimcp_checkpoint_get_stats(
    const nimcp_checkpoint_ctx_t* ctx,
    size_t* current_memory,
    size_t* peak_memory,
    size_t* saved_memory,
    int* recompute_count,
    double* recompute_time_ms)
{
    if (!ctx) {
        if (current_memory)   *current_memory = 0;
        if (peak_memory)      *peak_memory = 0;
        if (saved_memory)     *saved_memory = 0;
        if (recompute_count)  *recompute_count = 0;
        if (recompute_time_ms) *recompute_time_ms = 0.0;
        return;
    }

    if (current_memory)   *current_memory = ctx->current_memory;
    if (peak_memory)      *peak_memory = ctx->peak_memory;
    if (saved_memory)     *saved_memory = ctx->saved_memory;
    if (recompute_count)  *recompute_count = ctx->recompute_count;
    if (recompute_time_ms) *recompute_time_ms = ctx->total_recompute_time_ms;
}

void nimcp_checkpoint_print_stats(const nimcp_checkpoint_ctx_t* ctx)
{
    if (!ctx) return;

    printf("=== Gradient Checkpoint Statistics ===\n");
    printf("Strategy:          %d\n", (int)ctx->strategy);
    printf("Total layers:      %d\n", ctx->total_layers);
    printf("Num segments:      %d\n", ctx->num_segments);
    printf("Current memory:    %zu bytes\n", ctx->current_memory);
    printf("Peak memory:       %zu bytes\n", ctx->peak_memory);
    printf("Saved memory:      %zu bytes\n", ctx->saved_memory);
    printf("Recompute count:   %d\n", ctx->recompute_count);
    printf("Recompute time:    %.3f ms\n", ctx->total_recompute_time_ms);
    printf("Forward passes:    %lu\n", (unsigned long)ctx->forward_pass_count);
    printf("Backward passes:   %lu\n", (unsigned long)ctx->backward_pass_count);
    printf("=====================================\n");
}

int nimcp_checkpoint_get_info_string(
    const nimcp_checkpoint_ctx_t* ctx,
    char* buffer,
    size_t size)
{
    if (!ctx || !buffer || size == 0) return 0;

    const char* strategy_names[] = {
        "NONE", "SQRT", "EVERY_N", "SELECTIVE", "MEMORY_BUDGET"
    };
    const char* strat_name = "UNKNOWN";
    if ((int)ctx->strategy >= 0 && (int)ctx->strategy <= 4) {
        strat_name = strategy_names[(int)ctx->strategy];
    }

    return snprintf(buffer, size,
        "Checkpoint[strategy=%s layers=%d segments=%d "
        "mem=%zu/%zu saved=%zu recomputes=%d]",
        strat_name, ctx->total_layers, ctx->num_segments,
        ctx->current_memory, ctx->peak_memory,
        ctx->saved_memory, ctx->recompute_count);
}

void nimcp_checkpoint_set_profiling(
    nimcp_checkpoint_ctx_t* ctx,
    bool enable)
{
    if (!ctx) return;
    ctx->enable_profiling = enable;
}

void nimcp_checkpoint_set_verbose(
    nimcp_checkpoint_ctx_t* ctx,
    bool verbose)
{
    if (!ctx) return;
    ctx->verbose = verbose;
}

/*=============================================================================
 * Gradient Checkpoint - Utility Functions
 *=============================================================================*/

int nimcp_checkpoint_sqrt_interval(int num_layers)
{
    if (num_layers <= 0) return 1;
    int interval = (int)ceil(sqrt((double)num_layers));
    if (interval <= 0) interval = 1;
    return interval;
}

bool nimcp_checkpoint_is_checkpoint_layer(
    const nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx) return false;
    if (layer_idx < 0 || layer_idx >= ctx->total_layers) return false;

    switch (ctx->strategy) {
    case CKPT_STRATEGY_NONE:
        return true;

    case CKPT_STRATEGY_SQRT: {
        int interval = nimcp_checkpoint_sqrt_interval(ctx->total_layers);
        if (interval <= 0) interval = 1;
        return (layer_idx == 0) ||
               (layer_idx == ctx->total_layers - 1) ||
               (layer_idx % interval == 0);
    }

    case CKPT_STRATEGY_EVERY_N: {
        int n = ctx->checkpoint_every_n;
        if (n <= 0) n = 1;
        return (layer_idx == 0) ||
               (layer_idx == ctx->total_layers - 1) ||
               (layer_idx % n == 0);
    }

    case CKPT_STRATEGY_SELECTIVE:
        for (int i = 0; i < ctx->num_selective_layers; i++) {
            if (ctx->selective_layers[i] == layer_idx) return true;
        }
        return false;

    case CKPT_STRATEGY_MEMORY_BUDGET:
        return ctx->layer_info[layer_idx].is_checkpoint_boundary;

    default:
        return false;
    }
}

int nimcp_checkpoint_get_segment_for_layer(
    const nimcp_checkpoint_ctx_t* ctx,
    int layer_idx)
{
    if (!ctx) return -1;
    if (layer_idx < 0 || layer_idx >= ctx->total_layers) return -1;

    return ctx->layer_info[layer_idx].segment_idx;
}

/*=============================================================================
 * Activation Checkpoint - Configuration Init
 *=============================================================================*/

void nimcp_seq_checkpoint_config_init(nimcp_seq_checkpoint_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->strategy = CKPT_STRATEGY_SQRT;
    config->checkpoint_every_n = 0;  /* Auto from sqrt */
    config->memory_budget = 0;       /* Unlimited */
    config->preserve_rng = false;
    config->enable_profiling = false;
    config->verbose = false;
    config->auto_configure = false;
}

/*=============================================================================
 * Activation Checkpoint - Sequential Checkpoint Lifecycle
 *=============================================================================*/

nimcp_sequential_checkpoint_t* nimcp_sequential_checkpoint_create(
    nimcp_gpu_context_t* gpu_ctx,
    int num_layers,
    size_t memory_budget)
{
    (void)gpu_ctx;

    if (num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_create: num_layers must be > 0");
        return NULL;
    }

    nimcp_sequential_checkpoint_t* ckpt = nimcp_calloc(
        1, sizeof(nimcp_sequential_checkpoint_t));
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_sequential_checkpoint_create: allocation failed");
        return NULL;
    }

    ckpt->gpu_ctx = gpu_ctx;
    ckpt->num_layers = num_layers;

    /* Create underlying checkpoint context */
    ckpt->ckpt_ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, num_layers, memory_budget);
    if (!ckpt->ckpt_ctx) {
        nimcp_free(ckpt);
        return NULL;
    }

    /* Allocate per-layer arrays */
    ckpt->layer_contexts = nimcp_calloc((size_t)num_layers, sizeof(void*));
    ckpt->layer_output_sizes = nimcp_calloc((size_t)num_layers, sizeof(size_t));
    ckpt->activations = nimcp_calloc((size_t)num_layers,
                                     sizeof(nimcp_gpu_tensor_t*));
    ckpt->activation_stored = nimcp_calloc((size_t)num_layers, sizeof(bool));

    if (!ckpt->layer_contexts || !ckpt->layer_output_sizes ||
        !ckpt->activations || !ckpt->activation_stored) {
        nimcp_sequential_checkpoint_destroy(ckpt);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_sequential_checkpoint_create: array allocation failed");
        return NULL;
    }

    /* Initialize config defaults */
    nimcp_seq_checkpoint_config_init(&ckpt->config);
    ckpt->config.memory_budget = memory_budget;

    return ckpt;
}

nimcp_sequential_checkpoint_t* nimcp_sequential_checkpoint_create_with_config(
    nimcp_gpu_context_t* gpu_ctx,
    int num_layers,
    const nimcp_seq_checkpoint_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_create_with_config: config is NULL");
        return NULL;
    }

    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, num_layers, config->memory_budget);
    if (!ckpt) return NULL;

    /* Apply configuration */
    ckpt->config = *config;

    /* Reconfigure underlying context with strategy from config */
    nimcp_checkpoint_configure(ckpt->ckpt_ctx, config->strategy,
                               config->checkpoint_every_n);

    nimcp_checkpoint_set_profiling(ckpt->ckpt_ctx, config->enable_profiling);
    nimcp_checkpoint_set_verbose(ckpt->ckpt_ctx, config->verbose);

    return ckpt;
}

void nimcp_sequential_checkpoint_destroy(nimcp_sequential_checkpoint_t* ckpt)
{
    if (!ckpt) return;

    nimcp_checkpoint_ctx_destroy(ckpt->ckpt_ctx);
    nimcp_free(ckpt->layer_contexts);
    nimcp_free(ckpt->layer_output_sizes);
    nimcp_free(ckpt->activations);
    nimcp_free(ckpt->activation_stored);

    nimcp_free(ckpt);
}

bool nimcp_sequential_checkpoint_reset(nimcp_sequential_checkpoint_t* ckpt)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_reset: ckpt is NULL");
        return false;
    }

    /* Reset underlying context */
    nimcp_checkpoint_ctx_reset(ckpt->ckpt_ctx);

    /* Clear stored activations */
    for (int i = 0; i < ckpt->num_layers; i++) {
        ckpt->activations[i] = NULL;
        ckpt->activation_stored[i] = false;
    }

    ckpt->current_input = NULL;
    ckpt->current_output = NULL;
    ckpt->current_layer = 0;
    ckpt->total_activation_memory = 0;
    ckpt->saved_memory = 0;
    ckpt->recompute_count = 0;
    ckpt->recompute_time_ms = 0.0;

    return true;
}

/*=============================================================================
 * Activation Checkpoint - Sequential Checkpoint Configuration
 *=============================================================================*/

bool nimcp_sequential_checkpoint_set_layer_ctx(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    void* layer_ctx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_set_layer_ctx: ckpt is NULL");
        return false;
    }
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_set_layer_ctx: layer_idx out of range");
        return false;
    }

    ckpt->layer_contexts[layer_idx] = layer_ctx;
    return true;
}

bool nimcp_sequential_checkpoint_set_layer_size(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    size_t output_size)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_set_layer_size: ckpt is NULL");
        return false;
    }
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_set_layer_size: layer_idx out of range");
        return false;
    }

    ckpt->layer_output_sizes[layer_idx] = output_size;
    /* Also set in underlying context */
    nimcp_checkpoint_set_layer_size(ckpt->ckpt_ctx, layer_idx, output_size);
    return true;
}

bool nimcp_sequential_checkpoint_set_shape_fn(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_seq_layer_shape_fn_t shape_fn,
    void* ctx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_set_shape_fn: ckpt is NULL");
        return false;
    }

    ckpt->shape_fn = shape_fn;
    ckpt->shape_ctx = ctx;
    return true;
}

bool nimcp_sequential_checkpoint_configure(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_checkpoint_strategy_t strategy,
    int checkpoint_n)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_configure: ckpt is NULL");
        return false;
    }

    ckpt->config.strategy = strategy;
    ckpt->config.checkpoint_every_n = checkpoint_n;

    return nimcp_checkpoint_configure(ckpt->ckpt_ctx, strategy, checkpoint_n);
}

bool nimcp_sequential_checkpoint_auto_configure(
    nimcp_sequential_checkpoint_t* ckpt)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_auto_configure: ckpt is NULL");
        return false;
    }

    /* Compute total activation memory from layer sizes */
    size_t total = 0;
    for (int i = 0; i < ckpt->num_layers; i++) {
        total += ckpt->layer_output_sizes[i];
    }

    if (ckpt->config.memory_budget > 0 && total > ckpt->config.memory_budget) {
        /* Use sqrt strategy when over budget */
        ckpt->config.strategy = CKPT_STRATEGY_SQRT;
        nimcp_checkpoint_configure(ckpt->ckpt_ctx, CKPT_STRATEGY_SQRT, 0);
    } else {
        /* Within budget - no checkpointing needed */
        ckpt->config.strategy = CKPT_STRATEGY_NONE;
        nimcp_checkpoint_configure(ckpt->ckpt_ctx, CKPT_STRATEGY_NONE, 0);
    }

    return nimcp_checkpoint_auto_configure(
        ckpt->ckpt_ctx, ckpt->config.memory_budget,
        ckpt->layer_output_sizes, ckpt->num_layers);
}

/*=============================================================================
 * Activation Checkpoint - Forward Pass API
 *=============================================================================*/

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_forward(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* input,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_forward: ckpt is NULL");
        return NULL;
    }
    if (!input || !layer_forward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_forward: input or layer_forward is NULL");
        return NULL;
    }

    nimcp_checkpoint_begin_forward(ckpt->ckpt_ctx);

    /* Store initial input as activation[0] if we should save it */
    ckpt->current_input = input;
    if (nimcp_checkpoint_should_save(ckpt->ckpt_ctx, 0)) {
        ckpt->activations[0] = input;
        ckpt->activation_stored[0] = true;
    }

    /* Execute each layer sequentially */
    nimcp_gpu_tensor_t* current = input;
    for (int i = 0; i < ckpt->num_layers; i++) {
        ckpt->current_layer = i;

        /* Forward through layer i.
           On CPU stub, we pass the current tensor through the function.
           The output is written in-place or to a pre-allocated buffer. */
        nimcp_gpu_tensor_t* output = NULL;

        /* If we have an activation stored for this layer output, use it */
        layer_forward(i, forward_ctx, current, current);
        output = current;

        /* Mark layer in checkpoint context */
        nimcp_checkpoint_mark_layer(ckpt->ckpt_ctx, i, output);

        /* Store activation if checkpoint layer */
        if (nimcp_checkpoint_should_save(ckpt->ckpt_ctx, i)) {
            ckpt->activations[i] = output;
            ckpt->activation_stored[i] = true;
            ckpt->total_activation_memory += ckpt->layer_output_sizes[i];
        }

        current = output;
    }

    ckpt->current_output = current;
    nimcp_checkpoint_end_forward(ckpt->ckpt_ctx);

    return current;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_forward_layer(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* input,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_forward_layer: ckpt is NULL");
        return NULL;
    }
    if (!input || !layer_forward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_forward_layer: input or layer_forward is NULL");
        return NULL;
    }
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_forward_layer: layer_idx out of range");
        return NULL;
    }

    ckpt->current_layer = layer_idx;

    /* Execute single layer forward */
    layer_forward(layer_idx, forward_ctx, input, input);

    /* Mark and potentially store */
    nimcp_checkpoint_mark_layer(ckpt->ckpt_ctx, layer_idx, input);

    if (nimcp_checkpoint_should_save(ckpt->ckpt_ctx, layer_idx)) {
        ckpt->activations[layer_idx] = input;
        ckpt->activation_stored[layer_idx] = true;
    }

    return input;
}

/*=============================================================================
 * Activation Checkpoint - Backward Pass API
 *=============================================================================*/

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* backward_ctx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_backward: ckpt is NULL");
        return NULL;
    }
    if (!grad_output || !layer_backward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_backward: grad_output or layer_backward is NULL");
        return NULL;
    }

    nimcp_checkpoint_begin_backward(ckpt->ckpt_ctx);

    /* Execute backward through all layers in reverse */
    nimcp_gpu_tensor_t* current_grad = grad_output;
    for (int i = ckpt->num_layers - 1; i >= 0; i--) {
        ckpt->current_layer = i;

        /* Execute backward for layer i */
        layer_backward(i, backward_ctx, current_grad, current_grad);

        /* Free activation if no longer needed */
        if (ckpt->activation_stored[i]) {
            ckpt->activation_stored[i] = false;
            ckpt->activations[i] = NULL;
        }
    }

    nimcp_checkpoint_end_backward(ckpt->ckpt_ctx);

    return current_grad;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward_with_recompute(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_forward_fn_t layer_forward,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* forward_ctx,
    void* backward_ctx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_backward_with_recompute: ckpt is NULL");
        return NULL;
    }
    if (!grad_output || !layer_forward || !layer_backward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_backward_with_recompute: NULL parameter");
        return NULL;
    }

    nimcp_checkpoint_begin_backward(ckpt->ckpt_ctx);

    nimcp_gpu_tensor_t* current_grad = grad_output;
    for (int i = ckpt->num_layers - 1; i >= 0; i--) {
        ckpt->current_layer = i;

        /* If activation not stored, recompute from nearest checkpoint */
        if (!ckpt->activation_stored[i]) {
            /* Find nearest stored activation before this layer */
            int nearest = -1;
            for (int j = i - 1; j >= 0; j--) {
                if (ckpt->activation_stored[j]) {
                    nearest = j;
                    break;
                }
            }

            /* Recompute from nearest checkpoint through layer i */
            if (nearest >= 0 && ckpt->activations[nearest]) {
                nimcp_gpu_tensor_t* recomp = ckpt->activations[nearest];
                for (int j = nearest; j <= i; j++) {
                    layer_forward(j, forward_ctx, recomp, recomp);
                }
                ckpt->activations[i] = recomp;
                ckpt->activation_stored[i] = true;
                ckpt->recompute_count++;
            }
        }

        /* Execute backward for layer i */
        layer_backward(i, backward_ctx, current_grad, current_grad);

        /* Free activation */
        if (ckpt->activation_stored[i]) {
            ckpt->activation_stored[i] = false;
            ckpt->activations[i] = NULL;
        }
    }

    nimcp_checkpoint_end_backward(ckpt->ckpt_ctx);

    return current_grad;
}

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward_layer(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* backward_ctx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_backward_layer: ckpt is NULL");
        return NULL;
    }
    if (!grad_output || !layer_backward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_backward_layer: NULL parameter");
        return NULL;
    }
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_backward_layer: layer_idx out of range");
        return NULL;
    }

    ckpt->current_layer = layer_idx;

    /* Execute backward for single layer */
    layer_backward(layer_idx, backward_ctx, grad_output, grad_output);

    /* Free activation for this layer */
    if (ckpt->activation_stored[layer_idx]) {
        ckpt->activation_stored[layer_idx] = false;
        ckpt->activations[layer_idx] = NULL;
    }

    return grad_output;
}

/*=============================================================================
 * Activation Checkpoint - Activation Management API
 *=============================================================================*/

nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_get_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_get_activation: ckpt is NULL");
        return NULL;
    }
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_get_activation: layer_idx out of range");
        return NULL;
    }

    /* If stored, return directly */
    if (ckpt->activation_stored[layer_idx] && ckpt->activations[layer_idx]) {
        return ckpt->activations[layer_idx];
    }

    /* Need to recompute - find nearest stored activation */
    if (!layer_forward) {
        /* Can't recompute without forward function */
        return NULL;
    }

    int nearest = -1;
    for (int j = layer_idx - 1; j >= 0; j--) {
        if (ckpt->activation_stored[j] && ckpt->activations[j]) {
            nearest = j;
            break;
        }
    }

    if (nearest < 0) {
        /* No checkpoint available - use initial input */
        if (ckpt->current_input) {
            nimcp_gpu_tensor_t* recomp = ckpt->current_input;
            for (int j = 0; j <= layer_idx; j++) {
                layer_forward(j, forward_ctx, recomp, recomp);
            }
            ckpt->activations[layer_idx] = recomp;
            ckpt->activation_stored[layer_idx] = true;
            ckpt->recompute_count++;
            return recomp;
        }
        return NULL;
    }

    /* Recompute from nearest checkpoint */
    nimcp_gpu_tensor_t* recomp = ckpt->activations[nearest];
    for (int j = nearest; j <= layer_idx; j++) {
        layer_forward(j, forward_ctx, recomp, recomp);
    }
    ckpt->activations[layer_idx] = recomp;
    ckpt->activation_stored[layer_idx] = true;
    ckpt->recompute_count++;

    return recomp;
}

bool nimcp_sequential_checkpoint_has_activation(
    const nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx)
{
    if (!ckpt) return false;
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) return false;

    return ckpt->activation_stored[layer_idx] &&
           ckpt->activations[layer_idx] != NULL;
}

bool nimcp_sequential_checkpoint_store_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* activation)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_store_activation: ckpt is NULL");
        return false;
    }
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_store_activation: layer_idx out of range");
        return false;
    }
    if (!activation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_store_activation: activation is NULL");
        return false;
    }

    /* On CPU fallback, store the pointer directly (no clone needed) */
    ckpt->activations[layer_idx] = activation;
    ckpt->activation_stored[layer_idx] = true;
    ckpt->total_activation_memory += ckpt->layer_output_sizes[layer_idx];

    return true;
}

bool nimcp_sequential_checkpoint_free_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_free_activation: ckpt is NULL");
        return false;
    }
    if (layer_idx < 0 || layer_idx >= ckpt->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_free_activation: layer_idx out of range");
        return false;
    }

    if (ckpt->activation_stored[layer_idx]) {
        size_t freed_size = ckpt->layer_output_sizes[layer_idx];
        if (freed_size <= ckpt->total_activation_memory) {
            ckpt->total_activation_memory -= freed_size;
        }
        ckpt->saved_memory += freed_size;
    }

    ckpt->activations[layer_idx] = NULL;
    ckpt->activation_stored[layer_idx] = false;

    /* Also free in underlying context */
    nimcp_checkpoint_free_activation(ckpt->ckpt_ctx, layer_idx);

    return true;
}

/*=============================================================================
 * Activation Checkpoint - Statistics API
 *=============================================================================*/

void nimcp_sequential_checkpoint_get_stats(
    const nimcp_sequential_checkpoint_t* ckpt,
    size_t* total_memory,
    size_t* saved_memory,
    int* recompute_count,
    double* recompute_time_ms)
{
    if (!ckpt) {
        if (total_memory)     *total_memory = 0;
        if (saved_memory)     *saved_memory = 0;
        if (recompute_count)  *recompute_count = 0;
        if (recompute_time_ms) *recompute_time_ms = 0.0;
        return;
    }

    if (total_memory)     *total_memory = ckpt->total_activation_memory;
    if (saved_memory)     *saved_memory = ckpt->saved_memory;
    if (recompute_count)  *recompute_count = ckpt->recompute_count;
    if (recompute_time_ms) *recompute_time_ms = ckpt->recompute_time_ms;
}

void nimcp_sequential_checkpoint_print_stats(
    const nimcp_sequential_checkpoint_t* ckpt)
{
    if (!ckpt) return;

    printf("=== Sequential Checkpoint Statistics ===\n");
    printf("Num layers:          %d\n", ckpt->num_layers);
    printf("Total act. memory:   %zu bytes\n", ckpt->total_activation_memory);
    printf("Saved memory:        %zu bytes\n", ckpt->saved_memory);
    printf("Recompute count:     %d\n", ckpt->recompute_count);
    printf("Recompute time:      %.3f ms\n", ckpt->recompute_time_ms);

    /* Count stored activations */
    int stored = 0;
    for (int i = 0; i < ckpt->num_layers; i++) {
        if (ckpt->activation_stored[i]) stored++;
    }
    printf("Stored activations:  %d / %d\n", stored, ckpt->num_layers);
    printf("Strategy:            %d\n", (int)ckpt->config.strategy);
    printf("========================================\n");
}

int nimcp_sequential_checkpoint_get_info_string(
    const nimcp_sequential_checkpoint_t* ckpt,
    char* buffer,
    size_t size)
{
    if (!ckpt || !buffer || size == 0) return 0;

    int stored = 0;
    for (int i = 0; i < ckpt->num_layers; i++) {
        if (ckpt->activation_stored[i]) stored++;
    }

    return snprintf(buffer, size,
        "SeqCheckpoint[layers=%d stored=%d mem=%zu saved=%zu recomputes=%d]",
        ckpt->num_layers, stored,
        ckpt->total_activation_memory, ckpt->saved_memory,
        ckpt->recompute_count);
}

/*=============================================================================
 * Activation Checkpoint - Transformer Block Checkpointing
 *=============================================================================*/

nimcp_transformer_checkpoint_t* nimcp_transformer_checkpoint_create(
    nimcp_gpu_context_t* gpu_ctx,
    int num_blocks,
    size_t memory_budget)
{
    (void)gpu_ctx;

    if (num_blocks <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_transformer_checkpoint_create: num_blocks must be > 0");
        return NULL;
    }

    nimcp_transformer_checkpoint_t* ckpt = nimcp_calloc(
        1, sizeof(nimcp_transformer_checkpoint_t));
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_transformer_checkpoint_create: allocation failed");
        return NULL;
    }

    ckpt->num_blocks = num_blocks;

    /* Each transformer block has 2 sub-layers: attention + FFN
       So total sequential layers = num_blocks * 2 */
    ckpt->seq_ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, num_blocks * 2, memory_budget);
    if (!ckpt->seq_ckpt) {
        nimcp_free(ckpt);
        return NULL;
    }

    /* Allocate block configurations */
    ckpt->blocks = nimcp_calloc((size_t)num_blocks,
                                sizeof(nimcp_transformer_block_t));
    if (!ckpt->blocks) {
        nimcp_sequential_checkpoint_destroy(ckpt->seq_ckpt);
        nimcp_free(ckpt);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_transformer_checkpoint_create: blocks allocation failed");
        return NULL;
    }

    /* Allocate per-block attention and FFN intermediate storage */
    ckpt->attention_scores = nimcp_calloc((size_t)num_blocks,
                                          sizeof(nimcp_gpu_tensor_t*));
    ckpt->ffn_intermediate = nimcp_calloc((size_t)num_blocks,
                                          sizeof(nimcp_gpu_tensor_t*));

    if (!ckpt->attention_scores || !ckpt->ffn_intermediate) {
        nimcp_transformer_checkpoint_destroy(ckpt);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_transformer_checkpoint_create: attention/ffn allocation failed");
        return NULL;
    }

    return ckpt;
}

bool nimcp_transformer_checkpoint_configure_block(
    nimcp_transformer_checkpoint_t* ckpt,
    int block_idx,
    const nimcp_transformer_block_t* block)
{
    if (!ckpt) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_transformer_checkpoint_configure_block: ckpt is NULL");
        return false;
    }
    if (!block) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_transformer_checkpoint_configure_block: block is NULL");
        return false;
    }
    if (block_idx < 0 || block_idx >= ckpt->num_blocks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_transformer_checkpoint_configure_block: block_idx out of range");
        return false;
    }

    ckpt->blocks[block_idx] = *block;

    /* Estimate layer sizes for this block's attention and FFN sub-layers */
    /* Attention layer: batch * seq_len * num_heads * head_dim (approx embed_dim) */
    size_t attn_size = (size_t)block->embed_dim * (size_t)block->num_heads *
                       sizeof(float);
    /* FFN layer: batch * ffn_dim */
    size_t ffn_size = (size_t)block->ffn_dim * sizeof(float);

    int attn_layer = block_idx * 2;
    int ffn_layer = block_idx * 2 + 1;

    nimcp_sequential_checkpoint_set_layer_size(ckpt->seq_ckpt,
                                               attn_layer, attn_size);
    nimcp_sequential_checkpoint_set_layer_size(ckpt->seq_ckpt,
                                               ffn_layer, ffn_size);

    return true;
}

void nimcp_transformer_checkpoint_destroy(nimcp_transformer_checkpoint_t* ckpt)
{
    if (!ckpt) return;

    nimcp_sequential_checkpoint_destroy(ckpt->seq_ckpt);
    nimcp_free(ckpt->blocks);
    nimcp_free(ckpt->attention_scores);
    nimcp_free(ckpt->ffn_intermediate);
    nimcp_free(ckpt);
}

/*=============================================================================
 * Activation Checkpoint - Utility Functions
 *=============================================================================*/

bool nimcp_sequential_checkpoint_estimate_memory(
    int num_layers,
    const size_t* layer_sizes,
    size_t* no_ckpt_memory,
    size_t* sqrt_ckpt_memory,
    int* optimal_n)
{
    if (!layer_sizes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_estimate_memory: layer_sizes is NULL");
        return false;
    }
    if (num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_estimate_memory: num_layers must be > 0");
        return false;
    }

    /* Total memory without checkpointing */
    size_t total = 0;
    for (int i = 0; i < num_layers; i++) {
        total += layer_sizes[i];
    }
    if (no_ckpt_memory) *no_ckpt_memory = total;

    /* Sqrt checkpoint memory */
    int sqrt_interval = nimcp_checkpoint_sqrt_interval(num_layers);
    size_t sqrt_mem = 0;
    for (int i = 0; i < num_layers; i++) {
        if (i == 0 || i == num_layers - 1 ||
            (sqrt_interval > 0 && i % sqrt_interval == 0)) {
            sqrt_mem += layer_sizes[i];
        }
    }
    if (sqrt_ckpt_memory) *sqrt_ckpt_memory = sqrt_mem;

    /* Find optimal N using estimation API */
    if (optimal_n) {
        nimcp_checkpoint_estimate_t est;
        if (nimcp_checkpoint_estimate(layer_sizes, num_layers, &est)) {
            *optimal_n = est.optimal_n;
        } else {
            *optimal_n = sqrt_interval;
        }
    }

    return true;
}

bool nimcp_sequential_checkpoint_recommend(
    int num_layers,
    const size_t* layer_sizes,
    size_t memory_budget,
    nimcp_checkpoint_strategy_t* strategy,
    int* checkpoint_n)
{
    if (!layer_sizes || !strategy || !checkpoint_n) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_sequential_checkpoint_recommend: NULL parameter");
        return false;
    }
    if (num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_sequential_checkpoint_recommend: num_layers must be > 0");
        return false;
    }

    /* Delegate to the low-level recommendation function */
    return nimcp_checkpoint_recommend_strategy(
        layer_sizes, num_layers, memory_budget, strategy, checkpoint_n);
}
