/**
 * @file test_lang_tf_off_no_op.c
 * @brief TF-6 regression — TF master OFF must be a bit-identical no-op.
 *
 * The campaign defaults TF master ON and trigram/distrib lrs above 0. This
 * regression guards against a future change accidentally driving plasticity
 * even when the operator has explicitly flipped master OFF.
 *
 * Approach: snapshot gl_stats_t and the lexicon vocab_size at t=0, drive 50
 * cascade-feedback calls with a known correction pattern, and assert that
 * EVERY field that TF would touch is unchanged:
 *
 *   - tf_calls                    (TF-1 record_diff — not exercised here)
 *   - tf_deltas_captured
 *   - tf_outcome_ok
 *   - tf_outcome_blocked_*        (five fields)
 *   - tf_trigram_updates
 *   - tf_distrib_updates
 *   - tf_stdp_updates_pos
 *   - tf_stdp_updates_neg
 *   - vocab_size                  (no find_or_create -> no entry creation)
 *
 * The non-TF gl state (productions, comprehensions, etc.) is also asserted
 * unchanged since we never call those APIs in this test. This catches the
 * specific class of bug where someone wires the apply-functions into a path
 * that doesn't honor the master flag.
 *
 * Also runs a "mask zero, lrs raised" combination — same expectation: no-op.
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

#define EXPECT_EQ_U64(label, a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "FAIL %s:%d %s: %llu != %llu\n", \
                __func__, __LINE__, (label), \
                (unsigned long long)(a), (unsigned long long)(b)); \
        g_failures++; \
    } \
} while (0)

static const char* RAWS[] = {
    "a cat ran fast",     "a dog jumped high",  "a bird sang loud",
    "a fish swam deep",   "a fox stalked prey",
};
static const char* CORS[] = {
    "the cat ran fast",   "the dog jumped high","the bird sang loud",
    "the fish swam deep", "the fox stalked prey",
};

static grounded_language_t* mk_gl(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    if (gl) grounded_language_set_current_stage_int(gl, 2);
    return gl;
}

static void drive_50_turns(grounded_language_t* gl) {
    for (uint32_t i = 0; i < 50; i++) {
        uint32_t k = i % (sizeof(RAWS)/sizeof(RAWS[0]));
        (void)grounded_language_tf_apply_cascade_feedback(
            gl, RAWS[k], CORS[k], 0.5f, 0, 0, 0);
    }
}

/* TF-1: master OFF -> no counters change (NOT EVEN the blocked-by-master
 * counter, because the cascade entry short-circuits BEFORE the gate eval
 * to keep the master-OFF common path free). */
static void test_master_off_is_bit_identical(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;

    grounded_language_set_produce_corrector_feedback_enabled(gl, false);
    EXPECT(!grounded_language_get_produce_corrector_feedback_enabled(gl),
           "master flipped OFF");

    /* Raise lrs above default so we'd see plasticity if it leaked. */
    grounded_language_set_tf_lr_trigram     (gl, 0.05f);
    grounded_language_set_tf_lr_distrib     (gl, 0.05f);
    grounded_language_set_tf_lr_bridge_stdp (gl, 0.05f);

    gl_stats_t s0;
    grounded_language_get_stats(gl, &s0);

    drive_50_turns(gl);

    gl_stats_t s1;
    grounded_language_get_stats(gl, &s1);

    EXPECT_EQ_U64("tf_calls",                  s1.tf_calls,                  s0.tf_calls);
    EXPECT_EQ_U64("tf_deltas_captured",        s1.tf_deltas_captured,        s0.tf_deltas_captured);
    EXPECT_EQ_U64("tf_outcome_ok",             s1.tf_outcome_ok,             s0.tf_outcome_ok);
    EXPECT_EQ_U64("tf_outcome_blocked_stage",  s1.tf_outcome_blocked_stage,  s0.tf_outcome_blocked_stage);
    EXPECT_EQ_U64("tf_outcome_blocked_master", s1.tf_outcome_blocked_master, s0.tf_outcome_blocked_master);
    EXPECT_EQ_U64("tf_outcome_blocked_da",     s1.tf_outcome_blocked_da,     s0.tf_outcome_blocked_da);
    EXPECT_EQ_U64("tf_outcome_blocked_stale",  s1.tf_outcome_blocked_stale,  s0.tf_outcome_blocked_stale);
    EXPECT_EQ_U64("tf_outcome_blocked_retry",  s1.tf_outcome_blocked_retry,  s0.tf_outcome_blocked_retry);
    EXPECT_EQ_U64("tf_trigram_updates",        s1.tf_trigram_updates,        s0.tf_trigram_updates);
    EXPECT_EQ_U64("tf_distrib_updates",        s1.tf_distrib_updates,        s0.tf_distrib_updates);
    EXPECT_EQ_U64("tf_stdp_updates_pos",       s1.tf_stdp_updates_pos,       s0.tf_stdp_updates_pos);
    EXPECT_EQ_U64("tf_stdp_updates_neg",       s1.tf_stdp_updates_neg,       s0.tf_stdp_updates_neg);

    /* Non-TF state must also be untouched. */
    EXPECT_EQ_U64("vocab_size",                s1.vocab_size,                s0.vocab_size);
    EXPECT_EQ_U64("total_comprehensions",      s1.total_comprehensions,      s0.total_comprehensions);
    EXPECT_EQ_U64("total_productions",         s1.total_productions,         s0.total_productions);
    EXPECT_EQ_U64("total_bindings",            s1.total_bindings,            s0.total_bindings);

    grounded_language_destroy(gl);
}

