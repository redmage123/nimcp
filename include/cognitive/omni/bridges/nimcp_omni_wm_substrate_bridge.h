/**
 * @file nimcp_omni_wm_substrate_bridge.h
 * @brief World Model Substrate Bridge - Metabolic Constraints on World Model Computation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Neural Substrate
 * WHY:  Enable energy-aware world modeling with metabolic constraints
 * HOW:  Substrate state modulates prediction depth; WM signals computational demand
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * METABOLIC CONSTRAINTS ON PREDICTION (Hasenstaub et al., 2010):
 * --------------------------------------------------------------
 * Predictive coding requires energy for:
 *   1. Forward model computation (ATP for neural dynamics)
 *   2. Error computation and propagation (synaptic transmission)
 *   3. Weight updates (protein synthesis for learning)
 *
 * Under metabolic stress:
 *   - Prediction horizon shortens (fewer steps ahead)
 *   - Computation rate decreases (longer processing time)
 *   - Training/learning pauses (no plasticity without ATP)
 *
 * ENERGY BUDGET FOR WORLD MODELS (Attwell & Laughlin, 2001):
 * ----------------------------------------------------------
 * Neural computation costs:
 *   - Resting state: ~20% of body's O2/glucose for 2% of mass
 *   - Prediction: Additional 5-15% above resting
 *   - Learning: Additional 10-20% during plasticity
 *
 * OXYGEN-DEPENDENT COMPUTATION (Buzsaki et al., 2012):
 * ----------------------------------------------------
 * Oxygen availability directly affects:
 *   - Firing rate capacity (O2 needed for repolarization)
 *   - NMDA receptor function (hypoxia impairs LTP)
 *   - Mitochondrial ATP production (oxidative phosphorylation)
 *
 * GLUCOSE AND PREDICTION (Gold, 1995):
 * ------------------------------------
 * Glucose availability affects:
 *   - Working memory span (prediction buffer size)
 *   - Training capacity (protein synthesis requires glucose)
 *   - Cognitive flexibility (metabolic support for state switching)
 *
 * DATA FLOW:
 * ----------
 *   Substrate -> WM: ATP/O2/glucose availability, metabolic capacity
 *   WM -> Substrate: Computational demand signals, energy consumption rate
 *   Metabolic Stress -> WM: Reduce prediction horizon, slow computation
 *   Low ATP -> WM: Pause training, use simplified dynamics
 *   Low O2 -> WM: Reduce computation rate, shorter rollouts
 *   Low Glucose -> WM: Disable learning, use cached predictions
 *
 * INTEGRATION POINTS:
 * -------------------
 *   - Neural Substrate (nimcp_neural_substrate.h): ATP, O2, glucose, temperature
 *   - Metabolic Plasticity (nimcp_metabolic_plasticity.h): Energy for learning
 *   - World Model (nimcp_omni_world_model.h): RSSM, predictions, training
 *
 * BIO-ASYNC:
 *   Module ID: 0x0E69
 *   Message Range: 0x6900-0x69FF
 */

#ifndef NIMCP_OMNI_WM_SUBSTRATE_BRIDGE_H
#define NIMCP_OMNI_WM_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
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

/* Neural Substrate (from nimcp_neural_substrate.h) */
typedef struct neural_substrate neural_substrate_t;

/* Metabolic Plasticity (from nimcp_metabolic_plasticity.h) */
typedef struct metabolic_plasticity metabolic_plasticity_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Bio-async module ID for World Model Substrate Bridge */
#define BIO_MODULE_WM_SUBSTRATE_BRIDGE      0x0E69

/** Maximum prediction horizon when fully resourced */
#define WM_SUBSTRATE_MAX_HORIZON            32

/** Minimum prediction horizon under metabolic stress */
#define WM_SUBSTRATE_MIN_HORIZON            4

/** Default computation rate (predictions per second) */
#define WM_SUBSTRATE_DEFAULT_COMPUTE_RATE   100.0f

/** Critical ATP threshold for training */
#define WM_SUBSTRATE_ATP_TRAIN_THRESHOLD    0.5f

/** Critical ATP threshold for prediction */
#define WM_SUBSTRATE_ATP_PREDICT_THRESHOLD  0.3f

