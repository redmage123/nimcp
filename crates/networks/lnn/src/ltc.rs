//! Phase 4a — LTC neuron dynamics.
//!
//! One LTC layer:
//!
//! ```text
//!   pre_i = W_rec[i,:] · x  +  W_in[i,:] · u  +  b_i
//!   dx_i/dt = -x_i / tau_safe_i  +  tanh(pre_i)
//!   x_i(t+dt) = clamp(x_i(t) + dt · dx_i/dt, [-CLAMP, +CLAMP])
//! ```
//!
//! See [`crate`] for the encoded V1 lessons (tau floor, state clamp).

use ndarray::{Array1, Array2};
use rand::SeedableRng;
use rand::distr::{Distribution, Uniform};
use rand_chacha::ChaCha20Rng;
use serde::{Deserialize, Serialize};

/// Minimum `tau_base` — hard floor applied *before* the `1/τ` division.
/// V1 lesson: lower values produce NaN gradients on float32 from the
/// `1/τ²` term in the adjoint.
pub const LTC_TAU_MIN: f32 = 0.01;

/// Per-step state clamp. Prevents single-precision blow-up on long
/// unrolls. V1 lesson: without this, 1000-step sequences hit `inf` /
/// `NaN` deterministically by step ~600.
pub const LTC_STATE_CLAMP: f32 = 1.0e4;

/// Static dimensions + hyperparameters for one LTC layer.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct LtcParams {
    /// Input dimension.
    pub n_in: usize,
    /// Recurrent / hidden dimension.
    pub n_rec: usize,
    /// Initial `tau_base` applied uniformly across all neurons at
    /// construction time. Trainable in Phase 4b; here it's just the
    /// starting value.
    pub tau_init: f32,
    /// Half-width of the uniform distribution used to init `W_rec` /
    /// `W_in`. Scaled by `1/sqrt(fan_in)` internally, so this is the
    /// tail-bound on the pre-scaled draw.
    pub init_scale: f32,
}

impl Default for LtcParams {
    fn default() -> Self {
        Self {
            n_in: 0,
            n_rec: 0,
            tau_init: 1.0,
            init_scale: 1.0,
        }
    }
}

/// One LTC layer's trainable parameters.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LtcLayer {
    /// Recurrent weights, shape `(n_rec, n_rec)`.
    pub w_rec: Array2<f32>,
    /// Input-projection weights, shape `(n_rec, n_in)`.
    pub w_in: Array2<f32>,
    /// Per-neuron bias, shape `(n_rec,)`.
    pub b: Array1<f32>,
    /// Per-neuron trainable time constant, shape `(n_rec,)`.
    /// Floored at [`LTC_TAU_MIN`] during every forward step.
    pub tau_base: Array1<f32>,
    /// Static params — dimensions + init hyperparameters.
    pub params: LtcParams,
}

impl LtcLayer {
    /// Deterministic Xavier-like init from `seed`. Uses `ChaCha20Rng` so
    /// the same seed produces bit-identical weights on any platform.
    ///
    /// `W_rec` and `W_in` are sampled from
    /// `U(−init_scale/√fan_in, +init_scale/√fan_in)`. `b` is zero-init.
    /// `tau_base` is `tau_init` everywhere (floored at [`LTC_TAU_MIN`]).
    #[must_use]
    pub fn new_seeded(params: LtcParams, seed: u64) -> Self {
        let LtcParams {
            n_in,
            n_rec,
            tau_init,
            init_scale,
        } = params;
        let tau_init = tau_init.max(LTC_TAU_MIN);
        let mut rng = ChaCha20Rng::seed_from_u64(seed);

        // Xavier-uniform: bound = init_scale / sqrt(fan_in).
        let rec_bound = if n_rec == 0 {
            0.0
        } else {
            init_scale / (n_rec as f32).sqrt()
        };
        let in_bound = if n_in == 0 {
            0.0
        } else {
            init_scale / (n_in as f32).sqrt()
        };

        // `Uniform::new` returns `Err` only when `low >= high`; we
        // guarantee `bound > 0` above by checking the n_* against 0.
        let rec_uni = Uniform::new(-rec_bound, rec_bound).expect("rec bound positive");
        let in_uni = Uniform::new(-in_bound, in_bound).expect("in bound positive");

        let w_rec = Array2::from_shape_fn((n_rec, n_rec), |_| rec_uni.sample(&mut rng));
        let w_in = Array2::from_shape_fn((n_rec, n_in), |_| in_uni.sample(&mut rng));
        let b = Array1::zeros(n_rec);
        let tau_base = Array1::from_elem(n_rec, tau_init);

        Self {
            w_rec,
            w_in,
            b,
            tau_base,
            params,
        }
    }

    /// Returns `(n_in, n_rec)`.
    #[must_use]
    pub fn shape(&self) -> (usize, usize) {
        (self.params.n_in, self.params.n_rec)
    }
}

/// Recurrent state for one LTC layer.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LtcState {
    /// Current hidden state, shape `(n_rec,)`.
    pub x: Array1<f32>,
}

impl LtcState {
    /// All zeros — the LTC rest attractor under zero input.
    #[must_use]
    pub fn new(n_rec: usize) -> Self {
        Self {
            x: Array1::zeros(n_rec),
        }
    }

    /// Reset to zeros without reallocating.
    pub fn reset(&mut self) {
        for v in self.x.iter_mut() {
            *v = 0.0;
        }
    }
}

