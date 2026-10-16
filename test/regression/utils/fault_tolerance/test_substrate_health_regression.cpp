/**
 * @file test_substrate_health_regression.cpp
 * @brief Regression tests for Phase 5.10 Neural Substrate Health Integration
 * @date 2026-01-19
 *
 * Ensures backward compatibility and prevents regressions in
 * neural substrate health monitoring functionality.
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for neural substrate health regression tests
 */
class SubstrateHealthRegressionTest : public ::testing::Test {
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
 * API Stability Regression Tests
 * ============================================================================ */

TEST_F(SubstrateHealthRegressionTest, ConfigDefaultAPIStable) {
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);

    /* Verify specific default values haven't changed */
    EXPECT_FLOAT_EQ(config.atp_warning_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.atp_critical_threshold, 0.3f);
    EXPECT_FLOAT_EQ(config.oxygen_warning_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.oxygen_critical_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.glucose_warning_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.glucose_critical_threshold, 0.4f);
    EXPECT_FLOAT_EQ(config.hyperthermia_threshold, 40.0f);
    EXPECT_FLOAT_EQ(config.hypothermia_threshold, 32.0f);
    EXPECT_FLOAT_EQ(config.membrane_warning_threshold, 0.7f);
    EXPECT_FLOAT_EQ(config.membrane_critical_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.ion_warning_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.ion_critical_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.capacity_warning_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.capacity_critical_threshold, 0.3f);
    EXPECT_EQ(config.health_check_interval_ms, 50u);
}

TEST_F(SubstrateHealthRegressionTest, ConnectAPIStable) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    /* API must accept NULL config (uses defaults) */
    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, substrate), 0);

    /* API must accept valid config */
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);
    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, &config), 0);
}

TEST_F(SubstrateHealthRegressionTest, DisconnectAPIStable) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

    /* Disconnect must return 0 on success */
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, substrate), 0);

    /* Disconnect again must return -1 */
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, substrate), -1);
}

TEST_F(SubstrateHealthRegressionTest, GetMetricsAPIStable) {
    substrate_health_metrics_t metrics = {};

    /* Must succeed with no substrates */
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);

    if (substrate) {
        ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

        /* Must succeed with substrate */
        EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_substrates, 1u);
    }
}

TEST_F(SubstrateHealthRegressionTest, GetHealthScoreAPIStable) {
    /* Must return 100.0f with no substrates */
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_substrate_health_score(agent), 100.0f);

    if (substrate) {
        ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

        /* Must return value in [0, 100] with substrate */
        float score = nimcp_health_agent_get_substrate_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }
}

TEST_F(SubstrateHealthRegressionTest, NeedsAttentionAPIStable) {
    /* Must return false with no substrates */
    EXPECT_FALSE(nimcp_health_agent_substrate_needs_attention(agent));

    if (substrate) {
        ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

        /* Must not crash with substrate */
        (void)nimcp_health_agent_substrate_needs_attention(agent);
    }
}

TEST_F(SubstrateHealthRegressionTest, RecoveryAPIStable) {
    if (!substrate) {
        GTEST_SKIP() << "Neural substrate not available";
    }

    ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

    /* All recovery actions must return 0 on success */
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

TEST_F(SubstrateHealthRegressionTest, UpdateConfigAPIStable) {
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);
    config.atp_warning_threshold = 0.6f;

    /* Must succeed */
    EXPECT_EQ(nimcp_health_agent_update_substrate_config(agent, &config), 0);
}

/* ============================================================================
 * Data Structure Regression Tests
 * ============================================================================ */

TEST_F(SubstrateHealthRegressionTest, ConfigStructureComplete) {
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);

    /* Verify all expected fields exist */
    (void)config.enable_metabolic_monitoring;
    (void)config.atp_warning_threshold;
    (void)config.atp_critical_threshold;
    (void)config.oxygen_warning_threshold;
    (void)config.oxygen_critical_threshold;
    (void)config.glucose_warning_threshold;
    (void)config.glucose_critical_threshold;
    (void)config.enable_physical_monitoring;
    (void)config.hyperthermia_threshold;
    (void)config.hypothermia_threshold;
    (void)config.membrane_warning_threshold;
    (void)config.membrane_critical_threshold;
    (void)config.ion_warning_threshold;
    (void)config.ion_critical_threshold;
    (void)config.enable_performance_monitoring;
    (void)config.capacity_warning_threshold;
    (void)config.capacity_critical_threshold;
    (void)config.enable_auto_recovery;
    (void)config.enable_energy_boost;
    (void)config.enable_temp_regulation;
    (void)config.enable_ion_correction;
    (void)config.enable_membrane_repair;
    (void)config.health_check_interval_ms;
}

