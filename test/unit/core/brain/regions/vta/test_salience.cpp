/**
 * @file test_salience.cpp
 * @brief Unit tests for Incentive Salience (wanting) system
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/vta/nimcp_incentive_salience.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SalienceTest : public ::testing::Test {
protected:
    nimcp_salience_system_t system;

    void SetUp() override {
        int err = nimcp_salience_init(&system, nullptr);
        ASSERT_EQ(err, 0);
    }

    void TearDown() override {
        nimcp_salience_shutdown(&system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SalienceTest, InitSucceeds) {
    EXPECT_TRUE(system.initialized);
}

TEST_F(SalienceTest, InitNullReturnsError) {
    int err = nimcp_salience_init(nullptr, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(SalienceTest, ShutdownClearsState) {
    nimcp_salience_shutdown(&system);
    EXPECT_FALSE(system.initialized);
}

TEST_F(SalienceTest, ResetWorks) {
    system.state.wanting = 0.9f;
    int err = nimcp_salience_reset(&system);
    EXPECT_EQ(err, 0);
    EXPECT_TRUE(system.initialized);
}

TEST_F(SalienceTest, DefaultConfigValid) {
    nimcp_salience_config_t config = nimcp_salience_default_config();
    EXPECT_GT(config.da_wanting_gain, 0.0f);
    EXPECT_GE(config.wanting_baseline, 0.0f);
}

TEST_F(SalienceTest, CustomConfigApplied) {
    nimcp_salience_shutdown(&system);

    nimcp_salience_config_t config = nimcp_salience_default_config();
    config.da_wanting_gain = 2.0f;
    config.effort_sensitivity = 0.8f;

    nimcp_salience_init(&system, &config);

    EXPECT_FLOAT_EQ(system.config.da_wanting_gain, 2.0f);
    EXPECT_FLOAT_EQ(system.config.effort_sensitivity, 0.8f);
}

//=============================================================================
// Core Salience Tests
//=============================================================================

TEST_F(SalienceTest, UpdateSucceeds) {
    int err = nimcp_salience_update(&system, 50.0f, 10.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(SalienceTest, UpdateNullReturnsError) {
    EXPECT_EQ(nimcp_salience_update(nullptr, 50.0f, 10.0f), -1);
}

TEST_F(SalienceTest, UpdateWithNegativeDtDoesNotCrash) {
    /* Implementation does not validate dt - just verify no crash */
    int err = nimcp_salience_update(&system, 50.0f, -1.0f);
    EXPECT_EQ(err, 0);  /* Current implementation accepts any dt */
}

TEST_F(SalienceTest, ComputeWantingSucceeds) {
    float wanting;
    int err = nimcp_salience_compute_wanting(&system, 100.0f, &wanting);
    EXPECT_EQ(err, 0);
    EXPECT_GE(wanting, 0.0f);
    EXPECT_LE(wanting, 1.0f);
}

TEST_F(SalienceTest, HighDAIncreasesWanting) {
    float low_wanting, high_wanting;

    nimcp_salience_compute_wanting(&system, 20.0f, &low_wanting);
    nimcp_salience_compute_wanting(&system, 100.0f, &high_wanting);

    EXPECT_GT(high_wanting, low_wanting);
}

TEST_F(SalienceTest, GetWantingSucceeds) {
    float wanting;
    int err = nimcp_salience_get_wanting(&system, &wanting);
    EXPECT_EQ(err, 0);
    EXPECT_GE(wanting, 0.0f);
    EXPECT_LE(wanting, 1.0f);
}

TEST_F(SalienceTest, GetMotivationSucceeds) {
    nimcp_motivation_level_t level;
    int err = nimcp_salience_get_motivation(&system, &level);
    EXPECT_EQ(err, 0);
}

TEST_F(SalienceTest, GetVigorSucceeds) {
    float vigor;
    int err = nimcp_salience_get_vigor(&system, &vigor);
    EXPECT_EQ(err, 0);
    EXPECT_GE(vigor, 0.0f);
    EXPECT_LE(vigor, 1.0f);
}

//=============================================================================
// Goal Tests
//=============================================================================

TEST_F(SalienceTest, AddGoalSucceeds) {
    uint32_t goal_id;
    int err = nimcp_salience_add_goal(&system, GOAL_PRIMARY, 1.0f, 0.5f, 1000.0f, &goal_id);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.num_goals, 1u);
}

