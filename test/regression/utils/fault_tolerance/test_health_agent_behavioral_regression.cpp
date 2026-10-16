/**
 * @file test_health_agent_behavioral_regression.cpp
 * @brief Regression tests for NIMCP Health Agent Behavioral Integration (Phase 5.6)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Regression tests to ensure behavioral integration doesn't break
 * WHY:  Prevent regressions in Dragonfly/Portia health monitoring across releases
 * HOW:  Test specific behaviors that must remain consistent
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <cfloat>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

// Behavioral module immune bridge headers
#include "dragonfly/nimcp_dragonfly_immune_bridge.h"
#include "portia/nimcp_portia_monitoring.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Fixture for health agent behavioral regression tests
 */
class HealthAgentBehavioralRegressionTest : public ::testing::Test {
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
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

//=============================================================================
// Return Value Contract Tests
// These tests verify the documented return value contract is maintained
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, ReturnValues_NullAgentAlwaysReturnsMinusOne) {
    // All behavioral connect functions MUST return -1 for NULL agent
    // This is a documented behavior that must not change

    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_configure_behavioral(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(nullptr, nullptr, nullptr), -1);
}

TEST_F(HealthAgentBehavioralRegressionTest, ReturnValues_ValidAgentReturnsZero) {
    // All behavioral connect functions MUST return 0 for valid agent
    // Even with NULL bridge pointers (allowed for lazy connection)

    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, ReturnValues_NullMetricsReturnsMinusOne) {
    // get_behavioral_metrics MUST return -1 for NULL metrics pointer
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, nullptr), -1);
}

TEST_F(HealthAgentBehavioralRegressionTest, ReturnValues_NullActionReturnsMinusOne) {
    // request_behavioral_coordination MUST return -1 for NULL action
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(agent, nullptr, "reason"), -1);
}

TEST_F(HealthAgentBehavioralRegressionTest, ReturnValues_UnknownActionAccepted) {
    // request_behavioral_coordination accepts unknown actions (returns 0)
    // Unknown actions are logged but don't modify coordination state
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(agent, "invalid_action", "reason"), 0);
}

//=============================================================================
// Health Score Contract Tests
// These tests verify health score behavior is consistent
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, HealthScore_NullAgentReturns100) {
    // NULL agent MUST return 100.0 (safe default - healthy)
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_behavioral_health_score(nullptr), 100.0f);
}

TEST_F(HealthAgentBehavioralRegressionTest, HealthScore_NoConnectionsReturns100) {
    // No behavioral connections MUST return 100.0 (nothing to degrade)
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_behavioral_health_score(agent), 100.0f);
}

TEST_F(HealthAgentBehavioralRegressionTest, HealthScore_NullBridgesReturns100) {
    // Connected with NULL bridges MUST return 100.0 (nothing to monitor)
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_behavioral_health_score(agent), 100.0f);
}

TEST_F(HealthAgentBehavioralRegressionTest, HealthScore_AlwaysInValidRange) {
    // Health score MUST always be in [0, 100]
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    float score = nimcp_health_agent_get_behavioral_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

//=============================================================================
// IsUnhealthy Contract Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, IsUnhealthy_NullAgentReturnsFalse) {
    // NULL agent MUST return false (safe default - not unhealthy)
    EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(nullptr));
}

TEST_F(HealthAgentBehavioralRegressionTest, IsUnhealthy_NoConnectionsReturnsFalse) {
    // No behavioral connections MUST return false
    EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));
}

TEST_F(HealthAgentBehavioralRegressionTest, IsUnhealthy_NullBridgesReturnsFalse) {
    // NULL bridges MUST return false (nothing to be unhealthy)
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));
}

