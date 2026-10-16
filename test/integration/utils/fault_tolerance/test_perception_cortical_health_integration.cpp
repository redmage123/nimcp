/**
 * @file test_perception_cortical_health_integration.cpp
 * @brief Integration tests for Phase 5.12: Perception/Cortical Health Integration
 *
 * Tests the integration of health agent with perception and cortical components.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

/**
 * @brief Integration test fixture for Perception/Cortical Health tests
 */
class PerceptionCorticalHealthIntegrationTest : public ::testing::Test {
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
 * Multi-Component Registration Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthIntegrationTest, RegisterMultipleVisualAndAudioBridges) {
    // Register multiple visual bridges
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_visual(agent, mock_visual(i), nullptr), 0);
    }

    // Register multiple audio bridges
    for (int i = 0; i < 2; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_audio(agent, mock_audio(i), nullptr), 0);
    }

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 3u);
    EXPECT_EQ(metrics.num_audio_bridges, 2u);

    // Disconnect all
    for (int i = 0; i < 3; i++) {
        nimcp_health_agent_disconnect_visual(agent, mock_visual(i));
    }
    for (int i = 0; i < 2; i++) {
        nimcp_health_agent_disconnect_audio(agent, mock_audio(i));
    }

    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 0u);
    EXPECT_EQ(metrics.num_audio_bridges, 0u);
}

TEST_F(PerceptionCorticalHealthIntegrationTest, RegisterMultipleImmuneAndColumns) {
    // Register immune systems
    for (int i = 0; i < 2; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_cortical_immune(agent, mock_immune(i), nullptr), 0);
    }

    // Register columns
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, mock_column(i), nullptr), 0);
    }

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_immune_systems, 2u);
    EXPECT_EQ(metrics.num_hypercolumns, 4u);

    // Disconnect all
    for (int i = 0; i < 2; i++) {
        nimcp_health_agent_disconnect_cortical_immune(agent, mock_immune(i));
    }
    for (int i = 0; i < 4; i++) {
        nimcp_health_agent_disconnect_cortical_column(agent, mock_column(i));
    }

    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_immune_systems, 0u);
    EXPECT_EQ(metrics.num_hypercolumns, 0u);
}

