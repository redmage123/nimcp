/**
 * @file nimcp_homeostatic.c
 * @brief Implementation of Homeostatic Plasticity mechanisms
 *
 * ARCHITECTURAL OVERVIEW:
 * - Strategy Pattern: Different homeostatic mechanisms
 * - Factory Pattern: Preset configurations
 * - Observer Pattern: Stability monitoring
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - SIMD-friendly loops for batch scaling
 * - Branchless math in hot paths
 * - Cache-coherent memory layout
 * - Inline helpers for core computations
 *
 * COMPLEXITY ANALYSIS:
 * - synaptic_scaling_apply: O(n) - linear in weights
 * - intrinsic_plasticity_update: O(1) - constant per neuron
 * - metaplasticity_update: O(1) - constant per synapse
 * - controller_update: O(n×m) - neurons × synapses
 *
 * DESIGN PRINCIPLES:
 * - Guard clauses for early returns
 * - Functions < 50 lines
 * - Explicit WHAT/WHY comments
 * - Thread-safe where noted
 */

#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/homeostatic/nimcp_homeostatic_sleep_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "plasticity_homeostatic"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(homeostatic)

//=============================================================================
// Constants
//=============================================================================

#define EPSILON HOMEOSTATIC_EPSILON
#define MIN_RATE 0.001f     /**< Minimum rate to prevent division by zero */
#define MAX_RATE 1000.0f    /**< Maximum physiological firing rate */

//=============================================================================
// Internal Controller Structure
//=============================================================================

/**
 * @brief Internal homeostatic controller structure
 *
 * WHAT: Complete state for managing all homeostatic mechanisms
 * WHY:  Centralized control of stability mechanisms
 */
struct homeostatic_controller_struct {
    homeostatic_config_t config;
    uint32_t num_neurons;

    /* Per-neuron states */
    synaptic_scaling_state_t* scaling_states;
    intrinsic_plasticity_state_t* ip_states;
    metaplasticity_state_t* meta_states;

    /* Statistics */
    homeostatic_stats_t stats;

    /* Timing */
    float time_since_update;
    uint64_t total_time_us;

    /* Sleep integration */
    sleep_state_t current_sleep_state;

    /* Thread safety */
    nimcp_mutex_t mutex;
};

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Safe division with epsilon guard
 *
 * WHAT: Division that handles near-zero denominators
 * WHY:  Prevent NaN/Inf propagation
 *
 * COMPLEXITY: O(1)
 */
static inline float safe_divide(float numerator, float denominator) {
    return numerator / (denominator + EPSILON);
}

/**
 * @brief Exponential decay factor
 *
 * WHAT: Compute 1 - exp(-dt/tau) for exponential averaging
 * WHY:  Common operation in all homeostatic mechanisms
 *
 * NUMERICAL STABILITY:
 * - Validates inputs for NaN/Inf
 * - Clamps exponential argument to prevent underflow
 * - exp(-20) ≈ 2e-9 which is negligible, so decay_factor ≈ 1.0
 *
 * COMPLEXITY: O(1)
 */
static inline float decay_factor(float dt, float tau) {
    /* Validate inputs */
    if (isnan(dt) || isnan(tau) || dt < 0.0F || tau <= 0.0F) {
        return 0.0F;  /* No decay for invalid inputs */
    }

    /* Compute exponent argument with epsilon guard for division */
    float exp_arg = -dt / (tau + EPSILON);

    /* Clamp to prevent underflow - exp(-20) ≈ 2e-9 is negligible */
    if (exp_arg < -20.0F) {
        return 1.0F;  /* Complete decay when exp_arg is very negative */
    }

    float result = 1.0F - expf(exp_arg);

    /* Validate result */
    if (isnan(result)) {
        return 0.0F;
    }

    return result;
}

/**
 * @brief Soft bounds function
 *
 * WHAT: Sigmoid-like compression near bounds
 * WHY:  Prevent weights from saturating at limits
 *
 * FORMULA: factor = 1 - strength × (2×|w - 0.5| - 1)²
 *          Near bounds (0 or 1): factor → (1-strength)
 *          At center (0.5): factor → 1.0
 */
