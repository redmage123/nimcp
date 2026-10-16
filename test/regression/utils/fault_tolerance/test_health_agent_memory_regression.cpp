/**
 * @file test_health_agent_memory_regression.cpp
 * @brief Regression tests for Phase 5.7 Memory System Health Integration
 * @date 2026-01-18
 *
 * Ensures backward compatibility and prevents regressions in memory
 * health integration functionality.
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for memory health regression tests
 */
class HealthAgentMemoryRegressionTest : public ::testing::Test {
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
 * API Stability Regression Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryRegressionTest, ConfigDefaultAPIStable) {
    /* Config default functions must remain callable */
    health_agent_hippocampus_config_t hippo_config = {};
    health_agent_mammillary_config_t mammillary_config = {};

    nimcp_health_agent_hippocampus_config_default(&hippo_config);
    nimcp_health_agent_mammillary_config_default(&mammillary_config);

    /* Verify specific default values haven't changed */
    EXPECT_FLOAT_EQ(hippo_config.ca3_stability_threshold, 0.7f);
    EXPECT_FLOAT_EQ(mammillary_config.relay_efficiency_threshold, 0.7f);
}

TEST_F(HealthAgentMemoryRegressionTest, ConnectionAPIStable) {
    /* Connection functions must accept NULL for config */
    /* (They should fail gracefully when hippocampus/mammillary is NULL) */
    EXPECT_EQ(nimcp_health_agent_connect_hippocampus(agent, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_mammillary(agent, nullptr, nullptr), -1);
}

TEST_F(HealthAgentMemoryRegressionTest, MetricsAPIStable) {
    memory_health_metrics_t metrics = {};

    /* API must work with valid agent */
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);

    /* Verify metrics structure fields exist */
    EXPECT_GE(metrics.overall_memory_health, 0.0f);
    EXPECT_LE(metrics.overall_memory_health, 1.0f);
}

TEST_F(HealthAgentMemoryRegressionTest, ConsistencyAPIStable) {
    /* Validation must return valid result */
    int result = nimcp_health_agent_validate_memory_consistency(agent);
    EXPECT_GE(result, -1);  /* -1 for error, 0+ for inconsistency count */
}

TEST_F(HealthAgentMemoryRegressionTest, RecoveryAPIStable) {
    /* All recovery actions must be callable */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_NONE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_EMERGENCY_SAVE, 2), 0);
}

TEST_F(HealthAgentMemoryRegressionTest, NeedsAttentionAPIStable) {
    /* Must return boolean */
    bool result = nimcp_health_agent_memory_needs_attention(agent);
    EXPECT_FALSE(result);  /* No modules connected = no attention needed */
}

/* ============================================================================
 * Data Structure Regression Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryRegressionTest, MemoryMetricsStructureComplete) {
    memory_health_metrics_t metrics = {};
    nimcp_health_agent_get_memory_metrics(agent, &metrics);

    /* Verify all expected substructures exist */
    /* Hippocampus */
    (void)metrics.hippocampus.overall_health;
    (void)metrics.hippocampus.ca3_stability;
    (void)metrics.hippocampus.theta_gamma_coupling;
    (void)metrics.hippocampus.episode_utilization;
    (void)metrics.hippocampus.rhythm_disrupted;

    /* Mammillary */
    (void)metrics.mammillary.overall_health;
    (void)metrics.mammillary.relay_efficiency;
    (void)metrics.mammillary.hd_cell_coherence;
    (void)metrics.mammillary.hd_drift_rate;
    (void)metrics.mammillary.fornix_strength;

    /* Cross-tier */
    (void)metrics.cross_tier.hippo_to_mammillary_sync;
    (void)metrics.cross_tier.overall_circuit_integrity;
    (void)metrics.cross_tier.tier_mismatch_detected;

    /* Metabolic */
    (void)metrics.metabolic.hippocampus_atp_level;
    (void)metrics.metabolic.metabolic_stress;
    (void)metrics.metabolic.energy_constrained;
}

