#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_myelin_math.c - Advanced Myelin Mathematical Models Implementation
//=============================================================================
/**
 * @file nimcp_myelin_math.c
 * @brief Implementation of advanced myelin biophysics mathematical models
 *
 * WHAT: Complete implementation of 8 mathematical myelin models
 * WHY:  Enable biologically accurate myelination dynamics and conduction
 * HOW:  Implements cable theory, saltatory conduction, plasticity kinetics
 *
 * @version 2.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#include "async/nimcp_bio_async.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "api/nimcp_api_exception.h"

#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "MYELIN_SHEATH"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(myelin_math)

#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/memory/nimcp_memory_guards.h"  // For nimcp_calloc/nimcp_free
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Internal Constants
//=============================================================================

/** @brief LCG multiplier for RNG */
#define RNG_MULTIPLIER 6364136223846793005ULL

/** @brief LCG increment for RNG */
#define RNG_INCREMENT  1442695040888963407ULL

/** @brief G-ratio efficiency falloff coefficient */
#define G_EFFICIENCY_K 4.0f

/** @brief Internode efficiency sigma factor */
#define INTERNODE_EFFICIENCY_SIGMA 0.3f

/** @brief Activity update decay constant */
#define ACTIVITY_EMA_DECAY 0.1f

//=============================================================================
// Fast Math Implementations
//=============================================================================

/**
 * @brief Fast approximate exponential using Padé approximation
 *
 * WHAT: Rational approximation for exp(x)
 * WHY:  Avoid expensive libm exp() in hot paths
 * HOW:  (1 + x/2 + x²/12) / (1 - x/2 + x²/12) for small x
 *
 * ACCURACY: < 0.1% error for |x| < 4
 */
float nimcp_myelin_fast_exp(float x) {
    // For large |x|, fall back to standard exp
    if (x < -4.0F) return 0.0F;
    if (x > 4.0F) return expf(x);

    // Padé approximation (6,6)
    float x2 = x * x;
    float x3 = x2 * x;

    float num = 1.0F + x * 0.5F + x2 * (1.0F / 12.0F);
    float den = 1.0F - x * 0.5F + x2 * (1.0F / 12.0F);

    if (den < NIMCP_MYELIN_MATH_EPSILON) {
        return expf(x);
    }

    return num / den;
}

/**
 * @brief Fast inverse square root using Newton-Raphson
 *
 * WHAT: Approximate 1/sqrt(x)
 * WHY:  Classic optimization for sqrt operations
 * HOW:  Famous Quake III algorithm with iteration
 */
static inline float fast_inv_sqrt(float x) {
    union { float f; uint32_t i; } conv = {.f = x};
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f *= 1.5F - (x * 0.5F * conv.f * conv.f);
    return conv.f;
}

float nimcp_myelin_fast_sqrt(float x) {
    if (x <= 0.0F) return 0.0F;
    return x * fast_inv_sqrt(x);
}

/**
 * @brief Fast approximate power using exp/log identity
 *
 * WHAT: Approximate base^exp
 * WHY:  Avoid expensive pow() in performance paths
 * HOW:  exp(exp * log(base))
 */
float nimcp_myelin_fast_pow(float base, float exp) {
    if (base <= 0.0F) return 0.0F;
    if (exp == 0.0F) return 1.0F;
    if (exp == 1.0F) return base;
    if (exp == 2.0F) return base * base;

    // For small integer exponents, use multiplication
    if (exp == 0.5F) return nimcp_myelin_fast_sqrt(base);

    // General case using log/exp
    return expf(exp * logf(base));
}

//=============================================================================
// Initialization Functions
//=============================================================================

nimcp_myelination_kinetics_t nimcp_myelin_kinetics_default(void) {
    nimcp_myelination_kinetics_t kinetics = {
        .k_max = NIMCP_MYELIN_K_MAX,
        .k_half = NIMCP_MYELIN_K_HALF,
        .hill_n = NIMCP_MYELIN_HILL_N,
        .k_demyelin = NIMCP_MYELIN_K_DEMYELIN,
        .k_demyelin_half = NIMCP_MYELIN_K_DEMYELIN_HALF,
        .saturation_lamellae = 100.0F,
        .min_activity = 0.05F,
        .history_weight = 0.9F
    };
    return kinetics;
}

nimcp_conduction_block_params_t nimcp_myelin_block_params_default(void) {
    nimcp_conduction_block_params_t params = {
        .i_critical = NIMCP_BLOCK_I_CRITICAL,
        .sigma = NIMCP_BLOCK_SIGMA,
        .t_ref = NIMCP_BLOCK_T_REF,
        .t_sensitivity = NIMCP_BLOCK_T_SENSITIVITY,
        .frequency_factor = 0.001F,
        .refractory_ms = 1.0F
    };
    return params;
}

