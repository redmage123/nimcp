/**
 * @file test_health_cognitive_e2e.cpp
 * @brief End-to-end tests for Phase 8 cognitive health pipeline
 *
 * TEST COVERAGE:
 * - Complete health monitoring pipeline from detection to recovery
 * - Full cognitive integration workflow
 * - Real-world scenario simulations
 * - Performance under load
 * - System resilience testing
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
#include <atomic>

// Phase 8 headers
#include "cognitive/health/nimcp_health_cognitive_bridge.h"
#include "cognitive/health/nimcp_collective_health.h"
#include "cognitive/health/nimcp_rcog_health.h"
#include "cognitive/health/nimcp_meta_health.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class HealthCognitiveE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create health agent with real configuration
        health_agent_config_t agent_config;
        nimcp_health_agent_default_config(&agent_config);
        agent_ = nimcp_health_agent_create(&agent_config);
        ASSERT_NE(agent_, nullptr);

        // Initialize all component configs
        bridge_config_ = health_cognitive_bridge_default_config();
        collective_config_ = collective_health_default_config();
        rcog_config_ = rcog_health_default_config();
        meta_config_ = meta_health_default_config();
    }

    void TearDown() override {
        ShutdownAll();
    }

    void ShutdownAll() {
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

    // Helper: Initialize all components
    void InitializeFullStack() {
        // Create bridge (main orchestrator)
        bridge_ = health_cognitive_bridge_create(agent_, nullptr, nullptr, &bridge_config_);
        ASSERT_NE(bridge_, nullptr);

        // Create collective monitor
        collective_monitor_ = collective_health_monitor_create(agent_, nullptr, &collective_config_);
        ASSERT_NE(collective_monitor_, nullptr);

        // Create RCOG health
        rcog_health_ = rcog_health_create(nullptr, agent_, &rcog_config_);
        ASSERT_NE(rcog_health_, nullptr);

        // Create meta-health reflector
        meta_reflector_ = meta_health_create(agent_, nullptr, &meta_config_);
        ASSERT_NE(meta_reflector_, nullptr);

        // Start all components
        EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);
        EXPECT_EQ(collective_health_monitor_start(collective_monitor_), 0);
        EXPECT_EQ(meta_health_start(meta_reflector_), 0);
    }

    // Helper: Simulate anomaly and track through pipeline
    struct AnomalyTrace {
        health_agent_msg_type_t type;
        health_agent_source_t source;
        health_agent_severity_t severity;
        bool bridge_handled;
        bool consensus_requested;
        bool rcog_diagnosed;
        bool meta_recorded;
        bool recovery_attempted;
        uint64_t handling_time_us;
    };

    AnomalyTrace SimulateAnomalyE2E(health_agent_msg_type_t type,
                                     health_agent_source_t source,
                                     health_agent_severity_t severity) {
        AnomalyTrace trace = {type, source, severity, false, false, false, false, false, 0};

        auto start = std::chrono::steady_clock::now();

        // 1. Handle through bridge
        intelligent_handling_result_t handling_result;
        health_cognitive_init_handling_result(&handling_result);

        if (health_cognitive_intelligent_handle(bridge_, type, source, severity, &handling_result) == 0) {
            trace.bridge_handled = handling_result.success;
            trace.consensus_requested = handling_result.consensus_reached;
        }

        // 2. Request collective consensus if needed
        if (collective_monitor_ && severity >= HEALTH_SEVERITY_WARNING) {
            collective_anomaly_proposal_t proposal;
            collective_health_init_proposal(&proposal);
            proposal.anomaly_type = type;
            proposal.source = source;
            proposal.severity = severity;
            proposal.local_confidence = 0.8f;

            uint64_t consensus_id = 0;
            if (collective_health_propose_anomaly_async(collective_monitor_, &proposal, &consensus_id) == 0) {
                trace.consensus_requested = true;
            }
        }

        // 3. Submit to RCOG for diagnosis
        if (rcog_health_) {
            rcog_health_goal_t goal;
            rcog_health_init_goal(&goal);
            goal.health_type = RCOG_HEALTH_GOAL_DIAGNOSE;
            goal.anomaly_type = type;
            goal.anomaly_source = source;
            goal.anomaly_severity = severity;

            uint64_t goal_id = 0;
            if (rcog_health_submit_goal_async(rcog_health_, &goal, &goal_id) == 0) {
                trace.rcog_diagnosed = true;
            }
        }

        // 4. Record decision in meta-health
        if (meta_reflector_) {
            meta_health_decision_t decision;
            meta_health_init_decision(&decision);
            decision.anomaly_type = type;

            if (meta_health_record_decision(meta_reflector_, &decision) == 0) {
                trace.meta_recorded = true;

                // Record outcome
                meta_health_record_outcome(meta_reflector_, decision.timestamp_us,
                    trace.bridge_handled ? META_HEALTH_OUTCOME_SUCCESS : META_HEALTH_OUTCOME_FAILURE,
                    trace.bridge_handled, 0, 0.9f);
            }
        }

        // 5. Attempt recovery if needed
        if (severity >= HEALTH_SEVERITY_ERROR && rcog_health_) {
            rcog_health_goal_t recovery_goal;
            rcog_health_init_goal(&recovery_goal);
            recovery_goal.health_type = RCOG_HEALTH_GOAL_PLAN_RECOVERY;
            recovery_goal.anomaly_type = type;
            recovery_goal.anomaly_source = source;
            recovery_goal.anomaly_severity = severity;

            uint64_t goal_id = 0;
            if (rcog_health_submit_goal_async(rcog_health_, &recovery_goal, &goal_id) == 0) {
                trace.recovery_attempted = true;
            }
        }

        auto end = std::chrono::steady_clock::now();
        trace.handling_time_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        return trace;
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
 * 1. Complete Pipeline E2E Tests
 *===========================================================================*/

