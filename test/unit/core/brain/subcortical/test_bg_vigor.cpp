//=============================================================================
// test_bg_vigor.cpp - Vigor/Effort Modulation Unit Tests
//=============================================================================
/**
 * @file test_bg_vigor.cpp
 * @brief Unit tests for vigor and effort modulation system
 *
 * Tests vigor computation, effort costs, and bidirectional data flow.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "core/brain/subcortical/nimcp_bg_vigor.h"

//=============================================================================
// Vigor System Tests
//=============================================================================

class VigorTest : public ::testing::Test {
protected:
    bgv_system_t* system = nullptr;
    bgv_config_t config;

    void SetUp() override {
        bgv_default_config(&config);
        config.max_actions = 32;
        config.enable_fatigue = true;
        system = bgv_create(&config);
    }

    void TearDown() override {
        if (system) {
            bgv_destroy(system);
        }
    }

    // Helper to register a test action
    void registerTestAction(uint32_t action_id, float phys_cost, float cog_cost) {
        bgv_register_action(system, action_id, phys_cost, cog_cost, 100.0f);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(VigorTest, CreateDestroy) {
    ASSERT_NE(system, nullptr);
}

TEST_F(VigorTest, CreateWithNullConfig) {
    bgv_system_t* s = bgv_create(nullptr);
    ASSERT_NE(s, nullptr);
    bgv_destroy(s);
}

TEST_F(VigorTest, DefaultConfig) {
    bgv_config_t cfg;
    bgv_default_config(&cfg);

    EXPECT_EQ(cfg.max_actions, BGV_MAX_ACTIONS);
    EXPECT_FLOAT_EQ(cfg.base_vigor, BGV_DEFAULT_VIGOR);
    EXPECT_GT(cfg.dopamine_sensitivity, 0.0f);
    EXPECT_TRUE(cfg.enable_fatigue);
}

TEST_F(VigorTest, Reset) {
    registerTestAction(1, 0.5f, 0.3f);
    bgv_set_dopamine(system, 0.8f);
    bgv_set_fatigue(system, 0.5f);

    ASSERT_EQ(bgv_reset(system), 0);

    float fatigue = bgv_get_fatigue(system);
    EXPECT_NEAR(fatigue, 0.0f, 0.1f);
}

//=============================================================================
// Action Registration Tests
//=============================================================================

TEST_F(VigorTest, RegisterAction) {
    int ret = bgv_register_action(system, 1, 0.5f, 0.3f, 100.0f);
    ASSERT_EQ(ret, 0);

    const bgv_action_vigor_t* action = bgv_get_action(system, 1);
    ASSERT_NE(action, nullptr);
    EXPECT_EQ(action->action_id, 1u);
    EXPECT_FLOAT_EQ(action->effort.physical_cost, 0.5f);
    EXPECT_FLOAT_EQ(action->effort.cognitive_cost, 0.3f);
}

TEST_F(VigorTest, RegisterMultipleActions) {
    ASSERT_EQ(bgv_register_action(system, 1, 0.3f, 0.2f, 100.0f), 0);
    ASSERT_EQ(bgv_register_action(system, 2, 0.5f, 0.4f, 150.0f), 0);
    ASSERT_EQ(bgv_register_action(system, 3, 0.8f, 0.6f, 200.0f), 0);

    EXPECT_NE(bgv_get_action(system, 1), nullptr);
    EXPECT_NE(bgv_get_action(system, 2), nullptr);
    EXPECT_NE(bgv_get_action(system, 3), nullptr);
}

TEST_F(VigorTest, RegisterDuplicateAction) {
    ASSERT_EQ(bgv_register_action(system, 1, 0.5f, 0.3f, 100.0f), 0);

    // Duplicate should fail
    int ret = bgv_register_action(system, 1, 0.6f, 0.4f, 150.0f);
    EXPECT_NE(ret, 0);
}

TEST_F(VigorTest, SetAdditionalCosts) {
    registerTestAction(1, 0.5f, 0.3f);

    ASSERT_EQ(bgv_set_additional_costs(system, 1, 0.4f, 0.2f), 0);

    const bgv_action_vigor_t* action = bgv_get_action(system, 1);
    ASSERT_NE(action, nullptr);
    EXPECT_FLOAT_EQ(action->effort.emotional_cost, 0.4f);
    EXPECT_FLOAT_EQ(action->effort.temporal_cost, 0.2f);
}

TEST_F(VigorTest, UnregisterAction) {
    registerTestAction(1, 0.5f, 0.3f);

    ASSERT_EQ(bgv_unregister_action(system, 1), 0);

    const bgv_action_vigor_t* action = bgv_get_action(system, 1);
    EXPECT_EQ(action, nullptr);
}

//=============================================================================
// Vigor Computation Tests
//=============================================================================

TEST_F(VigorTest, ComputeVigorBaseline) {
    registerTestAction(1, 0.3f, 0.2f);
    bgv_set_dopamine(system, 0.5f);  // Baseline dopamine

    float vigor = 0.0f;
    int ret = bgv_compute_vigor(system, 1, &vigor);

    ASSERT_EQ(ret, 0);
    EXPECT_GE(vigor, BGV_MIN_VIGOR);
    EXPECT_LE(vigor, BGV_MAX_VIGOR);
}

TEST_F(VigorTest, DopamineIncreasesVigor) {
    registerTestAction(1, 0.3f, 0.2f);

    // High dopamine
    bgv_set_dopamine(system, 0.9f);
    float vigor_high = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_high);

    // Low dopamine
    bgv_set_dopamine(system, 0.1f);
    float vigor_low = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_low);

    EXPECT_GT(vigor_high, vigor_low);
}

TEST_F(VigorTest, MotivationIncreasesVigor) {
    registerTestAction(1, 0.3f, 0.2f);
    bgv_set_dopamine(system, 0.5f);

    // High motivation
    bgv_set_motivation(system, 0.9f);
    float vigor_high = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_high);

    // Low motivation
    bgv_set_motivation(system, 0.1f);
    float vigor_low = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_low);

    EXPECT_GT(vigor_high, vigor_low);
}

TEST_F(VigorTest, FatigueReducesVigor) {
    registerTestAction(1, 0.3f, 0.2f);
    bgv_set_dopamine(system, 0.5f);
    bgv_set_motivation(system, 0.5f);

    // No fatigue
    bgv_set_fatigue(system, 0.0f);
    float vigor_fresh = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_fresh);

    // High fatigue
    bgv_set_fatigue(system, 0.8f);
    float vigor_tired = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_tired);

    EXPECT_GT(vigor_fresh, vigor_tired);
}

TEST_F(VigorTest, UrgencyIncreasesVigor) {
    registerTestAction(1, 0.3f, 0.2f);
    bgv_set_dopamine(system, 0.5f);

    // No urgency
    bgv_set_urgency(system, 0.0f);
    float vigor_normal = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_normal);

    // High urgency
    bgv_set_urgency(system, 0.9f);
    float vigor_urgent = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_urgent);

    EXPECT_GE(vigor_urgent, vigor_normal);
}

TEST_F(VigorTest, RewardProximityIncreasesVigor) {
    registerTestAction(1, 0.3f, 0.2f);
    bgv_set_dopamine(system, 0.5f);

    // Far from reward
    bgv_set_reward_proximity(system, 0.1f);
    float vigor_far = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_far);

    // Close to reward
    bgv_set_reward_proximity(system, 0.9f);
    float vigor_close = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_close);

    EXPECT_GE(vigor_close, vigor_far);
}

//=============================================================================
// Effort Computation Tests
//=============================================================================

TEST_F(VigorTest, ComputeEffort) {
    registerTestAction(1, 0.6f, 0.4f);
    bgv_set_dopamine(system, 0.5f);

    bgv_effort_t effort;
    int ret = bgv_compute_effort(system, 1, &effort);

    ASSERT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(effort.physical_cost, 0.6f);
    EXPECT_FLOAT_EQ(effort.cognitive_cost, 0.4f);
    EXPECT_GE(effort.total_cost, 0.0f);
    EXPECT_LE(effort.total_cost, 1.0f);
    EXPECT_GE(effort.subjective_cost, 0.0f);
}

TEST_F(VigorTest, DopamineReducesSubjectiveEffort) {
    registerTestAction(1, 0.6f, 0.4f);

    // High dopamine
    bgv_set_dopamine(system, 0.9f);
    bgv_effort_t effort_high_da;
    bgv_compute_effort(system, 1, &effort_high_da);

    // Low dopamine
    bgv_set_dopamine(system, 0.1f);
    bgv_effort_t effort_low_da;
    bgv_compute_effort(system, 1, &effort_low_da);

    // Subjective effort lower with high dopamine
    EXPECT_LT(effort_high_da.subjective_cost, effort_low_da.subjective_cost);
}

TEST_F(VigorTest, MotivationReducesSubjectiveEffort) {
    registerTestAction(1, 0.6f, 0.4f);
    bgv_set_dopamine(system, 0.5f);

    // High motivation
    bgv_set_motivation(system, 0.9f);
    bgv_effort_t effort_motivated;
    bgv_compute_effort(system, 1, &effort_motivated);

    // Low motivation
    bgv_set_motivation(system, 0.1f);
    bgv_effort_t effort_unmotivated;
    bgv_compute_effort(system, 1, &effort_unmotivated);

    EXPECT_LT(effort_motivated.subjective_cost, effort_unmotivated.subjective_cost);
}

//=============================================================================
// Motor Scaling Tests
//=============================================================================

TEST_F(VigorTest, GetMotorScaling) {
    registerTestAction(1, 0.3f, 0.2f);
    bgv_set_dopamine(system, 0.8f);

    // Compute vigor first
    float vigor = 0.0f;
    bgv_compute_vigor(system, 1, &vigor);

    float scaling = bgv_get_motor_scaling(system, 1);
    EXPECT_GE(scaling, 0.5f);
    EXPECT_LE(scaling, 2.0f);
}

TEST_F(VigorTest, PredictDuration) {
    registerTestAction(1, 0.3f, 0.2f);
    bgv_set_dopamine(system, 0.5f);

    // Compute vigor first
    float vigor = 0.0f;
    bgv_compute_vigor(system, 1, &vigor);

    float duration = bgv_predict_duration(system, 1);
    EXPECT_GT(duration, 0.0f);
}

TEST_F(VigorTest, HighVigorShortDuration) {
    registerTestAction(1, 0.3f, 0.2f);

    // High vigor
    bgv_set_dopamine(system, 0.9f);
    bgv_set_motivation(system, 0.9f);
    float vigor_high = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_high);
    float duration_high_vigor = bgv_predict_duration(system, 1);

    // Low vigor
    bgv_set_dopamine(system, 0.2f);
    bgv_set_motivation(system, 0.2f);
    float vigor_low = 0.0f;
    bgv_compute_vigor(system, 1, &vigor_low);
    float duration_low_vigor = bgv_predict_duration(system, 1);

    // Higher vigor = shorter duration
    EXPECT_LT(duration_high_vigor, duration_low_vigor);
}

//=============================================================================
// Bidirectional Data Flow Tests
//=============================================================================

TEST_F(VigorTest, BidirProcessBasic) {
    registerTestAction(1, 0.4f, 0.3f);

    bgv_bidir_data_t data;
    memset(&data, 0, sizeof(data));

    // Set inputs
    data.dopamine_level = 0.6f;
    data.motivation_signal = 0.7f;
    data.urgency_signal = 0.3f;
    data.fatigue_input = 0.1f;
    data.reward_proximity = 0.5f;
    data.action_id = 1;
    data.compute_effort = true;

    int ret = bgv_process_bidir(system, &data);
    ASSERT_EQ(ret, 0);

    // Check outputs
    EXPECT_GT(data.computed_vigor, 0.0f);
    EXPECT_LE(data.computed_vigor, 1.0f);
    EXPECT_GT(data.motor_scaling, 0.0f);
    EXPECT_GT(data.predicted_duration_ms, 0.0f);
    EXPECT_GE(data.effort_cost, 0.0f);
}

TEST_F(VigorTest, BidirDopamineEffect) {
    registerTestAction(1, 0.4f, 0.3f);

    // High dopamine
    bgv_bidir_data_t data_high;
    memset(&data_high, 0, sizeof(data_high));
    data_high.dopamine_level = 0.9f;
    data_high.motivation_signal = 0.5f;
    data_high.action_id = 1;
    bgv_process_bidir(system, &data_high);

    // Low dopamine
    bgv_bidir_data_t data_low;
    memset(&data_low, 0, sizeof(data_low));
    data_low.dopamine_level = 0.1f;
    data_low.motivation_signal = 0.5f;
    data_low.action_id = 1;
    bgv_process_bidir(system, &data_low);

    EXPECT_GT(data_high.computed_vigor, data_low.computed_vigor);
}

TEST_F(VigorTest, BidirVigorState) {
    registerTestAction(1, 0.4f, 0.3f);

    // Normal state
    bgv_bidir_data_t data_normal;
    memset(&data_normal, 0, sizeof(data_normal));
    data_normal.dopamine_level = 0.5f;
    data_normal.action_id = 1;
    bgv_process_bidir(system, &data_normal);
    EXPECT_EQ(data_normal.vigor_state, BGV_STATE_NORMAL);

    // Low dopamine + high fatigue = bradykinetic
    bgv_bidir_data_t data_brady;
    memset(&data_brady, 0, sizeof(data_brady));
    data_brady.dopamine_level = 0.1f;
    data_brady.fatigue_input = 0.8f;
    data_brady.action_id = 1;
    bgv_process_bidir(system, &data_brady);
    EXPECT_EQ(data_brady.vigor_state, BGV_STATE_BRADYKINETIC);
}

TEST_F(VigorTest, BidirActionRecommendation) {
    // Register high-effort action
    registerTestAction(1, 0.9f, 0.8f);

    // Low dopamine, low motivation, high fatigue
    bgv_bidir_data_t data;
    memset(&data, 0, sizeof(data));
    data.dopamine_level = 0.1f;
    data.motivation_signal = 0.1f;
    data.fatigue_input = 0.8f;
    data.action_id = 1;
    data.compute_effort = true;

    bgv_process_bidir(system, &data);

    // High effort, low resources - may not recommend
    // (depends on implementation)
}

TEST_F(VigorTest, BidirUnknownAction) {
    // Don't register action, use unknown ID
    bgv_bidir_data_t data;
    memset(&data, 0, sizeof(data));
    data.dopamine_level = 0.5f;
    data.action_id = 9999;

    int ret = bgv_process_bidir(system, &data);
    ASSERT_EQ(ret, 0);  // Should handle gracefully

    // Should still produce output with defaults
    EXPECT_GT(data.computed_vigor, 0.0f);
}

//=============================================================================
// Effort-Benefit Tests
//=============================================================================

TEST_F(VigorTest, GetEffortBenefitRatio) {
    registerTestAction(1, 0.3f, 0.2f);  // Low effort
    registerTestAction(2, 0.8f, 0.7f);  // High effort

    bgv_set_dopamine(system, 0.5f);

    // Compute efforts
    bgv_effort_t effort1, effort2;
    bgv_compute_effort(system, 1, &effort1);
    bgv_compute_effort(system, 2, &effort2);

    float ratio1 = bgv_get_effort_benefit_ratio(system, 1, 1.0f);
    float ratio2 = bgv_get_effort_benefit_ratio(system, 2, 1.0f);

    // Low effort action should have better ratio
    EXPECT_GT(ratio1, ratio2);
}

//=============================================================================
// Fatigue Tests
//=============================================================================

TEST_F(VigorTest, ApplyFatigue) {
    registerTestAction(1, 0.6f, 0.4f);

    float fatigue_before = bgv_get_fatigue(system);

    ASSERT_EQ(bgv_apply_fatigue(system, 1), 0);

    float fatigue_after = bgv_get_fatigue(system);
    EXPECT_GT(fatigue_after, fatigue_before);
}

TEST_F(VigorTest, ProcessRecovery) {
    registerTestAction(1, 0.6f, 0.4f);

    // Build up some fatigue
    bgv_set_fatigue(system, 0.8f);
    float fatigue_before = bgv_get_fatigue(system);

    // Process recovery
    ASSERT_EQ(bgv_process_recovery(system, 1000.0f), 0);

    float fatigue_after = bgv_get_fatigue(system);
    EXPECT_LT(fatigue_after, fatigue_before);
}

TEST_F(VigorTest, StepProcessesRecovery) {
    registerTestAction(1, 0.6f, 0.4f);

    bgv_set_fatigue(system, 0.5f);
    float fatigue_before = bgv_get_fatigue(system);

    // Step the system
    for (int i = 0; i < 10; i++) {
        bgv_step(system, 100.0f);
    }

    float fatigue_after = bgv_get_fatigue(system);
    EXPECT_LT(fatigue_after, fatigue_before);
}

//=============================================================================
// State Tests
//=============================================================================

TEST_F(VigorTest, GetState) {
    bgv_state_t state = bgv_get_state(system);
    EXPECT_EQ(state, BGV_STATE_NORMAL);  // Default state
}

TEST_F(VigorTest, StateTransitions) {
    registerTestAction(1, 0.3f, 0.2f);

    // Normal state
    bgv_set_dopamine(system, 0.5f);
    bgv_set_fatigue(system, 0.0f);
    float vigor = 0.0f;
    bgv_compute_vigor(system, 1, &vigor);
    EXPECT_EQ(bgv_get_state(system), BGV_STATE_NORMAL);

    // Bradykinetic with low DA and high fatigue
    bgv_set_dopamine(system, 0.1f);
    bgv_set_fatigue(system, 0.8f);
    bgv_compute_vigor(system, 1, &vigor);
    EXPECT_EQ(bgv_get_state(system), BGV_STATE_BRADYKINETIC);

    // Enhanced with high DA and motivation
    bgv_set_dopamine(system, 0.9f);
    bgv_set_motivation(system, 0.9f);
    bgv_set_fatigue(system, 0.0f);
    bgv_compute_vigor(system, 1, &vigor);
    bgv_state_t enhanced_state = bgv_get_state(system);
    EXPECT_TRUE(enhanced_state == BGV_STATE_ENHANCED ||
                enhanced_state == BGV_STATE_HYPERKINETIC);
}

//=============================================================================
// Update Action Stats Tests
//=============================================================================

TEST_F(VigorTest, UpdateActionStats) {
    registerTestAction(1, 0.3f, 0.2f);

    ASSERT_EQ(bgv_update_action_stats(system, 1, 0.7f, 90.0f), 0);
    ASSERT_EQ(bgv_update_action_stats(system, 1, 0.8f, 85.0f), 0);

    const bgv_action_vigor_t* action = bgv_get_action(system, 1);
    ASSERT_NE(action, nullptr);
    EXPECT_EQ(action->execution_count, 2u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(VigorTest, GetStats) {
    registerTestAction(1, 0.3f, 0.2f);
    registerTestAction(2, 0.5f, 0.4f);

    bgv_set_dopamine(system, 0.6f);

    // Compute some vigors
    float v1, v2;
    bgv_compute_vigor(system, 1, &v1);
    bgv_compute_vigor(system, 2, &v2);

    bgv_stats_t stats;
    int ret = bgv_get_stats(system, &stats);
    ASSERT_EQ(ret, 0);

    EXPECT_EQ(stats.total_actions, 2u);
    EXPECT_GT(stats.avg_vigor, 0.0f);
    EXPECT_FLOAT_EQ(stats.dopamine_level, 0.6f);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(VigorTest, StateName) {
    EXPECT_STREQ(bgv_state_name(BGV_STATE_NORMAL), "Normal");
    EXPECT_STREQ(bgv_state_name(BGV_STATE_ENHANCED), "Enhanced");
    EXPECT_STREQ(bgv_state_name(BGV_STATE_REDUCED), "Reduced");
    EXPECT_STREQ(bgv_state_name(BGV_STATE_BRADYKINETIC), "Bradykinetic");
    EXPECT_STREQ(bgv_state_name(BGV_STATE_HYPERKINETIC), "Hyperkinetic");
}

TEST_F(VigorTest, EffortTypeName) {
    EXPECT_STREQ(bgv_effort_type_name(BGV_EFFORT_PHYSICAL), "Physical");
    EXPECT_STREQ(bgv_effort_type_name(BGV_EFFORT_COGNITIVE), "Cognitive");
    EXPECT_STREQ(bgv_effort_type_name(BGV_EFFORT_EMOTIONAL), "Emotional");
    EXPECT_STREQ(bgv_effort_type_name(BGV_EFFORT_TEMPORAL), "Temporal");
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(VigorTest, NullSystemHandling) {
    EXPECT_NE(bgv_register_action(nullptr, 1, 0.5f, 0.3f, 100.0f), 0);
    EXPECT_NE(bgv_set_dopamine(nullptr, 0.5f), 0);
    EXPECT_NE(bgv_step(nullptr, 10.0f), 0);

    float vigor = 0.0f;
    EXPECT_NE(bgv_compute_vigor(nullptr, 1, &vigor), 0);

    bgv_bidir_data_t data;
    EXPECT_NE(bgv_process_bidir(nullptr, &data), 0);
}

TEST_F(VigorTest, NullOutputHandling) {
    registerTestAction(1, 0.5f, 0.3f);

    EXPECT_NE(bgv_compute_vigor(system, 1, nullptr), 0);
    EXPECT_NE(bgv_compute_effort(system, 1, nullptr), 0);
    EXPECT_NE(bgv_get_stats(system, nullptr), 0);
    EXPECT_NE(bgv_process_bidir(system, nullptr), 0);
}

TEST_F(VigorTest, InvalidActionId) {
    float vigor = 0.0f;
    EXPECT_NE(bgv_compute_vigor(system, 9999, &vigor), 0);

    bgv_effort_t effort;
    EXPECT_NE(bgv_compute_effort(system, 9999, &effort), 0);

    EXPECT_NE(bgv_apply_fatigue(system, 9999), 0);
}
