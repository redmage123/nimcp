#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_buffer_pool.c - Buffer Pool Implementation with CoW Support
//=============================================================================
/**
 * @file nimcp_buffer_pool.c
 * @brief Buffer pool combining Memory Pool + CoW Manager for temporal buffers
 *
 * WHAT: High-level buffer pool for sparse channel allocation patterns
 * WHY:  Achieve 10x memory savings when many channels declared, few active
 * HOW:  CoW managers for shared templates + memory pools for private copies
 *
 * ARCHITECTURE:
 *
 *   Buffer Pool Structure:
 *   ┌──────────────────────────────────────────────────────────┐
 *   │ Buffer Pool                                              │
 *   │  ┌────────────────────────────────────────────────────┐ │
 *   │  │ CoW Manager (Integration Buffers)                  │ │
 *   │  │  - Shared template (fast/medium/slow)              │ │
 *   │  │  - Refcount tracking                               │ │
 *   │  └────────────────────────────────────────────────────┘ │
 *   │  ┌────────────────────────────────────────────────────┐ │
 *   │  │ Memory Pool (for private copies)                   │ │
 *   │  │  - O(1) acquire/release                            │ │
 *   │  │  - Thread-safe                                     │ │
 *   │  └────────────────────────────────────────────────────┘ │
 *   │  Channel Map: [0]=Shared, [1]=Shared, [2]=Private...  │
 *   └──────────────────────────────────────────────────────────┘
 *
 * SRP: This module has ONE responsibility - buffer pool management
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#include "utils/memory/nimcp_buffer_pool.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(buffer_pool)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Per-channel tracking information
 */
typedef struct {
    cow_handle_t integration_handle;  /**< Integration buffer CoW handle */
    cow_handle_t window_handle;       /**< Sliding window CoW handle */
    cow_handle_t accumulator_handle;  /**< Temporal accumulator CoW handle */
    bool active;                      /**< Is channel active? */
} channel_info_t;

/**
 * @brief Buffer pool structure
 */
struct buffer_pool_struct {
    // Configuration
    buffer_pool_config_t config;

    // CoW managers (one per buffer type)
    cow_manager_t integration_cow;
    cow_manager_t window_cow;
    cow_manager_t accumulator_cow;

    // Memory pools (for private copies)
    memory_pool_t integration_pool;
    memory_pool_t window_pool;
    memory_pool_t accumulator_pool;

    // Channel tracking
    channel_info_t* channels;  /**< Per-channel info array */
    size_t num_channels;       /**< Current number of channels */

    // Statistics
    size_t total_acquires;
    size_t cow_triggers;
    uint64_t total_alloc_time_ns;

    // Thread safety
    nimcp_platform_mutex_t mutex;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in nanoseconds
 */
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * NIMCP_NS_PER_SEC + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Calculate buffer size for integration buffers
 */
static inline size_t get_integration_buffer_size(const buffer_pool_config_t* config) {
    // Integration buffer contains fast, medium, slow timescale buffers
    return (config->fast_size + config->medium_size + config->slow_size) * sizeof(float);
}

/**
 * @brief Calculate buffer size for sliding window
 */
static inline size_t get_window_buffer_size(const buffer_pool_config_t* config) {
    return config->window_size * sizeof(float);
}

/**
 * @brief Calculate buffer size for temporal accumulator
 */
static inline size_t get_accumulator_buffer_size(const buffer_pool_config_t* config) {
    // Accumulator stores aggregated values across timescales
    return (config->fast_size + config->medium_size + config->slow_size) * sizeof(float);
}

//=============================================================================
// Buffer Pool API Implementation
//=============================================================================

NIMCP_EXPORT buffer_pool_t buffer_pool_create(const buffer_pool_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }
    if (config->max_channels == 0 || config->expected_active == 0) return NULL;

    // Allocate pool structure
    buffer_pool_t pool = nimcp_calloc(1, sizeof(struct buffer_pool_struct));
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pool is NULL");

