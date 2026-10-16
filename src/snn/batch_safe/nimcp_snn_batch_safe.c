/**
 * @file nimcp_snn_batch_safe.c
 * @brief C implementation of batch-safe biological stability.
 *
 * Each function here MUST produce the same output as the Python reference
 * in scripts/batch_safe_homeostasis/ (within floating-point tolerance).
 *
 * Test harness in tests/regression/test_batch_safe_c.py compares C output
 * to Python reference output across random inputs.
 */
#include "snn/nimcp_snn_batch_safe.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Feature flag: heap-allocated so it can be toggled at runtime. */
static bool g_batch_safe_enabled = false;

bool nimcp_snn_batch_safe_is_enabled(void)
{
    return g_batch_safe_enabled;
}

void nimcp_snn_batch_safe_set_enabled(bool enable)
{
    g_batch_safe_enabled = enable;
}

/* ========================================================================
 * 1. Synaptic Scaling — batch
 * ========================================================================
 *
 * rate_ema[n] ← α^B · rate_ema[n] + Σ_{b=0..B-1} (1-α) · α^(B-1-b) · fired[n,b]
 *
 * Implementation note: precompute α^(B-1-b) for each b once, reuse across
 * neurons. Single pass over batch × neurons.
 */
int nimcp_snn_scaling_apply_batch(float* rate_ema,
                                    const float* fired_batch,
                                    uint32_t batch_size,
                                    uint32_t n_neurons,
                                    float alpha)
{
    if (!rate_ema || !fired_batch || batch_size == 0 || n_neurons == 0)
        return -1;
    if (alpha <= 0.0f || alpha >= 1.0f) return -1;

    const float one_minus_alpha = 1.0f - alpha;

    /* Precompute α^(B-1-b) for each b in [0..B-1]. */
    float* powers = (float*)malloc(batch_size * sizeof(float));
    if (!powers) return -1;
    powers[batch_size - 1] = 1.0f;  /* α^0 = 1 */
    for (int32_t b = (int32_t)batch_size - 2; b >= 0; b--) {
        powers[b] = powers[b + 1] * alpha;
    }

    /* α^B */
    const float alpha_B = powers[0] * alpha;

    /* For each neuron: α^B · rate_ema + (1-α) · Σ powers[b] · fired[b][n] */
    for (uint32_t n = 0; n < n_neurons; n++) {
        float contrib = 0.0f;
        for (uint32_t b = 0; b < batch_size; b++) {
            contrib += powers[b] * fired_batch[b * n_neurons + n];
        }
        rate_ema[n] = alpha_B * rate_ema[n] + one_minus_alpha * contrib;
    }

    free(powers);
    return 0;
}

/* ========================================================================
 * 2. Short-Term Depression — batch with cap-aware fallback
 * ========================================================================
 *
 * Pure batched formula:
 *   depression[n] = decay^B · depression[n] + Σ jump · decay^(B-1-b) · fired[n,b]
 *
 * Cap (min(x, cap)) is non-linear; if batched formula would cross the cap
 * mid-batch, we fall back to sequential iteration for that neuron.
 */
int nimcp_snn_depression_apply_batch(float* depression,
                                       const float* fired_batch,
                                       uint32_t batch_size,
                                       uint32_t n_neurons,
                                       float decay,
                                       float jump,
                                       float cap)
{
    if (!depression || !fired_batch || batch_size == 0 || n_neurons == 0)
        return -1;
    if (decay <= 0.0f || decay >= 1.0f) return -1;
    if (jump < 0.0f || cap <= 0.0f) return -1;

    /* Precompute decay powers */
    float* powers = (float*)malloc(batch_size * sizeof(float));
    if (!powers) return -1;
    powers[batch_size - 1] = 1.0f;
    for (int32_t b = (int32_t)batch_size - 2; b >= 0; b--) {
        powers[b] = powers[b + 1] * decay;
    }
    const float decay_B = powers[0] * decay;

    for (uint32_t n = 0; n < n_neurons; n++) {
        /* Compute uncapped batched result */
        float jump_sum = 0.0f;
        for (uint32_t b = 0; b < batch_size; b++) {
            jump_sum += powers[b] * fired_batch[b * n_neurons + n];
        }
        float uncapped = decay_B * depression[n] + jump * jump_sum;

        /* Determine if cap would have been crossed mid-batch. */
        bool crossed = false;
        if (uncapped > cap) {
            crossed = true;
        } else {
            /* Even if final < cap, intermediate might cross. Walk through. */
            float cur = depression[n];
            for (uint32_t b = 0; b < batch_size; b++) {
                cur = decay * cur + jump * fired_batch[b * n_neurons + n];
                if (cur > cap) { crossed = true; break; }
            }
        }

        if (!crossed) {
            depression[n] = uncapped;
        } else {
            /* Sequential fallback: exact capping behavior */
            float cur = depression[n];
            for (uint32_t b = 0; b < batch_size; b++) {
                cur = decay * cur + jump * fired_batch[b * n_neurons + n];
                if (cur > cap) cur = cap;
            }
            depression[n] = cur;
        }
    }

    free(powers);
    return 0;
}

