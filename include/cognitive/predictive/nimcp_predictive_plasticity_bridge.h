/**
 * @file nimcp_predictive_plasticity_bridge.h
 * @brief Predictive Processing - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between predictive coding and synaptic plasticity
 * WHY:  Enable learning of prediction models from experience and feedback
 * HOW:  STDP for prediction-outcome associations, BCM for stabilization, reward
 *       modulation for model-based learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): Free energy minimization through learning
 * - Rao & Ballard (1999): Predictive coding and Hebbian learning
 * - Bastos et al. (2012): Canonical microcircuits and plasticity
 *
 * BIOLOGICAL BASIS:
 * - Cortical plasticity for predictive model updating
 * - Prediction error-driven synaptic modification
 * - Precision-weighted learning rates
 * - Hierarchical model consolidation
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of prediction-outcome pairs
 * - BCM: Stabilize core predictive patterns
 * - Homeostatic: Maintain balanced prediction levels
 * - Error-modulated: Learn from prediction error success
 *
 * @see nimcp_predictive.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_PREDICTIVE_PLASTICITY_BRIDGE_H
#define NIMCP_PREDICTIVE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum predictive synapses */
#define PREDICTIVE_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define PREDICTIVE_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_PREDICTIVE_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Predictive synapse types
 */
typedef enum {
    PREDICTIVE_SYNAPSE_PREDICTION = 0,   /**< Prediction generation */
    PREDICTIVE_SYNAPSE_ERROR,            /**< Error propagation (PROTECTED) */
    PREDICTIVE_SYNAPSE_PRECISION,        /**< Precision weighting */
    PREDICTIVE_SYNAPSE_ANTICIPATION,     /**< Temporal anticipation */
    PREDICTIVE_SYNAPSE_MODEL,            /**< Model state */
    PREDICTIVE_SYNAPSE_HIERARCHY         /**< Hierarchical connection (PROTECTED) */
} predictive_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    PREDICTIVE_LEARN_PREDICTION_CONFIRMED = 0, /**< Prediction was accurate */
    PREDICTIVE_LEARN_PREDICTION_VIOLATED,      /**< Prediction error occurred */
    PREDICTIVE_LEARN_ERROR_MINIMIZED,          /**< Error successfully reduced */
    PREDICTIVE_LEARN_ERROR_PERSISTED,          /**< Error remained high */
    PREDICTIVE_LEARN_MODEL_UPDATED,            /**< Internal model updated */
    PREDICTIVE_LEARN_MODEL_STABLE,             /**< Model unchanged */
    PREDICTIVE_LEARN_PRECISION_INCREASED,      /**< Confidence improved */
    PREDICTIVE_LEARN_ANTICIPATION_CORRECT,     /**< Temporal anticipation matched */
    PREDICTIVE_LEARN_ANTICIPATION_WRONG,       /**< Temporal anticipation missed */
    PREDICTIVE_LEARN_FREE_ENERGY_REDUCED       /**< Free energy decreased */
} predictive_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    PREDICTIVE_PLASTICITY_STATE_IDLE = 0,
    PREDICTIVE_PLASTICITY_STATE_LEARNING,
    PREDICTIVE_PLASTICITY_STATE_CONSOLIDATING,
    PREDICTIVE_PLASTICITY_STATE_UPDATING,
    PREDICTIVE_PLASTICITY_STATE_ERROR
} predictive_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Predictive-Plasticity bridge configuration
 */
