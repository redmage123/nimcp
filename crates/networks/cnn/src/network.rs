//! Top-level [`CnnNetwork`] — stacks a heterogeneous list of layers
//! and runs them in declared order.
//!
//! # Spec → network
//!
//! [`CnnConfig`] describes the network as a `Vec<CnnLayerSpec>`; the
//! constructor walks the spec, allocates each layer with a sub-seed
//! derived from the network seed, and threads the running shape through
//! to validate adjacency.
//!
//! # Forward contract
//!
//! - Input must be a 4-D `[batch, in_channels, height, width]` tensor
//!   matching the config's `input_shape`.
//! - Output is the dense vector `[batch, output_dim]` (the last linear
//!   layer's width).
//! - The forward pass holds a single live tensor at any moment; no
//!   activations are cached (training is a follow-up phase).
//!
//! # V1 lessons carried forward
//!
//! - Deterministic seed everywhere — same `rng_seed` produces bit-
//!   identical weights across runs and platforms.
//! - Shape errors caught at config time rather than at first forward —
//!   `CnnNetwork::new()` returns a [`CnnError::ShapeMismatch`] if any
//!   layer's expected input doesn't match the previous layer's output.

use ndarray::{Array2, Array4};
use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::activation::ReluLayer;
use crate::conv::Conv2dLayer;
use crate::flatten::FlattenLayer;
use crate::linear::LinearLayer;
use crate::pool::MaxPool2dLayer;

/// Declarative spec for a single CNN layer. The constructor maps each
/// variant to its concrete `*Layer` type.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CnnLayerSpec {
    /// `(out_channels, kh, kw, stride, padding)`.
    Conv2d {
        out_channels: usize,
        kh: usize,
        kw: usize,
        stride: usize,
        padding: usize,
    },
    /// `(kernel, stride)` — square pool with matching stride.
    MaxPool2d { kernel: usize, stride: usize },
    Relu,
    Flatten,
    /// Dense layer width. The constructor infers `in_features` from the
    /// running flattened shape; `out_features` is the spec value.
    Linear { out_features: usize },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CnnConfig {
    /// `(in_channels, height, width)` of the network input — the batch
    /// axis is flexible.
    pub input_shape: (usize, usize, usize),
    pub layers: Vec<CnnLayerSpec>,
    pub rng_seed: u64,
    /// Phase 11-substrate — biological substrate. Default disabled.
    #[serde(default)]
    pub substrate: CnnSubstrateCfg,
    /// Phase 11-substrate — thalamic routing. Default `None`.
    #[serde(default)]
    pub thalamic: Option<CnnThalamicCfg>,
}

/// Phase 11-substrate — per-network substrate config for the CNN.
///
/// The CNN is a single compartment (one chemistry region for the whole
/// network). When `enabled = false` (default), substrate is fully
/// skipped — [`CnnNetwork::forward_modulated`] delegates bit-for-bit to
/// [`CnnNetwork::forward`], training uses the base LR, and
/// `substrate_effects` stays `None`.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct CnnSubstrateCfg {
    /// Master switch.
    pub enabled: bool,
    /// Recompute cached effects every N modulated steps.
    pub update_period: u32,
    /// Apply `dend.integration_efficiency` as an output-logit gain.
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

impl Default for CnnSubstrateCfg {
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

/// Phase 11-substrate — thalamic routing for the CNN.
///
/// Single network-level [`nimcp_thalamic::ThalamicChannel`]. At each
/// modulated forward the input tensor is scaled by the mean attention
/// weight (or amplified in burst mode). Output activity (L2 norm of the
/// logits) above `submit_threshold` records a submit for subsequent
/// `router.tick()` Hebbian updates.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CnnThalamicCfg {
    /// Stable source identifier.
    pub source_id: u32,
    /// External destinations (≤ 16).
    pub destinations: Vec<u32>,
    /// Output magnitude above which the channel auto-records a submit.
    pub submit_threshold: f32,
    /// Initial relay mode.
    pub mode: nimcp_thalamic::RelayMode,
}

impl Default for CnnThalamicCfg {
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
pub enum CnnError {
    #[error("cnn: empty layer list")]
    Empty,
    #[error("cnn: shape mismatch in layer {layer_index} ({hint})")]
    ShapeMismatch { layer_index: usize, hint: String },
    #[error(
        "cnn: linear-after-non-flatten in layer {0} — call FlattenLayer before any LinearLayer"
    )]
    LinearWithoutFlatten(usize),
    #[error("cnn: pool/conv on a flattened tensor in layer {0}")]
    SpatialAfterFlatten(usize),
}

