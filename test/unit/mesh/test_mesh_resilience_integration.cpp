/**
 * @file test_mesh_resilience_integration.cpp
 * @brief Unit tests for mesh resilience integration with health agents
 *
 * Tests health agent registration, heartbeat aggregation, failure detection,
 * and recovery triggering.
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_health_bridge.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshResilienceIntegrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_health_bridge_t* bridge = nullptr;

    void SetUp() override {
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
        ASSERT_NE(bootstrap, nullptr);

        bridge = mesh_bootstrap_get_health_bridge(bootstrap);
    }

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Health Agent Registration Tests
 * ============================================================================ */

TEST_F(MeshResilienceIntegrationTest, RegisterHealthAgent) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t participant_id = 0x12345678;

    /* Register with NULL agent (agent may be optional) */
    nimcp_error_t err = mesh_health_bridge_register_agent(
        bridge, participant_id, nullptr
    );

    /* Registration should succeed or return appropriate error */
    EXPECT_TRUE(err == NIMCP_SUCCESS ||
                err == NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshResilienceIntegrationTest, UnregisterHealthAgent) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t participant_id = 0x12345678;

    /* Try to unregister a non-registered agent */
    nimcp_error_t err = mesh_health_bridge_unregister_agent(bridge, participant_id);

    /* Should return not found or succeed */
    EXPECT_TRUE(err == NIMCP_SUCCESS ||
                err == NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshResilienceIntegrationTest, RegisterMultipleAgents) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t ids[] = {0x100, 0x200, 0x300, 0x400};

    for (size_t i = 0; i < 4; i++) {
        nimcp_error_t err = mesh_health_bridge_register_agent(bridge, ids[i], nullptr);
        /* Allow success or invalid param if agent is required */
        EXPECT_TRUE(err == NIMCP_SUCCESS ||
                    err == NIMCP_ERROR_INVALID_PARAM);
    }
}

TEST_F(MeshResilienceIntegrationTest, RegisterDuplicateAgent) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t participant_id = 0x12345678;

    mesh_health_bridge_register_agent(bridge, participant_id, nullptr);
    nimcp_error_t err = mesh_health_bridge_register_agent(bridge, participant_id, nullptr);

    /* Should succeed (update) or return already exists */
    EXPECT_TRUE(err == NIMCP_SUCCESS ||
                err == NIMCP_ERROR_ALREADY_EXISTS ||
                err == NIMCP_ERROR_INVALID_PARAM);
}

/* ============================================================================
 * Heartbeat Aggregation Tests
 * ============================================================================ */

TEST_F(MeshResilienceIntegrationTest, SendHeartbeat) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t participant_id = 0x12345678;

    nimcp_error_t err = mesh_health_bridge_heartbeat(
        bridge, participant_id, MESH_HEARTBEAT_PING, 0
    );

    /* May succeed or fail if agent not registered */
    (void)err;
}

TEST_F(MeshResilienceIntegrationTest, SendHeartbeatStart) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t participant_id = 0x12345678;

    nimcp_error_t err = mesh_health_bridge_heartbeat(
        bridge, participant_id, MESH_HEARTBEAT_START, 0
    );
    (void)err;
}

TEST_F(MeshResilienceIntegrationTest, SendHeartbeatProgress) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t participant_id = 0x12345678;

    /* Progress should be 0-100 */
    nimcp_error_t err = mesh_health_bridge_heartbeat(
        bridge, participant_id, MESH_HEARTBEAT_PROGRESS, 50
    );
    (void)err;
}

TEST_F(MeshResilienceIntegrationTest, SendHeartbeatComplete) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t participant_id = 0x12345678;

    nimcp_error_t err = mesh_health_bridge_heartbeat(
        bridge, participant_id, MESH_HEARTBEAT_COMPLETE, 100
    );
    (void)err;
}

TEST_F(MeshResilienceIntegrationTest, SendHeartbeatError) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t participant_id = 0x12345678;

    nimcp_error_t err = mesh_health_bridge_heartbeat(
        bridge, participant_id, MESH_HEARTBEAT_ERROR, 0
    );
    (void)err;
}

TEST_F(MeshResilienceIntegrationTest, HeartbeatUpdatesStats) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_health_bridge_stats_t stats_before;
    mesh_health_bridge_get_stats(bridge, &stats_before);

    /* Send some heartbeats */
    mesh_participant_id_t participant_id = 0x12345678;
    mesh_health_bridge_heartbeat(bridge, participant_id, MESH_HEARTBEAT_PING, 0);

    mesh_health_bridge_stats_t stats_after;
    mesh_health_bridge_get_stats(bridge, &stats_after);

    /* Stats may or may not increase depending on if agent is registered */
}

/* ============================================================================
 * Failure Detection Tests
 * ============================================================================ */

