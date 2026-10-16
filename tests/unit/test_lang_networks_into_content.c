/**
 * @file test_lang_networks_into_content.c
 * @brief UNIT — non-SNN networks (ANN/LNN/HNN/CNN) feed the language cascade's
 *        content_intent, default-OFF (2026-06-05; REDESIGNED 2026-06-06).
 *
 * Directive: every network except the FNO feeds the SNN language generator.
 * They converge on content_intent (which seeds the Wernicke concept pop).
 *
 * REDESIGN (2026-06-06): the first cut blended each network's RAW vector
 * (content_intent[j] += w·v). That regressed produce hard (pod A/B: 5% vs 55%
 * intent-fidelity) because last_decision (ANN) and the LNN liquid state are
 * ~CONSTANT across a produce sweep and larger in magnitude than the prompt
 * seed, so they became a DC bias that overwrote the intent direction — every
 * prompt collapsed to the same salad. The fix: each network is UNIT-NORMALIZED
 * then scaled to at most a_net × ||intent-so-far|| (the anchor), and only
 * blended when an anchor exists (seed_norm > 0). So a network is a bounded tilt
 * that can never dominate, and with no anchor it contributes nothing.
 *   - ANN: last_decision->output_vector  → a_ann = 0.15 × anchor
 *   - LNN: liquid state                  → a_lnn = 0.12 × anchor
 *   - CNN: cached feature embedding       → a_cnn = 0.08 × anchor
 *   - HNN: energy-deviation               → produce floor (no content vector)
 *
 * Isolation trick: run the cascade with ONLY CASCADE_STAGE_CONTENT. With a NULL
 * prompt and no cognitive source there is NO anchor, so the new design must
 * blend nothing. To get a deterministic anchor we inject one working-memory
 * item (the WM blend runs before the network blocks) and check the network
 * contribution is present but BOUNDED well below the anchor.
 *
 * What this test guards deterministically:
 *   1. ANN flag OFF → a one-hot last_decision does NOT blend (shipped default).
 *   2. ANN flag ON but NO anchor (NULL prompt, no cognitive) → still ~0. This
 *      is the core regression guard: a constant decision can't manufacture
 *      intent out of an empty content vector.
 *   3. ANN flag ON WITH a WM anchor → the decision blends, but its magnitude is
 *      a_ann × ||anchor|| and stays far below the anchor (can't dominate).
 *   4. Default-OFF safety: a one-hot last_decision present but ALL flags off →
 *      content_intent stays ~0 (proves the shipped path is unchanged).
 *   5. Gating no-op safety: LNN/HNN/CNN flags ON with those nets ABSENT → no
 *      crash and no spurious content_intent write.
 *   6. The visual_cortex_get_cached_features getter: idle cortex → 0 count.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_networks_into_content.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_networks_into_content
 */

#include "language/nimcp_communication_cascade.h"
#include "language/nimcp_grounded_language.h"
#include "cognitive/nimcp_working_memory.h"
#include "perception/nimcp_visual_cortex.h"
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

/* Must mirror cascade_stage_content. */
#define A_ANN            0.15f   /* ANN contribution = A_ANN × ||anchor|| */
#define W_WORKING_MEMORY 0.25f   /* WM blend weight (used to build the anchor) */

static brain_t make_brain(const char* name) {
    return brain_create_minimal(name, BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 16, 8);
}

/* Install a one-hot decision output at dim k as brain->last_decision.
 * Returns the heap decision so the caller can detach + free it before
 * brain_destroy (we own this pointer; the brain must not free it). */
static brain_decision_t* set_onehot_decision(brain_t b, uint32_t dim, uint32_t k) {
    brain_decision_t* d = (brain_decision_t*)calloc(1, sizeof(brain_decision_t));
    if (!d) return NULL;
    d->output_vector = (float*)calloc(dim, sizeof(float));
    if (!d->output_vector) { free(d); return NULL; }
    d->output_vector[k] = 1.0f;
    d->output_size = dim;
    b->last_decision = d;
    return d;
}

static void detach_decision(brain_t b, brain_decision_t* d) {
    if (b->last_decision == d) b->last_decision = NULL;
    if (d) { free(d->output_vector); free(d); }
}

/* Ensure an empty working memory so we can inject a deterministic anchor. */
static bool ensure_empty_wm(brain_t b) {
    if (!b->working_memory) b->working_memory = working_memory_create();
    if (!b->working_memory) return false;
    working_memory_clear(b->working_memory);
    return true;
}

