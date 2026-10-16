//=============================================================================
// nimcp_buffer_pool.h - Buffer-Specific Memory Pool with CoW Support
//=============================================================================
/**
 * @file nimcp_buffer_pool.h
 * @brief Memory pool specialized for temporal buffers with CoW lazy initialization
 *
 * WHAT: Buffer-specific pool with Copy-on-Write for sparse channel allocation
 * WHY:  Optimize for common pattern: many channels declared, few active
 * HOW:  Shared template + CoW on first write + memory pool for copies
 *
 * ARCHITECTURE:
 *
 *   CoW + Pool Integration:
 *   ┌──────────────────────────────────────────────────────────┐
 *   │  Shared Template (1 allocation)                          │
 *   │  ┌────────────┐  ┌────────────┐  ┌────────────┐        │
 *   │  │Fast Buffer │  │Medium Buf  │  │Slow Buffer │        │
 *   │  └────────────┘  └────────────┘  └────────────┘        │
 *   └──────────────────────────────────────────────────────────┘
 *            ▲                ▲                ▲
 *            │                │                │
 *   ┌────────┴────────┬───────┴──────┬────────┴────────┐
 *   │  Channel 0      │  Channel 1   │  Channel 2      │
 *   │  [Shared]       │  [Shared]    │  [Private] ◄────┼─── CoW copy from pool
 *   └─────────────────┴──────────────┴─────────────────┘
 *                                            │
 *                                            ▼
 *                                     Memory Pool
 *                                     (Fast allocation)
 *
 * PERFORMANCE:
 * - Without CoW+Pool: 1000 channels × 5s = 5000ms creation
 * - With CoW+Pool: 1ms template + 100 active × 5ms = 501ms (10x faster)
 *
 * MEMORY:
 * - Without CoW: 1000 channels × 2,220 allocations = 2.2M allocations
 * - With CoW: 1 template + 100 active = 222K allocations (10x less)
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#ifndef NIMCP_BUFFER_POOL_H
#define NIMCP_BUFFER_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "utils/memory/nimcp_memory_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

// Buffer type definitions for pool management
// These are opaque pointers to buffers managed by the pool
// The actual buffer structure is internal to the pool implementation
typedef void* integration_buffer_t;    /**< Integration buffer handle (fast/medium/slow timescales) */
typedef void* sliding_window_t;        /**< Sliding window buffer handle */
typedef void* temporal_accumulator_t;  /**< Temporal accumulator buffer handle */

//=============================================================================
// Configuration and Types
//=============================================================================

/**
 * @brief Buffer pool configuration
 */
typedef struct {
    // Buffer sizes (for pool sizing)
    size_t fast_size;           /**< Fast timescale buffer size */
    size_t medium_size;         /**< Medium timescale buffer size */
    size_t slow_size;           /**< Slow timescale buffer size */
    size_t window_size;         /**< Sliding window size */

    // Pool configuration
    size_t max_channels;        /**< Maximum channels to support */
    size_t expected_active;     /**< Expected active channels (for pool sizing) */

    // CoW settings
    bool enable_cow;            /**< Enable Copy-on-Write lazy initialization */
    bool aggressive_init;       /**< Initialize all channels upfront (no CoW) */

    // Memory management
    float overallocation_factor; /**< Pool overallocation (1.0-2.0) */
} buffer_pool_config_t;

/**
 * @brief Buffer pool handle (opaque)
 */
typedef struct buffer_pool_struct* buffer_pool_t;

/**
 * @brief Buffer pool statistics
 */
typedef struct {
    // Pool statistics
    size_t total_capacity;      /**< Total buffer capacity */
    size_t allocated_buffers;   /**< Currently allocated buffers */
    size_t free_buffers;        /**< Free buffers in pool */
    size_t peak_allocated;      /**< Peak allocation */

    // CoW statistics
    bool cow_enabled;           /**< Is CoW enabled? */
    size_t shared_channels;     /**< Channels using shared template */
    size_t private_channels;    /**< Channels with private buffers */
    size_t cow_triggers;        /**< Total CoW copy operations */

    // Memory statistics
    size_t pool_memory_bytes;   /**< Total pool memory */
    size_t template_memory_bytes; /**< Shared template memory */
    size_t private_memory_bytes;  /**< Private buffer memory */
    size_t memory_savings_bytes;  /**< Memory saved by CoW */

    // Performance statistics
    uint64_t fast_allocations;  /**< Allocations from pool */
    uint64_t slow_allocations;  /**< Allocations from malloc */
    float avg_alloc_time_us;    /**< Average allocation time */
} buffer_pool_stats_t;

//=============================================================================
// Buffer Pool API
//=============================================================================

/**
 * @brief Create buffer pool
 *
 * WHAT: Creates memory pool optimized for temporal buffers with CoW
 * WHY:  Reduce allocation overhead and enable sparse channel usage
 * HOW:  Pre-allocates pool + creates shared template for CoW
 *
 * @param config Pool configuration (NULL for defaults)
 * @return Pool handle or NULL on failure
 *
 * COMPLEXITY: O(1) if CoW enabled, O(n) if aggressive_init=true
 * MEMORY: expected_active × buffer_sizes (not max_channels!)
 *
 * EXAMPLE:
 * ```c
 * buffer_pool_config_t config = {
 *     .fast_size = 100,
 *     .medium_size = 500,
 *     .slow_size = 2500,
 *     .max_channels = 1000,
 *     .expected_active = 100,
 *     .enable_cow = true
 * };
 * buffer_pool_t pool = buffer_pool_create(&config);
 * // Pool allocates for 100 channels, supports 1000 via CoW
 * ```
 */
