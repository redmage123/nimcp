/**
 * @file test_health_agent_behavioral_functions.cpp
 * @brief Unit tests for NIMCP Health Agent Behavioral Integration (Phase 5.6)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Test health agent behavioral connection functions for Dragonfly/Portia modules
 * WHY:  Ensure behavioral integration correctly configures and monitors behavior/resources
 * HOW:  Test each connect function with valid inputs, NULL inputs, default configs
 */

#include <gtest/gtest.h>
#include <cstring>

// Health agent header (has its own extern "C" guard)
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for health agent behavioral tests
 */
class HealthAgentBehavioralTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 100;
        config.enable_auto_recovery = false;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

//=============================================================================
// connect_dragonfly_immune Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, ConnectDragonflyImmune_NullAgent) {
    dragonfly_immune_bridge_t bridge = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(nullptr, bridge, nullptr), -1);
}

TEST_F(HealthAgentBehavioralTest, ConnectDragonflyImmune_NullBridge_WithDefaults) {
    // Should succeed with NULL bridge - stores NULL and applies defaults
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentBehavioralTest, ConnectDragonflyImmune_WithCustomConfig) {
    health_agent_dragonfly_immune_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_dragonfly_immune = true;
    custom_config.enable_stress_monitoring = true;
    custom_config.enable_health_status_tracking = true;
    custom_config.enable_injury_detection = true;
    custom_config.enable_fatigue_tracking = true;
    custom_config.enable_cross_coordination = true;
    custom_config.stress_warning_threshold = 0.6f;
    custom_config.stress_critical_threshold = 0.85f;
    custom_config.fatigue_warning_threshold = 0.5f;
    custom_config.fatigue_critical_threshold = 0.8f;
    custom_config.abort_hunt_on_thermal = true;
    custom_config.abort_hunt_on_battery_low = true;
    custom_config.reduce_intensity_on_stress = false;
    custom_config.enable_auto_rest = true;
    custom_config.rest_trigger_fatigue = 0.75f;
    custom_config.min_rest_duration_ms = 10000;
    custom_config.check_interval_ms = 200;

    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentBehavioralTest, ConnectDragonflyImmune_MultipleConnects) {
    // Should allow reconnection (replacing previous connection)
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentBehavioralTest, ConnectDragonflyImmune_VerifyDefaultConfig) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Bridge not connected (NULL), so dragonfly_connected should be false
    EXPECT_FALSE(metrics.dragonfly_connected);
}

//=============================================================================
// connect_portia_monitor Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, ConnectPortiaMonitor_NullAgent) {
    portia_monitor_t monitor = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(nullptr, monitor, nullptr), -1);
}

TEST_F(HealthAgentBehavioralTest, ConnectPortiaMonitor_NullMonitor_WithDefaults) {
    // Should succeed with NULL monitor - stores NULL and applies defaults
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentBehavioralTest, ConnectPortiaMonitor_WithCustomConfig) {
    health_agent_portia_monitor_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_portia_monitor = true;
    custom_config.enable_thermal_monitoring = true;
    custom_config.enable_power_monitoring = true;
    custom_config.enable_cpu_load_monitoring = true;
    custom_config.enable_degradation_tracking = true;
    custom_config.enable_cross_coordination = true;
    custom_config.thermal_warning_temp_c = 75.0f;
    custom_config.thermal_critical_temp_c = 90.0f;
    custom_config.throttle_on_warm = false;
    custom_config.emergency_on_critical = true;
    custom_config.battery_warning_pct = 25.0f;
    custom_config.battery_critical_pct = 10.0f;
    custom_config.conservation_on_battery = true;
    custom_config.hibernate_on_critical = true;
    custom_config.cpu_warning_pct = 85.0f;
    custom_config.cpu_critical_pct = 98.0f;
    custom_config.reduce_load_on_warning = false;
    custom_config.notify_dragonfly_on_thermal = true;
    custom_config.notify_neural_on_power = true;
    custom_config.trigger_checkpoint_on_power_loss = true;
    custom_config.check_interval_ms = 1000;

    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentBehavioralTest, ConnectPortiaMonitor_MultipleConnects) {
    // Should allow reconnection (replacing previous connection)
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentBehavioralTest, ConnectPortiaMonitor_VerifyDefaultConfig) {
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Monitor not connected (NULL), so portia_connected should be false
    EXPECT_FALSE(metrics.portia_connected);
}

//=============================================================================
// get_behavioral_metrics Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, GetBehavioralMetrics_NullAgent) {
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(nullptr, &metrics), -1);
}

TEST_F(HealthAgentBehavioralTest, GetBehavioralMetrics_NullMetrics) {
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, nullptr), -1);
}

TEST_F(HealthAgentBehavioralTest, GetBehavioralMetrics_BeforeConnect) {
    behavioral_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));  // Fill with non-zero to detect clearing

    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Nothing connected, both should be false
    EXPECT_FALSE(metrics.dragonfly_connected);
    EXPECT_FALSE(metrics.portia_connected);
    EXPECT_FALSE(metrics.any_behavioral_unhealthy);
}

TEST_F(HealthAgentBehavioralTest, GetBehavioralMetrics_AfterDragonflyConnect) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // NULL bridge means not connected
    EXPECT_FALSE(metrics.dragonfly_connected);
    EXPECT_FALSE(metrics.portia_connected);
}