static inline float soft_bound_factor(float weight, float strength) {
    float deviation = fabsf(weight - 0.5F);
    float compression = (2.0F * deviation);
    compression = compression * compression;
    return 1.0F - strength * compression;
}

//=============================================================================
// Factory Functions - Parameter Presets
//=============================================================================

synaptic_scaling_params_t homeostatic_scaling_params_default(void) {
    /* WHAT: Default synaptic scaling parameters
     * WHY:  Based on Turrigiano et al. 1998 experiments
     *
     * BIOLOGICAL:
     * - Target rate: ~5 Hz for cortical pyramidal neurons
     * - Time constant: Hours in biology, accelerated for simulation
     * - Exponent: 1.0 for linear scaling (multiplicative)
     */
    synaptic_scaling_params_t params = {
        .target_rate = 5.0F,              /* Hz - typical cortical rate */
        .scaling_time_constant = 10000.0F, /* ms = 10 sec (accelerated from hours) */
        .scaling_exponent = 1.0F,          /* Linear multiplicative scaling */
        .min_scaling_factor = 0.1F,        /* 10x maximum down-scaling */
        .max_scaling_factor = 10.0F,       /* 10x maximum up-scaling */
        .rate_averaging_tau = 1000.0F      /* ms = 1 sec for rate averaging */
    };
    return params;
}

synaptic_scaling_params_t homeostatic_scaling_params_fast(void) {
    /* WHAT: Fast scaling for rapid adaptation
     * WHY:  Useful for simulation speedup and testing
     */
    synaptic_scaling_params_t params = {
        .target_rate = 5.0F,
        .scaling_time_constant = 1000.0F,  /* 1 sec - 10x faster */
        .scaling_exponent = 1.0F,
        .min_scaling_factor = 0.2F,        /* Less aggressive bounds */
        .max_scaling_factor = 5.0F,
        .rate_averaging_tau = 200.0F       /* 200 ms - faster averaging */
    };
    return params;
}

intrinsic_plasticity_params_t homeostatic_ip_params_default(void) {
    /* WHAT: Default intrinsic plasticity parameters
     * WHY:  Based on Desai et al. 1999 experiments
     *
     * BIOLOGICAL:
     * - Threshold adaptation maintains target rate
     * - Gain adaptation maximizes information transmission
     */
    intrinsic_plasticity_params_t params = {
        .target_rate = 5.0F,              /* Hz */
        .threshold_tau = 5000.0F,          /* ms = 5 sec for threshold */
        .gain_tau = 10000.0F,              /* ms = 10 sec for gain */
        .min_threshold = -1.0F,            /* Normalized units */
        .max_threshold = 1.0F,
        .min_gain = 0.1F,                  /* Minimum gain */
        .max_gain = 10.0F,                 /* Maximum gain */
        .learning_rate = 0.01F             /* η for IP updates */
    };
    return params;
}

metaplasticity_params_t homeostatic_meta_params_default(void) {
    /* WHAT: Default metaplasticity parameters
     * WHY:  Based on BCM theory (Abraham & Bear 1996)
     *
     * BIOLOGICAL:
     * - θ_m tracks <r²> (mean squared activity)
     * - Higher activity → higher threshold → harder to potentiate
     */
    metaplasticity_params_t params = {
        .theta_tau = 1000.0F,              /* ms = 1 sec for θ sliding */
        .activity_tau = 500.0F,            /* ms for activity averaging */
        .min_theta = 0.01F,                /* Minimum threshold */
        .max_theta = 1.0F,                 /* Maximum threshold */
        .theta_power = 2.0F                /* Squared activity (BCM) */
    };
    return params;
}

