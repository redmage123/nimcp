# Migrating from V1 (Athena) to V2 (Hera)

**Audience:** existing V1 users and operators of `scripts/immerse_athena.py`.

**Scope:** concrete steps to move a training workload from V1's C brain
(Athena) to V2's Rust brain (Hera). V2 is intentionally narrower than
V1 — this document tells you what's the same, what's different, and
what's been dropped. No speculative roadmap.

---

## 1. Mental model

| | V1 — Athena | V2 — Hera |
|---|---|---|
| Language | C | Rust |
| Core struct | `brain_struct` (800+ fields) | `Brain` (small handle; actors own state) |
| Python module | `nimcp` | `nimcp_v2` |
| Networks | ANN, SNN, LNN, CNN, FNO, HNN | Adaptive (MLP), SNN, LNN |
| Cognitive modules | 60+ (introspection, ethics, theory of mind, …) | 0 — re-admit per 30-day isolation rule |
| Brain regions | 33+ first-class | subsumed by actors |
| Language bindings | 8 (Python, Java, Go, Rust, C++, Node, Perl, C#) | 1 (Python) |
| Checkpoint format | Custom binary | rkyv (adaptive) + JSON (SNN/LNN/memory) |
| Default model filesystem layout | `checkpoints/athena/`, `/var/run/athena/brain.sock` | `checkpoints/hera/`, `/var/run/hera/brain.sock` |

Both can run on the same host — paths don't collide.

---

## 2. Three migration paths

### Path A — adapter shim (fastest)

Drive V2 through V1's `scripts/immerse_athena.py` by flipping one flag:

```bash
python3 scripts/immerse_athena.py --backend=v2 [...]
# or:
NIMCP_BACKEND=v2 python3 scripts/immerse_athena.py [...]
```

The shim (`scripts/v2_brain_adapter.py`) exposes V1's 77-method brain
surface over `nimcp_v2.Brain`:

- **10 methods** pass-through directly (`learn_vector`, `predict`,
  `save`, `get_stats`, …).
- **~60 methods** are no-op stubs that return safe defaults and emit
  a single WARNING per method on first call — cognitive instrumentation
  V2 doesn't have (`bg_*`, `medulla_*`, `sleep_*`, `octopus_*`, …).
- **~7 methods** have semantic-gap adapters (e.g., `train_cognitive`
  reduces to `brain.learn()`; `speak` is a stub because V2 has no TTS).
  The language methods — `train_language`, `learn_language`,
  `generate_text`, `grounded_respond`, plus `comprehend`, `reason`,
  `set_reason_in_content`, `working_memory_add` — drive V2's grounded-
  language engine (L8) and cognition sources (C4/C5) when the brain's
  `config_json` has a `language` section; without one they stay stubs.
  `train_language` reports `1 − comprehension_confidence` as its loss
  proxy (V2 has no supervised language loss).

Expect WARNINGs to be loud on the first run — they tell you which
cognitive features your workload actually relies on and aren't in V2
yet.

Use this path to get V2 running under the same ops shell (systemd,
cron monitor, BrainProxy, checkpoint rotation) with minimal change.

### Path B — native Python (`nimcp_v2.Brain`)

Bypass the shim and use V2 directly:

```python
import nimcp_v2
brain = nimcp_v2.Brain(
    rng_seed=42,
    deterministic=True,
    layers=[64, 32, 10],
    activation="tanh",
)
# or for the full ensemble:
brain = nimcp_v2.Brain.from_json(json.dumps(cfg_dict))

loss = brain.learn(features, target, lr=0.01)
y = brain.predict(features)
stats = brain.stats()
brain.save_ensemble("checkpoints/hera/snapshot_v2_20260422")
```

Full Python surface documented in `pybind/src/lib.rs`:
Phase 1 (`learn` / `predict` / `save` / `load`),
Phase 6b (`stats` / `stats_json`),
Phase 7b (`snn_step`, `lnn_forward_step`, `lnn_train_step_mse`,
`memory_insert`, `memory_query_all`, `memory_consolidate`,
`save_ensemble` / `load_ensemble`, `from_json`).

### Path C — Rust host process

For pod-scale production, run the V2 daemon and drive it via a thin
Python client (one RPC per harness method). The daemon wraps `Brain`
behind a Unix socket with a line-delimited JSON protocol:

```bash
target/release/nimcp-v2-daemon \
  --socket /var/run/hera/brain.sock \
  --state-dir /var/lib/hera
```

This is the path `scripts/immerse_athena.py --backend=v2` will use once
we wire the shim to the daemon (Phase 7 also has direct in-process
loading via PyO3; pick based on deployment needs).

---

## 3. Behavioural differences to watch

### Stability mechanisms

V2's SNN inherits all of V1's Phase 3.5 biophysical fixes:

- AHP + Na/K pump spike-rate adaptation
- Basket-cell pool (fast-spiking inhibitory)
- Poisson noise with adaptive per-population factor
- Short-term depression
- Intrinsic reward + anti-reward
- Reward-coupled homeostatic scaling (opt-in)

In addition V2 ports master's `1a495f51d` three control fixes and the
`2bd4099ff` stronger Poisson defaults (20 Hz × 30 mV).

**All opt-in on V2.** Default brain sees bit-identical pre-Phase-3.5
behavior. Enable via `SnnConfig` / `PopulationSpec` fields (see
`crates/networks/snn/src/network.rs`).

### Substrate + thalamic

V2 also ships Path A biological infrastructure (`nimcp-substrate`,
`nimcp-thalamic`) with per-network adapters wired through `step()`.
V1 has parts of these too but V2's versions are purpose-built and
uniform across SNN + LNN.

**All opt-in.** Default brains see no multiplicative modulation.

### Dropped V1 features

Per `docs/V2_PLAN.md` §5, intentionally dropped from V2 core:

- 60 cognitive modules (reintroduce per 30-day isolation rule).
- 33 brain regions as first-class.
- CNN / FNO / HNN networks.
- Swarm runtime, drone bridges, ROS 2, sim-to-real.
- 9-layer safety governance (simplified to input validation + output
  bounds).
- Brain-native language, emergent alien language mode.
- Phi-3 / Claude teacher tight integration.
- 7 of 8 language bindings (Python only for now).

If your V1 workload depends on any of these, either use the shim path
(the no-op stubs keep the harness alive) or wait for the feature's
V2 port.

### Determinism

V2 is stricter about determinism than V1:

- All RNGs use `ChaCha20Rng` seeded from config.
- `deterministic: true` on `BrainConfig` forces virtual-time scheduling.
- Bit-identical checkpoint round-trips across runs with same seed.

Workloads that depend on non-deterministic behavior (e.g. race-based
priority queues in V1's `bio_router`) will behave differently.

---

## 4. Checkpoint migration

**V1 → V2 direct migration is not supported.** Formats differ
fundamentally. Options:

1. **Parallel training** — run V2 from scratch on the same data;
   compare against the V1 run. Cleanest path for a new model.
2. **Feature distillation** — use V1 model outputs as targets for a
   V2 training pass. Scripted per workload.
3. **Weights extraction** — for adaptive-only workloads, extract
   `neuron_t` weights from V1 checkpoints and rkyv-encode into V2's
   `Brain::save` format. Requires a small one-off tool; not provided.

V2's own checkpoints round-trip via `Brain::save_ensemble(path) /
load_ensemble(path)`, which writes a directory of per-network files
plus a `manifest.json` version tag.

---

## 5. Monitoring + ops

V2 writes the same file layout V1's monitors expect, under
`checkpoints/hera/`:

- `website/metrics.json` — same schema as V1 (`learn_calls`,
  `ann_loss`, `snn_loss`, `lnn_loss`, `training_active`, …).
  Existing `monitor_training_cron.sh` works unchanged once V2 is
  writing to this path.
- `training.log` — line-oriented, `tracing` subscriber output.
- `checkpoints/hera/immersive_state.json` — stage-state JSON.
- `checkpoints/hera/snapshot_v2_<timestamp>.snapshot` directories,
  rotated at 5 (V1 policy).

SNN-specific watchdog (`scripts/snn_watchdog.py`) speaks the V2
daemon's four RPCs unchanged:
`snn_pop_stats`, `snn_tune_get`, `snn_tune`, `snn_force_quench`.

---

## 6. Recipe — 10-minute smoke test

```bash
# 1. Build everything.
cd /home/bbrelin/nimcp
cargo build -p nimcp-pybind --release
cargo build -p nimcp-daemon --release
cp target/release/libnimcp_v2.so /tmp/nimcp_v2.so

# 2. Direct Python smoke.
PYTHONPATH=/tmp python3 -c "
import nimcp_v2
b = nimcp_v2.Brain(rng_seed=42, layers=[4, 8, 2], activation='tanh')
for _ in range(100):
    loss = b.learn([0.1, 0.2, 0.3, 0.4], [0.5, -0.2], lr=0.05)
print(f'final loss: {loss:.4f}')
print(f'stats keys: {sorted(b.stats().keys())}')
"

# 3. Shim smoke via harness flag (doesn't actually start a real training run).
PYTHONPATH=/tmp python3 -c "
import sys, os
sys.path.insert(0, 'scripts')
from v2_brain_adapter import V2BrainAdapter
b = V2BrainAdapter(layers=[4, 8, 2], activation='tanh')
b.learn_vector([0.1, 0.2, 0.3, 0.4], [0.5, -0.2], lr=0.05)
b.consolidate()  # no-op on adaptive-only brain
print('shim smoke OK')
"

# 4. Daemon smoke (socket round-trip).
./target/release/nimcp-v2-daemon --socket /tmp/hera-smoke.sock --state-dir /tmp/hera-state &
PID=$!
sleep 1
# (client test — use crates/daemon/tests/roundtrip.rs as reference)
kill $PID
```

If all three pass, V2 is ready for a real training run under the
existing harness (with `--backend=v2`).

---

## 7. When to stay on V1

- Your workload needs any of the dropped features (§3) that V2 hasn't
  re-admitted yet.
- You're running multilingual bindings (V2 is Python-only).
- You're running the full 9-layer safety stack and the simplified
  "input validation + output bounds" V2 layer isn't acceptable.
- Your ops stack is already tuned for V1's failure modes and you
  don't have headroom for a rewrite.

Otherwise V2 is production-ready for its documented scope: adaptive
MLP, SNN at 1.8M, LNN, memory, substrate+thalamic modulation, full
Python API, adapter shim, daemon.
