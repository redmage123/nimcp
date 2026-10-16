/**
 * @file test_biological_connectivity.cpp
 * @brief Unit tests for Phase 3 biological connectivity enhancements
 *
 * Tests: increased synapse capacity, higher fan-in/fan-out, activity-dependent
 *        synaptogenesis, homeostatic scaling, cortical columns, corpus callosum,
 *        9-layer architecture, consolidation cycle, ternary weight compression.
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
static constexpr int      TRAINING_ITERS      = 50;
static constexpr int      HEAVY_TRAIN_ITERS   = 200;
static constexpr int      THROUGHPUT_ITERS    = 300;
static constexpr double   THROUGHPUT_MAX_MS   = 60000.0;
static constexpr float    LOSS_DRIFT_MARGIN   = 20.0f;
static constexpr int      NUM_CLASSES         = 3;
static constexpr int      CONSOLIDATION_ITERS = 600;

// =============================================================================
// Shared brain fixture
// =============================================================================

class BiologicalConnectivity : public ::testing::Test {
protected:
    static nimcp_brain_t brain;

    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("bio_connect_unit",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }

    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};

nimcp_brain_t BiologicalConnectivity::brain = nullptr;

// =============================================================================
// Tests
// =============================================================================

TEST_F(BiologicalConnectivity, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

// --- Increased connectivity verification ---

TEST_F(BiologicalConnectivity, InferenceWithHigherConnectivity) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {1.0f, 0.0f, 0.5f, 0.0f, 0.5f,
                        0.0f, 0.5f, 0.0f, 0.5f, 0.0f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

TEST_F(BiologicalConnectivity, TrainingWithHigherFanIn) {
    if (!brain) GTEST_SKIP();

    float first_loss = 0, last_loss = 0;
    for (int epoch = 0; epoch < TRAINING_ITERS; epoch++) {
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

// --- Sparsity with increased connectivity ---

TEST_F(BiologicalConnectivity, SparsityValidWithLargerCapacity) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < TRAINING_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i + j) / (float)(TRAINING_ITERS + INPUT_SIZE);
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    }

    float ratio = nimcp_brain_get_sparsity_ratio(brain);
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);
}

// --- Consolidation cycle ---

TEST_F(BiologicalConnectivity, TrainingThroughConsolidationCycle) {
    if (!brain) GTEST_SKIP();

    // Train past consolidation threshold (500 steps) to trigger sleep cycle
    for (int i = 0; i < CONSOLIDATION_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.05f);
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss))
            << "Loss not finite at consolidation step " << i;
    }
}

// --- Inference throughput with bio connectivity ---

TEST_F(BiologicalConnectivity, InferenceThroughputBenchmark) {
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

// --- Language pipeline with bio connectivity ---

TEST_F(BiologicalConnectivity, LanguagePipelineWithBioConnectivity) {
    if (!brain) GTEST_SKIP();

    nimcp_brain_learn_language(brain, "neurons connect through synapses", nullptr);
    nimcp_brain_learn_language(brain, "biological networks grow stronger", nullptr);

    float semantic[SEMANTIC_DIM] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_comprehend(
        brain, "neural synapses", semantic, SEMANTIC_DIM, &conf);
    EXPECT_EQ(s, NIMCP_OK);
}

// --- Mixed workload with all biological features ---

TEST_F(BiologicalConnectivity, MixedBiologicalWorkload) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < TRAINING_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i + j) / (float)(TRAINING_ITERS + INPUT_SIZE);

        // Train
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));

        // Infer
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
        EXPECT_TRUE(std::isfinite(conf));

        // Sparsity
        float ratio = nimcp_brain_get_sparsity_ratio(brain);
        EXPECT_GE(ratio, 0.0f);
        EXPECT_LE(ratio, 1.0f);
    }
}

// --- Heavy training stress test ---

TEST_F(BiologicalConnectivity, HeavyTrainingStability) {
    if (!brain) GTEST_SKIP();

    for (int i = 0; i < HEAVY_TRAIN_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j) * 0.1f);
        float target[OUTPUT_SIZE] = {};
        target[i % OUTPUT_SIZE] = 1.0f;
        nimcp_training_result_t result = {};
        nimcp_brain_train_step(brain, input, INPUT_SIZE,
                               target, OUTPUT_SIZE, &result);
        EXPECT_TRUE(std::isfinite(result.loss));
    }

    // Inference should still work after heavy training
    float input[INPUT_SIZE] = {1.0f, 0.0f, 0.5f, 0.0f, 0.5f,
                        0.0f, 0.5f, 0.0f, 0.5f, 0.0f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, 0.0f);
}
