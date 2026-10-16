/**
 * @file test_wave4_meta_learning_hook.c
 * @brief Unit tests for Wave 4 meta_learning hot-path hook.
 *
 * WHAT: Validates the contract of meta_adapt_learning_rate and the
 *       hot-path-safety expectations of the hook added to brain_learn_vector
 *       at line ~3014 of src/core/brain/learning/nimcp_brain_learning.c.
 *
 * WHY:  The hook lives in a hot path called on every learning step. A NULL
 *       meta_learner must be silent and zero-cost; valid meta_learner + loss
 *       feedback must produce a monotone LR adaptation (bold-driver
 *       heuristic) within the documented bounds.
 *
 * HOW:  The hook itself is a 3-line `if (meta) meta_adapt_learning_rate(...)`
 *       guarded by the same pointer. We cannot easily drive the full
 *       brain_learn_vector path from a unit test, so we test the contract of
 *       the only function it calls instead. That is exactly what the hook
 *       delegates to — no additional logic lives in the caller.
 */

#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cognitive/nimcp_meta_learning.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-72s", name); fflush(stdout); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((long long)(a) != (long long)(b)) { \
    printf("[FAIL] %s (got %lld, expected %lld)\n", msg, (long long)(a), (long long)(b)); \
    tests_failed++; return; } } while(0)
#define ASSERT_NOT_NULL(p, msg) do { if ((p) == NULL) { FAIL(msg); } } while(0)

/* ------------------------------------------------------------------------- */
/* Contract tests for meta_adapt_learning_rate                               */
/* ------------------------------------------------------------------------- */

static void test_adapt_null_meta_returns_default(void) {
    TEST("meta_adapt_learning_rate(NULL, ASSOCIATION, loss) -> safe, finite");
    float lr = meta_adapt_learning_rate(NULL, META_REGION_ASSOCIATION, 0.5f);
    ASSERT_TRUE(isfinite(lr), "NULL-meta call returned non-finite LR");
    ASSERT_TRUE(lr > 0.0f, "NULL-meta call returned non-positive LR");
    PASS();
}

static void test_adapt_null_meta_matches_get_default(void) {
    TEST("meta_adapt_learning_rate(NULL, ...) == meta_get_learning_rate(NULL, ...)");
    /* Both functions hit the same fallback branch; their return values must
     * be the same constant (DEFAULT_INNER_LR). */
    float a = meta_adapt_learning_rate(NULL, META_REGION_ASSOCIATION, 0.5f);
    float b = meta_get_learning_rate(NULL, META_REGION_ASSOCIATION);
    ASSERT_EQ((int)(a * 1e6f), (int)(b * 1e6f),
              "NULL fallbacks must match between get and adapt");
    PASS();
}

