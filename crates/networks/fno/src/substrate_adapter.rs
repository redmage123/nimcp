//! Substrate + thalamic modulation helpers for the FNO (Phase
//! 11-substrate).
//!
//! Like the CNN, the FNO is a feed-forward operator with no membrane /
//! tau dynamics, so the substrate surface is the scalar set:
//!
//! - **Inference** — a thalamic attention scalar gates the input field
//!   and `dend.integration_efficiency` attenuates the output field
//!   (signal fidelity falls as the substrate degrades).
//! - **Training** — `dend.plasticity_mod` scales the learning rate and
//!   `(ltp_capacity, ltd_capacity)` apply asymmetric gating to the sign
//!   of each gradient component.
//!
//! Every helper is **identity** on `None` / full-health / zero-cache
//! effects — the disable path is bit-identical to pre-Phase-11-substrate
//! behaviour. The zero-cache sentinel mirrors V1 commit `43785ee5e`.

use nimcp_substrate::{AxonSubstrateEffects, DendriteSubstrateEffects};
use nimcp_thalamic::ThalamicChannel;

/// Convenience alias for the `(axon, dendrite)` effect pair the
/// substrate produces.
pub type Effects = (AxonSubstrateEffects, DendriteSubstrateEffects);

/// Thalamic attention scalar applied to the whole input field.
///
/// - `None` channel → `1.0` (identity).
/// - [`nimcp_thalamic::RelayMode::Burst`] → `1.2`.
/// - Otherwise → mean of the valid attention weights, clamped to
///   `[0, 1]`.
#[must_use]
pub fn attention_scalar(channel: Option<&ThalamicChannel>) -> f32 {
    let Some(ch) = channel else {
        return 1.0;
    };
    if ch.n_destinations == 0 {
        return 1.0;
    }
    match ch.mode {
        nimcp_thalamic::RelayMode::Burst => 1.2,
        _ => {
            #[allow(clippy::cast_precision_loss)]
            let n = ch.n_destinations as f32;
            let sum: f32 = ch
                .attention_weights
                .iter()
                .take(ch.n_destinations as usize)
                .sum();
            (sum / n).clamp(0.0, 1.0)
        }
    }
}

/// Output signal gain from `dend.integration_efficiency`. Returns `1.0`
/// when `apply = false`, `effects` is `None`, or the cache is zeroed.
#[must_use]
pub fn integration_gain(effects: Option<&Effects>, apply: bool) -> f32 {
    if !apply {
        return 1.0;
    }
    let Some((_axon, dend)) = effects else {
        return 1.0;
    };
    if dend.is_zero_cache() {
        return 1.0;
    }
    dend.integration_efficiency.clamp(0.0, 1.0)
}

/// Effective learning rate with `dend.plasticity_mod` applied. Returns
/// `base_lr` unchanged when `apply = false`, `effects` is `None`, or the
/// cache is zeroed.
#[must_use]
pub fn effective_lr(base_lr: f32, effects: Option<&Effects>, apply: bool) -> f32 {
    if !apply {
        return base_lr;
    }
    let Some((_axon, dend)) = effects else {
        return base_lr;
    };
    if dend.is_zero_cache() {
        return base_lr;
    }
    base_lr * dend.plasticity_mod.clamp(0.0, 1.0)
}

/// Asymmetric LTP/LTD gates. Returns `(ltp_scale, ltd_scale)`; callers
/// multiply potentiating gradient components by `ltp_scale` and
/// depressing ones by `ltd_scale`. Identity `(1.0, 1.0)` when
/// `apply = false`, `effects` is `None`, or the cache is zeroed.
#[must_use]
pub fn ltp_ltd_gates(effects: Option<&Effects>, apply: bool) -> (f32, f32) {
    if !apply {
        return (1.0, 1.0);
    }
    let Some((_axon, dend)) = effects else {
        return (1.0, 1.0);
    };
    if dend.is_zero_cache() {
        return (1.0, 1.0);
    }
    (
        dend.ltp_capacity.clamp(0.0, 1.0),
        dend.ltd_capacity.clamp(0.0, 1.0),
    )
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    fn identity_effects() -> Effects {
        (
            AxonSubstrateEffects::default(),
            DendriteSubstrateEffects::default(),
        )
    }

    fn zero_cache_effects() -> Effects {
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
    fn attention_identity_when_none() {
        assert_eq!(attention_scalar(None), 1.0);
    }

    #[test]
    fn attention_burst_amplifies() {
        let ch = ThalamicChannel {
            mode: nimcp_thalamic::RelayMode::Burst,
            ..ThalamicChannel::new(0, &[1]).unwrap()
        };
        assert_eq!(attention_scalar(Some(&ch)), 1.2);
    }

    #[test]
    fn attention_tonic_mean_weight() {
        let mut ch = ThalamicChannel::new(0, &[1, 2]).unwrap();
        ch.set_gate(1, 0.2);
        ch.set_gate(2, 0.8);
        assert!((attention_scalar(Some(&ch)) - 0.5).abs() < 1e-6);
    }

    #[test]
    fn integration_gain_reads_dend_efficiency() {
        let mut eff = identity_effects();
        eff.1.integration_efficiency = 0.6;
        assert!((integration_gain(Some(&eff), true) - 0.6).abs() < 1e-6);
        assert_eq!(integration_gain(Some(&eff), false), 1.0);
    }

    #[test]
    fn effective_lr_scales_with_plasticity() {
        let mut eff = identity_effects();
        eff.1.plasticity_mod = 0.25;
        assert!((effective_lr(0.04, Some(&eff), true) - 0.01).abs() < 1e-6);
    }

    #[test]
    fn ltp_ltd_reads_capacities() {
        let mut eff = identity_effects();
        eff.1.ltp_capacity = 0.4;
        eff.1.ltd_capacity = 0.9;
        let (p, d) = ltp_ltd_gates(Some(&eff), true);
        assert!((p - 0.4).abs() < 1e-6);
        assert!((d - 0.9).abs() < 1e-6);
    }

    // V1 commit 43785ee5e — zero-cache sentinel falls back to base.
    #[test]
    fn zero_cache_falls_back_everywhere() {
        let eff = zero_cache_effects();
        assert_eq!(integration_gain(Some(&eff), true), 1.0);
        assert_eq!(effective_lr(0.04, Some(&eff), true), 0.04);
        assert_eq!(ltp_ltd_gates(Some(&eff), true), (1.0, 1.0));
    }
}
