//! Phase 3a — LIF neuron dynamics.
//!
//! Leaky integrate-and-fire neuron population, per-step evolution:
//!
//! ```text
//! if refrac[i] > 0:
//!     refrac[i] -= 1; spike[i] = 0
//! else:
//!     v[i] += (dt / tau) * (v_rest - v[i] + I_syn[i])
//!     if v[i] >= v_thresh:
//!         v[i] = v_reset; refrac[i] = refrac_steps; spike[i] = 1
//!     else:
//!         spike[i] = 0
//! ```
//!
//! # Parallel code paths
//!
//! - [`lif_step_cpu`] — pure-Rust baseline used for tests, small
//!   populations, and CI hosts without a GPU.
//! - [`LifGpu`] (behind `#[cfg(feature = "cuda")]`) — NVRTC-compiled
//!   kernel with persistent device buffers for `v_mem` and `refrac`.
//!   `i_syn` is uploaded once per step; `spike` is downloaded once so
//!   the downstream CSR forward can consume it on the host. A
//!   CPU↔GPU equivalence test (`lif_cpu_gpu_match`) guards the two
//!   paths against drift.
//!
//! # Dimensional conventions
//!
//! All voltages are in millivolts; time in milliseconds. `dt_ms` is
//! passed per step (scheduler-owned), not stored on [`LifParams`].

#[cfg(feature = "cuda")]
use std::sync::Arc;

#[cfg(feature = "cuda")]
use cudarc::driver::{
    CudaContext, CudaFunction, CudaModule, CudaSlice, CudaStream, LaunchConfig, PushKernelArg,
};

#[cfg(feature = "cuda")]
use nimcp_gpu::GpuError;

// -------------------------------------------------------------------------
// Parameters
// -------------------------------------------------------------------------

/// LIF neuron parameters — shared across every neuron in a population.
///
/// `dt_ms` is *not* stored here: the scheduler owns the timestep so it
/// can shrink/grow without rebuilding the param struct.
#[derive(Debug, Clone, Copy, serde::Serialize, serde::Deserialize)]
#[serde(default)]
pub struct LifParams {
    /// Membrane time constant (ms). Typical cortical value ~20 ms.
    pub tau_ms: f32,
    /// Resting membrane voltage (mV).
    pub v_rest: f32,
    /// Spike threshold (mV). Voltage at or above this fires.
    pub v_thresh: f32,
    /// Reset voltage after spike (mV).
    pub v_reset: f32,
    /// Absolute refractory period, in integration steps.
    pub refrac_steps: u32,
    /// Conductance-based PSC config. `None` (default) = legacy
    /// current-mode dynamics (bit-identical to pre-CB behavior).
    /// `Some` enables CB synaptic input — `g_exc`/`g_inh` accumulate
    /// per-receptor and drive the membrane via reversal-potential
    /// terms. See [`crate::membrane`] for the equation.
    ///
    /// Migration note: when flipping `None → Some`, weights must be
    /// rescaled (typically by [`crate::membrane::SNN_CB_WEIGHT_SCALE`])
    /// to compensate for the ~50 mV avg driving force. Use
    /// [`crate::SnnNetwork::rescale_for_conductance`] for the one-shot
    /// rescale; the network tracks an `cb_weights_rescaled` sticky
    /// flag so re-enabling on an already-rescaled checkpoint is a
    /// no-op.
    #[serde(default)]
    pub conductance: Option<ConductanceParams>,
}

/// Conductance-based PSC parameters. Held inside [`LifParams::conductance`]
/// when CB mode is enabled for a population.
#[derive(Debug, Clone, Copy, serde::Serialize, serde::Deserialize)]
#[serde(default)]
pub struct ConductanceParams {
    /// Excitatory reversal potential (mV). Cortical AMPA ≈ 0 mV.
    pub e_exc_mv: f32,
    /// Inhibitory reversal potential (mV). Cortical GABA-A ≈ -80 mV.
    pub e_inh_mv: f32,
    /// Excitatory conductance decay time constant (ms). Fast AMPA ≈ 2 ms.
    pub tau_exc_ms: f32,
    /// Inhibitory conductance decay time constant (ms). GABA-A ≈ 8 ms,
    /// slower than AMPA so I has time to brake E.
    pub tau_inh_ms: f32,
}

impl Default for ConductanceParams {
    fn default() -> Self {
        Self {
            e_exc_mv: crate::membrane::SNN_E_EXC_MV,
            e_inh_mv: crate::membrane::SNN_E_INH_MV,
            tau_exc_ms: crate::membrane::SNN_TAU_EXC_MS,
            tau_inh_ms: crate::membrane::SNN_TAU_INH_MS,
        }
    }
}

impl Default for LifParams {
    /// Cortical defaults: `tau=20ms`, `v_rest=-70mV`, `v_thresh=-50mV`,
    /// `v_reset=-70mV`, `refrac=2` steps. Conductance disabled.
    fn default() -> Self {
        Self {
            tau_ms: 20.0,
            v_rest: -70.0,
            v_thresh: -50.0,
            v_reset: -70.0,
            refrac_steps: 2,
            conductance: None,
        }
    }
}

// -------------------------------------------------------------------------
// CPU state
// -------------------------------------------------------------------------

