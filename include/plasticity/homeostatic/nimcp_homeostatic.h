/**
 * @file nimcp_homeostatic.h
 * @brief Homeostatic Plasticity - Synaptic Scaling and Intrinsic Plasticity
 *
 * WHAT: Activity-dependent mechanisms that maintain neural stability
 * WHY:  Hebbian learning alone leads to runaway excitation; homeostasis balances
 *
 * BIOLOGICAL BASIS:
 * - Synaptic Scaling: Multiplicative adjustment of all synapses (Turrigiano et al. 1998)
 * - Intrinsic Plasticity: Adjustment of neuronal excitability (Desai et al. 1999)
 * - Metaplasticity: Plasticity of plasticity thresholds (Abraham & Bear 1996)
 *
 * MATHEMATICAL FORMULATION:
 *
 * 1. Synaptic Scaling (multiplicative):
 *    w_scaled = w × (target_rate / actual_rate)^α
 *    α = scaling exponent (0.5-2.0)
 *
 * 2. Intrinsic Plasticity (threshold adaptation):
 *    dθ/dt = (actual_rate - target_rate) / τ_ip
 *    θ = firing threshold
 *
 * 3. Metaplasticity (BCM threshold sliding):
 *    θ_m = <r²> (sliding threshold based on squared activity)
 *
 * DESIGN PATTERNS:
 * - Strategy Pattern: Different homeostatic mechanisms
 * - Observer Pattern: Notify when stability achieved
 * - Factory Method: Preset configurations for brain regions
 *
 * PERFORMANCE:
 * - O(1) per synapse for scaling
 * - O(n) per neuron for global normalization
 * - SIMD-optimized batch operations
 *
 * @author NIMCP Development Team
 * @date 2025-11-27
 */

#ifndef NIMCP_HOMEOSTATIC_H
#define NIMCP_HOMEOSTATIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cognitive/nimcp_sleep_wake.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define HOMEOSTATIC_EPSILON 1e-8f           /**< Numerical stability constant */
#define HOMEOSTATIC_DEFAULT_TARGET_RATE 5.0f /**< Default target firing rate (Hz) */
#define HOMEOSTATIC_MIN_WEIGHT 0.0f         /**< Minimum synaptic weight */
#define HOMEOSTATIC_MAX_WEIGHT 1.0f         /**< Maximum synaptic weight */

//=============================================================================
// Homeostatic Mechanism Types
//=============================================================================

/**
 * @brief Homeostatic mechanism type
 *
 * WHAT: Enumeration of available homeostatic mechanisms
 * WHY:  Different mechanisms for different timescales and purposes
 */
typedef enum {
    HOMEOSTATIC_SYNAPTIC_SCALING,    /**< Multiplicative scaling of all synapses */
    HOMEOSTATIC_INTRINSIC_PLASTICITY, /**< Threshold/excitability adjustment */
    HOMEOSTATIC_METAPLASTICITY,       /**< Sliding BCM threshold */
    HOMEOSTATIC_STRUCTURAL,           /**< Synapse addition/removal */
    HOMEOSTATIC_COMBINED              /**< Multiple mechanisms together */
} homeostatic_mechanism_t;

//=============================================================================
// Synaptic Scaling Structures
//=============================================================================

/**
 * @brief Synaptic scaling parameters
 *
 * WHAT: Configuration for synaptic scaling homeostasis
 * WHY:  Different brain regions have different scaling dynamics
 *
 * BIOLOGICAL: Based on Turrigiano et al. 1998 (TTX/bicuculline experiments)
 */
typedef struct {
    float target_rate;           /**< Target firing rate (Hz) - typically 1-10 Hz */
    float scaling_time_constant; /**< τ_scale: Time constant for scaling (hours→ms) */
    float scaling_exponent;      /**< α: Scaling exponent (0.5=sublinear, 1.0=linear, 2.0=supralinear) */
    float min_scaling_factor;    /**< Minimum multiplicative factor (0.1) */
    float max_scaling_factor;    /**< Maximum multiplicative factor (10.0) */
    float rate_averaging_tau;    /**< τ_avg: Time constant for rate averaging (seconds) */
} synaptic_scaling_params_t;

/**
 * @brief Synaptic scaling state for a neuron
 *
 * WHAT: Per-neuron state for synaptic scaling
 * WHY:  Track firing rate and compute scaling factors
 *
 * MEMORY: 24 bytes per neuron
 */