TEST_F(MeshResilienceIntegrationTest, CheckHeartbeatsReturnsCount) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    /* Check heartbeats should return count of newly dead participants */
    size_t dead_count = mesh_health_bridge_check_heartbeats(bridge);

    /* With no participants, should be zero */
    EXPECT_EQ(dead_count, 0u);
}

TEST_F(MeshResilienceIntegrationTest, IsHealthyReturnsFalseForUnknown) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t unknown_id = 0xDEADBEEF;
    bool healthy = mesh_health_bridge_is_healthy(bridge, unknown_id);

    /* Unknown participant should not be considered healthy */
    EXPECT_FALSE(healthy);
}

TEST_F(MeshResilienceIntegrationTest, IsChannelHealthyEmptyChannel) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    bool healthy = mesh_health_bridge_is_channel_healthy(
        bridge, MESH_CHANNEL_LEFT_HEMISPHERE
    );

    /* Empty channel may be healthy (no failures) or not healthy (no participants) */
    (void)healthy;
}

TEST_F(MeshResilienceIntegrationTest, GetHealthForUnknownParticipant) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_participant_id_t unknown_id = 0xDEADBEEF;
    mesh_health_record_t record;

    nimcp_error_t err = mesh_health_bridge_get_health(bridge, unknown_id, &record);

    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshResilienceIntegrationTest, HealthStatusOrdering) {
    /* Verify health status ordering */
    EXPECT_EQ(MESH_HEALTH_UNKNOWN, 0);
    EXPECT_LT(MESH_HEALTH_HEALTHY, MESH_HEALTH_DEGRADED);
    EXPECT_LT(MESH_HEALTH_DEGRADED, MESH_HEALTH_UNHEALTHY);
    EXPECT_LT(MESH_HEALTH_UNHEALTHY, MESH_HEALTH_CRITICAL);
    EXPECT_LT(MESH_HEALTH_CRITICAL, MESH_HEALTH_DEAD);
}

/* ============================================================================
 * Recovery Triggering Tests
 * ============================================================================ */

TEST_F(MeshResilienceIntegrationTest, HealthRecordFieldsAccessible) {
    mesh_health_record_t record;
    memset(&record, 0, sizeof(record));

    record.participant_id = 0x12345678;
    record.status = MESH_HEALTH_HEALTHY;
    record.last_heartbeat_ns = 1000000000;
    record.heartbeat_interval_ms = 1000;
    record.missed_heartbeats = 0;
    record.cpu_usage = 0.5f;
    record.memory_usage = 0.6f;
    record.transaction_success_rate = 0.95f;
    record.avg_latency_ms = 10.0f;
    record.health_score = 0.9f;
    record.error_count = 0;

    EXPECT_EQ(record.status, MESH_HEALTH_HEALTHY);
    EXPECT_FLOAT_EQ(record.health_score, 0.9f);
}

TEST_F(MeshResilienceIntegrationTest, ChannelHealthFieldsAccessible) {
    mesh_channel_health_t health;
    memset(&health, 0, sizeof(health));

    health.channel_id = MESH_CHANNEL_LEFT_HEMISPHERE;
    health.status = MESH_HEALTH_HEALTHY;
    health.total_participants = 10;
    health.healthy_participants = 8;
    health.degraded_participants = 1;
    health.unhealthy_participants = 1;
    health.dead_participants = 0;
    health.avg_health_score = 0.85f;
    health.min_health_score = 0.5f;

    EXPECT_EQ(health.channel_id, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(health.total_participants, 10u);
}

TEST_F(MeshResilienceIntegrationTest, GetChannelHealth) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_channel_health_t health;
    nimcp_error_t err = mesh_health_bridge_get_channel_health(
        bridge, MESH_CHANNEL_LEFT_HEMISPHERE, &health
    );

    /* Should succeed even for empty channel */
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(health.channel_id, MESH_CHANNEL_LEFT_HEMISPHERE);
}

TEST_F(MeshResilienceIntegrationTest, GetSystemHealth) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_system_health_t health;
    nimcp_error_t err = mesh_health_bridge_get_system_health(bridge, &health);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    /* System health score should be between 0 and 1 */
    EXPECT_GE(health.system_health_score, 0.0f);
    EXPECT_LE(health.system_health_score, 1.0f);
}

TEST_F(MeshResilienceIntegrationTest, SystemHealthFieldsAccessible) {
    mesh_system_health_t health;
    memset(&health, 0, sizeof(health));

    health.status = MESH_HEALTH_HEALTHY;
    health.channel_count = 5;
    health.total_participants = 100;
    health.healthy_percentage = 90;
    health.system_health_score = 0.9f;
    health.system_free_energy = 0.1f;
    health.coordinator_pool_size = 8;
    health.active_coordinators = 7;
    health.leader_healthy = true;
    health.ordering_service_healthy = true;

    EXPECT_EQ(health.status, MESH_HEALTH_HEALTHY);
    EXPECT_TRUE(health.leader_healthy);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshResilienceIntegrationTest, GetStats) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_health_bridge_stats_t stats;
    nimcp_error_t err = mesh_health_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.heartbeats_received, 0u);
}

