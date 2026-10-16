/**
 * @file test_eligibility_burst.cpp
 * @brief Unit tests for burst-triggered eligibility trace consolidation (Option 2.2)
 *
 * WHAT: Tests for "tags and capture" mechanism
 * WHY:  Validate that traces accumulate and only consolidate during dopamine bursts
 * HOW:  Test burst detection, consolidation gating, batch processing
 *
 * @version Option 2.2
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

    #include "plasticity/eligibility/nimcp_eligibility_trace.h"
    #include "plasticity/neuromodulators/nimcp_phasic_tonic.h"
    #include "core/neuralnet/nimcp_neuralnet.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EligibilityBurstTest : public ::testing::Test {
protected:
    eligibility_config_t config;
    eligibility_config_t burst_config;
    eligibility_trace_t trace;
    phasic_tonic_state_t phasic_tonic = {};
    synapse_t synapse;

    void SetUp() override {
        // Standard config
        config = eligibility_default_config();

        // Burst-triggered config
        burst_config = eligibility_default_config();
        burst_config.burst_triggered_mode = true;
        burst_config.burst_lr_multiplier = 3.0f;
        burst_config.min_burst_concentration = 0.3f;

        // Initialize trace
        eligibility_trace_init(&trace, 0);

        // Initialize phasic-tonic state (dopamine)
        phasic_tonic_config_t pt_config = phasic_tonic_config_dopamine_default();
        phasic_tonic_init(&phasic_tonic, &pt_config, 0);

        // Initialize synapse
        synapse.weight = 0.5f;
        synapse.target_id = 1;
        synapse.plasticity = 1.0f;
        synapse.strength = 0.5f;
    }

    void TearDown() override {
        // No dynamic allocation, nothing to clean up
    }
};

//=============================================================================
// Test 1: Burst Detection via Flag
//=============================================================================

TEST_F(EligibilityBurstTest, BurstDetection_Flag) {
    // Baseline: not in burst
    EXPECT_FALSE(eligibility_is_in_burst(&phasic_tonic, &burst_config));

    // Trigger burst
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 0, 0);

    // Should detect burst
    EXPECT_TRUE(eligibility_is_in_burst(&phasic_tonic, &burst_config));
    EXPECT_TRUE(phasic_tonic.in_burst_state);
}

//=============================================================================
// Test 2: Burst Detection via Concentration Threshold
//=============================================================================

TEST_F(EligibilityBurstTest, BurstDetection_Threshold) {
    // Manually set high concentration without setting burst flag
    phasic_tonic.in_burst_state = false;
    phasic_tonic_set_total_concentration(&phasic_tonic, 0.5f);  // Above threshold (0.3)

    // Should still detect as burst via concentration
    EXPECT_TRUE(eligibility_is_in_burst(&phasic_tonic, &burst_config));
}

//=============================================================================
// Test 3: Standard Mode - Always Consolidates
//=============================================================================

TEST_F(EligibilityBurstTest, StandardMode_AlwaysConsolidates) {
    // Set up trace with significant value
    trace.trace = 0.8f;
    float initial_weight = synapse.weight;

    // Standard mode (burst_triggered_mode = false)
    // Should consolidate even without burst
    EXPECT_FALSE(phasic_tonic.in_burst_state);

    float delta_w = eligibility_consolidate_on_burst(
        &synapse, &trace, &config, &phasic_tonic, 1.0f  // reward = 1.0
    );

    // Expect weight change
    EXPECT_GT(fabs(delta_w), 0.0f);
    EXPECT_NE(synapse.weight, initial_weight);
}

//=============================================================================
// Test 4: Burst-Triggered Mode - No Consolidation Without Burst
//=============================================================================

TEST_F(EligibilityBurstTest, BurstMode_NoConsolidationWithoutBurst) {
    // Set up trace with significant value
    trace.trace = 0.8f;
    float initial_weight = synapse.weight;

    // Burst-triggered mode, but NO burst
    EXPECT_FALSE(phasic_tonic.in_burst_state);

    float delta_w = eligibility_consolidate_on_burst(
        &synapse, &trace, &burst_config, &phasic_tonic, 1.0f
    );

    // Expect NO weight change (traces remain as "tags")
    EXPECT_EQ(delta_w, 0.0f);
    EXPECT_EQ(synapse.weight, initial_weight);
}

//=============================================================================
// Test 5: Burst-Triggered Mode - Consolidation During Burst
//=============================================================================

TEST_F(EligibilityBurstTest, BurstMode_ConsolidationDuringBurst) {
    // Set up trace with significant value
    trace.trace = 0.8f;
    float initial_weight = synapse.weight;

    // Trigger dopamine burst
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 0, 0);
    EXPECT_TRUE(phasic_tonic.in_burst_state);

    float delta_w = eligibility_consolidate_on_burst(
        &synapse, &trace, &burst_config, &phasic_tonic, 1.0f
    );

    // Expect weight change with burst amplification
    EXPECT_GT(delta_w, 0.0f);
    EXPECT_GT(synapse.weight, initial_weight);
}

//=============================================================================
// Test 6: Burst Amplification - 3x Learning Rate
//=============================================================================

TEST_F(EligibilityBurstTest, BurstAmplification_ThreeTimes) {
    trace.trace = 0.5f;

    // Standard mode weight change
    float standard_delta_w = eligibility_consolidate_on_burst(
        &synapse, &trace, &config, &phasic_tonic, 1.0f
    );

    // Reset synapse
    synapse.weight = 0.5f;

    // Burst mode with dopamine burst
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 0, 0);
    float burst_delta_w = eligibility_consolidate_on_burst(
        &synapse, &trace, &burst_config, &phasic_tonic, 1.0f
    );

    // Burst should produce ~3x larger weight change
    // (burst_lr_multiplier = 3.0)
    EXPECT_GT(burst_delta_w, standard_delta_w * 2.0f);  // At least 2x
}

//=============================================================================
// Test 7: Trace Accumulation Without Consolidation
//=============================================================================

TEST_F(EligibilityBurstTest, TraceAccumulation_WithoutConsolidation) {
    float initial_weight = synapse.weight;

    // Accumulate traces (multiple spikes)
    eligibility_trace_update(&trace, &burst_config, 0, 1.0f);    // t=0: spike
    eligibility_trace_update(&trace, &burst_config, 10, 1.0f);   // t=10: spike
    eligibility_trace_update(&trace, &burst_config, 20, 1.0f);   // t=20: spike

    // Trace should be high
    EXPECT_GT(trace.trace, 0.5f);

    // Try to consolidate WITHOUT burst
    float delta_w = eligibility_consolidate_on_burst(
        &synapse, &trace, &burst_config, &phasic_tonic, 1.0f
    );

    // No consolidation - traces remain as "tags"
    EXPECT_EQ(delta_w, 0.0f);
    EXPECT_EQ(synapse.weight, initial_weight);

    // Now trigger burst and consolidate
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 0, 0);
    delta_w = eligibility_consolidate_on_burst(
        &synapse, &trace, &burst_config, &phasic_tonic, 1.0f
    );

    // NOW consolidation happens
    EXPECT_GT(delta_w, 0.0f);
    EXPECT_GT(synapse.weight, initial_weight);
}

//=============================================================================
// Test 8: Batch Consolidation
//=============================================================================

TEST_F(EligibilityBurstTest, BatchConsolidation_MultipleSynapses) {
    const int num_synapses = 10;
    synapse_t synapses[num_synapses];
    eligibility_trace_t traces[num_synapses];

    // Initialize synapses and traces
    for (int i = 0; i < num_synapses; i++) {
        synapses[i].weight = 0.5f;
        synapses[i].target_id = i + 1;
        synapses[i].plasticity = 1.0f;
        synapses[i].strength = 0.5f;

        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f + (i * 0.05f);  // Varying traces
    }

    // Batch consolidate WITHOUT burst
    int num_consolidated = eligibility_consolidate_batch(
        synapses, traces, num_synapses,
        &burst_config, &phasic_tonic, 1.0f
    );

    // No consolidation
    EXPECT_EQ(num_consolidated, 0);

    // Trigger burst
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 0, 0);

    // Batch consolidate WITH burst
    num_consolidated = eligibility_consolidate_batch(
        synapses, traces, num_synapses,
        &burst_config, &phasic_tonic, 1.0f
    );

    // All synapses should be consolidated
    EXPECT_EQ(num_consolidated, num_synapses);

    // Verify all weights changed
    for (int i = 0; i < num_synapses; i++) {
        EXPECT_NE(synapses[i].weight, 0.5f);
    }
}

//=============================================================================
// Test 9: Batch Consolidation Performance
//=============================================================================

TEST_F(EligibilityBurstTest, BatchConsolidation_Performance) {
    const int num_synapses = 1000;
    synapse_t* synapses = new synapse_t[num_synapses];
    eligibility_trace_t* traces = new eligibility_trace_t[num_synapses];

    // Initialize
    for (int i = 0; i < num_synapses; i++) {
        synapses[i].weight = 0.5f;
        synapses[i].target_id = i + 1;
        synapses[i].plasticity = 1.0f;
        synapses[i].strength = 0.5f;

        eligibility_trace_init(&traces[i], 0);
        traces[i].trace = 0.5f;
    }

    // Trigger burst
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 0, 0);

    // Time batch consolidation
    auto start = std::chrono::high_resolution_clock::now();

    int num_consolidated = eligibility_consolidate_batch(
        synapses, traces, num_synapses,
        &burst_config, &phasic_tonic, 1.0f
    );

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Batch consolidation: " << num_synapses << " synapses in "
              << duration << " µs" << std::endl;

    // Expect reasonable performance
    EXPECT_LT(duration, 10000);  // Less than 10ms for 1000 synapses

    // All synapses consolidated
    EXPECT_EQ(num_consolidated, num_synapses);

    delete[] synapses;
    delete[] traces;
}

//=============================================================================
// Test 10: Reward Polarity - Positive vs Negative
//=============================================================================

TEST_F(EligibilityBurstTest, RewardPolarity_PositiveAndNegative) {
    trace.trace = 0.8f;

    // Trigger burst
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 0, 0);

    // Positive reward
    synapse_t synapse_pos = synapse;
    float delta_w_pos = eligibility_consolidate_on_burst(
        &synapse_pos, &trace, &burst_config, &phasic_tonic, 1.0f
    );

    // Negative reward
    synapse_t synapse_neg = synapse;
    float delta_w_neg = eligibility_consolidate_on_burst(
        &synapse_neg, &trace, &burst_config, &phasic_tonic, -1.0f
    );

    // Positive reward should increase weight
    EXPECT_GT(delta_w_pos, 0.0f);
    EXPECT_GT(synapse_pos.weight, synapse.weight);

    // Negative reward should decrease weight
    EXPECT_LT(delta_w_neg, 0.0f);
    EXPECT_LT(synapse_neg.weight, synapse.weight);

    // Magnitudes should be equal
    EXPECT_NEAR(fabs(delta_w_pos), fabs(delta_w_neg), 1e-6f);
}

//=============================================================================
// Test 11: Trace Threshold - Skip Negligible Traces
//=============================================================================

TEST_F(EligibilityBurstTest, TraceThreshold_SkipNegligible) {
    // Set trace below threshold
    trace.trace = 0.005f;  // threshold = 0.01
    float initial_weight = synapse.weight;

    // Trigger burst
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 0, 0);

    // Try to consolidate
    float delta_w = eligibility_consolidate_on_burst(
        &synapse, &trace, &burst_config, &phasic_tonic, 1.0f
    );

    // Should skip due to low trace
    EXPECT_EQ(delta_w, 0.0f);
    EXPECT_EQ(synapse.weight, initial_weight);
}

//=============================================================================
// Test 12: Burst Decay - Consolidation Window
//=============================================================================

TEST_F(EligibilityBurstTest, BurstDecay_ConsolidationWindow) {
    trace.trace = 0.8f;

    // Trigger burst
    phasic_tonic_trigger_burst(&phasic_tonic, 0.8f, 200, 0);  // 200ms duration
    EXPECT_TRUE(phasic_tonic.in_burst_state);

    // Consolidate during burst
    float delta_w_during = eligibility_consolidate_on_burst(
        &synapse, &trace, &burst_config, &phasic_tonic, 1.0f
    );
    EXPECT_GT(delta_w_during, 0.0f);

    // Update phasic-tonic to decay burst
    phasic_tonic_update(&phasic_tonic, 0.3f, 300000);  // 300ms later

    // Burst should have ended
    EXPECT_FALSE(phasic_tonic.in_burst_state);

    // Reset synapse
    synapse.weight = 0.5f;

    // Try to consolidate after burst
    float delta_w_after = eligibility_consolidate_on_burst(
        &synapse, &trace, &burst_config, &phasic_tonic, 1.0f
    );

    // No consolidation after burst ends
    EXPECT_EQ(delta_w_after, 0.0f);
}

//=============================================================================
// Test 13: Configuration Defaults
//=============================================================================

TEST_F(EligibilityBurstTest, Configuration_Defaults) {
    eligibility_config_t default_config = eligibility_default_config();

    // Verify Option 2.2 fields are initialized
    EXPECT_FALSE(default_config.burst_triggered_mode);  // Disabled by default
    EXPECT_EQ(default_config.burst_lr_multiplier, 3.0f);
    EXPECT_EQ(default_config.min_burst_concentration, 0.3f);

    // Verify backward compatibility
    EXPECT_EQ(default_config.decay_lambda, 0.95f);
    EXPECT_EQ(default_config.learning_rate, 0.001f);
    EXPECT_TRUE(default_config.use_neuromodulation);
    EXPECT_EQ(default_config.trace_threshold, 0.01f);
}

//=============================================================================
// Test 14: NULL Safety
//=============================================================================

TEST_F(EligibilityBurstTest, NullSafety_NoSegfault) {
    // NULL checks should prevent crashes
    EXPECT_FALSE(eligibility_is_in_burst(nullptr, &burst_config));
    EXPECT_FALSE(eligibility_is_in_burst(&phasic_tonic, nullptr));

    float delta_w = eligibility_consolidate_on_burst(
        nullptr, &trace, &burst_config, &phasic_tonic, 1.0f
    );
    EXPECT_EQ(delta_w, 0.0f);

    delta_w = eligibility_consolidate_on_burst(
        &synapse, nullptr, &burst_config, &phasic_tonic, 1.0f
    );
    EXPECT_EQ(delta_w, 0.0f);

    int num = eligibility_consolidate_batch(
        nullptr, &trace, 10, &burst_config, &phasic_tonic, 1.0f
    );
    EXPECT_EQ(num, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
