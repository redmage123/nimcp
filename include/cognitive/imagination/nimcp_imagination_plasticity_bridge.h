/**
 * @file nimcp_imagination_plasticity_bridge.h
 * @brief Imagination - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between imagination engine and synaptic plasticity
 * WHY:  Enable learning of generative mental simulation strategies from experience
 * HOW:  STDP for imagery-outcome associations, BCM for stabilization, reward
 *       modulation for creative imagination learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Pearson et al. (2015): Sensory imagination and mental imagery neural bases
 * - Moulton & Kosslyn (2009): Imagining predictions and memory consolidation
 * - Schacter et al. (2007): Remembering the past to imagine the future
 *
 * BIOLOGICAL BASIS:
 * - Visual cortex plasticity during mental imagery training
 * - Hippocampal consolidation of imagined scenarios
 * - Prefrontal control learning for directed imagination
 * - Creative combination strengthening through reward signals
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of imagery-outcome pairs
 * - BCM: Stabilize core mental imagery patterns
 * - Homeostatic: Maintain balanced vividness levels
 * - Reward-modulated: Learn from successful imaginations
 *
 * @see nimcp_imagination_engine.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_IMAGINATION_PLASTICITY_BRIDGE_H
#define NIMCP_IMAGINATION_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum imagination synapses */
#define IMAGINATION_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define IMAGINATION_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_IMAGINATION_PLASTICITY     0x1A51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Imagination synapse types
 */
typedef enum {
    IMAGINATION_SYNAPSE_VIVIDNESS = 0,    /**< Vividness generation */
    IMAGINATION_SYNAPSE_COHERENCE,         /**< Scene coherence (PROTECTED) */
    IMAGINATION_SYNAPSE_CREATIVITY,        /**< Creative combination */
    IMAGINATION_SYNAPSE_CONTROLLABILITY,   /**< Imagery control */
    IMAGINATION_SYNAPSE_COUNTERFACTUAL,    /**< Counterfactual simulation */
    IMAGINATION_SYNAPSE_PROSPECTIVE        /**< Future simulation (PROTECTED) */
} imagination_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    IMAGINATION_LEARN_IMAGERY_VIVID = 0,   /**< Vivid imagery achieved */
    IMAGINATION_LEARN_IMAGERY_FAINT,       /**< Faint/unclear imagery */
    IMAGINATION_LEARN_COHERENCE_HIGH,      /**< High scene coherence achieved */
    IMAGINATION_LEARN_COHERENCE_LOW,       /**< Low scene coherence */
    IMAGINATION_LEARN_CREATIVE_SUCCESS,    /**< Successful creative combination */
    IMAGINATION_LEARN_CREATIVE_FAILURE,    /**< Failed creative combination */
    IMAGINATION_LEARN_CONTROL_ACCURATE,    /**< Accurate imagery control */
    IMAGINATION_LEARN_COUNTERFACTUAL_VALID,/**< Valid counterfactual generated */
    IMAGINATION_LEARN_COUNTERFACTUAL_INVALID, /**< Invalid counterfactual */
    IMAGINATION_LEARN_PROSPECTIVE_ACCURATE /**< Accurate future prediction */
} imagination_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    IMAGINATION_PLASTICITY_STATE_IDLE = 0,
    IMAGINATION_PLASTICITY_STATE_LEARNING,
    IMAGINATION_PLASTICITY_STATE_CONSOLIDATING,
    IMAGINATION_PLASTICITY_STATE_UPDATING,
    IMAGINATION_PLASTICITY_STATE_ERROR
} imagination_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Imagination-Plasticity bridge configuration
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
    float target_vividness;              /**< Target vividness level */

    /* Reward modulation */
    float vividness_learning_boost;      /**< Boost for vivid imagery */
    float creativity_learning_boost;     /**< Boost for creative success */
    float coherence_modulation;          /**< Coherence learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_coherence;              /**< Protect coherence weights */
    bool protect_prospective;            /**< Protect prospective weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} imagination_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Imagination synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    imagination_synapse_type_t type;     /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} imagination_plasticity_synapse_t;

/**
 * @brief Imagery learning state
 */
typedef struct {
    float vividness_sensitivity;         /**< Sensitivity to vividness */
    float coherence_calibration;         /**< Coherence calibration level */
    float creativity_sensitivity;        /**< Sensitivity to creativity success */
    float controllability_strength;      /**< Control ability strength */
    float counterfactual_strength;       /**< Counterfactual generation strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} imagination_imagery_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    imagination_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} imagination_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t vivid_imagery_events;       /**< Vivid imagery learning events */
    uint64_t faint_imagery_events;       /**< Faint imagery corrections */
    uint64_t high_coherence_events;      /**< High coherence events */
    uint64_t creative_success_events;    /**< Creative success learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} imagination_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct imagination_plasticity_bridge imagination_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*imagination_plasticity_learn_callback_t)(
    imagination_plasticity_bridge_t* bridge,
    imagination_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Imagery update callback */
typedef void (*imagination_plasticity_imagery_callback_t)(
    imagination_plasticity_bridge_t* bridge,
    float old_vividness,
    float new_vividness,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
imagination_plasticity_config_t imagination_plasticity_config_default(void);

/**
 * @brief Create imagination plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
imagination_plasticity_bridge_t* imagination_plasticity_create(
    const imagination_plasticity_config_t* config
);

/**
 * @brief Destroy imagination plasticity bridge
 * @param bridge Bridge to destroy
 */
void imagination_plasticity_destroy(imagination_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_reset(imagination_plasticity_bridge_t* bridge);

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
int imagination_plasticity_register_synapse(
    imagination_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    imagination_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_unregister_synapse(
    imagination_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_get_synapse(
    imagination_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    imagination_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_protect_synapse(
    imagination_plasticity_bridge_t* bridge,
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
int imagination_plasticity_learn(
    imagination_plasticity_bridge_t* bridge,
    imagination_learn_event_t event,
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
float imagination_plasticity_apply_stdp(
    imagination_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply reward modulation
 * @param bridge Bridge handle
 * @param reward Reward signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_apply_reward(
    imagination_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_update_bcm(
    imagination_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_homeostatic_update(
    imagination_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_update_traces(
    imagination_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_consolidate(imagination_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get imagery state
 * @param bridge Bridge handle
 * @param state Output imagery state
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_get_imagery_state(
    imagination_plasticity_bridge_t* bridge,
    imagination_imagery_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_get_state(
    imagination_plasticity_bridge_t* bridge,
    imagination_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_get_stats(
    imagination_plasticity_bridge_t* bridge,
    imagination_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_reset_stats(imagination_plasticity_bridge_t* bridge);

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
int imagination_plasticity_register_learn_callback(
    imagination_plasticity_bridge_t* bridge,
    imagination_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register imagery update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_register_imagery_callback(
    imagination_plasticity_bridge_t* bridge,
    imagination_plasticity_imagery_callback_t callback,
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
int imagination_plasticity_bio_async_connect(imagination_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int imagination_plasticity_bio_async_disconnect(imagination_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool imagination_plasticity_is_bio_async_connected(imagination_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_IMAGINATION_PLASTICITY_BRIDGE_H */