TEST_F(SalienceTest, GoalCapacityEnforced) {
    for (int i = 0; i < SALIENCE_MAX_GOALS; i++) {
        uint32_t id;
        nimcp_salience_add_goal(&system, GOAL_PRIMARY, 1.0f, 0.5f, 1000.0f, &id);
    }

    uint32_t id;
    int err = nimcp_salience_add_goal(&system, GOAL_PRIMARY, 1.0f, 0.5f, 1000.0f, &id);
    EXPECT_EQ(err, -1);
}

TEST_F(SalienceTest, RemoveGoalSucceeds) {
    uint32_t goal_id;
    nimcp_salience_add_goal(&system, GOAL_PRIMARY, 1.0f, 0.5f, 1000.0f, &goal_id);

    int err = nimcp_salience_remove_goal(&system, goal_id);
    EXPECT_EQ(err, 0);
}

TEST_F(SalienceTest, SetActiveGoalSucceeds) {
    uint32_t goal_id;
    nimcp_salience_add_goal(&system, GOAL_PRIMARY, 1.0f, 0.5f, 1000.0f, &goal_id);

    int err = nimcp_salience_set_active_goal(&system, goal_id);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.active_goal_id, goal_id);
}

TEST_F(SalienceTest, UpdateGoalProgressSucceeds) {
    uint32_t goal_id;
    nimcp_salience_add_goal(&system, GOAL_PRIMARY, 1.0f, 0.5f, 1000.0f, &goal_id);

    int err = nimcp_salience_update_goal_progress(&system, goal_id, 0.5f);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(system.goals[goal_id].distance, 0.5f);
}

TEST_F(SalienceTest, GoalAchievedSucceeds) {
    uint32_t goal_id;
    nimcp_salience_add_goal(&system, GOAL_PRIMARY, 1.0f, 0.5f, 1000.0f, &goal_id);

    int err = nimcp_salience_goal_achieved(&system, goal_id, 1.0f);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.goals_achieved, 1u);
}

TEST_F(SalienceTest, GetGoalWantingSucceeds) {
    uint32_t goal_id;
    nimcp_salience_add_goal(&system, GOAL_PRIMARY, 1.0f, 0.3f, 500.0f, &goal_id);

    float wanting;
    int err = nimcp_salience_get_goal_wanting(&system, goal_id, &wanting);
    EXPECT_EQ(err, 0);
    EXPECT_GE(wanting, 0.0f);
}

//=============================================================================
// Effort-Utility Tests
//=============================================================================

TEST_F(SalienceTest, ComputeUtilitySucceeds) {
    nimcp_utility_result_t result;
    int err = nimcp_salience_compute_utility(&system, 1.0f, 0.5f, 1000.0f, 0.8f, &result);
    EXPECT_EQ(err, 0);
}

TEST_F(SalienceTest, HighEffortReducesUtility) {
    nimcp_utility_result_t low_effort, high_effort;

    nimcp_salience_compute_utility(&system, 1.0f, 0.2f, 1000.0f, 1.0f, &low_effort);
    nimcp_salience_compute_utility(&system, 1.0f, 0.9f, 1000.0f, 1.0f, &high_effort);

    EXPECT_GT(low_effort.net_utility, high_effort.net_utility);
}

TEST_F(SalienceTest, ComputeEffortCostSucceeds) {
    float cost;
    int err = nimcp_salience_compute_effort_cost(&system, 0.5f, EFFORT_PHYSICAL, &cost);
    EXPECT_EQ(err, 0);
    EXPECT_GT(cost, 0.0f);
}

TEST_F(SalienceTest, ApplyDelayDiscountSucceeds) {
    float discounted;
    int err = nimcp_salience_apply_delay_discount(&system, 1.0f, 1000.0f, &discounted);
    EXPECT_EQ(err, 0);
    EXPECT_LT(discounted, 1.0f);  /* Delayed reward should be worth less */
}

TEST_F(SalienceTest, IsWorthPursuingSucceeds) {
    bool worth_it;
    int err = nimcp_salience_is_worth_pursuing(&system, 1.0f, 0.3f, 1000.0f, &worth_it);
    EXPECT_EQ(err, 0);
}

TEST_F(SalienceTest, HighRewardLowEffortWorthPursuing) {
    bool worth_it;
    nimcp_salience_is_worth_pursuing(&system, 1.0f, 0.1f, 100.0f, &worth_it);
    EXPECT_TRUE(worth_it);
}

TEST_F(SalienceTest, LowRewardHighEffortNotWorth) {
    bool worth_it;
    nimcp_salience_is_worth_pursuing(&system, 0.1f, 0.9f, 10000.0f, &worth_it);
    EXPECT_FALSE(worth_it);
}

