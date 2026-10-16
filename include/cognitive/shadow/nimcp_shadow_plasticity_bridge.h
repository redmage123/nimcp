/**
 * @file nimcp_shadow_plasticity_bridge.h
 * @brief Shadow Emotions - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Bidirectional bridge between shadow emotions and synaptic plasticity
 * WHY:  Enable learning of defense patterns from experience and therapeutic feedback
 * HOW:  STDP for suppression-outcome associations, BCM for stabilization, reward
 *       modulation for shadow integration learning
 *
 * THEORETICAL FOUNDATIONS:
 * - Jung (1938): Shadow integration and individuation process
 * - Freud (1915): Defense mechanism development through learning
 * - LeDoux (2002): Emotional learning and memory consolidation
 * - Pennebaker (1997): Therapeutic expression and emotional processing
 *
 * BIOLOGICAL BASIS:
 * - Amygdala plasticity for fear/threat learning
 * - Prefrontal-amygdala connectivity for emotion regulation
 * - Hippocampal-cortical transfer for emotional memory
 * - VTA/NA for reward-based defense learning
 *
 * PLASTICITY TYPES:
 * - STDP: Temporal association of suppression-outcome pairs
 * - BCM: Stabilize core defense patterns
 * - Homeostatic: Maintain balanced emotional processing
 * - Reward-modulated: Learn from integration success
 *
 * @see nimcp_shadow_emotions.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_SHADOW_PLASTICITY_BRIDGE_H
#define NIMCP_SHADOW_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum shadow synapses */
#define SHADOW_PLASTICITY_MAX_SYNAPSES   256

/** @brief Default learning rate */
#define SHADOW_PLASTICITY_DEFAULT_LR     0.01f

/** @brief Bio-async module ID */
#define BIO_MODULE_SHADOW_PLASTICITY     0x0D61

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Shadow synapse types
 *
 * THEORY: Different pathways for processing suppressed emotional content
 */
typedef enum {
    SHADOW_SYNAPSE_SUPPRESSION = 0,      /**< Suppression pathway */
    SHADOW_SYNAPSE_DEFENSE,               /**< Defense mechanism (PROTECTED) */
    SHADOW_SYNAPSE_REPRESSION,            /**< Repression pathway */
    SHADOW_SYNAPSE_INTEGRATION,           /**< Integration pathway */
    SHADOW_SYNAPSE_BREAKTHROUGH,          /**< Breakthrough pathway */
    SHADOW_SYNAPSE_INSIGHT                /**< Insight pathway (PROTECTED) */
} shadow_synapse_type_t;

/**
 * @brief Learning event types for shadow processing
 *
 * THEORY: Events that drive plasticity in shadow processing circuits
 */
typedef enum {
    SHADOW_LEARN_SUPPRESSION_SUCCESS = 0, /**< Successful suppression */
    SHADOW_LEARN_SUPPRESSION_FAILURE,     /**< Failed suppression (breakthrough) */
    SHADOW_LEARN_DEFENSE_ACTIVATED,       /**< Defense mechanism triggered */
    SHADOW_LEARN_DEFENSE_BYPASSED,        /**< Defense mechanism bypassed */
    SHADOW_LEARN_INTEGRATION_PROGRESS,    /**< Progress in integration */
    SHADOW_LEARN_INTEGRATION_SETBACK,     /**< Setback in integration */
    SHADOW_LEARN_INSIGHT_GAINED,          /**< Therapeutic insight */
    SHADOW_LEARN_REPRESSION_LIFTED,       /**< Repression released */
    SHADOW_LEARN_BREAKTHROUGH_EVENT,      /**< Shadow material emerged */
    SHADOW_LEARN_ACCEPTANCE               /**< Shadow acceptance/integration */
} shadow_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SHADOW_PLASTICITY_STATE_IDLE = 0,
    SHADOW_PLASTICITY_STATE_LEARNING,
    SHADOW_PLASTICITY_STATE_CONSOLIDATING,
    SHADOW_PLASTICITY_STATE_UPDATING,
    SHADOW_PLASTICITY_STATE_ERROR
} shadow_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Shadow-Plasticity bridge configuration
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
    float target_integration;            /**< Target integration level */

    /* Reward modulation */
    float insight_learning_boost;        /**< Boost for insight events */
    float integration_learning_boost;    /**< Boost for integration success */
    float defense_modulation;            /**< Defense learning strength */

    /* Weight bounds */
    float weight_min;                    /**< Minimum synapse weight */
    float weight_max;                    /**< Maximum synapse weight */

    /* Core pattern protection */
    bool protect_defense_pathways;       /**< Protect defense pathway weights */
    bool protect_insight_pathways;       /**< Protect insight pathway weights */
    float protection_strength;           /**< How strongly to protect core patterns */

    /* Capacity */
    uint32_t max_synapses;               /**< Maximum synapses to track */

    /* Bio-async integration */
    bool enable_bio_async;               /**< Enable bio-async callbacks */
} shadow_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Shadow synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique synapse ID */
    shadow_synapse_type_t type;          /**< Synapse type */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Initial weight */
    float eligibility_trace;             /**< Eligibility trace */
    float bcm_threshold;                 /**< BCM sliding threshold */
    float avg_activity;                  /**< Average pre-synaptic activity */
    uint64_t last_update_us;             /**< Last update timestamp */
    uint32_t update_count;               /**< Number of updates */
    bool is_protected;                   /**< Protected from modification */
} shadow_plasticity_synapse_t;

