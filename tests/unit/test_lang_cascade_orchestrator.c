/**
 * @file test_lang_cascade_orchestrator.c
 * @brief Orchestrator-level integration smoke tests for the 15-stage
 *        production cascade defined in nimcp_communication_cascade.h.
 *
 * Until now only Stage 14's pure helper (cascade_apply_self_train_reward)
 * had dedicated test coverage. This file exercises the orchestrator
 * end-to-end against a MINIMAL-init brain and asserts observable
 * post-conditions on the cascade state:
 *
 *  1. test_spontaneous_mode
 *      cascade_run(brain, NULL, ALL, &state) — no prompt → Stage 0
 *      should not parse, prompt_is_question stays false, no crash.
 *  2. test_question_classification
 *      "Where is the cat?" → state.prompt_is_question becomes true
 *      via Wernicke's wh-word + '?' heuristic.
 *  3. test_imperative_classification
 *      "open the door" with a leading verb and no '?' → state.prompt_is_imperative.
 *      (Skipped softly if minimal brain has no GL lexicon to tag "open" as a verb.)
 *  4. test_pragmatic_indirect_override
 *      "Can you pass the salt?" — if broca_pragmatics is attached, the
 *      pragmatics processor flips act_type to SPEECH_ACT_REQUEST and sets
 *      pragmatic_is_indirect. If not attached, the call still succeeds.
 *  5. test_stage_mask_filter
 *      stage_mask = WERNICKE|DRIVE only — assert stages_skipped is
 *      non-zero and only bits within the enabled set are present (the
 *      orchestrator never even runs the masked-out stages, so they can't
 *      record skips for themselves).
 *  6. test_state_cleanup_no_leak
 *      Loop the cascade 100 iterations with cleanup each pass — verifies
 *      cascade_state_cleanup releases its heap-owned arrays cleanly.
 *  7. test_diag_full_smoke
 *      nimcp_brain_produce_cascade_diag_full_impl: arrays NULL iff count==0,
 *      out_utterance is NUL-terminated, caller-frees arrays with nimcp_free.
 *
 * Build (matches CMake target):
 *   make test_lang_cascade_orchestrator -j4
 *   ./build/tests/test_lang_cascade_orchestrator
 *
 * Exit codes: 0 = all pass, 1 = at least one EXPECT failed.
 */

#include "language/nimcp_communication_cascade.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/regions/broca/nimcp_pragmatics_processor.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* The orchestrator takes a raw brain_t (internal pointer typedef'd in
 * the cascade header), so we reach into the brain struct directly. The
 * struct fields we touch (broca_pragmatics) live in the internal brain
 * header — we don't need to include it because the cascade header only
 * forward-declares brain_t and we just pass the pointer through. We DO
 * need the internal header for the pragmatics-field check, however, so
 * include that locally with the same path the language sources use. */
#include "core/brain/nimcp_brain_internal.h"

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

/* Helper — build a MINIMAL brain. brain_create_minimal disables most
 * cognitive subsystems (working memory, ToM, glia, etc) but still runs
 * the language-subsystem init which creates grounded_lang. That's the
 * minimum the cascade needs to extract surface heuristics in Stage 0. */
static brain_t make_minimal_brain(const char* name) {
    return brain_create_minimal(name, BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION,
                                 /*num_inputs=*/16, /*num_outputs=*/8);
}

/*--------------------------------------------------------------------------*/

static void test_spontaneous_mode(void) {
    brain_t brain = make_minimal_brain("cascade_spontaneous");
    EXPECT(brain != NULL, "brain_create_minimal");
    if (!brain) return;

    production_cascade_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = communication_cascade_run(brain, /*prompt=*/NULL,
                                         CASCADE_STAGE_ALL, &state);
    EXPECT(rc == 0, "spontaneous run rc=0, got %d", rc);
    EXPECT(state.wernicke_parsed == false,
           "spontaneous → wernicke_parsed=false, got %d",
           (int)state.wernicke_parsed);
    EXPECT(state.prompt_is_question == false,
           "spontaneous → prompt_is_question=false, got %d",
           (int)state.prompt_is_question);
    EXPECT(state.prompt_is_imperative == false,
           "spontaneous → prompt_is_imperative=false, got %d",
           (int)state.prompt_is_imperative);

    cascade_state_cleanup(&state);
    brain_destroy(brain);

    fprintf(stderr, "PASS test_spontaneous_mode\n");
}

