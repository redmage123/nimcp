/**
 * @file nimcp_mirror_plasticity_bridge.h
 * @brief Mirror Neuron - Plasticity Module Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Bidirectional bridge between mirror neurons and centralized Plasticity module
 * WHY:  Enable unified learning rules (STDP, BCM, homeostatic) for mirror neuron plasticity
 * HOW:  Register mirror synapses with plasticity orchestrator, propagate spikes and rewards
 *
 * THEORETICAL FOUNDATIONS:
 * - Keysers & Gazzola (2014): Hebbian learning in mirror circuits
 * - Del Giudice et al. (2009): STDP models of mirror neuron development
 * - Chersi et al. (2011): Learning action chains via mirror neurons
 *
 * BIOLOGICAL BASIS:
 * - Mirror neuron plasticity follows STDP rules (obs-exec timing)
 * - BCM metaplasticity prevents saturation during learning
 * - Homeostatic scaling maintains stable activity levels
 * - Eligibility traces enable reward-modulated learning
 *
 * INTEGRATION FLOWS:
 *
 * Mirror --> Plasticity:
 *   1. Observation spikes trigger presynaptic events
 *   2. Execution spikes trigger postsynaptic events
 *   3. Action recognition triggers reward signal
 *   4. Mirror activation history drives metaplasticity
 *
 * Plasticity --> Mirror:
 *   1. Weight updates affect mirror-action associations
 *   2. LTP/LTD events modulate resonance strength
 *   3. Homeostatic scaling normalizes activations
 *   4. Consolidation events mark stable memories
 *
 * @see nimcp_plasticity_orchestrator.h
 * @see nimcp_plasticity_coordinator.h
 * @see nimcp_mirror_snn_bridge.h
 */

#ifndef NIMCP_MIRROR_PLASTICITY_BRIDGE_H
#define NIMCP_MIRROR_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "plasticity/nimcp_plasticity_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** @brief Maximum mirror synapses managed by bridge */
#define MIRROR_PLASTICITY_MAX_SYNAPSES      4096

/** @brief Maximum action-synapse associations */
#define MIRROR_PLASTICITY_MAX_ACTIONS       128

/** @brief Bio-async module ID */
#define BIO_MODULE_MIRROR_PLASTICITY_BRIDGE 0x0A10

/** @brief Default learning rate for mirror STDP */
#define MIRROR_PLASTICITY_DEFAULT_LR        0.001f

/** @brief Default STDP time window (ms) */
#define MIRROR_PLASTICITY_STDP_WINDOW       50.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Mirror synapse type
 */
typedef enum {
    MIRROR_SYNAPSE_OBS_TO_HIDDEN = 0,    /**< Observation to hidden */
    MIRROR_SYNAPSE_EXEC_TO_HIDDEN,       /**< Execution to hidden */
    MIRROR_SYNAPSE_HIDDEN_TO_OUTPUT,     /**< Hidden to output */
    MIRROR_SYNAPSE_LATERAL                /**< Lateral/recurrent */
} mirror_synapse_type_t;

/**
 * @brief Learning event type
 */
typedef enum {
    MIRROR_LEARN_NONE = 0,
    MIRROR_LEARN_LTP,                    /**< Long-term potentiation */
    MIRROR_LEARN_LTD,                    /**< Long-term depression */
    MIRROR_LEARN_HOMEOSTATIC,            /**< Homeostatic scaling */
    MIRROR_LEARN_CONSOLIDATION           /**< Memory consolidation */
} mirror_learn_event_t;

/**
 * @brief Bridge state
 */
typedef enum {
    MIRROR_PLASTICITY_STATE_IDLE = 0,
    MIRROR_PLASTICITY_STATE_LEARNING,
    MIRROR_PLASTICITY_STATE_CONSOLIDATING,
    MIRROR_PLASTICITY_STATE_SCALING
} mirror_plasticity_state_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Mirror-plasticity bridge configuration
 */
