# Python API Reference

**Last Updated:** 2026-04-19

Complete reference for the `nimcp.Brain` Python class. 200+ methods total;
this document groups them by purpose.

## Brain Lifecycle

```python
import nimcp
brain = nimcp.Brain(name="athena", size=1800000, task=10,
                    num_inputs=1024, num_outputs=2048)
# Or the full-init factory:
brain = nimcp.Brain.create_full(name="athena", task=nimcp.TASK_CLASSIFICATION,
                                 num_inputs=1024, num_outputs=2048,
                                 neuron_count=150000)
brain.save("/path/to/checkpoint.bin")
brain = nimcp.Brain.load("/path/to/checkpoint.bin")
```

Positional args: `(name, size, task)`. Keyword args include `num_inputs`,
`num_outputs`, `neuron_count`, etc.

## Core Training / Inference

| Method | Signature | Notes |
|--------|-----------|-------|
| `learn` | `(features, label, lr=0.0, confidence=1.0) -> float` | One-hot supervised |
| `learn_vector` | `(features, target, label=None, confidence=1.0, learning_rate=None) -> float` | Target vector |
| `learn_vector_batch` | `([(features, target), ...], learning_rate=None) -> float` | Accumulated update (currently limited by batch-instability) |
| `experience` | `(input, output_size, teacher_reward=0.0) -> dict` | Merged inference + learning |
| `experience_correct` | `(expected) -> float` | Teaching signal for last experience |
| `predict` | `(features) -> (label, confidence)` | Full prediction |
| `predict_fast` | `(features) -> (label, confidence)` | Skip cognitive stages |
| `predict_batch` | `(features_list) -> (labels, confidences)` | |
| `predict_in_domain` | `(features, domain) -> (label, confidence)` | Domain-scoped |
| `decide_full` | `(features) -> dict` | Includes output_vector |

## Test-Battery API (added 2026-03)

| Method | Purpose |
|--------|---------|
| `get_mental_health_report()` | Returns scores + severities for 23 disorders |
| `get_mental_health_check(name)` | Score for one disorder |
| `get_emotion_state()` | valence/arousal/intensity/dominant/stability |
| `get_internal_state(strategy=1)` | Compressed brain state vector |
| `predict_with_confidence(features)` | Prediction + epistemic/aleatoric uncertainty |
| `predict_with_deadline(features, deadline_ms)` | Forces fast-path, reports timing |
| `perturb_weights(magnitude, target, tag)` | Mark-test perturbation (logging stub — see known issues) |
| `enter_idle_with_telemetry(duration_ms)` | Offline consolidation window |
| `get_inner_speech_trace(n)` | Recent inner speech (may be empty) |
| `get_hypothesis_log(n)` | Recent abductions |

## State Snapshot / Restore

| Method | Purpose |
|--------|---------|
| `snapshot_cow()` | Instant COW snapshot; returns capsule |
| `restore_cow(snapshot)` | Restore from capsule |
| `destroy_cow_snapshot(snapshot)` | Free capsule memory |
| `cow_trial_snapshot()` | Alias for snapshot_cow (harness-friendly) |
| `cow_trial_restore(snapshot)` | Alias for restore_cow |

## SNN / LNN / CNN Access

| Method | Purpose |
|--------|---------|
| `enable_multi_network()` | Enable LNN + CNN ensemble |
| `init_cortex_cnns()` | Create 4 cortex CNN processors |
| `get_snn_stats()` | Spike counts, firing rates, sparsity, synchrony |
| `snn_get_stats()` | Alias (identical keys) |
| `get_population_history(pop_id)` | Last 256 steps of spike counts |
| `snn_force_quench(n=20)` | Force N homeostatic applies (saturation rescue) |
| `snn_set_input_scale(s)` | Input amplification factor |
| `snn_get_input_scale()` | Current factor |
| `lnn_get_state()` | LNN internal state vector |
| `lnn_get_stats()` | Tau distribution, gradient norms |
| `cnn_get_stats()` | Layers, params, labels |

## Biological / Cognitive

