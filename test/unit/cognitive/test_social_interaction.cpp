/**
 * @file test_social_interaction.cpp
 * @brief Unit tests for multi-agent social interaction framework.
 *
 * WHAT: Tests social interaction config defaults, role/scenario enums,
 *       constants, struct layout, NULL safety, and lifecycle constraints.
 * WHY:  Social interaction enables Theory-of-Mind training; regressions
 *       silently break cooperative/adversarial learning.
 * HOW:  Google Test, stub mode (no real brain — verifies API contracts).
 *
 * NOTE: nimcp_social_interaction_create() requires a non-NULL brain pointer.
 *       Tests verify graceful NULL rejection and test standalone functions.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "cognitive/social/nimcp_social_interaction.h"
}

// ============================================================================
// Config Defaults
// ============================================================================

TEST(SocialInteraction, ConfigDefaults) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    EXPECT_EQ(cfg.num_agents, 3u);
    EXPECT_EQ(cfg.max_rounds, 10u);
    EXPECT_GT(cfg.belief_dim, 0u);
    EXPECT_GE(cfg.perspective_overlap, 0.0f);
    EXPECT_LE(cfg.perspective_overlap, 1.0f);
    EXPECT_GT(cfg.trust_learning_rate, 0.0f);
    EXPECT_GE(cfg.communication_noise, 0.0f);
}

TEST(SocialInteraction, ConfigDefaultScenario) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    EXPECT_EQ(cfg.scenario, NIMCP_SCENARIO_COOPERATIVE);
}

TEST(SocialInteraction, ConfigDefaultBeliefDim) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    EXPECT_EQ(cfg.belief_dim, 128u);
}

TEST(SocialInteraction, ConfigDefaultPerspectiveOverlap) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    EXPECT_NEAR(cfg.perspective_overlap, 0.3f, 0.05f);
}

TEST(SocialInteraction, ConfigDefaultTrustLR) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    EXPECT_NEAR(cfg.trust_learning_rate, 0.1f, 0.05f);
}

TEST(SocialInteraction, ConfigDefaultCommNoise) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    EXPECT_NEAR(cfg.communication_noise, 0.05f, 0.02f);
}

TEST(SocialInteraction, ConfigDefaultDeception) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    EXPECT_FALSE(cfg.enable_deception);
}

// ============================================================================
// Lifecycle — Brain Required
// ============================================================================

TEST(SocialInteraction, CreateRequiresBrain) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    nimcp_social_interaction_t* si = nimcp_social_interaction_create(NULL, &cfg);
    EXPECT_EQ(si, nullptr) << "NULL brain should return NULL";
}

TEST(SocialInteraction, CreateWith0AgentsNull) {
    nimcp_social_config_t cfg = nimcp_social_config_default();
    cfg.num_agents = 0;
    nimcp_social_interaction_t* si = nimcp_social_interaction_create(NULL, &cfg);
    EXPECT_EQ(si, nullptr) << "0 agents should return NULL";
}

TEST(SocialInteraction, DestroyNull) {
    nimcp_social_interaction_destroy(NULL);
    SUCCEED() << "Destroy NULL did not crash";
}

// ============================================================================
// NULL Safety on Accessor Functions
// ============================================================================

TEST(SocialInteraction, GetBeliefNull) {
    float belief[64];
    int rc = nimcp_social_get_belief(NULL, 0, belief, 64);
    EXPECT_LT(rc, 0);
}

TEST(SocialInteraction, GetBeliefNullOutput) {
    int rc = nimcp_social_get_belief(NULL, 0, NULL, 64);
    EXPECT_LT(rc, 0);
}

TEST(SocialInteraction, GetTrustNull) {
    float trust = nimcp_social_get_trust(NULL, 0, 1);
    EXPECT_LE(trust, 0.0f) << "NULL context should return <= 0";
}

TEST(SocialInteraction, GetToMNull) {
    float model[64];
    int rc = nimcp_social_get_tom_model(NULL, 0, 1, model, 64);
    EXPECT_LT(rc, 0);
}

TEST(SocialInteraction, GetToMNullOutput) {
    int rc = nimcp_social_get_tom_model(NULL, 0, 1, NULL, 64);
    EXPECT_LT(rc, 0);
}

TEST(SocialInteraction, RunEpisodeNull) {
    nimcp_social_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = nimcp_social_run_episode(NULL, NULL, 0, &result);
    EXPECT_LT(rc, 0);
}

TEST(SocialInteraction, RunEpisodeNullResult) {
    int rc = nimcp_social_run_episode(NULL, NULL, 0, NULL);
    EXPECT_LT(rc, 0);
}

TEST(SocialInteraction, TrainNull) {
    nimcp_social_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = nimcp_social_train(NULL, 10, &result);
    EXPECT_LT(rc, 0);
}

TEST(SocialInteraction, TrainNullResult) {
    int rc = nimcp_social_train(NULL, 10, NULL);
    EXPECT_LT(rc, 0);
}

// ============================================================================
// Enum Values
// ============================================================================

TEST(SocialInteraction, RoleEnumValues) {
    EXPECT_EQ(NIMCP_SOCIAL_ROLE_OBSERVER, 0);
    EXPECT_EQ(NIMCP_SOCIAL_ROLE_TEACHER, 1);
    EXPECT_EQ(NIMCP_SOCIAL_ROLE_LEARNER, 2);
    EXPECT_EQ(NIMCP_SOCIAL_ROLE_COOPERATOR, 3);
    EXPECT_EQ(NIMCP_SOCIAL_ROLE_ADVERSARY, 4);
}

TEST(SocialInteraction, ScenarioEnumValues) {
    EXPECT_EQ(NIMCP_SCENARIO_COOPERATIVE, 0);
    EXPECT_EQ(NIMCP_SCENARIO_TEACHING, 1);
    EXPECT_EQ(NIMCP_SCENARIO_DEBATE, 2);
    EXPECT_EQ(NIMCP_SCENARIO_DECEPTION, 3);
    EXPECT_EQ(NIMCP_SCENARIO_JOINT_ATTENTION, 4);
}

// ============================================================================
// Constants
// ============================================================================

TEST(SocialInteraction, MaxAgentsConstant) {
    EXPECT_EQ(NIMCP_SOCIAL_MAX_AGENTS, 8);
}

TEST(SocialInteraction, MaxMsgLenConstant) {
    EXPECT_EQ(NIMCP_SOCIAL_MAX_MSG_LEN, 256);
}

// ============================================================================
// Struct Layout
// ============================================================================

TEST(SocialInteraction, AgentStructSize) {
    EXPECT_GT(sizeof(nimcp_social_agent_t), 0u);
}

TEST(SocialInteraction, AgentStructDefaults) {
    nimcp_social_agent_t agent;
    memset(&agent, 0, sizeof(agent));
    EXPECT_EQ(agent.agent_id, 0u);
    EXPECT_EQ(agent.role, NIMCP_SOCIAL_ROLE_OBSERVER);
    EXPECT_EQ(agent.mask_dim, 0u);
    EXPECT_EQ(agent.belief_dim, 0u);
    EXPECT_EQ(agent.inbox_count, 0u);
    EXPECT_EQ(agent.interaction_count, 0u);
    EXPECT_NEAR(agent.cumulative_reward, 0.0f, 0.001f);
}

TEST(SocialInteraction, MessageStructSize) {
    EXPECT_GT(sizeof(nimcp_social_message_t), 0u);
}

TEST(SocialInteraction, ResultStructDefaults) {
    nimcp_social_result_t result;
    memset(&result, 0, sizeof(result));
    EXPECT_EQ(result.rounds_completed, 0u);
    EXPECT_NEAR(result.collective_reward, 0.0f, 0.001f);
    EXPECT_NEAR(result.belief_convergence, 0.0f, 0.001f);
    EXPECT_NEAR(result.tom_accuracy, 0.0f, 0.001f);
    EXPECT_EQ(result.messages_exchanged, 0u);
    EXPECT_EQ(result.deceptions_detected, 0u);
}

TEST(SocialInteraction, ConfigStructCustomizable) {
    nimcp_social_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_agents = 4;
    cfg.scenario = NIMCP_SCENARIO_DEBATE;
    cfg.max_rounds = 20;
    cfg.belief_dim = 64;
    cfg.perspective_overlap = 0.5f;
    cfg.trust_learning_rate = 0.2f;
    cfg.communication_noise = 0.1f;
    cfg.enable_deception = true;

    EXPECT_EQ(cfg.num_agents, 4u);
    EXPECT_EQ(cfg.scenario, NIMCP_SCENARIO_DEBATE);
    EXPECT_EQ(cfg.max_rounds, 20u);
    EXPECT_TRUE(cfg.enable_deception);
}