typedef struct {
    /* STDP parameters */
    float stdp_ltp_window_ms;            /**< LTP time window */
    float stdp_ltd_window_ms;            /**< LTD time window */
    float stdp_a_plus;                   /**< LTP amplitude */
    float stdp_a_minus;                  /**< LTD amplitude */
    float stdp_tau_plus;                 /**< LTP time constant */
    float stdp_tau_minus;                /**< LTD time constant */

    /* BCM metaplasticity */
    bool enable_bcm;                     /**< Enable BCM rule */
    float bcm_threshold_tau;             /**< Threshold adaptation time constant */
    float bcm_activity_tau;              /**< Activity averaging time constant */

    /* Homeostatic plasticity */
    bool enable_homeostatic;             /**< Enable homeostatic scaling */
    float target_rate_hz;                /**< Target firing rate */
    float homeostatic_tau_ms;            /**< Scaling time constant */

    /* Eligibility traces */
    bool enable_eligibility;             /**< Enable eligibility traces */
    float eligibility_decay;             /**< Trace decay rate */
    float reward_modulation_gain;        /**< Reward scaling */

    /* Weight bounds */
    float weight_min;                    /**< Minimum weight */
    float weight_max;                    /**< Maximum weight */
    float initial_weight;                /**< Initial weight */

    /* Integration */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    bool enable_immune_integration;      /**< Enable immune modulation */
    bool enable_sleep_integration;       /**< Enable sleep modulation */

    /* Timing */
    float update_interval_ms;            /**< Update frequency */
    float consolidation_interval_ms;     /**< Consolidation check interval */
} mirror_plasticity_config_t;

//=============================================================================
// State Structures
//=============================================================================

/**
 * @brief Per-synapse state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Unique ID */
    uint32_t action_id;                  /**< Associated action */
    mirror_synapse_type_t type;          /**< Synapse type */

    /* Weight */
    float weight;                        /**< Current weight */
    float initial_weight;                /**< Starting weight */

    /* Traces */
    float pre_trace;                     /**< Presynaptic trace */
    float post_trace;                    /**< Postsynaptic trace */
    float eligibility_trace;             /**< Eligibility for reward */

    /* BCM state */
    float bcm_threshold;                 /**< Sliding threshold */
    float avg_activity;                  /**< Average activity */

    /* Timing */
    uint64_t last_pre_spike_us;          /**< Last presynaptic spike */
    uint64_t last_post_spike_us;         /**< Last postsynaptic spike */
    uint64_t last_update_us;             /**< Last update */

    /* Statistics */
    uint32_t ltp_count;                  /**< LTP events */
    uint32_t ltd_count;                  /**< LTD events */
    float total_weight_change;           /**< Cumulative change */
} mirror_plasticity_synapse_t;

/**
 * @brief Bridge state snapshot
 */
typedef struct {
    mirror_plasticity_state_t state;     /**< Current state */

    /* Synapse statistics */
    uint32_t total_synapses;             /**< Total managed synapses */
    uint32_t active_synapses;            /**< Recently active synapses */

    /* Weight distribution */
    float mean_weight;                   /**< Mean synaptic weight */
    float weight_variance;               /**< Weight variance */
    float min_weight;                    /**< Minimum weight */
    float max_weight;                    /**< Maximum weight */

    /* Learning metrics */
    float current_learning_rate;         /**< Effective learning rate */
    float immune_modulation;             /**< Immune modulation factor */
    float sleep_modulation;              /**< Sleep modulation factor */

    /* Activity */
    float mean_activity;                 /**< Mean network activity */
    bool homeostasis_achieved;           /**< Target rate reached */
} mirror_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Event counts */
    uint64_t total_ltp_events;           /**< Total LTP events */
    uint64_t total_ltd_events;           /**< Total LTD events */
    uint64_t total_pre_spikes;           /**< Total presynaptic spikes */
    uint64_t total_post_spikes;          /**< Total postsynaptic spikes */
    uint64_t total_rewards;              /**< Total reward signals */

    /* Magnitude statistics */
    float avg_ltp_magnitude;             /**< Average LTP change */
    float avg_ltd_magnitude;             /**< Average LTD change */
    float avg_reward_magnitude;          /**< Average reward */

    /* Homeostatic */
    uint64_t homeostatic_events;         /**< Scaling events */
    float total_scaling_factor;          /**< Cumulative scaling */

    /* Consolidation */
    uint64_t consolidation_events;       /**< Consolidation events */
    uint32_t consolidated_synapses;      /**< Synapses consolidated */

    /* Energy tracking */
    float total_energy_consumed;         /**< ATP consumed */
    float avg_energy_per_update;         /**< Energy per update */

    /* Bio-async */
    uint64_t bio_messages_sent;          /**< Messages sent */
    uint64_t bio_messages_received;      /**< Messages received */
} mirror_plasticity_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Weight change callback
 */
typedef void (*mirror_plasticity_weight_callback_t)(
    uint32_t synapse_id,
    uint32_t action_id,
    float old_weight,
    float new_weight,
    mirror_learn_event_t event_type,
    void* user_data
);

/**
 * @brief Consolidation callback
 */
typedef void (*mirror_plasticity_consolidation_callback_t)(
    uint32_t action_id,
    uint32_t synapse_count,
    float avg_weight,
    void* user_data
);

/**
 * @brief Homeostatic event callback
 */
