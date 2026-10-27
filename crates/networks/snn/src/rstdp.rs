//! Phase 3c — reward-modulated spike-timing-dependent plasticity (R-STDP).
//!
//! # Math
//!
//! Classic STDP (see [`nimcp_plasticity::stdp_weight_delta`]) gives a per-pair
//! weight delta as a function of `Δt = t_post - t_pre`. R-STDP turns this into
//! a three-factor rule:
//!
//! ```text
//! e(t+dt) = e(t) · exp(-dt / τ_e) + stdp_pair_contribution(t)
//! r(t+dt) = r(t) · exp(-dt / τ_r) + reward(t)
//! Δw      = (r(t) - baseline) · e(t)         (applied each step)
//! ```
//!
//! - `e` is a per-synapse **eligibility trace** (decays with `τ_e`, default 100 ms).
//! - `r` is a **scalar reward trace** (decays with `τ_r`, default 50 ms).
//! - Reward modulation is **multiplicative**: zero reward → zero update.
//! - The TD-style `baseline_reward` lets callers run advantage-style learning;
//!   `r == baseline` ⇒ `Δw = 0`.
//!
//! # Why the warmup gate exists (V1 regression)
//!
//! V1 ran R-STDP from `t = 0`, before homeostatic scaling had any firing-rate
//! estimate to work with. On fresh init, near-random activity produces
//! many causal spike pairs, each of which looks like "correct" Hebbian
//! evidence — and the reward trace multiplies it. Runaway LTP followed.
//!
//! The fix (inherited from [`nimcp_plasticity::warmup`]) is brutally simple:
//! R-STDP is a no-op on weights until `state.rate_samples ≥ warmup_samples`.
//! Traces still advance during warmup so the first post-gate step isn't
//! cold — but weights are frozen. [`tests::hebbian_runaway_regression`]
//! holds this behavior in place forever.
//!
//! # V1 lessons encoded as regression tests (all in [`mod tests`])
//!
//! 1. `default_params_have_sensible_warmup` — 100 is the V1-proven threshold.
//! 2. `warmup_blocks_early_updates` — no weight motion until the gate opens.
//! 3. `eligibility_trace_decays` — stale pairs evaporate; no "trace explosion".
//! 4. `reward_modulation_multiplicative` — reward = 0 ⇒ Δw = 0 (no default update).
//! 5. `td_baseline_subtracts` — `r - baseline` form works.
//! 6. `weight_clipped_to_bounds` — `[w_min, w_max]` never exceeded.
//! 7. `step_is_deterministic` — bit-identical given equal inputs.
//! 8. `hebbian_runaway_regression` — the full V1 bug, gate off vs on.
//!
//! # Performance
//!
//! [`step_rstdp`] is pure CPU (no `unsafe`). Inner loops walk the CSR rows
//! (post-major) in one pass. Eligibility-trace decay is a flat loop over
//! `state.eligibility` — amenable to auto-vectorization. A future phase may
//! lift this to GPU; V1 kept R-STDP on CPU because eligibility-trace state
//! is too large and update-heavy for a good GPU kernel.

use nimcp_plasticity::{
    DEFAULT_WARMUP_THRESHOLD, StdpParams, rate_samples_ready, stdp_weight_delta,
};
use serde::{Deserialize, Serialize};

use crate::csr::CsrSynapses;

/// R-STDP parameters.
///
/// `stdp` is the classic timing kernel; the remaining fields wrap it with
/// eligibility decay, reward gating, TD baseline, warmup, and weight clipping.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct RstdpParams {
    /// Classic STDP shape (A⁺, A⁻, τ⁺, τ⁻).
    pub stdp: StdpParams,
    /// Eligibility trace decay time constant, ms. Default 100.
    pub eligibility_tau_ms: f32,
    /// Reward-trace decay, ms. Default 50.
    pub reward_tau_ms: f32,
    /// Baseline reward subtracted from raw reward before modulating Δw
    /// (TD-style advantage). Default 0. Used verbatim when
    /// `baseline_alpha == 0`; otherwise it only seeds the EMA below.
    pub baseline_reward: f32,
    /// EMA rate for an *adaptive* reward baseline (V1
    /// `g_rstdp_baseline_alpha`, V1 default 0.001): after each step
    /// `baseline ← (1 − α)·baseline + α·reward_trace`, and Δw uses
    /// `reward_trace − baseline`. `0.0` (V2 default) disables the EMA and
    /// keeps the constant `baseline_reward` — bit-identical to pre-EMA
    /// behaviour. Live-tunable as `rstdp_baseline_alpha`.
    #[serde(default)]
    pub baseline_alpha: f32,
    /// Warmup threshold — R-STDP is a no-op on weights until
    /// `rate_samples ≥ warmup_samples`. Default
    /// [`nimcp_plasticity::DEFAULT_WARMUP_THRESHOLD`] (100).
    pub warmup_samples: u32,
    /// Lower weight bound. Δw is clipped so `w` stays in `[w_min, w_max]`.
    pub w_min: f32,
    /// Upper weight bound.
    pub w_max: f32,
}

impl Default for RstdpParams {
    /// V1-validated biologically plausible defaults. Matching the warmup
    /// threshold to [`DEFAULT_WARMUP_THRESHOLD`] is deliberate — it's the
    /// constant that held the line on the Hebbian-runaway regression.
    fn default() -> Self {
        Self {
            stdp: StdpParams::default(),
            eligibility_tau_ms: 100.0,
            reward_tau_ms: 50.0,
            baseline_reward: 0.0,
            baseline_alpha: 0.0,
            warmup_samples: DEFAULT_WARMUP_THRESHOLD,
            w_min: 0.0,
            w_max: 1.0,
        }
    }
}

/// Per-[`CsrSynapses`] runtime state for R-STDP.
///
/// Holds the eligibility trace (per synapse), the scalar reward trace,
/// last-spike bookkeeping for pair computation, and the rate-samples counter
/// that drives the warmup gate.
#[derive(Debug, Clone)]
pub struct RstdpState {
    /// Per-synapse eligibility trace, same length as the CSR `weights` array.
    pub eligibility: Vec<f32>,
    /// Scalar reward trace (running exponential average of recent reward).
    pub reward_trace: f32,
    /// Last pre-spike time (ms) per pre-neuron. Initialised to `f32::NEG_INFINITY`
    /// so "no prior spike" contributes a vanishing STDP term.
    pub last_pre_spike_ms: Vec<f32>,
    /// Last post-spike time (ms) per post-neuron, same convention.
    pub last_post_spike_ms: Vec<f32>,
    /// Rate-samples counter — incremented by one per [`step_rstdp`] call.
    pub rate_samples: u32,
    /// Adaptive reward baseline (EMA of `reward_trace`). Only read when
    /// `RstdpParams::baseline_alpha > 0`; seeded from `baseline_reward`
    /// on the first such step (`None` until then).
    pub reward_baseline: Option<f32>,
}

impl RstdpState {
    /// Allocate fresh state for a synapse population with `n_synapses`,
    /// `n_pre`, and `n_post`.
    #[must_use]
    pub fn new(n_synapses: usize, n_pre: u32, n_post: u32) -> Self {
        Self {
            eligibility: vec![0.0; n_synapses],
            reward_trace: 0.0,
            last_pre_spike_ms: vec![f32::NEG_INFINITY; n_pre as usize],
            last_post_spike_ms: vec![f32::NEG_INFINITY; n_post as usize],
            rate_samples: 0,
            reward_baseline: None,
        }
    }

    /// Reset all transient state. Does not resize buffers; a call to
    /// [`Self::new`] is appropriate if dimensions changed.
    pub fn reset(&mut self) {
        self.eligibility.fill(0.0);
        self.reward_trace = 0.0;
        self.last_pre_spike_ms.fill(f32::NEG_INFINITY);
        self.last_post_spike_ms.fill(f32::NEG_INFINITY);
        self.rate_samples = 0;
    }

    /// True iff enough rate samples have been observed for the gate to open.
    #[must_use]
    pub fn warmup_done(&self, params: &RstdpParams) -> bool {
        rate_samples_ready(self.rate_samples, params.warmup_samples)
    }
}

