//! Basket cell pool — fast-spiking inhibitory interneurons.
//!
//! One pool attaches per excitatory population. Cells receive aggregate
//! drive from the parent population's mean firing rate and project
//! strong uniform inhibition back to every neuron in the parent.
//! Approximates the PV+ basket cell inhibitory micro-circuit (~20% of
//! cortical neurons) at population scale.
//!
//! Port of master commit `6b5f82ac0 feat(snn): basket cell pool`.
//!
//! # Integration
//!
//! Each population step the network:
//!
//! 1. Calls [`BasketPool::emit_inhibition`] **before** the parent LIF
//!    step — adds `gain_inhib_to_parent × mean_basket_spike` to every
//!    parent `I_syn` entry. The gain is negative; the function does
//!    not flip sign, so callers see the inhibition as written.
//! 2. Parent LIF step runs.
//! 3. Calls [`BasketPool::step`] **after** the parent LIF — updates
//!    the drive EMA on `parent_mean_fire_rate`, integrates each basket
//!    cell's LIF, emits spikes.
//!
//! On the very first step the basket has not yet spiked, so step 1
//! injects zero. After that, basket inhibition lags the parent by
//! one step (V1 design — keeps the loop causal).

use serde::{Deserialize, Serialize};
use thiserror::Error;

const FRACTION_MIN: f32 = 0.01;
const FRACTION_MAX: f32 = 0.5;

// Fast-spiking interneuron defaults — shorter tau, lower threshold,
// shorter refractory than pyramidal cells.
const DEFAULT_TAU_MEM_MS: f32 = 5.0;
const DEFAULT_T_REF_MS: f32 = 1.0;
const DEFAULT_V_THRESH: f32 = -52.0;
const DEFAULT_V_RESET: f32 = -70.0;
const DEFAULT_V_REST: f32 = -65.0;
const DEFAULT_GAIN_DRIVE: f32 = 30.0;
const DEFAULT_GAIN_INHIB: f32 = -3.0;
const DEFAULT_TAU_DRIVE_MS: f32 = 10.0;

/// Per-pool basket-cell state. Owned by exactly one parent population.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BasketPool {
    /// Index of the parent excitatory population in the network.
    pub parent_pop_id: u32,
    /// Per-cell membrane voltage (mV).
    pub membrane_v: Vec<f32>,
    /// Per-cell refractory countdown (ms remaining).
    pub refractory: Vec<f32>,
    /// Per-cell binary spike output from the most recent step (0.0 / 1.0).
    pub spike_output: Vec<f32>,
    /// Membrane time constant (ms). Default `5.0` (fast-spiking).
    pub tau_mem_ms: f32,
    /// Absolute refractory period (ms). Default `1.0`.
    pub t_ref_ms: f32,
    /// Spike threshold (mV). Default `-52.0`.
    pub v_thresh: f32,
    /// Reset voltage after spike (mV). Default `-70.0`.
    pub v_reset: f32,
    /// Resting membrane voltage (mV). Default `-65.0`.
    pub v_rest: f32,
    /// mV depolarization injected per unit `parent_mean_fire_rate`. Default `30.0`.
    pub gain_drive_from_parent: f32,
    /// mV added to parent `I_syn` per unit basket mean spike. **Negative**
    /// (inhibitory). Default `-3.0`.
    pub gain_inhib_to_parent: f32,
    /// EMA smoothing window (ms) for parent drive. Default `10.0`.
    pub tau_drive_ms: f32,
    /// Internal EMA state of parent drive. Initial `0.0`.
    pub drive_filtered: f32,
}

/// Construction errors.
#[derive(Debug, Error, PartialEq, Eq)]
pub enum BasketError {
    /// `parent_n_neurons == 0` — no parent to inhibit.
    #[error("parent_n_neurons must be > 0")]
    EmptyParent,
}

