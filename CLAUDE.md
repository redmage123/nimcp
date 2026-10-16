# NIMCP Project Reference

**Version**: 0.9.0-beta (tag 2.7.0 — glial + substrate + SNN lightweight campaign)
**Last Updated**: 2026-05-16
**Path**: `/home/bbrelin/nimcp`

**Recent release highlights** (see `CHANGELOG.md` for the full entry):
- **SNN language-bridge plasticity wired into training loop
  (2026-05-14/15, unreleased)** — `brain_learn_vector` now drives
  bridge STDP (via `apply_stdp` in `brain_tick_lang_bridge_spike_routing`),
  trigram learning (`grounded_language_learn_text_bigrams` at `lr=0.02`),
  and comprehend-STDP (default flipped TRUE). The bridge is opted into
  SNN spike routing inside `attach_lang_adapters_to_substrate`, and
  `connect_neuromod` is called on the same path so the DA three-factor
  gate is live on resume — pre-fix the counters sat flat through entire
  runs. New `lang_status.plasticity.next_token_cold_start_skips`
  telemetry discriminates cold-start ramp from genuine stall.
  Greedy `produce` confidence-floor break now honors `min_produce_words`
  (4-of-4 stop conditions consistent). Commits `0d993d656`, `ac47802b0`,
  `6364a1468`, `378bc6109`. Detail: `docs/claude/modules/grounded-language.md`.
- **Wedge mitigation + watchdog (2026-05-15)** — `_cmd_learn_language` RPC
  defaults to a no-op via `NIMCP_DISABLE_LEARN_LANGUAGE=1` (re-enable
  with `"0"` in supervisord env). Stage-2 wedge root-caused via
  in-process `faulthandler.dump_traceback_later` (120s, appends to
  `/var/log/athena/brain_traceback.log`) — all 4 RPC workers blocked on
  the shared lock inside `grounded_language_learn_from_text`'s
  O(N² × B²) cross-bind loop. Trainer hardened: `MiniBatchTrainer.flush`
  surfaces silent learning failures, crashes after 50 consecutive
  flush failures, and `brain_client._send` no longer counts
  `FileNotFoundError` against `MAX_RETRIES` (`DAEMON_WAIT_TIMEOUT`
  300s → 1800s). Training-loop language learning is unaffected — the
  kill switch only no-ops the out-of-band `brain.learn_language(text)`
  RPC, not the in-C `learn_from_text` calls from `brain_learn_vector`.
  Commits `6dc8f6339`, `85d18d45c`, `d1d404cd2`.
- **Conductance-based SNN synapses (CB migration, unreleased)** — runtime
  flag `conductance_enabled` (default OFF) routes synaptic input through
  per-neuron `g_exc`/`g_inh` arrays with reversal-potential driving force,
  saturating naturally at E_exc/E_inh and preventing the dead↔runaway
  oscillation. New `snn_membrane.h` helpers + `snn_rescale_weights_for_conductance()`
  admin command. CB mode forces CPU fallback (GPU port deferred).
  Design: `docs/claude/cb-phase0-design.md`. Tests: 46 across unit/integration/regression/e2e.
- Glial networks now created + wired into the forward-pass hot path
  (astrocyte modulation, oligodendrocyte myelin boost, microglia pruning).
- Neural substrate + thalamic router created at brain init and attached to
  every SNN/LNN/cortex-CNN — Phase 1-4 adapters are no longer dormant.
- SNN cognitive bridges (attention, mirror-neurons, emotion, working-memory)
  migrated off dense pops to lightweight CSR.
- `synapse_id` widened uint32 → uint64 (bijective at 2M neuron scale).
- SIGPIPE `SIG_IGN` installed in `nimcp_init_internal` — daemon no longer
  silent-dies on closed socket peers.
- Thalamic router queue 1000 → 16384 + new backpressure-query API.
- RTX 5090 (Blackwell, `compute_120`) enabled when CUDA ≥ 12.8.

