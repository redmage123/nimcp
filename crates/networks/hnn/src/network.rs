//! Top-level [`HnnNetwork`] — owns the MLP, integrator timestep, and
//! the live `(q, p)` state.
//!
//! Forward dynamics: each call to [`HnnNetwork::step`] advances the
//! state by one symplectic Euler step using the MLP-defined
//! Hamiltonian. The MLP gradients are computed by reverse-mode autodiff
//! inside [`HamiltonianMlp::evaluate`].

use ndarray::Array1;
use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::integrator::symplectic_euler_step;
use crate::mlp::HamiltonianMlp;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HnnConfig {
    /// Number of canonical coordinates (q) and corresponding momenta (p).
    /// Total state vector length is `2 * dof`.
    pub dof: usize,
    /// Hidden widths of the Hamiltonian MLP. Empty → linear in `[q;p]`.
    pub hidden_layers: Vec<usize>,
    /// Integration timestep in dimensionless units.
    pub dt: f32,
    pub rng_seed: u64,
    /// Phase 11-substrate — biological substrate. Default disabled.
    #[serde(default)]
    pub substrate: HnnSubstrateCfg,
    /// Phase 11-substrate — thalamic routing. Default `None`.
    #[serde(default)]
    pub thalamic: Option<HnnThalamicCfg>,
}

impl Default for HnnConfig {
    fn default() -> Self {
        Self {
            dof: 1,
            hidden_layers: vec![32, 32],
            dt: 0.01,
            rng_seed: 0xA1A1,
            substrate: HnnSubstrateCfg::default(),
            thalamic: None,
        }
    }
}

/// Phase 11-substrate — per-network substrate config for the HNN.
///
/// The HNN is autonomous; substrate modulation affects only the
/// integration timestep (via membrane capacitance). When
/// `enabled = false` (default) substrate is fully skipped —
/// [`HnnNetwork::step_modulated`] delegates bit-for-bit to
/// [`HnnNetwork::step`] and the energy-conservation guarantee is
/// untouched.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct HnnSubstrateCfg {
    /// Master switch.
    pub enabled: bool,
    /// Recompute cached effects every N modulated steps.
    pub update_period: u32,
    /// Apply `axon.membrane_capacitance_mod` to the integration `dt`.
    pub capacitance_on: bool,
    /// Initial chemistry (default = full health).
    pub initial_state: nimcp_substrate::NeuralSubstrate,
    /// Per-step debit costs + passive recovery.
    pub dynamics: nimcp_substrate::NeuralSubstrateConfig,
}

impl Default for HnnSubstrateCfg {
    fn default() -> Self {
        Self {
            enabled: false,
            update_period: 10,
            capacitance_on: true,
            initial_state: nimcp_substrate::NeuralSubstrate::default(),
            dynamics: nimcp_substrate::NeuralSubstrateConfig::default(),
        }
    }
}

/// Phase 11-substrate — thalamic routing for the HNN.
///
/// The HNN has no afferent input, so the channel is purely output
/// routing: each modulated step records a submit when the post-step
/// momentum norm exceeds `submit_threshold`, letting downstream networks
/// be Hebbian-routed from the oscillator.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HnnThalamicCfg {
    /// Stable source identifier.
    pub source_id: u32,
    /// External destinations (≤ 16).
    pub destinations: Vec<u32>,
    /// Momentum-norm threshold above which a submit is recorded.
    pub submit_threshold: f32,
    /// Initial relay mode.
    pub mode: nimcp_thalamic::RelayMode,
}

impl Default for HnnThalamicCfg {
    fn default() -> Self {
        Self {
            source_id: 0,
            destinations: Vec::new(),
            submit_threshold: 0.5,
            mode: nimcp_thalamic::RelayMode::default(),
        }
    }
}

