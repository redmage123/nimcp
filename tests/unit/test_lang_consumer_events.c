/**
 * @file test_lang_consumer_events.c
 * @brief Audit-2 follow-up — verify TB-9 SPEECH_ACT and TB-10 TOPIC_SHIFT
 *        events fire to subscribers when their respective detectors trip.
 *
 * Coverage:
 *   1. test_speech_act_event_fires:
 *      Enable TB-9, comprehend a question, verify a SPEECH_ACT event
 *      lands on the subscriber with the QUESTION label.
 *   2. test_topic_shift_event_fires:
 *      Enable TB-10, push enough turns to cross the min_turns gate, then
 *      comprehend a topically-distinct utterance — verify TOPIC_SHIFT
 *      event lands.
 *   3. test_default_off_events_silent:
 *      Both flags OFF — the subscriber must never see the new event types.
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
    int speech_act_count;
    int topic_shift_count;
    gl_speech_act_t last_speech_act;
    float last_topic_similarity;
} ev_counter_t;

static int counter_cb(void* ctx, const gl_event_t* ev) {
    ev_counter_t* c = (ev_counter_t*)ctx;
    if (ev->type == GL_EVENT_SPEECH_ACT) {
        c->speech_act_count++;
        c->last_speech_act = ev->speech_act;
    } else if (ev->type == GL_EVENT_TOPIC_SHIFT) {
        c->topic_shift_count++;
        c->last_topic_similarity = ev->topic_similarity;
    }
    return 0;
}

static void test_speech_act_event_fires(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    grounded_language_set_speech_act_classification_enabled(gl, true);

    ev_counter_t c = {0};
    int rc = grounded_language_subscribe(gl, counter_cb, &c);
    EXPECT(rc == 0, "subscribe rc=%d", rc);

    gl_comprehension_result_t r;
    memset(&r, 0, sizeof(r));
    grounded_language_comprehend(gl, "what is your name?", &r);
    gl_comprehension_result_cleanup(&r);

    EXPECT(c.speech_act_count >= 1, "speech_act event fired (got %d)", c.speech_act_count);
    EXPECT(c.last_speech_act == GL_SPEECH_ACT_QUESTION,
           "speech_act label was QUESTION, got %d", c.last_speech_act);

    grounded_language_unsubscribe(gl, &c);
    grounded_language_destroy(gl);
}

static void seed_word(grounded_language_t* gl, const char* w, int seed) {
    /* Ground the word with a deterministic feature vector so comprehend
     * produces non-zero semantic vectors. Without this the topic-shift
     * detector skips because semantic_vec_is_zero. */
    float feat[256];
    for (int i = 0; i < 256; i++) {
        feat[i] = ((float)((seed * 31 + i * 17) & 0xff)) / 255.0f - 0.5f;
    }
    (void)grounded_language_fast_map(gl, w, feat, 256, /*OBJECT*/ 1);
}

static void test_topic_shift_event_fires(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    /* Seed two distinct topic clusters with very different feature vectors
     * so comprehend's semantic vectors land far apart (low cosine). */
    seed_word(gl, "dog",     11);
    seed_word(gl, "ran",     12);
    seed_word(gl, "park",    13);
    seed_word(gl, "quantum", 211);
    seed_word(gl, "physics", 212);
    seed_word(gl, "hard",    213);

    grounded_language_set_topic_shift_enabled(gl, true);
    grounded_language_set_topic_shift_threshold(gl, 0.9f);  /* aggressive */
    grounded_language_set_topic_shift_min_turns(gl, 2);

    ev_counter_t c = {0};
    grounded_language_subscribe(gl, counter_cb, &c);

    /* Build up a "dogs" topic context. */
    gl_comprehension_result_t r;
    for (int i = 0; i < 4; i++) {
        memset(&r, 0, sizeof(r));
        grounded_language_comprehend(gl, "the dog ran in the park", &r);
        gl_comprehension_result_cleanup(&r);
    }

    int before = c.topic_shift_count;
    /* Now an unrelated utterance — should trip the detector. */
    memset(&r, 0, sizeof(r));
    grounded_language_comprehend(gl, "quantum physics is hard", &r);
    gl_comprehension_result_cleanup(&r);

    EXPECT(c.topic_shift_count > before,
           "topic_shift event fired (before=%d after=%d, score=%.4f)",
           before, c.topic_shift_count, c.last_topic_similarity);

    grounded_language_unsubscribe(gl, &c);
    grounded_language_destroy(gl);
}

static void test_default_off_events_silent(void)
{
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    /* Both flags default OFF. */
    ev_counter_t c = {0};
    grounded_language_subscribe(gl, counter_cb, &c);

    gl_comprehension_result_t r;
    for (int i = 0; i < 4; i++) {
        memset(&r, 0, sizeof(r));
        grounded_language_comprehend(gl, "the dog ran", &r);
        gl_comprehension_result_cleanup(&r);
    }
    memset(&r, 0, sizeof(r));
    grounded_language_comprehend(gl, "quantum physics is hard?", &r);
    gl_comprehension_result_cleanup(&r);

    EXPECT(c.speech_act_count == 0,
           "no speech_act events when OFF (got %d)", c.speech_act_count);
    EXPECT(c.topic_shift_count == 0,
           "no topic_shift events when OFF (got %d)", c.topic_shift_count);

    grounded_language_unsubscribe(gl, &c);
    grounded_language_destroy(gl);
}

int main(void)
{
    fprintf(stderr, "=== test_lang_consumer_events (audit-2 follow-up) ===\n");
    test_speech_act_event_fires();
    test_topic_shift_event_fires();
    test_default_off_events_silent();

    if (g_failures == 0) {
        fprintf(stderr, "ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
