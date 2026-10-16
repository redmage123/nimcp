/**
 * @file test_lang_stepe_cognitive_intent.c
 * @brief UNIT — Tier 1 Step E: discourse / reasoning / imagination sources
 *        blend into the cascade's content_intent.
 *
 * Step E (2026-05-24) adds three more content_intent sources to
 * cascade_stage_content, on top of Step D's working memory:
 *   5c. discourse continuity — the prior turn's topic vector (w_discourse)
 *   5d. imagination          — active imagined-scenario latent (w_imagination)
 *   5e. reasoning            — cached inference conclusion (w_reasoning,
 *                              scaled by chain confidence)
 *
 * Isolation method (same as Step D): run communication_cascade_run with
 * stage_mask = CASCADE_STAGE_CONTENT only + NULL prompt. Every OTHER
 * content_intent source sits at zero state (no drive/episodic/goal/WM/
 * arcuate), so content_intent equals exactly the source under test and its
 * magnitude is deterministic.
 *
 * Guards:
 *   1. Discourse: with two pushed turns (prior one-hot at dim k), the prior
 *      turn lands at w_discourse * 1.0 in content_intent[k]; non-discourse
 *      dims stay ~0. With <2 turns the blend no-ops.
 *   2. Reasoning: a cached conclusion vector (one-hot at k, confidence c)
 *      lands at w_reasoning * c in content_intent[k]; cache dim==0 no-ops.
 *   3. Imagination: the copy accessor returns 0 (idle) on a fresh engine,
 *      so the cascade blend cleanly no-ops when nothing is being imagined.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_stepe_cognitive_intent.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_stepe_cognitive_intent
 */

#include "language/nimcp_communication_cascade.h"
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"

/* The imagination engine header redeclares audio_cortex_t / spatial_transform_t
 * / visual_training_state_t in ways that clash with the real definitions
 * pulled in transitively by nimcp_brain_internal.h (same TU conflict the
 * cascade hits). Forward-declare the three functions we exercise instead.
 * `struct imagination_engine` is the tag behind imagination_engine_t and
 * behind brain->imagination, so the types line up; config is passed NULL so
 * a `const void*` parameter is sufficient here. */
struct imagination_engine;
extern struct imagination_engine* imagination_engine_create(const void* config);
extern void imagination_engine_destroy(struct imagination_engine* engine);
extern uint32_t imagination_engine_copy_active_vector(
    struct imagination_engine* engine, float* caller_buf,
    uint32_t caller_cap, float* out_vividness);

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

/* Must mirror the weights in cascade_stage_content. */
#define W_DISCOURSE   0.15f
#define W_REASONING   0.3f

static brain_t make_brain(const char* name) {
    return brain_create_minimal(name, BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 16, 8);
}

