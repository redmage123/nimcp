/**
 * @file nimcp_snn_batch_safe.h
 * @brief Batch-safe biological stability mechanisms (Phase 4.1).
 *
 * These functions accept a batch of B samples and produce state updates
 * mathematically equivalent to N sequential applications of the existing
 * per-sample mechanisms.
 *
 * Each function is feature-flag gated via SNN_BATCH_SAFE_ENABLED — callers
 * must explicitly opt in. Sequential paths remain authoritative by default.
 *
 * Python reference implementations (proven equivalent via differential
 * tests) live in scripts/batch_safe_homeostasis/. See
 * docs/architecture/80_batch_safe_homeostasis.md for mathematical
 * derivations.
 */
#ifndef NIMCP_SNN_BATCH_SAFE_H
#define NIMCP_SNN_BATCH_SAFE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Feature flag
 * ========================================================================
 * Set via environment or build define to enable batch paths.
 * Default: OFF (sequential paths unchanged).
 */
bool nimcp_snn_batch_safe_is_enabled(void);
void nimcp_snn_batch_safe_set_enabled(bool enable);

/* ========================================================================
 * 1. Synaptic Scaling (batch-safe)
 * ========================================================================
 *
 * Recurrence (sequential, batch=1):
 *   rate_ema[n] ← α · rate_ema[n] + (1-α) · fired[n]
 *
 * Batch (mathematically equivalent to N sequential applications):
 *   α_B = α^B
 *   contrib[n] = Σ_{b=0..B-1} (1-α) · α^(B-1-b) · fired[n, b]
 *   rate_ema[n] ← α_B · rate_ema[n] + contrib[n]
 *
 * @param rate_ema  [in,out] Per-neuron firing-rate EMA (size n_neurons)
 * @param fired_batch [in] Flat fire pattern, size B*n_neurons,
 *                          indexed as fired_batch[b*n_neurons + n]
 * @param batch_size B — number of samples in the batch
 * @param n_neurons  Number of neurons
 * @param alpha      Decay factor (0 < alpha < 1)
 * @return 0 on success, -1 on invalid args
 */
int nimcp_snn_scaling_apply_batch(float* rate_ema,
                                    const float* fired_batch,
                                    uint32_t batch_size,
                                    uint32_t n_neurons,
                                    float alpha);

/* ========================================================================
 * 2. Short-Term Depression (batch-safe with cap-aware fallback)
 * ========================================================================
 *
 * Sequential: depression[n] ← decay·depression[n] + jump·fired[n],
 *              cap at max_depression.
 *
 * Batch uses linear recurrence when no cap would be crossed, falls back
 * to per-sample iteration when cap would have activated mid-batch.
 *
 * @param depression [in,out] Per-neuron depression level (size n_neurons)
 * @param fired_batch [in] Flat fire pattern [B*n_neurons]
 * @param batch_size B
 * @param n_neurons  Number of neurons
 * @param decay      Per-step decay factor (e.g. 0.95)
 * @param jump       Per-fire jump (e.g. 0.2)
 * @param cap        Maximum depression (e.g. 0.5)
 * @return 0 on success, -1 on invalid args
 */
int nimcp_snn_depression_apply_batch(float* depression,
                                       const float* fired_batch,
                                       uint32_t batch_size,
                                       uint32_t n_neurons,
                                       float decay,
                                       float jump,
                                       float cap);

/* ========================================================================
 * 3. Metabolic Budget (stateless — batch-safe by construction)
 * ========================================================================
 *
 * Per-neuron cap: Σ|w[n, :]| ≤ cap_per_fan_in · fan_in[n]
 * If total exceeds cap, scales weights uniformly.
 *
 * @param weights      [in,out] Flat incoming-weight array (CSR style)
 * @param row_ptr      [in] CSR row pointers, size n_neurons+1
 * @param n_neurons    Number of destination neurons
 * @param cap_per_fan_in  Cap coefficient (e.g. 0.8 → cap = 0.8 · fan_in)
 * @return 0 on success
 */
int nimcp_snn_metabolic_budget_apply(float* weights,
                                       const uint32_t* row_ptr,
                                       uint32_t n_neurons,
                                       float cap_per_fan_in);

