/**
 * @file test_ethics.cpp
 * @brief Comprehensive unit tests for NIMCP Ethics Engine
 */

#include "test_helpers.h"

#include "cognitive/ethics/nimcp_ethics.h"

#include <cstring>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class EthicsTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        ethics_config_t config = {.policies = nullptr,
                                  .num_policies = 0,
                                  .callback = nullptr,
                                  .callback_context = nullptr,
                                  .default_severity = 0.5f,
                                  .enable_learning = true,
                                  .action_feature_size = 10,
                                  .max_agents = 100,
                                  .golden_rule_threshold = 0.0f,
                                  .empathy_weight = 0.7f};

        engine = ethics_engine_create(&config);
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            ethics_engine_destroy(engine);
            engine = nullptr;
        }
    }

    // Helper to create test action context
    action_context_t create_test_action(float harm_level = 0.0f)
    {
        action_context_t action;
        memset(&action, 0, sizeof(action));

        // Allocate features
        static float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        action.features = features;
        action.num_features = 10;

        // Allocate affected agents
        static agent_id_t agents[3] = {1, 2, 3};
        action.affected_agents = agents;
        action.num_affected_agents = 3;

        action.predicted_harm = harm_level;
        action.fairness_violation = 0.0f;
        action.deception_level = 0.0f;
        action.autonomy_violation = 0.0f;
        action.privacy_violation = 0.0f;
        action.consent_violation = 0.0f;

        return action;
    }

    // Helper to create test policy
    ethics_policy_t create_test_policy(uint32_t id, ethics_violation_t type)
    {
        ethics_policy_t policy;
        memset(&policy, 0, sizeof(policy));

        policy.policy_id = id;
        snprintf(policy.name, sizeof(policy.name), "Test Policy %u", id);
        snprintf(policy.description, sizeof(policy.description),
                 "Test policy for violation type %d", type);
        policy.violation_type = type;
        policy.severity_threshold = 0.5f;
        policy.confidence_required = 0.8f;
        policy.action = ETHICS_ACTION_BLOCK;
        policy.enabled = true;
        policy.learned = false;

        return policy;
    }

    // Helper to create test outcome
    action_outcome_t create_test_outcome(float harm, float benefit)
    {
        action_outcome_t outcome;
        memset(&outcome, 0, sizeof(outcome));

        outcome.affected_agent = 1;
        outcome.actual_harm = harm;
        outcome.actual_benefit = benefit;
        outcome.emotional_impact = (benefit - harm);
        outcome.material_impact = 0.0f;
        outcome.autonomy_impact = 0.0f;
        outcome.impact_magnitude = (harm > benefit) ? harm : benefit;
        outcome.uncertainty = 0.1f;

        return outcome;
    }

    ethics_engine_t engine;
};

//=============================================================================
// Creation/Destruction Tests
//=============================================================================

TEST_F(EthicsTest, EngineCreation)
{
    EXPECT_NE(engine, nullptr);
}

TEST_F(EthicsTest, EngineCreationNullConfig)
{
    ethics_engine_t null_engine = ethics_engine_create(nullptr);
    EXPECT_EQ(null_engine, nullptr);
}

TEST_F(EthicsTest, EngineCreationMinimalConfig)
{
    ethics_config_t config = {0};
    config.action_feature_size = 10;
    config.max_agents = 10;
    config.golden_rule_threshold = 0.0f;
    config.empathy_weight = 0.7f;
    config.enable_learning = false;

    ethics_engine_t min_engine = ethics_engine_create(&config);
    EXPECT_NE(min_engine, nullptr);

    if (min_engine) {
        ethics_engine_destroy(min_engine);
    }
}