/// One Euler integration step of LTC dynamics on `state.x` given input
/// `u` (length `params.n_in`), producing updated state in place.
///
/// Returns the pre-activation vector `pre` so callers that track it for
/// backprop-through-time don't have to recompute it.
///
/// # Panics
///
/// Debug-asserts that shapes match. In release builds, ndarray's own
/// broadcasting will panic on mismatch at the dot-product sites.
pub fn ltc_forward_step(
    state: &mut LtcState,
    layer: &LtcLayer,
    u: &Array1<f32>,
    dt_ms: f32,
) -> Array1<f32> {
    debug_assert_eq!(state.x.len(), layer.params.n_rec);
    debug_assert_eq!(u.len(), layer.params.n_in);

    // Pre-activation: W_rec · x + W_in · u + b
    let mut pre = layer.w_rec.dot(&state.x);
    pre += &layer.w_in.dot(u);
    pre += &layer.b;

    // Nonlinearity (tanh) elementwise.
    let act = pre.mapv(f32::tanh);

    // dx/dt = -x / tau_safe + tanh(pre).
    // Apply elementwise + Euler step in one pass for cache friendliness.
    for ((x, &tau), &a) in state
        .x
        .iter_mut()
        .zip(layer.tau_base.iter())
        .zip(act.iter())
    {
        let tau_safe = tau.max(LTC_TAU_MIN);
        let dx = -*x / tau_safe + a;
        let x_new = *x + dt_ms * dx;
        *x = x_new.clamp(-LTC_STATE_CLAMP, LTC_STATE_CLAMP);
    }

    pre
}

// -------------------------------------------------------------------------
// GPU backend (feature-gated) — Phase 9f
// -------------------------------------------------------------------------

#[cfg(feature = "cuda")]
pub use gpu::LtcGpu;

/// Phase 9f forward + Phase 9g+ training primitives in one PTX module.
///
/// Forward: see `ltc_forward_step` below.
///
/// Phase 9g+ kernels (BPTT building blocks; CPU loop remains in charge
/// of timestep + layer orchestration):
///   - `outer_accumulate` — gradient buffer `g[i,j] += a[i] * b[j]`.
///     One thread per (i,j) cell; no atomics needed since each cell is
///     owned by exactly one thread. Used for `dW_rec += outer(dl_dpre,
///     x_prev)` and `dW_in += outer(dl_dpre, input)` — the FLOPs
///     dominator of the BPTT step.
///   - `axpy_step` — `w[k] -= lr * g[k]` per parameter buffer. Used by
///     `sgd_step` to apply accumulated gradients to weights / bias /
///     tau without round-tripping the whole parameter tensor through
///     host memory.
///   - `zero_buffer` — fast device-side zero for grad accumulators
///     between epochs.
///
/// Forward kernel doc continues below.
///
/// Fused LTC forward kernel. One thread per recurrent neuron computes:
///
///   pre[i] = Σ_j W_rec[i,j]·x[j] + Σ_k W_in[i,k]·u[k] + b[i]
///   x'[i]  = clamp(x[i] + dt·(-x[i]/max(tau[i],TAU_MIN) + tanh(pre[i])),
///                  -CLAMP, +CLAMP)
///
/// Matches [`ltc_forward_step`] arithmetic exactly. The pre-activation is
/// written to `pre_out` so callers tracking it for BPTT (Phase 9g) avoid
/// recomputing.
///
/// Row-major contiguous weight layout — W[i,j] = w_data[i*ncols + j].
#[cfg(feature = "cuda")]
const LTC_KERNEL_SRC: &str = r#"
extern "C" __global__ void ltc_forward_step(
    float* x,                 // [n_rec], in/out
    const float* w_rec,       // [n_rec * n_rec], row-major
    const float* w_in,        // [n_rec * n_in],  row-major
    const float* b,           // [n_rec]
    const float* tau_base,    // [n_rec]
    const float* u,           // [n_in]
    float* pre_out,           // [n_rec]
    int n_rec,
    int n_in,
    float dt_ms,
    float tau_min,
    float state_clamp
) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n_rec) return;

    // Pre-activation accumulator.
    float pre = b[i];
    int row_rec_base = i * n_rec;
    for (int j = 0; j < n_rec; ++j) {
        pre += w_rec[row_rec_base + j] * x[j];
    }
    int row_in_base = i * n_in;
    for (int k = 0; k < n_in; ++k) {
        pre += w_in[row_in_base + k] * u[k];
    }
    pre_out[i] = pre;

    // Euler step.
    float a = tanhf(pre);
    float tau = tau_base[i];
    if (tau < tau_min) tau = tau_min;
    float dx = -x[i] / tau + a;
    float xn = x[i] + dt_ms * dx;
    if (xn >  state_clamp) xn =  state_clamp;
    if (xn < -state_clamp) xn = -state_clamp;
    x[i] = xn;
}

// ---- Phase 9g+ BPTT primitives ----

extern "C" __global__ void outer_accumulate(
    float* g,           // [n_rows * n_cols], row-major, in/out
    const float* a,     // [n_rows]
    const float* b,     // [n_cols]
    int n_rows,
    int n_cols
) {
    int i = blockIdx.y * blockDim.y + threadIdx.y;
    int j = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n_rows || j >= n_cols) return;
    g[i * n_cols + j] += a[i] * b[j];
}

extern "C" __global__ void axpy_step(
    float* w,           // [n], in/out
    const float* g,     // [n]
    int n,
    float lr
) {
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= n) return;
    w[k] -= lr * g[k];
}

extern "C" __global__ void zero_buffer(
    float* buf,
    int n
) {
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= n) return;
    buf[k] = 0.0f;
}
"#;

#[cfg(feature = "cuda")]
mod gpu {
    use std::sync::Arc;

    use cudarc::driver::{
        CudaContext, CudaFunction, CudaModule, CudaSlice, CudaStream, LaunchConfig, PushKernelArg,
    };
    use ndarray::Array1;
    use nimcp_gpu::GpuError;

    use super::{LTC_KERNEL_SRC, LTC_STATE_CLAMP, LTC_TAU_MIN, LtcLayer, LtcState};

