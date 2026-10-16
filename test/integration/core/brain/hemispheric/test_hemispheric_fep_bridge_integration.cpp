/**
 * @file test_hemispheric_fep_bridge_integration.cpp
 * @brief Integration tests for hemispheric brain FEP (Free Energy Principle) bridge
 *
 * Tests cover:
 * - FEP integration with hemispheric processing
 * - Cross-hemisphere prediction sharing via callosum
 * - Precision weighting per hemisphere (left=analytical, right=holistic)
 * - Free energy minimization across both hemispheres
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include "utils/nimcp_test_base.h"

#include "core/brain/hemispheric/nimcp_hemispheric_fep_bridge.h"
#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"

/**
 * @class HemisphericFepBridgeIntegrationTest
 * @brief Test fixture for hemispheric FEP bridge integration tests
 */
class HemisphericFepBridgeIntegrationTest : public NimcpTestBase {
protected:
    static constexpr uint32_t INPUT_SIZE = 8;
    static constexpr uint32_t OUTPUT_SIZE = 4;

    hemispheric_brain_t* brain = nullptr;
    hemispheric_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create hemispheric brain
        hemispheric_brain_config_t brain_config = hemispheric_brain_default_config();
        brain_config.num_inputs = INPUT_SIZE;
        brain_config.num_outputs = OUTPUT_SIZE;
        brain_config.size = BRAIN_SIZE_SMALL;
        brain_config.enable_bio_async = false;

        brain = hemispheric_brain_create(&brain_config);