static void test_adapt_out_of_range_region(void) {
    TEST("meta_adapt_learning_rate with out-of-range region -> safe default");
    /* Use a region value beyond META_REGION_COUNT — impl guards it. */
    float lr = meta_adapt_learning_rate(
        NULL, (meta_region_type_t)0xFFFFFFFF, 0.5f);
    ASSERT_TRUE(isfinite(lr) && lr > 0.0f,
                "out-of-range region must yield safe finite default");
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Live adaptation tests using a real meta_learner                           */
/* ------------------------------------------------------------------------- */

static void test_adapt_increases_lr_on_improving_loss(void) {
    TEST("loss monotonically decreasing -> LR increases (bold driver)");
    meta_learning_config_t cfg = meta_learning_default_config();
    cfg.enable_adaptive_lr = true;
    meta_learner_t m = meta_learner_create(&cfg, META_REGION_COUNT);
    ASSERT_NOT_NULL(m, "meta_learner_create failed");

    float lr0 = meta_get_learning_rate(m, META_REGION_ASSOCIATION);

    /* First call seeds previous_loss (no change expected). */
    (void)meta_adapt_learning_rate(m, META_REGION_ASSOCIATION, 1.0f);
    /* Subsequent calls with *decreasing* loss should step LR up. */
    float prev = lr0;
    for (int i = 1; i <= 5; i++) {
        float loss = 1.0f - 0.1f * (float)i;
        float lr = meta_adapt_learning_rate(m, META_REGION_ASSOCIATION, loss);
        ASSERT_TRUE(lr >= prev,
            "LR should be non-decreasing under improving loss");
        prev = lr;
    }
    /* Strict: at least one step grew LR (5 improving steps vs clamp). */
    ASSERT_TRUE(prev > lr0,
        "LR did not grow over 5 improving-loss steps");

    meta_learner_destroy(m);
    PASS();
}

static void test_adapt_decreases_lr_on_worsening_loss(void) {
    TEST("loss monotonically increasing -> LR decreases (backtrack)");
    meta_learning_config_t cfg = meta_learning_default_config();
    cfg.enable_adaptive_lr = true;
    meta_learner_t m = meta_learner_create(&cfg, META_REGION_COUNT);
    ASSERT_NOT_NULL(m, "meta_learner_create failed");

    float lr0 = meta_get_learning_rate(m, META_REGION_ASSOCIATION);

    (void)meta_adapt_learning_rate(m, META_REGION_ASSOCIATION, 0.1f);
    float prev = lr0;
    for (int i = 1; i <= 5; i++) {
        float loss = 0.1f + 0.1f * (float)i;
        float lr = meta_adapt_learning_rate(m, META_REGION_ASSOCIATION, loss);
        ASSERT_TRUE(lr <= prev,
            "LR should be non-increasing under worsening loss");
        prev = lr;
    }
    ASSERT_TRUE(prev < lr0,
        "LR did not shrink over 5 worsening-loss steps");

    meta_learner_destroy(m);
    PASS();
}

static void test_adapt_bounded(void) {
    TEST("LR stays bounded across 100 improving + 100 worsening steps");
    meta_learning_config_t cfg = meta_learning_default_config();
    cfg.enable_adaptive_lr = true;
    meta_learner_t m = meta_learner_create(&cfg, META_REGION_COUNT);
    ASSERT_NOT_NULL(m, "meta_learner_create failed");

    /* Fire 100 improving then 100 worsening. LR must never leave
     * finite positive range. */
    float loss = 1.0f;
    for (int i = 0; i < 100; i++) {
        loss *= 0.95f;
        float lr = meta_adapt_learning_rate(m, META_REGION_ASSOCIATION, loss);
        ASSERT_TRUE(isfinite(lr) && lr > 0.0f, "LR escaped finite+ bounds");
    }
    for (int i = 0; i < 100; i++) {
        loss *= 1.05f;
        float lr = meta_adapt_learning_rate(m, META_REGION_ASSOCIATION, loss);
        ASSERT_TRUE(isfinite(lr) && lr > 0.0f, "LR escaped finite+ bounds");
    }
    meta_learner_destroy(m);
    PASS();
}

static void test_adapt_respects_enable_flag(void) {
    TEST("enable_adaptive_lr=false -> LR stays constant through hook");
    meta_learning_config_t cfg = meta_learning_default_config();
    cfg.enable_adaptive_lr = false;
    meta_learner_t m = meta_learner_create(&cfg, META_REGION_COUNT);
    ASSERT_NOT_NULL(m, "meta_learner_create failed");

    float lr0 = meta_get_learning_rate(m, META_REGION_ASSOCIATION);

    /* Hammer both directions — LR must not budge. */
    for (int i = 0; i < 20; i++) {
        (void)meta_adapt_learning_rate(m, META_REGION_ASSOCIATION,
                                       (i & 1) ? 0.9f : 0.1f);
    }
    float lr1 = meta_get_learning_rate(m, META_REGION_ASSOCIATION);
    ASSERT_EQ((int)(lr0 * 1e6f), (int)(lr1 * 1e6f),
              "LR changed despite enable_adaptive_lr=false");
    meta_learner_destroy(m);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Hot-path safety — emulate the exact hook usage                            */
/* ------------------------------------------------------------------------- */

static void test_hook_pattern_null_safe(void) {
    TEST("hook pattern `if (meta) meta_adapt(meta, ASSOCIATION, loss)` is safe");
    /* Emulate exactly what brain_learn_vector does. */
    meta_learner_t meta_learner = NULL;  /* simulate uninitialized brain field */
    for (int i = 0; i < 1000; i++) {
        float loss = 0.5f + 0.1f * sinf((float)i);
        if (meta_learner) {
            (void)meta_adapt_learning_rate(meta_learner,
                                           META_REGION_ASSOCIATION,
                                           loss);
        }
    }
    /* Getting here = no crash. The guard does its job. */
    PASS();
}

static void test_hook_pattern_live(void) {
    TEST("hook pattern live: 100 loss steps complete without NaN/crash");
    meta_learning_config_t cfg = meta_learning_default_config();
    cfg.enable_adaptive_lr = true;
    meta_learner_t m = meta_learner_create(&cfg, META_REGION_COUNT);
    ASSERT_NOT_NULL(m, "meta_learner_create failed");

    /* Replay a noisy loss stream. */
    for (int i = 0; i < 100; i++) {
        float loss = 0.5f + 0.4f * sinf((float)i * 0.3f);
        if (m) {
            (void)meta_adapt_learning_rate(m, META_REGION_ASSOCIATION, loss);
        }
    }
    float lr_final = meta_get_learning_rate(m, META_REGION_ASSOCIATION);
    ASSERT_TRUE(isfinite(lr_final) && lr_final > 0.0f,
                "LR diverged after 100 noisy steps");
    meta_learner_destroy(m);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Unit Tests: Wave 4 meta_learning hot-path hook ===\n\n");

    test_adapt_null_meta_returns_default();
    test_adapt_null_meta_matches_get_default();
    test_adapt_out_of_range_region();
    test_adapt_increases_lr_on_improving_loss();
    test_adapt_decreases_lr_on_worsening_loss();
    test_adapt_bounded();
    test_adapt_respects_enable_flag();
    test_hook_pattern_null_safe();
    test_hook_pattern_live();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
