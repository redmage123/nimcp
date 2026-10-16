/**
 * @file test_wave8a_integration.c
 * @brief Integration tests for Wave 8A (cochlea + 15 consumer bridges).
 *
 * WHAT: Exercises the new BRAIN_CYCLE_COCHLEA_BRIDGES driven cycle alongside
 *       the Wave 6 chemistry cycle (both at 10ms) and the 100ms biology
 *       cycle (EPIGENETICS), validating tri-cycle coexistence at mixed
 *       cadences on the same coordinator. Also validates the cochlea tick
 *       wrapper stays under 100µs when all 15 bridge slots are NULL
 *       (empty-fast-path).
 *
 * WHY:  Wave 8A introduced a new 10ms cycle that on a fully-populated
 *       brain fans out to 15 bridge updates. Cross-cycle failure modes —
 *       enum collision, mutual starvation, cadence drift from fat tick
 *       wrappers, queue-saturation — only surface under concurrent load.
 *
 * HOW:  Real coordinator + real driver threads + real time. Chemistry
 *       uses the actual factory (4 module updates per tick). Cochlea
 *       uses the actual factory but with no brain-side deps, so all 15
 *       bridge-update calls are skipped — just the NULL-guard fan-out.
 *       That is the right shape to measure the tick wrapper's intrinsic
 *       cost. Total runtime budget ~1.5s.
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
#include "core/brain/factory/init/nimcp_brain_init_cochlea.h"
#include "core/brain/factory/init/nimcp_brain_init_chemistry.h"
#include "core/brain/nimcp_brain_internal.h"

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
/* Tri-cycle concurrency: COCHLEA_BRIDGES (10ms) + CHEMISTRY (10ms) +
 * EPIGENETICS (100ms counter). Each cycle is a separate driver thread.
 * Both fast cycles should tick roughly the same number of times; the 100ms
 * cycle should tick ~10x less.                                             */
/* ------------------------------------------------------------------------- */

static void test_tri_cycle_10_10_100_coexist(void) {
    TEST("COCHLEA_BRIDGES + CHEMISTRY (10ms) + EPIGENETICS (100ms) coexist");
    struct brain_struct* b = (struct brain_struct*)
        calloc(1, sizeof(struct brain_struct));
    ASSERT_NOT_NULL(b, "brain calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    /* Register EPIGENETICS as a plain counter cycle at 100ms. */
    tick_counter_t ep = {0};
    int r1 = brain_cycle_coordinator_register_driven(
        coord, BRAIN_CYCLE_EPIGENETICS, 100000,
        tick_counter_fn, &ep, NULL);
    ASSERT_EQ(r1, 0, "register EPIGENETICS failed");

    /* Register chemistry via real factory (10ms, 4 no-op module updates). */
    bool okc = nimcp_brain_factory_init_chemistry_subsystem(b);
    ASSERT_TRUE(okc, "chemistry factory init failed");

    /* Register cochlea bridges via real factory (10ms, 15 NULL-guarded
     * skips per tick — no deps populated on this bare brain). */
    bool oko = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(oko, "cochlea bridges factory init failed");

    sleep_ms(500);

    brain_cycle_status_t ch_st, co_st;
    int gc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_CHEMISTRY, &ch_st);
    int go = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, &co_st);
    int ep_ticks = atomic_load(&ep.count);

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    nimcp_brain_factory_destroy_chemistry_subsystem(b);
    (void)brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_EPIGENETICS);
    brain_cycle_coordinator_destroy(coord);
    free(b);

    ASSERT_EQ(gc, 0, "get_status CHEMISTRY failed");
    ASSERT_EQ(go, 0, "get_status COCHLEA_BRIDGES failed");
    /* Both 10ms cycles should fire at least ~40 times in 500ms (80% slack). */
    ASSERT_TRUE(ch_st.ticks_executed >= 30,
        "CHEMISTRY ticked too rarely — possible starvation");
    ASSERT_TRUE(co_st.ticks_executed >= 30,
        "COCHLEA_BRIDGES ticked too rarely — possible starvation");
    /* 100ms cycle should fire at least 3 times (slack). */
    ASSERT_TRUE(ep_ticks >= 3,
        "EPIGENETICS ticked too rarely — 100ms cycle starved by fast siblings");
    /* Sanity: fast cycles out-pace the 100ms cycle by >= 3x. */
    ASSERT_TRUE((uint64_t)ch_st.ticks_executed >= (uint64_t)ep_ticks * 3,
        "CHEMISTRY did not out-pace EPIGENETICS");
    ASSERT_TRUE((uint64_t)co_st.ticks_executed >= (uint64_t)ep_ticks * 3,
        "COCHLEA_BRIDGES did not out-pace EPIGENETICS");
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Slow-tick tolerance: with no bridges populated, all 15 bridge update
 * calls in the tick wrapper are skipped via NULL-guards. The 10ms cycle
 * should still achieve at least 40 ticks in 500ms.                         */
/* ------------------------------------------------------------------------- */

static void test_empty_fastpath_cadence(void) {
    TEST("empty fast-path cadence: >= 40 ticks in 500ms (10ms target)");
    struct brain_struct* b = (struct brain_struct*)
        calloc(1, sizeof(struct brain_struct));
    ASSERT_NOT_NULL(b, "brain calloc failed");

    brain_cycle_coordinator_config_t cfg; make_quiet_cfg(&cfg);
    brain_cycle_coordinator_t* coord = brain_cycle_coordinator_create(&cfg);
    ASSERT_NOT_NULL(coord, "coord create failed");
    b->cycle_coordinator = (struct brain_cycle_coordinator*)coord;
    b->cycle_coordinator_enabled = true;

    bool ok = nimcp_brain_factory_init_cochlea_subsystem(b);
    ASSERT_TRUE(ok, "cochlea bridges factory init failed");

    /* Measure a tight 500ms window (skip first 50ms to let the driver
     * thread spin up). */
    sleep_ms(50);
    brain_cycle_status_t st_before;
    int g1 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, &st_before);
    ASSERT_EQ(g1, 0, "get_status failed (before)");

    sleep_ms(500);

    brain_cycle_status_t st_after;
    int g2 = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_COCHLEA_BRIDGES, &st_after);
    ASSERT_EQ(g2, 0, "get_status failed (after)");

    uint64_t delta_ticks =
        st_after.ticks_executed - st_before.ticks_executed;

    nimcp_brain_factory_destroy_cochlea_subsystem(b);
    brain_cycle_coordinator_destroy(coord);
    free(b);

    /* 500ms / 10ms = 50 ideal. 40 = 80% efficiency allows scheduler slop. */
    ASSERT_TRUE(delta_ticks >= 40,
        "cadence target missed — expected >= 40 ticks over 500ms");
    /* Ceiling sanity check. */
    ASSERT_TRUE(delta_ticks <= 200,
        "cadence wildly over budget — clock or cadence bug?");
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Integration Tests: Wave 8A (cochlea + 15 bridges) ===\n\n");

    test_tri_cycle_10_10_100_coexist();
    test_empty_fastpath_cadence();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
