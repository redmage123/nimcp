/**
 * @file test_wave5_physics_stage.c
 * @brief Unit tests for Wave 5 intuitive-physics post-forward stage.
 *
 * WHAT: Validates the stage_physics_task added to post_forward stage dispatch
 *       in src/core/brain/nimcp_brain_parallel_stages.c (Wave 5). One
 *       `intuitive_physics_step(engine, 0.016f)` should fire per
 *       brain_decide_submit_post_forward call when the engine exists AND the
 *       `intuitive_physics_enabled` flag is true — and nothing should fire
 *       otherwise. In every case `ctx.physics_done` must be set.
 *
 * WHY:  Wave 5 was a HIGH-statue fix: intuitive_physics was allocated at
 *       world-model init but only ever stepped from an isolated parietal
 *       path. The new stage wires it into the hot post-forward pipeline as
 *       state-advancement (outputs are not consumed yet). The unit surface we
 *       can drive without booting a full brain is: submit the post-forward
 *       tasks on a calloc'd brain struct with only the physics fields set,
 *       wait for the pool, inspect ctx + stats.
 *
 * HOW:  calloc(sizeof(struct brain_struct)) gives us a zero-initialized brain
 *       — every pointer the other 8 stages null-guard on is NULL, so they
 *       no-op cleanly. Real nimcp_thread_pool, real intuitive_physics engine
 *       (add_ground() makes it non-trivial but still cheap). We read
 *       engine->stats.step_count to count physics steps, and ctx flags to
 *       confirm all stages completed.
 *
 * CAVEATS: We calloc sizeof(struct brain_struct) directly, same pattern as
 *       test_wave4_predictive_immune_init. NEVER do this outside test code.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define ASSERT_FALSE(cond, msg) do { if ((cond)) { FAIL(msg); } } while(0)
#define ASSERT_EQ(a, b, msg) do { if ((long long)(a) != (long long)(b)) { \
    printf("[FAIL] %s (got %lld, expected %lld)\n", msg, (long long)(a), (long long)(b)); \
    tests_failed++; return; } } while(0)
#define ASSERT_NOT_NULL(p, msg) do { if ((p) == NULL) { FAIL(msg); } } while(0)

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static struct brain_struct* alloc_bare_brain(void) {
    return (struct brain_struct*)calloc(1, sizeof(struct brain_struct));
}

static void submit_and_wait(struct brain_struct* b,
                            nimcp_thread_pool_t* pool,
                            post_forward_context_t* ctx) {
    /* Stateless call site — matches how brain_decide drives the pipeline. */
    bool ok = brain_decide_submit_post_forward(
        (brain_t)b, NULL, NULL, 0,
        (struct nimcp_thread_pool*)pool, ctx);
    if (!ok) {
        fprintf(stderr, "brain_decide_submit_post_forward returned false\n");
        abort();
    }
    (void)nimcp_pool_wait(pool);
    if (ctx->_internal_args) {
        nimcp_free(ctx->_internal_args);
        ctx->_internal_args = NULL;
    }
}

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

static void test_ctx_zero_init_physics_done_is_false(void) {
    TEST("zero-init post_forward_context_t -> physics_done == false");
    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ASSERT_FALSE(ctx.physics_done,
        "zero-init must leave physics_done false (precondition for stage flag)");
    ASSERT_FALSE(ctx.engram_consol_done, "engram_consol_done must start false");
    ASSERT_FALSE(ctx.tom_done,          "tom_done must start false");
    ASSERT_TRUE(ctx._internal_args == NULL,
        "zero-init must leave _internal_args NULL");
    PASS();
}

static void test_physics_null_engine_stage_runs_no_crash(void) {
    TEST("brain->intuitive_physics == NULL: stage runs, physics_done set, no crash");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "brain calloc failed");

    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NOT_NULL(pool, "pool_create failed");

    /* Deliberately leave both physics fields zero (NULL + false). */
    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    submit_and_wait(b, pool, &ctx);

    ASSERT_TRUE(ctx.physics_done,
        "physics_done must be set even when engine is NULL");

    nimcp_pool_destroy(pool);
    free(b);
    PASS();
}

static void test_physics_engine_present_but_disabled(void) {
    TEST("engine != NULL but intuitive_physics_enabled == false: no step, flag set");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "brain calloc failed");

    intuitive_physics_engine_t* engine = intuitive_physics_create(NULL);
    ASSERT_NOT_NULL(engine, "intuitive_physics_create failed");
    (void)intuitive_physics_add_ground(engine);

    b->intuitive_physics = engine;
    b->intuitive_physics_enabled = false;  /* explicit — enabled gate is OFF */

    uint64_t step_count_before = intuitive_physics_get_stats(engine).step_count;

    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NOT_NULL(pool, "pool_create failed");

    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    submit_and_wait(b, pool, &ctx);

    ASSERT_TRUE(ctx.physics_done,
        "physics_done must be set when disabled");
    uint64_t step_count_after = intuitive_physics_get_stats(engine).step_count;
    ASSERT_EQ(step_count_after, step_count_before,
        "step_count changed despite intuitive_physics_enabled=false");

    nimcp_pool_destroy(pool);
    intuitive_physics_destroy(engine);
    free(b);
    PASS();
}

static void test_physics_engine_enabled_steps_once(void) {
    TEST("engine present + enabled: exactly one step per submit/wait");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "brain calloc failed");

    intuitive_physics_engine_t* engine = intuitive_physics_create(NULL);
    ASSERT_NOT_NULL(engine, "intuitive_physics_create failed");
    (void)intuitive_physics_add_ground(engine);

    b->intuitive_physics = engine;
    b->intuitive_physics_enabled = true;

    uint64_t step_count_before = intuitive_physics_get_stats(engine).step_count;

    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NOT_NULL(pool, "pool_create failed");

    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    submit_and_wait(b, pool, &ctx);

    ASSERT_TRUE(ctx.physics_done, "physics_done must be set");
    uint64_t step_count_after = intuitive_physics_get_stats(engine).step_count;
    ASSERT_EQ(step_count_after - step_count_before, 1ULL,
        "exactly one physics step must happen per submit cycle");

    nimcp_pool_destroy(pool);
    intuitive_physics_destroy(engine);
    free(b);
    PASS();
}

static void test_physics_null_brain_returns_false(void) {
    TEST("NULL brain -> brain_decide_submit_post_forward returns false (no hang)");
    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NOT_NULL(pool, "pool_create failed");

    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    bool ok = brain_decide_submit_post_forward(
        NULL, NULL, NULL, 0,
        (struct nimcp_thread_pool*)pool, &ctx);
    ASSERT_FALSE(ok,
        "submit_post_forward(NULL brain) must return false");
    ASSERT_FALSE(ctx.physics_done,
        "physics_done must remain false on rejected submit");

    nimcp_pool_destroy(pool);
    PASS();
}

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Unit Tests: Wave 5 intuitive-physics post-forward stage ===\n\n");

    test_ctx_zero_init_physics_done_is_false();
    test_physics_null_engine_stage_runs_no_crash();
    test_physics_engine_present_but_disabled();
    test_physics_engine_enabled_steps_once();
    test_physics_null_brain_returns_false();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
