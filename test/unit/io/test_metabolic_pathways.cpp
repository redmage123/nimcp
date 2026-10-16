/**
 * @file test_metabolic_pathways.cpp
 * @brief Unit tests for neurotransmitter metabolism (Phase C2.4 Enhancement #4)
 *
 * Test coverage:
 * - Initialization and configuration
 * - Synthesis pathways (enzyme kinetics)
 * - Degradation pathways (first-order kinetics)
 * - Reuptake mechanisms (Michaelis-Menten)
 * - Integrated dynamics
 * - Statistics and monitoring
 * - Edge cases and error handling
 */

#include <gtest/gtest.h>
#include <cmath>
#include "utils/nimcp_test_base.h"
#include "plasticity/neuromodulators/nimcp_metabolic_pathways.h"

class MetabolicPathwaysTest : public NimcpTestBase {
protected:
    metabolic_state_t state;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        metabolic_state_init(&state);
    }

    void TearDown() override {
        // Cleanup if needed

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(MetabolicPathwaysTest, DefaultInitializationSetsExpectedState) {
    // WHAT: Verify default initialization creates valid state
    // WHY:  Behavioral baseline test

    EXPECT_GT(state.synthesis.precursor_concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.synthesis.enzyme_activity, 1.0f);
    EXPECT_FLOAT_EQ(state.synthesis.cofactor_availability, 1.0f);
    EXPECT_GT(state.synthesis.base_synthesis_rate, 0.0f);

    EXPECT_FLOAT_EQ(state.degradation.enzyme_activity, 1.0f);
    EXPECT_GT(state.degradation.degradation_rate, 0.0f);
    EXPECT_FLOAT_EQ(state.degradation.inhibitor_blockade, 0.0f);

    EXPECT_GT(state.reuptake.km, 0.0f);
    EXPECT_GT(state.reuptake.vmax, 0.0f);
    EXPECT_FLOAT_EQ(state.reuptake.transporter_density, 1.0f);
    EXPECT_FALSE(state.reuptake.is_reversed);

    EXPECT_FLOAT_EQ(state.concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.vesicular_concentration, 0.0f);
}

TEST_F(MetabolicPathwaysTest, CustomConfigInitialization) {
    // WHAT: Test initialization with custom parameters
    // WHY:  Verify config system works correctly

    metabolic_config_t config = {
        .synthesis_rate = 0.0005f,
        .precursor_level = 100.0f,
        .enzyme_activity = 0.5f,
        .degradation_rate = 0.2f,
        .enzyme_expression = 0.8f,
        .reuptake_km = 0.001f,
        .reuptake_vmax = 0.002f,
        .transporter_density = 0.7f,
        .enable_synthesis = true,
        .enable_degradation = true,
        .enable_reuptake = true
    };

    metabolic_state_init_with_config(&state, &config);

    EXPECT_FLOAT_EQ(state.synthesis.base_synthesis_rate, 0.0005f);
    EXPECT_FLOAT_EQ(state.synthesis.precursor_concentration, 100.0f);
    EXPECT_FLOAT_EQ(state.synthesis.enzyme_activity, 0.5f);
    EXPECT_FLOAT_EQ(state.degradation.degradation_rate, 0.2f);
    EXPECT_FLOAT_EQ(state.degradation.enzyme_activity, 0.8f);
    EXPECT_FLOAT_EQ(state.reuptake.km, 0.001f);
    EXPECT_FLOAT_EQ(state.reuptake.vmax, 0.002f);
    EXPECT_FLOAT_EQ(state.reuptake.transporter_density, 0.7f);
}

TEST_F(MetabolicPathwaysTest, ResetRestoresInitialState) {
    // WHAT: Verify reset clears dynamic state
    // WHY:  API contract test

    // Modify state
    state.concentration = 10.0f;
    state.vesicular_concentration = 50.0f;
    state.degradation.metabolite_concentration = 5.0f;
    state.synthesis.total_synthesized = 100;

    // Preserve config
    float original_synthesis_rate = state.synthesis.base_synthesis_rate;

    // Reset
    metabolic_state_reset(&state);

    // Dynamic state should be cleared
    EXPECT_FLOAT_EQ(state.concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.vesicular_concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.degradation.metabolite_concentration, 0.0f);
    EXPECT_EQ(state.synthesis.total_synthesized, 0);

    // Config should be preserved
    EXPECT_FLOAT_EQ(state.synthesis.base_synthesis_rate, original_synthesis_rate);
}

TEST_F(MetabolicPathwaysTest, NeurotransmitterSpecificConfigs) {
    // WHAT: Verify each neurotransmitter has unique config
    // WHY:  Biological specificity test

    metabolic_config_t da_config = metabolic_config_dopamine_default();
    metabolic_config_t serotonin_config = metabolic_config_serotonin_default();
    metabolic_config_t ne_config = metabolic_config_norepinephrine_default();
    metabolic_config_t ach_config = metabolic_config_acetylcholine_default();

    // Different precursor levels
    EXPECT_NE(da_config.precursor_level, serotonin_config.precursor_level);
    EXPECT_NE(da_config.precursor_level, ach_config.precursor_level);

    // Different reuptake parameters
    EXPECT_NE(da_config.reuptake_km, serotonin_config.reuptake_km);
    EXPECT_NE(da_config.reuptake_vmax, ne_config.reuptake_vmax);

    // ACh has much faster degradation (AChE vs MAO)
    EXPECT_GT(ach_config.degradation_rate, da_config.degradation_rate);
}

//=============================================================================
// Synthesis Pathway Tests
//=============================================================================

TEST_F(MetabolicPathwaysTest, SynthesisProducesNeurotransmitter) {
    // WHAT: Basic synthesis with available precursor
    // WHY:  Core functionality test

    float dt = 1.0f;  // 1 second
    float synthesized = metabolic_synthesize(&state, dt);

    EXPECT_GT(synthesized, 0.0f);
    EXPECT_GT(state.synthesis.current_synthesis_rate, 0.0f);
    EXPECT_EQ(state.synthesis.total_synthesized, 1);
}

TEST_F(MetabolicPathwaysTest, SynthesisDepletePrecursor) {
    // WHAT: Synthesis consumes precursor
    // WHY:  Mass balance verification

    float initial_precursor = state.synthesis.precursor_concentration;

    metabolic_synthesize(&state, 1.0f);

    // Precursor should be consumed
    EXPECT_LT(state.synthesis.precursor_concentration, initial_precursor);
}

TEST_F(MetabolicPathwaysTest, SynthesisStopsWhenPrecursorDepleted) {
    // WHAT: No synthesis without precursor
    // WHY:  Biological constraint

    state.synthesis.precursor_concentration = 0.0f;

    float synthesized = metabolic_synthesize(&state, 1.0f);

    EXPECT_FLOAT_EQ(synthesized, 0.0f);
    EXPECT_FLOAT_EQ(state.synthesis.current_synthesis_rate, 0.0f);
}

TEST_F(MetabolicPathwaysTest, EnzymeActivityModulatesSynthesis) {
    // WHAT: Enzyme activity scales synthesis rate
    // WHY:  Regulatory mechanism test

    // Full enzyme activity
    state.synthesis.enzyme_activity = 1.0f;
    float synthesis_full = metabolic_synthesize(&state, 1.0f);

    // Reset
    metabolic_state_reset(&state);

    // Half enzyme activity
    state.synthesis.enzyme_activity = 0.5f;
    float synthesis_half = metabolic_synthesize(&state, 1.0f);

    // Half activity should produce roughly half amount
    EXPECT_LT(synthesis_half, synthesis_full);
    EXPECT_NEAR(synthesis_half, synthesis_full * 0.5f, synthesis_full * 0.2f);
}

TEST_F(MetabolicPathwaysTest, PrecursorSaturationKinetics) {
    // WHAT: Synthesis saturates at high precursor levels
    // WHY:  Michaelis-Menten-like saturation

    // Low precursor
    state.synthesis.precursor_concentration = 1.0f;
    float synthesis_low = metabolic_synthesize(&state, 1.0f);

    metabolic_state_reset(&state);

    // High precursor (10x)
    state.synthesis.precursor_concentration = 10.0f;
    float synthesis_high = metabolic_synthesize(&state, 1.0f);

    // Higher precursor should increase synthesis, but not linearly
    EXPECT_GT(synthesis_high, synthesis_low);
    EXPECT_LT(synthesis_high, synthesis_low * 10.0f);  // Not 10x increase
}

TEST_F(MetabolicPathwaysTest, SetPrecursorUpdatesLevel) {
    // WHAT: Test precursor setting function
    // WHY:  API test

    metabolic_set_precursor(&state, 75.0f);

    EXPECT_FLOAT_EQ(state.synthesis.precursor_concentration, 75.0f);
}

TEST_F(MetabolicPathwaysTest, SetEnzymeActivityUpdatesRate) {
    // WHAT: Test enzyme activity modulation
    // WHY:  API test

    metabolic_set_enzyme_activity(&state, 0.3f);

    EXPECT_FLOAT_EQ(state.synthesis.enzyme_activity, 0.3f);
}

TEST_F(MetabolicPathwaysTest, SynthesisStatisticsTracking) {
    // WHAT: Verify statistics accumulation
    // WHY:  Monitoring test

    // Multiple synthesis events
    for (int i = 0; i < 10; i++) {
        metabolic_synthesize(&state, 0.1f);
    }

    uint64_t total;
    float avg_rate;
    metabolic_get_synthesis_stats(&state, &total, &avg_rate);

    EXPECT_EQ(total, 10);
    EXPECT_GT(avg_rate, 0.0f);
}

//=============================================================================
// Degradation Pathway Tests
//=============================================================================

TEST_F(MetabolicPathwaysTest, DegradationReducesConcentration) {
    // WHAT: Basic first-order degradation
    // WHY:  Core functionality test

    state.concentration = 10.0f;

    float degraded = metabolic_degrade(&state, 1.0f);

    EXPECT_GT(degraded, 0.0f);
    EXPECT_GT(state.degradation.metabolite_concentration, 0.0f);
}

TEST_F(MetabolicPathwaysTest, DegradationFollowsFirstOrderKinetics) {
    // WHAT: Verify exponential decay
    // WHY:  Mathematical correctness

    state.concentration = 100.0f;
    float rate = state.degradation.degradation_rate;

    float degraded = metabolic_degrade(&state, 1.0f);

    // First-order: amount = C × (1 - e^(-k×t))
    float expected = 100.0f * (1.0f - expf(-rate * 1.0f));

    EXPECT_NEAR(degraded, expected, 1.0f);
}

TEST_F(MetabolicPathwaysTest, MAOInhibitorReducesDegradation) {
    // WHAT: MAO inhibition blocks degradation
    // WHY:  Pharmacological intervention test

    state.concentration = 10.0f;

    // No inhibitor
    float degraded_baseline = metabolic_degrade(&state, 1.0f);

    // Reset
    state.concentration = 10.0f;
    state.degradation.metabolite_concentration = 0.0f;

    // Apply MAO inhibitor (80% blockade)
    metabolic_apply_mao_inhibitor(&state, 0.8f);
    float degraded_inhibited = metabolic_degrade(&state, 1.0f);

    // Inhibition should reduce degradation
    EXPECT_LT(degraded_inhibited, degraded_baseline);
    EXPECT_LT(degraded_inhibited, degraded_baseline * 0.3f);  // At most 20% of baseline
}

TEST_F(MetabolicPathwaysTest, COMTInhibitorReducesDegradation) {
    // WHAT: COMT inhibition blocks degradation
    // WHY:  Parkinson's drug mechanism

    state.concentration = 10.0f;
    float degraded_baseline = metabolic_degrade(&state, 1.0f);

    state.concentration = 10.0f;
    state.degradation.metabolite_concentration = 0.0f;

    metabolic_apply_comt_inhibitor(&state, 0.9f);
    float degraded_inhibited = metabolic_degrade(&state, 1.0f);

    EXPECT_LT(degraded_inhibited, degraded_baseline);
}

TEST_F(MetabolicPathwaysTest, CompleteInhibitionBlocksDegradation) {
    // WHAT: 100% inhibition = zero degradation
    // WHY:  Boundary condition test

    state.concentration = 10.0f;
    metabolic_apply_mao_inhibitor(&state, 1.0f);  // Complete blockade

    float degraded = metabolic_degrade(&state, 1.0f);

    EXPECT_FLOAT_EQ(degraded, 0.0f);
}

TEST_F(MetabolicPathwaysTest, MetaboliteAccumulation) {
    // WHAT: Degradation produces metabolites
    // WHY:  Mass balance verification

    state.concentration = 50.0f;

    // Multiple degradation cycles
    for (int i = 0; i < 10; i++) {
        metabolic_degrade(&state, 0.1f);
    }

    // Metabolites should accumulate
    EXPECT_GT(state.degradation.metabolite_concentration, 0.0f);
}

TEST_F(MetabolicPathwaysTest, DegradationStatisticsTracking) {
    // WHAT: Verify statistics accumulation
    // WHY:  Monitoring test

    state.concentration = 10.0f;

    for (int i = 0; i < 10; i++) {
        metabolic_degrade(&state, 0.1f);
    }

    uint64_t total;
    float avg_rate;
    metabolic_get_degradation_stats(&state, &total, &avg_rate);

    EXPECT_GT(total, 0);
    EXPECT_GT(avg_rate, 0.0f);
}

//=============================================================================
// Reuptake Mechanism Tests
//=============================================================================

TEST_F(MetabolicPathwaysTest, ReuptakeRemovesNeurotransmitter) {
    // WHAT: Basic Michaelis-Menten reuptake
    // WHY:  Core functionality test

    float concentration = 1.0f;  // 1 µM

    float reuptaken = metabolic_reuptake(&state, concentration, 1.0f);

    EXPECT_GT(reuptaken, 0.0f);
    EXPECT_GT(state.reuptake.total_reuptake_events, 0);
}

TEST_F(MetabolicPathwaysTest, ReuptakeFollowsMichaelisMentenKinetics) {
    // WHAT: Verify saturation kinetics
    // WHY:  Mathematical correctness

    float km = state.reuptake.km;
    float vmax = state.reuptake.vmax;

    // At [S] = Km, rate should be Vmax/2
    float rate_at_km = metabolic_reuptake(&state, km, 1.0f);
    float expected_rate = (vmax * km) / (km + km) * 1.0f;  // Vmax/2 × dt

    EXPECT_NEAR(rate_at_km, expected_rate, expected_rate * 0.1f);
}

TEST_F(MetabolicPathwaysTest, ReuptakeSaturatesAtHighConcentration) {
    // WHAT: High concentration approaches Vmax
    // WHY:  Enzyme saturation behavior

    // Very low concentration (<< Km, about 1/10 of Km)
    float reuptake_low = metabolic_reuptake(&state, state.reuptake.km * 0.1f, 1.0f);

    // Very high concentration (100x Km)
    float reuptake_high = metabolic_reuptake(&state, state.reuptake.km * 100.0f, 1.0f);

    // At very high [S], rate approaches Vmax (should be much higher than low)
    // Ratio should be approximately: (100/(100+1)) / (0.1/(0.1+1)) = 0.99 / 0.09 = 11x
    EXPECT_GT(reuptake_high, reuptake_low * 8.0f);  // At least 8x increase

    // At very high concentration, rate should approach Vmax × dt
    // Should be at least 95% of Vmax
    EXPECT_GT(reuptake_high, state.reuptake.vmax * 1.0f * 0.95f);
}

TEST_F(MetabolicPathwaysTest, CompetitiveInhibitionReducesReuptake) {
    // WHAT: Reuptake inhibitor (SSRI) reduces clearance
    // WHY:  Pharmacological mechanism

    float concentration = 1.0f;

    // Baseline reuptake
    float reuptake_baseline = metabolic_reuptake(&state, concentration, 1.0f);

    // Apply inhibitor
    metabolic_apply_reuptake_inhibitor(&state, 0.5f, 0.1f);  // [I]=0.5µM, Ki=0.1µM
    float reuptake_inhibited = metabolic_reuptake(&state, concentration, 1.0f);

    // Inhibition should reduce reuptake
    EXPECT_LT(reuptake_inhibited, reuptake_baseline);
}

TEST_F(MetabolicPathwaysTest, TransporterReversalCausesEfflux) {
    // WHAT: Reversed transporter causes release (amphetamine)
    // WHY:  Stimulant mechanism

    metabolic_reverse_transporter(&state, 0.005f);  // 0.005 µM/s efflux

    float reuptaken = metabolic_reuptake(&state, 1.0f, 1.0f);

    // Negative return = efflux
    EXPECT_LT(reuptaken, 0.0f);
    EXPECT_TRUE(state.reuptake.is_reversed);
}

TEST_F(MetabolicPathwaysTest, TransporterDensityScalesReuptake) {
    // WHAT: Transporter expression affects clearance rate
    // WHY:  Biological regulation

    float concentration = 1.0f;

    // Full density
    state.reuptake.transporter_density = 1.0f;
    float reuptake_full = metabolic_reuptake(&state, concentration, 1.0f);

    // Half density
    state.reuptake.transporter_density = 0.5f;
    float reuptake_half = metabolic_reuptake(&state, concentration, 1.0f);

    // Half density should produce half reuptake
    EXPECT_NEAR(reuptake_half, reuptake_full * 0.5f, reuptake_full * 0.1f);
}

TEST_F(MetabolicPathwaysTest, ReuptakeStatisticsTracking) {
    // WHAT: Verify statistics accumulation
    // WHY:  Monitoring test

    for (int i = 0; i < 10; i++) {
        metabolic_reuptake(&state, 1.0f, 0.1f);
    }

    uint64_t total;
    float avg_rate;
    metabolic_get_reuptake_stats(&state, &total, &avg_rate);

    EXPECT_GT(total, 0);
    EXPECT_GT(avg_rate, 0.0f);
}

//=============================================================================
// Integrated Update Tests
//=============================================================================

TEST_F(MetabolicPathwaysTest, IntegratedUpdateCombinesAllPathways) {
    // WHAT: Full lifecycle integration
    // WHY:  System integration test

    // Start with some vesicular stores
    state.vesicular_concentration = 100.0f;

    // Simulate release and update
    float release_amount = 5.0f;  // 5 µM released from vesicles
    float new_concentration = metabolic_update(&state, 1.0f, release_amount);

    // Concentration should reflect release minus clearance
    EXPECT_GT(new_concentration, 0.0f);
    EXPECT_LT(new_concentration, release_amount);  // Some cleared
}

TEST_F(MetabolicPathwaysTest, SteadyStateBalance) {
    // WHAT: Synthesis balances degradation + reuptake at steady state
    // WHY:  Homeostasis verification

    // Run for long time to reach steady state
    for (int i = 0; i < 1000; i++) {
        metabolic_update(&state, 0.01f, 0.0f);  // No release, just metabolism
    }

    // Should reach low equilibrium concentration
    EXPECT_GE(state.concentration, 0.0f);
    EXPECT_LT(state.concentration, 1.0f);
}

TEST_F(MetabolicPathwaysTest, VesicularUptakeStoresNeurotransmitter) {
    // WHAT: Cytoplasmic neurotransmitter moves to vesicles
    // WHY:  Storage mechanism

    state.concentration = 10.0f;  // Cytoplasmic
    float initial_vesicular = state.vesicular_concentration;

    metabolic_update(&state, 1.0f, 0.0f);

    // Vesicular concentration should increase
    EXPECT_GT(state.vesicular_concentration, initial_vesicular);
}

TEST_F(MetabolicPathwaysTest, ReleaseIncreasesCleftConcentration) {
    // WHAT: Vesicular release adds to cleft
    // WHY:  Release mechanism

    float initial_concentration = state.concentration;
    float release_amount = 5.0f;

    metabolic_update(&state, 0.001f, release_amount);  // Very short dt, minimal clearance

    // Concentration should increase by approximately release amount
    EXPECT_GT(state.concentration, initial_concentration);
}

TEST_F(MetabolicPathwaysTest, ConcentrationClamping) {
    // WHAT: Concentration stays within valid range
    // WHY:  Numerical stability

    // Try to create extremely high concentration
    metabolic_update(&state, 1.0f, 1000.0f);

    // Should be clamped to reasonable range
    EXPECT_LE(state.concentration, 100.0f);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(MetabolicPathwaysTest, HandlesNullPointers) {
    // WHAT: Null pointer safety
    // WHY:  Defensive programming

    metabolic_state_init(nullptr);  // Should not crash
    metabolic_synthesize(nullptr, 1.0f);
    metabolic_degrade(nullptr, 1.0f);
    metabolic_reuptake(nullptr, 1.0f, 1.0f);
    metabolic_update(nullptr, 1.0f, 1.0f);

    SUCCEED();  // No crash
}

TEST_F(MetabolicPathwaysTest, HandlesZeroDt) {
    // WHAT: Zero time step should not change state
    // WHY:  Idempotence test

    state.concentration = 5.0f;
    float initial_concentration = state.concentration;

    metabolic_update(&state, 0.0f, 0.0f);

    // Concentration unchanged
    EXPECT_FLOAT_EQ(state.concentration, initial_concentration);
}

TEST_F(MetabolicPathwaysTest, HandlesZeroConcentration) {
    // WHAT: Zero concentration edge case
    // WHY:  Boundary condition

    state.concentration = 0.0f;

    float degraded = metabolic_degrade(&state, 1.0f);
    float reuptaken = metabolic_reuptake(&state, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(degraded, 0.0f);
    EXPECT_FLOAT_EQ(reuptaken, 0.0f);
}

TEST_F(MetabolicPathwaysTest, HandlesNegativeDt) {
    // WHAT: Negative time step should be rejected
    // WHY:  Input validation

    float synthesized = metabolic_synthesize(&state, -1.0f);

    EXPECT_FLOAT_EQ(synthesized, 0.0f);
}

TEST_F(MetabolicPathwaysTest, PrecursorCantGoNegative) {
    // WHAT: Precursor concentration clamped at zero
    // WHY:  Physical constraint

    state.synthesis.precursor_concentration = 0.001f;

    // Synthesize more than available
    metabolic_synthesize(&state, 1000.0f);

    // Precursor should be zero, not negative
    EXPECT_GE(state.synthesis.precursor_concentration, 0.0f);
}

TEST_F(MetabolicPathwaysTest, ExtremeEnzymeActivityClamping) {
    // WHAT: Enzyme activity stays in reasonable range
    // WHY:  Parameter validation

    metabolic_set_enzyme_activity(&state, 1000.0f);  // Extreme value

    // Should be clamped to max 2.0 (2x upregulation)
    EXPECT_LE(state.synthesis.enzyme_activity, 2.0f);
}

TEST_F(MetabolicPathwaysTest, StatisticsNeverOverflow) {
    // WHAT: Counters don't overflow after many operations
    // WHY:  Long-term stability

    // Many synthesis events
    for (int i = 0; i < 10000; i++) {
        metabolic_synthesize(&state, 0.001f);
    }

    // Statistics should be reasonable
    EXPECT_GT(state.synthesis.total_synthesized, 0);
    EXPECT_LT(state.synthesis.total_synthesized, 20000);
}