/// Baseline the reward trace is measured against this step: the constant
/// `baseline_reward` when the EMA is off (`baseline_alpha == 0`), else the
/// adaptive EMA (seeded from `baseline_reward` on first use).
#[must_use]
pub fn effective_baseline(state: &RstdpState, params: &RstdpParams) -> f32 {
    if params.baseline_alpha > 0.0 {
        state.reward_baseline.unwrap_or(params.baseline_reward)
    } else {
        params.baseline_reward
    }
}

/// Advance the adaptive reward baseline after a step (V1: `baseline =
/// (1−α)·baseline + α·reward`). No-op when `baseline_alpha == 0`. Runs
/// every step, warmup included, so the baseline is warm when weights
/// start moving.
pub fn update_baseline(state: &mut RstdpState, params: &RstdpParams) {
    if params.baseline_alpha > 0.0 {
        let a = params.baseline_alpha.min(1.0);
        let prev = state.reward_baseline.unwrap_or(params.baseline_reward);
        state.reward_baseline = Some((1.0 - a) * prev + a * state.reward_trace);
    }
}

/// Exponential-decay factor `exp(-dt / τ)` with defensive short-circuits:
/// non-positive `τ` → no decay (factor 1.0); non-finite `dt` → 0.0.
#[inline]
fn decay_factor(dt_ms: f32, tau_ms: f32) -> f32 {
    if tau_ms <= 0.0 {
        1.0
    } else if !dt_ms.is_finite() {
        0.0
    } else {
        (-dt_ms / tau_ms).exp()
    }
}

/// One R-STDP update pass over a [`CsrSynapses`] matrix. Returns the number
/// of synapses whose weight actually moved (useful for observability).
///
/// # Semantics
///
/// 1. Advance eligibility traces: `e *= exp(-dt / τ_e)`.
/// 2. Advance reward trace: `r *= exp(-dt / τ_r)`, then `r += reward`.
/// 3. **Pre-spike scan** (against the *prior* `last_post_spike` table):
///    for each synapse whose source pre fired this step, credit
///    `eligibility[syn] += stdp_weight_delta(t_ms, last_post_spike[post], …)`.
///    Then update `last_pre_spike[i] = t_ms` for every pre that fired.
/// 4. **Post-spike scan** (against the just-updated `last_pre_spike` table):
///    for each synapse whose dest post fired this step, credit
///    `eligibility[syn] += stdp_weight_delta(last_pre_spike[pre], t_ms, …)`.
///    Then update `last_post_spike[j] = t_ms`. Pairs where pre also fired
///    this step evaluate `stdp(t_ms, t_ms) = 0` — the V1 simultaneous-spike
///    convention — correctly avoiding double-counting.
/// 5. If the warmup gate is open, apply `Δw = (r - baseline) · e` to every
///    synapse, clipping `w` to `[w_min, w_max]`.
/// 6. Increment `rate_samples` by 1.
///
/// During warmup (`rate_samples < warmup_samples`), steps 1-4 still execute
/// so the trace is ready on the first post-gate step, but step 5 is skipped
/// — weights never move. This is the V1 Hebbian-runaway gate.
///
/// # CSR layout note (destination-major)
///
/// `csr.row_ptr[i]..csr.row_ptr[i+1]` gives **incoming** synapses to
/// post-neuron `i`; `csr.col_idx[k]` is the **pre**-neuron for synapse `k`.
/// This matches [`CsrSynapses`]'s invariant (row = post, col = pre) — the
/// same layout the `i_syn_cpu` forward uses.
///
/// # Panics
///
/// Does not panic on the fast path. Malformed inputs (mismatched slice
/// lengths, out-of-range indices, malformed row pointers) are treated as
/// no-ops — R-STDP is hot-path code and must never crash a running brain.
// HOT PATH: called every SNN tick that delivers a reward.
// Exact-equality float comparisons below are deliberate short-circuits:
// - `elig_decay != 1.0` skips the multiply loop when τ = ∞ or dt = 0.
// - `modulator != 0.0` skips the weight-apply loop when r exactly cancels baseline.
// - `dw == 0.0` skips the clamp + assign for zero-eligibility synapses.
// - `new_w != *w` counts an update only when the clamp actually moved w.
// None of these are "fuzzy equality" checks; they're optimizer guards.
#[allow(clippy::float_cmp)]
// The 8-arg signature is dictated by the V2_PLAN Phase 3c spec; collapsing
// it into a config struct would hide the per-call spike tensors + reward
// that are the actual message to R-STDP.
#[allow(clippy::too_many_arguments)]
pub fn step_rstdp(
    csr: &mut CsrSynapses,
    state: &mut RstdpState,
    pre_spikes: &[u8],
    post_spikes: &[u8],
    reward: f32,
    t_ms: f32,
    dt_ms: f32,
    params: &RstdpParams,
) -> u32 {
    let n_syn = csr.weights.len();
    let n_pre = csr.n_pre as usize;
    let n_post = csr.n_post as usize;

    // Structural sanity. Bail early rather than panic on shape errors.
    if csr.col_idx.len() != n_syn
        || csr.row_ptr.len() != n_post + 1
        || pre_spikes.len() != n_pre
        || post_spikes.len() != n_post
        || state.eligibility.len() != n_syn
        || state.last_pre_spike_ms.len() != n_pre
        || state.last_post_spike_ms.len() != n_post
    {
        // No-op on structural mismatch. Observability will show zero updates.
        state.rate_samples = state.rate_samples.saturating_add(1);
        return 0;
    }

    // 1. Decay eligibility traces in place.
    let elig_decay = decay_factor(dt_ms, params.eligibility_tau_ms);
    if elig_decay != 1.0 {
        for e in state.eligibility.iter_mut() {
            *e *= elig_decay;
        }
    }

    // 2. Decay reward trace, then inject new reward.
    let rew_decay = decay_factor(dt_ms, params.reward_tau_ms);
    state.reward_trace = state.reward_trace * rew_decay + reward;

    // 3 + 4. Credit spike-pair contributions into the eligibility trace.
    //
    //    CSR is post-major: each row belongs to one post-neuron, and each
    //    entry in the row names one pre-neuron. A single pass over rows
    //    covers both pre-driven and post-driven updates.
    //
    //    **Ordering is load-bearing**: we compute pair deltas against the
    //    *prior* last-spike tables, then update the tables. Otherwise,
    //    when both pre and post fire on the same step, both last-spike
    //    entries get set to `t_ms` first and `stdp_weight_delta(t, t) = 0`
    //    erases the prior pair we should have credited.
    //
    //    Specifically:
    //      pre-i fires at t ⇒ pair with last_post[j] *before* this step
    //                        (classic "post-prior-then-pre-now" = LTD if
    //                        last_post < t; sign handled inside stdp).
    //      post-j fires at t ⇒ pair with last_pre[i] *before* this step.
    //
    //    Updating last_pre after the pre-scan but before the post-scan
    //    also means a truly simultaneous spike on the same synapse gives
    //    the post-scan a pair with `last_pre = t`, yielding Δt = 0 → 0.
    //    That's the V1 convention and matches `stdp_weight_delta`'s tie
    //    rule.

    // --- Pre-spike scan (uses OLD last_post table) ---
    for (j_post, &last_post_j) in state.last_post_spike_ms.iter().enumerate() {
        let row_start = csr.row_ptr[j_post] as usize;
        let row_end = csr.row_ptr[j_post + 1] as usize;
        if row_start > row_end || row_end > n_syn {
            continue;
        }
        for syn in row_start..row_end {
            let i_pre = csr.col_idx[syn] as usize;
            if i_pre >= n_pre {
                continue;
            }
            if pre_spikes[i_pre] == 0 {
                continue;
            }
            let dw = stdp_weight_delta(t_ms, last_post_j, &params.stdp);
            state.eligibility[syn] += dw;
        }
    }

    // Now promote this step's pre-spikes into the last-pre table.
    for (i_pre, &spk) in pre_spikes.iter().enumerate() {
        if spk != 0 {
            state.last_pre_spike_ms[i_pre] = t_ms;
        }
    }

    // --- Post-spike scan (uses last_pre table that now reflects this step) ---
    // With last_pre updated, if pre-i also fired this step, stdp(t, t) = 0.
    // If pre-i fired earlier, stdp(t_earlier, t) gives the LTP contribution.
    for (j_post, &post_spk) in post_spikes.iter().enumerate() {
        if post_spk == 0 {
            continue;
        }
        let row_start = csr.row_ptr[j_post] as usize;
        let row_end = csr.row_ptr[j_post + 1] as usize;
        if row_start > row_end || row_end > n_syn {
            continue;
        }
        for syn in row_start..row_end {
            let i_pre = csr.col_idx[syn] as usize;
            if i_pre >= n_pre {
                continue;
            }
            let dw = stdp_weight_delta(state.last_pre_spike_ms[i_pre], t_ms, &params.stdp);
            state.eligibility[syn] += dw;
        }
    }

    // Promote this step's post-spikes into the last-post table.
    for (j_post, &spk) in post_spikes.iter().enumerate() {
        if spk != 0 {
            state.last_post_spike_ms[j_post] = t_ms;
        }
    }

    // 5. Apply reward modulation — but only once the warmup gate is open.
    //    During warmup, traces still advance (so the first post-gate step
    //    isn't cold); weights never move. This is the Hebbian-runaway gate.
    let mut n_moved: u32 = 0;
    if rate_samples_ready(state.rate_samples, params.warmup_samples) {
        let modulator = state.reward_trace - effective_baseline(state, params);
        // `modulator == 0` short-circuits: reward-trace is exactly at baseline,
        // so there's nothing to apply. This is the "zero reward ⇒ no update"
        // invariant the V1 `reward_modulation_multiplicative` regression holds.
        if modulator != 0.0 {
            for (syn, w) in csr.weights.iter_mut().enumerate() {
                let dw = modulator * state.eligibility[syn];
                if dw == 0.0 {
                    continue;
                }
                let new_w = (*w + dw).clamp(params.w_min, params.w_max);
                if new_w != *w {
                    *w = new_w;
                    n_moved += 1;
                }
            }
        }
    }

    // 6. Adaptive baseline tracks the reward trace (no-op when α = 0), then
    //    the rate-samples counter increments regardless of gate state.
    update_baseline(state, params);
    state.rate_samples = state.rate_samples.saturating_add(1);

    n_moved
}