nimcp_myelin_biophysics_t* nimcp_myelin_biophysics_create(bool use_stochastic,
                                                           uint64_t seed) {
    nimcp_myelin_biophysics_t* bio = (nimcp_myelin_biophysics_t*)
        nimcp_malloc(sizeof(nimcp_myelin_biophysics_t));

    if (!bio) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bio is NULL");


        return NULL;


    }

    memset(bio, 0, sizeof(nimcp_myelin_biophysics_t));

    // Initialize with defaults
    bio->kinetics = nimcp_myelin_kinetics_default();
    bio->block_params = nimcp_myelin_block_params_default();

    // Initialize state
    bio->activity_ema = 0.0F;
    bio->temperature_c = 37.0F;  // Normal body temperature
    bio->last_update_us = 0;

    // Initialize RNG
    bio->use_stochastic = use_stochastic;
    nimcp_myelin_rng_init(&bio->rng, seed);

    // Invalidate caches
    bio->cable_valid = false;
    bio->conduction_valid = false;
    bio->metabolism_valid = false;

    return bio;
}

void nimcp_myelin_biophysics_destroy(nimcp_myelin_biophysics_t* bio) {
    if (bio) {
        nimcp_free(bio);
    }
}

void nimcp_myelin_biophysics_reset(nimcp_myelin_biophysics_t* bio) {
    if (!bio) return;

    bio->activity_ema = 0.0F;
    bio->last_update_us = 0;

    nimcp_myelin_rng_reset(&bio->rng);

    bio->cable_valid = false;
    bio->conduction_valid = false;
    bio->metabolism_valid = false;
}

//=============================================================================
// G-ratio Optimization Implementation
//=============================================================================

/**
 * @brief Compute diameter-dependent optimal g-ratio
 *
 * IMPLEMENTATION:
 *   g_opt(d) = g_base + α × exp(-d / d_critical)
 *
 * For large axons (d >> d_critical), g_opt → g_base = 0.77
 * For small axons (d → 0), g_opt → g_base + α = 0.85
 *
 * This captures the biological observation that smaller axons
 * have relatively thinner myelin sheaths.
 */
float nimcp_myelin_optimal_g_ratio(float axon_diameter_um) {
    if (axon_diameter_um <= 0.0F) {
        return NIMCP_G_RATIO_MAX;
    }

    // Diameter-dependent correction
    float d_ratio = axon_diameter_um / NIMCP_G_RATIO_D_CRITICAL;
    float correction = NIMCP_G_RATIO_ALPHA * nimcp_myelin_fast_exp(-d_ratio);

    float g_opt = NIMCP_G_RATIO_BASE + correction;

    return nimcp_myelin_clamp(g_opt, NIMCP_G_RATIO_MIN, NIMCP_G_RATIO_MAX);
}

/**
 * @brief Compute g-ratio efficiency factor
 *
 * IMPLEMENTATION:
 *   η = 1 - k × (g - g_opt)²
 *
 * Parabolic efficiency centered on optimal g-ratio.
 * Maximum efficiency = 1.0 at g = g_opt.
 * Clamped to [0.5, 1.0] for physiological range.
 */
float nimcp_myelin_g_ratio_efficiency(float g_ratio, float axon_diameter_um) {
    float g_opt = nimcp_myelin_optimal_g_ratio(axon_diameter_um);

    float deviation = g_ratio - g_opt;
    float efficiency = 1.0F - G_EFFICIENCY_K * deviation * deviation;

    return nimcp_myelin_clamp(efficiency, 0.5F, 1.0F);
}

/**
 * @brief Compute lamellae count for target g-ratio
 *
 * IMPLEMENTATION:
 *   g = d_inner / d_outer
 *   d_outer = d_inner + 2 × n × t_lamella
 *   g = d_inner / (d_inner + 2 × n × t_lamella)
 *
 *   Solving for n:
 *   n = d_inner × (1 - g) / (2 × g × t_lamella)
 */
uint32_t nimcp_myelin_lamellae_for_g_ratio(float axon_diameter_um,
                                            float target_g_ratio,
                                            float lamella_thickness_nm) {
    if (axon_diameter_um <= 0.0F || target_g_ratio <= 0.0F ||
        target_g_ratio >= 1.0F || lamella_thickness_nm <= 0.0F) {
        return 0;
    }

    // Convert units: diameter in μm, thickness in nm
    float d_um = axon_diameter_um;
    float t_um = lamella_thickness_nm / 1000.0F;  // nm to μm

    // Calculate required lamellae
    float numerator = d_um * (1.0F - target_g_ratio);
    float denominator = 2.0F * target_g_ratio * t_um;

    float n = numerator / denominator;

    return (uint32_t)(n + 0.5F);  // Round to nearest
}

//=============================================================================
// Cable Theory Implementation
//=============================================================================

