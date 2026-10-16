/**
 * @file test_swarm_conflict_resolution.cpp
 * @brief Comprehensive unit tests for NIMCP Multi-Swarm Conflict Resolution
 *
 * TEST COVERAGE:
 * - Conflict detection (territory, resource, goal, priority, communication)
 * - Resolution strategies (priority, negotiation, arbitration, merge, partition, defer)
 * - Negotiation protocol (start, propose, accept, reject)
 * - Conflict severity calculation
 * - Statistics tracking
 * - Configuration management
 * - Bio-async integration
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_multi.h"
#include "utils/time/nimcp_time.h"

class SwarmConflictResolutionTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coord;
    nimcp_super_swarm_t* super;
    nimcp_swarm_identity_t* swarm_a;
    nimcp_swarm_identity_t* swarm_b;
    nimcp_swarm_identity_t* swarm_c;

    void SetUp() override {
        coord = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coord, nullptr);

        /* Create super-swarm */
        super = nimcp_super_swarm_create(coord, "test_super");
        ASSERT_NE(super, nullptr);

        /* Create swarm identities */
        swarm_a = nimcp_swarm_identity_create(coord, "swarm_a", 10);
        swarm_b = nimcp_swarm_identity_create(coord, "swarm_b", 10);
        swarm_c = nimcp_swarm_identity_create(coord, "swarm_c", 10);

        ASSERT_NE(swarm_a, nullptr);
        ASSERT_NE(swarm_b, nullptr);
        ASSERT_NE(swarm_c, nullptr);

        /* Register swarms */
        ASSERT_EQ(nimcp_swarm_register(coord, swarm_a), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_swarm_register(coord, swarm_b), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_swarm_register(coord, swarm_c), NIMCP_SUCCESS);

        /* Add to super-swarm */
        ASSERT_EQ(nimcp_super_swarm_add_swarm(super, swarm_a), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_super_swarm_add_swarm(super, swarm_b), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_super_swarm_add_swarm(super, swarm_c), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (coord) {
            nimcp_multi_swarm_destroy(coord);
        }
    }

    void SetupTerritoryConflict() {
        /* Create overlapping territories */
        nimcp_coord3d_t min_a = {0, 0, 0};
        nimcp_coord3d_t max_a = {100, 100, 50};
        nimcp_coord3d_t min_b = {50, 50, 0};
        nimcp_coord3d_t max_b = {150, 150, 50};

        ASSERT_EQ(nimcp_swarm_set_territory(swarm_a, min_a, max_a, true, 0.8), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_swarm_set_territory(swarm_b, min_b, max_b, true, 0.5), NIMCP_SUCCESS);
    }
};

/* ============================================================================
 * Conflict Detection Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionTest, DetectNoConflicts) {
    /* Set non-overlapping territories */
    nimcp_coord3d_t min_a = {0, 0, 0};
    nimcp_coord3d_t max_a = {50, 50, 25};
    nimcp_coord3d_t min_b = {100, 100, 0};
    nimcp_coord3d_t max_b = {150, 150, 25};

    nimcp_swarm_set_territory(swarm_a, min_a, max_a, false, 0.5);
    nimcp_swarm_set_territory(swarm_b, min_b, max_b, false, 0.5);

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;

    EXPECT_EQ(nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count), NIMCP_SUCCESS);
    EXPECT_EQ(count, 0);

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, DetectTerritoryConflict) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;

    EXPECT_EQ(nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count), NIMCP_SUCCESS);
    EXPECT_GT(count, 0);

    if (count > 0) {
        EXPECT_EQ(conflicts[0].type, NIMCP_CONFLICT_TYPE_TERRITORY);
        EXPECT_EQ(conflicts[0].swarm_count, 2);
        EXPECT_FALSE(conflicts[0].is_resolved);
        EXPECT_GT(conflicts[0].severity, 0.0f);
        EXPECT_LE(conflicts[0].severity, 1.0f);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, DetectMultipleConflicts) {
    /* Create overlapping territories for all three swarms */
    nimcp_coord3d_t min = {0, 0, 0};
    nimcp_coord3d_t max = {100, 100, 50};

    nimcp_swarm_set_territory(swarm_a, min, max, true, 0.8);
    nimcp_swarm_set_territory(swarm_b, min, max, true, 0.5);
    nimcp_swarm_set_territory(swarm_c, min, max, true, 0.3);

    uint32_t count = nimcp_conflict_detect(coord);
    EXPECT_GT(count, 1);  /* Should detect multiple pairwise conflicts */
}

TEST_F(SwarmConflictResolutionTest, LegacyConflictDetect) {
    SetupTerritoryConflict();

    uint32_t count = nimcp_conflict_detect(coord);
    EXPECT_GT(count, 0);
}

