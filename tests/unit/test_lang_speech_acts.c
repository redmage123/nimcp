/**
 * @file test_lang_speech_acts.c
 * @brief TB-9 — verify speech-act intent classification on comprehend.
 *
 * Coverage:
 *   1. test_default_off:
 *      Default toggle is OFF — every comprehend pass leaves
 *      result->speech_act == GL_SPEECH_ACT_UNKNOWN regardless of input,
 *      and none of the speech_act_* stats counters advance.
 *
 *   2. test_question:
 *      "what is your name?" → QUESTION, stats.speech_act_questions++.
 *      Also covers wh-word + auxiliary-as-first-token paths via two
 *      additional inputs.
 *
 *   3. test_imperative:
 *      "go fetch the ball" → IMPERATIVE (bare-verb cue),
 *      stats.speech_act_imperatives++.
 *
 *   4. test_greeting:
 *      "hello there" → GREETING, stats.speech_act_greetings++.
 *
 *   5. test_assertion:
 *      "the cat is happy" → ASSERTION (default fallback),
 *      stats.speech_act_assertions++.
 *
 *   6. test_exclamation:
 *      "amazing!" → EXCLAMATION, stats.speech_act_exclamations++.
 *
 *   7. test_setter_null_safe:
 *      Setter / getter handle NULL gl gracefully.
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

/* Run one comprehend pass and return the classified speech_act. The
 * result struct is fully cleaned up before return so the caller can
 * focus on the classification + stats. */
static gl_speech_act_t comprehend_get_act(grounded_language_t* gl,
                                            const char* text) {
    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    int rc = grounded_language_comprehend(gl, text, &r);
    if (rc != 0) {
        gl_comprehension_result_cleanup(&r);
        return GL_SPEECH_ACT_UNKNOWN;
    }
    gl_speech_act_t act = r.speech_act;
    gl_comprehension_result_cleanup(&r);
    return act;
}

static void test_default_off(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    EXPECT(!grounded_language_get_speech_act_classification_enabled(gl),
           "default OFF");

    /* A spread of input shapes — none should classify when OFF. */
    const char* inputs[] = {
        "what is your name?",
        "go fetch the ball",
        "hello there",
        "the cat is happy",
        "amazing!",
        NULL
    };
    for (int i = 0; inputs[i]; i++) {
        gl_speech_act_t act = comprehend_get_act(gl, inputs[i]);
        EXPECT(act == GL_SPEECH_ACT_UNKNOWN,
               "OFF: \"%s\" → %d (expected UNKNOWN=%d)",
               inputs[i], (int)act, (int)GL_SPEECH_ACT_UNKNOWN);
    }

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.speech_act_assertions == 0
            && stats.speech_act_questions == 0
            && stats.speech_act_imperatives == 0
            && stats.speech_act_greetings == 0
            && stats.speech_act_exclamations == 0,
           "OFF: all speech_act stats stay 0 (a=%llu q=%llu i=%llu g=%llu e=%llu)",
           (unsigned long long)stats.speech_act_assertions,
           (unsigned long long)stats.speech_act_questions,
           (unsigned long long)stats.speech_act_imperatives,
           (unsigned long long)stats.speech_act_greetings,
           (unsigned long long)stats.speech_act_exclamations);

    grounded_language_destroy(gl);
}

static void test_question(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_speech_act_classification_enabled(gl, true);
    EXPECT(grounded_language_get_speech_act_classification_enabled(gl),
           "ON");

    /* (a) trailing '?' wins. */
    EXPECT(comprehend_get_act(gl, "what is your name?") == GL_SPEECH_ACT_QUESTION,
           "trailing ? classifies as QUESTION");

    /* (b) wh-word as first token, no '?'. */
    EXPECT(comprehend_get_act(gl, "where did you go") == GL_SPEECH_ACT_QUESTION,
           "wh-word first token classifies as QUESTION");

    /* (c) auxiliary as first token, no '?'. */
    EXPECT(comprehend_get_act(gl, "is the cat hungry") == GL_SPEECH_ACT_QUESTION,
           "auxiliary first token classifies as QUESTION");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.speech_act_questions == 3,
           "questions counter == 3, got %llu",
           (unsigned long long)stats.speech_act_questions);

    grounded_language_destroy(gl);
}

static void test_imperative(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_speech_act_classification_enabled(gl, true);

    /* Bare-verb cue at front. */
    EXPECT(comprehend_get_act(gl, "go fetch the ball") == GL_SPEECH_ACT_IMPERATIVE,
           "go-fetch classifies as IMPERATIVE");

    /* "stop the noise" — single-cue first word, multi-token. */
    EXPECT(comprehend_get_act(gl, "stop the noise") == GL_SPEECH_ACT_IMPERATIVE,
           "stop-the-noise classifies as IMPERATIVE");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.speech_act_imperatives == 2,
           "imperatives counter == 2, got %llu",
           (unsigned long long)stats.speech_act_imperatives);

    grounded_language_destroy(gl);
}