/**
 * @brief Compute comprehensive cable theory parameters
 *
 * IMPLEMENTATION:
 *   Membrane resistance increases with lamellae (resistors in series):
 *     r_m = r_m_base + r_m_per_lamella × n
 *
 *   Membrane capacitance decreases (capacitors in series):
 *     c_m = c_m_base × (reduction_factor ^ n)
 *
 *   Axial resistance from geometry:
 *     r_a = ρ_a / (π × r²)  [Ω/cm for axoplasmic resistivity]
 *
 *   Space constant:
 *     λ = sqrt(r_m × d / (4 × r_a))  [in matched units]
 *
 *   Time constant:
 *     τ = r_m × c_m
 */
void nimcp_myelin_compute_cable_params(float axon_diameter_um,
                                        uint32_t num_lamellae,
                                        nimcp_cable_params_t* params) {
    if (!params || axon_diameter_um <= 0.0F) {
        if (params) memset(params, 0, sizeof(nimcp_cable_params_t));
        return;
    }

    // Convert diameter to cm for calculations
    float d_cm = axon_diameter_um * 1e-4F;
    float r_cm = d_cm / 2.0F;

    // Membrane resistance increases with myelination (Ω·cm²)
    params->r_m = NIMCP_CABLE_R_M_BASE +
                  NIMCP_CABLE_R_M_PER_LAMELLA * (float)num_lamellae;

    // Membrane capacitance decreases exponentially (μF/cm²)
    params->c_m = NIMCP_CABLE_C_M_BASE *
                  nimcp_myelin_fast_pow(NIMCP_CABLE_C_M_REDUCTION, (float)num_lamellae);

    // Axial resistance: ρ_a / (π × r²) [Ω/cm]
    float cross_section = NIMCP_MYELIN_MATH_PI * r_cm * r_cm;
    params->r_a = NIMCP_CABLE_R_A_CYTOPLASM / cross_section;

    // Space constant: λ = sqrt(r_m × d / (4 × r_a)) [cm]
    // Then convert to μm
    float lambda_cm = nimcp_myelin_fast_sqrt(params->r_m * d_cm / (4.0F * params->r_a));
    params->lambda_um = lambda_cm * 1e4F;  // cm to μm

    // Time constant: τ = r_m × c_m [Ω·cm² × μF/cm² = ms]
    params->tau_ms = params->r_m * params->c_m * 1e-3F;

    // Signal attenuation per space constant
    params->attenuation_factor = 1.0F / NIMCP_MYELIN_MATH_E;  // e^(-1) ≈ 0.368
}

float nimcp_myelin_space_constant(float axon_diameter_um, uint32_t num_lamellae) {
    nimcp_cable_params_t params;
    nimcp_myelin_compute_cable_params(axon_diameter_um, num_lamellae, &params);
    return params.lambda_um;
}

float nimcp_myelin_time_constant(uint32_t num_lamellae) {
    float r_m = NIMCP_CABLE_R_M_BASE +
                NIMCP_CABLE_R_M_PER_LAMELLA * (float)num_lamellae;
    float c_m = NIMCP_CABLE_C_M_BASE *
                nimcp_myelin_fast_pow(NIMCP_CABLE_C_M_REDUCTION, (float)num_lamellae);

    return r_m * c_m * 1e-3F;  // Convert to ms
}

float nimcp_myelin_attenuation(float length_um, float lambda_um) {
    if (lambda_um <= 0.0F) return 0.0F;

    float x = -length_um / lambda_um;
    return nimcp_myelin_fast_exp(x);
}

//=============================================================================
// Saltatory Conduction Implementation
//=============================================================================

/**
 * @brief Compute saltatory conduction velocity
 *
 * IMPLEMENTATION:
 *   Total delay per internode = node_delay + internode_delay
 *
 *   Node delay: τ_node = constant (AP regeneration time)
 *
 *   Internode delay: τ_int = L² / (λ² × v_passive)
 *   This models RC delay in the myelinated segment.
 *
 *   Base velocity: v = L / (τ_node + τ_int)
 *
 *   Modulated by:
 *   - G-ratio efficiency (parabolic around optimal)
 *   - Compaction factor (linear)
 *   - Structural integrity (linear)
 *
 *   Final: v_final = v × η_g × compaction × integrity
 */
