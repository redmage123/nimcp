/**
 * @file nimcp_snn_training.h
 * @brief SNN Training Module - Spike-Based Learning Algorithms
 *
 * WHAT: Training algorithms for spiking neural networks
 * WHY:  Enable learning in SNNs using biologically-plausible rules
 * HOW:  STDP, R-STDP, surrogate gradients, and eProp
 *
 * INTEGRATION:
 * - Uses existing synapse_t with STDP support
 * - Integrates with plasticity coordinator
 * - Bio-async enabled for distributed training
 *
 * BIOLOGICAL BASIS:
 * - STDP: Hebb's rule at spike precision (Bi & Poo 1998)
 * - R-STDP: Dopamine-modulated plasticity (Izhikevich 2007)
 * - Homeostatic: Maintain activity levels (Turrigiano 2012)
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 *
 * @author NIMCP Team
 * @date 2024
 */

#ifndef NIMCP_SNN_TRAINING_H
#define NIMCP_SNN_TRAINING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"

//=============================================================================
// Training Configuration Types
//=============================================================================

/**
 * @brief STDP (Spike-Timing Dependent Plasticity) configuration
 *
 * WHAT: Parameters for STDP learning rule
 * WHY:  Control strength and timing of plasticity
 * HOW:  Exponential windows for LTP and LTD
 */
typedef struct snn_stdp_config_s {
    float a_plus;           /**< LTP amplitude (positive) */
    float a_minus;          /**< LTD amplitude (negative) */
    float tau_plus;         /**< LTP time constant (ms) */
    float tau_minus;        /**< LTD time constant (ms) */
    float w_min;            /**< Minimum weight */
    float w_max;            /**< Maximum weight */
    bool soft_bounds;       /**< Use soft (multiplicative) bounds */
    bool symmetric;         /**< Use symmetric STDP window */
} snn_stdp_config_t;

/**
 * @brief R-STDP (Reward-modulated STDP) configuration
 *
 * WHAT: Parameters for reward-modulated learning
 * WHY:  Enable reinforcement learning in SNNs
 * HOW:  STDP modulated by reward/dopamine signal
 */
typedef struct snn_rstdp_config_s {
    snn_stdp_config_t stdp;     /**< Base STDP parameters */
    float eligibility_tau;       /**< Eligibility trace decay (ms) */
    float reward_tau;            /**< Reward signal decay (ms) */
    float baseline_reward;       /**< Baseline reward for stability */
    bool use_td_error;           /**< Use TD error instead of raw reward */
} snn_rstdp_config_t;

/**
 * @brief Surrogate gradient configuration
 *
 * WHAT: Parameters for gradient-based SNN training
 * WHY:  Enable backpropagation through spike functions
 * HOW:  Smooth surrogate for spike derivative
 */
typedef struct snn_surrogate_config_s {
    snn_surrogate_t type;       /**< Surrogate gradient type */
    float beta;                  /**< Steepness parameter */
    float threshold;             /**< Spike threshold */
    float learning_rate;         /**< Base learning rate */
    float momentum;              /**< Momentum coefficient */
    float weight_decay;          /**< L2 regularization */
} snn_surrogate_config_t;

/**
 * @brief eProp (Eligibility Propagation) configuration
 *
 * WHAT: Parameters for eProp learning
 * WHY:  Online learning with local eligibility traces
 * HOW:  Combines BPTT with eligibility traces
 */
typedef struct snn_eprop_config_s {
    float learning_rate;         /**< Learning rate */
    float eligibility_tau;       /**< Eligibility trace decay (ms) */
    float kappa;                 /**< Dampening factor */
    bool use_adam;               /**< Use Adam optimizer */
    float adam_beta1;            /**< Adam beta1 */
    float adam_beta2;            /**< Adam beta2 */
} snn_eprop_config_t;

/**
 * @brief Homeostatic plasticity configuration
 *
 * WHAT: Parameters for activity homeostasis
 * WHY:  Maintain stable activity levels
 * HOW:  Adjust thresholds or weights to target rate
 */
typedef struct snn_homeostatic_config_s {
    float target_rate;           /**< Target firing rate (Hz) */
    float rate_tau;              /**< Rate estimation time constant (ms) */
    float adaptation_rate;       /**< How fast to adapt */
    bool adjust_threshold;       /**< Adjust thresholds (intrinsic) */
    bool adjust_weights;         /**< Adjust weights (synaptic) */
} snn_homeostatic_config_t;

