/**
 * @file test_perception_cortical_health_functions.cpp
 * @brief Unit tests for Phase 5.12: Perception/Cortical Health Integration
 *
 * Tests the health agent's perception and cortical health monitoring functions.
 */

#include <gtest/gtest.h>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

/**
 * @brief Unit test fixture for Perception/Cortical Health tests
 */
class PerceptionCorticalHealthTest : public ::testing::Test {
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

    // Helper to create mock visual bridge pointers
    visual_cortical_bridge_t* mock_visual_bridge(uintptr_t id) {
        return reinterpret_cast<visual_cortical_bridge_t*>(0x20000 + id);
    }

    // Helper to create mock audio bridge pointers
    audio_cortical_bridge_t* mock_audio_bridge(uintptr_t id) {
        return reinterpret_cast<audio_cortical_bridge_t*>(0x30000 + id);
    }

    // Helper to create mock cortical immune system pointers
    cortical_immune_system_t* mock_cortical_immune(uintptr_t id) {
        return reinterpret_cast<cortical_immune_system_t*>(0x40000 + id);
    }

    // Helper to create mock hypercolumn pointers
    hypercolumn_t* mock_hypercolumn(uintptr_t id) {
        return reinterpret_cast<hypercolumn_t*>(0x50000 + id);
    }
};

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, PerceptionConfigDefault) {
    health_agent_perception_config_t config;
    nimcp_health_agent_perception_config_default(&config);

    // Verify latency monitoring
    EXPECT_TRUE(config.enable_latency_monitoring);
    EXPECT_GT(config.max_visual_latency_ms, 0.0);
    EXPECT_GT(config.max_audio_latency_ms, 0.0);

    // Verify selectivity monitoring
    EXPECT_TRUE(config.enable_selectivity_monitoring);
    EXPECT_GT(config.min_orientation_selectivity, 0.0f);
    EXPECT_GT(config.min_frequency_selectivity, 0.0f);

    // Verify buffer monitoring
    EXPECT_TRUE(config.enable_buffer_monitoring);

    // Verify mapping monitoring
    EXPECT_TRUE(config.enable_mapping_monitoring);
    EXPECT_GT(config.min_mapping_quality, 0.0f);

    // Verify auto-recovery
    EXPECT_TRUE(config.enable_auto_recovery);
}

TEST_F(PerceptionCorticalHealthTest, CorticalConfigDefault) {
    health_agent_cortical_config_t config;
    nimcp_health_agent_cortical_config_default(&config);

    // Verify layer monitoring
    EXPECT_TRUE(config.enable_layer_monitoring);
    EXPECT_GT(config.min_layer_health, 0.0f);

    // Verify activity monitoring
    EXPECT_TRUE(config.enable_activity_monitoring);
    EXPECT_GT(config.hyperexcitability_threshold, 0.0f);
    EXPECT_GT(config.hypoactivity_threshold, 0.0f);

    // Verify competition monitoring
    EXPECT_TRUE(config.enable_competition_monitoring);
    EXPECT_GT(config.min_wta_effectiveness, 0.0f);

    // Verify immune monitoring
    EXPECT_TRUE(config.enable_immune_monitoring);
    EXPECT_GT(config.inflammation_threshold, 0.0f);

    // Verify tuning monitoring
    EXPECT_TRUE(config.enable_tuning_monitoring);

    // Verify auto-recovery
    EXPECT_TRUE(config.enable_auto_recovery);
}

TEST_F(PerceptionCorticalHealthTest, PerceptionConfigDefaultNullSafe) {
    // Should not crash with null pointer
    nimcp_health_agent_perception_config_default(nullptr);
}

TEST_F(PerceptionCorticalHealthTest, CorticalConfigDefaultNullSafe) {
    // Should not crash with null pointer
    nimcp_health_agent_cortical_config_default(nullptr);
}

