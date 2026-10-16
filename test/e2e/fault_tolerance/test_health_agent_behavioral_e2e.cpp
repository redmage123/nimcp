/**
 * @file test_health_agent_behavioral_e2e.cpp
 * @brief End-to-end tests for NIMCP Health Agent Behavioral Integration (Phase 5.6)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: E2E tests for complete health agent behavioral monitoring workflows
 * WHY:  Verify full system behavior with Dragonfly/Portia immune bridges
 * HOW:  Test realistic scenarios from agent creation through extended operation
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

// Behavioral module immune bridge headers (forward declarations only, avoid conflicts)
// The actual types are forward declared in the health agent header

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief E2E fixture for health agent behavioral workflows
 */
class HealthAgentBehavioralE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        strncpy(config.agent_name, "BehavioralE2ETestAgent", sizeof(config.agent_name) - 1);
        config.check_interval_ms = 50;
        config.heartbeat_interval_ms = 100;
        config.watchdog_timeout_ms = 500;
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

/**
 * @brief E2E fixture for extended behavioral integration tests
 *
 * Note: Brain immune system types not included to avoid typedef conflicts.
 * Use base fixture for all behavioral tests.
 */
class HealthAgentBehavioralExtendedE2ETest : public HealthAgentBehavioralE2ETest {
protected:
    void SetUp() override {
        HealthAgentBehavioralE2ETest::SetUp();
        // Extended setup can be added here
    }

    void TearDown() override {
        HealthAgentBehavioralE2ETest::TearDown();
    }
};

//=============================================================================
// Full Workflow E2E Tests
//=============================================================================

TEST_F(HealthAgentBehavioralE2ETest, FullWorkflow_CreateConnectStartRunStop) {
    // Step 1: Connect behavioral modules
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Step 2: Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Step 3: Run with heartbeats for extended period
    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Step 4: Query behavioral metrics periodically
    for (int i = 0; i < 10; i++) {
        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
        EXPECT_GE(metrics.behavioral_health_score, 0.0f);
        EXPECT_LE(metrics.behavioral_health_score, 100.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Step 5: Get statistics
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.checks_performed, 0u);

    // Step 6: Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

TEST_F(HealthAgentBehavioralE2ETest, FullWorkflow_AllModulesWithBehavioral) {
    // Connect cognitive modules first
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);

    // Connect neural modules
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Then connect behavioral modules
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Start and run
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Verify both behavioral and general health
    EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));
    EXPECT_GE(nimcp_health_agent_get_behavioral_health_score(agent), 0.0f);

    // Also verify neural health since we connected those too
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, FullWorkflow_CustomConfigurations) {
    // Use custom configurations for behavioral modules
    health_agent_dragonfly_immune_config_t dragonfly_cfg;
    memset(&dragonfly_cfg, 0, sizeof(dragonfly_cfg));
    dragonfly_cfg.enable_dragonfly_immune = true;
    dragonfly_cfg.enable_stress_monitoring = true;
    dragonfly_cfg.enable_health_status_tracking = true;
    dragonfly_cfg.enable_injury_detection = true;
    dragonfly_cfg.enable_fatigue_tracking = true;
    dragonfly_cfg.enable_cross_coordination = true;
    dragonfly_cfg.stress_warning_threshold = 0.50f;
    dragonfly_cfg.stress_critical_threshold = 0.80f;
    dragonfly_cfg.fatigue_warning_threshold = 0.60f;
    dragonfly_cfg.fatigue_critical_threshold = 0.90f;
    dragonfly_cfg.abort_hunt_on_thermal = true;
    dragonfly_cfg.abort_hunt_on_battery_low = true;
    dragonfly_cfg.reduce_intensity_on_stress = true;
    dragonfly_cfg.enable_auto_rest = true;
    dragonfly_cfg.rest_trigger_fatigue = 0.70f;
    dragonfly_cfg.min_rest_duration_ms = 5000;
    dragonfly_cfg.check_interval_ms = 50;

    health_agent_portia_monitor_config_t portia_cfg;
    memset(&portia_cfg, 0, sizeof(portia_cfg));
    portia_cfg.enable_portia_monitor = true;
    portia_cfg.enable_thermal_monitoring = true;
    portia_cfg.enable_power_monitoring = true;
    portia_cfg.enable_cpu_load_monitoring = true;
    portia_cfg.enable_degradation_tracking = true;
    portia_cfg.enable_cross_coordination = true;
    portia_cfg.thermal_warning_temp_c = 70.0f;
    portia_cfg.thermal_critical_temp_c = 85.0f;
    portia_cfg.throttle_on_warm = true;
    portia_cfg.emergency_on_critical = true;
    portia_cfg.battery_warning_pct = 20.0f;
    portia_cfg.battery_critical_pct = 5.0f;
    portia_cfg.conservation_on_battery = true;
    portia_cfg.hibernate_on_critical = true;
    portia_cfg.cpu_warning_pct = 80.0f;
    portia_cfg.cpu_critical_pct = 95.0f;
    portia_cfg.reduce_load_on_warning = true;
    portia_cfg.check_interval_ms = 50;

    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, &dragonfly_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, &portia_cfg), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for extended period with custom configs
    for (int i = 0; i < 40; i++) {
        nimcp_health_agent_heartbeat(agent);

        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Cross-Module Coordination E2E Tests
//=============================================================================

TEST_F(HealthAgentBehavioralE2ETest, CrossModuleCoordination_ThermalAbortRequest) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for a bit
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Request thermal abort (simulating Portia detecting high temp)
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "abort_hunt", "thermal_spike"), 0);

    // Allow periodic update to copy atomic flags to metrics
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Verify metrics reflect coordination
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
    EXPECT_TRUE(metrics.thermal_abort_recommended);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, CrossModuleCoordination_PowerConservation) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for a bit
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Request power conservation (simulating low battery)
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "conservation_mode", "battery_low"), 0);

    // Allow periodic update to copy atomic flags to metrics
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Verify metrics reflect coordination
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
    EXPECT_TRUE(metrics.conservation_mode_active);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, CrossModuleCoordination_RestPeriod) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for a bit
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Request rest period (simulating high fatigue)
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "rest_period", "high_fatigue"), 0);

    // Continue running during rest period
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, CrossModuleCoordination_MultipleActions) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Request multiple coordination actions
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "abort_hunt", "thermal_spike"), 0);
    EXPECT_EQ(nimcp_health_agent_request_behavioral_coordination(
        agent, "conservation_mode", "battery_low"), 0);

    // Allow periodic update to copy atomic flags to metrics
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Verify all flags are set
    behavioral_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
    EXPECT_TRUE(metrics.thermal_abort_recommended);
    EXPECT_TRUE(metrics.conservation_mode_active);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Dynamic Reconfiguration E2E Tests
