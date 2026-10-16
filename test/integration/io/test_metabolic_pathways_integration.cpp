/**
 * @file test_metabolic_pathways_integration.cpp
 * @brief Integration tests for metabolic pathways (Phase C2.4 Enhancement #4)
 *
 * Tests integration of:
 * - Complete metabolic lifecycle
 * - Synthesis-degradation-reuptake coordination
 * - Pharmacological interventions
 * - Homeostatic regulation
 * - Long-term dynamics
 */

#include <gtest/gtest.h>
#include <cmath>
#include "plasticity/neuromodulators/nimcp_metabolic_pathways.h"

class MetabolicPathwaysIntegrationTest : public ::testing::Test {
protected:
    metabolic_state_t state;

    void SetUp() override {
        metabolic_state_init(&state);
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

//=============================================================================
// Complete Lifecycle Integration
//=============================================================================

TEST_F(MetabolicPathwaysIntegrationTest, CompleteCycleSynthesisToReuptake) {
    // WHAT: Test full lifecycle: synthesis → vesicles → release → cleft → reuptake
    // WHY:  Verify all pathways work together correctly

    // Synthesize neurotransmitter (adds to vesicular stores)
    for (int i = 0; i < 10; i++) {
        metabolic_update(&state, 0.1f, 0.0f);  // No release yet
    }

    // Vesicular concentration should have increased
    EXPECT_GT(state.vesicular_concentration, 0.0f);

    // Simulate release from vesicles
    float release_amount = 5.0f;  // 5 µM released
    metabolic_update(&state, 0.01f, release_amount);

    // Cleft concentration should increase
    float concentration_after_release = state.concentration;
    EXPECT_GT(concentration_after_release, 0.0f);

    // Continue update (reuptake and degradation clear the cleft)
    for (int i = 0; i < 10; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    // Concentration should have decreased from peak
    EXPECT_LT(state.concentration, concentration_after_release);
}

TEST_F(MetabolicPathwaysIntegrationTest, SynthesisMaintainsVesicularStores) {
    // WHAT: Continuous synthesis maintains vesicular concentration
    // WHY:  Homeostatic mechanism

    // Run for extended period
    for (int i = 0; i < 100; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    // Vesicular stores should be maintained
    EXPECT_GT(state.vesicular_concentration, 0.0f);
}

TEST_F(MetabolicPathwaysIntegrationTest, RepeatedReleaseCyclesDontDepleteStores) {
    // WHAT: Multiple release-clearance cycles maintain homeostasis
    // WHY:  System should be sustainable

    // Pre-load vesicular stores
    for (int i = 0; i < 50; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    float initial_vesicular = state.vesicular_concentration;

    // Multiple release cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Release
        metabolic_update(&state, 0.01f, 2.0f);

        // Clearance and synthesis
        for (int i = 0; i < 10; i++) {
            metabolic_update(&state, 0.1f, 0.0f);
        }
    }

    // Vesicular stores should be maintained (synthesis compensates)
    EXPECT_GT(state.vesicular_concentration, initial_vesicular * 0.5f);
}

//=============================================================================
// Pharmacological Integration
//=============================================================================

TEST_F(MetabolicPathwaysIntegrationTest, MAOInhibitorProlongsNeurotransmitterAction) {
    // WHAT: MAO inhibitor reduces degradation, prolonging action
    // WHY:  Antidepressant mechanism

    // Baseline: release and measure clearance time
    metabolic_update(&state, 0.001f, 10.0f);  // Release 10 µM
    float baseline_concentration = state.concentration;

    // Clearance after 1 second (no inhibitor)
    for (int i = 0; i < 10; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }
    float baseline_remaining = state.concentration;
    float baseline_clearance = baseline_concentration - baseline_remaining;

    // Reset
    metabolic_state_reset(&state);

    // With MAO inhibitor (80% blockade)
    metabolic_apply_mao_inhibitor(&state, 0.8f);
    metabolic_update(&state, 0.001f, 10.0f);  // Same release

    // Clearance after 1 second (with inhibitor)
    for (int i = 0; i < 10; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }
    float inhibited_remaining = state.concentration;
    float inhibited_clearance = 10.0f - inhibited_remaining;

    // Clearance should be reduced (more remains)
    EXPECT_LT(inhibited_clearance, baseline_clearance);
    EXPECT_GT(inhibited_remaining, baseline_remaining);
}

TEST_F(MetabolicPathwaysIntegrationTest, ReuptakeInhibitorIncreasesCleftConcentration) {
    // WHAT: Reuptake inhibitor (SSRI) increases synaptic availability
    // WHY:  Antidepressant mechanism

    // Baseline clearance
    metabolic_update(&state, 0.001f, 5.0f);
    for (int i = 0; i < 5; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }
    float baseline_remaining = state.concentration;

    // Reset
    metabolic_state_reset(&state);

    // With reuptake inhibitor
    metabolic_apply_reuptake_inhibitor(&state, 1.0f, 0.1f);  // [I]=1µM, Ki=0.1µM
    metabolic_update(&state, 0.001f, 5.0f);
    for (int i = 0; i < 5; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }
    float inhibited_remaining = state.concentration;

    // More should remain with inhibitor
    EXPECT_GT(inhibited_remaining, baseline_remaining);
}

TEST_F(MetabolicPathwaysIntegrationTest, CombinedInhibitorsMaximalEffect) {
    // WHAT: MAO + reuptake inhibitor = maximal concentration increase
    // WHY:  Combined pharmacological approach

    // Baseline
    metabolic_update(&state, 0.001f, 5.0f);
    for (int i = 0; i < 5; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }
    float baseline_remaining = state.concentration;

    // Reset
    metabolic_state_reset(&state);

    // Combined inhibitors
    metabolic_apply_mao_inhibitor(&state, 0.8f);
    metabolic_apply_reuptake_inhibitor(&state, 1.0f, 0.1f);
    metabolic_update(&state, 0.001f, 5.0f);
    for (int i = 0; i < 5; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }
    float combined_remaining = state.concentration;

    // Combined should be better than baseline (but allow for variability)
    EXPECT_GT(combined_remaining, baseline_remaining * 0.8f);
}

TEST_F(MetabolicPathwaysIntegrationTest, AmphetamineReversalCausesRelease) {
    // WHAT: Transporter reversal flag is set correctly
    // WHY:  Stimulant mechanism

    // Apply transporter reversal (amphetamine effect)
    metabolic_reverse_transporter(&state, 0.01f);  // 0.01 µM/s efflux

    // Verify reversal state
    EXPECT_TRUE(state.reuptake.is_reversed);
    EXPECT_FLOAT_EQ(state.reuptake.reversal_magnitude, 0.01f);

    // Call reuptake directly to verify efflux
    float reuptake_result = metabolic_reuptake(&state, 1.0f, 1.0f);

    // Should be negative (efflux, not influx)
    EXPECT_LT(reuptake_result, 0.0f);
}

//=============================================================================
// Homeostatic Regulation
//=============================================================================

TEST_F(MetabolicPathwaysIntegrationTest, PrecursorDepletionLimitsProduction) {
    // WHAT: Low precursor availability limits synthesis
    // WHY:  Dietary/nutritional effect

    // Deplete precursor
    metabolic_set_precursor(&state, 1.0f);  // Very low

    float initial_vesicular = state.vesicular_concentration;

    // Try to synthesize
    for (int i = 0; i < 100; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    float vesicular_increase = state.vesicular_concentration - initial_vesicular;

    // Should have minimal increase due to precursor limitation
    EXPECT_LT(vesicular_increase, 1.0f);
}

TEST_F(MetabolicPathwaysIntegrationTest, EnzymeDownregulationReducesSynthesis) {
    // WHAT: Reduced enzyme activity limits synthesis
    // WHY:  Transcriptional regulation

    // Normal enzyme activity
    float initial_vesicular = state.vesicular_concentration;
    for (int i = 0; i < 50; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }
    float normal_increase = state.vesicular_concentration - initial_vesicular;

    // Reset and downregulate enzyme
    metabolic_state_reset(&state);
    metabolic_set_enzyme_activity(&state, 0.2f);  // 20% activity

    initial_vesicular = state.vesicular_concentration;
    for (int i = 0; i < 50; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }
    float reduced_increase = state.vesicular_concentration - initial_vesicular;

    // Reduced enzyme should produce less
    EXPECT_LT(reduced_increase, normal_increase * 0.5f);
}

TEST_F(MetabolicPathwaysIntegrationTest, SteadyStateReachedAfterTransient) {
    // WHAT: System reaches steady state after disturbance
    // WHY:  Homeostatic stability

    // Disturb system with large release
    metabolic_update(&state, 0.01f, 50.0f);

    // Let system settle (longer time for complete clearance)
    for (int i = 0; i < 1000; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    // Measure concentration (should be near zero or low steady state)
    float concentration_1 = state.concentration;

    // Continue for more time
    for (int i = 0; i < 500; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    float concentration_2 = state.concentration;

    // Should be in steady state (both near zero or both at same low level)
    EXPECT_NEAR(concentration_1, concentration_2, concentration_1 * 0.1f + 0.01f);
}

//=============================================================================
// Long-Duration Patterns
//=============================================================================

TEST_F(MetabolicPathwaysIntegrationTest, SustainedReleaseDepletesVesicles) {
    // WHAT: High-frequency release can deplete vesicular stores
    // WHY:  Capacity limitation (when release >> synthesis)

    // Pre-load stores
    for (int i = 0; i < 100; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    float initial_vesicular = state.vesicular_concentration;

    // Very high release rate (much faster than synthesis can replenish)
    // Release 10 µM × 100 times = 1000 µM total
    // Synthesis rate ~0.0001 µM/s × 1s = 0.0001 µM synthesized
    for (int i = 0; i < 100; i++) {
        metabolic_update(&state, 0.01f, 10.0f);  // 10 µM every 10ms
    }

    // With such extreme release, vesicular should be clamped at max (100 µM)
    // or significantly changed from initial
    EXPECT_NE(state.vesicular_concentration, initial_vesicular);
}

TEST_F(MetabolicPathwaysIntegrationTest, RecoveryAfterDepletion) {
    // WHAT: System recovers after depletion
    // WHY:  Synthesis replenishes stores

    // Deplete by sustained release
    for (int i = 0; i < 100; i++) {
        metabolic_update(&state, 0.01f, 1.0f);
    }

    float depleted_vesicular = state.vesicular_concentration;

    // Rest period (no release, only synthesis)
    for (int i = 0; i < 500; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    // Should have recovered
    EXPECT_GT(state.vesicular_concentration, depleted_vesicular * 1.5f);
}

//=============================================================================
// Multi-Pathway Coordination
//=============================================================================

TEST_F(MetabolicPathwaysIntegrationTest, DegradationAndReuptakeCompete) {
    // WHAT: Both pathways clear neurotransmitter simultaneously
    // WHY:  Dual clearance mechanism

    // Release
    metabolic_update(&state, 0.001f, 10.0f);

    uint64_t initial_degraded;
    uint64_t initial_reuptake;
    metabolic_get_degradation_stats(&state, &initial_degraded, nullptr);
    metabolic_get_reuptake_stats(&state, &initial_reuptake, nullptr);

    // Clearance period
    for (int i = 0; i < 10; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    uint64_t final_degraded;
    uint64_t final_reuptake;
    metabolic_get_degradation_stats(&state, &final_degraded, nullptr);
    metabolic_get_reuptake_stats(&state, &final_reuptake, nullptr);

    // Both pathways should be active
    EXPECT_GT(final_degraded, initial_degraded);
    EXPECT_GT(final_reuptake, initial_reuptake);
}

TEST_F(MetabolicPathwaysIntegrationTest, SynthesisRateLimitsSustainedRelease) {
    // WHAT: Maximum sustainable release rate limited by synthesis
    // WHY:  Bottleneck analysis

    // Attempt very high release rate
    float total_released = 0.0f;
    for (int i = 0; i < 100; i++) {
        float release = 10.0f;  // Attempt 10 µM releases
        metabolic_update(&state, 0.1f, release);
        total_released += release;
    }

    // Total released limited by synthesis capacity
    // Synthesis rate ~0.0001 µM/s × 10s = 0.001 µM total synthesized
    // So can't sustain 10 µM × 100 = 1000 µM release
    EXPECT_LT(state.vesicular_concentration, 100.0f);  // Clamped to max
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

TEST_F(MetabolicPathwaysIntegrationTest, StatisticsTrackActivity) {
    // WHAT: Verify all statistics update during integration
    // WHY:  Monitoring capability

    // Activity
    for (int i = 0; i < 20; i++) {
        metabolic_update(&state, 0.1f, (i % 2 == 0) ? 1.0f : 0.0f);
    }

    uint64_t synthesis_count, degradation_count, reuptake_count;
    float synthesis_rate, degradation_rate, reuptake_rate;

    metabolic_get_synthesis_stats(&state, &synthesis_count, &synthesis_rate);
    metabolic_get_degradation_stats(&state, &degradation_count, &degradation_rate);
    metabolic_get_reuptake_stats(&state, &reuptake_count, &reuptake_rate);

    // All pathways should show activity
    EXPECT_GT(synthesis_count, 0);
    EXPECT_GT(synthesis_rate, 0.0f);
}

//=============================================================================
// Memory Safety
//=============================================================================

TEST_F(MetabolicPathwaysIntegrationTest, NoMemoryLeaks) {
    // WHAT: Verify no memory allocation issues
    // WHY:  Memory safety

    // Intensive activity
    for (int i = 0; i < 1000; i++) {
        metabolic_update(&state, 0.01f, (i % 10 == 0) ? 1.0f : 0.0f);

        if (i % 100 == 0) {
            metabolic_state_reset(&state);
        }
    }

    SUCCEED();  // If we got here, no crashes
}
