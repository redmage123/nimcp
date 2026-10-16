/**
 * @file test_health_cognitive_integration.cpp
 * @brief Integration tests for Phase 8 cognitive health subsystems
 *
 * TEST COVERAGE:
 * - Health cognitive bridge with real health agent
 * - Collective health monitoring with multiple instances
 * - RCOG health integration for intelligent diagnosis
 * - Meta-health reflection and learning cycle
 * - Cross-component communication
 * - Recovery coordination workflow
 *
 * PHASE: 8 - Collective & Recursive Cognition Integration (Section 27)
 *
 * @author NIMCP Development Team
 * @date 2025-01-18
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>

// Phase 8 headers
#include "cognitive/health/nimcp_health_cognitive_bridge.h"
#include "cognitive/health/nimcp_collective_health.h"
#include "cognitive/health/nimcp_rcog_health.h"
#include "cognitive/health/nimcp_meta_health.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class HealthCognitiveIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create health agent
        health_agent_config_t agent_config;
        nimcp_health_agent_default_config(&agent_config);
        agent_ = nimcp_health_agent_create(&agent_config);
        ASSERT_NE(agent_, nullptr);

        // Get default configs
        bridge_config_ = health_cognitive_bridge_default_config();
        collective_config_ = collective_health_default_config();
        rcog_config_ = rcog_health_default_config();
        meta_config_ = meta_health_default_config();
    }

    void TearDown() override {
        if (bridge_) {
            health_cognitive_bridge_stop(bridge_);
            health_cognitive_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
        if (collective_monitor_) {
            collective_health_monitor_stop(collective_monitor_);
            collective_health_monitor_destroy(collective_monitor_);
            collective_monitor_ = nullptr;
        }
        if (rcog_health_) {
            rcog_health_destroy(rcog_health_);
            rcog_health_ = nullptr;
        }
        if (meta_reflector_) {
            meta_health_stop(meta_reflector_);
            meta_health_destroy(meta_reflector_);
            meta_reflector_ = nullptr;
        }
        if (agent_) {
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }

    // Helper: Simulate anomaly detection
    void SimulateAnomaly(health_agent_msg_type_t type, health_agent_source_t source,
                         health_agent_severity_t severity) {
        if (agent_) {
            health_agent_message_t msg = nimcp_health_agent_create_message(
                type, severity, source, "Integration test anomaly");
            nimcp_health_agent_report_anomaly(agent_, &msg);
        }
    }

    nimcp_health_agent_t* agent_ = nullptr;
    health_cognitive_bridge_t* bridge_ = nullptr;
    collective_health_monitor_t* collective_monitor_ = nullptr;
    rcog_health_integration_t* rcog_health_ = nullptr;
    meta_health_reflector_t* meta_reflector_ = nullptr;

    cognitive_bridge_config_t bridge_config_;
    collective_health_config_t collective_config_;
    rcog_health_config_t rcog_config_;
    meta_health_config_t meta_config_;
};

/*=============================================================================
 * 1. Health Cognitive Bridge Integration Tests
 *===========================================================================*/

TEST_F(HealthCognitiveIntegrationTest, BridgeWithHealthAgentIntegration) {
    // Create bridge with real health agent
    bridge_ = health_cognitive_bridge_create(agent_, nullptr, nullptr, &bridge_config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Simulate multiple anomalies and verify handling
    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY,
        HEALTH_SEVERITY_WARNING, &result), 0);
    EXPECT_TRUE(result.success);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveIntegrationTest, BridgeMultipleAnomaliesSequential) {
    bridge_ = health_cognitive_bridge_create(agent_, nullptr, nullptr, &bridge_config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Handle sequence of different anomaly types
    health_agent_msg_type_t types[] = {
        HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_MSG_DEADLOCK_DETECTED,
        HEALTH_MSG_NAN_DETECTED,
        HEALTH_MSG_STATE_CORRUPTION,
        HEALTH_MSG_RESOURCE_EXHAUSTION
    };

    health_agent_source_t sources[] = {
        HEALTH_SOURCE_MEMORY,
        HEALTH_SOURCE_THREADING,
        HEALTH_SOURCE_NEURAL,
        HEALTH_SOURCE_CHECKPOINT,
        HEALTH_SOURCE_IO
    };

    for (int i = 0; i < 5; i++) {
        intelligent_handling_result_t result;
        health_cognitive_init_handling_result(&result);

        EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
            types[i], sources[i], HEALTH_SEVERITY_WARNING, &result), 0);
        EXPECT_TRUE(result.success);
    }

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveIntegrationTest, BridgeSeverityEscalation) {
    bridge_ = health_cognitive_bridge_create(agent_, nullptr, nullptr, &bridge_config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Test escalating severity levels
    health_agent_severity_t severities[] = {
        HEALTH_SEVERITY_INFO,
        HEALTH_SEVERITY_WARNING,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SEVERITY_FATAL
    };

    for (int i = 0; i < 5; i++) {
        intelligent_handling_result_t result;
        health_cognitive_init_handling_result(&result);

        EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
            HEALTH_MSG_ANOMALY_DETECTED, HEALTH_SOURCE_NEURAL,
            severities[i], &result), 0);
        EXPECT_TRUE(result.success);
    }

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

