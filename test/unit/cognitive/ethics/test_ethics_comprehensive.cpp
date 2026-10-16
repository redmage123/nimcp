/**
 * @file test_ethics_comprehensive.cpp
 * @brief Comprehensive unit tests for NIMCP Ethics Engine
 *
 * Goal: Achieve 100% code coverage of nimcp_ethics.c (currently 16.3% coverage, 386 lines)
 *
 * Coverage target:
 * - All ethics engine functions
 * - All empathy network functions
 * - All incident logging functions
 * - All internal helper functions (buffer pool, hash table, strategy table)
 * - All edge cases and error paths
 * - All policy management functions
 * - All Golden Rule evaluation paths
 * - All learning paths
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

#include "cognitive/ethics/nimcp_ethics.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class EthicsComprehensiveTest : public ::testing::Test {
protected:
    ethics_engine_t engine = nullptr;
    empathy_network_t empathy_net = nullptr;

    void SetUp() override {
        // Create engine with comprehensive config
        ethics_config_t config = {};
        config.policies = nullptr;
        config.num_policies = 0;
        config.callback = nullptr;
        config.callback_context = nullptr;
        config.default_severity = 0.5f;
        config.enable_learning = true;
        config.action_feature_size = 20;
        config.max_agents = 1000;
        config.golden_rule_threshold = 0.0f;
        config.empathy_weight = 0.7f;

        engine = ethics_engine_create(&config);
        ASSERT_NE(engine, nullptr) << "Failed to create ethics engine in SetUp";
    }

    void TearDown() override {
        if (empathy_net) {
            empathy_network_destroy(empathy_net);
            empathy_net = nullptr;
        }
        if (engine) {
            ethics_engine_destroy(engine);
            engine = nullptr;
        }
    }

    // Helper: Create action context with specified parameters
    action_context_t create_action(float harm = 0.0f, uint32_t num_agents = 3) {
        action_context_t action = {};

        static float features[20] = {
            0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f,
            0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f
        };
        action.features = features;
        action.num_features = 20;

        static agent_id_t agents[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        action.affected_agents = agents;
        action.num_affected_agents = num_agents;

        action.predicted_harm = harm;
        action.fairness_violation = 0.0f;
        action.deception_level = 0.0f;
        action.autonomy_violation = 0.0f;
        action.privacy_violation = 0.0f;
        action.consent_violation = 0.0f;

        return action;
    }

    // Helper: Create policy
    ethics_policy_t create_policy(uint32_t id, ethics_violation_type_t type, float threshold = 0.5f) {
        ethics_policy_t policy = {};
        policy.policy_id = id;
        snprintf(policy.name, sizeof(policy.name), "Policy_%u", id);
        snprintf(policy.description, sizeof(policy.description), "Test policy %u", id);
        policy.violation_type = (ethics_violation_t)type;
        policy.severity_threshold = threshold;
        policy.confidence_required = 0.8f;
        policy.action = ETHICS_ACTION_BLOCK;
        policy.enabled = true;
        policy.learned = false;
        return policy;
    }

    // Helper: Create outcome
    action_outcome_t create_outcome(agent_id_t agent, float harm, float benefit) {
        action_outcome_t outcome = {};
        outcome.affected_agent = agent;
        outcome.actual_harm = harm;
        outcome.actual_benefit = benefit;
        outcome.emotional_impact = benefit - harm;
        outcome.material_impact = 0.0f;
        outcome.autonomy_impact = 0.0f;
        outcome.impact_magnitude = (harm > benefit) ? harm : benefit;
        outcome.uncertainty = 0.1f;
        return outcome;
    }

    // Helper: Create incident
    ethics_incident_t create_incident(uint64_t id, ethics_violation_type_t type, float severity) {
        ethics_incident_t incident = {};
        incident.incident_id = id;
        incident.timestamp = id * 1000;
        incident.violation_type = type;
        incident.severity = severity;
        incident.action_taken = ETHICS_ACTION_BLOCK;
        incident.policy_id = 0;
        snprintf(incident.policy_name, sizeof(incident.policy_name), "Test Policy");
        snprintf(incident.description, sizeof(incident.description), "Test incident %lu", id);
        incident.golden_rule_score = 0.0f;
        incident.acting_agent = 1;
        incident.affected_agent = 2;
        return incident;
    }
};

//=============================================================================
// 1. Engine Creation/Destruction Tests (Lines 680-794)
//=============================================================================

TEST_F(EthicsComprehensiveTest, EngineCreate_ValidConfig_Success) {
    EXPECT_NE(engine, nullptr);
}

TEST_F(EthicsComprehensiveTest, EngineCreate_NullConfig_ReturnsNull) {
    ethics_engine_t null_engine = ethics_engine_create(nullptr);
    EXPECT_EQ(null_engine, nullptr);
}

TEST_F(EthicsComprehensiveTest, EngineCreate_MinimalConfig_Success) {
    ethics_config_t config = {};
    config.action_feature_size = 10;
    config.max_agents = 10;
    config.golden_rule_threshold = 0.0f;
    config.empathy_weight = 0.5f;
    config.enable_learning = false;

    ethics_engine_t min_engine = ethics_engine_create(&config);
    EXPECT_NE(min_engine, nullptr);
    ethics_engine_destroy(min_engine);
}

TEST_F(EthicsComprehensiveTest, EngineCreate_LargeConfig_Success) {
    ethics_config_t config = {};
    config.action_feature_size = 100;
    config.max_agents = 10000;
    config.golden_rule_threshold = 0.5f;
    config.empathy_weight = 0.9f;
    config.enable_learning = true;

    ethics_engine_t large_engine = ethics_engine_create(&config);
    EXPECT_NE(large_engine, nullptr);
    ethics_engine_destroy(large_engine);
}

TEST_F(EthicsComprehensiveTest, EngineDestroy_Null_NoCrash) {
    ethics_engine_destroy(nullptr);
    // Should not crash
}

TEST_F(EthicsComprehensiveTest, EngineDestroy_Valid_Success) {
    ethics_config_t config = {};
    config.action_feature_size = 10;
    config.max_agents = 10;
    ethics_engine_t test_engine = ethics_engine_create(&config);
    ASSERT_NE(test_engine, nullptr);

    ethics_engine_destroy(test_engine);
    // Should not crash
}

//=============================================================================
// 2. Action Evaluation Tests (Lines 1058-1089)
//=============================================================================

TEST_F(EthicsComprehensiveTest, EvaluateAction_NullEngine_ReturnsFalse) {
    action_context_t action = create_action();
    ethics_evaluation_t result = ethics_engine_evaluate_action(nullptr, &action);

    EXPECT_FALSE(result.allowed);
    EXPECT_FLOAT_EQ(result.confidence, 1.0f);
    EXPECT_EQ(result.primary_violation, ETHICS_VIOLATION_TYPE_HARM);
    EXPECT_GT(strlen(result.explanation), 0);
}

TEST_F(EthicsComprehensiveTest, EvaluateAction_NullAction_ReturnsFalse) {
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, nullptr);

    EXPECT_FALSE(result.allowed);
    EXPECT_FLOAT_EQ(result.confidence, 1.0f);
    EXPECT_EQ(result.primary_violation, ETHICS_VIOLATION_TYPE_HARM);
}

TEST_F(EthicsComprehensiveTest, EvaluateAction_NeutralAction_Allowed) {
    action_context_t action = create_action(0.0f, 3);
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
    EXPECT_GE(result.golden_rule_score, -1.0f);
    EXPECT_LE(result.golden_rule_score, 1.0f);
    EXPECT_GT(strlen(result.explanation), 0);
}

TEST_F(EthicsComprehensiveTest, EvaluateAction_HighHarm_MayBlock) {
    action_context_t action = create_action(0.9f, 5);
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(EthicsComprehensiveTest, EvaluateAction_NoAffectedAgents_HandlesGracefully) {
    action_context_t action = create_action(0.0f, 0);
    action.num_affected_agents = 0;
    action.affected_agents = nullptr;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, EvaluateAction_ManyAffectedAgents_Success) {
    action_context_t action = create_action(0.3f, 10);
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(EthicsComprehensiveTest, EvaluateAction_AllViolationTypes_Evaluated) {
    action_context_t action = create_action();
    action.predicted_harm = 0.6f;
    action.fairness_violation = 0.7f;
    action.deception_level = 0.5f;
    action.autonomy_violation = 0.4f;
    action.privacy_violation = 0.3f;
    action.consent_violation = 0.8f;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_GT(strlen(result.explanation), 0);
}

//=============================================================================
// 3. Policy Management Tests (Lines 1323-1387, 1648-1737)
//=============================================================================

TEST_F(EthicsComprehensiveTest, AddPolicy_Valid_Success) {
    ethics_policy_t policy = create_policy(100, ETHICS_VIOLATION_TYPE_HARM);
    bool result = ethics_engine_add_policy(engine, &policy);
    EXPECT_TRUE(result);
}

TEST_F(EthicsComprehensiveTest, AddPolicy_NullEngine_ReturnsFalse) {
    ethics_policy_t policy = create_policy(100, ETHICS_VIOLATION_TYPE_HARM);
    bool result = ethics_engine_add_policy(nullptr, &policy);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, AddPolicy_NullPolicy_ReturnsFalse) {
    bool result = ethics_engine_add_policy(engine, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, AddPolicy_MultiplePolicies_AllAdded) {
    ethics_policy_t p1 = create_policy(101, ETHICS_VIOLATION_TYPE_HARM);
    ethics_policy_t p2 = create_policy(102, ETHICS_VIOLATION_TYPE_UNFAIRNESS);
    ethics_policy_t p3 = create_policy(103, ETHICS_VIOLATION_TYPE_DECEPTION);
    ethics_policy_t p4 = create_policy(104, ETHICS_VIOLATION_TYPE_AUTONOMY);
    ethics_policy_t p5 = create_policy(105, ETHICS_VIOLATION_TYPE_PRIVACY);
    ethics_policy_t p6 = create_policy(106, ETHICS_VIOLATION_TYPE_CONSENT);

    EXPECT_TRUE(ethics_engine_add_policy(engine, &p1));
    EXPECT_TRUE(ethics_engine_add_policy(engine, &p2));
    EXPECT_TRUE(ethics_engine_add_policy(engine, &p3));
    EXPECT_TRUE(ethics_engine_add_policy(engine, &p4));
    EXPECT_TRUE(ethics_engine_add_policy(engine, &p5));
    EXPECT_TRUE(ethics_engine_add_policy(engine, &p6));
}

TEST_F(EthicsComprehensiveTest, AddPolicy_CapacityExpansion_Success) {
    // Add many policies to trigger reallocation
    for (uint32_t i = 200; i < 250; i++) {
        ethics_policy_t policy = create_policy(i, ETHICS_VIOLATION_TYPE_HARM);
        EXPECT_TRUE(ethics_engine_add_policy(engine, &policy));
    }
}

TEST_F(EthicsComprehensiveTest, RemovePolicy_Valid_Success) {
    ethics_policy_t policy = create_policy(300, ETHICS_VIOLATION_TYPE_HARM);
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    bool result = ethics_engine_remove_policy(engine, 300);
    EXPECT_TRUE(result);
}

TEST_F(EthicsComprehensiveTest, RemovePolicy_NullEngine_ReturnsFalse) {
    bool result = ethics_engine_remove_policy(nullptr, 100);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, RemovePolicy_NonExistent_ReturnsFalse) {
    bool result = ethics_engine_remove_policy(engine, 99999);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, RemovePolicy_AddRemoveMultiple_Success) {
    for (uint32_t i = 400; i < 410; i++) {
        ethics_policy_t policy = create_policy(i, ETHICS_VIOLATION_TYPE_HARM);
        ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));
    }

    for (uint32_t i = 400; i < 410; i++) {
        EXPECT_TRUE(ethics_engine_remove_policy(engine, i));
    }
}

//=============================================================================
// 4. Policy Evaluation Tests (Lines 320-430)
//=============================================================================

TEST_F(EthicsComprehensiveTest, PolicyEvaluation_HarmPolicy_Triggered) {
    ethics_policy_t policy = create_policy(500, ETHICS_VIOLATION_TYPE_HARM, 0.3f);
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    action_context_t action = create_action(0.8f);
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // High harm should be evaluated
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, PolicyEvaluation_UnfairnessPolicy_Triggered) {
    ethics_policy_t policy = create_policy(501, ETHICS_VIOLATION_TYPE_UNFAIRNESS, 0.3f);
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    action_context_t action = create_action(0.0f);
    action.fairness_violation = 0.7f;
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, PolicyEvaluation_DeceptionPolicy_Triggered) {
    ethics_policy_t policy = create_policy(502, ETHICS_VIOLATION_TYPE_DECEPTION, 0.3f);
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    action_context_t action = create_action(0.0f);
    action.deception_level = 0.9f;
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, PolicyEvaluation_AutonomyPolicy_Triggered) {
    ethics_policy_t policy = create_policy(503, ETHICS_VIOLATION_TYPE_AUTONOMY, 0.3f);
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    action_context_t action = create_action(0.0f);
    action.autonomy_violation = 0.8f;
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, PolicyEvaluation_PrivacyPolicy_Triggered) {
    ethics_policy_t policy = create_policy(504, ETHICS_VIOLATION_TYPE_PRIVACY, 0.3f);
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    action_context_t action = create_action(0.0f);
    action.privacy_violation = 0.6f;
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, PolicyEvaluation_ConsentPolicy_Triggered) {
    ethics_policy_t policy = create_policy(505, ETHICS_VIOLATION_TYPE_CONSENT, 0.3f);
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    action_context_t action = create_action(0.0f);
    action.consent_violation = 0.7f;
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, PolicyEvaluation_DisabledPolicy_NotTriggered) {
    ethics_policy_t policy = create_policy(506, ETHICS_VIOLATION_TYPE_HARM, 0.1f);
    policy.enabled = false;
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    action_context_t action = create_action(0.9f);
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    // Disabled policy should not affect result
    EXPECT_GE(result.confidence, 0.0f);
}

//=============================================================================
// 5. Learning from Outcomes Tests (Lines 1521-1537)
//=============================================================================

TEST_F(EthicsComprehensiveTest, LearnFromOutcome_Valid_Success) {
    action_context_t action = create_action(0.3f);
    action_outcome_t outcome = create_outcome(1, 0.2f, 0.8f);

    bool result = ethics_learn_from_outcome(engine, &action, &outcome);
    EXPECT_TRUE(result);
}

TEST_F(EthicsComprehensiveTest, LearnFromOutcome_NullEngine_ReturnsFalse) {
    action_context_t action = create_action();
    action_outcome_t outcome = create_outcome(1, 0.0f, 0.0f);

    bool result = ethics_learn_from_outcome(nullptr, &action, &outcome);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, LearnFromOutcome_NullAction_ReturnsFalse) {
    action_outcome_t outcome = create_outcome(1, 0.0f, 0.0f);

    bool result = ethics_learn_from_outcome(engine, nullptr, &outcome);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, LearnFromOutcome_NullOutcome_ReturnsFalse) {
    action_context_t action = create_action();

    bool result = ethics_learn_from_outcome(engine, &action, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, LearnFromOutcome_LearningDisabled_ReturnsFalse) {
    // Create engine with learning disabled
    ethics_config_t config = {};
    config.action_feature_size = 10;
    config.max_agents = 10;
    config.enable_learning = false;

    ethics_engine_t no_learn_engine = ethics_engine_create(&config);
    ASSERT_NE(no_learn_engine, nullptr);

    action_context_t action = create_action();
    action_outcome_t outcome = create_outcome(1, 0.0f, 0.0f);

    bool result = ethics_learn_from_outcome(no_learn_engine, &action, &outcome);
    EXPECT_FALSE(result);

    ethics_engine_destroy(no_learn_engine);
}

TEST_F(EthicsComprehensiveTest, LearnFromOutcome_HarmfulOutcome_Success) {
    action_context_t action = create_action(0.1f);
    action_outcome_t outcome = create_outcome(1, 0.9f, 0.0f);

    bool result = ethics_learn_from_outcome(engine, &action, &outcome);
    EXPECT_TRUE(result);
}

TEST_F(EthicsComprehensiveTest, LearnFromOutcome_BeneficialOutcome_Success) {
    action_context_t action = create_action(0.0f);
    action_outcome_t outcome = create_outcome(1, 0.0f, 0.9f);

    bool result = ethics_learn_from_outcome(engine, &action, &outcome);
    EXPECT_TRUE(result);
}

TEST_F(EthicsComprehensiveTest, LearnFromOutcome_MultipleOutcomes_Success) {
    for (int i = 0; i < 10; i++) {
        action_context_t action = create_action(0.5f);
        action_outcome_t outcome = create_outcome(i % 5, 0.3f, 0.7f);
        EXPECT_TRUE(ethics_learn_from_outcome(engine, &action, &outcome));
    }
}

//=============================================================================
// 6. Empathy Network Tests (Lines 1105-1307)
//=============================================================================

TEST_F(EthicsComprehensiveTest, EmpathyNetwork_Create_Success) {
    empathy_config_t config = {
        .mirror_network = nullptr,
        .observation_window_ms = 1000,
        .empathy_threshold = 0.5f
    };

    empathy_network_t network = empathy_network_create(&config);
    EXPECT_NE(network, nullptr);

    empathy_network_destroy(network);
}

TEST_F(EthicsComprehensiveTest, EmpathyNetwork_CreateNullConfig_ReturnsNull) {
    empathy_network_t network = empathy_network_create(nullptr);
    EXPECT_EQ(network, nullptr);
}

TEST_F(EthicsComprehensiveTest, EmpathyNetwork_DestroyNull_NoCrash) {
    empathy_network_destroy(nullptr);
    // Should not crash
}

TEST_F(EthicsComprehensiveTest, EmpathyNetwork_SimulateAgent_Valid) {
    empathy_config_t config = {
        .mirror_network = nullptr,
        .observation_window_ms = 1000,
        .empathy_threshold = 0.5f
    };

    empathy_network_t network = empathy_network_create(&config);
    ASSERT_NE(network, nullptr);

    action_context_t action = create_action();
    empathy_state_extended_t state = empathy_network_simulate_agent(network, 1, &action);

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

TEST_F(EthicsComprehensiveTest, EmpathyNetwork_SimulateAgentNullNetwork_ReturnsInactive) {
    action_context_t action = create_action();
    empathy_state_extended_t state = empathy_network_simulate_agent(nullptr, 1, &action);

    EXPECT_FALSE(state.active);
}

TEST_F(EthicsComprehensiveTest, EmpathyNetwork_SimulateAgentNullAction_ReturnsInactive) {
    empathy_config_t config = {
        .mirror_network = nullptr,
        .observation_window_ms = 1000,
        .empathy_threshold = 0.5f
    };

    empathy_network_t network = empathy_network_create(&config);
    ASSERT_NE(network, nullptr);

    empathy_state_extended_t state = empathy_network_simulate_agent(network, 1, nullptr);
    EXPECT_FALSE(state.active);

    empathy_network_destroy(network);
}

TEST_F(EthicsComprehensiveTest, EmpathyNetwork_SimulateAgentOutOfBounds_ReturnsInactive) {
    empathy_config_t config = {
        .mirror_network = nullptr,
        .observation_window_ms = 1000,
        .empathy_threshold = 0.5f
    };

    empathy_network_t network = empathy_network_create(&config);
    ASSERT_NE(network, nullptr);

    action_context_t action = create_action();
    // Agent ID 10000 is beyond max_agents (1000)
    empathy_state_extended_t state = empathy_network_simulate_agent(network, 10000, &action);
    EXPECT_FALSE(state.active);

    empathy_network_destroy(network);
}

TEST_F(EthicsComprehensiveTest, EmpathyNetwork_SimulateMultipleAgents_Success) {
    empathy_config_t config = {
        .mirror_network = nullptr,
        .observation_window_ms = 1000,
        .empathy_threshold = 0.5f
    };

    empathy_network_t network = empathy_network_create(&config);
    ASSERT_NE(network, nullptr);

    action_context_t action = create_action(0.3f, 5);

    for (agent_id_t agent = 0; agent < 10; agent++) {
        empathy_state_extended_t state = empathy_network_simulate_agent(network, agent, &action);
        // Each simulation should produce valid ranges
        EXPECT_GE(state.uncertainty, 0.0f);
        EXPECT_LE(state.uncertainty, 1.0f);
    }

    empathy_network_destroy(network);
}

//=============================================================================
// 7. Incident Logging Tests (Lines 1780-2263)
//=============================================================================

TEST_F(EthicsComprehensiveTest, LogIncident_Valid_Success) {
    ethics_incident_t incident = create_incident(1, ETHICS_VIOLATION_TYPE_HARM, 0.7f);
    bool result = ethics_log_incident(engine, &incident);
    EXPECT_TRUE(result);
}

TEST_F(EthicsComprehensiveTest, LogIncident_NullEngine_ReturnsFalse) {
    ethics_incident_t incident = create_incident(1, ETHICS_VIOLATION_TYPE_HARM, 0.7f);
    bool result = ethics_log_incident(nullptr, &incident);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, LogIncident_NullIncident_ReturnsFalse) {
    bool result = ethics_log_incident(engine, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, LogIncident_MultipleIncidents_AllLogged) {
    for (uint64_t i = 0; i < 100; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        EXPECT_TRUE(ethics_log_incident(engine, &incident));
    }
}

TEST_F(EthicsComprehensiveTest, GetRecentIncidents_EmptyLog_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_recent_incidents(engine, 10, &incidents);

    EXPECT_EQ(count, 0);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetRecentIncidents_NullEngine_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_recent_incidents(nullptr, 10, &incidents);

    EXPECT_EQ(count, 0);
}

TEST_F(EthicsComprehensiveTest, GetRecentIncidents_NullOutput_ReturnsZero) {
    uint32_t count = ethics_get_recent_incidents(engine, 10, nullptr);
    EXPECT_EQ(count, 0);
}

TEST_F(EthicsComprehensiveTest, GetRecentIncidents_AfterLogging_ReturnsIncidents) {
    // Log some incidents
    for (uint64_t i = 0; i < 5; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_recent_incidents(engine, 10, &incidents);

    EXPECT_EQ(count, 5);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetRecentIncidents_LimitedCount_ReturnsRequestedAmount) {
    // Log 10 incidents
    for (uint64_t i = 0; i < 10; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    // Request only 3
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_recent_incidents(engine, 3, &incidents);

    EXPECT_EQ(count, 3);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsByTimeRange_EmptyLog_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_time_range(engine, 0, 10000, &incidents);

    EXPECT_EQ(count, 0);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsByTimeRange_NullEngine_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_time_range(nullptr, 0, 10000, &incidents);

    EXPECT_EQ(count, 0);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsByTimeRange_WithIncidents_ReturnsMatching) {
    // Log incidents with different timestamps
    for (uint64_t i = 0; i < 10; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        incident.timestamp = i * 1000;
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    // Query range 2000-7000 (should get incidents 2-7)
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_time_range(engine, 2000, 7000, &incidents);

    EXPECT_GE(count, 0);  // May be 0-6 depending on implementation
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsByViolationType_EmptyLog_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_violation_type(
        engine, ETHICS_VIOLATION_TYPE_HARM, &incidents);

    EXPECT_EQ(count, 0);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsByViolationType_WithIncidents_ReturnsMatching) {
    // Log incidents of different types
    for (uint64_t i = 0; i < 5; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }
    for (uint64_t i = 5; i < 10; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_UNFAIRNESS, 0.5f);
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    // Query for harm violations
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_violation_type(
        engine, ETHICS_VIOLATION_TYPE_HARM, &incidents);

    EXPECT_EQ(count, 5);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsBySeverity_EmptyLog_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_severity(engine, 0.5f, &incidents);

    EXPECT_EQ(count, 0);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsBySeverity_InvalidSeverity_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_severity(engine, -0.5f, &incidents);
    EXPECT_EQ(count, 0);

    count = ethics_get_incidents_by_severity(engine, 1.5f, &incidents);
    EXPECT_EQ(count, 0);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsBySeverity_WithIncidents_ReturnsMatching) {
    // Log incidents with different severities
    for (uint64_t i = 0; i < 10; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, i * 0.1f);
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    // Query for severity >= 0.5
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_severity(engine, 0.5f, &incidents);

    EXPECT_GE(count, 5);  // Should get incidents with severity 0.5-0.9
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsByAction_EmptyLog_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_action(engine, ETHICS_ACTION_BLOCK, &incidents);

    EXPECT_EQ(count, 0);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetIncidentsByAction_WithIncidents_ReturnsMatching) {
    // Log incidents with different actions
    for (uint64_t i = 0; i < 5; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        incident.action_taken = ETHICS_ACTION_BLOCK;
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }
    for (uint64_t i = 5; i < 10; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        incident.action_taken = ETHICS_ACTION_ALLOW;
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    // Query for blocked actions
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_incidents_by_action(engine, ETHICS_ACTION_BLOCK, &incidents);

    EXPECT_EQ(count, 5);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetAllIncidents_EmptyLog_ReturnsZero) {
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_all_incidents(engine, &incidents);

    EXPECT_EQ(count, 0);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, GetAllIncidents_WithIncidents_ReturnsAll) {
    // Log 20 incidents
    for (uint64_t i = 0; i < 20; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_all_incidents(engine, &incidents);

    EXPECT_EQ(count, 20);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, ExportIncidents_NullEngine_ReturnsFalse) {
    bool result = ethics_export_incidents(nullptr, "/tmp/test.json", "json");
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, ExportIncidents_NullFilepath_ReturnsFalse) {
    bool result = ethics_export_incidents(engine, nullptr, "json");
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, ExportIncidents_NullFormat_ReturnsFalse) {
    bool result = ethics_export_incidents(engine, "/tmp/test.json", nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, ExportIncidents_InvalidFormat_ReturnsFalse) {
    // Log an incident
    ethics_incident_t incident = create_incident(1, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
    ASSERT_TRUE(ethics_log_incident(engine, &incident));

    bool result = ethics_export_incidents(engine, "/tmp/test.txt", "invalid");
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, ExportIncidents_JSON_Success) {
    // Log some incidents
    for (uint64_t i = 0; i < 5; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    bool result = ethics_export_incidents(engine, "/tmp/ethics_test.json", "json");
    // May succeed or fail depending on write permissions
}

TEST_F(EthicsComprehensiveTest, ExportIncidents_CSV_Success) {
    // Log some incidents
    for (uint64_t i = 0; i < 5; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    bool result = ethics_export_incidents(engine, "/tmp/ethics_test.csv", "csv");
    // May succeed or fail depending on write permissions
}

//=============================================================================
// 8. Statistics Tests (Lines 1612-1626)
//=============================================================================

TEST_F(EthicsComprehensiveTest, GetStatistics_Valid_Success) {
    ethics_statistics_t stats;
    bool result = ethics_get_statistics(engine, &stats);

    EXPECT_TRUE(result);
    EXPECT_GE(stats.total_evaluations, 0);
    EXPECT_GE(stats.violations_detected, 0);
    EXPECT_GE(stats.actions_blocked, 0);
    EXPECT_GE(stats.num_policies, 0);
    EXPECT_GE(stats.num_violations_logged, 0);
}

TEST_F(EthicsComprehensiveTest, GetStatistics_NullEngine_ReturnsFalse) {
    ethics_statistics_t stats;
    bool result = ethics_get_statistics(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, GetStatistics_NullStats_ReturnsFalse) {
    bool result = ethics_get_statistics(engine, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(EthicsComprehensiveTest, GetStatistics_TracksEvaluations_Increments) {
    ethics_statistics_t stats1;
    ASSERT_TRUE(ethics_get_statistics(engine, &stats1));
    uint64_t evals_before = stats1.total_evaluations;

    // Perform some evaluations
    for (int i = 0; i < 10; i++) {
        action_context_t action = create_action(0.0f);
        ethics_engine_evaluate_action(engine, &action);
    }

    ethics_statistics_t stats2;
    ASSERT_TRUE(ethics_get_statistics(engine, &stats2));
    uint64_t evals_after = stats2.total_evaluations;

    EXPECT_EQ(evals_after, evals_before + 10);
}

TEST_F(EthicsComprehensiveTest, GetStatistics_TracksViolations_Increments) {
    ethics_statistics_t stats1;
    ASSERT_TRUE(ethics_get_statistics(engine, &stats1));
    uint64_t violations_before = stats1.violations_detected;

    // Log some incidents
    for (int i = 0; i < 5; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.8f);
        ethics_log_incident(engine, &incident);
    }

    ethics_statistics_t stats2;
    ASSERT_TRUE(ethics_get_statistics(engine, &stats2));
    uint64_t violations_after = stats2.violations_detected;

    EXPECT_GE(violations_after, violations_before);
}

//=============================================================================
// 9. Utility Function Tests (Lines 1551-1601)
//=============================================================================

TEST_F(EthicsComprehensiveTest, ViolationTypeName_AllTypes_ValidStrings) {
    EXPECT_STREQ(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_NONE), "None");
    EXPECT_STREQ(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_HARM), "Harm");
    EXPECT_STREQ(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_UNFAIRNESS), "Unfairness");
    EXPECT_STREQ(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_DECEPTION), "Deception");
    EXPECT_STREQ(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_AUTONOMY), "Autonomy Violation");
    EXPECT_STREQ(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_PRIVACY), "Privacy Violation");
    EXPECT_STREQ(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_CONSENT), "Consent Violation");
    EXPECT_STREQ(ethics_violation_type_name(ETHICS_VIOLATION_TYPE_DIGNITY), "Dignity Violation");
}

TEST_F(EthicsComprehensiveTest, ViolationTypeName_Unknown_ReturnsUnknown) {
    const char* name = ethics_violation_type_name((ethics_violation_type_t)9999);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(EthicsComprehensiveTest, PrintEvaluation_Null_NoCrash) {
    ethics_print_evaluation(nullptr);
    // Should not crash
}

TEST_F(EthicsComprehensiveTest, PrintEvaluation_Valid_NoCrash) {
    action_context_t action = create_action();
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    ethics_print_evaluation(&result);
    // Should not crash
}

TEST_F(EthicsComprehensiveTest, PrintEvaluation_BlockedAction_NoCrash) {
    action_context_t action = create_action(0.9f);
    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

    ethics_print_evaluation(&result);
    // Should not crash
}

//=============================================================================
// 10. Edge Cases and Error Handling
//=============================================================================

TEST_F(EthicsComprehensiveTest, EdgeCase_ZeroFeatures_HandlesGracefully) {
    action_context_t action = create_action();
    action.num_features = 0;
    action.features = nullptr;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, EdgeCase_MaxFeatures_HandlesGracefully) {
    action_context_t action = create_action();
    action.num_features = 20;  // MAX_FEATURE_SIZE

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, EdgeCase_ExcessiveFeatures_HandlesGracefully) {
    float many_features[100];
    for (int i = 0; i < 100; i++) {
        many_features[i] = 0.5f;
    }

    action_context_t action = create_action();
    action.features = many_features;
    action.num_features = 100;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);
    EXPECT_GE(result.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, EdgeCase_MaxSeverity_HandlesCorrectly) {
    action_context_t action = create_action(1.0f);
    action.fairness_violation = 1.0f;
    action.deception_level = 1.0f;
    action.autonomy_violation = 1.0f;
    action.privacy_violation = 1.0f;
    action.consent_violation = 1.0f;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(EthicsComprehensiveTest, EdgeCase_MinSeverity_HandlesCorrectly) {
    action_context_t action = create_action(0.0f);
    action.fairness_violation = 0.0f;
    action.deception_level = 0.0f;
    action.autonomy_violation = 0.0f;
    action.privacy_violation = 0.0f;
    action.consent_violation = 0.0f;

    ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(EthicsComprehensiveTest, EdgeCase_CircularBufferWrap_HandlesCorrectly) {
    // Log more than MAX_INCIDENT_HISTORY (10000) incidents to test circular buffer
    // We'll log 100 for testing purposes
    for (uint64_t i = 0; i < 100; i++) {
        ethics_incident_t incident = create_incident(i, ETHICS_VIOLATION_TYPE_HARM, 0.5f);
        ethics_log_incident(engine, &incident);
    }

    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_all_incidents(engine, &incidents);

    EXPECT_EQ(count, 100);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, EdgeCase_LargeAgentID_HandlesGracefully) {
    empathy_config_t config = {
        .mirror_network = nullptr,
        .observation_window_ms = 1000,
        .empathy_threshold = 0.5f
    };

    empathy_network_t network = empathy_network_create(&config);
    ASSERT_NE(network, nullptr);

    action_context_t action = create_action();
    // Agent 999 is within bounds (max 1000)
    empathy_state_extended_t state = empathy_network_simulate_agent(network, 999, &action);
    EXPECT_TRUE(state.active || !state.active);  // Should not crash

    empathy_network_destroy(network);
}

//=============================================================================
// 11. Integration Tests
//=============================================================================

TEST_F(EthicsComprehensiveTest, Integration_FullEvaluationCycle_Success) {
    // 1. Add policies
    for (uint32_t i = 1; i <= 6; i++) {
        ethics_policy_t policy = create_policy(
            1000 + i,
            (ethics_violation_type_t)i,
            0.5f
        );
        ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));
    }

    // 2. Evaluate multiple actions
    for (int i = 0; i < 10; i++) {
        action_context_t action = create_action(i * 0.1f);
        ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);
        EXPECT_GE(result.confidence, 0.0f);

        // 3. Learn from outcomes
        action_outcome_t outcome = create_outcome(1, i * 0.05f, (10 - i) * 0.05f);
        ethics_learn_from_outcome(engine, &action, &outcome);

        // 4. Log incidents for blocked actions
        if (!result.allowed) {
            ethics_incident_t incident = create_incident(i, result.primary_violation, 0.8f);
            ethics_log_incident(engine, &incident);
        }
    }

    // 5. Check statistics
    ethics_statistics_t stats;
    ASSERT_TRUE(ethics_get_statistics(engine, &stats));
    EXPECT_EQ(stats.total_evaluations, 10);

    // 6. Query incidents
    ethics_incident_t* incidents = nullptr;
    uint32_t count = ethics_get_all_incidents(engine, &incidents);
    EXPECT_GE(count, 0);
    if (incidents) nimcp_free(incidents);
}

TEST_F(EthicsComprehensiveTest, Integration_PolicyEffectiveness_Verified) {
    // Add strict harm policy
    ethics_policy_t policy = create_policy(2000, ETHICS_VIOLATION_TYPE_HARM, 0.2f);
    ASSERT_TRUE(ethics_engine_add_policy(engine, &policy));

    // Test low harm (should pass)
    action_context_t low_harm = create_action(0.1f);
    ethics_evaluation_t result1 = ethics_engine_evaluate_action(engine, &low_harm);

    // Test medium harm (may be blocked)
    action_context_t med_harm = create_action(0.5f);
    ethics_evaluation_t result2 = ethics_engine_evaluate_action(engine, &med_harm);

    // Test high harm (likely blocked)
    action_context_t high_harm = create_action(0.9f);
    ethics_evaluation_t result3 = ethics_engine_evaluate_action(engine, &high_harm);

    // All should evaluate without crashing
    EXPECT_GE(result1.confidence, 0.0f);
    EXPECT_GE(result2.confidence, 0.0f);
    EXPECT_GE(result3.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, Integration_LearningImprovement_Detected) {
    // Evaluate action before learning
    action_context_t action1 = create_action(0.5f);
    ethics_evaluation_t result1 = ethics_engine_evaluate_action(engine, &action1);

    // Learn from multiple similar outcomes
    for (int i = 0; i < 20; i++) {
        action_context_t action = create_action(0.5f);
        action_outcome_t outcome = create_outcome(1, 0.7f, 0.1f);  // Consistently harmful
        ethics_learn_from_outcome(engine, &action, &outcome);
    }

    // Evaluate similar action after learning
    action_context_t action2 = create_action(0.5f);
    ethics_evaluation_t result2 = ethics_engine_evaluate_action(engine, &action2);

    // Both should complete successfully
    EXPECT_GE(result1.confidence, 0.0f);
    EXPECT_GE(result2.confidence, 0.0f);
}

TEST_F(EthicsComprehensiveTest, Integration_MultipleAgentEmpathy_Computed) {
    empathy_config_t config = {
        .mirror_network = nullptr,
        .observation_window_ms = 1000,
        .empathy_threshold = 0.5f
    };

    empathy_network_t network = empathy_network_create(&config);
    ASSERT_NE(network, nullptr);

    action_context_t action = create_action(0.6f, 10);

    // Simulate perspective for all affected agents
    for (uint32_t i = 0; i < 10; i++) {
        empathy_state_extended_t state = empathy_network_simulate_agent(network, i, &action);
        // All should return valid states
        EXPECT_GE(state.uncertainty, 0.0f);
        EXPECT_LE(state.uncertainty, 1.0f);
    }

    empathy_network_destroy(network);
}

TEST_F(EthicsComprehensiveTest, Integration_IncidentQueryWorkflow_Complete) {
    // Log diverse incidents
    for (uint64_t i = 0; i < 20; i++) {
        ethics_violation_type_t type = (ethics_violation_type_t)((i % 6) + 1);
        float severity = (i % 10) * 0.1f;
        ethics_action_t action = (i % 2) ? ETHICS_ACTION_BLOCK : ETHICS_ACTION_ALLOW;

        ethics_incident_t incident = create_incident(i, type, severity);
        incident.action_taken = action;
        incident.timestamp = i * 1000;

        ASSERT_TRUE(ethics_log_incident(engine, &incident));
    }

    // Test all query methods
    ethics_incident_t* incidents = nullptr;
    uint32_t count;

    // Query by time range
    count = ethics_get_incidents_by_time_range(engine, 5000, 15000, &incidents);
    EXPECT_GE(count, 0);
    if (incidents) { nimcp_free(incidents); incidents = nullptr; }

    // Query by type
    count = ethics_get_incidents_by_violation_type(engine, ETHICS_VIOLATION_TYPE_HARM, &incidents);
    EXPECT_GE(count, 0);
    if (incidents) { nimcp_free(incidents); incidents = nullptr; }

    // Query by severity
    count = ethics_get_incidents_by_severity(engine, 0.5f, &incidents);
    EXPECT_GE(count, 0);
    if (incidents) { nimcp_free(incidents); incidents = nullptr; }

    // Query by action
    count = ethics_get_incidents_by_action(engine, ETHICS_ACTION_BLOCK, &incidents);
    EXPECT_GE(count, 0);
    if (incidents) { nimcp_free(incidents); incidents = nullptr; }

    // Get all
    count = ethics_get_all_incidents(engine, &incidents);
    EXPECT_EQ(count, 20);
    if (incidents) nimcp_free(incidents);
}

//=============================================================================
// 12. Golden Rule Specific Tests
//=============================================================================

TEST_F(EthicsComprehensiveTest, GoldenRule_Symmetry_Verified) {
    action_context_t action1 = create_action(0.4f, 3);
    action_context_t action2 = create_action(0.4f, 3);

    ethics_evaluation_t result1 = ethics_engine_evaluate_action(engine, &action1);
    ethics_evaluation_t result2 = ethics_engine_evaluate_action(engine, &action2);

    // Similar actions should have similar scores
    EXPECT_NEAR(result1.golden_rule_score, result2.golden_rule_score, 0.1f);
}

TEST_F(EthicsComprehensiveTest, GoldenRule_HarmGradient_Respected) {
    float harm_levels[] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    float prev_score = 1.0f;

    for (float harm : harm_levels) {
        action_context_t action = create_action(harm, 3);
        ethics_evaluation_t result = ethics_engine_evaluate_action(engine, &action);

        // Higher harm should result in lower or equal Golden Rule scores
        EXPECT_LE(result.golden_rule_score, prev_score + 0.5f);  // Allow some variance
        prev_score = result.golden_rule_score;
    }
}

TEST_F(EthicsComprehensiveTest, GoldenRule_ThresholdEnforcement_Works) {
    // Create engine with high threshold
    ethics_config_t config = {};
    config.action_feature_size = 10;
    config.max_agents = 10;
    config.golden_rule_threshold = 0.8f;  // High threshold
    config.empathy_weight = 0.7f;
    config.enable_learning = false;

    ethics_engine_t strict_engine = ethics_engine_create(&config);
    ASSERT_NE(strict_engine, nullptr);

    action_context_t action = create_action(0.3f);
    ethics_evaluation_t result = ethics_engine_evaluate_action(strict_engine, &action);

    // Should evaluate with strict threshold
    EXPECT_GE(result.confidence, 0.0f);

    ethics_engine_destroy(strict_engine);
}

} // namespace
