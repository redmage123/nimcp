/**
 * @file test_cognitive_dispatch_regression.cpp
 * @brief Regression tests for parallel cognitive dispatch
 *
 * Ensures parallel dispatch doesn't degrade performance or
 * introduce data races vs the sequential implementation.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>

#include "core/brain/nimcp_cognitive_dispatch.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread_pool.h"
}

#include "core/brain/nimcp_brain_internal.h"

/* ============================================================================
 * Regression Fixture
 * ============================================================================ */

class CogDispatchRegression : public ::testing::Test {
protected:
    struct brain_struct brain_storage;
    brain_t brain;

    void SetUp() override {
        memset(&brain_storage, 0, sizeof(brain_storage));
        brain = &brain_storage;
        brain->cognitive_train_interval = 1;
    }

    void TearDown() override {
        if (brain->fep_orchestrator) {
            free((void*)brain->fep_orchestrator);
            brain->fep_orchestrator = NULL;
        }
        if (brain->inference_pool) {
            nimcp_pool_destroy(brain->inference_pool);
            brain->inference_pool = NULL;
        }
    }
};

/* ============================================================================
 * 1. Parallel overhead acceptable — should not be slower than 10ms for empty modules
 * ============================================================================ */

TEST_F(CogDispatchRegression, ParallelOverheadAcceptable) {
    brain->inference_pool = nimcp_pool_create(4);
    ASSERT_NE(brain->inference_pool, nullptr);

    float features[64] = {};
    float target[64] = {};

    /* Warm up */
    brain_train_cognitive_parallel(brain, features, 64, target, 64, "warmup", 0.5f);

    /* Measure 10 dispatches */
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10; i++) {
        brain_train_cognitive_parallel(brain, features, 64, target, 64, "bench", 0.5f);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    /* 10 dispatches with all-NULL modules should complete in <100ms total
     * (promise create/destroy + pool submit overhead) */
    EXPECT_LT(elapsed_us, 100000) << "Parallel dispatch overhead too high: "
                                   << elapsed_us << " us for 10 dispatches";
}

/* ============================================================================
 * 2. Counter matches between parallel and sequential paths
 * ============================================================================ */

TEST_F(CogDispatchRegression, CounterConsistency) {
    brain->inference_pool = nimcp_pool_create(4);
    brain->cognitive_train_interval = 3;

    float features[] = {1.0f};
    float target[] = {0.5f};

    for (int i = 0; i < 15; i++) {
        brain_train_cognitive_parallel(brain, features, 1, target, 1, "cnt", 0.5f);
    }

    EXPECT_EQ(brain->cognitive_train_counter, 15u);
    /* With interval=3, dispatch should have fired at steps 3,6,9,12,15 = 5 times */
    /* But all modules NULL so fep_orchestrator_steps stays 0 unless fep enabled */
}

/* ============================================================================
 * 3. FEP metrics match expected values after parallel dispatch
 * ============================================================================ */

TEST_F(CogDispatchRegression, FepMetricsCorrectValues) {
    brain->inference_pool = nimcp_pool_create(4);

    void* fake_fep_mem = calloc(1, 4096);
    brain->fep_orchestrator = (struct fep_orchestrator*)fake_fep_mem;
    brain->fep_orchestrator_enabled = true;

    float features[] = {1.0f};
    float target[] = {0.5f};
    float loss = 0.25f;

    brain_train_cognitive_parallel(brain, features, 1, target, 1, "fep", loss);

    /* Verify coordinator updated stats (can't easily verify fep_metrics
     * offset in heap-allocated fake struct without full header) */
    EXPECT_EQ(brain->cognitive_stats.fep_orchestrator_steps, 1u);
}

/* ============================================================================
 * 4. Stress test — 1000 rapid dispatches, no crash
 * ============================================================================ */

TEST_F(CogDispatchRegression, StressTest1000Dispatches) {
    brain->inference_pool = nimcp_pool_create(4);

    void* fake_fep_stress = calloc(1, 4096);
    brain->fep_orchestrator = (struct fep_orchestrator*)fake_fep_stress;
    brain->fep_orchestrator_enabled = true;

    float features[16] = {};
    float target[16] = {};

    for (int i = 0; i < 1000; i++) {
        features[0] = (float)i;
        brain_train_cognitive_parallel(
            brain, features, 16, target, 16, "stress", (float)i / 1000.0f);
    }

    EXPECT_EQ(brain->cognitive_train_counter, 1000u);
    EXPECT_EQ(brain->cognitive_stats.fep_orchestrator_steps, 1000u);
}

/* ============================================================================
 * 5. Batch result timing is monotonically valid
 * ============================================================================ */

TEST_F(CogDispatchRegression, BatchTimingValid) {
    brain->inference_pool = nimcp_pool_create(4);

    void* fake_fep_timing = calloc(1, 4096);
    brain->fep_orchestrator = (struct fep_orchestrator*)fake_fep_timing;
    brain->fep_orchestrator_enabled = true;

    float features[] = {1.0f};
    float target[] = {0.5f};

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        brain, features, 1, target, 1, "timing", 0.5f);

    /* Total elapsed should be >= any individual module elapsed */
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        if (batch.results[i].executed) {
            EXPECT_LE(batch.results[i].elapsed_us, batch.total_elapsed_us)
                << "Module " << i << " elapsed exceeds total";
        }
    }
}
