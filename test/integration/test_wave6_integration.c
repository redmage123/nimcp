/**
 * @file test_wave6_integration.c
 * @brief Integration tests for Wave 6 (chemistry cluster driven cycle).
 *
 * WHAT: Exercises the new BRAIN_CYCLE_CHEMISTRY driven cycle alongside
 *       other cycles (EPIGENETICS, PREDICTIVE_IMMUNE) on the same
 *       coordinator, and validates that the 10ms cadence is actually
 *       achieved under load (the single tick wrapper fans out to 4
 *       module updates — all no-op on an empty config).
 *
 * WHY:  Wave 6 added a new enum value, a new driver thread, and a tick
 *       wrapper that calls 4 module updates in series per pass. Cross-
 *       cycle failure modes — enum collision, mutual starvation, cadence
 *       drift from fat tick wrappers — only surface under concurrent load.
 *
 * HOW:  Drive the coordinator with a mix of tick-counter callbacks (for
 *       non-chemistry cycles — they're enum-value exercise only) and the
 *       real factory init for chemistry (exercises the full module stack).
 *       Real pthreads, real time. Total runtime ~1.5s.
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "core/brain/factory/init/nimcp_brain_init_chemistry.h"
#include "core/brain/nimcp_brain_internal.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-72s", name); fflush(stdout); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((long long)(a) != (long long)(b)) { \
    printf("[FAIL] %s (got %lld, expected %lld)\n", msg, (long long)(a), (long long)(b)); \
    tests_failed++; return; } } while(0)
#define ASSERT_NOT_NULL(p, msg) do { if ((p) == NULL) { FAIL(msg); } } while(0)

/* ------------------------------------------------------------------------- */

static void make_quiet_cfg(brain_cycle_coordinator_config_t* c) {
    brain_cycle_coordinator_default_config(c);
    c->enable_logging = false;
    c->enable_debug_logging = false;
}

typedef struct { atomic_int count; } tick_counter_t;

static void tick_counter_fn(void* ctx) {
    atomic_fetch_add(&((tick_counter_t*)ctx)->count, 1);
}

static void sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------------- */

static void test_chemistry_coexists_with_epigenetics(void) {
    TEST("CHEMISTRY + EPIGENETICS run concurrently, both tick independently");
    struct brain_struct* b = (struct brain_struct*)
        calloc(1, sizeof(struct brain_struct));
    ASSERT_NOT_NULL(b, "brain calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    /* Register EPIGENETICS as a plain counter cycle (25ms). */
    tick_counter_t ep = {0};
    int r1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_EPIGENETICS, 25000,
        tick_counter_fn, &ep, NULL);
    ASSERT_EQ(r1, 0, "register EPIGENETICS failed");

    /* Register chemistry via the real factory (10ms). */
    bool ok = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(ok, "chemistry factory init failed");

    sleep_ms(400);

    brain_cycle_status_t ch_st;
    int g1 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_CHEMISTRY, &ch_st);
    int ep_ticks = atomic_load(&ep.count);

    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_EPIGENETICS);
    brain_cycle_coordinator_destroy(coord);
    free(b);

    ASSERT_EQ(g1, 0, "get_status CHEMISTRY failed");
    ASSERT_TRUE(ch_st.ticks_executed >= 10,
        "CHEMISTRY ticked too rarely (expected ~40 over 400ms @ 10ms)");
    ASSERT_TRUE(ep_ticks >= 4,
        "EPIGENETICS ticked too rarely (expected ~16 over 400ms @ 25ms)");
    /* Chemistry at 10ms should outpace EPI at 25ms by >= 2x. */
    ASSERT_TRUE((uint64_t)ch_st.ticks_executed > (uint64_t)ep_ticks * 2,
        "CHEMISTRY didn't outpace EPIGENETICS (mutual starvation?)");
    ASSERT_EQ(ch_st.expected_interval_us, 10000ULL,
        "CHEMISTRY expected_interval wrong");
    PASS();
}

static void test_chemistry_coexists_with_predictive_immune(void) {
    TEST("CHEMISTRY + PREDICTIVE_IMMUNE coexist, tick counts independent");
    struct brain_struct* b = (struct brain_struct*)
        calloc(1, sizeof(struct brain_struct));
    ASSERT_NOT_NULL(b, "brain calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    /* PREDICTIVE_IMMUNE as a pure counter at 50ms — we're only checking
     * that chemistry doesn't starve it, not exercising the real module. */
    tick_counter_t pi = {0};
    int r1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE, 50000,
        tick_counter_fn, &pi, NULL);
    ASSERT_EQ(r1, 0, "register PREDICTIVE_IMMUNE failed");

    bool ok = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(ok, "chemistry factory init failed");

    sleep_ms(400);

    brain_cycle_status_t ch_st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_CHEMISTRY, &ch_st);
    int pi_ticks = atomic_load(&pi.count);

    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE);
    brain_cycle_coordinator_destroy(coord);
    free(b);

    ASSERT_EQ(g, 0, "get_status CHEMISTRY failed");
    ASSERT_TRUE(ch_st.ticks_executed >= 10,
        "CHEMISTRY starved by PREDICTIVE_IMMUNE");
    ASSERT_TRUE(pi_ticks >= 2,
        "PREDICTIVE_IMMUNE starved by CHEMISTRY");
    PASS();
}

static void test_chemistry_achieves_10ms_cadence(void) {
    TEST("chemistry hits ~10ms cadence: ticks >= 40 over 500ms window");
    /* Purpose: the single chemistry tick wrapper calls 4 module updates
     * (pumps, buffers, pH, NO). With an empty config (no regions/sources
     * added) each is O(1), so the 10ms cadence should be achievable
     * even on a loaded CI host. We allow 40 ticks (vs ideal 50) as a
     * slack threshold for scheduler jitter. */
    struct brain_struct* b = (struct brain_struct*)
        calloc(1, sizeof(struct brain_struct));
    ASSERT_NOT_NULL(b, "brain calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool ok = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(ok, "chemistry factory init failed");

    /* Measure a tight 500ms window (skip first 50ms to let the driver
     * thread spin up and warm up any first-call initialization costs). */
    sleep_ms(50);
    brain_cycle_status_t st_before;
    int g1 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_CHEMISTRY, &st_before);
    ASSERT_EQ(g1, 0, "get_status failed (before)");

    sleep_ms(500);

    brain_cycle_status_t st_after;
    int g2 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_CHEMISTRY, &st_after);
    ASSERT_EQ(g2, 0, "get_status failed (after)");

    uint64_t delta_ticks =
        st_after.ticks_executed - st_before.ticks_executed;

    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);

    /* 500ms / 10ms = 50 ideal. 40 = 80% efficiency allows scheduler slop. */
    ASSERT_TRUE(delta_ticks >= 40,
        "cadence target missed — expected >= 40 ticks over 500ms");
    /* Ceiling sanity check: shouldn't be unbounded. */
    ASSERT_TRUE(delta_ticks <= 200,
        "cadence wildly over budget — clock or cadence bug?");
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Integration Tests: Wave 6 (chemistry cluster) ===\n\n");

    test_chemistry_coexists_with_epigenetics();
    test_chemistry_coexists_with_predictive_immune();
    test_chemistry_achieves_10ms_cadence();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
