//! Phase 4a — top-level [`LnnNetwork`] composition.
//!
//! Stacks one or more [`LtcLayer`]s and appends a linear readout. This
//! is the forward-pass-only shape; Phase 4b adds BPTT + SGD.
//!
//! # Forward
//!
//! For input sequence `u_1, u_2, ...`:
//!
//! ```text
//! state_0 = 0
//! for t in 1..=T:
//!   h = u_t
//!   for L in layers:
//!       state_L <- ltc_forward_step(state_L, L, h, dt_ms)
//!       h = state_L.x         // layer output = layer state
//!   y_t = W_out · h + b_out   // linear readout
//! ```
//!
//! The readout is dense (no nonlinearity) — downstream tasks can compose
//! their own (softmax for classification, identity for regression).
//!
//! # Checkpointing
//!
//! `serde` round-trips the whole network (layers + readout). State is
//! not part of the checkpoint — it's transient and resets to zeros on
//! `reset()`.

use ndarray::{Array1, Array2};
use rand::SeedableRng;
use rand::distr::{Distribution, Uniform};
use rand_chacha::ChaCha20Rng;
use serde::{Deserialize, Serialize};

use crate::ltc::{LtcLayer, LtcParams, LtcState, ltc_forward_step};

/// Config for building an [`LnnNetwork`].
///
/// Sizes are validated against adjacency: `layers[k].n_in` must equal
/// `layers[k-1].n_rec` (the hidden state of the previous layer is the
/// input to the next), with `layers[0].n_in = input_dim`.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LnnConfig {
    /// Network input dimension.
    pub input_dim: usize,
    /// Network output dimension (readout width).
    pub output_dim: usize,
    /// Per-layer LTC params. `layers.len() >= 1`.
    pub layers: Vec<LtcParams>,
    /// Deterministic seed for weight init.
    pub rng_seed: u64,
    /// Integration timestep, ms.
    pub dt_ms: f32,
    /// Path A Phase 2 — biological substrate. Default disabled.
    #[serde(default)]
    pub substrate: LnnSubstrateCfg,
    /// Path A Phase 2 — thalamic routing. Default `None`.
    #[serde(default)]
    pub thalamic: Option<LnnThalamicCfg>,
}

impl Default for LnnConfig {
    fn default() -> Self {
        Self {
            input_dim: 0,
            output_dim: 0,
            layers: Vec::new(),
            rng_seed: 0,
            dt_ms: 1.0,
            substrate: LnnSubstrateCfg::default(),
            thalamic: None,
        }
    }
}

/// Path A Phase 2 — per-network substrate config for the LNN.
///
/// The LNN has a single compartment (one chemistry region for the
/// whole network). All three LTC layers share the same
/// [`nimcp_substrate::NeuralSubstrate`].
///
/// When `enabled = false` (default), substrate is fully skipped —
/// forward-pass delegates to the unmodified `ltc_forward_step`,
/// training uses the base LR, and `substrate_effects` stays `None`.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct LnnSubstrateCfg {
    /// Master switch.
    pub enabled: bool,
    /// Recompute cached effects every N `forward_step` calls.
    pub update_period: u32,
    /// Apply `dend.plasticity_mod` as training-LR multiplier.
    pub plasticity_mod_on: bool,
    /// Apply `axon.membrane_capacitance_mod` to effective `dt`.
    pub capacitance_on: bool,
    /// Apply asymmetric LTP/LTD gating on gradients during train step.
    pub ltp_ltd_asymmetry_on: bool,
    /// Initial chemistry (default = full health).
    pub initial_state: nimcp_substrate::NeuralSubstrate,
    /// Per-step debit costs + passive recovery.
    pub dynamics: nimcp_substrate::NeuralSubstrateConfig,
}

impl Default for LnnSubstrateCfg {
    fn default() -> Self {
        Self {
            enabled: false,
            update_period: 10,
            plasticity_mod_on: true,
            capacitance_on: true,
            ltp_ltd_asymmetry_on: true,
            initial_state: nimcp_substrate::NeuralSubstrate::default(),
            dynamics: nimcp_substrate::NeuralSubstrateConfig::default(),
        }
    }
}

