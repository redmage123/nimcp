//! CUDA implementation (only compiled with `--features cuda`).
//!
//! Uses [`cudarc`] with dynamic-loading to discover `libcudart.so` and
//! `libnvrtc.so` at runtime — the same binary works on dev (CUDA 12.0)
//! and pod (CUDA 12.8).

use cudarc::driver::{CudaContext, LaunchConfig, PushKernelArg};

use crate::{DeviceInfo, GpuError};

fn cuda_err<E: std::fmt::Debug>(e: E) -> GpuError {
    GpuError::Cuda(format!("{e:?}"))
}

/// Probe the default CUDA device (ordinal 0).
///
/// Convenience wrapper around [`probe_device_at`] for single-device callers.
pub fn probe_device() -> Result<DeviceInfo, GpuError> {
    probe_device_at(0)
}

/// Probe a specific CUDA device by ordinal.
///
/// Uses the cudarc driver API (not runtime API) so we don't need a
/// CUDA runtime library present — only `libcuda.so` from the driver.
///
/// # Errors
///
/// - [`GpuError::NoDevice`] if no CUDA-capable device is present at
///   this ordinal. Enumeration code (see
///   [`crate::enumerate_devices`]) relies on this to detect the end
///   of the device list.
/// - [`GpuError::Cuda`] for any runtime failure.
pub fn probe_device_at(ordinal: u32) -> Result<DeviceInfo, GpuError> {
    let ctx = CudaContext::new(ordinal as usize).map_err(|e| {
        // cudarc < 1.0 doesn't normalise CUDA_ERROR_INVALID_DEVICE into
        // its own variant, so we inspect the Debug form. If the driver
        // says "invalid device", surface NoDevice so enumeration can
        // stop cleanly. Other errors stay as Cuda(_).
        let msg = format!("{e:?}");
        if msg.contains("InvalidDevice") || msg.contains("INVALID_DEVICE") {
            GpuError::NoDevice
        } else {
            GpuError::Cuda(msg)
        }
    })?;

    let name = ctx.name().map_err(cuda_err)?;
    let (major_i, minor_i) = ctx.compute_capability().map_err(cuda_err)?;
    let (major, minor) = (major_i as u32, minor_i as u32);

    // Total memory via the driver-sys entry point. cudarc doesn't expose
    // a safe wrapper in 0.17, so we call cuDeviceTotalMem_v2 directly.
    // SAFETY: cu_device() returns a valid CUdevice handle tied to the
    // Arc<CudaContext> we just created. Writes `bytes` by-reference.
    let total_memory = unsafe {
        let mut bytes: usize = 0;
        let res = cudarc::driver::sys::cuDeviceTotalMem_v2(&mut bytes, ctx.cu_device());
        if res != cudarc::driver::sys::CUresult::CUDA_SUCCESS {
            return Err(GpuError::Cuda(format!("cuDeviceTotalMem: {res:?}")));
        }
        bytes as u64
    };

    tracing::info!(
        device = %name,
        ordinal = ordinal,
        compute_cap = format!("sm_{}{}", major, minor),
        memory_gb = total_memory / (1024 * 1024 * 1024),
        "cuda device probed"
    );

    Ok(DeviceInfo {
        ordinal,
        name,
        compute_cap: (major, minor),
        total_memory,
    })
}

/// Hello-world GPU kernel: elementwise vector add `c[i] = a[i] + b[i]`.
///
/// Compiles the kernel at runtime via NVRTC, uploads + launches, and
/// downloads results. Allocates three device buffers sized to `a.len()`.
///
/// Used as a smoke test that the full cudarc → libcudart → CUDA runtime
/// chain works on the host.
pub fn vector_add(a: &[f32], b: &[f32]) -> Result<Vec<f32>, GpuError> {
    if a.len() != b.len() {
        return Err(GpuError::Cuda(format!(
            "shape mismatch: a.len()={} b.len()={}",
            a.len(),
            b.len()
        )));
    }
    let n = a.len();

    let ctx = CudaContext::new(0).map_err(cuda_err)?;
    let stream = ctx.default_stream();

    // Compile the kernel via NVRTC. PTX is what cudarc launches.
    // We don't pass an arch flag: NVRTC picks the device's arch.
    let ptx = cudarc::nvrtc::compile_ptx(KERNEL_SRC).map_err(cuda_err)?;
    let module = ctx.load_module(ptx).map_err(cuda_err)?;
    let kernel = module.load_function("vector_add").map_err(cuda_err)?;

    // Host → device copies (stream-ordered).
    let d_a = stream.memcpy_stod(a).map_err(cuda_err)?;
    let d_b = stream.memcpy_stod(b).map_err(cuda_err)?;
    let mut d_c = stream.alloc_zeros::<f32>(n).map_err(cuda_err)?;

    // Launch with 256-thread blocks, enough blocks to cover n.
    let cfg = LaunchConfig::for_num_elems(n as u32);
    let n_u32 = n as u32;
    // SAFETY of the launch: we provide matching arg types that line up
    // with the kernel signature `(const float*, const float*, float*, int)`.
    // cudarc validates arity + sizes at launch time.
    let mut builder = stream.launch_builder(&kernel);
    builder.arg(&d_a);
    builder.arg(&d_b);
    builder.arg(&mut d_c);
    builder.arg(&n_u32);
    unsafe { builder.launch(cfg) }.map_err(cuda_err)?;

    // Device → host + synchronize.
    let out = stream.memcpy_dtov(&d_c).map_err(cuda_err)?;
    Ok(out)
}

/// Vector-add kernel source compiled by NVRTC at runtime.
const KERNEL_SRC: &str = r#"
extern "C" __global__ void vector_add(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        c[i] = a[i] + b[i];
    }
}
"#;

#[cfg(test)]
mod tests {
    use super::*;

    /// Helper: does this host have a functional CUDA device?
    /// Skips tests silently when none is available.
    fn cuda_available() -> bool {
        probe_device().is_ok()
    }

    #[test]
    fn probe_reports_compute_capability() {
        if !cuda_available() {
            eprintln!("skipping: no CUDA device on this host");
            return;
        }
        let info = probe_device().unwrap();
        assert!(
            info.compute_cap.0 >= 5,
            "compute major {} too low",
            info.compute_cap.0
        );
        assert!(info.total_memory > 0);
        assert!(!info.name.is_empty());
    }

    #[test]
    fn vector_add_roundtrips() {
        if !cuda_available() {
            eprintln!("skipping: no CUDA device on this host");
            return;
        }
        let a: Vec<f32> = (0..1024).map(|i| i as f32).collect();
        let b: Vec<f32> = (0..1024).map(|i| (i as f32) * 0.5).collect();
        let c = vector_add(&a, &b).unwrap();
        for i in 0..a.len() {
            let expected = a[i] + b[i];
            assert!(
                (c[i] - expected).abs() < 1e-6,
                "i={i}: got {}, expected {}",
                c[i],
                expected
            );
        }
    }

    #[test]
    fn vector_add_rejects_mismatched_lengths() {
        let a = [1.0_f32, 2.0];
        let b = [1.0_f32, 2.0, 3.0];
        assert!(vector_add(&a, &b).is_err());
    }
}