TEST_F(SubstrateHealthRegressionTest, MetricsStructureComplete) {
    substrate_health_metrics_t metrics = {};
    nimcp_health_agent_get_substrate_metrics(agent, &metrics);

    /* Verify all expected fields exist */
    (void)metrics.num_substrates;
    (void)metrics.any_substrate_unhealthy;
    (void)metrics.avg_atp_level;
    (void)metrics.min_atp_level;
    (void)metrics.avg_oxygen_saturation;
    (void)metrics.min_oxygen_saturation;
    (void)metrics.avg_glucose_level;
    (void)metrics.min_glucose_level;
    (void)metrics.avg_metabolic_capacity;
    (void)metrics.metabolic_crisis;
    (void)metrics.avg_temperature;
    (void)metrics.max_temperature;
    (void)metrics.min_temperature;
    (void)metrics.avg_membrane_integrity;
    (void)metrics.min_membrane_integrity;
    (void)metrics.avg_ion_balance;
    (void)metrics.min_ion_balance;
    (void)metrics.avg_physical_capacity;
    (void)metrics.physical_crisis;
    (void)metrics.avg_firing_rate_mod;
    (void)metrics.avg_transmission_efficiency;
    (void)metrics.avg_conduction_velocity;
    (void)metrics.avg_plasticity_capacity;
    (void)metrics.avg_overall_capacity;
    (void)metrics.total_alerts;
    (void)metrics.low_atp_alerts;
    (void)metrics.hypoxia_alerts;
    (void)metrics.hypoglycemia_alerts;
    (void)metrics.hyperthermia_alerts;
    (void)metrics.hypothermia_alerts;
    (void)metrics.ion_imbalance_alerts;
    (void)metrics.membrane_damage_alerts;
    (void)metrics.total_spikes_processed;
    (void)metrics.total_transmissions;
    (void)metrics.total_atp_consumed;
    (void)metrics.peak_metabolic_rate;
    (void)metrics.overall_substrate_health;
    (void)metrics.total_critical_events;
    (void)metrics.total_recoveries;
    (void)metrics.last_check_timestamp_us;
}

/* ============================================================================
 * Enum Value Regression Tests
 * ============================================================================ */

TEST_F(SubstrateHealthRegressionTest, RecoveryActionEnumValuesStable) {
    /* Verify enum values haven't changed */
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_NONE), 0);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_BOOST_ATP), 1);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_BOOST_OXYGEN), 2);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_BOOST_GLUCOSE), 3);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_COOL_DOWN), 4);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_WARM_UP), 5);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_BALANCE_IONS), 6);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_REPAIR_MEMBRANE), 7);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_REDUCE_ACTIVITY), 8);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_RESET_STATS), 9);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_SOFT_RESET), 10);
    EXPECT_EQ(static_cast<int>(SUBSTRATE_RECOVERY_FULL_RESET), 11);
}

/* ============================================================================
 * Error Handling Regression Tests
 * ============================================================================ */

TEST_F(SubstrateHealthRegressionTest, NullAgentHandling) {
    /* All functions must handle NULL agent gracefully */
    EXPECT_EQ(nimcp_health_agent_connect_substrate(nullptr, substrate, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(nullptr, substrate), -1);
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_substrate_recovery(nullptr, SUBSTRATE_RECOVERY_NONE, 0), -1);
    EXPECT_FALSE(nimcp_health_agent_substrate_needs_attention(nullptr));
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_substrate_health_score(nullptr), 100.0f);
    EXPECT_EQ(nimcp_health_agent_update_substrate_config(nullptr, nullptr), -1);
}

TEST_F(SubstrateHealthRegressionTest, NullParameterHandling) {
    /* Functions must handle NULL parameters gracefully */
    nimcp_health_agent_substrate_config_default(nullptr);  /* Should not crash */
    EXPECT_EQ(nimcp_health_agent_connect_substrate(agent, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_disconnect_substrate(agent, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_substrate_metrics(agent, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_update_substrate_config(agent, nullptr), -1);
}

/* ============================================================================
 * Default Value Regression Tests
 * ============================================================================ */

TEST_F(SubstrateHealthRegressionTest, DefaultBooleanFlags) {
    health_agent_substrate_config_t config = {};
    nimcp_health_agent_substrate_config_default(&config);

    /* These defaults should remain stable */
    EXPECT_TRUE(config.enable_metabolic_monitoring);
    EXPECT_TRUE(config.enable_physical_monitoring);
    EXPECT_TRUE(config.enable_performance_monitoring);
    EXPECT_TRUE(config.enable_auto_recovery);
    EXPECT_TRUE(config.enable_energy_boost);
    EXPECT_TRUE(config.enable_temp_regulation);
    EXPECT_TRUE(config.enable_ion_correction);
    EXPECT_TRUE(config.enable_membrane_repair);
}

/* ============================================================================
 * Backward Compatibility Tests
 * ============================================================================ */

TEST_F(SubstrateHealthRegressionTest, HealthScoreRangeConsistent) {
    /* Health score should always be in [0, 100] */

    /* No substrates case */
    float score = nimcp_health_agent_get_substrate_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    if (substrate) {
        ASSERT_EQ(nimcp_health_agent_connect_substrate(agent, substrate, nullptr), 0);

        /* With substrate */
        score = nimcp_health_agent_get_substrate_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }
}

TEST_F(SubstrateHealthRegressionTest, MetricsTimestampConsistent) {
    substrate_health_metrics_t metrics = {};
    ASSERT_EQ(nimcp_health_agent_get_substrate_metrics(agent, &metrics), 0);

    /* Timestamp should always be set */
    EXPECT_GT(metrics.last_check_timestamp_us, 0u);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
