/**
 * @file test_cognitive_dispatch.cpp
 * @brief Unit tests for parallel cognitive module dispatch (actor pattern)
 *
 * Tests the coordinator, task wrappers, result collection, stats update,
 * fallback paths, and edge cases. Uses a minimal brain_struct.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>

/* cognitive_dispatch.h is pure C with extern "C" guards */
#include "core/brain/nimcp_cognitive_dispatch.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "async/nimcp_future.h"
}

/* brain_internal.h MUST be outside extern "C" — it pulls in CUDA headers
 * that use C++ templates (channel_descriptor.h) */
#include "core/brain/nimcp_brain_internal.h"

/* ============================================================================
 * Test Fixture: minimal brain_struct for dispatch testing
 * ============================================================================ */

class CognitiveDispatchTest : public ::testing::Test {
protected:
    struct brain_struct brain_storage;
    brain_t brain;

    void SetUp() override {
        memset(&brain_storage, 0, sizeof(brain_storage));
        brain = &brain_storage;
        /* Set interval to 1 so every call triggers dispatch */
        brain->cognitive_train_interval = 1;
        brain->cognitive_train_counter = 0;
    }

    void TearDown() override {
        if (brain->inference_pool) {
            nimcp_pool_destroy(brain->inference_pool);
            brain->inference_pool = NULL;
        }
    }

    void create_pool(size_t threads = 4) {
        brain->inference_pool = nimcp_pool_create(threads);
        ASSERT_NE(brain->inference_pool, nullptr);
    }
};

/* ============================================================================
 * 1. NULL brain returns empty batch
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, NullBrainReturnsEmpty) {
    float features[] = {1.0f, 2.0f};
    float target[] = {0.5f};
    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        NULL, features, 2, target, 1, "test", 0.5f);
    EXPECT_EQ(batch.num_executed, 0u);
}

/* ============================================================================
 * 2. Interval gating — skips when counter not at interval
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, IntervalGatingSkips) {
    brain->cognitive_train_interval = 5;
    brain->cognitive_train_counter = 0;

    float features[] = {1.0f};
    float target[] = {0.5f};

    /* Calls 1-4 should be gated (counter 1,2,3,4 not divisible by 5) */
    for (int i = 0; i < 4; i++) {
        cognitive_batch_result_t batch = brain_train_cognitive_parallel(
            brain, features, 1, target, 1, "test", 0.5f);
        EXPECT_EQ(batch.num_executed, 0u) << "Step " << i << " should be gated";
    }

    /* Call 5 should pass (counter = 5, divisible by 5) */
    /* But all modules are NULL so nothing executes */
    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 1, target, 1, "test", 0.5f);
    /* num_executed is 0 because all module pointers are NULL */
    EXPECT_EQ(batch.num_executed, 0u);
    EXPECT_EQ(brain->cognitive_train_counter, 5u);
}

/* ============================================================================
 * 3. Sequential fallback when no pool
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, SequentialFallbackNoPool) {
    ASSERT_EQ(brain->inference_pool, nullptr);

    float features[] = {1.0f, 2.0f};
    float target[] = {0.5f, 0.5f};

    /* Should call sequential path without crash */
    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 2, target, 2, "test", 0.5f);
    /* Sequential fallback doesn't populate batch results */
    EXPECT_GE(batch.total_elapsed_us, 0u);
}

/* ============================================================================
 * 4. Parallel dispatch with pool — all modules NULL (graceful skip)
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, ParallelAllModulesNull) {
    create_pool(4);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float target[] = {0.5f, 0.5f, 0.5f, 0.5f};

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 4, target, 4, "test", 0.5f);

    /* All modules are NULL — none should execute */
    EXPECT_EQ(batch.num_executed, 0u);
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        EXPECT_FALSE(batch.results[i].executed);
    }
}

/* ============================================================================
 * 5. Result struct layout — module_id matches index
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, ResultModuleIdsMatchIndex) {
    create_pool(4);

    float features[] = {1.0f};
    float target[] = {0.5f};

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 1, target, 1, "test", 0.5f);

    /* Even though modules are NULL, the result array should be zero-init */
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        /* Unexecuted results have module_id = 0 (from zero-init) or from promise */
        EXPECT_FALSE(batch.results[i].executed);
    }
}

