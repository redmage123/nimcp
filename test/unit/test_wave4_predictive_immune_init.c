/**
 * @file test_wave4_predictive_immune_init.c
 * @brief Unit tests for Wave 4 predictive-immune factory init.
 *
 * WHAT: Validates nimcp_brain_factory_init_predictive_immune_subsystem and
 *       nimcp_brain_factory_destroy_predictive_immune_subsystem across the
 *       full set of prereq shapes (no coord, no predictive_network, no
 *       immune_system, happy path, idempotent, full lifecycle with ticks).
 *
 * WHY:  Wave 4 converted predictive_immune from a "statue" (implementation
 *       present, never instantiated) into a live driven cycle. Any regression
 *       in the prereq checks or the coordinator registration/unregistration
 *       would leave either a dormant module or a leaked driver thread.
 *
 * HOW:  Most tests use a calloc'd `struct brain_struct` with only the fields
 *       the factory reads set to concrete values. The full-lifecycle test
 *       creates real predictive_network + immune_system handles so the 100ms
 *       tick can fire without dereferencing fake pointers.
 *
 * CAVEATS: We calloc sizeof(struct brain_struct) directly, matching the
 *       pattern used by test/unit/core/brain/regions/brainstem/*.cpp. That
 *       pulls in the internal header — fine for test code but NEVER do this
 *       in production.
 */

/* POSIX for nanosleep, clock_gettime. */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/brain/factory/init/nimcp_brain_init_predictive_immune.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "cognitive/nimcp_predictive.h"
#include "cognitive/immune/nimcp_brain_immune.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-72s", name); fflush(stdout); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)
#define ASSERT_FALSE(cond, msg) do { if ((cond)) { FAIL(msg); } } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((long long)(a) != (long long)(b)) { \
    printf("[FAIL] %s (got %lld, expected %lld)\n", msg, (long long)(a), (long long)(b)); \
    tests_failed++; return; } } while(0)
#define ASSERT_NOT_NULL(p, msg) do { if ((p) == NULL) { FAIL(msg); } } while(0)
#define ASSERT_NULL(p, msg)     do { if ((p) != NULL) { FAIL(msg); } } while(0)

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void make_quiet_cfg(brain_cycle_coordinator_config_t* c) {
    brain_cycle_coordinator_default_config(c);
    c->enable_logging = false;
    c->enable_debug_logging = false;
}

static struct brain_struct* alloc_bare_brain(void) {
    /* calloc zero-initializes — no garbage in any unread field. */
    struct brain_struct* b = (struct brain_struct*)calloc(1, sizeof(struct brain_struct));
    return b;
}

static void sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------------- */
/* NULL-safety tests                                                         */
/* ------------------------------------------------------------------------- */

static void test_init_null_brain(void) {
    TEST("init(NULL) -> false, NIMCP_THROW_TO_IMMUNE log expected");
    bool rc = nimcp_brain_factory_init_predictive_immune_subsystem(NULL);
    ASSERT_FALSE(rc, "init(NULL) should return false");
    PASS();
}

