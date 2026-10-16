//=============================================================================
// test_three_factor_learning.cpp - Unit Tests for Three-Factor Learning
//=============================================================================
/**
 * @file test_three_factor_learning.cpp
 * @brief Unit tests for three-factor learning with dopamine modulation
 *
 * WHAT: Tests synapse_learn_three_factor() with different dopamine levels
 * WHY:  Verify three-factor rule: Δw = learning_rate × trace × reward × dopamine
 * HOW:  Isolated tests with controlled dopamine levels and reward signals
 *
 * TEST CATEGORIES:
 * 1. Dopamine modulation: High DA amplifies learning, low DA suppresses it
 * 2. Three-factor integration: Trace × reward × dopamine interaction
 * 3. Burst-triggered consolidation: Learning only during dopamine bursts
 * 4. Backward compatibility: Inline trace mode still works
 *
 * @author NIMCP Test Suite
 * @date 2025-11-16
 * @version 2.7.1
 */

#include <gtest/gtest.h>
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/synapse_compute/nimcp_synapse_compute.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"
#include <cmath>

//=============================================================================
// Test Fixtures
//=============================================================================

class ThreeFactorLearningTest : public ::testing::Test {
protected:
    synapse_t* synapse;
    neuron_t* pre_neuron;
    neuron_t* post_neuron;
    synapse_compute_context_t context;
    neuromodulator_system_t neuromod_system;

    void SetUp() override {
        // Create test synapse
        synapse = (synapse_t*)calloc(1, sizeof(synapse_t));
        synapse->weight = 0.5f;
        synapse->trace = 0.0f;
        synapse->last_change = 0.0f;

        // Create test neurons
        pre_neuron = (neuron_t*)calloc(1, sizeof(neuron_t));
        post_neuron = (neuron_t*)calloc(1, sizeof(neuron_t));

        // Initialize context
        memset(&context, 0, sizeof(context));
        context.current_time = 0;

        // Create neuromodulator system
        neuromod_system = neuromodulator_system_create(nullptr);
        context.neuromodulator_system = neuromod_system;
    }

    void TearDown() override {
        if (synapse->eligibility) {
            free(synapse->eligibility);
        }
        free(synapse);
        free(pre_neuron);
        free(post_neuron);
        if (neuromod_system) {
            neuromodulator_system_destroy(neuromod_system);
        }
    }

    // Helper: Set dopamine level
    void set_dopamine_level(float level) {
        if (neuromod_system) {
            neuromodulator_set_level(neuromod_system, NEUROMOD_DOPAMINE, level);
        }
    }

    // Helper: Enable eligibility trace
    void enable_eligibility_trace() {
        synapse->eligibility = (eligibility_trace_t*)calloc(1, sizeof(eligibility_trace_t));
        eligibility_trace_init(synapse->eligibility, 0);
        synapse->enable_eligibility = true;
    }
};

//=============================================================================
// Basic Three-Factor Learning Tests
//=============================================================================

TEST_F(ThreeFactorLearningTest, HighDopamineAmplifiesLearning) {
    // WHAT: High dopamine should amplify weight changes
    // WHY:  Three-factor rule: Δw ∝ dopamine
    // EXPECT: Weight change with high DA > weight change with low DA

    enable_eligibility_trace();
    float initial_weight = synapse->weight;

    // Set high dopamine
    set_dopamine_level(1.0f);

    // Apply learning with positive reward
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f,  // pre_spike_time
        15.0f,  // post_spike_time
        1.0f,   // reward_signal (positive)
        &context
    );

    float weight_change_high_da = synapse->weight - initial_weight;

    // Reset synapse
    synapse->weight = initial_weight;
    eligibility_trace_init(synapse->eligibility, 0);

    // Set low dopamine
    set_dopamine_level(0.1f);

    // Apply same learning
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, &context
    );

    float weight_change_low_da = synapse->weight - initial_weight;

    // High dopamine should produce larger weight change
    EXPECT_GT(std::abs(weight_change_high_da), std::abs(weight_change_low_da));
}

TEST_F(ThreeFactorLearningTest, ZeroDopamineSuppressesLearning) {
    // WHAT: Zero dopamine should prevent learning
    // WHY:  Three-factor rule requires DA > 0 for learning
    // EXPECT: No weight change with DA = 0

    enable_eligibility_trace();
    float initial_weight = synapse->weight;

    // Set zero dopamine
    set_dopamine_level(0.0f);

    // Apply learning with positive reward
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, &context
    );

    // Weight should not change significantly
    EXPECT_NEAR(synapse->weight, initial_weight, 0.01f);
}

TEST_F(ThreeFactorLearningTest, RewardSignModulatesDirection) {
    // WHAT: Reward sign determines weight change direction
    // WHY:  Positive reward → LTP, negative reward → LTD
    // EXPECT: Opposite weight changes for +/- reward

    enable_eligibility_trace();
    set_dopamine_level(0.8f);

    float initial_weight = synapse->weight;

    // Positive reward
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, &context
    );

    float weight_positive_reward = synapse->weight;

    // Reset
    synapse->weight = initial_weight;
    eligibility_trace_init(synapse->eligibility, 0);

    // Negative reward
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, -1.0f, &context
    );

    float weight_negative_reward = synapse->weight;

    // Changes should be opposite in sign
    float change_pos = weight_positive_reward - initial_weight;
    float change_neg = weight_negative_reward - initial_weight;
    EXPECT_LT(change_pos * change_neg, 0.0f);  // Opposite signs (product should be negative)
}

