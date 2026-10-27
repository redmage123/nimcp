//! Phase 3d — top-level [`SnnNetwork`] composition.
//!
//! Wires the four Phase 3 modules into a runnable spiking network:
//!
//! ```text
//!   inputs ─▶ I_syn ─▶ LIF ─▶ spike ─▶ CSR forward ─▶ I_syn ...
//!                                        │
//!                                        ▼
//!                                    R-STDP    ──▶ weight update
//!                                        │
//!                                        ▼
//!                                 homeostatic  ──▶ rate EMA + scale
//! ```
//!
//! # Ownership
//!
//! - [`SnnNetwork`] owns every population and every directed edge, plus
//!   the preallocated I_syn / spike scratch buffers — no per-tick
//!   allocation on the hot path.
//!
//! # Integration tests
//!
//! - `saturation_recovery` — builds a two-pop network, hand-saturates
//!   every weight to `w_max`, verifies spike rates are above target,
//!   runs [`SnnNetwork::apply_quiet_start`], then steps the network and
//!   asserts per-pop rates land inside the deadband within 100 steps.
//!   This is V2_PLAN Phase 3's exit criterion.

use nimcp_plasticity::HomeostaticParams;
use serde::{Deserialize, Serialize};

use crate::csr::{CsrError, CsrSynapses, PopulationId};
use crate::homeostatic::{DEFAULT_RATE_EMA_ALPHA, PopulationRateEma, step_homeostatic};
use crate::lif::{LifParams, LifState, lif_step_cpu};
use crate::rstdp::{RstdpParams, RstdpState, step_rstdp};
use crate::tuning::TunableState;

// -------------------------------------------------------------------------
// Config
// -------------------------------------------------------------------------

/// Opt-in adaptation-mechanism config (used for both AHP and pump).
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct AdaptationCfg {
    /// Exponential decay time constant (ms).
    pub tau_ms: f32,
    /// Hyperpolarization per unit adapt_var (mV).
    pub gain_mv: f32,
    /// Increment on each spike (usually 1.0).
    pub spike_bump: f32,
}

impl Default for AdaptationCfg {
    /// Fast AHP defaults — tau ~150 ms, gain 0.6 mV.
    fn default() -> Self {
        Self {
            tau_ms: 150.0,
            gain_mv: 0.6,
            spike_bump: 1.0,
        }
    }
}

impl AdaptationCfg {
    /// Slow Na/K pump defaults — tau ~5 s, gain 0.05 mV.
    #[must_use]
    pub fn pump_defaults() -> Self {
        Self {
            tau_ms: 5_000.0,
            gain_mv: 0.05,
            spike_bump: 1.0,
        }
    }
}

/// Opt-in basket-cell pool config.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct BasketCfg {
    /// Basket pool size as a fraction of parent population. Clamped to [0.01, 0.5].
    pub fraction: f32,
}

impl Default for BasketCfg {
    fn default() -> Self {
        Self { fraction: 0.2 }
    }
}

/// Opt-in biological substrate config — Path A Phase 1.
///
/// When `enabled`, the population owns a [`nimcp_substrate::NeuralSubstrate`]
/// that modulates its LIF dynamics (tau_mem, threshold, refractory),
/// R-STDP learning rate, and spike reliability each step. Cached
/// effects are recomputed every `update_period` steps (default `N=10`,
/// matching V1's cadence — chemistry changes slower than spikes).
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(default)]
pub struct SnnSubstrateCfg {
    /// Master switch. `false` disables substrate entirely — effects
    /// cache is never populated, hot path is identical to pre-Phase-1.
    pub enabled: bool,
    /// Recompute effects every N steps. Lower = more responsive,
    /// higher = cheaper. Default `10`.
    pub update_period: u32,
    /// Apply `spike_reliability` as probabilistic dropout after LIF
    /// threshold crossing. Default `true` (when `enabled`).
    pub spike_dropout_on: bool,
    /// Scale R-STDP learning rate by `plasticity_mod`. Default `true`.
    pub plasticity_mod_on: bool,
    /// Scale intrinsic-reward amplitude by `ca_handling_mod`. Default `true`.
    pub reward_mod_on: bool,
    /// Silence the population when `overall_capacity < threshold`.
    /// Matches V1's emergency-silence convention. Default `0.1`.
    pub emergency_silence_threshold: f32,
    /// Initial chemistry state. Default = full health.
    pub initial_state: nimcp_substrate::NeuralSubstrate,
    /// Per-spike / per-plasticity costs + passive recovery.
    pub dynamics: nimcp_substrate::NeuralSubstrateConfig,
}

impl Default for SnnSubstrateCfg {
    fn default() -> Self {
        Self {
            enabled: false,
            update_period: 10,
            spike_dropout_on: true,
            plasticity_mod_on: true,
            reward_mod_on: true,
            emergency_silence_threshold: 0.1,
            initial_state: nimcp_substrate::NeuralSubstrate::default(),
            dynamics: nimcp_substrate::NeuralSubstrateConfig::default(),
        }
    }
}

/// Static description of one neuron population.
///
/// [`Default`] produces a backward-compatible profile with every Phase 3.5
/// stability mechanism **off**: `noise.rate_hz = 0`, `depression.inc = 0`,
/// all `Option` fields `None`. Callers who want the master-tuned
/// production defaults set `noise: NoiseConfig::default()` etc.
/// explicitly — JSON construction via `#[serde(default)]` already
/// picks those.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PopulationSpec {
    /// Human-readable label.
    pub name: String,
    /// Number of LIF neurons.
    pub n_neurons: u32,
    /// LIF dynamics.
    pub lif: LifParams,
    /// Homeostatic target (fraction spiking per step).
    pub target_rate: f32,
    /// Homeostatic bounds + deadband + gain.
    pub homeostatic: HomeostaticParams,
    /// Poisson background noise config. Default is V1's 20 Hz × 30 mV;
    /// set `rate_hz = 0.0` to disable.
    #[serde(default)]
    pub noise: crate::noise::NoiseConfig,
    /// Short-term depression config. Default `inc=0.3, tau=50ms, cap=0.5`;
    /// set `inc = 0.0` to disable.
    #[serde(default)]
    pub depression: crate::depression::DepressionConfig,
    /// Optional fast M-current AHP adaptation. `None` disables.
    #[serde(default)]
    pub adaptation_ahp: Option<AdaptationCfg>,
    /// Optional slow Na/K pump adaptation. `None` disables.
    #[serde(default)]
    pub adaptation_pump: Option<AdaptationCfg>,
    /// Optional basket-cell pool. `None` disables.
    #[serde(default)]
    pub basket: Option<BasketCfg>,
    /// Biological substrate config — Path A Phase 1. Default disabled;
    /// when enabled, wires this population's chemistry state through
    /// the nimcp-substrate effect multipliers.
    #[serde(default)]
    pub substrate: SnnSubstrateCfg,
}

impl Default for PopulationSpec {
    /// Stability-mechanism-OFF profile — preserves Phase 3 test behavior.
    /// Use this as a base for `..Default::default()` struct-update syntax
    /// when you only want to override the fields that matter.
    fn default() -> Self {
        Self {
            name: "pop".into(),
            n_neurons: 16,
            lif: LifParams::default(),
            target_rate: 0.05,
            homeostatic: HomeostaticParams::default(),
            noise: crate::noise::NoiseConfig {
                rate_hz: 0.0,
                pulse_mv: 0.0,
                kind: crate::noise::NoiseKind::default(),
            },
            depression: crate::depression::DepressionConfig {
                inc: 0.0,
                ..crate::depression::DepressionConfig::default()
            },
            adaptation_ahp: None,
            adaptation_pump: None,
            basket: None,
            substrate: SnnSubstrateCfg::default(),
        }
    }
}

/// Static description of one directed synapse edge (src → dst).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EdgeSpec {
    /// Source population index.
    pub src: usize,
    /// Destination population index.
    pub dst: usize,
    /// Incoming fan-in per destination neuron. Clamped to `src.n_neurons`.
    pub fan_in: u32,
    /// Initial mean weight.
    pub weight_init: f32,
    /// Uniform jitter half-width.
    pub weight_jitter: f32,
    /// R-STDP kernel + gains.
    pub rstdp: RstdpParams,
}

/// Top-level configuration for [`SnnNetwork`].
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SnnConfig {
    /// Population specs.
    pub populations: Vec<PopulationSpec>,
    /// Directed synapse edges.
    pub edges: Vec<EdgeSpec>,
    /// Seed for the deterministic topology RNG.
    pub rng_seed: u64,
    /// Initial alpha for every population's firing-rate EMA. Defaults to
    /// [`DEFAULT_RATE_EMA_ALPHA`] when absent from JSON — hand-written
    /// daemon / Python configs no longer have to spell it out.
    #[serde(default = "default_rate_ema_alpha")]
    pub rate_ema_alpha: f32,
    /// Intrinsic-reward config shared across populations. See
    /// [`SnnNetwork::compute_intrinsic_reward`]; callers pass the
    /// result as the `reward` argument to [`SnnNetwork::step`] when
    /// they want rate-driven R-STDP.
    #[serde(default)]
    pub intrinsic_reward: crate::intrinsic_reward::IntrinsicRewardConfig,
    /// When `true`, `step()` uses [`crate::homeostatic::step_homeostatic_with_reward`]
    /// (widens scale-down floor to 0.90 when reward is stuck AND rate is
    /// saturated). When `false` (default, backward-compat), uses the
    /// tight-bounds [`crate::homeostatic::step_homeostatic`]. Enable on
    /// brains that drive the SNN with a reward signal and want the
    /// Phase 3.5 safety net against saturation↔collapse oscillation.
    #[serde(default)]
    pub reward_coupled_homeostatic: bool,
    /// Optional thalamic-channel config — Path A Phase 1. When `Some`,
    /// the SNN owns a [`nimcp_thalamic::ThalamicChannel`] that
    /// represents the network's output-side attention-gated routing
    /// to external destinations (other networks, brain regions). Each
    /// step, if the network-wide firing rate exceeds
    /// `submit_threshold`, the channel's submit counter is bumped —
    /// external callers tick the shared [`nimcp_thalamic::ThalamicRouter`]
    /// to update Hebbian route weights.
    #[serde(default)]
    pub thalamic: Option<SnnThalamicCfg>,
    /// Phase 9a — opt in to GPU-accelerated forward pass.
    ///
    /// When `true` **and** the crate is built with `--features cuda`,
    /// the hot loops for CSR synapse forward and LIF integration run
    /// on-device via [`crate::csr::CsrGpu`] + [`crate::lif::LifGpu`].
    /// Plasticity (R-STDP + homeostatic), noise, adaptation, basket,
    /// depression, and substrate effects stay on CPU this phase —
    /// kernels for those land in 9b/9c.
    ///
    /// Default `false`: CPU path unchanged. When `true` on a build
    /// without the `cuda` feature, `SnnNetwork::new` returns
    /// `SnnError::GpuUnavailable`.
    #[serde(default)]
    pub use_gpu_forward: bool,
    /// Phase 10c — which CUDA device each population (and every edge
    /// into it) is built on when `use_gpu_forward` is set. Validated on
    /// the CPU path too, otherwise ignored there. Default: everything on
    /// ordinal 0, i.e. the pre-Phase-10 behaviour.
    #[serde(default)]
    pub device_assignment: crate::device::DeviceAssignment,
}

fn default_rate_ema_alpha() -> f32 {
    DEFAULT_RATE_EMA_ALPHA
}

impl Default for SnnConfig {
    fn default() -> Self {
        Self {
            populations: Vec::new(),
            edges: Vec::new(),
            rng_seed: 0,
            rate_ema_alpha: DEFAULT_RATE_EMA_ALPHA,
            intrinsic_reward: crate::intrinsic_reward::IntrinsicRewardConfig::default(),
            reward_coupled_homeostatic: false,
            thalamic: None,
            use_gpu_forward: false,
            device_assignment: crate::device::DeviceAssignment::default(),
        }
    }
}

/// Optional network-level thalamic-channel config. When set, the SNN
/// constructs a [`nimcp_thalamic::ThalamicChannel`] at `source_id` with
/// the given destinations and auto-records a submit whenever the
/// network's fraction-spiking exceeds `submit_threshold`.
///
/// The [`nimcp_thalamic::ThalamicRouter`] itself is held externally
/// (brain-level); the SNN only owns its channel. Callers read
/// [`SnnNetwork::thalamic_channel`] to access the submit counter and
/// tick the router themselves.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SnnThalamicCfg {
    /// Stable source identifier. One per SNN instance.
    pub source_id: u32,
    /// External destination IDs — up to
    /// [`nimcp_thalamic::THALAMIC_MAX_DESTINATIONS`] (16).
    pub destinations: Vec<u32>,
    /// Fraction-spiking threshold above which `step()` auto-records
    /// a submit on this channel. Range `[0, 1]`. Default `0.01` (any
    /// activity above 1% average rate triggers submit).
    pub submit_threshold: f32,
    /// Initial relay mode. Default [`nimcp_thalamic::RelayMode::Tonic`].
    pub mode: nimcp_thalamic::RelayMode,
}

impl Default for SnnThalamicCfg {
    fn default() -> Self {
        Self {
            source_id: 0,
            destinations: Vec::new(),
            submit_threshold: 0.01,
            mode: nimcp_thalamic::RelayMode::default(),
        }
    }
}

// -------------------------------------------------------------------------
// Network
// -------------------------------------------------------------------------