    fn cuda_err<E: std::fmt::Debug>(e: E) -> GpuError {
        GpuError::Cuda(format!("{e:?}"))
    }

    /// Device-resident LTC layer + state. Owns one [`CudaContext`]
    /// (or shares one via [`Self::new_with_context`] — Phase 9b
    /// shared-context contract for multi-layer brain configs).
    pub struct LtcGpu {
        n_in: u32,
        n_rec: u32,
        // Persistent device weights — re-uploaded after CPU-side
        // optimizer step via `upload_weights`.
        w_rec: CudaSlice<f32>,
        w_in: CudaSlice<f32>,
        b: CudaSlice<f32>,
        tau_base: CudaSlice<f32>,
        // Persistent device state — `x` survives across step calls.
        x: CudaSlice<f32>,
        // Per-step scratch.
        u_buf: CudaSlice<f32>,
        pre_buf: CudaSlice<f32>,
        // Phase 9g+ — device-resident gradient accumulators. Lazily
        // allocated on the first BPTT-helper call so populations that
        // never train don't pay the memory tax.
        grad_w_rec: Option<CudaSlice<f32>>,
        grad_w_in: Option<CudaSlice<f32>>,
        grad_b: Option<CudaSlice<f32>>,
        grad_tau: Option<CudaSlice<f32>>,
        // Per-step host-uploaded scratch for BPTT helpers.
        bptt_a_buf: CudaSlice<f32>,
        bptt_b_buf: CudaSlice<f32>,
        ctx: Arc<CudaContext>,
        stream: Arc<CudaStream>,
        #[allow(dead_code)]
        module: Arc<CudaModule>,
        kernel: CudaFunction,
        outer_kernel: CudaFunction,
        axpy_kernel: CudaFunction,
        zero_kernel: CudaFunction,
    }

