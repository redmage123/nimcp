//! Substrate → SNN parameter modulation helpers.
//!
//! Pure functions — no state, no allocation. Adapters in
//! [`crate::network`] call these each step (after the cadence check
//! has refreshed cached effects) to produce the **effective** LIF +
//! R-STDP params the hot path uses this tick.
//!
//! All helpers are identity on the full-health effect defaults —
//! a population with `substrate.enabled = false` (effects = `None`)
//! sees pre-Phase-1 behavior bit-for-bit.

use nimcp_plasticity::StdpParams;
use nimcp_substrate::{AxonSubstrateEffects, DendriteSubstrateEffects};

use crate::lif::LifParams;
use crate::rstdp::RstdpParams;

/// Produce an effective [`LifParams`] with substrate multipliers
/// applied. When `effects` is `None`, returns `base` unchanged.
///
/// Modulations:
/// - `tau_ms *= dend.membrane_time_constant_mod` — degraded membrane
///   leaks faster (shorter effective tau).
/// - `v_thresh` shifted by `(1 - dend.spike_threshold_mod) ×
///   |v_thresh - v_rest|` — a `spike_threshold_mod < 1` means the
///   threshold sits closer to rest (easier to fire under ion stress).
/// - `refrac_steps *= axon.refractory_period_mod` — stressed pump →
///   longer refractory.
#[must_use]
pub fn effective_lif(
    base: &LifParams,
    effects: Option<&(AxonSubstrateEffects, DendriteSubstrateEffects)>,
) -> LifParams {
    let Some((axon, dend)) = effects else {
        return *base;
    };
    // Sentinel guard (V1 commit 43785ee5e): if dendrite cache was never
    // populated (zero-initialized) we'd silently scale tau_ms by 0 and
    // kill the membrane integrator. Fall back to base.
    if dend.is_zero_cache() {
        return *base;
    }
    let tau_ms = (base.tau_ms * dend.membrane_time_constant_mod).max(0.1);
    let v_gap = base.v_thresh - base.v_rest;
    let v_thresh = base.v_rest + v_gap * dend.spike_threshold_mod;
    #[allow(
        clippy::cast_precision_loss,
        clippy::cast_possible_truncation,
        clippy::cast_sign_loss
    )]
    let refrac_steps_f = (base.refrac_steps as f32 * axon.refractory_period_mod).round();
    let refrac_steps = refrac_steps_f.max(0.0) as u32;
    LifParams {
        tau_ms,
        v_rest: base.v_rest,
        v_thresh,
        v_reset: base.v_reset,
        refrac_steps,
        conductance: base.conductance,
    }
}

/// Produce an effective [`RstdpParams`] with substrate plasticity
/// multipliers applied. Both `a_plus` and `a_minus` get scaled by
/// `dend.plasticity_mod`. When `effects` is `None`, or the caller
/// opts out, returns `base` unchanged.
#[must_use]
pub fn effective_rstdp(
    base: &RstdpParams,
    effects: Option<&(AxonSubstrateEffects, DendriteSubstrateEffects)>,
    apply_plasticity_mod: bool,
) -> RstdpParams {
    if !apply_plasticity_mod {
        return *base;
    }
    let Some((_axon, dend)) = effects else {
        return *base;
    };
    // Sentinel guard: zero-cache → skip scaling (would kill LR).
    if dend.is_zero_cache() {
        return *base;
    }
    let plast = dend.plasticity_mod.clamp(0.0, 1.0);
    RstdpParams {
        stdp: StdpParams {
            a_plus: base.stdp.a_plus * plast,
            a_minus: base.stdp.a_minus * plast,
            ..base.stdp
        },
        ..*base
    }
}

/// Scale an external reward by the destination population's
/// Ca²⁺-handling modulation — LTP/LTD require clean Ca spikes, so a
/// stressed Ca system dampens the teaching signal. Identity (×1.0)
/// when `effects` is `None` or `apply = false`.
#[must_use]
pub fn effective_reward(
    reward: f32,
    effects: Option<&(AxonSubstrateEffects, DendriteSubstrateEffects)>,
    apply: bool,
) -> f32 {
    if !apply {
        return reward;
    }
    let Some((_axon, dend)) = effects else {
        return reward;
    };
    // Sentinel guard: zero-cache → return raw reward (don't zero it).
    if dend.is_zero_cache() {
        return reward;
    }
    reward * dend.ca_handling_mod.clamp(0.0, 1.0)
}

