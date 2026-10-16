//=============================================================================
// nimcp_shannon_monitor.h - Shannon Information Theory Monitoring for Event Routing
//=============================================================================
/**
 * @file nimcp_shannon_monitor.h
 * @brief Shannon information theory monitoring for cognitive-middleware integration
 *
 * WHAT: Real-time channel capacity, bottleneck detection, and information loss tracking
 * WHY:  Ensure optimal information flow between middleware and cognitive layers
 * HOW:  Computes H(X), H(Y), I(X;Y), channel capacity C = B log₂(1 + SNR)
 *
 * DESIGN PATTERNS:
 * - Observer: Monitors event flow statistics
 * - Strategy: Pluggable entropy calculation strategies
 * - Memento: Maintains event history for entropy calculation
 *
 * PERFORMANCE:
 * - Entropy calculation: O(n log n) where n = history size
 * - Channel capacity: O(1) computation
 * - Bottleneck detection: O(1) threshold check
 * - Total overhead: ~2-5µs per event
 *
 * MEMORY:
 * - Event history ring buffer: ~4KB (configurable)
 * - Statistics tracking: ~512 bytes
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0 (Phase 1.5.1)
 */

#ifndef NIMCP_SHANNON_MONITOR_H
#define NIMCP_SHANNON_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include "middleware/events/nimcp_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/**
 * @brief Default event history size for entropy calculation
 *
 * WHAT: Number of recent events to track
 * WHY:  Larger = more accurate entropy, but more memory
 * VALUE: 256 events = ~4KB (assuming 16 bytes per event)
 */
#define SHANNON_MONITOR_DEFAULT_HISTORY_SIZE 256

/**
 * @brief Default channel capacity bandwidth (events/sec)
 *
 * WHAT: Base bandwidth for capacity calculation
 * WHY:  C = B log₂(1 + SNR), need B estimate
 * VALUE: 10000 events/sec (typical cognitive processing rate)
 */
#define SHANNON_MONITOR_DEFAULT_BANDWIDTH 10000.0f

/**
 * @brief Default bottleneck threshold (utilization)
 *
 * WHAT: Channel utilization above which bottleneck is detected
 * WHY:  0.8 = approaching capacity, need to filter
 * VALUE: 0.8 (80% utilization)
 */
#define SHANNON_MONITOR_DEFAULT_BOTTLENECK_THRESHOLD 0.8f

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Shannon routing metrics
 *
 * WHAT: Information-theoretic metrics for event routing
 * WHY:  Provide insight into channel health and efficiency
 * HOW:  Computed from event history using Shannon formulas
 */
typedef struct {
    // Channel capacity monitoring
    float channel_capacity_bits_per_sec;   /**< C = B log₂(1 + SNR) */
    float current_throughput;               /**< Actual bits/sec */
    float capacity_utilization;             /**< throughput / capacity [0-1] */

    // Bottleneck detection
    bool bottleneck_detected;               /**< Is channel overloaded? */
    float bottleneck_severity;              /**< Severity [0-1] */
    uint32_t bottleneck_module;             /**< Which module is bottlenecked */

    // Information loss tracking
    float information_loss_rate;            /**< Bits/sec lost to filtering */
    float filtered_bits_per_sec;            /**< Information dropped */
    float loss_percentage;                  /**< Loss / total input [0-100] */

    // Entropy measurements (bits)
    float event_entropy;                    /**< H(events) */
    float cognitive_response_entropy;       /**< H(responses) */
    float mutual_information;               /**< I(events;responses) */

    // Statistics
    uint64_t total_events;                  /**< Total events processed */
    uint64_t filtered_events;               /**< Events filtered */
    uint64_t bottlenecked_events;           /**< Events dropped due to overload */

    // Timing
    uint64_t measurement_window_ms;         /**< Time window for rate calculations */
    uint64_t last_update_time_ms;           /**< Last metrics update */
} shannon_routing_metrics_t;

/**
 * @brief Shannon monitor configuration
 */
typedef struct {
    uint32_t history_size;                  /**< Event history ring buffer size */
    float bandwidth_events_per_sec;         /**< Base bandwidth for C calculation */
    float bottleneck_threshold;             /**< Utilization threshold [0-1] */
    float signal_to_noise_ratio;            /**< SNR for capacity calculation */
    uint64_t measurement_window_ms;         /**< Time window for rate calculations */
    bool enable_adaptive_snr;               /**< Adapt SNR based on observed data */
} shannon_monitor_config_t;

/**
 * @brief Opaque Shannon monitor handle
 */
typedef struct shannon_monitor shannon_monitor_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Shannon monitor with default configuration
 *
 * WHAT: Initialize Shannon information monitoring
 * WHY:  Enable bottleneck detection and adaptive filtering
 * HOW:  Allocates event history buffer and statistics tracking
 *
 * @return Monitor handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 * MALLOC: Yes (monitor structure + event history buffer)
 */