homeostatic_config_t homeostatic_config_default(void) {
    /* WHAT: Default configuration with all mechanisms enabled
     * WHY:  Comprehensive homeostasis for stable learning
     */
    homeostatic_config_t config = {
        .enable_synaptic_scaling = true,
        .enable_intrinsic_plasticity = true,
        .enable_metaplasticity = true,
        .enable_structural_plasticity = false,  /* Not yet implemented */
        .scaling_params = homeostatic_scaling_params_default(),
        .ip_params = homeostatic_ip_params_default(),
        .meta_params = homeostatic_meta_params_default(),
        .global_stability_threshold = 0.9F,     /* 90% neurons stable */
        .update_interval_ms = 100.0F            /* Update every 100ms */
    };
    return config;
}

//=============================================================================
// Synaptic Scaling Functions
//=============================================================================

synaptic_scaling_state_t synaptic_scaling_state_init(float initial_rate) {
    /* WHAT: Initialize scaling state
     * WHY:  Factory method ensures valid initial state
     */
    synaptic_scaling_state_t state = {
        .average_rate = nimcp_clampf(initial_rate, MIN_RATE, MAX_RATE),
        .scaling_factor = 1.0F,
        .rate_integral = 0.0F,
        .spike_count = 0,
        .last_update_time = 0,
        .is_stable = false
    };
    return state;
}

void synaptic_scaling_update_rate(synaptic_scaling_state_t* state,
                                  bool spike_occurred,
                                  float dt,
                                  const synaptic_scaling_params_t* params) {
    /* WHAT: Update running average firing rate
     * WHY:  Rate estimate drives scaling decisions
     *
     * FORMULA: rate_avg += decay × (instantaneous - rate_avg)
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state || !params) return;
    if (dt <= 0.0F) return;

    /* Compute instantaneous rate (Hz) from spike */
    float instantaneous = spike_occurred ? (1000.0F / dt) : 0.0F;

    /* Exponential moving average */
    float decay = decay_factor(dt, params->rate_averaging_tau);
    state->average_rate += decay * (instantaneous - state->average_rate);

    /* Clamp to valid range */
    state->average_rate = nimcp_clampf(state->average_rate, MIN_RATE, MAX_RATE);

    /* Update spike count */
    if (spike_occurred) {
        state->spike_count++;
    }

    /* Check stability (within 20% of target) */
    float ratio = state->average_rate / params->target_rate;
    state->is_stable = (ratio >= 0.8F && ratio <= 1.2F);
}

float synaptic_scaling_compute_factor(const synaptic_scaling_state_t* state,
                                      const synaptic_scaling_params_t* params) {
    /* WHAT: Compute multiplicative scaling factor
     * WHY:  Factor scales synapses toward target rate
     *
     * FORMULA: factor = (target / actual)^α
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state || !params) return 1.0F;

    /* Compute ratio of target to actual rate */
    float ratio = safe_divide(params->target_rate, state->average_rate);

    /* Apply scaling exponent */
    float factor = powf(ratio, params->scaling_exponent);

    /* Clamp to valid range */
    return nimcp_clampf(factor, params->min_scaling_factor, params->max_scaling_factor);
}

void synaptic_scaling_apply(float* weights,
                            uint32_t num_weights,
                            float scaling_factor) {
    /* WHAT: Apply multiplicative scaling to all weights
     * WHY:  Implement global synaptic homeostasis
     *
     * FORMULA: w_new = clamp(w_old × factor, w_min, w_max)
     *
     * COMPLEXITY: O(n)
     * PERFORMANCE: SIMD-friendly loop
     */

    /* Guard: Validate inputs */
    if (!weights || num_weights == 0) return;
    if (scaling_factor <= 0.0F) return;

    /* Apply scaling to each weight */
    for (uint32_t i = 0; i < num_weights; i++) {
        float scaled = weights[i] * scaling_factor;
        weights[i] = nimcp_clampf(scaled, HOMEOSTATIC_MIN_WEIGHT, HOMEOSTATIC_MAX_WEIGHT);
    }
}

