/**
 * @file nimcp_vta.h
 * @brief Ventral Tegmental Area (VTA) - Dopamine/Reward Center
 * @date 2026-01-11
 *
 * VTA is the primary dopaminergic nucleus, critical for:
 * - Reward processing and motivation
 * - Reward prediction error (RPE) signaling
 * - Incentive salience ("wanting")
 * - Goal-directed behavior
 *
 * Key functions:
 * 1. Dopamine (DA) release in response to reward cues
 * 2. Computing reward prediction errors (RPE = reward - expected)
 * 3. Driving motivation through mesolimbic pathway
 * 4. Supporting learning via DA-dependent plasticity
 */

#ifndef NIMCP_VTA_H
#define NIMCP_VTA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 * Constants
 *===========================================================================*/

#define VTA_MAX_PROJECTIONS        16
#define VTA_MAX_NAME_LEN           64
#define VTA_DEFAULT_TONIC_RATE     4.0f     /* Hz - baseline DA neuron firing */
#define VTA_PHASIC_MAX_RATE        30.0f    /* Hz - burst firing max */
#define VTA_DEFAULT_DA_BASELINE    50.0f    /* nM - baseline DA concentration */
#define VTA_DEFAULT_LEARNING_RATE  0.1f     /* TD learning rate */
#define VTA_DEFAULT_DISCOUNT       0.95f    /* Temporal discount factor gamma */

/*=============================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    VTA_OK = 0,
    VTA_ERROR_NULL,
    VTA_ERROR_NOT_INITIALIZED,
    VTA_ERROR_ALREADY_INITIALIZED,
    VTA_ERROR_INVALID_PARAM,
    VTA_ERROR_CAPACITY,
    VTA_ERROR_NOT_FOUND,
    VTA_ERROR_STATE,
    VTA_ERROR_INTERNAL
} nimcp_vta_error_t;

/*=============================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief VTA operating modes
 */
typedef enum {
    VTA_MODE_TONIC = 0,         /**< Baseline regular firing (~4 Hz) */
    VTA_MODE_PHASIC_EXCITATION, /**< Burst firing (positive RPE) */
    VTA_MODE_PHASIC_PAUSE,      /**< Pause in firing (negative RPE) */
    VTA_MODE_QUIESCENT          /**< Very low activity */
} nimcp_vta_mode_t;

/**
 * @brief Projection targets for DA release
 */
typedef enum {
    VTA_TARGET_NAC = 0,         /**< Nucleus accumbens (motivation) */
    VTA_TARGET_PFC,             /**< Prefrontal cortex (working memory) */
    VTA_TARGET_HIPPOCAMPUS,     /**< Memory consolidation */
    VTA_TARGET_AMYGDALA,        /**< Emotional learning */
    VTA_TARGET_STRIATUM,        /**< Motor/habit learning */
    VTA_TARGET_COUNT
} nimcp_vta_target_t;

/**
 * @brief Dopamine receptor types
 */
typedef enum {
    DA_RECEPTOR_D1 = 0,         /**< D1-like (Gs, excitatory) */
    DA_RECEPTOR_D2,             /**< D2-like (Gi, inhibitory) */
    DA_RECEPTOR_D3,             /**< D3 (limbic) */
    DA_RECEPTOR_D4,             /**< D4 (PFC) */
    DA_RECEPTOR_D5,             /**< D5 (hippocampus) */
    DA_RECEPTOR_COUNT
} nimcp_da_receptor_t;

/**
 * @brief VTA system status
 */
typedef enum {
    VTA_STATUS_NORMAL = 0,
    VTA_STATUS_HYPODOPAMINERGIC,  /**< Low DA - anhedonia */
    VTA_STATUS_HYPERDOPAMINERGIC, /**< High DA - mania */
    VTA_STATUS_DYSREGULATED
} nimcp_vta_status_t;

/*=============================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief VTA neuron pool state
 */
typedef struct {
    float membrane_potential;     /**< mV */
    float firing_rate;           /**< Hz */
    float excitatory_input;      /**< Total excitation */
    float inhibitory_input;      /**< Total inhibition (GABA from SNr) */
    float refractory_time;       /**< ms remaining */
    uint32_t spike_count;        /**< Spikes in current window */
    bool in_burst;               /**< Currently in burst mode */
    uint32_t burst_spikes;       /**< Spikes in current burst */
} nimcp_vta_neuron_pool_t;

/**
 * @brief Projection to target region
 */
typedef struct {
    uint32_t id;
    nimcp_vta_target_t target;
    char name[VTA_MAX_NAME_LEN];
    float weight;                /**< Projection strength [0-1] */
    float da_delivered;          /**< DA at target (nM) */
    float d1_activation;         /**< D1 receptor activation */
    float d2_activation;         /**< D2 receptor activation */
    bool enabled;
} nimcp_vta_projection_t;