shannon_monitor_t* shannon_monitor_create(void);

/**
 * @brief Create Shannon monitor with custom configuration
 *
 * @param config Custom configuration
 * @return Monitor handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 */
shannon_monitor_t* shannon_monitor_create_custom(
    const shannon_monitor_config_t* config
);

/**
 * @brief Destroy Shannon monitor
 *
 * @param monitor Monitor to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (destruction)
 */
void shannon_monitor_destroy(shannon_monitor_t* monitor);

//=============================================================================
// Event Tracking API
//=============================================================================

/**
 * @brief Record event for Shannon analysis
 *
 * WHAT: Add event to history for entropy/capacity calculation
 * WHY:  Build statistical model of event distribution
 * HOW:  Updates ring buffer, recomputes entropy if needed
 *
 * @param monitor Shannon monitor
 * @param event Event to record
 *
 * COMPLEXITY: O(1) amortized (entropy recalc every N events)
 * THREAD-SAFE: Yes (mutex protected)
 * LATENCY: ~1-2µs typical, ~50µs on entropy recalculation
 */
void shannon_monitor_record_event(
    shannon_monitor_t* monitor,
    const event_t* event
);

/**
 * @brief Record filtered event (dropped due to low information)
 *
 * WHAT: Track events that were filtered out
 * WHY:  Measure information loss
 * HOW:  Updates filtered event counter and loss statistics
 *
 * @param monitor Shannon monitor
 * @param event Event that was filtered
 * @param information_bits Information content of filtered event
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void shannon_monitor_record_filtered_event(
    shannon_monitor_t* monitor,
    const event_t* event,
    float information_bits
);

/**
 * @brief Record cognitive response to event
 *
 * WHAT: Track cognitive module's response to event
 * WHY:  Compute mutual information I(events;responses)
 * HOW:  Updates response history for entropy calculation
 *
 * @param monitor Shannon monitor
 * @param event Original event
 * @param response_type Type of cognitive response
 *
 * COMPLEXITY: O(1) amortized
 * THREAD-SAFE: Yes (mutex protected)
 */
void shannon_monitor_record_response(
    shannon_monitor_t* monitor,
    const event_t* event,
    uint32_t response_type
);

//=============================================================================
// Information Measurement API
//=============================================================================

/**
 * @brief Calculate information content of an event
 *
 * WHAT: Measure how many bits of information an event carries
 * WHY:  Filter low-information events to save processing
 * HOW:  I = -log₂(P(event)) where P is probability from history
 *
 * FORMULA: I(event) = -log₂(P(event_type, event_source, ...))
 *
 * @param monitor Shannon monitor
 * @param event Event to measure
 * @return Information content in bits
 *
 * COMPLEXITY: O(1) hash lookup
 * THREAD-SAFE: Yes (read-only access)
 * RANGE: [0, ~12] bits (0 = very common, 12 = very rare)
 *
 * EXAMPLE:
 *   Event occurs 50% of time → I = -log₂(0.5) = 1 bit
 *   Event occurs 1% of time → I = -log₂(0.01) = 6.64 bits
 *   Event occurs 0.1% of time → I = -log₂(0.001) = 9.97 bits
 */
float shannon_monitor_measure_event_information(
    const shannon_monitor_t* monitor,
    const event_t* event
);

