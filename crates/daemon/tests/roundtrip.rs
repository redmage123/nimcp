//! End-to-end roundtrip: spawn the daemon as a subprocess, connect, and
//! exercise every request type. Everything runs under a 10 s timeout so
//! a hung test fails loudly.

use std::io::{BufRead, BufReader, Write};
use std::os::unix::net::UnixStream;
use std::path::PathBuf;
use std::process::{Child, Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

use serde_json::{Value, json};

/// Wall-clock budget for the whole test. Whatever we're doing, if we
/// need more than this the daemon is hung.
const TIMEOUT: Duration = Duration::from_secs(10);

/// Cargo target dir discovery — tests may run from the package or the
/// workspace root, so pick whichever candidate exists.
fn daemon_binary() -> PathBuf {
    // Prefer `CARGO_BIN_EXE_*` when cargo test sets it for the bin.
    if let Ok(p) = std::env::var("CARGO_BIN_EXE_nimcp-v2-daemon") {
        let pb = PathBuf::from(p);
        if pb.exists() {
            return pb;
        }
    }
    // Fall back to known layouts.
    let candidates = [
        "../../target/debug/nimcp-v2-daemon",
        "../../target/release/nimcp-v2-daemon",
        "target/debug/nimcp-v2-daemon",
        "target/release/nimcp-v2-daemon",
    ];
    for c in candidates {
        let p = PathBuf::from(c);
        if p.exists() {
            return p;
        }
    }
    panic!(
        "could not locate nimcp-v2-daemon binary; checked CARGO_BIN_EXE_* and {:?}",
        candidates
    );
}

struct DaemonProc {
    child: Child,
    socket: PathBuf,
    metrics_path: PathBuf,
    _tmp: tempfile::TempDir,
}

impl DaemonProc {
    fn spawn() -> Self {
        Self::spawn_with_config(None)
    }

    /// Spawn with an optional `BrainConfig` JSON written to the temp dir
    /// and passed via `--config-json`.
    fn spawn_with_config(config_json: Option<&Value>) -> Self {
        let tmp = tempfile::tempdir().expect("tempdir");
        let socket = tmp.path().join("daemon.sock");
        let metrics_path = tmp.path().join("metrics.json");
        let state_dir = tmp.path().join("state");
        let training_log = tmp.path().join("training.log");
        let config_path = tmp.path().join("config.json");
        if let Some(cfg) = config_json {
            std::fs::write(&config_path, serde_json::to_vec(cfg).unwrap()).expect("write config");
        }

        let bin = daemon_binary();
        let mut cmd = Command::new(&bin);
        cmd.arg("--socket")
            .arg(&socket)
            .arg("--state-dir")
            .arg(&state_dir)
            .arg("--metrics-path")
            .arg(&metrics_path)
            .arg("--training-log")
            .arg(&training_log);
        if config_json.is_some() {
            cmd.arg("--config-json").arg(&config_path);
        }
        // Keep stderr so a daemon that dies before binding (config parse
        // error, brain construction failure) explains itself in the
        // assertion below instead of a bare 5s timeout.
        let stderr_path = tmp.path().join("daemon.stderr");
        let stderr_file = std::fs::File::create(&stderr_path).expect("stderr file");
        let mut child = cmd
            .env("NIMCP_METRICS_INTERVAL_SEC", "1")
            .stdin(Stdio::null())
            .stdout(Stdio::null())
            .stderr(Stdio::from(stderr_file))
            .spawn()
            .expect("spawn daemon");

        // Wait for the socket to appear — or the process to die.
        let deadline = Instant::now() + Duration::from_secs(5);
        while !socket.exists() && Instant::now() < deadline {
            if let Ok(Some(status)) = child.try_wait() {
                let err = std::fs::read_to_string(&stderr_path).unwrap_or_default();
                panic!("daemon exited before binding its socket ({status}):\n{err}");
            }
            thread::sleep(Duration::from_millis(25));
        }
        if !socket.exists() {
            let err = std::fs::read_to_string(&stderr_path).unwrap_or_default();
            panic!("daemon failed to bind socket in 5s; stderr:\n{err}");
        }

        Self {
            child,
            socket,
            metrics_path,
            _tmp: tmp,
        }
    }
}

impl Drop for DaemonProc {
    fn drop(&mut self) {
        // Force-kill in case the test bailed before sending Shutdown.
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

struct DaemonClient {
    reader: BufReader<UnixStream>,
    writer: UnixStream,
}

impl DaemonClient {
    fn connect(socket: &PathBuf) -> Self {
        let stream = UnixStream::connect(socket).expect("connect socket");
        stream
            .set_read_timeout(Some(Duration::from_secs(5)))
            .unwrap();
        let writer = stream.try_clone().unwrap();
        let reader = BufReader::new(stream);
        Self { reader, writer }
    }

    fn call(&mut self, req: &Value) -> Value {
        let line = serde_json::to_string(req).unwrap();
        writeln!(self.writer, "{line}").expect("write req");
        self.writer.flush().unwrap();
        let mut buf = String::new();
        self.reader.read_line(&mut buf).expect("read resp");
        serde_json::from_str(&buf).expect("decode resp")
    }
}

/// Run the full happy-path scenario. A panic here fails the test.
fn run_scenario() {
    let proc = DaemonProc::spawn();
    let mut client = DaemonClient::connect(&proc.socket);

    // 1. Ping.
    let pong = client.call(&json!({"cmd": "ping"}));
    assert_eq!(pong["status"], "ok", "ping resp: {pong}");
    assert_eq!(pong["data"]["pong"], true);

    // 2. Stats — default brain has layers = [1], so a Learn needs 1-dim
    // features/target. Verify shape via stats first.
    let stats = client.call(&json!({"cmd": "stats"}));
    assert_eq!(stats["status"], "ok", "stats resp: {stats}");
    let first_layer = stats["data"]["adaptive"]["layer_widths"][0]
        .as_u64()
        .expect("first layer width");
    let last_layer = stats["data"]["adaptive"]["layer_widths"]
        .as_array()
        .unwrap()
        .last()
        .unwrap()
        .as_u64()
        .unwrap();

    // 3. Learn with correctly-shaped vectors.
    let feats: Vec<f32> = vec![0.5_f32; first_layer as usize];
    let targs: Vec<f32> = vec![0.0_f32; last_layer as usize];
    let learn_resp = client.call(&json!({
        "cmd": "learn",
        "features": feats,
        "target": targs,
        "lr": 0.01,
    }));
    assert_eq!(learn_resp["status"], "ok", "learn resp: {learn_resp}");
    assert!(learn_resp["data"]["loss"].is_number(), "learn has no loss");

    // 4. Predict.
    let predict_resp = client.call(&json!({
        "cmd": "predict",
        "features": feats,
    }));
    assert_eq!(predict_resp["status"], "ok", "predict resp: {predict_resp}");
    assert_eq!(
        predict_resp["data"]["output"].as_array().unwrap().len(),
        last_layer as usize
    );

    // 5. Stats post-learn sees learn count = 1.
    let stats2 = client.call(&json!({"cmd": "stats"}));
    let loss_count = stats2["data"]["loss"]["adaptive"]["count"]
        .as_u64()
        .unwrap_or(0);
    assert!(
        loss_count >= 1,
        "expected loss count >= 1, got {loss_count}"
    );

    // 6-8. Every SNN request is a structured error on a brain without an
    //      SNN — including tune_get / tune / force_quench, which used to
    //      answer from a daemon-side mirror that never reached a network.
    for req in [
        json!({"cmd": "snn_pop_stats"}),
        json!({"cmd": "snn_tune_get"}),
        json!({"cmd": "snn_tune", "name": "rstdp_lr", "value": 0.002}),
        json!({"cmd": "snn_tune", "name": "nonsense", "value": 1.0}),
        json!({"cmd": "snn_force_quench", "n": 5}),
    ] {
        let r = client.call(&req);
        assert_eq!(r["status"], "err", "{req} → {r}");
        assert!(
            r["message"]
                .as_str()
                .unwrap()
                .to_lowercase()
                .contains("snn"),
            "{r}"
        );
    }

    // 8b. Language RPCs on a language-less brain are structured errors,
    //     not transport failures.
    for req in [
        json!({"cmd": "language_learn", "text": "the dog runs"}),
        json!({"cmd": "language_respond", "text": "dog"}),
        json!({"cmd": "reason", "query": "why"}),
        json!({"cmd": "set_reason_in_content", "on": true}),
        json!({"cmd": "working_memory_add", "data": [1.0], "salience": 0.5}),
        json!({"cmd": "cognition_state"}),
    ] {
        let r = client.call(&req);
        assert_eq!(r["status"], "err", "{req} → {r}");
        assert!(
            r["message"].as_str().unwrap().contains("not configured"),
            "{r}"
        );
    }

    // 9. Wait for the metrics writer to produce at least one file.
    //    The writer skips the first tick — wait up to 5s.
    let deadline = Instant::now() + Duration::from_secs(5);
    while !proc.metrics_path.exists() && Instant::now() < deadline {
        thread::sleep(Duration::from_millis(100));
    }
    assert!(
        proc.metrics_path.exists(),
        "metrics file {:?} not written within 5s",
        proc.metrics_path
    );
    let raw = std::fs::read_to_string(&proc.metrics_path).unwrap();
    let metrics: Value = serde_json::from_str(&raw).expect("metrics.json is valid JSON");
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
        assert!(
            metrics.get(key).is_some(),
            "metrics.json missing `{key}`: {metrics}"
        );
    }
    assert_eq!(metrics["ok"], true);
    assert!(metrics["learn_calls"].as_u64().unwrap() >= 1);

    // 10. Shutdown — daemon should exit with code 0 in a couple seconds.
    let bye = client.call(&json!({"cmd": "shutdown"}));
    assert_eq!(bye["status"], "ok", "shutdown resp: {bye}");
    drop(client);

    // Give the process up to 5s to exit.
    let mut proc = proc;
    let deadline = Instant::now() + Duration::from_secs(5);
    loop {
        match proc.child.try_wait() {
            Ok(Some(status)) => {
                assert!(status.success(), "daemon exit status: {status:?}");
                break;
            }
            Ok(None) => {
                if Instant::now() > deadline {
                    panic!("daemon did not exit within 5s of shutdown");
                }
                thread::sleep(Duration::from_millis(50));
            }
            Err(e) => panic!("wait failed: {e}"),
        }
    }
}

/// Language + cognition RPCs against a brain built with `language`
/// (L8 + C4/C5): learn → comprehend → respond → reason → toggle →
/// explicit WM / imagination feeds → cognition_state reflects it all.
fn run_language_scenario() {
    let cfg = json!({
        "rng_seed": 7,
        "deterministic": true,
        "state_dir": "./ignored",
        "adaptive": {"layers": [2, 4, 1], "rng_seed": 7, "activation": "Tanh"},
        "language": {
            "semantic_dim": 8,
            "rng_seed": 7,
            "enable_toxicity": true,
            "toxicity_ml_seed": 9,
            "spectrum_vocab_cap": null,
            "default_stage": 4,
            "min_produce_words": 1,
            "recurrent_max_iters": 1,
            "reason_in_content": false
        }
    });
    let proc = DaemonProc::spawn_with_config(Some(&cfg));
    let mut client = DaemonClient::connect(&proc.socket);

    for _ in 0..5 {
        let r = client.call(&json!({"cmd": "language_learn", "text": "the dog likes to run"}));
        assert_eq!(r["status"], "ok", "{r}");
        assert_eq!(r["data"]["learned"], true);
    }
    let r = client.call(&json!({"cmd": "language_learn", "text": "the cat likes to sleep"}));
    assert!(
        r["data"]["stats"]["learn_calls"].as_u64().unwrap() >= 6,
        "{r}"
    );

    let c = client.call(&json!({"cmd": "language_comprehend", "text": "the dog"}));
    assert_eq!(c["status"], "ok", "{c}");
    assert_eq!(c["data"]["vector"].as_array().unwrap().len(), 8);
    assert!(c["data"]["confidence"].as_f64().unwrap() > 0.0);

    let state0 = client.call(&json!({"cmd": "cognition_state"}));
    assert_eq!(state0["status"], "ok", "{state0}");
    assert_eq!(state0["data"]["now_ms"], 0);
    assert_eq!(
        state0["data"]["working_memory"].as_array().unwrap().len(),
        0
    );
    assert!(state0["data"]["imagination"].is_null());

    let resp = client.call(&json!({"cmd": "language_respond", "text": "the dog"}));
    assert_eq!(resp["status"], "ok", "{resp}");
    assert!(!resp["data"]["text"].as_str().unwrap().is_empty());
    assert_eq!(resp["data"]["blocked_for_toxicity"], false);
    assert!(resp["data"]["confidence"].as_f64().unwrap().is_finite());

    // One turn fed WM (the prompt) and staged the imagined scene.
    let state1 = client.call(&json!({"cmd": "cognition_state"}));
    assert_eq!(state1["data"]["now_ms"], 1000, "{state1}");
    assert_eq!(
        state1["data"]["working_memory"].as_array().unwrap().len(),
        1
    );
    assert_eq!(state1["data"]["imagination"]["dim"], 8);

    let reason = client.call(&json!({"cmd": "reason", "query": "what does the dog do"}));
    assert_eq!(reason["status"], "ok", "{reason}");
    assert!(reason["data"]["conclusion"]["confidence"].is_number());
    assert!(reason["data"]["lexicon_confidence"].is_number());
    assert!(reason["data"]["memory_hits"].is_array());

    let t = client.call(&json!({"cmd": "set_reason_in_content", "on": true}));
    assert_eq!(t["data"]["reason_in_content"], true, "{t}");

    let w = client.call(&json!({
        "cmd": "working_memory_add",
        "data": [0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        "salience": 0.9
    }));
    assert_eq!(w["status"], "ok", "{w}");
    assert_eq!(w["data"]["size"], 2);
    let i = client.call(&json!({
        "cmd": "imagination_stage",
        "content": [0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0],
        "vividness": 0.8
    }));
    assert_eq!(i["status"], "ok", "{i}");
    assert!((i["data"]["vividness"].as_f64().unwrap() - 0.8).abs() < 1e-6);

    let tox = client.call(&json!({"cmd": "classify_toxicity", "text": "the dog likes to run"}));
    assert_eq!(tox["status"], "ok", "{tox}");
    assert_eq!(tox["data"]["would_block"], false);

    // With reasoning on + seeded sources the respond path still works.
    let resp2 = client.call(&json!({"cmd": "language_respond", "text": "the dog"}));
    assert_eq!(resp2["status"], "ok", "{resp2}");
    assert!(!resp2["data"]["text"].as_str().unwrap().is_empty());
    let state2 = client.call(&json!({"cmd": "cognition_state"}));
    assert_eq!(state2["data"]["reason_in_content"], true);
    assert_eq!(state2["data"]["now_ms"], 2000);

    let bye = client.call(&json!({"cmd": "shutdown"}));
    assert_eq!(bye["status"], "ok", "{bye}");
}

/// SNN watchdog RPCs against a brain with a tiny SNN: tune_get reflects
/// construction values, tune is applied and read back, recorded-only knobs
/// say so, unknown / invalid are structured errors, force_quench reports
/// the neurons it reset, pop stats still work afterwards.
fn run_snn_watchdog_scenario() {
    let pop = |name: &str| {
        json!({
            "name": name, "n_neurons": 16, "lif": nimcp_snn::LifParams::default(),
            "target_rate": 0.1, "homeostatic": nimcp_plasticity::HomeostaticParams::default(),
        })
    };
    let cfg = json!({
        "rng_seed": 3,
        "deterministic": true,
        "state_dir": "./ignored",
        "adaptive": {"layers": [2, 4, 1], "rng_seed": 3, "activation": "Tanh"},
        "snn": {
            "populations": [pop("in"), pop("out")],
            "edges": [{"src": 0, "dst": 1, "fan_in": 8, "weight_init": 0.4, "weight_jitter": 0.0,
                       "rstdp": nimcp_snn::RstdpParams::default()}],
            "rng_seed": 3,
            // `rate_ema_alpha` deliberately omitted — serde default.
        }
    });
    let proc = DaemonProc::spawn_with_config(Some(&cfg));
    let mut client = DaemonClient::connect(&proc.socket);

    let get0 = client.call(&json!({"cmd": "snn_tune_get"}));
    assert_eq!(get0["status"], "ok", "{get0}");
    let p0 = &get0["data"]["params"];
    assert!(
        (p0["target_rate"].as_f64().unwrap() - 0.1).abs() < 1e-6,
        "{p0}"
    );
    assert!(
        (p0["rstdp_lr"].as_f64().unwrap() - 1e-4).abs() < 1e-9,
        "{p0}"
    );
    for k in [
        "homeo_min_scale",
        "homeo_max_scale",
        "noise_rate_hz",
        "max_scale_dead",
        "dead_threshold",
    ] {
        assert!(p0[k].is_number(), "{k} missing: {p0}");
    }

    let t = client.call(&json!({"cmd": "snn_tune", "name": "target_rate", "value": 0.05}));
    assert_eq!(t["status"], "ok", "{t}");
    assert_eq!(t["data"]["applied"], true);
    assert_eq!(t["data"]["populations"], 2);
    let get1 = client.call(&json!({"cmd": "snn_tune_get"}));
    assert!(
        (get1["data"]["params"]["target_rate"].as_f64().unwrap() - 0.05).abs() < 1e-6,
        "{get1}"
    );

    let lr = client.call(&json!({"cmd": "snn_tune", "name": "rstdp_lr", "value": 0.00005}));
    assert_eq!(lr["data"]["applied"], true, "{lr}");
    assert_eq!(lr["data"]["edges"], 1);

    let rec = client.call(&json!({"cmd": "snn_tune", "name": "max_scale_dead", "value": 1.07}));
    assert_eq!(rec["status"], "ok", "{rec}");
    assert_eq!(rec["data"]["applied"], false);
    assert!(
        rec["data"]["note"]
            .as_str()
            .unwrap()
            .contains("not applied")
    );

    let bad = client.call(&json!({"cmd": "snn_tune", "name": "nonsense", "value": 1.0}));
    assert_eq!(bad["status"], "err", "{bad}");
    let inv = client.call(&json!({"cmd": "snn_tune", "name": "target_rate", "value": 7.0}));
    assert_eq!(inv["status"], "err", "{inv}");

    let q = client.call(&json!({"cmd": "snn_force_quench", "n": 5}));
    assert_eq!(q["status"], "ok", "{q}");
    assert_eq!(q["data"]["zeroed"], 10, "5 per population × 2");

    let step =
        client.call(&json!({"cmd": "snn_step", "drive": [[], []], "reward": 0.0, "dt_ms": 1.0}));
    assert_eq!(step["status"], "ok", "{step}");
    let pops = client.call(&json!({"cmd": "snn_pop_stats"}));
    assert_eq!(pops["status"], "ok", "{pops}");
    assert_eq!(pops["data"]["pops"].as_array().unwrap().len(), 2);

    let bye = client.call(&json!({"cmd": "shutdown"}));
    assert_eq!(bye["status"], "ok", "{bye}");
}

fn run_under_timeout(scenario: fn()) {
    // Run the scenario on a dedicated thread so we can hard-fail on
    // the global TIMEOUT. The scenario spawns + reaps the daemon itself.
    let (tx, rx) = std::sync::mpsc::channel::<()>();
    let h = thread::spawn(move || {
        scenario();
        tx.send(()).ok();
    });

    match rx.recv_timeout(TIMEOUT) {
        Ok(()) => {
            h.join().expect("scenario thread panicked");
        }
        Err(_) => panic!(
            "daemon roundtrip exceeded {}s timeout — likely hang",
            TIMEOUT.as_secs()
        ),
    }
}

#[test]
fn daemon_roundtrip_all_commands_under_timeout() {
    run_under_timeout(run_scenario);
}

#[test]
fn daemon_language_rpcs_under_timeout() {
    run_under_timeout(run_language_scenario);
}

#[test]
fn daemon_snn_watchdog_rpcs_under_timeout() {
    run_under_timeout(run_snn_watchdog_scenario);
}

/// A config the brain rejects must surface the daemon's own error text
/// through the spawn helper, not a bare socket-bind timeout.
#[test]
fn daemon_spawn_reports_config_errors() {
    let bad = json!({
        "rng_seed": 1,
        "deterministic": true,
        "state_dir": "./ignored",
        "adaptive": {"layers": [2, 4, 1], "rng_seed": 1, "activation": "Tanh"},
        // `populations` missing → serde error at config parse.
        "snn": {"edges": [], "rng_seed": 1}
    });
    let outcome = thread::spawn(move || {
        let _proc = DaemonProc::spawn_with_config(Some(&bad));
    })
    .join();
    let payload = outcome.expect_err("spawn must panic on an unparseable config");
    let msg = payload
        .downcast_ref::<String>()
        .cloned()
        .or_else(|| payload.downcast_ref::<&str>().map(|s| (*s).to_string()))
        .unwrap_or_default();
    assert!(msg.contains("exited before binding"), "{msg}");
    assert!(
        msg.contains("populations"),
        "daemon stderr should name the missing field: {msg}"
    );
}
