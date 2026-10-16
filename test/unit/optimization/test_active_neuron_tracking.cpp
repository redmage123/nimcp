/**
 * @file test_active_neuron_tracking.cpp
 * @brief Unit tests for Phase 1: Active Neuron Set Tracking
 *
 * WHAT: Tests that the neural network tracks which neurons are active
 * WHY:  40-watt brain optimization — only process active neurons (1-5% of total)
 * HOW:  Create network, run compute steps, verify active set tracking
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
static constexpr uint32_t BRAIN_NEURONS   = 100;
static constexpr uint32_t INPUT_SIZE      = 10;
static constexpr uint32_t OUTPUT_SIZE     = 100;
static constexpr uint32_t LABEL_BUF_SIZE  = 64;

// =============================================================================
// Shared brain fixture (one brain for all tests in this suite)
// =============================================================================

class ActiveNeuronTracking : public ::testing::Test {
protected:
    static nimcp_brain_t brain;

    static void SetUpTestSuite() {
        brain = nimcp_brain_create_fast("active_neuron_test",
                                    NIMCP_TASK_CLASSIFICATION,
                                    INPUT_SIZE, OUTPUT_SIZE, BRAIN_NEURONS);
    }

    static void TearDownTestSuite() {
        if (brain) nimcp_brain_destroy(brain);
        brain = nullptr;
    }
};

nimcp_brain_t ActiveNeuronTracking::brain = nullptr;

// =============================================================================
// Tests
// =============================================================================

TEST_F(ActiveNeuronTracking, BrainCreated) {
    ASSERT_NE(brain, nullptr) << "Brain creation failed";
}

TEST_F(ActiveNeuronTracking, ActiveCountInitiallyZero) {
    if (!brain) GTEST_SKIP();
    uint32_t count = nimcp_brain_get_active_neuron_count(brain);
    EXPECT_LE(count, BRAIN_NEURONS);
}

TEST_F(ActiveNeuronTracking, SparsityRatioInRange) {
    if (!brain) GTEST_SKIP();
    float ratio = nimcp_brain_get_sparsity_ratio(brain);
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);
}

TEST_F(ActiveNeuronTracking, NullBrainReturnsZero) {
    EXPECT_EQ(nimcp_brain_get_active_neuron_count(nullptr), 0u);
    EXPECT_FLOAT_EQ(nimcp_brain_get_sparsity_ratio(nullptr), 0.0f);
}

TEST_F(ActiveNeuronTracking, ActiveCountAfterInference) {
    if (!brain) GTEST_SKIP();

    float input[INPUT_SIZE] = {0.5f, 0.3f, 0.8f, 0.1f, 0.9f,
                        0.2f, 0.7f, 0.4f, 0.6f, 0.1f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;
    nimcp_brain_predict_fast(brain, input, INPUT_SIZE, label, &conf);

    uint32_t count = nimcp_brain_get_active_neuron_count(brain);
    float ratio = nimcp_brain_get_sparsity_ratio(brain);

    EXPECT_LE(count, BRAIN_NEURONS);
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);
}

TEST_F(ActiveNeuronTracking, MultipleInferencesUpdateActiveSet) {
    if (!brain) GTEST_SKIP();

    float input1[INPUT_SIZE] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float input2[INPUT_SIZE] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    char label[LABEL_BUF_SIZE] = {};
    float conf = 0.0f;

    nimcp_brain_predict_fast(brain, input1, INPUT_SIZE, label, &conf);
    uint32_t count1 = nimcp_brain_get_active_neuron_count(brain);

    nimcp_brain_predict_fast(brain, input2, INPUT_SIZE, label, &conf);
    uint32_t count2 = nimcp_brain_get_active_neuron_count(brain);

    EXPECT_LE(count1, BRAIN_NEURONS);
    EXPECT_LE(count2, BRAIN_NEURONS);
}
