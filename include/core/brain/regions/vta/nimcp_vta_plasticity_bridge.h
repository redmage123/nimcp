/**
 * @file nimcp_vta_plasticity_bridge.h
 * @brief VTA - Plasticity Bidirectional Integration Bridge
 * @version 1.0.0
 * @date 2026-01-11
 *
 * WHAT: Bidirectional bridge between VTA (dopamine) and plasticity mechanisms
 * WHY:  Enable DA-gated learning, reward-modulated STDP, and TD-learning
 * HOW:  DA modulates eligibility traces, RPE drives weight updates
 *
 * THEORETICAL FOUNDATIONS:
 * - Schultz et al. (1997): Dopamine signal as TD error
 * - Izhikevich (2007): Dopamine-modulated STDP
 * - Reynolds & Wickens (2002): Dopamine and synaptic plasticity
 *
 * BIOLOGICAL BASIS:
 * - DA gates eligibility trace to weight conversion
 * - RPE determines direction and magnitude of weight change
 * - Tonic DA sets learning rate baseline
 * - Phasic DA bursts enable reward-based credit assignment
 *
 * INTEGRATION FLOWS:
 *
 * VTA --> Plasticity:
 *   1. DA level modulates global learning rate
 *   2. RPE signal drives weight update direction/magnitude
 *   3. Motivation affects effort-based learning
 *   4. Goal signals trigger consolidation
 *
 * Plasticity --> VTA:
 *   1. Learning progress affects DA baseline
 *   2. Prediction accuracy modulates RPE computation
 *   3. Skill acquisition signals trigger DA release
 *   4. Network performance feedback to VTA
 *
 * @see nimcp_vta.h
 * @see nimcp_vta_adapter.h
 * @see nimcp_plasticity.h
 */

#ifndef NIMCP_VTA_PLASTICITY_BRIDGE_H
#define NIMCP_VTA_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_vta_adapter;
typedef struct nimcp_vta_adapter* nimcp_vta_adapter_t;
struct nimcp_plasticity_coordinator;

/*=============================================================================
 * Constants
 *===========================================================================*/

/** @brief Maximum tracked synapses */
#define VTA_PLASTICITY_MAX_SYNAPSES     1024

/** @brief Default eligibility trace window (ms) */
#define VTA_PLASTICITY_TRACE_WINDOW     1000.0f

/** @brief Bio-async module ID */
#define BIO_MODULE_VTA_PLASTICITY       0x0D10

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief DA-modulated synapse types
 */
typedef enum {
    VTA_SYNAPSE_NAC = 0,             /**< Nucleus accumbens (reward) */
    VTA_SYNAPSE_PFC,                 /**< Prefrontal cortex (goals) */
    VTA_SYNAPSE_HIPPOCAMPAL,         /**< Hippocampal (context) */
    VTA_SYNAPSE_STRIATAL             /**< Striatal (action selection) */
} nimcp_vta_synapse_type_t;

/**
 * @brief Learning events modulated by DA
 */
typedef enum {
    VTA_LEARN_REWARD = 0,            /**< Reward-based learning */
    VTA_LEARN_PUNISHMENT,            /**< Punishment-based learning */
    VTA_LEARN_TD_UPDATE,             /**< TD error update */
    VTA_LEARN_GOAL_ACHIEVED,         /**< Goal achievement learning */
    VTA_LEARN_HABIT_FORMATION        /**< Habit formation */
} nimcp_vta_learn_event_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    VTA_PLASTICITY_STATE_IDLE = 0,
    VTA_PLASTICITY_STATE_OBSERVING,
    VTA_PLASTICITY_STATE_GATING,
    VTA_PLASTICITY_STATE_UPDATING,
    VTA_PLASTICITY_STATE_CONSOLIDATING,
    VTA_PLASTICITY_STATE_DISABLED
} nimcp_vta_plasticity_state_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief VTA-Plasticity bridge configuration
 */