// -------------------------------------------------------------------------
// GPU backend (feature-gated) — Phase 9d
// -------------------------------------------------------------------------

#[cfg(feature = "cuda")]
pub use gpu::RstdpGpu;

/// NVRTC kernel source — three R-STDP kernels in one module so they
/// share a single compile/load. Numeric semantics match
/// [`step_rstdp`] exactly; the equivalence test below is the
/// regression gate against drift.
///
/// Kernels:
///   - `rstdp_decay_eligibility` — `e[k] *= elig_decay`
///   - `rstdp_accumulate_pairs`  — pre+post scan combined; updates
///     eligibility AND last-spike tables. Per-row parallelism (one
///     thread per post-neuron); stays serial within a row to preserve
///     the V1 ordering invariant (last_pre updated after pre-scan,
///     before post-scan, on the SAME row).
///   - `rstdp_apply_reward`      — `dw = modulator * e[k]; w[k] =
///     clamp(w[k] + dw, w_min, w_max)`. Atomic counter for n_moved.
#[cfg(feature = "cuda")]
const RSTDP_KERNEL_SRC: &str = r#"
extern "C" __global__ void rstdp_decay_eligibility(
    float* eligibility,
    int n_syn,
    float elig_decay
) {
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= n_syn) return;
    eligibility[k] *= elig_decay;
}

// Branchless STDP kernel — same math as crates/plasticity stdp_weight_delta.
__device__ __forceinline__ float stdp_pair(
    float pre_t, float post_t,
    float a_plus, float a_minus, float tau_plus, float tau_minus
) {
    if (tau_plus <= 0.0f || tau_minus <= 0.0f) return 0.0f;
    float dt = post_t - pre_t;
    if (dt > 0.0f) {
        return a_plus * __expf(-dt / tau_plus);
    } else if (dt < 0.0f) {
        return -a_minus * __expf(dt / tau_minus);
    }
    return 0.0f;
}

extern "C" __global__ void rstdp_accumulate_pairs(
    float* eligibility,
    float* last_pre_spike_ms,
    float* last_post_spike_ms,
    const unsigned int* row_ptr,
    const unsigned int* col_idx,
    const unsigned char* pre_spikes,
    const unsigned char* post_spikes,
    int n_post,
    int n_pre,
    int n_syn,
    float t_ms,
    float a_plus, float a_minus,
    float tau_plus, float tau_minus
) {
    int j_post = blockIdx.x * blockDim.x + threadIdx.x;
    if (j_post >= n_post) return;

    unsigned int row_start = row_ptr[j_post];
    unsigned int row_end   = row_ptr[j_post + 1];
    if (row_start > row_end || (int)row_end > n_syn) return;

    // Pre-spike scan — uses OLD last_post[j_post].
    float last_post_j = last_post_spike_ms[j_post];
    for (unsigned int syn = row_start; syn < row_end; ++syn) {
        unsigned int i_pre = col_idx[syn];
        if ((int)i_pre >= n_pre) continue;
        if (pre_spikes[i_pre] == 0) continue;
        eligibility[syn] += stdp_pair(t_ms, last_post_j,
                                      a_plus, a_minus, tau_plus, tau_minus);
    }
    // Promote pre-spike timestamps. Many threads write the same
    // last_pre_spike_ms[i_pre] (one per post-row that contains pre i),
    // but they all write the same value (t_ms), so the race is benign.
    for (unsigned int syn = row_start; syn < row_end; ++syn) {
        unsigned int i_pre = col_idx[syn];
        if ((int)i_pre >= n_pre) continue;
        if (pre_spikes[i_pre] != 0) {
            last_pre_spike_ms[i_pre] = t_ms;
        }
    }

    // Post-spike scan — uses last_pre table (now containing this step's
    // promotions).
    if (post_spikes[j_post] != 0) {
        for (unsigned int syn = row_start; syn < row_end; ++syn) {
            unsigned int i_pre = col_idx[syn];
            if ((int)i_pre >= n_pre) continue;
            float last_pre_i = last_pre_spike_ms[i_pre];
            eligibility[syn] += stdp_pair(last_pre_i, t_ms,
                                          a_plus, a_minus, tau_plus, tau_minus);
        }
        last_post_spike_ms[j_post] = t_ms;
    }
}

extern "C" __global__ void rstdp_apply_reward(
    float* weights,
    const float* eligibility,
    int n_syn,
    float modulator,
    float w_min, float w_max,
    unsigned int* n_moved
) {
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= n_syn) return;
    float dw = modulator * eligibility[k];
    if (dw == 0.0f) return;
    float new_w = weights[k] + dw;
    if (new_w < w_min) new_w = w_min;
    if (new_w > w_max) new_w = w_max;
    if (new_w != weights[k]) {
        weights[k] = new_w;
        atomicAdd(n_moved, 1u);
    }
}
"#;

#[cfg(feature = "cuda")]
mod gpu {
    use std::sync::Arc;

    use cudarc::driver::{
        CudaContext, CudaFunction, CudaModule, CudaSlice, CudaStream, LaunchConfig, PushKernelArg,
    };
    use nimcp_gpu::GpuError;

    use super::{RSTDP_KERNEL_SRC, RstdpParams, RstdpState, rate_samples_ready};
    use crate::csr::CsrSynapses;

    fn cuda_err<E: std::fmt::Debug>(e: E) -> GpuError {
        GpuError::Cuda(format!("{e:?}"))
    }