/** Critical O2 threshold for computation */
#define WM_SUBSTRATE_O2_CRITICAL            0.5f

/** Critical glucose threshold for learning */
#define WM_SUBSTRATE_GLUCOSE_LEARN_THRESHOLD 0.4f

/** Temperature coefficient (Q10 for world model) */
#define WM_SUBSTRATE_Q10_COMPUTATION        2.3f

/** Normal operating temperature (Celsius) */
#define WM_SUBSTRATE_NORMAL_TEMP            37.0f

/** Metabolic state logging interval (milliseconds) */
#define WM_SUBSTRATE_LOG_INTERVAL_MS        1000

/* ============================================================================
 * Bio-Async Message Types (0x6900-0x69FF)
 *
 * NOTE: Core message types are defined in async/nimcp_bio_messages.h:
 *   BIO_MSG_WM_SUBSTRATE_METABOLIC = 0x6900
 *   BIO_MSG_WM_SUBSTRATE_DEMAND
 *   BIO_MSG_WM_SUBSTRATE_CONSTRAINT = 0x6910
 *   BIO_MSG_WM_SUBSTRATE_HORIZON_ADJUST
 *
 * Extended message types defined here for additional bridge functionality.
 * ============================================================================ */

/** Extended message types for WM Substrate Bridge (0x6920-0x69FF) */
typedef enum {
    /* Alert Messages (0x6930-0x693F) - extends bio_messages.h definitions */
    BIO_MSG_WM_SUBSTRATE_ALERT_LOW_ATP   = 0x6930,  /**< Low ATP alert */
    BIO_MSG_WM_SUBSTRATE_ALERT_HYPOXIA   = 0x6931,  /**< Hypoxia alert */
    BIO_MSG_WM_SUBSTRATE_ALERT_LOW_GLUCOSE = 0x6932, /**< Low glucose alert */
    BIO_MSG_WM_SUBSTRATE_ALERT_HYPERTHERMIA = 0x6933, /**< Hyperthermia alert */
    BIO_MSG_WM_SUBSTRATE_ALERT_RESOLVED  = 0x6934,  /**< Alert condition resolved */

    /* Bridge Status Messages (0x6940-0x694F) */
    BIO_MSG_WM_SUBSTRATE_BRIDGE_STATUS   = 0x6940,  /**< Bridge status update */
    BIO_MSG_WM_SUBSTRATE_BRIDGE_ERROR    = 0x6941,  /**< Bridge error notification */
    BIO_MSG_WM_SUBSTRATE_STATS_UPDATE    = 0x6942   /**< Statistics update */
} omni_wm_substrate_msg_type_t;

/* ============================================================================
 * Metabolic State Structures
 * ============================================================================ */

/**
 * @brief Current metabolic availability for world model
 *
 * WHAT: Snapshot of substrate resources available for computation
 * WHY:  WM adapts behavior based on available resources
 * HOW:  Normalized values [0-1] for each resource type
 */
typedef struct {
    /* Primary Resources */
    float atp_level;                /**< ATP availability [0-1] */
    float oxygen_saturation;        /**< O2 saturation [0-1] */
    float glucose_level;            /**< Glucose availability [0-1] */
    float temperature;              /**< Current temperature (Celsius) */

    /* Derived Metrics */
    float metabolic_capacity;       /**< Combined metabolic score [0-1] */
    float computational_capacity;   /**< Capacity for computation [0-1] */
    float learning_capacity;        /**< Capacity for training [0-1] */

    /* Rate Metrics */
    float metabolic_rate;           /**< Current consumption rate */
    float recovery_rate;            /**< Current recovery rate */

    /* Health Classification */
    uint32_t health_level;          /**< Substrate health level enum */
    bool is_stressed;               /**< True if any resource critical */
    bool is_critical;               /**< True if computation impaired */
} wm_metabolic_availability_t;

/**
 * @brief World Model computational demand
 *
 * WHAT: Signal from WM to substrate about resource requirements
 * WHY:  Substrate can anticipate and prepare for demand
 * HOW:  Normalized demand levels and specific requirements
 */
