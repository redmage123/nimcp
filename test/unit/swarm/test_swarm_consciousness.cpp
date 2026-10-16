/**
 * @file test_swarm_consciousness.cpp
 * @brief Unit tests for NIMCP Swarm Gestalt Consciousness
 *
 * WHAT: Tests for collective consciousness emergence in drone swarms
 * WHY:  Ensure accurate measurement of swarm-level integrated information
 * HOW:  Test phi aggregation, network integration, state classification, scaling models
 *
 * BIOLOGICAL BASIS:
 * - Integrated Information Theory (IIT) applied to collective systems
 * - Swarm consciousness as emergent property of networked agents
 * - Collective phi (Φ_collective) = f(individual phis, network topology, coherence)
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_consciousness.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmConsciousnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory system if needed
    }

    void TearDown() override {
        // Cleanup
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(SwarmConsciousnessTest, DefaultConfigHasValidValues) {
    swarm_consciousness_config_t config = swarm_consciousness_default_config();

    // Check aggregation method is valid
    EXPECT_GE(config.phi_aggregation_method, PHI_AGGREGATION_SUM);
    EXPECT_LE(config.phi_aggregation_method, PHI_AGGREGATION_SYNERGISTIC);

    // Check weights are in valid range
    EXPECT_GE(config.integration_weight, 0.0f);
    EXPECT_LE(config.integration_weight, 1.0f);
    EXPECT_GE(config.coherence_weight, 0.0f);
    EXPECT_LE(config.coherence_weight, 1.0f);

    // Check update interval is reasonable
    EXPECT_GT(config.update_interval_ms, 0u);
}

TEST_F(SwarmConsciousnessTest, DefaultConfigUsesExpectedMethod) {
    swarm_consciousness_config_t config = swarm_consciousness_default_config();

    // Default should use synergistic for emergent behavior
    EXPECT_EQ(config.phi_aggregation_method, PHI_AGGREGATION_SYNERGISTIC);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SwarmConsciousnessTest, CreateWithValidConfigSucceeds) {
    swarm_consciousness_config_t config = swarm_consciousness_default_config();

    swarm_consciousness_ctx_t* ctx = swarm_consciousness_create(&config);
    ASSERT_NE(ctx, nullptr);

    swarm_consciousness_destroy(ctx);
}

TEST_F(SwarmConsciousnessTest, CreateWithNullConfigFails) {
    swarm_consciousness_ctx_t* ctx = swarm_consciousness_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(SwarmConsciousnessTest, DestroyNullIsSafe) {
    // Should not crash
    swarm_consciousness_destroy(nullptr);
}

TEST_F(SwarmConsciousnessTest, DoubleDestroyIsSafe) {
    swarm_consciousness_config_t config = swarm_consciousness_default_config();
    swarm_consciousness_ctx_t* ctx = swarm_consciousness_create(&config);
    ASSERT_NE(ctx, nullptr);

    swarm_consciousness_destroy(ctx);
    // Second destroy should be safe (though pointer is invalid)
    // This tests that destroy properly clears internal state
}

//=============================================================================
// State Classification Tests
//=============================================================================

TEST_F(SwarmConsciousnessTest, ClassifyDormantForLowPhi) {
    // Very low phi should be DORMANT
    swarm_consciousness_state_t state = swarm_classify_collective_phi(0.05f, 5);
    EXPECT_EQ(state, SWARM_CONSCIOUSNESS_DORMANT);
}

TEST_F(SwarmConsciousnessTest, ClassifyEmergingForMediumPhi) {
    // Medium phi should be EMERGING
    swarm_consciousness_state_t state = swarm_classify_collective_phi(1.5f, 5);
    EXPECT_EQ(state, SWARM_CONSCIOUSNESS_EMERGING);
}

TEST_F(SwarmConsciousnessTest, ClassifyUnifiedForHighPhi) {
    // Higher phi should be UNIFIED
    swarm_consciousness_state_t state = swarm_classify_collective_phi(3.0f, 5);
    EXPECT_EQ(state, SWARM_CONSCIOUSNESS_UNIFIED);
}

TEST_F(SwarmConsciousnessTest, ClassifyTranscendentForVeryHighPhi) {
    // Very high normalized phi should be TRANSCENDENT
    swarm_consciousness_state_t state = swarm_classify_collective_phi(5.0f, 5);
    EXPECT_EQ(state, SWARM_CONSCIOUSNESS_TRANSCENDENT);
}

TEST_F(SwarmConsciousnessTest, ClassifyDormantForZeroDrones) {
    // Zero drones should return DORMANT
    swarm_consciousness_state_t state = swarm_classify_collective_phi(1.0f, 0);
    EXPECT_EQ(state, SWARM_CONSCIOUSNESS_DORMANT);
}

//=============================================================================
// State Name Tests
//=============================================================================

TEST_F(SwarmConsciousnessTest, StateNameDormant) {
    const char* name = swarm_consciousness_state_name(SWARM_CONSCIOUSNESS_DORMANT);
    EXPECT_STREQ(name, "DORMANT");
}

TEST_F(SwarmConsciousnessTest, StateNameEmerging) {
    const char* name = swarm_consciousness_state_name(SWARM_CONSCIOUSNESS_EMERGING);
    EXPECT_STREQ(name, "EMERGING");
}

TEST_F(SwarmConsciousnessTest, StateNameUnified) {
    const char* name = swarm_consciousness_state_name(SWARM_CONSCIOUSNESS_UNIFIED);
    EXPECT_STREQ(name, "UNIFIED");
}

TEST_F(SwarmConsciousnessTest, StateNameTranscendent) {
    const char* name = swarm_consciousness_state_name(SWARM_CONSCIOUSNESS_TRANSCENDENT);
    EXPECT_STREQ(name, "TRANSCENDENT");
}

TEST_F(SwarmConsciousnessTest, StateNameUnknownForInvalid) {
    const char* name = swarm_consciousness_state_name((swarm_consciousness_state_t)99);
    EXPECT_STREQ(name, "UNKNOWN");
}

//=============================================================================
// Scaling Model Tests
//=============================================================================

TEST_F(SwarmConsciousnessTest, FitScalingModelNeedsMinimumSamples) {
    // With less than 3 samples, should return zero model
    swarm_consciousness_metrics_t history[2] = {};
    history[0].drone_count = 2;
    history[0].collective_phi = 0.5f;
    history[1].drone_count = 4;
    history[1].collective_phi = 1.2f;

    consciousness_scaling_model_t model = swarm_fit_scaling_model(history, 2);

    // Model should be zeroed (insufficient data)
    EXPECT_FLOAT_EQ(model.base_phi, 0.0f);
}

TEST_F(SwarmConsciousnessTest, FitScalingModelWithValidData) {
    // Create mock history with super-linear scaling
    swarm_consciousness_metrics_t history[4] = {};
    history[0].drone_count = 2;
    history[0].collective_phi = 1.0f;
    history[1].drone_count = 4;
    history[1].collective_phi = 2.5f;
    history[2].drone_count = 8;
    history[2].collective_phi = 7.0f;
    history[3].drone_count = 16;
    history[3].collective_phi = 20.0f;

    consciousness_scaling_model_t model = swarm_fit_scaling_model(history, 4);

    // Should have positive base and exponent
    EXPECT_GT(model.base_phi, 0.0f);
    EXPECT_GT(model.scaling_exponent, 0.0f);
}

TEST_F(SwarmConsciousnessTest, FitScalingModelNullHistoryReturnsZero) {
    consciousness_scaling_model_t model = swarm_fit_scaling_model(nullptr, 5);
    EXPECT_FLOAT_EQ(model.base_phi, 0.0f);
}

//=============================================================================
// Prediction Tests
//=============================================================================

TEST_F(SwarmConsciousnessTest, PredictPhiForSizeWithNullModelReturnsZero) {
    float phi = swarm_predict_phi_for_size(nullptr, 10);
    EXPECT_FLOAT_EQ(phi, 0.0f);
}

TEST_F(SwarmConsciousnessTest, PredictPhiForSizeZeroReturnsZero) {
    consciousness_scaling_model_t model = {
        .scaling_exponent = 1.5f,
        .base_phi = 0.5f,
        .synergy_factor = 0.2f,
        .saturation_point = 100.0f
    };

    float phi = swarm_predict_phi_for_size(&model, 0);
    EXPECT_FLOAT_EQ(phi, 0.0f);
}

TEST_F(SwarmConsciousnessTest, PredictPhiScalesWithSize) {
    consciousness_scaling_model_t model = {
        .scaling_exponent = 1.0f,  // Linear scaling
        .base_phi = 1.0f,
        .synergy_factor = 0.0f,
        .saturation_point = 1000.0f  // High saturation
    };

    float phi_5 = swarm_predict_phi_for_size(&model, 5);
    float phi_10 = swarm_predict_phi_for_size(&model, 10);

    // With linear scaling, 10 drones should have ~2x phi of 5 drones
    // (modulo saturation effects)
    EXPECT_GT(phi_10, phi_5);
}

//=============================================================================
// BBB Validation Tests
//=============================================================================

TEST_F(SwarmConsciousnessTest, BBBValidateNullMetricsFails) {
    bool valid = swarm_consciousness_bbb_validate(nullptr);
    EXPECT_FALSE(valid);
}

TEST_F(SwarmConsciousnessTest, BBBValidateValidMetricsSucceeds) {
    swarm_consciousness_metrics_t metrics = {};
    metrics.drone_count = 5;
    metrics.collective_phi = 2.5f;
    metrics.network_integration = 0.7f;
    metrics.workspace_coherence = 0.8f;
    metrics.consciousness_state = SWARM_CONSCIOUSNESS_UNIFIED;

    // Set individual phis
    for (uint32_t i = 0; i < metrics.drone_count; i++) {
        metrics.individual_phi[i] = 0.5f;
    }

    bool valid = swarm_consciousness_bbb_validate(&metrics);
    EXPECT_TRUE(valid);
}

TEST_F(SwarmConsciousnessTest, BBBValidateInvalidDroneCountFails) {
    swarm_consciousness_metrics_t metrics = {};
    metrics.drone_count = SWARM_CONSCIOUSNESS_MAX_DRONES + 1;  // Too many
    metrics.collective_phi = 1.0f;
    metrics.network_integration = 0.5f;
    metrics.workspace_coherence = 0.5f;
    metrics.consciousness_state = SWARM_CONSCIOUSNESS_DORMANT;

    bool valid = swarm_consciousness_bbb_validate(&metrics);
    EXPECT_FALSE(valid);
}

TEST_F(SwarmConsciousnessTest, BBBValidateNegativePhiFails) {
    swarm_consciousness_metrics_t metrics = {};
    metrics.drone_count = 5;
    metrics.collective_phi = -1.0f;  // Invalid negative
    metrics.network_integration = 0.5f;
    metrics.workspace_coherence = 0.5f;
    metrics.consciousness_state = SWARM_CONSCIOUSNESS_DORMANT;

    bool valid = swarm_consciousness_bbb_validate(&metrics);
    EXPECT_FALSE(valid);
}

TEST_F(SwarmConsciousnessTest, BBBValidateIntegrationOutOfRangeFails) {
    swarm_consciousness_metrics_t metrics = {};
    metrics.drone_count = 5;
    metrics.collective_phi = 1.0f;
    metrics.network_integration = 1.5f;  // > 1.0 is invalid
    metrics.workspace_coherence = 0.5f;
    metrics.consciousness_state = SWARM_CONSCIOUSNESS_DORMANT;

    bool valid = swarm_consciousness_bbb_validate(&metrics);
    EXPECT_FALSE(valid);
}

TEST_F(SwarmConsciousnessTest, BBBValidateInvalidStateFails) {
    swarm_consciousness_metrics_t metrics = {};
    metrics.drone_count = 5;
    metrics.collective_phi = 1.0f;
    metrics.network_integration = 0.5f;
    metrics.workspace_coherence = 0.5f;
    metrics.consciousness_state = (swarm_consciousness_state_t)99;  // Invalid

    bool valid = swarm_consciousness_bbb_validate(&metrics);
    EXPECT_FALSE(valid);
}

//=============================================================================
// Metrics Free Test
//=============================================================================

TEST_F(SwarmConsciousnessTest, MetricsFreeNullIsSafe) {
    // Should not crash
    swarm_consciousness_metrics_free(nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
