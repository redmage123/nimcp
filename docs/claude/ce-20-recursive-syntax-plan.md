# CE-20: Recursive Syntax (Chomsky-style Merge) — Plan

**Status**: Draft — not started
**Owner**: bbrelin / Claude
**Prereq gate**: Stage-1 template recovery complete; grounded_language stable
**Estimated effort**: 4 wks (Phase A, 5 d) + 4–6 wks (Phase B, parallel)
**Dependencies**: GL-W5 templates (done), grounded_language (done), Broca/Wernicke pops (done), canonical corpus (done)

---

## 1. Motivation

Athena's current language production is **item-based**: GL-W5 templates store
flat bigram/trigram statistics + phrase-vector slots. This matches the
Tomasello "frame-and-slot" stage of human acquisition (~ages 1–3) but plateaus
there. Humans outgrow it because two new mechanisms come online by ~age 2.5–4:

1. **Hierarchical syntax** — phrases nest inside phrases ("the cat [that
   chased the mouse [that ate the cheese]]").
2. **Recursive Merge** (Chomsky / Hauser-Chomsky-Fitch 2002) — a single
   operator that combines two syntactic objects α, β into a new composite γ,
   and γ can itself be the input to another Merge.

Without recursion, Athena is structurally incapable of:
- Center-embedded relative clauses
- Arbitrary-depth coordination ("X and Y and Z and …")
- Compositional novel phrases beyond the depth of memorized templates
- Garden-path resolution
- Long-distance dependencies (wh-movement, subject-verb agreement across
  intervening NPs)

This isn't a frontier-AI nice-to-have. It's the hard floor of human-grade
language production. The current template collapse incident makes it worse:
flat templates have nowhere to "fall back to" when statistics drift, while a
recursive grammar can regenerate well-formed novel utterances from primitives.

## 2. Two-phase strategy

We build a **symbolic** recursion layer first (Phase A — fast, gives Athena
recursive output in one week) and then a **neural** Merge operator on top
(Phase B — biologically faithful, several weeks). Phase A becomes a
teacher/fallback signal during Phase B training.

### 2.1 Why both, in this order

- **Pure A** is fast but plateaus at hand-coded rules; doesn't learn novel
  constructions.
- **Pure B** is right but trains slowly from scratch — Broca's pop has no
  prior over Merge, so without a teacher signal the loss landscape is brutal.
- **A→B** lets us bootstrap: Phase A produces recursive parses that drive
  Phase B's loss, and Phase A stays online as a fallback whenever Phase B's
  confidence drops.

This mirrors the pattern we already used for grounded_language (rules-based
bootstrap → learned embeddings), and the GL-W5 → semantic-vector pipeline.

---

## 3. Phase A — Symbolic Recursive Grammar

### 3.1 Module layout

```
src/cognitive/language/
  nimcp_recursion_engine.h   ← public API
  nimcp_recursion_engine.c   ← rule store + rewrite + sampler
  nimcp_grammar_extractor.c  ← CKY parser over corpus → rule-prob estimates
  nimcp_grammar_rules.c      ← seed rule table (≈40 rules)
include/cognitive/language/
  nimcp_recursion_engine.h   (re-export)
tests/cognitive/language/
  test_recursion_unit.c
  test_recursion_integration.c
  test_recursion_regression.c
  test_recursion_e2e.c
```

### 3.2 Public API

```c
typedef struct nimcp_recursion_engine_s nimcp_recursion_engine_t;

nimcp_error_t nimcp_recursion_engine_create(nimcp_recursion_engine_t **out);
void          nimcp_recursion_engine_destroy(nimcp_recursion_engine_t *eng);

/* Generate a recursively-composed utterance.  Templates from GL-W5 are
   the terminals; this engine composes them.                              */
nimcp_error_t nimcp_recursion_engine_generate(
    nimcp_recursion_engine_t *eng,
    const grounded_language_t  *lex,
    const semantic_vector_t    *intent,
    int                         max_depth,
    char                       *out_utf8,
    size_t                      out_cap);

/* Train rule-application probabilities from a corpus.                     */
nimcp_error_t nimcp_recursion_engine_train(
    nimcp_recursion_engine_t *eng,
    const char *const         *corpus_lines,
    size_t                     n_lines);

/* Save / load — extends brain.bin via grounded_language sidecar.          */
nimcp_error_t nimcp_recursion_engine_save(const nimcp_recursion_engine_t*,
                                          const char *path);
nimcp_error_t nimcp_recursion_engine_load(nimcp_recursion_engine_t*,
                                          const char *path);
```

### 3.3 Seed rule table (≈40 rules)