| Method | Purpose |
|--------|---------|
| `thalamus_set_attention(nucleus, strength)` | LGN/MGN/VPL/VA/... |
| `thalamus_get_mode(nucleus)` | TONIC / BURST / INHIBITED |
| `bg_get_dopamine()` / `bg_get_rpe()` / `bg_get_conflict()` | Basal ganglia |
| `bg_update_reward(reward, expected)` | Reward prediction error |
| `substrate_get_health()` | OPTIMAL/STRESSED/COMPROMISED/CRITICAL |
| `substrate_get_metabolic()` | ATP, O2, glucose |
| `medulla_get_arousal()` | Arousal level |
| `medulla_get_circadian_efficiency()` | Time-of-day efficiency |
| `sleep_get_pressure()` / `sleep_get_state()` | Sleep system |
| `sleep_run_cycle()` | Run consolidation cycle |

## World Model / Imagination

| Method | Purpose |
|--------|---------|
| `enable_world_model(enabled=True)` | RSSM + JEPA + dreaming |
| `world_model_dream(horizon=5)` | Run imagination rollouts |
| `jepa_predict(context)` | Latent-space prediction |
| `enable_hamiltonian(enable=True)` | Hamiltonian dynamics on LNN |
| `enable_world_model_bridge(enable=True)` | Bridge for training |

## Cerebellum

| Method | Purpose |
|--------|---------|
| `cerebellum_predict_outcome(state)` | Forward model prediction |
| `cerebellum_process_error(error)` | Climbing-fiber LTD signal |

## Plasticity

| Method | Purpose |
|--------|---------|
| `enable_biological_plasticity(enabled=True)` | TPB+EDP+coordinator |
| `get_plasticity_stats()` | RPE, neuromod levels |
| `set_plasticity_state(state)` | ACQUISITION/CONSOLIDATION/MAINTENANCE/STABILIZING |
| `edp_process_reward(reward)` | Consolidate eligibility traces |
| `edp_process_novelty(novelty)` | Attention-modulated plasticity |

## Curiosity / Metacognition

| Method | Purpose |
|--------|---------|
| `curiosity_detect_gaps(topic=None)` | Return unknown concepts |
| `self_assess(domain)` | Self-capability estimate |
| `get_uncertainty(features)` | Epistemic + aleatoric uncertainty |
| `deliberate(topic)` | Multi-perspective deliberation |

## Audit / Safety

| Method | Purpose |
|--------|---------|
| `audit_log(desc, severity=0, details="")` | Append to audit log |
| `audit_search(min_severity=0, max_results=100)` | Query audit log |
| `memory_is_healthy()` | Health check |

## Utility

| Method | Purpose |
|--------|---------|
| `save(path)` | Serialize to file |
| `Brain.load(path)` | Class method: deserialize |
| `resize(new_neuron_count)` | Dynamic resize |
| `auto_resize()` | Auto-resize based on utilization |
| `get_neuron_count()` | Current neurons |
| `get_utilization_metrics()` | `(utilization, saturation)` |
| `probe()` | Metrics dict |
| `repair_nan_weights()` | Zero out NaN/Inf |
| `reinit_weights()` | He-init all weights |

## Daemon RPC (via `brain_client.BrainProxy`)

All the above methods are callable via the daemon. Additionally:

| Method | Notes |
|--------|-------|
| `BrainProxy(socket_path)` | Connect |
| `is_daemon_running(socket_path)` | Check availability |
| `._call(cmd, **kwargs)` | Generic RPC — used by test harness |
| `audit_log_event(event_type, severity, description)` | Append to battery audit log |

## Test Battery (`scripts/run_full_battery.py`)

```bash
python3 scripts/run_full_battery.py \
    --socket /var/run/athena/brain.sock \
    --output /var/lib/athena/reports \
    --notes "checkpoint description"
```

Runs 28 batteries, produces `report.txt` + `report.json` + `report.html`.

## See Also

- [cognitive_battery_api.md](cognitive_battery_api.md) — test battery details
- [../architecture/00_overview.md](../architecture/00_overview.md) — overall architecture
