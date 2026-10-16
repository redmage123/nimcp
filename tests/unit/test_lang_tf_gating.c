/**
 * @file test_lang_tf_gating.c
 * @brief TF-2 — Tier-feedback flag round-trip + outcome-gate matrix.
 *
 * Covers:
 *   1. Master / mask / lr setter+getter defaults + clamping (NaN, negative,
 *      over-max → defaults or clamps to per-path ceiling).
 *   2. LANC persistence — all 5 fields survive save / load.
 *   3. Outcome gate matrix:
 *      - default facts (stage=2, master=true, reward=0, ttl=5e6, age=0, retries=0) PASS
 *      - stage<2 -> BLOCKED_STAGE
 *      - master=false -> BLOCKED_MASTER
 *      - reward<0 -> BLOCKED_DA
 *      - ttl>0 and age>ttl -> BLOCKED_STALE
 *      - retries>0 -> BLOCKED_RETRY
 *      - ttl=0 disables staleness check (large age still passes)
 *      - precedence: stage rejects before master before DA before stale before retry
 *
 * Compile (CMake wires this in lang_smoke):
 *   gcc -O0 -g -I include tests/unit/test_lang_tf_gating.c \
 *       -L build/lib -lnimcp -lm -lpthread \
 *       -Wl,-rpath,build/lib -o /tmp/test_lang_tf_gating
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_persistence.h"
#include "language/nimcp_grounded_language_tf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
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

#define NEAR(a, b) (fabsf((float)(a) - (float)(b)) < 1e-5f)

static void test_flag_defaults(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;
    /* TF defaults flipped ON 2026-05-28 — master + all-correctors mask
     * default-on; bridge STDP lr still defaults to 0 for safety. */
    EXPECT(grounded_language_get_produce_corrector_feedback_enabled(gl) == true,
           "master default ON");
    EXPECT(grounded_language_get_tf_enabled_correctors(gl) == GL_TF_BIT_ALL_CORRECTORS,
           "mask default all-correctors");
    EXPECT(NEAR(grounded_language_get_tf_lr_trigram(gl), GL_TF_LR_TRIGRAM_DEFAULT),
           "trigram lr default 0.002, got %.5f", grounded_language_get_tf_lr_trigram(gl));
    EXPECT(NEAR(grounded_language_get_tf_lr_distrib(gl), GL_TF_LR_DISTRIB_DEFAULT),
           "distrib lr default 0.01, got %.5f", grounded_language_get_tf_lr_distrib(gl));
    EXPECT(NEAR(grounded_language_get_tf_lr_bridge_stdp(gl), GL_TF_LR_BRIDGE_STDP_DEFAULT),
           "bridge stdp lr default 0.0 (still safety-gated), got %.5f",
           grounded_language_get_tf_lr_bridge_stdp(gl));
    grounded_language_destroy(gl);
}

