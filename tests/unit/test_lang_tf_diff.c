/**
 * @file test_lang_tf_diff.c
 * @brief TF-1 — LCS-aligned cascade delta capture (observation only).
 *
 * Covers gl_tf_diff_correctors over the operation types Tier 1-3 correctors
 * actually produce in practice:
 *   1. identical inputs           -> 0 deltas
 *   2. F3-style substitution      -> 1 SUBSTITUTE delta
 *   3. T2-1-style noun + det drop -> 1 SUBSTITUTE + 1 DELETE
 *   4. T3-1-style a -> the swap   -> 1 SUBSTITUTE
 *   5. T3-2-style 'and' insertion -> 1 INSERT
 *   6. multi-delta utterance      -> several deltas in oldest-first order
 *   7. NULL / cap=0 / empty       -> 0 deltas, no crash
 *   8. truncation past cap        -> deltas[] holds first `cap`, return reflects what fit
 *
 * The plasticity wiring (TF-2..TF-5) is not exercised here — that's the
 * point of TF-1: the diff lands first as observation-only so we can verify
 * shape, attribution, and counters before any plasticity touches the NN.
 */

#include "language/nimcp_grounded_language_tf.h"

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

static const char* op_name(gl_tf_op_t op) {
    switch (op) {
        case GL_TF_OP_SUBSTITUTE: return "SUB";
        case GL_TF_OP_INSERT:     return "INS";
        case GL_TF_OP_DELETE:     return "DEL";
        default: return "?";
    }
}

static void dump(const char* tag, const gl_corrector_delta_t* d, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        fprintf(stderr, "  [%s] %s @%u  raw='%s' corr='%s'\n",
                tag, op_name(d[i].op), d[i].position,
                d[i].raw_token, d[i].corrected_token);
    }
}

/* 1. identical strings -> no deltas */
static void test_identical(void) {
    gl_corrector_delta_t d[16] = {0};
    uint32_t n = gl_tf_diff_correctors("the cat ran", "the cat ran",
                                       d, 16, GL_TF_SRC_AGGREGATE);
    EXPECT(n == 0, "identical -> 0 deltas (got %u)", n);
}

/* 2. single-word substitute (F3 verb form: was -> were) */
static void test_substitute_one(void) {
    gl_corrector_delta_t d[16] = {0};
    uint32_t n = gl_tf_diff_correctors("they was here", "they were here",
                                       d, 16, GL_TF_SRC_F3_AGREEMENT);
    EXPECT(n == 1, "one substitute (got %u)", n);
    if (n >= 1) {
        EXPECT(d[0].op == GL_TF_OP_SUBSTITUTE, "op=SUB (got %s)", op_name(d[0].op));
        EXPECT(d[0].position == 1, "pos=1 (got %u)", d[0].position);
        EXPECT(strcmp(d[0].raw_token, "was") == 0, "raw=was (got '%s')", d[0].raw_token);
        EXPECT(strcmp(d[0].corrected_token, "were") == 0, "corr=were (got '%s')", d[0].corrected_token);
        EXPECT(d[0].source == GL_TF_SRC_F3_AGREEMENT, "source stamped");
    }
}

/* 3. T2-1 pronominalization: "the cat ran the cat slept" -> "the cat ran it slept".
 * The determiner gets DROPPED and the noun gets SUBSTITUTED. */
static void test_pronominalize_pattern(void) {
    gl_corrector_delta_t d[16] = {0};
    uint32_t n = gl_tf_diff_correctors("the cat ran the cat slept",
                                       "the cat ran it slept",
                                       d, 16, GL_TF_SRC_T2_PRONOMINALIZE);
    /* Expected: 1 DEL ("the") + 1 SUB ("cat" -> "it"). The LCS may emit
     * these in either order depending on tie-breaks — verify by op set. */
    if (n != 2) { dump("pron", d, n); }
    EXPECT(n == 2, "expected 2 deltas (got %u)", n);
    int del_count = 0, sub_count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (d[i].op == GL_TF_OP_DELETE) {
            del_count++;
            EXPECT(strcmp(d[i].raw_token, "the") == 0, "DEL raw=the (got '%s')", d[i].raw_token);
        } else if (d[i].op == GL_TF_OP_SUBSTITUTE) {
            sub_count++;
            EXPECT(strcmp(d[i].raw_token, "cat") == 0, "SUB raw=cat (got '%s')", d[i].raw_token);
            EXPECT(strcmp(d[i].corrected_token, "it") == 0, "SUB corr=it (got '%s')", d[i].corrected_token);
        }
    }
    EXPECT(del_count == 1 && sub_count == 1, "1 DEL + 1 SUB (got %d/%d)", del_count, sub_count);
}