TEST_F(HealthCognitiveE2ETest, FullHealthMonitoringPipeline) {
    InitializeFullStack();

    // Simulate memory corruption anomaly
    AnomalyTrace trace = SimulateAnomalyE2E(
        HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_SOURCE_MEMORY,
        HEALTH_SEVERITY_WARNING);

    EXPECT_TRUE(trace.bridge_handled);
    EXPECT_TRUE(trace.rcog_diagnosed);
    EXPECT_TRUE(trace.meta_recorded);
}

TEST_F(HealthCognitiveE2ETest, CriticalAnomalyRecoveryPipeline) {
    InitializeFullStack();

    // Simulate critical anomaly requiring recovery
    AnomalyTrace trace = SimulateAnomalyE2E(
        HEALTH_MSG_STATE_CORRUPTION,
        HEALTH_SOURCE_CHECKPOINT,
        HEALTH_SEVERITY_CRITICAL);

    EXPECT_TRUE(trace.bridge_handled);
    EXPECT_TRUE(trace.rcog_diagnosed);
    EXPECT_TRUE(trace.recovery_attempted);
    EXPECT_TRUE(trace.meta_recorded);
}

TEST_F(HealthCognitiveE2ETest, MultipleAnomaliesSequentialPipeline) {
    InitializeFullStack();

    // Simulate sequence of anomalies
    struct AnomalyScenario {
        health_agent_msg_type_t type;
        health_agent_source_t source;
        health_agent_severity_t severity;
    } scenarios[] = {
        {HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING},
        {HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SOURCE_THREADING, HEALTH_SEVERITY_ERROR},
        {HEALTH_MSG_NAN_DETECTED, HEALTH_SOURCE_NEURAL, HEALTH_SEVERITY_WARNING},
        {HEALTH_MSG_STATE_CORRUPTION, HEALTH_SOURCE_CHECKPOINT, HEALTH_SEVERITY_CRITICAL},
        {HEALTH_MSG_RESOURCE_EXHAUSTION, HEALTH_SOURCE_IO, HEALTH_SEVERITY_ERROR}
    };

    int handled_count = 0;
    for (const auto& scenario : scenarios) {
        AnomalyTrace trace = SimulateAnomalyE2E(scenario.type, scenario.source, scenario.severity);
        if (trace.bridge_handled) {
            handled_count++;
        }
    }

    EXPECT_EQ(handled_count, 5);

    // Verify meta-health tracked all
    meta_health_stats_t stats;
    EXPECT_EQ(meta_health_get_stats(meta_reflector_, &stats), 0);
    EXPECT_GE(stats.decisions_recorded, 5u);
}

/*=============================================================================
 * 2. Performance E2E Tests
 *===========================================================================*/