static void test_destroy_null_brain(void) {
    TEST("destroy(NULL) -> safe no-op");
    nimcp_brain_factory_destroy_predictive_immune_subsystem(NULL);
    /* No crash = success. */
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Prereq-absent tests                                                       */
/* ------------------------------------------------------------------------- */

static void test_init_no_coordinator(void) {
    TEST("init with no cycle_coordinator -> false, field stays NULL");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");
    /* cycle_coordinator == NULL, cycle_coordinator_enabled == false. */

    bool rc = nimcp_brain_factory_init_predictive_immune_subsystem(b);

    ASSERT_FALSE(rc, "init should fail when coord absent");
    ASSERT_NULL(b->predictive_immune, "predictive_immune must stay NULL");
    ASSERT_FALSE(b->predictive_immune_enabled, "enabled flag must stay false");
    free(b);
    PASS();
}

static void test_init_coordinator_disabled(void) {
    TEST("init with coordinator present but disabled flag -> false");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create coord failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = false;  /* explicitly disabled */

    bool rc = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_FALSE(rc, "init should skip when coord disabled");
    ASSERT_NULL(b->predictive_immune, "predictive_immune must stay NULL");

    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

static void test_init_null_predictive_network(void) {
    TEST("init with NULL predictive_network -> false, field stays NULL");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create coord failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;
    b->predictive_network = NULL;
    /* Fake immune_system pointer is irrelevant — predictive_network is
     * checked first and we short-circuit. */
    b->immune_system = (brain_immune_system_t*)0xdeadbeef;

    bool rc = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_FALSE(rc, "init should fail when predictive_network NULL");
    ASSERT_NULL(b->predictive_immune, "predictive_immune must stay NULL");
    ASSERT_FALSE(b->predictive_immune_enabled, "enabled flag must stay false");

    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

static void test_init_null_immune_system(void) {
    TEST("init with NULL immune_system -> false, field stays NULL");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create coord failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;
    /* Create a real predictive network — cheap. */
    predictive_config_t pc = predictive_default_config();
    predictive_network_t pnet = predictive_create(&pc);
    ASSERT_NOT_NULL(pnet, "predictive_create failed");
    b->predictive_network = pnet;
    b->immune_system = NULL;

    bool rc = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_FALSE(rc, "init should fail when immune_system NULL");
    ASSERT_NULL(b->predictive_immune, "predictive_immune must stay NULL");
    ASSERT_FALSE(b->predictive_immune_enabled, "enabled flag must stay false");

    predictive_destroy(pnet);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Happy path + idempotency                                                  */
/* ------------------------------------------------------------------------- */

static void test_init_happy_path(void) {
    TEST("happy path: all prereqs present -> true, field populated");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create coord failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    predictive_config_t pc = predictive_default_config();
    predictive_network_t pnet = predictive_create(&pc);
    ASSERT_NOT_NULL(pnet, "predictive_create failed");
    b->predictive_network = pnet;

    brain_immune_config_t ic;
    (void)brain_immune_default_config(&ic);
    brain_immune_system_t* is = brain_immune_create(&ic);
    ASSERT_NOT_NULL(is, "brain_immune_create failed");
    b->immune_system = is;

    bool rc = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_TRUE(rc, "happy-path init should succeed");
    ASSERT_NOT_NULL(b->predictive_immune, "predictive_immune must be populated");
    ASSERT_TRUE(b->predictive_immune_enabled, "enabled flag must be true");

    /* Clean up via public destroy. */
    nimcp_brain_factory_destroy_predictive_immune_subsystem(b);
    ASSERT_NULL(b->predictive_immune, "destroy must clear the field");
    ASSERT_FALSE(b->predictive_immune_enabled, "destroy must clear enabled");

    brain_immune_destroy(is);
    predictive_destroy(pnet);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

static void test_init_idempotent(void) {
    TEST("second init call returns true without re-creating the module");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    predictive_config_t pc = predictive_default_config();
    predictive_network_t pnet = predictive_create(&pc);
    b->predictive_network = pnet;

    brain_immune_config_t ic; (void)brain_immune_default_config(&ic);
    brain_immune_system_t* is = brain_immune_create(&ic);
    b->immune_system = is;

    bool rc1 = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_TRUE(rc1, "first init failed");
    void* first = b->predictive_immune;
    ASSERT_NOT_NULL(first, "first init did not populate field");

    bool rc2 = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_TRUE(rc2, "second init should return true");
    ASSERT_TRUE(b->predictive_immune == first,
        "second init must not re-create the module (pointer changed)");

    nimcp_brain_factory_destroy_predictive_immune_subsystem(b);
    brain_immune_destroy(is);
    predictive_destroy(pnet);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Destroy semantics                                                         */
/* ------------------------------------------------------------------------- */

static void test_destroy_clears_and_unregisters(void) {
    TEST("destroy: clears field, unregisters cycle (second unregister fails)");
    struct brain_struct* b = alloc_bare_brain();
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    predictive_config_t pc = predictive_default_config();
    predictive_network_t pnet = predictive_create(&pc);
    b->predictive_network = pnet;

    brain_immune_config_t ic; (void)brain_immune_default_config(&ic);
    brain_immune_system_t* is = brain_immune_create(&ic);
    b->immune_system = is;

    bool rc = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_TRUE(rc, "init failed");

    nimcp_brain_factory_destroy_predictive_immune_subsystem(b);
    ASSERT_NULL(b->predictive_immune, "destroy did not clear field");
    ASSERT_FALSE(b->predictive_immune_enabled, "destroy did not clear flag");

    /* Second unregister on the coord should fail — proves destroy did
     * unregister during the first call. */
    int ur = brain_cycle_coordinator_unregister(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE);
    ASSERT_EQ(ur, -1,
        "second unregister should fail — destroy should have already unregistered");

    brain_immune_destroy(is);
    predictive_destroy(pnet);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Full lifecycle with live ticking                                          */
/* ------------------------------------------------------------------------- */

static void test_full_lifecycle_200ms(void) {
    TEST("full lifecycle: init -> run 300ms -> destroy, ticks > 0");
    struct brain_struct* b = alloc_bare_brain();
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    predictive_config_t pc = predictive_default_config();
    predictive_network_t pnet = predictive_create(&pc);
    b->predictive_network = pnet;

    brain_immune_config_t ic; (void)brain_immune_default_config(&ic);
    brain_immune_system_t* is = brain_immune_create(&ic);
    b->immune_system = is;

    bool rc = nimcp_brain_factory_init_predictive_immune_subsystem(b);
    ASSERT_TRUE(rc, "init failed");

    /* Module registered a 100ms driven cycle.  300ms lets >=2 ticks fire
     * even on a slow loaded host. */
    sleep_ms(300);

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_PREDICTIVE_IMMUNE, &st);
    ASSERT_EQ(g, 0, "get_status failed");
    ASSERT_TRUE(st.ticks_executed >= 1,
        "tick count zero — driver thread never ran");
    ASSERT_EQ(st.expected_interval_us, 100000ULL,
        "expected_interval_us should match 100ms registration");

    nimcp_brain_factory_destroy_predictive_immune_subsystem(b);

    brain_immune_destroy(is);
    predictive_destroy(pnet);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Unit Tests: Wave 4 predictive_immune factory init ===\n\n");

    test_init_null_brain();
    test_destroy_null_brain();
    test_init_no_coordinator();
    test_init_coordinator_disabled();
    test_init_null_predictive_network();
    test_init_null_immune_system();
    test_init_happy_path();
    test_init_idempotent();
    test_destroy_clears_and_unregisters();
    test_full_lifecycle_200ms();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
