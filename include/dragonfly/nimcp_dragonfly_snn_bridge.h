/**
 * @file nimcp_dragonfly_snn_bridge.h
 * @brief Dragonfly-to-SNN Backpropagation Bridge
 *
 * WHAT: Bridges dragonfly TSDN neurons to SNN training with surrogate gradients
 * WHY:  Optimize TSDN responses through gradient-based learning
 * HOW:  Use BPTT/E-prop with surrogate gradients for spike-based learning
 *
 * BIOLOGICAL BASIS:
 * - TSDN neurons are modeled as leaky integrate-and-fire spiking neurons
 * - Surrogate gradients approximate the non-differentiable spike function
 * - E-prop provides biologically-plausible online learning with eligibility traces
 * - Reward-modulated STDP for reinforcement learning from interception success
 *
 * TRAINING SCENARIOS:
 * 1. Supervised: Learn TSDN responses from labeled trajectories
 * 2. Reinforcement: Optimize interception success rate
 * 3. Self-supervised: Predict future target positions
 * 4. Hybrid: Combine CNN pretraining with SNN fine-tuning
 *
 * MATHEMATICAL FOUNDATION:
 * - TSDN neurons: dV/dt = -V/tau + I_syn, spike when V > V_thresh
 * - Surrogate gradient: dS/dV ≈ σ'(V) (smooth approximation)
 * - Eligibility trace: e(t) = exp(-t/tau_e) * σ'(V) * x_pre
 * - Weight update: dW = η * R * e(t) (reward-modulated)
 *
 * @author NIMCP Development Team
 * @date 2025-12-28
 */

#ifndef NIMCP_DRAGONFLY_SNN_BRIDGE_H
#define NIMCP_DRAGONFLY_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct dragonfly_system_s;
typedef struct dragonfly_system_s dragonfly_system_t;

//=============================================================================
// Constants
//=============================================================================

#define DRAGONFLY_SNN_MAX_TIMESTEPS 1000    /**< Max BPTT unroll steps */
#define DRAGONFLY_SNN_DEFAULT_BETA 1.0f     /**< Default surrogate gradient beta */
#define DRAGONFLY_SNN_DEFAULT_TAU 20.0f     /**< Default membrane time constant (ms) */
#define DRAGONFLY_SNN_DEFAULT_TAU_ELIG 50.0f /**< Default eligibility trace decay */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Surrogate gradient methods
 *
 * Match snn_surrogate_method_t from nimcp_snn_backprop.h
 */
typedef enum {
    DRAGONFLY_SURROGATE_SUPERSPIKE = 0, /**< σ'(x) = 1/(β|x|+1)² */
    DRAGONFLY_SURROGATE_FAST_SIGMOID,   /**< σ'(x) = x/(1+|x|)² */
    DRAGONFLY_SURROGATE_TRIANGULAR,     /**< σ'(x) = max(0,1-|x|/a) */
    DRAGONFLY_SURROGATE_EXPONENTIAL,    /**< σ'(x) = β·exp(-β|x|) */
    DRAGONFLY_SURROGATE_COUNT
} dragonfly_surrogate_t;

/**
 * @brief SNN training algorithm
 */
typedef enum {
    DRAGONFLY_TRAIN_BPTT = 0,           /**< Backprop through time */
    DRAGONFLY_TRAIN_TRUNCATED_BPTT,     /**< Truncated BPTT */
    DRAGONFLY_TRAIN_EPROP,              /**< E-prop with eligibility traces */
    DRAGONFLY_TRAIN_REWARD_STDP,        /**< Reward-modulated STDP */
    DRAGONFLY_TRAIN_HYBRID              /**< BPTT + STDP hybrid */
} dragonfly_snn_algorithm_t;

/**
 * @brief Loss function types
 */