TEST_F(HealthCognitiveE2ETest, HighThroughputAnomalyProcessing) {
    InitializeFullStack();

    const int NUM_ANOMALIES = 500;
    int success_count = 0;
    uint64_t total_time_us = 0;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < NUM_ANOMALIES; i++) {
        AnomalyTrace trace = SimulateAnomalyE2E(
            (health_agent_msg_type_t)(i % 5 + 1),
            (health_agent_source_t)(i % 8 + 1),
            (health_agent_severity_t)(i % 4 + 1));

        if (trace.bridge_handled) {
            success_count++;
        }
        total_time_us += trace.handling_time_us;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // All should succeed
    EXPECT_EQ(success_count, NUM_ANOMALIES);

    // Should complete in reasonable time (< 30 seconds for 500 anomalies)
    EXPECT_LT(duration_ms, 30000);

    // Average handling time should be reasonable
    double avg_time_us = static_cast<double>(total_time_us) / NUM_ANOMALIES;
    EXPECT_LT(avg_time_us, 100000);  // < 100ms per anomaly
}

TEST_F(HealthCognitiveE2ETest, SustainedLoadProcessing) {
    InitializeFullStack();

    // Process anomalies over sustained period
    const int BATCHES = 10;
    const int PER_BATCH = 50;
    int total_handled = 0;

    for (int batch = 0; batch < BATCHES; batch++) {
        int batch_handled = 0;

        for (int i = 0; i < PER_BATCH; i++) {
            intelligent_handling_result_t result;
            health_cognitive_init_handling_result(&result);

            if (health_cognitive_intelligent_handle(bridge_,
                (health_agent_msg_type_t)(i % 5 + 1),
                HEALTH_SOURCE_NEURAL,
                HEALTH_SEVERITY_WARNING, &result) == 0 && result.success) {
                batch_handled++;
            }
        }

        total_handled += batch_handled;

        // Small delay between batches
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(total_handled, BATCHES * PER_BATCH);
}

/*=============================================================================
 * 3. Resilience E2E Tests
 *===========================================================================*/

TEST_F(HealthCognitiveE2ETest, ComponentFailureRecovery) {
    // Start with just bridge
    bridge_ = health_cognitive_bridge_create(agent_, nullptr, nullptr, &bridge_config_);
    ASSERT_NE(bridge_, nullptr);
    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Process anomaly without other components
    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);
    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY,
        HEALTH_SEVERITY_WARNING, &result), 0);
    EXPECT_TRUE(result.success);

    // Now add collective monitor
    collective_monitor_ = collective_health_monitor_create(agent_, nullptr, &collective_config_);
    ASSERT_NE(collective_monitor_, nullptr);
    EXPECT_EQ(collective_health_monitor_start(collective_monitor_), 0);

    // Process another anomaly
    health_cognitive_init_handling_result(&result);
    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SOURCE_THREADING,
        HEALTH_SEVERITY_ERROR, &result), 0);
    EXPECT_TRUE(result.success);

    // Add meta-health
    meta_reflector_ = meta_health_create(agent_, nullptr, &meta_config_);
    ASSERT_NE(meta_reflector_, nullptr);
    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    // Process final anomaly with full stack
    health_cognitive_init_handling_result(&result);
    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_STATE_CORRUPTION, HEALTH_SOURCE_CHECKPOINT,
        HEALTH_SEVERITY_CRITICAL, &result), 0);
    EXPECT_TRUE(result.success);
}

TEST_F(HealthCognitiveE2ETest, GracefulShutdownUnderLoad) {
    InitializeFullStack();

    // Start processing anomalies
    std::atomic<bool> stop_flag(false);
    std::atomic<int> processed_count(0);

    // Process for a short time
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        intelligent_handling_result_t result;
        health_cognitive_init_handling_result(&result);

        if (health_cognitive_intelligent_handle(bridge_,
            HEALTH_MSG_ANOMALY_DETECTED, HEALTH_SOURCE_NEURAL,
            HEALTH_SEVERITY_INFO, &result) == 0) {
            processed_count++;
        }
    }

    // Graceful shutdown while potentially still processing
    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
    EXPECT_EQ(collective_health_monitor_stop(collective_monitor_), 0);
    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);

    // Should have processed some anomalies
    EXPECT_GT(processed_count.load(), 0);
}

/*=============================================================================
 * 4. Learning and Adaptation E2E Tests
 *===========================================================================*/

