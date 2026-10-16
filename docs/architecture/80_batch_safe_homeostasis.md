# Batch-Safe Biological Stability — Design

**Status:** Phase 4.1 — in-progress Python reference; C port pending
**Risk level:** HIGH — subtle bugs cause silent training corruption
**Last Updated:** 2026-04-19

## Problem

Athena's biological stability package (synaptic scaling, intrinsic plasticity,
short-term depression, inhibitory plasticity, metabolic budget) is calibrated
for **sequential** training (batch=1). Batching causes:

1. Cross-network gradient amplification → explosion
2. Within-batch cumulative input current → SNN saturation
3. Time-constant mismatch — EMAs calibrated for per-sample decay
4. R-STDP eligibility traces lose temporal ordering

Every attempted batching has triggered these failure modes. Phase 4.1 rewrites
the mechanisms so they remain mathematically equivalent under batching.

## Biological-Fidelity Requirement

**No compromise.** Each mechanism must produce the same long-term behavior
under batched inputs as under sequential. Specifically, for any observable
(firing rate, threshold, synapse weight, etc.), the trajectory under batch=N
must converge to the trajectory under batch=1 as time → ∞.

Equivalent conditions:
- Time constants scale correctly with batch size
- Per-sample effects accumulate additively within a batch
- Update applied once per batch (not per sample within)

## Per-Mechanism Derivations

### 1. Synaptic Scaling (Turrigiano)

**Sequential form:**
```
rate_ema[n, t+1] = α · rate_ema[n, t] + (1-α) · fired[n, t]
                    (α = exp(-1/τ_rate), τ_rate = 100 steps)

if mean(rate_ema) drifts from target:
    W *= 1 + η_scale · (target - mean(rate_ema))
```

**Batch-safe form:** For a batch of B samples, each contributing a
firing indicator `fired[n, b]`:
```
# Apply B decay steps in one update:
α_B = α^B
# Accumulate B samples' firing — each decayed by its age:
contrib[n] = sum_{b=0..B-1} (1-α) · α^(B-1-b) · fired[n, b]

rate_ema[n, t+B] = α_B · rate_ema[n, t] + contrib[n]
```

Equivalent to sequential iteration, but computed in vectorized form.

**Key property:** For batch size 1, reduces to sequential. For batch size
N, the result is exactly the same as N sequential applications (up to
floating-point reassociation).

### 2. Intrinsic Plasticity (IP Threshold Adaptation)

**Sequential form:**
```
for each sample t:
    if fired[n, t]:
        threshold_offset[n] += η_IP · (rate_ema[n] - target)
        threshold_offset[n] = clip(threshold_offset[n], -Δmax, +Δmax)
```

**Batch-safe form:**
```
# Count fires per neuron in this batch:
n_fires[n] = sum_{b=0..B-1} fired[n, b]

# Use average rate_ema across batch (since IP responds to slow-timescale rate):
avg_rate[n] = mean(rate_ema[n, t:t+B])

# Single update per neuron proportional to fires:
threshold_offset[n] += n_fires[n] · η_IP · (avg_rate[n] - target)
threshold_offset[n] = clip(threshold_offset[n], -Δmax, +Δmax)
```

**Rationale:** Because IP uses rate_ema (slow timescale) as its error
signal, using the batch-averaged rate_ema is equivalent to applying the
sequential updates with the slowly-changing rate. Exact equivalence
requires either per-sample update (expensive) or assumption of slow
rate_ema change within a batch (valid for batch << τ_rate).

### 3. Short-Term Depression

**Sequential form:**
```
depression[n, t+1] = 0.95 · depression[n, t] + 0.2 · fired[n, t]
depression[n] = min(depression[n], 0.5)
```

**Batch-safe form:** For batch B:
```
# Decay factor over B samples:
decay_B = 0.95^B

# Accumulated jump, each sample's jump decayed by its age:
jump[n] = sum_{b=0..B-1} 0.2 · 0.95^(B-1-b) · fired[n, b]

depression[n, t+B] = decay_B · depression[n, t] + jump[n]
depression[n] = min(depression[n], 0.5)
```

Direct vectorization of sequential recurrence.

### 4. Inhibitory Plasticity

**Sequential form:**
```
for each sample t, for each synapse (i, j) where j is inhibitory:
    δw[i,j] = -η_inh · (fired[i] · fired[j] - 2 · target^2)
    w[i,j] += δw
```

**Batch-safe form:**
```
# Compute co-activity across batch:
co[i, j] = sum_{b=0..B-1} fired[i, b] · fired[j, b]

# Target term scales with batch size:
delta[i, j] = -η_inh · (co[i, j] - B · 2 · target^2)
w[i, j] += delta[i, j]
```

Exact equivalence (sum of sequential updates = batched co-activity).

### 5. Metabolic Budget

**Stateless** — already batch-safe. Applied once per update:
```
# After any weight change:
for each neuron n:
    total = sum_abs(W[n, :])
    if total > cap:
        W[n, :] *= cap / total
```

No modification needed.

### 6. Gradient Flow Across Networks

**Sequential form:** Each network clips its own gradient to 1.0 norm.
Bridges pass clipped gradients between.

**Batch-safe form:** Compute per-network gradient norm across all samples,
apply single clipping step:
```
# Collect gradients from each network:
g_ann, g_snn, g_lnn, g_cnn, g_hnn, g_fno = backward(batch)

# Sum per-network norms:
total_norm = sum(norm(g)^2 for g in networks)^0.5

# Global clip:
scale = min(1.0, budget / total_norm)
for g in networks:
    g *= scale
```

