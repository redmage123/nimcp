/**
 * @file nimcp_omni_wm_security_immune_bridge.h
 * @brief World Model Security-Immune Bridge - Security Integration with Cytokine Modulation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Security and Immune systems
 * WHY:  Enable security-aware predictions with immune cytokine modulation of confidence
 * HOW:  WM predicts threats; security events train WM; cytokines modulate prediction dynamics
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * NEUROIMMUNE MODULATION OF COGNITION:
 * ------------------------------------
 * Cytokines released during immune responses profoundly affect cognitive processing:
 *
 *   IL-1, IL-6, TNF-alpha -> Pro-inflammatory -> Conservative predictions, lower confidence
 *   IL-10 -> Anti-inflammatory -> Normal operation, restored confidence
 *   IFN-gamma -> Adaptive immunity -> Pattern updates, enhanced learning
 *
 * This mirrors "sickness behavior" where inflammation reduces cognitive flexibility
 * and increases threat sensitivity - an evolutionarily advantageous response.
 *
 * PREDICTIVE SECURITY:
 * --------------------
 * The world model enables proactive threat detection:
 *
 *   1. ANOMALY PREDICTION: Use RSSM to forecast unusual state trajectories
 *   2. THREAT FORECASTING: Predict security events before they occur
 *   3. BBB STATE MODELING: Learn blood-brain barrier permeability dynamics
 *   4. ERROR-DRIVEN ALERTS: Prediction errors signal potential threats
 *
 * CYTOKINE MODULATION EFFECTS:
 * ----------------------------
 *   IL-1 (Pro-inflammatory):
 *     - Decreases prediction confidence by 15-30%
 *     - Shortens prediction horizon
 *     - Increases threat sensitivity
 *
 *   IL-6 (Acute Phase):
 *     - Accelerates prediction updates
 *     - Biases toward threat detection
 *     - Increases vigilance
 *
 *   TNF-alpha (Damage Signal):
 *     - Conservative predictions only
 *     - Maximizes false positive tolerance
 *     - Triggers immune response to PE
 *
 *   IL-10 (Anti-inflammatory):
 *     - Restores normal confidence levels
 *     - Returns to baseline prediction horizon
 *     - Enables model refinement
 *
 *   IFN-gamma (Adaptive Immunity):
 *     - Enhances pattern learning rate
 *     - Updates threat signatures
 *     - Strengthens memory of threats
 *
 * DATA FLOW:
 * ----------
 *   WM -> Security: Anomaly predictions, threat forecasts
 *   Security -> WM: Security events for training, BBB state
 *   Immune -> WM: Cytokine levels modulate prediction confidence
 *   WM -> Immune: Prediction errors trigger immune responses
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E63
 *   Message Range: 0x6300-0x63FF
 */

#ifndef NIMCP_OMNI_WM_SECURITY_IMMUNE_BRIDGE_H
#define NIMCP_OMNI_WM_SECURITY_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_SECURITY_* message types */
#include "security/nimcp_blood_brain_barrier.h"  /* bbb_system_t */
/* Phase 8: Forward declaration for health agent */
typedef struct nimcp_health_agent nimcp_health_agent_t;


#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* World Model (from nimcp_omni_world_model.h) */
typedef struct omni_world_model omni_world_model_t;

/* Security System (from nimcp_security.h) */
typedef struct nimcp_security_system nimcp_security_system_t;

/* BBB System type included from nimcp_blood_brain_barrier.h above:
 *   - bbb_system_t
 */

/* Brain Immune System (from nimcp_brain_immune.h) */
typedef struct brain_immune_system brain_immune_system_t;

/* Anomaly Detector (from nimcp_anomaly_detector.h) */
typedef struct nimcp_anomaly_detector_internal* nimcp_anomaly_detector_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Security-Immune Bridge */
#define BIO_MODULE_WM_SECURITY_IMMUNE_BRIDGE    0x0E63

/** Maximum state dimension for predictions */
#define WM_SECURITY_MAX_STATE_DIM               256