/**
 * @brief Shadow integration learning state
 *
 * Tracks the overall state of shadow processing plasticity
 */
typedef struct {
    float suppression_sensitivity;       /**< Sensitivity to suppression */
    float defense_calibration;           /**< Defense mechanism calibration */
    float integration_sensitivity;       /**< Sensitivity to integration progress */
    float insight_strength;              /**< Insight pathway strength */
    float breakthrough_threshold;        /**< Threshold for breakthrough events */
    float learning_rate_mod;             /**< Learning rate modifier */
    uint64_t last_learning_us;           /**< Last learning event */
} shadow_integration_state_t;

/**
 * @brief Bridge state information
 */
typedef struct {
    shadow_plasticity_state_t state;     /**< Current operational state */
    uint32_t active_synapses;            /**< Number of active synapses */
    float mean_weight;                   /**< Mean synapse weight */
    float weight_variance;               /**< Weight variance */
    float learning_rate_effective;       /**< Current effective learning rate */
} shadow_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_learning_events;      /**< Total learning events */
    uint64_t suppression_events;         /**< Suppression events */
    uint64_t breakthrough_events;        /**< Breakthrough events */
    uint64_t insight_events;             /**< Insight events */
    uint64_t integration_progress_events;/**< Integration progress events */
    uint64_t weight_updates;             /**< Total weight updates */
    uint64_t protected_updates_blocked;  /**< Updates blocked by protection */
    float mean_weight_change;            /**< Mean weight change magnitude */
    float total_potentiation;            /**< Total potentiation */
    float total_depression;              /**< Total depression */
} shadow_plasticity_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

typedef struct shadow_plasticity_bridge shadow_plasticity_bridge_t;

//=============================================================================
// Callbacks
//=============================================================================

/** @brief Learning event callback */
typedef void (*shadow_plasticity_learn_callback_t)(
    shadow_plasticity_bridge_t* bridge,
    shadow_learn_event_t event,
    float magnitude,
    void* user_data
);

/** @brief Integration update callback */
typedef void (*shadow_plasticity_integration_callback_t)(
    shadow_plasticity_bridge_t* bridge,
    float old_integration,
    float new_integration,
    void* user_data
);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create default configuration
 * @return Default configuration structure
 */
shadow_plasticity_config_t shadow_plasticity_config_default(void);

/**
 * @brief Create shadow plasticity bridge
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
shadow_plasticity_bridge_t* shadow_plasticity_create(
    const shadow_plasticity_config_t* config
);

/**
 * @brief Destroy shadow plasticity bridge
 * @param bridge Bridge to destroy
 */
void shadow_plasticity_destroy(shadow_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_reset(shadow_plasticity_bridge_t* bridge);

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
int shadow_plasticity_register_synapse(
    shadow_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    shadow_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister a synapse
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unregister
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_unregister_synapse(
    shadow_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param synapse Output synapse state
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_get_synapse(
    shadow_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    shadow_plasticity_synapse_t* synapse
);

/**
 * @brief Protect synapse from modification
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param protect true to protect, false to unprotect
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_protect_synapse(
    shadow_plasticity_bridge_t* bridge,
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
int shadow_plasticity_learn(
    shadow_plasticity_bridge_t* bridge,
    shadow_learn_event_t event,
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
float shadow_plasticity_apply_stdp(
    shadow_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float pre_time,
    float post_time
);

/**
 * @brief Apply reward modulation (therapeutic feedback)
 * @param bridge Bridge handle
 * @param reward Reward signal [-1, 1]
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_apply_reward(
    shadow_plasticity_bridge_t* bridge,
    float reward
);

/**
 * @brief Update BCM thresholds
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_update_bcm(
    shadow_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply homeostatic scaling
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_homeostatic_update(
    shadow_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Update eligibility traces
 * @param bridge Bridge handle
 * @param dt_ms Time step (ms)
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_update_traces(
    shadow_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Consolidate learning
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_consolidate(shadow_plasticity_bridge_t* bridge);

//=============================================================================
// State Query Functions
//=============================================================================

/**
 * @brief Get integration state
 * @param bridge Bridge handle
 * @param state Output integration state
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_get_integration_state(
    shadow_plasticity_bridge_t* bridge,
    shadow_integration_state_t* state
);

/**
 * @brief Get bridge state
 * @param bridge Bridge handle
 * @param state Output state structure
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_get_state(
    shadow_plasticity_bridge_t* bridge,
    shadow_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_get_stats(
    shadow_plasticity_bridge_t* bridge,
    shadow_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_reset_stats(shadow_plasticity_bridge_t* bridge);

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
int shadow_plasticity_register_learn_callback(
    shadow_plasticity_bridge_t* bridge,
    shadow_plasticity_learn_callback_t callback,
    void* user_data
);

/**
 * @brief Register integration update callback
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_register_integration_callback(
    shadow_plasticity_bridge_t* bridge,
    shadow_plasticity_integration_callback_t callback,
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
int shadow_plasticity_bio_async_connect(shadow_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 * @param bridge Bridge handle
 * @return 0 on success, -1 on failure
 */
int shadow_plasticity_bio_async_disconnect(shadow_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 * @param bridge Bridge handle
 * @return true if connected, false otherwise
 */
bool shadow_plasticity_is_bio_async_connected(shadow_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SHADOW_PLASTICITY_BRIDGE_H */
