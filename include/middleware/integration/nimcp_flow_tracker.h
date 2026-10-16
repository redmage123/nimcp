//=============================================================================
// nimcp_flow_tracker.h - Cross-Modal Information Flow Tracking
//=============================================================================
/**
 * @file nimcp_flow_tracker.h
 * @brief Cross-modal information flow tracking for cognitive-middleware integration
 *
 * WHAT: Monitor information flow between middleware and cognitive layers
 * WHY:  Understand and optimize layer integration efficiency
 * HOW:  Tracks bits/sec, latency, and efficiency η = I_out / I_in for each path
 *
 * DESIGN PATTERNS:
 * - Observer: Monitors multi-path information flow
 * - Facade: Simplified interface to complex flow analysis
 * - Strategy: Pluggable flow measurement strategies
 *
 * PERFORMANCE:
 * - Flow recording: O(1) per event
 * - Efficiency calculation: O(1) per path
 * - Total overhead: ~1-2µs per event
 *
 * MEMORY:
 * - Per-path statistics: ~400 bytes × 5 paths = ~2KB
 * - Latency histograms: ~256 bytes × 5 paths = ~1.2KB
 * - Total: ~3.2KB
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0 (Phase 1.5.1)
 */

#ifndef NIMCP_FLOW_TRACKER_H
#define NIMCP_FLOW_TRACKER_H

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
 * @brief Number of integration paths to track
 *
 * WHAT: Paths between middleware and cognitive modules
 * WHY:  Track each information flow path separately
 * VALUE: 5 main paths (see integration_path_t)
 */
#define FLOW_TRACKER_NUM_PATHS 5

/**
 * @brief Latency histogram bin count
 *
 * WHAT: Number of bins for latency distribution
 * WHY:  Track latency distribution for p50, p90, p99
 * VALUE: 32 bins (log scale: 1µs - 1ms)
 */
#define FLOW_TRACKER_LATENCY_BINS 32

/**
 * @brief Measurement window duration (milliseconds)
 *
 * WHAT: Time window for rate calculations
 * WHY:  Smooth out transient spikes
 * VALUE: 1000ms (1 second window)
 */
#define FLOW_TRACKER_MEASUREMENT_WINDOW_MS 1000

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Integration path enumeration
 *
 * WHAT: All information flow paths between layers
 * WHY:  Track each path separately for optimization
 * HOW:  Enum of source → destination paths
 */
typedef enum {
    PATH_MIDDLEWARE_TO_EXECUTIVE = 0,       /**< Middleware → Executive */
    PATH_MIDDLEWARE_TO_WORKSPACE = 1,       /**< Middleware → Global Workspace */
    PATH_MIDDLEWARE_TO_INTROSPECTION = 2,   /**< Middleware → Introspection */
    PATH_EXECUTIVE_TO_MIDDLEWARE = 3,       /**< Executive → Middleware (commands) */
    PATH_WORKSPACE_TO_MIDDLEWARE = 4,       /**< Workspace → Middleware (broadcasts) */

    PATH_COUNT = 5                          /**< Total path count */
} integration_path_t;

/**
 * @brief Per-path flow statistics
 *
 * WHAT: Information flow metrics for a single path
 * WHY:  Track efficiency, bottlenecks, latency per path
 * HOW:  Accumulate statistics over measurement window
 */
typedef struct {
    // Flow rates (bits/second)
    float input_rate_bits_per_sec;          /**< Input information rate */
    float output_rate_bits_per_sec;         /**< Output information rate */
    float throughput_bits_per_sec;          /**< Actual throughput */

    // Efficiency metrics
    float flow_efficiency;                  /**< η = I_out / I_in [0-1] */
    float bottleneck_severity;              /**< 0.0 = none, 1.0 = severe */

    // Channel capacity
    float channel_capacity_bits_per_sec;    /**< Max sustainable rate */
    float capacity_utilization;             /**< Current / max [0-1] */

    // Latency tracking (microseconds)
    float avg_latency_us;                   /**< Average latency */
    float min_latency_us;                   /**< Minimum observed */
    float max_latency_us;                   /**< Maximum observed */
    float p50_latency_us;                   /**< 50th percentile */
    float p90_latency_us;                   /**< 90th percentile */
    float p99_latency_us;                   /**< 99th percentile */
    float stddev_latency_us;                /**< Standard deviation */

    // Event counts
    uint64_t total_events;                  /**< Total events on this path */
    uint64_t filtered_events;               /**< Events filtered out */
    uint64_t bottlenecked_events;           /**< Events dropped due to overload */

    // Information loss
    float information_loss_bits;            /**< Total bits lost */
    float loss_percentage;                  /**< Loss / input × 100 */

    // Timing
    uint64_t measurement_window_start_ms;   /**< Window start time */
    uint64_t last_update_time_ms;           /**< Last update */
} path_flow_stats_t;