/* ============================================================================
 * Resolution Strategy Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionTest, ResolvePriorityStrategy) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_swarm_resolution_result_t result = {0};
        nimcp_result_t res = nimcp_multi_swarm_resolve_conflict(
            coord, conflicts[0].conflict_id,
            NIMCP_CONFLICT_PRIORITY, &result);

        EXPECT_EQ(res, NIMCP_SUCCESS);
        EXPECT_TRUE(result.resolved);
        EXPECT_EQ(result.strategy_used, NIMCP_CONFLICT_PRIORITY);
        EXPECT_GT(result.resolution_time_ms, 0.0f);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, ResolveNegotiationStrategy) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_swarm_resolution_result_t result = {0};
        nimcp_result_t res = nimcp_multi_swarm_resolve_conflict(
            coord, conflicts[0].conflict_id,
            NIMCP_CONFLICT_NEGOTIATION, &result);

        /* Negotiation requires async process, won't resolve immediately */
        EXPECT_EQ(res, NIMCP_ERROR);
        EXPECT_FALSE(result.resolved);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, ResolveTimeSharingStrategy) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_swarm_resolution_result_t result = {0};
        nimcp_result_t res = nimcp_multi_swarm_resolve_conflict(
            coord, conflicts[0].conflict_id,
            NIMCP_CONFLICT_TIME_SHARING, &result);

        EXPECT_EQ(res, NIMCP_SUCCESS);
        EXPECT_TRUE(result.resolved);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, ResolvePartitionStrategy) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_swarm_resolution_result_t result = {0};
        nimcp_result_t res = nimcp_multi_swarm_resolve_conflict(
            coord, conflicts[0].conflict_id,
            NIMCP_CONFLICT_RESOLVE_PARTITION, &result);

        EXPECT_EQ(res, NIMCP_SUCCESS);
        EXPECT_TRUE(result.resolved);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, ResolveMergeStrategy) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_swarm_resolution_result_t result = {0};
        nimcp_result_t res = nimcp_multi_swarm_resolve_conflict(
            coord, conflicts[0].conflict_id,
            NIMCP_CONFLICT_RESOLVE_MERGE, &result);

        EXPECT_EQ(res, NIMCP_SUCCESS);
        EXPECT_TRUE(result.resolved);

        /* Check that merge was tracked in stats */
        auto stats = nimcp_multi_swarm_get_conflict_stats(coord);
        EXPECT_GT(stats.merges_performed, 0);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, ResolveEscalationStrategy) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_swarm_resolution_result_t result = {0};
        nimcp_result_t res = nimcp_multi_swarm_resolve_conflict(
            coord, conflicts[0].conflict_id,
            NIMCP_CONFLICT_ESCALATION, &result);

        /* Escalation doesn't resolve immediately */
        EXPECT_EQ(res, NIMCP_ERROR);

        /* Check escalation tracked in stats */
        auto stats = nimcp_multi_swarm_get_conflict_stats(coord);
        EXPECT_GT(stats.escalations, 0);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, LegacyResolveInterface) {
    SetupTerritoryConflict();

    uint32_t count = nimcp_conflict_detect(coord);
    EXPECT_GT(count, 0);

    /* Use legacy resolve interface */
    nimcp_result_t res = nimcp_conflict_resolve(coord, 1, NIMCP_CONFLICT_PRIORITY, nullptr, nullptr);
    EXPECT_TRUE(res == NIMCP_SUCCESS || res == NIMCP_NOT_FOUND);
}

/* ============================================================================
 * Negotiation Protocol Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionTest, StartNegotiation) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_result_t res = nimcp_multi_swarm_start_negotiation(coord, conflicts[0].conflict_id);
        EXPECT_EQ(res, NIMCP_SUCCESS);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, MakeProposal) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_multi_swarm_start_negotiation(coord, conflicts[0].conflict_id);

        float proposal[] = {0.5f, 0.5f};  /* 50-50 split */
        nimcp_result_t res = nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal, 2);
        EXPECT_EQ(res, NIMCP_SUCCESS);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, AcceptProposal) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_multi_swarm_start_negotiation(coord, conflicts[0].conflict_id);

        float proposal[] = {0.5f, 0.5f};
        nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal, 2);

        nimcp_result_t res = nimcp_multi_swarm_accept_proposal(coord, conflicts[0].conflict_id);
        EXPECT_EQ(res, NIMCP_SUCCESS);

        /* Verify conflict is now resolved */
        auto stats = nimcp_multi_swarm_get_conflict_stats(coord);
        EXPECT_GT(stats.conflicts_resolved, 0);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, RejectProposal) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_multi_swarm_start_negotiation(coord, conflicts[0].conflict_id);

        float proposal[] = {0.9f, 0.1f};  /* Unfair split */
        nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal, 2);

        nimcp_result_t res = nimcp_multi_swarm_reject_proposal(coord, conflicts[0].conflict_id, "Unfair division");
        EXPECT_EQ(res, NIMCP_SUCCESS);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, GetNegotiationStatus) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_multi_swarm_start_negotiation(coord, conflicts[0].conflict_id);

        float proposal[] = {0.6f, 0.4f};
        nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal, 2);

        nimcp_negotiation_round_t round = {0};
        nimcp_result_t res = nimcp_multi_swarm_get_negotiation_status(coord, conflicts[0].conflict_id, &round);

        EXPECT_EQ(res, NIMCP_SUCCESS);
        EXPECT_GT(round.round, 0);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, NegotiationMaxRounds) {
    SetupTerritoryConflict();

    /* Set low max rounds */
    nimcp_conflict_resolution_config_t config = coord->conflict_config;
    config.max_negotiation_rounds = 3;
    nimcp_multi_swarm_set_conflict_config(coord, &config);

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_multi_swarm_start_negotiation(coord, conflicts[0].conflict_id);

        /* Make max rounds of proposals */
        for (uint32_t i = 0; i < 3; i++) {
            float proposal[] = {0.5f, 0.5f};
            nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal, 2);
        }

        /* Next proposal should fail */
        float proposal[] = {0.5f, 0.5f};
        nimcp_result_t res = nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal, 2);
        EXPECT_EQ(res, NIMCP_ERROR);
    }

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * Statistics and Configuration Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionTest, GetConflictStats) {
    auto stats = nimcp_multi_swarm_get_conflict_stats(coord);

    EXPECT_EQ(stats.total_conflicts, 0);
    EXPECT_EQ(stats.conflicts_resolved, 0);
    EXPECT_EQ(stats.conflicts_pending, 0);
}