    /// GPU-resident R-STDP state + the three compiled kernels.
    ///
    /// Eligibility, last-pre/last-post timestamp tables, and the CSR
    /// topology (`row_ptr`, `col_idx`, `weights`) all live on device
    /// across calls. Per step only the spike vectors and the scalar
    /// `n_moved` counter cross the bus.
    ///
    /// This is the Phase 9d device-side equivalent of [`super::step_rstdp`].
    pub struct RstdpGpu {
        n_syn: u32,
        n_pre: u32,
        n_post: u32,

        // Persistent device state (mirrors RstdpState).
        eligibility: CudaSlice<f32>,
        last_pre_spike_ms: CudaSlice<f32>,
        last_post_spike_ms: CudaSlice<f32>,

        // CSR topology (uploaded once, weights re-uploaded if homeostatic
        // moved them between steps — same convention as CsrGpu).
        row_ptr: CudaSlice<u32>,
        col_idx: CudaSlice<u32>,
        weights: CudaSlice<f32>,

        // Per-step scratch.
        pre_spikes_buf: CudaSlice<u8>,
        post_spikes_buf: CudaSlice<u8>,
        n_moved_dev: CudaSlice<u32>,

        ctx: Arc<CudaContext>,
        stream: Arc<CudaStream>,
        #[allow(dead_code)]
        module: Arc<CudaModule>,
        decay_kernel: CudaFunction,
        pair_kernel: CudaFunction,
        apply_kernel: CudaFunction,
    }

