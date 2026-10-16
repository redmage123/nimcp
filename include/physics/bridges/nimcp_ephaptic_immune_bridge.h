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
// nimcp_ephaptic_immune_bridge.h - Ephaptic to Immune System Threat Detection Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_immune_bridge.h
 * @brief Bridge between Ephaptic coupling and Brain Immune System threat detection
 *
 * WHAT: Connects ephaptic field dynamics with the brain immune system,
 *       enabling field-based anomaly detection and immune response triggering.
 *
 * WHY:  Bridges the gap between:
 *       - Ephaptic field patterns (neural synchronization state)
 *       - Immune system surveillance (anomaly detection)
 *       - Threat response coordination
 *       Abnormal ephaptic patterns (hypersynchrony, desynchrony) can indicate
 *       pathological states that the immune system should address.
 *
 * HOW:  Integration mechanisms:
 *       1. Field coherence anomalies trigger immune alerts
 *       2. Hypersynchrony (seizure-like) activates B-cell response
 *       3. Desynchrony (disconnection) signals potential lesions
 *       4. Field entropy maps to threat level estimation
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPHAPTIC COUPLING                      BRAIN IMMUNE SYSTEM
 * ─────────────────────────────────────────────────────────────────────
 * Hypersynchrony (high coherence)     -> Seizure-like threat detection
 * Desynchrony (low coherence)         -> Disconnection/lesion alert
 * Field entropy                       -> Disorder/inflammation marker
 * LFP amplitude extremes              -> Excitotoxicity warning
 * Coherence instability               -> Homeostatic threat
 * Spatial pattern abnormality         -> Localized damage detection
 * ```
 *
 * KEY MECHANISMS:
 * - Coherence monitoring: Detect abnormally high/low synchronization
 * - Entropy-based threat: High field entropy indicates disorder
 * - Spatial anomaly: Unusual field patterns suggest localized issues
 * - Temporal stability: Rapid coherence changes signal acute threats
 *
 * REFERENCES:
 * - Bhattacharya & Bhattacharya (2024) "Neuroimmune interactions in the brain"
 * - Dantzer et al. (2008) "From inflammation to sickness and depression"
 * - Kipnis (2016) "Multifaceted interactions between adaptive immunity and
 *   the central nervous system"
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_IMMUNE_BRIDGE_H
#define NIMCP_EPHAPTIC_IMMUNE_BRIDGE_H

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
#define EPHAPTIC_IMMUNE_MODULE_NAME         "ephaptic_immune_bridge"

/** Maximum monitored regions */
#define EPHAPTIC_IMMUNE_MAX_REGIONS         64

/** Maximum active threats */
#define EPHAPTIC_IMMUNE_MAX_THREATS         32

/** Maximum immune responses */
#define EPHAPTIC_IMMUNE_MAX_RESPONSES       16

/** Default hypersynchrony threshold (coherence) */
#define EPHAPTIC_IMMUNE_DEFAULT_HYPER_THRESH    0.95f

/** Default desynchrony threshold (coherence) */
#define EPHAPTIC_IMMUNE_DEFAULT_DESYNC_THRESH   0.2f

/** Default entropy threshold for threat */
#define EPHAPTIC_IMMUNE_DEFAULT_ENTROPY_THRESH  0.8f

/** Default LFP amplitude threshold (mV) */
#define EPHAPTIC_IMMUNE_DEFAULT_LFP_THRESH      10.0f

/** Default threat decay time constant (ms) */
#define EPHAPTIC_IMMUNE_DEFAULT_THREAT_TAU      5000.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Threat type detected from ephaptic patterns
 */
typedef enum {
    EPHAPTIC_IMMUNE_THREAT_NONE = 0,          /**< No threat detected */
    EPHAPTIC_IMMUNE_THREAT_HYPERSYNC,         /**< Hypersynchrony (seizure-like) */
    EPHAPTIC_IMMUNE_THREAT_DESYNC,            /**< Desynchronization (lesion-like) */
    EPHAPTIC_IMMUNE_THREAT_ENTROPY,           /**< High entropy (disorder) */
    EPHAPTIC_IMMUNE_THREAT_EXCITOTOXIC,       /**< Excitotoxic activity */
    EPHAPTIC_IMMUNE_THREAT_SPATIAL_ANOMALY,   /**< Spatial pattern anomaly */
    EPHAPTIC_IMMUNE_THREAT_INSTABILITY        /**< Temporal instability */
} ephaptic_immune_threat_type_t;

/**
 * @brief Threat severity level
 */
typedef enum {
    EPHAPTIC_IMMUNE_SEVERITY_NONE = 0,        /**< No threat */
    EPHAPTIC_IMMUNE_SEVERITY_LOW,             /**< Low (monitoring) */
    EPHAPTIC_IMMUNE_SEVERITY_MEDIUM,          /**< Medium (heightened alert) */
    EPHAPTIC_IMMUNE_SEVERITY_HIGH,            /**< High (active response) */
    EPHAPTIC_IMMUNE_SEVERITY_CRITICAL         /**< Critical (emergency response) */
} ephaptic_immune_severity_t;

/**
 * @brief Immune response type
 */
typedef enum {
    EPHAPTIC_IMMUNE_RESPONSE_NONE = 0,        /**< No response */
    EPHAPTIC_IMMUNE_RESPONSE_MONITOR,         /**< Increased monitoring */
    EPHAPTIC_IMMUNE_RESPONSE_ALERT,           /**< Alert other systems */
    EPHAPTIC_IMMUNE_RESPONSE_MICROGLIA,       /**< Microglia activation */
    EPHAPTIC_IMMUNE_RESPONSE_CYTOKINE,        /**< Cytokine signaling */
    EPHAPTIC_IMMUNE_RESPONSE_SUPPRESS,        /**< Activity suppression */
    EPHAPTIC_IMMUNE_RESPONSE_REPAIR           /**< Initiate repair process */
} ephaptic_immune_response_type_t;

/**
 * @brief Detection mode
 */
typedef enum {
    EPHAPTIC_IMMUNE_DETECT_THRESHOLD = 0,     /**< Simple threshold detection */
    EPHAPTIC_IMMUNE_DETECT_ADAPTIVE,          /**< Adaptive threshold */
    EPHAPTIC_IMMUNE_DETECT_PATTERN,           /**< Pattern-based detection */
    EPHAPTIC_IMMUNE_DETECT_COMBINED           /**< Combined detection methods */
} ephaptic_immune_detect_mode_t;

/**
 * @brief Monitoring region type
 */
typedef enum {
    EPHAPTIC_IMMUNE_REGION_CORTICAL = 0,      /**< Cortical region */
    EPHAPTIC_IMMUNE_REGION_SUBCORTICAL,       /**< Subcortical region */
    EPHAPTIC_IMMUNE_REGION_HIPPOCAMPAL,       /**< Hippocampal region */
    EPHAPTIC_IMMUNE_REGION_BRAINSTEM,         /**< Brainstem region */
    EPHAPTIC_IMMUNE_REGION_CUSTOM             /**< Custom region */
} ephaptic_immune_region_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for ephaptic-immune bridge
 */
typedef struct {
    /** Detection parameters */
    ephaptic_immune_detect_mode_t detect_mode; /**< Detection method */
    float hypersync_threshold;                  /**< Coherence threshold for hyper */
    float desync_threshold;                     /**< Coherence threshold for desync */
    float entropy_threshold;                    /**< Entropy threshold for threat */
    float lfp_amplitude_threshold;              /**< LFP threshold (mV) */
    float instability_window_ms;                /**< Window for instability detection */
    float instability_threshold;                /**< Coherence variance threshold */

    /** Adaptive thresholds */
    bool enable_adaptive;                       /**< Enable adaptive thresholds */
    float adaptation_rate;                      /**< Adaptation rate */
    float adaptation_window_ms;                 /**< Adaptation window */

    /** Threat parameters */
    float threat_decay_tau_ms;                  /**< Threat decay time constant */
    float threat_accumulation_rate;             /**< Threat build-up rate */
    float severity_thresholds[4];               /**< Thresholds for severity levels */

    /** Response parameters */
    bool auto_response;                         /**< Enable automatic responses */
    float response_delay_ms;                    /**< Delay before response */
    float response_cooldown_ms;                 /**< Cooldown between responses */

    /** Spatial monitoring */
    bool enable_spatial_monitoring;             /**< Monitor spatial patterns */
    float spatial_anomaly_threshold;            /**< Threshold for spatial anomaly */

    /** Update parameters */
    float update_interval_ms;                   /**< Bridge update interval */
} ephaptic_immune_config_t;

/**
 * @brief Monitored region state
 */
typedef struct {
    uint32_t region_id;                         /**< Region identifier */
    ephaptic_immune_region_type_t type;        /**< Region type */
    float position[3];                          /**< Region center (mm) */
    float radius;                               /**< Region radius (mm) */
    float coherence;                            /**< Current coherence */
    float entropy;                              /**< Current entropy */
    float lfp_amplitude;                        /**< Current LFP amplitude */
    float threat_level;                         /**< Accumulated threat level */
    ephaptic_immune_severity_t severity;       /**< Current severity */
    bool threat_active;                         /**< Is threat active */
} ephaptic_immune_region_state_t;

/**
 * @brief Detected threat
 */
typedef struct {
    uint32_t threat_id;                         /**< Threat identifier */
    ephaptic_immune_threat_type_t type;        /**< Threat type */
    ephaptic_immune_severity_t severity;       /**< Severity level */
    uint32_t region_id;                         /**< Affected region */
    float detection_time_ms;                    /**< When detected */
    float threat_level;                         /**< Threat magnitude [0,1] */
    float coherence_at_detection;               /**< Coherence when detected */
    float entropy_at_detection;                 /**< Entropy when detected */
    float duration_ms;                          /**< Duration so far */
    bool resolved;                              /**< Has threat resolved */
} ephaptic_immune_threat_t;

/**
 * @brief Immune response action
 */
typedef struct {
    uint32_t response_id;                       /**< Response identifier */
    ephaptic_immune_response_type_t type;      /**< Response type */
    uint32_t threat_id;                         /**< Associated threat */
    uint32_t region_id;                         /**< Target region */
    float activation_time_ms;                   /**< When activated */
    float strength;                             /**< Response strength [0,1] */
    float duration_ms;                          /**< Expected duration */
    bool active;                                /**< Is response active */
} ephaptic_immune_response_t;

/**
 * @brief Anomaly detection result
 */
typedef struct {
    bool anomaly_detected;                      /**< Was anomaly detected */
    ephaptic_immune_threat_type_t type;        /**< Type of anomaly */
    float coherence;                            /**< Coherence at detection */
    float entropy;                              /**< Entropy at detection */
    float lfp_amplitude;                        /**< LFP at detection */
    float deviation_score;                      /**< How anomalous [0,1] */
    float spatial_extent;                       /**< Spatial extent (mm) */
} ephaptic_immune_anomaly_t;

/**
 * @brief Field health assessment
 */
typedef struct {
    float health_score;                         /**< Overall health [0,1] */
    float coherence_health;                     /**< Coherence health [0,1] */
    float entropy_health;                       /**< Entropy health [0,1] */
    float stability_health;                     /**< Stability health [0,1] */
    uint32_t active_threats;                    /**< Number of active threats */
    uint32_t active_responses;                  /**< Number of active responses */
    ephaptic_immune_severity_t max_severity;   /**< Maximum current severity */
} ephaptic_immune_health_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t anomalies_detected;                /**< Total anomalies detected */
    uint64_t threats_generated;                 /**< Threats generated */
    uint64_t threats_resolved;                  /**< Threats resolved */
    uint64_t responses_triggered;               /**< Immune responses triggered */
    uint64_t hypersync_events;                  /**< Hypersynchrony events */
    uint64_t desync_events;                     /**< Desynchronization events */
    float avg_threat_duration_ms;               /**< Average threat duration */
    float avg_health_score;                     /**< Average health score */
    float max_threat_level;                     /**< Maximum threat level seen */
    float last_update_ms;                       /**< Last update timestamp */
} ephaptic_immune_stats_t;

/** Opaque bridge handle */
typedef struct ephaptic_immune_bridge_struct ephaptic_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration values
 * WHY:  Provide biologically plausible immune parameters
 * HOW:  Based on neuroimmune interaction literature
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_default_config(
    ephaptic_immune_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ephaptic-immune bridge
 *
 * WHAT: Allocates and initializes bridge instance
 * WHY:  Enable ephaptic-based immune surveillance
 * HOW:  Sets up monitoring regions, threat tracking, responses
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ephaptic_immune_bridge_t* ephaptic_immune_bridge_create(
    const ephaptic_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void ephaptic_immune_bridge_destroy(
    ephaptic_immune_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_bridge_reset(
    ephaptic_immune_bridge_t* bridge
);

//=============================================================================
// Region Management API
//=============================================================================

/**
 * @brief Register monitored region
 *
 * WHAT: Adds region for immune surveillance
 * WHY:  Track ephaptic health per region
 * HOW:  Stores region with spatial extent
 *
 * @param bridge    Bridge handle
 * @param region_id Region identifier
 * @param type      Region type
 * @param position  Region center (mm)
 * @param radius    Region radius (mm)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_register_region(
    ephaptic_immune_bridge_t* bridge,
    uint32_t region_id,
    ephaptic_immune_region_type_t type,
    const float position[3],
    float radius
);

/**
 * @brief Get region state
 *
 * @param bridge    Bridge handle
 * @param region_id Region identifier
 * @param state     Output region state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_get_region_state(
    const ephaptic_immune_bridge_t* bridge,
    uint32_t region_id,
    ephaptic_immune_region_state_t* state
);

/**
 * @brief Update region ephaptic state
 *
 * WHAT: Updates ephaptic measurements for region
 * WHY:  Provide current state for threat detection
 * HOW:  Updates coherence, entropy, LFP
 *
 * @param bridge      Bridge handle
 * @param region_id   Region identifier
 * @param coherence   Current coherence
 * @param entropy     Current entropy
 * @param lfp_amplitude Current LFP amplitude
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_update_region(
    ephaptic_immune_bridge_t* bridge,
    uint32_t region_id,
    float coherence,
    float entropy,
    float lfp_amplitude
);

//=============================================================================
// Anomaly Detection API
//=============================================================================

/**
 * @brief Detect anomaly in region
 *
 * WHAT: Checks for ephaptic anomalies in region
 * WHY:  Early detection of pathological activity
 * HOW:  Applies detection mode with current state
 *
 * BIOLOGICAL: Abnormal synchronization patterns (too high or too low)
 * can indicate pathological states like seizures or lesions.
 *
 * @param bridge    Bridge handle
 * @param region_id Region identifier
 * @param anomaly   Output anomaly result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_detect_anomaly(
    ephaptic_immune_bridge_t* bridge,
    uint32_t region_id,
    ephaptic_immune_anomaly_t* anomaly
);

/**
 * @brief Detect anomaly globally
 *
 * WHAT: Checks for anomalies across all regions
 * WHY:  Detect widespread or multi-region issues
 * HOW:  Aggregates region-level detections
 *
 * @param bridge        Bridge handle
 * @param anomaly_count Output: number of anomalies
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_detect_global(
    ephaptic_immune_bridge_t* bridge,
    uint32_t* anomaly_count
);

//=============================================================================
// Threat API
//=============================================================================

/**
 * @brief Generate threat from anomaly
 *
 * WHAT: Creates threat record from detected anomaly
 * WHY:  Track and respond to detected issues
 * HOW:  Converts anomaly to threat with severity
 *
 * @param bridge  Bridge handle
 * @param anomaly Detected anomaly
 * @param threat  Output threat record
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_generate_threat(
    ephaptic_immune_bridge_t* bridge,
    const ephaptic_immune_anomaly_t* anomaly,
    ephaptic_immune_threat_t* threat
);

/**
 * @brief Get active threat
 *
 * @param bridge    Bridge handle
 * @param threat_id Threat identifier
 * @param threat    Output threat record
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_get_threat(
    const ephaptic_immune_bridge_t* bridge,
    uint32_t threat_id,
    ephaptic_immune_threat_t* threat
);

/**
 * @brief Get active threat count
 *
 * @param bridge Bridge handle
 * @return Number of active threats, or 0 on error
 */
NIMCP_EXPORT uint32_t ephaptic_immune_get_threat_count(
    const ephaptic_immune_bridge_t* bridge
);

/**
 * @brief Resolve threat
 *
 * WHAT: Marks threat as resolved
 * WHY:  Clear resolved threats from tracking
 * HOW:  Sets resolved flag, updates stats
 *
 * @param bridge    Bridge handle
 * @param threat_id Threat identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_resolve_threat(
    ephaptic_immune_bridge_t* bridge,
    uint32_t threat_id
);

//=============================================================================
// Response API
//=============================================================================

/**
 * @brief Trigger immune response
 *
 * WHAT: Initiates immune response to threat
 * WHY:  Take action against detected threats
 * HOW:  Creates response based on threat severity
 *
 * @param bridge    Bridge handle
 * @param threat_id Threat to respond to
 * @param type      Response type
 * @param response  Output response record
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_trigger_response(
    ephaptic_immune_bridge_t* bridge,
    uint32_t threat_id,
    ephaptic_immune_response_type_t type,
    ephaptic_immune_response_t* response
);

/**
 * @brief Get recommended response for threat
 *
 * WHAT: Determines appropriate response for threat
 * WHY:  Automated response selection
 * HOW:  Maps threat type and severity to response
 *
 * @param bridge    Bridge handle
 * @param threat_id Threat identifier
 * @param type      Output: recommended response type
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_recommend_response(
    ephaptic_immune_bridge_t* bridge,
    uint32_t threat_id,
    ephaptic_immune_response_type_t* type
);

/**
 * @brief Get active responses
 *
 * @param bridge         Bridge handle
 * @param responses      Output array for responses
 * @param max_responses  Maximum responses to return
 * @return Number of active responses, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_get_responses(
    const ephaptic_immune_bridge_t* bridge,
    ephaptic_immune_response_t* responses,
    uint32_t max_responses
);

/**
 * @brief Complete response
 *
 * WHAT: Marks response as completed
 * WHY:  Track response lifecycle
 * HOW:  Deactivates response, updates stats
 *
 * @param bridge      Bridge handle
 * @param response_id Response identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_complete_response(
    ephaptic_immune_bridge_t* bridge,
    uint32_t response_id
);

//=============================================================================
// Health Assessment API
//=============================================================================

/**
 * @brief Assess field health
 *
 * WHAT: Computes overall ephaptic field health
 * WHY:  Holistic assessment of neural field state
 * HOW:  Aggregates coherence, entropy, stability metrics
 *
 * @param bridge  Bridge handle
 * @param health  Output health assessment
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_assess_health(
    ephaptic_immune_bridge_t* bridge,
    ephaptic_immune_health_t* health
);

/**
 * @brief Get health score
 *
 * @param bridge Bridge handle
 * @param score  Output: health score [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_get_health_score(
    const ephaptic_immune_bridge_t* bridge,
    float* score
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Decay threats, process responses, update health
 * HOW:  Called during simulation step
 *
 * @param bridge  Bridge handle
 * @param dt_ms   Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_update(
    ephaptic_immune_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Process all regions
 *
 * WHAT: Runs anomaly detection on all regions
 * WHY:  Batch processing of surveillance
 * HOW:  Iterates regions, detects anomalies, generates threats
 *
 * @param bridge Bridge handle
 * @return Number of anomalies detected, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_process_all(
    ephaptic_immune_bridge_t* bridge
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats  Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_get_stats(
    const ephaptic_immune_bridge_t* bridge,
    ephaptic_immune_stats_t* stats
);

/**
 * @brief Get maximum current severity
 *
 * @param bridge   Bridge handle
 * @param severity Output: maximum severity
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_immune_get_max_severity(
    const ephaptic_immune_bridge_t* bridge,
    ephaptic_immune_severity_t* severity
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge handle
 * @return true if bridge is active
 */
NIMCP_EXPORT bool ephaptic_immune_is_active(
    const ephaptic_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_IMMUNE_BRIDGE_H */