# Recurrent + Predictive Language Architecture

**Status**: design — implementation in slices
**Author**: 2026-05-18 thread
**Replaces**: `communication_cascade.c`'s sequential 15-stage pipeline (Levelt 1989 style)
**Motivation**: bridge-only training produces word-salad; cascade alone is sequential pipeline; real cortex is continuously recurrent with predictive coding. To achieve biologically-faithful coherent generation, replace the cascade with a recurrent dynamical system matching the published consensus models of language production (Dell 1986, Hickok & Poeppel 2007, Hagoort MUC 2005, Friston FEP).

## Target architecture

```
   ┌─────── prompt ───────┐
   │                      │
   ▼                      │
 [Wernicke] ◄───── arcuate fasciculus ─────► [Broca]
   │  ▲                                       ▲  │
   │  │ (predictions)                         │  │
   │  ▼                                       │  ▼
 [Temporal] ── lexical column ───► [Bridge] ──┴──► [Phonological]
   │            (competing                              │
   │             candidates)                            ▼
   │                                                [Motor]
   │           ┌─[OFC]──drive ─┐                      │
   ▼           ▼               │                      ▼
 [Working memory buffer] ◄─[PFC]─goal ◄──── self-feedback ──┐
   │                            ▲                            │
   ▼                            │                            │
 [Hippocampus]─episodic ────────┘                            │
   ▲                                                          │
   │                       ┌─[Cerebellum]─prediction ◄────────┘
   └─[Thalamus]─gating ────┘
```

All populations continuously active. Single iteration is a snapshot; the system iterates until activation patterns settle (or hard timeout).

## Computational model

Replace `communication_cascade_run` with `recurrent_language_run`:

```c
int recurrent_language_run(
    brain_t brain,
    const char* prompt,                  /* NULL = spontaneous */
    uint32_t max_iterations,             /* default 8 */
    float convergence_eps,               /* default 0.01 */
    production_cascade_state_t* state    /* same struct, reused */);
```

Each iteration:

```
1. Read-only snapshot of all population activations into local buffer.
2. For each population, compute next activation from snapshot + own state.
   - Drive: motivation_id, intensity from OFC state
   - Goal: speech_act from PFC + drive + prompt
   - Episodic: hippocampal retrieval given semantic_vector
   - Listener: sensory_integration adjustments
   - Content: blend of drive+goal+episodic+listener
   - Lexical: competing candidates (lateral inhibition)
   - Syntactic: Broca's CYK + grammar agreement
   - Self-comp: Wernicke re-parse own output
   - Phonological: syllable + prosody
   - Motor: articulation
   - Self-feedback: error monitoring
3. Compute predictions for each population (top-down):
   - Each population produces (current_estimate, prediction_for_inputs)
4. Compute prediction errors = current - prediction_from_downstream.
5. Apply FEP-driven plasticity weighted by prediction error.
6. Lateral updates: Wernicke ↔ Broca via arcuate fasciculus.
7. Thalamic gating: which populations get attention bandwidth.
8. Cerebellar correction: predict next-step pattern, drive online correction.
9. Convergence check: if max |activation_delta| < eps, exit; else iterate.
```

Final iteration's working-memory buffer is the utterance.

## Slices

Each slice is independently committable and brings observable behavior change.

### Slice 1: Iterative cascade loop with convergence

**What**: wrap the existing sequential cascade in an outer iteration loop. Same 15 stages run multiple times. After each pass, check if `self_match` and `utterance` have stabilized. Exit on convergence or max iterations.

**Why first**: scaffolds the iteration model without disturbing per-stage semantics. Lowest blast radius. Establishes that "settling dynamics" is the right metaphor before changing internal stages.

**API**:
```c
/* New function. Calls communication_cascade_run() up to max_iters times,
 * checking convergence between iterations. */
int communication_cascade_run_recurrent(
    brain_t brain, const char* prompt,
    uint32_t max_iters,        /* default 8 */
    float utterance_change_eps,/* default 0.0 = exact match */
    float self_match_eps,      /* default 0.01 */
    production_cascade_state_t* out_state);
```

