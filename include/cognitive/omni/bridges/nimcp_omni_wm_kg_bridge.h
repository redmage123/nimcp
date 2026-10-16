/**
 * @file nimcp_omni_wm_kg_bridge.h
 * @brief World Model KG Wiring Bridge - Integration with Knowledge Graph Module System
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with KG wiring system
 * WHY:  Enable semantic reasoning and module state prediction via world model
 * HOW:  KG entities/relationships train world model; WM predicts entity/module states
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * KNOWLEDGE-AUGMENTED WORLD MODELS:
 * ---------------------------------
 * Combining generative world models with structured knowledge graphs enables:
 *
 *   1. SEMANTIC STRUCTURE: KG provides entity-relationship structure
 *   2. TEMPORAL DYNAMICS: RSSM models how entities evolve over time
 *   3. COMPOSITIONAL PREDICTION: Predict entity states from relationships
 *   4. SYSTEM AWARENESS: Module dependency graph informs system state prediction
 *
 * WORLD MODEL + KG SYNERGY:
 * -------------------------
 * The RSSM world model learns to predict:
 *   s_t+1 = f(s_t, a_t, KG_context)
 *
 * Where KG_context provides:
 *   - Entity relationships affecting state transitions
 *   - Module dependency constraints
 *   - Exception patterns for anomaly detection
 *
 * DATA FLOW:
 * ----------
 *   KG -> WM: Semantic entity relationships for structured predictions
 *   KG -> WM: Module dependency graph for system state prediction
 *   WM -> KG: Predicted entity state changes
 *   WM -> KG: Anomaly detection for exception handling
 *   KG Registry -> WM: Module health states for system-wide prediction
 *   WM -> KG Registry: Predicted module failures
 *
 * INTEGRATION POINTS:
 * -------------------
 *   - KG Module Wiring (nimcp_kg_module_wiring.h): Module topology
 *   - KG Wiring Exception (nimcp_kg_wiring_exception.h): Exception handling
 *   - World Model (nimcp_omni_world_model.h): RSSM predictions
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E6B
 *   Message Range: 0x6B00-0x6BFF
 */

#ifndef NIMCP_OMNI_WM_KG_BRIDGE_H
#define NIMCP_OMNI_WM_KG_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_messages.h"  /* BIO_MSG_WM_KG_* message types */
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

/* KG Module Wiring (from nimcp_kg_module_wiring.h) */
typedef struct kg_module_wiring kg_module_wiring_t;

/* KG Wiring Manager - manages all module wirings */
typedef struct kg_wiring_manager kg_wiring_manager_t;

/* KG Module Registry - tracks module health and states */
typedef struct kg_module_registry kg_module_registry_t;

/* KG Exception (from nimcp_kg_wiring_exception.h)
 * Only forward declare if the actual header hasn't been included */
#ifndef NIMCP_KG_WIRING_EXCEPTION_H
struct nimcp_kg_wiring_exception;
typedef struct nimcp_kg_wiring_exception nimcp_kg_wiring_exception_t;
#endif

/* KG Event - generic event from KG system
 * Only forward declare if the actual header hasn't been included */
#ifndef NIMCP_KG_EVENTS_H
struct kg_event;
typedef struct kg_event kg_event_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model KG Bridge */
#define BIO_MODULE_WM_KG_BRIDGE             0x0E6B

/** Maximum state dimensionality for entity prediction */
#define WM_KG_MAX_STATE_DIM                 256

/** Maximum entities tracked per update */
#define WM_KG_MAX_ENTITIES_PER_UPDATE       64

/** Maximum relationships tracked per entity */
#define WM_KG_MAX_RELATIONSHIPS_PER_ENTITY  32

/** Maximum modules tracked for health prediction */
#define WM_KG_MAX_MODULES                   128

/** Default prediction horizon steps */
#define WM_KG_DEFAULT_HORIZON_STEPS         10

/** Default anomaly detection threshold */
#define WM_KG_DEFAULT_ANOMALY_THRESHOLD     0.7f