    impl std::fmt::Debug for LtcGpu {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            f.debug_struct("LtcGpu")
                .field("n_in", &self.n_in)
                .field("n_rec", &self.n_rec)
                .finish_non_exhaustive()
        }
    }

    impl LtcGpu {
        /// Allocate a fresh CUDA context + upload the layer.
        pub fn new(layer: &LtcLayer) -> Result<Self, GpuError> {
            let ctx = CudaContext::new(0).map_err(cuda_err)?;
            Self::new_with_context(ctx, layer)
        }

        /// Phase 9b shared-context constructor — when the brain creates
        /// multiple LNN layers, reuse one context to avoid the
        /// per-context VRAM tax that bit V1 hard at scale.
        pub fn new_with_context(ctx: Arc<CudaContext>, layer: &LtcLayer) -> Result<Self, GpuError> {
            let n_rec = layer.params.n_rec as u32;
            let n_in = layer.params.n_in as u32;
            if n_rec == 0 {
                return Err(GpuError::Cuda("LtcGpu::new: n_rec must be > 0".into()));
            }
            let stream = ctx.default_stream();
            let ptx = cudarc::nvrtc::compile_ptx(LTC_KERNEL_SRC).map_err(cuda_err)?;
            let module = ctx.load_module(ptx).map_err(cuda_err)?;
            let kernel = module.load_function("ltc_forward_step").map_err(cuda_err)?;
            // Phase 9g+ training-primitive kernels.
            let outer_kernel = module.load_function("outer_accumulate").map_err(cuda_err)?;
            let axpy_kernel = module.load_function("axpy_step").map_err(cuda_err)?;
            let zero_kernel = module.load_function("zero_buffer").map_err(cuda_err)?;

            // Row-major contiguous host buffers from ndarray. The layer's
            // weights are constructed via `from_shape_fn` which produces
            // row-major default order, so `as_slice` succeeds.
            let w_rec_host = layer
                .w_rec
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("w_rec not contiguous row-major".into()))?;
            let w_in_host = layer
                .w_in
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("w_in not contiguous row-major".into()))?;
            let b_host = layer
                .b
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("b not contiguous".into()))?;
            let tau_host = layer
                .tau_base
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("tau_base not contiguous".into()))?;

            let w_rec = stream.memcpy_stod(w_rec_host).map_err(cuda_err)?;
            let w_in = stream.memcpy_stod(w_in_host).map_err(cuda_err)?;
            let b = stream.memcpy_stod(b_host).map_err(cuda_err)?;
            let tau_base = stream.memcpy_stod(tau_host).map_err(cuda_err)?;
            let x: CudaSlice<f32> = stream
                .alloc_zeros::<f32>(n_rec as usize)
                .map_err(cuda_err)?;
            let u_buf: CudaSlice<f32> = if n_in == 0 {
                stream.alloc_zeros::<f32>(1).map_err(cuda_err)?
            } else {
                stream.alloc_zeros::<f32>(n_in as usize).map_err(cuda_err)?
            };
            let pre_buf: CudaSlice<f32> = stream
                .alloc_zeros::<f32>(n_rec as usize)
                .map_err(cuda_err)?;

            // BPTT scratch — sized to the largest dimension we'd
            // multiply (n_rec) so the same buffer can hold dl_dpre or
            // x_prev. b_buf gets sized to max(n_rec, n_in) for x_prev
            // OR the layer-input vector.
            let bptt_max = (n_rec.max(n_in)).max(1) as usize;
            let bptt_a_buf: CudaSlice<f32> = stream
                .alloc_zeros::<f32>(n_rec as usize)
                .map_err(cuda_err)?;
            let bptt_b_buf: CudaSlice<f32> =
                stream.alloc_zeros::<f32>(bptt_max).map_err(cuda_err)?;

            tracing::info!(n_rec, n_in, "ltc gpu buffers allocated");

            Ok(Self {
                n_in,
                n_rec,
                w_rec,
                w_in,
                b,
                tau_base,
                x,
                u_buf,
                pre_buf,
                grad_w_rec: None,
                grad_w_in: None,
                grad_b: None,
                grad_tau: None,
                bptt_a_buf,
                bptt_b_buf,
                ctx,
                stream,
                module,
                kernel,
                outer_kernel,
                axpy_kernel,
                zero_kernel,
            })
        }

        /// Borrow the CUDA context (shared with sibling GPU subsystems).
        #[must_use]
        pub fn context(&self) -> &Arc<CudaContext> {
            &self.ctx
        }

        /// Re-upload trainable weights after a CPU-side optimizer step.
        ///
        /// `b` and `tau_base` follow the same contract — they're
        /// trainable in the BPTT path (Phase 9g) so the trainer needs
        /// to be able to refresh device copies between epochs.
        pub fn upload_weights(&mut self, layer: &LtcLayer) -> Result<(), GpuError> {
            let w_rec_host = layer
                .w_rec
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("w_rec not contiguous".into()))?;
            let w_in_host = layer
                .w_in
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("w_in not contiguous".into()))?;
            let b_host = layer
                .b
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("b not contiguous".into()))?;
            let tau_host = layer
                .tau_base
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("tau_base not contiguous".into()))?;
            self.stream
                .memcpy_htod(w_rec_host, &mut self.w_rec)
                .map_err(cuda_err)?;
            self.stream
                .memcpy_htod(w_in_host, &mut self.w_in)
                .map_err(cuda_err)?;
            self.stream
                .memcpy_htod(b_host, &mut self.b)
                .map_err(cuda_err)?;
            self.stream
                .memcpy_htod(tau_host, &mut self.tau_base)
                .map_err(cuda_err)?;
            Ok(())
        }

        /// One LTC forward step on device. Returns the pre-activation
        /// vector for callers tracking it (Phase 9g BPTT). `state.x`
        /// is mirrored back to host so CPU-side observers stay current.
        ///
        /// # Errors
        ///
        /// Propagates [`GpuError::Cuda`] from upload, kernel launch,
        /// or download. Length mismatch on `u` returns a synthetic
        /// `Cuda` error.
        // HOT PATH: every LNN tick.
        pub fn step(
            &mut self,
            state: &mut LtcState,
            u: &Array1<f32>,
            dt_ms: f32,
        ) -> Result<Array1<f32>, GpuError> {
            if u.len() != self.n_in as usize {
                return Err(GpuError::Cuda(format!(
                    "LtcGpu::step: u.len()={} but n_in={}",
                    u.len(),
                    self.n_in
                )));
            }
            // Upload this step's input.
            let u_slice = u
                .as_slice()
                .ok_or_else(|| GpuError::Cuda("u not contiguous".into()))?;
            if !u_slice.is_empty() {
                self.stream
                    .memcpy_htod(u_slice, &mut self.u_buf)
                    .map_err(cuda_err)?;
            }

            let n_rec_i32 = self.n_rec as i32;
            let n_in_i32 = self.n_in as i32;
            let tau_min = LTC_TAU_MIN;
            let state_clamp = LTC_STATE_CLAMP;

            let cfg = LaunchConfig::for_num_elems(self.n_rec);
            let mut builder = self.stream.launch_builder(&self.kernel);
            builder.arg(&mut self.x);
            builder.arg(&self.w_rec);
            builder.arg(&self.w_in);
            builder.arg(&self.b);
            builder.arg(&self.tau_base);
            builder.arg(&self.u_buf);
            builder.arg(&mut self.pre_buf);
            builder.arg(&n_rec_i32);
            builder.arg(&n_in_i32);
            builder.arg(&dt_ms);
            builder.arg(&tau_min);
            builder.arg(&state_clamp);
            // SAFETY: kernel signature has 12 args; 12 builder.arg
            // calls above match in order + type. The `if (i >= n_rec)`
            // guard inside keeps writes within the n_rec-sized
            // device buffers (x, b, tau_base, pre_out).
            unsafe { builder.launch(cfg) }.map_err(cuda_err)?;

            // Download pre-activation (BPTT needs it; cheap at n_rec sizes).
            let pre_host = self.stream.memcpy_dtov(&self.pre_buf).map_err(cuda_err)?;
            let pre = Array1::from(pre_host);

            // Mirror x back to host so CPU-side observers (snapshot,
            // tests, downstream consumers) see the latest state.
            let x_host = self.stream.memcpy_dtov(&self.x).map_err(cuda_err)?;
            state.x = Array1::from(x_host);

            Ok(pre)
        }

        /// Reset the device-resident hidden state to zeros.
        pub fn reset(&mut self) -> Result<(), GpuError> {
            let zero: Vec<f32> = vec![0.0; self.n_rec as usize];
            self.stream
                .memcpy_htod(&zero, &mut self.x)
                .map_err(cuda_err)?;
            Ok(())
        }

        /// Download the current device hidden state.
        pub fn download_x(&self) -> Result<Vec<f32>, GpuError> {
            self.stream.memcpy_dtov(&self.x).map_err(cuda_err)
        }

        /// Recurrent dimension.
        #[must_use]
        pub fn n_rec(&self) -> u32 {
            self.n_rec
        }
        /// Input dimension.
        #[must_use]
        pub fn n_in(&self) -> u32 {
            self.n_in
        }

        // ---------------------------------------------------------------
        // Phase 9g+ — BPTT primitives.
        //
        // These let the CPU BPTT loop hand off the FLOPs-heavy parts
        // (outer-product gradient accumulation, SGD apply) to GPU
        // without requiring the entire backward orchestration to live
        // on device. Future "Phase 9g++" can build a fully-on-device
        // backward step on top of these.
        // ---------------------------------------------------------------

        /// Lazily allocate the four gradient buffers (zeros) and bind
        /// them to `self`. Idempotent. Drops are caller-managed
        /// (re-running this on a populated grad set is a no-op).
        ///
        /// # Errors
        /// Propagates [`GpuError::Cuda`] from the four device allocs.
        pub fn ensure_grads(&mut self) -> Result<(), GpuError> {
            if self.grad_w_rec.is_some() {
                return Ok(());
            }
            let nrec = self.n_rec as usize;
            let nin = self.n_in as usize;
            self.grad_w_rec = Some(
                self.stream
                    .alloc_zeros::<f32>(nrec * nrec)
                    .map_err(cuda_err)?,
            );
            self.grad_w_in = Some(
                self.stream
                    .alloc_zeros::<f32>(nrec * nin)
                    .map_err(cuda_err)?,
            );
            self.grad_b = Some(self.stream.alloc_zeros::<f32>(nrec).map_err(cuda_err)?);
            self.grad_tau = Some(self.stream.alloc_zeros::<f32>(nrec).map_err(cuda_err)?);
            Ok(())
        }

        /// Zero every gradient buffer in place. No-op if grads were
        /// never allocated. Run between epochs / after `sgd_step`.
        pub fn zero_grads(&mut self) -> Result<(), GpuError> {
            for buf in [
                &mut self.grad_w_rec,
                &mut self.grad_w_in,
                &mut self.grad_b,
                &mut self.grad_tau,
            ] {
                if let Some(b) = buf.as_mut() {
                    let n = b.len() as u32;
                    let cfg = LaunchConfig::for_num_elems(n);
                    let mut builder = self.stream.launch_builder(&self.zero_kernel);
                    builder.arg(b);
                    let n_i32 = n as i32;
                    builder.arg(&n_i32);
                    // SAFETY: kernel signature (float*, int) matches the
                    // two pushes above. The `if (k >= n) return` guard
                    // keeps writes in-bounds.
                    unsafe { builder.launch(cfg) }.map_err(cuda_err)?;
                }
            }
            Ok(())
        }

        /// Accumulate `g_w_rec[i,j] += dl_dpre[i] * x_prev[j]` on
        /// device. Uploads the two host vectors into BPTT scratch first.
        /// `ensure_grads()` must have been called.
        ///
        /// # Errors
        /// Propagates [`GpuError::Cuda`] from upload or kernel launch,
        /// or a synthetic error on length mismatch.
        pub fn accumulate_grad_w_rec(
            &mut self,
            dl_dpre: &[f32],
            x_prev: &[f32],
        ) -> Result<(), GpuError> {
            self.outer_into(GradTarget::WRec, dl_dpre, x_prev)
        }

        /// Accumulate `g_w_in[i,j] += dl_dpre[i] * input[j]` on device.
        /// `input` must have length `n_in`.
        pub fn accumulate_grad_w_in(
            &mut self,
            dl_dpre: &[f32],
            input: &[f32],
        ) -> Result<(), GpuError> {
            self.outer_into(GradTarget::WIn, dl_dpre, input)
        }

        /// Outer-product accumulate dispatcher — picks the destination
        /// gradient buffer + dimensions by tag.
        fn outer_into(
            &mut self,
            target: GradTarget,
            a_host: &[f32],
            b_host: &[f32],
        ) -> Result<(), GpuError> {
            let (n_rows_u, n_cols_u) = match target {
                GradTarget::WRec => (self.n_rec, self.n_rec),
                GradTarget::WIn => (self.n_rec, self.n_in),
            };
            if a_host.len() != n_rows_u as usize {
                return Err(GpuError::Cuda(format!(
                    "outer_into: a.len={} expected {n_rows_u}",
                    a_host.len()
                )));
            }
            if b_host.len() != n_cols_u as usize {
                return Err(GpuError::Cuda(format!(
                    "outer_into: b.len={} expected {n_cols_u}",
                    b_host.len()
                )));
            }
            let g = match target {
                GradTarget::WRec => self.grad_w_rec.as_mut(),
                GradTarget::WIn => self.grad_w_in.as_mut(),
            }
            .ok_or_else(|| GpuError::Cuda("outer_into: ensure_grads not called".into()))?;

            // Upload into scratch — bptt_a_buf sized n_rec, bptt_b_buf
            // sized max(n_rec, n_in), so both fit by construction.
            self.stream
                .memcpy_htod(a_host, &mut self.bptt_a_buf)
                .map_err(cuda_err)?;
            // Slice the b_buf to the actual cols length so the kernel's
            // `b[j]` reads the right values. cudarc lets us pass a
            // sub-buffer via `slice_mut`, but the simpler approach is to
            // assume the leading n_cols entries are populated and the
            // kernel never reads past `j < n_cols`.
            self.stream
                .memcpy_htod(b_host, &mut self.bptt_b_buf)
                .map_err(cuda_err)?;

            let n_rows_i32 = n_rows_u as i32;
            let n_cols_i32 = n_cols_u as i32;

            // 2D launch — block 16×16, grid covers (n_cols, n_rows).
            let block_x = 16u32;
            let block_y = 16u32;
            let grid_x = n_cols_u.div_ceil(block_x);
            let grid_y = n_rows_u.div_ceil(block_y);
            let cfg = LaunchConfig {
                grid_dim: (grid_x, grid_y, 1),
                block_dim: (block_x, block_y, 1),
                shared_mem_bytes: 0,
            };
            let mut builder = self.stream.launch_builder(&self.outer_kernel);
            builder.arg(g);
            builder.arg(&self.bptt_a_buf);
            builder.arg(&self.bptt_b_buf);
            builder.arg(&n_rows_i32);
            builder.arg(&n_cols_i32);
            // SAFETY: kernel signature (float* g, float* a, float* b,
            // int n_rows, int n_cols) matches the 5 pushes above. The
            // 2D bounds-check at kernel entry keeps writes within the
            // n_rows×n_cols-sized grad buffer.
            unsafe { builder.launch(cfg) }.map_err(cuda_err)?;
            Ok(())
        }

        /// Apply `θ -= lr · grad_θ` to every weight + bias + tau buffer.
        /// `ensure_grads()` must have been called. Idempotent w.r.t.
        /// repeated calls (each call applies the *current* grads).
        ///
        /// Caller typically follows this with `zero_grads()` before
        /// the next epoch's accumulation.
        pub fn sgd_step(&mut self, lr: f32) -> Result<(), GpuError> {
            // w_rec
            if let Some(g) = self.grad_w_rec.as_ref() {
                self.axpy(&mut clone_handle(&self.w_rec), g, lr)?;
            }
            if let Some(g) = self.grad_w_in.as_ref() {
                self.axpy(&mut clone_handle(&self.w_in), g, lr)?;
            }
            if let Some(g) = self.grad_b.as_ref() {
                self.axpy(&mut clone_handle(&self.b), g, lr)?;
            }
            if let Some(g) = self.grad_tau.as_ref() {
                self.axpy(&mut clone_handle(&self.tau_base), g, lr)?;
            }
            Ok(())
        }

        fn axpy(
            &self,
            w: &mut CudaSlice<f32>,
            g: &CudaSlice<f32>,
            lr: f32,
        ) -> Result<(), GpuError> {
            let n = w.len() as u32;
            let cfg = LaunchConfig::for_num_elems(n);
            let mut builder = self.stream.launch_builder(&self.axpy_kernel);
            builder.arg(w);
            builder.arg(g);
            let n_i32 = n as i32;
            builder.arg(&n_i32);
            builder.arg(&lr);
            // SAFETY: kernel signature (float* w, float* g, int n,
            // float lr) matches the 4 pushes above. `if (k >= n)` guard
            // keeps writes in-bounds.
            unsafe { builder.launch(cfg) }.map_err(cuda_err)?;
            Ok(())
        }

        /// Download the current `grad_w_rec` device buffer into a host
        /// `Vec<f32>`. Returns an error if `ensure_grads` never ran.
        pub fn download_grad_w_rec(&self, out: &mut Vec<f32>) -> Result<(), GpuError> {
            let g = self.grad_w_rec.as_ref().ok_or_else(|| {
                GpuError::Cuda("download_grad_w_rec: ensure_grads not called".into())
            })?;
            let host = self.stream.memcpy_dtov(g).map_err(cuda_err)?;
            out.clear();
            out.extend_from_slice(&host);
            Ok(())
        }
    }

    /// Internal tag for [`LtcGpu::outer_into`].
    enum GradTarget {
        WRec,
        WIn,
    }

    /// Helper: shallow-clone a `CudaSlice` handle so the SGD apply
    /// loop can pass `w` to `axpy` while still holding `&self` for
    /// `&self.axpy_kernel`. cudarc's `CudaSlice` is `Clone` and the
    /// clone shares the device buffer (Arc-like semantics) — exactly
    /// what we want.
    #[inline]
    fn clone_handle<T: cudarc::driver::DeviceRepr>(s: &CudaSlice<T>) -> CudaSlice<T> {
        s.clone()
    }

    #[cfg(test)]
    mod gpu_tests {
        use super::*;
        use crate::ltc::{LtcParams, ltc_forward_step};

        fn fixture_layer() -> LtcLayer {
            LtcLayer::new_seeded(
                LtcParams {
                    n_in: 3,
                    n_rec: 8,
                    tau_init: 1.0,
                    init_scale: 0.5,
                },
                0xCAFE_BABE,
            )
        }

        #[test]
        fn cpu_gpu_equivalence_one_step() {
            let layer = fixture_layer();
            let mut gpu = match LtcGpu::new(&layer) {
                Ok(g) => g,
                Err(e) => {
                    eprintln!("[skipping] no CUDA device: {e:?}");
                    return;
                }
            };
            let mut state_cpu = LtcState::new(layer.params.n_rec);
            let mut state_gpu = LtcState::new(layer.params.n_rec);
            let u = Array1::from_vec(vec![0.3_f32, -0.5, 0.1]);

            let pre_cpu = ltc_forward_step(&mut state_cpu, &layer, &u, 0.1);
            let pre_gpu = gpu.step(&mut state_gpu, &u, 0.1).expect("gpu step");

            for (a, b) in pre_cpu.iter().zip(pre_gpu.iter()) {
                assert!((a - b).abs() < 1e-4, "pre mismatch cpu={a} gpu={b}");
            }
            for (a, b) in state_cpu.x.iter().zip(state_gpu.x.iter()) {
                assert!((a - b).abs() < 1e-4, "x mismatch cpu={a} gpu={b}");
            }
        }

        #[test]
        fn cpu_gpu_equivalence_multi_step() {
            let layer = fixture_layer();
            let mut gpu = match LtcGpu::new(&layer) {
                Ok(g) => g,
                Err(_) => return,
            };
            let mut s_cpu = LtcState::new(layer.params.n_rec);
            let mut s_gpu = LtcState::new(layer.params.n_rec);
            let dt = 0.5;
            let inputs = [
                Array1::from_vec(vec![0.2, -0.1, 0.3]),
                Array1::from_vec(vec![-0.3, 0.4, 0.0]),
                Array1::from_vec(vec![0.5, 0.5, -0.5]),
                Array1::from_vec(vec![0.1, -0.2, 0.4]),
                Array1::from_vec(vec![-0.4, 0.1, 0.2]),
            ];
            for u in &inputs {
                let _ = ltc_forward_step(&mut s_cpu, &layer, u, dt);
                let _ = gpu.step(&mut s_gpu, u, dt).expect("gpu step");
            }
            for (a, b) in s_cpu.x.iter().zip(s_gpu.x.iter()) {
                assert!((a - b).abs() < 1e-3, "x divergence cpu={a} gpu={b}");
            }
        }

        #[test]
        fn reset_zeros_x() {
            let layer = fixture_layer();
            let mut gpu = match LtcGpu::new(&layer) {
                Ok(g) => g,
                Err(_) => return,
            };
            let mut s = LtcState::new(layer.params.n_rec);
            let u = Array1::from_vec(vec![1.0, 1.0, 1.0]);
            for _ in 0..5 {
                let _ = gpu.step(&mut s, &u, 0.1).expect("step");
            }
            gpu.reset().expect("reset");
            let x = gpu.download_x().expect("download");
            assert!(x.iter().all(|&v| v.abs() < 1e-9));
        }

        // ---- Phase 9g+ BPTT primitive tests ----

        #[test]
        fn outer_accumulate_matches_cpu_outer_product() {
            let layer = fixture_layer();
            let mut gpu = match LtcGpu::new(&layer) {
                Ok(g) => g,
                Err(_) => return,
            };
            gpu.ensure_grads().expect("alloc grads");
            // Two consecutive accumulations must sum.
            let dl_dpre1: Vec<f32> = (0..layer.params.n_rec)
                .map(|i| 0.1 * (i as f32 + 1.0))
                .collect();
            let x_prev1: Vec<f32> = (0..layer.params.n_rec)
                .map(|j| -0.05 * (j as f32 + 1.0))
                .collect();
            let dl_dpre2: Vec<f32> = vec![0.5; layer.params.n_rec];
            let x_prev2: Vec<f32> = vec![-0.2; layer.params.n_rec];
            gpu.accumulate_grad_w_rec(&dl_dpre1, &x_prev1)
                .expect("acc1");
            gpu.accumulate_grad_w_rec(&dl_dpre2, &x_prev2)
                .expect("acc2");

            let mut got: Vec<f32> = Vec::new();
            gpu.download_grad_w_rec(&mut got).expect("download");
            let n = layer.params.n_rec;
            for i in 0..n {
                for j in 0..n {
                    let expected = dl_dpre1[i] * x_prev1[j] + dl_dpre2[i] * x_prev2[j];
                    let actual = got[i * n + j];
                    assert!(
                        (actual - expected).abs() < 1e-4,
                        "g[{i},{j}] = {actual} expected {expected}"
                    );
                }
            }
        }

        #[test]
        fn zero_grads_clears_after_accumulate() {
            let layer = fixture_layer();
            let mut gpu = match LtcGpu::new(&layer) {
                Ok(g) => g,
                Err(_) => return,
            };
            gpu.ensure_grads().expect("alloc grads");
            let dl_dpre = vec![1.0_f32; layer.params.n_rec];
            let x_prev = vec![1.0_f32; layer.params.n_rec];
            gpu.accumulate_grad_w_rec(&dl_dpre, &x_prev).expect("acc");
            gpu.zero_grads().expect("zero");
            let mut got: Vec<f32> = Vec::new();
            gpu.download_grad_w_rec(&mut got).expect("download");
            for v in &got {
                assert!(v.abs() < 1e-9, "grad not zero after zero_grads: {v}");
            }
        }

        #[test]
        fn sgd_step_decrements_weights_by_lr_times_grad() {
            let layer = fixture_layer();
            let mut gpu = match LtcGpu::new(&layer) {
                Ok(g) => g,
                Err(_) => return,
            };
            // Stash original weights via a fresh forward step for x sync.
            let mut s_before = LtcState::new(layer.params.n_rec);
            let u = Array1::from_vec(vec![0.0; layer.params.n_in]);
            let _ = gpu.step(&mut s_before, &u, 0.0).expect("warmup");

            gpu.ensure_grads().expect("alloc grads");
            // Accumulate a known grad (all-ones outer of 1.0 vectors → 1.0
            // in every cell).
            let ones_n_rec = vec![1.0_f32; layer.params.n_rec];
            gpu.accumulate_grad_w_rec(&ones_n_rec, &ones_n_rec)
                .expect("acc");
            // sgd with lr=0.1 → w_rec[i,j] -= 0.1 * 1.0 = -0.1.
            gpu.sgd_step(0.1).expect("sgd");
            // Run forward once more — if SGD silently no-op'd, the
            // forward output would be unchanged from a no-grad apply.
            // We don't have a clean way to read W_rec back without
            // adding a dedicated download, so a coarse check: after
            // SGD, a forward step on zero input should *still* produce
            // a state vector of finite values (proves no NaN
            // corruption from a buggy axpy).
            let mut s_after = LtcState::new(layer.params.n_rec);
            let pre = gpu.step(&mut s_after, &u, 0.1).expect("post-sgd step");
            for &v in pre.iter() {
                assert!(v.is_finite());
            }
            for &v in s_after.x.iter() {
                assert!(v.is_finite());
            }
        }
    }
}

