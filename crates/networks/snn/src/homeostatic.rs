//! Phase 3e — population rate EMA + homeostatic scaling + quiet-start transform.
//!
//! This module owns the **wiring** between a spiking population's firing
//! statistics and the synaptic scaling that keeps those statistics in
//! check. The math lives in [`nimcp_plasticity`]; this crate only
//! aggregates state (an exponential-moving-average rate tracker) and
//! applies the scalar results to a flat weight vector.
//!
//! # Contract with the rest of Phase 3
//!
//! - [`PopulationRateEma::update`] is called once per integration step,
//!   after [`lif_step`] has produced a new spike mask, with
//!   `fraction_spiking = n_spikes / n_neurons` so the EMA is
//!   scale-invariant.
//! - [`step_homeostatic`] then calls `rate.update(...)`, checks the
//!   warmup gate ([`nimcp_plasticity::rate_samples_ready`]), and — if
//!   the gate is open — asks [`nimcp_plasticity::homeostatic_scale`]
//!   for a multiplicative correction and multiplies every entry in
//!   [`CsrSynapses::weights`] by it.
//! - [`apply_quiet_start_transform`] is the **load-time** escape hatch:
//!   given an estimate of each population's current firing rate it calls
//!   [`nimcp_plasticity::quiet_start_scale`] and bulk-applies the
//!   returned factors. This is the V2 fix for V1's "resume from
//!   saturated checkpoint re-enters saturation" trap — the transform
//!   runs at every weight load, not just fresh init.
//!
//! # V1 history encoded as regression tests
//!
//! - The default homeostatic bounds `[0.98, 1.02]` are the **only**
//!   production values. V1's `[0.90, 1.10]` "emergency band" caused
//!   bang-bang oscillation; [`default_params_not_widened`] asserts the
//!   tight floor remains in place.
//! - The warmup gate (`samples >= 100`) prevents runaway potentiation
//!   on fresh networks whose rate EMAs haven't stabilised yet —
//!   [`homeostatic_respects_warmup_gate`] pins it down.
//! - Quiet-start-as-transform (rather than init-only code path) is the
//!   saturated-resume fix; [`quiet_start_load_restart_cycle`] simulates
//!   the V1 failure mode and verifies V2 recovers automatically.

use nimcp_plasticity::{
    DEFAULT_WARMUP_THRESHOLD, HomeostaticParams, homeostatic_scale, quiet_start_scale,
    rate_samples_ready,
};
use serde::{Deserialize, Serialize};

use crate::csr::CsrSynapses;

// -------------------------------------------------------------------------
// Rate EMA
// -------------------------------------------------------------------------

/// Default smoothing factor for the per-population firing-rate EMA.
/// Corresponds to a ~100-step time constant — matches the warmup
/// threshold so the tracker settles about the same time plasticity
/// is unblocked.
pub const DEFAULT_RATE_EMA_ALPHA: f32 = 0.01;

/// Per-population exponential moving average of the firing rate
/// (spikes per neuron per step).
///
/// One instance per destination population; updated on every
/// integration step from the LIF spike vector. Serializes cleanly into
/// the Phase 3 checkpoint payload so resumed brains inherit their rate
/// history — critical for honouring the warmup gate after a restart.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PopulationRateEma {
    /// Current rate estimate — same units as the input fraction
    /// (`n_spikes / n_neurons`).
    pub ema: f32,
    /// Smoothing factor α ∈ (0, 1]. `new_ema = (1−α)·ema + α·sample`.
    pub alpha: f32,
    /// Number of [`PopulationRateEma::update`] calls so far. Drives
    /// the warmup gate.
    pub samples: u32,
    /// Target firing rate — stored here so sibling modules can read
    /// it without threading another parameter.
    pub target_rate: f32,
}

impl PopulationRateEma {
    /// Allocate a fresh EMA with `ema = 0.0`, no samples, and the given
    /// smoothing + target.
    ///
    /// `alpha` is clamped into `(0.0, 1.0]` because either end is a
    /// degenerate filter: `α = 0` ignores input forever, `α > 1`
    /// amplifies noise. Out-of-range inputs are rounded to the nearest
    /// valid value rather than panicking; this is load-path code.
    #[must_use]
    pub fn new(alpha: f32, target_rate: f32) -> Self {
        let clamped_alpha = if alpha.is_finite() {
            alpha.clamp(f32::EPSILON, 1.0)
        } else {
            DEFAULT_RATE_EMA_ALPHA
        };
        Self {
            ema: 0.0,
            alpha: clamped_alpha,
            samples: 0,
            target_rate,
        }
    }