        return NULL;

    }

    // Copy configuration
    memcpy(&pool->config, config, sizeof(buffer_pool_config_t));
    pool->num_channels = 0;

    // Calculate buffer sizes
    size_t integration_size = get_integration_buffer_size(config);
    size_t window_size = get_window_buffer_size(config);
    size_t accumulator_size = get_accumulator_buffer_size(config);

    // Create memory pools for private copies
    size_t pool_capacity = (size_t)(config->expected_active * config->overallocation_factor);

    memory_pool_config_t pool_config = memory_pool_default_config(integration_size, pool_capacity);
    pool->integration_pool = memory_pool_create(&pool_config);
    if (!pool->integration_pool) {
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "buffer_pool_create: pool->integration_pool is NULL");
        return NULL;
    }

    pool_config = memory_pool_default_config(window_size, pool_capacity);
    pool->window_pool = memory_pool_create(&pool_config);
    if (!pool->window_pool) {
        memory_pool_destroy(pool->integration_pool);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "buffer_pool_create: pool->window_pool is NULL");
        return NULL;
    }

    pool_config = memory_pool_default_config(accumulator_size, pool_capacity);
    pool->accumulator_pool = memory_pool_create(&pool_config);
    if (!pool->accumulator_pool) {
        memory_pool_destroy(pool->integration_pool);
        memory_pool_destroy(pool->window_pool);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "buffer_pool_create: pool->accumulator_pool is NULL");
        return NULL;
    }

    // Create CoW managers if CoW enabled
    if (config->enable_cow) {
        // Allocate zero-initialized templates
        float* integration_template = nimcp_calloc(integration_size / sizeof(float), sizeof(float));
        float* window_template = nimcp_calloc(window_size / sizeof(float), sizeof(float));
        float* accumulator_template = nimcp_calloc(accumulator_size / sizeof(float), sizeof(float));

        if (!integration_template || !window_template || !accumulator_template) {
            nimcp_free(integration_template);
            nimcp_free(window_template);
            nimcp_free(accumulator_template);
            memory_pool_destroy(pool->integration_pool);
            memory_pool_destroy(pool->window_pool);
            memory_pool_destroy(pool->accumulator_pool);
            nimcp_free(pool);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_create: required parameter is NULL (integration_template, window_template, accumulator_template)");
            return NULL;
        }

        cow_manager_config_t cow_config = cow_default_config(integration_size, pool->integration_pool);
        pool->integration_cow = cow_manager_create(&cow_config, integration_template);
        nimcp_free(integration_template);

        cow_config = cow_default_config(window_size, pool->window_pool);
        pool->window_cow = cow_manager_create(&cow_config, window_template);
        nimcp_free(window_template);

        cow_config = cow_default_config(accumulator_size, pool->accumulator_pool);
        pool->accumulator_cow = cow_manager_create(&cow_config, accumulator_template);
        nimcp_free(accumulator_template);

        if (!pool->integration_cow || !pool->window_cow || !pool->accumulator_cow) {
            cow_manager_destroy(pool->integration_cow);
            cow_manager_destroy(pool->window_cow);
            cow_manager_destroy(pool->accumulator_cow);
            memory_pool_destroy(pool->integration_pool);
            memory_pool_destroy(pool->window_pool);
            memory_pool_destroy(pool->accumulator_pool);
            nimcp_free(pool);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_create: required parameter is NULL (pool->integration_cow, pool->window_cow, pool->accumulator_cow)");
            return NULL;
        }
    }

    // Allocate channel tracking array
    pool->channels = nimcp_calloc(config->max_channels, sizeof(channel_info_t));
    if (!pool->channels) {
        cow_manager_destroy(pool->integration_cow);
        cow_manager_destroy(pool->window_cow);
        cow_manager_destroy(pool->accumulator_cow);
        memory_pool_destroy(pool->integration_pool);
        memory_pool_destroy(pool->window_pool);
        memory_pool_destroy(pool->accumulator_pool);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_create: pool->channels is NULL");
        return NULL;
    }

    // Initialize statistics
    pool->total_acquires = 0;
    pool->cow_triggers = 0;
    pool->total_alloc_time_ns = 0;

    // Initialize mutex
    if (nimcp_platform_mutex_init(&pool->mutex, false) != 0) {
        nimcp_free(pool->channels);
        cow_manager_destroy(pool->integration_cow);
        cow_manager_destroy(pool->window_cow);
        cow_manager_destroy(pool->accumulator_cow);
        memory_pool_destroy(pool->integration_pool);
        memory_pool_destroy(pool->window_pool);
        memory_pool_destroy(pool->accumulator_pool);
        nimcp_free(pool);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "buffer_pool_create: validation failed");
        return NULL;
    }

    return pool;
}

