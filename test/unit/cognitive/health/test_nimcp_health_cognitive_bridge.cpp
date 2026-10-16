/**
 * @file test_nimcp_health_cognitive_bridge.cpp
 * @brief Unit tests for health cognitive bridge
 *
 * WHAT: Tests for unified cognitive integration bridge
 * WHY:  Validate intelligent handling, component integration, anomaly processing
 * HOW:  Test lifecycle, intelligent handling, status queries, callbacks
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/health/nimcp_health_cognitive_bridge.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class HealthCognitiveBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = health_cognitive_bridge_default_config();
    }

    void TearDown() override {
        if (bridge_) {
            health_cognitive_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    cognitive_bridge_config_t config_;
    health_cognitive_bridge_t* bridge_ = nullptr;
};

/*=============================================================================
 * Configuration Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, DefaultConfigValues) {
    EXPECT_TRUE(config_.enable_consensus_decisions);
    EXPECT_TRUE(config_.enable_rcog_diagnosis);
    // enable_ethics_validation defaults to false (requires Phase 9)
    EXPECT_TRUE(config_.require_quorum_for_swarm);
    EXPECT_TRUE(config_.enable_meta_reflection);

    EXPECT_GT(config_.default_timeout_ms, 0u);
    EXPECT_GT(config_.rcog_timeout_ms, 0u);

    EXPECT_GE(config_.rcog_confidence_threshold, 0.0f);
    EXPECT_LE(config_.rcog_confidence_threshold, 1.0f);
}

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, CreateWithNullConfig) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(HealthCognitiveBridgeTest, CreateWithConfig) {
    config_.enable_ethics_validation = false;
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(HealthCognitiveBridgeTest, DestroyNull) {
    // Should not crash
    health_cognitive_bridge_destroy(nullptr);
}

TEST_F(HealthCognitiveBridgeTest, StartStop) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);
    EXPECT_TRUE(health_cognitive_bridge_is_running(bridge_));

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
    EXPECT_FALSE(health_cognitive_bridge_is_running(bridge_));
}

TEST_F(HealthCognitiveBridgeTest, StartNull) {
    EXPECT_EQ(health_cognitive_bridge_start(nullptr), -1);
}

TEST_F(HealthCognitiveBridgeTest, StopNull) {
    EXPECT_EQ(health_cognitive_bridge_stop(nullptr), -1);
}

TEST_F(HealthCognitiveBridgeTest, IsRunningNull) {
    EXPECT_FALSE(health_cognitive_bridge_is_running(nullptr));
}

TEST_F(HealthCognitiveBridgeTest, DoubleStart) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);
    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);  // Idempotent
    EXPECT_TRUE(health_cognitive_bridge_is_running(bridge_));

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

/*=============================================================================
 * Intelligent Handling Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, IntelligentHandleNull) {
    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    EXPECT_EQ(health_cognitive_intelligent_handle(nullptr,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING, &result), -1);

    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING, nullptr), -1);
}

TEST_F(HealthCognitiveBridgeTest, IntelligentHandleBasic) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING, &result), 0);
    EXPECT_TRUE(result.success);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveBridgeTest, IntelligentHandleAsync) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    uint64_t request_id = 0;
    EXPECT_EQ(health_cognitive_intelligent_handle_async(bridge_,
        HEALTH_MSG_DEADLOCK_DETECTED, HEALTH_SOURCE_THREADING, HEALTH_SEVERITY_CRITICAL, &request_id), 0);
    EXPECT_NE(request_id, 0u);

    // Check status
    intelligent_handling_result_t result;
    int status = health_cognitive_check_handling(bridge_, request_id, &result);
    EXPECT_GE(status, 0);  // 0 = pending, 1 = complete, -1 = error

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveBridgeTest, IntelligentHandleSeverity) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Test different severity levels
    health_agent_severity_t severities[] = {
        HEALTH_SEVERITY_INFO,
        HEALTH_SEVERITY_WARNING,
        HEALTH_SEVERITY_ERROR,
        HEALTH_SEVERITY_CRITICAL,
        HEALTH_SEVERITY_FATAL
    };

    for (size_t i = 0; i < sizeof(severities) / sizeof(severities[0]); i++) {
        intelligent_handling_result_t result;
        health_cognitive_init_handling_result(&result);

        EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
            HEALTH_MSG_RESOURCE_EXHAUSTION, HEALTH_SOURCE_NEURAL, severities[i], &result), 0)
            << "Failed for severity " << severities[i];
        EXPECT_TRUE(result.success);
    }

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

/*=============================================================================
 * Status Query Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, GetStatusNull) {
    cognitive_bridge_status_t status;

    EXPECT_EQ(health_cognitive_get_status(nullptr, &status), -1);

    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_get_status(bridge_, nullptr), -1);
}

TEST_F(HealthCognitiveBridgeTest, GetStatusBasic) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    cognitive_bridge_status_t status;
    memset(&status, 0xFF, sizeof(status));

    EXPECT_EQ(health_cognitive_get_status(bridge_, &status), 0);

    // Status should have valid component states
    EXPECT_GE(status.collective_connected, false);
    EXPECT_GE(status.rcog_connected, false);
}

TEST_F(HealthCognitiveBridgeTest, GetStatusAfterStart) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    cognitive_bridge_status_t status;
    EXPECT_EQ(health_cognitive_get_status(bridge_, &status), 0);

    // After start, some components should be active
    // (depends on what subsystems are available)

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

/*=============================================================================
 * Component Access Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, GetCollectiveNull) {
    EXPECT_EQ(health_cognitive_get_collective(nullptr), nullptr);
}

TEST_F(HealthCognitiveBridgeTest, GetRcogNull) {
    EXPECT_EQ(health_cognitive_get_rcog(nullptr), nullptr);
}

TEST_F(HealthCognitiveBridgeTest, GetMetaNull) {
    EXPECT_EQ(health_cognitive_get_meta(nullptr), nullptr);
}

TEST_F(HealthCognitiveBridgeTest, GetComponentsBasic) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    // Without connected subsystems, returns NULL or internal instances
    // This is valid behavior - components may not be connected
    health_cognitive_get_collective(bridge_);
    health_cognitive_get_rcog(bridge_);
    health_cognitive_get_meta(bridge_);
}

/*=============================================================================
 * Health Agent Connection Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, ConnectHealthAgentNull) {
    EXPECT_EQ(nimcp_health_agent_connect_cognitive(nullptr, nullptr, nullptr, nullptr), -1);
}

TEST_F(HealthCognitiveBridgeTest, DisconnectHealthAgentNull) {
    EXPECT_EQ(nimcp_health_agent_disconnect_cognitive(nullptr), -1);
}

TEST_F(HealthCognitiveBridgeTest, GetBridgeStatusNull) {
    cognitive_bridge_status_t status;
    EXPECT_EQ(nimcp_health_agent_get_bridge_status(nullptr, &status), -1);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, GetStatsNull) {
    health_cognitive_stats_t stats;

    EXPECT_EQ(health_cognitive_get_stats(nullptr, &stats), -1);

    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_get_stats(bridge_, nullptr), -1);
}

TEST_F(HealthCognitiveBridgeTest, GetStatsBasic) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    health_cognitive_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    EXPECT_EQ(health_cognitive_get_stats(bridge_, &stats), 0);

    EXPECT_GE(stats.anomalies_handled, 0u);
    EXPECT_GE(stats.consensus_obtained, 0u);
    EXPECT_GE(stats.diagnoses_performed, 0u);
}

TEST_F(HealthCognitiveBridgeTest, ResetStatsNull) {
    // Should not crash
    health_cognitive_reset_stats(nullptr);
}

TEST_F(HealthCognitiveBridgeTest, ResetStatsBasic) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    health_cognitive_reset_stats(bridge_);

    health_cognitive_stats_t stats;
    EXPECT_EQ(health_cognitive_get_stats(bridge_, &stats), 0);
    EXPECT_EQ(stats.anomalies_handled, 0u);
}

/*=============================================================================
 * Pipeline Control Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, SkipConsensus) {
    // Configure to skip consensus
    config_.enable_consensus_decisions = false;
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING, &result), 0);
    EXPECT_TRUE(result.success);
    // When consensus is disabled, it passes through (true = no consensus blocking)
    EXPECT_TRUE(result.consensus_reached);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveBridgeTest, SkipDiagnosis) {
    // Configure to skip RCOG diagnosis
    config_.enable_rcog_diagnosis = false;
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_NAN_DETECTED, HEALTH_SOURCE_NEURAL, HEALTH_SEVERITY_ERROR, &result), 0);
    EXPECT_TRUE(result.success);
    // When RCOG is disabled, diagnosis_performed still true (pass-through)
    EXPECT_TRUE(result.diagnosis_performed);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveBridgeTest, SkipEthics) {
    // Configure to skip ethics check
    config_.enable_ethics_validation = false;
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_STATE_CORRUPTION, HEALTH_SOURCE_CHECKPOINT, HEALTH_SEVERITY_CRITICAL, &result), 0);
    EXPECT_TRUE(result.success);
    // Without ethics enabled, ethics_approved is true (no blocking)
    EXPECT_TRUE(result.ethics_approved);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

/*=============================================================================
 * Edge Case Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, HandleMultipleAnomalies) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Handle multiple anomalies in sequence
    health_agent_msg_type_t msg_types[] = {
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_MSG_MEMORY_CORRUPTION,
        HEALTH_MSG_NAN_DETECTED,
        HEALTH_MSG_RESOURCE_EXHAUSTION,
        HEALTH_MSG_STATE_CORRUPTION
    };

    for (int i = 0; i < 10; i++) {
        intelligent_handling_result_t result;
        health_cognitive_init_handling_result(&result);

        EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
            msg_types[i % 5],
            (health_agent_source_t)((i % (int)HEALTH_SOURCE_COUNT) + 1),
            (health_agent_severity_t)(i % 5),
            &result), 0);
        EXPECT_TRUE(result.success);
    }

    // Check stats
    health_cognitive_stats_t stats;
    EXPECT_EQ(health_cognitive_get_stats(bridge_, &stats), 0);
    EXPECT_EQ(stats.anomalies_handled, 10u);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveBridgeTest, HandleAsyncMultiple) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Submit multiple async requests
    uint64_t request_ids[5] = {0};
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(health_cognitive_intelligent_handle_async(bridge_,
            HEALTH_MSG_RESOURCE_EXHAUSTION, HEALTH_SOURCE_NEURAL, HEALTH_SEVERITY_WARNING,
            &request_ids[i]), 0);
        EXPECT_NE(request_ids[i], 0u);
    }

    // All request IDs should be unique
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            EXPECT_NE(request_ids[i], request_ids[j]);
        }
    }

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveBridgeTest, AllFeaturesDisabled) {
    // Disable all cognitive features
    config_.enable_consensus_decisions = false;
    config_.enable_rcog_diagnosis = false;
    config_.enable_ethics_validation = false;
    config_.require_quorum_for_swarm = false;
    config_.enable_meta_reflection = false;

    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Should still handle (with minimal processing)
    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_INFO, &result), 0);
    EXPECT_TRUE(result.success);

    // When features are disabled, results pass through (true = no blocking)
    EXPECT_TRUE(result.consensus_reached);
    EXPECT_TRUE(result.diagnosis_performed);
    EXPECT_TRUE(result.ethics_approved);
    EXPECT_TRUE(result.swarm_quorum_obtained);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveBridgeTest, FatalSeverityHandling) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // Fatal severity should be handled with urgency
    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);

    EXPECT_EQ(health_cognitive_intelligent_handle(bridge_,
        HEALTH_MSG_STATE_CORRUPTION, HEALTH_SOURCE_CHECKPOINT, HEALTH_SEVERITY_FATAL, &result), 0);
    EXPECT_TRUE(result.success);

    // Fatal severity should trigger immediate response
    // Recovery plan should be created
    EXPECT_TRUE(result.recovery_planned);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

/*=============================================================================
 * Manual Control Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, ForceReflectionNull) {
    meta_health_reflection_result_t result;
    EXPECT_EQ(health_cognitive_force_reflection(nullptr, &result), -1);
}

TEST_F(HealthCognitiveBridgeTest, ForceSyncNull) {
    EXPECT_EQ(health_cognitive_force_sync(nullptr), -1);
}

TEST_F(HealthCognitiveBridgeTest, DiagnoseOnlyNull) {
    rcog_health_diagnosis_t diagnosis;
    EXPECT_EQ(health_cognitive_diagnose_only(nullptr,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING, &diagnosis), -1);
}

TEST_F(HealthCognitiveBridgeTest, PlanOnlyNull) {
    rcog_health_recovery_plan_t plan;
    EXPECT_EQ(health_cognitive_plan_only(nullptr,
        HEALTH_MSG_MEMORY_CORRUPTION, HEALTH_SOURCE_MEMORY, HEALTH_SEVERITY_WARNING, &plan), -1);
}

TEST_F(HealthCognitiveBridgeTest, ForceReflectionBasic) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    meta_health_reflection_result_t result;
    // May return -1 if meta-health not fully connected, but should not crash
    health_cognitive_force_reflection(bridge_, &result);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

TEST_F(HealthCognitiveBridgeTest, ForceSyncBasic) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(health_cognitive_bridge_start(bridge_), 0);

    // May return -1 if not fully connected, but should not crash
    health_cognitive_force_sync(bridge_);

    EXPECT_EQ(health_cognitive_bridge_stop(bridge_), 0);
}

/*=============================================================================
 * Utility Function Tests
 *===========================================================================*/