/** Default module failure prediction horizon (seconds) */
#define WM_KG_DEFAULT_FAILURE_HORIZON_SEC   60.0f

/* ============================================================================
 * Bio-Async Message Types (0x6B00-0x6BFF)
 * ============================================================================
 * Message types are defined in nimcp_bio_messages.h to avoid duplication.
 * Key message types used by this bridge:
 *   - BIO_MSG_WM_KG_ENTITY_PRED (0x6B00): Entity state prediction request
 *   - BIO_MSG_WM_KG_RELATIONSHIP_PRED: Relationship change prediction
 *   - BIO_MSG_WM_KG_MODULE_PRED: Module health prediction
 *   - BIO_MSG_WM_KG_EXCEPTION_NOTIFY (0x6B10): Exception notification to WM
 *   - BIO_MSG_WM_KG_WIRING_CHANGE: Wiring topology change
 *   - BIO_MSG_WM_KG_TRAINING_EVENT (0x6B20): KG event for WM training
 *   - BIO_MSG_WM_KG_SYSTEM_STABILITY: System stability forecast
 * ============================================================================ */

/** @brief Message type alias for KG bridge (uses bio_message_type_t from nimcp_bio_messages.h) */
typedef bio_message_type_t omni_wm_kg_msg_type_t;

/* ============================================================================
 * Entity/Relationship Types
 * ============================================================================ */

/**
 * @brief KG relationship type enumeration
 */
typedef enum {
    KG_REL_TYPE_UNKNOWN = 0,        /**< Unknown relationship */
    KG_REL_TYPE_DEPENDS_ON,         /**< Module A depends on Module B */
    KG_REL_TYPE_SENDS_TO,           /**< Module A sends messages to B */
    KG_REL_TYPE_RECEIVES_FROM,      /**< Module A receives from B */
    KG_REL_TYPE_INHIBITS,           /**< Module A inhibits Module B */
    KG_REL_TYPE_EXCITES,            /**< Module A excites Module B */
    KG_REL_TYPE_MODULATES,          /**< Module A modulates Module B */
    KG_REL_TYPE_CONTAINS,           /**< Entity A contains Entity B */
    KG_REL_TYPE_BELONGS_TO,         /**< Entity A belongs to Entity B */
    KG_REL_TYPE_RELATED_TO,         /**< Generic relationship */
    KG_REL_TYPE_COUNT               /**< Number of relationship types */
} kg_relationship_type_t;

/**
 * @brief Module health state enumeration
 */
typedef enum {
    KG_MODULE_HEALTH_UNKNOWN = 0,   /**< Unknown health state */
    KG_MODULE_HEALTH_HEALTHY,       /**< Fully operational */
    KG_MODULE_HEALTH_DEGRADED,      /**< Partially degraded */
    KG_MODULE_HEALTH_FAILING,       /**< Actively failing */
    KG_MODULE_HEALTH_FAILED,        /**< Completely failed */
    KG_MODULE_HEALTH_RECOVERING     /**< Recovering from failure */
} kg_module_health_t;

/* ============================================================================
 * Prediction Structures
 * ============================================================================ */

/**
 * @brief Entity state prediction from World Model
 *
 * WHAT: Predicted future state for a KG entity
 * WHY:  Enable proactive system management based on state forecasts
 * HOW:  RSSM forward prediction conditioned on entity relationships
 */
typedef struct {
    uint32_t entity_id;                         /**< Entity being predicted */
    float predicted_state[WM_KG_MAX_STATE_DIM]; /**< Predicted state vector */
    uint32_t state_dim;                         /**< Actual state dimensionality */
    float confidence;                           /**< Prediction confidence [0,1] */
    float uncertainty;                          /**< Prediction uncertainty (entropy) */
    uint32_t horizon_steps;                     /**< Prediction horizon in steps */
    uint64_t timestamp_us;                      /**< Prediction timestamp */
} wm_to_kg_entity_prediction_t;

