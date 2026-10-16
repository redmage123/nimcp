# Communication Cascade — Plan
**Status:** design + Phase 2A skeleton in progress
**Owner:** language production audit follow-up
**Last updated:** 2026-05-10

## Reframing

The previous "language" track treated production as a *bridge feature* — a softmax over concept↔word bindings. That model is fundamentally insufficient: real language emerges from a **multi-region cognitive cascade**, and treating it as a single-module problem produced word salad regardless of how thoroughly we tuned the bridge.

This plan reframes communication as a **first-class brain system**, the *Communication Cascade*, that orchestrates contributions from at least 9 distinct cognitive stages into a single utterance. The bridge becomes one stage (lexical selection) inside the cascade, not the whole thing.

## Why "wiring is enough" was wrong

The Phase 1 audit asked: "is wiring `grounded_respond` through Broca enough to fix output?" The experiment found:

1. Broca was never even attached to grounded_language (init ordering bug, since fixed in commit `374ae27d3`).
2. Even with the attach, Broca rejected every input because its lexicon had no POS info.
3. **Most importantly:** even if both above were fixed, Broca would have re-ordered word salad into grammatically-shaped salad. The bridge produces near-random words because it has no upstream goal — no drive, no listener model, no working-memory plan, no episodic context.

The real bottleneck is not output formatting — it's **input formation**. Production needs something to *say*. The cascade builds that.

## The 9-stage cascade

Each stage contributes a specific signal. Stages 1-5 build communicative intent; stages 6-9 surface it as language.

| # | Stage | Module(s) | What it contributes |
|---|---|---|---|
| 1 | **Drive** | Hypothalamus, Insula, Amygdala | Why speak now? Hunger urgency, social drive, fear, curiosity, comfort/discomfort |
| 2 | **Goal formation** | PFC, Working Memory | What is the speech act? Question, statement, request, alarm. What's the target concept? |
| 3 | **Listener model** | Theory of Mind | Who's listening? What do they know? What's their affect? |
| 4 | **Episodic retrieval** | Hippocampus | What relevant past experiences inform what to say? |
| 5 | **Content composition** | Semantic memory, OFC, Cingulate | Combine drive + goal + audience + memory into a weighted *content intent vector* |
| 6 | **Lexical selection** | SNN language bridge | For each active concept, choose a word |
| 7 | **Syntactic structuring** | Broca syntax processor | Apply grammar rules; reorder into grammatical sequence |
| 8 | **Phonological encoding** | Broca phonological | Words → phonemes; prosody assignment (FNO eventually) |
| 9 | **Motor / output** | Broca speech motor, Cerebellum | Articulator commands or, for text mode, render |

## Queryable state per module (audit complete)

All major modules expose getters needed to read state during a cascade tick:

- **Hypothalamus**: `hypo_drive_get_system_state()` returns 9 drives (hunger, thirst, social, curiosity, safety, autonomy, competence, etc.) with magnitudes + urgencies
- **Amygdala**: `amygdala_get_response()` returns fear_intensity, anxiety_level, threat_level
- **Insula**: `insula_get_emotional_state()` returns valence, arousal, joy/sadness/fear/anger/etc., somatic_marker
- **OFC**: `ofc_get_integrated_value()`, `ofc_get_emotion_modulated_value()` for stimulus value
- **PFC**: `prefrontal_get_active_goals()` returns goals[] with priority, value, urgency
- **Working Memory**: `working_memory_get(i)` + `_get_salience()` for active chunks
- **Theory of Mind**: `tom_get_agent_state()` returns belief, desire, intention, emotion + confidence
- **Cingulate**: `cingulate_evaluate_conflict()` for current conflict; `cingulate_get_control_level()` for cognitive control
- **Hippocampus**: `hippo_find_similar_episodes(query_vec)` for cue-based retrieval; `hippo_get_recent_episodes()` for temporal context
- **Semantic Memory**: `semantic_memory_query(features, max)` for concept activation with spreading
- **Engram**: `engram_recall(cue)` for pattern completion

Only gaps:
- **Episodic Replay**: write-only today, needs new accessor — but Hippocampus + Semantic Memory cover episodic retrieval needs for now
- **Engram similarity search**: pattern-completion only; not critical for first cascade

## Data structure: `production_cascade_state_t`

A single struct accumulates contributions across all 9 stages. Each stage reads inputs (prior stages + module state) and writes its outputs (one section of the struct). Stage-by-stage isolation keeps the design SOLID.

```c
typedef struct {
    /* Stage 1: Drive */
    float drive_magnitude;       /* 0..1, how strong is the urge to communicate */
    float drive_valence;         /* -1..1, approach vs avoid */
    float drive_arousal;         /* 0..1, calm vs urgent */
    uint8_t dominant_drive;      /* hypo_drive_type_t */

    /* Stage 2: Goal */
    speech_act_type_t act_type;  /* STATEMENT, QUESTION, REQUEST, EXCLAMATION, ALARM */
    uint64_t target_concept_id;  /* Primary concept being communicated */
    uint64_t topic_concept_ids[8]; /* Related concepts from WM + PFC goals */
    uint32_t topic_count;
    float    goal_priority;      /* 0..1 from PFC */

    /* Stage 3: Listener */
    bool     listener_known;
    tom_belief_t   listener_belief;
    tom_emotion_t  listener_emotion;
    float    audience_familiarity; /* 0..1 */

    /* Stage 4: Episodic */
    uint64_t episodic_concept_ids[16];
    float    episodic_relevances[16];
    uint32_t episodic_count;

    /* Stage 5: Content (the bridge's input) */
    float*   content_intent;     /* allocated; semantic_dim entries; combined intent vector */
    uint32_t content_dim;
    float    content_confidence; /* 0..1 — how strongly cohered the cascade is */

    /* Stage 6+: filled by bridge + broca */
    char*    utterance;          /* allocated; final text */
    uint32_t word_count;
    float    fluency;
    float    syntactic_validity; /* 0 if Broca rejected; 1 if grammatical */

    /* Diagnostics */
    uint32_t stages_completed;
    uint32_t stages_failed;
    char     failure_reason[128];
} production_cascade_state_t;
```

