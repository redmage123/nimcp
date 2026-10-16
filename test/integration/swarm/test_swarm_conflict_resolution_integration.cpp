/**
 * @file test_swarm_conflict_resolution_integration.cpp
 * @brief Integration tests for NIMCP Multi-Swarm Conflict Resolution
 *
 * TEST COVERAGE:
 * - Multi-swarm coordination with real conflicts
 * - End-to-end negotiation workflows
 * - Complex multi-way conflicts
 * - Mission-driven conflict scenarios
 * - Resource competition scenarios
 * - Bio-async message integration
 * - Statistics aggregation across swarms
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_multi.h"
#include "async/nimcp_bio_router.h"
#include "utils/time/nimcp_time.h"

class SwarmConflictResolutionIntegrationTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coord;
    std::vector<nimcp_swarm_identity_t*> swarms;
    nimcp_super_swarm_t* super;

    void SetUp() override {
        /* Initialize bio-async router if needed */
        coord = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coord, nullptr);

        super = nimcp_super_swarm_create(coord, "integration_test_super");
        ASSERT_NE(super, nullptr);

        /* Create multiple swarms for complex scenarios */
        for (int i = 0; i < 5; i++) {
            char name[32];
            snprintf(name, sizeof(name), "test_swarm_%d", i);

            auto* swarm = nimcp_swarm_identity_create(coord, name, 20 + i * 5);
            ASSERT_NE(swarm, nullptr);

            ASSERT_EQ(nimcp_swarm_register(coord, swarm), NIMCP_SUCCESS);
            ASSERT_EQ(nimcp_super_swarm_add_swarm(super, swarm), NIMCP_SUCCESS);

            swarms.push_back(swarm);
        }
    }

    void TearDown() override {
        swarms.clear();
        if (coord) {
            nimcp_multi_swarm_destroy(coord);
        }
    }

    void CreateComplexTerritoryConflict() {
        /* Create overlapping territories for multiple swarms */
        for (size_t i = 0; i < swarms.size(); i++) {
            float offset = i * 30.0f;
            nimcp_coord3d_t min = {offset, offset, 0};
            nimcp_coord3d_t max = {offset + 80, offset + 80, 50};

            float priority = 1.0f - (i * 0.15f);  /* Decreasing priorities */
            nimcp_swarm_set_territory(swarms[i], min, max, true, priority);
        }
    }

    void CreateMissionConflicts() {
        /* Create overlapping mission areas */
        nimcp_territory_bounds_t area1 = {{0,0,0}, {100,100,25}, 0, false, 0.8f};
        nimcp_territory_bounds_t area2 = {{50,50,0}, {150,150,25}, 0, false, 0.6f};

        uint64_t mission1 = nimcp_mission_create(coord, "Surveillance Mission",
            NIMCP_MISSION_PRIORITY_HIGH, area1, 0);
        uint64_t mission2 = nimcp_mission_create(coord, "Transport Mission",
            NIMCP_MISSION_PRIORITY_CRITICAL, area2, 0);

        ASSERT_GT(mission1, 0);
        ASSERT_GT(mission2, 0);

        /* Assign different swarms to missions */
        uint64_t swarm_ids1[] = {swarms[0]->swarm_id, swarms[1]->swarm_id};
        uint64_t swarm_ids2[] = {swarms[2]->swarm_id, swarms[3]->swarm_id};

        nimcp_mission_assign_swarms(coord, mission1, swarm_ids1, 2);
        nimcp_mission_assign_swarms(coord, mission2, swarm_ids2, 2);
    }
};

/* ============================================================================
 * Complex Conflict Scenarios
 * ============================================================================ */

TEST_F(SwarmConflictResolutionIntegrationTest, MultiSwarmTerritoryConflict) {
    CreateComplexTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;

    EXPECT_EQ(nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count), NIMCP_SUCCESS);
    EXPECT_GT(count, 0);

    /* Should detect multiple pairwise conflicts */
    EXPECT_GE(count, 3);

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionIntegrationTest, MissionBasedConflicts) {
    CreateMissionConflicts();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;

    EXPECT_EQ(nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count), NIMCP_SUCCESS);

    /* Check for goal conflicts */
    bool found_goal_conflict = false;
    for (uint32_t i = 0; i < count; i++) {
        if (conflicts[i].type == NIMCP_CONFLICT_TYPE_GOAL) {
            found_goal_conflict = true;
            break;
        }
    }

    EXPECT_TRUE(found_goal_conflict);

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionIntegrationTest, CascadingResolution) {
    CreateComplexTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    /* Resolve conflicts one by one */
    uint32_t resolved_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        nimcp_result_t res = nimcp_multi_swarm_resolve_conflict(
            coord, conflicts[i].conflict_id,
            NIMCP_CONFLICT_PRIORITY, nullptr);

        if (res == NIMCP_SUCCESS) {
            resolved_count++;
        }
    }

    EXPECT_GT(resolved_count, 0);

    /* Check statistics */
    auto stats = nimcp_multi_swarm_get_conflict_stats(coord);
    EXPECT_EQ(stats.conflicts_resolved, resolved_count);
    EXPECT_GT(stats.avg_resolution_time_ms, 0.0f);

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * End-to-End Negotiation Workflows
 * ============================================================================ */

