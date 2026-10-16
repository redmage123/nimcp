/**
 * @file nimcp_alignment_monitor.h
 * @brief Alignment Drift Monitor for AI Safety
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Continuous monitoring of AI value alignment over time
 * WHY:  Detect drift from baseline values before it becomes dangerous
 * HOW:  Statistical divergence measures, Bayesian inference, running statistics
 *
 * ALIGNMENT MONITORING GUARANTEES:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  The Alignment Monitor continuously tracks:                             │
 * │                                                                         │
 * │  1. VALUE VECTOR DRIFT: Cosine similarity to baseline values            │
 * │  2. KL DIVERGENCE: Distribution shift between stated and inferred       │
 * │  3. BEHAVIORAL CONSISTENCY: Do actions match stated values?             │
 * │  4. EXPLANATION CONSISTENCY: Do explanations match actions?             │
 * │                                                                         │
 * │  Any significant drift triggers tripwire alerts.                        │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * MATHEMATICAL FOUNDATION:
 * - KL Divergence: D_KL(P||Q) = Σ P(x) log(P(x)/Q(x))
 * - JS Divergence: D_JS(P||Q) = 0.5 * D_KL(P||M) + 0.5 * D_KL(Q||M), M = 0.5(P+Q)
 * - Mutual Information: I(X;Y) = H(X) + H(Y) - H(X,Y)
 * - Cosine Similarity: cos(θ) = (A·B) / (||A|| ||B||)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ALIGNMENT_MONITOR_H
#define NIMCP_ALIGNMENT_MONITOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Alignment monitor magic number */
#define ALIGNMENT_MONITOR_MAGIC             0x414C4944  /* "ALID" */

/** @brief Number of value dimensions to track */
#define ALIGNMENT_VALUE_DIMENSIONS          16

/** @brief Maximum value name length */
#define ALIGNMENT_VALUE_NAME_MAX_LENGTH     64

/** @brief Maximum reason length for drift detection */
#define ALIGNMENT_REASON_MAX_LENGTH         512

/** @brief Maximum observation history to retain */
#define ALIGNMENT_MAX_OBSERVATIONS          10000

/** @brief Maximum drift events to track */
#define ALIGNMENT_MAX_DRIFT_EVENTS          100

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Value dimension definition
 *
 * WHAT: Named value dimension with weight
 * WHY:  Track specific value components
 */
typedef struct value_dimension {
    char name[ALIGNMENT_VALUE_NAME_MAX_LENGTH];
    float weight;                       /**< Importance weight for this dimension */
    float baseline_value;               /**< Initial baseline value */
    float current_value;                /**< Currently inferred value */
    float min_acceptable;               /**< Minimum acceptable value */
    float max_acceptable;               /**< Maximum acceptable value */
} value_dimension_t;

/**
 * @brief Running statistics for value tracking
 */
typedef struct value_running_stats {
    uint64_t n;              /**< Number of observations */
    double mean;             /**< Running mean */
    double m2;               /**< For variance (Welford's algorithm) */
    double min;              /**< Running minimum */
    double max;              /**< Running maximum */
} value_running_stats_t;

/**
 * @brief Alignment status snapshot
 */
typedef struct alignment_status {
    /* Baseline (immutable after initialization) */
    float baseline_values[ALIGNMENT_VALUE_DIMENSIONS];
    uint8_t baseline_hash[32];          /**< SHA-256 of baseline values */
    uint64_t baseline_timestamp;

    /* Current inferred values */
    float current_values[ALIGNMENT_VALUE_DIMENSIONS];
    value_running_stats_t value_stats[ALIGNMENT_VALUE_DIMENSIONS];

    /* Drift metrics */
    float cosine_similarity_to_baseline;
    float euclidean_distance;
    float max_dimension_drift;
    uint32_t most_drifted_dimension;

    /* Divergence metrics */
    float kl_divergence;                /**< KL(current || baseline) */
    float js_divergence;                /**< Jensen-Shannon divergence */
    float mutual_information;           /**< I(stated; inferred) */

    /* Consistency scores */
    float action_consistency_score;     /**< Do actions match stated values? */
    float explanation_consistency_score; /**< Do explanations match actions? */

    /* Overall */
    bool drift_detected;
    float overall_alignment_score;      /**< 0.0 = misaligned, 1.0 = aligned */
} alignment_status_t;