/* ====================================================================== */
/* TEST 1: prior discourse turn blends into content_intent. */
static void test_discourse_continuity_blend(void) {
    brain_t b = make_brain("stepe_discourse");
    EXPECT(b != NULL, "create brain");
    if (!b) return;
    if (!b->grounded_lang) { fprintf(stderr, "  SKIP no grounded_lang\n"); brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    EXPECT(dim >= 8, "dim usable (%u)", dim);
    if (dim < 8) { brain_destroy(b); return; }

    /* Clear WM so it can't contribute (minimal brain may or may not have it). */
    if (b->working_memory) working_memory_clear(b->working_memory);

    const uint32_t k = 6u;

    /* Push two turns. communication_cascade_run is called with NULL prompt
     * (no comprehend → no extra push), so the ring holds exactly these two:
     * turn0 is the PRIOR (back=2), turn1 is the most-recent. */
    float* prior = (float*)calloc(dim, sizeof(float));
    float* recent = (float*)calloc(dim, sizeof(float));
    EXPECT(prior && recent, "alloc turn vecs");
    if (!prior || !recent) { free(prior); free(recent); brain_destroy(b); return; }
    prior[k] = 1.0f;          /* prior turn: distinctive one-hot at k */
    recent[(k + 3u) % dim] = 1.0f;
    EXPECT(grounded_language_push_turn(b->grounded_lang, prior, dim, 3u, true) == 0,
           "push prior turn");
    EXPECT(grounded_language_push_turn(b->grounded_lang, recent, dim, 3u, false) == 0,
           "push recent turn");
    EXPECT(grounded_language_get_discourse_turn_count(b->grounded_lang) >= 2,
           "two turns present");

    production_cascade_state_t state = {0};
    int rc = communication_cascade_run(b, /*prompt=*/NULL,
                                       CASCADE_STAGE_CONTENT, &state);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    EXPECT(state.content_intent && state.content_dim == dim, "content allocated");
    if (state.content_intent && state.content_dim == dim) {
        float got = state.content_intent[k];
        fprintf(stderr, "  discourse content_intent[%u] = %.4f (want ~%.4f)\n",
                k, got, W_DISCOURSE);
        EXPECT(fabsf(got - W_DISCOURSE) < 1e-3f,
               "prior turn blended: got %.4f want %.4f", got, W_DISCOURSE);
        /* A dim untouched by either turn must stay ~0. */
        uint32_t empty = (k + 1u) % dim;
        if (empty != ((k + 3u) % dim)) {
            EXPECT(fabsf(state.content_intent[empty]) < 1e-4f,
                   "non-discourse dim %u ~0 (got %.4f)", empty,
                   state.content_intent[empty]);
        }
    }
    cascade_state_cleanup(&state);
    free(prior);
    free(recent);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_discourse_continuity_blend\n");
}

/* TEST 1b: a single turn (count<2) → discourse blend no-ops. */
static void test_discourse_single_turn_noop(void) {
    brain_t b = make_brain("stepe_disc_single");
    if (!b) return;
    if (!b->grounded_lang) { brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (dim < 8) { brain_destroy(b); return; }
    if (b->working_memory) working_memory_clear(b->working_memory);

    const uint32_t k = 6u;
    float* only = (float*)calloc(dim, sizeof(float));
    if (!only) { brain_destroy(b); return; }
    only[k] = 1.0f;
    grounded_language_push_turn(b->grounded_lang, only, dim, 3u, true);

    production_cascade_state_t state = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &state);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    if (state.content_intent && state.content_dim == dim) {
        fprintf(stderr, "  single-turn content_intent[%u] = %.4f\n",
                k, state.content_intent[k]);
        EXPECT(fabsf(state.content_intent[k]) < 1e-4f,
               "single turn must not blend (back=2 missing) got %.4f",
               state.content_intent[k]);
    }
    cascade_state_cleanup(&state);
    free(only);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_discourse_single_turn_noop\n");
}

/* ====================================================================== */
/* TEST 2: cached reasoning conclusion blends into content_intent, scaled
 * by confidence. We set the cache directly (the read side is the cascade
 * integration point); the opt-in flag stays OFF so cascade_prime_reasoning
 * leaves our cache untouched. */
static void test_reasoning_blend(void) {
    brain_t b = make_brain("stepe_reason");
    if (!b) return;
    if (!b->grounded_lang) { brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (dim < 8) { brain_destroy(b); return; }
    if (b->working_memory) working_memory_clear(b->working_memory);

    const uint32_t k = 4u;
    const float conf = 0.5f;
    /* nimcp_calloc so brain_destroy's nimcp_free matches the allocator. */
    b->cascade_reasoning_vec = (float*)nimcp_calloc(dim, sizeof(float));
    EXPECT(b->cascade_reasoning_vec != NULL, "alloc reasoning cache");
    if (!b->cascade_reasoning_vec) { brain_destroy(b); return; }
    b->cascade_reasoning_vec[k] = 1.0f;
    b->cascade_reasoning_cap = dim;
    b->cascade_reasoning_dim = dim;          /* publish: valid */
    b->cascade_reasoning_confidence = conf;
    b->cascade_reason_in_content = false;    /* keep prime a no-op */

    production_cascade_state_t state = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &state);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    if (state.content_intent && state.content_dim == dim) {
        float got = state.content_intent[k];
        float want = W_REASONING * conf * 1.0f;  /* 0.15 */
        fprintf(stderr, "  reasoning content_intent[%u] = %.4f (want ~%.4f)\n",
                k, got, want);
        EXPECT(fabsf(got - want) < 1e-3f,
               "reasoning blended: got %.4f want %.4f", got, want);
    }
    cascade_state_cleanup(&state);
    brain_destroy(b);   /* frees cascade_reasoning_vec */
    if (g_failures == 0) fprintf(stderr, "PASS test_reasoning_blend\n");
}

/* TEST 2b: reasoning cache dim==0 → blend no-ops. */
static void test_reasoning_invalid_cache_noop(void) {
    brain_t b = make_brain("stepe_reason_off");
    if (!b) return;
    if (!b->grounded_lang) { brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (dim < 8) { brain_destroy(b); return; }
    if (b->working_memory) working_memory_clear(b->working_memory);

    const uint32_t k = 4u;
    b->cascade_reasoning_vec = (float*)nimcp_calloc(dim, sizeof(float));
    if (!b->cascade_reasoning_vec) { brain_destroy(b); return; }
    b->cascade_reasoning_vec[k] = 1.0f;
    b->cascade_reasoning_cap = dim;
    b->cascade_reasoning_dim = 0;            /* INVALID — must be ignored */
    b->cascade_reasoning_confidence = 0.9f;

    production_cascade_state_t state = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &state);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    if (state.content_intent && state.content_dim == dim) {
        fprintf(stderr, "  invalid-cache content_intent[%u] = %.4f\n",
                k, state.content_intent[k]);
        EXPECT(fabsf(state.content_intent[k]) < 1e-4f,
               "dim==0 cache must not blend (got %.4f)",
               state.content_intent[k]);
    }
    cascade_state_cleanup(&state);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_reasoning_invalid_cache_noop\n");
}

/* ====================================================================== */
/* TEST 3: imagination accessor returns 0 (idle) on a fresh engine, so the
 * cascade blend cleanly no-ops when nothing is being imagined. */
static void test_imagination_idle_returns_zero(void) {
    struct imagination_engine* eng = imagination_engine_create(NULL);
    EXPECT(eng != NULL, "create imagination engine");
    if (!eng) return;

    float buf[64];
    for (uint32_t i = 0; i < 64; i++) buf[i] = -1.0f;  /* sentinel */
    float vividness = 7.0f;
    uint32_t n = imagination_engine_copy_active_vector(eng, buf, 64u, &vividness);
    fprintf(stderr, "  imagination idle copy -> n=%u vividness=%.2f\n", n, vividness);
    EXPECT(n == 0, "idle engine copies 0 floats (got %u)", n);
    EXPECT(vividness == 0.0f, "idle vividness reset to 0 (got %.2f)", vividness);

    /* NULL-arg contracts. */
    EXPECT(imagination_engine_copy_active_vector(NULL, buf, 64u, NULL) == 0,
           "NULL engine -> 0");
    EXPECT(imagination_engine_copy_active_vector(eng, NULL, 64u, NULL) == 0,
           "NULL buf -> 0");
    EXPECT(imagination_engine_copy_active_vector(eng, buf, 0u, NULL) == 0,
           "0 cap -> 0");

    imagination_engine_destroy(eng);
    if (g_failures == 0) fprintf(stderr, "PASS test_imagination_idle_returns_zero\n");
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_stepe_cognitive_intent (Tier 1 Step E)\n");
    test_discourse_continuity_blend();
    test_discourse_single_turn_noop();
    test_reasoning_blend();
    test_reasoning_invalid_cache_noop();
    test_imagination_idle_returns_zero();

    if (g_failures == 0) {
        fprintf(stderr, "OK — Step E cognitive/discourse intent blends passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