/*
 * NOTE: snn_training_ctx_t is defined in nimcp_snn_types.h with these fields:
 *   - mode: Training mode (STDP, R-STDP, eProp, etc.)
 *   - surrogate: Surrogate gradient function type
 *   - surrogate_beta: Surrogate gradient sharpness
 *   - eligibility: Eligibility traces tensor
 *   - eligibility_decay: Trace decay rate
 *   - grad_membrane: Gradients w.r.t. membrane potential
 *   - grad_weights: Gradients w.r.t. weights
 *   - reward: Current reward signal
 *   - reward_baseline: Baseline for variance reduction
 *   - current_loss: Current training loss
 *   - smoothed_loss: EMA of loss
 */

//=============================================================================
// Training Context Lifecycle
//=============================================================================

/**
 * @brief Create STDP training context
 *
 * WHAT: Initialize context for STDP learning
 * WHY:  Prepare spike-timing based plasticity
 * HOW:  Allocate and configure STDP parameters
 *
 * @param config STDP configuration
 * @return Training context or NULL on failure
 */
snn_training_ctx_t* snn_training_create_stdp(const snn_stdp_config_t* config);

/**
 * @brief Create R-STDP training context
 *
 * WHAT: Initialize context for reward-modulated learning
 * WHY:  Enable reinforcement learning
 * HOW:  Allocate eligibility traces and reward state
 *
 * @param config R-STDP configuration
 * @param n_pre Number of presynaptic neurons
 * @param n_post Number of postsynaptic neurons
 * @return Training context or NULL on failure
 */
snn_training_ctx_t* snn_training_create_rstdp(const snn_rstdp_config_t* config,
                                               uint32_t n_pre,
                                               uint32_t n_post);

/**
 * @brief Create surrogate gradient training context
 *
 * WHAT: Initialize context for gradient-based training
 * WHY:  Enable backpropagation through spikes
 * HOW:  Allocate optimizer state
 *
 * @param config Surrogate gradient configuration
 * @param n_pre Number of presynaptic neurons
 * @param n_post Number of postsynaptic neurons
 * @return Training context or NULL on failure
 */
snn_training_ctx_t* snn_training_create_surrogate(const snn_surrogate_config_t* config,
                                                   uint32_t n_pre,
                                                   uint32_t n_post);

/**
 * @brief Create eProp training context
 *
 * WHAT: Initialize context for eProp learning
 * WHY:  Online learning with eligibility traces
 * HOW:  Allocate eligibility and optimizer state
 *
 * @param config eProp configuration
 * @param n_pre Number of presynaptic neurons
 * @param n_post Number of postsynaptic neurons
 * @return Training context or NULL on failure
 */
snn_training_ctx_t* snn_training_create_eprop(const snn_eprop_config_t* config,
                                               uint32_t n_pre,
                                               uint32_t n_post);

/**
 * @brief Destroy training context
 *
 * WHAT: Free training context resources
 * WHY:  Proper cleanup
 * HOW:  Free all allocated memory
 *
 * @param ctx Training context to destroy
 */
void snn_training_destroy(snn_training_ctx_t* ctx);

//=============================================================================
// Default Configuration Functions
//=============================================================================

/**
 * @brief Initialize STDP config with defaults
 *
 * WHAT: Set biologically-plausible STDP parameters
 * WHY:  Convenient initialization
 * HOW:  Values from Bi & Poo 1998
 *
 * @param config Config to initialize
 */
void snn_stdp_config_default(snn_stdp_config_t* config);

/**
 * @brief Initialize R-STDP config with defaults
 *
 * WHAT: Set default R-STDP parameters
 * WHY:  Convenient initialization
 * HOW:  Reasonable eligibility and reward traces
 *
 * @param config Config to initialize
 */
void snn_rstdp_config_default(snn_rstdp_config_t* config);

/**
 * @brief Initialize surrogate gradient config with defaults
 *
 * WHAT: Set default surrogate gradient parameters
 * WHY:  Convenient initialization
 * HOW:  Fast sigmoid with reasonable beta
 *
 * @param config Config to initialize
 */
void snn_surrogate_config_default(snn_surrogate_config_t* config);

/**
 * @brief Initialize eProp config with defaults
 *
 * WHAT: Set default eProp parameters
 * WHY:  Convenient initialization
 * HOW:  Values from Bellec et al. 2020
 *
 * @param config Config to initialize
 */
void snn_eprop_config_default(snn_eprop_config_t* config);

