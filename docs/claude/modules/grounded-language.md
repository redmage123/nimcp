# Grounded Language Module

**Path**: `src/language/nimcp_grounded_language.c` + sibling files
**Public header**: `include/language/nimcp_grounded_language.h`
**Internal layout**: `src/language/nimcp_grounded_language_internal.h`

Word↔concept binding lexicon with multi-turn discourse state, immune
content inspection, LGSS safety gates, and a cognitive event bus that
distributes comprehension/production events to ToM, inner speech,
imagination, and other cognitive modules.

## Runtime feature flags

Every behaviour-changing feature is OFF by default and exposed via a
`grounded_language_set_<feature>_enabled` setter, plus a getter for
telemetry. Legacy callers see bit-identical behaviour until they flip
the flag. Features added by the full-lang-walkthrough campaign:

| Flag                              | Default | Effect when ON                                                              |
|-----------------------------------|---------|-----------------------------------------------------------------------------|
| `enable_negation_inversion`       | TRUE    | Tier-2 #3 — invert activation sign for words after a negation cue           |
| `enable_sense_disambiguation`     | FALSE   | Tier-2 #6 — pick polysemous sense by intent-cosine                          |
| `enable_speech_act_classification`| FALSE   | TB-9 — populate `result->speech_act` (5-class rule-based)                    |
| `enable_sentence_segmentation`    | FALSE   | TB-6 — split input on `.`/`!`/`?` and recurse per sentence                  |
| `enable_topic_shift_detection`    | FALSE   | TB-10 — flag boundaries when latest turn cosine to mean prior K falls below |
| `enable_reconsolidation`          | FALSE   | TA-5 — decay binding strength of negated content words                      |

SNN-bridge-side flags (in `snn_lang_config_t`):

| Flag                          | Default     | Effect when ON                                  |
|-------------------------------|-------------|-------------------------------------------------|
| `enable_da_modulation`        | TRUE+gain=0 | TA-3 — gate STDP weight delta by current DA     |
| `enable_trigram_learning`     | FALSE       | TA-4 — extend bigram learn to (a,b)→c trigrams  |
| `enable_snn_spike_routing`    | FALSE       | PA-3 — route Broca/Wernicke spikes through bridge |
| `enable_comprehend_stdp`      | TRUE        | Fire CSTDP on every comprehend pass (default flipped 2026-05-14, commit `ac47802b0`) |
| `min_produce_words`           | 0 (off)     | TB-7 — suppress EOS until N words emitted       |
| `max_produce_words`           | 0 (off)     | TB-7 — hard cap at N words regardless of EOS    |
| streaming `set_stream_callback` | NULL    | TB-8 — fire per-token callback during produce   |

The bridge default for `enable_snn_spike_routing` stays FALSE (see the
sparsity-collapse note in `snn_lang_config_default`), but the brain's own
bridge is explicitly opted in via
`snn_language_bridge_set_snn_spike_routing(enabled=true, tau_ms=200)` at
the end of `attach_lang_adapters_to_substrate`
(`src/core/brain/factory/init/nimcp_brain_init_language_pops.c:354`).
Without this, `drain_pop_spikes` is a no-op and bridge STDP has nothing
to reinforce.

## Multi-turn state

Discourse turn ring + anaphora referent ring + bigram-spectrum count
matrix all persist via `grounded_language_save_multiturn_state` /
`_load_multiturn_state` (TA-1 — magic-tagged sidecar format).

## Persistence

Lexicon entries + bindings serialize via the persistence sidecar
(`grounded_language_persistence.c`); the multiturn state is a separate
sidecar with its own magic + version header. **Per-instance data
(TC-12)** lives directly on `struct grounded_language` —
`anaphora_state`, `bigram_spectrum`, plus a per-gl `tc12_lock` mutex
lazy-initialized on first use.

## Event bus (cognitive integration)

`grounded_language_subscribe_ex(gl, fn, ctx, type_mask, priority)`
hooks any cognitive consumer to one or more of:

- `GL_EVENT_NEW_WORD` — fresh lexicon entry
- `GL_EVENT_GROUNDED` — successful concept binding
- `GL_EVENT_COMPREHENDED` — comprehend finished
- `GL_EVENT_PRODUCED` — produce finished
- `GL_EVENT_NEEDS_GROUNDING` — low-confidence word seen

Pre-canned wrappers in `nimcp_grounded_language_cognitive_bridge.c`
attach inner_speech / imagination / **theory_of_mind (TC-13: real
`tom_observe` calls, not just logs)** / empathy / introspection /
reasoning / narrative / metacognition / analogical / emergent_language.

