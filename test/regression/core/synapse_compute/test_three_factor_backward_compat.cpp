//=============================================================================
// test_three_factor_backward_compat.cpp - Backward Compatibility Tests
//=============================================================================
/**
 * @file test_three_factor_backward_compat.cpp
 * @brief Regression tests ensuring backward compatibility
 *
 * WHAT: Tests that existing code still works with new three-factor implementation
 * WHY:  Ensure no breaking changes for existing users
 * HOW:  Test legacy usage patterns and edge cases
 *
 * REGRESSION SCENARIOS:
 * 1. Simple inline trace mode (no eligibility allocation)
 * 2. Learning without neuromodulator system
 * 3. Learning without context
 * 4. NULL pointer handling
 * 5. Legacy API compatibility
 *
 * @author NIMCP Test Suite
 * @date 2025-11-16
 * @version 2.7.1
 */

#include <gtest/gtest.h>
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/synapse_compute/nimcp_synapse_compute.h"
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class ThreeFactorBackwardCompatTest : public ::testing::Test {
protected:
    synapse_t* synapse;
    neuron_t* pre_neuron;
    neuron_t* post_neuron;

    void SetUp() override {
        synapse = (synapse_t*)calloc(1, sizeof(synapse_t));
        synapse->weight = 0.5f;
        synapse->trace = 0.0f;

        pre_neuron = (neuron_t*)calloc(1, sizeof(neuron_t));
        post_neuron = (neuron_t*)calloc(1, sizeof(neuron_t));
    }

    void TearDown() override {
        if (synapse->eligibility) {
            free(synapse->eligibility);
        }
        free(synapse);
        free(pre_neuron);
        free(post_neuron);
    }
};

//=============================================================================
// Legacy Usage Pattern Tests
//=============================================================================

TEST_F(ThreeFactorBackwardCompatTest, InlineTrace_NoEligibilityAllocation) {
    // WHAT: Synapse learning without eligibility_trace_t allocation
    // WHY:  Legacy code may not allocate eligibility traces
    // EXPECT: Falls back to inline trace mode

    // synapse->eligibility = NULL (default)
    ASSERT_EQ(synapse->eligibility, nullptr);

    float initial_weight = synapse->weight;

    // Apply learning (should use inline trace)
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, nullptr
    );

    // Weight should change (using inline trace)
    EXPECT_NE(synapse->weight, initial_weight);

    // No crash, no memory leak
    EXPECT_EQ(synapse->eligibility, nullptr);  // Should remain NULL
}

TEST_F(ThreeFactorBackwardCompatTest, NoNeuromodulatorSystem) {
    // WHAT: Learning without neuromodulator system
    // WHY:  Legacy networks may not have neuromod system
    // EXPECT: Uses default dopamine level

    synapse_compute_context_t context;
    memset(&context, 0, sizeof(context));
    context.neuromodulator_system = nullptr;  // No neuromod system

    float initial_weight = synapse->weight;

    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, &context
    );

    // Should still learn (with default dopamine)
    EXPECT_NE(synapse->weight, initial_weight);
}

TEST_F(ThreeFactorBackwardCompatTest, NullContext) {
    // WHAT: Learning with NULL context
    // WHY:  Legacy code may pass NULL
    // EXPECT: Graceful degradation, uses defaults

    float initial_weight = synapse->weight;

    // Pass NULL context
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, nullptr
    );

    // Should still work
    EXPECT_NE(synapse->weight, initial_weight);
}

TEST_F(ThreeFactorBackwardCompatTest, NullSynapse) {
    // WHAT: NULL synapse pointer
    // WHY:  Guard against programming errors
    // EXPECT: No crash, graceful return

    // Should not crash
    EXPECT_NO_THROW({
        synapse_learn_three_factor(
            nullptr, pre_neuron, post_neuron,
            10.0f, 15.0f, 1.0f, nullptr
        );
    });
}

//=============================================================================
// Inline Trace Behavior Tests
//=============================================================================

TEST_F(ThreeFactorBackwardCompatTest, InlineTrace_STDP) {
    // WHAT: Inline trace should implement STDP
    // WHY:  Basic learning rule compatibility
    // EXPECT: Pre-before-post → LTP, post-before-pre → LTD

    // LTP: pre before post
    synapse->weight = 0.5f;
    synapse->trace = 0.0f;

    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f,  // pre first
        15.0f,  // post after
        1.0f, nullptr
    );

    float weight_ltp = synapse->weight;

    // LTD: post before pre
    synapse->weight = 0.5f;
    synapse->trace = 0.0f;

    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        15.0f,  // pre after
        10.0f,  // post first
        1.0f, nullptr
    );

    float weight_ltd = synapse->weight;

    // LTP should increase weight more than LTD
    EXPECT_GT(weight_ltp - 0.5f, weight_ltd - 0.5f);
}

