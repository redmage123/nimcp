/**
 * @file nimcp_plasticity_gpu.h
 * @brief GPU-accelerated Plasticity Kernels
 *
 * WHAT: CUDA kernels for synaptic plasticity computations
 * WHY:  GPU acceleration for performance-critical plasticity rules
 * HOW:  Custom kernels for STDP, BCM, Homeostatic, STP, Calcium dynamics
 *
 * ARCHITECTURE:
 * - STDP: Spike-Timing-Dependent Plasticity (pair and triplet)
 * - BCM: Bienenstock-Cooper-Munro sliding threshold
 * - Homeostatic: Synaptic scaling and intrinsic plasticity
 * - STP: Short-Term Plasticity (depression/facilitation)
 * - Calcium: Calcium-dependent learning rate dynamics
 *
 * All functions support both CUDA GPU and CPU fallback implementations.
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PLASTICITY_GPU_H
#define NIMCP_PLASTICITY_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// STDP Parameters and Structures
//=============================================================================

/**
 * @brief Extended STDP parameters for GPU kernels
 */
typedef struct {
    float A_plus;       /**< LTP amplitude */
    float A_minus;      /**< LTD amplitude */
    float tau_plus;     /**< LTP time constant (ms) */
    float tau_minus;    /**< LTD time constant (ms) */
    float w_max;        /**< Maximum weight */
    float w_min;        /**< Minimum weight */
    float da_mod_gain;  /**< Dopamine modulation gain */
    float burst_amp;    /**< Burst amplification factor */
    bool soft_bounds;   /**< Use soft bounds (weight-dependent) */
} nimcp_gpu_stdp_params_t;

/**
 * @brief Triplet STDP parameters
 */
typedef struct {
    float A2_plus;      /**< Pairwise LTP amplitude */
    float A3_plus;      /**< Triplet LTP amplitude */
    float A2_minus;     /**< Pairwise LTD amplitude */
    float A3_minus;     /**< Triplet LTD amplitude */
    float tau_plus;     /**< Fast pre-trace time constant (ms) */
    float tau_minus;    /**< Fast post-trace time constant (ms) */
    float tau_x;        /**< Slow pre-trace time constant (ms) */
    float tau_y;        /**< Slow post-trace time constant (ms) */
    float w_max;        /**< Maximum weight */
    float w_min;        /**< Minimum weight */
} nimcp_gpu_triplet_stdp_params_t;

//=============================================================================
// BCM Parameters and Structures
//=============================================================================

/**
 * @brief BCM learning parameters for GPU
 */
typedef struct {
    float learning_rate;          /**< Base learning rate */
    float threshold_tau;          /**< Threshold adaptation time constant */
    float activity_tau;           /**< Activity averaging time constant */
    float min_threshold;          /**< Minimum modification threshold */
    float max_threshold;          /**< Maximum modification threshold */
    float theta_power;            /**< Power for threshold computation (typically 2) */
} nimcp_gpu_bcm_params_t;

/**
 * @brief BCM synapse state for GPU batch processing
 */
typedef struct {
    nimcp_gpu_tensor_t* weights;      /**< Synaptic weights */
    nimcp_gpu_tensor_t* thresholds;   /**< Sliding thresholds per synapse */
    nimcp_gpu_tensor_t* avg_activity; /**< Running average of post-synaptic activity */
    nimcp_gpu_tensor_t* eligibility;  /**< Eligibility traces */
    size_t n_synapses;                /**< Number of synapses */
} nimcp_gpu_bcm_state_t;

//=============================================================================
// Homeostatic Plasticity Parameters and Structures
//=============================================================================

/**
 * @brief Synaptic scaling parameters for GPU
 */
typedef struct {
    float target_rate;           /**< Target firing rate (Hz) */
    float scaling_tau;           /**< Scaling time constant */
    float scaling_exponent;      /**< Scaling power (0.5-2.0) */
    float min_scale;             /**< Minimum scaling factor */
    float max_scale;             /**< Maximum scaling factor */
    float rate_tau;              /**< Rate averaging time constant */
} nimcp_gpu_scaling_params_t;

