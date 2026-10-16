/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_infogeo_immune_bridge.h - Information Geometry to Immune System Bridge
//=============================================================================
/**
 * @file nimcp_infogeo_immune_bridge.h
 * @brief Bridge connecting Information Geometry with Brain Immune System
 *
 * WHAT: Provides bidirectional integration between Information Geometry and
 *       the brain immune system for geometry-aware anomaly detection.
 *
 * WHY:  Information geometry enables principled immune responses:
 *       - KL divergence detects distribution anomalies (abnormal patterns)
 *       - Fisher information identifies critical parameters to protect
 *       - Geodesic distances measure deviation from healthy states
 *       - Manifold curvature indicates system stability/stress
 *
 * HOW:  Two-way integration:
 *       1. InfoGeo -> Immune: Geometric anomalies trigger immune responses
 *       2. Immune -> InfoGeo: Inflammation affects Fisher computation
 *       3. KL divergence as anomaly detection signal
 *       4. Manifold drift indicates pathological states
 *
 * BIOLOGICAL ANALOGY:
 * ```
 * INFORMATION GEOMETRY                    IMMUNE SYSTEM
 * -----------------------------------------------------------------------
 * KL Divergence (high)                ->  Antigen detection (abnormality)
 * Fisher Information (parameter)      ->  Self/non-self discrimination
 * Manifold Drift                      ->  Disease progression detection
 * Geodesic to Healthy State           ->  Recovery distance estimation
 * High Curvature                      ->  System under stress
 * Natural Gradient Recovery           ->  Optimal healing direction
 *
 * IMMUNE EFFECTS ON GEOMETRY:
 * -----------------------------------------------------------------------
 * Inflammation                        ->  Increased noise in Fisher
 * Cytokine Release                    ->  Modulated learning rates
 * Immune Activation                   ->  Adaptive manifold boundaries
 * Recovery Process                    ->  Geodesic trajectory monitoring
 * ```
 *
 * KEY INSIGHT:
 * The brain immune system can use information geometry as a principled
 * anomaly detection system - KL divergence naturally measures how "different"
 * current distributions are from learned "healthy" baselines.
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_INFOGEO_IMMUNE_BRIDGE_H
#define NIMCP_INFOGEO_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define INFOGEO_IMMUNE_MODULE_NAME       "infogeo_immune_bridge"

/** Maximum monitored regions */
#define INFOGEO_IMMUNE_MAX_REGIONS       64

/** Maximum healthy baselines stored */
#define INFOGEO_IMMUNE_MAX_BASELINES     32

/** KL threshold for mild anomaly */
#define INFOGEO_IMMUNE_KL_MILD           0.3f

/** KL threshold for moderate anomaly */
#define INFOGEO_IMMUNE_KL_MODERATE       1.0f

/** KL threshold for severe anomaly */
#define INFOGEO_IMMUNE_KL_SEVERE         3.0f

/** Curvature threshold for stress */
#define INFOGEO_IMMUNE_CURV_STRESS       2.0f

/** Geodesic threshold for pathological distance */
#define INFOGEO_IMMUNE_GEODESIC_PATH     5.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Anomaly severity level
 */
typedef enum {
    INFOGEO_ANOMALY_NONE = 0,           /**< No anomaly detected */
    INFOGEO_ANOMALY_MILD,               /**< Mild deviation */
    INFOGEO_ANOMALY_MODERATE,           /**< Moderate anomaly */
    INFOGEO_ANOMALY_SEVERE,             /**< Severe anomaly */
    INFOGEO_ANOMALY_CRITICAL            /**< Critical anomaly */
} infogeo_anomaly_severity_t;

/**
 * @brief Anomaly type
 */
typedef enum {
    INFOGEO_ANOMALY_TYPE_KL = 0,        /**< KL divergence anomaly */
    INFOGEO_ANOMALY_TYPE_FISHER,        /**< Fisher information anomaly */
    INFOGEO_ANOMALY_TYPE_MANIFOLD,      /**< Manifold drift anomaly */
    INFOGEO_ANOMALY_TYPE_CURVATURE,     /**< Curvature anomaly */
    INFOGEO_ANOMALY_TYPE_GEODESIC       /**< Geodesic distance anomaly */
} infogeo_anomaly_type_t;

/**
 * @brief Immune response type
 */
typedef enum {
    INFOGEO_IMMUNE_RESP_NONE = 0,       /**< No response */
    INFOGEO_IMMUNE_RESP_MONITOR,        /**< Increased monitoring */
    INFOGEO_IMMUNE_RESP_ALERT,          /**< Alert other systems */
    INFOGEO_IMMUNE_RESP_CONTAIN,        /**< Contain affected region */
    INFOGEO_IMMUNE_RESP_REPAIR          /**< Initiate repair process */
} infogeo_immune_response_t;