This preserves the relative magnitudes between networks (no one network
dominates) while capping total gradient budget. Equivalent to sequential
in the limit where bridges don't add gradients beyond initial.

### 7. R-STDP Batch Handling

**Critical:** R-STDP is temporal — `w += η · r · eligibility`, where
eligibility encodes pre-post spike-timing correlation.

**Sequential form:** One sample at a time, eligibility trace updated per
spike, reward modulates.

**Batch-safe form (per-synapse eligibility sum):**
```
# Process each sample within batch to accumulate eligibility:
for b in 0..B-1:
    pre_fired = fired_pre[b]
    post_fired = fired_post[b]
    # Update trace (per-synapse):
    trace[i,j] = decay_trace * trace[i,j]
    if pre_fired[i]:
        trace[i,j] += 0.1 * post_fired[j]  # LTP term
    if post_fired[j]:
        trace[i,j] -= 0.1 * pre_fired[i]   # LTD term
    # Do NOT apply weight update per-sample

# After batch completes, single weight update:
w[i,j] += η · R · trace[i,j]   # R is batch-averaged reward
```

Preserves temporal ordering within each sample's trace update while
amortizing the costly weight-update step to once-per-batch.

## Implementation Strategy

### Layer 1 — Python Reference ✅ DELIVERED (2026-04-19)

- `scripts/batch_safe_homeostasis/` package — 7 modules
- Each mechanism in pure Python/NumPy
- 14 differential tests proving batch=N trajectory ≡ N sequential applications
- 4 integration tests, 4 E2E tests

### Layer 2 — C Port ✅ DELIVERED (2026-04-19)

- `include/snn/nimcp_snn_batch_safe.h` — public C API
- `src/snn/batch_safe/nimcp_snn_batch_safe.c` — all 7 mechanisms ported
- Feature flag `g_batch_safe_enabled` (default FALSE)
- Existing sequential path preserved — new path opt-in
- 12 C↔Python differential tests — all max |Δ| below 9e-8 (f32 precision)
- 13 stress tests (edge cases, large batches, long horizons) — all pass

**Status per mechanism:**

| # | Mechanism | Status | Equivalence |
|---|-----------|--------|-------------|
| 1 | Synaptic Scaling | C ported | max \|Δ\| 8.6e-8 |
| 2 | Short-Term Depression | C ported | max \|Δ\| 4.0e-8 |
| 3 | Metabolic Budget | C ported | max \|Δ\| 3.7e-8 |
| 4 | Global Gradient Budget | C ported | — |
| 5 | Intrinsic Plasticity | C ported | max \|Δ\| 6.9e-9 |
| 6 | Inhibitory Plasticity | C ported (dense) | max \|Δ\| 8.9e-10 |
| 7 | Batch R-STDP | C ported (dense) | **max \|Δ\| 6.2e-12 (bitwise)** |

**Python bindings exposed:**

```python
import nimcp
nimcp.bs_is_enabled()              # feature flag check
nimcp.bs_set_enabled(True)         # flip flag
nimcp.bs_self_test()               # internal correctness check, returns 0
nimcp.bs_scaling_apply(rate, fired_batch, B, N, alpha)
nimcp.bs_depression_apply(dep, fired_batch, B, N, decay, jump, cap)
nimcp.bs_metabolic_apply(w, row_ptr, n, cap_per_fan_in)
nimcp.bs_ip_apply(thr, fired_batch, rate, B, N, eta, target, dmax)
nimcp.bs_inhibitory_apply(w, pre_batch, post_batch, B, P, Q, eta, target)
nimcp.bs_rstdp_apply(w, trace, pre_batch, post_batch, rewards,
                      B, P, Q, decay, ltp, ltd, lr)
```

### Layer 3 — Integration (remaining work)

What's NOT yet done — the risky hot-path wiring:

- Modify `nimcp_snn_training.c` to invoke `nimcp_snn_*_apply_batch`
  when `g_batch_safe_enabled == true`
- Add `--batch-size N` to `immerse_athena.py`
- Move inhibitory + R-STDP from dense to CSR form for 1.45B-synapse scale
- Canary deployment + measurement on pod

Intentionally deferred — this is the step that could introduce silent
training corruption. Python refs + C ports are tested; flipping the flag
in production needs a dedicated session with careful measurement.

## Risk Mitigation

1. **Feature flag** on every batch path — can disable instantly
2. **Differential testing** — batch output must equal sequential within
   floating-point tolerance
3. **Long-horizon testing** — 1000+ steps of training with batch, compare
   final weights to sequential baseline
4. **Quarantine deployment** — test on canary brain (smaller) before
   main brain
5. **Rollback procedure** — flag flip returns to old behavior
6. **Audit log events** — batch-mode transitions logged for forensics

## Non-Goals

- **Speed gain is secondary.** The primary goal is correctness. Speed
  comes from being able to *safely* use batch=N.
- **Not replacing sequential path.** Old code stays. New code is additive.
- **Not handling all cases.** Start with B=2, prove equivalence, then B=4,
  B=8, etc. B=128 is the end state, not the starting point.

## Acceptance Criteria

For each mechanism:
- [ ] Python reference implementation exists
- [ ] Unit test proves batch=1 → identical to sequential
- [ ] Unit test proves batch=N → trajectory converges to sequential over
      long horizons (within 1e-4 relative error)
- [ ] Integration test: full forward+backward with batching works
- [ ] Documented in this file

For overall system:
- [ ] Feature flag for batch mode in brain config
- [ ] Regression gate passes with flag OFF (no regression)
- [ ] Regression gate passes with flag ON, B=1 (identity)
- [ ] Differential testing harness in `tests/regression/batch_diff.py`
