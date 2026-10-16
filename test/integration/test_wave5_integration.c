/**
 * @file test_wave5_integration.c
 * @brief Integration tests for Wave 5 physics stage in the post-forward
 *        parallel pipeline.
 *
 * WHAT: Exercises the full submit-then-wait cycle of
 *       brain_decide_submit_post_forward with the new Wave-5 physics stage
 *       alongside at least one other live stage (theory-of-mind, which
 *       null-guards but still exercises the shared task-args path).
 *
 * WHY:  The unit suite proves the physics stage in isolation. This suite
 *       proves it coexists with its 8 siblings: the task-args array bump
 *       from 8 to 9 entries didn't shift pointers for any pre-existing
 *       stage, and both the physics engine AND peer subsystems can run in
 *       the same submit cycle without interference.
 *
 * HOW:  calloc'd brain struct + real thread pool + real physics engine.
 *       TOM is null-guarded on brain->theory_of_mind — we leave it NULL so
 *       stage_tom_task runs its null branch (proves coexistence without
 *       needing a live ToM module). The physics engine gets a ground plane
 *       and a single free-falling sphere so stepping is non-trivial and
 *       its effect is measurable.
 */

#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdbool.h>
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
#define ASSERT_EQ(a, b, msg) do { if ((long long)(a) != (long long)(b)) { \
    printf("[FAIL] %s (got %lld, expected %lld)\n", msg, (long long)(a), (long long)(b)); \
    tests_failed++; return; } } while(0)
#define ASSERT_NOT_NULL(p, msg) do { if ((p) == NULL) { FAIL(msg); } } while(0)

/* ------------------------------------------------------------------------- */

static struct brain_struct* alloc_bare_brain(void) {
    return (struct brain_struct*)calloc(1, sizeof(struct brain_struct));
}

/* Add a simple free-falling sphere 5m above the ground. Returns its id. */
static uint32_t add_falling_sphere(intuitive_physics_engine_t* engine) {
    ip_object_t sphere = {
        .position      = { 0.0f, 5.0f, 0.0f },
        .velocity      = { 0, 0, 0 },
        .orientation   = { 1.0f, 0, 0, 0 },
        .mass          = 1.0f,
        .restitution   = 0.3f,
        .friction      = 0.5f,
        .shape         = {
            .type = IP_SHAPE_SPHERE,
            .sphere = { .radius = 0.5f },
        },
        .is_static     = false,
        .visible       = true,
        .active        = true,
    };
    return intuitive_physics_add_object(engine, &sphere);
}

/* ------------------------------------------------------------------------- */

static void test_full_submit_advances_engine_state(void) {
    TEST("submit_post_forward with real engine advances step_count + scene time");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "brain calloc failed");

    intuitive_physics_engine_t* engine = intuitive_physics_create(NULL);
    ASSERT_NOT_NULL(engine, "intuitive_physics_create failed");
    (void)intuitive_physics_add_ground(engine);
    uint32_t sphere_id = add_falling_sphere(engine);
    ASSERT_TRUE(sphere_id != UINT32_MAX, "add sphere failed");

    b->intuitive_physics = engine;
    b->intuitive_physics_enabled = true;

    /* Capture "before" observations. */
    uint64_t steps_before = intuitive_physics_get_stats(engine).step_count;
    float    time_before  = engine->scene.time;
    float    y_before     = engine->scene.objects[sphere_id].position.y;

    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NOT_NULL(pool, "pool_create failed");

    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    bool ok = brain_decide_submit_post_forward(
        (brain_t)b, NULL, NULL, 0,
        (struct nimcp_thread_pool*)pool, &ctx);
    ASSERT_TRUE(ok, "brain_decide_submit_post_forward failed");

    (void)nimcp_pool_wait(pool);
    nimcp_free(ctx._internal_args);
    ctx._internal_args = NULL;

    /* Context flag set. */
    ASSERT_TRUE(ctx.physics_done, "physics_done not set after submit+wait");

    /* Engine actually advanced. */
    ip_stats_t s_after = intuitive_physics_get_stats(engine);
    ASSERT_EQ(s_after.step_count - steps_before, 1ULL,
        "step_count did not advance by exactly 1");
    ASSERT_TRUE(engine->scene.time > time_before,
        "scene.time did not advance");

    /* The sphere is in free fall with gravity — y should have decreased. */
    float y_after = engine->scene.objects[sphere_id].position.y;
    ASSERT_TRUE(y_after < y_before,
        "sphere y did not decrease — physics not actually stepping");

    nimcp_pool_destroy(pool);
    intuitive_physics_destroy(engine);
    free(b);
    PASS();
}

static void test_physics_coexists_with_peer_stage_all_flags_set(void) {
    TEST("physics + peer stages: all 9 done-flags set in one submit");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "brain calloc failed");

    intuitive_physics_engine_t* engine = intuitive_physics_create(NULL);
    ASSERT_NOT_NULL(engine, "intuitive_physics_create failed");
    (void)intuitive_physics_add_ground(engine);

    b->intuitive_physics = engine;
    b->intuitive_physics_enabled = true;
    /* All other subsystem pointers left NULL — their null-guarded stages
     * will run their NULL branches and still set their done flags. This
     * exercises the full 9-task dispatch path. */

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NOT_NULL(pool, "pool_create failed");

    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    bool ok = brain_decide_submit_post_forward(
        (brain_t)b, NULL, NULL, 0,
        (struct nimcp_thread_pool*)pool, &ctx);
    ASSERT_TRUE(ok, "submit_post_forward failed");

    (void)nimcp_pool_wait(pool);
    nimcp_free(ctx._internal_args);
    ctx._internal_args = NULL;

    /* All nine done flags must be set. */
    ASSERT_TRUE(ctx.engram_consol_done,    "engram_consol_done not set");
    ASSERT_TRUE(ctx.systems_consol_done,   "systems_consol_done not set");
    ASSERT_TRUE(ctx.wm_transfer_done,      "wm_transfer_done not set");
    ASSERT_TRUE(ctx.semantic_done,         "semantic_done not set");
    ASSERT_TRUE(ctx.glial_done,            "glial_done not set");
    ASSERT_TRUE(ctx.tom_done,              "tom_done not set");
    ASSERT_TRUE(ctx.shannon_done,          "shannon_done not set");
    ASSERT_TRUE(ctx.quantum_shannon_done,  "quantum_shannon_done not set");
    ASSERT_TRUE(ctx.physics_done,          "physics_done not set");

    /* Physics engine stepped exactly once. */
    ASSERT_EQ(intuitive_physics_get_stats(engine).step_count, 1ULL,
        "expected exactly 1 physics step for 1 submit");

    nimcp_pool_destroy(pool);
    intuitive_physics_destroy(engine);
    free(b);
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Integration Tests: Wave 5 physics stage in post-forward pipeline ===\n\n");

    test_full_submit_advances_engine_state();
    test_physics_coexists_with_peer_stage_all_flags_set();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