    /// Update the EMA with this step's `fraction_spiking = n_spikes / n_neurons`.
    ///
    /// Invalid inputs (`NaN`, `±∞`) are dropped silently — a corrupt
    /// spike count must not poison the rate tracker and thus every
    /// downstream plasticity rule.
    pub fn update(&mut self, fraction_spiking: f32) {
        if !fraction_spiking.is_finite() {
            return;
        }
        // Standard EMA: smooths fast transients, tracks the running mean.
        self.ema = (1.0 - self.alpha) * self.ema + self.alpha * fraction_spiking;
        // Saturating add guards against pathological run lengths (> 4 Gticks).
        self.samples = self.samples.saturating_add(1);
    }

    /// Current rate estimate.
    #[must_use]
    pub fn rate(&self) -> f32 {
        self.ema
    }

    /// Number of samples folded into the EMA so far.
    #[must_use]
    pub fn samples(&self) -> u32 {
        self.samples
    }

    /// `true` once enough samples have been accumulated for
    /// rate-dependent plasticity to fire safely. Delegates to
    /// [`nimcp_plasticity::rate_samples_ready`] with the shared
    /// [`DEFAULT_WARMUP_THRESHOLD`] so the gate can never drift between
    /// callers.
    #[must_use]
    pub fn warmup_done(&self) -> bool {
        rate_samples_ready(self.samples, DEFAULT_WARMUP_THRESHOLD)
    }
}

// -------------------------------------------------------------------------
// Homeostatic step
// -------------------------------------------------------------------------

/// Apply one homeostatic scaling step to `csr` in place.
///
/// Semantics:
///
/// 1. Call `rate.update(fraction_spiking_this_step)`.
/// 2. If `rate.warmup_done()` is `false`, return `None` without
///    touching weights — the EMA isn't trustworthy yet, so acting on
///    it would re-create V1's fresh-init runaway.
/// 3. Ask [`homeostatic_scale`] for `scale`. If the plasticity layer
///    returns the deadband sentinel `1.0`, return `None` (no work).
/// 4. Otherwise multiply every entry in `csr.weights` by `scale` and
///    return `Some(scale)`.
///
/// The returned `Option<f32>` lets the caller log / emit an event only
/// when weights actually moved, keeping per-step overhead minimal in
/// the steady state (no-op is the common case once a pop is balanced).
// HOT PATH: called per population per step.
pub fn step_homeostatic(
    csr: &mut CsrSynapses,
    rate: &mut PopulationRateEma,
    fraction_spiking_this_step: f32,
    params: &HomeostaticParams,
) -> Option<f32> {
    rate.update(fraction_spiking_this_step);

    if !rate.warmup_done() {
        return None;
    }

    let scale = homeostatic_scale(rate.ema, rate.target_rate, params);
    // Deadband sentinel: plasticity returns exactly 1.0 when the rate is
    // inside the allowed band. Skip the write entirely — avoids dirtying
    // cache lines for a no-op multiply. Exact-equality is intentional:
    // the plasticity crate's contract is a literal `1.0` return value,
    // not "approximately 1.0".
    #[allow(clippy::float_cmp)]
    if scale == 1.0 {
        return None;
    }

    for w in &mut csr.weights {
        *w *= scale;
    }
    Some(scale)
}

// -------------------------------------------------------------------------
// Phase 3.5 — reward-coupled homeostatic (master `1a495f51d` Fix 3)
// -------------------------------------------------------------------------

/// Reward threshold below which we consider R-STDP "stuck" — the
/// intrinsic-reward Gaussian has collapsed to ~0 because the population
/// is far above target. Default `0.1` (matches V1 heuristic).
pub const REWARD_STUCK_THRESHOLD: f32 = 0.1;
/// Rate-over-target ratio above which we consider the population
/// saturated. Default `1.5` (V1 matches).
pub const SATURATION_RATIO: f32 = 1.5;
/// Widened scale-down floor when saturated + reward-stuck. Default
/// `0.90` — ten times larger scale-down per step than the tight
/// `[0.98, 1.02]` loop. Use only when normal homeostatic can't pull
/// the pop back down because R-STDP has given up.
pub const SATURATED_MIN_SCALE: f32 = 0.90;