/*=============================================================================
 * 2. Collective Health Integration Tests
 *===========================================================================*/

TEST_F(HealthCognitiveIntegrationTest, CollectiveMonitorWithAgentIntegration) {
    collective_monitor_ = collective_health_monitor_create(agent_, nullptr, &collective_config_);
    ASSERT_NE(collective_monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(collective_monitor_), 0);
    EXPECT_TRUE(collective_health_monitor_is_running(collective_monitor_));

    // Get collective summary
    collective_health_summary_t summary;
    EXPECT_EQ(collective_health_get_summary(collective_monitor_, &summary), 0);
    EXPECT_GE(summary.collective_health_score, 0.0f);
    EXPECT_LE(summary.collective_health_score, 1.0f);

    EXPECT_EQ(collective_health_monitor_stop(collective_monitor_), 0);
}

TEST_F(HealthCognitiveIntegrationTest, CollectiveAnomalyConsensusWorkflow) {
    collective_monitor_ = collective_health_monitor_create(agent_, nullptr, &collective_config_);
    ASSERT_NE(collective_monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(collective_monitor_), 0);

    // Submit anomaly proposal
    collective_anomaly_proposal_t proposal;
    collective_health_init_proposal(&proposal);
    proposal.anomaly_type = HEALTH_MSG_MEMORY_CORRUPTION;
    proposal.source = HEALTH_SOURCE_MEMORY;
    proposal.severity = HEALTH_SEVERITY_WARNING;
    proposal.local_confidence = 0.8f;

    uint64_t consensus_id = 0;
    EXPECT_EQ(collective_health_propose_anomaly_async(collective_monitor_, &proposal, &consensus_id), 0);

    // Check consensus status
    collective_anomaly_consensus_t consensus_result;
    int status = collective_health_check_consensus(collective_monitor_, consensus_id, &consensus_result);
    EXPECT_GE(status, 0);  // 0 = pending, 1 = complete

    EXPECT_EQ(collective_health_monitor_stop(collective_monitor_), 0);
}

TEST_F(HealthCognitiveIntegrationTest, CollectiveSwarmActionWorkflow) {
    collective_monitor_ = collective_health_monitor_create(agent_, nullptr, &collective_config_);
    ASSERT_NE(collective_monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(collective_monitor_), 0);

    // Request swarm immune action
    swarm_immune_request_t request;
    collective_health_init_swarm_request(&request);
    request.action = SWARM_IMMUNE_QUARANTINE_INSTANCE;
    request.urgency = 0.5f;
    request.target_instance_id = 1;

    uint64_t action_id = 0;
    EXPECT_EQ(collective_health_request_swarm_action_async(collective_monitor_, &request, &action_id), 0);

    // Get statistics
    collective_health_stats_t stats;
    EXPECT_EQ(collective_health_get_stats(collective_monitor_, &stats), 0);

    EXPECT_EQ(collective_health_monitor_stop(collective_monitor_), 0);
}

/*=============================================================================
 * 3. RCOG Health Integration Tests
 *===========================================================================*/