```
S    → NP VP
S    → S CONJ S          (* coordination *)
NP   → DET N
NP   → DET ADJ N
NP   → NP PP             (* recursion *)
NP   → NP REL_CLAUSE     (* recursion *)
VP   → V
VP   → V NP
VP   → V NP PP
VP   → V S_BAR           (* "I think [that X]" — recursion *)
PP   → P NP
REL_CLAUSE → REL VP
S_BAR → COMP S
…
```

Terminals (DET, N, ADJ, V, P, REL, COMP) draw from grounded_language entries
tagged with the appropriate POS. POS tagging reuses the morphology pass from
GL-W2.

### 3.4 Sampler

Stochastic top-down derivation:
1. Start from S.
2. At each non-terminal, sample a rule weighted by learned probability.
3. Hard depth cap (`max_depth`) to prevent runaway recursion.
4. At terminals, pick a lexicon entry whose semantic vector is closest to
   the residual intent vector.
5. Linearize, apply morphology (already exists in GL-W2), emit.

### 3.5 Phase A integration points

| Site | Change |
|------|--------|
| `produce_text()` (broca path) | Call `recursion_engine_generate()` first; fall back to GL-W5 template-fill if recursion confidence < 0.4 or output_len < 3 |
| `_run_extra_corpora_priming` | Call `recursion_engine_train()` once per stage from canonical corpus lines |
| `brain_save` / `brain_load` | Extend grounded_language sidecar with rule-probability table |
| `_cmd_grounded_respond` | Add `recursion_depth` field to response so eval can confirm recursion engaged |

### 3.6 Phase A tests (TDD — write before code)

| Test | What it verifies |
|------|------------------|
| `test_recursion_unit.c` | `merge(α, β) → γ` shape, rule-probability normalization, depth cap |
| `test_recursion_integration.c` | Full S → NP VP → "the cat sat" generation against seeded lex |
| `test_recursion_regression.c` | GL-W5 template path still works when engine disabled |
| `test_recursion_e2e.c` | After training on 100 canonical sentences, produces recursive output (NP→NP PP, S→S CONJ S) and well-formedness rate >0.7 |

### 3.7 Phase A risks

- **Rule explosion** at depth >5 → mitigate with hard cap = 4 + budget on
  total non-terminal expansions per utterance (32).
- **POS tagging gaps** in lexicon → rules that demand a missing POS skip.
- **Mode mixing** with GL-W5 templates → confidence gate, not blend.

---

## 4. Phase B — Neural Merge Operator

### 4.1 Concept

Train a learned operator
```
merge: ℝ^d × ℝ^d × {label} → ℝ^d × {STOP, CONTINUE}
```
that lives **inside Broca's SNN pop** (`broca_substrate`). Inputs are two
phrase vectors; output is a composite vector + stop signal. Recursion is just
"feed γ back through merge with another phrase vector".

This matches the neurolinguistic claim that pars opercularis (Broca, BA44)
implements hierarchical structure-building, while pars triangularis (BA45)
handles semantic composition (which is already partly modeled by
grounded_language phrase vectors).

### 4.2 Architecture

- **Phase-vector encoder**: word/phrase vectors from grounded_language are
  the leaves.
- **Merge head**: a small MLP (or learned linear + ReLU + linear)
  conditioned on a syntactic-label embedding (NP, VP, PP, S, S_BAR, …).
  Lives as a new sub-network attached to the Broca pop's afferent path.
- **Stop gate**: separate sigmoid head — when stop > 0.5, stop recursion
  and linearize.
- **Linearizer**: reuses Phase A's terminal expansion + morphology.

### 4.3 Loss

Three components, summed with curriculum-controlled weights:
1. **Reconstruction**: parse a corpus sentence with Phase A's parser →
   walk the parse tree bottom-up applying `merge` → final vector should be
   close to the sentence's semantic_vector. (Teacher-forced.)
2. **Production-match**: when generating, the produced utterance's
   semantic vector should be close to the input intent vector. (RL-style,
   high variance.)
3. **Well-formedness**: a small classifier (trained on Phase A output) that
   distinguishes well-formed from scrambled — used as a regularizer.

### 4.4 Training schedule

| Stage | Loss weights (recon / prod / wf) | Phase A role |
|-------|-----------------------------------|--------------|
| B0 (1 wk) | 1.0 / 0.0 / 0.0 | Full teacher — parse trees fed in |
| B1 (1 wk) | 0.7 / 0.2 / 0.1 | Teacher + light production loss |
| B2 (2 wk) | 0.3 / 0.5 / 0.2 | Mostly self-driven; A as fallback |
| B3 (∞)    | 0.1 / 0.7 / 0.2 | A only when neural confidence < 0.3 |

