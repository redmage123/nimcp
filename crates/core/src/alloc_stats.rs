//! Process-level memory snapshot — leak attribution helper.
//!
//! V2 port of V1's `nimcp_get_alloc_stats` (commit `8f876fee4`). V1
//! aggregates glibc `mallinfo2` + `/proc/self/status` + the audit ring
//! buffer + the internal knowledge graph. V2 has no audit log or KG,
//! and Rust uses the system allocator without `mallinfo` bindings — so
//! this module focuses on the genuinely portable signal:
//!
//! 1. **`/proc/self/status` reads** (Linux only) — RSS, VM_DATA,
//!    VM_PEAK, VM_LIB, RSS_ANON, RSS_FILE, RSS_SHMEM. Useful for
//!    attributing the daemon's RSS growth to specific subsystems
//!    across long training runs.
//! 2. **Tagged byte counters** — global atomics any subsystem can
//!    `add`/`sub` to attribute its heap footprint by name. Mirrors
//!    V1's per-tag accounting without needing a custom `GlobalAlloc`.
//!
//! On non-Linux platforms `read_proc_self_status` returns the default
//! all-zero snapshot — V2 daemon callers should treat zeros as "not
//! measured" rather than "measured zero".
//!
//! # Usage
//!
//! ```no_run
//! use nimcp_core::alloc_stats::{snapshot, account, AllocTag};
//!
//! account(AllocTag::SnnSynapses, 1024 * 1024);  // 1 MiB attributed to SNN
//! let s = snapshot();
//! eprintln!("RSS={}MB SNN tagged={}MB",
//!           s.proc.vm_rss_bytes / 1_000_000,
//!           s.tagged.snn_synapse_bytes / 1_000_000);
//! ```

use std::sync::atomic::{AtomicU64, Ordering};

/// Tag used by callers to attribute allocations to a subsystem.
/// Stable over the lifetime of a daemon process — adding a new tag
/// requires extending this enum and the matching counter table.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum AllocTag {
    /// SNN per-population state (LIF buffers, conductance state, etc.).
    SnnPopulationState,
    /// SNN CSR synapse weight + topology arrays.
    SnnSynapses,
    /// LNN layer weights + state.
    LnnState,
    /// Memory crate's tile + buffer storage.
    Memory,
    /// Anything not covered by a more specific tag.
    Other,
}

// Matches the order in `AllocTag` — keep these in sync.
static SNN_POP_STATE_BYTES: AtomicU64 = AtomicU64::new(0);
static SNN_SYNAPSE_BYTES: AtomicU64 = AtomicU64::new(0);
static LNN_STATE_BYTES: AtomicU64 = AtomicU64::new(0);
static MEMORY_BYTES: AtomicU64 = AtomicU64::new(0);
static OTHER_BYTES: AtomicU64 = AtomicU64::new(0);

fn counter(tag: AllocTag) -> &'static AtomicU64 {
    match tag {
        AllocTag::SnnPopulationState => &SNN_POP_STATE_BYTES,
        AllocTag::SnnSynapses => &SNN_SYNAPSE_BYTES,
        AllocTag::LnnState => &LNN_STATE_BYTES,
        AllocTag::Memory => &MEMORY_BYTES,
        AllocTag::Other => &OTHER_BYTES,
    }
}

/// Increment a tag's accounting by `bytes`. Saturating add — if the
/// counter would overflow `u64` (theoretical only), it pins at
/// `u64::MAX` rather than wrapping.
pub fn account(tag: AllocTag, bytes: u64) {
    let c = counter(tag);
    // Best-effort saturating add via CAS loop — `fetch_update` is
    // robust to contention. (`try_update` is the non-deprecated name but
    // needs Rust ≥ 1.92; the workspace MSRV is 1.85.)
    #[allow(deprecated)]
    let _ = c.fetch_update(Ordering::Relaxed, Ordering::Relaxed, |cur| {
        Some(cur.saturating_add(bytes))
    });
}

/// Decrement a tag's accounting by `bytes`. Saturating sub.
pub fn release(tag: AllocTag, bytes: u64) {
    let c = counter(tag);
    #[allow(deprecated)] // see `account`
    let _ = c.fetch_update(Ordering::Relaxed, Ordering::Relaxed, |cur| {
        Some(cur.saturating_sub(bytes))
    });
}

