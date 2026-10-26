//! Short-term synaptic depression (STD).
//!
//! Port of master commit `309ee15e8 feat(snn): runtime-tunable STD
//! dynamics + stronger defaults`.
//!
//! # What this models
//!
//! Each pre-synaptic neuron carries a per-neuron **depression** state
//! `d[i] ∈ [0, cap]`. On every spike, `d[i]` is bumped by `inc`.
//! Between spikes, `d[i]` decays back toward zero with time constant
//! `tau_ms`. When synaptic current is computed, every outgoing weight
//! is scaled by `(1 − d[i])`, so recently-firing neurons contribute
//! less. This is the synaptic-vesicle-depletion story, reduced to its
//! simplest mean-field form.
//!
//! STD is a **local** negative-feedback loop: one burst's first few
//! spikes get full weight, later spikes get depressed contributions.
//! Stabilizes runaway on short timescales (ms) while leaving the
//! slower homeostatic / adaptation loops room to engage.
//!
//! # Design notes
//!
//! - Depression lives per pre-synaptic neuron, not per synapse — keeps
//!   state linear in `n_neurons`, not `n_synapses`.
//! - Tunables are per-population (not global) so different pops can
//!   have different STD profiles (input pops often run with less STD).
//! - The `cap` prevents extreme depression from hard-zeroing the
//!   outgoing current; V1 defaults to 0.5 (i.e. no synapse ever loses
//!   more than half its weight from STD alone).

use serde::{Deserialize, Serialize};

/// Default STD increment per spike (master `309ee15e8`: V1 was 0.2,
/// stronger default now 0.3 after empirical tuning).
pub const DEFAULT_DEP_INC: f32 = 0.3;
/// Default STD recovery time constant (ms). Matches master's new
/// runtime-tunable default.
pub const DEFAULT_DEP_TAU_MS: f32 = 50.0;
/// Default STD cap (max fraction of the weight that can be depressed).
pub const DEFAULT_DEP_CAP: f32 = 0.5;

/// Setter lower bound for `inc`. Inclusive.
pub const DEP_INC_MIN: f32 = 0.0;
/// Setter upper bound for `inc`. Inclusive.
pub const DEP_INC_MAX: f32 = 1.0;
/// Setter lower bound for `tau_ms`. Inclusive.
pub const DEP_TAU_MS_MIN: f32 = 1.0;
/// Setter upper bound for `tau_ms`. Inclusive.
pub const DEP_TAU_MS_MAX: f32 = 10_000.0;
/// Setter lower bound for `cap`. Inclusive.
pub const DEP_CAP_MIN: f32 = 0.0;
/// Setter upper bound for `cap`. Inclusive.
pub const DEP_CAP_MAX: f32 = 1.0;

/// Per-population STD parameters. Runtime-tunable via live config
/// update; setters clamp into the documented ranges.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct DepressionConfig {
    /// Increment added to `d[i]` on each spike of neuron `i`.
    /// Range `[0, 1]`. Default `0.3`.
    pub inc: f32,
    /// Recovery time constant (ms) — `decay = exp(-dt / tau)`.
    /// Range `[1, 10_000]`. Default `50`.
    pub tau_ms: f32,
    /// Hard cap on `d[i]` — the outgoing weight scale is clamped to
    /// `>= 1 - cap`. Range `[0, 1]`. Default `0.5`.
    pub cap: f32,
}

impl Default for DepressionConfig {
    fn default() -> Self {
        Self {
            inc: DEFAULT_DEP_INC,
            tau_ms: DEFAULT_DEP_TAU_MS,
            cap: DEFAULT_DEP_CAP,
        }
    }
}

impl DepressionConfig {
    /// Clamp all fields into the V1-documented valid ranges. Silent —
    /// out-of-range values are config bugs, but clamping is nicer
    /// than refusing to construct.
    pub fn clamp(&mut self) {
        self.inc = self.inc.clamp(DEP_INC_MIN, DEP_INC_MAX);
        self.tau_ms = self.tau_ms.clamp(DEP_TAU_MS_MIN, DEP_TAU_MS_MAX);
        self.cap = self.cap.clamp(DEP_CAP_MIN, DEP_CAP_MAX);
    }

