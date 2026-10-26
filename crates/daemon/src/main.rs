//! NIMCP V2 daemon — hosts a [`nimcp_brain::Brain`] behind a Unix socket.
//!
//! # Wire protocol
//!
//! One JSON object per line. Client sends [`protocol::Request`], daemon
//! replies with [`protocol::Response`]. See [`protocol`] for the schema.
//!
//! # Concurrency model
//!
//! One tokio multi-thread runtime, one `Mutex<Brain>` shared between the
//! socket-accept loop and the metrics writer. Each accepted connection
//! gets its own task; requests on a given connection are handled
//! strictly in order. The mutex is held only while the brain method is
//! running — brief enough that the 1 s metrics tick never starves.
//!
//! # V1 compatibility
//!
//! Every tick the daemon writes `website/metrics.json` (schema matches
//! `website/metrics_runpod.py`) and emits a one-line human log into
//! `training.log` for each request — the existing cron monitor
//! (`scripts/monitor_training_cron.sh`) sees V2 without modification.

#![forbid(unsafe_code)]

mod metrics_writer;
mod protocol;

use std::path::PathBuf;
use std::sync::Arc;
use std::time::Duration;

use clap::Parser;
use ndarray::Array1;
use nimcp_brain::{Brain, BrainConfig};
use serde_json::json;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{UnixListener, UnixStream};
use tokio::sync::Mutex;

use crate::metrics_writer::{MetricsState, spawn_writer_task};
use crate::protocol::{Request, Response};

/// Default metrics-writer tick, seconds.
const DEFAULT_METRICS_INTERVAL_SEC: u64 = 1;

/// CLI.
#[derive(Debug, Parser)]
#[command(name = "nimcp-v2-daemon", version, about = "NIMCP V2 brain daemon — the Hera model")]
struct Args {
    /// Path to the Unix socket the daemon listens on. Removed before
    /// bind so a stale socket from a prior crash doesn't block startup.
    /// Production path: `/var/run/hera/brain.sock` (with systemd); this
    /// default lives under `/tmp` so unprivileged `cargo run` works too.
    #[arg(long, default_value = "/tmp/nimcp-hera.sock")]
    socket: PathBuf,

    /// Directory the brain uses for state / checkpoint staging.
    #[arg(long, default_value = "./nimcp-state")]
    state_dir: PathBuf,

    /// Optional JSON file holding a [`nimcp_brain::BrainConfig`]. If
    /// absent, `BrainConfig::default()` is used.
    #[arg(long)]
    config_json: Option<PathBuf>,

    /// Where to write `metrics.json`. Default: `./website/metrics.json`.
    #[arg(long, default_value = "./website/metrics.json")]
    metrics_path: PathBuf,

    /// Where the human training log goes. Default: `./training.log`.
    #[arg(long, default_value = "./training.log")]
    training_log: PathBuf,
}

fn load_config(path: Option<&PathBuf>, state_dir: PathBuf) -> eyre_like::Result<BrainConfig> {
    let mut cfg = match path {
        Some(p) => {
            let bytes = std::fs::read(p).map_err(|e| format!("read config {p:?}: {e}"))?;
            serde_json::from_slice::<BrainConfig>(&bytes)
                .map_err(|e| format!("parse config {p:?}: {e}"))?
        }
        None => BrainConfig::default(),
    };
    // CLI overrides whatever was in the JSON so `--state-dir` always wins.
    cfg.state_dir = state_dir;
    Ok(cfg)
}

// Tiny in-crate alternative to the `eyre` dependency so we don't pull
// in another crate for error-type plumbing. We only need a `Box<dyn Error>`.
mod eyre_like {
    pub type Result<T> = std::result::Result<T, Box<dyn std::error::Error + Send + Sync>>;
}

/// Well-known tunable names accepted by [`Request::SnnTune`]. The daemon
/// stores them in a simple in-memory map so [`Request::SnnTuneGet`]
/// returns a coherent view even if the underlying SNN has no hook for
/// the knob yet. Unknown names are rejected.
const TUNABLE_NAMES: &[&str] = &[
    "rstdp_lr",
    "homeo_min_scale",
    "homeo_max_scale",
    "max_scale_dead",
    "dead_threshold",
    "target_rate",
    "noise_rate_hz",
    "rstdp_baseline_alpha",
];