/// Construction error for [`SnnNetwork`].
#[derive(Debug, thiserror::Error)]
pub enum SnnError {
    /// Edge references an out-of-range population index.
    #[error("edge {edge_idx}: population index {pop_idx} >= n_populations {n_pops}")]
    EdgePopOutOfRange {
        /// Offending edge index.
        edge_idx: usize,
        /// Bad population index.
        pop_idx: usize,
        /// Total population count.
        n_pops: usize,
    },
    /// Underlying CSR builder error.
    #[error("edge {edge_idx}: csr build failed — {err}")]
    CsrBuild {
        /// Offending edge index.
        edge_idx: usize,
        /// Underlying [`CsrError`].
        err: CsrError,
    },
    /// A per-population stability mechanism (adaptation, basket, etc.)
    /// failed to initialize. Message encodes the pop index + mechanism.
    #[error("population {pop_idx}: stability mechanism init failed — {msg}")]
    StabilityInit {
        /// Offending population index.
        pop_idx: usize,
        /// Detail — which mechanism and the underlying error.
        msg: String,
    },
    /// `use_gpu_forward = true` but the crate was built without
    /// `--features cuda`, or CUDA device / NVRTC init failed.
    #[error("GPU forward requested but unavailable: {0}")]
    GpuUnavailable(String),
    /// `device_assignment` could not be resolved against the population list.
    #[error("device assignment: {0}")]
    DeviceAssignment(#[from] crate::device::DeviceAssignError),
    /// [`SnnNetwork::tune`] was given a name outside [`TUNABLE_NAMES`].
    #[error("unknown tunable `{name}` (known: {known})", name = .0, known = TUNABLE_NAMES.join(", "))]
    UnknownTunable(String),
    /// [`SnnNetwork::tune`] was given a value outside the knob's range.
    #[error("tunable `{name}` = {value}: {reason}")]
    InvalidTunable {
        /// Knob name.
        name: String,
        /// Rejected value.
        value: f32,
        /// Why it was rejected.
        reason: &'static str,
    },
}

/// Per-population runtime state.
struct Pop {
    spec: PopulationSpec,
    state: LifState,
    rate: PopulationRateEma,
    i_syn_scratch: Vec<f32>,
    // Phase 3.5 — stability mechanisms.
    noise_rng: rand_chacha::ChaCha20Rng,
    /// Pink-noise runtime state — `Some` iff `spec.noise.kind` is
    /// [`crate::noise::NoiseKind::Pink`]. The Poisson path keeps this
    /// `None` so no per-step allocation overhead bleeds in.
    pink_state: Option<crate::noise::PinkNoiseState>,
    depression_state: crate::depression::DepressionState,
    adaptation_ahp: Option<crate::adaptation::AdaptationState>,
    adaptation_pump: Option<crate::adaptation::AdaptationState>,
    basket: Option<crate::basket::BasketPool>,
    /// Per-neuron hyperpolarization buffer (shared between AHP + pump).
    hyperpol_scratch: Vec<f32>,
    /// Per-neuron `fired` buffer as f32, for adaptation + depression updates.
    fired_f32_scratch: Vec<f32>,
    /// Phase 9e — per-neuron membrane-prep delta. Accumulates Poisson +
    /// pink noise pulses (positive) and AHP/pump hyperpolarization
    /// (negative) per step. Applied to `v_mem` immediately before the
    /// LIF integration; on the GPU LIF path it's uploaded alongside
    /// `i_syn` and added inside the kernel. Replaces the prior
    /// in-place `v_mem` mutation pattern that forced a CPU fallback
    /// when stability mechanisms were active.
    v_prep_scratch: Vec<f32>,
    /// Per-pre-neuron depression scale = `1 - depression.d[i]`, clamped
    /// to `[1 - cap, 1]`. Recomputed each step from depression_state.
    depression_scale_scratch: Vec<f32>,
    // Phase 1 — substrate state (Path A).
    /// Per-population chemistry. `None` when `spec.substrate.enabled = false`.
    substrate_state: Option<nimcp_substrate::NeuralSubstrate>,
    /// Cached effect bundles, recomputed every `spec.substrate.update_period` steps.
    substrate_effects: Option<(
        nimcp_substrate::AxonSubstrateEffects,
        nimcp_substrate::DendriteSubstrateEffects,
    )>,
    /// Tick counter for substrate effect recomputation cadence.
    substrate_tick_counter: u32,
    /// Dedicated RNG for substrate-driven spike dropout, so it doesn't
    /// consume the Poisson-noise RNG's stream.
    substrate_dropout_rng: rand_chacha::ChaCha20Rng,
    /// Counter of plasticity updates this step — fed to
    /// `debit_activity` at end of step.
    substrate_plasticity_count: u32,
    /// Phase 9a — GPU LIF integrator. `Some` iff the network was
    /// built with `use_gpu_forward = true` AND the crate has the
    /// `cuda` feature enabled.
    #[cfg(feature = "cuda")]
    lif_gpu: Option<crate::lif::LifGpu>,
}

/// Per-edge runtime state.
struct Edge {
    spec: EdgeSpec,
    csr: CsrSynapses,
    rstdp: RstdpState,
    /// Scratch I_syn buffer for this edge's CSR forward. Preallocated
    /// at construction; the CSR writes into it, the `step` pipeline
    /// then adds it into the destination population's I_syn scratch.
    i_syn_scratch: Vec<f32>,
    /// Phase 9a — GPU CSR forward. `Some` iff `use_gpu_forward`
    /// was requested at construction. Uploaded once; weights are
    /// re-uploaded after homeostatic scaling each step (hot sync).
    #[cfg(feature = "cuda")]
    csr_gpu: Option<crate::csr::CsrGpu>,
    /// Phase 9d — GPU R-STDP plasticity. `Some` iff `use_gpu_forward`
    /// was requested at construction AND init succeeded. The GPU side
    /// owns eligibility + last-spike timestamp tables; the CPU
    /// `rstdp` field still tracks scalar state (`reward_trace`,
    /// `rate_samples`) for checkpoint snapshots.
    #[cfg(feature = "cuda")]
    rstdp_gpu: Option<crate::rstdp::RstdpGpu>,
    /// CB-mode scratch buffers: per-post-neuron g_exc / g_inh deltas
    /// produced by [`CsrSynapses::i_syn_cpu_cb_with_pre_scale`].
    /// Allocated lazily on the first CB step (kept across steps once
    /// allocated to avoid repeated growth).
    cb_g_exc_scratch: Vec<f32>,
    cb_g_inh_scratch: Vec<f32>,
}

/// Spiking neural network — the Phase 3 composition.
pub struct SnnNetwork {
    populations: Vec<Pop>,
    edges: Vec<Edge>,
    t_ms: f32,
    reward_coupled_homeostatic: bool,
    /// Optional thalamic channel (network-level output routing). `None`
    /// when `config.thalamic` is `None` — no per-step work happens on
    /// the disable path.
    thalamic_channel: Option<nimcp_thalamic::ThalamicChannel>,
    /// Cached `submit_threshold` so hot path doesn't keep dereferencing
    /// the optional cfg.
    thalamic_submit_threshold: f32,
    /// Sticky CB rescale flag — set to `true` once
    /// [`Self::rescale_for_conductance`] has run. Idempotent guard
    /// preventing the V1 double-rescale bomb (commit `f79764c9b` →
    /// fix in `7e69a2a5f` + `312c9b323`).
    cb_weights_rescaled: bool,
    /// Phase 10c — device ordinal per population, resolved from
    /// `config.device_assignment`. All zeros on the CPU path.
    pop_device: Vec<u32>,
    /// Phase 10b — one context per distinct ordinal in `pop_device`.
    /// `Some` iff `use_gpu_forward`.
    #[cfg(feature = "cuda")]
    gpu: Option<nimcp_gpu::MultiGpuContext>,
    /// Live-tuning: the virtual `rstdp_lr` knob (V1 watchdog name). V2's
    /// R-STDP has no single learning rate — `tune("rstdp_lr", v)` scales
    /// every edge's STDP amplitudes `A±` by `v / baseline`, where
    /// `baseline` is [`RSTDP_LR_BASELINE`] and the reference amplitudes
    /// are the construction-time `A±` kept in `rstdp_ref`.
    rstdp_lr: TunableState,
    /// Construction-time `(a_plus, a_minus)` per edge — the reference the
    /// `rstdp_lr` scale is applied to (avoids drift from repeated ratios).
    rstdp_ref: Vec<(f32, f32)>,
    /// Watchdog knobs V2 accepts for wire parity but has no mechanism
    /// for (see [`TUNABLE_NAMES`] docs). Stored so `tunables()` reflects
    /// what the operator set; never read by `step()`.
    unapplied_tunables: std::collections::BTreeMap<String, f32>,
}

/// Live-tunable knob names accepted by [`SnnNetwork::tune`] — the V1
/// `snn_watchdog.py` vocabulary plus V2's `noise_pulse_mv`.
///
/// | name | applies to | V2 effect |
/// |---|---|---|
/// | `rstdp_lr` | every edge | scales STDP `A±` by `value / RSTDP_LR_BASELINE` |
/// | `homeo_min_scale` / `homeo_max_scale` | every population | homeostatic scale clamp |
/// | `target_rate` | every population | homeostatic target (fraction spiking / step) |
/// | `noise_rate_hz` / `noise_pulse_mv` | every population | background noise |
/// | `max_scale_dead` / `dead_threshold` / `rstdp_baseline_alpha` | — | **recorded only**: V2 has no dead-population scale override and no reward-baseline EMA; `TuneOutcome::applied == false` |
pub const TUNABLE_NAMES: &[&str] = &[
    "rstdp_lr",
    "homeo_min_scale",
    "homeo_max_scale",
    "target_rate",
    "noise_rate_hz",
    "noise_pulse_mv",
    "max_scale_dead",
    "dead_threshold",
    "rstdp_baseline_alpha",
];

/// Value of the virtual `rstdp_lr` knob that leaves the construction-time
/// STDP amplitudes untouched (V1's default `rstdp_lr`).
pub const RSTDP_LR_BASELINE: f32 = 1e-4;

/// Result of one [`SnnNetwork::tune`] call.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct TuneOutcome {
    /// Knob name as given.
    pub name: String,
    /// Value now in force (after any clamping — none today).
    pub value: f32,
    /// `false` for the recorded-only knobs.
    pub applied: bool,
    /// Populations touched.
    pub populations: usize,
    /// Edges touched.
    pub edges: usize,
    /// Operator-readable note for recorded-only knobs.
    pub note: Option<String>,
}

impl SnnNetwork {
    /// Build a network from `config`.
    #[allow(clippy::needless_pass_by_value)] // `config` owns Vecs we take apart.
    pub fn new(config: SnnConfig) -> Result<Self, SnnError> {
        let n_pops = config.populations.len();

        use rand::SeedableRng;

        let pop_sizes: Vec<u32> = config.populations.iter().map(|p| p.n_neurons).collect();
        let pop_device = config.device_assignment.resolve(&pop_sizes)?;

        // Phase 10 — one context per distinct device (Phase 9b's single
        // shared context is the `SingleDevice` case; per-object contexts
        // in 9a exhausted VRAM). A population's LIF integrator and every
        // edge into it are built on the population's context; the host
        // uploads each edge's source spikes and downloads its i_syn, so
        // no device-to-device traffic is needed.
        #[cfg(feature = "cuda")]
        let gpu: Option<nimcp_gpu::MultiGpuContext> = if config.use_gpu_forward {
            let ordinals = config.device_assignment.ordinals(&pop_sizes)?;
            Some(nimcp_gpu::MultiGpuContext::new(&ordinals).map_err(|e| {
                SnnError::GpuUnavailable(format!("MultiGpuContext init {ordinals:?}: {e:?}"))
            })?)
        } else {
            None
        };
        #[cfg(feature = "cuda")]
        let pop_ctx: Vec<Option<std::sync::Arc<cudarc::driver::CudaContext>>> = pop_device
            .iter()
            .map(|&o| gpu.as_ref().and_then(|g| g.context_for_ordinal(o).cloned()))
            .collect();

        let mut populations: Vec<Pop> = Vec::with_capacity(config.populations.len());
        for (pop_idx, spec) in config.populations.iter().enumerate() {
            let state = LifState::new(spec.n_neurons, &spec.lif);
            let rate = PopulationRateEma::new(config.rate_ema_alpha, spec.target_rate);
            let n = spec.n_neurons as usize;
            let i_syn_scratch = vec![0.0; n];
            let hyperpol_scratch = vec![0.0; n];
            let fired_f32_scratch = vec![0.0; n];
            let depression_scale_scratch = vec![1.0; n];
            let noise_rng_seed = config
                .rng_seed
                .wrapping_add(0xAA55_AA55_AA55_AA55)
                .wrapping_add(pop_idx as u64);
            let noise_rng = rand_chacha::ChaCha20Rng::seed_from_u64(noise_rng_seed);
            // Pink-noise state is only allocated when the caller opts
            // in via `NoiseKind::Pink`; otherwise `None` keeps the
            // Poisson path bit-identical to pre-pink behavior.
            let pink_state = match spec.noise.kind {
                crate::noise::NoiseKind::Poisson => None,
                crate::noise::NoiseKind::Pink { n_octaves } => {
                    let pink_seed = config
                        .rng_seed
                        .wrapping_add(0xC0FF_EE00_C0FF_EE00)
                        .wrapping_add(pop_idx as u64);
                    Some(crate::noise::PinkNoiseState::new(n_octaves, pink_seed))
                }
            };
            let depression_state = crate::depression::DepressionState::new(n);

            // Substrate runtime state — owns chemistry iff enabled.
            let substrate_state = if spec.substrate.enabled {
                let mut s = spec.substrate.initial_state;
                // Propagate the population's index as region_id if
                // caller didn't set one (keeps per-region chemistry
                // identifiable in stats).
                if s.region_id == 0 {
                    s.region_id = pop_idx as u32;
                }
                s.clamp();
                Some(s)
            } else {
                None
            };
            let substrate_effects = None;
            let substrate_tick_counter = 0_u32;
            let substrate_dropout_rng = rand_chacha::ChaCha20Rng::seed_from_u64(
                config
                    .rng_seed
                    .wrapping_add(0x5A5A_5A5A_5A5A_5A5A)
                    .wrapping_add(pop_idx as u64),
            );
            let substrate_plasticity_count = 0_u32;
            let adaptation_ahp = spec
                .adaptation_ahp
                .as_ref()
                .map(|cfg| {
                    crate::adaptation::AdaptationState::new(
                        n,
                        cfg.tau_ms,
                        cfg.gain_mv,
                        cfg.spike_bump,
                    )
                })
                .transpose()
                .map_err(|e| SnnError::StabilityInit {
                    pop_idx,
                    msg: format!("adaptation_ahp: {e}"),
                })?;
            let adaptation_pump = spec
                .adaptation_pump
                .as_ref()
                .map(|cfg| {
                    crate::adaptation::AdaptationState::new(
                        n,
                        cfg.tau_ms,
                        cfg.gain_mv,
                        cfg.spike_bump,
                    )
                })
                .transpose()
                .map_err(|e| SnnError::StabilityInit {
                    pop_idx,
                    msg: format!("adaptation_pump: {e}"),
                })?;
            let basket = spec
                .basket
                .as_ref()
                .map(|cfg| crate::basket::BasketPool::new(pop_idx as u32, n, cfg.fraction))
                .transpose()
                .map_err(|e| SnnError::StabilityInit {
                    pop_idx,
                    msg: format!("basket: {e}"),
                })?;
            // Phase 9a/9b/10 — GPU LIF integrator on this population's
            // assigned device.
            #[cfg(feature = "cuda")]
            let lif_gpu = if let Some(ctx) = pop_ctx[pop_idx].as_ref() {
                Some(
                    crate::lif::LifGpu::new_with_context(ctx.clone(), spec.n_neurons, &spec.lif)
                        .map_err(|e| {
                            SnnError::GpuUnavailable(format!("LifGpu pop {pop_idx}: {e:?}"))
                        })?,
                )
            } else {
                None
            };

            let v_prep_scratch = vec![0.0_f32; n];
            populations.push(Pop {
                spec: spec.clone(),
                state,
                rate,
                i_syn_scratch,
                noise_rng,
                pink_state,
                depression_state,
                adaptation_ahp,
                adaptation_pump,
                basket,
                hyperpol_scratch,
                fired_f32_scratch,
                v_prep_scratch,
                depression_scale_scratch,
                substrate_state,
                substrate_effects,
                substrate_tick_counter,
                substrate_dropout_rng,
                substrate_plasticity_count,
                #[cfg(feature = "cuda")]
                lif_gpu,
            });
        }

        // Reject GPU request when feature not compiled in.
        #[cfg(not(feature = "cuda"))]
        if config.use_gpu_forward {
            return Err(SnnError::GpuUnavailable(
                "crate built without --features cuda".into(),
            ));
        }

        let mut edges: Vec<Edge> = Vec::with_capacity(config.edges.len());
        for (edge_idx, spec) in config.edges.iter().enumerate() {
            if spec.src >= n_pops {
                return Err(SnnError::EdgePopOutOfRange {
                    edge_idx,
                    pop_idx: spec.src,
                    n_pops,
                });
            }
            if spec.dst >= n_pops {
                return Err(SnnError::EdgePopOutOfRange {
                    edge_idx,
                    pop_idx: spec.dst,
                    n_pops,
                });
            }

            let src_pop = &populations[spec.src];
            let dst_pop = &populations[spec.dst];
            let edge_seed = config
                .rng_seed
                .wrapping_add(edge_idx as u64)
                .wrapping_mul(0x9E37_79B9_7F4A_7C15);

            let csr = CsrSynapses::random_uniform_seeded(
                PopulationId(spec.src as u32),
                PopulationId(spec.dst as u32),
                src_pop.spec.n_neurons,
                dst_pop.spec.n_neurons,
                spec.fan_in,
                spec.weight_init,
                spec.weight_jitter,
                edge_seed,
            );

            let rstdp = RstdpState::new(
                csr.weights.len(),
                src_pop.spec.n_neurons,
                dst_pop.spec.n_neurons,
            );
            let i_syn_scratch = vec![0.0_f32; dst_pop.spec.n_neurons as usize];

            // Phase 9a/9b/10 — GPU CSR forward on the destination
            // population's device (i_syn is per destination neuron).
            #[cfg(feature = "cuda")]
            let csr_gpu = if let Some(ctx) = pop_ctx[spec.dst].as_ref() {
                Some(
                    crate::csr::CsrGpu::new_with_context(ctx.clone(), &csr).map_err(|e| {
                        SnnError::GpuUnavailable(format!("CsrGpu edge {edge_idx}: {e:?}"))
                    })?,
                )
            } else {
                None
            };
            // Phase 9d — GPU R-STDP, same device as the edge's CSR.
            #[cfg(feature = "cuda")]
            let rstdp_gpu = if let Some(ctx) = pop_ctx[spec.dst].as_ref() {
                Some(
                    crate::rstdp::RstdpGpu::new_with_context(ctx.clone(), &csr).map_err(|e| {
                        SnnError::GpuUnavailable(format!("RstdpGpu edge {edge_idx}: {e:?}"))
                    })?,
                )
            } else {
                None
            };

            edges.push(Edge {
                spec: spec.clone(),
                csr,
                rstdp,
                i_syn_scratch,
                #[cfg(feature = "cuda")]
                csr_gpu,
                #[cfg(feature = "cuda")]
                rstdp_gpu,
                cb_g_exc_scratch: Vec::new(),
                cb_g_inh_scratch: Vec::new(),
            });
        }

        // Thalamic channel — constructed from optional network-level cfg.
        let (thalamic_channel, thalamic_submit_threshold) = match config.thalamic.as_ref() {
            Some(cfg) => {
                let mut ch = nimcp_thalamic::ThalamicChannel::new(cfg.source_id, &cfg.destinations)
                    .ok_or_else(|| SnnError::StabilityInit {
                        pop_idx: 0,
                        msg: format!(
                            "thalamic: too many destinations ({} > {})",
                            cfg.destinations.len(),
                            nimcp_thalamic::THALAMIC_MAX_DESTINATIONS
                        ),
                    })?;
                ch.mode = cfg.mode;
                (Some(ch), cfg.submit_threshold.clamp(0.0, 1.0))
            }
            None => (None, 0.0),
        };

        let rstdp_ref = edges
            .iter()
            .map(|e| (e.spec.rstdp.stdp.a_plus, e.spec.rstdp.stdp.a_minus))
            .collect();
        Ok(Self {
            populations,
            edges,
            t_ms: 0.0,
            reward_coupled_homeostatic: config.reward_coupled_homeostatic,
            thalamic_channel,
            thalamic_submit_threshold,
            cb_weights_rescaled: false,
            pop_device,
            #[cfg(feature = "cuda")]
            gpu,
            rstdp_lr: TunableState::snapshot(RSTDP_LR_BASELINE),
            rstdp_ref,
            unapplied_tunables: std::collections::BTreeMap::new(),
        })
    }