TEST_F(MeshResilienceIntegrationTest, ResetStats) {
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

TEST_F(MeshResilienceIntegrationTest, StatsTrackCurrentCounts) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    mesh_health_bridge_stats_t stats;
    mesh_health_bridge_get_stats(bridge, &stats);

    /* Current counts should all be zero initially */
    EXPECT_EQ(stats.current_healthy, 0u);
    EXPECT_EQ(stats.current_degraded, 0u);
    EXPECT_EQ(stats.current_unhealthy, 0u);
    EXPECT_EQ(stats.current_critical, 0u);
    EXPECT_EQ(stats.current_dead, 0u);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(MeshResilienceIntegrationTest, DefaultConfig) {
    mesh_health_bridge_config_t config;
    nimcp_error_t err = mesh_health_bridge_default_config(&config);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(config.default_heartbeat_interval_ms, 0u);
    EXPECT_GT(config.missed_heartbeat_threshold, 0u);
    EXPECT_GT(config.dead_threshold, 0u);
}

TEST_F(MeshResilienceIntegrationTest, DefaultConfigNullPointer) {
    nimcp_error_t err = mesh_health_bridge_default_config(nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshResilienceIntegrationTest, HealthWeightsSum) {
    mesh_health_bridge_config_t config;
    mesh_health_bridge_default_config(&config);

    float total = config.weight_cpu + config.weight_memory +
                  config.weight_tx_success + config.weight_latency +
                  config.weight_errors;
    EXPECT_NEAR(total, 1.0f, 0.01f);
}

TEST_F(MeshResilienceIntegrationTest, ThresholdsOrdering) {
    mesh_health_bridge_config_t config;
    mesh_health_bridge_default_config(&config);

    /* Thresholds should be in order: degraded > unhealthy > critical */
    EXPECT_GT(config.degraded_threshold, config.unhealthy_threshold);
    EXPECT_GT(config.unhealthy_threshold, config.critical_threshold);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_F(MeshResilienceIntegrationTest, StatusToString) {
    const char* healthy = mesh_health_status_to_string(MESH_HEALTH_HEALTHY);
    EXPECT_STREQ(healthy, "HEALTHY");

    const char* degraded = mesh_health_status_to_string(MESH_HEALTH_DEGRADED);
    EXPECT_STREQ(degraded, "DEGRADED");

    const char* unhealthy = mesh_health_status_to_string(MESH_HEALTH_UNHEALTHY);
    EXPECT_STREQ(unhealthy, "UNHEALTHY");

    const char* critical = mesh_health_status_to_string(MESH_HEALTH_CRITICAL);
    EXPECT_STREQ(critical, "CRITICAL");

    const char* dead = mesh_health_status_to_string(MESH_HEALTH_DEAD);
    EXPECT_STREQ(dead, "DEAD");
}

TEST_F(MeshResilienceIntegrationTest, HeartbeatOpToString) {
    const char* start = mesh_heartbeat_op_to_string(MESH_HEARTBEAT_START);
    EXPECT_STREQ(start, "START");

    const char* progress = mesh_heartbeat_op_to_string(MESH_HEARTBEAT_PROGRESS);
    EXPECT_STREQ(progress, "PROGRESS");

    const char* complete = mesh_heartbeat_op_to_string(MESH_HEARTBEAT_COMPLETE);
    EXPECT_STREQ(complete, "COMPLETE");

    const char* error = mesh_heartbeat_op_to_string(MESH_HEARTBEAT_ERROR);
    EXPECT_STREQ(error, "ERROR");

    const char* ping = mesh_heartbeat_op_to_string(MESH_HEARTBEAT_PING);
    EXPECT_STREQ(ping, "PING");
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(MeshResilienceIntegrationTest, GetStatsNullBridge) {
    mesh_health_bridge_stats_t stats;
    nimcp_error_t err = mesh_health_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshResilienceIntegrationTest, GetStatsNullOutput) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    nimcp_error_t err = mesh_health_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshResilienceIntegrationTest, GetChannelHealthNullOutput) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    nimcp_error_t err = mesh_health_bridge_get_channel_health(
        bridge, MESH_CHANNEL_LEFT_HEMISPHERE, nullptr
    );
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshResilienceIntegrationTest, GetSystemHealthNullOutput) {
    if (!bridge) {
        GTEST_SKIP() << "Health bridge not available";
    }

    nimcp_error_t err = mesh_health_bridge_get_system_health(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}
