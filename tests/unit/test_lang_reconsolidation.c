/**
 * @file test_lang_reconsolidation.c
 * @brief TA-5 — verify reconsolidation-on-contradiction.
 *
 * Coverage:
 *   1. test_default_off:
 *      Default toggle is OFF — comprehend with negation does not
 *      decay bindings, reconsolidation_events stays 0.
 *
 *   2. test_direct_api:
 *      grounded_language_reconsolidate_word multiplies binding strength
 *      by (1 - decay). Stats counters advance even when the word isn't
 *      in the lexicon (curriculum-meaningful signal).
 *
 *   3. test_decay_clamp:
 *      Setter clamps decay to [0, 0.5]; NaN/negative → 0; >0.5 → 0.5.
 *
 *   4. test_comprehend_with_negation:
 *      Enable reconsolidation, register a word with bindings, comprehend
 *      a sentence with explicit negation marking that word — its
 *      binding strength weakens.
 */

#include "language/nimcp_grounded_language.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

static void register_word(grounded_language_t* gl, const char* w, int seed)
{
    float feat[256];
    for (int i = 0; i < 256; i++) {
        feat[i] = ((float)((seed * 31 + i * 17) & 0xff)) / 255.0f - 0.5f;
    }
    (void)grounded_language_fast_map(gl, w, feat, 256, /*OBJECT*/ 1);
}

static float get_first_binding_strength(grounded_language_t* gl,
                                          const char* w) {
    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, w);
    if (!e || e->binding_count == 0) return -1.0f;
    return e->bindings[0].strength;
}

static void test_default_off(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    EXPECT(!grounded_language_get_reconsolidation_enabled(gl),
           "default OFF");

    register_word(gl, "fish", 7);
    float s_before = get_first_binding_strength(gl, "fish");
    EXPECT(s_before > 0.0f, "fish binding registered, got %.4f", s_before);

    /* Comprehend with negation but reconsolidation OFF — no decay. */
    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, "this is not fish", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    float s_after = get_first_binding_strength(gl, "fish");
    EXPECT(fabsf(s_after - s_before) < 1e-6f,
           "OFF: strength unchanged, before=%.4f after=%.4f",
           s_before, s_after);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.reconsolidation_events == 0,
           "OFF: events=0, got %llu",
           (unsigned long long)stats.reconsolidation_events);

    grounded_language_destroy(gl);
}

static void test_direct_api(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    register_word(gl, "cat", 3);
    float s_before = get_first_binding_strength(gl, "cat");
    EXPECT(s_before > 0.0f, "cat binding, got %.4f", s_before);

    /* 10% decay — binding shrinks by exactly that ratio. */
    uint32_t n = grounded_language_reconsolidate_word(gl, "cat", 0.10f);
    EXPECT(n >= 1, "decayed at least one binding, got %u", n);

    float s_after = get_first_binding_strength(gl, "cat");
    float expected = s_before * 0.9f;
    EXPECT(fabsf(s_after - expected) < 1e-5f,
           "10%% decay: expected %.4f, got %.4f", expected, s_after);

    /* Word not in lexicon — counter still bumps. */
    n = grounded_language_reconsolidate_word(gl, "nonexistent", 0.10f);
    EXPECT(n == 0, "missing word decays nothing, got %u", n);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.reconsolidation_events == 2,
           "events==2 (one hit + one miss), got %llu",
           (unsigned long long)stats.reconsolidation_events);
    EXPECT(stats.reconsolidation_bindings_decayed >= 1,
           "bindings_decayed>=1, got %llu",
           (unsigned long long)stats.reconsolidation_bindings_decayed);

    /* NULL gl/word handled gracefully. */
    EXPECT(grounded_language_reconsolidate_word(NULL, "x", 0.1f) == 0,
           "NULL gl returns 0");
    EXPECT(grounded_language_reconsolidate_word(gl, NULL, 0.1f) == 0,
           "NULL word returns 0");

    grounded_language_destroy(gl);
}

static void test_decay_clamp(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_reconsolidation_decay(gl, NAN);
    EXPECT(grounded_language_get_reconsolidation_decay(gl) == 0.0f,
           "NaN clamped to 0");

    grounded_language_set_reconsolidation_decay(gl, -1.0f);
    EXPECT(grounded_language_get_reconsolidation_decay(gl) == 0.0f,
           "negative clamped to 0");

    grounded_language_set_reconsolidation_decay(gl, 0.9f);
    EXPECT(grounded_language_get_reconsolidation_decay(gl) == 0.5f,
           "above-cap clamped to 0.5");

    grounded_language_set_reconsolidation_decay(gl, 0.05f);
    EXPECT(fabsf(grounded_language_get_reconsolidation_decay(gl) - 0.05f) < 1e-6f,
           "in-range value preserved");

    grounded_language_destroy(gl);
}

static void test_comprehend_with_negation(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_reconsolidation_enabled(gl, true);
    grounded_language_set_reconsolidation_decay(gl, 0.10f);
    EXPECT(grounded_language_get_reconsolidation_enabled(gl), "ON");

    register_word(gl, "shark", 11);
    float s_before = get_first_binding_strength(gl, "shark");
    EXPECT(s_before > 0.0f, "shark binding, got %.4f", s_before);

    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, "this is not shark", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    float s_after = get_first_binding_strength(gl, "shark");
    EXPECT(s_after < s_before,
           "ON: shark binding decayed: before=%.4f after=%.4f",
           s_before, s_after);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.reconsolidation_events >= 1,
           "events>=1, got %llu",
           (unsigned long long)stats.reconsolidation_events);

    grounded_language_destroy(gl);
}

/* Regression: TA-5 in comprehend depends on TA-3-/Tier-2-#3 negation
 * pass — without enable_negation_inversion the negate_word[] flags
 * stay all-false and the reconsolidation pass has nothing to act on.
 * Documents the dependency and verifies the no-fire case. */
static void test_requires_negation_inversion(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    /* Both flags togglable. */
    grounded_language_set_reconsolidation_enabled(gl, true);
    grounded_language_set_reconsolidation_decay(gl, 0.2f);
    /* Disable the negation pass — TA-5 should not fire. */
    grounded_language_set_negation_enabled(gl, false);
    EXPECT(!grounded_language_get_negation_enabled(gl),
           "negation OFF");
    EXPECT(grounded_language_get_reconsolidation_enabled(gl),
           "reconsolidation ON");

    register_word(gl, "whale", 13);
    float s_before = get_first_binding_strength(gl, "whale");
    EXPECT(s_before > 0.0f, "whale binding, got %.4f", s_before);

    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, "this is not whale", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    /* Without negation pass running, no decay. */
    float s_after = get_first_binding_strength(gl, "whale");
    EXPECT(fabsf(s_after - s_before) < 1e-6f,
           "negation OFF + recon ON: binding unchanged "
           "before=%.4f after=%.4f", s_before, s_after);

    /* And no reconsolidation_events should have fired. */
    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.reconsolidation_events == 0,
           "events must stay 0 when negation is OFF, got %llu",
           (unsigned long long)stats.reconsolidation_events);

    grounded_language_destroy(gl);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_reconsolidation (TA-5) ===\n");
    test_default_off();
    test_direct_api();
    test_decay_clamp();
    test_comprehend_with_negation();
    test_requires_negation_inversion();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