/// Per-population LIF state, host-resident.
///
/// All three buffers have length [`LifState::n_neurons`]. Parallel to the
/// GPU layout so equivalence tests can compare element-by-element.
#[derive(Debug, Clone)]
pub struct LifState {
    /// Membrane voltage per neuron (mV).
    pub v_mem: Vec<f32>,
    /// Refractory counter per neuron (integration steps remaining).
    pub refrac: Vec<u32>,
    /// Spike flag per neuron for the most recent step (1 = fired, 0 = idle).
    pub spike: Vec<u8>,
    /// Neuron count — fixed at construction.
    pub n_neurons: u32,
    /// Conductance state. `Some` iff [`LifParams::conductance`] was
    /// `Some` at construction. Disable path: `None`, no allocation,
    /// no per-step work.
    pub conductance: Option<ConductanceState>,
}

/// Per-neuron excitatory + inhibitory conductances. Allocated only when
/// CB mode is enabled — otherwise [`LifState::conductance`] is `None`
/// and no memory is paid.
#[derive(Debug, Clone)]
pub struct ConductanceState {
    /// Excitatory conductance per neuron (dimensionless).
    pub g_exc: Vec<f32>,
    /// Inhibitory conductance per neuron (dimensionless).
    pub g_inh: Vec<f32>,
}

impl ConductanceState {
    /// Allocate at zero for every neuron.
    #[must_use]
    pub fn new(n_neurons: u32) -> Self {
        let n = n_neurons as usize;
        Self {
            g_exc: vec![0.0; n],
            g_inh: vec![0.0; n],
        }
    }

    /// Reset all conductances to zero — call on network reset.
    pub fn reset(&mut self) {
        self.g_exc.fill(0.0);
        self.g_inh.fill(0.0);
    }
}

impl LifState {
    /// Allocate state for `n_neurons` at rest, no active refractories, no spikes.
    ///
    /// CB state (`g_exc`/`g_inh`) is allocated iff `params.conductance`
    /// is `Some`. Disable path: zero allocation overhead.
    #[must_use]
    pub fn new(n_neurons: u32, params: &LifParams) -> Self {
        let n = n_neurons as usize;
        Self {
            v_mem: vec![params.v_rest; n],
            refrac: vec![0u32; n],
            spike: vec![0u8; n],
            n_neurons,
            conductance: params
                .conductance
                .as_ref()
                .map(|_| ConductanceState::new(n_neurons)),
        }
    }

    /// Return every neuron to rest and clear spikes + refractory counters.
    /// Also zeros conductance state if present.
    pub fn reset(&mut self, params: &LifParams) {
        self.v_mem.fill(params.v_rest);
        self.refrac.fill(0);
        self.spike.fill(0);
        if let Some(cb) = self.conductance.as_mut() {
            cb.reset();
        }
    }

    /// Count neurons that fired on the most recent step.
    #[must_use]
    pub fn n_spikes_this_step(&self) -> u32 {
        // Spikes are 0/1 by construction; summing as u32 is safe because
        // n_neurons fits in u32 by definition.
        self.spike.iter().map(|&s| u32::from(s)).sum()
    }

    /// Enable CB mode in-place: allocate `ConductanceState` if not
    /// already present. Idempotent. Used by
    /// [`crate::SnnNetwork::rescale_for_conductance`] when an admin
    /// flips the network into CB mode at runtime.
    pub fn enable_conductance_in_place(&mut self) {
        if self.conductance.is_none() {
            self.conductance = Some(ConductanceState::new(self.n_neurons));
        }
    }
}

// -------------------------------------------------------------------------
// CPU step
// -------------------------------------------------------------------------

/// Advance a [`LifState`] by one timestep on the CPU.
///
/// Decrements refractory counters on refracting neurons and suppresses
/// their spike; otherwise integrates membrane voltage, emits a spike and
/// resets on threshold crossing.
///
/// # Panics
///
/// Panics if `i_syn.len()` does not equal `state.n_neurons`. The
/// scheduler should guarantee this, so it's a programming error rather
/// than a recoverable runtime condition.
///
/// # Formula
///
/// `v_new = v + (dt / tau) * (v_rest - v + I_syn)`
///
/// With `dt/tau` precomputed once per step for cache friendliness.
// HOT PATH: called on every integration step; keep branch-light.
pub fn lif_step_cpu(state: &mut LifState, i_syn: &[f32], params: &LifParams, dt_ms: f32) {
    // CB dispatch: when both params and state agree on CB mode, route to
    // the conductance integrator. Otherwise the legacy current-mode path
    // runs unchanged — bit-identical to pre-CB behavior.
    if params.conductance.is_some() && state.conductance.is_some() {
        lif_step_cpu_cb(state, params, dt_ms);
        return;
    }
    assert_eq!(
        i_syn.len(),
        state.n_neurons as usize,
        "lif_step_cpu: i_syn length {} must match n_neurons {}",
        i_syn.len(),
        state.n_neurons
    );
    // dt / tau computed once — param-dependent but constant within the step.
    let dt_over_tau = dt_ms / params.tau_ms;
    let v_rest = params.v_rest;
    let v_thresh = params.v_thresh;
    let v_reset = params.v_reset;
    let refrac_steps = params.refrac_steps;

    // Zip the four parallel buffers so LLVM sees contiguous access and
    // clippy doesn't flag `for i in 0..n` (needless_range_loop).
    for (((v_mem, refrac), spike), &i) in state
        .v_mem
        .iter_mut()
        .zip(state.refrac.iter_mut())
        .zip(state.spike.iter_mut())
        .zip(i_syn.iter())
    {
        if *refrac > 0 {
            *refrac -= 1;
            *spike = 0;
            continue;
        }
        let v_new = *v_mem + dt_over_tau * (v_rest - *v_mem + i);
        if v_new >= v_thresh {
            *v_mem = v_reset;
            *refrac = refrac_steps;
            *spike = 1;
        } else {
            *v_mem = v_new;
            *spike = 0;
        }
    }
}

