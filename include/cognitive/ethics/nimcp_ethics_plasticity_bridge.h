/**
 * @file nimcp_ethics_plasticity_bridge.h
 * @brief Ethics Engine - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between ethics engine and synaptic plasticity
 * WHY:  Enable learning of ethical patterns from experience and feedback
 * HOW:  STDP for ethical associations, BCM for stabilization, reward
 *       modulation for outcome-based learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Kohlberg (1981): Stages of moral development through learning
 * - Bandura (1986): Social learning of moral behavior
 * - Churchland (2011): Braintrust - neural basis of moral learning
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic reward signals modulate moral learning
 * - Oxytocin enhances prosocial learning
 * - Prefrontal-limbic plasticity shapes moral intuitions
 * - Repeated exposure strengthens ethical associations
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of context-outcome pairs
 * - BCM: Stabilize core ethical principles (Asimov's Laws)
 * - Homeostatic: Maintain balanced moral sensitivity
 * - Reward-modulated: Learn from ethical outcomes
 *
 * @see nimcp_ethics.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_ETHICS_PLASTICITY_BRIDGE_H
#define NIMCP_ETHICS_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum ethical synapses */
#define ETHICS_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define ETHICS_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_ETHICS_PLASTICITY     0x0D31

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Ethical synapse types
 */
typedef enum {
    ETHICS_SYNAPSE_HARM_DETECTION = 0, /**< Harm-context associations */
    ETHICS_SYNAPSE_FAIRNESS,            /**< Fairness associations */
    ETHICS_SYNAPSE_GOLDEN_RULE,         /**< Golden Rule learning */
    ETHICS_SYNAPSE_FIRST_LAW,           /**< First Law associations */
    ETHICS_SYNAPSE_EMPATHY,             /**< Empathy connections */
    ETHICS_SYNAPSE_OUTCOME,             /**< Outcome prediction */
    ETHICS_SYNAPSE_CONFLICT             /**< Conflict resolution */
} ethics_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    ETHICS_LEARN_POSITIVE_OUTCOME = 0, /**< Positive ethical outcome */
    ETHICS_LEARN_NEGATIVE_OUTCOME,      /**< Negative ethical outcome */
    ETHICS_LEARN_HARM_AVOIDED,          /**< Successfully avoided harm */
    ETHICS_LEARN_HARM_CAUSED,           /**< Harm was caused */
    ETHICS_LEARN_GOLDEN_RULE_APPLIED,   /**< Golden Rule successfully applied */
    ETHICS_LEARN_CONFLICT_RESOLVED,     /**< Moral conflict resolved */
    ETHICS_LEARN_FIRST_LAW_ACTIVATED    /**< First Law was protective */
} ethics_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    ETHICS_PLASTICITY_STATE_IDLE = 0,
    ETHICS_PLASTICITY_STATE_LEARNING,
    ETHICS_PLASTICITY_STATE_CONSOLIDATING,
    ETHICS_PLASTICITY_STATE_UPDATING,
    ETHICS_PLASTICITY_STATE_ERROR
} ethics_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Ethics-Plasticity bridge configuration
 */
typedef struct {
    /* Learning parameters */
    float base_learning_rate;        /**< Base learning rate */
    float stdp_tau_plus_ms;          /**< STDP potentiation time constant */
    float stdp_tau_minus_ms;         /**< STDP depression time constant */
    float stdp_a_plus;               /**< STDP potentiation magnitude */
    float stdp_a_minus;              /**< STDP depression magnitude */

    /* BCM parameters */
    float bcm_tau_ms;                /**< BCM threshold time constant */
    float bcm_target_rate;           /**< BCM target activity */

    /* Homeostatic parameters */
    float homeostatic_tau_ms;        /**< Homeostatic time constant */
    float target_sensitivity;        /**< Target moral sensitivity */

    /* Reward modulation */
    float reward_learning_boost;     /**< Boost for rewarded outcomes */
    float punishment_learning_boost; /**< Boost for punished outcomes */
    float dopamine_modulation;       /**< Dopamine-like modulation strength */

    /* Weight bounds */
    float weight_min;                /**< Minimum synapse weight */
    float weight_max;                /**< Maximum synapse weight */

    /* Core principle protection */
    bool protect_first_law;          /**< Protect First Law weights */
    bool protect_golden_rule;        /**< Protect Golden Rule weights */
    float protection_strength;       /**< How strongly to protect core principles */

    /* Capacity */
    uint32_t max_synapses;           /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;           /**< Enable bio-async callbacks */
} ethics_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Ethical synapse state
 */
typedef struct {
    uint32_t synapse_id;             /**< Unique synapse ID */
    ethics_synapse_type_t type;      /**< Synapse type */
    float weight;                    /**< Current weight */
    float initial_weight;            /**< Initial weight */
    float eligibility_trace;         /**< Eligibility trace */
    float bcm_threshold;             /**< BCM sliding threshold */
    float avg_activity;              /**< Average pre-synaptic activity */
    uint64_t last_update_us;         /**< Last update timestamp */
    uint32_t update_count;           /**< Number of updates */
    bool is_protected;               /**< Protected from modification */
} ethics_plasticity_synapse_t;