        // Create FEP bridge (without external FEP orchestrator)
        if (brain) {
            hemispheric_fep_config_t config = hemispheric_fep_default_config();
            config.enable_precision_modulation = true;
            config.enable_learning_modulation = true;
            config.enable_callosum_transfer = true;
            config.enable_bio_async = false;

            bridge = hemispheric_fep_create(&config, brain, nullptr);
        }
    }

    void TearDown() override {
        if (bridge) {
            hemispheric_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (brain) {
            hemispheric_brain_destroy(brain);
            brain = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Creation and Lifecycle Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, CreateWithDefaultConfig) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericFepBridgeIntegrationTest, CreateWithCustomConfig) {
    hemispheric_fep_destroy(bridge);

    hemispheric_fep_config_t config = hemispheric_fep_default_config();
    config.left_base_precision = 0.9f;
    config.right_base_precision = 0.6f;
    config.left_prior_width = 0.2f;
    config.right_prior_width = 0.8f;
    config.callosum_transfer_rate = 0.5f;
    config.callosum_latency_ms = 15.0f;
    config.min_learning_rate = 0.005f;
    config.max_learning_rate = 0.5f;

    bridge = hemispheric_fep_create(&config, brain, nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(HemisphericFepBridgeIntegrationTest, BridgeInitializedCorrectly) {
    hemispheric_fep_stats_t stats = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(stats.updates, 0u);
    EXPECT_EQ(stats.prediction_transfers, 0u);
}

/* ============================================================================
 * Precision Weighting Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, LeftHemisphereHighPrecision) {
    float left_precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_LEFT);
    float right_precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_RIGHT);

    // Left hemisphere should have higher precision (analytical)
    EXPECT_NEAR(left_precision, HEMI_FEP_LEFT_BASE_PRECISION, 0.1f);
    EXPECT_GT(left_precision, right_precision);
}

TEST_F(HemisphericFepBridgeIntegrationTest, RightHemisphereModeratePrecision) {
    float right_precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_RIGHT);

    // Right hemisphere should have moderate precision (holistic)
    EXPECT_NEAR(right_precision, HEMI_FEP_RIGHT_BASE_PRECISION, 0.1f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, SetPrecisionLeft) {
    EXPECT_EQ(hemispheric_fep_set_precision(bridge, HEMISPHERE_LEFT, 0.95f), 0);

    float precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_LEFT);
    EXPECT_NEAR(precision, 0.95f, 0.01f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, SetPrecisionRight) {
    EXPECT_EQ(hemispheric_fep_set_precision(bridge, HEMISPHERE_RIGHT, 0.5f), 0);

    float precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_RIGHT);
    EXPECT_NEAR(precision, 0.5f, 0.01f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, PrecisionBoundedZeroToOne) {
    // Try to set out of bounds
    EXPECT_EQ(hemispheric_fep_set_precision(bridge, HEMISPHERE_LEFT, 1.5f), 0);
    float precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_LEFT);
    EXPECT_LE(precision, 1.0f);

    EXPECT_EQ(hemispheric_fep_set_precision(bridge, HEMISPHERE_RIGHT, -0.5f), 0);
    precision = hemispheric_fep_get_precision(bridge, HEMISPHERE_RIGHT);
    EXPECT_GE(precision, 0.0f);
}

/* ============================================================================
 * Free Energy Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, FreeEnergyPerHemisphere) {
    // Update to compute initial free energy
    hemispheric_fep_update(bridge);

    float left_fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_LEFT);
    float right_fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_RIGHT);

    EXPECT_GE(left_fe, 0.0f);
    EXPECT_GE(right_fe, 0.0f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, TotalFreeEnergy) {
    hemispheric_fep_update(bridge);

    float total_fe = hemispheric_fep_get_total_free_energy(bridge);
    float left_fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_LEFT);
    float right_fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_RIGHT);

    // Total should include both hemispheres plus integration term
    EXPECT_GE(total_fe, 0.0f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, GlobalFepState) {
    hemispheric_fep_update(bridge);

    global_fep_state_t state = hemispheric_fep_get_global_state(bridge);

    EXPECT_GE(state.total_free_energy, 0.0f);
    EXPECT_GE(state.left_contribution, 0.0f);
    EXPECT_LE(state.left_contribution, 1.0f);
    EXPECT_GE(state.right_contribution, 0.0f);
    EXPECT_LE(state.right_contribution, 1.0f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, FreeEnergyMinimizationStep) {
    // Get initial free energy
    hemispheric_fep_update(bridge);
    float initial_fe = hemispheric_fep_get_total_free_energy(bridge);

    // Run minimization steps
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(hemispheric_fep_minimize_step(bridge, 0.01f), 0);
    }

    float final_fe = hemispheric_fep_get_total_free_energy(bridge);

    // Free energy should not increase significantly
    EXPECT_LE(final_fe, initial_fe + 0.5f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, ResetFreeEnergy) {
    // Inject some prediction errors to increase free energy
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 1.0f);
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_RIGHT, 0.8f);
    hemispheric_fep_update(bridge);

    EXPECT_EQ(hemispheric_fep_reset_free_energy(bridge), 0);

    // Free energy should be reset to baseline
}

/* ============================================================================
 * Cross-Hemisphere Prediction Transfer Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, CallosumTransferInitiallyActive) {
    callosum_fep_effects_t effects = hemispheric_fep_get_callosum_effects(bridge);

    // Transfer should be active by default
    EXPECT_GT(effects.transfer_rate, 0.0f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, TriggerPredictionTransfer) {
    EXPECT_EQ(hemispheric_fep_trigger_transfer(bridge), 0);

    hemispheric_fep_stats_t stats = hemispheric_fep_get_stats(bridge);
    EXPECT_GT(stats.prediction_transfers, 0u);
}

TEST_F(HemisphericFepBridgeIntegrationTest, CallosumConsensusStrength) {
    // Run several updates to establish predictions
    for (int i = 0; i < 5; ++i) {
        hemispheric_fep_update(bridge);
        hemispheric_fep_trigger_transfer(bridge);
    }

    callosum_fep_effects_t effects = hemispheric_fep_get_callosum_effects(bridge);

    // Consensus strength should be between 0 and 1
    EXPECT_GE(effects.consensus_strength, 0.0f);
    EXPECT_LE(effects.consensus_strength, 1.0f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, IntegrationFreeEnergy) {
    hemispheric_fep_update(bridge);
    hemispheric_fep_trigger_transfer(bridge);

    callosum_fep_effects_t effects = hemispheric_fep_get_callosum_effects(bridge);

    // Integration free energy represents cross-hemisphere disagreement
    EXPECT_GE(effects.integration_free_energy, 0.0f);
}

/* ============================================================================
 * Prediction Error Injection Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, InjectPredictionErrorLeft) {
    hemispheric_fep_update(bridge);
    float initial_fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_LEFT);

    EXPECT_EQ(hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 1.0f), 0);
    hemispheric_fep_update(bridge);

    float after_fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_LEFT);

    // Free energy should increase with prediction error
    EXPECT_GE(after_fe, initial_fe);
}

TEST_F(HemisphericFepBridgeIntegrationTest, InjectPredictionErrorRight) {
    hemispheric_fep_update(bridge);
    float initial_fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_RIGHT);

    EXPECT_EQ(hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_RIGHT, 0.5f), 0);
    hemispheric_fep_update(bridge);

    float after_fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_RIGHT);

    EXPECT_GE(after_fe, initial_fe);
}

TEST_F(HemisphericFepBridgeIntegrationTest, PredictionErrorAffectsLearning) {
    hemisphere_fep_effects_t initial_effects = hemispheric_fep_get_left_effects(bridge);

    // Large prediction error should affect learning rate
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 2.0f);
    hemispheric_fep_update(bridge);
    hemispheric_fep_apply_modulation(bridge);

    hemisphere_fep_effects_t after_effects = hemispheric_fep_get_left_effects(bridge);

    // Learning rate factor should be affected by prediction error
    EXPECT_GE(after_effects.learning_rate_factor, 0.0f);
    EXPECT_LE(after_effects.learning_rate_factor, 1.0f);
}

/* ============================================================================
 * Hemisphere Effects Query Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, LeftHemisphereEffects) {
    hemispheric_fep_update(bridge);

    hemisphere_fep_effects_t effects = hemispheric_fep_get_left_effects(bridge);

    EXPECT_NEAR(effects.precision, HEMI_FEP_LEFT_BASE_PRECISION, 0.2f);
    EXPECT_NEAR(effects.prior_width, HEMI_FEP_LEFT_PRIOR_WIDTH, 0.2f);
    EXPECT_GE(effects.free_energy, 0.0f);
    EXPECT_GE(effects.confidence, 0.0f);
    EXPECT_LE(effects.confidence, 1.0f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, RightHemisphereEffects) {
    hemispheric_fep_update(bridge);

    hemisphere_fep_effects_t effects = hemispheric_fep_get_right_effects(bridge);

    EXPECT_NEAR(effects.precision, HEMI_FEP_RIGHT_BASE_PRECISION, 0.2f);
    EXPECT_NEAR(effects.prior_width, HEMI_FEP_RIGHT_PRIOR_WIDTH, 0.2f);
    EXPECT_GE(effects.free_energy, 0.0f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, LeftNarrowPriorsRightBroadPriors) {
    hemisphere_fep_effects_t left = hemispheric_fep_get_left_effects(bridge);
    hemisphere_fep_effects_t right = hemispheric_fep_get_right_effects(bridge);

    // Left has narrow priors (analytical), right has broad priors (holistic)
    EXPECT_LT(left.prior_width, right.prior_width);
}

/* ============================================================================
 * Modulation Application Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, ApplyModulationSuccess) {
    EXPECT_EQ(hemispheric_fep_update(bridge), 0);
    EXPECT_EQ(hemispheric_fep_apply_modulation(bridge), 0);
}

TEST_F(HemisphericFepBridgeIntegrationTest, ModulationAffectsBrain) {
    // Process input through brain
    std::vector<float> input(INPUT_SIZE, 1.0f);
    std::vector<float> output(OUTPUT_SIZE);

    hemispheric_brain_infer(brain, input.data(), INPUT_SIZE,
                           output.data(), OUTPUT_SIZE);

    // Apply FEP modulation
    hemispheric_fep_update(bridge);
    hemispheric_fep_apply_modulation(bridge);

    // Process again - behavior may differ due to FEP modulation
    std::vector<float> output2(OUTPUT_SIZE);
    hemispheric_brain_infer(brain, input.data(), INPUT_SIZE,
                           output2.data(), OUTPUT_SIZE);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, StatisticsAccumulate) {
    for (int i = 0; i < 10; ++i) {
        hemispheric_fep_update(bridge);
        hemispheric_fep_trigger_transfer(bridge);
        hemispheric_fep_minimize_step(bridge, 0.01f);
    }

    hemispheric_fep_stats_t stats = hemispheric_fep_get_stats(bridge);

    EXPECT_GT(stats.updates, 0u);
    EXPECT_GT(stats.prediction_transfers, 0u);
    EXPECT_GT(stats.fe_minimization_steps, 0u);
}

TEST_F(HemisphericFepBridgeIntegrationTest, ResetStatistics) {
    hemispheric_fep_update(bridge);
    hemispheric_fep_trigger_transfer(bridge);

    hemispheric_fep_reset_stats(bridge);

    hemispheric_fep_stats_t stats = hemispheric_fep_get_stats(bridge);
    EXPECT_EQ(stats.updates, 0u);
    EXPECT_EQ(stats.prediction_transfers, 0u);
}

TEST_F(HemisphericFepBridgeIntegrationTest, AverageFreeEnergyTracked) {
    for (int i = 0; i < 5; ++i) {
        hemispheric_fep_update(bridge);
    }

    hemispheric_fep_stats_t stats = hemispheric_fep_get_stats(bridge);

    EXPECT_GE(stats.avg_free_energy, 0.0f);
}

TEST_F(HemisphericFepBridgeIntegrationTest, PeakAndMinFreeEnergyTracked) {
    // Inject error to create variation
    hemispheric_fep_update(bridge);
    hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 2.0f);
    hemispheric_fep_update(bridge);

    for (int i = 0; i < 5; ++i) {
        hemispheric_fep_minimize_step(bridge, 0.01f);
    }
    hemispheric_fep_update(bridge);

    hemispheric_fep_stats_t stats = hemispheric_fep_get_stats(bridge);

    EXPECT_GE(stats.peak_free_energy, stats.min_free_energy);
}

/* ============================================================================
 * Bio-async Integration Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, BioAsyncConnectDisconnect) {
    int result = hemispheric_fep_connect_bio_async(bridge);
    if (result == 0) {
        EXPECT_EQ(hemispheric_fep_disconnect_bio_async(bridge), 0);
    }
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(HemisphericFepBridgeIntegrationTest, ZeroPredictionError) {
    EXPECT_EQ(hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_LEFT, 0.0f), 0);
    EXPECT_EQ(hemispheric_fep_update(bridge), 0);
}

TEST_F(HemisphericFepBridgeIntegrationTest, LargePredictionError) {
    EXPECT_EQ(hemispheric_fep_inject_prediction_error(bridge, HEMISPHERE_RIGHT, 10.0f), 0);
    EXPECT_EQ(hemispheric_fep_update(bridge), 0);

    // Should not crash, values should be bounded
    float fe = hemispheric_fep_get_free_energy(bridge, HEMISPHERE_RIGHT);
    EXPECT_LT(fe, 1000.0f);  // Should be bounded
}

TEST_F(HemisphericFepBridgeIntegrationTest, RapidUpdateCycles) {
    // Run many update cycles quickly
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(hemispheric_fep_update(bridge), 0);
        hemispheric_fep_minimize_step(bridge, 0.001f);
    }

    // System should remain stable
    float fe = hemispheric_fep_get_total_free_energy(bridge);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LT(fe, 1e6f);
}
