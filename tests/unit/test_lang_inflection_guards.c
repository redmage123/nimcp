/**
 * @file test_lang_inflection_guards.c
 * @brief RC1/RC2 surface-corrector guards (2026-06-01 produce-junk fixes).
 *
 * RC1 — the inflection GENERATORS must not re-suffix an already-inflected
 *       surface form as if it were a base verb. Before the guard, a gerund or
 *       participle fed to the corrector produced "preventinged" (preventing+ed),
 *       "movinged", "headinged", "throwns" (thrown+s).
 *   - gl_morph_past_tense leaves -ing gerunds, -ed pasts and irregular past
 *     participles unchanged (so the F4b caller's strcmp(out,in) check skips
 *     them), while still producing real irregulars (sing->sang) and regulars
 *     (walk->walked, love->loved).
 *   - gl_morph_inflect_3sg leaves -ed/participle forms unchanged (thrown stays
 *     thrown) while still producing real 3sg (walk->walks, sing->sings).
 *
 * RC2 — gl_apply_f4_fluency inserts at most ONE noun-noun possessive 's, so a
 *       produced run of abstract nouns can no longer cascade into
 *       "creation's education's organization's ...".
 *
 * Compile (CMake wires this into lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_inflection_guards.c \
 *       -L build/lib -lnimcp -lm -lpthread -Wl,-rpath,build/lib -o /tmp/test_lang_ig
 */

#include "language/nimcp_grounded_language.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); g_failures++; \
    } \
} while (0)

/* Helper: past tense of `v`. */
static const char* past(const char* v, char* buf, size_t n) {
    gl_morph_past_tense(v, buf, n); return buf;
}
/* Helper: 3sg of `v`. */
static const char* tsg(const char* v, char* buf, size_t n) {
    gl_morph_inflect_3sg(v, buf, n); return buf;
}

static void test_rc1_past_tense_guard(void) {
    char b[64];
    /* Gerunds / participles / already-past: left UNCHANGED (no double-suffix). */
    EXPECT(strcmp(past("preventing", b, sizeof b), "preventing") == 0, "got '%s'", b);
    EXPECT(strcmp(past("moving",     b, sizeof b), "moving")     == 0, "got '%s'", b);
    EXPECT(strcmp(past("heading",    b, sizeof b), "heading")    == 0, "got '%s'", b);
    EXPECT(strcmp(past("using",      b, sizeof b), "using")      == 0, "got '%s'", b);
    EXPECT(strcmp(past("thrown",     b, sizeof b), "thrown")     == 0, "got '%s'", b);
    EXPECT(strcmp(past("prevented",  b, sizeof b), "prevented")  == 0, "got '%s'", b);
    /* Real verbs still inflect: regulars + irregulars. */
    EXPECT(strcmp(past("walk", b, sizeof b), "walked") == 0, "got '%s'", b);
    EXPECT(strcmp(past("love", b, sizeof b), "loved")  == 0, "got '%s'", b);
    EXPECT(strcmp(past("run",  b, sizeof b), "ran")    == 0, "got '%s'", b);
    EXPECT(strcmp(past("sing", b, sizeof b), "sang")   == 0, "got '%s'", b);
}

static void test_rc1_3sg_guard(void) {
    char b[64];
    /* Participles / already-past: left UNCHANGED (no thrown->throwns). */
    EXPECT(strcmp(tsg("thrown",    b, sizeof b), "thrown")    == 0, "got '%s'", b);
    EXPECT(strcmp(tsg("given",     b, sizeof b), "given")     == 0, "got '%s'", b);
    EXPECT(strcmp(tsg("prevented", b, sizeof b), "prevented") == 0, "got '%s'", b);
    /* Real verbs still get 3sg -s (including -ing-lettered base verbs). */
    EXPECT(strcmp(tsg("walk", b, sizeof b), "walks") == 0, "got '%s'", b);
    EXPECT(strcmp(tsg("sing", b, sizeof b), "sings") == 0, "got '%s'", b);
}

static int count_apostrophe_s(const char* s) {
    int c = 0;
    for (const char* p = s; *p; p++)
        if (p[0] == '\'' && (p[1] == 's' || p[1] == 'S')) c++;
    return c;
}

static void test_rc2_possessive_cap(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create gl"); if (!gl) return;
    grounded_language_set_current_stage_int(gl, 2);  /* F4 gate is stage>=2 */

    char out[256];
    /* A run of -tion abstract nouns (classifier tags all NOUN). Before RC2 this
     * cascaded a possessive onto every adjacent pair; now at most one. */
    gl_apply_f4_fluency(gl, "creation education organization information", out, sizeof out);
    int poss = count_apostrophe_s(out);
    EXPECT(poss <= 1, "possessive cascade not capped: %d in '%s'", poss, out);

    grounded_language_destroy(gl);
}

int main(void) {
    test_rc1_past_tense_guard();
    test_rc1_3sg_guard();
    test_rc2_possessive_cap();
    if (g_failures == 0) {
        printf("test_lang_inflection_guards: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_inflection_guards: %d FAILURE(S)\n", g_failures);
    return 1;
}
