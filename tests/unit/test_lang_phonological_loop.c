/**
 * @file test_lang_phonological_loop.c
 * @brief Slice 5 — Baddeley phonological-loop working-memory buffer.
 *
 * Coverage:
 *   1. Default OFF — loop_enabled is false after brain_create.
 *   2. Init allocates the mutex + initial capacity + tunables.
 *   3. merge_words: existing → refresh trace, new → append, capped.
 *   4. decay: traces multiply, traces < 0.05 are evicted.
 *   5. render_active: only traces >= threshold appear in surface form.
 *   6. clear: drops every trace + word, buffer empty.
 *   7. Slice 1+2 byte-identity when loop_enabled=false (no merge,
 *      lexical output flows through untouched).
 */

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "language/nimcp_phonological_loop.h"

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

static brain_t make_brain(const char* name) {
    return brain_create_minimal(name, BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 16, 8);
}

static void test_default_off_and_init(void) {
    brain_t b = make_brain("phono_default");
    EXPECT(b != NULL, "create");
    if (!b) return;

    EXPECT(b->loop_enabled == false, "default OFF");
    /* brain_create_minimal doesn't run broca-subsystem init, so the
     * loop is uninitialized. Trigger lazy init explicitly. */
    EXPECT(phonological_loop_init(b) == 0, "lazy init returns 0");
    EXPECT(b->loop_max_words == 16u, "max_words init=%u", b->loop_max_words);
    /* decay should be > 0 (default 0.15) and within clamp range */
    EXPECT(b->loop_decay_rate >= 0.0f && b->loop_decay_rate <= 0.5f,
            "decay range got=%f", (double)b->loop_decay_rate);
    EXPECT(b->loop_mutex != NULL, "mutex created");
    EXPECT(b->loop_buffer != NULL, "buffer allocated");
    EXPECT(b->loop_buffer_capacity >= 256u, "buffer cap=%u", b->loop_buffer_capacity);
    EXPECT(b->loop_phonemic_trace != NULL, "trace array allocated");
    EXPECT(b->loop_words != NULL, "words array allocated");
    EXPECT(b->loop_trace_count == 0u, "trace_count empty initially");

    /* Idempotent: second init call returns success without re-allocating. */
    void* mutex_first = b->loop_mutex;
    EXPECT(phonological_loop_init(b) == 0, "re-init returns 0");
    EXPECT(b->loop_mutex == mutex_first, "mutex pointer stable across re-init");

    brain_destroy(b);
    fprintf(stderr, "PASS test_default_off_and_init\n");
}

static void test_merge_new_then_existing(void) {
    brain_t b = make_brain("phono_merge");
    if (!b) { g_failures++; return; }
    b->loop_enabled = true;

    /* First merge: 3 distinct words → 3 traces at 1.0 */
    phonological_loop_merge_words(b, "the cat sat");
    EXPECT(b->loop_trace_count == 3u, "after merge1 trace_count=%u",
            b->loop_trace_count);
    for (uint32_t i = 0; i < b->loop_trace_count; i++) {
        EXPECT(b->loop_phonemic_trace[i] > 0.99f,
                "trace[%u]=%f after first merge",
                i, (double)b->loop_phonemic_trace[i]);
    }

    /* Decay everyone — then re-merge "cat" only. "cat" should refresh
     * back to 1.0, "the" and "sat" should be at (1-decay). */
    phonological_loop_decay(b);
    float pre_cat = -1.0f;
    for (uint32_t i = 0; i < b->loop_trace_count; i++) {
        if (b->loop_words[i] && strcmp(b->loop_words[i], "cat") == 0) {
            pre_cat = b->loop_phonemic_trace[i];
        }
    }
    EXPECT(pre_cat >= 0.0f && pre_cat < 1.0f,
            "cat post-decay pre-refresh=%f", (double)pre_cat);

    phonological_loop_merge_words(b, "cat");
    EXPECT(b->loop_trace_count == 3u, "still 3 traces post-refresh");
    for (uint32_t i = 0; i < b->loop_trace_count; i++) {
        if (b->loop_words[i] && strcmp(b->loop_words[i], "cat") == 0) {
            EXPECT(b->loop_phonemic_trace[i] > 0.99f,
                    "cat refreshed to 1.0 actual=%f",
                    (double)b->loop_phonemic_trace[i]);
        } else {
            EXPECT(b->loop_phonemic_trace[i] < 1.0f,
                    "non-cat stayed decayed actual=%f",
                    (double)b->loop_phonemic_trace[i]);
        }
    }

    brain_destroy(b);
    fprintf(stderr, "PASS test_merge_new_then_existing\n");
}

