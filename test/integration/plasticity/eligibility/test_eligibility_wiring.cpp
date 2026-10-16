/**
 * @file test_eligibility_wiring.cpp
 * @brief Integration tests for eligibility trace wiring into cognitive pipeline
 *
 * WHAT: Tests that eligibility traces actually work in the brain/learning loop
 * WHY:  Unit tests validate API, integration tests validate end-to-end functionality
 * HOW:  Create synapses, trigger learning, verify traces accumulated and consolidated
 *
 * @version Option 2.2 Integration
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/synapse_compute/nimcp_synapse_compute.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EligibilityWiringTest : public ::testing::Test {
protected:
    synapse_t synapse_inline;    // Uses inline trace
    synapse_t synapse_full_api;  // Uses full eligibility trace API
    neuron_t pre_neuron;
    neuron_t post_neuron;

    void SetUp() override {
        // Initialize inline trace synapse (backward compatible mode)
        memset(&synapse_inline, 0, sizeof(synapse_t));
        synapse_inline.weight = 0.5f;
        synapse_inline.target_id = 1;
        synapse_inline.plasticity = 1.0f;
        synapse_inline.strength = 0.5f;
        synapse_inline.trace = 0.0f;  // Inline trace
        synapse_inline.eligibility = NULL;  // No full API
        synapse_inline.enable_eligibility = false;

        // Initialize full API synapse (Option 2.2 mode)
        memset(&synapse_full_api, 0, sizeof(synapse_t));
        synapse_full_api.weight = 0.5f;
        synapse_full_api.target_id = 1;
        synapse_full_api.plasticity = 1.0f;
        synapse_full_api.strength = 0.5f;
        synapse_full_api.trace = 0.0f;  // Not used in full API mode

        // Allocate eligibility trace
        synapse_full_api.eligibility = (eligibility_trace_t*)calloc(1, sizeof(eligibility_trace_t));
        synapse_full_api.enable_eligibility = true;
        eligibility_trace_init(synapse_full_api.eligibility, 0);

        // Initialize neurons (minimal setup)
        memset(&pre_neuron, 0, sizeof(neuron_t));
        memset(&post_neuron, 0, sizeof(neuron_t));
        pre_neuron.state = 1.0f;
        post_neuron.state = 0.5f;
    }

    void TearDown() override {
        if (synapse_full_api.eligibility) {
            free(synapse_full_api.eligibility);
            synapse_full_api.eligibility = NULL;
        }
    }
};

//=============================================================================
// Integration Test 1: Inline Trace Mode (Backward Compatible)
//=============================================================================

TEST_F(EligibilityWiringTest, InlineMode_BackwardCompatible) {
    float initial_weight = synapse_inline.weight;
    float initial_inline_trace = synapse_inline.trace;

    // Trigger learning with reward
    synapse_learn_three_factor(
        &synapse_inline,
        &pre_neuron,
        &post_neuron,
        10.0f,   // pre_spike_time
        20.0f,   // post_spike_time (LTP timing)
        1.0f,    // reward_signal
        NULL     // context
    );

    // Verify inline trace was updated
    EXPECT_GT(synapse_inline.trace, initial_inline_trace);

    // Verify weight changed due to reward
    EXPECT_NE(synapse_inline.weight, initial_weight);

    // Verify eligibility trace NOT used (should still be NULL)
    EXPECT_EQ(synapse_inline.eligibility, nullptr);
}

//=============================================================================
// Integration Test 2: Full API Mode Activation
//=============================================================================

TEST_F(EligibilityWiringTest, FullAPIMode_Activated) {
    ASSERT_NE(synapse_full_api.eligibility, nullptr);
    ASSERT_TRUE(synapse_full_api.enable_eligibility);

    float initial_weight = synapse_full_api.weight;
    float initial_trace = eligibility_get_trace(synapse_full_api.eligibility);

    // Trigger learning with reward
    synapse_learn_three_factor(
        &synapse_full_api,
        &pre_neuron,
        &post_neuron,
        10.0f,   // pre_spike_time
        20.0f,   // post_spike_time (LTP timing)
        1.0f,    // reward_signal
        NULL  // Context not needed - eligibility enabled via synapse flag
    );

    // Verify eligibility trace was updated
    float updated_trace = eligibility_get_trace(synapse_full_api.eligibility);
    EXPECT_GT(updated_trace, initial_trace);

    // Verify weight changed
    EXPECT_NE(synapse_full_api.weight, initial_weight);
}

//=============================================================================
// Integration Test 3: Trace Accumulation Over Multiple Spikes
//=============================================================================

TEST_F(EligibilityWiringTest, TraceAccumulation_MultipleSpikes) {
    float initial_trace = eligibility_get_trace(synapse_full_api.eligibility);

    // Multiple spike pairs
    for (int i = 0; i < 5; i++) {
        float t = i * 50.0f;
        synapse_learn_three_factor(
            &synapse_full_api,
            &pre_neuron,
            &post_neuron,
            t,       // pre_spike_time
            t + 10,  // post_spike_time (LTP)
            0.0f,    // No reward yet
            NULL
        );
    }

    // Trace should accumulate
    float accumulated_trace = eligibility_get_trace(synapse_full_api.eligibility);
    EXPECT_GT(accumulated_trace, initial_trace);

    // Weight shouldn't change much without reward
    EXPECT_NEAR(synapse_full_api.weight, 0.5f, 0.1f);
}

//=============================================================================
// Integration Test 4: Reward-Based Consolidation
//=============================================================================

TEST_F(EligibilityWiringTest, RewardConsolidation_WeightChange) {
    // Accumulate trace
    synapse_learn_three_factor(
        &synapse_full_api,
        &pre_neuron,
        &post_neuron,
        10.0f,
        20.0f,
        0.0f,    // No reward
        NULL
    );

    float trace_after_spike = eligibility_get_trace(synapse_full_api.eligibility);
    EXPECT_GT(trace_after_spike, 0.0f);

    float weight_before_reward = synapse_full_api.weight;

    // Now provide reward
    synapse_learn_three_factor(
        &synapse_full_api,
        &pre_neuron,
        &post_neuron,
        0.0f,    // No new spike
        0.0f,
        1.0f,    // Strong reward
        NULL
    );

    // Weight should change significantly with reward
    EXPECT_NE(synapse_full_api.weight, weight_before_reward);
    EXPECT_GT(synapse_full_api.weight, weight_before_reward);  // Positive reward → LTP
}

//=============================================================================
// Integration Test 5: LTP vs LTD Based on Spike Timing
//=============================================================================

TEST_F(EligibilityWiringTest, SpikeTimingDependence_LTPLTD) {
    // LTP condition: pre before post
    synapse_t syn_ltp = synapse_full_api;
    syn_ltp.eligibility = (eligibility_trace_t*)calloc(1, sizeof(eligibility_trace_t));
    syn_ltp.enable_eligibility = true;
    eligibility_trace_init(syn_ltp.eligibility, 0);
    syn_ltp.weight = 0.5f;

    synapse_learn_three_factor(
        &syn_ltp,
        &pre_neuron,
        &post_neuron,
        10.0f,   // pre before
        20.0f,   // post after
        1.0f,    // reward
        NULL
    );

    float weight_ltp = syn_ltp.weight;

    // LTD condition: post before pre
    synapse_t syn_ltd = synapse_full_api;
    syn_ltd.eligibility = (eligibility_trace_t*)calloc(1, sizeof(eligibility_trace_t));
    syn_ltd.enable_eligibility = true;
    eligibility_trace_init(syn_ltd.eligibility, 0);
    syn_ltd.weight = 0.5f;

    synapse_learn_three_factor(
        &syn_ltd,
        &pre_neuron,
        &post_neuron,
        20.0f,   // pre after
        10.0f,   // post before
        1.0f,    // reward
        NULL
    );

    float weight_ltd = syn_ltd.weight;

    // LTP should increase weight more than LTD
    EXPECT_GT(weight_ltp, 0.5f);   // LTP → weight increase
    EXPECT_LT(weight_ltd, 0.5f);   // LTD → weight decrease

    // Cleanup
    free(syn_ltp.eligibility);
    free(syn_ltd.eligibility);
}

//=============================================================================
// Integration Test 6: Trace Decay Over Time
//=============================================================================

TEST_F(EligibilityWiringTest, TraceDecay_Temporal) {
    // Set initial trace
    synapse_learn_three_factor(
        &synapse_full_api,
        &pre_neuron,
        &post_neuron,
        10.0f,
        20.0f,
        0.0f,
        NULL
    );

    float trace_initial = eligibility_get_trace(synapse_full_api.eligibility);
    EXPECT_GT(trace_initial, 0.0f);

    // Simulate time passing without new spikes (just decay)
    for (int i = 0; i < 10; i++) {
        synapse_learn_three_factor(
            &synapse_full_api,
            &pre_neuron,
            &post_neuron,
            0.0f,    // No spike
            0.0f,
            0.0f,    // No reward
            NULL
        );
    }

    float trace_decayed = eligibility_get_trace(synapse_full_api.eligibility);

    // Trace should decay
    EXPECT_LT(trace_decayed, trace_initial);
}

//=============================================================================
// Integration Test 7: Weight Bounds Enforcement
//=============================================================================

TEST_F(EligibilityWiringTest, WeightBounds_Clamping) {
    synapse_full_api.weight = 9.5f;  // Near upper bound

    // Large reward signal
    for (int i = 0; i < 10; i++) {
        synapse_learn_three_factor(
            &synapse_full_api,
            &pre_neuron,
            &post_neuron,
            i * 10.0f,
            i * 10.0f + 10.0f,
            1.0f,    // Strong reward
            NULL
        );
    }

    // Weight should be clamped at 10.0
    EXPECT_LE(synapse_full_api.weight, 10.0f);
    EXPECT_GE(synapse_full_api.weight, 9.5f);  // Should have increased
}

//=============================================================================
// Integration Test 8: Zero Reward = No Consolidation
//=============================================================================

TEST_F(EligibilityWiringTest, ZeroReward_NoConsolidation) {
    // Accumulate trace
    synapse_learn_three_factor(
        &synapse_full_api,
        &pre_neuron,
        &post_neuron,
        10.0f,
        20.0f,
        0.0f,
        NULL
    );

    float weight_before = synapse_full_api.weight;

    // Multiple updates with zero reward
    for (int i = 0; i < 5; i++) {
        synapse_learn_three_factor(
            &synapse_full_api,
            &pre_neuron,
            &post_neuron,
            0.0f,
            0.0f,
            0.0f,   // Zero reward
            NULL
        );
    }

    // Weight should remain approximately the same
    EXPECT_NEAR(synapse_full_api.weight, weight_before, 0.01f);
}

//=============================================================================
// Integration Test 9: Mode Selection Consistency
//=============================================================================

TEST_F(EligibilityWiringTest, ModeSelection_Automatic) {
    // Test that synapses correctly choose their mode

    // Inline mode synapse
    synapse_learn_three_factor(
        &synapse_inline,
        &pre_neuron,
        &post_neuron,
        10.0f, 20.0f, 1.0f,
        NULL
    );

    // Should use inline trace
    EXPECT_EQ(synapse_inline.eligibility, nullptr);
    EXPECT_GT(synapse_inline.trace, 0.0f);  // Inline trace updated

    // Full API mode synapse
    synapse_learn_three_factor(
        &synapse_full_api,
        &pre_neuron,
        &post_neuron,
        10.0f, 20.0f, 1.0f,
        NULL
    );

    // Should use full API
    EXPECT_NE(synapse_full_api.eligibility, nullptr);
    EXPECT_GT(eligibility_get_trace(synapse_full_api.eligibility), 0.0f);
}

//=============================================================================
// Integration Test 10: Negative Reward = Weight Decrease
//=============================================================================

TEST_F(EligibilityWiringTest, NegativeReward_WeightDecrease) {
    float initial_weight = synapse_full_api.weight;

    // Accumulate trace
    synapse_learn_three_factor(
        &synapse_full_api,
        &pre_neuron,
        &post_neuron,
        10.0f,
        20.0f,
        0.0f,
        NULL
    );

    // Apply negative reward (punishment)
    synapse_learn_three_factor(
        &synapse_full_api,
        &pre_neuron,
        &post_neuron,
        0.0f,
        0.0f,
        -1.0f,   // Negative reward
        NULL
    );

    // Weight should decrease
    EXPECT_LT(synapse_full_api.weight, initial_weight);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