NIMCP_EXPORT void buffer_pool_destroy(buffer_pool_t pool) {
    if (!pool) return;

    // Destroy mutex
    nimcp_platform_mutex_destroy(&pool->mutex);

    // Release all channel handles
    for (size_t i = 0; i < pool->num_channels; i++) {
        if (pool->channels[i].active) {
            cow_release(pool->channels[i].integration_handle);
            cow_release(pool->channels[i].window_handle);
            cow_release(pool->channels[i].accumulator_handle);
        }
    }

    // Free channel tracking
    nimcp_free(pool->channels);

    // Destroy CoW managers
    cow_manager_destroy(pool->integration_cow);
    cow_manager_destroy(pool->window_cow);
    cow_manager_destroy(pool->accumulator_cow);

    // Destroy memory pools
    memory_pool_destroy(pool->integration_pool);
    memory_pool_destroy(pool->window_pool);
    memory_pool_destroy(pool->accumulator_pool);

    // Free pool
    nimcp_free(pool);
}

/*
 * PLACEHOLDER FUNCTION IMPLEMENTATIONS - NOT YET ACTIVE
 *
 * These functions are placeholders for future Phase 1.2/1.3 integration.
 * They were previously commented out pending middleware type integration.
 * Now enabled for full buffer pool functionality with sliding windows,
 * temporal accumulators, and integration buffers.
 */
#if 1  /* Enabled: Middleware types now integrated */