/// Like [`step_homeostatic`] but widens the scale-down floor when the
/// population is saturated AND R-STDP reward has collapsed.
///
/// Concretely: when `reward < REWARD_STUCK_THRESHOLD`
/// **and** `rate_ema > SATURATION_RATIO × target_rate`, the effective
/// `HomeostaticParams::min_scale` is dropped to
/// [`SATURATED_MIN_SCALE`] (`0.90`) instead of the default `0.98`.
/// The upper bound is untouched — we never aggressively potentiate.
///
/// Port of master commit `1a495f51d feat(snn): three control fixes` —
/// specifically the reward-coupled homeostatic block that breaks the
/// SATURATION↔FULL_COLLAPSE oscillation by giving homeostatic enough
/// authority to pull the pop down when R-STDP can't.
// HOT PATH: called per population per step.
pub fn step_homeostatic_with_reward(
    csr: &mut CsrSynapses,
    rate: &mut PopulationRateEma,
    fraction_spiking_this_step: f32,
    params: &HomeostaticParams,
    reward: f32,
) -> Option<f32> {
    // Update rate EMA unconditionally so warmup advances even if we
    // don't scale — same as step_homeostatic.
    rate.update(fraction_spiking_this_step);
    if !rate.warmup_done() {
        return None;
    }

    let saturated = rate.ema > SATURATION_RATIO * rate.target_rate;
    let stuck = reward < REWARD_STUCK_THRESHOLD;
    let effective_params = if saturated && stuck {
        HomeostaticParams {
            min_scale: SATURATED_MIN_SCALE.min(params.min_scale),
            ..*params
        }
    } else {
        *params
    };

    let scale = homeostatic_scale(rate.ema, rate.target_rate, &effective_params);
    #[allow(clippy::float_cmp)]
    if scale == 1.0 {
        return None;
    }

    for w in &mut csr.weights {
        *w *= scale;
    }
    Some(scale)
}

// -------------------------------------------------------------------------
// Quiet-start transform
// -------------------------------------------------------------------------

/// One-shot weight scaling applied whenever weights are **loaded** —
/// fresh init, checkpoint resume, or any other moment the operator
/// suspects a population may be saturated.
///
/// For each destination population the corresponding incoming
/// [`CsrSynapses::weights`] is multiplied by the factor produced by
/// [`nimcp_plasticity::quiet_start_scale`]. The plasticity crate
/// enforces the `[0.01, 5.0]` envelope, so this routine is a pure
/// in-place multiply.
///
/// Unlike [`step_homeostatic`] (whose bounds are the tight `[0.98, 1.02]`
/// loop gains), this transform is **aggressive by design** — it is the
/// V1 resume-trap fix, and must actually move weights far enough in one
/// shot to break out of a saturated attractor.
///
/// Returns the per-population scale actually applied, in the same order
/// as `csrs` / `observed_rates_per_pop`. Callers typically log the
/// vector as a single audit event ("quiet-start applied at load").
///
/// # Precondition
///
/// `csrs.len() == observed_rates_per_pop.len()`. If they differ the
/// function is a no-op and returns an empty vector — a length mismatch
/// is a caller bug but not worth panicking in load-time code.
#[must_use]
pub fn apply_quiet_start_transform(
    csrs: &mut [CsrSynapses],
    observed_rates_per_pop: &[f32],
    target_rate: f32,
) -> Vec<f32> {
    if csrs.len() != observed_rates_per_pop.len() {
        tracing::warn!(
            n_csrs = csrs.len(),
            n_rates = observed_rates_per_pop.len(),
            "apply_quiet_start_transform: length mismatch, skipping"
        );
        return Vec::new();
    }

    let scales = quiet_start_scale(observed_rates_per_pop, target_rate);

    // Mirror the plasticity crate's element-wise return: the i-th csr
    // gets the i-th scale. Skip the in-place multiply when the scale is
    // exactly 1.0 to avoid dirtying cache lines on already-healthy pops.
    // Exact-equality is intentional: plasticity returns a literal `1.0`
    // for pops inside the healthy band, not "approximately 1.0".
    for (csr, &s) in csrs.iter_mut().zip(scales.iter()) {
        #[allow(clippy::float_cmp)]
        let is_noop = s == 1.0;
        if !is_noop {
            for w in &mut csr.weights {
                *w *= s;
            }
        }
    }

    scales
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::float_cmp)] // exact-equality asserts are the V1 regression point
mod tests {
    use super::*;