void synaptic_scaling_apply_soft_bounds(float* weights,
                                        uint32_t num_weights,
                                        float scaling_factor,
                                        float soft_bound_strength) {
    /* WHAT: Apply scaling with soft bounds near weight limits
     * WHY:  Prevent weight saturation at bounds
     *
     * FORMULA: effective_factor = factor × soft_bound_factor(w)
     *          Weights near 0 or 1 scale less aggressively
     *
     * COMPLEXITY: O(n)
     */

    /* Guard: Validate inputs */
    if (!weights || num_weights == 0) return;
    if (scaling_factor <= 0.0F) return;

    /* Clamp strength */
    soft_bound_strength = nimcp_clampf(soft_bound_strength, 0.0F, 1.0F);

    /* Apply scaling with soft bounds */
    for (uint32_t i = 0; i < num_weights; i++) {
        float w = weights[i];
        float sb = soft_bound_factor(w, soft_bound_strength);
        float effective_factor = 1.0F + sb * (scaling_factor - 1.0F);
        float scaled = w * effective_factor;
        weights[i] = nimcp_clampf(scaled, HOMEOSTATIC_MIN_WEIGHT, HOMEOSTATIC_MAX_WEIGHT);
    }
}

//=============================================================================
// Intrinsic Plasticity Functions
//=============================================================================

intrinsic_plasticity_state_t intrinsic_plasticity_state_init(float initial_threshold,
                                                              float initial_gain) {
    /* WHAT: Initialize IP state
     * WHY:  Factory method ensures valid initial state
     */
    intrinsic_plasticity_state_t state = {
        .threshold = initial_threshold,
        .gain = nimcp_clampf(initial_gain, 0.1F, 10.0F),
        .average_rate = 0.0F,
        .average_input = 0.0F,
        .last_update_time = 0,
        .is_stable = false
    };
    return state;
}

void intrinsic_plasticity_update_threshold(intrinsic_plasticity_state_t* state,
                                           float current_rate,
                                           float dt,
                                           const intrinsic_plasticity_params_t* params) {
    /* WHAT: Adapt firing threshold based on activity
     * WHY:  Maintain target rate via threshold adjustment
     *
     * FORMULA: dθ/dt = η × (rate - target) / τ
     *          Firing too much → increase threshold
     *          Firing too little → decrease threshold
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state || !params) return;
    if (dt <= 0.0F) return;

    /* Compute rate error (positive = too active) */
    float rate_error = current_rate - params->target_rate;

    /* Compute threshold change */
    float decay = decay_factor(dt, params->threshold_tau);
    float delta_theta = params->learning_rate * rate_error * decay;

    /* Update threshold */
    state->threshold += delta_theta;
    state->threshold = nimcp_clampf(state->threshold, params->min_threshold, params->max_threshold);

    /* Update average rate */
    float rate_decay = decay_factor(dt, 1000.0F);  /* 1 sec averaging */
    state->average_rate += rate_decay * (current_rate - state->average_rate);

    /* Check stability */
    float ratio = state->average_rate / params->target_rate;
    state->is_stable = (ratio >= 0.8F && ratio <= 1.2F);
}

void intrinsic_plasticity_update_gain(intrinsic_plasticity_state_t* state,
                                      float input_mean,
                                      float input_variance,
                                      float dt,
                                      const intrinsic_plasticity_params_t* params) {
    /* WHAT: Adapt gain based on input statistics
     * WHY:  Maximize information transmission (infomax)
     *
     * FORMULA: dg/dt = η × (σ_target - σ_actual) × g / τ
     *          Low variance → increase gain
     *          High variance → decrease gain
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state || !params) return;
    if (dt <= 0.0F) return;

    /* Target variance (want unit variance output) */
    float target_variance = 1.0F;

    /* Compute variance error */
    float output_variance = state->gain * state->gain * (input_variance + EPSILON);
    float variance_error = target_variance - output_variance;

    /* Compute gain change */
    float decay = decay_factor(dt, params->gain_tau);
    float delta_gain = params->learning_rate * variance_error * state->gain * decay;

    /* Update gain */
    state->gain += delta_gain;
    state->gain = nimcp_clampf(state->gain, params->min_gain, params->max_gain);

    /* Update average input */
    float input_decay = decay_factor(dt, 1000.0F);
    state->average_input += input_decay * (input_mean - state->average_input);
}

