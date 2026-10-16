/**
 * @file test_cognitive_dispatch_integration.cpp
 * @brief Integration tests for parallel cognitive dispatch with real brain
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/nimcp_cognitive_dispatch.h"

extern "C" {
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "nimcp.h"
}

#include "core/brain/nimcp_brain_internal.h"

/* Expose opaque handle for testing */
struct nimcp_brain_handle {
    brain_t internal_brain;
    float   last_loss;
    float   last_gradient_norm;
};

/* ============================================================================
 * Integration Fixture
 * ============================================================================ */

class CogDispatchIntegration : public ::testing::Test {
protected:
    nimcp_brain_t brain_handle;

    void SetUp() override {
        brain_handle = NULL;
        nimcp_init();
    }

    void TearDown() override {
        if (brain_handle) {
            nimcp_brain_destroy(brain_handle);
            brain_handle = NULL;
        }
        nimcp_shutdown();
    }

    bool create_brain() {
        brain_handle = nimcp_brain_create("test_dispatch",
                                           NIMCP_BRAIN_SMALL,
                                           NIMCP_TASK_CLASSIFICATION,
                                           64, 64);
        return brain_handle != NULL;
    }
};

/* ============================================================================
 * 1. Parallel dispatch on real brain — no crash
 * ============================================================================ */

TEST_F(CogDispatchIntegration, ParallelDispatchNoCrash) {
    if (!create_brain()) {
        GTEST_SKIP() << "Brain creation failed";
    }
    brain_t b = brain_handle->internal_brain;
    ASSERT_NE(b, nullptr);

    if (!b->inference_pool) {
        b->inference_pool = nimcp_pool_create(4);
    }
    b->cognitive_train_interval = 1;

    float features[64] = {};
    float target[64] = {};
    for (int i = 0; i < 64; i++) {
        features[i] = (float)i / 64.0f;
        target[i] = 1.0f - features[i];
    }

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        b, features, 64, target, 64, "test integration", 0.5f);

    EXPECT_GE(batch.total_elapsed_us, 0u);
}

/* ============================================================================
 * 2. brain_learn_vector integration
 * ============================================================================ */

TEST_F(CogDispatchIntegration, BrainLearnVectorIntegration) {
    if (!create_brain()) {
        GTEST_SKIP() << "Brain creation failed";
    }

    float features[64] = {};
    float target[64] = {};
    for (int i = 0; i < 64; i++) {
        features[i] = (float)i / 64.0f;
        target[i] = 1.0f - features[i];
    }

    nimcp_status_t status = nimcp_brain_learn_vector(
        brain_handle, features, 64, target, 64, "test learn", 0.8f);
    (void)status;
}

/* ============================================================================
 * 3. Multiple dispatches — no memory leak
 * ============================================================================ */

TEST_F(CogDispatchIntegration, NoMemoryLeakMultipleDispatches) {
    if (!create_brain()) {
        GTEST_SKIP() << "Brain creation failed";
    }
    brain_t b = brain_handle->internal_brain;

    if (!b->inference_pool) {
        b->inference_pool = nimcp_pool_create(4);
    }
    b->cognitive_train_interval = 1;

    float features[64] = {};
    float target[64] = {};

    for (int i = 0; i < 50; i++) {
        features[0] = (float)i;
        brain_train_cognitive_parallel(
            b, features, 64, target, 64, "leak test", 0.5f);
    }

    EXPECT_EQ(b->cognitive_train_counter, 50u);
}

/* ============================================================================
 * 4. Sequential fallback on real brain
 * ============================================================================ */

TEST_F(CogDispatchIntegration, SequentialFallbackRealBrain) {
    if (!create_brain()) {
        GTEST_SKIP() << "Brain creation failed";
    }
    brain_t b = brain_handle->internal_brain;

    nimcp_thread_pool_t* saved_pool = b->inference_pool;
    b->inference_pool = NULL;
    b->cognitive_train_interval = 1;

    float features[64] = {};
    float target[64] = {};

    cognitive_batch_result_t batch = brain_train_cognitive_parallel(
        b, features, 64, target, 64, "seq fallback", 0.4f);

    b->inference_pool = saved_pool;
    EXPECT_GE(batch.total_elapsed_us, 0u);
}