/**
 * @brief Intrinsic plasticity parameters for GPU
 */
typedef struct {
    float target_rate;           /**< Target firing rate */
    float threshold_tau;         /**< Threshold adaptation time constant */
    float gain_tau;              /**< Gain adaptation time constant */
    float min_threshold;         /**< Minimum threshold */
    float max_threshold;         /**< Maximum threshold */
    float min_gain;              /**< Minimum gain */
    float max_gain;              /**< Maximum gain */
    float learning_rate;         /**< Intrinsic plasticity learning rate */
} nimcp_gpu_intrinsic_params_t;

/**
 * @brief Homeostatic state for GPU batch processing
 */
typedef struct {
    nimcp_gpu_tensor_t* scaling_factors;  /**< Per-neuron scaling factors */
    nimcp_gpu_tensor_t* avg_rates;        /**< Running average firing rates */
    nimcp_gpu_tensor_t* thresholds;       /**< Firing thresholds */
    nimcp_gpu_tensor_t* gains;            /**< Input-output gains */
    size_t n_neurons;                     /**< Number of neurons */
} nimcp_gpu_homeostatic_state_t;

//=============================================================================
// STP Parameters and Structures
//=============================================================================

/**
 * @brief Short-term plasticity parameters for GPU
 */
typedef struct {
    float U;            /**< Baseline release probability */
    float tau_D;        /**< Depression recovery time constant (ms) */
    float tau_F;        /**< Facilitation decay time constant (ms) */
} nimcp_gpu_stp_params_t;

/**
 * @brief STP state for GPU batch processing
 */
typedef struct {
    nimcp_gpu_tensor_t* x;           /**< Available resources [0-1] */
    nimcp_gpu_tensor_t* u;           /**< Utilization probability [0-1] */
    nimcp_gpu_tensor_t* last_spike;  /**< Last spike time per synapse */
    size_t n_synapses;               /**< Number of synapses */
    nimcp_gpu_stp_params_t params;   /**< STP parameters */
} nimcp_gpu_stp_state_t;

//=============================================================================
// Calcium Dynamics Parameters and Structures
//=============================================================================

/**
 * @brief Calcium dynamics parameters for GPU
 */
typedef struct {
    float baseline;          /**< Resting calcium concentration (uM) */
    float threshold_ltd;     /**< LTD threshold (uM) */
    float threshold_ltp;     /**< LTP threshold (uM) */
    float threshold_sat;     /**< Saturation threshold (uM) */
    float max_conc;          /**< Maximum concentration (uM) */
    float decay_tau;         /**< Decay time constant (ms) */
    float pump_rate;         /**< Pump extrusion rate */
    float buffer_capacity;   /**< Buffering capacity */
    float influx_alpha;      /**< Influx rate constant */
    float omega_max;         /**< Maximum learning rate */
    float omega_power;       /**< Omega function power */
} nimcp_gpu_calcium_params_t;

/**
 * @brief Calcium state for GPU batch processing
 */
typedef struct {
    nimcp_gpu_tensor_t* concentration;   /**< Current calcium concentration */
    nimcp_gpu_tensor_t* learning_rate;   /**< Current learning rate from omega */
    nimcp_gpu_tensor_t* nmda_activation; /**< NMDA receptor activation */
    size_t n_synapses;                   /**< Number of synapses */
    nimcp_gpu_calcium_params_t params;   /**< Calcium parameters */
} nimcp_gpu_calcium_state_t;

//=============================================================================
// STDP GPU Functions
//=============================================================================

/**
 * @brief Create default GPU STDP parameters
 * @return Default STDP parameters
 */
NIMCP_EXPORT nimcp_gpu_stdp_params_t nimcp_gpu_stdp_params_default(void);

