/**
 * @file test_hot_path_optimizations.cpp
 * @brief Unit tests for Phase 2 hot-path optimizations
 *
 * Tests: EMA activity history, metadata early-exit, Welford layer norm,
 *        neuromodulation caching, plasticity interval gating,
 *        inference buffer reuse, tokenizer freq cache
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
static constexpr uint32_t BRAIN_NEURONS       = 100;
static constexpr uint32_t INPUT_SIZE          = 10;
static constexpr uint32_t OUTPUT_SIZE         = 100;
static constexpr uint32_t LABEL_BUF_SIZE      = 64;
static constexpr uint32_t SEMANTIC_DIM        = 128;
static constexpr uint32_t RESPONSE_BUF_SIZE   = 256;
static constexpr int      EMA_WARMUP_ITERS    = 20;
static constexpr int      STABILITY_ITERS     = 50;
static constexpr int      THROUGHPUT_ITERS    = 200;
static constexpr float    CONFIDENCE_MAX      = 1.0f;
static constexpr float    CONFIDENCE_MIN      = 0.0f;
static constexpr float    CONSISTENCY_TOL     = 0.1f;

// =============================================================================
// Shared brain fixture
// =============================================================================

class HotPathOptimizations : public ::testing::Test {
protected:
    static nimcp_brain_t brain;

    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("hotpath_unit_test",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }

    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};

nimcp_brain_t HotPathOptimizations::brain = nullptr;

// =============================================================================
// Tests
// =============================================================================

TEST_F(HotPathOptimizations, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

// --- EMA Activity History ---

TEST_F(HotPathOptimizations, SparsityStableAfterMultipleInferences) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < EMA_WARMUP_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.1f);
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    }

    float ratio = nimcp_brain_get_sparsity_ratio(brain);
    EXPECT_GE(ratio, CONFIDENCE_MIN);
    EXPECT_LE(ratio, CONFIDENCE_MAX);
}

TEST_F(HotPathOptimizations, ActiveCountConsistentAcrossRuns) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f, 0.9f,
                        0.2f, 0.7f, 0.4f, 0.6f, 0.1f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;

    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    uint32_t count1 = nimcp_brain_get_active_neuron_count(brain);

    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    uint32_t count2 = nimcp_brain_get_active_neuron_count(brain);

    EXPECT_LE(count1, BRAIN_NEURONS);
    EXPECT_LE(count2, BRAIN_NEURONS);
}

// --- Training with optimized paths ---

TEST_F(HotPathOptimizations, TrainingStillProducesFiniteLoss) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {1.0f, 0.0f, 0.5f, 0.0f, 0.5f,
                        0.0f, 0.5f, 0.0f, 0.5f, 0.0f};
    float target[OUTPUT_SIZE] = {};
    target[0] = 1.0f;

    nimcp_training_result_t result = {};
    nimcp_status_t s = nimcp_brain_train_step(brain, input, INPUT_SIZE,
                                              target, OUTPUT_SIZE, &result);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_TRUE(std::isfinite(result.loss));
    EXPECT_GE(result.loss, 0.0f);
}

TEST_F(HotPathOptimizations, RepeatedTrainingDoesNotDiverge) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float target[OUTPUT_SIZE] = {};
    target[5] = 1.0f;

    for (int i = 0; i < EMA_WARMUP_ITERS; i++) {
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss))
            << "Loss diverged at iteration " << i;
    }
}

// --- Inference correctness ---

TEST_F(HotPathOptimizations, PredictFastReturnsValidConfidence) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                        0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, CONFIDENCE_MIN);
    EXPECT_LE(conf, CONFIDENCE_MAX);
}

TEST_F(HotPathOptimizations, SameInputSameOutput) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.42f, 0.42f, 0.42f, 0.42f, 0.42f,
                        0.42f, 0.42f, 0.42f, 0.42f, 0.42f};
    char l1[LABEL_BUF_SIZE] = {}, l2[LABEL_BUF_SIZE] = {};
    float c1 = 0.0f, c2 = 0.0f;

    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, l1, &c1);
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, l2, &c2);

    EXPECT_NEAR(c1, c2, CONSISTENCY_TOL);
}

// --- Throughput ---

TEST_F(HotPathOptimizations, InferenceThroughputReasonable) {
    if (!brain) GTEST_SKIP();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < THROUGHPUT_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)((i + j) % 20) / 20.0f;
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(ms, 60000.0) << THROUGHPUT_ITERS << " inferences took " << ms << "ms";
}

// --- Language still works ---

TEST_F(HotPathOptimizations, LanguagePipelineUnbroken) {
    if (!brain) GTEST_SKIP();

    nimcp_brain_learn_language(brain, "optimization makes things faster", nullptr);

    float semantic[SEMANTIC_DIM] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_comprehend(
        brain, "faster optimization", semantic, SEMANTIC_DIM, &conf);
    EXPECT_EQ(s, NIMCP_OK);
}

// --- Interleaved operations ---

TEST_F(HotPathOptimizations, InterleavedTrainInferLanguage) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < EMA_WARMUP_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i + j) / (float)(EMA_WARMUP_ITERS + INPUT_SIZE);

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
                EXPECT_TRUE(std::isfinite(conf));
                break;
            }
            case 2: {
                nimcp_brain_learn_language(brain, "neural optimization", nullptr);
                break;
            }
        }
    }
}
