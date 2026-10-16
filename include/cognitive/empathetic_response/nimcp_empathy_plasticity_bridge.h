/**
 * @file nimcp_empathy_plasticity_bridge.h
 * @brief Empathy - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between empathy engine and synaptic plasticity
 * WHY:  Enable learning of empathetic response patterns from experience and feedback
 * HOW:  STDP for emotional association learning, BCM for stabilization, reward
 *       modulation for compassionate response learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Decety & Lamm (2006): Human empathy through the lens of social neuroscience
 * - Singer et al. (2004): Empathy for pain involves affective but not sensory components
 * - Klimecki et al. (2013): Functional neural plasticity and compassion training
 *
 * BIOLOGICAL BASIS:
 * - Anterior insula plasticity for interoceptive awareness
 * - Mirror neuron system adaptation for emotional mirroring
 * - Prefrontal plasticity for perspective-taking improvement
 * - Reward circuits modulate compassionate action learning
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of emotional stimulus-response pairs
 * - BCM: Stabilize core empathetic response patterns
 * - Homeostatic: Maintain balanced emotional regulation
 * - Reward-modulated: Learn from compassionate action outcomes
 *
 * @see nimcp_empathetic_response.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_EMPATHY_PLASTICITY_BRIDGE_H
#define NIMCP_EMPATHY_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum empathy synapses */
#define EMPATHY_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define EMPATHY_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_EMPATHY_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Empathy synapse types
 */
typedef enum {
    EMPATHY_SYNAPSE_MIRRORING = 0,       /**< Emotional mirroring */
    EMPATHY_SYNAPSE_PERSPECTIVE,          /**< Perspective-taking (PROTECTED) */
    EMPATHY_SYNAPSE_AFFECTIVE,            /**< Affective sharing */
    EMPATHY_SYNAPSE_COMPASSION,           /**< Compassion response */
    EMPATHY_SYNAPSE_REGULATION,           /**< Emotion regulation (PROTECTED) */
    EMPATHY_SYNAPSE_VALIDATION            /**< Validation response */
} empathy_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    EMPATHY_LEARN_MIRRORING_ACCURATE = 0, /**< Accurate emotional mirroring */
    EMPATHY_LEARN_MIRRORING_ERROR,         /**< Mirroring error */
    EMPATHY_LEARN_PERSPECTIVE_SUCCESS,     /**< Successful perspective-taking */
    EMPATHY_LEARN_PERSPECTIVE_FAILURE,     /**< Perspective-taking failure */
    EMPATHY_LEARN_COMPASSION_EFFECTIVE,    /**< Effective compassion response */
    EMPATHY_LEARN_COMPASSION_INEFFECTIVE,  /**< Ineffective compassion response */
    EMPATHY_LEARN_VALIDATION_ACCEPTED,     /**< Validation response accepted */
    EMPATHY_LEARN_VALIDATION_REJECTED,     /**< Validation response rejected */
    EMPATHY_LEARN_DEESCALATION_SUCCESS,    /**< Successful de-escalation */
    EMPATHY_LEARN_BOUNDARY_RESPECTED       /**< Appropriate boundary setting */
} empathy_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    EMPATHY_PLASTICITY_STATE_IDLE = 0,
    EMPATHY_PLASTICITY_STATE_LEARNING,
    EMPATHY_PLASTICITY_STATE_CONSOLIDATING,
    EMPATHY_PLASTICITY_STATE_UPDATING,
    EMPATHY_PLASTICITY_STATE_ERROR
} empathy_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Empathy-Plasticity bridge configuration
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
    float target_empathy;                /**< Target empathy level */

    /* Reward modulation */
    float compassion_learning_boost;     /**< Boost for effective compassion */
    float validation_learning_boost;     /**< Boost for accepted validation */
    float mirroring_modulation;          /**< Mirroring learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_perspective_taking;     /**< Protect perspective-taking weights */
    bool protect_emotion_regulation;     /**< Protect emotion regulation weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} empathy_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Empathy synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    empathy_synapse_type_t type;         /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} empathy_plasticity_synapse_t;

/**
 * @brief Empathic capacity state
 */
typedef struct {
    float mirroring_accuracy;            /**< Mirroring accuracy level */
    float perspective_calibration;       /**< Perspective-taking calibration */
    float compassion_sensitivity;        /**< Sensitivity to compassion cues */
    float validation_effectiveness;      /**< Validation effectiveness */
    float regulation_strength;           /**< Emotion regulation strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} empathy_capacity_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    empathy_plasticity_state_t state;    /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} empathy_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t mirroring_accurate_events;  /**< Accurate mirroring events */
    uint64_t mirroring_error_events;     /**< Mirroring error corrections */
    uint64_t compassion_effective_events;/**< Effective compassion events */
    uint64_t validation_accepted_events; /**< Validation accepted learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} empathy_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct empathy_plasticity_bridge empathy_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*empathy_plasticity_learn_callback_t)(
    empathy_plasticity_bridge_t* bridge,
    empathy_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Empathic capacity update callback */
typedef void (*empathy_plasticity_capacity_callback_t)(
    empathy_plasticity_bridge_t* bridge,
    float old_capacity,
    float new_capacity,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
empathy_plasticity_config_t empathy_plasticity_config_default(void);

/**
 * @brief Create empathy plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
empathy_plasticity_bridge_t* empathy_plasticity_create(
    const empathy_plasticity_config_t* config
);

/**
 * @brief Destroy empathy plasticity bridge
 * @param bridge Bridge to destroy
 */
void empathy_plasticity_destroy(empathy_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_reset(empathy_plasticity_bridge_t* bridge);

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
int empathy_plasticity_register_synapse(
    empathy_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    empathy_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_unregister_synapse(
    empathy_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_get_synapse(
    empathy_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    empathy_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_protect_synapse(
    empathy_plasticity_bridge_t* bridge,
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
int empathy_plasticity_learn(
    empathy_plasticity_bridge_t* bridge,
    empathy_learn_event_t event,
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
float empathy_plasticity_apply_stdp(
    empathy_plasticity_bridge_t* bridge,
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
int empathy_plasticity_apply_reward(
    empathy_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_update_bcm(
    empathy_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_homeostatic_update(
    empathy_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_update_traces(
    empathy_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_consolidate(empathy_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get empathic capacity state
 * @param bridge Bridge handle
 * @param state Output capacity state
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_get_capacity_state(
    empathy_plasticity_bridge_t* bridge,
    empathy_capacity_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_get_state(
    empathy_plasticity_bridge_t* bridge,
    empathy_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_get_stats(
    empathy_plasticity_bridge_t* bridge,
    empathy_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_reset_stats(empathy_plasticity_bridge_t* bridge);

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
int empathy_plasticity_register_learn_callback(
    empathy_plasticity_bridge_t* bridge,
    empathy_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register capacity update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_register_capacity_callback(
    empathy_plasticity_bridge_t* bridge,
    empathy_plasticity_capacity_callback_t callback,
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
int empathy_plasticity_bio_async_connect(empathy_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int empathy_plasticity_bio_async_disconnect(empathy_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool empathy_plasticity_is_bio_async_connected(empathy_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMPATHY_PLASTICITY_BRIDGE_H */