float intrinsic_plasticity_apply(float input,
                                 const intrinsic_plasticity_state_t* state) {
    /* WHAT: Transform input through adapted threshold/gain
     * WHY:  Implement homeostatic input-output function
     *
     * FORMULA: output = gain × (input - threshold)
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state) return input;

    /* Apply threshold and gain */
    float shifted = input - state->threshold;
    float scaled = state->gain * shifted;

    return scaled;
}

//=============================================================================
// Metaplasticity Functions
//=============================================================================

metaplasticity_state_t metaplasticity_state_init(float initial_theta) {
    /* WHAT: Initialize metaplasticity state
     * WHY:  Factory method ensures valid initial state
     */
    metaplasticity_state_t state = {
        .theta = nimcp_clampf(initial_theta, 0.01F, 1.0F),
        .activity_squared_avg = initial_theta,
        .activity_avg = sqrtf(initial_theta),
        .plasticity_rate = 1.0F
    };
    return state;
}

void metaplasticity_update_theta(metaplasticity_state_t* state,
                                 float current_activity,
                                 float dt,
                                 const metaplasticity_params_t* params) {
    /* WHAT: Update sliding modification threshold
     * WHY:  BCM sliding threshold prevents runaway plasticity
     *
     * FORMULA: θ → <r^p> where p = theta_power (typically 2)
     *          dθ/dt = (r^p - θ) / τ
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state || !params) return;
    if (dt <= 0.0F) return;

    /* Compute activity to the power (typically squared) */
    float activity_power = powf(fabsf(current_activity), params->theta_power);

    /* Update running average of activity^p */
    float decay = decay_factor(dt, params->theta_tau);
    state->activity_squared_avg += decay * (activity_power - state->activity_squared_avg);

    /* Update theta toward activity average */
    state->theta = state->activity_squared_avg;
    state->theta = nimcp_clampf(state->theta, params->min_theta, params->max_theta);

    /* Update activity average */
    float act_decay = decay_factor(dt, params->activity_tau);
    state->activity_avg += act_decay * (fabsf(current_activity) - state->activity_avg);

    /* Update effective plasticity rate
     * Higher theta → harder to potentiate → lower effective rate
     */
    state->plasticity_rate = safe_divide(1.0F, 1.0F + state->theta);
}

float metaplasticity_get_effective_rate(const metaplasticity_state_t* state,
                                        float base_plasticity_rate) {
    /* WHAT: Get effective plasticity rate modulated by metaplasticity
     * WHY:  History-dependent modulation of learning
     *
     * FORMULA: Higher theta (more prior activity) reduces effective rate
     *          rate = base_rate × (1 - theta) × plasticity_rate
     *          This implements BCM-style sliding threshold modulation
     *
     * COMPLEXITY: O(1)
     */

    /* Guard: Validate inputs */
    if (!state) return base_plasticity_rate;

    /* Modulate by theta: higher theta -> lower effective rate */
    float theta_modulation = 1.0F - (0.8F * state->theta);  /* Range: 0.2 to 1.0 */
    return base_plasticity_rate * theta_modulation * state->plasticity_rate;
}

//=============================================================================
// Homeostatic Controller Functions
//=============================================================================