> **Documentation is modularized.** See `docs/claude/` for detailed documentation.
> **Master Index**: See `docs/INDEX.md` for complete documentation navigation.

---

## Quick Reference

| Topic | File |
|-------|------|
| **External API Guide** | [EXTERNAL_API_GUIDE.md](docs/EXTERNAL_API_GUIDE.md) |
| Project Vision & Motivation | [00-overview.md](docs/claude/00-overview.md) |
| Build & Test Commands | [01-build-test.md](docs/claude/01-build-test.md) |
| Coding Standards & Protocols | [02-coding-standards.md](docs/claude/02-coding-standards.md) |
| Key API Patterns | [03-api-patterns.md](docs/claude/03-api-patterns.md) |
| File Organization | [04-file-organization.md](docs/claude/04-file-organization.md) |
| Resource Optimization | [05-resource-optimization.md](docs/claude/05-resource-optimization.md) |
| Error Codes | [06-error-codes.md](docs/claude/06-error-codes.md) |
| Common Issues | [07-common-issues.md](docs/claude/07-common-issues.md) |

## Architecture At A Glance

- **2M neuron brain** with multi-layer diamond architecture (3/5/7 layers by size)
- **6 network types**: Main neural net, LNN (liquid/temporal + Hamiltonian), SNN (spiking + FNO population), CNN (visual/audio), FNO (Fourier spectral), HNN (energy-conserving)
- **60+ cognitive modules**: introspection, ethics, theory of mind, imagination, reasoning, emotions, etc. + 13 cognitive enhancements (inner speech, episodic replay, world model, attention, working memory, analogical transfer, multi-timescale memory, emotional learning, contrastive self-learning, self-curriculum, dynamic arch search, social interaction, emergent language)
- **33+ brain regions**: prefrontal, occipital, parietal, hippocampus, cerebellum, basal ganglia, etc.
- **Full biological plasticity**: STDP, BCM, eligibility traces, dendritic, homeostatic, 6 neuromodulators
- **Multimodal perception**: visual cortex, audio cortex, speech cortex, somatosensory
- **Edge/Robot platform**: sensor hub (12 types), safety watchdog, motor output, 4 drone bridges (MAVLink/DJI/MSP/Parrot), ROS 2 bridge, sim-to-real bridge, URDF embodiment, sensorimotor loop
- **GPU-accelerated** (CUDA kernels for forward/backward/plasticity), ~15-16.5 GB VRAM for 2M neurons
- **240 Python API methods**, 8 language bindings synced, immersive developmental training curriculum
- **~2,600 source files**, 2,456 headers, 800+ fields on brain_struct
- **Swarm runtime**: master/edge federation, UDP discovery, Byzantine tolerance, gossip learning
- **Brain-native language**: learned vocabulary, autoregressive decoding, emergent alien language mode
- **9-layer safety**: LGSS governance, non-removable ethics, tamper-resistant audit, formal verification hooks

## Trigger-Action Rules

### Routing Table
| Trigger | Action |
|---------|--------|
| Code change in src/ | Run `make nimcp -j4` to verify build |
| Change to neuron_t or synapse layout | Rebuild + reinstall Python .so |
| New test file created | Run the specific test to verify it passes |
| Bug report or error | Use systematic debugging: reproduce -> isolate -> fix -> verify |
| Memory/learning worth preserving | Write to MEMORY.md BEFORE responding |
| Complex multi-file change | Create task list with TaskCreate |
| Unclear requirements | Use AskUserQuestion, don't assume |
| Session ending or context getting long | Update journal.md with handoff notes |

### Enforcement Rules
1. **Write-Before-Speak**: If something is worth remembering, write it to a file BEFORE saying "I'll remember"
2. **No Empty Promises**: Never say "I'll keep that in mind" without an actual file write
3. **Verify Before Commit**: Always run `make nimcp -j4` before any git commit
4. **Test What You Write**: New test files must be built and run before committing
5. **Immediate Memory Updates**: Don't defer MEMORY.md updates