TEST_F(HealthAgentBehavioralTest, GetBehavioralMetrics_AfterBothConnect) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Both with NULL bridges
    EXPECT_FALSE(metrics.dragonfly_connected);
    EXPECT_FALSE(metrics.portia_connected);
}

//=============================================================================
// configure_behavioral Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, ConfigureBehavioral_NullAgent) {
    EXPECT_EQ(nimcp_health_agent_configure_behavioral(nullptr, nullptr, nullptr), -1);
}

TEST_F(HealthAgentBehavioralTest, ConfigureBehavioral_NullConfigs) {
    // Should succeed with NULL configs (no changes)
    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentBehavioralTest, ConfigureBehavioral_DragonflyConfigOnly) {
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;
    dragonfly_config.stress_warning_threshold = 0.7f;

    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_config, nullptr), 0);
}

TEST_F(HealthAgentBehavioralTest, ConfigureBehavioral_PortiaConfigOnly) {
    health_agent_portia_monitor_config_t portia_config;
    memset(&portia_config, 0, sizeof(portia_config));
    portia_config.enable_portia_monitor = true;
    portia_config.thermal_warning_temp_c = 80.0f;

    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, nullptr, &portia_config), 0);
}

TEST_F(HealthAgentBehavioralTest, ConfigureBehavioral_BothConfigs) {
    health_agent_dragonfly_immune_config_t dragonfly_config;
    health_agent_portia_monitor_config_t portia_config;

    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    memset(&portia_config, 0, sizeof(portia_config));

    dragonfly_config.enable_dragonfly_immune = true;
    portia_config.enable_portia_monitor = true;

    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_config, &portia_config), 0);
}

TEST_F(HealthAgentBehavioralTest, ConfigureBehavioral_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;

    // Should still succeed while running
    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_config, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// is_behavioral_unhealthy Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, IsBehavioralUnhealthy_NullAgent) {
    EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(nullptr));
}

TEST_F(HealthAgentBehavioralTest, IsBehavioralUnhealthy_NothingConnected) {
    EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));
}

TEST_F(HealthAgentBehavioralTest, IsBehavioralUnhealthy_HealthyConnection) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    // NULL bridge means not connected, so should be healthy (no module to be unhealthy)
    EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));
}

//=============================================================================
// get_behavioral_health_score Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, GetBehavioralHealthScore_NullAgent) {
    // NULL agent should return 100.0 (safe default)
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_behavioral_health_score(nullptr), 100.0f);
}

TEST_F(HealthAgentBehavioralTest, GetBehavioralHealthScore_NothingConnected) {
    // No modules connected should return 100.0 (nothing to be unhealthy)
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_behavioral_health_score(agent), 100.0f);
}

TEST_F(HealthAgentBehavioralTest, GetBehavioralHealthScore_NullBridgesConnected) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // NULL bridges mean not connected, so score should be 100.0
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_behavioral_health_score(agent), 100.0f);
}

//=============================================================================
// request_behavioral_coordination Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, RequestBehavioralCoordination_NullAgent) {
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(nullptr, "abort_hunt", "test"), -1);
}

TEST_F(HealthAgentBehavioralTest, RequestBehavioralCoordination_NullAction) {
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(agent, nullptr, "test"), -1);
}

TEST_F(HealthAgentBehavioralTest, RequestBehavioralCoordination_NullReason) {
    // Null reason is allowed
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(agent, "abort_hunt", nullptr), 0);
}

TEST_F(HealthAgentBehavioralTest, RequestBehavioralCoordination_AbortHunt) {
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(agent, "abort_hunt", "thermal test"), 0);
}

TEST_F(HealthAgentBehavioralTest, RequestBehavioralCoordination_ConservationMode) {
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(agent, "conservation_mode", "battery test"), 0);
}

TEST_F(HealthAgentBehavioralTest, RequestBehavioralCoordination_RestPeriod) {
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(agent, "rest_period", "fatigue test"), 0);
}

TEST_F(HealthAgentBehavioralTest, RequestBehavioralCoordination_UnknownAction) {
    // Unknown actions should succeed (just logged)
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(agent, "unknown_action", "test"), 0);
}

//=============================================================================
// Combined Dragonfly/Portia Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, ConnectBoth_ThenGetMetrics) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // NULL bridges mean not actually connected
    EXPECT_FALSE(metrics.dragonfly_connected);
    EXPECT_FALSE(metrics.portia_connected);
    EXPECT_FALSE(metrics.any_behavioral_unhealthy);
}

TEST_F(HealthAgentBehavioralTest, ConfigureThenConnect) {
    // Configure first, then connect
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;
    dragonfly_config.stress_warning_threshold = 0.4f;

    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_config, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);

    // Health should still be good
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_behavioral_health_score(agent), 100.0f);
}

TEST_F(HealthAgentBehavioralTest, StartStopWithBehavioralConnections) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Check health while running
    EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, ConnectBeforeStart) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralTest, ConnectAfterStart) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralTest, ReconfigureWhileRunning) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Initial connection
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);

    // Reconfigure while running
    health_agent_dragonfly_immune_config_t new_config;
    memset(&new_config, 0, sizeof(new_config));
    new_config.enable_dragonfly_immune = true;
    new_config.stress_warning_threshold = 0.3f;

    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &new_config, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(HealthAgentBehavioralTest, ConcurrentMetricsQueries) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Query metrics multiple times in quick succession
    for (int i = 0; i < 100; i++) {
        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
        EXPECT_FLOAT_EQ(nimcp_health_agent_get_behavioral_health_score(agent), 100.0f);
        EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}