static void test_setter_clamping(void) {
    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create"); if (!gl) return;

    /* Master round-trip. */
    grounded_language_set_produce_corrector_feedback_enabled(gl, true);
    EXPECT(grounded_language_get_produce_corrector_feedback_enabled(gl), "master ON after set");
    grounded_language_set_produce_corrector_feedback_enabled(gl, false);
    EXPECT(!grounded_language_get_produce_corrector_feedback_enabled(gl), "master OFF after set");

    /* Mask — high bits stripped to the low GL_TF_BIT_ALL_CORRECTORS bits. */
    grounded_language_set_tf_enabled_correctors(gl, GL_TF_BIT_T3_GIVENNESS | GL_TF_BIT_T2_PRONOMINALIZE);
    EXPECT(grounded_language_get_tf_enabled_correctors(gl) ==
               (GL_TF_BIT_T3_GIVENNESS | GL_TF_BIT_T2_PRONOMINALIZE),
           "mask round-trip");
    grounded_language_set_tf_enabled_correctors(gl, 0xFFFFu);
    EXPECT(grounded_language_get_tf_enabled_correctors(gl) == GL_TF_BIT_ALL_CORRECTORS,
           "mask 0xFFFF clamped to low 5 bits");

    /* lr clamping — negative -> 0, NaN -> 0, > max -> max. */
    grounded_language_set_tf_lr_trigram(gl, -0.5f);
    EXPECT(NEAR(grounded_language_get_tf_lr_trigram(gl), 0.0f), "trig negative -> 0");
    grounded_language_set_tf_lr_trigram(gl, NAN);
    EXPECT(NEAR(grounded_language_get_tf_lr_trigram(gl), 0.0f), "trig NaN -> 0");
    grounded_language_set_tf_lr_trigram(gl, 5.0f);
    EXPECT(NEAR(grounded_language_get_tf_lr_trigram(gl), GL_TF_LR_TRIGRAM_MAX),
           "trig > max clamped to %.3f", GL_TF_LR_TRIGRAM_MAX);

    grounded_language_set_tf_lr_distrib(gl, 5.0f);
    EXPECT(NEAR(grounded_language_get_tf_lr_distrib(gl), GL_TF_LR_DISTRIB_MAX), "distrib clamp");
    grounded_language_set_tf_lr_bridge_stdp(gl, 5.0f);
    EXPECT(NEAR(grounded_language_get_tf_lr_bridge_stdp(gl), GL_TF_LR_BRIDGE_STDP_MAX), "stdp clamp");

    /* NULL safety. */
    grounded_language_set_produce_corrector_feedback_enabled(NULL, true);  /* must not crash */
    grounded_language_set_tf_enabled_correctors(NULL, 0x1);
    grounded_language_set_tf_lr_trigram(NULL, 0.01f);
    EXPECT(grounded_language_get_produce_corrector_feedback_enabled(NULL) == false,
           "NULL getter -> false");
    EXPECT(grounded_language_get_tf_enabled_correctors(NULL) == 0, "NULL getter -> 0");
    EXPECT(NEAR(grounded_language_get_tf_lr_trigram(NULL), GL_TF_LR_TRIGRAM_DEFAULT),
           "NULL lr getter -> default");

    grounded_language_destroy(gl);
}

static void test_persist_round_trip(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/test_lang_tf_%d.bin", (int)getpid());

    grounded_language_t* gl = grounded_language_create(64, NULL);
    EXPECT(gl != NULL, "create save"); if (!gl) return;
    /* Defaults are now ON / all-bits — flip to non-default values BEFORE
     * save so we can prove the saved bytes survive (otherwise post-load
     * default-reasserting would mask a broken round-trip). */
    grounded_language_set_produce_corrector_feedback_enabled(gl, false);
    grounded_language_set_tf_enabled_correctors(gl,
        GL_TF_BIT_T3_GIVENNESS | GL_TF_BIT_T2_PRONOMINALIZE);
    grounded_language_set_tf_lr_trigram(gl, 0.003f);
    grounded_language_set_tf_lr_distrib(gl, 0.02f);
    grounded_language_set_tf_lr_bridge_stdp(gl, 0.04f);
    FILE* f = fopen(path, "wb");
    if (!f) { grounded_language_destroy(gl); return; }
    int rc = grounded_language_save_multiturn_state(gl, f);
    fclose(f);
    EXPECT(rc == 0, "save rc=%d", rc);
    grounded_language_destroy(gl);

    grounded_language_t* gl2 = grounded_language_create(64, NULL);
    if (!gl2) { unlink(path); return; }
    /* Pre-load is the new ON default. */
    EXPECT(grounded_language_get_produce_corrector_feedback_enabled(gl2),
           "master default ON pre-load");
    f = fopen(path, "rb");
    if (!f) { grounded_language_destroy(gl2); unlink(path); return; }
    rc = grounded_language_load_multiturn_state(gl2, f);
    fclose(f);
    EXPECT(rc == 0, "load rc=%d", rc);
    EXPECT(!grounded_language_get_produce_corrector_feedback_enabled(gl2),
           "master OFF post-load (saved value won, not default)");
    EXPECT(grounded_language_get_tf_enabled_correctors(gl2) ==
               (GL_TF_BIT_T3_GIVENNESS | GL_TF_BIT_T2_PRONOMINALIZE),
           "mask round-trip");
    EXPECT(NEAR(grounded_language_get_tf_lr_trigram(gl2), 0.003f), "trig round-trip");
    EXPECT(NEAR(grounded_language_get_tf_lr_distrib(gl2), 0.02f), "distrib round-trip");
    EXPECT(NEAR(grounded_language_get_tf_lr_bridge_stdp(gl2), 0.04f), "stdp round-trip");
    grounded_language_destroy(gl2);
    unlink(path);
}

