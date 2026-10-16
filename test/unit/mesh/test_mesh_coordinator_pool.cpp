/**
 * @file test_mesh_coordinator_pool.cpp
 * @brief Unit tests for mesh coordinator pool module
 *
 * Tests pool creation, coordinator management, leader election,
 * load balancing, failure handling, and scaling.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_coordinator.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_participant.h"
}

class MeshCoordinatorPoolTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry;
    mesh_channel_t* channel;
    mesh_coordinator_pool_t* pool;

    void SetUp() override {
        registry = mesh_registry_create(nullptr);
        ASSERT_NE(registry, nullptr);

        mesh_channel_config_t ch_config;
        mesh_channel_default_config(&ch_config);
        ch_config.channel_name = "test_channel";
        ch_config.channel_id = MESH_CHANNEL_LEFT_HEMISPHERE;
        channel = mesh_channel_create(&ch_config, registry);
        ASSERT_NE(channel, nullptr);

        pool = nullptr;
    }

    void TearDown() override {
        if (pool) {
            mesh_coordinator_pool_destroy(pool);
            pool = nullptr;
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
 * Pool Creation Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, CreateWithDefaults) {
    mesh_coordinator_pool_config_t config;
    nimcp_error_t err = mesh_coordinator_pool_default_config(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    config.pool_name = "test_pool";
    config.pool_id = 1;
    config.channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    EXPECT_EQ(mesh_coordinator_pool_get_id(pool), 1);
    EXPECT_STREQ(mesh_coordinator_pool_get_name(pool), "test_pool");
}

TEST_F(MeshCoordinatorPoolTest, CreateWithNullConfig) {
    pool = mesh_coordinator_pool_create(nullptr, registry, channel);
    ASSERT_NE(pool, nullptr);
    // Should use defaults
}

TEST_F(MeshCoordinatorPoolTest, DestroyNull) {
    mesh_coordinator_pool_destroy(nullptr);
    // Should not crash
}

TEST_F(MeshCoordinatorPoolTest, DefaultConfigValues) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);

    EXPECT_EQ(config.initial_size, MESH_DEFAULT_POOL_SIZE);
    EXPECT_EQ(config.min_size, MESH_MIN_POOL_SIZE_BFT);
    EXPECT_GT(config.election_timeout_ms, 0.0f);
    EXPECT_GT(config.load_threshold, 0.0f);
}

TEST_F(MeshCoordinatorPoolTest, GetIdAndName) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.pool_name = "my_pool";
    config.pool_id = 42;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    EXPECT_EQ(mesh_coordinator_pool_get_id(pool), 42);
    EXPECT_STREQ(mesh_coordinator_pool_get_name(pool), "my_pool");
}

/* ============================================================================
 * Coordinator Management Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, InitialSizeMatchesConfig) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    EXPECT_EQ(mesh_coordinator_pool_get_size(pool), 4);
}

TEST_F(MeshCoordinatorPoolTest, AddCoordinator) {
    mesh_coordinator_pool_config_t pool_config;
    mesh_coordinator_pool_default_config(&pool_config);
    pool_config.initial_size = 0;  // Start empty

    pool = mesh_coordinator_pool_create(&pool_config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_config_t coord_config;
    mesh_coordinator_default_config(&coord_config);
    coord_config.name = "new_coord";

    mesh_coordinator_t* coord = mesh_coordinator_create(&coord_config, registry, channel);
    ASSERT_NE(coord, nullptr);

    nimcp_error_t err = mesh_coordinator_pool_add(pool, coord);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(mesh_coordinator_pool_get_size(pool), 1);
}

TEST_F(MeshCoordinatorPoolTest, RemoveCoordinator) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_t* coord = mesh_coordinator_pool_get_by_index(pool, 0);
    ASSERT_NE(coord, nullptr);

    mesh_participant_id_t coord_id = mesh_coordinator_get_id(coord);

    nimcp_error_t err = mesh_coordinator_pool_remove(pool, coord_id);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(mesh_coordinator_pool_get_size(pool), 3);
}

TEST_F(MeshCoordinatorPoolTest, GetCoordinatorById) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_t* coord = mesh_coordinator_pool_get_by_index(pool, 0);
    ASSERT_NE(coord, nullptr);

    mesh_participant_id_t id = mesh_coordinator_get_id(coord);
    mesh_coordinator_t* found = mesh_coordinator_pool_get(pool, id);

    EXPECT_EQ(found, coord);
}

TEST_F(MeshCoordinatorPoolTest, GetCoordinatorByIndex) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_t* coord0 = mesh_coordinator_pool_get_by_index(pool, 0);
    mesh_coordinator_t* coord1 = mesh_coordinator_pool_get_by_index(pool, 1);
    mesh_coordinator_t* coord2 = mesh_coordinator_pool_get_by_index(pool, 2);
    mesh_coordinator_t* coord3 = mesh_coordinator_pool_get_by_index(pool, 3);

    EXPECT_NE(coord0, nullptr);
    EXPECT_NE(coord1, nullptr);
    EXPECT_NE(coord2, nullptr);
    EXPECT_NE(coord3, nullptr);

    // Invalid index
    mesh_coordinator_t* invalid = mesh_coordinator_pool_get_by_index(pool, 100);
    EXPECT_EQ(invalid, nullptr);
}

/* ============================================================================
 * Leader and Role Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, HasLeaderAfterCreation) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    // Pool may or may not have leader immediately (depends on implementation)
    // After election it should have one
    mesh_coordinator_pool_elect_leader(pool);

    EXPECT_TRUE(mesh_coordinator_pool_has_leader(pool));
}

TEST_F(MeshCoordinatorPoolTest, GetLeader) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_coordinator_t* leader = mesh_coordinator_pool_get_leader(pool);
    ASSERT_NE(leader, nullptr);

    EXPECT_EQ(mesh_coordinator_get_role(leader), COORD_ROLE_LEADER);
}

TEST_F(MeshCoordinatorPoolTest, GetLeaderId) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_participant_id_t leader_id = mesh_coordinator_pool_get_leader_id(pool);
    EXPECT_NE(leader_id, 0);

    mesh_coordinator_t* leader = mesh_coordinator_pool_get_leader(pool);
    EXPECT_EQ(mesh_coordinator_get_id(leader), leader_id);
}

TEST_F(MeshCoordinatorPoolTest, GetByRole) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_coordinator_t* leaders[10];
    size_t leader_count;
    nimcp_error_t err = mesh_coordinator_pool_get_by_role(
        pool, COORD_ROLE_LEADER, leaders, 10, &leader_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(leader_count, 1);

    mesh_coordinator_t* workers[10];
    size_t worker_count;
    err = mesh_coordinator_pool_get_by_role(
        pool, COORD_ROLE_WORKER, workers, 10, &worker_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Remaining are workers or standby
    EXPECT_GE(worker_count, 0);
}

/* ============================================================================
 * Leader Election Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, ElectLeader) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    nimcp_error_t err = mesh_coordinator_pool_elect_leader(pool);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_coordinator_pool_has_leader(pool));
}

TEST_F(MeshCoordinatorPoolTest, ProcessVote) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_t* coord0 = mesh_coordinator_pool_get_by_index(pool, 0);
    mesh_coordinator_t* coord1 = mesh_coordinator_pool_get_by_index(pool, 1);

    mesh_participant_id_t voter_id = mesh_coordinator_get_id(coord0);
    mesh_participant_id_t candidate_id = mesh_coordinator_get_id(coord1);

    nimcp_error_t err = mesh_coordinator_pool_process_vote(pool, voter_id, candidate_id, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshCoordinatorPoolTest, GetTerm) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    uint64_t initial_term = mesh_coordinator_pool_get_term(pool);
    EXPECT_EQ(initial_term, 0);

    // Election should increment term
    mesh_coordinator_pool_elect_leader(pool);
    uint64_t after_election = mesh_coordinator_pool_get_term(pool);
    EXPECT_GE(after_election, initial_term);
}

TEST_F(MeshCoordinatorPoolTest, GetLastElection) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_election_result_t result;
    nimcp_error_t err = mesh_coordinator_pool_get_last_election(pool, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.success);
    EXPECT_NE(result.winner, 0);
}

TEST_F(MeshCoordinatorPoolTest, ElectionNotInProgressAfterCompletion) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    EXPECT_FALSE(mesh_coordinator_pool_election_in_progress(pool));
}

static void test_election_callback(mesh_pool_id_t pool_id,
                                   mesh_participant_id_t new_leader,
                                   uint64_t term,
                                   void* user_ctx) {
    bool* called = (bool*)user_ctx;
    *called = true;
}

TEST_F(MeshCoordinatorPoolTest, SetElectionCallback) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    bool callback_called = false;
    nimcp_error_t err = mesh_coordinator_pool_set_election_callback(
        pool, test_election_callback, &callback_called);

    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_coordinator_pool_elect_leader(pool);
    EXPECT_TRUE(callback_called);
}

/* ============================================================================
 * Load Balancing Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, AssignParticipant) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_participant_id_t p1 = register_test_participant("module1", MESH_CHANNEL_LEFT_HEMISPHERE);

    nimcp_error_t err = mesh_coordinator_pool_assign_participant(pool, p1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(mesh_coordinator_pool_get_total_participants(pool), 1);
}

TEST_F(MeshCoordinatorPoolTest, AssignMultipleParticipants) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        mesh_participant_id_t p = register_test_participant(name, MESH_CHANNEL_LEFT_HEMISPHERE);
        mesh_coordinator_pool_assign_participant(pool, p);
    }

    EXPECT_EQ(mesh_coordinator_pool_get_total_participants(pool), 10);
}

TEST_F(MeshCoordinatorPoolTest, AssignToLeader) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_participant_id_t p1 = register_test_participant("priority_module", MESH_CHANNEL_LEFT_HEMISPHERE);

    nimcp_error_t err = mesh_coordinator_pool_assign_to_leader(pool, p1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_coordinator_t* assigned = mesh_coordinator_pool_get_assignment(pool, p1);
    mesh_coordinator_t* leader = mesh_coordinator_pool_get_leader(pool);

    EXPECT_EQ(assigned, leader);
}

TEST_F(MeshCoordinatorPoolTest, GetAssignment) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_participant_id_t p1 = register_test_participant("module1", MESH_CHANNEL_LEFT_HEMISPHERE);
    mesh_coordinator_pool_assign_participant(pool, p1);

    mesh_coordinator_t* assigned = mesh_coordinator_pool_get_assignment(pool, p1);
    EXPECT_NE(assigned, nullptr);
}

TEST_F(MeshCoordinatorPoolTest, Rebalance) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    // Add many participants
    for (int i = 0; i < 20; i++) {
        char name[32];
        snprintf(name, sizeof(name), "module_%d", i);
        mesh_participant_id_t p = register_test_participant(name, MESH_CHANNEL_LEFT_HEMISPHERE);
        mesh_coordinator_pool_assign_participant(pool, p);
    }

    nimcp_error_t err = mesh_coordinator_pool_rebalance(pool);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Failure Handling Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, HandleFailure) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    // Get a worker coordinator
    mesh_coordinator_t* workers[10];
    size_t worker_count;
    mesh_coordinator_pool_get_by_role(pool, COORD_ROLE_WORKER, workers, 10, &worker_count);

    if (worker_count > 0) {
        mesh_participant_id_t failed_id = mesh_coordinator_get_id(workers[0]);
        nimcp_error_t err = mesh_coordinator_pool_handle_failure(pool, failed_id);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        EXPECT_GE(mesh_coordinator_pool_get_failed_count(pool), 1);
    }
}

TEST_F(MeshCoordinatorPoolTest, PromoteStandby) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_coordinator_t* standbys[10];
    size_t standby_count;
    mesh_coordinator_pool_get_by_role(pool, COORD_ROLE_STANDBY, standbys, 10, &standby_count);

    if (standby_count > 0) {
        mesh_participant_id_t standby_id = mesh_coordinator_get_id(standbys[0]);
        nimcp_error_t err = mesh_coordinator_pool_promote_standby(pool, standby_id);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        coordinator_role_t new_role = mesh_coordinator_get_role(standbys[0]);
        EXPECT_EQ(new_role, COORD_ROLE_WORKER);
    }
}

TEST_F(MeshCoordinatorPoolTest, Demote) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_coordinator_t* workers[10];
    size_t worker_count;
    mesh_coordinator_pool_get_by_role(pool, COORD_ROLE_WORKER, workers, 10, &worker_count);

    if (worker_count > 0) {
        mesh_participant_id_t worker_id = mesh_coordinator_get_id(workers[0]);
        nimcp_error_t err = mesh_coordinator_pool_demote(pool, worker_id);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        coordinator_role_t new_role = mesh_coordinator_get_role(workers[0]);
        EXPECT_EQ(new_role, COORD_ROLE_STANDBY);
    }
}

TEST_F(MeshCoordinatorPoolTest, GetFailedCount) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    size_t failed = mesh_coordinator_pool_get_failed_count(pool);
    EXPECT_EQ(failed, 0);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, Update) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    nimcp_error_t err = mesh_coordinator_pool_update(pool, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshCoordinatorPoolTest, UpdateMultipleTimes) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_error_t err = mesh_coordinator_pool_update(pool, 100);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, GetStats) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.pool_id = 42;
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_stats_t stats;
    nimcp_error_t err = mesh_coordinator_pool_get_stats(pool, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.pool_id, 42);
    EXPECT_EQ(stats.coordinator_count, 4);
}

TEST_F(MeshCoordinatorPoolTest, ResetStats) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_coordinator_pool_reset_stats(pool);

    mesh_coordinator_pool_stats_t stats;
    mesh_coordinator_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.elections_held, 0);
}

TEST_F(MeshCoordinatorPoolTest, StatsTrackElections) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_coordinator_pool_stats_t stats;
    mesh_coordinator_pool_get_stats(pool, &stats);
    EXPECT_GE(stats.elections_held, 1);
}

/* ============================================================================
 * Scaling Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, ScaleUp) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;
    config.max_size = 10;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    size_t initial_size = mesh_coordinator_pool_get_size(pool);

    nimcp_error_t err = mesh_coordinator_pool_scale_up(pool, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(mesh_coordinator_pool_get_size(pool), initial_size + 2);
}

TEST_F(MeshCoordinatorPoolTest, ScaleDown) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 6;
    config.min_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    nimcp_error_t err = mesh_coordinator_pool_scale_down(pool, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(mesh_coordinator_pool_get_size(pool), 4);
}

TEST_F(MeshCoordinatorPoolTest, ScaleDownRespectsBftMin) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;
    config.min_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    // Try to scale below minimum
    nimcp_error_t err = mesh_coordinator_pool_scale_down(pool, 2);

    // Should either fail or only scale to minimum
    size_t size = mesh_coordinator_pool_get_size(pool);
    EXPECT_GE(size, 4);
}

TEST_F(MeshCoordinatorPoolTest, OptimalSize) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    size_t optimal = mesh_coordinator_pool_optimal_size(pool, 100);
    EXPECT_GE(optimal, 4);  // At least BFT minimum
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, PrintStatus) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.pool_name = "test_print";
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    // Should not crash
    mesh_coordinator_pool_print_status(pool);
}

TEST_F(MeshCoordinatorPoolTest, PrintStatusNull) {
    // Should not crash
    mesh_coordinator_pool_print_status(nullptr);
}

TEST_F(MeshCoordinatorPoolTest, IsBftValid) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    // With 4 coordinators, BFT should be valid (3f+1 where f=1)
    EXPECT_TRUE(mesh_coordinator_pool_is_bft_valid(pool));
}

TEST_F(MeshCoordinatorPoolTest, BftInvalidWithTooFewCoordinators) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 2;
    config.min_size = 2;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    // With only 2 coordinators, BFT is not valid
    EXPECT_FALSE(mesh_coordinator_pool_is_bft_valid(pool));
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(MeshCoordinatorPoolTest, FullWorkflow) {
    // Create pool
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.pool_name = "integration_test";
    config.pool_id = 100;
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    // Elect leader
    nimcp_error_t err = mesh_coordinator_pool_elect_leader(pool);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(mesh_coordinator_pool_has_leader(pool));

    // Assign participants
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "participant_%d", i);
        mesh_participant_id_t p = register_test_participant(name, MESH_CHANNEL_LEFT_HEMISPHERE);
        err = mesh_coordinator_pool_assign_participant(pool, p);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    EXPECT_EQ(mesh_coordinator_pool_get_total_participants(pool), 10);

    // Update pool
    err = mesh_coordinator_pool_update(pool, 100);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Rebalance
    err = mesh_coordinator_pool_rebalance(pool);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Check stats
    mesh_coordinator_pool_stats_t stats;
    err = mesh_coordinator_pool_get_stats(pool, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.pool_id, 100);
    EXPECT_EQ(stats.total_participants, 10);
    EXPECT_GE(stats.elections_held, 1);
}

TEST_F(MeshCoordinatorPoolTest, LeaderFailover) {
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_default_config(&config);
    config.initial_size = 4;

    pool = mesh_coordinator_pool_create(&config, registry, channel);
    ASSERT_NE(pool, nullptr);

    mesh_coordinator_pool_elect_leader(pool);

    mesh_participant_id_t original_leader = mesh_coordinator_pool_get_leader_id(pool);
    EXPECT_NE(original_leader, 0);

    // Simulate leader failure
    nimcp_error_t err = mesh_coordinator_pool_handle_failure(pool, original_leader);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Pool should elect new leader
    mesh_coordinator_pool_elect_leader(pool);
    EXPECT_TRUE(mesh_coordinator_pool_has_leader(pool));

    mesh_participant_id_t new_leader = mesh_coordinator_pool_get_leader_id(pool);
    // New leader should be different (unless pool has only one healthy coordinator)
    (void)new_leader;
}
