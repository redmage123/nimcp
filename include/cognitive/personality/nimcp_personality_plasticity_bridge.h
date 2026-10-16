/**
 * @file nimcp_personality_plasticity_bridge.h
 * @brief Personality - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between personality engine and synaptic plasticity
 * WHY:  Enable learning of personality trait adjustments from experience and feedback
 * HOW:  STDP for trait-behavior associations, BCM for stabilization, reward
 *       modulation for personality adaptation learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Roberts & Mroczek (2008): Personality trait change in adulthood
 * - Costa & McCrae (1994): Trait stability and plasticity
 * - Kandel (2001): Cellular mechanisms of learning and personality
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal plasticity for conscientiousness adaptation
 * - Amygdala plasticity for neuroticism/emotional learning
 * - Striatal circuits for approach/avoidance learning
 * - Serotonergic modulation of trait stability
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of trait-outcome pairs
 * - BCM: Stabilize core personality patterns
 * - Homeostatic: Maintain trait balance
 * - Reward-modulated: Learn from behavioral success/failure
 *
 * @see nimcp_personality.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_PERSONALITY_PLASTICITY_BRIDGE_H
#define NIMCP_PERSONALITY_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum personality synapses */
#define PERSONALITY_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define PERSONALITY_PLASTICITY_DEFAULT_LR     0.005f

/** @brief Bio-async module ID */
#define BIO_MODULE_PERSONALITY_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Personality synapse types
 */
typedef enum {
    PERSONALITY_SYNAPSE_OPENNESS = 0,        /**< Openness trait */
    PERSONALITY_SYNAPSE_CONSCIENTIOUSNESS,    /**< Conscientiousness trait (PROTECTED) */
    PERSONALITY_SYNAPSE_EXTRAVERSION,         /**< Extraversion trait */
    PERSONALITY_SYNAPSE_AGREEABLENESS,        /**< Agreeableness trait */
    PERSONALITY_SYNAPSE_NEUROTICISM,          /**< Neuroticism trait */
    PERSONALITY_SYNAPSE_APPROACH,             /**< Approach tendency */
    PERSONALITY_SYNAPSE_AVOIDANCE,            /**< Avoidance tendency */
    PERSONALITY_SYNAPSE_STABILITY             /**< Trait stability (PROTECTED) */
} personality_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    PERSONALITY_LEARN_TRAIT_CONFIRMED = 0,   /**< Trait behavior was successful */
    PERSONALITY_LEARN_TRAIT_MISMATCH,         /**< Trait-behavior mismatch */
    PERSONALITY_LEARN_SOCIAL_SUCCESS,         /**< Social behavior success */
    PERSONALITY_LEARN_SOCIAL_FAILURE,         /**< Social behavior failure */
    PERSONALITY_LEARN_APPROACH_REWARDED,      /**< Approach behavior rewarded */
    PERSONALITY_LEARN_AVOIDANCE_REWARDED,     /**< Avoidance behavior rewarded */
    PERSONALITY_LEARN_EMOTIONAL_REGULATION,   /**< Emotional regulation success */
    PERSONALITY_LEARN_STRESS_ADAPTATION,      /**< Stress response adaptation */
    PERSONALITY_LEARN_OPENNESS_REWARDED,      /**< Openness behavior rewarded */
    PERSONALITY_LEARN_STABILITY_EVENT         /**< Trait stability reinforcement */
} personality_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    PERSONALITY_PLASTICITY_STATE_IDLE = 0,
    PERSONALITY_PLASTICITY_STATE_LEARNING,
    PERSONALITY_PLASTICITY_STATE_CONSOLIDATING,
    PERSONALITY_PLASTICITY_STATE_UPDATING,
    PERSONALITY_PLASTICITY_STATE_ERROR
} personality_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Personality-Plasticity bridge configuration
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
    float target_trait_level;            /**< Target trait balance level */

    /* Reward modulation */
    float social_learning_boost;         /**< Boost for social success */
    float emotional_learning_boost;      /**< Boost for emotional regulation */
    float trait_modulation;              /**< Trait learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_conscientiousness;      /**< Protect conscientiousness weights */
    bool protect_stability;              /**< Protect stability weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} personality_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Personality synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    personality_synapse_type_t type;     /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} personality_plasticity_synapse_t;

/**
 * @brief Trait adaptation state
 */
typedef struct {
    float openness_sensitivity;          /**< Sensitivity to openness learning */
    float conscientiousness_calibration; /**< Conscientiousness calibration level */
    float extraversion_sensitivity;      /**< Sensitivity to extraversion learning */
    float agreeableness_sensitivity;     /**< Sensitivity to agreeableness learning */
    float neuroticism_sensitivity;       /**< Sensitivity to neuroticism learning */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} personality_adaptation_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    personality_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} personality_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t trait_confirmed_events;     /**< Trait confirmed events */
    uint64_t trait_mismatch_events;      /**< Trait mismatch corrections */
    uint64_t social_success_events;      /**< Social success events */
    uint64_t emotional_regulation_events; /**< Emotional regulation learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} personality_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct personality_plasticity_bridge personality_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*personality_plasticity_learn_callback_t)(
    personality_plasticity_bridge_t* bridge,
    personality_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Trait adaptation callback */
typedef void (*personality_plasticity_adaptation_callback_t)(
    personality_plasticity_bridge_t* bridge,
    float old_trait_level,
    float new_trait_level,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
personality_plasticity_config_t personality_plasticity_config_default(void);

/**
 * @brief Create personality plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
personality_plasticity_bridge_t* personality_plasticity_create(
    const personality_plasticity_config_t* config
);

/**
 * @brief Destroy personality plasticity bridge
 * @param bridge Bridge to destroy
 */
void personality_plasticity_destroy(personality_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_reset(personality_plasticity_bridge_t* bridge);

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
int personality_plasticity_register_synapse(
    personality_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    personality_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_unregister_synapse(
    personality_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_get_synapse(
    personality_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    personality_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_protect_synapse(
    personality_plasticity_bridge_t* bridge,
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
int personality_plasticity_learn(
    personality_plasticity_bridge_t* bridge,
    personality_learn_event_t event,
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
float personality_plasticity_apply_stdp(
    personality_plasticity_bridge_t* bridge,
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
int personality_plasticity_apply_reward(
    personality_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_update_bcm(
    personality_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_homeostatic_update(
    personality_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_update_traces(
    personality_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_consolidate(personality_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get trait adaptation state
 * @param bridge Bridge handle
 * @param state Output adaptation state
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_get_adaptation_state(
    personality_plasticity_bridge_t* bridge,
    personality_adaptation_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_get_state(
    personality_plasticity_bridge_t* bridge,
    personality_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_get_stats(
    personality_plasticity_bridge_t* bridge,
    personality_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_reset_stats(personality_plasticity_bridge_t* bridge);

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
int personality_plasticity_register_learn_callback(
    personality_plasticity_bridge_t* bridge,
    personality_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register trait adaptation callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_register_adaptation_callback(
    personality_plasticity_bridge_t* bridge,
    personality_plasticity_adaptation_callback_t callback,
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
int personality_plasticity_bio_async_connect(personality_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int personality_plasticity_bio_async_disconnect(personality_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool personality_plasticity_is_bio_async_connected(personality_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PERSONALITY_PLASTICITY_BRIDGE_H */
