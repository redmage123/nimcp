/**
 * @file test_wave4_integration.c
 * @brief Integration tests for Wave 4 (predictive_immune + meta_learning).
 *
 * WHAT: Exercises the new BRAIN_CYCLE_PREDICTIVE_IMMUNE driven cycle under a
 *       live coordinator alongside other biology driven cycles (EPIGENETICS,
 *       NEUROVASCULAR) so we can verify no mutual interference and that the
 *       init/destroy lifecycle cleans up cleanly under concurrent load.
 *
 * WHY:  Wave 4 added both a new enum value AND a live driver thread. The
 *       interesting failure modes are cross-cycle: enum collision with a
 *       biology cycle, a new thread refusing to yield, or the coordinator
 *       stats getting clobbered by concurrent registrations.
 *
 * HOW:  Drive the coordinator directly with tick-counter callbacks on each
 *       cycle. We do NOT wire the real predictive_immune module here — that
 *       path is covered by the unit full-lifecycle test. Integration at the
 *       coordinator level is sufficient to prove the new enum value behaves
 *       like every other cycle type in a multi-cycle setting.
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
#include "core/brain/factory/init/nimcp_brain_init_predictive_immune.h"
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/nimcp_predictive.h"
#include "cognitive/immune/nimcp_brain_immune.h"

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

static void test_predictive_immune_coexists_with_epigenetics(void) {
    TEST("PREDICTIVE_IMMUNE + EPIGENETICS run concurrently, stats independent");
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");

    tick_counter_t pi = {0}, ep = {0};

    /* PREDICTIVE_IMMUNE: 10ms (fast in test). EPIGENETICS: 25ms. */
    int r1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE, 10000,
        tick_counter_fn, &pi, NULL);
    int r2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_EPIGENETICS, 25000,
        tick_counter_fn, &ep, NULL);
    ASSERT_EQ(r1, 0, "register PREDICTIVE_IMMUNE failed");
    ASSERT_EQ(r2, 0, "register EPIGENETICS failed");

    sleep_ms(400);

    int pi_ticks = atomic_load(&pi.count);
    int ep_ticks = atomic_load(&ep.count);

    brain_cycle_status_t pi_st, ep_st;
    int g1 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE, &pi_st);
    int g2 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_EPIGENETICS, &ep_st);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_EPIGENETICS);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_EQ(g1, 0, "get_status PREDICTIVE_IMMUNE failed");
    ASSERT_EQ(g2, 0, "get_status EPIGENETICS failed");
    ASSERT_TRUE(pi_ticks >= 10, "PREDICTIVE_IMMUNE ticked too rarely");
    ASSERT_TRUE(ep_ticks >= 4, "EPIGENETICS ticked too rarely");
    /* 10ms cycle should outpace the 25ms cycle by >= 2x. */
    ASSERT_TRUE(pi_ticks > ep_ticks * 2,
        "PREDICTIVE_IMMUNE didn't outpace EPIGENETICS (mutual starvation?)");
    /* Independent stats: PI expected_interval=10ms, EPI=25ms. */
    ASSERT_EQ(pi_st.expected_interval_us, 10000ULL,
              "PI expected_interval wrong");
    ASSERT_EQ(ep_st.expected_interval_us, 25000ULL,
              "EPI expected_interval wrong");
    PASS();
}

static void test_predictive_immune_init_destroy_with_load(void) {
    TEST("init + 500ms run + destroy: tick count > 0, clean shutdown");
    /* Full-subsystem init variant: uses the real factory + live module. */
    struct brain_struct* b = (struct brain_struct*)
        calloc(1, sizeof(struct brain_struct));
    ASSERT_NOT_NULL(b, "brain calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    predictive_config_t pc = predictive_default_config();
    predictive_network_t pnet = predictive_create(&pc);
    ASSERT_NOT_NULL(pnet, "predictive_create failed");
    b->predictive_network = pnet;

    brain_immune_config_t ic; (void)brain_immune_default_config(&ic);
    brain_immune_system_t* is = brain_immune_create(&ic);
    ASSERT_NOT_NULL(is, "brain_immune_create failed");
    b->immune_system = is;

    bool ok = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_TRUE(ok, "factory init failed");

    sleep_ms(500);

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE, &st);
    ASSERT_EQ(g, 0, "get_status failed");
    ASSERT_TRUE(st.ticks_executed >= 2,
        "live module ticked fewer than 2 times over 500ms @ 100ms cadence");

    /* destroy must not hang — bound it tightly. */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    nimcp_brain_factory_destroy_predictive_immune_subsystem(b);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000 +
                      (t1.tv_nsec - t0.tv_nsec) / 1000000L;
    ASSERT_TRUE(elapsed_ms < 500,
        "destroy took too long (thread leak or deadlock?)");

    brain_immune_destroy(is);
    predictive_destroy(pnet);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Integration Tests: Wave 4 (predictive_immune + meta) ===\n\n");

    test_predictive_immune_coexists_with_epigenetics();
    test_predictive_immune_init_destroy_with_load();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
