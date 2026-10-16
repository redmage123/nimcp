/**
 * @file test_cycle_coordinator_driven.c
 * @brief Unit tests for brain_cycle_coordinator_register_driven() API.
 *
 * WHAT: Validates argument checking, happy-path ticking, conflict detection,
 *       unregister-join semantics, and destroy-with-active-driver cleanup.
 * WHY:  register_driven() spawns a per-cycle pthread; any leaked thread,
 *       stuck registration, or double-registration is a concurrency bug.
 * HOW:  Drives the API directly with real pthreads + real mutexes + real
 *       monotonic time. No mocks — concurrency bugs only show up for real.
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

/* ------------------------------------------------------------------------- */
/* Shared test fixtures                                                      */
/* ------------------------------------------------------------------------- */

/** Quiet config — turns off logging so test output stays readable. */
static void make_quiet_config(brain_cycle_coordinator_config_t* c) {
    brain_cycle_coordinator_default_config(c);
    c->enable_logging = false;
    c->enable_debug_logging = false;
}

/** Thread-safe tick counter. */
typedef struct {
    atomic_int count;
} tick_counter_t;

static void tick_counter_fn(void* ctx) {
    tick_counter_t* tc = (tick_counter_t*)ctx;
    atomic_fetch_add(&tc->count, 1);
}

static void nosleep_tick_fn(void* ctx) { (void)ctx; }

static brain_cycle_health_t always_healthy_fn(void* handle) {
    (void)handle;
    return BRAIN_CYCLE_HEALTH_HEALTHY;
}

static void sleep_ms(int ms) {
    struct timespec ts = { .tv_sec = ms / 1000,
                           .tv_nsec = (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

static void test_register_driven_null_coord(void) {
    TEST("register_driven NULL coord -> -1");
    int rc = brain_cycle_coordinator_register_driven(
        NULL, BRAIN_CYCLE_IMMUNE_TICK, 10000, nosleep_tick_fn, NULL, NULL);
    ASSERT_EQ(rc, -1, "should reject NULL coord");
    PASS();
}

static void test_register_driven_invalid_type(void) {
    TEST("register_driven invalid type -> -1");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    int rc1 = brain_cycle_coordinator_register_driven(
        coord, (brain_cycle_type_t)-1, 10000, nosleep_tick_fn, NULL, NULL);
    int rc2 = brain_cycle_coordinator_register_driven(
        coord, (brain_cycle_type_t)BRAIN_CYCLE_COUNT, 10000,
        nosleep_tick_fn, NULL, NULL);

    brain_cycle_coordinator_destroy(coord);
    ASSERT_EQ(rc1, -1, "should reject negative type");
    ASSERT_EQ(rc2, -1, "should reject out-of-range type");
    PASS();
}

static void test_register_driven_null_tick_fn(void) {
    TEST("register_driven NULL tick_fn -> -1");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 10000, NULL, NULL, NULL);

    brain_cycle_coordinator_destroy(coord);
    ASSERT_EQ(rc, -1, "should reject NULL tick_fn");
    PASS();
}

static void test_register_driven_interval_below_1ms(void) {
    TEST("register_driven interval_us < 1000 -> -1");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    int rc_zero = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 0, nosleep_tick_fn, NULL, NULL);
    int rc_999 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 999, nosleep_tick_fn, NULL, NULL);

    brain_cycle_coordinator_destroy(coord);
    ASSERT_EQ(rc_zero, -1, "should reject 0");
    ASSERT_EQ(rc_999, -1, "should reject 999us");
    PASS();
}

static void test_register_driven_conflicts_with_register(void) {
    TEST("register_driven conflicts with prior register()");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    int rc1 = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, NULL, NULL);
    ASSERT_EQ(rc1, 0, "first register() should succeed");

    int rc2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 10000, nosleep_tick_fn, NULL, NULL);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_IMMUNE_TICK);
    brain_cycle_coordinator_destroy(coord);
    ASSERT_EQ(rc2, -1, "register_driven on live registration must fail");
    PASS();
}

static void test_register_driven_double_driven(void) {
    TEST("register_driven twice on same type -> -1 second time");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t tc = {0};
    int rc1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 5000, tick_counter_fn, &tc, NULL);
    int rc2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 5000, tick_counter_fn, &tc, NULL);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    brain_cycle_coordinator_destroy(coord);
    ASSERT_EQ(rc1, 0, "first driven register should succeed");
    ASSERT_EQ(rc2, -1, "second driven register should be rejected");
    PASS();
}

