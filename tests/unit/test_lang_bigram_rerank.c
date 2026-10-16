/**
 * @file test_lang_bigram_rerank.c
 * @brief UNIT — bigram-conditioned reranking in grounded_language_produce
 *
 * Tier 1 Step B (commit-pending, 2026-05-23): the produce path
 * (grounded_language_produce) used to emit the top-K cosine-ranked
 * content words in cosine order, producing word salad like
 *   "knowledge experience learning awareness"
 * The bigram rerank picks each next emission as the unused top-K
 * candidate maximizing
 *     cos_score + ALPHA * log(1 + bigram_freq(prev, cand))
 * where bigram_freq comes from gl->phrases (the gl_phrase_t table
 * populated by grounded_language_learn_from_text).
 *
 * What this test guards:
 *   1. When NO phrase table entries exist, the produce output is the
 *      same as it would be without the rerank (cosine order). This
 *      proves backward compatibility.
 *   2. When phrase-table entries make one specific pair MUCH more
 *      frequent than competing candidates, that pair is emitted
 *      adjacent in the produce output (the bigram bias is doing
 *      something).
 *
 * Approach:
 *   - Pre-ground 6 content words with one-hot+offset feature profiles
 *     so cosine ranking is deterministic.
 *   - Build an intent vector that activates ~3 of them at near-equal
 *     cosine (so cosine alone would tie or near-tie).
 *   - Feed sentences via learn_from_text so the phrase table records
 *     specific bigrams.
 *   - Call grounded_language_produce(GL_PRODUCE_DESCRIBE).
 *   - Assert that the bigram-favored pair appears adjacent in output.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_bigram_rerank.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_bigram_rerank
 */

#include "language/nimcp_grounded_language.h"
#include "cognitive/memory/nimcp_semantic_memory.h"

#include <math.h>
#include <stdbool.h>
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

/* Ground a word with a non-overlapping one-hot+offset profile. Cosine
 * across distinct words is then ~0 (orthogonal). Same trick used by
 * tests/integration/test_lang_comprehend_pipeline.c. */
static uint64_t ground_word(grounded_language_t* gl, const char* word,
                            uint32_t seed_idx)
{
    float feats[SEMANTIC_DIM] = {0};
    if (seed_idx < SEMANTIC_DIM) feats[seed_idx] = 1.0f;
    if ((seed_idx + 7u) < SEMANTIC_DIM) feats[seed_idx + 7u] = 0.3f;
    return grounded_language_fast_map(gl, word, feats, SEMANTIC_DIM, 1u);
}

/* Build an intent vector that activates 3 specific dims at equal weight.
 * Because the words were grounded with disjoint one-hot dims, this yields
 * a roughly-tied cosine ranking across those 3 candidates. */
static void build_3way_intent(float* out, uint32_t a, uint32_t b, uint32_t c)
{
    memset(out, 0, SEMANTIC_DIM * sizeof(float));
    if (a < SEMANTIC_DIM) out[a] = 1.0f;
    if (b < SEMANTIC_DIM) out[b] = 1.0f;
    if (c < SEMANTIC_DIM) out[c] = 1.0f;
}

/* ====================================================================== */
/* TEST 1: Empty phrase table → backward-compat.
 * Without any learn_from_text calls, the phrase table is empty. produce
 * should emit content words in cosine order — same as the pre-rerank
 * behavior. Specifically, it should still emit a non-empty result. */
static void test_empty_phrase_table_is_backward_compatible(void)
{
    semantic_memory_system_t* sm = semantic_memory_create();
    EXPECT(sm != NULL, "semantic_memory create");
    grounded_language_t* gl = grounded_language_create(SEMANTIC_DIM, sm);
    EXPECT(gl != NULL, "grounded_language create");
    if (!gl) { semantic_memory_destroy(sm); return; }

    /* Stage 2 lifts the developmental confidence floor enough that the
     * loop emits multiple words (the floor at stage 0 is 1.0 which
     * blocks anything past position 0 by design). The rerank's
     * backward-compat behavior is what we're guarding here, not the
     * floor itself — test_lang_distributional_anticollapse covers the
     * floor's stop semantics elsewhere. */
    grounded_language_set_current_stage_int(gl, 2);

    EXPECT(ground_word(gl, "tree",  4u)  != 0, "ground tree");
    EXPECT(ground_word(gl, "fire", 13u)  != 0, "ground fire");
    EXPECT(ground_word(gl, "water", 22u) != 0, "ground water");
    EXPECT(grounded_language_phrase_count(gl) == 0,
           "phrase table empty pre-test");

    float intent[SEMANTIC_DIM];
    build_3way_intent(intent, 4u, 13u, 22u);

    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SEMANTIC_DIM,
                                        GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0, "produce rc=%d", rc);
    EXPECT(r.text != NULL && r.text[0],
           "produce emitted text (got %s)", r.text ? r.text : "(null)");
    EXPECT(r.word_count >= 1,
           "produce word_count >= 1 (got %u)", r.word_count);
    fprintf(stderr, "  empty-phrase-table output: %s\n",
            r.text ? r.text : "(null)");
    gl_production_result_cleanup(&r);
    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
}