/* ========================================================================
 * 4. Global Gradient Budget (across networks)
 * ========================================================================
 *
 * Treats multiple network gradients as one concatenated vector; clips
 * global L2 norm to `budget`. Preserves relative magnitudes between
 * networks — no single network dominates.
 *
 * @param grad_arrays  Array of pointers to per-network gradient buffers
 * @param grad_sizes   Size of each gradient buffer
 * @param n_networks   Number of networks
 * @param budget       Target global L2 norm (e.g. 1.0)
 * @return 0 on success
 */
int nimcp_snn_gradient_budget_apply(float** grad_arrays,
                                      const uint32_t* grad_sizes,
                                      uint32_t n_networks,
                                      float budget);

/* ========================================================================
 * 5. Intrinsic Plasticity (batch-safe approximation)
 * ========================================================================
 *
 * Update threshold_offset[n] based on number of fires and avg rate_ema.
 * Exact when rate_ema is constant over batch; bounded error otherwise.
 *
 * @param threshold_offset [in,out] Per-neuron threshold adjustment
 * @param fired_batch [in] Flat fire pattern [B*n_neurons]
 * @param rate_ema [in] Current per-neuron rate_ema (used as batch-avg
 *                       approximation)
 * @param batch_size B
 * @param n_neurons  Number of neurons
 * @param eta        IP learning rate (e.g. 0.5)
 * @param target     Target firing rate (e.g. 0.03)
 * @param delta_max  Max abs threshold offset (e.g. 0.01)
 * @return 0 on success
 */
int nimcp_snn_ip_apply_batch(float* threshold_offset,
                              const float* fired_batch,
                              const float* rate_ema,
                              uint32_t batch_size,
                              uint32_t n_neurons,
                              float eta,
                              float target,
                              float delta_max);

/* ========================================================================
 * 6. Inhibitory Plasticity (batch-safe, dense matrix form)
 * ========================================================================
 *
 * Sequential rule per sample:
 *   Δw[i,j] = -η · (fired_pre[i] · fired_post[j] - 2·target^2)
 *
 * Batch form (exact sum of per-sample deltas):
 *   Δw[i,j] = -η · ( Σ_b fired_pre[i,b]·fired_post[j,b]  - B·2·target^2 )
 *
 * @param w [in,out]       Flat weight matrix of size n_pre*n_post,
 *                          indexed as w[i*n_post + j]
 * @param fired_pre_batch  Flat [B*n_pre] pre-synaptic fire pattern
 * @param fired_post_batch Flat [B*n_post] post-synaptic fire pattern
 * @param batch_size       B
 * @param n_pre, n_post    Matrix dimensions
 * @param eta              Learning rate (e.g. 0.01)
 * @param target_rate      Target firing rate (e.g. 0.03)
 * @return 0 on success
 */
int nimcp_snn_inhibitory_apply_batch(float* w,
                                       const float* fired_pre_batch,
                                       const float* fired_post_batch,
                                       uint32_t batch_size,
                                       uint32_t n_pre,
                                       uint32_t n_post,
                                       float eta,
                                       float target_rate);

/* ========================================================================
 * 7. Batch R-STDP (dense matrix form)
 * ========================================================================
 *
 * Preserves temporal ordering within batch (per-sample trace updates);
 * applies single weight update using Σ r_b · trace_b — mathematically
 * EXACTLY equivalent to per-sample sequential updates.
 *
 * @param w [in,out]        Flat weight matrix [n_pre*n_post]
 * @param trace [in,out]    Flat eligibility trace [n_pre*n_post]
 * @param fired_pre_batch   Flat [B*n_pre]
 * @param fired_post_batch  Flat [B*n_post]
 * @param rewards           Per-sample rewards, size B
 * @param batch_size        B
 * @param n_pre, n_post     Matrix dimensions
 * @param trace_decay       Eligibility decay per sample (e.g. 0.9)
 * @param ltp_rate          LTP increment (e.g. 0.01)
 * @param ltd_rate          LTD decrement (e.g. 0.01)
 * @param learning_rate     R-STDP learning rate (e.g. 0.0005)
 * @return 0 on success
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
                                  float learning_rate);

/* ========================================================================
 * Self-test: run all batch-safe operations against known inputs and
 * verify outputs match the Python reference values.
 * ========================================================================
 *
 * Returns 0 on all-pass, >0 on failure count.
 */
int nimcp_snn_batch_safe_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_BATCH_SAFE_H */