### 4.5 Wiring

| Site | Change |
|------|--------|
| `nimcp_brain_init_language_pops.c` | Add `merge_head` allocation hung off `broca_substrate` |
| `produce_text()` | If `merge_head_enabled && confidence > 0.3`, use neural merge; else fall through to Phase A; else GL-W5 |
| `brain_learn_vector` | Extend language-loss tap to drive merge_head gradient |
| `brain_save` / `brain_load` | Merge-head weights as new sidecar `merge_head.bin` |
| Python bindings | `produce_text(..., recursion_mode={"neural","symbolic","template"})` |

### 4.6 Phase B tests

| Test | What it verifies |
|------|------------------|
| Unit | `merge` op shape, gradient flows through label embedding |
| Integration | Recursion depth 3+ produces well-formed NP→NP PP chains after 1k training sentences |
| Regression | Phase A and template paths unaffected when neural disabled |
| E2E | BLiMP-lite (minimal pairs): subject-verb agreement, reflexive binding, island constraints — accuracy > 0.65 after B2 |

### 4.7 Phase B risks

- **Vanishing gradient through deep recursive applications** — mitigate
  with the same diversity-loss + gradient-normalization stack used
  elsewhere; cap unrolled recursion at 4 in training.
- **Catastrophic interference with grounded_language** — Broca pop already
  carries grounded-vector signal; mitigate with frozen-during-B0
  grounded_language and gradual unfreezing in B1+.
- **Confidence calibration drift** — same temperature-scaling pass we
  added for DK-C.

---

## 5. Prerequisites & ordering

**Hard prereqs (must be done before CE-20 starts):**
1. ✅ GL-W5 compositional templates (done)
2. ✅ grounded_language with phrase-vector slots (done)
3. ✅ Canonical corpus ingestion (done)
4. ✅ Broca/Wernicke language pops (done — 200K neurons)
5. ⏳ **Stage-1 template recovery** — not done. Current campaign in flight.
6. ⏳ **Phrasal grounding extension** — grounded_language tracks word-level
   groundings; CE-20 needs phrasal groundings ("the red ball" as a
   composite of grounded primitives, not an opaque chunk).

**Soft prereqs (would help, not blocking):**
- POS tagger over lexicon entries (we have morphology in GL-W2; need to add
  category tags).
- Larger canonical corpus (current 7 works × tiny chunks won't yield robust
  rule statistics; aim for 100k+ sentences before B0).

## 6. Failure modes to plan around

1. **Recursion + flat-template mode-mixing in production**: do NOT blend.
   Confidence-gate. We learned this from the SNN-bridge-blend incident
   2026-05-05.
2. **Recursion over ungrounded primitives → well-formed nonsense**:
   require all leaf terminals to have grounded_language entries with a
   confidence score above 0.5.
3. **Training-time recursive expansion blows the SNN step budget**: cap
   training-time depth at 4; production-time depth at 6.
4. **State-file rewinds losing rule probabilities**: persist rule-prob
   table as its own sidecar (atomic rename), not just inside brain.bin.

## 7. Acceptance criteria (Phase A + B together)

1. Production rate of NP→NP PP / S→S CONJ S constructions ≥ 0.15 of
   non-trivial utterances (vs. 0 today).
2. BLiMP-lite minimal-pair accuracy ≥ 0.65 after B2.
3. Maximum production depth ≥ 4 demonstrated on held-out intents.
4. No regression on existing language eval (template-only mode still works
   when neural+symbolic disabled).
5. No regression on multimodal grounding eval.

## 8. Out of scope (deferred to later CE)

- Movement / wh-extraction (CE-21)
- Inflectional agreement beyond what morphology already covers (CE-22)
- Phonological-articulatory closed loop with audio output (CE-23)
- Pragmatic / theory-of-mind shaping of production (depends on CE-8 wiring,
  separate campaign)

## 9. Tracking

Tasks to file once campaign opens:
- CE-20-A1: recursion_engine module skeleton + tests
- CE-20-A2: seed rules + POS-tag pass over lexicon
- CE-20-A3: CKY extractor + rule-prob estimation
- CE-20-A4: integration into produce_text + sidecar save/load
- CE-20-A5: Phase A tests + walkthrough
- CE-20-B0: merge_head module + reconstruction loss training
- CE-20-B1..B3: progressive curriculum (per §4.4)
- CE-20-B-tests: unit + integration + regression + e2e + BLiMP-lite

---

**Decision required before kickoff**: confirm Stage-1 recovery has
stabilized (template-collapse symptoms gone, multimodal eval passes), then
queue CE-20-A1.
