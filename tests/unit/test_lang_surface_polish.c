/**
 * @file test_lang_surface_polish.c
 * @brief UNIT — Tier 1 Step F1: capitalize + terminal-punctuate the final
 *        cascade utterance.
 *
 * cascade_apply_surface_polish() rewrites state->utterance so the returned
 * text reads as written language: first letter capitalized, terminal
 * punctuation appended. The terminal mark is classified from the
 * UTTERANCE's own leading word (wh-word / auxiliary → '?', else '.'), NOT
 * the prompt — a declarative answer to a question still ends in '.'.
 *
 * Guards:
 *   1. Plain statement     → capitalized + '.'
 *   2. wh-led output       → capitalized + '?'
 *   3. auxiliary-led output→ capitalized + '?'
 *   4. Already punctuated  → idempotent (no double terminal), still capitalized
 *   5. Trailing whitespace → trimmed before the terminal
 *   6. NULL / empty        → no crash, no spurious allocation
 *
 * The utterance must be heap-allocated with nimcp_calloc because the
 * function nimcp_free()s the old buffer and installs a new one.
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_surface_polish.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,/home/bbrelin/nimcp/build/lib \
 *       -o /tmp/test_lang_surface_polish
 */

#include "language/nimcp_communication_cascade.h"
#include "utils/memory/nimcp_memory.h"

#include <stdint.h>
#include <stdio.h>
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

/* Heap-dup with nimcp_calloc so the function's nimcp_free matches. */
static char* nimcp_dup(const char* s) {
    size_t n = strlen(s);
    char* p = (char*)nimcp_calloc(n + 1, 1);
    if (p) memcpy(p, s, n);
    return p;
}

/* Run polish on `in`, return the polished string (caller must NOT free —
 * we copy it into a static buffer and free the state's). */
static void polish_into(const char* in, char* out, size_t out_cap) {
    production_cascade_state_t st;
    memset(&st, 0, sizeof(st));
    st.utterance = in ? nimcp_dup(in) : NULL;
    cascade_apply_surface_polish(&st);
    if (st.utterance) {
        snprintf(out, out_cap, "%s", st.utterance);
        nimcp_free(st.utterance);
    } else {
        out[0] = '\0';
    }
}

static void test_statement_gets_period(void) {
    char out[256];
    polish_into("the cat sits on the mat", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "The cat sits on the mat.") == 0,
           "statement -> capitalized + period, got '%s'", out);
}

static void test_wh_gets_question(void) {
    char out[256];
    polish_into("where is the cat", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "Where is the cat?") == 0,
           "wh-led -> capitalized + '?', got '%s'", out);
}

static void test_aux_gets_question(void) {
    char out[256];
    polish_into("are you there", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "Are you there?") == 0,
           "aux-led -> capitalized + '?', got '%s'", out);
}

static void test_idempotent_terminal(void) {
    char out[256];
    /* Already ends with '!': keep it, no extra terminal, still capitalize. */
    polish_into("hello world!", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "Hello world!") == 0,
           "existing terminal preserved, got '%s'", out);
    /* Already a question mark. */
    polish_into("what now?", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "What now?") == 0,
           "existing '?' preserved, got '%s'", out);
}

static void test_trailing_whitespace_trimmed(void) {
    char out[256];
    polish_into("the dog runs   ", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "The dog runs.") == 0,
           "trailing ws trimmed before terminal, got '%s'", out);
}

/* Tier 2 coherence touch-up: interior sentence starts (after a terminal mark
 * + space) get capitalized too — the multi-sentence output that produce-side
 * pronominalization now generates reads correctly. */
static void test_interior_sentence_caps(void) {
    char out[256];
    polish_into("the creation organized. it activated", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "The creation organized. It activated.") == 0,
           "interior sentence start capitalized, got '%s'", out);

    /* Three sentences, declarative-led (whole-utterance terminal is '.').
     * Both interior starts get capitalized. */
    polish_into("the cat sleeps. it dreams. the dog waits", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "The cat sleeps. It dreams. The dog waits.") == 0,
           "two interior caps, got '%s'", out);

    /* No internal space after a dot (abbreviation-like) must NOT trigger. */
    polish_into("u.s.a is big", out, sizeof(out));
    fprintf(stderr, "  '%s'\n", out);
    EXPECT(strcmp(out, "U.s.a is big.") == 0,
           "no-space dot left alone, got '%s'", out);
}

static void test_null_and_empty(void) {
    production_cascade_state_t st;
    memset(&st, 0, sizeof(st));
    st.utterance = NULL;
    cascade_apply_surface_polish(&st);  /* must not crash */
    EXPECT(st.utterance == NULL, "NULL utterance stays NULL");

    st.utterance = nimcp_dup("");
    cascade_apply_surface_polish(&st);
    EXPECT(st.utterance && st.utterance[0] == '\0', "empty stays empty");
    if (st.utterance) nimcp_free(st.utterance);
}

int main(void) {
    fprintf(stderr, "[UNIT] test_lang_surface_polish (Tier 1 Step F1)\n");
    test_statement_gets_period();
    test_wh_gets_question();
    test_aux_gets_question();
    test_idempotent_terminal();
    test_trailing_whitespace_trimmed();
    test_interior_sentence_caps();
    test_null_and_empty();

    if (g_failures == 0) {
        fprintf(stderr, "OK — surface polish guards passed\n");
        return 0;
    }
    fprintf(stderr, "FAIL — %d failure(s)\n", g_failures);
    return 1;
}
