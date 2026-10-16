/**
 * @file nimcp_communication_cascade.h
 * @brief Multi-region production cascade for language output.
 *
 * Reframes language production as a coordinated multi-region cascade,
 * not a bridge softmax. Each stage reads state from one or more
 * cognitive modules (hypothalamus, PFC, WM, ToM, hippocampus, semantic
 * memory) and contributes a section of the production_cascade_state_t.
 *
 * Phase 2A skeleton: all 9 stages exist; intent-formation stages (1-5)
 * read real module state and combine into a weighted content_intent
 * vector; output stages (6-9) currently pass through to the SNN
 * language bridge while we build the GL→Broca lexicon mirror (Phase
 * 2C). Each stage is independently testable and can be enabled or
 * disabled via the cascade config.
 *
 * Design: see docs/claude/communication-cascade-plan.md.
 */
#ifndef NIMCP_COMMUNICATION_CASCADE_H
#define NIMCP_COMMUNICATION_CASCADE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/broca/nimcp_pragmatics_processor.h"  /* speech_act_type_t */
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h" /* retrieval_result_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl — full struct lives in nimcp_brain_internal.h. */
struct brain_struct;
typedef struct brain_struct* brain_t;

/* Forward decl — full struct lives in nimcp_snn_language_bridge.h.
 * Cascade Stage 11 (self-train) calls the bridge's plasticity API. */
struct snn_language_bridge;

/* Per-stage enable bits. Default (0) = all stages on. */
typedef enum {
    CASCADE_STAGE_WERNICKE      = 1u << 0,  /* Stage 0 — input comprehension */
    CASCADE_STAGE_DRIVE         = 1u << 1,
    CASCADE_STAGE_GOAL          = 1u << 2,
    CASCADE_STAGE_LISTENER      = 1u << 3,
    CASCADE_STAGE_EPISODIC      = 1u << 4,
    CASCADE_STAGE_CONTENT       = 1u << 5,
    CASCADE_STAGE_LEXICAL       = 1u << 6,
    CASCADE_STAGE_SYNTACTIC     = 1u << 7,
    CASCADE_STAGE_SELF_COMP     = 1u << 8,  /* Stage 8 (Phase 2D-B) — Wernicke validates own output */
    CASCADE_STAGE_PHONOLOGICAL  = 1u << 9,
    CASCADE_STAGE_MOTOR         = 1u << 10,
    CASCADE_STAGE_SELF_FEEDBACK = 1u << 11, /* Stage 9 — write produced utterance back to WM + cognitive bus */
    CASCADE_STAGE_SPEECH_REPAIR = 1u << 12, /* Stage 10 (Item 5) — perturbation-retry on low self_match */
    CASCADE_STAGE_PROSODY       = 1u << 13, /* Wave 2 Item 9 — FNO-shaped prosodic contour (F0/dur/intensity) */
    CASCADE_STAGE_SELF_TRAIN    = 1u << 14, /* Stage 11 (Wave 2 Item #10) — reward-modulated bridge learning */
    CASCADE_STAGE_ALL           = 0x7FFF /* bits 0..14 */
} cascade_stage_mask_t;

/* Single struct accumulates stage outputs. Allocated by caller; the
 * orchestrator allocates content_intent and utterance, owns them, and
 * frees them on cleanup. */
