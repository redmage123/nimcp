//! Substrate + thalamic modulation helpers for the LNN.
//!
//! Port of the Phase 2 adapter plan from
//! `docs/design/substrate_thalamic_integration.md §5.2`. The LNN is a
//! single-compartment network; substrate state + thalamic channel are
//! held at the [`crate::LnnNetwork`] level (not per-layer).
//!
//! # What this module is NOT
//!
//! Not a per-neuron cable model. V1's full substrate has per-neuron
//! chemistry granularity; V2's LNN adapter aggregates to a single
//! `NeuralSubstrate` per network. That matches the LNN's natural
//! "one region" footprint in V2_PLAN §5.
//!
//! # Modulations
//!
//! - `tau_eff[i] = max(tau_floor, tau_base[i] × dend.membrane_time_constant_mod)` —
//!   LNN has **learned** tau, so we *multiply* the learned value rather
//!   than overwrite it. Floor `LTC_TAU_MIN` from [`crate::ltc`].
//! - `capacitance_factor = axon.membrane_capacitance_mod` — scales the
//!   effective integration `dt` (smaller `C_m` → faster integration).
//! - `lr_eff = lr × dend.plasticity_mod` — applied to Adam / SGD at the
//!   trainer layer. Asymmetric LTP/LTD gating uses `ltp_capacity` for
//!   weight-increasing gradients and `ltd_capacity` for decreasing.
//!
//! All helpers are identity on `None` or full-health effects — the
//! disable path is bit-identical to pre-Phase-2 behavior.

use ndarray::Array1;
use nimcp_substrate::{AxonSubstrateEffects, DendriteSubstrateEffects};
use nimcp_thalamic::ThalamicChannel;

use crate::ltc::{LTC_TAU_MIN, LtcLayer};

/// Per-neuron effective tau for one LTC layer.
///
/// Pure function; returns a freshly allocated `Vec<f32>` of length
/// `layer.tau_base.len()`. Callers pass this into a tau-override aware
/// variant of [`crate::ltc::ltc_forward_step`] (see
/// [`ltc_forward_step_with_tau_override`]).
///
/// Identity when `effects` is `None`: returns a clone of `tau_base`.
#[must_use]
pub fn effective_tau(
    layer: &LtcLayer,
    effects: Option<&(AxonSubstrateEffects, DendriteSubstrateEffects)>,
) -> Array1<f32> {
    let Some((_axon, dend)) = effects else {
        return layer.tau_base.clone();
    };
    // Sentinel guard (V1 commit 43785ee5e): zero-cache → return base
    // tau unscaled. Otherwise we'd multiply learned tau values by 0
    // and floor everything to LTC_TAU_MIN.
    if dend.is_zero_cache() {
        return layer.tau_base.clone();
    }
    let mod_factor = dend.membrane_time_constant_mod;
    layer
        .tau_base
        .iter()
        .map(|&t| (t * mod_factor).max(LTC_TAU_MIN))
        .collect()
}

/// Capacitance-corrected effective timestep. A `membrane_capacitance_mod`
/// of `1.0` is identity; a smaller mod speeds up integration (less
/// capacitance = voltage reaches equilibrium faster).
///
/// Clamped to `[0.5 × dt_ms, 2.0 × dt_ms]` to avoid numerical instability
/// at extreme substrate states.
#[must_use]
pub fn effective_dt_ms(
    dt_ms: f32,
    effects: Option<&(AxonSubstrateEffects, DendriteSubstrateEffects)>,
) -> f32 {
    let Some((axon, _dend)) = effects else {
        return dt_ms;
    };
    let c = axon.membrane_capacitance_mod.clamp(0.5, 2.0);
    // Smaller capacitance → larger effective dt (voltage moves faster
    // per unit time). This is the inverse relationship.
    (dt_ms / c).clamp(dt_ms * 0.5, dt_ms * 2.0)
}

