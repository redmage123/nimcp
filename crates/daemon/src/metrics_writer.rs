//! Periodic writer for `website/metrics.json` + `training.log` in the
//! schema V1's cron monitor (`scripts/monitor_training_cron.sh`) expects.
//!
//! The writer owns a snapshot of the daemon's lifetime counters and,
//! once per tick, calls [`MetricsSnapshot::write_atomic`] to produce the
//! JSON file via a `<path>.tmp` + rename pattern — the monitor never
//! sees a half-written buffer.

use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use nimcp_brain::Brain;
use serde::{Deserialize, Serialize};
use tokio::sync::Mutex;

/// Cumulative counters tracked outside the brain. Request handlers
/// atomically mutate these via the shared [`MetricsState`].
#[derive(Debug, Default, Clone, Serialize, Deserialize)]
pub struct CountersSnapshot {
    /// Number of successful `Learn` calls since boot.
    pub learn_calls: u64,
    /// Number of successful `Predict` calls since boot.
    pub infer_calls: u64,
    /// Number of errors returned to clients since boot.
    pub errors: u64,
    /// Number of SNN steps completed since boot.
    pub snn_steps: u64,
    /// Cumulative spike count across all SNN steps.
    pub snn_spikes: u64,
    /// Unix timestamp (seconds) of the last learn call. Used for
    /// `training_active` freshness.
    pub last_learn_unix: f64,
}

/// Shared, mutable state for the metrics writer. Cheap to clone;
/// internals are behind a tokio `Mutex` so the writer and handlers can
/// coexist without blocking each other for long.
#[derive(Debug, Clone)]
pub struct MetricsState {
    inner: Arc<Mutex<CountersSnapshot>>,
    started_at: Instant,
}

impl MetricsState {
    /// Construct an empty counter set and anchor "boot time" to now.
    #[must_use]
    pub fn new() -> Self {
        Self {
            inner: Arc::new(Mutex::new(CountersSnapshot::default())),
            started_at: Instant::now(),
        }
    }

    /// Uptime, seconds.
    pub fn uptime_secs(&self) -> f64 {
        self.started_at.elapsed().as_secs_f64()
    }

    /// Bump `learn_calls` and update `last_learn_unix`.
    pub async fn record_learn(&self) {
        let mut g = self.inner.lock().await;
        g.learn_calls = g.learn_calls.saturating_add(1);
        g.last_learn_unix = unix_now();
    }

    /// Bump `infer_calls`.
    pub async fn record_infer(&self) {
        let mut g = self.inner.lock().await;
        g.infer_calls = g.infer_calls.saturating_add(1);
    }

    /// Bump `errors`.
    pub async fn record_error(&self) {
        let mut g = self.inner.lock().await;
        g.errors = g.errors.saturating_add(1);
    }

    /// Record one SNN step with its spike count.
    pub async fn record_snn_step(&self, spikes: u32) {
        let mut g = self.inner.lock().await;
        g.snn_steps = g.snn_steps.saturating_add(1);
        g.snn_spikes = g.snn_spikes.saturating_add(u64::from(spikes));
    }

    /// Read a cheap copy of the current counters.
    pub async fn snapshot(&self) -> CountersSnapshot {
        self.inner.lock().await.clone()
    }
}

impl Default for MetricsState {
    fn default() -> Self {
        Self::new()
    }
}

/// Flat view matching the V1 `metrics.json` schema that
/// `website/metrics_runpod.py` writes.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MetricsSnapshot {
    /// Unix timestamp (seconds since epoch, float).
    pub timestamp: f64,
    /// Always true — the daemon only gets here if it's alive.
    pub ok: bool,
    /// Daemon uptime (seconds).
    pub uptime: f64,
    /// Cumulative learn-call count.
    pub learn_calls: u64,
    /// Cumulative predict-call count.
    pub infer_calls: u64,
    /// Cumulative client-facing error count.
    pub errors: u64,
    /// Sum of configured layer widths — V1's "neuron_count" proxy.
    pub neuron_count: u64,
    /// Adaptive-net EMA loss. `None` before the first learn.
    pub ann_loss: Option<f32>,
    /// SNN scalar loss — reward-driven network, no scalar loss; always `None`.
    pub snn_loss: Option<f32>,
    /// LNN EMA loss. `None` before the first LNN train step.
    pub lnn_loss: Option<f32>,
    /// True iff `learn_calls` grew in the last 5 minutes.
    pub training_active: bool,
    /// Cumulative adaptive optimizer steps (same as `learn_calls`).
    pub ann_steps: u64,
    /// Cumulative SNN spikes emitted.
    pub snn_spikes: u64,
    /// Live SNN firing rate (Hz). Approximated from the last step; 0.0
    /// before any step has run.
    pub snn_rate_hz: f32,
    /// Live SNN sparsity (fraction of neurons *not* spiking this step).
    pub snn_sparsity: f32,
}

fn unix_now() -> f64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs_f64())
        .unwrap_or(0.0)
}

/// How long after the last learn call we still consider training active.
pub const TRAINING_ACTIVE_WINDOW_SECS: f64 = 300.0;

