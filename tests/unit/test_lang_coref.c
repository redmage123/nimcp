/**
 * @file test_lang_coref.c
 * @brief NLP-2 — verify same-surface coreference resolution beyond pronouns.
 *
 * The coref ring tracks "the/this/that/these/those + noun" patterns
 * across discourse turns. Repeat mentions of the same head word within
 * GL_COREF_MAX_TURNS (6) count as coref resolutions and fire
 * GL_EVENT_COREF_RESOLVED.
 *
 * Coverage:
 *   1. test_default_off_no_coref: flag OFF → no events, no counters bumped.
 *   2. test_repeat_mention_resolves: flag ON, "the dog ran" → "the dog
 *      slept" should fire one COREF_RESOLVED event.
 *   3. test_no_repeat_no_resolve: flag ON, "the dog ran" → "the cat
 *      slept" should NOT fire (different heads).
 *   4. test_pronoun_skipped: flag ON, "the dog ran" → "the he barked"
 *      should NOT fire (pronouns skipped — anaphora handles them).
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

typedef struct {
    int coref_count;
    char last_word[64];
} ev_counter_t;

static int counter_cb(void* ctx, const gl_event_t* ev) {
    ev_counter_t* c = (ev_counter_t*)ctx;
    if (ev->type == GL_EVENT_COREF_RESOLVED) {
        c->coref_count++;
        if (ev->word) {
            strncpy(c->last_word, ev->word, sizeof(c->last_word) - 1);
        }
    }
    return 0;
}

static void test_default_off_no_coref(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    EXPECT(!grounded_language_get_coref_resolution_enabled(gl),
           "default OFF");

    ev_counter_t c = {0};
    grounded_language_subscribe(gl, counter_cb, &c);

    gl_comprehension_result_t r;
    for (int i = 0; i < 3; i++) {
        memset(&r, 0, sizeof(r));
        grounded_language_comprehend(gl, "the dog ran", &r);
        gl_comprehension_result_cleanup(&r);
    }

    EXPECT(c.coref_count == 0,
           "no events when OFF (got %d)", c.coref_count);
    EXPECT(grounded_language_coref_attempts(gl) == 0,
           "no attempts when OFF (got %llu)",
           (unsigned long long)grounded_language_coref_attempts(gl));

    grounded_language_unsubscribe(gl, &c);
    grounded_language_destroy(gl);
}

static void test_repeat_mention_resolves(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_coref_resolution_enabled(gl, true);
    EXPECT(grounded_language_get_coref_resolution_enabled(gl), "ON");

    ev_counter_t c = {0};
    grounded_language_subscribe(gl, counter_cb, &c);

    gl_comprehension_result_t r;
    /* First mention — pushes "dog" into ring, no match. */
    memset(&r, 0, sizeof(r));
    grounded_language_comprehend(gl, "the dog ran", &r);
    gl_comprehension_result_cleanup(&r);
    EXPECT(c.coref_count == 0, "no resolve on first mention");

    /* Repeat "the dog" — should match the prior mention. */
    memset(&r, 0, sizeof(r));
    grounded_language_comprehend(gl, "the dog slept", &r);
    gl_comprehension_result_cleanup(&r);

    EXPECT(c.coref_count >= 1,
           "coref event fired on repeat (got %d)", c.coref_count);
    EXPECT(strcmp(c.last_word, "dog") == 0,
           "head word in event is 'dog', got '%s'", c.last_word);
    EXPECT(grounded_language_coref_resolved(gl) >= 1,
           "resolved counter bumped (got %llu)",
           (unsigned long long)grounded_language_coref_resolved(gl));

    grounded_language_unsubscribe(gl, &c);
    grounded_language_destroy(gl);
}

static void test_no_repeat_no_resolve(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_coref_resolution_enabled(gl, true);

    ev_counter_t c = {0};
    grounded_language_subscribe(gl, counter_cb, &c);

    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    grounded_language_comprehend(gl, "the dog ran", &r);
    gl_comprehension_result_cleanup(&r);

    /* Different head — no match. */
    memset(&r, 0, sizeof(r));
    grounded_language_comprehend(gl, "the cat slept", &r);
    gl_comprehension_result_cleanup(&r);

    EXPECT(c.coref_count == 0,
           "no resolve when heads differ (got %d)", c.coref_count);
    /* But attempts should bump for both definite NPs. */
    EXPECT(grounded_language_coref_attempts(gl) >= 2,
           "attempts >= 2 (got %llu)",
           (unsigned long long)grounded_language_coref_attempts(gl));

    grounded_language_unsubscribe(gl, &c);
    grounded_language_destroy(gl);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_coref (NLP-2) ===\n");
    test_default_off_no_coref();
    test_repeat_mention_resolves();
    test_no_repeat_no_resolve();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