typedef struct {
    /* DA modulation parameters */
    bool enable_da_gating;           /**< DA gates learning */
    float da_lr_multiplier_min;      /**< Min LR multiplier at low DA */
    float da_lr_multiplier_max;      /**< Max LR multiplier at high DA */
    float da_gating_threshold;       /**< DA threshold for gating */

    /* RPE-based learning */
    bool enable_rpe_learning;        /**< RPE drives weight changes */
    float rpe_learning_rate;         /**< Base RPE learning rate */
    float positive_rpe_boost;        /**< Boost for positive RPE */
    float negative_rpe_scale;        /**< Scale for negative RPE */

    /* STDP parameters */
    float stdp_ltp_window_ms;        /**< LTP time window */
    float stdp_ltd_window_ms;        /**< LTD time window */
    float da_stdp_modulation;        /**< DA modulation of STDP */

    /* Eligibility trace parameters */
    bool enable_eligibility_traces;  /**< Enable 3-factor learning */
    float eligibility_decay_tau;     /**< Trace decay time constant */
    float da_trace_conversion;       /**< DA converts traces to weights */
    float trace_accumulation_rate;   /**< Trace accumulation rate */

    /* TD learning parameters */
    bool enable_td_learning;         /**< Enable TD learning */
    float td_discount_factor;        /**< Discount factor gamma */
    float td_learning_rate;          /**< TD learning rate alpha */

    /* Motivation-based modulation */
    bool enable_motivation_mod;      /**< Motivation affects learning */
    float high_motivation_boost;     /**< LR boost at high motivation */
    float effort_cost_penalty;       /**< Effort cost on learning */

    /* Weight bounds */
    float weight_min;                /**< Minimum weight */
    float weight_max;                /**< Maximum weight */
    float initial_weight;            /**< Initial weight */

    /* Integration */
    bool enable_bio_async;           /**< Enable bio-async messaging */
} nimcp_vta_plasticity_config_t;

/*=============================================================================
 * Synapse Structure
 *===========================================================================*/

/**
 * @brief DA-modulated synapse state
 */
typedef struct {
    uint32_t synapse_id;             /**< Unique identifier */
    nimcp_vta_synapse_type_t type;   /**< Synapse type */

    /* Weight state */
    float weight;                    /**< Current weight */
    float initial_weight;            /**< Initial weight */

    /* Timing state */
    uint64_t last_pre_spike_us;      /**< Last pre-synaptic spike */
    uint64_t last_post_spike_us;     /**< Last post-synaptic spike */

    /* Eligibility trace */
    float eligibility_trace;         /**< Current trace value */
    float trace_peak;                /**< Peak trace value */
    uint64_t trace_start_us;         /**< Trace start time */

    /* TD learning state */
    float value_estimate;            /**< Value function estimate */
    float prev_value_estimate;       /**< Previous estimate */

    /* Habit state */
    float habit_strength;            /**< Habit formation [0, 1] */
    uint32_t reinforcement_count;    /**< Times reinforced */
} nimcp_vta_plasticity_synapse_t;

/*=============================================================================
 * State Structures
 *===========================================================================*/

/**
 * @brief Current DA modulation state
 */
typedef struct {
    float da_level;                  /**< Current DA level */
    float current_rpe;               /**< Current RPE value */
    float motivation;                /**< Current motivation */
    float lr_multiplier;             /**< Current LR multiplier */
    float eligibility_gate;          /**< Eligibility conversion gate */
    bool reward_received;            /**< Recent reward flag */
    uint64_t last_reward_us;         /**< Last reward timestamp */
} nimcp_vta_plasticity_da_state_t;

/**
 * @brief Bridge runtime state
 */
typedef struct {
    nimcp_vta_plasticity_state_t state;
    nimcp_vta_plasticity_da_state_t da;
    uint32_t registered_synapses;
    float global_lr_modulation;
    float avg_value_estimate;
    bool bio_async_connected;
} nimcp_vta_plasticity_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t total_pre_spikes;
    uint64_t total_post_spikes;
    uint64_t ltp_events;
    uint64_t ltd_events;
    uint64_t td_updates;
    uint64_t reward_events;
    float total_reward;
    float avg_rpe;
    float avg_weight_change;
} nimcp_vta_plasticity_stats_t;

/*=============================================================================
 * Modulation Output
 *===========================================================================*/

/**
 * @brief Learning modulation output
 */
typedef struct {
    float lr_multiplier;             /**< Learning rate multiplier */
    float rpe_signal;                /**< Current RPE for updates */
    float eligibility_gate;          /**< Eligibility conversion factor */
    float value_gradient;            /**< TD value gradient */
    float effort_discount;           /**< Effort-based discount */
    bool trigger_update;             /**< Trigger weight update now */
} nimcp_vta_plasticity_modulation_t;

/*=============================================================================
 * Callback Types
 *===========================================================================*/

/**
 * @brief Weight change callback
 */
typedef void (*nimcp_vta_weight_change_cb)(
    uint32_t synapse_id,
    nimcp_vta_synapse_type_t type,
    float old_weight,
    float new_weight,
    nimcp_vta_learn_event_t event_type,
    float rpe,
    void* user_data
);

/*=============================================================================
 * Main Bridge Structure
 *===========================================================================*/

typedef struct nimcp_vta_plasticity_bridge nimcp_vta_plasticity_bridge_t;

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Get default configuration
 */
nimcp_vta_plasticity_config_t nimcp_vta_plasticity_config_default(void);

/**
 * @brief Create VTA-plasticity bridge
 */
nimcp_vta_plasticity_bridge_t* nimcp_vta_plasticity_create(
    const nimcp_vta_plasticity_config_t* config
);

/**
 * @brief Destroy VTA-plasticity bridge
 */