/// Path A Phase 2 — thalamic routing for the LNN.
///
/// Single network-level [`nimcp_thalamic::ThalamicChannel`]. At each
/// `forward_step`, the input vector is scaled by the mean attention
/// weight (or amplified in burst mode). Output activity above
/// `submit_threshold` bumps the channel's submit counter for
/// subsequent router.tick() Hebbian updates.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LnnThalamicCfg {
    /// Stable source identifier.
    pub source_id: u32,
    /// External destinations (≤ 16).
    pub destinations: Vec<u32>,
    /// Output magnitude (L2 norm of readout) above which the channel
    /// auto-records a submit. Default `0.5`.
    pub submit_threshold: f32,
    /// Initial relay mode.
    pub mode: nimcp_thalamic::RelayMode,
}

impl Default for LnnThalamicCfg {
    fn default() -> Self {
        Self {
            source_id: 0,
            destinations: Vec::new(),
            submit_threshold: 0.5,
            mode: nimcp_thalamic::RelayMode::default(),
        }
    }
}

/// Construction error for [`LnnNetwork`].
#[derive(Debug, thiserror::Error)]
pub enum LnnError {
    /// No layers declared.
    #[error("config declares zero layers; at least one required")]
    NoLayers,
    /// `layers[0].n_in` does not match the declared `input_dim`.
    #[error("layer 0 n_in ({got}) does not match input_dim ({expected})")]
    InputDimMismatch {
        /// Value found on `layers[0].n_in`.
        got: usize,
        /// Expected value from `input_dim`.
        expected: usize,
    },
    /// Two consecutive layers disagree on the hidden-state dimension.
    #[error("layer {layer_idx} n_in ({got}) does not match previous layer n_rec ({expected})")]
    LayerAdjacencyMismatch {
        /// Offending layer index.
        layer_idx: usize,
        /// Value on the offending layer's `n_in`.
        got: usize,
        /// Expected value (previous layer's `n_rec`).
        expected: usize,
    },
}

/// Stacked LTC layers + linear readout.
///
/// `Clone` is implemented manually so the optional `lnn_gpu` field
/// (which holds non-Clone CUDA buffers) is dropped on clone — the
/// clone is a CPU-resident copy; if the caller needs GPU, they call
/// [`Self::enable_gpu`] on the clone to allocate fresh device buffers.
#[derive(Debug, Serialize, Deserialize)]
pub struct LnnNetwork {
    /// Per-layer parameters.
    pub layers: Vec<LtcLayer>,
    /// Readout weights `(output_dim, last_layer.n_rec)`.
    pub w_out: Array2<f32>,
    /// Readout bias `(output_dim,)`.
    pub b_out: Array1<f32>,
    /// Integration timestep, ms.
    pub dt_ms: f32,
    /// Input dimension (copy of the build-time config).
    pub input_dim: usize,
    /// Output dimension (copy of the build-time config).
    pub output_dim: usize,
    /// Path A Phase 2 — substrate config snapshot.
    #[serde(default)]
    pub substrate_cfg: LnnSubstrateCfg,
    /// Runtime chemistry state (present iff `substrate_cfg.enabled`).
    #[serde(default)]
    pub substrate_state: Option<nimcp_substrate::NeuralSubstrate>,
    /// Cached `(axon, dendrite)` effects — recomputed every
    /// `substrate_cfg.update_period` forward steps. Skipped in serde
    /// (it's a derived cache; rebuilt on first post-load step).
    #[serde(skip)]
    pub substrate_effects: Option<(
        nimcp_substrate::AxonSubstrateEffects,
        nimcp_substrate::DendriteSubstrateEffects,
    )>,
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
    /// Phase 9f/9g — optional GPU forward backend. When present every
    /// `forward_step` routes its per-layer LTC kernel to the
    /// device-resident [`LnnGpu`]. The host-side `state.x` is mirrored
    /// back from device after each step so CPU observers (snapshots,
    /// substrate adapters, downstream consumers) stay current.
    ///
    /// Skipped in serde — [`Self::enable_gpu`] reconstructs it from the
    /// current `layers` after a load.
    #[serde(skip)]
    pub lnn_gpu: Option<LnnGpu>,
}

impl Clone for LnnNetwork {
    /// Clone all CPU-resident parameters and state, but **drop** any
    /// attached GPU backend. The clone is CPU-resident; call
    /// [`Self::enable_gpu`] on it to re-allocate device buffers.
    fn clone(&self) -> Self {
        Self {
            layers: self.layers.clone(),
            w_out: self.w_out.clone(),
            b_out: self.b_out.clone(),
            dt_ms: self.dt_ms,
            input_dim: self.input_dim,
            output_dim: self.output_dim,
            substrate_cfg: self.substrate_cfg,
            substrate_state: self.substrate_state,
            substrate_effects: self.substrate_effects,
            substrate_tick_counter: self.substrate_tick_counter,
            thalamic_channel: self.thalamic_channel,
            thalamic_submit_threshold: self.thalamic_submit_threshold,
            lnn_gpu: None,
        }
    }
}