**Wiring**: new flag `cascade_recurrent_enabled` defaults OFF; respond_via_cascade routes through recurrent path when on.

**Files**: `nimcp_communication_cascade.c`, `nimcp.h`, `nimcp_part_core.c`, `nimcp_python.c`, `brain_daemon.py`, `brain_client.py`.

### Slice 2: Bidirectional Wernicke ↔ Broca per iteration

**What**: between iterations, feed Broca's emerging utterance back into Wernicke's parser. Wernicke produces a refined parse that Broca uses on the next iteration. Implements the arcuate fasciculus as a real bidirectional information channel, not just "Wernicke runs once at start, Broca runs once at end".

**Why second**: this is the single most important biological-fidelity change. The arcuate fasciculus is the load-bearing white-matter tract for language; making it bidirectional in code matches the anatomy.

### Slice 3: FEP prediction-error hooks ✓ SHIPPED

**What**: each major cascade stage produces a trivial prediction of what its inputs SHOULD look like given the cognitive state, observes the actual input, and records the L2 norm of (actual - predicted) as a per-stage prediction-error scalar on `production_cascade_state_t`. The recurrent loop tracks the per-iteration `pe_total` trajectory and bumps `fep_precision` on high-surprise iterations so `cascade_stage_self_train` applies more plasticity to surprising inputs (Friston FEP: precision-weighted prediction error drives learning).

**Why third**: predictive coding is the mathematical heart of biological inference. Once the iteration scaffold exists (Slice 1) and the arcuate-feedback loop carries meaningful inter-iteration signal (Slice 2), predictions are a natural addition.

**Hooks landed** (all default-zero so the existing single-pass cascade behavior is bit-identical to pre-Slice-3):

| Stage | Predictor | PE scalar |
|-------|-----------|-----------|
| Content | Pre-arcuate blend of drives/episodic/listener/goal (steps 1..5) | `pe_content_norm` — magnitude of the arcuate-feedback correction applied this iter. Drops as the recurrent loop converges. |
| Lexical | "The bridge knows what it's saying" — the bridge's own `prod.semantic_vector` should equal `content_intent`. | `pe_lexical_norm` — `‖content_intent − prod.semantic_vector‖ / sqrt(dim)`. Fallback `1 − fluency` when the bridge doesn't fill `semantic_vector`. |
| Syntactic | "Broca accepts the bridge's word sequence." | `pe_syntactic_norm` — `1 − syntactic_validity`. Stays 0 when the stage was skipped (no observation → no surprise). |
| Self-comp | "My own utterance comprehends back to the original intent." | `pe_self_comp_norm` — `1 − self_match`. Full surprise (`1.0`) when even parsing my own output failed. |

`pe_total = sum(finite per-stage norms)` is the iteration's surprise budget. The recurrent loop pushes `pe_total` into a per-iter trace + summary scalars on the brain; consumers read them via `nimcp_brain_get_cascade_fep_metrics()` / `Brain.get_cascade_fep_metrics()` / RPC `get_cascade_fep_metrics`.

**FEP precision update** (recurrent loop only): `fep_precision_{iter+1} = clamp(1.0 + 0.5 × pe_total_iter, [0.5, 4.0])`. Stage_self_train multiplies its `lr_scale` by `precision` before invoking the bridge plasticity API, so a surprising iteration produces stronger learning on the next iter.

**Surface added** (all opt-in / append-only):

- `production_cascade_state_t` — 6 fields: `pe_content_norm`, `pe_lexical_norm`, `pe_syntactic_norm`, `pe_self_comp_norm`, `pe_total`, `fep_iteration`, `fep_precision`.
- `nimcp_cascade_diag_full_t` — same 6 fields mirrored.
- `brain_struct` — `fep_iterations_run`, `fep_pe_trace[64]`, `fep_pe_initial/terminal/min/max/mean`, `fep_pe_decay_rate`, `fep_converged`.
- New type `nimcp_cascade_fep_metrics_t` in `nimcp_communication_cascade.h`.
- C API: `nimcp_brain_get_cascade_fep_metrics()` in `nimcp.h`.
- Python: `Brain.get_cascade_fep_metrics()` returning dict; `Brain.produce_cascade()` adds 7 PE/FEP fields to its diag dict.
- Daemon RPC: `get_cascade_fep_metrics`.
- BrainProxy: `get_cascade_fep_metrics()`.

