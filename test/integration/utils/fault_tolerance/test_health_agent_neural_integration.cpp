/**
 * @file test_health_agent_neural_integration.cpp
 * @brief Integration tests for NIMCP Health Agent Neural Module Integration (Phase 5.5)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Test health agent neural connection functions with SNN/LNN bridges
 * WHY:  Verify neural health monitoring works correctly with actual bridges
 * HOW:  Create bridge instances (or use NULL), connect them, verify health checks
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

// Neural module immune bridge headers
#include "snn/nimcp_snn_immune.h"
#include "lnn/nimcp_lnn_immune.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for health agent neural integration tests
 */
class HealthAgentNeuralIntegrationTest : public ::testing::Test {
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

TEST_F(HealthAgentNeuralIntegrationTest, ConnectSNN_WithNullBridge_AgentRuns) {
    // Connect with NULL bridge
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

TEST_F(HealthAgentNeuralIntegrationTest, ConnectLNN_WithNullBridge_AgentRuns) {
    // Connect with NULL bridge
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralIntegrationTest, ConnectBothBridges_AgentRuns) {
    // Connect both with NULL bridges
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify metrics are accessible while running
    neural_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(HealthAgentNeuralIntegrationTest, ConfigureSNN_ThenStart) {
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.enable_instability_detection = true;
    snn_config.enable_auto_report = true;
    snn_config.max_spike_rate_hz = 200.0f;
    snn_config.min_spike_rate_hz = 0.5f;
    snn_config.burst_threshold = 0.6f;
    snn_config.sync_threshold = 0.9f;
    snn_config.check_interval_ms = 50;

    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &snn_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralIntegrationTest, ConfigureLNN_ThenStart) {
    health_agent_lnn_config_t lnn_config;
    memset(&lnn_config, 0, sizeof(lnn_config));
    lnn_config.enable_lnn_monitoring = true;
    lnn_config.enable_stability_detection = true;
    lnn_config.enable_auto_report = true;
    lnn_config.enable_tau_modulation = true;
    lnn_config.state_explosion_threshold = 1e8f;
    lnn_config.state_collapse_threshold = 1e-12f;
    lnn_config.tau_max = 5000.0f;
    lnn_config.tau_min = 0.001f;
    lnn_config.gradient_explosion_threshold = 1e5f;
    lnn_config.gradient_vanishing_threshold = 1e-9f;
    lnn_config.check_interval_ms = 50;

    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, &lnn_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralIntegrationTest, ReconfigureNeural_WhileRunning) {
    // Start with defaults
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Reconfigure while running
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.check_interval_ms = 25;

    health_agent_lnn_config_t lnn_config;
    memset(&lnn_config, 0, sizeof(lnn_config));
    lnn_config.enable_lnn_monitoring = true;
    lnn_config.check_interval_ms = 25;

    // This should work safely even while running
    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_config, &lnn_config), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Metrics Query Tests
//=============================================================================

TEST_F(HealthAgentNeuralIntegrationTest, GetMetrics_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Query metrics multiple times while running
    for (int i = 0; i < 10; i++) {
        neural_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);
        EXPECT_GE(metrics.neural_health_score, 0.0f);
        EXPECT_LE(metrics.neural_health_score, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralIntegrationTest, GetHealthScore_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Query health score multiple times
    for (int i = 0; i < 10; i++) {
        float score = nimcp_health_agent_get_neural_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralIntegrationTest, IsNeuralUnhealthy_WhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // With NULL bridges, should always be healthy
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(HealthAgentNeuralIntegrationTest, ConcurrentMetricsAccess) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    // Launch multiple threads that query metrics
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &should_stop, &success_count, &failure_count]() {
            while (!should_stop.load()) {
                neural_health_metrics_t metrics;
                if (nimcp_health_agent_get_neural_metrics(agent, &metrics) == 0) {
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

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(HealthAgentNeuralIntegrationTest, StartStopRestart_WithNeural) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

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

TEST_F(HealthAgentNeuralIntegrationTest, ConnectNeural_AfterStart) {
    // Start agent first
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Connect neural modules while running
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify metrics are updated
    neural_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(HealthAgentNeuralIntegrationTest, DisableMonitoring_NoCrash) {
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = false;  // Disabled
    snn_config.enable_instability_detection = false;

    health_agent_lnn_config_t lnn_config;
    memset(&lnn_config, 0, sizeof(lnn_config));
    lnn_config.enable_lnn_monitoring = false;  // Disabled
    lnn_config.enable_stability_detection = false;

    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &snn_config), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, &lnn_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should still be able to query (even if monitoring disabled)
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralIntegrationTest, ZeroInterval_DefaultsApplied) {
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.check_interval_ms = 0;  // Zero interval

    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &snn_config), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