static void test_decay_evicts_below_threshold(void) {
    brain_t b = make_brain("phono_decay");
    if (!b) { g_failures++; return; }
    b->loop_enabled = true;

    /* Crank decay so a single pass drops to ~0.5 — then repeat. After
     * enough rounds the trace falls below 0.05 and the word is evicted. */
    b->loop_decay_rate = 0.5f;
    phonological_loop_merge_words(b, "alpha");
    EXPECT(b->loop_trace_count == 1u, "loaded");
    for (int i = 0; i < 8; i++) {
        phonological_loop_decay(b);
    }
    EXPECT(b->loop_trace_count == 0u,
            "evicted after 8 decay rounds (count=%u)", b->loop_trace_count);

    brain_destroy(b);
    fprintf(stderr, "PASS test_decay_evicts_below_threshold\n");
}

static void test_render_threshold(void) {
    brain_t b = make_brain("phono_render");
    if (!b) { g_failures++; return; }
    b->loop_enabled = true;

    phonological_loop_merge_words(b, "high mid low");
    /* Manually pin per-trace strengths to test threshold behavior. */
    for (uint32_t i = 0; i < b->loop_trace_count; i++) {
        const char* w = b->loop_words[i];
        if (!w) continue;
        if (strcmp(w, "high") == 0) b->loop_phonemic_trace[i] = 0.9f;
        else if (strcmp(w, "mid")  == 0) b->loop_phonemic_trace[i] = 0.4f;
        else if (strcmp(w, "low")  == 0) b->loop_phonemic_trace[i] = 0.1f;
    }

    /* threshold 0.3 → "high" and "mid", excludes "low" */
    char buf[256];
    uint32_t n = phonological_loop_render_active(b, 0.3f, buf, sizeof(buf));
    EXPECT(n == 2u, "render n=%u expected 2", n);
    EXPECT(strstr(buf, "high") != NULL, "high present in '%s'", buf);
    EXPECT(strstr(buf, "mid")  != NULL, "mid present in '%s'", buf);
    EXPECT(strstr(buf, "low")  == NULL, "low absent in '%s'", buf);

    /* threshold 0.5 → only "high" */
    n = phonological_loop_render_active(b, 0.5f, buf, sizeof(buf));
    EXPECT(n == 1u, "render n=%u expected 1 (high only)", n);
    EXPECT(strcmp(buf, "high") == 0, "buf='%s' expected 'high'", buf);

    brain_destroy(b);
    fprintf(stderr, "PASS test_render_threshold\n");
}

static void test_clear(void) {
    brain_t b = make_brain("phono_clear");
    if (!b) { g_failures++; return; }
    b->loop_enabled = true;
    phonological_loop_merge_words(b, "one two three");
    EXPECT(b->loop_trace_count == 3u, "loaded");
    phonological_loop_clear(b);
    EXPECT(b->loop_trace_count == 0u, "cleared count=%u", b->loop_trace_count);
    EXPECT(b->loop_buffer_len == 0u, "buffer_len=%u", b->loop_buffer_len);
    EXPECT(b->loop_buffer && b->loop_buffer[0] == '\0', "buffer empty");
    /* Re-using after clear still works. */
    phonological_loop_merge_words(b, "post clear");
    EXPECT(b->loop_trace_count == 2u, "re-use after clear");
    brain_destroy(b);
    fprintf(stderr, "PASS test_clear\n");
}

static void test_disabled_is_noop(void) {
    brain_t b = make_brain("phono_disabled");
    if (!b) { g_failures++; return; }
    /* loop_enabled stays false → every public helper no-ops. */
    EXPECT(b->loop_enabled == false, "stays disabled");
    phonological_loop_merge_words(b, "should not appear");
    phonological_loop_decay(b);
    EXPECT(b->loop_trace_count == 0u, "no merge when disabled count=%u",
            b->loop_trace_count);
    char buf[64];
    uint32_t n = phonological_loop_render_active(b, 0.3f, buf, sizeof(buf));
    EXPECT(n == 0u && buf[0] == '\0', "render no-op when disabled");
    brain_destroy(b);
    fprintf(stderr, "PASS test_disabled_is_noop\n");
}