typedef void (*mirror_plasticity_homeostatic_callback_t)(
    float current_rate,
    float target_rate,
    float scale_factor,
    void* user_data
);

/**
 * @brief Energy depletion callback
 */
typedef void (*mirror_plasticity_energy_callback_t)(
    float atp_level,
    bool learning_blocked,
    void* user_data
);

//=============================================================================
// Bridge Context
//=============================================================================

/** Forward declaration */
typedef struct mirror_plasticity_bridge mirror_plasticity_bridge_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * @return Default configuration with biological parameters
 */
mirror_plasticity_config_t mirror_plasticity_config_default(void);

/**
 * @brief Create mirror-plasticity bridge
 *
 * WHAT: Initialize bidirectional bridge to plasticity module
 * WHY:  Enable unified learning for mirror neurons
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
mirror_plasticity_bridge_t* mirror_plasticity_create(
    const mirror_plasticity_config_t* config
);

/**
 * @brief Create with existing orchestrator
 *
 * @param config Configuration (NULL for defaults)
 * @param orchestrator Existing plasticity orchestrator
 * @return Bridge handle or NULL on error
 */
mirror_plasticity_bridge_t* mirror_plasticity_create_with_orchestrator(
    const mirror_plasticity_config_t* config,
    plasticity_orchestrator_t* orchestrator
);

/**
 * @brief Destroy bridge
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void mirror_plasticity_destroy(mirror_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Register synapse with plasticity
 *
 * @param bridge Bridge handle
 * @param action_id Associated action
 * @param type Synapse type
 * @param initial_weight Initial weight
 * @return Synapse ID or UINT32_MAX on error
 */