TEST_F(HealthAgentMemoryRegressionTest, HippocampusConfigStructureComplete) {
    health_agent_hippocampus_config_t config = {};
    nimcp_health_agent_hippocampus_config_default(&config);

    /* Verify all expected fields exist */
    (void)config.ca3_stability_threshold;
    (void)config.theta_gamma_min_coupling;
    (void)config.episode_utilization_warning;
    (void)config.episode_utilization_critical;
    (void)config.theta_power_min;
    (void)config.gamma_power_min;
    (void)config.monitor_oscillations;
    (void)config.monitor_pattern_separation;
    (void)config.monitor_pattern_completion;
    (void)config.health_check_interval_ms;
}

TEST_F(HealthAgentMemoryRegressionTest, MammillaryConfigStructureComplete) {
    health_agent_mammillary_config_t config = {};
    nimcp_health_agent_mammillary_config_default(&config);

    /* Verify all expected fields exist */
    (void)config.relay_efficiency_threshold;
    (void)config.hd_drift_max_degrees;
    (void)config.fornix_strength_threshold;
    (void)config.trace_utilization_warning;
    (void)config.trace_utilization_critical;
    (void)config.monitor_papez_circuit;
    (void)config.papez_integrity_threshold;
    (void)config.monitor_hd_cells;
    (void)config.hd_coherence_threshold;
    (void)config.health_check_interval_ms;
}

/* ============================================================================
 * Enum Value Regression Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryRegressionTest, RecoveryActionEnumValuesStable) {
    /* Verify enum values haven't changed */
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_NONE), 0);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_RESET_CA3), 1);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_STABILIZE_RHYTHMS), 2);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_HD_DRIFT_CORRECT), 3);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_FORNIX_STRENGTHEN), 4);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_FORCE_CONSOLIDATION), 5);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_PAPEZ_REPAIR), 6);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_EXPAND_CAPACITY), 7);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_GC_OLD_TRACES), 8);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_CROSS_TIER_SYNC), 9);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_METABOLIC_BOOST), 10);
    EXPECT_EQ(static_cast<int>(MEMORY_RECOVERY_EMERGENCY_SAVE), 11);
}

/* ============================================================================
 * Error Handling Regression Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryRegressionTest, NullAgentHandling) {
    /* All functions must handle NULL agent gracefully */
    EXPECT_EQ(nimcp_health_agent_connect_hippocampus(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_mammillary(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_validate_memory_consistency(nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(nullptr, MEMORY_RECOVERY_NONE, 0), -1);
    EXPECT_FALSE(nimcp_health_agent_memory_needs_attention(nullptr));
}

TEST_F(HealthAgentMemoryRegressionTest, NullParameterHandling) {
    /* Functions must handle NULL parameters gracefully */
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, nullptr), -1);
    nimcp_health_agent_hippocampus_config_default(nullptr);  /* Should not crash */
    nimcp_health_agent_mammillary_config_default(nullptr);   /* Should not crash */
}

/* ============================================================================
 * Default Value Regression Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryRegressionTest, HippocampusDefaultThresholds) {
    health_agent_hippocampus_config_t config = {};
    nimcp_health_agent_hippocampus_config_default(&config);

    /* These defaults should remain stable */
    EXPECT_FLOAT_EQ(config.ca3_stability_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.theta_gamma_min_coupling, 0.5f);
    EXPECT_FLOAT_EQ(config.episode_utilization_warning, 0.8f);
    EXPECT_FLOAT_EQ(config.episode_utilization_critical, 0.95f);
}

TEST_F(HealthAgentMemoryRegressionTest, MammillaryDefaultThresholds) {
    health_agent_mammillary_config_t config = {};
    nimcp_health_agent_mammillary_config_default(&config);

    /* These defaults should remain stable */
    EXPECT_FLOAT_EQ(config.relay_efficiency_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.hd_drift_max_degrees, 5.0f);
    EXPECT_FLOAT_EQ(config.fornix_strength_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.trace_utilization_warning, 0.8f);
}

/* ============================================================================
 * Backward Compatibility Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryRegressionTest, ExistingAgentFunctionalityPreserved) {
    /* Verify that adding memory health doesn't break existing agent functionality */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Agent should still work */
    health_agent_stats_t stats = {};
    nimcp_health_agent_get_stats(agent, &stats);

    /* Stop should still work */
    nimcp_health_agent_stop(agent);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