/// One LIF step in conductance mode.
///
/// Reads `state.conductance.{g_exc, g_inh}` (filled by the upstream
/// CSR/basket/noise deposit pass) and computes
/// `dv = ((v_rest - v) + g_exc·(E_exc - v) + g_inh·(E_inh - v)) / tau · dt`
/// via [`crate::membrane::compute_dv`], then applies threshold + refractory.
///
/// After integration, conductances are decayed by
/// `exp(-dt/tau_exc)` and `exp(-dt/tau_inh)` so the next step starts
/// from the decayed value.
///
/// # Panics
///
/// Panics if `state.conductance` is `None` or if the conductance buffers
/// have a different length than the membrane buffers — both are
/// programming errors, not runtime conditions.
pub fn lif_step_cpu_cb(state: &mut LifState, params: &LifParams, dt_ms: f32) {
    let cb_params = params
        .conductance
        .as_ref()
        .expect("lif_step_cpu_cb requires params.conductance to be Some");
    let cb_state = state
        .conductance
        .as_mut()
        .expect("lif_step_cpu_cb requires state.conductance to be Some");
    assert_eq!(
        cb_state.g_exc.len(),
        state.v_mem.len(),
        "g_exc length must match v_mem length"
    );
    assert_eq!(
        cb_state.g_inh.len(),
        state.v_mem.len(),
        "g_inh length must match v_mem length"
    );

    let v_rest = params.v_rest;
    let v_thresh = params.v_thresh;
    let v_reset = params.v_reset;
    let refrac_steps = params.refrac_steps;
    let tau_ms = params.tau_ms;
    let e_exc = cb_params.e_exc_mv;
    let e_inh = cb_params.e_inh_mv;
    // Decay factors computed once per step.
    let decay_exc = (-dt_ms / cb_params.tau_exc_ms.max(1e-3)).exp();
    let decay_inh = (-dt_ms / cb_params.tau_inh_ms.max(1e-3)).exp();

    for ((((v_mem, refrac), spike), g_exc), g_inh) in state
        .v_mem
        .iter_mut()
        .zip(state.refrac.iter_mut())
        .zip(state.spike.iter_mut())
        .zip(cb_state.g_exc.iter_mut())
        .zip(cb_state.g_inh.iter_mut())
    {
        if *refrac > 0 {
            *refrac -= 1;
            *spike = 0;
            // Still decay conductances during refractory — synapses
            // continue to relax even if the soma can't fire.
            crate::membrane::decay_one(g_exc, g_inh, decay_exc, decay_inh);
            continue;
        }
        let dv = crate::membrane::compute_dv(
            *v_mem, v_rest, tau_ms, dt_ms, 0.0, *g_exc, *g_inh, e_exc, e_inh, true,
        );
        let v_new = *v_mem + dv;
        if v_new >= v_thresh {
            *v_mem = v_reset;
            *refrac = refrac_steps;
            *spike = 1;
        } else {
            *v_mem = v_new;
            *spike = 0;
        }
        // Decay conductances after integration so this step's deposits
        // (already in g_exc/g_inh) drove the integration; next step
        // sees the decayed values + whatever new spikes deposit.
        crate::membrane::decay_one(g_exc, g_inh, decay_exc, decay_inh);
    }
}

// -------------------------------------------------------------------------
// GPU kernel source
// -------------------------------------------------------------------------

/// NVRTC-compiled kernel: one timestep of LIF dynamics, parallel over
/// neurons. Matches [`lif_step_cpu`] exactly; the equivalence test in
/// this file is the regression gate against drift.
///
/// Two entry points:
///   - `lif_step` — base current-mode integrator (no v_prep).
///   - `lif_step_with_prep` — Phase 9e variant that adds a per-neuron
///     `v_prep` delta (Poisson noise + adaptation hyperpol from CPU)
///     to v_mem before integration. Lifts the cpu_prep guard so
///     stability-on populations can run on GPU.
#[cfg(feature = "cuda")]
const LIF_KERNEL_SRC: &str = r#"
extern "C" __global__ void lif_step(
    float* v_mem,
    unsigned int* refrac,
    unsigned char* spike,
    const float* i_syn,
    int n,
    float dt_over_tau,
    float v_rest,
    float v_thresh,
    float v_reset,
    unsigned int refrac_steps
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    if (refrac[i] > 0) {
        refrac[i] -= 1;
        spike[i] = 0;
        return;
    }
    float v = v_mem[i] + dt_over_tau * (v_rest - v_mem[i] + i_syn[i]);
    if (v >= v_thresh) {
        v_mem[i] = v_reset;
        refrac[i] = refrac_steps;
        spike[i] = 1;
    } else {
        v_mem[i] = v;
        spike[i] = 0;
    }
}