impl BasketPool {
    /// Construct a new pool. `n_cells = max(1, round(parent_n × fraction))`,
    /// where `fraction` is clamped to `[0.01, 0.5]` (V1 default range).
    pub fn new(parent_pop_id: u32, parent_n_neurons: usize, fraction: f32) -> Result<Self, BasketError> {
        if parent_n_neurons == 0 {
            return Err(BasketError::EmptyParent);
        }
        let frac = fraction.clamp(FRACTION_MIN, FRACTION_MAX);
        #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss, clippy::cast_precision_loss)]
        let n_cells = ((parent_n_neurons as f32 * frac) as usize).max(1);

        Ok(Self {
            parent_pop_id,
            membrane_v: vec![DEFAULT_V_REST; n_cells],
            refractory: vec![0.0; n_cells],
            spike_output: vec![0.0; n_cells],
            tau_mem_ms: DEFAULT_TAU_MEM_MS,
            t_ref_ms: DEFAULT_T_REF_MS,
            v_thresh: DEFAULT_V_THRESH,
            v_reset: DEFAULT_V_RESET,
            v_rest: DEFAULT_V_REST,
            gain_drive_from_parent: DEFAULT_GAIN_DRIVE,
            gain_inhib_to_parent: DEFAULT_GAIN_INHIB,
            tau_drive_ms: DEFAULT_TAU_DRIVE_MS,
            drive_filtered: 0.0,
        })
    }

    /// Number of basket cells in this pool.
    #[must_use]
    pub fn n_cells(&self) -> usize {
        self.membrane_v.len()
    }

    /// Reset membrane to rest, refractory to zero, spikes to zero,
    /// drive EMA to zero.
    pub fn reset(&mut self) {
        self.membrane_v.fill(self.v_rest);
        self.refractory.fill(0.0);
        self.spike_output.fill(0.0);
        self.drive_filtered = 0.0;
    }

    /// Inject inhibition into the parent population's `I_syn` array.
    /// Each parent neuron sees `gain_inhib_to_parent × mean_basket_spike`.
    /// Uses the basket spike output from the **previous** step.
    pub fn emit_inhibition(&self, parent_i_syn: &mut [f32]) {
        if parent_i_syn.is_empty() || self.spike_output.is_empty() {
            return;
        }
        #[allow(clippy::cast_precision_loss)]
        let mean_spike: f32 =
            self.spike_output.iter().sum::<f32>() / self.spike_output.len() as f32;
        let delta = self.gain_inhib_to_parent * mean_spike;
        if delta == 0.0 {
            return;
        }
        for i in parent_i_syn.iter_mut() {
            *i += delta;
        }
    }

    /// Per-step basket update. Call AFTER the parent LIF step.
    /// `parent_mean_fire_rate` is the fraction of parent neurons that
    /// fired this step, in `[0, 1]`.
    pub fn step(&mut self, parent_mean_fire_rate: f32, dt_ms: f32) {
        if dt_ms <= 0.0 {
            return;
        }

        // EMA on parent drive — smooths bursty input.
        let alpha = if self.tau_drive_ms > 0.0 {
            (dt_ms / self.tau_drive_ms).min(1.0)
        } else {
            1.0
        };
        self.drive_filtered += alpha * (parent_mean_fire_rate - self.drive_filtered);

        let i_input = self.drive_filtered * self.gain_drive_from_parent;
        let tau_inv = if self.tau_mem_ms > 0.0 {
            dt_ms / self.tau_mem_ms
        } else {
            1.0
        };

        for ((v, r), s) in self
            .membrane_v
            .iter_mut()
            .zip(self.refractory.iter_mut())
            .zip(self.spike_output.iter_mut())
        {
            let dv = (self.v_rest - *v + i_input) * tau_inv;
            *v += dv;

            if *v >= self.v_thresh && *r <= 0.0 {
                *v = self.v_reset;
                *r = self.t_ref_ms;
                *s = 1.0;
            } else {
                *s = 0.0;
            }

            *r -= dt_ms;
            if *r < 0.0 {
                *r = 0.0;
            }
        }
    }

