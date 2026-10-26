//! Substrate → effects pure-function pipeline + bidirectional debit.
//!
//! [`compute_effects`] takes a (clamped) [`NeuralSubstrate`] and
//! produces the two effect bundles consumers read each step.
//! [`debit_activity`] is the reverse path: each network tick reports
//! spike + plasticity counts, which subtract from ATP / ion balance
//! / membrane integrity per the costs in [`NeuralSubstrateConfig`].
//!
//! # Formulas
//!
//! Formulas are deliberately simple: monotone, clamped to documented
//! ranges, returning identity (all multipliers = 1.0) at full health.
//! They are NOT bit-for-bit copies of V1's bridge formulas — V1 has
//! hundreds of lines of tuning knobs we don't need for V2 integration.
//! The adapter tests only care that a degraded substrate produces
//! multipliers < 1 on the relevant fields, not that a specific ATP
//! value maps to a specific Q10 number.
//!
//! # Key relationships captured
//!
//! Axon:
//! - `temperature_q10_factor`: Q10 ≈ 3 around 37°C, shifted to [0.5, 1.5].
//! - `atp_velocity_factor`, `pump_activity`: linear in ATP (pump is
//!   ATP-consuming).
//! - `myelin_efficiency`: linear in myelin_integrity.
//! - `spike_reliability`: `ion_balance × ap_amplitude_mod`.
//! - `refractory_period_mod`: `1.0 + (1 - pump_activity) × 0.3` — pump
//!   failure lengthens refractory.
//! - `membrane_leak_mod`: `1.0 + 2 × (1 - membrane_integrity)` —
//!   damaged membrane leaks more.
//! - `overall_velocity_mod` / `overall_capacity`: products of the main
//!   degrading factors.
//!
//! Dendrite:
//! - `membrane_time_constant_mod`: tied to membrane_integrity + Ca
//!   handling.
//! - `ca_handling_mod`: `ca_homeostasis × ca_pump_efficiency × ca_buffer_capacity`.
//! - `plasticity_mod`: gated by ATP (plasticity is expensive) AND by
//!   Ca handling (LTP/LTD need clean Ca spikes).
//! - `ltp_capacity` / `ltd_capacity`: both scaled by plasticity_mod,
//!   but LTD is slightly cheaper so it falls off last.

use crate::effects::{AxonSubstrateEffects, DendriteSubstrateEffects};
use crate::state::{NeuralSubstrate, NeuralSubstrateConfig, REST_TEMPERATURE_C};