TEST_F(HealthCognitiveIntegrationTest, RcogHealthDiagnosisWorkflow) {
    rcog_health_ = rcog_health_create(nullptr, agent_, &rcog_config_);
    ASSERT_NE(rcog_health_, nullptr);

    // Submit diagnosis goal
    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    goal.health_type = RCOG_HEALTH_GOAL_DIAGNOSE;
    goal.anomaly_type = HEALTH_MSG_MEMORY_CORRUPTION;
    goal.anomaly_source = HEALTH_SOURCE_MEMORY;
    goal.anomaly_severity = HEALTH_SEVERITY_WARNING;

    uint64_t goal_id = 0;
    EXPECT_EQ(rcog_health_submit_goal_async(rcog_health_, &goal, &goal_id), 0);
    EXPECT_NE(goal_id, 0u);

    // Get result (may be pending)
    rcog_health_answer_t result;
    rcog_health_init_answer(&result);
    int status = rcog_health_get_answer(rcog_health_, goal_id, &result, 1000);
    EXPECT_GE(status, 0);
    rcog_health_free_answer(&result);
}

TEST_F(HealthCognitiveIntegrationTest, RcogHealthRecoveryPlanningWorkflow) {
    rcog_health_ = rcog_health_create(nullptr, agent_, &rcog_config_);
    ASSERT_NE(rcog_health_, nullptr);

    // Submit recovery planning goal
    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    goal.health_type = RCOG_HEALTH_GOAL_PLAN_RECOVERY;
    goal.anomaly_type = HEALTH_MSG_STATE_CORRUPTION;
    goal.anomaly_source = HEALTH_SOURCE_CHECKPOINT;
    goal.anomaly_severity = HEALTH_SEVERITY_ERROR;

    uint64_t goal_id = 0;
    EXPECT_EQ(rcog_health_submit_goal_async(rcog_health_, &goal, &goal_id), 0);

    // Get statistics
    rcog_health_stats_t stats;
    EXPECT_EQ(rcog_health_get_stats(rcog_health_, &stats), 0);
}

TEST_F(HealthCognitiveIntegrationTest, RcogHealthPreventionWorkflow) {
    rcog_health_ = rcog_health_create(nullptr, agent_, &rcog_config_);
    ASSERT_NE(rcog_health_, nullptr);

    // Submit prevention goal
    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    goal.health_type = RCOG_HEALTH_GOAL_PREDICT_FAILURE;
    goal.anomaly_type = HEALTH_MSG_RESOURCE_EXHAUSTION;
    goal.anomaly_source = HEALTH_SOURCE_IO;
    goal.anomaly_severity = HEALTH_SEVERITY_WARNING;

    uint64_t goal_id = 0;
    EXPECT_EQ(rcog_health_submit_goal_async(rcog_health_, &goal, &goal_id), 0);
}

/*=============================================================================
 * 4. Meta-Health Integration Tests
 *===========================================================================*/

TEST_F(HealthCognitiveIntegrationTest, MetaHealthReflectionCycleIntegration) {
    meta_reflector_ = meta_health_create(agent_, nullptr, &meta_config_);
    ASSERT_NE(meta_reflector_, nullptr);

    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    // Record multiple decisions
    for (int i = 0; i < 10; i++) {
        meta_health_decision_t decision;
        meta_health_init_decision(&decision);
        decision.anomaly_type = (health_agent_msg_type_t)(i % 5);
        decision.timestamp_us = 1000000 + i * 100000;

        EXPECT_EQ(meta_health_record_decision(meta_reflector_, &decision), 0);

        // Record outcome
        meta_health_outcome_t outcome = (i % 2 == 0) ?
            META_HEALTH_OUTCOME_SUCCESS : META_HEALTH_OUTCOME_PARTIAL_SUCCESS;

        EXPECT_EQ(meta_health_record_outcome(meta_reflector_, decision.timestamp_us,
            outcome, (i % 2 == 0), 100 + i * 10, 0.8f), 0);
    }

    // Perform reflection
    meta_health_reflection_result_t reflection;
    memset(&reflection, 0, sizeof(reflection));
    EXPECT_EQ(meta_health_reflect(meta_reflector_, &reflection), 0);

    // Apply learnings
    int applied = meta_health_apply_learnings(meta_reflector_, &reflection);
    EXPECT_GE(applied, 0);

    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
}

