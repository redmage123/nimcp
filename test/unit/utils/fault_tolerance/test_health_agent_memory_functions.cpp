/**
 * @file test_health_agent_memory_functions.cpp
 * @brief Unit tests for Phase 5.7 Memory System Health Integration
 * @date 2026-01-18
 *
 * Tests for hippocampus and mammillary health monitoring integration
 * with the health agent.
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for memory health integration tests
 */
class HealthAgentMemoryTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 1000;
        config.message_queue_depth = 64;
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = false;

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
 * Configuration Default Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryTest, HippocampusConfigDefault) {
    health_agent_hippocampus_config_t config = {};

    nimcp_health_agent_hippocampus_config_default(&config);

    EXPECT_FLOAT_EQ(config.ca3_stability_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.theta_gamma_min_coupling, 0.5f);
    EXPECT_FLOAT_EQ(config.episode_utilization_warning, 0.8f);
    EXPECT_FLOAT_EQ(config.episode_utilization_critical, 0.95f);
    EXPECT_FLOAT_EQ(config.theta_power_min, 0.3f);
    EXPECT_FLOAT_EQ(config.gamma_power_min, 0.2f);
    EXPECT_TRUE(config.monitor_oscillations);
    EXPECT_TRUE(config.monitor_pattern_separation);
    EXPECT_TRUE(config.monitor_pattern_completion);
    EXPECT_EQ(config.health_check_interval_ms, 1000u);
}

TEST_F(HealthAgentMemoryTest, HippocampusConfigDefaultNullSafe) {
    /* Should not crash with NULL */
    nimcp_health_agent_hippocampus_config_default(nullptr);
}

TEST_F(HealthAgentMemoryTest, MammillaryConfigDefault) {
    health_agent_mammillary_config_t config = {};

    nimcp_health_agent_mammillary_config_default(&config);

    EXPECT_FLOAT_EQ(config.relay_efficiency_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.hd_drift_max_degrees, 5.0f);
    EXPECT_FLOAT_EQ(config.fornix_strength_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.trace_utilization_warning, 0.8f);
    EXPECT_FLOAT_EQ(config.trace_utilization_critical, 0.95f);
    EXPECT_TRUE(config.monitor_papez_circuit);
    EXPECT_FLOAT_EQ(config.papez_integrity_threshold, 0.7f);
    EXPECT_TRUE(config.monitor_hd_cells);
    EXPECT_FLOAT_EQ(config.hd_coherence_threshold, 0.6f);
    EXPECT_EQ(config.health_check_interval_ms, 1000u);
}

TEST_F(HealthAgentMemoryTest, MammillaryConfigDefaultNullSafe) {
    /* Should not crash with NULL */
    nimcp_health_agent_mammillary_config_default(nullptr);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryTest, ConnectHippocampusNullAgent) {
    /* Fake hippocampus pointer for testing */
    nimcp_hippocampus_t* fake_hippo = reinterpret_cast<nimcp_hippocampus_t*>(0x1234);

    int result = nimcp_health_agent_connect_hippocampus(nullptr, fake_hippo, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentMemoryTest, ConnectHippocampusNullHippocampus) {
    int result = nimcp_health_agent_connect_hippocampus(agent, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentMemoryTest, ConnectMammillaryNullAgent) {
    /* Fake mammillary pointer for testing */
    nimcp_mammillary_t* fake_mammillary = reinterpret_cast<nimcp_mammillary_t*>(0x5678);

    int result = nimcp_health_agent_connect_mammillary(nullptr, fake_mammillary, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentMemoryTest, ConnectMammillaryNullMammillary) {
    int result = nimcp_health_agent_connect_mammillary(agent, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Metrics Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryTest, GetMemoryMetricsNullAgent) {
    memory_health_metrics_t metrics = {};

    int result = nimcp_health_agent_get_memory_metrics(nullptr, &metrics);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentMemoryTest, GetMemoryMetricsNullMetrics) {
    int result = nimcp_health_agent_get_memory_metrics(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentMemoryTest, GetMemoryMetricsNoConnections) {
    memory_health_metrics_t metrics = {};

    int result = nimcp_health_agent_get_memory_metrics(agent, &metrics);
    EXPECT_EQ(result, 0);

    /* With no connections, should report perfect health */
    EXPECT_FLOAT_EQ(metrics.overall_memory_health, 1.0f);

    /* Hippocampus should be zeroed */
    EXPECT_FLOAT_EQ(metrics.hippocampus.overall_health, 0.0f);

    /* Mammillary should be zeroed */
    EXPECT_FLOAT_EQ(metrics.mammillary.overall_health, 0.0f);

    /* Timestamp should be set */
    EXPECT_GT(metrics.last_check_timestamp, 0u);
}

/* ============================================================================
 * Consistency Validation Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryTest, ValidateConsistencyNullAgent) {
    int result = nimcp_health_agent_validate_memory_consistency(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentMemoryTest, ValidateConsistencyNoConnections) {
    int result = nimcp_health_agent_validate_memory_consistency(agent);
    EXPECT_EQ(result, 0);  /* No inconsistencies when nothing connected */
}

/* ============================================================================
 * Recovery Action Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryTest, MemoryRecoveryNullAgent) {
    int result = nimcp_health_agent_memory_recovery(nullptr, MEMORY_RECOVERY_RESET_CA3, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(HealthAgentMemoryTest, MemoryRecoveryResetCA3) {
    int result = nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_RESET_CA3, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentMemoryTest, MemoryRecoveryHDDriftCorrect) {
    int result = nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_HD_DRIFT_CORRECT, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentMemoryTest, MemoryRecoveryForceConsolidation) {
    int result = nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_FORCE_CONSOLIDATION, 2);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentMemoryTest, MemoryRecoveryEmergencySave) {
    int result = nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_EMERGENCY_SAVE, 2);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentMemoryTest, MemoryRecoveryAllActions) {
    /* Test all recovery actions to ensure they don't crash */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_NONE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_STABILIZE_RHYTHMS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_FORNIX_STRENGTHEN, 1), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_PAPEZ_REPAIR, 2), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_EXPAND_CAPACITY, 2), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_GC_OLD_TRACES, 2), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_CROSS_TIER_SYNC, 2), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_METABOLIC_BOOST, 2), 0);
}

