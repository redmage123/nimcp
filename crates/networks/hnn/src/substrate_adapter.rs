//! Substrate + thalamic modulation helpers for the HNN (Phase
//! 11-substrate).
//!
//! The HNN is an **autonomous** integrator — [`crate::HnnNetwork::step`]
//! advances the internal `(q, p)` state with no external input. So the
//! substrate surface is different from the CNN / FNO:
//!
//! - **No input attention.** There is no afferent signal to gate; the
//!   thalamic channel is purely *output routing* — it records a submit
//!   when post-step activity (momentum magnitude) crosses a threshold so
//!   downstream networks can be Hebbian-routed from the HNN.
//! - **Integration timestep.** `axon.membrane_capacitance_mod` scales the
//!   effective `dt` exactly as in the LNN: smaller capacitance → voltage
//!   (here, phase-space state) moves faster → larger effective `dt`.
//!   Clamped to `[0.5·dt, 2.0·dt]` so a degraded substrate can't blow up
//!   the symplectic integrator.
//!
//! **Energy-conservation invariant.** At full health
//! (`membrane_capacitance_mod = 1.0`) [`effective_dt`] returns `dt`
//! unchanged, so a full-health substrate leaves the symplectic dynamics
//! bit-identical and the energy-conservation guarantee holds. Substrate
//! degradation deliberately perturbs `dt` — a less accurate integrator
//! is the biologically faithful consequence of a starved substrate.
//!
//! Helpers are identity on `None` effects — the disable path is
//! bit-identical to pre-Phase-11-substrate behaviour.

use nimcp_substrate::{AxonSubstrateEffects, DendriteSubstrateEffects};
use nimcp_thalamic::ThalamicChannel;

/// Convenience alias for the `(axon, dendrite)` effect pair the
/// substrate produces.
pub type Effects = (AxonSubstrateEffects, DendriteSubstrateEffects);

/// Capacitance-corrected effective timestep. `membrane_capacitance_mod`
/// of `1.0` is identity; a smaller mod speeds up integration. Clamped to
/// `[0.5·dt, 2.0·dt]`.
///
/// Returns `dt` unchanged when `effects` is `None` — preserving the
/// energy-conservation guarantee on the disable path.
#[must_use]
pub fn effective_dt(dt: f32, effects: Option<&Effects>) -> f32 {
    let Some((axon, _dend)) = effects else {
        return dt;
    };
    let c = axon.membrane_capacitance_mod.clamp(0.5, 2.0);
    // Smaller capacitance → larger effective dt (state moves faster per
    // unit time). Inverse relationship, matching the LNN adapter.
    (dt / c).clamp(dt * 0.5, dt * 2.0)
}

/// Whether a degraded substrate would perturb the timestep — `true` only
/// when `effects` is present AND `membrane_capacitance_mod` differs from
/// `1.0`. Lets the network skip the modulated path entirely at full
/// health (so energy conservation is provably untouched).
#[must_use]
pub fn perturbs_dt(effects: Option<&Effects>) -> bool {
    match effects {
        Some((axon, _)) => (axon.membrane_capacitance_mod - 1.0).abs() > f32::EPSILON,
        None => false,
    }
}

/// Post-step submit decision for the thalamic channel: returns `true`
/// when the L2 norm of the momentum vector exceeds `threshold`. Pure
/// helper so the network's `step_modulated` stays readable.
#[must_use]
pub fn momentum_crosses(p: &ndarray::Array1<f32>, threshold: f32) -> bool {
    let mag = p.iter().map(|v| v * v).sum::<f32>().sqrt();
    mag > threshold
}

/// Effective output gain from the source to a destination using a
/// router's Hebbian weights, falling back to the channel's own gate when
/// the router has no learned route yet. Returns `1.0` when `channel` is
/// `None`. Shared shape with the CNN / FNO / LNN `thalamic_output_gain`.
#[must_use]
pub fn output_gain(
    channel: Option<&ThalamicChannel>,
    router: &nimcp_thalamic::ThalamicRouter,
    dest_id: u32,
) -> f32 {
    let Some(ch) = channel else {
        return 1.0;
    };
    let router_gain = router.effective_gain(ch.source_id, dest_id);
    if router_gain == 0.0 {
        ch.get_gate(dest_id)
    } else {
        router_gain
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use ndarray::Array1;

    fn identity_effects() -> Effects {
        (
            AxonSubstrateEffects::default(),
            DendriteSubstrateEffects::default(),
        )
    }

    #[test]
    fn effective_dt_identity_when_none() {
        assert_eq!(effective_dt(0.01, None), 0.01);
    }

    #[test]
    fn effective_dt_identity_at_full_health() {
        let eff = identity_effects();
        assert_eq!(effective_dt(0.01, Some(&eff)), 0.01);
    }

    #[test]
    fn effective_dt_speeds_up_with_lower_capacitance() {
        let mut eff = identity_effects();
        eff.0.membrane_capacitance_mod = 0.8;
        let dt = effective_dt(0.01, Some(&eff));
        assert!(dt > 0.01, "smaller C_m should increase effective dt");
    }

    #[test]
    fn effective_dt_clamps_at_extremes() {
        let mut eff = identity_effects();
        eff.0.membrane_capacitance_mod = 0.1;
        let dt = effective_dt(0.01, Some(&eff));
        assert!(dt <= 0.02 + 1e-9, "clamp at 2x");
    }

    #[test]
    fn perturbs_dt_false_at_full_health_and_none() {
        assert!(!perturbs_dt(None));
        assert!(!perturbs_dt(Some(&identity_effects())));
        let mut eff = identity_effects();
        eff.0.membrane_capacitance_mod = 0.9;
        assert!(perturbs_dt(Some(&eff)));
    }

    #[test]
    fn momentum_crosses_threshold() {
        let p = Array1::from_vec(vec![3.0, 4.0]); // norm 5.0
        assert!(momentum_crosses(&p, 4.9));
        assert!(!momentum_crosses(&p, 5.1));
    }
}