/** Maximum threat forecast horizon (steps) */
#define WM_SECURITY_MAX_FORECAST_HORIZON        16

/** Maximum number of tracked threat signatures */
#define WM_SECURITY_MAX_THREAT_SIGNATURES       64

/** Maximum cytokine types tracked */
#define WM_SECURITY_MAX_CYTOKINES               6

/** Default anomaly prediction confidence threshold */
#define WM_SECURITY_DEFAULT_ANOMALY_THRESHOLD   0.7f

/** Default immune modulation sensitivity */
#define WM_SECURITY_DEFAULT_IMMUNE_SENSITIVITY  1.0f

/* ============================================================================
 * Bio-Async Message Types (0x6300-0x63FF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_SECURITY_ANOMALY_PRED (0x6300): Anomaly prediction from WM
 *   - BIO_MSG_WM_SECURITY_THREAT_FORECAST: Threat forecast
 *   - BIO_MSG_WM_IMMUNE_CYTOKINE_UPDATE: Cytokine levels to WM
 * ============================================================================ */

/** @brief Message type alias for Security-Immune bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_security_immune_msg_type_t;

/* ============================================================================
 * Cytokine Effect Structures
 * ============================================================================ */

/**
 * @brief Individual cytokine modulation effect
 *
 * WHAT: Effect of a single cytokine on WM prediction
 * WHY:  Different cytokines have different cognitive effects
 * HOW:  Level (0-1) and modulation multipliers
 */
typedef struct {
    float level;                    /**< Cytokine concentration [0.0-1.0] */
    float confidence_multiplier;    /**< Effect on prediction confidence */
    float horizon_multiplier;       /**< Effect on prediction horizon */
    float learning_rate_multiplier; /**< Effect on learning rate */
    float vigilance_boost;          /**< Increase in threat sensitivity */
} wm_cytokine_effect_t;

/**
 * @brief Full cytokine modulation state
 *
 * WHAT: All cytokine effects combined
 * WHY:  Track complete immune modulation profile
 * HOW:  Individual cytokine levels and combined effects
 *
 * BIOLOGICAL MAPPING:
 *   il1_effect: IL-1β - Fever, sickness behavior, reduced cognition
 *   il6_effect: IL-6 - Acute phase response, metabolic changes
 *   tnf_alpha_effect: TNF-α - Inflammation, damage signaling
 *   il10_effect: IL-10 - Anti-inflammatory, resolution
 *   ifn_gamma_effect: IFN-γ - Adaptive immunity, pattern learning
 */
typedef struct {
    /* Individual cytokine effects */
    wm_cytokine_effect_t il1_effect;        /**< IL-1β effects */
    wm_cytokine_effect_t il6_effect;        /**< IL-6 effects */
    wm_cytokine_effect_t tnf_alpha_effect;  /**< TNF-α effects */
    wm_cytokine_effect_t il10_effect;       /**< IL-10 effects */
    wm_cytokine_effect_t ifn_gamma_effect;  /**< IFN-γ effects */

    /* Combined modulation values */
    float combined_confidence_mod;   /**< Net confidence modification [0.5-1.5] */
    float combined_horizon_mod;      /**< Net horizon modification [0.25-1.0] */
    float combined_learning_mod;     /**< Net learning rate modification [0.5-2.0] */
    float combined_vigilance;        /**< Net vigilance level [0.0-1.0] */

    /* Inflammation state */
    uint32_t inflammation_level;     /**< 0=none, 1=local, 2=regional, 3=systemic, 4=storm */
    bool is_cytokine_storm;          /**< Cytokine storm detected (dangerous) */

    /* Timing */
    uint64_t last_update_us;         /**< Last update timestamp */
    double decay_rate;               /**< Cytokine decay rate per second */
} immune_to_wm_modulation_t;

/* ============================================================================
 * Security Effect Structures
 * ============================================================================ */

/**
 * @brief Threat prediction from World Model
 *
 * WHAT: WM-generated threat forecast
 * WHY:  Proactive security through prediction
 * HOW:  State trajectory analysis for anomalies
 */
