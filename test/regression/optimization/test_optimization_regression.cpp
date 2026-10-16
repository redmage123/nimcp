/**
 * @file test_optimization_regression.cpp
 * @brief Regression tests for 40-watt brain optimizations
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <cstdlib>

extern "C" {
#include "nimcp.h"
}

// =============================================================================
// Test constants
// =============================================================================
static constexpr uint32_t BRAIN_NEURONS         = 100;
static constexpr uint32_t INPUT_SIZE            = 10;
static constexpr uint32_t OUTPUT_SIZE           = 100;
static constexpr uint32_t LABEL_BUF_SIZE        = 64;
static constexpr int      RAPID_CYCLE_ITERS     = 100;
static constexpr int      CREATE_DESTROY_ITERS  = 3;
static constexpr float    CONSISTENCY_TOL       = 0.05f;
static constexpr uint32_t SMALL_NEURONS         = 50;
static constexpr uint32_t SMALL_INPUT_SIZE      = 5;
static constexpr uint32_t SMALL_OUTPUT_SIZE     = 50;

class OptimizationRegression : public ::testing::Test {
protected:
    static nimcp_brain_t brain;
    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("optim_regression",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }
    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};
nimcp_brain_t OptimizationRegression::brain = nullptr;

TEST_F(OptimizationRegression, ActiveCountNullBrain) {
    EXPECT_EQ(nimcp_brain_get_active_neuron_count(nullptr), 0u);
}

TEST_F(OptimizationRegression, SparsityRatioNullBrain) {
    EXPECT_FLOAT_EQ(nimcp_brain_get_sparsity_ratio(nullptr), 0.0f);
}

TEST_F(OptimizationRegression, ZeroInputInference) {
    if (!brain) GTEST_SKIP();
    float zeros[INPUT_SIZE] = {};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, zeros, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_TRUE(std::isfinite(conf));
}

TEST_F(OptimizationRegression, LargeInputValues) {
    if (!brain) GTEST_SKIP();
    float large[INPUT_SIZE] = {100, -100, 50, -50, 0, 100, -100, 50, -50, 0};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_brain_predict_fast(brain, large, INPUT_SIZE, label, &conf);
    EXPECT_TRUE(std::isfinite(conf));
}

TEST_F(OptimizationRegression, TinyInputValues) {
    if (!brain) GTEST_SKIP();
    float tiny[INPUT_SIZE] = {1e-30f, -1e-30f, 1e-20f, -1e-20f, 0,
                       1e-30f, -1e-30f, 1e-20f, -1e-20f, 0};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_brain_predict_fast(brain, tiny, INPUT_SIZE, label, &conf);
    EXPECT_TRUE(std::isfinite(conf));
}

TEST_F(OptimizationRegression, LearnWithZeroTarget) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float zeros[OUTPUT_SIZE] = {};
    nimcp_training_result_t result = {};
    nimcp_brain_train_step(brain, input, INPUT_SIZE, zeros, OUTPUT_SIZE, &result);
    EXPECT_TRUE(std::isfinite(result.loss));
}

TEST_F(OptimizationRegression, LearnWithAllOnesTarget) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    float ones[OUTPUT_SIZE];
    for (uint32_t i = 0; i < OUTPUT_SIZE; i++) ones[i] = 1.0f;
    nimcp_training_result_t result = {};
    nimcp_brain_train_step(brain, input, INPUT_SIZE, ones, OUTPUT_SIZE, &result);
    EXPECT_TRUE(std::isfinite(result.loss));
}

TEST_F(OptimizationRegression, RapidTrainInferCycles) {
    if (!brain) GTEST_SKIP();
    for (int i = 0; i < RAPID_CYCLE_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i + j) / (float)(RAPID_CYCLE_ITERS + INPUT_SIZE);
        if (i % 2 == 0) {
            float target[OUTPUT_SIZE] = {};
            target[i % OUTPUT_SIZE] = 1.0f;
            nimcp_training_result_t result = {};
            nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        } else {
            char label[LABEL_BUF_SIZE] = {};
            float conf = 0.0f;
            nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
        }
    }
    SUCCEED();
}

TEST_F(OptimizationRegression, IdenticalInputsProduceSameOutput) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {0.42f, 0.42f, 0.42f, 0.42f, 0.42f,
                        0.42f, 0.42f, 0.42f, 0.42f, 0.42f};
    char l1[LABEL_BUF_SIZE] = {}, l2[LABEL_BUF_SIZE] = {};
    float c1 = 0, c2 = 0;
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, l1, &c1);
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, l2, &c2);
    EXPECT_NEAR(c1, c2, CONSISTENCY_TOL);
}

TEST_F(OptimizationRegression, CreateDestroyMultipleBrains) {
    for (int i = 0; i < CREATE_DESTROY_ITERS; i++) {
        nimcp_brain_t temp = nimcp_brain_create_fast("temp_reg",
                                    NIMCP_TASK_CLASSIFICATION,
                                    SMALL_INPUT_SIZE, SMALL_OUTPUT_SIZE, SMALL_NEURONS);
        if (temp) {
            float input[SMALL_INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
            char label[LABEL_BUF_SIZE] = {};
            float conf = 0.0f;
            nimcp_brain_predict_fast(temp, input, SMALL_INPUT_SIZE, label, &conf);
            nimcp_brain_destroy(temp);
        }
    }
    SUCCEED();
}
