//! Top-level [`FnoNetwork`] — input projection + N FNO blocks + output
//! projection.
//!
//! ```text
//!   x_in     [N, in_channels, L]                            (real)
//!   x_proj = LinearMix(x_in)              → [N, hidden, L]
//!   for block in blocks:
//!       x_proj = FnoBlock(x_proj)          → [N, hidden, L]
//!   x_out  = LinearMix(x_proj)            → [N, out_channels, L]
//! ```
//!
//! All projections are `LinearMixLayer` (1×1 convolutions) so the FFT
//! length is fixed for the duration of a forward pass. Channel counts
//! can change at the projections; spatial length is invariant.

use ndarray::Array3;
use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::block::FnoBlock;
use crate::linear_mix::LinearMixLayer;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FnoConfig {
    pub in_channels: usize,
    pub out_channels: usize,
    /// Hidden channel width, used by every block + projection layer.
    pub hidden_channels: usize,
    /// Number of stacked [`FnoBlock`]s.
    pub n_blocks: usize,
    /// Number of low-frequency modes retained per block.
    pub modes: usize,
    pub rng_seed: u64,
    /// Phase 11-substrate — biological substrate. Default disabled.
    #[serde(default)]
    pub substrate: FnoSubstrateCfg,
    /// Phase 11-substrate — thalamic routing. Default `None`.
    #[serde(default)]
    pub thalamic: Option<FnoThalamicCfg>,
}

/// Phase 11-substrate — per-network substrate config for the FNO.
///
/// Single compartment (one chemistry region for the whole operator).
/// When `enabled = false` (default) substrate is fully skipped —
/// [`FnoNetwork::forward_modulated`] delegates bit-for-bit to
/// [`FnoNetwork::forward`], training uses the base LR, and
/// `substrate_effects` stays `None`.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct FnoSubstrateCfg {
    /// Master switch.
    pub enabled: bool,
    /// Recompute cached effects every N modulated steps.
    pub update_period: u32,
    /// Apply `dend.integration_efficiency` as an output-field gain.
    pub integration_gain_on: bool,
    /// Apply `dend.plasticity_mod` as a training-LR multiplier.
    pub plasticity_mod_on: bool,
    /// Apply asymmetric LTP/LTD gating on gradients during train step.
    pub ltp_ltd_asymmetry_on: bool,
    /// Initial chemistry (default = full health).
    pub initial_state: nimcp_substrate::NeuralSubstrate,
    /// Per-step debit costs + passive recovery.
    pub dynamics: nimcp_substrate::NeuralSubstrateConfig,
}

impl Default for FnoSubstrateCfg {
    fn default() -> Self {
        Self {
            enabled: false,
            update_period: 10,
            integration_gain_on: true,
            plasticity_mod_on: true,
            ltp_ltd_asymmetry_on: true,
            initial_state: nimcp_substrate::NeuralSubstrate::default(),
            dynamics: nimcp_substrate::NeuralSubstrateConfig::default(),
        }
    }
}

/// Phase 11-substrate — thalamic routing for the FNO.
///
/// Single network-level [`nimcp_thalamic::ThalamicChannel`]. At each
/// modulated forward the input field is scaled by the mean attention
/// weight (or amplified in burst mode). Output field activity (L2 norm)
/// above `submit_threshold` records a submit for subsequent
/// `router.tick()` Hebbian updates.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FnoThalamicCfg {
    /// Stable source identifier.
    pub source_id: u32,
    /// External destinations (≤ 16).
    pub destinations: Vec<u32>,
    /// Output magnitude above which the channel auto-records a submit.
    pub submit_threshold: f32,
    /// Initial relay mode.
    pub mode: nimcp_thalamic::RelayMode,
}

