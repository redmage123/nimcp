/**
 * @file nimcp_global_workspace_shannon.h
 * @brief Shannon information theory integration for Global Workspace
 *
 * WHAT: Shannon-monitored workspace competition and broadcast
 * WHY:  Information content determines workspace access and prevents subscriber overload
 * HOW:  Information-weighted competition + channel capacity monitoring per subscriber
 *
 * PHASE: 1.5.3 - Global Workspace Integration + Information Competition
 *
 * KEY ENHANCEMENTS:
 * 1. Competition strength = salience × information_content
 *    - High-information, salient events win workspace access
 * 2. Shannon-monitored broadcasts
 *    - Track information delivery to each subscriber
 *    - Detect bottlenecks (subscriber overload)
 * 3. Adaptive broadcast rate
 *    - Reduce rate when bottlenecks detected
 *    - Prevents information loss
 *
 * MATHEMATICAL FOUNDATION:
 * - Information content: H(X) = -Σ p(x) log₂ p(x)
 * - Mutual information: I(X;Y) = H(X) - H(X|Y)
 * - Channel capacity: C = max I(X;Y)
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 * @version 1.0.0 Phase 1.5.3
 */

#ifndef NIMCP_GLOBAL_WORKSPACE_SHANNON_H
#define NIMCP_GLOBAL_WORKSPACE_SHANNON_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/global_workspace/nimcp_global_workspace.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/**
 * @brief Default information threshold for competition (bits)
 *
 * WHAT: Minimum information content to participate in competition
 * WHY:  Filter out low-information noise
 * VALUE: 2.0 bits (equivalent to distinguishing 4 equally-likely events)
 */
#define GWS_DEFAULT_INFO_THRESHOLD_BITS 2.0f

/**
 * @brief Default subscriber capacity (bits/sec)
 *
 * WHAT: Default processing capacity per subscriber
 * WHY:  Estimate how much information a subscriber can handle
 * VALUE: 100 bits/sec (conservative estimate)
 */
#define GWS_DEFAULT_SUBSCRIBER_CAPACITY 100.0f

/**
 * @brief Bottleneck utilization threshold
 *
 * WHAT: Above this utilization, subscriber is considered bottlenecked
 * WHY:  Trigger adaptive rate reduction
 * VALUE: 0.9 (90% capacity)
 */
#define GWS_BOTTLENECK_THRESHOLD 0.9f

/**
 * @brief Broadcast rate reduction factor on bottleneck
 *
 * WHAT: Multiply rate by this when bottleneck detected
 * WHY:  Reduce load on overloaded subscribers
 * VALUE: 0.5 (50% reduction)
 */
#define GWS_BOTTLENECK_RATE_REDUCTION 0.5f

/**
 * @brief Broadcast rate recovery factor
 *
 * WHAT: Multiply rate by this when no bottleneck (recovery)
 * WHY:  Gradually restore normal rate
 * VALUE: 1.1 (10% increase per period)
 */
#define GWS_RATE_RECOVERY_FACTOR 1.1f

/**
 * @brief Information normalization factor
 *
 * WHAT: Scale information content for competition strength calculation
 * WHY:  Map bits to [0,1] range for combination with salience
 * VALUE: 10.0 bits (assumes max ~10 bits of useful information)
 */
#define GWS_INFO_NORMALIZATION_BITS 10.0f

//=============================================================================
// Shannon Broadcast Metrics
//=============================================================================

/**
 * @brief Shannon broadcast metrics per-broadcast
 *
 * WHAT: Information delivery and loss tracking for a single broadcast
 * WHY:  Monitor information flow to subscribers, detect bottlenecks
 * HOW:  Populated during broadcast, analyzed for adaptation
 */
typedef struct {
    /* Bottleneck detection */
    bool bottleneck_detected;               /**< Any subscriber bottlenecked? */
    cognitive_module_t bottlenecked_module; /**< First bottlenecked subscriber */
    uint32_t num_bottlenecked;              /**< Total bottlenecked subscribers */

    /* Per-subscriber metrics */
    float information_delivered[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];  /**< Bits delivered */
    float information_loss[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];       /**< Bits lost (overload) */
    float subscriber_utilization[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS]; /**< Load/capacity */

    /* Aggregate metrics */
    float total_info_delivered;             /**< Sum of delivered bits */
    float total_info_loss;                  /**< Sum of lost bits */
    float delivery_efficiency;              /**< delivered / (delivered + loss) */

    /* Broadcast metadata */
    float content_info_bits;                /**< Information content of broadcast */
    uint32_t num_subscribers;               /**< Subscribers at broadcast time */
    uint64_t broadcast_timestamp_ms;        /**< When broadcast occurred */

} shannon_broadcast_metrics_t;