/**
 * @brief Calculate channel capacity
 *
 * WHAT: Compute maximum sustainable event rate
 * WHY:  Prevent cognitive module overload
 * HOW:  C = B log₂(1 + SNR) where B = bandwidth, SNR = signal-to-noise
 *
 * FORMULA: C = B × log₂(1 + S/N) bits/second
 *   where:
 *     B = bandwidth (events/sec)
 *     S = signal power (average event information)
 *     N = noise power (entropy of filtered events)
 *
 * @param monitor Shannon monitor
 * @return Channel capacity in bits/second
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float shannon_monitor_calculate_channel_capacity(
    const shannon_monitor_t* monitor
);

/**
 * @brief Get current throughput
 *
 * WHAT: Measure actual information flow rate
 * WHY:  Compare against channel capacity
 * HOW:  Sum of event information over measurement window
 *
 * @param monitor Shannon monitor
 * @return Throughput in bits/second
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float shannon_monitor_get_throughput(
    const shannon_monitor_t* monitor
);

/**
 * @brief Get capacity utilization
 *
 * WHAT: Fraction of channel capacity being used
 * WHY:  Detect approaching saturation
 * HOW:  utilization = throughput / capacity
 *
 * @param monitor Shannon monitor
 * @return Utilization [0-1] (0 = idle, 1 = saturated)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float shannon_monitor_get_utilization(
    const shannon_monitor_t* monitor
);

//=============================================================================
// Bottleneck Detection API
//=============================================================================

/**
 * @brief Detect information bottleneck
 *
 * WHAT: Identify if channel is overloaded
 * WHY:  Prevent information loss and latency spikes
 * HOW:  Checks if utilization > threshold
 *
 * ALGORITHM:
 *   if (throughput / capacity > threshold):
 *     bottleneck = true
 *     severity = (utilization - threshold) / (1.0 - threshold)
 *
 * @param monitor Shannon monitor
 * @param[out] bottleneck_module Which module is bottlenecked (optional)
 * @return Bottleneck severity [0-1] (0 = none, 1 = severe)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float shannon_monitor_detect_bottleneck(
    const shannon_monitor_t* monitor,
    uint32_t* bottleneck_module
);

/**
 * @brief Check if bottleneck is detected
 *
 * @param monitor Shannon monitor
 * @return true if bottleneck detected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
bool shannon_monitor_is_bottlenecked(
    const shannon_monitor_t* monitor
);

//=============================================================================
// Metrics Access API
//=============================================================================

/**
 * @brief Get current Shannon routing metrics
 *
 * @param monitor Shannon monitor
 * @return Metrics snapshot (copy)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (returns copy)
 */
shannon_routing_metrics_t shannon_monitor_get_metrics(
    const shannon_monitor_t* monitor
);

/**
 * @brief Get event entropy
 *
 * WHAT: Measure diversity of event distribution
 * WHY:  High entropy = unpredictable, low = predictable
 * HOW:  H(X) = -Σ P(x) log₂ P(x)
 *
 * @param monitor Shannon monitor
 * @return Entropy in bits
 *
 * COMPLEXITY: O(1) (cached)
 * THREAD-SAFE: Yes (read-only access)
 * RANGE: [0, log₂(num_event_types)] bits
 */
float shannon_monitor_get_event_entropy(
    const shannon_monitor_t* monitor
);

/**
 * @brief Get mutual information between events and responses
 *
 * WHAT: Measure how much event predicts response
 * WHY:  High I(X;Y) = events inform responses effectively
 * HOW:  I(X;Y) = H(X) + H(Y) - H(X,Y)
 *
 * @param monitor Shannon monitor
 * @return Mutual information in bits
 *
 * COMPLEXITY: O(1) (cached)
 * THREAD-SAFE: Yes (read-only access)
 * RANGE: [0, min(H(X), H(Y))] bits
 */
float shannon_monitor_get_mutual_information(
    const shannon_monitor_t* monitor
);

/**
 * @brief Get information loss percentage
 *
 * WHAT: Percentage of input information lost to filtering
 * WHY:  Measure cost of adaptive filtering
 * HOW:  loss% = (filtered_bits / total_bits) × 100
 *
 * @param monitor Shannon monitor
 * @return Loss percentage [0-100]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float shannon_monitor_get_information_loss_percentage(
    const shannon_monitor_t* monitor
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set signal-to-noise ratio
 *
 * WHAT: Configure SNR for channel capacity calculation
 * WHY:  Different workloads have different noise characteristics
 * HOW:  Updates SNR parameter in C = B log₂(1 + SNR)
 *
 * @param monitor Shannon monitor
 * @param snr Signal-to-noise ratio (typical: 10-100)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void shannon_monitor_set_snr(
    shannon_monitor_t* monitor,
    float snr
);

/**
 * @brief Set bottleneck threshold
 *
 * @param monitor Shannon monitor
 * @param threshold Utilization threshold [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void shannon_monitor_set_bottleneck_threshold(
    shannon_monitor_t* monitor,
    float threshold
);

/**
 * @brief Enable adaptive SNR estimation
 *
 * WHAT: Automatically estimate SNR from observed data
 * WHY:  Adapt to changing noise characteristics
 * HOW:  SNR ≈ (signal_power / noise_power) from statistics
 *
 * @param monitor Shannon monitor
 * @param enable true to enable, false to disable
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void shannon_monitor_enable_adaptive_snr(
    shannon_monitor_t* monitor,
    bool enable
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Reset monitor statistics
 *
 * WHAT: Clear event history and reset counters
 * WHY:  Start fresh measurement period
 * HOW:  Zeros out all statistics and history
 *
 * @param monitor Shannon monitor
 *
 * COMPLEXITY: O(n) where n = history size
 * THREAD-SAFE: Yes (mutex protected)
 */
void shannon_monitor_reset(
    shannon_monitor_t* monitor
);

/**
 * @brief Get default Shannon monitor configuration
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
shannon_monitor_config_t shannon_monitor_default_config(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SHANNON_MONITOR_H
