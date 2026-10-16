/**
 * @file nimcp_jepa_plasticity_bridge.h
 * @brief JEPA - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between JEPA predictor and synaptic plasticity
 * WHY:  Enable learning of prediction strategies from self-supervised experience
 * HOW:  STDP for prediction-target associations, BCM for stabilization, reward
 *       modulation for latent representation learning
 *
 * THEORETICAL FOUNDATIONS:
 * - LeCun (2022): JEPA - Self-supervised predictive learning
 * - Assran et al. (2023): I-JEPA - Image-based JEPA
 * - Rao & Ballard (1999): Predictive coding in visual cortex
 *
 * BIOLOGICAL BASIS:
 * - Predictive coding enables top-down learning signals
 * - Cortical hierarchies learn through prediction errors
 * - Self-supervised learning mirrors unsupervised cortical plasticity
 * - Multi-modal integration through cross-modal associations
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of context-target pairs
 * - BCM: Stabilize learned latent representations
 * - Homeostatic: Maintain balanced prediction activity
 * - Precision-modulated: Learn from prediction confidence
 *
 * @see nimcp_jepa_predictor.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_JEPA_PLASTICITY_BRIDGE_H
#define NIMCP_JEPA_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum JEPA synapses */
#define JEPA_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define JEPA_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_JEPA_PLASTICITY     0x0E51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief JEPA synapse types
 */
typedef enum {
    JEPA_SYNAPSE_CONTEXT = 0,            /**< Context encoding */
    JEPA_SYNAPSE_PREDICTION,              /**< Prediction pathway (PROTECTED) */
    JEPA_SYNAPSE_ERROR,                   /**< Error signal pathway */
    JEPA_SYNAPSE_LATENT,                  /**< Latent representation */
    JEPA_SYNAPSE_MULTIMODAL,              /**< Multi-modal integration */
    JEPA_SYNAPSE_SELF_SUPERVISED          /**< Self-supervised learning (PROTECTED) */
} jepa_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    JEPA_LEARN_PREDICTION_ACCURATE = 0,  /**< Accurate prediction made */
    JEPA_LEARN_PREDICTION_ERROR,          /**< Prediction error detected */
    JEPA_LEARN_CONTEXT_MATCHED,           /**< Context successfully matched */
    JEPA_LEARN_CONTEXT_MISMATCH,          /**< Context mismatch detected */
    JEPA_LEARN_LATENT_CONVERGED,          /**< Latent representation converged */
    JEPA_LEARN_LATENT_DIVERGED,           /**< Latent representation diverged */
    JEPA_LEARN_MULTIMODAL_ALIGNED,        /**< Multi-modal alignment achieved */
    JEPA_LEARN_MASKING_SUCCESS,           /**< Masking prediction success */
    JEPA_LEARN_MASKING_FAILURE,           /**< Masking prediction failure */
    JEPA_LEARN_EMBEDDING_IMPROVED         /**< Embedding quality improved */
} jepa_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    JEPA_PLASTICITY_STATE_IDLE = 0,
    JEPA_PLASTICITY_STATE_LEARNING,
    JEPA_PLASTICITY_STATE_CONSOLIDATING,
    JEPA_PLASTICITY_STATE_UPDATING,
    JEPA_PLASTICITY_STATE_ERROR
} jepa_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief JEPA-Plasticity bridge configuration
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
    float target_prediction_accuracy;    /**< Target prediction accuracy */

    /* Prediction learning modulation */
    float prediction_accuracy_boost;     /**< Boost for accurate predictions */
    float error_learning_boost;          /**< Boost for error correction */
    float context_modulation;            /**< Context learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_prediction_pathway;     /**< Protect prediction pathway weights */
    bool protect_self_supervised;        /**< Protect self-supervised weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} jepa_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief JEPA synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    jepa_synapse_type_t type;            /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} jepa_plasticity_synapse_t;

/**
 * @brief Prediction learning state
 */
typedef struct {
    float prediction_accuracy;           /**< Current prediction accuracy */
    float context_calibration;           /**< Context calibration level */
    float latent_quality;                /**< Latent representation quality */
    float multimodal_alignment;          /**< Multi-modal alignment strength */
    float self_supervised_strength;      /**< Self-supervised signal strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} jepa_prediction_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    jepa_plasticity_state_t state;       /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} jepa_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t prediction_accurate_events; /**< Accurate prediction events */
    uint64_t prediction_error_events;    /**< Prediction error events */
    uint64_t context_matched_events;     /**< Context matched events */
    uint64_t latent_converged_events;    /**< Latent convergence learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} jepa_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct jepa_plasticity_bridge jepa_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*jepa_plasticity_learn_callback_t)(
    jepa_plasticity_bridge_t* bridge,
    jepa_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Prediction update callback */
typedef void (*jepa_plasticity_prediction_callback_t)(
    jepa_plasticity_bridge_t* bridge,
    float old_accuracy,
    float new_accuracy,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
jepa_plasticity_config_t jepa_plasticity_config_default(void);

/**
 * @brief Create JEPA plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
jepa_plasticity_bridge_t* jepa_plasticity_create(
    const jepa_plasticity_config_t* config
);

/**
 * @brief Destroy JEPA plasticity bridge
 * @param bridge Bridge to destroy
 */
void jepa_plasticity_destroy(jepa_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_reset(jepa_plasticity_bridge_t* bridge);

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
int jepa_plasticity_register_synapse(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    jepa_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_unregister_synapse(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_get_synapse(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    jepa_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_protect_synapse(
    jepa_plasticity_bridge_t* bridge,
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
int jepa_plasticity_learn(
    jepa_plasticity_bridge_t* bridge,
    jepa_learn_event_t event,
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
float jepa_plasticity_apply_stdp(
    jepa_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply precision modulation
 * @param bridge Bridge handle
 * @param precision Precision signal [0, 1]
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_apply_precision(
    jepa_plasticity_bridge_t* bridge,
    float precision
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_update_bcm(
    jepa_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_homeostatic_update(
    jepa_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_update_traces(
    jepa_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_consolidate(jepa_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get prediction state
 * @param bridge Bridge handle
 * @param state Output prediction state
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_get_prediction_state(
    jepa_plasticity_bridge_t* bridge,
    jepa_prediction_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_get_state(
    jepa_plasticity_bridge_t* bridge,
    jepa_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_get_stats(
    jepa_plasticity_bridge_t* bridge,
    jepa_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_reset_stats(jepa_plasticity_bridge_t* bridge);

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
int jepa_plasticity_register_learn_callback(
    jepa_plasticity_bridge_t* bridge,
    jepa_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register prediction update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_register_prediction_callback(
    jepa_plasticity_bridge_t* bridge,
    jepa_plasticity_prediction_callback_t callback,
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
int jepa_plasticity_bio_async_connect(jepa_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int jepa_plasticity_bio_async_disconnect(jepa_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool jepa_plasticity_is_bio_async_connected(jepa_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_PLASTICITY_BRIDGE_H */
