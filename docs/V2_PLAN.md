# NIMCP V2 — Implementation Plan (Rust Rewrite)

**Status:** planning
**Branch:** `v2` (forked from `master` at `fc952e587`)
**Target:** Rust, async-first, event-sourced, actor-model cognitive architecture
**Model name:** **Hera** (V1 is **Athena**; V2 gets a distinct name so
both can run side-by-side on the same host without path collisions —
systemd unit `hera-brain.service`, socket `/var/run/hera/brain.sock`,
checkpoints `checkpoints/hera/`).

---

## 1. Why V2

V1 (this repo's `master`) taught us concrete lessons. The ones that justify a rewrite:

| V1 pain | V2 answer |
|---|---|
| 800-field `brain_struct`, cross-cutting access patterns | Actor model — every subsystem owns its state |
| Races on shared mutable state (today's `connected_dst` bug, pr_memory tick, many more) | Event-sourced; state mutations are messages |
| Cognitive recovery handler hangs 16s on corrupt heap | Crash-first design: cheap rollback via event replay |
| Saturated-checkpoint resume trap (quiet-start doesn't re-apply) | Quiet-start as a load-time weight transform, keyed on stats |
| Tight-bounds homeostatic oscillation bug (designed and refixed) | Feedback loops analyzed for phase margin before wiring |
| 60 cognitive modules, most unused or "statues" | Strict entry rule: prove value in isolation for 30+ days |
| 8 language bindings, maintenance drag | Python only via PyO3; others post-parity |
| 600+ commits before core training was stable | Toy-brain harness: 100-neuron variants, full-dynamics in seconds |
| C: NULL derefs, unchecked mallocs, missing locks | Rust: ownership, `Send`/`Sync` enforced at compile |

We keep V1's **biological grounding ambition** — homeostasis, multi-timescale memory, neuromodulators, multi-network ensemble. We change the implementation.

---

## 2. Principles

1. **No `unsafe` without a written justification in the PR.** The few FFI points (CUDA, PyO3) will have `unsafe`; nothing else.
2. **Event-sourced state.** Every mutation is an event appended to a log; current state is a materialized view. Checkpointing = truncate log + dump view. Recovery = replay tail.
3. **Actors own their state.** Inter-actor communication is typed messages. No `Arc<Mutex<T>>` passed between domains.
4. **One scheduler.** Tokio-based. Deterministic replay when `--deterministic` flag set.
5. **Toy before scale.** Every feature demonstrated on a 100-neuron toy brain first (runs in <5s) before being tested at 1.8M.
6. **Crash-first.** Use `minidump-writer` crate for crash dumps + `eyre` for rich backtraces from day one. No elaborate recovery; just log + rollback.
7. **Feedback control modeled.** Any feedback loop (homeostatic scaling, R-STDP rate, etc.) gets a Python notebook with phase-margin analysis before wiring.
8. **Quiet-start is a transform, not a code path.** Applied at any weight load based on observed saturation stats.
9. **Scope discipline.** Nothing enters core until 30+ days of isolation validation. No exceptions.

---

## 3. Architecture

### 3.1 Core abstractions

```rust
// Every state change is an event
pub trait Event: Serialize + DeserializeOwned + Send + 'static {
    fn apply(self, state: &mut Self::State);
    type State;
}

// Actors have a mailbox and process messages asynchronously
#[async_trait]
pub trait Actor: Send + 'static {
    type Msg: Send + 'static;
    async fn handle(&mut self, msg: Self::Msg, ctx: &mut Context) -> Result<(), Error>;
}

// Compute kernels are pure: (inputs, weights) -> outputs
pub trait Kernel {
    type Input;
    type Weights;
    type Output;
    fn forward(&self, input: &Self::Input, weights: &Self::Weights) -> Self::Output;
    fn backward(&self, input: &Self::Input, output_grad: &Self::Output,
                weights: &Self::Weights) -> (Self::Input, Self::Weights);
}
```

### 3.2 Top-level layout

```
nimcp-v2/
├── crates/
│   ├── core/              # traits, errors, ID types
│   ├── eventlog/          # persistent append-only log (sled or rkyv)
│   ├── scheduler/         # tokio-based actor runtime
│   ├── gpu/               # compute backend (cust or wgpu)
│   ├── networks/
│   │   ├── adaptive/      # MLP (the V1 "ANN")
│   │   ├── snn/           # spiking (LIF + CSR synapses)
│   │   └── lnn/           # liquid time-constant
│   ├── plasticity/        # STDP, R-STDP, homeostatic (each isolated, unit-testable)
│   ├── memory/            # Z-Ladder (port of V1 Phase E, simplified)
│   ├── checkpoint/        # state serialization via rkyv
│   └── brain/             # integration: brain = composition of actors
├── python/                # PyO3 bindings: nimcp-py
├── tests/                 # cross-crate integration
├── benches/               # criterion benchmarks vs V1
├── examples/
│   ├── toy_brain.rs       # 100 neurons, XOR learning
│   └── saturation.rs      # tests quiet-start recovery
└── Cargo.toml             # workspace root
```

### 3.3 Key invariants

- **No global state.** No `static` muts, no thread-local storage for domain data.
- **All I/O through actors.** No direct file/socket access from computation code.
- **Deterministic seeds.** Every RNG gets an explicit seed from config.
- **Actor death is isolated.** A panicking actor doesn't take down siblings; supervisor logs + restarts.

---

## 4. Phase plan

Estimated for one experienced Rust developer full-time. Numbers are realistic, not optimistic.

### Phase 0 — Foundation (3 weeks)
- Cargo workspace with above crate layout
- Core traits (`Event`, `Actor`, `Kernel`)
- Tokio-based scheduler with deterministic-replay mode
- Event log backed by `sled` (or `rkyv` + custom append file)
- PyO3 scaffolding — empty `nimcp-py` crate with `Brain::new()`
- CI: rustfmt, clippy, cargo test, cargo bench, coverage (`cargo-llvm-cov`)
- Crash pipeline: `minidump-writer` on panic, structured logs via `tracing`
- **Exit criteria:** empty actor runs, emits events, checkpoint + restore round-trips via event replay.

### Phase 1 — Minimum viable brain (6 weeks)
- Adaptive network on CPU: MLP forward + backward, SGD
- Event log captures every weight update
- Python API: `Brain::new(config)`, `brain.learn(x, y)`, `brain.predict(x)`
- Toy-brain test: 100 neurons learn XOR in <5 seconds
- Determinism: identical seed → identical weights, bit-for-bit, across runs
- **Exit criteria:** replaces `adaptive_network_learn` from V1 for MLP workloads on CPU. Benchmark: within 2× of V1 single-threaded.

### Phase 2 — GPU compute (5 weeks)
- Choose backend: **`cust`** (NVIDIA-specific, mature) over `wgpu` (portable but younger for HPC)
- GPU actor: submit job → future; batches across callers
- Port adaptive network forward/backward to CUDA kernels
- Benchmark parity: must match or beat V1 per-step throughput at 150K neurons
- **Exit criteria:** 150K-neuron MLP trains at V1 wall-clock or better.

### Phase 3 — SNN (8 weeks)
- LIF neurons as pure kernel
- CSR synapse format (port V1's lightweight approach — this one V1 got right)
- R-STDP plasticity as event source
- Homeostatic scaling with **load-time quiet-start transform**
- Saturation recovery: integration test that loads saturated weights + verifies recovery within 100 steps
- **Exit criteria:** 1.8M neuron SNN runs at V1 rate; recovery from saturation is automatic.

### Phase 4 — Multi-network ensemble (6 weeks)
- LNN (liquid time-constant) — CPU to start
- Actor per network: adaptive, SNN, LNN
- Shared loss aggregator actor
- Joint checkpoint atomic across networks
- **Exit criteria:** 3-network ensemble trains on multimodal input (image + text), loss converges, checkpoint round-trips.

### Phase 5 — One memory system (5 weeks)
- Z-Ladder port (V1 Phase E was well-designed; just translate)
- Landmark API with capacity eviction
- Checkpoint with full feature payload (unified_mem_manager equivalent by default)
- **Exit criteria:** landmark save/load preserves features; query returns expected hits.

### Phase 6 — Introspection (4 weeks)
- Read-only query actor that aggregates state across all brain actors
- Metrics: firing rates, loss, weight stats, memory audits
- Exposed via Python API
- **Exit criteria:** `brain.stats()` returns a comprehensive, documented dict.

### Phase 7 — Training harness (V1 harness via adapter, 2–3 weeks)

**Revised from the original greenfield plan.** Master already has a
battle-tested operational shell — systemd service, daemon, Unix socket,
BrainProxy, cron monitor, `metrics.json` exporter, timestamped checkpoint
rotation, 24 domains of data on disk. Building all of that again for V2
duplicates infrastructure that works. Instead, V2 rides under the
existing harness via an adapter shim that maps V1's brain API onto V2's
narrower surface.

#### Audit result (see `scripts/immerse_athena.py`)

The harness calls 77 distinct `brain.*` methods:

| Class | Count | Treatment |
|---|---|---|
| Directly mappable to V2 | ~10 | Thin pass-through: `learn_vector` → `Brain::learn`, `save` → `Brain::save_ensemble`, `*_get_stats` → slices of `Brain::stats`, etc. |
| Cognitive instrumentation V2 dropped in §5 | ~60 | Safe no-op stubs returning reasonable defaults (`bg_*`, `medulla_*`, `sleep_*`, `octopus_*`, `visual_cortex_*`, `utm_*`, `enable_*`, etc.) |
| Semantic-gap (harness depends on behavior V2 has no equivalent for) | ~7 | Explicit adapter choice: `train_cognitive` reduces to adaptive+LNN `learn()`, `decide_full` returns `{output: predict(...), ethics: null}`, `train_language`/`learn_language`/`speak`/`generate_text` stub (no tokenizer/TTS on V2) |

#### Sub-phases

- **7a — docs:** this V2_PLAN update (no code).
- **7b — Python bindings expansion:** currently only the adaptive path
  reaches Python. Add `brain.snn_configure/step`, `brain.lnn_configure/
  train_step_mse/forward_step`, `brain.memory_insert/mark_landmark/
  query_*`, `brain.save_ensemble/load_ensemble`, plus dedicated
  `brain.snn_stats()`, `brain.lnn_stats()`, `brain.memory_stats()`
  helpers so the adapter shim has real V2 functionality to forward to.
- **7c — adapter shim:** `scripts/v2_brain_adapter.py` — a Python class
  providing all 77 methods. Mappable ones thin-wrap `nimcp_v2.Brain`;
  no-ops return safe defaults (logged at DEBUG); semantic-gap ones
  reduce to the closest V2 primitive or stub. Unit-tested per method
  category.
- **7d — harness integration:** `scripts/immerse_athena.py --backend=v2`
  flag; when set, `import v2_brain_adapter as nimcp`. The rest of the
  harness is untouched. Small curriculum (1 domain, few thousand steps)
  runs end-to-end.
- **7e — V2 daemon:** `crates/daemon` — a Rust binary wrapping `Brain`
  behind a Unix socket. Protocol is a lean subset of V1's daemon
  protocol — enough that a thin Python client can drive learn / predict
  / save / stats. Managed via a `hera-brain.service` unit modelled on
  V1's `athena-brain.service`. **V2's model is called Hera** (the V1
  model is Athena; V2 gets its own name to avoid confusion when both
  are running on the same host).
- **7f — file-based monitoring compatibility:** master's monitoring
  stack has two layers. This sub-phase covers the file-based layer
  (scripts that read logs + state files on disk, no socket calls):
  `monitor_training_cron.sh`, `monitor_athena.py`, `monitor_collapse.sh`,
  `monitor_stage_completion.sh`, `monitor_training.sh`,
  `watchdog_training.sh`. Adapts "for free" if V2 writes:
    - `website/metrics.json` — existing schema (`learn_calls`,
      `ann_loss`, `snn_loss`, `lnn_loss`, `training_active`,
      `timestamp`, `uptime`, etc.) produced from `Brain::stats()` +
      `LossStats`.
    - `training.log` — line-oriented human-readable log (tracing
      subscriber → file appender).
    - `checkpoints/hera/immersive_state.json` — stage-state JSON the
      stage-completion monitor reads. V2 owns this directory; the V1
      `checkpoints/athena/` stays untouched when both backends run
      side-by-side on the same host.
    - `checkpoints/hera/snapshot_v2_*.snapshot` — timestamped
      ensemble-save directories, rotation kept at 5 (V1 policy).
  Net effort: tiny — mostly write-these-paths wiring.
- **7g — SNN watchdog socket-RPC parity:** master's `snn_watchdog.py`
  is the one monitor that talks to the brain daemon directly (not
  via files). It uses 4 RPCs to detect and correct SNN runaway /
  quench events:
    - `snn_pop_stats` → already collected by `Brain::stats()` under
      `snn.populations`; daemon surfaces as RPC.
    - `snn_tune_get` → expose current `LifParams` / `RstdpParams` /
      `HomeostaticParams` (V2 already has live tuning per commit
      `83ab43f01 feat(snn): real-time parameter tuning`).
    - `snn_tune {name, value}` → forward to V2's live-tuning API.
    - `snn_force_quench {n}` → zero `n` random neurons' membrane
      potentials. V2 autonomous watchdog (`6b5853058`) already has a
      quench primitive; reuse.
  Exit: `python3 scripts/snn_watchdog.py --socket /var/run/hera/
  brain.sock` monitors a running V2 daemon without modification.
  V2 keeps its own autonomous in-process watchdog from phase 3f as
  a second line of defense (belt + suspenders).
- **7h — end-to-end validation:** run `immerse_athena.py --backend=v2`
  for N hours on one domain under systemd; verify checkpoints rotate,
  metrics update, file-based monitors see `training_active=true`,
  socket-based SNN watchdog connects and stays quiet, alerts stay
  empty, and final adaptive loss has decreased.

#### Exit criteria

- `python3 scripts/immerse_athena.py --backend=v2 --domain <name>` runs
  without crashing for ≥1 hour; adaptive loss monotone-decreases on
  EMA; checkpoints round-trip via `Brain::save_ensemble` /
  `load_ensemble`.
- V2 daemon under `hera-brain.service` survives operator disconnect
  (same semantics as V1 daemon per commit `6b5853058`).
- Existing file-based monitor cron (`monitor_training_cron.sh`) correctly
  reports V2's `training_active` state; `TRAINING_ALERT.txt` stays
  empty for the full run.
- `scripts/snn_watchdog.py --socket /var/run/hera/brain.sock`
  connects, reads population stats, and either issues no corrective
  action (healthy run) or successfully tunes / quenches on induced
  runaway (injected-fault test).

#### What's explicitly not in this phase (deferred)

- Multi-stage curriculum from scratch — the existing curriculum already
  has stages; we ride it.
- Claude teacher HTTP actor — harness already has integration; shim
  passes through or stubs.
- Multimodal data loaders — harness already has them; when a loader
  produces features V2 can't consume (raw images, audio frames), the
  adapter stubs the cortex call and trains downstream networks on what
  the harness already flattens to vectors.
- Cognitive module parity — reintroduction of dropped modules happens
  per the §5 "30-day isolation" rule, not as part of Phase 7.

### Phase 8 — Production hardening (3 weeks)
- Load testing (OOM under pressure, long-running stability)
- Performance profiling + optimization
- Documentation: public API, concepts, migration guide from V1
- **Exit criteria:** 24-hour training run on RunPod with no crashes, deterministic output.

### Phase 9 — Full GPU integration (4–6 weeks)

Phase 2 delivered the GPU path for the adaptive MLP. SNN and LNN are
CPU-only; Path A + Phase 3.5 stability mechanisms (adaptation / basket /
noise / depression / intrinsic reward) have no GPU kernels. This phase
completes the GPU story across every V2 network.

#### State at entry (v2 HEAD `10c26ef2e`)

| Network | GPU forward | GPU backward | GPU plasticity | Wired into `step()` |
|---|---|---|---|---|
| Adaptive MLP | ✓ | ✓ | ✓ (SGD) | ✓ |
| SNN | ✓ (LIF + CSR kernels exist) | — | — | ✗ — kernels exist, not used |
| LNN | — | — | — | — |

`LifGpu` + `CsrGpu` live in `crates/networks/snn/src/{lif,csr}.rs`
behind `cfg(feature = "cuda")`. They're unused by `SnnNetwork::step()`
— the hot path calls `lif_step_cpu` + `csr.i_syn_cpu` unconditionally.

#### Sub-phase plan

- **9a — SNN forward on GPU (opt-in).** Add `use_gpu_forward: bool`
  to `SnnConfig`. When set, `SnnNetwork::step()` uses the existing
  `LifGpu` + `CsrGpu` kernels for the hot loop. Plasticity /
  homeostatic / noise / adaptation / basket stay on CPU. Expected
  win: at 1.8M neurons, 7 steps/sec (CPU stability-on) → 40+ steps/sec.
- **9b — SNN plasticity on GPU.** New CUDA kernels for:
    - R-STDP eligibility trace + weight update
    - Homeostatic synaptic scaling
    - Poisson noise injection (cuRAND)
    - Short-term depression update
  Wire behind the same `use_gpu_forward` flag (rename to `use_gpu`).
- **9c — SNN stability mechanisms on GPU.**
    - Adaptation (AHP + pump) update
    - Basket pool step
    - Intrinsic reward aggregation (reduction kernel)
    - Substrate effects → device-resident cache; refresh every N steps.
- **9d — LNN LTC forward on GPU.** New `LtcGpu` module:
    - Pre-activation matrix-vector: `W_rec·x + W_in·u + b`
    - Tanh kernel
    - Euler step with per-neuron tau
- **9e — LNN BPTT adjoint + SGD on GPU.** Hardest sub-phase:
    - Backward pass through time (matrix-matrix kernels)
    - Gradient clipping on device
    - SGD step on device
    - Respect `tau_safe` floor throughout.
- **9f — Brain-level backend dispatch.**
    - `BrainConfig.backend: Backend::{Cpu, Gpu, Hybrid}`
    - `Brain::new` picks matching impl per config.
    - Checkpoint compat: weights serialise from device → host on
      `save_ensemble`; load inverse.
    - Cross-backend bit-identical: same seed → same weights after
      N training steps, independent of backend (within floating
      tolerance noted in tests).

#### Exit criteria

- 1.8M-neuron SNN trains at **≥50 steps/sec** on RTX 4000 (sm_89)
  with all stability mechanisms active (currently 3.7 steps/sec CPU).
- LNN forward-sequence at 512 hidden × 64-step sequence: GPU ≤ 2×
  CPU on same hardware in the degenerate case (tiny sequence) and
  ≥10× CPU on representative-sequence lengths (≥256 timesteps).
- `Brain::from_json` with `"backend": "gpu"` successfully constructs
  the full ensemble on a CUDA-capable host; adaptive / SNN / LNN
  all train together on device.
- Bench `benches/snn` adds `--gpu` shape preset; reports GPU / CPU
  ratio per shape. Same for a new `benches/lnn`.
- No behaviour regression: `cargo test --workspace` passes
  identically with and without `--features cuda`.

#### Non-scope

- Multi-GPU (single device for Phase 9).
- Mixed precision (FP16 / BF16): opt-in follow-up.
- CUDA graphs for kernel fusion: post-9f optimisation.
- GPU-to-GPU peer transfer: post-9f optimisation.

#### Dev constraint

Hetzner dev server has no CUDA-capable GPU. GPU paths compile under
`--features cuda` but require a pod (RTX 4000 on Hetzner with
Ollama paused, or RunPod RTX 5090) for runtime tests. Each sub-phase
lands as one or more commits that:
1. Compile cleanly under `--features cuda`.
2. Preserve CPU-only behaviour (disable-path bit-identical).
3. Are validated on a pod before merging to `v2` mainline — per-phase
   pod-run log attached to the commit message.

**Total:** ~10 months single-developer to V1 core-feature parity
(Phase 7 dropped from 5 weeks to 2–3 weeks by reusing master's harness
via an adapter shim instead of rebuilding it). Phase 9 adds 4–6 weeks
on top of that for the full-GPU story.

---

### Phase 10 — Multi-GPU (3–4 weeks)

Phase 9 was explicitly single-device. V1 already partitions the 1.8M-neuron
SNN across the pod's 2× RTX 5090s (visible in training logs as
`[MULTI-GPU] Pop p: switching to GPU N / Launching CSR kernel on GPU N`).
V2 must match this before it can replace V1 on the pod.

#### What V1 actually does (ignoring the enum zoo)

V1's `multigpu_config_t` lists five partition strategies (LAYER / NEURON
/ HYBRID / DYNAMIC / AUTO) and three load-balance strategies, but the
hot path uses exactly one shape: **per-population device assignment with
host-mediated spike broadcast and isyn gather.**

Per step, V1:
1. Uploads the global spike vector once to each device (`cudaMemcpyAsync`
   from host to each `d_spike_vec[dev]`).
2. For each population `p`, calls `cudaSetDevice(pop[p].device_id)`,
   launches its CSR SpMV kernel against its device-resident weight
   block, then copies the resulting `isyn` chunk back to the host-side
   accumulator.
3. CPU drives LIF integration, plasticity, homeostatic scaling against
   the gathered `isyn`.

No P2P, no partition rebalance, no NCCL — static manual/round-robin
assignment at network construction time. Phase 10 mirrors this, not the
full enum.

#### Sub-phase plan

- **10a — Device enumeration.** `nimcp-gpu::enumerate_devices() →
  Vec<DeviceInfo>`. Walks ordinals via `CudaContext::new(n)` until
  failure. Tests skip gracefully when fewer than 2 devices are present
  (dev + Hetzner have 0–1; only vast.ai has 2).
- **10b — MultiGpuContext.** `Vec<Arc<CudaContext>>` keyed by ordinal.
  Methods: `new(device_ids)`, `device_count()`, `context_for(ordinal)`,
  `synchronize_all()`. One context per device (one default stream each
  for Phase 10; stream pool deferred).
- **10c — SNN population device assignment.** Network-level
  `SnnConfig.device_assignment: DeviceAssignment::{SingleDevice,
  RoundRobin, Manual, MemoryBalanced}` (`crates/networks/snn/src/
  device.rs`), default `SingleDevice { ordinal: 0 }` = pre-Phase-10
  behaviour. `Manual` carries one ordinal per population (index-aligned
  with `populations`) rather than a field on `PopulationSpec` — keeps
  the dozen existing `PopulationSpec` literals intact and puts a
  topology decision at the topology level. `MemoryBalanced` is greedy
  largest-population-first onto the least-loaded device (neuron count
  as the proxy; synapse memory scales with the destination size).
  `SnnNetwork::new` resolves it to `pop_device: Vec<u32>`, opens a
  `MultiGpuContext` over the distinct ordinals, and builds each
  population's `LifGpu` — and the `CsrGpu` + `RstdpGpu` of every edge
  *into* it — on that population's context.
- **10d — Per-step spike broadcast.** Already satisfied by the 9a
  design: `CsrGpu::i_syn(src_spikes, …)` uploads the edge's source
  population spikes to the edge's own device every step, so each
  device only ever receives the spikes it needs. No separate mirror.
- **10e — Per-device CSR SpMV + host gather.** Also already the 9a
  shape: `i_syn` runs on the edge's context and downloads into the
  host-side `edge.i_syn_scratch`, which the step pipeline sums into the
  destination population. cudarc binds the owning context on every
  stream op, so one thread can drive all devices in sequence.
- **10f — Checkpoint save/load.** Nothing device-specific: host-side
  `edge.csr.weights` is the source of truth (re-uploaded each step)
  and `WeightSnapshot` reads it. A multi-device snapshot restores into
  a single-device network and vice versa — covered by a test, not code.
- **10g — Backend dispatch + V1-parity validation.**
  `Backend::MultiGpu { devices }` (JSON `{"multigpu":{"devices":[0,1]}}`,
  Python `backend="gpu:0,1"`). `apply_backend_to_snn` forces
  `use_gpu_forward` and sets `DeviceAssignment::MemoryBalanced` over
  the devices unless the SNN config pins its own assignment. LNN stays
  on ordinal 0 (non-scope). Same CPU fallback as `Gpu`. **Still open:**
  the soak on vast's 2× RTX 5090 at V1's apportionment (ANN 150K /
  SNN 1.8M / LNN 512) — must match V1's steps/sec within 10 %.

#### Exit criteria

- `enumerate_devices()` returns ≥ 2 entries on vast; test skips when fewer.
- `cargo test --workspace --features cuda` passes identically with
  `MultiGpuContext::new(&[0])` (degenerate single-device) and with
  `MultiGpuContext::new(&[0, 1])` on a multi-device host.
- SNN 1.8M at V1 apportionment: ≥ V1's current throughput ± 10 % on
  the same 2× RTX 5090 hardware. Allowed drift budget is validated
  before merge; wider drift blocks the merge.
- Checkpoint written by multi-GPU V2 loads cleanly into single-GPU V2
  on Hetzner (RTX 4000) and vice versa — ckpt file is device-count-
  independent.
- `nimcp-pybind` surfaces `Backend::MultiGpu` to Python so the V1
  training harness can drive V2 side-by-side on the same pod.

#### Non-scope (defer to Phase 11+)

- GPU-to-GPU P2P transfer (Phase 10 goes host-mediated — matches V1).
- NCCL / collective ops (overkill for 2 devices; revisit at 4+).
- Dynamic rebalancing (static assignment at init; manual Python-side
  retune if V1-parity bench shows imbalance > 15 %).
- Multi-GPU for LNN / adaptive (SNN only — matches V1's actual use;
  LNN tiny, adaptive single-device sufficient).
- Multi-host (distributed) — that is V2's eventual Phase 12.

#### Dev constraint

Hetzner dev has 1 GPU; vast.ai pod has 2. All 10a–f sub-phases must
compile and the single-device degenerate path must pass on Hetzner.
Multi-device integration tests (10e, 10g) only run on vast — attach
pod-run log to the merge commit (same policy as Phase 9).

#### Status (2026-10-26)

10a–10f implemented; 10g dispatch implemented, soak open. Validated on
the vast pod (2× RTX 5090, driver 580, CUDA runtime 12.8 via PyTorch's
bundled libs) with `cargo test --features cuda` for `nimcp-gpu` (20),
`nimcp-snn` (238) and `nimcp-brain` (37 + 17 integration): 312 passed,
0 failed. `multi_device_round_robin_matches_single_device` ran the real
two-device path — spike counts and weights bit-identical to the
single-device network, and the multi-device snapshot restored into it.
Toolchain + build dir were staged on the pod ephemerally (`/dev/shm` is
`noexec`, so binaries went under `/root/v2rust`) and removed afterward.

### Cognition track (C1–C4)

Tracked in commit messages rather than phases: C1+C2 `WorkingMemory` +
`ImaginationEngine` cores in `nimcp-cognition` (`6bd9680b20`), C3
lexicon-grounded `GroundedLanguage::reason` in `nimcp-language`
(`9abc011cc5`). **C4 (2026-10-26)**: `Brain::reason` enriches the C3
conclusion with Z-Ladder recall — top-5 hits with cosine ≥ 0.5 are
blended into the conclusion vector (≤ 0.3 × mean similarity) and lift
confidence by ≤ 0.25 × best similarity of the remaining headroom; each
hit is `access`ed so recall reinforces consolidation. `language_respond`
now supplies that conclusion to the cascade when `reason_in_content` is
on — the flag is no longer dormant. Still unwired into the brain:
working memory and imagination as cascade sources; pybind has no
language surface yet (separate item).

---

## 5. What to port vs rewrite vs drop

### Port (math is the math)
- Adaptive network forward/backward
- STDP / BCM / eligibility trace equations
- SNN LIF dynamics
- Z-Ladder structure + tier transitions
- Prime signature for content addressing
- Fluctuation-driven init formulas

### Rewrite (V1 abstraction was wrong)
- Brain struct → actor hierarchy
- bio_router → typed actor messages
- Signal handler architecture → panic hook + minidump
- Checkpoint format → rkyv (versioned, schema-evolution-safe)
- GPU weight cache → owned by GPU actor, not smeared across brain

### Drop for now — reintroduce later if value is demonstrated
- 60 cognitive modules (start with 0, add on value proof)
- 33 brain regions as first-class objects (actors subsume them)
- Swarm runtime, drone bridges, ROS 2, sim-to-real
- 8 language bindings (Python only until users demand more)
- HNN, FNO (late additions, not load-bearing)
- Brain-native language, emergent alien language
- Phi-3 adapter, Claude teacher tight integration
- 9-layer safety governance (simplify to 1 layer: input validation + output bounds)

---

## 5½. Development targets — dev-first, pod for scale validation

V2 is developed **primarily on the Hetzner dev server** (RTX 4000 SFF Ada,
sm_89, 20 GB VRAM, CUDA 12.0). RunPod (RTX 5090 Blackwell, sm_120, 32 GB,
CUDA 12.8) is used for full-scale validation at phase exits.

This shifts the inner-loop cost from "SSH + deploy + build on remote + test"
to "cargo test" — measured gain in developer throughput.

| Phase | GPU work | Dev? | Pod? |
|---|---|---|---|
| 0 foundation | none | ✓ | — |
| 1 MLP CPU | none | ✓ | — |
| 2 MLP GPU | offload to CUDA | ✓ (small, sm_89) | validate sm_120 |
| 3 SNN (1.8M) | big VRAM | **down-sized here**, full scale on pod | ✓ |
| 4 ensemble | big VRAM | down-sized here | ✓ |
| 5 memory | CPU | ✓ | — |
| 6 introspection | CPU | ✓ | — |
| 7 training | mixed | down-sized here | validate |
| 8 hardening | full scale | — | ✓ |

Ollama currently holds ~19.5 GB of dev server VRAM. Phases 3+ at full scale
will need Ollama paused during the run. For iteration, scale down to
~300 K neurons — fits in 500 MB.

`NIMCP_GPU_ARCH` env var controls build-time arch targeting:

| Value | Use case |
|---|---|
| `dev` (default) | sm_89 only — fastest compile, local dev |
| `pod` | sm_120 only — release build for pod deploy |
| `all` | sm_89 + sm_120 + PTX — CI / universal tarball |

## 6. Risks + mitigations

| Risk | Mitigation |
|---|---|
| Rust CUDA ecosystem less mature than C++ | `cust` is production-ready; worst case, use `cxx` bridge to thin C++ CUDA layer |
| Scope creep (V1's original sin) | 30-day isolation rule, enforced in PR checklist |
| Learning curve if author isn't Rust-fluent | Pair-program with senior Rust eng OR accept 1.5× timeline |
| Python interop perf on large tensors | Measure early; use `numpy-rs` + zero-copy where possible |
| Checkpoint schema evolution | rkyv's versioning + explicit migration functions |
| GPU determinism for replay mode | `cuDnn` deterministic mode + CPU fallback for test harness |
| Feedback loop instability (V1's homeostatic oscillation) | Phase-margin notebook per loop, REQUIRED before merge |

---

## 7. Decision points that need resolution before Phase 0

1. **GPU crate: `cust` vs `cudarc` vs `cxx` bridge?** Recommend `cust` — most mature, Rust-idiomatic, but NVIDIA-only.
2. **Event log backend: `sled` vs custom rkyv-on-append-file?** Recommend custom — sled is KV-oriented, overkill for append-only.
3. **Async runtime: `tokio` vs `async-std` vs `smol`?** Recommend `tokio` — ecosystem mass.
4. **Serialization: `rkyv` vs `bincode` vs `postcard`?** Recommend `rkyv` — zero-copy, schema versioning.
5. **RNG: `rand` vs `rand_chacha`?** Use `rand_chacha` everywhere — deterministic seeded output.
6. **Linear algebra: `ndarray` vs `nalgebra` vs in-house?** `ndarray` for n-dim tensor, `nalgebra` for linear algebra kernels, custom for hot paths.

---

## 8. What success looks like

At end of Phase 8:
- Bit-for-bit reproducible runs
- 24+ hour training with zero crashes
- Matches V1 throughput on 1.8M-neuron benchmark
- Clean Python API, fully documented
- No `unsafe` outside FFI boundaries
- Every feedback loop has a phase-margin analysis
- Toy-brain regressions run in CI in <30 seconds
- Migration guide for V1 users

At that point V2 is a viable replacement for V1's core. Ecosystem features (swarm, drones, edge, brain-native language, etc.) can be added in a second year if value is demonstrated.

---

## 9. First commit

```bash
cd nimcp
git checkout v2
# Remove all V1 content (optional — or keep as reference under v1/)
# Create cargo workspace
cargo new --lib crates/core
cargo new --lib crates/eventlog
# ... etc per §3.2
# Create python/ with maturin config
# Create Cargo.toml workspace root
```

Author: nimcp maintainers
Last updated: 2026-04-21
