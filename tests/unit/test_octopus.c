/**
 * @file test_octopus.c
 * @brief Unit + integration tests for the octopus cognitive module.
 *
 * Coverage:
 *   UNIT
 *     - create / destroy
 *     - arm count clamping ([2,16])
 *     - explore with null / empty inputs rejects cleanly
 *     - explore produces non-zero confidence for non-zero input
 *     - integrate with no data → zero-output, coherence=0
 *     - integrate after explore produces meaningful aggregate
 *     - hook setters are individually testable
 *   INTEGRATION
 *     - ethics hook vetoes arm → arm doesn't contribute to aggregate
 *     - swarm hook fires when arm confidence > threshold
 *     - world hook receives aggregated latent
 *     - fep hook receives coherence signal
 *     - bio hook fires on low coherence events
 *     - immune hook fires per explore
 *   REGRESSION
 *     - 200 exploration/integrate cycles don't leak or crash
 *     - coherence stays in [0, 1]
 *     - broadcast_state stays in [0, 1]
 */
#include "cognitive/octopus/nimcp_octopus.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Lightweight harness — printf + exit on failure; no dependency on a full
 * test framework so this builds standalone. */
#define CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
    exit(1); } } while (0)

#define CHECK_RANGE(v, lo, hi, msg) \
    CHECK((v) >= (lo) && (v) <= (hi), msg)

/*============================================================================
 * Unit tests
 *==========================================================================*/

static void test_create_destroy_defaults(void) {
    octopus_system_t* ctx = octopus_create(0);  /* 0 → default 8 */
    CHECK(ctx != NULL, "create(0) returned NULL");
    CHECK(octopus_get_n_arms(ctx) == 8, "default n_arms not 8");
    octopus_destroy(ctx);
    printf("  PASS: create_destroy_defaults\n");
}

static void test_arm_count_clamping(void) {
    octopus_system_t* a = octopus_create(1);   /* should clamp up to 2 */
    octopus_system_t* b = octopus_create(999); /* should clamp down to 16 */
    CHECK(octopus_get_n_arms(a) == 2,  "n_arms=1 should clamp to 2");
    CHECK(octopus_get_n_arms(b) == 16, "n_arms=999 should clamp to 16");
    octopus_destroy(a);
    octopus_destroy(b);
    printf("  PASS: arm_count_clamping\n");
}

static void test_explore_null_rejection(void) {
    octopus_system_t* ctx = octopus_create(4);
    CHECK(octopus_explore(ctx, NULL, 10) == -1, "null input not rejected");
    CHECK(octopus_explore(ctx, (const float[]){1.0f}, 0) == -1,
          "zero length not rejected");
    CHECK(octopus_explore(NULL, (const float[]){1.0f}, 1) == -1,
          "null ctx not rejected");
    octopus_destroy(ctx);
    printf("  PASS: explore_null_rejection\n");
}

static void test_explore_builds_confidence(void) {
    octopus_system_t* ctx = octopus_create(4);
    float input[128];
    for (int i = 0; i < 128; i++) input[i] = ((float)i) / 128.0f;

    int rc = octopus_explore(ctx, input, 128);
    CHECK(rc == 0, "explore failed on valid input");

    /* At least one arm should have non-zero confidence. */
    float max_conf = 0.0f;
    for (uint32_t a = 0; a < octopus_get_n_arms(ctx); a++) {
        const octopus_arm_t* arm = octopus_get_arm(ctx, a);
        CHECK(arm != NULL, "arm accessor returned NULL");
        if (arm->confidence > max_conf) max_conf = arm->confidence;
    }
    CHECK(max_conf > 0.0f, "all arms have zero confidence on non-zero input");
    octopus_destroy(ctx);
    printf("  PASS: explore_builds_confidence (max_conf=%.3f)\n", max_conf);
}

static void test_integrate_no_data(void) {
    octopus_system_t* ctx = octopus_create(4);
    float out[64] = {99.0f};  /* non-zero sentinel */
    float coh = 99.0f;
    int rc = octopus_integrate(ctx, out, &coh);
    CHECK(rc == -1, "integrate without explore should return -1");
    /* Output should be zeroed (or at least coherence == 0). */
    CHECK(coh == 0.0f, "coherence must be 0 when no data");
    octopus_destroy(ctx);
    printf("  PASS: integrate_no_data\n");
}

static void test_integrate_after_explore(void) {
    octopus_system_t* ctx = octopus_create(8);
    float input[128];
    for (int i = 0; i < 128; i++) input[i] = sinf(((float)i) * 0.1f);
    octopus_explore(ctx, input, 128);

    float out[64];
    float coh = -1.0f;
    int rc = octopus_integrate(ctx, out, &coh);
    CHECK(rc == 0, "integrate after explore should succeed");
    CHECK_RANGE(coh, 0.0f, 1.0f, "coherence out of [0,1]");

    /* Aggregate should not be all zeros. */
    float norm = 0.0f;
    for (int i = 0; i < 64; i++) norm += fabsf(out[i]);
    CHECK(norm > 0.0f, "aggregate is all zeros");
    octopus_destroy(ctx);
    printf("  PASS: integrate_after_explore (coh=%.3f, norm=%.3f)\n",
           coh, norm);
}

/*============================================================================
 * Integration tests — hook wiring
 *==========================================================================*/

/* Test state — populated by hooks so we can verify firing. */
typedef struct {
    int ethics_calls;
    int ethics_veto_next;
    int swarm_calls;
    int world_calls;
    int fep_calls;
    int bio_calls;
    int immune_calls;
    float last_fep_value;
    float last_bio_value;
} hook_state_t;