/**
 * @brief Ethical principle state
 */
typedef struct {
    float harm_sensitivity;          /**< Sensitivity to harm */
    float fairness_sensitivity;      /**< Sensitivity to unfairness */
    float empathy_strength;          /**< Empathy connection strength */
    float golden_rule_strength;      /**< Golden Rule adherence */
    float first_law_strength;        /**< First Law priority */
    float learning_rate_mod;         /**< Learning rate modifier */
    uint64_t last_learning_us;       /**< Last learning event */
} ethics_principle_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    ethics_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;        /**< Number of active synapses */
    float mean_weight;               /**< Mean synapse weight */
    float weight_variance;           /**< Weight variance */
    float learning_rate_effective;   /**< Current effective learning rate */
} ethics_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;  /**< Total learning events */
    uint64_t positive_outcomes;      /**< Positive ethical outcomes */
    uint64_t negative_outcomes;      /**< Negative ethical outcomes */
    uint64_t harm_avoidance_events;  /**< Harm avoidance learning */
    uint64_t first_law_activations;  /**< First Law learning events */
    uint64_t weight_updates;         /**< Total weight updates */
    uint64_t protected_updates_blocked; /**< Updates blocked by protection */
    float mean_weight_change;        /**< Mean weight change magnitude */
    float total_potentiation;        /**< Total potentiation */
    float total_depression;          /**< Total depression */
} ethics_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct ethics_plasticity_bridge ethics_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Weight change callback */
typedef void (*ethics_weight_change_cb)(
    uint32_t synapse_id,
    ethics_synapse_type_t type,
    float old_weight,
    float new_weight,
    ethics_learn_event_t event_type,
    void* user_data
);

/** @brief Principle update callback */
typedef void (*ethics_principle_update_cb)(
    const ethics_principle_state_t* state,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
ethics_plasticity_config_t ethics_plasticity_config_default(void);

/**
 * @brief Create ethics plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
ethics_plasticity_bridge_t* ethics_plasticity_create(
    const ethics_plasticity_config_t* config
);

/**
 * @brief Destroy ethics plasticity bridge
 * @param bridge Bridge to destroy
 */
void ethics_plasticity_destroy(ethics_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_reset(ethics_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register ethical synapse
 * @param bridge Bridge handle
 * @param synapse_id Unique synapse ID
 * @param type Synapse type
 * @param initial_weight Initial weight
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_register_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    ethics_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to remove
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_unregister_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_get_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    ethics_plasticity_synapse_t* synapse
);

//=============================================================================
// Learning Functions
//=============================================================================

/**
 * @brief Process learning event
 * @param bridge Bridge handle
 * @param event_type Type of learning event
 * @param context_activation Context activation level
 * @param outcome_value Outcome value [-1, 1]
 * @param timestamp Event timestamp (0 for current time)
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_learn(
    ethics_plasticity_bridge_t* bridge,
    ethics_learn_event_t event_type,
    float context_activation,
    float outcome_value,
    uint64_t timestamp
);

/**
 * @brief Apply STDP update
 * @param bridge Bridge handle
 * @param synapse_id Synapse to update
 * @param pre_spike_time Pre-synaptic spike time
 * @param post_spike_time Post-synaptic spike time
 * @return Weight change applied
 */
float ethics_plasticity_apply_stdp(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t pre_spike_time,
    uint64_t post_spike_time
);

/**
 * @brief Apply reward-modulated learning
 * @param bridge Bridge handle
 * @param reward Reward signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_apply_reward(
    ethics_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_update_traces(
    ethics_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_update_bcm(
    ethics_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param target_activity Target activity level
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_homeostatic_update(
    ethics_plasticity_bridge_t* bridge,
    float target_activity
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_consolidate(ethics_plasticity_bridge_t* bridge);

//=============================================================================
// Principle Protection
//=============================================================================

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse to protect
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_protect_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Unprotect synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unprotect
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_unprotect_synapse(
    ethics_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Protect all First Law synapses
 * @param bridge Bridge handle
 * @return Number of synapses protected
 */
int ethics_plasticity_protect_first_law(ethics_plasticity_bridge_t* bridge);

/**
 * @brief Protect all Golden Rule synapses
 * @param bridge Bridge handle
 * @return Number of synapses protected
 */
int ethics_plasticity_protect_golden_rule(ethics_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get principle state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_get_principle_state(
    ethics_plasticity_bridge_t* bridge,
    ethics_principle_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_get_state(
    ethics_plasticity_bridge_t* bridge,
    ethics_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_get_stats(
    ethics_plasticity_bridge_t* bridge,
    ethics_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_reset_stats(ethics_plasticity_bridge_t* bridge);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register weight change callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_set_weight_callback(
    ethics_plasticity_bridge_t* bridge,
    ethics_weight_change_cb callback,
    void* user_data
);

/**
 * @brief Register principle update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_set_principle_callback(
    ethics_plasticity_bridge_t* bridge,
    ethics_principle_update_cb callback,
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
int ethics_plasticity_bio_async_connect(ethics_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int ethics_plasticity_bio_async_disconnect(ethics_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool ethics_plasticity_is_bio_async_connected(ethics_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ETHICS_PLASTICITY_BRIDGE_H */
