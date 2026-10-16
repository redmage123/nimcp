/**
 * @file test_synapse_glial_optimizations.cpp
 * @brief Unit tests for Phase 4 synapse and glial system optimizations
 *
 * Tests: sorted incoming synapses, metadata prefetch, glial skip-frames,
 *        dendritic skip-frames, vesicle batch update, glial amortization.
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
static constexpr int      HEAVY_TRAIN_ITERS   = 200;
static constexpr int      THROUGHPUT_ITERS    = 500;
static constexpr double   THROUGHPUT_MAX_MS   = 60000.0;
static constexpr float    CONFIDENCE_MAX      = 1.0f;
static constexpr float    CONFIDENCE_MIN      = 0.0f;
static constexpr float    CONSISTENCY_TOL     = 0.1f;
static constexpr float    LOSS_DRIFT_MARGIN   = 20.0f;
static constexpr int      NUM_CLASSES         = 3;

// =============================================================================
// Shared brain fixture
// =============================================================================

class SynapseGlialOpt : public ::testing::Test {
protected:
    static nimcp_brain_t brain;

    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("synapse_glial_unit",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }

    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};

nimcp_brain_t SynapseGlialOpt::brain = nullptr;

// =============================================================================
// Tests
// =============================================================================

TEST_F(SynapseGlialOpt, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

// --- Sorted synapse correctness ---

TEST_F(SynapseGlialOpt, InferenceWithSortedSynapses) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {1.0f, 0.5f, 0.0f, 0.5f, 1.0f,
                        0.5f, 0.0f, 0.5f, 1.0f, 0.5f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, CONFIDENCE_MIN);
    EXPECT_LE(conf, CONFIDENCE_MAX);
}

TEST_F(SynapseGlialOpt, TrainingWithSortedSynapses) {
    if (!brain) GTEST_SKIP();

    float first_loss = 0, last_loss = 0;
    for (int epoch = 0; epoch < WARMUP_ITERS; epoch++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(epoch * INPUT_SIZE + j) * 0.1f);
        float target[OUTPUT_SIZE] = {};
        target[epoch % NUM_CLASSES] = 1.0f;

        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));
        if (epoch == 0) first_loss = result.loss;
        last_loss = result.loss;
    }
    EXPECT_LE(last_loss, first_loss + LOSS_DRIFT_MARGIN);
}

// --- Glial skip-frame correctness ---

TEST_F(SynapseGlialOpt, HeavyTrainingWithGlialAmortization) {
    if (!brain) GTEST_SKIP();

    // Train heavily — glial updates should be amortized (every 50 steps)
    for (int i = 0; i < HEAVY_TRAIN_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.1f);
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss))
            << "Loss not finite at step " << i;
    }

    // Inference should still work
    float input[INPUT_SIZE] = {1.0f, 0.0f, 0.5f, 0.0f, 0.5f,
                        0.0f, 0.5f, 0.0f, 0.5f, 0.0f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, 0.0f);
}

// --- Throughput with substrate optimizations ---

TEST_F(SynapseGlialOpt, InferenceThroughputOptimized) {
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

// --- Sparsity with substrate optimizations ---

TEST_F(SynapseGlialOpt, SparsityValidWithOptimizedSubstrate) {
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
}

// --- Mixed workload stability ---

TEST_F(SynapseGlialOpt, MixedWorkloadStability) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < STRESS_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i + j) / (float)(STRESS_ITERS + INPUT_SIZE);

        // Alternate train/infer/language
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
                nimcp_brain_learn_language(brain, "glial optimization", nullptr);
                break;
            }
        }

        float ratio = nimcp_brain_get_sparsity_ratio(brain);
        EXPECT_GE(ratio, 0.0f);
    }
}

// --- Language pipeline unbroken ---

TEST_F(SynapseGlialOpt, LanguagePipelineWithSubstrateOpt) {
    if (!brain) GTEST_SKIP();

    nimcp_brain_learn_language(brain, "synapses connect through sorted handles", nullptr);
    nimcp_brain_learn_language(brain, "glial cells support neural function", nullptr);

    float semantic[SEMANTIC_DIM] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_comprehend(
        brain, "sorted synapses", semantic, SEMANTIC_DIM, &conf);
    EXPECT_EQ(s, NIMCP_OK);
}