homeostatic_controller_t homeostatic_controller_create(
    const homeostatic_config_t* config,
    uint32_t num_neurons) {
    /* WHAT: Create homeostatic controller
     * WHY:  Unified management of homeostatic mechanisms
     *
     * COMPLEXITY: O(n) for allocation
     */

    /* Guard: Validate inputs */
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_controller_create: config is NULL");
        return NULL;
    }
    if (num_neurons == 0) {
        NIMCP_LOGGING_ERROR("Zero neurons specified");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "homeostatic_controller_create: num_neurons is zero");
        return NULL;
    }

    /* Allocate controller */
    homeostatic_controller_t ctrl = nimcp_calloc(1, sizeof(struct homeostatic_controller_struct));
    if (!ctrl) {
        NIMCP_LOGGING_ERROR("Failed to allocate homeostatic controller");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_controller_create: controller allocation failed");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctrl->config, config, sizeof(homeostatic_config_t));
    ctrl->num_neurons = num_neurons;

    /* Allocate scaling states */
    if (config->enable_synaptic_scaling) {
        ctrl->scaling_states = nimcp_calloc(num_neurons, sizeof(synaptic_scaling_state_t));
        if (!ctrl->scaling_states) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_controller_create: scaling_states allocation failed");
            homeostatic_controller_destroy(ctrl);
            return NULL;
        }
        for (uint32_t i = 0; i < num_neurons; i++) {
            ctrl->scaling_states[i] = synaptic_scaling_state_init(config->scaling_params.target_rate);
        }
    }

    /* Allocate IP states */
    if (config->enable_intrinsic_plasticity) {
        ctrl->ip_states = nimcp_calloc(num_neurons, sizeof(intrinsic_plasticity_state_t));
        if (!ctrl->ip_states) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_controller_create: ip_states allocation failed");
            homeostatic_controller_destroy(ctrl);
            return NULL;
        }
        for (uint32_t i = 0; i < num_neurons; i++) {
            ctrl->ip_states[i] = intrinsic_plasticity_state_init(0.0F, 1.0F);
        }
    }

    /* Allocate metaplasticity states */
    if (config->enable_metaplasticity) {
        ctrl->meta_states = nimcp_calloc(num_neurons, sizeof(metaplasticity_state_t));
        if (!ctrl->meta_states) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeostatic_controller_create: meta_states allocation failed");
            homeostatic_controller_destroy(ctrl);
            return NULL;
        }
        for (uint32_t i = 0; i < num_neurons; i++) {
            ctrl->meta_states[i] = metaplasticity_state_init(0.1F);
        }
    }

    /* Initialize statistics */
    memset(&ctrl->stats, 0, sizeof(homeostatic_stats_t));

    /* Initialize sleep state to awake */
    ctrl->current_sleep_state = SLEEP_STATE_AWAKE;

    /* Initialize mutex for thread safety */
    if (nimcp_mutex_init(&ctrl->mutex, NULL) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_UNKNOWN, "homeostatic_controller_create: mutex init failed");
        homeostatic_controller_destroy(ctrl);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created homeostatic controller: neurons=%u, scaling=%d, ip=%d, meta=%d",
                       num_neurons,
                       config->enable_synaptic_scaling,
                       config->enable_intrinsic_plasticity,
                       config->enable_metaplasticity);

    return ctrl;
}

void homeostatic_controller_destroy(homeostatic_controller_t controller) {
    /* WHAT: Free controller resources
     * WHY:  Prevent memory leaks
     */

    /* Guard: Validate input */
    if (!controller) return;

    /* Destroy mutex */
    nimcp_mutex_destroy(&controller->mutex);

    nimcp_free(controller->scaling_states);
    nimcp_free(controller->ip_states);
    nimcp_free(controller->meta_states);
    nimcp_free(controller);
}