uint32_t mirror_plasticity_register_synapse(
    mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    mirror_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to remove
 * @return 0 on success
 */
int mirror_plasticity_unregister_synapse(
    mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to query
 * @param state Output state
 * @return 0 on success
 */
int mirror_plasticity_get_synapse(
    const mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    mirror_plasticity_synapse_t* state
);

/**
 * @brief Get synapse weight
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to query
 * @return Weight or NAN on error
 */
float mirror_plasticity_get_weight(
    const mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get weights for action
 *
 * @param bridge Bridge handle
 * @param action_id Action to query
 * @param weights Output weight array
 * @param max_weights Array capacity
 * @return Number of weights copied
 */
int mirror_plasticity_get_action_weights(
    const mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float* weights,
    uint32_t max_weights
);

//=============================================================================
// Mirror --> Plasticity Pathway (Spike Events)
//=============================================================================

/**
 * @brief Notify presynaptic spike (observation)
 *
 * WHAT: Record observation spike for STDP
 * WHY:  Trigger LTD if recent postsynaptic spike
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse receiving input
 * @param timestamp_us Spike time
 * @return Weight change applied
 */
float mirror_plasticity_pre_spike(
    mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Notify postsynaptic spike (execution)
 *
 * WHAT: Record execution spike for STDP
 * WHY:  Trigger LTP if recent presynaptic spike
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse that spiked
 * @param timestamp_us Spike time
 * @return Weight change applied
 */
float mirror_plasticity_post_spike(
    mirror_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Notify observation event for action
 *
 * @param bridge Bridge handle
 * @param action_id Action observed
 * @param strength Observation strength
 * @param timestamp_us Event time
 * @return 0 on success
 */
int mirror_plasticity_observation(
    mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float strength,
    uint64_t timestamp_us
);

/**
 * @brief Notify execution event for action
 *
 * @param bridge Bridge handle
 * @param action_id Action executed
 * @param strength Execution strength
 * @param timestamp_us Event time
 * @return 0 on success
 */
int mirror_plasticity_execution(
    mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float strength,
    uint64_t timestamp_us
);

//=============================================================================
// Reward and Learning
//=============================================================================

/**
 * @brief Apply reward signal
 *
 * WHAT: Apply reward to eligibility traces
 * WHY:  Reinforce recent activity based on outcome
 *
 * @param bridge Bridge handle
 * @param reward Reward magnitude [-1, 1]
 * @param timestamp_us Event time
 * @return Number of synapses updated
 */
int mirror_plasticity_reward(
    mirror_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
);

/**
 * @brief Apply reward for specific action
 *
 * @param bridge Bridge handle
 * @param action_id Action to reward
 * @param reward Reward magnitude
 * @return Number of synapses updated
 */
int mirror_plasticity_reward_action(
    mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float reward
);

/**
 * @brief Trigger consolidation
 *
 * WHAT: Force memory consolidation
 * WHY:  Convert short-term to long-term changes
 *
 * @param bridge Bridge handle
 * @return Number of synapses consolidated
 */
int mirror_plasticity_consolidate(mirror_plasticity_bridge_t* bridge);

//=============================================================================
// Plasticity --> Mirror Pathway (Modulation)
//=============================================================================

/**
 * @brief Get modulation for action
 *
 * WHAT: Query current plasticity state affecting action
 * WHY:  Apply learning-dependent modulation to mirror activity
 *
 * @param bridge Bridge handle
 * @param action_id Action to query
 * @param modulation Output modulation factor
 * @return 0 on success
 */
int mirror_plasticity_get_action_modulation(
    const mirror_plasticity_bridge_t* bridge,
    uint32_t action_id,
    float* modulation
);

/**
 * @brief Get global learning rate modulation
 *
 * @param bridge Bridge handle
 * @return Current effective learning rate multiplier
 */
float mirror_plasticity_get_lr_modulation(
    const mirror_plasticity_bridge_t* bridge
);

/**
 * @brief Check if learning is blocked (low energy)
 *
 * @param bridge Bridge handle
 * @return true if learning blocked
 */
bool mirror_plasticity_is_learning_blocked(
    const mirror_plasticity_bridge_t* bridge
);

//=============================================================================
// External System Integration
//=============================================================================

/**
 * @brief Connect to immune system
 *
 * @param bridge Bridge handle
 * @param immune_system Immune system handle
 * @return 0 on success
 */
int mirror_plasticity_connect_immune(
    mirror_plasticity_bridge_t* bridge,
    void* immune_system
);

/**
 * @brief Connect to sleep system
 *
 * @param bridge Bridge handle
 * @param sleep_system Sleep system handle
 * @return 0 on success
 */
int mirror_plasticity_connect_sleep(
    mirror_plasticity_bridge_t* bridge,
    void* sleep_system
);

/**
 * @brief Connect to bio-async
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int mirror_plasticity_connect_bio_async(mirror_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int mirror_plasticity_disconnect_bio_async(mirror_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async status
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool mirror_plasticity_is_bio_async_connected(
    const mirror_plasticity_bridge_t* bridge
);

//=============================================================================
// Callback Registration
//=============================================================================

/**
 * @brief Register weight change callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success
 */
int mirror_plasticity_register_weight_callback(
    mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_weight_callback_t callback,
    void* user_data
);

/**
 * @brief Register consolidation callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success
 */
int mirror_plasticity_register_consolidation_callback(
    mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_consolidation_callback_t callback,
    void* user_data
);

/**
 * @brief Register homeostatic callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success
 */
int mirror_plasticity_register_homeostatic_callback(
    mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_homeostatic_callback_t callback,
    void* user_data
);

/**
 * @brief Register energy callback
 *
 * @param bridge Bridge handle
 * @param callback Callback function
 * @param user_data User context
 * @return 0 on success
 */
int mirror_plasticity_register_energy_callback(
    mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_energy_callback_t callback,
    void* user_data
);

//=============================================================================
// State Query
//=============================================================================

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success
 */
int mirror_plasticity_get_state(
    const mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_bridge_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success
 */
int mirror_plasticity_get_stats(
    const mirror_plasticity_bridge_t* bridge,
    mirror_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge handle
 */
void mirror_plasticity_reset_stats(mirror_plasticity_bridge_t* bridge);

/**
 * @brief Get ATP level
 *
 * @param bridge Bridge handle
 * @return ATP level [0, 1]
 */
float mirror_plasticity_get_atp_level(const mirror_plasticity_bridge_t* bridge);

//=============================================================================
// Update Loop
//=============================================================================

/**
 * @brief Update bridge
 *
 * WHAT: Run plasticity update cycle
 * WHY:  Decay traces, apply homeostasis, check consolidation
 *
 * @param bridge Bridge handle
 * @param dt_ms Time delta
 * @return 0 on success
 */
int mirror_plasticity_update(mirror_plasticity_bridge_t* bridge, float dt_ms);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success
 */
int mirror_plasticity_reset(mirror_plasticity_bridge_t* bridge);

//=============================================================================
// Direct Orchestrator Access
//=============================================================================

/**
 * @brief Get underlying orchestrator
 *
 * @param bridge Bridge handle
 * @return Orchestrator handle (do not destroy)
 */
plasticity_orchestrator_t* mirror_plasticity_get_orchestrator(
    mirror_plasticity_bridge_t* bridge
);

/**
 * @brief Get orchestrator statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success
 */
int mirror_plasticity_get_orchestrator_stats(
    const mirror_plasticity_bridge_t* bridge,
    plasticity_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIRROR_PLASTICITY_BRIDGE_H */
