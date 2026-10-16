/**
 * @file test_lang_pronominalization.c
 * @brief Tier 2 — produce-side pronominalization (recency anaphora).
 *
 * Covers gl_apply_pronominalization + the produce_pronominalize flag:
 *   1. setter/getter — default OFF, round-trips.
 *   2. LANC persistence — flag survives save/load (trailing byte).
 *   3. object-position re-mention → "it" ("the creation organized the
 *      creation" -> "the creation organized it"; determiner dropped).
 *   4. no-op when there is no re-mention, no verb between mentions, or the
 *      stage gate (<2) is in effect.
 *   5. NULL / empty safety.
 *
 * Test words use morphology-classified forms (-tion -> NOUN, -ed -> VERB) so
 * gl_f4_class resolves them deterministically without grounding.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_pronominalization.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build/lib -o /tmp/test_lang_pronominalization
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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

static grounded_language_t* mk_gl(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (gl) grounded_language_set_current_stage_int(gl, 2);
    return gl;
}

static void test_setter_getter(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    EXPECT(!grounded_language_get_produce_pronominalize(gl), "default OFF");
    grounded_language_set_produce_pronominalize(gl, true);
    EXPECT(grounded_language_get_produce_pronominalize(gl), "ON after set");
    grounded_language_set_produce_pronominalize(gl, false);
    EXPECT(!grounded_language_get_produce_pronominalize(gl), "OFF after clear");
    EXPECT(!grounded_language_get_produce_pronominalize(NULL), "NULL -> false");
    grounded_language_destroy(gl);
}

static void test_persist_round_trip(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_lang_pron_%d.bin", (int)getpid());

    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create save"); if (!gl) return;
    grounded_language_set_produce_pronominalize(gl, true);
    FILE* f = fopen(path, "wb");
    if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save rc=%d", rc);
    grounded_language_destroy(gl);

    grounded_language_t* gl2 = grounded_language_create(64, NULL);
    if (!gl2) { unlink(path); return; }
    EXPECT(!grounded_language_get_produce_pronominalize(gl2), "default OFF pre-load");
    f = fopen(path, "rb");
    if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load rc=%d", rc);
    EXPECT(grounded_language_get_produce_pronominalize(gl2), "ON post-load");
    grounded_language_destroy(gl2);
    unlink(path);
}

static void test_object_remention(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];

    /* object-position re-mention across a verb -> "it", determiner dropped. */
    gl_apply_pronominalization(gl, "the creation organized the creation", out, sizeof(out));
    EXPECT(strcmp(out, "the creation organized it") == 0,
           "object re-mention -> it (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_no_ops(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];

    /* No re-mention — different nouns, unchanged. */
    gl_apply_pronominalization(gl, "the creation organized the position", out, sizeof(out));
    EXPECT(strcmp(out, "the creation organized the position") == 0,
           "distinct nouns unchanged (got '%s')", out);

    /* Re-mention but NO verb between -> not an object slot, unchanged. */
    gl_apply_pronominalization(gl, "the creation creation", out, sizeof(out));
    EXPECT(strcmp(out, "the creation creation") == 0,
           "adjacent re-mention (no verb) unchanged (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_stage_gate(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];

    grounded_language_set_current_stage_int(gl, 1);
    int fixes = gl_apply_pronominalization(gl, "the creation organized the creation",
                                           out, sizeof(out));
    EXPECT(fixes == 0, "stage 1 -> 0 fixes (got %d)", fixes);
    EXPECT(strcmp(out, "the creation organized the creation") == 0,
           "stage 1 passthrough (got '%s')", out);

    /* Same input at stage 2 IS pronominalized — proves the gate blocked it. */
    grounded_language_set_current_stage_int(gl, 2);
    gl_apply_pronominalization(gl, "the creation organized the creation", out, sizeof(out));
    EXPECT(strcmp(out, "the creation organized it") == 0,
           "stage 2 pronominalizes (got '%s')", out);

    grounded_language_destroy(gl);
}

/* T2-3: punctuation-tolerant verb detection enables a cross-clause subject
 * re-mention ("...organized. the creation activated" -> "...organized. it
 * activated"). Without lemma stripping, "organized." would not register as the
 * required verb-between and the second "creation" (subject of "activated")
 * would be left unpronominalized. */
static void test_punct_cross_clause_subject(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    gl_apply_pronominalization(gl,
        "the creation organized. the creation activated", out, sizeof(out));
    EXPECT(strcmp(out, "the creation organized. it activated") == 0,
           "cross-clause subject -> it (got '%s')", out);
    grounded_language_destroy(gl);
}

/* T2-3: trailing punctuation on the replaced noun is re-attached to the
 * pronoun so a sentence boundary survives ("...the creation." -> "...it."). */
static void test_trailing_punct_preserved(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    gl_apply_pronominalization(gl,
        "the creation organized the creation.", out, sizeof(out));
    EXPECT(strcmp(out, "the creation organized it.") == 0,
           "trailing period preserved (got '%s')", out);
    grounded_language_destroy(gl);
}

/* C3 (morphology) + plural path: now that "creations" classifies as NOUN via
 * the new "-tions" plural rule, a plural subject re-mention across a clause
 * becomes "they". */
static void test_plural_subject(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    gl_apply_pronominalization(gl,
        "the creations organized. the creations activated", out, sizeof(out));
    EXPECT(strcmp(out, "the creations organized. they activated") == 0,
           "plural subject re-mention -> they (got '%s')", out);
    grounded_language_destroy(gl);
}

/* C1: a re-mention with a verb between the mentions but NEITHER an adjacent
 * following verb (subject) NOR preceding verb (object) is left as the full
 * noun, not guessed into a pronoun by fallthrough. */
static void test_unanchored_not_pronominalized(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    /* second "creation" is preceded by "and" (not a verb) and is utterance-
     * final (no following verb) -> neither subject nor object -> unchanged. */
    gl_apply_pronominalization(gl,
        "the creation organized and the creation", out, sizeof(out));
    EXPECT(strcmp(out, "the creation organized and the creation") == 0,
           "unanchored re-mention unchanged (got '%s')", out);
    grounded_language_destroy(gl);
}

static void test_safety(void) {
    char out[64];
    EXPECT(gl_apply_pronominalization(NULL, NULL, out, sizeof(out)) == -1, "NULL in -> -1");
    EXPECT(gl_apply_pronominalization(NULL, "hi", NULL, 0) == -1, "NULL out -> -1");
    int rc = gl_apply_pronominalization(NULL, "the creation organized the creation",
                                        out, sizeof(out));
    EXPECT(rc >= 0, "NULL gl ungated rc=%d", rc);
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_pronominalization (Tier 2)\n");
    test_setter_getter();
    test_persist_round_trip();
    test_object_remention();
    test_no_ops();
    test_stage_gate();
    test_punct_cross_clause_subject();
    test_trailing_punct_preserved();
    test_plural_subject();
    test_unanchored_not_pronominalized();
    test_safety();
    if (g_failures == 0) { fprintf(stderr, "ALL PASS\n"); return 0; }
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
}
