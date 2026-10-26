//! Multi-device discovery + context ownership — Phase 10a / 10b.
//!
//! Thin layer over cudarc's per-device `CudaContext::new(ordinal)`.
//! 10a adds enumeration; 10b adds [`MultiGpuContext`], which owns one
//! Arc'd context per device for the lifetime of a run. Phase 10c–g wire
//! those contexts into SNN population dispatch. See `docs/V2_PLAN.md` §10.

use crate::{DeviceInfo, GpuError};

#[cfg(feature = "cuda")]
use std::sync::Arc;

#[cfg(feature = "cuda")]
use cudarc::driver::{CudaContext, CudaStream};

/// Enumerate every CUDA-capable device visible to the current process.
///
/// Asks the driver for the device count, then probes each ordinal via
/// [`crate::probe_device_at`].
///
/// # Returns
///
/// - `Ok(vec![])` on a host with no CUDA GPUs (or no driver — cudarc
///   fails to load `libcuda.so`, which we report as zero devices rather
///   than an error so CPU-only hosts behave like the cpu-only build).
/// - `Ok(vec![d0, d1, …])` with one [`DeviceInfo`] per visible device,
///   ordinals dense from 0.
/// - `Err(GpuError::Cuda(_))` if a device the driver reported fails to
///   probe.
#[cfg(feature = "cuda")]
pub fn enumerate_devices() -> Result<Vec<DeviceInfo>, GpuError> {
    // cudarc's dynamic loader panics (not Err) when libcuda.so is absent;
    // a cuda build on a driverless host must still report zero devices.
    let count = match std::panic::catch_unwind(CudaContext::device_count) {
        Ok(Ok(n)) if n > 0 => n as u32,
        Ok(Ok(_)) => return Ok(Vec::new()),
        Ok(Err(e)) => {
            tracing::debug!(err = ?e, "enumerate_devices: driver unavailable, 0 devices");
            return Ok(Vec::new());
        }
        Err(_) => {
            tracing::debug!("enumerate_devices: CUDA driver library not loadable, 0 devices");
            return Ok(Vec::new());
        }
    };
    (0..count).map(crate::probe_device_at).collect()
}

/// Stub for CPU-only builds: no devices.
#[cfg(not(feature = "cuda"))]
pub fn enumerate_devices() -> Result<Vec<DeviceInfo>, GpuError> {
    Ok(Vec::new())
}

/// One live context per device, held for the lifetime of a run.
///
/// Positions in the context are *indices* (0..n), ordinals are the CUDA
/// device numbers passed to [`MultiGpuContext::new`]. The two coincide
/// for `new(&[0, 1])` but not for `new(&[1])`.
#[cfg(feature = "cuda")]
pub struct MultiGpuContext {
    contexts: Vec<Arc<CudaContext>>,
    infos: Vec<DeviceInfo>,
}

#[cfg(feature = "cuda")]
impl MultiGpuContext {
    /// Open a context on each listed ordinal, in order.
    ///
    /// # Errors
    ///
    /// - [`GpuError::NoDevice`] if `ordinals` is empty or any ordinal
    ///   does not exist.
    /// - [`GpuError::Cuda`] on duplicate ordinals or driver failure.
    pub fn new(ordinals: &[u32]) -> Result<Self, GpuError> {
        if ordinals.is_empty() {
            return Err(GpuError::NoDevice);
        }
        let mut seen = std::collections::HashSet::with_capacity(ordinals.len());
        for &o in ordinals {
            if !seen.insert(o) {
                return Err(GpuError::Cuda(format!("duplicate device ordinal {o}")));
            }
        }

        let mut contexts = Vec::with_capacity(ordinals.len());
        let mut infos = Vec::with_capacity(ordinals.len());
        for &ordinal in ordinals {
            let info = crate::probe_device_at(ordinal)?;
            let ctx = CudaContext::new(ordinal as usize)
                .map_err(|e| GpuError::Cuda(format!("{e:?}")))?;
            contexts.push(ctx);
            infos.push(info);
        }

        tracing::info!(
            devices = ?ordinals,
            "multi-gpu context opened"
        );
        Ok(Self { contexts, infos })
    }

    /// Open a context on every device the driver reports.
    pub fn all() -> Result<Self, GpuError> {
        let devs = enumerate_devices()?;
        let ordinals: Vec<u32> = devs.iter().map(|d| d.ordinal).collect();
        Self::new(&ordinals)
    }

    /// Number of devices in this context.
    pub fn device_count(&self) -> usize {
        self.contexts.len()
    }