    /// Convenience — build a CSR with uniform `value` weights of length `n`.
    fn csr_with(value: f32, n: usize) -> CsrSynapses {
        CsrSynapses::from_weights(vec![value; n])
    }

    /// Drive the EMA with a constant fraction; it must converge to that
    /// value. With a 50-step time constant (α = 0.02) 500 samples
    /// crushes the residual from the zero-initialised state below 1e-3
    /// — `(1 − α)^500 · 0.3 ≈ 1.3 × 10⁻⁵`.
    #[test]
    fn ema_converges_under_constant_input() {
        // Use a slightly faster filter than the DEFAULT so 500 samples
        // settles all the way to the 1e-3 tolerance this test asserts.
        // (At the module default α = 0.01 convergence is 500-step
        // half-life — real wiring runs more than 500 steps, but the
        // test wants to see a tight bound quickly.)
        let mut rate = PopulationRateEma::new(0.02, 0.03);
        for _ in 0..500 {
            rate.update(0.3);
        }
        assert!(
            (rate.rate() - 0.3).abs() < 1e-3,
            "EMA did not converge: got {}",
            rate.rate()
        );
        assert_eq!(rate.samples(), 500);
        assert!(rate.warmup_done());
    }

    /// Before the warmup gate opens, `step_homeostatic` is a no-op even
    /// under an extreme rate. This is the V1 "Hebbian runaway on fresh
    /// init" regression.
    #[test]
    fn homeostatic_respects_warmup_gate() {
        let mut csr = csr_with(1.0, 64);
        let before = csr.weights.clone();

        let mut rate = PopulationRateEma::new(DEFAULT_RATE_EMA_ALPHA, 0.03);
        let params = HomeostaticParams::default();

        // 99 steps < warmup threshold (100) — even at fraction 1.0 the
        // loop must not touch weights.
        for _ in 0..99 {
            let out = step_homeostatic(&mut csr, &mut rate, 1.0, &params);
            assert!(out.is_none(), "expected warmup no-op, got {out:?}");
        }
        assert_eq!(csr.weights, before, "weights changed during warmup");
        assert!(!rate.warmup_done());
    }

    /// After warmup, a rate well above target (here: EMA ≈ 0.87 after
    /// 200 saturated samples at α = 0.01, orders of magnitude above the
    /// 0.03 target) must clamp to exactly 0.98 (the tight lower bound).
    /// Any other value would indicate someone widened the defaults.
    #[test]
    fn homeostatic_tight_bounds_enforced() {
        let mut csr = csr_with(1.0, 8);
        let mut rate = PopulationRateEma::new(DEFAULT_RATE_EMA_ALPHA, 0.03);

        // Push 200 samples at fraction = 1.0 so the warmup gate opens
        // and the EMA sits far above target. The exact EMA value
        // doesn't matter — any rate outside [0.98, 1.02]·target clamps
        // to the tight lower bound.
        for _ in 0..200 {
            rate.update(1.0);
        }
        assert!(rate.warmup_done());
        assert!(
            rate.rate() > 0.03 * 1.02,
            "EMA {} unexpectedly inside deadband; can't test clamp",
            rate.rate()
        );

        let params = HomeostaticParams::default();
        let scale = step_homeostatic(&mut csr, &mut rate, 1.0, &params)
            .expect("expected Some(scale) at saturated rate");
        assert_eq!(scale, 0.98, "tight lower bound violated: {scale}");

        // Spot-check the weights: every entry should be ≈ 0.98.
        for &w in &csr.weights {
            assert!(
                (w - 0.98).abs() < 1e-6,
                "weight {w} did not receive 0.98 scale"
            );
        }
    }

    /// Pin the plasticity defaults as a regression against anyone who
    /// tries to widen them back to V1's `[0.90, 1.10]` "emergency band".
    #[test]
    fn default_params_not_widened() {
        let p = HomeostaticParams::default();
        assert_eq!(p.min_scale, 0.98, "min_scale widened — V1 bug returning");
        assert_eq!(p.max_scale, 1.02, "max_scale widened — V1 bug returning");
    }