### Self-Check Before Every Response
- Did the user share something that should be persisted? -> Write it now
- Am I about to make a promise to remember? -> Write first, then respond
- Is this a pattern/lesson that will help future sessions? -> Update MEMORY.md

## Module Documentation

| Module | File |
|--------|------|
| Hemispheric Brain | [modules/hemispheric-brain.md](docs/claude/modules/hemispheric-brain.md) |
| Pink Noise Bridges | [modules/pink-noise.md](docs/claude/modules/pink-noise.md) |
| Brain Immune System | [modules/brain-immune.md](docs/claude/modules/brain-immune.md) |
| Training-Immune Integration | [modules/training-immune.md](docs/claude/modules/training-immune.md) |
| Cross-Bridge Integration | [modules/cross-bridge.md](docs/claude/modules/cross-bridge.md) |
| Neural Substrate + Thalamic Router | [modules/substrate.md](docs/claude/modules/substrate.md) |
| Glial Cell Infrastructure | [modules/glial.md](docs/claude/modules/glial.md) |
| Liquid Neural Networks | [modules/lnn.md](docs/claude/modules/lnn.md) |
| Bio-Async Integration | [modules/bio-async.md](docs/claude/modules/bio-async.md) |
| Introspection | [modules/introspection.md](docs/claude/modules/introspection.md) |
| Positional Encoding | [modules/positional-encoding.md](docs/claude/modules/positional-encoding.md) |
| Tensor Integration | [modules/tensor.md](docs/claude/modules/tensor.md) |
| Metabolic Modulation | [modules/metabolic-modulation.md](docs/claude/modules/metabolic-modulation.md) |
| Brain Regions | [modules/brain-regions-roadmap.md](docs/claude/modules/brain-regions-roadmap.md) |
| Recursive Cognition | [modules/recursive-cognition.md](docs/claude/modules/recursive-cognition.md) |
| Imagination Engine | [modules/imagination-engine.md](docs/claude/modules/imagination-engine.md) |
| Grounded Language | [modules/grounded-language.md](docs/claude/modules/grounded-language.md) |
| Genius Profiles | [modules/genius-profiles.md](docs/claude/modules/genius-profiles.md) |
| Lock Ordering | [modules/lock-ordering.md](docs/claude/modules/lock-ordering.md) |
| Hamiltonian Neural Networks | [modules/hnn.md](docs/claude/modules/hnn.md) |
| Fourier Neural Operators | [modules/fno.md](docs/claude/modules/fno.md) |
| Swarm Runtime | [modules/swarm-runtime.md](docs/claude/modules/swarm-runtime.md) |
| Edge Platform | [modules/edge-platform.md](docs/claude/modules/edge-platform.md) |

---

## Safety Hardening

### Ethics Module — Non-Removable Dependency
The ethics module is **always created** during brain initialization, regardless of `enable_ethics` configuration. The `enable_ethics` flag controls whether ethics evaluation **blocks** actions, but the module itself is always present for audit and monitoring. Both `brain_decide()` and `brain_learn_vector()` contain mandatory ethics gates that log critical warnings if the ethics module is missing. These gates cannot be disabled via configuration.

### Tamper-Resistant Safety Audit Log
The `nimcp_safety_audit_*` API (`include/security/nimcp_audit_log.h`) provides always-on, append-only audit logging:
- **100,000-entry in-memory ring buffer** + append-only disk log (`/var/log/nimcp/nimcp_safety_audit.log`)
- **Monotonic sequence numbers** — gaps indicate deleted entries (tampering)
- **CRC32 checksums** per entry — mismatches indicate modified entries
- **Cannot be disabled** via any config flag — always active once `nimcp_init()` is called
- **Thread-safe** via internal mutex; best-effort disk writes (never blocks on I/O failure)
- Events logged: inference (every 1000th), learning (every 1000th), ethics violations (all), watchdog triggers (all), swarm events, checkpoint operations
- Use `nimcp_safety_audit_verify_integrity()` to detect tampering