/* TF-2: mask zero (no correctors enabled) -> same no-op contract. */
static void test_mask_zero_is_bit_identical(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;

    /* Master stays ON. Mask flipped to 0. */
    EXPECT(grounded_language_get_produce_corrector_feedback_enabled(gl),
           "master default ON");
    grounded_language_set_tf_enabled_correctors(gl, 0u);
    EXPECT(grounded_language_get_tf_enabled_correctors(gl) == 0u,
           "mask flipped to 0");

    /* Raise lrs. */
    grounded_language_set_tf_lr_trigram(gl, 0.05f);
    grounded_language_set_tf_lr_distrib(gl, 0.05f);
    grounded_language_set_tf_lr_bridge_stdp(gl, 0.05f);

    gl_stats_t s0;
    grounded_language_get_stats(gl, &s0);

    drive_50_turns(gl);

    gl_stats_t s1;
    grounded_language_get_stats(gl, &s1);

    EXPECT_EQ_U64("tf_calls",                  s1.tf_calls,                  s0.tf_calls);
    EXPECT_EQ_U64("tf_outcome_ok",             s1.tf_outcome_ok,             s0.tf_outcome_ok);
    EXPECT_EQ_U64("tf_outcome_blocked_stage",  s1.tf_outcome_blocked_stage,  s0.tf_outcome_blocked_stage);
    EXPECT_EQ_U64("tf_outcome_blocked_master", s1.tf_outcome_blocked_master, s0.tf_outcome_blocked_master);
    EXPECT_EQ_U64("tf_outcome_blocked_da",     s1.tf_outcome_blocked_da,     s0.tf_outcome_blocked_da);
    EXPECT_EQ_U64("tf_outcome_blocked_stale",  s1.tf_outcome_blocked_stale,  s0.tf_outcome_blocked_stale);
    EXPECT_EQ_U64("tf_outcome_blocked_retry",  s1.tf_outcome_blocked_retry,  s0.tf_outcome_blocked_retry);
    EXPECT_EQ_U64("tf_trigram_updates",        s1.tf_trigram_updates,        s0.tf_trigram_updates);
    EXPECT_EQ_U64("tf_distrib_updates",        s1.tf_distrib_updates,        s0.tf_distrib_updates);
    EXPECT_EQ_U64("tf_stdp_updates_pos",       s1.tf_stdp_updates_pos,       s0.tf_stdp_updates_pos);
    EXPECT_EQ_U64("tf_stdp_updates_neg",       s1.tf_stdp_updates_neg,       s0.tf_stdp_updates_neg);
    EXPECT_EQ_U64("vocab_size",                s1.vocab_size,                s0.vocab_size);

    grounded_language_destroy(gl);
}

/* TF-3: master ON + mask all + all lrs == 0 -> path is reached (outcome_ok
 * bumps) but ZERO plasticity updates because every apply early-outs on lr<=0.
 * Validates that the lr=0 path is the safety floor — not just by convention
 * but by code. */
static void test_zero_lrs_is_no_plasticity(void) {
    grounded_language_t* gl = mk_gl();
    EXPECT(gl != NULL, "create"); if (!gl) return;

    EXPECT(grounded_language_get_produce_corrector_feedback_enabled(gl),
           "master default ON");
    grounded_language_set_tf_lr_trigram     (gl, 0.0f);
    grounded_language_set_tf_lr_distrib     (gl, 0.0f);
    grounded_language_set_tf_lr_bridge_stdp (gl, 0.0f);

    gl_stats_t s0;
    grounded_language_get_stats(gl, &s0);

    drive_50_turns(gl);

    gl_stats_t s1;
    grounded_language_get_stats(gl, &s1);

    /* outcome_ok DID bump — the gate passed. */
    EXPECT(s1.tf_outcome_ok > s0.tf_outcome_ok,
           "outcome_ok bumped (%llu -> %llu)",
           (unsigned long long)s0.tf_outcome_ok,
           (unsigned long long)s1.tf_outcome_ok);

    /* But every plasticity counter stayed flat. */
    EXPECT_EQ_U64("tf_trigram_updates",  s1.tf_trigram_updates,  s0.tf_trigram_updates);
    EXPECT_EQ_U64("tf_distrib_updates",  s1.tf_distrib_updates,  s0.tf_distrib_updates);
    EXPECT_EQ_U64("tf_stdp_updates_pos", s1.tf_stdp_updates_pos, s0.tf_stdp_updates_pos);
    EXPECT_EQ_U64("tf_stdp_updates_neg", s1.tf_stdp_updates_neg, s0.tf_stdp_updates_neg);

    grounded_language_destroy(gl);
}

int main(void) {
    test_master_off_is_bit_identical();
    test_mask_zero_is_bit_identical();
    test_zero_lrs_is_no_plasticity();
    if (g_failures == 0) {
        printf("test_lang_tf_off_no_op: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_tf_off_no_op: %d FAILURE(S)\n", g_failures);
    return 1;
}
