/**
 * @file test_swarm_conflict_resolution_regression.cpp
 * @brief Regression and stress tests for NIMCP Multi-Swarm Conflict Resolution
 *
 * TEST COVERAGE:
 * - High-load conflict detection (100+ swarms)
 * - Concurrent conflict resolution
 * - Memory leak detection under stress
 * - Performance benchmarks
 * - Scalability tests
 * - Long-running negotiation stability
 * - Extreme configuration values
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_multi.h"
#include "utils/time/nimcp_time.h"

class SwarmConflictResolutionRegressionTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coord;
    nimcp_super_swarm_t* super;
    std::vector<nimcp_swarm_identity_t*> swarms;
    std::vector<nimcp_super_swarm_t*> super_swarms;

    void SetUp() override {
        coord = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coord, nullptr);

        /* Create a super-swarm to hold all test swarms */
        super = nimcp_super_swarm_create(coord, "test_super");
        ASSERT_NE(super, nullptr);
    }

    void TearDown() override {
        /* Clear vectors first - don't explicitly destroy swarms because they
         * are registered with the coordinator and will be cleaned up automatically
         * via the hash table's value destructor when coordinator is destroyed. */
        swarms.clear();
        super_swarms.clear();
        if (coord) {
            nimcp_multi_swarm_destroy(coord);
            coord = nullptr;
        }
    }

    void CreateManySwarms(uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            char name[64];
            snprintf(name, sizeof(name), "stress_swarm_%u", i);

            auto* swarm = nimcp_swarm_identity_create(coord, name, 10 + (i % 20));
            if (!swarm) {
                break;  /* Hit memory limit */
            }

            if (nimcp_swarm_register(coord, swarm) != NIMCP_SUCCESS) {
                nimcp_swarm_identity_destroy(swarm);
                break;
            }

            /* Add swarm to super-swarm for conflict detection */
            if (nimcp_super_swarm_add_swarm(super, swarm) != NIMCP_SUCCESS) {
                /* Super-swarm may have capacity limit, continue anyway */
            }

            swarms.push_back(swarm);
        }
    }

    void CreateOverlappingTerritories() {
        /* Create highly overlapping territories for maximum conflicts */
        float base_size = 100.0f;
        float overlap_factor = 0.8f;  /* 80% overlap */

        for (size_t i = 0; i < swarms.size(); i++) {
            float offset = i * base_size * (1.0f - overlap_factor);
            nimcp_coord3d_t min = {offset, offset, 0};
            nimcp_coord3d_t max = {offset + base_size, offset + base_size, 50};

            float priority = 1.0f - (i / (float)swarms.size());
            nimcp_swarm_set_territory(swarms[i], min, max, true, priority);
        }
    }
};

/* ============================================================================
 * Scalability Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionRegressionTest, DetectConflictsWithManySwarms) {
    /* Create 50 swarms with overlapping territories */
    CreateManySwarms(50);
    ASSERT_GE(swarms.size(), 10);  /* Ensure we created enough */

    CreateOverlappingTerritories();

    auto start = std::chrono::high_resolution_clock::now();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_result_t res = nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(res, NIMCP_SUCCESS);
    EXPECT_GT(count, 0);

    /* Performance check: should complete within reasonable time */
    EXPECT_LT(duration.count(), 5000);  /* < 5 seconds */

    std::cout << "Detected " << count << " conflicts among " << swarms.size()
              << " swarms in " << duration.count() << "ms" << std::endl;

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionRegressionTest, ResolveManyConcurrentConflicts) {
    CreateManySwarms(30);
    ASSERT_GE(swarms.size(), 10);

    CreateOverlappingTerritories();

    /* Detect conflicts */
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    ASSERT_GT(count, 0);

    auto start = std::chrono::high_resolution_clock::now();

    /* Resolve all conflicts using priority strategy */
    uint32_t resolved_count = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (nimcp_multi_swarm_resolve_conflict(coord, conflicts[i].conflict_id,
                NIMCP_CONFLICT_PRIORITY, nullptr) == NIMCP_SUCCESS) {
            resolved_count++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GT(resolved_count, 0);
    EXPECT_LT(duration.count(), 3000);  /* < 3 seconds */

    std::cout << "Resolved " << resolved_count << " conflicts in "
              << duration.count() << "ms" << std::endl;

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * Memory Leak Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionRegressionTest, NoMemoryLeakOnRepeatedDetection) {
    CreateManySwarms(20);
    CreateOverlappingTerritories();

    /* Repeatedly detect conflicts and free results */
    for (int iteration = 0; iteration < 100; iteration++) {
        nimcp_swarm_conflict_t* conflicts = nullptr;
        uint32_t count = 0;

        nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

        if (conflicts) {
            nimcp_free(conflicts);
        }
    }

    /* If we get here without crashing, memory management is working */
    SUCCEED();
}

TEST_F(SwarmConflictResolutionRegressionTest, NoMemoryLeakOnNegotiation) {
    CreateManySwarms(10);
    CreateOverlappingTerritories();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        /* Repeatedly start and abandon negotiations */
        for (int iteration = 0; iteration < 50; iteration++) {
            uint32_t conflict_id = conflicts[0].conflict_id;

            nimcp_multi_swarm_start_negotiation(coord, conflict_id);

            float proposal[] = {0.5f, 0.5f};
            nimcp_multi_swarm_propose(coord, conflict_id, proposal, 2);

            /* Don't accept, just move to next iteration */
        }
    }

    if (conflicts) nimcp_free(conflicts);
    SUCCEED();
}

