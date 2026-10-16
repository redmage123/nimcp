/**
 * @file test_wave5_e2e.c
 * @brief End-to-end soak test for Wave 5 physics stage — 20 submit cycles.
 *
 * WHAT: Repeats submit → pool_wait → clear-ctx 20 times with a real
 *       intuitive_physics engine attached to a calloc'd brain. Verifies:
 *         - No hang (real wall-clock budget).
 *         - physics_done flips to true on every iteration.
 *         - engine->stats.step_count increments by exactly 1 per submit.
 *         - The other 8 stage done-flags also flip every iteration.
 *
 * WHY:  A single-shot integration test would miss state-leak bugs —
 *       _internal_args not freed, done flags not reset, step counter
 *       drifting by >1. Looping N=20 iterations on real pthreads is the
 *       cheapest way to catch those under sustained load.
 *
 * HOW:  Plain C, zero GTest, links libnimcp only. Runtime budget is well
 *       under 1s (20 iterations of a ~1ms stage dispatch each).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain_parallel_stages.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/physics/nimcp_intuitive_physics.h"

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

#define N_ITERATIONS 20

/* ------------------------------------------------------------------------- */

static struct brain_struct* alloc_bare_brain(void) {
    return (struct brain_struct*)calloc(1, sizeof(struct brain_struct));
}

/* ------------------------------------------------------------------------- */

static void test_twenty_iteration_soak(void) {
    TEST("20 submit/wait/clear cycles: every iter flips 9 flags + advances physics by 1");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "brain calloc failed");

    intuitive_physics_engine_t* engine = intuitive_physics_create(NULL);
    ASSERT_NOT_NULL(engine, "intuitive_physics_create failed");
    (void)intuitive_physics_add_ground(engine);

    b->intuitive_physics = engine;
    b->intuitive_physics_enabled = true;

    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NOT_NULL(pool, "pool_create failed");

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    uint64_t expected_steps = 0;

    for (int iter = 0; iter < N_ITERATIONS; iter++) {
        post_forward_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));  /* reset each iteration */

        bool ok = brain_decide_submit_post_forward(
            (brain_t)b, NULL, NULL, 0,
            (struct nimcp_thread_pool*)pool, &ctx);
        ASSERT_TRUE(ok, "submit_post_forward failed mid-loop");

        (void)nimcp_pool_wait(pool);
        nimcp_free(ctx._internal_args);
        ctx._internal_args = NULL;

        expected_steps++;

        /* Per-iter invariants. */
        ASSERT_TRUE(ctx.physics_done,         "physics_done not set mid-loop");
        ASSERT_TRUE(ctx.engram_consol_done,   "engram_consol_done not set mid-loop");
        ASSERT_TRUE(ctx.systems_consol_done,  "systems_consol_done not set mid-loop");
        ASSERT_TRUE(ctx.wm_transfer_done,     "wm_transfer_done not set mid-loop");
        ASSERT_TRUE(ctx.semantic_done,        "semantic_done not set mid-loop");
        ASSERT_TRUE(ctx.glial_done,           "glial_done not set mid-loop");
        ASSERT_TRUE(ctx.tom_done,             "tom_done not set mid-loop");
        ASSERT_TRUE(ctx.shannon_done,         "shannon_done not set mid-loop");
        ASSERT_TRUE(ctx.quantum_shannon_done, "quantum_shannon_done not set mid-loop");

        uint64_t actual_steps = intuitive_physics_get_stats(engine).step_count;
        ASSERT_EQ(actual_steps, expected_steps,
            "step_count drifted from expected (exactly 1 per submit)");
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    long elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000 +
                      (t1.tv_nsec - t0.tv_nsec) / 1000000L;

    /* 20 iterations should complete well under 3s even on slow CI. */
    ASSERT_TRUE(elapsed_ms < 3000,
        "soak took > 3s — possible stall or priority issue");

    ASSERT_EQ(intuitive_physics_get_stats(engine).step_count,
              (uint64_t)N_ITERATIONS,
              "final step count != N_ITERATIONS");

    nimcp_pool_destroy(pool);
    intuitive_physics_destroy(engine);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== E2E Tests: Wave 5 physics stage soak ===\n\n");

    test_twenty_iteration_soak();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