typedef struct {
    /* Learning parameters */
    float base_learning_rate;            /**< Base learning rate */
    float stdp_tau_plus_ms;              /**< STDP potentiation time constant */
    float stdp_tau_minus_ms;             /**< STDP depression time constant */
    float stdp_a_plus;                   /**< STDP potentiation magnitude */
    float stdp_a_minus;                  /**< STDP depression magnitude */

    /* BCM parameters */
    float bcm_tau_ms;                    /**< BCM threshold time constant */
    float bcm_target_rate;               /**< BCM target activity */

    /* Homeostatic parameters */
    float homeostatic_tau_ms;            /**< Homeostatic time constant */
    float target_prediction;             /**< Target prediction level */

    /* Error modulation */
    float error_reduction_boost;         /**< Boost for error reduction */
    float model_update_boost;            /**< Boost for model updates */
    float precision_modulation;          /**< Precision learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_error_pathways;         /**< Protect error propagation weights */
    bool protect_hierarchy;              /**< Protect hierarchical weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} predictive_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Predictive synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    predictive_synapse_type_t type;      /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} predictive_plasticity_synapse_t;

/**
 * @brief Model learning state
 */
typedef struct {
    float prediction_accuracy;           /**< Prediction accuracy */
    float model_calibration;             /**< Model calibration level */
    float error_sensitivity;             /**< Sensitivity to errors */
    float precision_strength;            /**< Precision weighting strength */
    float anticipation_strength;         /**< Anticipation strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} predictive_model_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    predictive_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} predictive_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t prediction_confirmed_events;/**< Prediction confirmed events */
    uint64_t prediction_violated_events; /**< Prediction violation corrections */
    uint64_t error_minimized_events;     /**< Error minimized events */
    uint64_t model_update_events;        /**< Model update learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} predictive_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct predictive_plasticity_bridge predictive_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*predictive_plasticity_learn_callback_t)(
    predictive_plasticity_bridge_t* bridge,
    predictive_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Model update callback */
typedef void (*predictive_plasticity_model_callback_t)(
    predictive_plasticity_bridge_t* bridge,
    float old_prediction,
    float new_prediction,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
predictive_plasticity_config_t predictive_plasticity_config_default(void);

/**
 * @brief Create predictive plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
predictive_plasticity_bridge_t* predictive_plasticity_create(
    const predictive_plasticity_config_t* config
);

/**
 * @brief Destroy predictive plasticity bridge
 * @param bridge Bridge to destroy
 */
void predictive_plasticity_destroy(predictive_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_reset(predictive_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register a synapse for plasticity tracking
 * @param bridge Bridge handle
 * @param synapse_id Unique synapse ID
 * @param type Synapse type
 * @param initial_weight Initial weight
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_register_synapse(
    predictive_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    predictive_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_unregister_synapse(
    predictive_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_get_synapse(
    predictive_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    predictive_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_protect_synapse(
    predictive_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    bool protect
);

//=============================================================================
// Learning Functions
//=============================================================================

/**
 * @brief Apply learning event
 * @param bridge Bridge handle
 * @param event Event type
 * @param magnitude Event magnitude [0-1]
 * @param synapse_id Target synapse
 * @param context Context strength
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_learn(
    predictive_plasticity_bridge_t* bridge,
    predictive_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float context
);

/**
 * @brief Apply STDP to synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param pre_time Pre-synaptic spike time (ms)
 * @param post_time Post-synaptic spike time (ms)
 * @return Weight change, NAN on failure
 */
float predictive_plasticity_apply_stdp(
    predictive_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply error modulation
 * @param bridge Bridge handle
 * @param error Error signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_apply_error(
    predictive_plasticity_bridge_t* bridge,
    float error
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_update_bcm(
    predictive_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_homeostatic_update(
    predictive_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_update_traces(
    predictive_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_consolidate(predictive_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get model state
 * @param bridge Bridge handle
 * @param state Output model state
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_get_model_state(
    predictive_plasticity_bridge_t* bridge,
    predictive_model_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_get_state(
    predictive_plasticity_bridge_t* bridge,
    predictive_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_get_stats(
    predictive_plasticity_bridge_t* bridge,
    predictive_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_reset_stats(predictive_plasticity_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register learning event callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_register_learn_callback(
    predictive_plasticity_bridge_t* bridge,
    predictive_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register model update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_register_model_callback(
    predictive_plasticity_bridge_t* bridge,
    predictive_plasticity_model_callback_t callback,
    void* user_data
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_bio_async_connect(predictive_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int predictive_plasticity_bio_async_disconnect(predictive_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool predictive_plasticity_is_bio_async_connected(predictive_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_PLASTICITY_BRIDGE_H */
