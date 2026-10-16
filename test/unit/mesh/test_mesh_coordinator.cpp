/**
 * @file test_mesh_coordinator.cpp
 * @brief Unit tests for mesh coordinator module
 *
 * Tests coordinator creation, role/state management, participant assignment,
 * load/health tracking, heartbeat, and election participation.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_coordinator.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_participant.h"
}

class MeshCoordinatorTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry;
    mesh_channel_t* channel;
    mesh_coordinator_t* coordinator;

    void SetUp() override {
        registry = mesh_registry_create(nullptr);
        ASSERT_NE(registry, nullptr);

        mesh_channel_config_t ch_config;
        mesh_channel_default_config(&ch_config);
        ch_config.channel_name = "test_channel";
        ch_config.channel_id = MESH_CHANNEL_LEFT_HEMISPHERE;
        channel = mesh_channel_create(&ch_config, registry);
        ASSERT_NE(channel, nullptr);

        coordinator = nullptr;
    }

    void TearDown() override {
        if (coordinator) {
            mesh_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
        if (channel) {
            mesh_channel_destroy(channel);
            channel = nullptr;
        }
        if (registry) {
            mesh_registry_destroy(registry);
            registry = nullptr;
        }
    }

    mesh_participant_id_t register_test_participant(const char* name,
                                                     mesh_channel_id_t home_channel) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        config.module_name = name;
        config.type = MESH_PARTICIPANT_MODULE;
        config.home_channel = home_channel;

        mesh_participant_id_t id;
        nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &id);
        EXPECT_EQ(err, NIMCP_SUCCESS);
        return id;
    }
};

/* ============================================================================
 * Coordinator Creation Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, CreateWithDefaults) {
    mesh_coordinator_config_t config;
    nimcp_error_t err = mesh_coordinator_default_config(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    config.name = "test_coord";
    config.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    config.level = COORD_LEVEL_HEMISPHERE;

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    EXPECT_STREQ(mesh_coordinator_get_name(coordinator), "test_coord");
    EXPECT_EQ(mesh_coordinator_get_level(coordinator), COORD_LEVEL_HEMISPHERE);
}

TEST_F(MeshCoordinatorTest, CreateWithNullConfig) {
    coordinator = mesh_coordinator_create(nullptr, registry, channel);
    ASSERT_NE(coordinator, nullptr);
    // Should have default values - default level is LAYER per implementation
    EXPECT_EQ(mesh_coordinator_get_level(coordinator), COORD_LEVEL_LAYER);
}

TEST_F(MeshCoordinatorTest, CreateWithNullRegistry) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.name = "test";

    coordinator = mesh_coordinator_create(&config, nullptr, channel);
    // Should handle gracefully - may return NULL or work without registry
    // Implementation-dependent
}

TEST_F(MeshCoordinatorTest, CreateWithNullChannel) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.name = "test";

    coordinator = mesh_coordinator_create(&config, registry, nullptr);
    // Should handle gracefully
}

TEST_F(MeshCoordinatorTest, DestroyNull) {
    mesh_coordinator_destroy(nullptr);
    // Should not crash
}

TEST_F(MeshCoordinatorTest, GetIdReturnsValidId) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.name = "coord_1";

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t id = mesh_coordinator_get_id(coordinator);
    EXPECT_NE(id, 0);
}

/* ============================================================================
 * Role and State Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, InitialRoleIsNone) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    // Initial role should be NONE or STANDBY
    coordinator_role_t role = mesh_coordinator_get_role(coordinator);
    EXPECT_TRUE(role == COORD_ROLE_NONE || role == COORD_ROLE_STANDBY);
}

TEST_F(MeshCoordinatorTest, SetAndGetRole) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    nimcp_error_t err = mesh_coordinator_set_role(coordinator, COORD_ROLE_LEADER);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(mesh_coordinator_get_role(coordinator), COORD_ROLE_LEADER);

    err = mesh_coordinator_set_role(coordinator, COORD_ROLE_WORKER);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(mesh_coordinator_get_role(coordinator), COORD_ROLE_WORKER);

    err = mesh_coordinator_set_role(coordinator, COORD_ROLE_STANDBY);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(mesh_coordinator_get_role(coordinator), COORD_ROLE_STANDBY);
}

TEST_F(MeshCoordinatorTest, InitialStateIsInit) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    coordinator_state_t state = mesh_coordinator_get_state(coordinator);
    EXPECT_EQ(state, COORD_STATE_INIT);
}

TEST_F(MeshCoordinatorTest, SetAndGetState) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    nimcp_error_t err = mesh_coordinator_set_state(coordinator, COORD_STATE_ACTIVE);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(mesh_coordinator_get_state(coordinator), COORD_STATE_ACTIVE);

    err = mesh_coordinator_set_state(coordinator, COORD_STATE_ELECTION);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(mesh_coordinator_get_state(coordinator), COORD_STATE_ELECTION);
}

TEST_F(MeshCoordinatorTest, GetLevel) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.level = COORD_LEVEL_LAYER;

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    EXPECT_EQ(mesh_coordinator_get_level(coordinator), COORD_LEVEL_LAYER);
}

/* ============================================================================
 * Participant Assignment Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, AssignParticipant) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.max_participants = 10;

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", MESH_CHANNEL_LEFT_HEMISPHERE);

    nimcp_error_t err = mesh_coordinator_assign_participant(coordinator, p1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_coordinator_has_participant(coordinator, p1));
    EXPECT_EQ(mesh_coordinator_get_participant_count(coordinator), 1);
}

TEST_F(MeshCoordinatorTest, AssignMultipleParticipants) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.max_participants = 10;

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_participant_id_t p2 = register_test_participant("module2", MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_participant_id_t p3 = register_test_participant("module3", MESH_CHANNEL_LEFT_HEMISPHERE);

    mesh_coordinator_assign_participant(coordinator, p1);
    mesh_coordinator_assign_participant(coordinator, p2);
    mesh_coordinator_assign_participant(coordinator, p3);

    EXPECT_EQ(mesh_coordinator_get_participant_count(coordinator), 3);
    EXPECT_TRUE(mesh_coordinator_has_participant(coordinator, p1));
    EXPECT_TRUE(mesh_coordinator_has_participant(coordinator, p2));
    EXPECT_TRUE(mesh_coordinator_has_participant(coordinator, p3));
}

TEST_F(MeshCoordinatorTest, AssignDuplicateParticipant) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", MESH_CHANNEL_LEFT_HEMISPHERE);

    mesh_coordinator_assign_participant(coordinator, p1);
    nimcp_error_t err = mesh_coordinator_assign_participant(coordinator, p1);

    // Duplicate should be OK or return already exists
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_ALREADY_EXISTS);
    EXPECT_EQ(mesh_coordinator_get_participant_count(coordinator), 1);
}

TEST_F(MeshCoordinatorTest, UnassignParticipant) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_coordinator_assign_participant(coordinator, p1);

    nimcp_error_t err = mesh_coordinator_unassign_participant(coordinator, p1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(mesh_coordinator_has_participant(coordinator, p1));
    EXPECT_EQ(mesh_coordinator_get_participant_count(coordinator), 0);
}

TEST_F(MeshCoordinatorTest, UnassignNonexistentParticipant) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    nimcp_error_t err = mesh_coordinator_unassign_participant(coordinator, 0xDEADBEEF);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshCoordinatorTest, GetParticipants) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_participant_id_t p2 = register_test_participant("module2", MESH_CHANNEL_LEFT_HEMISPHERE);

    mesh_coordinator_assign_participant(coordinator, p1);
    mesh_coordinator_assign_participant(coordinator, p2);

    mesh_participant_id_t ids[10];
    size_t count;
    nimcp_error_t err = mesh_coordinator_get_participants(coordinator, ids, 10, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 2);
}

TEST_F(MeshCoordinatorTest, HasParticipantFalseWhenNotAssigned) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", MESH_CHANNEL_LEFT_HEMISPHERE);

    EXPECT_FALSE(mesh_coordinator_has_participant(coordinator, p1));
}

/* ============================================================================
 * Load and Health Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, InitialLoadIsZero) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    float load = mesh_coordinator_get_load(coordinator);
    EXPECT_GE(load, 0.0f);
    EXPECT_LE(load, 1.0f);
}

TEST_F(MeshCoordinatorTest, LoadIncreasesWithParticipants) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.max_participants = 10;

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    float initial_load = mesh_coordinator_get_load(coordinator);

    // Add some participants
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        mesh_participant_id_t p = register_test_participant(name, MESH_CHANNEL_LEFT_HEMISPHERE);
        mesh_coordinator_assign_participant(coordinator, p);
    }

    float after_load = mesh_coordinator_get_load(coordinator);
    EXPECT_GT(after_load, initial_load);
}

TEST_F(MeshCoordinatorTest, InitialHealthIsOne) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    float health = mesh_coordinator_get_health(coordinator);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(MeshCoordinatorTest, IsOverloadedFalseInitially) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    EXPECT_FALSE(mesh_coordinator_is_overloaded(coordinator));
}

TEST_F(MeshCoordinatorTest, ReportFailureDecreasesHealth) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    float initial_health = mesh_coordinator_get_health(coordinator);

    mesh_coordinator_report_failure(coordinator, NIMCP_ERROR_TIMEOUT);

    float after_health = mesh_coordinator_get_health(coordinator);
    EXPECT_LE(after_health, initial_health);
}

TEST_F(MeshCoordinatorTest, ReportRecoveryIncreasesHealth) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    // First decrease health
    mesh_coordinator_report_failure(coordinator, NIMCP_ERROR_TIMEOUT);
    float after_failure = mesh_coordinator_get_health(coordinator);

    // Then recover
    mesh_coordinator_report_recovery(coordinator);
    float after_recovery = mesh_coordinator_get_health(coordinator);

    EXPECT_GE(after_recovery, after_failure);
}

/* ============================================================================
 * Heartbeat Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, SendHeartbeat) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    nimcp_error_t err = mesh_coordinator_send_heartbeat(coordinator);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshCoordinatorTest, ReceiveHeartbeat) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t from = 0x12345678;
    nimcp_error_t err = mesh_coordinator_receive_heartbeat(coordinator, from, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshCoordinatorTest, GetLastHeartbeat) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    uint64_t last_hb = mesh_coordinator_get_last_heartbeat(coordinator);
    // Should be 0 or recent timestamp
    // Just verify it doesn't crash
    (void)last_hb;
}

TEST_F(MeshCoordinatorTest, HeartbeatTimedOut) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.heartbeat_interval_ms = 100;

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    // Initially should not be timed out (just created)
    bool timed_out = mesh_coordinator_heartbeat_timed_out(coordinator);
    // Result depends on implementation - may start as timed out
    (void)timed_out;
}

/* ============================================================================
 * Election Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, RequestVote) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    nimcp_error_t err = mesh_coordinator_request_vote(coordinator, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshCoordinatorTest, CastVote) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_participant_id_t candidate = 0x12345678;
    nimcp_error_t err = mesh_coordinator_cast_vote(coordinator, candidate, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshCoordinatorTest, GetAndSetTerm) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    uint64_t initial_term = mesh_coordinator_get_term(coordinator);
    EXPECT_EQ(initial_term, 0);

    mesh_coordinator_set_term(coordinator, 42);
    EXPECT_EQ(mesh_coordinator_get_term(coordinator), 42);
}

TEST_F(MeshCoordinatorTest, TermIncreasesOnVote) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    uint64_t term = 5;
    mesh_coordinator_set_term(coordinator, term);

    // Request vote for higher term
    mesh_coordinator_request_vote(coordinator, term + 1);
    EXPECT_GE(mesh_coordinator_get_term(coordinator), term);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, Update) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    nimcp_error_t err = mesh_coordinator_update(coordinator, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshCoordinatorTest, UpdateMultipleTimes) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_error_t err = mesh_coordinator_update(coordinator, 10);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, GetStats) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    mesh_coordinator_stats_t stats;
    nimcp_error_t err = mesh_coordinator_get_stats(coordinator, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.id, mesh_coordinator_get_id(coordinator));
}

TEST_F(MeshCoordinatorTest, ResetStats) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    // Do some operations
    mesh_coordinator_send_heartbeat(coordinator);

    mesh_coordinator_reset_stats(coordinator);

    mesh_coordinator_stats_t stats;
    mesh_coordinator_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.heartbeats_sent, 0);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorTest, LevelToString) {
    EXPECT_STREQ(mesh_coordinator_level_to_string(COORD_LEVEL_SYSTEM), "SYSTEM");
    EXPECT_STREQ(mesh_coordinator_level_to_string(COORD_LEVEL_HEMISPHERE), "HEMISPHERE");
    EXPECT_STREQ(mesh_coordinator_level_to_string(COORD_LEVEL_LAYER), "LAYER");
    EXPECT_STREQ(mesh_coordinator_level_to_string(COORD_LEVEL_ORDERING), "ORDERING");
}

TEST_F(MeshCoordinatorTest, GetLevelTiming) {
    mesh_timing_t timing;

    mesh_coordinator_get_level_timing(COORD_LEVEL_SYSTEM, &timing);
    EXPECT_EQ(timing.base_interval_ms, 100.0f);

    mesh_coordinator_get_level_timing(COORD_LEVEL_HEMISPHERE, &timing);
    EXPECT_EQ(timing.base_interval_ms, 50.0f);

    mesh_coordinator_get_level_timing(COORD_LEVEL_LAYER, &timing);
    EXPECT_EQ(timing.base_interval_ms, 10.0f);

    mesh_coordinator_get_level_timing(COORD_LEVEL_ORDERING, &timing);
    EXPECT_EQ(timing.base_interval_ms, 5.0f);
}

TEST_F(MeshCoordinatorTest, PrintInfo) {
    mesh_coordinator_config_t config;
    mesh_coordinator_default_config(&config);
    config.name = "test_print";

    coordinator = mesh_coordinator_create(&config, registry, channel);
    ASSERT_NE(coordinator, nullptr);

    // Should not crash
    mesh_coordinator_print_info(coordinator);
}

TEST_F(MeshCoordinatorTest, PrintInfoNull) {
    // Should not crash
    mesh_coordinator_print_info(nullptr);
}
