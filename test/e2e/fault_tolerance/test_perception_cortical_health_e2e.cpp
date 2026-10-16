/**
 * @file test_perception_cortical_health_e2e.cpp
 * @brief End-to-end tests for Phase 5.12: Perception/Cortical Health Integration
 *
 * Tests complete lifecycle scenarios for perception and cortical health monitoring.
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
 * @brief E2E test fixture for Perception/Cortical Health tests
 */
class PerceptionCorticalHealthE2ETest : public ::testing::Test {
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
 * Full Lifecycle Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthE2ETest, PerceptionSystemFullLifecycle) {
    // Phase 1: Configure
    health_agent_perception_config_t config;
    nimcp_health_agent_perception_config_default(&config);
    config.max_visual_latency_ms = 150.0;
    config.max_audio_latency_ms = 75.0;
    config.enable_auto_recovery = true;

    // Phase 2: Connect visual bridges
    visual_cortical_bridge_t* visual1 = mock_visual(1);
    visual_cortical_bridge_t* visual2 = mock_visual(2);
    EXPECT_EQ(nimcp_health_agent_connect_visual(agent, visual1, &config), 0);
    EXPECT_EQ(nimcp_health_agent_connect_visual(agent, visual2, nullptr), 0);

    // Phase 3: Connect audio bridges
    audio_cortical_bridge_t* audio1 = mock_audio(1);
    EXPECT_EQ(nimcp_health_agent_connect_audio(agent, audio1, nullptr), 0);

    // Phase 4: Monitor health
    for (int i = 0; i < 5; i++) {
        perception_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_perception_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_visual_bridges, 2u);
        EXPECT_EQ(metrics.num_audio_bridges, 1u);
        EXPECT_FALSE(metrics.any_bridge_unhealthy);

        float score = nimcp_health_agent_get_perception_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        bool needs_attention = nimcp_health_agent_perception_needs_attention(agent);
        (void)needs_attention;

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Phase 5: Perform recovery actions
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_FLUSH_BUFFERS, -1), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_REBUILD_MAPS, 0), 0);

    // Phase 6: Update configuration
    config.max_visual_latency_ms = 200.0;
    EXPECT_EQ(nimcp_health_agent_update_perception_config(agent, &config), 0);

    // Phase 7: Disconnect bridges
    EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, visual1), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, visual2), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_audio(agent, audio1), 0);

    // Phase 8: Verify cleanup
    perception_health_metrics_t final_metrics;
    nimcp_health_agent_get_perception_metrics(agent, &final_metrics);
    EXPECT_EQ(final_metrics.num_visual_bridges, 0u);
    EXPECT_EQ(final_metrics.num_audio_bridges, 0u);
}

TEST_F(PerceptionCorticalHealthE2ETest, CorticalSystemFullLifecycle) {
    // Phase 1: Configure
    health_agent_cortical_config_t config;
    nimcp_health_agent_cortical_config_default(&config);
    config.inflammation_threshold = 0.6f;
    config.enable_auto_recovery = true;

    // Phase 2: Connect cortical immune systems
    cortical_immune_system_t* immune1 = mock_immune(1);
    cortical_immune_system_t* immune2 = mock_immune(2);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_immune(agent, immune1, &config), 0);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_immune(agent, immune2, nullptr), 0);

    // Phase 3: Connect cortical columns
    hypercolumn_t* col1 = mock_column(1);
    hypercolumn_t* col2 = mock_column(2);
    hypercolumn_t* col3 = mock_column(3);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, col1, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, col2, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, col3, nullptr), 0);

    // Phase 4: Monitor health
    for (int i = 0; i < 5; i++) {
        cortical_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_cortical_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_immune_systems, 2u);
        EXPECT_EQ(metrics.num_hypercolumns, 3u);
        EXPECT_FALSE(metrics.any_column_unhealthy);
        EXPECT_TRUE(metrics.immune_system_active);

        float score = nimcp_health_agent_get_cortical_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        bool needs_attention = nimcp_health_agent_cortical_needs_attention(agent);
        (void)needs_attention;

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Phase 5: Perform recovery actions
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_NORMALIZE_ACTIVITY, -1), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_REDUCE_INFLAMMATION, -1), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_SHARPEN_TUNING, 0), 0);

    // Phase 6: Update configuration
    config.inflammation_threshold = 0.5f;
    EXPECT_EQ(nimcp_health_agent_update_cortical_config(agent, &config), 0);

    // Phase 7: Disconnect components
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_immune(agent, immune1), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_immune(agent, immune2), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, col1), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, col2), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, col3), 0);

    // Phase 8: Verify cleanup
    cortical_health_metrics_t final_metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &final_metrics);
    EXPECT_EQ(final_metrics.num_immune_systems, 0u);
    EXPECT_EQ(final_metrics.num_hypercolumns, 0u);
}