typedef struct {
    float average_rate;          /**< Running average firing rate (Hz) */
    float scaling_factor;        /**< Current multiplicative scaling factor */
    float rate_integral;         /**< Integrated rate for averaging */
    uint64_t spike_count;        /**< Spike counter for rate computation */
    uint64_t last_update_time;   /**< Last update timestamp (μs) */
    bool is_stable;              /**< True if within target range */
} synaptic_scaling_state_t;

//=============================================================================
// Intrinsic Plasticity Structures
//=============================================================================

/**
 * @brief Intrinsic plasticity parameters
 *
 * WHAT: Configuration for intrinsic excitability plasticity
 * WHY:  Modulates neuronal input-output function
 *
 * BIOLOGICAL: Based on Desai et al. 1999 (cultured cortical neurons)
 */
typedef struct {
    float target_rate;           /**< Target firing rate (Hz) */
    float threshold_tau;         /**< τ_θ: Time constant for threshold adaptation (ms) */
    float gain_tau;              /**< τ_g: Time constant for gain adaptation (ms) */
    float min_threshold;         /**< Minimum firing threshold */
    float max_threshold;         /**< Maximum firing threshold */
    float min_gain;              /**< Minimum gain (steepness of f-I curve) */
    float max_gain;              /**< Maximum gain */
    float learning_rate;         /**< η: Learning rate for IP */
} intrinsic_plasticity_params_t;

/**
 * @brief Intrinsic plasticity state for a neuron
 *
 * WHAT: Per-neuron state for intrinsic plasticity
 * WHY:  Track and adapt threshold and gain
 *
 * MEMORY: 28 bytes per neuron
 */
typedef struct {
    float threshold;             /**< Current firing threshold (mV or normalized) */
    float gain;                  /**< Current gain (f-I curve slope) */
    float average_rate;          /**< Running average firing rate */
    float average_input;         /**< Running average input current */
    uint64_t last_update_time;   /**< Last update timestamp (μs) */
    bool is_stable;              /**< True if within target range */
} intrinsic_plasticity_state_t;

//=============================================================================
// Metaplasticity Structures
//=============================================================================

/**
 * @brief Metaplasticity parameters
 *
 * WHAT: Configuration for metaplasticity (plasticity of plasticity)
 * WHY:  Sliding threshold prevents runaway LTP/LTD
 *
 * BIOLOGICAL: Abraham & Bear 1996 (BCM theory extension)
 */
typedef struct {
    float theta_tau;             /**< τ_θ: Time constant for threshold sliding (ms) */
    float activity_tau;          /**< τ_a: Time constant for activity averaging (ms) */
    float min_theta;             /**< Minimum modification threshold */
    float max_theta;             /**< Maximum modification threshold */
    float theta_power;           /**< Power for <r^p> (typically 2 for BCM) */
} metaplasticity_params_t;

/**
 * @brief Metaplasticity state for a synapse/neuron
 *
 * WHAT: Per-synapse or per-neuron metaplasticity state
 * WHY:  Track sliding threshold
 *
 * MEMORY: 16 bytes
 */
typedef struct {
    float theta;                 /**< Current modification threshold */
    float activity_squared_avg;  /**< Running average of squared activity */
    float activity_avg;          /**< Running average of activity */
    float plasticity_rate;       /**< Current effective plasticity rate */
} metaplasticity_state_t;

//=============================================================================
// Combined Homeostatic Controller
//=============================================================================

/**
 * @brief Homeostatic controller configuration
 *
 * WHAT: Master configuration for all homeostatic mechanisms
 * WHY:  Unified control of multiple stability mechanisms
 */
typedef struct {
    bool enable_synaptic_scaling;
    bool enable_intrinsic_plasticity;
    bool enable_metaplasticity;
    bool enable_structural_plasticity;
    bool enable_bio_async;            /**< Enable bio-async communication */

    synaptic_scaling_params_t scaling_params;
    intrinsic_plasticity_params_t ip_params;
    metaplasticity_params_t meta_params;

    float global_stability_threshold; /**< When to consider system stable */
    float update_interval_ms;         /**< How often to apply homeostasis */
} homeostatic_config_t;

/**
 * @brief Homeostatic controller statistics
 *
 * WHAT: Monitoring metrics for homeostatic mechanisms
 * WHY:  Track effectiveness and convergence
 */