void homeostatic_controller_update(homeostatic_controller_t controller,
                                   const float* firing_rates,
                                   float* weights,
                                   uint32_t num_synapses_per_neuron,
                                   float dt) {
    /* WHAT: Run one homeostatic update cycle
     * WHY:  Periodic maintenance of stability
     *
     * COMPLEXITY: O(n × m) where n = neurons, m = synapses per neuron
     */

    /* Guard: Validate inputs */
    if (!controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_controller_update: controller is NULL");
        return;
    }
    if (!firing_rates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_controller_update: firing_rates is NULL");
        return;
    }

    /* Thread-safe update with mutex lock */
    nimcp_mutex_lock(&controller->mutex);

    /* Accumulate time */
    controller->time_since_update += dt;

    /* Check if update interval reached */
    if (controller->time_since_update < controller->config.update_interval_ms) {
        nimcp_mutex_unlock(&controller->mutex);
        return;
    }
    controller->time_since_update = 0.0F;
    controller->stats.total_updates++;

    /* Phase 8: Heartbeat at start of update cycle */
    homeostatic_heartbeat("homeostatic_update", 0.0f);

    /* Counters for statistics */
    uint32_t above = 0, below = 0, stable = 0;
    float sum_rate = 0.0F, sum_rate_sq = 0.0F;
    float sum_factor = 0.0F;

    /* Update each neuron */
    for (uint32_t n = 0; n < controller->num_neurons; n++) {
        float rate = firing_rates[n];
        sum_rate += rate;
        sum_rate_sq += rate * rate;

        /* Synaptic scaling */
        if (controller->config.enable_synaptic_scaling && controller->scaling_states) {
            synaptic_scaling_state_t* ss = &controller->scaling_states[n];

            /* Update rate estimate using actual firing rate directly */
            float decay = 1.0F - expf(-dt / controller->config.scaling_params.rate_averaging_tau);
            ss->average_rate += decay * (rate - ss->average_rate);
            ss->average_rate = fmaxf(0.0F, fminf(ss->average_rate, 1000.0F));

            /* Apply sleep state modulation to target rate (Tononi's SHY) */
            float sleep_target_modifier = homeostatic_sleep_target_for_state(controller->current_sleep_state);
            float modulated_target = controller->config.scaling_params.target_rate * sleep_target_modifier;

            /* Check stability (within 20% of modulated target) */
            float ratio = (modulated_target > 1e-8f)
                ? ss->average_rate / modulated_target : 0.0f;
            ss->is_stable = (ratio >= 0.8F && ratio <= 1.2F);

            /* Compute scaling factor with modulated target */
            float base_factor = safe_divide(modulated_target, ss->average_rate);
            float factor = powf(base_factor, controller->config.scaling_params.scaling_exponent);
            factor = nimcp_clampf(factor, controller->config.scaling_params.min_scaling_factor,
                           controller->config.scaling_params.max_scaling_factor);

            /* Apply sleep state modulation (Tononi's SHY) */
            float sleep_scaling_rate = homeostatic_sleep_scaling_for_state(controller->current_sleep_state);
            float modulated_factor = 1.0F + (factor - 1.0F) * sleep_scaling_rate;

            ss->scaling_factor = modulated_factor;
            sum_factor += modulated_factor;

            /* Apply scaling to this neuron's weights */
            if (weights && num_synapses_per_neuron > 0) {
                float* neuron_weights = weights + (n * num_synapses_per_neuron);
                synaptic_scaling_apply_soft_bounds(neuron_weights, num_synapses_per_neuron,
                                                   modulated_factor, 0.3F);
                controller->stats.scaling_events++;
            }

            /* Track stability */
            if (ss->is_stable) {
                stable++;
            } else if (ss->average_rate > controller->config.scaling_params.target_rate) {
                above++;
            } else {
                below++;
            }
        }

        /* Intrinsic plasticity */
        if (controller->config.enable_intrinsic_plasticity && controller->ip_states) {
            intrinsic_plasticity_state_t* ip = &controller->ip_states[n];
            intrinsic_plasticity_update_threshold(ip, rate, dt, &controller->config.ip_params);
            controller->stats.threshold_adjustments++;
        }

        /* Metaplasticity */
        if (controller->config.enable_metaplasticity && controller->meta_states) {
            metaplasticity_state_t* mp = &controller->meta_states[n];
            metaplasticity_update_theta(mp, rate, dt, &controller->config.meta_params);
        }

        /* Phase 8: Periodic heartbeat for large neuron counts */
        if ((n & 0x3F) == 0 && controller->num_neurons > 64) {
            homeostatic_heartbeat("homeostatic_update",
                                  (float)(n + 1) / (float)controller->num_neurons);
        }
    }

    /* Update statistics */
    if (controller->num_neurons == 0) {
        nimcp_mutex_unlock(&controller->mutex);
        return;
    }
    controller->stats.mean_firing_rate = sum_rate / controller->num_neurons;
    controller->stats.rate_variance = (sum_rate_sq / controller->num_neurons) -
                                      (controller->stats.mean_firing_rate * controller->stats.mean_firing_rate);
    controller->stats.mean_scaling_factor = sum_factor / controller->num_neurons;
    controller->stats.neurons_above_target = above;
    controller->stats.neurons_below_target = below;
    controller->stats.neurons_stable = stable;
    controller->stats.stability_score = (float)stable / controller->num_neurons;

    nimcp_mutex_unlock(&controller->mutex);
}

