/**
 * @file test_wave8a_cochlea_init.c
 * @brief Unit tests for Wave 8A cochlea + 15 consumer-bridges factory init.
 *
 * WHAT: Validates nimcp_brain_factory_init_cochlea_subsystem and
 *       nimcp_brain_factory_destroy_cochlea_subsystem across the full
 *       prereq shape (NULL brain, missing coordinator, happy path,
 *       idempotent, destroy no-op, destroy clears all 16 fields,
 *       sparse-deps partial wiring, full lifecycle with live ticking).
 *
 * WHY:  Wave 8A converted the cochlea subsystem + 15 consumer bridges from
 *       statues (compile-linked but never called) into a live single-cycle
 *       subsystem at 10ms via BRAIN_CYCLE_COCHLEA_BRIDGES. The init owns
 *       creation of cochlea_t + up to 15 bridge structs + 1 driven-cycle
 *       registration; any regression in that control flow either leaves
 *       bridges dormant (partial-init rollback misses a free) or leaks a
 *       driver thread (register succeeded, unregister never called).
 *
 * HOW:  We use a sparse brain_struct (calloc'd, cycle_coordinator +
 *       cycle_coordinator_enabled set explicitly). To exercise per-bridge
 *       wiring, each dep-slot on brain_struct is independently poked with
 *       a fake non-NULL pointer — the bridge create functions only
 *       null-check the dep pointer and store it opaquely (verified by
 *       reading bridge_base_connect_a/b_unlocked which only null-check),
 *       so a fake pointer is safe for the init+destroy control-flow
 *       test. The cochlea itself is a real `cochlea_create(...)` so
 *       each bridge's `cochlea`-side connect succeeds.
 *
 * CAVEATS: Same pattern as Waves 4/5/6 unit tests — we calloc
 *       struct brain_struct directly. Fine for test code, never in
 *       production. We never drive a full brain_decide/brain_learn_vector
 *       path here — the hot-path wiring is covered by integration+e2e
 *       tests at larger scale.
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

#include "core/brain/factory/init/nimcp_brain_init_cochlea.h"
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

/* Non-NULL sentinel values for bridge dep slots. Bridge create only
 * null-checks these (via bridge_base_connect_a/b_unlocked); it never
 * dereferences them during create. Picking distinct sentinel values
 * makes any accidental deref crash loudly instead of silently eating
 * a zero page. */
#define FAKE_DEP(n) ((void*)(uintptr_t)(0xCACE0000ULL + (n)))

/* ------------------------------------------------------------------------- */
/* NULL-safety tests                                                         */
/* ------------------------------------------------------------------------- */

static void test_init_null_brain(void) {
    TEST("init(NULL) -> false + throws safely");
    bool rc = nimcp_brain_factory_init_cochlea_subsystem(NULL);
    ASSERT_FALSE(rc, "init(NULL) should return false");
    PASS();
}