    impl std::fmt::Debug for RstdpGpu {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            f.debug_struct("RstdpGpu")
                .field("n_syn", &self.n_syn)
                .field("n_pre", &self.n_pre)
                .field("n_post", &self.n_post)
                .finish_non_exhaustive()
        }
    }

    impl RstdpGpu {
        /// Upload CSR topology + initialise device-side R-STDP state.
        ///
        /// Reuses the caller-supplied [`CudaContext`] (Phase 9b shared
        /// context contract). State buffers start at zero / `-inf`
        /// matching [`RstdpState::new`].
        pub fn new_with_context(
            ctx: Arc<CudaContext>,
            csr: &CsrSynapses,
        ) -> Result<Self, GpuError> {
            let n_syn = csr.weights.len() as u32;
            let n_pre = csr.n_pre;
            let n_post = csr.n_post;
            if n_syn == 0 || n_pre == 0 || n_post == 0 {
                return Err(GpuError::Cuda("RstdpGpu::new: empty CSR".into()));
            }
            let stream = ctx.default_stream();
            let ptx = cudarc::nvrtc::compile_ptx(RSTDP_KERNEL_SRC).map_err(cuda_err)?;
            let module = ctx.load_module(ptx).map_err(cuda_err)?;
            let decay_kernel = module
                .load_function("rstdp_decay_eligibility")
                .map_err(cuda_err)?;
            let pair_kernel = module
                .load_function("rstdp_accumulate_pairs")
                .map_err(cuda_err)?;
            let apply_kernel = module
                .load_function("rstdp_apply_reward")
                .map_err(cuda_err)?;

            let eligibility: CudaSlice<f32> = stream
                .alloc_zeros::<f32>(n_syn as usize)
                .map_err(cuda_err)?;
            let last_pre_init: Vec<f32> = vec![f32::NEG_INFINITY; n_pre as usize];
            let last_post_init: Vec<f32> = vec![f32::NEG_INFINITY; n_post as usize];
            let last_pre_spike_ms = stream.memcpy_stod(&last_pre_init).map_err(cuda_err)?;
            let last_post_spike_ms = stream.memcpy_stod(&last_post_init).map_err(cuda_err)?;

            let row_ptr = stream.memcpy_stod(&csr.row_ptr).map_err(cuda_err)?;
            let col_idx = stream.memcpy_stod(&csr.col_idx).map_err(cuda_err)?;
            let weights = stream.memcpy_stod(&csr.weights).map_err(cuda_err)?;

            let pre_spikes_buf: CudaSlice<u8> =
                stream.alloc_zeros::<u8>(n_pre as usize).map_err(cuda_err)?;
            let post_spikes_buf: CudaSlice<u8> = stream
                .alloc_zeros::<u8>(n_post as usize)
                .map_err(cuda_err)?;
            let n_moved_dev: CudaSlice<u32> = stream.alloc_zeros::<u32>(1).map_err(cuda_err)?;

            tracing::info!(n_syn, n_pre, n_post, "rstdp gpu buffers allocated");

            Ok(Self {
                n_syn,
                n_pre,
                n_post,
                eligibility,
                last_pre_spike_ms,
                last_post_spike_ms,
                row_ptr,
                col_idx,
                weights,
                pre_spikes_buf,
                post_spikes_buf,
                n_moved_dev,
                ctx,
                stream,
                module,
                decay_kernel,
                pair_kernel,
                apply_kernel,
            })
        }

        /// Mirror [`RstdpGpu::new_with_context`] without a pre-existing context.
        pub fn new(csr: &CsrSynapses) -> Result<Self, GpuError> {
            let ctx = CudaContext::new(0).map_err(cuda_err)?;
            Self::new_with_context(ctx, csr)
        }

        /// Upload weights back to device — call after any CPU-side
        /// homeostatic scaling step, mirroring CsrGpu's contract.
        pub fn upload_weights(&mut self, weights_host: &[f32]) -> Result<(), GpuError> {
            if weights_host.len() != self.n_syn as usize {
                return Err(GpuError::Cuda(format!(
                    "RstdpGpu::upload_weights: len={} expected {}",
                    weights_host.len(),
                    self.n_syn
                )));
            }
            self.stream
                .memcpy_htod(weights_host, &mut self.weights)
                .map_err(cuda_err)
        }

        /// Borrow the CUDA context (shared with sibling GPU subsystems).
        #[must_use]
        pub fn context(&self) -> &Arc<CudaContext> {
            &self.ctx
        }

        /// Run one R-STDP update on device. Mirrors [`super::step_rstdp`]:
        /// decays eligibility → accumulates pre+post pair credits →
        /// (optionally) applies reward modulation through the warmup gate
        /// → bumps `rate_samples`. Returns the count of weights that
        /// actually moved.
        ///
        /// `state` is the **CPU-side** [`RstdpState`] handle: only its
        /// scalar fields (`reward_trace`, `rate_samples`) are touched here
        /// — the GPU owns the per-synapse and per-neuron arrays. Keep the
        /// CPU handle around so checkpoint snapshots can read these
        /// scalars without an extra device hop.
        // HOT PATH: called every SNN tick that delivers a reward.
        #[allow(clippy::too_many_arguments)]
        #[allow(clippy::float_cmp)] // short-circuit guards, not fuzzy compares
        pub fn step(
            &mut self,
            state: &mut RstdpState,
            pre_spikes: &[u8],
            post_spikes: &[u8],
            reward: f32,
            t_ms: f32,
            dt_ms: f32,
            params: &RstdpParams,
        ) -> Result<u32, GpuError> {
            if pre_spikes.len() != self.n_pre as usize || post_spikes.len() != self.n_post as usize
            {
                return Err(GpuError::Cuda(format!(
                    "RstdpGpu::step: spike len mismatch (pre={} expected={}, post={} expected={})",
                    pre_spikes.len(),
                    self.n_pre,
                    post_spikes.len(),
                    self.n_post,
                )));
            }

            // Decay factors — same defensive short-circuit as CPU path.
            let elig_decay = if params.eligibility_tau_ms <= 0.0 {
                1.0
            } else if !dt_ms.is_finite() {
                0.0
            } else {
                (-dt_ms / params.eligibility_tau_ms).exp()
            };
            let rew_decay = if params.reward_tau_ms <= 0.0 {
                1.0
            } else if !dt_ms.is_finite() {
                0.0
            } else {
                (-dt_ms / params.reward_tau_ms).exp()
            };

            // 1. Decay eligibility traces (skip when factor == 1.0).
            if elig_decay != 1.0 {
                let cfg = LaunchConfig::for_num_elems(self.n_syn);
                let mut builder = self.stream.launch_builder(&self.decay_kernel);
                builder.arg(&mut self.eligibility);
                let n_syn_i32 = self.n_syn as i32;
                builder.arg(&n_syn_i32);
                builder.arg(&elig_decay);
                // SAFETY: kernel signature
                //   (float* eligibility, int n_syn, float decay)
                // matches the three args pushed above.
                unsafe { builder.launch(cfg) }.map_err(cuda_err)?;
            }

            // 2. Decay reward trace + inject new reward (CPU scalar).
            state.reward_trace = state.reward_trace * rew_decay + reward;

            // 3 + 4. Upload spike vectors + run pair-credit kernel.
            self.stream
                .memcpy_htod(pre_spikes, &mut self.pre_spikes_buf)
                .map_err(cuda_err)?;
            self.stream
                .memcpy_htod(post_spikes, &mut self.post_spikes_buf)
                .map_err(cuda_err)?;

            let cfg_post = LaunchConfig::for_num_elems(self.n_post);
            let mut builder = self.stream.launch_builder(&self.pair_kernel);
            builder.arg(&mut self.eligibility);
            builder.arg(&mut self.last_pre_spike_ms);
            builder.arg(&mut self.last_post_spike_ms);
            builder.arg(&self.row_ptr);
            builder.arg(&self.col_idx);
            builder.arg(&self.pre_spikes_buf);
            builder.arg(&self.post_spikes_buf);
            let n_post_i32 = self.n_post as i32;
            let n_pre_i32 = self.n_pre as i32;
            let n_syn_i32 = self.n_syn as i32;
            builder.arg(&n_post_i32);
            builder.arg(&n_pre_i32);
            builder.arg(&n_syn_i32);
            builder.arg(&t_ms);
            builder.arg(&params.stdp.a_plus);
            builder.arg(&params.stdp.a_minus);
            builder.arg(&params.stdp.tau_plus_ms);
            builder.arg(&params.stdp.tau_minus_ms);
            // SAFETY: kernel signature has 14 args; the 14 builder.arg
            // calls above match in order + type. The if-guards inside
            // the kernel keep all writes within the device buffer
            // bounds (eligibility:n_syn, last_pre:n_pre,
            // last_post:n_post).
            unsafe { builder.launch(cfg_post) }.map_err(cuda_err)?;

            // 5. Apply reward modulation iff warmup gate is open.
            let mut n_moved: u32 = 0;
            if rate_samples_ready(state.rate_samples, params.warmup_samples) {
                let modulator = state.reward_trace - effective_baseline(state, params);
                if modulator != 0.0 {
                    // Reset the device-side counter to zero before launch.
                    let zero_host: [u32; 1] = [0];
                    self.stream
                        .memcpy_htod(&zero_host, &mut self.n_moved_dev)
                        .map_err(cuda_err)?;

                    let cfg_syn = LaunchConfig::for_num_elems(self.n_syn);
                    let mut builder = self.stream.launch_builder(&self.apply_kernel);
                    builder.arg(&mut self.weights);
                    builder.arg(&self.eligibility);
                    let n_syn_i32 = self.n_syn as i32;
                    builder.arg(&n_syn_i32);
                    builder.arg(&modulator);
                    builder.arg(&params.w_min);
                    builder.arg(&params.w_max);
                    builder.arg(&mut self.n_moved_dev);
                    // SAFETY: kernel signature has 7 args; matches the 7
                    // pushes above. atomicAdd writes to n_moved_dev are
                    // bounds-safe (single u32 slot).
                    unsafe { builder.launch(cfg_syn) }.map_err(cuda_err)?;

                    let host_n_moved = self
                        .stream
                        .memcpy_dtov(&self.n_moved_dev)
                        .map_err(cuda_err)?;
                    n_moved = host_n_moved.first().copied().unwrap_or(0);
                }
            }

            // 6. Adaptive baseline + rate counter (matches CPU contract).
            update_baseline(state, params);
            state.rate_samples = state.rate_samples.saturating_add(1);
            Ok(n_moved)
        }

        /// Download the current device weights into the supplied host
        /// buffer. Mirrors [`crate::csr::CsrGpu::download_weights`] —
        /// call after a training run to checkpoint final weights, or
        /// inside an equivalence test.
        pub fn download_weights(&self, out: &mut Vec<f32>) -> Result<(), GpuError> {
            let host = self.stream.memcpy_dtov(&self.weights).map_err(cuda_err)?;
            out.clear();
            out.extend_from_slice(&host);
            Ok(())
        }

        /// Download the current eligibility trace into the host buffer.
        /// Test / debug helper; not on the hot path.
        pub fn download_eligibility(&self, out: &mut Vec<f32>) -> Result<(), GpuError> {
            let host = self
                .stream
                .memcpy_dtov(&self.eligibility)
                .map_err(cuda_err)?;
            out.clear();
            out.extend_from_slice(&host);
            Ok(())
        }

        /// Number of synapses on this edge.
        #[must_use]
        pub fn n_syn(&self) -> u32 {
            self.n_syn
        }
    }

    #[cfg(test)]
    mod gpu_tests {
        use super::*;
        use crate::csr::PopulationId;
        use crate::rstdp::step_rstdp;

        fn fc_csr(n_pre: u32, n_post: u32, w0: f32) -> CsrSynapses {
            let mut triples: Vec<(u32, u32, f32)> = Vec::with_capacity((n_pre * n_post) as usize);
            for post in 0..n_post {
                for pre in 0..n_pre {
                    triples.push((pre, post, w0));
                }
            }
            CsrSynapses::from_triples(PopulationId(0), PopulationId(1), n_pre, n_post, triples)
                .expect("fc_csr")
        }

        // The GPU equivalence test is a strong contract — same inputs,
        // same final weights to within float tolerance. Skip when no
        // CUDA device is reachable so CI on CPU-only runners stays green.
        #[test]
        fn cpu_gpu_equivalence_after_warmup() {
            let n_pre = 4;
            let n_post = 4;

            let mut csr_cpu = fc_csr(n_pre, n_post, 0.5);
            let mut csr_gpu_host = fc_csr(n_pre, n_post, 0.5);
            let mut state_cpu = RstdpState::new(csr_cpu.weights.len(), n_pre, n_post);
            let mut state_gpu = RstdpState::new(csr_cpu.weights.len(), n_pre, n_post);
            let params = RstdpParams {
                warmup_samples: 0, // skip the gate to focus on math
                ..RstdpParams::default()
            };

            let mut gpu = match RstdpGpu::new(&csr_gpu_host) {
                Ok(g) => g,
                Err(e) => {
                    eprintln!("[skipping] no CUDA device for rstdp gpu equivalence: {e:?}");
                    return;
                }
            };

            // Drive a deterministic spike pattern over 10 steps.
            let pre_seq = [
                [1, 0, 1, 0],
                [0, 1, 1, 0],
                [1, 1, 0, 0],
                [0, 0, 1, 1],
                [1, 0, 0, 1],
                [0, 1, 0, 1],
                [1, 1, 1, 0],
                [0, 0, 0, 1],
                [1, 0, 1, 1],
                [0, 1, 1, 1],
            ];
            let post_seq = [
                [0, 1, 0, 1],
                [1, 0, 0, 1],
                [0, 0, 1, 1],
                [1, 1, 0, 0],
                [0, 1, 1, 0],
                [1, 0, 1, 0],
                [0, 0, 1, 1],
                [1, 1, 0, 0],
                [0, 1, 0, 1],
                [1, 0, 1, 0],
            ];

            for step in 0..10 {
                let t_ms = step as f32;
                let _ = step_rstdp(
                    &mut csr_cpu,
                    &mut state_cpu,
                    &pre_seq[step],
                    &post_seq[step],
                    0.5,
                    t_ms,
                    1.0,
                    &params,
                );
                let _ = gpu
                    .step(
                        &mut state_gpu,
                        &pre_seq[step],
                        &post_seq[step],
                        0.5,
                        t_ms,
                        1.0,
                        &params,
                    )
                    .expect("gpu step");
            }

            let mut gpu_weights: Vec<f32> = Vec::new();
            gpu.download_weights(&mut gpu_weights).expect("download");
            assert_eq!(gpu_weights.len(), csr_cpu.weights.len());
            for (i, (g, c)) in gpu_weights.iter().zip(csr_cpu.weights.iter()).enumerate() {
                assert!(
                    (g - c).abs() < 1e-4,
                    "weight {i}: gpu={g} cpu={c} (diff {})",
                    (g - c).abs()
                );
            }
            // Reward trace + rate samples should also be in lockstep.
            assert!((state_cpu.reward_trace - state_gpu.reward_trace).abs() < 1e-4);
            assert_eq!(state_cpu.rate_samples, state_gpu.rate_samples);
        }

        #[test]
        fn warmup_gate_blocks_weight_motion() {
            let n_pre = 2;
            let n_post = 2;
            let csr = fc_csr(n_pre, n_post, 0.5);
            let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
            let params = RstdpParams::default();

            let mut gpu = match RstdpGpu::new(&csr) {
                Ok(g) => g,
                Err(_) => return,
            };
            for step in 0..50 {
                let n_moved = gpu
                    .step(&mut state, &[1, 1], &[1, 1], 1.0, step as f32, 1.0, &params)
                    .expect("gpu step");
                assert_eq!(n_moved, 0, "warmup gate must block movement");
            }
            let mut w: Vec<f32> = Vec::new();
            gpu.download_weights(&mut w).expect("download");
            for (i, &x) in w.iter().enumerate() {
                assert!(
                    (x - 0.5).abs() < 1e-6,
                    "weight {i} moved during warmup: {x}"
                );
            }
        }
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)] // exact-equality asserts are deliberate in regressions
mod tests {
    //! # R-STDP regression suite
    //!
    //! Each test encodes a V1 lesson (see module docs for context). The
    //! suite is intentionally dense — these behaviors are load-bearing.

