/**
 * @file test_capacity_manager_integration.cpp
 * @brief Integration tests for Phase 5.8 Dynamic Capacity Management
 * @date 2026-01-18
 *
 * Tests integration between capacity manager and health agent.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/fault_tolerance/nimcp_capacity_manager.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for capacity manager integration tests
 */
class CapacityManagerIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    capacity_manager_t* cm1 = nullptr;
    capacity_manager_t* cm2 = nullptr;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 100;
        config.message_queue_depth = 64;
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = true;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);

        /* Create capacity managers */
        capacity_config_t cm_config = {};
        capacity_config_default(&cm_config);
        cm_config.initial_capacity = 100;
        cm_config.max_capacity = 1000;

        ASSERT_EQ(capacity_manager_create(&cm1, &cm_config, "test_module_1"), 0);

        cm_config.initial_capacity = 200;
        ASSERT_EQ(capacity_manager_create(&cm2, &cm_config, "test_module_2"), 0);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_stop(agent);
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
        if (cm1) {
            capacity_manager_destroy(cm1);
            cm1 = nullptr;
        }
        if (cm2) {
            capacity_manager_destroy(cm2);
            cm2 = nullptr;
        }
    }
};

/* ============================================================================
 * Registration Tests
 * ============================================================================ */

TEST_F(CapacityManagerIntegrationTest, RegisterSingleManager) {
    int result = nimcp_health_agent_register_capacity_manager(agent, cm1);
    EXPECT_EQ(result, 0);

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 1u);
}

TEST_F(CapacityManagerIntegrationTest, RegisterMultipleManagers) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm2), 0);

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 2u);
}

TEST_F(CapacityManagerIntegrationTest, RegisterDuplicate) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);  /* Should succeed but not duplicate */

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 1u);
}

TEST_F(CapacityManagerIntegrationTest, RegisterNullManager) {
    int result = nimcp_health_agent_register_capacity_manager(agent, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(CapacityManagerIntegrationTest, RegisterNullAgent) {
    int result = nimcp_health_agent_register_capacity_manager(nullptr, cm1);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Unregistration Tests
 * ============================================================================ */

TEST_F(CapacityManagerIntegrationTest, UnregisterManager) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm2), 0);

    EXPECT_EQ(nimcp_health_agent_unregister_capacity_manager(agent, cm1), 0);

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 1u);
}

TEST_F(CapacityManagerIntegrationTest, UnregisterNotRegistered) {
    int result = nimcp_health_agent_unregister_capacity_manager(agent, cm1);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Metrics Tests
 * ============================================================================ */

TEST_F(CapacityManagerIntegrationTest, MetricsWithNoManagers) {
    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);

    EXPECT_EQ(metrics.num_managers, 0u);
    EXPECT_EQ(metrics.managers_at_warning, 0u);
    EXPECT_EQ(metrics.managers_at_critical, 0u);
    EXPECT_FALSE(metrics.any_at_capacity);
}

TEST_F(CapacityManagerIntegrationTest, MetricsWithHealthyManagers) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm2), 0);

    /* Fill to 50% */
    for (int i = 0; i < 50; i++) {
        capacity_manager_request_slot(cm1);
    }
    for (int i = 0; i < 100; i++) {
        capacity_manager_request_slot(cm2);
    }

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);

    EXPECT_EQ(metrics.num_managers, 2u);
    EXPECT_EQ(metrics.managers_at_warning, 0u);
    EXPECT_EQ(metrics.managers_at_critical, 0u);
    EXPECT_FALSE(metrics.any_at_capacity);
    EXPECT_FLOAT_EQ(metrics.overall_pressure, 0.5f);  /* (50/100 + 100/200) / 2 */
}

TEST_F(CapacityManagerIntegrationTest, MetricsWithWarningLevel) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);

    /* Fill to 95% */
    for (int i = 0; i < 95; i++) {
        capacity_manager_request_slot(cm1);
    }

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);

    EXPECT_EQ(metrics.managers_at_warning, 1u);
}

TEST_F(CapacityManagerIntegrationTest, MetricsWithCriticalLevel) {
    /* Create a manager that won't auto-expand */
    capacity_config_t config = {};
    capacity_config_default(&config);
    config.initial_capacity = 50;
    config.max_capacity = 50;  /* Cannot expand */
    config.enable_auto_expand = false;

    capacity_manager_t* cm_critical = nullptr;
    ASSERT_EQ(capacity_manager_create(&cm_critical, &config, "critical_module"), 0);

    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm_critical), 0);

    /* Fill to capacity */
    for (int i = 0; i < 50; i++) {
        capacity_manager_request_slot(cm_critical);
    }

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);

    EXPECT_EQ(metrics.managers_at_critical, 1u);
    EXPECT_TRUE(metrics.any_at_capacity);
    EXPECT_NE(metrics.critical_module, nullptr);

    nimcp_health_agent_unregister_capacity_manager(agent, cm_critical);
    capacity_manager_destroy(cm_critical);
}

/* ============================================================================
 * Needs Attention Tests
 * ============================================================================ */

TEST_F(CapacityManagerIntegrationTest, NeedsAttentionNoManagers) {
    bool result = nimcp_health_agent_capacity_needs_attention(agent);
    EXPECT_FALSE(result);
}

TEST_F(CapacityManagerIntegrationTest, NeedsAttentionHealthy) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);

    for (int i = 0; i < 50; i++) {
        capacity_manager_request_slot(cm1);
    }

    bool result = nimcp_health_agent_capacity_needs_attention(agent);
    EXPECT_FALSE(result);
}

TEST_F(CapacityManagerIntegrationTest, NeedsAttentionAtWarning) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);

    for (int i = 0; i < 95; i++) {
        capacity_manager_request_slot(cm1);
    }

    bool result = nimcp_health_agent_capacity_needs_attention(agent);
    EXPECT_TRUE(result);
}

/* ============================================================================
 * Running Agent Tests
 * ============================================================================ */

TEST_F(CapacityManagerIntegrationTest, MetricsWhileAgentRunning) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (int i = 0; i < 50; i++) {
        capacity_manager_request_slot(cm1);
    }

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 1u);

    nimcp_health_agent_stop(agent);
}

TEST_F(CapacityManagerIntegrationTest, RegisterWhileAgentRunning) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm1), 0);

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 1u);

    nimcp_health_agent_stop(agent);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