/**
 * @brief Cross-modal flow metrics (all paths)
 *
 * WHAT: Information flow metrics for all integration paths
 * WHY:  Provide complete view of layer integration
 * HOW:  Array of per-path statistics
 */
typedef struct {
    // Per-path statistics
    path_flow_stats_t paths[FLOW_TRACKER_NUM_PATHS];

    // Global metrics
    float total_throughput_bits_per_sec;    /**< Sum across all paths */
    float avg_flow_efficiency;              /**< Average across paths */
    float worst_path_efficiency;            /**< Minimum efficiency */
    integration_path_t worst_path;          /**< Which path is worst */

    // Global bottleneck status
    bool any_bottleneck_detected;           /**< Any path bottlenecked? */
    uint32_t num_bottlenecked_paths;        /**< Count of bottlenecked paths */

    // Timing
    uint64_t measurement_window_ms;         /**< Window duration */
    uint64_t last_global_update_ms;         /**< Last global update */
} cross_modal_flow_metrics_t;

/**
 * @brief Flow tracker configuration
 */
typedef struct {
    uint64_t measurement_window_ms;         /**< Measurement window duration */
    uint32_t latency_histogram_bins;        /**< Number of latency bins */
    float efficiency_warning_threshold;     /**< Warn if η < threshold */
    float bottleneck_threshold;             /**< Bottleneck if utilization > threshold */
    bool enable_latency_tracking;           /**< Track detailed latency stats */
} flow_tracker_config_t;

/**
 * @brief Opaque flow tracker handle
 */
typedef struct flow_tracker flow_tracker_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create flow tracker with default configuration
 *
 * WHAT: Initialize cross-modal flow tracking
 * WHY:  Monitor information flow between layers
 * HOW:  Allocates per-path statistics and histograms
 *
 * @return Tracker handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 * MALLOC: Yes (tracker structure + histograms)
 */
flow_tracker_t* flow_tracker_create(void);

/**
 * @brief Create flow tracker with custom configuration
 *
 * @param config Custom configuration
 * @return Tracker handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 */
flow_tracker_t* flow_tracker_create_custom(
    const flow_tracker_config_t* config
);

/**
 * @brief Destroy flow tracker
 *
 * @param tracker Tracker to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (destruction)
 */
void flow_tracker_destroy(flow_tracker_t* tracker);

//=============================================================================
// Flow Recording API
//=============================================================================

/**
 * @brief Record information flow on a path
 *
 * WHAT: Track information transfer from source to destination
 * WHY:  Build statistics for efficiency calculation
 * HOW:  Updates flow rates, latency, efficiency for path
 *
 * @param tracker Flow tracker
 * @param path Which path (middleware→cognitive or vice versa)
 * @param information_bits Information content transferred
 * @param latency_us Latency of transfer (optional, 0 = don't track)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected per path)
 * LATENCY: ~1-2µs
 */
void flow_tracker_record_flow(
    flow_tracker_t* tracker,
    integration_path_t path,
    float information_bits,
    uint64_t latency_us
);

/**
 * @brief Record filtered flow (information loss)
 *
 * WHAT: Track information that was filtered out on a path
 * WHY:  Measure information loss and efficiency degradation
 * HOW:  Updates loss statistics without affecting throughput
 *
 * @param tracker Flow tracker
 * @param path Which path
 * @param information_bits Information content that was filtered
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void flow_tracker_record_filtered_flow(
    flow_tracker_t* tracker,
    integration_path_t path,
    float information_bits
);

/**
 * @brief Record bottlenecked flow (dropped due to overload)
 *
 * @param tracker Flow tracker
 * @param path Which path
 * @param information_bits Information content that was dropped
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void flow_tracker_record_bottlenecked_flow(
    flow_tracker_t* tracker,
    integration_path_t path,
    float information_bits
);

//=============================================================================
// Metrics Access API
//=============================================================================

/**
 * @brief Get cross-modal flow metrics for all paths
 *
 * @param tracker Flow tracker
 * @return Flow metrics snapshot (copy)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (returns copy)
 */
cross_modal_flow_metrics_t flow_tracker_get_metrics(
    const flow_tracker_t* tracker
);

/**
 * @brief Get flow statistics for a specific path
 *
 * @param tracker Flow tracker
 * @param path Which path
 * @return Path statistics snapshot (copy)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (returns copy)
 */