/**
 * @brief Initialize homeostatic config with defaults
 *
 * WHAT: Set default homeostatic parameters
 * WHY:  Convenient initialization
 * HOW:  Target cortical firing rates
 *
 * @param config Config to initialize
 */
void snn_homeostatic_config_default(snn_homeostatic_config_t* config);

//=============================================================================
// STDP Training Functions
//=============================================================================

/**
 * @brief Compute STDP weight change
 *
 * WHAT: Calculate weight change from spike timing
 * WHY:  Core STDP learning rule
 * HOW:  Exponential windows for LTP/LTD
 *
 * @param ctx Training context
 * @param dt_pre_post Time difference (t_post - t_pre) in ms
 * @param current_weight Current synapse weight
 * @return Weight change (delta_w)
 */
float snn_stdp_compute_delta_w(const snn_training_ctx_t* ctx,
                                float dt_pre_post,
                                float current_weight);

/**
 * @brief Apply STDP to synapse
 *
 * WHAT: Update synapse weight with STDP
 * WHY:  Execute learning from spike pair
 * HOW:  Compute delta_w and apply with bounds
 *
 * @param ctx Training context
 * @param synapse Synapse to update
 * @param t_pre Presynaptic spike time (ms)
 * @param t_post Postsynaptic spike time (ms)
 * @return New weight after update
 */
float snn_stdp_update(snn_training_ctx_t* ctx,
                      synapse_t* synapse,
                      float t_pre,
                      float t_post);

/**
 * @brief Apply STDP to all synapses in network
 *
 * WHAT: Network-wide STDP update
 * WHY:  Batch learning from all spike pairs
 * HOW:  Iterate all synapses, apply STDP rule
 *
 * @param ctx Training context
 * @param network SNN network
 * @param t_current Current simulation time (ms)
 * @return Number of synapses updated
 */
uint32_t snn_stdp_apply_network(snn_training_ctx_t* ctx,
                                 snn_network_t* network,
                                 float t_current);

//=============================================================================
// R-STDP Training Functions
//=============================================================================

/**
 * @brief Update eligibility traces
 *
 * WHAT: Decay and update eligibility traces
 * WHY:  Track potential synaptic changes
 * HOW:  Exponential decay with STDP-based updates
 *
 * @param ctx Training context
 * @param dt Time step (ms)
 */
void snn_rstdp_update_eligibility(snn_training_ctx_t* ctx, float dt);

/**
 * @brief Set reward signal
 *
 * WHAT: Provide reward/punishment signal
 * WHY:  Modulate eligibility traces into weight changes
 * HOW:  Store and filter reward signal
 *
 * @param ctx Training context
 * @param reward Reward value (positive = good, negative = bad)
 */
void snn_rstdp_set_reward(snn_training_ctx_t* ctx, float reward);

/*============================================================================
 * Runtime-tunable SNN parameters (Phase 4 real-time ops). Setters are
 * guarded with sane bounds; values outside the valid range are ignored.
 * Readers return the current value. No brain restart needed to apply.
 *==========================================================================*/
void  snn_tune_set_rstdp_lr(float v);
void  snn_tune_set_rstdp_baseline_alpha(float v);
void  snn_tune_set_target_rate(float v);
void  snn_tune_set_homeo_bounds(float min_scale, float max_scale);
void  snn_tune_set_max_scale_dead(float v);
void  snn_tune_set_dead_threshold(float v);
void  snn_tune_set_metabolic_cap(float v);
void  snn_tune_set_noise_rate_hz(float v);
void  snn_tune_set_noise_pulse_mv(float v);
void  snn_tune_set_intrinsic_alpha(float v);
void  snn_tune_set_anti_reward_enabled(float v);
void  snn_tune_set_anti_reward_threshold_ratio(float v);
void  snn_tune_set_anti_reward_gain(float v);
void  snn_tune_set_target_rate_input(float v);
void  snn_tune_set_depression_inc(float v);
void  snn_tune_set_depression_tau_ms(float v);
void  snn_tune_set_depression_cap(float v);
/* Biophysical stability mechanisms (wired into SNN step hot path). */
void  snn_tune_set_ahp_enabled(float v);
void  snn_tune_set_ahp_tau_ms(float v);
void  snn_tune_set_ahp_gain_mv(float v);
void  snn_tune_set_pump_enabled(float v);
void  snn_tune_set_pump_tau_ms(float v);
void  snn_tune_set_pump_gain_mv(float v);
void  snn_tune_set_basket_enabled(float v);
void  snn_tune_set_basket_fraction(float v);
void  snn_tune_set_noise_ei_ratio(float v);
/* Substrate adapter tunables (Phase 1 biological-substrate wiring). */
void  snn_tune_set_substrate_enabled(float v);
void  snn_tune_set_substrate_update_period(float v);
void  snn_tune_set_substrate_spike_dropout_on(float v);
void  snn_tune_set_substrate_plasticity_mod_on(float v);
/* F8: AHP + Na/K pump <-> substrate ATP coupling. When 1, the SNN hot
 * path scales AHP/pump hyperpolarization by substrate pump_activity
 * (clamped [0.1, 1.0]). When 0, the scaling factor is a hardcoded 1.0
 * (identity, bit-exact pre-F8 behavior). */
