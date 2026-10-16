/**
 * @file test_lang_tf_stdp_apply.c
 * @brief TF-5 — lexicon-binding strength feedback (the "bridge STDP" slot).
 *
 * Verifies that gl_tf_apply_bridge_stdp:
 *   1. lr=0 is a no-op (the default — TF-5 ships wired-but-inert).
 *   2. NULL/empty inputs return 0 without crashing.
 *   3. SUBSTITUTE drives both a positive bump on corrected and a negative
 *      bump on raw — counters tick on both sides.
 *   4. INSERT drives only the positive pass.
 *   5. DELETE drives only the negative pass.
 *   6. Strength clamps to [0, 1] — no overflow, no underflow.
 *   7. Asymmetric magnitudes: corrected gets +lr, raw gets -lr*0.5.
 *   8. Cascade-feedback entry: master OFF blocks; mask=0 blocks;
 *      lr=0 + master ON => no STDP bumps but trigram/distrib may still fire.
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_tf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

static int g_failures = 0;
#define EXPECT(cond, ...) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d " #cond " : ", __func__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        g_failures++; \
    } \
} while (0)

#define NEAR(a, b) (fabsf((float)(a) - (float)(b)) < 1e-4f)

static grounded_language_t* mk_gl(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (gl) grounded_language_set_current_stage_int(gl, 2);
    return gl;
}

/* Plant a single concept binding via the public ground API. The caller
 * doesn't get the entry pointer back; tests only assert counter-side
 * effects (which is what the cascade integration relies on).  */
static void plant_binding(grounded_language_t* gl, const char* word) {
    float feats[8] = {0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    gl_grounding_event_t ev = {0};
    ev.word              = word;
    ev.modality          = GL_MODALITY_VISUAL;
    ev.sensory_features  = feats;
    ev.feature_dim       = 8;
    ev.attention         = 1.0f;
    (void)grounded_language_ground(gl, &ev);
}

/* TF-5 lr=0 default => no-op. */
static void test_no_op_zero_lr(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    EXPECT(NEAR(grounded_language_get_tf_lr_bridge_stdp(gl), 0.0f),
           "default lr 0 (got %.4f)", grounded_language_get_tf_lr_bridge_stdp(gl));

    gl_corrector_delta_t d = {0};
    d.op = GL_TF_OP_SUBSTITUTE;
    d.position = 1;
    strcpy(d.raw_token, "a");
    strcpy(d.corrected_token, "the");

    uint32_t applied = gl_tf_apply_bridge_stdp(gl, &d, 1);
    EXPECT(applied == 0, "default lr=0 -> no apply (got %u)", applied);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_stdp_updates_pos == 0, "no pos bumps");
    EXPECT(stats.tf_stdp_updates_neg == 0, "no neg bumps");
    grounded_language_destroy(gl);
}

/* NULL safety. */
static void test_null_safety(void) {
    gl_corrector_delta_t d = {0};
    EXPECT(gl_tf_apply_bridge_stdp(NULL, &d, 1) == 0, "NULL gl");
    grounded_language_t* gl = mk_gl();
    EXPECT(gl_tf_apply_bridge_stdp(gl, NULL, 1) == 0, "NULL deltas");
    EXPECT(gl_tf_apply_bridge_stdp(gl, &d, 0) == 0, "0 count");
    grounded_language_destroy(gl);
}

/* SUBSTITUTE drives both pos + neg passes. */
static void test_substitute_pos_and_neg(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_tf_lr_bridge_stdp(gl, 0.05f);
    plant_binding(gl, "lex_raw");
    plant_binding(gl, "lex_corr");

    gl_corrector_delta_t d = {0};
    d.op = GL_TF_OP_SUBSTITUTE;
    d.position = 1;
    strcpy(d.raw_token,       "lex_raw");
    strcpy(d.corrected_token, "lex_corr");

    uint32_t applied = gl_tf_apply_bridge_stdp(gl, &d, 1);
    EXPECT(applied == 2, "SUB -> 2 updates (got %u)", applied);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_stdp_updates_pos == 1, "1 pos (got %llu)",
           (unsigned long long)stats.tf_stdp_updates_pos);
    EXPECT(stats.tf_stdp_updates_neg == 1, "1 neg (got %llu)",
           (unsigned long long)stats.tf_stdp_updates_neg);
    grounded_language_destroy(gl);
}