/**
 * @brief Shannon workspace configuration
 *
 * WHAT: Configuration for Shannon-enhanced workspace features
 * WHY:  Tune information-theoretic parameters
 * HOW:  Passed to enable functions or set during workspace creation
 */
typedef struct {
    /* Competition enhancement */
    bool enable_info_weighted_competition;  /**< Use info content in competition? */
    float info_threshold_bits;              /**< Min info to compete */
    float info_weight;                      /**< Weight of info vs salience (0-1) */

    /* Broadcast monitoring */
    bool enable_shannon_monitoring;         /**< Track info delivery? */
    float default_subscriber_capacity;      /**< Default bits/sec per subscriber */
    float bottleneck_threshold;             /**< Utilization threshold for bottleneck */

    /* Adaptive rate control */
    bool enable_adaptive_rate;              /**< Auto-adjust broadcast rate? */
    float rate_reduction_factor;            /**< Reduce by this on bottleneck */
    float rate_recovery_factor;             /**< Recover by this when clear */
    float min_broadcast_rate;               /**< Floor for rate reduction */
    float max_broadcast_rate;               /**< Ceiling for rate */

} shannon_workspace_config_t;

/**
 * @brief Shannon workspace state (extended statistics)
 *
 * WHAT: Accumulated Shannon metrics for workspace
 * WHY:  Track information flow over time
 * HOW:  Updated on each broadcast, queryable for analysis
 */
typedef struct {
    /* Competition metrics */
    uint64_t total_competitions_with_info;  /**< Competitions using info weighting */
    float avg_winner_info_bits;             /**< Average info content of winners */
    float avg_loser_info_bits;              /**< Average info content of losers */
    uint64_t low_info_rejections;           /**< Rejected due to low info content */

    /* Broadcast metrics */
    uint64_t total_shannon_broadcasts;      /**< Broadcasts with Shannon monitoring */
    float total_info_delivered_bits;        /**< Cumulative delivered */
    float total_info_lost_bits;             /**< Cumulative lost */
    float overall_delivery_efficiency;      /**< delivered / total */

    /* Bottleneck metrics */
    uint64_t bottleneck_events;             /**< Times bottleneck detected */
    uint64_t rate_reductions;               /**< Times rate was reduced */
    uint64_t rate_recoveries;               /**< Times rate recovered */
    float current_broadcast_rate;           /**< Current rate multiplier (0-1) */

    /* Per-subscriber statistics */
    float subscriber_total_delivered[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];
    float subscriber_total_lost[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];
    uint64_t subscriber_bottleneck_count[GLOBAL_WORKSPACE_MAX_SUBSCRIBERS];

} shannon_workspace_stats_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default Shannon workspace configuration
 *
 * WHAT: Sensible defaults for Shannon-enhanced workspace
 * WHY:  Convenient starting point
 * HOW:  Return struct with documented defaults
 *
 * @return Default configuration
 *
 * DEFAULTS:
 * - enable_info_weighted_competition = true
 * - info_threshold_bits = 2.0
 * - info_weight = 0.5 (equal weight to salience and info)
 * - enable_shannon_monitoring = true
 * - enable_adaptive_rate = true
 */
shannon_workspace_config_t shannon_workspace_default_config(void);

/**
 * @brief Enable Shannon features on existing workspace
 *
 * WHAT: Add Shannon monitoring to workspace without recreation
 * WHY:  Upgrade existing workspace
 * HOW:  Allocate Shannon state, set callbacks
 *
 * @param workspace Global workspace handle
 * @param config Shannon configuration (NULL for defaults)
 * @return true if enabled successfully
 *
 * COMPLEXITY: O(S) where S = subscribers
 * THREAD-SAFE: No
 */
bool global_workspace_enable_shannon(
    global_workspace_t* workspace,
    const shannon_workspace_config_t* config
);

/**
 * @brief Disable Shannon features
 *
 * WHAT: Remove Shannon monitoring from workspace
 * WHY:  Reduce overhead if not needed
 * HOW:  Free Shannon state, remove callbacks
 *
 * @param workspace Global workspace handle
 */
void global_workspace_disable_shannon(global_workspace_t* workspace);

/**
 * @brief Check if Shannon features are enabled
 *
 * @param workspace Global workspace handle
 * @return true if Shannon monitoring is active
 */
bool global_workspace_is_shannon_enabled(const global_workspace_t* workspace);

//=============================================================================
// Information Measurement
//=============================================================================

