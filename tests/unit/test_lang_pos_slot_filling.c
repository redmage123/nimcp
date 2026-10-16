/**
 * @file test_lang_pos_slot_filling.c
 * @brief UNIT — POS-aware slot-filling in grounded_language_produce.
 *
 * Tier 1 Step C (2026-05-23): the produce emit loop adds a soft,
 * stage-scaled POS-transition bias on top of cosine + bigram scoring.
 * Given the previously emitted word's class it nudges toward the class
 * that grammatically follows (start->NOUN, NOUN->VERB, VERB->NOUN, ...),
 * weighted by class_confidence. The effect turns a relevance-only word
 * salad into a NOUN-VERB-NOUN (SVO) skeleton.
 *
 * What this test guards:
 *   1. At a stage where the bias is ON (stage 2+), with cosine TIED
 *      across a noun, a verb, and a second noun (disjoint one-hot dims
 *      and no phrase-table entries), the verb is NOT emitted first —
 *      position 0 is a NOUN (subject before predicate). This is the core
 *      grammatical win.
 *   2. Graceful degradation: words with UNKNOWN class (too short for the
 *      morphology classifier, never seen in context) still produce a
 *      valid non-empty result at stage 2 — the bias applies nothing and
 *      the loop falls back to pure cosine + bigram ordering.
 *
 * The three content words rely on the morphology classifier
 * (gl_morph_pos_hint): "creation"/"motion" -> NOUN (-tion), "running" ->
 * VERB (-ing). fast_map stamps those classes at confidence 0.4 on word
 * creation; we never call learn_from_text so the positional heuristic
 * can't overwrite them.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_pos_slot_filling.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_pos_slot_filling
 */

#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"

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

#define SEMANTIC_DIM 64u

/* Ground a word with a disjoint one-hot+offset profile (cosine across
 * distinct words ~0). Same trick as test_lang_bigram_rerank. */
static uint64_t ground_word(grounded_language_t* gl, const char* word,
                            uint32_t seed_idx) {
    float feats[SEMANTIC_DIM] = {0};
    if (seed_idx < SEMANTIC_DIM) feats[seed_idx] = 1.0f;
    if ((seed_idx + 7u) < SEMANTIC_DIM) feats[seed_idx + 7u] = 0.3f;
    return grounded_language_fast_map(gl, word, feats, SEMANTIC_DIM, 1u);
}

/* Split result text into up to max_w words; returns count. Pointers index
 * into the caller-owned mutable copy `work`. */
static uint32_t split_words(char* work, const char** out, uint32_t max_w) {
    uint32_t n = 0;
    char* p = work;
    char* tok = NULL;
    bool in = false;
    for (; *p; p++) {
        if (*p == ' ') { *p = '\0'; in = false; }
        else if (!in) {
            in = true; tok = p;
            if (n < max_w) out[n] = tok;
            n++;
        }
    }
    return n;
}

/* Return the morphology-inferred class for a freshly grounded word. */
static gl_word_class_t class_of(grounded_language_t* gl, const char* w) {
    const gl_lexicon_entry_t* e = grounded_language_lookup(gl, w);
    return e ? e->learned_class : GL_CLASS_UNKNOWN;
}