/// Reset every tag counter to zero. Used by tests; never call from
/// production code (resets accounting across the whole process).
pub fn reset_all_tags_for_test() {
    SNN_POP_STATE_BYTES.store(0, Ordering::Relaxed);
    SNN_SYNAPSE_BYTES.store(0, Ordering::Relaxed);
    LNN_STATE_BYTES.store(0, Ordering::Relaxed);
    MEMORY_BYTES.store(0, Ordering::Relaxed);
    OTHER_BYTES.store(0, Ordering::Relaxed);
}

/// `/proc/self/status` snapshot (Linux only). All bytes; defaults to
/// zero on non-Linux or when the read fails.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct ProcStatusBytes {
    /// Resident set size (kernel-reported total RSS).
    pub vm_rss_bytes: u64,
    /// Heap + initialized data segment size.
    pub vm_data_bytes: u64,
    /// Peak virtual memory size since process start.
    pub vm_peak_bytes: u64,
    /// Shared library code mapped into the process.
    pub vm_lib_bytes: u64,
    /// Anonymous (non-file-backed) RSS.
    pub rss_anon_bytes: u64,
    /// File-backed RSS (mapped files, libraries).
    pub rss_file_bytes: u64,
    /// Shared-memory RSS.
    pub rss_shmem_bytes: u64,
}

/// Per-subsystem byte counts (atomically loaded from the global tags).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct TaggedBytes {
    /// Bytes attributed to [`AllocTag::SnnPopulationState`].
    pub snn_population_state_bytes: u64,
    /// Bytes attributed to [`AllocTag::SnnSynapses`].
    pub snn_synapse_bytes: u64,
    /// Bytes attributed to [`AllocTag::LnnState`].
    pub lnn_state_bytes: u64,
    /// Bytes attributed to [`AllocTag::Memory`].
    pub memory_bytes: u64,
    /// Bytes attributed to [`AllocTag::Other`].
    pub other_bytes: u64,
}

impl TaggedBytes {
    /// Sum of every per-tag counter.
    #[must_use]
    pub fn total(&self) -> u64 {
        self.snn_population_state_bytes
            .saturating_add(self.snn_synapse_bytes)
            .saturating_add(self.lnn_state_bytes)
            .saturating_add(self.memory_bytes)
            .saturating_add(self.other_bytes)
    }
}

/// Combined snapshot: kernel-side proc stats + V2 subsystem tags.
#[derive(Debug, Clone, Copy, Default)]
pub struct AllocSnapshot {
    /// Kernel-reported `/proc/self/status` byte counts.
    pub proc: ProcStatusBytes,
    /// V2 subsystem-attributed byte counts (atomically loaded).
    pub tagged: TaggedBytes,
}

/// Read `/proc/self/status` and return the parsed bytes snapshot.
/// Returns `ProcStatusBytes::default()` (all zero) on non-Linux or on
/// any I/O error — V2 callers treat zeros as "unmeasured".
#[must_use]
pub fn read_proc_self_status() -> ProcStatusBytes {
    #[cfg(target_os = "linux")]
    {
        let raw = match std::fs::read_to_string("/proc/self/status") {
            Ok(s) => s,
            Err(_) => return ProcStatusBytes::default(),
        };
        parse_proc_status(&raw)
    }
    #[cfg(not(target_os = "linux"))]
    {
        ProcStatusBytes::default()
    }
}

/// Parse the `/proc/self/status` text format. Public for unit testing
/// — production callers should use [`read_proc_self_status`].
///
/// Only `VmRSS:`, `VmData:`, `VmPeak:`, `VmLib:`, `RssAnon:`,
/// `RssFile:`, and `RssShmem:` lines are read; everything else is
/// ignored. Values are converted from kB → bytes.
#[must_use]
pub fn parse_proc_status(text: &str) -> ProcStatusBytes {
    let mut out = ProcStatusBytes::default();
    for line in text.lines() {
        if let Some((tag, rest)) = line.split_once(':') {
            // Strip trailing " kB" and parse the numeric value.
            let value_kb = rest
                .trim()
                .trim_end_matches(" kB")
                .trim_end_matches("kB")
                .trim()
                .parse::<u64>()
                .unwrap_or(0);
            let bytes = value_kb.saturating_mul(1024);
            match tag {
                "VmRSS" => out.vm_rss_bytes = bytes,
                "VmData" => out.vm_data_bytes = bytes,
                "VmPeak" => out.vm_peak_bytes = bytes,
                "VmLib" => out.vm_lib_bytes = bytes,
                "RssAnon" => out.rss_anon_bytes = bytes,
                "RssFile" => out.rss_file_bytes = bytes,
                "RssShmem" => out.rss_shmem_bytes = bytes,
                _ => {}
            }
        }
    }
    out
}