/* ====================================================================== */
/* TEST 1+2: ANN gate OFF → no blend; ON but NO ANCHOR → still no blend.   */
static void test_ann_gate_and_anchor(void) {
    brain_t b = make_brain("ann_gate");
    EXPECT(b != NULL, "create brain");
    if (!b) return;
    if (!b->grounded_lang) {
        fprintf(stderr, "  SKIP: minimal brain has no grounded_lang\n");
        brain_destroy(b); return;
    }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    EXPECT(dim >= 8, "semantic_dim usable (got %u)", dim);
    if (dim < 8) { brain_destroy(b); return; }

    const uint32_t k = 5u;
    brain_decision_t* d = set_onehot_decision(b, dim, k);
    EXPECT(d != NULL, "alloc decision");
    if (!d) { brain_destroy(b); return; }

    /* (1) Flag OFF (default) → the one-hot decision must NOT blend. */
    b->cascade_ann_in_content = false;
    production_cascade_state_t s_off = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &s_off);
    EXPECT(rc == 0, "cascade(off) rc=%d", rc);
    if (s_off.content_intent && s_off.content_dim == dim) {
        EXPECT(fabsf(s_off.content_intent[k]) < 1e-4f,
               "ANN OFF must not blend (got %.4f)", s_off.content_intent[k]);
    }
    cascade_state_cleanup(&s_off);

    /* (2) Flag ON but NO anchor (NULL prompt + no cognitive source) → the
     * redesign gates the blend on seed_norm>0, so content_intent stays ~0.
     * THIS is the regression guard: a constant decision can't fabricate intent
     * from an empty content vector (the old raw blend wrote w_ann here). */
    b->cascade_ann_in_content = true;
    production_cascade_state_t s_on = {0};
    rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &s_on);
    EXPECT(rc == 0, "cascade(on) rc=%d", rc);
    if (s_on.content_intent && s_on.content_dim == dim) {
        float maxabs = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            float a = fabsf(s_on.content_intent[i]);
            if (a > maxabs) maxabs = a;
        }
        fprintf(stderr, "  ANN on, no-anchor max|content_intent| = %.6f (want ~0)\n",
                maxabs);
        EXPECT(maxabs < 1e-4f,
               "ANN ON with no anchor must blend nothing (max %.6f)", maxabs);
    }
    cascade_state_cleanup(&s_on);

    detach_decision(b, d);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_ann_gate_and_anchor\n");
}

/* ====================================================================== */
/* TEST 3: ANN ON WITH a WM anchor → the decision blends, but its magnitude
 * is A_ANN × ||anchor|| and stays far below the anchor (cannot dominate). */
static void test_ann_bounded_by_anchor(void) {
    brain_t b = make_brain("ann_bounded");
    if (!b) return;
    if (!b->grounded_lang) { brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (dim < 8) { brain_destroy(b); return; }
    if (!ensure_empty_wm(b)) { brain_destroy(b); return; }

    /* Anchor: one-hot WM item at dim m, salience 0.9 → the WM blend writes
     * content_intent[m] = W_WORKING_MEMORY × 0.9 = 0.225 before the network
     * blocks run, so ||anchor|| = 0.225 (single nonzero dim). */
    const uint32_t m = 2u;          /* anchor dim */
    const uint32_t k = 5u;          /* decision dim (distinct from m) */
    const float salience = 0.9f;
    float* item = (float*)calloc(dim, sizeof(float));
    if (!item) { brain_destroy(b); return; }
    item[m] = 1.0f;
    EXPECT(working_memory_add(b->working_memory, item, dim, salience), "wm add");

    brain_decision_t* d = set_onehot_decision(b, dim, k);
    if (!d) { free(item); brain_destroy(b); return; }
    b->cascade_ann_in_content = true;

    production_cascade_state_t s = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &s);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    if (s.content_intent && s.content_dim == dim) {
        float anchor = W_WORKING_MEMORY * salience;          /* 0.225 */
        float got    = s.content_intent[k];                  /* ANN at k */
        float want   = A_ANN * anchor;                       /* 0.15×0.225 */
        fprintf(stderr, "  anchor[%u]=%.4f  ANN[%u]=%.4f (want ~%.4f)\n",
                m, s.content_intent[m], k, got, want);
        /* The decision blended (nonzero at k)... */
        EXPECT(got > 1e-4f, "ANN must blend when anchored (got %.4f)", got);
        /* ...at the bounded magnitude A_ANN × ||anchor||... */
        EXPECT(fabsf(got - want) < 1e-3f,
               "ANN bounded blend: got %.4f want %.4f", got, want);
        /* ...and is far below the anchor itself (cannot dominate direction). */
        EXPECT(got < 0.5f * s.content_intent[m],
               "ANN (%.4f) must stay well below anchor (%.4f)",
               got, s.content_intent[m]);
        /* The anchor dim is untouched by the network blend. */
        EXPECT(fabsf(s.content_intent[m] - anchor) < 1e-3f,
               "anchor dim preserved: got %.4f want %.4f",
               s.content_intent[m], anchor);
    }
    cascade_state_cleanup(&s);
    detach_decision(b, d);
    free(item);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_ann_bounded_by_anchor\n");
}

