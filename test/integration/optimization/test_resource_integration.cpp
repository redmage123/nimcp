/**
 * @file test_resource_integration.cpp
 * @brief Integration tests for Phase 3 resource optimizations
 *
 * Tests cross-system interactions: training + inference pipeline,
 * sparsity tracking, language + classification, stress workloads.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

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
static constexpr uint32_t SEMANTIC_DIM        = 128;
static constexpr uint32_t RESPONSE_BUF_SIZE   = 256;
static constexpr int      TRAINING_EPOCHS     = 10;
static constexpr int      NUM_CLASSES         = 3;
static constexpr int      MIXED_OPS_ITERS     = 30;
static constexpr int      STRESS_ITERS        = 100;

class ResourceIntegration : public ::testing::Test {
protected:
    static nimcp_brain_t brain;
    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("resource_integration",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }
    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};
nimcp_brain_t ResourceIntegration::brain = nullptr;

TEST_F(ResourceIntegration, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

TEST_F(ResourceIntegration, TrainThenInferPipeline) {
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

TEST_F(ResourceIntegration, SparsityTrackingWithResourceOpts) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < MIXED_OPS_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.2f);

        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);

        float ratio = nimcp_brain_get_sparsity_ratio(brain);
        EXPECT_GE(ratio, 0.0f);
        EXPECT_LE(ratio, 1.0f);

        uint32_t count = nimcp_brain_get_active_neuron_count(brain);
        EXPECT_LE(count, BRAIN_NEURONS);
    }
}

TEST_F(ResourceIntegration, LanguageAndClassificationTogether) {
    if (!brain) GTEST_SKIP();

    nimcp_brain_learn_language(brain, "optimized memory pools are efficient", nullptr);
    nimcp_brain_learn_language(brain, "tensor fusion reduces overhead", nullptr);

    float input[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float target[OUTPUT_SIZE] = {};
    target[0] = 1.0f;
    nimcp_training_result_t result = {};
    nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
    EXPECT_TRUE(std::isfinite(result.loss));

    float semantic[SEMANTIC_DIM] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_comprehend(
        brain, "efficient memory", semantic, SEMANTIC_DIM, &conf);
    EXPECT_EQ(s, NIMCP_OK);

    char label[LABEL_BUF_SIZE] = {};
    s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
}

TEST_F(ResourceIntegration, MixedWorkloadStress) {
    if (!brain) GTEST_SKIP();

    for (int round = 0; round < MIXED_OPS_ITERS; round++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(round + j) / (float)(MIXED_OPS_ITERS + INPUT_SIZE);

        float target[OUTPUT_SIZE] = {};
        target[round % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));

        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
        EXPECT_TRUE(std::isfinite(conf));

        if (round % 5 == 0)
            nimcp_brain_learn_language(brain, "resource integration test", nullptr);

        float ratio = nimcp_brain_get_sparsity_ratio(brain);
        EXPECT_GE(ratio, 0.0f);
    }
}

TEST_F(ResourceIntegration, GroundedResponseStillWorks) {
    if (!brain) GTEST_SKIP();

    char response[RESPONSE_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_grounded_respond(
        brain, "describe the resource optimization", response, sizeof(response), &conf);
    EXPECT_EQ(s, NIMCP_OK);
}

TEST_F(ResourceIntegration, HeavyTrainingThenInference) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < STRESS_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)((i + j) % INPUT_SIZE) / (float)INPUT_SIZE;
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));
    }

    // Inference after heavy training should still work
    float input[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
}