/**
 * @brief Relationship information from KG to WM
 *
 * WHAT: Entity relationship for world model context
 * WHY:  Relationships constrain state predictions
 * HOW:  Provide graph structure as context to RSSM
 */
typedef struct {
    uint32_t source_entity_id;                  /**< Source entity ID */
    uint32_t target_entity_id;                  /**< Target entity ID */
    kg_relationship_type_t relationship_type;  /**< Type of relationship */
    float relationship_strength;                /**< Strength/weight [0,1] */
    bool is_bidirectional;                      /**< Bidirectional relationship */
    uint64_t last_updated_us;                   /**< Last update timestamp */
} kg_to_wm_relationship_t;

/**
 * @brief Module state information from KG Registry to WM
 *
 * WHAT: Current state of a brain module for prediction
 * WHY:  Enable system-wide health prediction and failure forecasting
 * HOW:  Aggregate module metrics into predictable state
 */
typedef struct {
    uint32_t module_id;                         /**< Module identifier */
    char module_name[64];                       /**< Module name */
    kg_module_health_t health_state;            /**< Current health state */
    float health_score;                         /**< Numeric health [0,1] */
    uint32_t exception_count;                   /**< Recent exception count */
    uint32_t message_backlog;                   /**< Pending message count */
    float cpu_utilization;                      /**< CPU usage [0,1] */
    float memory_utilization;                   /**< Memory usage [0,1] */
    uint64_t last_update_us;                    /**< Last state update */
    bool is_critical;                           /**< Is critical path module */
} kg_to_wm_module_state_t;

/**
 * @brief Module failure prediction from WM
 *
 * WHAT: Predicted future module health
 * WHY:  Enable proactive failure prevention
 * HOW:  RSSM predicts health trajectory from current state
 */
typedef struct {
    uint32_t module_id;                         /**< Module being predicted */
    float failure_probability;                  /**< P(failure) in horizon */
    float time_to_failure_sec;                  /**< Estimated time to failure */
    kg_module_health_t predicted_health;        /**< Predicted health state */
    float confidence;                           /**< Prediction confidence */
    uint32_t contributing_factors;              /**< Bitmask of factors */
    char reason[128];                           /**< Human-readable reason */
} wm_to_kg_failure_prediction_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from World Model to KG System
 *
 * WHAT: WM predictions and anomaly detections flowing to KG
 * WHY:  Inform KG of predicted changes and detected anomalies
 * HOW:  State predictions, relationship changes, stability scores
 */
typedef struct {
    /* Entity-level predictions */
    uint32_t entity_predictions_count;
    wm_to_kg_entity_prediction_t* entity_predictions;
    uint32_t entity_predictions_capacity;

    /* Relationship predictions */
    float relationship_change_probability;       /**< P(any relationship changes) */
    uint32_t predicted_new_relationships;        /**< Expected new relationships */
    uint32_t predicted_removed_relationships;    /**< Expected removed relationships */

    /* Module failure predictions */
    uint32_t failure_predictions_count;
    wm_to_kg_failure_prediction_t* failure_predictions;
    uint32_t failure_predictions_capacity;

    /* System-level predictions */
    float system_stability_score;               /**< Overall stability [0,1] */
    float system_entropy;                       /**< System state entropy */
    uint32_t predicted_exception_count;         /**< Expected exceptions */

    /* Anomaly detection */
    bool anomaly_detected;                      /**< Anomaly flag */
    float anomaly_score;                        /**< Anomaly severity [0,1] */
    uint32_t anomaly_entity_id;                 /**< Entity with anomaly */
    char anomaly_description[128];              /**< Description of anomaly */
} omni_wm_to_kg_effects_t;

/**
 * @brief Effects from KG System to World Model
 *
 * WHAT: KG context and events flowing to world model
 * WHY:  Provide structural context for predictions
 * HOW:  Entity states, relationships, module health
 */