    /// Is STD effectively disabled? True when `inc == 0` or `cap == 0`
    /// — either blocks depression updates from having any downstream
    /// effect, so the caller can skip the per-step work.
    #[must_use]
    pub fn is_disabled(&self) -> bool {
        self.inc <= 0.0 || self.cap <= 0.0
    }

    /// Multiplicative decay factor for the given timestep.
    /// `d_new = d_old * decay(dt)` between spikes.
    #[must_use]
    pub fn decay(&self, dt_ms: f32) -> f32 {
        if self.tau_ms <= 0.0 || dt_ms <= 0.0 {
            return 1.0;
        }
        (-dt_ms / self.tau_ms).exp()
    }
}

/// Per-population depression state. `d[i]` is the current depression
/// for pre-synaptic neuron `i`, in `[0, cap]`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DepressionState {
    /// Per-neuron depression. Initialized to zero (no depression on
    /// a cold brain).
    pub d: Vec<f32>,
}

impl DepressionState {
    /// New zeroed state for `n_neurons` pre-synaptic neurons.
    #[must_use]
    pub fn new(n_neurons: usize) -> Self {
        Self {
            d: vec![0.0; n_neurons],
        }
    }

    /// Reset every entry to zero without reallocating.
    pub fn reset(&mut self) {
        self.d.fill(0.0);
    }

    /// Number of neurons this state covers.
    #[must_use]
    pub fn n_neurons(&self) -> usize {
        self.d.len()
    }
}

/// One per-step depression update.
///
/// 1. Multiplicative decay: `d[i] *= decay(dt)`.
/// 2. Spike bump: `d[i] += inc` wherever `fired[i] > 0.5`.
/// 3. Clamp to `[0, cap]`.
///
/// `fired` shorter than `state.d` is tolerated — missing entries are
/// treated as "did not fire" (no bump).
pub fn step_depression(
    state: &mut DepressionState,
    cfg: &DepressionConfig,
    fired: &[f32],
    dt_ms: f32,
) {
    if cfg.is_disabled() || state.d.is_empty() {
        return;
    }
    let decay = cfg.decay(dt_ms);
    let inc = cfg.inc;
    let cap = cfg.cap;
    for (i, d) in state.d.iter_mut().enumerate() {
        let mut nv = *d * decay;
        if fired.get(i).copied().unwrap_or(0.0) > 0.5 {
            nv += inc;
        }
        if nv > cap {
            nv = cap;
        } else if nv < 0.0 {
            nv = 0.0;
        }
        *d = nv;
    }
}