/* ====================================================================== */
/* TEST 1: POS bias puts the subject (noun) before the verb. */
static void test_noun_precedes_verb_when_bias_on(void) {
    semantic_memory_system_t* sm = semantic_memory_create();
    grounded_language_t* gl = grounded_language_create(SEMANTIC_DIM, sm);
    EXPECT(gl != NULL, "create gl");
    if (!gl) { semantic_memory_destroy(sm); return; }

    /* Stage 2 turns the POS bias on (weight 0.08) and drops the floor to
     * 0.15 so the loop emits multiple words. */
    grounded_language_set_current_stage_int(gl, 2);

    EXPECT(ground_word(gl, "creation", 4u)  != 0, "ground creation");
    EXPECT(ground_word(gl, "running",  13u) != 0, "ground running");
    EXPECT(ground_word(gl, "motion",   22u) != 0, "ground motion");

    /* Precondition: morphology classified them as we expect. If the
     * classifier ever changes, this test's premise is void — assert it. */
    EXPECT(class_of(gl, "creation") == GL_CLASS_NOUN, "creation is NOUN");
    EXPECT(class_of(gl, "running")  == GL_CLASS_VERB, "running is VERB");
    EXPECT(class_of(gl, "motion")   == GL_CLASS_NOUN, "motion is NOUN");

    /* Intent ties all three at the cosine level (disjoint dims, equal
     * weights). No phrase-table entries, so the only tiebreaker past
     * cosine is the POS-transition bias. */
    float intent[SEMANTIC_DIM] = {0};
    intent[4u]  = 1.0f;  /* creation */
    intent[13u] = 1.0f;  /* running  */
    intent[22u] = 1.0f;  /* motion   */

    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SEMANTIC_DIM,
                                       GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0, "produce rc=%d", rc);
    EXPECT(r.text && r.text[0], "produce emitted text");
    if (rc == 0 && r.text) {
        fprintf(stderr, "  stage-2 output: %s\n", r.text);
        char work[256];
        snprintf(work, sizeof(work), "%s", r.text);
        const char* w[8] = {0};
        uint32_t nw = split_words(work, w, 8);
        EXPECT(nw >= 2, "emitted >=2 words (got %u)", nw);
        if (nw >= 2) {
            /* Step F2 may scaffold a leading determiner ("the creation
             * running ..."), so the subject noun is the first CONTENT word,
             * not necessarily token 0. Skip leading function/pronoun words
             * (their class is FUNCTION/PRONOUN, or "the"/"a"/"is" literally
             * when they aren't yet grounded with a class). */
            uint32_t ci = 0;
            while (ci < nw &&
                   (class_of(gl, w[ci]) == GL_CLASS_FUNCTION ||
                    class_of(gl, w[ci]) == GL_CLASS_PRONOUN ||
                    strcmp(w[ci], "the") == 0 ||
                    strcmp(w[ci], "a") == 0 ||
                    strcmp(w[ci], "is") == 0)) {
                ci++;
            }
            EXPECT(ci < nw, "found a content word among %u tokens", nw);
            if (ci < nw) {
                /* Core assertion: the verb must NOT lead the content words.
                 * With tied cosine + bias, both nouns get the start->NOUN
                 * bonus and the verb does not, so the first content word is
                 * deterministically a noun. */
                EXPECT(strcmp(w[ci], "running") != 0,
                       "verb 'running' must not lead content (got '%s')", r.text);
                EXPECT(class_of(gl, w[ci]) == GL_CLASS_NOUN,
                       "first content word must be a NOUN (got '%s' = class %d)",
                       w[ci], (int)class_of(gl, w[ci]));
            }
        }
    }
    gl_production_result_cleanup(&r);
    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
}

/* ====================================================================== */
/* TEST 2: graceful degradation when POS tags are UNKNOWN. */
static void test_unknown_class_degrades_gracefully(void) {
    semantic_memory_system_t* sm = semantic_memory_create();
    grounded_language_t* gl = grounded_language_create(SEMANTIC_DIM, sm);
    if (!gl) { semantic_memory_destroy(sm); return; }
    grounded_language_set_current_stage_int(gl, 2);

    /* Short, suffix-less words → morphology returns UNKNOWN. The POS bias
     * has nothing to act on; produce must still emit a valid result. */
    EXPECT(ground_word(gl, "rok",  4u)  != 0, "ground rok");
    EXPECT(ground_word(gl, "vex",  13u) != 0, "ground vex");
    EXPECT(ground_word(gl, "zib",  22u) != 0, "ground zib");
    EXPECT(class_of(gl, "rok") == GL_CLASS_UNKNOWN, "rok class UNKNOWN");

    float intent[SEMANTIC_DIM] = {0};
    intent[4u] = 1.0f; intent[13u] = 1.0f; intent[22u] = 1.0f;

    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SEMANTIC_DIM,
                                       GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0, "produce rc=%d (graceful)", rc);
    EXPECT(r.text && r.text[0], "produce emitted text under UNKNOWN tags");
    if (r.text) fprintf(stderr, "  unknown-tag output: %s\n", r.text);
    gl_production_result_cleanup(&r);
    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_pos_slot_filling\n");
    test_noun_precedes_verb_when_bias_on();
    test_unknown_class_degrades_gracefully();

    if (g_failures == 0) {
        fprintf(stderr, "OK — POS slot-filling guards passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