typedef struct {
    /* Stage 0: Wernicke comprehension of input prompt — phrase
     * structure analysis, speech-act classification, ambiguity detection.
     * Empty/zero values when prompt is NULL or Wernicke isn't attached. */
    bool     wernicke_parsed;          /* true if syntactic_parse_sentence succeeded */
    bool     prompt_is_question;       /* derived from wh-words / aux inversion / "?" */
    bool     prompt_is_imperative;     /* missing-subject + leading verb */
    bool     prompt_is_garden_path;    /* set when parser flagged ambiguity */
    uint32_t prompt_word_count;        /* # words after tokenization */
    float    prompt_complexity;        /* 0..1, normalized syntactic complexity */
    /* Subject / verb / object slots — populated when the parse identifies
     * thematic roles. Each is either a tokenized word string from the
     * prompt or "" if the role wasn't filled. Phase 2D-A v1 uses simple
     * head-finding rather than full thematic-role assignment. */
    char     prompt_subject[32];
    char     prompt_verb[32];
    char     prompt_object[32];

    /* Stage 1: Drive — read from hypothalamus / insula / amygdala */
    float    drive_magnitude;     /* 0..1, strength of urge to communicate */
    float    drive_valence;       /* -1..1, approach vs avoid */
    float    drive_arousal;       /* 0..1, calm vs urgent */
    uint8_t  dominant_drive;      /* hypo_drive_type_t value */

    /* Stage 2: Goal — read from PFC + WM */
    speech_act_type_t act_type;
    uint64_t target_concept_id;       /* primary concept being expressed */
    uint64_t topic_concept_ids[8];    /* related concepts from WM + goals */
    uint32_t topic_count;
    float    goal_priority;            /* 0..1 from PFC top goal */

    /* Stage 3: Listener — read from ToM */
    bool     listener_known;
    float    listener_belief_confidence; /* tom_belief_t.confidence */
    float    listener_emotion_valence;   /* simplified from tom_emotion_t */
    float    audience_familiarity;       /* 0..1 */

    /* Stage 4: Episodic — read from hippocampus (similarity search) */
    uint64_t episodic_concept_ids[16];
    float    episodic_relevances[16];
    uint32_t episodic_count;
    /* Phase 2B: actual retrieved memories. Owned by cascade state —
     * cascade_state_cleanup frees memories[] and similarities[] inside.
     * Used by stage_content to lift feature vectors into the intent. */
    retrieval_result_t episodic_retrieval;

    /* Stage 5: Content — combined intent vector that drives the bridge */
    float*   content_intent;       /* allocated; semantic_dim entries */
    uint32_t content_dim;
    float    content_confidence;   /* 0..1 — how cohered the cascade is */

    /* Stages 6-9: filled by lexical/syntactic/phonological/motor */
    char*    utterance;            /* allocated; final text */
    uint32_t word_count;
    float    fluency;
    float    syntactic_validity;   /* 0 if Broca rejected, 1 if grammatical */

    /* Stage 8 (Phase 2D-B): Wernicke validation of brain's own output —
     * the sensorimotor loop. After Broca produces, we comprehend the
     * utterance via Wernicke + grounded_language_comprehend and compare
     * the derived semantic vector to content_intent. High match = the
     * production path successfully encoded the intent. Low match = the
     * brain said something but it's not what it meant. */
    bool     self_parsed;             /* true if Wernicke parse_sentence succeeded */
    float    self_complexity;         /* 0..1, syntactic complexity of own output */
    float    self_match;              /* 0..1, cosine sim between intent and re-comprehended utterance */
    float    self_grammaticality;     /* 0 / 0.5 / 1.0 — derived from parse + grammaticality flag */

    /* Diagnostics */
    uint32_t stages_completed;
    uint32_t stages_failed;
    uint32_t stages_skipped;       /* per-stage skip mask */
    char     failure_reason[128];

    /* Pragmatics — APPENDED at end of struct to avoid ABI shift on
     * existing fields. Set by cascade Stage 2 (Goal) when Broca's
     * pragmatics processor classifies the prompt as an indirect speech
     * act ("Can you pass the salt?" surface=question, indirect=REQUEST).
     * When true, act_type is overridden to SPEECH_ACT_REQUEST or
     * SPEECH_ACT_COMMAND so downstream stages produce a request-shaped
     * response rather than a literal yes/no answer. */
    bool     pragmatic_is_indirect;

    /* Stage 10 (Item 5) — speech repair retry. APPENDED at end of struct
     * to avoid ABI shift on existing fields. When self_match (Phase 2D-B)
     * falls below a threshold, the orchestrator re-runs lexical+syntactic
     * +self_comp with a small Gaussian perturbation to content_intent and
     * keeps the best-scoring candidate. Diagnostics:
     *   repair_attempts   — how many retry rounds were executed (0..N)
     *   best_self_match   — best score seen across retries (>= original)
     *   best_utterance    — heap-allocated copy of best-scoring utterance;
     *                       owned by cascade state, freed in cleanup. */
    uint32_t repair_attempts;
    float    best_self_match;
    char*    best_utterance;

    /* Stage 9 (Wave 2 Item 7) — phonological output. APPENDED at end of
     * struct to avoid ABI shift on existing fields. Converts state->utterance
     * (Broca-rendered text from Stage 7) into a phoneme sequence using a
     * lightweight rule-based English G2P, then runs the sequence through
     * Broca's phonological_processor_t to obtain syllable diagnostics.
     *
     * Diagnostics:
     *   phoneme_count     — number of phonemes emitted (0 when skipped)
     *   phoneme_sequence  — heap-allocated array of uint8_t phoneme codes
     *                       (ASCII-letter encoding: 'a'..'z' for vowels +
     *                       consonants, ' ' for word boundaries, plus
     *                       digraph sentinels '$' (sh), '&' (ch), '#' (th),
     *                       '@' (ng), '%' (ph→f)). Owned by cascade state,
     *                       freed in cleanup.
     *   syllable_count    — syllables generated by the phonological processor
     *                       (0 if syllabification skipped or failed).
     *   phon_voiced_ratio — fraction of phonemes flagged voiced (0..1);
     *                       a coarse diagnostic for downstream prosody. */
    uint32_t phoneme_count;
    uint8_t* phoneme_sequence;
    uint32_t syllable_count;
    float    phon_voiced_ratio;

    /* Stage 11 (Wave 2 Item 9) — prosodic contour. APPENDED at end of
     * struct to avoid ABI shift on existing fields. Maps the cascade's
     * emotional/syntactic feature vector (drive_arousal, drive_valence,
     * act_type, prompt_is_question, self_grammaticality) onto a
     * per-syllable F0/duration/intensity contour using a spectral
     * (FNO-style) basis. Intent → contour rules:
     *   - drive_arousal     ↑ → wider pitch range (Hz_peak - Hz_floor)
     *   - drive_valence     ↑ → higher baseline F0 (approach prosody)
     *   - act_type=QUESTION → final-rise contour (last syllable +30%)
     *   - act_type=COMMAND  → emphasis-front, falling tail
     *   - act_type=DECLARE  → declination (gradual fall)
     *   - self_grammaticality → confidence boost on intensity dB
     *
     * Allocated arrays are owned by the cascade state — cleanup frees them.
     * All three vectors are sized to prosody_syllable_count entries.
     *
     * Diagnostics:
     *   prosody_pitch_hz       — per-syllable F0 in Hz (typical 80..400)
     *   prosody_duration_ms    — per-syllable duration in ms (typical 50..200)
     *   prosody_intensity_db   — per-syllable amplitude in dB (typical 50..90)
     *   prosody_syllable_count — number of syllables in contour (0 when skipped)
     *   prosody_mean_f0        — mean of pitch_hz, summary diagnostic
     *   prosody_pitch_range    — max(pitch_hz) - min(pitch_hz), summary diagnostic */
    float*   prosody_pitch_hz;
    float*   prosody_duration_ms;
    float*   prosody_intensity_db;
    uint32_t prosody_syllable_count;
    float    prosody_mean_f0;
    float    prosody_pitch_range;

    /* Stage 12 (Wave 2 Item #10) — reward-modulated SNN bridge training.
     * APPENDED at end of struct to avoid ABI shift on existing fields.
     * When the runtime flag brain->cascade_self_train_enabled is true AND
     * stage_self_comprehension produced a valid self_match, the cascade
     * computes reward = (self_match - baseline), updates the per-brain EMA
     * baseline, and applies snn_language_bridge_echo_correct() to each
     * produced word with lr_scale proportional to the reward sign and
     * magnitude. Diagnostics surface here for trainers + tests:
     *   train_applied — true iff the bridge plasticity hook ran.
     *   train_reward  — the reward signal that was actually applied
     *                   (self_match - baseline_before_update). May be
     *                   negative when the brain underperforms its
     *                   recent average. */
    bool  train_applied;
    float train_reward;

    /* Speech-repair (disfluency cleaner) — APPENDED at end of struct.
     * The orchestrator runs Broca's speech_repair_clean() on the brain's
     * own utterance just before Stage 8 (self-comprehension) so the
     * self_match score reflects intended content, not production noise.
     *   utterance_pre_repair  — heap-allocated copy of utterance BEFORE
     *                           cleaning (NULL when no repair was applied
     *                           or no utterance existed). Owned by the
     *                           cascade state, freed in cleanup.
     *   speech_repair_applied — true iff speech_repair_clean produced a
     *                           non-identical output and state->utterance
     *                           was replaced. */
    char* utterance_pre_repair;
    bool  speech_repair_applied;

    /* === SLICE 3 — FEP PREDICTION-ERROR HOOKS (2026-05-18) ===
     * APPENDED at end of struct to avoid ABI shift on existing fields.
     *
     * Predictive-coding view of the cascade: each major stage maintains
     * an implicit model of what its inputs SHOULD look like given the
     * cognitive state. Once the stage runs and observes its actual input,
     * we record the L2 norm of (actual - predicted) as a per-stage
     * prediction-error scalar. High prediction error = "the upstream
     * cognitive state didn't match what this stage expected" — a
     * surprise signal. Friston's Free-Energy Principle: precision-weighted
     * prediction error drives plasticity.
     *
     * Predictions are deliberately TRIVIAL at the start of this slice
     * (mean / identity / persistence-from-prior-iter). The SIGNAL we care
     * about is the SHAPE of pe over recurrent iterations: it should
     * climb in early iterations (the system is surprised by its own
     * outputs) and fall as the recurrent loop settles (the system
     * predicts its own next-iter inputs well). Slice 3 wires the
     * machinery; later slices replace trivial predictors with population-
     * dynamic models.
     *
     * Fields:
     *   pe_content_norm    — Stage Content predicts what content_intent
     *                        WOULD look like from drives + episodic +
     *                        listener + goal (no arcuate feedback). The
     *                        observed content_intent is the actual, with
     *                        arcuate feedback applied. PE = ‖actual -
     *                        predicted‖ / sqrt(dim). Climbs when arcuate
     *                        feedback is strong (lots to correct), falls
     *                        toward zero as the recurrent loop converges.
     *   pe_lexical_norm    — Stage Lexical predicts that its output's
     *                        semantic vector (re-comprehended) matches
     *                        the input content_intent. PE = ‖content_intent
     *                        - (cosine-aligned echo of bridge output)‖ /
     *                        sqrt(dim). On a settled cascade this drops
     *                        as the bridge produces utterances closer to
     *                        the intent.
     *   pe_syntactic_norm  — Stage Syntactic predicts that Broca will
     *                        accept the bridge's word sequence. PE =
     *                        (1.0 - syntactic_validity) when stage ran,
     *                        else 0. Trivially derived but kept as its
     *                        own field for downstream consumers.
     *   pe_self_comp_norm  — Stage Self-Comp's prediction is "my own
     *                        utterance comprehends back to content_intent."
     *                        PE = (1.0 - self_match) when self_parsed,
     *                        else 1.0 (full surprise — the brain
     *                        couldn't even parse its own output).
     *   pe_total           — Sum of finite per-stage norms; the
     *                        "surprise budget" of this iteration.
     *   fep_iteration      — Which recurrent iteration this state came
     *                        from (0 for single-pass cascade). Lets a
     *                        consumer plot PE-vs-iter trajectories.
     *   fep_precision      — Precision weighting applied to PE when
     *                        scaling plasticity in stage_self_train.
     *                        Default 1.0; the recurrent loop bumps it
     *                        on high-surprise iterations so the bridge
     *                        learns FASTER from surprising inputs (FEP:
     *                        precision is the inverse-variance estimate). */
    float    pe_content_norm;
    float    pe_lexical_norm;
    float    pe_syntactic_norm;
    float    pe_self_comp_norm;
    float    pe_total;
    uint32_t fep_iteration;
    float    fep_precision;

    /* === SLICE 7 — CEREBELLAR PREDICTION-CORRECTION (2026-05-18) ===
     * APPENDED at end of struct to avoid ABI shift on existing fields.
     *
     * The cerebellum acts as a forward-model predictor for motor +
     * prosody trajectories. Each affected stage builds an 8-element
     * feature vector (the cerebellum adapter's native motor_command
     * width is 8 dimensions — see motor_command[8] in nuclei_output_t)
     * BEFORE running the stage's main body, calls
     * cerebellum_predict_outcome, runs the stage, builds the realised
     * "actual" vector, calls cerebellum_update_forward_model +
     * cerebellum_broadcast_error so Purkinje LTD learns from the
     * mismatch.
     *
     * Fields are populated only when brain->cerebellar_correction_enabled
     * is true AND brain->cerebellum is non-NULL; otherwise they remain
     * zero (cascade_state is calloc-zeroed by communication_cascade_run).
     * That preserves the slice's default-OFF byte-identical contract.
     *
     *   cereb_motor_predicted[8]   — last predicted motor vector from
     *                                cerebellum_predict_outcome inside
     *                                cascade_stage_motor.
     *   cereb_motor_actual[8]      — realised motor vector built post-stage.
     *   cereb_motor_pe_norm        — ‖actual - predicted‖ / sqrt(8) for
     *                                the motor stage; 0 when skipped.
     *   cereb_prosody_predicted[3] — last predicted (mean_F0,
     *                                mean_duration, mean_intensity) bias
     *                                from cerebellum, projected to the
     *                                prosody stage's 3 summary metrics.
     *   cereb_prosody_actual[3]    — realised (mean_F0, mean_dur, mean_int)
     *                                summary after stage_prosody finishes.
     *   cereb_prosody_pe_norm      — ‖actual - predicted‖ / sqrt(3) for
     *                                prosody stage; 0 when skipped.
     *   cereb_correction_applied   — true on iters where motor/prosody
     *                                stages consumed correction_pending and
     *                                applied the cerebellar prediction bias
     *                                at correction_strength.
     *   cereb_predictions_made     — count of forward-model calls in this
     *                                cascade run (typically 0 or 2 — motor
     *                                + prosody).
     */
    float    cereb_motor_predicted[8];
    float    cereb_motor_actual[8];
    float    cereb_motor_pe_norm;
    float    cereb_prosody_predicted[3];
    float    cereb_prosody_actual[3];
    float    cereb_prosody_pe_norm;
    bool     cereb_correction_applied;
    uint32_t cereb_predictions_made;

    /* S1-C1 fix: recurrent-loop settling steps. Distinct from
     * repair_attempts which counts speech-repair retries. Pre-fix the
     * recurrent loop OVERWROTE repair_attempts with its iteration count,
     * corrupting any reader that expected repair-retry semantics.
     * Owner: communication_cascade_run_recurrent writes this to the
     * number of recurrent passes (iter+1) it executed before
     * convergence or max_iters. Reader: nimcp_cascade_diag_full_t /
     * Python diag-dict surfaces it as 'settling_steps' alongside
     * 'repair_attempts'. APPENDED at end of struct. */
    uint32_t settling_steps;

    /* S3+S6-H2/H4 fix (2026-05-19) — split pe_content_norm telemetry.
     * Pre-fix pe_content_norm was recorded against a snapshot taken
     * pre-arcuate-AND-pre-gate, so the value folded together two
     * unrelated effects:
     *   ||gate * (pre + k*arcuate) - pre||
     * In particular a thalamic gate of 0.5 with ZERO arcuate feedback
     * reported a nonzero "PE" that was just attenuation, misleading the
     * Slice 3 precision update into bumping fep_precision on
     * gate-toggles. Likewise, toggling the gate mid-recurrent-run
     * polluted pe_total with gating-noise.
     *
     * Post-fix: take TWO snapshots. pe_content_arcuate_norm measures
     * the L2 of (post-arcuate - pre) — the FEP prediction-error part.
     * pe_content_gate_norm measures the L2 of (post-gate - post-arcuate)
     * — pure thalamic attenuation, not a prediction error.
     *
     * The legacy pe_content_norm is kept for ABI compatibility and now
     * holds the SAME value as pe_content_arcuate_norm (semantically
     * "the FEP-relevant slice of the stage's effect"), which is what
     * pe_total has always wanted. */
    float    pe_content_arcuate_norm;
    float    pe_content_gate_norm;

    /* S7-H2 fix (2026-05-19): per-stage flags surfacing whether the
     * cerebellar bias was actually applied this iter (gated on
     * correction_pending AND prediction confidence > 0.3 — pre-training
     * the forward model is identity so a 0-confidence bias would just
     * map voicing onto pitch). Also record the predict-call confidence
     * itself so trainers can monitor forward-model learning curve.
     * APPENDED at end. Layout (struct already 8-byte aligned at offset
     * 904 from pe_content_gate_norm = 4-byte float at 900): two bools
     * (2 bytes), 6 bytes trailing pad to 4-byte align, two floats
     * (8 bytes) = 16 bytes total → struct size 920. */
    bool     cereb_motor_bias_applied;
    bool     cereb_prosody_bias_applied;
    float    cereb_motor_confidence;
    float    cereb_prosody_confidence;

    /* === SLICE D — CASCADE SELF-TRAIN EXTERNAL-REWARD GATING (2026-05-19) ===
     * Per-call tunables read by cascade_stage_self_train. The orchestrator
     * copies brain->cascade_self_train_reward_threshold and
     * brain->cascade_self_train_reward_ttl_us into these fields before
     * invoking the stage. Zero (calloc default) means "use built-in
     * fallback" (threshold=0.5, ttl_us=5,000,000 µs / 5s).
     *
     *   cascade_self_train_reward_threshold — reward must be >= this for
     *       the self-train body to run. Negative-reward (punishment) is
     *       always below threshold and never fires plasticity.
     *   reward_ttl_us — staleness cutoff. (now - brain->last_external_reward_us)
     *       must be <= ttl_us for the reward to count as "fresh".
     *
     * APPENDED at end of struct — ABI append-only; sentinel bumped below. */
    float    cascade_self_train_reward_threshold;
    uint64_t reward_ttl_us;
} production_cascade_state_t;

