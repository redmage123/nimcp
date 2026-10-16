/**
 * @file test_lang_f4_fluency.c
 * @brief Tier 1 Step F4 — production-fluency surface polish.
 *
 * Covers the three correctors in gl_apply_f4_fluency plus the two new
 * morphology helpers they rely on:
 *   - gl_morph_past_tense  (irregular table + regular -ed orthography)
 *   - gl_article_for       (phonetic a/an, silent-h + vowel-letter exceptions)
 *   - F4a articles         (first-mention indefinite + a/an phonetics + re-mention "the")
 *   - F4b tense            (past anchor drags following present verbs to past)
 *   - F4c pronoun case     (subject/object pronoun by slot; possessive determiner)
 *   - F4c possessive 's    (noun-noun → "creation's education")
 *   - stage gate           (< 2 → byte-identical passthrough)
 *   - NULL / empty safety
 *
 * The correctors classify words via gl_f4_class, which falls back to the
 * morphology hint (-tion→NOUN, -ed/-ate→VERB) — so the test needs only a gl
 * at stage 2 and crafted input strings; no grounding required.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_f4_fluency.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build/lib -o /tmp/test_lang_f4_fluency
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

/* ---- pure morphology helpers ---- */

static void test_morph_past_tense(void) {
    char out[64];
    struct { const char* in; const char* want; } C[] = {
        {"go","went"}, {"run","ran"}, {"eat","ate"}, {"see","saw"},
        {"is","was"}, {"are","were"}, {"have","had"},
        {"love","loved"}, {"walk","walked"}, {"try","tried"},
        {"operate","operated"}, {"play","played"}, {NULL,NULL}
    };
    for (int i = 0; C[i].in; i++) {
        int n = gl_morph_past_tense(C[i].in, out, sizeof(out));
        EXPECT(n > 0, "past_tense(%s) rc", C[i].in);
        EXPECT(strcmp(out, C[i].want) == 0,
               "past_tense(%s) = '%s' want '%s'", C[i].in, out, C[i].want);
    }
    /* error paths */
    EXPECT(gl_morph_past_tense(NULL, out, sizeof(out)) == -1, "NULL verb -> -1");
    EXPECT(gl_morph_past_tense("go", NULL, 0) == -1, "NULL out -> -1");
}

static void test_article_for(void) {
    struct { const char* w; const char* want; } C[] = {
        {"apple","an"}, {"egg","an"}, {"ocean","an"}, {"idea","an"},
        {"dog","a"}, {"tree","a"}, {"problem","a"},
        {"hour","an"}, {"honest","an"},          /* silent h */
        {"university","a"}, {"unicorn","a"}, {"european","a"}, /* /j/ glide */
        {"one","a"},                               /* /w/ glide */
        {NULL,NULL}
    };
    for (int i = 0; C[i].w; i++)
        EXPECT(strcmp(gl_article_for(C[i].w), C[i].want) == 0,
               "article_for(%s) = '%s' want '%s'",
               C[i].w, gl_article_for(C[i].w), C[i].want);
    EXPECT(strcmp(gl_article_for(NULL), "a") == 0, "NULL -> 'a'");
    EXPECT(strcmp(gl_article_for(""), "a") == 0, "empty -> 'a'");
}

/* ---- F4 corrector (stage 2) ---- */

static grounded_language_t* mk_gl(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (gl) grounded_language_set_current_stage_int(gl, 2);
    return gl;
}

