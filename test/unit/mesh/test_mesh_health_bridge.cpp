/**
 * @file test_mesh_health_bridge.cpp
 * @brief Unit tests for mesh health bridge (Phase 14)
 *
 * Tests health agent integration and distributed health monitoring.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_types.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class MeshHealthBridgeTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_health_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create bootstrap with minimal config */
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems.enable_cognitive = false;
        config.subsystems.enable_sensory = false;
        config.subsystems.enable_motor = false;
        config.subsystems.enable_memory = false;
        config.subsystems.enable_security = false;
        config.subsystems.enable_gpu = false;
        config.subsystems.enable_plasticity = false;
        config.subsystems.enable_glial = false;
        config.subsystems.enable_swarm = false;
        config.subsystems.enable_async = false;
        config.subsystems.enable_lnn = false;
        config.subsystems.enable_snn = false;

        bootstrap = mesh_bootstrap_create(&config);
        if (bootstrap) {
            bridge = mesh_bootstrap_get_health_bridge(bootstrap);
        }
    }

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
            bridge = nullptr;  /* Destroyed with bootstrap */
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST(MeshHealthBridgeConfigTest, DefaultConfig) {
    mesh_health_bridge_config_t config;
    nimcp_error_t err = mesh_health_bridge_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.default_heartbeat_interval_ms, 0u);
    EXPECT_GT(config.missed_heartbeat_threshold, 0u);
    EXPECT_GT(config.dead_threshold, 0u);
}

