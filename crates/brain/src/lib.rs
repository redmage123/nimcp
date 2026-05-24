//! NIMCP V2 — top-level brain integration.
//!
//! This crate composes the smaller crates into a runnable `Brain`. It owns:
//!
//! - The scheduler (hosts all actors)
//! - The event log (the source of truth)
//! - A collection of network actors (adaptive / SNN / LNN)
//! - The memory actor (Z-Ladder)
//! - The checkpoint coordinator
//!
//! **No 800-field struct.** A `Brain` is a small handle that routes requests
//! to the right actor; each actor owns its own state.
//!
//! # Phase 1..4c scope
//!
//! Phase 1 wired an [`AdaptiveNet`] (MLP). Phase 4c wires in the remaining
//! two networks of the Phase 4 ensemble — [`SnnNetwork`] (spiking) and
//! [`LnnNetwork`] (liquid time-constant). All three are **optional** so
//! callers can keep a lightweight single-network brain, and the joint
//! checkpoint is a directory whose contents round-trip atomically.
//!
//! The scheduler still sits in the struct as a placeholder — actor-per-
//! network routing and a shared loss aggregator ride in later phases
//! (4d+, per V2_PLAN.md).

#![forbid(unsafe_code)]

pub mod actors;
pub mod stats;

use std::path::Path;

use ndarray::{Array1, Array3, Array4};
use nimcp_adaptive::{AdaptiveConfig, AdaptiveError, AdaptiveNet};
use nimcp_cnn::{CnnConfig, CnnNetwork};
use nimcp_core::{Error, Result};
use nimcp_fno::{FnoConfig, FnoNetwork};
use nimcp_hnn::{HnnConfig, HnnNetwork};
use nimcp_language::{CascadeConfig, GroundedLanguage};
use nimcp_lnn::{LnnConfig, LnnNetwork, LtcState, TrainParams};
use nimcp_toxicity::ToxicityStack;
use nimcp_memory::{MemoryNode, QueryHit, ZLadder, ZLadderConfig};
use nimcp_scheduler::{Scheduler, SchedulerConfig};
use nimcp_snn::{SnnConfig, SnnNetwork};
use serde::{Deserialize, Serialize};

/// Phase 9h — execution backend selector. Drives whether SNN + LNN run
/// their forward paths on CPU or GPU.
///
/// `Cpu` is the default — works on every host, no CUDA dependency.
/// `Gpu` is only meaningful when the underlying network crates were
/// compiled with `--features cuda`; on a CPU-only build the brain
/// constructor falls back to CPU and logs a warning.
///
/// The selector is brain-wide rather than per-network so the user has a
/// single knob (e.g. `nimcp.set_backend("gpu")`) to flip the entire
/// inference pipeline.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "lowercase")]
pub enum Backend {
    /// CPU forward pass for every network. Always available.
    #[default]
    Cpu,
    /// GPU forward pass where compiled in. Falls back to CPU + logs a
    /// warning when the `cuda` feature wasn't enabled at build time.
    Gpu,
}

/// Top-level brain configuration.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BrainConfig {
    /// Seed for deterministic init.
    pub rng_seed: u64,
    /// Whether to run in deterministic (single-threaded, virtual time) mode.
    pub deterministic: bool,
    /// Path where the event log + checkpoints live.
    pub state_dir: std::path::PathBuf,
    /// Adaptive network config — the MLP the brain trains against.
    pub adaptive: AdaptiveConfig,
    /// Optional SNN config. `None` → this brain has no spiking network;
    /// `snn_*` methods return errors.
    #[serde(default)]
    pub snn: Option<SnnConfig>,
    /// Optional LNN config. Same semantics as `snn`.
    #[serde(default)]
    pub lnn: Option<LnnConfig>,
    /// Optional CNN config (Phase 11a). `None` → brain has no CNN;
    /// `cnn_*` methods return `Error::Config`.
    #[serde(default)]
    pub cnn: Option<CnnConfig>,
    /// Optional FNO config (Phase 11b). Same semantics as `cnn`.
    #[serde(default)]
    pub fno: Option<FnoConfig>,
    /// Optional HNN config (Phase 11c). Same semantics as `cnn`.
    #[serde(default)]
    pub hnn: Option<HnnConfig>,
    /// Z-Ladder config. `None` → no memory subsystem (Phase 1-4
    /// brains remain valid).
    #[serde(default)]
    pub memory: Option<ZLadderConfig>,
    /// Grounded-language config. `None` → no language subsystem;
    /// `language_*` methods return `Error::Config`.
    #[serde(default)]
    pub language: Option<LanguageConfig>,
    /// Phase 9h — brain-wide execution backend. When `Gpu`, SNN's
    /// `use_gpu_forward` flag is forced on at construction and LNN's
    /// `enable_gpu` is called automatically. When `Cpu` the per-
    /// network configs are honored as-is.
    #[serde(default)]
    pub backend: Backend,
}

impl Default for BrainConfig {
    fn default() -> Self {
        Self {
            rng_seed: 0x5EED,
            deterministic: false,
            state_dir: std::path::PathBuf::from("./nimcp-state"),
            adaptive: AdaptiveConfig::default(),
            snn: None,
            lnn: None,
            cnn: None,
            fno: None,
            hnn: None,
            memory: None,
            language: None,
            backend: Backend::Cpu,
        }
    }
}

/// Grounded-language subsystem config (Phase L8).
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct LanguageConfig {
    /// Semantic / distributional embedding width.
    pub semantic_dim: usize,
    /// RNG seed for deterministic language learning.
    pub rng_seed: u64,
    /// Build the content-safety toxicity stack + gate `respond`.
    pub enable_toxicity: bool,
    /// ML-head seed for the toxicity stack (when `enable_toxicity`).
    pub toxicity_ml_seed: u64,
    /// Enable the bigram FFT spectrum diagnostic with this vocab cap.
    pub spectrum_vocab_cap: Option<usize>,
    /// Default developmental stage for `language_respond`.
    pub default_stage: u32,
    /// Minimum words `language_respond` emits.
    pub min_produce_words: usize,
    /// Recurrent cascade settling iterations (1 = single pass).
    pub recurrent_max_iters: usize,
    /// Opt-in: blend a reasoning conclusion into the cascade content
    /// intent (V1 `reason_in_content`, Tier-1 Step E). Default OFF;
    /// runtime-togglable via [`Brain::set_reason_in_content`]. Dormant
    /// until V2 grows a reasoning subsystem to supply the vector.
    #[serde(default)]
    pub reason_in_content: bool,
}

impl Default for LanguageConfig {
    fn default() -> Self {
        Self {
            semantic_dim: nimcp_language::SEMANTIC_DIM,
            rng_seed: 0x1A4E,
            enable_toxicity: true,
            toxicity_ml_seed: 0x70C1,
            spectrum_vocab_cap: None,
            default_stage: 4,
            min_produce_words: 1,
            recurrent_max_iters: 1,
            reason_in_content: false,
        }
    }
}

/// Result of [`Brain::language_respond`] — carries the (possibly
/// safety-substituted) text plus whether the toxicity gate fired.
#[derive(Debug, Clone)]
pub struct LanguageResponse {
    /// The response text (a counterclaim when `blocked_for_toxicity`).
    pub text: String,
    /// Response confidence (self-match, or `1 − harm` when blocked).
    pub confidence: f32,
    /// True when the must-run toxicity gate replaced the response with a
    /// counterclaim (input was blocked).
    pub blocked_for_toxicity: bool,
    /// Toxic category that triggered the block (empty if not blocked).
    pub toxicity_category: String,
}

/// The top-level brain handle.
pub struct Brain {
    config: BrainConfig,
    #[allow(dead_code)] // scheduler wires up in later phases
    scheduler: Scheduler,
    adaptive: AdaptiveNet,
    snn: Option<SnnNetwork>,
    lnn: Option<LnnNetwork>,
    /// Transient LNN runtime state — mirrors `lnn.new_state()`, reset on
    /// `lnn_reset` or fresh brain.
    lnn_state: Option<Vec<LtcState>>,
    /// Phase 11a — convolutional network. Stateless inference (no
    /// transient state needed alongside it).
    cnn: Option<CnnNetwork>,
    /// Phase 11b — Fourier Neural Operator. Stateless inference.
    fno: Option<FnoNetwork>,
    /// Phase 11c — Hamiltonian Neural Network. State is held inside
    /// the network itself (`q`, `p`).
    hnn: Option<HnnNetwork>,
    memory: Option<ZLadder>,
    /// Phase L8 — grounded-language engine (lexicon, embeddings,
    /// comprehend/produce, cascade). `None` unless `config.language`.
    language: Option<GroundedLanguage>,
    /// Phase L8 — content-safety stack. Built iff `language` is present
    /// AND `LanguageConfig::enable_toxicity`. Not serialized wholesale
    /// (regex/templates rebuild from bundled data); only ML weights are
    /// checkpointed.
    toxicity: Option<ToxicityStack>,
    /// Runtime `reason_in_content` toggle (V1 RPC parity). Initialized
    /// from `LanguageConfig`; gates the cascade reasoning-blend source.
    lang_reason_in_content: bool,
    /// Phase 6c — training-loss tracker for the adaptive network. Always
    /// present; `count == 0` before the first `learn()`.
    adaptive_loss: stats::LossTracker,
    /// Phase 6c — training-loss tracker for the LNN, matched to
    /// `self.lnn.is_some()`.
    lnn_loss: Option<stats::LossTracker>,
    /// Path A — shared thalamic router. Built lazily when any wired
    /// network has a thalamic channel. `None` when no network declares
    /// a thalamic config.
    thalamic_router: Option<nimcp_thalamic::ThalamicRouter>,
}