    use super::*;
    use crate::csr::{CsrSynapses, PopulationId};

    /// Build a fully-connected `n_pre × n_post` CSR for tests, with every
    /// synapse initialised to `w0`. Triples are generated in post-major
    /// order so the resulting `col_idx` within each row is [0, 1, ..., n_pre).
    fn fc_csr(n_pre: u32, n_post: u32, w0: f32) -> CsrSynapses {
        let mut triples: Vec<(u32, u32, f32)> = Vec::with_capacity((n_pre * n_post) as usize);
        for post in 0..n_post {
            for pre in 0..n_pre {
                triples.push((pre, post, w0));
            }
        }
        CsrSynapses::from_triples(PopulationId(0), PopulationId(1), n_pre, n_post, triples)
            .expect("fc_csr: from_triples")
    }

    fn default_params() -> RstdpParams {
        RstdpParams::default()
    }

    /// The default threshold matches [`DEFAULT_WARMUP_THRESHOLD`] from
    /// `nimcp-plasticity`. If either number moves, the other must move too.
    #[test]
    fn default_params_have_sensible_warmup() {
        assert_eq!(RstdpParams::default().warmup_samples, 100);
        assert_eq!(
            RstdpParams::default().warmup_samples,
            DEFAULT_WARMUP_THRESHOLD
        );
    }

    /// V1 bug #1: R-STDP fired from `t = 0`, producing Hebbian runaway on
    /// fresh init. The warmup gate must suppress weight motion for the
    /// first `warmup_samples` calls.
    #[test]
    fn warmup_blocks_early_updates() {
        let n_pre = 2;
        let n_post = 2;
        let mut csr = fc_csr(n_pre, n_post, 0.5);
        let w_before = csr.weights.clone();

        let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
        let params = default_params();

        // Spikes on both ends + positive reward for 99 consecutive steps.
        // Without the gate, V1 would potentiate to saturation; V2 must not.
        for step in 0..99 {
            let _ = step_rstdp(
                &mut csr,
                &mut state,
                &[1, 1],
                &[1, 1],
                1.0,
                step as f32,
                1.0,
                &params,
            );
            assert_eq!(
                csr.weights, w_before,
                "weights moved during warmup at step {step}; V1 runaway regression"
            );
        }
        assert!(!state.warmup_done(&params));

        // Step 100 — gate opens (inclusive ≥ threshold).
        let _ = step_rstdp(
            &mut csr,
            &mut state,
            &[1, 1],
            &[1, 1],
            1.0,
            99.0,
            1.0,
            &params,
        );
        assert!(state.warmup_done(&params));
    }

    /// Eligibility traces must decay exponentially with `τ_e`. Without this,
    /// a stale pre/post pair from 10 seconds ago would contribute to the
    /// next reward modulation — the V1 "trace explosion" bug.
    #[test]
    fn eligibility_trace_decays() {
        let n_pre = 1;
        let n_post = 1;
        let mut csr = fc_csr(n_pre, n_post, 0.5);
        let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
        let params = RstdpParams {
            warmup_samples: 0, // focus on trace behavior
            ..default_params()
        };

        // Seed eligibility manually: one pre-spike at t=0, then a post-spike
        // 5 ms later. With reward = 0 the trace will accumulate without
        // driving weight updates.
        // Step 1: pre-spike at t=0, no post → nothing yet (last_post is -inf,
        // so stdp returns 0; but last_pre is recorded at t=0).
        step_rstdp(&mut csr, &mut state, &[1], &[0], 0.0, 0.0, 1.0, &params);
        // Step 2: post-spike at t=5 → LTP (pre leads post by 5ms).
        step_rstdp(&mut csr, &mut state, &[0], &[1], 0.0, 5.0, 5.0, &params);
        let e_initial = state.eligibility[0];
        assert!(
            e_initial > 0.0,
            "eligibility should have grown from pre→post pair, got {e_initial}"
        );

        // Advance 200 ms with no spikes and no reward. With τ_e = 100 ms,
        // eligibility should drop by roughly exp(-200/100) ≈ 0.135.
        step_rstdp(&mut csr, &mut state, &[0], &[0], 0.0, 205.0, 200.0, &params);
        let e_after = state.eligibility[0];
        let ratio = e_after / e_initial;
        let expected = (-200.0_f32 / 100.0).exp();
        assert!(
            (ratio - expected).abs() < 1e-4,
            "eligibility decay ratio {ratio}, expected {expected}"
        );
    }

    /// V1 invariant: reward modulation is multiplicative. If reward trace is
    /// zero, no update happens — not a default-zero update, literally zero.
    #[test]
    fn reward_modulation_multiplicative() {
        let n_pre = 1;
        let n_post = 1;
        let mut csr = fc_csr(n_pre, n_post, 0.5);
        let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
        let params = RstdpParams {
            warmup_samples: 0,
            reward_tau_ms: f32::INFINITY, // keep reward_trace stable
            ..default_params()
        };

        // Seed an eligibility trace via LTP-ordered spikes, still reward = 0.
        step_rstdp(&mut csr, &mut state, &[1], &[0], 0.0, 0.0, 1.0, &params);
        step_rstdp(&mut csr, &mut state, &[0], &[1], 0.0, 5.0, 5.0, &params);
        assert!(state.eligibility[0] > 0.0);
        // Despite positive eligibility, reward has been zero throughout, so
        // no weight motion may happen.
        assert_eq!(
            csr.weights[0], 0.5,
            "zero reward must produce zero Δw; got w = {}",
            csr.weights[0]
        );

        // Now inject reward = 1 and verify Δw > 0 (LTP).
        step_rstdp(&mut csr, &mut state, &[0], &[0], 1.0, 6.0, 1.0, &params);
        assert!(
            csr.weights[0] > 0.5,
            "positive reward + positive eligibility must potentiate; w = {}",
            csr.weights[0]
        );
    }

    /// TD-style baseline: `r == baseline` must leave weights untouched even
    /// with positive eligibility.
    #[test]
    fn td_baseline_subtracts() {
        let n_pre = 1;
        let n_post = 1;
        let mut csr = fc_csr(n_pre, n_post, 0.5);
        let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);

