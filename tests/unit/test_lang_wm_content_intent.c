/**
 * @file test_lang_wm_content_intent.c
 * @brief UNIT — Tier 1 Step D: working-memory feature vectors blend into
 *        the cascade's content_intent.
 *
 * Tier 1 Step D (2026-05-24): cascade_stage_content now lifts the feature
 * vector of every active working-memory item and adds it to content_intent,
 * scaled by w_working_memory (0.25) × the item's salience. This makes
 * production reflect what the brain is "holding in mind", not just lexical
 * relevance to the prompt. Before Step D the pragmatic stage only COUNTED
 * WM items and discarded their vectors.
 *
 * Why this is testable in isolation: communication_cascade_run accepts a
 * stage_mask. Running with ONLY CASCADE_STAGE_CONTENT (and a NULL prompt)
 * leaves every other content_intent source at its zero state —
 *   - prompt seed: NULL prompt → nothing
 *   - drive bias:  stage_drive didn't run → drive_magnitude/arousal == 0
 *   - episodic:    episodic_count == 0
 *   - listener:    listener_known == false
 *   - goal:        goal_priority == 0
 *   - arcuate:     arcuate_feedback_vec == NULL on a fresh brain
 * so the resulting content_intent is EXACTLY the working-memory blend.
 * That makes the contribution deterministic: content_intent[k] should be
 * w_working_memory × salience × fv[k].
 *
 * What this test guards:
 *   1. A WM item one-hot at dim k (salience above the 0.2 floor) lands in
 *      content_intent[k] at the expected magnitude (0.25 × salience), and
 *      dims with no WM signal stay ~0.
 *   2. Salience gating: an item below the 0.2 salience floor contributes
 *      nothing (decayed chunks don't smear the intent).
 *   3. Empty WM → content-only cascade leaves content_intent ~0 (the blend
 *      never writes spuriously).
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_wm_content_intent.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_wm_content_intent
 */

#include "language/nimcp_communication_cascade.h"
#include "language/nimcp_grounded_language.h"
#include "cognitive/nimcp_working_memory.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

/* Must mirror w_working_memory in cascade_stage_content. */
#define W_WORKING_MEMORY 0.25f

static brain_t make_brain(const char* name) {
    return brain_create_minimal(name, BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 16, 8);
}

/* Minimal brain may have working_memory disabled by config; ensure one
 * exists and is empty so each test starts from a known WM state. */
static bool ensure_empty_wm(brain_t b) {
    if (!b->working_memory) {
        b->working_memory = working_memory_create();
    }
    if (!b->working_memory) return false;
    working_memory_clear(b->working_memory);
    return true;
}

/* ====================================================================== */
/* TEST 1: a salient WM item's feature vector lands in content_intent. */
static void test_wm_vector_blends_into_intent(void) {
    brain_t b = make_brain("wm_blend");
    EXPECT(b != NULL, "create brain");
    if (!b) return;

    if (!b->grounded_lang) {
        fprintf(stderr, "  SKIP: minimal brain has no grounded_lang\n");
        brain_destroy(b);
        return;
    }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    EXPECT(dim >= 8, "semantic_dim usable (got %u)", dim);
    if (dim < 8) { brain_destroy(b); return; }

    EXPECT(ensure_empty_wm(b), "ensure WM");
    if (!b->working_memory) { brain_destroy(b); return; }

    /* One-hot WM item at dim k, salience 0.9 (well above the 0.2 floor). */
    const uint32_t k = 5u;
    const float salience = 0.9f;
    float* item = (float*)calloc(dim, sizeof(float));
    EXPECT(item != NULL, "alloc item");
    if (!item) { brain_destroy(b); return; }
    item[k] = 1.0f;
    EXPECT(working_memory_add(b->working_memory, item, dim, salience),
           "wm add");

    production_cascade_state_t state = {0};
    int rc = communication_cascade_run(b, /*prompt=*/NULL,
                                       CASCADE_STAGE_CONTENT, &state);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    EXPECT(state.content_intent != NULL, "content_intent allocated");
    EXPECT(state.content_dim == dim, "content_dim=%u want %u",
           state.content_dim, dim);

    if (state.content_intent && state.content_dim == dim) {
        float got = state.content_intent[k];
        float want = W_WORKING_MEMORY * salience * 1.0f;  /* 0.225 */
        fprintf(stderr, "  content_intent[%u] = %.4f (want ~%.4f)\n",
                k, got, want);
        EXPECT(fabsf(got - want) < 1e-3f,
               "WM dim k blended: got %.4f want %.4f", got, want);
        /* A dim with no WM signal must stay ~0 (no other source ran). */
        uint32_t other = (k + 1u) % dim;
        EXPECT(fabsf(state.content_intent[other]) < 1e-4f,
               "non-WM dim %u must be ~0 (got %.4f)",
               other, state.content_intent[other]);
    }

    cascade_state_cleanup(&state);
    free(item);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_wm_vector_blends_into_intent\n");
}

/* ====================================================================== */
/* TEST 2: an item below the salience floor contributes nothing. */
static void test_low_salience_skipped(void) {
    brain_t b = make_brain("wm_low_sal");
    if (!b) return;
    if (!b->grounded_lang) { brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (dim < 8) { brain_destroy(b); return; }
    if (!ensure_empty_wm(b)) { brain_destroy(b); return; }

    const uint32_t k = 5u;
    float* item = (float*)calloc(dim, sizeof(float));
    if (!item) { brain_destroy(b); return; }
    item[k] = 1.0f;
    /* 0.1 < 0.2 floor → step 5b skips this item. */
    EXPECT(working_memory_add(b->working_memory, item, dim, 0.1f),
           "wm add low-salience");

    production_cascade_state_t state = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &state);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    if (state.content_intent && state.content_dim == dim) {
        fprintf(stderr, "  low-salience content_intent[%u] = %.4f\n",
                k, state.content_intent[k]);
        EXPECT(fabsf(state.content_intent[k]) < 1e-4f,
               "below-floor item must NOT blend (got %.4f)",
               state.content_intent[k]);
    }

    cascade_state_cleanup(&state);
    free(item);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_low_salience_skipped\n");
}

/* ====================================================================== */
/* TEST 3: empty WM → content-only cascade leaves content_intent ~0. */
static void test_empty_wm_no_signal(void) {
    brain_t b = make_brain("wm_empty");
    if (!b) return;
    if (!b->grounded_lang) { brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (dim < 8) { brain_destroy(b); return; }
    if (!ensure_empty_wm(b)) { brain_destroy(b); return; }

    production_cascade_state_t state = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &state);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    if (state.content_intent && state.content_dim == dim) {
        float maxabs = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            float a = fabsf(state.content_intent[i]);
            if (a > maxabs) maxabs = a;
        }
        fprintf(stderr, "  empty-WM max|content_intent| = %.6f\n", maxabs);
        EXPECT(maxabs < 1e-4f,
               "empty WM must leave content_intent ~0 (max %.6f)", maxabs);
    }

    cascade_state_cleanup(&state);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_empty_wm_no_signal\n");
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_wm_content_intent (Tier 1 Step D)\n");
    test_wm_vector_blends_into_intent();
    test_low_salience_skipped();
    test_empty_wm_no_signal();

    if (g_failures == 0) {
        fprintf(stderr, "OK — WM→content_intent blend guards passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