    /// Drive saturated rate for 200 post-warmup steps. Weights must
    /// monotonically shrink — the V1 `[0.90, 1.10]` loop overshot and
    /// rang; the tight bounds guarantee one-sided convergence.
    #[test]
    fn anti_oscillation_regression() {
        let mut csr = csr_with(1.0, 4);
        let mut rate = PopulationRateEma::new(DEFAULT_RATE_EMA_ALPHA, 0.03);
        // Force-warm the EMA so the gate opens immediately.
        for _ in 0..200 {
            rate.update(1.0);
        }

        let params = HomeostaticParams::default();
        let mut last = csr.weights[0];
        for step in 0..200 {
            // Keep the rate pinned saturated — this is the pathological
            // "can't break free" input the tight loop is designed for.
            let out = step_homeostatic(&mut csr, &mut rate, 1.0, &params);
            // Every step should actually scale (saturated rate, scale < 1.0).
            assert!(out.is_some(), "step {step}: expected scaling, got no-op");
            let now = csr.weights[0];
            // Monotonic non-increase — never ring upward.
            assert!(
                now <= last + 1e-9,
                "step {step}: weight went UP ({last} -> {now}) — oscillation!"
            );
            // And never becomes negative (would be a hard math bug).
            assert!(now > 0.0, "step {step}: weight non-positive {now}");
            last = now;
        }
        // After 200 steps of saturated drive weights should have shrunk
        // meaningfully — 0.98^200 ≈ 0.018.
        assert!(
            csr.weights[0] < 0.05,
            "after 200 saturated steps weights barely moved: {}",
            csr.weights[0]
        );
    }

    /// Three populations with mixed observed rates. Quiet-start must
    /// push the hot ones down aggressively and leave the healthy one
    /// roughly alone.
    #[test]
    fn quiet_start_pulls_saturated_pops_down_on_load() {
        let mut csrs = vec![
            csr_with(1.0, 16), // saturated
            csr_with(1.0, 16), // half-saturated
            csr_with(1.0, 16), // healthy
        ];
        let observed = [1.0_f32, 0.5, 0.03];
        let target = 0.03_f32;

        let scales = apply_quiet_start_transform(&mut csrs, &observed, target);
        assert_eq!(scales.len(), 3);

        // Hot pops: scales well below 0.1.
        assert!(scales[0] < 0.1, "hot pop scale {} too mild", scales[0]);
        assert!(scales[1] < 0.1, "hot pop scale {} too mild", scales[1]);

        // Healthy pop: ≈ 1.0 (within 5%). Matches plasticity's healthy
        // band behaviour.
        assert!(
            (scales[2] - 1.0).abs() < 0.05,
            "healthy pop scale {} drifted",
            scales[2]
        );

        // Cross-check: weights multiplied by the reported scale.
        for (csr, s) in csrs.iter().zip(scales.iter()) {
            for &w in &csr.weights {
                assert!(
                    (w - s).abs() < 1e-6,
                    "weight {w} did not receive expected scale {s}"
                );
            }
        }
    }

    /// The V1 saturation-recovery regression. Simulate the full failure
    /// mode: train a pop into saturation, "save" its weights, throw
    /// away the state, "load" fresh, apply quiet-start against the
    /// observed saturated rate, and check recovery is automatic.
    #[test]
    fn quiet_start_load_restart_cycle() {
        // 1. Pop trained into saturation. Weights wound up large.
        let saturated_weights: Vec<f32> = vec![2.5_f32; 32];
        // 2. "Save" — just capture the weights we'll reload.
        let on_disk = saturated_weights.clone();
        // 3. "Load" — fresh CSR populated with the saved weights.
        let mut csrs = vec![CsrSynapses::from_weights(on_disk)];
        // 4. Observed rate on first re-observation = saturated.
        let observed_rates = [1.0_f32];
        let target = 0.03_f32;

        let scales = apply_quiet_start_transform(&mut csrs, &observed_rates, target);

        // Quiet-start's saturated branch uses `target / r`, so at r=1,
        // target=0.03 the scale should be ~0.03. Allow a small fudge
        // for the plasticity-crate floor (0.01) — i.e. the result must
        // be within [0.01, 0.05].
        assert!(
            scales[0] >= 0.01 && scales[0] <= 0.05,
            "quiet-start scale {} out of recovery band",
            scales[0]
        );

        // New weights should be roughly `saturated * scales[0]` ≈
        // `2.5 * 0.03` ≈ 0.075. More importantly: ~1/33 of the saved
        // weights, proving saturation is broken in one shot regardless
        // of the fact this was a resume (not a fresh init).
        let new_w = csrs[0].weights[0];
        let saved_w = saturated_weights[0];
        let ratio = new_w / saved_w;
        assert!(
            ratio <= 0.05,
            "new/saved ratio {ratio} not aggressive enough — V1 resume trap returning"
        );
    }