/* ========================================================================
 * 3. Metabolic Budget — stateless per-row cap
 * ========================================================================
 */
int nimcp_snn_metabolic_budget_apply(float* weights,
                                       const uint32_t* row_ptr,
                                       uint32_t n_neurons,
                                       float cap_per_fan_in)
{
    if (!weights || !row_ptr || n_neurons == 0) return -1;
    if (cap_per_fan_in <= 0.0f) return -1;

    for (uint32_t n = 0; n < n_neurons; n++) {
        const uint32_t lo = row_ptr[n];
        const uint32_t hi = row_ptr[n + 1];
        const uint32_t fan_in = hi - lo;
        if (fan_in == 0) continue;

        float total_abs = 0.0f;
        for (uint32_t k = lo; k < hi; k++) {
            total_abs += fabsf(weights[k]);
        }
        const float cap = cap_per_fan_in * (float)fan_in;
        if (total_abs > cap) {
            const float scale = cap / total_abs;
            for (uint32_t k = lo; k < hi; k++) {
                weights[k] *= scale;
            }
        }
    }
    return 0;
}

/* ========================================================================
 * 4. Global Gradient Budget
 * ========================================================================
 */
int nimcp_snn_gradient_budget_apply(float** grad_arrays,
                                      const uint32_t* grad_sizes,
                                      uint32_t n_networks,
                                      float budget)
{
    if (!grad_arrays || !grad_sizes || n_networks == 0) return -1;
    if (budget <= 0.0f) return -1;

    /* Total L2 norm across all networks */
    double total_sq = 0.0;
    for (uint32_t i = 0; i < n_networks; i++) {
        const float* g = grad_arrays[i];
        if (!g) continue;
        for (uint32_t k = 0; k < grad_sizes[i]; k++) {
            total_sq += (double)g[k] * (double)g[k];
        }
    }
    const double total = sqrt(total_sq);
    if (total <= (double)budget) return 0;  /* under budget, no-op */

    const float scale = (float)((double)budget / total);
    for (uint32_t i = 0; i < n_networks; i++) {
        float* g = grad_arrays[i];
        if (!g) continue;
        for (uint32_t k = 0; k < grad_sizes[i]; k++) {
            g[k] *= scale;
        }
    }
    return 0;
}

/* ========================================================================
 * 5. Intrinsic Plasticity — batch
 * ========================================================================
 *
 * For each neuron:
 *   n_fires = #b: fired[n, b]
 *   Δ = n_fires · η · (rate_ema[n] - target)
 *   threshold_offset[n] ← clip(threshold_offset[n] + Δ, -δmax, δmax)
 */
int nimcp_snn_ip_apply_batch(float* threshold_offset,
                              const float* fired_batch,
                              const float* rate_ema,
                              uint32_t batch_size,
                              uint32_t n_neurons,
                              float eta,
                              float target,
                              float delta_max)
{
    if (!threshold_offset || !fired_batch || !rate_ema) return -1;
    if (batch_size == 0 || n_neurons == 0) return -1;

    for (uint32_t n = 0; n < n_neurons; n++) {
        uint32_t n_fires = 0;
        for (uint32_t b = 0; b < batch_size; b++) {
            if (fired_batch[b * n_neurons + n] > 0.5f) n_fires++;
        }
        if (n_fires == 0) continue;
        const float err = rate_ema[n] - target;
        float delta = (float)n_fires * eta * err;
        threshold_offset[n] += delta;
        if (threshold_offset[n] > delta_max) threshold_offset[n] = delta_max;
        else if (threshold_offset[n] < -delta_max) threshold_offset[n] = -delta_max;
    }
    return 0;
}

/* ========================================================================
 * 6. Inhibitory Plasticity — batch (dense matrix form)
 * ========================================================================
 *
 * Δw[i,j] = -η · ( Σ_b fired_pre[i,b]·fired_post[j,b]  - B·2·target^2 )
 */
int nimcp_snn_inhibitory_apply_batch(float* w,
                                       const float* fired_pre_batch,
                                       const float* fired_post_batch,
                                       uint32_t batch_size,
                                       uint32_t n_pre,
                                       uint32_t n_post,
                                       float eta,
                                       float target_rate)
{
    if (!w || !fired_pre_batch || !fired_post_batch) return -1;
    if (batch_size == 0 || n_pre == 0 || n_post == 0) return -1;

    const float target_sq = target_rate * target_rate;
    const float B_offset = (float)batch_size * 2.0f * target_sq;

    for (uint32_t i = 0; i < n_pre; i++) {
        for (uint32_t j = 0; j < n_post; j++) {
            float co = 0.0f;
            for (uint32_t b = 0; b < batch_size; b++) {
                co += fired_pre_batch[b * n_pre + i]
                       * fired_post_batch[b * n_post + j];
            }
            w[i * n_post + j] += -eta * (co - B_offset);
        }
    }
    return 0;
}

