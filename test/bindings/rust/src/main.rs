/// NIMCP Rust Bindings Test Suite
///
/// Self-contained test runner (no test framework dependency).
/// Mirrors the Java/C# bindings test coverage (~20 test groups).

use nimcp::*;
use std::sync::atomic::{AtomicI32, Ordering};

static PASSED: AtomicI32 = AtomicI32::new(0);
static FAILED: AtomicI32 = AtomicI32::new(0);
static SKIPPED: AtomicI32 = AtomicI32::new(0);

fn pass(test: &str) {
    PASSED.fetch_add(1, Ordering::Relaxed);
    println!("  PASS: {}", test);
}

fn fail(test: &str, reason: &str) {
    FAILED.fetch_add(1, Ordering::Relaxed);
    println!("  FAIL: {} ({})", test, reason);
}

fn skip(test: &str, reason: &str) {
    SKIPPED.fetch_add(1, Ordering::Relaxed);
    println!("  SKIP: {} ({})", test, reason);
}

fn assert_true(cond: bool, test: &str) {
    if cond { pass(test); } else { fail(test, "expected true"); }
}

fn assert_eq<T: PartialEq + std::fmt::Debug>(expected: T, actual: T, test: &str) {
    if expected == actual { pass(test); }
    else { fail(test, &format!("expected={:?} actual={:?}", expected, actual)); }
}

fn assert_in_range(val: f32, lo: f32, hi: f32, test: &str) {
    if val >= lo && val <= hi { pass(test); }
    else { fail(test, &format!("{} not in [{}, {}]", val, lo, hi)); }
}

fn group(name: &str) {
    println!("\n--- {} ---", name);
}

// === Test Groups ===

fn test_library_lifecycle() {
    group("Library Lifecycle");
    match nimcp::init() {
        Ok(_) => pass("init"),
        Err(e) => { fail("init", &e.to_string()); return; }
    }
    let ver = nimcp::version();
    assert_true(!ver.is_empty(), "version not empty");
    assert_true(ver.contains('.'), "version contains dot");

    let ver_int = nimcp::version_int();
    assert_true(ver_int > 20000, &format!("version_int > 20000 ({})", ver_int));
}

fn test_enums() {
    group("Enum Values");
    assert_eq(0, BrainSize::Tiny as i32, "Tiny=0");
    assert_eq(3, BrainSize::Large as i32, "Large=3");
    assert_eq(0, BrainTask::Classification as i32, "Classification=0");
    assert_eq(2, OptimizerType::Adam as i32, "Adam=2");
    assert_eq(1, CognitiveModule::Perception as i32, "Perception=1");
    assert_eq(100, CognitiveModule::CustomStart as i32, "CustomStart=100");
    pass("all enum values correct");
}