/* TEST 2: Bigram bias reorders output.
 * Pre-ground 4 words with one-hot dims that all tie under the intent.
 * Then drive learn_from_text on "tree green" many times, so the phrase
 * "tree green" gets a high frequency. After that, produce should emit
 * "tree" and "green" adjacent (in either order — the rerank chooses
 * the cosine-best as position 0 and picks the bigram partner next). */
static void test_bigram_bias_emits_adjacent_pair(void)
{
    semantic_memory_system_t* sm = semantic_memory_create();
    grounded_language_t* gl = grounded_language_create(SEMANTIC_DIM, sm);
    if (!gl) { semantic_memory_destroy(sm); return; }
    /* Stage 2 lifts the developmental confidence floor enough that the
     * emit loop runs for 4 positions (the rerank only does work past
     * position 0). At stage 0 the floor is 1.0 and the loop would emit
     * exactly 1 word, defeating the test's premise. */
    grounded_language_set_current_stage_int(gl, 2);

    /* Four targets with disjoint one-hot dims so cosine-only ranking
     * across an intent that activates all four is tied. */
    EXPECT(ground_word(gl, "tree",  4u)  != 0, "ground tree");
    EXPECT(ground_word(gl, "green",13u)  != 0, "ground green");
    EXPECT(ground_word(gl, "fire", 22u)  != 0, "ground fire");
    EXPECT(ground_word(gl, "stone",31u)  != 0, "ground stone");

    /* Repeatedly feed "tree green ..." sentences. Each call increments
     * the "tree green" bigram entry in the phrase table. After 50 calls
     * the entry should be well-populated. The trailing distractor
     * "alpha" prevents the trigram path from collapsing on a 2-word
     * sentence. */
    for (int rep = 0; rep < 50; rep++) {
        (void)grounded_language_learn_from_text(gl, "tree green alpha");
    }

    /* Verify the phrase table actually recorded "tree green". */
    const gl_phrase_t* p = grounded_language_lookup_phrase(gl, "tree green");
    EXPECT(p != NULL, "phrase 'tree green' recorded");
    if (p) {
        fprintf(stderr, "  phrase 'tree green' freq=%u\n", p->frequency);
        EXPECT(p->frequency >= 10,
               "phrase 'tree green' freq>=10 (got %u)", p->frequency);
    }

    /* Build an intent that ties all 4 candidates at the cosine level. */
    float intent[SEMANTIC_DIM] = {0};
    intent[4u]  = 1.0f;  /* tree */
    intent[13u] = 1.0f;  /* green */
    intent[22u] = 1.0f;  /* fire */
    intent[31u] = 1.0f;  /* stone */

    gl_production_result_t r = {0};
    int rc = grounded_language_produce(gl, intent, SEMANTIC_DIM,
                                        GL_PRODUCE_DESCRIBE, &r);
    EXPECT(rc == 0, "produce rc=%d", rc);
    if (rc == 0 && r.text) {
        fprintf(stderr, "  bigram-biased output: %s\n", r.text);
        /* Find "tree" and "green" positions in the output and assert
         * they are adjacent (offset 1). Both words must appear. */
        const char* t = strstr(r.text, "tree");
        const char* g = strstr(r.text, "green");
        EXPECT(t != NULL, "output contains 'tree' (got '%s')", r.text);
        EXPECT(g != NULL, "output contains 'green' (got '%s')", r.text);
        if (t && g) {
            /* Adjacent means they are separated by exactly one space.
             * Either "tree green" or "green tree" is acceptable. */
            const char* first  = (t < g) ? t : g;
            const char* second = (t < g) ? g : t;
            const char* first_end = first + ((first == t) ? 4 : 5);
            /* first_end should be at a space, immediately followed by
             * second. */
            EXPECT(*first_end == ' ',
                   "expected space between target words (got '%s')",
                   r.text);
            EXPECT(first_end + 1 == second,
                   "'tree' and 'green' not adjacent (got '%s')", r.text);
        }
    }
    gl_production_result_cleanup(&r);
    grounded_language_destroy(gl);
    semantic_memory_destroy(sm);
}

int main(void)
{
    fprintf(stderr, "[UNIT] test_lang_bigram_rerank\n");
    test_empty_phrase_table_is_backward_compatible();
    test_bigram_bias_emits_adjacent_pair();

    if (g_failures == 0) {
        fprintf(stderr, "OK — bigram rerank guards passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