/**
 * @brief Update STDP eligibility traces on GPU
 *
 * trace_new = trace_old * exp(-dt/tau) + spike
 *
 * @param ctx GPU context
 * @param pre_trace Pre-synaptic trace (in-place update)
 * @param post_trace Post-synaptic trace (in-place update)
 * @param pre_spikes Pre-synaptic spikes (binary)
 * @param post_spikes Post-synaptic spikes (binary)
 * @param dt Time step (ms)
 * @param params STDP parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stdp_update_traces(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* pre_trace,
    nimcp_gpu_tensor_t* post_trace,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    float dt,
    const nimcp_gpu_stdp_params_t* params
);

/**
 * @brief Apply STDP weight update on GPU
 *
 * Computes weight changes based on spike timing using traces.
 * LTP: post spike with pre trace (pre before post)
 * LTD: pre spike with post trace (post before pre)
 *
 * @param ctx GPU context
 * @param weights Weight matrix [n_post x n_pre] (in-place update)
 * @param pre_spikes Pre-synaptic spikes
 * @param post_spikes Post-synaptic spikes
 * @param pre_trace Pre-synaptic eligibility trace
 * @param post_trace Post-synaptic eligibility trace
 * @param params STDP parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stdp_apply(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace,
    const nimcp_gpu_tensor_t* post_trace,
    const nimcp_gpu_stdp_params_t* params
);

/**
 * @brief Apply dopamine-modulated STDP on GPU
 *
 * Weight changes scaled by dopamine concentration.
 *
 * @param ctx GPU context
 * @param weights Weight matrix (in-place update)
 * @param pre_spikes Pre-synaptic spikes
 * @param post_spikes Post-synaptic spikes
 * @param pre_trace Pre-synaptic eligibility trace
 * @param post_trace Post-synaptic eligibility trace
 * @param dopamine Dopamine concentration per synapse
 * @param params STDP parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stdp_apply_modulated(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* pre_trace,
    const nimcp_gpu_tensor_t* post_trace,
    const nimcp_gpu_tensor_t* dopamine,
    const nimcp_gpu_stdp_params_t* params
);

//=============================================================================
// Triplet STDP GPU Functions
//=============================================================================

/**
 * @brief Create default triplet STDP parameters (Pfister & Gerstner 2006)
 * @return Default triplet STDP parameters
 */
NIMCP_EXPORT nimcp_gpu_triplet_stdp_params_t nimcp_gpu_triplet_stdp_params_default(void);

/**
 * @brief Update all four triplet STDP traces
 *
 * @param ctx GPU context
 * @param r1_pre Fast pre-synaptic trace
 * @param r2_pre Slow pre-synaptic trace
 * @param o1_post Fast post-synaptic trace
 * @param o2_post Slow post-synaptic trace
 * @param pre_spikes Pre-synaptic spikes
 * @param post_spikes Post-synaptic spikes
 * @param dt Time step (ms)
 * @param params Triplet STDP parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_triplet_stdp_update_traces(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* r1_pre,
    nimcp_gpu_tensor_t* r2_pre,
    nimcp_gpu_tensor_t* o1_post,
    nimcp_gpu_tensor_t* o2_post,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    float dt,
    const nimcp_gpu_triplet_stdp_params_t* params
);

/**
 * @brief Apply triplet STDP weight update
 *
 * LTP = A2_plus * r1_pre + A3_plus * r2_pre * o1_post
 * LTD = A2_minus * o1_post + A3_minus * r1_pre * o2_post
 *
 * @param ctx GPU context
 * @param weights Weight matrix (in-place update)
 * @param pre_spikes Pre-synaptic spikes
 * @param post_spikes Post-synaptic spikes
 * @param r1_pre Fast pre-synaptic trace
 * @param r2_pre Slow pre-synaptic trace
 * @param o1_post Fast post-synaptic trace
 * @param o2_post Slow post-synaptic trace
 * @param params Triplet STDP parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_triplet_stdp_apply(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_spikes,
    const nimcp_gpu_tensor_t* post_spikes,
    const nimcp_gpu_tensor_t* r1_pre,
    const nimcp_gpu_tensor_t* r2_pre,
    const nimcp_gpu_tensor_t* o1_post,
    const nimcp_gpu_tensor_t* o2_post,
    const nimcp_gpu_triplet_stdp_params_t* params
);

//=============================================================================
// BCM GPU Functions
//=============================================================================

/**
 * @brief Create default BCM parameters
 * @return Default BCM parameters
 */