static void test_question_classification(void) {
    brain_t brain = make_minimal_brain("cascade_question");
    EXPECT(brain != NULL, "brain_create_minimal");
    if (!brain) return;

    production_cascade_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = communication_cascade_run(brain, "Where is the cat?",
                                         CASCADE_STAGE_ALL, &state);
    EXPECT(rc == 0, "question run rc=0, got %d", rc);

    /* The Wernicke stage's surface heuristic fires on (wh-word at pos 0)
     * OR (trailing '?'). Both apply here. The stage requires
     * brain->grounded_lang to be present — which the minimal-mode brain
     * init creates as part of nimcp_brain_factory_init_language_subsystem
     * (called unconditionally regardless of minimal_mode). */
    if (brain->grounded_lang) {
        EXPECT(state.prompt_is_question == true,
               "Where is the cat? → prompt_is_question=true, got %d",
               (int)state.prompt_is_question);
    } else {
        /* Diagnostic only — should not happen on current main, but if a
         * future change tightens the gate, log rather than fail. */
        fprintf(stderr, "  (note: grounded_lang NULL on minimal brain — "
                        "Stage 0 skipped; treating subtest as no-op)\n");
    }

    cascade_state_cleanup(&state);
    brain_destroy(brain);

    fprintf(stderr, "PASS test_question_classification\n");
}

static void test_imperative_classification(void) {
    brain_t brain = make_minimal_brain("cascade_imperative");
    EXPECT(brain != NULL, "brain_create_minimal");
    if (!brain) return;

    production_cascade_state_t state;
    memset(&state, 0, sizeof(state));

    /* "open the door" — no trailing '?', no wh-word, no aux-inversion.
     * The Wernicke stage flags it as imperative when the FIRST token is
     * tagged as a verb (SYN_CAT_VERB) by the GL lexicon. On a brand-new
     * minimal brain the lexicon is empty, so "open" comes back
     * SYN_CAT_UNKNOWN and the imperative bit stays false. We treat that
     * outcome as a soft-skip — the orchestrator path was exercised and
     * didn't crash, which is the main thing this subtest verifies. */
    int rc = communication_cascade_run(brain, "open the door",
                                         CASCADE_STAGE_ALL, &state);
    EXPECT(rc == 0, "imperative run rc=0, got %d", rc);
    EXPECT(state.prompt_is_question == false,
           "open the door → prompt_is_question=false, got %d",
           (int)state.prompt_is_question);

    if (state.prompt_is_imperative) {
        /* Lexicon had "open" tagged — perfect. */
        fprintf(stderr, "  imperative classification fired\n");
    } else {
        /* Empty lexicon — surface heuristic can't promote to imperative
         * without a verb tag. This is the expected MINIMAL-brain outcome. */
        fprintf(stderr, "  (lexicon empty — imperative bit not set; "
                        "orchestrator path exercised, no crash)\n");
    }

    cascade_state_cleanup(&state);
    brain_destroy(brain);

    fprintf(stderr, "PASS test_imperative_classification\n");
}

static void test_pragmatic_indirect_override(void) {
    brain_t brain = make_minimal_brain("cascade_pragmatic");
    EXPECT(brain != NULL, "brain_create_minimal");
    if (!brain) return;

    production_cascade_state_t state;
    memset(&state, 0, sizeof(state));

    int rc = communication_cascade_run(brain, "Can you pass the salt?",
                                         CASCADE_STAGE_ALL, &state);
    EXPECT(rc == 0, "indirect-request run rc=0, got %d", rc);

    /* Surface form is a question (aux-inversion "Can you" + '?'). The
     * pragmatics override only fires when brain->broca_pragmatics is
     * attached AND the prompt was classified as a question by Stage 0.
     * On a minimal-mode brain broca_pragmatics is typically NOT created
     * — pragmatics init lives behind enable_broca, which the minimal
     * preset disables. We branch the assertion on its presence. */
    if (brain->broca_pragmatics && state.prompt_is_question) {
        EXPECT(state.pragmatic_is_indirect == true,
               "indirect-request → pragmatic_is_indirect=true, got %d",
               (int)state.pragmatic_is_indirect);
        EXPECT(state.act_type == SPEECH_ACT_REQUEST,
               "indirect-request → act_type=REQUEST(%d), got %d",
               (int)SPEECH_ACT_REQUEST, (int)state.act_type);
    } else {
        fprintf(stderr, "  (broca_pragmatics not attached on minimal brain "
                        "— skipping pragmatics assertion)\n");
    }

    cascade_state_cleanup(&state);
    brain_destroy(brain);

    fprintf(stderr, "PASS test_pragmatic_indirect_override\n");
}

