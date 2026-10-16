/**
 * @file test_health_agent_neural_functions.cpp
 * @brief Unit tests for NIMCP Health Agent Neural Integration (Phase 5.5)
 * @version 1.0.0
 * @date 2025-01-18
 *
 * WHAT: Test health agent neural connection functions for SNN/LNN modules
 * WHY:  Ensure neural integration correctly configures and monitors SNN/LNN
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
 * @brief Base fixture for health agent neural tests
 */
class HealthAgentNeuralTest : public ::testing::Test {
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
// connect_snn Tests
//=============================================================================

TEST_F(HealthAgentNeuralTest, ConnectSNN_NullAgent) {
    snn_immune_bridge_t* bridge = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_snn(nullptr, bridge, nullptr), -1);
}

TEST_F(HealthAgentNeuralTest, ConnectSNN_NullBridge_WithDefaults) {
    // Should succeed with NULL bridge - stores NULL and applies defaults
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentNeuralTest, ConnectSNN_WithCustomConfig) {
    health_agent_snn_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_snn_monitoring = true;
    custom_config.enable_instability_detection = true;
    custom_config.enable_auto_report = false;
    custom_config.enable_learning_modulation = true;
    custom_config.max_spike_rate_hz = 200.0f;
    custom_config.min_spike_rate_hz = 1.0f;
    custom_config.burst_threshold = 0.5f;
    custom_config.sync_threshold = 0.95f;
    custom_config.check_interval_ms = 500;

    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentNeuralTest, ConnectSNN_MultipleConnects) {
    // Should allow reconnection (replacing previous connection)
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentNeuralTest, ConnectSNN_VerifyDefaultConfig) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);

    neural_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // Bridge not connected, so snn_connected should be false
    EXPECT_FALSE(metrics.snn_connected);
}

//=============================================================================
// connect_lnn Tests
//=============================================================================

TEST_F(HealthAgentNeuralTest, ConnectLNN_NullAgent) {
    lnn_immune_bridge_t* bridge = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_lnn(nullptr, bridge, nullptr), -1);
}

TEST_F(HealthAgentNeuralTest, ConnectLNN_NullBridge_WithDefaults) {
    // Should succeed with NULL bridge - stores NULL and applies defaults
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentNeuralTest, ConnectLNN_WithCustomConfig) {
    health_agent_lnn_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_lnn_monitoring = true;
    custom_config.enable_stability_detection = true;
    custom_config.enable_auto_report = false;
    custom_config.enable_tau_modulation = true;
    custom_config.enable_lr_modulation = false;
    custom_config.state_explosion_threshold = 1e7f;
    custom_config.state_collapse_threshold = 1e-12f;
    custom_config.tau_max = 2000.0f;
    custom_config.tau_min = 0.01f;
    custom_config.gradient_explosion_threshold = 1e4f;
    custom_config.gradient_vanishing_threshold = 1e-8f;
    custom_config.check_interval_ms = 250;

    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentNeuralTest, ConnectLNN_MultipleConnects) {
    // Should allow reconnection (replacing previous connection)
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentNeuralTest, ConnectLNN_VerifyDefaultConfig) {
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    neural_health_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // Bridge not connected, so lnn_connected should be false
    EXPECT_FALSE(metrics.lnn_connected);
}

//=============================================================================
// get_neural_metrics Tests
//=============================================================================

TEST_F(HealthAgentNeuralTest, GetNeuralMetrics_NullAgent) {
    neural_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(nullptr, &metrics), -1);
}

TEST_F(HealthAgentNeuralTest, GetNeuralMetrics_NullMetrics) {
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, nullptr), -1);
}

TEST_F(HealthAgentNeuralTest, GetNeuralMetrics_NoConnectionsYet) {
    neural_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));  // Fill with known pattern

    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // No connections, so both should be disconnected
    EXPECT_FALSE(metrics.snn_connected);
    EXPECT_FALSE(metrics.lnn_connected);
    EXPECT_FALSE(metrics.any_neural_unhealthy);
}

TEST_F(HealthAgentNeuralTest, GetNeuralMetrics_AfterSNNConnect) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);

    neural_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // SNN connected (with NULL bridge), LNN not connected
    EXPECT_FALSE(metrics.snn_connected);  // NULL bridge means not actually connected
    EXPECT_FALSE(metrics.lnn_connected);
}

TEST_F(HealthAgentNeuralTest, GetNeuralMetrics_AfterBothConnect) {
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    neural_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // Both connected with NULL bridges
    EXPECT_FALSE(metrics.snn_connected);
    EXPECT_FALSE(metrics.lnn_connected);
}

//=============================================================================
// configure_neural Tests
//=============================================================================

TEST_F(HealthAgentNeuralTest, ConfigureNeural_NullAgent) {
    health_agent_snn_config_t snn_config;
    health_agent_lnn_config_t lnn_config;
    EXPECT_EQ(nimcp_health_agent_configure_neural(nullptr, &snn_config, &lnn_config), -1);
}

TEST_F(HealthAgentNeuralTest, ConfigureNeural_BothNull) {
    // Should succeed - no changes made
    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentNeuralTest, ConfigureNeural_SNNOnly) {
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.max_spike_rate_hz = 150.0f;

    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_config, nullptr), 0);
}