extern "C" __global__ void lif_step_with_prep(
    float* v_mem,
    unsigned int* refrac,
    unsigned char* spike,
    const float* i_syn,
    const float* v_prep,
    int n,
    float dt_over_tau,
    float v_rest,
    float v_thresh,
    float v_reset,
    unsigned int refrac_steps
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    if (refrac[i] > 0) {
        refrac[i] -= 1;
        spike[i] = 0;
        return;
    }
    // Add Poisson/pink noise + AHP/pump hyperpol to v_mem first, then
    // integrate. Mirrors the CPU sequence in network.rs step #1+#3.
    float v_pre = v_mem[i] + v_prep[i];
    float v = v_pre + dt_over_tau * (v_rest - v_pre + i_syn[i]);
    if (v >= v_thresh) {
        v_mem[i] = v_reset;
        refrac[i] = refrac_steps;
        spike[i] = 1;
    } else {
        v_mem[i] = v;
        spike[i] = 0;
    }
}
"#;

// -------------------------------------------------------------------------
// GPU LIF
// -------------------------------------------------------------------------

/// GPU-resident LIF state + kernel handle.
///
/// `v_mem` and `refrac` stay on the device across calls; only `i_syn`
/// (host → device, one step's worth of input) and `spike` (device →
/// host mirror for downstream CSR forward) cross the bus per step.
///
/// The kernel is compiled once in [`LifGpu::new`] via NVRTC and the
/// [`CudaModule`] is held on the struct so the [`CudaFunction`] handle
/// stays valid for the lifetime of the population.
#[cfg(feature = "cuda")]
pub struct LifGpu {
    n_neurons: u32,
    v_mem: CudaSlice<f32>,
    refrac: CudaSlice<u32>,
    spike: CudaSlice<u8>,
    ctx: Arc<CudaContext>,
    stream: Arc<CudaStream>,
    // Held to keep the loaded PTX alive for the kernel handle.
    #[allow(dead_code)]
    module: Arc<CudaModule>,
    kernel: CudaFunction,
    /// Phase 9e — `lif_step_with_prep` entry point. Loaded from the
    /// same PTX as the base kernel.
    kernel_with_prep: CudaFunction,
    // Mirror of the most recent spike count — updated on every `step` so
    // `spikes_this_step` is a cheap scalar accessor.
    last_spikes: u32,
}

#[cfg(feature = "cuda")]
impl std::fmt::Debug for LifGpu {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("LifGpu")
            .field("n_neurons", &self.n_neurons)
            .field("last_spikes", &self.last_spikes)
            .finish_non_exhaustive()
    }
}

#[cfg(feature = "cuda")]
fn cuda_err<E: std::fmt::Debug>(e: E) -> GpuError {
    GpuError::Cuda(format!("{e:?}"))
}

#[cfg(feature = "cuda")]
impl LifGpu {
    /// Allocate device buffers for an `n_neurons` population at rest and
    /// compile the LIF kernel once.
    ///
    /// # Errors
    ///
    /// Propagates any [`GpuError::Cuda`] from context creation, NVRTC
    /// compilation, module load, or device allocation.
    pub fn new(n_neurons: u32, params: &LifParams) -> Result<Self, GpuError> {
        let ctx = CudaContext::new(0).map_err(cuda_err)?;
        Self::new_with_context(ctx, n_neurons, params)
    }

    /// Like [`Self::new`] but reuses a caller-supplied context.
    ///
    /// Phase 9b fix: sharing one [`CudaContext`] across every
    /// [`LifGpu`] + [`crate::csr::CsrGpu`] in the network was the
    /// blocker for 1.8M-neuron scale — each fresh context costs
    /// several hundred MB of VRAM, and 6+ edges exhausted the 20 GB
    /// RTX 4000 budget before the weights ever got uploaded.
    pub fn new_with_context(
        ctx: Arc<CudaContext>,
        n_neurons: u32,
        params: &LifParams,
    ) -> Result<Self, GpuError> {
        if n_neurons == 0 {
            return Err(GpuError::Cuda("LifGpu::new: n_neurons must be > 0".into()));
        }
        let n = n_neurons as usize;

        let stream = ctx.default_stream();
        let ptx = cudarc::nvrtc::compile_ptx(LIF_KERNEL_SRC).map_err(cuda_err)?;
        let module = ctx.load_module(ptx).map_err(cuda_err)?;
        let kernel = module.load_function("lif_step").map_err(cuda_err)?;
        let kernel_with_prep = module
            .load_function("lif_step_with_prep")
            .map_err(cuda_err)?;

        let v_init: Vec<f32> = vec![params.v_rest; n];
        let v_mem = stream.memcpy_stod(&v_init).map_err(cuda_err)?;
        let refrac: CudaSlice<u32> = stream.alloc_zeros::<u32>(n).map_err(cuda_err)?;
        let spike: CudaSlice<u8> = stream.alloc_zeros::<u8>(n).map_err(cuda_err)?;

        tracing::info!(
            n_neurons,
            tau_ms = params.tau_ms,
            v_thresh = params.v_thresh,
            "lif gpu buffers allocated",
        );

        Ok(Self {
            n_neurons,
            v_mem,
            refrac,
            spike,
            ctx,
            stream,
            module,
            kernel,
            kernel_with_prep,
            last_spikes: 0,
        })
    }

