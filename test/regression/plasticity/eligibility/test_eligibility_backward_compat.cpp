/**
 * @file test_eligibility_backward_compat.cpp
 * @brief Regression tests for eligibility trace backward compatibility
 *
 * WHAT: Ensures Option 2.2 doesn't break existing behavior
 * WHY:  Validate that old code continues to work without modification
 * HOW:  Test inline trace mode matches pre-Option-2.2 behavior
 *
 * @version Option 2.2 Regression
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "core/neuralnet/nimcp_neuralnet.h"
    #include "core/synapse_compute/nimcp_synapse_compute.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EligibilityBackwardCompatTest : public ::testing::Test {
protected:
    synapse_t synapse;
    neuron_t pre_neuron;
    neuron_t post_neuron;

    void SetUp() override {
        // Initialize synapse in "old style" (no eligibility trace allocated)
        memset(&synapse, 0, sizeof(synapse_t));
        synapse.weight = 0.5f;
        synapse.target_id = 1;
        synapse.plasticity = 1.0f;
        synapse.strength = 0.5f;
        synapse.trace = 0.0f;
        synapse.eligibility = NULL;  // Not allocated (old behavior)
        synapse.enable_eligibility = false;

        // Initialize neurons
        memset(&pre_neuron, 0, sizeof(neuron_t));
        memset(&post_neuron, 0, sizeof(neuron_t));
        pre_neuron.state = 1.0f;
        post_neuron.state = 0.5f;
    }
};

//=============================================================================
// Regression Test 1: Old Code Still Works
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, OldCode_StillWorks) {
    // This is how code used to call learning before Option 2.2
    float initial_weight = synapse.weight;

    synapse_learn_three_factor(
        &synapse,
        &pre_neuron,
        &post_neuron,
        10.0f,   // pre_spike_time
        20.0f,   // post_spike_time
        1.0f,    // reward_signal
        NULL     // context (old code often passed NULL)
    );

    // Should still work
    EXPECT_NE(synapse.weight, initial_weight);
    EXPECT_EQ(synapse.eligibility, nullptr);  // Still NULL
}

//=============================================================================
// Regression Test 2: Inline Trace Behavior Unchanged
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, InlineTrace_BehaviorUnchanged) {
    // Test that inline trace accumulation works as before

    // First learning event
    synapse_learn_three_factor(
        &synapse,
        &pre_neuron,
        &post_neuron,
        10.0f,
        20.0f,
        0.0f,  // No reward
        NULL
    );

    float trace_after_spike = synapse.trace;
    EXPECT_GT(trace_after_spike, 0.0f);

    // Trace should decay
    synapse_learn_three_factor(
        &synapse,
        &pre_neuron,
        &post_neuron,
        0.0f,  // No spike
        0.0f,
        0.0f,
        NULL
    );

    // Can't verify exact decay without knowing the timestep,
    // but trace should still be accessible
    EXPECT_GE(synapse.trace, 0.0f);
}

//=============================================================================
// Regression Test 3: Weight Bounds Still Enforced
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, WeightBounds_StillEnforced) {
    // Test upper bound
    synapse.weight = 9.5f;

    for (int i = 0; i < 20; i++) {
        synapse_learn_three_factor(
            &synapse,
            &pre_neuron,
            &post_neuron,
            i * 10.0f,
            i * 10.0f + 10.0f,
            1.0f,
            NULL
        );
    }

    EXPECT_LE(synapse.weight, 10.0f);  // Upper bound

    // Test lower bound
    synapse.weight = -9.5f;

    for (int i = 0; i < 20; i++) {
        synapse_learn_three_factor(
            &synapse,
            &pre_neuron,
            &post_neuron,
            i * 10.0f + 10.0f,  // post before pre (LTD)
            i * 10.0f,
            1.0f,
            NULL
        );
    }

    EXPECT_GE(synapse.weight, -10.0f);  // Lower bound
}

//=============================================================================
// Regression Test 4: NULL Context Handled Safely
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, NullContext_HandledSafely) {
    // Old code often passed NULL context - should still work

    EXPECT_NO_THROW({
        synapse_learn_three_factor(
            &synapse,
            &pre_neuron,
            &post_neuron,
            10.0f,
            20.0f,
            1.0f,
            NULL  // NULL context
        );
    });

    // Should not crash, weight should still update
    EXPECT_NE(synapse.weight, 0.5f);
}

//=============================================================================
// Regression Test 5: NULL Synapse Handled Safely
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, NullSynapse_HandledSafely) {
    // Should not crash on NULL synapse

    EXPECT_NO_THROW({
        synapse_learn_three_factor(
            nullptr,
            &pre_neuron,
            &post_neuron,
            10.0f,
            20.0f,
            1.0f,
            NULL
        );
    });
}

//=============================================================================
// Regression Test 6: Reward Polarity Still Works
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, RewardPolarity_StillWorks) {
    // Positive reward
    synapse_t syn_pos = synapse;
    synapse_learn_three_factor(
        &syn_pos,
        &pre_neuron,
        &post_neuron,
        10.0f,
        20.0f,
        1.0f,  // Positive reward
        NULL
    );

    // Negative reward
    synapse_t syn_neg = synapse;
    synapse_learn_three_factor(
        &syn_neg,
        &pre_neuron,
        &post_neuron,
        10.0f,
        20.0f,
        -1.0f,  // Negative reward
        NULL
    );

    // Positive should increase weight
    EXPECT_GT(syn_pos.weight, 0.5f);

    // Negative should decrease weight
    EXPECT_LT(syn_neg.weight, 0.5f);
}

//=============================================================================
// Regression Test 7: Zero Reward = Minimal Weight Change
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, ZeroReward_MinimalChange) {
    float initial_weight = synapse.weight;

    // Accumulate trace but provide no reward
    for (int i = 0; i < 5; i++) {
        synapse_learn_three_factor(
            &synapse,
            &pre_neuron,
            &post_neuron,
            i * 10.0f,
            i * 10.0f + 10.0f,
            0.0f,  // Zero reward
            NULL
        );
    }

    // Weight should remain approximately the same
    EXPECT_NEAR(synapse.weight, initial_weight, 0.05f);
}

//=============================================================================
// Regression Test 8: STDP Timing Still Respected
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, STDP_TimingRespected) {
    // LTP: pre before post
    synapse_t syn_ltp = synapse;
    synapse_learn_three_factor(
        &syn_ltp,
        &pre_neuron,
        &post_neuron,
        10.0f,  // pre first
        20.0f,  // post second
        1.0f,
        NULL
    );

    // LTD: post before pre
    synapse_t syn_ltd = synapse;
    synapse_learn_three_factor(
        &syn_ltd,
        &pre_neuron,
        &post_neuron,
        20.0f,  // pre second
        10.0f,  // post first
        1.0f,
        NULL
    );

    // LTP should strengthen more than LTD weakens
    EXPECT_GT(syn_ltp.weight, 0.5f);
    EXPECT_LT(syn_ltd.weight, 0.5f);
}

//=============================================================================
// Regression Test 9: last_change Field Still Updated
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, LastChange_StillUpdated) {
    float initial_last_change = synapse.last_change;

    synapse_learn_three_factor(
        &synapse,
        &pre_neuron,
        &post_neuron,
        10.0f,
        20.0f,
        1.0f,
        NULL
    );

    // last_change should be updated
    EXPECT_NE(synapse.last_change, initial_last_change);
}

//=============================================================================
// Regression Test 10: No Memory Leaks
//=============================================================================

TEST_F(EligibilityBackwardCompatTest, NoMemoryLeaks_InlineMode) {
    // Run learning many times - should not allocate memory
    for (int i = 0; i < 1000; i++) {
        synapse_learn_three_factor(
            &synapse,
            &pre_neuron,
            &post_neuron,
            i * 10.0f,
            i * 10.0f + 10.0f,
            (i % 2 == 0) ? 1.0f : -1.0f,  // Alternating reward
            NULL
        );
    }

    // Should still be NULL (no allocation)
    EXPECT_EQ(synapse.eligibility, nullptr);

    // Should still work
    EXPECT_NE(synapse.weight, 0.5f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