typedef struct {
    uint32_t threat_type;            /**< Predicted threat type */
    float* predicted_state;          /**< Predicted anomalous state */
    uint32_t state_dim;              /**< State dimensionality */
    float confidence;                /**< Prediction confidence [0.0-1.0] */
    uint32_t horizon_steps;          /**< Steps ahead predicted */
    float time_to_threat_ms;         /**< Estimated time until threat */
    float severity_estimate;         /**< Estimated severity [0.0-1.0] */
    uint64_t timestamp_us;           /**< When prediction was made */
} wm_threat_prediction_t;

/**
 * @brief Security event for WM training
 *
 * WHAT: Security event to train world model
 * WHY:  Learn threat patterns from actual events
 * HOW:  Pre/post states, threat info, outcome
 */
typedef struct {
    uint32_t event_type;             /**< Security event type */
    float* pre_event_state;          /**< State before event */
    float* post_event_state;         /**< State after event */
    uint32_t state_dim;              /**< State dimensionality */
    float* threat_signature;         /**< Threat signature learned */
    uint32_t signature_dim;          /**< Signature dimensionality */
    float severity;                  /**< Event severity */
    bool was_predicted;              /**< Did WM predict this? */
    float prediction_lead_time_ms;   /**< How early was prediction */
    uint64_t timestamp_us;           /**< Event timestamp */
} security_event_for_wm_t;

/**
 * @brief BBB (Blood-Brain Barrier) state for WM
 *
 * WHAT: Current BBB permeability and threat state
 * WHY:  Model security boundary dynamics
 * HOW:  Permeability levels, threat counts, breach info
 */
typedef struct {
    float permeability;              /**< Current permeability [0.0-1.0] */
    float integrity;                 /**< Barrier integrity [0.0-1.0] */
    uint32_t active_threats;         /**< Number of active threats */
    uint32_t blocked_threats;        /**< Threats blocked since last update */
    uint32_t breaches;               /**< Active breaches */
    float stress_level;              /**< BBB stress level [0.0-1.0] */
    bool is_compromised;             /**< BBB is compromised */
    uint64_t last_breach_us;         /**< Timestamp of last breach */
} bbb_state_for_wm_t;

/* ============================================================================
 * Bidirectional Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from World Model to Security/Immune
 *
 * WHAT: WM predictions and triggers flowing to security/immune
 * WHY:  Enable predictive security and PE-driven immune responses
 * HOW:  Threat forecasts, anomaly predictions, PE signals
 */
typedef struct {
    /* Anomaly Predictions */
    float anomaly_score;             /**< Current anomaly score [0.0-1.0] */
    float anomaly_confidence;        /**< Confidence in anomaly detection */
    float* anomaly_features;         /**< Triggered anomaly features */
    uint32_t anomaly_features_dim;   /**< Feature dimensionality */
    bool anomaly_detected;           /**< Anomaly above threshold */

    /* Threat Forecasts */
    wm_threat_prediction_t* active_predictions; /**< Active threat predictions */
    uint32_t num_predictions;        /**< Number of active predictions */
    float max_threat_confidence;     /**< Highest threat confidence */

    /* Prediction Errors (for immune system) */
    float forward_pe;                /**< Forward prediction error */
    float backward_pe;               /**< Backward inference error */
    float combined_pe;               /**< Combined prediction error */
    float pe_precision;              /**< Precision of PE estimate */

    /* Immune Triggers */
    bool should_trigger_immune;      /**< PE exceeds immune threshold */
    float immune_trigger_strength;   /**< Strength of immune trigger */
    uint32_t suggested_response;     /**< Suggested immune response type */

    /* State Information */
    float* current_state_snapshot;   /**< Current WM state */
    uint32_t state_dim;              /**< State dimensionality */
    float state_uncertainty;         /**< Uncertainty in current state */
    double snapshot_timestamp;       /**< When snapshot was taken */
} omni_wm_to_security_effects_t;