/// Effective pre-synaptic weight multiplier for neuron `i`:
/// `1 − d[i]`, clamped to `[1 − cap, 1]`. Multiply outgoing weights
/// by this when computing synaptic current.
#[must_use]
pub fn weight_multiplier(state: &DepressionState, cap: f32, i: usize) -> f32 {
    let d = state.d.get(i).copied().unwrap_or(0.0);
    (1.0 - d).clamp(1.0 - cap, 1.0)
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn default_matches_master_309ee15e8() {
        let cfg = DepressionConfig::default();
        assert_eq!(cfg.inc, 0.3);
        assert_eq!(cfg.tau_ms, 50.0);
        assert_eq!(cfg.cap, 0.5);
    }

    #[test]
    fn clamp_enforces_ranges() {
        let mut cfg = DepressionConfig {
            inc: 2.0,
            tau_ms: 0.5,
            cap: 1.5,
        };
        cfg.clamp();
        assert_eq!(cfg.inc, DEP_INC_MAX);
        assert_eq!(cfg.tau_ms, DEP_TAU_MS_MIN);
        assert_eq!(cfg.cap, DEP_CAP_MAX);

        let mut cfg = DepressionConfig {
            inc: -1.0,
            tau_ms: 50_000.0,
            cap: -0.5,
        };
        cfg.clamp();
        assert_eq!(cfg.inc, DEP_INC_MIN);
        assert_eq!(cfg.tau_ms, DEP_TAU_MS_MAX);
        assert_eq!(cfg.cap, DEP_CAP_MIN);
    }

    #[test]
    fn is_disabled_for_zero_inc_or_cap() {
        assert!(
            DepressionConfig {
                inc: 0.0,
                ..DepressionConfig::default()
            }
            .is_disabled()
        );
        assert!(
            DepressionConfig {
                cap: 0.0,
                ..DepressionConfig::default()
            }
            .is_disabled()
        );
    }

    #[test]
    fn decay_matches_formula() {
        let cfg = DepressionConfig::default();
        let d = cfg.decay(50.0);
        let expected = (-1.0_f32).exp();
        assert!((d - expected).abs() < 1e-6);
    }

    #[test]
    fn decay_one_for_nonpositive_args() {
        let cfg = DepressionConfig::default();
        assert_eq!(cfg.decay(0.0), 1.0);
        assert_eq!(cfg.decay(-1.0), 1.0);
        let bad = DepressionConfig {
            tau_ms: 0.0,
            ..cfg
        };
        assert_eq!(bad.decay(1.0), 1.0);
    }

    #[test]
    fn step_spike_bumps_by_inc() {
        let mut s = DepressionState::new(3);
        let cfg = DepressionConfig {
            inc: 0.2,
            tau_ms: 10_000.0, // ~no decay over 1ms
            cap: 1.0,
        };
        step_depression(&mut s, &cfg, &[1.0, 0.0, 1.0], 1.0);
        assert!((s.d[0] - 0.2).abs() < 1e-3);
        assert_eq!(s.d[1], 0.0);
        assert!((s.d[2] - 0.2).abs() < 1e-3);
    }

    #[test]
    fn step_caps_at_cap() {
        let mut s = DepressionState::new(1);
        s.d[0] = 0.49;
        let cfg = DepressionConfig {
            inc: 0.3,
            tau_ms: 10_000.0,
            cap: 0.5,
        };
        step_depression(&mut s, &cfg, &[1.0], 1.0);
        assert!((s.d[0] - 0.5).abs() < 1e-3);
    }

    #[test]
    fn step_decays_toward_zero_no_spikes() {
        let mut s = DepressionState::new(1);
        s.d[0] = 0.5;
        let cfg = DepressionConfig::default();
        step_depression(&mut s, &cfg, &[0.0], 50.0); // one tau
        let expected = 0.5 * (-1.0_f32).exp();
        assert!((s.d[0] - expected).abs() < 1e-5);
    }

    #[test]
    fn step_no_op_when_disabled() {
        let mut s = DepressionState::new(4);
        s.d = vec![0.25; 4];
        let cfg = DepressionConfig {
            inc: 0.0,
            ..DepressionConfig::default()
        };
        step_depression(&mut s, &cfg, &[1.0; 4], 1.0);
        assert_eq!(s.d, vec![0.25; 4]);
    }

    #[test]
    fn reset_zeros_state_keeps_capacity() {
        let mut s = DepressionState::new(3);
        s.d = vec![0.3, 0.4, 0.5];
        s.reset();
        assert_eq!(s.d, vec![0.0; 3]);
        assert_eq!(s.n_neurons(), 3);
    }

    #[test]
    fn weight_multiplier_clamps_at_one_minus_cap() {
        let mut s = DepressionState::new(2);
        s.d[0] = 0.0;
        s.d[1] = 1.0; // above cap of 0.5 (shouldn't happen via step_ but test clamp)
        assert_eq!(weight_multiplier(&s, 0.5, 0), 1.0);
        assert_eq!(weight_multiplier(&s, 0.5, 1), 0.5);
    }

    #[test]
    fn weight_multiplier_out_of_bounds_index_is_one() {
        let s = DepressionState::new(1);
        assert_eq!(weight_multiplier(&s, 0.5, 99), 1.0);
    }

    #[test]
    fn fired_shorter_than_state_is_not_fired() {
        let mut s = DepressionState::new(4);
        let cfg = DepressionConfig {
            inc: 0.2,
            tau_ms: 10_000.0,
            cap: 1.0,
        };
        // Only 2 entries — neurons 2,3 should not be bumped.
        step_depression(&mut s, &cfg, &[1.0, 1.0], 1.0);
        assert!(s.d[0] > 0.15);
        assert!(s.d[1] > 0.15);
        assert_eq!(s.d[2], 0.0);
        assert_eq!(s.d[3], 0.0);
    }
}
