/**
 * @file test_nimcp_shared_intentionality.cpp
 * @brief Unit tests for shared intentionality (joint goals and we-mode)
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/collective_cognition/nimcp_shared_intentionality.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class SharedIntentionalityTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = shared_intentionality_default_config();
        si_ = shared_intentionality_create(&config_);
        ASSERT_NE(si_, nullptr);
    }

    void TearDown() override {
        if (si_) {
            shared_intentionality_destroy(si_);
            si_ = nullptr;
        }
    }

    shared_goal_t CreateTestGoal(const char* description, float priority) {
        shared_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        strncpy(goal.description, description, sizeof(goal.description) - 1);
        goal.priority = priority;
        goal.proposer_id = 1;
        goal.state = GOAL_STATE_PROPOSED;
        goal.progress = 0.0f;
        return goal;
    }

    joint_attention_t CreateTestAttention(float salience) {
        joint_attention_t attention;
        memset(&attention, 0, sizeof(attention));
        attention.salience = salience;
        attention.proposer_id = 1;
        for (int i = 0; i < SI_MAX_FEATURE_DIM; i++) {
            attention.feature_vector[i] = 0.1f * i;
        }
        return attention;
    }

    shared_intentionality_config_t config_;
    shared_intentionality_t* si_ = nullptr;
};

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, CreateWithNullConfig) {
    shared_intentionality_t* si = shared_intentionality_create(nullptr);
    ASSERT_NE(si, nullptr);
    shared_intentionality_destroy(si);
}

TEST_F(SharedIntentionalityTest, DestroyNull) {
    shared_intentionality_destroy(nullptr);  // Should not crash
}

TEST_F(SharedIntentionalityTest, Reset) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal = CreateTestGoal("Test Goal", 0.5f);
    ASSERT_NE(shared_intentionality_propose_goal(si_, &goal), 0u);

    EXPECT_EQ(shared_intentionality_reset(si_), 0);

    shared_goal_t goals[10];
    EXPECT_EQ(shared_intentionality_get_active_goals(si_, goals, 10), 0u);
}

/*=============================================================================
 * Instance Management Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, RegisterInstance) {
    EXPECT_EQ(shared_intentionality_register_instance(si_, 1), 0);
}

TEST_F(SharedIntentionalityTest, RegisterMultipleInstances) {
    EXPECT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    EXPECT_EQ(shared_intentionality_register_instance(si_, 2), 0);
    EXPECT_EQ(shared_intentionality_register_instance(si_, 3), 0);
}

TEST_F(SharedIntentionalityTest, RegisterDuplicateInstance) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    EXPECT_EQ(shared_intentionality_register_instance(si_, 1), -1);
}

TEST_F(SharedIntentionalityTest, UnregisterInstance) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    EXPECT_EQ(shared_intentionality_unregister_instance(si_, 1), 0);
}

TEST_F(SharedIntentionalityTest, UnregisterNonexistentInstance) {
    EXPECT_EQ(shared_intentionality_unregister_instance(si_, 999), -1);
}

/*=============================================================================
 * Shared Goal Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, ProposeGoal) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal = CreateTestGoal("Explore Area", 0.8f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    EXPECT_NE(goal_id, 0u);
}

TEST_F(SharedIntentionalityTest, CommitToGoal) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    shared_goal_t goal = CreateTestGoal("Defend Position", 0.9f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    EXPECT_EQ(shared_intentionality_commit_to_goal(si_, goal_id, 2, 0.8f), 0);
}

TEST_F(SharedIntentionalityTest, CommitToNonexistentGoal) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    EXPECT_EQ(shared_intentionality_commit_to_goal(si_, 999, 1, 0.5f), -1);
}

TEST_F(SharedIntentionalityTest, WithdrawFromGoal) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    shared_goal_t goal = CreateTestGoal("Mission", 0.7f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    ASSERT_EQ(shared_intentionality_commit_to_goal(si_, goal_id, 2, 0.8f), 0);
    EXPECT_EQ(shared_intentionality_withdraw_from_goal(si_, goal_id, 2), 0);
}

TEST_F(SharedIntentionalityTest, UpdateGoalProgress) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal = CreateTestGoal("Build Structure", 0.6f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    EXPECT_EQ(shared_intentionality_update_goal_progress(si_, goal_id, 0.5f), 0);

    shared_goal_t retrieved;
    ASSERT_EQ(shared_intentionality_get_goal(si_, goal_id, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.progress, 0.5f);
}

TEST_F(SharedIntentionalityTest, CompleteGoal) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal = CreateTestGoal("Task", 0.5f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    EXPECT_EQ(shared_intentionality_complete_goal(si_, goal_id), 0);

    shared_goal_t retrieved;
    ASSERT_EQ(shared_intentionality_get_goal(si_, goal_id, &retrieved), 0);
    EXPECT_EQ(retrieved.state, GOAL_STATE_COMPLETED);
}

TEST_F(SharedIntentionalityTest, FailGoal) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal = CreateTestGoal("Risky Mission", 0.5f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    EXPECT_EQ(shared_intentionality_fail_goal(si_, goal_id), 0);

    shared_goal_t retrieved;
    ASSERT_EQ(shared_intentionality_get_goal(si_, goal_id, &retrieved), 0);
    EXPECT_EQ(retrieved.state, GOAL_STATE_FAILED);
}

TEST_F(SharedIntentionalityTest, GetGoal) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal = CreateTestGoal("Retrieve Item", 0.7f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    shared_goal_t retrieved;
    ASSERT_EQ(shared_intentionality_get_goal(si_, goal_id, &retrieved), 0);
    EXPECT_STREQ(retrieved.description, "Retrieve Item");
    EXPECT_FLOAT_EQ(retrieved.priority, 0.7f);
}

TEST_F(SharedIntentionalityTest, GetNonexistentGoal) {
    shared_goal_t goal;
    EXPECT_EQ(shared_intentionality_get_goal(si_, 999, &goal), -1);
}

TEST_F(SharedIntentionalityTest, GetActiveGoals) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal1 = CreateTestGoal("Goal 1", 0.5f);
    shared_goal_t goal2 = CreateTestGoal("Goal 2", 0.6f);
    shared_goal_t goal3 = CreateTestGoal("Goal 3", 0.7f);

    shared_intentionality_propose_goal(si_, &goal1);
    shared_intentionality_propose_goal(si_, &goal2);
    shared_intentionality_propose_goal(si_, &goal3);

    shared_goal_t active[10];
    uint32_t count = shared_intentionality_get_active_goals(si_, active, 10);
    EXPECT_EQ(count, 3u);
}

/*=============================================================================
 * Joint Attention Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, ProposeAttention) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    joint_attention_t attention = CreateTestAttention(0.8f);
    uint32_t attention_id = shared_intentionality_propose_attention(si_, &attention);
    EXPECT_NE(attention_id, 0u);
}

TEST_F(SharedIntentionalityTest, JoinAttention) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    joint_attention_t attention = CreateTestAttention(0.9f);
    uint32_t attention_id = shared_intentionality_propose_attention(si_, &attention);
    ASSERT_NE(attention_id, 0u);

    EXPECT_EQ(shared_intentionality_join_attention(si_, attention_id, 2), 0);
}

TEST_F(SharedIntentionalityTest, JoinNonexistentAttention) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    EXPECT_EQ(shared_intentionality_join_attention(si_, 999, 1), -1);
}

TEST_F(SharedIntentionalityTest, LeaveAttention) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    joint_attention_t attention = CreateTestAttention(0.7f);
    uint32_t attention_id = shared_intentionality_propose_attention(si_, &attention);
    ASSERT_NE(attention_id, 0u);

    ASSERT_EQ(shared_intentionality_join_attention(si_, attention_id, 2), 0);
    EXPECT_EQ(shared_intentionality_leave_attention(si_, attention_id, 2), 0);
}

TEST_F(SharedIntentionalityTest, GetAttention) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    joint_attention_t attention = CreateTestAttention(0.85f);
    uint32_t attention_id = shared_intentionality_propose_attention(si_, &attention);
    ASSERT_NE(attention_id, 0u);

    joint_attention_t retrieved;
    ASSERT_EQ(shared_intentionality_get_attention(si_, attention_id, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.salience, 0.85f);
}

TEST_F(SharedIntentionalityTest, GetActiveAttentions) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    joint_attention_t att1 = CreateTestAttention(0.5f);
    joint_attention_t att2 = CreateTestAttention(0.6f);

    shared_intentionality_propose_attention(si_, &att1);
    shared_intentionality_propose_attention(si_, &att2);

    joint_attention_t active[10];
    uint32_t count = shared_intentionality_get_active_attentions(si_, active, 10);
    EXPECT_EQ(count, 2u);
}

/*=============================================================================
 * Role Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, AssignRole) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    shared_goal_t goal = CreateTestGoal("Team Task", 0.8f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    ASSERT_EQ(shared_intentionality_commit_to_goal(si_, goal_id, 2, 0.9f), 0);
    EXPECT_EQ(shared_intentionality_assign_role(si_, goal_id, 2, ROLE_EXECUTOR), 0);
}

TEST_F(SharedIntentionalityTest, AssignRoleNonexistentGoal) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    EXPECT_EQ(shared_intentionality_assign_role(si_, 999, 1, ROLE_LEADER), -1);
}

TEST_F(SharedIntentionalityTest, NegotiateRoles) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 3), 0);

    shared_goal_t goal = CreateTestGoal("Complex Mission", 0.9f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    // All instances commit
    shared_intentionality_commit_to_goal(si_, goal_id, 1, 0.8f);
    shared_intentionality_commit_to_goal(si_, goal_id, 2, 0.7f);
    shared_intentionality_commit_to_goal(si_, goal_id, 3, 0.6f);

    EXPECT_EQ(shared_intentionality_negotiate_roles(si_, goal_id), 0);
}

TEST_F(SharedIntentionalityTest, GetRole) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    shared_goal_t goal = CreateTestGoal("Task", 0.5f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    shared_intentionality_commit_to_goal(si_, goal_id, 2, 0.8f);
    shared_intentionality_assign_role(si_, goal_id, 2, ROLE_VERIFIER);

    role_assignment_t assignment;
    ASSERT_EQ(shared_intentionality_get_role(si_, goal_id, 2, &assignment), 0);
    EXPECT_EQ(assignment.role, ROLE_VERIFIER);
}

/*=============================================================================
 * We-Mode Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, GetWeMode) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    we_mode_state_t state;
    ASSERT_EQ(shared_intentionality_get_we_mode(si_, &state), 0);
    EXPECT_GE(state.we_mode_strength, 0.0f);
    EXPECT_LE(state.we_mode_strength, 1.0f);
}

TEST_F(SharedIntentionalityTest, IsWeModeActiveInitially) {
    EXPECT_FALSE(shared_intentionality_is_we_mode_active(si_));
}

TEST_F(SharedIntentionalityTest, EnterWeMode) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    EXPECT_EQ(shared_intentionality_enter_we_mode(si_), 0);
    EXPECT_TRUE(shared_intentionality_is_we_mode_active(si_));
}

TEST_F(SharedIntentionalityTest, ExitWeMode) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    shared_intentionality_enter_we_mode(si_);
    ASSERT_TRUE(shared_intentionality_is_we_mode_active(si_));

    EXPECT_EQ(shared_intentionality_exit_we_mode(si_), 0);
    EXPECT_FALSE(shared_intentionality_is_we_mode_active(si_));
}

TEST_F(SharedIntentionalityTest, WeModeEmergence) {
    // We-mode should emerge naturally with strong commitments
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 3), 0);

    shared_goal_t goal = CreateTestGoal("Important Mission", 1.0f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    // Strong commitments from all
    shared_intentionality_commit_to_goal(si_, goal_id, 1, 0.95f);
    shared_intentionality_commit_to_goal(si_, goal_id, 2, 0.90f);
    shared_intentionality_commit_to_goal(si_, goal_id, 3, 0.85f);

    // Joint attention
    joint_attention_t attention = CreateTestAttention(0.9f);
    uint32_t att_id = shared_intentionality_propose_attention(si_, &attention);
    shared_intentionality_join_attention(si_, att_id, 2);
    shared_intentionality_join_attention(si_, att_id, 3);

    ASSERT_EQ(shared_intentionality_update(si_), 0);

    we_mode_state_t state;
    ASSERT_EQ(shared_intentionality_get_we_mode(si_, &state), 0);
    // With strong commitments and joint attention, we-mode strength should be elevated
    EXPECT_GT(state.joint_commitment, 0.5f);
}

/*=============================================================================
 * Update Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, UpdateEmpty) {
    EXPECT_EQ(shared_intentionality_update(si_), 0);
}

TEST_F(SharedIntentionalityTest, UpdateWithGoals) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    shared_goal_t goal = CreateTestGoal("Goal", 0.5f);
    shared_intentionality_propose_goal(si_, &goal);

    EXPECT_EQ(shared_intentionality_update(si_), 0);
}

/*=============================================================================
 * Callback Tests
 *===========================================================================*/