//=============================================================================

TEST_F(HealthAgentBehavioralE2ETest, DynamicReconfig_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run with initial config
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Reconfigure while running
    health_agent_dragonfly_immune_config_t dragonfly_cfg;
    memset(&dragonfly_cfg, 0, sizeof(dragonfly_cfg));
    dragonfly_cfg.enable_dragonfly_immune = true;
    dragonfly_cfg.check_interval_ms = 25;  // Faster checks

    health_agent_portia_monitor_config_t portia_cfg;
    memset(&portia_cfg, 0, sizeof(portia_cfg));
    portia_cfg.enable_portia_monitor = true;
    portia_cfg.check_interval_ms = 25;  // Faster checks

    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_cfg, &portia_cfg), 0);

    // Continue running with new config
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_heartbeat(agent);

        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, DynamicReconfig_DisableEnableMonitoring) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run with monitoring enabled
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Disable monitoring
    health_agent_dragonfly_immune_config_t dragonfly_cfg;
    memset(&dragonfly_cfg, 0, sizeof(dragonfly_cfg));
    dragonfly_cfg.enable_dragonfly_immune = false;

    health_agent_portia_monitor_config_t portia_cfg;
    memset(&portia_cfg, 0, sizeof(portia_cfg));
    portia_cfg.enable_portia_monitor = false;

    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_cfg, &portia_cfg), 0);

    // Run with monitoring disabled
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        // Should still be callable even when disabled
        EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Re-enable monitoring
    dragonfly_cfg.enable_dragonfly_immune = true;
    portia_cfg.enable_portia_monitor = true;
    EXPECT_EQ(nimcp_health_agent_configure_behavioral(agent, &dragonfly_cfg, &portia_cfg), 0);

    // Run with monitoring re-enabled
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Concurrent Operations E2E Tests
//=============================================================================