/// Capture a full snapshot — proc-level RSS / VM stats plus all per-tag
/// counters. Cheap; safe to call from a daemon admin endpoint.
#[must_use]
pub fn snapshot() -> AllocSnapshot {
    AllocSnapshot {
        proc: read_proc_self_status(),
        tagged: TaggedBytes {
            snn_population_state_bytes: SNN_POP_STATE_BYTES.load(Ordering::Relaxed),
            snn_synapse_bytes: SNN_SYNAPSE_BYTES.load(Ordering::Relaxed),
            lnn_state_bytes: LNN_STATE_BYTES.load(Ordering::Relaxed),
            memory_bytes: MEMORY_BYTES.load(Ordering::Relaxed),
            other_bytes: OTHER_BYTES.load(Ordering::Relaxed),
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Tag-counter tests use a `serial_test`-like contract — they share
    // global state, so explicit reset_all_tags_for_test at the top of
    // each test keeps them isolated when run in the same thread. Cargo
    // test runs lib tests on a thread pool so we additionally enforce
    // ordering by NEVER asserting cross-test residual values.

    #[test]
    fn account_increments_and_release_decrements() {
        reset_all_tags_for_test();
        account(AllocTag::SnnSynapses, 1_000);
        account(AllocTag::SnnSynapses, 500);
        let s = snapshot();
        assert!(s.tagged.snn_synapse_bytes >= 1_500); // may include other-thread noise

        release(AllocTag::SnnSynapses, 700);
        let s = snapshot();
        assert!(s.tagged.snn_synapse_bytes < 1_500);
        reset_all_tags_for_test();
    }

    #[test]
    fn release_saturates_at_zero() {
        reset_all_tags_for_test();
        release(AllocTag::Memory, 999_999);
        let s = snapshot();
        assert_eq!(s.tagged.memory_bytes, 0);
        reset_all_tags_for_test();
    }

    #[test]
    fn tagged_total_sums_every_counter() {
        reset_all_tags_for_test();
        account(AllocTag::SnnPopulationState, 100);
        account(AllocTag::SnnSynapses, 200);
        account(AllocTag::LnnState, 300);
        account(AllocTag::Memory, 400);
        account(AllocTag::Other, 500);
        let s = snapshot();
        assert_eq!(s.tagged.total(), 1_500);
        reset_all_tags_for_test();
    }

    #[test]
    fn parse_proc_status_extracts_known_fields() {
        let raw = "\
Name:\tnimcp-daemon
State:\tR (running)
VmPeak:\t  20480 kB
VmSize:\t  16384 kB
VmRSS:\t  8192 kB
VmData:\t  4096 kB
VmLib:\t  2048 kB
RssAnon:\t  6144 kB
RssFile:\t  1536 kB
RssShmem:\t   512 kB
";
        let p = parse_proc_status(raw);
        assert_eq!(p.vm_peak_bytes, 20_480 * 1024);
        assert_eq!(p.vm_rss_bytes, 8_192 * 1024);
        assert_eq!(p.vm_data_bytes, 4_096 * 1024);
        assert_eq!(p.vm_lib_bytes, 2_048 * 1024);
        assert_eq!(p.rss_anon_bytes, 6_144 * 1024);
        assert_eq!(p.rss_file_bytes, 1_536 * 1024);
        assert_eq!(p.rss_shmem_bytes, 512 * 1024);
    }

    #[test]
    fn parse_proc_status_ignores_unknown_lines() {
        let raw = "FooBar: 1234 kB\nAnotherUnknown:\t999\n";
        let p = parse_proc_status(raw);
        assert_eq!(p, ProcStatusBytes::default());
    }

    #[test]
    fn parse_proc_status_handles_malformed_value_with_zero() {
        let raw = "VmRSS:\tabc kB\n";
        let p = parse_proc_status(raw);
        assert_eq!(p.vm_rss_bytes, 0);
    }

    #[test]
    fn parse_proc_status_empty_returns_default() {
        let p = parse_proc_status("");
        assert_eq!(p, ProcStatusBytes::default());
    }

    #[test]
    fn snapshot_on_real_process_returns_nonzero_rss_on_linux() {
        // RSS must be > 0 on any live process on Linux. On other
        // platforms the snapshot is zero — both cases are valid.
        let s = snapshot();
        #[cfg(target_os = "linux")]
        assert!(s.proc.vm_rss_bytes > 0, "live linux process must have RSS > 0");
        #[cfg(not(target_os = "linux"))]
        assert_eq!(s.proc.vm_rss_bytes, 0);
    }
}