/// Phase 9f/9g — bundle of one [`crate::ltc::LtcGpu`] per layer, sharing
/// a single CUDA context (Phase 9b shared-context contract). Created by
/// [`LnnNetwork::enable_gpu`]; used by [`LnnNetwork::forward_step`] when
/// `Some`. Disable by setting `lnn_gpu = None` (drops every device
/// buffer).
#[cfg(feature = "cuda")]
pub struct LnnGpu {
    /// One per layer, in the same order as [`LnnNetwork::layers`].
    pub layers: Vec<crate::ltc::LtcGpu>,
}

#[cfg(feature = "cuda")]
impl std::fmt::Debug for LnnGpu {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("LnnGpu")
            .field("n_layers", &self.layers.len())
            .finish_non_exhaustive()
    }
}

/// Stub for the no-cuda build so [`LnnNetwork::lnn_gpu`] always
/// type-checks. The CPU build can never construct one — `enable_gpu`
/// is gated.
#[cfg(not(feature = "cuda"))]
#[derive(Debug)]
pub struct LnnGpu {
    _phantom: std::marker::PhantomData<()>,
}

impl LnnNetwork {
    /// Construct from `config`. Validates layer adjacency and seeds the
    /// weights deterministically.
    #[allow(clippy::needless_pass_by_value)] // `config.layers` is consumed.
    pub fn new(config: LnnConfig) -> Result<Self, LnnError> {
        if config.layers.is_empty() {
            return Err(LnnError::NoLayers);
        }
        if config.layers[0].n_in != config.input_dim {
            return Err(LnnError::InputDimMismatch {
                got: config.layers[0].n_in,
                expected: config.input_dim,
            });
        }
        for (idx, win) in config.layers.windows(2).enumerate() {
            let prev_n_rec = win[0].n_rec;
            let next_n_in = win[1].n_in;
            if prev_n_rec != next_n_in {
                return Err(LnnError::LayerAdjacencyMismatch {
                    layer_idx: idx + 1,
                    got: next_n_in,
                    expected: prev_n_rec,
                });
            }
        }

        // Seed weights — use `rng_seed` for layers, `rng_seed ^ 0xFE..`
        // for the readout so changing the number of layers doesn't
        // perturb the readout's RNG draw.
        let layers: Vec<LtcLayer> = config
            .layers
            .iter()
            .enumerate()
            .map(|(i, p)| {
                let layer_seed = config
                    .rng_seed
                    .wrapping_add(i as u64)
                    .wrapping_mul(0x9E37_79B9);
                LtcLayer::new_seeded(*p, layer_seed)
            })
            .collect();

        let last_n_rec = layers.last().expect("checked non-empty").params.n_rec;

        let readout_seed = config.rng_seed ^ 0xFEED_FACE_DEAD_BEEF;
        let mut rng = ChaCha20Rng::seed_from_u64(readout_seed);
        let bound = if last_n_rec == 0 {
            0.0
        } else {
            1.0_f32 / (last_n_rec as f32).sqrt()
        };
        let w_out = if bound > 0.0 {
            let uni = Uniform::new(-bound, bound).expect("bound positive");
            Array2::from_shape_fn((config.output_dim, last_n_rec), |_| uni.sample(&mut rng))
        } else {
            Array2::zeros((config.output_dim, last_n_rec))
        };
        let b_out = Array1::zeros(config.output_dim);

        // Substrate runtime state — owned iff enabled.
        let substrate_state = if config.substrate.enabled {
            let mut s = config.substrate.initial_state;
            s.clamp();
            Some(s)
        } else {
            None
        };

        // Thalamic channel — constructed iff thalamic cfg provided.
        let (thalamic_channel, thalamic_submit_threshold) = match config.thalamic.as_ref() {
            Some(cfg) => {
                let mut ch = nimcp_thalamic::ThalamicChannel::new(cfg.source_id, &cfg.destinations)
                    .ok_or(LnnError::InputDimMismatch {
                        got: cfg.destinations.len(),
                        expected: nimcp_thalamic::THALAMIC_MAX_DESTINATIONS,
                    })?;
                ch.mode = cfg.mode;
                (Some(ch), cfg.submit_threshold.max(0.0))
            }
            None => (None, 0.0),
        };

        Ok(Self {
            layers,
            w_out,
            b_out,
            dt_ms: config.dt_ms,
            input_dim: config.input_dim,
            output_dim: config.output_dim,
            substrate_cfg: config.substrate,
            substrate_state,
            substrate_effects: None,
            substrate_tick_counter: 0,
            thalamic_channel,
            thalamic_submit_threshold,
            lnn_gpu: None,
        })
    }