/**
 * @brief Effects from Security/Immune to World Model
 *
 * WHAT: Security events and immune modulation flowing to WM
 * WHY:  Train WM from events, modulate predictions via cytokines
 * HOW:  Security events, BBB state, cytokine modulation
 */
typedef struct {
    /* Security Events for Training */
    security_event_for_wm_t* pending_events; /**< Events pending training */
    uint32_t num_pending_events;     /**< Number of pending events */
    float training_priority;         /**< Priority for training */

    /* BBB State */
    bbb_state_for_wm_t bbb_state;    /**< Current BBB state */
    bool bbb_state_valid;            /**< Is BBB state current */

    /* Immune Modulation */
    immune_to_wm_modulation_t immune_modulation; /**< Cytokine modulation */
    bool modulation_active;          /**< Is modulation active */

    /* Alert State */
    uint32_t security_alert_level;   /**< Current alert level (0-4) */
    bool under_attack;               /**< Active attack in progress */
    float attack_severity;           /**< Severity of current attack */

    /* Threat Signatures for Learning */
    float** threat_signatures;       /**< Learned threat signatures */
    uint32_t num_signatures;         /**< Number of signatures */
    uint32_t signature_dim;          /**< Signature dimensionality */
} security_immune_to_wm_effects_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Security-Immune Bridge configuration
 *
 * WHAT: Parameters controlling WM-Security-Immune integration
 * WHY:  Tune anomaly detection, immune modulation, and training
 * HOW:  Configurable thresholds, sensitivities, and feature flags
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;          /**< Enable bidirectional modulation */
    float sensitivity;               /**< General sensitivity [0.5-2.0] */

    /* Anomaly Detection Settings */
    bool enable_anomaly_prediction;  /**< Enable WM anomaly predictions */
    float anomaly_threshold;         /**< Threshold for anomaly detection [0.0-1.0] */
    float anomaly_confidence_min;    /**< Minimum confidence for alerts */
    uint32_t anomaly_feature_count;  /**< Number of anomaly features tracked */

    /* Threat Forecasting Settings */
    bool enable_threat_forecasting;  /**< Enable threat trajectory prediction */
    uint32_t forecast_horizon;       /**< Prediction horizon (steps) */
    float forecast_confidence_min;   /**< Minimum confidence for forecasts */
    uint32_t max_active_forecasts;   /**< Maximum simultaneous forecasts */

    /* Security Event Training Settings */
    bool enable_security_training;   /**< Train WM from security events */
    float security_learning_rate;    /**< Learning rate for security events */
    uint32_t max_pending_events;     /**< Maximum pending events buffer */
    float event_priority_decay;      /**< Priority decay for old events */

    /* BBB Integration Settings */
    bool enable_bbb_modeling;        /**< Model BBB dynamics */
    float bbb_state_weight;          /**< Weight of BBB state in predictions */
    float bbb_breach_pe_threshold;   /**< PE threshold to signal breach */

    /* Immune Modulation Settings */
    bool enable_immune_modulation;   /**< Enable cytokine modulation */
    float immune_sensitivity;        /**< Sensitivity to cytokines [0.5-2.0] */
    float cytokine_decay_rate;       /**< Cytokine decay per second */

    /* IL-1 Modulation Parameters */
    float il1_confidence_factor;     /**< IL-1 effect on confidence [0.7-0.85] */
    float il1_horizon_factor;        /**< IL-1 effect on horizon [0.5-0.8] */
    float il1_vigilance_boost;       /**< IL-1 vigilance increase [0.1-0.3] */

    /* IL-6 Modulation Parameters */
    float il6_learning_factor;       /**< IL-6 effect on learning [1.2-1.5] */
    float il6_update_factor;         /**< IL-6 effect on update rate [1.1-1.3] */
    float il6_vigilance_boost;       /**< IL-6 vigilance increase [0.15-0.25] */

    /* TNF-alpha Modulation Parameters */
    float tnf_confidence_factor;     /**< TNF-α effect on confidence [0.5-0.7] */
    float tnf_conservatism_boost;    /**< TNF-α increase in conservatism [0.2-0.4] */
    float tnf_pe_immune_threshold;   /**< PE threshold for immune trigger [0.3-0.5] */

    /* IL-10 Modulation Parameters */
    float il10_restoration_rate;     /**< IL-10 confidence restoration rate */
    float il10_resolution_factor;    /**< IL-10 resolution acceleration */

    /* IFN-gamma Modulation Parameters */
    float ifn_learning_boost;        /**< IFN-γ learning rate boost [1.5-2.0] */
    float ifn_pattern_strength;      /**< IFN-γ pattern encoding strength */

    /* PE -> Immune Trigger Settings */
    bool enable_pe_immune_trigger;   /**< Enable PE-triggered immune responses */
    float pe_immune_threshold;       /**< PE threshold for immune trigger */
    float pe_immune_scale;           /**< Scale factor for PE->immune */

    /* Bio-async Settings */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} omni_wm_security_immune_config_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Security-Immune Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and accuracy metrics
 */