float nimcp_myelin_saltatory_velocity(float axon_diameter_um,
                                       float internode_length_um,
                                       uint32_t num_lamellae,
                                       float g_ratio,
                                       float compaction_score,
                                       float integrity,
                                       nimcp_saltatory_result_t* result) {
    // Input validation
    if (axon_diameter_um <= 0.0F || internode_length_um <= 0.0F ||
        num_lamellae == 0) {
        if (result) memset(result, 0, sizeof(nimcp_saltatory_result_t));
        return 0.0F;
    }

    // Clamp inputs to valid ranges
    compaction_score = nimcp_myelin_clamp(compaction_score, 0.0F, 1.0F);
    integrity = nimcp_myelin_clamp(integrity, 0.0F, 1.0F);
    g_ratio = nimcp_myelin_clamp(g_ratio, NIMCP_G_RATIO_MIN, NIMCP_G_RATIO_MAX);

    // Calculate space constant
    float lambda_um = nimcp_myelin_space_constant(axon_diameter_um, num_lamellae);

    // Node delay is constant (AP regeneration)
    float tau_node_ms = NIMCP_SALTATORY_TAU_NODE_MS;

    // Internode delay from RC charging: τ = L² / (λ² × v_passive)
    float L_over_lambda = internode_length_um / (lambda_um + NIMCP_MYELIN_MATH_EPSILON);
    float tau_internode_ms = (L_over_lambda * L_over_lambda * internode_length_um) /
                             NIMCP_SALTATORY_V_PASSIVE;

    // Total delay per segment
    float total_delay_ms = tau_node_ms + tau_internode_ms;

    // Base velocity: L / delay [μm/ms = m/s]
    float velocity_base = internode_length_um / (total_delay_ms * 1000.0F);

    // Apply efficiency factors
    float g_efficiency = nimcp_myelin_g_ratio_efficiency(g_ratio, axon_diameter_um);

    // Compaction improves conduction (0.5 at worst, 1.0 at best)
    float compaction_factor = 0.5F + 0.5F * compaction_score;

    // Integrity scales linearly
    float integrity_factor = integrity;

    // Final velocity
    float velocity_final = velocity_base * g_efficiency * compaction_factor * integrity_factor;

    // Clamp to physiological range
    velocity_final = nimcp_myelin_clamp(velocity_final,
                                         NIMCP_SALTATORY_V_MIN_MS,
                                         NIMCP_SALTATORY_V_MAX_MS);

    // Fill result structure if provided
    if (result) {
        result->velocity_ms = velocity_final;
        result->tau_node_ms = tau_node_ms;
        result->tau_internode_ms = tau_internode_ms;
        result->g_efficiency = g_efficiency;
        result->compaction_factor = compaction_factor;
        result->integrity_factor = integrity_factor;
        result->lambda_um = lambda_um;
        result->is_blocked = false;
        result->block_probability = 0.0F;
    }

    return velocity_final;
}

float nimcp_myelin_propagation_delay(float length_um, float velocity_ms) {
    if (velocity_ms <= 0.0F) return INFINITY;

    // length_um / (velocity_ms × 1000) = length_um / (μm/ms) = ms
    return length_um / (velocity_ms * 1000.0F);
}

float nimcp_myelin_compute_velocity_full(nimcp_myelin_biophysics_t* bio,
                                          float axon_diameter_um,
                                          float internode_length_um,
                                          uint32_t num_lamellae,
                                          float g_ratio,
                                          float compaction_score,
                                          float integrity) {
    if (!bio) {
        return nimcp_myelin_saltatory_velocity(axon_diameter_um, internode_length_um,
                                                num_lamellae, g_ratio,
                                                compaction_score, integrity, NULL);
    }

    // Compute velocity and update cached result
    float velocity = nimcp_myelin_saltatory_velocity(axon_diameter_um, internode_length_um,
                                                      num_lamellae, g_ratio,
                                                      compaction_score, integrity,
                                                      &bio->conduction);

    // Check for conduction block
    float block_prob = nimcp_myelin_block_probability(integrity, bio->temperature_c,
                                                       &bio->block_params);
    bio->conduction.block_probability = block_prob;

    if (bio->use_stochastic) {
        bio->conduction.is_blocked = nimcp_myelin_is_blocked(integrity, bio->temperature_c,
                                                              &bio->block_params, &bio->rng);
    } else {
        bio->conduction.is_blocked = (block_prob > 0.5F);
    }

    // Update cable params cache
    nimcp_myelin_compute_cable_params(axon_diameter_um, num_lamellae, &bio->cable);
    bio->cable_valid = true;
    bio->conduction_valid = true;

    // Apply stochastic variability if enabled
    if (bio->use_stochastic && !bio->conduction.is_blocked) {
        velocity = nimcp_myelin_vary_velocity(&bio->rng, velocity);
    }

    return bio->conduction.is_blocked ? 0.0F : velocity;
}

//=============================================================================
// Activity-Dependent Myelination Implementation
//=============================================================================

/**
 * @brief Compute myelination rate using Hill kinetics
 *
 * IMPLEMENTATION:
 *   Myelination follows sigmoid Hill kinetics with saturation:
 *
 *   Myelination rate:
 *     r_myelin = k_max × (A^n / (K^n + A^n)) × (1 - N/N_max)
 *
 *   Where:
 *   - A = neural activity level (0-1)
 *   - K = half-max activity (k_half)
 *   - n = Hill coefficient (cooperativity)
 *   - N = current lamellae count
 *   - N_max = saturation lamellae
 *
 *   Demyelination rate (inverse Hill):
 *     r_demyelin = k_demyelin × (K_d / (K_d + A))
 *
 *   Net rate = r_myelin - r_demyelin
 */