### LGSS (Layered Governance Safety System) — Non-Removable
LGSS is **always created** during brain initialization, regardless of `enable_lgss` configuration. The `enable_lgss` flag controls rule strictness, not existence. LGSS is wired into **all** brain pipelines:

| Pipeline Point | Location | What It Does |
|----------------|----------|--------------|
| **Input Validation** | `brain_decide()` top | Checks for NaN/Inf/adversarial features; blocks with null decision |
| **Action Interceptor** | `brain_decide()` after ethics | Evaluates decision against safety KB; blocks or escalates |
| **Motor Gate** | `brain_decide()` after watchdog | Validates output vector magnitude/intent; zeros unsafe outputs |
| **Training Guard** | `brain_learn_vector()` after ethics | Validates training data; skips poisoned learning steps |
| **Reward Alignment** | `brain_apply_reward_learning()` | Validates reward signals; blocks reward hacking attempts |

**LGSS Audit Events**: `LGSS_ACTION_BLOCKED`, `LGSS_INPUT_REJECTED`, `LGSS_TRAINING_BLOCKED`, `LGSS_MOTOR_BLOCKED`, `LGSS_REWARD_BLOCKED` — all logged to the tamper-resistant safety audit log.

**Key API**: `lgss_evaluate(brain->lgss, &context, &result)` — uses `safety_action_context_t` with string/numeric fields and `SAFETY_DOMAIN_*` hints. Result `SAFETY_ACTION_DENY` triggers blocking.

### Files Modified for Safety Hardening
| File | Change |
|------|--------|
| `src/core/brain/nimcp_brain_part_core.c` | Mandatory ethics gate + LGSS input/action/motor gates + audit logging in `brain_decide()` |
| `src/core/brain/learning/nimcp_brain_learning.c` | Mandatory ethics gate + LGSS training guard + reward alignment + audit logging |
| `src/core/brain/factory/init/nimcp_brain_init_safety_verify.c` | LGSS non-removable: always created regardless of `enable_lgss` |
| `src/core/brain/cognitive/nimcp_brain_cognitive.c` | Ethics init no longer gated by `enable_ethics` |
| `src/core/brain/factory/init/nimcp_brain_init_monitoring.c` | Ethics init no longer gated by `enable_ethics` |
| `include/security/nimcp_audit_log.h` | Safety audit API + LGSS event types |
| `src/security/nimcp_audit_log.c` | Safety audit implementation |
| `src/api/nimcp_part_lifecycle.c` | Audit log init in `nimcp_init_internal()` |

---

## CRITICAL WARNING - PAST VIOLATIONS

**Claude has repeatedly ignored the MANDATORY Test Writing Protocol.**

**Before Claude writes ANY test code, the user MUST require Claude to:**
1. Show the complete header file content that was read
2. List the exact function signatures to be used
3. Only then approve test code writing

**See [02-coding-standards.md](docs/claude/02-coding-standards.md) for full protocol.**

---

## Essential Commands

```bash
# Build library
cd /home/bbrelin/nimcp/build && cmake .. && make nimcp -j4

# Build + install Python bindings (MUST do after neuron_t/synapse changes)
make nimcp_python -j4 && cp build/lib/python/nimcp.so ~/.local/lib/python3.12/site-packages/nimcp.cpython-312-x86_64-linux-gnu.so

# Training
cd /home/bbrelin/nimcp && python3 scripts/immerse_athena.py
# With resume: python3 scripts/immerse_athena.py --resume

# Monitor training (persistent cron)
tail -30 monitoring.log

# Git
git add -A && git commit --no-verify -m "message" && git push
```

## Critical GOTCHAs

