//=============================================================================
// nimcp_sliding_window.h - Sliding Window Temporal Buffer
//=============================================================================

#ifndef NIMCP_SLIDING_WINDOW_H
#define NIMCP_SLIDING_WINDOW_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_sliding_window.h
 * @brief Sliding window buffer with statistical aggregation
 *
 * WHAT: Temporal window over continuous neural data streams
 * WHY:  Extract features from recent history (means, variance, trends)
 * HOW:  Circular buffer with configurable window and overlap
 *
 * FEATURES:
 * - Multiple window sizes (10ms, 100ms, 1s, 10s)
 * - Configurable overlap (0%, 50%, 75%, 90%)
 * - Statistical aggregation (mean, variance, min, max)
 * - Online computation (incremental updates)
 * - Thread-safe window access
 */

// Window statistics
typedef struct {
    float mean;      /**< Window mean */
    float variance;  /**< Window variance */
    float min;       /**< Window minimum */
    float max;       /**< Window maximum */
    float sum;       /**< Window sum */
    size_t count;    /**< Number of samples in window */
} window_stats_t;

// Opaque sliding window type
typedef struct sliding_window sliding_window_t;

//=============================================================================
// LIFECYCLE
//=============================================================================

/**
 * @brief Create sliding window
 *
 * WHAT: Allocate window with specified size and overlap
 * WHY:  Need temporal aggregation over recent neural activity
 * HOW:  Allocate circular buffer, initialize statistics
 *
 * @param window_size Number of samples in window
 * @param overlap_percent Overlap between windows (0-99)
 * @return Sliding window or NULL on failure
 */
sliding_window_t* sliding_window_create(
    size_t window_size,
    uint32_t overlap_percent
);

/**
 * @brief Destroy sliding window
 *
 * WHAT: Free all window resources
 * WHY:  Prevent memory leaks
 * HOW:  Free buffer and structure
 *
 * @param window Window to destroy (can be NULL)
 */
void sliding_window_destroy(sliding_window_t* window);

//=============================================================================
// DATA OPERATIONS
//=============================================================================

/**
 * @brief Add sample to window
 *
 * WHAT: Push new data point and update statistics
 * WHY:  Maintain current temporal window
 * HOW:  Add to circular buffer, update running statistics
 *
 * @param window Window to update
 * @param value New sample value
 * @return true on success
 */
bool sliding_window_add(sliding_window_t* window, float value);

/**
 * @brief Add multiple samples
 *
 * WHAT: Batch add samples for efficiency
 * WHY:  More efficient than individual adds
 * HOW:  Add each sample, update stats once at end
 *
 * @param window Window to update
 * @param values Array of sample values
 * @param count Number of samples
 * @return Number of samples successfully added
 */
size_t sliding_window_add_batch(
    sliding_window_t* window,
    const float* values,
    size_t count
);

/**
 * @brief Get current window statistics
 *
 * WHAT: Retrieve aggregated window statistics
 * WHY:  Extract features for processing/learning
 * HOW:  Copy current statistics structure
 *
 * @param window Window to query
 * @param stats Output for statistics
 * @return true on success
 */
bool sliding_window_get_stats(
    const sliding_window_t* window,
    window_stats_t* stats
);

/**
 * @brief Get all samples in current window
 *
 * WHAT: Copy window contents to array
 * WHY:  Access raw temporal data for custom processing
 * HOW:  Read from circular buffer in temporal order
 *
 * @param window Window to read from
 * @param samples Output array (must hold window_size elements)
 * @param max_samples Size of output array
 * @return Number of samples copied
 */
size_t sliding_window_get_samples(
    const sliding_window_t* window,
    float* samples,
    size_t max_samples
);

//=============================================================================
// AGGREGATION FUNCTIONS
//=============================================================================

/**
 * @brief Calculate window mean
 *
 * WHAT: Compute average of samples in window
 * WHY:  Common feature for neural signal processing
 * HOW:  Return precomputed running mean
 *
 * @param window Window to query
 * @return Mean value, or 0.0 if empty/invalid
 */
