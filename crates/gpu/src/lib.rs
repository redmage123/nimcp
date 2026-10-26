//! NIMCP V2 — GPU compute backend.
//!
//! # Dual-target design
//!
//! V2 must run on two NVIDIA GPUs with different CUDA toolkit versions:
//!
//! | Host              | GPU                  | Compute cap | CUDA toolkit |
//! |-------------------|----------------------|-------------|--------------|
//! | Dev (Hetzner)     | RTX 4000 SFF Ada     | sm_89       | 12.0         |
//! | Pod (RunPod)      | RTX 5090 Blackwell   | sm_120      | 12.8+        |
//!
//! [`cudarc`] with the `dynamic-loading` feature resolves `libcudart.so`
//! at process start, so the same Rust binary runs against any CUDA 12.x.
//!
//! # Phase 2 scope
//!
//! - Device probe via cudarc driver API: compute capability, name, memory
//! - Hello-world kernel: vector add via runtime-compiled PTX (NVRTC)
//! - Phase 2b: MLP forward + persistent [`mlp_forward::GpuWeightCache`]
//!   (matmul + bias + ReLU/Tanh activation; weights stay device-resident
//!   across forward calls)
//! - Phase 2c: MLP backward + in-place SGD (see [`mlp_backward`])
//! - Unit tests that skip gracefully when no GPU is present

// The CUDA kernel-launch path requires one `unsafe` block (cudarc's
// `.launch(cfg)` is unsafe because the caller asserts that the kernel
// args match the kernel signature). Scoped to cuda_impl.rs only.
#![deny(unsafe_code)]
#![cfg_attr(feature = "cuda", allow(unsafe_code))]

use thiserror::Error;

#[cfg(feature = "cuda")]
mod cuda_impl;

#[cfg(feature = "cuda")]
pub mod mlp_forward;

#[cfg(feature = "cuda")]
pub mod mlp_backward;

pub mod multi_device;

#[cfg(feature = "cuda")]
pub use cuda_impl::{probe_device, probe_device_at, vector_add};

#[cfg(feature = "cuda")]
pub use mlp_forward::{Activation, GpuLayer, GpuWeightCache, mlp_forward as gpu_mlp_forward};

pub use multi_device::enumerate_devices;

#[cfg(feature = "cuda")]
pub use multi_device::MultiGpuContext;

/// GPU backend errors.
#[derive(Debug, Error)]
pub enum GpuError {
    /// CUDA is compiled out. Build with `--features cuda`.
    #[error("CUDA support not compiled in (rebuild with --features cuda)")]
    NotAvailable,

    /// No CUDA-capable device found on this host.
    #[error("no CUDA device found")]
    NoDevice,

    /// Device capability is below the minimum this binary supports.
    #[error("device compute capability {found:?} below minimum {required:?}")]
    CapabilityTooLow {
        /// Capability detected on this device.
        found: (u32, u32),
        /// Minimum capability the binary was compiled for.
        required: (u32, u32),
    },

    /// CUDA runtime API returned an error.
    #[error("cuda error: {0}")]
    Cuda(String),
}

/// Device probe result — enough to choose kernel variants at runtime.
#[derive(Debug, Clone)]
pub struct DeviceInfo {
    /// Device index (0-based).
    pub ordinal: u32,
    /// Human-readable name (e.g. "NVIDIA RTX 4000 SFF Ada Generation").
    pub name: String,
    /// Compute capability as (major, minor). sm_89 → (8, 9); sm_120 → (12, 0).
    pub compute_cap: (u32, u32),
    /// Total global memory in bytes.
    pub total_memory: u64,
}

/// Stub for CPU-only builds. Returns `GpuError::NotAvailable`.
#[cfg(not(feature = "cuda"))]
pub fn probe_device() -> Result<DeviceInfo, GpuError> {
    Err(GpuError::NotAvailable)
}

#[cfg(test)]
#[cfg(not(feature = "cuda"))]
mod stub_tests {
    use super::*;

    #[test]
    fn probe_returns_not_available_without_cuda() {
        assert!(matches!(probe_device(), Err(GpuError::NotAvailable)));
    }

    #[test]
    fn device_info_is_debug() {
        let d = DeviceInfo {
            ordinal: 0,
            name: "RTX 4000 Ada".into(),
            compute_cap: (8, 9),
            total_memory: 20 * 1024 * 1024 * 1024,
        };
        let s = format!("{d:?}");
        assert!(s.contains("4000"));
    }
}