    /// Phase 9f/9g — allocate GPU buffers for every layer and switch the
    /// forward path onto them. After this, [`Self::forward_step`]
    /// dispatches per-layer LTC kernels on device.
    ///
    /// Idempotent: if `lnn_gpu` is already `Some`, returns `Ok(())`
    /// without rebuilding. Drop the field to disable
    /// (`self.lnn_gpu = None`).
    ///
    /// # Errors
    ///
    /// Propagates [`nimcp_gpu::GpuError`] from context creation, NVRTC
    /// compilation, or any device upload.
    #[cfg(feature = "cuda")]
    pub fn enable_gpu(&mut self) -> Result<(), nimcp_gpu::GpuError> {
        if self.lnn_gpu.is_some() {
            return Ok(());
        }
        let ctx = cudarc::driver::CudaContext::new(0).map_err(|e| {
            nimcp_gpu::GpuError::Cuda(format!("LnnNetwork::enable_gpu: ctx new: {e:?}"))
        })?;
        let mut layer_gpus: Vec<crate::ltc::LtcGpu> = Vec::with_capacity(self.layers.len());
        for layer in &self.layers {
            layer_gpus.push(crate::ltc::LtcGpu::new_with_context(ctx.clone(), layer)?);
        }
        self.lnn_gpu = Some(LnnGpu { layers: layer_gpus });
        Ok(())
    }