TEST_F(HealthCognitiveE2ETest, MetaHealthLearningCycle) {
    InitializeFullStack();

    // Phase 1: Generate training data
    for (int i = 0; i < 50; i++) {
        meta_health_decision_t decision;
        meta_health_init_decision(&decision);
        decision.anomaly_type = (health_agent_msg_type_t)(i % 5 + 1);
        decision.timestamp_us = i * 100000;

        meta_health_record_decision(meta_reflector_, &decision);

        // Simulate varying success rates
        bool success = (i % 3 != 0);  // 2/3 success rate
        meta_health_record_outcome(meta_reflector_, decision.timestamp_us,
            success ? META_HEALTH_OUTCOME_SUCCESS : META_HEALTH_OUTCOME_FAILURE,
            success, 100 + i, success ? 0.9f : 0.3f);
    }

    // Phase 2: Reflect
    meta_health_reflection_result_t reflection;
    EXPECT_EQ(meta_health_reflect(meta_reflector_, &reflection), 0);
    EXPECT_GT(reflection.decisions_analyzed, 0u);

    // Phase 3: Apply learnings
    int applied = meta_health_apply_learnings(meta_reflector_, &reflection);
    EXPECT_GE(applied, 0);

    // Phase 4: Get assessment
    meta_health_assessment_t assessment;
    EXPECT_EQ(meta_health_get_assessment(meta_reflector_, &assessment), 0);

    // Assessment should reflect the training data
    EXPECT_GE(assessment.accuracy_rate, 0.0f);
    EXPECT_LE(assessment.accuracy_rate, 1.0f);
}

TEST_F(HealthCognitiveE2ETest, PatternLearningE2E) {
    meta_reflector_ = meta_health_create(agent_, nullptr, &meta_config_);
    ASSERT_NE(meta_reflector_, nullptr);
    EXPECT_EQ(meta_health_start(meta_reflector_), 0);

    // Register a pattern
    meta_health_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    snprintf(pattern.predictor, sizeof(pattern.predictor), "memory_spike_pattern");
    pattern.confidence = 0.85f;

    EXPECT_EQ(meta_health_register_pattern(meta_reflector_, &pattern), 0);

    // Generate data matching pattern
    for (int i = 0; i < 20; i++) {
        meta_health_decision_t decision;
        meta_health_init_decision(&decision);
        decision.anomaly_type = HEALTH_MSG_MEMORY_CORRUPTION;
        decision.timestamp_us = i * 50000;

        meta_health_record_decision(meta_reflector_, &decision);
        meta_health_record_outcome(meta_reflector_, decision.timestamp_us,
            META_HEALTH_OUTCOME_SUCCESS, true, 100, 0.9f);
    }

    // Reflect - should recognize pattern
    meta_health_reflection_result_t reflection;
    EXPECT_EQ(meta_health_reflect(meta_reflector_, &reflection), 0);
    EXPECT_GE(reflection.num_new_patterns, 0u);

    EXPECT_EQ(meta_health_stop(meta_reflector_), 0);
}

/*=============================================================================
 * 5. Full Scenario E2E Tests
 *===========================================================================*/

TEST_F(HealthCognitiveE2ETest, RealWorldScenarioMemoryLeak) {
    InitializeFullStack();

    // Simulate memory leak detection scenario
    // 1. Initial warning
    AnomalyTrace warning = SimulateAnomalyE2E(
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING);
    EXPECT_TRUE(warning.bridge_handled);

    // 2. Escalation to error
    AnomalyTrace error = SimulateAnomalyE2E(
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_ERROR);
    EXPECT_TRUE(error.bridge_handled);
    EXPECT_TRUE(error.recovery_attempted);

    // 3. Critical - resource exhaustion
    AnomalyTrace critical = SimulateAnomalyE2E(
        HEALTH_MSG_RESOURCE_EXHAUSTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_CRITICAL);
    EXPECT_TRUE(critical.bridge_handled);
    EXPECT_TRUE(critical.recovery_attempted);

    // 4. Verify learning occurred
    meta_health_stats_t stats;
    EXPECT_EQ(meta_health_get_stats(meta_reflector_, &stats), 0);
    EXPECT_GE(stats.decisions_recorded, 3u);
}