/* Walkthrough-4 audit M MEDIUM #6 — ABI size sentinels.
 *
 * production_cascade_state_t and nimcp_cascade_diag_full_t /
 * nimcp_cascade_counters are accessed BY FIELD from multiple TUs
 * (cascade.c, cascade_wernicke.c, api/nimcp_part_core.c, the Python
 * binding shadow struct in nimcp_python.c). A stale TU compiled
 * against an older header offset would read garbage; cascade_state_cleanup
 * could free past the buffer.
 *
 * If you append a field, recompute by adding sizeof(field) + alignment
 * padding and bump the literal. The compiler will hard-fail any TU
 * that disagrees, which is the entire point.
 *
 * Computed on x86_64 Linux gcc with default packing. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(production_cascade_state_t) == 928,
    "production_cascade_state_t ABI: size drifted from expected 928. "
    "Append-only on the struct; bump this literal in lockstep. "
    "Slice 3 (2026-05-18) appended 6 prediction-error fields (24 bytes) "
    "atop the 760-byte baseline → 784. Slice 7 (2026-05-18) appended "
    "cerebellar prediction-correction fields (104 bytes — cereb_motor_* / "
    "cereb_prosody_* / cereb_correction_applied / cereb_predictions_made) "
    "→ 888. S1-C1 fix (2026-05-18) appended uint32_t settling_steps "
    "(4 bytes data + 4 bytes tail alignment padding to keep struct align) "
    "→ 896. S3+S6-H2/H4 fix (2026-05-19) appended pe_content_arcuate_norm "
    "+ pe_content_gate_norm (8 bytes, no padding needed — both float, "
    "struct already 8-byte aligned at offset 896) → 904. S7-H2 fix "
    "(2026-05-19) appended cereb_motor_bias_applied + "
    "cereb_prosody_bias_applied + cereb_motor_confidence + "
    "cereb_prosody_confidence (2 bools + 2 floats — packed into 8 bytes "
    "via slot reuse in earlier trailing pad) → 912. Slice D (2026-05-19) "
    "appended cascade_self_train_reward_threshold (float, 4 bytes, "
    "offset 912) + reward_ttl_us (uint64_t, requires 8-byte alignment so "
    "4 bytes padding before, offset 920, 8 bytes) → 928.");
#endif

/**
 * @brief Run the full 9-stage production cascade and produce an utterance.
 *
 * @param brain          Brain handle (internal pointer).
 * @param prompt_or_null Optional input text. If non-null, the prompt is
 *                       comprehended first and seeds stage 1's intent
 *                       vector — mimics responding to a question. If
 *                       null, the cascade runs purely from internal
 *                       state — mimics spontaneous speech.
 * @param stage_mask     Bitmask of cascade_stage_mask_t enabling
 *                       specific stages. 0 = CASCADE_STAGE_ALL.
 * @param out_state      Caller-owned struct populated by the cascade.
 *                       Free with cascade_state_cleanup().
 *
 * @return 0 on success; -1 on fatal failure. Per-stage failures are
 *         non-fatal — they're recorded in out_state->stages_failed and
 *         the cascade continues with whatever signal is available.
 */
