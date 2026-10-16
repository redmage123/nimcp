/**
 * @file enhanced_stats.c
 * @brief Statistical analysis for enhanced consciousness
 *
 * WHAT: Information geometry, dynamics, and binding metrics
 * WHY:  Single responsibility for statistical/mathematical analysis
 * HOW:  Time series analysis, entropy estimation, phase coherence
 *
 * @author NIMCP Development Team
 * @date 2026-02-16
 * @version 2.6.3
 */

#include "nimcp_swarm_consciousness_enhanced_internal.h"

//=============================================================================
// Shared Statistical Utilities
//=============================================================================

/**
 * WHAT: Compute autocorrelation at given lag
 * WHY:  Detect critical slowing and phase transitions
 * HOW:  Standard autocorrelation formula
 */
float enhanced_compute_autocorrelation(const float* data, uint32_t count, uint32_t lag) {
    if (!data || count <= lag + 1) return 0.0f;

    float mean = enhanced_compute_mean(data, count);
    float variance = enhanced_compute_variance(data, count);
    if (variance < 1e-10f) return 0.0f;

    float autocorr = 0.0f;
    uint32_t n = count - lag;
    for (uint32_t i = 0; i < n; i++) {
        autocorr += (data[i] - mean) * (data[i + lag] - mean);
    }
    autocorr /= (n * variance);

    return autocorr;
}

/**
 * WHAT: Estimate differential entropy
 * WHY:  Information geometry computation
 * HOW:  Use central statistics module
 */
float enhanced_estimate_entropy(const float* data, uint32_t count, uint32_t bins, float bin_width) {
    (void)bin_width;  /* bin_width handled internally by statistics module */
    if (!data || count == 0 || bins == 0) return 0.0f;

    /* Use central statistics module for differential entropy estimation */
    return nimcp_stats_differential_entropy(data, count, bins);
}

//=============================================================================
// Information Geometry Functions
//=============================================================================

/**
 * WHAT: Compute information geometry metrics
 * WHY:  Deep analysis of information integration
 * HOW:  Compute mutual info, transfer entropy from phi history
 */
bool swarm_compute_information_geometry(
    swarm_consciousness_enhanced_ctx_t* ctx,
    information_geometry_t* geometry)
{
    if (!ctx || !geometry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_information_geometry: required parameter is NULL (ctx, geometry)");
        return false;
    }

    return enhanced_compute_geometry_impl(ctx, geometry);
}

/**
 * WHAT: Internal implementation of geometry computation
 * WHY:  Separate public API from implementation
 * HOW:  Entropy-based information measures
 */
bool enhanced_compute_geometry_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    information_geometry_t* geometry)
{
    nimcp_mutex_lock(&ctx->lock);

    if (ctx->history_count < MIN_GEOMETRY_SAMPLES) {
        nimcp_mutex_unlock(&ctx->lock);
        memset(geometry, 0, sizeof(*geometry));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "enhanced_compute_geometry_impl: validation failed");
        return false;
    }

    uint32_t n = ctx->history_count;
    uint32_t bins = DEFAULT_ENTROPY_BINS;
    float bin_width = ctx->config.entropy_bin_width;

    // Compute entropy of phi history
    float H_phi = enhanced_estimate_entropy(ctx->phi_history, n, bins, bin_width);

    // For full mutual information matrix, we need per-drone histories
    // For now, compute simplified total correlation from phi series
    geometry->total_correlation = H_phi;

    // Integration approximation: difference between joint and sum of marginals
    geometry->integration = H_phi * 0.5f;  // Simplified

    // Complexity: integration weighted by information content
    geometry->complexity = geometry->integration * H_phi;

    // Redundancy: shared information (simplified estimate)
    geometry->redundancy = geometry->total_correlation - geometry->integration;
    if (geometry->redundancy < 0) geometry->redundancy = 0;

    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

//=============================================================================
// Consciousness Dynamics Functions
//=============================================================================

/**
 * WHAT: Compute consciousness dynamics metrics
 * WHY:  Detect phase transitions, criticality
 * HOW:  Time series analysis of phi trajectory
 */
bool swarm_compute_consciousness_dynamics(
    swarm_consciousness_enhanced_ctx_t* ctx,
    consciousness_dynamics_t* dynamics)
{
    if (!ctx || !dynamics) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_consciousness_dynamics: required parameter is NULL (ctx, dynamics)");
        return false;
    }

    return enhanced_compute_dynamics_impl(ctx, dynamics);
}

/**
 * WHAT: Internal implementation of dynamics computation
 * WHY:  Separate public API from implementation
 * HOW:  Lyapunov, autocorrelation, variance analysis
 */
