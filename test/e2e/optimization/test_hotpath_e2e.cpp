/**
 * @file test_hotpath_e2e.cpp
 * @brief End-to-end tests for Phase 2 hot-path optimizations
 *
 * Full pipeline tests: train classifier end-to-end, grounded language,
 * mixed workload stability, throughput benchmarks.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>

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
static constexpr uint32_t SEMANTIC_DIM          = 128;
static constexpr uint32_t TEXT_BUF_SIZE         = 256;
static constexpr int      TRAINING_EPOCHS       = 15;
static constexpr int      NUM_CLASSES           = 3;
static constexpr float    LOSS_DRIFT_MARGIN     = 20.0f;
static constexpr int      THROUGHPUT_ITERS      = 500;
static constexpr double   THROUGHPUT_MAX_MS     = 60000.0;
static constexpr int      MIXED_ROUNDS          = 10;
static constexpr int      HEAVY_TRAIN_ITERS     = 200;

class HotPathE2E : public ::testing::Test {
protected:
    static nimcp_brain_t brain;
    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("hotpath_e2e",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }
    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};
nimcp_brain_t HotPathE2E::brain = nullptr;

TEST_F(HotPathE2E, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

TEST_F(HotPathE2E, TrainClassifierEndToEnd) {
    if (!brain) GTEST_SKIP();

    float first_loss = 0, last_loss = 0;
    for (int epoch = 0; epoch < TRAINING_EPOCHS; epoch++) {
        for (int cls = 0; cls < NUM_CLASSES; cls++) {
            float input[INPUT_SIZE] = {};
            input[cls] = 1.0f;
            input[cls + NUM_CLASSES] = 0.5f;
            float target[OUTPUT_SIZE] = {};
            target[cls] = 1.0f;

            nimcp_training_result_t result = {};
            nimcp_brain_train_step(brain, input, INPUT_SIZE,
                                   target, OUTPUT_SIZE, &result);
            EXPECT_TRUE(std::isfinite(result.loss));
            if (epoch == 0 && cls == 0) first_loss = result.loss;
            last_loss = result.loss;
        }
    }
    EXPECT_LE(last_loss, first_loss + LOSS_DRIFT_MARGIN);
}

TEST_F(HotPathE2E, InferenceAfterHeavyTraining) {
    if (!brain) GTEST_SKIP();

    // Heavy training phase
    for (int i = 0; i < HEAVY_TRAIN_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.1f);
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
    }

    // Inference should still work
    float input[INPUT_SIZE] = {1, 0, 0, 0.5f, 0, 0, 0, 0, 0, 0};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

TEST_F(HotPathE2E, GroundedLanguagePipeline) {
    if (!brain) GTEST_SKIP();

    float dog_f[SEMANTIC_DIM] = {};
    float cat_f[SEMANTIC_DIM] = {};
    dog_f[0] = 1.0f; dog_f[10] = 0.5f;
    cat_f[5] = 1.0f; cat_f[15] = 0.5f;

    nimcp_brain_ground_word(brain, "dog", dog_f, SEMANTIC_DIM, 0, 0.9f);
    nimcp_brain_ground_word(brain, "cat", cat_f, SEMANTIC_DIM, 0, 0.9f);

    nimcp_brain_learn_language(brain, "the fast dog runs efficiently", nullptr);
    nimcp_brain_learn_language(brain, "the slow cat rests quietly", nullptr);

    float semantic[SEMANTIC_DIM] = {};
    float conf = 0;
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

TEST_F(HotPathE2E, InferenceThroughputBenchmark) {
    if (!brain) GTEST_SKIP();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < THROUGHPUT_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)((i + j) % 20) / 20.0f;
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double per_inference_us = (ms * 1000.0) / THROUGHPUT_ITERS;
    EXPECT_LT(ms, THROUGHPUT_MAX_MS)
        << THROUGHPUT_ITERS << " inferences in " << ms << "ms ("
        << per_inference_us << " us/inference)";
}

TEST_F(HotPathE2E, MixedWorkloadStability) {
    if (!brain) GTEST_SKIP();

    for (int round = 0; round < MIXED_ROUNDS; round++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(round + j) / 15.0f;

        // Train
        float target[OUTPUT_SIZE] = {};
        target[round * INPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));

        // Infer
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
        EXPECT_TRUE(std::isfinite(conf));

        // Language
        nimcp_brain_learn_language(brain, "optimized neural pathways", nullptr);

        // Sparsity
        float ratio = nimcp_brain_get_sparsity_ratio(brain);
        EXPECT_GE(ratio, 0.0f);
        EXPECT_LE(ratio, 1.0f);
    }
}

TEST_F(HotPathE2E, SparsityTrackingDuringWorkload) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < MIXED_ROUNDS * 2; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i * j % 17) / 17.0f;
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);

        float ratio = nimcp_brain_get_sparsity_ratio(brain);
        EXPECT_GE(ratio, 0.0f);
        EXPECT_LE(ratio, 1.0f);
    }
}

TEST_F(HotPathE2E, TrainingThroughputBenchmark) {
    if (!brain) GTEST_SKIP();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < THROUGHPUT_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)((i + j) % 20) / 20.0f;
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    double per_train_us = (ms * 1000.0) / THROUGHPUT_ITERS;
    EXPECT_LT(ms, THROUGHPUT_MAX_MS)
        << THROUGHPUT_ITERS << " training steps in " << ms << "ms ("
        << per_train_us << " us/step)";
}