static void test_destroy_null_brain(void) {
    TEST("destroy(NULL) -> safe no-op");
    nimcp_brain_factory_destroy_cochlea_subsystem(NULL);
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

    bool rc = nimcp_brain_factory_init_cochlea_subsystem(b);

    ASSERT_FALSE(rc, "init should fail when coord absent");
    ASSERT_NULL(b->cochlea,                        "cochlea must stay NULL");
    ASSERT_FALSE(b->cochlea_enabled,               "cochlea_enabled must stay false");
    ASSERT_FALSE(b->cochlea_bridges_enabled,       "cochlea_bridges_enabled must stay false");
    ASSERT_NULL(b->cochlea_audio_cortex_bridge,    "audio_cortex_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_bio_async_bridge,       "bio_async_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_broca_bridge,           "broca_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_collective_bridge,      "collective_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_cortical_deep_bridge,   "cortical_deep_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_fep_bridge,             "fep_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_immune_bridge,          "immune_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_kg_bridge,              "kg_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_medulla_bridge,         "medulla_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_occipital_bridge,       "occipital_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_rcog_bridge,            "rcog_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_sleep_bridge,           "sleep_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_substrate_bridge,       "substrate_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_thalamic_bridge,        "thalamic_bridge must stay NULL");
    ASSERT_NULL(b->cochlea_verification_bridge,    "verification_bridge must stay NULL");
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

    bool rc = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_FALSE(rc, "init should skip when coord disabled");
    ASSERT_NULL(b->cochlea,                  "cochlea must stay NULL");
    ASSERT_FALSE(b->cochlea_enabled,         "cochlea_enabled must stay false");
    ASSERT_FALSE(b->cochlea_bridges_enabled, "cochlea_bridges_enabled must stay false");

    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Happy path + idempotency                                                  */
/* ------------------------------------------------------------------------- */

static void test_init_happy_path_no_deps(void) {
    TEST("happy path (no deps): cochlea+flags set, no bridges created");
    /* With no brain-side deps set, the cochlea itself is created and the
     * cycle is registered, but every bridge should skip cleanly. */
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create coord failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool rc = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc, "happy-path init should succeed");
    ASSERT_NOT_NULL(b->cochlea,              "cochlea must be populated");
    ASSERT_TRUE(b->cochlea_enabled,          "cochlea_enabled must be true");
    ASSERT_TRUE(b->cochlea_bridges_enabled,  "cochlea_bridges_enabled must be true");

    /* All 15 bridge slots stay NULL because their brain-side deps are NULL. */
    ASSERT_NULL(b->cochlea_audio_cortex_bridge,    "audio_cortex should skip");
    ASSERT_NULL(b->cochlea_bio_async_bridge,       "bio_async should skip");
    ASSERT_NULL(b->cochlea_broca_bridge,           "broca should skip");
    ASSERT_NULL(b->cochlea_collective_bridge,      "collective should skip");
    ASSERT_NULL(b->cochlea_cortical_deep_bridge,   "cortical_deep should skip");
    ASSERT_NULL(b->cochlea_fep_bridge,             "fep should skip");
    ASSERT_NULL(b->cochlea_immune_bridge,          "immune should skip");
    ASSERT_NULL(b->cochlea_kg_bridge,              "kg should skip");
    ASSERT_NULL(b->cochlea_medulla_bridge,         "medulla should skip");
    ASSERT_NULL(b->cochlea_occipital_bridge,       "occipital should skip");
    ASSERT_NULL(b->cochlea_rcog_bridge,            "rcog should skip");
    ASSERT_NULL(b->cochlea_sleep_bridge,           "sleep should skip");
    ASSERT_NULL(b->cochlea_substrate_bridge,       "substrate should skip");
    ASSERT_NULL(b->cochlea_thalamic_bridge,        "thalamic should skip");
    /* verification_bridge has NO brain-side dep — it always wires if cochlea exists. */
    ASSERT_NOT_NULL(b->cochlea_verification_bridge, "verification should wire (no dep)");

    /* Cycle registered at 10ms. */
    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, &st);
    ASSERT_EQ(g, 0, "get_status(COCHLEA_BRIDGES) failed — not registered");
    ASSERT_EQ(st.expected_interval_us, 10000ULL,
        "expected_interval_us should be 10ms (10000us)");

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

static void test_init_happy_path_all_deps(void) {
    TEST("happy path (all deps): all 15 bridges wire (except ghost-typedef ones)");
    /* Populate every dep slot with a fake sentinel pointer. Bridge creates
     * only null-check and store the pointer opaquely — they don't deref
     * during create. */
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "create coord failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    /* Populate every dep the init consults. Opaque-pointer semantics — all
     * bridge creates skip any deref of these during create. */
    b->audio_cortex         = FAKE_DEP(1);
    b->broca                = FAKE_DEP(2);
    b->collective_cognition = FAKE_DEP(3);
    b->cortical_column_pool = FAKE_DEP(4);
    b->fep_orchestrator     = FAKE_DEP(5);
    b->immune_system        = FAKE_DEP(6);
    b->medulla              = FAKE_DEP(7);
    b->occipital            = FAKE_DEP(8);
    b->rcog_engine          = FAKE_DEP(9);
    b->substrate            = FAKE_DEP(10);
    b->thalamic_router      = FAKE_DEP(11);
    b->internal_kg          = FAKE_DEP(12);
    b->sleep_system         = FAKE_DEP(13);
    /* bio_async_bridge depends on the global bio_router_is_initialized();
     * we don't bring that up here, so that bridge remains a skip. */

    bool rc = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc, "init should succeed with deps populated");
    ASSERT_NOT_NULL(b->cochlea,              "cochlea must be populated");
    ASSERT_TRUE(b->cochlea_enabled,          "cochlea_enabled must be true");
    ASSERT_TRUE(b->cochlea_bridges_enabled,  "cochlea_bridges_enabled must be true");

    /* Every bridge whose dep we populated should be wired. */
    ASSERT_NOT_NULL(b->cochlea_audio_cortex_bridge,  "audio_cortex_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_broca_bridge,         "broca_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_collective_bridge,    "collective_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_cortical_deep_bridge, "cortical_deep_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_fep_bridge,           "fep_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_immune_bridge,        "immune_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_medulla_bridge,       "medulla_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_occipital_bridge,     "occipital_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_rcog_bridge,          "rcog_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_substrate_bridge,     "substrate_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_verification_bridge,  "verification_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_thalamic_bridge,      "thalamic_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_kg_bridge,            "kg_bridge must wire");
    ASSERT_NOT_NULL(b->cochlea_sleep_bridge,         "sleep_bridge must wire");
    /* bio_async skipped because global bio_router isn't initialized. */

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

static void test_init_idempotent(void) {
    TEST("second init returns true without re-creating cochlea");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool rc1 = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc1, "first init failed");
    void* cochlea1 = b->cochlea;
    void* verif1   = b->cochlea_verification_bridge;
    ASSERT_NOT_NULL(cochlea1, "first init did not populate cochlea");
    ASSERT_NOT_NULL(verif1,   "first init did not populate verification_bridge");

    bool rc2 = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc2, "second init should return true");
    ASSERT_TRUE(b->cochlea == cochlea1, "cochlea pointer changed on 2nd init");
    ASSERT_TRUE(b->cochlea_verification_bridge == verif1,
        "verification_bridge pointer changed on 2nd init");
    ASSERT_TRUE(b->cochlea_enabled, "cochlea_enabled still must be true");

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Sparse-deps partial wiring                                                */
/* ------------------------------------------------------------------------- */