/* ============================================================================
 * Visual Bridge Connection Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, ConnectVisualBridge) {
    visual_cortical_bridge_t* bridge = mock_visual_bridge(1);

    int result = nimcp_health_agent_connect_visual(agent, bridge, nullptr);
    EXPECT_EQ(result, 0);

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 1u);
}

TEST_F(PerceptionCorticalHealthTest, ConnectMultipleVisualBridges) {
    for (int i = 0; i < 4; i++) {
        visual_cortical_bridge_t* bridge = mock_visual_bridge(i);
        int result = nimcp_health_agent_connect_visual(agent, bridge, nullptr);
        EXPECT_EQ(result, 0);
    }

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 4u);
}

TEST_F(PerceptionCorticalHealthTest, ConnectVisualBridgeDuplicatePrevention) {
    visual_cortical_bridge_t* bridge = mock_visual_bridge(1);

    EXPECT_EQ(nimcp_health_agent_connect_visual(agent, bridge, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_visual(agent, bridge, nullptr), 0);

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 1u);
}

TEST_F(PerceptionCorticalHealthTest, DisconnectVisualBridge) {
    visual_cortical_bridge_t* bridge = mock_visual_bridge(1);

    nimcp_health_agent_connect_visual(agent, bridge, nullptr);
    EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, bridge), 0);

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_visual_bridges, 0u);
}

TEST_F(PerceptionCorticalHealthTest, DisconnectVisualBridgeNotFound) {
    visual_cortical_bridge_t* bridge = mock_visual_bridge(1);
    EXPECT_EQ(nimcp_health_agent_disconnect_visual(agent, bridge), -1);
}

TEST_F(PerceptionCorticalHealthTest, ConnectVisualBridgeNullAgentSafe) {
    visual_cortical_bridge_t* bridge = mock_visual_bridge(1);
    EXPECT_EQ(nimcp_health_agent_connect_visual(nullptr, bridge, nullptr), -1);
}

TEST_F(PerceptionCorticalHealthTest, ConnectVisualBridgeNullBridgeSafe) {
    EXPECT_EQ(nimcp_health_agent_connect_visual(agent, nullptr, nullptr), -1);
}

/* ============================================================================
 * Audio Bridge Connection Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, ConnectAudioBridge) {
    audio_cortical_bridge_t* bridge = mock_audio_bridge(1);

    int result = nimcp_health_agent_connect_audio(agent, bridge, nullptr);
    EXPECT_EQ(result, 0);

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_audio_bridges, 1u);
}

TEST_F(PerceptionCorticalHealthTest, ConnectMultipleAudioBridges) {
    for (int i = 0; i < 4; i++) {
        audio_cortical_bridge_t* bridge = mock_audio_bridge(i);
        int result = nimcp_health_agent_connect_audio(agent, bridge, nullptr);
        EXPECT_EQ(result, 0);
    }

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_audio_bridges, 4u);
}

TEST_F(PerceptionCorticalHealthTest, ConnectAudioBridgeDuplicatePrevention) {
    audio_cortical_bridge_t* bridge = mock_audio_bridge(1);

    EXPECT_EQ(nimcp_health_agent_connect_audio(agent, bridge, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_audio(agent, bridge, nullptr), 0);

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_audio_bridges, 1u);
}

TEST_F(PerceptionCorticalHealthTest, DisconnectAudioBridge) {
    audio_cortical_bridge_t* bridge = mock_audio_bridge(1);

    nimcp_health_agent_connect_audio(agent, bridge, nullptr);
    EXPECT_EQ(nimcp_health_agent_disconnect_audio(agent, bridge), 0);

    perception_health_metrics_t metrics;
    nimcp_health_agent_get_perception_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_audio_bridges, 0u);
}

TEST_F(PerceptionCorticalHealthTest, DisconnectAudioBridgeNotFound) {
    audio_cortical_bridge_t* bridge = mock_audio_bridge(1);
    EXPECT_EQ(nimcp_health_agent_disconnect_audio(agent, bridge), -1);
}

/* ============================================================================
 * Cortical Immune System Connection Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, ConnectCorticalImmune) {
    cortical_immune_system_t* immune = mock_cortical_immune(1);

    int result = nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr);
    EXPECT_EQ(result, 0);

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_immune_systems, 1u);
}

TEST_F(PerceptionCorticalHealthTest, ConnectMultipleCorticalImmune) {
    for (int i = 0; i < 4; i++) {
        cortical_immune_system_t* immune = mock_cortical_immune(i);
        int result = nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr);
        EXPECT_EQ(result, 0);
    }

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_immune_systems, 4u);
}

TEST_F(PerceptionCorticalHealthTest, ConnectCorticalImmuneDuplicatePrevention) {
    cortical_immune_system_t* immune = mock_cortical_immune(1);

    EXPECT_EQ(nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr), 0);

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_immune_systems, 1u);
}

TEST_F(PerceptionCorticalHealthTest, DisconnectCorticalImmune) {
    cortical_immune_system_t* immune = mock_cortical_immune(1);

    nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_immune(agent, immune), 0);

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_immune_systems, 0u);
}

TEST_F(PerceptionCorticalHealthTest, DisconnectCorticalImmuneNotFound) {
    cortical_immune_system_t* immune = mock_cortical_immune(1);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_immune(agent, immune), -1);
}

/* ============================================================================
 * Cortical Column Connection Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, ConnectCorticalColumn) {
    hypercolumn_t* column = mock_hypercolumn(1);

    int result = nimcp_health_agent_connect_cortical_column(agent, column, nullptr);
    EXPECT_EQ(result, 0);

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_hypercolumns, 1u);
}

TEST_F(PerceptionCorticalHealthTest, ConnectMultipleCorticalColumns) {
    for (int i = 0; i < 8; i++) {
        hypercolumn_t* column = mock_hypercolumn(i);
        int result = nimcp_health_agent_connect_cortical_column(agent, column, nullptr);
        EXPECT_EQ(result, 0);
    }

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_hypercolumns, 8u);
}

TEST_F(PerceptionCorticalHealthTest, ConnectCorticalColumnDuplicatePrevention) {
    hypercolumn_t* column = mock_hypercolumn(1);

    EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, column, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_cortical_column(agent, column, nullptr), 0);

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_hypercolumns, 1u);
}

TEST_F(PerceptionCorticalHealthTest, DisconnectCorticalColumn) {
    hypercolumn_t* column = mock_hypercolumn(1);

    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, column), 0);

    cortical_health_metrics_t metrics;
    nimcp_health_agent_get_cortical_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_hypercolumns, 0u);
}

TEST_F(PerceptionCorticalHealthTest, DisconnectCorticalColumnNotFound) {
    hypercolumn_t* column = mock_hypercolumn(1);
    EXPECT_EQ(nimcp_health_agent_disconnect_cortical_column(agent, column), -1);
}

/* ============================================================================
 * Perception Metrics Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, GetPerceptionMetricsEmpty) {
    perception_health_metrics_t metrics;
    int result = nimcp_health_agent_get_perception_metrics(agent, &metrics);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(metrics.num_visual_bridges, 0u);
    EXPECT_EQ(metrics.num_audio_bridges, 0u);
    EXPECT_GT(metrics.last_check_timestamp_us, 0u);
}

TEST_F(PerceptionCorticalHealthTest, GetPerceptionMetricsWithBridges) {
    visual_cortical_bridge_t* visual = mock_visual_bridge(1);
    audio_cortical_bridge_t* audio = mock_audio_bridge(1);

    nimcp_health_agent_connect_visual(agent, visual, nullptr);
    nimcp_health_agent_connect_audio(agent, audio, nullptr);

    perception_health_metrics_t metrics;
    int result = nimcp_health_agent_get_perception_metrics(agent, &metrics);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(metrics.num_visual_bridges, 1u);
    EXPECT_EQ(metrics.num_audio_bridges, 1u);
    EXPECT_GE(metrics.avg_visual_latency_ms, 0.0);
    EXPECT_GE(metrics.avg_audio_latency_ms, 0.0);
    EXPECT_GE(metrics.avg_orientation_selectivity, 0.0f);
    EXPECT_GE(metrics.avg_frequency_selectivity, 0.0f);
    EXPECT_GE(metrics.overall_perception_health, 0.0f);
    EXPECT_LE(metrics.overall_perception_health, 100.0f);
}

TEST_F(PerceptionCorticalHealthTest, GetPerceptionMetricsNullSafe) {
    EXPECT_EQ(nimcp_health_agent_get_perception_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_perception_metrics(agent, nullptr), -1);
}

/* ============================================================================
 * Cortical Metrics Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, GetCorticalMetricsEmpty) {
    cortical_health_metrics_t metrics;
    int result = nimcp_health_agent_get_cortical_metrics(agent, &metrics);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(metrics.num_immune_systems, 0u);
    EXPECT_EQ(metrics.num_hypercolumns, 0u);
    EXPECT_GT(metrics.last_check_timestamp_us, 0u);
}

TEST_F(PerceptionCorticalHealthTest, GetCorticalMetricsWithComponents) {
    cortical_immune_system_t* immune = mock_cortical_immune(1);
    hypercolumn_t* column = mock_hypercolumn(1);

    nimcp_health_agent_connect_cortical_immune(agent, immune, nullptr);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    cortical_health_metrics_t metrics;
    int result = nimcp_health_agent_get_cortical_metrics(agent, &metrics);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(metrics.num_immune_systems, 1u);
    EXPECT_EQ(metrics.num_hypercolumns, 1u);
    EXPECT_GE(metrics.layer_2_3_health, 0.0f);
    EXPECT_GE(metrics.layer_4_health, 0.0f);
    EXPECT_GE(metrics.layer_5_6_health, 0.0f);
    EXPECT_GE(metrics.wta_effectiveness, 0.0f);
    EXPECT_GE(metrics.overall_cortical_health, 0.0f);
    EXPECT_LE(metrics.overall_cortical_health, 100.0f);
}

TEST_F(PerceptionCorticalHealthTest, GetCorticalMetricsNullSafe) {
    EXPECT_EQ(nimcp_health_agent_get_cortical_metrics(nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_get_cortical_metrics(agent, nullptr), -1);
}

/* ============================================================================
 * Health Score Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, GetPerceptionHealthScoreNoBridges) {
    float score = nimcp_health_agent_get_perception_health_score(agent);
    EXPECT_EQ(score, 100.0f);  // No bridges = assume healthy
}

TEST_F(PerceptionCorticalHealthTest, GetPerceptionHealthScoreWithBridges) {
    visual_cortical_bridge_t* bridge = mock_visual_bridge(1);
    nimcp_health_agent_connect_visual(agent, bridge, nullptr);

    float score = nimcp_health_agent_get_perception_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(PerceptionCorticalHealthTest, GetPerceptionHealthScoreNullSafe) {
    float score = nimcp_health_agent_get_perception_health_score(nullptr);
    EXPECT_EQ(score, 100.0f);
}

TEST_F(PerceptionCorticalHealthTest, GetCorticalHealthScoreNoComponents) {
    float score = nimcp_health_agent_get_cortical_health_score(agent);
    EXPECT_EQ(score, 100.0f);  // No components = assume healthy
}

TEST_F(PerceptionCorticalHealthTest, GetCorticalHealthScoreWithComponents) {
    hypercolumn_t* column = mock_hypercolumn(1);
    nimcp_health_agent_connect_cortical_column(agent, column, nullptr);

    float score = nimcp_health_agent_get_cortical_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(PerceptionCorticalHealthTest, GetCorticalHealthScoreNullSafe) {
    float score = nimcp_health_agent_get_cortical_health_score(nullptr);
    EXPECT_EQ(score, 100.0f);
}

/* ============================================================================
 * Needs Attention Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, PerceptionNeedsAttentionNoBridges) {
    bool needs = nimcp_health_agent_perception_needs_attention(agent);
    EXPECT_FALSE(needs);  // No bridges = no attention needed
}

TEST_F(PerceptionCorticalHealthTest, PerceptionNeedsAttentionNullSafe) {
    bool needs = nimcp_health_agent_perception_needs_attention(nullptr);
    EXPECT_FALSE(needs);
}

TEST_F(PerceptionCorticalHealthTest, CorticalNeedsAttentionNoComponents) {
    bool needs = nimcp_health_agent_cortical_needs_attention(agent);
    EXPECT_FALSE(needs);  // No components = no attention needed
}

TEST_F(PerceptionCorticalHealthTest, CorticalNeedsAttentionNullSafe) {
    bool needs = nimcp_health_agent_cortical_needs_attention(nullptr);
    EXPECT_FALSE(needs);
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, PerceptionRecoveryNone) {
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_NONE, -1), 0);
}

TEST_F(PerceptionCorticalHealthTest, PerceptionRecoveryAllActions) {
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_FLUSH_BUFFERS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_RESET_GAIN, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_ADJUST_GAIN, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_RESET_FILTERS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_CLEAR_MAPS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_REBUILD_MAPS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_RESET_SELECTIVITY, 0), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_SOFT_RESET, -1), 0);
    EXPECT_EQ(nimcp_health_agent_perception_recovery(agent, PERCEPTION_RECOVERY_FULL_RESET, -1), 0);
}

TEST_F(PerceptionCorticalHealthTest, PerceptionRecoveryNullSafe) {
    EXPECT_EQ(nimcp_health_agent_perception_recovery(nullptr, PERCEPTION_RECOVERY_NONE, -1), -1);
}

TEST_F(PerceptionCorticalHealthTest, CorticalRecoveryNone) {
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_NONE, -1), 0);
}

TEST_F(PerceptionCorticalHealthTest, CorticalRecoveryAllActions) {
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_NORMALIZE_ACTIVITY, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_RESET_COMPETITION, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_REBALANCE_INHIBITION, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_REDUCE_INFLAMMATION, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_SUPPRESS_MICROGLIA, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_RESET_LAYERS, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_SHARPEN_TUNING, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_RESET_PLASTICITY, 0), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_SOFT_RESET, -1), 0);
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(agent, CORTICAL_RECOVERY_FULL_RESET, -1), 0);
}

TEST_F(PerceptionCorticalHealthTest, CorticalRecoveryNullSafe) {
    EXPECT_EQ(nimcp_health_agent_cortical_recovery(nullptr, CORTICAL_RECOVERY_NONE, -1), -1);
}

/* ============================================================================
 * Configuration Update Tests
 * ============================================================================ */

