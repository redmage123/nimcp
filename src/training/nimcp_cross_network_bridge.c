/**
 * @file nimcp_cross_network_bridge.c
 * @brief Cross-network bridge encoding/decoding with differentiable gradient flow
 *
 * WHAT: Specialized encoding/decoding for cross-network-type boundaries
 * WHY:  SNN↔ANN and LNN↔SNN boundaries need domain-specific transforms that
 *       are differentiable for end-to-end gradient flow
 * HOW:  Rate-to-spike (sigmoid threshold + SuperSpike surrogate),
 *       spike-to-rate (exponential filter), continuous-to-spike (LNN ODE→spike)
 *
 * GRADIENT FLOW:
 *   All bridges use SuperSpike surrogate gradient: σ'(x) = 1/(β|x|+1)²
 *   This enables backpropagation across the spike/rate boundary.
 *
 * @author NIMCP Development Team
 * @date 2026-03-11
 */

#include "training/nimcp_unified_training.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

//=============================================================================
// SuperSpike Surrogate Gradient (Zenke & Ganguli 2018)
//=============================================================================

/**
 * @brief SuperSpike surrogate gradient: σ'(x) = 1 / (β|x| + 1)²
 *
 * B7: Now takes beta as parameter (from bridge struct) instead of #define.
 */
static inline float superspike_surrogate(float x, float beta) {
    float denom = beta * fabsf(x) + 1.0f;
    return 1.0f / (denom * denom);
}

//=============================================================================
// Rate-to-Spike Bridge (ANN → SNN)
//=============================================================================
// Forward: Convert continuous [0,1]-normalized activations to spike probabilities
//          using a sigmoid threshold. Deterministic (soft) spikes for differentiability.
//
// Math:    spike_rate[i] = σ(gain * (rate[i] - threshold))
//          where σ is the sigmoid function, gain controls sharpness
//
// Backward: dL/drate[i] = dL/dspike[i] * gain * σ(x) * (1 - σ(x))
//           Falls back to surrogate gradient near threshold
//=============================================================================

/**
 * @brief Forward pass: continuous rates → soft spike rates
 * B7: Uses configurable gain/threshold from bridge struct
 */
void bridge_rate_to_spike_forward(const nimcp_cross_network_bridge_t* b,
                                   const float* source_output,
                                   float* target_input) {
    uint32_t dim = (b->source_dim < b->target_dim) ? b->source_dim : b->target_dim;
    float gain = b->spike_gain;
    float threshold = b->spike_threshold;

    for (uint32_t i = 0; i < dim; i++) {
        float x = gain * (source_output[i] - threshold);
        float spike_rate = 1.0f / (1.0f + expf(-x));
        target_input[i] = spike_rate;
    }
    for (uint32_t i = dim; i < b->target_dim; i++) {
        target_input[i] = 0.0f;
    }
}

/**
 * @brief Backward pass: dL/dspike → dL/drate using sigmoid derivative + surrogate
 */
void bridge_rate_to_spike_backward(const nimcp_cross_network_bridge_t* b,
                                    const float* dl_dtarget,
                                    float* dl_dsource) {
    if (!dl_dsource) return;
    uint32_t dim = (b->source_dim < b->target_dim) ? b->source_dim : b->target_dim;
    float gain = b->spike_gain;
    float threshold = b->spike_threshold;
    float beta = b->surrogate_beta;

    for (uint32_t i = 0; i < dim; i++) {
        float x = gain * (b->last_source_output[i] - threshold);
        float sig = 1.0f / (1.0f + expf(-x));
        float dsig = sig * (1.0f - sig);
        float grad = gain * (dsig + 0.1f * superspike_surrogate(x, beta));
        dl_dsource[i] = dl_dtarget[i] * grad;
    }
}

//=============================================================================
// Spike-to-Rate Bridge (SNN → ANN)
//=============================================================================
// Forward: Convert spike counts/rates to continuous activations
//          using exponential smoothing with learnable time constant.
//
// Math:    rate[i] = (1 - α) * rate_prev[i] + α * spike[i]
//          where α = dt/τ controls smoothing (τ = time constant)
//          For single-step: rate[i] = spike[i] (direct passthrough)
//
// Backward: dL/dspike[i] = dL/drate[i] * α * surrogate(spike[i] - threshold)
//           Surrogate gradient makes the spike→rate boundary differentiable
//=============================================================================

/**
 * @brief Forward pass: spikes → continuous rates via exponential smoothing
 * B7: Uses configurable alpha/threshold from bridge struct
 */