TEST_F(EthicsTest, EngineDestructionNullSafe)
{
    ethics_engine_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Action Evaluation Tests
//=============================================================================

TEST_F(EthicsTest, EvaluateNeutralAction)
{
    action_context_t action = create_test_action(0.0f);

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
    EXPECT_GE(result.golden_rule_score, -1.0f);
    EXPECT_LE(result.golden_rule_score, 1.0f);
}

TEST_F(EthicsTest, EvaluateHarmfulAction)
{
    action_context_t action = create_test_action(0.9f);  // High harm

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Harmful action should likely be blocked
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(EthicsTest, EvaluateBeneficialAction)
{
    action_context_t action = create_test_action(0.0f);  // No harm

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(EthicsTest, EvaluateActionNull)
{
    ethics_evaluation_t result = ethics_engine_evaluate_action(nullptr, nullptr);
    EXPECT_FALSE(result.allowed);

    action_context_t action = create_test_action();
    result = ethics_engine_evaluate_action(nullptr, &action);
    EXPECT_FALSE(result.allowed);

    result = ethics_engine_evaluate_action(engine, nullptr);
    EXPECT_FALSE(result.allowed);
}

TEST_F(EthicsTest, EvaluationIncludesExplanation)
{
    action_context_t action = create_test_action(0.0f);

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GT(strlen(result.explanation), 0);
}

//=============================================================================
// Policy Management Tests
//=============================================================================

TEST_F(EthicsTest, AddPolicy)
{
    ethics_policy_t policy = create_test_policy(100, ETHICS_VIOLATION_HARM);

    bool success = ethics_engine_add_policy(engine, &policy);
    EXPECT_TRUE(success);
}

TEST_F(EthicsTest, AddPolicyNull)
{
    ethics_policy_t policy = create_test_policy(100, ETHICS_VIOLATION_HARM);

    EXPECT_FALSE(ethics_engine_add_policy(nullptr, &policy));
    EXPECT_FALSE(ethics_engine_add_policy(engine, nullptr));
}

TEST_F(EthicsTest, AddMultiplePolicies)
{
    ethics_policy_t policy1 = create_test_policy(101, ETHICS_VIOLATION_HARM);
    ethics_policy_t policy2 = create_test_policy(102, ETHICS_VIOLATION_UNFAIRNESS);
    ethics_policy_t policy3 = create_test_policy(103, ETHICS_VIOLATION_DECEPTION);

    EXPECT_TRUE(ethics_engine_add_policy(engine, &policy1));
    EXPECT_TRUE(ethics_engine_add_policy(engine, &policy2));
    EXPECT_TRUE(ethics_engine_add_policy(engine, &policy3));
}

TEST_F(EthicsTest, RemovePolicy)
{
    ethics_policy_t policy = create_test_policy(200, ETHICS_VIOLATION_HARM);

    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    bool removed = ethics_engine_remove_policy(engine, 200);
    EXPECT_TRUE(removed);
}

TEST_F(EthicsTest, RemoveNonexistentPolicy)
{
    bool removed = ethics_engine_remove_policy(engine, 9999);
    EXPECT_FALSE(removed);
}

TEST_F(EthicsTest, RemovePolicyNull)
{
    EXPECT_FALSE(ethics_engine_remove_policy(nullptr, 100));
}

// Note: ethics_get_policies function not available in API
// TEST_F(EthicsTest, GetPolicies) {
//     ethics_policy_t policy1 = create_test_policy(301, ETHICS_VIOLATION_HARM);
//     ethics_policy_t policy2 = create_test_policy(302, ETHICS_VIOLATION_UNFAIRNESS);
//
//     ASSERT_TRUE(ethics_engine_add_policy(engine, &policy1));
//     ASSERT_TRUE(ethics_engine_add_policy(engine, &policy2));
//
//     ethics_policy_t policies[10];
//     uint32_t num_policies = ethics_get_policies(engine, policies, 10);
//
//     // Should include at least the Golden Rule policy (policy_id 0) + added policies
//     EXPECT_GE(num_policies, 2);
// }
//
// TEST_F(EthicsTest, GetPoliciesNull) {
//     ethics_policy_t policies[10];
//
//     EXPECT_EQ(ethics_get_policies(nullptr, policies, 10), 0);
//     EXPECT_EQ(ethics_get_policies(engine, nullptr, 10), 0);
// }

//=============================================================================
// Learning from Outcomes Tests
//=============================================================================

TEST_F(EthicsTest, LearnFromOutcome)
{
    action_context_t action = create_test_action(0.3f);
    action_outcome_t outcome = create_test_outcome(0.2f, 0.8f);

    bool success = ethics_learn_from_outcome(engine, &action, &outcome);
    EXPECT_TRUE(success);
}

TEST_F(EthicsTest, LearnFromHarmfulOutcome)
{
    action_context_t action = create_test_action(0.1f);
    action_outcome_t outcome = create_test_outcome(0.9f, 0.0f);  // High harm

    bool success = ethics_learn_from_outcome(engine, &action, &outcome);
    EXPECT_TRUE(success);
}

TEST_F(EthicsTest, LearnFromBeneficialOutcome)
{
    action_context_t action = create_test_action(0.0f);
    action_outcome_t outcome = create_test_outcome(0.0f, 0.9f);  // High benefit

    bool success = ethics_learn_from_outcome(engine, &action, &outcome);
    EXPECT_TRUE(success);
}

TEST_F(EthicsTest, LearnFromOutcomeNull)
{
    action_context_t action = create_test_action();
    action_outcome_t outcome = create_test_outcome(0.0f, 0.0f);

    EXPECT_FALSE(ethics_learn_from_outcome(nullptr, &action, &outcome));
    EXPECT_FALSE(ethics_learn_from_outcome(engine, nullptr, &outcome));
    EXPECT_FALSE(ethics_learn_from_outcome(engine, &action, nullptr));
}

TEST_F(EthicsTest, LearningDisabled)
{
    // Create engine with learning disabled
    ethics_config_t config = {0};
    config.action_feature_size = 10;
    config.max_agents = 10;
    config.golden_rule_threshold = 0.0f;
    config.empathy_weight = 0.7f;
    config.enable_learning = false;

    ethics_engine_t no_learn_engine = ethics_engine_create(&config);
    ASSERT_NE(no_learn_engine, nullptr);

    action_context_t action = create_test_action();
    action_outcome_t outcome = create_test_outcome(0.0f, 0.0f);

    bool success = ethics_learn_from_outcome(no_learn_engine, &action, &outcome);
    EXPECT_FALSE(success);

    ethics_engine_destroy(no_learn_engine);
}

//=============================================================================
// Empathy Network Tests
//=============================================================================

TEST_F(EthicsTest, EmpathyNetworkCreation)
{
    empathy_config_t config = {
        .mirror_network = nullptr, .observation_window_ms = 1000, .empathy_threshold = 0.5f};

    empathy_network_t network = empathy_network_create(&config);
    EXPECT_NE(network, nullptr);

    if (network) {
        empathy_network_destroy(network);
    }
}

TEST_F(EthicsTest, EmpathyNetworkCreationNullConfig)
{
    empathy_network_t network = empathy_network_create(nullptr);
    EXPECT_EQ(network, nullptr);
}

TEST_F(EthicsTest, EmpathyNetworkDestructionNullSafe)
{
    empathy_network_destroy(nullptr);
    // Should not crash
}

TEST_F(EthicsTest, SimulateAgentPerspective)
{
    empathy_config_t config = {
        .mirror_network = nullptr, .observation_window_ms = 1000, .empathy_threshold = 0.5f};

    empathy_network_t network = empathy_network_create(&config);
    ASSERT_NE(network, nullptr);

    action_context_t action = create_test_action();
    agent_id_t agent_id = 1;

    empathy_state_extended_t state = empathy_network_simulate_agent(network, agent_id, &action);

    // Check that state values are in valid ranges
    EXPECT_GE(state.emotional_valence, -1.0f);
    EXPECT_LE(state.emotional_valence, 1.0f);
    EXPECT_GE(state.material_impact, -1.0f);
    EXPECT_LE(state.material_impact, 1.0f);
    EXPECT_GE(state.autonomy_impact, -1.0f);
    EXPECT_LE(state.autonomy_impact, 1.0f);
    EXPECT_GE(state.impact_magnitude, 0.0f);
    EXPECT_LE(state.impact_magnitude, 1.0f);
    EXPECT_GE(state.uncertainty, 0.0f);
    EXPECT_LE(state.uncertainty, 1.0f);

    empathy_network_destroy(network);
}

TEST_F(EthicsTest, SimulateAgentPerspectiveNull)
{
    empathy_config_t config = {
        .mirror_network = nullptr, .observation_window_ms = 1000, .empathy_threshold = 0.5f};

    empathy_network_t network = empathy_network_create(&config);
    ASSERT_NE(network, nullptr);

    action_context_t action = create_test_action();

    empathy_state_extended_t state = empathy_network_simulate_agent(nullptr, 1, &action);
    EXPECT_FALSE(state.active);

    state = empathy_network_simulate_agent(network, 1, nullptr);
    EXPECT_FALSE(state.active);

    empathy_network_destroy(network);
}

//=============================================================================
// Violation Type Tests
//=============================================================================

TEST_F(EthicsTest, ViolationTypeNames)
{
    EXPECT_NE(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_NONE), nullptr);
    EXPECT_NE(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_HARM), nullptr);
    EXPECT_NE(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_UNFAIRNESS), nullptr);
    EXPECT_NE(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_DECEPTION), nullptr);
    EXPECT_NE(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_AUTONOMY), nullptr);
    EXPECT_NE(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_PRIVACY), nullptr);
    EXPECT_NE(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_CONSENT), nullptr);
    EXPECT_NE(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_DIGNITY), nullptr);
}

