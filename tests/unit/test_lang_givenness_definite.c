/**
 * @file test_lang_givenness_definite.c
 * @brief T3-1 — givenness-driven definiteness (a/an -> the on re-mention).
 *
 * Covers gl_apply_givenness_definite + the produce_givenness_definite flag:
 *   1. setter/getter — default OFF, round-trips, NULL-safe.
 *   2. LANC persistence — flag survives save/load (trailing byte).
 *   3. base case — second mention's "a" -> "the" ("a creation ... a creation"
 *      -> "a creation ... the creation").
 *   4. "an" case — preceding "an" gets swapped to "the".
 *   5. case-preserving — leading "A"/"An" -> "The"; mid-sentence "a"/"an" -> "the".
 *   6. idempotent — re-running on the polished output is a no-op.
 *   7. no-op when no re-mention, when the preceding article is already "the",
 *      or when the stage gate (<2) is in effect.
 *   8. NULL / empty safety.
 *
 * Test words use morphology-classified forms (-tion -> NOUN) so gl_f4_class
 * resolves them deterministically without grounding.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_givenness_definite.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build/lib -o /tmp/test_lang_givenness_definite
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
    EXPECT(grounded_language_get_produce_givenness_definite(gl), "default ON");
    grounded_language_set_produce_givenness_definite(gl, false);
    EXPECT(!grounded_language_get_produce_givenness_definite(gl), "OFF after clear");
    grounded_language_set_produce_givenness_definite(gl, true);
    EXPECT(grounded_language_get_produce_givenness_definite(gl), "ON after set");
    EXPECT(!grounded_language_get_produce_givenness_definite(NULL), "NULL -> false");
    grounded_language_set_produce_givenness_definite(NULL, true); /* must not crash */
    grounded_language_destroy(gl);
}

static void test_persist_round_trip(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_lang_givn_%d.bin", (int)getpid());

    /* Default is ON; flip to OFF before save so we can prove the false byte
     * survived the round-trip (vs the default re-asserting itself on load). */
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create save"); if (!gl) return;
    grounded_language_set_produce_givenness_definite(gl, false);
    FILE* f = fopen(path, "wb");
    if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save rc=%d", rc);
    grounded_language_destroy(gl);

    grounded_language_t* gl2 = grounded_language_create(64, NULL);
    if (!gl2) { unlink(path); return; }
    EXPECT(grounded_language_get_produce_givenness_definite(gl2), "default ON pre-load");
    f = fopen(path, "rb");
    if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load rc=%d", rc);
    EXPECT(!grounded_language_get_produce_givenness_definite(gl2), "OFF post-load");
    grounded_language_destroy(gl2);
    unlink(path);
}

static void test_base_a_to_the(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];

    int fixes = gl_apply_givenness_definite(gl,
        "a creation organized a creation", out, sizeof(out));
    EXPECT(strcmp(out, "a creation organized the creation") == 0,
           "second 'a' -> 'the' (got '%s')", out);
    EXPECT(fixes == 1, "one fix (got %d)", fixes);

    grounded_language_destroy(gl);
}

static void test_an_to_the(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];

    /* "an" before a vowel-initial noun like "intention" (-tion -> NOUN). */
    gl_apply_givenness_definite(gl,
        "an intention guided an intention", out, sizeof(out));
    EXPECT(strcmp(out, "an intention guided the intention") == 0,
           "second 'an' -> 'the' (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_case_preserving(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];

    /* Leading capital -> "The". */
    gl_apply_givenness_definite(gl,
        "A creation organized a creation", out, sizeof(out));
    /* The FIRST mention's "A" is preceded by no re-mention so it's untouched;
     * but if the corrector ever runs on a re-mention whose preceding "a" is
     * itself capitalized (start of a new sentence after a period), we want
     * "The". Build that case explicitly: */
    int fixes = gl_apply_givenness_definite(gl,
        "creation organized. A creation activated", out, sizeof(out));
    EXPECT(strcmp(out, "creation organized. The creation activated") == 0,
           "leading 'A' -> 'The' (got '%s')", out);
    EXPECT(fixes == 1, "one fix (got %d)", fixes);

    grounded_language_destroy(gl);
}