void  snn_tune_set_ahp_pump_substrate_coupling(float v);

float snn_tune_get_rstdp_lr(void);
float snn_tune_get_rstdp_baseline_alpha(void);
float snn_tune_get_target_rate(void);
float snn_tune_get_homeo_min_scale(void);
float snn_tune_get_homeo_max_scale(void);
float snn_tune_get_max_scale_dead(void);
float snn_tune_get_dead_threshold(void);
float snn_tune_get_metabolic_cap(void);
float snn_tune_get_noise_rate_hz(void);
float snn_tune_get_noise_pulse_mv(void);
float snn_tune_get_intrinsic_alpha(void);
float snn_tune_get_anti_reward_enabled(void);
float snn_tune_get_anti_reward_threshold_ratio(void);
float snn_tune_get_anti_reward_gain(void);
float snn_tune_get_target_rate_input(void);
float snn_tune_get_depression_inc(void);
float snn_tune_get_depression_tau_ms(void);
float snn_tune_get_depression_cap(void);
float snn_tune_get_ahp_enabled(void);
float snn_tune_get_ahp_tau_ms(void);
float snn_tune_get_ahp_gain_mv(void);
float snn_tune_get_pump_enabled(void);
float snn_tune_get_pump_tau_ms(void);
float snn_tune_get_pump_gain_mv(void);
float snn_tune_get_basket_enabled(void);
float snn_tune_get_basket_fraction(void);
float snn_tune_get_noise_ei_ratio(void);
float snn_tune_get_substrate_enabled(void);
float snn_tune_get_substrate_update_period(void);
float snn_tune_get_substrate_spike_dropout_on(void);
float snn_tune_get_substrate_plasticity_mod_on(void);
float snn_tune_get_ahp_pump_substrate_coupling(void);

/*============================================================================
 * Intrinsic SNN reward — firing-rate-target-based. Use as the primary
 * R-STDP teaching signal to decouple SNN learning from ANN-loss signal.
 *==========================================================================*/
float snn_compute_intrinsic_reward(snn_network_t* network);

/*============================================================================
 * Adaptive noise factor — scales Poisson noise probability per population
 * based on firing_rate_ema vs target. 1.0 when dead, linearly down to 0.0
 * when at target. Called from the SNN noise-injection loop.
 *==========================================================================*/
float snn_noise_factor_for_pop(const snn_population_t* pop);

/**
 * @brief Apply R-STDP weight updates
 *
 * WHAT: Convert eligibility traces to weight changes
 * WHY:  Reward-modulated plasticity
 * HOW:  delta_w = eligibility * reward_trace
 *
 * @param ctx Training context
 * @param network SNN network
 * @return Number of synapses updated
 */
uint32_t snn_rstdp_apply(snn_training_ctx_t* ctx, snn_network_t* network);

//=============================================================================
// Surrogate Gradient Functions
//=============================================================================

/**
 * @brief Compute surrogate gradient of spike function
 *
 * WHAT: Smooth derivative of spike threshold
 * WHY:  Enable backpropagation through spikes
 * HOW:  Various surrogate functions (sigmoid, arctan, etc.)
 *
 * @param ctx Training context
 * @param membrane_v Membrane potential
 * @return Surrogate gradient value
 *
 * NOTE: Renamed from snn_surrogate_gradient() to snn_training_surrogate_gradient()
 * to resolve ODR conflict with snn_backprop's snn_surrogate_gradient().
 * Use snn_training_surrogate_gradient() for snn_training_ctx_t contexts,
 * or the backprop header's snn_surrogate_gradient() for snn_backprop_ctx_t contexts.
 */
float snn_training_surrogate_gradient(const snn_training_ctx_t* ctx, float membrane_v);