#[derive(Debug, Error)]
pub enum HnnError {
    #[error("hnn: dof must be > 0")]
    ZeroDof,
    #[error("hnn: dt must be > 0")]
    NonPositiveDt,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HnnNetwork {
    pub config: HnnConfig,
    pub mlp: HamiltonianMlp,
    /// Current generalised coordinates.
    pub q: Array1<f32>,
    /// Current canonical momenta.
    pub p: Array1<f32>,
    /// Runtime chemistry state (present iff `config.substrate.enabled`).
    #[serde(default)]
    pub substrate_state: Option<nimcp_substrate::NeuralSubstrate>,
    /// Cached `(axon, dendrite)` effects — recomputed every
    /// `config.substrate.update_period` modulated steps. Skipped in serde.
    #[serde(skip)]
    pub substrate_effects: Option<crate::substrate_adapter::Effects>,
    /// Cadence tick counter.
    #[serde(default)]
    pub substrate_tick_counter: u32,
    /// Thalamic channel (network-level output routing). `None` when
    /// `config.thalamic` is `None`.
    #[serde(default)]
    pub thalamic_channel: Option<nimcp_thalamic::ThalamicChannel>,
    /// Submit threshold cached from thalamic cfg.
    #[serde(default)]
    pub thalamic_submit_threshold: f32,
}

impl HnnNetwork {
    pub fn new(config: HnnConfig) -> Result<Self, HnnError> {
        if config.dof == 0 {
            return Err(HnnError::ZeroDof);
        }
        if !(config.dt > 0.0) {
            return Err(HnnError::NonPositiveDt);
        }
        let mlp = HamiltonianMlp::new(config.dof, &config.hidden_layers, config.rng_seed);
        let q = Array1::<f32>::zeros(config.dof);
        let p = Array1::<f32>::zeros(config.dof);
        let substrate_state = if config.substrate.enabled {
            Some(config.substrate.initial_state)
        } else {
            None
        };
        let (thalamic_channel, thalamic_submit_threshold) = match config.thalamic.as_ref() {
            Some(cfg) => {
                let ch = nimcp_thalamic::ThalamicChannel::new(cfg.source_id, &cfg.destinations)
                    .map(|mut ch| {
                        ch.mode = cfg.mode;
                        ch
                    });
                (ch, cfg.submit_threshold.max(0.0))
            }
            None => (None, 0.0),
        };
        Ok(Self {
            config,
            mlp,
            q,
            p,
            substrate_state,
            substrate_effects: None,
            substrate_tick_counter: 0,
            thalamic_channel,
            thalamic_submit_threshold,
        })
    }

    /// Reset state to zeros. Caller can mutate `q` / `p` afterward via
    /// [`HnnNetwork::set_state`].
    pub fn reset(&mut self) {
        self.q.fill(0.0);
        self.p.fill(0.0);
    }

    pub fn set_state(&mut self, q: Array1<f32>, p: Array1<f32>) {
        assert_eq!(q.len(), self.config.dof, "set_state: q dof mismatch");
        assert_eq!(p.len(), self.config.dof, "set_state: p dof mismatch");
        self.q = q;
        self.p = p;
    }

    /// Current Hamiltonian value (single forward through the MLP, no
    /// state mutation).
    pub fn energy(&self) -> f32 {
        let (h, _, _) = self.mlp.evaluate(&self.q, &self.p);
        h
    }

    /// Advance one symplectic Euler step using the MLP-defined `H`.
    /// Returns the energy at the *start* of the step (so the caller can
    /// log a series without an extra forward pass).
    pub fn step(&mut self) -> f32 {
        let mlp = &self.mlp;
        symplectic_euler_step(&mut self.q, &mut self.p, self.config.dt, |q, p| {
            mlp.evaluate(q, p)
        })
    }

    // -------------------------------------------------------------------
    // Phase 11-substrate — substrate + thalamic modulation.
    // -------------------------------------------------------------------

    /// Refresh the cached `(axon, dend)` substrate effects from the
    /// current [`Self::substrate_state`]. No-op when substrate is
    /// disabled or the state is `None`.
    pub fn recompute_substrate_effects(&mut self) {
        if !self.config.substrate.enabled {
            return;
        }
        if let Some(ref s) = self.substrate_state {
            self.substrate_effects = Some(nimcp_substrate::compute_effects(s));
        }
    }

