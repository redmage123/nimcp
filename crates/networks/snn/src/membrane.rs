//! SNN membrane integration helpers — current-mode and conductance-mode.
//!
//! Single-source-of-truth for the LIF membrane equation. Both the legacy
//! current-mode path and the new conductance-based (CB) path share this
//! module's pure helpers so the math is verified in one place.
//!
//! V2 port of `include/snn/nimcp_snn_membrane.h` (V1 commit `dec956ab9`).
//!
//! # CB equation
//!
//! ```text
//! dv = ( (v_rest - v) + g_exc*(E_exc - v) + g_inh*(E_inh - v) ) / tau_eff * dt
//! ```
//!
//! Voltage `v` is physically incapable of crossing `E_exc` / `E_inh` —
//! the brake is in the synapse equation itself, not in a thermostatic
//! homeostatic controller. This is why CB suppresses the dead↔runaway
//! oscillation that current-mode dynamics fall into under high recurrent
//! gain.
//!
//! # Current-mode equation (unchanged, for reference)
//!
//! ```text
//! dv = ( (v_rest - v) + I_syn ) / tau_eff * dt
//! ```
//!
//! # Pure functions
//!
//! These helpers are `#[inline]` pure functions of their inputs — no
//! globals, no I/O — so they unit-test in isolation and the SNN hot
//! loop pays no call overhead.

/// Default V1 reversal potential for AMPA-like excitatory synapses (mV).
pub const SNN_E_EXC_MV: f32 = 0.0;
/// Default V1 reversal potential for GABA-A-like inhibitory synapses (mV).
pub const SNN_E_INH_MV: f32 = -80.0;
/// Default V1 fast AMPA decay time constant (ms).
pub const SNN_TAU_EXC_MS: f32 = 2.0;
/// Default V1 GABA-A decay time constant (ms) — slower than AMPA so I
/// has time to brake E.
pub const SNN_TAU_INH_MS: f32 = 8.0;
/// Default V1 weight rescale factor applied at CB enable. Average
/// driving force at rest is ~50 mV, so dividing weights by 50
/// preserves the legacy current-mode firing rate at the moment of
/// switchover.
pub const SNN_CB_WEIGHT_SCALE: f32 = 1.0 / 50.0;

/// Lower bound on `tau_eff_ms` to keep the integration stable. Upstream
/// callers always pass `tau_eff > 0`, but a defensive clamp prevents an
/// inf/-inf cascade if a future caller drifts off-spec.
const TAU_FLOOR_MS: f32 = 1e-3;

/// Maximum |dv| permitted per integration step (mV). Real cortical
/// neurons swing 60–80 mV across an action-potential cycle; a |dv|
/// larger than 100 mV per dt indicates pathological tau collapse,
/// runaway g_exc, or an unrescaled-weight CB run. Clamping keeps the
/// LIF state from blowing up while leaving normal dynamics untouched.
const DV_CLAMP_MV: f32 = 100.0;

/// One LIF integration step's `dv`, in current OR conductance mode.
///
/// Selects the synaptic-drive term by `conductance_mode`:
///
/// - **current mode** (`conductance_mode = false`): `i_syn` is the
///   summed synaptic input (mV-equivalent); `g_*` and `e_*` ignored.
/// - **CB mode** (`conductance_mode = true`): `g_exc` / `g_inh` are
///   per-neuron conductances (dimensionless); `i_syn` ignored.
///
/// In CB mode the result is additionally clamped so `v + dv` cannot
/// cross either reversal potential — that's the key conductance-based
/// invariant. Without this extra clamp a very large `g_exc` combined
/// with the [`DV_CLAMP_MV`] safety clamp could let `v` leapfrog over
/// `e_exc` on a single step.
///
/// Pure function. Inputs are read by value; output is the dv to apply
/// to `v` (the caller does `v += dv` then handles threshold + refrac).
#[inline]
#[must_use]
#[allow(clippy::too_many_arguments)] // hot-path; keeping inline params to avoid struct overhead
pub fn compute_dv(
    v: f32,
    v_rest: f32,
    tau_eff_ms: f32,
    dt_ms: f32,
    i_syn: f32,
    g_exc: f32,
    g_inh: f32,
    e_exc: f32,
    e_inh: f32,
    conductance_mode: bool,
) -> f32 {
    let tau_eff = tau_eff_ms.max(TAU_FLOOR_MS);
    let leak = v_rest - v;
    let drive = if conductance_mode {
        leak + g_exc * (e_exc - v) + g_inh * (e_inh - v)
    } else {
        leak + i_syn
    };
    let mut dv = drive * (dt_ms / tau_eff);
    // Biophysical clamp.
    dv = dv.clamp(-DV_CLAMP_MV, DV_CLAMP_MV);
    // CB-mode reversal-potential bound.
    if conductance_mode {
        let v_after = v + dv;
        if v_after > e_exc {
            dv = e_exc - v;
        } else if v_after < e_inh {
            dv = e_inh - v;
        }
    }
    dv
}