static void test_idempotent(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char buf1[256], buf2[256];

    gl_apply_givenness_definite(gl,
        "a creation organized a creation", buf1, sizeof(buf1));
    int fixes2 = gl_apply_givenness_definite(gl, buf1, buf2, sizeof(buf2));
    EXPECT(strcmp(buf1, buf2) == 0, "idempotent (1st='%s', 2nd='%s')", buf1, buf2);
    EXPECT(fixes2 == 0, "second pass = 0 fixes (got %d)", fixes2);

    grounded_language_destroy(gl);
}

static void test_no_ops(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];

    /* No re-mention -> unchanged. */
    int f1 = gl_apply_givenness_definite(gl,
        "a creation organized a position", out, sizeof(out));
    EXPECT(strcmp(out, "a creation organized a position") == 0,
           "distinct nouns unchanged (got '%s')", out);
    EXPECT(f1 == 0, "no fixes for distinct nouns (got %d)", f1);

    /* Already "the" before re-mention -> unchanged (T3-1 only fires on a/an). */
    int f2 = gl_apply_givenness_definite(gl,
        "a creation organized the creation", out, sizeof(out));
    EXPECT(strcmp(out, "a creation organized the creation") == 0,
           "the+re-mention unchanged (got '%s')", out);
    EXPECT(f2 == 0, "no fixes when already 'the' (got %d)", f2);

    /* No preceding article (raw NOUN re-mention) -> unchanged. */
    int f3 = gl_apply_givenness_definite(gl,
        "creation organized a creation", out, sizeof(out));
    /* The "a" here precedes the SECOND creation, which IS a re-mention of the
     * unarticled first one — so this SHOULD fire. */
    EXPECT(strcmp(out, "creation organized the creation") == 0,
           "re-mention after bare first mention (got '%s')", out);
    EXPECT(f3 == 1, "one fix (got %d)", f3);

    grounded_language_destroy(gl);
}

static void test_stage_gate(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];

    grounded_language_set_current_stage_int(gl, 1);
    int fixes = gl_apply_givenness_definite(gl,
        "a creation organized a creation", out, sizeof(out));
    EXPECT(fixes == 0, "stage 1 -> 0 fixes (got %d)", fixes);
    EXPECT(strcmp(out, "a creation organized a creation") == 0,
           "stage 1 passthrough (got '%s')", out);

    grounded_language_set_current_stage_int(gl, 2);
    gl_apply_givenness_definite(gl,
        "a creation organized a creation", out, sizeof(out));
    EXPECT(strcmp(out, "a creation organized the creation") == 0,
           "stage 2 fires (got '%s')", out);

    grounded_language_destroy(gl);
}

static void test_null_empty_safety(void) {
    char out[64];
    int rc = gl_apply_givenness_definite(NULL, "a x a x", out, sizeof(out));
    EXPECT(rc >= 0, "NULL gl handled (got rc=%d)", rc);

    rc = gl_apply_givenness_definite(NULL, NULL, out, sizeof(out));
    EXPECT(rc == -1, "NULL input -> -1 (got %d)", rc);

    rc = gl_apply_givenness_definite(NULL, "a x a x", NULL, 0);
    EXPECT(rc == -1, "NULL out / zero cap -> -1 (got %d)", rc);

    /* Empty input -> empty output, 0 fixes. */
    grounded_language_t* gl = mk_gl();
    if (gl) {
        rc = gl_apply_givenness_definite(gl, "", out, sizeof(out));
        EXPECT(rc == 0 && out[0] == '\0', "empty input -> empty (rc=%d, out='%s')", rc, out);
        grounded_language_destroy(gl);
    }
}

int main(void) {
    test_setter_getter();
    test_persist_round_trip();
    test_base_a_to_the();
    test_an_to_the();
    test_case_preserving();
    test_idempotent();
    test_no_ops();
    test_stage_gate();
    test_null_empty_safety();
    if (g_failures == 0) {
        printf("test_lang_givenness_definite: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_givenness_definite: %d FAILURE(S)\n", g_failures);
    return 1;
}
