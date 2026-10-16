/**
 * @file test_cycle_coordinator_regression.c
 * @brief Regression tests for observation-only brain cycle coordinator APIs.
 *
 * WHAT: Guards that adding register_driven() + extending unregister() did not
 *       break the pre-existing register()/notify_tick()/check_health()/
 *       get_status()/dependency APIs for observation-only cycles.
 * WHY:  All prior callers (brain_update, immune_tick, etc.) use the classic
 *       observation pattern — any regression there breaks the whole brain.
 * HOW:  Exercise each original API on observation-only cycles and assert that
 *       nothing from the driven code path leaks through (e.g., no thread is
 *       spawned, unregister is synchronous).
 */

/* POSIX for clock_gettime, CLOCK_MONOTONIC. */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
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

static brain_cycle_health_t stub_health_fn(void* handle) {
    brain_cycle_health_t* v = (brain_cycle_health_t*)handle;
    return v ? *v : BRAIN_CYCLE_HEALTH_HEALTHY;
}

/* ------------------------------------------------------------------------- */

static void test_observation_register_unchanged(void) {
    TEST("observation-only register() + unregister() still work");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    /* Register same type twice — second should fail exactly as before. */
    int r1 = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, NULL, NULL);
    int r2 = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, NULL, NULL);
    int u1 = brain_cycle_coordinator_unregister(
        coord, BRAIN_CYCLE_BRAIN_UPDATE);
    int u2 = brain_cycle_coordinator_unregister(
        coord, BRAIN_CYCLE_BRAIN_UPDATE);

    brain_cycle_coordinator_destroy(coord);

    ASSERT_EQ(r1, 0, "register should succeed");
    ASSERT_EQ(r2, -1, "duplicate register should fail");
    ASSERT_EQ(u1, 0, "unregister should succeed");
    ASSERT_EQ(u2, -1, "double unregister should fail");
    PASS();
}

static void test_observation_unregister_is_synchronous(void) {
    TEST("observation unregister returns immediately (no driver thread)");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, NULL, NULL);
    ASSERT_EQ(rc, 0, "register failed");

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int ur = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_IMMUNE_TICK);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    ASSERT_EQ(ur, 0, "unregister failed");

    long elapsed_us = (t1.tv_sec - t0.tv_sec) * 1000000L +
                      (t1.tv_nsec - t0.tv_nsec) / 1000L;

    brain_cycle_coordinator_destroy(coord);

    /* Observation-only unregister should not block — no join needed.
     * Allow 20ms for scheduler jitter but should be microseconds typically. */
    ASSERT_TRUE(elapsed_us < 20000,
        "observation unregister blocked unexpectedly (did a thread start?)");
    PASS();
}

static void test_notify_tick_updates_stats(void) {
    TEST("notify_tick accumulates stats for observation-only cycle");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, NULL, NULL);
    ASSERT_EQ(rc, 0, "register failed");

    for (int i = 0; i < 7; i++) {
        int nt = brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_BRAIN_UPDATE, 1500);
        ASSERT_EQ(nt, 0, "notify_tick failed");
    }

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &st);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_BRAIN_UPDATE);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_EQ(g, 0, "get_status failed");
    ASSERT_EQ(st.ticks_executed, 7, "ticks_executed wrong");
    ASSERT_TRUE(st.avg_duration_us > 0.0, "avg_duration_us not updated");
    PASS();
}

static void test_notify_tick_without_registration_fails(void) {
    TEST("notify_tick on unregistered cycle still returns -1");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    int nt = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_GC_AGENT, 42);

    brain_cycle_coordinator_destroy(coord);
    ASSERT_EQ(nt, -1, "notify_tick on unregistered should return -1");
    PASS();
}

static void test_check_health_observation_only(void) {
    TEST("check_health() + get_status() still work for observation cycles");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    brain_cycle_health_t verdict = BRAIN_CYCLE_HEALTH_HEALTHY;
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &verdict, stub_health_fn);
    ASSERT_EQ(rc, 0, "register failed");

    (void)brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_OSCILLATIONS, 500);

    int issues = brain_cycle_coordinator_check_health(coord);
    ASSERT_TRUE(issues >= 0, "check_health failed");

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &st);
    ASSERT_EQ(g, 0, "get_status failed");
    ASSERT_EQ(st.health, BRAIN_CYCLE_HEALTH_HEALTHY, "health mismatch");

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_OSCILLATIONS);
    brain_cycle_coordinator_destroy(coord);
    PASS();
}

static void test_dependency_graph_unaffected(void) {
    TEST("add_dependency + check_dependencies still work");
    brain_cycle_coordinator_config_t cfg; make_quiet_config(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_TRUE(coord != NULL, "create failed");

    /* Register BRAIN_UPDATE and make it depend on IMMUNE_TICK. */
    brain_cycle_health_t immune_v = BRAIN_CYCLE_HEALTH_HEALTHY;
    brain_cycle_health_t update_v = BRAIN_CYCLE_HEALTH_HEALTHY;

    (void)brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &immune_v, stub_health_fn);
    (void)brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &update_v, stub_health_fn);

    int ad = brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, BRAIN_CYCLE_IMMUNE_TICK);
    ASSERT_EQ(ad, 0, "add_dependency failed");

    /* Update health state via check_health so dependency check has live data. */
    (void)brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 100);
    (void)brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, 100);
    (void)brain_cycle_coordinator_check_health(coord);

    bool sat = false;
    int cd = brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &sat);
    ASSERT_EQ(cd, 0, "check_dependencies failed");
    ASSERT_TRUE(sat, "dependency on healthy cycle should be satisfied");

    /* Now flip the dependency to ERROR and re-check. */
    immune_v = BRAIN_CYCLE_HEALTH_ERROR;
    (void)brain_cycle_coordinator_check_health(coord);

    sat = true;
    (void)brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &sat);

    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_IMMUNE_TICK);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_BRAIN_UPDATE);
    brain_cycle_coordinator_destroy(coord);

    ASSERT_TRUE(!sat, "dependency on ERROR cycle should NOT be satisfied");
    PASS();
}

int main(void) {
    printf("\n=== Regression Tests: observation-only brain cycle APIs ===\n\n");

    test_observation_register_unchanged();
    test_observation_unregister_is_synchronous();
    test_notify_tick_updates_stats();
    test_notify_tick_without_registration_fails();
    test_check_health_observation_only();
    test_dependency_graph_unaffected();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