TEST_F(HealthCognitiveIntegrationTest, MetaHealthAssessmentWorkflow) {
    meta_reflector_ = meta_health_create(agent_, nullptr, &meta_config_);
    ASSERT_NE(meta_reflector_, nullptr);

    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    // Record decisions and outcomes
    for (int i = 0; i < 5; i++) {
        meta_health_decision_t decision;
        meta_health_init_decision(&decision);
        decision.anomaly_type = HEALTH_MSG_MEMORY_CORRUPTION;
        decision.timestamp_us = 1000000 * (i + 1);

        meta_health_record_decision(meta_reflector_, &decision);
        meta_health_record_outcome(meta_reflector_, decision.timestamp_us,
            META_HEALTH_OUTCOME_SUCCESS, true, 100, 0.9f);
    }

    // Get assessment
    meta_health_assessment_t assessment;
    EXPECT_EQ(meta_health_get_assessment(meta_reflector_, &assessment), 0);
    EXPECT_GE(assessment.accuracy_rate, 0.0f);
    EXPECT_LE(assessment.accuracy_rate, 1.0f);

    // Get weaknesses
    meta_health_weakness_t weaknesses;
    EXPECT_EQ(meta_health_get_weaknesses(meta_reflector_, &weaknesses), 0);

    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
}

/*=============================================================================
 * 5. Cross-Component Integration Tests
 *===========================================================================*/

TEST_F(HealthCognitiveIntegrationTest, BridgeWithMetaHealthIntegration) {
    // Create meta-health reflector first
    meta_reflector_ = meta_health_create(agent_, nullptr, &meta_config_);
    ASSERT_NE(meta_reflector_, nullptr);

    // Create bridge (will use meta-health for reflection)
    bridge_config_.enable_meta_reflection = true;
    bridge_ = health_cognitive_bridge_create(agent_, nullptr, nullptr, &bridge_config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);
    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    // Handle anomalies through bridge
    for (int i = 0; i < 5; i++) {
        intelligent_handling_result_t result;
        health_cognitive_init_handling_result(&result);

        EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
            HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY,
            HEALTH_SEVERITY_WARNING, &result), 0);
    }

    // Meta-health should track these
    meta_health_stats_t stats;
    meta_health_get_stats(meta_reflector_, &stats);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
}

TEST_F(HealthCognitiveIntegrationTest, CollectiveWithRcogIntegration) {
    // Create RCOG health integration
    rcog_health_ = rcog_health_create(nullptr, agent_, &rcog_config_);
    ASSERT_NE(rcog_health_, nullptr);

    // Create collective monitor
    collective_monitor_ = collective_health_monitor_create(agent_, nullptr, &collective_config_);
    ASSERT_NE(collective_monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(collective_monitor_), 0);

    // Submit anomaly for collective consensus
    collective_anomaly_proposal_t proposal;
    collective_health_init_proposal(&proposal);
    proposal.anomaly_type = HEALTH_MSG_NAN_DETECTED;
    proposal.source = HEALTH_SOURCE_NEURAL;
    proposal.severity = HEALTH_SEVERITY_ERROR;
    proposal.local_confidence = 0.7f;

    uint64_t consensus_id = 0;
    EXPECT_EQ(collective_health_propose_anomaly_async(collective_monitor_, &proposal, &consensus_id), 0);

    // Submit diagnosis goal to RCOG
    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    goal.health_type = RCOG_HEALTH_GOAL_DIAGNOSE;
    goal.anomaly_type = HEALTH_MSG_NAN_DETECTED;
    goal.anomaly_source = HEALTH_SOURCE_NEURAL;

    uint64_t goal_id = 0;
    EXPECT_EQ(rcog_health_submit_goal_async(rcog_health_, &goal, &goal_id), 0);

    EXPECT_EQ(collective_health_monitor_stop(collective_monitor_), 0);
}

/*=============================================================================
 * 6. Stress and Performance Integration Tests
 *===========================================================================*/

