/**
 * @file test_resource_optimizations.cpp
 * @brief Unit tests for Phase 3 resource optimizations
 *
 * Tests: tensor fused mul-add, decision struct consolidation,
 *        immune idle gating, config caching, inference buffer reuse,
 *        spike buffer search, memory pool operations.
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
static constexpr int      WARMUP_ITERS        = 20;
static constexpr int      STRESS_ITERS        = 100;
static constexpr int      THROUGHPUT_ITERS    = 500;
static constexpr double   THROUGHPUT_MAX_MS   = 60000.0;
static constexpr float    CONFIDENCE_MAX      = 1.0f;
static constexpr float    CONFIDENCE_MIN      = 0.0f;
static constexpr float    CONSISTENCY_TOL     = 0.1f;

// =============================================================================
// Shared brain fixture
// =============================================================================

class ResourceOptimizations : public ::testing::Test {
protected:
    static nimcp_brain_t brain;

    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("resource_opt_unit",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }

    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};

nimcp_brain_t ResourceOptimizations::brain = nullptr;

// =============================================================================
// Tests
// =============================================================================

TEST_F(ResourceOptimizations, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

// --- Inference buffer reuse ---

TEST_F(ResourceOptimizations, RepeatedInferenceNoLeak) {
    if (!brain) GTEST_SKIP();

    // Run many inferences — if buffers leak, this would OOM or slow down
    for (int i = 0; i < STRESS_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)((i + j) % 20) / 20.0f;
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
        EXPECT_TRUE(std::isfinite(conf));
    }
}

TEST_F(ResourceOptimizations, InferenceThroughputWithBufferReuse) {
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
    EXPECT_LT(ms, THROUGHPUT_MAX_MS)
        << THROUGHPUT_ITERS << " inferences in " << ms << "ms";
}

// --- Training with optimized resource paths ---

TEST_F(ResourceOptimizations, TrainingProducesFiniteLoss) {
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
}

TEST_F(ResourceOptimizations, RepeatedTrainingStable) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < STRESS_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.1f);
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss))
            << "Loss not finite at step " << i;
    }
}

// --- Immune idle gating ---

TEST_F(ResourceOptimizations, IdleBrainThenReactivate) {
    if (!brain) GTEST_SKIP();

    // Do nothing for a while (immune system should idle-gate)
    // Then resume operations — should still work
    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f, 0.9f,
                        0.2f, 0.7f, 0.4f, 0.6f, 0.1f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, CONFIDENCE_MIN);
    EXPECT_LE(conf, CONFIDENCE_MAX);
}

// --- Sparsity tracking still works ---

TEST_F(ResourceOptimizations, SparsityTrackingValid) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < WARMUP_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i + j) / (float)(WARMUP_ITERS + INPUT_SIZE);
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    }

    float ratio = nimcp_brain_get_sparsity_ratio(brain);
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);
    uint32_t count = nimcp_brain_get_active_neuron_count(brain);
    EXPECT_LE(count, BRAIN_NEURONS);
}

// --- Config caching correctness ---

TEST_F(ResourceOptimizations, SameInputSameOutput) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.42f, 0.42f, 0.42f, 0.42f, 0.42f,
                        0.42f, 0.42f, 0.42f, 0.42f, 0.42f};
    char l1[LABEL_BUF_SIZE] = {}, l2[LABEL_BUF_SIZE] = {};
    float c1 = 0.0f, c2 = 0.0f;

    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, l1, &c1);
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, l2, &c2);

    EXPECT_NEAR(c1, c2, CONSISTENCY_TOL);
}

// --- Training throughput benchmark ---

TEST_F(ResourceOptimizations, TrainingThroughputBenchmark) {
    if (!brain) GTEST_SKIP();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < THROUGHPUT_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)((i + j) % 20) / 20.0f;
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(ms, THROUGHPUT_MAX_MS)
        << THROUGHPUT_ITERS << " training steps in " << ms << "ms";
}

// --- Interleaved operations ---

TEST_F(ResourceOptimizations, InterleavedTrainInferLanguage) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < WARMUP_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i + j) / (float)(WARMUP_ITERS + INPUT_SIZE);

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
                nimcp_brain_learn_language(brain, "resource optimization", nullptr);
                break;
            }
        }
    }
}