typedef struct {
    uint64_t total_updates;
    uint64_t scaling_events;
    uint64_t threshold_adjustments;
    float mean_scaling_factor;
    float mean_firing_rate;
    float rate_variance;
    float stability_score;       /**< 0-1, higher = more stable */
    uint32_t neurons_above_target;
    uint32_t neurons_below_target;
    uint32_t neurons_stable;
} homeostatic_stats_t;

/**
 * @brief Opaque handle to homeostatic controller
 */
typedef struct homeostatic_controller_struct* homeostatic_controller_t;

//=============================================================================
// Factory Functions - Parameter Presets
//=============================================================================

/**
 * @brief Create default synaptic scaling parameters
 *
 * WHAT: Factory method for cortical synaptic scaling
 * WHY:  Provides biologically plausible defaults
 *
 * BIOLOGICAL: Based on Turrigiano et al. 1998
 * - Target rate: 5 Hz (typical cortical neuron)
 * - Time constant: Hours (accelerated to seconds for simulation)
 *
 * @return Synaptic scaling parameters
 */
synaptic_scaling_params_t homeostatic_scaling_params_default(void);

/**
 * @brief Create fast synaptic scaling parameters
 *
 * WHAT: Accelerated scaling for rapid adaptation
 * WHY:  Faster convergence in simulation
 *
 * @return Fast scaling parameters
 */
synaptic_scaling_params_t homeostatic_scaling_params_fast(void);

/**
 * @brief Create intrinsic plasticity parameters
 *
 * WHAT: Factory method for intrinsic plasticity
 * WHY:  Provides defaults for threshold/gain adaptation
 *
 * BIOLOGICAL: Based on Desai et al. 1999
 *
 * @return Intrinsic plasticity parameters
 */
intrinsic_plasticity_params_t homeostatic_ip_params_default(void);

/**
 * @brief Create metaplasticity parameters
 *
 * WHAT: Factory method for metaplasticity
 * WHY:  Provides defaults for sliding threshold
 *
 * BIOLOGICAL: Based on BCM theory
 *
 * @return Metaplasticity parameters
 */
metaplasticity_params_t homeostatic_meta_params_default(void);

/**
 * @brief Create default homeostatic controller configuration
 *
 * WHAT: Full configuration with all mechanisms enabled
 * WHY:  Comprehensive homeostasis for stable learning
 *
 * @return Default configuration
 */
homeostatic_config_t homeostatic_config_default(void);

//=============================================================================
// Synaptic Scaling Functions
//=============================================================================

/**
 * @brief Initialize synaptic scaling state
 *
 * WHAT: Factory method for scaling state initialization
 * WHY:  Ensure valid initial state
 *
 * @param initial_rate Initial estimated firing rate (Hz)
 * @return Initialized scaling state
 */
synaptic_scaling_state_t synaptic_scaling_state_init(float initial_rate);

/**
 * @brief Update firing rate estimate
 *
 * WHAT: Update running average of firing rate
 * WHY:  Rate estimate drives scaling decisions
 *
 * FORMULA: rate_avg = rate_avg + (1 - exp(-dt/τ)) × (instantaneous - rate_avg)
 *
 * COMPLEXITY: O(1)
 *
 * @param state Scaling state to update
 * @param spike_occurred True if spike occurred this timestep
 * @param dt Time step (ms)
 * @param params Scaling parameters
 */
void synaptic_scaling_update_rate(synaptic_scaling_state_t* state,
                                  bool spike_occurred,
                                  float dt,
                                  const synaptic_scaling_params_t* params);

/**
 * @brief Compute scaling factor for synapse weights
 *
 * WHAT: Calculate multiplicative factor based on rate deviation
 * WHY:  Scale synapses to bring rate toward target
 *
 * FORMULA: factor = (target_rate / actual_rate)^α
 *
 * COMPLEXITY: O(1)
 *
 * @param state Current scaling state
 * @param params Scaling parameters
 * @return Scaling factor (clamped to valid range)
 */
float synaptic_scaling_compute_factor(const synaptic_scaling_state_t* state,
                                      const synaptic_scaling_params_t* params);

/**
 * @brief Apply scaling to array of weights
 *
 * WHAT: Multiplicatively scale all weights
 * WHY:  Implement global synaptic homeostasis
 *
 * FORMULA: w_new[i] = clamp(w_old[i] × factor, w_min, w_max)
 *
 * COMPLEXITY: O(n) where n = num_weights
 * PERFORMANCE: SIMD-optimized for large arrays
 *
 * @param weights Array of synaptic weights (modified in place)
 * @param num_weights Number of weights
 * @param scaling_factor Multiplicative factor
 */
