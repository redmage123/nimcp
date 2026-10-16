/**
 * @file test_wave6_e2e.c
 * @brief End-to-end tests for Wave 6 — 5-cycle soak including CHEMISTRY.
 *
 * WHAT: Runs CHEMISTRY (10ms) + PREDICTIVE_IMMUNE (100ms) + EPIGENETICS
 *       (100ms) + NEUROVASCULAR (100ms) + NEUROGENESIS (1s) as five
 *       concurrent driven cycles on one coordinator for 2 seconds.
 *       Verifies every cycle ticked and CHEMISTRY, the new fast cycle,
 *       outpaces NEUROGENESIS (the slowest) by >= 40x.
 *
 * WHY:  This is the highest-value cross-cycle guard at the Wave 6 boundary.
 *       It catches thread leaks, stuck drivers, enum collisions, and
 *       timing interference that only appear under sustained multi-cycle
 *       load. The 40x ratio specifically proves the 10ms cadence holds
 *       under contention with 4 slower neighbors.
 *
 * HOW:  Real pthreads, real time. Plain tick-counter callbacks per cycle
 *       (no chemistry module stack — that's covered by unit + integration).
 *       Total runtime budget ~2.2s.
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

static void test_wave6_five_cycle_soak_2s(void) {
    TEST("E2E: CHEMISTRY+PI+EPI+NVC+NEURO over 2s, CHEMISTRY >= 40x NEURO");
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");

    tick_counter_t c_chem = {0}, c_pi = {0}, c_ep = {0};
    tick_counter_t c_nvc  = {0}, c_neu = {0};

    /* Five driven cycles. Chemistry at 10ms is the new fast cadence.
     * The other four run at their live-wiring cadences (100ms, 100ms,
     * 100ms, 1s). */
    int r1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_CHEMISTRY, 10000,
        tick_counter_fn, &c_chem, NULL);
    int r2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE, 100000,
        tick_counter_fn, &c_pi, NULL);
    int r3 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_EPIGENETICS, 100000,
        tick_counter_fn, &c_ep, NULL);
    int r4 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_NEUROVASCULAR, 100000,
        tick_counter_fn, &c_nvc, NULL);
    int r5 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_NEUROGENESIS, 1000000,
        tick_counter_fn, &c_neu, NULL);

    ASSERT_EQ(r1, 0, "register CHEMISTRY failed");
    ASSERT_EQ(r2, 0, "register PREDICTIVE_IMMUNE failed");
    ASSERT_EQ(r3, 0, "register EPIGENETICS failed");
    ASSERT_EQ(r4, 0, "register NEUROVASCULAR failed");
    ASSERT_EQ(r5, 0, "register NEUROGENESIS failed");

    /* Soak for 2 seconds. Expected:
     *   CHEMISTRY at 10ms    -> ~200 ticks
     *   PI/EP/NVC at 100ms   -> ~20 ticks each
     *   NEUROGENESIS at 1s   -> ~2 ticks
     * Generous lower bounds to absorb jitter on a loaded CI host. */
    sleep_ms(2000);

    int v_chem = atomic_load(&c_chem.count);
    int v_pi   = atomic_load(&c_pi.count);
    int v_ep   = atomic_load(&c_ep.count);
    int v_nvc  = atomic_load(&c_nvc.count);
    int v_neu  = atomic_load(&c_neu.count);

    /* Every cycle MUST have ticked at least once. */
    ASSERT_TRUE(v_chem > 0, "CHEMISTRY never ticked");
    ASSERT_TRUE(v_pi   > 0, "PREDICTIVE_IMMUNE never ticked");
    ASSERT_TRUE(v_ep   > 0, "EPIGENETICS never ticked");
    ASSERT_TRUE(v_nvc  > 0, "NEUROVASCULAR never ticked");
    ASSERT_TRUE(v_neu  > 0, "NEUROGENESIS never ticked");

    /* Substantive lower bounds (~50% of expected to absorb jitter). */
    ASSERT_TRUE(v_chem >= 80, "CHEMISTRY tick count unreasonably low");
    ASSERT_TRUE(v_pi   >= 6,  "PREDICTIVE_IMMUNE tick count unreasonably low");
    ASSERT_TRUE(v_ep   >= 6,  "EPIGENETICS tick count unreasonably low");
    ASSERT_TRUE(v_nvc  >= 6,  "NEUROVASCULAR tick count unreasonably low");
    ASSERT_TRUE(v_neu  >= 1,  "NEUROGENESIS tick count unreasonably low");

    /* The defining Wave 6 guarantee: CHEMISTRY outpaces the 1s cycle by
     * the expected 100x ratio (100x ideal, 40x is our slack floor). */
    ASSERT_TRUE(v_chem >= v_neu * 40,
        "CHEMISTRY/NEUROGENESIS ratio < 40x — fast cadence missed under load");

    /* Unregister all five. Each call joins its driver thread. */
    int u1 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_CHEMISTRY);
    int u2 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE);
    int u3 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_EPIGENETICS);
    int u4 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_NEUROVASCULAR);
    int u5 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_NEUROGENESIS);

    ASSERT_EQ(u1, 0, "unregister CHEMISTRY failed");
    ASSERT_EQ(u2, 0, "unregister PREDICTIVE_IMMUNE failed");
    ASSERT_EQ(u3, 0, "unregister EPIGENETICS failed");
    ASSERT_EQ(u4, 0, "unregister NEUROVASCULAR failed");
    ASSERT_EQ(u5, 0, "unregister NEUROGENESIS failed");

    /* After unregister, tick counts must halt. */
    int after_chem = atomic_load(&c_chem.count);
    int after_pi   = atomic_load(&c_pi.count);
    int after_ep   = atomic_load(&c_ep.count);
    int after_nvc  = atomic_load(&c_nvc.count);
    int after_neu  = atomic_load(&c_neu.count);
    sleep_ms(150);
    int later_chem = atomic_load(&c_chem.count);
    int later_pi   = atomic_load(&c_pi.count);
    int later_ep   = atomic_load(&c_ep.count);
    int later_nvc  = atomic_load(&c_nvc.count);
    int later_neu  = atomic_load(&c_neu.count);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    brain_cycle_coordinator_destroy(coord);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000 +
                      (t1.tv_nsec - t0.tv_nsec) / 1000000L;

    ASSERT_EQ(later_chem, after_chem, "CHEMISTRY ticked after unregister (thread leak)");
    ASSERT_EQ(later_pi,   after_pi,   "PI ticked after unregister (thread leak)");
    ASSERT_EQ(later_ep,   after_ep,   "EPI ticked after unregister (thread leak)");
    ASSERT_EQ(later_nvc,  after_nvc,  "NVC ticked after unregister (thread leak)");
    ASSERT_EQ(later_neu,  after_neu,  "NEURO ticked after unregister (thread leak)");
    ASSERT_TRUE(elapsed_ms < 500, "destroy took too long (possible hang)");
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== E2E Tests: Wave 6 5-cycle soak with CHEMISTRY ===\n\n");

    test_wave6_five_cycle_soak_2s();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
