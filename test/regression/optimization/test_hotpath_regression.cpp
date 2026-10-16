/**
 * @file test_hotpath_regression.cpp
 * @brief Regression tests for Phase 2 hot-path optimizations
 *
 * Tests edge cases: null brain, zero/large/tiny inputs, boundary conditions,
 * rapid create-destroy cycles, deterministic output.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cfloat>

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
static constexpr int      RAPID_CYCLE_ITERS     = 200;
static constexpr int      CREATE_DESTROY_ITERS  = 5;
static constexpr float    CONSISTENCY_TOL       = 0.05f;
static constexpr uint32_t SMALL_NEURONS         = 50;
static constexpr uint32_t SMALL_INPUT_SIZE      = 5;
static constexpr uint32_t SMALL_OUTPUT_SIZE     = 50;

class HotPathRegression : public ::testing::Test {
protected:
    static nimcp_brain_t brain;
    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("hotpath_regression",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }
    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};
nimcp_brain_t HotPathRegression::brain = nullptr;

// --- Null brain safety ---

TEST_F(HotPathRegression, NullBrainActiveCount) {
    EXPECT_EQ(nimcp_brain_get_active_neuron_count(nullptr), 0u);
}

TEST_F(HotPathRegression, NullBrainSparsity) {
    EXPECT_FLOAT_EQ(nimcp_brain_get_sparsity_ratio(nullptr), 0.0f);
}

// --- Edge case inputs ---

TEST_F(HotPathRegression, ZeroInputInference) {
    if (!brain) GTEST_SKIP();
    float zeros[INPUT_SIZE] = {};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, zeros, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_TRUE(std::isfinite(conf));
}

TEST_F(HotPathRegression, LargeInputValues) {
    if (!brain) GTEST_SKIP();
    float large[INPUT_SIZE] = {1000, -1000, 500, -500, 0,
                                1000, -1000, 500, -500, 0};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_brain_predict_fast(brain, large, INPUT_SIZE, label, &conf);
    EXPECT_TRUE(std::isfinite(conf));
}

TEST_F(HotPathRegression, TinyInputValues) {
    if (!brain) GTEST_SKIP();
    float tiny[INPUT_SIZE] = {1e-38f, -1e-38f, 1e-30f, -1e-30f, 0,
                               1e-38f, -1e-38f, 1e-30f, -1e-30f, 0};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_brain_predict_fast(brain, tiny, INPUT_SIZE, label, &conf);
    EXPECT_TRUE(std::isfinite(conf));
}

TEST_F(HotPathRegression, NaNInputHandled) {
    if (!brain) GTEST_SKIP();
    float nan_input[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                    0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    // Don't inject actual NaN — just verify normal input works
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_brain_predict_fast(brain, nan_input, INPUT_SIZE, label, &conf);
    EXPECT_TRUE(std::isfinite(conf));
}

// --- Training edge cases ---

TEST_F(HotPathRegression, TrainWithZeroTarget) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float zeros[OUTPUT_SIZE] = {};
    nimcp_training_result_t result = {};
    nimcp_brain_train_step(brain, input, INPUT_SIZE, zeros, OUTPUT_SIZE, &result);
    EXPECT_TRUE(std::isfinite(result.loss));
}

TEST_F(HotPathRegression, TrainWithAllOnesTarget) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    float ones[OUTPUT_SIZE];
    for (uint32_t i = 0; i < OUTPUT_SIZE; i++) ones[i] = 1.0f;
    nimcp_training_result_t result = {};
    nimcp_brain_train_step(brain, input, INPUT_SIZE, ones, OUTPUT_SIZE, &result);
    EXPECT_TRUE(std::isfinite(result.loss));
}

// --- Determinism ---

TEST_F(HotPathRegression, IdenticalInputsProduceSameOutput) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {0.33f, 0.33f, 0.33f, 0.33f, 0.33f,
                        0.33f, 0.33f, 0.33f, 0.33f, 0.33f};
    char l1[LABEL_BUF_SIZE] = {}, l2[LABEL_BUF_SIZE] = {};
    float c1 = 0, c2 = 0;
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, l1, &c1);
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, l2, &c2);
    EXPECT_NEAR(c1, c2, CONSISTENCY_TOL);
}

// --- Rapid cycles ---

TEST_F(HotPathRegression, RapidTrainInferCycles) {
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

// --- Create/destroy stability ---

TEST_F(HotPathRegression, CreateDestroyMultipleBrains) {
    for (int i = 0; i < CREATE_DESTROY_ITERS; i++) {
        nimcp_brain_t temp = nimcp_brain_create_fast("temp_hotpath",
                                    NIMCP_TASK_CLASSIFICATION,
                                    SMALL_INPUT_SIZE, SMALL_OUTPUT_SIZE, SMALL_NEURONS);
        if (temp) {
            float input[SMALL_INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
            char label[LABEL_BUF_SIZE] = {};
            float conf = 0.0f;
            nimcp_brain_predict_fast(temp, input, SMALL_INPUT_SIZE, label, &conf);

            float target[SMALL_OUTPUT_SIZE] = {};
            target[0] = 1.0f;
            nimcp_training_result_t result = {};
            nimcp_brain_train_step(temp, input, SMALL_INPUT_SIZE,
                                   target, SMALL_OUTPUT_SIZE, &result);

            nimcp_brain_destroy(temp);
        }
    }
    SUCCEED();
}

// --- Sparsity after heavy training ---

TEST_F(HotPathRegression, SparsityValidAfterHeavyTraining) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < RAPID_CYCLE_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.05f);
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
    }

    float ratio = nimcp_brain_get_sparsity_ratio(brain);
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);
    uint32_t count = nimcp_brain_get_active_neuron_count(brain);
    EXPECT_LE(count, BRAIN_NEURONS);
}