/// Default values returned by `snn_tune_get` on a fresh daemon.
fn default_tunables() -> serde_json::Map<String, serde_json::Value> {
    let mut m = serde_json::Map::new();
    m.insert("rstdp_lr".into(), json!(0.0001_f32));
    m.insert("homeo_min_scale".into(), json!(0.98_f32));
    m.insert("homeo_max_scale".into(), json!(1.02_f32));
    m.insert("max_scale_dead".into(), json!(1.05_f32));
    m.insert("dead_threshold".into(), json!(0.1_f32));
    m.insert("target_rate".into(), json!(0.03_f32));
    m.insert("noise_rate_hz".into(), json!(20.0_f32));
    m.insert("rstdp_baseline_alpha".into(), json!(0.001_f32));
    m
}

/// Shared daemon state — just the tunable-parameter mirror for now.
/// Brain + metrics live in their own Arc<Mutex<_>>.
#[derive(Debug, Clone)]
struct SharedState {
    tunables: Arc<Mutex<serde_json::Map<String, serde_json::Value>>>,
    shutdown: Arc<tokio::sync::Notify>,
}

impl SharedState {
    fn new() -> Self {
        Self {
            tunables: Arc::new(Mutex::new(default_tunables())),
            shutdown: Arc::new(tokio::sync::Notify::new()),
        }
    }
}

// -------------------------------------------------------------------------
// Handler
// -------------------------------------------------------------------------

