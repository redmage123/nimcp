//! Spike-rate adaptation — shared mechanism for AHP and Na/K pump.
//!
//! Single-responsibility module: spike-triggered exponentially-decaying
//! hyperpolarizing current. Used for both fast M-current adaptation
//! (`tau ~150 ms`) and slow Na+/K+ pump (`tau ~5 s`) — the math is
//! identical, only the parameters differ. Port of master commit
//! `a3a1de0e2 feat(snn): shared adaptation mechanism (AHP + pump)`.
//!
//! # Usage in the LIF step
//!
//! Each integration step a population invokes:
//!
//! 1. [`AdaptationState::compute_hyperpol`] **before** computing dV —
//!    fills an output buffer with `gain_mv × adapt_var[n]` per neuron.
//!    Subtract this from the synaptic input so adapted neurons see a
//!    hyperpolarized membrane.
//! 2. The LIF step computes whether each neuron fires.
//! 3. [`AdaptationState::update`] **after** the firing decision — decays
//!    `adapt_var` by `exp(-dt_ms / tau_ms)` and bumps it by `spike_bump`
//!    wherever the neuron fired.
//!
//! `adapt_var` starts at zero, so a freshly-created state contributes
//! no hyperpolarization until the first spike.

use serde::{Deserialize, Serialize};
use thiserror::Error;

/// Per-population adaptation state. One instance per mechanism — populations
/// that want both fast AHP and slow pump hold two of these.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AdaptationState {
    /// Per-neuron adaptation variable. Always `>= 0`.
    pub adapt_var: Vec<f32>,
    /// Exponential decay time constant (ms). Strictly positive.
    pub tau_ms: f32,
    /// mV of hyperpolarization per unit `adapt_var`.
    pub gain_mv: f32,
    /// Increment applied on each spike (typically `1.0`).
    pub spike_bump: f32,
}

/// Construction errors. Invalid parameters are caller bugs, not runtime
/// conditions, so they surface as a typed error rather than a panic.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum AdaptationError {
    /// Caller passed `n_neurons == 0` — meaningless for a per-neuron state.
    #[error("n_neurons must be > 0")]
    EmptyPopulation,
    /// `tau_ms` was zero or negative — would divide by zero in `update`.
    #[error("tau_ms must be > 0 (got {0})")]
    NonPositiveTau(String),
    /// `gain_mv` was negative — adaptation must hyperpolarize, not depolarize.
    #[error("gain_mv must be >= 0 (got {0})")]
    NegativeGain(String),
}

impl AdaptationState {
    /// Construct a fresh adaptation state with all `adapt_var = 0`.
    pub fn new(
        n_neurons: usize,
        tau_ms: f32,
        gain_mv: f32,
        spike_bump: f32,
    ) -> Result<Self, AdaptationError> {
        if n_neurons == 0 {
            return Err(AdaptationError::EmptyPopulation);
        }
        if tau_ms.partial_cmp(&0.0) != Some(std::cmp::Ordering::Greater) {
            return Err(AdaptationError::NonPositiveTau(format!("{tau_ms}")));
        }
        if gain_mv < 0.0 {
            return Err(AdaptationError::NegativeGain(format!("{gain_mv}")));
        }
        Ok(Self {
            adapt_var: vec![0.0; n_neurons],
            tau_ms,
            gain_mv,
            spike_bump,
        })
    }

    /// Number of neurons this state covers.
    #[must_use]
    pub fn n_neurons(&self) -> usize {
        self.adapt_var.len()
    }

    /// Zero `adapt_var` without changing the parameters.
    pub fn reset(&mut self) {
        self.adapt_var.fill(0.0);
    }

    /// Fill `out_hyperpol_mv[n] = gain_mv × adapt_var[n]`. Buffer is
    /// overwritten, not accumulated. Caller is responsible for length:
    /// the implementation copies up to `min(out.len(), n_neurons)` and
    /// returns the count actually written.
    pub fn compute_hyperpol(&self, out_hyperpol_mv: &mut [f32]) -> usize {
        let n = self.adapt_var.len().min(out_hyperpol_mv.len());
        let gain = self.gain_mv;
        for (out, &av) in out_hyperpol_mv
            .iter_mut()
            .zip(self.adapt_var.iter())
            .take(n)
        {
            *out = gain * av;
        }
        n
    }

