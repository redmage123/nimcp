#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_sliding_window.c - Sliding Window Implementation
//=============================================================================

#include "middleware/buffering/nimcp_sliding_window.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "middleware/buffering/nimcp_circular_buffer.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "middleware_sliding_window"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sliding_window)

#include <string.h>
#include <math.h>
#include <float.h>

/**
 * @brief Sliding window structure
 *
 * WHAT: Window over temporal data with running statistics
 * WHY:  Efficient feature extraction from streams
 * HOW:  Circular buffer + Welford's online variance algorithm + Memory Pool
 */
struct sliding_window {
    circular_buffer_t* buffer;  /**< Underlying circular buffer */
    size_t window_size;         /**< Window size in samples */
    uint32_t overlap_percent;   /**< Overlap percentage (0-99) */

    // Running statistics (Welford's algorithm)
    window_stats_t stats;       /**< Current statistics */
    float m2;                   /**< Sum of squared differences (for variance) */

    /* P2-MW-05 fix: Separate lifetime counter for recalculation trigger.
     * stats.count saturates at window_size, so modulo on it never re-triggers.
     * This counter monotonically increases and is used for recalculation checks. */
    uint64_t total_samples_added; /**< Lifetime sample counter (never wraps in practice) */

    // Memory pool for temporary allocations (stats recalculation)
    memory_pool_t temp_buffer_pool;  /**< Pool for temp sample arrays (already a pointer type) */
};

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Update running statistics (Welford's algorithm)
 *
 * WHAT: Incrementally update mean and variance
 * WHY:  Numerically stable, single-pass computation
 * HOW:  Welford's online algorithm for variance
 *
 * Reference: Knuth, The Art of Computer Programming, Vol 2, 3rd Ed, p 232
 */
static void update_stats_add(sliding_window_t* window, float value) {
    if (!window) return;

    window->stats.count++;

    // Update sum
    window->stats.sum += value;

    // Update mean and M2 (Welford's algorithm)
    float delta = value - window->stats.mean;
    window->stats.mean += delta / window->stats.count;
    float delta2 = value - window->stats.mean;
    window->m2 += delta * delta2;

    // Update variance
    if (window->stats.count > 1) {
        window->stats.variance = window->m2 / (window->stats.count - 1);
    } else {
        window->stats.variance = 0.0F;
    }

    // Update min/max
    if (window->stats.count == 1) {
        window->stats.min = value;
        window->stats.max = value;
    } else {
        if (value < window->stats.min) window->stats.min = value;
        if (value > window->stats.max) window->stats.max = value;
    }
}

/**
 * @brief Update statistics when removing oldest sample
 *
 * WHAT: Remove sample from running statistics
 * WHY:  Maintain correct statistics as window slides
 * HOW:  Reverse Welford's algorithm (requires full recalculation for exact results)
 */
static void update_stats_remove(sliding_window_t* window, float old_value) {
    if (!window || window->stats.count == 0) return;

    // For exact results with min/max, need to recalculate from scratch
    // This is done when needed via reset_stats
    // For mean and variance, we can update incrementally

    window->stats.sum -= old_value;

    if (window->stats.count > 1) {
        float delta = old_value - window->stats.mean;
        window->stats.mean = (window->stats.sum) / (window->stats.count - 1);
        float delta2 = old_value - window->stats.mean;
        window->m2 -= delta * delta2;

        if (window->m2 < 0.0F) window->m2 = 0.0F;  // Numerical stability

        if (window->stats.count > 2) {
            window->stats.variance = window->m2 / (window->stats.count - 2);
        } else {
            window->stats.variance = 0.0F;
        }
    }

    window->stats.count--;
}

/**
 * @brief Recalculate all statistics from scratch
 *
 * WHAT: Recompute statistics from buffered samples
 * WHY:  Correct numerical drift, update min/max after removals
 * HOW:  Scan buffer, apply Welford's algorithm
 */
/**
 * P3 note: Thread-safety concern - this function pops all samples then pushes
 * them back. During the pop-push window, another thread could observe an empty
 * or partially-empty buffer. If thread safety is required, the caller must
 * hold an external lock. A future improvement would be to use a peek/iterate
 * API on the circular buffer that doesn't modify the buffer state.
 */
static void recalculate_stats(sliding_window_t* window) {
    if (!window) return;

    // Reset statistics
    memset(&window->stats, 0, sizeof(window_stats_t));
    window->m2 = 0.0F;
    window->stats.min = FLT_MAX;
    window->stats.max = -FLT_MAX;

    // Get all samples
    size_t count = circular_buffer_size(window->buffer);
    if (count == 0) return;

    // Use memory pool for temp allocation (63x faster than malloc)
    float* samples = (float*)memory_pool_acquire(window->temp_buffer_pool);
    if (!samples) return;

    size_t retrieved = circular_buffer_pop_batch(window->buffer, samples, count);

    // Recalculate from scratch
    for (size_t i = 0; i < retrieved; i++) {
        update_stats_add(window, samples[i]);
    }

    // Put samples back
    circular_buffer_push_batch(window->buffer, samples, retrieved);

    // Release back to pool (no free, just mark available)
    memory_pool_release(window->temp_buffer_pool, samples);
}

//=============================================================================
// LIFECYCLE
//=============================================================================

sliding_window_t* sliding_window_create(
    size_t window_size,
    uint32_t overlap_percent
) {
    // Guard: validate inputs
    if (window_size == 0 || overlap_percent >= 100) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sliding_window_create: invalid window_size or overlap_percent");
        return NULL;
    }

    // Allocate structure
    sliding_window_t* window = nimcp_calloc(1, sizeof(sliding_window_t));
    if (!window) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sliding_window_create: failed to allocate window");
        return NULL;
    }

    // Create circular buffer (overwrite oldest on overflow)
    window->buffer = circular_buffer_create(
        sizeof(float),
        window_size,
        OVERFLOW_OVERWRITE
    );
    if (!window->buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sliding_window_create: failed to create buffer");
        nimcp_free(window);
        return NULL;
    }

    // Initialize configuration
    window->window_size = window_size;
    window->overlap_percent = overlap_percent;

    // Create memory pool for temp allocations (stats recalculation)
    // Pool size: 2 buffers × window_size floats (double buffer for safety)
    /* P2-MW-04 fix: Check for integer overflow in block_size calculation */
    if (window_size > SIZE_MAX / sizeof(float)) {
        circular_buffer_destroy(window->buffer);
        nimcp_free(window);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sliding_window_create: window_size overflow");
        return NULL;
    }
    memory_pool_config_t pool_config = {
        .block_size = window_size * sizeof(float),  // Each block holds full window
        .num_blocks = 2,                             // Double buffer for safety
        .alignment = 16,                             // 16-byte alignment for SIMD
        .enable_tracking = false,                    // No need for stats tracking
        .enable_guard_pages = false                  // No guard pages needed
    };
    window->temp_buffer_pool = memory_pool_create(&pool_config);
    if (!window->temp_buffer_pool) {
        circular_buffer_destroy(window->buffer);
        nimcp_free(window);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sliding_window_create: window->temp_buffer_pool is NULL");
        return NULL;
    }

    // Initialize statistics
    memset(&window->stats, 0, sizeof(window_stats_t));
    window->m2 = 0.0F;
    window->stats.min = FLT_MAX;
    window->stats.max = -FLT_MAX;

    return window;
}

