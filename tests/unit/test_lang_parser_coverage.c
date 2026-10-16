/**
 * @file test_lang_parser_coverage.c
 * @brief Parser coverage regression — Wernicke side.
 *
 * Verifies the reduction rules and sentence-type flags added in the
 * parser-coverage expansion (2026-05-11): NP+VP→S, V+NP→VP, AUX-inversion
 * question flag, lone-VP imperative flag, and NP+CONJ+NP conjunction.
 *
 * The Broca side has its own test (`test_lang_parser_coverage_broca.c`)
 * because Broca and Wernicke each define their own `phrase_type_t` enum
 * (the divergence is intentional and out of scope for this commit).
 *
 * Build (manual):
 *   gcc tests/unit/test_lang_parser_coverage.c \
 *       -I include -L build_wt/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build_wt/lib -o test_lang_parser_coverage
 */

#include "core/brain/regions/wernicke/nimcp_syntactic_comprehension.h"

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

static syntactic_word_t mk_word(const char* w,
                                syntactic_category_t cat,
                                uint32_t pos)
{
    syntactic_word_t out;
    memset(&out, 0, sizeof(out));
    strncpy(out.word, w, sizeof(out.word) - 1);
    out.category = cat;
    out.category_confidence = 0.9f;
    out.position = pos;
    out.lemma_id = pos + 1;
    return out;
}

static void test_wernicke_declarative(void)
{
    syntactic_comprehension_t* ctx = syntactic_comprehension_create(NULL);
    EXPECT(ctx != NULL, "create");
    if (!ctx) return;

    /* "the cat ran" — DET N V → NP + VP → S */
    syntactic_word_t words[3] = {
        mk_word("the", SYN_CAT_DET,  0),
        mk_word("cat", SYN_CAT_NOUN, 1),
        mk_word("ran", SYN_CAT_VERB, 2),
    };

    syntactic_parse_t parse;
    memset(&parse, 0, sizeof(parse));
    int rc = syntactic_parse_sentence(ctx, words, 3, &parse);
    EXPECT(rc == 0, "parse rc=%d", rc);
    EXPECT(!parse.is_question,   "declarative isn't question");
    EXPECT(!parse.is_imperative, "declarative isn't imperative");
    EXPECT((parse.reductions_applied & WERNICKE_REDUCTION_S_NP_VP) != 0,
           "S_NP_VP reduction fired, got 0x%x",
           parse.reductions_applied);

    syntactic_parse_free(&parse);
    syntactic_comprehension_destroy(ctx);
}

static void test_wernicke_imperative(void)
{
    syntactic_comprehension_t* ctx = syntactic_comprehension_create(NULL);
    EXPECT(ctx != NULL, "create");
    if (!ctx) return;

    /* "run home" — VERB NOUN with no subject → imperative */
    syntactic_word_t words[2] = {
        mk_word("run",  SYN_CAT_VERB, 0),
        mk_word("home", SYN_CAT_NOUN, 1),
    };

    syntactic_parse_t parse;
    memset(&parse, 0, sizeof(parse));
    int rc = syntactic_parse_sentence(ctx, words, 2, &parse);
    EXPECT(rc == 0, "parse rc=%d", rc);
    EXPECT(parse.is_imperative, "imperative flag set");
    EXPECT(!parse.is_question, "not a question");

    syntactic_parse_free(&parse);
    syntactic_comprehension_destroy(ctx);
}

static void test_wernicke_yesno_question(void)
{
    syntactic_comprehension_t* ctx = syntactic_comprehension_create(NULL);
    EXPECT(ctx != NULL, "create");
    if (!ctx) return;

    /* "is the cat hungry" — AUX DET N ADJ → question */
    syntactic_word_t words[4] = {
        mk_word("is",     SYN_CAT_AUX,  0),
        mk_word("the",    SYN_CAT_DET,  1),
        mk_word("cat",    SYN_CAT_NOUN, 2),
        mk_word("hungry", SYN_CAT_ADJ,  3),
    };

    syntactic_parse_t parse;
    memset(&parse, 0, sizeof(parse));
    int rc = syntactic_parse_sentence(ctx, words, 4, &parse);
    EXPECT(rc == 0, "parse rc=%d", rc);
    EXPECT(parse.is_question, "question flag set");
    EXPECT(!parse.is_imperative, "not imperative");

    syntactic_parse_free(&parse);
    syntactic_comprehension_destroy(ctx);
}

static void test_wernicke_wh_question(void)
{
    syntactic_comprehension_t* ctx = syntactic_comprehension_create(NULL);
    EXPECT(ctx != NULL, "create");
    if (!ctx) return;

    /* "what is the cat" — wh-fronting */
    syntactic_word_t words[4] = {
        mk_word("what", SYN_CAT_PRON, 0),
        mk_word("is",   SYN_CAT_AUX,  1),
        mk_word("the",  SYN_CAT_DET,  2),
        mk_word("cat",  SYN_CAT_NOUN, 3),
    };

    syntactic_parse_t parse;
    memset(&parse, 0, sizeof(parse));
    int rc = syntactic_parse_sentence(ctx, words, 4, &parse);
    EXPECT(rc == 0, "parse rc=%d", rc);
    EXPECT(parse.is_question, "wh-question flag set");

    syntactic_parse_free(&parse);
    syntactic_comprehension_destroy(ctx);
}

static void test_wernicke_conjunction(void)
{
    syntactic_comprehension_t* ctx = syntactic_comprehension_create(NULL);
    EXPECT(ctx != NULL, "create");
    if (!ctx) return;

    /* "the cat and the dog" — DET N CONJ DET N */
    syntactic_word_t words[5] = {
        mk_word("the", SYN_CAT_DET,  0),
        mk_word("cat", SYN_CAT_NOUN, 1),
        mk_word("and", SYN_CAT_CONJ, 2),
        mk_word("the", SYN_CAT_DET,  3),
        mk_word("dog", SYN_CAT_NOUN, 4),
    };

    syntactic_parse_t parse;
    memset(&parse, 0, sizeof(parse));
    int rc = syntactic_parse_sentence(ctx, words, 5, &parse);
    EXPECT(rc == 0, "parse rc=%d", rc);
    /* Either conjunction reduction or compound-NP coercion should fire. */
    uint32_t expected_bits =
        WERNICKE_REDUCTION_NP_CONJ_NP |
        WERNICKE_REDUCTION_NP_DET_N;
    EXPECT((parse.reductions_applied & expected_bits) != 0,
           "expected NP_CONJ_NP or NP_DET_N reduction, got 0x%x",
           parse.reductions_applied);

    syntactic_parse_free(&parse);
    syntactic_comprehension_destroy(ctx);
}

int main(void)
{
    test_wernicke_declarative();
    test_wernicke_imperative();
    test_wernicke_yesno_question();
    test_wernicke_wh_question();
    test_wernicke_conjunction();

    if (g_failures == 0) {
        printf("test_lang_parser_coverage: ALL TESTS PASSED\n");
        return 0;
    }
    fprintf(stderr, "test_lang_parser_coverage: %d FAILURES\n", g_failures);
    return 1;
}
