/**
 * @file test_cycle_coordinator_driven_integration.c
 * @brief Integration tests for driven brain cycles.
 *
 * WHAT: Exercises multi-cycle concurrency and mixed driven/observation-only
 *       registrations under the same coordinator.
 * WHY:  Per-cycle driver threads must run without mutual interference, and
 *       manual notify_tick on an observation cycle must remain independent of
 *       any concurrent driven cycle.
 * HOW:  Real pthreads + real time; tolerances are inequalities so scheduler
 *       jitter on a loaded host doesn't cause flakes.
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
#include <unistd.h>

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

typedef struct {
    atomic_int count;
    int sleep_ms;
} slow_tick_ctx_t;

static void slow_tick_fn(void* ctx) {
    slow_tick_ctx_t* s = (slow_tick_ctx_t*)ctx;
    atomic_fetch_add(&s->count, 1);
    struct timespec ts = {0, (long)s->sleep_ms * 1000000L};
    nanosleep(&ts, NULL);
}

typedef struct {
    atomic_int invocations;
    brain_cycle_health_t verdict;
} health_probe_t;

static brain_cycle_health_t probe_health_fn(void* handle) {
    health_probe_t* p = (health_probe_t*)handle;
    atomic_fetch_add(&p->invocations, 1);
    return p->verdict;
}

static void probe_tick_fn(void* ctx) { (void)ctx; }

static void sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------------- */

static void test_two_driven_different_intervals(void) {
    TEST("two driven cycles at different intervals run concurrently");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t fast = {0}, slow = {0};
    /* fast: 5ms, slow: 25ms */
    int rc1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 5000, tick_counter_fn, &fast, NULL);
    int rc2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_HEALTH_AGENT, 25000, tick_counter_fn, &slow, NULL);
    ASSERT_EQ(rc1, 0, "fast register failed");
    ASSERT_EQ(rc2, 0, "slow register failed");

    sleep_ms(500);

    int f = atomic_load(&fast.count);
    int s = atomic_load(&slow.count);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_HEALTH_AGENT);
    brain_cycle_coordinator_destroy(coord);

    /* Fast (5ms) should fire >> slow (25ms) — expect ~100 vs ~20, accept >= 2x. */
    ASSERT_TRUE(f > s * 2,
        "fast cycle didn't outpace slow cycle (possible mutual starvation)");
    ASSERT_TRUE(s >= 5, "slow cycle ticked too few times");
    PASS();
}

static void test_driven_plus_manual_notify_coexist(void) {
    TEST("driven cycle + manual notify_tick on different cycle coexist");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t driven_cnt = {0};
    int rc1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 5000,
        tick_counter_fn, &driven_cnt, NULL);
    ASSERT_EQ(rc1, 0, "driven register failed");

    /* Observation-only */
    int rc2 = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, NULL, NULL);
    ASSERT_EQ(rc2, 0, "observation register failed");

    /* Hammer observation cycle with manual notifies while driver runs. */
    for (int i = 0; i < 50; i++) {
        (void)brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_BRAIN_UPDATE, 100);
        sleep_ms(2);
    }

    brain_cycle_status_t drv_s, obs_s;
    int g1 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &drv_s);
    int g2 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &obs_s);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_BRAIN_UPDATE);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_EQ(g1, 0, "driven get_status failed");
    ASSERT_EQ(g2, 0, "observation get_status failed");
    ASSERT_TRUE(drv_s.ticks_executed >= 5, "driven ticks too low");
    ASSERT_EQ(obs_s.ticks_executed, 50, "observation ticks wrong");
    PASS();
}

static void test_driven_stats_populate(void) {
    TEST("driven stats (ticks_executed, avg_duration) populate over 500ms");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    slow_tick_ctx_t ctx = { .count = 0, .sleep_ms = 3 };
    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 10000, slow_tick_fn, &ctx, NULL);
    ASSERT_EQ(rc, 0, "register failed");

    sleep_ms(500);

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &st);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_EQ(g, 0, "get_status failed");
    ASSERT_TRUE(st.ticks_executed >= 20, "ticks_executed too low");
    /* Our tick_fn sleeps ~3ms → avg_duration_us > 1000us. */
    ASSERT_TRUE(st.avg_duration_us > 1000.0,
        "avg_duration_us did not reflect 3ms tick work");
    PASS();
}

static void test_health_fn_verdict_propagates(void) {
    TEST("health_fn verdict propagates via check_health + get_status");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    health_probe_t probe = { .invocations = 0,
                             .verdict = BRAIN_CYCLE_HEALTH_DEGRADED };

    /* Per register_driven contract, tick_ctx is also passed as cycle_handle
     * to health_fn. So one pointer both drives the tick and feeds the health. */
    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 5000,
        probe_tick_fn, &probe, probe_health_fn);
    ASSERT_EQ(rc, 0, "register_driven failed");

    /* Let at least one tick happen and run a health check. */
    sleep_ms(50);
    int issues = brain_cycle_coordinator_check_health(coord);
    (void)issues;

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &st);

    int inv = atomic_load(&probe.invocations);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_EQ(g, 0, "get_status failed");
    ASSERT_TRUE(inv >= 1, "health_fn was never invoked");
    ASSERT_EQ(st.health, BRAIN_CYCLE_HEALTH_DEGRADED,
              "verdict did not propagate to status");
    PASS();
}

static void test_shutdown_waits_for_inflight_tick(void) {
    TEST("unregister waits for in-flight 100ms tick_fn to finish");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    slow_tick_ctx_t ctx = { .count = 0, .sleep_ms = 100 };
    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 10000, slow_tick_fn, &ctx, NULL);
    ASSERT_EQ(rc, 0, "register failed");

    /* Wait for first tick to start. */
    sleep_ms(30);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int ur = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ASSERT_EQ(ur, 0, "unregister failed");

    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000 +
                      (t1.tv_nsec - t0.tv_nsec) / 1000000L;

    brain_cycle_coordinator_destroy(coord);

    /* Upper bound: driver slice(100ms) + tick(100ms) + slack. Must be
     * bounded, not hung. */
    ASSERT_TRUE(elapsed_ms < 500,
        "unregister took too long (possible deadlock)");
    PASS();
}

int main(void) {
    printf("\n=== Integration Tests: brain_cycle_coordinator driven ===\n\n");

    test_two_driven_different_intervals();
    test_driven_plus_manual_notify_coexist();
    test_driven_stats_populate();
    test_health_fn_verdict_propagates();
    test_shutdown_waits_for_inflight_tick();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