static int goal_callback_count = 0;
static void test_goal_callback(const shared_goal_t* goal,
                               shared_goal_state_t old_state,
                               void* user_data) {
    goal_callback_count++;
}

TEST_F(SharedIntentionalityTest, SetGoalCallback) {
    goal_callback_count = 0;
    EXPECT_EQ(shared_intentionality_set_goal_callback(si_, test_goal_callback, nullptr), 0);

    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    shared_goal_t goal = CreateTestGoal("Callback Test", 0.5f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    // Complete the goal to trigger callback
    shared_intentionality_complete_goal(si_, goal_id);

    EXPECT_GT(goal_callback_count, 0);
}

static int attention_callback_count = 0;
static void test_attention_callback(const joint_attention_t* attention,
                                    bool is_new,
                                    void* user_data) {
    attention_callback_count++;
}

TEST_F(SharedIntentionalityTest, SetAttentionCallback) {
    attention_callback_count = 0;
    EXPECT_EQ(shared_intentionality_set_attention_callback(si_, test_attention_callback, nullptr), 0);

    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    joint_attention_t attention = CreateTestAttention(0.7f);
    shared_intentionality_propose_attention(si_, &attention);

    EXPECT_GT(attention_callback_count, 0);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, GetStats) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal = CreateTestGoal("Stats Test", 0.5f);
    shared_intentionality_propose_goal(si_, &goal);

    shared_intentionality_stats_t stats;
    ASSERT_EQ(shared_intentionality_get_stats(si_, &stats), 0);
    EXPECT_GT(stats.goals_proposed, 0u);
}