void sliding_window_destroy(sliding_window_t* window) {
    if (!window) return;

    circular_buffer_destroy(window->buffer);
    memory_pool_destroy(window->temp_buffer_pool);
    nimcp_free(window);
}

//=============================================================================
// DATA OPERATIONS
//=============================================================================

bool sliding_window_add(sliding_window_t* window, float value) {
    // Guard: validate input
    if (!window) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sliding_window_add: window is NULL");
        return false;
    }

    // If window is full, remove oldest sample
    float old_value = 0.0F;
    bool was_full = circular_buffer_is_full(window->buffer);

    if (was_full) {
        circular_buffer_pop(window->buffer, &old_value);
        update_stats_remove(window, old_value);
    }

    // Add new sample
    if (!circular_buffer_push(window->buffer, &value)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sliding_window_add: circular_buffer_push is NULL");
        return false;
    }

    // Update statistics
    update_stats_add(window, value);

    /* P2-MW-05 fix: Use lifetime counter instead of stats.count for recalculation
     * trigger. stats.count saturates at window_size and would never re-trigger. */
    window->total_samples_added++;
    if (window->total_samples_added % 1000 == 0) {
        recalculate_stats(window);
    }

    return true;
}

size_t sliding_window_add_batch(
    sliding_window_t* window,
    const float* values,
    size_t count
) {
    // Guard: validate inputs
    if (!window || !values || count == 0) return 0;

    size_t added = 0;
    for (size_t i = 0; i < count; i++) {
        if (sliding_window_add(window, values[i])) {
            added++;
        }
    }

    return added;
}