TEST_F(ThreeFactorLearningTest, STDPTimingMatters) {
    // WHAT: Pre-post spike timing affects trace strength
    // WHY:  STDP creates temporal window for learning
    // EXPECT: Close spikes → stronger trace → larger weight change

    enable_eligibility_trace();
    set_dopamine_level(0.8f);

    float initial_weight = synapse->weight;

    // Close spikes (Δt = 5ms)
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, &context
    );

    float weight_close_spikes = synapse->weight;

    // Reset
    synapse->weight = initial_weight;
    eligibility_trace_init(synapse->eligibility, 0);

    // Distant spikes (Δt = 100ms)
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 110.0f, 1.0f, &context
    );

    float weight_distant_spikes = synapse->weight;

    // Close spikes should produce larger change
    float change_close = std::abs(weight_close_spikes - initial_weight);
    float change_distant = std::abs(weight_distant_spikes - initial_weight);
    EXPECT_GT(change_close, change_distant);
}

//=============================================================================
// Eligibility Trace Integration Tests
//=============================================================================

TEST_F(ThreeFactorLearningTest, TraceDecaysOverTime) {
    // WHAT: Eligibility trace should decay exponentially
    // WHY:  Implements temporal credit assignment window
    // EXPECT: Trace strength decreases with time

    enable_eligibility_trace();
    set_dopamine_level(0.8f);

    // Build up trace with spike
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 0.0f,  // No reward, just build trace
        &context
    );

    float trace_initial = synapse->eligibility->trace;

    // Advance time and decay
    context.current_time = 100;
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        0.0f, 0.0f, 0.0f,  // No spikes, just decay
        &context
    );

    float trace_after_decay = synapse->eligibility->trace;

    // Trace should have decayed
    EXPECT_LT(trace_after_decay, trace_initial);
}

TEST_F(ThreeFactorLearningTest, DelayedRewardWorks) {
    // WHAT: Reward can be delivered after spike
    // WHY:  Eligibility trace bridges temporal gap
    // EXPECT: Weight changes even with delayed reward

    enable_eligibility_trace();
    set_dopamine_level(0.8f);

    float initial_weight = synapse->weight;

    // Spike without reward (build trace)
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 0.0f, &context
    );

    // Check trace exists
    EXPECT_GT(synapse->eligibility->trace, 0.0f);

    // Deliver reward later (within trace window)
    context.current_time = 50;
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        0.0f, 0.0f, 1.0f,  // Reward without spike
        &context
    );

    // Weight should have changed
    EXPECT_NE(synapse->weight, initial_weight);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(ThreeFactorLearningTest, InlineTraceMode_StillWorks) {
    // WHAT: Synapse without eligibility allocation uses inline trace
    // WHY:  Backward compatibility with simple mode
    // EXPECT: Learning works without eligibility_trace_t allocation

    // Do NOT enable eligibility trace
    // synapse->eligibility = NULL;

    set_dopamine_level(0.8f);
    float initial_weight = synapse->weight;

    // Apply learning
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, &context
    );

    // Weight should change (using inline trace)
    EXPECT_NE(synapse->weight, initial_weight);
}

TEST_F(ThreeFactorLearningTest, NoContext_FallsBack) {
    // WHAT: Learning without context should still work
    // WHY:  Graceful degradation
    // EXPECT: Uses default dopamine level

    enable_eligibility_trace();
    float initial_weight = synapse->weight;

    // Learn without context (NULL)
    synapse_learn_three_factor(
        synapse, pre_neuron, post_neuron,
        10.0f, 15.0f, 1.0f, nullptr
    );

    // Weight should change (using defaults)
    EXPECT_NE(synapse->weight, initial_weight);
}

//=============================================================================
// Dopamine Range Tests
//=============================================================================

TEST_F(ThreeFactorLearningTest, DopamineLevels_0_to_1) {
    // WHAT: Test learning across dopamine range [0, 1]
    // WHY:  Verify monotonic relationship
    // EXPECT: Higher DA → larger weight change

    enable_eligibility_trace();

    float dopamine_levels[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    float weight_changes[5];

    for (int i = 0; i < 5; i++) {
        float initial_weight = 0.5f;
        synapse->weight = initial_weight;
        eligibility_trace_init(synapse->eligibility, 0);

        set_dopamine_level(dopamine_levels[i]);

        synapse_learn_three_factor(
            synapse, pre_neuron, post_neuron,
            10.0f, 15.0f, 1.0f, &context
        );

        weight_changes[i] = std::abs(synapse->weight - initial_weight);
    }

    // Verify monotonic increase
    for (int i = 1; i < 5; i++) {
        EXPECT_GE(weight_changes[i], weight_changes[i-1]);
    }
}

//=============================================================================
// Weight Bounds Tests
//=============================================================================

TEST_F(ThreeFactorLearningTest, WeightBounds_Respected) {
    // WHAT: Weights should stay within [-10, 10]
    // WHY:  Prevent numerical overflow
    // EXPECT: Clamping at boundaries

    enable_eligibility_trace();
    set_dopamine_level(1.0f);

    // Start near upper bound
    synapse->weight = 9.5f;

    // Apply strong positive learning
    for (int i = 0; i < 100; i++) {
        synapse_learn_three_factor(
            synapse, pre_neuron, post_neuron,
            10.0f, 15.0f, 1.0f, &context
        );
    }

    // Weight should not exceed 10
    EXPECT_LE(synapse->weight, 10.0f);

    // Start near lower bound
    synapse->weight = -9.5f;
    eligibility_trace_init(synapse->eligibility, 0);

    // Apply strong negative learning
    for (int i = 0; i < 100; i++) {
        synapse_learn_three_factor(
            synapse, pre_neuron, post_neuron,
            10.0f, 15.0f, -1.0f, &context
        );
    }

    // Weight should not go below -10
    EXPECT_GE(synapse->weight, -10.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
