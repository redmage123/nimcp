/**
 * @file test_health_agent_neural_regression.cpp
 * @brief Regression tests for NIMCP Health Agent Neural Integration (Phase 5.5)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Regression tests to ensure neural integration doesn't break
 * WHY:  Prevent regressions in SNN/LNN health monitoring across releases
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

// Neural module immune bridge headers
#include "snn/nimcp_snn_immune.h"
#include "lnn/nimcp_lnn_immune.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Fixture for health agent neural regression tests
 */
class HealthAgentNeuralRegressionTest : public ::testing::Test {
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

TEST_F(HealthAgentNeuralRegressionTest, ReturnValues_NullAgentAlwaysReturnsMinusOne) {
    // All neural connect functions MUST return -1 for NULL agent
    // This is a documented behavior that must not change

    EXPECT_EQ(nimcp_health_agent_connect_snn(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_configure_neural(nullptr, nullptr, nullptr), -1);
}

TEST_F(HealthAgentNeuralRegressionTest, ReturnValues_ValidAgentReturnsZero) {
    // All neural connect functions MUST return 0 for valid agent
    // Even with NULL bridge pointers (allowed for lazy connection)

    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, nullptr, nullptr), 0);

    neural_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, ReturnValues_NullMetricsReturnsMinusOne) {
    // get_neural_metrics MUST return -1 for NULL metrics pointer
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, nullptr), -1);
}

//=============================================================================
// Health Score Contract Tests
// These tests verify health score behavior is consistent
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, HealthScore_NullAgentReturns100) {
    // NULL agent MUST return 100.0 (safe default - healthy)
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_neural_health_score(nullptr), 100.0f);
}

TEST_F(HealthAgentNeuralRegressionTest, HealthScore_NoConnectionsReturns100) {
    // No neural connections MUST return 100.0 (nothing to degrade)
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_neural_health_score(agent), 100.0f);
}

TEST_F(HealthAgentNeuralRegressionTest, HealthScore_NullBridgesReturns100) {
    // Connected with NULL bridges MUST return 100.0 (nothing to monitor)
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_neural_health_score(agent), 100.0f);
}

TEST_F(HealthAgentNeuralRegressionTest, HealthScore_AlwaysInValidRange) {
    // Health score MUST always be in [0, 100]
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    float score = nimcp_health_agent_get_neural_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

//=============================================================================
// IsUnhealthy Contract Tests
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, IsUnhealthy_NullAgentReturnsFalse) {
    // NULL agent MUST return false (safe default - not unhealthy)
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(nullptr));
}

TEST_F(HealthAgentNeuralRegressionTest, IsUnhealthy_NoConnectionsReturnsFalse) {
    // No neural connections MUST return false
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
}

TEST_F(HealthAgentNeuralRegressionTest, IsUnhealthy_NullBridgesReturnsFalse) {
    // NULL bridges MUST return false (nothing to be unhealthy)
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
}

//=============================================================================
// Default Configuration Contract Tests
// These tests verify default configurations are applied correctly
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, DefaultConfig_SNN) {
    // When config is NULL, defaults MUST be applied
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);

    // Agent should be functional after connection with defaults
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, DefaultConfig_LNN) {
    // When config is NULL, defaults MUST be applied
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Agent should be functional after connection with defaults
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, DefaultConfig_BothNeural) {
    // Both neural modules with defaults
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Custom Configuration Contract Tests
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, CustomConfig_SNN_Accepted) {
    health_agent_snn_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_snn_monitoring = true;
    custom_config.enable_instability_detection = true;
    custom_config.enable_auto_report = false;
    custom_config.enable_learning_modulation = true;
    custom_config.max_spike_rate_hz = 200.0f;
    custom_config.min_spike_rate_hz = 0.5f;
    custom_config.burst_threshold = 0.6f;
    custom_config.sync_threshold = 0.9f;
    custom_config.check_interval_ms = 50;

    // Custom config MUST be accepted
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &custom_config), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, CustomConfig_LNN_Accepted) {
    health_agent_lnn_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_lnn_monitoring = true;
    custom_config.enable_stability_detection = true;
    custom_config.enable_auto_report = false;
    custom_config.enable_tau_modulation = true;
    custom_config.enable_lr_modulation = false;
    custom_config.state_explosion_threshold = 1e8f;
    custom_config.state_collapse_threshold = 1e-12f;
    custom_config.tau_max = 5000.0f;
    custom_config.tau_min = 0.001f;
    custom_config.gradient_explosion_threshold = 1e5f;
    custom_config.gradient_vanishing_threshold = 1e-9f;
    custom_config.check_interval_ms = 50;

    // Custom config MUST be accepted
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, &custom_config), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Metrics Structure Contract Tests
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, Metrics_SNNFieldsExist) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);

    neural_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));  // Fill with known pattern
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // SNN fields MUST be populated (even if bridge is NULL)
    // With NULL bridge, snn_connected should be false
    EXPECT_FALSE(metrics.snn_connected);
    EXPECT_TRUE(metrics.snn_healthy);  // Default is healthy
}