TEST_F(SharedIntentionalityTest, ResetStats) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);

    shared_goal_t goal = CreateTestGoal("Stats Test", 0.5f);
    shared_intentionality_propose_goal(si_, &goal);

    shared_intentionality_reset_stats(si_);

    shared_intentionality_stats_t stats;
    ASSERT_EQ(shared_intentionality_get_stats(si_, &stats), 0);
    EXPECT_EQ(stats.goals_proposed, 0u);
}

/*=============================================================================
 * Utility Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, RoleTypeName) {
    EXPECT_STREQ(role_type_name(ROLE_LEADER), "LEADER");
    EXPECT_STREQ(role_type_name(ROLE_FOLLOWER), "FOLLOWER");
    EXPECT_STREQ(role_type_name(ROLE_OBSERVER), "OBSERVER");
    EXPECT_STREQ(role_type_name(ROLE_EXECUTOR), "EXECUTOR");
    EXPECT_STREQ(role_type_name(ROLE_VERIFIER), "VERIFIER");
    EXPECT_STREQ(role_type_name(ROLE_COMMUNICATOR), "COMMUNICATOR");
}

TEST_F(SharedIntentionalityTest, GoalStateName) {
    EXPECT_STREQ(goal_state_name(GOAL_STATE_PROPOSED), "PROPOSED");
    EXPECT_STREQ(goal_state_name(GOAL_STATE_NEGOTIATING), "NEGOTIATING");
    EXPECT_STREQ(goal_state_name(GOAL_STATE_ACCEPTED), "ACCEPTED");
    EXPECT_STREQ(goal_state_name(GOAL_STATE_ACTIVE), "ACTIVE");
    EXPECT_STREQ(goal_state_name(GOAL_STATE_COMPLETED), "COMPLETED");
    EXPECT_STREQ(goal_state_name(GOAL_STATE_ABANDONED), "ABANDONED");
    EXPECT_STREQ(goal_state_name(GOAL_STATE_FAILED), "FAILED");
}

/*=============================================================================
 * Debug Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, DumpDoesNotCrash) {
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);

    shared_goal_t goal = CreateTestGoal("Dump Test", 0.5f);
    shared_intentionality_propose_goal(si_, &goal);

    joint_attention_t attention = CreateTestAttention(0.7f);
    shared_intentionality_propose_attention(si_, &attention);

    shared_intentionality_dump(si_);  // Should not crash
    shared_intentionality_dump(nullptr);  // Should not crash
}

/*=============================================================================
 * Integration Tests
 *===========================================================================*/