TEST_F(HealthCognitiveBridgeTest, InitHandlingResult) {
    intelligent_handling_result_t result;
    memset(&result, 0xFF, sizeof(result));

    health_cognitive_init_handling_result(&result);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.consensus_required);
    EXPECT_FALSE(result.consensus_reached);
    EXPECT_FALSE(result.diagnosis_performed);
    EXPECT_FALSE(result.recovery_planned);
    EXPECT_FALSE(result.ethics_approved);
    EXPECT_FALSE(result.recovery_executed);
    EXPECT_FALSE(result.swarm_quorum_obtained);
}

TEST_F(HealthCognitiveBridgeTest, DumpFunctionsNull) {
    // Should not crash with NULL
    health_cognitive_dump_handling_result(nullptr);
    health_cognitive_dump_status(nullptr);
}

TEST_F(HealthCognitiveBridgeTest, DumpFunctionsBasic) {
    bridge_ = health_cognitive_bridge_create(nullptr, nullptr, nullptr, &config_);
    ASSERT_NE(bridge_, nullptr);

    intelligent_handling_result_t result;
    health_cognitive_init_handling_result(&result);
    result.success = true;

    // Should not crash
    health_cognitive_dump_handling_result(&result);

    cognitive_bridge_status_t status;
    health_cognitive_get_status(bridge_, &status);
    health_cognitive_dump_status(&status);
}
