/**
 * @file test_amygdala_fep_bridge.cpp
 * @brief Unit tests for amygdala FEP bridge module
 *
 * Tests FEP-amygdala integration including threat prediction,
 * active inference for defensive behavior, and precision weighting.
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_amygdala_fep_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AmygdalaFEPBridgeTest : public ::testing::Test {
protected:
    amyg_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        amyg_fep_config_t config;
        amyg_fep_bridge_default_config(&config);
        bridge = amyg_fep_bridge_create(nullptr, nullptr, &config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            amyg_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, DefaultConfig) {
    amyg_fep_config_t config;
    EXPECT_EQ(amyg_fep_bridge_default_config(&config), 0);

    // Model settings
    EXPECT_EQ(config.default_model, AMYG_FEP_MODEL_VIGILANT);
    EXPECT_TRUE(config.auto_model_selection);
    EXPECT_GT(config.model_evidence_threshold, 0.0f);

    // Precision settings
    EXPECT_EQ(config.precision_mode, AMYG_FEP_PRECISION_ADAPTIVE);
    EXPECT_GT(config.sensory_precision, 0.0f);
    EXPECT_GT(config.interoceptive_precision, 0.0f);
    EXPECT_GT(config.contextual_precision, 0.0f);
    EXPECT_GT(config.prior_precision, 0.0f);

    // Arousal/stress modulation
    EXPECT_GT(config.arousal_precision_gain, 0.0f);
    EXPECT_GT(config.stress_precision_gain, 0.0f);

    // Inference settings
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_LE(config.learning_rate, 1.0f);
    EXPECT_GT(config.inference_steps, 0u);

    // Integration settings
    EXPECT_TRUE(config.use_interoception);
    EXPECT_TRUE(config.use_context);
    EXPECT_GT(config.prediction_horizon_ms, 0.0f);
}

TEST_F(AmygdalaFEPBridgeTest, ValidateConfig) {
    amyg_fep_config_t config;
    amyg_fep_bridge_default_config(&config);
    EXPECT_EQ(amyg_fep_bridge_validate_config(&config), 0);

    // Null config
    EXPECT_NE(amyg_fep_bridge_validate_config(nullptr), 0);
}

TEST_F(AmygdalaFEPBridgeTest, InvalidConfig) {
    amyg_fep_config_t config;
    amyg_fep_bridge_default_config(&config);

    // Invalid learning rate
    config.learning_rate = 0.0f;
    EXPECT_NE(amyg_fep_bridge_validate_config(&config), 0);

    // Invalid learning rate (too high)
    config.learning_rate = 1.5f;
    EXPECT_NE(amyg_fep_bridge_validate_config(&config), 0);
}

TEST_F(AmygdalaFEPBridgeTest, CreateWithCustomConfig) {
    amyg_fep_config_t config;
    amyg_fep_bridge_default_config(&config);
    config.default_model = AMYG_FEP_MODEL_THREAT;
    config.learning_rate = 0.1f;

    amyg_fep_bridge_t* custom = amyg_fep_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(custom, nullptr);
    amyg_fep_bridge_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, CreateAndDestroy) {
    amyg_fep_bridge_t* b = amyg_fep_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    amyg_fep_bridge_destroy(b);
}

TEST_F(AmygdalaFEPBridgeTest, DestroyNull) {
    amyg_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(AmygdalaFEPBridgeTest, Reset) {
    // First compute some errors to change state
    float observations[5] = {0.7f, 0.3f, 0.5f, 0.4f, 0.2f};
    amyg_fep_errors_t errors;
    amyg_fep_compute_errors(bridge, observations, 5, &errors);

    // Reset
    EXPECT_EQ(amyg_fep_bridge_reset(bridge), 0);

    // Verify reset state
    amyg_fep_stats_t stats;
    EXPECT_EQ(amyg_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.inference_steps_total, 0u);
    EXPECT_EQ(stats.predictions_made, 0u);
}

TEST_F(AmygdalaFEPBridgeTest, ResetNull) {
    EXPECT_NE(amyg_fep_bridge_reset(nullptr), 0);
}

//=============================================================================
// Prediction Error Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, ComputeErrors) {
    float observations[5] = {0.8f, 0.2f, 0.5f, 0.6f, 0.3f};  // High threat observation
    amyg_fep_errors_t errors;

    EXPECT_EQ(amyg_fep_compute_errors(bridge, observations, 5, &errors), 0);

    // Should have safety error (threat observed when expecting safe-ish)
    EXPECT_GE(errors.safety_error, 0.0f);
    EXPECT_GE(errors.total_free_energy, 0.0f);
    EXPECT_GE(errors.precision_weighted_error, 0.0f);
}

TEST_F(AmygdalaFEPBridgeTest, ComputeErrorsNull) {
    float observations[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    amyg_fep_errors_t errors;

    EXPECT_NE(amyg_fep_compute_errors(nullptr, observations, 5, &errors), 0);
    EXPECT_NE(amyg_fep_compute_errors(bridge, nullptr, 5, &errors), 0);
    EXPECT_NE(amyg_fep_compute_errors(bridge, observations, 5, nullptr), 0);
}

TEST_F(AmygdalaFEPBridgeTest, GetFreeEnergy) {
    float initial_fe = amyg_fep_get_free_energy(bridge);
    EXPECT_GE(initial_fe, 0.0f);

    // After observation with threat, free energy should increase
    float observations[5] = {0.9f, 0.1f, 0.5f, 0.7f, 0.2f};
    amyg_fep_errors_t errors;
    amyg_fep_compute_errors(bridge, observations, 5, &errors);

    float fe = amyg_fep_get_free_energy(bridge);
    EXPECT_GT(fe, 0.0f);
}

TEST_F(AmygdalaFEPBridgeTest, GetSurprise) {
    float surprise = amyg_fep_get_surprise(bridge);
    EXPECT_GE(surprise, 0.0f);
}

TEST_F(AmygdalaFEPBridgeTest, UpdatePrecision) {
    EXPECT_EQ(amyg_fep_update_precision(bridge, 0.8f, 0.3f), 0);
    EXPECT_NE(amyg_fep_update_precision(nullptr, 0.8f, 0.3f), 0);
}

TEST_F(AmygdalaFEPBridgeTest, SetInteroception) {
    amyg_fep_interoception_t intero = {
        .heart_rate_deviation = 0.3f,
        .respiratory_rate = 0.2f,
        .skin_conductance = 0.5f,
        .muscle_tension = 0.4f,
        .gut_feeling = -0.2f
    };
    EXPECT_EQ(amyg_fep_set_interoception(bridge, &intero), 0);
    EXPECT_NE(amyg_fep_set_interoception(nullptr, &intero), 0);
    EXPECT_NE(amyg_fep_set_interoception(bridge, nullptr), 0);
}

//=============================================================================
// Active Inference Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, InferState) {
    float observations[5] = {0.6f, 0.4f, 0.5f, 0.5f, 0.3f};
    amyg_fep_inference_t inference;

    EXPECT_EQ(amyg_fep_infer_state(bridge, observations, 5, &inference), 0);

    EXPECT_EQ(inference.state_dim, 5u);
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_GE(inference.precision[i], 0.0f);
    }
}

TEST_F(AmygdalaFEPBridgeTest, InferStateNull) {
    float observations[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    amyg_fep_inference_t inference;

    EXPECT_NE(amyg_fep_infer_state(nullptr, observations, 5, &inference), 0);
    EXPECT_NE(amyg_fep_infer_state(bridge, nullptr, 5, &inference), 0);
    EXPECT_NE(amyg_fep_infer_state(bridge, observations, 5, nullptr), 0);
}

TEST_F(AmygdalaFEPBridgeTest, SelectAction) {
    amyg_fep_action_t action;
    float action_value;

    EXPECT_EQ(amyg_fep_select_action(bridge, &action, &action_value), 0);

    // Action should be valid
    EXPECT_GE((int)action, AMYG_FEP_ACTION_OBSERVE);
    EXPECT_LE((int)action, AMYG_FEP_ACTION_APPROACH);
}

TEST_F(AmygdalaFEPBridgeTest, SelectActionNull) {
    amyg_fep_action_t action;

    EXPECT_NE(amyg_fep_select_action(nullptr, &action, nullptr), 0);
    EXPECT_NE(amyg_fep_select_action(bridge, nullptr, nullptr), 0);
}

TEST_F(AmygdalaFEPBridgeTest, ApplyAction) {
    EXPECT_EQ(amyg_fep_apply_action(bridge, AMYG_FEP_ACTION_OBSERVE), 0);
    EXPECT_EQ(amyg_fep_apply_action(bridge, AMYG_FEP_ACTION_ORIENT), 0);
    EXPECT_EQ(amyg_fep_apply_action(bridge, AMYG_FEP_ACTION_FREEZE), 0);
    EXPECT_EQ(amyg_fep_apply_action(bridge, AMYG_FEP_ACTION_AVOID), 0);
    EXPECT_EQ(amyg_fep_apply_action(bridge, AMYG_FEP_ACTION_APPROACH), 0);
}

TEST_F(AmygdalaFEPBridgeTest, ApplyActionNull) {
    EXPECT_NE(amyg_fep_apply_action(nullptr, AMYG_FEP_ACTION_OBSERVE), 0);
}

TEST_F(AmygdalaFEPBridgeTest, ExpectedFreeEnergy) {
    float efe_observe = amyg_fep_expected_free_energy(bridge, AMYG_FEP_ACTION_OBSERVE);
    float efe_orient = amyg_fep_expected_free_energy(bridge, AMYG_FEP_ACTION_ORIENT);
    float efe_freeze = amyg_fep_expected_free_energy(bridge, AMYG_FEP_ACTION_FREEZE);
    float efe_avoid = amyg_fep_expected_free_energy(bridge, AMYG_FEP_ACTION_AVOID);
    float efe_approach = amyg_fep_expected_free_energy(bridge, AMYG_FEP_ACTION_APPROACH);

    // All EFE values should be finite
    EXPECT_TRUE(std::isfinite(efe_observe));
    EXPECT_TRUE(std::isfinite(efe_orient));
    EXPECT_TRUE(std::isfinite(efe_freeze));
    EXPECT_TRUE(std::isfinite(efe_avoid));
    EXPECT_TRUE(std::isfinite(efe_approach));
}

//=============================================================================
// Generative Model Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, SetModel) {
    EXPECT_EQ(amyg_fep_set_model(bridge, AMYG_FEP_MODEL_SAFE), 0);
    EXPECT_EQ(amyg_fep_set_model(bridge, AMYG_FEP_MODEL_VIGILANT), 0);
    EXPECT_EQ(amyg_fep_set_model(bridge, AMYG_FEP_MODEL_THREAT), 0);
    EXPECT_EQ(amyg_fep_set_model(bridge, AMYG_FEP_MODEL_DANGER), 0);
    EXPECT_EQ(amyg_fep_set_model(bridge, AMYG_FEP_MODEL_PANIC), 0);
}

TEST_F(AmygdalaFEPBridgeTest, SetModelNull) {
    EXPECT_NE(amyg_fep_set_model(nullptr, AMYG_FEP_MODEL_SAFE), 0);
}

TEST_F(AmygdalaFEPBridgeTest, SetModelInvalid) {
    EXPECT_NE(amyg_fep_set_model(bridge, (amyg_fep_model_t)99), 0);
}

TEST_F(AmygdalaFEPBridgeTest, GetBestModel) {
    amyg_fep_model_t best = amyg_fep_get_best_model(bridge);

    // Should be a valid model type
    EXPECT_GE((int)best, AMYG_FEP_MODEL_SAFE);
    EXPECT_LE((int)best, AMYG_FEP_MODEL_PANIC);
}

TEST_F(AmygdalaFEPBridgeTest, GetModelEvidence) {
    for (int m = AMYG_FEP_MODEL_SAFE; m <= AMYG_FEP_MODEL_PANIC; m++) {
        float evidence = amyg_fep_get_model_evidence(bridge, (amyg_fep_model_t)m);
        EXPECT_GE(evidence, 0.0f);
        EXPECT_LE(evidence, 1.0f);
    }
}

TEST_F(AmygdalaFEPBridgeTest, Predict) {
    // First set some state by inferring
    float observations[5] = {0.5f, 0.3f, 0.4f, 0.5f, 0.6f};
    amyg_fep_inference_t inference;
    amyg_fep_infer_state(bridge, observations, 5, &inference);

    // Now predict forward
    float predicted[5];
    int result = amyg_fep_predict(bridge, 500.0f, predicted, 5);
    EXPECT_GT(result, 0);
}

TEST_F(AmygdalaFEPBridgeTest, PredictNull) {
    float predicted[5];
    EXPECT_EQ(amyg_fep_predict(nullptr, 500.0f, predicted, 5), -1);
    EXPECT_EQ(amyg_fep_predict(bridge, 500.0f, nullptr, 5), -1);
}

TEST_F(AmygdalaFEPBridgeTest, Conditioning) {
    float cs_features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_EQ(amyg_fep_condition(bridge, cs_features, 8, 0.8f), 0);
}

TEST_F(AmygdalaFEPBridgeTest, Extinction) {
    float cs_features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_EQ(amyg_fep_extinction(bridge, cs_features, 8), 0);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, ConnectAmygdala) {
    EXPECT_EQ(amyg_fep_connect_amygdala(bridge, nullptr), 0);
    EXPECT_NE(amyg_fep_connect_amygdala(nullptr, nullptr), 0);
}

TEST_F(AmygdalaFEPBridgeTest, ConnectSystem) {
    EXPECT_EQ(amyg_fep_connect_system(bridge, nullptr), 0);
    EXPECT_NE(amyg_fep_connect_system(nullptr, nullptr), 0);
}

TEST_F(AmygdalaFEPBridgeTest, Update) {
    EXPECT_EQ(amyg_fep_update(bridge, 50.0f), 0);
}

TEST_F(AmygdalaFEPBridgeTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(amyg_fep_update(bridge, 50.0f), 0);
    }
}

TEST_F(AmygdalaFEPBridgeTest, UpdateNull) {
    EXPECT_NE(amyg_fep_update(nullptr, 50.0f), 0);
}

TEST_F(AmygdalaFEPBridgeTest, SyncWithAmygdala) {
    EXPECT_EQ(amyg_fep_sync_with_amygdala(bridge), 0);
    EXPECT_NE(amyg_fep_sync_with_amygdala(nullptr), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, GetStats) {
    amyg_fep_stats_t stats;
    EXPECT_EQ(amyg_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.inference_steps_total, 0u);
    EXPECT_EQ(stats.predictions_made, 0u);
}

TEST_F(AmygdalaFEPBridgeTest, StatsAccumulate) {
    // Perform some operations
    float observations[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    amyg_fep_errors_t errors;
    amyg_fep_compute_errors(bridge, observations, 5, &errors);

    amyg_fep_inference_t inference;
    amyg_fep_infer_state(bridge, observations, 5, &inference);

    amyg_fep_stats_t stats;
    EXPECT_EQ(amyg_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.predictions_made, 0u);
    EXPECT_GT(stats.inference_steps_total, 0u);
}

TEST_F(AmygdalaFEPBridgeTest, ModelSwitchesTracked) {
    // Switch models
    amyg_fep_set_model(bridge, AMYG_FEP_MODEL_SAFE);
    amyg_fep_set_model(bridge, AMYG_FEP_MODEL_THREAT);
    amyg_fep_set_model(bridge, AMYG_FEP_MODEL_PANIC);

    amyg_fep_stats_t stats;
    EXPECT_EQ(amyg_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.model_switches, 0u);
}

TEST_F(AmygdalaFEPBridgeTest, ResetStats) {
    // Perform operations to build up stats
    float observations[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    amyg_fep_errors_t errors;
    amyg_fep_compute_errors(bridge, observations, 5, &errors);

    // Reset stats
    EXPECT_EQ(amyg_fep_bridge_reset_stats(bridge), 0);

    amyg_fep_stats_t stats;
    EXPECT_EQ(amyg_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.predictions_made, 0u);
    EXPECT_EQ(stats.inference_steps_total, 0u);
}

TEST_F(AmygdalaFEPBridgeTest, NullStats) {
    amyg_fep_stats_t stats;
    EXPECT_NE(amyg_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(amyg_fep_bridge_get_stats(bridge, nullptr), 0);
    EXPECT_NE(amyg_fep_bridge_reset_stats(nullptr), 0);
}

//=============================================================================
// Name Function Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, ModelNames) {
    EXPECT_STREQ(amyg_fep_model_name(AMYG_FEP_MODEL_SAFE), "safe");
    EXPECT_STREQ(amyg_fep_model_name(AMYG_FEP_MODEL_VIGILANT), "vigilant");
    EXPECT_STREQ(amyg_fep_model_name(AMYG_FEP_MODEL_THREAT), "threat");
    EXPECT_STREQ(amyg_fep_model_name(AMYG_FEP_MODEL_DANGER), "danger");
    EXPECT_STREQ(amyg_fep_model_name(AMYG_FEP_MODEL_PANIC), "panic");
}

TEST_F(AmygdalaFEPBridgeTest, ActionNames) {
    EXPECT_STREQ(amyg_fep_action_name(AMYG_FEP_ACTION_OBSERVE), "observe");
    EXPECT_STREQ(amyg_fep_action_name(AMYG_FEP_ACTION_ORIENT), "orient");
    EXPECT_STREQ(amyg_fep_action_name(AMYG_FEP_ACTION_FREEZE), "freeze");
    EXPECT_STREQ(amyg_fep_action_name(AMYG_FEP_ACTION_AVOID), "avoid");
    EXPECT_STREQ(amyg_fep_action_name(AMYG_FEP_ACTION_APPROACH), "approach");
}

TEST_F(AmygdalaFEPBridgeTest, PrecisionModeNames) {
    EXPECT_STREQ(amyg_fep_precision_mode_name(AMYG_FEP_PRECISION_FIXED), "fixed");
    EXPECT_STREQ(amyg_fep_precision_mode_name(AMYG_FEP_PRECISION_ADAPTIVE), "adaptive");
    EXPECT_STREQ(amyg_fep_precision_mode_name(AMYG_FEP_PRECISION_INTEROCEPTIVE), "interoceptive");
}

TEST_F(AmygdalaFEPBridgeTest, SafetyBeliefNames) {
    EXPECT_STREQ(amyg_fep_safety_belief_name(AMYG_FEP_BELIEF_SAFE), "safe");
    EXPECT_STREQ(amyg_fep_safety_belief_name(AMYG_FEP_BELIEF_UNCERTAIN), "uncertain");
    EXPECT_STREQ(amyg_fep_safety_belief_name(AMYG_FEP_BELIEF_UNSAFE), "unsafe");
}

//=============================================================================
// Workflow Tests
//=============================================================================

TEST_F(AmygdalaFEPBridgeTest, FullThreatProcessingLoop) {
    float dt = 50.0f;

    for (int cycle = 0; cycle < 20; cycle++) {
        // Simulate increasing threat
        float threat = 0.3f + cycle * 0.03f;
        float observations[5] = {threat, 0.5f, 0.4f, threat * 0.8f, 0.3f};

        // 1. Compute prediction errors
        amyg_fep_errors_t errors;
        EXPECT_EQ(amyg_fep_compute_errors(bridge, observations, 5, &errors), 0);

        // 2. Update beliefs via inference
        amyg_fep_inference_t inference;
        EXPECT_EQ(amyg_fep_infer_state(bridge, observations, 5, &inference), 0);

        // 3. Select best action
        amyg_fep_action_t action;
        float action_value;
        EXPECT_EQ(amyg_fep_select_action(bridge, &action, &action_value), 0);

        // 4. Apply action
        EXPECT_EQ(amyg_fep_apply_action(bridge, action), 0);

        // 5. Update internal state
        EXPECT_EQ(amyg_fep_update(bridge, dt), 0);
    }

    // Verify stats accumulated
    amyg_fep_stats_t stats;
    EXPECT_EQ(amyg_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.predictions_made, 20u);
    EXPECT_GT(stats.inference_steps_total, 0u);
}

TEST_F(AmygdalaFEPBridgeTest, ConditioningIncreasesTheatPrior) {
    // Get initial threat belief after neutral observation
    float observations[5] = {0.3f, 0.5f, 0.5f, 0.3f, 0.5f};
    amyg_fep_inference_t inference_before;
    amyg_fep_infer_state(bridge, observations, 5, &inference_before);
    float threat_before = inference_before.beliefs[0];

    // Apply conditioning
    float cs_features[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    amyg_fep_condition(bridge, cs_features, 8, 0.9f);

    // Check threat belief increased
    amyg_fep_inference_t inference_after;
    amyg_fep_infer_state(bridge, observations, 5, &inference_after);
    float threat_after = inference_after.beliefs[0];

    EXPECT_GT(threat_after, threat_before);
}

TEST_F(AmygdalaFEPBridgeTest, ExtinctionReducesThreat) {
    // First condition to high threat
    float cs_features[8] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    amyg_fep_condition(bridge, cs_features, 8, 0.9f);

    // Get threat before extinction
    float observations[5] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    amyg_fep_inference_t inference_before;
    amyg_fep_infer_state(bridge, observations, 5, &inference_before);
    float threat_before = inference_before.beliefs[0];

    // Apply multiple extinction trials
    for (int i = 0; i < 10; i++) {
        amyg_fep_extinction(bridge, cs_features, 8);
    }

    // Check threat belief decreased
    amyg_fep_inference_t inference_after;
    amyg_fep_infer_state(bridge, observations, 5, &inference_after);
    float threat_after = inference_after.beliefs[0];

    EXPECT_LT(threat_after, threat_before);
}

TEST_F(AmygdalaFEPBridgeTest, HighThreatSelectsDefensiveAction) {
    // Set high threat observation
    float high_threat[5] = {0.9f, 0.1f, 0.2f, 0.8f, 0.1f};
    amyg_fep_errors_t errors;
    amyg_fep_compute_errors(bridge, high_threat, 5, &errors);

    amyg_fep_inference_t inference;
    amyg_fep_infer_state(bridge, high_threat, 5, &inference);

    // Select action
    amyg_fep_action_t action;
    amyg_fep_select_action(bridge, &action, nullptr);

    // High threat should NOT select observe or approach
    // Should select freeze, avoid, or orient
    EXPECT_NE(action, AMYG_FEP_ACTION_APPROACH);
}