/* ============================================================================
 * Needs Attention Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryTest, NeedsAttentionNullAgent) {
    bool result = nimcp_health_agent_memory_needs_attention(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(HealthAgentMemoryTest, NeedsAttentionNoConnections) {
    bool result = nimcp_health_agent_memory_needs_attention(agent);
    EXPECT_FALSE(result);  /* No connections = nothing needs attention */
}

/* ============================================================================
 * Recovery Action Enum Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryTest, RecoveryActionEnumValues) {
    /* Verify enum values are distinct */
    EXPECT_NE(MEMORY_RECOVERY_RESET_CA3, MEMORY_RECOVERY_NONE);
    EXPECT_NE(MEMORY_RECOVERY_STABILIZE_RHYTHMS, MEMORY_RECOVERY_RESET_CA3);
    EXPECT_NE(MEMORY_RECOVERY_HD_DRIFT_CORRECT, MEMORY_RECOVERY_STABILIZE_RHYTHMS);
    EXPECT_NE(MEMORY_RECOVERY_FORNIX_STRENGTHEN, MEMORY_RECOVERY_HD_DRIFT_CORRECT);
    EXPECT_NE(MEMORY_RECOVERY_FORCE_CONSOLIDATION, MEMORY_RECOVERY_FORNIX_STRENGTHEN);
    EXPECT_NE(MEMORY_RECOVERY_PAPEZ_REPAIR, MEMORY_RECOVERY_FORCE_CONSOLIDATION);
    EXPECT_NE(MEMORY_RECOVERY_EXPAND_CAPACITY, MEMORY_RECOVERY_PAPEZ_REPAIR);
    EXPECT_NE(MEMORY_RECOVERY_GC_OLD_TRACES, MEMORY_RECOVERY_EXPAND_CAPACITY);
    EXPECT_NE(MEMORY_RECOVERY_CROSS_TIER_SYNC, MEMORY_RECOVERY_GC_OLD_TRACES);
    EXPECT_NE(MEMORY_RECOVERY_METABOLIC_BOOST, MEMORY_RECOVERY_CROSS_TIER_SYNC);
    EXPECT_NE(MEMORY_RECOVERY_EMERGENCY_SAVE, MEMORY_RECOVERY_METABOLIC_BOOST);
}

/* ============================================================================
 * Metrics Structure Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryTest, MemoryMetricsStructureSize) {
    /* Verify struct is reasonably sized */
    EXPECT_GT(sizeof(memory_health_metrics_t), 0u);
    EXPECT_LT(sizeof(memory_health_metrics_t), 4096u);  /* Should be under 4KB */
}

TEST_F(HealthAgentMemoryTest, HippocampusConfigStructureSize) {
    EXPECT_GT(sizeof(health_agent_hippocampus_config_t), 0u);
    EXPECT_LT(sizeof(health_agent_hippocampus_config_t), 256u);
}

TEST_F(HealthAgentMemoryTest, MammillaryConfigStructureSize) {
    EXPECT_GT(sizeof(health_agent_mammillary_config_t), 0u);
    EXPECT_LT(sizeof(health_agent_mammillary_config_t), 256u);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