typedef struct {
    /* Demand Levels [0-1] */
    float prediction_demand;        /**< Demand from prediction operations */
    float training_demand;          /**< Demand from learning/training */
    float rollout_demand;           /**< Demand from policy rollouts */
    float overall_demand;           /**< Combined computational demand */

    /* Specific Requirements */
    uint32_t requested_horizon;     /**< Desired prediction horizon */
    float requested_compute_rate;   /**< Desired predictions per second */
    bool training_active;           /**< Is training currently active */
    bool dreaming_active;           /**< Is offline simulation active */

    /* Consumption Metrics */
    float estimated_atp_per_sec;    /**< Estimated ATP consumption rate */
    float estimated_o2_per_sec;     /**< Estimated O2 consumption rate */
    float estimated_glucose_per_sec; /**< Estimated glucose consumption rate */
} wm_computational_demand_t;

/* ============================================================================
 * Effect Structures
 * ============================================================================ */

/**
 * @brief Effects from Substrate to World Model
 *
 * WHAT: Metabolic constraints affecting WM operation
 * WHY:  WM must adapt to available resources
 * HOW:  Modulation factors and operational limits
 */
typedef struct {
    /* Prediction Modulation */
    uint32_t allowed_horizon;       /**< Maximum allowed prediction horizon */
    float horizon_scale;            /**< Horizon scaling factor [0-1] */
    float compute_rate_scale;       /**< Computation rate scaling [0-1] */
    float prediction_confidence_mod; /**< Confidence modulation [0-1] */

    /* Training Constraints */
    bool training_permitted;        /**< Whether training is allowed */
    float learning_rate_scale;      /**< Learning rate scaling [0-1] */
    bool dreaming_permitted;        /**< Whether offline simulation allowed */
    float dream_length_scale;       /**< Dream episode length scaling [0-1] */

    /* Computation Adjustments */
    bool use_simplified_dynamics;   /**< Use energy-efficient dynamics */
    bool use_cached_predictions;    /**< Prefer cached over fresh predictions */
    float update_interval_scale;    /**< Update interval scaling [1-N] */

    /* Temperature Effects (Q10) */
    float temperature_mod;          /**< Temperature-based rate modulation */
    float q10_factor;               /**< Current Q10 scaling factor */

    /* Alert Flags */
    bool low_atp_alert;             /**< ATP critically low */
    bool hypoxia_alert;             /**< Oxygen critically low */
    bool low_glucose_alert;         /**< Glucose critically low */
    bool hyperthermia_alert;        /**< Temperature too high */
} substrate_to_wm_effects_t;

/**
 * @brief Effects from World Model to Substrate
 *
 * WHAT: WM's impact on metabolic resources
 * WHY:  Substrate tracks resource consumption
 * HOW:  Consumption rates and demand signals
 */
typedef struct {
    /* Current Consumption */
    float atp_consumption_rate;     /**< Current ATP consumption (units/sec) */
    float o2_consumption_rate;      /**< Current O2 consumption (units/sec) */
    float glucose_consumption_rate; /**< Current glucose consumption (units/sec) */

    /* Cumulative Consumption */
    float total_atp_consumed;       /**< Total ATP consumed this session */
    float total_o2_consumed;        /**< Total O2 consumed this session */
    float total_glucose_consumed;   /**< Total glucose consumed this session */

    /* Activity Metrics */
    uint64_t predictions_made;      /**< Predictions since last update */
    uint64_t training_steps;        /**< Training steps since last update */
    uint64_t rollouts_completed;    /**< Rollouts since last update */

    /* Demand Signal */
    wm_computational_demand_t demand; /**< Current demand state */

    /* Efficiency Metrics */
    float energy_efficiency;        /**< Predictions per ATP unit */
    float computation_efficiency;   /**< Useful work per energy */
} wm_to_substrate_effects_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief World Model Substrate Bridge configuration
 *
 * WHAT: Parameters controlling WM-Substrate integration
 * WHY:  Tune metabolic constraints and modulation behavior
 * HOW:  Thresholds, scaling factors, and feature flags
 */