    /// Borrow the CUDA context (shared with sibling GPU subsystems).
    #[must_use]
    pub fn context(&self) -> &Arc<CudaContext> {
        &self.ctx
    }

    /// Borrow the default stream.
    #[must_use]
    pub fn stream(&self) -> &Arc<CudaStream> {
        &self.stream
    }

    /// Neuron count this LIF population was built with.
    #[must_use]
    pub fn n_neurons(&self) -> u32 {
        self.n_neurons
    }

    /// Advance one LIF timestep on the GPU.
    ///
    /// Uploads `i_syn`, launches the kernel, downloads `spike` into
    /// `out_spikes` for the host-side CSR forward kernel. `v_mem` and
    /// `refrac` stay device-resident across calls.
    ///
    /// `out_spikes` is cleared + resized to `n_neurons` internally so
    /// callers can reuse the same buffer across steps without managing
    /// its shape.
    ///
    /// # Errors
    ///
    /// Propagates any [`GpuError::Cuda`] from memcpy / kernel launch.
    // HOT PATH: called on every integration step; one kernel, two memcpys.
    pub fn step(
        &mut self,
        i_syn: &[f32],
        out_spikes: &mut Vec<u8>,
        params: &LifParams,
        dt_ms: f32,
    ) -> Result<(), GpuError> {
        if i_syn.len() != self.n_neurons as usize {
            return Err(GpuError::Cuda(format!(
                "LifGpu::step: i_syn.len()={} but n_neurons={}",
                i_syn.len(),
                self.n_neurons
            )));
        }

        // Upload this step's current. Fresh alloc per step keeps the code
        // simple; the alloc is stream-ordered and dwarfed by the kernel
        // launch overhead itself on populations where this matters.
        let i_syn_dev = self.stream.memcpy_stod(i_syn).map_err(cuda_err)?;

        let n_i32 = self.n_neurons as i32;
        let dt_over_tau = dt_ms / params.tau_ms;
        let v_rest = params.v_rest;
        let v_thresh = params.v_thresh;
        let v_reset = params.v_reset;
        let refrac_steps = params.refrac_steps;

        let cfg = LaunchConfig::for_num_elems(self.n_neurons);
        let mut builder = self.stream.launch_builder(&self.kernel);
        builder.arg(&mut self.v_mem);
        builder.arg(&mut self.refrac);
        builder.arg(&mut self.spike);
        builder.arg(&i_syn_dev);
        builder.arg(&n_i32);
        builder.arg(&dt_over_tau);
        builder.arg(&v_rest);
        builder.arg(&v_thresh);
        builder.arg(&v_reset);
        builder.arg(&refrac_steps);
        // SAFETY: the kernel signature is
        //   (float* v_mem, unsigned int* refrac, unsigned char* spike,
        //    const float* i_syn, int n, float dt_over_tau,
        //    float v_rest, float v_thresh, float v_reset,
        //    unsigned int refrac_steps)
        // and the ten args pushed above match in order + type. The
        // `if (i >= n)` guard inside the kernel prevents out-of-range
        // writes to the four device buffers (all sized to n_neurons).
        unsafe { builder.launch(cfg) }.map_err(cuda_err)?;

        // Mirror the spike buffer back to the host. memcpy_dtov synchronises
        // the stream implicitly, so on return `out_spikes` is current.
        let host_spikes = self.stream.memcpy_dtov(&self.spike).map_err(cuda_err)?;
        self.last_spikes = host_spikes.iter().map(|&s| u32::from(s)).sum();
        out_spikes.clear();
        out_spikes.extend_from_slice(&host_spikes);

        Ok(())
    }

