/**
 * @file nimcp_tom_plasticity_bridge.h
 * @brief Theory of Mind - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between Theory of Mind engine and synaptic plasticity
 * WHY:  Enable learning of social cognition patterns from experience and feedback
 * HOW:  STDP for mentalizing associations, BCM for belief stabilization, reward
 *       modulation for social prediction learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Premack & Woodruff (1978): Theory of Mind concept origin
 * - Saxe & Kanwisher (2003): Selectivity of ToM brain regions
 * - Waytz et al. (2010): Social cognition and anthropomorphism
 * - Buckner & Carroll (2007): Default network and mentalizing
 *
 * BIOLOGICAL BASIS:
 * - TPJ plasticity enhances perspective-taking ability
 * - mPFC plasticity improves mental state inference
 * - Dopaminergic signals modulate social prediction accuracy
 * - STS plasticity refines intention recognition
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of belief-outcome pairs
 * - BCM: Stabilize core belief attribution patterns
 * - Homeostatic: Maintain balanced social cognition
 * - Reward-modulated: Learn from social prediction accuracy
 *
 * PROTECTED SYNAPSES:
 * - BELIEF: Core belief state representation (fundamental to ToM)
 * - PERSPECTIVE: Perspective-taking capability (essential for empathy)
 *
 * @see nimcp_theory_of_mind.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_TOM_PLASTICITY_BRIDGE_H
#define NIMCP_TOM_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum ToM synapses */
#define TOM_PLASTICITY_MAX_SYNAPSES     256

/** @brief Default learning rate */
#define TOM_PLASTICITY_DEFAULT_LR       0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_TOM_PLASTICITY       0x0D51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Theory of Mind synapse types
 */
typedef enum {
    TOM_SYNAPSE_BELIEF = 0,             /**< Belief state representation */
    TOM_SYNAPSE_DESIRE,                  /**< Desire/goal inference */
    TOM_SYNAPSE_INTENTION,               /**< Intention recognition */
    TOM_SYNAPSE_PERSPECTIVE,             /**< Perspective-taking */
    TOM_SYNAPSE_EMPATHY,                 /**< Empathic processing */
    TOM_SYNAPSE_SOCIAL                   /**< Social context */
} tom_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    TOM_LEARN_CORRECT_BELIEF = 0,        /**< Belief prediction was accurate */
    TOM_LEARN_FALSE_BELIEF_DETECTED,     /**< False belief correctly detected */
    TOM_LEARN_FALSE_BELIEF_MISSED,       /**< Missed false belief */
    TOM_LEARN_INTENTION_CORRECT,         /**< Intention correctly predicted */
    TOM_LEARN_INTENTION_WRONG,           /**< Intention incorrectly predicted */
    TOM_LEARN_PERSPECTIVE_ALIGNED,       /**< Perspective correctly taken */
    TOM_LEARN_PERSPECTIVE_ERROR,         /**< Perspective error */
    TOM_LEARN_EMPATHY_ACCURATE,          /**< Empathic inference accurate */
    TOM_LEARN_EMPATHY_ERROR,             /**< Empathic inference error */
    TOM_LEARN_DECEPTION_CORRECT,         /**< Deception correctly detected */
    TOM_LEARN_DECEPTION_MISSED,          /**< Deception missed */
    TOM_LEARN_SOCIAL_CONTEXT_CORRECT     /**< Social context correctly understood */
} tom_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    TOM_PLASTICITY_STATE_IDLE = 0,
    TOM_PLASTICITY_STATE_LEARNING,
    TOM_PLASTICITY_STATE_CONSOLIDATING,
    TOM_PLASTICITY_STATE_UPDATING,
    TOM_PLASTICITY_STATE_ERROR
} tom_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief ToM-Plasticity bridge configuration
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
    float target_empathy;                /**< Target empathy calibration */

    /* Reward modulation */
    float belief_learning_boost;         /**< Boost for accurate belief predictions */
    float deception_learning_boost;      /**< Boost for deception learning */
    float empathy_modulation;            /**< Empathy learning strength */
    float perspective_modulation;        /**< Perspective learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_belief_patterns;        /**< Protect core belief patterns */
    bool protect_perspective_patterns;   /**< Protect perspective-taking */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} tom_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief ToM synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    tom_synapse_type_t type;             /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} tom_plasticity_synapse_t;

/**
 * @brief Social cognition calibration state
 */
typedef struct {
    float belief_sensitivity;            /**< Sensitivity to belief states */
    float intention_calibration;         /**< Intention inference calibration */
    float perspective_sensitivity;       /**< Sensitivity to perspective */
    float empathy_strength;              /**< Empathic processing strength */
    float deception_sensitivity;         /**< Sensitivity to deception */
    float social_calibration;            /**< Social context calibration */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} tom_calibration_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    tom_plasticity_state_t state;        /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} tom_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t correct_belief_events;      /**< Correct belief predictions */
    uint64_t false_belief_detections;    /**< False belief detections */
    uint64_t false_belief_misses;        /**< Missed false beliefs */
    uint64_t perspective_alignments;     /**< Perspective alignments */
    uint64_t empathy_accurate_events;    /**< Accurate empathy inferences */
    uint64_t deception_detections;       /**< Deception detections */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} tom_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct tom_plasticity_bridge tom_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*tom_plasticity_learn_callback_t)(
    tom_plasticity_bridge_t* bridge,
    tom_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Calibration update callback */
typedef void (*tom_plasticity_calibration_callback_t)(
    tom_plasticity_bridge_t* bridge,
    float old_calibration,
    float new_calibration,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
tom_plasticity_config_t tom_plasticity_config_default(void);

/**
 * @brief Create ToM plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
tom_plasticity_bridge_t* tom_plasticity_create(
    const tom_plasticity_config_t* config
);

/**
 * @brief Destroy ToM plasticity bridge
 * @param bridge Bridge to destroy
 */
void tom_plasticity_destroy(tom_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_reset(tom_plasticity_bridge_t* bridge);

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
int tom_plasticity_register_synapse(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    tom_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_unregister_synapse(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_get_synapse(
    tom_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    tom_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_protect_synapse(
    tom_plasticity_bridge_t* bridge,
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
int tom_plasticity_learn(
    tom_plasticity_bridge_t* bridge,
    tom_learn_event_t event,
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
float tom_plasticity_apply_stdp(
    tom_plasticity_bridge_t* bridge,
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
int tom_plasticity_apply_reward(
    tom_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_update_bcm(
    tom_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_homeostatic_update(
    tom_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_update_traces(
    tom_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_consolidate(tom_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get calibration state
 * @param bridge Bridge handle
 * @param state Output calibration state
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_get_calibration_state(
    tom_plasticity_bridge_t* bridge,
    tom_calibration_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_get_state(
    tom_plasticity_bridge_t* bridge,
    tom_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_get_stats(
    tom_plasticity_bridge_t* bridge,
    tom_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_reset_stats(tom_plasticity_bridge_t* bridge);

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
int tom_plasticity_register_learn_callback(
    tom_plasticity_bridge_t* bridge,
    tom_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register calibration update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_register_calibration_callback(
    tom_plasticity_bridge_t* bridge,
    tom_plasticity_calibration_callback_t callback,
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
int tom_plasticity_bio_async_connect(tom_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int tom_plasticity_bio_async_disconnect(tom_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool tom_plasticity_is_bio_async_connected(tom_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TOM_PLASTICITY_BRIDGE_H */