## Safety integration

- **LGSS** (TA-2) — input + output gates evaluate every comprehend +
  produce against the safety KB. Blocks bump `lgss_inputs_blocked` /
  `lgss_outputs_blocked` and emit audit events.
- **Brain immune system** (IM-3) — content inspection on comprehend
  via 5 rule-based heuristics (NaN/Inf, Welford-tracked outlier,
  repetition spam, lexicon collision, negation cascade). Inflammation
  > 0.5 registers an antigen; > 0.7 skips engram encoding.

## Cycle coordinator integration

`brain_tick_language` runs at 16ms (`BRAIN_CYCLE_LANGUAGE`). Drains
broca/wernicke bio-router inboxes, ticks the language immune bridge
+ orchestrator, fires the SNN spike-routing path, and at ~1Hz cadence
(CC-1) refreshes the bigram-spectrum FFT cache via
`grounded_language_tick_bigram_spectrum`.

## Training-loop plasticity wiring (2026-05-14, commit `ac47802b0`)

Before this commit a verified call-graph trace showed `brain_learn_vector`
never reaching the bridge STDP / trigram / comprehend-STDP paths —
counters sat flat at zero through entire training runs. The plasticity
functions were wired only to inference (`brain_decide`), the API, and the
cascade. Three independent gaps were closed:

| Path | Where it's now driven from | Counter it advances |
|------|----------------------------|--------------------|
| Bridge STDP (`snn_language_bridge_apply_stdp`) | `brain_tick_lang_bridge_spike_routing` calls it once per learn step after the drain+tick (`src/core/brain/nimcp_brain_tick_language.c:143`). Spike routing opt-in lives in `attach_lang_adapters_to_substrate`. | `total_stdp_updates` + `da_gated_stdp_passes` |
| Trigram (`grounded_language_learn_text_bigrams`) | `brain_learn_vector` calls it at `lr=0.02` after `learn_from_text` + `learn_syntax` (`src/core/brain/learning/nimcp_brain_learning.c:351`). Walks `(prev,next)` bigrams and (when `enable_trigram_learning`) `(a,b)→c` triples through the bridge. | `total_trigram_updates` |
| Comprehend-STDP | `enable_comprehend_stdp` default flipped TRUE; cascade stage-0 already wires `comprehend` on every input. | `comprehend_stdp_passes` + `comprehend_stdp_pairs_fired` |

The resume path also reaches `attach_lang_adapters_to_substrate`, which
now unconditionally calls `snn_language_bridge_connect_neuromod` (commit
`6364a1468`) so the DA three-factor gate is live whether the brain
came up cold or via checkpoint. The cold-init path in
`nimcp_brain_init_language.c:842` is gated on the neuromod system
existing at that earlier init point; the resume path didn't satisfy
that gate. `connect_neuromod` just stores the pointer, so double-calling
across both paths is harmless.

## lang_status `plasticity` block

`brain_daemon`'s `lang_status` RPC exposes the bridge counters under
`response["plasticity"]` (`scripts/brain_daemon.py:2037`). Fields:

| Field | Type | Meaning |
|-------|------|---------|
| `total_stdp_updates` | uint64 | Spike-driven STDP weight writes (commit `82b2d9253`) |
| `total_trigram_updates` | uint64 | Bigram + trigram next-token updates from `learn_text_bigrams` |
| `echo_correct_calls` / `_pairs` / `_target_misses` | uint64 | Cascade self-train teacher-signal stats |
| `comprehend_stdp_passes` / `_pairs_fired` | uint64 | CSTDP firings |
| `da_gated_stdp_passes` | uint64 | STDP passes that ran the three-factor DA gate |
| `last_da_modulation` | float | Most recent `1 + DA × gain` multiplier (also surfaced by `echo_correct` since commit `384eded89`) |
| `next_token_cold_start_skips` | uint64 | Process-global counter (commit `6364a1468`) — times `grounded_language_learn_next_token_triple` bailed early because prev-words lacked concept bindings (total ≤ 1e-6). Climbing alongside flat `total_trigram_updates` = cold-start ramp; flat alongside flat = genuine stall (input labels too short to tokenize to 3 tokens). |

## Decode stop-conditions and `min_produce_words` (commit `0d993d656`)

`snn_language_bridge_produce` has four stop conditions in the greedy
decode path; all four now respect `min_produce_words`:

1. EOS pick (line ~2042) — original TB-7 wiring.
2. Beam decoder EOS path (line ~2610) — original TB-7 wiring.
3. Confidence-floor break (line ~2088) — **fixed 2026-05-14**.
   Previously, when per-step confidence dropped below 0.01 and at least
   one word had been emitted, the loop broke immediately, collapsing
   the greedy path to a 1-word utterance on any undertrained bridge.
   That in turn starved the cascade self-train trigram path (needs
   ≥3 tokens). The floor break now suppresses while
   `word_count < min_produce_words` and bumps
   `bridge->stats.length_min_suppressions` to mirror the EOS telemetry.
4. Hard `max_produce_words` cap.

With the default `min_produce_words == 0` the guard is a no-op and all
four conditions fire bit-for-bit as before.

`deploy_to_pod.sh` step 5b drops a detached poller (commit `378bc6109`)
that calls `set_length_control(min=3, max=0)` once the daemon socket
is serving — so the floor is active on production without a manual
RPC. Env overrides: `POD_MIN_PRODUCE_WORDS`, `POD_MAX_PRODUCE_WORDS`
(0 skips activation).

## Wedge mitigation: `learn_language` kill switch (commit `6dc8f6339`)

The `_cmd_learn_language` RPC handler defaults to a no-op via the
`NIMCP_DISABLE_LEARN_LANGUAGE` env var (default `"1"`,
`scripts/brain_daemon.py:2810`). Set to `"0"` in the supervisord env to
re-enable.

This is defensive engineering for the 2026-05-15 stage-2 wedge.
`brain.learn_language(text)` is fronted by `_cmd_learn_language` →
`grounded_language_learn_from_text`. The wedge watchdog
(commit `85d18d45c`) captured all 4 RPC workers blocked on the shared
`BrainService._lock` with the lock-holder deep inside
`grounded_language_learn_from_text` (lines 3500-3518 are the cross-bind
loop — O(N² × B²) in lexicon size × bindings-per-entry). At stage 2 the
fan-in of additional `learn_language` callers (sibling_dialog,
socratic_qa, failure_replay, music_rhythm) saturated the worker pool
so even `ping` timed out. The kill switch lets training continue via
`learn_vector_batch` (the primary path) until a C-side cap / per-call
deadline lands.

Training-loop language learning is **not** affected:
`brain_learn_vector` (the per-step path used by the immersive trainer)
calls `grounded_language_learn_from_text` + `_learn_syntax` +
`_learn_text_bigrams` directly inside the C library, bypassing the
daemon RPC layer entirely. The kill switch only no-ops the
out-of-band `brain.learn_language(text)` RPC.

## Wedge watchdog (commit `85d18d45c`)

`brain_daemon._install_wedge_watchdog` calls
`faulthandler.dump_traceback_later(120s, repeat=True)` at startup and
appends all-thread Python stacks to `/var/log/athena/brain_traceback.log`.
This is the diagnostic mechanism for catching future GIL / worker
wedges when ptrace is unavailable inside the pod container
(`CAP_SYS_PTRACE` missing, `kernel.yama.ptrace_scope=1` read-only).
The dumping thread is separate from the GIL-holding thread, so even a
fully-wedged hot thread eventually gives up the GIL long enough for
the watchdog to fire. Cost is ~5ms per dump every 2 min.

## Trainer silent-failure surfacing (commit `d1d404cd2`)

`MiniBatchTrainer.flush()` (`scripts/immerse_athena.py:1934`) used to
swallow every `learn_vector_batch` exception in a bare `except` and
treat the failed flush as loss=0.0 — after a brain SIGTERM the trainer
"trained" silently for 3 hours with no actual learning. Three changes:

- Log first failure of every streak, then every 10th failure.
- Crash after 50 consecutive flush failures
  (`MAX_CONSECUTIVE_FLUSH_FAILURES = 50`) so supervisord restarts
  the trainer.
- Recovery log on first success after a failing streak.

Companion `brain_client.py` (commit `d1d404cd2`) hardens the retry
budget: `FileNotFoundError` ("socket gone" — daemon restarting) no
longer counts against `MAX_RETRIES=5`; it triggers
`_wait_for_daemon` instead. `DAEMON_WAIT_TIMEOUT` bumped 300s → 1800s
to cover the full-init resume of a 2M-neuron checkpoint after a brain
SIGKILL.

## T3-2 cohesive conjunction insertion (2026-05-29)

Tier 3 closer for the produce surface-corrector stack. Inserts "and"
between adjacent SVO clauses in the polished utterance so two-clause
produce output reads with a connective:

```
the cat ran the dog jumped  →  the cat ran and the dog jumped
the cat ran he jumped       →  the cat ran and he jumped
```

**Detection.** Walks the token stream, classifies each via `gl_f4_class`
with a cold-lexicon surface fallback (`t3_conj_is_verb` mirrors
`gl_f4_is_past`: `-ed` / `-ing` suffix + irregular-past shortlist;
`t3_conj_is_pronoun_surface` covers he/she/it/they/...). The first
sequence VERB → (DET/NOUN/PRONOUN that is NOT a connective) → VERB
fires a single insert.

**Idempotent.** Re-running on the polished output sees the connective at
the candidate boundary and bails.

**Cascade position.**

```
F4 fluency  →  T2 pronominalize  →  T3-1 givenness  →  T3-2 conjunction
   →  TF-1 record_diff (delta capture)  →  TF-3/4/5 apply  →  F1 polish
```

T3-2 sits BEFORE the TF diff so the inserted "and" contributes to the
delta stream — TF-3 trigram and TF-4 distributional feedback learn
`VERB and DET` patterns over time.

**Runtime knob.** `produce_t3_conjunction`, default **ON**. Stage-gated
>= 2 inside the helper. Bit assignment for TF feedback gating:
`GL_TF_BIT_T3_CONJUNCTION = 16` (in `GL_TF_BIT_ALL_CORRECTORS = 31`).

**Test surface.** `test_lang_t3_conjunction.c` — 10 sub-tests covering
default-ON, setter round-trip, stage gate, basic insert, idempotent,
existing-connective skip (and/but/so/or/then/while/because/although),
single-clause no-change, no-second-verb (object follow) no-change,
NULL/empty safety, pronoun-led clause B.

## Tier-Feedback (TF) plasticity loop (2026-05-28/29)

Tier 1-3 surface correctors (F3 agreement, F4 fluency, T2-1 pronominalize,
T3-1 givenness, T3-2 conjunction) emit deterministic post-readout polish.
The TF subsystem captures the per-token delta between raw produce output
and the cascade-corrected output, then feeds those deltas back into
plasticity at a small learning rate so the network eventually internalizes
the corrected form. The correctors graduate to scaffolding the network
no longer needs.

**Design constraints** — per-position delta (never whole-output imitation),
small lr (~10× smaller than curriculum), outcome-gated (only fires on
clean produce + neutral/positive DA + fresh reward + zero repair attempts),
default ON for trigram + distributional paths, default OFF for binding-STDP.

**File map** —
- `include/language/nimcp_grounded_language_tf.h` — API surface
- `src/language/nimcp_grounded_language_tf.c` — implementation (diff +
  gate + 3 apply paths + cascade dispatcher)
- `src/language/nimcp_communication_cascade.c` — call sites
  (`grounded_language_tf_record_diff` for TF-1 telemetry, then
  `grounded_language_tf_apply_cascade_feedback` for plasticity dispatch)

**API surface** —

| Function | Purpose |
|---|---|
| `gl_tf_diff_correctors(raw, corrected, deltas, cap, source)` | Pure LCS-aligned Wagner-Fischer; emits SUBSTITUTE/INSERT/DELETE records. Bounded at 96 tokens. |
| `grounded_language_tf_record_diff(gl, raw, corrected, deltas, cap)` | Diff + telemetry bump (`tf_calls`, `tf_deltas_captured`). Independent of plasticity. |
| `gl_tf_outcome_ok_reason(facts) -> enum` | Outcome gate: stage ≥ 2, master ON, reward ≥ 0, age ≤ ttl, repair_attempts == 0. Returns a specific reason on rejection so per-reason telemetry can split. |
| `gl_tf_apply_trigram(gl, corrected_text, deltas, n)` | TF-3 — drives `learn_next_token_pair/_triple` at `tf_lr_trigram`. DELETE skipped. |
| `gl_tf_apply_distributional(gl, corrected_text, deltas, n)` | TF-4 — EMA-nudge the corrected token's `context_vector` toward the local window mean; L2-normalize. |
| `gl_tf_apply_bridge_stdp(gl, deltas, n)` | TF-5 — asymmetric strength touch on the top lexicon binding. Default lr 0.0 (inert). |
| `grounded_language_tf_apply_cascade_feedback(gl, raw, corrected, reward, age, ttl, repair)` | Cascade dispatcher. Master/mask fast-skip first, then build outcome facts, gate, then dispatch to the three apply paths per their lrs. Sole public entry from cascade. |