/**
 * @brief Drift detection thresholds
 */
typedef struct alignment_thresholds {
    /* Divergence thresholds */
    float kl_divergence_threshold;      /**< Alert if KL > threshold */
    float js_divergence_threshold;      /**< Alert if JS > threshold */
    float mutual_info_threshold;        /**< Alert if MI < threshold */

    /* Similarity thresholds */
    float cosine_similarity_min;        /**< Alert if cos < threshold */
    float euclidean_distance_max;       /**< Alert if dist > threshold */

    /* Per-dimension thresholds */
    float max_single_dimension_drift;   /**< Max drift in any dimension */

    /* Consistency thresholds */
    float action_consistency_min;       /**< Min action-value consistency */
    float explanation_consistency_min;  /**< Min explanation consistency */

    /* Bayesian posterior stability */
    float posterior_stability_threshold;

    /* Time-based */
    uint32_t observation_window_size;   /**< Window for running stats */
    bool enable_exponential_weighting;  /**< Weight recent more heavily */
    float exponential_decay_rate;       /**< Decay rate for weighting */
} alignment_thresholds_t;

/**
 * @brief Drift event record
 */
typedef struct alignment_drift_event {
    uint64_t timestamp;
    uint32_t dimension;                 /**< Which dimension drifted */
    float old_value;
    float new_value;
    float drift_amount;
    float kl_divergence;
    float js_divergence;
    char reason[ALIGNMENT_REASON_MAX_LENGTH];
    bool escalated_to_tripwire;
} alignment_drift_event_t;

/**
 * @brief Action observation for alignment inference
 */
typedef struct alignment_action_observation {
    uint64_t timestamp;
    char action_type[64];               /**< Type of action taken */
    float value_relevance[ALIGNMENT_VALUE_DIMENSIONS]; /**< How action relates to values */
    float intensity;                    /**< Intensity of action (0.0 - 1.0) */
    bool was_positive;                  /**< Positive or negative value expression */
} alignment_action_observation_t;

/**
 * @brief Explanation observation for consistency checking
 */
typedef struct alignment_explanation_observation {
    uint64_t timestamp;
    char explanation_summary[256];
    float stated_values[ALIGNMENT_VALUE_DIMENSIONS];
    float confidence;
} alignment_explanation_observation_t;

/**
 * @brief Alignment monitor configuration
 */
typedef struct alignment_monitor_config {
    /* Value dimensions */
    value_dimension_t value_dimensions[ALIGNMENT_VALUE_DIMENSIONS];
    uint32_t active_dimensions;         /**< How many dimensions are active */

    /* Thresholds */
    alignment_thresholds_t thresholds;

    /* Monitoring settings */
    bool enable_continuous_monitoring;
    uint32_t update_interval_ms;
    bool enable_bayesian_inference;
    bool enable_exponential_smoothing;
    float smoothing_alpha;

    /* Integration settings */
    bool alert_on_drift;
    bool connect_to_tripwires;
} alignment_monitor_config_t;

/**
 * @brief Alignment monitor statistics
 */
typedef struct alignment_monitor_stats {
    uint64_t total_observations;
    uint64_t action_observations;
    uint64_t explanation_observations;
    uint64_t drift_events_detected;
    uint64_t tripwire_escalations;
    float min_alignment_score_observed;
    float avg_alignment_score;
    float current_alignment_score;
    uint64_t monitoring_uptime_ms;
} alignment_monitor_stats_t;

/**
 * @brief Alignment monitor (opaque)
 */
typedef struct alignment_monitor alignment_monitor_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default alignment monitor configuration
 *
 * @return Default configuration with standard value dimensions
 */