NIMCP_EXPORT nimcp_gpu_bcm_params_t nimcp_gpu_bcm_params_default(void);

/**
 * @brief Create BCM state on GPU
 *
 * @param ctx GPU context
 * @param n_pre Number of pre-synaptic neurons
 * @param n_post Number of post-synaptic neurons
 * @param params BCM parameters
 * @return BCM state or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_bcm_state_t* nimcp_gpu_bcm_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_pre,
    size_t n_post,
    const nimcp_gpu_bcm_params_t* params
);

/**
 * @brief Destroy BCM state
 * @param state BCM state to destroy
 */
NIMCP_EXPORT void nimcp_gpu_bcm_state_destroy(nimcp_gpu_bcm_state_t* state);

/**
 * @brief Update BCM sliding thresholds
 *
 * theta = theta + (post^theta_power - theta) / tau
 *
 * @param ctx GPU context
 * @param thresholds Threshold tensor (in-place update)
 * @param post_activity Post-synaptic activity
 * @param dt Time step (ms)
 * @param params BCM parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_bcm_update_threshold(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* thresholds,
    const nimcp_gpu_tensor_t* post_activity,
    float dt,
    const nimcp_gpu_bcm_params_t* params
);

/**
 * @brief Apply BCM learning rule on GPU
 *
 * dw = eta * post * (post - theta) * pre
 *
 * @param ctx GPU context
 * @param weights Weight matrix (in-place update)
 * @param pre_activity Pre-synaptic activity
 * @param post_activity Post-synaptic activity
 * @param thresholds Sliding thresholds
 * @param dt Time step (ms)
 * @param params BCM parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_bcm_apply(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_activity,
    const nimcp_gpu_tensor_t* post_activity,
    const nimcp_gpu_tensor_t* thresholds,
    float dt,
    const nimcp_gpu_bcm_params_t* params
);

/**
 * @brief Apply neuromodulator-gated BCM
 *
 * @param ctx GPU context
 * @param weights Weight matrix (in-place update)
 * @param pre_activity Pre-synaptic activity
 * @param post_activity Post-synaptic activity
 * @param thresholds Sliding thresholds
 * @param neuromodulator Neuromodulator level (0-1)
 * @param dt Time step (ms)
 * @param params BCM parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_bcm_apply_modulated(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* pre_activity,
    const nimcp_gpu_tensor_t* post_activity,
    const nimcp_gpu_tensor_t* thresholds,
    float neuromodulator,
    float dt,
    const nimcp_gpu_bcm_params_t* params
);

//=============================================================================
// Homeostatic Plasticity GPU Functions
//=============================================================================

/**
 * @brief Create default synaptic scaling parameters
 * @return Default scaling parameters
 */
NIMCP_EXPORT nimcp_gpu_scaling_params_t nimcp_gpu_scaling_params_default(void);

/**
 * @brief Create default intrinsic plasticity parameters
 * @return Default intrinsic plasticity parameters
 */
NIMCP_EXPORT nimcp_gpu_intrinsic_params_t nimcp_gpu_intrinsic_params_default(void);

