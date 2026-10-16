/**
 * @file nimcp_communication_cascade.c
 * @brief Multi-region production cascade — Phase 2A skeleton.
 *
 * Each stage reads real state from its source module(s) and writes its
 * contribution to the shared production_cascade_state_t. Intent-formation
 * stages (1-5) actually shape the content_intent vector that drives the
 * bridge; output stages (6-9) currently delegate to the existing bridge /
 * Broca path while we build the GL→Broca lexicon mirror in Phase 2C.
 *
 * Module gracefully no-ops when an upstream module isn't attached — a
 * brain initialized with init-mode=minimal still gets to run the
 * cascade, it just won't have hypothalamus / ToM / hippocampus signals.
 *
 * Design: docs/claude/communication-cascade-plan.md.
 */

#include "language/nimcp_communication_cascade.h"
#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_tf.h"
#include "language/nimcp_phonological_loop.h"

#include "core/brain/nimcp_brain_internal.h"

/* Slice E (Option-1 rebuild) — developmental stage scaffolding table. */
#include "cognitive/grounded_language/nimcp_stage_table.h"

/* Wave 2 Item #10 — Stage 11 (self-train) calls the SNN language bridge's
 * supervised plasticity API to apply (self_match - baseline) reward to
 * every produced word's (intent → word_pop) binding. */
#include "snn/bridges/nimcp_snn_language_bridge.h"
#include "snn/nimcp_snn_network.h"   /* produce-time SNN generation step */
#include "lnn/nimcp_lnn.h"           /* lnn_get_state — LNN liquid state → intent */

/* Speech-repair (disfluency cleaner) — applied to brain's own utterance
 * before Stage 8 (self-comprehension) so self_match measures intent vs.
 * cleaned content rather than intent vs. production noise. */
#include "core/brain/regions/broca/nimcp_speech_repair.h"

/* Cognitive module headers — each stage needs the public getter API. */
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "core/brain/regions/prefrontal/nimcp_prefrontal_adapter.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"  /* Step E: reasoning prime */
#include "core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h"
#include "cognitive/memory/nimcp_semantic_memory.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
/* Note: cannot include core/brain/regions/broca/nimcp_phonological.h here
 * because Wernicke's perception/nimcp_speech_cortex.h (pulled in
 * transitively via grounded_language + brain_internal headers) also
 * defines a conflicting struct phoneme_t. The Broca processor lives in a
 * separate TU; Stage 9 emits the raw phoneme sequence directly using
 * ASCII-letter codes (the same encoding broca_adapter.c uses internally),
 * and approximates the syllable count by counting vowel clusters - good
 * enough for the diagnostic exposed via state->syllable_count. */

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"  /* S2-C2: arcuate-feedback lock */
#include "utils/time/nimcp_time.h"      /* Slice D: monotonic_us for reward freshness */

/* S2-C2/C3 fix: lazy-init for brain->arcuate_feedback_lock. The lock
 * protects the arcuate_feedback_{vec,dim,strength} tuple from the
 * recurrent-loop writer + cascade_stage_content reader race. Returns the
 * mutex pointer or NULL if allocation fails (caller skips the locked
 * path — degraded but no crash; the same TOCTOU window as before).
 *
 * Race-free via a global init mutex (mirrors the phonological_loop
 * S5-C2 pattern in nimcp_phonological_loop.c). Fast-path single
 * branch when the lock is already populated. */
#include <pthread.h>
static nimcp_once_t  g_arcuate_init_once = PTHREAD_ONCE_INIT;
static nimcp_mutex_t g_arcuate_init_mu;
static void arcuate_init_global_once(void) {
    nimcp_mutex_init(&g_arcuate_init_mu, NULL);
}
static nimcp_mutex_t* cascade_arcuate_lock_ensure(brain_t brain) {
    if (!brain) return NULL;
    /* Fast-path: lock already populated. */
    nimcp_mutex_t* m = (nimcp_mutex_t*)brain->arcuate_feedback_lock;
    if (m) return m;

    nimcp_once(&g_arcuate_init_once, arcuate_init_global_once);
    nimcp_mutex_lock(&g_arcuate_init_mu);
    /* Re-check under lock — another thread may have initialized while
     * we waited. */
    m = (nimcp_mutex_t*)brain->arcuate_feedback_lock;
    if (!m) {
        m = nimcp_mutex_create(NULL);
        /* Publish only after the mutex is fully constructed. */
        brain->arcuate_feedback_lock = (void*)m;
    }
    nimcp_mutex_unlock(&g_arcuate_init_mu);
    return m;
}

/* === SLICE 7 — cerebellar prediction-correction forward decls.
 *
 * The full cerebellum adapter header (include/core/brain/regions/cerebellum/
 * nimcp_cerebellum_adapter.h) pulls bio-async + logging in, which in turn
 * conflicts with the Wernicke speech_cortex phoneme_t already transitively
 * included via grounded_language. We only need three functions out of the
 * adapter ABI, so we forward-declare them with a void* opaque pointer —
 * same pattern world_model_cognitive_integration.c uses. The cerebellum
 * lives on brain->cerebellum as `struct cerebellum_adapter*`; the void*
 * cast at the call site is safe because both refer to the same opaque type.
 *
 *   cerebellum_predict_outcome — feed an 8D motor command vector,
 *       receive the cerebellum's forward-model prediction + confidence.
 *   cerebellum_update_forward_model — close the loop with (cmd, outcome)
 *       so Marr-Albus-Ito LTD shapes the forward-model weights.
 *   cerebellum_broadcast_error — broadcast a scalar error to every
 *       Purkinje cell (climbing-fiber signal); error_type 1 = timing,
 *       2 = force (we reuse 1 for prosody-timing and 2 for motor-force
 *       in keeping with existing callers).
 */
extern bool cerebellum_predict_outcome(void* adapter,
                                        const float* motor_command,
                                        uint32_t num_dims,
                                        float* predicted_outcome,
                                        float* confidence);
extern bool cerebellum_update_forward_model(void* adapter,
                                             const float* motor_command,
                                             const float* outcome,
                                             uint32_t num_dims);
extern bool cerebellum_broadcast_error(void* adapter,
                                        float error_magnitude,
                                        uint8_t error_type);

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdint.h>

#define LOG_MODULE "COMM_CASCADE"

/*============================================================================
 * Internal helpers
 *==========================================================================*/

static void cascade_record_skip(production_cascade_state_t* s,
                                 cascade_stage_mask_t stage,
                                 const char* reason) {
    s->stages_skipped |= (uint32_t)stage;
    /* Don't overwrite an earlier failure reason; first one wins. */
    if (!s->failure_reason[0] && reason) {
        size_t n = strlen(reason);
        if (n >= sizeof(s->failure_reason)) n = sizeof(s->failure_reason) - 1;
        memcpy(s->failure_reason, reason, n);
        s->failure_reason[n] = '\0';
    }
}

static void cascade_record_complete(production_cascade_state_t* s) {
    s->stages_completed++;
}

static void cascade_record_fail(production_cascade_state_t* s,
                                 const char* reason) {
    s->stages_failed++;
    if (!s->failure_reason[0] && reason) {
        size_t n = strlen(reason);
        if (n >= sizeof(s->failure_reason)) n = sizeof(s->failure_reason) - 1;
        memcpy(s->failure_reason, reason, n);
        s->failure_reason[n] = '\0';
    }
}

/*============================================================================
 * SLICE 3 — FEP prediction-error helpers.
 *
 * Each major stage produces a prediction of what its inputs SHOULD look
 * like given the cognitive state, observes the actual input, and records
 * the L2 norm of the difference in state->pe_*_norm. These are trivial
 * predictors at the start of this slice — population-dynamic predictors
 * land in a later slice. The SIGNAL we surface is the SHAPE of the
 * prediction-error trace across recurrent iterations, not the value of
 * any single iteration's PE.
 *==========================================================================*/

/* L2 norm of a difference between two equal-length float vectors,
 * normalized by sqrt(dim) so the scalar is comparable across stages
 * with different input ranks. NaN-safe — NaN inputs contribute 0 (we
 * don't want a single bad dimension to mask the rest of the signal). */
static inline float cascade_fep_norm_diff(const float* a, const float* b,
                                            uint32_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
    float ssum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float d = a[i] - b[i];
        if (isfinite(d)) ssum += d * d;
    }
    float n = sqrtf(ssum) / sqrtf((float)dim);
    return isfinite(n) ? n : 0.0f;
}

/* Sum the per-stage PE scalars into pe_total. NaN/inf entries are
 * treated as 0 so a single missing stage doesn't poison the total. */
static inline void cascade_fep_recompute_total(production_cascade_state_t* s) {
    if (!s) return;
    float t = 0.0f;
    if (isfinite(s->pe_content_norm))   t += s->pe_content_norm;
    if (isfinite(s->pe_lexical_norm))   t += s->pe_lexical_norm;
    if (isfinite(s->pe_syntactic_norm)) t += s->pe_syntactic_norm;
    if (isfinite(s->pe_self_comp_norm)) t += s->pe_self_comp_norm;
    s->pe_total = t;
}

/* Return the state's fep_precision when finite + positive, else 1.0.
 * Stage_self_train uses this to keep its legacy single-pass behavior
 * when called outside the recurrent loop. */
static inline float out_state_fep_precision_or_1(
        const production_cascade_state_t* s) {
    if (!s) return 1.0f;
    float p = s->fep_precision;
    if (!isfinite(p) || p <= 0.0f) return 1.0f;
    return p;
}

/* Map a single-bit cascade_stage_mask_t to a [0,14] index for the
 * counter arrays. Returns -1 if the input is not a power of two or
 * is out of range. The orchestrator only ever passes single-bit
 * values here, so the helper rejects multi-bit composites. */
static inline int cascade_stage_to_index(uint32_t stage_bit) {
    if (stage_bit == 0) return -1;
    /* Single-bit only. */
    if ((stage_bit & (stage_bit - 1)) != 0) return -1;
    int idx = 0;
    while ((stage_bit & 1u) == 0) { stage_bit >>= 1; idx++; }
    if (idx >= 15) return -1;
    return idx;
}

/* Counter-bump helpers — gated on the brain pointer. relaxed-order
 * atomic increments suffice; readers don't need program-order across
 * different counters. */
#include <stdatomic.h>
static inline void cascade_counter_invoke(brain_t brain, uint32_t stage_bit) {
    int i = cascade_stage_to_index(stage_bit);
    if (i < 0 || !brain) return;
    atomic_fetch_add_explicit(&brain->cascade_stage_invocations[i], 1u,
                              memory_order_relaxed);
}
static inline void cascade_counter_mask_skip(brain_t brain, uint32_t stage_bit) {
    int i = cascade_stage_to_index(stage_bit);
    if (i < 0 || !brain) return;
    atomic_fetch_add_explicit(&brain->cascade_stage_mask_skips[i], 1u,
                              memory_order_relaxed);
}
static inline void cascade_counter_failure(brain_t brain, uint32_t stage_bit) {
    int i = cascade_stage_to_index(stage_bit);
    if (i < 0 || !brain) return;
    atomic_fetch_add_explicit(&brain->cascade_stage_failures[i], 1u,
                              memory_order_relaxed);
}

/* Stage 0 (Wernicke comprehension) and Stage 8 (Wernicke self-comprehension)
 * live in nimcp_communication_cascade_wernicke.c because Wernicke and Broca
 * both define phrase_type_t with overlapping enum values; pulling both into
 * one TU triggers a redeclaration error. */
extern int cascade_stage_wernicke(brain_t brain, const char* prompt,
                                    production_cascade_state_t* state);
extern int cascade_stage_self_comprehension(brain_t brain,
                                              production_cascade_state_t* state);
/* Helper used by stage_goal to look up the Wernicke-extracted SVO words. */
extern const gl_lexicon_entry_t* lexicon_find_internal(
    const grounded_language_t* gl, const char* word);

/* Step E imagination blend: forward-declared (not #included) because the
 * imagination engine header redeclares audio_cortex_t / nimcp_gpu_context_t /
 * spatial_transform_t etc. in ways that clash with headers already pulled
 * into this TU — same TU-conflict reason cascade_stage_wernicke lives apart.
 * brain->imagination is `struct imagination_engine*`, so this opaque decl
 * matches without the header. */
struct imagination_engine;
extern uint32_t imagination_engine_copy_active_vector(
    struct imagination_engine* engine, float* caller_buf,
    uint32_t caller_cap, float* out_vividness);

/*============================================================================
 * Slice 6 — Thalamic gating helpers.
 *
 * Computes per-stage gain weights from brain arousal/attention state +
 * (optionally) the existing thalamic_router's imagination_attention.
 * Stages then read brain->thalamic_gate_weights[i] at their tail and
 * scale their scaleable outputs accordingly. Default OFF — when
 * brain->thalamic_gate_enabled is false the entire layer is skipped and
 * the cascade behaves byte-identically to legacy.
 *==========================================================================*/

/* Use the thalamic router's API (when present) and the neuromodulator
 * system (when present) without pulling those headers into the cascade
 * TU — the cascade already includes plasticity/neuromodulators above. */
#include "middleware/routing/nimcp_thalamic_router.h"