typedef enum {
    DRAGONFLY_LOSS_SPIKE_COUNT = 0,     /**< Match spike counts */
    DRAGONFLY_LOSS_SPIKE_TIMING,        /**< Match spike times */
    DRAGONFLY_LOSS_RATE_MSE,            /**< MSE on firing rates */
    DRAGONFLY_LOSS_DIRECTION_ERROR,     /**< Error in encoded direction */
    DRAGONFLY_LOSS_INTERCEPTION,        /**< Interception success reward */
    DRAGONFLY_LOSS_COMBINED             /**< Weighted combination */
} dragonfly_snn_loss_t;

/**
 * @brief Neuron model type
 */
typedef enum {
    DRAGONFLY_NEURON_LIF = 0,           /**< Leaky integrate-and-fire */
    DRAGONFLY_NEURON_ALIF,              /**< Adaptive LIF */
    DRAGONFLY_NEURON_IZHIKEVICH,        /**< Izhikevich model */
    DRAGONFLY_NEURON_ADEX               /**< Adaptive exponential */
} dragonfly_neuron_model_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Surrogate gradient configuration
 */
typedef struct {
    dragonfly_surrogate_t method;   /**< Surrogate method */
    float beta;                     /**< Steepness parameter */
    float width;                    /**< Width for triangular */
    bool adaptive_beta;             /**< Adapt beta during training */
} dragonfly_surrogate_config_t;

/**
 * @brief Eligibility trace configuration
 */
typedef struct {
    float tau_eligibility;          /**< Eligibility decay time (ms) */
    float tau_reward;               /**< Reward trace decay (ms) */
    bool use_local_traces;          /**< Per-synapse eligibility */
    bool normalize_traces;          /**< Normalize trace magnitudes */
} dragonfly_eligibility_config_t;

/**
 * @brief TSDN neuron parameters
 */
typedef struct {
    dragonfly_neuron_model_t model; /**< Neuron model type */
    float tau_membrane;             /**< Membrane time constant (ms) */
    float v_threshold;              /**< Spike threshold */
    float v_reset;                  /**< Reset potential */
    float v_rest;                   /**< Resting potential */
    float refractory_ms;            /**< Refractory period (ms) */
    float leak_conductance;         /**< Leak conductance */
} dragonfly_tsdn_params_t;

/**
 * @brief Training configuration
 */
typedef struct {
    dragonfly_snn_algorithm_t algorithm; /**< Training algorithm */
    dragonfly_snn_loss_t loss_type;      /**< Loss function */
    dragonfly_surrogate_config_t surrogate; /**< Surrogate config */
    dragonfly_eligibility_config_t eligibility; /**< Eligibility config */
    dragonfly_tsdn_params_t neuron_params; /**< Neuron parameters */

    /* BPTT parameters */
    uint32_t unroll_steps;          /**< BPTT unroll length */
    bool truncate_gradients;        /**< Truncate long gradients */

    /* Learning rate */
    float learning_rate;            /**< Base learning rate */
    float lr_decay;                 /**< Learning rate decay */
    float min_learning_rate;        /**< Minimum learning rate */

    /* Regularization */
    float weight_decay;             /**< L2 weight regularization */
    float activity_regularization;  /**< Spike rate regularization */
    float gradient_clip;            /**< Gradient clipping threshold */

    /* Reward shaping */
    float reward_baseline;          /**< Baseline reward for variance reduction */
    float reward_discount;          /**< Temporal discount factor */
    bool use_advantage;             /**< Use advantage estimation */

    /* Loss weights for combined loss */
    float loss_weight_spike;        /**< Weight for spike loss */
    float loss_weight_direction;    /**< Weight for direction loss */
    float loss_weight_reward;       /**< Weight for reward loss */
} dragonfly_snn_config_t;

/**
 * @brief Gradient information
 */