async fn handle_request(
    req: Request,
    brain: &Arc<Mutex<Brain>>,
    metrics: &MetricsState,
    shared: &SharedState,
) -> Response {
    match req {
        Request::Ping => {
            tracing::info!("ping");
            Response::ok(json!({"pong": true}))
        }

        Request::Learn {
            features,
            target,
            lr,
        } => {
            let mut g = brain.lock().await;
            // Validate against the adaptive config to turn panics into errors.
            let adaptive_cfg = &g.config().adaptive;
            let first = *adaptive_cfg.layers.first().unwrap_or(&0);
            let last = *adaptive_cfg.layers.last().unwrap_or(&0);
            if features.len() != first {
                return Response::err(format!(
                    "learn: features len {} != first layer {}",
                    features.len(),
                    first
                ));
            }
            if target.len() != last {
                return Response::err(format!(
                    "learn: target len {} != last layer {}",
                    target.len(),
                    last
                ));
            }
            let f = Array1::from(features);
            let t = Array1::from(target);
            let loss = g.learn(&f, &t, lr);
            drop(g);
            metrics.record_learn().await;
            tracing::info!(lr, loss, "learn");
            Response::ok(json!({"loss": loss}))
        }

        Request::Predict { features } => {
            let g = brain.lock().await;
            let adaptive_cfg = &g.config().adaptive;
            let first = *adaptive_cfg.layers.first().unwrap_or(&0);
            if features.len() != first {
                return Response::err(format!(
                    "predict: features len {} != first layer {}",
                    features.len(),
                    first
                ));
            }
            let f = Array1::from(features);
            let out = g.predict(&f);
            drop(g);
            metrics.record_infer().await;
            tracing::info!(n_out = out.len(), "predict");
            Response::ok(json!({"output": out.to_vec()}))
        }

        Request::Stats => {
            let g = brain.lock().await;
            match g.stats_json() {
                Ok(s) => {
                    // Reparse so clients get a real JSON object, not a
                    // string containing JSON.
                    match serde_json::from_str::<serde_json::Value>(&s) {
                        Ok(v) => Response::ok(v),
                        Err(e) => Response::err(format!("stats reparse: {e}")),
                    }
                }
                Err(e) => Response::err(format!("stats: {e}")),
            }
        }

        Request::SaveEnsemble { dir } => {
            let g = brain.lock().await;
            match g.save_ensemble(PathBuf::from(&dir)) {
                Ok(()) => {
                    tracing::info!(dir = %dir, "save_ensemble");
                    Response::ok(json!({"saved": dir}))
                }
                Err(e) => Response::err(format!("save_ensemble: {e}")),
            }
        }

        Request::LoadEnsemble { dir } => {
            let mut g = brain.lock().await;
            match g.load_ensemble(PathBuf::from(&dir)) {
                Ok(()) => {
                    tracing::info!(dir = %dir, "load_ensemble");
                    Response::ok(json!({"loaded": dir}))
                }
                Err(e) => Response::err(format!("load_ensemble: {e}")),
            }
        }

        Request::SnnStep {
            drive,
            reward,
            dt_ms,
        } => {
            let mut g = brain.lock().await;
            if g.snn().is_none() {
                return Response::err("snn: brain has no SNN configured");
            }
            let slices: Vec<&[f32]> = drive.iter().map(Vec::as_slice).collect();
            match g.snn_step(&slices, reward, dt_ms) {
                Ok(spikes) => {
                    drop(g);
                    metrics.record_snn_step(spikes).await;
                    tracing::info!(spikes, reward, dt_ms, "snn_step");
                    Response::ok(json!({"spikes": spikes}))
                }
                Err(e) => Response::err(format!("snn_step: {e}")),
            }
        }

        Request::SnnPopStats => {
            let g = brain.lock().await;
            let Some(snn) = g.snn() else {
                return Response::err("snn: brain has no SNN configured");
            };
            let mut pops: Vec<serde_json::Value> = Vec::with_capacity(snn.n_populations());
            for i in 0..snn.n_populations() {
                let spikes = snn.spikes(i);
                let n = spikes.len();
                let spikes_this_step = spikes.iter().filter(|&&s| s != 0).count() as u32;
                let rate_ema = snn.rate_ema(i);
                let pink_alpha = snn.pink_alpha(i);
                pops.push(json!({
                    "name": format!("pop_{i}"),
                    "pop_idx": i,
                    "n_neurons": n,
                    "rate_ema": rate_ema,
                    "firing_rate_ema": rate_ema,
                    "spikes_this_step": spikes_this_step,
                    "pink_alpha": pink_alpha,
                }));
            }
            Response::ok(json!({"pops": pops}))
        }

        Request::SnnTuneGet => {
            let t = shared.tunables.lock().await.clone();
            Response::ok(json!({"params": t}))
        }

        Request::SnnTune { name, value } => {
            if !TUNABLE_NAMES.contains(&name.as_str()) {
                return Response::err(format!("unknown tunable: {name}"));
            }
            let mut t = shared.tunables.lock().await;
            t.insert(name.clone(), json!(value));
            drop(t);
            tracing::info!(name = %name, value, "snn_tune");
            Response::ok(json!({"name": name, "value": value}))
        }

        Request::SnnForceQuench { n } => {
            let g = brain.lock().await;
            if g.snn().is_none() {
                return Response::err("snn: brain has no SNN configured");
            }
            // V2 SNN does not expose `v_mem_mut` publicly — acknowledge
            // the RPC so the watchdog's action loop stays intact.
            tracing::info!(n, "snn_force_quench (no-op: v_mem_mut not exposed)");
            Response::ok(json!({
                "requested": n,
                "zeroed": 0_u32,
                "note": "v2 snn exposes v_mem read-only; force_quench is a no-op",
            }))
        }

        // ---- Language (L8) + cognition (C4/C5) ----
        Request::LanguageLearn { text } => {
            let mut g = brain.lock().await;
            match g.language_learn(&text) {
                Ok(()) => {
                    tracing::debug!(chars = text.len(), "language_learn");
                    let stats = g.language().map(|gl| gl.stats);
                    Response::ok(json!({"learned": true, "stats": stats}))
                }
                Err(e) => Response::err(format!("language_learn: {e}")),
            }
        }

        Request::LanguageComprehend { text } => {
            let mut g = brain.lock().await;
            match g.language_comprehend(&text) {
                Ok((vec, confidence)) => {
                    Response::ok(json!({"vector": vec.to_vec(), "confidence": confidence}))
                }
                Err(e) => Response::err(format!("language_comprehend: {e}")),
            }
        }

        Request::LanguageRespond { text } => {
            let mut g = brain.lock().await;
            match g.language_respond(&text) {
                Ok(r) => {
                    tracing::info!(blocked = r.blocked_for_toxicity, "language_respond");
                    Response::ok(json!({
                        "text": r.text,
                        "confidence": r.confidence,
                        "blocked_for_toxicity": r.blocked_for_toxicity,
                        "toxicity_category": r.toxicity_category,
                    }))
                }
                Err(e) => Response::err(format!("language_respond: {e}")),
            }
        }

        Request::Reason { query } => {
            let mut g = brain.lock().await;
            match g.reason(&query) {
                Ok(r) => Response::ok(r),
                Err(e) => Response::err(format!("reason: {e}")),
            }
        }

        Request::SetReasonInContent { on } => {
            let mut g = brain.lock().await;
            if g.language().is_none() {
                return Response::err("set_reason_in_content: language not configured on this brain");
            }
            g.set_reason_in_content(on);
            tracing::info!(on, "set_reason_in_content");
            Response::ok(json!({"reason_in_content": g.reason_in_content()}))
        }

        Request::ClassifyToxicity { text } => {
            let g = brain.lock().await;
            match g.classify_toxicity(&text) {
                Ok((harm, fairness, block)) => Response::ok(json!({
                    "predicted_harm": harm,
                    "fairness_violation": fairness,
                    "would_block": block,
                })),
                Err(e) => Response::err(format!("classify_toxicity: {e}")),
            }
        }

        Request::WorkingMemoryAdd { data, salience } => {
            let mut g = brain.lock().await;
            match g.working_memory_add(&data, salience) {
                Ok(()) => {
                    let size = g.working_memory().map_or(0, nimcp_cognition::WorkingMemory::size);
                    Response::ok(json!({"size": size, "now_ms": g.cognition_now_ms()}))
                }
                Err(e) => Response::err(format!("working_memory_add: {e}")),
            }
        }

        Request::ImaginationStage { content, vividness } => {
            let mut g = brain.lock().await;
            match g.imagination_stage(&content, vividness) {
                Ok(()) => {
                    let viv = g.imagination().and_then(|im| im.active_vector()).map(|(_, v)| v);
                    Response::ok(json!({"staged": true, "vividness": viv}))
                }
                Err(e) => Response::err(format!("imagination_stage: {e}")),
            }
        }

        Request::CognitionState => {
            let g = brain.lock().await;
            if g.language().is_none() {
                return Response::err("cognition_state: language not configured on this brain");
            }
            let wm: Vec<serde_json::Value> = g
                .working_memory()
                .map(|wm| {
                    wm.items()
                        .iter()
                        .map(|i| {
                            json!({
                                "salience": i.salience,
                                "timestamp_ms": i.timestamp_ms,
                                "dim": i.data.len(),
                            })
                        })
                        .collect()
                })
                .unwrap_or_default();
            let imag = g
                .imagination()
                .and_then(|im| im.active_vector())
                .map(|(v, viv)| json!({"vividness": viv, "dim": v.len()}));
            Response::ok(json!({
                "now_ms": g.cognition_now_ms(),
                "working_memory": wm,
                "imagination": imag,
                "reason_in_content": g.reason_in_content(),
            }))
        }

        Request::Shutdown => {
            tracing::info!("shutdown requested");
            shared.shutdown.notify_waiters();
            Response::ok(json!({"shutting_down": true}))
        }
    }
}