TEST_F(SharedIntentionalityTest, FullGoalLifecycle) {
    // Register multiple instances
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 3), 0);

    // Propose a goal
    shared_goal_t goal = CreateTestGoal("Build Base", 0.9f);
    uint32_t goal_id = shared_intentionality_propose_goal(si_, &goal);
    ASSERT_NE(goal_id, 0u);

    // All instances commit
    ASSERT_EQ(shared_intentionality_commit_to_goal(si_, goal_id, 1, 0.9f), 0);
    ASSERT_EQ(shared_intentionality_commit_to_goal(si_, goal_id, 2, 0.8f), 0);
    ASSERT_EQ(shared_intentionality_commit_to_goal(si_, goal_id, 3, 0.7f), 0);

    // Assign roles
    ASSERT_EQ(shared_intentionality_assign_role(si_, goal_id, 1, ROLE_LEADER), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si_, goal_id, 2, ROLE_EXECUTOR), 0);
    ASSERT_EQ(shared_intentionality_assign_role(si_, goal_id, 3, ROLE_VERIFIER), 0);

    // Update progress
    for (float progress = 0.0f; progress <= 1.0f; progress += 0.2f) {
        ASSERT_EQ(shared_intentionality_update_goal_progress(si_, goal_id, progress), 0);
        ASSERT_EQ(shared_intentionality_update(si_), 0);
    }

    // Complete the goal
    ASSERT_EQ(shared_intentionality_complete_goal(si_, goal_id), 0);

    // Verify completion
    shared_goal_t final_goal;
    ASSERT_EQ(shared_intentionality_get_goal(si_, goal_id, &final_goal), 0);
    EXPECT_EQ(final_goal.state, GOAL_STATE_COMPLETED);

    // Check stats
    shared_intentionality_stats_t stats;
    ASSERT_EQ(shared_intentionality_get_stats(si_, &stats), 0);
    EXPECT_EQ(stats.goals_completed, 1u);
}