int communication_cascade_run(
    brain_t brain,
    const char* prompt_or_null,
    uint32_t stage_mask,
    production_cascade_state_t* out_state);

/**
 * @brief Biological-fidelity recurrent variant — iterates the cascade
 *        until the utterance and self_match converge, mimicking the
 *        settling dynamics of real cortex.
 *
 * Calls communication_cascade_run() up to @p max_iters times. After
 * each iteration, compares the produced utterance string + self_match
 * to the previous iteration's values. Convergence is reached when
 * BOTH:
 *   - utterance is byte-identical to the previous iteration
 *   - |self_match - prev_self_match| <= @p self_match_eps
 *
 * Between iterations, the bridge's plasticity from stage_self_train
 * (when enabled) is the load-bearing change: each iteration's STDP
 * shifts the bridge's bindings slightly, the next cascade run reads
 * the modified bridge, the system settles toward an attractor.
 *
 * This is Slice 1 of the recurrent-language-architecture rewrite — it
 * establishes the iteration scaffold without changing per-stage
 * semantics. Subsequent slices wire bidirectional Wernicke↔Broca,
 * FEP prediction-error hooks, lateral inhibition, etc.
 *
 * @param brain           the brain
 * @param prompt_or_null  same as communication_cascade_run
 * @param max_iters       max iterations (default 8 if 0 passed)
 * @param self_match_eps  convergence tolerance (default 0.01)
 * @param out_state       caller-owned; populated with final iteration's state
 * @return                0 on success; -1 on fatal failure of any iteration.
 *                        out_state->repair_attempts is *additionally* bumped
 *                        per iteration past the first so callers can see how
 *                        many settling steps the system took.
 */