static void test_greeting(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_speech_act_classification_enabled(gl, true);

    EXPECT(comprehend_get_act(gl, "hello there") == GL_SPEECH_ACT_GREETING,
           "hello-there classifies as GREETING");

    /* "good morning friend" — second-token cue. */
    EXPECT(comprehend_get_act(gl, "good morning friend") == GL_SPEECH_ACT_GREETING,
           "good-morning classifies as GREETING via 2nd token");

    /* Greeting with '?' — greeting still wins (rule (1) is checked first). */
    EXPECT(comprehend_get_act(gl, "hi there?") == GL_SPEECH_ACT_GREETING,
           "hi-there? classifies as GREETING (cue beats trailing ?)");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.speech_act_greetings == 3,
           "greetings counter == 3, got %llu",
           (unsigned long long)stats.speech_act_greetings);

    grounded_language_destroy(gl);
}

static void test_assertion(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_speech_act_classification_enabled(gl, true);

    /* Default declarative — no '?', no '!', not wh/aux/imperative/greeting. */
    EXPECT(comprehend_get_act(gl, "the cat is happy") == GL_SPEECH_ACT_ASSERTION,
           "the-cat-is-happy classifies as ASSERTION");

    EXPECT(comprehend_get_act(gl, "snow is white") == GL_SPEECH_ACT_ASSERTION,
           "snow-is-white classifies as ASSERTION");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.speech_act_assertions == 2,
           "assertions counter == 2, got %llu",
           (unsigned long long)stats.speech_act_assertions);

    grounded_language_destroy(gl);
}

static void test_exclamation(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_speech_act_classification_enabled(gl, true);

    /* Single-token exclamation — no imperative cue, single content word. */
    EXPECT(comprehend_get_act(gl, "amazing!") == GL_SPEECH_ACT_EXCLAMATION,
           "amazing! classifies as EXCLAMATION");

    /* First-person pronoun + '!' → declarative-style exclamation. */
    EXPECT(comprehend_get_act(gl, "i won the game!") == GL_SPEECH_ACT_EXCLAMATION,
           "i-won-the-game! classifies as EXCLAMATION (first-person pronoun)");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.speech_act_exclamations == 2,
           "exclamations counter == 2, got %llu",
           (unsigned long long)stats.speech_act_exclamations);

    grounded_language_destroy(gl);
}

static void test_setter_null_safe(void)
{
    /* Setter / getter must handle NULL gracefully (campaign convention). */
    grounded_language_set_speech_act_classification_enabled(NULL, true);
    EXPECT(!grounded_language_get_speech_act_classification_enabled(NULL),
           "NULL gl getter returns false");
}

/* Regression: post-campaign audit found that empty / whitespace-only /
 * punctuation-only inputs fell through to the ASSERTION default and
 * bumped speech_act_assertions, which is dishonest — there's no intent
 * to assert when there are no content words. The fix returns UNKNOWN for
 * those cases and skips the per-class counters. */
static void test_empty_input_returns_unknown(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_speech_act_classification_enabled(gl, true);

    const char* empties[] = {
        "",         /* truly empty */
        "   ",      /* whitespace-only */
        "\t\n",     /* whitespace-only (mixed) */
        ".",        /* single terminator */
        "?!.",      /* terminator-only soup */
        NULL
    };
    for (int i = 0; empties[i]; i++) {
        gl_speech_act_t act = comprehend_get_act(gl, empties[i]);
        EXPECT(act == GL_SPEECH_ACT_UNKNOWN,
               "empty input %d (\"%s\") classifies as UNKNOWN, got %d",
               i, empties[i], (int)act);
    }

    /* Counters: none of the per-class counters should have advanced. */
    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.speech_act_assertions == 0
            && stats.speech_act_questions == 0
            && stats.speech_act_imperatives == 0
            && stats.speech_act_greetings == 0
            && stats.speech_act_exclamations == 0,
           "empty inputs: per-class counters stay 0 "
           "(a=%llu q=%llu i=%llu g=%llu e=%llu)",
           (unsigned long long)stats.speech_act_assertions,
           (unsigned long long)stats.speech_act_questions,
           (unsigned long long)stats.speech_act_imperatives,
           (unsigned long long)stats.speech_act_greetings,
           (unsigned long long)stats.speech_act_exclamations);

    grounded_language_destroy(gl);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_speech_acts (TB-9) ===\n");
    test_default_off();
    test_question();
    test_imperative();
    test_greeting();
    test_assertion();
    test_exclamation();
    test_setter_null_safe();
    test_empty_input_returns_unknown();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