**What's NOT here** (deferred to walkthrough / later slices):

1. Predictors are still TRIVIAL (identity / fluency / validity / self_match). A real population-dynamic predictor at each stage — e.g. Broca's syntactic processor learning a model of what bridge-output it expects given the content_intent — lands in a later slice.
2. The PE signal is read-only on the bridge plasticity path; we precision-scale `lr_scale`, but we don't yet route the per-stage PE into stage-specific plasticity (e.g. Wernicke's parse-error → Wernicke STDP).
3. The `pe_content_norm` predictor records the arcuate-feedback magnitude — useful, but not the textbook "predict downstream input" semantics. A proper Content predictor would model what the lexical stage will produce given the current intent.

### Slice 4: Lateral inhibition in lexical selection — SHIPPED

**What**: a new `snn_language_bridge_decode_with_lateral_inhibition()` wraps the standard cosine-`decode_spikes` argmax with a recurrent competition. Top-K candidates excite themselves AND inhibit each other across `T` micro-steps:

```
new_a[k] = sigmoid(a[k] * gain_self - sum_{j != k} a[j] * gain_inhibit)
```

After settling, the winner is re-ranked by post-competition activation rather than the one-shot cosine score. Implements the cohort model (Marslen-Wilson 1987) and interactive activation (McClelland 1981).

**Why fourth**: lexical competition is well-documented in psycholinguistics (cohort model, semantic interference). The iteration scaffold from Slices 1-2 ensures the bridge has the right granularity for this kind of within-step settling.

**Wiring**: default OFF via a new `enable_lateral_inhibition` bool on `snn_lang_config_t` (append-only — static_assert bumped 172 -> 188 bytes). When flipped on, the bridge's `produce()` loop transparently swaps `decode_spikes` for the lateral path on every per-word decode (and the same swap inside `produce_beam_search`). Hyperparameters runtime-tunable: `gain_self` (default 1.5), `gain_inhibit` (default 0.026 ~= 0.8/(K-1) for K=32), `micro_steps` (default 20).

**Stability**: activations are bounded to [0, 1] via sigmoid each step. NaN/Inf would force the function to fall back to cosine top-K with a one-shot warning. With the default gains, the leader saturates near ~0.62 and subordinates decay toward 0 within ~10-15 micro-steps.

**Cost**: K * T multiplies + K sigmoids per word selected. At K=32, T=20 that's ~640 ops + 20 sigmoid evals per emitted word — under 5us at -O2 on x86_64.

**Surface added — all opt-in, no legacy behavior change**:
```
C:      snn_language_bridge_decode_with_lateral_inhibition
        snn_language_bridge_set_lateral_inhibition_enabled
        snn_language_bridge_get_lateral_inhibition_enabled
        snn_language_bridge_set_lateral_inhibition_params
        snn_language_bridge_get_lateral_inhibition_params
API:    nimcp_brain_set_lateral_inhibition_enabled
        nimcp_brain_get_lateral_inhibition_enabled
        nimcp_brain_set_lateral_inhibition_params
        nimcp_brain_get_lateral_inhibition_params
Python: Brain.set_lateral_inhibition_enabled(bool)
        Brain.get_lateral_inhibition_enabled() -> bool
        Brain.set_lateral_inhibition_params(gain_self, gain_inhibit, micro_steps)
        Brain.get_lateral_inhibition_params() -> {gain_self, gain_inhibit, micro_steps}
Daemon: _cmd_set_lateral_inhibition_enabled
        _cmd_get_lateral_inhibition_enabled
        _cmd_set_lateral_inhibition_params
        _cmd_get_lateral_inhibition_params
Client: BrainProxy.set_lateral_inhibition_enabled
        BrainProxy.get_lateral_inhibition_enabled
        BrainProxy.set_lateral_inhibition_params
        BrainProxy.get_lateral_inhibition_params
```

### Slice 5: Phonological-loop working memory buffer (shipped 2026-05-18)