int communication_cascade_run_recurrent(
    brain_t brain,
    const char* prompt_or_null,
    uint32_t max_iters,
    float self_match_eps,
    production_cascade_state_t* out_state);

/** Free heap-owned fields in the cascade state. */
void cascade_state_cleanup(production_cascade_state_t* state);

/**
 * @brief Tier 1 Step F1 — surface polish on the final utterance.
 *
 * Capitalizes the first letter of state->utterance and appends terminal
 * punctuation (`?` if the utterance's leading word is a wh-word/auxiliary,
 * else `.`), rewriting state->utterance in place. Idempotent w.r.t.
 * existing terminal punctuation. No-op on NULL/empty. Called as the last
 * step of communication_cascade_run; exposed for direct unit testing.
 */
void cascade_apply_surface_polish(production_cascade_state_t* state);

/**
 * @brief Wave 2 Item #10 — toggle the cascade self-training hook.
 *
 * When enabled, the orchestrator runs Stage 11
 * (cascade_stage_self_train) after Stage 8 self-comprehension and
 * before Stage 9 self-feedback. Stage 11 calls the SNN language
 * bridge's echo-correct plasticity API on every produced word with
 * a learning rate proportional to (self_match - baseline), driving
 * three-factor learning where the bridge's per-binding eligibility
 * traces are the synaptic memory, dopamine is the bridge's optional
 * neuromodulator gain, and (self_match - baseline) is the reward
 * prediction error.
 *
 * Default OFF. Maps directly to brain->cascade_self_train_enabled.
 * Returns 0 on success, -1 on invalid brain pointer.
 */
int communication_cascade_set_self_train_enabled(brain_t brain, bool enabled);

/** Read the self-train flag. Returns false on NULL brain. */
bool communication_cascade_get_self_train_enabled(brain_t brain);

/**
 * @brief Configure the self-train EMA + LR-scale tunables.
 *
 * @param alpha    EMA mixing rate for the running baseline; clamped to
 *                 [0, 1]. Default 0.05 (slow averaging — ~20-call window).
 *                 0 freezes the baseline at its current value; 1 makes
 *                 every call its own baseline (reward becomes 0).
 * @param lr_scale Multiplier applied to echo_correct lr_scale. Clamped
 *                 to [0, 10]. Default 1.0. Set < 1 to attenuate early
 *                 training, > 1 for fast imprinting (with caveats —
 *                 large values amplify negative-reward LTD via the
 *                 LTD branch of strengthen_binding).
 *
 * Returns 0 on success, -1 on invalid brain pointer.
 */
int communication_cascade_set_self_train_tunables(brain_t brain,
                                                    float alpha,
                                                    float lr_scale);

/**
 * @brief Wave 2 Item #10 — pure helper that applies the reward signal.
 *
 * Exposed for testability — the production cascade orchestrator calls
 * this internally as Stage 11, but unit tests can drive it directly
 * against a bridge + handcrafted utterance + intent. No phantom-API
 * risk: the helper composes only existing bridge APIs
 * (snn_language_bridge_echo_correct) plus a memory-safe tokenizer.
 *
 * @param bridge     SNN language bridge with concept-word bindings.
 *                   May be NULL — returns 0 (no-op) with *out_reward=0.
 * @param intent     Content intent vector that drove the produce.
 * @param intent_dim Length of intent.
 * @param utterance  Whitespace-separated produced text. NULL or empty
 *                   skips with *out_reward=0.
 * @param self_match Phase 2D-B re-comprehension cosine. NaN/Inf skip.
 * @param baseline_inout  In/out EMA baseline. Read once to compute the
 *                        reward, then updated by alpha-mixing self_match.
 * @param alpha      EMA mixing rate, clamped to [0,1].
 * @param lr_scale   Multiplier applied to echo_correct lr_scale.
 * @param out_reward Optional — set to the reward signal that was applied
 *                   (self_match - baseline_before_update). Always defined
 *                   when the call returns; 0 when the call short-circuits.
 *
 * Returns the number of bindings strengthened across all produced words,
 * or 0 when nothing fired (skip path). Never returns negative.
 */
int cascade_apply_self_train_reward(
    struct snn_language_bridge* bridge,
    const float* intent,
    uint32_t intent_dim,
    const char* utterance,
    float self_match,
    float* baseline_inout,
    float alpha,
    float lr_scale,
    float* out_reward);

/* ---------- Full diagnostic snapshot for Python/RPC introspection. ----------
 *
 * The legacy diag impl exposes only 8 of ~50 cascade fields. Trainers and
 * monitoring scripts need the rest: per-stage drives, listener inference,
 * episodic/content state, self_match, repair retries, prosody contours,
 * train reward, failure reasons. This struct captures ALL scalars; the
 * companion impl ALSO malloc-copies the per-syllable / per-phoneme arrays
 * into out-pointers that the caller frees.
 *
 * Field order is contiguous-by-stage for readability — it is NOT an ABI
 * contract. Callers should access by name. New fields APPEND at the end. */