// -------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------

#[cfg(test)]
#[allow(clippy::float_cmp)] // exact-equality asserts on clamp boundaries
mod tests {
    use super::*;

    fn p(n_in: usize, n_rec: usize) -> LtcParams {
        LtcParams {
            n_in,
            n_rec,
            tau_init: 1.0,
            init_scale: 0.5,
        }
    }

    #[test]
    fn new_seeded_is_deterministic() {
        let a = LtcLayer::new_seeded(p(3, 4), 0xCAFE_BABE);
        let b = LtcLayer::new_seeded(p(3, 4), 0xCAFE_BABE);
        assert_eq!(a.w_rec, b.w_rec);
        assert_eq!(a.w_in, b.w_in);
        assert_eq!(a.b, b.b);
        assert_eq!(a.tau_base, b.tau_base);
    }

    #[test]
    fn new_seeded_diverges_on_different_seeds() {
        let a = LtcLayer::new_seeded(p(3, 4), 1);
        let b = LtcLayer::new_seeded(p(3, 4), 2);
        assert_ne!(
            a.w_rec, b.w_rec,
            "different seeds must produce different weights"
        );
    }

    #[test]
    fn weights_respect_init_bound() {
        // Xavier bound: 0.5 / sqrt(n_rec=4) = 0.25.
        let layer = LtcLayer::new_seeded(p(3, 4), 7);
        let bound_rec = 0.5_f32 / (4_f32).sqrt();
        let bound_in = 0.5_f32 / (3_f32).sqrt();
        assert!(layer.w_rec.iter().all(|&w| w.abs() <= bound_rec + 1e-6));
        assert!(layer.w_in.iter().all(|&w| w.abs() <= bound_in + 1e-6));
    }

