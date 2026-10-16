/**
 * @file test_learning_workspace.cpp
 * @brief Unit tests for Phase 2: Learning Workspace Buffer Reuse
 *
 * WHAT: Tests that learning workspace is properly initialized and reused
 * WHY:  40-watt brain: eliminate malloc/free per learning call
 * HOW:  Train brain multiple times, verify no crashes and correct behavior
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>

extern "C" {
#include "nimcp.h"
}

// =============================================================================
// Test constants
// =============================================================================
static constexpr uint32_t BRAIN_NEURONS       = 100;
static constexpr uint32_t INPUT_SIZE          = 10;
static constexpr uint32_t OUTPUT_SIZE         = 100;
static constexpr uint32_t LABEL_BUF_SIZE      = 64;
static constexpr int      BUFFER_REUSE_EPOCHS = 20;
static constexpr int      INTERLEAVE_ITERS    = 10;
static constexpr int      BACKPROP_ITERS      = 5;
static constexpr float    LOSS_DRIFT_MARGIN   = 2.0f;

// =============================================================================
// Shared brain fixture
// =============================================================================

class LearningWorkspace : public ::testing::Test {
protected:
    static nimcp_brain_t brain;

    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("workspace_test",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }

    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};

nimcp_brain_t LearningWorkspace::brain = nullptr;

// =============================================================================
// Tests
// =============================================================================

TEST_F(LearningWorkspace, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

TEST_F(LearningWorkspace, SingleLearnCall) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float target[OUTPUT_SIZE] = {};
    target[0] = 1.0f;

    nimcp_training_result_t result = {};
    nimcp_status_t s = nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_TRUE(std::isfinite(result.loss));
}

TEST_F(LearningWorkspace, MultipleLearnCallsReuseBuffers) {
    if (!brain) GTEST_SKIP();

    for (int epoch = 0; epoch < BUFFER_REUSE_EPOCHS; epoch++) {
        float input[INPUT_SIZE];
        float target[OUTPUT_SIZE] = {};

        for (uint32_t i = 0; i < INPUT_SIZE; i++) {
            input[i] = (float)(epoch * INPUT_SIZE + i) / (float)(BUFFER_REUSE_EPOCHS * INPUT_SIZE);
        }
        target[epoch % OUTPUT_SIZE] = 1.0f;

        nimcp_training_result_t result = {};
        nimcp_status_t s = nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        EXPECT_EQ(s, NIMCP_OK);
        EXPECT_TRUE(std::isfinite(result.loss));
    }
}

TEST_F(LearningWorkspace, InterleavedLearnAndInference) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < INTERLEAVE_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++) input[j] = (float)rand() / RAND_MAX;

        if (i % 2 == 0) {
            float target[OUTPUT_SIZE] = {};
            target[i % OUTPUT_SIZE] = 1.0f;
            nimcp_training_result_t result = {};
            nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
            EXPECT_TRUE(std::isfinite(result.loss));
        } else {
            char label[LABEL_BUF_SIZE] = {};
            float conf = 0.0f;
            nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
            EXPECT_TRUE(std::isfinite(conf));
        }
    }
}

TEST_F(LearningWorkspace, BackpropDeltaPersistence) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                        0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float target[OUTPUT_SIZE] = {};
    target[50] = 1.0f;

    float losses[BACKPROP_ITERS];
    for (int i = 0; i < BACKPROP_ITERS; i++) {
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        losses[i] = result.loss;
        EXPECT_TRUE(std::isfinite(losses[i]));
    }

    EXPECT_LT(losses[BACKPROP_ITERS - 1], losses[0] + LOSS_DRIFT_MARGIN);
}