/**
 * @brief Reward state tracking
 */
typedef struct {
    float expected_reward;        /**< Predicted reward value */
    float actual_reward;          /**< Received reward */
    float prediction_error;       /**< RPE = actual - expected */
    float cumulative_reward;      /**< Total rewards received */
    float average_reward;         /**< Running average */
    uint32_t reward_count;        /**< Number of rewards */
    float last_reward_time;       /**< When last reward was received */
} nimcp_reward_state_t;

/**
 * @brief Motivation/wanting state
 */
typedef struct {
    float wanting;               /**< Incentive salience [0-1] */
    float liking;                /**< Hedonic impact [0-1] */
    float effort_willingness;    /**< Willing effort [0-1] */
    float goal_proximity;        /**< How close to goal [0-1] */
    float urgency;               /**< Time pressure [0-1] */
} nimcp_motivation_state_t;

/**
 * @brief VTA configuration
 */
typedef struct {
    float baseline_firing_rate;   /**< Hz */
    float baseline_da;            /**< nM */
    float learning_rate;          /**< TD learning alpha */
    float discount_factor;        /**< Temporal discount gamma */
    float rpe_sensitivity;        /**< RPE -> DA gain */
    float da_decay_tau;           /**< DA decay time constant (ms) */
    float burst_threshold;        /**< RPE threshold for burst */
    float pause_threshold;        /**< RPE threshold for pause */
    bool enable_autoreceptors;    /**< D2 autoreceptor feedback */
} nimcp_vta_config_t;

/**
 * @brief VTA metrics
 */
typedef struct {
    uint32_t update_count;
    float total_simulation_time;
    uint32_t total_spikes;
    uint32_t burst_count;
    uint32_t pause_count;
    float total_da_released;
    float positive_rpe_sum;
    float negative_rpe_sum;
    uint32_t reward_events;
} nimcp_vta_metrics_t;

/**
 * @brief Main VTA system structure
 */
typedef struct nimcp_vta_system {
    bool initialized;

    /* Neuron pool */
    nimcp_vta_neuron_pool_t neurons;

    /* Dopamine state */
    float da_concentration;      /**< Current DA level (nM) */
    float da_release_rate;       /**< Release rate (nM/ms) */
    float tonic_firing_rate;     /**< Tonic mode rate (Hz) */

    /* Operating mode */
    nimcp_vta_mode_t mode;
    float mode_duration;         /**< Time in current mode (ms) */
    float phasic_burst_remaining;/**< Remaining burst time (ms) */

    /* Reward processing */
    nimcp_reward_state_t reward;

    /* Motivation */
    nimcp_motivation_state_t motivation;

    /* Projections */
    nimcp_vta_projection_t projections[VTA_MAX_PROJECTIONS];
    uint32_t num_projections;

    /* Configuration */
    nimcp_vta_config_t config;

    /* Metrics */
    nimcp_vta_metrics_t metrics;

    /* Timing */
    float simulation_time;       /**< Total time (ms) */

    /* Status */
    nimcp_vta_status_t status;

} nimcp_vta_system_t;

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Initialize VTA system with default configuration
 */
nimcp_vta_error_t nimcp_vta_init(nimcp_vta_system_t* vta, const nimcp_vta_config_t* config);

/**
 * @brief Shutdown VTA system
 */
nimcp_vta_error_t nimcp_vta_shutdown(nimcp_vta_system_t* vta);

/**
 * @brief Reset VTA to initial state
 */
nimcp_vta_error_t nimcp_vta_reset(nimcp_vta_system_t* vta);

/**
 * @brief Get default configuration
 */
nimcp_vta_config_t nimcp_vta_default_config(void);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update VTA simulation
 * @param vta VTA system
 * @param dt Time step (ms)
 */
nimcp_vta_error_t nimcp_vta_update(nimcp_vta_system_t* vta, float dt);

/*=============================================================================
 * Reward Processing API
 *===========================================================================*/

/**
 * @brief Signal reward receipt
 * @param vta VTA system
 * @param reward_magnitude Reward value (positive = reward, negative = punishment)
 */
nimcp_vta_error_t nimcp_vta_signal_reward(nimcp_vta_system_t* vta, float reward_magnitude);

/**
 * @brief Signal expected reward (prediction)
 * @param vta VTA system
 * @param expected_value Predicted reward value
 */
nimcp_vta_error_t nimcp_vta_set_expectation(nimcp_vta_system_t* vta, float expected_value);

/**
 * @brief Get current reward prediction error
 */
nimcp_vta_error_t nimcp_vta_get_rpe(nimcp_vta_system_t* vta, float* rpe);

/**
 * @brief Compute RPE from reward and expectation
 */
