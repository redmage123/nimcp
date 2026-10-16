/**
 * @file test_basal_ganglia_fep_bridge.cpp
 * @brief Unit tests for Basal Ganglia FEP Bridge
 *
 * Tests action selection as expected free energy minimization,
 * dopamine-precision mapping, habit crystallization, and model selection.
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia_fep_bridge.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BasalGangliaFEPBridgeTest : public ::testing::Test {
protected:
    bg_fep_bridge_t* bridge = nullptr;
    bg_fep_config_t config;

    void SetUp() override {
        bg_fep_default_config(&config);
        bridge = bg_fep_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            bg_fep_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, DefaultConfig) {
    bg_fep_config_t cfg;
    bg_fep_default_config(&cfg);

    EXPECT_EQ(cfg.default_model, BG_FEP_MODEL_EXPLOIT);
    EXPECT_TRUE(cfg.auto_model_selection);
    EXPECT_EQ(cfg.precision_mode, BG_FEP_PRECISION_DOPAMINE);
    EXPECT_GT(cfg.action_precision, 0.0f);
    EXPECT_GT(cfg.pragmatic_weight, 0.0f);
    EXPECT_GT(cfg.selection_temperature, 0.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, ValidateConfig) {
    bg_fep_config_t cfg;
    bg_fep_default_config(&cfg);

    EXPECT_TRUE(bg_fep_validate_config(&cfg));

    // Invalid model
    cfg.default_model = (bg_fep_model_t)100;
    EXPECT_FALSE(bg_fep_validate_config(&cfg));
    bg_fep_default_config(&cfg);

    // Invalid temperature
    cfg.selection_temperature = 0.0f;
    EXPECT_FALSE(bg_fep_validate_config(&cfg));
    bg_fep_default_config(&cfg);

    // Invalid learning rate
    cfg.reward_learning_rate = -0.1f;
    EXPECT_FALSE(bg_fep_validate_config(&cfg));
}

TEST_F(BasalGangliaFEPBridgeTest, InvalidConfig) {
    bg_fep_config_t cfg;
    bg_fep_default_config(&cfg);
    cfg.default_model = (bg_fep_model_t)999;

    bg_fep_bridge_t* bad_bridge = bg_fep_create(&cfg);
    EXPECT_EQ(bad_bridge, nullptr);
}

TEST_F(BasalGangliaFEPBridgeTest, CreateWithCustomConfig) {
    bg_fep_config_t custom;
    bg_fep_default_config(&custom);
    custom.epistemic_weight = 0.5f;
    custom.selection_temperature = 2.0f;

    bg_fep_bridge_t* custom_bridge = bg_fep_create(&custom);
    ASSERT_NE(custom_bridge, nullptr);
    bg_fep_destroy(custom_bridge);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, CreateAndDestroy) {
    bg_fep_bridge_t* b = bg_fep_create(nullptr);
    ASSERT_NE(b, nullptr);
    bg_fep_destroy(b);  // Should not crash
}

TEST_F(BasalGangliaFEPBridgeTest, DestroyNull) {
    bg_fep_destroy(nullptr);  // Should not crash
}

TEST_F(BasalGangliaFEPBridgeTest, Reset) {
    // Set some state
    bg_fep_set_dopamine(bridge, 0.9f);
    bg_fep_set_habit(bridge, 0, 0.8f);

    // Reset
    EXPECT_EQ(bg_fep_reset(bridge), 0);

    // Verify reset state
    EXPECT_FLOAT_EQ(bg_fep_get_dopamine_precision(bridge), config.action_precision);
}

TEST_F(BasalGangliaFEPBridgeTest, ResetNull) {
    EXPECT_EQ(bg_fep_reset(nullptr), -1);
}

//=============================================================================
// Prediction Error Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, ComputeErrors) {
    bg_fep_errors_t errors;
    int result = bg_fep_compute_errors(bridge, 0, 1.0f, 0.5f, &errors);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(errors.reward_error, 0.5f);  // 1.0 - 0.5
    EXPECT_GT(errors.total_free_energy, 0.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, ComputeErrorsNull) {
    bg_fep_errors_t errors;
    EXPECT_EQ(bg_fep_compute_errors(nullptr, 0, 1.0f, 0.5f, &errors), -1);
    EXPECT_EQ(bg_fep_compute_errors(bridge, 0, 1.0f, 0.5f, nullptr), -1);
}

TEST_F(BasalGangliaFEPBridgeTest, ComputeErrorsInvalidAction) {
    bg_fep_errors_t errors;
    EXPECT_EQ(bg_fep_compute_errors(bridge, BG_FEP_MAX_ACTIONS + 1, 1.0f, 0.5f, &errors), -1);
}

TEST_F(BasalGangliaFEPBridgeTest, GetFreeEnergy) {
    // Initially should be zero or small
    float fe = bg_fep_get_free_energy(bridge);
    EXPECT_GE(fe, 0.0f);

    // After computing errors, should be positive
    bg_fep_errors_t errors;
    bg_fep_compute_errors(bridge, 0, 1.0f, 0.0f, &errors);
    fe = bg_fep_get_free_energy(bridge);
    EXPECT_GT(fe, 0.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, GetSurprise) {
    float surprise = bg_fep_get_surprise(bridge);
    EXPECT_GE(surprise, 0.0f);

    // Large prediction error = high surprise
    bg_fep_errors_t errors;
    bg_fep_compute_errors(bridge, 0, 10.0f, 0.0f, &errors);
    surprise = bg_fep_get_surprise(bridge);
    EXPECT_GT(surprise, 0.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, UpdatePrecision) {
    // Small prediction error should increase precision
    int result = bg_fep_update_precision(bridge, 0, 0.1f);
    EXPECT_EQ(result, 0);

    // Large prediction error should decrease precision
    result = bg_fep_update_precision(bridge, 0, 10.0f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Active Inference Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, EvaluateActions) {
    float action_values[4] = {0.5f, 0.8f, 0.3f, 0.6f};
    bg_fep_action_eval_t evaluations[4];

    int result = bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);
    EXPECT_EQ(result, 0);

    // All actions should have expected free energy computed
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(evaluations[i].action_id, (uint32_t)i);
        // Posterior should sum to approximately 1
    }

    float sum = 0.0f;
    for (int i = 0; i < 4; i++) {
        sum += evaluations[i].posterior_probability;
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

TEST_F(BasalGangliaFEPBridgeTest, EvaluateActionsNull) {
    float action_values[4] = {0.5f, 0.8f, 0.3f, 0.6f};
    bg_fep_action_eval_t evaluations[4];

    EXPECT_EQ(bg_fep_evaluate_actions(nullptr, action_values, 4, evaluations), -1);
    EXPECT_EQ(bg_fep_evaluate_actions(bridge, nullptr, 4, evaluations), -1);
    EXPECT_EQ(bg_fep_evaluate_actions(bridge, action_values, 4, nullptr), -1);
}

TEST_F(BasalGangliaFEPBridgeTest, SelectAction) {
    float action_values[4] = {0.5f, 0.8f, 0.3f, 0.6f};
    bg_fep_action_eval_t evaluations[4];

    bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);

    uint32_t selected;
    int result = bg_fep_select_action(bridge, evaluations, 4, &selected);

    EXPECT_EQ(result, 0);
    EXPECT_LT(selected, 4u);
}

TEST_F(BasalGangliaFEPBridgeTest, SelectActionNull) {
    bg_fep_action_eval_t evaluations[4];
    uint32_t selected;

    EXPECT_EQ(bg_fep_select_action(nullptr, evaluations, 4, &selected), -1);
    EXPECT_EQ(bg_fep_select_action(bridge, nullptr, 4, &selected), -1);
    EXPECT_EQ(bg_fep_select_action(bridge, evaluations, 4, nullptr), -1);
    EXPECT_EQ(bg_fep_select_action(bridge, evaluations, 0, &selected), -1);
}

TEST_F(BasalGangliaFEPBridgeTest, HighValueActionSelected) {
    // First, train the outcome models so they reflect the action values
    // Action 1 has highest reward
    for (int i = 0; i < 10; i++) {
        bg_fep_update_outcome_model(bridge, 0, 0.1f, 0.1f, true);
        bg_fep_update_outcome_model(bridge, 1, 0.9f, 0.1f, true);
        bg_fep_update_outcome_model(bridge, 2, 0.2f, 0.1f, true);
        bg_fep_update_outcome_model(bridge, 3, 0.3f, 0.1f, true);
    }

    float action_values[4] = {0.1f, 0.9f, 0.2f, 0.3f};
    bg_fep_action_eval_t evaluations[4];

    bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);

    uint32_t selected;
    bg_fep_select_action(bridge, evaluations, 4, &selected);

    // After training, highest reward action should be selected
    EXPECT_EQ(selected, 1u);
}

TEST_F(BasalGangliaFEPBridgeTest, ExpectedFreeEnergy) {
    // Train outcome models first
    for (int i = 0; i < 10; i++) {
        bg_fep_update_outcome_model(bridge, 1, 0.8f, 0.1f, true);  // High reward
        bg_fep_update_outcome_model(bridge, 2, 0.3f, 0.1f, true);  // Low reward
    }

    float action_values[4] = {0.5f, 0.8f, 0.3f, 0.6f};
    bg_fep_action_eval_t evaluations[4];

    bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);

    // Higher reward action should have lower expected free energy
    float efe_low = bg_fep_expected_free_energy(bridge, 2);  // Trained with 0.3
    float efe_high = bg_fep_expected_free_energy(bridge, 1); // Trained with 0.8

    // Lower reward action has higher EFE (worse)
    EXPECT_GT(efe_low, efe_high);
}

TEST_F(BasalGangliaFEPBridgeTest, GetPolicyEntropy) {
    // Before evaluation, entropy should be 0
    float entropy_before = bg_fep_get_policy_entropy(bridge);
    EXPECT_GE(entropy_before, 0.0f);

    // After evaluation with diverse values, entropy should be positive
    float action_values[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    bg_fep_action_eval_t evaluations[4];
    bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);

    float entropy_after = bg_fep_get_policy_entropy(bridge);
    EXPECT_GT(entropy_after, 0.0f);  // Equal values = high entropy
}

TEST_F(BasalGangliaFEPBridgeTest, GetInferenceState) {
    float action_values[4] = {0.5f, 0.8f, 0.3f, 0.6f};
    bg_fep_action_eval_t evaluations[4];
    bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);

    uint32_t selected;
    bg_fep_select_action(bridge, evaluations, 4, &selected);

    bg_fep_inference_t inference;
    int result = bg_fep_get_inference_state(bridge, &inference);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(inference.num_actions, 4u);
    EXPECT_EQ(inference.selected_action, selected);
}

//=============================================================================
// Generative Model Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, SetModel) {
    EXPECT_EQ(bg_fep_set_model(bridge, BG_FEP_MODEL_EXPLORE), 0);
    EXPECT_EQ(bg_fep_set_model(bridge, BG_FEP_MODEL_EXPLOIT), 0);
    EXPECT_EQ(bg_fep_set_model(bridge, BG_FEP_MODEL_HABIT), 0);
    EXPECT_EQ(bg_fep_set_model(bridge, BG_FEP_MODEL_CAUTIOUS), 0);
}

TEST_F(BasalGangliaFEPBridgeTest, SetModelNull) {
    EXPECT_EQ(bg_fep_set_model(nullptr, BG_FEP_MODEL_EXPLORE), -1);
}

TEST_F(BasalGangliaFEPBridgeTest, SetModelInvalid) {
    EXPECT_EQ(bg_fep_set_model(bridge, (bg_fep_model_t)999), -1);
}

TEST_F(BasalGangliaFEPBridgeTest, GetBestModel) {
    bg_fep_model_t best = bg_fep_get_best_model(bridge);
    EXPECT_LT((int)best, BG_FEP_NUM_POLICY_MODELS);
}

TEST_F(BasalGangliaFEPBridgeTest, GetModelEvidence) {
    float evidence = bg_fep_get_model_evidence(bridge, BG_FEP_MODEL_EXPLOIT);
    EXPECT_GT(evidence, 0.0f);
    EXPECT_LE(evidence, 1.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, UpdateOutcomeModel) {
    int result = bg_fep_update_outcome_model(bridge, 0, 1.0f, 0.2f, true);
    EXPECT_EQ(result, 0);

    // Outcome model should be updated
    bg_fep_outcome_model_t outcome;
    bg_fep_predict_outcome(bridge, 0, &outcome);
    EXPECT_GT(outcome.reward_mean, 0.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, PredictOutcome) {
    bg_fep_outcome_model_t outcome;
    int result = bg_fep_predict_outcome(bridge, 0, &outcome);

    EXPECT_EQ(result, 0);
    EXPECT_GE(outcome.success_probability, 0.0f);
    EXPECT_LE(outcome.success_probability, 1.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, PredictOutcomeNull) {
    bg_fep_outcome_model_t outcome;
    EXPECT_EQ(bg_fep_predict_outcome(nullptr, 0, &outcome), -1);
    EXPECT_EQ(bg_fep_predict_outcome(bridge, 0, nullptr), -1);
}

//=============================================================================
// Habit Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, SetHabit) {
    int result = bg_fep_set_habit(bridge, 0, 0.8f);
    EXPECT_EQ(result, 0);

    float prior = bg_fep_get_habit_prior(bridge, 0);
    EXPECT_GT(prior, 1.0f);  // Should be boosted
}

TEST_F(BasalGangliaFEPBridgeTest, SetHabitNull) {
    EXPECT_EQ(bg_fep_set_habit(nullptr, 0, 0.8f), -1);
}

TEST_F(BasalGangliaFEPBridgeTest, SetHabitClamp) {
    // Values should be clamped to [0, 1]
    EXPECT_EQ(bg_fep_set_habit(bridge, 0, 2.0f), 0);
    EXPECT_EQ(bg_fep_set_habit(bridge, 0, -0.5f), 0);
}

TEST_F(BasalGangliaFEPBridgeTest, GetHabitPrior) {
    // Without habit, prior should be 1.0
    float prior = bg_fep_get_habit_prior(bridge, 0);
    EXPECT_FLOAT_EQ(prior, 1.0f);

    // With habit, prior should be boosted
    bg_fep_set_habit(bridge, 0, 0.9f);
    prior = bg_fep_get_habit_prior(bridge, 0);
    EXPECT_GT(prior, 1.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, ClearHabit) {
    bg_fep_set_habit(bridge, 0, 0.8f);
    EXPECT_GT(bg_fep_get_habit_prior(bridge, 0), 1.0f);

    int result = bg_fep_clear_habit(bridge, 0);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bg_fep_get_habit_prior(bridge, 0), 1.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, IsHabitMode) {
    EXPECT_FALSE(bg_fep_is_habit_mode(bridge));

    bg_fep_set_model(bridge, BG_FEP_MODEL_HABIT);
    EXPECT_TRUE(bg_fep_is_habit_mode(bridge));
}

TEST_F(BasalGangliaFEPBridgeTest, HabitBoostsSelection) {
    // Action 0 has habit, action 1 has higher value
    float action_values[2] = {0.4f, 0.5f};
    bg_fep_action_eval_t evaluations[2];

    // Set strong habit on action 0
    bg_fep_set_habit(bridge, 0, 0.9f);

    bg_fep_evaluate_actions(bridge, action_values, 2, evaluations);

    // Habitual action should have lower expected free energy despite lower value
    float efe_habit = bg_fep_expected_free_energy(bridge, 0);
    float efe_other = bg_fep_expected_free_energy(bridge, 1);

    // With strong habit prior boost, action 0 should be preferred
    EXPECT_LT(efe_habit, efe_other);
}

//=============================================================================
// Dopamine Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, SetDopamine) {
    int result = bg_fep_set_dopamine(bridge, 0.8f);
    EXPECT_EQ(result, 0);

    float precision = bg_fep_get_dopamine_precision(bridge);
    EXPECT_GT(precision, config.action_precision);  // High DA = high precision
}

TEST_F(BasalGangliaFEPBridgeTest, SetDopamineLow) {
    bg_fep_set_dopamine(bridge, 0.2f);

    float precision = bg_fep_get_dopamine_precision(bridge);
    EXPECT_LT(precision, config.action_precision * 1.5f);  // Low DA = lower precision
}

TEST_F(BasalGangliaFEPBridgeTest, SetDopamineClamp) {
    EXPECT_EQ(bg_fep_set_dopamine(bridge, 2.0f), 0);  // Clamped to 1.0
    EXPECT_EQ(bg_fep_set_dopamine(bridge, -0.5f), 0); // Clamped to 0.0
}

TEST_F(BasalGangliaFEPBridgeTest, ProcessRPE) {
    // Positive RPE should update model evidence
    int result = bg_fep_process_rpe(bridge, 0.5f);
    EXPECT_EQ(result, 0);

    // Negative RPE
    result = bg_fep_process_rpe(bridge, -0.3f);
    EXPECT_EQ(result, 0);
}

TEST_F(BasalGangliaFEPBridgeTest, ProcessRPENull) {
    EXPECT_EQ(bg_fep_process_rpe(nullptr, 0.5f), -1);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, ConnectBasalGanglia) {
    // Can connect NULL (no BG available)
    int result = bg_fep_connect_basal_ganglia(bridge, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(BasalGangliaFEPBridgeTest, ConnectOrchestrator) {
    int dummy = 42;
    int result = bg_fep_connect_orchestrator(bridge, &dummy);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, Update) {
    int result = bg_fep_update(bridge, 16.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(BasalGangliaFEPBridgeTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(bg_fep_update(bridge, 16.0f), 0);
    }
}

TEST_F(BasalGangliaFEPBridgeTest, UpdateNull) {
    EXPECT_EQ(bg_fep_update(nullptr, 16.0f), -1);
}

TEST_F(BasalGangliaFEPBridgeTest, SyncWithBG) {
    // Without connected BG, should return error
    EXPECT_EQ(bg_fep_sync_with_bg(bridge), -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, GetStats) {
    bg_fep_stats_t stats;
    int result = bg_fep_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_inferences, 0u);
}

TEST_F(BasalGangliaFEPBridgeTest, StatsAccumulate) {
    float action_values[4] = {0.5f, 0.8f, 0.3f, 0.6f};
    bg_fep_action_eval_t evaluations[4];
    uint32_t selected;

    // Perform some inferences
    for (int i = 0; i < 10; i++) {
        bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);
        bg_fep_select_action(bridge, evaluations, 4, &selected);
    }

    bg_fep_stats_t stats;
    bg_fep_get_stats(bridge, &stats);

    EXPECT_EQ(stats.action_selections, 10u);
    EXPECT_EQ(stats.total_inferences, 10u);
}

TEST_F(BasalGangliaFEPBridgeTest, ModelSwitchesTracked) {
    bg_fep_set_model(bridge, BG_FEP_MODEL_EXPLORE);
    bg_fep_set_model(bridge, BG_FEP_MODEL_EXPLOIT);
    bg_fep_set_model(bridge, BG_FEP_MODEL_HABIT);

    bg_fep_stats_t stats;
    bg_fep_get_stats(bridge, &stats);

    EXPECT_EQ(stats.model_switches, 3u);
}

TEST_F(BasalGangliaFEPBridgeTest, ResetStats) {
    float action_values[4] = {0.5f, 0.8f, 0.3f, 0.6f};
    bg_fep_action_eval_t evaluations[4];
    uint32_t selected;

    bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);
    bg_fep_select_action(bridge, evaluations, 4, &selected);

    EXPECT_EQ(bg_fep_reset_stats(bridge), 0);

    bg_fep_stats_t stats;
    bg_fep_get_stats(bridge, &stats);
    EXPECT_EQ(stats.action_selections, 0u);
}

TEST_F(BasalGangliaFEPBridgeTest, NullStats) {
    EXPECT_EQ(bg_fep_get_stats(nullptr, nullptr), -1);
    EXPECT_EQ(bg_fep_reset_stats(nullptr), -1);
}

//=============================================================================
// Name Function Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, ModelNames) {
    EXPECT_STREQ(bg_fep_model_name(BG_FEP_MODEL_EXPLORE), "Explore");
    EXPECT_STREQ(bg_fep_model_name(BG_FEP_MODEL_EXPLOIT), "Exploit");
    EXPECT_STREQ(bg_fep_model_name(BG_FEP_MODEL_HABIT), "Habit");
    EXPECT_STREQ(bg_fep_model_name(BG_FEP_MODEL_CAUTIOUS), "Cautious");
    EXPECT_STREQ(bg_fep_model_name((bg_fep_model_t)999), "Unknown");
}

TEST_F(BasalGangliaFEPBridgeTest, ActionStateNames) {
    EXPECT_STREQ(bg_fep_action_state_name(BG_FEP_ACTION_PENDING), "Pending");
    EXPECT_STREQ(bg_fep_action_state_name(BG_FEP_ACTION_EVALUATING), "Evaluating");
    EXPECT_STREQ(bg_fep_action_state_name(BG_FEP_ACTION_SELECTED), "Selected");
    EXPECT_STREQ(bg_fep_action_state_name(BG_FEP_ACTION_EXECUTING), "Executing");
    EXPECT_STREQ(bg_fep_action_state_name(BG_FEP_ACTION_FEEDBACK), "Feedback");
}

TEST_F(BasalGangliaFEPBridgeTest, PrecisionModeNames) {
    EXPECT_STREQ(bg_fep_precision_mode_name(BG_FEP_PRECISION_FIXED), "Fixed");
    EXPECT_STREQ(bg_fep_precision_mode_name(BG_FEP_PRECISION_DOPAMINE), "Dopamine");
    EXPECT_STREQ(bg_fep_precision_mode_name(BG_FEP_PRECISION_ADAPTIVE), "Adaptive");
    EXPECT_STREQ(bg_fep_precision_mode_name(BG_FEP_PRECISION_HIERARCHICAL), "Hierarchical");
}

TEST_F(BasalGangliaFEPBridgeTest, ConfidenceNames) {
    EXPECT_STREQ(bg_fep_confidence_name(BG_FEP_CONFIDENCE_LOW), "Low");
    EXPECT_STREQ(bg_fep_confidence_name(BG_FEP_CONFIDENCE_MEDIUM), "Medium");
    EXPECT_STREQ(bg_fep_confidence_name(BG_FEP_CONFIDENCE_HIGH), "High");
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(BasalGangliaFEPBridgeTest, FullActionSelectionLoop) {
    // Simulate full action selection with outcome feedback
    float action_values[4] = {0.5f, 0.8f, 0.3f, 0.6f};
    bg_fep_action_eval_t evaluations[4];

    // Evaluate actions
    bg_fep_evaluate_actions(bridge, action_values, 4, evaluations);

    // Select action
    uint32_t selected;
    bg_fep_select_action(bridge, evaluations, 4, &selected);
    EXPECT_LT(selected, 4u);

    // Simulate outcome
    float reward = 0.9f;
    bg_fep_update_outcome_model(bridge, selected, reward, 0.1f, true);

    // Compute prediction error
    bg_fep_errors_t errors;
    bg_fep_compute_errors(bridge, selected, reward, action_values[selected], &errors);

    // Process RPE
    bg_fep_process_rpe(bridge, errors.reward_error);

    // Update precision
    bg_fep_update_precision(bridge, selected, errors.outcome_error);

    // Update bridge
    bg_fep_update(bridge, 16.0f);

    // Verify stats
    bg_fep_stats_t stats;
    bg_fep_get_stats(bridge, &stats);
    EXPECT_EQ(stats.action_selections, 1u);
}

TEST_F(BasalGangliaFEPBridgeTest, ExplorationVsExploitation) {
    // Test that exploration model increases epistemic weight effect
    bg_fep_set_model(bridge, BG_FEP_MODEL_EXPLORE);

    float action_values[2] = {0.6f, 0.4f};
    bg_fep_action_eval_t evaluations[2];

    // With exploration, uncertain action (higher variance) should be more valuable
    bg_fep_evaluate_actions(bridge, action_values, 2, evaluations);

    // Entropy should be higher with exploration mode
    float entropy = bg_fep_get_policy_entropy(bridge);
    EXPECT_GT(entropy, 0.0f);
}

TEST_F(BasalGangliaFEPBridgeTest, LearningImprovesPredictions) {
    // Train outcome model with consistent reward
    for (int i = 0; i < 20; i++) {
        bg_fep_update_outcome_model(bridge, 0, 0.8f, 0.1f, true);
    }

    // Outcome model should converge to observed values
    bg_fep_outcome_model_t outcome;
    bg_fep_predict_outcome(bridge, 0, &outcome);

    EXPECT_NEAR(outcome.reward_mean, 0.8f, 0.1f);
    EXPECT_NEAR(outcome.success_probability, 1.0f, 0.1f);
}

TEST_F(BasalGangliaFEPBridgeTest, DopamineModulatesPrecision) {
    float baseline_precision = bg_fep_get_dopamine_precision(bridge);

    // High dopamine
    bg_fep_set_dopamine(bridge, 0.9f);
    float high_da_precision = bg_fep_get_dopamine_precision(bridge);

    // Low dopamine
    bg_fep_set_dopamine(bridge, 0.1f);
    float low_da_precision = bg_fep_get_dopamine_precision(bridge);

    EXPECT_GT(high_da_precision, baseline_precision);
    EXPECT_LT(low_da_precision, high_da_precision);
}
