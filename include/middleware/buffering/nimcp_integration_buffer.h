//=============================================================================
// nimcp_integration_buffer.h - Multi-Timescale Integration Buffer
//=============================================================================

#ifndef NIMCP_INTEGRATION_BUFFER_H
#define NIMCP_INTEGRATION_BUFFER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_integration_buffer.h
 * @brief Hierarchical multi-timescale temporal integration
 *
 * WHAT: Pyramid-structured buffer with multiple integration timescales
 * WHY:  Neural processing occurs at multiple temporal scales simultaneously
 * HOW:  Fast (10ms), medium (100ms), slow (1s) integrated representations
 *
 * FEATURES:
 * - Three timescale levels (configurable)
 * - Hierarchical buffering (pyramid structure)
 * - Delta encoding for compression
 * - Timestamp tracking
 * - Time-range queries
 */

// Integration level
typedef enum {
    TIMESCALE_FAST = 0,    /**< Fast timescale (10ms typical) */
    TIMESCALE_MEDIUM = 1,  /**< Medium timescale (100ms typical) */
    TIMESCALE_SLOW = 2,    /**< Slow timescale (1s typical) */
    TIMESCALE_COUNT = 3    /**< Number of timescale levels */
} timescale_level_t;

// Time-stamped sample
typedef struct {
    float value;      /**< Sample value */
    uint64_t timestamp; /**< Timestamp (in microseconds or simulation ticks) */
} timestamped_sample_t;

// Opaque integration buffer type
typedef struct integration_buffer integration_buffer_t;

//=============================================================================
// LIFECYCLE
//=============================================================================

/**
 * @brief Create multi-timescale integration buffer
 *
 * WHAT: Allocate hierarchical buffer with three timescale levels
 * WHY:  Enable multi-scale temporal processing
 * HOW:  Create three buffers with different sizes and integration rates
 *
 * @param fast_size Size of fast buffer (samples)
 * @param medium_size Size of medium buffer (samples)
 * @param slow_size Size of slow buffer (samples)
 * @param num_channels Number of parallel channels
 * @return Integration buffer or NULL on failure
 */
integration_buffer_t* integration_buffer_create(
    size_t fast_size,
    size_t medium_size,
    size_t slow_size,
    size_t num_channels
);

/**
 * @brief Destroy integration buffer
 *
 * WHAT: Free all buffer resources
 * WHY:  Prevent memory leaks
 * HOW:  Free all timescale buffers and structure
 *
 * @param buffer Buffer to destroy (can be NULL)
 */
void integration_buffer_destroy(integration_buffer_t* buffer);

//=============================================================================
// DATA OPERATIONS
//=============================================================================

/**
 * @brief Add sample to all timescale levels
 *
 * WHAT: Push sample to fast buffer, propagate to slower levels
 * WHY:  Maintain multi-scale representation
 * HOW:  Add to fast, average to medium/slow based on downsampling ratio
 *
 * @param buffer Buffer to update
 * @param channel Channel index
 * @param value Sample value
 * @param timestamp Sample timestamp
 * @return true on success
 */
bool integration_buffer_add(
    integration_buffer_t* buffer,
    size_t channel,
    float value,
    uint64_t timestamp
);

/**
 * @brief Get latest value from timescale
 *
 * WHAT: Retrieve most recent integrated value at timescale
 * WHY:  Access multi-scale temporal features
 * HOW:  Read from appropriate level buffer
 *
 * @param buffer Buffer to query
 * @param level Timescale level to read from
 * @param channel Channel index
 * @return Latest value, or 0.0 if invalid
 */
float integration_buffer_get_latest(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel
);

/**
 * @brief Get time window of samples
 *
 * WHAT: Retrieve all samples within time range
 * WHY:  Time-based queries for event correlation
 * HOW:  Search buffer for samples matching timestamp range
 *
 * @param buffer Buffer to query
 * @param level Timescale level
 * @param channel Channel index
 * @param start_time Start timestamp (inclusive)
 * @param end_time End timestamp (inclusive)
 * @param samples Output array
 * @param max_samples Size of output array
 * @return Number of samples retrieved
 */