//=============================================================================
// Cue Tests
//=============================================================================

TEST_F(SalienceTest, AddCueSucceeds) {
    uint32_t cue_id;
    int err = nimcp_salience_add_cue(&system, 0.5f, &cue_id);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.num_cues, 1u);
}

TEST_F(SalienceTest, CueCapacityEnforced) {
    for (int i = 0; i < SALIENCE_MAX_CUES; i++) {
        uint32_t id;
        nimcp_salience_add_cue(&system, 0.5f, &id);
    }

    uint32_t id;
    int err = nimcp_salience_add_cue(&system, 0.5f, &id);
    EXPECT_EQ(err, -1);
}

TEST_F(SalienceTest, CuePresentSucceeds) {
    uint32_t cue_id;
    nimcp_salience_add_cue(&system, 0.5f, &cue_id);

    int err = nimcp_salience_cue_present(&system, cue_id);
    EXPECT_EQ(err, 0);
    EXPECT_TRUE(system.cues[cue_id].present);
}

TEST_F(SalienceTest, CueAbsentSucceeds) {
    uint32_t cue_id;
    nimcp_salience_add_cue(&system, 0.5f, &cue_id);
    nimcp_salience_cue_present(&system, cue_id);

    int err = nimcp_salience_cue_absent(&system, cue_id);
    EXPECT_EQ(err, 0);
    EXPECT_FALSE(system.cues[cue_id].present);
}

TEST_F(SalienceTest, UpdateCueSucceeds) {
    uint32_t cue_id;
    nimcp_salience_add_cue(&system, 0.3f, &cue_id);
    nimcp_salience_cue_present(&system, cue_id);

    int err = nimcp_salience_update_cue(&system, cue_id, 1.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(SalienceTest, GetCueWantingSucceeds) {
    float cue_wanting;
    int err = nimcp_salience_get_cue_wanting(&system, &cue_wanting);
    EXPECT_EQ(err, 0);
    EXPECT_GE(cue_wanting, 0.0f);
}

TEST_F(SalienceTest, CuePresentIncreasesWanting) {
    uint32_t cue_id;
    nimcp_salience_add_cue(&system, 0.8f, &cue_id);

    float before;
    nimcp_salience_get_cue_wanting(&system, &before);

    nimcp_salience_cue_present(&system, cue_id);

    float after;
    nimcp_salience_get_cue_wanting(&system, &after);

    EXPECT_GT(after, before);
}

//=============================================================================
// Liking Tests
//=============================================================================

TEST_F(SalienceTest, SignalLikingSucceeds) {
    int err = nimcp_salience_signal_liking(&system, 0.8f);
    EXPECT_EQ(err, 0);
}

TEST_F(SalienceTest, GetLikingSucceeds) {
    nimcp_salience_signal_liking(&system, 0.7f);

    float liking;
    int err = nimcp_salience_get_liking(&system, &liking);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(liking, 0.7f);
}

TEST_F(SalienceTest, GetWantingLikingRatioSucceeds) {
    float ratio;
    int err = nimcp_salience_get_wanting_liking_ratio(&system, &ratio);
    EXPECT_EQ(err, 0);
}

TEST_F(SalienceTest, LikingClampedToRange) {
    nimcp_salience_signal_liking(&system, 1.5f);
    float liking;
    nimcp_salience_get_liking(&system, &liking);
    EXPECT_LE(liking, 1.0f);

    nimcp_salience_signal_liking(&system, -0.5f);
    nimcp_salience_get_liking(&system, &liking);
    EXPECT_GE(liking, 0.0f);
}

//=============================================================================
// State Tests
//=============================================================================

TEST_F(SalienceTest, GetStateSucceeds) {
    nimcp_salience_state_t state;
    int err = nimcp_salience_get_state(&system, &state);
    EXPECT_EQ(err, 0);
    EXPECT_GE(state.wanting, 0.0f);
    EXPECT_LE(state.wanting, 1.0f);
}

TEST_F(SalienceTest, LongTermStability) {
    for (int i = 0; i < 10000; i++) {
        nimcp_salience_update(&system, 50.0f + (float)(i % 50), 1.0f);
    }

    nimcp_salience_state_t state;
    nimcp_salience_get_state(&system, &state);

    EXPECT_FALSE(std::isnan(state.wanting));
    EXPECT_FALSE(std::isinf(state.wanting));
    EXPECT_GE(state.wanting, 0.0f);
    EXPECT_LE(state.wanting, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