TEST_F(HealthCognitiveIntegrationTest, HighVolumeAnomalyHandling) {
    bridge_ = health_cognitive_bridge_create(agent_, nullptr, nullptr, &bridge_config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Handle many anomalies rapidly
    const int NUM_ANOMALIES = 100;
    int success_count = 0;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_ANOMALIES; i++) {
        intelligent_handling_result_t result;
        health_cognitive_init_handling_result(&result);

        if (health_cognitive_intelligent_handle(bridge_,
            (health_agent_msg_type_t)(i % 5 + 1),
            (health_agent_source_t)(i % 8 + 1),
            HEALTH_SEVERITY_WARNING, &result) == 0 && result.success) {
            success_count++;
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // All should succeed
    EXPECT_EQ(success_count, NUM_ANOMALIES);

    // Should complete in reasonable time (less than 5 seconds)
    EXPECT_LT(duration.count(), 5000);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveIntegrationTest, ConcurrentGoalSubmission) {
    rcog_health_ = rcog_health_create(nullptr, agent_, &rcog_config_);
    ASSERT_NE(rcog_health_, nullptr);

    // Submit multiple goals using synchronous API
    const int NUM_GOALS = 20;
    std::vector<rcog_health_answer_t> answers(NUM_GOALS);

    int successful_submissions = 0;
    for (int i = 0; i < NUM_GOALS; i++) {
        rcog_health_goal_t goal;
        rcog_health_init_goal(&goal);
        goal.health_type = (rcog_health_goal_type_t)(i % 3);
        goal.anomaly_type = (health_agent_msg_type_t)(i % 5 + 1);

        // Synchronous submission - returns immediately with answer
        int result = rcog_health_submit_goal(rcog_health_, &goal, &answers[i]);
        if (result == 0) {
            successful_submissions++;
        }
    }

    // At least some goals should be submitted successfully
    EXPECT_GT(successful_submissions, 0);
}

/*=============================================================================
 * 7. Recovery Workflow Integration Tests
 *===========================================================================*/

TEST_F(HealthCognitiveIntegrationTest, FullRecoveryWorkflow) {
    // Create all components
    bridge_ = health_cognitive_bridge_create(agent_, nullptr, nullptr, &bridge_config_);
    ASSERT_NE(bridge_, nullptr);

    meta_reflector_ = meta_health_create(agent_, nullptr, &meta_config_);
    ASSERT_NE(meta_reflector_, nullptr);

    rcog_health_ = rcog_health_create(nullptr, agent_, &rcog_config_);
    ASSERT_NE(rcog_health_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);
    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    // 1. Detect anomaly through bridge
    intelligent_handling_result_t handling_result;
    health_cognitive_init_handling_result(&handling_result);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_STATE_CORRUPTION, HEALTH_SOURCE_CHECKPOINT,
        HEALTH_SEVERITY_ERROR, &handling_result), 0);

    // 2. Submit recovery goal to RCOG
    rcog_health_goal_t recovery_goal;
    rcog_health_init_goal(&recovery_goal);
    recovery_goal.health_type = RCOG_HEALTH_GOAL_PLAN_RECOVERY;
    recovery_goal.anomaly_type = HEALTH_MSG_STATE_CORRUPTION;
    recovery_goal.anomaly_source = HEALTH_SOURCE_CHECKPOINT;
    recovery_goal.anomaly_severity = HEALTH_SEVERITY_ERROR;

    uint64_t goal_id = 0;
    EXPECT_EQ(rcog_health_submit_goal_async(rcog_health_, &recovery_goal, &goal_id), 0);

    // 3. Record decision in meta-health
    meta_health_decision_t decision;
    meta_health_init_decision(&decision);
    decision.anomaly_type = HEALTH_MSG_STATE_CORRUPTION;
    decision.timestamp_us = 1000000;

    EXPECT_EQ(meta_health_record_decision(meta_reflector_, &decision), 0);

    // 4. Record outcome
    EXPECT_EQ(meta_health_record_outcome(meta_reflector_, decision.timestamp_us,
        META_HEALTH_OUTCOME_SUCCESS, true, 500, 0.95f), 0);

    // 5. Reflect on the recovery
    meta_health_reflection_result_t reflection;
    EXPECT_EQ(meta_health_reflect(meta_reflector_, &reflection), 0);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
}