**What**: replace one-shot utterance construction with a buffer that's incrementally refined each iteration. Baddeley's phonological loop model.

**Implementation**:
- `phonological_loop` lives on the brain struct (`loop_*` fields, append-only). Holds: surface buffer + per-word phonemic traces + the words themselves + decay rate + max-word cap + last-refresh timestamp + enabled flag + per-loop mutex.
- Allocated once in `nimcp_brain_factory_init_broca_subsystem` (sibling of `speech_repair`), freed in the matching destroy. Initial capacity 256 bytes buffer + 16 trace slots; grows on demand via realloc, non-fatal on alloc failure.
- Default OFF — when `loop_enabled=false`, the recurrent cascade is byte-identical to Slice 1+2.

**Recurrent-loop integration**:
1. `communication_cascade_run_recurrent` calls `phonological_loop_clear(brain)` on entry (single-recurrent-run buffer; not persisted across calls).
2. At the start of each iteration after the first, `phonological_loop_decay(brain)` multiplies every trace by `(1 - decay_rate)` and evicts traces below 0.05.
3. Inside `cascade_stage_lexical`, AFTER the bridge produces, `phonological_loop_merge_words(brain, state->utterance)` tokenizes the produced text; existing words refresh trace to 1.0, new words append (capped at `loop_max_words=16`).
4. `cascade_stage_lexical` then SWAPS `state->utterance` to the buffer's surface form (`phonological_loop_render_active`, threshold 0.3). All downstream stages — `cascade_stage_syntactic`, `cascade_stage_self_comprehension`, speech-repair, prosody — see the buffered version.

**Public surface (all default-OFF)**:
- C API: `nimcp_brain_set_phonological_loop_enabled` / `_set_phonological_loop_decay` / `_clear_phonological_loop` / `_get_phonological_loop_state` (buffer + trace_count) / `_get_phonological_loop_diag` (full struct).
- Python: `Brain.set_phonological_loop_enabled` / `set_phonological_loop_decay` / `clear_phonological_loop` / `get_phonological_loop_state` / `get_phonological_loop_diag`.
- Daemon RPC: `set_phonological_loop_enabled` / `set_phonological_loop_decay` / `clear_phonological_loop` / `get_phonological_loop_state` / `get_phonological_loop_diag`.
- Client: `BrainProxy.set_phonological_loop_enabled` / `set_phonological_loop_decay` / `clear_phonological_loop` / `get_phonological_loop_state` / `get_phonological_loop_diag`.

**Files**:
- `include/core/brain/nimcp_brain_internal.h` (loop_* fields appended).
- `include/language/nimcp_phonological_loop.h` + `src/language/nimcp_phonological_loop.c` (new; all loop logic).
- `include/nimcp.h` + `src/api/nimcp_part_core.c` (public C API).
- `src/core/brain/factory/init/nimcp_brain_init_broca.c` (init / destroy wiring).
- `src/language/nimcp_communication_cascade.c` (recurrent loop + lexical stage swap).
- `src/bindings/python/nimcp_python.c`, `scripts/brain_daemon.py`, `scripts/brain_client.py`.

### Slice 6: Thalamic gating

**What**: thalamus modulates which populations get processing bandwidth. Already wired into substrate (per `substrate-plan` memory) but not gating cascade flow.

**Status**: implemented 2026-05-18, default OFF.

**Mechanism**: 15-slot per-stage gate weight array on `brain_struct` (`thalamic_gate_weights[15]`) re-derived from arousal (NE neuromodulator) + attention (ACh neuromodulator) at the top of every `communication_cascade_run` call when `thalamic_gate_enabled == true`. The existing `thalamic_router`'s imagination_attention (`thalamic_router_get_imagination_attention`) attenuates the `self_train` gate so dreamy outputs aren't reinforced as deliberate productions. Each stage's gate is the bit-position index into `cascade_stage_mask_t`.

**Gate-weight derivation rule** (per stage):
- baseline = 0.5 * (NE + ACh)
- motor / prosody / self_feedback / speech_repair: 0.7 * NE + 0.3 * ACh (arousal-heavy)
- content / episodic / lexical / goal: 0.4 * NE + 0.6 * ACh (attention-heavy)
- self_train: (0.3 * NE + 0.7 * ACh) * (1 - 0.5 * imag_attn) (attention-heavy + dream-attenuated)
- everyone else: baseline
- All clamped to [0, 1]