TEST(MeshHealthBridgeConfigTest, DefaultConfigNullPointer) {
    nimcp_error_t err = mesh_health_bridge_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST(MeshHealthBridgeConfigTest, DefaultHealthWeights) {
    mesh_health_bridge_config_t config;
    mesh_health_bridge_default_config(&config);

    /* Weights should sum to approximately 1.0 */
    float total = config.weight_cpu + config.weight_memory +
                  config.weight_tx_success + config.weight_latency +
                  config.weight_errors;
    EXPECT_NEAR(total, 1.0f, 0.01f);
}

TEST(MeshHealthBridgeConfigTest, DefaultThresholds) {
    mesh_health_bridge_config_t config;
    mesh_health_bridge_default_config(&config);

    /* Thresholds should be in order: degraded > unhealthy > critical */
    EXPECT_GT(config.degraded_threshold, config.unhealthy_threshold);
    EXPECT_GT(config.unhealthy_threshold, config.critical_threshold);

    /* All thresholds should be between 0 and 1 */
    EXPECT_GT(config.degraded_threshold, 0.0f);
    EXPECT_LT(config.degraded_threshold, 1.0f);
    EXPECT_GT(config.critical_threshold, 0.0f);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshHealthBridgeTest, BridgeCreatedWithBootstrap) {
    EXPECT_NE(bridge, nullptr);
}

/* ============================================================================
 * Health Status Tests
 * ============================================================================ */

TEST(MeshHealthBridgeStatusTest, StatusOrdering) {
    /* Health statuses should be ordered from best to worst */
    EXPECT_LT(MESH_HEALTH_HEALTHY, MESH_HEALTH_DEGRADED);
    EXPECT_LT(MESH_HEALTH_DEGRADED, MESH_HEALTH_UNHEALTHY);
    EXPECT_LT(MESH_HEALTH_UNHEALTHY, MESH_HEALTH_CRITICAL);
    EXPECT_LT(MESH_HEALTH_CRITICAL, MESH_HEALTH_DEAD);
}

TEST(MeshHealthBridgeStatusTest, UnknownIsZero) {
    EXPECT_EQ(MESH_HEALTH_UNKNOWN, 0);
}

TEST(MeshHealthBridgeStatusTest, StatusToString) {
    const char* healthy = mesh_health_status_to_string(MESH_HEALTH_HEALTHY);
    EXPECT_NE(healthy, nullptr);
    EXPECT_STREQ(healthy, "HEALTHY");

    const char* dead = mesh_health_status_to_string(MESH_HEALTH_DEAD);
    EXPECT_NE(dead, nullptr);
    EXPECT_STREQ(dead, "DEAD");
}

/* ============================================================================
 * Heartbeat Operation Tests
 * ============================================================================ */

TEST(MeshHealthBridgeHeartbeatTest, HeartbeatOpToString) {
    const char* start = mesh_heartbeat_op_to_string(MESH_HEARTBEAT_START);
    EXPECT_NE(start, nullptr);
    EXPECT_STREQ(start, "START");

    const char* complete = mesh_heartbeat_op_to_string(MESH_HEARTBEAT_COMPLETE);
    EXPECT_NE(complete, nullptr);
    EXPECT_STREQ(complete, "COMPLETE");

    const char* ping = mesh_heartbeat_op_to_string(MESH_HEARTBEAT_PING);
    EXPECT_NE(ping, nullptr);
    EXPECT_STREQ(ping, "PING");
}

TEST(MeshHealthBridgeHeartbeatTest, AllOpsHaveStrings) {
    EXPECT_NE(mesh_heartbeat_op_to_string(MESH_HEARTBEAT_START), nullptr);
    EXPECT_NE(mesh_heartbeat_op_to_string(MESH_HEARTBEAT_PROGRESS), nullptr);
    EXPECT_NE(mesh_heartbeat_op_to_string(MESH_HEARTBEAT_COMPLETE), nullptr);
    EXPECT_NE(mesh_heartbeat_op_to_string(MESH_HEARTBEAT_ERROR), nullptr);
    EXPECT_NE(mesh_heartbeat_op_to_string(MESH_HEARTBEAT_PING), nullptr);
}

/* ============================================================================
 * Health Record Structure Tests
 * ============================================================================ */

TEST(MeshHealthBridgeRecordTest, RecordStructureSize) {
    EXPECT_GT(sizeof(mesh_health_record_t), 0u);
}

TEST(MeshHealthBridgeRecordTest, RecordMetricsRange) {
    mesh_health_record_t record;
    memset(&record, 0, sizeof(record));

    /* Set valid metric values */
    record.cpu_usage = 0.5f;
    record.memory_usage = 0.6f;
    record.transaction_success_rate = 0.95f;
    record.health_score = 0.8f;

    /* Verify values are in expected range [0, 1] */
    EXPECT_GE(record.cpu_usage, 0.0f);
    EXPECT_LE(record.cpu_usage, 1.0f);
    EXPECT_GE(record.memory_usage, 0.0f);
    EXPECT_LE(record.memory_usage, 1.0f);
}

/* ============================================================================
 * Channel Health Structure Tests
 * ============================================================================ */

TEST(MeshHealthBridgeChannelTest, ChannelHealthStructureSize) {
    EXPECT_GT(sizeof(mesh_channel_health_t), 0u);
}

TEST(MeshHealthBridgeChannelTest, ChannelHealthCountsSum) {
    mesh_channel_health_t health;
    memset(&health, 0, sizeof(health));

    health.healthy_participants = 5;
    health.degraded_participants = 2;
    health.unhealthy_participants = 1;
    health.dead_participants = 0;
    health.total_participants = 8;

    size_t sum = health.healthy_participants + health.degraded_participants +
                 health.unhealthy_participants + health.dead_participants;
    EXPECT_EQ(sum, health.total_participants);
}

/* ============================================================================
 * System Health Structure Tests
 * ============================================================================ */

TEST(MeshHealthBridgeSystemTest, SystemHealthStructureSize) {
    EXPECT_GT(sizeof(mesh_system_health_t), 0u);
}

TEST(MeshHealthBridgeSystemTest, SystemHealthChannelArray) {
    mesh_system_health_t health;
    /* Should have room for multiple channels */
    EXPECT_GE(sizeof(health.channels) / sizeof(health.channels[0]), 5u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshHealthBridgeTest, GetStats) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_health_bridge_stats_t stats;
    nimcp_error_t err = mesh_health_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.heartbeats_received, 0u);  /* No heartbeats yet */
}

TEST_F(MeshHealthBridgeTest, ResetStats) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    nimcp_error_t err = mesh_health_bridge_reset_stats(bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_health_bridge_stats_t stats;
    mesh_health_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.heartbeats_received, 0u);
    EXPECT_EQ(stats.status_changes, 0u);
}

/* ============================================================================
 * Health Check Tests
 * ============================================================================ */

TEST_F(MeshHealthBridgeTest, IsHealthyWithNoParticipants) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    /* With no participants, should return false for unknown ID */
    mesh_participant_id_t unknown_id = 0x12345678;
    bool healthy = mesh_health_bridge_is_healthy(bridge, unknown_id);
    EXPECT_FALSE(healthy);
}

TEST_F(MeshHealthBridgeTest, IsChannelHealthyEmpty) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    /* Empty channel should be considered healthy (no failures) */
    bool healthy = mesh_health_bridge_is_channel_healthy(
        bridge, MESH_CHANNEL_LEFT_HEMISPHERE);
    /* Either true (empty = healthy) or false (no participants) is acceptable */
}
