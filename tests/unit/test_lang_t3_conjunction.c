/**
 * @file test_lang_t3_conjunction.c
 * @brief T3-2 — cohesive conjunction insertion between adjacent SVO clauses.
 *
 * Verifies that gl_apply_t3_conjunction:
 *   1. Default flag is ON (per "default should be on" directive).
 *   2. Stage-gates: at stage<2 the output is copied unchanged regardless
 *      of the input pattern.
 *   3. Inserts "and" between adjacent SVO clauses ("cat ran dog jumped"
 *      -> "cat ran and dog jumped").
 *   4. Is idempotent: re-running on the polished output finds "and" at the
 *      boundary and bails.
 *   5. Skips when the boundary token is already a connective (and/but/so/
 *      or/then/while/because/although).
 *   6. Returns 0 (no insert) when the input has only one clause.
 *   7. Returns 0 when the second candidate has no following verb.
 *   8. NULL/empty/tiny inputs are handled gracefully.
 *   9. Setter/getter round-trip the flag.
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

static grounded_language_t* mk_gl_stage2(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (gl) grounded_language_set_current_stage_int(gl, 2);
    return gl;
}

static void test_default_on(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    EXPECT(grounded_language_get_produce_t3_conjunction(gl),
           "default ON");
    grounded_language_destroy(gl);
}

static void test_setter_getter_round_trip(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_produce_t3_conjunction(gl, false);
    EXPECT(!grounded_language_get_produce_t3_conjunction(gl), "set false");
    grounded_language_set_produce_t3_conjunction(gl, true);
    EXPECT(grounded_language_get_produce_t3_conjunction(gl), "set true");
    grounded_language_destroy(gl);
}

static void test_stage_gate(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_current_stage_int(gl, 1);   /* below threshold */
    char out[256];
    const char* in = "the cat ran the dog jumped";
    int n = gl_apply_t3_conjunction(gl, in, out, sizeof(out));
    EXPECT(n == 0, "stage<2 -> 0 inserts (got %d)", n);
    EXPECT(strcmp(out, in) == 0, "stage<2 -> copied unchanged (got '%s')", out);
    grounded_language_destroy(gl);
}

static void test_basic_insert(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    int n = gl_apply_t3_conjunction(gl, "the cat ran the dog jumped",
                                    out, sizeof(out));
    EXPECT(n == 1, "1 insert (got %d)", n);
    EXPECT(strcmp(out, "the cat ran and the dog jumped") == 0,
           "got '%s'", out);
    grounded_language_destroy(gl);
}

static void test_idempotent(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    int n = gl_apply_t3_conjunction(gl, "the cat ran and the dog jumped",
                                    out, sizeof(out));
    EXPECT(n == 0, "re-run is no-op (got %d)", n);
    EXPECT(strcmp(out, "the cat ran and the dog jumped") == 0,
           "got '%s'", out);
    grounded_language_destroy(gl);
}

static void test_existing_connective_skipped(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    const char* CASES[][2] = {
        {"the cat ran but the dog jumped", "the cat ran but the dog jumped"},
        {"the cat ran so the dog jumped",  "the cat ran so the dog jumped"},
        {"the cat ran or the dog jumped",  "the cat ran or the dog jumped"},
        {"the cat ran then the dog jumped","the cat ran then the dog jumped"},
        {"the cat ran because the dog jumped",
         "the cat ran because the dog jumped"},
    };
    for (size_t i = 0; i < sizeof(CASES)/sizeof(CASES[0]); i++) {
        char out[256];
        int n = gl_apply_t3_conjunction(gl, CASES[i][0], out, sizeof(out));
        EXPECT(n == 0, "case[%zu] no insert (got %d)", i, n);
        EXPECT(strcmp(out, CASES[i][1]) == 0,
               "case[%zu] '%s' -> '%s' (want '%s')",
               i, CASES[i][0], out, CASES[i][1]);
    }
    grounded_language_destroy(gl);
}

static void test_single_clause_no_change(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    int n = gl_apply_t3_conjunction(gl, "the cat ran fast", out, sizeof(out));
    EXPECT(n == 0, "single clause -> 0 inserts (got %d)", n);
    EXPECT(strcmp(out, "the cat ran fast") == 0, "got '%s'", out);
    grounded_language_destroy(gl);
}

static void test_no_second_verb_no_change(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    /* "the cat ran the fence" — clause-A verb is "ran", "the fence" is
     * a noun phrase but has NO following verb, so it's the *object* not
     * a new clause. Must NOT insert "and". */
    int n = gl_apply_t3_conjunction(gl, "the cat ran the fence",
                                    out, sizeof(out));
    EXPECT(n == 0, "no second verb -> 0 inserts (got %d)", n);
    EXPECT(strcmp(out, "the cat ran the fence") == 0, "got '%s'", out);
    grounded_language_destroy(gl);
}

static void test_null_and_empty(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    EXPECT(gl_apply_t3_conjunction(gl, NULL, out, sizeof(out)) == -1,
           "NULL in -> -1");
    EXPECT(gl_apply_t3_conjunction(gl, "the cat", NULL, 0) == -1,
           "NULL out -> -1");
    /* tiny inputs that tokenize to <4 tokens just pass through. */
    int n = gl_apply_t3_conjunction(gl, "cat ran", out, sizeof(out));
    EXPECT(n == 0, "2-token in -> 0 inserts (got %d)", n);
    EXPECT(strcmp(out, "cat ran") == 0, "got '%s'", out);
    grounded_language_destroy(gl);
}

static void test_pronoun_subject_second_clause(void) {
    grounded_language_t* gl = mk_gl_stage2();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    char out[256];
    /* "the cat ran he jumped" — second clause starts with a pronoun. */
    int n = gl_apply_t3_conjunction(gl, "the cat ran he jumped",
                                    out, sizeof(out));
    EXPECT(n == 1, "pronoun-led clause B -> 1 insert (got %d)", n);
    EXPECT(strcmp(out, "the cat ran and he jumped") == 0, "got '%s'", out);
    grounded_language_destroy(gl);
}

int main(void) {
    test_default_on();
    test_setter_getter_round_trip();
    test_stage_gate();
    test_basic_insert();
    test_idempotent();
    test_existing_connective_skipped();
    test_single_clause_no_change();
    test_no_second_verb_no_change();
    test_null_and_empty();
    test_pronoun_subject_second_clause();
    if (g_failures == 0) {
        printf("test_lang_t3_conjunction: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_t3_conjunction: %d FAILURE(S)\n", g_failures);
    return 1;
}