    /// Phase 9e — like [`Self::step`] but also adds a per-neuron
    /// `v_prep` delta to v_mem before integration. The CPU side
    /// computes `v_prep` from Poisson/pink noise + AHP/pump
    /// hyperpolarisation; the kernel handles the in-place add so the
    /// device-resident `v_mem` stays the source of truth across steps.
    ///
    /// `v_prep.len()` must equal `n_neurons` — same precondition as
    /// `i_syn`.
    ///
    /// # Errors
    ///
    /// Propagates any [`GpuError::Cuda`] from upload / kernel launch /
    /// download, plus a synthetic error on length mismatch.
    pub fn step_with_prep(
        &mut self,
        i_syn: &[f32],
        v_prep: &[f32],
        out_spikes: &mut Vec<u8>,
        params: &LifParams,
        dt_ms: f32,
    ) -> Result<(), GpuError> {
        if i_syn.len() != self.n_neurons as usize {
            return Err(GpuError::Cuda(format!(
                "LifGpu::step_with_prep: i_syn.len()={} but n_neurons={}",
                i_syn.len(),
                self.n_neurons
            )));
        }
        if v_prep.len() != self.n_neurons as usize {
            return Err(GpuError::Cuda(format!(
                "LifGpu::step_with_prep: v_prep.len()={} but n_neurons={}",
                v_prep.len(),
                self.n_neurons
            )));
        }

        let i_syn_dev = self.stream.memcpy_stod(i_syn).map_err(cuda_err)?;
        let v_prep_dev = self.stream.memcpy_stod(v_prep).map_err(cuda_err)?;

        let n_i32 = self.n_neurons as i32;
        let dt_over_tau = dt_ms / params.tau_ms;
        let v_rest = params.v_rest;
        let v_thresh = params.v_thresh;
        let v_reset = params.v_reset;
        let refrac_steps = params.refrac_steps;

        let cfg = LaunchConfig::for_num_elems(self.n_neurons);
        let mut builder = self.stream.launch_builder(&self.kernel_with_prep);
        builder.arg(&mut self.v_mem);
        builder.arg(&mut self.refrac);
        builder.arg(&mut self.spike);
        builder.arg(&i_syn_dev);
        builder.arg(&v_prep_dev);
        builder.arg(&n_i32);
        builder.arg(&dt_over_tau);
        builder.arg(&v_rest);
        builder.arg(&v_thresh);
        builder.arg(&v_reset);
        builder.arg(&refrac_steps);
        // SAFETY: kernel signature is
        //   (float* v_mem, unsigned int* refrac, unsigned char* spike,
        //    const float* i_syn, const float* v_prep, int n,
        //    float dt_over_tau, float v_rest, float v_thresh,
        //    float v_reset, unsigned int refrac_steps)
        // matching the eleven args pushed above. The `if (i >= n)`
        // guard inside keeps writes within bounds of all four
        // n_neurons-sized device buffers.
        unsafe { builder.launch(cfg) }.map_err(cuda_err)?;

        let host_spikes = self.stream.memcpy_dtov(&self.spike).map_err(cuda_err)?;
        self.last_spikes = host_spikes.iter().map(|&s| u32::from(s)).sum();
        out_spikes.clear();
        out_spikes.extend_from_slice(&host_spikes);
        Ok(())
    }

    /// Reset every neuron to rest on the device and clear refractory +
    /// spike mirrors. Used on e.g. brain-wide reset between episodes.
    ///
    /// # Errors
    ///
    /// Propagates any [`GpuError::Cuda`] from the three device writes.
    pub fn reset(&mut self, params: &LifParams) -> Result<(), GpuError> {
        let n = self.n_neurons as usize;
        let v_init: Vec<f32> = vec![params.v_rest; n];
        let refrac_init: Vec<u32> = vec![0; n];
        let spike_init: Vec<u8> = vec![0; n];
        self.stream
            .memcpy_htod(&v_init, &mut self.v_mem)
            .map_err(cuda_err)?;
        self.stream
            .memcpy_htod(&refrac_init, &mut self.refrac)
            .map_err(cuda_err)?;
        self.stream
            .memcpy_htod(&spike_init, &mut self.spike)
            .map_err(cuda_err)?;
        self.last_spikes = 0;
        Ok(())
    }

    /// Spikes recorded on the most recent [`LifGpu::step`]. Zero before
    /// the first step, and after every [`LifGpu::reset`].
    #[must_use]
    pub fn spikes_this_step(&self) -> u32 {
        self.last_spikes
    }

    /// Download device state into a fresh CPU [`LifState`]. Test / debug
    /// helper — not on the hot path.
    ///
    /// # Errors
    ///
    /// Propagates any [`GpuError::Cuda`] from the three device reads.
    pub fn snapshot(&self) -> Result<LifState, GpuError> {
        let v_mem = self.stream.memcpy_dtov(&self.v_mem).map_err(cuda_err)?;
        let refrac = self.stream.memcpy_dtov(&self.refrac).map_err(cuda_err)?;
        let spike = self.stream.memcpy_dtov(&self.spike).map_err(cuda_err)?;
        Ok(LifState {
            v_mem,
            refrac,
            spike,
            n_neurons: self.n_neurons,
            // GPU LIF runs in current-mode only (per V1 design lock —
            // CB forces CPU fallback). Snapshot is therefore CB-disabled.
            conductance: None,
        })
    }
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    fn cortical() -> LifParams {
        LifParams::default()
    }

    /// Single-neuron helper used by several tests.
    fn singleton_state(params: &LifParams) -> LifState {
        LifState::new(1, params)
    }

    #[test]
    fn default_params_are_cortical() {
        let p = LifParams::default();
        assert!((p.tau_ms - 20.0).abs() < f32::EPSILON);
        assert!((p.v_rest - -70.0).abs() < f32::EPSILON);
        assert!((p.v_thresh - -50.0).abs() < f32::EPSILON);
        assert!((p.v_reset - -70.0).abs() < f32::EPSILON);
        assert_eq!(p.refrac_steps, 2);
    }

    #[test]
    fn new_state_is_at_rest() {
        let p = cortical();
        let s = LifState::new(16, &p);
        assert_eq!(s.n_neurons, 16);
        assert_eq!(s.v_mem.len(), 16);
        assert_eq!(s.refrac.len(), 16);
        assert_eq!(s.spike.len(), 16);
        assert!(s.v_mem.iter().all(|&v| (v - p.v_rest).abs() < f32::EPSILON));
        assert!(s.refrac.iter().all(|&r| r == 0));
        assert!(s.spike.iter().all(|&s| s == 0));
        assert_eq!(s.n_spikes_this_step(), 0);
    }