nimcp_vta_error_t nimcp_vta_compute_rpe(
    nimcp_vta_system_t* vta,
    float actual_reward,
    float* rpe
);

/*=============================================================================
 * Motivation API
 *===========================================================================*/

/**
 * @brief Update motivation based on goal value
 */
nimcp_vta_error_t nimcp_vta_modulate_motivation(
    nimcp_vta_system_t* vta,
    float goal_value,
    float* motivation
);

/**
 * @brief Get current wanting signal
 */
nimcp_vta_error_t nimcp_vta_get_wanting(nimcp_vta_system_t* vta, float* wanting);

/**
 * @brief Get current liking signal
 */
nimcp_vta_error_t nimcp_vta_get_liking(nimcp_vta_system_t* vta, float* liking);

/**
 * @brief Compute effort-reward tradeoff
 */
nimcp_vta_error_t nimcp_vta_compute_effort_utility(
    nimcp_vta_system_t* vta,
    float reward_value,
    float effort_required,
    float* net_utility
);

/*=============================================================================
 * DA Control API
 *===========================================================================*/

/**
 * @brief Apply excitation to VTA
 */
nimcp_vta_error_t nimcp_vta_apply_excitation(nimcp_vta_system_t* vta, float strength);

/**
 * @brief Apply inhibition to VTA (from GABAergic inputs)
 */
nimcp_vta_error_t nimcp_vta_apply_inhibition(nimcp_vta_system_t* vta, float strength);

/**
 * @brief Trigger phasic burst
 */
nimcp_vta_error_t nimcp_vta_trigger_burst(
    nimcp_vta_system_t* vta,
    float intensity,
    float duration_ms
);

/**
 * @brief Trigger phasic pause
 */
nimcp_vta_error_t nimcp_vta_trigger_pause(
    nimcp_vta_system_t* vta,
    float depth,
    float duration_ms
);

/**
 * @brief Get current DA concentration
 */
nimcp_vta_error_t nimcp_vta_get_da(nimcp_vta_system_t* vta, float* da);

/**
 * @brief Get current firing rate
 */
nimcp_vta_error_t nimcp_vta_get_firing_rate(nimcp_vta_system_t* vta, float* rate);

/*=============================================================================
 * Mode API
 *===========================================================================*/

/**
 * @brief Get current operating mode
 */
nimcp_vta_error_t nimcp_vta_get_mode(nimcp_vta_system_t* vta, nimcp_vta_mode_t* mode);

/**
 * @brief Force mode transition
 */
nimcp_vta_error_t nimcp_vta_set_mode(nimcp_vta_system_t* vta, nimcp_vta_mode_t mode);

/*=============================================================================
 * Projection API
 *===========================================================================*/

/**
 * @brief Add projection to target region
 */
nimcp_vta_error_t nimcp_vta_add_projection(
    nimcp_vta_system_t* vta,
    nimcp_vta_target_t target,
    const char* name,
    float weight,
    uint32_t* id
);

/**
 * @brief Get projection by ID
 */
nimcp_vta_projection_t* nimcp_vta_get_projection(nimcp_vta_system_t* vta, uint32_t id);

/**
 * @brief Get projection by target type
 */
nimcp_vta_projection_t* nimcp_vta_get_projection_by_target(
    nimcp_vta_system_t* vta,
    nimcp_vta_target_t target
);

/**
 * @brief Get DA level at target
 */
nimcp_vta_error_t nimcp_vta_get_da_at_target(
    nimcp_vta_system_t* vta,
    nimcp_vta_target_t target,
    float* da
);

/**
 * @brief Get D1/D2 activation ratio at target
 */
nimcp_vta_error_t nimcp_vta_get_receptor_balance(
    nimcp_vta_system_t* vta,
    nimcp_vta_target_t target,
    float* d1_d2_ratio
);

/*=============================================================================
 * Status API
 *===========================================================================*/

/**
 * @brief Get VTA state summary
 */
nimcp_vta_error_t nimcp_vta_get_state(
    nimcp_vta_system_t* vta,
    float* da,
    float* rpe,
    float* wanting,
    nimcp_vta_mode_t* mode
);

/**
 * @brief Get VTA status
 */
nimcp_vta_error_t nimcp_vta_get_status(nimcp_vta_system_t* vta, nimcp_vta_status_t* status);

/**
 * @brief Get metrics
 */
nimcp_vta_error_t nimcp_vta_get_metrics(nimcp_vta_system_t* vta, nimcp_vta_metrics_t* metrics);

/**
 * @brief Reset metrics
 */
nimcp_vta_error_t nimcp_vta_reset_metrics(nimcp_vta_system_t* vta);

/*=============================================================================
 * Utility API
 *===========================================================================*/

/**
 * @brief Get error string
 */
const char* nimcp_vta_error_string(nimcp_vta_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_H */