static void test_register_driven_happy_path(void) {
    TEST("register_driven happy path: tick_fn invoked, stats update");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t tc = {0};
    const uint64_t interval_us = 10000; /* 10ms */
    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, interval_us,
        tick_counter_fn, &tc, NULL);
    ASSERT_EQ(rc, 0, "register_driven should succeed");

    /* Wait for several ticks — 200ms should give ~20 ticks at 10ms. */
    sleep_ms(200);

    int ticks = atomic_load(&tc.count);
    ASSERT_TRUE(ticks >= 2, "tick_fn must fire within 2 intervals");

    brain_cycle_status_t status;
    int rs = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &status);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_EQ(rs, 0, "get_status should succeed");
    ASSERT_TRUE(status.ticks_executed >= 2, "stats ticks_executed not updated");
    ASSERT_EQ(status.expected_interval_us, interval_us,
              "expected_interval_us should match registration");
    PASS();
}

static void test_unregister_driven_joins_thread(void) {
    TEST("unregister joins driven thread, clears registration");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t tc = {0};
    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_HEALTH_AGENT, 5000, tick_counter_fn, &tc, NULL);
    ASSERT_EQ(rc, 0, "register_driven failed");

    sleep_ms(60);

    int ur = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_HEALTH_AGENT);
    ASSERT_EQ(ur, 0, "unregister should succeed");

    int count_at_unreg = atomic_load(&tc.count);
    /* After unregister returns, the driver thread is joined — no further
     * ticks possible. Verify the count stays stable over 100ms. */
    sleep_ms(100);
    int count_after = atomic_load(&tc.count);
    ASSERT_EQ(count_after, count_at_unreg,
              "tick_fn fired after unregister returned (thread not joined)");

    /* Second unregister should fail (already unregistered). */
    int ur2 = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_HEALTH_AGENT);

    brain_cycle_coordinator_destroy(coord);
    ASSERT_EQ(ur2, -1, "second unregister should fail (not registered)");
    PASS();
}

static void test_register_driven_reregister_after_unregister(void) {
    TEST("register_driven after unregister succeeds (state cleared)");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t tc1 = {0};
    tick_counter_t tc2 = {0};

    int rc1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, 5000, tick_counter_fn, &tc1, NULL);
    ASSERT_EQ(rc1, 0, "initial register_driven failed");
    sleep_ms(40);

    int ur = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_BRAIN_UPDATE);
    ASSERT_EQ(ur, 0, "unregister failed");

    int rc2 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, 5000, tick_counter_fn, &tc2, NULL);
    ASSERT_EQ(rc2, 0, "re-register after unregister should succeed");

    sleep_ms(60);
    int t2 = atomic_load(&tc2.count);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_BRAIN_UPDATE);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_TRUE(t2 >= 1, "second registration did not tick");
    PASS();
}

static void test_destroy_with_active_driven(void) {
    TEST("destroy() cleans up active driven cycles (no hang)");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t tc = {0};
    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 5000, tick_counter_fn, &tc, NULL);
    ASSERT_EQ(rc, 0, "register_driven failed");

    sleep_ms(30);

    /* destroy() with live driver must stop + join + free without hanging. */
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    brain_cycle_coordinator_destroy(coord);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000 +
                      (t1.tv_nsec - t0.tv_nsec) / 1000000L;
    /* Join should complete in under 500ms — driver slice is 100ms. */
    ASSERT_TRUE(elapsed_ms < 500,
        "destroy() took too long (possible thread-leak / hang)");
    PASS();
}

static void test_register_driven_with_health_fn(void) {
    TEST("register_driven happy path with health_fn");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    tick_counter_t tc = {0};
    int rc = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_OSCILLATIONS, 5000, tick_counter_fn, &tc,
        always_healthy_fn);
    ASSERT_EQ(rc, 0, "register_driven with health_fn failed");

    sleep_ms(40);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_TRUE(atomic_load(&tc.count) >= 1, "tick_fn did not fire");
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Unit Tests: brain_cycle_coordinator_register_driven ===\n\n");

    test_register_driven_null_coord();
    test_register_driven_invalid_type();
    test_register_driven_null_tick_fn();
    test_register_driven_interval_below_1ms();
    test_register_driven_conflicts_with_register();
    test_register_driven_double_driven();
    test_register_driven_happy_path();
    test_unregister_driven_joins_thread();
    test_register_driven_reregister_after_unregister();
    test_destroy_with_active_driven();
    test_register_driven_with_health_fn();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