/// True when `overall_capacity` has fallen below the emergency-silence
/// threshold. Adapter callers zero the population's spike vector when
/// this fires — matches V1's emergency-silence convention
/// (chemistry stress → no firing this step).
#[must_use]
pub fn emergency_silence(
    effects: Option<&(AxonSubstrateEffects, DendriteSubstrateEffects)>,
    threshold: f32,
) -> bool {
    let Some((axon, _dend)) = effects else {
        return false;
    };
    axon.overall_capacity < threshold
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    fn base_lif() -> LifParams {
        LifParams::default()
    }

    fn base_rstdp() -> RstdpParams {
        RstdpParams::default()
    }

    fn identity_effects() -> (AxonSubstrateEffects, DendriteSubstrateEffects) {
        (
            AxonSubstrateEffects::default(),
            DendriteSubstrateEffects::default(),
        )
    }

    #[test]
    fn effective_lif_identity_when_none() {
        let base = base_lif();
        let eff = effective_lif(&base, None);
        assert_eq!(eff.tau_ms, base.tau_ms);
        assert_eq!(eff.v_thresh, base.v_thresh);
        assert_eq!(eff.refrac_steps, base.refrac_steps);
    }

    #[test]
    fn effective_lif_identity_when_full_health() {
        let base = base_lif();
        let effects = identity_effects();
        let eff = effective_lif(&base, Some(&effects));
        assert_eq!(eff.tau_ms, base.tau_ms);
        assert_eq!(eff.v_thresh, base.v_thresh);
        assert_eq!(eff.refrac_steps, base.refrac_steps);
    }

    #[test]
    fn effective_lif_shorter_tau_under_membrane_stress() {
        let base = base_lif();
        let mut effects = identity_effects();
        effects.1.membrane_time_constant_mod = 0.6;
        let eff = effective_lif(&base, Some(&effects));
        assert!(eff.tau_ms < base.tau_ms);
    }

    #[test]
    fn effective_lif_threshold_moves_toward_rest() {
        let base = base_lif(); // v_rest=-70, v_thresh=-50 → gap=20
        let mut effects = identity_effects();
        effects.1.spike_threshold_mod = 0.9; // 10% smaller gap
        let eff = effective_lif(&base, Some(&effects));
        assert!(eff.v_thresh < base.v_thresh); // closer to rest (more negative)
        assert!((eff.v_thresh - (-52.0)).abs() < 1e-3);
    }

    #[test]
    fn effective_lif_refractory_lengthens_under_pump_stress() {
        let base = LifParams {
            refrac_steps: 10,
            ..base_lif()
        };
        let mut effects = identity_effects();
        effects.0.refractory_period_mod = 1.3;
        let eff = effective_lif(&base, Some(&effects));
        assert_eq!(eff.refrac_steps, 13);
    }

    #[test]
    fn effective_rstdp_scales_lr_by_plasticity_mod() {
        let base = base_rstdp();
        let mut effects = identity_effects();
        effects.1.plasticity_mod = 0.5;
        let eff = effective_rstdp(&base, Some(&effects), true);
        assert!((eff.stdp.a_plus - base.stdp.a_plus * 0.5).abs() < 1e-6);
        assert!((eff.stdp.a_minus - base.stdp.a_minus * 0.5).abs() < 1e-6);
    }

    #[test]
    fn effective_rstdp_identity_when_opted_out() {
        let base = base_rstdp();
        let mut effects = identity_effects();
        effects.1.plasticity_mod = 0.1;
        let eff = effective_rstdp(&base, Some(&effects), false);
        assert_eq!(eff.stdp.a_plus, base.stdp.a_plus);
    }

    #[test]
    fn effective_reward_scales_by_ca_handling() {
        let mut effects = identity_effects();
        effects.1.ca_handling_mod = 0.5;
        let r = effective_reward(0.8, Some(&effects), true);
        assert!((r - 0.4).abs() < 1e-6);
    }

    #[test]
    fn effective_reward_identity_when_opted_out() {
        let mut effects = identity_effects();
        effects.1.ca_handling_mod = 0.0;
        let r = effective_reward(0.8, Some(&effects), false);
        assert_eq!(r, 0.8);
    }

    #[test]
    fn emergency_silence_fires_below_threshold() {
        let mut effects = identity_effects();
        effects.0.overall_capacity = 0.05;
        assert!(emergency_silence(Some(&effects), 0.1));
    }

    #[test]
    fn emergency_silence_quiet_at_full_health() {
        let effects = identity_effects();
        assert!(!emergency_silence(Some(&effects), 0.1));
    }

    #[test]
    fn emergency_silence_no_fire_when_effects_none() {
        assert!(!emergency_silence(None, 0.1));
    }

    // V1 commit 43785ee5e: zero-cache sentinel — adapter must fall back
    // to base when the dendrite cache was never populated.
    fn zero_cache_effects() -> (AxonSubstrateEffects, DendriteSubstrateEffects) {
        let dend = DendriteSubstrateEffects {
            membrane_time_constant_mod: 0.0,
            space_constant_mod: 0.0,
            integration_efficiency: 0.0,
            attenuation_mod: 0.0,
            nmda_mg_block_mod: 0.0,
            spike_threshold_mod: 0.0,
            na_channel_availability: 0.0,
            ca_pump_efficiency: 0.0,
            ca_buffer_capacity: 0.0,
            ca_handling_mod: 0.0,
            ltp_capacity: 0.0,
            ltd_capacity: 0.0,
            spine_growth_capacity: 0.0,
            plasticity_mod: 0.0,
            overall_capacity: 0.0,
        };
        (AxonSubstrateEffects::default(), dend)
    }

    #[test]
    fn effective_lif_falls_back_on_zero_cache() {
        let base = base_lif();
        let effects = zero_cache_effects();
        let eff = effective_lif(&base, Some(&effects));
        assert_eq!(eff.tau_ms, base.tau_ms);
        assert_eq!(eff.v_thresh, base.v_thresh);
        assert_eq!(eff.refrac_steps, base.refrac_steps);
    }

    #[test]
    fn effective_rstdp_falls_back_on_zero_cache() {
        let base = base_rstdp();
        let effects = zero_cache_effects();
        let eff = effective_rstdp(&base, Some(&effects), true);
        assert_eq!(eff.stdp.a_plus, base.stdp.a_plus);
        assert_eq!(eff.stdp.a_minus, base.stdp.a_minus);
    }

    #[test]
    fn effective_reward_falls_back_on_zero_cache() {
        let effects = zero_cache_effects();
        let r = effective_reward(0.8, Some(&effects), true);
        assert_eq!(r, 0.8);
    }
}