typedef struct {
    /* Active entities context */
    uint32_t active_entities_count;
    uint32_t* active_entity_ids;
    uint32_t active_entities_capacity;

    /* Relationship graph */
    uint32_t relationship_count;
    kg_to_wm_relationship_t* relationships;
    uint32_t relationships_capacity;

    /* Module states from registry */
    uint32_t module_count;
    kg_to_wm_module_state_t* module_states;
    uint32_t module_states_capacity;

    /* Exception context */
    uint32_t pending_exception_count;
    bool exception_propagation_active;
    uint32_t last_exception_module_id;

    /* Wiring topology */
    uint32_t total_wiring_count;                /**< Total module wirings */
    uint32_t connected_modules_count;           /**< Connected module count */
    float graph_density;                        /**< Edge/possible_edge ratio */
    float avg_in_degree;                        /**< Average input connections */
    float avg_out_degree;                       /**< Average output connections */

    /* Training context */
    uint32_t pending_training_events;
    uint64_t last_kg_event_time_us;
} kg_to_omni_wm_effects_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model KG Bridge configuration
 *
 * WHAT: Parameters controlling WM-KG integration
 * WHY:  Tune prediction, anomaly detection, and training behavior
 * HOW:  Configurable thresholds, horizons, and feature flags
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;                     /**< Enable bidirectional flow */
    float sensitivity;                          /**< General sensitivity [0.5-2.0] */

    /* Entity Prediction Settings */
    bool enable_entity_prediction;              /**< Predict entity states */
    uint32_t default_prediction_horizon;        /**< Default prediction steps */
    float prediction_confidence_threshold;      /**< Min confidence to report */
    uint32_t max_entities_per_batch;            /**< Max entities per prediction batch */

    /* Relationship Prediction Settings */
    bool enable_relationship_prediction;        /**< Predict relationship changes */
    float relationship_change_threshold;        /**< Min P(change) to report */

    /* Module Health Prediction Settings */
    bool enable_module_prediction;              /**< Predict module failures */
    float failure_probability_threshold;        /**< Min P(fail) to report */
    float failure_horizon_sec;                  /**< Failure prediction horizon */

    /* Anomaly Detection Settings */
    bool enable_anomaly_detection;              /**< Detect anomalies in KG */
    float anomaly_threshold;                    /**< Anomaly score threshold */
    bool report_anomalies_to_exception;         /**< Forward to exception system */

    /* Training Settings */
    bool enable_training_from_kg;               /**< Train WM from KG events */
    float kg_training_learning_rate;            /**< Learning rate for KG events */
    uint32_t training_batch_size;               /**< Batch size for training */
    float training_priority_decay;              /**< Priority decay for old events */

    /* Registry Integration Settings */
    bool enable_registry_sync;                  /**< Sync with module registry */
    float registry_sync_interval_sec;           /**< Sync interval in seconds */

    /* Bio-async Settings */
    bool enable_bio_async;                      /**< Enable bio-async messaging */
} omni_wm_kg_bridge_config_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model KG Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, optimization
 * HOW:  Counters, averages, timing metrics
 */