//=============================================================================
// Default Configuration Contract Tests
// These tests verify default configurations are applied correctly
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, DefaultConfig_Dragonfly) {
    // When config is NULL, defaults MUST be applied
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);

    // Agent should be functional after connection with defaults
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, DefaultConfig_Portia) {
    // When config is NULL, defaults MUST be applied
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Agent should be functional after connection with defaults
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, DefaultConfig_BothBehavioral) {
    // Both behavioral modules with defaults
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Custom Configuration Contract Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, CustomConfig_Dragonfly_Accepted) {
    health_agent_dragonfly_immune_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_dragonfly_immune = true;
    custom_config.enable_stress_monitoring = true;
    custom_config.enable_health_status_tracking = true;
    custom_config.enable_injury_detection = true;
    custom_config.enable_fatigue_tracking = true;
    custom_config.enable_cross_coordination = true;
    custom_config.stress_warning_threshold = 0.50f;
    custom_config.stress_critical_threshold = 0.80f;
    custom_config.fatigue_warning_threshold = 0.60f;
    custom_config.fatigue_critical_threshold = 0.90f;
    custom_config.abort_hunt_on_thermal = true;
    custom_config.abort_hunt_on_battery_low = true;
    custom_config.reduce_intensity_on_stress = true;
    custom_config.enable_auto_rest = true;
    custom_config.rest_trigger_fatigue = 0.70f;
    custom_config.min_rest_duration_ms = 5000;
    custom_config.check_interval_ms = 50;

    // Custom config MUST be accepted
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &custom_config), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, CustomConfig_Portia_Accepted) {
    health_agent_portia_monitor_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_portia_monitor = true;
    custom_config.enable_thermal_monitoring = true;
    custom_config.enable_power_monitoring = true;
    custom_config.enable_cpu_load_monitoring = true;
    custom_config.enable_degradation_tracking = true;
    custom_config.enable_cross_coordination = true;
    custom_config.thermal_warning_temp_c = 70.0f;
    custom_config.thermal_critical_temp_c = 85.0f;
    custom_config.throttle_on_warm = true;
    custom_config.emergency_on_critical = true;
    custom_config.battery_warning_pct = 20.0f;
    custom_config.battery_critical_pct = 5.0f;
    custom_config.conservation_on_battery = true;
    custom_config.hibernate_on_critical = true;
    custom_config.cpu_warning_pct = 80.0f;
    custom_config.cpu_critical_pct = 95.0f;
    custom_config.reduce_load_on_warning = true;
    custom_config.check_interval_ms = 50;

    // Custom config MUST be accepted
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, &custom_config), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Metrics Structure Contract Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, Metrics_DragonflyFieldsExist) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));  // Fill with known pattern
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Dragonfly fields MUST be populated (even if bridge is NULL)
    // With NULL bridge, dragonfly_connected should be false
    EXPECT_FALSE(metrics.dragonfly_connected);
    EXPECT_TRUE(metrics.dragonfly_healthy);  // Default is healthy
}

TEST_F(HealthAgentBehavioralRegressionTest, Metrics_PortiaFieldsExist) {
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Portia fields MUST be populated
    EXPECT_FALSE(metrics.portia_connected);
    EXPECT_TRUE(metrics.portia_healthy);  // Default is healthy
}

TEST_F(HealthAgentBehavioralRegressionTest, Metrics_CombinedFieldsExist) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Combined fields MUST be valid
    EXPECT_FALSE(metrics.any_behavioral_unhealthy);
    EXPECT_GE(metrics.behavioral_health_score, 0.0f);
    EXPECT_LE(metrics.behavioral_health_score, 100.0f);
}

TEST_F(HealthAgentBehavioralRegressionTest, Metrics_CoordinationFieldsExist) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

    // Coordination fields should initially be false
    EXPECT_FALSE(metrics.thermal_abort_recommended);
    EXPECT_FALSE(metrics.power_abort_recommended);
    EXPECT_FALSE(metrics.conservation_mode_active);
}

//=============================================================================
// Coordination Action Contract Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, Coordination_AbortHuntSetsFlag) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Request abort_hunt action
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "abort_hunt", "thermal_test"), 0);

    // Allow periodic update to run and copy atomic flags to metrics
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Flag MUST be set after periodic update
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
    EXPECT_TRUE(metrics.thermal_abort_recommended);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Coordination_ConservationModeSetsFlag) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Request conservation_mode action
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "conservation_mode", "battery_low"), 0);

    // Allow periodic update to run and copy atomic flags to metrics
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Flag MUST be set after periodic update
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
    EXPECT_TRUE(metrics.conservation_mode_active);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Coordination_RestPeriodAccepted) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Request rest_period action MUST be accepted
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "rest_period", "fatigue_test"), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Coordination_NullReasonAccepted) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);

    // NULL reason MUST be accepted
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "abort_hunt", nullptr), 0);
}

//=============================================================================
// Multiple Connections Contract Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, MultipleConnects_Dragonfly_Allowed) {
    // Multiple connections MUST be allowed (reconnection)
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    }
}

TEST_F(HealthAgentBehavioralRegressionTest, MultipleConnects_Portia_Allowed) {
    // Multiple connections MUST be allowed (reconnection)
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    }
}