/// Internal layer enum — the constructed counterpart to [`CnnLayerSpec`].
/// `serde` is derived so the whole network round-trips through JSON.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum CnnLayer {
    Conv(Conv2dLayer),
    Pool(MaxPool2dLayer),
    Relu(ReluLayer),
    Flatten(FlattenLayer),
    Linear(LinearLayer),
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CnnNetwork {
    pub input_shape: (usize, usize, usize),
    pub output_dim: usize,
    pub layers: Vec<CnnLayer>,
    /// Phase 11-substrate — substrate config snapshot.
    #[serde(default)]
    pub substrate_cfg: CnnSubstrateCfg,
    /// Runtime chemistry state (present iff `substrate_cfg.enabled`).
    #[serde(default)]
    pub substrate_state: Option<nimcp_substrate::NeuralSubstrate>,
    /// Cached `(axon, dendrite)` effects — recomputed every
    /// `substrate_cfg.update_period` modulated steps. Skipped in serde
    /// (derived cache; rebuilt on first post-load modulated step).
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

impl CnnNetwork {
    pub fn new(config: CnnConfig) -> Result<Self, CnnError> {
        if config.layers.is_empty() {
            return Err(CnnError::Empty);
        }
        let (mut c, mut h, mut w) = config.input_shape;
        let mut flat_dim: Option<usize> = None;
        let mut output_dim = 0;
        let mut layers: Vec<CnnLayer> = Vec::with_capacity(config.layers.len());

        for (idx, spec) in config.layers.iter().enumerate() {
            // Sub-seed each layer so reordering / inserting layers
            // doesn't shift every downstream init.
            let sub_seed = config.rng_seed.wrapping_add(idx as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15);
            match spec {
                CnnLayerSpec::Conv2d {
                    out_channels,
                    kh,
                    kw,
                    stride,
                    padding,
                } => {
                    if flat_dim.is_some() {
                        return Err(CnnError::SpatialAfterFlatten(idx));
                    }
                    let layer = Conv2dLayer::new(
                        c,
                        *out_channels,
                        *kh,
                        *kw,
                        *stride,
                        *stride,
                        *padding,
                        *padding,
                        sub_seed,
                    );
                    let (h_out, w_out) = layer.output_hw(h, w);
                    if h_out == 0 || w_out == 0 {
                        return Err(CnnError::ShapeMismatch {
                            layer_index: idx,
                            hint: format!(
                                "conv produces zero spatial dim from {h}x{w} (kernel {kh}x{kw}, stride {stride}, pad {padding})"
                            ),
                        });
                    }
                    c = *out_channels;
                    h = h_out;
                    w = w_out;
                    layers.push(CnnLayer::Conv(layer));
                }
                CnnLayerSpec::MaxPool2d { kernel, stride } => {
                    if flat_dim.is_some() {
                        return Err(CnnError::SpatialAfterFlatten(idx));
                    }
                    let layer = MaxPool2dLayer::new(*kernel, *kernel, *stride, *stride);
                    let (h_out, w_out) = layer.output_hw(h, w);
                    if h_out == 0 || w_out == 0 {
                        return Err(CnnError::ShapeMismatch {
                            layer_index: idx,
                            hint: format!(
                                "pool produces zero spatial dim from {h}x{w} (kernel {kernel}, stride {stride})"
                            ),
                        });
                    }
                    h = h_out;
                    w = w_out;
                    layers.push(CnnLayer::Pool(layer));
                }
                CnnLayerSpec::Relu => {
                    layers.push(CnnLayer::Relu(ReluLayer::new()));
                }
                CnnLayerSpec::Flatten => {
                    if flat_dim.is_some() {
                        return Err(CnnError::ShapeMismatch {
                            layer_index: idx,
                            hint: "flatten called twice".into(),
                        });
                    }
                    flat_dim = Some(c * h * w);
                    layers.push(CnnLayer::Flatten(FlattenLayer::new()));
                }
                CnnLayerSpec::Linear { out_features } => {
                    let in_f = flat_dim.ok_or(CnnError::LinearWithoutFlatten(idx))?;
                    let layer = LinearLayer::new(in_f, *out_features, sub_seed);
                    flat_dim = Some(*out_features);
                    output_dim = *out_features;
                    layers.push(CnnLayer::Linear(layer));
                }
            }
        }

        if output_dim == 0 {
            // Network ended on a non-Linear; output is whatever the
            // last spatial / flat layer produced. Treat the running
            // flattened size (if flatten ran) or c*h*w as the dim.
            output_dim = flat_dim.unwrap_or(c * h * w);
        }

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
            input_shape: config.input_shape,
            output_dim,
            layers,
            substrate_cfg: config.substrate,
            substrate_state,
            substrate_effects: None,
            substrate_tick_counter: 0,
            thalamic_channel,
            thalamic_submit_threshold,
        })
    }

    /// Forward over a 4-D input. Returns `[batch, output_dim]`.
    ///
    /// # Panics
    /// If `input.shape()[1..]` doesn't match `self.input_shape`.
    pub fn forward(&self, input: &Array4<f32>) -> Array2<f32> {
        let (_n, c, h, w) = input.dim();
        assert_eq!(
            (c, h, w),
            self.input_shape,
            "cnn input shape mismatch (expected {:?}, got {:?})",
            self.input_shape,
            (c, h, w)
        );

        // Carry both possible representations — at most one is "live"
        // at a time, transitioning at the Flatten layer.
        let mut spatial: Option<Array4<f32>> = Some(input.clone());
        let mut flat: Option<Array2<f32>> = None;

        for layer in &self.layers {
            match layer {
                CnnLayer::Conv(c) => {
                    let cur = spatial.take().expect("conv after flatten");
                    spatial = Some(c.forward(&cur));
                }
                CnnLayer::Pool(p) => {
                    let cur = spatial.take().expect("pool after flatten");
                    spatial = Some(p.forward(&cur));
                }
                CnnLayer::Relu(r) => {
                    if let Some(cur) = spatial.take() {
                        spatial = Some(r.forward(&cur));
                    } else if let Some(cur) = flat.take() {
                        // Apply ReLU on flat tensor too — promote to a
                        // 4-D shape `[N, C, 1, 1]`-equivalent isn't
                        // needed; do it in place.
                        flat = Some(cur.mapv(|x| if x > 0.0 { x } else { 0.0 }));
                    }
                }
                CnnLayer::Flatten(f) => {
                    let cur = spatial.take().expect("flatten without spatial");
                    flat = Some(f.forward(&cur));
                }
                CnnLayer::Linear(l) => {
                    let cur = flat.take().expect("linear without flatten");
                    flat = Some(l.forward(&cur));
                }
            }
        }

        if let Some(f) = flat {
            f
        } else {
            // Network ended on a spatial layer — flatten implicitly so
            // the caller always sees `[batch, out_dim]`.
            let s = spatial.expect("forward produced no output");
            FlattenLayer::new().forward(&s)
        }
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
    /// the cached effects attenuate the output logits by
    /// `dend.integration_efficiency`; when a thalamic channel is open the
    /// input tensor is attention-scaled and output activity above
    /// `submit_threshold` records a submit. Substrate ATP/ion/membrane is
    /// debited at the end of the step.
    ///
    /// Falls back to [`Self::forward`] bit-for-bit when neither substrate
    /// nor thalamic is configured.
    pub fn forward_modulated(&mut self, input: &Array4<f32>) -> Array2<f32> {
        let has_substrate = self.substrate_cfg.enabled;
        let has_thalamic = self.thalamic_channel.is_some();
        if !has_substrate && !has_thalamic {
            return self.forward(input);
        }

        // Substrate effects cadence.
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

        // Thalamic attention-scale on the whole input tensor.
        let attn = crate::substrate_adapter::attention_scalar(self.thalamic_channel.as_ref());
        let modulated_input = if (attn - 1.0).abs() > f32::EPSILON {
            input * attn
        } else {
            input.clone()
        };

        // Core forward.
        let mut out = self.forward(&modulated_input);

        // Substrate output gain (signal fidelity falls as chemistry
        // degrades).
        let gain = crate::substrate_adapter::integration_gain(
            self.substrate_effects.as_ref(),
            has_substrate && self.substrate_cfg.integration_gain_on,
        );
        if (gain - 1.0).abs() > f32::EPSILON {
            out.mapv_inplace(|v| v * gain);
        }

        // Thalamic auto-submit on output magnitude.
        if let Some(ch) = self.thalamic_channel.as_mut() {
            let mag = out.iter().map(|v| v * v).sum::<f32>().sqrt();
            if mag > self.thalamic_submit_threshold {
                ch.record_submit();
            }
        }

        // Substrate debit — one "spike-equivalent" per output unit that
        // crosses a soft threshold (|y| > 0.5).
        if let Some(ref mut s) = self.substrate_state {
            let n_active = out.iter().filter(|&&v| v.abs() > 0.5).count() as u64;
            nimcp_substrate::debit_activity(s, &self.substrate_cfg.dynamics, n_active, 0);
        }

        out
    }

    /// Compute the effective output gain from the CNN to a given
    /// destination using the supplied router's Hebbian weights. Returns
    /// `1.0` when the CNN has no thalamic channel.
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
    /// Returns `None` on CNNs built without substrate.
    #[must_use]
    pub fn substrate_state(&self) -> Option<&nimcp_substrate::NeuralSubstrate> {
        self.substrate_state.as_ref()
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use ndarray::Array4;

    #[test]
    fn deterministic_init_round_trips_bit_for_bit() {
        let cfg = CnnConfig {
            input_shape: (1, 8, 8),
            layers: vec![
                CnnLayerSpec::Conv2d {
                    out_channels: 4,
                    kh: 3,
                    kw: 3,
                    stride: 1,
                    padding: 1,
                },
                CnnLayerSpec::Relu,
                CnnLayerSpec::MaxPool2d { kernel: 2, stride: 2 },
                CnnLayerSpec::Flatten,
                CnnLayerSpec::Linear { out_features: 5 },
            ],
            rng_seed: 0xC1A551C,
            substrate: Default::default(),
            thalamic: None,
        };
        let a = CnnNetwork::new(cfg.clone()).unwrap();
        let b = CnnNetwork::new(cfg).unwrap();

        // Compare conv weights.
        let (CnnLayer::Conv(ca), CnnLayer::Conv(cb)) = (&a.layers[0], &b.layers[0]) else {
            panic!("conv expected at index 0");
        };
        assert_eq!(ca.weight, cb.weight, "conv weights differ across builds");

        // Linear weights.
        let (CnnLayer::Linear(la), CnnLayer::Linear(lb)) = (&a.layers[4], &b.layers[4]) else {
            panic!("linear expected at index 4");
        };
        assert_eq!(la.weight, lb.weight, "linear weights differ across builds");
    }

    #[test]
    fn conv_output_shape_matches_formula() {
        let layer = Conv2dLayer::new(3, 8, 3, 3, 1, 1, 1, 1, 0xABC);
        let inp = Array4::<f32>::zeros((2, 3, 16, 16));
        let out = layer.forward(&inp);
        assert_eq!(out.dim(), (2, 8, 16, 16));

        let strided = Conv2dLayer::new(3, 4, 3, 3, 2, 2, 0, 0, 0xDEF);
        let out2 = strided.forward(&inp);
        assert_eq!(out2.dim(), (2, 4, 7, 7));
    }

    #[test]
    fn maxpool_downsamples_correctly() {
        let pool = MaxPool2dLayer::square(2);
        let inp = Array4::from_shape_fn((1, 1, 4, 4), |(_, _, h, w)| (h * 4 + w) as f32);
        let out = pool.forward(&inp);
        assert_eq!(out.dim(), (1, 1, 2, 2));
        // 4×4 grid 0..15 with 2×2 max pool → corners 5, 7, 13, 15.
        assert_eq!(out[[0, 0, 0, 0]], 5.0);
        assert_eq!(out[[0, 0, 0, 1]], 7.0);
        assert_eq!(out[[0, 0, 1, 0]], 13.0);
        assert_eq!(out[[0, 0, 1, 1]], 15.0);
    }

    #[test]
    fn lenet_shaped_forward_is_finite() {
        // Conv → Pool → Conv → Pool → Flatten → Linear.
        let cfg = CnnConfig {
            input_shape: (1, 28, 28),
            layers: vec![
                CnnLayerSpec::Conv2d {
                    out_channels: 6,
                    kh: 5,
                    kw: 5,
                    stride: 1,
                    padding: 0,
                }, // 28 → 24
                CnnLayerSpec::Relu,
                CnnLayerSpec::MaxPool2d { kernel: 2, stride: 2 }, // 24 → 12
                CnnLayerSpec::Conv2d {
                    out_channels: 16,
                    kh: 5,
                    kw: 5,
                    stride: 1,
                    padding: 0,
                }, // 12 → 8
                CnnLayerSpec::Relu,
                CnnLayerSpec::MaxPool2d { kernel: 2, stride: 2 }, // 8 → 4
                CnnLayerSpec::Flatten,
                CnnLayerSpec::Linear { out_features: 10 },
            ],
            rng_seed: 0x1E_4E_57,
            substrate: Default::default(),
            thalamic: None,
        };
        let net = CnnNetwork::new(cfg).unwrap();
        assert_eq!(net.output_dim, 10);

        // Random-ish input — deterministic via shape-fn so the test is
        // reproducible without dragging in `rand`.
        let inp = Array4::from_shape_fn((1, 1, 28, 28), |(_, _, h, w)| {
            ((h * 28 + w) as f32 * 0.001).sin()
        });
        let out = net.forward(&inp);
        assert_eq!(out.dim(), (1, 10));
        for v in out.iter() {
            assert!(v.is_finite(), "lenet forward produced non-finite: {v}");
        }
    }

    #[test]
    fn config_round_trips_through_serde() {
        let cfg = CnnConfig {
            input_shape: (3, 16, 16),
            layers: vec![
                CnnLayerSpec::Conv2d {
                    out_channels: 4,
                    kh: 3,
                    kw: 3,
                    stride: 1,
                    padding: 1,
                },
                CnnLayerSpec::Flatten,
                CnnLayerSpec::Linear { out_features: 2 },
            ],
            rng_seed: 99,
            substrate: Default::default(),
            thalamic: None,
        };
        let net = CnnNetwork::new(cfg).unwrap();
        let json = serde_json::to_string(&net).unwrap();
        let restored: CnnNetwork = serde_json::from_str(&json).unwrap();
        assert_eq!(restored.input_shape, net.input_shape);
        assert_eq!(restored.output_dim, net.output_dim);
        assert_eq!(restored.layers.len(), net.layers.len());
    }

    // --- Phase 11-substrate ---

    fn small_cfg(seed: u64) -> CnnConfig {
        CnnConfig {
            input_shape: (1, 4, 4),
            layers: vec![
                CnnLayerSpec::Conv2d {
                    out_channels: 2,
                    kh: 3,
                    kw: 3,
                    stride: 1,
                    padding: 1,
                },
                CnnLayerSpec::Relu,
                CnnLayerSpec::Flatten,
                CnnLayerSpec::Linear { out_features: 3 },
            ],
            rng_seed: seed,
            substrate: Default::default(),
            thalamic: None,
        }
    }

    #[test]
    fn modulated_equals_forward_when_disabled() {
        let mut net = CnnNetwork::new(small_cfg(0xABCD)).unwrap();
        let inp = Array4::from_shape_fn((1, 1, 4, 4), |(_, _, h, w)| (h + w) as f32 * 0.1);
        let plain = net.forward(&inp);
        let modulated = net.forward_modulated(&inp);
        assert_eq!(plain, modulated, "disabled path must be bit-identical");
    }

    #[test]
    fn substrate_full_health_is_identity() {
        let mut cfg = small_cfg(0xBEEF);
        cfg.substrate.enabled = true; // full-health initial_state by default
        let mut net = CnnNetwork::new(cfg).unwrap();
        let plain_net = CnnNetwork::new(small_cfg(0xBEEF)).unwrap();
        let inp = Array4::from_shape_fn((1, 1, 4, 4), |(_, _, h, w)| (h * 4 + w) as f32 * 0.05);
        let modulated = net.forward_modulated(&inp);
        let plain = plain_net.forward(&inp);
        for (a, b) in modulated.iter().zip(plain.iter()) {
            assert!((a - b).abs() < 1e-6, "full health should be identity: {a} vs {b}");
        }
    }

    #[test]
    fn substrate_debits_under_activity() {
        let mut cfg = small_cfg(0xD00D);
        cfg.substrate.enabled = true;
        let mut net = CnnNetwork::new(cfg).unwrap();
        let atp_before = net.substrate_state().unwrap().atp_level;
        // Large input → several output units cross |y| > 0.5.
        let inp = Array4::from_elem((1, 1, 4, 4), 5.0);
        for _ in 0..20 {
            let _ = net.forward_modulated(&inp);
        }
        let atp_after = net.substrate_state().unwrap().atp_level;
        assert!(atp_after <= atp_before, "ATP should not rise under activity");
    }

    #[test]
    fn thalamic_burst_amplifies_input() {
        let mut cfg = small_cfg(0x7777);
        cfg.thalamic = Some(CnnThalamicCfg {
            source_id: 1,
            destinations: vec![2, 3],
            submit_threshold: 1e9, // never submit, isolate the input-scale effect
            mode: nimcp_thalamic::RelayMode::Burst,
        });
        let mut net = CnnNetwork::new(cfg).unwrap();
        let plain_net = CnnNetwork::new(small_cfg(0x7777)).unwrap();
        let inp = Array4::from_shape_fn((1, 1, 4, 4), |(_, _, h, w)| (h + w) as f32 * 0.1 + 0.05);
        let modulated = net.forward_modulated(&inp);
        let plain = plain_net.forward(&inp);
        // Burst scales input by 1.2 → through the conv (linear) + relu the
        // output should differ from the unscaled forward somewhere.
        let differs = modulated
            .iter()
            .zip(plain.iter())
            .any(|(a, b)| (a - b).abs() > 1e-6);
        assert!(differs, "burst attention should change the output");
    }
}