    /// Empty input must not panic and must return an empty vector.
    #[test]
    fn quiet_start_empty_no_op() {
        let mut csrs: Vec<CsrSynapses> = Vec::new();
        let observed: [f32; 0] = [];
        let scales = apply_quiet_start_transform(&mut csrs, &observed, 0.03);
        assert!(scales.is_empty());
    }

    /// Run 100 homeostatic steps followed by one quiet-start
    /// application. Both composability matters (neither code path
    /// should clobber the other) and the result must stay finite.
    #[test]
    fn homeostatic_then_quiet_start_are_composable() {
        let mut csr = csr_with(1.0, 16);
        let mut rate = PopulationRateEma::new(DEFAULT_RATE_EMA_ALPHA, 0.03);
        // Force-warm then run 100 step_homeostatic calls at a saturated
        // rate — after this the weights will be smaller but the rate
        // still reads saturated because we keep feeding it 1.0.
        for _ in 0..200 {
            rate.update(1.0);
        }
        let params = HomeostaticParams::default();
        for _ in 0..100 {
            step_homeostatic(&mut csr, &mut rate, 1.0, &params);
        }

        // Weights should have shrunk but remain finite + positive.
        for &w in &csr.weights {
            assert!(
                w.is_finite(),
                "weight {w} not finite after step_homeostatic"
            );
            assert!(w > 0.0, "weight {w} went non-positive");
        }
        let after_homeostatic = csr.weights[0];

        // Now the operator notices saturation at the rate level and
        // applies quiet-start. This must succeed on already-scaled
        // weights without panicking or producing non-finite values.
        let mut csrs = [csr];
        let scales = apply_quiet_start_transform(&mut csrs, &[1.0], 0.03);
        assert_eq!(scales.len(), 1);

        for &w in &csrs[0].weights {
            assert!(
                w.is_finite(),
                "weight {w} not finite after quiet-start composition"
            );
            assert!(w > 0.0, "weight {w} went non-positive");
            // Quiet-start further shrank (scale < 1.0 at saturated rate).
            assert!(
                w < after_homeostatic,
                "quiet-start did not further shrink {w} vs {after_homeostatic}"
            );
        }
    }

    /// Sanity: NaN / Inf fractions are dropped rather than poisoning
    /// the EMA. Callers get the same rate they had before the bad
    /// sample and no sample is counted.
    #[test]
    fn ema_ignores_non_finite_samples() {
        let mut rate = PopulationRateEma::new(DEFAULT_RATE_EMA_ALPHA, 0.03);
        for _ in 0..10 {
            rate.update(0.2);
        }
        let snapshot = rate.rate();
        let samples_before = rate.samples();

        rate.update(f32::NAN);
        rate.update(f32::INFINITY);
        rate.update(f32::NEG_INFINITY);

        assert_eq!(rate.rate(), snapshot, "EMA absorbed a non-finite sample");
        assert_eq!(
            rate.samples(),
            samples_before,
            "sample counter bumped on non-finite input"
        );
    }

    /// Sanity: `PopulationRateEma::new` clamps a pathological alpha
    /// rather than storing it verbatim. This is load-path code; a bad
    /// config shouldn't be able to brick the loop.
    #[test]
    fn ema_new_clamps_alpha() {
        assert_eq!(PopulationRateEma::new(0.0, 0.03).alpha, f32::EPSILON);
        assert_eq!(PopulationRateEma::new(-1.0, 0.03).alpha, f32::EPSILON);
        assert_eq!(PopulationRateEma::new(2.0, 0.03).alpha, 1.0);
        // NaN → fallback to the module default.
        assert_eq!(
            PopulationRateEma::new(f32::NAN, 0.03).alpha,
            DEFAULT_RATE_EMA_ALPHA
        );
    }

