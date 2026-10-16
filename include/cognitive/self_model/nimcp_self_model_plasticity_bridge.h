/**
 * @file nimcp_self_model_plasticity_bridge.h
 * @brief Self Model - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between self model engine and synaptic plasticity
 * WHY:  Enable learning of self-representation patterns from experience and feedback
 * HOW:  STDP for self-awareness associations, BCM for stabilization, reward
 *       modulation for agency and identity learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Gallagher (2000): Sense of agency development through learning
 * - Tsakiris (2010): Body ownership plasticity
 * - Conway (2005): Self-memory system development
 *
 * BIOLOGICAL BASIS:
 * - mPFC plasticity shapes self-referential processing
 * - TPJ plasticity enhances self-other distinction
 * - Insular plasticity modulates interoceptive self-awareness
 * - Default mode network plasticity supports autobiographical self
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of self-experience pairs
 * - BCM: Stabilize core identity patterns
 * - Homeostatic: Maintain balanced self-representation
 * - Reward-modulated: Learn from agency prediction accuracy
 *
 * PROTECTED SYNAPSES:
 * - IDENTITY synapses: Core identity cannot be easily modified
 * - BOUNDARY synapses: Self-other distinction must remain stable
 *
 * @see nimcp_self_model.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_SELF_MODEL_PLASTICITY_BRIDGE_H
#define NIMCP_SELF_MODEL_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum self model synapses */
#define SELF_MODEL_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define SELF_MODEL_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_SELF_MODEL_PLASTICITY     0x0D51

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Self model synapse types
 */
typedef enum {
    SELF_SYNAPSE_IDENTITY = 0,      /**< Core identity representation (PROTECTED) */
    SELF_SYNAPSE_AGENCY,            /**< Agency/control estimation */
    SELF_SYNAPSE_BOUNDARY,          /**< Self-other boundary (PROTECTED) */
    SELF_SYNAPSE_NARRATIVE,         /**< Self-narrative coherence */
    SELF_SYNAPSE_CAPABILITY,        /**< Self-capability assessment */
    SELF_SYNAPSE_CONTINUITY         /**< Temporal self-continuity */
} self_model_synapse_type_t;

/**
 * @brief Learning event types
 */
typedef enum {
    SELF_LEARN_AGENCY_CONFIRMED = 0,     /**< Agency prediction confirmed */
    SELF_LEARN_AGENCY_VIOLATED,          /**< Agency prediction violated */
    SELF_LEARN_OWNERSHIP_CONFIRMED,      /**< Body ownership confirmed */
    SELF_LEARN_OWNERSHIP_VIOLATED,       /**< Body ownership violated */
    SELF_LEARN_BOUNDARY_CLARIFIED,       /**< Self-other boundary clarified */
    SELF_LEARN_BOUNDARY_VIOLATED,        /**< Self-other boundary violated */
    SELF_LEARN_IDENTITY_REINFORCED,      /**< Core identity reinforced */
    SELF_LEARN_NARRATIVE_UPDATED,        /**< Self-narrative updated */
    SELF_LEARN_CAPABILITY_UPDATED,       /**< Capability assessment updated */
    SELF_LEARN_CONTINUITY_MAINTAINED     /**< Temporal continuity maintained */
} self_model_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SELF_MODEL_PLASTICITY_STATE_IDLE = 0,
    SELF_MODEL_PLASTICITY_STATE_LEARNING,
    SELF_MODEL_PLASTICITY_STATE_CONSOLIDATING,
    SELF_MODEL_PLASTICITY_STATE_UPDATING,
    SELF_MODEL_PLASTICITY_STATE_ERROR
} self_model_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Self Model-Plasticity bridge configuration
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
    float target_agency;                 /**< Target agency level */

    /* Reward modulation */
    float agency_learning_boost;         /**< Boost for agency learning */
    float boundary_learning_boost;       /**< Boost for boundary learning */
    float identity_modulation;           /**< Identity learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_identity;               /**< Protect core identity patterns */
    bool protect_boundary;               /**< Protect self-other boundary */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} self_model_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Self model synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    self_model_synapse_type_t type;      /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} self_model_plasticity_synapse_t;

/**
 * @brief Self model calibration state
 */
typedef struct {
    float agency_sensitivity;            /**< Sensitivity to agency signals */
    float boundary_calibration;          /**< Boundary calibration level */
    float identity_stability;            /**< Identity stability level */
    float narrative_coherence;           /**< Narrative coherence strength */
    float capability_accuracy;           /**< Capability assessment accuracy */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} self_model_calibration_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    self_model_plasticity_state_t state; /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} self_model_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t agency_confirmed_events;    /**< Agency confirmed events */
    uint64_t agency_violated_events;     /**< Agency violated corrections */
    uint64_t boundary_clarified_events;  /**< Boundary clarification events */
    uint64_t identity_reinforced_events; /**< Identity reinforcement events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} self_model_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct self_model_plasticity_bridge self_model_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*self_model_plasticity_learn_callback_t)(
    self_model_plasticity_bridge_t* bridge,
    self_model_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Calibration update callback */
typedef void (*self_model_plasticity_calibration_callback_t)(
    self_model_plasticity_bridge_t* bridge,
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
self_model_plasticity_config_t self_model_plasticity_config_default(void);

/**
 * @brief Create self model plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
self_model_plasticity_bridge_t* self_model_plasticity_create(
    const self_model_plasticity_config_t* config
);

/**
 * @brief Destroy self model plasticity bridge
 * @param bridge Bridge to destroy
 */
void self_model_plasticity_destroy(self_model_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_reset(self_model_plasticity_bridge_t* bridge);

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
int self_model_plasticity_register_synapse(
    self_model_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    self_model_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_unregister_synapse(
    self_model_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_get_synapse(
    self_model_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    self_model_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_protect_synapse(
    self_model_plasticity_bridge_t* bridge,
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
int self_model_plasticity_learn(
    self_model_plasticity_bridge_t* bridge,
    self_model_learn_event_t event,
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
float self_model_plasticity_apply_stdp(
    self_model_plasticity_bridge_t* bridge,
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
int self_model_plasticity_apply_reward(
    self_model_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_update_bcm(
    self_model_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_homeostatic_update(
    self_model_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_update_traces(
    self_model_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_consolidate(self_model_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get calibration state
 * @param bridge Bridge handle
 * @param state Output calibration state
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_get_calibration_state(
    self_model_plasticity_bridge_t* bridge,
    self_model_calibration_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_get_state(
    self_model_plasticity_bridge_t* bridge,
    self_model_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_get_stats(
    self_model_plasticity_bridge_t* bridge,
    self_model_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_reset_stats(self_model_plasticity_bridge_t* bridge);

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
int self_model_plasticity_register_learn_callback(
    self_model_plasticity_bridge_t* bridge,
    self_model_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register calibration update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_register_calibration_callback(
    self_model_plasticity_bridge_t* bridge,
    self_model_plasticity_calibration_callback_t callback,
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
int self_model_plasticity_bio_async_connect(self_model_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int self_model_plasticity_bio_async_disconnect(self_model_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool self_model_plasticity_is_bio_async_connected(self_model_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SELF_MODEL_PLASTICITY_BRIDGE_H */