TEST_F(SharedIntentionalityTest, JointAttentionDynamics) {
    // Register instances
    ASSERT_EQ(shared_intentionality_register_instance(si_, 1), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 2), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 3), 0);
    ASSERT_EQ(shared_intentionality_register_instance(si_, 4), 0);

    // Propose attention target
    joint_attention_t attention = CreateTestAttention(0.9f);
    uint32_t att_id = shared_intentionality_propose_attention(si_, &attention);
    ASSERT_NE(att_id, 0u);

    // Other instances join
    ASSERT_EQ(shared_intentionality_join_attention(si_, att_id, 2), 0);
    ASSERT_EQ(shared_intentionality_join_attention(si_, att_id, 3), 0);
    ASSERT_EQ(shared_intentionality_join_attention(si_, att_id, 4), 0);

    // Verify joint attention
    joint_attention_t retrieved;
    ASSERT_EQ(shared_intentionality_get_attention(si_, att_id, &retrieved), 0);
    EXPECT_EQ(retrieved.attending_count, 4u);

    // Some instances leave
    ASSERT_EQ(shared_intentionality_leave_attention(si_, att_id, 3), 0);
    ASSERT_EQ(shared_intentionality_leave_attention(si_, att_id, 4), 0);

    ASSERT_EQ(shared_intentionality_get_attention(si_, att_id, &retrieved), 0);
    EXPECT_EQ(retrieved.attending_count, 2u);
}