typedef struct {
    /* Stage 0 — Wernicke input comprehension */
    int      wernicke_parsed;
    int      prompt_is_question;
    int      prompt_is_imperative;
    int      prompt_is_garden_path;
    uint32_t prompt_word_count;
    float    prompt_complexity;
    char     prompt_subject[32];
    char     prompt_verb[32];
    char     prompt_object[32];

    /* Stage 1 — Drive */
    float    drive_magnitude;
    float    drive_valence;
    float    drive_arousal;
    uint8_t  dominant_drive;

    /* Stage 2 — Goal + Pragmatics */
    uint8_t  act_type;
    int      pragmatic_is_indirect;
    uint32_t topic_count;
    float    goal_priority;

    /* Stage 3 — Listener (ToM) */
    int      listener_known;
    float    listener_belief_confidence;
    float    listener_emotion_valence;
    float    audience_familiarity;

    /* Stage 4 — Episodic */
    uint32_t episodic_count;

    /* Stage 5 — Content intent */
    uint32_t content_dim;
    float    content_confidence;

    /* Stages 6-7 — Lexical + Syntactic */
    uint32_t word_count;
    float    fluency;
    float    syntactic_validity;

    /* Stage 8 — Self-comprehension */
    int      self_parsed;
    float    self_complexity;
    float    self_match;
    float    self_grammaticality;

    /* Stage 9 — Phonological */
    uint32_t phoneme_count;
    uint32_t syllable_count;
    float    phon_voiced_ratio;

    /* Stage 12 — Speech repair */
    uint32_t repair_attempts;
    float    best_self_match;

    /* Stage 13 — Prosody (scalar summaries; arrays in out-pointers below) */
    uint32_t prosody_syllable_count;
    float    prosody_mean_f0;
    float    prosody_pitch_range;

    /* Stage 14 — Self-train */
    int      train_applied;
    float    train_reward;

    /* Diagnostics */
    uint32_t stages_completed;
    uint32_t stages_failed;
    uint32_t stages_skipped;
    char     failure_reason[128];

    /* SLICE 3 — FEP prediction-error scalars per stage. APPENDED at end
     * of the diag struct to keep the binding shadow stable for older
     * consumers. New consumers can read these fields by name. See the
     * production_cascade_state_t docstring above for the meaning of each. */
    float    pe_content_norm;
    float    pe_lexical_norm;
    float    pe_syntactic_norm;
    float    pe_self_comp_norm;
    float    pe_total;
    uint32_t fep_iteration;
    float    fep_precision;

    /* S1-C1 fix: distinguish recurrent-loop settling from speech-repair
     * retries. repair_attempts above is owned by Stage 12 (speech repair);
     * settling_steps is the number of recurrent passes the recurrent
     * cascade ran before convergence or max_iters. APPENDED at end. */
    uint32_t settling_steps;

    /* S3+S6-H2/H4 fix (2026-05-19) — split pe_content_norm telemetry.
     * pe_content_norm above now equals pe_content_arcuate_norm. The
     * separate gate-only norm is surfaced so consumers can attribute
     * "movement in content_intent" to FEP-correction vs thalamic
     * attenuation. APPENDED at end. */
    float    pe_content_arcuate_norm;
    float    pe_content_gate_norm;
} nimcp_cascade_diag_full_t;

/**
 * @brief Run cascade and snapshot ALL state into the diag struct + arrays.
 *
 * @param brain                Brain handle (internal pointer).
 * @param prompt_or_null       Same as communication_cascade_run.
 * @param out_utterance        Caller buffer for the final utterance text;
 *                             may be NULL if caller doesn't need it.
 * @param out_text_max         Capacity of out_utterance.
 * @param out_best_utterance   Caller buffer for the speech-repair best
 *                             candidate (NULL if no repair fired).
 * @param out_best_max         Capacity of out_best_utterance.
 * @param out                  Populated with all scalars on rc=0.
 * @param out_phoneme_sequence If non-NULL, set to a heap-allocated copy
 *                             of state->phoneme_sequence sized
 *                             out->phoneme_count bytes. **Caller frees
 *                             via nimcp_free**. Set to NULL when
 *                             phoneme_count is 0.
 * @param out_prosody_pitch_hz / out_prosody_duration_ms / out_prosody_intensity_db
 *                             Same ownership semantics — heap-allocated
 *                             arrays sized out->prosody_syllable_count
 *                             floats each. **Caller frees via nimcp_free**.
 *                             NULL when prosody_syllable_count is 0.
 *
 * @return 0 on success; -1 on fatal failure. On failure all out-arrays
 *         remain NULL and the struct is zero-initialized.
 */
int nimcp_brain_produce_cascade_diag_full_impl(
    brain_t brain,
    const char* prompt_or_null,
    char* out_utterance,
    uint32_t out_text_max,
    char* out_best_utterance,
    uint32_t out_best_max,
    nimcp_cascade_diag_full_t* out,
    uint8_t** out_phoneme_sequence,
    float**   out_prosody_pitch_hz,
    float**   out_prosody_duration_ms,
    float**   out_prosody_intensity_db);

/* ----------------------------------------------------------------------
 * Cascade telemetry counters (Batch K).
 *
 * The diag struct above captures a per-RUN snapshot. These counters are
 * LIFETIME totals — flow telemetry across every cascade invocation. They
 * answer "how often has the cascade fired" + "where does it spend its
 * effort" + "where does it fail." Useful for trainer dashboards that
 * cannot reasonably tap every per-run state field.
 *
 * The brain-owned counter block is updated in-place by the orchestrator
 * with relaxed-order atomic increments — safe to read concurrently from
 * the RO socket thread pool. Callers see a snapshot via the public RO
 * API; reset zeros the whole block.
 * ---------------------------------------------------------------------- */

/* Total cascade stages. Update CASCADE_STAGE_COUNT in lockstep with the
 * stage_mask bits in cascade_stage_mask_t. */
#define NIMCP_CASCADE_STAGE_COUNT 15u