### Tensor API
- `nimcp_tensor_sum()` returns `nimcp_tensor_t*`, not scalar
- `nimcp_tensor_create(dims, rank, dtype)` requires 3 args (rank, not ndims)
- `op_div` uses epsilon clamping (1e-7), does NOT log warnings

### Mutex API (use thread layer, not platform layer)
- `nimcp_mutex_create(attr)` - allocate and init, returns `nimcp_mutex_t*`
- `nimcp_mutex_init(mutex, attr)` - init existing struct
- `nimcp_mutex_free()` = destroy + free (correct for heap-allocated mutexes, NOT a bug)
- **Deadlock prevention**: Never call public mutex-locking functions from within locked code

### Neural Architecture
- **EMBEDDED_CAPACITY**: 320 synapses inline per neuron (not 128 or 256)
- **Multi-layer diamond**: Small (<5K)=3 layers, Medium (5K-100K)=5 layers, Large (100K+)=7 layers
- **Brain init modes**: FULL (all 80+ subsystems), FAST (6 of 27 waves, ~14s), MINIMAL (core only)
- **Hot/cold neuron split**: Frequently-accessed fields in `neuron_t`, cold data in `neuron_cold_data_t`

### LNN Gradient System
- Per-layer tensors (grad_W_rec, grad_tau_base, grad_b_in) are the REAL gradients
- ctx->grad_params is legacy/dead - do NOT read from it for norms or clipping
- Gradient clip to 1.0 norm AFTER adjoint computation, BEFORE get_gradients
- Per-step clamping [-1e4, 1e4] prevents accumulation explosion
- tau_safe floor 0.01 prevents 1/tau^2 explosion

### Return Value Conventions
- FEP bridges return `0`/`-1` (not NIMCP_OK/NIMCP_ERROR_*)
- Metabolic modulation: `metabolic_compute_effects()` returns `0`/`-1`
- Standard NIMCP functions return `nimcp_error_t` codes

### Type Distinctions
- `nimcp_brain_t` (public handle) vs `brain_t` (internal pointer) - use `handle->internal_brain`
- `brain_immune_get_antigen_copy()` preferred over `brain_immune_get_antigen()` (dangling pointer)
- `copy_decision_deep()` for cache, `brain_free_decision()` for cleanup
- `nimcp_bio_promise_complete(promise, result)` takes 2 args, NOT 3
- `neuromodulator_system_t` is already a pointer typedef - don't double-pointer it

### Files That Must NEVER Have Raw NIMCP_THROW_TO_IMMUNE
1. `src/utils/exception/` - ALL files (infinite recursion)
2. `src/utils/memory/nimcp_memory.c` - Use `MEMORY_SAFE_THROW()`
3. `src/utils/memory/nimcp_unified_memory.c` - Use `UMM_SAFE_THROW()`
4. `src/security/nimcp_constant_time.c` - Gate with `nimcp_exception_system_is_initialized()`

### Memory Implementation - Raw malloc Only
Files `nimcp_memory.c`, `nimcp_unified_memory.c`, `nimcp_constant_time.c` MUST use raw `malloc/calloc/free/realloc`.

### GPU Rules
- `.cu` files must use `nimcp_malloc/nimcp_free`, not raw malloc
- NVCC doesn't support C11 `_Atomic` - use `volatile` + GCC `__atomic_*` builtins
- GPU stream pool: `nimcp_gpu_get_pool_stream(ctx)` round-robin from 8 streams

### Python .so Installation
After ANY change to `neuron_t`, `sparse_synapse_storage_t`, or brain struct layout:
```bash
make nimcp_python -j4
cp build/lib/python/nimcp.so ~/.local/lib/python3.12/site-packages/nimcp.cpython-312-x86_64-linux-gnu.so
```
Failure to do this causes SIGSEGV from stale field offsets.

### Guard Clause Pattern
Both braces AND return required. `NIMCP_THROW_TO_IMMUNE` alone doesn't halt execution.

### setjmp/longjmp
Variables modified between `setjmp` and `longjmp` MUST be `volatile`.
