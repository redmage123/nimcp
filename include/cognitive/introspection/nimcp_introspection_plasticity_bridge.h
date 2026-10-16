/**
 * @file nimcp_introspection_plasticity_bridge.h
 * @brief Introspection - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between introspection engine and synaptic plasticity
 * WHY:  Enable learning of metacognitive patterns from experience and feedback
 * HOW:  STDP for self-awareness associations, BCM for stabilization, reward
 *       modulation for calibration learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Koriat (1997): Monitoring and control in metacognition
 * - Nelson & Narens (1990): Metamemory framework
 * - Flavell (1979): Metacognition and cognitive monitoring
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex plasticity shapes metacognitive abilities
 * - Dopaminergic signals modulate confidence calibration
 * - ACC plasticity enhances error detection
 * - Repeated introspection strengthens self-monitoring circuits
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of insight-outcome pairs
 * - BCM: Stabilize core metacognitive patterns
 * - Homeostatic: Maintain balanced confidence calibration
 * - Reward-modulated: Learn from prediction accuracy
 *
 * @see nimcp_introspection.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_INTROSPECTION_PLASTICITY_BRIDGE_H
#define NIMCP_INTROSPECTION_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum introspection synapses */
#define INTROSPECTION_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define INTROSPECTION_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_INTROSPECTION_PLASTICITY     0x0D41

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Introspection synapse types
 */
typedef enum {
    INTROSPECTION_SYNAPSE_UNCERTAINTY = 0, /**< Uncertainty estimation */
    INTROSPECTION_SYNAPSE_CONFIDENCE,       /**< Confidence calibration */
    INTROSPECTION_SYNAPSE_PATTERN,          /**< Pattern recognition */
    INTROSPECTION_SYNAPSE_ERROR,            /**< Error detection */
    INTROSPECTION_SYNAPSE_METACOGNITION,    /**< Metacognitive monitoring */
    INTROSPECTION_SYNAPSE_STATE_TRACK,      /**< State tracking */
    INTROSPECTION_SYNAPSE_CALIBRATION       /**< Calibration learning */
} introspection_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    INTROSPECTION_LEARN_CORRECT_CONFIDENCE = 0, /**< Confidence was accurate */
    INTROSPECTION_LEARN_OVERCONFIDENCE,          /**< Was overconfident */
    INTROSPECTION_LEARN_UNDERCONFIDENCE,         /**< Was underconfident */
    INTROSPECTION_LEARN_ERROR_DETECTED,          /**< Error correctly detected */
    INTROSPECTION_LEARN_ERROR_MISSED,            /**< Missed an error */
    INTROSPECTION_LEARN_PATTERN_MATCH,           /**< Pattern correctly recognized */
    INTROSPECTION_LEARN_STATE_TRACKED,           /**< State correctly tracked */
    INTROSPECTION_LEARN_UNCERTAINTY_CALIBRATED   /**< Uncertainty calibration improved */
} introspection_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    INTROSPECTION_PLASTICITY_STATE_IDLE = 0,
    INTROSPECTION_PLASTICITY_STATE_LEARNING,
    INTROSPECTION_PLASTICITY_STATE_CONSOLIDATING,
    INTROSPECTION_PLASTICITY_STATE_UPDATING,
    INTROSPECTION_PLASTICITY_STATE_ERROR
} introspection_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Introspection-Plasticity bridge configuration
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
    float target_confidence;             /**< Target confidence calibration */

    /* Reward modulation */
    float accuracy_learning_boost;       /**< Boost for accurate predictions */
    float error_learning_boost;          /**< Boost for error learning */
    float calibration_modulation;        /**< Calibration learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_core_patterns;          /**< Protect core metacognitive patterns */
    bool protect_error_detection;        /**< Protect error detection weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} introspection_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Introspection synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    introspection_synapse_type_t type;   /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} introspection_plasticity_synapse_t;

/**
 * @brief Metacognitive calibration state
 */
typedef struct {
    float uncertainty_sensitivity;       /**< Sensitivity to uncertainty */
    float confidence_calibration;        /**< Confidence calibration level */
    float error_sensitivity;             /**< Sensitivity to errors */
    float pattern_strength;              /**< Pattern recognition strength */
    float metacognition_strength;        /**< Metacognitive monitoring strength */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} introspection_calibration_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    introspection_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} introspection_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t correct_confidence_events;  /**< Correct confidence events */
    uint64_t overconfidence_events;      /**< Overconfidence corrections */
    uint64_t underconfidence_events;     /**< Underconfidence corrections */
    uint64_t error_detection_events;     /**< Error detection learning */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} introspection_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct introspection_plasticity_bridge introspection_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*introspection_plasticity_learn_callback_t)(
    introspection_plasticity_bridge_t* bridge,
    introspection_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Calibration update callback */
typedef void (*introspection_plasticity_calibration_callback_t)(
    introspection_plasticity_bridge_t* bridge,
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
introspection_plasticity_config_t introspection_plasticity_config_default(void);

/**
 * @brief Create introspection plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
introspection_plasticity_bridge_t* introspection_plasticity_create(
    const introspection_plasticity_config_t* config
);

/**
 * @brief Destroy introspection plasticity bridge
 * @param bridge Bridge to destroy
 */
void introspection_plasticity_destroy(introspection_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_reset(introspection_plasticity_bridge_t* bridge);

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
int introspection_plasticity_register_synapse(
    introspection_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    introspection_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_unregister_synapse(
    introspection_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_get_synapse(
    introspection_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    introspection_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_protect_synapse(
    introspection_plasticity_bridge_t* bridge,
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
int introspection_plasticity_learn(
    introspection_plasticity_bridge_t* bridge,
    introspection_learn_event_t event,
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
float introspection_plasticity_apply_stdp(
    introspection_plasticity_bridge_t* bridge,
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
int introspection_plasticity_apply_reward(
    introspection_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_update_bcm(
    introspection_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_homeostatic_update(
    introspection_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_update_traces(
    introspection_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_consolidate(introspection_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get calibration state
 * @param bridge Bridge handle
 * @param state Output calibration state
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_get_calibration_state(
    introspection_plasticity_bridge_t* bridge,
    introspection_calibration_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_get_state(
    introspection_plasticity_bridge_t* bridge,
    introspection_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_get_stats(
    introspection_plasticity_bridge_t* bridge,
    introspection_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_reset_stats(introspection_plasticity_bridge_t* bridge);

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
int introspection_plasticity_register_learn_callback(
    introspection_plasticity_bridge_t* bridge,
    introspection_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register calibration update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_register_calibration_callback(
    introspection_plasticity_bridge_t* bridge,
    introspection_plasticity_calibration_callback_t callback,
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
int introspection_plasticity_bio_async_connect(introspection_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int introspection_plasticity_bio_async_disconnect(introspection_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool introspection_plasticity_is_bio_async_connected(introspection_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_INTROSPECTION_PLASTICITY_BRIDGE_H */