typedef struct nimcp_cascade_counters {
    /* Entry-point flow counters. */
    uint64_t total_runs;
    uint64_t runs_with_prompt;
    uint64_t runs_spontaneous;
    uint64_t runs_fatal_error;

    /* Per-stage observability. invocations counts body-runs (post mask
     * check); mask_skips counts mask-gated short-circuits; failures
     * counts stages whose recorder hit cascade_record_skip with the
     * SKIP-due-to-error path. Arrays sized to the stage count. */
    uint64_t stage_invocations[NIMCP_CASCADE_STAGE_COUNT];
    uint64_t stage_mask_skips[NIMCP_CASCADE_STAGE_COUNT];
    uint64_t stage_failures[NIMCP_CASCADE_STAGE_COUNT];

    /* Semantic counters surfaced by individual stages. */
    uint64_t pragmatics_indirect_overrides;
    uint64_t wernicke_lexicon_miss;
    uint64_t speech_repair_applied;
    uint64_t self_train_steps_matched;     /* train_applied == true */
    uint64_t self_train_steps_no_bindings; /* self_train ran but no plasticity */
    uint64_t self_produced_events_fired;
    uint64_t discourse_ring_pushes_user;
    uint64_t discourse_ring_pushes_self;

    /* S1-H3+H4 fix (2026-05-19) — count of times the recurrent loop hit
     * an OOM on its convergence-check / arcuate-feedback / target-vec
     * heap allocation and degraded to a less-precise convergence path.
     * Non-zero indicates the recurrent loop was running with reduced
     * settling-detection (useful for "loop ran to max_iters" debugging). */
    uint64_t recurrent_oom_count;

    /* S3-H3 fix (2026-05-19) — count of stage_lexical invocations where
     * prod.semantic_vector was NULL and the FEP pe_lexical channel was
     * skipped (set to 0). Non-zero indicates an older bridge that doesn't
     * fill semantic_vector; FEP precision-weighting is operating with one
     * less signal. */
    uint64_t fep_lexical_skipped;

    /* S3-H6 fix (2026-05-19) — count of stage_self_train invocations
     * where the effective lr_scale * gate * precision computation hit
     * the CASCADE_SELF_TRAIN_LR_SCALE_MAX cap. Non-zero indicates the
     * cap is engaging and the intended precision-boost is being clipped. */
    uint64_t self_train_precision_cap_hits;
} nimcp_cascade_counters_t;

/* Walkthrough-4 audit M MEDIUM — ABI size sentinels for the two
 * Python-binding-mirrored structs. The Python TU duplicates the
 * cascade_counters layout locally (bk_cascade_counters_local_t in
 * src/bindings/python/nimcp_python.c) to avoid pulling the cascade
 * header in. If the C struct grows without the local mirror being
 * updated, the cast at the boundary stack-smashes. Sentinel catches
 * any drift at compile time.
 *
 * nimcp_cascade_diag_full_t is similarly read field-by-name from
 * Python and the daemon RPC layer. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(nimcp_cascade_diag_full_t) == 424,
    "nimcp_cascade_diag_full_t ABI: size drifted from expected 424. "
    "Append-only; update binding shadow types in lockstep. "
    "Slice 3 (2026-05-18) appended 7 FEP prediction-error fields (28 bytes). "
    "S1-C1 fix (2026-05-18) appended uint32_t settling_steps (4 bytes) → 416. "
    "S3+S6-H2/H4 fix (2026-05-19) appended pe_content_arcuate_norm + "
    "pe_content_gate_norm (8 bytes) → 424.");
_Static_assert(sizeof(nimcp_cascade_counters_t) == 480,
    "nimcp_cascade_counters_t ABI: size drifted from expected 480. "
    "Append-only; update binding shadow types in lockstep. "
    "S1-H3+H4 fix (2026-05-19) appended uint64_t recurrent_oom_count "
    "(8 bytes) → 464. "
    "S3-H3 + S3-H6 fix (2026-05-19) appended uint64_t fep_lexical_skipped "
    "+ uint64_t self_train_precision_cap_hits (16 bytes) → 480.");
#endif

/**
 * @brief Snapshot the brain's lifetime cascade counters.
 *
 * Lock-free via _Atomic relaxed loads. The struct is copied by value;
 * the snapshot is consistent per-field but counters may individually
 * have advanced between fields if another thread is running the
 * cascade. For monitoring this is fine.
 *
 * @param brain Brain handle (internal pointer; NULL → -1).
 * @param out   Caller-owned snapshot struct, zeroed and populated.
 * @return 0 on success, -1 on invalid args.
 */
int nimcp_brain_get_cascade_counters_impl(
    brain_t brain,
    nimcp_cascade_counters_t* out);

/**
 * @brief Zero the brain's lifetime cascade counters.
 *
 * Atomic per-field store; the operation is not transactional across
 * fields but every counter ends at zero.
 *
 * @param brain Brain handle (NULL → -1).
 * @return 0 on success, -1 on invalid args.
 */
int nimcp_brain_reset_cascade_counters_impl(brain_t brain);

/* ----------------------------------------------------------------------
 * Slice 6 — Thalamic gating of cascade-stage bandwidth.
 *
 * Per-stage gain control on the cascade. Real thalamus (esp. pulvinar)
 * gain-modulates cortical regions according to attention/arousal state.
 * Each cascade stage gets a [0.0, 1.0] gate that scales its scaleable
 * outputs (content_intent magnitude, prosody intensity, etc) when
 * brain->thalamic_gate_enabled is true. Default OFF — cascade is
 * byte-identical to legacy when disabled.
 *
 * Gate-derivation rule (when enabled and manual_override[i] is false):
 *   - Read arousal from NE neuromodulator level (NEUROMOD_NOREPINEPHRINE);
 *     attention from ACh (NEUROMOD_ACETYLCHOLINE); fall back to 0.5 each
 *     when neither modulator system is present.
 *   - High arousal boosts motor + prosody + self_feedback (urgent speech).
 *   - High attention boosts content + episodic + lexical (deliberate
 *     speech).
 *   - thalamic_router imagination_attention (when available) globally
 *     attenuates self_train (don't reinforce dreamy outputs).
 *   - All gates clamped to [0, 1].
 *
 * Manual overrides set via cascade_set_thalamic_gate_for_stage() persist
 * until cleared (weight < 0 clears the override and restores
 * auto-computed value on the next compute).
 *
 * Application points (matching the design doc Slice 6 spec):
 *   stage_content:    multiply content_intent[] by gate before confidence
 *   stage_lexical:    fluency *= gate
 *   stage_self_train: lr_scale *= gate (effective in the reward path)
 *   stage_motor:      no-op currently (text mode) — gate stored only
 *   stage_prosody:    intensity_db scaled by gate (volume gain)
 *   stage_phonological / stage_self_feedback / stage_drive / stage_goal /
 *     stage_listener / stage_episodic / stage_syntactic / stage_self_comp /
 *     stage_speech_repair / stage_wernicke: gate stored for diag only —
 *     these stages produce structured artifacts whose magnitude is
 *     dimensionless, so scaling is not meaningful. Suppression (gate=0)
 *     of these stages should be done via the cascade_stage_mask_t bit.
 *
 * All APIs return 0 on success, -1 on invalid args. */

