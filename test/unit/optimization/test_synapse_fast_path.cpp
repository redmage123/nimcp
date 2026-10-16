/**
 * @file test_synapse_fast_path.cpp
 * @brief Unit tests for Phase 5: Sparse Synapse Fast Path
 */

#include <gtest/gtest.h>
#include <cmath>

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
static constexpr int      THROUGHPUT_ITERS   = 100;
static constexpr float    CONSISTENCY_TOL    = 0.1f;

// =============================================================================
// Shared brain fixture
// =============================================================================

class SynapseFastPath : public ::testing::Test {
protected:
    static nimcp_brain_t brain;
    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("synapse_fast_test",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }
    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};
nimcp_brain_t SynapseFastPath::brain = nullptr;

TEST_F(SynapseFastPath, BrainCreated) {
    ASSERT_NE(brain, nullptr);
}

TEST_F(SynapseFastPath, InferenceWithFastPath) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f, 0.9f,
                        0.2f, 0.7f, 0.4f, 0.6f, 0.1f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);
}

TEST_F(SynapseFastPath, TrainingWithFastPath) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {1.0f, 0.0f, 0.5f, 0.0f, 0.5f,
                        0.0f, 0.5f, 0.0f, 0.5f, 0.0f};
    float target[OUTPUT_SIZE] = {};
    target[3] = 1.0f;
    nimcp_training_result_t result = {};
    nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
    EXPECT_TRUE(std::isfinite(result.loss));
}

TEST_F(SynapseFastPath, ConsistentOutput) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                        0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    char label1[LABEL_BUF_SIZE] = {}, label2[LABEL_BUF_SIZE] = {};
    float conf1 = 0.0f, conf2 = 0.0f;
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label1, &conf1);
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label2, &conf2);
    EXPECT_NEAR(conf1, conf2, CONSISTENCY_TOL);
}

TEST_F(SynapseFastPath, HighThroughput) {
    if (!brain) GTEST_SKIP();
    for (int i = 0; i < THROUGHPUT_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = sinf((float)(i * INPUT_SIZE + j));
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
        EXPECT_TRUE(std::isfinite(conf));
    }
}
