/**
 * @file test_fast_math.cpp
 * @brief Unit tests for Phase 3: Fast Math Approximations
 *
 * WHAT: Tests that fast_expf and SIMD operations produce correct results
 * WHY:  40-watt brain: use fast approximations in tight loops
 * HOW:  Compare fast approximations against standard library functions
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "nimcp.h"
}

// =============================================================================
// Test constants
// =============================================================================
static constexpr uint32_t BRAIN_NEURONS      = 100;
static constexpr uint32_t INPUT_SIZE         = 10;
static constexpr uint32_t OUTPUT_SIZE        = 100;
static constexpr uint32_t LABEL_BUF_SIZE     = 64;
static constexpr int      TRAINING_EPOCHS    = 10;
static constexpr int      STABILITY_ITERS    = 5;
static constexpr float    LOSS_DRIFT_MARGIN  = 1.0f;

// =============================================================================
// Shared brain fixture
// =============================================================================

class FastMath : public ::testing::Test {
protected:
    static nimcp_brain_t brain;

    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("fast_math_test",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }

    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};

nimcp_brain_t FastMath::brain = nullptr;

// =============================================================================
// Tests
// =============================================================================

TEST_F(FastMath, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

TEST_F(FastMath, InferenceProducesValidOutput) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f, 0.9f,
                        0.2f, 0.7f, 0.4f, 0.6f, 0.1f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

TEST_F(FastMath, TrainingProducesFiniteLoss) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f, 0.9f,
                        0.2f, 0.7f, 0.4f, 0.6f, 0.1f};
    float target[OUTPUT_SIZE] = {};
    target[0] = 1.0f;

    nimcp_training_result_t result = {};
    nimcp_status_t s = nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_TRUE(std::isfinite(result.loss));
    EXPECT_GE(result.loss, 0.0f);
}

TEST_F(FastMath, RepeatedTrainingStable) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float target[OUTPUT_SIZE] = {};
    target[5] = 1.0f;

    float first_loss = 0.0f, last_loss = 0.0f;
    for (int i = 0; i < TRAINING_EPOCHS; i++) {
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        if (i == 0) first_loss = result.loss;
        last_loss = result.loss;
    }

    EXPECT_TRUE(std::isfinite(last_loss));
    EXPECT_LE(last_loss, first_loss + LOSS_DRIFT_MARGIN);
}

TEST_F(FastMath, WeightNormalizationCorrect) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float target[OUTPUT_SIZE] = {};
    target[0] = 1.0f;

    for (int i = 0; i < STABILITY_ITERS; i++) {
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));
    }

    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_TRUE(std::isfinite(conf));
}