TEST_F(SwarmConflictResolutionIntegrationTest, CompleteNegotiationWorkflow) {
    /* Create two swarms with overlapping territories */
    nimcp_coord3d_t min = {0, 0, 0};
    nimcp_coord3d_t max = {100, 100, 50};

    nimcp_swarm_set_territory(swarms[0], min, max, true, 0.7f);
    nimcp_swarm_set_territory(swarms[1], min, max, true, 0.5f);

    /* Detect conflict */
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    ASSERT_GT(count, 0);

    uint32_t conflict_id = conflicts[0].conflict_id;

    /* Start negotiation */
    EXPECT_EQ(nimcp_multi_swarm_start_negotiation(coord, conflict_id), NIMCP_SUCCESS);

    /* Round 1: Initial proposal */
    float proposal1[] = {0.7f, 0.3f};
    EXPECT_EQ(nimcp_multi_swarm_propose(coord, conflict_id, proposal1, 2), NIMCP_SUCCESS);

    /* Check status */
    nimcp_negotiation_round_t round = {0};
    EXPECT_EQ(nimcp_multi_swarm_get_negotiation_status(coord, conflict_id, &round), NIMCP_SUCCESS);
    EXPECT_EQ(round.round, 1);

    /* Reject unfair proposal */
    EXPECT_EQ(nimcp_multi_swarm_reject_proposal(coord, conflict_id, "Too imbalanced"), NIMCP_SUCCESS);

    /* Round 2: Better proposal */
    float proposal2[] = {0.6f, 0.4f};
    EXPECT_EQ(nimcp_multi_swarm_propose(coord, conflict_id, proposal2, 2), NIMCP_SUCCESS);

    /* Reject again */
    EXPECT_EQ(nimcp_multi_swarm_reject_proposal(coord, conflict_id, "Still not fair"), NIMCP_SUCCESS);

    /* Round 3: Fair proposal */
    float proposal3[] = {0.55f, 0.45f};
    EXPECT_EQ(nimcp_multi_swarm_propose(coord, conflict_id, proposal3, 2), NIMCP_SUCCESS);

    /* Accept */
    EXPECT_EQ(nimcp_multi_swarm_accept_proposal(coord, conflict_id), NIMCP_SUCCESS);

    /* Verify resolution */
    auto stats = nimcp_multi_swarm_get_conflict_stats(coord);
    EXPECT_GT(stats.conflicts_resolved, 0);

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionIntegrationTest, NegotiationTimeout) {
    /* Set short timeout */
    nimcp_conflict_resolution_config_t config = coord->conflict_config;
    config.negotiation_timeout_ms = 100.0f;  /* 100ms */
    nimcp_multi_swarm_set_conflict_config(coord, &config);

    nimcp_coord3d_t min = {0, 0, 0};
    nimcp_coord3d_t max = {50, 50, 25};
    nimcp_swarm_set_territory(swarms[0], min, max, true, 0.5f);
    nimcp_swarm_set_territory(swarms[1], min, max, true, 0.5f);

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        uint32_t conflict_id = conflicts[0].conflict_id;

        /* Start negotiation */
        EXPECT_EQ(nimcp_multi_swarm_start_negotiation(coord, conflict_id), NIMCP_SUCCESS);

        /* Wait for timeout */
        nimcp_time_sleep_ms(150);

        /* Starting negotiation again should fail due to timeout */
        nimcp_result_t res = nimcp_multi_swarm_start_negotiation(coord, conflict_id);
        EXPECT_EQ(res, NIMCP_ERROR);
    }

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * Resource Competition Scenarios
 * ============================================================================ */

TEST_F(SwarmConflictResolutionIntegrationTest, ResourceSharing) {
    /* Add capabilities to swarms */
    nimcp_swarm_add_capability(swarms[0], NIMCP_SWARM_CAP_SURVEILLANCE, 0.8f, 10, true);
    nimcp_swarm_add_capability(swarms[1], NIMCP_SWARM_CAP_TRANSPORT, 0.7f, 5, true);

    /* Request resource from another swarm */
    uint64_t req_id = nimcp_resource_request(coord,
        swarms[1]->swarm_id, swarms[0]->swarm_id,
        NIMCP_RESOURCE_REQ_CAPABILITY, 2,
        NIMCP_MISSION_PRIORITY_HIGH);

    EXPECT_GT(req_id, 0);

    /* Approve request */
    EXPECT_EQ(nimcp_resource_approve(coord, req_id, 0.5f), NIMCP_SUCCESS);
}