void bridge_spike_to_rate_forward(const nimcp_cross_network_bridge_t* b,
                                   const float* source_output,
                                   float* target_input) {
    uint32_t dim = (b->source_dim < b->target_dim) ? b->source_dim : b->target_dim;
    float alpha = b->spike_rate_alpha;

    for (uint32_t i = 0; i < dim; i++) {
        float prev = (b->last_target_input && i < b->target_dim) ?
                      b->last_target_input[i] : 0.0f;
        float rate = (1.0f - alpha) * prev + alpha * source_output[i];
        if (rate < 0.0f) rate = 0.0f;
        if (rate > 1.0f) rate = 1.0f;
        target_input[i] = rate;
    }
    for (uint32_t i = dim; i < b->target_dim; i++) {
        target_input[i] = 0.0f;
    }
}

/**
 * @brief Backward pass: dL/drate → dL/dspike using surrogate gradient
 */
void bridge_spike_to_rate_backward(const nimcp_cross_network_bridge_t* b,
                                    const float* dl_dtarget,
                                    float* dl_dsource) {
    if (!dl_dsource) return;
    uint32_t dim = (b->source_dim < b->target_dim) ? b->source_dim : b->target_dim;

    float alpha = b->spike_rate_alpha;
    float threshold = b->spike_threshold;
    float beta = b->surrogate_beta;

    for (uint32_t i = 0; i < dim; i++) {
        float spike_val = b->last_source_output[i];
        float surr = superspike_surrogate(spike_val - threshold, beta);
        dl_dsource[i] = dl_dtarget[i] * alpha * surr;
    }
}

/* Helper: safely read cached source output */
static inline float source_output_val(const nimcp_cross_network_bridge_t* b, uint32_t i) {
    return (b->last_source_output && i < b->source_dim) ? b->last_source_output[i] : 0.0f;
}

//=============================================================================
// Continuous-to-Spike Bridge (LNN → SNN)
//=============================================================================
// Forward: Convert continuous LNN ODE state to spike probabilities.
//          LNN outputs are unbounded continuous values from ODE integration.
//          We normalize via tanh → [0,1] → soft spike via sigmoid.
//
// Math:    normalized[i] = 0.5 * (tanh(scale * x[i]) + 1)     // map to [0,1]
//          spike_prob[i] = σ(gain * (normalized[i] - threshold))
//
// Backward: Chain rule through tanh + sigmoid + surrogate.
//=============================================================================

#define CONT_TO_SPIKE_SCALE     1.0f   /* tanh input scaling */

/**
 * @brief Forward pass: continuous ODE states → soft spike probabilities
 * B7: Uses configurable gain/threshold from bridge struct
 */
void bridge_continuous_to_spike_forward(const nimcp_cross_network_bridge_t* b,
                                         const float* source_output,
                                         float* target_input) {
    uint32_t dim = (b->source_dim < b->target_dim) ? b->source_dim : b->target_dim;
    float gain = b->spike_gain;
    float threshold = b->spike_threshold;

    for (uint32_t i = 0; i < dim; i++) {
        float t = tanhf(CONT_TO_SPIKE_SCALE * source_output[i]);
        float normalized = 0.5f * (t + 1.0f);
        float x = gain * (normalized - threshold);
        float spike_prob = 1.0f / (1.0f + expf(-x));
        target_input[i] = spike_prob;
    }
    for (uint32_t i = dim; i < b->target_dim; i++) {
        target_input[i] = 0.0f;
    }
}

/**
 * @brief Backward pass: dL/dspike → dL/d(ODE_state) through tanh + sigmoid + surrogate
 */
void bridge_continuous_to_spike_backward(const nimcp_cross_network_bridge_t* b,
                                          const float* dl_dtarget,
                                          float* dl_dsource) {
    if (!dl_dsource) return;
    uint32_t dim = (b->source_dim < b->target_dim) ? b->source_dim : b->target_dim;

    float gain = b->spike_gain;
    float threshold = b->spike_threshold;
    float beta = b->surrogate_beta;

    for (uint32_t i = 0; i < dim; i++) {
        float src = source_output_val(b, i);

        float t = tanhf(CONT_TO_SPIKE_SCALE * src);
        float normalized = 0.5f * (t + 1.0f);
        float x = gain * (normalized - threshold);
        float sig = 1.0f / (1.0f + expf(-x));

        float dsig = sig * (1.0f - sig);
        float dspike_dnorm = gain * (dsig + 0.1f * superspike_surrogate(x, beta));

        float dtanh = 1.0f - t * t;
        float dnorm_dsrc = 0.5f * CONT_TO_SPIKE_SCALE * dtanh;

        dl_dsource[i] = dl_dtarget[i] * dspike_dnorm * dnorm_dsrc;
    }
}