void synaptic_scaling_apply(float* weights,
                            uint32_t num_weights,
                            float scaling_factor);

/**
 * @brief Apply scaling with weight-dependent bounds
 *
 * WHAT: Soft bounds on scaling (stronger scaling for extreme weights)
 * WHY:  Prevent weights from saturating at bounds
 *
 * BIOLOGICAL: Weights near bounds scale less to maintain distribution
 *
 * @param weights Array of weights
 * @param num_weights Number of weights
 * @param scaling_factor Base scaling factor
 * @param soft_bound_strength How strongly to apply soft bounds (0-1)
 */
void synaptic_scaling_apply_soft_bounds(float* weights,
                                        uint32_t num_weights,
                                        float scaling_factor,
                                        float soft_bound_strength);

//=============================================================================
// Intrinsic Plasticity Functions
//=============================================================================

/**
 * @brief Initialize intrinsic plasticity state
 *
 * WHAT: Factory method for IP state initialization
 * WHY:  Ensure valid initial state
 *
 * @param initial_threshold Initial firing threshold
 * @param initial_gain Initial gain
 * @return Initialized IP state
 */
intrinsic_plasticity_state_t intrinsic_plasticity_state_init(float initial_threshold,
                                                              float initial_gain);

/**
 * @brief Update intrinsic plasticity (threshold adaptation)
 *
 * WHAT: Adapt firing threshold based on activity
 * WHY:  Maintain target firing rate via threshold
 *
 * FORMULA: dθ/dt = η × (actual_rate - target_rate) / τ
 *          If firing too much → increase threshold
 *          If firing too little → decrease threshold
 *
 * COMPLEXITY: O(1)
 *
 * @param state IP state to update
 * @param current_rate Current firing rate (Hz)
 * @param dt Time step (ms)
 * @param params IP parameters
 */
void intrinsic_plasticity_update_threshold(intrinsic_plasticity_state_t* state,
                                           float current_rate,
                                           float dt,
                                           const intrinsic_plasticity_params_t* params);

/**
 * @brief Update intrinsic plasticity (gain adaptation)
 *
 * WHAT: Adapt input-output gain based on input statistics
 * WHY:  Maximize information transmission
 *
 * FORMULA: dg/dt = η × (H_target - H_actual) × g
 *          Where H = entropy of output distribution
 *
 * COMPLEXITY: O(1)
 *
 * @param state IP state to update
 * @param input_mean Mean input current
 * @param input_variance Variance of input current
 * @param dt Time step (ms)
 * @param params IP parameters
 */
void intrinsic_plasticity_update_gain(intrinsic_plasticity_state_t* state,
                                      float input_mean,
                                      float input_variance,
                                      float dt,
                                      const intrinsic_plasticity_params_t* params);

/**
 * @brief Apply intrinsic plasticity to neuron activation
 *
 * WHAT: Transform input through adapted threshold/gain
 * WHY:  Implement homeostatic input-output function
 *
 * FORMULA: output = gain × (input - threshold)
 *          Or: output = sigmoid(gain × (input - threshold))
 *
 * COMPLEXITY: O(1)
 *
 * @param input Raw input current
 * @param state IP state with threshold and gain
 * @return Transformed output
 */
float intrinsic_plasticity_apply(float input,
                                 const intrinsic_plasticity_state_t* state);

//=============================================================================
// Metaplasticity Functions
//=============================================================================

/**
 * @brief Initialize metaplasticity state
 *
 * WHAT: Factory method for metaplasticity state
 * WHY:  Ensure valid initial state
 *
 * @param initial_theta Initial modification threshold
 * @return Initialized metaplasticity state
 */
metaplasticity_state_t metaplasticity_state_init(float initial_theta);

/**
 * @brief Update sliding modification threshold
 *
 * WHAT: Update θ_m based on activity history
 * WHY:  Implement BCM sliding threshold
 *
 * FORMULA: θ_m → <r²> (mean squared activity)
 *          dθ/dt = (r² - θ) / τ_θ
 *
 * COMPLEXITY: O(1)
 *
 * @param state Metaplasticity state
 * @param current_activity Current post-synaptic activity
 * @param dt Time step (ms)
 * @param params Metaplasticity parameters
 */
void metaplasticity_update_theta(metaplasticity_state_t* state,
                                 float current_activity,
                                 float dt,
                                 const metaplasticity_params_t* params);