typedef struct {
    /* General Settings */
    bool enable_modulation;         /**< Enable metabolic modulation */
    float sensitivity;              /**< General sensitivity [0.5-2.0] */

    /* ATP Modulation Settings */
    bool enable_atp_modulation;     /**< Modulate WM based on ATP */
    float atp_training_threshold;   /**< ATP threshold for training [0-1] */
    float atp_prediction_threshold; /**< ATP threshold for prediction [0-1] */
    float atp_critical_threshold;   /**< Critical ATP level [0-1] */

    /* Oxygen Modulation Settings */
    bool enable_o2_modulation;      /**< Modulate WM based on O2 */
    float o2_critical_threshold;    /**< Critical O2 level [0-1] */
    float o2_compute_scale;         /**< O2 impact on compute rate [0-1] */

    /* Glucose Modulation Settings */
    bool enable_glucose_modulation; /**< Modulate WM based on glucose */
    float glucose_learning_threshold; /**< Glucose threshold for learning [0-1] */

    /* Temperature Settings */
    bool enable_temperature_effects; /**< Apply Q10 temperature effects */
    float q10_coefficient;          /**< Q10 temperature coefficient */
    float normal_temperature;       /**< Normal operating temp (Celsius) */
    float hyperthermia_threshold;   /**< High temp threshold (Celsius) */

    /* Horizon Settings */
    uint32_t max_horizon;           /**< Maximum prediction horizon */
    uint32_t min_horizon;           /**< Minimum prediction horizon */
    bool adaptive_horizon;          /**< Dynamically adjust horizon */

    /* Computation Rate Settings */
    float base_compute_rate;        /**< Base predictions per second */
    float min_compute_rate;         /**< Minimum compute rate */
    bool adaptive_rate;             /**< Dynamically adjust rate */

    /* Energy Cost Settings */
    float atp_per_prediction;       /**< ATP cost per prediction step */
    float atp_per_training_step;    /**< ATP cost per training step */
    float atp_per_rollout_step;     /**< ATP cost per rollout step */

    /* Logging Settings */
    bool enable_metabolic_logging;  /**< Log metabolic state */
    uint32_t log_interval_ms;       /**< Logging interval (milliseconds) */

    /* Bio-async Settings */
    bool enable_bio_async;          /**< Enable bio-async messaging */
} omni_wm_substrate_bridge_config_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief World Model Substrate Bridge statistics
 *
 * WHAT: Metrics for monitoring bridge operation
 * WHY:  Performance tracking, debugging, and optimization
 * HOW:  Counters, averages, and event tracking
 */
typedef struct {
    /* Constraint Statistics */
    uint64_t horizon_reductions;    /**< Times horizon was reduced */
    uint64_t rate_reductions;       /**< Times compute rate was reduced */
    uint64_t training_pauses;       /**< Times training was paused */
    uint64_t training_resumes;      /**< Times training was resumed */
    uint64_t simplified_dynamics_activations; /**< Times simplified mode used */

    /* Alert Statistics */
    uint64_t low_atp_alerts;        /**< Low ATP alert count */
    uint64_t hypoxia_alerts;        /**< Hypoxia alert count */
    uint64_t low_glucose_alerts;    /**< Low glucose alert count */
    uint64_t hyperthermia_alerts;   /**< Hyperthermia alert count */
    uint64_t alerts_resolved;       /**< Total alerts resolved */

    /* Resource Consumption */
    float total_atp_consumed;       /**< Total ATP consumed */
    float total_o2_consumed;        /**< Total O2 consumed */
    float total_glucose_consumed;   /**< Total glucose consumed */
    float peak_consumption_rate;    /**< Peak consumption rate */

    /* Efficiency Metrics */
    float mean_energy_efficiency;   /**< Average energy efficiency */
    float mean_horizon_achieved;    /**< Average prediction horizon */
    float mean_compute_rate;        /**< Average computation rate */

    /* Capacity Tracking */
    float min_atp_observed;         /**< Minimum ATP level seen */
    float min_o2_observed;          /**< Minimum O2 level seen */
    float min_glucose_observed;     /**< Minimum glucose level seen */
    float max_temp_observed;        /**< Maximum temperature seen */

    /* Timing Statistics */
    uint64_t total_updates;         /**< Total update cycles */
    double total_processing_time_ms; /**< Total processing time */
    double mean_update_time_ms;     /**< Average update duration */
    uint64_t last_update_time_us;   /**< Last update timestamp */

    /* Error Statistics */
    uint64_t errors_total;          /**< Total errors encountered */
    uint64_t errors_metabolic;      /**< Metabolic-related errors */
    uint64_t errors_constraint;     /**< Constraint-related errors */
} omni_wm_substrate_bridge_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief World Model Substrate Bridge
 *
 * WHAT: Main bridge structure connecting WM with neural substrate
 * WHY:  Orchestrates bidirectional metabolic information flow
 * HOW:  Maintains connections, effects, and metabolic state
 *
 * Memory Layout:
 *   bridge_base_t base MUST be first for pointer casting compatibility
 */