/// Effective learning rate with plasticity modulation applied. Returns
/// `base_lr` unchanged when `effects` is `None` or `apply = false`.
#[must_use]
pub fn effective_lr(
    base_lr: f32,
    effects: Option<&(AxonSubstrateEffects, DendriteSubstrateEffects)>,
    apply: bool,
) -> f32 {
    if !apply {
        return base_lr;
    }
    let Some((_axon, dend)) = effects else {
        return base_lr;
    };
    // Sentinel guard: zero-cache → return base LR unscaled.
    if dend.is_zero_cache() {
        return base_lr;
    }
    base_lr * dend.plasticity_mod.clamp(0.0, 1.0)
}

/// Asymmetric LTP/LTD gating. Returns `(ltp_scale, ltd_scale)` for
/// gradients where positive values potentiate (LTP) and negative
/// values depress (LTD).
///
/// When `apply = false` or `effects = None` returns `(1.0, 1.0)` —
/// identity. Callers multiply positive gradient components by
/// `ltp_scale` and negative components by `ltd_scale`.
#[must_use]
pub fn ltp_ltd_gates(
    effects: Option<&(AxonSubstrateEffects, DendriteSubstrateEffects)>,
    apply: bool,
) -> (f32, f32) {
    if !apply {
        return (1.0, 1.0);
    }
    let Some((_axon, dend)) = effects else {
        return (1.0, 1.0);
    };
    // Sentinel guard: zero-cache → identity gate.
    if dend.is_zero_cache() {
        return (1.0, 1.0);
    }
    (
        dend.ltp_capacity.clamp(0.0, 1.0),
        dend.ltd_capacity.clamp(0.0, 1.0),
    )
}

/// Apply attention-weighted scaling to an input vector at the LNN
/// entry point.
///
/// The convention: when a thalamic channel is present, the input gets
/// multiplied by the mean of the channel's first `n_valid` attention
/// weights — "how much attention does the thalamus pay to feeding
/// this network right now". In [`nimcp_thalamic::RelayMode::Burst`]
/// the weights are ignored and the input is amplified by `1.2`
/// (biological: burst relays up-modulate afferents).
///
/// Identity when `channel` is `None`.
#[must_use]
pub fn attention_scale_input(
    input: &Array1<f32>,
    channel: Option<&ThalamicChannel>,
) -> Array1<f32> {
    let Some(ch) = channel else {
        return input.clone();
    };
    if ch.n_destinations == 0 {
        return input.clone();
    }
    match ch.mode {
        nimcp_thalamic::RelayMode::Burst => input * 1.2,
        _ => {
            #[allow(clippy::cast_precision_loss)]
            let n = ch.n_destinations as f32;
            let sum: f32 = ch
                .attention_weights
                .iter()
                .take(ch.n_destinations as usize)
                .sum();
            let mean_attn = (sum / n).clamp(0.0, 1.0);
            input * mean_attn
        }
    }
}

/// Tau clamp for [`nimcp_thalamic::RelayMode::Burst`] — biological
/// intuition: burst-relay destinations run with shorter time constants
/// (more responsive). Returns the floor to clamp per-layer `tau_eff`
/// to, or `None` if no clamp applies.
///
/// Used by adapters that consult both `effective_tau` + the channel:
/// after computing `tau_eff`, clamp every entry to at most this value
/// when `Some`.
#[must_use]
pub fn burst_tau_clamp_ms(channel: Option<&ThalamicChannel>) -> Option<f32> {
    let ch = channel?;
    if ch.mode == nimcp_thalamic::RelayMode::Burst {
        // Burst clamp: 1 ms ceiling. Clamped up from the floor
        // `LTC_TAU_MIN` at the caller.
        Some(1.0)
    } else {
        None
    }
}

// -------------------------------------------------------------------------
// ltc_forward_step variant with substrate overrides
// -------------------------------------------------------------------------

