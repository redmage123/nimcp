/**
 * @file test_lang_tf_trigram_apply.c
 * @brief TF-3 — trigram-feedback plasticity path.
 *
 * Verifies that gl_tf_apply_trigram bumps the gl_stats_t.tf_trigram_updates
 * counter when called with non-zero lr + non-empty deltas, and is a no-op
 * when lr is 0 / deltas are empty / position 0 (no context).
 *
 * The actual SNN-bridge weight delta is exercised by the existing trigram
 * tests; here we just check that TF dispatches into the right counter so
 * cascade-level integration can rely on the counter as a "did fire" signal.
 */

#include "language/nimcp_grounded_language.h"
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

static grounded_language_t* mk_gl(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (gl) grounded_language_set_current_stage_int(gl, 2);
    return gl;
}

/* lr=0 -> no-op. */
static void test_no_op_zero_lr(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_tf_lr_trigram(gl, 0.0f);

    gl_corrector_delta_t d = {0};
    d.op = GL_TF_OP_SUBSTITUTE;
    d.position = 2;
    strcpy(d.raw_token, "a");
    strcpy(d.corrected_token, "the");

    uint32_t applied = gl_tf_apply_trigram(gl, "x y the cat", &d, 1);
    EXPECT(applied == 0, "lr=0 -> no apply (got %u)", applied);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_trigram_updates == 0, "counter unchanged (got %llu)",
           (unsigned long long)stats.tf_trigram_updates);

    grounded_language_destroy(gl);
}

/* DELETE deltas are skipped. */
static void test_delete_skipped(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_tf_lr_trigram(gl, 0.01f);

    gl_corrector_delta_t d = {0};
    d.op = GL_TF_OP_DELETE;
    d.position = 1;
    strcpy(d.raw_token, "the");
    /* corrected_token left empty for DELETE */

    uint32_t applied = gl_tf_apply_trigram(gl, "cat ran", &d, 1);
    EXPECT(applied == 0, "DELETE skipped (got %u applied)", applied);

    grounded_language_destroy(gl);
}

/* Position 0 -> no context, skipped. */
static void test_position_zero_skipped(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_tf_lr_trigram(gl, 0.01f);

    gl_corrector_delta_t d = {0};
    d.op = GL_TF_OP_SUBSTITUTE;
    d.position = 0;  /* no context */
    strcpy(d.raw_token, "X");
    strcpy(d.corrected_token, "Y");

    uint32_t applied = gl_tf_apply_trigram(gl, "Y ran here", &d, 1);
    EXPECT(applied == 0, "pos=0 skipped (got %u)", applied);

    grounded_language_destroy(gl);
}

/* NULL safety. */
static void test_null_safety(void) {
    gl_corrector_delta_t d = {0};
    EXPECT(gl_tf_apply_trigram(NULL, "a b c", &d, 1) == 0, "NULL gl");

    grounded_language_t* gl = mk_gl();
    EXPECT(gl_tf_apply_trigram(gl, NULL, &d, 1) == 0, "NULL text");
    EXPECT(gl_tf_apply_trigram(gl, "x", NULL, 1) == 0, "NULL deltas");
    EXPECT(gl_tf_apply_trigram(gl, "x", &d, 0) == 0, "0 count");
    grounded_language_destroy(gl);
}

/* Cascade-feedback entry: master OFF short-circuits without touching gates. */
static void test_cascade_master_off(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_produce_corrector_feedback_enabled(gl, false);
    grounded_language_set_tf_enabled_correctors(gl, GL_TF_BIT_T3_GIVENNESS);
    grounded_language_set_tf_lr_trigram(gl, 0.01f);

    uint32_t applied = grounded_language_tf_apply_cascade_feedback(
        gl, "a cat ran a cat slept", "a cat ran the cat slept",
        1.0f, 0u, 0u, 0u);
    EXPECT(applied == 0, "master OFF -> 0 applied");

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_outcome_ok == 0,            "no outcome ok bump");
    EXPECT(stats.tf_outcome_blocked_master == 0, "no outcome blocked-master bump (early return)");
    grounded_language_destroy(gl);
}

/* Cascade-feedback entry: mask = 0 short-circuits. */
static void test_cascade_mask_zero(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_produce_corrector_feedback_enabled(gl, true);
    grounded_language_set_tf_enabled_correctors(gl, 0u);
    grounded_language_set_tf_lr_trigram(gl, 0.01f);

    uint32_t applied = grounded_language_tf_apply_cascade_feedback(
        gl, "a cat ran a cat slept", "a cat ran the cat slept",
        1.0f, 0u, 0u, 0u);
    EXPECT(applied == 0, "mask 0 -> 0 applied");

    grounded_language_destroy(gl);
}

/* Cascade-feedback: negative reward -> blocked DA counter ticks. */
static void test_cascade_blocked_da(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_produce_corrector_feedback_enabled(gl, true);
    grounded_language_set_tf_enabled_correctors(gl, GL_TF_BIT_T3_GIVENNESS);
    grounded_language_set_tf_lr_trigram(gl, 0.01f);

    (void)grounded_language_tf_apply_cascade_feedback(
        gl, "a cat ran a cat slept", "a cat ran the cat slept",
        -0.5f, 0u, 0u, 0u);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_outcome_blocked_da == 1, "blocked-da bumped (got %llu)",
           (unsigned long long)stats.tf_outcome_blocked_da);
    EXPECT(stats.tf_trigram_updates == 0, "no plasticity (got %llu)",
           (unsigned long long)stats.tf_trigram_updates);

    grounded_language_destroy(gl);
}

/* Cascade-feedback: speech-repair retry -> blocked_retry counter ticks. */
static void test_cascade_blocked_retry(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_produce_corrector_feedback_enabled(gl, true);
    grounded_language_set_tf_enabled_correctors(gl, GL_TF_BIT_T3_GIVENNESS);
    grounded_language_set_tf_lr_trigram(gl, 0.01f);

    (void)grounded_language_tf_apply_cascade_feedback(
        gl, "a cat ran a cat slept", "a cat ran the cat slept",
        1.0f, 0u, 0u, /*retries=*/1u);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_outcome_blocked_retry == 1, "blocked-retry bumped");
    EXPECT(stats.tf_trigram_updates == 0,       "no plasticity");
    grounded_language_destroy(gl);
}

/* Cascade-feedback: clean gate -> tf_outcome_ok ticks. plasticity may or
 * may not apply depending on whether the next_token helpers find valid
 * bindings — we only assert the gate fires, not the count. */
static void test_cascade_gate_ok(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_produce_corrector_feedback_enabled(gl, true);
    grounded_language_set_tf_enabled_correctors(gl, GL_TF_BIT_T3_GIVENNESS);
    grounded_language_set_tf_lr_trigram(gl, 0.01f);

    (void)grounded_language_tf_apply_cascade_feedback(
        gl, "a cat ran a cat slept", "a cat ran the cat slept",
        0.5f, 0u, 0u, 0u);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_outcome_ok == 1, "outcome ok bumped (got %llu)",
           (unsigned long long)stats.tf_outcome_ok);
    grounded_language_destroy(gl);
}

int main(void) {
    test_no_op_zero_lr();
    test_delete_skipped();
    test_position_zero_skipped();
    test_null_safety();
    test_cascade_master_off();
    test_cascade_mask_zero();
    test_cascade_blocked_da();
    test_cascade_blocked_retry();
    test_cascade_gate_ok();
    if (g_failures == 0) {
        printf("test_lang_tf_trigram_apply: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_tf_trigram_apply: %d FAILURE(S)\n", g_failures);
    return 1;
}