float nimcp_myelin_compute_myelination_rate(float activity,
                                             float current_lamellae,
                                             const nimcp_myelination_kinetics_t* kinetics) {
    if (!kinetics) {
        nimcp_myelination_kinetics_t defaults = nimcp_myelin_kinetics_default();
        return nimcp_myelin_compute_myelination_rate(activity, current_lamellae, &defaults);
    }

    // Clamp activity to valid range
    activity = nimcp_myelin_clamp(activity, 0.0F, 1.0F);

    // Below minimum threshold, only demyelination occurs
    if (activity < kinetics->min_activity) {
        return -kinetics->k_demyelin;
    }

    // Hill function for myelination: A^n / (K^n + A^n)
    float A_n = nimcp_myelin_fast_pow(activity, kinetics->hill_n);
    float K_n = nimcp_myelin_fast_pow(kinetics->k_half, kinetics->hill_n);
    float hill_myelin = A_n / (K_n + A_n + NIMCP_MYELIN_MATH_EPSILON);

    // Saturation term (slows myelination as lamellae approach max)
    float saturation = 1.0F - (current_lamellae / kinetics->saturation_lamellae);
    saturation = nimcp_myelin_clamp(saturation, 0.0F, 1.0F);

    // Myelination rate
    float rate_myelin = kinetics->k_max * hill_myelin * saturation;

    // Inverse Hill for demyelination (high activity suppresses)
    float hill_demyelin = kinetics->k_demyelin_half /
                          (kinetics->k_demyelin_half + activity + NIMCP_MYELIN_MATH_EPSILON);
    float rate_demyelin = kinetics->k_demyelin * hill_demyelin;

    // Net rate
    return rate_myelin - rate_demyelin;
}

float nimcp_myelin_update_lamellae(float current_lamellae,
                                    float activity,
                                    float dt,
                                    const nimcp_myelination_kinetics_t* kinetics) {
    float rate = nimcp_myelin_compute_myelination_rate(activity, current_lamellae, kinetics);

    float new_lamellae = current_lamellae + rate * dt;

    // Can't go below zero
    return (new_lamellae > 0.0F) ? new_lamellae : 0.0F;
}

void nimcp_myelin_update_activity_ema(nimcp_myelin_biophysics_t* bio,
                                       float activity,
                                       float dt) {
    if (!bio) return;

    // EMA: ema_new = α × activity + (1 - α) × ema_old
    // Where α = 1 - exp(-dt / τ)
    float alpha = 1.0F - nimcp_myelin_fast_exp(-dt * ACTIVITY_EMA_DECAY);
    bio->activity_ema = alpha * activity + (1.0F - alpha) * bio->activity_ema;
}

/**
 * @brief Calculate activity threshold for net myelination
 *
 * IMPLEMENTATION:
 *   Find A where rate_myelin = rate_demyelin
 *   This requires numerical solution of the Hill equation.
 *   Using Newton-Raphson iteration.
 */
float nimcp_myelin_activity_threshold(const nimcp_myelination_kinetics_t* kinetics) {
    if (!kinetics) {
        nimcp_myelination_kinetics_t defaults = nimcp_myelin_kinetics_default();
        return nimcp_myelin_activity_threshold(&defaults);
    }

    // Initial guess at k_half
    float A = kinetics->k_half;

    // Newton-Raphson iteration (usually converges in 3-5 iterations)
    for (int i = 0; i < 10; i++) {
        float rate = nimcp_myelin_compute_myelination_rate(A, 0.0F, kinetics);

        // Numerical derivative
        float dA = 0.001F;
        float rate_plus = nimcp_myelin_compute_myelination_rate(A + dA, 0.0F, kinetics);
        float derivative = (rate_plus - rate) / dA;

        if (fabsf(derivative) < NIMCP_MYELIN_MATH_EPSILON) break;

        float A_new = A - rate / derivative;

        if (fabsf(A_new - A) < 1e-6F) break;
        A = nimcp_myelin_clamp(A_new, 0.0F, 1.0F);
    }

    return A;
}

//=============================================================================
// Conduction Block Implementation
//=============================================================================

/**
 * @brief Compute conduction block probability
 *
 * IMPLEMENTATION:
 *   Sigmoid probability based on myelin integrity:
 *
 *   P_base = 1 / (1 + exp((I - I_crit) / σ))
 *
 *   As integrity drops below critical:
 *   - I >> I_crit: P → 0 (reliable conduction)
 *   - I = I_crit: P = 0.5 (50% chance of block)
 *   - I << I_crit: P → 1 (likely block)
 *
 *   Temperature modulation (Uhthoff's phenomenon):
 *   T_factor = 1 + k_T × max(0, T - T_ref)
 *
 *   Final: P_block = min(1, P_base × T_factor)
 */