/**
 * @brief Measure information content of feature vector
 *
 * WHAT: Calculate Shannon entropy of feature vector
 * WHY:  Determine information content for competition weighting
 * HOW:  Treat normalized features as probability distribution, compute H(X)
 *
 * ALGORITHM:
 * 1. Normalize features to sum to 1 (treat as probabilities)
 * 2. H(X) = -Σ p(x) log₂ p(x)
 * 3. Handle zeros (0 log 0 = 0 by convention)
 *
 * @param features Feature vector
 * @param dim Dimensionality
 * @return Information content in bits
 *
 * COMPLEXITY: O(D) where D = dimensionality
 * RANGE: 0 to log₂(D) bits
 */
float shannon_measure_feature_information(
    const float* features,
    uint32_t dim
);

/**
 * @brief Measure relative information (KL divergence from uniform)
 *
 * WHAT: How much information relative to maximum entropy
 * WHY:  Normalized measure independent of dimensionality
 * HOW:  D_KL(P || U) where U is uniform distribution
 *
 * @param features Feature vector
 * @param dim Dimensionality
 * @return Relative information (0 = uniform, higher = more structured)
 *
 * COMPLEXITY: O(D)
 * RANGE: 0 to log₂(D)
 */
float shannon_measure_relative_information(
    const float* features,
    uint32_t dim
);

//=============================================================================
// Information-Weighted Competition
//=============================================================================

/**
 * @brief Compete for workspace with information weighting
 *
 * WHAT: Module competes with salience × information content
 * WHY:  High-information, salient events should win consciousness
 * HOW:  Compute info content, multiply with salience, call standard compete
 *
 * ALGORITHM:
 * 1. Measure info_bits = shannon_measure_feature_information(content, dim)
 * 2. competition_strength = salience * (info_bits / GWS_INFO_NORMALIZATION_BITS)
 * 3. Clamp to [0, 1]
 * 4. If info_bits < info_threshold, reject (return false)
 * 5. Call standard global_workspace_compete() with competition_strength
 *
 * @param workspace Global workspace handle
 * @param module Competing module
 * @param content Content to broadcast if wins
 * @param content_dim Content dimensionality
 * @param salience Raw salience score (0-1)
 * @param info_bits Output: measured information content (can be NULL)
 * @return true if won competition and broadcast
 *
 * COMPLEXITY: O(D + N) where D=dim, N=competitors
 * THREAD-SAFE: No
 */
bool global_workspace_compete_with_info(
    global_workspace_t* workspace,
    cognitive_module_t module,
    const float* content,
    uint32_t content_dim,
    float salience,
    float* info_bits
);

//=============================================================================
// Shannon-Monitored Broadcast
//=============================================================================

/**
 * @brief Broadcast with Shannon monitoring
 *
 * WHAT: Broadcast content to subscribers with channel capacity tracking
 * WHY:  Prevent subscriber overload, detect bottlenecks
 * HOW:  Check each subscriber's load vs capacity, reduce delivery if overloaded
 *
 * ALGORITHM (per subscriber):
 * 1. Get subscriber_capacity and subscriber_load
 * 2. utilization = load / capacity
 * 3. If utilization > BOTTLENECK_THRESHOLD:
 *    - Mark bottleneck detected
 *    - delivered = content_info_bits * (1 - utilization)
 *    - loss = content_info_bits - delivered
 * 4. Else:
 *    - delivered = content_info_bits
 *    - loss = 0
 * 5. Notify subscriber (standard notification)
 *
 * @param workspace Global workspace handle
 * @param content Content to broadcast
 * @param dim Content dimensionality
 * @param content_info_bits Information content of content
 * @return Broadcast metrics (bottleneck status, per-subscriber delivery)
 *
 * COMPLEXITY: O(S × D) where S=subscribers, D=dim
 * THREAD-SAFE: No
 */
shannon_broadcast_metrics_t global_workspace_broadcast_with_shannon(
    global_workspace_t* workspace,
    const float* content,
    uint32_t dim,
    float content_info_bits
);

//=============================================================================
// Subscriber Capacity Management
//=============================================================================

/**
 * @brief Set subscriber processing capacity
 *
 * WHAT: Define how much information a subscriber can process
 * WHY:  Enable bottleneck detection
 * HOW:  Store capacity, used in utilization calculation
 *
 * @param workspace Global workspace handle
 * @param subscriber Subscriber module
 * @param capacity_bits_per_sec Processing capacity in bits/sec
 * @return true if set successfully
 *
 * COMPLEXITY: O(S) where S = subscribers (to find subscriber)
 */
bool global_workspace_set_subscriber_capacity(
    global_workspace_t* workspace,
    cognitive_module_t subscriber,
    float capacity_bits_per_sec
);

/**
 * @brief Get subscriber processing capacity
 *
 * @param workspace Global workspace handle
 * @param subscriber Subscriber module
 * @return Capacity in bits/sec, or 0 if not found
 */
float global_workspace_get_subscriber_capacity(
    const global_workspace_t* workspace,
    cognitive_module_t subscriber
);