bool sliding_window_get_stats(
    const sliding_window_t* window,
    window_stats_t* stats
) {
    // Guard: validate inputs
    if (!window || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sliding_window_get_stats: required parameter is NULL (window, stats)");
        return false;
    }

    // Copy statistics
    memcpy(stats, &window->stats, sizeof(window_stats_t));
    return true;
}

size_t sliding_window_get_samples(
    const sliding_window_t* window,
    float* samples,
    size_t max_samples
) {
    // Guard: validate inputs
    if (!window || !samples || max_samples == 0) return 0;

    // Get samples from buffer
    size_t count = circular_buffer_size(window->buffer);
    size_t to_read = (count < max_samples) ? count : max_samples;

    // Read via peek to avoid modifying buffer
    size_t read_count = 0;
    for (size_t i = 0; i < to_read; i++) {
        if (circular_buffer_peek(window->buffer, i, &samples[i])) {
            read_count++;
        }
    }

    return read_count;
}

//=============================================================================
// AGGREGATION FUNCTIONS
//=============================================================================

float sliding_window_mean(const sliding_window_t* window) {
    // Guard: validate input
    if (!window || window->stats.count == 0) return 0.0F;
    return window->stats.mean;
}

float sliding_window_variance(const sliding_window_t* window) {
    // Guard: validate input
    if (!window || window->stats.count < 2) return 0.0F;
    return window->stats.variance;
}

float sliding_window_stddev(const sliding_window_t* window) {
    // Guard: validate input
    if (!window || window->stats.count < 2) return 0.0F;
    return sqrtf(window->stats.variance);
}

float sliding_window_min(const sliding_window_t* window) {
    // Guard: validate input
    if (!window || window->stats.count == 0) return 0.0F;
    return (window->stats.min == FLT_MAX) ? 0.0F : window->stats.min;
}

float sliding_window_max(const sliding_window_t* window) {
    // Guard: validate input
    if (!window || window->stats.count == 0) return 0.0F;
    return (window->stats.max == -FLT_MAX) ? 0.0F : window->stats.max;
}

float sliding_window_range(const sliding_window_t* window) {
    // Guard: validate input
    if (!window || window->stats.count == 0) return 0.0F;

    float min = sliding_window_min(window);
    float max = sliding_window_max(window);
    return max - min;
}

//=============================================================================
// QUERY OPERATIONS
//=============================================================================

size_t sliding_window_size(const sliding_window_t* window) {
    // Guard: validate input
    if (!window) return 0;
    return window->window_size;
}

size_t sliding_window_count(const sliding_window_t* window) {
    // Guard: validate input
    if (!window) return 0;
    return window->stats.count;
}

bool sliding_window_is_full(const sliding_window_t* window) {
    /* P3-MW-03 fix: Consistent NULL handling across query functions.
     * All other query functions (size, count, overlap, stride) return early
     * without throwing. is_full should be consistent. */
    if (!window) return false;
    return window->stats.count >= window->window_size;
}

uint32_t sliding_window_overlap(const sliding_window_t* window) {
    // Guard: validate input
    if (!window) return 0;
    return window->overlap_percent;
}

size_t sliding_window_stride(const sliding_window_t* window) {
    // Guard: validate input
    if (!window || window->window_size == 0) return 0;

    // Stride = window_size * (1 - overlap%)
    return window->window_size * (100 - window->overlap_percent) / 100;
}

//=============================================================================
// MANAGEMENT
//=============================================================================

void sliding_window_clear(sliding_window_t* window) {
    // Guard: validate input
    if (!window) return;

    // Clear buffer
    circular_buffer_clear(window->buffer);

    // Reset statistics
    memset(&window->stats, 0, sizeof(window_stats_t));
    window->m2 = 0.0F;
    window->stats.min = FLT_MAX;
    window->stats.max = -FLT_MAX;
}

void sliding_window_reset_stats(sliding_window_t* window) {
    // Guard: validate input
    if (!window) return;

    // Recalculate from scratch
    recalculate_stats(window);
}