    /// Substrate + thalamic-aware symplectic step. When
    /// `config.substrate.enabled` the integration `dt` is scaled by
    /// `axon.membrane_capacitance_mod`; when a thalamic channel is open a
    /// submit is recorded if the post-step momentum norm exceeds the
    /// threshold. Substrate is debited at the end of the step.
    ///
    /// Falls back to [`Self::step`] bit-for-bit when neither substrate
    /// nor thalamic is configured. At **full health** the effective `dt`
    /// equals `config.dt`, so the energy-conservation guarantee is
    /// preserved.
    ///
    /// Returns the energy at the start of the step (same as [`Self::step`]).
    pub fn step_modulated(&mut self) -> f32 {
        let has_substrate = self.config.substrate.enabled;
        let has_thalamic = self.thalamic_channel.is_some();
        if !has_substrate && !has_thalamic {
            return self.step();
        }

        if has_substrate {
            let should_recompute = self.substrate_effects.is_none()
                || self.substrate_tick_counter >= self.config.substrate.update_period;
            if should_recompute {
                self.recompute_substrate_effects();
                self.substrate_tick_counter = 0;
            } else {
                self.substrate_tick_counter = self.substrate_tick_counter.saturating_add(1);
            }
        }

        // Effective dt from substrate capacitance (identity at full health).
        let dt_eff = if has_substrate && self.config.substrate.capacitance_on {
            crate::substrate_adapter::effective_dt(self.config.dt, self.substrate_effects.as_ref())
        } else {
            self.config.dt
        };

        let mlp = &self.mlp;
        let energy = symplectic_euler_step(&mut self.q, &mut self.p, dt_eff, |q, p| {
            mlp.evaluate(q, p)
        });

        // Thalamic auto-submit on post-step momentum magnitude.
        if let Some(ch) = self.thalamic_channel.as_mut() {
            if crate::substrate_adapter::momentum_crosses(&self.p, self.thalamic_submit_threshold) {
                ch.record_submit();
            }
        }

        // Substrate debit — one "activity-equivalent" per dof whose
        // momentum crosses a soft threshold (|p_i| > 0.5).
        if let Some(ref mut s) = self.substrate_state {
            let n_active = self.p.iter().filter(|&&v| v.abs() > 0.5).count() as u64;
            nimcp_substrate::debit_activity(s, &self.config.substrate.dynamics, n_active, 0);
        }

        energy
    }

    /// Effective output gain from the HNN to a given destination using
    /// the supplied router's Hebbian weights. Returns `1.0` when the HNN
    /// has no thalamic channel.
    #[must_use]
    pub fn thalamic_output_gain(
        &self,
        router: &nimcp_thalamic::ThalamicRouter,
        dest_id: u32,
    ) -> f32 {
        crate::substrate_adapter::output_gain(self.thalamic_channel.as_ref(), router, dest_id)
    }

