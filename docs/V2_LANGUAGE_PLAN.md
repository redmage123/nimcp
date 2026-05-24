# V2 Language System + Toxicity Module — Implementation Plan

**Status**: in progress
**Owner**: V2 language port

V1 has a ~41,600-LOC grounded-language system (51 C files, incl. a single
210 KB cascade orchestrator) plus a ~2,600-LOC toxicity/harm stack. V2
has *zero* language code. This plan re-implements the system from scratch
on V2's idioms (composed modules + traits, `ndarray`, `serde`,
deterministic seeded RNG) rather than line-by-line porting the C
god-struct.

## What V1 actually does (from the source survey)

- The **real engine** is `grounded_language.c` (~13 K LOC): a lexicon of
  Hebbian word→concept bindings, per-word distributional embeddings,
  n-gram/phrase tables, a comprehend path (text → semantic vector) and a
  produce path (semantic vector → text).
- The **SNN language bridge is gutted** — `decode_spikes`, `apply_stdp`,
  `bind`, `echo_correct`, etc. are no-op stubs (the "Slice B" rehoming
  never landed). The header documents the *intended* STDP/decode/beam
  math; we port that design, not the dead stubs.
- The **15-stage communication cascade** is the genuine orchestration
  layer (drive → goal → listener → episodic → content → lexical →
  syntactic → self-comprehension → speech-repair → phonological →
  prosody → motor → self-train → self-feedback), with FEP
  prediction-error settling, thalamic gating, and the developmental
  **confidence-floor as the sole length authority**.
- The **toxicity stack** is near-standalone: a regex pattern classifier
  (allowlist span-collect + toxic span-suppress + max-merge), a
  char-trigram-hashed 1024→256→64→2 MLP, and a stage-graded counterclaim
  generator. Critical invariants: the must-run gate sits **above** the
  cascade-vs-bridge branch, and the training path is **mark-not-filter**.

## Crates

- `crates/language` (`nimcp-language`) — the grounded-language engine.
- `crates/toxicity` (`nimcp-toxicity`) — the content-safety stack
  (near-standalone; depends only on `regex`, `ndarray`, `serde`).

Both are CPU-only. The SNN bridge / cascade wiring into `nimcp-brain` is
the last phase.

## Design principles carried from the survey

1. **One persistence format.** V1 had two divergent serializers (`.gl_lang`
   sidecar vs standalone `_save`) — a recurring bug source. V2 uses one
   versioned `serde` format.
2. **SGNS, not pure-attraction.** Distributional embeddings use frequency
   subsampling `sqrt(T/freq)` + K=5 negative sampling + L2-normalize.
   This is the root-cause fix for V1's repeated distributional collapse;
   it is implemented from the start, not bolted on.
3. **Confidence-floor is the length authority.** Production length is
   gated by a developmental confidence floor (stage 0→1.0 … stage 4+→0.0),
   never a hard word cap.
4. **mark-not-filter.** Toxic *training* data is logged / down-weighted /
   antigen-presented, never silently dropped; toxic *output* is blocked
   with a counterclaim at a non-bypassable gate.
5. **No god-struct, no `void*`.** Opt-in V1 features (negation, WSD,
   coref, …) become composable pipeline stages, not `bool` flags on a
   monolith.

## Phases (dependency order)

| Phase | Scope | Depends on |
|-------|-------|-----------|
| **L0** | plan + crate scaffolds (this doc) | — |
| **L1** | lexicon (word↔id, Hebbian bindings) + concept registry (union-find) | L0 |
| **L2** | distributional embeddings — SGNS `learn_from_text` | L1 |
| **L3** | n-gram / phrase table + bigram FFT spectrum | L1 |
| **L4** | comprehend (text → semantic vector) | L1–L3 |
| **L5** | produce (semantic vector → text) | L1–L3 |
| **L6** | persistence (single canonical serde format) | L1–L5 |
| **T1–T3** | toxicity stack (pattern classifier + ML head + counterclaim) | L0 (independent of L1–L6) |
| **L7+L8** | 15-stage cascade + SNN Broca/Wernicke wiring + brain integration (must-run toxicity gate above the cascade branch) + pybind | all |

## Sync log

### 2026-05-24 — V1 Tier-1 Steps D + E (cascade content-intent blending)

V1 commits `655630deb` (Step D), `1525e5be6` (Step E), `b5cd45463` (Step E
RPC follow-up) extended `cascade_stage_content` to blend four new sources
into the content intent. Synced to V2:

- **Discourse continuity (5c, w=0.15)** — *ported natively*. V2 has the
  discourse ring, so `Discourse::recent_turn_vector(back)` + a prior-turn
  (back = 2) blend in the cascade content build. This replaced the earlier
  ad-hoc "running context vector" blend with V1's actual mechanism.
- **Working memory (5b, w=0.25 × salience, 0.2 floor)**, **imagination
  (5d, w=0.2 × vividness)**, **reasoning (5e, w=0.3 × confidence, gated by
  `reason_in_content`)** — *structure ported, dormant*. V2 has no working-
  memory / imagination / reasoning subsystems, so these enter through a
  `ContentSources` hook the brain fills once those subsystems exist. The
  weights + the per-element `isfinite`/truncation guard + the
  `reason_in_content` opt-in (runtime-togglable via
  `Brain::set_reason_in_content`, V1 RPC parity) are all in place; the
  brain currently supplies no sources, so only discourse continuity is
  active — exactly mirroring V1, where reasoning ships default-OFF.

Not synced: the V1 RPC/daemon/client wiring (`b5cd45463`'s pybind +
`brain_daemon.py` + `brain_client.py`) — V2's pybind/daemon language
surface is itself a later deliverable; the brain-level setter/getter is
the V2 equivalent for now.

## Acceptance per phase

Each phase ships with crate tests green, no new clippy denials, and a
commit to `v2`. The toxicity TSV data files (`toxicity_rules.tsv`,
`_counterclaims.tsv`, `_antiframes.tsv`, `_curriculum.tsv`) are carried
over verbatim so the curated content survives the port.

## Out of scope (for now)

- GPU kernels for any language path (CPU-first, like CNN/FNO/HNN).
- The directives/combinatorial *action-harm* stack (System B) and
  gustatory toxicity (System C) — those are action-safety / sensory
  simulation, unrelated to the language-output gate.
- Pod deployment — V1 (Athena) is still the production system; V2 stays
  off the pod.