**Application points** (only where scaling is meaningful):
- `stage_content`: `content_intent[]` multiplied by `gate_content` before confidence
- `stage_lexical`: `state->fluency *= gate_lexical`
- `stage_prosody`: `intensity_db[]` shifted by `20*log10(gate_prosody)` dB (perceptual)
- `stage_self_train`: effective `lr_scale *= gate_self_train`

Other stages (wernicke / drive / goal / listener / episodic / syntactic / self_comp / phonological / motor / self_feedback / speech_repair) produce structured discrete artifacts whose magnitudes are dimensionless; their gates are computed + surfaced via diagnostics but not multiplied into any output. To fully suppress a structured-output stage, use the existing `cascade_stage_mask_t` bit.

**Manual overrides**: `set_thalamic_gate_for_stage(stage_idx, weight)`. `weight < 0` clears the override; otherwise weight is clamped to [0, 1] and locked until the next clear. Useful for ablation studies — e.g. set motor=0 to silence articulator output.

**API**:
```
C:      nimcp_brain_set_thalamic_gate_enabled
        nimcp_brain_get_thalamic_gate_enabled
        nimcp_brain_set_thalamic_gate_for_stage
        nimcp_brain_get_thalamic_gates
Python: Brain.set_thalamic_gate_enabled(enabled)
        Brain.get_thalamic_gate_enabled() -> bool
        Brain.set_thalamic_gate_for_stage(idx, weight)
        Brain.get_thalamic_gates() -> dict
Daemon: _cmd_set_thalamic_gate_enabled
        _cmd_set_thalamic_gate_for_stage
        _cmd_get_thalamic_gates
Client: BrainProxy.set_thalamic_gate_enabled(enabled)
        BrainProxy.set_thalamic_gate_for_stage(idx, weight)
        BrainProxy.get_thalamic_gates() -> dict
```

**Diagnostics RPC** returns `{"enabled": bool, "gates": {"drive": float, "goal": float, ...}, "weights": [...], "overrides": [...], "stage_names": [...]}`.

**Known gaps**:
- Stage gates other than content / lexical / prosody / self_train are computed-but-not-applied. The structured-artifact stages would need stage-specific scaling rules (e.g. shrink Wernicke parse confidence threshold) to honor gate < 1. Out of scope for Slice 6 since suppression of those stages is already cleanly achievable via the `cascade_stage_mask_t` bit.
- Gate compute reads `thalamic_router`'s imagination_attention; the router's `set_attention(source_id, dest_id, weight)` per-route table is NOT consulted because there's no source/dest ID mapping for cascade stages. A future slice could allocate dedicated thalamic source/dest IDs for each cascade stage and route through the router proper.
- No checkpoint persistence — gates are runtime state, re-derived on first cascade call after a load.

### Slice 7: Cerebellar prediction-correction

**What**: cerebellum predicts next motor pattern; online correction signal feeds back into motor and prosody.

**Status**: implemented 2026-05-18.

**Existing cerebellum API used** (from `include/core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h`, impl in `src/core/brain/regions/cerebellum/nimcp_cerebellum_adapter.c`):

- `cerebellum_predict_outcome(adapter, motor_cmd[N], num_dims, out_predicted[N], out_confidence)` — forward-model feed-forward. Internal weights are 8×8 (see `forward_model_t::weights[64]`); `num_dims` is clamped to 8 internally.
- `cerebellum_update_forward_model(adapter, motor_cmd[N], outcome[N], num_dims)` — gradient-descent update on the (cmd, outcome) pair.
- `cerebellum_broadcast_error(adapter, error_magnitude, error_type)` — climbing-fiber-style global error broadcast to every Purkinje cell; error_type 1 = timing (used by prosody), 2 = force (used by motor), following the convention already used by `world_model_cognitive_integration.c` and `medulla_cerebellum_bridge.c`.
- Adapter lifetime + lifecycle is owned by `brain->cerebellum` — created in `nimcp_brain_factory_init_cerebellum_subsystem`, destroyed in the matching teardown; no per-cascade resource ownership.