/**
 * @brief Backpropagate error through SNN layer
 *
 * WHAT: Compute gradients for one layer
 * WHY:  Enable gradient descent training
 * HOW:  Chain rule with surrogate gradients
 *
 * @param ctx Training context
 * @param output_grad Gradient from output layer [n_outputs]
 * @param membrane_v Membrane potentials [n_neurons]
 * @param n_neurons Number of neurons
 * @param input_grad Output gradient for previous layer [n_inputs]
 * @return SNN_SUCCESS or error code
 */
int snn_surrogate_backward(snn_training_ctx_t* ctx,
                           const float* output_grad,
                           const float* membrane_v,
                           uint32_t n_neurons,
                           float* input_grad);

/**
 * @brief Apply gradient updates with optimizer
 *
 * WHAT: Update weights with computed gradients
 * WHY:  Execute gradient descent step
 * HOW:  Adam optimizer or SGD
 *
 * @param ctx Training context
 * @param weights Weight matrix [n_pre][n_post]
 * @param gradients Gradient matrix [n_pre][n_post]
 * @return SNN_SUCCESS or error code
 */
int snn_surrogate_apply_gradients(snn_training_ctx_t* ctx,
                                   float** weights,
                                   float** gradients);

//=============================================================================
// eProp Functions
//=============================================================================

/**
 * @brief Update eProp eligibility traces
 *
 * WHAT: Online eligibility trace updates
 * WHY:  Track gradients over time
 * HOW:  Combine eligibility with learning signals
 *
 * @param ctx Training context
 * @param pre_spikes Presynaptic spike mask [n_pre]
 * @param post_spikes Postsynaptic spike mask [n_post]
 * @param dt Time step (ms)
 */
void snn_eprop_update_eligibility(snn_training_ctx_t* ctx,
                                   const uint8_t* pre_spikes,
                                   const uint8_t* post_spikes,
                                   float dt);

/**
 * @brief Apply eProp weight updates
 *
 * WHAT: Update weights from eligibility and learning signal
 * WHY:  Online gradient-based learning
 * HOW:  Modulate eligibility by error signal
 *
 * @param ctx Training context
 * @param network SNN network
 * @param learning_signal Error or reward signal
 * @return Number of synapses updated
 */
uint32_t snn_eprop_apply(snn_training_ctx_t* ctx,
                          snn_network_t* network,
                          float learning_signal);

//=============================================================================
// Homeostatic Functions
//=============================================================================

/**
 * @brief Update rate estimates
 *
 * WHAT: Track firing rates per neuron
 * WHY:  Basis for homeostatic regulation
 * HOW:  Exponential moving average
 *
 * @param ctx Training context
 * @param spikes Spike mask [n_neurons]
 * @param n_neurons Number of neurons
 * @param dt Time step (ms)
 */
void snn_homeostatic_update_rates(snn_training_ctx_t* ctx,
                                   const uint8_t* spikes,
                                   uint32_t n_neurons,
                                   float dt);

/**
 * @brief Apply homeostatic regulation
 *
 * WHAT: Adjust parameters to maintain target rate
 * WHY:  Prevent runaway excitation or silence
 * HOW:  Modify thresholds or weights
 *
 * @param ctx Training context
 * @param network SNN network
 * @return Number of adjustments made
 */
uint32_t snn_homeostatic_apply(snn_training_ctx_t* ctx, snn_network_t* network);

//=============================================================================
// Training Statistics
//=============================================================================

/**
 * @brief Get training statistics
 *
 * WHAT: Retrieve training metrics
 * WHY:  Monitor learning progress
 * HOW:  Return counters and totals
 *
 * @param ctx Training context
 * @param weight_updates Output for weight update count
 * @param training_steps Output for step count
 * @param total_delta_w Output for total weight change
 */
void snn_training_get_stats(const snn_training_ctx_t* ctx,
                            uint64_t* weight_updates,
                            uint64_t* training_steps,
                            float* total_delta_w);

/**
 * @brief Reset training statistics
 *
 * WHAT: Clear training counters
 * WHY:  Fresh start for new training phase
 * HOW:  Zero all statistics
 *
 * @param ctx Training context
 */
void snn_training_reset_stats(snn_training_ctx_t* ctx);

/**
 * @brief Reset training context state
 *
 * WHAT: Reset eligibility traces and optimizer state
 * WHY:  Start fresh training episode
 * HOW:  Zero all state variables
 *
 * @param ctx Training context
 */
void snn_training_reset(snn_training_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_TRAINING_H */
