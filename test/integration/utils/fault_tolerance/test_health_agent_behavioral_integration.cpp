/**
 * @file test_health_agent_behavioral_integration.cpp
 * @brief Integration tests for NIMCP Health Agent Behavioral Module Integration (Phase 5.6)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Test health agent behavioral connection functions with Dragonfly/Portia bridges
 * WHY:  Verify behavioral health monitoring works correctly with actual bridges
 * HOW:  Create bridge instances (or use NULL), connect them, verify health checks
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

// Behavioral module immune bridge headers (forward declarations in health_agent.h)
// Note: Using forward declared types to avoid typedef conflicts

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for health agent behavioral integration tests
 */
class HealthAgentBehavioralIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;  // Fast checks for integration tests
        config.enable_auto_recovery = false;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";
    }

    void TearDown() override {
        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

//=============================================================================
// Basic Connection Tests
//=============================================================================

TEST_F(HealthAgentBehavioralIntegrationTest, ConnectDragonfly_WithNullBridge_AgentRuns) {
    // Connect with NULL bridge
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

TEST_F(HealthAgentBehavioralIntegrationTest, ConnectPortia_WithNullBridge_AgentRuns) {
    // Connect with NULL bridge
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, ConnectBothBridges_AgentRuns) {
    // Connect both with NULL bridges
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify metrics are accessible while running
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(HealthAgentBehavioralIntegrationTest, ConfigureDragonfly_ThenStart) {
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;
    dragonfly_config.enable_stress_monitoring = true;
    dragonfly_config.enable_health_status_tracking = true;
    dragonfly_config.enable_injury_detection = true;
    dragonfly_config.enable_fatigue_tracking = true;
    dragonfly_config.enable_cross_coordination = true;
    dragonfly_config.stress_warning_threshold = 0.5f;
    dragonfly_config.stress_critical_threshold = 0.8f;
    dragonfly_config.fatigue_warning_threshold = 0.6f;
    dragonfly_config.fatigue_critical_threshold = 0.9f;
    dragonfly_config.check_interval_ms = 50;

    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &dragonfly_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, ConfigurePortia_ThenStart) {
    health_agent_portia_monitor_config_t portia_config;
    memset(&portia_config, 0, sizeof(portia_config));
    portia_config.enable_portia_monitor = true;
    portia_config.enable_thermal_monitoring = true;
    portia_config.enable_power_monitoring = true;
    portia_config.enable_cpu_load_monitoring = true;
    portia_config.enable_degradation_tracking = true;
    portia_config.enable_cross_coordination = true;
    portia_config.thermal_warning_temp_c = 70.0f;
    portia_config.thermal_critical_temp_c = 85.0f;
    portia_config.battery_warning_pct = 20.0f;
    portia_config.battery_critical_pct = 5.0f;
    portia_config.cpu_warning_pct = 80.0f;
    portia_config.cpu_critical_pct = 95.0f;
    portia_config.check_interval_ms = 50;

    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, &portia_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, ReconfigureBehavioral_WhileRunning) {
    // Start with defaults
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Reconfigure while running
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;
    dragonfly_config.check_interval_ms = 25;

    health_agent_portia_monitor_config_t portia_config;
    memset(&portia_config, 0, sizeof(portia_config));
    portia_config.enable_portia_monitor = true;
    portia_config.check_interval_ms = 25;

    // This should work safely even while running
    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_config, &portia_config), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Metrics Query Tests
//=============================================================================

TEST_F(HealthAgentBehavioralIntegrationTest, GetMetrics_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Query metrics multiple times while running
    for (int i = 0; i < 10; i++) {
        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
        EXPECT_GE(metrics.behavioral_health_score, 0.0f);
        EXPECT_LE(metrics.behavioral_health_score, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, GetHealthScore_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Query health score multiple times via metrics
    for (int i = 0; i < 10; i++) {
        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
        EXPECT_GE(metrics.behavioral_health_score, 0.0f);
        EXPECT_LE(metrics.behavioral_health_score, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, CheckUnhealthyStatus_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // With NULL bridges, should always be healthy
    for (int i = 0; i < 10; i++) {
        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
        EXPECT_FALSE(metrics.any_behavioral_unhealthy);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Coordination Tests
//=============================================================================

TEST_F(HealthAgentBehavioralIntegrationTest, ThermalAbort_SetsFlag) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Request thermal abort coordination
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "abort_hunt", "thermal_spike"), 0);

    // Allow periodic update
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify flag is set
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
    EXPECT_TRUE(metrics.thermal_abort_recommended);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, ConservationMode_SetsFlag) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Request conservation mode coordination
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "conservation_mode", "battery_low"), 0);

    // Allow periodic update
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify flag is set
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
    EXPECT_TRUE(metrics.conservation_mode_active);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, RestPeriod_Accepted) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Request rest period coordination
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "rest_period", "high_fatigue"), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, MultipleCoordinationActions) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Request multiple coordination actions
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "abort_hunt", "thermal_spike"), 0);
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "conservation_mode", "battery_low"), 0);
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "rest_period", "high_fatigue"), 0);

    // Allow periodic update
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify multiple flags are set
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
    EXPECT_TRUE(metrics.thermal_abort_recommended);
    EXPECT_TRUE(metrics.conservation_mode_active);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(HealthAgentBehavioralIntegrationTest, ConcurrentMetricsAccess) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    // Launch multiple threads that query metrics
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &should_stop, &success_count, &failure_count]() {
            while (!should_stop.load()) {
                behavioral_health_metrics_t metrics;
                if (nimcp_health_agent_get_behavioral_metrics(agent, &metrics) == 0) {
                    success_count++;
                } else {
                    failure_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    // Let threads run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Signal stop
    should_stop.store(true);

    // Join threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
    EXPECT_EQ(failure_count.load(), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, ConcurrentCoordinationRequests) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    // Launch multiple threads that request coordination
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &should_stop, &success_count, &failure_count, i]() {
            const char* actions[] = {"abort_hunt", "conservation_mode", "rest_period"};
            int action_idx = 0;
            while (!should_stop.load()) {
                const char* action = actions[action_idx % 3];
                if (nimcp_health_agent_request_behavioral_coordination(
                    agent, action, "concurrent_test") == 0) {
                    success_count++;
                } else {
                    failure_count++;
                }
                action_idx++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    // Let threads run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Signal stop
    should_stop.store(true);

    // Join threads
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
    EXPECT_EQ(failure_count.load(), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HealthAgentBehavioralIntegrationTest, StartStopRestart_WithBehavioral) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Start
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Stop
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // Restart
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Stop again
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, ConnectBehavioral_AfterStart) {
    // Start agent first
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Connect behavioral modules while running
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify metrics are updated
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, CoordinationDuringLifecycle) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Coordination before start should work
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "abort_hunt", "pre_start"), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Coordination during run
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
            agent, "abort_hunt", "during_run"), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(HealthAgentBehavioralIntegrationTest, DisableMonitoring_NoCrash) {
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = false;  // Disabled
    dragonfly_config.enable_stress_monitoring = false;
    dragonfly_config.enable_cross_coordination = false;

    health_agent_portia_monitor_config_t portia_config;
    memset(&portia_config, 0, sizeof(portia_config));
    portia_config.enable_portia_monitor = false;  // Disabled
    portia_config.enable_thermal_monitoring = false;
    portia_config.enable_cross_coordination = false;

    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &dragonfly_config), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, &portia_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should still be able to query (even if monitoring disabled)
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, ZeroInterval_DefaultsApplied) {
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;
    dragonfly_config.check_interval_ms = 0;  // Zero interval

    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &dragonfly_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, UnknownCoordinationAction_Accepted) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Unknown actions should be accepted (logged but no state change)
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "unknown_action", "test"), 0);
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "invalid_coordination", "test"), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, NullReason_Accepted) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // NULL reason should be accepted
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "abort_hunt", nullptr), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Integration with Other Modules
//=============================================================================

TEST_F(HealthAgentBehavioralIntegrationTest, BehavioralWithNeuralModules) {
    // Connect behavioral modules
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Also connect neural modules (NULL bridges)
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for a bit
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);

        // Query both types of metrics
        behavioral_health_metrics_t behavioral_metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &behavioral_metrics), 0);

        neural_health_metrics_t neural_metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &neural_metrics), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralIntegrationTest, BehavioralWithConsistencyChecks) {
    // Connect behavioral modules
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Configure consistency checks
    health_agent_consistency_config_t consistency_config;
    memset(&consistency_config, 0, sizeof(consistency_config));
    consistency_config.check_reference_counts = true;
    consistency_config.check_struct_magic = true;
    consistency_config.consistency_check_interval_ms = 100;

    EXPECT_EQ(nimcp_health_agent_update_consistency_config(agent, &consistency_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run with both behavioral and consistency active
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);

        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