typedef struct {
    /* Anomaly Detection Statistics */
    uint64_t anomalies_predicted;    /**< Total anomalies predicted */
    uint64_t anomalies_verified;     /**< Predictions verified by security */
    uint64_t false_positives;        /**< False positive predictions */
    uint64_t false_negatives;        /**< Missed anomalies */
    float mean_anomaly_confidence;   /**< Average anomaly confidence */
    float anomaly_precision;         /**< Precision = TP / (TP + FP) */
    float anomaly_recall;            /**< Recall = TP / (TP + FN) */

    /* Threat Forecasting Statistics */
    uint64_t threats_forecasted;     /**< Total threats forecasted */
    uint64_t forecasts_accurate;     /**< Accurate forecasts */
    float mean_forecast_lead_time_ms;/**< Average forecast lead time */
    float forecast_accuracy;         /**< Forecast accuracy rate */

    /* Security Training Statistics */
    uint64_t security_events_processed; /**< Events used for training */
    uint64_t threat_signatures_learned; /**< Unique signatures learned */
    float mean_security_training_loss;  /**< Average training loss */

    /* BBB Statistics */
    uint64_t bbb_state_updates;      /**< BBB state updates received */
    uint64_t bbb_breaches_detected;  /**< Breaches detected */
    float mean_bbb_integrity;        /**< Average BBB integrity */

    /* Immune Modulation Statistics */
    uint64_t modulation_updates;     /**< Immune modulation updates */
    uint64_t pe_immune_triggers;     /**< PE-triggered immune responses */
    float mean_confidence_modifier;  /**< Average confidence modification */
    float mean_vigilance_level;      /**< Average vigilance level */
    uint64_t cytokine_storms;        /**< Cytokine storm events */

    /* Timing Statistics */
    uint64_t total_updates;          /**< Total update cycles */
    double total_processing_time_ms; /**< Total processing time */
    double mean_update_time_ms;      /**< Average update duration */
    uint64_t last_update_time_us;    /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;           /**< Total errors */
    uint64_t errors_prediction;      /**< Prediction errors */
    uint64_t errors_training;        /**< Training errors */
    uint64_t errors_modulation;      /**< Modulation errors */
} omni_wm_security_immune_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Security-Immune Bridge
 *
 * WHAT: Main bridge structure connecting WM with Security and Immune systems
 * WHY:  Orchestrates bidirectional information flow with cytokine modulation
 * HOW:  Maintains connections, effects, and modulation state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_security_immune_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    omni_wm_security_immune_config_t config; /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model; /**< World model (RSSM) */
    nimcp_security_system_t* security; /**< Security system */
    bbb_system_t bbb_system;         /**< Blood-brain barrier */
    brain_immune_system_t* immune;   /**< Brain immune system */
    nimcp_anomaly_detector_t anomaly_detector; /**< Anomaly detector */

    /* Bidirectional Effects */
    omni_wm_to_security_effects_t wm_to_security; /**< Effects: WM -> Security */
    security_immune_to_wm_effects_t security_to_wm; /**< Effects: Security -> WM */

    /* Internal Modulation State */
    immune_to_wm_modulation_t current_modulation; /**< Current immune modulation */
    float baseline_confidence;       /**< Baseline prediction confidence */
    uint32_t baseline_horizon;       /**< Baseline prediction horizon */
    float baseline_learning_rate;    /**< Baseline learning rate */

    /* Threat Prediction Buffer */
    wm_threat_prediction_t* prediction_buffer; /**< Active predictions */
    uint32_t prediction_buffer_size; /**< Current buffer occupancy */
    uint32_t prediction_buffer_capacity; /**< Buffer capacity */

    /* Security Event Buffer */
    security_event_for_wm_t* event_buffer; /**< Pending security events */
    uint32_t event_buffer_size;      /**< Current buffer occupancy */
    uint32_t event_buffer_capacity;  /**< Buffer capacity */

    /* Threat Signature Cache */
    float** signature_cache;         /**< Learned threat signatures */
    uint32_t signature_count;        /**< Number of signatures */
    uint32_t signature_capacity;     /**< Cache capacity */
    uint32_t signature_dim;          /**< Signature dimensionality */

    /* Internal State */
    bool under_attack;               /**< Currently under attack */
    uint32_t alert_level;            /**< Current security alert level */
    bool modulation_active;          /**< Immune modulation active */
    uint64_t attack_start_time_us;   /**< When attack started */

    /* Statistics */
    omni_wm_security_immune_stats_t stats; /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;     /**< Per-instance health agent */
} omni_wm_security_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Sets all config fields to defaults based on neuroimmune research
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_default_config(
    omni_wm_security_immune_config_t* config);