TEST_F(HealthAgentNeuralRegressionTest, Metrics_LNNFieldsExist) {
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    neural_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // LNN fields MUST be populated
    EXPECT_FALSE(metrics.lnn_connected);
    EXPECT_TRUE(metrics.lnn_healthy);  // Default is healthy
}

TEST_F(HealthAgentNeuralRegressionTest, Metrics_CombinedFieldsExist) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    neural_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // Combined fields MUST be valid
    EXPECT_FALSE(metrics.any_neural_unhealthy);
    EXPECT_GE(metrics.neural_health_score, 0.0f);
    EXPECT_LE(metrics.neural_health_score, 100.0f);
}

//=============================================================================
// Multiple Connections Contract Tests
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, MultipleConnects_SNN_Allowed) {
    // Multiple connections MUST be allowed (reconnection)
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    }
}

TEST_F(HealthAgentNeuralRegressionTest, MultipleConnects_LNN_Allowed) {
    // Multiple connections MUST be allowed (reconnection)
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    }
}

TEST_F(HealthAgentNeuralRegressionTest, MultipleConfigures_Allowed) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Multiple configurations MUST be allowed
    for (int i = 0; i < 5; i++) {
        health_agent_snn_config_t snn_config;
        memset(&snn_config, 0, sizeof(snn_config));
        snn_config.enable_snn_monitoring = true;
        snn_config.check_interval_ms = 50 + i * 10;

        health_agent_lnn_config_t lnn_config;
        memset(&lnn_config, 0, sizeof(lnn_config));
        lnn_config.enable_lnn_monitoring = true;
        lnn_config.check_interval_ms = 50 + i * 10;

        EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_config, &lnn_config), 0);
    }
}

//=============================================================================
// Boundary Value Tests
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, Boundary_ZeroCheckInterval_Accepted) {
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.check_interval_ms = 0;

    // Zero interval MUST be accepted (implementation handles it)
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &snn_config), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, Boundary_MaxThresholds_Accepted) {
    health_agent_lnn_config_t lnn_config;
    memset(&lnn_config, 0, sizeof(lnn_config));
    lnn_config.enable_lnn_monitoring = true;
    lnn_config.state_explosion_threshold = FLT_MAX;
    lnn_config.gradient_explosion_threshold = FLT_MAX;
    lnn_config.tau_max = FLT_MAX;

    // Max thresholds MUST be accepted
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, &lnn_config), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, Boundary_ZeroThresholds_Accepted) {
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.max_spike_rate_hz = 0.0f;
    snn_config.min_spike_rate_hz = 0.0f;

    // Zero thresholds MUST be accepted
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &snn_config), 0);
}

//=============================================================================
// Thread Safety Contract Tests
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, ThreadSafety_ConcurrentMetricsQueries) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    // Multiple threads querying metrics MUST not crash
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

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    should_stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
    EXPECT_EQ(failure_count.load(), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, ThreadSafety_ConcurrentHealthScoreQueries) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::atomic<bool> should_stop{false};
    std::atomic<int> valid_scores{0};
    std::atomic<int> invalid_scores{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &should_stop, &valid_scores, &invalid_scores]() {
            while (!should_stop.load()) {
                float score = nimcp_health_agent_get_neural_health_score(agent);
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

//=============================================================================
// Lifecycle Contract Tests
//=============================================================================

TEST_F(HealthAgentNeuralRegressionTest, Lifecycle_ConnectBeforeStart) {
    // Connection before start MUST work
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, Lifecycle_ConnectAfterStart) {
    // Connection after start MUST work
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, Lifecycle_ConfigureWhileRunning) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Configure while running MUST work
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;

    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_config, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentNeuralRegressionTest, Lifecycle_StartStopRestart) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Start/stop/restart MUST work with neural connections
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
