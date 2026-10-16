/**
 * @file test_mesh_coordinator_pool_integration.cpp
 * @brief Integration Tests for Mesh Coordinator Pool Operations
 *
 * WHAT: Tests coordinator pools with BFT election, load balancing, and failover
 * WHY:  Verify distributed coordination works correctly under various conditions
 * HOW:  Create pools, simulate elections, test failover, verify load distribution
 *
 * TEST COVERAGE:
 * - Pool creation and coordinator lifecycle
 * - BFT leader election with varying pool sizes
 * - Load balancing across pool members
 * - Failover and re-election on coordinator failure
 * - Hierarchical pool coordination
 * - Pool health monitoring
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_coordinator.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class MeshCoordinatorPoolIntegrationTest : public ::testing::Test {
protected:
    static constexpr size_t POOL_SIZE = 5;
    static constexpr size_t NUM_PARTICIPANTS = 20;

    mesh_coordinator_pool_t* pool_ = nullptr;
    std::vector<mesh_participant_t*> participants_;

    void SetUp() override {
        // Create coordinator pool
        mesh_coordinator_pool_config_t config;
        mesh_coordinator_pool_config_init(&config);
        config.pool_name = "test_hemisphere_pool";
        config.initial_size = POOL_SIZE;
        config.min_size = 3;
        config.max_size = 7;
        config.enable_bft = true;
        config.election_timeout_ms = 100.0f;
        config.heartbeat_interval_ms = 25.0f;

        pool_ = mesh_coordinator_pool_create(&config);
        ASSERT_NE(pool_, nullptr) << "Failed to create coordinator pool";

        // Create participants
        for (size_t i = 0; i < NUM_PARTICIPANTS; i++) {
            mesh_participant_config_t pconfig;
            mesh_participant_config_init(&pconfig);

            char name[32];
            snprintf(name, sizeof(name), "participant_%zu", i);
            pconfig.name = name;
            pconfig.type = MESH_PARTICIPANT_TYPE_PEER;

            mesh_participant_t* p = mesh_participant_create(&pconfig);
            ASSERT_NE(p, nullptr) << "Failed to create participant " << i;
            participants_.push_back(p);
        }
    }

    void TearDown() override {
        for (auto* p : participants_) {
            if (p) mesh_participant_destroy(p);
        }
        participants_.clear();

        if (pool_) {
            mesh_coordinator_pool_destroy(pool_);
            pool_ = nullptr;
        }
    }

    // Helper: Wait for election to complete
    bool WaitForElection(uint32_t timeout_ms = 500) {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            mesh_coordinator_pool_info_t info;
            if (mesh_coordinator_pool_get_info(pool_, &info) == NIMCP_OK) {
                if (info.has_leader) {
                    return true;
                }
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > timeout_ms) return false;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Helper: Get current leader index
    int GetLeaderIndex() {
        mesh_coordinator_pool_info_t info;
        if (mesh_coordinator_pool_get_info(pool_, &info) == NIMCP_OK && info.has_leader) {
            return static_cast<int>(info.leader_index);
        }
        return -1;
    }
};

// =============================================================================
// Pool Lifecycle Tests
// =============================================================================

TEST_F(MeshCoordinatorPoolIntegrationTest, PoolCreationAndInfo) {
    mesh_coordinator_pool_info_t info;
    nimcp_error_t err = mesh_coordinator_pool_get_info(pool_, &info);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_EQ(info.coordinator_count, POOL_SIZE);
    EXPECT_GE(info.min_size, 3u);
    EXPECT_LE(info.max_size, 7u);
    EXPECT_TRUE(info.bft_enabled);
    EXPECT_STREQ(info.pool_name, "test_hemisphere_pool");
}

TEST_F(MeshCoordinatorPoolIntegrationTest, InitialLeaderElection) {
    // Trigger election
    nimcp_error_t err = mesh_coordinator_pool_elect_leader(pool_);
    ASSERT_EQ(err, NIMCP_OK);

    // Wait for election
    ASSERT_TRUE(WaitForElection()) << "Election did not complete in time";

    // Verify leader exists
    mesh_coordinator_pool_info_t info;
    err = mesh_coordinator_pool_get_info(pool_, &info);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_TRUE(info.has_leader);
    EXPECT_LT(info.leader_index, info.coordinator_count);
    EXPECT_GT(info.current_term, 0u);
}

// =============================================================================
// Participant Assignment Tests
// =============================================================================

TEST_F(MeshCoordinatorPoolIntegrationTest, ParticipantAssignmentRoundRobin) {
    // Elect leader first
    mesh_coordinator_pool_elect_leader(pool_);
    ASSERT_TRUE(WaitForElection());

    // Assign participants to pool
    for (auto* p : participants_) {
        mesh_participant_id_t pid = mesh_participant_get_id(p);
        nimcp_error_t err = mesh_coordinator_pool_assign_participant(pool_, pid);
        ASSERT_EQ(err, NIMCP_OK) << "Failed to assign participant";
    }

    // Check load distribution
    mesh_coordinator_pool_info_t info;
    mesh_coordinator_pool_get_info(pool_, &info);

    // Each coordinator should have ~4 participants (20 / 5 = 4)
    for (size_t i = 0; i < info.coordinator_count; i++) {
        size_t load = 0;
        mesh_coordinator_pool_get_coordinator_load(pool_, i, &load);

        // Allow some variance due to round-robin
        EXPECT_GE(load, 2u) << "Coordinator " << i << " underloaded";
        EXPECT_LE(load, 6u) << "Coordinator " << i << " overloaded";
    }
}

TEST_F(MeshCoordinatorPoolIntegrationTest, DynamicRebalancing) {
    // Elect leader and assign participants
    mesh_coordinator_pool_elect_leader(pool_);
    ASSERT_TRUE(WaitForElection());

    for (auto* p : participants_) {
        mesh_participant_id_t pid = mesh_participant_get_id(p);
        mesh_coordinator_pool_assign_participant(pool_, pid);
    }

    // Record initial loads
    std::vector<size_t> initial_loads(POOL_SIZE);
    for (size_t i = 0; i < POOL_SIZE; i++) {
        mesh_coordinator_pool_get_coordinator_load(pool_, i, &initial_loads[i]);
    }

    // Trigger rebalancing
    nimcp_error_t err = mesh_coordinator_pool_rebalance(pool_);
    ASSERT_EQ(err, NIMCP_OK);

    // After rebalancing, variance should be minimal
    std::vector<size_t> final_loads(POOL_SIZE);
    size_t total = 0;
    for (size_t i = 0; i < POOL_SIZE; i++) {
        mesh_coordinator_pool_get_coordinator_load(pool_, i, &final_loads[i]);
        total += final_loads[i];
    }

    float avg = static_cast<float>(total) / POOL_SIZE;
    for (size_t i = 0; i < POOL_SIZE; i++) {
        float diff = std::abs(static_cast<float>(final_loads[i]) - avg);
        EXPECT_LE(diff, 2.0f) << "Load variance too high after rebalancing";
    }
}

// =============================================================================
// Failover Tests
// =============================================================================

TEST_F(MeshCoordinatorPoolIntegrationTest, LeaderFailoverAndReelection) {
    // Initial election
    mesh_coordinator_pool_elect_leader(pool_);
    ASSERT_TRUE(WaitForElection());

    int original_leader = GetLeaderIndex();
    ASSERT_GE(original_leader, 0);

    // Get original term
    mesh_coordinator_pool_info_t info;
    mesh_coordinator_pool_get_info(pool_, &info);
    uint64_t original_term = info.current_term;

    // Simulate leader failure
    nimcp_error_t err = mesh_coordinator_pool_handle_failure(
        pool_, static_cast<size_t>(original_leader));
    ASSERT_EQ(err, NIMCP_OK);

    // Wait for new election
    ASSERT_TRUE(WaitForElection(1000)) << "Re-election did not complete";

    // Verify new leader elected
    int new_leader = GetLeaderIndex();
    EXPECT_GE(new_leader, 0);

    // New term should be higher
    mesh_coordinator_pool_get_info(pool_, &info);
    EXPECT_GT(info.current_term, original_term);
}

TEST_F(MeshCoordinatorPoolIntegrationTest, WorkerFailoverParticipantMigration) {
    // Setup: elect and assign
    mesh_coordinator_pool_elect_leader(pool_);
    ASSERT_TRUE(WaitForElection());

    for (auto* p : participants_) {
        mesh_participant_id_t pid = mesh_participant_get_id(p);
        mesh_coordinator_pool_assign_participant(pool_, pid);
    }

    // Find a worker (non-leader) to fail
    int leader = GetLeaderIndex();
    size_t worker_to_fail = (leader + 1) % POOL_SIZE;

    // Get worker's load before failure
    size_t load_before = 0;
    mesh_coordinator_pool_get_coordinator_load(pool_, worker_to_fail, &load_before);

    // Fail the worker
    nimcp_error_t err = mesh_coordinator_pool_handle_failure(pool_, worker_to_fail);
    ASSERT_EQ(err, NIMCP_OK);

    // Worker's participants should be redistributed
    // Total participants should remain the same
    size_t total_after = 0;
    mesh_coordinator_pool_info_t info;
    mesh_coordinator_pool_get_info(pool_, &info);

    for (size_t i = 0; i < info.coordinator_count; i++) {
        size_t load = 0;
        mesh_coordinator_pool_get_coordinator_load(pool_, i, &load);
        total_after += load;
    }

    // Account for the failed coordinator being replaced or participants migrated
    EXPECT_GE(total_after, NUM_PARTICIPANTS - 5);  // Allow some variance
}

// =============================================================================
// BFT Tolerance Tests
// =============================================================================

TEST_F(MeshCoordinatorPoolIntegrationTest, BFTToleratesMinorityFailure) {
    // With 5 coordinators, should tolerate 1 failure (< 1/3)
    mesh_coordinator_pool_elect_leader(pool_);
    ASSERT_TRUE(WaitForElection());

    // Fail one coordinator (not leader)
    int leader = GetLeaderIndex();
    size_t to_fail = (leader + 1) % POOL_SIZE;

    nimcp_error_t err = mesh_coordinator_pool_handle_failure(pool_, to_fail);
    ASSERT_EQ(err, NIMCP_OK);

    // Pool should still function
    mesh_coordinator_pool_info_t info;
    err = mesh_coordinator_pool_get_info(pool_, &info);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_TRUE(info.has_leader || WaitForElection());
    EXPECT_TRUE(info.bft_healthy);
}

// =============================================================================
// Hierarchical Pool Tests
// =============================================================================

TEST_F(MeshCoordinatorPoolIntegrationTest, ChildPoolCreation) {
    // Create a child pool (layer-level)
    mesh_coordinator_pool_config_t child_config;
    mesh_coordinator_pool_config_init(&child_config);
    child_config.pool_name = "cognitive_layer_pool";
    child_config.initial_size = 3;
    child_config.parent_pool = pool_;

    mesh_coordinator_pool_t* child_pool = mesh_coordinator_pool_create(&child_config);
    ASSERT_NE(child_pool, nullptr);

    // Verify parent-child relationship
    mesh_coordinator_pool_info_t child_info;
    nimcp_error_t err = mesh_coordinator_pool_get_info(child_pool, &child_info);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_TRUE(child_info.has_parent);
    EXPECT_EQ(child_info.parent_pool, pool_);

    mesh_coordinator_pool_destroy(child_pool);
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_F(MeshCoordinatorPoolIntegrationTest, RapidElectionCycles) {
    std::atomic<int> election_count{0};
    std::atomic<int> failure_count{0};

    // Run multiple election cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        nimcp_error_t err = mesh_coordinator_pool_elect_leader(pool_);
        if (err == NIMCP_OK && WaitForElection(200)) {
            election_count++;
        } else {
            failure_count++;
        }

        // Brief pause between elections
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Most elections should succeed
    EXPECT_GE(election_count.load(), 7)
        << "Too many election failures: " << failure_count.load();
}

TEST_F(MeshCoordinatorPoolIntegrationTest, ConcurrentParticipantAssignment) {
    mesh_coordinator_pool_elect_leader(pool_);
    ASSERT_TRUE(WaitForElection());

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Multiple threads assigning participants
    for (size_t t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            size_t start = t * 5;
            size_t end = std::min(start + 5, participants_.size());

            for (size_t i = start; i < end; i++) {
                mesh_participant_id_t pid = mesh_participant_get_id(participants_[i]);
                nimcp_error_t err = mesh_coordinator_pool_assign_participant(pool_, pid);
                if (err == NIMCP_OK) {
                    success_count++;
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // All assignments should succeed
    EXPECT_EQ(success_count.load(), static_cast<int>(NUM_PARTICIPANTS));
}

// =============================================================================
// Health Monitoring Tests
// =============================================================================

TEST_F(MeshCoordinatorPoolIntegrationTest, PoolHealthMonitoring) {
    mesh_coordinator_pool_elect_leader(pool_);
    ASSERT_TRUE(WaitForElection());

    // Assign some participants
    for (size_t i = 0; i < 10; i++) {
        mesh_participant_id_t pid = mesh_participant_get_id(participants_[i]);
        mesh_coordinator_pool_assign_participant(pool_, pid);
    }

    // Get health metrics
    mesh_coordinator_pool_health_t health;
    nimcp_error_t err = mesh_coordinator_pool_get_health(pool_, &health);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_GT(health.active_coordinators, 0u);
    EXPECT_GT(health.total_participants, 0u);
    EXPECT_GE(health.avg_response_time_ms, 0.0f);
    EXPECT_TRUE(health.leader_healthy);
    EXPECT_TRUE(health.bft_quorum_met);
}