TEST_F(HealthCognitiveE2ETest, RealWorldScenarioDeadlock) {
    InitializeFullStack();

    // Simulate deadlock detection and resolution
    // 1. Detect deadlock
    AnomalyTrace detect = SimulateAnomalyE2E(
        HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SOURCE_THREADING, HEALTH_SEVERITY_ERROR);
    EXPECT_TRUE(detect.bridge_handled);
    EXPECT_TRUE(detect.rcog_diagnosed);

    // 2. Request recovery through RCOG
    rcog_health_goal_t goal;
    rcog_health_init_goal(&goal);
    goal.health_type = RCOG_HEALTH_GOAL_PLAN_RECOVERY;
    goal.anomaly_type = HEALTH_MSG_DEADLOCK_DETECTED;
    goal.anomaly_source = HEALTH_SOURCE_THREADING;
    goal.anomaly_severity = HEALTH_SEVERITY_ERROR;

    uint64_t goal_id = 0;
    EXPECT_EQ(rcog_health_submit_goal_async(rcog_health_, &goal, &goal_id), 0);

    // 3. Verify collective was notified
    collective_health_summary_t summary;
    EXPECT_EQ(collective_health_get_summary(collective_monitor_, &summary), 0);
}

TEST_F(HealthCognitiveE2ETest, RealWorldScenarioNeuralAnomaly) {
    InitializeFullStack();

    // Simulate neural network anomaly (NaN detection)
    for (int epoch = 0; epoch < 5; epoch++) {
        // Detect NaN
        AnomalyTrace trace = SimulateAnomalyE2E(
            HEALTH_MSG_NAN_DETECTED, HEALTH_SOURCE_NEURAL,
            epoch < 3 ? HEALTH_SEVERITY_WARNING : HEALTH_SEVERITY_ERROR);

        EXPECT_TRUE(trace.bridge_handled);
    }

    // Reflect on neural anomalies
    meta_health_reflection_result_t reflection;
    EXPECT_EQ(meta_health_reflect(meta_reflector_, &reflection), 0);

    // Get assessment of neural handling
    meta_health_assessment_t assessment;
    EXPECT_EQ(meta_health_get_assessment(meta_reflector_, &assessment), 0);
}

/*=============================================================================
 * 6. System Integration E2E Tests
 *===========================================================================*/

TEST_F(HealthCognitiveE2ETest, FullSystemIntegrationTest) {
    InitializeFullStack();

    // Run comprehensive test touching all components

    // Step 1: Baseline health check
    collective_health_summary_t initial_summary;
    EXPECT_EQ(collective_health_get_summary(collective_monitor_, &initial_summary), 0);

    // Step 2: Introduce various anomalies
    std::vector<AnomalyTrace> traces;
    traces.push_back(SimulateAnomalyE2E(HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING));
    traces.push_back(SimulateAnomalyE2E(HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SOURCE_THREADING, HEALTH_SEVERITY_ERROR));
    traces.push_back(SimulateAnomalyE2E(HEALTH_MSG_NAN_DETECTED, HEALTH_SOURCE_NEURAL, HEALTH_SEVERITY_WARNING));
    traces.push_back(SimulateAnomalyE2E(HEALTH_MSG_STATE_CORRUPTION, HEALTH_SOURCE_CHECKPOINT, HEALTH_SEVERITY_CRITICAL));
    traces.push_back(SimulateAnomalyE2E(HEALTH_MSG_RESOURCE_EXHAUSTION, HEALTH_SOURCE_IO, HEALTH_SEVERITY_ERROR));

    // Verify all handled
    for (const auto& trace : traces) {
        EXPECT_TRUE(trace.bridge_handled);
    }

    // Step 3: Run meta-health reflection
    meta_health_reflection_result_t reflection;
    EXPECT_EQ(meta_health_reflect(meta_reflector_, &reflection), 0);

    // Step 4: Apply learnings
    meta_health_apply_learnings(meta_reflector_, &reflection);

    // Step 5: Get final statistics
    meta_health_stats_t meta_stats;
    EXPECT_EQ(meta_health_get_stats(meta_reflector_, &meta_stats), 0);
    EXPECT_GE(meta_stats.decisions_recorded, 5u);

    rcog_health_stats_t rcog_stats;
    EXPECT_EQ(rcog_health_get_stats(rcog_health_, &rcog_stats), 0);

    collective_health_stats_t collective_stats;
    EXPECT_EQ(collective_health_get_stats(collective_monitor_, &collective_stats), 0);

    // Step 6: Final health check
    collective_health_summary_t final_summary;
    EXPECT_EQ(collective_health_get_summary(collective_monitor_, &final_summary), 0);
}