/**
 * @brief Create homeostatic state on GPU
 *
 * @param ctx GPU context
 * @param n_neurons Number of neurons
 * @return Homeostatic state or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_homeostatic_state_t* nimcp_gpu_homeostatic_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_neurons
);

/**
 * @brief Destroy homeostatic state
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_gpu_homeostatic_state_destroy(nimcp_gpu_homeostatic_state_t* state);

/**
 * @brief Update firing rate estimates
 *
 * rate = rate + (spike - rate) * (1 - exp(-dt/tau))
 *
 * @param ctx GPU context
 * @param avg_rates Running average rates (in-place update)
 * @param spikes Current spikes
 * @param dt Time step (ms)
 * @param params Scaling parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_homeostatic_update_rates(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* avg_rates,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_scaling_params_t* params
);

/**
 * @brief Compute synaptic scaling factors
 *
 * factor = (target_rate / actual_rate)^alpha
 *
 * @param ctx GPU context
 * @param scaling_factors Output scaling factors
 * @param avg_rates Current average rates
 * @param params Scaling parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_homeostatic_compute_scaling(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* scaling_factors,
    const nimcp_gpu_tensor_t* avg_rates,
    const nimcp_gpu_scaling_params_t* params
);

/**
 * @brief Apply synaptic scaling to weights
 *
 * w_new = clamp(w_old * scaling_factor, w_min, w_max)
 *
 * @param ctx GPU context
 * @param weights Weight matrix (in-place update)
 * @param scaling_factors Scaling factors per post-synaptic neuron
 * @param w_min Minimum weight
 * @param w_max Maximum weight
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_homeostatic_apply_scaling(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* scaling_factors,
    float w_min,
    float w_max
);

/**
 * @brief Update intrinsic plasticity (threshold adaptation)
 *
 * threshold += eta * (actual_rate - target_rate)
 *
 * @param ctx GPU context
 * @param thresholds Firing thresholds (in-place update)
 * @param avg_rates Current average rates
 * @param dt Time step (ms)
 * @param params Intrinsic plasticity parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_intrinsic_plasticity_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* thresholds,
    const nimcp_gpu_tensor_t* avg_rates,
    float dt,
    const nimcp_gpu_intrinsic_params_t* params
);

//=============================================================================
// STP GPU Functions
//=============================================================================

/**
 * @brief Create default STP parameters
 * @return Default STP parameters
 */
NIMCP_EXPORT nimcp_gpu_stp_params_t nimcp_gpu_stp_params_default(void);

/**
 * @brief Create STP state on GPU
 *
 * @param ctx GPU context
 * @param n_synapses Number of synapses
 * @param params STP parameters
 * @return STP state or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_stp_state_t* nimcp_gpu_stp_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    const nimcp_gpu_stp_params_t* params
);

/**
 * @brief Destroy STP state
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_gpu_stp_state_destroy(nimcp_gpu_stp_state_t* state);

/**
 * @brief Update STP state (continuous decay)
 *
 * x += (1 - x) * (1 - exp(-dt/tau_D))  (resource recovery)
 * u += (U - u) * (1 - exp(-dt/tau_F))  (facilitation decay)
 *
 * @param ctx GPU context
 * @param state STP state (in-place update)
 * @param dt Time step (ms)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stp_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_stp_state_t* state,
    float dt
);

/**
 * @brief Process presynaptic spikes for STP
 *
 * u = u + U * (1 - u)  (facilitation)
 * x = x - u * x        (depression)
 *
 * @param ctx GPU context
 * @param state STP state (in-place update)
 * @param spikes Pre-synaptic spikes
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stp_process_spikes(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_stp_state_t* state,
    const nimcp_gpu_tensor_t* spikes
);

/**
 * @brief Compute STP modulation factor
 *
 * modulation = u * x (current effective strength)
 *
 * @param ctx GPU context
 * @param state STP state
 * @param modulation Output modulation factors
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stp_get_modulation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_stp_state_t* state,
    nimcp_gpu_tensor_t* modulation
);

/**
 * @brief Apply STP modulation to weights
 *
 * effective_weight = base_weight * u * x
 *
 * @param ctx GPU context
 * @param base_weights Base synaptic weights
 * @param state STP state
 * @param effective_weights Output: modulated weights
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stp_apply(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* base_weights,
    const nimcp_gpu_stp_state_t* state,
    nimcp_gpu_tensor_t* effective_weights
);

/**
 * @brief Reset STP state to resting values
 *
 * @param ctx GPU context
 * @param state STP state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_stp_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_stp_state_t* state
);

//=============================================================================
// Calcium Dynamics GPU Functions
//=============================================================================

/**
 * @brief Create default calcium dynamics parameters
 * @return Default calcium parameters
 */
