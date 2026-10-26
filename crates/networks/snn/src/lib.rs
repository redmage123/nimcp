//! NIMCP V2 — spiking neural network.
//!
//! # Phase 3 principles (ported from V1's hard lessons)
//!
//! 1. **Quiet-start is a load-time weight transform.** V1's quiet-start
//!    only applied to fresh init; resuming from a saturated checkpoint
//!    re-entered the saturated state. V2 keys the transform on observed
//!    firing statistics at load time — works the same on fresh or
//!    resumed weights. See [`nimcp_plasticity::quiet_start`].
//!
//! 2. **CSR synapse storage owned per population.** V1's lightweight SNN
//!    refactor (2026-03 onwards) got this right. V2 mirrors the layout:
//!    `row_ptr` + `col_idx` + `weights`, all flat arrays.
//!
//! 3. **Homeostatic scaling on the flat weights array.** Row-major, one
//!    multiply per synapse, no strided entries. Ports the perf fix from
//!    V1 commit `3a7aa5f7d`.
//!
//! 4. **Tight homeostatic bounds `[0.98, 1.02]` always.** The V1
//!    "emergency band" oscillation bug is a regression test from day
//!    one — see `nimcp-plasticity::homeostatic::anti_oscillation_*`.
//!
//! 5. **R-STDP warmup gate (`rate_samples >= 100`).** Without it, Hebbian
//!    LTP runs away on fresh init before homeostasis engages. V1 learned
//!    this the hard way; V2 ships the gate + its test from the start.
//!
//! # Module layout
//!
//! - [`lif`]: LIF neuron dynamics (membrane voltage, spike emission, refractory)
//! - [`csr`]: CSR synapse storage + `I_syn` forward kernel
//! - [`rstdp`]: reward-modulated STDP weight updates + eligibility traces
//! - [`homeostatic`]: population-level synaptic scaling + quiet-start
//! - [`network`]: top-level [`SnnNetwork`] composition (wires above)

// The CPU path forbids unsafe. With `--features cuda`, the LIF GPU kernel
// launch path requires one `unsafe` block per call (cudarc's `.launch(cfg)`
// is `unsafe fn`). Scoped to `lif.rs`'s `#[cfg(feature = "cuda")]` section.
#![cfg_attr(not(feature = "cuda"), forbid(unsafe_code))]
#![cfg_attr(feature = "cuda", deny(unsafe_code))]
#![cfg_attr(feature = "cuda", allow(unsafe_code))]

pub mod adaptation;
pub mod autotune;
pub mod basket;
pub mod csr;
pub mod depression;
pub mod device;
pub mod homeostatic;
pub mod intrinsic_reward;
pub mod lif;
pub mod membrane;
pub mod network;
pub mod noise;
pub mod rstdp;
pub mod substrate_adapter;
pub mod tuning;

pub use adaptation::{AdaptationError, AdaptationState};
pub use autotune::{Actuator, AutotuneController, AutotuneSnapshot, CycleMetrics, Decision};
pub use basket::{BasketError, BasketPool};
pub use csr::{CsrSynapses, Population, PopulationId};
pub use depression::{DepressionConfig, DepressionState, step_depression, weight_multiplier};
pub use device::{DeviceAssignError, DeviceAssignment};
pub use homeostatic::step_homeostatic_with_reward;
pub use homeostatic::{PopulationRateEma, apply_quiet_start_transform, step_homeostatic};
pub use intrinsic_reward::{IntrinsicRewardConfig, compute_network_reward, compute_per_pop_reward};
pub use lif::{ConductanceParams, ConductanceState, LifParams, LifState, lif_step_cpu};
pub use membrane::{
    SNN_CB_WEIGHT_SCALE, SNN_E_EXC_MV, SNN_E_INH_MV, SNN_TAU_EXC_MS, SNN_TAU_INH_MS,
};
pub use network::{RSTDP_LR_BASELINE, SnnConfig, SnnNetwork, TUNABLE_NAMES, TuneOutcome};
pub use noise::{
    NoiseConfig, NoiseKind, PinkNoiseState, inject_pink_noise, inject_poisson_noise,
    noise_factor_for_pop,
};
pub use rstdp::{RstdpParams, RstdpState, step_rstdp};
pub use tuning::{HealthyStreak, TunableState};