        // Setup params — baseline = 0 so the zero-reward setup steps don't
        // drive any modulator. (A non-zero baseline with zero reward would
        // produce `modulator = -baseline`, which is exactly *not* what this
        // test is trying to isolate.)
        let params_setup = RstdpParams {
            warmup_samples: 0,
            baseline_reward: 0.0,
            reward_tau_ms: f32::INFINITY,
            ..default_params()
        };
        // Test-moment params — baseline = 1 so reward = 1 gives modulator 0.
        let params_test = RstdpParams {
            baseline_reward: 1.0,
            ..params_setup
        };

        // Build positive eligibility.
        step_rstdp(
            &mut csr,
            &mut state,
            &[1],
            &[0],
            0.0,
            0.0,
            1.0,
            &params_setup,
        );
        step_rstdp(
            &mut csr,
            &mut state,
            &[0],
            &[1],
            0.0,
            5.0,
            5.0,
            &params_setup,
        );
        assert!(state.eligibility[0] > 0.0);
        // Setup must not have moved weights (zero reward, zero baseline).
        assert_eq!(
            csr.weights[0], 0.5,
            "setup moved w unexpectedly; got w = {}",
            csr.weights[0]
        );

        // reward == baseline ⇒ net modulator == 0 ⇒ no Δw.
        step_rstdp(
            &mut csr,
            &mut state,
            &[0],
            &[0],
            1.0, // raw reward equals baseline
            6.0,
            1.0,
            &params_test,
        );
        assert_eq!(
            csr.weights[0], 0.5,
            "reward == baseline must leave w unchanged; got w = {}",
            csr.weights[0]
        );
    }

    /// Weights must clamp to `[w_min, w_max]`. Hammering with huge reward
    /// may not overshoot.
    ///
    /// Separately verify both bounds: drive LTP hard to saturate upward,
    /// then flip to LTD to saturate downward. A single monotone regime
    /// could mask a clamp that's only half-implemented.
    #[test]
    fn weight_clipped_to_bounds() {
        let n_pre = 1;
        let n_post = 1;
        let params_setup = RstdpParams {
            warmup_samples: 0,
            w_min: 0.0,
            w_max: 1.0,
            reward_tau_ms: f32::INFINITY,
            eligibility_tau_ms: f32::INFINITY,
            baseline_reward: 0.0,
            ..default_params()
        };

        // --- Up-clamp check -------------------------------------------------
        {
            let mut csr = fc_csr(n_pre, n_post, 0.5);
            let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
            // Seed positive eligibility with one clean pre→post LTP pair,
            // reward = 0 so the seed itself doesn't move w.
            step_rstdp(
                &mut csr,
                &mut state,
                &[1],
                &[0],
                0.0,
                0.0,
                1.0,
                &params_setup,
            );
            step_rstdp(
                &mut csr,
                &mut state,
                &[0],
                &[1],
                0.0,
                1.0,
                1.0,
                &params_setup,
            );
            assert!(
                state.eligibility[0] > 0.0,
                "seed eligibility must be positive"
            );
            assert_eq!(csr.weights[0], 0.5, "seed must not move w");

            // Now hammer with huge positive reward — pure apply, no new pairs.
            for step in 0..50 {
                step_rstdp(
                    &mut csr,
                    &mut state,
                    &[0],
                    &[0],
                    1_000.0,
                    (step + 2) as f32,
                    1.0,
                    &params_setup,
                );
            }
            assert!(
                csr.weights[0] <= params_setup.w_max + 1e-6,
                "up-clamp violated: {}",
                csr.weights[0]
            );
            assert!(
                (csr.weights[0] - params_setup.w_max).abs() < 1e-6,
                "did not saturate upward: {}",
                csr.weights[0]
            );
        }

        // --- Down-clamp check ---------------------------------------------
        {
            let mut csr = fc_csr(n_pre, n_post, 0.5);
            let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
            // Seed negative eligibility with post→pre ordering (LTD), reward 0.
            step_rstdp(
                &mut csr,
                &mut state,
                &[0],
                &[1],
                0.0,
                0.0,
                1.0,
                &params_setup,
            );
            step_rstdp(
                &mut csr,
                &mut state,
                &[1],
                &[0],
                0.0,
                1.0,
                1.0,
                &params_setup,
            );
            assert!(
                state.eligibility[0] < 0.0,
                "seed eligibility must be negative"
            );

            // Hammer with huge positive reward; dw = + · (−) is large-negative,
            // pushing w toward w_min.
            for step in 0..50 {
                step_rstdp(
                    &mut csr,
                    &mut state,
                    &[0],
                    &[0],
                    1_000.0,
                    (step + 2) as f32,
                    1.0,
                    &params_setup,
                );
            }
            assert!(
                csr.weights[0] >= params_setup.w_min - 1e-6,
                "down-clamp violated: {}",
                csr.weights[0]
            );
            assert!(
                (csr.weights[0] - params_setup.w_min).abs() < 1e-6,
                "did not saturate downward: {}",
                csr.weights[0]
            );
        }
    }

    /// Two fresh states driven by identical spikes + reward produce
    /// bit-identical weights.
    #[test]
    fn step_is_deterministic() {
        let n_pre = 4;
        let n_post = 4;
        let mut csr_a = fc_csr(n_pre, n_post, 0.5);
        let mut csr_b = fc_csr(n_pre, n_post, 0.5);
        let mut state_a = RstdpState::new(csr_a.weights.len(), n_pre, n_post);
        let mut state_b = RstdpState::new(csr_b.weights.len(), n_pre, n_post);
        let params = RstdpParams {
            warmup_samples: 0,
            ..default_params()
        };

        // A mix of sparse spike vectors. Include a negative reward to make
        // sure sign-sensitivity is deterministic too.
        let patterns: &[(&[u8], &[u8], f32)] = &[
            (&[1, 0, 0, 1], &[0, 1, 0, 0], 0.3),
            (&[0, 1, 1, 0], &[1, 0, 0, 1], 0.0),
            (&[1, 1, 0, 0], &[0, 0, 1, 0], 0.7),
            (&[0, 0, 1, 1], &[1, 1, 0, 0], -0.2),
        ];
        for (step, (pre, post, reward)) in patterns.iter().cycle().take(50).enumerate() {
            let t = step as f32;
            step_rstdp(
                &mut csr_a,
                &mut state_a,
                pre,
                post,
                *reward,
                t,
                1.0,
                &params,
            );
            step_rstdp(
                &mut csr_b,
                &mut state_b,
                pre,
                post,
                *reward,
                t,
                1.0,
                &params,
            );
        }
        assert_eq!(
            csr_a.weights, csr_b.weights,
            "R-STDP step is not deterministic"
        );
        assert_eq!(state_a.eligibility, state_b.eligibility);
        assert_eq!(state_a.reward_trace, state_b.reward_trace);
    }

    /// # V1 Hebbian-runaway regression
    ///
    /// With the warmup gate disabled (`warmup_samples = 0`) and a clean
    /// pre-leads-post spike pair driving positive eligibility, positive
    /// reward pumped in for 50 steps drives weights to saturate at `w_max`.
    /// That's the V1 runaway signature — unbounded weight growth from
    /// Hebbian evidence on a fresh network. Re-enabling the gate blocks
    /// every weight application in the same window, so weights stay at
    /// their initial value.
    ///
    /// If a future engineer proposes "we don't really need the warmup gate
    /// anymore, the homeostatic scaler will catch it":
    /// - The commented-out block below is what happens without the gate.
    /// - The active block below is what we ship.
    /// - This test has to pass as-is on any PR that touches this file.
    #[test]
    fn hebbian_runaway_regression() {
        let n_pre = 4;
        let n_post = 4;
        let params_no_gate = RstdpParams {
            warmup_samples: 0,
            eligibility_tau_ms: f32::INFINITY,
            reward_tau_ms: f32::INFINITY,
            ..default_params()
        };
        let params_with_gate = RstdpParams {
            warmup_samples: 100,
            ..params_no_gate
        };

        // Drive pattern: one clean LTP seed (all-pre fire at t=0, all-post
        // fire at t=1) produces uniformly-positive eligibility, then 50
        // further steps of reward=1 pump that eligibility into weights.
        //
        // Why not "saturated spikes every step": with the asymmetric STDP
        // kernel (a_minus > a_plus, Song et al. 2000 defaults), fully-
        // synchronous firing every step actually nets negative eligibility
        // and weights would *decay* — a real bug in its own right, but
        // not the runaway bug this test is guarding. Seeding cleanly and
        // then hammering reward isolates the warmup-gate behavior.
        let drive = |params: &RstdpParams| -> Vec<f32> {
            let mut csr = fc_csr(n_pre, n_post, 0.5);
            let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
            let all_on_pre: Vec<u8> = vec![1; n_pre as usize];
            let none_pre: Vec<u8> = vec![0; n_pre as usize];
            let all_on_post: Vec<u8> = vec![1; n_post as usize];
            let none_post: Vec<u8> = vec![0; n_post as usize];

            // Seed positive eligibility (pre fires at t=0, post at t=1).
            step_rstdp(
                &mut csr,
                &mut state,
                &all_on_pre,
                &none_post,
                1.0,
                0.0,
                1.0,
                params,
            );
            step_rstdp(
                &mut csr,
                &mut state,
                &none_pre,
                &all_on_post,
                1.0,
                1.0,
                1.0,
                params,
            );
            // 50 reward-only steps — no spikes, so eligibility doesn't change
            // (eligibility_tau = ∞), but the reward trace multiplies it into
            // weights each call. Gate-off: weights climb to w_max.
            for step in 0..50 {
                step_rstdp(
                    &mut csr,
                    &mut state,
                    &none_pre,
                    &none_post,
                    1.0,
                    (step + 2) as f32,
                    1.0,
                    params,
                );
            }
            csr.weights
        };

        // Gate OFF: V1 behavior. Weights run away to w_max.
        let w_no_gate = drive(&params_no_gate);
        let max_no_gate = w_no_gate.iter().copied().fold(0.0_f32, f32::max);
        assert!(
            max_no_gate >= 0.99,
            "V1 bug: without warmup gate, weights should run away to w_max; got max {max_no_gate}"
        );

        // Gate ON: only 52 calls, all below `warmup_samples = 100`. Every
        // call skips the weight-apply step → weights must be untouched.
        let w_with_gate = drive(&params_with_gate);
        assert!(
            w_with_gate.iter().all(|&w| (w - 0.5).abs() < 1e-6),
            "V2 gate should freeze weights during warmup; got {w_with_gate:?}"
        );

        // Proposed-removal note: if someone sets `warmup_samples = 0` on
        // this rule, they've re-opened the V1 bug. This test is the
        // tripwire — do not relax it.
        // let params_sneaky = RstdpParams { warmup_samples: 0, ..Default::default() };
        // let w_sneaky = drive(&params_sneaky);
        // let max_sneaky = w_sneaky.iter().copied().fold(0.0_f32, f32::max);
        // assert!(max_sneaky >= 0.99); // V1 bug back.
    }

    /// Structural mismatch (e.g. `pre_spikes.len() != n_pre`) must not panic.
    /// R-STDP is hot path; it returns "no updates" and keeps the brain alive.
    #[test]
    fn structural_mismatch_is_noop() {
        let n_pre = 2;
        let n_post = 2;
        let mut csr = fc_csr(n_pre, n_post, 0.5);
        let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
        let params = RstdpParams {
            warmup_samples: 0,
            ..default_params()
        };

        // pre_spikes has wrong length.
        let n = step_rstdp(
            &mut csr,
            &mut state,
            &[1, 1, 1], // len mismatch
            &[1, 1],
            1.0,
            0.0,
            1.0,
            &params,
        );
        assert_eq!(n, 0);
        // Rate sample still advances so repeated bad calls don't freeze
        // the gate's notion of time.
        assert_eq!(state.rate_samples, 1);
    }

    /// `reset()` clears transient state without reallocating buffers.
    #[test]
    fn reset_clears_state() {
        let n_pre = 2;
        let n_post = 2;
        let mut csr = fc_csr(n_pre, n_post, 0.5);
        let mut state = RstdpState::new(csr.weights.len(), n_pre, n_post);
        let params = RstdpParams {
            warmup_samples: 0,
            ..default_params()
        };

        for step in 0..5 {
            step_rstdp(
                &mut csr,
                &mut state,
                &[1, 0],
                &[0, 1],
                0.5,
                step as f32,
                1.0,
                &params,
            );
        }
        assert_ne!(state.reward_trace, 0.0);
        assert!(state.rate_samples > 0);

        state.reset();
        assert_eq!(state.reward_trace, 0.0);
        assert_eq!(state.rate_samples, 0);
        assert!(state.eligibility.iter().all(|&e| e == 0.0));
        assert!(
            state
                .last_pre_spike_ms
                .iter()
                .all(|&t| t == f32::NEG_INFINITY)
        );
        assert!(
            state
                .last_post_spike_ms
                .iter()
                .all(|&t| t == f32::NEG_INFINITY)
        );
        // Buffer lengths preserved.
        assert_eq!(state.eligibility.len(), 4);
    }

    // --- adaptive reward baseline (V1 rstdp_baseline_alpha) ---

    /// α = 0 (default): the EMA never engages and `baseline_reward` is used
    /// verbatim — the pre-EMA disable path.
    #[test]
    fn baseline_alpha_zero_is_constant_baseline() {
        let mut csr = fc_csr(2, 2, 0.5);
        let mut state = RstdpState::new(csr.weights.len(), 2, 2);
        let params = RstdpParams {
            baseline_reward: 0.25,
            warmup_samples: 0,
            ..RstdpParams::default()
        };
        assert_eq!(effective_baseline(&state, &params), 0.25);
        step_rstdp(
            &mut csr,
            &mut state,
            &[1, 0],
            &[0, 1],
            1.0,
            1.0,
            1.0,
            &params,
        );
        assert!(state.reward_baseline.is_none());
        assert_eq!(effective_baseline(&state, &params), 0.25);
    }

    /// α > 0: the baseline is seeded from `baseline_reward` and then tracks
    /// `reward_trace` by `(1−α)·b + α·trace` each step; the modulator is
    /// `trace − baseline`, so a constant reward is progressively discounted.
    #[test]
    fn baseline_alpha_tracks_reward_trace() {
        let mut csr = fc_csr(2, 2, 0.5);
        let mut state = RstdpState::new(csr.weights.len(), 2, 2);
        let params = RstdpParams {
            baseline_reward: 0.0,
            baseline_alpha: 0.5,
            reward_tau_ms: f32::INFINITY, // trace = cumulative reward, easy to follow
            warmup_samples: 0,
            ..RstdpParams::default()
        };
        // Step 1: trace = 1.0; modulator used 0 (seed); baseline → 0.5·0 + 0.5·1 = 0.5
        step_rstdp(
            &mut csr,
            &mut state,
            &[0, 0],
            &[0, 0],
            1.0,
            1.0,
            1.0,
            &params,
        );
        assert_eq!(state.reward_baseline, Some(0.5));
        // Step 2: trace = 2.0; baseline → 0.5·0.5 + 0.5·2 = 1.25
        step_rstdp(
            &mut csr,
            &mut state,
            &[0, 0],
            &[0, 0],
            1.0,
            2.0,
            1.0,
            &params,
        );
        assert_eq!(state.reward_baseline, Some(1.25));
        assert_eq!(effective_baseline(&state, &params), 1.25);
        // α is clamped to 1 → baseline jumps to the trace.
        let fast = RstdpParams {
            baseline_alpha: 5.0,
            ..params
        };
        step_rstdp(&mut csr, &mut state, &[0, 0], &[0, 0], 1.0, 3.0, 1.0, &fast);
        assert_eq!(state.reward_baseline, Some(state.reward_trace));
    }

    /// Old JSON without `baseline_alpha` deserializes to 0 (disabled).
    #[test]
    fn baseline_alpha_defaults_via_serde() {
        let p: RstdpParams = serde_json::from_str("{}").unwrap();
        assert_eq!(p.baseline_alpha, 0.0);
    }
}