/// Produce `(axon, dendrite)` effect bundles from the current substrate
/// state. Pure — the input is read via `&`, outputs are owned.
///
/// Output structs are clamped via [`AxonSubstrateEffects::clamp`] and
/// [`DendriteSubstrateEffects::clamp`] before return so adapters need
/// no defensive checks.
///
/// At full health ([`NeuralSubstrate::default`]) both outputs are the
/// [`Default`] identity — every multiplier equals `1.0`.
#[must_use]
pub fn compute_effects(
    substrate: &NeuralSubstrate,
) -> (AxonSubstrateEffects, DendriteSubstrateEffects) {
    let mut s = *substrate;
    s.clamp();

    // ------------------- Axon -------------------

    // Q10 effect: at 37°C → 1.0; at 30°C ≈ 0.85; at 40°C ≈ 1.13. Clamp
    // at [0.5, 1.5] via the struct.
    let t_delta = s.temperature - REST_TEMPERATURE_C;
    let q10_raw = 3.0_f32.powf(t_delta / 10.0).clamp(0.5, 1.5);

    let atp = s.atp_level;
    let myelin = s.myelin_integrity;
    let ion = s.ion_balance;
    let pump = s.na_k_pump_activity;
    let membrane = s.membrane_integrity;

    let atp_vel = atp; // linear
    let overall_velocity = q10_raw * atp_vel * (0.5 + 0.5 * myelin);

    let ap_amp = ion * (0.5 + 0.5 * pump); // gradient + pump reset quality
    let spike_rel = ion * ap_amp;
    let refractory = 1.0 + (1.0 - pump) * 0.3;
    let transport = atp * myelin;
    let kinesin = atp * s.astrocytic_support;
    // Capacitance slight range [0.8, 1.2]. 1.0 at full health,
    // drops to 0.8 as membrane degrades (lost thickness / area).
    let cap_mod = 1.0 - 0.2 * (1.0 - membrane);
    let leak_mod = 1.0 + 2.0 * (1.0 - membrane); // leak increases as membrane fails

    let axon_overall = (atp * ion * membrane * myelin).powf(0.25);

    let mut axon = AxonSubstrateEffects {
        temperature_q10_factor: q10_raw,
        atp_velocity_factor: atp_vel,
        myelin_efficiency: myelin,
        overall_velocity_mod: overall_velocity,
        ion_gradient_strength: ion,
        ap_amplitude_mod: ap_amp,
        spike_reliability: spike_rel,
        pump_activity: pump,
        refractory_period_mod: refractory,
        transport_efficiency: transport,
        kinesin_activity: kinesin,
        membrane_capacitance_mod: cap_mod,
        membrane_leak_mod: leak_mod,
        overall_capacity: axon_overall,
    };
    axon.clamp();

    // ------------------ Dendrite ------------------

    // tau_m modulation: slightly shorter tau (smaller mod) when
    // membrane degrades (more leak) — 1 → 1.0, 0 → 0.5.
    let tau_mod = 0.5 + 0.5 * membrane;
    let space_mod = 0.5 + 0.5 * membrane;
    let integration = membrane * ion * (0.5 + 0.5 * atp);
    let attenuation = 1.0 + (1.0 - membrane); // 1.0 at full; up to 2.0 when collapsed

    // NMDA Mg block: 1.0 at full health; drops as Ca handling fails
    // (range [0.8, 1.2] per V1 doc, but full health = 1.0).
    let nmda_mg = 1.0 - 0.2 * (1.0 - s.ca_homeostasis);
    // Spike threshold modulation: 1.0 at full; drops as ion balance
    // degrades (threshold easier to cross without Na+ gradient).
    let thresh_mod = 1.0 - 0.2 * (1.0 - ion);
    let na_channel = membrane; // dendritic Na+ availability tracks membrane
    let ca_pump_eff = s.ca_homeostasis * atp; // pump needs ATP
    let ca_buffer = s.ca_homeostasis; // simplified
    let ca_handling = s.ca_homeostasis * ca_pump_eff.sqrt() * ca_buffer;

    let plasticity = atp * ca_handling * s.astrocytic_support;
    let ltp = plasticity; // both gated by ATP
    let ltd = plasticity.sqrt(); // LTD is slightly cheaper — falls off slower
    let spine_growth = plasticity * s.glucose_level;

    let dend_overall = (membrane * ion * atp * s.ca_homeostasis).powf(0.25);

    let mut dend = DendriteSubstrateEffects {
        membrane_time_constant_mod: tau_mod,
        space_constant_mod: space_mod,
        integration_efficiency: integration,
        attenuation_mod: attenuation,
        nmda_mg_block_mod: nmda_mg,
        spike_threshold_mod: thresh_mod,
        na_channel_availability: na_channel,
        ca_pump_efficiency: ca_pump_eff,
        ca_buffer_capacity: ca_buffer,
        ca_handling_mod: ca_handling,
        ltp_capacity: ltp,
        ltd_capacity: ltd,
        spine_growth_capacity: spine_growth,
        plasticity_mod: plasticity,
        overall_capacity: dend_overall,
    };
    dend.clamp();

    (axon, dend)
}

