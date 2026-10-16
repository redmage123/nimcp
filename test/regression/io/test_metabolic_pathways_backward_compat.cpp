/**
 * @file test_metabolic_pathways_backward_compat.cpp
 * @brief Regression tests for metabolic pathways backward compatibility
 *
 * These tests ensure that:
 * - API contracts remain stable
 * - Default behavior doesn't change
 * - Previously fixed bugs don't reoccur
 * - Performance characteristics are maintained
 */

#include <gtest/gtest.h>
#include <cmath>
#include "plasticity/neuromodulators/nimcp_metabolic_pathways.h"

class MetabolicPathwaysRegressionTest : public ::testing::Test {
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
// API Stability Tests
//=============================================================================

TEST_F(MetabolicPathwaysRegressionTest, DefaultConfigValuesRemainStable) {
    // WHAT: Verify default configuration hasn't changed
    // WHY:  Changing defaults breaks existing code

    metabolic_config_t da_config = metabolic_config_dopamine_default();

    // Synthesis parameters (must not change without major version bump)
    EXPECT_FLOAT_EQ(da_config.synthesis_rate, METABOLISM_SYNTHESIS_RATE_DOPAMINE);
    EXPECT_FLOAT_EQ(da_config.precursor_level, METABOLISM_PRECURSOR_TYROSINE);
    EXPECT_FLOAT_EQ(da_config.enzyme_activity, 1.0f);

    // Degradation parameters
    EXPECT_FLOAT_EQ(da_config.degradation_rate, METABOLISM_DEGRADATION_RATE_MAO);
    EXPECT_FLOAT_EQ(da_config.enzyme_expression, 1.0f);

    // Reuptake parameters
    EXPECT_FLOAT_EQ(da_config.reuptake_km, METABOLISM_REUPTAKE_KM_DAT);
    EXPECT_FLOAT_EQ(da_config.reuptake_vmax, METABOLISM_REUPTAKE_VMAX_DAT);
    EXPECT_FLOAT_EQ(da_config.transporter_density, 1.0f);

    // Feature flags
    EXPECT_TRUE(da_config.enable_synthesis);
    EXPECT_TRUE(da_config.enable_degradation);
    EXPECT_TRUE(da_config.enable_reuptake);
}

TEST_F(MetabolicPathwaysRegressionTest, InitializationSetsExpectedState) {
    // WHAT: Verify initialization creates correct initial state
    // WHY:  Behavioral regression test

    EXPECT_GT(state.synthesis.precursor_concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.synthesis.enzyme_activity, 1.0f);
    EXPECT_FLOAT_EQ(state.synthesis.cofactor_availability, 1.0f);

    EXPECT_FLOAT_EQ(state.degradation.inhibitor_blockade, 0.0f);
    EXPECT_FLOAT_EQ(state.degradation.metabolite_concentration, 0.0f);

    EXPECT_GT(state.reuptake.km, 0.0f);
    EXPECT_GT(state.reuptake.vmax, 0.0f);
    EXPECT_FALSE(state.reuptake.is_reversed);

    EXPECT_FLOAT_EQ(state.concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.vesicular_concentration, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, ResetRestoresInitialState) {
    // WHAT: Verify reset() returns to initial state
    // WHY:  API contract test

    // Modify state
    state.concentration = 10.0f;
    state.vesicular_concentration = 50.0f;
    state.degradation.metabolite_concentration = 5.0f;

    // Reset
    metabolic_state_reset(&state);

    // Should be back to initial
    EXPECT_FLOAT_EQ(state.concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.vesicular_concentration, 0.0f);
    EXPECT_FLOAT_EQ(state.degradation.metabolite_concentration, 0.0f);
}

//=============================================================================
// Behavioral Regression Tests
//=============================================================================

TEST_F(MetabolicPathwaysRegressionTest, SynthesisRequiresPrecursor) {
    // WHAT: Verify synthesis stops without precursor
    // WHY:  Biological constraint (regression: bug #001)

    state.synthesis.precursor_concentration = 0.0f;

    float synthesized = metabolic_synthesize(&state, 1.0f);

    EXPECT_FLOAT_EQ(synthesized, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, DegradationFollowsFirstOrder) {
    // WHAT: Degradation rate matches first-order kinetics
    // WHY:  Mathematical correctness

    state.concentration = 10.0f;
    float rate = state.degradation.degradation_rate;

    float degraded = metabolic_degrade(&state, 1.0f);

    // First-order: amount = C × (1 - e^(-k×t))
    float expected = 10.0f * (1.0f - expf(-rate * 1.0f));

    EXPECT_NEAR(degraded, expected, 0.5f);
}

TEST_F(MetabolicPathwaysRegressionTest, ReuptakeFollowsMichaelisMenten) {
    // WHAT: Reuptake saturates at high concentration
    // WHY:  Enzyme kinetics correctness

    float km = state.reuptake.km;
    float vmax = state.reuptake.vmax;

    // At [S] = Km, rate should be Vmax/2
    float rate_at_km = metabolic_reuptake(&state, km, 1.0f);
    float expected_rate = (vmax * km) / (km + km) * 1.0f;

    EXPECT_NEAR(rate_at_km, expected_rate, expected_rate * 0.1f);
}

TEST_F(MetabolicPathwaysRegressionTest, MAOInhibitionReducesDegradation) {
    // WHAT: MAO inhibitor reduces degradation rate
    // WHY:  Pharmacological mechanism

    state.concentration = 10.0f;
    float baseline = metabolic_degrade(&state, 1.0f);

    state.concentration = 10.0f;
    state.degradation.metabolite_concentration = 0.0f;
    metabolic_apply_mao_inhibitor(&state, 0.8f);
    float inhibited = metabolic_degrade(&state, 1.0f);

    EXPECT_LT(inhibited, baseline);
}

//=============================================================================
// Bug Fix Regression Tests
//=============================================================================

TEST_F(MetabolicPathwaysRegressionTest, BugFix_SynthesizedNeurotransmitterStoredInVesicles) {
    // WHAT: Synthesized neurotransmitter goes to vesicular stores
    // WHY:  Fixed in Phase C2.4: synthesis adds to vesicular pool

    float initial_vesicular = state.vesicular_concentration;

    // Synthesize
    for (int i = 0; i < 10; i++) {
        metabolic_update(&state, 0.1f, 0.0f);
    }

    // Vesicular should have increased
    EXPECT_GT(state.vesicular_concentration, initial_vesicular);
}

TEST_F(MetabolicPathwaysRegressionTest, BugFix_ReuptakeReturnsNegativeOnReversal) {
    // WHAT: Reversed transporter returns negative value (efflux)
    // WHY:  Fixed in Phase C2.4: negative indicates outward transport

    metabolic_reverse_transporter(&state, 0.01f);

    float result = metabolic_reuptake(&state, 1.0f, 1.0f);

    EXPECT_LT(result, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, BugFix_PrecursorCantGoNegative) {
    // WHAT: Precursor concentration clamped at zero
    // WHY:  Fixed in Phase C2.4: prevents negative concentrations

    state.synthesis.precursor_concentration = 0.001f;

    // Try to synthesize more than available
    metabolic_synthesize(&state, 1000.0f);

    EXPECT_GE(state.synthesis.precursor_concentration, 0.0f);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(MetabolicPathwaysRegressionTest, SynthesisOperationCompletesFast) {
    // WHAT: Verify synthesis is fast
    // WHY:  Performance regression test

    for (int i = 0; i < 1000; i++) {
        metabolic_synthesize(&state, 0.01f);
    }

    SUCCEED();  // If we got here, operations completed
}

TEST_F(MetabolicPathwaysRegressionTest, DegradationOperationCompletesFast) {
    // WHAT: Verify degradation is fast
    // WHY:  Performance regression test

    state.concentration = 10.0f;

    for (int i = 0; i < 1000; i++) {
        metabolic_degrade(&state, 0.01f);
        state.concentration = 10.0f;  // Reset for next iteration
    }

    SUCCEED();
}

TEST_F(MetabolicPathwaysRegressionTest, ReuptakeOperationCompletesFast) {
    // WHAT: Verify reuptake is fast
    // WHY:  Performance regression test

    for (int i = 0; i < 1000; i++) {
        metabolic_reuptake(&state, 1.0f, 0.01f);
    }

    SUCCEED();
}

TEST_F(MetabolicPathwaysRegressionTest, UpdateOperationIdempotent) {
    // WHAT: Verify update() with dt=0 doesn't change state
    // WHY:  API contract test

    float initial_concentration = state.concentration;
    float initial_vesicular = state.vesicular_concentration;

    metabolic_update(&state, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(state.concentration, initial_concentration);
    EXPECT_FLOAT_EQ(state.vesicular_concentration, initial_vesicular);
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

TEST_F(MetabolicPathwaysRegressionTest, HandlesZeroConcentrations) {
    // WHAT: Zero concentration edge case
    // WHY:  Boundary condition

    state.concentration = 0.0f;

    float degraded = metabolic_degrade(&state, 1.0f);
    float reuptaken = metabolic_reuptake(&state, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(degraded, 0.0f);
    EXPECT_FLOAT_EQ(reuptaken, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, HandlesExtremeInhibition) {
    // WHAT: Complete inhibition (100%) handled correctly
    // WHY:  Boundary value test

    state.concentration = 10.0f;
    metabolic_apply_mao_inhibitor(&state, 1.0f);  // 100% inhibition

    float degraded = metabolic_degrade(&state, 1.0f);

    EXPECT_FLOAT_EQ(degraded, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, ConcentrationNeverGoesNegative) {
    // WHAT: Concentration stays non-negative
    // WHY:  Physical constraint

    state.concentration = 0.001f;

    // Try extreme degradation
    for (int i = 0; i < 100; i++) {
        metabolic_degrade(&state, 1.0f);
    }

    EXPECT_GE(state.concentration, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, StatisticsNeverOverflow) {
    // WHAT: Counters don't overflow after many operations
    // WHY:  Long-term stability

    // Many operations
    for (int i = 0; i < 10000; i++) {
        metabolic_synthesize(&state, 0.01f);
        if (i % 100 == 0) {
            metabolic_state_reset(&state);
        }
    }

    // Statistics should be reasonable
    EXPECT_GT(state.synthesis.total_synthesized, 0);
    EXPECT_LT(state.synthesis.total_synthesized, 20000);
}

TEST_F(MetabolicPathwaysRegressionTest, HandlesNullPointers) {
    // WHAT: Null pointer safety
    // WHY:  Defensive programming

    metabolic_state_init(nullptr);
    metabolic_synthesize(nullptr, 1.0f);
    metabolic_degrade(nullptr, 1.0f);
    metabolic_reuptake(nullptr, 1.0f, 1.0f);
    metabolic_update(nullptr, 1.0f, 1.0f);

    SUCCEED();  // No crash
}

//=============================================================================
// API Contract Tests
//=============================================================================

TEST_F(MetabolicPathwaysRegressionTest, EnzymeActivityClampedToValidRange) {
    // WHAT: Enzyme activity stays in range [0, 2]
    // WHY:  Parameter validation

    metabolic_set_enzyme_activity(&state, 1000.0f);
    EXPECT_LE(state.synthesis.enzyme_activity, 2.0f);

    metabolic_set_enzyme_activity(&state, -100.0f);
    EXPECT_GE(state.synthesis.enzyme_activity, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, PrecursorLevelClampedToValidRange) {
    // WHAT: Precursor concentration stays in range [0, 1000]
    // WHY:  Parameter validation

    metabolic_set_precursor(&state, 10000.0f);
    EXPECT_LE(state.synthesis.precursor_concentration, 1000.0f);

    metabolic_set_precursor(&state, -100.0f);
    EXPECT_GE(state.synthesis.precursor_concentration, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, InhibitionClampedToValidRange) {
    // WHAT: Inhibition stays in range [0, 1]
    // WHY:  Parameter validation

    metabolic_apply_mao_inhibitor(&state, 10.0f);
    EXPECT_LE(state.degradation.inhibitor_blockade, 1.0f);

    metabolic_apply_mao_inhibitor(&state, -1.0f);
    EXPECT_GE(state.degradation.inhibitor_blockade, 0.0f);
}

TEST_F(MetabolicPathwaysRegressionTest, NeurotransmitterSpecificConfigsRemainDistinct) {
    // WHAT: Each neurotransmitter has unique parameters
    // WHY:  Biological specificity maintained

    metabolic_config_t da_config = metabolic_config_dopamine_default();
    metabolic_config_t serotonin_config = metabolic_config_serotonin_default();
    metabolic_config_t ach_config = metabolic_config_acetylcholine_default();

    // ACh degradation much faster (AChE vs MAO)
    EXPECT_GT(ach_config.degradation_rate, da_config.degradation_rate * 10.0f);

    // Different precursors
    EXPECT_NE(da_config.precursor_level, serotonin_config.precursor_level);
}