    /// Read-only access to the substrate state (for diagnostics / tests).
    /// Returns `None` on HNNs built without substrate.
    #[must_use]
    pub fn substrate_state(&self) -> Option<&nimcp_substrate::NeuralSubstrate> {
        self.substrate_state.as_ref()
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn network_steps_without_panic_and_state_changes() {
        let cfg = HnnConfig {
            dof: 2,
            hidden_layers: vec![16, 16],
            dt: 0.005,
            rng_seed: 0xC0FE,
            substrate: Default::default(),
            thalamic: None,
        };
        let mut net = HnnNetwork::new(cfg).unwrap();
        net.set_state(
            Array1::from_vec(vec![0.5, -0.3]),
            Array1::from_vec(vec![0.1, 0.2]),
        );
        let q0 = net.q.clone();
        let p0 = net.p.clone();
        for _ in 0..10 {
            net.step();
        }
        assert_ne!(net.q, q0, "q should evolve under stepping");
        assert_ne!(net.p, p0, "p should evolve under stepping");
        for v in net.q.iter().chain(net.p.iter()) {
            assert!(v.is_finite(), "step produced non-finite state: {v}");
        }
    }

    #[test]
    fn network_serde_round_trip() {
        let cfg = HnnConfig {
            dof: 1,
            hidden_layers: vec![8],
            dt: 0.01,
            rng_seed: 0xABCD,
            substrate: Default::default(),
            thalamic: None,
        };
        let net = HnnNetwork::new(cfg).unwrap();
        let json = serde_json::to_string(&net).unwrap();
        let restored: HnnNetwork = serde_json::from_str(&json).unwrap();
        assert_eq!(restored.config.dof, net.config.dof);
        assert_eq!(restored.q, net.q);
        // MLP weights match.
        for (wa, wb) in net.mlp.weights.iter().zip(restored.mlp.weights.iter()) {
            assert_eq!(wa, wb);
        }
    }

    // --- Phase 11-substrate ---

    fn small_cfg(seed: u64) -> HnnConfig {
        HnnConfig {
            dof: 2,
            hidden_layers: vec![16, 16],
            dt: 0.01,
            rng_seed: seed,
            substrate: Default::default(),
            thalamic: None,
        }
    }

    #[test]
    fn modulated_equals_step_when_disabled() {
        let mut a = HnnNetwork::new(small_cfg(0x1212)).unwrap();
        let mut b = HnnNetwork::new(small_cfg(0x1212)).unwrap();
        let q0 = Array1::from_vec(vec![0.4, -0.2]);
        let p0 = Array1::from_vec(vec![0.1, 0.3]);
        a.set_state(q0.clone(), p0.clone());
        b.set_state(q0, p0);
        for _ in 0..50 {
            let ea = a.step();
            let eb = b.step_modulated();
            assert_eq!(ea, eb, "disabled path energy must match step()");
        }
        assert_eq!(a.q, b.q, "disabled path q must match");
        assert_eq!(a.p, b.p, "disabled path p must match");
    }

    #[test]
    fn full_health_substrate_preserves_energy_conservation() {
        // H = 0.5(q² + p²) is the MLP-free analytic check; here we use the
        // learned MLP but assert the *modulated* trajectory matches the
        // plain one bit-for-bit at full health (capacitance_mod = 1.0).
        let mut cfg = small_cfg(0x3434);
        cfg.substrate.enabled = true; // full-health initial_state by default
        let mut modn = HnnNetwork::new(cfg).unwrap();
        let mut plain = HnnNetwork::new(small_cfg(0x3434)).unwrap();
        let q0 = Array1::from_vec(vec![0.6, -0.1]);
        let p0 = Array1::from_vec(vec![0.2, 0.05]);
        modn.set_state(q0.clone(), p0.clone());
        plain.set_state(q0, p0);
        for _ in 0..200 {
            let em = modn.step_modulated();
            let ep = plain.step();
            assert!((em - ep).abs() < 1e-6, "full health must not perturb dt: {em} vs {ep}");
        }
        for (a, b) in modn.q.iter().zip(plain.q.iter()) {
            assert!((a - b).abs() < 1e-6, "q diverged at full health");
        }
    }

    #[test]
    fn substrate_debits_under_activity() {
        let mut cfg = small_cfg(0x5656);
        cfg.substrate.enabled = true;
        let mut net = HnnNetwork::new(cfg).unwrap();
        net.set_state(
            Array1::from_vec(vec![1.5, -1.2]),
            Array1::from_vec(vec![2.0, -1.8]), // large p → crosses |p|>0.5
        );
        let atp_before = net.substrate_state().unwrap().atp_level;
        for _ in 0..30 {
            let _ = net.step_modulated();
        }
        let atp_after = net.substrate_state().unwrap().atp_level;
        assert!(atp_after <= atp_before, "ATP should not rise under activity");
    }

    #[test]
    fn thalamic_records_submits_on_large_momentum() {
        let mut cfg = small_cfg(0x7878);
        cfg.thalamic = Some(HnnThalamicCfg {
            source_id: 9,
            destinations: vec![10],
            submit_threshold: 0.1,
            mode: nimcp_thalamic::RelayMode::Tonic,
        });
        let mut net = HnnNetwork::new(cfg).unwrap();
        net.set_state(
            Array1::from_vec(vec![0.5, 0.5]),
            Array1::from_vec(vec![1.0, 1.0]), // norm ~1.41 > 0.1
        );
        let _ = net.step_modulated();
        let ch = net.thalamic_channel.as_ref().unwrap();
        assert!(ch.submits_this_step >= 1, "large momentum should record a submit");
    }
}