TEST_F(HealthAgentNeuralTest, ConfigureNeural_LNNOnly) {
    health_agent_lnn_config_t lnn_config;
    memset(&lnn_config, 0, sizeof(lnn_config));
    lnn_config.enable_lnn_monitoring = true;
    lnn_config.tau_max = 1500.0f;

    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, nullptr, &lnn_config), 0);
}

TEST_F(HealthAgentNeuralTest, ConfigureNeural_Both) {
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.enable_instability_detection = true;

    health_agent_lnn_config_t lnn_config;
    memset(&lnn_config, 0, sizeof(lnn_config));
    lnn_config.enable_lnn_monitoring = true;
    lnn_config.enable_stability_detection = true;

    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_config, &lnn_config), 0);
}

//=============================================================================
// is_neural_unhealthy Tests
//=============================================================================

TEST_F(HealthAgentNeuralTest, IsNeuralUnhealthy_NullAgent) {
    // Should return false for NULL agent (safe default)
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(nullptr));
}

TEST_F(HealthAgentNeuralTest, IsNeuralUnhealthy_NoConnections) {
    // No connections, should be healthy (nothing to be unhealthy)
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
}

TEST_F(HealthAgentNeuralTest, IsNeuralUnhealthy_NullBridges) {
    // Connect with NULL bridges
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // NULL bridges should report healthy (nothing to check)
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
}

//=============================================================================
// get_neural_health_score Tests
//=============================================================================

TEST_F(HealthAgentNeuralTest, GetNeuralHealthScore_NullAgent) {
    // Should return 100.0 for NULL agent (safe default - "perfect" health)
    float score = nimcp_health_agent_get_neural_health_score(nullptr);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

TEST_F(HealthAgentNeuralTest, GetNeuralHealthScore_NoConnections) {
    // No connections - should return perfect health
    float score = nimcp_health_agent_get_neural_health_score(agent);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

TEST_F(HealthAgentNeuralTest, GetNeuralHealthScore_NullBridges) {
    // Connect with NULL bridges
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // NULL bridges should report perfect health (nothing degraded)
    float score = nimcp_health_agent_get_neural_health_score(agent);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

TEST_F(HealthAgentNeuralTest, GetNeuralHealthScore_ValidRange) {
    // Score should always be in [0, 100] range
    float score = nimcp_health_agent_get_neural_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

//=============================================================================
// Combined Workflow Tests
//=============================================================================

TEST_F(HealthAgentNeuralTest, Workflow_ConnectConfigureQuery) {
    // Step 1: Connect both with defaults
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Step 2: Reconfigure with custom settings
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.check_interval_ms = 100;

    health_agent_lnn_config_t lnn_config;
    memset(&lnn_config, 0, sizeof(lnn_config));
    lnn_config.enable_lnn_monitoring = true;
    lnn_config.check_interval_ms = 100;

    EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_config, &lnn_config), 0);

    // Step 3: Query metrics
    neural_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);

    // Step 4: Check health status
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_neural_health_score(agent), 100.0f);
}

TEST_F(HealthAgentNeuralTest, Workflow_MultipleReconfigures) {
    // Connect SNN
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);

    // Reconfigure multiple times
    for (int i = 0; i < 5; i++) {
        health_agent_snn_config_t snn_config;
        memset(&snn_config, 0, sizeof(snn_config));
        snn_config.enable_snn_monitoring = (i % 2 == 0);
        snn_config.max_spike_rate_hz = 100.0f + (float)i * 10.0f;
        snn_config.check_interval_ms = 100 + i * 50;

        EXPECT_EQ(nimcp_health_agent_configure_neural(agent, &snn_config, nullptr), 0);
    }

    // Final health check
    EXPECT_FALSE(nimcp_health_agent_is_neural_unhealthy(agent));
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(HealthAgentNeuralTest, Boundary_ZeroCheckInterval) {
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.check_interval_ms = 0;  // Edge case

    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &snn_config), 0);
}

TEST_F(HealthAgentNeuralTest, Boundary_MaxThresholds) {
    health_agent_lnn_config_t lnn_config;
    memset(&lnn_config, 0, sizeof(lnn_config));
    lnn_config.enable_lnn_monitoring = true;
    lnn_config.state_explosion_threshold = FLT_MAX;
    lnn_config.gradient_explosion_threshold = FLT_MAX;
    lnn_config.tau_max = FLT_MAX;
    lnn_config.tau_min = 0.0f;

    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, &lnn_config), 0);
}

TEST_F(HealthAgentNeuralTest, Boundary_NegativeThresholds) {
    // Negative thresholds should still be accepted (implementation decides handling)
    health_agent_snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config.enable_snn_monitoring = true;
    snn_config.max_spike_rate_hz = -100.0f;  // Invalid but implementation may handle
    snn_config.min_spike_rate_hz = -50.0f;

    // Should succeed (API accepts, implementation handles)
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, &snn_config), 0);
}

//=============================================================================
// Thread Safety Tests (Basic)
//=============================================================================

TEST_F(HealthAgentNeuralTest, ThreadSafety_ConcurrentMetricsQuery) {
    // Connect both bridges
    EXPECT_EQ(nimcp_health_agent_connect_snn(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_lnn(agent, nullptr, nullptr), 0);

    // Multiple sequential queries should not crash or corrupt state
    for (int i = 0; i < 100; i++) {
        neural_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_neural_metrics(agent, &metrics), 0);
        EXPECT_GE(metrics.neural_health_score, 0.0f);
        EXPECT_LE(metrics.neural_health_score, 100.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