/**
 * @brief Get subscriber current load
 *
 * WHAT: Current information processing load
 * WHY:  Calculate utilization for bottleneck detection
 * HOW:  Track recent information delivered, compute rate
 *
 * @param workspace Global workspace handle
 * @param subscriber Subscriber module
 * @return Current load in bits/sec, or 0 if not found
 */
float global_workspace_get_subscriber_load(
    const global_workspace_t* workspace,
    cognitive_module_t subscriber
);

/**
 * @brief Update subscriber load (internal, called after broadcast)
 *
 * WHAT: Add delivered information to subscriber's load
 * WHY:  Track cumulative load for utilization
 * HOW:  Exponential moving average with decay
 *
 * @param workspace Global workspace handle
 * @param subscriber Subscriber module
 * @param delivered_bits Information just delivered
 */
void global_workspace_update_subscriber_load(
    global_workspace_t* workspace,
    cognitive_module_t subscriber,
    float delivered_bits
);

//=============================================================================
// Adaptive Broadcast Rate Control
//=============================================================================

/**
 * @brief Set broadcast rate multiplier
 *
 * WHAT: Scale broadcast frequency
 * WHY:  Reduce rate when bottlenecks detected
 * HOW:  Multiply normal rate by this factor
 *
 * @param workspace Global workspace handle
 * @param rate_multiplier Rate multiplier (0.1 to 1.0)
 * @return true if set successfully
 *
 * VALIDATION: Clamped to [min_rate, max_rate]
 */
bool global_workspace_set_broadcast_rate(
    global_workspace_t* workspace,
    float rate_multiplier
);

/**
 * @brief Get current broadcast rate multiplier
 *
 * @param workspace Global workspace handle
 * @return Current rate multiplier (0.1 to 1.0)
 */
float global_workspace_get_broadcast_rate(const global_workspace_t* workspace);

/**
 * @brief Adapt broadcast rate based on bottleneck status
 *
 * WHAT: Auto-adjust rate based on recent bottleneck history
 * WHY:  Self-regulating information flow
 * HOW:  Reduce on bottleneck, recover when clear
 *
 * ALGORITHM:
 * 1. If bottleneck_detected in recent broadcasts:
 *    - rate *= rate_reduction_factor
 *    - Clamp to min_rate
 * 2. Else:
 *    - rate *= rate_recovery_factor
 *    - Clamp to max_rate
 *
 * @param workspace Global workspace handle
 * @param metrics Most recent broadcast metrics
 */
void global_workspace_adapt_broadcast_rate(
    global_workspace_t* workspace,
    const shannon_broadcast_metrics_t* metrics
);

//=============================================================================
// Statistics and Monitoring
//=============================================================================

/**
 * @brief Get Shannon workspace statistics
 *
 * WHAT: Retrieve accumulated Shannon metrics
 * WHY:  Analyze information flow, efficiency, bottlenecks
 * HOW:  Copy internal statistics
 *
 * @param workspace Global workspace handle
 * @param stats Output statistics structure
 * @return true if Shannon is enabled and stats available
 */
bool global_workspace_get_shannon_stats(
    const global_workspace_t* workspace,
    shannon_workspace_stats_t* stats
);

/**
 * @brief Reset Shannon statistics
 *
 * WHAT: Clear accumulated Shannon metrics
 * WHY:  Start fresh measurement period
 * HOW:  Zero all Shannon statistics
 *
 * @param workspace Global workspace handle
 */
void global_workspace_reset_shannon_stats(global_workspace_t* workspace);

/**
 * @brief Get last broadcast Shannon metrics
 *
 * WHAT: Retrieve metrics from most recent broadcast
 * WHY:  Inspect individual broadcast performance
 * HOW:  Copy cached metrics from last broadcast
 *
 * @param workspace Global workspace handle
 * @param metrics Output metrics structure
 * @return true if metrics available
 */
bool global_workspace_get_last_broadcast_metrics(
    const global_workspace_t* workspace,
    shannon_broadcast_metrics_t* metrics
);

//=============================================================================
// Event Handler Integration
//=============================================================================

/**
 * @brief Handle salience peak with information weighting
 *
 * WHAT: Process salience peak event with Shannon-enhanced competition
 * WHY:  Entry point for salience-driven workspace access
 * HOW:  Measure info, compute weighted competition, broadcast with monitoring
 *
 * This is the main integration point from the spec:
 * competition_strength = peak->salience_score * (info_bits / 10.0f)
 *
 * @param workspace Global workspace handle
 * @param peak Salience peak data from salience detector
 * @return true if event won competition and was broadcast
 */
bool global_workspace_on_salience_peak_shannon(
    global_workspace_t* workspace,
    const void* peak  /* salience_peak_data_t* */
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GLOBAL_WORKSPACE_SHANNON_H */