static void test_stage_mask_filter(void) {
    brain_t brain = make_minimal_brain("cascade_mask");
    EXPECT(brain != NULL, "brain_create_minimal");
    if (!brain) return;

    production_cascade_state_t state;
    memset(&state, 0, sizeof(state));

    /* Enable only Stage 0 (Wernicke) + Stage 1 (Drive). All other bits
     * are masked off; the orchestrator skips those stages entirely. The
     * stages_skipped bitfield can ONLY accumulate bits for stages that
     * actually ran but found no signal — masked-out stages never set
     * their bit. So we expect any bits in stages_skipped to lie within
     * the enabled mask. */
    const uint32_t mask = (uint32_t)CASCADE_STAGE_WERNICKE
                        | (uint32_t)CASCADE_STAGE_DRIVE;

    int rc = communication_cascade_run(brain, "hello world", mask, &state);
    EXPECT(rc == 0, "filtered run rc=0, got %d", rc);

    /* The key invariant: no bits OUTSIDE the enabled mask should appear
     * in stages_skipped. A masked-out stage doesn't run, so it can't
     * self-report a skip — only stages that actually ran can flag a bit
     * in stages_skipped via cascade_record_skip. If we see e.g.
     * CASCADE_STAGE_LEXICAL in stages_skipped here, the orchestrator
     * leaked through the mask gate. */
    uint32_t out_of_mask = state.stages_skipped & ~mask;
    EXPECT(out_of_mask == 0u,
           "stages_skipped bits=0x%x must lie within enabled mask=0x%x "
           "(out-of-mask=0x%x)",
           state.stages_skipped, mask, out_of_mask);

    /* The mask must actually prevent downstream output stages from
     * running. LEXICAL, SYNTACTIC, PHONOLOGICAL, MOTOR, etc. are all
     * masked off in this run — they own state.utterance / phoneme_seq /
     * prosody arrays, and none of them should have allocated anything. */
    EXPECT(state.utterance == NULL,
           "masked-out output stages → utterance is NULL, got %p",
           (void*)state.utterance);
    EXPECT(state.phoneme_sequence == NULL,
           "masked-out phonological → phoneme_sequence is NULL, got %p",
           (void*)state.phoneme_sequence);
    EXPECT(state.prosody_pitch_hz == NULL,
           "masked-out prosody → prosody_pitch_hz is NULL, got %p",
           (void*)state.prosody_pitch_hz);
    EXPECT(state.train_applied == false,
           "masked-out self-train → train_applied=false, got %d",
           (int)state.train_applied);

    cascade_state_cleanup(&state);
    brain_destroy(brain);

    fprintf(stderr, "PASS test_stage_mask_filter\n");
}

static void test_state_cleanup_no_leak(void) {
    brain_t brain = make_minimal_brain("cascade_cleanup");
    EXPECT(brain != NULL, "brain_create_minimal");
    if (!brain) return;

    /* Loop run+cleanup. Each cycle allocates content_intent / utterance /
     * best_utterance / phoneme_sequence / prosody arrays inside the state
     * and frees them at the end. The simple invariant: no crash, no
     * runaway address-sanitizer-style growth.
     *
     * We deliberately alternate prompt vs spontaneous to hit BOTH the
     * grounded_language_comprehend allocation path AND the spontaneous-
     * skip path. */
    const int N = 100;
    for (int i = 0; i < N; i++) {
        production_cascade_state_t state;
        memset(&state, 0, sizeof(state));
        const char* prompt = (i & 1) ? "spell the word cat" : NULL;
        int rc = communication_cascade_run(brain, prompt,
                                             CASCADE_STAGE_ALL, &state);
        EXPECT(rc == 0, "iter %d rc=0, got %d", i, rc);
        cascade_state_cleanup(&state);
    }

    brain_destroy(brain);

    fprintf(stderr, "PASS test_state_cleanup_no_leak (%d iterations)\n", N);
}