    #[test]
    fn lif_rest_without_drive() {
        // No input, no drift from rest, no spikes — over many steps.
        let p = cortical();
        let mut s = LifState::new(8, &p);
        let i_syn = vec![0.0_f32; 8];
        for _ in 0..200 {
            lif_step_cpu(&mut s, &i_syn, &p, 1.0);
        }
        assert!(s.v_mem.iter().all(|&v| (v - p.v_rest).abs() < 1e-5));
        assert_eq!(s.n_spikes_this_step(), 0);
    }

    #[test]
    fn lif_spikes_under_constant_drive() {
        // Constant suprathreshold drive pulls steady-state above threshold;
        // the discrete update crosses it periodically. We assert "spiked
        // at least once" and "did not fire every step" — the latter rules
        // out a runaway update that ignores refractory. 25 mV puts v_ss
        // at v_rest + 25 = -45, comfortably above the -50 threshold.
        let p = cortical();
        let mut s = singleton_state(&p);
        let i_syn = vec![25.0_f32];
        let mut total = 0u32;
        for _ in 0..100 {
            lif_step_cpu(&mut s, &i_syn, &p, 1.0);
            total += s.n_spikes_this_step();
        }
        assert!(total > 0, "expected at least one spike under 25 mV drive");
        assert!(
            total < 100,
            "refractory period should suppress some steps, got {total}/100"
        );
    }

    #[test]
    fn lif_respects_refractory() {
        // Drive hard enough to guarantee a spike on the first tick, then
        // verify the next `refrac_steps` ticks are spike-suppressed and
        // the counter decrements monotonically.
        let p = cortical();
        let mut s = singleton_state(&p);
        // A large positive I_syn guarantees a first-step crossing.
        let i_syn = vec![1_000.0_f32];

        lif_step_cpu(&mut s, &i_syn, &p, 1.0);
        assert_eq!(s.spike[0], 1, "expected spike on driven first step");
        assert!((s.v_mem[0] - p.v_reset).abs() < 1e-5);
        assert_eq!(s.refrac[0], p.refrac_steps);

        // Now every step for `refrac_steps` must NOT spike even under
        // massive drive, and `refrac` decreases by exactly 1 each step.
        for i in 0..p.refrac_steps {
            let remaining_before = s.refrac[0];
            lif_step_cpu(&mut s, &i_syn, &p, 1.0);
            assert_eq!(s.spike[0], 0, "step {i}: spike should be suppressed");
            assert_eq!(
                s.refrac[0],
                remaining_before - 1,
                "step {i}: refrac must decrement"
            );
        }
        // Counter back to 0 — next driven step should be allowed to spike.
        assert_eq!(s.refrac[0], 0);
        lif_step_cpu(&mut s, &i_syn, &p, 1.0);
        assert_eq!(s.spike[0], 1, "post-refractory, should spike again");
    }

    #[test]
    fn reset_clears_state() {
        let p = cortical();
        let mut s = LifState::new(4, &p);
        // Force some non-rest state.
        let i_syn = vec![1_000.0_f32; 4];
        for _ in 0..5 {
            lif_step_cpu(&mut s, &i_syn, &p, 1.0);
        }
        assert!(s.refrac.iter().any(|&r| r > 0) || s.spike.iter().any(|&sp| sp > 0));

        s.reset(&p);
        assert!(s.v_mem.iter().all(|&v| (v - p.v_rest).abs() < f32::EPSILON));
        assert!(s.refrac.iter().all(|&r| r == 0));
        assert!(s.spike.iter().all(|&sp| sp == 0));
        assert_eq!(s.n_spikes_this_step(), 0);
    }

    #[test]
    fn subthreshold_drive_never_fires() {
        // I_syn chosen so steady-state stays below threshold:
        //   v_ss = v_rest + I_syn = -70 + 10 = -60 mV < -50 mV.
        let p = cortical();
        let mut s = LifState::new(4, &p);
        let i_syn = vec![10.0_f32; 4];
        let mut total = 0u32;
        for _ in 0..500 {
            lif_step_cpu(&mut s, &i_syn, &p, 1.0);
            total += s.n_spikes_this_step();
        }
        assert_eq!(total, 0, "subthreshold drive must never fire");
        // Voltage has settled near -60 mV.
        for &v in &s.v_mem {
            assert!(
                (v - -60.0).abs() < 1e-3,
                "expected steady-state near -60 mV, got {v}"
            );
        }
    }

    #[test]
    fn n_spikes_counts_all_firing_neurons() {
        let p = cortical();
        let mut s = LifState::new(5, &p);
        // Three neurons suprathreshold, two subthreshold.
        let i_syn = vec![1_000.0, 0.0, 1_000.0, 0.0, 1_000.0];
        lif_step_cpu(&mut s, &i_syn, &p, 1.0);
        assert_eq!(s.n_spikes_this_step(), 3);
    }

    // --------- GPU path tests (compiled only with --features cuda) ---------

    #[cfg(feature = "cuda")]
    fn cuda_available() -> bool {
        nimcp_gpu::probe_device().is_ok()
    }