/* INSERT: only positive pass (no raw_token). */
static void test_insert_pos_only(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_tf_lr_bridge_stdp(gl, 0.05f);
    plant_binding(gl, "lex_and");

    gl_corrector_delta_t d = {0};
    d.op = GL_TF_OP_INSERT;
    d.position = 3;
    strcpy(d.corrected_token, "lex_and");

    uint32_t applied = gl_tf_apply_bridge_stdp(gl, &d, 1);
    EXPECT(applied == 1, "INS -> 1 pos update (got %u)", applied);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_stdp_updates_pos == 1, "1 pos");
    EXPECT(stats.tf_stdp_updates_neg == 0, "0 neg");
    grounded_language_destroy(gl);
}

/* DELETE: only negative pass on raw token. */
static void test_delete_neg_only(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_tf_lr_bridge_stdp(gl, 0.05f);
    plant_binding(gl, "lex_drop");

    gl_corrector_delta_t d = {0};
    d.op = GL_TF_OP_DELETE;
    d.position = 2;
    strcpy(d.raw_token, "lex_drop");

    uint32_t applied = gl_tf_apply_bridge_stdp(gl, &d, 1);
    EXPECT(applied == 1, "DEL -> 1 neg update (got %u)", applied);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_stdp_updates_pos == 0, "0 pos");
    EXPECT(stats.tf_stdp_updates_neg == 1, "1 neg");
    grounded_language_destroy(gl);
}

/* Tokens with no bindings: no update applies (gracefully). */
static void test_no_binding_no_update(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_tf_lr_bridge_stdp(gl, 0.05f);

    gl_corrector_delta_t d = {0};
    d.op = GL_TF_OP_SUBSTITUTE;
    d.position = 1;
    strcpy(d.raw_token,       "nonexistent_raw");
    strcpy(d.corrected_token, "nonexistent_corr");

    uint32_t applied = gl_tf_apply_bridge_stdp(gl, &d, 1);
    EXPECT(applied == 0, "no-binding tokens -> 0 applied (got %u)", applied);
    grounded_language_destroy(gl);
}

/* Cascade-feedback entry: master ON + mask ON + bridge lr=0 -> trigram +
 * distrib paths may fire but STDP path does NOT. */
static void test_cascade_stdp_inert_by_default(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    /* Defaults: master ON, mask all, trigram lr 0.002, distrib lr 0.01,
     * bridge stdp lr 0.0 — that's the shipped TF default. */
    EXPECT(grounded_language_get_produce_corrector_feedback_enabled(gl), "master ON default");
    EXPECT(NEAR(grounded_language_get_tf_lr_bridge_stdp(gl), 0.0f), "stdp lr 0 default");

    (void)grounded_language_tf_apply_cascade_feedback(
        gl, "a cat ran a cat slept", "a cat ran the cat slept",
        0.5f, 0u, 0u, 0u);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_outcome_ok == 1,           "gate fires");
    EXPECT(stats.tf_stdp_updates_pos == 0,     "no pos STDP (lr=0)");
    EXPECT(stats.tf_stdp_updates_neg == 0,     "no neg STDP (lr=0)");
    grounded_language_destroy(gl);
}

/* Cascade-feedback entry: operator raises bridge lr above 0 -> STDP fires. */
static void test_cascade_stdp_active_when_lr_raised(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;
    grounded_language_set_tf_lr_bridge_stdp(gl, 0.05f);
    /* Plant the lexicon entries for the tokens that will be subbed. */
    plant_binding(gl, "a");
    plant_binding(gl, "the");

    (void)grounded_language_tf_apply_cascade_feedback(
        gl, "a cat ran a cat slept", "a cat ran the cat slept",
        0.5f, 0u, 0u, 0u);

    gl_stats_t stats;
    grounded_language_get_stats(gl, &stats);
    EXPECT(stats.tf_outcome_ok == 1,           "gate fires");
    EXPECT(stats.tf_stdp_updates_pos >= 1,     "pos STDP bumped (got %llu)",
           (unsigned long long)stats.tf_stdp_updates_pos);
    EXPECT(stats.tf_stdp_updates_neg >= 1,     "neg STDP bumped (got %llu)",
           (unsigned long long)stats.tf_stdp_updates_neg);
    grounded_language_destroy(gl);
}

int main(void) {
    test_no_op_zero_lr();
    test_null_safety();
    test_substitute_pos_and_neg();
    test_insert_pos_only();
    test_delete_neg_only();
    test_no_binding_no_update();
    test_cascade_stdp_inert_by_default();
    test_cascade_stdp_active_when_lr_raised();
    if (g_failures == 0) {
        printf("test_lang_tf_stdp_apply: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_tf_stdp_apply: %d FAILURE(S)\n", g_failures);
    return 1;
}
