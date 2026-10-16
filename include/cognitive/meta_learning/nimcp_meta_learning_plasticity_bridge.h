/**
 * @file nimcp_meta_learning_plasticity_bridge.h
 * @brief Meta Learning - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between meta learning engine and synaptic plasticity
 * WHY:  Enable learning of meta-learning patterns from experience and feedback
 * HOW:  STDP for learning associations, BCM for stabilization, reward
 *       modulation for adaptation learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Thrun & Pratt (1998): Learning to learn
 * - Harlow (1949): Learning sets - biological meta-learning
 * - Wang (2018): Prefrontal cortex as a meta-reinforcement learning system
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex plasticity shapes meta-learning abilities
 * - Dopaminergic signals modulate learning rate adaptation
 * - Hippocampal plasticity enables rapid task encoding
 * - Repeated learning strengthens meta-cognitive circuits
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of task-outcome pairs
 * - BCM: Stabilize core meta-learning patterns
 * - Homeostatic: Maintain balanced learning rates
 * - Reward-modulated: Learn from task performance
 *
 * @see nimcp_meta_learning.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_META_LEARNING_PLASTICITY_BRIDGE_H
#define NIMCP_META_LEARNING_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum meta learning synapses */
#define META_LEARNING_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define META_LEARNING_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_META_LEARNING_PLASTICITY     0x0D51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Meta learning synapse types
 */
typedef enum {
    META_SYNAPSE_LEARNING_RATE = 0, /**< Learning rate adaptation */
    META_SYNAPSE_STRATEGY,           /**< Strategy selection */
    META_SYNAPSE_TRANSFER,           /**< Transfer learning */
    META_SYNAPSE_GENERALIZATION,     /**< Generalization ability */
    META_SYNAPSE_ADAPTATION,         /**< Adaptation speed */
    META_SYNAPSE_CONSOLIDATION       /**< Knowledge consolidation */
} meta_learning_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    META_LEARN_RATE_CORRECT = 0,        /**< Learning rate was appropriate */
    META_LEARN_RATE_TOO_HIGH,            /**< Learning rate was too high */
    META_LEARN_RATE_TOO_LOW,             /**< Learning rate was too low */
    META_LEARN_TRANSFER_SUCCESS,         /**< Successful transfer */
    META_LEARN_TRANSFER_FAILURE,         /**< Failed transfer attempt */
    META_LEARN_STRATEGY_EFFECTIVE,       /**< Strategy was effective */
    META_LEARN_STRATEGY_INEFFECTIVE,     /**< Strategy was ineffective */
    META_LEARN_GENERALIZATION_SUCCESS    /**< Successful generalization */
} meta_learning_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    META_LEARNING_PLASTICITY_STATE_IDLE = 0,
    META_LEARNING_PLASTICITY_STATE_LEARNING,
    META_LEARNING_PLASTICITY_STATE_CONSOLIDATING,
    META_LEARNING_PLASTICITY_STATE_UPDATING,
    META_LEARNING_PLASTICITY_STATE_ERROR
} meta_learning_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Meta Learning-Plasticity bridge configuration
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
    float target_learning_rate;          /**< Target learning rate */

    /* Reward modulation */
    float transfer_learning_boost;       /**< Boost for successful transfer */
    float adaptation_learning_boost;     /**< Boost for adaptation learning */
    float consolidation_modulation;      /**< Consolidation learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_core_patterns;          /**< Protect core meta-learning patterns */
    bool protect_consolidation;          /**< Protect consolidation weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} meta_learning_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Meta learning synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    meta_learning_synapse_type_t type;   /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} meta_learning_plasticity_synapse_t;

/**
 * @brief Meta-learning adaptation state
 */
typedef struct {
    float learning_rate_sensitivity;     /**< Sensitivity to learning rate changes */
    float transfer_calibration;          /**< Transfer learning calibration */
    float adaptation_sensitivity;        /**< Sensitivity to adaptation needs */
    float strategy_strength;             /**< Strategy selection strength */
    float consolidation_strength;        /**< Consolidation strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} meta_learning_adaptation_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    meta_learning_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} meta_learning_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t correct_rate_events;        /**< Correct rate events */
    uint64_t rate_too_high_events;       /**< Rate too high corrections */
    uint64_t rate_too_low_events;        /**< Rate too low corrections */
    uint64_t transfer_success_events;    /**< Transfer success learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} meta_learning_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct meta_learning_plasticity_bridge meta_learning_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*meta_learning_plasticity_learn_callback_t)(
    meta_learning_plasticity_bridge_t* bridge,
    meta_learning_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Adaptation update callback */
typedef void (*meta_learning_plasticity_adaptation_callback_t)(
    meta_learning_plasticity_bridge_t* bridge,
    float old_adaptation,
    float new_adaptation,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
meta_learning_plasticity_config_t meta_learning_plasticity_config_default(void);

/**
 * @brief Create meta learning plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
meta_learning_plasticity_bridge_t* meta_learning_plasticity_create(
    const meta_learning_plasticity_config_t* config
);

/**
 * @brief Destroy meta learning plasticity bridge
 * @param bridge Bridge to destroy
 */
void meta_learning_plasticity_destroy(meta_learning_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_reset(meta_learning_plasticity_bridge_t* bridge);

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
int meta_learning_plasticity_register_synapse(
    meta_learning_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    meta_learning_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_unregister_synapse(
    meta_learning_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_get_synapse(
    meta_learning_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    meta_learning_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_protect_synapse(
    meta_learning_plasticity_bridge_t* bridge,
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
int meta_learning_plasticity_learn(
    meta_learning_plasticity_bridge_t* bridge,
    meta_learning_learn_event_t event,
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
float meta_learning_plasticity_apply_stdp(
    meta_learning_plasticity_bridge_t* bridge,
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
int meta_learning_plasticity_apply_reward(
    meta_learning_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_update_bcm(
    meta_learning_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_homeostatic_update(
    meta_learning_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_update_traces(
    meta_learning_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_consolidate(meta_learning_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get adaptation state
 * @param bridge Bridge handle
 * @param state Output adaptation state
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_get_adaptation_state(
    meta_learning_plasticity_bridge_t* bridge,
    meta_learning_adaptation_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_get_state(
    meta_learning_plasticity_bridge_t* bridge,
    meta_learning_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_get_stats(
    meta_learning_plasticity_bridge_t* bridge,
    meta_learning_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_reset_stats(meta_learning_plasticity_bridge_t* bridge);

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
int meta_learning_plasticity_register_learn_callback(
    meta_learning_plasticity_bridge_t* bridge,
    meta_learning_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register adaptation update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_register_adaptation_callback(
    meta_learning_plasticity_bridge_t* bridge,
    meta_learning_plasticity_adaptation_callback_t callback,
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
int meta_learning_plasticity_bio_async_connect(meta_learning_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int meta_learning_plasticity_bio_async_disconnect(meta_learning_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool meta_learning_plasticity_is_bio_async_connected(meta_learning_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_META_LEARNING_PLASTICITY_BRIDGE_H */