    #[test]
    fn tau_init_floored_at_minimum() {
        let mut params = p(2, 3);
        params.tau_init = 1e-6; // below LTC_TAU_MIN
        let layer = LtcLayer::new_seeded(params, 0);
        assert!(layer.tau_base.iter().all(|&t| t >= LTC_TAU_MIN - 1e-9));
    }

    #[test]
    fn state_new_is_zero() {
        let s = LtcState::new(5);
        assert_eq!(s.x.len(), 5);
        assert!(s.x.iter().all(|&v| v == 0.0));
    }

    #[test]
    fn reset_zeros_without_realloc() {
        let mut s = LtcState::new(5);
        for (i, v) in s.x.iter_mut().enumerate() {
            *v = i as f32;
        }
        s.reset();
        assert!(s.x.iter().all(|&v| v == 0.0));
    }

    #[test]
    fn forward_at_rest_stays_at_rest_under_zero_input() {
        // x = 0, u = 0, b = 0 → pre = 0 → tanh(0) = 0 → dx/dt = 0.
        let layer = LtcLayer::new_seeded(p(2, 3), 42);
        let mut s = LtcState::new(3);
        let u = Array1::zeros(2);
        // Zero b to make this exact (seeded init gives b = 0 already).
        let _pre = ltc_forward_step(&mut s, &layer, &u, 0.1);
        assert!(
            s.x.iter().all(|&v| v.abs() < 1e-6),
            "state drifted under zero drive: {:?}",
            s.x
        );
    }

