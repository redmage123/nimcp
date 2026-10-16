/**
 * @file test_substrate_health_functions.cpp
 * @brief Unit tests for Phase 5.10 Neural Substrate Health Integration
 * @date 2026-01-19
 *
 * Tests the neural substrate health monitoring functionality
 * in the health agent.
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for neural substrate health tests
 */
class SubstrateHealthTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    neural_substrate_t* substrate = nullptr;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 100;
        config.message_queue_depth = 64;
        config.watchdog_timeout_ms = 5000;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);

        /* Create substrate with default config */
        substrate_config_t sub_cfg = {};
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
    }

    void TearDown() override {
        if (substrate) {
            if (agent) {
                nimcp_health_agent_disconnect_substrate(agent, substrate);
            }
            substrate_destroy(substrate);
            substrate = nullptr;
        }
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * Config Default Tests
 * ============================================================================ */

TEST_F(SubstrateHealthTest, ConfigDefaultSetsReasonableValues) {
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);

    /* Metabolic thresholds */
    EXPECT_FLOAT_EQ(config.atp_warning_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.atp_critical_threshold, 0.3f);
    EXPECT_FLOAT_EQ(config.oxygen_warning_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.oxygen_critical_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.glucose_warning_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.glucose_critical_threshold, 0.4f);

    /* Physical thresholds */
    EXPECT_FLOAT_EQ(config.hyperthermia_threshold, 40.0f);
    EXPECT_FLOAT_EQ(config.hypothermia_threshold, 32.0f);
    EXPECT_FLOAT_EQ(config.membrane_warning_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.membrane_critical_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.ion_warning_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.ion_critical_threshold, 0.5f);

    /* Performance thresholds */
    EXPECT_FLOAT_EQ(config.capacity_warning_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.capacity_critical_threshold, 0.3f);

    /* Enables */
    EXPECT_TRUE(config.enable_metabolic_monitoring);
    EXPECT_TRUE(config.enable_physical_monitoring);
    EXPECT_TRUE(config.enable_performance_monitoring);
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_energy_boost);
    EXPECT_TRUE(config.enable_temp_regulation);
    EXPECT_TRUE(config.enable_ion_correction);
    EXPECT_TRUE(config.enable_membrane_repair);

    /* Intervals */
    EXPECT_EQ(config.health_check_interval_ms, 50u);
}

TEST_F(SubstrateHealthTest, ConfigDefaultHandlesNull) {
    /* Should not crash */
    nimcp_health_agent_substrate_config_default(nullptr);
}

/* ============================================================================
 * Connect Tests
 * ============================================================================ */

TEST_F(SubstrateHealthTest, ConnectWithNullAgentFails) {
    EXPECT_EQ(nimcp_health_agent_connect_substrate(nullptr, substrate, nullptr), -1);
}

TEST_F(SubstrateHealthTest, ConnectWithNullSubstrateFails) {
    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, nullptr, nullptr), -1);
}

TEST_F(SubstrateHealthTest, ConnectWithValidSubstrateSucceeds) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }
    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

    substrate_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, 1u);
}

TEST_F(SubstrateHealthTest, ConnectWithCustomConfig) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);
    config.atp_warning_threshold = 0.6f;
    config.health_check_interval_ms = 100;

    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, &config), 0);
}

TEST_F(SubstrateHealthTest, ConnectSameSubstrateTwiceReturnsSafe) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);
    /* Second connect should succeed but not duplicate */
    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

    substrate_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, 1u);  /* Still only one */
}

TEST_F(SubstrateHealthTest, ConnectMultipleSubstrates) {
    /* Create second substrate */
    substrate_config_t cfg2 = {};
    substrate_default_config(&cfg2);
    neural_substrate_t* sub2 = substrate_create(&cfg2);

    if (!substrate || !sub2) {
        if (sub2) substrate_destroy(sub2);
        GTEST_SKIP() << "Neural substrates not available";
    }

    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, sub2, nullptr), 0);

    substrate_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, 2u);

    nimcp_health_agent_disconnect_substrate(agent, sub2);
    substrate_destroy(sub2);
}

/* ============================================================================
 * Disconnect Tests
 * ============================================================================ */

TEST_F(SubstrateHealthTest, DisconnectWithNullAgentFails) {
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(nullptr, substrate), -1);
}

TEST_F(SubstrateHealthTest, DisconnectWithNullSubstrateFails) {
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, nullptr), -1);
}

TEST_F(SubstrateHealthTest, DisconnectNotConnectedFails) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, substrate), -1);
}

TEST_F(SubstrateHealthTest, DisconnectConnectedSucceeds) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, substrate), 0);

    substrate_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, 0u);
}

/* ============================================================================
 * Get Metrics Tests
 * ============================================================================ */

TEST_F(SubstrateHealthTest, GetMetricsWithNullAgentFails) {
    substrate_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(nullptr, &metrics), -1);
}

TEST_F(SubstrateHealthTest, GetMetricsWithNullMetricsFails) {
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, nullptr), -1);
}