/**
 * @brief Create World Model Security-Immune Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_security_immune_bridge_t* omni_wm_security_immune_bridge_create(
    const omni_wm_security_immune_config_t* config);

/**
 * @brief Destroy World Model Security-Immune Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_security_immune_bridge_destroy(
    omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects, modulation, and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, reset stats, restore baseline modulation
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_reset(
    omni_wm_security_immune_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all systems to bridge
 *
 * WHAT: Establish connections to WM, Security, BBB, and Immune systems
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param security Security system - optional
 * @param bbb_system BBB system - optional
 * @param immune Brain immune system - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_connect(
    omni_wm_security_immune_bridge_t* bridge,
    omni_world_model_t* world_model,
    nimcp_security_system_t* security,
    bbb_system_t bbb_system,
    brain_immune_system_t* immune);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_connect_world_model(
    omni_wm_security_immune_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect security system
 *
 * @param bridge Bridge instance
 * @param security Security system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_connect_security(
    omni_wm_security_immune_bridge_t* bridge,
    nimcp_security_system_t* security);

/**
 * @brief Connect BBB system
 *
 * @param bridge Bridge instance
 * @param bbb_system BBB system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_connect_bbb(
    omni_wm_security_immune_bridge_t* bridge,
    bbb_system_t bbb_system);

/**
 * @brief Connect brain immune system
 *
 * @param bridge Bridge instance
 * @param immune Brain immune system to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_connect_immune(
    omni_wm_security_immune_bridge_t* bridge,
    brain_immune_system_t* immune);

/**
 * @brief Connect anomaly detector
 *
 * @param bridge Bridge instance
 * @param detector Anomaly detector to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_connect_anomaly_detector(
    omni_wm_security_immune_bridge_t* bridge,
    nimcp_anomaly_detector_t detector);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_security_immune_bridge_is_connected(
    const omni_wm_security_immune_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM, security, and immune systems
 * HOW:  Update modulation, generate predictions, process events
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_update(
    omni_wm_security_immune_bridge_t* bridge,
    float dt);

/* ============================================================================
 * Anomaly Prediction API
 * ============================================================================ */