/**
 * @brief Compute effective plasticity rate
 *
 * WHAT: Calculate current LTP/LTD rate based on metaplastic state
 * WHY:  Modulate plasticity strength based on history
 *
 * FORMULA: effective_rate = base_rate × f(θ_m)
 *          Where f() modulates based on sliding threshold
 *
 * @param state Metaplasticity state
 * @param base_plasticity_rate Base learning rate
 * @return Effective plasticity rate
 */
float metaplasticity_get_effective_rate(const metaplasticity_state_t* state,
                                        float base_plasticity_rate);

//=============================================================================
// Homeostatic Controller Functions
//=============================================================================

/**
 * @brief Create homeostatic controller
 *
 * WHAT: Factory method for controller
 * WHY:  Unified management of homeostatic mechanisms
 *
 * @param config Controller configuration
 * @param num_neurons Number of neurons to manage
 * @return Controller handle or NULL on failure
 */
homeostatic_controller_t homeostatic_controller_create(
    const homeostatic_config_t* config,
    uint32_t num_neurons);

/**
 * @brief Destroy homeostatic controller
 *
 * WHAT: Free controller resources
 * WHY:  Prevent memory leaks
 *
 * @param controller Controller to destroy
 */
void homeostatic_controller_destroy(homeostatic_controller_t controller);

/**
 * @brief Update all homeostatic mechanisms
 *
 * WHAT: Run one homeostatic update cycle
 * WHY:  Periodic maintenance of stability
 *
 * @param controller Controller handle
 * @param firing_rates Array of firing rates [num_neurons]
 * @param weights 2D weight matrix [num_neurons × num_synapses_per_neuron]
 * @param num_synapses_per_neuron Synapses per neuron
 * @param dt Time step (ms)
 */
void homeostatic_controller_update(homeostatic_controller_t controller,
                                   const float* firing_rates,
                                   float* weights,
                                   uint32_t num_synapses_per_neuron,
                                   float dt);

/**
 * @brief Get controller statistics
 *
 * WHAT: Retrieve homeostatic monitoring metrics
 * WHY:  Monitor stability and convergence
 *
 * @param controller Controller handle
 * @param stats Output statistics
 * @return true on success
 */
bool homeostatic_controller_get_stats(homeostatic_controller_t controller,
                                      homeostatic_stats_t* stats);

/**
 * @brief Check if system is stable
 *
 * WHAT: Determine if homeostasis achieved target
 * WHY:  Know when learning can proceed normally
 *
 * @param controller Controller handle
 * @return true if stable
 */
bool homeostatic_controller_is_stable(homeostatic_controller_t controller);

/**
 * @brief Reset controller to initial state
 *
 * WHAT: Reset all states to initial values
 * WHY:  Start fresh after major perturbation
 *
 * @param controller Controller handle
 */
void homeostatic_controller_reset(homeostatic_controller_t controller);

/**
 * @brief Set current sleep state for homeostatic modulation
 *
 * WHAT: Update the sleep state that modulates homeostatic plasticity
 * WHY:  Sleep state controls when and how much synaptic scaling occurs
 * HOW:  Store state, used in next update to apply sleep-based modulation
 *
 * BIOLOGICAL: Tononi's Synaptic Homeostasis Hypothesis - sleep is primary time for scaling
 *
 * @param controller Controller handle
 * @param sleep_state Current sleep state
 * @return true on success
 */
bool homeostatic_controller_set_sleep_state(homeostatic_controller_t controller,
                                             sleep_state_t sleep_state);

/**
 * @brief Get current sleep state
 *
 * WHAT: Query the current sleep state affecting homeostasis
 * WHY:  Check what modulation is being applied
 *
 * @param controller Controller handle
 * @return Current sleep state
 */
sleep_state_t homeostatic_controller_get_sleep_state(homeostatic_controller_t controller);

//=============================================================================
// Module Management
//=============================================================================

/**
 * @brief Initialize homeostatic module
 *
 * WHAT: Sets up homeostatic plasticity module
 * WHY: Prepares module for use
 * HOW: Initializes global state
 *
 * @param config Module configuration (can be NULL)
 * @return true on success, false on failure
 */
bool homeostatic_module_init(const homeostatic_config_t* config);

/**
 * @brief Destroy homeostatic module
 *
 * WHAT: Cleans up module resources
 * WHY: Proper shutdown and cleanup
 * HOW: Resets global state
 */
void homeostatic_module_destroy(void);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_HOMEOSTATIC_H