/// Decay one neuron's conductances by precomputed exponential factors.
///
/// Caller computes `decay_exc = (-dt/tau_exc).exp()` and `decay_inh`
/// **once per population step** (NOT per neuron) to avoid `expf` in the
/// inner loop — matches V1's pattern with `dep_decay`.
#[inline]
pub fn decay_one(g_exc: &mut f32, g_inh: &mut f32, decay_exc: f32, decay_inh: f32) {
    *g_exc *= decay_exc;
    *g_inh *= decay_inh;
}

/// Deposit one synaptic weight into either `i_syn` (current mode) or
/// `g_exc` / `g_inh` (CB mode), routing CB by weight sign.
///
/// In CB mode: positive weight → `g_exc`, negative weight → `g_inh`
/// (deposit `|weight|`). Receptor type is derived from sign — no
/// per-synapse field change needed.
///
/// Branch-free path is hoisted by the compiler when `conductance_mode`
/// is captured before the inner loop (LICM).
#[inline]
pub fn deposit_synapse(
    i_syn: &mut f32,
    g_exc: &mut f32,
    g_inh: &mut f32,
    weight: f32,
    conductance_mode: bool,
) {
    if conductance_mode {
        if weight >= 0.0 {
            *g_exc += weight;
        } else {
            *g_inh += -weight;
        }
    } else {
        *i_syn += weight;
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    // -------- compute_dv: current-mode bit-identity with legacy --------

    #[test]
    fn current_mode_matches_legacy_lif_formula() {
        // V1 / V2 legacy: dv = dt/tau * (v_rest - v + i_syn)
        let v = -65.0;
        let v_rest = -70.0;
        let tau = 20.0;
        let dt = 1.0;
        let i_syn = 3.0;
        let dv = compute_dv(v, v_rest, tau, dt, i_syn, 0.0, 0.0, 0.0, -80.0, false);
        let expected = (dt / tau) * (v_rest - v + i_syn);
        assert!((dv - expected).abs() < 1e-6);
    }

    // -------- compute_dv: CB equation correctness --------

    #[test]
    fn cb_mode_drives_v_toward_e_exc_under_excitation() {
        // High g_exc with E_exc = 0 should pull v upward (positive dv).
        let dv = compute_dv(-65.0, -70.0, 20.0, 1.0, 0.0, 0.5, 0.0, 0.0, -80.0, true);
        assert!(dv > 0.0);
    }

    #[test]
    fn cb_mode_drives_v_toward_e_inh_under_inhibition() {
        // High g_inh with E_inh = -80 and v = -65 should pull v down.
        let dv = compute_dv(-65.0, -70.0, 20.0, 1.0, 0.0, 0.0, 0.5, 0.0, -80.0, true);
        assert!(dv < 0.0);
    }

    #[test]
    fn cb_mode_v_cannot_cross_e_exc() {
        // v already at -50; an enormous g_exc tries to overshoot E_exc=0.
        let v = -50.0;
        let dv = compute_dv(v, -70.0, 20.0, 1.0, 0.0, 1000.0, 0.0, 0.0, -80.0, true);
        let v_after = v + dv;
        assert!(
            v_after <= 0.0 + 1e-3,
            "v_after = {v_after} should be <= E_exc"
        );
    }

    #[test]
    fn cb_mode_v_cannot_cross_e_inh() {
        // v at -75; massive g_inh tries to overshoot E_inh=-80.
        let v = -75.0;
        let dv = compute_dv(v, -70.0, 20.0, 1.0, 0.0, 0.0, 1000.0, 0.0, -80.0, true);
        let v_after = v + dv;
        assert!(
            v_after >= -80.0 - 1e-3,
            "v_after = {v_after} should be >= E_inh"
        );
    }

    #[test]
    fn dv_is_clamped_to_100mv() {
        // Pathological tau collapse: tau very small → dv would explode.
        let dv = compute_dv(
            -65.0, -70.0, 0.001, 1.0, 1000.0, 0.0, 0.0, 0.0, -80.0, false,
        );
        assert!(dv.abs() <= DV_CLAMP_MV);
    }

    #[test]
    fn tiny_tau_does_not_panic() {
        // tau = 0 would be a div-by-zero without TAU_FLOOR_MS.
        let dv = compute_dv(-65.0, -70.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0, -80.0, false);
        assert!(dv.is_finite());
    }

    #[test]
    fn cb_mode_at_resting_with_zero_conductance_matches_leak_only() {
        // At v_rest with g_exc = g_inh = 0: dv should be zero.
        let dv = compute_dv(-70.0, -70.0, 20.0, 1.0, 0.0, 0.0, 0.0, 0.0, -80.0, true);
        assert!(dv.abs() < 1e-6);
    }

    // -------- decay_one --------

    #[test]
    fn decay_one_applies_factors() {
        let mut g_exc = 1.0;
        let mut g_inh = 0.5;
        decay_one(&mut g_exc, &mut g_inh, 0.5, 0.8);
        assert!((g_exc - 0.5).abs() < 1e-6);
        assert!((g_inh - 0.4).abs() < 1e-6);
    }

    #[test]
    fn decay_one_at_zero_stays_zero() {
        let mut g_exc = 0.0;
        let mut g_inh = 0.0;
        decay_one(&mut g_exc, &mut g_inh, 0.5, 0.5);
        assert_eq!(g_exc, 0.0);
        assert_eq!(g_inh, 0.0);
    }

    #[test]
    fn decay_factor_at_one_is_identity() {
        let mut g_exc = 0.7;
        let mut g_inh = 0.3;
        decay_one(&mut g_exc, &mut g_inh, 1.0, 1.0);
        assert!((g_exc - 0.7).abs() < 1e-6);
        assert!((g_inh - 0.3).abs() < 1e-6);
    }

    // -------- deposit_synapse --------

    #[test]
    fn current_mode_deposits_into_i_syn() {
        let mut i_syn = 0.0;
        let mut g_exc = 0.0;
        let mut g_inh = 0.0;
        deposit_synapse(&mut i_syn, &mut g_exc, &mut g_inh, 0.5, false);
        deposit_synapse(&mut i_syn, &mut g_exc, &mut g_inh, -0.3, false);
        assert!((i_syn - 0.2).abs() < 1e-6);
        assert_eq!(g_exc, 0.0);
        assert_eq!(g_inh, 0.0);
    }

    #[test]
    fn cb_mode_routes_positive_weight_to_g_exc() {
        let mut i_syn = 0.0;
        let mut g_exc = 0.0;
        let mut g_inh = 0.0;
        deposit_synapse(&mut i_syn, &mut g_exc, &mut g_inh, 0.4, true);
        assert!((g_exc - 0.4).abs() < 1e-6);
        assert_eq!(g_inh, 0.0);
        assert_eq!(i_syn, 0.0);
    }

    #[test]
    fn cb_mode_routes_negative_weight_to_g_inh_as_abs() {
        let mut i_syn = 0.0;
        let mut g_exc = 0.0;
        let mut g_inh = 0.0;
        deposit_synapse(&mut i_syn, &mut g_exc, &mut g_inh, -0.4, true);
        assert_eq!(g_exc, 0.0);
        assert!((g_inh - 0.4).abs() < 1e-6); // deposited as |weight|
        assert_eq!(i_syn, 0.0);
    }

    #[test]
    fn cb_mode_zero_weight_no_op() {
        let mut i_syn = 0.0;
        let mut g_exc = 0.0;
        let mut g_inh = 0.0;
        deposit_synapse(&mut i_syn, &mut g_exc, &mut g_inh, 0.0, true);
        assert_eq!(i_syn, 0.0);
        assert_eq!(g_exc, 0.0); // routed to g_exc as positive (>=0) but adds zero
        assert_eq!(g_inh, 0.0);
    }

    // -------- composition: decay + deposit + integrate --------

    #[test]
    fn cb_one_full_step_composition() {
        // Simulate: decay g, deposit a spike weight, then compute dv.
        let mut g_exc = 0.4;
        let mut g_inh = 0.1;
        // dt=1ms, tau_exc=2ms → decay_exc = exp(-0.5) ≈ 0.6065
        let decay_exc = (-1.0_f32 / 2.0).exp();
        let decay_inh = (-1.0_f32 / 8.0).exp();
        decay_one(&mut g_exc, &mut g_inh, decay_exc, decay_inh);
        // Deposit a +0.2 weight.
        let mut i_syn_unused = 0.0;
        deposit_synapse(&mut i_syn_unused, &mut g_exc, &mut g_inh, 0.2, true);
        // Integrate.
        let dv = compute_dv(-65.0, -70.0, 20.0, 1.0, 0.0, g_exc, g_inh, 0.0, -80.0, true);
        // Reasonable bounds: g_exc clearly drives v up.
        assert!(dv > 0.0);
        assert!(dv.is_finite());
    }
}