/* 4. T3-1 givenness: "a cat ran a cat slept" -> "a cat ran the cat slept".
 * One SUBSTITUTE on the second "a" position. */
static void test_givenness_pattern(void) {
    gl_corrector_delta_t d[16] = {0};
    uint32_t n = gl_tf_diff_correctors("a cat ran a cat slept",
                                       "a cat ran the cat slept",
                                       d, 16, GL_TF_SRC_T3_GIVENNESS);
    EXPECT(n == 1, "one delta (got %u)", n);
    if (n >= 1) {
        EXPECT(d[0].op == GL_TF_OP_SUBSTITUTE, "op=SUB");
        EXPECT(strcmp(d[0].raw_token, "a") == 0, "raw=a");
        EXPECT(strcmp(d[0].corrected_token, "the") == 0, "corr=the");
    }
}

/* 5. T3-2 conjunction: "the cat ran the dog slept" -> "the cat ran and the dog slept".
 * One INSERT for "and". */
static void test_insert_pattern(void) {
    gl_corrector_delta_t d[16] = {0};
    uint32_t n = gl_tf_diff_correctors("the cat ran the dog slept",
                                       "the cat ran and the dog slept",
                                       d, 16, GL_TF_SRC_T3_CONJUNCTION);
    EXPECT(n == 1, "one insert (got %u)", n);
    if (n >= 1) {
        EXPECT(d[0].op == GL_TF_OP_INSERT, "op=INS (got %s)", op_name(d[0].op));
        EXPECT(strcmp(d[0].corrected_token, "and") == 0,
               "corr=and (got '%s')", d[0].corrected_token);
        EXPECT(d[0].position == 3, "pos=3 (got %u)", d[0].position);
    }
}

/* 6. Multi-delta: substitute + insert in oldest-first order. */
static void test_multi_delta(void) {
    gl_corrector_delta_t d[16] = {0};
    uint32_t n = gl_tf_diff_correctors("they was here the cat slept",
                                       "they were here and the cat slept",
                                       d, 16, GL_TF_SRC_AGGREGATE);
    if (n != 2) { dump("multi", d, n); }
    EXPECT(n == 2, "expected 2 deltas (got %u)", n);
    if (n >= 2) {
        /* Oldest-first: the SUB at position 1 must come before the INS at position 3. */
        EXPECT(d[0].position < d[1].position,
               "oldest-first ordering: %u < %u", d[0].position, d[1].position);
    }
}

/* 7. NULL safety and zero-cap. */
static void test_null_safety(void) {
    gl_corrector_delta_t d[4] = {0};
    EXPECT(gl_tf_diff_correctors(NULL, "x", d, 4, GL_TF_SRC_AGGREGATE) == 0, "NULL raw");
    EXPECT(gl_tf_diff_correctors("x", NULL, d, 4, GL_TF_SRC_AGGREGATE) == 0, "NULL corrected");
    EXPECT(gl_tf_diff_correctors("x", "y", NULL, 4, GL_TF_SRC_AGGREGATE) == 0, "NULL deltas");
    EXPECT(gl_tf_diff_correctors("x", "y", d, 0, GL_TF_SRC_AGGREGATE) == 0, "cap=0");
    EXPECT(gl_tf_diff_correctors("", "", d, 4, GL_TF_SRC_AGGREGATE) == 0, "both empty");
}

/* 8. Cap truncation — the diff yields N deltas but we hand it cap=1. */
static void test_cap_truncation(void) {
    gl_corrector_delta_t d[2] = {0};
    uint32_t n = gl_tf_diff_correctors("they was here the cat slept",
                                       "they were here and the cat slept",
                                       d, 1, GL_TF_SRC_AGGREGATE);
    EXPECT(n == 1, "cap=1 truncates (got %u)", n);
}

int main(void) {
    test_identical();
    test_substitute_one();
    test_pronominalize_pattern();
    test_givenness_pattern();
    test_insert_pattern();
    test_multi_delta();
    test_null_safety();
    test_cap_truncation();
    if (g_failures == 0) {
        printf("test_lang_tf_diff: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_tf_diff: %d FAILURE(S)\n", g_failures);
    return 1;
}
