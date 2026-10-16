/**
 * @file test_lang_topic_shift.c
 * @brief TB-10 — verify topic-shift detection in discourse.
 *
 * Coverage:
 *   1. test_default_off:
 *      Default toggle is OFF — comprehending a topically-coherent
 *      dialog reports no shifts (counter == 0, last_was_topic_shift
 *      stays false, last_topic_shift_score stays at the 1.0 init).
 *
 *   2. test_threshold_clamp:
 *      Threshold setter clamps to [0, 1]; NaN → 0; negative → 0;
 *      above-cap → 1.0. min_turns clamps to [2, MAX_TURNS_PUBLIC].
 *
 *   3. test_coherent_dialog:
 *      Enabled, four cat-themed turns — high cosine across the ring
 *      means no boundaries fire. topic_shifts_evaluated advances
 *      starting at the (min_turns+1)th comprehend; topic_shifts_detected
 *      stays 0.
 *
 *   4. test_abrupt_shift:
 *      Enabled, three cat-themed turns followed by a quantum-mechanics
 *      turn. The 4th comprehend evaluates the detector against the
 *      prior 3 cat turns; the cosine score collapses below the
 *      threshold and last_was_topic_shift becomes true while
 *      topic_shifts_detected advances by 1.
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

/* Register a content word with deterministic features so that
 * comprehending a sentence containing the word produces a non-zero
 * semantic vector aligned with that feature seed. Different seeds
 * produce different (mostly-orthogonal) vectors, which is what the
 * topic-shift detector keys off. */
static void register_word(grounded_language_t* gl, const char* w, int seed)
{
    float feat[256];
    for (int i = 0; i < 256; i++) {
        feat[i] = ((float)((seed * 31 + i * 17) & 0xff)) / 255.0f - 0.5f;
    }
    (void)grounded_language_fast_map(gl, w, feat, 256, /*OBJECT*/ 1);
}

/* Run a comprehend + drop the result — for tests that only care about
 * the side-effect on gl's discourse buffer + topic-shift state. */
static void run_comprehend(grounded_language_t* gl, const char* text)
{
    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, text, &r);
    EXPECT(rc == 0, "comprehend rc=%d for '%s'", rc, text);
    gl_comprehension_result_cleanup(&r);
}

static void test_default_off(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    EXPECT(!grounded_language_get_topic_shift_enabled(gl),
           "default OFF");
    EXPECT(grounded_language_get_last_topic_shift_score(gl) == 1.0f,
           "init score == 1.0");
    EXPECT(!grounded_language_last_was_topic_shift(gl),
           "init flag == false");

    register_word(gl, "cat", 7);
    /* Run a coherent batch of comprehends with the flag OFF. Even
     * though discourse turns get pushed, the detector must not
     * advance any counter. */
    run_comprehend(gl, "the cat sat on the mat");
    run_comprehend(gl, "the cat is happy");
    run_comprehend(gl, "i love this cat");
    run_comprehend(gl, "the cat purred");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.topic_shifts_evaluated == 0,
           "OFF: evaluated=0, got %llu",
           (unsigned long long)stats.topic_shifts_evaluated);
    EXPECT(stats.topic_shifts_detected == 0,
           "OFF: detected=0, got %llu",
           (unsigned long long)stats.topic_shifts_detected);
    EXPECT(!grounded_language_last_was_topic_shift(gl),
           "OFF: flag stays false");

    grounded_language_destroy(gl);
}

static void test_threshold_clamp(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_topic_shift_threshold(gl, NAN);
    EXPECT(grounded_language_get_topic_shift_threshold(gl) == 0.0f,
           "NaN clamped to 0");

    grounded_language_set_topic_shift_threshold(gl, -0.5f);
    EXPECT(grounded_language_get_topic_shift_threshold(gl) == 0.0f,
           "negative clamped to 0");

    grounded_language_set_topic_shift_threshold(gl, 2.5f);
    EXPECT(grounded_language_get_topic_shift_threshold(gl) == 1.0f,
           "above-cap clamped to 1");

    grounded_language_set_topic_shift_threshold(gl, 0.42f);
    EXPECT(fabsf(grounded_language_get_topic_shift_threshold(gl) - 0.42f) < 1e-6f,
           "in-range value preserved");

    /* min_turns clamping. */
    grounded_language_set_topic_shift_min_turns(gl, 0u);
    EXPECT(grounded_language_get_topic_shift_min_turns(gl) == 2u,
           "0 clamped up to 2, got %u",
           grounded_language_get_topic_shift_min_turns(gl));

    grounded_language_set_topic_shift_min_turns(gl, 1u);
    EXPECT(grounded_language_get_topic_shift_min_turns(gl) == 2u,
           "1 clamped up to 2");

    grounded_language_set_topic_shift_min_turns(gl, 1000u);
    EXPECT(grounded_language_get_topic_shift_min_turns(gl)
               == (uint32_t)GL_DISCOURSE_MAX_TURNS_PUBLIC,
           "huge clamped down to MAX");

    grounded_language_set_topic_shift_min_turns(gl, 4u);
    EXPECT(grounded_language_get_topic_shift_min_turns(gl) == 4u,
           "in-range value preserved");

    /* NULL-safe getters. */
    EXPECT(grounded_language_get_topic_shift_enabled(NULL) == false,
           "NULL gl get_enabled returns false");
    EXPECT(grounded_language_get_topic_shift_threshold(NULL) == 0.0f,
           "NULL gl get_threshold returns 0");
    EXPECT(grounded_language_get_topic_shift_min_turns(NULL) == 0u,
           "NULL gl get_min_turns returns 0");
    EXPECT(grounded_language_get_last_topic_shift_score(NULL) == 1.0f,
           "NULL gl get_last_score returns 1");
    EXPECT(!grounded_language_last_was_topic_shift(NULL),
           "NULL gl last_was returns false");

    grounded_language_destroy(gl);
}

