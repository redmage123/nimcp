/**
 * @file test_lang_function_words.c
 * @brief UNIT — Tier 1 Step F2: function-word scaffolding in produce.
 *
 * The produce candidate pool excludes function words, so the bare emit
 * stream is content-only ("creation running motion"). F2 inserts a
 * determiner before a noun-phrase head (NOUN/ADJ at start or after a verb)
 * and a copula before a predicate adjective — stage-gated by the same
 * weight as the POS bias (off at stages 0-1).
 *
 * With tied cosine over NOUN/VERB/NOUN and the stage-2 POS bias ordering
 * them subject-verb-object, the expected surface form is:
 *     "the creation running the motion"
 *   - "the" before the subject noun (utterance start)
 *   - "the" before the object noun (after the verb)
 * and word_count counts the scaffolding tokens (3 content + 2 function = 5).
 *
 * Guards:
 *   1. Stage 2: first token is "the"; ≥2 "the" inserted; word_count == 5.
 *   2. Stage 1 (gate off): NO function words inserted — bare content only.
 *
 * Reuses the disjoint one-hot grounding trick from test_lang_pos_slot_filling.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_function_words.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_function_words
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

static uint64_t ground_word(grounded_language_t* gl, const char* word,
                            uint32_t seed_idx) {
    float feats[SEMANTIC_DIM] = {0};
    if (seed_idx < SEMANTIC_DIM) feats[seed_idx] = 1.0f;
    if ((seed_idx + 7u) < SEMANTIC_DIM) feats[seed_idx + 7u] = 0.3f;
    return grounded_language_fast_map(gl, word, feats, SEMANTIC_DIM, 1u);
}

static uint32_t split_words(char* work, const char** out, uint32_t max_w) {
    uint32_t n = 0;
    bool in = false;
    for (char* p = work; *p; p++) {
        if (*p == ' ') { *p = '\0'; in = false; }
        else if (!in) { in = true; if (n < max_w) out[n] = p; n++; }
    }
    return n;
}

static uint32_t count_tok(const char** w, uint32_t n, const char* t) {
    uint32_t c = 0;
    for (uint32_t i = 0; i < n; i++) if (strcmp(w[i], t) == 0) c++;
    return c;
}

/* TEST 1: stage 2 inserts determiners around the nouns. */
static void test_determiners_inserted_stage2(void) {
    semantic_memory_system_t* sm = semantic_memory_create();
    grounded_language_t* gl = grounded_language_create(SEMANTIC_DIM, sm);
    EXPECT(gl != NULL, "create gl");
    if (!gl) { semantic_memory_destroy(sm); return; }
    grounded_language_set_current_stage_int(gl, 2);

    EXPECT(ground_word(gl, "creation", 4u)  != 0, "ground creation");
    EXPECT(ground_word(gl, "running",  13u) != 0, "ground running");
    EXPECT(ground_word(gl, "motion",   22u) != 0, "ground motion");
    EXPECT(grounded_language_lookup(gl, "creation")->learned_class == GL_CLASS_NOUN, "creation NOUN");
    EXPECT(grounded_language_lookup(gl, "running")->learned_class  == GL_CLASS_VERB, "running VERB");
    EXPECT(grounded_language_lookup(gl, "motion")->learned_class   == GL_CLASS_NOUN, "motion NOUN");

    float intent[SEMANTIC_DIM] = {0};
    intent[4u] = 1.0f; intent[13u] = 1.0f; intent[22u] = 1.0f;

    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SEMANTIC_DIM,
                                       GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0, "produce rc=%d", rc);
    EXPECT(r.text && r.text[0], "produce emitted text");
    if (rc == 0 && r.text) {
        fprintf(stderr, "  stage-2 output: '%s' (word_count=%u)\n",
                r.text, r.word_count);
        char work[256];
        snprintf(work, sizeof(work), "%s", r.text);
        const char* w[16] = {0};
        uint32_t nw = split_words(work, w, 16);
        EXPECT(nw >= 4, "expected scaffolded length (got %u)", nw);
        if (nw >= 1) {
            EXPECT(strcmp(w[0], "the") == 0,
                   "first token should be determiner 'the' (got '%s')", w[0]);
        }
        EXPECT(count_tok(w, nw, "the") >= 2,
               "expected >=2 'the' (subject+object), got %u",
               count_tok(w, nw, "the"));
        /* word_count must include the scaffolding tokens. */
        EXPECT(r.word_count == nw,
               "word_count %u should equal surface tokens %u", r.word_count, nw);
        EXPECT(r.word_count >= 5, "scaffolded word_count >=5 (got %u)", r.word_count);
    }
    gl_production_result_cleanup(&r);
    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
    if (g_failures == 0) fprintf(stderr, "PASS test_determiners_inserted_stage2\n");
}

/* TEST 2: stage 1 gate is off — no function words inserted. */
static void test_no_scaffolding_stage1(void) {
    semantic_memory_system_t* sm = semantic_memory_create();
    grounded_language_t* gl = grounded_language_create(SEMANTIC_DIM, sm);
    if (!gl) { semantic_memory_destroy(sm); return; }
    grounded_language_set_current_stage_int(gl, 1);

    ground_word(gl, "creation", 4u);
    ground_word(gl, "running",  13u);
    ground_word(gl, "motion",   22u);

    float intent[SEMANTIC_DIM] = {0};
    intent[4u] = 1.0f; intent[13u] = 1.0f; intent[22u] = 1.0f;

    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SEMANTIC_DIM,
                                       GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0, "produce rc=%d", rc);
    if (rc == 0 && r.text) {
        fprintf(stderr, "  stage-1 output: '%s'\n", r.text);
        char work[256];
        snprintf(work, sizeof(work), "%s", r.text);
        const char* w[16] = {0};
        uint32_t nw = split_words(work, w, 16);
        EXPECT(count_tok(w, nw, "the") == 0,
               "stage 1 must insert no 'the' (got %u)", count_tok(w, nw, "the"));
        EXPECT(count_tok(w, nw, "is") == 0,
               "stage 1 must insert no 'is' (got %u)", count_tok(w, nw, "is"));
    }
    gl_production_result_cleanup(&r);
    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
    if (g_failures == 0) fprintf(stderr, "PASS test_no_scaffolding_stage1\n");
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_function_words (Tier 1 Step F2)\n");
    test_determiners_inserted_stage2();
    test_no_scaffolding_stage1();

    if (g_failures == 0) {
        fprintf(stderr, "OK — function-word scaffolding guards passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