/**
 * @brief Generate anomaly prediction from current state
 *
 * WHAT: Use WM to predict anomalous state trajectories
 * WHY:  Proactive threat detection
 * HOW:  Forward prediction with anomaly scoring
 *
 * @param bridge Bridge instance
 * @param horizon_steps Steps to predict ahead
 * @param anomaly_score_out Output: anomaly score [0.0-1.0]
 * @param confidence_out Output: prediction confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_predict_anomaly(
    omni_wm_security_immune_bridge_t* bridge,
    uint32_t horizon_steps,
    float* anomaly_score_out,
    float* confidence_out);

/**
 * @brief Report verified anomaly (for training)
 *
 * WHAT: Provide feedback on anomaly prediction accuracy
 * WHY:  Train WM from verified security events
 * HOW:  Update prediction accuracy statistics, adjust thresholds
 *
 * @param bridge Bridge instance
 * @param was_true_positive Was the prediction correct?
 * @param actual_severity Actual severity of anomaly
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_report_anomaly(
    omni_wm_security_immune_bridge_t* bridge,
    bool was_true_positive,
    float actual_severity);

/* ============================================================================
 * Threat Forecasting API
 * ============================================================================ */

/**
 * @brief Generate threat forecast
 *
 * WHAT: Predict potential security threats
 * WHY:  Enable preemptive security measures
 * HOW:  Trajectory prediction with threat classification
 *
 * @param bridge Bridge instance
 * @param threat_type Specific threat type to forecast (0 for any)
 * @param prediction_out Output: threat prediction
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_forecast_threat(
    omni_wm_security_immune_bridge_t* bridge,
    uint32_t threat_type,
    wm_threat_prediction_t* prediction_out);

/**
 * @brief Get active threat predictions
 *
 * @param bridge Bridge instance
 * @return Pointer to active predictions array (do not free)
 */
const wm_threat_prediction_t* omni_wm_security_immune_bridge_get_active_predictions(
    const omni_wm_security_immune_bridge_t* bridge,
    uint32_t* count_out);

/* ============================================================================
 * Security Event API
 * ============================================================================ */

/**
 * @brief Process security event for WM training
 *
 * WHAT: Train world model from security event
 * WHY:  Learn threat patterns from actual events
 * HOW:  Extract transition, update RSSM, store signature
 *
 * @param bridge Bridge instance
 * @param event Security event to process
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_process_security_event(
    omni_wm_security_immune_bridge_t* bridge,
    const security_event_for_wm_t* event);

/**
 * @brief Update BBB state
 *
 * WHAT: Receive BBB state update
 * WHY:  Model security boundary in predictions
 * HOW:  Store state, adjust prediction parameters
 *
 * @param bridge Bridge instance
 * @param bbb_state Current BBB state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_update_bbb_state(
    omni_wm_security_immune_bridge_t* bridge,
    const bbb_state_for_wm_t* bbb_state);

/**
 * @brief Set security alert level
 *
 * WHAT: Notify bridge of security alert level change
 * WHY:  Adjust prediction behavior based on threat level
 * HOW:  Modify vigilance, conservatism based on alert
 *
 * @param bridge Bridge instance
 * @param alert_level Alert level (0-4)
 * @param under_attack True if active attack in progress
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_set_alert_level(
    omni_wm_security_immune_bridge_t* bridge,
    uint32_t alert_level,
    bool under_attack);

/* ============================================================================
 * Immune Modulation API
 * ============================================================================ */

/**
 * @brief Update cytokine levels
 *
 * WHAT: Receive cytokine level updates from immune system
 * WHY:  Modulate WM predictions based on immune state
 * HOW:  Update modulation parameters, apply to predictions
 *
 * @param bridge Bridge instance
 * @param il1_level IL-1β level [0.0-1.0]
 * @param il6_level IL-6 level [0.0-1.0]
 * @param tnf_alpha_level TNF-α level [0.0-1.0]
 * @param il10_level IL-10 level [0.0-1.0]
 * @param ifn_gamma_level IFN-γ level [0.0-1.0]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_update_cytokines(
    omni_wm_security_immune_bridge_t* bridge,
    float il1_level,
    float il6_level,
    float tnf_alpha_level,
    float il10_level,
    float ifn_gamma_level);

/**
 * @brief Set inflammation level
 *
 * WHAT: Directly set inflammation level
 * WHY:  Allow inflammation-based modulation without individual cytokines
 * HOW:  Map inflammation to combined modulation parameters
 *
 * @param bridge Bridge instance
 * @param inflammation_level Level (0=none, 1=local, 2=regional, 3=systemic, 4=storm)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_set_inflammation(
    omni_wm_security_immune_bridge_t* bridge,
    uint32_t inflammation_level);

/**
 * @brief Get current immune modulation state
 *
 * @param bridge Bridge instance
 * @return Pointer to modulation state (do not free)
 */