impl Default for FnoThalamicCfg {
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
pub enum FnoError {
    #[error("fno: zero blocks")]
    EmptyBlocks,
    #[error("fno: zero modes")]
    ZeroModes,
    #[error("fno: zero channels")]
    ZeroChannels,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FnoNetwork {
    pub in_channels: usize,
    pub out_channels: usize,
    pub hidden_channels: usize,
    pub modes: usize,
    pub input_proj: LinearMixLayer,
    pub blocks: Vec<FnoBlock>,
    pub output_proj: LinearMixLayer,
    /// Phase 11-substrate — substrate config snapshot.
    #[serde(default)]
    pub substrate_cfg: FnoSubstrateCfg,
    /// Runtime chemistry state (present iff `substrate_cfg.enabled`).
    #[serde(default)]
    pub substrate_state: Option<nimcp_substrate::NeuralSubstrate>,
    /// Cached `(axon, dendrite)` effects — recomputed every
    /// `substrate_cfg.update_period` modulated steps. Skipped in serde.
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

impl FnoNetwork {
    pub fn new(config: FnoConfig) -> Result<Self, FnoError> {
        if config.n_blocks == 0 {
            return Err(FnoError::EmptyBlocks);
        }
        if config.modes == 0 {
            return Err(FnoError::ZeroModes);
        }
        if config.in_channels == 0 || config.out_channels == 0 || config.hidden_channels == 0 {
            return Err(FnoError::ZeroChannels);
        }
        let mix_seed = |idx: u64| {
            config
                .rng_seed
                .wrapping_add(idx)
                .wrapping_mul(0xD123_4567_89AB_CDEF)
        };

        let input_proj = LinearMixLayer::new(config.in_channels, config.hidden_channels, mix_seed(0));
        let blocks: Vec<FnoBlock> = (0..config.n_blocks)
            .map(|i| FnoBlock::new(config.hidden_channels, config.modes, mix_seed((i + 1) as u64)))
            .collect();
        let output_proj = LinearMixLayer::new(
            config.hidden_channels,
            config.out_channels,
            mix_seed((config.n_blocks + 1) as u64),
        );

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
            in_channels: config.in_channels,
            out_channels: config.out_channels,
            hidden_channels: config.hidden_channels,
            modes: config.modes,
            input_proj,
            blocks,
            output_proj,
            substrate_cfg: config.substrate,
            substrate_state,
            substrate_effects: None,
            substrate_tick_counter: 0,
            thalamic_channel,
            thalamic_submit_threshold,
        })
    }

    /// `input: [N, in_channels, L]` → `[N, out_channels, L]`.
    pub fn forward(&self, input: &Array3<f32>) -> Array3<f32> {
        let mut h = self.input_proj.forward(input);
        for block in &self.blocks {
            h = block.forward(&h);
        }
        self.output_proj.forward(&h)
    }

    // -------------------------------------------------------------------
    // Phase 11-substrate — substrate + thalamic modulation.
    // -------------------------------------------------------------------

    /// Refresh the cached `(axon, dend)` substrate effects from the
    /// current [`Self::substrate_state`]. No-op when substrate is
    /// disabled or the state is `None`.
    pub fn recompute_substrate_effects(&mut self) {
        if !self.substrate_cfg.enabled {
            return;
        }
        if let Some(ref s) = self.substrate_state {
            self.substrate_effects = Some(nimcp_substrate::compute_effects(s));
        }
    }