TEST_F(HealthAgentBehavioralRegressionTest, MultipleConfigures_Allowed) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Multiple configurations MUST be allowed
    for (int i = 0; i < 5; i++) {
        health_agent_dragonfly_immune_config_t dragonfly_config;
        memset(&dragonfly_config, 0, sizeof(dragonfly_config));
        dragonfly_config.enable_dragonfly_immune = true;
        dragonfly_config.check_interval_ms = 50 + i * 10;

        health_agent_portia_monitor_config_t portia_config;
        memset(&portia_config, 0, sizeof(portia_config));
        portia_config.enable_portia_monitor = true;
        portia_config.check_interval_ms = 50 + i * 10;

        EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_config, &portia_config), 0);
    }
}

//=============================================================================
// Boundary Value Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, Boundary_ZeroCheckInterval_Accepted) {
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;
    dragonfly_config.check_interval_ms = 0;

    // Zero interval MUST be accepted (implementation handles it)
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &dragonfly_config), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Boundary_MaxThresholds_Accepted) {
    health_agent_portia_monitor_config_t portia_config;
    memset(&portia_config, 0, sizeof(portia_config));
    portia_config.enable_portia_monitor = true;
    portia_config.thermal_warning_temp_c = FLT_MAX;
    portia_config.thermal_critical_temp_c = FLT_MAX;
    portia_config.battery_warning_pct = 100.0f;
    portia_config.battery_critical_pct = 100.0f;

    // Max thresholds MUST be accepted
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, &portia_config), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Boundary_ZeroThresholds_Accepted) {
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;
    dragonfly_config.stress_warning_threshold = 0.0f;
    dragonfly_config.stress_critical_threshold = 0.0f;

    // Zero thresholds MUST be accepted
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &dragonfly_config), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Boundary_NegativeThresholds_Accepted) {
    // Negative values may be edge cases but should not crash
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;
    dragonfly_config.stress_warning_threshold = -0.5f;

    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &dragonfly_config), 0);
}

//=============================================================================
// Thread Safety Contract Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, ThreadSafety_ConcurrentMetricsQueries) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    // Multiple threads querying metrics MUST not crash
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

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    should_stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
    EXPECT_EQ(failure_count.load(), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, ThreadSafety_ConcurrentHealthScoreQueries) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> valid_scores{0};
    std::atomic<int> invalid_scores{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &should_stop, &valid_scores, &invalid_scores]() {
            while (!should_stop.load()) {
                float score = nimcp_health_agent_get_behavioral_health_score(agent);
                if (score >= 0.0f && score <= 100.0f) {
                    valid_scores++;
                } else {
                    invalid_scores++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    should_stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(valid_scores.load(), 0);
    EXPECT_EQ(invalid_scores.load(), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, ThreadSafety_ConcurrentCoordinationRequests) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &should_stop, &success_count, i]() {
            const char* actions[] = {"abort_hunt", "conservation_mode", "rest_period"};
            while (!should_stop.load()) {
                const char* action = actions[i % 3];
                if (nimcp_health_agent_request_behavioral_coordination(agent, action, "thread_test") == 0) {
                    success_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    should_stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Lifecycle Contract Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, Lifecycle_ConnectBeforeStart) {
    // Connection before start MUST work
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Lifecycle_ConnectAfterStart) {
    // Connection after start MUST work
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Lifecycle_ConfigureWhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Configure while running MUST work
    health_agent_dragonfly_immune_config_t dragonfly_config;
    memset(&dragonfly_config, 0, sizeof(dragonfly_config));
    dragonfly_config.enable_dragonfly_immune = true;

    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_config, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Lifecycle_StartStopRestart) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Start/stop/restart MUST work with behavioral connections
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralRegressionTest, Lifecycle_CoordinationDuringOperation) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Coordination requests during operation MUST work
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
            agent, "abort_hunt", "lifecycle_test"), 0);
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Integration with Neural Modules Contract Tests
//=============================================================================

TEST_F(HealthAgentBehavioralRegressionTest, Integration_NeuralAndBehavioral) {
    // Connect both neural and behavioral modules
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Both APIs MUST work together
    neural_health_metrics_t neural_metrics;
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &neural_metrics), 0);

    behavioral_health_metrics_t behavioral_metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &behavioral_metrics), 0);

    // Both should report healthy
    EXPECT_FALSE(neural_metrics.any_neural_unhealthy);
    EXPECT_FALSE(behavioral_metrics.any_behavioral_unhealthy);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