typedef struct {
    /* Entity Prediction Statistics */
    uint64_t entity_predictions_made;           /**< Total entity predictions */
    uint64_t entity_predictions_correct;        /**< Correct predictions (verified) */
    float mean_entity_prediction_error;         /**< Average prediction error */
    float mean_entity_confidence;               /**< Average prediction confidence */

    /* Relationship Prediction Statistics */
    uint64_t relationship_predictions_made;     /**< Relationship predictions */
    uint64_t relationship_changes_predicted;    /**< Predicted changes */
    uint64_t relationship_changes_actual;       /**< Actual changes observed */

    /* Module Prediction Statistics */
    uint64_t module_predictions_made;           /**< Module health predictions */
    uint64_t module_failures_predicted;         /**< Predicted failures */
    uint64_t module_failures_actual;            /**< Actual failures observed */
    float mean_failure_lead_time_sec;           /**< Avg lead time for predictions */

    /* Anomaly Statistics */
    uint64_t anomalies_detected;                /**< Total anomalies detected */
    uint64_t anomalies_confirmed;               /**< Confirmed true anomalies */
    float mean_anomaly_score;                   /**< Average anomaly severity */

    /* Training Statistics */
    uint64_t kg_events_processed;               /**< KG events processed */
    uint64_t training_updates;                  /**< Training updates performed */
    float mean_training_loss;                   /**< Average training loss */

    /* Registry Statistics */
    uint64_t registry_syncs;                    /**< Registry synchronizations */
    uint32_t modules_tracked;                   /**< Modules being tracked */
    uint32_t modules_degraded;                  /**< Currently degraded modules */

    /* Timing Statistics */
    uint64_t total_updates;                     /**< Total update cycles */
    double total_processing_time_ms;            /**< Total processing time */
    double mean_update_time_ms;                 /**< Average update duration */
    uint64_t last_update_time_us;               /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;                      /**< Total errors encountered */
    uint64_t errors_prediction;                 /**< Prediction errors */
    uint64_t errors_training;                   /**< Training errors */
    uint64_t errors_registry;                   /**< Registry sync errors */
} omni_wm_kg_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model KG Bridge
 *
 * WHAT: Main bridge structure connecting WM with KG wiring system
 * WHY:  Orchestrates bidirectional information flow for semantic prediction
 * HOW:  Maintains connections, effects, prediction state, and statistics
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_kg_bridge {
    bridge_base_t base;                         /**< MUST be first: base infrastructure */

    /* Configuration */
    omni_wm_kg_bridge_config_t config;          /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;            /**< World model (RSSM) */
    kg_wiring_manager_t* kg_wiring;             /**< KG wiring manager */
    kg_module_registry_t* registry;             /**< Module registry */

    /* Bidirectional Effects */
    omni_wm_to_kg_effects_t wm_to_kg;           /**< Effects: WM -> KG */
    kg_to_omni_wm_effects_t kg_to_wm;           /**< Effects: KG -> WM */

    /* Prediction State */
    uint64_t last_prediction_time_us;           /**< Last prediction timestamp */
    uint32_t prediction_sequence;               /**< Prediction sequence number */
    bool prediction_in_progress;                /**< Prediction currently running */

    /* Training State */
    uint32_t training_events_queued;            /**< Events waiting for training */
    uint64_t last_training_time_us;             /**< Last training timestamp */

    /* Registry Sync State */
    uint64_t last_registry_sync_us;             /**< Last registry sync time */
    uint32_t registry_sync_failures;            /**< Consecutive sync failures */

    /* Statistics */
    omni_wm_kg_bridge_stats_t stats;            /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;          /**< Per-instance health agent */
} omni_wm_kg_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with reasonable values
 * HOW:  Sets all config fields to defaults
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_default_config(
    omni_wm_kg_bridge_config_t* config);

/**
 * @brief Create World Model KG Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_kg_bridge_t* omni_wm_kg_bridge_create(
    const omni_wm_kg_bridge_config_t* config);

/**
 * @brief Destroy World Model KG Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, free buffers, cleanup base
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_kg_bridge_destroy(omni_wm_kg_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Reset effects and statistics, keep connections
 * WHY:  Allow fresh start without reconnection
 * HOW:  Zero effects, reset stats, preserve config
 *
 * @param bridge Bridge to reset
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_reset(omni_wm_kg_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all KG systems to bridge
 *
 * WHAT: Establish connections to WM, KG wiring, and registry
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param kg_wiring KG wiring manager - optional
 * @param registry Module registry - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_connect(
    omni_wm_kg_bridge_t* bridge,
    omni_world_model_t* world_model,
    kg_wiring_manager_t* kg_wiring,
    kg_module_registry_t* registry);

/**
 * @brief Connect world model
 */