## Interfaces between stages

Each stage is a function:
```c
int cascade_stage_drive(brain_t brain, production_cascade_state_t* state);
int cascade_stage_goal(brain_t brain, production_cascade_state_t* state);
int cascade_stage_listener(brain_t brain, production_cascade_state_t* state);
int cascade_stage_episodic(brain_t brain, production_cascade_state_t* state);
int cascade_stage_content(brain_t brain, production_cascade_state_t* state);
int cascade_stage_lexical(brain_t brain, production_cascade_state_t* state);
int cascade_stage_syntactic(brain_t brain, production_cascade_state_t* state);
int cascade_stage_phonological(brain_t brain, production_cascade_state_t* state);
int cascade_stage_motor(brain_t brain, production_cascade_state_t* state);
```

Top-level orchestrator:
```c
int nimcp_brain_produce_cascade(
    nimcp_brain_t brain,
    const char* prompt_or_null,    /* if non-null, ground stage 1's drive in this comprehended input */
    production_cascade_state_t* out_state);
```

If `prompt_or_null` is provided, comprehend it first to get a semantic vector that biases the drive/goal stages (mimics "responding to a question"). If null, the cascade is fully internally driven (mimics spontaneous speech).

## Acceptance criteria

The cascade is "working" when the following hold simultaneously on a brain trained for ≥1 stage:

1. **Multi-word grammatical output**: outputs of length ≥3 words pass Broca's syntax validation (`syntax_valid && agreement_valid`).
2. **Semantic discrimination**: 50 distinct prompts produce ≥30 distinct outputs.
3. **State sensitivity**: same prompt with hypothalamus in HUNGRY state vs SATED state produces measurably different output (different drive_dominant, different topic concepts).
4. **Listener-aware**: with ToM listener_belief set to "knows nothing" vs "knows everything", output explanatory content vs short reference.
5. **Memory-aware**: when hippocampus has a relevant episodic trace, the output references it.

## Phases

### Phase 2A — Skeleton (this turn)
- Write the data structure header
- Build the 9 stage functions as **real reads from existing modules** (not no-ops) but with simple combine logic
- Wire to a new public API + RPC
- Build green, basic unit test
- **Goal:** end-to-end data flow exists; output is bridge-quality but goes through the cascade frame

### Phase 2B — Real cognitive contributions (week 1-2)
- Stage 1: pull actual drive state, weight intent by (urgency × valence)
- Stage 2: PFC goals → speech_act_type classification; WM contents → topic concepts
- Stage 3: ToM `tom_get_agent_state()` → listener belief/emotion bias
- Stage 4: Hippocampus `hippo_find_similar_episodes` on the comprehend vector → episodic concept IDs
- Stage 5: weighted sum: `intent = α·comprehend + β·drive + γ·goal + δ·listener + ε·episodic`
- Test: state-sensitivity acceptance criterion

### Phase 2C — Lexical + syntactic (week 2-3)
- GL→Broca lexicon mirror (POS extraction from WordNet, push to Broca)
- Stage 6: bridge selects words from `content_intent`
- Stage 7: Broca's syntax processor reorders, validates agreement
- Test: multi-word grammatical output criterion

### Phase 2D — Validation + iteration (week 3-4)
- Lang_eval through cascade
- Compare to bridge-only baseline
- Per-stage diagnostics dashboard
- Acceptance-criteria run

### Phase 2E — Speech (FNO/HNN/motor) (week 4-6)
- Stage 8: prosody (FNO contour, HNN-validated coherence)
- Stage 9: speech motor → audio output (or text rendering)

## What this commits us to

This is a **multi-week, multi-region build**. It will not produce communicative output in days. Each phase has a verifiable milestone that signals progress. If a phase's acceptance criterion fails, we stop and diagnose before adding the next layer.

The honest scope:
- Phase 2A: skeleton in 1-2 days
- Phase 2B-D: 3-4 weeks
- Phase 2E: 2-3 weeks
- **Total: 6-8 weeks for a first cascade demo.**

After that, training on real corpora (with cascade firing through the trainer hooks) is what produces *good* communication, not the cascade architecture itself. That's another 2-3 months minimum.

## What we are NOT doing

- Not falling back to LLM-based generation. The cascade IS the communication system.
- Not adding new neural networks. All the substrate exists; we're orchestrating it.
- Not training on synthetic conversation data. The brain learns from real grounded experience.
- Not waiting for "perfect" before iterating. Each phase ships something testable.

## Risks

1. **Stage 5 weight tuning** could turn into a nightmare of hyperparameter tuning. Mitigation: start with equal weights, only tune if Phase 2D shows specific failure modes.

2. **Broca's lexicon mirror** might surface that Broca's CYK rules don't cover modern English well. Mitigation: Phase 2C has a sub-experiment to verify the chart actually builds; if not, we audit Broca's grammar rules.

3. **Cascade overhead** could make produce calls slow (~50-100ms vs current ~10ms). Mitigation: per-stage timing in diagnostics; fast-path option that skips low-priority stages when drive is below threshold.

4. **The skeleton showing "no obvious change in output"** is a real risk during Phase 2A — if the cascade is plumbed but stages don't yet meaningfully diverge from bridge-only, output will look the same. This is expected; Phase 2B is where divergence appears.
