/**
 * @file test_lang_anaphora.c
 * @brief Tier-1 #2 — verify rule-based anaphora / pronoun resolution.
 *
 * Pattern: standalone smoke test. Compile:
 *   gcc -I include tests/unit/test_lang_anaphora.c \
 *       -L build/lib -lnimcp -lm \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_anaphora
 *
 * Coverage:
 *   1. test_default_off:
 *      Anaphora is OFF by default. Comprehending "the cat sat. it was warm."
 *      treats "it" as an ordinary lookup-chain miss; the global
 *      anaphora_resolutions counter stays unchanged.
 *
 *   2. test_resolve_female_after_push:
 *      Enable. Comprehend "Alice walked. She smiled." — "Alice" is pushed
 *      to the referent ring as singular_male via the capitalization
 *      heuristic, but actually we want female. We pre-classify by
 *      emitting "she Alice" first to mark Alice female, OR we push Alice
 *      explicitly via a learn step. Simpler: for a deterministic test of
 *      the SHE-resolves path, comprehend "Mary walked. She smiled." after
 *      we register "Mary" via fast_map AND a preceding "she Mary" cue
 *      that propagates female to the next noun. The cleaner pattern: just
 *      comprehend "she Alice walked. She smiled." — first "she" sets
 *      pending_female so Alice is pushed as female; second "She" then
 *      resolves to "alice" → counter +=1.
 *
 *   3. test_gender_mismatch:
 *      Enable. Comprehend "Bob ate. She left." — Bob is pushed as male
 *      (capitalized). "She" finds no female referent → no resolution.
 *      Counter unchanged.
 *
 *   4. test_plural_resolves_to_plural:
 *      Enable. Comprehend "the cats slept. they purred." — "cats" is
 *      pushed as plural (trailing 's'), "they" resolves to "cats" →
 *      counter +=1.
 */

#include "language/nimcp_grounded_language.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static int g_failures = 0;

#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

/* Bind a word to a unique concept via fast_map so the lexicon entry has
 * something to fold on resolution. The features themselves are arbitrary;
 * fast_map gives the entry a real concept id and a non-empty bindings
 * array. */
static void register_word(grounded_language_t* gl, const char* w, int seed) {
    /* semantic_dim is GL_SEMANTIC_DIM (default). We don't depend on the
     * exact value here — fast_map clamps to gl->semantic_dim internally. */
    float feat[256];
    for (int i = 0; i < 256; i++) {
        feat[i] = ((float)((seed * 31 + i * 17) & 0xff)) / 255.0f - 0.5f;
    }
    uint64_t cid = grounded_language_fast_map(gl, w, feat, 256, /*OBJECT*/ 1);
    /* fast_map can return 0 only on alloc failure / invalid args; with a
     * fresh gl this should always succeed. */
    if (cid == 0) {
        fprintf(stderr, "register_word('%s') failed\n", w);
    }
}

/* ------------------------------------------------------------------ */

static void test_default_off(void) {
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    register_word(gl, "cat", 1);

    uint64_t before = grounded_language_anaphora_resolutions();

    /* Anaphora is OFF by default — no set_anaphora_enabled call. */
    gl_comprehension_result_t r = {0};
    int rc = grounded_language_comprehend(gl,
        "the cat sat. it was warm.", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    uint64_t after = grounded_language_anaphora_resolutions();
    EXPECT(after == before,
           "counter must not advance with anaphora off (before=%llu after=%llu)",
           (unsigned long long)before, (unsigned long long)after);

    grounded_language_destroy(gl);
}

static void test_resolve_female_after_push(void) {
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    register_word(gl, "alice", 11);

    bool ok = grounded_language_set_anaphora_enabled(gl, true);
    EXPECT(ok, "set_anaphora_enabled");

    uint64_t before = grounded_language_anaphora_resolutions();

    /* "she Alice walked." — first "she" sets pending_female so Alice is
     * pushed as singular_female. Then "She smiled." resolves to "alice".
     * tokenize_text strips punctuation; "Alice" becomes lower-cased to
     * "alice" before the lexicon hit, and the same lowercased form
     * lands in the referent ring — so the second "She" finds it. */
    gl_comprehension_result_t r = {0};
    int rc = grounded_language_comprehend(gl,
        "she Alice walked. She smiled.", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    uint64_t after = grounded_language_anaphora_resolutions();
    EXPECT(after >= before + 1,
           "expected ≥1 resolution (before=%llu after=%llu)",
           (unsigned long long)before, (unsigned long long)after);

    grounded_language_destroy(gl);
}

static void test_gender_mismatch(void) {
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    register_word(gl, "bob", 22);

    bool ok = grounded_language_set_anaphora_enabled(gl, true);
    EXPECT(ok, "set_anaphora_enabled");

    uint64_t before = grounded_language_anaphora_resolutions();

    /* "Bob ate. She left." — Bob → male. No female referent. "She" must
     * not resolve. */
    gl_comprehension_result_t r = {0};
    int rc = grounded_language_comprehend(gl,
        "Bob ate. She left.", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    uint64_t after = grounded_language_anaphora_resolutions();
    EXPECT(after == before,
           "expected 0 resolutions (before=%llu after=%llu)",
           (unsigned long long)before, (unsigned long long)after);

    grounded_language_destroy(gl);
}

static void test_plural_resolves_to_plural(void) {
    grounded_language_t* gl = grounded_language_create(0, NULL);
    EXPECT(gl != NULL, "create");
    if (!gl) return;

    register_word(gl, "cats", 33);

    bool ok = grounded_language_set_anaphora_enabled(gl, true);
    EXPECT(ok, "set_anaphora_enabled");

    uint64_t before = grounded_language_anaphora_resolutions();

    /* "the cats slept. they purred." — "cats" pushed as plural (trailing
     * 's' + len ≥ 4). "they" resolves to "cats". */
    gl_comprehension_result_t r = {0};
    int rc = grounded_language_comprehend(gl,
        "the cats slept. they purred.", &r);
    EXPECT(rc == 0, "comprehend rc=%d", rc);
    gl_comprehension_result_cleanup(&r);

    uint64_t after = grounded_language_anaphora_resolutions();
    EXPECT(after >= before + 1,
           "expected ≥1 resolution for plural (before=%llu after=%llu)",
           (unsigned long long)before, (unsigned long long)after);

    grounded_language_destroy(gl);
}

int main(void)
{
    fprintf(stderr, "[Tier1-#2] test_lang_anaphora\n");
    test_default_off();
    test_resolve_female_after_push();
    test_gender_mismatch();
    test_plural_resolves_to_plural();

    if (g_failures == 0) {
        fprintf(stderr, "OK — all 4 tests passed\n");
        return 0;
    } else {
        fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
        return 1;
    }
}