impl Brain {
    /// Boot a new brain with the given config. SNN / LNN are constructed
    /// only if the corresponding config is `Some`.
    pub fn new(config: BrainConfig) -> Result<Self> {
        let sched_cfg = SchedulerConfig {
            deterministic: config.deterministic,
            mailbox_capacity: 1024,
            rng_seed: config.rng_seed,
            ..SchedulerConfig::default()
        };
        let scheduler = Scheduler::new(sched_cfg);

        // Propagate the brain's rng_seed into adaptive unless the caller
        // overrode it explicitly. Same seed → same init, bit-for-bit.
        let mut adaptive_cfg = config.adaptive.clone();
        if adaptive_cfg.rng_seed == AdaptiveConfig::default().rng_seed {
            adaptive_cfg.rng_seed = config.rng_seed;
        }
        let adaptive = AdaptiveNet::new(adaptive_cfg);

        // Phase 9h — backend dispatch. When `Backend::Gpu`, force SNN's
        // `use_gpu_forward` flag on so per-pop LifGpu / per-edge CsrGpu
        // / RstdpGpu are allocated at construction. CPU build with
        // `Backend::Gpu` is benign — SnnNetwork errors out with a
        // GpuUnavailable, which we log and degrade to CPU.
        let snn = if let Some(mut cfg) = config.snn.clone() {
            if matches!(config.backend, Backend::Gpu) {
                cfg.use_gpu_forward = true;
            }
            match SnnNetwork::new(cfg.clone()) {
                Ok(net) => Some(net),
                Err(e) if matches!(config.backend, Backend::Gpu) => {
                    tracing::warn!(
                        ?e,
                        "Backend::Gpu requested but SNN GPU init failed; falling back to CPU"
                    );
                    cfg.use_gpu_forward = false;
                    Some(
                        SnnNetwork::new(cfg)
                            .map_err(|e| Error::Config(format!("snn: {e}")))?,
                    )
                }
                Err(e) => return Err(Error::Config(format!("snn: {e}"))),
            }
        } else {
            None
        };

        let (lnn, lnn_state) = if let Some(cfg) = config.lnn.clone() {
            // `mut` only needed under cuda feature where enable_gpu
            // mutates the network; allow-unused gates the cpu build.
            #[cfg_attr(not(feature = "cuda"), allow(unused_mut))]
            let mut net = LnnNetwork::new(cfg).map_err(|e| Error::Config(format!("lnn: {e}")))?;
            // Phase 9h — Backend::Gpu also enables LNN GPU forward
            // path. The CPU stub returns an error; on GPU build the
            // call succeeds or the underlying nimcp_gpu::GpuError is
            // logged and we fall back to CPU forward.
            #[cfg(feature = "cuda")]
            if matches!(config.backend, Backend::Gpu) {
                if let Err(e) = net.enable_gpu() {
                    tracing::warn!(
                        ?e,
                        "Backend::Gpu requested but LNN GPU init failed; using CPU forward"
                    );
                }
            }
            #[cfg(not(feature = "cuda"))]
            if matches!(config.backend, Backend::Gpu) {
                tracing::warn!(
                    "Backend::Gpu requested but nimcp-lnn was not compiled with --features cuda; using CPU forward"
                );
            }
            let state = net.new_state();
            (Some(net), Some(state))
        } else {
            (None, None)
        };

        // Phase 11a/b/c — these networks are CPU-only on V2 today;
        // `Backend::Gpu` is honored as a no-op (the plan deferred GPU
        // ports to per-network `11x-gpu` follow-up phases).
        let cnn = if let Some(cfg) = config.cnn.clone() {
            Some(CnnNetwork::new(cfg).map_err(|e| Error::Config(format!("cnn: {e}")))?)
        } else {
            None
        };
        let fno = if let Some(cfg) = config.fno.clone() {
            Some(FnoNetwork::new(cfg).map_err(|e| Error::Config(format!("fno: {e}")))?)
        } else {
            None
        };
        let hnn = if let Some(cfg) = config.hnn.clone() {
            Some(HnnNetwork::new(cfg).map_err(|e| Error::Config(format!("hnn: {e}")))?)
        } else {
            None
        };

        let memory = if let Some(cfg) = config.memory.clone() {
            Some(ZLadder::new(cfg).map_err(|e| Error::Config(format!("memory: {e}")))?)
        } else {
            None
        };

        // Phase L8 — grounded language + content-safety stack.
        let (language, toxicity) = if let Some(cfg) = config.language.clone() {
            let mut gl = GroundedLanguage::new(cfg.semantic_dim, cfg.rng_seed);
            if let Some(cap) = cfg.spectrum_vocab_cap {
                gl.enable_bigram_spectrum(cap);
            }
            let tox = if cfg.enable_toxicity {
                Some(
                    ToxicityStack::with_defaults(cfg.toxicity_ml_seed)
                        .map_err(|e| Error::Config(format!("toxicity: {e}")))?,
                )
            } else {
                None
            };
            (Some(gl), tox)
        } else {
            (None, None)
        };
        let lang_reason_in_content = config
            .language
            .as_ref()
            .is_some_and(|c| c.reason_in_content);

        tracing::info!(
            layers = ?config.adaptive.layers,
            seed = config.rng_seed,
            has_snn = snn.is_some(),
            has_lnn = lnn.is_some(),
            has_cnn = cnn.is_some(),
            has_fno = fno.is_some(),
            has_hnn = hnn.is_some(),
            has_memory = memory.is_some(),
            "brain created"
        );
        let lnn_loss = lnn.as_ref().map(|_| stats::LossTracker::default());

        // Shared thalamic router — opened iff any network declares a
        // thalamic channel. Each network's channel lives on the network
        // itself; the router holds Hebbian weights across (source, dst)
        // pairs and is ticked once per `tick_thalamic()` call.
        let mut thalamic_router: Option<nimcp_thalamic::ThalamicRouter> = None;
        let needs_router = snn
            .as_ref()
            .is_some_and(|s| s.thalamic_channel().is_some())
            || lnn
                .as_ref()
                .is_some_and(|l| l.thalamic_channel.is_some())
            || cnn
                .as_ref()
                .is_some_and(|c| c.thalamic_channel.is_some())
            || fno
                .as_ref()
                .is_some_and(|f| f.thalamic_channel.is_some())
            || hnn
                .as_ref()
                .is_some_and(|h| h.thalamic_channel.is_some());
        if needs_router {
            let mut router = nimcp_thalamic::ThalamicRouter::new(
                nimcp_thalamic::ThalamicRouterConfig::default(),
            );
            // Helper: open a channel from any network's `ThalamicChannel`.
            let open_from = |router: &mut nimcp_thalamic::ThalamicRouter,
                             ch: &nimcp_thalamic::ThalamicChannel| {
                let dests: Vec<u32> = ch
                    .destinations
                    .iter()
                    .take(ch.n_destinations as usize)
                    .copied()
                    .collect();
                let _ = router.open_channel(ch.source_id, &dests);
            };
            if let Some(s) = snn.as_ref()
                && let Some(ch) = s.thalamic_channel()
            {
                open_from(&mut router, ch);
            }
            if let Some(l) = lnn.as_ref()
                && let Some(ch) = l.thalamic_channel.as_ref()
            {
                open_from(&mut router, ch);
            }
            if let Some(c) = cnn.as_ref()
                && let Some(ch) = c.thalamic_channel.as_ref()
            {
                open_from(&mut router, ch);
            }
            if let Some(f) = fno.as_ref()
                && let Some(ch) = f.thalamic_channel.as_ref()
            {
                open_from(&mut router, ch);
            }
            if let Some(h) = hnn.as_ref()
                && let Some(ch) = h.thalamic_channel.as_ref()
            {
                open_from(&mut router, ch);
            }
            thalamic_router = Some(router);
        }

        Ok(Self {
            config,
            scheduler,
            adaptive,
            snn,
            lnn,
            lnn_state,
            cnn,
            fno,
            hnn,
            memory,
            language,
            toxicity,
            lang_reason_in_content,
            adaptive_loss: stats::LossTracker::default(),
            lnn_loss,
            thalamic_router,
        })
    }

    // -------------------------------------------------------------------------
    // Path A Phase 3 — brain-level thalamic router.
    // -------------------------------------------------------------------------

    /// Immutable handle to the shared thalamic router, if any network
    /// has a thalamic channel open.
    pub fn thalamic_router(&self) -> Option<&nimcp_thalamic::ThalamicRouter> {
        self.thalamic_router.as_ref()
    }