size_t integration_buffer_get_time_range(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel,
    uint64_t start_time,
    uint64_t end_time,
    timestamped_sample_t* samples,
    size_t max_samples
);

/**
 * @brief Get all samples from a timescale level
 *
 * WHAT: Retrieve entire buffer for a timescale
 * WHY:  Bulk access for analysis
 * HOW:  Copy all samples from level buffer
 *
 * @param buffer Buffer to query
 * @param level Timescale level
 * @param channel Channel index
 * @param samples Output array
 * @param max_samples Size of output array
 * @return Number of samples retrieved
 */
size_t integration_buffer_get_all_samples(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel,
    timestamped_sample_t* samples,
    size_t max_samples
);

//=============================================================================
// STATISTICS
//=============================================================================

/**
 * @brief Get timescale mean
 *
 * WHAT: Calculate mean across timescale buffer
 * WHY:  Temporal baseline at specific scale
 * HOW:  Average all samples in level buffer
 *
 * @param buffer Buffer to query
 * @param level Timescale level
 * @param channel Channel index
 * @return Mean value, or 0.0 if invalid
 */
float integration_buffer_mean(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel
);

/**
 * @brief Get timescale variance
 *
 * WHAT: Calculate variance across timescale buffer
 * WHY:  Temporal variability at specific scale
 * HOW:  Compute variance of samples in level buffer
 *
 * @param buffer Buffer to query
 * @param level Timescale level
 * @param channel Channel index
 * @return Variance, or 0.0 if invalid
 */
float integration_buffer_variance(
    const integration_buffer_t* buffer,
    timescale_level_t level,
    size_t channel
);

/**
 * @brief Calculate trend across timescales
 *
 * WHAT: Compute difference between slow and fast levels
 * WHY:  Detect long-term trends vs short-term fluctuations
 * HOW:  Return (slow_mean - fast_mean)
 *
 * @param buffer Buffer to query
 * @param channel Channel index
 * @return Trend value (positive = increasing, negative = decreasing)
 */
float integration_buffer_trend(
    const integration_buffer_t* buffer,
    size_t channel
);

//=============================================================================
// QUERY OPERATIONS
//=============================================================================

/**
 * @brief Get buffer capacity for timescale
 *
 * WHAT: Return buffer size for specified level
 * WHY:  Determine temporal extent at each scale
 * HOW:  Return size field for level
 *
 * @param buffer Buffer to query
 * @param level Timescale level
 * @return Buffer capacity in samples
 */
size_t integration_buffer_capacity(
    const integration_buffer_t* buffer,
    timescale_level_t level
);

/**
 * @brief Get current sample count for timescale
 *
 * WHAT: Return number of samples in level buffer
 * WHY:  Check buffer fill status
 * HOW:  Query underlying circular buffer
 *
 * @param buffer Buffer to query
 * @param level Timescale level
 * @return Current sample count
 */
size_t integration_buffer_count(
    const integration_buffer_t* buffer,
    timescale_level_t level
);

/**
 * @brief Get number of channels
 *
 * WHAT: Return channel count
 * WHY:  Determine buffer dimensionality
 * HOW:  Return num_channels field
 *
 * @param buffer Buffer to query
 * @return Number of channels
 */
size_t integration_buffer_num_channels(const integration_buffer_t* buffer);

//=============================================================================
// MANAGEMENT
//=============================================================================

/**
 * @brief Clear all buffers
 *
 * WHAT: Reset all timescale levels to empty
 * WHY:  Discard temporal history
 * HOW:  Clear each level buffer
 *
 * @param buffer Buffer to clear
 */
void integration_buffer_clear(integration_buffer_t* buffer);

/**
 * @brief Clear specific timescale level
 *
 * WHAT: Reset one timescale buffer
 * WHY:  Selective history clearing
 * HOW:  Clear specified level buffer
 *
 * @param buffer Buffer to clear
 * @param level Timescale level to clear
 */
void integration_buffer_clear_level(
    integration_buffer_t* buffer,
    timescale_level_t level
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_INTEGRATION_BUFFER_H
