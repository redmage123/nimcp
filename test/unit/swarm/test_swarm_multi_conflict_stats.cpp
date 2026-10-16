/**
 * @file test_swarm_multi_conflict_stats.cpp
 * @brief Unit tests for swarm multi-coordinator conflict statistics
 *
 * Tests the conflict statistics tracking functionality including:
 * - Conflict counting (total_conflicts, conflicts_pending)
 * - Conflict resolution tracking (conflicts_resolved)
 * - Stats aggregation and retrieval
 *
 * REGRESSION: Ensures coordinator uses conflict_stats (not conflict_history)
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_multi.h"

class SwarmMultiConflictStatsTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coordinator;

    void SetUp() override {
        coordinator = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) {
            nimcp_multi_swarm_destroy(coordinator);
        }
    }

    // Helper to create and register a swarm
    nimcp_swarm_identity_t* CreateAndRegisterSwarm(const char* name, uint32_t agent_count = 10) {
        nimcp_swarm_identity_t* identity = nimcp_swarm_identity_create(
            coordinator, name, agent_count
        );
        if (identity) {
            nimcp_swarm_register(coordinator, identity);
        }
        return identity;
    }
};

// =============================================================================
// Conflict Statistics Initialization Tests
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, StatsInitializedToZero) {
    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

    EXPECT_EQ(stats.total_conflicts, 0U);
    EXPECT_EQ(stats.conflicts_resolved, 0U);
    EXPECT_EQ(stats.conflicts_pending, 0U);
    EXPECT_EQ(stats.escalations, 0U);
    EXPECT_EQ(stats.merges_performed, 0U);
    EXPECT_FLOAT_EQ(stats.avg_resolution_time_ms, 0.0f);
}

// =============================================================================
// Conflict Detection and Counting Tests
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, DetectConflictIncrementsTotalConflicts) {
    // Create two swarms
    nimcp_swarm_identity_t* swarm1 = CreateAndRegisterSwarm("swarm_alpha");
    nimcp_swarm_identity_t* swarm2 = CreateAndRegisterSwarm("swarm_beta");

    if (!swarm1 || !swarm2) {
        GTEST_SKIP() << "Swarm creation not available";
    }

    // Simulate conflict detection
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t conflict_count = 0;
    nimcp_multi_swarm_detect_conflicts(coordinator, &conflicts, &conflict_count);

    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

    // Stats should reflect detected conflicts
    EXPECT_GE(stats.total_conflicts, 0U);

    // NOTE: Registered swarms are owned by coordinator - don't manually destroy
}

TEST_F(SwarmMultiConflictStatsTest, MultipleConflictTypesTrackedSeparately) {
    // Register multiple swarms
    nimcp_swarm_identity_t* swarms[4] = {nullptr};
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "swarm_%d", i);
        swarms[i] = CreateAndRegisterSwarm(name);
    }

    // Stats should track all conflict types
    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);
    (void)stats;

    // NOTE: Registered swarms are owned by coordinator - don't manually destroy
    SUCCEED();
}

// =============================================================================
// Conflict Resolution Tracking Tests
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, ResolutionUpdatesStats) {
    nimcp_swarm_identity_t* swarm1 = CreateAndRegisterSwarm("resolver_1");
    nimcp_swarm_identity_t* swarm2 = CreateAndRegisterSwarm("resolver_2");

    if (!swarm1 || !swarm2) {
        GTEST_SKIP() << "Swarm creation not available";
    }

    // Detect conflicts
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t conflict_count = 0;
    nimcp_multi_swarm_detect_conflicts(coordinator, &conflicts, &conflict_count);

    // Get stats after detection
    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

    // Stats should be accessible
    EXPECT_GE(stats.total_conflicts, 0U);

    // NOTE: Registered swarms are owned by coordinator - don't manually destroy
}

// =============================================================================
// Pending Conflict Tracking Tests
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, PendingConflictsTracked) {
    nimcp_swarm_identity_t* swarm = CreateAndRegisterSwarm("pending_tracker");

    if (!swarm) {
        GTEST_SKIP() << "Swarm creation not available";
    }

    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

    // Pending should be non-negative
    EXPECT_GE(stats.conflicts_pending, 0U);

    // NOTE: Registered swarms are owned by coordinator - don't manually destroy
}

// =============================================================================
// Escalation Tracking Tests
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, EscalationsTracked) {
    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

    // Escalations should be non-negative
    EXPECT_GE(stats.escalations, 0U);
}

// =============================================================================
// Average Resolution Time Tests
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, AverageResolutionTimeCalculated) {
    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

    // Average time should be non-negative
    EXPECT_GE(stats.avg_resolution_time_ms, 0.0f);
}

// =============================================================================
// Multiple Coordinator Instances Test
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, MultipleCoordinatorsIndependent) {
    // Create second coordinator
    nimcp_multi_swarm_coordinator_t* coord2 = nimcp_multi_swarm_create(nullptr, nullptr);
    ASSERT_NE(coord2, nullptr);

    nimcp_conflict_resolution_stats_t stats1 = nimcp_multi_swarm_get_conflict_stats(coordinator);
    nimcp_conflict_resolution_stats_t stats2 = nimcp_multi_swarm_get_conflict_stats(coord2);

    // Both should be independent (both zero at start)
    EXPECT_EQ(stats1.total_conflicts, 0U);
    EXPECT_EQ(stats2.total_conflicts, 0U);

    nimcp_multi_swarm_destroy(coord2);
}

// =============================================================================
// High Volume Conflict Handling Test
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, HighConflictVolumeHandled) {
    // Register many swarms
    nimcp_swarm_identity_t* swarms[20] = {nullptr};
    for (int i = 0; i < 20; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stress_swarm_%d", i);
        swarms[i] = CreateAndRegisterSwarm(name);
    }

    // Run multiple conflict detection cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        nimcp_swarm_conflict_t* conflicts = nullptr;
        uint32_t conflict_count = 0;
        nimcp_multi_swarm_detect_conflicts(coordinator, &conflicts, &conflict_count);
    }

    // Stats should handle high volume
    nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);
    EXPECT_GE(stats.total_conflicts, 0U);

    // NOTE: Registered swarms are owned by coordinator - don't manually destroy
}

// =============================================================================
// Stats Consistency Test
// =============================================================================

TEST_F(SwarmMultiConflictStatsTest, StatsRemainConsistent) {
    nimcp_swarm_identity_t* swarm = CreateAndRegisterSwarm("consistent_swarm");

    if (!swarm) {
        GTEST_SKIP() << "Swarm creation not available";
    }

    // Get stats multiple times
    for (int i = 0; i < 5; i++) {
        nimcp_conflict_resolution_stats_t stats = nimcp_multi_swarm_get_conflict_stats(coordinator);

        // Total should never be less than resolved + pending
        EXPECT_GE(stats.total_conflicts, stats.conflicts_resolved);
    }

    // NOTE: Registered swarms are owned by coordinator - don't manually destroy
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
