/**
 * @file test_optimization_integration.cpp
 * @brief Integration tests for all 40-watt brain optimization phases
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdio>

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
static constexpr uint32_t SEMANTIC_DIM       = 128;
static constexpr uint32_t RESPONSE_BUF_SIZE  = 256;
static constexpr int      TRAINING_EPOCHS    = 5;
static constexpr int      NUM_CLASSES        = 3;
static constexpr int      MIXED_OPS_ITERS    = 10;
static constexpr int      STRESS_ITERS       = 50;

class OptimizationIntegration : public ::testing::Test {
protected:
    static nimcp_brain_t brain;
    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("optim_integration",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }
    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};
nimcp_brain_t OptimizationIntegration::brain = nullptr;

TEST_F(OptimizationIntegration, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

TEST_F(OptimizationIntegration, TrainAndInferPipeline) {
    if (!brain) GTEST_SKIP();

    float inputs[NUM_CLASSES][INPUT_SIZE] = {
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
    };
    float targets[NUM_CLASSES][OUTPUT_SIZE] = {};
    targets[0][0] = 1.0f;
    targets[1][1] = 1.0f;
    targets[2][2] = 1.0f;

    for (int epoch = 0; epoch < TRAINING_EPOCHS; epoch++) {
        for (int i = 0; i < NUM_CLASSES; i++) {
            nimcp_training_result_t result = {};
            nimcp_brain_train_step(brain, inputs[i], INPUT_SIZE,
                                   targets[i], OUTPUT_SIZE, &result);
            EXPECT_TRUE(std::isfinite(result.loss));
        }
    }

    for (int i = 0; i < NUM_CLASSES; i++) {
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_status_t s = nimcp_brain_predict_fast(brain, inputs[i], INPUT_SIZE,
                                                     label, &conf);
        EXPECT_EQ(s, NIMCP_OK);
        EXPECT_TRUE(std::isfinite(conf));
    }
}

TEST_F(OptimizationIntegration, ActiveNeuronTrackingDuringTraining) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float target[OUTPUT_SIZE] = {};
    target[0] = 1.0f;
    nimcp_training_result_t result = {};
    nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);

    uint32_t count = nimcp_brain_get_active_neuron_count(brain);
    float ratio = nimcp_brain_get_sparsity_ratio(brain);
    EXPECT_LE(count, BRAIN_NEURONS);
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);
}

TEST_F(OptimizationIntegration, LanguageWithOptimizations) {
    if (!brain) GTEST_SKIP();

    nimcp_brain_learn_language(brain, "the big red dog runs fast", nullptr);

    float semantic[SEMANTIC_DIM] = {};
    float confidence = 0.0f;
    nimcp_status_t s = nimcp_brain_comprehend(
        brain, "big dog", semantic, SEMANTIC_DIM, &confidence);
    EXPECT_EQ(s, NIMCP_OK);

    char response[RESPONSE_BUF_SIZE] = {};
    s = nimcp_brain_grounded_respond(
        brain, "describe the dog", response, sizeof(response), &confidence);
    EXPECT_EQ(s, NIMCP_OK);
}

TEST_F(OptimizationIntegration, WorkspaceReuseAcrossMixedOperations) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < MIXED_OPS_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)((i + j) % INPUT_SIZE) / (float)INPUT_SIZE;

        switch (i % 3) {
            case 0: {
                float target[OUTPUT_SIZE] = {};
                target[i % OUTPUT_SIZE] = 1.0f;
                nimcp_training_result_t result = {};
                nimcp_brain_train_step(brain, input, INPUT_SIZE,
                                       target, OUTPUT_SIZE, &result);
                EXPECT_TRUE(std::isfinite(result.loss));
                break;
            }
            case 1: {
                char label[LABEL_BUF_SIZE] = {};
                float conf = 0.0f;
                nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
                break;
            }
            case 2: {
                nimcp_brain_learn_language(brain, "neural networks learn patterns", nullptr);
                break;
            }
        }
    }
}

TEST_F(OptimizationIntegration, StressTestAllPhases) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < STRESS_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.1f);
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));
    }

    float check[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                         0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_brain_predict_fast(brain, check, INPUT_SIZE, label, &conf);
    EXPECT_TRUE(std::isfinite(conf));
    EXPECT_LE(nimcp_brain_get_active_neuron_count(brain), BRAIN_NEURONS);
}