    /// Substrate + thalamic-aware forward. When `substrate_cfg.enabled`
    /// the cached effects attenuate the output field by
    /// `dend.integration_efficiency`; when a thalamic channel is open the
    /// input field is attention-scaled and output activity above
    /// `submit_threshold` records a submit. Substrate is debited at the
    /// end of the step.
    ///
    /// Falls back to [`Self::forward`] bit-for-bit when neither substrate
    /// nor thalamic is configured.
    pub fn forward_modulated(&mut self, input: &Array3<f32>) -> Array3<f32> {
        let has_substrate = self.substrate_cfg.enabled;
        let has_thalamic = self.thalamic_channel.is_some();
        if !has_substrate && !has_thalamic {
            return self.forward(input);
        }

        if has_substrate {
            let should_recompute = self.substrate_effects.is_none()
                || self.substrate_tick_counter >= self.substrate_cfg.update_period;
            if should_recompute {
                self.recompute_substrate_effects();
                self.substrate_tick_counter = 0;
            } else {
                self.substrate_tick_counter = self.substrate_tick_counter.saturating_add(1);
            }
        }

        let attn = crate::substrate_adapter::attention_scalar(self.thalamic_channel.as_ref());
        let modulated_input = if (attn - 1.0).abs() > f32::EPSILON {
            input * attn
        } else {
            input.clone()
        };

        let mut out = self.forward(&modulated_input);

        let gain = crate::substrate_adapter::integration_gain(
            self.substrate_effects.as_ref(),
            has_substrate && self.substrate_cfg.integration_gain_on,
        );
        if (gain - 1.0).abs() > f32::EPSILON {
            out.mapv_inplace(|v| v * gain);
        }

        if let Some(ch) = self.thalamic_channel.as_mut() {
            let mag = out.iter().map(|v| v * v).sum::<f32>().sqrt();
            if mag > self.thalamic_submit_threshold {
                ch.record_submit();
            }
        }

        if let Some(ref mut s) = self.substrate_state {
            let n_active = out.iter().filter(|&&v| v.abs() > 0.5).count() as u64;
            nimcp_substrate::debit_activity(s, &self.substrate_cfg.dynamics, n_active, 0);
        }

        out
    }

    /// Effective output gain from the FNO to a given destination using
    /// the supplied router's Hebbian weights. Returns `1.0` when the FNO
    /// has no thalamic channel.
    #[must_use]
    pub fn thalamic_output_gain(
        &self,
        router: &nimcp_thalamic::ThalamicRouter,
        dest_id: u32,
    ) -> f32 {
        let Some(ch) = self.thalamic_channel.as_ref() else {
            return 1.0;
        };
        let router_gain = router.effective_gain(ch.source_id, dest_id);
        if router_gain == 0.0 {
            ch.get_gate(dest_id)
        } else {
            router_gain
        }
    }