    // -------------------------------------------------------------------------
    // Live tuning + quench (V1 watchdog RPC parity: snn_tune / snn_tune_get /
    // snn_force_quench).
    // -------------------------------------------------------------------------

    /// Current value of every knob in [`TUNABLE_NAMES`]. Population-level
    /// knobs report population 0 (V1 tunes them network-wide, and `tune`
    /// writes all populations). Recorded-only knobs report the last value
    /// set, or V1's default before any `tune`.
    #[must_use]
    pub fn tunables(&self) -> std::collections::BTreeMap<String, f32> {
        let mut m = std::collections::BTreeMap::new();
        m.insert("rstdp_lr".into(), self.rstdp_lr.current);
        if let Some(p0) = self.populations.first() {
            m.insert("homeo_min_scale".into(), p0.spec.homeostatic.min_scale);
            m.insert("homeo_max_scale".into(), p0.spec.homeostatic.max_scale);
            m.insert("target_rate".into(), p0.spec.target_rate);
            m.insert("noise_rate_hz".into(), p0.spec.noise.rate_hz);
            m.insert("noise_pulse_mv".into(), p0.spec.noise.pulse_mv);
        }
        for (name, default) in [
            ("max_scale_dead", 1.05_f32),
            ("dead_threshold", 0.1),
            ("rstdp_baseline_alpha", 0.001),
        ] {
            let v = self
                .unapplied_tunables
                .get(name)
                .copied()
                .unwrap_or(default);
            m.insert(name.into(), v);
        }
        m
    }