TEST_F(PerceptionCorticalHealthTest, UpdatePerceptionConfig) {
    health_agent_perception_config_t config;
    nimcp_health_agent_perception_config_default(&config);
    config.max_visual_latency_ms = 200.0;
    config.max_audio_latency_ms = 100.0;

    EXPECT_EQ(nimcp_health_agent_update_perception_config(agent, &config), 0);
}

TEST_F(PerceptionCorticalHealthTest, UpdatePerceptionConfigNullSafe) {
    health_agent_perception_config_t config;
    EXPECT_EQ(nimcp_health_agent_update_perception_config(nullptr, &config), -1);
    EXPECT_EQ(nimcp_health_agent_update_perception_config(agent, nullptr), -1);
}

TEST_F(PerceptionCorticalHealthTest, UpdateCorticalConfig) {
    health_agent_cortical_config_t config;
    nimcp_health_agent_cortical_config_default(&config);
    config.min_layer_health = 0.7f;
    config.inflammation_threshold = 0.4f;

    EXPECT_EQ(nimcp_health_agent_update_cortical_config(agent, &config), 0);
}

TEST_F(PerceptionCorticalHealthTest, UpdateCorticalConfigNullSafe) {
    health_agent_cortical_config_t config;
    EXPECT_EQ(nimcp_health_agent_update_cortical_config(nullptr, &config), -1);
    EXPECT_EQ(nimcp_health_agent_update_cortical_config(agent, nullptr), -1);
}
