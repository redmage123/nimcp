/**
 * @file nimcp_reasoning_plasticity_bridge.h
 * @brief Reasoning - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between reasoning engine and synaptic plasticity
 * WHY:  Enable learning of reasoning patterns from experience and feedback
 * HOW:  STDP for inference associations, BCM for stabilization, reward
 *       modulation for reasoning accuracy learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Anderson (1993): ACT-R procedural learning
 * - Tenenbaum (2011): Bayesian reasoning development
 * - Holyoak (2012): Relational reasoning learning
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex plasticity shapes reasoning abilities
 * - Dopaminergic signals modulate inference accuracy
 * - ACC plasticity enhances conflict detection
 * - Repeated reasoning strengthens logical circuits
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of premise-conclusion pairs
 * - BCM: Stabilize core reasoning patterns
 * - Homeostatic: Maintain balanced inference calibration
 * - Reward-modulated: Learn from reasoning accuracy
 *
 * @see nimcp_reasoning_integration.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_REASONING_PLASTICITY_BRIDGE_H
#define NIMCP_REASONING_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum reasoning synapses */
#define REASONING_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define REASONING_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_REASONING_PLASTICITY     0x0D51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Reasoning synapse types
 */
typedef enum {
    REASON_SYNAPSE_DEDUCTION = 0,   /**< Deductive reasoning */
    REASON_SYNAPSE_INDUCTION,        /**< Inductive reasoning */
    REASON_SYNAPSE_ABDUCTION,        /**< Abductive inference */
    REASON_SYNAPSE_CAUSAL,           /**< Causal reasoning */
    REASON_SYNAPSE_ANALOGY,          /**< Analogical reasoning */
    REASON_SYNAPSE_EVIDENCE          /**< Evidence evaluation */
} reasoning_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    REASON_LEARN_VALID_CONCLUSION = 0,   /**< Valid conclusion reached */
    REASON_LEARN_INVALID_CONCLUSION,     /**< Invalid conclusion corrected */
    REASON_LEARN_CAUSAL_CONFIRMED,       /**< Causal relationship confirmed */
    REASON_LEARN_CAUSAL_REFUTED,         /**< Causal relationship refuted */
    REASON_LEARN_ANALOGY_MATCHED,        /**< Analogy correctly matched */
    REASON_LEARN_ANALOGY_FAILED,         /**< Analogy incorrectly applied */
    REASON_LEARN_EVIDENCE_INTEGRATED,    /**< Evidence properly integrated */
    REASON_LEARN_CONFLICT_RESOLVED       /**< Conflict successfully resolved */
} reasoning_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    REASONING_PLASTICITY_STATE_IDLE = 0,
    REASONING_PLASTICITY_STATE_LEARNING,
    REASONING_PLASTICITY_STATE_CONSOLIDATING,
    REASONING_PLASTICITY_STATE_UPDATING,
    REASONING_PLASTICITY_STATE_ERROR
} reasoning_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Reasoning-Plasticity bridge configuration
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
    float target_inference_accuracy;     /**< Target inference accuracy */

    /* Reward modulation */
    float accuracy_learning_boost;       /**< Boost for accurate inferences */
    float error_learning_boost;          /**< Boost for error learning */
    float causal_modulation;             /**< Causal learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_deduction;              /**< Protect deductive reasoning weights */
    bool protect_causal;                 /**< Protect causal reasoning weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} reasoning_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Reasoning synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    reasoning_synapse_type_t type;       /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} reasoning_plasticity_synapse_t;

/**
 * @brief Reasoning calibration state
 */
typedef struct {
    float deduction_strength;            /**< Deduction capability strength */
    float induction_accuracy;            /**< Induction accuracy level */
    float causal_sensitivity;            /**< Sensitivity to causal patterns */
    float analogy_matching;              /**< Analogy matching strength */
    float evidence_weighting;            /**< Evidence weighting calibration */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} reasoning_calibration_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    reasoning_plasticity_state_t state;  /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} reasoning_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t valid_conclusion_events;    /**< Valid conclusion events */
    uint64_t invalid_conclusion_events;  /**< Invalid conclusion corrections */
    uint64_t causal_learning_events;     /**< Causal learning events */
    uint64_t analogy_learning_events;    /**< Analogy learning events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} reasoning_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct reasoning_plasticity_bridge reasoning_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*reasoning_plasticity_learn_callback_t)(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Calibration update callback */
typedef void (*reasoning_plasticity_calibration_callback_t)(
    reasoning_plasticity_bridge_t* bridge,
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
reasoning_plasticity_config_t reasoning_plasticity_config_default(void);

/**
 * @brief Create reasoning plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
reasoning_plasticity_bridge_t* reasoning_plasticity_create(
    const reasoning_plasticity_config_t* config
);

/**
 * @brief Destroy reasoning plasticity bridge
 * @param bridge Bridge to destroy
 */
void reasoning_plasticity_destroy(reasoning_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_reset(reasoning_plasticity_bridge_t* bridge);

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
int reasoning_plasticity_register_synapse(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    reasoning_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_unregister_synapse(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_get_synapse(
    reasoning_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    reasoning_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_protect_synapse(
    reasoning_plasticity_bridge_t* bridge,
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
int reasoning_plasticity_learn(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_learn_event_t event,
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
float reasoning_plasticity_apply_stdp(
    reasoning_plasticity_bridge_t* bridge,
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
int reasoning_plasticity_apply_reward(
    reasoning_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_update_bcm(
    reasoning_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_homeostatic_update(
    reasoning_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_update_traces(
    reasoning_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_consolidate(reasoning_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get calibration state
 * @param bridge Bridge handle
 * @param state Output calibration state
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_get_calibration_state(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_calibration_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_get_state(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_get_stats(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_reset_stats(reasoning_plasticity_bridge_t* bridge);

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
int reasoning_plasticity_register_learn_callback(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register calibration update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_register_calibration_callback(
    reasoning_plasticity_bridge_t* bridge,
    reasoning_plasticity_calibration_callback_t callback,
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
int reasoning_plasticity_bio_async_connect(reasoning_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int reasoning_plasticity_bio_async_disconnect(reasoning_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool reasoning_plasticity_is_bio_async_connected(reasoning_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REASONING_PLASTICITY_BRIDGE_H */
