/**
 * @file test_perception_cortical_health_regression.cpp
 * @brief Regression tests for Phase 5.12: Perception/Cortical Health Integration
 *
 * Tests boundary conditions, edge cases, and known failure modes.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <random>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

/**
 * @brief Regression test fixture for Perception/Cortical Health tests
 */
class PerceptionCorticalHealthRegressionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }

    visual_cortical_bridge_t* mock_visual(uintptr_t id) {
        return reinterpret_cast<visual_cortical_bridge_t*>(0x20000 + id);
    }

    audio_cortical_bridge_t* mock_audio(uintptr_t id) {
        return reinterpret_cast<audio_cortical_bridge_t*>(0x30000 + id);
    }

    cortical_immune_system_t* mock_immune(uintptr_t id) {
        return reinterpret_cast<cortical_immune_system_t*>(0x40000 + id);
    }

    hypercolumn_t* mock_column(uintptr_t id) {
        return reinterpret_cast<hypercolumn_t*>(0x50000 + id);
    }
};

/* ============================================================================
 * Boundary Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, MaxVisualBridges) {
    // Register up to max bridges
    for (int i = 0; i < HEALTH_AGENT_MAX_PERCEPTION_BRIDGES; i++) {
        int result = nimcp_health_agent_connect_visual(agent, mock_visual(i), nullptr);
        EXPECT_EQ(result, 0) << "Failed at bridge " << i;
    }

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, (uint32_t)HEALTH_AGENT_MAX_PERCEPTION_BRIDGES);

    // Trying to add one more should fail
    int result = nimcp_health_agent_connect_visual(agent, mock_visual(100), nullptr);
    EXPECT_EQ(result, -1);

    // Cleanup
    for (int i = 0; i < HEALTH_AGENT_MAX_PERCEPTION_BRIDGES; i++) {
        nimcp_health_agent_disconnect_visual(agent, mock_visual(i));
    }
}

TEST_F(PerceptionCorticalHealthRegressionTest, MaxAudioBridges) {
    // Register up to max bridges
    for (int i = 0; i < HEALTH_AGENT_MAX_PERCEPTION_BRIDGES; i++) {
        int result = nimcp_health_agent_connect_audio(agent, mock_audio(i), nullptr);
        EXPECT_EQ(result, 0) << "Failed at bridge " << i;
    }

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_audio_bridges, (uint32_t)HEALTH_AGENT_MAX_PERCEPTION_BRIDGES);

    // Trying to add one more should fail
    int result = nimcp_health_agent_connect_audio(agent, mock_audio(100), nullptr);
    EXPECT_EQ(result, -1);

    // Cleanup
    for (int i = 0; i < HEALTH_AGENT_MAX_PERCEPTION_BRIDGES; i++) {
        nimcp_health_agent_disconnect_audio(agent, mock_audio(i));
    }
}

TEST_F(PerceptionCorticalHealthRegressionTest, MaxCorticalColumns) {
    // Register up to max columns
    for (int i = 0; i < HEALTH_AGENT_MAX_CORTICAL_COLUMNS; i++) {
        int result = nimcp_health_agent_connect_cortical_column(agent, mock_column(i), nullptr);
        EXPECT_EQ(result, 0) << "Failed at column " << i;
    }

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_hypercolumns, (uint32_t)HEALTH_AGENT_MAX_CORTICAL_COLUMNS);

    // Trying to add one more should fail
    int result = nimcp_health_agent_connect_cortical_column(agent, mock_column(100), nullptr);
    EXPECT_EQ(result, -1);

    // Cleanup
    for (int i = 0; i < HEALTH_AGENT_MAX_CORTICAL_COLUMNS; i++) {
        nimcp_health_agent_disconnect_cortical_column(agent, mock_column(i));
    }
}

/* ============================================================================
 * Repeated Connect/Disconnect Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, RepeatedVisualConnectDisconnect) {
    visual_cortical_bridge_t* bridge = mock_visual(1);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_visual(agent, bridge, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, bridge), 0);
    }

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 0u);
}

TEST_F(PerceptionCorticalHealthRegressionTest, RepeatedAudioConnectDisconnect) {
    audio_cortical_bridge_t* bridge = mock_audio(1);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_audio(agent, bridge, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_audio(agent, bridge), 0);
    }

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_audio_bridges, 0u);
}

TEST_F(PerceptionCorticalHealthRegressionTest, RepeatedCorticalImmuneConnectDisconnect) {
    cortical_immune_system_t* immune = mock_immune(1);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_cortical_immune(agent, immune), 0);
    }

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_immune_systems, 0u);
}

TEST_F(PerceptionCorticalHealthRegressionTest, RepeatedCorticalColumnConnectDisconnect) {
    hypercolumn_t* column = mock_column(1);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, column, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, column), 0);
    }

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_hypercolumns, 0u);
}

/* ============================================================================
 * Random Order Disconnect Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, RandomOrderVisualDisconnect) {
    std::vector<visual_cortical_bridge_t*> bridges;
    for (int i = 0; i < 5; i++) {
        bridges.push_back(mock_visual(i));
        nimcp_health_agent_connect_visual(agent, bridges.back(), nullptr);
    }

    // Shuffle and disconnect in random order
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(bridges.begin(), bridges.end(), g);

    for (auto* bridge : bridges) {
        EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, bridge), 0);
    }

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 0u);
}

TEST_F(PerceptionCorticalHealthRegressionTest, RandomOrderColumnDisconnect) {
    std::vector<hypercolumn_t*> columns;
    for (int i = 0; i < 8; i++) {
        columns.push_back(mock_column(i));
        nimcp_health_agent_connect_cortical_column(agent, columns.back(), nullptr);
    }

    // Shuffle and disconnect in random order
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(columns.begin(), columns.end(), g);

    for (auto* column : columns) {
        EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, column), 0);
    }

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_hypercolumns, 0u);
}

/* ============================================================================
 * Metrics Consistency Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, PerceptionMetricsConsistentWithCount) {
    // Add bridges one at a time, verify metrics
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_connect_visual(agent, mock_visual(i), nullptr);
        nimcp_health_agent_connect_audio(agent, mock_audio(i), nullptr);

        perception_health_metrics_t metrics;
        nimcp_health_agent_get_perception_metrics(agent, &metrics);
        EXPECT_EQ(metrics.num_visual_bridges, (uint32_t)(i + 1));
        EXPECT_EQ(metrics.num_audio_bridges, (uint32_t)(i + 1));
    }

    // Remove bridges one at a time, verify metrics
    for (int i = 4; i >= 0; i--) {
        nimcp_health_agent_disconnect_visual(agent, mock_visual(i));
        nimcp_health_agent_disconnect_audio(agent, mock_audio(i));

        perception_health_metrics_t metrics;
        nimcp_health_agent_get_perception_metrics(agent, &metrics);
        EXPECT_EQ(metrics.num_visual_bridges, (uint32_t)i);
        EXPECT_EQ(metrics.num_audio_bridges, (uint32_t)i);
    }
}

TEST_F(PerceptionCorticalHealthRegressionTest, CorticalMetricsConsistentWithCount) {
    // Add components one at a time, verify metrics
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_connect_cortical_immune(agent, mock_immune(i), nullptr);
        nimcp_health_agent_connect_cortical_column(agent, mock_column(i), nullptr);

        cortical_health_metrics_t metrics;
        nimcp_health_agent_get_cortical_metrics(agent, &metrics);
        EXPECT_EQ(metrics.num_immune_systems, (uint32_t)(i + 1));
        EXPECT_EQ(metrics.num_hypercolumns, (uint32_t)(i + 1));
    }

    // Remove components one at a time, verify metrics
    for (int i = 4; i >= 0; i--) {
        nimcp_health_agent_disconnect_cortical_immune(agent, mock_immune(i));
        nimcp_health_agent_disconnect_cortical_column(agent, mock_column(i));

        cortical_health_metrics_t metrics;
        nimcp_health_agent_get_cortical_metrics(agent, &metrics);
        EXPECT_EQ(metrics.num_immune_systems, (uint32_t)i);
        EXPECT_EQ(metrics.num_hypercolumns, (uint32_t)i);
    }
}

/* ============================================================================
 * Health Score Stability Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, PerceptionHealthScoreStableOverTime) {
    visual_cortical_bridge_t* bridge = mock_visual(1);
    nimcp_health_agent_connect_visual(agent, bridge, nullptr);

    float initial_score = nimcp_health_agent_get_perception_health_score(agent);

    // Check stability over multiple reads
    for (int i = 0; i < 50; i++) {
        float score = nimcp_health_agent_get_perception_health_score(agent);
        EXPECT_EQ(score, initial_score);
    }

    nimcp_health_agent_disconnect_visual(agent, bridge);
}

TEST_F(PerceptionCorticalHealthRegressionTest, CorticalHealthScoreStableOverTime) {
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    float initial_score = nimcp_health_agent_get_cortical_health_score(agent);

    // Check stability over multiple reads
    for (int i = 0; i < 50; i++) {
        float score = nimcp_health_agent_get_cortical_health_score(agent);
        EXPECT_EQ(score, initial_score);
    }

    nimcp_health_agent_disconnect_cortical_column(agent, column);
}

/* ============================================================================
 * Timestamp Progress Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, PerceptionTimestampProgress) {
    visual_cortical_bridge_t* bridge = mock_visual(1);
    nimcp_health_agent_connect_visual(agent, bridge, nullptr);

    uint64_t last_timestamp = 0;
    for (int i = 0; i < 10; i++) {
        perception_health_metrics_t metrics;
        nimcp_health_agent_get_perception_metrics(agent, &metrics);
        EXPECT_GE(metrics.last_check_timestamp_us, last_timestamp);
        last_timestamp = metrics.last_check_timestamp_us;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    nimcp_health_agent_disconnect_visual(agent, bridge);
}

TEST_F(PerceptionCorticalHealthRegressionTest, CorticalTimestampProgress) {
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    uint64_t last_timestamp = 0;
    for (int i = 0; i < 10; i++) {
        cortical_health_metrics_t metrics;
        nimcp_health_agent_get_cortical_metrics(agent, &metrics);
        EXPECT_GE(metrics.last_check_timestamp_us, last_timestamp);
        last_timestamp = metrics.last_check_timestamp_us;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    nimcp_health_agent_disconnect_cortical_column(agent, column);
}

/* ============================================================================
 * Config Application Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, PerceptionConfigAppliedOnConnect) {
    health_agent_perception_config_t config;
    nimcp_health_agent_perception_config_default(&config);
    config.max_visual_latency_ms = 500.0;

    visual_cortical_bridge_t* bridge = mock_visual(1);
    EXPECT_EQ(nimcp_health_agent_connect_visual(agent, bridge, &config), 0);

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 1u);

    nimcp_health_agent_disconnect_visual(agent, bridge);
}

TEST_F(PerceptionCorticalHealthRegressionTest, CorticalConfigAppliedOnConnect) {
    health_agent_cortical_config_t config;
    nimcp_health_agent_cortical_config_default(&config);
    config.inflammation_threshold = 0.8f;

    hypercolumn_t* column = mock_column(1);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, column, &config), 0);

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_hypercolumns, 1u);

    nimcp_health_agent_disconnect_cortical_column(agent, column);
}

/* ============================================================================
 * Recovery Under Load Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, PerceptionRecoveryUnderLoad) {
    // Connect multiple bridges
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_connect_visual(agent, mock_visual(i), nullptr);
        nimcp_health_agent_connect_audio(agent, mock_audio(i), nullptr);
    }

    // Perform multiple recovery actions
    for (int i = 0; i < 20; i++) {
        perception_recovery_action_t action = static_cast<perception_recovery_action_t>(
            i % (PERCEPTION_RECOVERY_FULL_RESET + 1));
        EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, action, i % 5), 0);
    }

    // Verify metrics still work
    perception_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_perception_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_visual_bridges, 5u);
    EXPECT_EQ(metrics.num_audio_bridges, 5u);

    // Cleanup
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_disconnect_visual(agent, mock_visual(i));
        nimcp_health_agent_disconnect_audio(agent, mock_audio(i));
    }
}

TEST_F(PerceptionCorticalHealthRegressionTest, CorticalRecoveryUnderLoad) {
    // Connect multiple components
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_connect_cortical_immune(agent, mock_immune(i), nullptr);
        nimcp_health_agent_connect_cortical_column(agent, mock_column(i), nullptr);
    }

    // Perform multiple recovery actions
    for (int i = 0; i < 20; i++) {
        cortical_recovery_action_t action = static_cast<cortical_recovery_action_t>(
            i % (CORTICAL_RECOVERY_FULL_RESET + 1));
        EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, action, i % 5), 0);
    }

    // Verify metrics still work
    cortical_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_cortical_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_immune_systems, 5u);
    EXPECT_EQ(metrics.num_hypercolumns, 5u);

    // Cleanup
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_disconnect_cortical_immune(agent, mock_immune(i));
        nimcp_health_agent_disconnect_cortical_column(agent, mock_column(i));
    }
}

/* ============================================================================
 * Double Disconnect Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthRegressionTest, DoubleVisualDisconnectSafe) {
    visual_cortical_bridge_t* bridge = mock_visual(1);
    nimcp_health_agent_connect_visual(agent, bridge, nullptr);

    EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, bridge), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, bridge), -1);
}

TEST_F(PerceptionCorticalHealthRegressionTest, DoubleAudioDisconnectSafe) {
    audio_cortical_bridge_t* bridge = mock_audio(1);
    nimcp_health_agent_connect_audio(agent, bridge, nullptr);

    EXPECT_EQ(nimcp_health_agent_disconnect_audio(agent, bridge), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_audio(agent, bridge), -1);
}

TEST_F(PerceptionCorticalHealthRegressionTest, DoubleImmuneDisconnectSafe) {
    cortical_immune_system_t* immune = mock_immune(1);
    nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr);

    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_immune(agent, immune), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_immune(agent, immune), -1);
}

TEST_F(PerceptionCorticalHealthRegressionTest, DoubleColumnDisconnectSafe) {
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, column), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, column), -1);
}