    #[cfg(feature = "cuda")]
    #[test]
    fn gpu_new_allocates_rest_state() {
        if !cuda_available() {
            eprintln!("skipping: no CUDA device on this host");
            return;
        }
        let p = cortical();
        let gpu = LifGpu::new(64, &p).unwrap();
        assert_eq!(gpu.n_neurons(), 64);
        assert_eq!(gpu.spikes_this_step(), 0);

        let snap = gpu.snapshot().unwrap();
        assert_eq!(snap.n_neurons, 64);
        assert!(snap.v_mem.iter().all(|&v| (v - p.v_rest).abs() < 1e-5));
        assert!(snap.refrac.iter().all(|&r| r == 0));
        assert!(snap.spike.iter().all(|&s| s == 0));
    }

    #[cfg(feature = "cuda")]
    #[test]
    fn gpu_reset_round_trips() {
        if !cuda_available() {
            eprintln!("skipping: no CUDA device on this host");
            return;
        }
        let p = cortical();
        let mut gpu = LifGpu::new(32, &p).unwrap();
        let mut spikes = Vec::new();
        let drive = vec![1_000.0_f32; 32];
        for _ in 0..5 {
            gpu.step(&drive, &mut spikes, &p, 1.0).unwrap();
        }
        gpu.reset(&p).unwrap();
        assert_eq!(gpu.spikes_this_step(), 0);
        let snap = gpu.snapshot().unwrap();
        assert!(snap.v_mem.iter().all(|&v| (v - p.v_rest).abs() < 1e-5));
        assert!(snap.refrac.iter().all(|&r| r == 0));
        assert!(snap.spike.iter().all(|&s| s == 0));
    }

    #[cfg(feature = "cuda")]
    #[test]
    fn gpu_rejects_wrong_input_length() {
        if !cuda_available() {
            eprintln!("skipping: no CUDA device on this host");
            return;
        }
        let p = cortical();
        let mut gpu = LifGpu::new(10, &p).unwrap();
        let mut spikes = Vec::new();
        let bad = vec![0.0_f32; 7];
        assert!(gpu.step(&bad, &mut spikes, &p, 1.0).is_err());
    }

    /// Deterministic LCG so we don't drag `rand` into the hot-path deps
    /// of this module's tests.
    #[cfg(feature = "cuda")]
    struct Lcg(u64);

    #[cfg(feature = "cuda")]
    impl Lcg {
        fn new(seed: u64) -> Self {
            Self(seed | 1)
        }
        fn next_f32(&mut self) -> f32 {
            self.0 = self
                .0
                .wrapping_mul(6_364_136_223_846_793_005)
                .wrapping_add(1);
            let x = ((self.0 >> 33) as u32) as f32;
            x / (u32::MAX as f32) - 0.5
        }
    }

    #[cfg(feature = "cuda")]
    #[test]
    fn lif_cpu_gpu_match() {
        if !cuda_available() {
            eprintln!("skipping: no CUDA device on this host");
            return;
        }
        // 1000 neurons, 50 steps, random I_syn in roughly [-5, +25] mV
        // so some neurons spike and most do not — exercises both
        // branches of the kernel on every step.
        let p = cortical();
        let n: u32 = 1000;
        let mut cpu = LifState::new(n, &p);
        let mut gpu = LifGpu::new(n, &p).unwrap();

        let mut rng = Lcg::new(0xDEAD_BEEF_C0FF_EE42);
        let mut spikes_gpu = Vec::new();
        let dt_ms = 1.0_f32;

        for step in 0..50 {
            // Build this step's I_syn: biased toward 10 mV so ~half of
            // the population is near threshold under sustained drive.
            let i_syn: Vec<f32> = (0..n)
                .map(|_| 10.0 + rng.next_f32() * 30.0) // [-5, +25] mV
                .collect();

            lif_step_cpu(&mut cpu, &i_syn, &p, dt_ms);
            gpu.step(&i_syn, &mut spikes_gpu, &p, dt_ms).unwrap();

            // Spikes must match bit-exactly — the kernel encodes exactly
            // the same decision boundary as the CPU path.
            assert_eq!(
                cpu.spike, spikes_gpu,
                "step {step}: spike mask divergence (cpu vs gpu)"
            );
            assert_eq!(
                cpu.n_spikes_this_step(),
                gpu.spikes_this_step(),
                "step {step}: spike count divergence"
            );

            // Voltages + refractories must match closely. f32 rounding at
            // the FMA/mul-add boundary can differ between CPU scalar ops
            // and the GPU, so use a small tolerance for v_mem; refrac is
            // integer and must match exactly.
            let snap = gpu.snapshot().unwrap();
            for i in 0..n as usize {
                assert_eq!(
                    cpu.refrac[i], snap.refrac[i],
                    "step {step}: refrac[{i}] cpu={} gpu={}",
                    cpu.refrac[i], snap.refrac[i]
                );
                let diff = (cpu.v_mem[i] - snap.v_mem[i]).abs();
                assert!(
                    diff < 1e-4,
                    "step {step}: v_mem[{i}] cpu={} gpu={} diff={}",
                    cpu.v_mem[i],
                    snap.v_mem[i],
                    diff,
                );
            }
        }
    }
}