/* Public stage enumeration mirroring cascade_stage_mask_t bit positions
 * (0..14). Used by the gate setters/getters so callers don't have to
 * compute the bit-index manually. */
typedef enum {
    NIMCP_CASCADE_STAGE_WERNICKE_IDX      = 0,
    NIMCP_CASCADE_STAGE_DRIVE_IDX         = 1,
    NIMCP_CASCADE_STAGE_GOAL_IDX          = 2,
    NIMCP_CASCADE_STAGE_LISTENER_IDX      = 3,
    NIMCP_CASCADE_STAGE_EPISODIC_IDX      = 4,
    NIMCP_CASCADE_STAGE_CONTENT_IDX       = 5,
    NIMCP_CASCADE_STAGE_LEXICAL_IDX       = 6,
    NIMCP_CASCADE_STAGE_SYNTACTIC_IDX     = 7,
    NIMCP_CASCADE_STAGE_SELF_COMP_IDX     = 8,
    NIMCP_CASCADE_STAGE_PHONOLOGICAL_IDX  = 9,
    NIMCP_CASCADE_STAGE_MOTOR_IDX         = 10,
    NIMCP_CASCADE_STAGE_SELF_FEEDBACK_IDX = 11,
    NIMCP_CASCADE_STAGE_SPEECH_REPAIR_IDX = 12,
    NIMCP_CASCADE_STAGE_PROSODY_IDX       = 13,
    NIMCP_CASCADE_STAGE_SELF_TRAIN_IDX    = 14
} nimcp_cascade_stage_idx_t;

/** Master enable for the thalamic gating layer. Default OFF. */
int communication_cascade_set_thalamic_gate_enabled(brain_t brain, bool enabled);
bool communication_cascade_get_thalamic_gate_enabled(brain_t brain);

/** Manual override of a single stage's gate weight.
 *  Pass weight < 0 to CLEAR the manual override (next compute will
 *  re-derive the gate from arousal/attention). Otherwise weight is
 *  clamped to [0.0, 1.0]. */
int communication_cascade_set_thalamic_gate_for_stage(brain_t brain,
                                                      nimcp_cascade_stage_idx_t stage,
                                                      float weight);

/** Read current gate weights into caller-owned arrays. out_weights and
 *  out_overrides may each be NULL. count_out (NULL-tolerant) is set to
 *  the number of populated entries (currently 15). */
int communication_cascade_get_thalamic_gates(brain_t brain,
                                              float* out_weights,
                                              bool*  out_overrides,
                                              uint32_t* count_out);

/** Force a re-derivation of gate weights from current brain state.
 *  The orchestrator calls this once at the top of every cascade run
 *  (when enabled), so callers normally don't need to invoke it
 *  directly — it's exposed for tests + diagnostics. Stages flagged in
 *  manual_override[] are left alone. */
int communication_cascade_compute_thalamic_gates(brain_t brain);

/* ----------------------------------------------------------------------
 * SLICE 3 — FEP recurrent metrics.
 *
 * Across the iterations of communication_cascade_run_recurrent, the per-
 * stage prediction errors trace a trajectory. A well-settling recurrent
 * system shows pe_total climbing in the first 1-3 iterations (the
 * arcuate feedback is making the next iter "try harder") and then
 * falling toward zero as the system converges on a coherent utterance.
 * A wedged cascade shows pe_total flat or growing — the recurrent loop
 * isn't actually fixing anything between iterations.
 *
 * These metrics summarize that trajectory and live on the brain so a
 * monitor / trainer can snapshot them after each cascade run. Reset
 * happens implicitly at the start of every recurrent run.
 * ---------------------------------------------------------------------- */

typedef struct nimcp_cascade_fep_metrics {
    /* Number of iterations actually run in the latest recurrent call.
     * 0 when the recurrent loop has never been invoked. */
    uint32_t iterations_run;

    /* Per-iteration pe_total trace. Sized to a fixed maximum so the
     * caller doesn't need to allocate. Indices [iterations_run..) are
     * zeroed. Max matches the runtime cap in communication_cascade_run_
     * recurrent (64). */
    float    pe_total_trace[64];

    /* Summary scalars derived from the trace. All are NaN/0-safe when
     * iterations_run < 2. */
    float    pe_total_initial;       /* trace[0] */
    float    pe_total_terminal;      /* trace[iterations_run-1] */
    float    pe_total_min;
    float    pe_total_max;
    float    pe_total_mean;

    /* Decay rate: (initial - terminal) / max(initial, eps). Positive =
     * system reduced its surprise across iterations (good). Negative =
     * system became more surprised (bad — the recurrent loop is
     * diverging). 0 when iterations_run < 2. */
    float    pe_decay_rate;

    /* Converged flag — set true when the recurrent loop exited because
     * utterance + self_match stabilized (not because it hit max_iters).
     * Decoupled from pe_decay_rate so callers can detect "converged on
     * a bad attractor" (low decay + converged=true). */
    int      converged;
} nimcp_cascade_fep_metrics_t;

/* Same compile-time ABI guarantee as the other binding-mirrored structs.
 * Layout: u32(4) + 64×float(256) + 5×float(20) + float(4) + int(4) = 288.
 * Plus 4 bytes of trailing padding to align to 8-byte boundary? Actually
 * with iterations_run(u32) + 64 floats (already 4-aligned) + 5 floats +
 * decay rate + int — all 4-byte fields, no padding required. Total 288. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(nimcp_cascade_fep_metrics_t) == 288,
    "nimcp_cascade_fep_metrics_t ABI: size drifted from expected 288. "
    "Update binding shadow types in lockstep.");
#endif

/**
 * @brief Snapshot the brain's latest recurrent-cascade FEP metrics.
 *
 * Returns the metrics from the MOST RECENT call to
 * communication_cascade_run_recurrent. Zero-initialized if the recurrent
 * loop has never been invoked. Lock-free: the recurrent loop writes the
 * struct in place at exit; readers see at-worst a stale snapshot, not a
 * torn one (the writes are aligned 4-byte stores). For tighter
 * consistency wrap an external mutex; this API trades that for read
 * latency.
 *
 * @param brain Brain handle (NULL → -1).
 * @param out   Caller-owned struct; zeroed and populated on rc=0.
 * @return 0 on success, -1 on invalid args.
 */
int nimcp_brain_get_cascade_fep_metrics_impl(
    brain_t brain,
    nimcp_cascade_fep_metrics_t* out);

#ifdef __cplusplus
}
#endif

#endif  /* NIMCP_COMMUNICATION_CASCADE_H */
