/**
 * @file test_wave4_e2e.c
 * @brief End-to-end tests for Wave 4 — 4-cycle soak with PREDICTIVE_IMMUNE.
 *
 * WHAT: Runs PREDICTIVE_IMMUNE + EPIGENETICS + NEUROVASCULAR + NEUROGENESIS
 *       as four concurrent driven cycles on a shared coordinator for 2s,
 *       verifies every cycle ticked, then unregisters and destroys cleanly.
 *
 * WHY:  This is the highest-value cross-cycle guard — it catches thread
 *       leaks, stuck drivers, and enum collisions that only appear under
 *       sustained multi-cycle load.
 *
 * HOW:  Real pthreads, real time. Total runtime budget ~2.2s. Per-cycle
 *       tolerances use lower bounds only — jitter on a loaded CI box is
 *       real.
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

static void test_wave4_four_cycle_soak_2s(void) {
    TEST("E2E: 4 driven cycles (PI+EPI+NVC+NEURO) over 2s, all tick, clean shutdown");
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");

    tick_counter_t c_pi = {0}, c_ep = {0}, c_nvc = {0}, c_neu = {0};

    /* Use the same 100ms cadence the live wiring uses for PI/EP/NVC,
     * NEUROGENESIS is intrinsically slower — use 200ms here to keep the
     * test bounded while still exercising a slower driver. */
    int r1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE, 100000,
        tick_counter_fn, &c_pi, NULL);
    int r2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_EPIGENETICS, 100000,
        tick_counter_fn, &c_ep, NULL);
    int r3 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_NEUROVASCULAR, 100000,
        tick_counter_fn, &c_nvc, NULL);
    int r4 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_NEUROGENESIS, 200000,
        tick_counter_fn, &c_neu, NULL);

    ASSERT_EQ(r1, 0, "register PREDICTIVE_IMMUNE failed");
    ASSERT_EQ(r2, 0, "register EPIGENETICS failed");
    ASSERT_EQ(r3, 0, "register NEUROVASCULAR failed");
    ASSERT_EQ(r4, 0, "register NEUROGENESIS failed");

    /* Soak for 2 seconds. Expected:
     *   PI/EP/NVC at 100ms -> ~20 ticks
     *   NEUROGENESIS at 200ms -> ~10 ticks
     * Use generous lower bounds — a loaded CI box jitters. */
    sleep_ms(2000);

    int v_pi  = atomic_load(&c_pi.count);
    int v_ep  = atomic_load(&c_ep.count);
    int v_nvc = atomic_load(&c_nvc.count);
    int v_neu = atomic_load(&c_neu.count);

    /* Every cycle MUST have ticked at least a few times. */
    ASSERT_TRUE(v_pi  > 0, "PREDICTIVE_IMMUNE never ticked");
    ASSERT_TRUE(v_ep  > 0, "EPIGENETICS never ticked");
    ASSERT_TRUE(v_nvc > 0, "NEUROVASCULAR never ticked");
    ASSERT_TRUE(v_neu > 0, "NEUROGENESIS never ticked");

    /* Substantive lower bounds (30% of expected to absorb jitter). */
    ASSERT_TRUE(v_pi  >= 6,  "PREDICTIVE_IMMUNE tick count unreasonably low");
    ASSERT_TRUE(v_ep  >= 6,  "EPIGENETICS tick count unreasonably low");
    ASSERT_TRUE(v_nvc >= 6,  "NEUROVASCULAR tick count unreasonably low");
    ASSERT_TRUE(v_neu >= 3,  "NEUROGENESIS tick count unreasonably low");

    /* Unregister all four. Each call joins its driver thread. */
    int u1 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE);
    int u2 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_EPIGENETICS);
    int u3 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_NEUROVASCULAR);
    int u4 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_NEUROGENESIS);

    ASSERT_EQ(u1, 0, "unregister PREDICTIVE_IMMUNE failed");
    ASSERT_EQ(u2, 0, "unregister EPIGENETICS failed");
    ASSERT_EQ(u3, 0, "unregister NEUROVASCULAR failed");
    ASSERT_EQ(u4, 0, "unregister NEUROGENESIS failed");

    /* After all unregisters, tick counts must halt. */
    int after_pi  = atomic_load(&c_pi.count);
    int after_ep  = atomic_load(&c_ep.count);
    int after_nvc = atomic_load(&c_nvc.count);
    int after_neu = atomic_load(&c_neu.count);
    sleep_ms(150);
    int later_pi  = atomic_load(&c_pi.count);
    int later_ep  = atomic_load(&c_ep.count);
    int later_nvc = atomic_load(&c_nvc.count);
    int later_neu = atomic_load(&c_neu.count);

    /* destroy must complete promptly even with no drivers — sanity bound. */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    brain_cycle_coordinator_destroy(coord);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000 +
                      (t1.tv_nsec - t0.tv_nsec) / 1000000L;

    ASSERT_EQ(later_pi,  after_pi,  "PI ticked after unregister (thread leak)");
    ASSERT_EQ(later_ep,  after_ep,  "EP ticked after unregister (thread leak)");
    ASSERT_EQ(later_nvc, after_nvc, "NVC ticked after unregister (thread leak)");
    ASSERT_EQ(later_neu, after_neu, "NEURO ticked after unregister (thread leak)");
    ASSERT_TRUE(elapsed_ms < 500, "destroy took too long (possible hang)");
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== E2E Tests: Wave 4 multi-cycle soak ===\n\n");

    test_wave4_four_cycle_soak_2s();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