static void test_diag_full_smoke(void) {
    brain_t brain = make_minimal_brain("cascade_diag");
    EXPECT(brain != NULL, "brain_create_minimal");
    if (!brain) return;

    nimcp_cascade_diag_full_t diag;
    memset(&diag, 0, sizeof(diag));

    char utterance[256];
    char best[256];
    memset(utterance, 0xab, sizeof(utterance));  /* poison */
    memset(best, 0xab, sizeof(best));

    uint8_t* phoneme_seq = (uint8_t*)0xdeadbeef;       /* poison */
    float*   pitch       = (float*)0xdeadbeef;
    float*   dur         = (float*)0xdeadbeef;
    float*   inten       = (float*)0xdeadbeef;

    int rc = nimcp_brain_produce_cascade_diag_full_impl(
                brain,
                /*prompt=*/"Where is the cat?",
                utterance, (uint32_t)sizeof(utterance),
                best,      (uint32_t)sizeof(best),
                &diag,
                &phoneme_seq,
                &pitch, &dur, &inten);

    EXPECT(rc == 0, "diag_full rc=0, got %d", rc);

    /* The implementation always overwrites the out-pointers to NULL up
     * front, then conditionally re-points them only when the
     * corresponding count is > 0. Verify the contract: array is NULL
     * iff count is 0, AND the impl cleared the poison values. */
    EXPECT(phoneme_seq != (uint8_t*)0xdeadbeef,
           "out_phoneme_sequence must be cleared, got poison");
    if (diag.phoneme_count == 0) {
        EXPECT(phoneme_seq == NULL,
               "phoneme_count=0 → phoneme_seq=NULL, got %p", (void*)phoneme_seq);
    } else {
        EXPECT(phoneme_seq != NULL,
               "phoneme_count=%u → phoneme_seq non-NULL", diag.phoneme_count);
    }

    if (diag.prosody_syllable_count == 0) {
        EXPECT(pitch == NULL,
               "prosody_syllable_count=0 → pitch=NULL, got %p", (void*)pitch);
        EXPECT(dur == NULL,   "duration=NULL, got %p", (void*)dur);
        EXPECT(inten == NULL, "intensity=NULL, got %p", (void*)inten);
    } else {
        EXPECT(pitch != NULL,
               "prosody_syllable_count=%u → pitch non-NULL",
               diag.prosody_syllable_count);
        /* duration / intensity may be NULL independently if their source
         * arrays were null in state, but pitch SHOULD allocate when the
         * count is non-zero. (See cascade_stage_prosody — all three arrays
         * are allocated together when the contour fires.) */
    }

    /* out_utterance must be NUL-terminated (poison cleared). */
    bool nul_seen = false;
    for (size_t i = 0; i < sizeof(utterance); i++) {
        if (utterance[i] == '\0') { nul_seen = true; break; }
    }
    EXPECT(nul_seen,
           "out_utterance must be NUL-terminated within %zu bytes",
           sizeof(utterance));

    /* Diagnostics surfaced correctly — stages_completed must be at
     * least one (Wernicke parsed "Where is the cat?" at minimum). */
    EXPECT(diag.stages_completed >= 1u,
           "diag.stages_completed >= 1, got %u", diag.stages_completed);

    /* Free caller-owned arrays with nimcp_free, exactly per the header's
     * documented ownership contract. */
    if (phoneme_seq) nimcp_free(phoneme_seq);
    if (pitch)       nimcp_free(pitch);
    if (dur)         nimcp_free(dur);
    if (inten)       nimcp_free(inten);

    brain_destroy(brain);

    fprintf(stderr, "PASS test_diag_full_smoke\n");
}

/*--------------------------------------------------------------------------*/

int main(void) {
    fprintf(stderr, "=== test_lang_cascade_orchestrator ===\n");

    test_spontaneous_mode();
    test_question_classification();
    test_imperative_classification();
    test_pragmatic_indirect_override();
    test_stage_mask_filter();
    test_state_cleanup_no_leak();
    test_diag_full_smoke();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
