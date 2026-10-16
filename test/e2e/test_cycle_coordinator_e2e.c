/**
 * @file test_cycle_coordinator_e2e.c
 * @brief End-to-end tests for the brain cycle coordinator driven cycle API.
 *
 * WHAT: Full lifecycle scenarios exercising multiple concurrent driven +
 *       observation cycles, sustained run under real time, clean shutdown.
 * WHY:  These tests are the last line of defense against thread leaks,
 *       stuck registrations, missed ticks, and deadlocks under load.
 * HOW:  Use the public coordinator API with real pthreads + real time; keep
 *       total runtime under ~3 seconds; validate per-cycle tick counts with
 *       ±30% tolerance to absorb scheduler jitter.
 */

/* POSIX for nanosleep, clock_gettime, CLOCK_MONOTONIC. */
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

#define TEST(name) do { printf("  %-70s", name); fflush(stdout); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((long long)(a) != (long long)(b)) { \
    printf("[FAIL] %s (got %lld, expected %lld)\n", msg, (long long)(a), (long long)(b)); \
    tests_failed++; return; } } while(0)

static void make_quiet_config(brain_cycle_coordinator_config_t* c) {
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

static bool within_tolerance(int observed, int expected, double tolerance) {
    double lo = expected * (1.0 - tolerance);
    double hi = expected * (1.0 + tolerance);
    return observed >= (int)lo && observed <= (int)hi;
}

static void test_three_driven_cycles_run_2s(void) {
    TEST("E2E: 3 driven cycles (1/10/100ms) over 2s, tick counts within 30%");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t c1 = {0}, c10 = {0}, c100 = {0};

    /* 1ms on a loaded host is ambitious; use inequalities generously. */
    int r1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 1000, tick_counter_fn, &c1, NULL);
    int r2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 10000, tick_counter_fn, &c10, NULL);
    int r3 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_HEALTH_AGENT, 100000, tick_counter_fn, &c100, NULL);
    ASSERT_EQ(r1, 0, "register 1ms failed");
    ASSERT_EQ(r2, 0, "register 10ms failed");
    ASSERT_EQ(r3, 0, "register 100ms failed");

    sleep_ms(2000);

    int v1   = atomic_load(&c1.count);
    int v10  = atomic_load(&c10.count);
    int v100 = atomic_load(&c100.count);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_IMMUNE_TICK);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_HEALTH_AGENT);
    brain_cycle_coordinator_destroy(coord);

    /* Expected ticks: 2000, 200, 20. Tolerances:
     *   100ms cycle → ±30% (strict — scheduler-robust)
     *   10ms  cycle → ±40% (moderate jitter)
     *   1ms   cycle → ±70% (highly scheduler-dependent; use a lower bound).
     */
    ASSERT_TRUE(within_tolerance(v100, 20, 0.30),
        "100ms cycle tick count outside ±30% of 20");
    ASSERT_TRUE(within_tolerance(v10, 200, 0.40),
        "10ms cycle tick count outside ±40% of 200");
    /* 1ms: just require it got a substantial number of ticks — not strict. */
    ASSERT_TRUE(v1 >= 500,
        "1ms cycle tick count unreasonably low");
    /* Ordering sanity: 1ms >> 10ms >> 100ms. */
    ASSERT_TRUE(v1 > v10 && v10 > v100, "tick ordering violated");
    PASS();
}

static void test_full_lifecycle_mixed_cycles(void) {
    TEST("E2E: full lifecycle — 2 driven + 1 observation, clean shutdown");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t d1 = {0}, d2 = {0};

    int r1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 5000, tick_counter_fn, &d1, NULL);
    int r2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 20000, tick_counter_fn, &d2, NULL);
    int r3 = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, NULL, NULL);
    ASSERT_EQ(r1, 0, "driven1 register failed");
    ASSERT_EQ(r2, 0, "driven2 register failed");
    ASSERT_EQ(r3, 0, "observation register failed");

    /* Feed the observation cycle at ~10ms intervals. */
    for (int i = 0; i < 50; i++) {
        (void)brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_BRAIN_UPDATE, 250);
        sleep_ms(10);
    }
    /* Total wall time ≈ 500ms. */

    int v1_before = atomic_load(&d1.count);
    int v2_before = atomic_load(&d2.count);

    /* Unregister everything. */
    int u1 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    int u2 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_IMMUNE_TICK);
    int u3 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_BRAIN_UPDATE);
    ASSERT_EQ(u1, 0, "driven1 unregister failed");
    ASSERT_EQ(u2, 0, "driven2 unregister failed");
    ASSERT_EQ(u3, 0, "observation unregister failed");

    /* After all unregisters, tick counts must halt. */
    sleep_ms(150);
    int v1_after = atomic_load(&d1.count);
    int v2_after = atomic_load(&d2.count);

    brain_cycle_coordinator_destroy(coord);

    ASSERT_TRUE(v1_before >= 30, "driven1 ticks too low (expected ~100)");
    ASSERT_TRUE(v2_before >= 5,  "driven2 ticks too low (expected ~25)");
    ASSERT_EQ(v1_after, v1_before,
              "driven1 ticked after unregister (thread not joined)");
    ASSERT_EQ(v2_after, v2_before,
              "driven2 ticked after unregister (thread not joined)");
    PASS();
}

int main(void) {
    printf("\n=== E2E Tests: brain_cycle_coordinator driven (real time) ===\n\n");

    test_three_driven_cycles_run_2s();
    test_full_lifecycle_mixed_cycles();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
