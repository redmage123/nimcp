/**
 * @file test_wave6_chemistry_init.c
 * @brief Unit tests for Wave 6 chemistry factory init.
 *
 * WHAT: Validates nimcp_brain_factory_init_chemistry_subsystem and
 *       nimcp_brain_factory_destroy_chemistry_subsystem across the full
 *       prereq shape (NULL, no coord, disabled coord, happy path,
 *       idempotent, destroy no-op, destroy clears, full lifecycle,
 *       re-init after destroy).
 *
 * WHY:  Wave 6 converted 4 chemistry modules from statues (compile-disabled)
 *       into a live single-cycle subsystem via the cycle coordinator at
 *       10ms. The init owns creation of 4 module structs + 1 driven-cycle
 *       registration; any regression in that control flow either leaves
 *       modules dormant (partial-init rollback misses a free) or leaks a
 *       driver thread (register succeeded, unregister never called).
 *
 * HOW:  We use a sparse brain_struct (calloc'd, cycle_coordinator +
 *       cycle_coordinator_enabled set explicitly) — the chemistry modules
 *       don't read any other brain field, so this is enough to exercise
 *       the full init+destroy path. We deliberately avoid pulling in any
 *       chemistry header here (their typedef collision with
 *       brain_internal.h would break the file). All verification is via
 *       the documented observable surface: `brain->chemistry_enabled`,
 *       the four opaque-struct pointers, and coordinator stats.
 *
 * CAVEATS: Same as Wave 4 unit test — we calloc struct brain_struct
 *       directly. Fine for test code, never in production.
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

#include "core/brain/factory/init/nimcp_brain_init_chemistry.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"

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
    bool rc = nimcp_brain_factory_init_chemistry_subsystem(NULL);
    ASSERT_FALSE(rc, "init(NULL) should return false");
    PASS();
}

static void test_destroy_null_brain(void) {
    TEST("destroy(NULL) -> safe no-op");
    nimcp_brain_factory_destroy_chemistry_subsystem(NULL);
    /* No crash = success. */
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Prereq-absent tests                                                       */
/* ------------------------------------------------------------------------- */