    /// Context for the device at `index` (position in the list passed
    /// to [`Self::new`], not the CUDA ordinal).
    pub fn context(&self, index: usize) -> Option<&Arc<CudaContext>> {
        self.contexts.get(index)
    }

    /// Context for the device with CUDA ordinal `ordinal`.
    pub fn context_for_ordinal(&self, ordinal: u32) -> Option<&Arc<CudaContext>> {
        self.infos
            .iter()
            .position(|d| d.ordinal == ordinal)
            .and_then(|i| self.contexts.get(i))
    }

    /// Default stream of the device at `index`.
    pub fn stream(&self, index: usize) -> Option<Arc<CudaStream>> {
        self.contexts.get(index).map(|c| c.default_stream())
    }

    /// Probe results, in the same order as the contexts.
    pub fn infos(&self) -> &[DeviceInfo] {
        &self.infos
    }

    /// Block until every device has drained its work.
    pub fn synchronize_all(&self) -> Result<(), GpuError> {
        for (i, ctx) in self.contexts.iter().enumerate() {
            ctx.synchronize().map_err(|e| {
                GpuError::Cuda(format!("synchronize device index {i}: {e:?}"))
            })?;
        }
        Ok(())
    }
}

#[cfg(feature = "cuda")]
impl std::fmt::Debug for MultiGpuContext {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("MultiGpuContext")
            .field("devices", &self.infos)
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn enumerate_never_errors_on_zero_devices() {
        let devs = enumerate_devices().expect("enumerate must not err on 0 devices");
        if devs.is_empty() {
            eprintln!("skipping: 0 GPUs on this host");
            return;
        }
        for (i, d) in devs.iter().enumerate() {
            assert_eq!(d.ordinal, i as u32, "ordinals must be dense from 0");
            assert!(!d.name.is_empty());
            assert!(d.total_memory > 0);
        }
    }

    #[test]
    fn enumerate_is_deterministic() {
        let a = enumerate_devices().unwrap();
        let b = enumerate_devices().unwrap();
        assert_eq!(a.len(), b.len());
        for (da, db) in a.iter().zip(&b) {
            assert_eq!(da.ordinal, db.ordinal);
            assert_eq!(da.name, db.name);
            assert_eq!(da.compute_cap, db.compute_cap);
            assert_eq!(da.total_memory, db.total_memory);
        }
    }

    #[cfg(feature = "cuda")]
    mod cuda {
        use super::super::*;

        fn gpus() -> Vec<DeviceInfo> {
            enumerate_devices().unwrap_or_default()
        }

        #[test]
        fn empty_ordinals_is_no_device() {
            assert!(matches!(MultiGpuContext::new(&[]), Err(GpuError::NoDevice)));
        }

        #[test]
        fn duplicate_ordinals_rejected() {
            if gpus().is_empty() {
                eprintln!("skipping: 0 GPUs");
                return;
            }
            assert!(matches!(MultiGpuContext::new(&[0, 0]), Err(GpuError::Cuda(_))));
        }

        #[test]
        fn single_device_degenerate_path() {
            if gpus().is_empty() {
                eprintln!("skipping: 0 GPUs");
                return;
            }
            let mg = MultiGpuContext::new(&[0]).unwrap();
            assert_eq!(mg.device_count(), 1);
            assert_eq!(mg.infos()[0].ordinal, 0);
            assert!(mg.context(0).is_some());
            assert!(mg.context(1).is_none());
            assert!(mg.context_for_ordinal(0).is_some());
            assert!(mg.stream(0).is_some());
            mg.synchronize_all().unwrap();
        }

        #[test]
        fn all_opens_every_device() {
            let n = gpus().len();
            if n == 0 {
                eprintln!("skipping: 0 GPUs");
                return;
            }
            let mg = MultiGpuContext::all().unwrap();
            assert_eq!(mg.device_count(), n);
            for i in 0..n {
                assert_eq!(mg.context(i).unwrap().ordinal(), i);
            }
            mg.synchronize_all().unwrap();
        }

        #[test]
        fn two_devices_index_vs_ordinal() {
            if gpus().len() < 2 {
                eprintln!("skipping: <2 GPUs (multi-device path only runs on vast)");
                return;
            }
            let mg = MultiGpuContext::new(&[1, 0]).unwrap();
            assert_eq!(mg.device_count(), 2);
            assert_eq!(mg.context(0).unwrap().ordinal(), 1);
            assert_eq!(mg.context(1).unwrap().ordinal(), 0);
            assert_eq!(mg.context_for_ordinal(1).unwrap().ordinal(), 1);
            mg.synchronize_all().unwrap();
        }
    }
}