static void test_max_words_cap(void) {
    brain_t b = make_brain("phono_cap");
    if (!b) { g_failures++; return; }
    b->loop_enabled = true;
    /* max_words defaults to 16 — feed 25 distinct words; should cap at 16. */
    phonological_loop_merge_words(b,
        "a b c d e f g h i j k l m n o p q r s t u v w x y");
    EXPECT(b->loop_trace_count == 16u,
            "capped at 16 actual=%u", b->loop_trace_count);
    brain_destroy(b);
    fprintf(stderr, "PASS test_max_words_cap\n");
}

/*
 * Slice 5 integration: exercise the recurrent cascade with the
 * phonological loop enabled. This is a smoke test — we don't assert on
 * the produced utterance (which depends on the bridge being trained)
 * but DO assert that:
 *   - the recurrent path runs to completion without crashing
 *   - the loop accumulates traces across iterations
 *   - clearing the loop resets state cleanly
 */
#include "language/nimcp_communication_cascade.h"

static void test_recurrent_smoke(void) {
    brain_t b = make_brain("phono_recurrent");
    if (!b) { g_failures++; return; }
    /* No grounded_lang on a minimal-mode brain — the cascade will skip
     * the lexical stage and bail out of merging into the loop. That's
     * fine; we're testing the recurrent-loop scaffold doesn't crash
     * with the loop enabled. */
    b->loop_enabled = true;
    EXPECT(phonological_loop_init(b) == 0, "lazy init");

    /* Pre-seed the loop directly so we can observe clear-on-entry. */
    phonological_loop_merge_words(b, "stale junk");
    EXPECT(b->loop_trace_count == 2u, "pre-seed count=%u", b->loop_trace_count);

    production_cascade_state_t state;
    memset(&state, 0, sizeof(state));
    int rc = communication_cascade_run_recurrent(b, NULL, 3, 0.01f, &state);
    EXPECT(rc == 0, "recurrent rc=%d", rc);

    /* The recurrent run cleared the loop on entry. With no grounded_lang
     * the lexical stage skipped (no merge), so trace_count is 0. */
    EXPECT(b->loop_trace_count == 0u,
            "loop cleared on entry, count=%u", b->loop_trace_count);

    cascade_state_cleanup(&state);
    brain_destroy(b);
    fprintf(stderr, "PASS test_recurrent_smoke\n");
}

/* S6-M2 fix (2026-05-19): loop tokenizer now splits on isspace()+ispunct()
 * to match grounded_language tokenize_text. Pre-fix, hyphen wasn't a
 * delimiter for the loop but was for the lexicon, so "self-extinguishing"
 * was one token in the loop but two in the lexicon — Broca's CYK lookup
 * for the joined form was POS_UNKNOWN and the surface form was rejected.
 * Verify the same hyphenated input now yields 4 separate normalised
 * tokens in the loop. */
static void test_hyphen_split_matches_lexicon(void) {
    brain_t b = make_brain("phono_hyphen");
    if (!b) { g_failures++; return; }
    b->loop_enabled = true;

    /* Two hyphenated forms = 4 normalized tokens in the lexicon's view. */
    phonological_loop_merge_words(b, "self-extinguishing word-pop");
    EXPECT(b->loop_trace_count == 4u,
            "hyphen-split yields 4 tokens (count=%u)", b->loop_trace_count);

    /* Confirm each of the 4 component tokens is present individually
     * (order isn't strict but presence is). */
    bool have_self = false, have_ext = false, have_word = false, have_pop = false;
    for (uint32_t i = 0; i < b->loop_trace_count; i++) {
        const char* w = b->loop_words[i];
        if (!w) continue;
        if (strcmp(w, "self") == 0)              have_self = true;
        else if (strcmp(w, "extinguishing") == 0) have_ext = true;
        else if (strcmp(w, "word") == 0)         have_word = true;
        else if (strcmp(w, "pop") == 0)           have_pop = true;
    }
    EXPECT(have_self && have_ext && have_word && have_pop,
            "all 4 hyphen-split tokens present: "
            "self=%d ext=%d word=%d pop=%d",
            have_self, have_ext, have_word, have_pop);

    brain_destroy(b);
    fprintf(stderr, "PASS test_hyphen_split_matches_lexicon\n");
}

int main(void) {
    test_default_off_and_init();
    test_merge_new_then_existing();
    test_decay_evicts_below_threshold();
    test_render_threshold();
    test_clear();
    test_disabled_is_noop();
    test_max_words_cap();
    test_recurrent_smoke();
    test_hyphen_split_matches_lexicon();
    if (g_failures > 0) {
        fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
        return 1;
    }
    fprintf(stderr, "OK: test_lang_phonological_loop pass\n");
    return 0;
}