// -------------------------------------------------------------------------
// Connection loop
// -------------------------------------------------------------------------

async fn serve_connection(
    mut stream: UnixStream,
    brain: Arc<Mutex<Brain>>,
    metrics: MetricsState,
    shared: SharedState,
) {
    let (read_half, mut write_half) = stream.split();
    let mut reader = BufReader::new(read_half);
    let mut line = String::new();

    loop {
        line.clear();
        let n = match reader.read_line(&mut line).await {
            Ok(0) => return, // EOF — client closed.
            Ok(n) => n,
            Err(e) => {
                tracing::debug!(error = %e, "connection read error");
                return;
            }
        };
        let _ = n;
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }

        let response = match serde_json::from_str::<Request>(trimmed) {
            Ok(req) => {
                let was_shutdown = matches!(req, Request::Shutdown);
                let resp = handle_request(req, &brain, &metrics, &shared).await;
                if matches!(resp, Response::Err { .. }) {
                    metrics.record_error().await;
                }
                if was_shutdown {
                    // Flush the OK response before the listener loop tears us
                    // down.
                    let line = match serde_json::to_string(&resp) {
                        Ok(s) => s,
                        Err(_) => "{\"status\":\"err\",\"message\":\"encode failed\"}".into(),
                    };
                    let _ = write_half.write_all(line.as_bytes()).await;
                    let _ = write_half.write_all(b"\n").await;
                    let _ = write_half.flush().await;
                    return;
                }
                resp
            }
            Err(e) => {
                metrics.record_error().await;
                Response::err(format!("invalid request json: {e}"))
            }
        };

        let body = match serde_json::to_string(&response) {
            Ok(s) => s,
            Err(e) => format!("{{\"status\":\"err\",\"message\":\"encode: {e}\"}}"),
        };
        if write_half.write_all(body.as_bytes()).await.is_err() {
            return;
        }
        if write_half.write_all(b"\n").await.is_err() {
            return;
        }
        if write_half.flush().await.is_err() {
            return;
        }
    }
}

// -------------------------------------------------------------------------
// main
// -------------------------------------------------------------------------