/* ====================================================================== */
/* TEST 4: default-OFF safety — one-hot decision present, ALL flags off →
 * content_intent ~0 (the shipped path is byte-identical to pre-change). */
static void test_all_off_safety(void) {
    brain_t b = make_brain("all_off");
    if (!b) return;
    if (!b->grounded_lang) { brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (dim < 8) { brain_destroy(b); return; }

    brain_decision_t* d = set_onehot_decision(b, dim, 3u);
    if (!d) { brain_destroy(b); return; }

    EXPECT(!b->cascade_ann_in_content && !b->cascade_lnn_in_content &&
           !b->cascade_hnn_in_content && !b->cascade_cnn_in_content,
           "all four gates default OFF");

    production_cascade_state_t s = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &s);
    EXPECT(rc == 0, "cascade rc=%d", rc);
    if (s.content_intent && s.content_dim == dim) {
        float maxabs = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            float a = fabsf(s.content_intent[i]);
            if (a > maxabs) maxabs = a;
        }
        fprintf(stderr, "  all-off max|content_intent| = %.6f\n", maxabs);
        EXPECT(maxabs < 1e-4f,
               "all gates off must leave content_intent ~0 (max %.6f)", maxabs);
    }
    cascade_state_cleanup(&s);
    detach_decision(b, d);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_all_off_safety\n");
}

/* ====================================================================== */
/* TEST 5: gating no-op safety — LNN/HNN/CNN flags ON but those nets absent
 * (tiny brain) → no crash, no spurious write. */
static void test_absent_nets_no_crash(void) {
    brain_t b = make_brain("absent_nets");
    if (!b) return;
    if (!b->grounded_lang) { brain_destroy(b); return; }
    uint32_t dim = grounded_language_get_semantic_dim(b->grounded_lang);
    if (dim < 8) { brain_destroy(b); return; }

    b->cascade_ann_in_content = true;   /* last_decision is NULL → skip */
    b->cascade_lnn_in_content = true;   /* LNN absent or idle → ~0 */
    b->cascade_hnn_in_content = true;   /* energy-deviation 0 → floor only */
    b->cascade_cnn_in_content = true;   /* visual_cortex NULL/idle → skip */

    production_cascade_state_t s = {0};
    int rc = communication_cascade_run(b, NULL, CASCADE_STAGE_CONTENT, &s);
    EXPECT(rc == 0, "cascade rc=%d (must not crash with absent nets)", rc);
    if (s.content_intent && s.content_dim == dim) {
        float maxabs = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            float a = fabsf(s.content_intent[i]);
            if (a > maxabs) maxabs = a;
        }
        fprintf(stderr, "  absent-nets max|content_intent| = %.6f\n", maxabs);
        EXPECT(maxabs < 1e-4f,
               "absent nets must not write content_intent (max %.6f)", maxabs);
    }
    cascade_state_cleanup(&s);
    brain_destroy(b);
    if (g_failures == 0) fprintf(stderr, "PASS test_absent_nets_no_crash\n");
}

/* ====================================================================== */
/* TEST 6: the visual_cortex_get_cached_features getter — an idle cortex
 * (never processed an image) returns a clean 0-count, NULL args rejected. */
static void test_cnn_getter_idle(void) {
    visual_cortex_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.input_width = 16; cfg.input_height = 16;
    cfg.num_v1_filters = 4; cfg.feature_dim = 16;
    visual_cortex_t* vc = visual_cortex_create(&cfg);
    if (!vc) {
        fprintf(stderr, "  SKIP: visual_cortex_create returned NULL on this build\n");
        return;
    }
    float buf[32]; uint32_t n = 12345u;
    int rc = visual_cortex_get_cached_features(vc, buf, 32u, &n);
    EXPECT(rc == 0, "getter rc=%d on idle cortex", rc);
    EXPECT(n == 0u, "idle cortex → 0-count (got %u)", n);

    rc = visual_cortex_get_cached_features(vc, NULL, 32u, &n);
    EXPECT(rc == -1, "NULL out rejected");

    visual_cortex_destroy(vc);
    if (g_failures == 0) fprintf(stderr, "PASS test_cnn_getter_idle\n");
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_networks_into_content (redesign 2026-06-06)\n");
    test_ann_gate_and_anchor();
    test_ann_bounded_by_anchor();
    test_all_off_safety();
    test_absent_nets_no_crash();
    test_cnn_getter_idle();

    if (g_failures == 0) {
        fprintf(stderr, "OK — networks→content_intent bounded-blend guards passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