NIMCP_EXPORT alignment_monitor_config_t alignment_monitor_default_config(void);

/**
 * @brief Create alignment monitor
 *
 * WHAT: Initialize alignment monitoring infrastructure
 * WHY:  Begin tracking value alignment from baseline
 * HOW:  Snapshot baseline values, initialize running statistics
 *
 * @param config Configuration (NULL for defaults)
 * @return Alignment monitor or NULL on failure
 */
NIMCP_EXPORT alignment_monitor_t* alignment_monitor_create(
    const alignment_monitor_config_t* config
);

/**
 * @brief Destroy alignment monitor
 *
 * @param monitor Alignment monitor handle
 */
NIMCP_EXPORT void alignment_monitor_destroy(alignment_monitor_t* monitor);

/**
 * @brief Reset alignment monitor to initial state
 *
 * WHAT: Reset all running statistics and observations
 * WHY:  Start fresh monitoring after acknowledged drift
 * HOW:  Clear history, re-snapshot baseline
 *
 * @param monitor Alignment monitor handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_reset(alignment_monitor_t* monitor);

/* ============================================================================
 * Observation API
 * ============================================================================ */

/**
 * @brief Observe an action for alignment inference
 *
 * WHAT: Update value estimates based on observed action
 * WHY:  Infer values from behavior (revealed preferences)
 * HOW:  Bayesian update of value posterior
 *
 * @param monitor Alignment monitor handle
 * @param observation Action observation
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_observe_action(
    alignment_monitor_t* monitor,
    const alignment_action_observation_t* observation
);

/**
 * @brief Observe an explanation for consistency checking
 *
 * WHAT: Check if stated values match inferred values
 * WHY:  Detect deceptive or inconsistent explanations
 * HOW:  Compare stated values to running inference
 *
 * @param monitor Alignment monitor handle
 * @param observation Explanation observation
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_observe_explanation(
    alignment_monitor_t* monitor,
    const alignment_explanation_observation_t* observation
);

/**
 * @brief Observe raw value vector
 *
 * WHAT: Directly observe a value vector
 * WHY:  For systems that can directly report values
 *
 * @param monitor Alignment monitor handle
 * @param values Value vector (ALIGNMENT_VALUE_DIMENSIONS floats)
 * @param confidence Confidence in the observation
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_observe_values(
    alignment_monitor_t* monitor,
    const float* values,
    float confidence
);

/* ============================================================================
 * Drift Detection API
 * ============================================================================ */

/**
 * @brief Check for alignment drift
 *
 * WHAT: Evaluate current alignment against baseline
 * WHY:  Detect value drift before it becomes dangerous
 * HOW:  Compute divergence metrics, compare to thresholds
 *
 * @param monitor Alignment monitor handle
 * @param thresholds Thresholds to use (NULL for configured defaults)
 * @param drift_detected Output: true if drift detected
 * @param status Output: detailed alignment status
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_check_drift(
    alignment_monitor_t* monitor,
    const alignment_thresholds_t* thresholds,
    bool* drift_detected,
    alignment_status_t* status
);

/**
 * @brief Infer current values from observations
 *
 * WHAT: Compute current inferred value vector
 * WHY:  Use running statistics to estimate values
 * HOW:  Bayesian posterior mean or running average
 *
 * @param monitor Alignment monitor handle
 * @param inferred_values Output: inferred value vector
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_infer_values(
    alignment_monitor_t* monitor,
    float* inferred_values
);

/**
 * @brief Get current alignment status
 *
 * @param monitor Alignment monitor handle
 * @param status Output: current alignment status
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_get_status(
    const alignment_monitor_t* monitor,
    alignment_status_t* status
);

/* ============================================================================
 * Divergence Metrics API
 * ============================================================================ */