TEST_F(SubstrateHealthTest, GetMetricsNoSubstratesReturnsHealthy) {
    substrate_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);

    EXPECT_EQ(metrics.num_substrates, 0u);
    EXPECT_FLOAT_EQ(metrics.overall_substrate_health, 100.0f);
    EXPECT_GT(metrics.last_check_timestamp_us, 0u);
}

TEST_F(SubstrateHealthTest, GetMetricsWithConnectedSubstrate) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

    substrate_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);

    EXPECT_EQ(metrics.num_substrates, 1u);
    EXPECT_GE(metrics.overall_substrate_health, 0.0f);
    EXPECT_LE(metrics.overall_substrate_health, 100.0f);
    EXPECT_GT(metrics.last_check_timestamp_us, 0u);
}

/* ============================================================================
 * Get Health Score Tests
 * ============================================================================ */

TEST_F(SubstrateHealthTest, GetHealthScoreNullAgentReturnsHealthy) {
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_substrate_health_score(nullptr), 100.0f);
}

TEST_F(SubstrateHealthTest, GetHealthScoreNoSubstratesReturnsHealthy) {
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_substrate_health_score(agent), 100.0f);
}

TEST_F(SubstrateHealthTest, GetHealthScoreWithConnectedSubstrate) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

    float score = nimcp_health_agent_get_substrate_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

/* ============================================================================
 * Needs Attention Tests
 * ============================================================================ */

TEST_F(SubstrateHealthTest, NeedsAttentionNullAgentReturnsFalse) {
    EXPECT_FALSE(nimcp_health_agent_substrate_needs_attention(nullptr));
}

TEST_F(SubstrateHealthTest, NeedsAttentionNoSubstratesReturnsFalse) {
    EXPECT_FALSE(nimcp_health_agent_substrate_needs_attention(agent));
}

TEST_F(SubstrateHealthTest, NeedsAttentionHealthySubstrateReturnsFalse) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);
    /* Fresh substrate should be healthy */
    EXPECT_FALSE(nimcp_health_agent_substrate_needs_attention(agent));
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(SubstrateHealthTest, RecoveryNullAgentFails) {
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(nullptr, SUBSTRATE_RECOVERY_BOOST_ATP, 0), -1);
}

TEST_F(SubstrateHealthTest, RecoveryNoSubstratesFails) {
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BOOST_ATP, 0), -1);
}

TEST_F(SubstrateHealthTest, RecoveryInvalidIndexFails) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BOOST_ATP, 99), -1);
}

TEST_F(SubstrateHealthTest, RecoveryValidSubstrateSucceeds) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BOOST_ATP, 0), 0);
}

TEST_F(SubstrateHealthTest, RecoveryAllSubstrates) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);
    /* -1 means all substrates */
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_SOFT_RESET, -1), 0);
}

TEST_F(SubstrateHealthTest, AllRecoveryActions) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

    /* Test all recovery actions */
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_NONE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BOOST_ATP, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BOOST_OXYGEN, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BOOST_GLUCOSE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_COOL_DOWN, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_WARM_UP, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BALANCE_IONS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_REPAIR_MEMBRANE, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_REDUCE_ACTIVITY, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_RESET_STATS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_SOFT_RESET, 0), 0);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_FULL_RESET, 0), 0);
}

/* ============================================================================
 * Update Config Tests
 * ============================================================================ */

TEST_F(SubstrateHealthTest, UpdateConfigNullAgentFails) {
    health_agent_substrate_config_t config = {};
    EXPECT_EQ(nimcp_health_agent_update_substrate_config(nullptr, &config), -1);
}

TEST_F(SubstrateHealthTest, UpdateConfigNullConfigFails) {
    EXPECT_EQ(nimcp_health_agent_update_substrate_config(agent, nullptr), -1);
}

TEST_F(SubstrateHealthTest, UpdateConfigSucceeds) {
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);
    config.atp_warning_threshold = 0.6f;
    config.health_check_interval_ms = 200;

    EXPECT_EQ(nimcp_health_agent_update_substrate_config(agent, &config), 0);
}

/* ============================================================================
 * Full Workflow Test
 * ============================================================================ */

TEST_F(SubstrateHealthTest, FullWorkflow) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    /* Configure */
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);
    config.enable_auto_recovery = true;

    /* Connect */
    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, &config), 0);

    /* Get metrics */
    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_substrates, 1u);

    /* Check health */
    float score = nimcp_health_agent_get_substrate_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    /* Check if needs attention */
    bool needs = nimcp_health_agent_substrate_needs_attention(agent);
    (void)needs;

    /* Trigger recovery */
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(agent, SUBSTRATE_RECOVERY_BOOST_ATP, 0), 0);

    /* Update config */
    config.atp_warning_threshold = 0.6f;
    EXPECT_EQ(nimcp_health_agent_update_substrate_config(agent, &config), 0);

    /* Disconnect */
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, substrate), 0);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
