/**
 * @file test_dragonfly_fep_bridge.cpp
 * @brief Unit tests for dragonfly FEP (Free Energy Principle) bridge module
 *
 * Tests FEP-dragonfly integration including generative models, active inference,
 * prediction error computation, and precision weighting.
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_fep_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonFEPBridgeTest : public ::testing::Test {
protected:
    dragonfly_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        dragonfly_fep_config_t config;
        dragonfly_fep_bridge_default_config(&config);
        bridge = dragonfly_fep_bridge_create(nullptr, nullptr, &config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, DefaultConfig) {
    dragonfly_fep_config_t config;
    EXPECT_EQ(dragonfly_fep_bridge_default_config(&config), 0);

    // Model settings
    EXPECT_EQ(config.default_model, FEP_MODEL_CONSTANT_VELOCITY);
    EXPECT_TRUE(config.auto_model_selection);
    EXPECT_GT(config.model_evidence_threshold, 0.0f);

    // Precision settings
    EXPECT_EQ(config.precision_mode, FEP_PRECISION_ADAPTIVE);
    EXPECT_GT(config.sensory_precision, 0.0f);
    EXPECT_GT(config.proprioceptive_precision, 0.0f);
    EXPECT_GT(config.prior_precision, 0.0f);

    // Inference settings
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_LE(config.learning_rate, 1.0f);
    EXPECT_GT(config.inference_steps, 0u);
    EXPECT_GT(config.action_precision, 0.0f);

    // Integration settings
    EXPECT_GT(config.prediction_horizon_ms, 0.0f);
}

TEST_F(DragonFEPBridgeTest, ValidateConfig) {
    dragonfly_fep_config_t config;
    dragonfly_fep_bridge_default_config(&config);
    EXPECT_EQ(dragonfly_fep_bridge_validate_config(&config), 0);

    // Null config
    EXPECT_NE(dragonfly_fep_bridge_validate_config(nullptr), 0);
}

TEST_F(DragonFEPBridgeTest, InvalidConfig) {
    dragonfly_fep_config_t config;
    dragonfly_fep_bridge_default_config(&config);

    // Invalid learning rate
    config.learning_rate = 0.0f;
    EXPECT_NE(dragonfly_fep_bridge_validate_config(&config), 0);

    // Invalid learning rate (too high)
    config.learning_rate = 1.5f;
    EXPECT_NE(dragonfly_fep_bridge_validate_config(&config), 0);
}

TEST_F(DragonFEPBridgeTest, CreateWithCustomConfig) {
    dragonfly_fep_config_t config;
    dragonfly_fep_bridge_default_config(&config);
    config.default_model = FEP_MODEL_MANEUVERING;
    config.learning_rate = 0.05f;

    dragonfly_fep_bridge_t* custom = dragonfly_fep_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(custom, nullptr);
    dragonfly_fep_bridge_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, CreateAndDestroy) {
    dragonfly_fep_bridge_t* b = dragonfly_fep_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    dragonfly_fep_bridge_destroy(b);
}

TEST_F(DragonFEPBridgeTest, DestroyNull) {
    dragonfly_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(DragonFEPBridgeTest, Reset) {
    // First compute some errors to change state
    float observations[6] = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f};
    dragonfly_fep_errors_t errors;
    dragonfly_fep_compute_errors(bridge, observations, 6, &errors);

    // Reset
    EXPECT_EQ(dragonfly_fep_bridge_reset(bridge), 0);

    // Verify reset state
    dragonfly_fep_stats_t stats;
    EXPECT_EQ(dragonfly_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.inference_steps_total, 0u);
    EXPECT_EQ(stats.predictions_made, 0u);
}

TEST_F(DragonFEPBridgeTest, ResetNull) {
    EXPECT_NE(dragonfly_fep_bridge_reset(nullptr), 0);
}

//=============================================================================
// Prediction Error Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, ComputeErrors) {
    float observations[6] = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f};
    dragonfly_fep_errors_t errors;

    EXPECT_EQ(dragonfly_fep_compute_errors(bridge, observations, 6, &errors), 0);

    // Initial errors should be non-zero since beliefs start at 0
    EXPECT_GT(errors.sensory_error, 0.0f);
    EXPECT_GE(errors.model_error, 0.0f);
    EXPECT_GE(errors.total_free_energy, 0.0f);
    EXPECT_GE(errors.precision_weighted_error, 0.0f);
}

TEST_F(DragonFEPBridgeTest, ComputeErrorsNull) {
    float observations[6] = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f};
    dragonfly_fep_errors_t errors;

    EXPECT_NE(dragonfly_fep_compute_errors(nullptr, observations, 6, &errors), 0);
    EXPECT_NE(dragonfly_fep_compute_errors(bridge, nullptr, 6, &errors), 0);
    EXPECT_NE(dragonfly_fep_compute_errors(bridge, observations, 6, nullptr), 0);
}

TEST_F(DragonFEPBridgeTest, GetFreeEnergy) {
    float initial_fe = dragonfly_fep_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f);

    // After observation, free energy should be calculated
    float observations[6] = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f};
    dragonfly_fep_errors_t errors;
    dragonfly_fep_compute_errors(bridge, observations, 6, &errors);

    float fe = dragonfly_fep_get_free_energy(bridge);
    EXPECT_GT(fe, 0.0f);
}

TEST_F(DragonFEPBridgeTest, GetSurprise) {
    float surprise = dragonfly_fep_get_surprise(bridge);
    EXPECT_GE(surprise, 0.0f);
}

TEST_F(DragonFEPBridgeTest, UpdatePrecision) {
    EXPECT_EQ(dragonfly_fep_update_precision(bridge, 0.9f, 0.8f), 0);
    EXPECT_NE(dragonfly_fep_update_precision(nullptr, 0.9f, 0.8f), 0);
}

//=============================================================================
// Active Inference Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, InferState) {
    float observations[6] = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f};
    dragonfly_fep_inference_t inference;

    EXPECT_EQ(dragonfly_fep_infer_state(bridge, observations, 6, &inference), 0);

    // Should have updated beliefs
    EXPECT_EQ(inference.state_dim, 6u);
    // Beliefs should move toward observations
    for (uint32_t i = 0; i < 6; i++) {
        EXPECT_GE(inference.precision[i], 0.0f);
    }
}

TEST_F(DragonFEPBridgeTest, InferStateNull) {
    float observations[6] = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f};
    dragonfly_fep_inference_t inference;

    EXPECT_NE(dragonfly_fep_infer_state(nullptr, observations, 6, &inference), 0);
    EXPECT_NE(dragonfly_fep_infer_state(bridge, nullptr, 6, &inference), 0);
    EXPECT_NE(dragonfly_fep_infer_state(bridge, observations, 6, nullptr), 0);
}

TEST_F(DragonFEPBridgeTest, SelectAction) {
    dragonfly_fep_action_t action;
    float action_value;

    EXPECT_EQ(dragonfly_fep_select_action(bridge, &action, &action_value), 0);

    // Action should be valid
    EXPECT_GE((int)action, FEP_ACTION_OBSERVE);
    EXPECT_LE((int)action, FEP_ACTION_PREDICT);
}

TEST_F(DragonFEPBridgeTest, SelectActionNull) {
    dragonfly_fep_action_t action;

    EXPECT_NE(dragonfly_fep_select_action(nullptr, &action, nullptr), 0);
    EXPECT_NE(dragonfly_fep_select_action(bridge, nullptr, nullptr), 0);
}

TEST_F(DragonFEPBridgeTest, ApplyAction) {
    EXPECT_EQ(dragonfly_fep_apply_action(bridge, FEP_ACTION_OBSERVE), 0);
    EXPECT_EQ(dragonfly_fep_apply_action(bridge, FEP_ACTION_PURSUIT), 0);
    EXPECT_EQ(dragonfly_fep_apply_action(bridge, FEP_ACTION_INTERCEPT), 0);
    EXPECT_EQ(dragonfly_fep_apply_action(bridge, FEP_ACTION_PREDICT), 0);
}

TEST_F(DragonFEPBridgeTest, ApplyActionNull) {
    EXPECT_NE(dragonfly_fep_apply_action(nullptr, FEP_ACTION_OBSERVE), 0);
}

TEST_F(DragonFEPBridgeTest, ExpectedFreeEnergy) {
    float efe_observe = dragonfly_fep_expected_free_energy(bridge, FEP_ACTION_OBSERVE);
    float efe_pursuit = dragonfly_fep_expected_free_energy(bridge, FEP_ACTION_PURSUIT);
    float efe_intercept = dragonfly_fep_expected_free_energy(bridge, FEP_ACTION_INTERCEPT);
    float efe_predict = dragonfly_fep_expected_free_energy(bridge, FEP_ACTION_PREDICT);

    // All EFE values should be finite
    EXPECT_TRUE(std::isfinite(efe_observe));
    EXPECT_TRUE(std::isfinite(efe_pursuit));
    EXPECT_TRUE(std::isfinite(efe_intercept));
    EXPECT_TRUE(std::isfinite(efe_predict));
}

//=============================================================================
// Generative Model Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, SetModel) {
    EXPECT_EQ(dragonfly_fep_set_model(bridge, FEP_MODEL_LINEAR), 0);
    EXPECT_EQ(dragonfly_fep_set_model(bridge, FEP_MODEL_CONSTANT_VELOCITY), 0);
    EXPECT_EQ(dragonfly_fep_set_model(bridge, FEP_MODEL_CONSTANT_ACCELERATION), 0);
    EXPECT_EQ(dragonfly_fep_set_model(bridge, FEP_MODEL_MANEUVERING), 0);
    EXPECT_EQ(dragonfly_fep_set_model(bridge, FEP_MODEL_EVASIVE), 0);
}

TEST_F(DragonFEPBridgeTest, SetModelNull) {
    EXPECT_NE(dragonfly_fep_set_model(nullptr, FEP_MODEL_LINEAR), 0);
}

TEST_F(DragonFEPBridgeTest, SetModelInvalid) {
    EXPECT_NE(dragonfly_fep_set_model(bridge, (dragonfly_fep_model_t)99), 0);
}

TEST_F(DragonFEPBridgeTest, GetBestModel) {
    dragonfly_fep_model_t best = dragonfly_fep_get_best_model(bridge);

    // Should be a valid model type
    EXPECT_GE((int)best, FEP_MODEL_LINEAR);
    EXPECT_LE((int)best, FEP_MODEL_EVASIVE);
}

TEST_F(DragonFEPBridgeTest, GetModelEvidence) {
    for (int m = FEP_MODEL_LINEAR; m <= FEP_MODEL_EVASIVE; m++) {
        float evidence = dragonfly_fep_get_model_evidence(bridge, (dragonfly_fep_model_t)m);
        EXPECT_GE(evidence, 0.0f);
        EXPECT_LE(evidence, 1.0f);
    }
}

TEST_F(DragonFEPBridgeTest, Predict) {
    // First set some state beliefs by inferring
    float observations[6] = {10.0f, 20.0f, 30.0f, 1.0f, 2.0f, 3.0f};
    dragonfly_fep_inference_t inference;
    dragonfly_fep_infer_state(bridge, observations, 6, &inference);

    // Now predict forward
    float predicted[6];
    int result = dragonfly_fep_predict(bridge, 100.0f, predicted, 6);
    EXPECT_GT(result, 0);
}

TEST_F(DragonFEPBridgeTest, PredictNull) {
    float predicted[6];
    EXPECT_EQ(dragonfly_fep_predict(nullptr, 100.0f, predicted, 6), -1);
    EXPECT_EQ(dragonfly_fep_predict(bridge, 100.0f, nullptr, 6), -1);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, ConnectDragonfly) {
    EXPECT_EQ(dragonfly_fep_connect_dragonfly(bridge, nullptr), 0);
    EXPECT_NE(dragonfly_fep_connect_dragonfly(nullptr, nullptr), 0);
}

TEST_F(DragonFEPBridgeTest, ConnectSystem) {
    EXPECT_EQ(dragonfly_fep_connect_system(bridge, nullptr), 0);
    EXPECT_NE(dragonfly_fep_connect_system(nullptr, nullptr), 0);
}

TEST_F(DragonFEPBridgeTest, Update) {
    EXPECT_EQ(dragonfly_fep_update(bridge, 16.0f), 0);
}

TEST_F(DragonFEPBridgeTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(dragonfly_fep_update(bridge, 16.0f), 0);
    }
}

TEST_F(DragonFEPBridgeTest, UpdateNull) {
    EXPECT_NE(dragonfly_fep_update(nullptr, 16.0f), 0);
}

TEST_F(DragonFEPBridgeTest, SyncWithTracker) {
    EXPECT_EQ(dragonfly_fep_sync_with_tracker(bridge), 0);
    EXPECT_NE(dragonfly_fep_sync_with_tracker(nullptr), 0);
}

TEST_F(DragonFEPBridgeTest, SyncWithTSDN) {
    EXPECT_EQ(dragonfly_fep_sync_with_tsdn(bridge), 0);
    EXPECT_NE(dragonfly_fep_sync_with_tsdn(nullptr), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, GetStats) {
    dragonfly_fep_stats_t stats;
    EXPECT_EQ(dragonfly_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.inference_steps_total, 0u);
    EXPECT_EQ(stats.predictions_made, 0u);
}

TEST_F(DragonFEPBridgeTest, StatsAccumulate) {
    // Perform some operations
    float observations[6] = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f};
    dragonfly_fep_errors_t errors;
    dragonfly_fep_compute_errors(bridge, observations, 6, &errors);

    dragonfly_fep_inference_t inference;
    dragonfly_fep_infer_state(bridge, observations, 6, &inference);

    dragonfly_fep_stats_t stats;
    EXPECT_EQ(dragonfly_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.predictions_made, 0u);
    EXPECT_GT(stats.inference_steps_total, 0u);
}

TEST_F(DragonFEPBridgeTest, ModelSwitchesTracked) {
    // Switch models
    dragonfly_fep_set_model(bridge, FEP_MODEL_LINEAR);
    dragonfly_fep_set_model(bridge, FEP_MODEL_MANEUVERING);
    dragonfly_fep_set_model(bridge, FEP_MODEL_EVASIVE);

    dragonfly_fep_stats_t stats;
    EXPECT_EQ(dragonfly_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.model_switches, 0u);
}

TEST_F(DragonFEPBridgeTest, ResetStats) {
    // Perform operations to build up stats
    float observations[6] = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f};
    dragonfly_fep_errors_t errors;
    dragonfly_fep_compute_errors(bridge, observations, 6, &errors);

    // Reset stats
    EXPECT_EQ(dragonfly_fep_bridge_reset_stats(bridge), 0);

    dragonfly_fep_stats_t stats;
    EXPECT_EQ(dragonfly_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.predictions_made, 0u);
    EXPECT_EQ(stats.inference_steps_total, 0u);
}

TEST_F(DragonFEPBridgeTest, NullStats) {
    dragonfly_fep_stats_t stats;
    EXPECT_NE(dragonfly_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(dragonfly_fep_bridge_get_stats(bridge, nullptr), 0);
    EXPECT_NE(dragonfly_fep_bridge_reset_stats(nullptr), 0);
}

//=============================================================================
// Name Function Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, ModelNames) {
    EXPECT_STREQ(dragonfly_fep_model_name(FEP_MODEL_LINEAR), "linear");
    EXPECT_STREQ(dragonfly_fep_model_name(FEP_MODEL_CONSTANT_VELOCITY), "constant_velocity");
    EXPECT_STREQ(dragonfly_fep_model_name(FEP_MODEL_CONSTANT_ACCELERATION), "constant_acceleration");
    EXPECT_STREQ(dragonfly_fep_model_name(FEP_MODEL_MANEUVERING), "maneuvering");
    EXPECT_STREQ(dragonfly_fep_model_name(FEP_MODEL_EVASIVE), "evasive");
}

TEST_F(DragonFEPBridgeTest, ActionNames) {
    EXPECT_STREQ(dragonfly_fep_action_name(FEP_ACTION_OBSERVE), "observe");
    EXPECT_STREQ(dragonfly_fep_action_name(FEP_ACTION_PURSUIT), "pursuit");
    EXPECT_STREQ(dragonfly_fep_action_name(FEP_ACTION_INTERCEPT), "intercept");
    EXPECT_STREQ(dragonfly_fep_action_name(FEP_ACTION_PREDICT), "predict");
}

TEST_F(DragonFEPBridgeTest, PrecisionModeNames) {
    EXPECT_STREQ(dragonfly_fep_precision_mode_name(FEP_PRECISION_FIXED), "fixed");
    EXPECT_STREQ(dragonfly_fep_precision_mode_name(FEP_PRECISION_ADAPTIVE), "adaptive");
    EXPECT_STREQ(dragonfly_fep_precision_mode_name(FEP_PRECISION_HIERARCHICAL), "hierarchical");
}

//=============================================================================
// Active Inference Workflow Tests
//=============================================================================

TEST_F(DragonFEPBridgeTest, FullInferenceLoop) {
    // Simulate a complete inference cycle
    float dt = 16.0f;

    for (int cycle = 0; cycle < 10; cycle++) {
        // 1. Receive observations
        float observations[6] = {
            10.0f + cycle * 0.5f,  // x moving
            20.0f + cycle * 0.3f,  // y moving
            5.0f,                  // z constant
            0.5f,                  // vx
            0.3f,                  // vy
            0.0f                   // vz
        };

        // 2. Compute prediction errors
        dragonfly_fep_errors_t errors;
        EXPECT_EQ(dragonfly_fep_compute_errors(bridge, observations, 6, &errors), 0);

        // 3. Update beliefs via inference
        dragonfly_fep_inference_t inference;
        EXPECT_EQ(dragonfly_fep_infer_state(bridge, observations, 6, &inference), 0);

        // 4. Select best action
        dragonfly_fep_action_t action;
        float action_value;
        EXPECT_EQ(dragonfly_fep_select_action(bridge, &action, &action_value), 0);

        // 5. Apply action
        EXPECT_EQ(dragonfly_fep_apply_action(bridge, action), 0);

        // 6. Update internal state
        EXPECT_EQ(dragonfly_fep_update(bridge, dt), 0);
    }

    // Verify stats accumulated
    dragonfly_fep_stats_t stats;
    EXPECT_EQ(dragonfly_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.predictions_made, 10u);
    EXPECT_GT(stats.inference_steps_total, 0u);
}

TEST_F(DragonFEPBridgeTest, PredictionAccuracyImproves) {
    // Track that free energy decreases as beliefs converge

    float initial_fe = 0.0f;
    float final_fe = 0.0f;

    // Fixed target position
    float target_pos[6] = {50.0f, 30.0f, 10.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < 50; i++) {
        dragonfly_fep_errors_t errors;
        dragonfly_fep_compute_errors(bridge, target_pos, 6, &errors);

        if (i == 0) {
            initial_fe = errors.total_free_energy;
        }

        dragonfly_fep_inference_t inference;
        dragonfly_fep_infer_state(bridge, target_pos, 6, &inference);

        final_fe = dragonfly_fep_get_free_energy(bridge);
    }

    // Free energy should decrease as beliefs converge
    EXPECT_LT(final_fe, initial_fe);
}
