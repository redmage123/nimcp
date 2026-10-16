/**
 * @file test_health_agent_memory_integration.cpp
 * @brief Integration tests for Phase 5.7 Memory System Health Integration
 * @date 2026-01-18
 *
 * Tests integration between health agent and memory system modules
 * (hippocampus, mammillary, engram, consolidation).
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for memory health integration tests
 */
class HealthAgentMemoryIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 100;
        config.message_queue_depth = 64;
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = true;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * Metrics Collection Integration Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryIntegrationTest, MetricsCollectionWithoutModules) {
    memory_health_metrics_t metrics = {};

    /* Start the agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Let agent run briefly */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Collect metrics */
    int result = nimcp_health_agent_get_memory_metrics(agent, &metrics);
    EXPECT_EQ(result, 0);

    /* Verify defaults when no modules connected */
    EXPECT_FLOAT_EQ(metrics.overall_memory_health, 1.0f);
    EXPECT_GT(metrics.last_check_timestamp, 0u);

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryIntegrationTest, MetricsTimestampUpdates) {
    memory_health_metrics_t metrics1 = {};
    memory_health_metrics_t metrics2 = {};

    /* Collect first metrics */
    int result1 = nimcp_health_agent_get_memory_metrics(agent, &metrics1);
    EXPECT_EQ(result1, 0);

    /* Wait a bit */
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    /* Collect second metrics */
    int result2 = nimcp_health_agent_get_memory_metrics(agent, &metrics2);
    EXPECT_EQ(result2, 0);

    /* Second timestamp should be later */
    EXPECT_GE(metrics2.last_check_timestamp, metrics1.last_check_timestamp);
}

/* ============================================================================
 * Consistency Validation Integration Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryIntegrationTest, ConsistencyValidationWithAgent) {
    /* Start the agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Run consistency check */
    int result = nimcp_health_agent_validate_memory_consistency(agent);
    EXPECT_GE(result, 0);  /* Should return 0 or positive inconsistency count */

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryIntegrationTest, ConsistencyCheckMultipleTimes) {
    /* Run multiple consistency checks */
    for (int i = 0; i < 5; i++) {
        int result = nimcp_health_agent_validate_memory_consistency(agent);
        EXPECT_GE(result, 0);
    }
}

/* ============================================================================
 * Recovery Action Integration Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryIntegrationTest, RecoveryActionsWhileRunning) {
    /* Start the agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Trigger various recovery actions while agent is running */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_RESET_CA3, 0), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_HD_DRIFT_CORRECT, 1), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_FORCE_CONSOLIDATION, 2), 0);

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryIntegrationTest, EmergencySaveRecovery) {
    /* Start the agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Trigger emergency save */
    int result = nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_EMERGENCY_SAVE, 2);
    EXPECT_EQ(result, 0);

    /* Agent should still be operational */
    bool needs_attention = nimcp_health_agent_memory_needs_attention(agent);
    /* Result can be true or false, just shouldn't crash */
    (void)needs_attention;

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Configuration Integration Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryIntegrationTest, CustomHippocampusConfig) {
    health_agent_hippocampus_config_t config = {};
    nimcp_health_agent_hippocampus_config_default(&config);

    /* Customize thresholds */
    config.ca3_stability_threshold = 0.8f;
    config.theta_gamma_min_coupling = 0.6f;
    config.health_check_interval_ms = 500;

    /* Can't connect without real hippocampus, but test config handling */
    EXPECT_FLOAT_EQ(config.ca3_stability_threshold, 0.8f);
}

TEST_F(HealthAgentMemoryIntegrationTest, CustomMammillaryConfig) {
    health_agent_mammillary_config_t config = {};
    nimcp_health_agent_mammillary_config_default(&config);

    /* Customize thresholds */
    config.relay_efficiency_threshold = 0.8f;
    config.hd_drift_max_degrees = 3.0f;
    config.health_check_interval_ms = 500;

    /* Verify customization */
    EXPECT_FLOAT_EQ(config.relay_efficiency_threshold, 0.8f);
    EXPECT_FLOAT_EQ(config.hd_drift_max_degrees, 3.0f);
}

/* ============================================================================
 * Cross-Module Integration Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryIntegrationTest, MetricsAndRecoverySequence) {
    memory_health_metrics_t metrics = {};

    /* Start agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Collect initial metrics */
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);

    /* Run consistency check */
    int inconsistencies = nimcp_health_agent_validate_memory_consistency(agent);
    EXPECT_GE(inconsistencies, 0);

    /* If inconsistencies, trigger cross-tier sync */
    if (inconsistencies > 0) {
        EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_CROSS_TIER_SYNC, 2), 0);
    }

    /* Collect metrics again */
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryIntegrationTest, NeedsAttentionDuringOperation) {
    /* Start agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Check attention status multiple times */
    for (int i = 0; i < 3; i++) {
        bool needs_attention = nimcp_health_agent_memory_needs_attention(agent);
        /* Without connected modules, should not need attention */
        EXPECT_FALSE(needs_attention);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryIntegrationTest, RapidMetricsCollection) {
    memory_health_metrics_t metrics = {};

    /* Start agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Rapid metrics collection */
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);
    }

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryIntegrationTest, RapidRecoveryActions) {
    /* Start agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Rapid recovery actions */
    for (int i = 0; i < 50; i++) {
        memory_recovery_action_t action = static_cast<memory_recovery_action_t>(i % 12);
        int target = i % 3;
        EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, action, target), 0);
    }

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
