/**
 * @file nimcp_wellbeing_plasticity_bridge.h
 * @brief Wellbeing - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between wellbeing system and synaptic plasticity
 * WHY:  Enable learning of wellbeing patterns from experience and lifestyle feedback
 * HOW:  STDP for wellbeing associations, BCM for stabilization, reward
 *       modulation for positive psychology interventions
 *
 * THEORETICAL FOUNDATIONS:
 * - Fredrickson (2001): Broaden-and-build theory of positive emotions
 * - Ryff & Singer (2008): Know thyself and become what you are
 * - Davidson (2000): Affective neuroscience of wellbeing
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal-limbic plasticity shapes emotional regulation
 * - Dopaminergic reward circuits modulate hedonic learning
 * - Oxytocin pathways strengthen social connection associations
 * - Repeated positive experiences strengthen wellbeing circuits
 * - Resilience involves stress inoculation and recovery plasticity
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of wellbeing-context pairs
 * - BCM: Stabilize core resilience and flourishing patterns
 * - Homeostatic: Maintain balanced hedonic/eudaimonic processing
 * - Reward-modulated: Learn from positive life events
 *
 * PROTECTED SYNAPSES:
 * - RESILIENCE: Core psychological resilience (protected from erosion)
 * - FLOURISHING: Core flourishing capacity (protected foundation)
 *
 * @see nimcp_wellbeing.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_WELLBEING_PLASTICITY_BRIDGE_H
#define NIMCP_WELLBEING_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum wellbeing synapses */
#define WELLBEING_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define WELLBEING_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_WELLBEING_PLASTICITY     0x0D51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Wellbeing synapse types
 */
typedef enum {
    WELLBEING_SYNAPSE_HEDONIC = 0,    /**< Hedonic (pleasure) associations */
    WELLBEING_SYNAPSE_EUDAIMONIC,      /**< Eudaimonic (meaning) associations */
    WELLBEING_SYNAPSE_VITALITY,        /**< Vitality/energy connections */
    WELLBEING_SYNAPSE_RESILIENCE,      /**< Resilience pathways (PROTECTED) */
    WELLBEING_SYNAPSE_SOCIAL,          /**< Social connection learning */
    WELLBEING_SYNAPSE_BALANCE          /**< Life balance associations */
} wellbeing_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    WELLBEING_LEARN_POSITIVE_EXPERIENCE = 0, /**< Positive life experience */
    WELLBEING_LEARN_NEGATIVE_EXPERIENCE,      /**< Negative life experience */
    WELLBEING_LEARN_STRESS_RECOVERED,         /**< Successfully recovered from stress */
    WELLBEING_LEARN_STRESS_ACCUMULATED,       /**< Chronic stress accumulation */
    WELLBEING_LEARN_SOCIAL_SUPPORT,           /**< Social support received */
    WELLBEING_LEARN_MEANING_FOUND,            /**< Discovered meaning/purpose */
    WELLBEING_LEARN_GOAL_ACHIEVED,            /**< Achieved meaningful goal */
    WELLBEING_LEARN_BALANCE_IMPROVED          /**< Life balance improved */
} wellbeing_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    WELLBEING_PLASTICITY_STATE_IDLE = 0,
    WELLBEING_PLASTICITY_STATE_LEARNING,
    WELLBEING_PLASTICITY_STATE_CONSOLIDATING,
    WELLBEING_PLASTICITY_STATE_UPDATING,
    WELLBEING_PLASTICITY_STATE_ERROR
} wellbeing_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Wellbeing-Plasticity bridge configuration
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
    float target_wellbeing;              /**< Target wellbeing level */

    /* Reward modulation */
    float positive_learning_boost;       /**< Boost for positive experiences */
    float negative_learning_rate;        /**< Rate for negative experiences */
    float social_modulation;             /**< Social support learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core wellbeing protection */
    bool protect_resilience;             /**< Protect resilience weights */
    bool protect_flourishing;            /**< Protect flourishing weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} wellbeing_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Wellbeing synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    wellbeing_synapse_type_t type;       /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} wellbeing_plasticity_synapse_t;

/**
 * @brief Wellbeing foundation state
 */
typedef struct {
    float hedonic_sensitivity;           /**< Sensitivity to pleasure */
    float eudaimonic_strength;           /**< Meaning-seeking strength */
    float vitality_capacity;             /**< Energy/vitality capacity */
    float resilience_level;              /**< Current resilience level */
    float social_connection_strength;    /**< Social bonding strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} wellbeing_foundation_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    wellbeing_plasticity_state_t state;  /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} wellbeing_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t positive_experience_events; /**< Positive experience events */
    uint64_t negative_experience_events; /**< Negative experience events */
    uint64_t stress_recovery_events;     /**< Stress recovery learning */
    uint64_t social_support_events;      /**< Social support learning events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} wellbeing_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct wellbeing_plasticity_bridge wellbeing_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*wellbeing_plasticity_learn_callback_t)(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Foundation update callback */
typedef void (*wellbeing_plasticity_foundation_callback_t)(
    wellbeing_plasticity_bridge_t* bridge,
    float old_level,
    float new_level,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
wellbeing_plasticity_config_t wellbeing_plasticity_config_default(void);

/**
 * @brief Create wellbeing plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
wellbeing_plasticity_bridge_t* wellbeing_plasticity_create(
    const wellbeing_plasticity_config_t* config
);

/**
 * @brief Destroy wellbeing plasticity bridge
 * @param bridge Bridge to destroy
 */
void wellbeing_plasticity_destroy(wellbeing_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_reset(wellbeing_plasticity_bridge_t* bridge);

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
int wellbeing_plasticity_register_synapse(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    wellbeing_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_unregister_synapse(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_get_synapse(
    wellbeing_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    wellbeing_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_protect_synapse(
    wellbeing_plasticity_bridge_t* bridge,
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
int wellbeing_plasticity_learn(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_learn_event_t event,
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
float wellbeing_plasticity_apply_stdp(
    wellbeing_plasticity_bridge_t* bridge,
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
int wellbeing_plasticity_apply_reward(
    wellbeing_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_update_bcm(
    wellbeing_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_homeostatic_update(
    wellbeing_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_update_traces(
    wellbeing_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_consolidate(wellbeing_plasticity_bridge_t* bridge);

//=============================================================================
// Wellbeing Protection Functions
//=============================================================================

/**
 * @brief Protect all resilience synapses
 * @param bridge Bridge handle
 * @return Number of synapses protected
 */
int wellbeing_plasticity_protect_resilience(wellbeing_plasticity_bridge_t* bridge);

/**
 * @brief Protect all flourishing foundation synapses
 * @param bridge Bridge handle
 * @return Number of synapses protected
 */
int wellbeing_plasticity_protect_flourishing(wellbeing_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get foundation state
 * @param bridge Bridge handle
 * @param state Output foundation state
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_get_foundation_state(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_foundation_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_get_state(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_get_stats(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_reset_stats(wellbeing_plasticity_bridge_t* bridge);

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
int wellbeing_plasticity_register_learn_callback(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register foundation update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_register_foundation_callback(
    wellbeing_plasticity_bridge_t* bridge,
    wellbeing_plasticity_foundation_callback_t callback,
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
int wellbeing_plasticity_bio_async_connect(wellbeing_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int wellbeing_plasticity_bio_async_disconnect(wellbeing_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool wellbeing_plasticity_is_bio_async_connected(wellbeing_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_PLASTICITY_BRIDGE_H */