**Data flow** (default OFF; enabled via `nimcp_brain_set_cerebellar_correction_enabled(brain, true)`):

```
   cascade_stage_motor:
     build pre-stage 8D feature (drive, syntax, phonology, lexical)
     cerebellum_predict_outcome → predicted[8]      → state.cereb_motor_predicted
     (correction_pending: record bump diag, no text-mode effector yet)
     run stage body (text mode: no physical render)
     build post-stage 8D actual
     cerebellum_update_forward_model(pre, actual)
     cerebellum_broadcast_error(pe_norm, 2 /* force */)

   cascade_stage_prosody:
     build pre-stage 8D feature
     cerebellum_predict_outcome → predicted[8]      → state.cereb_prosody_predicted[0..2]
     if correction_pending:
       base_f0  = (1-strength)·base_f0  + strength·(80 + 320·pred[6])
       range_hz = (1-strength)·range_hz + strength·(      320·pred[7])
       state.cereb_correction_applied = true
     run stage body (FFT-style synthesis)
     build post-stage 8D actual (slots[6..7] = realised F0+range)
     cerebellum_update_forward_model(pre, actual)
     cerebellum_broadcast_error(pe_norm, 1 /* timing */)

   communication_cascade_run_recurrent (between iters):
     accum_pe = motor_pe_norm + prosody_pe_norm
     brain->cerebellar_correction_pending = (accum_pe > brain->cerebellar_pe_threshold)
     # pending consumed (and reset implicitly) by next iter's stages
   On exit: restore saved correction_pending.
```

**Files touched**:
- `include/core/brain/nimcp_brain_internal.h` — append-only 7 fields on `brain_struct` (`cerebellar_correction_enabled` + strength + pending + pe_threshold + 3 diag counters).
- `include/language/nimcp_communication_cascade.h` — append-only 7 fields on `production_cascade_state_t`; static_assert bumped 760 → 864.
- `include/nimcp.h` — three new public functions + a `nimcp_cerebellar_diag_t` snapshot struct.
- `src/language/nimcp_communication_cascade.c` — forward-declared the 3 cerebellum API entries (same pattern as `world_model_cognitive_integration.c`); added 7 helper statics (`cereb_clamp01`, `cereb_build_motor_features` / `_actual` / `_prosody_features`, `cereb_norm_diff`, `cereb_bump_*`); rewrote `cascade_stage_motor` (now does predict-stage-update-broadcast or falls back to the legacy skip-record when the flag is off); injected predict-bias-update around `cascade_stage_prosody`'s F0/range/synthesis; added between-iter correction-pending update + save/restore in `communication_cascade_run_recurrent`.
- `src/api/nimcp_part_core.c` — three thin C wrappers + a diag-snapshot wrapper.
- `src/bindings/python/nimcp_python.c` — four Python methods registered in the methods table.
- `scripts/brain_daemon.py` — three RPC handlers.
- `scripts/brain_client.py` — three `BrainProxy` wrappers.
- `tests/unit/test_lang_cerebellar_correction.c` — 5 subtests covering default-OFF, strength roundtrip, motor skip when OFF, on-but-no-cerebellum still skips, recurrent save/restore of `correction_pending`. Registered in `tests/CMakeLists.txt`.

**Default-OFF guarantee**: when the flag is off OR `brain->cerebellum` is NULL, `cascade_stage_motor` short-circuits to the original `cascade_record_skip("stage_motor: text mode...")` body and `cascade_stage_prosody` never enters its cerebellar block — the cascade is byte-identical to pre-Slice-7 master for callers who don't flip the flag.