/* ========================================================================
 * 7. Batch R-STDP — EXACT equivalence to sequential
 * ========================================================================
 *
 * Accumulates Σ r_b · trace_b into a delta buffer, applies once.
 * Traces updated per-sample (temporal ordering preserved).
 */
int nimcp_snn_rstdp_apply_batch(float* w,
                                  float* trace,
                                  const float* fired_pre_batch,
                                  const float* fired_post_batch,
                                  const float* rewards,
                                  uint32_t batch_size,
                                  uint32_t n_pre,
                                  uint32_t n_post,
                                  float trace_decay,
                                  float ltp_rate,
                                  float ltd_rate,
                                  float learning_rate)
{
    if (!w || !trace || !fired_pre_batch || !fired_post_batch || !rewards)
        return -1;
    if (batch_size == 0 || n_pre == 0 || n_post == 0) return -1;

    const size_t total = (size_t)n_pre * (size_t)n_post;
    float* delta = (float*)calloc(total, sizeof(float));
    if (!delta) return -1;

    for (uint32_t b = 0; b < batch_size; b++) {
        const float* pre_b = &fired_pre_batch[b * n_pre];
        const float* post_b = &fired_post_batch[b * n_post];
        const float r_b = rewards[b];

        for (uint32_t i = 0; i < n_pre; i++) {
            const bool pre_fired = pre_b[i] > 0.5f;
            for (uint32_t j = 0; j < n_post; j++) {
                const size_t idx = (size_t)i * n_post + j;
                const bool post_fired = post_b[j] > 0.5f;

                trace[idx] *= trace_decay;
                if (pre_fired && post_fired) {
                    trace[idx] += ltp_rate;
                } else if (post_fired && !pre_fired) {
                    trace[idx] -= ltd_rate;
                }
                delta[idx] += r_b * trace[idx];
            }
        }
    }

    for (size_t k = 0; k < total; k++) {
        w[k] += learning_rate * delta[k];
    }

    free(delta);
    return 0;
}

/* ========================================================================
 * Self-test — simple correctness checks
 * ========================================================================
 */
int nimcp_snn_batch_safe_self_test(void)
{
    int failures = 0;

    /* Test 1: scaling batch=1 gives approximately same result as sequential */
    {
        float rate_ema[3] = {0.03f, 0.03f, 0.03f};
        float fired[3] = {1.0f, 0.0f, 1.0f};
        nimcp_snn_scaling_apply_batch(rate_ema, fired, 1, 3, 0.99f);
        /* Expected: rate = 0.99*0.03 + 0.01*fired */
        const float expected_0 = 0.99f * 0.03f + 0.01f * 1.0f;
        if (fabsf(rate_ema[0] - expected_0) > 1e-6f) failures++;
    }

    /* Test 2: depression jumps on fire */
    {
        float dep[2] = {0.0f, 0.0f};
        float fired[2] = {1.0f, 0.0f};
        nimcp_snn_depression_apply_batch(dep, fired, 1, 2, 0.95f, 0.2f, 0.5f);
        /* After fire: dep[0] = 0.95*0 + 0.2 = 0.2 */
        if (fabsf(dep[0] - 0.2f) > 1e-6f) failures++;
        if (fabsf(dep[1]) > 1e-6f) failures++;
    }

    /* Test 3: metabolic budget scales when over */
    {
        float w[3] = {2.0f, -1.5f, 3.0f};
        uint32_t rp[2] = {0, 3};
        nimcp_snn_metabolic_budget_apply(w, rp, 1, 1.0f);
        /* total=6.5, cap=3, scale=3/6.5; Σ|w| should = 3 */
        float total = fabsf(w[0]) + fabsf(w[1]) + fabsf(w[2]);
        if (fabsf(total - 3.0f) > 1e-5f) failures++;
    }

    /* Test 4: gradient budget is no-op when under */
    {
        float g1[2] = {0.1f, 0.1f};
        float g2[2] = {0.1f, 0.1f};
        float* grads[2] = {g1, g2};
        uint32_t sizes[2] = {2, 2};
        nimcp_snn_gradient_budget_apply(grads, sizes, 2, 1.0f);
        if (fabsf(g1[0] - 0.1f) > 1e-6f) failures++;
    }

    /* Test 5: IP update */
    {
        float thr[1] = {0.0f};
        float fired[1] = {1.0f};
        float rate[1] = {0.10f};  /* target=0.03 → err=0.07 */
        nimcp_snn_ip_apply_batch(thr, fired, rate, 1, 1, 0.5f, 0.03f, 1.0f);
        /* Δ = 1 * 0.5 * 0.07 = 0.035 */
        if (fabsf(thr[0] - 0.035f) > 1e-6f) failures++;
    }

    return failures;
}