fn init_tracing(training_log: &PathBuf) -> eyre_like::Result<()> {
    use tracing_subscriber::fmt::writer::MakeWriterExt;
    use tracing_subscriber::{EnvFilter, fmt};

    if let Some(parent) = training_log.parent()
        && !parent.as_os_str().is_empty()
    {
        std::fs::create_dir_all(parent)
            .map_err(|e| format!("create {parent:?}: {e}"))?;
    }
    // Plain append-only line log. We don't daily-rotate here — supervisord
    // and logrotate handle that in production.
    let file = std::fs::OpenOptions::new()
        .append(true)
        .create(true)
        .open(training_log)
        .map_err(|e| format!("open {training_log:?}: {e}"))?;

    let env_filter = EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new("info"));
    let writer = std::io::stderr.and(file);
    let subscriber = fmt()
        .with_env_filter(env_filter)
        .with_target(false)
        .with_ansi(false)
        .with_writer(writer)
        .finish();
    tracing::subscriber::set_global_default(subscriber).map_err(|e| format!("set tracing: {e}"))?;
    Ok(())
}

#[tokio::main(flavor = "multi_thread")]
async fn main() -> eyre_like::Result<()> {
    let args = Args::parse();
    init_tracing(&args.training_log)?;

    tracing::info!(socket = ?args.socket, state_dir = ?args.state_dir, "starting nimcp-v2-daemon");

    // Build brain.
    let cfg = load_config(args.config_json.as_ref(), args.state_dir.clone())?;
    let brain = Brain::new(cfg).map_err(|e| format!("brain: {e}"))?;
    let brain = Arc::new(Mutex::new(brain));

    // Metrics writer.
    let metrics = MetricsState::new();
    let interval_secs = std::env::var("NIMCP_METRICS_INTERVAL_SEC")
        .ok()
        .and_then(|s| s.parse::<u64>().ok())
        .unwrap_or(DEFAULT_METRICS_INTERVAL_SEC);
    let writer_handle = spawn_writer_task(
        brain.clone(),
        metrics.clone(),
        args.metrics_path.clone(),
        Duration::from_secs(interval_secs.max(1)),
    );

    // Unix socket bind.
    if args.socket.exists() {
        let _ = std::fs::remove_file(&args.socket);
    }
    if let Some(parent) = args.socket.parent()
        && !parent.as_os_str().is_empty()
    {
        std::fs::create_dir_all(parent).map_err(|e| format!("create socket parent: {e}"))?;
    }
    let listener = UnixListener::bind(&args.socket).map_err(|e| format!("bind socket: {e}"))?;
    tracing::info!(socket = ?args.socket, "listening");

    let shared = SharedState::new();
    let shutdown = shared.shutdown.clone();

    // Serve loop — drops out when shutdown is notified OR SIGINT/SIGTERM.
    let accept_loop = {
        let brain = brain.clone();
        let metrics = metrics.clone();
        let shared = shared.clone();
        async move {
            loop {
                match listener.accept().await {
                    Ok((stream, _addr)) => {
                        let brain = brain.clone();
                        let metrics = metrics.clone();
                        let shared = shared.clone();
                        tokio::spawn(async move {
                            serve_connection(stream, brain, metrics, shared).await;
                        });
                    }
                    Err(e) => {
                        tracing::warn!(error = %e, "accept failed");
                        // Back off briefly on accept errors.
                        tokio::time::sleep(Duration::from_millis(50)).await;
                    }
                }
            }
        }
    };

    let mut sigterm = tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate())
        .map_err(|e| format!("sigterm: {e}"))?;

    tokio::select! {
        _ = accept_loop => {}
        _ = shutdown.notified() => {
            tracing::info!("shutdown notify fired");
        }
        _ = tokio::signal::ctrl_c() => {
            tracing::info!("sigint received");
        }
        _ = sigterm.recv() => {
            tracing::info!("sigterm received");
        }
    }

    // Teardown. Writer gets aborted — no need to wait for a tick that
    // may be 1s out.
    writer_handle.abort();
    let _ = writer_handle.await;

    // Final metrics flush so the monitor sees the last counters.
    {
        let snap = {
            let g = brain.lock().await;
            crate::metrics_writer::MetricsSnapshot::capture(&g, &metrics).await
        };
        if let Err(e) = snap.write_atomic(&args.metrics_path) {
            tracing::warn!(error = %e, "final metrics write failed");
        }
    }

    if args.socket.exists() {
        let _ = std::fs::remove_file(&args.socket);
    }
    tracing::info!("nimcp-v2-daemon exiting cleanly");
    Ok(())
}