NIMCP_EXPORT nimcp_gpu_calcium_params_t nimcp_gpu_calcium_params_default(void);

/**
 * @brief Create calcium state on GPU
 *
 * @param ctx GPU context
 * @param n_synapses Number of synapses
 * @param params Calcium parameters
 * @return Calcium state or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_calcium_state_t* nimcp_gpu_calcium_state_create(
    nimcp_gpu_context_t* ctx,
    size_t n_synapses,
    const nimcp_gpu_calcium_params_t* params
);

/**
 * @brief Destroy calcium state
 * @param state State to destroy
 */
NIMCP_EXPORT void nimcp_gpu_calcium_state_destroy(nimcp_gpu_calcium_state_t* state);

/**
 * @brief Update calcium dynamics (decay/extrusion)
 *
 * d[Ca]/dt = -pump_rate * [Ca] - buffer_cap * ([Ca] - baseline)
 *
 * @param ctx GPU context
 * @param state Calcium state (in-place update)
 * @param dt Time step (ms)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_calcium_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state,
    float dt
);

/**
 * @brief Trigger NMDA-mediated calcium influx
 *
 * [Ca] += alpha * nmda_activation * mg_block_factor
 *
 * @param ctx GPU context
 * @param state Calcium state (in-place update)
 * @param nmda_activation NMDA receptor activation level
 * @param postsynaptic_voltage Postsynaptic voltage (mV)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_calcium_nmda_influx(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state,
    const nimcp_gpu_tensor_t* nmda_activation,
    const nimcp_gpu_tensor_t* postsynaptic_voltage
);

/**
 * @brief Compute learning rates from calcium concentrations (Omega function)
 *
 * omega([Ca]) = omega_max * (([Ca] - theta_LTD) / (theta_LTP - theta_LTD))^p
 *
 * @param ctx GPU context
 * @param state Calcium state (updates learning_rate tensor)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_calcium_compute_learning_rate(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state
);

/**
 * @brief Apply calcium-dependent weight update
 *
 * Uses learning rate from omega function to modulate plasticity
 *
 * @param ctx GPU context
 * @param weights Weight matrix (in-place update)
 * @param state Calcium state (contains learning rates)
 * @param pre_activity Pre-synaptic activity
 * @param post_activity Post-synaptic activity
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_calcium_apply_plasticity(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_calcium_state_t* state,
    const nimcp_gpu_tensor_t* pre_activity,
    const nimcp_gpu_tensor_t* post_activity
);

/**
 * @brief Reset calcium to baseline
 *
 * @param ctx GPU context
 * @param state Calcium state
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_calcium_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_calcium_state_t* state
);

/**
 * @brief Compute Mg2+ block factor for NMDA
 *
 * block = 1 / (1 + [Mg] * exp(-0.062 * V) / 3.57)
 *
 * @param ctx GPU context
 * @param voltage Postsynaptic voltage (mV)
 * @param mg_block Output: Mg block factors
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_calcium_mg_block(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* voltage,
    nimcp_gpu_tensor_t* mg_block
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get plasticity regime from calcium concentration
 *
 * 0 = no plasticity, 1 = LTD, 2 = transition, 3 = LTP, 4 = saturated
 *
 * @param ctx GPU context
 * @param concentration Calcium concentration tensor
 * @param regime Output: regime per synapse (int tensor)
 * @param params Calcium parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_calcium_get_regime(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* concentration,
    nimcp_gpu_tensor_t* regime,
    const nimcp_gpu_calcium_params_t* params
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PLASTICITY_GPU_H
