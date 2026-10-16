/**
 * @file nimcp_fep_plasticity_bridge.h
 * @brief Free Energy Principle - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between FEP engine and synaptic plasticity
 * WHY:  Enable learning of generative models from prediction errors and beliefs
 * HOW:  STDP for prediction-outcome associations, BCM for belief stabilization,
 *       precision-weighted learning for model parameter updates
 *
 * THEORETICAL FOUNDATIONS:
 * - Friston (2010): FEP and hierarchical predictive coding
 * - Bogacz (2017): A tutorial on the free-energy framework for perception
 * - Bastos et al. (2012): Canonical microcircuits for predictive coding
 *
 * BIOLOGICAL BASIS:
 * - NMDA-dependent plasticity for precision estimation
 * - Dopaminergic modulation for prediction error signaling
 * - Dendritic processing for belief-prediction comparison
 * - Cortical oscillations for hierarchical message passing
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of prediction-outcome pairs
 * - BCM: Stabilize generative model parameters
 * - Homeostatic: Maintain balanced free energy levels
 * - Precision-modulated: Learn from weighted prediction errors
 *
 * @see nimcp_free_energy.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_FEP_PLASTICITY_BRIDGE_H
#define NIMCP_FEP_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum FEP synapses */
#define FEP_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define FEP_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_FEP_PLASTICITY     0x0E51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief FEP synapse types
 */
typedef enum {
    FEP_SYNAPSE_PREDICTION = 0,          /**< Prediction generation (PROTECTED) */
    FEP_SYNAPSE_PRECISION,               /**< Precision weighting */
    FEP_SYNAPSE_BELIEF,                  /**< Belief representation */
    FEP_SYNAPSE_ERROR,                   /**< Prediction error */
    FEP_SYNAPSE_ACTIVE_INFERENCE,        /**< Active inference (PROTECTED) */
    FEP_SYNAPSE_VARIATIONAL              /**< Variational inference */
} fep_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    FEP_LEARN_PREDICTION_CONFIRMED = 0,  /**< Prediction matched observation */
    FEP_LEARN_PREDICTION_ERROR,          /**< Prediction error occurred */
    FEP_LEARN_PRECISION_UPDATE,          /**< Precision weighting adjusted */
    FEP_LEARN_BELIEF_UPDATE,             /**< Belief state updated */
    FEP_LEARN_FREE_ENERGY_REDUCED,       /**< Free energy successfully reduced */
    FEP_LEARN_FREE_ENERGY_INCREASED,     /**< Free energy increased (surprise) */
    FEP_LEARN_ACTIVE_INFERENCE_SUCCESS,  /**< Active inference achieved goal */
    FEP_LEARN_ACTIVE_INFERENCE_FAILURE,  /**< Active inference failed */
    FEP_LEARN_MODEL_COMPLEXITY_REDUCED,  /**< Model simplified */
    FEP_LEARN_VARIATIONAL_BOUND_TIGHT    /**< Variational bound tightened */
} fep_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    FEP_PLASTICITY_STATE_IDLE = 0,
    FEP_PLASTICITY_STATE_LEARNING,
    FEP_PLASTICITY_STATE_CONSOLIDATING,
    FEP_PLASTICITY_STATE_UPDATING,
    FEP_PLASTICITY_STATE_ERROR
} fep_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief FEP-Plasticity bridge configuration
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
    float target_free_energy;            /**< Target free energy level */

    /* Precision-weighted learning */
    float pred_error_learning_boost;     /**< Boost for prediction error learning */
    float precision_learning_scale;      /**< Precision scales learning rate */
    float belief_update_modulation;      /**< Belief update learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_prediction_weights;     /**< Protect prediction synapses */
    bool protect_active_inference;       /**< Protect active inference synapses */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} fep_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief FEP synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    fep_synapse_type_t type;             /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} fep_plasticity_synapse_t;

/**
 * @brief Inference learning state
 */
typedef struct {
    float prediction_accuracy;           /**< Prediction accuracy level */
    float precision_calibration;         /**< Precision calibration level */
    float belief_stability;              /**< Belief stability measure */
    float free_energy_trend;             /**< Free energy trend (decreasing=good) */
    float model_complexity;              /**< Current model complexity */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} fep_inference_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    fep_plasticity_state_t state;        /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} fep_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t prediction_confirmed_events;/**< Prediction confirmed events */
    uint64_t prediction_error_events;    /**< Prediction error events */
    uint64_t free_energy_reduced_events; /**< Free energy reduced events */
    uint64_t active_inference_events;    /**< Active inference events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} fep_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct fep_plasticity_bridge fep_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*fep_plasticity_learn_callback_t)(
    fep_plasticity_bridge_t* bridge,
    fep_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Inference state update callback */
typedef void (*fep_plasticity_inference_callback_t)(
    fep_plasticity_bridge_t* bridge,
    float old_free_energy,
    float new_free_energy,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
fep_plasticity_config_t fep_plasticity_config_default(void);

/**
 * @brief Create FEP plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
fep_plasticity_bridge_t* fep_plasticity_create(
    const fep_plasticity_config_t* config
);

/**
 * @brief Destroy FEP plasticity bridge
 * @param bridge Bridge to destroy
 */
void fep_plasticity_destroy(fep_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_reset(fep_plasticity_bridge_t* bridge);

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
int fep_plasticity_register_synapse(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    fep_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_unregister_synapse(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_get_synapse(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    fep_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_protect_synapse(
    fep_plasticity_bridge_t* bridge,
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
 * @param precision Precision weighting for learning
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_learn(
    fep_plasticity_bridge_t* bridge,
    fep_learn_event_t event,
    float magnitude,
    uint32_t synapse_id,
    float precision
);

/**
 * @brief Apply STDP to synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param pre_time Pre-synaptic spike time (ms)
 * @param post_time Post-synaptic spike time (ms)
 * @return Weight change, NAN on failure
 */
float fep_plasticity_apply_stdp(
    fep_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply prediction error modulation
 * @param bridge Bridge handle
 * @param pred_error Prediction error signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_apply_pred_error(
    fep_plasticity_bridge_t* bridge,
    float pred_error
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_update_bcm(
    fep_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_homeostatic_update(
    fep_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_update_traces(
    fep_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_consolidate(fep_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get inference state
 * @param bridge Bridge handle
 * @param state Output inference state
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_get_inference_state(
    fep_plasticity_bridge_t* bridge,
    fep_inference_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_get_state(
    fep_plasticity_bridge_t* bridge,
    fep_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_get_stats(
    fep_plasticity_bridge_t* bridge,
    fep_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_reset_stats(fep_plasticity_bridge_t* bridge);

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
int fep_plasticity_register_learn_callback(
    fep_plasticity_bridge_t* bridge,
    fep_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register inference state update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_register_inference_callback(
    fep_plasticity_bridge_t* bridge,
    fep_plasticity_inference_callback_t callback,
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
int fep_plasticity_bio_async_connect(fep_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int fep_plasticity_bio_async_disconnect(fep_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool fep_plasticity_is_bio_async_connected(fep_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_PLASTICITY_BRIDGE_H */