NIMCP_EXPORT integration_buffer_t buffer_pool_acquire_integration_buffer(
    buffer_pool_t pool,
    size_t channel_id,
    bool needs_private
) {
    if (!pool || channel_id >= pool->config.max_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "buffer_pool_acquire_integration_buffer: pool is NULL");
        return NULL;
    }

    nimcp_platform_mutex_lock(&pool->mutex);

    uint64_t start = get_time_ns();

    // Ensure channel exists
    if (channel_id >= pool->num_channels) {
        pool->num_channels = channel_id + 1;
    }

    channel_info_t* channel = &pool->channels[channel_id];

    // If channel not active, acquire CoW handle
    if (!channel->active) {
        if (pool->config.enable_cow && pool->integration_cow) {
            channel->integration_handle = cow_acquire(pool->integration_cow);
            if (!channel->integration_handle) {
                nimcp_platform_mutex_unlock(&pool->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_acquire_integration_buffer: channel->integration_handle is NULL");
                return NULL;
            }
        } else {
            // Direct allocation from pool (no CoW)
            void* buffer = memory_pool_acquire(pool->integration_pool);
            if (!buffer) {
                nimcp_platform_mutex_unlock(&pool->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "buffer_pool_acquire_integration_buffer: buffer is NULL");
                return NULL;
            }
            // Store as opaque handle (cast to cow_handle_t for consistency)
            channel->integration_handle = (cow_handle_t)buffer;
        }
        channel->active = true;
    }

    // If needs private and using CoW, trigger CoW copy
    if (needs_private && pool->config.enable_cow && pool->integration_cow) {
        if (cow_is_shared(channel->integration_handle)) {
            void* private_data = cow_write(channel->integration_handle);
            if (!private_data) {
                nimcp_platform_mutex_unlock(&pool->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_acquire_integration_buffer: private_data is NULL");
                return NULL;
            }
            pool->cow_triggers++;
        }
    }

    void* result = NULL;
    if (pool->config.enable_cow && pool->integration_cow) {
        result = (void*)cow_read(channel->integration_handle);
    } else {
        result = (void*)channel->integration_handle;
    }

    uint64_t end = get_time_ns();
    pool->total_acquires++;
    pool->total_alloc_time_ns += (end - start);

    nimcp_platform_mutex_unlock(&pool->mutex);

    return result;
}

NIMCP_EXPORT sliding_window_t buffer_pool_acquire_sliding_window(
    buffer_pool_t pool,
    size_t channel_id,
    bool needs_private
) {
    if (!pool || channel_id >= pool->config.max_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "buffer_pool_acquire_sliding_window: pool is NULL");
        return NULL;
    }

    nimcp_platform_mutex_lock(&pool->mutex);

    // Ensure channel exists
    if (channel_id >= pool->num_channels) {
        pool->num_channels = channel_id + 1;
    }

    channel_info_t* channel = &pool->channels[channel_id];

    // If channel not active, acquire CoW handle
    if (!channel->active || !channel->window_handle) {
        if (pool->config.enable_cow && pool->window_cow) {
            channel->window_handle = cow_acquire(pool->window_cow);
            if (!channel->window_handle) {
                nimcp_platform_mutex_unlock(&pool->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_acquire_sliding_window: channel->window_handle is NULL");
                return NULL;
            }
        } else {
            void* buffer = memory_pool_acquire(pool->window_pool);
            if (!buffer) {
                nimcp_platform_mutex_unlock(&pool->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_acquire_sliding_window: buffer is NULL");
                return NULL;
            }
            channel->window_handle = (cow_handle_t)buffer;
        }
        channel->active = true;  // Mark channel as active
    }

    // Trigger CoW if needed
    if (needs_private && pool->config.enable_cow && pool->window_cow) {
        if (cow_is_shared(channel->window_handle)) {
            cow_write(channel->window_handle);
            pool->cow_triggers++;
        }
    }

    void* result = NULL;
    if (pool->config.enable_cow && pool->window_cow) {
        result = (void*)cow_read(channel->window_handle);
    } else {
        result = (void*)channel->window_handle;
    }

    pool->total_acquires++;
    nimcp_platform_mutex_unlock(&pool->mutex);

    return result;
}

NIMCP_EXPORT temporal_accumulator_t buffer_pool_acquire_temporal_accumulator(
    buffer_pool_t pool,
    size_t channel_id,
    bool needs_private
) {
    if (!pool || channel_id >= pool->config.max_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "buffer_pool_acquire_temporal_accumulator: pool is NULL");
        return NULL;
    }

    nimcp_platform_mutex_lock(&pool->mutex);

    // Ensure channel exists
    if (channel_id >= pool->num_channels) {
        pool->num_channels = channel_id + 1;
    }

    channel_info_t* channel = &pool->channels[channel_id];

    // If channel not active, acquire CoW handle
    if (!channel->active || !channel->accumulator_handle) {
        if (pool->config.enable_cow && pool->accumulator_cow) {
            channel->accumulator_handle = cow_acquire(pool->accumulator_cow);
            if (!channel->accumulator_handle) {
                nimcp_platform_mutex_unlock(&pool->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_acquire_temporal_accumulator: channel->accumulator_handle is NULL");
                return NULL;
            }
        } else {
            void* buffer = memory_pool_acquire(pool->accumulator_pool);
            if (!buffer) {
                nimcp_platform_mutex_unlock(&pool->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_acquire_temporal_accumulator: buffer is NULL");
                return NULL;
            }
            channel->accumulator_handle = (cow_handle_t)buffer;
        }
        channel->active = true;  // Mark channel as active
    }

    // Trigger CoW if needed
    if (needs_private && pool->config.enable_cow && pool->accumulator_cow) {
        if (cow_is_shared(channel->accumulator_handle)) {
            cow_write(channel->accumulator_handle);
            pool->cow_triggers++;
        }
    }

    void* result = NULL;
    if (pool->config.enable_cow && pool->accumulator_cow) {
        result = (void*)cow_read(channel->accumulator_handle);
    } else {
        result = (void*)channel->accumulator_handle;
    }

    pool->total_acquires++;
    nimcp_platform_mutex_unlock(&pool->mutex);

    return result;
}

NIMCP_EXPORT void buffer_pool_release_integration_buffer(
    buffer_pool_t pool,
    integration_buffer_t buffer
) {
    (void)pool;
    (void)buffer;
    // For CoW-based system, release happens when channel is deactivated
    // This is a no-op for now, kept for API completeness
}

NIMCP_EXPORT void buffer_pool_release_sliding_window(
    buffer_pool_t pool,
    sliding_window_t window
) {
    (void)pool;
    (void)window;
    // No-op for CoW system
}

NIMCP_EXPORT void buffer_pool_release_temporal_accumulator(
    buffer_pool_t pool,
    temporal_accumulator_t accumulator
) {
    (void)pool;
    (void)accumulator;
    // No-op for CoW system
}

#endif  /* 1 - Buffer type acquire/release functions enabled */

NIMCP_EXPORT bool buffer_pool_cow_make_private(
    buffer_pool_t pool,
    size_t channel_id
) {
    if (!pool || channel_id >= pool->num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "buffer_pool_cow_make_private: pool is NULL");
        return false;
    }
    if (!pool->config.enable_cow) return true;  // Already private

    nimcp_platform_mutex_lock(&pool->mutex);

    channel_info_t* channel = &pool->channels[channel_id];
    if (!channel->active) {
        nimcp_platform_mutex_unlock(&pool->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_cow_make_private: channel->active is NULL");
        return false;
    }

    bool success = true;

    // Make all buffers private
    if (channel->integration_handle && cow_is_shared(channel->integration_handle)) {
        success &= cow_make_private(channel->integration_handle);
        if (success) pool->cow_triggers++;
    }

    if (channel->window_handle && cow_is_shared(channel->window_handle)) {
        success &= cow_make_private(channel->window_handle);
        if (success) pool->cow_triggers++;
    }

    if (channel->accumulator_handle && cow_is_shared(channel->accumulator_handle)) {
        success &= cow_make_private(channel->accumulator_handle);
        if (success) pool->cow_triggers++;
    }

    nimcp_platform_mutex_unlock(&pool->mutex);

    return success;
}

NIMCP_EXPORT bool buffer_pool_is_channel_shared(
    buffer_pool_t pool,
    size_t channel_id
) {
    if (!pool || channel_id >= pool->num_channels) {
        return false;
    }
    if (!pool->config.enable_cow) {
        return false;
    }

    nimcp_platform_mutex_lock(&pool->mutex);

    channel_info_t* channel = &pool->channels[channel_id];
    bool shared = false;

    if (channel->active && channel->integration_handle) {
        shared = cow_is_shared(channel->integration_handle);
    }

    nimcp_platform_mutex_unlock(&pool->mutex);

    return shared;
}

NIMCP_EXPORT size_t buffer_pool_reset(buffer_pool_t pool) {
    if (!pool) return 0;

    nimcp_platform_mutex_lock(&pool->mutex);

    size_t reset_count = 0;

    // Release all active channels
    for (size_t i = 0; i < pool->num_channels; i++) {
        if (pool->channels[i].active) {
            if (pool->config.enable_cow) {
                cow_release(pool->channels[i].integration_handle);
                cow_release(pool->channels[i].window_handle);
                cow_release(pool->channels[i].accumulator_handle);
            } else {
                memory_pool_release(pool->integration_pool, (void*)pool->channels[i].integration_handle);
                memory_pool_release(pool->window_pool, (void*)pool->channels[i].window_handle);
                memory_pool_release(pool->accumulator_pool, (void*)pool->channels[i].accumulator_handle);
            }
            pool->channels[i].integration_handle = NULL;
            pool->channels[i].window_handle = NULL;
            pool->channels[i].accumulator_handle = NULL;
            pool->channels[i].active = false;
            reset_count++;
        }
    }

    // Reset pools
    memory_pool_reset(pool->integration_pool);
    memory_pool_reset(pool->window_pool);
    memory_pool_reset(pool->accumulator_pool);

    pool->num_channels = 0;

    nimcp_platform_mutex_unlock(&pool->mutex);

    return reset_count;
}

NIMCP_EXPORT bool buffer_pool_get_stats(
    buffer_pool_t pool,
    buffer_pool_stats_t* stats
) {
    if (!pool || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "buffer_pool_get_stats: required parameter is NULL (pool, stats)");
        return false;
    }

    nimcp_platform_mutex_lock(&pool->mutex);

    memset(stats, 0, sizeof(buffer_pool_stats_t));

    // Get pool statistics
    memory_pool_stats_t pool_stats;
    memory_pool_get_stats(pool->integration_pool, &pool_stats);
    stats->total_capacity = pool_stats.total_blocks;
    stats->allocated_buffers = pool_stats.allocated_blocks;
    stats->free_buffers = pool_stats.free_blocks;
    stats->peak_allocated = pool_stats.peak_allocated;
    stats->pool_memory_bytes = pool_stats.pool_size_bytes;

    // Get CoW statistics
    stats->cow_enabled = pool->config.enable_cow;
    if (pool->config.enable_cow && pool->integration_cow) {
        cow_stats_t cow_stats;
        cow_get_stats(pool->integration_cow, &cow_stats);
        stats->shared_channels = cow_stats.active_shared;
        stats->private_channels = cow_stats.active_private;
        stats->cow_triggers = cow_stats.cow_triggers;
        stats->memory_savings_bytes = cow_stats.memory_saved_bytes;

        // Template memory
        size_t integration_size = get_integration_buffer_size(&pool->config);
        size_t window_size = get_window_buffer_size(&pool->config);
        size_t accumulator_size = get_accumulator_buffer_size(&pool->config);
        stats->template_memory_bytes = integration_size + window_size + accumulator_size;

        // Private memory
        stats->private_memory_bytes = cow_stats.active_private * integration_size;
    }

    // Performance statistics
    stats->fast_allocations = pool->total_acquires - pool->cow_triggers;
    stats->slow_allocations = pool->cow_triggers;
    stats->avg_alloc_time_us = pool->total_acquires > 0 ?
        (float)pool->total_alloc_time_ns / (float)pool->total_acquires / (float)NIMCP_NS_PER_US : 0.0F;

    nimcp_platform_mutex_unlock(&pool->mutex);

    return true;
}

NIMCP_EXPORT bool buffer_pool_expand(
    buffer_pool_t pool,
    size_t additional_capacity
) {
    if (!pool || additional_capacity == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "buffer_pool_expand: pool is NULL");
        return false;
    }

    // Not implemented - pools are fixed size for Phase 0
    // Will be implemented in Phase 1 if needed
    (void)additional_capacity;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "buffer_pool_expand: pool is NULL");
    return false;
}

NIMCP_EXPORT size_t buffer_pool_calculate_memory(
    const buffer_pool_config_t* config
) {
    if (!config) return 0;

    size_t integration_size = (config->fast_size + config->medium_size + config->slow_size) * sizeof(float);
    size_t window_size = config->window_size * sizeof(float);
    size_t accumulator_size = integration_size;  // Same as integration

    size_t buffer_memory = (integration_size + window_size + accumulator_size);

    if (config->enable_cow) {
        // Template + expected active channels
        return buffer_memory + (buffer_memory * config->expected_active);
    } else {
        // All channels allocated
        return buffer_memory * config->max_channels;
    }
}