/// Like [`crate::ltc::ltc_forward_step`] but:
///
/// - Substitutes `tau_override` for `layer.tau_base` when supplied.
/// - Uses `dt_ms_override` instead of raw `dt_ms` when different from
///   `dt_ms`.
///
/// Pre-allocations are avoided; the method clones the tau slice into
/// the computation path only when needed. The override-less case (both
/// `None`) delegates to the base kernel for bit-identical behavior.
pub fn ltc_forward_step_with_tau_override(
    state: &mut crate::ltc::LtcState,
    layer: &crate::ltc::LtcLayer,
    u: &Array1<f32>,
    dt_ms: f32,
    tau_override: Option<&Array1<f32>>,
    dt_ms_override: Option<f32>,
) -> Array1<f32> {
    // Fast path — no substrate overrides, delegate.
    if tau_override.is_none() && dt_ms_override.is_none() {
        return crate::ltc::ltc_forward_step(state, layer, u, dt_ms);
    }
    let dt_eff = dt_ms_override.unwrap_or(dt_ms);

    // Pre-activation: W_rec · x + W_in · u + b
    let mut pre = layer.w_rec.dot(&state.x);
    pre += &layer.w_in.dot(u);
    pre += &layer.b;
    let act = pre.mapv(f32::tanh);

    // dx/dt = -x / tau_safe + tanh(pre). Euler step.
    let tau_source: &Array1<f32> = tau_override.unwrap_or(&layer.tau_base);
    for ((x, &tau), &a) in state.x.iter_mut().zip(tau_source.iter()).zip(act.iter()) {
        let tau_safe = tau.max(LTC_TAU_MIN);
        let dx = -*x / tau_safe + a;
        let x_new = *x + dt_eff * dx;
        *x = x_new.clamp(-crate::ltc::LTC_STATE_CLAMP, crate::ltc::LTC_STATE_CLAMP);
    }
    pre
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use crate::ltc::{LtcLayer, LtcParams, LtcState};

    fn layer() -> LtcLayer {
        LtcLayer::new_seeded(
            LtcParams {
                n_in: 2,
                n_rec: 4,
                tau_init: 1.0,
                init_scale: 0.5,
            },
            0,
        )
    }

    fn identity_effects() -> (AxonSubstrateEffects, DendriteSubstrateEffects) {
        (
            AxonSubstrateEffects::default(),
            DendriteSubstrateEffects::default(),
        )
    }

    #[test]
    fn effective_tau_identity_when_none() {
        let l = layer();
        let t = effective_tau(&l, None);
        assert_eq!(t, l.tau_base);
    }

    #[test]
    fn effective_tau_identity_at_full_health() {
        let l = layer();
        let eff = identity_effects();
        let t = effective_tau(&l, Some(&eff));
        assert_eq!(t, l.tau_base);
    }

    #[test]
    fn effective_tau_shortens_under_membrane_stress() {
        let l = layer();
        let mut eff = identity_effects();
        eff.1.membrane_time_constant_mod = 0.5;
        let t = effective_tau(&l, Some(&eff));
        for (e, b) in t.iter().zip(l.tau_base.iter()) {
            assert!(e <= b, "tau_eff should be <= tau_base under stress");
        }
    }

    #[test]
    fn effective_tau_respects_floor() {
        let l = layer();
        let mut eff = identity_effects();
        eff.1.membrane_time_constant_mod = 0.0001;
        let t = effective_tau(&l, Some(&eff));
        for &e in t.iter() {
            assert!(e >= LTC_TAU_MIN);
        }
    }

    #[test]
    fn effective_dt_identity_when_none() {
        assert_eq!(effective_dt_ms(0.1, None), 0.1);
    }

    #[test]
    fn effective_dt_scales_with_capacitance() {
        let mut eff = identity_effects();
        eff.0.membrane_capacitance_mod = 0.8;
        let dt = effective_dt_ms(1.0, Some(&eff));
        assert!(dt > 1.0, "smaller C_m should increase effective dt");
    }

    #[test]
    fn effective_dt_clamps_at_extremes() {
        let mut eff = identity_effects();
        eff.0.membrane_capacitance_mod = 0.1;
        let dt = effective_dt_ms(1.0, Some(&eff));
        assert!(dt <= 2.0); // clamp at 2x
    }

    #[test]
    fn effective_lr_identity_when_opted_out() {
        let mut eff = identity_effects();
        eff.1.plasticity_mod = 0.2;
        assert_eq!(effective_lr(0.01, Some(&eff), false), 0.01);
    }

    #[test]
    fn effective_lr_scales_with_plasticity_mod() {
        let mut eff = identity_effects();
        eff.1.plasticity_mod = 0.5;
        let lr = effective_lr(0.02, Some(&eff), true);
        assert!((lr - 0.01).abs() < 1e-6);
    }

    #[test]
    fn ltp_ltd_gates_identity_when_opted_out() {
        let mut eff = identity_effects();
        eff.1.ltp_capacity = 0.3;
        eff.1.ltd_capacity = 0.4;
        assert_eq!(ltp_ltd_gates(Some(&eff), false), (1.0, 1.0));
    }

    #[test]
    fn ltp_ltd_gates_read_dend_capacities() {
        let mut eff = identity_effects();
        eff.1.ltp_capacity = 0.3;
        eff.1.ltd_capacity = 0.7;
        let (p, d) = ltp_ltd_gates(Some(&eff), true);
        assert!((p - 0.3).abs() < 1e-6);
        assert!((d - 0.7).abs() < 1e-6);
    }

    #[test]
    fn attention_scale_identity_when_channel_none() {
        let x = Array1::from_vec(vec![1.0, 2.0, 3.0]);
        let y = attention_scale_input(&x, None);
        assert_eq!(x, y);
    }

    #[test]
    fn attention_scale_burst_amplifies() {
        let ch = nimcp_thalamic::ThalamicChannel {
            mode: nimcp_thalamic::RelayMode::Burst,
            ..nimcp_thalamic::ThalamicChannel::new(0, &[1]).unwrap()
        };
        let x = Array1::from_vec(vec![1.0]);
        let y = attention_scale_input(&x, Some(&ch));
        assert!((y[0] - 1.2).abs() < 1e-6);
    }

    #[test]
    fn attention_scale_tonic_uses_mean_weight() {
        let mut ch = nimcp_thalamic::ThalamicChannel::new(0, &[1, 2]).unwrap();
        ch.set_gate(1, 0.5);
        ch.set_gate(2, 0.7);
        // mean = 0.6
        let x = Array1::from_vec(vec![10.0]);
        let y = attention_scale_input(&x, Some(&ch));
        assert!((y[0] - 6.0).abs() < 1e-6);
    }

    #[test]
    fn burst_tau_clamp_matches_mode() {
        let ch_burst = nimcp_thalamic::ThalamicChannel {
            mode: nimcp_thalamic::RelayMode::Burst,
            ..nimcp_thalamic::ThalamicChannel::new(0, &[1]).unwrap()
        };
        assert_eq!(burst_tau_clamp_ms(Some(&ch_burst)), Some(1.0));

        let ch_tonic = nimcp_thalamic::ThalamicChannel::new(0, &[1]).unwrap();
        assert_eq!(burst_tau_clamp_ms(Some(&ch_tonic)), None);

        assert_eq!(burst_tau_clamp_ms(None), None);
    }

    // V1 commit 43785ee5e — zero-cache sentinel for LNN adapter.
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
    fn effective_tau_falls_back_on_zero_cache() {
        let l = layer();
        let eff = zero_cache_effects();
        let t = effective_tau(&l, Some(&eff));
        assert_eq!(t, l.tau_base);
    }

    #[test]
    fn effective_lr_falls_back_on_zero_cache() {
        let eff = zero_cache_effects();
        assert_eq!(effective_lr(0.02, Some(&eff), true), 0.02);
    }

    #[test]
    fn ltp_ltd_gates_fall_back_on_zero_cache() {
        let eff = zero_cache_effects();
        assert_eq!(ltp_ltd_gates(Some(&eff), true), (1.0, 1.0));
    }

    #[test]
    fn forward_step_override_identity_when_none() {
        let l = layer();
        let mut st_a = LtcState::new(4);
        let mut st_b = LtcState::new(4);
        let u = Array1::from_vec(vec![0.3, -0.4]);
        let pre_a = ltc_forward_step_with_tau_override(&mut st_a, &l, &u, 0.1, None, None);
        let pre_b = crate::ltc::ltc_forward_step(&mut st_b, &l, &u, 0.1);
        assert_eq!(pre_a, pre_b);
        assert_eq!(st_a.x, st_b.x);
    }
}