TEST_F(HealthAgentBehavioralE2ETest, ConcurrentOps_MetricsQueryDuringOperation) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> total_queries{0};
    std::atomic<int> failed_queries{0};

    // Thread 1: Query behavioral metrics
    std::thread metrics_thread([this, &should_stop, &total_queries, &failed_queries]() {
        while (!should_stop.load()) {
            behavioral_health_metrics_t metrics;
            if (nimcp_health_agent_get_behavioral_metrics(agent, &metrics) == 0) {
                total_queries++;
            } else {
                failed_queries++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Thread 2: Query health score
    std::thread score_thread([this, &should_stop, &total_queries]() {
        while (!should_stop.load()) {
            float score = nimcp_health_agent_get_behavioral_health_score(agent);
            if (score >= 0.0f && score <= 100.0f) {
                total_queries++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Thread 3: Check is_unhealthy
    std::thread unhealthy_thread([this, &should_stop, &total_queries]() {
        while (!should_stop.load()) {
            (void)nimcp_health_agent_is_behavioral_unhealthy(agent);
            total_queries++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Main thread: heartbeats
    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    should_stop.store(true);
    metrics_thread.join();
    score_thread.join();
    unhealthy_thread.join();

    EXPECT_GT(total_queries.load(), 100);
    EXPECT_EQ(failed_queries.load(), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, ConcurrentOps_ReconfigDuringQuery) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> query_count{0};
    std::atomic<int> reconfig_count{0};

    // Thread: Query metrics continuously
    std::thread query_thread([this, &should_stop, &query_count]() {
        while (!should_stop.load()) {
            behavioral_health_metrics_t metrics;
            if (nimcp_health_agent_get_behavioral_metrics(agent, &metrics) == 0) {
                query_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Main thread: reconfigure periodically
    for (int i = 0; i < 20; i++) {
        health_agent_dragonfly_immune_config_t dragonfly_cfg;
        memset(&dragonfly_cfg, 0, sizeof(dragonfly_cfg));
        dragonfly_cfg.enable_dragonfly_immune = true;
        dragonfly_cfg.check_interval_ms = 25 + (i % 5) * 10;

        if (nimcp_health_agent_configure_behavioral(agent, &dragonfly_cfg, nullptr) == 0) {
            reconfig_count++;
        }

        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    should_stop.store(true);
    query_thread.join();

    EXPECT_GT(query_count.load(), 50);
    EXPECT_GT(reconfig_count.load(), 10);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, ConcurrentOps_CoordinationRequests) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> coordination_count{0};

    // Thread 1: Request thermal aborts
    std::thread thermal_thread([this, &should_stop, &coordination_count]() {
        while (!should_stop.load()) {
            if (nimcp_health_agent_request_behavioral_coordination(
                    agent, "abort_hunt", "thermal_test") == 0) {
                coordination_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Thread 2: Request power conservation
    std::thread power_thread([this, &should_stop, &coordination_count]() {
        while (!should_stop.load()) {
            if (nimcp_health_agent_request_behavioral_coordination(
                    agent, "conservation_mode", "power_test") == 0) {
                coordination_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Main thread: heartbeats and metrics queries
    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);
        behavioral_health_metrics_t metrics;
        nimcp_health_agent_get_behavioral_metrics(agent, &metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    should_stop.store(true);
    thermal_thread.join();
    power_thread.join();

    EXPECT_GT(coordination_count.load(), 10);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Extended Operation E2E Tests
//=============================================================================

TEST_F(HealthAgentBehavioralE2ETest, ExtendedOperation_3SecondsRuntime) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    auto start_time = std::chrono::steady_clock::now();
    int heartbeat_count = 0;
    int metrics_queries = 0;

    // Run for 3 seconds
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(3)) {
        nimcp_health_agent_heartbeat(agent);
        heartbeat_count++;

        if (heartbeat_count % 5 == 0) {
            behavioral_health_metrics_t metrics;
            EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
            EXPECT_FALSE(metrics.any_behavioral_unhealthy);
            metrics_queries++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_GT(heartbeat_count, 100);
    EXPECT_GT(metrics_queries, 20);

    health_agent_stats_t stats;
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GT(stats.checks_performed, 0u);
    EXPECT_GE(stats.uptime_ms, 2500u);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, ExtendedOperation_StartStopCycle) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    // Multiple start/stop cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);
        EXPECT_TRUE(nimcp_health_agent_is_running(agent));

        // Run each cycle for 200ms
        for (int i = 0; i < 10; i++) {
            nimcp_health_agent_heartbeat(agent);

            behavioral_health_metrics_t metrics;
            EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
        EXPECT_FALSE(nimcp_health_agent_is_running(agent));

        // Brief pause between cycles
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

//=============================================================================
// Neural + Behavioral Integration E2E Tests
//=============================================================================

TEST_F(HealthAgentBehavioralE2ETest, Integration_NeuralAndBehavioral) {
    // Connect neural modules
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Connect behavioral modules
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run and query both neural and behavioral metrics
    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);

        neural_health_metrics_t neural_metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &neural_metrics), 0);

        behavioral_health_metrics_t behavioral_metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &behavioral_metrics), 0);

        // Both should report healthy
        EXPECT_FALSE(neural_metrics.any_neural_unhealthy);
        EXPECT_FALSE(behavioral_metrics.any_behavioral_unhealthy);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, Integration_FullModuleStack) {
    // Connect all cognitive modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);

    // Connect all neural modules
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Connect all behavioral modules
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Sustained operation with full module stack
    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_heartbeat(agent);

        // Query various health indicators
        EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
        EXPECT_FALSE(nimcp_health_agent_is_behavioral_unhealthy(agent));

        float neural_score = nimcp_health_agent_get_neural_health_score(agent);
        float behavioral_score = nimcp_health_agent_get_behavioral_health_score(agent);

        EXPECT_GE(neural_score, 0.0f);
        EXPECT_LE(neural_score, 100.0f);
        EXPECT_GE(behavioral_score, 0.0f);
        EXPECT_LE(behavioral_score, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Stress Test E2E
//=============================================================================

TEST_F(HealthAgentBehavioralE2ETest, StressTest_RapidOperations) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    int operations = 0;

    // Rapid operations for 2 seconds
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
        // Rapid heartbeats
        nimcp_health_agent_heartbeat(agent);
        operations++;

        // Rapid metrics queries
        behavioral_health_metrics_t metrics;
        nimcp_health_agent_get_behavioral_metrics(agent, &metrics);
        operations++;

        // Rapid health score queries
        nimcp_health_agent_get_behavioral_health_score(agent);
        operations++;

        // Rapid unhealthy checks
        nimcp_health_agent_is_behavioral_unhealthy(agent);
        operations++;

        // Minimal sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_GT(operations, 1000);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, StressTest_RapidCoordinationRequests) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    int coordination_requests = 0;

    // Rapid coordination requests for 1 second
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(1)) {
        nimcp_health_agent_request_behavioral_coordination(agent, "abort_hunt", "stress_test");
        coordination_requests++;

        nimcp_health_agent_request_behavioral_coordination(agent, "conservation_mode", "stress_test");
        coordination_requests++;

        nimcp_health_agent_heartbeat(agent);

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_GT(coordination_requests, 200);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Integration with Immune System E2E Tests
//=============================================================================

TEST_F(HealthAgentBehavioralExtendedE2ETest, ExtendedIntegration_BehavioralWithAllCognitive) {
    // Connect cognitive modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);

    // Connect behavioral modules
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run with both behavioral and cognitive connections
    for (int i = 0; i < 30; i++) {
        nimcp_health_agent_heartbeat(agent);

        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Error Recovery E2E Tests
//=============================================================================

TEST_F(HealthAgentBehavioralE2ETest, ErrorRecovery_ContinuousOperationAfterBadConfig) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Attempt various edge case configurations
    health_agent_dragonfly_immune_config_t edge_cfg;
    memset(&edge_cfg, 0, sizeof(edge_cfg));

    // Edge case: negative values
    edge_cfg.stress_warning_threshold = -1.0f;
    edge_cfg.stress_critical_threshold = -0.5f;
    nimcp_health_agent_configure_behavioral(agent, &edge_cfg, nullptr);

    // Agent should still be functional
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        behavioral_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_behavioral_metrics(agent, &metrics), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentBehavioralE2ETest, ErrorRecovery_UnknownCoordinationActions) {
    EXPECT_EQ(nimcp_health_agent_connect_dragonfly_immune(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_portia_monitor(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Request unknown coordination action - these are accepted (logged but no state change)
    int result = nimcp_health_agent_request_behavioral_coordination(
        agent, "unknown_action", "test_reason");

    // Unknown actions are accepted (return 0) but don't modify coordination state
    EXPECT_EQ(result, 0);

    // Agent should still be functional
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
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
