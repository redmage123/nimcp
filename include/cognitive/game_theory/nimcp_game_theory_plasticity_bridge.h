/**
 * @file nimcp_game_theory_plasticity_bridge.h
 * @brief Game Theory - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between game theory engine and synaptic plasticity
 * WHY:  Enable learning of strategic behaviors from experience and feedback
 * HOW:  STDP for strategy-outcome associations, BCM for stabilization, reward
 *       modulation for strategy learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Fudenberg & Levine (1998): Theory of learning in games
 * - Erev & Roth (1998): Predicting how people play games
 * - Camerer (2003): Behavioral game theory
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic system modulates reward prediction errors
 * - Prefrontal cortex for strategy maintenance
 * - Striatum for action value learning
 * - Orbitofrontal cortex for outcome evaluation
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of strategy-payoff pairs
 * - BCM: Stabilize successful strategy patterns
 * - Homeostatic: Maintain balanced cooperation/defection
 * - Reward-modulated: Learn from game outcomes
 *
 * @see nimcp_game_theory.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_GAME_THEORY_PLASTICITY_BRIDGE_H
#define NIMCP_GAME_THEORY_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum game theory synapses */
#define GAME_THEORY_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define GAME_THEORY_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_GAME_THEORY_PLASTICITY     0x1511

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Game theory synapse types
 */
typedef enum {
    GT_SYNAPSE_COOPERATION = 0,          /**< Cooperation tendency */
    GT_SYNAPSE_DEFECTION,                 /**< Defection tendency */
    GT_SYNAPSE_STRATEGY,                  /**< Strategy encoding (PROTECTED) */
    GT_SYNAPSE_PAYOFF,                    /**< Payoff expectation */
    GT_SYNAPSE_OPPONENT,                  /**< Opponent modeling */
    GT_SYNAPSE_EQUILIBRIUM                /**< Equilibrium tracking (PROTECTED) */
} game_theory_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    GT_LEARN_COOPERATION_REWARDED = 0,   /**< Cooperation led to good outcome */
    GT_LEARN_COOPERATION_PUNISHED,        /**< Cooperation led to bad outcome */
    GT_LEARN_DEFECTION_REWARDED,          /**< Defection led to good outcome */
    GT_LEARN_DEFECTION_PUNISHED,          /**< Defection led to bad outcome */
    GT_LEARN_EQUILIBRIUM_REACHED,         /**< Nash equilibrium reached */
    GT_LEARN_EQUILIBRIUM_DEVIATED,        /**< Deviated from equilibrium */
    GT_LEARN_OPPONENT_PREDICTED,          /**< Opponent correctly predicted */
    GT_LEARN_OPPONENT_SURPRISED,          /**< Opponent behavior surprising */
    GT_LEARN_PAYOFF_EXCEEDED,             /**< Payoff exceeded expectation */
    GT_LEARN_PAYOFF_DISAPPOINTED          /**< Payoff below expectation */
} game_theory_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    GT_PLASTICITY_STATE_IDLE = 0,
    GT_PLASTICITY_STATE_LEARNING,
    GT_PLASTICITY_STATE_CONSOLIDATING,
    GT_PLASTICITY_STATE_UPDATING,
    GT_PLASTICITY_STATE_ERROR
} game_theory_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Game Theory-Plasticity bridge configuration
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
    float target_cooperation;            /**< Target cooperation level */

    /* Reward modulation */
    float payoff_learning_boost;         /**< Boost for high payoff */
    float equilibrium_learning_boost;    /**< Boost for equilibrium */
    float opponent_prediction_modulation; /**< Opponent prediction strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_strategy_weights;       /**< Protect strategy encoding weights */
    bool protect_equilibrium_weights;    /**< Protect equilibrium tracking weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} game_theory_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Game theory synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    game_theory_synapse_type_t type;     /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} game_theory_plasticity_synapse_t;

/**
 * @brief Strategy learning state
 */
typedef struct {
    float cooperation_tendency;          /**< Learned cooperation tendency */
    float defection_tendency;            /**< Learned defection tendency */
    float payoff_sensitivity;            /**< Sensitivity to payoff differences */
    float opponent_model_accuracy;       /**< Opponent prediction accuracy */
    float equilibrium_tracking;          /**< Equilibrium tracking strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} game_theory_strategy_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    game_theory_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} game_theory_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t cooperation_rewarded_events; /**< Cooperation rewarded events */
    uint64_t defection_rewarded_events;  /**< Defection rewarded events */
    uint64_t equilibrium_events;         /**< Equilibrium learning events */
    uint64_t opponent_prediction_events; /**< Opponent prediction events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} game_theory_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct game_theory_plasticity_bridge game_theory_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*game_theory_plasticity_learn_callback_t)(
    game_theory_plasticity_bridge_t* bridge,
    game_theory_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Strategy update callback */
typedef void (*game_theory_plasticity_strategy_callback_t)(
    game_theory_plasticity_bridge_t* bridge,
    float old_cooperation,
    float new_cooperation,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
game_theory_plasticity_config_t game_theory_plasticity_config_default(void);

/**
 * @brief Create game theory plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
game_theory_plasticity_bridge_t* game_theory_plasticity_create(
    const game_theory_plasticity_config_t* config
);

/**
 * @brief Destroy game theory plasticity bridge
 * @param bridge Bridge to destroy
 */
void game_theory_plasticity_destroy(game_theory_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_reset(game_theory_plasticity_bridge_t* bridge);

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
int game_theory_plasticity_register_synapse(
    game_theory_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    game_theory_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_unregister_synapse(
    game_theory_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_get_synapse(
    game_theory_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    game_theory_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_protect_synapse(
    game_theory_plasticity_bridge_t* bridge,
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
int game_theory_plasticity_learn(
    game_theory_plasticity_bridge_t* bridge,
    game_theory_learn_event_t event,
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
float game_theory_plasticity_apply_stdp(
    game_theory_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply reward modulation (payoff signal)
 * @param bridge Bridge handle
 * @param reward Reward signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_apply_reward(
    game_theory_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_update_bcm(
    game_theory_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_homeostatic_update(
    game_theory_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_update_traces(
    game_theory_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_consolidate(game_theory_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get strategy state
 * @param bridge Bridge handle
 * @param state Output strategy state
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_get_strategy_state(
    game_theory_plasticity_bridge_t* bridge,
    game_theory_strategy_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_get_state(
    game_theory_plasticity_bridge_t* bridge,
    game_theory_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_get_stats(
    game_theory_plasticity_bridge_t* bridge,
    game_theory_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_reset_stats(game_theory_plasticity_bridge_t* bridge);

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
int game_theory_plasticity_register_learn_callback(
    game_theory_plasticity_bridge_t* bridge,
    game_theory_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register strategy update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_register_strategy_callback(
    game_theory_plasticity_bridge_t* bridge,
    game_theory_plasticity_strategy_callback_t callback,
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
int game_theory_plasticity_bio_async_connect(game_theory_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int game_theory_plasticity_bio_async_disconnect(game_theory_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool game_theory_plasticity_is_bio_async_connected(game_theory_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GAME_THEORY_PLASTICITY_BRIDGE_H */
