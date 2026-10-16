/**
 * @file nimcp_social_plasticity_bridge.h
 * @brief Social Cognition - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between social cognition and synaptic plasticity
 * WHY:  Enable learning of social strategies from relationship experience
 * HOW:  STDP for trust-outcome associations, BCM for stabilization, reward
 *       modulation for social bonding policy learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Dunbar (1998): Social learning and relationship maintenance
 * - Tomasello (2009): Cultural learning and social cognition
 * - Frith & Frith (2012): Plasticity in social brain networks
 *
 * BIOLOGICAL BASIS:
 * - Oxytocin modulates social learning and trust plasticity
 * - Medial prefrontal cortex for social value learning
 * - Amygdala for social threat and reward learning
 * - Hippocampus for social memory consolidation
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of trust-outcome pairs
 * - BCM: Stabilize core bonding patterns
 * - Homeostatic: Maintain balanced social engagement
 * - Reward-modulated: Learn from relationship success
 *
 * @see nimcp_love_loyalty_friendship.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_SOCIAL_PLASTICITY_BRIDGE_H
#define NIMCP_SOCIAL_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum social synapses */
#define SOCIAL_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define SOCIAL_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_SOCIAL_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Social synapse types
 */
typedef enum {
    SOCIAL_SYNAPSE_TRUST = 0,            /**< Trust evaluation */
    SOCIAL_SYNAPSE_BONDING,              /**< Bonding strength (PROTECTED) */
    SOCIAL_SYNAPSE_COOPERATION,          /**< Cooperation tendency */
    SOCIAL_SYNAPSE_RECIPROCITY,          /**< Reciprocity tracking */
    SOCIAL_SYNAPSE_HIERARCHY,            /**< Hierarchy processing */
    SOCIAL_SYNAPSE_LOYALTY               /**< Loyalty commitment (PROTECTED) */
} social_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    SOCIAL_LEARN_TRUST_CONFIRMED = 0,    /**< Trust was justified */
    SOCIAL_LEARN_TRUST_VIOLATED,         /**< Trust was betrayed */
    SOCIAL_LEARN_BOND_STRENGTHENED,      /**< Bond grew stronger */
    SOCIAL_LEARN_BOND_WEAKENED,          /**< Bond grew weaker */
    SOCIAL_LEARN_COOPERATION_SUCCESS,    /**< Successful cooperation */
    SOCIAL_LEARN_COOPERATION_FAILURE,    /**< Failed cooperation */
    SOCIAL_LEARN_RECIPROCITY_MATCHED,    /**< Reciprocity balanced */
    SOCIAL_LEARN_SUPPORT_RECEIVED,       /**< Support from other */
    SOCIAL_LEARN_SUPPORT_GIVEN,          /**< Support to other */
    SOCIAL_LEARN_LOYALTY_TESTED          /**< Loyalty was tested */
} social_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SOCIAL_PLASTICITY_STATE_IDLE = 0,
    SOCIAL_PLASTICITY_STATE_LEARNING,
    SOCIAL_PLASTICITY_STATE_CONSOLIDATING,
    SOCIAL_PLASTICITY_STATE_UPDATING,
    SOCIAL_PLASTICITY_STATE_ERROR
} social_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Social-Plasticity bridge configuration
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
    float target_bonding;                /**< Target bonding level */

    /* Reward modulation */
    float trust_learning_boost;          /**< Boost for trust confirmation */
    float cooperation_learning_boost;    /**< Boost for cooperation success */
    float bonding_modulation;            /**< Bonding learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_bonding_strength;       /**< Protect bonding weights */
    bool protect_loyalty_commitment;     /**< Protect loyalty weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} social_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Social synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    social_synapse_type_t type;          /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} social_plasticity_synapse_t;

/**
 * @brief Social bonding learning state
 */
typedef struct {
    float trust_sensitivity;             /**< Sensitivity to trust signals */
    float bonding_calibration;           /**< Bonding calibration level */
    float cooperation_sensitivity;       /**< Sensitivity to cooperation */
    float reciprocity_strength;          /**< Reciprocity tracking strength */
    float hierarchy_awareness;           /**< Hierarchy awareness strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} social_bonding_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    social_plasticity_state_t state;     /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} social_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t trust_confirmed_events;     /**< Trust confirmed events */
    uint64_t trust_violated_events;      /**< Trust violation corrections */
    uint64_t bond_strengthened_events;   /**< Bond strengthened events */
    uint64_t cooperation_success_events; /**< Cooperation success learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} social_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct social_plasticity_bridge social_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*social_plasticity_learn_callback_t)(
    social_plasticity_bridge_t* bridge,
    social_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Bonding update callback */
typedef void (*social_plasticity_bonding_callback_t)(
    social_plasticity_bridge_t* bridge,
    float old_bonding,
    float new_bonding,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
social_plasticity_config_t social_plasticity_config_default(void);

/**
 * @brief Create social plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
social_plasticity_bridge_t* social_plasticity_create(
    const social_plasticity_config_t* config
);

/**
 * @brief Destroy social plasticity bridge
 * @param bridge Bridge to destroy
 */
void social_plasticity_destroy(social_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_plasticity_reset(social_plasticity_bridge_t* bridge);

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
int social_plasticity_register_synapse(
    social_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    social_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int social_plasticity_unregister_synapse(
    social_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int social_plasticity_get_synapse(
    social_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    social_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int social_plasticity_protect_synapse(
    social_plasticity_bridge_t* bridge,
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
int social_plasticity_learn(
    social_plasticity_bridge_t* bridge,
    social_learn_event_t event,
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
float social_plasticity_apply_stdp(
    social_plasticity_bridge_t* bridge,
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
int social_plasticity_apply_reward(
    social_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int social_plasticity_update_bcm(
    social_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int social_plasticity_homeostatic_update(
    social_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int social_plasticity_update_traces(
    social_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_plasticity_consolidate(social_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get bonding state
 * @param bridge Bridge handle
 * @param state Output bonding state
 * @return 0 on success, -1 on failure
 */
int social_plasticity_get_bonding_state(
    social_plasticity_bridge_t* bridge,
    social_bonding_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int social_plasticity_get_state(
    social_plasticity_bridge_t* bridge,
    social_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int social_plasticity_get_stats(
    social_plasticity_bridge_t* bridge,
    social_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_plasticity_reset_stats(social_plasticity_bridge_t* bridge);

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
int social_plasticity_register_learn_callback(
    social_plasticity_bridge_t* bridge,
    social_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register bonding update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int social_plasticity_register_bonding_callback(
    social_plasticity_bridge_t* bridge,
    social_plasticity_bonding_callback_t callback,
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
int social_plasticity_bio_async_connect(social_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int social_plasticity_bio_async_disconnect(social_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool social_plasticity_is_bio_async_connected(social_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SOCIAL_PLASTICITY_BRIDGE_H */