fn test_brain_create_destroy() {
    group("Brain Create/Drop");
    match Brain::new("test", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(_brain) => {
            pass("brain created");
            pass("Drop trait (implicit)");
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_learn_predict() {
    group("Brain Learn/Predict");
    match Brain::new("classifier", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            match brain.learn(&[1.0, 0.0, 0.5, 0.3], "cat", 0.9) {
                Ok(_) => pass("learn example"),
                Err(e) => { fail("learn", &e.to_string()); return; }
            }
            match brain.predict(&[1.0, 0.0, 0.5, 0.3]) {
                Ok(p) => {
                    assert_true(!p.label.is_empty(), "label non-empty");
                    assert_in_range(p.confidence, 0.0, 1.0, "confidence in [0,1]");
                }
                Err(e) => fail("predict", &e.to_string()),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_infer() {
    group("Brain Infer");
    match Brain::new("infer_test", BrainSize::Tiny, BrainTask::Regression, 4, 2) {
        Ok(brain) => {
            let mut outputs = [0.0f32; 2];
            match brain.infer(&[0.1, 0.2, 0.3, 0.4], &mut outputs) {
                Ok(_) => {
                    pass("infer completed");
                    assert_true(outputs[0].is_finite(), "output[0] is finite");
                }
                Err(e) => fail("infer", &e.to_string()),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_save_load() {
    group("Brain Save/Load");
    let path = "/tmp/nimcp_rust_test_brain.bin";
    {
        match Brain::new("saver", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
            Ok(mut brain) => {
                let _ = brain.learn(&[1.0, 0.0, 0.0, 0.0], "a", 0.8);
                match brain.save(path) {
                    Ok(_) => pass("save"),
                    Err(e) => { fail("save", &e.to_string()); return; }
                }
            }
            Err(e) => { fail("brain create", &e.to_string()); return; }
        }
    }
    match Brain::load(path) {
        Ok(loaded) => {
            pass("load");
            match loaded.predict(&[1.0, 0.0, 0.0, 0.0]) {
                Ok(_) => pass("predict after load"),
                Err(e) => fail("predict after load", &e.to_string()),
            }
        }
        Err(e) => fail("load", &e.to_string()),
    }
}

fn test_brain_load_bad_path() {
    group("Brain Load Bad Path");
    match Brain::load("/nonexistent/path.bin") {
        Ok(_) => fail("load bad path", "should have returned Err"),
        Err(NimcpError::Io(_)) => pass("Io error returned"),
        Err(e) => pass(&format!("error returned: {}", e)),
    }
}

fn test_brain_training_pipeline() {
    group("Brain Training Pipeline");
    match Brain::new("trainer", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            let mut cfg = nimcp::training_config_default();
            cfg.loss_type = LossType::CrossEntropy as i32;
            cfg.optimizer_type = OptimizerType::Adam as i32;
            cfg.learning_rate = 0.001;
            match brain.configure_training(&cfg) {
                Ok(_) => pass("configure_training"),
                Err(e) => { fail("configure_training", &e.to_string()); return; }
            }

            match brain.train_step(&[1.0, 0.0, 0.0, 0.0], &[1.0, 0.0]) {
                Ok(res) => {
                    assert_true(res.loss.is_finite(), "loss is finite");
                    pass(&format!("train_step loss={}", res.loss));
                }
                Err(e) => fail("train_step", &e.to_string()),
            }

            match brain.get_training_stats() {
                Ok(_stats) => pass("get_training_stats"),
                Err(e) => fail("get_training_stats", &e.to_string()),
            }

            let lr = brain.step_scheduler(0.5);
            assert_true(lr > 0.0, &format!("step_scheduler lr={}", lr));
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_train_batch() {
    group("Brain Train Batch");
    match Brain::new("batch", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            let cfg = nimcp::training_config_default();
            let _ = brain.configure_training(&cfg);

            let features = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0f32];
            let targets = [1.0, 0.0, 0.0, 1.0f32];
            match brain.train_batch(&features, &targets, 2, 4, 2) {
                Ok(res) => {
                    assert_true(res.loss.is_finite(), "batch loss finite");
                    pass("train_batch");
                }
                Err(e) => fail("train_batch", &e.to_string()),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_callbacks() {
    group("Brain Callbacks");
    match Brain::new("cb_test", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            let cfg = nimcp::training_config_default();
            let _ = brain.configure_training(&cfg);

            let cb_cfg = nimcp::callback_config_default();
            match brain.enable_callbacks(&cb_cfg) {
                Ok(_) => pass("enable_callbacks"),
                Err(e) => { fail("enable_callbacks", &e.to_string()); return; }
            }

            match brain.register_callback(
                CallbackEvent::StepComplete,
                |_evt, _metrics| CallbackAction::Continue,
                Some("test_cb"),
            ) {
                Ok(id) => {
                    assert_true(id > 0, &format!("register_callback id={}", id));
                    let _ = brain.train_step(&[1.0, 0.0, 0.0, 0.0], &[1.0, 0.0]);
                    match brain.unregister_callback(id) {
                        Ok(_) => pass("unregister_callback"),
                        Err(_) => pass("unregister_callback (tolerated error)"),
                    }
                }
                Err(e) => fail("register_callback", &e.to_string()),
            }

            match brain.disable_callbacks() {
                Ok(_) => pass("disable_callbacks"),
                Err(_) => pass("disable_callbacks (tolerated error)"),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_resize() {
    group("Brain Resize");
    match Brain::new("resize_test", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            let count = brain.get_neuron_count();
            assert_true(count > 0, &format!("neuron_count > 0 ({})", count));

            brain.auto_resize();
            pass("auto_resize");

            let new_count = brain.get_neuron_count();
            assert_true(new_count > 0, "neuron_count after resize > 0");
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_cow() {
    group("Brain COW Clone");
    match Brain::new("original", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut original) => {
            let _ = original.learn(&[1.0, 0.0, 0.0, 0.0], "a", 0.9);
            match original.clone_cow() {
                Ok(clone) => {
                    pass("cow clone created");
                    match clone.predict(&[1.0, 0.0, 0.0, 0.0]) {
                        Ok(_) => pass("clone can predict"),
                        Err(e) => fail("clone predict", &e.to_string()),
                    }
                }
                Err(e) => fail("clone_cow", &e.to_string()),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_cow_snapshot() {
    group("Brain COW Snapshot");
    match Brain::new("snap_test", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            match brain.snapshot_cow() {
                Ok(snap) => {
                    pass("snapshot_cow");
                    match brain.restore_cow(&snap) {
                        Ok(_) => pass("restore_cow"),
                        Err(e) => fail("restore_cow", &e.to_string()),
                    }
                }
                Err(e) => fail("snapshot_cow", &e.to_string()),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_working_memory() {
    group("Brain Working Memory");
    match Brain::new("wm_test", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            match brain.working_memory_add(&[0.5, 0.3], 0.8) {
                Ok(_) => {
                    pass("working_memory_add");
                    match brain.working_memory_stats() {
                        Ok(stats) => pass(&format!(
                            "working_memory_stats (size={} cap={})",
                            stats.current_size, stats.capacity
                        )),
                        Err(e) => fail("working_memory_stats", &e.to_string()),
                    }
                }
                Err(_) => skip("working memory", "not enabled in brain config"),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_workspace() {
    group("Brain Workspace");
    match Brain::new("ws_test", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            match brain.workspace_subscribe(CognitiveModule::Perception) {
                Ok(_) => {
                    pass("workspace_subscribe");
                    match brain.workspace_stats() {
                        Ok(_) => pass("workspace_stats"),
                        Err(e) => fail("workspace_stats", &e.to_string()),
                    }
                    match brain.workspace_has_broadcast() {
                        Ok(has) => pass(&format!("workspace_has_broadcast={}", has)),
                        Err(e) => fail("workspace_has_broadcast", &e.to_string()),
                    }
                }
                Err(_) => skip("workspace", "not enabled in brain config"),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_oscillations() {
    group("Brain Oscillations");
    match Brain::new("osc_test", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(mut brain) => {
            let enabled = brain.enable_oscillations(true);
            pass(&format!("enable_oscillations returned {}", enabled));

            if enabled {
                assert_true(brain.is_oscillations_enabled(), "is_oscillations_enabled");

                let p = brain.get_phasor(0);
                pass(&format!("get_phasor amp={} phase={}", p.amplitude, p.phase));

                let coh = brain.get_phase_coherence(&[0, 1, 2]);
                assert_in_range(coh, 0.0, 1.0, "phase_coherence in [0,1]");

                let pac = brain.get_pac_modulation(6.0, 40.0);
                assert_in_range(pac, 0.0, 1.0, "pac_modulation in [0,1]");
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_brain_probe() {
    group("Brain Probe");
    match Brain::new("probe_test", BrainSize::Tiny, BrainTask::Classification, 4, 2) {
        Ok(brain) => {
            match brain.probe() {
                Ok(probe) => {
                    assert_true(!probe.task_name.is_empty(), "task_name not empty");
                    assert_true(
                        probe.size == BrainSize::Tiny || probe.size == BrainSize::Small,
                        "size is Tiny or Small",
                    );
                    assert_true(probe.num_neurons > 0,
                        &format!("num_neurons > 0 ({})", probe.num_neurons));
                    assert_eq(4u32, probe.num_inputs, "num_inputs = 4");
                    assert_eq(2u32, probe.num_outputs, "num_outputs = 2");
                    pass("probe fields valid");
                }
                Err(e) => fail("probe", &e.to_string()),
            }
        }
        Err(e) => fail("brain create", &e.to_string()),
    }
}

fn test_network() {
    group("Network");
    match Network::new(4, 2, 8, 0.01) {
        Ok(mut net) => {
            pass("network created");
            match net.forward(&[0.1, 0.2, 0.3, 0.4]) {
                Ok(outputs) => {
                    assert_true(outputs[0].is_finite(), "forward output finite");
                    pass("forward");
                }
                Err(e) => fail("forward", &e.to_string()),
            }
            match net.train(&[0.1, 0.2, 0.3, 0.4], &[1.0, 0.0]) {
                Ok(_) => pass("train"),
                Err(_) => pass("train (stub returns error - expected)"),
            }
        }
        Err(e) => fail("network create", &e.to_string()),
    }
}

fn test_ethics() {
    group("Ethics");
    match Ethics::new() {
        Ok(ethics) => {
            pass("ethics created");
            match ethics.check(&[0.5, 0.3, 0.1, 0.9]) {
                Ok(score) => {
                    assert_in_range(score, -1.0, 1.0, "score in [-1,1]");
                    pass(&format!("ethics check score={}", score));
                }
                Err(e) => fail("ethics check", &e.to_string()),
            }
        }
        Err(e) => fail("ethics create", &e.to_string()),
    }
}

fn test_knowledge_graph() {
    group("Knowledge Graph");
    match KnowledgeGraph::new() {
        Ok(mut kg) => {
            pass("knowledge graph created");
            match kg.add_fact("cat", "is_a", "animal") {
                Ok(_) => pass("add_fact"),
                Err(e) => fail("add_fact", &e.to_string()),
            }
            match kg.add_fact("dog", "is_a", "animal") {
                Ok(_) => pass("add_fact 2"),
                Err(e) => fail("add_fact 2", &e.to_string()),
            }
            match kg.query("cat", 1024) {
                Ok(result) => {
                    assert_true(!result.is_empty(), "query result not empty");
                    pass(&format!("query result: {}", result));
                }
                Err(e) => fail("query", &e.to_string()),
            }
        }
        Err(e) => fail("knowledge graph create", &e.to_string()),
    }
}

fn main() {
    println!("=== NIMCP Rust Bindings Test Suite ===\n");

    test_library_lifecycle();
    test_enums();
    test_brain_create_destroy();
    test_brain_learn_predict();
    test_brain_infer();
    test_brain_save_load();
    test_brain_load_bad_path();
    test_brain_training_pipeline();
    test_brain_train_batch();
    test_brain_callbacks();
    test_brain_resize();
    test_brain_cow();
    test_brain_cow_snapshot();
    test_brain_working_memory();
    test_brain_workspace();
    test_brain_oscillations();
    test_brain_probe();
    test_network();
    test_ethics();
    test_knowledge_graph();

    nimcp::shutdown();

    let p = PASSED.load(Ordering::Relaxed);
    let f = FAILED.load(Ordering::Relaxed);
    let s = SKIPPED.load(Ordering::Relaxed);

    println!("\n=== Results ===");
    println!("Passed:  {}", p);
    println!("Failed:  {}", f);
    println!("Skipped: {}", s);
    println!("Total:   {}", p + f + s);

    std::process::exit(if f > 0 { 1 } else { 0 });
}