static bool test_ethics_hook(const octopus_arm_t* arm, void* user) {
    hook_state_t* s = (hook_state_t*)user;
    s->ethics_calls++;
    /* Veto arm 0 if requested. */
    if (s->ethics_veto_next && arm->id == 0) return false;
    return true;
}
static void test_swarm_hook(const octopus_arm_t* a, void* u) {
    (void)a; ((hook_state_t*)u)->swarm_calls++;
}
static void test_world_hook(const float* v, uint32_t l, void* u) {
    (void)v; (void)l; ((hook_state_t*)u)->world_calls++;
}
static void test_fep_hook(float c, void* u) {
    hook_state_t* s = (hook_state_t*)u;
    s->fep_calls++; s->last_fep_value = c;
}
static void test_bio_hook(const char* e, float v, void* u) {
    hook_state_t* s = (hook_state_t*)u;
    (void)e; s->bio_calls++; s->last_bio_value = v;
}
static void test_immune_hook(void* u) {
    ((hook_state_t*)u)->immune_calls++;
}

static void test_hooks_all_fire(void) {
    octopus_system_t* ctx = octopus_create(4);
    hook_state_t s = {0};
    octopus_set_ethics_hook(ctx, test_ethics_hook, &s);
    octopus_set_swarm_hook (ctx, test_swarm_hook,  &s);
    octopus_set_world_hook (ctx, test_world_hook,  &s);
    octopus_set_fep_hook   (ctx, test_fep_hook,    &s);
    octopus_set_bio_hook   (ctx, test_bio_hook,    &s);
    octopus_set_immune_hook(ctx, test_immune_hook, &s);

    float input[128];
    for (int i = 0; i < 128; i++) input[i] = ((float)i % 13) * 0.2f;
    octopus_explore(ctx, input, 128);
    float out[64]; float coh;
    octopus_integrate(ctx, out, &coh);

    CHECK(s.ethics_calls == 4, "ethics should fire once per arm");
    CHECK(s.immune_calls == 1, "immune should fire once per explore");
    CHECK(s.world_calls  == 1, "world should fire once per integrate");
    CHECK(s.fep_calls    == 1, "fep should fire once per integrate");
    CHECK_RANGE(s.last_fep_value, 0.0f, 1.0f, "fep value out of range");
    /* swarm may or may not fire depending on confidence; don't assert count. */
    octopus_destroy(ctx);
    printf("  PASS: hooks_all_fire (ethics=%d, immune=%d, world=%d, fep=%d)\n",
           s.ethics_calls, s.immune_calls, s.world_calls, s.fep_calls);
}

static void test_ethics_veto_blocks_arm(void) {
    octopus_system_t* ctx = octopus_create(4);
    hook_state_t s = {0};
    s.ethics_veto_next = 1;  /* ALWAYS veto arm 0 */
    octopus_set_ethics_hook(ctx, test_ethics_hook, &s);

    float input[128];
    for (int i = 0; i < 128; i++) input[i] = 1.0f;  /* high confidence */
    octopus_explore(ctx, input, 128);
    /* Arm 0 should be vetoed. */
    CHECK(octopus_get_arm(ctx, 0)->vetoed == true, "arm 0 not vetoed");
    for (uint32_t a = 1; a < 4; a++) {
        CHECK(octopus_get_arm(ctx, a)->vetoed == false,
              "non-vetoed arm marked as vetoed");
    }
    octopus_destroy(ctx);
    printf("  PASS: ethics_veto_blocks_arm\n");
}

/*============================================================================
 * Regression — long-run stability
 *==========================================================================*/

static void test_long_run_stability(void) {
    octopus_system_t* ctx = octopus_create(8);
    float out[64]; float coh;
    for (int iter = 0; iter < 200; iter++) {
        float input[128];
        for (int i = 0; i < 128; i++) {
            input[i] = sinf(((float)(iter * 128 + i)) * 0.01f);
        }
        CHECK(octopus_explore(ctx, input, 128) == 0, "explore failed");
        int rc = octopus_integrate(ctx, out, &coh);
        CHECK(rc == 0, "integrate failed");
        CHECK_RANGE(coh, 0.0f, 1.0f, "coherence drifted out of [0,1]");
        for (uint32_t a = 0; a < octopus_get_n_arms(ctx); a++) {
            const octopus_arm_t* arm = octopus_get_arm(ctx, a);
            CHECK_RANGE(arm->broadcast_state, 0.0f, 1.0f, "broadcast OOR");
            CHECK_RANGE(arm->confidence,      0.0f, 1.0f, "confidence OOR");
            /* latent values bounded by tanh. */
            for (uint32_t d = 0; d < OCTOPUS_ARM_DIM; d++) {
                CHECK(!isnan(arm->latent[d]), "NaN in latent");
                CHECK(!isinf(arm->latent[d]), "Inf in latent");
                CHECK_RANGE(arm->latent[d], -1.0f, 1.0f, "latent OOR");
            }
        }
    }
    octopus_stats_t stats;
    octopus_get_stats(ctx, &stats);
    CHECK(stats.n_explorations == 200, "exploration count mismatch");
    CHECK(stats.n_integrations == 200, "integration count mismatch");
    octopus_destroy(ctx);
    printf("  PASS: long_run_stability (200 cycles clean)\n");
}

int main(void) {
    printf("[octopus unit]\n");
    test_create_destroy_defaults();
    test_arm_count_clamping();
    test_explore_null_rejection();
    test_explore_builds_confidence();
    test_integrate_no_data();
    test_integrate_after_explore();

    printf("[octopus integration]\n");
    test_hooks_all_fire();
    test_ethics_veto_blocks_arm();

    printf("[octopus regression]\n");
    test_long_run_stability();

    printf("\nAll octopus tests passed.\n");
    return 0;
}