    /// Diagnostic: fraction of basket cells that fired on the most recent step.
    #[must_use]
    pub fn mean_rate(&self) -> f32 {
        if self.spike_output.is_empty() {
            return 0.0;
        }
        #[allow(clippy::cast_precision_loss)]
        let denom = self.spike_output.len() as f32;
        self.spike_output.iter().sum::<f32>() / denom
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn rejects_empty_parent() {
        assert!(matches!(
            BasketPool::new(0, 0, 0.2),
            Err(BasketError::EmptyParent)
        ));
    }

    #[test]
    fn fraction_below_min_clamps_up() {
        let bp = BasketPool::new(0, 1000, 0.001).unwrap();
        // Min fraction 0.01 × 1000 = 10 cells.
        assert_eq!(bp.n_cells(), 10);
    }

    #[test]
    fn fraction_above_max_clamps_down() {
        let bp = BasketPool::new(0, 100, 0.99).unwrap();
        // Max fraction 0.5 × 100 = 50 cells.
        assert_eq!(bp.n_cells(), 50);
    }

    #[test]
    fn at_least_one_cell_even_for_tiny_parent() {
        let bp = BasketPool::new(0, 1, 0.01).unwrap();
        assert_eq!(bp.n_cells(), 1);
    }

    #[test]
    fn membrane_starts_at_rest() {
        let bp = BasketPool::new(0, 100, 0.2).unwrap();
        for v in &bp.membrane_v {
            assert_eq!(*v, DEFAULT_V_REST);
        }
        for s in &bp.spike_output {
            assert_eq!(*s, 0.0);
        }
        assert_eq!(bp.drive_filtered, 0.0);
    }

    #[test]
    fn cold_start_emits_zero_inhibition() {
        let bp = BasketPool::new(0, 10, 0.2).unwrap();
        let mut parent_i_syn = vec![5.0; 10];
        bp.emit_inhibition(&mut parent_i_syn);
        // No spikes yet → no delta added.
        for v in &parent_i_syn {
            assert_eq!(*v, 5.0);
        }
    }

    #[test]
    fn ema_smooths_drive_input() {
        let mut bp = BasketPool::new(0, 100, 0.2).unwrap();
        bp.step(1.0, 1.0);
        let after_first = bp.drive_filtered;
        bp.step(1.0, 1.0);
        let after_second = bp.drive_filtered;
        // EMA monotonically approaches input but doesn't reach it on
        // step 1 with tau_drive=10 and dt=1.
        assert!(after_first > 0.0 && after_first < 1.0);
        assert!(after_second > after_first && after_second < 1.0);
    }

    #[test]
    fn high_parent_drive_eventually_spikes_basket() {
        let mut bp = BasketPool::new(0, 100, 0.2).unwrap();
        // Drive saturates EMA → ~30 mV depolarization input → bridges
        // the v_rest=-65 → v_thresh=-52 gap (~13 mV). Look at the
        // whole window, not just the final step — spiking is periodic
        // so the very last step may happen to be a post-reset step
        // where spike_output is zero.
        let mut saw_spike = false;
        for _ in 0..200 {
            bp.step(1.0, 1.0);
            if bp.spike_output.iter().any(|&s| s > 0.5) {
                saw_spike = true;
            }
        }
        assert!(saw_spike, "high drive should produce at least one spike");
        assert!(bp.drive_filtered > 0.95, "drive EMA should saturate");
    }

    #[test]
    fn refractory_blocks_immediate_re_spike() {
        let mut bp = BasketPool::new(0, 1, 0.5).unwrap(); // single cell
        // Force the cell into a fired state by manually setting v above thresh.
        bp.membrane_v[0] = -40.0;
        bp.step(0.0, 0.5); // should fire (no input, but already over threshold)
        assert_eq!(bp.spike_output[0], 1.0);
        let r1 = bp.refractory[0];
        assert!(r1 > 0.0, "refractory must be set after spike");
        // Try to fire again immediately — refractory should suppress.
        bp.membrane_v[0] = -40.0;
        bp.step(0.0, 0.1);
        assert_eq!(bp.spike_output[0], 0.0, "refractory must block re-spike");
    }

    #[test]
    fn negative_dt_is_no_op() {
        let mut bp = BasketPool::new(0, 4, 0.5).unwrap();
        let before = bp.drive_filtered;
        bp.step(0.5, -1.0);
        assert_eq!(bp.drive_filtered, before);
    }

    #[test]
    fn reset_returns_to_initial_state() {
        let mut bp = BasketPool::new(0, 50, 0.2).unwrap();
        for _ in 0..20 {
            bp.step(0.5, 1.0);
        }
        bp.reset();
        for v in &bp.membrane_v {
            assert_eq!(*v, DEFAULT_V_REST);
        }
        assert_eq!(bp.drive_filtered, 0.0);
        assert_eq!(bp.mean_rate(), 0.0);
    }

    #[test]
    fn inhibition_is_uniform_across_parent() {
        let mut bp = BasketPool::new(0, 50, 0.2).unwrap();
        // Force every basket cell to have spiked.
        bp.spike_output.fill(1.0);
        let mut parent = vec![0.0_f32; 50];
        bp.emit_inhibition(&mut parent);
        // Same delta everywhere.
        let first = parent[0];
        assert!(first < 0.0, "delta should be negative (inhibition)");
        for v in &parent {
            assert_eq!(*v, first);
        }
    }
}
