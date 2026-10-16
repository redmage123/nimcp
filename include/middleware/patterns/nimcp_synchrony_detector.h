//=============================================================================
// nimcp_synchrony_detector.h - Synchronized Neural Activity Detection
//=============================================================================

#ifndef NIMCP_SYNCHRONY_DETECTOR_H
#define NIMCP_SYNCHRONY_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_synchrony_detector.h
 * @brief Detect synchronized neural activity patterns
 *
 * WHAT: Real-time detection of synchronized firing in neural populations
 * WHY:  Synchrony indicates coordinated neural processing, critical events, and binding
 * HOW:  Cross-correlation, population spike coincidence, and sliding window analysis
 *
 * BIOLOGICAL BASIS:
 * - Synchronous firing binds distributed representations (Binding Problem)
 * - Gamma synchrony (30-100Hz) underlies attention and consciousness
 * - Critical events (>50% population firing) signal important state transitions
 * - Synchrony measured via cross-correlation and spike coincidence detection
 *
 * ALGORITHMS:
 * - Pairwise cross-correlation (all neuron pairs within window)
 * - Population spike coincidence (±5ms window for biological realism)
 * - Synchrony index calculation (0-1 scale, >0.7 is highly synchronized)
 * - Critical event detection (>50% population threshold)
 * - Multi-scale analysis (10ms, 100ms, 1000ms windows)
 */

// ============================================================================
// CONSTANTS
// ============================================================================

#define SYNCHRONY_COINCIDENCE_WINDOW_MS 5.0f    // ±5ms biological window
#define SYNCHRONY_CRITICAL_THRESHOLD 0.5f       // 50% population threshold
#define SYNCHRONY_HIGH_THRESHOLD 0.7f           // High synchrony threshold
#define SYNCHRONY_MAX_NEURONS 10000             // Maximum neurons to analyze
#define SYNCHRONY_MAX_WINDOWS 3                 // Multiple time scales

// ============================================================================
// TYPES
// ============================================================================

/**
 * @brief Synchrony detection result
 *
 * WHAT: Complete synchrony analysis for a time window
 * WHY:  Provide comprehensive synchrony metrics for downstream processing
 * HOW:  Combine multiple synchrony measures and statistics
 */
typedef struct {
    float synchrony_index;        // Overall synchrony [0.0, 1.0]
    float coincidence_rate;       // Spike coincidence rate [0.0, 1.0]
    float mean_correlation;       // Mean pairwise correlation
    uint32_t critical_events;     // Number of critical events detected
    bool is_synchronized;         // Above high threshold (>0.7)
    bool is_critical_event;       // Above critical threshold (>0.5)
    uint32_t neurons_firing;      // Number of neurons that fired
    uint32_t total_neurons;       // Total neurons analyzed
    float window_duration_ms;     // Analysis window duration
} synchrony_result_t;

/**
 * @brief Synchrony detector configuration
 */
typedef struct {
    uint32_t num_neurons;                          // Number of neurons to monitor
    float window_sizes_ms[SYNCHRONY_MAX_WINDOWS];  // Multiple time scales
    uint32_t num_windows;                          // Number of windows
    float coincidence_window_ms;                   // Spike coincidence window
    float critical_threshold;                      // Critical event threshold
    float high_threshold;                          // High synchrony threshold
    bool enable_correlation;                       // Compute cross-correlation
    bool enable_coincidence;                       // Compute spike coincidence
    bool enable_critical_detection;                // Detect critical events
} synchrony_detector_config_t;

/**
 * @brief Opaque synchrony detector handle
 */
typedef struct synchrony_detector synchrony_detector_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Create synchrony detector with configuration
 *
 * WHAT: Initialize synchrony detection system
 * WHY:  Set up data structures for multi-scale synchrony analysis
 * HOW:  Allocate spike buffers, correlation matrices, and sliding windows
 *
 * @param config Detector configuration (NULL for defaults)
 * @return Detector handle or NULL on failure
 */
synchrony_detector_t* synchrony_detector_create(const synchrony_detector_config_t* config);

/**
 * @brief Destroy synchrony detector and free resources
 */
void synchrony_detector_destroy(synchrony_detector_t* detector);

/**
 * @brief Add spike event for analysis
 *
 * WHAT: Record neuron spike at specific time
 * WHY:  Build temporal spike pattern for synchrony analysis
 * HOW:  Add to sliding window buffers, update running statistics
 *
 * @param detector Detector handle
 * @param neuron_id Neuron identifier [0, num_neurons)
 * @param timestamp_ms Spike time in milliseconds
 * @return true on success, false on error
 */
bool synchrony_detector_add_spike(synchrony_detector_t* detector,
                                   uint32_t neuron_id,
                                   double timestamp_ms);

/**
 * @brief Detect synchrony in current window
 *
 * WHAT: Analyze recent spikes for synchronous activity
 * WHY:  Identify coordinated neural processing and critical events
 * HOW:  Compute cross-correlation, coincidence rate, and synchrony index
 *
 * @param detector Detector handle
 * @param window_idx Window index (0 = shortest scale)
 * @param result Output synchrony analysis (required)
 * @return true on success, false on error
 */
bool synchrony_detector_detect(synchrony_detector_t* detector,
                                uint32_t window_idx,
                                synchrony_result_t* result);

/**
 * @brief Reset detector state
 *
 * WHAT: Clear all spike history
 * WHY:  Start fresh analysis after task change or reset
 * HOW:  Zero buffers, reset timestamps, clear statistics
 */
void synchrony_detector_reset(synchrony_detector_t* detector);

/**
 * @brief Get detector statistics
 *
 * @param detector Detector handle
 * @param total_spikes Output: total spikes processed
 * @param total_critical_events Output: total critical events detected
 * @param mean_synchrony Output: mean synchrony index over time
 * @return true on success, false on error
 */
bool synchrony_detector_get_stats(const synchrony_detector_t* detector,
                                   uint64_t* total_spikes,
                                   uint64_t* total_critical_events,
                                   float* mean_synchrony);

/**
 * @brief Compute pairwise correlation between two neurons
 *
 * WHAT: Calculate temporal correlation of spike trains
 * WHY:  Measure synchrony strength between neuron pairs
 * HOW:  Cross-correlation of spike times within window
 *
 * @param detector Detector handle
 * @param neuron_a First neuron ID
 * @param neuron_b Second neuron ID
 * @param window_ms Time window for correlation
 * @return Correlation coefficient [-1.0, 1.0] or 0.0 on error
 */
float synchrony_detector_compute_correlation(const synchrony_detector_t* detector,
                                              uint32_t neuron_a,
                                              uint32_t neuron_b,
                                              float window_ms);

/**
 * @brief Get default configuration
 */
synchrony_detector_config_t synchrony_detector_default_config(uint32_t num_neurons);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYNCHRONY_DETECTOR_H
