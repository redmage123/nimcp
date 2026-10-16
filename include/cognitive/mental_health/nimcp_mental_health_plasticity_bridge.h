/**
 * @file nimcp_mental_health_plasticity_bridge.h
 * @brief Mental Health - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between mental health monitoring and synaptic plasticity
 * WHY:  Enable learning of emotional regulation patterns and resilience factors
 * HOW:  STDP for mood-outcome associations, BCM for stabilization, reward
 *       modulation for coping strategy learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Davidson (2000): Affective neuroscience and neuroplasticity
 * - McEwen (2007): Stress-induced structural plasticity
 * - Krystal & Bhagwagar (2007): Depression and synaptic plasticity
 * - Duman & Aghajanian (2012): Synaptic plasticity and depression
 *
 * BIOLOGICAL BASIS:
 * - Chronic stress reduces hippocampal plasticity
 * - Antidepressant effects mediated through BDNF and plasticity
 * - Emotional regulation strengthens prefrontal-amygdala connections
 * - Resilience associated with preserved synaptic plasticity under stress
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of mood-behavior pairs
 * - BCM: Stabilize healthy emotional patterns
 * - Homeostatic: Maintain balanced mood states
 * - Stress-modulated: Adaptation to stress exposure
 *
 * @see nimcp_mental_health.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_MENTAL_HEALTH_PLASTICITY_BRIDGE_H
#define NIMCP_MENTAL_HEALTH_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum mental health synapses */
#define MENTAL_HEALTH_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define MENTAL_HEALTH_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_MENTAL_HEALTH_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Mental health synapse types
 */
typedef enum {
    MENTAL_HEALTH_SYNAPSE_MOOD_REGULATION = 0,   /**< Mood regulation (PROTECTED) */
    MENTAL_HEALTH_SYNAPSE_ANXIETY_RESPONSE,       /**< Anxiety response pathway */
    MENTAL_HEALTH_SYNAPSE_DEPRESSION_BUFFER,      /**< Depression buffer (PROTECTED) */
    MENTAL_HEALTH_SYNAPSE_STRESS_COPING,          /**< Stress coping mechanism */
    MENTAL_HEALTH_SYNAPSE_RESILIENCE,             /**< Resilience factor (PROTECTED) */
    MENTAL_HEALTH_SYNAPSE_SOCIAL_SUPPORT          /**< Social support processing */
} mental_health_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    MENTAL_HEALTH_LEARN_MOOD_IMPROVED = 0,       /**< Mood state improved */
    MENTAL_HEALTH_LEARN_MOOD_DECLINED,            /**< Mood state declined */
    MENTAL_HEALTH_LEARN_ANXIETY_REDUCED,          /**< Anxiety successfully reduced */
    MENTAL_HEALTH_LEARN_ANXIETY_INCREASED,        /**< Anxiety spike occurred */
    MENTAL_HEALTH_LEARN_COPING_SUCCESS,           /**< Coping strategy worked */
    MENTAL_HEALTH_LEARN_COPING_FAILURE,           /**< Coping strategy failed */
    MENTAL_HEALTH_LEARN_STRESS_RECOVERED,         /**< Stress recovery completed */
    MENTAL_HEALTH_LEARN_STRESS_PROLONGED,         /**< Chronic stress condition */
    MENTAL_HEALTH_LEARN_RESILIENCE_DEMONSTRATED,  /**< Resilience under pressure */
    MENTAL_HEALTH_LEARN_SOCIAL_SUPPORT_RECEIVED   /**< Social support engagement */
} mental_health_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    MENTAL_HEALTH_PLASTICITY_STATE_IDLE = 0,
    MENTAL_HEALTH_PLASTICITY_STATE_LEARNING,
    MENTAL_HEALTH_PLASTICITY_STATE_CONSOLIDATING,
    MENTAL_HEALTH_PLASTICITY_STATE_UPDATING,
    MENTAL_HEALTH_PLASTICITY_STATE_ERROR
} mental_health_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Mental Health-Plasticity bridge configuration
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
    float target_mood_level;             /**< Target mood equilibrium */

    /* Stress modulation */
    float stress_learning_reduction;     /**< Stress reduces learning capacity */
    float resilience_learning_boost;     /**< Boost for resilience events */
    float mood_improvement_boost;        /**< Boost for mood improvements */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_mood_regulation;        /**< Protect mood regulation weights */
    bool protect_depression_buffer;      /**< Protect depression buffer weights */
    bool protect_resilience;             /**< Protect resilience weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} mental_health_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Mental health synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    mental_health_synapse_type_t type;   /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} mental_health_plasticity_synapse_t;

/**
 * @brief Emotional regulation learning state
 */