static void test_init_no_coordinator(void) {
    TEST("init with no cycle_coordinator -> false, fields stay NULL");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");
    /* cycle_coordinator == NULL, cycle_coordinator_enabled == false. */

    bool rc = nimcp_brain_factory_init_chemistry_subsystem(b);

    ASSERT_FALSE(rc, "init should fail when coord absent");
    ASSERT_NULL(b->ph_system,      "ph_system must stay NULL");
    ASSERT_NULL(b->proton_pumps,   "proton_pumps must stay NULL");
    ASSERT_NULL(b->buffer_manager, "buffer_manager must stay NULL");
    ASSERT_NULL(b->no_system,      "no_system must stay NULL");
    ASSERT_FALSE(b->chemistry_enabled, "chemistry_enabled must stay false");
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

    bool rc = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_FALSE(rc, "init should skip when coord disabled");
    ASSERT_NULL(b->ph_system,      "ph_system must stay NULL");
    ASSERT_NULL(b->proton_pumps,   "proton_pumps must stay NULL");
    ASSERT_NULL(b->buffer_manager, "buffer_manager must stay NULL");
    ASSERT_NULL(b->no_system,      "no_system must stay NULL");
    ASSERT_FALSE(b->chemistry_enabled, "chemistry_enabled must stay false");

    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Happy path + idempotency                                                  */
/* ------------------------------------------------------------------------- */

static void test_init_happy_path(void) {
    TEST("happy path: 4 fields populated, chemistry_enabled, cycle registered");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create coord failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool rc = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(rc, "happy-path init should succeed");
    ASSERT_NOT_NULL(b->ph_system,      "ph_system must be populated");
    ASSERT_NOT_NULL(b->proton_pumps,   "proton_pumps must be populated");
    ASSERT_NOT_NULL(b->buffer_manager, "buffer_manager must be populated");
    ASSERT_NOT_NULL(b->no_system,      "no_system must be populated");
    ASSERT_TRUE(b->chemistry_enabled,  "chemistry_enabled must be true");

    /* Verify cycle registration via get_status — registered cycles return 0. */
    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_CHEMISTRY, &st);
    ASSERT_EQ(g, 0, "get_status(CHEMISTRY) failed — not registered");
    ASSERT_EQ(st.expected_interval_us, 10000ULL,
        "expected_interval_us should be 10ms (10000us)");

    /* Clean up via public destroy. */
    nimcp_brain_factory_destroy_chemistry_subsystem(b);

    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

static void test_init_idempotent(void) {
    TEST("second init returns true without re-creating structs");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool rc1 = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(rc1, "first init failed");
    void* ph1  = b->ph_system;
    void* pp1  = b->proton_pumps;
    void* bm1  = b->buffer_manager;
    void* no1  = b->no_system;
    ASSERT_NOT_NULL(ph1, "first init did not populate ph_system");
    ASSERT_NOT_NULL(pp1, "first init did not populate proton_pumps");
    ASSERT_NOT_NULL(bm1, "first init did not populate buffer_manager");
    ASSERT_NOT_NULL(no1, "first init did not populate no_system");

    bool rc2 = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(rc2, "second init should return true");
    ASSERT_TRUE(b->ph_system      == ph1, "ph_system pointer changed on 2nd init");
    ASSERT_TRUE(b->proton_pumps   == pp1, "proton_pumps pointer changed on 2nd init");
    ASSERT_TRUE(b->buffer_manager == bm1, "buffer_manager pointer changed on 2nd init");
    ASSERT_TRUE(b->no_system      == no1, "no_system pointer changed on 2nd init");
    ASSERT_TRUE(b->chemistry_enabled, "chemistry_enabled still must be true");

    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Destroy semantics                                                         */
/* ------------------------------------------------------------------------- */

static void test_destroy_clears_and_unregisters(void) {
    TEST("destroy: all 4 fields NULL, flag false, cycle unregistered");
    struct brain_struct* b = alloc_bare_brain();
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool rc = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(rc, "init failed");

    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    ASSERT_NULL(b->ph_system,      "destroy did not clear ph_system");
    ASSERT_NULL(b->proton_pumps,   "destroy did not clear proton_pumps");
    ASSERT_NULL(b->buffer_manager, "destroy did not clear buffer_manager");
    ASSERT_NULL(b->no_system,      "destroy did not clear no_system");
    ASSERT_FALSE(b->chemistry_enabled, "destroy did not clear flag");

    /* Second unregister on the coord must fail — proves destroy DID
     * unregister during the first call. */
    int ur = brain_cycle_coordinator_unregister(
        coord, BRAIN_CYCLE_CHEMISTRY);
    ASSERT_EQ(ur, -1,
        "second unregister must fail — destroy should have already unregistered");

    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Full lifecycle with live ticking                                          */
/* ------------------------------------------------------------------------- */

static void test_full_lifecycle_200ms(void) {
    TEST("full lifecycle: init -> run 200ms -> destroy, ticks > 0");
    struct brain_struct* b = alloc_bare_brain();
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool rc = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(rc, "init failed");

    /* Module registered a 10ms driven cycle. 200ms lets ~20 ticks fire
     * even on a slow loaded host; we only require >= 1 for baseline. */
    sleep_ms(200);

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_CHEMISTRY, &st);
    ASSERT_EQ(g, 0, "get_status(CHEMISTRY) failed");
    ASSERT_TRUE(st.ticks_executed >= 1,
        "tick count zero — chemistry driver thread never ran");
    ASSERT_EQ(st.expected_interval_us, 10000ULL,
        "expected_interval_us should match 10ms registration");

    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Re-init after destroy                                                     */
/* ------------------------------------------------------------------------- */

static void test_reinit_after_destroy(void) {
    TEST("after destroy, re-init succeeds (state cleared, coord empty slot)");
    struct brain_struct* b = alloc_bare_brain();
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool rc1 = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(rc1, "first init failed");
    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    ASSERT_FALSE(b->chemistry_enabled, "destroy did not clear flag");

    /* Fresh init should rebuild cleanly. */
    bool rc2 = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(rc2, "re-init after destroy failed");
    ASSERT_TRUE(b->chemistry_enabled, "flag not set on re-init");
    ASSERT_NOT_NULL(b->ph_system,      "ph_system NULL after re-init");
    ASSERT_NOT_NULL(b->proton_pumps,   "proton_pumps NULL after re-init");
    ASSERT_NOT_NULL(b->buffer_manager, "buffer_manager NULL after re-init");
    ASSERT_NOT_NULL(b->no_system,      "no_system NULL after re-init");

    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Unit Tests: Wave 6 chemistry factory init ===\n\n");

    test_init_null_brain();
    test_destroy_null_brain();
    test_init_no_coordinator();
    test_init_coordinator_disabled();
    test_init_happy_path();
    test_init_idempotent();
    test_destroy_clears_and_unregisters();
    test_full_lifecycle_200ms();
    test_reinit_after_destroy();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