bool enhanced_compute_dynamics_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    consciousness_dynamics_t* dynamics)
{
    nimcp_mutex_lock(&ctx->lock);

    if (ctx->history_count < MIN_DYNAMICS_SAMPLES) {
        nimcp_mutex_unlock(&ctx->lock);
        memset(dynamics, 0, sizeof(*dynamics));
        dynamics->current_phase = CONSCIOUSNESS_PHASE_CHAOS;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "enhanced_compute_dynamics_impl: validation failed");
        return false;
    }

    uint32_t n = ctx->history_count;

    // Compute variance
    float variance = enhanced_compute_variance(ctx->phi_history, n);

    // Compute autocorrelation at lag 1
    float autocorr = enhanced_compute_autocorrelation(ctx->phi_history, n, 1);

    // Compute variance trend (change over window)
    float early_var = enhanced_compute_variance(ctx->phi_history, n / 2);
    float late_var = enhanced_compute_variance(ctx->phi_history + n / 2, n / 2);
    float var_trend = (late_var - early_var) / (early_var + 0.001f);

    // Approximate Lyapunov exponent from divergence
    float lyap = 0.0f;
    if (n > 2) {
        float sum_log_div = 0.0f;
        uint32_t count = 0;
        for (uint32_t i = 1; i < n; i++) {
            float diff = fabsf(ctx->phi_history[i] - ctx->phi_history[i-1]);
            if (diff > 0.001f) {
                sum_log_div += logf(diff);
                count++;
            }
        }
        if (count > 0) {
            lyap = sum_log_div / count;
        }
    }

    // Determine phase based on metrics
    consciousness_phase_t phase;
    if (lyap > 0.5f && variance > ctx->config.critical_variance_threshold) {
        phase = CONSCIOUSNESS_PHASE_CHAOS;
    } else if (autocorr > CRITICAL_AUTOCORRELATION_THRESHOLD) {
        phase = CONSCIOUSNESS_PHASE_CRITICAL;
    } else if (variance < ctx->config.critical_variance_threshold * 0.1f) {
        phase = CONSCIOUSNESS_PHASE_FROZEN;
    } else {
        phase = CONSCIOUSNESS_PHASE_ORDERED;
    }

    // Detect near-transition
    bool near_transition = (autocorr > CRITICAL_AUTOCORRELATION_THRESHOLD * 0.8f) ||
                          (fabsf(var_trend) > 0.5f);

    // Fill dynamics struct
    dynamics->current_phase = phase;
    dynamics->lyapunov_exponent = lyap;
    dynamics->autocorrelation = autocorr;
    dynamics->variance_trend = var_trend;
    dynamics->attractor_strength = 1.0f - fabsf(lyap);
    dynamics->near_transition = near_transition;
    dynamics->transition_probability = near_transition ? 0.3f : 0.05f;

    nimcp_mutex_unlock(&ctx->lock);

    return true;
}

//=============================================================================
// Neural Binding Functions
//=============================================================================

/**
 * WHAT: Compute neural binding metrics
 * WHY:  Detect gamma-band synchronization
 * HOW:  Phase coherence analysis of phi oscillations
 */
bool swarm_compute_neural_binding(
    swarm_consciousness_enhanced_ctx_t* ctx,
    neural_binding_t* binding)
{
    if (!ctx || !binding) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_compute_neural_binding: required parameter is NULL (ctx, binding)");
        return false;
    }

    return enhanced_compute_binding_impl(ctx, binding);
}

/**
 * WHAT: Internal implementation of binding computation
 * WHY:  Separate public API from implementation
 * HOW:  Gamma power and phase coherence estimation
 */
bool enhanced_compute_binding_impl(
    swarm_consciousness_enhanced_ctx_t* ctx,
    neural_binding_t* binding)
{
    nimcp_mutex_lock(&ctx->lock);

    memset(binding, 0, sizeof(*binding));

    if (ctx->history_count < 10) {
        nimcp_mutex_unlock(&ctx->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "enhanced_compute_binding_impl: validation failed");
        return false;
    }

    // Estimate gamma power from high-frequency phi fluctuations
    float gamma_power = 0.0f;
    float phase_sum_cos = 0.0f;
    float phase_sum_sin = 0.0f;
    uint32_t n = ctx->history_count;

    // Simple gamma estimation: high-frequency variance
    for (uint32_t i = 1; i < n; i++) {
        float diff = ctx->phi_history[i] - ctx->phi_history[i-1];
        gamma_power += diff * diff;

        // Estimate phase from rate of change
        float phase = atan2f(diff, ctx->phi_history[i]);
        phase_sum_cos += cosf(phase);
        phase_sum_sin += sinf(phase);
    }
    gamma_power /= (n - 1);

    // Phase coherence (PLV) from mean resultant length
    float mean_cos = phase_sum_cos / (n - 1);
    float mean_sin = phase_sum_sin / (n - 1);
    float phase_coherence = sqrtf(mean_cos * mean_cos + mean_sin * mean_sin);
    float mean_phase = atan2f(mean_sin, mean_cos);

    // Binding strength combines power and coherence
    float binding_strength = gamma_power * phase_coherence;
    bool binding_active = phase_coherence > ctx->config.phase_coherence_threshold;

    // Count bound drones (those contributing to coherence)
    uint32_t bound_count = binding_active ? (ctx->remote_phi_count + 1) : 0;

    // Fill binding struct
    binding->gamma_power = gamma_power;
    binding->phase_coherence = phase_coherence;
    binding->mean_phase = mean_phase;
    binding->binding_strength = binding_strength;
    binding->binding_active = binding_active;
    binding->bound_drone_count = bound_count;

    nimcp_mutex_unlock(&ctx->lock);

    return true;
}