TEST_F(ThreeFactorBackwardCompatTest, InlineTrace_Decay) {
    // WHAT: Inline trace should decay over time
    // WHY:  Temporal credit assignment window
    // EXPECT: Multiple calls decay the trace

    // Build up trace
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 0.0f, nullptr
    );

    float trace_after_spike = synapse->trace;

    // Apply decay (no spikes, just time)
    synapse_compute_context_t context;
    memset(&context, 0, sizeof(context));

    for (int i = 0; i < 10; i++) {
        synapse_learn_three_factor(
            synapse, pre_neuron, post_neuron,
            0.0f, 0.0f, 0.0f, &context
        );
    }

    float trace_after_decay = synapse->trace;

    // Trace should have decayed
    EXPECT_LT(trace_after_decay, trace_after_spike);
}

//=============================================================================
// Weight Bounds Tests (Legacy Behavior)
//=============================================================================

TEST_F(ThreeFactorBackwardCompatTest, WeightBounds_LegacyLimits) {
    // WHAT: Weight bounds should be [-10, 10] (legacy limits)
    // WHY:  Prevent numerical overflow
    // EXPECT: Clamping at [-10, 10]

    // Test upper bound
    synapse->weight = 9.5f;

    for (int i = 0; i < 100; i++) {
        synapse_learn_three_factor(
            synapse, pre_neuron, post_neuron,
            10.0f, 15.0f, 1.0f, nullptr
        );
    }

    EXPECT_LE(synapse->weight, 10.0f);

    // Test lower bound
    synapse->weight = -9.5f;

    for (int i = 0; i < 100; i++) {
        synapse_learn_three_factor(
            synapse, pre_neuron, post_neuron,
            10.0f, 15.0f, -1.0f, nullptr
        );
    }

    EXPECT_GE(synapse->weight, -10.0f);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(ThreeFactorBackwardCompatTest, ZeroSpikeTimes) {
    // WHAT: Both spike times = 0
    // WHY:  Indicates no spikes (common case)
    // EXPECT: No crash, trace decays

    float initial_trace = 0.5f;
    synapse->trace = initial_trace;

    synapse_compute_context_t context;
    memset(&context, 0, sizeof(context));

    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        0.0f, 0.0f, 0.0f, &context
    );

    // Trace should decay (not stay constant)
    EXPECT_LE(synapse->trace, initial_trace);
}

TEST_F(ThreeFactorBackwardCompatTest, OnlyPreSpike) {
    // WHAT: Only pre-spike, no post-spike
    // WHY:  Partial spike pair
    // EXPECT: No STDP update, just decay

    synapse->trace = 0.0f;

    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f,  // pre spike
        0.0f,   // no post spike
        0.0f, nullptr
    );

    // Trace should not increase significantly (no pair)
    EXPECT_NEAR(synapse->trace, 0.0f, 0.1f);
}

TEST_F(ThreeFactorBackwardCompatTest, ZeroReward) {
    // WHAT: Reward signal = 0
    // WHY:  Neutral outcome
    // EXPECT: Weight unchanged (no reward → no learning)

    float initial_weight = synapse->weight;

    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f,
        0.0f,  // Zero reward
        nullptr
    );

    // Weight should not change significantly
    EXPECT_NEAR(synapse->weight, initial_weight, 0.05f);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(ThreeFactorBackwardCompatTest, VeryLargeTimeDifference) {
    // WHAT: Δt = 1000ms (very large)
    // WHY:  Test exp() overflow
    // EXPECT: No crash, negligible STDP

    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f,
        1010.0f,  // 1000ms later
        1.0f, nullptr
    );

    // Should not crash
    EXPECT_TRUE(std::isfinite(synapse->weight));
}

TEST_F(ThreeFactorBackwardCompatTest, VerySmallTimeDifference) {
    // WHAT: Δt = 0.1ms (very small)
    // WHY:  Test numerical precision
    // EXPECT: Strong STDP

    float initial_weight = synapse->weight;

    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f,
        10.1f,  // 0.1ms later
        1.0f, nullptr
    );

    // Should produce strong learning
    float change = std::abs(synapse->weight - initial_weight);
    EXPECT_GT(change, 0.001f);
}

//=============================================================================
// Repeated Learning Tests
//=============================================================================

TEST_F(ThreeFactorBackwardCompatTest, RepeatedLearning_NoMemoryLeak) {
    // WHAT: Call learning 10,000 times
    // WHY:  Check for memory leaks or corruption
    // EXPECT: No crash, stable behavior

    for (int i = 0; i < 10000; i++) {
        synapse_learn_three_factor(
            synapse, pre_neuron, post_neuron,
            10.0f + (i % 100) / 10.0f,
            15.0f + (i % 100) / 10.0f,
            (i % 2) ? 1.0f : -1.0f,
            nullptr
        );
    }

    // Should not crash
    EXPECT_TRUE(std::isfinite(synapse->weight));
    EXPECT_TRUE(std::isfinite(synapse->trace));

    // Weight should be within bounds
    EXPECT_GE(synapse->weight, -10.0f);
    EXPECT_LE(synapse->weight, 10.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