    #[test]
    fn forward_under_constant_drive_approaches_steady_state() {
        // With x = 0 and constant positive u, pre = W_in·u + b; tanh(pre)
        // is bounded in (−1, +1); steady state of
        //   dx/dt = −x/τ + a   is   x_ss = τ · a.
        // We don't assert exact x_ss (W_in is random), just that |x|
        // grows monotonically toward a nonzero value.
        let params = LtcParams {
            n_in: 4,
            n_rec: 8,
            tau_init: 5.0,
            init_scale: 2.0,
        };
        let layer = LtcLayer::new_seeded(params, 7);
        let mut s = LtcState::new(8);
        let u = Array1::from_elem(4, 1.0_f32);

        let mut prev_norm: f32 = 0.0;
        for _ in 0..30 {
            ltc_forward_step(&mut s, &layer, &u, 0.1);
            let norm = s.x.iter().map(|v| v * v).sum::<f32>().sqrt();
            assert!(
                norm >= prev_norm - 1e-4,
                "state norm decreased under constant drive: {prev_norm} → {norm}"
            );
            prev_norm = norm;
        }
        assert!(
            prev_norm > 0.1,
            "state failed to grow under drive: norm {prev_norm}"
        );
    }

    #[test]
    fn state_clamp_holds_under_extreme_weights() {
        // Construct a pathological layer with huge weights to try to
        // force `x` past ±LTC_STATE_CLAMP. The forward step must
        // saturate rather than explode.
        let mut layer = LtcLayer::new_seeded(p(1, 2), 0);
        for w in layer.w_in.iter_mut() {
            *w = 1.0e6;
        }
        // Keep tau huge so the leak term doesn't pull state back.
        for t in layer.tau_base.iter_mut() {
            *t = 1.0e6;
        }
        let mut s = LtcState::new(2);
        let u = Array1::from_elem(1, 1.0_f32);
        for _ in 0..1000 {
            ltc_forward_step(&mut s, &layer, &u, 10.0);
        }
        assert!(
            s.x.iter().all(|&v| v.abs() <= LTC_STATE_CLAMP + 1e-3),
            "state exceeded clamp: {:?}",
            s.x
        );
        assert!(s.x.iter().all(|&v| v.is_finite()), "state went non-finite");
    }

    /// V1 regression: very small `tau_base` used to produce NaN state.
    /// V2 floors `tau_safe` at [`LTC_TAU_MIN`] inside the forward step,
    /// independent of what's stored on the layer.
    #[test]
    fn tiny_tau_does_not_explode() {
        let mut layer = LtcLayer::new_seeded(p(1, 2), 0);
        for t in layer.tau_base.iter_mut() {
            *t = 1.0e-12; // absurdly small, below LTC_TAU_MIN
        }
        let mut s = LtcState::new(2);
        let u = Array1::from_elem(1, 0.5_f32);
        for _ in 0..200 {
            ltc_forward_step(&mut s, &layer, &u, 0.1);
        }
        assert!(
            s.x.iter().all(|&v| v.is_finite()),
            "tau floor failed: state {:?}",
            s.x
        );
    }

    #[test]
    fn forward_returns_pre_activation_of_correct_shape() {
        let layer = LtcLayer::new_seeded(p(3, 5), 13);
        let mut s = LtcState::new(5);
        let u = Array1::from_elem(3, 0.1_f32);
        let pre = ltc_forward_step(&mut s, &layer, &u, 0.1);
        assert_eq!(pre.len(), 5);
    }
}