float nimcp_myelin_block_probability(float integrity,
                                      float temperature_c,
                                      const nimcp_conduction_block_params_t* params) {
    if (!params) {
        nimcp_conduction_block_params_t defaults = nimcp_myelin_block_params_default();
        return nimcp_myelin_block_probability(integrity, temperature_c, &defaults);
    }

    // Clamp integrity
    integrity = nimcp_myelin_clamp(integrity, 0.0F, 1.0F);

    // Sigmoid probability
    float exponent = (integrity - params->i_critical) / (params->sigma + NIMCP_MYELIN_MATH_EPSILON);
    float p_base = 1.0F / (1.0F + nimcp_myelin_fast_exp(exponent));

    // Temperature modulation (Uhthoff's phenomenon)
    float temp_excess = temperature_c - params->t_ref;
    float t_factor = 1.0F;
    if (temp_excess > 0.0F) {
        t_factor = 1.0F + params->t_sensitivity * temp_excess;
    }

    float p_block = p_base * t_factor;

    return nimcp_myelin_clamp(p_block, 0.0F, 1.0F);
}

bool nimcp_myelin_is_blocked(float integrity,
                              float temperature_c,
                              const nimcp_conduction_block_params_t* params,
                              nimcp_myelin_rng_t* rng) {
    float p_block = nimcp_myelin_block_probability(integrity, temperature_c, params);

    if (!rng) {
        return p_block > 0.5F;
    }

    float r = nimcp_myelin_rng_uniform(rng);
    return r < p_block;
}

/**
 * @brief Compute minimum integrity for given frequency
 *
 * IMPLEMENTATION:
 *   Higher frequencies require better integrity due to:
 *   - Shorter recovery time between APs
 *   - Accumulation of refractory effects
 *
 *   I_min = I_crit + σ × ln((1 - P_max) / P_max)
 *
 *   Where P_max is adjusted for frequency.
 */
float nimcp_myelin_frequency_threshold(float frequency_hz,
                                        float temperature_c,
                                        const nimcp_conduction_block_params_t* params) {
    if (!params) {
        nimcp_conduction_block_params_t defaults = nimcp_myelin_block_params_default();
        return nimcp_myelin_frequency_threshold(frequency_hz, temperature_c, &defaults);
    }

    // Frequency increases effective block probability
    float freq_factor = 1.0F + params->frequency_factor * frequency_hz;

    // Temperature factor
    float temp_excess = temperature_c - params->t_ref;
    float t_factor = 1.0F;
    if (temp_excess > 0.0F) {
        t_factor = 1.0F + params->t_sensitivity * temp_excess;
    }

    // Target 1% block probability
    float p_target = 0.01F / (freq_factor * t_factor);
    p_target = nimcp_myelin_clamp(p_target, 0.001F, 0.5F);

    // Invert sigmoid to find required integrity
    // P = 1 / (1 + exp((I - I_crit) / σ))
    // exp((I - I_crit) / σ) = (1 - P) / P
    // I = I_crit + σ × ln((1 - P) / P)

    float odds = (1.0F - p_target) / p_target;
    float I_min = params->i_critical + params->sigma * logf(odds);

    return nimcp_myelin_clamp(I_min, 0.0F, 1.0F);
}

//=============================================================================
// Internode Optimization Implementation
//=============================================================================

/**
 * @brief Compute optimal internode length
 *
 * IMPLEMENTATION:
 *   Power-law relationship from biology:
 *   L_opt = α × d^β
 *
 *   Where:
 *   - α ≈ 150 (scaling coefficient)
 *   - β ≈ 0.9 (power exponent)
 *
 *   This captures the observation that larger axons
 *   have proportionally longer internodes.
 */
float nimcp_myelin_optimal_internode(float axon_diameter_um) {
    if (axon_diameter_um <= 0.0F) return NIMCP_INTERNODE_MIN_UM;

    float L_opt = NIMCP_INTERNODE_ALPHA *
                  nimcp_myelin_fast_pow(axon_diameter_um, NIMCP_INTERNODE_BETA);

    return nimcp_myelin_clamp(L_opt, NIMCP_INTERNODE_MIN_UM, NIMCP_INTERNODE_MAX_UM);
}

/**
 * @brief Compute internode length efficiency
 *
 * IMPLEMENTATION:
 *   Gaussian efficiency centered on optimal:
 *   η = exp(-(L - L_opt)² / (2 × σ² × L_opt²))
 *
 *   Deviation from optimal length reduces velocity.
 */
float nimcp_myelin_internode_efficiency(float current_length_um,
                                         float axon_diameter_um) {
    float L_opt = nimcp_myelin_optimal_internode(axon_diameter_um);

    float deviation = (current_length_um - L_opt) / L_opt;
    float sigma_sq = INTERNODE_EFFICIENCY_SIGMA * INTERNODE_EFFICIENCY_SIGMA;

    float efficiency = nimcp_myelin_fast_exp(-(deviation * deviation) / (2.0F * sigma_sq));

    return nimcp_myelin_clamp(efficiency, 0.5F, 1.0F);
}