/* ============================================================================
 * Health Check Cycle Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthIntegrationTest, PerceptionHealthCheckCycle) {
    visual_cortical_bridge_t* bridge = mock_visual(1);
    nimcp_health_agent_connect_visual(agent, bridge, nullptr);

    // Run multiple health check cycles
    for (int i = 0; i < 5; i++) {
        perception_health_metrics_t metrics;
        int result = nimcp_health_agent_get_perception_metrics(agent, &metrics);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(metrics.num_visual_bridges, 1u);
        EXPECT_GT(metrics.last_check_timestamp_us, 0u);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    nimcp_health_agent_disconnect_visual(agent, bridge);
}

TEST_F(PerceptionCorticalHealthIntegrationTest, CorticalHealthCheckCycle) {
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    // Run multiple health check cycles
    for (int i = 0; i < 5; i++) {
        cortical_health_metrics_t metrics;
        int result = nimcp_health_agent_get_cortical_metrics(agent, &metrics);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(metrics.num_hypercolumns, 1u);
        EXPECT_GT(metrics.last_check_timestamp_us, 0u);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    nimcp_health_agent_disconnect_cortical_column(agent, column);
}

/* ============================================================================
 * Configuration Update Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthIntegrationTest, RuntimePerceptionConfigUpdate) {
    visual_cortical_bridge_t* bridge = mock_visual(1);
    nimcp_health_agent_connect_visual(agent, bridge, nullptr);

    // Get initial metrics
    perception_health_metrics_t metrics1;
    nimcp_health_agent_get_perception_metrics(agent, &metrics1);

    // Update configuration
    health_agent_perception_config_t new_config;
    nimcp_health_agent_perception_config_default(&new_config);
    new_config.max_visual_latency_ms = 200.0;
    new_config.max_audio_latency_ms = 100.0;
    EXPECT_EQ(nimcp_health_agent_update_perception_config(agent, &new_config), 0);

    // Get metrics after config update
    perception_health_metrics_t metrics2;
    nimcp_health_agent_get_perception_metrics(agent, &metrics2);
    EXPECT_GE(metrics2.last_check_timestamp_us, metrics1.last_check_timestamp_us);

    nimcp_health_agent_disconnect_visual(agent, bridge);
}

TEST_F(PerceptionCorticalHealthIntegrationTest, RuntimeCorticalConfigUpdate) {
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    // Get initial metrics
    cortical_health_metrics_t metrics1;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics1);

    // Update configuration
    health_agent_cortical_config_t new_config;
    nimcp_health_agent_cortical_config_default(&new_config);
    new_config.min_layer_health = 0.7f;
    new_config.inflammation_threshold = 0.4f;
    EXPECT_EQ(nimcp_health_agent_update_cortical_config(agent, &new_config), 0);

    // Get metrics after config update
    cortical_health_metrics_t metrics2;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics2);
    EXPECT_GE(metrics2.last_check_timestamp_us, metrics1.last_check_timestamp_us);

    nimcp_health_agent_disconnect_cortical_column(agent, column);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthIntegrationTest, ConcurrentPerceptionAccess) {
    visual_cortical_bridge_t* bridge = mock_visual(1);
    nimcp_health_agent_connect_visual(agent, bridge, nullptr);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Launch multiple threads accessing perception health
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 25; j++) {
                perception_health_metrics_t metrics;
                if (nimcp_health_agent_get_perception_metrics(agent, &metrics) == 0) {
                    success_count++;
                }

                float score = nimcp_health_agent_get_perception_health_score(agent);
                if (score >= 0.0f && score <= 100.0f) {
                    success_count++;
                }

                bool needs_attention = nimcp_health_agent_perception_needs_attention(agent);
                (void)needs_attention;
                success_count++;
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // All operations should succeed
    EXPECT_EQ(success_count.load(), 4 * 25 * 3);

    nimcp_health_agent_disconnect_visual(agent, bridge);
}

TEST_F(PerceptionCorticalHealthIntegrationTest, ConcurrentCorticalAccess) {
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Launch multiple threads accessing cortical health
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 25; j++) {
                cortical_health_metrics_t metrics;
                if (nimcp_health_agent_get_cortical_metrics(agent, &metrics) == 0) {
                    success_count++;
                }

                float score = nimcp_health_agent_get_cortical_health_score(agent);
                if (score >= 0.0f && score <= 100.0f) {
                    success_count++;
                }

                bool needs_attention = nimcp_health_agent_cortical_needs_attention(agent);
                (void)needs_attention;
                success_count++;
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // All operations should succeed
    EXPECT_EQ(success_count.load(), 4 * 25 * 3);

    nimcp_health_agent_disconnect_cortical_column(agent, column);
}

/* ============================================================================
 * Recovery Integration Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthIntegrationTest, PerceptionRecoverySequence) {
    visual_cortical_bridge_t* bridge = mock_visual(1);
    nimcp_health_agent_connect_visual(agent, bridge, nullptr);

    // Perform recovery sequence
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_FLUSH_BUFFERS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_RESET_GAIN, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_REBUILD_MAPS, 0), 0);

    // Verify health metrics still work
    perception_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_perception_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_visual_bridges, 1u);

    nimcp_health_agent_disconnect_visual(agent, bridge);
}

TEST_F(PerceptionCorticalHealthIntegrationTest, CorticalRecoverySequence) {
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    // Perform recovery sequence
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_NORMALIZE_ACTIVITY, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_REBALANCE_INHIBITION, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_SHARPEN_TUNING, 0), 0);

    // Verify health metrics still work
    cortical_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_cortical_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_hypercolumns, 1u);

    nimcp_health_agent_disconnect_cortical_column(agent, column);
}

/* ============================================================================
 * Mixed Perception/Cortical Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthIntegrationTest, MixedPerceptionCorticalOperations) {
    // Connect perception components
    visual_cortical_bridge_t* visual = mock_visual(1);
    audio_cortical_bridge_t* audio = mock_audio(1);
    nimcp_health_agent_connect_visual(agent, visual, nullptr);
    nimcp_health_agent_connect_audio(agent, audio, nullptr);

    // Connect cortical components
    cortical_immune_system_t* immune = mock_immune(1);
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    // Get both metrics
    perception_health_metrics_t perc_metrics;
    cortical_health_metrics_t cort_metrics;
    EXPECT_EQ(nimcp_health_agent_get_perception_metrics(agent, &perc_metrics), 0);
    EXPECT_EQ(nimcp_health_agent_get_cortical_metrics(agent, &cort_metrics), 0);

    EXPECT_EQ(perc_metrics.num_visual_bridges, 1u);
    EXPECT_EQ(perc_metrics.num_audio_bridges, 1u);
    EXPECT_EQ(cort_metrics.num_immune_systems, 1u);
    EXPECT_EQ(cort_metrics.num_hypercolumns, 1u);

    // Get both health scores
    float perc_score = nimcp_health_agent_get_perception_health_score(agent);
    float cort_score = nimcp_health_agent_get_cortical_health_score(agent);
    EXPECT_GE(perc_score, 0.0f);
    EXPECT_LE(perc_score, 100.0f);
    EXPECT_GE(cort_score, 0.0f);
    EXPECT_LE(cort_score, 100.0f);

    // Disconnect all
    nimcp_health_agent_disconnect_visual(agent, visual);
    nimcp_health_agent_disconnect_audio(agent, audio);
    nimcp_health_agent_disconnect_cortical_immune(agent, immune);
    nimcp_health_agent_disconnect_cortical_column(agent, column);
}