typedef struct {
    float* weight_gradients;        /**< Gradients for TSDN weights */
    float* bias_gradients;          /**< Gradients for biases */
    uint32_t num_weights;           /**< Number of weight gradients */
    uint32_t num_biases;            /**< Number of bias gradients */
    float gradient_norm;            /**< L2 norm of gradients */
    bool clipped;                   /**< Whether gradients were clipped */
} dragonfly_snn_gradients_t;

/**
 * @brief Training statistics
 */
typedef struct {
    uint64_t training_steps;
    uint64_t spikes_total;
    float avg_spike_rate;
    float current_loss;
    float avg_loss;
    float min_loss;
    float direction_error_deg;
    float interception_rate;
    float avg_reward;
    float gradient_norm_avg;
    float learning_rate_current;
    uint32_t gradient_clips;
    float training_time_sec;
} dragonfly_snn_stats_t;

/**
 * @brief SNN bridge handle
 */
typedef struct dragonfly_snn_bridge_s dragonfly_snn_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Initialize default SNN bridge configuration
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_bridge_default_config(dragonfly_snn_config_t* config);

/**
 * @brief Validate SNN bridge configuration
 * @param config Configuration to validate
 * @return 0 if valid, -1 if invalid
 */
int dragonfly_snn_bridge_validate_config(const dragonfly_snn_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create SNN training bridge
 * @param dragonfly Dragonfly system (may be NULL)
 * @param snn_trainer SNN trainer handle (may be NULL)
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
dragonfly_snn_bridge_t* dragonfly_snn_bridge_create(
    dragonfly_system_t* dragonfly,
    void* snn_trainer,
    const dragonfly_snn_config_t* config
);

/**
 * @brief Destroy SNN bridge
 * @param bridge Bridge to destroy
 */
void dragonfly_snn_bridge_destroy(dragonfly_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_bridge_reset(dragonfly_snn_bridge_t* bridge);

//=============================================================================
// Forward Pass
//=============================================================================

/**
 * @brief Run forward pass through TSDN network
 * @param bridge SNN bridge
 * @param input Input spike trains or currents
 * @param input_size Size of input
 * @param timesteps Number of simulation steps
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_forward(
    dragonfly_snn_bridge_t* bridge,
    const float* input,
    uint32_t input_size,
    uint32_t timesteps
);

/**
 * @brief Get TSDN spike output
 * @param bridge SNN bridge
 * @param spikes Output spike buffer
 * @param max_spikes Buffer size
 * @return Number of spikes, -1 on error
 */
int dragonfly_snn_get_spikes(
    const dragonfly_snn_bridge_t* bridge,
    float* spikes,
    uint32_t max_spikes
);

/**
 * @brief Get membrane potentials
 * @param bridge SNN bridge
 * @param potentials Output potential buffer
 * @param num_neurons Number of neurons
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_get_potentials(
    const dragonfly_snn_bridge_t* bridge,
    float* potentials,
    uint32_t num_neurons
);

//=============================================================================
// Backward Pass
//=============================================================================

/**
 * @brief Compute loss from target
 * @param bridge SNN bridge
 * @param target Target output (direction, spikes, etc.)
 * @param target_size Size of target
 * @return Loss value, -1.0 on error
 */
float dragonfly_snn_compute_loss(
    dragonfly_snn_bridge_t* bridge,
    const float* target,
    uint32_t target_size
);

/**
 * @brief Run backward pass (compute gradients)
 * @param bridge SNN bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_backward(dragonfly_snn_bridge_t* bridge);

/**
 * @brief Get computed gradients
 * @param bridge SNN bridge
 * @param gradients Output gradient structure
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_get_gradients(
    const dragonfly_snn_bridge_t* bridge,
    dragonfly_snn_gradients_t* gradients
);

/**
 * @brief Apply gradients to update weights
 * @param bridge SNN bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_apply_gradients(dragonfly_snn_bridge_t* bridge);

//=============================================================================
// Training
//=============================================================================

/**
 * @brief Run one complete training step
 * @param bridge SNN bridge
 * @param input Input data
 * @param input_size Input size
 * @param target Target output
 * @param target_size Target size
 * @param timesteps Simulation timesteps
 * @return Loss value, -1.0 on error
 */
float dragonfly_snn_train_step(
    dragonfly_snn_bridge_t* bridge,
    const float* input,
    uint32_t input_size,
    const float* target,
    uint32_t target_size,
    uint32_t timesteps
);

/**
 * @brief Apply reward signal (for RL training)
 * @param bridge SNN bridge
 * @param reward Reward value
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_apply_reward(
    dragonfly_snn_bridge_t* bridge,
    float reward
);

/**
 * @brief Update eligibility traces
 * @param bridge SNN bridge
 * @param dt_ms Time step
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_update_eligibility(
    dragonfly_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Set learning rate
 * @param bridge SNN bridge
 * @param lr New learning rate
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_set_learning_rate(
    dragonfly_snn_bridge_t* bridge,
    float lr
);

/**
 * @brief Decay learning rate
 * @param bridge SNN bridge
 * @return New learning rate, -1.0 on error
 */
float dragonfly_snn_decay_learning_rate(dragonfly_snn_bridge_t* bridge);

//=============================================================================
// TSDN Integration
//=============================================================================

/**
 * @brief Sync weights from dragonfly TSDN
 * @param bridge SNN bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_sync_from_tsdn(dragonfly_snn_bridge_t* bridge);

/**
 * @brief Push trained weights to dragonfly TSDN
 * @param bridge SNN bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_push_to_tsdn(dragonfly_snn_bridge_t* bridge);

/**
 * @brief Get direction error from current TSDN state
 * @param bridge SNN bridge
 * @param target_azimuth Target direction azimuth
 * @param target_elevation Target direction elevation
 * @return Error in degrees, -1.0 on error
 */
float dragonfly_snn_get_direction_error(
    const dragonfly_snn_bridge_t* bridge,
    float target_azimuth,
    float target_elevation
);

//=============================================================================
// Integration
//=============================================================================

/**
 * @brief Connect to dragonfly system
 * @param bridge SNN bridge
 * @param dragonfly Dragonfly system
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_connect_dragonfly(
    dragonfly_snn_bridge_t* bridge,
    dragonfly_system_t* dragonfly
);

/**
 * @brief Connect to SNN trainer
 * @param bridge SNN bridge
 * @param trainer SNN trainer handle
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_connect_trainer(
    dragonfly_snn_bridge_t* bridge,
    void* trainer
);

/**
 * @brief Check if training is active
 * @param bridge SNN bridge
 * @return true if training
 */
bool dragonfly_snn_is_training(const dragonfly_snn_bridge_t* bridge);

/**
 * @brief Set training mode
 * @param bridge SNN bridge
 * @param training Enable/disable training
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_set_training(dragonfly_snn_bridge_t* bridge, bool training);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get training statistics
 * @param bridge SNN bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_bridge_get_stats(
    const dragonfly_snn_bridge_t* bridge,
    dragonfly_snn_stats_t* stats
);

/**
 * @brief Reset training statistics
 * @param bridge SNN bridge
 * @return 0 on success, -1 on error
 */
int dragonfly_snn_bridge_reset_stats(dragonfly_snn_bridge_t* bridge);

//=============================================================================
// Utility
//=============================================================================

/**
 * @brief Get algorithm name
 * @param algorithm Algorithm type
 * @return Algorithm name string
 */
const char* dragonfly_snn_algorithm_name(dragonfly_snn_algorithm_t algorithm);

/**
 * @brief Get surrogate method name
 * @param method Surrogate method
 * @return Method name string
 */
const char* dragonfly_snn_surrogate_name(dragonfly_surrogate_t method);

/**
 * @brief Get loss type name
 * @param loss Loss type
 * @return Loss name string
 */
const char* dragonfly_snn_loss_name(dragonfly_snn_loss_t loss);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_SNN_BRIDGE_H */