void nimcp_vta_plasticity_destroy(nimcp_vta_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 */
int nimcp_vta_plasticity_reset(nimcp_vta_plasticity_bridge_t* bridge);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect to VTA adapter
 */
int nimcp_vta_plasticity_connect_vta(
    nimcp_vta_plasticity_bridge_t* bridge,
    nimcp_vta_adapter_t vta_adapter
);

/**
 * @brief Connect to plasticity coordinator
 */
int nimcp_vta_plasticity_connect_coordinator(
    nimcp_vta_plasticity_bridge_t* bridge,
    struct nimcp_plasticity_coordinator* coordinator
);

/*=============================================================================
 * Synapse Management
 *===========================================================================*/

/**
 * @brief Register synapse for DA-modulated learning
 */
int nimcp_vta_plasticity_register_synapse(
    nimcp_vta_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_vta_synapse_type_t type,
    float initial_weight
);

/**
 * @brief Unregister synapse
 */
int nimcp_vta_plasticity_unregister_synapse(
    nimcp_vta_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse state
 */
int nimcp_vta_plasticity_get_synapse(
    nimcp_vta_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    nimcp_vta_plasticity_synapse_t* synapse
);

/*=============================================================================
 * Event Recording (VTA --> Plasticity)
 *===========================================================================*/

/**
 * @brief Record pre-synaptic spike
 */
int nimcp_vta_plasticity_pre_spike(
    nimcp_vta_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Record post-synaptic spike
 */
int nimcp_vta_plasticity_post_spike(
    nimcp_vta_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_us
);

/**
 * @brief Record reward signal
 */
int nimcp_vta_plasticity_reward(
    nimcp_vta_plasticity_bridge_t* bridge,
    float reward,
    uint64_t timestamp_us
);

/**
 * @brief Record RPE signal
 */
int nimcp_vta_plasticity_rpe(
    nimcp_vta_plasticity_bridge_t* bridge,
    float rpe,
    uint64_t timestamp_us
);

/**
 * @brief Update DA level
 */
int nimcp_vta_plasticity_set_da_level(
    nimcp_vta_plasticity_bridge_t* bridge,
    float da_level,
    float motivation
);

/*=============================================================================
 * Update Functions
 *===========================================================================*/

/**
 * @brief Update all plasticity mechanisms
 */
int nimcp_vta_plasticity_update(
    nimcp_vta_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Apply TD update
 */
int nimcp_vta_plasticity_td_update(
    nimcp_vta_plasticity_bridge_t* bridge,
    float current_value,
    float next_value,
    float reward
);

/**
 * @brief Convert eligibility traces (on DA signal)
 */
int nimcp_vta_plasticity_convert_traces(
    nimcp_vta_plasticity_bridge_t* bridge,
    float da_signal
);

/*=============================================================================
 * Query Functions (Plasticity --> VTA)
 *===========================================================================*/

/**
 * @brief Get current learning modulation
 */
int nimcp_vta_plasticity_get_modulation(
    nimcp_vta_plasticity_bridge_t* bridge,
    nimcp_vta_plasticity_modulation_t* modulation
);

/**
 * @brief Get learning progress signal
 */
float nimcp_vta_plasticity_get_learning_progress(
    nimcp_vta_plasticity_bridge_t* bridge
);

/**
 * @brief Get prediction accuracy
 */
float nimcp_vta_plasticity_get_prediction_accuracy(
    nimcp_vta_plasticity_bridge_t* bridge
);

/**
 * @brief Get average value estimate
 */
float nimcp_vta_plasticity_get_avg_value(
    nimcp_vta_plasticity_bridge_t* bridge
);

/*=============================================================================
 * State and Statistics
 *===========================================================================*/

/**
 * @brief Get bridge state
 */
int nimcp_vta_plasticity_get_state(
    const nimcp_vta_plasticity_bridge_t* bridge,
    nimcp_vta_plasticity_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 */
int nimcp_vta_plasticity_get_stats(
    const nimcp_vta_plasticity_bridge_t* bridge,
    nimcp_vta_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 */
void nimcp_vta_plasticity_reset_stats(nimcp_vta_plasticity_bridge_t* bridge);

/*=============================================================================
 * Callbacks
 *===========================================================================*/

/**
 * @brief Register weight change callback
 */
int nimcp_vta_plasticity_set_weight_callback(
    nimcp_vta_plasticity_bridge_t* bridge,
    nimcp_vta_weight_change_cb callback,
    void* user_data
);

/*=============================================================================
 * Bio-Async Integration
 *===========================================================================*/

/**
 * @brief Connect to bio-async router
 */
int nimcp_vta_plasticity_connect_bio_async(nimcp_vta_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int nimcp_vta_plasticity_disconnect_bio_async(nimcp_vta_plasticity_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 */
bool nimcp_vta_plasticity_is_bio_async_connected(const nimcp_vta_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_PLASTICITY_BRIDGE_H */