**Runtime knobs** (all 5-layer reachable — C/Python/daemon-config/persistence) —

| Knob | Default | Role |
|---|---|---|
| `produce_corrector_feedback_enabled` | `true` | TF master |
| `tf_enabled_correctors` | `0x1F` (all) | Per-corrector bitmask |
| `tf_lr_trigram` | `0.002` | TF-3 lr |
| `tf_lr_distrib` | `0.01` | TF-4 lr |
| `tf_lr_bridge_stdp` | `0.0` | TF-5 lr — wired-but-inert by default |

Bit constants (`GL_TF_BIT_F3_AGREEMENT=1`, `_F4_FLUENCY=2`,
`_T2_PRONOMINALIZE=4`, `_T3_GIVENNESS=8`, `_T3_CONJUNCTION=16`,
`_ALL_CORRECTORS=0x1F`) — per-corrector bits gate which surface
corrector RUNS upstream (not which delta-source is trained on); the
diff is always aggregate.

**Telemetry surface** — `lang_status` RPC returns a `tf` block:

```json
{
  "tf": {
    "calls":            <int>,   // record_diff invocations (TF-1)
    "deltas_captured":  <int>,   // total deltas emitted by record_diff
    "outcome": {
      "ok":              <int>,
      "blocked_stage":   <int>,  // curriculum stage < 2
      "blocked_master":  <int>,  // (always 0 — master fast-skips)
      "blocked_da":      <int>,  // last_external_reward < 0
      "blocked_stale":   <int>,  // age > ttl
      "blocked_retry":   <int>   // repair_attempts > 0
    },
    "updates": {
      "trigram":  <int>,  // tf_lr_trigram applies that returned success
      "distrib":  <int>,  // tf_lr_distrib EMA nudges applied
      "stdp_pos": <int>,  // corrected-side strength bumps
      "stdp_neg": <int>   // raw-side strength bumps (half-lr)
    },
    "config": { "master": <bool>, "enabled_correctors_mask": <int>,
                 "lr_trigram": <float>, "lr_distrib": <float>,
                 "lr_bridge_stdp": <float> }
  }
}
```

Diagnostic rules of thumb:
- `tf.outcome.blocked_stage` dominating → curriculum hasn't reached stage 2.
- `tf.outcome.blocked_da` dominating → recent reward stream is punishing.
- `tf.outcome.blocked_retry` dominating → speech-repair is firing — TF
  is correctly suppressing on noisy passes.
- `tf.updates.trigram` flat with `tf.outcome.ok > 0` → upstream
  `next_token_cold_start_skips` bouncing (curriculum hasn't bound enough
  context).
- `tf.updates.stdp_pos > 0` when no operator has raised
  `tf_lr_bridge_stdp` → BUG (path is wired-but-inert by spec).

**Mode-collapse risks + soak gates** — TF-4 nudges target's context_vector
toward LOCAL neighbors; sustained substitution of the same closed-class
word (e.g. T3-1 "a" → "the") could homogenize function-word embeddings.
Watch pairwise cosine across {the, a, in, of, with} during soak; if any
pair > 0.95 the lr is too high. TF-5 strengthens corrected-token top
bindings; this is why its default lr is 0 — left unattended at default
0.05 a single dominant closed-class binding could climb the produce-score
and crowd out content words.

**Test surface** —

| Test | Slice |
|---|---|
| `test_lang_tf_diff` | TF-1 — diff correctness |
| `test_lang_tf_gating` | TF-2 — defaults + clamp + persist + gate matrix + precedence |
| `test_lang_tf_trigram_apply` | TF-3 — bigram fallback + triple path + counter bumps |
| `test_lang_tf_distrib_apply` | TF-4 — EMA + L2-normalize + window edges |
| `test_lang_tf_stdp_apply` | TF-5 — asym pos/neg, clamps, lr=0 inert |
| `test_lang_tf_integration_50turn` | TF-6 — end-to-end gate matrix, 50 turns + composite |
| `test_lang_tf_off_no_op` | TF-6 — regression: TF-OFF (3 ways) is bit-identical no-op |

## Tests

`tests/unit/test_lang_*.c` — 13 standalone-C tests under the
`lang_smoke` ctest label. Each Tier-A/B/C campaign feature has its own
test file. **Footgun**: after `gl_stats_t` or `snn_lang_stats_t` field
additions, force-rebuild dependent tests (`rm` the .o + cmake build) —
the dep tracker doesn't always catch header layout changes and the
tests will stack-smash at shutdown rather than report a clean fail.