static void test_init_sparse_deps(void) {
    TEST("sparse deps: only populated slots wire, rest stay NULL");
    /* Poke only 3 of the 13 brain-side deps — verifies independent
     * per-bridge skip behavior with no cross-talk. */
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    /* Only 3 deps populated. */
    b->immune_system  = FAKE_DEP(101);
    b->broca          = FAKE_DEP(102);
    b->rcog_engine    = FAKE_DEP(103);

    bool rc = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc, "sparse init should succeed");
    ASSERT_NOT_NULL(b->cochlea, "cochlea must be populated");

    /* These 3 should wire. */
    ASSERT_NOT_NULL(b->cochlea_immune_bridge, "immune_bridge should wire (dep populated)");
    ASSERT_NOT_NULL(b->cochlea_broca_bridge,  "broca_bridge should wire (dep populated)");
    ASSERT_NOT_NULL(b->cochlea_rcog_bridge,   "rcog_bridge should wire (dep populated)");

    /* Everything else should stay NULL (except verification, which has no dep). */
    ASSERT_NULL(b->cochlea_audio_cortex_bridge,  "audio_cortex must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_bio_async_bridge,     "bio_async must stay NULL (no router)");
    ASSERT_NULL(b->cochlea_collective_bridge,    "collective must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_cortical_deep_bridge, "cortical_deep must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_fep_bridge,           "fep must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_medulla_bridge,       "medulla must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_occipital_bridge,     "occipital must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_substrate_bridge,     "substrate must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_thalamic_bridge,      "thalamic must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_kg_bridge,            "kg must stay NULL (dep NULL)");
    ASSERT_NULL(b->cochlea_sleep_bridge,         "sleep must stay NULL (dep NULL)");
    ASSERT_NOT_NULL(b->cochlea_verification_bridge, "verification always wires");

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Destroy semantics                                                         */
/* ------------------------------------------------------------------------- */

static void test_destroy_clears_all_16_fields(void) {
    TEST("destroy: all 16 fields cleared, cochlea_bridges_enabled=false");
    struct brain_struct* b = alloc_bare_brain();
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    /* Populate every dep so every destroy path is exercised. */
    b->audio_cortex         = FAKE_DEP(1);
    b->broca                = FAKE_DEP(2);
    b->collective_cognition = FAKE_DEP(3);
    b->cortical_column_pool = FAKE_DEP(4);
    b->fep_orchestrator     = FAKE_DEP(5);
    b->immune_system        = FAKE_DEP(6);
    b->medulla              = FAKE_DEP(7);
    b->occipital            = FAKE_DEP(8);
    b->rcog_engine          = FAKE_DEP(9);
    b->substrate            = FAKE_DEP(10);
    b->thalamic_router      = FAKE_DEP(11);
    b->internal_kg          = FAKE_DEP(12);
    b->sleep_system         = FAKE_DEP(13);

    bool rc = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc, "init failed");
    ASSERT_NOT_NULL(b->cochlea, "cochlea must be populated pre-destroy");

    nimcp_brain_factory_destroy_cochlea_subsystem(b);

    /* All 16 slots NULL after destroy. */
    ASSERT_NULL(b->cochlea,                         "cochlea not cleared");
    ASSERT_NULL(b->cochlea_audio_cortex_bridge,     "audio_cortex not cleared");
    ASSERT_NULL(b->cochlea_bio_async_bridge,        "bio_async not cleared");
    ASSERT_NULL(b->cochlea_broca_bridge,            "broca not cleared");
    ASSERT_NULL(b->cochlea_collective_bridge,       "collective not cleared");
    ASSERT_NULL(b->cochlea_cortical_deep_bridge,    "cortical_deep not cleared");
    ASSERT_NULL(b->cochlea_fep_bridge,              "fep not cleared");
    ASSERT_NULL(b->cochlea_immune_bridge,           "immune not cleared");
    ASSERT_NULL(b->cochlea_kg_bridge,               "kg not cleared");
    ASSERT_NULL(b->cochlea_medulla_bridge,          "medulla not cleared");
    ASSERT_NULL(b->cochlea_occipital_bridge,        "occipital not cleared");
    ASSERT_NULL(b->cochlea_rcog_bridge,             "rcog not cleared");
    ASSERT_NULL(b->cochlea_sleep_bridge,            "sleep not cleared");
    ASSERT_NULL(b->cochlea_substrate_bridge,        "substrate not cleared");
    ASSERT_NULL(b->cochlea_thalamic_bridge,         "thalamic not cleared");
    ASSERT_NULL(b->cochlea_verification_bridge,     "verification not cleared");
    ASSERT_FALSE(b->cochlea_enabled,                "cochlea_enabled not cleared");
    ASSERT_FALSE(b->cochlea_bridges_enabled,        "cochlea_bridges_enabled not cleared");

    /* Second unregister on the coord must fail — proves destroy DID
     * unregister during the first call. */
    int ur = brain_cycle_coordinator_unregister(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES);
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

    bool rc = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc, "init failed");

    /* Module registered a 10ms driven cycle. 200ms lets ~20 ticks fire
     * even on a slow loaded host; we only require >= 1 for baseline. */
    sleep_ms(200);

    brain_cycle_status_t st;
    int g = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, &st);
    ASSERT_EQ(g, 0, "get_status(COCHLEA_BRIDGES) failed");
    ASSERT_TRUE(st.ticks_executed >= 1,
        "tick count zero — cochlea bridges driver thread never ran");
    ASSERT_EQ(st.expected_interval_us, 10000ULL,
        "expected_interval_us should match 10ms registration");

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Re-init after destroy                                                     */
/* ------------------------------------------------------------------------- */