typedef struct omni_wm_substrate_bridge {
    bridge_base_t base;             /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    omni_wm_substrate_bridge_config_t config; /**< Bridge configuration */

    /* Connected Systems */
    omni_world_model_t* world_model;     /**< World model (RSSM) */
    neural_substrate_t* substrate;        /**< Neural substrate */
    metabolic_plasticity_t* metabolic;    /**< Metabolic plasticity system */

    /* Current Metabolic State */
    wm_metabolic_availability_t availability; /**< Current resource availability */

    /* Bidirectional Effects */
    substrate_to_wm_effects_t substrate_effects; /**< Effects: Substrate -> WM */
    wm_to_substrate_effects_t wm_effects;        /**< Effects: WM -> Substrate */

    /* Operational State */
    bool training_was_active;       /**< Training state before pause */
    uint32_t current_horizon;       /**< Current effective horizon */
    float current_compute_rate;     /**< Current effective compute rate */
    bool is_constrained;            /**< Currently under metabolic constraint */
    uint32_t active_alerts;         /**< Bitmask of active alerts */

    /* Logging State */
    uint64_t last_log_time_us;      /**< Last metabolic log timestamp */

    /* Statistics */
    omni_wm_substrate_bridge_stats_t stats; /**< Bridge statistics */

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;      /**< Per-instance health agent */
} omni_wm_substrate_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible defaults for bridge configuration
 * WHY:  Convenient initialization with biologically-plausible values
 * HOW:  Sets all config fields to evidence-based defaults
 *
 * @param config Configuration structure to initialize
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_default_config(
    omni_wm_substrate_bridge_config_t* config);

/**
 * @brief Create World Model Substrate Bridge
 *
 * WHAT: Allocate and initialize bridge
 * WHY:  Required before connecting systems
 * HOW:  Allocate structure, initialize base, set config
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
omni_wm_substrate_bridge_t* omni_wm_substrate_bridge_create(
    const omni_wm_substrate_bridge_config_t* config);

/**
 * @brief Destroy World Model Substrate Bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource management
 * HOW:  Disconnect systems, cleanup base, free memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void omni_wm_substrate_bridge_destroy(omni_wm_substrate_bridge_t* bridge);

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
nimcp_error_t omni_wm_substrate_bridge_reset(omni_wm_substrate_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect all systems to bridge
 *
 * WHAT: Establish connections to WM, substrate, and metabolic systems
 * WHY:  Single call to wire up all systems
 * HOW:  Store pointers, validate connections, activate bridge
 *
 * @param bridge Bridge instance
 * @param world_model World model (RSSM) - required
 * @param substrate Neural substrate - required
 * @param metabolic Metabolic plasticity - optional
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_connect(
    omni_wm_substrate_bridge_t* bridge,
    omni_world_model_t* world_model,
    neural_substrate_t* substrate,
    metabolic_plasticity_t* metabolic);

/**
 * @brief Connect world model
 *
 * @param bridge Bridge instance
 * @param world_model World model to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_connect_world_model(
    omni_wm_substrate_bridge_t* bridge,
    omni_world_model_t* world_model);

/**
 * @brief Connect neural substrate
 *
 * @param bridge Bridge instance
 * @param substrate Neural substrate to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_connect_substrate(
    omni_wm_substrate_bridge_t* bridge,
    neural_substrate_t* substrate);

/**
 * @brief Connect metabolic plasticity system
 *
 * @param bridge Bridge instance
 * @param metabolic Metabolic plasticity to connect
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_connect_metabolic(
    omni_wm_substrate_bridge_t* bridge,
    metabolic_plasticity_t* metabolic);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge to check
 * @return true if WM and substrate connected (minimum requirement)
 */