    /// Tick the thalamic router: for each open source whose channel
    /// reported submits this step, bump the Hebbian weight + decay.
    ///
    /// The networks' channels hold their own `submits_this_step`
    /// counters — we forward those into the router's own `record_submit`
    /// (which flows through its per-channel counter) before calling
    /// `tick()`. This double-book-keeping keeps the router authoritative
    /// without requiring the network to know about the router.
    ///
    /// Returns the number of submits forwarded this tick (diagnostic).
    pub fn tick_thalamic(&mut self) -> u32 {
        let Some(router) = self.thalamic_router.as_mut() else {
            return 0;
        };
        let mut forwarded = 0_u32;

        if let Some(snn) = self.snn.as_mut()
            && let Some(ch) = snn.thalamic_channel_mut()
        {
            let count = ch.submits_this_step;
            for _ in 0..count {
                if router.record_submit(ch.source_id) {
                    forwarded = forwarded.saturating_add(1);
                }
            }
            // Network-side counter is cleared by the network's own tick;
            // we don't mutate it here, just forward observations.
        }
        if let Some(lnn) = self.lnn.as_mut()
            && let Some(ch) = lnn.thalamic_channel.as_mut()
        {
            let count = ch.submits_this_step;
            for _ in 0..count {
                if router.record_submit(ch.source_id) {
                    forwarded = forwarded.saturating_add(1);
                }
            }
        }
        if let Some(cnn) = self.cnn.as_mut()
            && let Some(ch) = cnn.thalamic_channel.as_mut()
        {
            let count = ch.submits_this_step;
            for _ in 0..count {
                if router.record_submit(ch.source_id) {
                    forwarded = forwarded.saturating_add(1);
                }
            }
        }
        if let Some(fno) = self.fno.as_mut()
            && let Some(ch) = fno.thalamic_channel.as_mut()
        {
            let count = ch.submits_this_step;
            for _ in 0..count {
                if router.record_submit(ch.source_id) {
                    forwarded = forwarded.saturating_add(1);
                }
            }
        }
        if let Some(hnn) = self.hnn.as_mut()
            && let Some(ch) = hnn.thalamic_channel.as_mut()
        {
            let count = ch.submits_this_step;
            for _ in 0..count {
                if router.record_submit(ch.source_id) {
                    forwarded = forwarded.saturating_add(1);
                }
            }
        }

        router.tick();
        forwarded
    }

    /// Accessor for the config.
    pub fn config(&self) -> &BrainConfig {
        &self.config
    }

    /// One training step against an MSE target. Returns pre-update loss.
    ///
    /// # Panics
    /// Panics if `features.len()` or `target.len()` don't match the first
    /// or last configured layer width. Callers that can't guarantee shape
    /// should validate before calling — the bindings layer does this.
    pub fn learn(&mut self, features: &Array1<f32>, target: &Array1<f32>, lr: f32) -> f32 {
        let loss = self.adaptive.learn(features, target, lr);
        self.adaptive_loss.observe(loss);
        loss
    }

    /// Forward pass. Returns the brain's output vector.
    pub fn predict(&self, features: &Array1<f32>) -> Array1<f32> {
        self.adaptive.forward(features)
    }