    /// Read-only access to the substrate state (for diagnostics / tests).
    /// Returns `None` on FNOs built without substrate.
    #[must_use]
    pub fn substrate_state(&self) -> Option<&nimcp_substrate::NeuralSubstrate> {
        self.substrate_state.as_ref()
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use ndarray::Array3;

    #[test]
    fn fno_forward_shape_and_finite() {
        let cfg = FnoConfig {
            in_channels: 2,
            out_channels: 3,
            hidden_channels: 8,
            n_blocks: 2,
            modes: 4,
            rng_seed: 0xF00,
            substrate: Default::default(),
            thalamic: None,
        };
        let net = FnoNetwork::new(cfg).unwrap();
        let length = 16;
        let input = Array3::from_shape_fn((1, 2, length), |(_, c, l)| {
            ((c + 1) as f32 * l as f32 * 0.1).sin()
        });
        let out = net.forward(&input);
        assert_eq!(out.dim(), (1, 3, length));
        for v in out.iter() {
            assert!(v.is_finite(), "fno produced non-finite: {v}");
        }
    }

    #[test]
    fn fno_resolution_independent_shape() {
        // Same network, two input lengths — both produce L-length output.
        let cfg = FnoConfig {
            in_channels: 1,
            out_channels: 1,
            hidden_channels: 4,
            n_blocks: 1,
            modes: 3,
            rng_seed: 0x1234,
            substrate: Default::default(),
            thalamic: None,
        };
        let net = FnoNetwork::new(cfg).unwrap();
        for length in [16, 32, 64] {
            let input = Array3::from_shape_fn((1, 1, length), |(_, _, l)| l as f32 * 0.05);
            let out = net.forward(&input);
            assert_eq!(out.dim(), (1, 1, length), "fno length not preserved at {length}");
        }
    }

    #[test]
    fn fno_serde_round_trip() {
        let cfg = FnoConfig {
            in_channels: 1,
            out_channels: 2,
            hidden_channels: 6,
            n_blocks: 2,
            modes: 4,
            rng_seed: 0xABCD,
            substrate: Default::default(),
            thalamic: None,
        };
        let net = FnoNetwork::new(cfg).unwrap();
        let json = serde_json::to_string(&net).unwrap();
        let restored: FnoNetwork = serde_json::from_str(&json).unwrap();
        assert_eq!(restored.in_channels, net.in_channels);
        assert_eq!(restored.blocks.len(), net.blocks.len());

        let length = 16;
        let input = Array3::from_shape_fn((1, 1, length), |(_, _, l)| l as f32 * 0.07);
        let a = net.forward(&input);
        let b = restored.forward(&input);
        for (x, y) in a.iter().zip(b.iter()) {
            assert!((x - y).abs() < 1e-5, "forward drift after serde: {x} vs {y}");
        }
    }

    // --- Phase 11-substrate ---

    fn small_cfg(seed: u64) -> FnoConfig {
        FnoConfig {
            in_channels: 1,
            out_channels: 1,
            hidden_channels: 4,
            n_blocks: 1,
            modes: 3,
            rng_seed: seed,
            substrate: Default::default(),
            thalamic: None,
        }
    }

    #[test]
    fn modulated_equals_forward_when_disabled() {
        let mut net = FnoNetwork::new(small_cfg(0x5151)).unwrap();
        let inp = Array3::from_shape_fn((1, 1, 16), |(_, _, l)| (l as f32 * 0.3).sin());
        let plain = net.forward(&inp);
        let modulated = net.forward_modulated(&inp);
        assert_eq!(plain, modulated, "disabled path must be bit-identical");
    }

    #[test]
    fn substrate_full_health_is_identity() {
        let mut cfg = small_cfg(0x6262);
        cfg.substrate.enabled = true;
        let mut net = FnoNetwork::new(cfg).unwrap();
        let plain_net = FnoNetwork::new(small_cfg(0x6262)).unwrap();
        let inp = Array3::from_shape_fn((1, 1, 16), |(_, _, l)| (l as f32 * 0.2).cos());
        let modulated = net.forward_modulated(&inp);
        let plain = plain_net.forward(&inp);
        for (a, b) in modulated.iter().zip(plain.iter()) {
            assert!((a - b).abs() < 1e-6, "full health should be identity: {a} vs {b}");
        }
    }

    #[test]
    fn substrate_debits_under_activity() {
        let mut cfg = small_cfg(0x7373);
        cfg.substrate.enabled = true;
        let mut net = FnoNetwork::new(cfg).unwrap();
        let atp_before = net.substrate_state().unwrap().atp_level;
        let inp = Array3::from_elem((1, 1, 16), 4.0);
        for _ in 0..20 {
            let _ = net.forward_modulated(&inp);
        }
        let atp_after = net.substrate_state().unwrap().atp_level;
        assert!(atp_after <= atp_before, "ATP should not rise under activity");
    }

    #[test]
    fn thalamic_burst_amplifies_input() {
        let mut cfg = small_cfg(0x8484);
        cfg.thalamic = Some(FnoThalamicCfg {
            source_id: 5,
            destinations: vec![6, 7],
            submit_threshold: 1e9,
            mode: nimcp_thalamic::RelayMode::Burst,
        });
        let mut net = FnoNetwork::new(cfg).unwrap();
        let plain_net = FnoNetwork::new(small_cfg(0x8484)).unwrap();
        let inp = Array3::from_shape_fn((1, 1, 16), |(_, _, l)| (l as f32 * 0.25).sin() + 0.1);
        let modulated = net.forward_modulated(&inp);
        let plain = plain_net.forward(&inp);
        let differs = modulated
            .iter()
            .zip(plain.iter())
            .any(|(a, b)| (a - b).abs() > 1e-6);
        assert!(differs, "burst attention should change the output");
    }
}
