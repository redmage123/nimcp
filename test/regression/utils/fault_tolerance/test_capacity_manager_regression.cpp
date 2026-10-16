/**
 * @file test_capacity_manager_regression.cpp
 * @brief Regression tests for Phase 5.8 Dynamic Capacity Management
 * @date 2026-01-18
 *
 * Ensures backward compatibility and prevents regressions in
 * capacity management functionality.
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_capacity_manager.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for capacity manager regression tests
 */
class CapacityManagerRegressionTest : public ::testing::Test {
protected:
    capacity_manager_t* cm = nullptr;

    void SetUp() override {
        capacity_config_t config = {};
        capacity_config_default(&config);
        config.initial_capacity = 100;
        config.max_capacity = 500;

        int result = capacity_manager_create(&cm, &config, "regression_test");
        ASSERT_EQ(result, 0);
        ASSERT_NE(cm, nullptr);
    }

    void TearDown() override {
        if (cm) {
            capacity_manager_destroy(cm);
            cm = nullptr;
        }
    }
};

/* ============================================================================
 * API Stability Regression Tests
 * ============================================================================ */

TEST_F(CapacityManagerRegressionTest, ConfigDefaultAPIStable) {
    capacity_config_t config = {};
    capacity_config_default(&config);

    /* Verify specific default values haven't changed */
    EXPECT_EQ(config.initial_capacity, 512u);
    EXPECT_EQ(config.max_capacity, 0u);
    EXPECT_FLOAT_EQ(config.growth_factor, 2.0f);
    EXPECT_FLOAT_EQ(config.shrink_threshold, 0.25f);
    EXPECT_FLOAT_EQ(config.warning_threshold, 0.9f);
    EXPECT_FLOAT_EQ(config.elevated_threshold, 0.75f);
}

TEST_F(CapacityManagerRegressionTest, CreateAPIStable) {
    capacity_manager_t* test_cm = nullptr;
    capacity_config_t config = {};
    capacity_config_default(&config);

    /* API must accept NULL config (uses defaults) */
    EXPECT_EQ(capacity_manager_create(&test_cm, nullptr, "test"), 0);
    ASSERT_NE(test_cm, nullptr);
    capacity_manager_destroy(test_cm);

    /* API must accept valid config */
    EXPECT_EQ(capacity_manager_create(&test_cm, &config, "test"), 0);
    ASSERT_NE(test_cm, nullptr);
    capacity_manager_destroy(test_cm);
}

TEST_F(CapacityManagerRegressionTest, InitAPIStable) {
    capacity_manager_t local_cm;
    capacity_config_t config = {};
    capacity_config_default(&config);

    /* Init must work with NULL config */
    EXPECT_EQ(capacity_manager_init(&local_cm, nullptr, "test"), 0);

    /* Init must work with valid config */
    EXPECT_EQ(capacity_manager_init(&local_cm, &config, "test"), 0);
}

TEST_F(CapacityManagerRegressionTest, SlotRequestReleaseAPIStable) {
    /* Request must return 0 on success */
    EXPECT_EQ(capacity_manager_request_slot(cm), 0);

    /* Release must return 0 on success */
    EXPECT_EQ(capacity_manager_release_slot(cm), 0);

    /* Release on empty must return -1 */
    EXPECT_EQ(capacity_manager_release_slot(cm), -1);
}

TEST_F(CapacityManagerRegressionTest, ExpansionAPIStable) {
    /* Trigger expand must return 0 on success */
    EXPECT_EQ(capacity_manager_trigger_expand(cm), 0);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 200u);  /* 100 * 2.0 */
}

TEST_F(CapacityManagerRegressionTest, StatsAPIStable) {
    capacity_stats_t stats = {};
    capacity_manager_get_stats(cm, &stats);

    /* Verify all expected fields exist and have valid defaults */
    EXPECT_EQ(stats.current_count, 0u);
    EXPECT_EQ(stats.capacity, 100u);
    EXPECT_FLOAT_EQ(stats.utilization, 0.0f);
    EXPECT_EQ(stats.level, CAPACITY_LEVEL_NORMAL);
    EXPECT_EQ(stats.expansions, 0u);
    EXPECT_EQ(stats.shrinks, 0u);
    EXPECT_EQ(stats.cleanup_triggers, 0u);
    EXPECT_EQ(stats.failed_allocations, 0u);
}