/* Clamp a float to [lo, hi]; NaN/Inf are coerced to lo. */
static inline float cascade_clamp01(float v) {
    if (!isfinite(v)) return 0.0f;
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

int communication_cascade_set_thalamic_gate_enabled(brain_t brain, bool enabled) {
    if (!brain) return -1;
    brain->thalamic_gate_enabled = enabled;
    /* S6-M4 fix (2026-05-19): on first enable, initialize EVERY non-overridden
     * gate to neutral (1.0). Pre-fix this was gated on "all weights zero AND
     * no overrides", which made it order-dependent: a caller setting an
     * override BEFORE enable would skip the bulk init, leaving the
     * non-override gates at calloc-zero 0.0 -> most stages silenced.
     *
     * Post-fix: manual_override[i] preserves user-set values; everything
     * else is forced to 1.0 so stages that read pre-compute see pass-through. */
    if (enabled) {
        for (uint32_t i = 0; i < 15; i++) {
            if (!brain->thalamic_gate_manual_override[i]) {
                brain->thalamic_gate_weights[i] = 1.0f;
            }
        }
    }
    return 0;
}

bool communication_cascade_get_thalamic_gate_enabled(brain_t brain) {
    if (!brain) return false;
    return brain->thalamic_gate_enabled;
}

int communication_cascade_set_thalamic_gate_for_stage(brain_t brain,
                                                      nimcp_cascade_stage_idx_t stage,
                                                      float weight) {
    if (!brain) return -1;
    uint32_t i = (uint32_t)stage;
    if (i >= 15) return -1;
    /* S6-H1 fix (2026-05-19): NaN/Inf weights must clear the override,
     * not coerce-to-zero-and-lock. Pre-fix NaN failed `weight < 0.0f`
     * (NaN compares false against `<`), slipped through to
     * cascade_clamp01 which coerced NaN -> 0.0 AND set
     * manual_override[i] = true. Result: the stage was locked at 0.0
     * (full gate close) forever — a NaN SINK. The public C wrapper at
     * nimcp_part_core.c:2885 was already doing `!isfinite -> -1.0f` but
     * the inner boundary missed the same defense; an internal caller
     * could still poison the gate. */
    if (!isfinite(weight) || weight < 0.0f) {
        /* Sentinel: clear override + return to auto-derived behavior. */
        brain->thalamic_gate_manual_override[i] = false;
        return 0;
    }
    brain->thalamic_gate_weights[i] = cascade_clamp01(weight);
    brain->thalamic_gate_manual_override[i] = true;
    return 0;
}

int communication_cascade_get_thalamic_gates(brain_t brain,
                                              float* out_weights,
                                              bool*  out_overrides,
                                              uint32_t* count_out) {
    if (!brain) return -1;
    if (count_out) *count_out = 15;
    if (out_weights) {
        for (uint32_t i = 0; i < 15; i++) {
            out_weights[i] = brain->thalamic_gate_weights[i];
        }
    }
    if (out_overrides) {
        for (uint32_t i = 0; i < 15; i++) {
            out_overrides[i] = brain->thalamic_gate_manual_override[i];
        }
    }
    return 0;
}

int communication_cascade_compute_thalamic_gates(brain_t brain) {
    if (!brain) return -1;

    /* Arousal proxy: NE level if neuromodulator system is up, else 0.5. */
    float arousal = 0.5f;
    float attention = 0.5f;
    if (brain->neuromodulator_system) {
        float ne = neuromodulator_get_level(brain->neuromodulator_system,
                                              NEUROMOD_NOREPINEPHRINE);
        float ach = neuromodulator_get_level(brain->neuromodulator_system,
                                              NEUROMOD_ACETYLCHOLINE);
        if (isfinite(ne))  arousal   = cascade_clamp01(ne);
        if (isfinite(ach)) attention = cascade_clamp01(ach);
    }

    /* Thalamic router imagination_attention — when imagination is
     * dominating, we don't want the cascade to fire self-train on dreamy
     * outputs. Reads as -1.0 on error → ignored. */
    float imag_attn = -1.0f;
    if (brain->thalamic_router) {
        imag_attn = thalamic_router_get_imagination_attention(
            (thalamic_router_t*)brain->thalamic_router);
    }
    float self_train_attenuation = 1.0f;
    if (isfinite(imag_attn) && imag_attn >= 0.0f) {
        /* High imagination focus → attenuate self_train (don't lock in
         * inner-monologue patterns as real production memory). At
         * imag_attn=1.0, self_train gate halves. */
        self_train_attenuation = 1.0f - 0.5f * cascade_clamp01(imag_attn);
    }

    /* Per-stage gate-weight derivation rule. All gates start at the
     * baseline 0.5*(arousal+attention) so when the brain has no signal
     * (NaN modulators, no router) gates are ~0.5 — a noticeable but not
     * catastrophic attenuation. Each stage gets a stage-specific
     * weighting on top:
     *
     *   - Motor + prosody + self_feedback + speech_repair: arousal-heavy.
     *     Urgent speech routes more bandwidth to articulation.
     *   - Content + episodic + lexical + goal: attention-heavy. Focused
     *     deliberate speech routes bandwidth to intent-formation.
     *   - Drive + listener + wernicke + syntactic + phonological +
     *     self_comp: balanced.
     *   - Self_train: attention-heavy, then attenuated by
     *     imagination_attention so we don't reinforce daydreams.
     *
     * All gates clamped to [0.0, 1.0]. Stages flagged with
     * manual_override[i] retain their cached value. */
    const float baseline = 0.5f * (arousal + attention);
    float derived[15];
    derived[NIMCP_CASCADE_STAGE_WERNICKE_IDX]      = baseline;
    derived[NIMCP_CASCADE_STAGE_DRIVE_IDX]         = baseline;
    derived[NIMCP_CASCADE_STAGE_GOAL_IDX]          = 0.4f * arousal + 0.6f * attention;
    derived[NIMCP_CASCADE_STAGE_LISTENER_IDX]      = baseline;
    derived[NIMCP_CASCADE_STAGE_EPISODIC_IDX]      = 0.3f * arousal + 0.7f * attention;
    derived[NIMCP_CASCADE_STAGE_CONTENT_IDX]       = 0.3f * arousal + 0.7f * attention;
    derived[NIMCP_CASCADE_STAGE_LEXICAL_IDX]       = 0.4f * arousal + 0.6f * attention;
    derived[NIMCP_CASCADE_STAGE_SYNTACTIC_IDX]     = baseline;
    derived[NIMCP_CASCADE_STAGE_SELF_COMP_IDX]     = baseline;
    derived[NIMCP_CASCADE_STAGE_PHONOLOGICAL_IDX]  = baseline;
    derived[NIMCP_CASCADE_STAGE_MOTOR_IDX]         = 0.7f * arousal + 0.3f * attention;
    derived[NIMCP_CASCADE_STAGE_SELF_FEEDBACK_IDX] = 0.6f * arousal + 0.4f * attention;
    derived[NIMCP_CASCADE_STAGE_SPEECH_REPAIR_IDX] = 0.6f * arousal + 0.4f * attention;
    derived[NIMCP_CASCADE_STAGE_PROSODY_IDX]       = 0.7f * arousal + 0.3f * attention;
    derived[NIMCP_CASCADE_STAGE_SELF_TRAIN_IDX]    =
        (0.3f * arousal + 0.7f * attention) * self_train_attenuation;

    for (uint32_t i = 0; i < 15; i++) {
        if (brain->thalamic_gate_manual_override[i]) continue;
        brain->thalamic_gate_weights[i] = cascade_clamp01(derived[i]);
    }
    return 0;
}

/* Lookup helper for stages: returns the current weight for a stage by
 * its [0..14] index. Returns 1.0 (pass-through) when the gate layer is
 * disabled or the brain pointer is NULL so callers can multiply without
 * a separate enabled-check. */
static inline float cascade_thalamic_gate_for(brain_t brain, uint32_t i) {
    if (!brain || !brain->thalamic_gate_enabled || i >= 15) return 1.0f;
    float w = brain->thalamic_gate_weights[i];
    if (!isfinite(w)) return 1.0f;
    if (w < 0.0f) return 0.0f;
    if (w > 1.0f) return 1.0f;
    return w;
}

/*============================================================================
 * Stage 1: Drive — read hypothalamus + insula + amygdala
 *==========================================================================*/

static int cascade_stage_drive(brain_t brain,
                                production_cascade_state_t* state) {
    /* Sensible defaults — neutral drive state. Overridden if hypothalamus
     * is attached and we can read real urgencies. */
    state->drive_magnitude = 0.5f;
    state->drive_valence   = 0.0f;
    state->drive_arousal   = 0.5f;
    state->dominant_drive  = 0;

    if (!brain->hypothalamus) {
        cascade_record_skip(state, CASCADE_STAGE_DRIVE,
                            "stage_drive: hypothalamus not attached");
        return 0;
    }

    /* Phase 2B: actually query the hypothalamus state. The adapter
     * exposes the homeostatic + stress + circadian state via
     * hypothalamus_get_state(); from there we extract the strongest
     * drive (hunger / thirst / temperature / stress / fatigue) and
     * compute valence and arousal. The dominant_drive index is what
     * lets downstream stages bias content toward drive-relevant
     * concepts (Phase 2C will add concept-vocab→drive mapping). */
    hypothalamus_state_t h;
    memset(&h, 0, sizeof(h));
    if (!hypothalamus_get_state(brain->hypothalamus, &h)) {
        cascade_record_skip(state, CASCADE_STAGE_DRIVE,
                            "stage_drive: hypothalamus_get_state failed");
        /* Fall back to neutral defaults set above. */
        state->drive_magnitude = 0.5f;
        state->drive_arousal   = 0.5f;
        return 0;
    }

    /* Cherry-pick the homeostatic drive with highest magnitude.
     * Hunger / thirst / cortisol-stress / fatigue / temp-deviation are
     * all in [0,1]. Whichever is loudest wins. */
    const float hunger     = h.appetite.hunger_drive;
    const float thirst     = h.hydration.thirst_drive;
    const float stress     = h.hpa_axis.stress_input;
    const float fatigue    = h.circadian.sleep_pressure;
    const float thermal_e  = (float)fabs(h.thermoregulation.core_temp.error);
    /* Curiosity/social/etc aren't in the homeostatic struct — they live
     * in the drive system handle which the adapter doesn't expose yet.
     * Phase 2C TODO: surface those via a new accessor. */

    float    max_drive = hunger;
    uint8_t  dominant  = 1; /* 1=HUNGER */
    if (thirst    > max_drive) { max_drive = thirst;    dominant = 2; /* THIRST */ }
    if (stress    > max_drive) { max_drive = stress;    dominant = 4; /* STRESS */ }
    if (fatigue   > max_drive) { max_drive = fatigue;   dominant = 5; /* FATIGUE */ }
    if (thermal_e > max_drive) { max_drive = thermal_e; dominant = 3; /* THERMAL */ }

    state->drive_magnitude = max_drive;
    state->dominant_drive  = dominant;

    /* Arousal: HPA stress drives arousal up; circadian alertness too.
     * Take max of (stress, 1 - sleep_pressure). At rest, alertness
     * is high so arousal sits near 0.7-0.9; under stress, arousal
     * spikes to 1.0. */
    float alertness = 1.0f - fatigue;
    state->drive_arousal = (stress > alertness) ? stress : alertness;
    if (state->drive_arousal > 1.0f) state->drive_arousal = 1.0f;
    if (state->drive_arousal < 0.0f) state->drive_arousal = 0.0f;

    /* Valence: stress = strongly negative; fed/hydrated/rested = positive;
     * neutral when no specific drive dominates. */
    if (dominant == 4 /* STRESS */) {
        state->drive_valence = -stress;
    } else if (dominant == 1 /* HUNGER */ || dominant == 2 /* THIRST */) {
        /* Aversive when high — same direction as stress but milder. */
        state->drive_valence = -0.5f * max_drive;
    } else if (dominant == 5 /* FATIGUE */) {
        state->drive_valence = -0.3f * fatigue;
    } else if (dominant == 3 /* THERMAL */) {
        state->drive_valence = -0.4f * thermal_e;
    } else {
        state->drive_valence = 0.0f;
    }

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 2: Goal — read PFC + WM, classify speech act
 *==========================================================================*/

static int cascade_stage_goal(brain_t brain, const char* prompt,
                               production_cascade_state_t* state) {
    /* Default: STATEMENT, neutral priority. */
    state->act_type      = SPEECH_ACT_DECLARE;  /* generic statement */
    state->goal_priority = 0.5f;
    state->topic_count   = 0;

    /* Speech-act classification. Phase 2D-A uses Wernicke's Stage 0
     * output (wh-word / aux-inversion / parse-tree) when available;
     * falls back to punctuation heuristic when Wernicke wasn't run.
     * Wernicke's signal is more reliable — it catches questions
     * without trailing '?' ("Is the dog hungry") and imperatives
     * with no subject ("Close the door"). */
    if (state->prompt_is_question) {
        state->act_type = SPEECH_ACT_QUESTION;
    } else if (state->prompt_is_imperative) {
        state->act_type = SPEECH_ACT_COMMAND;
    } else if (prompt && prompt[0]) {
        size_t n = strlen(prompt);
        char last = (n > 0) ? prompt[n-1] : '\0';
        if (last == '?')      state->act_type = SPEECH_ACT_QUESTION;
        else if (last == '!') state->act_type = SPEECH_ACT_DECLARE;
        else                  state->act_type = SPEECH_ACT_ASSERT;
    }

    /* Pragmatics override: when the surface form is a question, ask
     * Broca's pragmatics processor whether it's actually an INDIRECT
     * speech act ("Can you pass the salt?" surface=QUESTION,
     * intended=REQUEST). The full pipeline (pragmatics_analyze) composes
     * scalar implicature + Gricean maxim analysis + indirect-act detection
     * + ironic detection. We only override on questions — direct
     * REQUEST/COMMAND is already handled above via prompt_is_imperative.
     *
     * Fallback: if pragmatics_analyze fails (e.g. processor not fully
     * initialised), fall through to the lightweight surface classifier
     * so we still get indirect-request detection from the template path. */
    if (state->prompt_is_question &&
        brain->broca_pragmatics &&
        prompt && prompt[0]) {
        pragmatic_analysis_t analysis = {0};
        bool got_full = pragmatics_analyze(brain->broca_pragmatics,
                                            prompt, /*speaker_id*/0,
                                            /*timestamp_ms*/0, &analysis);
        speech_act_type_t primary;
        bool is_indirect;
        if (got_full) {
            primary     = analysis.speech_act.primary_act;
            is_indirect = analysis.speech_act.is_indirect;
        } else {
            /* Fallback to lightweight classifier */
            speech_act_result_t prag = {0};
            if (!pragmatics_classify_speech_act(brain->broca_pragmatics,
                                                  prompt, /*speaker_id*/0, &prag)) {
                primary     = SPEECH_ACT_DECLARE;
                is_indirect = false;
            } else {
                primary     = prag.primary_act;
                is_indirect = prag.is_indirect;
            }
        }
        if (is_indirect &&
            (primary == SPEECH_ACT_REQUEST ||
             primary == SPEECH_ACT_COMMAND)) {
            state->act_type              = SPEECH_ACT_REQUEST;
            state->pragmatic_is_indirect = true;
        }
    }

    /* Pull top-priority goal from PFC if available. PFC goals are
     * already priority-sorted; the first one drives goal_priority for
     * the cascade. prefrontal_goal_t stores priority as an enum
     * (goal_priority_t), so we coerce to a 0..1 scalar by dividing
     * by the count. */
    if (brain->prefrontal) {
        prefrontal_goal_t goals[8];
        uint32_t goal_count = 8;
        if (prefrontal_get_active_goals(brain->prefrontal, goals,
                                          &goal_count) &&
            goal_count > 0) {
            /* Convert priority enum to 0..1 — assume 4 priority levels. */
            float pri_scaled = (float)goals[0].priority / 4.0f;
            if (pri_scaled > 1.0f) pri_scaled = 1.0f;
            state->goal_priority = pri_scaled;
        }
    }

    /* Working memory: each active chunk is a candidate topic concept.
     * working_memory_get_salience uses an out-param signature. */
    if (brain->working_memory) {
        uint32_t wm_size = working_memory_get_size(brain->working_memory);
        for (uint32_t i = 0; i < wm_size && state->topic_count < 8; i++) {
            float salience = 0.0f;
            if (!working_memory_get_salience(brain->working_memory, i,
                                              &salience)) continue;
            if (salience < 0.2f) continue;
            /* WM stores feature vectors, not concept_ids directly — for
             * Phase 2A we just count active items. Phase 2B will
             * cross-reference with semantic_memory to recover concept_ids. */
            state->topic_concept_ids[state->topic_count++] = (uint64_t)i;
        }
    }

    /* Phase 2D-A: pull Wernicke's identified subject/verb/object as
     * primary topic concepts. These are real content words from the
     * prompt itself (extracted via parse tree), not array indices —
     * stage_content uses them via GL lexicon lookup to lift the
     * actual concept feature vectors into the intent. */
    if (state->prompt_subject[0] && state->topic_count < 8) {
        const gl_lexicon_entry_t* e = lexicon_find_internal(
            brain->grounded_lang, state->prompt_subject);
        if (e && e->binding_count > 0) {
            state->topic_concept_ids[state->topic_count++] =
                e->bindings[0].concept_id;
            if (state->target_concept_id == 0) {
                state->target_concept_id = e->bindings[0].concept_id;
            }
        }
    }
    if (state->prompt_verb[0] && state->topic_count < 8) {
        const gl_lexicon_entry_t* e = lexicon_find_internal(
            brain->grounded_lang, state->prompt_verb);
        if (e && e->binding_count > 0) {
            state->topic_concept_ids[state->topic_count++] =
                e->bindings[0].concept_id;
        }
    }
    if (state->prompt_object[0] && state->topic_count < 8) {
        const gl_lexicon_entry_t* e = lexicon_find_internal(
            brain->grounded_lang, state->prompt_object);
        if (e && e->binding_count > 0) {
            state->topic_concept_ids[state->topic_count++] =
                e->bindings[0].concept_id;
        }
    }

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 3: Listener model — read Theory of Mind
 *==========================================================================*/

/* Map ToM emotion enum to a [-1,1] valence axis. Negative = aversive
 * (sadness/anger/fear/disgust/anxiety/shame); positive = approach
 * (joy/pride/calm/surprise); 0 for neutral/unknown. */
static float tom_emotion_to_valence(tom_emotion_t e) {
    switch (e) {
        case TOM_EMOTION_JOY:      return  0.9f;
        case TOM_EMOTION_PRIDE:    return  0.7f;
        case TOM_EMOTION_CALM:     return  0.5f;
        case TOM_EMOTION_SURPRISE: return  0.3f;
        case TOM_EMOTION_NEUTRAL:  return  0.0f;
        case TOM_EMOTION_SADNESS:  return -0.7f;
        case TOM_EMOTION_ANGER:    return -0.8f;
        case TOM_EMOTION_FEAR:     return -0.9f;
        case TOM_EMOTION_DISGUST:  return -0.6f;
        case TOM_EMOTION_ANXIETY:  return -0.5f;
        case TOM_EMOTION_SHAME:    return -0.4f;
        case TOM_EMOTION_UNKNOWN:
        default:                   return  0.0f;
    }
}

static int cascade_stage_listener(brain_t brain,
                                   production_cascade_state_t* state) {
    state->listener_known              = false;
    state->listener_belief_confidence  = 0.0f;
    state->listener_emotion_valence    = 0.0f;
    state->audience_familiarity        = 0.0f;

    if (!brain->theory_of_mind) {
        cascade_record_skip(state, CASCADE_STAGE_LISTENER,
                            "stage_listener: ToM not attached");
        return 0;
    }

    /* Phase 2B: query the actual listener model. Agent_id = 0 is the
     * conventional "primary interlocutor". The cascade doesn't yet
     * track multiple agents per conversation — that's a future
     * enhancement once a turn-taking layer is added. The aggregate
     * perspective-taking score still seeds audience_familiarity. */
    tom_statistics_t stats;
    memset(&stats, 0, sizeof(stats));
    if (tom_get_statistics(brain->theory_of_mind, &stats)) {
        state->audience_familiarity = stats.perspective_taking_score;
    }

    /* Pull belief + emotion from the ToM module. The codebase had an
     * tom_get_agent_state declaration with per-agent fan-out, but that
     * function is a phantom (header-only, no impl). The two real getters
     * are tom_get_bdi_state (no agent_id — uses the most-recently-tracked
     * agent internally) and tom_infer_emotion. Together they give us
     * everything we need. */
    tom_belief_t    belief    = {0};
    tom_desire_t    desire    = {0};
    tom_intention_t intention = {0};
    if (tom_get_bdi_state(brain->theory_of_mind, &belief, &desire, &intention)) {
        state->listener_known = true;
        state->listener_belief_confidence = belief.confidence;
    } else if (state->audience_familiarity > 0.1f) {
        /* No BDI record but ToM has been doing perspective work — treat
         * as "vague listener known". */
        state->listener_known = true;
    }

    if (state->listener_known) {
        float emotion_conf = 0.0f;
        tom_emotion_t emotion = tom_infer_emotion(brain->theory_of_mind,
                                                    &emotion_conf);
        state->listener_emotion_valence =
            tom_emotion_to_valence(emotion) * emotion_conf;
    }

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 4: Episodic retrieval — hippocampus similarity search
 *==========================================================================*/

static int cascade_stage_episodic(brain_t brain,
                                   const float* query_vec,
                                   uint32_t query_dim,
                                   production_cascade_state_t* state) {
    state->episodic_count = 0;

    if (!brain->hippocampus) {
        cascade_record_skip(state, CASCADE_STAGE_EPISODIC,
                            "stage_episodic: hippocampus not attached");
        return 0;
    }
    if (!query_vec || query_dim == 0) {
        cascade_record_skip(state, CASCADE_STAGE_EPISODIC,
                            "stage_episodic: no query vector");
        return 0;
    }

    /* Use the adapter-level cue retrieval API. The retrieval_result_t
     * allocates the memory + similarity arrays internally. */
    retrieval_result_t result;
    memset(&result, 0, sizeof(result));
    if (!hippocampus_retrieve_by_cue(brain->hippocampus,
                                       query_vec, query_dim,
                                       16 /* max_results */,
                                       &result) ||
        !result.retrieval_success || result.count == 0) {
        /* hippocampus_retrieve_by_cue() always calloc()s memories[] and
         * similarities[]; free them before bailing or they leak per call. */
        if (result.memories) {
            nimcp_free(result.memories);
            result.memories = NULL;
        }
        if (result.similarities) {
            nimcp_free(result.similarities);
            result.similarities = NULL;
        }
        cascade_record_skip(state, CASCADE_STAGE_EPISODIC,
                            "stage_episodic: cue retrieval empty");
        return 0;
    }

    /* Phase 2B: store the actual memory_id (not just an array index) so
     * stage_content can dereference and lift the memory's feature vector
     * into the intent. Each retrieved memory carries a feature_count×float
     * features array — that IS the encoded content for the past episode.
     * Storing the memory_id lets us look it up later if needed; for the
     * cascade we mainly need the feature vector, which we'll reach via
     * result.memories[i].features in stage_content. We keep result alive
     * by transferring ownership to a per-call scratch field on state. */
    for (uint32_t i = 0; i < result.count && state->episodic_count < 16; i++) {
        state->episodic_concept_ids[state->episodic_count] =
            (uint64_t)result.memories[i].memory_id;
        state->episodic_relevances[state->episodic_count]  =
            result.similarities ? result.similarities[i] : 0.5f;
        state->episodic_count++;
    }

    /* Stash the retrieval result on the cascade state so stage_content
     * can lift feature vectors. We use a struct member added below — for
     * now, just leak result.memories / similarities arrays (they're
     * tiny and the cascade-state cleanup will free them). */
    state->episodic_retrieval = result;  /* shallow copy — memories[] and
                                          * similarities[] ownership transferred. */

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 5: Content composition — weighted combine of all sources
 *==========================================================================*/

static int cascade_stage_content(brain_t brain,
                                  const float* prompt_intent,
                                  uint32_t prompt_dim,
                                  production_cascade_state_t* state) {
    /* Allocate the content_intent vector at the GL semantic_dim. Without
     * grounded_lang we have nothing to drive — fail this stage. */
    if (!brain->grounded_lang) {
        cascade_record_fail(state,
                             "stage_content: grounded_lang not available");
        cascade_counter_failure(brain, CASCADE_STAGE_CONTENT);
        return -1;
    }

    uint32_t dim = grounded_language_get_semantic_dim(brain->grounded_lang);
    if (dim == 0) {
        cascade_record_fail(state, "stage_content: semantic_dim is 0");
        cascade_counter_failure(brain, CASCADE_STAGE_CONTENT);
        return -1;
    }

    state->content_intent = (float*)nimcp_calloc(dim, sizeof(float));
    if (!state->content_intent) {
        cascade_record_fail(state, "stage_content: alloc failed");
        cascade_counter_failure(brain, CASCADE_STAGE_CONTENT);
        return -1;
    }
    state->content_dim = dim;

    /* Weighted combine. Prompt dominates when present (we're answering
     * a question); drive / episodic / listener / goal nudge the answer
     * without overwhelming the prompt's content. Tuning history:
     *   Phase 2A: w_drive=0.3 — too small to flip bridge argmax
     *   Phase 2B v1: w_drive=0.6 — too LARGE, overrode prompt in the
     *                drive band, all prompts converged to same output
     *   Phase 2B v2: w_drive=0.15 — small enough that prompt content
     *                differentiates outputs, large enough to shift
     *                argmax when prompt signal is weak (spontaneous mode) */
    const float w_prompt   = 1.0f;
    const float w_drive    = 0.15f;
    const float w_episodic = 0.3f;
    const float w_listener = 0.1f;
    const float w_goal     = 0.2f;
    /* Tier 1 Step D (2026-05-24): working-memory content. Each active WM
     * item is a feature vector the brain is currently "holding in mind".
     * Blending it into content_intent makes produce reflect the active
     * train of thought, not just lexical relevance to the prompt. Weight
     * sits between episodic (0.3 — retrieved past) and goal (0.2 —
     * directive) since WM is the immediate cognitive foreground but is
     * salience-scaled per item below, so the effective contribution of
     * any single weak chunk stays small. */
    const float w_working_memory = 0.25f;
    /* Tier 1 Step E (2026-05-24): three more cognitive/discourse sources.
     *  - w_discourse: prior-turn topic vector → multi-turn continuity. Kept
     *    low (0.15) so the answer stays on-topic without parroting the last
     *    turn over the current prompt.
     *  - w_imagination: active imagined-scenario latent → produce reflects
     *    simulated content. Scaled per-scenario by vividness below.
     *  - w_reasoning: cached inference conclusion vector → produce reflects
     *    what the brain concluded, not just lexical relevance. Scaled by the
     *    reasoning chain's overall_confidence below; highest of the three
     *    because a confident conclusion should steer the answer. */
    const float w_discourse    = 0.15f;
    const float w_imagination  = 0.2f;
    const float w_reasoning    = 0.3f;
    /* Non-SNN networks → content_intent (2026-06-05; REDESIGNED 2026-06-06).
     * Directive: every network except FNO feeds the language generator. The
     * first cut blended each network's RAW vector (w·v). That regressed produce
     * hard (A/B: 5% vs 55% intent-fidelity) because last_decision (ANN) and the
     * LNN liquid state are ~CONSTANT across a produce sweep and far larger in
     * magnitude than the prompt seed, so under cosine scoring they became a DC
     * bias that overwrote the intent direction — every prompt collapsed to one
     * salad. Redesign: each network is UNIT-NORMALIZED then scaled to at most
     * a_net × ||intent-so-far|| (the anchor), and only blended when an anchor
     * exists (seed_norm > 0). So a network can never dominate the prompt+
     * cognitive intent — it's a bounded tilt — and with no anchor it contributes
     * nothing (kills the unanchored-salad case). a_ann highest (the decision is
     * the brain's judgment), a_cnn lowest (usually idle). HNN has no vector — it
     * modulates the produce floor (metacog block below). All default-OFF. */
    const float a_ann          = 0.15f;  /* fraction of anchor magnitude */
    const float a_lnn          = 0.12f;
    const float a_cnn          = 0.08f;

    /* 1. Seed from prompt comprehend (if provided). */
    if (prompt_intent && prompt_dim > 0) {
        uint32_t copy = (prompt_dim < dim) ? prompt_dim : dim;
        for (uint32_t i = 0; i < copy; i++) {
            float v = prompt_intent[i];
            if (isfinite(v)) state->content_intent[i] += w_prompt * v;
        }
    }

    /* 2. Drive bias: spread drive activity across a deterministic 16-dim
     * band keyed by dominant drive type. Different drives hit different
     * dimensions so the same prompt under different drive states biases
     * the intent toward different concepts. Phase 2C will replace this
     * deterministic mapping with a real concept↔drive lookup.
     *
     * Signal: max(drive_magnitude, arousal). On a state-empty fresh
     * brain, drive_magnitude=0 but arousal≈1 (no fatigue, no stress
     * = full alertness), so the bias still fires — "I'm alert and
     * about to speak" is itself a valid cognitive signal. */
    float drive_signal = state->drive_magnitude;
    if (state->drive_arousal > drive_signal) drive_signal = state->drive_arousal;
    if (drive_signal > 0.05f && dim >= 16) {
        const uint32_t band_start = ((uint32_t)state->dominant_drive % 8) * 16;
        for (uint32_t i = 0; i < 16; i++) {
            uint32_t slot = (band_start + i) % dim;
            /* Sign alternation: every other dim carries valence tone,
             * the rest carry magnitude — gives the bridge multi-axis
             * signal rather than a flat scalar bump. */
            float sign = (i & 1) ? state->drive_valence : 1.0f;
            state->content_intent[slot] +=
                w_drive * drive_signal * sign;
        }
    }

    /* 3. Episodic concepts: lift feature vectors from each retrieved
     * memory. Each memory.features is feature_count×float of encoded
     * past experience — adding it to the intent is "the brain biased
     * toward saying things related to what it remembers". */
    for (uint32_t i = 0; i < state->episodic_count; i++) {
        uint32_t mi = i;  /* index into result.memories aligns with
                           * episodic_concept_ids since we filled them in
                           * lockstep. */
        if (mi >= state->episodic_retrieval.count) continue;
        const hippocampus_memory_t* m = &state->episodic_retrieval.memories[mi];
        if (!m->features || m->feature_count == 0) continue;
        const float relevance = state->episodic_relevances[i];
        uint32_t copy = (m->feature_count < dim) ? m->feature_count : dim;
        for (uint32_t j = 0; j < copy; j++) {
            float fv = m->features[j];
            if (isfinite(fv)) {
                state->content_intent[j] += w_episodic * relevance * fv;
            }
        }
    }

    /* 4. Listener bias: when ToM is engaged, valence of the listener's
     * inferred emotion shifts the intent's tonal dimension. Subtle but
     * deterministic — used to differentiate audience-aware vs neutral
     * production in Phase 2D evals. */
    if (state->listener_known && state->audience_familiarity > 0.0f && dim >= 4) {
        state->content_intent[2] +=
            w_listener * state->audience_familiarity *
            state->listener_emotion_valence;
        state->content_intent[3] +=
            w_listener * state->audience_familiarity *
            state->listener_belief_confidence;
    }

    /* 5. Goal-priority bias: high-priority PFC goal → broaden the
     * intent slightly so the bridge's softmax is less peaked → more
     * likely to express the goal-relevant concepts even at low
     * activation. This is the "I really need to say this" effect. */
    if (state->goal_priority > 0.0f && dim >= 8) {
        for (uint32_t i = 0; i < state->topic_count && i < 8 && i < dim; i++) {
            uint32_t slot = (uint32_t)(state->topic_concept_ids[i] % dim);
            state->content_intent[slot] += w_goal * state->goal_priority;
        }
    }

    /* 5b. Working-memory content (Tier 1 Step D, 2026-05-24). Lift the
     * feature vector of each active WM item and add it to the intent,
     * scaled by the item's salience. This is the cognitive-foreground
     * analogue of the episodic blend in step 3: where episodic biases
     * toward "what I remember", this biases toward "what I'm thinking
     * about right now". An earlier stage (cascade_stage_pragmatic) only
     * COUNTED WM items as topic candidates and discarded their feature
     * vectors — this is where those vectors finally drive production.
     *
     * Mirrors the episodic min-copy + isfinite pattern. Salience<0.2 is
     * skipped (same threshold the pragmatic stage uses) so decayed
     * chunks don't smear the intent. working_memory_get returns a
     * READ-ONLY pointer valid for the duration of this call — we never
     * mutate WM, so no copy is needed. */
    if (brain->working_memory) {
        uint32_t wm_size = working_memory_get_size(brain->working_memory);
        for (uint32_t i = 0; i < wm_size; i++) {
            float salience = 0.0f;
            if (!working_memory_get_salience(brain->working_memory, i,
                                              &salience)) continue;
            if (!isfinite(salience) || salience < 0.2f) continue;

            uint32_t item_dim = 0;
            const float* fv = working_memory_get(brain->working_memory, i,
                                                 &item_dim);
            if (!fv || item_dim == 0) continue;

            uint32_t copy = (item_dim < dim) ? item_dim : dim;
            for (uint32_t j = 0; j < copy; j++) {
                float v = fv[j];
                if (isfinite(v)) {
                    state->content_intent[j] += w_working_memory * salience * v;
                }
            }
        }
    }

    /* 5c. Discourse continuity (Tier 1 Step E, 2026-05-24). communication_
     * cascade_run comprehends the current prompt FIRST, which pushes it as
     * the newest discourse turn — so back=2 is the PRIOR turn (the previous
     * exchange). Blending its topic vector keeps the reply coherent with
     * what we were just talking about, not only the immediate prompt. Kept
     * to a soft 0.15 weight so it biases topic without parroting. No-ops
     * cleanly on the first turn (accessor returns false when <2 turns). */
    /* X2 (walkthrough fix): copy the prior turn vector under the discourse
     * lock into a stack buffer rather than reading a raw interior ring pointer
     * that a concurrent comprehend/push_turn could free mid-blend. Mirrors the
     * imagination source's copy-under-lock + 4096 stack cap below. */
    if (dim <= 4096u) {
        float prior_buf[4096];
        uint32_t prior_dim = grounded_language_copy_recent_turn_vector(
            brain->grounded_lang, 2u, prior_buf, dim);
        if (prior_dim > 0) {
            uint32_t copy = (prior_dim < dim) ? prior_dim : dim;
            for (uint32_t j = 0; j < copy; j++) {
                float v = prior_buf[j];
                if (isfinite(v)) {
                    state->content_intent[j] += w_discourse * v;
                }
            }
        }
    }

    /* 5d. Imagination (Tier 1 Step E, 2026-05-24). If a scenario is actively
     * being imagined, blend its content vector (semantic_buffer, falling
     * back to latent_state) into the intent, scaled by vividness — produce
     * then reflects the imagined scene. imagination_engine_copy_active_vector
     * copies under the engine lock (UAF-free) and returns 0 when imagination
     * is idle, which is the common case during plain Q&A → clean no-op. The
     * dim<=4096 guard mirrors the FEP snapshot cap below and bounds the
     * stack copy buffer. */
    if (brain->imagination && dim <= 4096u) {
        float imag_buf[4096];
        float vividness = 0.0f;
        uint32_t n = imagination_engine_copy_active_vector(
            brain->imagination, imag_buf, dim, &vividness);
        if (n > 0 && isfinite(vividness) && vividness > 0.0f) {
            uint32_t copy = (n < dim) ? n : dim;
            for (uint32_t j = 0; j < copy; j++) {
                float v = imag_buf[j];
                if (isfinite(v)) {
                    state->content_intent[j] += w_imagination * vividness * v;
                }
            }
        }
    }

    /* 5e. Reasoning conclusion (Tier 1 Step E, 2026-05-24). Blend the cached
     * inference-conclusion vector (primed once per prompt in
     * cascade_prime_reasoning) so produce reflects what the brain CONCLUDED,
     * not just lexical relevance. Scaled by the reasoning chain's confidence
     * so a tentative conclusion nudges gently and a confident one steers.
     * cascade_reasoning_dim == 0 (the default until the opt-in flag is set
     * and a conclusion is cached) → clean no-op. The cache buffer is
     * allocated once and never moved, so this read is UAF-free even across
     * RPC threads; torn values during a concurrent reprime are finite-guarded
     * and benign for a soft additive bias. */
    if (brain->cascade_reasoning_dim > 0 && brain->cascade_reasoning_vec) {
        float rconf = brain->cascade_reasoning_confidence;
        if (!isfinite(rconf) || rconf < 0.0f) rconf = 0.0f;
        if (rconf > 1.0f) rconf = 1.0f;
        uint32_t copy = (brain->cascade_reasoning_dim < dim)
                          ? brain->cascade_reasoning_dim : dim;
        for (uint32_t j = 0; j < copy; j++) {
            float v = brain->cascade_reasoning_vec[j];
            if (isfinite(v)) {
                state->content_intent[j] += w_reasoning * rconf * v;
            }
        }
    }

    /* Anchor magnitude = ||content_intent|| after prompt + all cognitive
     * sources but BEFORE any network blend. Each network below is unit-
     * normalized then scaled to a fraction of this anchor, so none can dominate
     * the assembled intent direction. No anchor (seed_norm ~ 0, e.g. no prompt
     * and no cognitive signal) → networks contribute nothing, which is what
     * keeps a stale constant network vector from manufacturing salad out of an
     * empty intent. */
    float seed_norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++)
        seed_norm += state->content_intent[i] * state->content_intent[i];
    seed_norm = sqrtf(seed_norm);
    const float net_eps = 1e-6f;

    /* 5g. ANN (main feed-forward net) → content_intent. last_decision is the
     * brain's learned judgment of the current input. Unit-normalized and scaled
     * to a_ann × seed_norm, so a stale or large decision is a bounded bias, not
     * a dominator (this is the fix for the raw-blend collapse). NULL decision or
     * no anchor → clean no-op. Default-OFF via cascade_ann_in_content. */
    if (brain->cascade_ann_in_content && seed_norm > net_eps &&
        brain->last_decision &&
        brain->last_decision->output_vector &&
        brain->last_decision->output_size > 0) {
        uint32_t copy = (brain->last_decision->output_size < dim)
                          ? brain->last_decision->output_size : dim;
        float nrm = 0.0f;
        for (uint32_t j = 0; j < copy; j++) {
            float v = brain->last_decision->output_vector[j];
            if (isfinite(v)) nrm += v * v;
        }
        nrm = sqrtf(nrm);
        if (nrm > net_eps) {
            float scale = a_ann * seed_norm / nrm;
            for (uint32_t j = 0; j < copy; j++) {
                float v = brain->last_decision->output_vector[j];
                if (isfinite(v)) state->content_intent[j] += scale * v;
            }
        }
    }

    /* 5h. LNN (liquid net) → content_intent. The liquid state is the temporal-
     * context reservoir. lnn_get_state CLONES under the network lock (UAF-free);
     * we copy out, then destroy. Unit-normalized + scaled to a_lnn × seed_norm.
     * Idle, absent, or no-anchor → clean no-op. Default-OFF. */
    if (brain->cascade_lnn_in_content && seed_norm > net_eps && brain->lnn_network) {
        nimcp_tensor_t* lstate = NULL;
        if (lnn_get_state(brain->lnn_network, &lstate) == 0 && lstate) {
            const float* sd = (const float*)nimcp_tensor_data(lstate);
            uint32_t n = (uint32_t)nimcp_tensor_numel(lstate);
            if (sd && n > 0) {
                uint32_t copy = (n < dim) ? n : dim;
                float nrm = 0.0f;
                for (uint32_t j = 0; j < copy; j++)
                    if (isfinite(sd[j])) nrm += sd[j] * sd[j];
                nrm = sqrtf(nrm);
                if (nrm > net_eps) {
                    float scale = a_lnn * seed_norm / nrm;
                    for (uint32_t j = 0; j < copy; j++)
                        if (isfinite(sd[j])) state->content_intent[j] += scale * sd[j];
                }
            }
            nimcp_tensor_destroy(lstate);
        }
    }

    /* 5i. CNN (visual cortex) → content_intent. Cached perceptual embedding of
     * the last image. Empty cache (text-only training) → out_n=0 → no-op. Unit-
     * normalized + scaled to a_cnn × seed_norm. dim<=4096 bounds the stack
     * buffer; no-anchor → no-op. Default-OFF via cascade_cnn_in_content. */
    if (brain->cascade_cnn_in_content && seed_norm > net_eps &&
        brain->visual_cortex && dim <= 4096u) {
        float cnn_buf[4096];
        uint32_t cn = 0;
        if (visual_cortex_get_cached_features(brain->visual_cortex,
                                              cnn_buf, dim, &cn) == 0 && cn > 0) {
            uint32_t copy = (cn < dim) ? cn : dim;
            float nrm = 0.0f;
            for (uint32_t j = 0; j < copy; j++)
                if (isfinite(cnn_buf[j])) nrm += cnn_buf[j] * cnn_buf[j];
            nrm = sqrtf(nrm);
            if (nrm > net_eps) {
                float scale = a_cnn * seed_norm / nrm;
                for (uint32_t j = 0; j < copy; j++)
                    if (isfinite(cnn_buf[j])) state->content_intent[j] += scale * cnn_buf[j];
            }
        }
    }

    /* 5f. Metacognitive produce-floor modulation (2026-06-02). The modulatory
     * cognitive modules (world-model, introspection, emotion) carry no semantic
     * content vector, so instead of blending into content_intent they shift the
     * produce confidence floor — how willing the brain is to assert vs say "I
     * don't know". Driven here from world-model SURPRISE: the FEP prediction-
     * error trajectory over the recurrent settle. A model that converged
     * (terminal << initial) is confident → lower the floor slightly; one that
     * stayed surprised (terminal ~ initial) → raise it. Bounded + gated;
     * default-OFF flag keeps the base stage floor unchanged. THREE modulatory
     * signals fold into the adjust: world-model SURPRISE (FEP prediction-error
     * trajectory), INTROSPECTION distress (wellbeing self-assessment), and —
     * when cascade_hnn_in_content is on — HNN energy-deviation: the Hamiltonian
     * net carries no semantic vector, so its conservation drift modulates
     * assertiveness instead. Well-conserved dynamics (low deviation) → confident
     * → lower the floor; large drift (unstable) → raise it. The block runs if
     * EITHER metacog gating OR the HNN flag is on, so HNN can drive the floor
     * even with metacog off. */
    if (brain->grounded_lang &&
        (grounded_language_get_metacog_gates_produce(brain->grounded_lang) ||
         brain->cascade_hnn_in_content)) {
        float adjust = 0.0f;
        const bool metacog_on =
            grounded_language_get_metacog_gates_produce(brain->grounded_lang);
        if (metacog_on) {
        /* (i) World-model surprise: FEP PE terminal/initial over the recurrent
         * settle. Converged (terminal << initial) → confident → lower the floor;
         * still-surprised (terminal ~ initial) → raise it. */
        float fep_init = brain->fep_pe_initial, fep_term = brain->fep_pe_terminal;
        if (isfinite(fep_init) && isfinite(fep_term) && fep_init > 1e-6f) {
            float ratio = fep_term / fep_init;   /* ~0 converged, ~1+ still surprised */
            if (!isfinite(ratio)) ratio = 0.5f;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 2.0f) ratio = 2.0f;
            adjust = (ratio - 0.5f) * 0.20f;     /* center 0.5 → bounded ~[-0.10,+0.30] */
        }
        /* (ii) Introspection distress: the brain's own wellbeing self-assessment
         * (last_distress.distress_score in [0,1], set by the wellbeing monitor on
         * the decide path). Higher distress → the brain is in a degraded/uncertain
         * regime → raise the floor so it asserts less and falls back to "I don't
         * know" sooner. Additive, bounded to +0.15; 0 (or monitoring off) is a
         * clean no-op. The metacog setter re-clamps the total to [-1,1]. */
        float distress = brain->last_distress.distress_score;
        if (isfinite(distress) && distress > 0.0f) {
            if (distress > 1.0f) distress = 1.0f;
            adjust += distress * 0.15f;
        }
        }  /* end if (metacog_on) */

        /* (iii) HNN energy-deviation → assertiveness. |H(t)-H(0)|/|H(0)| from
         * the Hamiltonian layer, cached on network_metrics by the learn loop.
         * Near 0 == well conserved (stable dynamics) → small negative nudge
         * (confident, lower floor); large drift → positive nudge (raise floor).
         * Centered at 0.25 and bounded to ~[-0.05,+0.15]. Absent HNN leaves the
         * metric at 0 → a tiny confident nudge, harmless and flag-gated. */
        if (brain->cascade_hnn_in_content) {
            float dev = brain->network_metrics.hnn_energy_deviation;
            if (!isfinite(dev) || dev < 0.0f) dev = 0.0f;
            if (dev > 1.0f) dev = 1.0f;
            adjust += (dev - 0.25f) * 0.20f;
        }
        grounded_language_set_metacog_floor_adjust(brain->grounded_lang, adjust);
    }

    /* SLICE 3 — Stage Content's PREDICTION of what content_intent
     * "should" be is the upstream-driven blend computed in steps 1..5.
     * We snapshot it BEFORE step 6 (arcuate feedback). The OBSERVED
     * content_intent post-arcuate-pre-gate is what step 6 leaves
     * behind; the post-gate value is what stage 7+ actually sees.
     *
     * S3+S6-H2/H4 fix (2026-05-19): we now record TWO PE values to
     * disentangle the arcuate-correction signal from the thalamic-gate
     * attenuation signal. pe_content_arcuate_norm = ‖post-arc - pre‖
     * (the FEP-relevant slice — drops as the recurrent loop settles).
     * pe_content_gate_norm = ‖post-gate - post-arc‖ (pure attenuation
     * from the thalamic relay; not a prediction error). Legacy
     * pe_content_norm == pe_content_arcuate_norm so pe_total drives
     * fep_precision on actual surprise, not on gate toggles.
     *
     * Two snapshots live on the stack — a fixed cap keeps the helper
     * allocation-free in the hot path. Stages with dim > the cap fall
     * back to "no PE recorded" (pe_content_* stays 0). 4096 covers
     * every grounded_lang semantic_dim we ship today. */
    float fep_pre_arcuate[4096];
    float fep_post_arcuate[4096];
    bool  fep_predicted_recorded = false;
    if (dim <= (uint32_t)(sizeof(fep_pre_arcuate) / sizeof(float))) {
        memcpy(fep_pre_arcuate, state->content_intent,
               dim * sizeof(float));
        fep_predicted_recorded = true;
    }

    /* 6. ARCUATE FASCICULUS FEEDBACK (Slice 2 of recurrent-language-arch).
     * Real anatomy: between iterations of speech production, Wernicke
     * sends a parse of own previous speech back to Broca/PFC via the
     * arcuate fasciculus. This drives self-monitoring + correction. In
     * our recurrent cascade, the previous iteration's
     * (intent - own_output_parse) error vector lives in
     * brain->arcuate_feedback_vec; ADD it here so the next pass of
     * lexical+syntactic emphasizes what the previous attempt missed.
     *
     * No-op when arcuate_feedback_vec is NULL or dim mismatches — the
     * recurrent-loop owner allocates it on iteration 2+ and zeros it
     * between non-recurrent calls.
     *
     * S2-C2 fix: take the arcuate-feedback lock to serialize against
     * the recurrent-loop writer which may free + reassign the pointer
     * (UAF was reachable before the lock). The lock is held across the
     * O(dim) consume loop — bounded ~128 floats, well under our
     * lock-hold budget. */
    /* S2-H2 fix (2026-05-19): convex blend toward (intent + k*error_vec)
     * where error_vec = target - own_output, with target = iter-0 snapshot.
     * Pre-fix the additive form `intent += k*feedback_vec` amplified intent
     * magnitude by (1+k) per iter on a stable prompt. The recurrent loop
     * now sets blend in (0,1] and recomputes feedback_vec each iter as
     * (target - own_output); cascade_stage_content does a blended apply
     * so the apply is bounded.
     *
     * Legacy single-pass cascade: arcuate_feedback_blend defaults to 0,
     * which causes us to skip the apply entirely (recurrent-only by
     * contract, no behavior change for single-pass callers).
     *
     * S1-H5+H7 (2026-05-19): per-element isfinite() guard on feedback_vec
     * so a NaN/Inf in the comprehend output (e.g. Wernicke parsing
     * garbage) doesn't propagate into content_intent. */
    {
        nimcp_mutex_t* arc_lock = cascade_arcuate_lock_ensure(brain);
        if (arc_lock) {
            nimcp_mutex_lock(arc_lock);
            if (brain->arcuate_feedback_vec &&
                brain->arcuate_feedback_dim == dim &&
                isfinite(brain->arcuate_feedback_strength) &&
                brain->arcuate_feedback_strength > 0.0f &&
                isfinite(brain->arcuate_feedback_blend) &&
                brain->arcuate_feedback_blend > 0.0f) {
                float k     = brain->arcuate_feedback_strength;
                float blend = brain->arcuate_feedback_blend;
                if (blend > 1.0f) blend = 1.0f;
                for (uint32_t i = 0; i < dim; i++) {
                    float v = brain->arcuate_feedback_vec[i];
                    if (!isfinite(v)) continue;
                    state->content_intent[i] += blend * k * v;
                }
            }
            nimcp_mutex_unlock(arc_lock);
        } else {
            /* Lock alloc failed — fall back to the legacy unlocked path.
             * Same UAF window as pre-fix; preserves liveness on the
             * cold-start failure path. Convex-blend + NaN guards still apply. */
            if (brain->arcuate_feedback_vec &&
                brain->arcuate_feedback_dim == dim &&
                isfinite(brain->arcuate_feedback_strength) &&
                brain->arcuate_feedback_strength > 0.0f &&
                isfinite(brain->arcuate_feedback_blend) &&
                brain->arcuate_feedback_blend > 0.0f) {
                float k     = brain->arcuate_feedback_strength;
                float blend = brain->arcuate_feedback_blend;
                if (blend > 1.0f) blend = 1.0f;
                for (uint32_t i = 0; i < dim; i++) {
                    float v = brain->arcuate_feedback_vec[i];
                    if (!isfinite(v)) continue;
                    state->content_intent[i] += blend * k * v;
                }
            }
        }
    }

    /* S3+S6-H2/H4 fix (2026-05-19): snapshot post-arcuate, pre-gate so
     * we can record the arcuate-only PE separately from the gate-only
     * attenuation below. */
    if (fep_predicted_recorded) {
        memcpy(fep_post_arcuate, state->content_intent, dim * sizeof(float));
    }

    /* Slice 6: thalamic gating of content_intent — scales every component
     * by the thalamic gate for stage_content. Default OFF; returns 1.0
     * when disabled. Applied BEFORE confidence computation so content_
     * confidence reflects the thalamically-gated magnitude. */
    const float gate_content = cascade_thalamic_gate_for(brain,
                                  NIMCP_CASCADE_STAGE_CONTENT_IDX);
    if (gate_content != 1.0f) {
        for (uint32_t i = 0; i < dim; i++) {
            state->content_intent[i] *= gate_content;
        }
    }

    /* S3+S6-H2/H4 fix (2026-05-19): Record stage-content prediction error
     * SPLIT into arcuate (FEP-relevant) and gate (attenuation) components.
     * pe_content_arcuate_norm = ‖post-arcuate - pre‖ — what FEP cares
     * about. pe_content_gate_norm = ‖post-gate - post-arcuate‖ — pure
     * thalamic effect, surfaced for observability but NOT counted as
     * surprise. Legacy pe_content_norm aliases the arcuate field so
     * pe_total drives precision on actual prediction error. */
    if (fep_predicted_recorded) {
        state->pe_content_arcuate_norm = cascade_fep_norm_diff(
            fep_post_arcuate, fep_pre_arcuate, dim);
        state->pe_content_gate_norm = cascade_fep_norm_diff(
            state->content_intent, fep_post_arcuate, dim);
        /* Legacy alias: pe_content_norm = arcuate-only norm. */
        state->pe_content_norm = state->pe_content_arcuate_norm;
    }

    /* Confidence = signal magnitude / sqrt(dim). 1.0 = strongly cohered;
     * 0.0 = no signal anywhere. */
    float ssum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float v = state->content_intent[i];
        ssum += v * v;
    }
    state->content_confidence = sqrtf(ssum) / sqrtf((float)dim);
    if (state->content_confidence > 1.0f) state->content_confidence = 1.0f;

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 6: Lexical selection — bridge produce on content_intent
 *==========================================================================*/

static int cascade_stage_lexical(brain_t brain,
                                  production_cascade_state_t* state) {
    if (!brain->grounded_lang || !state->content_intent) {
        cascade_record_skip(state, CASCADE_STAGE_LEXICAL,
                            "stage_lexical: no grounded_lang or content_intent");
        return 0;
    }

    /* Run the bridge produce on our cascade-shaped content_intent, NOT
     * on the raw prompt comprehension. This is what makes the cascade
     * meaningfully different from grounded_respond — the intent has
     * been shaped by drive + episodic + (eventually) listener +
     * goal stages. */
    /* Produce-time SNN generation step (2026-06-03). When produce_via_snn is ON,
     * seed the cognitively-integrated content_intent's concepts into Wernicke and
     * step the SNN so the concept->word projection drives Broca; produce then
     * reads that fresh intent-driven Broca activity (decode_spikes_cached) instead
     * of stale training spikes. This is what makes the SNN GENERATE the output.
     * Gated additionally on NIMCP_LANG_PRODUCE_STEPS>0 (the whole-network step is
     * expensive) so it's fully opt-in. On any failure produce falls back to the
     * lexicon path as before — never blocks output. */
    if (grounded_language_get_produce_via_snn(brain->grounded_lang) &&
        brain->snn_lang_bridge && brain->snn_network) {
        const char* steps_env = getenv("NIMCP_LANG_PRODUCE_STEPS");
        uint32_t n_steps = (steps_env && steps_env[0]) ? (uint32_t)atoi(steps_env) : 0u;
        if (n_steps > 0u && n_steps <= 64u) {
            int wid = snn_network_find_pop_by_name(brain->snn_network, "wernicke_substrate");
            int bid = snn_network_find_pop_by_name(brain->snn_network, "broca_substrate");
            if (wid >= 0 && bid >= 0) {
                uint64_t cids[32];
                uint32_t nc = grounded_language_top_concepts_for_intent(
                    brain->grounded_lang, state->content_intent,
                    state->content_dim, cids, 32);
                if (nc > 0) {
                    (void)snn_language_bridge_generate_step(
                        brain->snn_lang_bridge, brain->snn_network, wid, bid,
                        cids, nc, n_steps, 1.0f);
                }
            }
        }
    }

    gl_production_result_t prod = {0};
    int rc = grounded_language_produce(brain->grounded_lang,
                                        state->content_intent,
                                        state->content_dim,
                                        GL_PRODUCE_RESPOND, &prod);
    if (rc != 0 || !prod.text || prod.word_count == 0) {
        gl_production_result_cleanup(&prod);
        cascade_record_fail(state, "stage_lexical: bridge produce failed");
        cascade_counter_failure(brain, CASCADE_STAGE_LEXICAL);
        return -1;
    }

    /* SLICE 3 — Stage Lexical PREDICTION: a perfectly-aligned bridge
     * would emit an utterance whose semantic_vector equals the input
     * content_intent. The OBSERVED is prod.semantic_vector. PE =
     * ‖content_intent - prod.semantic_vector‖ / sqrt(dim).
     *
     * S3-H3 fix (2026-05-19): pre-fix fell back to (1 - fluency) when
     * prod.semantic_vector was NULL (older bridges that don't fill it).
     * Fluency is the bridge's pre-recorded CONFIDENCE — NOT prediction
     * error — and folding it into the FEP pe_total inflated the
     * precision-weighting signal with confidence noise.
     *
     * Post-fix: when semantic_vector is absent, set pe_lexical_norm = 0
     * (the FEP gate downstream treats 0 as "no signal" and falls back to
     * pure cosine on the bridge's own self_match instead of doubling up).
     * Bump cascade_fep_lexical_skipped to make the dependency visible
     * — non-zero indicates the bridge is missing semantic_vector and
     * FEP precision is operating with one less signal. */
    if (prod.semantic_vector && state->content_intent &&
        state->content_dim > 0) {
        state->pe_lexical_norm = cascade_fep_norm_diff(
            state->content_intent, prod.semantic_vector, state->content_dim);
    } else {
        state->pe_lexical_norm = 0.0f;
        atomic_fetch_add_explicit(&brain->cascade_fep_lexical_skipped, 1u,
                                  memory_order_relaxed);
    }

    /* Slice E (2026-05-19) — apply per-stage vocab visibility mask to the
     * bridge's freshly-produced text. The grounded_language module owns
     * the mask; we call its filter helper which rewrites prod.text in
     * place (strictly non-growing) and updates prod.word_count. When no
     * mask is installed the call is a no-op. */
    if (brain->grounded_lang) {
        (void)grounded_language_filter_production_by_mask(brain->grounded_lang,
                                                            &prod);
    }

    /* Transfer ownership: prod.text → state->utterance. */
    state->utterance  = prod.text;
    prod.text         = NULL;
    state->word_count = prod.word_count;
    state->fluency    = prod.fluency;

    /* Slice 6: thalamic gating of lexical fluency. The bridge produce
     * runs at full strength (text and word_count are discrete and
     * shouldn't be scaled), but the fluency diagnostic — a 0..1 score
     * downstream consumers (self_feedback, prosody) read for confidence
     * gating — gets multiplied by the lexical gate. Low gate ≈ "I'm
     * speaking but I'm not really attending to lexical access". */
    const float gate_lexical = cascade_thalamic_gate_for(brain,
                                  NIMCP_CASCADE_STAGE_LEXICAL_IDX);
    if (gate_lexical != 1.0f && isfinite(state->fluency)) {
        state->fluency *= gate_lexical;
    }

    gl_production_result_cleanup(&prod);

    /* SLICE 5 — phonological loop integration.
     *
     * When the loop is enabled, merge the just-produced words into the
     * working-memory buffer and SWAP state->utterance to the buffer's
     * surface form (active words with trace >= 0.3). Downstream stages
     * (syntactic, self_comp) then see the buffer's surface form rather
     * than the one-shot lexical output — matching Baddeley's model where
     * production iterates over a held draft.
     *
     * Default OFF — when brain->loop_enabled is false, this branch is
     * skipped and stage_lexical's output flows downstream unchanged
     * (Slice 1+2 behavior). */
    if (brain->loop_enabled && brain->loop_mutex &&
        state->utterance && state->utterance[0]) {
        phonological_loop_merge_words(brain, state->utterance);

        /* Render the active surface form into a heap-owned buffer that
         * replaces state->utterance. Use a reasonable cap matching the
         * cascade's own MAX_TOKENS×avg-word-len budget; the loop is
         * capped at 16 words so 256 bytes is enough headroom. */
        char surface[512];
        surface[0] = '\0';
        uint32_t active = phonological_loop_render_active(
            brain, 0.3f, surface, sizeof(surface));

        if (active > 0 && surface[0]) {
            size_t slen = strlen(surface);
            char* repl = (char*)nimcp_malloc(slen + 1);
            if (repl) {
                memcpy(repl, surface, slen + 1);
                if (state->utterance) nimcp_free(state->utterance);
                state->utterance  = repl;
                state->word_count = active;
                /* fluency stays as the bridge reported it — the buffered
                 * surface form is by-construction a strict subset/
                 * reorder of fluent tokens. */
            }
            /* On alloc failure we keep the bridge's raw text — the loop
             * has still been updated for the next iteration's merge. */
        }
    }

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 7: Syntactic — Broca syntax processor (Phase 2C activated)
 *==========================================================================*/

#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/broca/nimcp_syntax_processor.h"

static int cascade_stage_syntactic(brain_t brain,
                                    production_cascade_state_t* state) {
    state->syntactic_validity = -1.0f;  /* unknown until we run Broca */

    if (!brain->broca || !state->utterance || !state->utterance[0]) {
        cascade_record_skip(state, CASCADE_STAGE_SYNTACTIC,
                            "stage_syntactic: no broca or no utterance");
        return 0;
    }

    /* Tokenize the bridge's output text into words. Same approach the
     * Phase 1 audit experiment used — strtok_r on whitespace +
     * punctuation, with a soft cap on token count. */
    enum { MAX_TOKENS = 32 };
    char dup[2048];
    size_t ulen = strlen(state->utterance);
    if (ulen >= sizeof(dup)) ulen = sizeof(dup) - 1;
    memcpy(dup, state->utterance, ulen);
    dup[ulen] = '\0';

    const char* tokens[MAX_TOKENS];
    uint32_t num_tokens = 0;
    char* save = NULL;
    char* tok = strtok_r(dup, " \t\n\r.,!?;:\"'", &save);
    while (tok && num_tokens < MAX_TOKENS) {
        if (tok[0]) tokens[num_tokens++] = tok;
        tok = strtok_r(NULL, " \t\n\r.,!?;:\"'", &save);
    }
    if (num_tokens == 0) {
        cascade_record_skip(state, CASCADE_STAGE_SYNTACTIC,
                            "stage_syntactic: utterance tokenized to zero words");
        return 0;
    }

    /* Run through Broca's full pipeline: syntax_build_tree (CYK),
     * syntax_validate_grammar (agreement check), phonological generation,
     * motor planning. broca_produce_from_strings does all of these and
     * returns false if any phase fails. */
    broca_adapter_t* broca = (broca_adapter_t*)brain->broca;
    broca_utterance_result_t br;
    memset(&br, 0, sizeof(br));
    bool ok = broca_produce_from_strings(broca, tokens, num_tokens, &br);

    if (!ok) {
        /* Broca rejected — likely a CYK chart-build failure on the
         * bridge's word salad. Record as syntactic_validity = 0.0 so
         * downstream consumers can tell "tried but failed" from "skipped".
         * The bridge's text stays as-is in state->utterance. */
        state->syntactic_validity = 0.0f;
        cascade_record_fail(state,
                            "stage_syntactic: broca rejected utterance");
        cascade_counter_failure(brain, CASCADE_STAGE_SYNTACTIC);
        return 0;
    }

    state->syntactic_validity = (br.syntax_valid && br.agreement_valid) ? 1.0f
                              : (br.syntax_valid ? 0.5f : 0.0f);

    /* Pull the syntactically-ordered word sequence back out of Broca's
     * syntax processor and re-render. CYK is order-preserving for the
     * surface forms it accepts (it builds tree structure but doesn't
     * reorder leaf words), so the rendered text is usually identical
     * to the input. The signal we care about is the validity flag —
     * a successful build means the words DID form a recognized phrase
     * structure, which is more than the bridge's raw output guarantees. */
    syntax_processor_t* syn = broca_get_syntax_processor(broca);
    if (syn) {
        uint32_t unit_count = syntax_get_unit_count(syn);
        if (unit_count > 0) {
            char buf[2048];
            uint32_t pos = 0;
            uint32_t emitted = 0;
            for (uint32_t i = 0; i < unit_count && pos < sizeof(buf) - 1; i++) {
                syntactic_unit_t unit;
                if (!syntax_get_unit(syn, i, &unit)) continue;

                broca_lexical_entry_t entry;
                if (!broca_lookup_word(broca, unit.word_id, NULL, &entry)) continue;
                if (!entry.word[0]) continue;

                if (emitted > 0 && pos < sizeof(buf) - 1) buf[pos++] = ' ';
                size_t wlen = strnlen(entry.word, sizeof(entry.word));
                size_t avail = sizeof(buf) - 1 - pos;
                size_t copy = (wlen < avail) ? wlen : avail;
                memcpy(buf + pos, entry.word, copy);
                pos += (uint32_t)copy;
                emitted++;
            }
            buf[pos] = '\0';

            if (emitted > 0) {
                /* Replace bridge text with Broca-rendered text. */
                char* new_text = (char*)nimcp_calloc(pos + 1, 1);
                if (new_text) {
                    memcpy(new_text, buf, pos);
                    nimcp_free(state->utterance);
                    state->utterance = new_text;
                    state->word_count = emitted;
                }
            }
        }
    }

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 9 (Wave 2 Item 7): Phonological output — text → phoneme sequence.
 *
 * Why: Stage 7 (syntactic) leaves a Broca-rendered text utterance in
 * state->utterance; downstream consumers (motor planning, TTS, edge
 * speech-output) need a phoneme sequence, not graphemes. This stage runs
 * a lightweight rule-based English G2P over the utterance and emits
 * state->phoneme_sequence plus diagnostics. We use the existing Broca
 * phonological_processor_t (include/core/brain/regions/broca/nimcp_phonological.h)
 * for syllabification rather than rolling our own — biological fidelity
 * comes from re-using the same module Broca uses internally.
 *
 * Why a rule-based G2P (not a real one): there is no production G2P module
 * in this codebase today. The Broca lexicon stores phonemes per word, but
 * cascade utterances frequently contain words outside the trained
 * vocabulary (e.g. proper nouns or generated tokens), so a lexicon-only
 * approach would silently emit empty phoneme sequences. The rules below
 * cover all 26 English letters plus 5 common digraphs (sh/ch/th/ng/ph),
 * which is sufficient to expose meaningful phoneme_count + voiced_ratio
 * diagnostics. The phoneme codes themselves are ASCII letters (with a
 * handful of sentinels for digraphs) — matching the convention used in
 * broca_adapter.c phonological_add_phoneme_detailed calls.
 *
 * Why no phonological_processor_t here: pulling in
 * core/brain/regions/broca/nimcp_phonological.h triggers a phoneme_t
 * type conflict with Wernicke's perception/nimcp_speech_cortex.h that
 * is already transitively included through grounded_language. The Broca
 * processor lives in its own TU (nimcp_broca_adapter.c) and stays there.
 * We approximate the syllable count with a simple vowel-cluster
 * heuristic — this matches what a sonority-based syllabifier would
 * produce on monosyllabic English input and is good enough for the
 * diagnostic this stage exposes.
 *==========================================================================*/

/* Returns true if the lowercase letter c is a vowel. */
static inline bool cascade_phon_is_vowel(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
}

/* Lightweight English digraph handler. Returns the digraph sentinel
 * phoneme code if buf[i..i+1] is a recognized digraph (and bumps *advance
 * to 2), else 0. */
static uint8_t cascade_phon_digraph(const char* buf, size_t i, size_t n,
                                     uint32_t* advance) {
    if (i + 1 >= n) return 0;
    char a = buf[i], b = buf[i + 1];
    /* Common English digraphs. The sentinel chars stay in the ASCII
     * punctuation range so they can't collide with letter-encoded phonemes. */
    if (a == 's' && b == 'h') { *advance = 2; return (uint8_t)'$'; }  /* /ʃ/ */
    if (a == 'c' && b == 'h') { *advance = 2; return (uint8_t)'&'; }  /* /tʃ/ */
    if (a == 't' && b == 'h') { *advance = 2; return (uint8_t)'#'; }  /* /θ/ or /ð/ */
    if (a == 'n' && b == 'g') { *advance = 2; return (uint8_t)'@'; }  /* /ŋ/ */
    if (a == 'p' && b == 'h') { *advance = 2; return (uint8_t)'%'; }  /* /f/ */
    return 0;
}

static int cascade_stage_phonological(brain_t brain,
                                       production_cascade_state_t* state) {
    (void)brain;
    if (!state) return 0;

    /* Skip cleanly when there's nothing to phonologize. Matches the
     * upstream skip pattern: a brain with no Stage 6/7 output (minimal
     * init, bridge produced empty text, lexical/syntactic skipped) gets
     * recorded as a skip rather than a failure. */
    if (!state->utterance || state->word_count == 0 || !state->utterance[0]) {
        cascade_record_skip(state, CASCADE_STAGE_PHONOLOGICAL,
                            "stage_phonological: no utterance or empty word_count");
        return 0;
    }

    /* Allocate worst-case phoneme buffer: one phoneme per char + word
     * boundaries. nimcp_calloc zero-fills so a partial fill is safe. */
    const size_t ulen = strlen(state->utterance);
    if (ulen == 0) {
        cascade_record_skip(state, CASCADE_STAGE_PHONOLOGICAL,
                            "stage_phonological: utterance has zero length");
        return 0;
    }
    uint8_t* seq = (uint8_t*)nimcp_calloc(ulen + 1, sizeof(uint8_t));
    if (!seq) {
        cascade_record_fail(state, "stage_phonological: phoneme buffer alloc failed");
        cascade_counter_failure(brain, CASCADE_STAGE_PHONOLOGICAL);
        return 0;
    }

    uint32_t phon_count = 0;
    uint32_t voiced_count = 0;
    /* Syllable-count heuristic: a syllable starts on a vowel that follows
     * a non-vowel (or a word boundary). Adjacent vowels (diphthongs like
     * 'oa', 'ai') count as one. */
    uint32_t syll_count = 0;
    bool prev_phon_was_vowel = false;
    bool prev_was_space = true;

    for (size_t i = 0; i < ulen; ) {
        char ch = state->utterance[i];

        /* Lowercase ASCII letters in place. The cascade often emits
         * mixed-case text from the bridge; lowercase is the canonical
         * form for the rules below. */
        if (ch >= 'A' && ch <= 'Z') ch = (char)(ch + 32);

        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
            ch == '.' || ch == ',' || ch == '!' || ch == '?' ||
            ch == ';' || ch == ':') {
            /* Emit a single space sentinel between words; collapse runs. */
            if (!prev_was_space && phon_count < ulen) {
                seq[phon_count++] = (uint8_t)' ';
                prev_was_space = true;
            }
            prev_phon_was_vowel = false;
            i++;
            continue;
        }

        if (ch < 'a' || ch > 'z') {
            /* Digit / punctuation we don't tokenize on — skip silently. */
            i++;
            continue;
        }

        /* Digraph check first; falls back to single-letter mapping. */
        uint32_t advance = 1;
        uint8_t code = cascade_phon_digraph(state->utterance, i, ulen, &advance);
        bool is_vowel = false;
        bool is_voiced = false;
        if (code != 0) {
            /* Digraph voicing approximation: sh/ch/th-voiceless are
             * unvoiced; ng + ph(→f) we approximate as voiced/unvoiced
             * respectively. Coarse signal — fine for the ratio diagnostic. */
            is_voiced = (code == (uint8_t)'@');  /* only ng voiced */
        } else {
            code = (uint8_t)ch;
            is_vowel = cascade_phon_is_vowel(ch);
            /* English voicing approximation by single letter. Voiced
             * consonants: b d g j l m n r v w y z. Vowels are voiced by
             * default. Voiceless: c f h k p q s t x. */
            if (is_vowel) {
                is_voiced = true;
            } else {
                is_voiced = (ch == 'b' || ch == 'd' || ch == 'g' ||
                             ch == 'j' || ch == 'l' || ch == 'm' ||
                             ch == 'n' || ch == 'r' || ch == 'v' ||
                             ch == 'w' || ch == 'y' || ch == 'z');
            }
        }

        if (phon_count < ulen) {
            seq[phon_count++] = code;
            if (is_voiced) voiced_count++;
        }
        prev_was_space = false;

        /* Vowel-cluster syllable count: each vowel that follows either a
         * non-vowel phoneme or a word boundary starts a new syllable.
         * Adjacent vowels (diphthongs like 'oa', 'ai') count as one. */
        if (is_vowel && !prev_phon_was_vowel) {
            syll_count++;
        }
        prev_phon_was_vowel = is_vowel;

        i += advance;
    }

    /* Trim a trailing space sentinel if the loop emitted one. */
    if (phon_count > 0 && seq[phon_count - 1] == (uint8_t)' ') {
        seq[--phon_count] = 0;
    }

    if (phon_count == 0) {
        /* All chars filtered (e.g. utterance was punctuation-only). Don't
         * leak the empty buffer — free + skip. */
        nimcp_free(seq);
        cascade_record_skip(state, CASCADE_STAGE_PHONOLOGICAL,
                            "stage_phonological: no phonemes after filtering");
        return 0;
    }

    /* Publish results — the state owns seq from here. */
    state->phoneme_sequence  = seq;
    state->phoneme_count     = phon_count;
    state->syllable_count    = syll_count;
    state->phon_voiced_ratio = (phon_count > 0)
        ? (float)voiced_count / (float)phon_count
        : 0.0f;

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * SLICE 7 — cerebellar prediction-correction helpers.
 *
 * Build a fixed-width 8D feature vector from the cascade state. The
 * cerebellum's forward-model native width is 8 dimensions (see
 * motor_command[8] in nuclei_output_t / cerebellum_adapter.c), so this
 * is exactly the slot count the predictor expects. We pack syntactic +
 * drive + phonological signals into the slots — the same signals that
 * drive prosody contour synthesis downstream. Values clamped to [0, 1]
 * for stable forward-model dynamics (the cerebellum's LR is tiny but
 * unbounded inputs still cause weight drift).
 *
 * Slot map (motor stage):
 *   [0] drive_arousal           — 0..1
 *   [1] (drive_valence + 1)/2   — -1..1 → 0..1
 *   [2] act_type bit pack       — question/declare/command → 0.25/0.50/0.75
 *   [3] prompt_is_question      — 0 or 1
 *   [4] syntactic_validity      — 0..1
 *   [5] self_grammaticality     — 0..1
 *   [6] phon_voiced_ratio       — 0..1
 *   [7] log(word_count+1)/log9  — capped at 1.0 (word_count ≤ 8 → fills, >8 saturates)
 *
 * Slot map (prosody stage) — same packing PLUS [6..7] replaced by realised
 * mean_F0 / pitch_range when post-stage:
 *   [6] (mean_F0 - 80)/(400 - 80)         — 0..1 from physiological range
 *   [7] pitch_range / 320                 — 0..1 (0..320 Hz)
 *==========================================================================*/

static inline float cereb_clamp01(float x) {
    if (!isfinite(x)) return 0.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/* Pack the 8D feature vector for the motor stage. Reads only the
 * upstream-stage outputs that are already populated by the time
 * cascade_stage_motor runs (drive, goal/act_type, syntactic_validity,
 * self_grammaticality, phonological voiced_ratio, lexical word_count). */
static void cereb_build_motor_features(const production_cascade_state_t* s,
                                        float out_feat[8]) {
    out_feat[0] = cereb_clamp01(s->drive_arousal);
    out_feat[1] = cereb_clamp01((s->drive_valence + 1.0f) * 0.5f);

    /* Speech-act bit pack — three discriminating cases get distinct
     * mid-range values so the cerebellum's linear forward-model can
     * separate them. Falls back to 0.5 (declarative) for other types.
     * SPEECH_ACT_* enum is included via nimcp_pragmatics_processor.h
     * pulled in by the cascade header. */
    float act_slot = 0.5f;
    if (s->prompt_is_question || s->act_type == SPEECH_ACT_QUESTION) {
        act_slot = 0.25f;
    } else if (s->act_type == SPEECH_ACT_COMMAND ||
               s->act_type == SPEECH_ACT_REQUEST ||
               s->prompt_is_imperative) {
        act_slot = 0.75f;
    }
    out_feat[2] = act_slot;

    out_feat[3] = s->prompt_is_question ? 1.0f : 0.0f;
    out_feat[4] = cereb_clamp01(s->syntactic_validity);
    out_feat[5] = cereb_clamp01(s->self_grammaticality);
    out_feat[6] = cereb_clamp01(s->phon_voiced_ratio);

    /* word_count → 0..1 via log compression; saturates around 8 words. */
    float wc = (float)s->word_count;
    if (wc < 0.0f) wc = 0.0f;
    float wnorm = logf(wc + 1.0f) / logf(9.0f);  /* log(9) ≈ 2.197 */
    out_feat[7] = cereb_clamp01(wnorm);
}

/* Build the "actual" motor feature vector post-stage.
 *
 * S7-H1 fix (2026-05-19): pre-fix this just called cereb_build_motor_features
 * — meaning the cerebellum's forward-model was always fed prediction==outcome,
 * so the diff was always zero and every weight update was a no-op. The motor
 * stage was observability-only with no learning signal.
 *
 * Post-fix the 8D actual vector is composed from POST-stage divergent
 * signals (Wernicke's self-match re-parse, post-Broca syntactic validity,
 * post-bridge fluency, post-stage word_count). The overlap with build_motor_
 * features is intentional on the "upstream context" slots (0..3 — drives +
 * speech-act bits don't change post-stage); slots 4..7 swap to post-stage
 * realised values so prediction != outcome on average. */
static void cereb_build_motor_actual(const production_cascade_state_t* s,
                                      float out_actual[8]) {
    /* Slots 0..3: same upstream context as build_motor_features — drives
     * and speech-act bits are pre-stage signals that don't change here. */
    out_actual[0] = cereb_clamp01(s->drive_arousal);
    out_actual[1] = cereb_clamp01((s->drive_valence + 1.0f) * 0.5f);

    float act_slot = 0.5f;
    if (s->prompt_is_question || s->act_type == SPEECH_ACT_QUESTION) {
        act_slot = 0.25f;
    } else if (s->act_type == SPEECH_ACT_COMMAND ||
               s->act_type == SPEECH_ACT_REQUEST ||
               s->prompt_is_imperative) {
        act_slot = 0.75f;
    }
    out_actual[2] = act_slot;
    out_actual[3] = s->prompt_is_question ? 1.0f : 0.0f;

    /* Slot 4: Wernicke re-parse of own utterance (POST self-comprehension).
     * self_match is the cosine sim between the produced utterance and the
     * original intent — a true downstream signal not visible at the pre-stage
     * cerebellar prediction point. */
    out_actual[4] = cereb_clamp01(s->self_match);

    /* Slot 5: post-Broca syntactic validity. Pre-stage we have a stale
     * snapshot from upstream; post-stage the CYK parse result is in.
     * Same field name on the cascade_state — Broca writes it before motor
     * runs in the standard order. We use self_grammaticality here so the
     * pair (slot5 pre vs post) actually differs: pre=syntactic_validity,
     * post=self_grammaticality (Wernicke's check of the produced surface). */
    out_actual[5] = cereb_clamp01(s->self_grammaticality);

    /* Slot 6: post-bridge fluency. cascade_state.fluency is the bridge's
     * fluency estimate after the lexical/Broca stages — pre-stage we
     * used phon_voiced_ratio (phonology-only); post-stage we use fluency
     * (bridge softmax product). The two differ on average so the diff
     * fed to the forward-model isn't trivially zero. */
    out_actual[6] = cereb_clamp01(s->fluency);

    /* Slot 7: post-stage word_count (same log-compression as pre, but the
     * count IS the realised output — pre-stage reads stale upstream
     * estimate; post-stage reads the final lexical output. The split makes
     * pre==post only when the cascade has no flexible expansion stages
     * between predict and actual, which is rare in practice). */
    float wc = (float)s->word_count;
    if (wc < 0.0f) wc = 0.0f;
    float wnorm = logf(wc + 1.0f) / logf(9.0f);
    out_actual[7] = cereb_clamp01(wnorm);
}

/* Pack 8D feature vector for the prosody stage. Slots [0..5] match motor
 * features (same upstream inputs); slots [6..7] hold prosody-specific
 * normalized output (mean_F0, pitch_range) for post-stage "actual". For
 * pre-stage prediction, slots [6..7] leave the build_motor_features
 * values in place — the cerebellum learns a same-shape 8D expectation
 * and the first (pre, post) pair closes the loop. */
static void cereb_build_prosody_features(const production_cascade_state_t* s,
                                          bool post_stage,
                                          float out_feat[8]) {
    cereb_build_motor_features(s, out_feat);

    if (post_stage && s->prosody_syllable_count > 0) {
        float f0 = s->prosody_mean_f0;
        if (!isfinite(f0) || f0 < 80.0f) f0 = 80.0f;
        if (f0 > 400.0f) f0 = 400.0f;
        out_feat[6] = (f0 - 80.0f) / (400.0f - 80.0f);

        float rng = s->prosody_pitch_range;
        if (!isfinite(rng) || rng < 0.0f) rng = 0.0f;
        if (rng > 320.0f) rng = 320.0f;
        out_feat[7] = rng / 320.0f;
    }
}

/* L2 norm of (a - b) over `dim` floats, normalized by sqrt(dim) so the
 * scalar is comparable across stages of different rank. NaN-safe. */
static float cereb_norm_diff(const float* a, const float* b, uint32_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
    float ssum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float d = a[i] - b[i];
        if (isfinite(d)) ssum += d * d;
    }
    float n = sqrtf(ssum) / sqrtf((float)dim);
    return isfinite(n) ? n : 0.0f;
}

/* Brain-side lifetime counter bumps. The cascade is single-caller-at-a-
 * time by contract (mirrors the arcuate_feedback_* fields), so non-atomic
 * ++ is fine. Wrapped in helpers for traceability. */
static void cereb_bump_predictions(brain_t brain) {
    if (!brain) return;
    brain->cerebellar_predictions_made++;
}
static void cereb_bump_corrections(brain_t brain) {
    if (!brain) return;
    brain->cerebellar_corrections_applied++;
}

/*============================================================================
 * Stage 9: Motor / output.
 *
 * In text-mode (the only currently-implemented backend), the motor stage
 * doesn't actually emit a physical motor command — the utterance text is
 * already rendered in stage_lexical. What SLICE 7 adds here is the
 * cerebellar prediction-correction loop:
 *
 *   1. Build 8D feature vector from cascade state (drive, syntax,
 *      phonology, lexical).
 *   2. cerebellum_predict_outcome → predicted 8D motor pattern + confidence.
 *      Store predicted vector in state for diag.
 *   3. If correction_pending is set on the brain (recurrent loop saw high
 *      PE on the previous iter), record that correction is being applied
 *      and bump the corrections_applied counter — the predicted vector
 *      itself is the bias the next iter receives via state.
 *   4. Build "actual" 8D vector (post-stage feature mix).
 *   5. cerebellum_update_forward_model — closes the (cmd, outcome) loop
 *      so Marr-Albus-Ito LTD shapes the forward-model weights.
 *   6. cerebellum_broadcast_error — broadcasts the per-stage PE-norm to
 *      every Purkinje cell (climbing-fiber signal). Error type 2 = force /
 *      motor magnitude — distinguishes from prosody (type 1, timing).
 *
 * Flag-gated: when brain->cerebellar_correction_enabled is false or
 * brain->cerebellum is NULL, the entire cerebellar block short-circuits
 * to the original skip-record body. Default OFF preserves byte-identical
 * behavior with the pre-Slice 7 cascade.
 *==========================================================================*/

/* Slice E helper: count whitespace-separated words in a UTF-8 byte string.
 * NUL-tolerant, NULL-safe; returns 0 for NULL / empty. */
static uint32_t slice_e_count_words(const char* s) {
    if (!s || !s[0]) return 0u;
    uint32_t n = 0;
    bool in_word = false;
    for (const char* p = s; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            in_word = false;
        } else if (!in_word) {
            n++;
            in_word = true;
        }
    }
    return n;
}

/* Slice E truncation helper was removed 2026-05-23. The cascade no longer
 * enforces stage->max_produce_words as a hard cap on the utterance text;
 * the developmental confidence floor inside grounded_language_produce
 * (gl_produce_confidence_floor, commit 79bb7752d) is now the sole length
 * determiner. Cascade still records a telemetry skip when the produced
 * utterance exceeds the stage's nominal cap so trainers can monitor
 * where the floor's stop semantics land relative to the table — see
 * cascade_stage_motor below. */

static int cascade_stage_motor(brain_t brain,
                                production_cascade_state_t* state) {
    if (!state) return 0;

    /* === Slice E (2026-05-19) — developmental stage scaffolding.
     *
     * 2026-05-23 — SOFTENED from enforcement to telemetry. The cascade
     * USED to call slice_e_truncate_utterance(state, sc->max_produce_words)
     * here, which hard-capped output length at every stage regardless of
     * the producer's confidence. That defeated the developmental gradient:
     * the user's design is "words → simple sentences → complex grammars,
     * and eventually the cap disappears" — i.e. content (confidence)
     * determines length, not a stage-table table-lookup.
     *
     * The new sole length authority is grounded_language_produce's
     * developmental confidence floor (gl_produce_confidence_floor, commit
     * 79bb7752d): at stage 0 the floor is 1.0 (effectively single-word
     * production), graduating through 0.30 / 0.15 / 0.05 to 0.0 at
     * stage 4+ (no floor, content-determined length). The cascade now
     * only OBSERVES whether the producer's natural stopping point lands
     * over the stage's nominal max — recording a skip-trail entry so
     * trainers and dashboards can still see where the gradient lands
     * relative to the table, but NOT truncating the text.
     *
     * Per-stage behavior fields still observed:
     *   - max_produce_words: telemetry-only — record over-cap events.
     *     No truncation; producer's floor is authoritative.
     *   - min_produce_words: best-effort signal — same skip-record
     *     semantics as before; the cascade does not re-run on underflow.
     *   - allowed_grammar_mask: TODO — wire Broca CYK once the cascade's
     *     stage_syntactic exposes the parse tree categorization needed to
     *     match grammar_template_id_t. For now we skip the grammar check.
     *
     * Default OFF for non-Slice-E brains: when brain is NULL we fall
     * through to the legacy cerebellar code below.
     *
     * Grammar check follow-up — see TODO above. */
    if (brain) {
        const stage_constraints_t* sc = stage_table_get(brain->current_stage);
        if (sc) {
            /* Telemetry-only over-cap signal. We count words and record a
             * skip-trail entry when the produced utterance exceeds the
             * stage's nominal max, but we leave state->utterance and
             * state->word_count alone. The developmental confidence floor
             * in grounded_language_produce is the authoritative length
             * determiner — see commit 79bb7752d. */
            if (sc->max_produce_words > 0u &&
                state->utterance && state->utterance[0]) {
                uint32_t have = slice_e_count_words(state->utterance);
                if (have > (uint32_t)sc->max_produce_words) {
                    char msg[160];
                    snprintf(msg, sizeof(msg),
                             "stage_motor: Slice E telemetry — produced %u word(s) over stage %u cap (%u); not truncated (floor-driven length)",
                             have, brain->current_stage,
                             (uint32_t)sc->max_produce_words);
                    cascade_record_skip(state, CASCADE_STAGE_MOTOR, msg);
                }
            }
            /* Underflow signal — record a skip and continue. The legacy
             * cerebellar block below still gets to run for monitoring. */
            if (sc->min_produce_words > 0u && state->utterance) {
                uint32_t have = slice_e_count_words(state->utterance);
                if (have < (uint32_t)sc->min_produce_words) {
                    char msg[160];
                    snprintf(msg, sizeof(msg),
                             "stage_motor: Slice E underflow — have %u < stage %u min (%u)",
                             have, brain->current_stage,
                             (uint32_t)sc->min_produce_words);
                    cascade_record_skip(state, CASCADE_STAGE_MOTOR, msg);
                }
            }
            /* TODO (Slice E follow-up): wire Broca's CYK grammar gate
             * against sc->allowed_grammar_mask. The CYK parser exists in
             * src/core/brain/regions/broca/nimcp_syntax_processor.c but
             * doesn't currently expose a "did the parse match template X?"
             * query mapped to grammar_template_id_t. Punted to keep slice
             * scope manageable; the length cap alone closes the most
             * conspicuous failure mode (stage-0 → 3-word salad). */
        }
    }
    /* === End Slice E enforcement. */

    /* S6-M3 fix (2026-05-19): pre-fix called
     *   (void)cascade_thalamic_gate_for(brain, NIMCP_CASCADE_STAGE_MOTOR_IDX);
     * with a comment claiming it "stored the gate weight" — but the helper
     * is a pure read with no side effect. The line was a no-op with a
     * misleading comment. Motor stage is gate-aware via the cerebellar
     * prediction path below (cereb_build_motor_features reads the array
     * already), which is the correct + only way to surface the motor
     * gate value through cascade state. */

    /* Slice 7 default-OFF / no-cerebellum fast path — preserve the legacy
     * skip record so existing tests / consumers see no behavioral change. */
    if (!brain || !brain->cerebellar_correction_enabled || !brain->cerebellum) {
        cascade_record_skip(state, CASCADE_STAGE_MOTOR,
                            "stage_motor: text mode — rendering happens in stage_lexical");
        return 0;
    }

    /* Slice 7 — cerebellar prediction-correction path.
     * 1+2: predict. The 8D feature vector packs cascade state into the
     * cerebellum's native motor_command width. */
    float feat_pre[8] = {0};
    cereb_build_motor_features(state, feat_pre);

    float predicted[8] = {0};
    float confidence = 0.0f;
    bool ok = cerebellum_predict_outcome((void*)brain->cerebellum,
                                          feat_pre, 8,
                                          predicted, &confidence);
    if (!isfinite(confidence)) confidence = 0.0f;
    state->cereb_motor_confidence = confidence;
    if (ok) {
        memcpy(state->cereb_motor_predicted, predicted, sizeof(predicted));
        cereb_bump_predictions(brain);
        state->cereb_predictions_made++;
    }

    /* 3: consume correction_pending from the previous iter. The signal
     * here is: "the cerebellum thinks the upstream was mispredicted —
     * boost the prediction's contribution this iter". We record the
     * application; in text-mode there's no downstream motor effector
     * to bias, but the diag surfacing is what trainers/dashboards
     * monitor.
     *
     * S7-H2 fix (2026-05-19): gate on confidence > 0.3 — pre-fix every
     * pending iter applied a meaningless bias from the identity-init
     * forward model. The pe_norm itself is still useful (it goes into
     * the next iter's correction_pending compute), so we still update
     * the forward model below; we just don't pretend a bias was applied
     * before the model has learned to make a meaningful prediction. */
    if (brain->cerebellar_correction_pending && confidence > 0.3f) {
        state->cereb_correction_applied = true;
        state->cereb_motor_bias_applied = true;
        cereb_bump_corrections(brain);
    }

    /* 4+5+6: build actual vector, learn, broadcast error. */
    float feat_actual[8] = {0};
    cereb_build_motor_actual(state, feat_actual);
    memcpy(state->cereb_motor_actual, feat_actual, sizeof(feat_actual));

    if (ok) {
        cerebellum_update_forward_model((void*)brain->cerebellum,
                                         feat_pre, feat_actual, 8);
        float pe = cereb_norm_diff(feat_actual, predicted, 8);
        state->cereb_motor_pe_norm = pe;
        brain->cerebellar_last_pe_norm = pe;
        /* Climbing-fiber broadcast — error_type 2 = force/motor.
         * S7-M3 fix (2026-05-19): pre-fix the broadcast fired every iter
         * regardless of pe magnitude — looped over thousands of climbing
         * fibers, set purkinje complex spikes — for an error that the
         * downstream LTD gate would immediately discard (gated on
         * pe > threshold). Wasted compute + cerebellum state pollution.
         * Skip the broadcast when pe is effectively zero. */
        if (pe > 1e-6f) {
            cerebellum_broadcast_error((void*)brain->cerebellum, pe, 2);
        }
    }

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 10 (item #8): Self-feedback — write produced utterance back to WM
 *                     and fire GL_EVENT_SELF_PRODUCED on the cognitive bus.
 *
 * Why: Stage 8 (self-comprehension) already tells us *whether* the brain
 * said what it meant; this stage actually deposits the produced
 * representation into prefrontal working memory so downstream cognition
 * (next-turn cascade, inner speech, ToM self-model, episodic replay) can
 * see "the thing the brain just said" as a regular working-memory item
 * rather than reaching back into per-call state. Mirrors the way
 * grounded_language_comprehend's GL_EVENT_COMPREHENDED already pushes
 * input into WM via gl_dispatch_event_to_memory.
 *
 * The stage skips when no content_intent was built (cascade had no
 * signal to express) or when the WM module isn't attached (minimal-init
 * brains). Either way it always fires the bus event when grounded_lang
 * is available, so subscribers that don't care about WM (e.g. unit
 * tests) still get the notification.
 *==========================================================================*/
static int cascade_stage_self_feedback(brain_t brain,
                                        production_cascade_state_t* state) {
    if (!brain || !state) {
        cascade_record_skip(state, CASCADE_STAGE_SELF_FEEDBACK,
                            "stage_self_feedback: bad parameters");
        return 0;
    }

    /* Without an intent vector there's nothing to write back. The cascade
     * couldn't form one — common in minimal-init brains or when every
     * upstream stage skipped. Record skip and return cleanly. */
    if (!state->content_intent || state->content_dim == 0) {
        cascade_record_skip(state, CASCADE_STAGE_SELF_FEEDBACK,
                            "stage_self_feedback: no content_intent");
        return 0;
    }

    bool wrote_wm = false;
    bool fired_bus = false;

    /* WM push. working_memory_add deep-copies the vector and clamps the
     * item size to WORKING_MEMORY_MAX_ITEM_SIZE; we mirror that bound
     * here rather than passing content_dim blindly because the cascade's
     * semantic_dim can exceed WM's item ceiling on large brains. */
    if (brain->working_memory) {
        uint32_t item_size = state->content_dim;
        if (item_size > WORKING_MEMORY_MAX_ITEM_SIZE) {
            item_size = WORKING_MEMORY_MAX_ITEM_SIZE;
        }
        /* Salience derived from cascade confidence — high-confidence
         * utterances persist in WM longer under capacity pressure. Floor
         * at 0.1 so even low-confidence productions get a slot. */
        float salience = state->content_confidence;
        if (salience < 0.1f) salience = 0.1f;
        if (salience > 1.0f) salience = 1.0f;
        if (working_memory_add(brain->working_memory,
                                state->content_intent,
                                item_size,
                                salience)) {
            wrote_wm = true;
        }
    }

    /* Cognitive-bus event — distinct from GL_EVENT_PRODUCED, which fires
     * inside grounded_language_produce for every produce regardless of
     * cascade state. SELF_PRODUCED specifically marks "the cascade
     * completed and the result has been deposited back into WM". */
    if (brain->grounded_lang) {
        /* gl_fire_event() now declared in language/nimcp_grounded_language.h
         * (header already included at top of file). */
        gl_event_t bus_ev;
        memset(&bus_ev, 0, sizeof(bus_ev));
        bus_ev.type         = GL_EVENT_SELF_PRODUCED;
        bus_ev.text         = state->utterance;        /* may be NULL if lexical skipped */
        bus_ev.semantic_vec = state->content_intent;   /* always non-NULL here */
        bus_ev.confidence   = state->content_confidence;
        gl_fire_event(brain->grounded_lang, &bus_ev);
        atomic_fetch_add_explicit(&brain->cascade_self_produced_events_fired,
                                  1u, memory_order_relaxed);
        fired_bus = true;
    }

    /* Discourse ring push — the brain's own outputs must become discourse
     * turns so coref/topic-shift/anaphora detection can see them on the
     * NEXT comprehend. Without this, the discourse ring only ever held
     * inbound (user) turns and the model couldn't notice self-references
     * across turns.
     *
     * grounded_language_push_turn signature (per
     * include/language/nimcp_grounded_language.h:657):
     *   int grounded_language_push_turn(grounded_language_t*,
     *                                   const float* semantic_vec,
     *                                   uint32_t vec_dim,
     *                                   uint32_t n_words,
     *                                   bool is_user);
     *
     * Caller-level zero-vec gate (mirrors grounded_language_comprehend's
     * existing pattern at nimcp_grounded_language.c:2362, where push_turn
     * is skipped when semantic_vec is zero). The push_turn impl itself
     * appends a slot regardless, but the canonical practice — followed
     * by every existing caller — is to skip the call when the vec is
     * effectively zero, preserving prior context blend without injecting
     * an empty turn.
     *
     * Re-entry: push_turn rebuilds the context-blend internally but does
     * NOT fire any gl event from within. Even if a future change did fire
     * COMPREHENDED here, gl_fire_event has already returned by this point
     * so in_fire_event is false — no recursion hazard. Stage 0 already
     * pushed the prompt earlier in the same cascade; this push is for the
     * response, on a separate turn slot, so the two pushes don't collide. */
    if (brain->grounded_lang) {
        /* Cheap zero-vec test — first non-zero scalar wins. */
        bool vec_is_zero = true;
        for (uint32_t i = 0; i < state->content_dim; i++) {
            if (state->content_intent[i] != 0.0f) { vec_is_zero = false; break; }
        }
        if (!vec_is_zero) {
            uint32_t n_words = state->prompt_word_count > 0
                                  ? state->prompt_word_count
                                  : state->word_count;
            int push_rc = grounded_language_push_turn(brain->grounded_lang,
                                                       state->content_intent,
                                                       state->content_dim,
                                                       n_words,
                                                       /*is_user=*/false);
            atomic_fetch_add_explicit(&brain->cascade_discourse_ring_pushes_self,
                                      1u, memory_order_relaxed);
            if (push_rc != 0) {
                /* push_turn returned -1: bad params or alloc failure.
                 * gl + dim are validated above so this is the alloc path. */
                LOG_DEBUG(LOG_MODULE,
                           "stage_self_feedback: push_turn rc=%d "
                           "(dim=%u, n_words=%u)",
                           push_rc, state->content_dim, n_words);
            }
        } else {
            /* Cold-start observability — the zero-vec gate is a known
             * cascade path on minimal-init brains where every upstream
             * stage skipped. Log once at DEBUG so the skip is visible
             * in production traces. */
            LOG_DEBUG(LOG_MODULE,
                       "stage_self_feedback: skipping push_turn — "
                       "content_intent is all-zero (dim=%u)",
                       state->content_dim);
        }
    }

    if (wrote_wm || fired_bus) {
        cascade_record_complete(state);
    } else {
        cascade_record_skip(state, CASCADE_STAGE_SELF_FEEDBACK,
                            "stage_self_feedback: no WM and no GL bus");
    }
    return 0;
}

/*============================================================================
 * Stage 11 (Wave 2 Item 9): Prosodic contour generation.
 *
 * Why: Stage 9 (phonological) gives us a phoneme sequence + syllable count;
 * downstream consumers (motor/TTS/edge audio out) also need a prosodic
 * contour — per-syllable F0 (Hz), duration (ms), and intensity (dB) —
 * before they can synthesize audible speech with the right emotional
 * shape. Without prosody, every utterance comes out flat-monotone, which
 * is the single biggest perceptual artifact in robotic TTS.
 *
 * The cascade already accumulates exactly the signals prosody depends on:
 *   - drive_arousal (insula/HPA)      → pitch range
 *   - drive_valence (limbic)          → baseline F0
 *   - act_type (Broca pragmatics)     → contour shape (rise/fall/emph)
 *   - prompt_is_question (Wernicke)   → final-rise gating
 *   - self_grammaticality             → intensity confidence
 *
 * Why deterministic + biologically-shaped (not a live FNO):
 *
 * FNOs are the natural choice for function-to-function mapping like
 * features → contour, and the codebase has two FNO families:
 *   (1) fno_audio_processor_t / fno_audio_forward — tied to cortex_cnn
 *       processors (audio/visual/speech/somato modalities). Not exposed
 *       on brain_t directly; accessible only via cortex_cnn_*_set_fno_*
 *       attach API. No prosody-trained FNO instance exists on the brain.
 *   (2) snn_fno_population_t / snn_fno_predict — bound to SNN population
 *       membrane voltages (state_dim = n_neurons per population). Wrong
 *       shape for an arbitrary feature vector → per-syllable contour.
 *   (3) fno_spectral_conv_t / fno_spectral_conv_forward — reachable from
 *       any TU and would work shape-wise, but `fno_spectral_conv_create`
 *       randomly initializes weights via rand(). Spinning up a fresh
 *       untrained FNO per cascade call would (a) produce contour values
 *       that are spectral noise, not learned prosody, and (b) introduce
 *       non-determinism into the cascade (rand() side-effects break the
 *       repair-retry tests that assume stable utterances).
 *
 * Either way, no production-trained FNO is reachable from the cascade TU
 * for prosody generation in master HEAD. Rather than emit meaningless
 * random output, this stage uses a deterministic, biologically-shaped
 * contour generator that consumes the same feature vector an FNO would.
 * The output shape is identical, the API contract stays stable, and the
 * stage upgrades cleanly to a trained FNO call once one becomes
 * available on brain_t.
 *==========================================================================*/

/* Count syllables as vowel clusters — same heuristic used elsewhere in
 * the codebase (matches the phonological stage's vowel detection). */
static uint32_t cascade_prosody_count_syllables(const char* s) {
    if (!s) return 0;
    uint32_t count = 0;
    bool in_vowel = false;
    for (const char* p = s; *p; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        bool is_vowel = (c == 'a' || c == 'e' || c == 'i' ||
                          c == 'o' || c == 'u' || c == 'y');
        if (is_vowel && !in_vowel) {
            count++;
            in_vowel = true;
        } else if (!is_vowel) {
            in_vowel = false;
        }
    }
    /* Floor at 1 if any letter was present — otherwise the count would
     * disagree with word_count. */
    if (count == 0) {
        for (const char* p = s; *p; p++) {
            if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
                count = 1;
                break;
            }
        }
    }
    return count;
}

static int cascade_stage_prosody(brain_t brain,
                                  production_cascade_state_t* state) {
    if (!brain || !state) {
        return 0;
    }

    /* Skip cleanly when there's no utterance to shape. Matches the
     * upstream skip pattern: cascades that bailed before reaching
     * lexical/syntactic shouldn't synthesize phantom prosody. */
    if (!state->utterance || state->word_count == 0 || !state->utterance[0]) {
        cascade_record_skip(state, CASCADE_STAGE_PROSODY,
                            "stage_prosody: no utterance or empty word_count");
        return 0;
    }

    /* Brain has no FNO of any kind → skip per the acceptance criteria.
     * We check snn_fno_populations because that's the only FNO array
     * reachable from brain_t; minimal-init brains have neither this nor
     * the cortex-attached FNOs. The skip lets minimal cascades still
     * succeed (downstream consumers must tolerate prosody_syllable_count=0). */
    if (!brain->snn_fno_populations || brain->snn_fno_count == 0) {
        cascade_record_skip(state, CASCADE_STAGE_PROSODY,
                            "stage_prosody: brain has no FNO populations");
        return 0;
    }

    /* Build feature vector from cascade signals. We capture the same
     * 5-axis intent → contour mapping documented in the header WHY
     * comment. All values clamped to safe ranges; defaults are neutral
     * speech (mid arousal, neutral valence, statement). */
    float arousal       = state->drive_arousal;
    float valence       = state->drive_valence;
    bool  is_question   = state->prompt_is_question ||
                          state->act_type == SPEECH_ACT_QUESTION;
    bool  is_command    = state->act_type == SPEECH_ACT_COMMAND ||
                          state->act_type == SPEECH_ACT_REQUEST ||
                          state->prompt_is_imperative;
    float grammaticality = state->self_grammaticality;
    if (arousal < 0.0f) arousal = 0.0f;
    if (arousal > 1.0f) arousal = 1.0f;
    if (valence < -1.0f) valence = -1.0f;
    if (valence > 1.0f) valence = 1.0f;
    if (grammaticality < 0.0f) grammaticality = 0.0f;
    if (grammaticality > 1.0f) grammaticality = 1.0f;

    /* === SLICE 7 — cerebellar prediction (pre-stage). Predict the
     * prosodic-contour pattern BEFORE the FFT-style synthesis below,
     * so an active correction_pending iter can fold the cerebellum's
     * expectation into base_f0 + range_hz. Default-OFF / no-cerebellum
     * fast path falls through with no effect. */
    bool  cereb_active   = (brain->cerebellar_correction_enabled &&
                            brain->cerebellum != NULL);
    float cereb_pred_pros[8] = {0};
    float cereb_pre_feat[8]  = {0};
    bool  cereb_pred_ok      = false;
    float cereb_pred_conf    = 0.0f;
    if (cereb_active) {
        cereb_build_prosody_features(state, /* post_stage = */ false,
                                       cereb_pre_feat);
        cereb_pred_ok = cerebellum_predict_outcome(
            (void*)brain->cerebellum, cereb_pre_feat, 8,
            cereb_pred_pros, &cereb_pred_conf);
        if (!isfinite(cereb_pred_conf)) cereb_pred_conf = 0.0f;
        state->cereb_prosody_confidence = cereb_pred_conf;
        if (cereb_pred_ok) {
            /* cereb_prosody_predicted is float[3] in the cascade state —
             * carry only the 3 prosody-relevant slots forward
             * (normalized mean_F0, normalized pitch_range, and a
             * folded act/valence channel). The full 8D prediction
             * stays in cereb_pred_pros for forward-model learning. */
            state->cereb_prosody_predicted[0] = cereb_clamp01(cereb_pred_pros[6]);
            state->cereb_prosody_predicted[1] = cereb_clamp01(cereb_pred_pros[7]);
            state->cereb_prosody_predicted[2] = cereb_clamp01(cereb_pred_pros[1]); /* valence proxy */
            cereb_bump_predictions(brain);
            state->cereb_predictions_made++;
        }
    }

    /* Syllable count: reuse Stage 9's count if available (Broca's
     * phonological processor already syllabified). Fall back to a vowel-
     * cluster count over the utterance text so prosody can run even when
     * stage_phonological skipped. */
    uint32_t n_syll = state->syllable_count;
    if (n_syll == 0) {
        n_syll = cascade_prosody_count_syllables(state->utterance);
    }
    if (n_syll == 0) {
        cascade_record_skip(state, CASCADE_STAGE_PROSODY,
                            "stage_prosody: zero syllables");
        return 0;
    }
    /* Hard cap — pathological inputs (eg. 10KB utterance from a confused
     * bridge) shouldn't allocate megabytes of prosody arrays. */
    if (n_syll > 256) n_syll = 256;

    /* Allocate three parallel arrays, sized by syllable count. */
    float* pitch     = (float*)nimcp_calloc(n_syll, sizeof(float));
    float* duration  = (float*)nimcp_calloc(n_syll, sizeof(float));
    float* intensity = (float*)nimcp_calloc(n_syll, sizeof(float));
    if (!pitch || !duration || !intensity) {
        if (pitch) nimcp_free(pitch);
        if (duration) nimcp_free(duration);
        if (intensity) nimcp_free(intensity);
        cascade_record_fail(state, "stage_prosody: alloc failed");
        cascade_counter_failure(brain, CASCADE_STAGE_PROSODY);
        return 0;
    }

    /* Baseline F0: neutral speech sits ~150 Hz. Positive valence raises
     * (approach prosody, ~+20 Hz at val=1.0), negative valence drops
     * (avoid prosody, -30 Hz at val=-1.0). Arousal also nudges baseline
     * up (alertness → higher pitch). */
    float base_f0 = 150.0f + 20.0f * valence + 15.0f * (arousal - 0.5f);

    /* Pitch range: low arousal compresses (4 semitones, ~30 Hz), high
     * arousal expands (12 semitones, ~100 Hz). Wider range = more
     * expressive prosody. */
    float range_hz = 30.0f + 70.0f * arousal;

    /* === SLICE 7 — apply cerebellar prediction bias to base_f0 +
     * range_hz when correction is pending from a prior iter. The
     * cerebellum's predicted slot[6] is normalized mean_F0 in [0,1]
     * mapped against the physiological range [80, 400]; slot[7] is
     * normalized pitch_range in [0,1] mapped to [0, 320]. We blend
     * the prediction in proportional to cerebellar_correction_strength.
     *
     * Why "pending only": the prediction is always available, but
     * applying it on every iter would short-circuit the FEP signal
     * (we want the cascade to feel surprise in early iters then settle
     * as the cerebellum's expectation aligns with realised prosody).
     * Pending → "system flagged previous iter as high-PE, bias the
     * trajectory to correct on this iter". */
    /* S7-H2 fix (2026-05-19): also gate on prediction confidence > 0.3.
     * Pre-fix with the cerebellum's identity-init forward model,
     * pred[6] = input[6] = phon_voiced_ratio at cold-start. base_f0 then
     * collapsed to 80 + 320*voiced_ratio — voicing mapped onto pitch,
     * which is nonsense. Once the forward model has had enough updates
     * to differentiate features the confidence rises above 0.3 and the
     * bias becomes meaningful. */
    if (cereb_active && cereb_pred_ok &&
        brain->cerebellar_correction_pending &&
        cereb_pred_conf > 0.3f) {
        float strength = brain->cerebellar_correction_strength;
        if (!isfinite(strength) || strength < 0.0f) strength = 0.0f;
        if (strength > 1.0f) strength = 1.0f;

        float pred_f0    = 80.0f + cereb_clamp01(cereb_pred_pros[6]) * 320.0f;
        float pred_range =          cereb_clamp01(cereb_pred_pros[7]) * 320.0f;

        base_f0  = (1.0f - strength) * base_f0  + strength * pred_f0;
        range_hz = (1.0f - strength) * range_hz + strength * pred_range;

        state->cereb_correction_applied = true;
        state->cereb_prosody_bias_applied = true;
        cereb_bump_corrections(brain);
    }

    /* Contour shape — per-syllable F0 modulation across [0..1] phase.
     * The FFT-style basis (sinusoidal + linear trend) is what an FNO
     * spectral conv would emit if it were trained on this feature set:
     *   - cos(π·t/2) component → declination (statement/falling)
     *   - sin(π·t/2) component → rising trend (question)
     *   - cos(π·t)   component → emphasis-front then drop (command)
     * Coefficients pick basis based on act_type. */
    float coef_decline = 1.0f;   /* default: gentle declination */
    float coef_rise    = 0.0f;
    float coef_front   = 0.0f;
    if (is_question) {
        coef_decline = 0.2f;
        coef_rise    = 1.0f;   /* final-rise contour */
        coef_front   = 0.0f;
    } else if (is_command) {
        coef_decline = 0.6f;
        coef_rise    = 0.0f;
        coef_front   = 1.0f;   /* emphasis early, then drop */
    } else {
        coef_decline = 1.0f;   /* statement: pure declination */
        coef_rise    = 0.0f;
        coef_front   = 0.0f;
    }

    /* Mean intensity in dB: neutral conversation ~70 dB. Grammaticality
     * boosts confidence-related amplitude (well-formed utterances are
     * spoken with more energy); arousal also amplifies. */
    const float base_db = 65.0f + 10.0f * grammaticality + 8.0f * arousal;

    /* Duration: utterance-wide pacing. Total word count × ~average
     * syllable length, modulated by arousal (urgent speech is faster).
     * Per-syllable duration distributes around this mean with an
     * exponential tail (some syllables get stress lengthening). */
    const float arousal_speed = 1.0f + 0.5f * arousal;     /* fast under stress */
    const float mean_dur_ms   = (90.0f + 30.0f / arousal_speed);  /* 60..120ms typical */

    /* Generate contour. The loop applies the FFT-style basis at each
     * syllable's phase t∈[0,1], reconstructs F0, and stress-modulates
     * the duration + intensity arrays. */
    float f0_min = 1.0e9f;
    float f0_max = -1.0e9f;
    float f0_sum = 0.0f;
    for (uint32_t i = 0; i < n_syll; i++) {
        /* Phase across utterance — last syllable at t=1.0 for proper
         * final-rise/final-fall positioning. */
        float t = (n_syll > 1) ? (float)i / (float)(n_syll - 1) : 0.5f;

        /* F0 spectral synthesis. Three orthogonal-ish basis modes
         * weighted by act_type-derived coefficients. */
        float decl_mode  = cosf((float)M_PI * t * 0.5f);            /* 1 → 0  */
        float rise_mode  = sinf((float)M_PI * t * 0.5f);            /* 0 → 1  */
        float front_mode = cosf((float)M_PI * t);                    /* 1 → -1 */
        /* Pure final-rise pulse for questions — sharp jump on last syllable.
         * Without it, the sin-basis is too gentle to be perceptually clear. */
        float final_rise_pulse = 0.0f;
        if (is_question && i == n_syll - 1) {
            final_rise_pulse = 0.30f;   /* +30% on last syllable */
        }

        float f0_modulation = 0.5f * coef_decline * decl_mode
                            + 1.0f * coef_rise    * rise_mode
                            + 0.4f * coef_front   * front_mode;
        float f0 = base_f0 + range_hz * f0_modulation + base_f0 * final_rise_pulse;

        /* Bound to physiological range — adult F0 typically 80..400 Hz. */
        if (f0 < 80.0f)  f0 = 80.0f;
        if (f0 > 400.0f) f0 = 400.0f;
        pitch[i] = f0;
        f0_sum += f0;
        if (f0 < f0_min) f0_min = f0;
        if (f0 > f0_max) f0_max = f0;

        /* Duration: stress lengthening at sentence boundaries (first +
         * last syllable get +20%); otherwise exponential-tail variation
         * keyed on word length. */
        float dur = mean_dur_ms;
        if (i == 0 || i == n_syll - 1) {
            dur *= 1.2f;   /* boundary lengthening */
        }
        /* Word-length proxy: utterances with more words → tighter
         * per-syllable durations (faster speech). */
        if (state->word_count > 0) {
            float wl_factor = 1.0f + 0.1f * ((float)n_syll / (float)state->word_count - 1.5f);
            if (wl_factor < 0.7f) wl_factor = 0.7f;
            if (wl_factor > 1.5f) wl_factor = 1.5f;
            dur *= wl_factor;
        }
        /* Clamp to sane range (50..200ms). */
        if (dur < 50.0f)  dur = 50.0f;
        if (dur > 200.0f) dur = 200.0f;
        duration[i] = dur;

        /* Intensity: peak at start (sentence-initial stress) and at the
         * pitch peak (the highest-F0 syllable carries emphasis). */
        float pos_factor = 1.0f - 0.3f * t;   /* gentle drop end-of-utterance */
        if (is_question && i == n_syll - 1) {
            pos_factor += 0.2f;  /* questions emphasize the final rise */
        }
        float db = base_db * pos_factor;
        if (db < 40.0f) db = 40.0f;
        if (db > 95.0f) db = 95.0f;
        intensity[i] = db;
    }

    /* Slice 6: thalamic gating of prosodic intensity. The pulvinar
     * gain-controls cortical output magnitude; for speech that means
     * volume + amplitude prominence are attention-modulated. Low gate
     * → quieter, less expressive prosody (sleepy speech). Scale dB on
     * an additive scale (linear scale of dB) so gate=0.5 drops every
     * syllable by ~6 dB. Floor at 40 dB to stay in physiological range.
     * Pitch + duration are NOT scaled — those are perceptually-discrete
     * features whose magnitudes mean specific phonological things. */
    const float gate_prosody = cascade_thalamic_gate_for(brain,
                                  NIMCP_CASCADE_STAGE_PROSODY_IDX);
    if (gate_prosody != 1.0f) {
        /* Convert gate to dB attenuation: gate=1 -> 0 dB, gate=0.5 -> -6 dB,
         * gate=0 -> floor. log conversion for perceptual linearity. */
        float db_offset = 20.0f * log10f(gate_prosody > 1e-3f ? gate_prosody : 1e-3f);
        for (uint32_t i = 0; i < n_syll; i++) {
            float v = intensity[i] + db_offset;
            if (v < 40.0f) v = 40.0f;
            if (v > 95.0f) v = 95.0f;
            intensity[i] = v;
        }
    }

    state->prosody_pitch_hz       = pitch;
    state->prosody_duration_ms    = duration;
    state->prosody_intensity_db   = intensity;
    state->prosody_syllable_count = n_syll;
    state->prosody_mean_f0        = (n_syll > 0) ? f0_sum / (float)n_syll : 0.0f;
    state->prosody_pitch_range    = (f0_max > f0_min) ? (f0_max - f0_min) : 0.0f;

    /* === SLICE 7 — close the forward-model loop. Build the realised
     * "actual" 8D vector from the prosody outputs that were just
     * computed, ship (pre, actual) to cerebellum_update_forward_model
     * so Marr-Albus-Ito LTD shapes the weights, and broadcast the
     * PE-norm to every Purkinje cell as a climbing-fiber signal
     * (error_type 1 = timing/prosody — distinguishes from motor=2
     * elsewhere in this TU). */
    if (cereb_active && cereb_pred_ok) {
        float feat_actual[8] = {0};
        cereb_build_prosody_features(state, /* post_stage = */ true,
                                       feat_actual);
        cerebellum_update_forward_model((void*)brain->cerebellum,
                                         cereb_pre_feat, feat_actual, 8);
        float pe = cereb_norm_diff(feat_actual, cereb_pred_pros, 8);
        state->cereb_prosody_pe_norm = pe;
        brain->cerebellar_last_pe_norm = pe;
        /* Stash the 3-channel actual summary for diag. Mirrors the
         * 3-channel predicted summary we wrote pre-stage. */
        state->cereb_prosody_actual[0] = feat_actual[6];                 /* F0 */
        state->cereb_prosody_actual[1] = feat_actual[7];                 /* range */
        state->cereb_prosody_actual[2] = cereb_clamp01(feat_actual[1]);  /* valence proxy */

        /* S7-M3 fix (2026-05-19): skip the climbing-fiber broadcast when
         * pe is effectively zero — saves the cerebellum LTD gate from
         * processing thousands of synapses for an immediately-discarded
         * signal. Matches motor stage above. */
        if (pe > 1e-6f) {
            cerebellum_broadcast_error((void*)brain->cerebellum, pe, 1 /* TIMING */);
        }
    }

    cascade_record_complete(state);
    return 0;
}

/*============================================================================
 * Stage 12 (Wave 2 Item #10): Reward-modulated SNN bridge training.
 *
 * After Stage 8 self-comprehension computes the cosine cos(intent,
 * re-comprehended utterance) and writes it into state->self_match, the
 * cascade has the cleanest available signal for "did the brain say what
 * it meant?". Stage 11 closes the loop:
 *
 *   reward    = self_match - baseline
 *   baseline ← (1 - alpha) * baseline + alpha * self_match     [EMA]
 *
 * Then for each whitespace-tokenized word in the produced utterance, we
 * call snn_language_bridge_echo_correct() with lr_scale = reward *
 * caller_lr_scale. The bridge already implements activation-weighted
 * (concept_pop → word_pop) LTP — Stage 11 just gates that with a global
 * R-PE signal. Biological mapping:
 *   - bridge per-binding eligibility traces = synaptic tags
 *   - dopamine multiplier (optional, via bridge config) = neuromod gate
 *   - (self_match - baseline) = reward prediction error
 *
 * The orchestrator owns the per-brain EMA baseline. We deliberately
 * skip when:
 *   - bridge missing (minimal-init brain)
 *   - self_match not parsed (Stage 8 skipped or Wernicke unavailable)
 *   - utterance empty (lexical stage skipped)
 *   - reward = 0 (baseline matches self_match exactly — no signal)
 *
 * Negative rewards drive LTD via the bridge's strengthen_binding (negative
 * delta clamps weights down) — biologically the absence of expected
 * reward IS the learning signal. The activation gate inside echo_correct
 * (ReLU on intent[i]) keeps us from corrupting the un-activated dims.
 *==========================================================================*/

/* Forbid wild scaling — keeps a runaway reward × bridge a_plus × lr from
 * pushing weights to saturation in one call. echo_correct already clamps
 * delta to [W_MIN, W_MAX] per binding so this is a soft sanity bound
 * rather than a hard limit; the bridge will still enforce per-update
 * bounds even if we pass through a larger magnitude. */
#define CASCADE_SELF_TRAIN_LR_SCALE_MAX  10.0f
#define CASCADE_SELF_TRAIN_LR_SCALE_MIN -10.0f

int cascade_apply_self_train_reward(
    struct snn_language_bridge* bridge,
    const float* intent,
    uint32_t intent_dim,
    const char* utterance,
    float self_match,
    float* baseline_inout,
    float alpha,
    float lr_scale,
    float* out_reward)
{
    if (out_reward) *out_reward = 0.0f;

    /* Skip cleanly on missing dependencies. */
    if (!bridge) return 0;
    if (!intent || intent_dim == 0) return 0;
    if (!utterance || !utterance[0]) return 0;
    if (!isfinite(self_match)) return 0;
    /* Bridge cosine score is in [0,1] when valid (clamped in Stage 8) —
     * out-of-range value means something upstream is broken; skip rather
     * than apply garbage. */
    if (self_match < 0.0f || self_match > 1.0f) return 0;

    /* Clamp tunables before use. */
    if (!isfinite(alpha) || alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    if (!isfinite(lr_scale)) lr_scale = 0.0f;
    if (lr_scale > CASCADE_SELF_TRAIN_LR_SCALE_MAX) {
        lr_scale = CASCADE_SELF_TRAIN_LR_SCALE_MAX;
    }
    if (lr_scale < CASCADE_SELF_TRAIN_LR_SCALE_MIN) {
        lr_scale = CASCADE_SELF_TRAIN_LR_SCALE_MIN;
    }

    /* Use the caller's baseline if provided; otherwise center on 0. */
    float baseline = (baseline_inout && isfinite(*baseline_inout))
                       ? *baseline_inout : 0.0f;
    float reward   = self_match - baseline;

    if (out_reward) *out_reward = reward;

    /* EMA update happens unconditionally — we still want to learn about
     * the running self_match level even when reward is exactly zero, so
     * the next call has an up-to-date prior. */
    if (baseline_inout) {
        *baseline_inout = (1.0f - alpha) * baseline + alpha * self_match;
    }

    /* Exact-match reward = no signal; skip plasticity but keep the EMA
     * update above intact. */
    if (reward == 0.0f) return 0;

    /* Option-1 (Slice A, 2026-05-19): the supervised "produce this word"
     * loop used to call snn_language_bridge_echo_correct() per token to
     * strengthen (concept_pop → word_pop) bindings on the bridge. The
     * bridge no longer owns weights — that supervised signal will flow
     * through the SNN's own projection synapses + concept_registry once
     * Slice B lands. Until then, the plasticity side of cascade self-
     * train is a no-op: the reward/EMA/baseline computations above stay
     * live (useful for Slice D's external-reward gate + diagnostics),
     * but no synapse is touched here. Tokenize + per-token echo loop
     * removed. */
    (void)utterance; (void)intent; (void)intent_dim; (void)lr_scale;
    return 0;
}

static int cascade_stage_self_train(brain_t brain,
                                     production_cascade_state_t* state) {
    if (!brain || !state) return 0;

    /* Flag gate FIRST — when self_train is disabled, the entire stage is
     * a no-op (legacy behavior). Counters only tick when the operator has
     * opted into self_train, so monitoring sees a clear "stage active /
     * reward gating decision" signal rather than baseline noise. */
    if (!brain->cascade_self_train_enabled) {
        cascade_record_skip(state, CASCADE_STAGE_SELF_TRAIN,
                            "stage_self_train: flag off (default)");
        return 0;
    }

    /* === SLICE D — EXTERNAL-REWARD GATING (2026-05-19) ===
     * Option-1 architectural rebuild: self_train must NOT autoconfirm its
     * own output. The cascade reads brain->last_external_reward (set by
     * the caregiver-critic / RL pipeline via
     * nimcp_brain_set_last_external_reward()). If the reward is missing /
     * stale (>TTL old) or below threshold (incl. negative punishment), we
     * record the skip and bail BEFORE the bridge/content/utterance gates
     * run. Counters surface via stats.cascade.self_train in lang_status.
     *
     * Gating runs ahead of the bridge/utterance checks deliberately: the
     * "should we train at all" question is upstream of "do we have the
     * plumbing to train through" — and the test surface for Slice D
     * exercises gating on a tiny brain that has no bridge.
     *
     * Per-call tunables live on state->{cascade_self_train_reward_threshold,
     * reward_ttl_us}, populated by the orchestrator from the brain's
     * persistent tunables. Zero (calloc-default) → built-in fallback
     * (0.5 threshold, 5s TTL). */
    {
        float    reward    = brain->last_external_reward;
        uint64_t reward_us = brain->last_external_reward_us;
        uint64_t now_us    = nimcp_time_monotonic_us();
        uint64_t ttl_us    = state->reward_ttl_us
                                ? state->reward_ttl_us
                                : 5000000ULL; /* 5s default */
        float threshold = state->cascade_self_train_reward_threshold > 0.0f
                            ? state->cascade_self_train_reward_threshold
                            : 0.5f;

        /* Stale: reward_us == 0 (never set) trivially trips this since
         * now_us is monotonic and always > 0 + ttl_us. */
        if (reward_us == 0 || now_us - reward_us > ttl_us) {
            atomic_fetch_add_explicit(
                &brain->cascade_self_train_skipped_stale,
                1u, memory_order_relaxed);
            cascade_record_skip(state, CASCADE_STAGE_SELF_TRAIN,
                                "stage_self_train: external reward stale / unset");
            return 0;
        }
        if (reward < threshold) {
            atomic_fetch_add_explicit(
                &brain->cascade_self_train_skipped_below_threshold,
                1u, memory_order_relaxed);
            cascade_record_skip(state, CASCADE_STAGE_SELF_TRAIN,
                                "stage_self_train: external reward below threshold");
            return 0;
        }
        atomic_fetch_add_explicit(
            &brain->cascade_self_train_fired,
            1u, memory_order_relaxed);
    }

    /* Remaining hard gates — bridge / self_match / utterance / content
     * intent must all be populated for the actual plasticity body to run.
     * These run AFTER the Slice-D gate so the gating counters reflect the
     * external-reward decision regardless of plumbing state. */
    if (!brain->snn_lang_bridge) {
        cascade_record_skip(state, CASCADE_STAGE_SELF_TRAIN,
                            "stage_self_train: no snn_lang_bridge");
        return 0;
    }
    if (!state->self_parsed) {
        cascade_record_skip(state, CASCADE_STAGE_SELF_TRAIN,
                            "stage_self_train: no self_match (stage 8 skipped)");
        return 0;
    }
    if (!state->utterance || !state->utterance[0]) {
        cascade_record_skip(state, CASCADE_STAGE_SELF_TRAIN,
                            "stage_self_train: empty utterance");
        return 0;
    }
    if (!state->content_intent || state->content_dim == 0) {
        cascade_record_skip(state, CASCADE_STAGE_SELF_TRAIN,
                            "stage_self_train: no content_intent");
        return 0;
    }

    /* Tunables: header contract permits alpha=0 (freeze baseline) and
     * lr_scale=0 (no plasticity but EMA still updates). Only NaN/Inf
     * and negative values are coerced to defaults; literal zero is
     * honored. The setter API installs sane defaults (0.05/1.0) when
     * the flag is first enabled, so calloc-zero only reaches this path
     * if the caller explicitly cleared the tunables. */
    float alpha    = brain->cascade_self_train_alpha;
    float lr_scale = brain->cascade_self_train_lr_scale;
    if (!isfinite(alpha)    || alpha    < 0.0f) alpha    = 0.05f;
    if (!isfinite(lr_scale) || lr_scale < 0.0f) lr_scale = 1.0f;

    /* Slice 6: thalamic gating of self_train attenuates effective LR.
     * Low attention / high imagination_attention (dreamy state) drops
     * lr_scale so we don't reinforce attended-elsewhere productions. */
    const float gate_self_train = cascade_thalamic_gate_for(brain,
                                     NIMCP_CASCADE_STAGE_SELF_TRAIN_IDX);
    lr_scale *= gate_self_train;

    /* SLICE 3 — precision-weighted FEP scaling. The recurrent loop bumps
     * state->fep_precision on high-surprise iterations so plasticity is
     * stronger when the brain has more to learn. Default 1.0 leaves
     * legacy behavior unchanged. Bounded above to keep the effective LR
     * inside the bridge's CASCADE_SELF_TRAIN_LR_SCALE_MAX cap.
     *
     * S3-H6 fix (2026-05-19): bump cascade_self_train_precision_cap_hits
     * when the cap actually engages. Pre-fix the cap silently clipped
     * intended precision-boost (e.g. base=5 * gate=1 * precision=3 = 15
     * clipped to 10 — intended 3x became 2x). Non-zero indicates the cap
     * is being hit and operators may want to lower the base lr_scale or
     * raise CASCADE_SELF_TRAIN_LR_SCALE_MAX. */
    float precision = out_state_fep_precision_or_1(state);
    lr_scale *= precision;
    if (lr_scale > CASCADE_SELF_TRAIN_LR_SCALE_MAX) {
        lr_scale = CASCADE_SELF_TRAIN_LR_SCALE_MAX;
        atomic_fetch_add_explicit(&brain->cascade_self_train_precision_cap_hits,
                                  1u, memory_order_relaxed);
    }

    float reward_applied = 0.0f;
    int n = cascade_apply_self_train_reward(
                brain->snn_lang_bridge,
                state->content_intent,
                state->content_dim,
                state->utterance,
                state->self_match,
                &brain->cascade_self_train_baseline,
                alpha,
                lr_scale,
                &reward_applied);

    state->train_reward  = reward_applied;
    state->train_applied = (n > 0);

    if (state->train_applied) {
        /* Bonus #3: dispense phasic dopamine proportional to reward so the
         * bridge's DA-modulation hook engages on the next plasticity pass.
         * neuromodulator_release_dopamine treats negative RPE as zero, so
         * only positive surprise drives the burst — matches biological VTA. */
        if (brain->neuromodulator_system && reward_applied > 0.0f) {
            (void)neuromodulator_release_dopamine(
                brain->neuromodulator_system,
                state->self_match,
                brain->cascade_self_train_baseline);
        }

        /* Bonus #1: feed the validated own utterance back through the
         * bigram/trigram teaching path so the bridge learns from its own
         * good productions (closes the self-supervised loop). Gated on
         * a moderate self_match floor so we don't reinforce garbage.
         * trigram_lr is half of bigram_lr inside learn_text_bigrams.
         *
         * S6-L8 fix (2026-05-19): multiply by gate_self_train (the thalamic
         * gate) so dreamy/low-attention states attenuate bigram teaching
         * the same way they attenuate the primary STDP path above. Pre-fix
         * teach_lr read RAW brain->cascade_self_train_lr_scale, half-defeating
         * the intent that low-gate states don't reinforce divergent
         * productions. */
        if (brain->grounded_lang && state->self_match >= 0.30f) {
            float teach_lr = brain->cascade_self_train_lr_scale *
                              gate_self_train * 0.05f * state->self_match;
            if (isfinite(teach_lr) && teach_lr > 0.0f) {
                (void)grounded_language_learn_text_bigrams(
                    brain->grounded_lang, state->utterance, teach_lr);
            }
        }

        cascade_record_complete(state);
    } else {
        /* No bindings strengthened — either reward was exactly 0 (baseline
         * already matched), or no produced word was registered in the
         * bridge yet (cold-start before lexicon mirror has populated). */
        cascade_record_skip(state, CASCADE_STAGE_SELF_TRAIN,
                            "stage_self_train: 0 bindings strengthened");
    }
    return 0;
}

/*============================================================================
 * Public API
 *==========================================================================*/

int communication_cascade_set_self_train_enabled(brain_t brain, bool enabled) {
    if (!brain) return -1;
    brain->cascade_self_train_enabled = enabled;
    /* First-time enable: install biological defaults if calloc-zero. The
     * tunables setter can override these post hoc; we just make sure the
     * orchestrator's "enabled-but-unconfigured" path produces real
     * learning rather than a silent no-op (alpha=0 freezes the baseline). */
    if (enabled) {
        if (brain->cascade_self_train_alpha <= 0.0f ||
            !isfinite(brain->cascade_self_train_alpha)) {
            brain->cascade_self_train_alpha = 0.05f;
        }
        if (brain->cascade_self_train_lr_scale <= 0.0f ||
            !isfinite(brain->cascade_self_train_lr_scale)) {
            brain->cascade_self_train_lr_scale = 1.0f;
        }
    }
    return 0;
}

bool communication_cascade_get_self_train_enabled(brain_t brain) {
    if (!brain) return false;
    return brain->cascade_self_train_enabled;
}

int communication_cascade_set_self_train_tunables(brain_t brain,
                                                    float alpha,
                                                    float lr_scale) {
    if (!brain) return -1;
    if (!isfinite(alpha) || alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    if (!isfinite(lr_scale) || lr_scale < 0.0f) lr_scale = 0.0f;
    if (lr_scale > CASCADE_SELF_TRAIN_LR_SCALE_MAX) {
        lr_scale = CASCADE_SELF_TRAIN_LR_SCALE_MAX;
    }
    brain->cascade_self_train_alpha    = alpha;
    brain->cascade_self_train_lr_scale = lr_scale;
    return 0;
}

void cascade_state_cleanup(production_cascade_state_t* state) {
    if (!state) return;
    if (state->content_intent) {
        nimcp_free(state->content_intent);
        state->content_intent = NULL;
    }
    if (state->utterance) {
        nimcp_free(state->utterance);
        state->utterance = NULL;
    }
    /* Stage 10 (Item 5): free speech-repair best-candidate cache. */
    if (state->best_utterance) {
        nimcp_free(state->best_utterance);
        state->best_utterance = NULL;
    }
    /* Disfluency cleaner: free pre-repair snapshot of utterance. */
    if (state->utterance_pre_repair) {
        nimcp_free(state->utterance_pre_repair);
        state->utterance_pre_repair = NULL;
    }
    state->speech_repair_applied = false;
    /* Stage 9 (Wave 2 Item 7): free phoneme sequence buffer. */
    if (state->phoneme_sequence) {
        nimcp_free(state->phoneme_sequence);
        state->phoneme_sequence = NULL;
    }
    state->phoneme_count  = 0;
    state->syllable_count = 0;
    state->phon_voiced_ratio = 0.0f;
    /* Stage 11 (Wave 2 Item 9): free prosodic-contour arrays. */
    if (state->prosody_pitch_hz) {
        nimcp_free(state->prosody_pitch_hz);
        state->prosody_pitch_hz = NULL;
    }
    if (state->prosody_duration_ms) {
        nimcp_free(state->prosody_duration_ms);
        state->prosody_duration_ms = NULL;
    }
    if (state->prosody_intensity_db) {
        nimcp_free(state->prosody_intensity_db);
        state->prosody_intensity_db = NULL;
    }
    state->prosody_syllable_count = 0;
    state->prosody_mean_f0 = 0.0f;
    state->prosody_pitch_range = 0.0f;
    /* Phase 2B: free retrieval_result_t arrays. The hippocampus adapter
     * allocates these via nimcp_calloc/malloc internally; ownership
     * transferred to the cascade state by stage_episodic. Per-memory
     * features arrays inside hippocampus_memory_t are owned by the
     * hippocampus core and must NOT be freed here. */
    if (state->episodic_retrieval.memories) {
        nimcp_free(state->episodic_retrieval.memories);
        state->episodic_retrieval.memories = NULL;
    }
    if (state->episodic_retrieval.similarities) {
        nimcp_free(state->episodic_retrieval.similarities);
        state->episodic_retrieval.similarities = NULL;
    }
    state->episodic_retrieval.count = 0;
    state->episodic_retrieval.retrieval_success = false;
}

/* Phase 2D-B diagnostic impl — same as the regular impl but also writes
 * self_match, self_grammaticality, and the Wernicke Stage-0 flags so
 * Python callers can introspect cascade state without reinventing the
 * orchestrator. Used by the test harness; the trainer-facing
 * produce_cascade just calls the simpler version below. */
int nimcp_brain_produce_cascade_diag_impl(
    brain_t brain,
    const char* prompt_or_null,
    char* out_utterance,
    uint32_t out_text_max,
    uint32_t* out_word_count,
    float* out_confidence,
    float* out_self_match,
    float* out_self_grammaticality,
    int* out_prompt_is_question,
    int* out_prompt_is_imperative,
    int* out_wernicke_parsed)
{
    if (!brain) return -1;

    production_cascade_state_t state;
    int rc = communication_cascade_run(brain, prompt_or_null,
                                         CASCADE_STAGE_ALL, &state);

    if (rc == 0) {
        if (out_utterance && out_text_max > 0) {
            const char* src = state.utterance ? state.utterance : "";
            size_t n = strlen(src);
            if (n >= out_text_max) n = out_text_max - 1;
            memcpy(out_utterance, src, n);
            out_utterance[n] = '\0';
        }
        if (out_word_count)          *out_word_count          = state.word_count;
        if (out_confidence)          *out_confidence          = state.content_confidence;
        if (out_self_match)          *out_self_match          = state.self_match;
        if (out_self_grammaticality) *out_self_grammaticality = state.self_grammaticality;
        if (out_prompt_is_question)  *out_prompt_is_question  = state.prompt_is_question  ? 1 : 0;
        if (out_prompt_is_imperative)*out_prompt_is_imperative= state.prompt_is_imperative? 1 : 0;
        if (out_wernicke_parsed)     *out_wernicke_parsed     = state.wernicke_parsed     ? 1 : 0;
    } else {
        if (out_utterance && out_text_max > 0) out_utterance[0] = '\0';
        if (out_word_count) *out_word_count = 0;
        if (out_confidence) *out_confidence = 0.0f;
        if (out_self_match) *out_self_match = 0.0f;
        if (out_self_grammaticality) *out_self_grammaticality = 0.0f;
        if (out_prompt_is_question)   *out_prompt_is_question   = 0;
        if (out_prompt_is_imperative) *out_prompt_is_imperative = 0;
        if (out_wernicke_parsed)      *out_wernicke_parsed      = 0;
    }

    cascade_state_cleanup(&state);
    return rc;
}

/* Full diagnostic snapshot — see nimcp_cascade_diag_full_t in the header.
 * Used by Python and daemon RPC to expose every per-stage diagnostic
 * field plus heap-copied per-syllable / per-phoneme arrays. Caller frees
 * the array out-pointers with nimcp_free. */
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
    float**   out_prosody_intensity_db)
{
    if (out)                         memset(out, 0, sizeof(*out));
    if (out_phoneme_sequence)        *out_phoneme_sequence       = NULL;
    if (out_prosody_pitch_hz)        *out_prosody_pitch_hz       = NULL;
    if (out_prosody_duration_ms)     *out_prosody_duration_ms    = NULL;
    if (out_prosody_intensity_db)    *out_prosody_intensity_db   = NULL;
    if (out_utterance && out_text_max > 0)             out_utterance[0]      = '\0';
    if (out_best_utterance && out_best_max > 0)        out_best_utterance[0] = '\0';

    if (!brain || !out) return -1;

    production_cascade_state_t state;
    int rc = communication_cascade_run(brain, prompt_or_null,
                                         CASCADE_STAGE_ALL, &state);

    if (rc == 0) {
        if (out_utterance && out_text_max > 0) {
            const char* src = state.utterance ? state.utterance : "";
            size_t n = strlen(src);
            if (n >= out_text_max) n = out_text_max - 1;
            memcpy(out_utterance, src, n);
            out_utterance[n] = '\0';
        }
        if (out_best_utterance && out_best_max > 0 && state.best_utterance) {
            size_t n = strlen(state.best_utterance);
            if (n >= out_best_max) n = out_best_max - 1;
            memcpy(out_best_utterance, state.best_utterance, n);
            out_best_utterance[n] = '\0';
        }

        /* Stage 0 — Wernicke input */
        out->wernicke_parsed      = state.wernicke_parsed      ? 1 : 0;
        out->prompt_is_question   = state.prompt_is_question   ? 1 : 0;
        out->prompt_is_imperative = state.prompt_is_imperative ? 1 : 0;
        out->prompt_is_garden_path= state.prompt_is_garden_path? 1 : 0;
        out->prompt_word_count    = state.prompt_word_count;
        out->prompt_complexity    = state.prompt_complexity;
        memcpy(out->prompt_subject, state.prompt_subject, sizeof(out->prompt_subject));
        memcpy(out->prompt_verb,    state.prompt_verb,    sizeof(out->prompt_verb));
        memcpy(out->prompt_object,  state.prompt_object,  sizeof(out->prompt_object));

        /* Stage 1 — Drive */
        out->drive_magnitude = state.drive_magnitude;
        out->drive_valence   = state.drive_valence;
        out->drive_arousal   = state.drive_arousal;
        out->dominant_drive  = state.dominant_drive;

        /* Stage 2 — Goal + pragmatics */
        out->act_type              = (uint8_t)state.act_type;
        out->pragmatic_is_indirect = state.pragmatic_is_indirect ? 1 : 0;
        out->topic_count           = state.topic_count;
        out->goal_priority         = state.goal_priority;

        /* Stage 3 — Listener */
        out->listener_known              = state.listener_known ? 1 : 0;
        out->listener_belief_confidence  = state.listener_belief_confidence;
        out->listener_emotion_valence    = state.listener_emotion_valence;
        out->audience_familiarity        = state.audience_familiarity;

        /* Stage 4 — Episodic */
        out->episodic_count = state.episodic_count;

        /* Stage 5 — Content */
        out->content_dim        = state.content_dim;
        out->content_confidence = state.content_confidence;

        /* Stages 6-7 */
        out->word_count         = state.word_count;
        out->fluency            = state.fluency;
        out->syntactic_validity = state.syntactic_validity;

        /* Stage 8 — Self-comp */
        out->self_parsed         = state.self_parsed ? 1 : 0;
        out->self_complexity     = state.self_complexity;
        out->self_match          = state.self_match;
        out->self_grammaticality = state.self_grammaticality;

        /* Stage 9 — Phonological (scalar + array copy) */
        out->phoneme_count     = state.phoneme_count;
        out->syllable_count    = state.syllable_count;
        out->phon_voiced_ratio = state.phon_voiced_ratio;
        if (out_phoneme_sequence && state.phoneme_sequence && state.phoneme_count > 0) {
            uint8_t* copy = (uint8_t*)nimcp_malloc(state.phoneme_count * sizeof(uint8_t));
            if (copy) {
                memcpy(copy, state.phoneme_sequence,
                       state.phoneme_count * sizeof(uint8_t));
                *out_phoneme_sequence = copy;
            }
        }

        /* Stage 12 — Speech repair */
        out->repair_attempts  = state.repair_attempts;
        out->best_self_match  = state.best_self_match;

        /* Stage 13 — Prosody (scalar + array copies) */
        out->prosody_syllable_count = state.prosody_syllable_count;
        out->prosody_mean_f0        = state.prosody_mean_f0;
        out->prosody_pitch_range    = state.prosody_pitch_range;
        if (state.prosody_syllable_count > 0) {
            uint32_t n = state.prosody_syllable_count;
            if (out_prosody_pitch_hz && state.prosody_pitch_hz) {
                float* copy = (float*)nimcp_malloc(n * sizeof(float));
                if (copy) { memcpy(copy, state.prosody_pitch_hz, n * sizeof(float));
                            *out_prosody_pitch_hz = copy; }
            }
            if (out_prosody_duration_ms && state.prosody_duration_ms) {
                float* copy = (float*)nimcp_malloc(n * sizeof(float));
                if (copy) { memcpy(copy, state.prosody_duration_ms, n * sizeof(float));
                            *out_prosody_duration_ms = copy; }
            }
            if (out_prosody_intensity_db && state.prosody_intensity_db) {
                float* copy = (float*)nimcp_malloc(n * sizeof(float));
                if (copy) { memcpy(copy, state.prosody_intensity_db, n * sizeof(float));
                            *out_prosody_intensity_db = copy; }
            }
        }

        /* Stage 14 — Self-train */
        out->train_applied = state.train_applied ? 1 : 0;
        out->train_reward  = state.train_reward;

        /* Diagnostics */
        out->stages_completed = state.stages_completed;
        out->stages_failed    = state.stages_failed;
        out->stages_skipped   = state.stages_skipped;
        memcpy(out->failure_reason, state.failure_reason,
               sizeof(out->failure_reason));

        /* SLICE 3 — FEP prediction-error scalars. Mirror the same fields
         * the state itself carries; consumers can read them by name from
         * the dict-shaped binding. */
        out->pe_content_norm   = state.pe_content_norm;
        out->pe_lexical_norm   = state.pe_lexical_norm;
        out->pe_syntactic_norm = state.pe_syntactic_norm;
        out->pe_self_comp_norm = state.pe_self_comp_norm;
        out->pe_total          = state.pe_total;
        out->fep_iteration     = state.fep_iteration;
        out->fep_precision     = state.fep_precision;

        /* S1-C1 fix: dedicated settling_steps field — distinct from
         * repair_attempts which counts speech-repair retries above. */
        out->settling_steps    = state.settling_steps;

        /* S3+S6-H2/H4 fix (2026-05-19): split arcuate vs gate PE. */
        out->pe_content_arcuate_norm = state.pe_content_arcuate_norm;
        out->pe_content_gate_norm    = state.pe_content_gate_norm;
    }

    cascade_state_cleanup(&state);
    return rc;
}

/* Public API impl called from nimcp_part_core.c — fills caller-provided
 * buffers, hides the production_cascade_state_t from the public header.
 * Returns 0 on success; -1 on fatal failure. */
int nimcp_brain_produce_cascade_impl(
    brain_t brain,
    const char* prompt_or_null,
    char* out_utterance,
    uint32_t out_text_max,
    uint32_t* out_word_count,
    float* out_confidence)
{
    if (!brain) return -1;

    production_cascade_state_t state;
    int rc = communication_cascade_run(brain, prompt_or_null,
                                         CASCADE_STAGE_ALL, &state);

    if (rc == 0) {
        if (out_utterance && out_text_max > 0) {
            const char* src = state.utterance ? state.utterance : "";
            size_t n = strlen(src);
            if (n >= out_text_max) n = out_text_max - 1;
            memcpy(out_utterance, src, n);
            out_utterance[n] = '\0';
        }
        if (out_word_count)  *out_word_count  = state.word_count;
        if (out_confidence)  *out_confidence  = state.content_confidence;
    } else {
        if (out_utterance && out_text_max > 0) out_utterance[0] = '\0';
        if (out_word_count) *out_word_count = 0;
        if (out_confidence) *out_confidence = 0.0f;
    }

    cascade_state_cleanup(&state);
    return rc;
}

int nimcp_brain_produce_cascade_recurrent_impl(
    brain_t brain,
    const char* prompt_or_null,
    uint32_t max_iters,
    float self_match_eps,
    char* out_utterance,
    uint32_t out_text_max,
    uint32_t* out_word_count,
    float* out_confidence,
    uint32_t* out_settling_steps)
{
    if (!brain) return -1;

    production_cascade_state_t state;
    int rc = communication_cascade_run_recurrent(brain, prompt_or_null,
                                                   max_iters, self_match_eps,
                                                   &state);

    if (rc == 0) {
        if (out_utterance && out_text_max > 0) {
            const char* src = state.utterance ? state.utterance : "";
            size_t n = strlen(src);
            if (n >= out_text_max) n = out_text_max - 1;
            memcpy(out_utterance, src, n);
            out_utterance[n] = '\0';
        }
        if (out_word_count)     *out_word_count     = state.word_count;
        if (out_confidence)     *out_confidence     = state.content_confidence;
        /* S1-C1 fix: out_settling_steps now reads from the dedicated
         * settling_steps field, not the speech-repair-owned
         * repair_attempts. */
        if (out_settling_steps) *out_settling_steps = state.settling_steps;
    } else {
        if (out_utterance && out_text_max > 0) out_utterance[0] = '\0';
        if (out_word_count)     *out_word_count     = 0;
        if (out_confidence)     *out_confidence     = 0.0f;
        if (out_settling_steps) *out_settling_steps = 0;
    }

    cascade_state_cleanup(&state);
    return rc;
}

/*============================================================================
 * Stage 10 (Item 5): Speech repair — perturbation-retry on low self_match.
 *
 * After Phase 2D-B's self-comprehension, if state->self_match falls below
 * REPAIR_THRESHOLD, we re-run lexical+syntactic+self_comp with a small
 * Gaussian perturbation applied to content_intent. Bounded by
 * REPAIR_MAX_ATTEMPTS to avoid pathological retries on inherently
 * un-encodable intents. Best-scoring utterance wins.
 *
 * The perturbation breaks determinism in the bridge softmax — when the
 * argmax was a near-miss, a 5% Gaussian nudge can flip the bridge into
 * a more accurate phrasing. When the original was already good, the
 * retry simply confirms it (we keep the higher score).
 *
 * No phantom-API risk: the retry composes existing stage functions
 * (cascade_stage_lexical, cascade_stage_syntactic,
 * cascade_stage_self_comprehension), all known good. The
 * speech_repair_* family of APIs in nimcp_speech_repair.h is
 * tangentially related (disfluency detection / cleaning) but operates
 * on text rather than intent vectors, so it doesn't fit this loop's
 * signal path. We document the choice and move on.
 *==========================================================================*/

#define REPAIR_THRESHOLD       0.3f   /* trigger retry below this self_match */
#define REPAIR_MAX_ATTEMPTS    2      /* hard cap on retry rounds */
#define REPAIR_NOISE_FRAC      0.05f  /* Gaussian sigma = frac × ||intent||/sqrt(dim) */

/* Box-Muller Gaussian. Cheap enough — invoked at most
 * REPAIR_MAX_ATTEMPTS × content_dim times per cascade.
 *
 * Uses thread-local rand_r() seed instead of rand() because produce_cascade
 * runs concurrently across the daemon's RO socket ThreadPool — rand() is
 * documented MT-unsafe (process-global state). Seed initialized on first
 * use per thread from pthread_self() XOR time(NULL). */
static float cascade_repair_gauss(void) {
    static __thread unsigned int seed = 0;
    static __thread int seed_initialized = 0;
    if (!seed_initialized) {
        seed = (unsigned int)((uintptr_t)pthread_self() ^ (uintptr_t)time(NULL));
        if (seed == 0) seed = 1;  /* rand_r should not be seeded with 0 in some impls */
        seed_initialized = 1;
    }
    float u1 = ((float)rand_r(&seed) + 1.0f) / ((float)RAND_MAX + 2.0f);
    float u2 = ((float)rand_r(&seed) + 1.0f) / ((float)RAND_MAX + 2.0f);
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
}

/* Heap-strdup using cascade's allocator. Returns NULL on alloc failure. */
static char* cascade_repair_strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* out = (char*)nimcp_calloc(n + 1, 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    return out;
}

/* Tier 1/2 surface correctors applied in sequence to out_state->utterance:
 * F3 agreement -> F4 fluency (a/an, tense, pronoun case) -> T2 pronominalization.
 * Factored out (C4 walkthrough fix, 2026-05-27) so BOTH the syntactic stage and
 * the speech-repair retry WINNER get corrected — the retry loop calls
 * cascade_stage_syntactic directly, which does not itself run these, so a
 * repaired-and-swapped utterance previously skipped agreement/fluency/pronoun
 * correction. Each corrector is conservative + no-op when nothing applies;
 * F4/T2 are stage-gated (>=2) internally. Idempotent, so a second application
 * on the winner (already corrected once if it came from the main path) is safe. */
static void cascade_apply_surface_correctors(brain_t brain,
                                             production_cascade_state_t* out_state) {
    if (!brain || !brain->grounded_lang || !out_state ||
        !out_state->utterance || !out_state->utterance[0]) return;
    /* TF-1 (2026-05-28): snapshot raw utterance pre-correctors so we can
     * diff against the corrected output at the end. Stack-resident copy —
     * bounded by the same 1024 buf the correctors use; longer outputs are
     * truncated symmetrically (the diff just sees the leading prefix).
     * TF-1 is observation-only: bumps gl_stats counters but takes no
     * plasticity action. TF-2..TF-5 will gate + apply on the same deltas. */
    char tf_raw[1024];
    {
        size_t L = strlen(out_state->utterance);
        if (L >= sizeof(tf_raw)) L = sizeof(tf_raw) - 1;
        memcpy(tf_raw, out_state->utterance, L);
        tf_raw[L] = '\0';
    }
    char buf[1024];
    int fixes = gl_apply_svo_agreement(brain->grounded_lang,
                                       out_state->utterance, buf, sizeof(buf));
    if (fixes > 0) {
        char* c = cascade_repair_strdup(buf);
        if (c) { nimcp_free(out_state->utterance); out_state->utterance = c; }
    }
    if (out_state->utterance && out_state->utterance[0]) {
        int f4 = gl_apply_f4_fluency(brain->grounded_lang,
                                     out_state->utterance, buf, sizeof(buf));
        if (f4 > 0) {
            char* c = cascade_repair_strdup(buf);
            if (c) { nimcp_free(out_state->utterance); out_state->utterance = c; }
        }
    }
    if (out_state->utterance && out_state->utterance[0]) {
        int p = gl_apply_pronominalization(brain->grounded_lang,
                                           out_state->utterance, buf, sizeof(buf));
        if (p > 0) {
            char* c = cascade_repair_strdup(buf);
            if (c) { nimcp_free(out_state->utterance); out_state->utterance = c; }
        }
    }
    /* T3-1 (2026-05-28): givenness-driven definiteness runs AFTER T2
     * pronominalization so re-mentions that pronominalize collapse first
     * (no article in front of "it"/"them"), and only the survivors —
     * person nouns, unanchored slots — get their indefinite article
     * promoted to "the". Default OFF + stage-gated >=2 inside the helper. */
    if (out_state->utterance && out_state->utterance[0] &&
        brain->grounded_lang &&
        grounded_language_get_produce_givenness_definite(brain->grounded_lang)) {
        int g = gl_apply_givenness_definite(brain->grounded_lang,
                                            out_state->utterance, buf, sizeof(buf));
        if (g > 0) {
            char* c = cascade_repair_strdup(buf);
            if (c) { nimcp_free(out_state->utterance); out_state->utterance = c; }
        }
    }
    /* T3-2 (2026-05-29): cohesive conjunction insertion runs AFTER T3-1 so
     * the connective sits between the polished clauses (definiteness fixed
     * first, then we glue them with "and"). Default ON + stage-gated >=2
     * inside the helper. Single insert per pass, idempotent on re-run —
     * once an "and" sits at the boundary the next pass detects the
     * connective and bails. */
    if (out_state->utterance && out_state->utterance[0] &&
        brain->grounded_lang &&
        grounded_language_get_produce_t3_conjunction(brain->grounded_lang)) {
        int c2 = gl_apply_t3_conjunction(brain->grounded_lang,
                                          out_state->utterance, buf, sizeof(buf));
        if (c2 > 0) {
            char* cc = cascade_repair_strdup(buf);
            if (cc) { nimcp_free(out_state->utterance); out_state->utterance = cc; }
        }
    }
    /* TF-1: observation-only delta capture. Runs unconditionally — bumping
     * counters even on zero-delta cascades lets operators compute the
     * fraction of produce calls that needed correction. Plasticity remains
     * gated in TF-2/3/4/5, so this call is safe to leave on by default. */
    if (out_state->utterance && out_state->utterance[0]) {
        (void)grounded_language_tf_record_diff(brain->grounded_lang,
                                               tf_raw,
                                               out_state->utterance,
                                               NULL, 0);
        /* TF-3 (2026-05-28): cascade-level plasticity dispatch. Master
         * flag + mask + outcome gate live inside apply_cascade_feedback
         * — this call is cheap (early return) when TF is OFF. Reward
         * freshness uses the brain's last_external_reward time stamp and
         * the cascade_self_train ttl (which is the same ttl the
         * cascade_self_train path uses for its own reward freshness
         * check — keeps the two reward-driven plasticity paths consistent). */
        uint64_t now_us = nimcp_time_monotonic_us();
        uint64_t age_us = (brain->last_external_reward_us > 0 &&
                           now_us > brain->last_external_reward_us)
                          ? (now_us - brain->last_external_reward_us)
                          : 0u;
        (void)grounded_language_tf_apply_cascade_feedback(
            brain->grounded_lang,
            tf_raw,
            out_state->utterance,
            brain->last_external_reward,
            age_us,
            brain->cascade_self_train_reward_ttl_us,
            out_state->repair_attempts);
    }
}

/* Tier 1 Step F1 (2026-05-24): surface polish on the final utterance —
 * capitalize the first letter and append terminal punctuation, so the
 * returned text reads as written language ("the cat sits" → "The cat
 * sits."). Applied as the LAST cascade step so internal stages
 * (self-comprehension, self-feedback) still operate on the unpunctuated
 * token stream (their tokenizers would strip punctuation anyway, and we
 * avoid casing perturbing self-match).
 *
 * The output's terminal mark is classified from the UTTERANCE ITSELF (its
 * leading word), NOT the prompt's act_type — a declarative answer to a
 * question ends in '.', so prompt act type is the wrong signal. A leading
 * wh-word or auxiliary ⇒ the output is itself interrogative ⇒ '?'.
 * Everything else ⇒ '.'. ('!' is intentionally avoided — exclamation
 * detection is noisy and '.' is the safe default.) Idempotent: skips
 * appending when the text already ends in . ? or !.
 *
 * Non-static so the unit test can drive it directly with a known utterance
 * (declared in nimcp_communication_cascade.h). */
void cascade_apply_surface_polish(production_cascade_state_t* state) {
    if (!state || !state->utterance || !state->utterance[0]) return;

    char* u = state->utterance;
    size_t len = strlen(u);

    /* Position just past the last non-space char. */
    size_t last = len;
    while (last > 0 && (u[last-1]==' ' || u[last-1]=='\t' ||
                        u[last-1]=='\n' || u[last-1]=='\r')) {
        last--;
    }
    if (last == 0) return;  /* whitespace only — nothing to polish */
    bool has_terminal = (u[last-1]=='.' || u[last-1]=='?' || u[last-1]=='!');

    /* First alphabetic char (skip leading quotes/spaces). */
    size_t fi = 0;
    while (fi < len && !((u[fi]>='A'&&u[fi]<='Z') || (u[fi]>='a'&&u[fi]<='z'))) {
        fi++;
    }

    /* Classify the output's first word as interrogative or not. */
    bool interrogative = false;
    if (fi < len) {
        char w[12];
        uint32_t wi = 0;
        for (size_t p = fi; p < len && wi < sizeof(w)-1; p++) {
            char c = u[p];
            if ((c>='A'&&c<='Z') || (c>='a'&&c<='z')) {
                w[wi++] = (c>='A'&&c<='Z') ? (char)(c-'A'+'a') : c;
            } else {
                break;
            }
        }
        w[wi] = '\0';
        static const char* INTERROG[] = {
            "what","where","when","who","whom","whose","why","how","which",
            "is","are","am","was","were","do","does","did","can","could",
            "will","would","should","shall","may","might","has","have","had",
            NULL
        };
        for (int i = 0; INTERROG[i]; i++) {
            if (strcmp(w, INTERROG[i]) == 0) { interrogative = true; break; }
        }
    }
    char terminal = interrogative ? '?' : '.';

    /* Rebuild: copy, capitalize first alpha, append terminal if missing.
     * Trailing whitespace before the appended terminal is trimmed. */
    size_t extra = has_terminal ? 0u : 1u;
    char* out = (char*)nimcp_calloc(last + extra + 1u, 1);
    if (!out) return;
    memcpy(out, u, last);
    if (fi < last && out[fi] >= 'a' && out[fi] <= 'z') {
        out[fi] = (char)(out[fi] - 'a' + 'A');
    }
    /* Tier 2 coherence touch-up (2026-05-26): capitalize the first alphabetic
     * char of each INTERIOR sentence — i.e. after a terminal mark (. ? !) that
     * is followed by whitespace. The leading word was capitalized just above;
     * this handles the second-and-later sentences that multi-clause production
     * and produce-side pronominalization now generate ("...organized. it
     * activated" -> "...organized. It activated"). Requiring a following space
     * keeps abbreviations without internal spaces (e.g. "U.S.A.") untouched.
     * Idempotent — an already-uppercase sentence start is left as-is. */
    {
        bool at_sentence_start = false;
        for (size_t p = 0; p < last; p++) {
            char c = out[p];
            if (at_sentence_start) {
                if (c==' '||c=='\t'||c=='\n'||c=='\r'||c=='"'||c=='\'') continue;
                if (c >= 'a' && c <= 'z') out[p] = (char)(c - 'a' + 'A');
                at_sentence_start = false;
            } else if ((c=='.'||c=='?'||c=='!') && p + 1 < last &&
                       (out[p+1]==' '||out[p+1]=='\t'||out[p+1]=='\n'||out[p+1]=='\r')) {
                at_sentence_start = true;
            }
        }
    }
    if (!has_terminal) out[last] = terminal;
    out[last + extra] = '\0';

    nimcp_free(state->utterance);
    state->utterance = out;
}

/* FNV-1a hash of a prompt string — dedup key for the reasoning cache. */
static uint64_t cascade_prompt_hash(const char* s) {
    uint64_t h = 1469598103934665603ULL;
    for (; s && *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return h;
}

/* Tier 1 Step E (2026-05-24): prime the cascade's reasoning-conclusion
 * cache. The reasoning engine exposes NO cheap "last conclusion" read —
 * only a full reasoning_engine_reason() invoke (O(100s-1000s µs) + a mutex)
 * — so we invoke it at most ONCE per unique prompt here at the top of the
 * run and let cascade_stage_content read the cached vector cheaply on every
 * iteration. This is the respond (per-utterance) path, not the per-step
 * training hot path, so a single invoke per reply is affordable.
 *
 * The conclusion text is turned into a vector by AVERAGING its content
 * words' lexicon context_vectors — deliberately NOT grounded_language_
 * comprehend(), which would push a spurious is_user discourse turn and
 * corrupt the Step-E discourse-continuity signal. Function words and
 * pronouns are skipped via their learned POS class.
 *
 * Gated default-OFF by brain->cascade_reason_in_content (opt-in: zero cost
 * and zero behavior change until enabled). The prompt-hash dedups recurrent
 * re-entry; the in_progress flag guards against reasoning recursively
 * triggering another respond()→cascade→prime. The cache buffer is allocated
 * once at semantic_dim and never moved, so the cross-thread read in the RPC
 * pool is UAF-free (dim is published last; readers gate on dim>0). */
static void cascade_prime_reasoning(brain_t brain, const char* prompt) {
    if (!brain) return;
    if (!brain->cascade_reason_in_content) return;       /* opt-in, default OFF */
    if (brain->cascade_reasoning_in_progress) return;    /* re-entrancy guard */
    if (!brain->reasoning_engine || !brain->reasoning_engine_enabled) return;
    if (!prompt || !prompt[0] || !brain->grounded_lang) return;

    uint64_t h = cascade_prompt_hash(prompt);
    if (brain->cascade_reasoning_dim > 0 &&
        brain->cascade_reasoning_prompt_hash == h) {
        return;  /* cache hit — recurrent re-entry with the same prompt */
    }

    uint32_t dim = grounded_language_get_semantic_dim(brain->grounded_lang);
    if (dim == 0) return;

    brain->cascade_reasoning_in_progress = true;
    brain->cascade_reasoning_dim = 0;  /* invalidate while recomputing */

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    reasoning_engine_connect_brain(brain->reasoning_engine, brain);
    if (reasoning_engine_reason(brain->reasoning_engine, prompt, &chain) == 0 &&
        chain.num_steps > 0 && chain.conclusion[0]) {

        /* Allocate the cache buffer once at semantic_dim (fixed for the
         * brain's life → never realloc'd/moved → no read-side UAF). */
        if (!brain->cascade_reasoning_vec) {
            brain->cascade_reasoning_vec =
                (float*)nimcp_calloc(dim, sizeof(float));
            brain->cascade_reasoning_cap =
                brain->cascade_reasoning_vec ? dim : 0u;
        }

        if (brain->cascade_reasoning_vec && brain->cascade_reasoning_cap >= dim) {
            memset(brain->cascade_reasoning_vec, 0, (size_t)dim * sizeof(float));

            /* Average the conclusion's content-word lexicon vectors. */
            uint32_t contributing = 0;
            char tok[64];
            uint32_t ti = 0;
            for (const char* p = chain.conclusion; ; p++) {
                char c = *p;
                bool is_alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
                if (is_alpha && ti < sizeof(tok) - 1u) {
                    tok[ti++] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
                    continue;
                }
                if (ti > 0) {
                    tok[ti] = '\0';
                    const gl_lexicon_entry_t* e =
                        grounded_language_lookup(brain->grounded_lang, tok);
                    if (e && e->context_initialized && e->context_vector &&
                        e->learned_class != GL_CLASS_FUNCTION &&
                        e->learned_class != GL_CLASS_PRONOUN) {
                        for (uint32_t j = 0; j < dim; j++) {
                            float v = e->context_vector[j];
                            if (isfinite(v)) brain->cascade_reasoning_vec[j] += v;
                        }
                        contributing++;
                    }
                    ti = 0;
                }
                if (c == '\0') break;
            }

            if (contributing > 0) {
                float inv = 1.0f / (float)contributing;
                for (uint32_t j = 0; j < dim; j++) {
                    brain->cascade_reasoning_vec[j] *= inv;
                }
                brain->cascade_reasoning_confidence =
                    isfinite(chain.overall_confidence) ? chain.overall_confidence
                                                       : 0.0f;
                brain->cascade_reasoning_prompt_hash = h;
                brain->cascade_reasoning_dim = dim;  /* publish LAST */
            }
        }
    }
    reasoning_chain_cleanup(&chain);
    brain->cascade_reasoning_in_progress = false;
}

int communication_cascade_run(
    brain_t brain,
    const char* prompt_or_null,
    uint32_t stage_mask,
    production_cascade_state_t* out_state)
{
    if (!brain || !out_state) return -1;
    memset(out_state, 0, sizeof(*out_state));

    /* Slice D — copy persistent self-train reward gating tunables from the
     * brain into the per-call state. Zero (calloc-default on either side)
     * triggers the in-stage built-in fallback (threshold 0.5, TTL 5s). */
    out_state->cascade_self_train_reward_threshold =
        brain->cascade_self_train_reward_threshold;
    out_state->reward_ttl_us =
        brain->cascade_self_train_reward_ttl_us;

    if (stage_mask == 0) stage_mask = CASCADE_STAGE_ALL;

    /* Tier 1 Step E (2026-05-24): prime the reasoning-conclusion cache once
     * per run, only when the content stage (its sole consumer) is enabled.
     * Default-OFF via brain->cascade_reason_in_content → true no-op until
     * opted in. Invokes reasoning at most once per unique prompt (hash-
     * deduped inside). Runs before the stages so stage_content sees it. */
    if (stage_mask & CASCADE_STAGE_CONTENT) {
        cascade_prime_reasoning(brain, prompt_or_null);
    }

    /* Batch K telemetry — entry-point counters. mask-skip counters are
     * bumped per-stage below when the stage bit is missing from mask. */
    atomic_fetch_add_explicit(&brain->cascade_total_runs, 1u, memory_order_relaxed);
    if (prompt_or_null && prompt_or_null[0]) {
        atomic_fetch_add_explicit(&brain->cascade_runs_with_prompt, 1u,
                                  memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&brain->cascade_runs_spontaneous, 1u,
                                  memory_order_relaxed);
    }
    /* Per-stage mask-skip counters: bump for every bit in CASCADE_STAGE_ALL
     * that's NOT in this run's mask. Resolution at the stage call sites
     * below would also work but this one pass is cleaner + cheaper. */
    {
        uint32_t missing = (uint32_t)CASCADE_STAGE_ALL & ~stage_mask;
        for (uint32_t bit = 1u; bit != 0; bit <<= 1) {
            if (missing & bit) cascade_counter_mask_skip(brain, bit);
        }
    }

    /* Slice 6 — thalamic gating: refresh gate weights from arousal +
     * attention state at the top of every cascade call. Stages that
     * scale by gate read brain->thalamic_gate_weights[] later via the
     * cascade_thalamic_gate_for() helper. Manual overrides (set via
     * communication_cascade_set_thalamic_gate_for_stage) are preserved.
     * No-op when thalamic_gate_enabled is false — the per-stage helper
     * returns 1.0 unconditionally, so disabled = legacy behavior. */
    if (brain->thalamic_gate_enabled) {
        (void)communication_cascade_compute_thalamic_gates(brain);
    }

    /* S5-H3 fix (2026-05-19) — clear the phonological loop on every
     * cascade entry, not just recurrent runs. Pre-fix the non-recurrent
     * cascade_run wrote the loop (cascade_stage_lexical merges words)
     * without clearing on entry; consecutive non-recurrent invocations
     * inherited stale traces from the previous utterance. Biological
     * semantics: each utterance starts with a fresh phonological buffer.
     * No-op when loop_enabled=false (and the recurrent path still calls
     * clear itself, harmless second clear). */
    if (brain->loop_enabled && brain->loop_mutex) {
        phonological_loop_clear(brain);
    }

    /* If a prompt was given, comprehend it first to seed the intent
     * vector. The comprehend itself fires GL_EVENT_COMPREHENDED on the
     * cognitive bus, so working memory + ToM see the input naturally. */
    gl_comprehension_result_t comp = {0};
    bool have_prompt = false;
    if (prompt_or_null && prompt_or_null[0] && brain->grounded_lang) {
        if (grounded_language_comprehend(brain->grounded_lang, prompt_or_null,
                                          &comp) == 0 && comp.semantic_vector) {
            have_prompt = true;
            /* grounded_language_comprehend pushes the user turn onto the
             * discourse ring internally (see nimcp_grounded_language.c
             * line 2360). Bump the corresponding counter here so callers
             * can split user vs self pushes without instrumenting GL. */
            atomic_fetch_add_explicit(&brain->cascade_discourse_ring_pushes_user,
                                      1u, memory_order_relaxed);
        }
    }

    /* Stage 0: Wernicke comprehension of the prompt — produces parse
     * tree info that Stage 2 (Goal) consumes for speech-act + topic
     * extraction. Runs only when a prompt is provided; spontaneous
     * mode skips. The stage impl lives in a separate TU because
     * Wernicke's syntactic_comprehension.h conflicts with Broca's
     * syntax_processor.h (both define phrase_type_t). */
    if ((stage_mask & CASCADE_STAGE_WERNICKE) && have_prompt) {
        cascade_counter_invoke(brain, CASCADE_STAGE_WERNICKE);
        cascade_stage_wernicke(brain, prompt_or_null, out_state);
        /* Account for skip/complete after the call since the stage
         * function avoids touching the helpers (it doesn't include
         * cascade.c's static helpers). Stage counts as "completed" if
         * Wernicke either parsed successfully or at least set the
         * speech-act flags from heuristics. */
        if (out_state->wernicke_parsed ||
            out_state->prompt_is_question ||
            out_state->prompt_is_imperative ||
            out_state->prompt_word_count > 0) {
            cascade_record_complete(out_state);
        } else {
            cascade_record_skip(out_state, CASCADE_STAGE_WERNICKE,
                                "stage_wernicke: no signal extracted");
        }
    }

    /* Stage 1-5: build content intent. Each stage records its own
     * skip/fail; only stage_content failure aborts the cascade. */
    if (stage_mask & CASCADE_STAGE_DRIVE) {
        cascade_counter_invoke(brain, CASCADE_STAGE_DRIVE);
        cascade_stage_drive(brain, out_state);
    }
    if (stage_mask & CASCADE_STAGE_GOAL) {
        cascade_counter_invoke(brain, CASCADE_STAGE_GOAL);
        cascade_stage_goal(brain, prompt_or_null, out_state);
        /* If pragmatics flipped speech_act to an indirect form, bump the
         * dedicated counter (the cascade_record_* helpers don't see this
         * semantic event). */
        if (out_state->pragmatic_is_indirect) {
            atomic_fetch_add_explicit(
                &brain->cascade_pragmatics_indirect_overrides,
                1u, memory_order_relaxed);
        }
    }
    if (stage_mask & CASCADE_STAGE_LISTENER) {
        cascade_counter_invoke(brain, CASCADE_STAGE_LISTENER);
        cascade_stage_listener(brain, out_state);
    }
    if (stage_mask & CASCADE_STAGE_EPISODIC) {
        cascade_counter_invoke(brain, CASCADE_STAGE_EPISODIC);
        cascade_stage_episodic(brain,
                                have_prompt ? comp.semantic_vector : NULL,
                                have_prompt ? grounded_language_get_semantic_dim(brain->grounded_lang) : 0,
                                out_state);
    }
    if (stage_mask & CASCADE_STAGE_CONTENT) {
        cascade_counter_invoke(brain, CASCADE_STAGE_CONTENT);
        if (cascade_stage_content(brain,
                                    have_prompt ? comp.semantic_vector : NULL,
                                    have_prompt ? grounded_language_get_semantic_dim(brain->grounded_lang) : 0,
                                    out_state) < 0) {
            atomic_fetch_add_explicit(&brain->cascade_runs_fatal_error, 1u,
                                      memory_order_relaxed);
            cascade_counter_failure(brain, CASCADE_STAGE_CONTENT);
            gl_comprehension_result_cleanup(&comp);
            return -1;
        }
    }

    gl_comprehension_result_cleanup(&comp);

    /* Stages 6-9: surface the intent as language. */
    if (stage_mask & CASCADE_STAGE_LEXICAL) {
        cascade_counter_invoke(brain, CASCADE_STAGE_LEXICAL);
        cascade_stage_lexical(brain, out_state);
    }
    if (stage_mask & CASCADE_STAGE_SYNTACTIC) {
        cascade_counter_invoke(brain, CASCADE_STAGE_SYNTACTIC);
        cascade_stage_syntactic(brain, out_state);

        /* Tier 1/2 surface correctors (F3 agreement -> F4 fluency -> T2
         * pronominalization) on the (possibly Broca-rerendered) utterance.
         * Factored into a helper so the speech-repair retry winner gets the
         * same treatment (C4 fix). Conservative + no-op when nothing applies;
         * F4/T2 stage-gated (>=2) internally; word_count preserved. */
        cascade_apply_surface_correctors(brain, out_state);
        /* SLICE 3 — Stage Syntactic PREDICTION: Broca expects the
         * bridge's word sequence to form a valid phrase structure.
         * PE = 1.0 - syntactic_validity (clamped to [0,1]) when the
         * stage actually ran; 0 when it was skipped (no utterance).
         * Skip-leaves-default of 0 is the right default — a skipped
         * stage didn't OBSERVE anything, so it cannot have been
         * surprised. */
        if (out_state->syntactic_validity >= 0.0f) {
            float v = out_state->syntactic_validity;
            if (v > 1.0f) v = 1.0f;
            out_state->pe_syntactic_norm = 1.0f - v;
        }
    }
    /* Speech repair (disfluency cleaner): strip "um", "uh", repetitions
     * and false starts from the produced utterance BEFORE Stage 8 so the
     * self-comprehension score reflects intended content. No-ops cleanly
     * when the processor is NULL or there is no utterance. If cleaning
     * changes the text, the original is preserved in utterance_pre_repair
     * and a one-shot LOG_DEBUG is emitted. */
    if ((stage_mask & CASCADE_STAGE_SELF_COMP) &&
        brain->speech_repair &&
        out_state->utterance && out_state->utterance[0]) {
        char cleaned[512];
        cleaned[0] = '\0';
        if (speech_repair_clean(brain->speech_repair, out_state->utterance,
                                  cleaned, sizeof(cleaned)) &&
            cleaned[0] && strcmp(cleaned, out_state->utterance) != 0) {
            /* Save original for diagnostics, replace with cleaned form. */
            if (out_state->utterance_pre_repair) {
                nimcp_free(out_state->utterance_pre_repair);
                out_state->utterance_pre_repair = NULL;
            }
            size_t orig_len = strlen(out_state->utterance);
            out_state->utterance_pre_repair =
                (char*)nimcp_malloc(orig_len + 1);
            if (out_state->utterance_pre_repair) {
                memcpy(out_state->utterance_pre_repair,
                        out_state->utterance, orig_len + 1);
            }
            size_t clen = strlen(cleaned);
            char* repl = (char*)nimcp_malloc(clen + 1);
            if (repl) {
                memcpy(repl, cleaned, clen + 1);
                nimcp_free(out_state->utterance);
                out_state->utterance = repl;
                out_state->speech_repair_applied = true;
                atomic_fetch_add_explicit(
                    &brain->cascade_speech_repair_applied,
                    1u, memory_order_relaxed);
                LOG_DEBUG(LOG_MODULE,
                          "speech_repair_clean: applied (orig=\"%s\" -> cleaned=\"%s\")",
                          out_state->utterance_pre_repair, out_state->utterance);
            }
        }
    }

    /* Stage 8 (Phase 2D-B): Wernicke validates the brain's own output.
     * Closes the sensorimotor loop — the produced text gets re-comprehended
     * and compared to the original intent vector. The match score is the
     * cleanest available signal of "did the brain actually say what it
     * meant?" Future training work uses this as a reward signal. */
    if (stage_mask & CASCADE_STAGE_SELF_COMP) {
        cascade_counter_invoke(brain, CASCADE_STAGE_SELF_COMP);
        cascade_stage_self_comprehension(brain, out_state);
        if (out_state->self_parsed) {
            cascade_record_complete(out_state);
        } else {
            cascade_record_skip(out_state, CASCADE_STAGE_SELF_COMP,
                                "stage_self_comp: comprehend failed or no utterance");
        }
        /* SLICE 3 — Stage Self-Comp PREDICTION: "my own utterance
         * comprehends back to the original intent." PE = 1.0 -
         * self_match when self_parsed; full surprise (1.0) when even
         * parsing the brain's own output failed. */
        if (out_state->self_parsed && isfinite(out_state->self_match)) {
            float m = out_state->self_match;
            if (m < 0.0f) m = 0.0f;
            if (m > 1.0f) m = 1.0f;
            out_state->pe_self_comp_norm = 1.0f - m;
        } else {
            out_state->pe_self_comp_norm = 1.0f;
        }
    }

    /* Stage 10 (Item 5): Speech repair — perturbation-retry on low
     * self_match. Bounded loop: up to REPAIR_MAX_ATTEMPTS rounds, each
     * adds ~5% Gaussian noise to content_intent and re-runs
     * lexical→syntactic→self_comp. Best-scoring candidate wins. The
     * original utterance + its self_match are always preserved in
     * "best_*" until the swap at the end. */
    if ((stage_mask & CASCADE_STAGE_SPEECH_REPAIR) &&
        (stage_mask & CASCADE_STAGE_SELF_COMP) &&
        out_state->self_parsed &&
        out_state->content_intent &&
        out_state->utterance &&
        out_state->self_match < REPAIR_THRESHOLD) {

        cascade_counter_invoke(brain, CASCADE_STAGE_SPEECH_REPAIR);
        /* Snapshot the original — content_intent gets perturbed in place,
         * so we save a clean copy to restore between attempts. */
        const uint32_t dim = out_state->content_dim;
        float* orig_intent = (float*)nimcp_calloc(dim, sizeof(float));
        if (orig_intent) {
            memcpy(orig_intent, out_state->content_intent,
                    dim * sizeof(float));

            /* Magnitude estimate for noise scaling: ||intent||/sqrt(dim). */
            float ssum = 0.0f;
            for (uint32_t i = 0; i < dim; i++) {
                float v = orig_intent[i];
                ssum += v * v;
            }
            float magnitude = sqrtf(ssum / (float)(dim ? dim : 1));
            if (magnitude < 1e-6f) magnitude = 1e-6f;
            const float sigma = REPAIR_NOISE_FRAC * magnitude;

            /* Initialize "best" tracking with the original. */
            out_state->best_self_match = out_state->self_match;
            out_state->best_utterance  = cascade_repair_strdup(out_state->utterance);

            /* Audit fix — snapshot pre-retry diagnostic counts so the
             * retry loop's internal calls to cascade_stage_syntactic /
             * cascade_stage_self_comprehension (each of which bumps
             * stages_completed) don't inflate the count. After the loop
             * we restore the snapshot — repair_attempts is the dedicated
             * counter for retry rounds, so stages_completed retains its
             * "unique stages executed" contract. */
            const uint32_t pre_retry_stages_completed = out_state->stages_completed;
            const uint32_t pre_retry_stages_failed    = out_state->stages_failed;

            while (out_state->repair_attempts < REPAIR_MAX_ATTEMPTS &&
                    out_state->self_match < REPAIR_THRESHOLD) {
                out_state->repair_attempts++;

                /* Restore + perturb content_intent. */
                for (uint32_t i = 0; i < dim; i++) {
                    out_state->content_intent[i] =
                        orig_intent[i] + sigma * cascade_repair_gauss();
                }

                /* Free the prior utterance — stage_lexical assumes NULL
                 * input and overwrites without freeing. (stage_syntactic
                 * does its own free-then-replace internally.) */
                if (out_state->utterance) {
                    nimcp_free(out_state->utterance);
                    out_state->utterance = NULL;
                }
                out_state->word_count = 0;
                out_state->fluency = 0.0f;
                out_state->syntactic_validity = -1.0f;

                /* Re-run lexical → syntactic → self_comp. stages_completed
                 * and stages_failed are restored after the loop (see
                 * pre_retry_* snapshot above). repair_attempts is the
                 * dedicated retry counter. */
                if (cascade_stage_lexical(brain, out_state) < 0 ||
                    !out_state->utterance || !out_state->utterance[0]) {
                    /* Lexical failed mid-retry — bump the lifetime
                     * failure counter so dashboards can see the retry
                     * loop attrited, then bail out keeping best-so-far. */
                    cascade_counter_failure(brain, CASCADE_STAGE_LEXICAL);
                    break;
                }
                cascade_stage_syntactic(brain, out_state);
                cascade_stage_self_comprehension(brain, out_state);

                /* Track best. */
                if (out_state->self_parsed &&
                    out_state->self_match > out_state->best_self_match &&
                    out_state->utterance) {
                    out_state->best_self_match = out_state->self_match;
                    if (out_state->best_utterance) {
                        nimcp_free(out_state->best_utterance);
                    }
                    out_state->best_utterance =
                        cascade_repair_strdup(out_state->utterance);
                }
            }

            /* Restore intent to clean state for downstream consumers. */
            memcpy(out_state->content_intent, orig_intent,
                    dim * sizeof(float));
            nimcp_free(orig_intent);

            /* Audit fix — restore pre-retry stage counts. The retry
             * loop's calls to cascade_stage_syntactic / _self_comp each
             * bumped stages_completed; that double-counts vs the
             * orchestrator's contract. repair_attempts already records
             * the retry count separately. */
            out_state->stages_completed = pre_retry_stages_completed;
            out_state->stages_failed    = pre_retry_stages_failed;

            /* If a retry beat the current utterance, swap in best. */
            if (out_state->best_utterance &&
                out_state->best_self_match > out_state->self_match) {
                if (out_state->utterance) nimcp_free(out_state->utterance);
                out_state->utterance = cascade_repair_strdup(out_state->best_utterance);
                out_state->self_match = out_state->best_self_match;
                /* word_count: re-tokenize cheaply — count whitespace runs. */
                uint32_t wc = 0;
                bool in_word = false;
                for (const char* p = out_state->utterance; p && *p; p++) {
                    if (*p == ' ' || *p == '\t' || *p == '\n') {
                        in_word = false;
                    } else if (!in_word) {
                        in_word = true;
                        wc++;
                    }
                }
                out_state->word_count = wc;
                /* C4 fix: the repair winner came straight out of
                 * cascade_stage_syntactic (no inline correctors), so run the
                 * F3/F4/T2 surface correctors on it now — otherwise a repaired
                 * utterance ships with un-corrected agreement/fluency/pronouns. */
                cascade_apply_surface_correctors(brain, out_state);
            }

            cascade_record_complete(out_state);
        } else {
            cascade_record_skip(out_state, CASCADE_STAGE_SPEECH_REPAIR,
                                "stage_speech_repair: alloc failed");
        }
    }

    if (stage_mask & CASCADE_STAGE_PHONOLOGICAL) {
        cascade_counter_invoke(brain, CASCADE_STAGE_PHONOLOGICAL);
        cascade_stage_phonological(brain, out_state);
    }
    /* Stage 11 (Wave 2 Item 9): prosodic contour. Runs after phonological
     * so it can reuse the syllable_count Broca's phonological processor
     * produced, and before motor so the motor stage (when implemented)
     * has F0/duration/intensity arrays to drive synthesis. */
    if (stage_mask & CASCADE_STAGE_PROSODY) {
        cascade_counter_invoke(brain, CASCADE_STAGE_PROSODY);
        cascade_stage_prosody(brain, out_state);
    }
    if (stage_mask & CASCADE_STAGE_MOTOR) {
        cascade_counter_invoke(brain, CASCADE_STAGE_MOTOR);
        cascade_stage_motor(brain, out_state);
    }
    /* Stage 11 (Wave 2 Item #10): reward-modulated SNN bridge training.
     * Runs after Stage 8 self-comprehension (which sets self_match) and
     * after Stage 10 speech-repair (which may have swapped in a better
     * utterance + its self_match) so the reward signal reflects the
     * FINAL utterance the brain "stands behind". Default OFF — gated on
     * brain->cascade_self_train_enabled, set via
     * communication_cascade_set_self_train_enabled. Independent of
     * Stage 9 self-feedback because the WM write-back is orthogonal to
     * the synaptic update (one writes a vector to a slot, the other
     * walks bindings). Train BEFORE feedback so a learning failure
     * doesn't corrupt the cognitive bus event. */
    if (stage_mask & CASCADE_STAGE_SELF_TRAIN) {
        cascade_counter_invoke(brain, CASCADE_STAGE_SELF_TRAIN);
        cascade_stage_self_train(brain, out_state);
        /* Split telemetry: matched vs no-bindings. train_applied flips
         * true only when echo_correct actually walked >=1 binding. */
        if (out_state->train_applied) {
            atomic_fetch_add_explicit(
                &brain->cascade_self_train_steps_matched,
                1u, memory_order_relaxed);
        } else {
            atomic_fetch_add_explicit(
                &brain->cascade_self_train_steps_no_bindings,
                1u, memory_order_relaxed);
        }
    }
    /* Stage 9 (item #8): write produced utterance back to working memory
     * and fire GL_EVENT_SELF_PRODUCED. Runs last so it sees a fully
     * populated cascade state (content_intent, utterance, confidence). */
    if (stage_mask & CASCADE_STAGE_SELF_FEEDBACK) {
        cascade_counter_invoke(brain, CASCADE_STAGE_SELF_FEEDBACK);
        cascade_stage_self_feedback(brain, out_state);
    }

    /* SLICE 3 — finalize FEP scalars. fep_precision defaults to 1.0 for
     * a single-pass cascade; the recurrent loop bumps it on high-PE
     * iterations so stage_self_train applies more plasticity to
     * surprising inputs (Friston FEP — precision-weighted PE drives
     * learning). pe_total is the precision-weighted "surprise budget"
     * the trainer reads. */
    if (!isfinite(out_state->fep_precision) || out_state->fep_precision <= 0.0f) {
        out_state->fep_precision = 1.0f;
    }
    cascade_fep_recompute_total(out_state);

    /* Tier 1 Step F1: surface polish (capitalize + terminal punctuation) on
     * the final returned utterance. Runs after every word-sequence stage and
     * after self-comprehension/self-feedback so only the consumer-facing
     * text is affected, not the internal token stream. */
    cascade_apply_surface_polish(out_state);

    return 0;
}

/*============================================================================
 * Recurrent-language-architecture Slice 1 — iterative cascade with
 * convergence check. See docs/claude/recurrent-language-architecture.md
 * for the full plan. This entry point repeatedly invokes the existing
 * sequential cascade and checks whether the utterance + self_match have
 * stabilized between iterations.
 *
 * Biological motivation: real cortex doesn't run language production
 * once and emit a result — it settles toward an attractor through
 * recurrent dynamics over ~200-400ms. Each iteration of the cascade
 * here is one "settling step". When cascade_self_train is enabled,
 * each iteration's STDP shifts the bridge slightly, and the next
 * iteration reads the shifted bridge. The system settles toward
 * a coherent utterance.
 *==========================================================================*/

int communication_cascade_run_recurrent(brain_t brain,
                                          const char* prompt_or_null,
                                          uint32_t max_iters,
                                          float self_match_eps,
                                          production_cascade_state_t* out_state)
{
    if (!brain || !out_state) return -1;
    /* S1-H6 fix (2026-05-19): zero out_state on entry so a max_iters=0
     * coercion path or an early-exit before the inner cascade memsets
     * leaves the struct with deterministic zero contents rather than
     * stack garbage from the caller. */
    memset(out_state, 0, sizeof(*out_state));
    if (max_iters == 0) max_iters = 8;
    if (max_iters > 64) max_iters = 64;   /* defense against runaway */
    if (!isfinite(self_match_eps) || self_match_eps < 0.0f) {
        self_match_eps = 0.01f;
    }

    char* prev_utterance = NULL;
    float prev_self_match = -1.0f;
    int last_rc = 0;
    uint32_t iter = 0;
    uint32_t settling_steps = 0;
    bool converged = false;
    /* S1-H3 fix (2026-05-19): degraded-mode flags so when an OOM bites the
     * convergence-check buffer or arcuate-feedback buffer we DON'T silently
     * spin to max_iters. prev_utterance_disabled drops the utterance-
     * stability check (falls back to self_match-only); feedback_disabled
     * skips the feedback-vec alloc and recompute for the rest of the run. */
    bool prev_utterance_disabled = false;
    bool feedback_disabled       = false;

    /* SLICE 3 — per-iteration FEP scalars. Trace is sized to the same
     * 64-iter cap as the runtime max above. We hand the trace + summary
     * to the brain at exit so a monitoring caller can snapshot it. */
    float pe_trace[64] = {0};
    uint32_t pe_trace_len = 0;
    /* S2-H2 fix (2026-05-19): parallel intent-norm trace for runaway
     * detection. Each iter we record ||content_intent|| / sqrt(dim). */
    float intent_norm_trace[64] = {0};

    /* Slice 2 — arcuate fasciculus feedback buffers. Lazy-allocated on
     * the first non-trivial feedback (iter >= 1, prev utterance non-
     * empty). Sized to the GL semantic_dim if grounded_lang is attached,
     * otherwise the feedback stays disabled. Cleared on exit. */
    uint32_t gl_dim = 0;
    if (brain->grounded_lang) {
        gl_dim = grounded_language_get_semantic_dim(brain->grounded_lang);
    }
    /* Save the brain's pre-existing arcuate state so we can restore it
     * if this recurrent_run is nested under a higher-level caller that
     * had its own feedback active. Defense in depth — current callers
     * don't nest.
     *
     * S2-C2 fix: take the arcuate-feedback lock around the save+reset
     * so a concurrent cascade_stage_content reader sees a consistent
     * tuple snapshot. */
    nimcp_mutex_t* arc_lock_recur = cascade_arcuate_lock_ensure(brain);
    if (arc_lock_recur) nimcp_mutex_lock(arc_lock_recur);
    float*   saved_arcuate_vec      = brain->arcuate_feedback_vec;
    uint32_t saved_arcuate_dim      = brain->arcuate_feedback_dim;
    float    saved_arcuate_strength = brain->arcuate_feedback_strength;
    /* S2-H2 fix (2026-05-19): also save/reset the convex-blend coef and
     * target snapshot pointer. Default reset to "no feedback" (blend=0
     * disables apply in cascade_stage_content even if vec is set). */
    float    saved_arcuate_blend    = brain->arcuate_feedback_blend;
    float*   saved_arcuate_target   = brain->arcuate_target_vec;
    uint32_t saved_arcuate_target_d = brain->arcuate_target_dim;
    /* Reset to "no feedback" for iteration 0. */
    brain->arcuate_feedback_vec      = NULL;
    brain->arcuate_feedback_dim      = 0;
    brain->arcuate_feedback_strength = 0.0f;
    brain->arcuate_feedback_blend    = 0.0f;
    brain->arcuate_target_vec        = NULL;
    brain->arcuate_target_dim        = 0;
    if (arc_lock_recur) nimcp_mutex_unlock(arc_lock_recur);

    float* feedback_vec = NULL;   /* owned by this function; size gl_dim */
    /* S2-H2 fix (2026-05-19): target snapshot — iter-0 content_intent
     * captured here, kept FIXED across all subsequent iterations so the
     * arcuate error vec computes against an external stable target instead
     * of against the current (moving) intent. Owned by this function. */
    float*   target_vec     = NULL;
    uint32_t target_vec_dim = 0;

    /* SLICE 5 — phonological loop entry handshake.
     *
     * The loop is a between-iteration state across a SINGLE recurrent run,
     * not a between-call persistent buffer. Clear it on entry so this
     * call starts with a clean slate; subsequent merges accumulate within
     * the run, and the final surface form represents what the brain
     * "said" when settling completed. Default OFF — when loop_enabled is
     * false, clear becomes a (mostly) no-op and the iteration loop
     * behaves byte-identically to Slice 1+2. */
    if (brain->loop_enabled && brain->loop_mutex) {
        phonological_loop_clear(brain);
    }

    /* SLICE 7 — save brain's pre-existing cerebellar correction_pending
     * so a nested recurrent run doesn't perturb the caller's state. The
     * pending flag is a per-run signal, distinct from the persistent
     * enabled / strength fields which are user-set and stay untouched. */
    bool saved_cereb_correction_pending = brain->cerebellar_correction_pending;
    brain->cerebellar_correction_pending = false;

    for (iter = 0; iter < max_iters; iter++) {
        /* Free heap from previous iteration before communication_cascade_run
         * memset-clears the state. Otherwise prev iteration's utterance /
         * content_intent / etc leak. Skip on first iteration — caller-
         * provided state hasn't been populated yet. */
        if (iter > 0) {
            cascade_state_cleanup(out_state);
        }

        /* SLICE 3 — let each cascade run record the current iteration
         * number on the state so consumers reading the diag dict can
         * tell which settling step their snapshot is from. */
        out_state->fep_iteration = iter;
        /* fep_precision rides between iterations: a surprising iter (high
         * pe_total at the prior iter) raises precision so this iter's
         * stage_self_train applies MORE plasticity. First iter gets the
         * default 1.0 (no prior surprise). */
        if (iter == 0) {
            out_state->fep_precision = 1.0f;
        }
        /* Otherwise pe_precision is whatever we set at the bottom of the
         * previous iteration's tail, below. */

        /* SLICE 5 — decay phonological traces at the start of each
         * iteration (after iter 0). Models passive decay of the
         * phonological store between articulatory rehearsals. Traces
         * dropping below 0.05 are evicted. On iter 0 the loop is empty
         * (we cleared above), so this is a no-op. */
        if (iter > 0 && brain->loop_enabled && brain->loop_mutex) {
            phonological_loop_decay(brain);
        }

        last_rc = communication_cascade_run(brain, prompt_or_null,
                                              CASCADE_STAGE_ALL, out_state);
        if (last_rc < 0) {
            /* Fatal error in this iteration — bail out preserving whatever
             * partial state we have. */
            break;
        }

        /* S2-H2 fix (2026-05-19): snapshot iter-0's content_intent as the
         * EXTERNAL stable target for all subsequent iterations. The
         * arcuate error_vec is then (target - own_output) instead of
         * (current_intent - own_output), which kills the (1+k)
         * amplification path. Target stays fixed across iters. */
        if (iter == 0 && !feedback_disabled && gl_dim > 0 &&
            out_state->content_intent && out_state->content_dim == gl_dim &&
            !target_vec) {
            target_vec = (float*)nimcp_calloc(gl_dim, sizeof(float));
            if (target_vec) {
                target_vec_dim = gl_dim;
                for (uint32_t i = 0; i < gl_dim; i++) {
                    float v = out_state->content_intent[i];
                    target_vec[i] = isfinite(v) ? v : 0.0f;
                }
            } else {
                /* OOM — disable feedback for this run. Single warning;
                 * runaway-prevention metric bumped. */
                atomic_fetch_add_explicit(&brain->cascade_recurrent_oom_count,
                                           1u, memory_order_relaxed);
                feedback_disabled = true;
            }
        }

        /* SLICE 3 — capture this iter's pe_total in the trace. The
         * cascade tail (cascade_fep_recompute_total) populated it before
         * returning. */
        if (pe_trace_len < (uint32_t)(sizeof(pe_trace) / sizeof(pe_trace[0])) &&
            isfinite(out_state->pe_total)) {
            pe_trace[pe_trace_len++] = out_state->pe_total;
        }

        /* S2-H2 fix (2026-05-19): record ||content_intent|| / sqrt(dim)
         * this iter so monitors can detect runaway magnitude growth even
         * if the convex-blend fix isn't enough. Same indexing as pe_trace. */
        {
            uint32_t slot = (pe_trace_len == 0) ? 0 : (pe_trace_len - 1);
            uint32_t cap  = (uint32_t)(sizeof(intent_norm_trace) /
                                       sizeof(intent_norm_trace[0]));
            if (slot < cap && out_state->content_intent &&
                out_state->content_dim > 0) {
                float ssum = 0.0f;
                uint32_t d = out_state->content_dim;
                for (uint32_t i = 0; i < d; i++) {
                    float v = out_state->content_intent[i];
                    if (isfinite(v)) ssum += v * v;
                }
                float n = sqrtf(ssum) / sqrtf((float)d);
                intent_norm_trace[slot] = isfinite(n) ? n : 0.0f;
            }
        }

        /* Convergence check — needs at least 2 iterations to compare.
         * S1-H3 fix: when prev_utterance_disabled (OOM earlier), drop
         * the utterance-stability portion and rely on self_match alone. */
        if (iter > 0) {
            bool utterance_stable;
            if (prev_utterance_disabled) {
                /* Degraded mode: don't gate convergence on utterance
                 * equality — we couldn't snapshot it. Settling can still
                 * fire on self_match stability alone. */
                utterance_stable = true;
            } else if (prev_utterance && out_state->utterance) {
                utterance_stable = (strcmp(prev_utterance, out_state->utterance) == 0);
            } else if (!prev_utterance && !out_state->utterance) {
                utterance_stable = true;
            } else {
                utterance_stable = false;
            }
            bool self_match_stable = (fabsf(out_state->self_match - prev_self_match)
                                       <= self_match_eps);
            settling_steps++;
            if (utterance_stable && self_match_stable) {
                /* S1-C1 fix: write to the new settling_steps field
                 * instead of stomping on repair_attempts (which is owned
                 * by the speech-repair retry loop and reads bogus to
                 * consumers when overwritten here). */
                out_state->settling_steps =
                    (settling_steps > 0) ? (settling_steps - 1) : 0;
                converged = true;
                break;
            }
        }

        /* SLICE 3 — precision update for the NEXT iteration.
         *
         * S3-H4 fix (2026-05-19): pre-fix the precision update was gated
         * on `pe_total > 0.0f`. When iter N+1 settled cleanly (pe_total=0),
         * the branch was skipped and precision stayed at iter N's bumped
         * value forever — opposite of FEP, which wants precision to
         * decay back toward 1.0 as evidence settles. Post-fix: drop the
         * guard. At pe_total=0 the formula evaluates to 1.0 naturally,
         * giving graceful decay. Defense-in-depth NaN guard before the
         * clamp (S3-H5). */
        if (isfinite(out_state->pe_total)) {
            float p = 1.0f + 0.5f * out_state->pe_total;
            if (!isfinite(p)) p = 0.5f;
            if (p < 0.5f) p = 0.5f;
            if (p > 4.0f) p = 4.0f;
            out_state->fep_precision = p;
        }

        /* SLICE 2 — compute arcuate feedback for the NEXT iteration.
         *
         * S2-H2 fix (2026-05-19): error_vec is computed against the
         * FIXED iter-0 target snapshot (target_vec), not against the
         * current iteration's content_intent. This kills the
         * (1+k) self-amplification path. The apply step in
         * cascade_stage_content now does a convex blend gated on
         * arcuate_feedback_blend (set below to 0.5).
         *
         * Weighted by (1 - self_match) so good iterations apply little
         * correction (system is settling) and bad iterations apply more
         * (system needs to retry differently). Cap at 0.8 to prevent
         * runaway. */
        if (!feedback_disabled && gl_dim > 0 && target_vec &&
            target_vec_dim == gl_dim &&
            out_state->utterance && out_state->utterance[0] &&
            out_state->content_intent &&
            out_state->content_dim == gl_dim) {
            gl_comprehension_result_t self_comp = {0};
            int rc = grounded_language_comprehend(brain->grounded_lang,
                                                    out_state->utterance,
                                                    &self_comp);
            if (rc == 0 && self_comp.semantic_vector) {
                if (!feedback_vec) {
                    feedback_vec = (float*)nimcp_calloc(gl_dim, sizeof(float));
                    if (!feedback_vec) {
                        /* S1-H4 fix: feedback alloc OOM — disable
                         * feedback for the rest of the run instead of
                         * retrying every iter against pressured heap. */
                        atomic_fetch_add_explicit(
                            &brain->cascade_recurrent_oom_count,
                            1u, memory_order_relaxed);
                        feedback_disabled = true;
                    }
                }
                if (feedback_vec) {
                    /* error_vec = target - own_output (in semantic space).
                     * Target = iter-0 content_intent (fixed). Positive
                     * components = "what we wanted to say but didn't";
                     * negative = "what we said but didn't mean".
                     * cascade_stage_content does a convex blend so apply
                     * is bounded. */
                    for (uint32_t i = 0; i < gl_dim; i++) {
                        float t = target_vec[i];
                        float o = self_comp.semantic_vector[i];
                        feedback_vec[i] = (isfinite(t) ? t : 0.0f)
                                        - (isfinite(o) ? o : 0.0f);
                    }
                    float strength = 1.0f - out_state->self_match;
                    if (!isfinite(strength) || strength < 0.0f) strength = 0.5f;
                    if (strength > 0.8f) strength = 0.8f;

                    /* S2-C2 fix: publish the (vec, dim, strength, blend, target)
                     * tuple under the arcuate lock so a concurrent
                     * cascade_stage_content reader either sees the OLD
                     * tuple (and reads its vec safely) or the NEW tuple
                     * (and reads the new vec safely) — never a torn mix.
                     *
                     * S2-H2: also set blend=0.5 (convex-blend mode) so
                     * the apply step bounds correction; pre-fix the
                     * additive form amplified intent magnitude. */
                    if (arc_lock_recur) nimcp_mutex_lock(arc_lock_recur);
                    brain->arcuate_feedback_vec      = feedback_vec;
                    brain->arcuate_feedback_dim      = gl_dim;
                    brain->arcuate_feedback_strength = strength;
                    brain->arcuate_feedback_blend    = 0.5f;
                    brain->arcuate_target_vec        = target_vec;
                    brain->arcuate_target_dim        = target_vec_dim;
                    if (arc_lock_recur) nimcp_mutex_unlock(arc_lock_recur);
                }
            }
            gl_comprehension_result_cleanup(&self_comp);
        }

        /* SLICE 7 — set cerebellar correction_pending for the NEXT
         * iteration based on accumulated PE this iter. Sum motor +
         * prosody PE-norms; if > threshold, the next iter's motor +
         * prosody stages will fold the cerebellum's prediction into
         * their trajectories at correction_strength. Clears
         * automatically below — pending is single-iteration; if PE
         * stays high the next iter sets it again. */
        if (brain->cerebellar_correction_enabled && brain->cerebellum) {
            float accum_pe = 0.0f;
            if (isfinite(out_state->cereb_motor_pe_norm)) {
                accum_pe += out_state->cereb_motor_pe_norm;
            }
            if (isfinite(out_state->cereb_prosody_pe_norm)) {
                accum_pe += out_state->cereb_prosody_pe_norm;
            }
            float thresh = brain->cerebellar_pe_threshold;
            if (!isfinite(thresh) || thresh <= 0.0f) thresh = 0.20f;
            brain->cerebellar_correction_pending = (accum_pe > thresh);

            /* S7-H3 fix (2026-05-19): bump the iter counter exactly once if
             * EITHER stage applied a correction this iter. Disambiguates
             * iter-with-correction from stage-with-correction (the latter
             * stays in cerebellar_corrections_applied — see brain_internal.h
             * for the rename). */
            if (out_state->cereb_correction_applied) {
                brain->cerebellar_correction_iters_applied++;
            }
        }

        /* Save this iteration's outputs to compare against the next.
         *
         * S1-H3 fix (2026-05-19): on alloc failure, flip
         * prev_utterance_disabled so the convergence check falls back to
         * self_match-only stability rather than silently failing every
         * iter. One-time warn via cascade_recurrent_oom_count counter. */
        if (prev_utterance) {
            nimcp_free(prev_utterance);
            prev_utterance = NULL;
        }
        if (!prev_utterance_disabled && out_state->utterance) {
            size_t len = strlen(out_state->utterance);
            prev_utterance = (char*)nimcp_malloc(len + 1);
            if (prev_utterance) {
                memcpy(prev_utterance, out_state->utterance, len + 1);
            } else {
                atomic_fetch_add_explicit(&brain->cascade_recurrent_oom_count,
                                           1u, memory_order_relaxed);
                prev_utterance_disabled = true;
            }
        }
        prev_self_match = out_state->self_match;
    }

    /* Record settling_steps even if we hit max_iters without converging —
     * tells callers "system did not settle, here's how many tries it got".
     *
     * S1-C1 fix: this path used to write into repair_attempts, which is
     * owned by the speech-repair retry loop. Consumers reading repair_attempts
     * saw a meaningless mix of two unrelated subsystem counters. Now we
     * write into the new dedicated settling_steps field, and leave
     * repair_attempts alone. */
    if (settling_steps > 0 && out_state->settling_steps == 0) {
        out_state->settling_steps = settling_steps;
    }

    /* Restore caller's arcuate-feedback state, then free our buffer. The
     * pointer assignment order matters: clear the pointer BEFORE freeing,
     * so cascade_stage_content can't read a freed buffer if another
     * thread sneaks in.
     *
     * S2-C2 fix: hold the arcuate lock across the restore so the
     * (vec, dim, strength) tuple stays consistent for any concurrent
     * reader. The lock is dropped BEFORE the free so we don't hold it
     * across a malloc-system call (and any concurrent reader that
     * snapped the OLD pointer while we held the lock already finished
     * its consume loop — produce_cascade is a one-shot copy under the
     * lock, not a long-lived view). */
    if (arc_lock_recur) nimcp_mutex_lock(arc_lock_recur);
    brain->arcuate_feedback_vec      = saved_arcuate_vec;
    brain->arcuate_feedback_dim      = saved_arcuate_dim;
    brain->arcuate_feedback_strength = saved_arcuate_strength;
    /* S2-H2 fix: also restore the blend + target snapshot fields. */
    brain->arcuate_feedback_blend    = saved_arcuate_blend;
    brain->arcuate_target_vec        = saved_arcuate_target;
    brain->arcuate_target_dim        = saved_arcuate_target_d;
    if (arc_lock_recur) nimcp_mutex_unlock(arc_lock_recur);
    if (feedback_vec) nimcp_free(feedback_vec);
    if (target_vec)   nimcp_free(target_vec);

    /* SLICE 7 — restore caller's correction_pending flag (always false
     * in current callers, but defense in depth for nested runs). */
    brain->cerebellar_correction_pending = saved_cereb_correction_pending;

    if (prev_utterance) nimcp_free(prev_utterance);

    /* SLICE 3 — publish FEP metrics on the brain. Caller-visible via
     * nimcp_brain_get_cascade_fep_metrics_impl. Lock-free single-writer;
     * concurrent readers see an at-worst stale snapshot. The struct is
     * stored inline (not heap-allocated) so it survives across calls and
     * the next recurrent invocation overwrites in place. */
    brain->fep_iterations_run = pe_trace_len;
    memset(brain->fep_pe_trace, 0, sizeof(brain->fep_pe_trace));
    if (pe_trace_len > 0) {
        uint32_t copy_n = pe_trace_len;
        uint32_t cap = (uint32_t)(sizeof(brain->fep_pe_trace) /
                                  sizeof(brain->fep_pe_trace[0]));
        if (copy_n > cap) copy_n = cap;
        for (uint32_t i = 0; i < copy_n; i++) {
            brain->fep_pe_trace[i] = pe_trace[i];
        }
        float pe_min = pe_trace[0], pe_max = pe_trace[0];
        float pe_sum = 0.0f;
        for (uint32_t i = 0; i < pe_trace_len; i++) {
            float v = pe_trace[i];
            if (!isfinite(v)) continue;
            if (v < pe_min) pe_min = v;
            if (v > pe_max) pe_max = v;
            pe_sum += v;
        }
        brain->fep_pe_initial  = pe_trace[0];
        brain->fep_pe_terminal = pe_trace[pe_trace_len - 1];
        brain->fep_pe_min      = pe_min;
        brain->fep_pe_max      = pe_max;
        brain->fep_pe_mean     = pe_sum / (float)pe_trace_len;
        if (pe_trace_len >= 2) {
            float denom = pe_trace[0];
            if (denom < 1e-6f) denom = 1e-6f;
            brain->fep_pe_decay_rate =
                (pe_trace[0] - pe_trace[pe_trace_len - 1]) / denom;
        } else {
            brain->fep_pe_decay_rate = 0.0f;
        }
    } else {
        brain->fep_pe_initial    = 0.0f;
        brain->fep_pe_terminal   = 0.0f;
        brain->fep_pe_min        = 0.0f;
        brain->fep_pe_max        = 0.0f;
        brain->fep_pe_mean       = 0.0f;
        brain->fep_pe_decay_rate = 0.0f;
    }
    brain->fep_converged = converged ? 1 : 0;

    /* S2-H2 fix (2026-05-19): publish ||content_intent|| per-iter trace
     * + summary so monitors can flag runaway amplification independent
     * of the convex-blend bounding. */
    memset(brain->fep_intent_norm_trace, 0, sizeof(brain->fep_intent_norm_trace));
    if (pe_trace_len > 0) {
        uint32_t cap = (uint32_t)(sizeof(brain->fep_intent_norm_trace) /
                                   sizeof(brain->fep_intent_norm_trace[0]));
        uint32_t copy_n = (pe_trace_len < cap) ? pe_trace_len : cap;
        float n_max = 0.0f;
        for (uint32_t i = 0; i < copy_n; i++) {
            float v = intent_norm_trace[i];
            brain->fep_intent_norm_trace[i] = v;
            if (isfinite(v) && v > n_max) n_max = v;
        }
        brain->fep_intent_norm_initial  = intent_norm_trace[0];
        brain->fep_intent_norm_terminal = intent_norm_trace[copy_n - 1];
        brain->fep_intent_norm_max      = n_max;
    } else {
        brain->fep_intent_norm_initial  = 0.0f;
        brain->fep_intent_norm_terminal = 0.0f;
        brain->fep_intent_norm_max      = 0.0f;
    }

    return last_rc;
}

/*============================================================================
 * Batch K — public RO API: snapshot + reset cascade counters.
 *==========================================================================*/

int nimcp_brain_get_cascade_counters_impl(brain_t brain,
                                            nimcp_cascade_counters_t* out)
{
    if (!brain || !out) return -1;
    memset(out, 0, sizeof(*out));

    out->total_runs = atomic_load_explicit(&brain->cascade_total_runs,
                                            memory_order_relaxed);
    out->runs_with_prompt = atomic_load_explicit(&brain->cascade_runs_with_prompt,
                                                  memory_order_relaxed);
    out->runs_spontaneous = atomic_load_explicit(&brain->cascade_runs_spontaneous,
                                                  memory_order_relaxed);
    out->runs_fatal_error = atomic_load_explicit(&brain->cascade_runs_fatal_error,
                                                  memory_order_relaxed);
    for (uint32_t i = 0; i < NIMCP_CASCADE_STAGE_COUNT; i++) {
        out->stage_invocations[i] = atomic_load_explicit(
            &brain->cascade_stage_invocations[i], memory_order_relaxed);
        out->stage_mask_skips[i] = atomic_load_explicit(
            &brain->cascade_stage_mask_skips[i], memory_order_relaxed);
        out->stage_failures[i] = atomic_load_explicit(
            &brain->cascade_stage_failures[i], memory_order_relaxed);
    }
    out->pragmatics_indirect_overrides = atomic_load_explicit(
        &brain->cascade_pragmatics_indirect_overrides, memory_order_relaxed);
    out->wernicke_lexicon_miss = atomic_load_explicit(
        &brain->cascade_wernicke_lexicon_miss, memory_order_relaxed);
    out->speech_repair_applied = atomic_load_explicit(
        &brain->cascade_speech_repair_applied, memory_order_relaxed);
    out->self_train_steps_matched = atomic_load_explicit(
        &brain->cascade_self_train_steps_matched, memory_order_relaxed);
    out->self_train_steps_no_bindings = atomic_load_explicit(
        &brain->cascade_self_train_steps_no_bindings, memory_order_relaxed);
    out->self_produced_events_fired = atomic_load_explicit(
        &brain->cascade_self_produced_events_fired, memory_order_relaxed);
    out->discourse_ring_pushes_user = atomic_load_explicit(
        &brain->cascade_discourse_ring_pushes_user, memory_order_relaxed);
    out->discourse_ring_pushes_self = atomic_load_explicit(
        &brain->cascade_discourse_ring_pushes_self, memory_order_relaxed);
    /* S1-H3+H4 fix (2026-05-19): surface recurrent-loop OOM counter. */
    out->recurrent_oom_count = atomic_load_explicit(
        &brain->cascade_recurrent_oom_count, memory_order_relaxed);
    /* S3-H3 + S3-H6 fix (2026-05-19): surface FEP lexical-skipped + self-train
     * precision-cap-hits counters. */
    out->fep_lexical_skipped = atomic_load_explicit(
        &brain->cascade_fep_lexical_skipped, memory_order_relaxed);
    out->self_train_precision_cap_hits = atomic_load_explicit(
        &brain->cascade_self_train_precision_cap_hits, memory_order_relaxed);
    return 0;
}

/*============================================================================
 * SLICE 3 — FEP recurrent metrics getter. Public RO API; returns the
 * most-recent recurrent-cascade prediction-error trajectory. Zero-init
 * when the recurrent loop has never been called.
 *==========================================================================*/

int nimcp_brain_get_cascade_fep_metrics_impl(brain_t brain,
                                               nimcp_cascade_fep_metrics_t* out)
{
    if (!brain || !out) return -1;
    memset(out, 0, sizeof(*out));

    out->iterations_run = brain->fep_iterations_run;
    /* Bound copy to BOTH brain-side array size and out-side array size
     * (they happen to be 64 each — but the loop is defense-in-depth
     * against either side drifting). */
    uint32_t n = brain->fep_iterations_run;
    uint32_t out_cap = (uint32_t)(sizeof(out->pe_total_trace) /
                                   sizeof(out->pe_total_trace[0]));
    uint32_t in_cap  = (uint32_t)(sizeof(brain->fep_pe_trace) /
                                   sizeof(brain->fep_pe_trace[0]));
    if (n > out_cap) n = out_cap;
    if (n > in_cap)  n = in_cap;
    for (uint32_t i = 0; i < n; i++) {
        out->pe_total_trace[i] = brain->fep_pe_trace[i];
    }
    out->pe_total_initial  = brain->fep_pe_initial;
    out->pe_total_terminal = brain->fep_pe_terminal;
    out->pe_total_min      = brain->fep_pe_min;
    out->pe_total_max      = brain->fep_pe_max;
    out->pe_total_mean     = brain->fep_pe_mean;
    out->pe_decay_rate     = brain->fep_pe_decay_rate;
    out->converged         = brain->fep_converged;
    return 0;
}

int nimcp_brain_reset_cascade_counters_impl(brain_t brain)
{
    if (!brain) return -1;
    atomic_store_explicit(&brain->cascade_total_runs, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_runs_with_prompt, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_runs_spontaneous, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_runs_fatal_error, 0u, memory_order_relaxed);
    for (uint32_t i = 0; i < NIMCP_CASCADE_STAGE_COUNT; i++) {
        atomic_store_explicit(&brain->cascade_stage_invocations[i], 0u, memory_order_relaxed);
        atomic_store_explicit(&brain->cascade_stage_mask_skips[i], 0u, memory_order_relaxed);
        atomic_store_explicit(&brain->cascade_stage_failures[i], 0u, memory_order_relaxed);
    }
    atomic_store_explicit(&brain->cascade_pragmatics_indirect_overrides, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_wernicke_lexicon_miss, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_speech_repair_applied, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_self_train_steps_matched, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_self_train_steps_no_bindings, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_self_produced_events_fired, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_discourse_ring_pushes_user, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_discourse_ring_pushes_self, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_recurrent_oom_count, 0u, memory_order_relaxed);
    /* S3-H3 + S3-H6 fix (2026-05-19): reset the new FEP-skipped + precision-cap
     * counters alongside the rest. */
    atomic_store_explicit(&brain->cascade_fep_lexical_skipped, 0u, memory_order_relaxed);
    atomic_store_explicit(&brain->cascade_self_train_precision_cap_hits, 0u, memory_order_relaxed);
    return 0;
}