path_flow_stats_t flow_tracker_get_path_stats(
    const flow_tracker_t* tracker,
    integration_path_t path
);

/**
 * @brief Calculate flow efficiency for a path
 *
 * WHAT: Measure how much input information reaches output
 * WHY:  Identify lossy paths that need optimization
 * HOW:  η = I_out / I_in = output_rate / input_rate
 *
 * FORMULA:
 *   η = (information_delivered) / (information_input)
 *   Perfect efficiency: η = 1.0 (no loss)
 *   Total loss: η = 0.0 (nothing gets through)
 *
 * @param tracker Flow tracker
 * @param path Which path
 * @return Efficiency [0.0-1.0] (1.0 = perfect, 0.0 = total loss)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float flow_tracker_calculate_efficiency(
    const flow_tracker_t* tracker,
    integration_path_t path
);

/**
 * @brief Get throughput for a path
 *
 * @param tracker Flow tracker
 * @param path Which path
 * @return Throughput in bits/second
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float flow_tracker_get_throughput(
    const flow_tracker_t* tracker,
    integration_path_t path
);

/**
 * @brief Get capacity utilization for a path
 *
 * @param tracker Flow tracker
 * @param path Which path
 * @return Utilization [0-1] (0 = idle, 1 = saturated)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float flow_tracker_get_utilization(
    const flow_tracker_t* tracker,
    integration_path_t path
);

/**
 * @brief Get average latency for a path
 *
 * @param tracker Flow tracker
 * @param path Which path
 * @return Average latency in microseconds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only access)
 */
float flow_tracker_get_avg_latency(
    const flow_tracker_t* tracker,
    integration_path_t path
);

/**
 * @brief Get p99 latency for a path
 *
 * WHAT: 99th percentile latency
 * WHY:  Measure tail latency (worst 1% of events)
 * HOW:  Computed from latency histogram
 *
 * @param tracker Flow tracker
 * @param path Which path
 * @return P99 latency in microseconds
 *
 * COMPLEXITY: O(1) (cached from histogram)
 * THREAD-SAFE: Yes (read-only access)
 */
float flow_tracker_get_p99_latency(
    const flow_tracker_t* tracker,
    integration_path_t path
);

//=============================================================================
// Analysis API
//=============================================================================

/**
 * @brief Identify bottleneck path
 *
 * WHAT: Find which path has worst efficiency/utilization
 * WHY:  Focus optimization efforts on bottleneck
 * HOW:  Compares efficiency across all paths
 *
 * @param tracker Flow tracker
 * @param[out] efficiency Bottleneck path's efficiency (optional)
 * @return Path with worst efficiency
 *
 * COMPLEXITY: O(n) where n = number of paths (5)
 * THREAD-SAFE: Yes (read-only access)
 */
integration_path_t flow_tracker_find_bottleneck(
    const flow_tracker_t* tracker,
    float* efficiency
);

/**
 * @brief Check if any path is bottlenecked
 *
 * @param tracker Flow tracker
 * @return true if any path has utilization > threshold
 *
 * COMPLEXITY: O(n) where n = number of paths (5)
 * THREAD-SAFE: Yes (read-only access)
 */
bool flow_tracker_has_bottleneck(
    const flow_tracker_t* tracker
);

/**
 * @brief Get total throughput across all paths
 *
 * @param tracker Flow tracker
 * @return Total throughput in bits/second
 *
 * COMPLEXITY: O(n) where n = number of paths (5)
 * THREAD-SAFE: Yes (read-only access)
 */
float flow_tracker_get_total_throughput(
    const flow_tracker_t* tracker
);

/**
 * @brief Get average efficiency across all paths
 *
 * @param tracker Flow tracker
 * @return Average efficiency [0-1]
 *
 * COMPLEXITY: O(n) where n = number of paths (5)
 * THREAD-SAFE: Yes (read-only access)
 */
float flow_tracker_get_avg_efficiency(
    const flow_tracker_t* tracker
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Reset flow tracker statistics
 *
 * WHAT: Clear all statistics and restart measurement
 * WHY:  Start fresh measurement period
 * HOW:  Zeros out all path statistics
 *
 * @param tracker Flow tracker
 *
 * COMPLEXITY: O(n) where n = number of paths
 * THREAD-SAFE: Yes (mutex protected)
 */
void flow_tracker_reset(
    flow_tracker_t* tracker
);

/**
 * @brief Get default flow tracker configuration
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
flow_tracker_config_t flow_tracker_default_config(void);

/**
 * @brief Convert path enum to string (for logging)
 *
 * @param path Path to convert
 * @return String representation
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
const char* flow_tracker_path_to_string(
    integration_path_t path
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FLOW_TRACKER_H
