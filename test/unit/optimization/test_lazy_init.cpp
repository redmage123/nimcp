/**
 * @file test_lazy_init.cpp
 * @brief Unit tests for Phase 4: Lazy Initialization
 *
 * WHAT: Tests that FAST mode brain skips non-essential subsystem init
 * WHY:  40-watt brain: don't allocate what you don't use
 * HOW:  Create brain in FAST mode, verify it works without optional subsystems
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
static constexpr uint32_t RESPONSE_BUF_SIZE  = 256;
static constexpr int      STABILITY_ITERS    = 50;
static constexpr uint32_t SMALL_NEURONS      = 50;
static constexpr uint32_t SMALL_INPUT_SIZE   = 5;
static constexpr uint32_t SMALL_OUTPUT_SIZE  = 50;

// =============================================================================
// Shared brain fixture
// =============================================================================

class LazyInit : public ::testing::Test {
protected:
    static nimcp_brain_t brain;
    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("lazy_init_test",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }
    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};
nimcp_brain_t LazyInit::brain = nullptr;

TEST_F(LazyInit, BrainCreatedInFastMode) {
    ASSERT_NE(brain, nullptr);
}

TEST_F(LazyInit, InferenceWorksWithoutOptionalSubsystems) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f, 0.9f,
                        0.2f, 0.7f, 0.4f, 0.6f, 0.1f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_GE(conf, 0.0f);
}

TEST_F(LazyInit, TrainingWorksWithoutOptionalSubsystems) {
    if (!brain) GTEST_SKIP();
    float input[INPUT_SIZE] = {1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    float target[OUTPUT_SIZE] = {};
    target[0] = 1.0f;
    nimcp_training_result_t result = {};
    nimcp_status_t s = nimcp_brain_train_step(brain, input, INPUT_SIZE, target, OUTPUT_SIZE, &result);
    EXPECT_EQ(s, NIMCP_OK);
    EXPECT_TRUE(std::isfinite(result.loss));
}

TEST_F(LazyInit, LanguageWorksWithoutEmotionalSystems) {
    if (!brain) GTEST_SKIP();
    char response[RESPONSE_BUF_SIZE] = {};
    float confidence = 0.0f;
    nimcp_status_t s = nimcp_brain_grounded_respond(
        brain, "hello world", response, sizeof(response), &confidence);
    EXPECT_EQ(s, NIMCP_OK);
}

TEST_F(LazyInit, MultipleInferencesStable) {
    if (!brain) GTEST_SKIP();
    for (int i = 0; i < STABILITY_ITERS; i++) {
        float input[INPUT_SIZE];
        for (uint32_t j = 0; j < INPUT_SIZE; j++)
            input[j] = (float)(i * INPUT_SIZE + j) / (float)(STABILITY_ITERS * INPUT_SIZE);
        char label[LABEL_BUF_SIZE] = {};
        float conf = 0.0f;
        nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);
        EXPECT_TRUE(std::isfinite(conf));
    }
}

TEST_F(LazyInit, DestroyCleanWithoutCrash) {
    nimcp_brain_t temp = nimcp_brain_create_fast("temp_lazy",
                                NIMCP_TASK_CLASSIFICATION,
                                SMALL_INPUT_SIZE, SMALL_OUTPUT_SIZE, SMALL_NEURONS);
    if (temp) nimcp_brain_destroy(temp);
    SUCCEED();
}