const immune_to_wm_modulation_t* omni_wm_security_immune_bridge_get_modulation(
    const omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Check if prediction error should trigger immune response
 *
 * WHAT: Evaluate if current PE warrants immune activation
 * WHY:  Enable PE-driven immune responses
 * HOW:  Compare PE to threshold, consider modulation state
 *
 * @param bridge Bridge instance
 * @param prediction_error Current prediction error
 * @param should_trigger_out Output: true if should trigger immune
 * @param trigger_strength_out Output: strength of trigger [0.0-1.0]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_check_pe_trigger(
    omni_wm_security_immune_bridge_t* bridge,
    float prediction_error,
    bool* should_trigger_out,
    float* trigger_strength_out);

/**
 * @brief Trigger immune response from prediction error
 *
 * WHAT: Signal immune system to respond to prediction error
 * WHY:  Large PEs may indicate threats requiring immune response
 * HOW:  Present PE as antigen to immune system
 *
 * @param bridge Bridge instance
 * @param prediction_error Prediction error magnitude
 * @param error_source Source of error (for classification)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_trigger_immune(
    omni_wm_security_immune_bridge_t* bridge,
    float prediction_error,
    uint32_t error_source);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from WM to security
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_security_effects_t* omni_wm_security_immune_bridge_get_wm_effects(
    const omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Get current effects from security/immune to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const security_immune_to_wm_effects_t* omni_wm_security_immune_bridge_get_security_effects(
    const omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_get_stats(
    const omni_wm_security_immune_bridge_t* bridge,
    omni_wm_security_immune_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_reset_stats(
    omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Get modulated prediction confidence
 *
 * WHAT: Get current confidence level with immune modulation applied
 * WHY:  Other modules need modulated confidence
 * HOW:  baseline_confidence * combined_confidence_mod
 *
 * @param bridge Bridge instance
 * @return Modulated confidence [0.0-1.0]
 */
float omni_wm_security_immune_bridge_get_modulated_confidence(
    const omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Get modulated prediction horizon
 *
 * WHAT: Get current horizon with immune modulation applied
 * WHY:  Other modules need modulated horizon
 * HOW:  baseline_horizon * combined_horizon_mod
 *
 * @param bridge Bridge instance
 * @return Modulated horizon (steps)
 */
uint32_t omni_wm_security_immune_bridge_get_modulated_horizon(
    const omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Get modulated learning rate
 *
 * WHAT: Get current learning rate with immune modulation applied
 * WHY:  Other modules need modulated learning rate
 * HOW:  baseline_learning_rate * combined_learning_mod
 *
 * @param bridge Bridge instance
 * @return Modulated learning rate
 */
float omni_wm_security_immune_bridge_get_modulated_learning_rate(
    const omni_wm_security_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_connect_bio_async(
    omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_disconnect_bio_async(
    omni_wm_security_immune_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_security_immune_bridge_is_bio_async_connected(
    const omni_wm_security_immune_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_security_immune_msg_type_to_string(
    omni_wm_security_immune_msg_type_t msg_type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_security_immune_bridge_validate_config(
    const omni_wm_security_immune_config_t* config);

/**
 * @brief Compute combined cytokine modulation
 *
 * WHAT: Compute net effect of all cytokines
 * WHY:  Internal helper, exposed for testing
 * HOW:  Weighted combination of individual effects
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_security_immune_bridge_compute_modulation(
    omni_wm_security_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_SECURITY_IMMUNE_BRIDGE_H */