/* Default-pass facts: stage=2, master=true, reward=0 (neutral OK),
 * ttl=5e6 us (5s), age=0 (fresh), retries=0 (first try). */
static gl_tf_outcome_facts_t default_facts(void) {
    gl_tf_outcome_facts_t f = {0};
    f.stage                = 2;
    f.master_enabled       = true;
    f.last_external_reward = 0.0f;
    f.reward_age_us        = 0u;
    f.reward_ttl_us        = 5000000u;
    f.repair_attempts      = 0u;
    return f;
}

static void test_gate_default_pass(void) {
    gl_tf_outcome_facts_t f = default_facts();
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_OK, "default facts pass");
    EXPECT(gl_tf_outcome_ok(&f),                         "boolean default true");
}

static void test_gate_stage(void) {
    gl_tf_outcome_facts_t f = default_facts();
    f.stage = 1;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_STAGE, "stage<2 blocks");
    f.stage = 0;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_STAGE, "stage=0 blocks");
    f.stage = 2;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_OK, "stage=2 passes");
}

static void test_gate_master(void) {
    gl_tf_outcome_facts_t f = default_facts();
    f.master_enabled = false;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_MASTER, "master off blocks");
}

static void test_gate_da(void) {
    gl_tf_outcome_facts_t f = default_facts();
    f.last_external_reward = -0.1f;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_DA, "neg reward blocks");
    f.last_external_reward = -1.0f;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_DA, "large neg blocks");
    f.last_external_reward = 0.0f;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_OK, "0 reward passes (neutral)");
    f.last_external_reward = 0.5f;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_OK, "+ reward passes");
}

static void test_gate_stale(void) {
    gl_tf_outcome_facts_t f = default_facts();
    f.reward_age_us = 6000000u;  /* 6s, ttl=5s */
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_STALE, "stale > ttl blocks");
    f.reward_age_us = 5000000u;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_OK, "age == ttl passes");
    f.reward_age_us = 9999999u;
    f.reward_ttl_us = 0u;  /* disable staleness */
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_OK, "ttl=0 disables stale check");
}

static void test_gate_retry(void) {
    gl_tf_outcome_facts_t f = default_facts();
    f.repair_attempts = 1u;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_RETRY, "retry blocks");
    f.repair_attempts = 0u;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_OK, "retry=0 passes");
}

static void test_gate_null(void) {
    EXPECT(gl_tf_outcome_ok_reason(NULL) == GL_TF_OUTCOME_BLOCKED_MASTER, "NULL -> blocked_master");
    EXPECT(!gl_tf_outcome_ok(NULL), "NULL bool false");
}

/* Precedence: when multiple checks would fail, the gate returns the first
 * failing reason. Verifies the order stage -> master -> DA -> stale -> retry. */
static void test_gate_precedence(void) {
    gl_tf_outcome_facts_t f = default_facts();
    f.stage = 0;
    f.master_enabled = false;
    f.last_external_reward = -1.0f;
    f.reward_age_us = 9999999u;
    f.repair_attempts = 5u;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_STAGE,
           "stage rejects first");
    f.stage = 2;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_MASTER,
           "master rejects second");
    f.master_enabled = true;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_DA,
           "DA rejects third");
    f.last_external_reward = 0.0f;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_STALE,
           "stale rejects fourth");
    f.reward_age_us = 0u;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_BLOCKED_RETRY,
           "retry rejects last");
    f.repair_attempts = 0u;
    EXPECT(gl_tf_outcome_ok_reason(&f) == GL_TF_OUTCOME_OK, "all clean passes");
}

int main(void) {
    test_flag_defaults();
    test_setter_clamping();
    test_persist_round_trip();
    test_gate_default_pass();
    test_gate_stage();
    test_gate_master();
    test_gate_da();
    test_gate_stale();
    test_gate_retry();
    test_gate_null();
    test_gate_precedence();
    if (g_failures == 0) {
        printf("test_lang_tf_gating: ALL PASS\n");
        return 0;
    }
    fprintf(stderr, "test_lang_tf_gating: %d FAILURE(S)\n", g_failures);
    return 1;
}