    /// Persist the brain's weights to `path`. Phase 1 only saves the
    /// adaptive net; later phases extend via CheckpointCoordinator.
    pub fn save<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let bytes = self.adaptive.save().map_err(adaptive_to_core_err)?;
        std::fs::write(path.as_ref(), bytes).map_err(Error::from)
    }

    /// Reload weights from a previous [`Brain::save`]. Shape is inferred
    /// from disk; the config's `layers` must match (shape-mismatched
    /// loads are a `Error::Config`).
    pub fn load<P: AsRef<Path>>(&mut self, path: P) -> Result<()> {
        let bytes = std::fs::read(path.as_ref()).map_err(Error::from)?;
        self.adaptive.load(&bytes).map_err(adaptive_to_core_err)?;
        Ok(())
    }

    // -------------------------------------------------------------------------
    // SNN access — all return `Error::Config` if the brain was constructed
    // without an SNN config.
    // -------------------------------------------------------------------------

    /// Immutable handle to the SNN, if present.
    pub fn snn(&self) -> Option<&SnnNetwork> {
        self.snn.as_ref()
    }

    /// Mutable handle to the SNN, if present.
    pub fn snn_mut(&mut self) -> Option<&mut SnnNetwork> {
        self.snn.as_mut()
    }

    /// One SNN integration step. See [`SnnNetwork::step`].
    pub fn snn_step(&mut self, external_i_syn: &[&[f32]], reward: f32, dt_ms: f32) -> Result<u32> {
        let snn = self
            .snn
            .as_mut()
            .ok_or_else(|| Error::Config("snn not configured on this brain".into()))?;
        Ok(snn.step(external_i_syn, reward, dt_ms))
    }

    // -------------------------------------------------------------------------
    // LNN access — same pattern as SNN.
    // -------------------------------------------------------------------------

    /// Immutable handle to the LNN, if present.
    pub fn lnn(&self) -> Option<&LnnNetwork> {
        self.lnn.as_ref()
    }

    /// Reset transient LNN state to zeros. No-op if no LNN.
    pub fn lnn_reset(&mut self) {
        if let (Some(net), Some(state)) = (self.lnn.as_ref(), self.lnn_state.as_mut()) {
            *state = net.new_state();
        }
    }

    /// Step the LNN forward one sample; returns the readout. Carries
    /// state across calls; use [`lnn_reset`] to start a new sequence.
    pub fn lnn_forward_step(&mut self, input: &Array1<f32>) -> Result<Array1<f32>> {
        let (Some(net), Some(state)) = (self.lnn.as_ref(), self.lnn_state.as_mut()) else {
            return Err(Error::Config("lnn not configured on this brain".into()));
        };
        Ok(net.forward_step(state, input))
    }

    /// Run the LNN over an entire sequence (resets state at the start).
    /// Returns per-step readouts.
    pub fn lnn_forward_sequence(&mut self, inputs: &[Array1<f32>]) -> Result<Vec<Array1<f32>>> {
        let (Some(net), Some(state)) = (self.lnn.as_ref(), self.lnn_state.as_mut()) else {
            return Err(Error::Config("lnn not configured on this brain".into()));
        };
        *state = net.new_state();
        let mut out = Vec::with_capacity(inputs.len());
        for u in inputs {
            out.push(net.forward_step(state, u));
        }
        Ok(out)
    }

    /// One LNN training step (MSE over sequence) with the supplied hyperparams.
    /// Returns `(loss, grad_norm)`.
    pub fn lnn_train_step_mse(
        &mut self,
        inputs: &[Array1<f32>],
        targets: &[Array1<f32>],
        params: &TrainParams,
    ) -> Result<(f32, f32)> {
        let lnn = self
            .lnn
            .as_mut()
            .ok_or_else(|| Error::Config("lnn not configured on this brain".into()))?;
        let (loss, grad_norm) = nimcp_lnn::train_step_mse(lnn, inputs, targets, params);
        if let Some(tracker) = self.lnn_loss.as_mut() {
            tracker.observe(loss);
        }
        Ok((loss, grad_norm))
    }

    // -------------------------------------------------------------------------
    // Phase 11a — CNN access.
    // -------------------------------------------------------------------------

    /// Immutable handle to the CNN, if present.
    pub fn cnn(&self) -> Option<&CnnNetwork> {
        self.cnn.as_ref()
    }

    /// CNN forward over a 4-D `[batch, in_channels, H, W]` input.
    /// Returns `[batch, output_dim]` (the network's last linear width).
    pub fn cnn_predict(&self, input: &Array4<f32>) -> Result<ndarray::Array2<f32>> {
        let net = self
            .cnn
            .as_ref()
            .ok_or_else(|| Error::Config("cnn not configured on this brain".into()))?;
        Ok(net.forward(input))
    }

    /// One CNN training step (MSE) — `(loss, grad_norm)`. Phase 11a-train.
    pub fn cnn_train_step_mse(
        &mut self,
        input: &Array4<f32>,
        target: &ndarray::Array2<f32>,
        lr: f32,
    ) -> Result<(f32, f32)> {
        let net = self
            .cnn
            .as_mut()
            .ok_or_else(|| Error::Config("cnn not configured on this brain".into()))?;
        Ok(nimcp_cnn::train_step_mse(net, input, target, lr))
    }

    // -------------------------------------------------------------------------
    // Phase 11b — FNO access.
    // -------------------------------------------------------------------------

    /// Immutable handle to the FNO, if present.
    pub fn fno(&self) -> Option<&FnoNetwork> {
        self.fno.as_ref()
    }

    /// FNO forward over a 3-D `[batch, in_channels, length]` input.
    /// Returns `[batch, out_channels, length]`.
    pub fn fno_predict(&self, input: &Array3<f32>) -> Result<Array3<f32>> {
        let net = self
            .fno
            .as_ref()
            .ok_or_else(|| Error::Config("fno not configured on this brain".into()))?;
        Ok(net.forward(input))
    }

    /// One FNO training step (MSE) — `(loss, grad_norm)`. Phase 11b-train.
    pub fn fno_train_step_mse(
        &mut self,
        input: &Array3<f32>,
        target: &Array3<f32>,
        lr: f32,
    ) -> Result<(f32, f32)> {
        let net = self
            .fno
            .as_mut()
            .ok_or_else(|| Error::Config("fno not configured on this brain".into()))?;
        Ok(nimcp_fno::train_step_mse(net, input, target, lr))
    }

    // -------------------------------------------------------------------------
    // Phase 11c — HNN access.
    // -------------------------------------------------------------------------

    /// Immutable handle to the HNN, if present.
    pub fn hnn(&self) -> Option<&HnnNetwork> {
        self.hnn.as_ref()
    }

    /// Mutable handle to the HNN, if present.
    pub fn hnn_mut(&mut self) -> Option<&mut HnnNetwork> {
        self.hnn.as_mut()
    }

    /// Set the HNN's `(q, p)` state. Lengths must equal `dof`.
    pub fn hnn_set_state(&mut self, q: Array1<f32>, p: Array1<f32>) -> Result<()> {
        let net = self
            .hnn
            .as_mut()
            .ok_or_else(|| Error::Config("hnn not configured on this brain".into()))?;
        net.set_state(q, p);
        Ok(())
    }

    /// Advance the HNN one symplectic Euler step. Returns the
    /// Hamiltonian value at the *start* of the step.
    pub fn hnn_step(&mut self) -> Result<f32> {
        let net = self
            .hnn
            .as_mut()
            .ok_or_else(|| Error::Config("hnn not configured on this brain".into()))?;
        Ok(net.step())
    }

    /// Current HNN Hamiltonian value (pure forward, no state mutation).
    pub fn hnn_energy(&self) -> Result<f32> {
        let net = self
            .hnn
            .as_ref()
            .ok_or_else(|| Error::Config("hnn not configured on this brain".into()))?;
        Ok(net.energy())
    }

    // -------------------------------------------------------------------------
    // Phase L8 — grounded language + content safety.
    // -------------------------------------------------------------------------

    /// Immutable handle to the grounded-language engine, if present.
    pub fn language(&self) -> Option<&GroundedLanguage> {
        self.language.as_ref()
    }

    /// Mutable handle to the grounded-language engine, if present.
    pub fn language_mut(&mut self) -> Option<&mut GroundedLanguage> {
        self.language.as_mut()
    }

    /// Whether the content-safety stack is wired in.
    #[must_use]
    pub fn has_toxicity(&self) -> bool {
        self.toxicity.is_some()
    }

    /// Learn from a text span (distributional + n-gram learning). This is
    /// **mark-not-filter**: toxic training text is logged + the toxicity
    /// ML head is nudged toward the pattern verdict, but the text is still
    /// learned (V1 never silently drops training data).
    pub fn language_learn(&mut self, text: &str) -> Result<()> {
        let toxic = self.toxicity.as_ref().map(|t| t.classify(text));
        if let Some(t) = self.toxicity.as_mut() {
            // Nudge the ML head toward the pattern teacher (online).
            t.train_ml_from_pattern(text, 0.05, 0.02);
        }
        if let Some(tr) = &toxic {
            if tr.would_block {
                tracing::warn!(
                    category = %tr.matched_category,
                    harm = tr.predicted_harm,
                    "language_learn: toxic training text (marked, not filtered)"
                );
            }
        }
        let gl = self
            .language
            .as_mut()
            .ok_or_else(|| Error::Config("language not configured on this brain".into()))?;
        gl.learn_from_text(text);
        Ok(())
    }

    /// Comprehend a text span → `(semantic_vector, confidence)`.
    pub fn language_comprehend(&mut self, text: &str) -> Result<(Array1<f32>, f32)> {
        let gl = self
            .language
            .as_mut()
            .ok_or_else(|| Error::Config("language not configured on this brain".into()))?;
        let r = gl.comprehend(text);
        Ok((Array1::from(r.semantic_vector), r.comprehension_confidence))
    }

    /// Respond to an input prompt. The content-safety gate is the
    /// **first** thing that runs — above the cascade — so it cannot be
    /// bypassed by the production path (the V1 cascade-bypass lesson).
    ///
    /// 1. If the toxicity stack flags the *input*, return a stage-graded
    ///    counterclaim (no production).
    /// 2. Otherwise run the cascade.
    /// 3. Defense-in-depth: if the *output* flags, scale confidence by
    ///    `(1 − harm)` but never modify the text (mark-not-filter).
    pub fn language_respond(&mut self, input: &str) -> Result<LanguageResponse> {
        let cfg = self
            .config
            .language
            .clone()
            .ok_or_else(|| Error::Config("language not configured on this brain".into()))?;
        let stage = cfg.default_stage;

        // (1) MUST-RUN input gate — above the cascade.
        if let Some(tox) = self.toxicity.as_ref() {
            let verdict = tox.classify(input);
            if verdict.would_block {
                let cc = tox.counterclaim(input, &verdict.matched_category, i32::from(stage as u16));
                return Ok(LanguageResponse {
                    text: cc.text,
                    confidence: 1.0 - verdict.predicted_harm,
                    blocked_for_toxicity: true,
                    toxicity_category: verdict.matched_category,
                });
            }
        }

        // (2) Cascade production. V2 has no working-memory / imagination /
        // reasoning subsystems yet, so no `ContentSources` are supplied —
        // the cascade applies native discourse continuity only. The
        // `reason_in_content` flag is threaded for parity (it gates the
        // reasoning blend once a source exists).
        let cascade_cfg = CascadeConfig {
            stage,
            min_produce_words: cfg.min_produce_words,
            recurrent_max_iters: cfg.recurrent_max_iters,
            reason_in_content: self.lang_reason_in_content,
        };
        let gl = self
            .language
            .as_mut()
            .ok_or_else(|| Error::Config("language not configured on this brain".into()))?;
        let resp = gl.respond(input, &cascade_cfg);

        // (3) Defense-in-depth output gate — mark, don't filter.
        let mut confidence = resp.confidence;
        if let Some(tox) = self.toxicity.as_ref() {
            let out_verdict = tox.classify(&resp.text);
            if out_verdict.would_block {
                tracing::warn!(
                    category = %out_verdict.matched_category,
                    "language_respond: produced text flagged — confidence scaled, text kept"
                );
                confidence *= 1.0 - out_verdict.predicted_harm;
            }
        }
        Ok(LanguageResponse {
            text: resp.text,
            confidence,
            blocked_for_toxicity: false,
            toxicity_category: String::new(),
        })
    }

    /// Classify text for toxicity (diagnostic). Returns
    /// `(predicted_harm, fairness_violation, would_block)`.
    pub fn classify_toxicity(&self, text: &str) -> Result<(f32, f32, bool)> {
        let tox = self
            .toxicity
            .as_ref()
            .ok_or_else(|| Error::Config("toxicity stack not configured on this brain".into()))?;
        let r = tox.classify(text);
        Ok((r.predicted_harm, r.fairness_violation, r.would_block))
    }

    /// Runtime toggle for the cascade reasoning-blend (V1
    /// `nimcp_brain_set_reason_in_content` RPC parity). Dormant until a
    /// reasoning subsystem supplies a conclusion vector.
    pub fn set_reason_in_content(&mut self, on: bool) {
        self.lang_reason_in_content = on;
    }

    /// Current `reason_in_content` toggle.
    #[must_use]
    pub fn reason_in_content(&self) -> bool {
        self.lang_reason_in_content
    }

    // -------------------------------------------------------------------------
    // Memory (Z-Ladder) access.
    // -------------------------------------------------------------------------

    /// Immutable handle to the Z-Ladder, if present.
    pub fn memory(&self) -> Option<&ZLadder> {
        self.memory.as_ref()
    }

    /// Mutable handle to the Z-Ladder, if present.
    pub fn memory_mut(&mut self) -> Option<&mut ZLadder> {
        self.memory.as_mut()
    }

    /// Insert a new memory node. Fails with `Error::Config` if no
    /// memory subsystem was configured or if the underlying ladder
    /// rejects (e.g. duplicate ID).
    pub fn memory_insert(&mut self, node: MemoryNode) -> Result<()> {
        let mem = self
            .memory
            .as_mut()
            .ok_or_else(|| Error::Config("memory not configured on this brain".into()))?;
        mem.insert(node)
            .map_err(|e| Error::Config(format!("memory insert: {e}")))
    }

    /// Mark a node as a landmark — elevate to Z3 + protect from demotion.
    pub fn memory_mark_landmark(&mut self, id: u64, reason: &str) -> Result<()> {
        let mem = self
            .memory
            .as_mut()
            .ok_or_else(|| Error::Config("memory not configured on this brain".into()))?;
        mem.mark_landmark(id, reason)
            .map_err(|e| Error::Config(format!("mark_landmark: {e}")))
    }

    /// Query all tiers for the top-`k` cosine matches to `query`.
    pub fn memory_query_all(&self, query: &[f32], k: usize) -> Result<Vec<QueryHit>> {
        let mem = self
            .memory
            .as_ref()
            .ok_or_else(|| Error::Config("memory not configured on this brain".into()))?;
        Ok(mem.query_all_tiers(query, k))
    }

    /// Query the landmark subset for the top-`k` cosine matches to `query`.
    pub fn memory_query_landmarks(&self, query: &[f32], k: usize) -> Result<Vec<QueryHit>> {
        let mem = self
            .memory
            .as_ref()
            .ok_or_else(|| Error::Config("memory not configured on this brain".into()))?;
        Ok(mem.query_landmarks_by_similarity(query, k))
    }

    // -------------------------------------------------------------------------
    // Phase 6 — introspection.
    // -------------------------------------------------------------------------

    /// Collect a read-only stats snapshot across every configured
    /// subsystem. See [`stats::BrainStats`] for the full schema.
    ///
    /// Cheap to call — linear in the total parameter / node count —
    /// but not free; callers polling on a tight loop should throttle.
    #[must_use]
    pub fn stats(&self) -> stats::BrainStats {
        stats::BrainStats {
            rng_seed: self.config.rng_seed,
            adaptive: Some(stats::collect_adaptive(&self.adaptive)),
            snn: self.snn.as_ref().map(stats::collect_snn),
            lnn: self
                .lnn
                .as_ref()
                .map(|net| stats::collect_lnn(net, self.lnn_state.as_deref())),
            memory: self.memory.as_ref().map(stats::collect_memory),
            loss: stats::LossStats {
                adaptive: Some(self.adaptive_loss),
                lnn: self.lnn_loss,
            },
        }
    }

    /// Convenience: [`Brain::stats`] encoded as a JSON string. Backs
    /// the Python `brain.stats()` binding (which decodes into a dict
    /// on the Python side).
    pub fn stats_json(&self) -> Result<String> {
        serde_json::to_string(&self.stats())
            .map_err(|e| Error::Serialization(format!("stats_json: {e}")))
    }

    // -------------------------------------------------------------------------
    // Joint atomic ensemble checkpoint.
    // -------------------------------------------------------------------------

    /// Save every configured network into `dir`, atomically. Writes:
    ///
    /// - `adaptive.rkyv` — MLP weights (always present)
    /// - `snn.json` — SNN weight snapshot (only if SNN configured)
    /// - `lnn.json` — LNN full network (only if LNN configured)
    /// - `manifest.json` — which subfiles are present, plus a format version
    ///
    /// Atomicity is achieved by writing to `<dir>.tmp/` first and then
    /// `rename(<dir>.tmp, <dir>)` — the old ensemble stays intact if any
    /// subfile write fails.
    pub fn save_ensemble<P: AsRef<Path>>(&self, dir: P) -> Result<()> {
        let final_dir = dir.as_ref().to_path_buf();
        let tmp_dir = {
            let mut d = final_dir.clone();
            let file_name = d.file_name().ok_or_else(|| {
                Error::Config("save_ensemble: target must have a filename component".into())
            })?;
            let mut tmp_name = file_name.to_owned();
            tmp_name.push(".tmp");
            d.set_file_name(tmp_name);
            d
        };

        // Nuke any leftover `tmp_dir` from a crashed prior save.
        if tmp_dir.exists() {
            std::fs::remove_dir_all(&tmp_dir).map_err(Error::from)?;
        }
        std::fs::create_dir_all(&tmp_dir).map_err(Error::from)?;

        // Adaptive.
        let adaptive_bytes = self.adaptive.save().map_err(adaptive_to_core_err)?;
        std::fs::write(tmp_dir.join("adaptive.rkyv"), &adaptive_bytes).map_err(Error::from)?;

        let mut manifest = EnsembleManifest::default();
        manifest.files.push("adaptive.rkyv".into());

        // SNN.
        if let Some(snn) = &self.snn {
            let snap = snn.snapshot();
            let bytes = serde_json::to_vec(&snap)
                .map_err(|e| Error::Serialization(format!("snn snapshot: {e}")))?;
            std::fs::write(tmp_dir.join("snn.json"), bytes).map_err(Error::from)?;
            manifest.files.push("snn.json".into());
        }

        // LNN — serialize the whole network (weights + hyperparams).
        if let Some(lnn) = &self.lnn {
            let bytes = serde_json::to_vec(lnn)
                .map_err(|e| Error::Serialization(format!("lnn serialize: {e}")))?;
            std::fs::write(tmp_dir.join("lnn.json"), bytes).map_err(Error::from)?;
            manifest.files.push("lnn.json".into());
        }

        // CNN / FNO / HNN — same pattern (full network round-trip).
        if let Some(cnn) = &self.cnn {
            let bytes = serde_json::to_vec(cnn)
                .map_err(|e| Error::Serialization(format!("cnn serialize: {e}")))?;
            std::fs::write(tmp_dir.join("cnn.json"), bytes).map_err(Error::from)?;
            manifest.files.push("cnn.json".into());
        }
        if let Some(fno) = &self.fno {
            let bytes = serde_json::to_vec(fno)
                .map_err(|e| Error::Serialization(format!("fno serialize: {e}")))?;
            std::fs::write(tmp_dir.join("fno.json"), bytes).map_err(Error::from)?;
            manifest.files.push("fno.json".into());
        }
        if let Some(hnn) = &self.hnn {
            let bytes = serde_json::to_vec(hnn)
                .map_err(|e| Error::Serialization(format!("hnn serialize: {e}")))?;
            std::fs::write(tmp_dir.join("hnn.json"), bytes).map_err(Error::from)?;
            manifest.files.push("hnn.json".into());
        }

        // Memory — full ZLadder snapshot (tiers + features + landmarks +
        // clock + stats). V1 E6's "restore preserves features" rule is
        // enforced by `MemoryNode` carrying `Vec<f32>` through serde.
        if let Some(mem) = &self.memory {
            let bytes = serde_json::to_vec(mem)
                .map_err(|e| Error::Serialization(format!("memory serialize: {e}")))?;
            std::fs::write(tmp_dir.join("memory.json"), bytes).map_err(Error::from)?;
            manifest.files.push("memory.json".into());
        }

        // Language — the whole grounded-language engine via its single
        // canonical format (lexicon + embeddings + n-grams + discourse).
        if let Some(gl) = &self.language {
            let json = gl
                .to_json()
                .map_err(|e| Error::Serialization(format!("language serialize: {e}")))?;
            std::fs::write(tmp_dir.join("language.json"), json.as_bytes()).map_err(Error::from)?;
            manifest.files.push("language.json".into());
        }
        // Toxicity — only the ML weights are learned state (the regex
        // rules + counterclaim templates rebuild from bundled data).
        if let Some(tox) = &self.toxicity {
            let json = tox
                .ml_to_json()
                .map_err(|e| Error::Serialization(format!("toxicity ml serialize: {e}")))?;
            std::fs::write(tmp_dir.join("toxicity_ml.json"), json.as_bytes()).map_err(Error::from)?;
            manifest.files.push("toxicity_ml.json".into());
        }

        // Manifest — last so its presence signals "this dir is complete".
        let manifest_bytes = serde_json::to_vec_pretty(&manifest)
            .map_err(|e| Error::Serialization(format!("manifest: {e}")))?;
        std::fs::write(tmp_dir.join("manifest.json"), manifest_bytes).map_err(Error::from)?;

        // Atomic swap: remove any old dir, rename tmp into place.
        if final_dir.exists() {
            std::fs::remove_dir_all(&final_dir).map_err(Error::from)?;
        }
        std::fs::rename(&tmp_dir, &final_dir).map_err(Error::from)?;

        tracing::info!(dir = ?final_dir, files = ?manifest.files, "ensemble saved");
        Ok(())
    }

    /// Restore from a directory produced by [`save_ensemble`]. Every
    /// configured network is restored; subfiles missing from the
    /// directory leave the corresponding network unchanged.
    pub fn load_ensemble<P: AsRef<Path>>(&mut self, dir: P) -> Result<()> {
        let dir = dir.as_ref();
        if !dir.is_dir() {
            return Err(Error::Config(format!(
                "load_ensemble: {dir:?} is not a directory"
            )));
        }
        let manifest_bytes = std::fs::read(dir.join("manifest.json")).map_err(Error::from)?;
        let manifest: EnsembleManifest = serde_json::from_slice(&manifest_bytes)
            .map_err(|e| Error::Serialization(format!("manifest decode: {e}")))?;
        if manifest.version != EnsembleManifest::default().version {
            return Err(Error::Config(format!(
                "manifest version {} unsupported (expected {})",
                manifest.version,
                EnsembleManifest::default().version
            )));
        }

        // Adaptive.
        if manifest.files.iter().any(|f| f == "adaptive.rkyv") {
            let bytes = std::fs::read(dir.join("adaptive.rkyv")).map_err(Error::from)?;
            self.adaptive.load(&bytes).map_err(adaptive_to_core_err)?;
        }

        // SNN — apply WeightSnapshot via `restore`. Returns `false` on
        // shape mismatch, which we surface as `Error::Config`.
        if manifest.files.iter().any(|f| f == "snn.json") {
            let snn = self.snn.as_mut().ok_or_else(|| {
                Error::Config("snapshot has snn.json but brain was built without snn".into())
            })?;
            let bytes = std::fs::read(dir.join("snn.json")).map_err(Error::from)?;
            let snap: nimcp_snn::network::WeightSnapshot = serde_json::from_slice(&bytes)
                .map_err(|e| Error::Serialization(format!("snn snapshot decode: {e}")))?;
            if !snn.restore(&snap) {
                return Err(Error::Config(
                    "snn snapshot shape does not match current brain".into(),
                ));
            }
        }

        // LNN — replace whole network. Verify the shape matches the
        // brain's current LNN config before overwriting.
        if manifest.files.iter().any(|f| f == "lnn.json") {
            let lnn_slot = self.lnn.as_mut().ok_or_else(|| {
                Error::Config("snapshot has lnn.json but brain was built without lnn".into())
            })?;
            let bytes = std::fs::read(dir.join("lnn.json")).map_err(Error::from)?;
            let restored: LnnNetwork = serde_json::from_slice(&bytes)
                .map_err(|e| Error::Serialization(format!("lnn decode: {e}")))?;
            if restored.input_dim != lnn_slot.input_dim
                || restored.output_dim != lnn_slot.output_dim
                || restored.layers.len() != lnn_slot.layers.len()
            {
                return Err(Error::Config(
                    "lnn snapshot shape does not match current brain".into(),
                ));
            }
            *lnn_slot = restored;
            // Reset transient state to fresh zeros so the restored
            // network starts with a clean runtime state.
            if let Some(state) = self.lnn_state.as_mut() {
                *state = lnn_slot.new_state();
            }
        }

        // CNN — replace whole network. Verify shape against current.
        if manifest.files.iter().any(|f| f == "cnn.json") {
            let slot = self.cnn.as_mut().ok_or_else(|| {
                Error::Config("snapshot has cnn.json but brain was built without cnn".into())
            })?;
            let bytes = std::fs::read(dir.join("cnn.json")).map_err(Error::from)?;
            let restored: CnnNetwork = serde_json::from_slice(&bytes)
                .map_err(|e| Error::Serialization(format!("cnn decode: {e}")))?;
            if restored.input_shape != slot.input_shape
                || restored.output_dim != slot.output_dim
                || restored.layers.len() != slot.layers.len()
            {
                return Err(Error::Config(
                    "cnn snapshot shape does not match current brain".into(),
                ));
            }
            *slot = restored;
        }
        // FNO — same pattern.
        if manifest.files.iter().any(|f| f == "fno.json") {
            let slot = self.fno.as_mut().ok_or_else(|| {
                Error::Config("snapshot has fno.json but brain was built without fno".into())
            })?;
            let bytes = std::fs::read(dir.join("fno.json")).map_err(Error::from)?;
            let restored: FnoNetwork = serde_json::from_slice(&bytes)
                .map_err(|e| Error::Serialization(format!("fno decode: {e}")))?;
            if restored.in_channels != slot.in_channels
                || restored.out_channels != slot.out_channels
                || restored.hidden_channels != slot.hidden_channels
                || restored.modes != slot.modes
                || restored.blocks.len() != slot.blocks.len()
            {
                return Err(Error::Config(
                    "fno snapshot shape does not match current brain".into(),
                ));
            }
            *slot = restored;
        }
        // HNN — replace whole network including (q, p) state.
        if manifest.files.iter().any(|f| f == "hnn.json") {
            let slot = self.hnn.as_mut().ok_or_else(|| {
                Error::Config("snapshot has hnn.json but brain was built without hnn".into())
            })?;
            let bytes = std::fs::read(dir.join("hnn.json")).map_err(Error::from)?;
            let restored: HnnNetwork = serde_json::from_slice(&bytes)
                .map_err(|e| Error::Serialization(format!("hnn decode: {e}")))?;
            if restored.config.dof != slot.config.dof
                || restored.mlp.weights.len() != slot.mlp.weights.len()
            {
                return Err(Error::Config(
                    "hnn snapshot shape does not match current brain".into(),
                ));
            }
            *slot = restored;
        }

        // Memory — full ZLadder replace (including tiers, landmarks,
        // clock, stats). The Z-Ladder doesn't own neural weights, so a
        // shape-mismatch check means "do the currently-configured
        // tier counts + max_landmarks line up with what's on disk".
        if manifest.files.iter().any(|f| f == "memory.json") {
            let mem_slot = self.memory.as_mut().ok_or_else(|| {
                Error::Config("snapshot has memory.json but brain was built without memory".into())
            })?;
            let bytes = std::fs::read(dir.join("memory.json")).map_err(Error::from)?;
            let restored: ZLadder = serde_json::from_slice(&bytes)
                .map_err(|e| Error::Serialization(format!("memory decode: {e}")))?;
            // Config keys we consider structural: per-tier capacity + max_landmarks.
            let same_caps = (0..4).all(|i| {
                restored.config().tiers[i].capacity == mem_slot.config().tiers[i].capacity
            });
            let same_max_landmarks =
                restored.config().max_landmarks == mem_slot.config().max_landmarks;
            if !same_caps || !same_max_landmarks {
                return Err(Error::Config(
                    "memory snapshot capacities do not match current brain config".into(),
                ));
            }
            *mem_slot = restored;
        }

        // Language — restore the whole engine (validates magic/version +
        // rebuilds the lexicon index inside from_json).
        if manifest.files.iter().any(|f| f == "language.json") {
            let slot = self.language.as_mut().ok_or_else(|| {
                Error::Config("snapshot has language.json but brain was built without language".into())
            })?;
            let json = std::fs::read_to_string(dir.join("language.json")).map_err(Error::from)?;
            *slot = GroundedLanguage::from_json(&json)
                .map_err(|e| Error::Serialization(format!("language decode: {e}")))?;
        }
        // Toxicity — restore only the ML weights onto the rebuilt stack.
        if manifest.files.iter().any(|f| f == "toxicity_ml.json") {
            let slot = self.toxicity.as_mut().ok_or_else(|| {
                Error::Config(
                    "snapshot has toxicity_ml.json but brain was built without toxicity".into(),
                )
            })?;
            let json = std::fs::read_to_string(dir.join("toxicity_ml.json")).map_err(Error::from)?;
            slot.restore_ml_json(&json)
                .map_err(|e| Error::Serialization(format!("toxicity ml decode: {e}")))?;
        }

        tracing::info!(dir = ?dir, files = ?manifest.files, "ensemble loaded");
        Ok(())
    }
}

