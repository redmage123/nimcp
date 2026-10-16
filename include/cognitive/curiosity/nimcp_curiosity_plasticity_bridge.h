/**
 * @file nimcp_curiosity_plasticity_bridge.h
 * @brief Curiosity - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between curiosity engine and synaptic plasticity
 * WHY:  Enable learning of exploration strategies from experience and feedback
 * HOW:  STDP for novelty-reward associations, BCM for stabilization, reward
 *       modulation for exploration policy learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Gruber et al. (2014): Curiosity enhances hippocampal memory
 * - Kang et al. (2009): Curiosity-driven exploration and learning
 * - Oudeyer & Kaplan (2007): Intrinsic motivation and curiosity
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic system modulates curiosity-driven plasticity
 * - Hippocampal novelty signals enhance LTP
 * - VTA activation during exploration strengthens seeking circuits
 * - Repeated exploration strengthens information-seeking pathways
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of curiosity-outcome pairs
 * - BCM: Stabilize core exploration patterns
 * - Homeostatic: Maintain balanced curiosity levels
 * - Reward-modulated: Learn from information gain success
 *
 * @see nimcp_curiosity.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_CURIOSITY_PLASTICITY_BRIDGE_H
#define NIMCP_CURIOSITY_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum curiosity synapses */
#define CURIOSITY_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define CURIOSITY_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_CURIOSITY_PLASTICITY     0x0D51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Curiosity synapse types
 */
typedef enum {
    CURIOSITY_SYNAPSE_NOVELTY = 0,       /**< Novelty detection */
    CURIOSITY_SYNAPSE_EXPLORATION,        /**< Exploration drive (PROTECTED) */
    CURIOSITY_SYNAPSE_INFORMATION,        /**< Information gain */
    CURIOSITY_SYNAPSE_INTEREST,           /**< Interest modulation */
    CURIOSITY_SYNAPSE_SEEKING,            /**< Active seeking */
    CURIOSITY_SYNAPSE_LEARNING            /**< Learning progress (PROTECTED) */
} curiosity_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    CURIOSITY_LEARN_NOVELTY_CONFIRMED = 0, /**< Novelty was genuinely new */
    CURIOSITY_LEARN_FALSE_NOVELTY,         /**< False novelty detection */
    CURIOSITY_LEARN_INFO_GAIN_HIGH,        /**< High information gain achieved */
    CURIOSITY_LEARN_INFO_GAIN_LOW,         /**< Low information gain */
    CURIOSITY_LEARN_EXPLORATION_SUCCESS,   /**< Successful exploration */
    CURIOSITY_LEARN_EXPLORATION_FAILURE,   /**< Failed exploration */
    CURIOSITY_LEARN_INTEREST_MATCHED,      /**< Interest prediction matched */
    CURIOSITY_LEARN_SURPRISE_POSITIVE,     /**< Positive surprise */
    CURIOSITY_LEARN_SURPRISE_NEGATIVE,     /**< Negative surprise */
    CURIOSITY_LEARN_PROGRESS_MADE          /**< Learning progress detected */
} curiosity_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    CURIOSITY_PLASTICITY_STATE_IDLE = 0,
    CURIOSITY_PLASTICITY_STATE_LEARNING,
    CURIOSITY_PLASTICITY_STATE_CONSOLIDATING,
    CURIOSITY_PLASTICITY_STATE_UPDATING,
    CURIOSITY_PLASTICITY_STATE_ERROR
} curiosity_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Curiosity-Plasticity bridge configuration
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
    float target_exploration;            /**< Target exploration level */

    /* Reward modulation */
    float info_gain_learning_boost;      /**< Boost for high info gain */
    float exploration_learning_boost;    /**< Boost for exploration success */
    float novelty_modulation;            /**< Novelty learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_exploration_drive;      /**< Protect exploration drive weights */
    bool protect_learning_progress;      /**< Protect learning progress weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} curiosity_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Curiosity synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    curiosity_synapse_type_t type;       /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} curiosity_plasticity_synapse_t;

/**
 * @brief Exploration learning state
 */
typedef struct {
    float novelty_sensitivity;           /**< Sensitivity to novelty */
    float exploration_calibration;       /**< Exploration calibration level */
    float info_gain_sensitivity;         /**< Sensitivity to information gain */
    float interest_strength;             /**< Interest detection strength */
    float seeking_strength;              /**< Seeking behavior strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} curiosity_exploration_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    curiosity_plasticity_state_t state;  /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} curiosity_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t novelty_confirmed_events;   /**< Novelty confirmed events */
    uint64_t false_novelty_events;       /**< False novelty corrections */
    uint64_t high_info_gain_events;      /**< High info gain events */
    uint64_t exploration_success_events; /**< Exploration success learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} curiosity_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct curiosity_plasticity_bridge curiosity_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*curiosity_plasticity_learn_callback_t)(
    curiosity_plasticity_bridge_t* bridge,
    curiosity_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Exploration update callback */
typedef void (*curiosity_plasticity_exploration_callback_t)(
    curiosity_plasticity_bridge_t* bridge,
    float old_exploration,
    float new_exploration,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
curiosity_plasticity_config_t curiosity_plasticity_config_default(void);

/**
 * @brief Create curiosity plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
curiosity_plasticity_bridge_t* curiosity_plasticity_create(
    const curiosity_plasticity_config_t* config
);

/**
 * @brief Destroy curiosity plasticity bridge
 * @param bridge Bridge to destroy
 */
void curiosity_plasticity_destroy(curiosity_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_reset(curiosity_plasticity_bridge_t* bridge);

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
int curiosity_plasticity_register_synapse(
    curiosity_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    curiosity_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_unregister_synapse(
    curiosity_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_get_synapse(
    curiosity_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    curiosity_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_protect_synapse(
    curiosity_plasticity_bridge_t* bridge,
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
int curiosity_plasticity_learn(
    curiosity_plasticity_bridge_t* bridge,
    curiosity_learn_event_t event,
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
float curiosity_plasticity_apply_stdp(
    curiosity_plasticity_bridge_t* bridge,
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
int curiosity_plasticity_apply_reward(
    curiosity_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_update_bcm(
    curiosity_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_homeostatic_update(
    curiosity_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_update_traces(
    curiosity_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_consolidate(curiosity_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get exploration state
 * @param bridge Bridge handle
 * @param state Output exploration state
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_get_exploration_state(
    curiosity_plasticity_bridge_t* bridge,
    curiosity_exploration_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_get_state(
    curiosity_plasticity_bridge_t* bridge,
    curiosity_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_get_stats(
    curiosity_plasticity_bridge_t* bridge,
    curiosity_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_reset_stats(curiosity_plasticity_bridge_t* bridge);

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
int curiosity_plasticity_register_learn_callback(
    curiosity_plasticity_bridge_t* bridge,
    curiosity_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register exploration update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_register_exploration_callback(
    curiosity_plasticity_bridge_t* bridge,
    curiosity_plasticity_exploration_callback_t callback,
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
int curiosity_plasticity_bio_async_connect(curiosity_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int curiosity_plasticity_bio_async_disconnect(curiosity_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool curiosity_plasticity_is_bio_async_connected(curiosity_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CURIOSITY_PLASTICITY_BRIDGE_H */