**Known gaps / TODOs**:
- Text-mode motor stage's "actual" feature vector is currently built from the same upstream signals as the "predicted" — i.e. the cerebellum sees a degenerate (cmd ≈ outcome) pair, so the forward-model weights drift only marginally. Once a physical motor backend lands (TTS / articulator synthesizer), `cereb_build_motor_actual` should read the actual articulatory plan and the cerebellum will start learning real trajectory mappings.
- The cerebellum's forward-model uses a fixed 8×8 weight matrix; our 8D feature packing under-uses the slot count. A future slice could widen to 16+ slots once we add lexical-stress + intonation features.
- Prosody bias is applied only to `base_f0` + `range_hz` (the two scalars that gate the FFT-style contour synthesis). A future slice could bias the per-syllable `coef_decline`/`coef_rise`/`coef_front` coefficients directly.
- Deeper end-to-end tests that actually exercise `cerebellum_predict_outcome` need a non-minimal-init brain (FULL/FAST modes wire the cerebellum subsystem) and belong in `tests/integration` rather than the lang_smoke suite added here.

## Migration strategy

- Old `communication_cascade_run` stays intact. Recurrent path is opt-in via flag.
- Each slice can be rolled forward / back independently.
- Eval: after each slice, run `lang_eval.py` and compare diversity + coherence + self_match.
- Tests: per-slice unit tests in `tests/cascade_recurrent_*.c`.

## Post-load wire-up (CROSS Wave-3, 2026-05-19)

Every slice-1..7 setter (respond_via_cascade, cascade_self_train_enabled,
trigram_learning_enabled, ltd_margin, lateral_inhibition_enabled,
thalamic_gate_enabled, phonological_loop_enabled, cerebellar_correction_enabled,
arcuate_feedback_strength, etc.) is RUNTIME-ONLY. `nimcp_brain_save` does NOT
persist these fields, and `nimcp_brain_load` calloc-zeros them on the reloaded
brain. Operators / trainers MUST re-apply every flag after every checkpoint
load — pre-fix they didn't, and production runs silently reverted to
bridge-only mode.

Source of truth: `data/lang_runtime_default.json`. `brain_daemon.py` calls
`_apply_runtime_lang_config(brain, logger)` immediately after
`_activate_cb_default`, which reads (in order):

1. `$LANG_CONFIG` env var (file path, or `off` to skip the apply)
2. `/etc/athena/runtime_lang_config.json`
3. The bundled default `data/lang_runtime_default.json`

The default config enables Slice 1+3+4+5+6 in their tested-default
configuration. Slice 7 (cerebellar correction) ships OFF until the
forward-model has trained enough to clear the confidence>0.3 gate
(see S7-H2 in the bugfix history). Per-key error handling: an
unknown key, an unsupported tunable, or a temporarily-disconnected
bridge logs a WARN and continues. Apply counts surface to the daemon
log on every brain start.

## KNOWN_GAPS

- **S3-H1 (Slice 3 predictors are textbook-trivial)**: the current
  prediction-error fields (`pe_content_*`, `pe_lexical_norm`,
  `pe_motor_*`, `pe_prosody_*`) are correction-magnitude norms, not
  proper "predict downstream input" predictors. A genuine FEP predictor
  would per-population learn a top-down expectation of its inputs and
  compute PE = expected - observed. The current values are close enough
  to FEP-style precision-weighting to be useful as a heuristic, but a
  proper Slice 8 design pass should rebuild them on per-population
  predictor networks.
- **Arcuate setter**: no public C/Python setter for
  `arcuate_feedback_strength` — the recurrent loop writes it
  internally and the runtime-only flag is also restored to zero on
  checkpoint reload. The default config's `_slice_2_arcuate` block
  documents this gap.

## What this is NOT

- NOT a performance optimization. Each iteration takes longer than the current single-pass cascade. The goal is biological fidelity and learnable structure, not speedup.
- NOT a replacement for fixing the underlying training (echo + cascade self-train still needed). Architecture + training are orthogonal axes.
- NOT a guarantee of fluent output. The architecture becomes CAPABLE of coherent generation; learning the right weights still requires training cycles.

## Open questions

- Convergence criterion: stop on `self_match` stable, or on full population state stable? Probably the latter; needs measurement.
- Max iteration count: real cortex settles in ~200-400ms; at our ~50ms per stage that's 4-8 iterations. Start with 8, tune.
- How to combine FEP prediction error with existing STDP/echo/comprehend_stdp signals? Likely additive in the LR weighting.
- Lateral inhibition implementation: explicit recurrent network, or simulate via repeated softmax-with-temperature?