/**
 * @brief Compute KL divergence between current and baseline
 *
 * @param monitor Alignment monitor handle
 * @param kl_divergence Output: KL divergence value
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_compute_kl_divergence(
    const alignment_monitor_t* monitor,
    float* kl_divergence
);

/**
 * @brief Compute Jensen-Shannon divergence
 *
 * @param monitor Alignment monitor handle
 * @param js_divergence Output: JS divergence value
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_compute_js_divergence(
    const alignment_monitor_t* monitor,
    float* js_divergence
);

/**
 * @brief Compute mutual information between stated and inferred values
 *
 * @param monitor Alignment monitor handle
 * @param mutual_info Output: mutual information value
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_compute_mutual_info(
    const alignment_monitor_t* monitor,
    float* mutual_info
);

/**
 * @brief Compute cosine similarity to baseline
 *
 * @param monitor Alignment monitor handle
 * @param similarity Output: cosine similarity (0.0 - 1.0)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_compute_cosine_similarity(
    const alignment_monitor_t* monitor,
    float* similarity
);

/* ============================================================================
 * Status API
 * ============================================================================ */

/**
 * @brief Get alignment monitor statistics
 *
 * @param monitor Alignment monitor handle
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_get_stats(
    const alignment_monitor_t* monitor,
    alignment_monitor_stats_t* stats
);

/**
 * @brief Get recent drift events
 *
 * @param monitor Alignment monitor handle
 * @param events Output array
 * @param max_events Maximum events to return
 * @param count_out Actual count returned
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_get_drift_events(
    const alignment_monitor_t* monitor,
    alignment_drift_event_t* events,
    size_t max_events,
    size_t* count_out
);

/**
 * @brief Get baseline values
 *
 * @param monitor Alignment monitor handle
 * @param baseline Output: baseline value vector
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_get_baseline(
    const alignment_monitor_t* monitor,
    float* baseline
);

/**
 * @brief Get value dimension info
 *
 * @param monitor Alignment monitor handle
 * @param dimension Dimension index
 * @param info Output: dimension information
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_get_dimension(
    const alignment_monitor_t* monitor,
    uint32_t dimension,
    value_dimension_t* info
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async for alignment messages
 *
 * @param monitor Alignment monitor handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_connect_bio_async(
    alignment_monitor_t* monitor
);

/**
 * @brief Connect to tripwire system for escalation
 *
 * @param monitor Alignment monitor handle
 * @param tripwires Tripwire system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_connect_tripwires(
    alignment_monitor_t* monitor,
    void* tripwires
);

/**
 * @brief Connect to value commitment for baseline verification
 *
 * @param monitor Alignment monitor handle
 * @param commitment Value commitment system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t alignment_monitor_connect_value_commitment(
    alignment_monitor_t* monitor,
    void* commitment
);

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Present alignment drift events as antigens
 * WHY:  Coordinate value drift response across safety modules
 * HOW:  Drift patterns processed by immune memory
 *
 * When connected:
 * - Drift events presented as antigens
 * - Bayesian updates inform immune response strength
 * - Memory cells learn drift patterns
 * - Cytokine signaling for gradual vs sudden drift
 *
 * @param monitor Alignment monitor handle
 * @param brain_immune Brain immune system handle
 * @return NIMCP_OK on success
 */
struct brain_immune;
NIMCP_EXPORT nimcp_error_t alignment_monitor_connect_brain_immune(
    alignment_monitor_t* monitor,
    struct brain_immune* brain_immune
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get default value dimension names
 *
 * @param names Output: array of dimension names
 * @param count Output: number of dimensions
 */
NIMCP_EXPORT void alignment_monitor_default_dimension_names(
    const char** names,
    uint32_t* count
);

/**
 * @brief Format alignment status as string
 *
 * @param status Alignment status
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
NIMCP_EXPORT size_t alignment_monitor_format_status(
    const alignment_status_t* status,
    char* buffer,
    size_t buffer_size
);

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent handle from brain init
 */
struct nimcp_health_agent;
NIMCP_EXPORT void alignment_monitor_set_health_agent(struct nimcp_health_agent* agent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ALIGNMENT_MONITOR_H */