static void test_f4a_articles(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create gl"); if (!gl) return;
    char out[256];

    /* first-mention singular count noun: "the" -> indefinite, phonetic. */
    gl_apply_f4_fluency(gl, "the creation", out, sizeof(out));
    EXPECT(strcmp(out, "a creation") == 0, "first-mention -> 'a creation' (got '%s')", out);

    gl_apply_f4_fluency(gl, "the education", out, sizeof(out));
    EXPECT(strcmp(out, "an education") == 0, "vowel head -> 'an education' (got '%s')", out);

    /* already-indefinite, no clear head: just fix a/an phonetics. */
    gl_apply_f4_fluency(gl, "a apple", out, sizeof(out));
    EXPECT(strcmp(out, "an apple") == 0, "phonetic a->an (got '%s')", out);
    gl_apply_f4_fluency(gl, "an dog", out, sizeof(out));
    EXPECT(strcmp(out, "a dog") == 0, "phonetic an->a (got '%s')", out);

    /* re-mention of the same noun takes definite "the". */
    gl_apply_f4_fluency(gl, "the creation thinks the creation", out, sizeof(out));
    EXPECT(strcmp(out, "a creation thinks the creation") == 0,
           "re-mention keeps 'the' (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_f4b_tense(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create gl"); if (!gl) return;
    char out[256];

    /* Past anchor (-ed) drags the following present verb (-ate) to past. */
    gl_apply_f4_fluency(gl, "she educated and operate", out, sizeof(out));
    EXPECT(strcmp(out, "she educated and operated") == 0,
           "tense consistency (got '%s')", out);

    /* No past anchor → leave present verbs alone. */
    gl_apply_f4_fluency(gl, "she educate and operate", out, sizeof(out));
    EXPECT(strcmp(out, "she educate and operate") == 0,
           "no past anchor -> unchanged (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_f4c_pronoun_case(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create gl"); if (!gl) return;
    char out[256];

    /* Object pronoun in subject slot → subject form (capital I). */
    gl_apply_f4_fluency(gl, "me organize things", out, sizeof(out));
    EXPECT(strcmp(out, "I organize things") == 0,
           "me->I subject (got '%s')", out);

    /* Subject pronoun in object slot (after a verb) → object form. */
    gl_apply_f4_fluency(gl, "creation organize i", out, sizeof(out));
    EXPECT(strcmp(out, "creation organize me") == 0,
           "i->me object (got '%s')", out);

    /* Bare pronoun before a noun → possessive determiner. */
    gl_apply_f4_fluency(gl, "he creation", out, sizeof(out));
    EXPECT(strcmp(out, "his creation") == 0,
           "he->his possessive (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_f4c_possessive_s(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create gl"); if (!gl) return;
    char out[256];

    /* Two consecutive (morphology-tagged) nouns → possessive 's. */
    gl_apply_f4_fluency(gl, "creation education", out, sizeof(out));
    EXPECT(strcmp(out, "creation's education") == 0,
           "noun-noun possessive (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_stage_gate(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create gl"); if (!gl) return;
    char out[256];

    /* Stage 0/1: byte-identical passthrough, zero edits. */
    grounded_language_set_current_stage_int(gl, 1);
    int fixes = gl_apply_f4_fluency(gl, "a apple", out, sizeof(out));
    EXPECT(fixes == 0, "stage 1 makes 0 edits (got %d)", fixes);
    EXPECT(strcmp(out, "a apple") == 0, "stage 1 passthrough (got '%s')", out);

    /* Same input at stage 2 IS corrected — proves the gate is what blocked it. */
    grounded_language_set_current_stage_int(gl, 2);
    gl_apply_f4_fluency(gl, "a apple", out, sizeof(out));
    EXPECT(strcmp(out, "an apple") == 0, "stage 2 corrects (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_safety(void) {
    char out[64];
    EXPECT(gl_apply_f4_fluency(NULL, NULL, out, sizeof(out)) == -1, "NULL in -> -1");
    EXPECT(gl_apply_f4_fluency(NULL, "hi", NULL, 0) == -1, "NULL out -> -1");
    /* NULL gl is ungated (unit-test mode) — must not crash, returns >=0. */
    int rc = gl_apply_f4_fluency(NULL, "a apple", out, sizeof(out));
    EXPECT(rc >= 0, "NULL gl ungated rc=%d", rc);
    EXPECT(strcmp(out, "an apple") == 0, "NULL gl still corrects a/an (got '%s')", out);
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_f4_fluency (Tier 1 Step F4)\n");
    test_morph_past_tense();
    test_article_for();
    test_f4a_articles();
    test_f4b_tense();
    test_f4c_pronoun_case();
    test_f4c_possessive_s();
    test_stage_gate();
    test_safety();

    if (g_failures == 0) { fprintf(stderr, "ALL PASS\n"); return 0; }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