bool homeostatic_controller_get_stats(homeostatic_controller_t controller,
                                      homeostatic_stats_t* stats) {
    /* WHAT: Retrieve homeostatic statistics
     * WHY:  Monitor stability and convergence
     */

    /* Guard: Validate inputs */
    if (!controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_controller_get_stats: controller is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_controller_get_stats: stats is NULL");
        return false;
    }

    memcpy(stats, &controller->stats, sizeof(homeostatic_stats_t));
    return true;
}

bool homeostatic_controller_is_stable(homeostatic_controller_t controller) {
    /* WHAT: Check if system is stable
     * WHY:  Know when learning can proceed normally
     */

    /* Guard: Validate input */
    if (!controller) {
        return false;
    }

    return controller->stats.stability_score >= controller->config.global_stability_threshold;
}

void homeostatic_controller_reset(homeostatic_controller_t controller) {
    /* WHAT: Reset controller to initial state
     * WHY:  Start fresh after major perturbation
     */

    /* Guard: Validate input */
    if (!controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_controller_reset: controller is NULL");
        return;
    }

    /* Reset scaling states */
    if (controller->scaling_states) {
        for (uint32_t i = 0; i < controller->num_neurons; i++) {
            controller->scaling_states[i] = synaptic_scaling_state_init(
                controller->config.scaling_params.target_rate);
        }
    }

    /* Reset IP states */
    if (controller->ip_states) {
        for (uint32_t i = 0; i < controller->num_neurons; i++) {
            controller->ip_states[i] = intrinsic_plasticity_state_init(0.0F, 1.0F);
        }
    }

    /* Reset metaplasticity states */
    if (controller->meta_states) {
        for (uint32_t i = 0; i < controller->num_neurons; i++) {
            controller->meta_states[i] = metaplasticity_state_init(0.1F);
        }
    }

    /* Reset statistics */
    memset(&controller->stats, 0, sizeof(homeostatic_stats_t));
    controller->time_since_update = 0.0F;
}

bool homeostatic_controller_set_sleep_state(homeostatic_controller_t controller,
                                             sleep_state_t sleep_state) {
    /* WHAT: Set current sleep state for homeostatic modulation
     * WHY:  Sleep state controls when and how much synaptic scaling occurs
     * HOW:  Store state, used in next update to apply sleep-based modulation
     *
     * BIOLOGICAL: Tononi's Synaptic Homeostasis Hypothesis - sleep is primary time for scaling
     */

    /* Guard: Validate input */
    if (!controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_controller_set_sleep_state: controller is NULL");
        return false;
    }

    controller->current_sleep_state = sleep_state;
    return true;
}

sleep_state_t homeostatic_controller_get_sleep_state(homeostatic_controller_t controller) {
    /* WHAT: Get current sleep state
     * WHY:  Query what modulation is being applied
     */

    /* Guard: Validate input */
    if (!controller) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_controller_get_sleep_state: controller is NULL");
        return SLEEP_STATE_AWAKE;
    }

    return controller->current_sleep_state;
}

//=============================================================================
// KG Reader Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Allow homeostatic module to introspect its own capabilities and connections
 * WHY:  Self-awareness enables adaptive behavior and system introspection
 * HOW:  Query KG for Homeostatic_Module entity and its relations
 *
 * COMPLEXITY: O(n) where n = number of observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 if not found or error
 */
int homeostatic_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeostatic_query_self_knowledge: kg is NULL");
        return 0;
    }

    /* Query our own entity from the knowledge graph */
    const kg_entity_t* self = kg_reader_get_entity(kg, "Homeostatic_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            NIMCP_LOGGING_DEBUG("Homeostatic self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Homeostatic_Module");
    if (connections) {
        NIMCP_LOGGING_DEBUG("Homeostatic has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Homeostatic_Module");
    if (incoming) {
        NIMCP_LOGGING_DEBUG("Homeostatic has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