TEST_F(EthicsTest, ViolationTypeNameUnknown)
{
    const char* name = ethics_violation_type_name((ethics_violation_type_t) 9999);
    EXPECT_STREQ(name, "Unknown");
}

//=============================================================================
// Golden Rule Evaluation Tests
//=============================================================================

TEST_F(EthicsTest, GoldenRuleSymmetry)
{
    // Create two similar actions
    action_context_t action1 = create_test_action(0.5f);
    action_context_t action2 = create_test_action(0.5f);

    ethics_evaluation_t result1 = ethics_engine_evaluate_action(engine, &action1);
    ethics_evaluation_t result2 = ethics_engine_evaluate_action(engine, &action2);

    // Similar actions should have similar Golden Rule scores
    EXPECT_TRUE(float_equals(result1.golden_rule_score, result2.golden_rule_score));
}

TEST_F(EthicsTest, GoldenRuleHarmPenalty)
{
    action_context_t low_harm = create_test_action(0.1f);
    action_context_t high_harm = create_test_action(0.9f);

    ethics_evaluation_t result_low = ethics_engine_evaluate_action(engine, &low_harm);
    ethics_evaluation_t result_high = ethics_engine_evaluate_action(engine, &high_harm);

    // Higher harm should result in lower (more negative) Golden Rule score
    // (or blocked action)
    if (result_low.allowed && !result_high.allowed) {
        // High harm blocked, low harm allowed - expected
        EXPECT_TRUE(true);
    } else {
        // If both evaluated, high harm should have lower score
        EXPECT_LE(result_high.golden_rule_score, result_low.golden_rule_score);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EthicsTest, GetStatistics)
{
    ethics_statistics_t stats;
    bool success = ethics_get_statistics(engine, &stats);

    EXPECT_TRUE(success);
    EXPECT_GE(stats.total_evaluations, 0);
    EXPECT_GE(stats.violations_detected, 0);
    EXPECT_GE(stats.actions_blocked, 0);
    EXPECT_GE(stats.num_policies, 0);
    EXPECT_GE(stats.num_violations_logged, 0);
}

TEST_F(EthicsTest, GetStatisticsNull)
{
    ethics_statistics_t stats;

    EXPECT_FALSE(ethics_get_statistics(nullptr, &stats));
    EXPECT_FALSE(ethics_get_statistics(engine, nullptr));
}

TEST_F(EthicsTest, StatisticsTrackEvaluations)
{
    ethics_statistics_t stats1;
    ASSERT_TRUE(ethics_get_statistics(engine, &stats1));
    uint64_t evals_before = stats1.total_evaluations;

    action_context_t action = create_test_action();
    ethics_engine_evaluate_action(engine, &action);

    ethics_statistics_t stats2;
    ASSERT_TRUE(ethics_get_statistics(engine, &stats2));
    uint64_t evals_after = stats2.total_evaluations;

    EXPECT_GT(evals_after, evals_before);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(EthicsTest, PrintEvaluationNull)
{
    ethics_print_evaluation(nullptr);
    // Should not crash
}

TEST_F(EthicsTest, PrintEvaluation)
{
    action_context_t action = create_test_action();
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    ethics_print_evaluation(&result);
    // Should not crash
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

TEST_F(EthicsTest, ActionWithNoAffectedAgents)
{
    action_context_t action = create_test_action();
    action.num_affected_agents = 0;
    action.affected_agents = nullptr;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Should still evaluate without crashing
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(EthicsTest, ActionWithMaxViolations)
{
    action_context_t action = create_test_action();
    action.predicted_harm = 1.0f;
    action.fairness_violation = 1.0f;
    action.deception_level = 1.0f;
    action.autonomy_violation = 1.0f;
    action.privacy_violation = 1.0f;
    action.consent_violation = 1.0f;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Should handle maximum violations
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(EthicsTest, ActionWithZeroFeatures)
{
    action_context_t action = create_test_action();
    action.num_features = 0;
    action.features = nullptr;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Should still evaluate
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsTest, LargeNumberOfAffectedAgents)
{
    action_context_t action = create_test_action();

    // Create large array of agents
    const int NUM_AGENTS = 50;
    agent_id_t agents[NUM_AGENTS];
    for (int i = 0; i < NUM_AGENTS; i++) {
        agents[i] = i;
    }
    action.affected_agents = agents;
    action.num_affected_agents = NUM_AGENTS;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Should handle many affected agents
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EthicsTest, IntegrationFullEthicalEvaluation)
{
    // 1. Add custom policy
    ethics_policy_t policy = create_test_policy(500, ETHICS_VIOLATION_HARM);
    policy.severity_threshold = 0.3f;
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    // 2. Evaluate action
    action_context_t action = create_test_action(0.4f);
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // 3. Learn from outcome
    action_outcome_t outcome = create_test_outcome(0.4f, 0.1f);
    bool learned = ethics_learn_from_outcome(engine, &action, &outcome);
    EXPECT_TRUE(learned);

    // 4. Check statistics
    ethics_statistics_t stats;
    ASSERT_TRUE(ethics_get_statistics(engine, &stats));
    EXPECT_GT(stats.total_evaluations, 0);
}

TEST_F(EthicsTest, IntegrationGoldenRuleLearning)
{
    // Evaluate action before learning
    action_context_t action1 = create_test_action(0.5f);
    ethics_evaluation_t result1 = ethics_engine_evaluate_action(engine, &action1);

    // Learn from multiple outcomes
    for (int i = 0; i < 5; i++) {
        action_context_t action = create_test_action(0.5f);
        action_outcome_t outcome = create_test_outcome(0.6f, 0.0f);
        ethics_learn_from_outcome(engine, &action, &outcome);
    }

    // Evaluate similar action after learning
    action_context_t action2 = create_test_action(0.5f);
    ethics_evaluation_t result2 = ethics_engine_evaluate_action(engine, &action2);

    // Both evaluations should complete successfully
    EXPECT_GE(result1.confidence, 0.0f);
    EXPECT_GE(result2.confidence, 0.0f);
}

TEST_F(EthicsTest, IntegrationPolicyEffectiveness)
{
    // Create strict harm policy
    ethics_policy_t strict_policy = create_test_policy(600, ETHICS_VIOLATION_HARM);
    strict_policy.severity_threshold = 0.1f;  // Very low threshold
    strict_policy.action = ETHICS_ACTION_BLOCK;
    ASSERT_TRUE(ethics_engine_add_policy(engine, &strict_policy));

    // Test with various harm levels
    action_context_t low_harm = create_test_action(0.05f);
    action_context_t med_harm = create_test_action(0.5f);
    action_context_t high_harm = create_test_action(0.95f);

    ethics_evaluation_t result_low = ethics_engine_evaluate_action(engine, &low_harm);
    ethics_evaluation_t result_med = ethics_engine_evaluate_action(engine, &med_harm);
    ethics_evaluation_t result_high = ethics_engine_evaluate_action(engine, &high_harm);

    // All should evaluate
    EXPECT_GE(result_low.confidence, 0.0f);
    EXPECT_GE(result_med.confidence, 0.0f);
    EXPECT_GE(result_high.confidence, 0.0f);
}

}  // anonymous namespace