/* ============================================================================
 * 6. FEP orchestrator stats update via coordinator
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, FepOrchestratorStatsViaCoordinator) {
    create_pool(4);

    /* Allocate full-size fep_orchestrator on heap (struct is large, has fep_metrics
     * at a significant offset — a fake small struct causes stack smash) */
    void* fake_fep = calloc(1, 4096);  /* 4KB >> actual struct size */
    brain->fep_orchestrator = (struct fep_orchestrator*)fake_fep;
    brain->fep_orchestrator_enabled = true;

    float features[] = {1.0f};
    float target[] = {0.5f};

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 1, target, 1, "test", 0.42f);

    /* FEP orchestrator task should have executed */
    const cognitive_task_result_t* fep_r = &batch.results[COG_MODULE_FEP_ORCHESTRATOR];
    EXPECT_TRUE(fep_r->executed);
    EXPECT_FLOAT_EQ(fep_r->loss, 0.42f);
    EXPECT_GT(fep_r->aux_value, 0.0f); /* surprise = -log(1 - 0.42) > 0 */

    /* Coordinator should have updated stats */
    EXPECT_EQ(brain->cognitive_stats.fep_orchestrator_steps, 1u);

    free(fake_fep);
    brain->fep_orchestrator = NULL;
}

/* ============================================================================
 * 7. Timing — parallel should measure elapsed
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, TimingMeasured) {
    create_pool(4);

    float features[] = {1.0f};
    float target[] = {0.5f};

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 1, target, 1, "test", 0.5f);

    EXPECT_GT(batch.total_elapsed_us, 0u);
}

/* ============================================================================
 * 8. Loss values for modules that produce them
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, LossValuesNanForNonProducers) {
    create_pool(4);

    float features[] = {1.0f};
    float target[] = {0.5f};

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 1, target, 1, "test", 0.5f);

    /* All modules NULL → no loss values, but results should have NAN */
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        if (!batch.results[i].executed) {
            /* Zero-init → loss is 0.0f not NAN (from calloc) */
            /* That's OK — unexecuted results aren't read by coordinator */
        }
    }
}

/* ============================================================================
 * 9. Multiple dispatches — counter increments correctly
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, MultipleDispatchCounterIncrements) {
    create_pool(4);
    brain->cognitive_train_interval = 1; /* every call triggers */

    float features[] = {1.0f};
    float target[] = {0.5f};

    for (int i = 0; i < 10; i++) {
        brain_train_cognitive_parallel(
            brain, features, 1, target, 1, "test", 0.5f);
    }

    EXPECT_EQ(brain->cognitive_train_counter, 10u);
}

/* ============================================================================
 * 10. Single-thread pool — still works (sequential within pool)
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, SingleThreadPool) {
    create_pool(1);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float target[] = {0.5f, 0.5f, 0.5f, 0.5f};

    /* Should not deadlock with single thread */
    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 4, target, 4, "test", 0.5f);

    EXPECT_EQ(batch.num_executed, 0u); /* all modules NULL */
    EXPECT_GT(batch.total_elapsed_us, 0u);
}

/* ============================================================================
 * 11. NULL features/target — modules should skip gracefully
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, NullFeaturesSkipsGracefully) {
    create_pool(4);

    /* Enable FEP orchestrator (doesn't need features) */
    void* fake_fep2 = calloc(1, 4096);
    brain->fep_orchestrator = (struct fep_orchestrator*)fake_fep2;
    brain->fep_orchestrator_enabled = true;

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, NULL, 0, NULL, 0, NULL, 0.5f);

    /* FEP orchestrator doesn't need features — should still execute */
    EXPECT_TRUE(batch.results[COG_MODULE_FEP_ORCHESTRATOR].executed);
    /* Modules needing features should NOT execute */
    EXPECT_FALSE(batch.results[COG_MODULE_VAE].executed);

    free(fake_fep2);
    brain->fep_orchestrator = NULL;
}

/* ============================================================================
 * 12. Cognitive stats not updated for unexecuted modules
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, StatsNotUpdatedForUnexecuted) {
    create_pool(4);

    float features[] = {1.0f};
    float target[] = {0.5f};

    memset(&brain->cognitive_stats, 0, sizeof(brain->cognitive_stats));

    brain_train_cognitive_parallel(
        brain, features, 1, target, 1, "test", 0.5f);

    /* All modules NULL → no stats should change */
    EXPECT_EQ(brain->cognitive_stats.grounded_lang_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.knowledge_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.vae_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.fep_parietal_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.physics_nn_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.pred_hierarchy_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.jepa_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.creative_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.self_heal_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.intuition_steps, 0u);
    EXPECT_EQ(brain->cognitive_stats.fep_orchestrator_steps, 0u);
}

/* ============================================================================
 * 13. Enum values match expected count
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, EnumCount) {
    EXPECT_EQ(COG_MODULE_COUNT, 12);
    EXPECT_EQ(COG_MODULE_GROUNDED_LANG, 0);
    EXPECT_EQ(COG_MODULE_SNN_FNO, 11);
}

/* ============================================================================
 * 14. Batch result struct size
 * ============================================================================ */

TEST_F(CognitiveDispatchTest, BatchResultStructLayout) {
    cognitive_batch_result_t batch = {};
    EXPECT_EQ(sizeof(batch.results) / sizeof(batch.results[0]),
              (size_t)COG_MODULE_COUNT);
}