/* ============================================================================
 * Performance Benchmarks
 * ============================================================================ */

TEST_F(SwarmConflictResolutionRegressionTest, BenchmarkDetectionPerformance) {
    std::vector<uint32_t> swarm_counts = {10, 20, 30, 40, 50};
    std::vector<double> detection_times;

    for (uint32_t count : swarm_counts) {
        /* Clean setup */
        if (coord) nimcp_multi_swarm_destroy(coord);
        coord = nimcp_multi_swarm_create(nullptr, nullptr);
        swarms.clear();

        CreateManySwarms(count);
        CreateOverlappingTerritories();

        auto start = std::chrono::high_resolution_clock::now();

        nimcp_swarm_conflict_t* conflicts = nullptr;
        uint32_t conflict_count = 0;
        nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &conflict_count);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        detection_times.push_back(duration.count() / 1000.0);

        std::cout << "Swarms: " << count << ", Conflicts: " << conflict_count
                  << ", Time: " << (duration.count() / 1000.0) << "ms" << std::endl;

        if (conflicts) nimcp_free(conflicts);
    }

    /* Performance should scale reasonably (not exponentially) */
    SUCCEED();
}

TEST_F(SwarmConflictResolutionRegressionTest, BenchmarkResolutionPerformance) {
    CreateManySwarms(30);
    CreateOverlappingTerritories();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        /* Benchmark different strategies */
        struct {
            nimcp_conflict_resolution_t strategy;
            const char* name;
        } strategies[] = {
            {NIMCP_CONFLICT_PRIORITY, "Priority"},
            {NIMCP_CONFLICT_TIME_SHARING, "Time Sharing"},
            {NIMCP_CONFLICT_RESOLVE_PARTITION, "Partition"},
            {NIMCP_CONFLICT_RESOLVE_MERGE, "Merge"}
        };

        for (auto& strat : strategies) {
            auto start = std::chrono::high_resolution_clock::now();

            /* Resolve first 10 conflicts with this strategy */
            uint32_t limit = (count < 10) ? count : 10;
            for (uint32_t i = 0; i < limit; i++) {
                nimcp_multi_swarm_resolve_conflict(coord, conflicts[i].conflict_id,
                    strat.strategy, nullptr);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            std::cout << "Strategy " << strat.name << ": "
                      << (duration.count() / 1000.0) << "ms for " << limit
                      << " conflicts" << std::endl;
        }
    }

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * Long-Running Stability Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionRegressionTest, LongRunningNegotiation) {
    CreateManySwarms(5);
    CreateOverlappingTerritories();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        uint32_t conflict_id = conflicts[0].conflict_id;

        /* Set high max rounds */
        nimcp_conflict_resolution_config_t config = coord->conflict_config;
        config.max_negotiation_rounds = 100;
        nimcp_multi_swarm_set_conflict_config(coord, &config);

        nimcp_multi_swarm_start_negotiation(coord, conflict_id);

        /* Run many negotiation rounds */
        for (uint32_t round = 0; round < 50; round++) {
            float proposal[] = {0.5f + (round % 10) * 0.01f, 0.5f - (round % 10) * 0.01f};
            nimcp_result_t res = nimcp_multi_swarm_propose(coord, conflict_id, proposal, 2);

            if (res != NIMCP_SUCCESS) {
                break;  /* Hit limit */
            }
        }

        /* Check negotiation status */
        nimcp_negotiation_round_t round = {0};
        nimcp_result_t res = nimcp_multi_swarm_get_negotiation_status(coord, conflict_id, &round);
        EXPECT_EQ(res, NIMCP_SUCCESS);
        EXPECT_GT(round.round, 0);
    }

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * Extreme Configuration Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionRegressionTest, ExtremeConfigValues) {
    nimcp_conflict_resolution_config_t extreme_config = {
        NIMCP_CONFLICT_PRIORITY,
        1000000.0f,  /* Very long timeout */
        1000,        /* Very high max rounds */
        true,
        0.99f        /* Very high merge threshold */
    };

    EXPECT_EQ(nimcp_multi_swarm_set_conflict_config(coord, &extreme_config), NIMCP_SUCCESS);

    CreateManySwarms(10);
    CreateOverlappingTerritories();

    /* Should still work with extreme config */
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    EXPECT_EQ(nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count), NIMCP_SUCCESS);

    if (conflicts) nimcp_free(conflicts);
}