TEST_F(SwarmConflictResolutionIntegrationTest, CommunicationBridges) {
    /* Create bridges between swarms */
    uint64_t bridge1 = nimcp_comm_bridge_create(coord,
        swarms[0]->swarm_id, swarms[1]->swarm_id, nullptr, 0);
    uint64_t bridge2 = nimcp_comm_bridge_create(coord,
        swarms[1]->swarm_id, swarms[2]->swarm_id, nullptr, 0);

    EXPECT_GT(bridge1, 0);
    EXPECT_GT(bridge2, 0);

    /* Update bridge quality */
    EXPECT_EQ(nimcp_comm_bridge_update_quality(coord, bridge1, 0.9f), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_comm_bridge_update_quality(coord, bridge2, 0.7f), NIMCP_SUCCESS);
}

/* ============================================================================
 * Strategy Comparison Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionIntegrationTest, CompareResolutionStrategies) {
    CreateComplexTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count >= 3) {
        /* Test different strategies on different conflicts */
        nimcp_swarm_resolution_result_t result1 = {0};
        nimcp_swarm_resolution_result_t result2 = {0};
        nimcp_swarm_resolution_result_t result3 = {0};

        nimcp_multi_swarm_resolve_conflict(coord, conflicts[0].conflict_id,
            NIMCP_CONFLICT_PRIORITY, &result1);
        nimcp_multi_swarm_resolve_conflict(coord, conflicts[1].conflict_id,
            NIMCP_CONFLICT_TIME_SHARING, &result2);
        nimcp_multi_swarm_resolve_conflict(coord, conflicts[2].conflict_id,
            NIMCP_CONFLICT_RESOLVE_PARTITION, &result3);

        /* All should be resolved */
        EXPECT_TRUE(result1.resolved);
        EXPECT_TRUE(result2.resolved);
        EXPECT_TRUE(result3.resolved);

        /* Compare resolution times */
        EXPECT_GT(result1.resolution_time_ms, 0.0f);
        EXPECT_GT(result2.resolution_time_ms, 0.0f);
        EXPECT_GT(result3.resolution_time_ms, 0.0f);
    }

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * Auto-Resolution Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionIntegrationTest, AutoResolveMultipleConflicts) {
    CreateComplexTerritoryConflict();

    /* Detect conflicts */
    uint32_t detected = nimcp_conflict_detect(coord);
    EXPECT_GT(detected, 0);

    /* Auto-resolve all conflicts */
    uint32_t resolved = nimcp_conflict_auto_resolve(coord, nullptr, nullptr);
    EXPECT_GT(resolved, 0);

    /* Check statistics */
    auto stats = nimcp_multi_swarm_get_conflict_stats(coord);
    EXPECT_EQ(stats.conflicts_resolved, resolved);
}

/* ============================================================================
 * Statistics Aggregation Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionIntegrationTest, AggregateStatistics) {
    CreateComplexTerritoryConflict();

    /* Get initial stats */
    auto stats_before = nimcp_multi_swarm_get_conflict_stats(coord);

    /* Detect and resolve conflicts */
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        /* Resolve half the conflicts */
        for (uint32_t i = 0; i < count / 2; i++) {
            nimcp_multi_swarm_resolve_conflict(coord, conflicts[i].conflict_id,
                NIMCP_CONFLICT_PRIORITY, nullptr);
        }
    }

    /* Get updated stats */
    auto stats_after = nimcp_multi_swarm_get_conflict_stats(coord);

    EXPECT_GT(stats_after.total_conflicts, stats_before.total_conflicts);
    EXPECT_GT(stats_after.conflicts_resolved, stats_before.conflicts_resolved);
    EXPECT_GT(stats_after.conflicts_pending, stats_before.conflicts_pending);

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * Coordinator Status Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionIntegrationTest, PrintCoordinatorStatus) {
    CreateComplexTerritoryConflict();
    nimcp_conflict_detect(coord);

    /* This should not crash */
    nimcp_multi_swarm_print_status(coord, true);

    SUCCEED();
}

TEST_F(SwarmConflictResolutionIntegrationTest, MultiSwarmStats) {
    uint32_t total_swarms, total_agents, active_missions, active_conflicts;

    nimcp_multi_swarm_get_stats(coord, &total_swarms, &total_agents,
                                &active_missions, &active_conflicts);

    EXPECT_EQ(total_swarms, swarms.size());
    EXPECT_GT(total_agents, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
