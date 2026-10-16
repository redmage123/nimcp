/**
 * @file nimcp_self_awareness_plasticity_bridge.h
 * @brief Self-Awareness - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between self-awareness engine and synaptic plasticity
 * WHY:  Enable learning of self-recognition patterns and agency attribution from
 *       experience and feedback
 * HOW:  STDP for self-recognition associations, BCM for stabilization, reward
 *       modulation for metacognitive learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Dehaene & Changeux (2011): Experimental and theoretical approaches to consciousness
 * - Damasio (2010): Self comes to mind - constructing the conscious brain
 * - Gallagher & Zahavi (2008): The phenomenological mind
 * - Hohwy (2013): The predictive mind
 *
 * BIOLOGICAL BASIS:
 * - Medial prefrontal cortex plasticity for self-referential learning
 * - Insula synaptic changes for interoceptive calibration
 * - TPJ plasticity for agency attribution refinement
 * - ACC plasticity for metacognitive monitoring adjustment
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of self-recognition-outcome pairs
 * - BCM: Stabilize core self-model patterns
 * - Homeostatic: Maintain balanced self-awareness levels
 * - Reward-modulated: Learn from agency prediction success
 *
 * @see nimcp_self_awareness_extended.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_SELF_AWARENESS_PLASTICITY_BRIDGE_H
#define NIMCP_SELF_AWARENESS_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum self-awareness synapses */
#define SELF_AWARENESS_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define SELF_AWARENESS_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_SELF_AWARENESS_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Self-awareness synapse types
 */
typedef enum {
    SELF_SYNAPSE_RECOGNITION = 0,        /**< Self-recognition */
    SELF_SYNAPSE_BODY_OWNERSHIP,         /**< Body ownership (PROTECTED) */
    SELF_SYNAPSE_AGENCY,                 /**< Agency attribution */
    SELF_SYNAPSE_METACOGNITIVE,          /**< Metacognitive state */
    SELF_SYNAPSE_REFLECTION,             /**< Self-reflection */
    SELF_SYNAPSE_CONTINUITY              /**< Temporal continuity (PROTECTED) */
} self_awareness_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    SELF_LEARN_RECOGNITION_CONFIRMED = 0, /**< Self was genuinely recognized */
    SELF_LEARN_FALSE_RECOGNITION,         /**< False self-recognition */
    SELF_LEARN_AGENCY_CONFIRMED,          /**< Agency attribution was correct */
    SELF_LEARN_AGENCY_MISMATCH,           /**< Agency mismatch detected */
    SELF_LEARN_BODY_OWNERSHIP_GAIN,       /**< Body ownership strengthened */
    SELF_LEARN_BODY_OWNERSHIP_LOSS,       /**< Body ownership weakened */
    SELF_LEARN_METACOG_ACCURATE,          /**< Metacognitive assessment accurate */
    SELF_LEARN_METACOG_INACCURATE,        /**< Metacognitive assessment wrong */
    SELF_LEARN_REFLECTION_INSIGHT,        /**< Reflection led to insight */
    SELF_LEARN_CONTINUITY_MAINTAINED      /**< Temporal continuity maintained */
} self_awareness_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SELF_PLASTICITY_STATE_IDLE = 0,
    SELF_PLASTICITY_STATE_LEARNING,
    SELF_PLASTICITY_STATE_CONSOLIDATING,
    SELF_PLASTICITY_STATE_UPDATING,
    SELF_PLASTICITY_STATE_ERROR
} self_awareness_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Self-awareness-Plasticity bridge configuration
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
    float target_self_awareness;         /**< Target self-awareness level */

    /* Reward modulation */
    float recognition_learning_boost;    /**< Boost for self-recognition */
    float agency_learning_boost;         /**< Boost for agency confirmation */
    float metacog_modulation;            /**< Metacognitive learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_body_ownership;         /**< Protect body ownership weights */
    bool protect_temporal_continuity;    /**< Protect temporal continuity weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} self_awareness_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Self-awareness synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    self_awareness_synapse_type_t type;  /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} self_awareness_plasticity_synapse_t;

/**
 * @brief Self-awareness learning state
 */
typedef struct {
    float recognition_sensitivity;       /**< Sensitivity to self-recognition */
    float body_ownership_calibration;    /**< Body ownership calibration level */
    float agency_sensitivity;            /**< Sensitivity to agency signals */
    float metacog_strength;              /**< Metacognitive strength */
    float reflection_depth;              /**< Reflection depth */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} self_awareness_learning_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    self_awareness_plasticity_state_t state;  /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} self_awareness_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t recognition_confirmed_events; /**< Recognition confirmed events */
    uint64_t false_recognition_events;   /**< False recognition corrections */
    uint64_t agency_confirmed_events;    /**< Agency confirmed events */
    uint64_t metacog_accurate_events;    /**< Metacognitive accurate learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} self_awareness_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct self_awareness_plasticity_bridge self_awareness_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*self_awareness_plasticity_learn_callback_t)(
    self_awareness_plasticity_bridge_t* bridge,
    self_awareness_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Self-awareness update callback */
typedef void (*self_awareness_plasticity_awareness_callback_t)(
    self_awareness_plasticity_bridge_t* bridge,
    float old_awareness,
    float new_awareness,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
self_awareness_plasticity_config_t self_awareness_plasticity_config_default(void);

/**
 * @brief Create self-awareness plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
self_awareness_plasticity_bridge_t* self_awareness_plasticity_create(
    const self_awareness_plasticity_config_t* config
);

/**
 * @brief Destroy self-awareness plasticity bridge
 * @param bridge Bridge to destroy
 */
void self_awareness_plasticity_destroy(self_awareness_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_reset(self_awareness_plasticity_bridge_t* bridge);

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
int self_awareness_plasticity_register_synapse(
    self_awareness_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    self_awareness_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_unregister_synapse(
    self_awareness_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_get_synapse(
    self_awareness_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    self_awareness_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_protect_synapse(
    self_awareness_plasticity_bridge_t* bridge,
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
int self_awareness_plasticity_learn(
    self_awareness_plasticity_bridge_t* bridge,
    self_awareness_learn_event_t event,
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
float self_awareness_plasticity_apply_stdp(
    self_awareness_plasticity_bridge_t* bridge,
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
int self_awareness_plasticity_apply_reward(
    self_awareness_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_update_bcm(
    self_awareness_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_homeostatic_update(
    self_awareness_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_update_traces(
    self_awareness_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_consolidate(self_awareness_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get learning state
 * @param bridge Bridge handle
 * @param state Output learning state
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_get_learning_state(
    self_awareness_plasticity_bridge_t* bridge,
    self_awareness_learning_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_get_state(
    self_awareness_plasticity_bridge_t* bridge,
    self_awareness_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_get_stats(
    self_awareness_plasticity_bridge_t* bridge,
    self_awareness_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_reset_stats(self_awareness_plasticity_bridge_t* bridge);

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
int self_awareness_plasticity_register_learn_callback(
    self_awareness_plasticity_bridge_t* bridge,
    self_awareness_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register awareness update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_register_awareness_callback(
    self_awareness_plasticity_bridge_t* bridge,
    self_awareness_plasticity_awareness_callback_t callback,
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
int self_awareness_plasticity_bio_async_connect(self_awareness_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_awareness_plasticity_bio_async_disconnect(self_awareness_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool self_awareness_plasticity_is_bio_async_connected(self_awareness_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_AWARENESS_PLASTICITY_BRIDGE_H */