impl MetricsSnapshot {
    /// Capture one snapshot combining [`MetricsState`] + a read over the
    /// brain's stats. Locking is brief — one tokio mutex acquisition for
    /// the counters, one `brain.stats()` call which is itself cheap.
    pub async fn capture(brain: &Brain, state: &MetricsState) -> Self {
        let counters = state.snapshot().await;
        let stats = brain.stats();
        let now = unix_now();

        let neuron_count: u64 = stats
            .adaptive
            .as_ref()
            .map(|a| a.layer_widths.iter().copied().sum::<usize>() as u64)
            .unwrap_or(0)
            + stats
                .snn
                .as_ref()
                .map(|s| {
                    s.populations
                        .iter()
                        .map(|p| p.n_neurons as u64)
                        .sum::<u64>()
                })
                .unwrap_or(0)
            + stats
                .lnn
                .as_ref()
                .map(|l| l.layers.iter().map(|lay| lay.n_rec as u64).sum::<u64>())
                .unwrap_or(0);

        let ann_loss = stats.loss.adaptive.and_then(|t| t.ema);
        let lnn_loss = stats.loss.lnn.and_then(|t| t.ema);

        // SNN live stats: mean of per-pop rate-EMAs for firing rate,
        // sparsity is (1 - mean_spike_fraction_this_step).
        let (snn_rate_hz, snn_sparsity) = if let Some(snn) = stats.snn.as_ref() {
            let n_pops = snn.populations.len().max(1);
            let mean_rate_ema: f32 =
                snn.populations.iter().map(|p| p.rate_ema).sum::<f32>() / n_pops as f32;
            let mean_spike_frac: f32 = snn
                .populations
                .iter()
                .map(|p| {
                    let n = p.n_neurons.max(1) as f32;
                    p.spikes_this_step as f32 / n
                })
                .sum::<f32>()
                / n_pops as f32;
            // rate_ema is fraction-spiking-per-step; 1-sparsity is the
            // active fraction. Sparsity = 1 - active_fraction.
            (
                mean_rate_ema * 1000.0,
                (1.0 - mean_spike_frac).clamp(0.0, 1.0),
            )
        } else {
            (0.0, 0.0)
        };

        let training_active = counters.last_learn_unix > 0.0
            && (now - counters.last_learn_unix) < TRAINING_ACTIVE_WINDOW_SECS;

        Self {
            timestamp: now,
            ok: true,
            uptime: state.uptime_secs(),
            learn_calls: counters.learn_calls,
            infer_calls: counters.infer_calls,
            errors: counters.errors,
            neuron_count,
            ann_loss,
            snn_loss: None,
            lnn_loss,
            training_active,
            ann_steps: counters.learn_calls,
            snn_spikes: counters.snn_spikes,
            snn_rate_hz,
            snn_sparsity,
        }
    }

    /// Atomic write: serialize to `<path>.tmp` then rename to `<path>`.
    /// Returns any filesystem error so the caller can log it; the
    /// daemon does not abort on metrics-write failure.
    pub fn write_atomic(&self, path: &Path) -> std::io::Result<()> {
        if let Some(parent) = path.parent()
            && !parent.as_os_str().is_empty()
        {
            std::fs::create_dir_all(parent)?;
        }
        let tmp = with_tmp_suffix(path);
        let bytes = serde_json::to_vec(self)
            .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
        std::fs::write(&tmp, bytes)?;
        std::fs::rename(&tmp, path)?;
        Ok(())
    }
}

fn with_tmp_suffix(path: &Path) -> PathBuf {
    let mut s = path.as_os_str().to_owned();
    s.push(".tmp");
    PathBuf::from(s)
}

/// Spawn a background task that, every `interval`, captures a snapshot
/// and writes it to `metrics_path`. Returns the `JoinHandle`; the caller
/// aborts the task at shutdown.
pub fn spawn_writer_task(
    brain: Arc<Mutex<Brain>>,
    state: MetricsState,
    metrics_path: PathBuf,
    interval: Duration,
) -> tokio::task::JoinHandle<()> {
    tokio::spawn(async move {
        let mut ticker = tokio::time::interval(interval);
        // First tick fires immediately; skip it so we don't race the
        // brain-boot log line for the same timestamp.
        ticker.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
        ticker.tick().await;

        loop {
            ticker.tick().await;
            let snap = {
                let g = brain.lock().await;
                MetricsSnapshot::capture(&g, &state).await
            };
            if let Err(e) = snap.write_atomic(&metrics_path) {
                tracing::warn!(path = ?metrics_path, error = %e, "metrics write failed");
            }
        }
    })
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;

    #[test]
    fn with_tmp_suffix_is_sibling_file() {
        let p = PathBuf::from("/tmp/x/metrics.json");
        let tmp = with_tmp_suffix(&p);
        assert_eq!(tmp, PathBuf::from("/tmp/x/metrics.json.tmp"));
    }

    #[test]
    fn default_counters_are_zero() {
        let c = CountersSnapshot::default();
        assert_eq!(c.learn_calls, 0);
        assert_eq!(c.errors, 0);
        assert_eq!(c.snn_spikes, 0);
        assert_eq!(c.last_learn_unix, 0.0);
    }

    #[test]
    fn snapshot_serializes_with_expected_keys() {
        let s = MetricsSnapshot {
            timestamp: 1.0,
            ok: true,
            uptime: 2.0,
            learn_calls: 3,
            infer_calls: 4,
            errors: 5,
            neuron_count: 6,
            ann_loss: Some(0.1),
            snn_loss: None,
            lnn_loss: None,
            training_active: false,
            ann_steps: 3,
            snn_spikes: 7,
            snn_rate_hz: 0.0,
            snn_sparsity: 0.0,
        };
        let j = serde_json::to_value(&s).unwrap();
        // Cron monitor + runpod exporter care about these specific keys.
        for key in [
            "timestamp",
            "ok",
            "uptime",
            "learn_calls",
            "infer_calls",
            "errors",
            "neuron_count",
            "ann_loss",
            "snn_loss",
            "lnn_loss",
            "training_active",
            "ann_steps",
            "snn_spikes",
            "snn_rate_hz",
            "snn_sparsity",
        ] {
            assert!(j.get(key).is_some(), "missing key {key}");
        }
    }
}