/// Manifest describing which subfiles are in an ensemble checkpoint dir.
#[derive(Debug, Clone, Serialize, Deserialize)]
struct EnsembleManifest {
    /// Manifest schema version — bump on breaking layout change.
    version: u32,
    /// Subfile names present in this snapshot.
    files: Vec<String>,
}

impl Default for EnsembleManifest {
    fn default() -> Self {
        Self {
            version: 1,
            files: Vec::new(),
        }
    }
}

fn adaptive_to_core_err(e: AdaptiveError) -> Error {
    match e {
        AdaptiveError::ShapeMismatch { expected, got } => Error::Config(format!(
            "adaptive shape mismatch: expected {expected}, got {got}"
        )),
        AdaptiveError::Serialization(msg) => Error::Serialization(msg),
        AdaptiveError::Checkpoint(msg) => Error::Config(format!("checkpoint: {msg}")),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ndarray::Array1;

    #[tokio::test]
    async fn brain_boots_with_default_config() {
        let brain = Brain::new(BrainConfig::default()).unwrap();
        assert_eq!(brain.config().rng_seed, 0x5EED);
        // Phase 9h — default backend is CPU.
        assert_eq!(brain.config().backend, Backend::Cpu);
    }

    /// Phase 9h — Backend::Gpu boots even on a CPU-only build by
    /// degrading to CPU forward path with a warning. The brain handle
    /// is fully functional for inference + learning afterward.
    #[tokio::test]
    async fn backend_gpu_boots_on_cpu_only_build() {
        let cfg = BrainConfig {
            backend: Backend::Gpu,
            ..Default::default()
        };
        let brain = Brain::new(cfg).expect("Backend::Gpu must degrade gracefully on CPU build");
        assert_eq!(brain.config().backend, Backend::Gpu);
    }

    /// Backend round-trips through serde — the deployed config can
    /// pin the backend in JSON / YAML and the brain honors it on load.
    #[test]
    fn backend_serde_round_trip() {
        let cpu = serde_json::to_string(&Backend::Cpu).unwrap();
        let gpu = serde_json::to_string(&Backend::Gpu).unwrap();
        assert_eq!(cpu, "\"cpu\"");
        assert_eq!(gpu, "\"gpu\"");
        let back_cpu: Backend = serde_json::from_str(&cpu).unwrap();
        let back_gpu: Backend = serde_json::from_str(&gpu).unwrap();
        assert_eq!(back_cpu, Backend::Cpu);
        assert_eq!(back_gpu, Backend::Gpu);
    }

    /// Old configs (pre-9h, no `backend` field) deserialize with the
    /// default Cpu — backwards compat for existing on-disk configs.
    #[test]
    fn backend_default_via_serde_when_field_absent() {
        let raw = r#"{
            "rng_seed": 42,
            "deterministic": true,
            "state_dir": "./state",
            "adaptive": {
                "layers": [4, 8, 1],
                "rng_seed": 42,
                "activation": "Tanh"
            }
        }"#;
        let cfg: BrainConfig = serde_json::from_str(raw).expect("legacy config must parse");
        assert_eq!(cfg.backend, Backend::Cpu);
    }

    /// End-to-end XOR inside the Brain API — this is the Phase 1 exit
    /// criterion: 100-neuron toy brain trains on XOR in <5s.
    #[tokio::test]
    async fn brain_trains_xor_end_to_end() {
        let cfg = BrainConfig {
            rng_seed: 0x42,
            deterministic: true,
            adaptive: AdaptiveConfig {
                layers: vec![2, 16, 1],
                rng_seed: 0x42,
                activation: nimcp_adaptive::Activation::Tanh,
            },
            ..Default::default()
        };
        let mut brain = Brain::new(cfg).unwrap();

        let samples: [(Array1<f32>, Array1<f32>); 4] = [
            (
                Array1::from_vec(vec![0.0, 0.0]),
                Array1::from_vec(vec![0.0]),
            ),
            (
                Array1::from_vec(vec![0.0, 1.0]),
                Array1::from_vec(vec![1.0]),
            ),
            (
                Array1::from_vec(vec![1.0, 0.0]),
                Array1::from_vec(vec![1.0]),
            ),
            (
                Array1::from_vec(vec![1.0, 1.0]),
                Array1::from_vec(vec![0.0]),
            ),
        ];

        let start = std::time::Instant::now();
        let mut final_loss = f32::INFINITY;
        for _step in 0..5000 {
            let mut mean = 0.0;
            for (x, y) in &samples {
                mean += brain.learn(x, y, 0.1);
            }
            final_loss = mean / 4.0;
            if final_loss < 0.05 {
                break;
            }
        }
        let elapsed = start.elapsed();
        assert!(
            final_loss < 0.05,
            "XOR didn't converge: final_loss={final_loss}"
        );
        assert!(elapsed.as_secs() < 5, "XOR took too long: {:?}", elapsed);

        // Prediction sanity: (1, 0) should be close to 1; (1, 1) close to 0.
        let p10 = brain.predict(&Array1::from_vec(vec![1.0, 0.0]))[0];
        let p11 = brain.predict(&Array1::from_vec(vec![1.0, 1.0]))[0];
        assert!(p10 > 0.5, "predict(1,0)={p10}, expected >0.5");
        assert!(p11 < 0.5, "predict(1,1)={p11}, expected <0.5");
    }

    #[tokio::test]
    async fn save_load_round_trip() {
        let cfg = BrainConfig {
            adaptive: AdaptiveConfig {
                layers: vec![3, 5, 2],
                rng_seed: 7,
                activation: nimcp_adaptive::Activation::Relu,
            },
            ..Default::default()
        };
        let mut a = Brain::new(cfg.clone()).unwrap();
        let x = Array1::from_vec(vec![1.0, -0.5, 0.25]);

        // Train for a few steps so a != b initially.
        for _ in 0..20 {
            a.learn(&x, &Array1::from_vec(vec![0.0, 1.0]), 0.05);
        }
        let y_a = a.predict(&x);

        let tmp = tempfile::NamedTempFile::new().unwrap();
        a.save(tmp.path()).unwrap();

        let mut b = Brain::new(cfg).unwrap();
        b.load(tmp.path()).unwrap();
        let y_b = b.predict(&x);

        for (pa, pb) in y_a.iter().zip(y_b.iter()) {
            assert!((pa - pb).abs() < 1e-6, "save/load drift: {pa} vs {pb}");
        }
    }

    // -------------------------------------------------------------------------
    // Phase 4c tests — 3-network ensemble.
    // -------------------------------------------------------------------------

    fn ensemble_config(seed: u64) -> BrainConfig {
        use nimcp_lnn::LtcParams;
        use nimcp_plasticity::HomeostaticParams;
        use nimcp_snn::network::{EdgeSpec, PopulationSpec};
        use nimcp_snn::{LifParams, RstdpParams};

        let adaptive = AdaptiveConfig {
            layers: vec![4, 8, 2],
            rng_seed: seed,
            activation: nimcp_adaptive::Activation::Tanh,
        };

        // Tiny SNN — just enough to exercise save/load + step.
        let snn = Some(SnnConfig {
            populations: vec![
                PopulationSpec {
                    name: "in".into(),
                    n_neurons: 32,
                    lif: LifParams::default(),
                    target_rate: 0.1,
                    homeostatic: HomeostaticParams::default(),
                    ..PopulationSpec::default()
                },
                PopulationSpec {
                    name: "out".into(),
                    n_neurons: 32,
                    lif: LifParams::default(),
                    target_rate: 0.1,
                    homeostatic: HomeostaticParams::default(),
                    ..PopulationSpec::default()
                },
            ],
            edges: vec![EdgeSpec {
                src: 0,
                dst: 1,
                fan_in: 8,
                weight_init: 1.0,
                weight_jitter: 0.2,
                rstdp: RstdpParams {
                    warmup_samples: 0,
                    w_max: 5.0,
                    ..RstdpParams::default()
                },
            }],
            rng_seed: seed.wrapping_add(1),
            rate_ema_alpha: 0.05,
            ..SnnConfig::default()
        });

        let lnn = Some(LnnConfig {
            input_dim: 3,
            output_dim: 1,
            layers: vec![LtcParams {
                n_in: 3,
                n_rec: 8,
                tau_init: 1.0,
                init_scale: 1.0,
            }],
            rng_seed: seed.wrapping_add(2),
            dt_ms: 0.1,
            ..LnnConfig::default()
        });

        BrainConfig {
            rng_seed: seed,
            deterministic: true,
            adaptive,
            snn,
            lnn,
            ..Default::default()
        }
    }

    #[tokio::test]
    async fn ensemble_brain_boots_all_three_networks() {
        let brain = Brain::new(ensemble_config(42)).unwrap();
        assert!(brain.snn().is_some(), "SNN should be present");
        assert!(brain.lnn().is_some(), "LNN should be present");
        assert_eq!(brain.snn().unwrap().n_populations(), 2);
        assert_eq!(brain.lnn().unwrap().layers.len(), 1);
    }

    #[tokio::test]
    async fn lnn_train_reduces_loss_inside_brain() {
        let mut brain = Brain::new(ensemble_config(42)).unwrap();
        let inputs: Vec<Array1<f32>> = (0..15)
            .map(|t| Array1::from_vec(vec![(t as f32 * 0.2).sin(), 0.3, -0.1]))
            .collect();
        let targets: Vec<Array1<f32>> = (0..15).map(|_| Array1::from_vec(vec![0.5])).collect();

        let y_before = brain.lnn_forward_sequence(&inputs).unwrap();
        let loss_before = nimcp_lnn::mse_sequence_loss(&y_before, &targets);

        let params = TrainParams {
            lr: 2.0e-2,
            grad_clip: 1.0,
        };
        for _ in 0..50 {
            brain
                .lnn_train_step_mse(&inputs, &targets, &params)
                .unwrap();
        }
        let y_after = brain.lnn_forward_sequence(&inputs).unwrap();
        let loss_after = nimcp_lnn::mse_sequence_loss(&y_after, &targets);

        assert!(
            loss_after < loss_before,
            "brain-routed LNN train did not reduce loss: {loss_before} -> {loss_after}"
        );
    }

    #[tokio::test]
    async fn snn_step_inside_brain_does_not_panic() {
        let mut brain = Brain::new(ensemble_config(42)).unwrap();
        let drive: Vec<f32> = vec![500.0; 32];
        let empty: Vec<f32> = Vec::new();
        let slices: Vec<&[f32]> = vec![&drive, &empty];
        for _ in 0..20 {
            brain.snn_step(&slices, 0.0, 1.0).unwrap();
        }
    }

    /// Phase 4c exit criterion (partial): train all three networks,
    /// save the ensemble atomically to a directory, reboot a fresh
    /// brain with the same config, restore, and verify every network's
    /// output is bit-identical on matched inputs.
    #[tokio::test]
    async fn ensemble_save_load_round_trip() {
        let cfg = ensemble_config(7);
        let mut a = Brain::new(cfg.clone()).unwrap();

        // Train the adaptive + LNN a bit so weights diverge from init.
        let feat = Array1::from_vec(vec![0.2, -0.4, 0.1, 0.9]);
        let tgt = Array1::from_vec(vec![0.7, -0.2]);
        for _ in 0..30 {
            a.learn(&feat, &tgt, 0.05);
        }

        let inputs: Vec<Array1<f32>> = (0..6)
            .map(|t| Array1::from_vec(vec![(t as f32 * 0.1).cos(), 0.2, 0.3]))
            .collect();
        let lnn_targets: Vec<Array1<f32>> = (0..6).map(|_| Array1::from_vec(vec![0.2])).collect();
        let params = TrainParams {
            lr: 1.0e-2,
            grad_clip: 1.0,
        };
        for _ in 0..20 {
            a.lnn_train_step_mse(&inputs, &lnn_targets, &params)
                .unwrap();
        }

        // Step SNN a few times so its weights have moved.
        let drive: Vec<f32> = vec![500.0; 32];
        let empty: Vec<f32> = Vec::new();
        let slices: Vec<&[f32]> = vec![&drive, &empty];
        for _ in 0..20 {
            a.snn_step(&slices, 0.1, 1.0).unwrap();
        }

        // Capture reference outputs.
        let y_adaptive_a = a.predict(&feat);
        let y_lnn_a = a.lnn_forward_sequence(&inputs).unwrap();
        let snn_weights_a: Vec<f32> = a.snn().unwrap().edge_weights(0).to_vec();

        // Save into a directory.
        let tmp = tempfile::tempdir().unwrap();
        let ensemble_dir = tmp.path().join("brain");
        a.save_ensemble(&ensemble_dir).unwrap();
        assert!(ensemble_dir.join("manifest.json").exists());
        assert!(ensemble_dir.join("adaptive.rkyv").exists());
        assert!(ensemble_dir.join("snn.json").exists());
        assert!(ensemble_dir.join("lnn.json").exists());

        // Reboot + load.
        let mut b = Brain::new(cfg).unwrap();
        b.load_ensemble(&ensemble_dir).unwrap();

        // Every network matches a's output.
        let y_adaptive_b = b.predict(&feat);
        for (pa, pb) in y_adaptive_a.iter().zip(y_adaptive_b.iter()) {
            assert!((pa - pb).abs() < 1e-6, "adaptive drift: {pa} vs {pb}");
        }

        let y_lnn_b = b.lnn_forward_sequence(&inputs).unwrap();
        for (t, (ya, yb)) in y_lnn_a.iter().zip(y_lnn_b.iter()).enumerate() {
            for (pa, pb) in ya.iter().zip(yb.iter()) {
                assert!(
                    (pa - pb).abs() < 1e-6,
                    "lnn drift at step {t}: {pa} vs {pb}"
                );
            }
        }

        let snn_weights_b: Vec<f32> = b.snn().unwrap().edge_weights(0).to_vec();
        assert_eq!(
            snn_weights_a, snn_weights_b,
            "snn weights differ after load"
        );
    }

    // -------------------------------------------------------------------------
    // Phase 11 tests — CNN / FNO / HNN integration.
    // -------------------------------------------------------------------------

    fn phase11_config(seed: u64) -> BrainConfig {
        use nimcp_cnn::{CnnConfig, CnnLayerSpec};
        use nimcp_fno::FnoConfig;
        use nimcp_hnn::HnnConfig;

        let cnn = Some(CnnConfig {
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
                CnnLayerSpec::Linear { out_features: 6 },
            ],
            rng_seed: seed.wrapping_add(11),
            substrate: Default::default(),
            thalamic: None,
        });
        let fno = Some(FnoConfig {
            in_channels: 1,
            out_channels: 1,
            hidden_channels: 4,
            n_blocks: 1,
            modes: 3,
            rng_seed: seed.wrapping_add(12),
            substrate: Default::default(),
            thalamic: None,
        });
        let hnn = Some(HnnConfig {
            dof: 2,
            hidden_layers: vec![8],
            dt: 0.01,
            rng_seed: seed.wrapping_add(13),
            substrate: Default::default(),
            thalamic: None,
        });

        BrainConfig {
            rng_seed: seed,
            deterministic: true,
            cnn,
            fno,
            hnn,
            ..Default::default()
        }
    }

    /// Phase 11 SHIP criterion: a single brain config can declare all
    /// of {snn, lnn, cnn, fno, hnn} simultaneously and boot. Here we
    /// boot with cnn+fno+hnn (the new networks) and verify each is
    /// callable end-to-end.
    #[tokio::test]
    async fn phase11_brain_boots_all_three_new_networks() {
        let mut brain = Brain::new(phase11_config(0xB101)).unwrap();
        assert!(brain.cnn().is_some());
        assert!(brain.fno().is_some());
        assert!(brain.hnn().is_some());

        // CNN forward.
        let cnn_input = ndarray::Array4::<f32>::zeros((1, 1, 8, 8));
        let cnn_out = brain.cnn_predict(&cnn_input).unwrap();
        assert_eq!(cnn_out.dim(), (1, 6));

        // FNO forward.
        let fno_input = ndarray::Array3::<f32>::zeros((1, 1, 16));
        let fno_out = brain.fno_predict(&fno_input).unwrap();
        assert_eq!(fno_out.dim(), (1, 1, 16));

        // HNN — set state, step, energy.
        brain
            .hnn_set_state(
                Array1::from_vec(vec![1.0, 0.0]),
                Array1::from_vec(vec![0.0, 0.5]),
            )
            .unwrap();
        let _e0 = brain.hnn_energy().unwrap();
        brain.hnn_step().unwrap();
        let _e1 = brain.hnn_energy().unwrap();
    }

    /// Save+load round-trip including the three new networks.
    #[tokio::test]
    async fn phase11_save_load_round_trip() {
        let cfg = phase11_config(0xB102);
        let a = Brain::new(cfg.clone()).unwrap();
        let cnn_input = ndarray::Array4::from_shape_fn((1, 1, 8, 8), |(_, _, h, w)| {
            ((h * 8 + w) as f32 * 0.01).sin()
        });
        let cnn_a = a.cnn_predict(&cnn_input).unwrap();

        let tmp = tempfile::tempdir().unwrap();
        let dir = tmp.path().join("brain");
        a.save_ensemble(&dir).unwrap();
        assert!(dir.join("cnn.json").exists());
        assert!(dir.join("fno.json").exists());
        assert!(dir.join("hnn.json").exists());

        let mut b = Brain::new(cfg).unwrap();
        b.load_ensemble(&dir).unwrap();
        let cnn_b = b.cnn_predict(&cnn_input).unwrap();
        for (x, y) in cnn_a.iter().zip(cnn_b.iter()) {
            assert!((x - y).abs() < 1e-6, "cnn drift after load: {x} vs {y}");
        }
    }

    /// Phase 11-substrate: a thalamic channel declared on the CNN must
    /// (a) cause the brain to open its shared router and register the
    /// CNN's channel, and (b) have its submits forwarded through
    /// `tick_thalamic`.
    #[tokio::test]
    async fn phase11_substrate_cnn_thalamic_wires_into_router() {
        use nimcp_cnn::{CnnConfig, CnnLayerSpec, CnnThalamicCfg};

        const CNN_SOURCE: u32 = 4001;
        let cnn = Some(CnnConfig {
            input_shape: (1, 4, 4),
            layers: vec![
                CnnLayerSpec::Flatten,
                CnnLayerSpec::Linear { out_features: 3 },
            ],
            rng_seed: 0xB103,
            substrate: Default::default(),
            thalamic: Some(CnnThalamicCfg {
                source_id: CNN_SOURCE,
                destinations: vec![10, 11],
                submit_threshold: 0.5,
                mode: nimcp_thalamic::RelayMode::Tonic,
            }),
        });
        let cfg = BrainConfig {
            rng_seed: 0xB103,
            deterministic: true,
            cnn,
            ..Default::default()
        };
        let mut brain = Brain::new(cfg).unwrap();

        // (a) Router opened and the CNN channel registered.
        let router = brain.thalamic_router().expect("router should be open");
        assert!(
            router.channel(CNN_SOURCE).is_some(),
            "CNN thalamic channel must be registered in the router"
        );

        // (b) Drive two submits on the CNN's own channel (test is a child
        // module, so it can touch the private field), then forward them.
        let cnn_ch = brain
            .cnn
            .as_mut()
            .unwrap()
            .thalamic_channel
            .as_mut()
            .unwrap();
        cnn_ch.record_submit();
        cnn_ch.record_submit();
        let forwarded = brain.tick_thalamic();
        assert_eq!(forwarded, 2, "both CNN submits should forward to the router");
    }

    #[tokio::test]
    async fn ensemble_save_is_atomic_under_partial_failure() {
        // If the target dir already exists with arbitrary contents, a
        // successful save_ensemble must leave only the new layout (no
        // leftover files from the prior content).
        let cfg = ensemble_config(11);
        let a = Brain::new(cfg).unwrap();

        let tmp = tempfile::tempdir().unwrap();
        let dir = tmp.path().join("brain");
        std::fs::create_dir(&dir).unwrap();
        std::fs::write(dir.join("stale.txt"), b"leftover").unwrap();

        a.save_ensemble(&dir).unwrap();
        assert!(
            !dir.join("stale.txt").exists(),
            "stale file survived atomic swap"
        );
        assert!(dir.join("manifest.json").exists());
    }

    // -------------------------------------------------------------------
    // Phase L8 — language + toxicity brain integration.
    // -------------------------------------------------------------------

    fn language_config(seed: u64) -> BrainConfig {
        BrainConfig {
            rng_seed: seed,
            deterministic: true,
            language: Some(LanguageConfig {
                semantic_dim: 8,
                rng_seed: seed,
                enable_toxicity: true,
                toxicity_ml_seed: seed ^ 0x9,
                spectrum_vocab_cap: None,
                default_stage: 4,
                min_produce_words: 1,
                recurrent_max_iters: 1,
                reason_in_content: false,
            }),
            ..Default::default()
        }
    }

    /// Ground a small vocabulary so the cascade can actually produce.
    fn teach(brain: &mut Brain) {
        use nimcp_language::Modality;
        let gl = brain.language_mut().unwrap();
        gl.ground("dog", &[1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl.ground("cat", &[0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Visual);
        gl.ground("run", &[0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0], Modality::Motor);
        for _ in 0..10 {
            brain.language_learn("the dog likes to run").unwrap();
        }
    }

    #[tokio::test]
    async fn language_brain_boots_and_responds() {
        let mut brain = Brain::new(language_config(0x18)).unwrap();
        assert!(brain.language().is_some());
        assert!(brain.has_toxicity());
        teach(&mut brain);
        let (vec, conf) = brain.language_comprehend("the dog").unwrap();
        assert_eq!(vec.len(), 8);
        assert!(conf > 0.0);
        let r = brain.language_respond("dog").unwrap();
        assert!(!r.blocked_for_toxicity);
        assert!(!r.text.is_empty());
    }

    #[tokio::test]
    async fn toxicity_gate_blocks_above_cascade() {
        let mut brain = Brain::new(language_config(7)).unwrap();
        teach(&mut brain);
        // Toxic input must be intercepted BEFORE the cascade runs — the
        // response is a counterclaim, flagged as blocked.
        let r = brain.language_respond("kill all immigrants").unwrap();
        assert!(r.blocked_for_toxicity, "toxic input must be gated");
        assert!(!r.text.is_empty(), "counterclaim emitted");
        assert_eq!(r.toxicity_category, "violence_against_group");
    }

    #[tokio::test]
    async fn benign_input_passes_the_gate() {
        let mut brain = Brain::new(language_config(8)).unwrap();
        teach(&mut brain);
        let r = brain.language_respond("the cat").unwrap();
        assert!(!r.blocked_for_toxicity);
    }

    #[tokio::test]
    async fn classify_toxicity_diagnostic() {
        let brain = Brain::new(language_config(9)).unwrap();
        let (harm, _fair, block) = brain.classify_toxicity("muslims are subhuman").unwrap();
        assert!(harm >= 0.9 && block);
        let (h2, _f2, b2) = brain.classify_toxicity("what a lovely garden").unwrap();
        assert!(h2 < 0.7 && !b2);
    }

    #[tokio::test]
    async fn language_survives_ensemble_round_trip() {
        let mut a = Brain::new(language_config(21)).unwrap();
        teach(&mut a);
        let resp_a = a.language_respond("dog").unwrap();

        let tmp = tempfile::tempdir().unwrap();
        let dir = tmp.path().join("brain");
        a.save_ensemble(&dir).unwrap();
        assert!(dir.join("language.json").exists());
        assert!(dir.join("toxicity_ml.json").exists());

        let mut b = Brain::new(language_config(21)).unwrap();
        b.load_ensemble(&dir).unwrap();
        // Vocabulary + bindings restored → identical response.
        let resp_b = b.language_respond("dog").unwrap();
        assert_eq!(resp_a.text, resp_b.text);
        assert_eq!(b.language().unwrap().lexicon.vocab_count(), a.language().unwrap().lexicon.vocab_count());
    }

    #[tokio::test]
    async fn brain_without_language_errors_cleanly() {
        let mut brain = Brain::new(BrainConfig::default()).unwrap();
        assert!(brain.language().is_none());
        assert!(brain.language_respond("hi").is_err());
        assert!(brain.classify_toxicity("hi").is_err());
    }

    #[tokio::test]
    async fn reason_in_content_toggle_round_trips() {
        // Default OFF (V1 parity), runtime-togglable.
        let mut brain = Brain::new(language_config(31)).unwrap();
        assert!(!brain.reason_in_content());
        brain.set_reason_in_content(true);
        assert!(brain.reason_in_content());
        brain.set_reason_in_content(false);
        assert!(!brain.reason_in_content());
        // Initialized from config when set there.
        let mut cfg = language_config(32);
        cfg.language.as_mut().unwrap().reason_in_content = true;
        let brain2 = Brain::new(cfg).unwrap();
        assert!(brain2.reason_in_content());
    }
}