nimcp_error_t omni_wm_kg_bridge_connect_world_model(
    omni_wm_kg_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect KG wiring manager
 */
nimcp_error_t omni_wm_kg_bridge_connect_kg_wiring(
    omni_wm_kg_bridge_t* bridge,
    kg_wiring_manager_t* kg_wiring);

/**
 * @brief Connect module registry
 */
nimcp_error_t omni_wm_kg_bridge_connect_registry(
    omni_wm_kg_bridge_t* bridge,
    kg_module_registry_t* registry);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if world model connected (minimum requirement)
 */
bool omni_wm_kg_bridge_is_connected(const omni_wm_kg_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional information flow
 * WHY:  Called each timestep to sync WM and KG systems
 * HOW:  Gather KG context, compute predictions, update effects
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_update(
    omni_wm_kg_bridge_t* bridge,
    float dt);

/* ============================================================================
 * Entity Prediction API
 * ============================================================================ */

/**
 * @brief Predict future state of a KG entity
 *
 * WHAT: Use RSSM to predict entity state evolution
 * WHY:  Enable proactive system management
 * HOW:  Forward dynamics with entity context
 *
 * @param bridge Bridge instance
 * @param entity_id Entity to predict
 * @param horizon_steps Prediction horizon in steps
 * @param out_prediction Output prediction (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_predict_entity(
    omni_wm_kg_bridge_t* bridge,
    uint32_t entity_id,
    uint32_t horizon_steps,
    wm_to_kg_entity_prediction_t* out_prediction);

/**
 * @brief Predict batch of entity states
 *
 * @param bridge Bridge instance
 * @param entity_ids Array of entity IDs
 * @param entity_count Number of entities
 * @param horizon_steps Prediction horizon
 * @param out_predictions Output array (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_predict_entities_batch(
    omni_wm_kg_bridge_t* bridge,
    const uint32_t* entity_ids,
    uint32_t entity_count,
    uint32_t horizon_steps,
    wm_to_kg_entity_prediction_t* out_predictions);

/* ============================================================================
 * Relationship Prediction API
 * ============================================================================ */

/**
 * @brief Predict relationship changes for an entity
 *
 * WHAT: Estimate probability of relationship changes
 * WHY:  Anticipate graph topology evolution
 * HOW:  Model relationship dynamics from state trajectory
 *
 * @param bridge Bridge instance
 * @param entity_id Entity to analyze
 * @param out_change_probability Output: P(any change)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_predict_relationships(
    omni_wm_kg_bridge_t* bridge,
    uint32_t entity_id,
    float* out_change_probability);

/**
 * @brief Predict specific relationship strength evolution
 *
 * @param bridge Bridge instance
 * @param source_id Source entity
 * @param target_id Target entity
 * @param horizon_steps Prediction horizon
 * @param out_predicted_strength Output: predicted strength
 * @param out_confidence Output: prediction confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_predict_relationship_strength(
    omni_wm_kg_bridge_t* bridge,
    uint32_t source_id,
    uint32_t target_id,
    uint32_t horizon_steps,
    float* out_predicted_strength,
    float* out_confidence);

/* ============================================================================
 * Module Health Prediction API
 * ============================================================================ */

/**
 * @brief Predict module failure probability
 *
 * WHAT: Forecast module health degradation and failure
 * WHY:  Enable proactive maintenance and failover
 * HOW:  RSSM predicts health trajectory from current metrics
 *
 * @param bridge Bridge instance
 * @param module_id Module to predict
 * @param out_failure_probability Output: P(failure)
 * @param out_time_to_failure Output: estimated seconds to failure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_predict_module_failure(
    omni_wm_kg_bridge_t* bridge,
    uint32_t module_id,
    float* out_failure_probability,
    float* out_time_to_failure);

/**
 * @brief Get full failure prediction for module
 *
 * @param bridge Bridge instance
 * @param module_id Module to predict
 * @param out_prediction Output prediction (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_get_module_prediction(
    omni_wm_kg_bridge_t* bridge,
    uint32_t module_id,
    wm_to_kg_failure_prediction_t* out_prediction);

/**
 * @brief Predict system-wide stability
 *
 * @param bridge Bridge instance
 * @param out_stability_score Output: stability [0,1]
 * @param out_predicted_exceptions Output: expected exception count
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_predict_system_stability(
    omni_wm_kg_bridge_t* bridge,
    float* out_stability_score,
    uint32_t* out_predicted_exceptions);

/* ============================================================================
 * Exception Handling Integration API
 * ============================================================================ */

/**
 * @brief Notify bridge of KG exception
 *
 * WHAT: Forward exception to WM for pattern learning
 * WHY:  Train world model on exception patterns
 * HOW:  Add exception to training data, update anomaly model
 *
 * @param bridge Bridge instance
 * @param exception Exception that occurred
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_on_exception(
    omni_wm_kg_bridge_t* bridge,
    const nimcp_kg_wiring_exception_t* exception);

/**
 * @brief Check if entity state is anomalous
 *
 * @param bridge Bridge instance
 * @param entity_id Entity to check
 * @param out_is_anomalous Output: true if anomalous
 * @param out_anomaly_score Output: anomaly severity [0,1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_check_anomaly(
    omni_wm_kg_bridge_t* bridge,
    uint32_t entity_id,
    bool* out_is_anomalous,
    float* out_anomaly_score);

/* ============================================================================
 * Training API
 * ============================================================================ */

/**
 * @brief Train world model from KG event
 *
 * WHAT: Update RSSM parameters from observed KG event
 * WHY:  Improve predictions based on actual observations
 * HOW:  Add to training buffer, trigger batch training
 *
 * @param bridge Bridge instance
 * @param event KG event for training
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_train_from_kg_event(
    omni_wm_kg_bridge_t* bridge,
    const kg_event_t* event);

/**
 * @brief Train from entity state observation
 *
 * @param bridge Bridge instance
 * @param entity_id Entity observed
 * @param observed_state Observed state vector
 * @param state_dim State dimensionality
 * @param timestamp_us Observation timestamp
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_train_from_observation(
    omni_wm_kg_bridge_t* bridge,
    uint32_t entity_id,
    const float* observed_state,
    uint32_t state_dim,
    uint64_t timestamp_us);

/**
 * @brief Trigger batch training from queued events
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_flush_training(
    omni_wm_kg_bridge_t* bridge);

/* ============================================================================
 * Registry Sync API
 * ============================================================================ */

/**
 * @brief Sync with module registry
 *
 * WHAT: Update module states from registry
 * WHY:  Keep predictions informed by current health
 * HOW:  Query registry, update kg_to_wm effects
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_sync_registry(
    omni_wm_kg_bridge_t* bridge);

/**
 * @brief Update module health state
 *
 * @param bridge Bridge instance
 * @param module_id Module ID
 * @param health_state New health state
 * @param health_score Numeric health [0,1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_update_module_health(
    omni_wm_kg_bridge_t* bridge,
    uint32_t module_id,
    kg_module_health_t health_state,
    float health_score);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from WM to KG
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const omni_wm_to_kg_effects_t* omni_wm_kg_bridge_get_wm_effects(
    const omni_wm_kg_bridge_t* bridge);

/**
 * @brief Get current effects from KG to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const kg_to_omni_wm_effects_t* omni_wm_kg_bridge_get_kg_effects(
    const omni_wm_kg_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_get_stats(
    const omni_wm_kg_bridge_t* bridge,
    omni_wm_kg_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_reset_stats(
    omni_wm_kg_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_connect_bio_async(
    omni_wm_kg_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_kg_bridge_disconnect_bio_async(
    omni_wm_kg_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_kg_bridge_is_bio_async_connected(
    const omni_wm_kg_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_kg_msg_type_to_string(omni_wm_kg_msg_type_t msg_type);

/**
 * @brief Get relationship type name string
 *
 * @param rel_type Relationship type
 * @return Human-readable relationship name
 */
const char* omni_wm_kg_relationship_type_to_string(kg_relationship_type_t rel_type);

/**
 * @brief Get module health state name string
 *
 * @param health Health state
 * @return Human-readable health state name
 */
const char* omni_wm_kg_module_health_to_string(kg_module_health_t health);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_kg_bridge_validate_config(
    const omni_wm_kg_bridge_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_KG_BRIDGE_H */