typedef struct {
    float mood_regulation_strength;      /**< Strength of mood regulation */
    float anxiety_coping_calibration;    /**< Anxiety coping calibration */
    float depression_resistance;         /**< Depression resistance level */
    float stress_resilience;             /**< Stress resilience factor */
    float social_support_sensitivity;    /**< Sensitivity to social support */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} mental_health_regulation_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    mental_health_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} mental_health_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t mood_improvement_events;    /**< Mood improvement events */
    uint64_t mood_decline_events;        /**< Mood decline events */
    uint64_t coping_success_events;      /**< Coping success events */
    uint64_t resilience_events;          /**< Resilience demonstration events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} mental_health_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct mental_health_plasticity_bridge mental_health_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*mental_health_plasticity_learn_callback_t)(
    mental_health_plasticity_bridge_t* bridge,
    mental_health_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Regulation update callback */
typedef void (*mental_health_plasticity_regulation_callback_t)(
    mental_health_plasticity_bridge_t* bridge,
    float old_regulation,
    float new_regulation,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
mental_health_plasticity_config_t mental_health_plasticity_config_default(void);

/**
 * @brief Create mental health plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
mental_health_plasticity_bridge_t* mental_health_plasticity_create(
    const mental_health_plasticity_config_t* config
);

/**
 * @brief Destroy mental health plasticity bridge
 * @param bridge Bridge to destroy
 */
void mental_health_plasticity_destroy(mental_health_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_reset(mental_health_plasticity_bridge_t* bridge);

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
int mental_health_plasticity_register_synapse(
    mental_health_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    mental_health_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_unregister_synapse(
    mental_health_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_get_synapse(
    mental_health_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    mental_health_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_protect_synapse(
    mental_health_plasticity_bridge_t* bridge,
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
int mental_health_plasticity_learn(
    mental_health_plasticity_bridge_t* bridge,
    mental_health_learn_event_t event,
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
float mental_health_plasticity_apply_stdp(
    mental_health_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply stress modulation
 * @param bridge Bridge handle
 * @param stress_level Current stress level [0, 1]
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_apply_stress(
    mental_health_plasticity_bridge_t* bridge,
    float stress_level
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_update_bcm(
    mental_health_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_homeostatic_update(
    mental_health_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_update_traces(
    mental_health_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_consolidate(mental_health_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get regulation state
 * @param bridge Bridge handle
 * @param state Output regulation state
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_get_regulation_state(
    mental_health_plasticity_bridge_t* bridge,
    mental_health_regulation_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_get_state(
    mental_health_plasticity_bridge_t* bridge,
    mental_health_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_get_stats(
    mental_health_plasticity_bridge_t* bridge,
    mental_health_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_reset_stats(mental_health_plasticity_bridge_t* bridge);

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
int mental_health_plasticity_register_learn_callback(
    mental_health_plasticity_bridge_t* bridge,
    mental_health_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register regulation update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_register_regulation_callback(
    mental_health_plasticity_bridge_t* bridge,
    mental_health_plasticity_regulation_callback_t callback,
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
int mental_health_plasticity_bio_async_connect(mental_health_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int mental_health_plasticity_bio_async_disconnect(mental_health_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool mental_health_plasticity_is_bio_async_connected(mental_health_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_PLASTICITY_BRIDGE_H */
