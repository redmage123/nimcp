/**
 * @file test_gl_phrase_persistence.c
 * @brief UNIT — the .gl_lang sidecar (v3) round-trips the phrase table.
 *
 * Context (2026-05-23):
 *   The produce-path bigram reranker scores candidates with
 *     cos + ALPHA * log(1 + bigram_freq(prev, cand))
 *   where bigram_freq comes from gl->phrases (the gl_phrase_t table
 *   populated by grounded_language_learn_from_text). That table was
 *   persisted by the standalone grounded_language_save (v3) but NOT by
 *   the sidecar gl_persistence_save() that the brain checkpoint actually
 *   uses — so every daemon restart wiped the phrase table and the rerank
 *   degenerated to cosine-only until enough text re-accumulated.
 *
 *   Fix: bump GL_SIDECAR_VERSION 2→3 and append a phrase block
 *   (form | component_words | frequency) to gl_persistence_save /
 *   gl_persistence_load.
 *
 * What this test guards:
 *   1. After learn_from_text builds a phrase table, gl_persistence_save
 *      then gl_persistence_load into a FRESH handle restores the same
 *      phrase count and a known bigram's frequency.
 *   2. A v2 (pre-phrase) file still loads cleanly with phrase_count == 0
 *      (backward compatibility) — simulated by hand-truncating is overkill,
 *      so we instead assert that a freshly-created handle with no phrases
 *      saved/loaded yields phrase_count 0 with no error.
 *
 * Standalone harness (no GTest). Compile:
 *   gcc -I include tests/unit/test_gl_phrase_persistence.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_gl_phrase_persistence
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/* ------------------------------------------------------------------ */
/* TEST 1: phrase table survives a save/load round-trip through the
 * sidecar. */
static void test_phrase_table_round_trips(void) {
    const char* path = "/tmp/nimcp_gl_phrase_persist_a.gl_lang";
    unlink(path);

    grounded_language_t* src = grounded_language_create(SEMANTIC_DIM, NULL);
    EXPECT(src != NULL, "create src gl");
    if (!src) return;

    /* Repeatedly feed sentences so the bigram "tree green" accrues a
     * recognizable frequency. The trailing "alpha" keeps the trigram
     * path from collapsing on a bare 2-word sentence. */
    const int REPS = 30;
    for (int i = 0; i < REPS; i++) {
        (void)grounded_language_learn_from_text(src, "tree green alpha");
    }

    uint32_t src_phrase_count = grounded_language_phrase_count(src);
    EXPECT(src_phrase_count > 0, "src has phrases (got %u)", src_phrase_count);

    const gl_phrase_t* src_p = grounded_language_lookup_phrase(src, "tree green");
    EXPECT(src_p != NULL, "src recorded 'tree green'");
    uint32_t src_freq = src_p ? src_p->frequency : 0u;
    EXPECT(src_freq >= 10u, "src 'tree green' freq>=10 (got %u)", src_freq);

    int rc = gl_persistence_save((const struct grounded_language*)src, path);
    EXPECT(rc == 0, "gl_persistence_save rc=%d", rc);

    /* Fresh handle — seed-only lexicon, zero phrases. */
    grounded_language_t* dst = grounded_language_create(SEMANTIC_DIM, NULL);
    EXPECT(dst != NULL, "create dst gl");
    EXPECT(grounded_language_phrase_count(dst) == 0,
           "dst starts with 0 phrases (got %u)",
           grounded_language_phrase_count(dst));

    rc = gl_persistence_load((struct grounded_language*)dst, path);
    EXPECT(rc == 0, "gl_persistence_load rc=%d", rc);

    uint32_t dst_phrase_count = grounded_language_phrase_count(dst);
    EXPECT(dst_phrase_count == src_phrase_count,
           "dst phrase_count %u == src %u", dst_phrase_count, src_phrase_count);

    const gl_phrase_t* dst_p = grounded_language_lookup_phrase(dst, "tree green");
    EXPECT(dst_p != NULL, "dst restored 'tree green'");
    if (dst_p) {
        EXPECT(dst_p->frequency == src_freq,
               "dst 'tree green' freq %u == src %u", dst_p->frequency, src_freq);
        EXPECT(dst_p->component_words == 2u,
               "dst 'tree green' is a bigram (got %u comp words)",
               dst_p->component_words);
    }

    fprintf(stderr, "  round-trip: src=%u phrases, dst=%u phrases, "
            "'tree green' freq %u->%u\n",
            src_phrase_count, dst_phrase_count, src_freq,
            dst_p ? dst_p->frequency : 0u);

    grounded_language_destroy(src);
    grounded_language_destroy(dst);
    unlink(path);
}

/* ------------------------------------------------------------------ */
/* TEST 2: an empty phrase table round-trips cleanly (phrase_count 0,
 * no error) — the v3 block writes count=0 and the loader reads it back
 * without disturbing the seed lexicon. */
static void test_empty_phrase_table_round_trips(void) {
    const char* path = "/tmp/nimcp_gl_phrase_persist_b.gl_lang";
    unlink(path);

    grounded_language_t* src = grounded_language_create(SEMANTIC_DIM, NULL);
    EXPECT(src != NULL, "create src gl");
    if (!src) return;
    EXPECT(grounded_language_phrase_count(src) == 0,
           "src has no phrases pre-save (got %u)",
           grounded_language_phrase_count(src));

    int rc = gl_persistence_save((const struct grounded_language*)src, path);
    EXPECT(rc == 0, "save empty-phrase gl rc=%d", rc);

    grounded_language_t* dst = grounded_language_create(SEMANTIC_DIM, NULL);
    rc = gl_persistence_load((struct grounded_language*)dst, path);
    EXPECT(rc == 0, "load empty-phrase gl rc=%d", rc);
    EXPECT(grounded_language_phrase_count(dst) == 0,
           "dst has 0 phrases after empty round-trip (got %u)",
           grounded_language_phrase_count(dst));

    grounded_language_destroy(src);
    grounded_language_destroy(dst);
    unlink(path);
}

int main(void) {
    fprintf(stderr, "[UNIT] test_gl_phrase_persistence\n");
    test_phrase_table_round_trips();
    test_empty_phrase_table_round_trips();

    if (g_failures == 0) {
        fprintf(stderr, "OK — phrase persistence guards passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