/* ============================================================================
 * Combined System Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthE2ETest, CombinedPerceptionCorticalLifecycle) {
    // Connect perception components
    visual_cortical_bridge_t* visual = mock_visual(1);
    audio_cortical_bridge_t* audio = mock_audio(1);
    EXPECT_EQ(nimcp_health_agent_connect_visual(agent, visual, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_audio(agent, audio, nullptr), 0);

    // Connect cortical components
    cortical_immune_system_t* immune = mock_immune(1);
    hypercolumn_t* column = mock_column(1);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, column, nullptr), 0);

    // Monitor both systems
    for (int i = 0; i < 10; i++) {
        perception_health_metrics_t perc_metrics;
        cortical_health_metrics_t cort_metrics;

        EXPECT_EQ(nimcp_health_agent_get_perception_metrics(agent, &perc_metrics), 0);
        EXPECT_EQ(nimcp_health_agent_get_cortical_metrics(agent, &cort_metrics), 0);

        float perc_score = nimcp_health_agent_get_perception_health_score(agent);
        float cort_score = nimcp_health_agent_get_cortical_health_score(agent);

        EXPECT_GE(perc_score, 0.0f);
        EXPECT_LE(perc_score, 100.0f);
        EXPECT_GE(cort_score, 0.0f);
        EXPECT_LE(cort_score, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Perform coordinated recovery
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_SOFT_RESET, -1), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_SOFT_RESET, -1), 0);

    // Disconnect all
    EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, visual), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_audio(agent, audio), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_immune(agent, immune), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, column), 0);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthE2ETest, PerceptionStressTest) {
    // Fill up with max bridges
    for (int i = 0; i < HEALTH_AGENT_MAX_PERCEPTION_BRIDGES; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_visual(agent, mock_visual(i), nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_audio(agent, mock_audio(i), nullptr), 0);
    }

    // Stress test with concurrent operations
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count]() {
            for (int i = 0; i < 50; i++) {
                perception_health_metrics_t metrics;
                if (nimcp_health_agent_get_perception_metrics(agent, &metrics) == 0) {
                    success_count++;
                }

                float score = nimcp_health_agent_get_perception_health_score(agent);
                if (score >= 0.0f && score <= 100.0f) {
                    success_count++;
                }

                // Mix in some recovery actions
                if (i % 10 == 0) {
                    perception_recovery_action_t action = static_cast<perception_recovery_action_t>(
                        i % (PERCEPTION_RECOVERY_FULL_RESET + 1));
                    if (nimcp_health_agent_perception_recovery(agent, action, -1) == 0) {
                        success_count++;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have at least the metrics and score checks
    EXPECT_GE(success_count.load(), 4 * 50 * 2);

    // Cleanup
    for (int i = 0; i < HEALTH_AGENT_MAX_PERCEPTION_BRIDGES; i++) {
        nimcp_health_agent_disconnect_visual(agent, mock_visual(i));
        nimcp_health_agent_disconnect_audio(agent, mock_audio(i));
    }
}

TEST_F(PerceptionCorticalHealthE2ETest, CorticalStressTest) {
    // Fill up cortical components
    for (int i = 0; i < HEALTH_AGENT_MAX_PERCEPTION_BRIDGES; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_cortical_immune(agent, mock_immune(i), nullptr), 0);
    }
    for (int i = 0; i < HEALTH_AGENT_MAX_CORTICAL_COLUMNS; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, mock_column(i), nullptr), 0);
    }

    // Stress test with concurrent operations
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count]() {
            for (int i = 0; i < 50; i++) {
                cortical_health_metrics_t metrics;
                if (nimcp_health_agent_get_cortical_metrics(agent, &metrics) == 0) {
                    success_count++;
                }

                float score = nimcp_health_agent_get_cortical_health_score(agent);
                if (score >= 0.0f && score <= 100.0f) {
                    success_count++;
                }

                // Mix in some recovery actions
                if (i % 10 == 0) {
                    cortical_recovery_action_t action = static_cast<cortical_recovery_action_t>(
                        i % (CORTICAL_RECOVERY_FULL_RESET + 1));
                    if (nimcp_health_agent_cortical_recovery(agent, action, -1) == 0) {
                        success_count++;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have at least the metrics and score checks
    EXPECT_GE(success_count.load(), 4 * 50 * 2);

    // Cleanup
    for (int i = 0; i < HEALTH_AGENT_MAX_PERCEPTION_BRIDGES; i++) {
        nimcp_health_agent_disconnect_cortical_immune(agent, mock_immune(i));
    }
    for (int i = 0; i < HEALTH_AGENT_MAX_CORTICAL_COLUMNS; i++) {
        nimcp_health_agent_disconnect_cortical_column(agent, mock_column(i));
    }
}

/* ============================================================================
 * Metrics Validation Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthE2ETest, PerceptionMetricsValidation) {
    visual_cortical_bridge_t* visual = mock_visual(1);
    audio_cortical_bridge_t* audio = mock_audio(1);
    nimcp_health_agent_connect_visual(agent, visual, nullptr);
    nimcp_health_agent_connect_audio(agent, audio, nullptr);

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);

    // Validate connection status
    EXPECT_EQ(metrics.num_visual_bridges, 1u);
    EXPECT_EQ(metrics.num_audio_bridges, 1u);
    EXPECT_FALSE(metrics.any_bridge_unhealthy);
    EXPECT_FALSE(metrics.any_connection_lost);

    // Validate latency metrics
    EXPECT_GE(metrics.avg_visual_latency_ms, 0.0);
    EXPECT_GE(metrics.max_visual_latency_ms, 0.0);
    EXPECT_GE(metrics.avg_audio_latency_ms, 0.0);
    EXPECT_GE(metrics.max_audio_latency_ms, 0.0);

    // Validate selectivity metrics
    EXPECT_GE(metrics.avg_orientation_selectivity, 0.0f);
    EXPECT_GE(metrics.avg_frequency_selectivity, 0.0f);
    EXPECT_GE(metrics.feature_selectivity_score, 0.0f);
    EXPECT_LE(metrics.feature_selectivity_score, 100.0f);

    // Validate mapping metrics
    EXPECT_GE(metrics.retinotopic_mapping_quality, 0.0f);
    EXPECT_LE(metrics.retinotopic_mapping_quality, 1.0f);
    EXPECT_GE(metrics.tonotopic_mapping_quality, 0.0f);
    EXPECT_LE(metrics.tonotopic_mapping_quality, 1.0f);

    // Validate overall health
    EXPECT_GE(metrics.overall_perception_health, 0.0f);
    EXPECT_LE(metrics.overall_perception_health, 100.0f);

    nimcp_health_agent_disconnect_visual(agent, visual);
    nimcp_health_agent_disconnect_audio(agent, audio);
}

TEST_F(PerceptionCorticalHealthE2ETest, CorticalMetricsValidation) {
    cortical_immune_system_t* immune = mock_immune(1);
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);

    // Validate connection status
    EXPECT_EQ(metrics.num_immune_systems, 1u);
    EXPECT_EQ(metrics.num_hypercolumns, 1u);
    EXPECT_FALSE(metrics.any_column_unhealthy);
    EXPECT_TRUE(metrics.immune_system_active);

    // Validate layer health
    EXPECT_GE(metrics.layer_2_3_health, 0.0f);
    EXPECT_LE(metrics.layer_2_3_health, 1.0f);
    EXPECT_GE(metrics.layer_4_health, 0.0f);
    EXPECT_LE(metrics.layer_4_health, 1.0f);
    EXPECT_GE(metrics.layer_5_6_health, 0.0f);
    EXPECT_LE(metrics.layer_5_6_health, 1.0f);
    EXPECT_FALSE(metrics.layer_communication_failure);

    // Validate activity metrics
    EXPECT_GE(metrics.avg_column_activity, 0.0f);
    EXPECT_LE(metrics.avg_column_activity, 1.0f);
    EXPECT_FALSE(metrics.hyperexcitability_detected);
    EXPECT_FALSE(metrics.hypoactivity_detected);

    // Validate immune metrics
    EXPECT_GE(metrics.microglial_activation_level, 0.0f);
    EXPECT_LE(metrics.microglial_activation_level, 1.0f);
    EXPECT_GE(metrics.inflammation_index, 0.0f);
    EXPECT_LE(metrics.inflammation_index, 1.0f);
    EXPECT_FALSE(metrics.inflammation_critical);

    // Validate overall health
    EXPECT_GE(metrics.overall_cortical_health, 0.0f);
    EXPECT_LE(metrics.overall_cortical_health, 100.0f);

    nimcp_health_agent_disconnect_cortical_immune(agent, immune);
    nimcp_health_agent_disconnect_cortical_column(agent, column);
}

/* ============================================================================
 * Recovery Scenario Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthE2ETest, PerceptionRecoveryScenario) {
    visual_cortical_bridge_t* visual = mock_visual(1);
    audio_cortical_bridge_t* audio = mock_audio(1);
    nimcp_health_agent_connect_visual(agent, visual, nullptr);
    nimcp_health_agent_connect_audio(agent, audio, nullptr);

    // Simulate recovery sequence
    // Step 1: Flush buffers
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_FLUSH_BUFFERS, -1), 0);

    // Step 2: Reset gain
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_RESET_GAIN, 0), 0);

    // Step 3: Reset filters
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_RESET_FILTERS, 0), 0);

    // Step 4: Rebuild maps
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_REBUILD_MAPS, -1), 0);

    // Step 5: Reset selectivity
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_RESET_SELECTIVITY, -1), 0);

    // Verify system still healthy
    perception_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_perception_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_visual_bridges, 1u);
    EXPECT_EQ(metrics.num_audio_bridges, 1u);

    nimcp_health_agent_disconnect_visual(agent, visual);
    nimcp_health_agent_disconnect_audio(agent, audio);
}

TEST_F(PerceptionCorticalHealthE2ETest, CorticalRecoveryScenario) {
    cortical_immune_system_t* immune = mock_immune(1);
    hypercolumn_t* column = mock_column(1);
    nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    // Simulate recovery sequence
    // Step 1: Normalize activity
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_NORMALIZE_ACTIVITY, -1), 0);

    // Step 2: Rebalance inhibition
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_REBALANCE_INHIBITION, -1), 0);

    // Step 3: Reduce inflammation
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_REDUCE_INFLAMMATION, -1), 0);

    // Step 4: Suppress microglia
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_SUPPRESS_MICROGLIA, -1), 0);

    // Step 5: Sharpen tuning
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_SHARPEN_TUNING, 0), 0);

    // Step 6: Reset plasticity
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_RESET_PLASTICITY, -1), 0);

    // Verify system still healthy
    cortical_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_cortical_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_immune_systems, 1u);
    EXPECT_EQ(metrics.num_hypercolumns, 1u);

    nimcp_health_agent_disconnect_cortical_immune(agent, immune);
    nimcp_health_agent_disconnect_cortical_column(agent, column);
}