/// Debit activity into the substrate: decrement ATP, ion balance, and
/// membrane integrity per the per-spike / per-plasticity costs in
/// `cfg`. Adds back the passive recovery term so a lightly-loaded
/// region stays near full health without requiring the chemistry
/// tick to be running.
///
/// Values are **clamped to `[0, 1]`** after each update. The substrate
/// cannot go negative, and cannot exceed full health from passive
/// recovery alone.
pub fn debit_activity(
    substrate: &mut NeuralSubstrate,
    cfg: &NeuralSubstrateConfig,
    n_spikes: u64,
    n_plasticity: u64,
) {
    #[allow(clippy::cast_precision_loss)]
    let spikes_f = n_spikes as f32;
    #[allow(clippy::cast_precision_loss)]
    let plast_f = n_plasticity as f32;

    let atp_debit = spikes_f * cfg.atp_cost_per_spike + plast_f * cfg.atp_cost_per_plasticity;
    let ion_debit = spikes_f * cfg.ion_cost_per_spike;
    let mem_debit = spikes_f * cfg.membrane_cost_per_spike;

    substrate.atp_level =
        (substrate.atp_level - atp_debit + cfg.atp_passive_recovery).clamp(0.0, 1.0);
    substrate.ion_balance = (substrate.ion_balance - ion_debit).clamp(0.0, 1.0);
    substrate.membrane_integrity = (substrate.membrane_integrity - mem_debit).clamp(0.0, 1.0);
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn full_health_produces_identity() {
        let s = NeuralSubstrate::default();
        let (a, d) = compute_effects(&s);
        assert!(a.is_identity(), "axon not identity at full health: {a:?}");
        assert!(
            d.is_identity(),
            "dendrite not identity at full health: {d:?}"
        );
    }

    #[test]
    fn low_atp_degrades_velocity_and_plasticity() {
        let s = NeuralSubstrate {
            atp_level: 0.2,
            ..Default::default()
        };
        let (a, d) = compute_effects(&s);
        assert!(a.atp_velocity_factor < 1.0);
        assert!(a.overall_velocity_mod < 1.0);
        assert!(a.overall_capacity < 1.0);
        assert!(d.plasticity_mod < 1.0);
        assert!(d.ltp_capacity < 1.0);
    }

    #[test]
    fn low_ion_balance_degrades_spike_reliability() {
        let s = NeuralSubstrate {
            ion_balance: 0.3,
            ..Default::default()
        };
        let (a, _) = compute_effects(&s);
        assert!(a.ion_gradient_strength < 1.0);
        assert!(a.spike_reliability < 1.0);
    }

    #[test]
    fn low_membrane_increases_leak() {
        let s = NeuralSubstrate {
            membrane_integrity: 0.4,
            ..Default::default()
        };
        let (a, d) = compute_effects(&s);
        assert!(a.membrane_leak_mod > 1.0, "leak should increase");
        assert!(d.space_constant_mod < 1.0);
        assert!(d.attenuation_mod > 1.0);
    }

    #[test]
    fn cold_temperature_reduces_velocity() {
        let s = NeuralSubstrate {
            temperature: 30.0, // 7°C below rest
            ..Default::default()
        };
        let (a, _) = compute_effects(&s);
        assert!(a.temperature_q10_factor < 1.0);
    }

    #[test]
    fn debit_reduces_atp() {
        let mut s = NeuralSubstrate::default();
        let cfg = NeuralSubstrateConfig {
            atp_passive_recovery: 0.0,
            ..Default::default()
        };
        debit_activity(&mut s, &cfg, 10_000, 0);
        assert!(s.atp_level < 1.0);
        assert!(s.atp_level > 0.0);
    }

    #[test]
    fn debit_respects_passive_recovery() {
        let mut s = NeuralSubstrate {
            atp_level: 0.5,
            ..Default::default()
        };
        let cfg = NeuralSubstrateConfig {
            atp_passive_recovery: 0.1,
            atp_cost_per_spike: 0.0,
            ..Default::default()
        };
        debit_activity(&mut s, &cfg, 0, 0);
        assert!(s.atp_level > 0.5);
    }

    #[test]
    fn debit_clamps_at_zero() {
        let mut s = NeuralSubstrate {
            atp_level: 0.01,
            ..Default::default()
        };
        let cfg = NeuralSubstrateConfig {
            atp_cost_per_spike: 1.0,
            atp_passive_recovery: 0.0,
            ..Default::default()
        };
        debit_activity(&mut s, &cfg, 1000, 0);
        assert_eq!(s.atp_level, 0.0);
    }

    #[test]
    fn debit_clamps_at_one() {
        let mut s = NeuralSubstrate {
            atp_level: 0.99,
            ..Default::default()
        };
        let cfg = NeuralSubstrateConfig {
            atp_cost_per_spike: 0.0,
            atp_passive_recovery: 1.0,
            ..Default::default()
        };
        debit_activity(&mut s, &cfg, 0, 0);
        assert_eq!(s.atp_level, 1.0);
    }

    #[test]
    fn compute_is_monotone_in_atp() {
        let s_high = NeuralSubstrate {
            atp_level: 0.9,
            ..Default::default()
        };
        let s_low = NeuralSubstrate {
            atp_level: 0.3,
            ..Default::default()
        };
        let (a_high, d_high) = compute_effects(&s_high);
        let (a_low, d_low) = compute_effects(&s_low);
        assert!(a_high.atp_velocity_factor >= a_low.atp_velocity_factor);
        assert!(a_high.overall_velocity_mod >= a_low.overall_velocity_mod);
        assert!(d_high.plasticity_mod >= d_low.plasticity_mod);
    }

    #[test]
    fn warm_temperature_boosts_velocity() {
        let s_hot = NeuralSubstrate {
            temperature: 40.0,
            ..Default::default()
        };
        let s_cool = NeuralSubstrate {
            temperature: 34.0,
            ..Default::default()
        };
        let (a_hot, _) = compute_effects(&s_hot);
        let (a_cool, _) = compute_effects(&s_cool);
        assert!(a_hot.temperature_q10_factor > a_cool.temperature_q10_factor);
    }
}