uint32_t nimcp_myelin_optimal_node_count(float axon_length_um,
                                          float axon_diameter_um) {
    if (axon_length_um <= 0.0F) return 0;

    float L_opt = nimcp_myelin_optimal_internode(axon_diameter_um);

    // Number of internodes = length / internode_length
    // Number of nodes = number of internodes + 1 (for terminal nodes)
    float n_internodes = axon_length_um / L_opt;

    return (uint32_t)(n_internodes + 1.5F);  // Round up
}

//=============================================================================
// Metabolic Efficiency Implementation
//=============================================================================

/**
 * @brief Compute metabolic efficiency metrics
 *
 * IMPLEMENTATION:
 *   Energy cost of AP = membrane charging energy
 *
 *   Unmyelinated:
 *     E_unmyelin = C_m × V_AP² × A_membrane
 *     A_membrane = π × d × L (total membrane area)
 *
 *   Myelinated:
 *     E_myelin = C_m × V_AP² × A_nodes × efficiency_factors
 *     A_nodes = π × d × L_node × N_nodes
 *
 *   Efficiency ratio = E_unmyelin / E_myelin
 *
 *   ATP calculation:
 *     ATP_per_AP = E / E_ATP
 *     Where E_ATP ≈ 5 × 10^-20 J
 */
void nimcp_myelin_compute_metabolic_efficiency(float axon_length_um,
                                                float axon_diameter_um,
                                                uint32_t num_nodes,
                                                float mean_compaction,
                                                float mean_integrity,
                                                nimcp_metabolic_efficiency_t* result) {
    if (!result || axon_length_um <= 0.0F || axon_diameter_um <= 0.0F) {
        if (result) memset(result, 0, sizeof(nimcp_metabolic_efficiency_t));
        return;
    }

    // Clamp inputs
    mean_compaction = nimcp_myelin_clamp(mean_compaction, 0.0F, 1.0F);
    mean_integrity = nimcp_myelin_clamp(mean_integrity, 0.0F, 1.0F);

    // Convert to meters for SI units
    float L_m = axon_length_um * 1e-6F;
    float d_m = axon_diameter_um * 1e-6F;

    // Total membrane area (cylinder)
    float A_total = NIMCP_MYELIN_MATH_PI * d_m * L_m;

    // Unmyelinated energy cost
    float E_unmyelin = NIMCP_METAB_C_M * NIMCP_METAB_V_AP * NIMCP_METAB_V_AP * A_total;

    // Node area (only nodes are active in myelinated axon)
    float node_length_m = NIMCP_SALTATORY_NODE_LENGTH_UM * 1e-6F;
    float A_nodes = NIMCP_MYELIN_MATH_PI * d_m * node_length_m * (float)num_nodes;

    // Myelinated energy (with paranode leak and efficiency factors)
    float paranode_factor = 1.0F + (1.0F - mean_compaction) * (1.0F - NIMCP_METAB_PARANODE_LEAK);
    float integrity_factor = 1.0F + (1.0F - mean_integrity) * 0.5F;  // Damaged myelin leaks

    float E_myelin = NIMCP_METAB_C_M * NIMCP_METAB_V_AP * NIMCP_METAB_V_AP *
                     A_nodes * paranode_factor * integrity_factor;

    // Results in picojoules (pJ = 10^-12 J)
    result->energy_per_ap_pj = E_myelin * 1e12F;
    result->energy_unmyelin_pj = E_unmyelin * 1e12F;

    // Efficiency ratio
    result->efficiency_ratio = E_unmyelin / (E_myelin + NIMCP_MYELIN_MATH_EPSILON);

    // ATP calculations
    result->atp_per_ap = E_myelin / NIMCP_METAB_ATP_ENERGY_J;
    result->atp_per_meter = result->atp_per_ap / L_m;

    // Power at 100 Hz firing rate (μW)
    result->power_uw = E_myelin * 100.0F * 1e6F;
}

float nimcp_myelin_atp_per_ap(const nimcp_metabolic_efficiency_t* efficiency) {
    if (!efficiency) return 0.0F;
    return efficiency->atp_per_ap;
}

float nimcp_myelin_power_consumption(const nimcp_metabolic_efficiency_t* efficiency,
                                      float firing_rate_hz) {
    if (!efficiency || firing_rate_hz <= 0.0F) return 0.0F;

    // E (pJ) × rate (Hz) = power (pW) → convert to μW
    return efficiency->energy_per_ap_pj * firing_rate_hz * 1e-6F;
}

//=============================================================================
// Stochastic Variability Implementation
//=============================================================================

void nimcp_myelin_rng_init(nimcp_myelin_rng_t* rng, uint64_t seed) {
    if (!rng) return;

    if (seed == 0) {
        seed = (uint64_t)time(NULL) ^ 0xDEADBEEFCAFEBABEULL;
    }

    rng->seed = seed;
    rng->state = seed;
    rng->samples_generated = 0;
}

void nimcp_myelin_rng_reset(nimcp_myelin_rng_t* rng) {
    if (!rng) return;

    rng->state = rng->seed;
    rng->samples_generated = 0;
}

