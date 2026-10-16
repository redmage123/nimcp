/**
 * @file test_wave5_regression.c
 * @brief Regression tests for Wave 5 post_forward_context_t shape change.
 *
 * WHAT: Guards against two classes of regression introduced by the Wave-5
 *       patch:
 *         (1) The 8 pre-existing stage `*_done` flags must still be set by
 *             their stage wrappers when submit_post_forward is called.
 *             Bumping the task-args array from 8 to 9 slots and submitting
 *             a new task at args[8] must not have displaced any pre-existing
 *             submit call (which would silently stop running that stage's
 *             task and leave its flag false).
 *         (2) `sizeof(post_forward_context_t)` must have *grown* relative
 *             to the pre-Wave-5 layout. Can't pin to an exact byte count
 *             due to padding, but it must exceed the old minimum of
 *             (8 bools + 1 void*). The new minimum is (9 bools + 1 void*),
 *             which with natural padding on every platform we support is
 *             strictly larger.
 *
 * WHY:  These are exactly the silent-failure modes the WHAT-WHY-HOW flags.
 *       A wrong index on the new submit would have the pool run 9 tasks on
 *       8 arg slots and one slot not-dispatched. A swapped submit at args[8]
 *       would overwrite a peer's data.
 *
 * HOW:  Real thread pool, bare-bones brain struct. No physics engine
 *       required here — the 8 peer stages null-guard and still set flags
 *       when their subsystem pointers are NULL, which is enough to detect
 *       a missing submit. Size assertion is a static expression against a
 *       pre-Wave-5 reference size computed from the known old layout.
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

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-72s", name); fflush(stdout); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; return; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)
#define ASSERT_NOT_NULL(p, msg) do { if ((p) == NULL) { FAIL(msg); } } while(0)

/* Pre-Wave-5 layout had 8 bools and 1 void*. Compute its size MINIMUM
 * assuming optimal packing (8 bools = 8 bytes, then void* aligned).
 * The new layout has 9 bools — must be at least one byte larger. */
#define PRE_WAVE5_MIN_SIZE (8 + sizeof(void*))

/* ------------------------------------------------------------------------- */

static struct brain_struct* alloc_bare_brain(void) {
    return (struct brain_struct*)calloc(1, sizeof(struct brain_struct));
}

/* ------------------------------------------------------------------------- */

static void test_all_eight_preexisting_flags_still_set(void) {
    TEST("all 8 pre-Wave-5 done flags still get set after submit+wait");
    struct brain_struct* b = alloc_bare_brain();
    ASSERT_NOT_NULL(b, "brain calloc failed");

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

    /* Every one of the 8 legacy done flags must be true. If the new submit
     * at args[8] displaced a peer's submit call, one of these would be
     * false. */
    ASSERT_TRUE(ctx.engram_consol_done,    "engram_consol_done not set");
    ASSERT_TRUE(ctx.systems_consol_done,   "systems_consol_done not set");
    ASSERT_TRUE(ctx.wm_transfer_done,      "wm_transfer_done not set");
    ASSERT_TRUE(ctx.semantic_done,         "semantic_done not set");
    ASSERT_TRUE(ctx.glial_done,            "glial_done not set");
    ASSERT_TRUE(ctx.tom_done,              "tom_done not set");
    ASSERT_TRUE(ctx.shannon_done,          "shannon_done not set");
    ASSERT_TRUE(ctx.quantum_shannon_done,  "quantum_shannon_done not set");

    nimcp_pool_destroy(pool);
    free(b);
    PASS();
}

static void test_context_size_grew(void) {
    TEST("sizeof(post_forward_context_t) exceeds pre-Wave-5 minimum");
    /* Pre-Wave-5: 8 bools + void*. Post-Wave-5: 9 bools + void*.
     * On every platform we support, adding a bool after 8 existing bools
     * either occupies a previously-used padding byte (new size ==
     * old size, padding-only change) or causes the struct to grow. The
     * pre-Wave-5 minimum (tightest packing) was 8 + sizeof(void*). The
     * new struct MUST be strictly larger than that minimum. */
    size_t sz = sizeof(post_forward_context_t);
    if (sz <= PRE_WAVE5_MIN_SIZE) {
        printf("[FAIL] sizeof got %zu, expected > %zu\n",
               sz, (size_t)PRE_WAVE5_MIN_SIZE);
        tests_failed++;
        return;
    }
    PASS();
}

static void test_context_has_distinct_physics_done_field(void) {
    TEST("physics_done is addressable and distinct from every other flag");
    post_forward_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* Flip physics_done; no other bool should change. */
    ctx.physics_done = true;
    ASSERT_TRUE(ctx.physics_done, "write to physics_done didn't stick");
    ASSERT_TRUE(!ctx.engram_consol_done, "physics_done write leaked into engram_consol");
    ASSERT_TRUE(!ctx.systems_consol_done, "physics_done write leaked into systems_consol");
    ASSERT_TRUE(!ctx.wm_transfer_done, "physics_done write leaked into wm_transfer");
    ASSERT_TRUE(!ctx.semantic_done, "physics_done write leaked into semantic");
    ASSERT_TRUE(!ctx.glial_done, "physics_done write leaked into glial");
    ASSERT_TRUE(!ctx.tom_done, "physics_done write leaked into tom");
    ASSERT_TRUE(!ctx.shannon_done, "physics_done write leaked into shannon");
    ASSERT_TRUE(!ctx.quantum_shannon_done, "physics_done write leaked into quantum_shannon");
    PASS();
}

/* ------------------------------------------------------------------------- */

int main(void) {
    printf("\n=== Regression Tests: Wave 5 post_forward_context_t shape ===\n\n");

    test_context_size_grew();
    test_context_has_distinct_physics_done_field();
    test_all_eight_preexisting_flags_still_set();

    printf("\n=== Results: %d passed, %d failed ===\n\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