/* ============================================================================
 * Data Structure Regression Tests
 * ============================================================================ */

TEST_F(CapacityManagerRegressionTest, CapacityConfigStructureComplete) {
    capacity_config_t config = {};
    capacity_config_default(&config);

    /* Verify all expected fields exist */
    (void)config.initial_capacity;
    (void)config.max_capacity;
    (void)config.growth_factor;
    (void)config.shrink_threshold;
    (void)config.warning_threshold;
    (void)config.elevated_threshold;
    (void)config.enable_auto_expand;
    (void)config.enable_auto_shrink;
    (void)config.enable_immune_cleanup;
    (void)config.enable_trend_analysis;
}

TEST_F(CapacityManagerRegressionTest, CapacityStatsStructureComplete) {
    capacity_stats_t stats = {};
    capacity_manager_get_stats(cm, &stats);

    /* Verify all expected fields exist */
    (void)stats.current_count;
    (void)stats.capacity;
    (void)stats.utilization;
    (void)stats.level;
    (void)stats.expansions;
    (void)stats.shrinks;
    (void)stats.cleanup_triggers;
    (void)stats.failed_allocations;
    (void)stats.peak_utilization;
    (void)stats.peak_count;
    (void)stats.growth_rate_per_sec;
    (void)stats.time_to_capacity_sec;
    (void)stats.growth_trend_valid;
    (void)stats.last_expansion_time;
    (void)stats.last_check_time;
}

/* ============================================================================
 * Enum Value Regression Tests
 * ============================================================================ */

TEST_F(CapacityManagerRegressionTest, CapacityLevelEnumValuesStable) {
    /* Verify enum values haven't changed */
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_NORMAL), 0);
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_ELEVATED), 1);
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_WARNING), 2);
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_CRITICAL), 3);
    EXPECT_EQ(static_cast<int>(CAPACITY_LEVEL_EXCEEDED), 4);
}

/* ============================================================================
 * Error Handling Regression Tests
 * ============================================================================ */

TEST_F(CapacityManagerRegressionTest, NullManagerHandling) {
    /* All functions must handle NULL manager gracefully */
    EXPECT_EQ(capacity_manager_request_slot(nullptr), -1);
    EXPECT_EQ(capacity_manager_release_slot(nullptr), -1);
    EXPECT_EQ(capacity_manager_trigger_expand(nullptr), -1);
    EXPECT_EQ(capacity_manager_trigger_shrink(nullptr), -1);
    EXPECT_EQ(capacity_manager_trigger_cleanup(nullptr, 10), -1);
    EXPECT_FLOAT_EQ(capacity_manager_get_utilization(nullptr), 0.0f);
    EXPECT_EQ(capacity_manager_get_level(nullptr), CAPACITY_LEVEL_EXCEEDED);
    EXPECT_TRUE(capacity_manager_is_full(nullptr));
    EXPECT_FLOAT_EQ(capacity_manager_time_to_capacity(nullptr), -1.0f);
}

TEST_F(CapacityManagerRegressionTest, NullParameterHandling) {
    /* Functions must handle NULL parameters gracefully */
    capacity_config_default(nullptr);  /* Should not crash */
    capacity_manager_get_stats(cm, nullptr);  /* Should not crash */
    capacity_manager_set_count(nullptr, 10);  /* Should not crash */
    capacity_manager_reset_stats(nullptr);  /* Should not crash */
}

TEST_F(CapacityManagerRegressionTest, InvalidMagicHandling) {
    /* Create a manager and corrupt its magic */
    capacity_manager_t local_cm;
    capacity_manager_init(&local_cm, nullptr, "test");

    /* Corrupt magic */
    local_cm.magic = 0xDEADBEEF;

    /* Operations should fail gracefully */
    EXPECT_EQ(capacity_manager_request_slot(&local_cm), -1);
    EXPECT_EQ(capacity_manager_release_slot(&local_cm), -1);
}

/* ============================================================================
 * Default Value Regression Tests
 * ============================================================================ */