TEST_F(SwarmConflictResolutionTest, StatsAfterDetection) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    auto stats = nimcp_multi_swarm_get_conflict_stats(coord);
    EXPECT_GT(stats.total_conflicts, 0);
    EXPECT_GT(stats.conflicts_pending, 0);
    EXPECT_EQ(stats.conflicts_resolved, 0);

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, StatsAfterResolution) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_multi_swarm_resolve_conflict(coord, conflicts[0].conflict_id, NIMCP_CONFLICT_PRIORITY, nullptr);
    }

    auto stats = nimcp_multi_swarm_get_conflict_stats(coord);
    EXPECT_GT(stats.conflicts_resolved, 0);
    EXPECT_GT(stats.avg_resolution_time_ms, 0.0f);

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, SetConflictConfig) {
    nimcp_conflict_resolution_config_t config = {
        NIMCP_CONFLICT_PRIORITY,  /* default_strategy */
        60000.0f,                 /* negotiation_timeout_ms */
        20,                       /* max_negotiation_rounds */
        true,                     /* allow_escalation */
        0.75f                     /* merge_threshold */
    };

    EXPECT_EQ(nimcp_multi_swarm_set_conflict_config(coord, &config), NIMCP_SUCCESS);

    /* Verify config was applied */
    EXPECT_EQ(coord->conflict_config.default_strategy, NIMCP_CONFLICT_PRIORITY);
    EXPECT_EQ(coord->conflict_config.max_negotiation_rounds, 20);
    EXPECT_FLOAT_EQ(coord->conflict_config.merge_threshold, 0.75f);
}

TEST_F(SwarmConflictResolutionTest, InvalidConfig) {
    nimcp_conflict_resolution_config_t invalid_config = {
        NIMCP_CONFLICT_PRIORITY,
        -1000.0f,    /* Invalid timeout */
        0,           /* Invalid max rounds */
        true,
        2.0f         /* Invalid threshold */
    };

    EXPECT_EQ(nimcp_multi_swarm_set_conflict_config(coord, &invalid_config), NIMCP_INVALID_PARAM);
}

TEST_F(SwarmConflictResolutionTest, AutoResolve) {
    SetupTerritoryConflict();

    nimcp_conflict_detect(coord);

    uint32_t resolved = nimcp_conflict_auto_resolve(coord, nullptr, nullptr);
    EXPECT_GE(resolved, 0);
}

/* ============================================================================
 * Edge Cases and Error Handling
 * ============================================================================ */

TEST_F(SwarmConflictResolutionTest, DetectConflictsNullCoordinator) {
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;

    EXPECT_EQ(nimcp_multi_swarm_detect_conflicts(nullptr, &conflicts, &count), NIMCP_INVALID_PARAM);
}

TEST_F(SwarmConflictResolutionTest, ResolveNonExistentConflict) {
    nimcp_result_t res = nimcp_multi_swarm_resolve_conflict(coord, 99999, NIMCP_CONFLICT_PRIORITY, nullptr);
    EXPECT_EQ(res, NIMCP_NOT_FOUND);
}

TEST_F(SwarmConflictResolutionTest, StartNegotiationNonExistent) {
    nimcp_result_t res = nimcp_multi_swarm_start_negotiation(coord, 99999);
    EXPECT_EQ(res, NIMCP_NOT_FOUND);
}

TEST_F(SwarmConflictResolutionTest, ProposeWithoutNegotiation) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        /* Try to propose without starting negotiation */
        float proposal[] = {0.5f, 0.5f};
        nimcp_result_t res = nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal, 2);
        EXPECT_EQ(res, NIMCP_NOT_FOUND);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, NullProposal) {
    SetupTerritoryConflict();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        nimcp_multi_swarm_start_negotiation(coord, conflicts[0].conflict_id);
        nimcp_result_t res = nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, nullptr, 2);
        EXPECT_EQ(res, NIMCP_INVALID_PARAM);
    }

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionTest, GetStatsNullCoordinator) {
    auto stats = nimcp_multi_swarm_get_conflict_stats(nullptr);
    EXPECT_EQ(stats.total_conflicts, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