static void test_coherent_dialog(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_topic_shift_enabled(gl, true);
    /* Use the conservative defaults — threshold 0.3, min_turns 3. */
    EXPECT(grounded_language_get_topic_shift_enabled(gl), "ON");

    /* Register a cat-themed word so all four turns share the same
     * dominant feature seed and produce highly-correlated semantic
     * vectors. */
    register_word(gl, "cat", 7);

    run_comprehend(gl, "the cat sat on the mat");
    run_comprehend(gl, "the cat is happy");
    run_comprehend(gl, "i love this cat");
    run_comprehend(gl, "the cat purred");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    /* Detector evaluates the (min_turns+1)th and later comprehends.
     * With min_turns=3 and 4 successful pushes, that's the 4th
     * comprehend → exactly 1 evaluation expected. (Successful push
     * is contingent on a non-zero semantic vector — empty seeds
     * could push 0 turns, hence >= rather than == below.) */
    EXPECT(stats.topic_shifts_evaluated >= 1,
           "coherent: evaluated >= 1, got %llu",
           (unsigned long long)stats.topic_shifts_evaluated);
    /* Coherent dialog must NOT trip the boundary. */
    EXPECT(stats.topic_shifts_detected == 0,
           "coherent: detected == 0, got %llu (score=%.4f)",
           (unsigned long long)stats.topic_shifts_detected,
           grounded_language_get_last_topic_shift_score(gl));
    EXPECT(!grounded_language_last_was_topic_shift(gl),
           "coherent: flag stays false");

    grounded_language_destroy(gl);
}

static void test_abrupt_shift(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_topic_shift_enabled(gl, true);
    /* Bump threshold above default so even a moderate cosine drop
     * crosses it. The detector clamps cosine into [0, 1] — picking
     * 0.95 means almost any cross-topic transition fires. */
    grounded_language_set_topic_shift_threshold(gl, 0.95f);
    /* Three priors are enough to anchor a stable mean. */
    grounded_language_set_topic_shift_min_turns(gl, 3u);

    /* Cat-themed priors, then a sharply-different topic. Use a
     * different seed for the unrelated word so its semantic vector
     * is roughly orthogonal to the cat cluster. */
    register_word(gl, "cat",     7);
    register_word(gl, "quantum", 211);
    register_word(gl, "particles", 199);

    run_comprehend(gl, "the cat sat on the mat");
    run_comprehend(gl, "the cat is happy");
    run_comprehend(gl, "i love this cat");

    /* Snapshot detected count just before the shift. */
    gl_stats_t before;
    grounded_language_get_stats(gl, &before);

    run_comprehend(gl, "quantum mechanics governs particles");

    gl_stats_t after;
    grounded_language_get_stats(gl, &after);

    EXPECT(after.topic_shifts_evaluated >= before.topic_shifts_evaluated + 1,
           "shift turn evaluated, before=%llu after=%llu",
           (unsigned long long)before.topic_shifts_evaluated,
           (unsigned long long)after.topic_shifts_evaluated);
    EXPECT(after.topic_shifts_detected >= before.topic_shifts_detected + 1,
           "shift detected, before=%llu after=%llu (score=%.4f, thr=%.4f)",
           (unsigned long long)before.topic_shifts_detected,
           (unsigned long long)after.topic_shifts_detected,
           grounded_language_get_last_topic_shift_score(gl),
           grounded_language_get_topic_shift_threshold(gl));
    EXPECT(grounded_language_last_was_topic_shift(gl),
           "last_was flag set after abrupt shift (score=%.4f, thr=%.4f)",
           grounded_language_get_last_topic_shift_score(gl),
           grounded_language_get_topic_shift_threshold(gl));
    EXPECT(grounded_language_get_last_topic_shift_score(gl)
               < grounded_language_get_topic_shift_threshold(gl),
           "last score below threshold (score=%.4f, thr=%.4f)",
           grounded_language_get_last_topic_shift_score(gl),
           grounded_language_get_topic_shift_threshold(gl));

    grounded_language_destroy(gl);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_topic_shift (TB-10) ===\n");
    test_default_off();
    test_threshold_clamp();
    test_coherent_dialog();
    test_abrupt_shift();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