    /// If caller and CSR slices differ in length we skip rather than
    /// panic. Return value is the empty vec and no weights move.
    #[test]
    fn quiet_start_length_mismatch_skips() {
        let mut csrs = vec![csr_with(1.0, 4), csr_with(1.0, 4)];
        let before: Vec<Vec<f32>> = csrs.iter().map(|c| c.weights.clone()).collect();
        let scales = apply_quiet_start_transform(&mut csrs, &[1.0], 0.03);
        assert!(scales.is_empty());
        for (c, b) in csrs.iter().zip(before.iter()) {
            assert_eq!(&c.weights, b, "weights moved on length mismatch");
        }
    }

    // ---------------------------------------------------------------------
    // Phase 3.5 — reward-coupled homeostatic (master 1a495f51d Fix 3)
    // ---------------------------------------------------------------------

    /// Saturated + reward-stuck → widened scale-down bound (0.90 instead
    /// of 0.98) lets homeostatic pull the pop down more aggressively
    /// than the tight loop allows.
    #[test]
    fn reward_coupled_widens_scale_down_when_stuck() {
        let mut csr = csr_with(1.0, 8);
        let mut rate = PopulationRateEma::new(DEFAULT_RATE_EMA_ALPHA, 0.03);
        for _ in 0..200 {
            rate.update(1.0);
        }
        let params = HomeostaticParams::default();
        // Reward collapsed (< 0.1) AND rate far above target → saturated.
        let scale = step_homeostatic_with_reward(&mut csr, &mut rate, 1.0, &params, 0.0)
            .expect("expected Some(scale) at saturated+stuck");
        // Widened lower bound is 0.90, so the actual scale is 0.90 not 0.98.
        assert!(
            scale < 0.95,
            "widened scale-down not applied: got {scale}, expected ~0.90"
        );
        assert!(
            scale >= 0.90 - 1e-6,
            "scale overshot widened floor: {scale}"
        );
    }

    /// Saturated but reward healthy → normal tight bounds apply. Reward
    /// is doing its job; homeostatic stays on its tight leash.
    #[test]
    fn reward_coupled_keeps_tight_bounds_when_reward_healthy() {
        let mut csr = csr_with(1.0, 8);
        let mut rate = PopulationRateEma::new(DEFAULT_RATE_EMA_ALPHA, 0.03);
        for _ in 0..200 {
            rate.update(1.0);
        }
        let params = HomeostaticParams::default();
        // reward=0.5 > 0.1 threshold → R-STDP is still functional.
        let scale = step_homeostatic_with_reward(&mut csr, &mut rate, 1.0, &params, 0.5)
            .expect("expected Some(scale) at saturated+healthy-reward");
        assert_eq!(scale, 0.98, "tight bound violated when reward is healthy");
    }

    /// Reward collapsed but rate inside band → no scaling needed at all.
    /// Widening shouldn't fire just because reward happens to be low;
    /// the rate must also be saturated.
    #[test]
    fn reward_coupled_no_op_inside_band() {
        let mut csr = csr_with(1.0, 8);
        // Fast alpha + enough samples so ema settles cleanly at target.
        let mut rate = PopulationRateEma::new(0.1, 0.03);
        for _ in 0..500 {
            rate.update(0.03);
        }
        assert!(
            (rate.rate() - 0.03).abs() < 1e-4,
            "EMA should settle to target"
        );
        let params = HomeostaticParams::default();
        let outcome = step_homeostatic_with_reward(&mut csr, &mut rate, 0.03, &params, 0.0);
        assert!(outcome.is_none(), "unexpected scale inside deadband");
    }

    /// Warmup is respected by the reward-coupled variant, same as the
    /// plain one — no scaling before the sample threshold.
    #[test]
    fn reward_coupled_respects_warmup() {
        let mut csr = csr_with(1.0, 8);
        let before = csr.weights.clone();
        let mut rate = PopulationRateEma::new(DEFAULT_RATE_EMA_ALPHA, 0.03);
        let params = HomeostaticParams::default();
        let outcome = step_homeostatic_with_reward(&mut csr, &mut rate, 1.0, &params, 0.0);
        assert!(outcome.is_none());
        assert_eq!(csr.weights, before);
    }
}
