/**
 * @file nimcp_bridge_gpu.c
 * @brief GPU-Aware Bridge Base Utilities Implementation
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Implements GPU context management for bridge modules
 * WHY:  Provides consistent GPU acceleration support across all bridges
 * HOW:  Wraps GPU context from brain, manages work buffers, handles sync
 *
 * PHASE 2.1: GPU Integration Plan - Bridge GPU Utilities
 */

#include "utils/bridge/nimcp_bridge_gpu.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bridge_gpu)

//=============================================================================
// Lifecycle API
//=============================================================================

bridge_gpu_context_t* bridge_gpu_context_create(brain_t brain) {
    // Allocate context structure
    bridge_gpu_context_t* ctx = nimcp_malloc(sizeof(bridge_gpu_context_t));
    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge GPU context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    // Initialize to safe defaults (CPU mode)
    ctx->gpu_ctx = NULL;
    ctx->work_buffer = NULL;
    ctx->stream = NULL;
    ctx->gpu_available = false;
    ctx->work_buffer_size = 0;

    // If no brain provided, return CPU-only context
    if (!brain) {
        NIMCP_LOGGING_DEBUG("Bridge GPU context created in CPU-only mode (no brain)");
        return ctx;
    }

    // Check if brain has GPU enabled
    // Note: brain_t is a pointer to brain_struct (opaque)
    if (!brain->gpu_enabled || !brain->gpu_ctx) {
        NIMCP_LOGGING_DEBUG("Bridge GPU context created in CPU-only mode (brain GPU disabled)");
        return ctx;
    }

    // Extract GPU context from brain
    ctx->gpu_ctx = brain->gpu_ctx;
    ctx->gpu_available = true;

    // Use the brain's compute stream for bridge operations
    // This ensures proper synchronization with brain's GPU operations
    ctx->stream = (void*)nimcp_gpu_get_compute_stream(ctx->gpu_ctx);

    NIMCP_LOGGING_DEBUG("Bridge GPU context created with GPU acceleration enabled");

    return ctx;
}

void bridge_gpu_context_destroy(bridge_gpu_context_t* ctx) {
    if (!ctx) {
        return;
    }

    // Free work buffer if allocated
    if (ctx->work_buffer && ctx->gpu_ctx) {
        // Ensure all operations complete before freeing
        bridge_gpu_sync(ctx);
        nimcp_gpu_free(ctx->gpu_ctx, ctx->work_buffer);
        ctx->work_buffer = NULL;
        ctx->work_buffer_size = 0;
    }

    // Note: We do NOT destroy gpu_ctx - it belongs to the brain
    // We only borrowed a reference to it
    ctx->gpu_ctx = NULL;
    ctx->stream = NULL;
    ctx->gpu_available = false;

    nimcp_free(ctx);

    NIMCP_LOGGING_DEBUG("Bridge GPU context destroyed");
}

//=============================================================================
// Decision API
//=============================================================================

bool bridge_should_use_gpu(bridge_gpu_context_t* ctx, size_t data_size) {
    // NULL context -> use CPU
    if (!ctx) {
        return false;
    }

    // GPU not available -> use CPU
    if (!ctx->gpu_available) {
        return false;
    }

    // Data too small -> kernel launch overhead makes CPU faster
    if (data_size < BRIDGE_GPU_MIN_ELEMENTS_THRESHOLD) {
        return false;
    }

    // GPU context must be valid
    if (!nimcp_gpu_context_is_valid(ctx->gpu_ctx)) {
        return false;
    }

    return true;
}

//=============================================================================
// Buffer Management API
//=============================================================================

bool bridge_gpu_ensure_buffer(bridge_gpu_context_t* ctx, size_t min_size) {
    // NULL context -> failure
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge_gpu_ensure_buffer: ctx is NULL");
        return false;
    }

    // GPU not available -> can't allocate GPU buffer
    if (!ctx->gpu_available || !ctx->gpu_ctx) {
        NIMCP_LOGGING_WARN("Cannot allocate GPU buffer: GPU not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge_gpu_ensure_buffer: required parameter is NULL (ctx->gpu_available, ctx->gpu_ctx)");
        return false;
    }

    // Current buffer sufficient
    if (ctx->work_buffer && ctx->work_buffer_size >= min_size) {
        return true;
    }

    // Need to (re)allocate
    // First, free existing buffer if any
    if (ctx->work_buffer) {
        // Sync before freeing to ensure no pending operations use it
        bridge_gpu_sync(ctx);
        nimcp_gpu_free(ctx->gpu_ctx, ctx->work_buffer);
        ctx->work_buffer = NULL;
        ctx->work_buffer_size = 0;
    }

    // Allocate new buffer with some headroom to reduce future reallocations
    // Use 1.5x requested size, but at least BRIDGE_GPU_DEFAULT_BUFFER_SIZE
    size_t alloc_size = min_size;
    if (alloc_size < BRIDGE_GPU_DEFAULT_BUFFER_SIZE) {
        alloc_size = BRIDGE_GPU_DEFAULT_BUFFER_SIZE;
    } else {
        // Add 50% headroom for growth
        alloc_size = (alloc_size * 3) / 2;
    }

    ctx->work_buffer = nimcp_gpu_malloc(ctx->gpu_ctx, alloc_size);
    if (!ctx->work_buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate GPU work buffer (%zu bytes)", alloc_size);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge_gpu_ensure_buffer: ctx->work_buffer is NULL");
        return false;
    }

    ctx->work_buffer_size = alloc_size;
    NIMCP_LOGGING_DEBUG("Allocated GPU work buffer: %zu bytes (requested: %zu)",
                        alloc_size, min_size);

    return true;
}

//=============================================================================
// Synchronization API
//=============================================================================

void bridge_gpu_sync(bridge_gpu_context_t* ctx) {
    // NULL context or GPU not available -> no-op
    if (!ctx || !ctx->gpu_available || !ctx->gpu_ctx) {
        return;
    }

    // Synchronize the compute stream
    int result = nimcp_gpu_stream_synchronize(ctx->gpu_ctx);
    if (result != 0) {
        NIMCP_LOGGING_WARN("GPU stream synchronization failed: %s",
                          nimcp_gpu_context_get_error(ctx->gpu_ctx));
    }
}
