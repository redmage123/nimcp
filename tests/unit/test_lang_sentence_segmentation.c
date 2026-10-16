/**
 * @file test_lang_sentence_segmentation.c
 * @brief TB-6 — verify sentence-boundary segmentation in comprehend.
 *
 * Coverage:
 *   1. test_default_off_single_turn:
 *      Default toggle is OFF — a 2-sentence input pushes exactly ONE
 *      discourse turn (legacy behaviour preserved bit-for-bit).
 *      sentences_processed and multi_sentence_inputs stay 0.
 *
 *   2. test_enabled_two_sentences:
 *      Flag ON — same 2-sentence input pushes TWO discourse turns,
 *      sentences_processed advances by 2, and multi_sentence_inputs
 *      advances by 1.
 *
 *   3. test_enabled_single_sentence:
 *      Flag ON, single-sentence input — sentences_processed advances by
 *      1, multi_sentence_inputs stays 0, and only one discourse turn
 *      is pushed.
 *
 *   4. test_toggle_setter_getter:
 *      Setter / getter NULL-safety + flip-flop round-trip.
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

/* Small helper — register a content word so the comprehend pass produces
 * a non-zero semantic vector and the auto-push at the bottom of comprehend
 * fires (zero-vector inputs skip the push). Mirrors the test-helper
 * pattern in test_lang_reconsolidation.c. */
static void register_word(grounded_language_t* gl, const char* w, int seed)
{
    float feat[256];
    for (int i = 0; i < 256; i++) {
        feat[i] = ((float)((seed * 31 + i * 17) & 0xff)) / 255.0f - 0.5f;
    }
    (void)grounded_language_fast_map(gl, w, feat, 256, /*OBJECT*/ 1);
}

static void test_default_off_single_turn(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    EXPECT(!grounded_language_get_sentence_segmentation_enabled(gl),
           "default OFF");

    register_word(gl, "cat", 5);
    register_word(gl, "happy", 9);

    uint8_t turns_before = grounded_language_get_discourse_turn_count(gl);

    /* 2-sentence input. With segmentation OFF the legacy code path
     * processes the whole string as one utterance and pushes one turn. */
    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, "the cat sat. it was happy.", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    uint8_t turns_after = grounded_language_get_discourse_turn_count(gl);
    EXPECT((turns_after - turns_before) == 1,
           "OFF: exactly 1 discourse turn pushed, before=%u after=%u",
           (unsigned)turns_before, (unsigned)turns_after);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.sentences_processed == 0,
           "OFF: sentences_processed=0, got %llu",
           (unsigned long long)stats.sentences_processed);
    EXPECT(stats.multi_sentence_inputs == 0,
           "OFF: multi_sentence_inputs=0, got %llu",
           (unsigned long long)stats.multi_sentence_inputs);

    grounded_language_destroy(gl);
}

static void test_enabled_two_sentences(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_sentence_segmentation_enabled(gl, true);
    EXPECT(grounded_language_get_sentence_segmentation_enabled(gl), "ON");

    register_word(gl, "cat", 5);
    register_word(gl, "happy", 9);

    uint8_t turns_before = grounded_language_get_discourse_turn_count(gl);

    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, "the cat sat. it was happy.", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    /* The discourse buffer is a ring of capacity GL_DISCOURSE_MAX_TURNS
     * (8). Both turns should fit without eviction since we started
     * empty, so the count should advance by 2. Compare against
     * before-snapshot to stay robust if the buffer ever wraps in
     * unrelated tests. */
    uint8_t turns_after = grounded_language_get_discourse_turn_count(gl);
    EXPECT((turns_after - turns_before) == 2,
           "ON: exactly 2 discourse turns pushed, before=%u after=%u",
           (unsigned)turns_before, (unsigned)turns_after);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.sentences_processed == 2,
           "ON: sentences_processed==2, got %llu",
           (unsigned long long)stats.sentences_processed);
    EXPECT(stats.multi_sentence_inputs == 1,
           "ON: multi_sentence_inputs==1, got %llu",
           (unsigned long long)stats.multi_sentence_inputs);

    grounded_language_destroy(gl);
}

static void test_enabled_single_sentence(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_sentence_segmentation_enabled(gl, true);

    register_word(gl, "dog", 13);

    uint8_t turns_before = grounded_language_get_discourse_turn_count(gl);

    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, "the dog ran.", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    uint8_t turns_after = grounded_language_get_discourse_turn_count(gl);
    EXPECT((turns_after - turns_before) == 1,
           "ON+single: exactly 1 turn, before=%u after=%u",
           (unsigned)turns_before, (unsigned)turns_after);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.sentences_processed == 1,
           "ON+single: sentences_processed==1, got %llu",
           (unsigned long long)stats.sentences_processed);
    EXPECT(stats.multi_sentence_inputs == 0,
           "ON+single: multi_sentence_inputs==0, got %llu",
           (unsigned long long)stats.multi_sentence_inputs);

    grounded_language_destroy(gl);
}

static void test_toggle_setter_getter(void)
{
    /* NULL-safe: setter must not crash, getter returns false. */
    grounded_language_set_sentence_segmentation_enabled(NULL, true);
    EXPECT(!grounded_language_get_sentence_segmentation_enabled(NULL),
           "NULL gl getter returns false");

    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    EXPECT(!grounded_language_get_sentence_segmentation_enabled(gl),
           "default OFF on fresh instance");
    grounded_language_set_sentence_segmentation_enabled(gl, true);
    EXPECT(grounded_language_get_sentence_segmentation_enabled(gl),
           "set true → true");
    grounded_language_set_sentence_segmentation_enabled(gl, false);
    EXPECT(!grounded_language_get_sentence_segmentation_enabled(gl),
           "set false → false");

    grounded_language_destroy(gl);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_sentence_segmentation (TB-6) ===\n");
    test_default_off_single_turn();
    test_enabled_two_sentences();
    test_enabled_single_sentence();
    test_toggle_setter_getter();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