bool omni_wm_substrate_bridge_is_connected(const omni_wm_substrate_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Main update cycle
 *
 * WHAT: Process bidirectional metabolic information flow
 * WHY:  Called each timestep to sync WM and substrate
 * HOW:  Read substrate state, compute constraints, apply to WM
 *
 * @param bridge Bridge instance
 * @param dt Time delta in seconds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_update(
    omni_wm_substrate_bridge_t* bridge,
    float dt);

/**
 * @brief Force metabolic state refresh
 *
 * WHAT: Immediately update metabolic availability from substrate
 * WHY:  Use when metabolic state may have changed significantly
 * HOW:  Query substrate, recompute all derived metrics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_refresh_metabolic_state(
    omni_wm_substrate_bridge_t* bridge);

/* ============================================================================
 * Metabolic Constraint API
 * ============================================================================ */

/**
 * @brief Get current metabolic availability
 *
 * WHAT: Query current resource availability for WM
 * WHY:  WM components can check resources before operations
 * HOW:  Return current availability structure
 *
 * @param bridge Bridge instance
 * @param availability Output: current availability (caller allocates)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_get_availability(
    const omni_wm_substrate_bridge_t* bridge,
    wm_metabolic_availability_t* availability);

/**
 * @brief Check if training is permitted
 *
 * WHAT: Quick check if WM training is allowed
 * WHY:  Avoid expensive training setup if energy insufficient
 * HOW:  Check ATP and glucose against thresholds
 *
 * @param bridge Bridge instance
 * @return true if training is permitted
 */
bool omni_wm_substrate_bridge_can_train(const omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Check if prediction is permitted
 *
 * WHAT: Quick check if WM prediction is allowed
 * WHY:  Avoid prediction if critically depleted
 * HOW:  Check ATP against critical threshold
 *
 * @param bridge Bridge instance
 * @return true if prediction is permitted
 */
bool omni_wm_substrate_bridge_can_predict(const omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Get allowed prediction horizon
 *
 * WHAT: Get maximum allowed prediction steps
 * WHY:  WM should not exceed metabolically-allowed horizon
 * HOW:  Scale max horizon by metabolic capacity
 *
 * @param bridge Bridge instance
 * @return Maximum allowed prediction horizon (steps)
 */
uint32_t omni_wm_substrate_bridge_get_allowed_horizon(
    const omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Get allowed computation rate
 *
 * WHAT: Get maximum allowed predictions per second
 * WHY:  WM should not exceed metabolically-allowed rate
 * HOW:  Scale base rate by O2 and temperature factors
 *
 * @param bridge Bridge instance
 * @return Maximum allowed computation rate (predictions/second)
 */
float omni_wm_substrate_bridge_get_allowed_compute_rate(
    const omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Get learning rate scale
 *
 * WHAT: Get scaling factor for WM learning rate
 * WHY:  Reduce learning when energy is limited
 * HOW:  Scale by ATP and glucose availability
 *
 * @param bridge Bridge instance
 * @return Learning rate scaling factor [0-1]
 */
float omni_wm_substrate_bridge_get_learning_rate_scale(
    const omni_wm_substrate_bridge_t* bridge);

/* ============================================================================
 * Demand Signaling API
 * ============================================================================ */

/**
 * @brief Signal computational demand to substrate
 *
 * WHAT: Notify substrate of WM's resource requirements
 * WHY:  Substrate can anticipate demand and allocate resources
 * HOW:  Update demand structure, optionally send bio-async message
 *
 * @param bridge Bridge instance
 * @param demand Computational demand signal
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_signal_demand(
    omni_wm_substrate_bridge_t* bridge,
    const wm_computational_demand_t* demand);

/**
 * @brief Report prediction activity
 *
 * WHAT: Report predictions made for energy accounting
 * WHY:  Track consumption for constraint calculation
 * HOW:  Accumulate prediction count, update consumption
 *
 * @param bridge Bridge instance
 * @param num_predictions Number of predictions made
 * @param horizon Average prediction horizon
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_report_predictions(
    omni_wm_substrate_bridge_t* bridge,
    uint32_t num_predictions,
    uint32_t horizon);

/**
 * @brief Report training activity
 *
 * WHAT: Report training steps for energy accounting
 * WHY:  Track consumption, may trigger pause if depleted
 * HOW:  Accumulate training count, check thresholds
 *
 * @param bridge Bridge instance
 * @param num_steps Number of training steps
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_report_training(
    omni_wm_substrate_bridge_t* bridge,
    uint32_t num_steps);

/**
 * @brief Report rollout activity
 *
 * WHAT: Report policy rollouts for energy accounting
 * WHY:  Rollouts are expensive, need careful tracking
 * HOW:  Accumulate rollout count, update consumption
 *
 * @param bridge Bridge instance
 * @param num_rollouts Number of rollouts completed
 * @param total_steps Total steps across all rollouts
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_report_rollouts(
    omni_wm_substrate_bridge_t* bridge,
    uint32_t num_rollouts,
    uint32_t total_steps);

/* ============================================================================
 * Alert Management API
 * ============================================================================ */

/**
 * @brief Check for active metabolic alerts
 *
 * WHAT: Get bitmask of currently active alerts
 * WHY:  WM can adjust behavior based on active alerts
 * HOW:  Return alert bitmask from substrate monitoring
 *
 * @param bridge Bridge instance
 * @return Bitmask of active alerts
 */
uint32_t omni_wm_substrate_bridge_get_active_alerts(
    const omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Check if any alert is active
 *
 * @param bridge Bridge instance
 * @return true if any metabolic alert is active
 */
bool omni_wm_substrate_bridge_has_alert(const omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Check specific alert type
 *
 * @param bridge Bridge instance
 * @param alert_bit Alert bit to check (use message types as flags)
 * @return true if specific alert is active
 */
bool omni_wm_substrate_bridge_check_alert(
    const omni_wm_substrate_bridge_t* bridge,
    uint32_t alert_bit);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current effects from substrate to WM
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const substrate_to_wm_effects_t* omni_wm_substrate_bridge_get_substrate_effects(
    const omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Get current effects from WM to substrate
 *
 * @param bridge Bridge instance
 * @return Pointer to effects structure (do not free)
 */
const wm_to_substrate_effects_t* omni_wm_substrate_bridge_get_wm_effects(
    const omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge instance
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_get_stats(
    const omni_wm_substrate_bridge_t* bridge,
    omni_wm_substrate_bridge_stats_t* stats);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_reset_stats(
    omni_wm_substrate_bridge_t* bridge);

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_connect_bio_async(
    omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async router
 *
 * @param bridge Bridge instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t omni_wm_substrate_bridge_disconnect_bio_async(
    omni_wm_substrate_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Bridge instance
 * @return true if connected to bio-async router
 */
bool omni_wm_substrate_bridge_is_bio_async_connected(
    const omni_wm_substrate_bridge_t* bridge);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get message type name string
 *
 * @param msg_type Message type
 * @return Human-readable message name
 */
const char* omni_wm_substrate_msg_type_to_string(omni_wm_substrate_msg_type_t msg_type);

/**
 * @brief Validate bridge configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS or error code describing issue
 */
nimcp_error_t omni_wm_substrate_bridge_validate_config(
    const omni_wm_substrate_bridge_config_t* config);

/**
 * @brief Compute Q10 temperature factor
 *
 * WHAT: Calculate rate scaling based on temperature
 * WHY:  Biological rates scale with Q10 temperature coefficient
 * HOW:  Q10 formula: rate_scale = Q10^((T - T_normal) / 10)
 *
 * @param temperature Current temperature (Celsius)
 * @param normal_temp Normal temperature (Celsius)
 * @param q10 Q10 coefficient
 * @return Rate scaling factor
 */
float omni_wm_substrate_compute_q10_factor(
    float temperature,
    float normal_temp,
    float q10);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OMNI_WM_SUBSTRATE_BRIDGE_H */