/**
 * @brief Health state classification
 */
typedef enum {
    INFOGEO_HEALTH_HEALTHY = 0,         /**< Within normal bounds */
    INFOGEO_HEALTH_STRESSED,            /**< Elevated but manageable */
    INFOGEO_HEALTH_COMPROMISED,         /**< Partially affected */
    INFOGEO_HEALTH_PATHOLOGICAL         /**< Severely abnormal */
} infogeo_health_state_t;

/**
 * @brief Recovery tracking mode
 */
typedef enum {
    INFOGEO_RECOVERY_NONE = 0,          /**< Not in recovery */
    INFOGEO_RECOVERY_MONITORING,        /**< Monitoring recovery */
    INFOGEO_RECOVERY_ACTIVE,            /**< Actively recovering */
    INFOGEO_RECOVERY_COMPLETED          /**< Recovery completed */
} infogeo_recovery_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for Information Geometry-Immune bridge
 */
typedef struct {
    /** Anomaly detection thresholds */
    float kl_mild_threshold;             /**< KL for mild anomaly */
    float kl_moderate_threshold;         /**< KL for moderate anomaly */
    float kl_severe_threshold;           /**< KL for severe anomaly */
    float curvature_stress_threshold;    /**< Curvature for stress */
    float geodesic_pathological_thresh;  /**< Geodesic for pathological */

    /** Response settings */
    bool enable_auto_response;           /**< Automatic immune response */
    float response_delay_ms;             /**< Delay before response */
    float monitoring_window_ms;          /**< Monitoring time window */

    /** Baseline settings */
    uint32_t baseline_samples;           /**< Samples for baseline */
    float baseline_update_rate;          /**< Rate of baseline adaptation */
    bool enable_adaptive_baseline;       /**< Adapt baselines over time */

    /** Recovery settings */
    bool enable_recovery_tracking;       /**< Track recovery progress */
    float recovery_check_interval_ms;    /**< Recovery check interval */
    float recovery_threshold;            /**< KL threshold for recovery */

    /** Inflammation effects */
    bool enable_inflammation_effects;    /**< Inflammation affects geometry */
    float inflammation_fisher_noise;     /**< Noise added to Fisher */
    float inflammation_lr_scaling;       /**< Learning rate scaling */

    /** General settings */
    float update_interval_ms;            /**< Bridge update interval */
    bool enable_logging;                 /**< Enable logging */
} infogeo_immune_config_t;

/**
 * @brief Healthy baseline state
 */
typedef struct {
    uint32_t region_id;                 /**< Region identifier */
    float* mean_distribution;           /**< Healthy mean distribution */
    float* fisher_diagonal;             /**< Healthy Fisher diagonal */
    float* manifold_embedding;          /**< Healthy manifold position */
    uint32_t dim;                       /**< Dimensionality */
    float mean_curvature;               /**< Healthy mean curvature */
    uint64_t samples_collected;         /**< Samples in baseline */
    float last_update_ms;               /**< Last baseline update */
} infogeo_baseline_t;

/**
 * @brief Anomaly detection result
 */
typedef struct {
    uint32_t region_id;                 /**< Region where detected */
    infogeo_anomaly_type_t type;        /**< Type of anomaly */
    infogeo_anomaly_severity_t severity; /**< Severity level */
    float metric_value;                 /**< Metric value that triggered */
    float threshold;                    /**< Threshold that was exceeded */
    float confidence;                   /**< Detection confidence */
    float timestamp_ms;                 /**< Detection timestamp */
} infogeo_anomaly_t;

/**
 * @brief Immune response action
 */
typedef struct {
    infogeo_immune_response_t response; /**< Response type */
    uint32_t target_region;             /**< Target region */
    infogeo_anomaly_t triggering_anomaly; /**< What triggered response */
    float response_strength;            /**< Response magnitude [0,1] */
    float timestamp_ms;                 /**< Response timestamp */
    bool acknowledged;                  /**< Whether response was acked */
} infogeo_immune_action_t;

/**
 * @brief Health assessment for region
 */
typedef struct {
    uint32_t region_id;                 /**< Region identifier */
    infogeo_health_state_t state;       /**< Current health state */
    float kl_from_baseline;             /**< KL divergence from baseline */
    float geodesic_from_healthy;        /**< Geodesic to healthy state */
    float curvature_deviation;          /**< Curvature vs baseline */
    float fisher_deviation;             /**< Fisher deviation ratio */
    float overall_health_score;         /**< Combined health [0,1] */
    float timestamp_ms;                 /**< Assessment timestamp */
} infogeo_health_assessment_t;

/**
 * @brief Recovery tracking state
 */