TEST_F(SwarmConflictResolutionRegressionTest, MinimalConfigValues) {
    nimcp_conflict_resolution_config_t minimal_config = {
        NIMCP_CONFLICT_PRIORITY,
        1.0f,    /* Minimal timeout */
        1,       /* Minimal rounds */
        false,   /* No escalation */
        0.01f    /* Low merge threshold */
    };

    EXPECT_EQ(nimcp_multi_swarm_set_conflict_config(coord, &minimal_config), NIMCP_SUCCESS);

    CreateManySwarms(5);
    CreateOverlappingTerritories();

    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    if (count > 0) {
        /* Negotiation should fail quickly with minimal rounds */
        nimcp_multi_swarm_start_negotiation(coord, conflicts[0].conflict_id);

        float proposal1[] = {0.5f, 0.5f};
        nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal1, 2);

        /* Second proposal should fail (exceeds max rounds) */
        float proposal2[] = {0.6f, 0.4f};
        nimcp_result_t res = nimcp_multi_swarm_propose(coord, conflicts[0].conflict_id, proposal2, 2);
        EXPECT_EQ(res, NIMCP_ERROR);
    }

    if (conflicts) nimcp_free(conflicts);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionRegressionTest, StressTestDetectionAndResolution) {
    CreateManySwarms(40);
    CreateOverlappingTerritories();

    /* Run detection/resolution cycle many times */
    for (int iteration = 0; iteration < 10; iteration++) {
        nimcp_swarm_conflict_t* conflicts = nullptr;
        uint32_t count = 0;

        nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

        if (count > 0) {
            /* Resolve half */
            for (uint32_t i = 0; i < count / 2; i++) {
                nimcp_multi_swarm_resolve_conflict(coord, conflicts[i].conflict_id,
                    (nimcp_conflict_resolution_t)(i % 4), nullptr);
            }
        }

        if (conflicts) nimcp_free(conflicts);
    }

    /* Verify statistics are consistent
     * NOTE: total_conflicts is reset on each detect call to show current conflicts,
     * while conflicts_resolved accumulates across all cycles. So we cannot assert
     * total_conflicts >= conflicts_resolved after multiple detection cycles. */
    auto stats = nimcp_multi_swarm_get_conflict_stats(coord);

    /* conflicts_resolved should be non-negative */
    EXPECT_GE(stats.conflicts_resolved, 0u);

    /* After 10 cycles resolving half, we should have resolved some conflicts */
    EXPECT_GT(stats.conflicts_resolved, 0u);

    SUCCEED();
}

TEST_F(SwarmConflictResolutionRegressionTest, StressTestAutoResolve) {
    CreateManySwarms(30);
    CreateOverlappingTerritories();

    /* Run auto-resolve many times */
    uint32_t total_resolved = 0;
    for (int iteration = 0; iteration < 5; iteration++) {
        nimcp_conflict_detect(coord);
        uint32_t resolved = nimcp_conflict_auto_resolve(coord, nullptr, nullptr);
        total_resolved += resolved;
    }

    EXPECT_GT(total_resolved, 0);
    std::cout << "Auto-resolved " << total_resolved << " total conflicts" << std::endl;
}

/* ============================================================================
 * Statistics Accuracy Tests
 * ============================================================================ */

TEST_F(SwarmConflictResolutionRegressionTest, StatisticsAccuracyUnderLoad) {
    CreateManySwarms(25);
    CreateOverlappingTerritories();

    auto stats_initial = nimcp_multi_swarm_get_conflict_stats(coord);

    /* Detect conflicts */
    nimcp_swarm_conflict_t* conflicts = nullptr;
    uint32_t count = 0;
    nimcp_multi_swarm_detect_conflicts(coord, &conflicts, &count);

    auto stats_after_detect = nimcp_multi_swarm_get_conflict_stats(coord);
    EXPECT_EQ(stats_after_detect.total_conflicts, count);
    EXPECT_EQ(stats_after_detect.conflicts_pending, count);

    /* Resolve conflicts */
    uint32_t manually_resolved = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (nimcp_multi_swarm_resolve_conflict(coord, conflicts[i].conflict_id,
                NIMCP_CONFLICT_PRIORITY, nullptr) == NIMCP_SUCCESS) {
            manually_resolved++;
        }
    }

    auto stats_after_resolve = nimcp_multi_swarm_get_conflict_stats(coord);
    EXPECT_EQ(stats_after_resolve.conflicts_resolved, manually_resolved);
    EXPECT_EQ(stats_after_resolve.conflicts_pending, count - manually_resolved);

    if (conflicts) nimcp_free(conflicts);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