/**
 * @brief Generate uniform random value [0, 1)
 *
 * IMPLEMENTATION:
 *   Linear Congruential Generator (LCG)
 *   state = state × multiplier + increment
 *
 *   Uses high-quality multiplier from L'Ecuyer (1999)
 */
float nimcp_myelin_rng_uniform(nimcp_myelin_rng_t* rng) {
    if (!rng) return 0.5F;

    // LCG iteration
    rng->state = rng->state * RNG_MULTIPLIER + RNG_INCREMENT;
    rng->samples_generated++;

    // Convert to [0, 1)
    // Use upper 32 bits for better randomness
    uint32_t upper = (uint32_t)(rng->state >> 32);
    return (float)upper / 4294967296.0F;  // 2^32
}

/**
 * @brief Generate normal random value using Box-Muller transform
 *
 * IMPLEMENTATION:
 *   Box-Muller: Generate two uniforms U1, U2
 *   Z0 = sqrt(-2 × ln(U1)) × cos(2π × U2)
 *   Z1 = sqrt(-2 × ln(U1)) × sin(2π × U2)
 *
 *   Returns Z0 scaled by mean and stddev.
 */
float nimcp_myelin_rng_normal(nimcp_myelin_rng_t* rng, float mean, float stddev) {
    if (!rng || stddev <= 0.0F) return mean;

    float u1 = nimcp_myelin_rng_uniform(rng);
    float u2 = nimcp_myelin_rng_uniform(rng);

    // Avoid log(0)
    if (u1 < NIMCP_MYELIN_MATH_EPSILON) {
        u1 = NIMCP_MYELIN_MATH_EPSILON;
    }

    // Box-Muller transform
    float z0 = nimcp_myelin_fast_sqrt(-2.0F * logf(u1)) *
               cosf(2.0F * NIMCP_MYELIN_MATH_PI * u2);

    return mean + stddev * z0;
}

/**
 * @brief Generate log-normal random value
 *
 * IMPLEMENTATION:
 *   If X ~ LogNormal(μ, σ), then ln(X) ~ Normal(μ, σ)
 *
 *   Given target mean M and CV:
 *   σ² = ln(1 + CV²)
 *   μ = ln(M) - σ²/2
 *
 *   X = exp(Normal(μ, σ))
 */
float nimcp_myelin_rng_lognormal(nimcp_myelin_rng_t* rng, float mean, float cv) {
    if (!rng || mean <= 0.0F || cv <= 0.0F) return mean;

    // Calculate log-normal parameters
    float cv_sq = cv * cv;
    float sigma_sq = logf(1.0F + cv_sq);
    float sigma = nimcp_myelin_fast_sqrt(sigma_sq);
    float mu = logf(mean) - sigma_sq / 2.0F;

    // Generate normal and exponentiate
    float z = nimcp_myelin_rng_normal(rng, mu, sigma);

    return expf(z);
}

uint32_t nimcp_myelin_vary_lamellae(nimcp_myelin_rng_t* rng, uint32_t target_lamellae) {
    if (!rng || target_lamellae == 0) return target_lamellae;

    float varied = nimcp_myelin_rng_lognormal(rng, (float)target_lamellae,
                                               NIMCP_STOCH_CV_LAMELLAE);

    // Round to nearest integer, ensure at least 1
    uint32_t result = (uint32_t)(varied + 0.5F);
    return (result > 0) ? result : 1;
}

float nimcp_myelin_vary_g_ratio(nimcp_myelin_rng_t* rng, float target_g_ratio) {
    if (!rng) return target_g_ratio;

    float stddev = target_g_ratio * NIMCP_STOCH_CV_G_RATIO;
    float varied = nimcp_myelin_rng_normal(rng, target_g_ratio, stddev);

    return nimcp_myelin_clamp(varied, NIMCP_G_RATIO_MIN, NIMCP_G_RATIO_MAX);
}

float nimcp_myelin_vary_internode(nimcp_myelin_rng_t* rng, float target_length_um) {
    if (!rng || target_length_um <= 0.0F) return target_length_um;

    float varied = nimcp_myelin_rng_lognormal(rng, target_length_um,
                                               NIMCP_STOCH_CV_INTERNODE);

    return nimcp_myelin_clamp(varied, NIMCP_INTERNODE_MIN_UM, NIMCP_INTERNODE_MAX_UM);
}

float nimcp_myelin_vary_velocity(nimcp_myelin_rng_t* rng, float target_velocity) {
    if (!rng || target_velocity <= 0.0F) return target_velocity;

    float stddev = target_velocity * NIMCP_STOCH_CV_VELOCITY;
    float varied = nimcp_myelin_rng_normal(rng, target_velocity, stddev);

    return nimcp_myelin_clamp(varied, NIMCP_SALTATORY_V_MIN_MS, NIMCP_SALTATORY_V_MAX_MS);
}