    /// Set a live-tunable knob network-wide. See [`TUNABLE_NAMES`] for the
    /// vocabulary and what each knob does on V2. Takes effect on the next
    /// `step()`; GPU paths read these params per launch, so no re-upload
    /// is needed.
    ///
    /// # Errors
    ///
    /// [`SnnError::UnknownTunable`] for a name outside the vocabulary;
    /// [`SnnError::InvalidTunable`] for a non-finite or out-of-range value.
    pub fn tune(&mut self, name: &str, value: f32) -> Result<TuneOutcome, SnnError> {
        if !TUNABLE_NAMES.contains(&name) {
            return Err(SnnError::UnknownTunable(name.to_string()));
        }
        let invalid = |reason: &'static str| SnnError::InvalidTunable {
            name: name.to_string(),
            value,
            reason,
        };
        if !value.is_finite() {
            return Err(invalid("must be finite"));
        }
        let n_pops = self.populations.len();
        let n_edges = self.edges.len();
        let ok = |applied: bool, pops: usize, edges: usize, note: Option<String>| {
            Ok(TuneOutcome {
                name: name.to_string(),
                value,
                applied,
                populations: pops,
                edges,
                note,
            })
        };
        match name {
            "rstdp_lr" => {
                if value < 0.0 {
                    return Err(invalid("must be >= 0"));
                }
                let scale = value / RSTDP_LR_BASELINE;
                for (edge, &(a_plus, a_minus)) in self.edges.iter_mut().zip(&self.rstdp_ref) {
                    edge.spec.rstdp.stdp.a_plus = a_plus * scale;
                    edge.spec.rstdp.stdp.a_minus = a_minus * scale;
                }
                self.rstdp_lr.set_current(value);
                ok(true, 0, n_edges, None)
            }
            "homeo_min_scale" => {
                if !(0.0 < value && value <= 1.0) {
                    return Err(invalid("must be in (0, 1]"));
                }
                for p in &mut self.populations {
                    p.spec.homeostatic.min_scale = value;
                }
                ok(true, n_pops, 0, None)
            }
            "homeo_max_scale" => {
                if value < 1.0 {
                    return Err(invalid("must be >= 1"));
                }
                for p in &mut self.populations {
                    p.spec.homeostatic.max_scale = value;
                }
                ok(true, n_pops, 0, None)
            }
            "target_rate" => {
                if !(0.0 < value && value <= 1.0) {
                    return Err(invalid("must be in (0, 1] (fraction spiking per step)"));
                }
                for p in &mut self.populations {
                    p.spec.target_rate = value;
                    p.rate.target_rate = value;
                }
                ok(true, n_pops, 0, None)
            }
            "noise_rate_hz" => {
                if !(0.0..=crate::noise::NOISE_RATE_HZ_MAX).contains(&value) {
                    return Err(invalid("must be in [0, NOISE_RATE_HZ_MAX]"));
                }
                for p in &mut self.populations {
                    p.spec.noise.rate_hz = value;
                }
                ok(true, n_pops, 0, None)
            }
            "noise_pulse_mv" => {
                if !(0.0..=crate::noise::NOISE_PULSE_MV_MAX).contains(&value) {
                    return Err(invalid("must be in [0, NOISE_PULSE_MV_MAX]"));
                }
                for p in &mut self.populations {
                    p.spec.noise.pulse_mv = value;
                }
                ok(true, n_pops, 0, None)
            }
            _ => {
                // max_scale_dead / dead_threshold / rstdp_baseline_alpha
                self.unapplied_tunables.insert(name.to_string(), value);
                ok(
                    false,
                    0,
                    0,
                    Some(format!(
                        "`{name}` recorded but not applied: V2's homeostatic step has no \
                         dead-population scale override and its R-STDP uses a constant \
                         baseline_reward, not an EMA"
                    )),
                )
            }
        }
    }

    /// Emergency quench (V1 `snn_force_quench`): in every population, pull
    /// the first `n` neurons' membrane voltage back to `v_reset`
    /// (biophysically a forced reset; V1 zeroed in its normalised units).
    /// Returns the number of neurons quenched across all populations.
    /// On the GPU LIF path the device-resident `v_mem` is rewritten too
    /// (CPU `state.v_mem` is not the source of truth there).
    ///
    /// # Errors
    ///
    /// [`SnnError::GpuUnavailable`] if a device write fails.
    pub fn force_quench(&mut self, n: u32) -> Result<usize, SnnError> {
        let mut total = 0usize;
        for pop in &mut self.populations {
            let k = (n as usize).min(pop.state.v_mem.len());
            if k == 0 {
                continue;
            }
            let v_reset = pop.spec.lif.v_reset;
            pop.state.v_mem[..k].fill(v_reset);
            pop.state.spike[..k].fill(0);
            #[cfg(feature = "cuda")]
            if let Some(gpu) = pop.lif_gpu.as_mut() {
                gpu.quench_prefix(k, v_reset)
                    .map_err(|e| SnnError::GpuUnavailable(format!("quench: {e:?}")))?;
            }
            total += k;
        }
        Ok(total)
    }

    /// Device ordinal each population (and the edges into it) was
    /// assigned to. All zeros unless `config.device_assignment` spread
    /// them.
    #[must_use]
    pub fn population_devices(&self) -> &[u32] {
        &self.pop_device
    }

    /// Block until every device this network uses has drained its
    /// work. No-op when the network runs on CPU.
    #[cfg(feature = "cuda")]
    pub fn synchronize_gpus(&self) -> Result<(), nimcp_gpu::GpuError> {
        match self.gpu.as_ref() {
            Some(g) => g.synchronize_all(),
            None => Ok(()),
        }
    }

    /// Read-only access to the network's thalamic channel, if configured.
    /// Returns `None` on brains built without `config.thalamic`.
    #[must_use]
    pub fn thalamic_channel(&self) -> Option<&nimcp_thalamic::ThalamicChannel> {
        self.thalamic_channel.as_ref()
    }

    /// Mutable access to the thalamic channel — for the brain layer to
    /// push attention updates from a router broadcast.
    pub fn thalamic_channel_mut(&mut self) -> Option<&mut nimcp_thalamic::ThalamicChannel> {
        self.thalamic_channel.as_mut()
    }

    /// Compute the effective output gain from this SNN to a given
    /// destination, using the supplied router's Hebbian weights.
    /// Returns `1.0` (no modulation) when the SNN has no thalamic
    /// channel OR the destination isn't registered — preserves the
    /// disable-path contract.
    #[must_use]
    pub fn thalamic_output_gain(
        &self,
        router: &nimcp_thalamic::ThalamicRouter,
        dest_id: u32,
    ) -> f32 {
        let Some(ch) = self.thalamic_channel.as_ref() else {
            return 1.0;
        };
        // Router exposes effective_gain (attention × Hebbian). If the
        // source isn't registered with this router, fall back to the
        // channel's own attention weight.
        let router_gain = router.effective_gain(ch.source_id, dest_id);
        if router_gain == 0.0 {
            // Likely: router doesn't have a channel open for this source.
            // Use the SNN-owned channel's attention weight alone.
            ch.get_gate(dest_id)
        } else {
            router_gain
        }
    }

    /// Number of populations.
    #[must_use]
    pub fn n_populations(&self) -> usize {
        self.populations.len()
    }

    /// Number of directed edges.
    #[must_use]
    pub fn n_edges(&self) -> usize {
        self.edges.len()
    }

    /// Read-only view of a population's current spike buffer.
    #[must_use]
    pub fn spikes(&self, pop_idx: usize) -> &[u8] {
        &self.populations[pop_idx].state.spike
    }

    /// Read-only view of a population's current membrane voltage vector.
    #[must_use]
    pub fn v_mem(&self, pop_idx: usize) -> &[f32] {
        &self.populations[pop_idx].state.v_mem
    }

    /// Read-only view of a population's firing-rate EMA.
    #[must_use]
    pub fn rate_ema(&self, pop_idx: usize) -> f32 {
        self.populations[pop_idx].rate.rate()
    }

    /// Online 1/f spectrum health check.
    ///
    /// For a population running [`crate::noise::NoiseKind::Pink`], runs
    /// `nimcp_fractal::dfa_alpha(&ring, 8, 256)` over the rolling ring
    /// buffer of recent pink-noise samples and returns the DFA scaling
    /// exponent α. Expected values:
    ///
    /// - α ≈ 1.0 → 1/f noise (the target).
    /// - α ≈ 0.5 → white noise — generator is misbehaving.
    /// - α ≈ 1.5 → Brownian — generator is integrating something.
    ///
    /// Returns `None` when the population does not use pink noise OR
    /// when its ring buffer has not yet filled to
    /// [`crate::noise::PINK_RING_CAPACITY`] samples.
    #[must_use]
    pub fn pink_alpha(&self, pop_idx: usize) -> Option<f32> {
        let pop = self.populations.get(pop_idx)?;
        let pink = pop.pink_state.as_ref()?;
        if !pink.is_full() {
            return None;
        }
        // VecDeque may be non-contiguous — copy to a flat slice. 1024
        // f32 samples = 4 KiB, negligible vs. DFA cost itself.
        let ring: Vec<f32> = pink.ring.iter().copied().collect();
        nimcp_fractal::dfa_alpha(&ring, 8, 256).ok()
    }

    /// Mutable view of a single edge's weights. For tests and operator
    /// preloads.
    pub fn edge_weights_mut(&mut self, edge_idx: usize) -> &mut [f32] {
        &mut self.edges[edge_idx].csr.weights
    }

    /// Read-only edge weights.
    #[must_use]
    pub fn edge_weights(&self, edge_idx: usize) -> &[f32] {
        &self.edges[edge_idx].csr.weights
    }

    /// Simulated time (ms).
    #[must_use]
    pub fn t_ms(&self) -> f32 {
        self.t_ms
    }

    /// Network-wide CB rescale flag — set to `true` after
    /// [`Self::rescale_for_conductance`] runs successfully. Idempotent
    /// re-runs become a no-op so callers can safely invoke at every
    /// daemon startup.
    #[must_use]
    pub fn cb_weights_rescaled(&self) -> bool {
        self.cb_weights_rescaled
    }

    /// Enable conductance-based PSC dynamics on every population.
    ///
    /// In-place — installs [`ConductanceParams`] (default values) on
    /// each `LifParams` and allocates `g_exc`/`g_inh` state buffers.
    /// Idempotent: re-running on a CB-enabled network is a no-op.
    ///
    /// **Important:** weights must be rescaled before CB dynamics
    /// behave like the previous current-mode regime — call
    /// [`Self::rescale_for_conductance`] once after this. The
    /// network's `cb_weights_rescaled` sticky flag tracks whether
    /// that's happened.
    pub fn enable_conductance(&mut self) {
        for pop in &mut self.populations {
            if pop.spec.lif.conductance.is_none() {
                pop.spec.lif.conductance = Some(crate::lif::ConductanceParams::default());
            }
            pop.state.enable_conductance_in_place();
        }
    }

    /// Disable conductance-based dynamics on every population — flips
    /// every [`LifParams::conductance`] back to `None`. Conductance
    /// state buffers are dropped to reclaim memory. Idempotent.
    ///
    /// Note: does **not** rescale weights back; callers that need to
    /// undo the CB-enable rescale should multiply the inverse factor.
    /// `cb_weights_rescaled` is reset to `false`.
    pub fn disable_conductance(&mut self) {
        for pop in &mut self.populations {
            pop.spec.lif.conductance = None;
            pop.state.conductance = None;
        }
        self.cb_weights_rescaled = false;
    }

    /// One-shot rescale of every CSR edge's weights by `factor` (typically
    /// [`crate::SNN_CB_WEIGHT_SCALE`] = `1/50`) to compensate for the
    /// ~50 mV avg driving force introduced by switching to CB mode.
    ///
    /// **Idempotent**: if `cb_weights_rescaled` is already `true`,
    /// returns without rescaling. This makes `_load_persistent_snn_tunes`
    /// patterns safe — V1 commit `f79764c9b` learned this lesson the
    /// hard way (the daemon force-rescaled on every startup, doubling
    /// the rescale on every save+load cycle).
    ///
    /// Returns `true` if the rescale was applied this call, `false`
    /// if it was a no-op (already rescaled).
    pub fn rescale_for_conductance(&mut self, factor: f32) -> bool {
        if self.cb_weights_rescaled {
            return false;
        }
        for edge in &mut self.edges {
            edge.csr.scale_all_weights(factor);
            // GPU mirror, if present, will be re-uploaded on the next
            // current-mode step. CB mode forces CPU fallback so the
            // mirror going stale doesn't matter for CB pops.
        }
        self.cb_weights_rescaled = true;
        true
    }

    /// One integration step.
    ///
    /// `external_i_syn` is indexed by population. Pass an empty outer
    /// slice for no drive. Each inner slice must match the population's
    /// neuron count or it is silently ignored.
    ///
    /// Returns total spikes emitted across all populations this step.
    pub fn step(&mut self, external_i_syn: &[&[f32]], reward: f32, dt_ms: f32) -> u32 {
        // 0. Substrate effect cadence — recompute cached (axon, dendrite)
        //    effects every `update_period` steps for each substrate-
        //    enabled population. Reset per-step plasticity count too.
        for pop in self.populations.iter_mut() {
            if !pop.spec.substrate.enabled {
                continue;
            }
            pop.substrate_plasticity_count = 0;
            let should_recompute = pop.substrate_effects.is_none()
                || pop.substrate_tick_counter >= pop.spec.substrate.update_period;
            if should_recompute {
                if let Some(ref s) = pop.substrate_state {
                    pop.substrate_effects = Some(nimcp_substrate::compute_effects(s));
                    pop.substrate_tick_counter = 0;
                }
            } else {
                pop.substrate_tick_counter = pop.substrate_tick_counter.saturating_add(1);
            }
        }

        // 1. Per-pop prep: zero scratches, inject external drive, apply
        //    Poisson noise and basket inhibition, refresh per-source
        //    depression scale. Adaptation hyperpol is subtracted from
        //    v_mem directly so the LIF step sees the post-adaptation
        //    membrane.
        //
        //    CB-mode pops use conductance state for synaptic input —
        //    `i_syn_scratch` is still touched (for non-synaptic things
        //    like adaptation/noise that V1 keeps current-style) but
        //    edge weights deposit into `state.conductance.g_exc/g_inh`
        //    via `i_syn_cpu_cb_with_pre_scale` in step #2 below.
        //    External drive in CB mode routes through the CB deposit
        //    helper just like a synaptic input — positive→g_exc,
        //    negative→g_inh.
        for (pop_idx, pop) in self.populations.iter_mut().enumerate() {
            let cb_mode = pop.spec.lif.conductance.is_some() && pop.state.conductance.is_some();

            // Zero I_syn scratch (still used for non-synaptic terms in CB
            // mode; for current mode it accumulates synaptic input).
            for v in pop.i_syn_scratch.iter_mut() {
                *v = 0.0;
            }
            // External drive — current mode writes to i_syn_scratch;
            // CB mode routes by sign into the persistent g_exc/g_inh
            // state (added to whatever residual the prior step left).
            if let Some(ext) = external_i_syn.get(pop_idx)
                && ext.len() == pop.i_syn_scratch.len()
            {
                if cb_mode {
                    let cb = pop
                        .state
                        .conductance
                        .as_mut()
                        .expect("cb_mode implies conductance state present");
                    for ((g_exc, g_inh), &src) in
                        cb.g_exc.iter_mut().zip(cb.g_inh.iter_mut()).zip(ext.iter())
                    {
                        if src >= 0.0 {
                            *g_exc += src;
                        } else {
                            *g_inh += -src;
                        }
                    }
                } else {
                    for (dst, &src) in pop.i_syn_scratch.iter_mut().zip(ext.iter()) {
                        *dst = src;
                    }
                }
            }

            // Basket inhibition (uses PRIOR-step basket spike_output —
            // first step is a no-op because spike_output is all zeros).
            // CB mode: basket emits into i_syn_scratch first then we
            // route by sign into g_inh (basket is purely inhibitory so
            // contributions are <= 0). Use a temporary write into
            // i_syn_scratch then sweep into g_inh; this matches V1's
            // pattern of reusing existing emission code via
            // deposit_synapse.
            if let Some(bp) = &pop.basket {
                bp.emit_inhibition(&mut pop.i_syn_scratch);
                if cb_mode {
                    let cb = pop
                        .state
                        .conductance
                        .as_mut()
                        .expect("cb_mode implies conductance state present");
                    for (g_inh, src) in cb.g_inh.iter_mut().zip(pop.i_syn_scratch.iter_mut()) {
                        if *src < 0.0 {
                            *g_inh += -*src;
                            *src = 0.0;
                        }
                    }
                }
            }

            // Phase 9e — accumulate every v_mem-direct prep contribution
            // (noise + adaptation hyperpol) into `v_prep_scratch` instead
            // of mutating `state.v_mem` in place. CPU LIF applies it
            // before integration; GPU LIF uploads it as a kernel arg.
            // This unblocks GPU LIF on stability-on populations.
            for x in pop.v_prep_scratch.iter_mut() {
                *x = 0.0;
            }

            // Adaptive-factor background noise into v_prep. Dead pops
            // get full injection; at-target pops get none. Branches on
            // the configured [`crate::noise::NoiseKind`]:
            //   - Poisson → per-neuron Bernoulli pulses
            //   - Pink    → one shared 1/f sample per step, also recorded
            //     into the population's ring buffer for DFA monitoring.
            let factor = crate::noise::noise_factor_for_pop(pop.rate.rate(), pop.spec.target_rate);
            match pop.spec.noise.kind {
                crate::noise::NoiseKind::Poisson => {
                    crate::noise::inject_poisson_noise(
                        &mut pop.noise_rng,
                        &mut pop.v_prep_scratch,
                        &pop.spec.noise,
                        dt_ms,
                        factor,
                    );
                }
                crate::noise::NoiseKind::Pink { .. } => {
                    if let Some(pink) = pop.pink_state.as_mut() {
                        crate::noise::inject_pink_noise(
                            pink,
                            &mut pop.v_prep_scratch,
                            pop.spec.noise.pulse_mv,
                            dt_ms,
                            factor,
                        );
                    }
                }
            }

            // Adaptation hyperpol: subtract gain*adapt_var from v_prep.
            // AHP and pump share the scratch buffer to avoid an extra
            // allocation per step.
            let n = pop.state.v_mem.len();
            if pop.adaptation_ahp.is_some() || pop.adaptation_pump.is_some() {
                for x in pop.hyperpol_scratch.iter_mut().take(n) {
                    *x = 0.0;
                }
                if let Some(a) = &pop.adaptation_ahp {
                    // Fill scratch with AHP contribution.
                    a.compute_hyperpol(&mut pop.hyperpol_scratch);
                }
                if let Some(a) = &pop.adaptation_pump {
                    // Add pump contribution on top. Use a temporary so
                    // we don't overwrite AHP.
                    let mut tmp = vec![0.0; n];
                    a.compute_hyperpol(&mut tmp);
                    for (h, &t) in pop.hyperpol_scratch.iter_mut().zip(tmp.iter()) {
                        *h += t;
                    }
                }
                for (vp, &h) in pop
                    .v_prep_scratch
                    .iter_mut()
                    .zip(pop.hyperpol_scratch.iter())
                {
                    *vp -= h;
                }
            }

            // Refresh per-source depression scale from depression_state.
            let cap = pop.spec.depression.cap;
            for (scale, &d) in pop
                .depression_scale_scratch
                .iter_mut()
                .zip(pop.depression_state.d.iter())
            {
                *scale = (1.0 - d).clamp(1.0 - cap, 1.0);
            }
        }

        // 2. Forward every CSR edge, applying per-source depression scale.
        //    Destination CB-mode pops receive sign-routed deltas into
        //    `state.conductance.{g_exc, g_inh}`; current-mode pops keep
        //    the legacy `i_syn_scratch` accumulation. The CSR forward
        //    output target is decided per-edge based on the destination
        //    population's mode.
        for edge in &mut self.edges {
            let src = edge.spec.src;
            let dst = edge.spec.dst;
            debug_assert_ne!(src, dst, "self-loops unsupported in Phase 3");

            let dst_cb_mode = self.populations[dst].spec.lif.conductance.is_some()
                && self.populations[dst].state.conductance.is_some();

            let src_spikes = &self.populations[src].state.spike;
            // Depression scale — only supply when STD actually in play,
            // so the non-depression hot path stays at v1 perf.
            let pre_scale: Option<&[f32]> = if self.populations[src].spec.depression.is_disabled() {
                None
            } else {
                Some(&self.populations[src].depression_scale_scratch)
            };

            if dst_cb_mode {
                // CB path: split per-post into g_exc / g_inh deltas.
                // GPU CB is deferred (V1 design lock); always CPU.
                let n_post = self.populations[dst].spec.n_neurons as usize;
                if edge.cb_g_exc_scratch.len() != n_post {
                    edge.cb_g_exc_scratch.resize(n_post, 0.0);
                    edge.cb_g_inh_scratch.resize(n_post, 0.0);
                }
                edge.csr.i_syn_cpu_cb_with_pre_scale(
                    src_spikes,
                    pre_scale,
                    &mut edge.cb_g_exc_scratch,
                    &mut edge.cb_g_inh_scratch,
                );
                continue;
            }

            // Current-mode path (unchanged from pre-CB).
            // Phase 9a — GPU path when CsrGpu present. STD pre-scale
            // isn't wired on GPU yet (substrate/depression/basket stay
            // CPU); fall back to CPU when a scale vector is supplied.
            #[cfg(feature = "cuda")]
            let used_gpu = if pre_scale.is_none() {
                if let Some(csr_gpu) = edge.csr_gpu.as_mut() {
                    // Re-upload weights each step — homeostatic + R-STDP
                    // wrote CPU-side; GPU mirror must catch up.
                    // In 9b this sync goes away when plasticity runs
                    // on device.
                    csr_gpu
                        .upload_weights(&edge.csr.weights)
                        .map_err(|e| {
                            tracing::warn!(?e, "CsrGpu upload_weights failed; falling back to CPU");
                            e
                        })
                        .ok();
                    csr_gpu
                        .i_syn(src_spikes, &mut edge.i_syn_scratch)
                        .map_err(|e| {
                            tracing::warn!(?e, "CsrGpu i_syn failed; falling back to CPU");
                            e
                        })
                        .is_ok()
                } else {
                    false
                }
            } else {
                false
            };
            #[cfg(not(feature = "cuda"))]
            let used_gpu = false;

            if !used_gpu {
                edge.csr
                    .i_syn_cpu_with_pre_scale(src_spikes, pre_scale, &mut edge.i_syn_scratch);
            }
        }
        for edge in &self.edges {
            let dst_pop = &mut self.populations[edge.spec.dst];
            let dst_cb_mode =
                dst_pop.spec.lif.conductance.is_some() && dst_pop.state.conductance.is_some();
            if dst_cb_mode {
                let cb = dst_pop
                    .state
                    .conductance
                    .as_mut()
                    .expect("dst_cb_mode implies conductance state present");
                for ((g_exc, g_inh), (&de, &di)) in
                    cb.g_exc.iter_mut().zip(cb.g_inh.iter_mut()).zip(
                        edge.cb_g_exc_scratch
                            .iter()
                            .zip(edge.cb_g_inh_scratch.iter()),
                    )
                {
                    *g_exc += de;
                    *g_inh += di;
                }
            } else {
                for (d, &s) in dst_pop
                    .i_syn_scratch
                    .iter_mut()
                    .zip(edge.i_syn_scratch.iter())
                {
                    *d += s;
                }
            }
        }

        // 3. Advance LIF dynamics. Substrate-enabled populations use
        //    effective params (tau / threshold / refractory modulated).
        for pop in self.populations.iter_mut() {
            let effective = crate::substrate_adapter::effective_lif(
                &pop.spec.lif,
                pop.substrate_effects.as_ref(),
            );

            // Phase 9a/9e — GPU LIF when LifGpu present. Phase 9e folds
            // the prior cpu_prep_modified_v_mem guard into a single
            // `v_prep` arg the kernel adds to v_mem, so noise +
            // adaptation populations can run on GPU. CB mode and
            // substrate state-mutating prep still force CPU fallback.
            //
            // CB mode CPU-only contract: conductance integrator stays
            // on CPU (per V1 design lock — `lif_step_cpu` dispatches
            // to `lif_step_cpu_cb` automatically when params + state
            // agree).
            let cpu_only = pop.spec.lif.conductance.is_some() || pop.spec.substrate.enabled;
            #[cfg(feature = "cuda")]
            let used_gpu = if !cpu_only && let Some(lif_gpu) = pop.lif_gpu.as_mut() {
                let mut spikes_buf: Vec<u8> = Vec::with_capacity(pop.state.spike.len());
                match lif_gpu.step_with_prep(
                    &pop.i_syn_scratch,
                    &pop.v_prep_scratch,
                    &mut spikes_buf,
                    &effective,
                    dt_ms,
                ) {
                    Ok(()) => {
                        let n = spikes_buf.len().min(pop.state.spike.len());
                        pop.state.spike[..n].copy_from_slice(&spikes_buf[..n]);
                        true
                    }
                    Err(e) => {
                        tracing::warn!(?e, "LifGpu::step_with_prep failed; falling back to CPU");
                        false
                    }
                }
            } else {
                false
            };
            #[cfg(not(feature = "cuda"))]
            let used_gpu = false;
            #[cfg(not(feature = "cuda"))]
            let _ = cpu_only; // silence unused warning

            if !used_gpu {
                // Apply the v_prep delta to v_mem before the integrator
                // sees it (mirrors the GPU kernel's inline add).
                for (v, &p) in pop.state.v_mem.iter_mut().zip(pop.v_prep_scratch.iter()) {
                    *v += p;
                }
                lif_step_cpu(&mut pop.state, &pop.i_syn_scratch, &effective, dt_ms);
            }

            // Substrate emergency silence + probabilistic spike dropout.
            if pop.spec.substrate.enabled {
                if crate::substrate_adapter::emergency_silence(
                    pop.substrate_effects.as_ref(),
                    pop.spec.substrate.emergency_silence_threshold,
                ) {
                    // Chemistry stress — zero every spike this step.
                    for s in pop.state.spike.iter_mut() {
                        *s = 0;
                    }
                } else if pop.spec.substrate.spike_dropout_on {
                    if let Some((axon, _)) = pop.substrate_effects {
                        let reliability = axon.spike_reliability.clamp(0.0, 1.0);
                        if reliability < 1.0 {
                            use rand::Rng;
                            for s in pop.state.spike.iter_mut() {
                                if *s != 0
                                    && pop.substrate_dropout_rng.random::<f32>() > reliability
                                {
                                    *s = 0;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 4. Post-LIF: update adaptation + depression + basket using
        //    the fresh spike output.
        for pop in self.populations.iter_mut() {
            // Build f32 fired buffer once per pop.
            for (f, &s) in pop.fired_f32_scratch.iter_mut().zip(pop.state.spike.iter()) {
                *f = if s != 0 { 1.0 } else { 0.0 };
            }
            if let Some(a) = pop.adaptation_ahp.as_mut() {
                a.update(&pop.fired_f32_scratch, dt_ms);
            }
            if let Some(a) = pop.adaptation_pump.as_mut() {
                a.update(&pop.fired_f32_scratch, dt_ms);
            }
            if !pop.spec.depression.is_disabled() {
                crate::depression::step_depression(
                    &mut pop.depression_state,
                    &pop.spec.depression,
                    &pop.fired_f32_scratch,
                    dt_ms,
                );
            }
            if let Some(bp) = pop.basket.as_mut() {
                let n = pop.state.n_neurons as usize;
                let mean_fire = if n == 0 {
                    0.0
                } else {
                    pop.state.n_spikes_this_step() as f32 / n as f32
                };
                bp.step(mean_fire, dt_ms);
            }
        }

        // 5. R-STDP weight updates. Destination pop's substrate
        //    modulates the effective LR + Ca-handling scales reward.
        for edge in &mut self.edges {
            let src = edge.spec.src;
            let dst = edge.spec.dst;
            let dst_effects = self.populations[dst].substrate_effects.as_ref().copied();
            let dst_plasticity_mod_on = self.populations[dst].spec.substrate.enabled
                && self.populations[dst].spec.substrate.plasticity_mod_on;
            let dst_reward_mod_on = self.populations[dst].spec.substrate.enabled
                && self.populations[dst].spec.substrate.reward_mod_on;
            let eff_rstdp = crate::substrate_adapter::effective_rstdp(
                &edge.spec.rstdp,
                dst_effects.as_ref(),
                dst_plasticity_mod_on,
            );
            let eff_reward = crate::substrate_adapter::effective_reward(
                reward,
                dst_effects.as_ref(),
                dst_reward_mod_on,
            );
            let (pre_spikes, post_spikes): (&[u8], &[u8]) = if src < dst {
                let (a, b) = self.populations.split_at(dst);
                (&a[src].state.spike, &b[0].state.spike)
            } else {
                let (a, b) = self.populations.split_at(src);
                (&b[0].state.spike, &a[dst].state.spike)
            };
            // Phase 9d — GPU R-STDP when RstdpGpu present. Falls back
            // to CPU on any device error so a single bad step does not
            // halt the network.
            #[cfg(feature = "cuda")]
            let used_gpu = if let Some(rstdp_gpu) = edge.rstdp_gpu.as_mut() {
                // Sync CPU-side weight updates from homeostatic / quiet-
                // start back to device before plasticity runs.
                let upload_ok = rstdp_gpu
                    .upload_weights(&edge.csr.weights)
                    .map_err(|e| {
                        tracing::warn!(?e, "RstdpGpu upload_weights failed; falling back to CPU");
                        e
                    })
                    .is_ok();
                if upload_ok {
                    match rstdp_gpu.step(
                        &mut edge.rstdp,
                        pre_spikes,
                        post_spikes,
                        eff_reward,
                        self.t_ms,
                        dt_ms,
                        &eff_rstdp,
                    ) {
                        Ok(_n_moved) => {
                            // Mirror device weights back to CPU so the
                            // next forward + homeostatic pass sees them.
                            // 9d intentionally keeps this sync — Phase
                            // 9 closeout removes it once homeostatic +
                            // forward also live on device.
                            let mut host_w = std::mem::take(&mut edge.csr.weights);
                            let download_ok = rstdp_gpu
                                .download_weights(&mut host_w)
                                .map_err(|e| {
                                    tracing::warn!(?e, "RstdpGpu download_weights failed");
                                    e
                                })
                                .is_ok();
                            edge.csr.weights = host_w;
                            download_ok
                        }
                        Err(e) => {
                            tracing::warn!(?e, "RstdpGpu::step failed; falling back to CPU");
                            false
                        }
                    }
                } else {
                    false
                }
            } else {
                false
            };
            #[cfg(not(feature = "cuda"))]
            let used_gpu = false;

            if !used_gpu {
                step_rstdp(
                    &mut edge.csr,
                    &mut edge.rstdp,
                    pre_spikes,
                    post_spikes,
                    eff_reward,
                    self.t_ms,
                    dt_ms,
                    &eff_rstdp,
                );
            }
            // Count plasticity updates for substrate debit: approximate
            // as one update per post-spike (an eligibility window per
            // post-spike triggers weight writes).
            let n_post_spikes: u32 = post_spikes.iter().map(|&s| u32::from(s != 0)).sum();
            self.populations[dst].substrate_plasticity_count = self.populations[dst]
                .substrate_plasticity_count
                .saturating_add(n_post_spikes);
        }

        // 6. Homeostatic scaling per destination population — reward-
        //    coupled variant: widens scale-down when reward is stuck
        //    near zero AND rate is saturated.
        for edge in &mut self.edges {
            let dst_pop = &mut self.populations[edge.spec.dst];
            let n_post = dst_pop.state.n_neurons as usize;
            let fraction_spiking = if n_post == 0 {
                0.0
            } else {
                dst_pop.state.n_spikes_this_step() as f32 / n_post as f32
            };
            if self.reward_coupled_homeostatic {
                let _ = crate::homeostatic::step_homeostatic_with_reward(
                    &mut edge.csr,
                    &mut dst_pop.rate,
                    fraction_spiking,
                    &dst_pop.spec.homeostatic,
                    reward,
                );
            } else {
                let _ = step_homeostatic(
                    &mut edge.csr,
                    &mut dst_pop.rate,
                    fraction_spiking,
                    &dst_pop.spec.homeostatic,
                );
            }
        }

        // 7. Substrate debit: decrement ATP / ion / membrane based on
        //    this step's spikes + plasticity updates.
        for pop in self.populations.iter_mut() {
            if !pop.spec.substrate.enabled {
                continue;
            }
            let Some(ref mut s) = pop.substrate_state else {
                continue;
            };
            let n_spikes = u64::from(pop.state.n_spikes_this_step());
            let n_plast = u64::from(pop.substrate_plasticity_count);
            nimcp_substrate::debit_activity(s, &pop.spec.substrate.dynamics, n_spikes, n_plast);
        }

        // 8. Advance clock and tally activity.
        self.t_ms += dt_ms;
        let total_spikes: u32 = self
            .populations
            .iter()
            .map(|p| p.state.n_spikes_this_step())
            .sum();

        // 9. Thalamic auto-submit: if the network's fraction-spiking
        //    exceeds submit_threshold, bump the channel's submit
        //    counter. External router.tick() folds this into the
        //    Hebbian update on the caller's cadence.
        if let Some(ch) = self.thalamic_channel.as_mut() {
            #[allow(clippy::cast_precision_loss)]
            let total_neurons: u64 = self
                .populations
                .iter()
                .map(|p| u64::from(p.state.n_neurons))
                .sum();
            if total_neurons > 0 {
                let fraction = total_spikes as f32 / total_neurons as f32;
                if fraction > self.thalamic_submit_threshold {
                    ch.record_submit();
                }
            }
        }

        total_spikes
    }

    /// Compute the network-level intrinsic reward from per-population
    /// firing-rate EMAs and the configured [`IntrinsicRewardConfig`].
    /// Returns the scalar R-STDP teaching signal — pass as the
    /// `reward` argument to [`Self::step`] when running intrinsic-only
    /// training. Combine with external reward additively when needed.
    #[must_use]
    pub fn compute_intrinsic_reward(
        &self,
        cfg: &crate::intrinsic_reward::IntrinsicRewardConfig,
    ) -> f32 {
        crate::intrinsic_reward::compute_network_reward(
            self.populations
                .iter()
                .map(|p| (p.rate.rate(), p.spec.target_rate)),
            cfg,
        )
    }

    /// Load-time weight transform — applied whenever weights may have
    /// come from a saturated source (fresh init with high initial
    /// weights, checkpoint resume, operator paste).
    ///
    /// Per destination population, delegates to
    /// [`nimcp_plasticity::quiet_start_scale`] with the observed rate
    /// and target, then applies the same scale to every incoming edge.
    /// Returns the per-edge scales actually applied.
    pub fn apply_quiet_start(&mut self, observed_rates_per_pop: &[f32]) -> Vec<f32> {
        let mut scales_out = vec![1.0_f32; self.edges.len()];
        let n_pops = self.populations.len();
        for dst_idx in 0..n_pops {
            let target = self.populations[dst_idx].spec.target_rate;
            let observed = observed_rates_per_pop.get(dst_idx).copied().unwrap_or(0.0);
            let scale_vec = nimcp_plasticity::quiet_start_scale(&[observed], target);
            let scale = scale_vec.first().copied().unwrap_or(1.0);

            #[allow(clippy::float_cmp)]
            let is_noop = scale == 1.0;
            for (idx, edge) in self.edges.iter_mut().enumerate() {
                if edge.spec.dst != dst_idx {
                    continue;
                }
                if !is_noop {
                    for w in edge.csr.weights.iter_mut() {
                        *w *= scale;
                    }
                }
                scales_out[idx] = scale;
            }
        }
        scales_out
    }
}

// -------------------------------------------------------------------------
// Serialization — weights + rate EMAs.
// -------------------------------------------------------------------------

/// Weight snapshot for checkpointing.
///
/// `cb_weights_rescaled` is persisted in the snapshot itself — V2's
/// equivalent of V1's `<checkpoint>.cb_rescaled` JSON sidecar
/// (commit `312c9b323`). When the daemon resumes from a checkpoint
/// the load path reads this flag and skips the one-shot weight
/// rescale if `true`, preventing the V1 double-rescale bomb.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WeightSnapshot {
    /// Per-edge weight vectors.
    pub edge_weights: Vec<Vec<f32>>,
    /// Per-pop firing-rate EMAs.
    pub rate_emas: Vec<PopulationRateEma>,
    /// CB rescale sticky flag — `true` iff the snapshotted network
    /// had `cb_weights_rescaled = true`. Defaulting to `false` keeps
    /// older checkpoints (without this field) safe under serde —
    /// they'll trigger a fresh rescale on first CB enable, which is
    /// the conservative behavior.
    #[serde(default)]
    pub cb_weights_rescaled: bool,
}

impl SnnNetwork {
    /// Snapshot weights + rate EMAs + CB rescale flag.
    #[must_use]
    pub fn snapshot(&self) -> WeightSnapshot {
        WeightSnapshot {
            edge_weights: self.edges.iter().map(|e| e.csr.weights.clone()).collect(),
            rate_emas: self.populations.iter().map(|p| p.rate.clone()).collect(),
            cb_weights_rescaled: self.cb_weights_rescaled,
        }
    }

    /// Restore weights + rate EMAs + CB rescale flag. Shapes must match;
    /// returns `false` on mismatch without modifying state.
    pub fn restore(&mut self, snap: &WeightSnapshot) -> bool {
        if snap.edge_weights.len() != self.edges.len()
            || snap.rate_emas.len() != self.populations.len()
        {
            return false;
        }
        for (edge, w) in self.edges.iter().zip(snap.edge_weights.iter()) {
            if edge.csr.weights.len() != w.len() {
                return false;
            }
        }
        for (edge, w) in self.edges.iter_mut().zip(snap.edge_weights.iter()) {
            edge.csr.weights.copy_from_slice(w);
        }
        for (pop, ema) in self.populations.iter_mut().zip(snap.rate_emas.iter()) {
            pop.rate = ema.clone();
        }
        // Restore the CB sticky flag — if checkpoint says weights are
        // already rescaled, subsequent rescale_for_conductance calls
        // will be no-ops. Old checkpoints (no field) deserialize with
        // `false` via #[serde(default)], which triggers a fresh
        // rescale on first CB enable.
        self.cb_weights_rescaled = snap.cb_weights_rescaled;
        true
    }
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::float_cmp)] // exact-equality asserts intentional in round-trip tests
mod tests {
    use super::*;

    /// Build a minimal two-population config. Sizes + fan-in chosen so
    /// saturated weights can drive downstream to threshold given the
    /// LIF `v_rest = -70 / v_thresh = -50` gap of 20 mV.
    fn two_pop_config(
        seed: u64,
        warmup_samples: u32,
        target_rate: f32,
        weight_init: f32,
    ) -> SnnConfig {
        let lif = LifParams::default();
        let n = 128_u32;
        let pop_a = PopulationSpec {
            name: "A".into(),
            n_neurons: n,
            lif,
            target_rate,
            homeostatic: HomeostaticParams::default(),
            noise: crate::noise::NoiseConfig {
                rate_hz: 0.0,
                pulse_mv: 0.0,
                kind: crate::noise::NoiseKind::default(),
            },
            depression: crate::depression::DepressionConfig {
                inc: 0.0,
                ..crate::depression::DepressionConfig::default()
            },
            adaptation_ahp: None,
            adaptation_pump: None,
            basket: None,
            substrate: SnnSubstrateCfg::default(),
        };
        let pop_b = PopulationSpec {
            name: "B".into(),
            n_neurons: n,
            lif,
            target_rate,
            homeostatic: HomeostaticParams::default(),
            noise: crate::noise::NoiseConfig {
                rate_hz: 0.0,
                pulse_mv: 0.0,
                kind: crate::noise::NoiseKind::default(),
            },
            depression: crate::depression::DepressionConfig {
                inc: 0.0,
                ..crate::depression::DepressionConfig::default()
            },
            adaptation_ahp: None,
            adaptation_pump: None,
            basket: None,
            substrate: SnnSubstrateCfg::default(),
        };
        // w_max bumped so a saturated edge can drive the downstream
        // population to fire on a single pre-spike step — V1's
        // production `w_max=1.0` requires hundreds-of-fan-in scale,
        // which these tests deliberately stay below to keep them fast.
        let rstdp = RstdpParams {
            warmup_samples,
            w_max: 10.0,
            ..RstdpParams::default()
        };
        let edge = EdgeSpec {
            src: 0,
            dst: 1,
            fan_in: 64,
            weight_init,
            weight_jitter: 0.0,
            rstdp,
        };
        SnnConfig {
            populations: vec![pop_a, pop_b],
            edges: vec![edge],
            rng_seed: seed,
            rate_ema_alpha: 0.05,
            reward_coupled_homeostatic: false,
            intrinsic_reward: crate::intrinsic_reward::IntrinsicRewardConfig::default(),
            thalamic: None,
            use_gpu_forward: false,
            device_assignment: crate::device::DeviceAssignment::default(),
        }
    }

    #[test]
    fn constructs_and_reports_shape() {
        let cfg = two_pop_config(0, 100, 0.1, 0.5);
        let net = SnnNetwork::new(cfg).expect("build");
        assert_eq!(net.n_populations(), 2);
        assert_eq!(net.n_edges(), 1);
        assert_eq!(net.spikes(0).len(), 128);
        assert_eq!(net.v_mem(1).len(), 128);
    }

    #[test]
    fn rejects_out_of_range_edge() {
        let mut cfg = two_pop_config(0, 100, 0.1, 0.5);
        cfg.edges[0].dst = 5;
        match SnnNetwork::new(cfg) {
            Err(SnnError::EdgePopOutOfRange { pop_idx, .. }) => assert_eq!(pop_idx, 5),
            Err(other) => panic!("expected EdgePopOutOfRange, got {other:?}"),
            Ok(_) => panic!("expected EdgePopOutOfRange, got Ok"),
        }
    }

    #[test]
    fn step_is_deterministic_across_runs() {
        let a_cfg = two_pop_config(0xABCD, 0, 0.1, 0.3);
        let b_cfg = two_pop_config(0xABCD, 0, 0.1, 0.3);
        let mut a = SnnNetwork::new(a_cfg).expect("a");
        let mut b = SnnNetwork::new(b_cfg).expect("b");
        let ext = vec![25.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..40 {
            a.step(&slices, 0.0, 1.0);
            b.step(&slices, 0.0, 1.0);
        }
        assert_eq!(a.edge_weights(0), b.edge_weights(0));
        assert_eq!(a.spikes(1), b.spikes(1));
    }

    #[test]
    fn external_drive_elicits_downstream_spikes() {
        let cfg = two_pop_config(1, 0, 0.1, 1.8);
        let mut net = SnnNetwork::new(cfg).expect("build");

        // Strong suprathreshold drive so pop A fires on (nearly) every
        // step. With LIF leak at `dt/τ = 0.05` and a 20 mV rest→threshold
        // gap, I_syn of a few hundred crosses threshold in one step.
        let ext = vec![1_000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];

        let mut saw_b_spike = false;
        for _ in 0..150 {
            net.step(&slices, 0.0, 1.0);
            if net.spikes(1).iter().any(|&s| s != 0) {
                saw_b_spike = true;
                break;
            }
        }
        assert!(
            saw_b_spike,
            "downstream pop B should spike under saturated drive"
        );
    }

    /// V2_PLAN Phase 3 exit criterion: a saturated network recovers
    /// into the deadband within 100 steps after `apply_quiet_start`.
    #[test]
    fn saturation_recovery() {
        let target = 0.15_f32;
        let cfg = two_pop_config(7, 0, target, 1.8);
        let mut net = SnnNetwork::new(cfg).expect("build");

        // Hand-saturate every weight at w_max (= 10.0 per two_pop_config).
        for w in net.edge_weights_mut(0) {
            *w = 10.0;
        }

        // Strong drive on pop A so it fires almost every step — we need
        // pop B to saturate before we can exercise recovery.
        let ext = vec![1_000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..120 {
            net.step(&slices, 0.0, 1.0);
        }
        let saturated = net.rate_ema(1);
        assert!(
            saturated > 2.0 * target,
            "setup: pop B should be saturated (>2× target); got {saturated}"
        );

        // Apply quiet-start with the observed saturated rate.
        let observed = vec![0.0, saturated];
        let applied = net.apply_quiet_start(&observed);
        assert!(
            applied[0] < 1.0,
            "quiet-start should have scaled edge 0 down; got {}",
            applied[0]
        );

        // Let homeostatic finish the job.
        for _ in 0..100 {
            net.step(&slices, 0.0, 1.0);
        }

        let final_rate = net.rate_ema(1);
        let deadband = HomeostaticParams::default().deadband_frac * target;
        assert!(
            (final_rate - target).abs() <= 2.0 * deadband + 0.05,
            "recovery: pop B rate {final_rate} not within 2×deadband+0.05 of target {target} (deadband={deadband})"
        );
    }

    #[test]
    fn snapshot_restore_round_trip() {
        let cfg = two_pop_config(42, 0, 0.1, 0.4);
        let mut net = SnnNetwork::new(cfg).expect("build");
        for (i, w) in net.edge_weights_mut(0).iter_mut().enumerate() {
            *w = 0.1 + 0.01 * i as f32;
        }
        let snap = net.snapshot();
        for w in net.edge_weights_mut(0) {
            *w = 0.0;
        }
        assert!(net.restore(&snap));
        assert_eq!(net.edge_weights(0)[5], snap.edge_weights[0][5]);
    }

    #[test]
    fn restore_rejects_shape_mismatch() {
        let cfg_a = two_pop_config(0, 0, 0.1, 0.4);
        let cfg_b = two_pop_config(0, 0, 0.1, 0.4);
        let mut a = SnnNetwork::new(cfg_a).expect("a");
        let b = SnnNetwork::new(cfg_b).expect("b");
        let mut snap = b.snapshot();
        snap.edge_weights[0].truncate(1);
        assert!(!a.restore(&snap));
    }

    // ---------------------------------------------------------------
    // Phase 1 Path A — SNN substrate adapter integration tests.
    // ---------------------------------------------------------------

    /// Helper: build a two-pop config and enable substrate on every
    /// population with a given initial ATP level (everything else at
    /// full health).
    fn two_pop_config_with_substrate(atp: f32) -> SnnConfig {
        let mut cfg = two_pop_config(11, 0, 0.05, 0.4);
        for p in cfg.populations.iter_mut() {
            let mut initial = nimcp_substrate::NeuralSubstrate {
                atp_level: atp,
                ..nimcp_substrate::NeuralSubstrate::default()
            };
            initial.clamp();
            p.substrate = SnnSubstrateCfg {
                enabled: true,
                initial_state: initial,
                // Disable passive recovery so debits actually stick
                // visibly across the short test run.
                dynamics: nimcp_substrate::NeuralSubstrateConfig {
                    atp_passive_recovery: 0.0,
                    ..nimcp_substrate::NeuralSubstrateConfig::default()
                },
                ..SnnSubstrateCfg::default()
            };
        }
        cfg
    }

    /// Disable path: a substrate-off network steps identically to a
    /// pre-Phase-1 network — no crash, no NaN, substrate_effects stays
    /// `None` (never populated because enabled=false).
    #[test]
    fn substrate_disable_path_no_effects_cache() {
        let mut net = SnnNetwork::new(two_pop_config(0, 0, 0.1, 0.5)).unwrap();
        let ext = vec![1000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..20 {
            net.step(&slices, 0.0, 1.0);
        }
        assert!(net.populations[0].substrate_state.is_none());
        assert!(net.populations[0].substrate_effects.is_none());
    }

    /// Substrate on at full health: effects get cached on the first
    /// step, substrate_state is populated, and the full-health effects
    /// equal the identity — so the hot path sees identity modulation.
    #[test]
    fn substrate_full_health_caches_identity_effects() {
        let mut net = SnnNetwork::new(two_pop_config_with_substrate(1.0)).unwrap();
        let ext = vec![1000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        net.step(&slices, 0.0, 1.0);
        let effects = net.populations[0].substrate_effects.as_ref().unwrap();
        assert!(effects.0.is_identity(), "axon effects should be identity");
        assert!(
            effects.1.is_identity(),
            "dendrite effects should be identity"
        );
    }

    /// Substrate at very low ATP + activity: ATP continues to drop as
    /// the debit loop subtracts per-spike cost from pop 0. Without
    /// passive recovery (disabled in helper) the delta is observable.
    #[test]
    fn substrate_debits_atp_as_spikes_accumulate() {
        // Use a generous per-spike cost so 10 steps at ~100% firing
        // produces a measurable debit even with a small population.
        let mut cfg = two_pop_config_with_substrate(0.5);
        for p in cfg.populations.iter_mut() {
            p.substrate.dynamics.atp_cost_per_spike = 1.0e-4;
        }
        let mut net = SnnNetwork::new(cfg).unwrap();
        let ext = vec![1000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];

        let atp_before = net.populations[0]
            .substrate_state
            .as_ref()
            .map(|s| s.atp_level)
            .unwrap();
        assert!((atp_before - 0.5).abs() < 1e-5);

        for _ in 0..20 {
            net.step(&slices, 0.0, 1.0);
        }

        let atp_after = net.populations[0]
            .substrate_state
            .as_ref()
            .map(|s| s.atp_level)
            .unwrap();
        assert!(
            atp_after < atp_before,
            "ATP should have debited: {atp_before} -> {atp_after}"
        );
    }

    /// Emergency silence: substrate with very low overall_capacity
    /// (crashed chemistry) zeros every spike on the next step.
    #[test]
    fn substrate_emergency_silence_zeros_spikes() {
        let mut cfg = two_pop_config_with_substrate(0.01);
        for p in cfg.populations.iter_mut() {
            // Threshold much higher than axon.overall_capacity at
            // atp=0.01 ensures the silence branch fires.
            p.substrate.emergency_silence_threshold = 0.8;
            // Also crash membrane + ion balance so overall_capacity collapses.
            p.substrate.initial_state.membrane_integrity = 0.05;
            p.substrate.initial_state.ion_balance = 0.05;
        }
        let mut net = SnnNetwork::new(cfg).unwrap();
        let ext = vec![10_000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..10 {
            net.step(&slices, 0.0, 1.0);
        }
        // Every spike should have been zeroed by the silence branch.
        let any_spike = net.spikes(0).iter().any(|&s| s != 0);
        assert!(!any_spike, "emergency silence should zero all spikes");
    }

    /// Effects cadence: after the first step, effects are cached;
    /// after `update_period` further steps, they recompute.
    #[test]
    fn substrate_effects_cache_respects_cadence() {
        let mut cfg = two_pop_config_with_substrate(1.0);
        for p in cfg.populations.iter_mut() {
            p.substrate.update_period = 5;
        }
        let mut net = SnnNetwork::new(cfg).unwrap();
        let ext = vec![500.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];

        // No effects yet — first step will populate them.
        assert!(net.populations[0].substrate_effects.is_none());
        net.step(&slices, 0.0, 1.0);
        assert!(
            net.populations[0].substrate_effects.is_some(),
            "effects should be cached after first step"
        );
    }

    // ---------------------------------------------------------------
    // Phase 1 Path A — SNN thalamic hook.
    // ---------------------------------------------------------------

    fn two_pop_config_with_thalamic(submit_threshold: f32) -> SnnConfig {
        let mut cfg = two_pop_config(13, 0, 0.05, 1.8);
        cfg.thalamic = Some(SnnThalamicCfg {
            source_id: 42,
            destinations: vec![100, 200, 300],
            submit_threshold,
            mode: nimcp_thalamic::RelayMode::Tonic,
        });
        cfg
    }

    /// Disable path: no thalamic cfg → `thalamic_channel()` is `None`
    /// and `thalamic_output_gain` returns identity 1.0.
    #[test]
    fn thalamic_disabled_returns_identity_gain() {
        let router =
            nimcp_thalamic::ThalamicRouter::new(nimcp_thalamic::ThalamicRouterConfig::default());
        let net = SnnNetwork::new(two_pop_config(0, 0, 0.1, 0.5)).unwrap();
        assert!(net.thalamic_channel().is_none());
        assert_eq!(net.thalamic_output_gain(&router, 100), 1.0);
    }

    /// Channel is constructed when cfg is Some; destinations are
    /// registered with the right initial attention weights (1.0).
    #[test]
    fn thalamic_channel_constructs_from_cfg() {
        let net = SnnNetwork::new(two_pop_config_with_thalamic(0.01)).unwrap();
        let ch = net.thalamic_channel().expect("channel should be Some");
        assert_eq!(ch.source_id, 42);
        assert_eq!(ch.n_destinations, 3);
        assert_eq!(ch.get_gate(100), 1.0);
        assert_eq!(ch.get_gate(200), 1.0);
        assert_eq!(ch.get_gate(999), 1.0); // unknown dest → 1.0
    }

    /// Destinations beyond the 16-cap surface as a construction error.
    #[test]
    fn thalamic_too_many_destinations_errors() {
        let mut cfg = two_pop_config(0, 0, 0.1, 0.5);
        cfg.thalamic = Some(SnnThalamicCfg {
            source_id: 1,
            destinations: (0..20).collect(),
            submit_threshold: 0.01,
            mode: nimcp_thalamic::RelayMode::default(),
        });
        assert!(matches!(
            SnnNetwork::new(cfg),
            Err(SnnError::StabilityInit { .. })
        ));
    }

    /// Auto-submit: a step with firing above threshold bumps the
    /// channel's submit counter; a step below threshold doesn't.
    #[test]
    fn thalamic_auto_submit_above_threshold() {
        // Very low threshold so any activity triggers.
        let mut net = SnnNetwork::new(two_pop_config_with_thalamic(0.0)).unwrap();
        // Drive pop 0 hard — definitely above threshold.
        let ext = vec![5000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..5 {
            net.step(&slices, 0.0, 1.0);
        }
        let submits = net.thalamic_channel().unwrap().submits_this_step;
        assert!(
            submits > 0,
            "auto-submit should have fired on active steps, got {submits}"
        );
    }

    /// High threshold suppresses auto-submit even under external drive.
    #[test]
    fn thalamic_threshold_gates_auto_submit() {
        let mut net = SnnNetwork::new(two_pop_config_with_thalamic(0.99)).unwrap();
        let ext = vec![5000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..5 {
            net.step(&slices, 0.0, 1.0);
        }
        let submits = net.thalamic_channel().unwrap().submits_this_step;
        assert_eq!(submits, 0, "high threshold should suppress auto-submit");
    }

    /// effective_output_gain reflects the router's Hebbian update when
    /// the caller has opened a matching channel on the router.
    #[test]
    fn thalamic_output_gain_reads_router() {
        let mut router =
            nimcp_thalamic::ThalamicRouter::new(nimcp_thalamic::ThalamicRouterConfig {
                hebbian_lr: 0.5,
                hebbian_decay: 0.0,
                ..Default::default()
            });
        let _ = router.open_channel(42, &[100]).unwrap();
        // Reinforce the (42, 100) route.
        router.record_submit(42);
        router.tick();
        assert!(router.hebbian_weight(42, 100) > 1.0);

        // SNN's thalamic_output_gain should reflect the reinforced route.
        let net = SnnNetwork::new(two_pop_config_with_thalamic(0.0)).unwrap();
        let gain = net.thalamic_output_gain(&router, 100);
        assert!(gain > 1.0, "reinforced route should boost gain, got {gain}");
    }

    // ---------------------------------------------------------------
    // Pink-noise integration — opt-in alternative to Poisson.
    // ---------------------------------------------------------------

    /// Build a two-pop config with pink noise on pop 0 and Poisson
    /// (pulse_mv=0 → effectively off) on pop 1. `n_octaves` propagates
    /// through to the [`crate::noise::PinkNoiseState`].
    fn two_pop_config_with_pink(n_octaves: u32, pulse_mv: f32) -> SnnConfig {
        let mut cfg = two_pop_config(0xF00D, 0, 0.05, 0.4);
        cfg.populations[0].noise = crate::noise::NoiseConfig::new_pink(n_octaves, pulse_mv);
        cfg
    }

    /// Pink-noise path is actually taken when `kind == Pink`: the
    /// generator must be constructed (pink_state.is_some()) and
    /// pink_alpha starts out `None` because the ring is empty.
    #[test]
    fn pink_path_activates_when_kind_pink() {
        let net = SnnNetwork::new(two_pop_config_with_pink(8, 20.0)).unwrap();
        // pink_alpha reflects pink-state presence + ring fill. Immediately
        // after construction the ring is empty → None.
        assert!(
            net.pink_alpha(0).is_none(),
            "pink_alpha must be None before the ring fills"
        );
        // Poisson-kind population never exposes alpha regardless of
        // how many steps run.
        assert!(net.pink_alpha(1).is_none());
    }

    /// After exactly [`crate::noise::PINK_RING_CAPACITY`] steps the
    /// ring is full, so `pink_alpha` returns `Some(_)` and the value
    /// is a sensible 1/f-like exponent.
    #[test]
    fn pink_alpha_returns_some_after_ring_fills() {
        let mut net = SnnNetwork::new(two_pop_config_with_pink(16, 20.0)).unwrap();
        // Zero external drive — we're measuring the noise generator, not
        // network dynamics. The ring fills regardless of firing rate.
        let empty: &[f32] = &[];
        let slices: Vec<&[f32]> = vec![empty, empty];
        for _ in 0..crate::noise::PINK_RING_CAPACITY {
            net.step(&slices, 0.0, 1.0);
        }
        let alpha = net
            .pink_alpha(0)
            .expect("alpha must be Some once the ring is full");
        assert!(alpha.is_finite(), "pink_alpha must be finite, got {alpha}");
        // Coarse 1/f sanity — Voss-McCartney with 16 octaves should put
        // α firmly above white (0.5). We don't over-tighten because the
        // DFA is approximate on 1024 samples.
        assert!(
            alpha > 0.5,
            "16-octave pink noise α={alpha} should exceed white noise baseline 0.5"
        );
    }

    /// Out-of-range pop index → None (no panic).
    #[test]
    fn pink_alpha_out_of_range_is_none() {
        let net = SnnNetwork::new(two_pop_config_with_pink(8, 20.0)).unwrap();
        assert!(net.pink_alpha(99).is_none());
    }

    /// Disable-path bit-identical contract: a network built entirely
    /// with Poisson defaults (`NoiseConfig::default()` is Poisson)
    /// produces the same per-step spike trains and weights as a
    /// hypothetical pre-pink build. We assert this by running two
    /// independently-constructed networks with the same seed and
    /// verifying byte-equality — any accidental consumption of the
    /// Poisson RNG from the pink path would diverge them.
    #[test]
    fn poisson_disable_path_is_deterministic_and_unchanged() {
        // Explicit Poisson config with master-tuned defaults.
        let mk_cfg = || {
            let mut cfg = two_pop_config(0xABCD_1234, 0, 0.1, 0.3);
            for p in cfg.populations.iter_mut() {
                p.noise = crate::noise::NoiseConfig::default();
                assert_eq!(p.noise.kind, crate::noise::NoiseKind::Poisson);
            }
            cfg
        };
        let mut a = SnnNetwork::new(mk_cfg()).unwrap();
        let mut b = SnnNetwork::new(mk_cfg()).unwrap();
        let ext = vec![25.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..50 {
            a.step(&slices, 0.0, 1.0);
            b.step(&slices, 0.0, 1.0);
        }
        assert_eq!(a.edge_weights(0), b.edge_weights(0));
        assert_eq!(a.spikes(0), b.spikes(0));
        assert_eq!(a.spikes(1), b.spikes(1));
        // Poisson-kind populations never expose α.
        assert!(a.pink_alpha(0).is_none());
        assert!(a.pink_alpha(1).is_none());
    }

    /// Bit-identical contract (stricter): the pre-existing `two_pop_config`
    /// helper produces `rate_hz=0, pulse_mv=0` noise → the Poisson
    /// inject is a fast no-op. Adding pink-noise plumbing must not
    /// perturb v_mem at all on that path — we verify by running with
    /// zero external drive and asserting every neuron stays at v_rest.
    #[test]
    fn poisson_zero_amplitude_leaves_v_mem_at_rest() {
        let mut net = SnnNetwork::new(two_pop_config(7, 0, 0.05, 0.4)).unwrap();
        let empty: &[f32] = &[];
        let slices: Vec<&[f32]> = vec![empty, empty];
        for _ in 0..20 {
            net.step(&slices, 0.0, 1.0);
        }
        // Both populations use pulse_mv=0 Poisson → no perturbation; no
        // spikes; v_mem stays at v_rest.
        let v_rest = net.v_mem(0)[0];
        assert!(
            net.v_mem(0).iter().all(|&v| (v - v_rest).abs() < 1e-6),
            "disable path must leave every v_mem at v_rest"
        );
        assert!(net.spikes(0).iter().all(|&s| s == 0));
    }

    // -----------------------------------------------------------------
    // V2 Phase 10 — conductance-based PSC migration tests.
    // Port of V1 commit dec956ab9's 46-test matrix, condensed to V2's
    // single-test-module layout. Each test exercises one CB invariant.
    // -----------------------------------------------------------------

    /// REGRESSION: with no pop opted in, every existing assertion must
    /// hold bit-identically. Disable path must never allocate
    /// conductance state (would waste memory in production).
    #[test]
    fn cb_disable_path_allocates_no_state() {
        let net = SnnNetwork::new(two_pop_config(0, 0, 0.1, 0.4)).unwrap();
        for pop_idx in 0..net.n_populations() {
            assert!(
                net.populations[pop_idx].state.conductance.is_none(),
                "pop {pop_idx} should have no CB state when LifParams.conductance is None"
            );
            assert!(
                net.populations[pop_idx].spec.lif.conductance.is_none(),
                "pop {pop_idx} LifParams.conductance should default None"
            );
        }
        assert!(!net.cb_weights_rescaled());
    }

    /// REGRESSION: enabling CB allocates state on every pop and exposes
    /// the rescale flag. Disabling drops state and resets the flag.
    #[test]
    fn cb_enable_disable_round_trip() {
        let mut net = SnnNetwork::new(two_pop_config(0, 0, 0.1, 0.4)).unwrap();
        net.enable_conductance();
        for pop_idx in 0..net.n_populations() {
            assert!(net.populations[pop_idx].state.conductance.is_some());
            assert!(net.populations[pop_idx].spec.lif.conductance.is_some());
        }

        let applied = net.rescale_for_conductance(crate::membrane::SNN_CB_WEIGHT_SCALE);
        assert!(applied, "first rescale should apply");
        assert!(net.cb_weights_rescaled());
        // V1 double-rescale-bomb defense: second call no-op.
        let second = net.rescale_for_conductance(crate::membrane::SNN_CB_WEIGHT_SCALE);
        assert!(!second, "second rescale must be a no-op (idempotent)");

        net.disable_conductance();
        for pop_idx in 0..net.n_populations() {
            assert!(net.populations[pop_idx].state.conductance.is_none());
            assert!(net.populations[pop_idx].spec.lif.conductance.is_none());
        }
        assert!(!net.cb_weights_rescaled(), "disable resets the flag");
    }

    /// REGRESSION: rescale shrinks all weights uniformly by the factor.
    #[test]
    fn cb_rescale_multiplies_every_weight() {
        let mut net = SnnNetwork::new(two_pop_config(7, 0, 0.1, 0.4)).unwrap();
        let pre: Vec<f32> = net.edge_weights(0).to_vec();
        net.enable_conductance();
        let factor = 0.02; // 1/50
        net.rescale_for_conductance(factor);
        let post = net.edge_weights(0);
        assert_eq!(pre.len(), post.len());
        for (a, b) in pre.iter().zip(post.iter()) {
            assert!((b - a * factor).abs() < 1e-6, "{b} != {a} * {factor}");
        }
    }

    /// E2E: under unbounded current-mode drive an SNN can run away
    /// (v overshoots threshold by huge amounts every step). CB mode
    /// physically forbids this — `v` cannot exceed E_exc=0 mV ever.
    #[test]
    fn cb_v_mem_never_crosses_e_exc_under_runaway_drive() {
        let mut net = SnnNetwork::new(two_pop_config(7, 0, 0.1, 1.8)).unwrap();
        // Hand-saturate weights at w_max (=10) to manufacture a runaway
        // condition that current-mode would fall into immediately.
        for w in net.edge_weights_mut(0) {
            *w = 10.0;
        }
        net.enable_conductance();
        // V1 default: 1/50 rescale.
        net.rescale_for_conductance(crate::membrane::SNN_CB_WEIGHT_SCALE);

        // Suprathreshold external drive (route through CB sign-deposit).
        let ext = vec![1_000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..200 {
            net.step(&slices, 0.0, 1.0);
            for pop_idx in 0..net.n_populations() {
                for &v in net.v_mem(pop_idx) {
                    assert!(
                        v.is_finite() && v <= crate::membrane::SNN_E_EXC_MV + 1e-3,
                        "pop {pop_idx} v={v} crossed E_exc={}",
                        crate::membrane::SNN_E_EXC_MV
                    );
                }
            }
        }
    }

    /// E2E: CB mode keeps v above E_inh under purely inhibitory drive.
    #[test]
    fn cb_v_mem_never_crosses_e_inh_under_inhibition() {
        let mut net = SnnNetwork::new(two_pop_config(0, 0, 0.1, 0.4)).unwrap();
        net.enable_conductance();
        // Strong negative external drive routes to g_inh.
        let ext = vec![-1_000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        for _ in 0..150 {
            net.step(&slices, 0.0, 1.0);
            for pop_idx in 0..net.n_populations() {
                for &v in net.v_mem(pop_idx) {
                    assert!(
                        v.is_finite() && v >= crate::membrane::SNN_E_INH_MV - 1e-3,
                        "pop {pop_idx} v={v} crossed E_inh={}",
                        crate::membrane::SNN_E_INH_MV
                    );
                }
            }
        }
    }

    /// INTEGRATION: a CB-enabled network with finite excitatory drive
    /// still emits spikes — the brake doesn't suppress all activity,
    /// it just prevents runaway. Validates that CB integration is
    /// alive end-to-end (deposit → decay → integrate → spike chain).
    #[test]
    fn cb_emits_spikes_under_moderate_drive() {
        let mut net = SnnNetwork::new(two_pop_config(11, 0, 0.1, 0.6)).unwrap();
        net.enable_conductance();
        // Don't rescale weights — keep them at their current-mode
        // magnitude so a single excitatory pre-spike has a strong
        // enough g_exc impulse to cross threshold.
        let ext = vec![1_000.0_f32; 128];
        let slices: Vec<&[f32]> = vec![&ext, &[]];
        let mut total_spikes = 0u64;
        for _ in 0..100 {
            total_spikes += u64::from(net.step(&slices, 0.0, 1.0));
        }
        assert!(
            total_spikes > 0,
            "CB-enabled network must emit some spikes under suprathreshold drive"
        );
    }

    /// INTEGRATION: g_exc decays toward zero between spikes (not
    /// stuck at peak). Disable external drive and let any residual
    /// conductance from initial deposits relax.
    #[test]
    fn cb_g_exc_decays_to_near_zero_between_spikes() {
        let mut net = SnnNetwork::new(two_pop_config(0, 0, 0.1, 0.4)).unwrap();
        net.enable_conductance();
        // Deposit one spike's worth via external drive, then run many
        // steps with no further drive — g_exc should decay.
        let ext = vec![5.0_f32; 128];
        let empty: &[f32] = &[];
        net.step(&[&ext, empty], 0.0, 1.0); // single deposit
        for _ in 0..100 {
            net.step(&[empty, empty], 0.0, 1.0);
        }
        // tau_exc=2ms, after 100 ms of decay g_exc << 1e-10.
        let cb = net.populations[0]
            .state
            .conductance
            .as_ref()
            .expect("CB enabled");
        for &g in &cb.g_exc {
            assert!(g.abs() < 1e-3, "g_exc {g} did not decay to near-zero");
        }
    }

    /// CHECKPOINT: snapshot persists `cb_weights_rescaled`; restore
    /// reads it back. Mirrors the V1 sidecar marker contract from
    /// commit `312c9b323` so a daemon restart cannot double-rescale.
    #[test]
    fn cb_snapshot_round_trips_rescaled_flag() {
        let mut net = SnnNetwork::new(two_pop_config(0, 0, 0.1, 0.4)).unwrap();
        net.enable_conductance();
        net.rescale_for_conductance(crate::membrane::SNN_CB_WEIGHT_SCALE);
        assert!(net.cb_weights_rescaled());

        let snap = net.snapshot();
        assert!(snap.cb_weights_rescaled);

        // Fresh network, restore the snapshot — flag should come back.
        let mut net2 = SnnNetwork::new(two_pop_config(0, 0, 0.1, 0.4)).unwrap();
        net2.enable_conductance();
        assert!(!net2.cb_weights_rescaled(), "fresh net starts unrescaled");
        assert!(net2.restore(&snap));
        assert!(net2.cb_weights_rescaled(), "restore reinstates the flag");
        // Subsequent rescale should be a no-op.
        let applied = net2.rescale_for_conductance(crate::membrane::SNN_CB_WEIGHT_SCALE);
        assert!(!applied, "double-rescale must be prevented after restore");
    }

    /// INTEGRATION: GPU path is bypassed when CB is enabled (per V1
    /// design lock — CB CPU only). We verify by enabling CB on a
    /// network whose config requested GPU forward; the call should
    /// silently fall through to CPU and not panic.
    #[test]
    fn cb_forces_cpu_fallback_even_when_gpu_requested() {
        // Build with use_gpu_forward = true but enable CB. On a
        // CPU-only build (no `cuda` feature) GPU was never live; on a
        // CUDA build the cpu_prep_modified_v_mem guard now includes
        // CB → GPU is bypassed. Either way the step must not panic
        // and v_mem stays bounded.
        let mut cfg = two_pop_config(0, 0, 0.1, 0.4);
        cfg.use_gpu_forward = true;
        let mut net = match SnnNetwork::new(cfg) {
            Ok(n) => n,
            Err(_) => return, // GPU unavailable on this builder; nothing to verify
        };
        net.enable_conductance();
        let ext = vec![100.0_f32; 128];
        let empty: &[f32] = &[];
        for _ in 0..30 {
            net.step(&[&ext, empty], 0.0, 1.0);
        }
        for v in net.v_mem(0) {
            assert!(v.is_finite() && *v <= crate::membrane::SNN_E_EXC_MV + 1e-3);
        }
    }

    /// Phase 10 — populations spread round-robin over every visible
    /// device must step identically to the single-device network, and
    /// a multi-device snapshot must restore into a single-device one.
    /// Multi-device assertions only bite on hosts with ≥2 GPUs (the
    /// vast pod); on one GPU this degenerates to RoundRobin over [0].
    #[cfg(feature = "cuda")]
    #[test]
    fn multi_device_round_robin_matches_single_device() {
        let devs = nimcp_gpu::enumerate_devices().unwrap_or_default();
        if devs.is_empty() {
            eprintln!("skipping: no CUDA device on this host");
            return;
        }
        let ordinals: Vec<u32> = devs.iter().map(|d| d.ordinal).collect();

        let mut cfg = two_pop_config(3, 0, 0.1, 0.4);
        cfg.use_gpu_forward = true;
        let mut single = SnnNetwork::new(cfg.clone()).expect("single-device GPU network");
        cfg.device_assignment = crate::device::DeviceAssignment::RoundRobin {
            devices: ordinals.clone(),
        };
        let mut multi = SnnNetwork::new(cfg).expect("multi-device GPU network");

        assert_eq!(single.population_devices(), &[0, 0]);
        assert_eq!(multi.population_devices()[0], ordinals[0]);
        assert_eq!(multi.population_devices()[1], ordinals[1 % ordinals.len()]);

        let ext = vec![100.0_f32; 128];
        let empty: &[f32] = &[];
        for step in 0..30 {
            let a = single.step(&[&ext, empty], 0.5, 1.0);
            let b = multi.step(&[&ext, empty], 0.5, 1.0);
            assert_eq!(a, b, "spike counts diverged at step {step}");
        }
        multi.synchronize_gpus().unwrap();

        let (s, m) = (single.snapshot(), multi.snapshot());
        assert_eq!(s.edge_weights, m.edge_weights);
        assert!(
            single.restore(&m),
            "multi-device snapshot must restore into single-device net"
        );
    }

    #[test]
    fn device_assignment_errors_surface_from_new() {
        let mut cfg = two_pop_config(0, 0, 0.1, 0.4);
        cfg.device_assignment = crate::device::DeviceAssignment::Manual {
            per_population: vec![0],
        };
        match SnnNetwork::new(cfg) {
            Err(SnnError::DeviceAssignment(
                crate::device::DeviceAssignError::ManualLengthMismatch {
                    given: 1,
                    n_pops: 2,
                },
            )) => {}
            other => panic!("expected ManualLengthMismatch, got {:?}", other.err()),
        }
    }

    #[test]
    fn cpu_path_records_resolved_devices() {
        let mut cfg = two_pop_config(0, 0, 0.1, 0.4);
        cfg.device_assignment = crate::device::DeviceAssignment::Manual {
            per_population: vec![1, 0],
        };
        let net = SnnNetwork::new(cfg).unwrap();
        assert_eq!(net.population_devices(), &[1, 0]);
    }

    #[test]
    fn snn_config_json_defaults_rate_ema_alpha() {
        let raw = r#"{"populations": [], "edges": [], "rng_seed": 7}"#;
        let cfg: SnnConfig = serde_json::from_str(raw).unwrap();
        assert_eq!(cfg.rate_ema_alpha, DEFAULT_RATE_EMA_ALPHA);
        // An explicit value still wins.
        let raw = r#"{"populations": [], "edges": [], "rng_seed": 7, "rate_ema_alpha": 0.5}"#;
        let cfg: SnnConfig = serde_json::from_str(raw).unwrap();
        assert_eq!(cfg.rate_ema_alpha, 0.5);
    }

    // --- live tuning + quench ---

    #[test]
    fn tunables_report_construction_values() {
        let net = SnnNetwork::new(two_pop_config(5, 0, 0.1, 0.4)).unwrap();
        let t = net.tunables();
        for name in TUNABLE_NAMES {
            assert!(t.contains_key(*name), "missing {name}");
        }
        assert_eq!(t["rstdp_lr"], RSTDP_LR_BASELINE);
        assert_eq!(t["target_rate"], 0.1);
        assert_eq!(t["homeo_min_scale"], HomeostaticParams::default().min_scale);
        assert_eq!(t["max_scale_dead"], 1.05);
    }

    #[test]
    fn tune_rstdp_lr_scales_amplitudes_from_reference() {
        let mut net = SnnNetwork::new(two_pop_config(5, 0, 0.1, 0.4)).unwrap();
        let (a_plus, a_minus) = (
            net.edges[0].spec.rstdp.stdp.a_plus,
            net.edges[0].spec.rstdp.stdp.a_minus,
        );
        let out = net.tune("rstdp_lr", RSTDP_LR_BASELINE * 0.5).unwrap();
        assert!(out.applied);
        assert_eq!(out.edges, net.n_edges());
        assert!((net.edges[0].spec.rstdp.stdp.a_plus - a_plus * 0.5).abs() < 1e-9);
        assert!((net.edges[0].spec.rstdp.stdp.a_minus - a_minus * 0.5).abs() < 1e-9);
        // Scale is relative to the construction reference, not the last value.
        net.tune("rstdp_lr", RSTDP_LR_BASELINE * 2.0).unwrap();
        assert!((net.edges[0].spec.rstdp.stdp.a_plus - a_plus * 2.0).abs() < 1e-9);
        assert_eq!(net.tunables()["rstdp_lr"], RSTDP_LR_BASELINE * 2.0);
        // Back to baseline restores the originals exactly.
        net.tune("rstdp_lr", RSTDP_LR_BASELINE).unwrap();
        assert_eq!(net.edges[0].spec.rstdp.stdp.a_plus, a_plus);
    }

    #[test]
    fn tune_population_knobs_write_every_population() {
        let mut net = SnnNetwork::new(two_pop_config(5, 0, 0.1, 0.4)).unwrap();
        let out = net.tune("target_rate", 0.05).unwrap();
        assert_eq!(out.populations, 2);
        for p in &net.populations {
            assert_eq!(p.spec.target_rate, 0.05);
            assert_eq!(p.rate.target_rate, 0.05);
        }
        net.tune("homeo_max_scale", 1.1).unwrap();
        net.tune("homeo_min_scale", 0.9).unwrap();
        net.tune("noise_rate_hz", 0.0).unwrap();
        net.tune("noise_pulse_mv", 10.0).unwrap();
        let t = net.tunables();
        assert_eq!(t["homeo_max_scale"], 1.1);
        assert_eq!(t["homeo_min_scale"], 0.9);
        assert_eq!(t["noise_rate_hz"], 0.0);
        assert_eq!(t["noise_pulse_mv"], 10.0);
        // Still steps.
        let ext = vec![0.0_f32; 128];
        let empty: &[f32] = &[];
        let _ = net.step(&[&ext, empty], 0.0, 1.0);
    }

    #[test]
    fn tune_recorded_only_knobs_report_unapplied() {
        let mut net = SnnNetwork::new(two_pop_config(5, 0, 0.1, 0.4)).unwrap();
        for name in ["max_scale_dead", "dead_threshold", "rstdp_baseline_alpha"] {
            let out = net.tune(name, 0.42).unwrap();
            assert!(!out.applied, "{name}");
            assert!(out.note.as_deref().unwrap().contains("not applied"));
            assert_eq!(net.tunables()[name], 0.42);
        }
    }

    #[test]
    fn tune_rejects_unknown_and_invalid() {
        let mut net = SnnNetwork::new(two_pop_config(5, 0, 0.1, 0.4)).unwrap();
        assert!(matches!(
            net.tune("nonsense", 1.0),
            Err(SnnError::UnknownTunable(_))
        ));
        assert!(matches!(
            net.tune("target_rate", 0.0),
            Err(SnnError::InvalidTunable { .. })
        ));
        assert!(matches!(
            net.tune("target_rate", f32::NAN),
            Err(SnnError::InvalidTunable { .. })
        ));
        assert!(matches!(
            net.tune("homeo_max_scale", 0.5),
            Err(SnnError::InvalidTunable { .. })
        ));
        assert!(matches!(
            net.tune("noise_rate_hz", -1.0),
            Err(SnnError::InvalidTunable { .. })
        ));
        assert!(matches!(
            net.tune("rstdp_lr", -1e-4),
            Err(SnnError::InvalidTunable { .. })
        ));
        // Nothing changed.
        assert_eq!(net.tunables()["target_rate"], 0.1);
    }

    #[test]
    fn force_quench_resets_prefix_and_counts() {
        let mut net = SnnNetwork::new(two_pop_config(5, 0, 0.1, 0.4)).unwrap();
        // Drive pop A hard so v_mem moves off rest.
        let ext = vec![30.0_f32; 128];
        let empty: &[f32] = &[];
        for _ in 0..3 {
            net.step(&[&ext, empty], 0.0, 1.0);
        }
        let v_reset = LifParams::default().v_reset;
        let before: Vec<f32> = net.v_mem(0).to_vec();
        assert!(
            before[..15].iter().any(|v| (*v - v_reset).abs() > 1e-6),
            "drive should move v_mem"
        );
        let quenched = net.force_quench(15).unwrap();
        assert_eq!(quenched, 30, "15 per population × 2 populations");
        assert!(net.v_mem(0)[..15].iter().all(|v| *v == v_reset));
        assert_eq!(
            net.v_mem(0)[15..],
            before[15..],
            "untouched beyond the prefix"
        );
        assert!(net.v_mem(1)[..15].iter().all(|v| *v == v_reset));
        // n larger than a population clamps.
        assert_eq!(net.force_quench(1_000).unwrap(), 256);
        assert_eq!(net.force_quench(0).unwrap(), 0);
    }
}