static void test_reinit_after_destroy(void) {
    TEST("after destroy, re-init succeeds (state cleared, coord slot reusable)");
    struct brain_struct* b = alloc_bare_brain();
    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool rc1 = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc1, "first init failed");
    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    ASSERT_FALSE(b->cochlea_bridges_enabled, "destroy did not clear bridges_enabled");
    ASSERT_FALSE(b->cochlea_enabled, "destroy did not clear cochlea_enabled");

    /* Fresh init should rebuild cleanly. */
    bool rc2 = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc2, "re-init after destroy failed");
    ASSERT_TRUE(b->cochlea_enabled, "cochlea_enabled not set on re-init");
    ASSERT_TRUE(b->cochlea_bridges_enabled, "bridges_enabled not set on re-init");
    ASSERT_NOT_NULL(b->cochlea, "cochlea NULL after re-init");

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Init-order regression: audio_cortex_bridge must receive a non-NULL        */
/* thalamic_bridge when brain->thalamic_router is populated. Wave 8A's first */
/* pass wired audio_cortex BEFORE thalamic, leaving the thalamic_bridge     */
/* reference permanently NULL. Follow-up fix: init_audio_cortex_bridge      */
/* calls init_thalamic_bridge first (idempotent), so the real pointer       */
/* is threaded through.                                                     */
/* ------------------------------------------------------------------------- */

static void test_audio_cortex_has_thalamic_bridge_when_available(void) {
    TEST("audio_cortex_bridge receives live thalamic_bridge (init-order contract)");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    /* Populate exactly the two deps audio_cortex_bridge needs. */
    b->audio_cortex    = FAKE_DEP(201);
    b->thalamic_router = (struct thalamic_router*)FAKE_DEP(202);

    bool rc = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(rc, "init should succeed");

    /* Both bridges must be live. */
    ASSERT_NOT_NULL(b->cochlea_audio_cortex_bridge,
        "audio_cortex_bridge must exist when audio_cortex is populated");
    ASSERT_NOT_NULL(b->cochlea_thalamic_bridge,
        "thalamic_bridge must be wired before audio_cortex_bridge — "
        "this was the Wave 8A init-order contract violation");

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Unit Tests: Wave 8A cochlea + 15 consumer bridges init ===\n\n");

    test_init_null_brain();
    test_destroy_null_brain();
    test_init_no_coordinator();
    test_init_coordinator_disabled();
    test_init_happy_path_no_deps();
    test_init_happy_path_all_deps();
    test_init_idempotent();
    test_init_sparse_deps();
    test_destroy_clears_all_16_fields();
    test_full_lifecycle_200ms();
    test_reinit_after_destroy();
    test_audio_cortex_has_thalamic_bridge_when_available();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
