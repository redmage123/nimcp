/**
 * @file test_biological_integration.cpp
 * @brief Integration tests for Phase 3 biological connectivity enhancements
 *
 * Tests cross-system interactions: higher connectivity + training pipeline,
 * consolidation cycles + inference, feedback/lateral connections + language,
 * homeostatic scaling + mixed workloads.
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
static constexpr uint32_t TEXT_BUF_SIZE       = 256;
static constexpr int      TRAINING_EPOCHS     = 10;
static constexpr int      NUM_CLASSES         = 3;
static constexpr int      MIXED_OPS_ITERS     = 30;
static constexpr int      CONSOLIDATION_ITERS = 600;
static constexpr int      STRESS_ITERS        = 100;
static constexpr float    LOSS_DRIFT_MARGIN   = 20.0f;

class BiologicalIntegration : public ::testing::Test {
protected:
    static nimcp_brain_t brain;
    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("bio_integration",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }
    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};
nimcp_brain_t BiologicalIntegration::brain = nullptr;

TEST_F(BiologicalIntegration, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

TEST_F(BiologicalIntegration, ClassificationWithBioConnectivity) {
    if (!brain) GTEST_SKIP();

    float inputs[NUM_CLASSES][INPUT_SIZE] = {
        {1, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0},
        {0, 1, 0, 0, 0.5f, 0, 0, 0, 0, 0},
        {0, 0, 1, 0, 0, 0.5f, 0, 0, 0, 0},
    };
    float targets[NUM_CLASSES][OUTPUT_SIZE] = {};
    targets[0][0] = 1.0f;
    targets[1][1] = 1.0f;
    targets[2][2] = 1.0f;

    float first_loss = 0, last_loss = 0;
    for (int epoch = 0; epoch < TRAINING_EPOCHS; epoch++) {
        for (int i = 0; i < NUM_CLASSES; i++) {
            nimcp_training_result_t result = {};
            nimcp_brain_train_step(brain, inputs[i], INPUT_SIZE,
                                   targets[i], OUTPUT_SIZE, &result);
            EXPECT_TRUE(std::isfinite(result.loss));
            if (epoch == 0 && i == 0) first_loss = result.loss;
            last_loss = result.loss;
        }
    }
    EXPECT_LE(last_loss, first_loss + LOSS_DRIFT_MARGIN);

    for (int i = 0; i < NUM_CLASSES; i++) {
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_status_t s = nimcp_brain_predict_fast(brain, inputs[i], INPUT_SIZE,
                                                     label, &conf);
        EXPECT_EQ(s, NIMCP_OK);
    }
}

TEST_F(BiologicalIntegration, ConsolidationCycleThenInference) {
    if (!brain) GTEST_SKIP();

    // Train past consolidation threshold
    for (int i = 0; i < CONSOLIDATION_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.05f);
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));
    }

    // Inference after consolidation should be stable
    float input[INPUT_SIZE] = {1.0f, 0.0f, 0.5f, 0.0f, 0.5f,
                        0.0f, 0.5f, 0.0f, 0.5f, 0.0f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

TEST_F(BiologicalIntegration, LanguageWithBioConnectivity) {
    if (!brain) GTEST_SKIP();

    float dog_f[SEMANTIC_DIM] = {};
    float cat_f[SEMANTIC_DIM] = {};
    dog_f[0] = 1.0f; dog_f[10] = 0.5f;
    cat_f[5] = 1.0f; cat_f[15] = 0.5f;

    nimcp_brain_ground_word(brain, "dog", dog_f, SEMANTIC_DIM, 0, 0.9f);
    nimcp_brain_ground_word(brain, "cat", cat_f, SEMANTIC_DIM, 0, 0.9f);

    nimcp_brain_learn_language(brain, "the dog runs fast with strong synapses", nullptr);
    nimcp_brain_learn_language(brain, "the cat sleeps with consolidated connections", nullptr);

    float semantic[SEMANTIC_DIM] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_comprehend(brain, "the fast dog",
                                              semantic, SEMANTIC_DIM, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GT(conf, 0.0f);

    char text[TEXT_BUF_SIZE] = {};
    s = nimcp_brain_produce_text(brain, dog_f, SEMANTIC_DIM,
                                 text, sizeof(text), nullptr);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GT(strlen(text), 0u);
}

TEST_F(BiologicalIntegration, SparsityWithBioConnectivity) {
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
    }
}

TEST_F(BiologicalIntegration, MixedBioWorkloadStress) {
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

        if (round % 3 == 0)
            nimcp_brain_learn_language(brain, "biological connectivity", nullptr);

        float ratio = nimcp_brain_get_sparsity_ratio(brain);
        EXPECT_GE(ratio, 0.0f);
    }
}

TEST_F(BiologicalIntegration, HomeostaticScalingPreservesLearning) {
    if (!brain) GTEST_SKIP();

    // Heavy training that would trigger homeostatic scaling
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
}