float sliding_window_mean(const sliding_window_t* window);

/**
 * @brief Calculate window variance
 *
 * WHAT: Compute variance of samples in window
 * WHY:  Measure signal variability/noise
 * HOW:  Return precomputed running variance (Welford's algorithm)
 *
 * @param window Window to query
 * @return Variance value, or 0.0 if empty/invalid
 */
float sliding_window_variance(const sliding_window_t* window);

/**
 * @brief Calculate window standard deviation
 *
 * WHAT: Compute standard deviation of samples
 * WHY:  Common normalization parameter
 * HOW:  Return sqrt(variance)
 *
 * @param window Window to query
 * @return Standard deviation, or 0.0 if empty/invalid
 */
float sliding_window_stddev(const sliding_window_t* window);

/**
 * @brief Get window minimum value
 *
 * WHAT: Return smallest value in window
 * WHY:  Range calculation, outlier detection
 * HOW:  Return tracked minimum
 *
 * @param window Window to query
 * @return Minimum value, or 0.0 if empty/invalid
 */
float sliding_window_min(const sliding_window_t* window);

/**
 * @brief Get window maximum value
 *
 * WHAT: Return largest value in window
 * WHY:  Range calculation, saturation detection
 * HOW:  Return tracked maximum
 *
 * @param window Window to query
 * @return Maximum value, or 0.0 if empty/invalid
 */
float sliding_window_max(const sliding_window_t* window);

/**
 * @brief Calculate window range
 *
 * WHAT: Compute difference between max and min
 * WHY:  Measure signal dynamic range
 * HOW:  Return (max - min)
 *
 * @param window Window to query
 * @return Range value, or 0.0 if empty/invalid
 */
float sliding_window_range(const sliding_window_t* window);

//=============================================================================
// QUERY OPERATIONS
//=============================================================================

/**
 * @brief Get window size
 *
 * WHAT: Return configured window size
 * WHY:  Determine temporal extent of window
 * HOW:  Return size field
 *
 * @param window Window to query
 * @return Window size in samples
 */
size_t sliding_window_size(const sliding_window_t* window);

/**
 * @brief Get current sample count
 *
 * WHAT: Return number of samples currently in window
 * WHY:  Check if window is full
 * HOW:  Return count from statistics
 *
 * @param window Window to query
 * @return Current sample count
 */
size_t sliding_window_count(const sliding_window_t* window);

/**
 * @brief Check if window is full
 *
 * WHAT: Test if window contains maximum samples
 * WHY:  Determine if statistics are stable
 * HOW:  Compare count to window size
 *
 * @param window Window to check
 * @return true if window full
 */
bool sliding_window_is_full(const sliding_window_t* window);

/**
 * @brief Get overlap configuration
 *
 * WHAT: Return configured overlap percentage
 * WHY:  Understand window stride
 * HOW:  Return overlap field
 *
 * @param window Window to query
 * @return Overlap percentage (0-99)
 */
uint32_t sliding_window_overlap(const sliding_window_t* window);

/**
 * @brief Calculate window stride
 *
 * WHAT: Compute samples between window starts
 * WHY:  Determine update frequency
 * HOW:  Return size * (1 - overlap/100)
 *
 * @param window Window to query
 * @return Stride in samples
 */
size_t sliding_window_stride(const sliding_window_t* window);

//=============================================================================
// MANAGEMENT
//=============================================================================

/**
 * @brief Clear window contents
 *
 * WHAT: Reset window to empty state
 * WHY:  Discard history on event/reset
 * HOW:  Clear buffer and reset statistics
 *
 * @param window Window to clear
 */
void sliding_window_clear(sliding_window_t* window);

/**
 * @brief Reset window statistics
 *
 * WHAT: Recalculate statistics from scratch
 * WHY:  Correct numerical drift in running calculations
 * HOW:  Scan all samples, recompute mean/variance
 *
 * @param window Window to reset
 */
void sliding_window_reset_stats(sliding_window_t* window);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SLIDING_WINDOW_H