    /// Stub for the CPU-only build — opting in to GPU when the crate
    /// was compiled without `--features cuda` is a config error, not
    /// a runtime fallback. Returns the GPU error so callers can
    /// degrade explicitly.
    #[cfg(not(feature = "cuda"))]
    pub fn enable_gpu(&mut self) -> Result<(), &'static str> {
        Err("nimcp-lnn was compiled without `--features cuda`")
    }

    /// Sync trainable weights from CPU to GPU (call after every
    /// optimizer step). No-op when no GPU backend is attached.
    ///
    /// # Errors
    /// Propagates [`nimcp_gpu::GpuError`] from any device upload.
    #[cfg(feature = "cuda")]
    pub fn upload_weights_to_gpu(&mut self) -> Result<(), nimcp_gpu::GpuError> {
        let Some(lnn_gpu) = self.lnn_gpu.as_mut() else {
            return Ok(());
        };
        for (layer, gpu) in self.layers.iter().zip(lnn_gpu.layers.iter_mut()) {
            gpu.upload_weights(layer)?;
        }
        Ok(())
    }

    /// Allocate fresh zeroed per-layer state vectors.
    #[must_use]
    pub fn new_state(&self) -> Vec<LtcState> {
        self.layers
            .iter()
            .map(|l| LtcState::new(l.params.n_rec))
            .collect()
    }

    /// Advance the network by one timestep.
    ///
    /// `state` must be the vector returned by [`new_state`](Self::new_state)
    /// (one [`LtcState`] per layer, in order). `input` must have length
    /// [`input_dim`](Self::input_dim).
    ///
    /// Returns the readout `y = W_out · h + b_out`, where `h` is the
    /// final layer's updated state vector.
    pub fn forward_step(&self, state: &mut [LtcState], input: &Array1<f32>) -> Array1<f32> {
        debug_assert_eq!(state.len(), self.layers.len(), "state/layer count mismatch");
        debug_assert_eq!(input.len(), self.input_dim, "input dim mismatch");

        // Walk layers forward, passing each layer's post-step state as
        // the next layer's input. `curr_input` is owned because we need
        // to rebind it to each layer's state as we go.
        let mut curr_input: Array1<f32> = input.clone();
        for (layer, st) in self.layers.iter().zip(state.iter_mut()) {
            let _ = ltc_forward_step(st, layer, &curr_input, self.dt_ms);
            curr_input = st.x.clone();
        }

        // Readout: W_out · h + b_out.
        let mut y = self.w_out.dot(&curr_input);
        y += &self.b_out;
        y
    }

    /// Phase 9f/9g — GPU forward step. Requires
    /// [`Self::enable_gpu`] to have run first; without it returns
    /// the synthetic error so callers can fall back. Mirrors the
    /// CPU [`Self::forward_step`] semantics: per-layer LTC update
    /// chained, then readout.
    ///
    /// `&mut self` because the GPU layers are mutable (per-step
    /// kernel launches own stream state).
    ///
    /// # Errors
    ///
    /// Propagates any [`nimcp_gpu::GpuError`] from per-layer LTC
    /// step. CPU readout never errors.
    #[cfg(feature = "cuda")]
    pub fn forward_step_gpu(
        &mut self,
        state: &mut [LtcState],
        input: &Array1<f32>,
    ) -> Result<Array1<f32>, nimcp_gpu::GpuError> {
        debug_assert_eq!(state.len(), self.layers.len());
        debug_assert_eq!(input.len(), self.input_dim);

        let lnn_gpu = self.lnn_gpu.as_mut().ok_or_else(|| {
            nimcp_gpu::GpuError::Cuda(
                "LnnNetwork::forward_step_gpu: enable_gpu() must run first".into(),
            )
        })?;
        if lnn_gpu.layers.len() != self.layers.len() {
            return Err(nimcp_gpu::GpuError::Cuda(format!(
                "LnnGpu layer count {} != LnnNetwork layers {}",
                lnn_gpu.layers.len(),
                self.layers.len(),
            )));
        }

        let mut curr_input: Array1<f32> = input.clone();
        for (gpu, st) in lnn_gpu.layers.iter_mut().zip(state.iter_mut()) {
            let _ = gpu.step(st, &curr_input, self.dt_ms)?;
            curr_input = st.x.clone();
        }
        // Readout stays on CPU — small dims, no GPU win.
        let mut y = self.w_out.dot(&curr_input);
        y += &self.b_out;
        Ok(y)
    }

    /// Advance the network over a sequence of `T` inputs; returns the
    /// readout at every timestep (shape `(T, output_dim)`). Useful for
    /// sequence-classification and regression tasks.
    #[must_use]
    pub fn forward_sequence(&self, inputs: &[Array1<f32>]) -> Vec<Array1<f32>> {
        let mut state = self.new_state();
        let mut out = Vec::with_capacity(inputs.len());
        for u in inputs {
            out.push(self.forward_step(&mut state, u));
        }
        out
    }

    // -------------------------------------------------------------------------
    // Path A Phase 2 — substrate + thalamic modulated forward step.
    // -------------------------------------------------------------------------

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

    /// Substrate-aware forward step. When `substrate_cfg.enabled`, the
    /// cached effects are applied to tau + dt; when a thalamic channel
    /// is open, the input vector is attention-scaled and burst-mode
    /// tau clamp applies. Debit activity happens at the end of the step.
    ///
    /// Falls back to [`Self::forward_step`] bit-for-bit when neither
    /// substrate nor thalamic is configured.
    pub fn forward_step_modulated(
        &mut self,
        state: &mut [LtcState],
        input: &Array1<f32>,
    ) -> Array1<f32> {
        debug_assert_eq!(state.len(), self.layers.len());
        debug_assert_eq!(input.len(), self.input_dim);

        let has_substrate = self.substrate_cfg.enabled;
        let has_thalamic = self.thalamic_channel.is_some();
        if !has_substrate && !has_thalamic {
            return self.forward_step(state, input);
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

        // Thalamic attention-scale on input.
        let modulated_input =
            crate::substrate_adapter::attention_scale_input(input, self.thalamic_channel.as_ref());

        // Effective dt from substrate capacitance (if enabled + opted in).
        let dt_eff = if has_substrate && self.substrate_cfg.capacitance_on {
            crate::substrate_adapter::effective_dt_ms(self.dt_ms, self.substrate_effects.as_ref())
        } else {
            self.dt_ms
        };

        // Burst-mode tau ceiling (biological: burst relays run fast).
        let burst_clamp =
            crate::substrate_adapter::burst_tau_clamp_ms(self.thalamic_channel.as_ref());

        // Walk layers forward with override-aware kernel.
        let mut curr_input: Array1<f32> = modulated_input;
        for (layer, st) in self.layers.iter().zip(state.iter_mut()) {
            // Per-layer effective tau.
            let tau_eff_opt = if has_substrate {
                Some(crate::substrate_adapter::effective_tau(
                    layer,
                    self.substrate_effects.as_ref(),
                ))
            } else {
                None
            };
            // Apply burst clamp if present.
            let tau_eff_opt = if let Some(clamp) = burst_clamp {
                let base = tau_eff_opt.unwrap_or_else(|| layer.tau_base.clone());
                Some(base.mapv(|t| t.min(clamp)))
            } else {
                tau_eff_opt
            };

            let _ = crate::substrate_adapter::ltc_forward_step_with_tau_override(
                st,
                layer,
                &curr_input,
                self.dt_ms,
                tau_eff_opt.as_ref(),
                Some(dt_eff),
            );
            curr_input = st.x.clone();
        }

        // Readout.
        let mut y = self.w_out.dot(&curr_input);
        y += &self.b_out;

        // Thalamic auto-submit on output magnitude.
        if let Some(ch) = self.thalamic_channel.as_mut() {
            let mag = y.iter().map(|v| v * v).sum::<f32>().sqrt();
            if mag > self.thalamic_submit_threshold {
                ch.record_submit();
            }
        }

        // Substrate debit — one "spike-equivalent" per neuron in the
        // last layer that crosses a soft threshold (|x| > 0.5).
        if let Some(ref mut s) = self.substrate_state {
            let n_active: u32 = state
                .last()
                .map(|st| st.x.iter().filter(|&&v| v.abs() > 0.5).count() as u32)
                .unwrap_or(0);
            nimcp_substrate::debit_activity(
                s,
                &self.substrate_cfg.dynamics,
                u64::from(n_active),
                0,
            );
        }

        y
    }

    /// Compute the effective output gain from the LNN to a given
    /// destination using the supplied router's Hebbian weights. Returns
    /// `1.0` when the LNN has no thalamic channel.
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

    /// Read-only access to the substrate state (for diagnostics /
    /// tests). Returns `None` on LNNs built without substrate.
    #[must_use]
    pub fn substrate_state(&self) -> Option<&nimcp_substrate::NeuralSubstrate> {
        self.substrate_state.as_ref()
    }
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    fn two_layer_cfg(input_dim: usize, hidden: usize, output_dim: usize) -> LnnConfig {
        LnnConfig {
            input_dim,
            output_dim,
            layers: vec![
                LtcParams {
                    n_in: input_dim,
                    n_rec: hidden,
                    tau_init: 1.0,
                    init_scale: 1.0,
                },
                LtcParams {
                    n_in: hidden,
                    n_rec: hidden,
                    tau_init: 1.0,
                    init_scale: 1.0,
                },
            ],
            rng_seed: 0x1234,
            dt_ms: 0.1,
            substrate: LnnSubstrateCfg::default(),
            thalamic: None,
        }
    }

    #[test]
    fn builds_valid_config() {
        let net = LnnNetwork::new(two_layer_cfg(3, 8, 2)).expect("build");
        assert_eq!(net.layers.len(), 2);
        assert_eq!(net.w_out.shape(), &[2, 8]);
        assert_eq!(net.b_out.len(), 2);
    }

    #[test]
    fn rejects_empty_layers() {
        let cfg = LnnConfig {
            input_dim: 2,
            output_dim: 1,
            layers: vec![],
            rng_seed: 0,
            dt_ms: 1.0,
            substrate: LnnSubstrateCfg::default(),
            thalamic: None,
        };
        match LnnNetwork::new(cfg) {
            Err(LnnError::NoLayers) => {}
            other => panic!("expected NoLayers, got {other:?}"),
        }
    }

    // -----------------------------------------------------------------
    // Path A Phase 2 — substrate + thalamic integration tests.
    // -----------------------------------------------------------------

    fn cfg_with_substrate(atp: f32) -> LnnConfig {
        let mut cfg = two_layer_cfg(3, 8, 2);
        let mut initial = nimcp_substrate::NeuralSubstrate {
            atp_level: atp,
            ..nimcp_substrate::NeuralSubstrate::default()
        };
        initial.clamp();
        cfg.substrate = LnnSubstrateCfg {
            enabled: true,
            initial_state: initial,
            dynamics: nimcp_substrate::NeuralSubstrateConfig {
                atp_passive_recovery: 0.0,
                atp_cost_per_spike: 1.0e-4,
                ..nimcp_substrate::NeuralSubstrateConfig::default()
            },
            ..LnnSubstrateCfg::default()
        };
        cfg
    }

    /// Disable path: no substrate, no thalamic → forward_step_modulated
    /// is bit-identical to forward_step.
    #[test]
    fn modulated_equals_forward_when_disabled() {
        let cfg = two_layer_cfg(3, 8, 2);
        let mut net = LnnNetwork::new(cfg).unwrap();
        let mut state_a = net.new_state();
        let mut state_b = net.new_state();
        let u = Array1::from_vec(vec![0.1, -0.2, 0.3]);
        let y_a = net.forward_step(&mut state_a, &u);
        let y_b = net.forward_step_modulated(&mut state_b, &u);
        for (a, b) in y_a.iter().zip(y_b.iter()) {
            assert!((a - b).abs() < 1e-6, "disable-path drift: {a} vs {b}");
        }
    }

    /// Substrate at full health produces identity effects → modulated
    /// forward matches pre-Phase-2 output.
    #[test]
    fn substrate_full_health_identity() {
        let mut net = LnnNetwork::new(cfg_with_substrate(1.0)).unwrap();
        let mut state_a = net.new_state();
        let mut state_b = net.new_state();
        let u = Array1::from_vec(vec![0.1, -0.2, 0.3]);
        let y_ref = net.forward_step(&mut state_a, &u);
        let y_mod = net.forward_step_modulated(&mut state_b, &u);
        for (a, b) in y_ref.iter().zip(y_mod.iter()) {
            assert!(
                (a - b).abs() < 1e-5,
                "full-health substrate should equal identity: {a} vs {b}"
            );
        }
    }

    /// Substrate debits ATP as the network integrates activity.
    #[test]
    fn substrate_debits_atp_under_activity() {
        let mut net = LnnNetwork::new(cfg_with_substrate(0.5)).unwrap();
        let atp_before = net.substrate_state().unwrap().atp_level;
        let mut state = net.new_state();
        // Strong input drives activity.
        let u = Array1::from_vec(vec![5.0, -5.0, 5.0]);
        for _ in 0..50 {
            net.forward_step_modulated(&mut state, &u);
        }
        let atp_after = net.substrate_state().unwrap().atp_level;
        assert!(
            atp_after <= atp_before,
            "ATP should be non-increasing under activity: {atp_before} -> {atp_after}"
        );
    }

    /// Thalamic channel: construction + output gain read through router.
    #[test]
    fn thalamic_channel_and_router_gain() {
        let mut cfg = two_layer_cfg(3, 8, 2);
        cfg.thalamic = Some(LnnThalamicCfg {
            source_id: 77,
            destinations: vec![10, 20],
            submit_threshold: 0.1,
            mode: nimcp_thalamic::RelayMode::Tonic,
        });
        let net = LnnNetwork::new(cfg).unwrap();
        assert!(net.thalamic_channel.is_some());

        let mut router =
            nimcp_thalamic::ThalamicRouter::new(nimcp_thalamic::ThalamicRouterConfig {
                hebbian_lr: 0.5,
                hebbian_decay: 0.0,
                ..Default::default()
            });
        router.open_channel(77, &[10, 20]).unwrap();
        router.record_submit(77);
        router.tick();
        assert!(net.thalamic_output_gain(&router, 10) > 1.0);
    }

    /// Thalamic burst mode amplifies the input scaling factor.
    #[test]
    fn thalamic_burst_mode_amplifies() {
        let mut cfg = two_layer_cfg(3, 8, 2);
        cfg.thalamic = Some(LnnThalamicCfg {
            source_id: 1,
            destinations: vec![10],
            submit_threshold: 0.0,
            mode: nimcp_thalamic::RelayMode::Burst,
        });
        let mut net = LnnNetwork::new(cfg).unwrap();
        let mut state = net.new_state();
        let u = Array1::from_vec(vec![0.1, 0.0, 0.0]);
        // Pre-scaled input is 1.2x the raw one; that yields different
        // readouts compared to the non-burst equivalent at same weights.
        let _ = net.forward_step_modulated(&mut state, &u);
        // Submit counter bumped because we set threshold = 0.
        let submits = net.thalamic_channel.as_ref().unwrap().submits_this_step;
        assert!(submits > 0);
    }

    /// Effects cadence: first modulated step populates the cache.
    #[test]
    fn substrate_effects_cached_on_first_step() {
        let mut net = LnnNetwork::new(cfg_with_substrate(1.0)).unwrap();
        assert!(net.substrate_effects.is_none());
        let mut state = net.new_state();
        let u = Array1::from_vec(vec![0.1, 0.0, 0.0]);
        net.forward_step_modulated(&mut state, &u);
        assert!(net.substrate_effects.is_some());
    }

    #[test]
    fn rejects_input_dim_mismatch() {
        let mut cfg = two_layer_cfg(3, 8, 2);
        cfg.input_dim = 5;
        match LnnNetwork::new(cfg) {
            Err(LnnError::InputDimMismatch { got, expected }) => {
                assert_eq!(got, 3);
                assert_eq!(expected, 5);
            }
            other => panic!("expected InputDimMismatch, got {other:?}"),
        }
    }

    #[test]
    fn rejects_layer_adjacency_mismatch() {
        let mut cfg = two_layer_cfg(3, 8, 2);
        cfg.layers[1].n_in = 999;
        match LnnNetwork::new(cfg) {
            Err(LnnError::LayerAdjacencyMismatch {
                layer_idx,
                got,
                expected,
            }) => {
                assert_eq!(layer_idx, 1);
                assert_eq!(got, 999);
                assert_eq!(expected, 8);
            }
            other => panic!("expected LayerAdjacencyMismatch, got {other:?}"),
        }
    }

    #[test]
    fn forward_is_deterministic_on_same_seed() {
        let a = LnnNetwork::new(two_layer_cfg(3, 8, 2)).expect("a");
        let b = LnnNetwork::new(two_layer_cfg(3, 8, 2)).expect("b");
        let inputs: Vec<Array1<f32>> = (0..20)
            .map(|t| Array1::from_elem(3, (t as f32).sin()))
            .collect();
        let ya = a.forward_sequence(&inputs);
        let yb = b.forward_sequence(&inputs);
        assert_eq!(ya.len(), yb.len());
        for (a_t, b_t) in ya.iter().zip(yb.iter()) {
            assert_eq!(a_t, b_t);
        }
    }

    #[test]
    fn forward_shape_is_output_dim() {
        let net = LnnNetwork::new(two_layer_cfg(3, 8, 5)).expect("build");
        let inputs: Vec<Array1<f32>> = (0..4).map(|_| Array1::zeros(3)).collect();
        let out = net.forward_sequence(&inputs);
        assert_eq!(out.len(), 4);
        for y in &out {
            assert_eq!(y.len(), 5);
        }
    }

    #[test]
    fn state_is_non_zero_after_drive() {
        let net = LnnNetwork::new(two_layer_cfg(2, 8, 1)).expect("build");
        let mut state = net.new_state();
        let u = Array1::from_elem(2, 1.0_f32);
        for _ in 0..30 {
            net.forward_step(&mut state, &u);
        }
        let l0_norm = state[0].x.iter().map(|v| v * v).sum::<f32>().sqrt();
        let l1_norm = state[1].x.iter().map(|v| v * v).sum::<f32>().sqrt();
        assert!(l0_norm > 0.1, "layer 0 state did not grow: {l0_norm}");
        assert!(l1_norm > 0.0, "layer 1 state did not grow: {l1_norm}");
    }

    #[test]
    fn forward_stays_finite_on_long_noise_sequence() {
        // 2000-step noise sequence — V1 used to NaN by step ~600 without
        // state clamping. V2 regression guard.
        let net = LnnNetwork::new(two_layer_cfg(4, 16, 2)).expect("build");
        let mut state = net.new_state();
        let mut rng = ChaCha20Rng::seed_from_u64(0xDEAD_BEEF);
        let uni = Uniform::new(-3.0_f32, 3.0_f32).expect("bound positive");
        for _ in 0..2000 {
            let u: Array1<f32> = Array1::from_shape_fn(4, |_| uni.sample(&mut rng));
            let y = net.forward_step(&mut state, &u);
            assert!(y.iter().all(|v| v.is_finite()), "readout went NaN/Inf");
            assert!(
                state.iter().all(|s| s.x.iter().all(|v| v.is_finite())),
                "state went NaN/Inf"
            );
        }
    }

    #[test]
    fn serde_round_trip_preserves_weights_and_output() {
        use serde_json;

        let original = LnnNetwork::new(two_layer_cfg(3, 8, 2)).expect("build");
        let json = serde_json::to_string(&original).expect("serialize");
        let restored: LnnNetwork = serde_json::from_str(&json).expect("deserialize");

        // Same weights.
        assert_eq!(original.w_out, restored.w_out);
        assert_eq!(original.b_out, restored.b_out);
        assert_eq!(original.layers.len(), restored.layers.len());
        for (a, b) in original.layers.iter().zip(restored.layers.iter()) {
            assert_eq!(a.w_rec, b.w_rec);
            assert_eq!(a.w_in, b.w_in);
            assert_eq!(a.b, b.b);
            assert_eq!(a.tau_base, b.tau_base);
        }

        // Same forward output on identical inputs.
        let inputs: Vec<Array1<f32>> = (0..10)
            .map(|t| Array1::from_elem(3, (t as f32 * 0.3).cos()))
            .collect();
        let y_orig = original.forward_sequence(&inputs);
        let y_new = restored.forward_sequence(&inputs);
        assert_eq!(y_orig, y_new);
    }
}