    /// Per-step update: decay by `exp(-dt_ms / tau_ms)`, then bump by
    /// `spike_bump` wherever `fired[n] > 0.5`. Called AFTER the firing
    /// decision so adaptation tracks the spikes that just happened.
    /// Extra entries in `fired` past `n_neurons` are ignored; entries
    /// past `fired.len()` are treated as not-fired.
    pub fn update(&mut self, fired: &[f32], dt_ms: f32) {
        let decay = (-dt_ms / self.tau_ms).exp();
        let bump = self.spike_bump;
        for (i, v) in self.adapt_var.iter_mut().enumerate() {
            let mut nv = *v * decay;
            if fired.get(i).copied().unwrap_or(0.0) > 0.5 {
                nv += bump;
            }
            *v = nv;
        }
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn rejects_empty_population() {
        assert!(matches!(
            AdaptationState::new(0, 150.0, 0.6, 1.0),
            Err(AdaptationError::EmptyPopulation)
        ));
    }

    #[test]
    fn rejects_non_positive_tau() {
        assert!(matches!(
            AdaptationState::new(8, 0.0, 0.6, 1.0),
            Err(AdaptationError::NonPositiveTau(_))
        ));
        assert!(matches!(
            AdaptationState::new(8, -1.0, 0.6, 1.0),
            Err(AdaptationError::NonPositiveTau(_))
        ));
    }

    #[test]
    fn rejects_negative_gain() {
        assert!(matches!(
            AdaptationState::new(8, 150.0, -0.1, 1.0),
            Err(AdaptationError::NegativeGain(_))
        ));
    }

    #[test]
    fn starts_at_zero_no_hyperpolarization() {
        let a = AdaptationState::new(4, 150.0, 0.6, 1.0).unwrap();
        let mut out = [0.0_f32; 4];
        let n = a.compute_hyperpol(&mut out);
        assert_eq!(n, 4);
        assert_eq!(out, [0.0, 0.0, 0.0, 0.0]);
    }

    #[test]
    fn spike_bumps_adapt_var() {
        let mut a = AdaptationState::new(3, 1000.0, 0.6, 1.0).unwrap();
        a.update(&[1.0, 0.0, 1.0], 1.0);
        // Decay factor over 1ms with tau=1000ms ≈ exp(-0.001) ≈ 0.999;
        // starts at 0, so post-update is ~bump for fired neurons.
        assert!((a.adapt_var[0] - 1.0).abs() < 1e-3);
        assert_eq!(a.adapt_var[1], 0.0);
        assert!((a.adapt_var[2] - 1.0).abs() < 1e-3);
    }

    #[test]
    fn decay_follows_exponential() {
        let mut a = AdaptationState::new(1, 100.0, 1.0, 1.0).unwrap();
        a.adapt_var[0] = 4.0;
        a.update(&[0.0], 50.0);
        // decay = exp(-50/100) = exp(-0.5) ≈ 0.60653
        let expected = 4.0 * (-0.5_f32).exp();
        assert!((a.adapt_var[0] - expected).abs() < 1e-5);
    }

    #[test]
    fn compute_hyperpol_scales_by_gain() {
        let mut a = AdaptationState::new(2, 150.0, 0.5, 1.0).unwrap();
        a.adapt_var = vec![2.0, 4.0];
        let mut out = [0.0_f32; 2];
        a.compute_hyperpol(&mut out);
        assert_eq!(out, [1.0, 2.0]);
    }

    #[test]
    fn reset_zeros_adapt_var_keeps_params() {
        let mut a = AdaptationState::new(3, 150.0, 0.6, 1.0).unwrap();
        a.adapt_var = vec![1.0, 2.0, 3.0];
        a.reset();
        assert_eq!(a.adapt_var, vec![0.0, 0.0, 0.0]);
        assert_eq!(a.tau_ms, 150.0);
        assert_eq!(a.gain_mv, 0.6);
    }

    /// Integration: spike → bump → decay → smaller bump → settles.
    #[test]
    fn fires_then_decays_then_fires_again() {
        let mut a = AdaptationState::new(1, 50.0, 1.0, 1.0).unwrap();
        a.update(&[1.0], 1.0); // bump to ~1.0
        let v1 = a.adapt_var[0];
        // Wait 50ms with no spikes: decay to ~1/e ≈ 0.368.
        a.update(&[0.0], 50.0);
        let v2 = a.adapt_var[0];
        assert!(v2 < v1);
        // Spike again — accumulates.
        a.update(&[1.0], 1.0);
        let v3 = a.adapt_var[0];
        assert!(v3 > v2);
    }

    #[test]
    fn fired_shorter_than_population_is_not_fired() {
        let mut a = AdaptationState::new(4, 100.0, 1.0, 1.0).unwrap();
        // Only 2 entries supplied — neurons 2 and 3 should not get bumped.
        a.update(&[1.0, 1.0], 1.0);
        assert!(a.adapt_var[0] > 0.5);
        assert!(a.adapt_var[1] > 0.5);
        assert_eq!(a.adapt_var[2], 0.0);
        assert_eq!(a.adapt_var[3], 0.0);
    }
}