TEST_F(CapacityManagerRegressionTest, DefaultThresholds) {
    capacity_config_t config = {};
    capacity_config_default(&config);

    /* These defaults should remain stable */
    EXPECT_FLOAT_EQ(config.growth_factor, 2.0f);
    EXPECT_FLOAT_EQ(config.shrink_threshold, 0.25f);
    EXPECT_FLOAT_EQ(config.warning_threshold, 0.9f);
    EXPECT_FLOAT_EQ(config.elevated_threshold, 0.75f);
}

TEST_F(CapacityManagerRegressionTest, DefaultBooleanFlags) {
    capacity_config_t config = {};
    capacity_config_default(&config);

    /* These defaults should remain stable */
    EXPECT_TRUE(config.enable_auto_expand);
    EXPECT_FALSE(config.enable_auto_shrink);
    EXPECT_TRUE(config.enable_immune_cleanup);
    EXPECT_TRUE(config.enable_trend_analysis);
}

/* ============================================================================
 * Backward Compatibility Tests
 * ============================================================================ */

TEST_F(CapacityManagerRegressionTest, LevelCalculationConsistent) {
    /* Fill to specific percentages and verify level */

    /* 0% = NORMAL */
    EXPECT_EQ(capacity_manager_get_level(cm), CAPACITY_LEVEL_NORMAL);

    /* 50% = NORMAL */
    for (int i = 0; i < 50; i++) capacity_manager_request_slot(cm);
    EXPECT_EQ(capacity_manager_get_level(cm), CAPACITY_LEVEL_NORMAL);

    /* 75% = ELEVATED */
    for (int i = 50; i < 75; i++) capacity_manager_request_slot(cm);
    EXPECT_EQ(capacity_manager_get_level(cm), CAPACITY_LEVEL_ELEVATED);

    /* 90% = WARNING */
    for (int i = 75; i < 90; i++) capacity_manager_request_slot(cm);
    EXPECT_EQ(capacity_manager_get_level(cm), CAPACITY_LEVEL_WARNING);

    /* 100% = CRITICAL */
    for (int i = 90; i < 100; i++) capacity_manager_request_slot(cm);
    EXPECT_EQ(capacity_manager_get_level(cm), CAPACITY_LEVEL_CRITICAL);
}

TEST_F(CapacityManagerRegressionTest, GrowthFactorBehavior) {
    /* Expansion should multiply by growth factor (2.0) */
    capacity_stats_t stats;

    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 100u);

    capacity_manager_trigger_expand(cm);
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 200u);

    capacity_manager_trigger_expand(cm);
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 400u);

    /* Should cap at max_capacity (500) */
    capacity_manager_trigger_expand(cm);
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.capacity, 500u);
}

TEST_F(CapacityManagerRegressionTest, UtilizationCalculation) {
    /* Utilization should be current_count / capacity */
    for (int i = 0; i < 25; i++) capacity_manager_request_slot(cm);
    EXPECT_FLOAT_EQ(capacity_manager_get_utilization(cm), 0.25f);

    for (int i = 25; i < 50; i++) capacity_manager_request_slot(cm);
    EXPECT_FLOAT_EQ(capacity_manager_get_utilization(cm), 0.5f);

    for (int i = 50; i < 75; i++) capacity_manager_request_slot(cm);
    EXPECT_FLOAT_EQ(capacity_manager_get_utilization(cm), 0.75f);

    for (int i = 75; i < 100; i++) capacity_manager_request_slot(cm);
    EXPECT_FLOAT_EQ(capacity_manager_get_utilization(cm), 1.0f);
}

/* ============================================================================
 * Health Agent Integration Regression Tests
 * ============================================================================ */

TEST_F(CapacityManagerRegressionTest, HealthAgentRegistrationAPIStable) {
    health_agent_config_t agent_config = {};
    agent_config.heartbeat_interval_ms = 100;
    agent_config.message_queue_depth = 64;
    agent_config.watchdog_timeout_ms = 5000;

    nimcp_health_agent_t* agent = nimcp_health_agent_create(&agent_config);
    ASSERT_NE(agent, nullptr);

    /* Registration should work */
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm), 0);

    /* Metrics should work */
    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 1u);

    /* Unregistration should work */
    EXPECT_EQ(nimcp_health_agent_unregister_capacity_manager(agent, cm), 0);

    nimcp_health_agent_destroy(agent);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