typedef struct {
    uint32_t region_id;                 /**< Region in recovery */
    infogeo_recovery_mode_t mode;       /**< Current recovery mode */
    float initial_kl;                   /**< KL at start of recovery */
    float current_kl;                   /**< Current KL */
    float target_kl;                    /**< Target KL for recovery */
    float recovery_rate;                /**< Rate of KL decrease */
    float estimated_time_to_recovery;   /**< Estimated time remaining */
    float progress;                     /**< Recovery progress [0,1] */
    float start_time_ms;                /**< Recovery start time */
} infogeo_recovery_state_t;

/**
 * @brief Inflammation effect on geometry
 */
typedef struct {
    uint32_t region_id;                 /**< Affected region */
    float inflammation_level;           /**< Current inflammation [0,1] */
    float fisher_noise_added;           /**< Noise added to Fisher */
    float learning_rate_scaling;        /**< LR scaling factor */
    float manifold_stability;           /**< Manifold stability [0,1] */
} infogeo_inflammation_effect_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t anomalies_detected;        /**< Total anomalies detected */
    uint64_t mild_anomalies;            /**< Mild anomaly count */
    uint64_t moderate_anomalies;        /**< Moderate anomaly count */
    uint64_t severe_anomalies;          /**< Severe anomaly count */
    uint64_t immune_responses;          /**< Immune responses triggered */
    uint64_t recoveries_completed;      /**< Successful recoveries */
    float avg_kl_divergence;            /**< Average KL from baselines */
    float avg_health_score;             /**< Average health score */
    float time_in_pathological_ms;      /**< Time in pathological state */
    float last_update_ms;               /**< Last update timestamp */
} infogeo_immune_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct infogeo_immune_bridge_struct infogeo_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_default_config(
    infogeo_immune_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Information Geometry-Immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT infogeo_immune_bridge_t* infogeo_immune_bridge_create(
    const infogeo_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void infogeo_immune_bridge_destroy(
    infogeo_immune_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_reset(infogeo_immune_bridge_t* bridge);

//=============================================================================
// Baseline Management API
//=============================================================================

/**
 * @brief Initialize healthy baseline for region
 *
 * WHAT: Establishes healthy reference state for anomaly detection
 * WHY:  Anomalies are deviations from learned healthy baseline
 * HOW:  Stores distribution, Fisher, manifold state as reference
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param distribution Reference distribution
 * @param fisher_diagonal Reference Fisher diagonal
 * @param dim Dimensionality
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_init_baseline(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    const float* distribution,
    const float* fisher_diagonal,
    uint32_t dim
);

/**
 * @brief Update baseline with new sample
 *
 * WHAT: Updates healthy baseline with new observation
 * WHY:  Adaptive baselines track slow healthy drift
 * HOW:  Exponential moving average update
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param distribution Current distribution sample
 * @param dim Dimensionality
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_update_baseline(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    const float* distribution,
    uint32_t dim
);

/**
 * @brief Get baseline for region
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param baseline Output baseline state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_get_baseline(
    const infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    infogeo_baseline_t* baseline
);

//=============================================================================
// Anomaly Detection API (InfoGeo -> Immune)
//=============================================================================

/**
 * @brief Check for KL divergence anomaly
 *
 * WHAT: Detects anomaly based on KL divergence from baseline
 * WHY:  KL divergence measures distribution abnormality
 * HOW:  Computes KL from current to baseline, checks thresholds
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param current_distribution Current distribution
 * @param dim Dimensionality
 * @param anomaly Output anomaly (if detected)
 * @return true if anomaly detected, false otherwise
 */
NIMCP_EXPORT bool infogeo_immune_check_kl_anomaly(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    const float* current_distribution,
    uint32_t dim,
    infogeo_anomaly_t* anomaly
);

/**
 * @brief Check for manifold drift anomaly
 *
 * WHAT: Detects anomaly from manifold embedding drift
 * WHY:  Manifold drift indicates structural changes
 * HOW:  Computes geodesic from current to baseline embedding
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param current_embedding Current manifold embedding
 * @param dim Embedding dimensionality
 * @param anomaly Output anomaly (if detected)
 * @return true if anomaly detected, false otherwise
 */
NIMCP_EXPORT bool infogeo_immune_check_manifold_anomaly(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    const float* current_embedding,
    uint32_t dim,
    infogeo_anomaly_t* anomaly
);

/**
 * @brief Check for curvature anomaly
 *
 * WHAT: Detects anomaly from abnormal curvature
 * WHY:  High curvature indicates system stress
 * HOW:  Compares current curvature to baseline
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param current_curvature Current Ricci curvature
 * @param anomaly Output anomaly (if detected)
 * @return true if anomaly detected, false otherwise
 */
NIMCP_EXPORT bool infogeo_immune_check_curvature_anomaly(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    float current_curvature,
    infogeo_anomaly_t* anomaly
);

/**
 * @brief Run full anomaly scan
 *
 * WHAT: Comprehensive anomaly check across all metrics
 * WHY:  Single call for complete anomaly detection
 * HOW:  Checks KL, manifold, curvature for all regions
 *
 * @param bridge Bridge handle
 * @param anomalies Output array of detected anomalies
 * @param max_anomalies Maximum anomalies to return
 * @return Number of anomalies detected, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_scan_anomalies(
    infogeo_immune_bridge_t* bridge,
    infogeo_anomaly_t* anomalies,
    uint32_t max_anomalies
);

//=============================================================================
// Health Assessment API
//=============================================================================

/**
 * @brief Assess health of region
 *
 * WHAT: Computes comprehensive health assessment
 * WHY:  Single health metric for region status
 * HOW:  Combines KL, geodesic, curvature metrics
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param assessment Output health assessment
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_assess_health(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    infogeo_health_assessment_t* assessment
);

/**
 * @brief Get system-wide health score
 *
 * @param bridge Bridge handle
 * @return Health score [0,1], 1.0 = fully healthy
 */
NIMCP_EXPORT float infogeo_immune_get_system_health(
    const infogeo_immune_bridge_t* bridge
);

/**
 * @brief Get health state for region
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @return Health state classification
 */
NIMCP_EXPORT infogeo_health_state_t infogeo_immune_get_health_state(
    const infogeo_immune_bridge_t* bridge,
    uint32_t region_id
);

//=============================================================================
// Immune Response API
//=============================================================================

/**
 * @brief Trigger immune response
 *
 * WHAT: Initiates immune response to anomaly
 * WHY:  Enables automated response to detected anomalies
 * HOW:  Creates response action based on anomaly severity
 *
 * @param bridge Bridge handle
 * @param anomaly Anomaly that triggered response
 * @param action Output immune action
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_trigger_response(
    infogeo_immune_bridge_t* bridge,
    const infogeo_anomaly_t* anomaly,
    infogeo_immune_action_t* action
);

/**
 * @brief Get pending immune actions
 *
 * @param bridge Bridge handle
 * @param actions Output array of pending actions
 * @param max_actions Maximum actions to return
 * @return Number of pending actions, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_get_pending_actions(
    const infogeo_immune_bridge_t* bridge,
    infogeo_immune_action_t* actions,
    uint32_t max_actions
);

/**
 * @brief Acknowledge immune action
 *
 * @param bridge Bridge handle
 * @param action_id Action identifier to acknowledge
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_acknowledge_action(
    infogeo_immune_bridge_t* bridge,
    uint32_t action_id
);

//=============================================================================
// Recovery Tracking API
//=============================================================================

/**
 * @brief Start recovery tracking for region
 *
 * WHAT: Begins monitoring recovery progress
 * WHY:  Track return to healthy state
 * HOW:  Records initial state, monitors KL toward baseline
 *
 * @param bridge Bridge handle
 * @param region_id Region to track
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_start_recovery_tracking(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Get recovery state
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param recovery Output recovery state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_get_recovery_state(
    const infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    infogeo_recovery_state_t* recovery
);

/**
 * @brief Compute optimal recovery direction
 *
 * WHAT: Computes natural gradient toward healthy state
 * WHY:  Provides optimal direction for recovery
 * HOW:  Natural gradient from current to baseline
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param recovery_direction Output recovery direction
 * @param dim Dimensionality
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_compute_recovery_direction(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    float* recovery_direction,
    uint32_t dim
);

//=============================================================================
// Inflammation Effects API (Immune -> InfoGeo)
//=============================================================================

/**
 * @brief Set inflammation level for region
 *
 * WHAT: Sets inflammation level affecting geometry computation
 * WHY:  Inflammation introduces noise and affects learning
 * HOW:  Scales Fisher noise and learning rate
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param inflammation_level Inflammation level [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_set_inflammation(
    infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    float inflammation_level
);

/**
 * @brief Get inflammation effects on geometry
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param effect Output inflammation effect
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_get_inflammation_effect(
    const infogeo_immune_bridge_t* bridge,
    uint32_t region_id,
    infogeo_inflammation_effect_t* effect
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Run anomaly scans, update recovery tracking
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_update(
    infogeo_immune_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int infogeo_immune_get_stats(
    const infogeo_immune_bridge_t* bridge,
    infogeo_immune_stats_t* stats
);

/**
 * @brief Get number of active anomalies
 *
 * @param bridge Bridge handle
 * @return Number of active anomalies
 */
NIMCP_EXPORT uint32_t infogeo_immune_active_anomaly_count(
    const infogeo_immune_bridge_t* bridge
);

/**
 * @brief Check if any region is in pathological state
 *
 * @param bridge Bridge handle
 * @return true if any region is pathological
 */
NIMCP_EXPORT bool infogeo_immune_has_pathological_region(
    const infogeo_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INFOGEO_IMMUNE_BRIDGE_H */