NIMCP_EXPORT buffer_pool_t buffer_pool_create(const buffer_pool_config_t* config);

/**
 * @brief Destroy buffer pool
 *
 * @param pool Pool handle
 */
NIMCP_EXPORT void buffer_pool_destroy(buffer_pool_t pool);

//=============================================================================
// Buffer Acquisition API
//=============================================================================
/**
 * @brief Acquire integration buffer from pool
 *
 * WHAT: Allocates integration buffer (fast/medium/slow timescales)
 * WHY:  Avoid malloc overhead
 * HOW:  CoW reference (if new) or pool allocation (if needs private copy)
 *
 * @param pool Pool handle
 * @param channel_id Channel identifier
 * @param needs_private true to force private copy, false for CoW shared
 * @return Buffer handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Thread-safe
 */
NIMCP_EXPORT integration_buffer_t buffer_pool_acquire_integration_buffer(
    buffer_pool_t pool,
    size_t channel_id,
    bool needs_private
);

/**
 * @brief Acquire sliding window from pool
 *
 * @param pool Pool handle
 * @param channel_id Channel identifier
 * @param needs_private true to force private copy
 * @return Window handle or NULL on failure
 */
NIMCP_EXPORT sliding_window_t buffer_pool_acquire_sliding_window(
    buffer_pool_t pool,
    size_t channel_id,
    bool needs_private
);

/**
 * @brief Acquire temporal accumulator from pool
 *
 * @param pool Pool handle
 * @param channel_id Channel identifier
 * @param needs_private true to force private copy
 * @return Accumulator handle or NULL on failure
 */
NIMCP_EXPORT temporal_accumulator_t buffer_pool_acquire_temporal_accumulator(
    buffer_pool_t pool,
    size_t channel_id,
    bool needs_private
);

/**
 * @brief Release integration buffer back to pool
 *
 * @param pool Pool handle
 * @param buffer Buffer to release
 */
NIMCP_EXPORT void buffer_pool_release_integration_buffer(
    buffer_pool_t pool,
    integration_buffer_t buffer
);

/**
 * @brief Release sliding window back to pool
 *
 * @param pool Pool handle
 * @param window Window to release
 */
NIMCP_EXPORT void buffer_pool_release_sliding_window(
    buffer_pool_t pool,
    sliding_window_t window
);

/**
 * @brief Release temporal accumulator back to pool
 *
 * @param pool Pool handle
 * @param accumulator Accumulator to release
 */
NIMCP_EXPORT void buffer_pool_release_temporal_accumulator(
    buffer_pool_t pool,
    temporal_accumulator_t accumulator
);

/**
 * @brief Trigger Copy-on-Write for channel
 *
 * WHAT: Converts shared template reference to private buffer
 * WHY:  Enable writes to channel-specific data
 * HOW:  Allocates from pool, copies template data
 *
 * @param pool Pool handle
 * @param channel_id Channel identifier
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(m) where m = buffer size
 * WHEN CALLED: Automatically on first write to shared channel
 */
NIMCP_EXPORT bool buffer_pool_cow_make_private(
    buffer_pool_t pool,
    size_t channel_id
);

/**
 * @brief Check if channel is using shared template
 *
 * @param pool Pool handle
 * @param channel_id Channel identifier
 * @return true if shared (CoW), false if private
 */
NIMCP_EXPORT bool buffer_pool_is_channel_shared(
    buffer_pool_t pool,
    size_t channel_id
);

/**
 * @brief Reset pool (prepare for reuse)
 *
 * WHAT: Returns all buffers to pool, resets to shared template
 * WHY:  Fast reuse without reallocation
 * HOW:  Marks all channels as shared, rebuilds free-list
 *
 * @param pool Pool handle
 * @return Number of buffers reset
 */
NIMCP_EXPORT size_t buffer_pool_reset(buffer_pool_t pool);

/**
 * @brief Get pool statistics
 *
 * @param pool Pool handle
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool buffer_pool_get_stats(
    buffer_pool_t pool,
    buffer_pool_stats_t* stats
);

/**
 * @brief Expand pool capacity
 *
 * WHAT: Grows pool to accommodate more channels
 * WHY:  Handle cases where active channels exceed expected_active
 * HOW:  Allocates additional pool blocks
 *
 * @param pool Pool handle
 * @param additional_capacity Number of additional buffers
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool buffer_pool_expand(
    buffer_pool_t pool,
    size_t additional_capacity
);

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Calculate memory required for buffer pool
 *
 * @param config Pool configuration
 * @return Required memory in bytes
 */
NIMCP_EXPORT size_t buffer_pool_calculate_memory(
    const buffer_pool_config_t* config
);

/**
 * @brief Get default buffer pool configuration
 *
 * @param fast_size Fast timescale buffer size
 * @param medium_size Medium timescale buffer size
 * @param slow_size Slow timescale buffer size
 * @param max_channels Maximum channels
 * @param expected_active Expected active channels
 * @return Default configuration
 */
static inline buffer_pool_config_t buffer_pool_default_config(
    size_t fast_size,
    size_t medium_size,
    size_t slow_size,
    size_t max_channels,
    size_t expected_active
) {
    buffer_pool_config_t config = {
        .fast_size = fast_size,
        .medium_size = medium_size,
        .slow_size = slow_size,
        .window_size = fast_size,  // Match fast for consistency
        .max_channels = max_channels,
        .expected_active = expected_active,
        .enable_cow = true,         // Enable CoW by default
        .aggressive_init = false,   // Lazy initialization
        .overallocation_factor = 1.2f // 20% overallocation
    };
    return config;
}

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_BUFFER_POOL_H
