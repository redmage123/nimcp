/**
 * @file test_swarm_conflict_regression.cpp
 * @brief Regression tests for multi-swarm conflict resolution
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_conflict.h"
#include "swarm/nimcp_swarm_multi.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include <cstring>
#include <cmath>

/**
 * Test fixture for regression tests
 */
class ConflictRegressionTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coordinator;
    swarm_conflict_resolver_t resolver;

    void SetUp() override {
        nimcp_memory_init();
        coordinator = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coordinator, nullptr);

        resolver = conflict_resolver_create(coordinator, nullptr);
        ASSERT_NE(resolver, nullptr);
    }

    void TearDown() override {
        if (resolver) {
            conflict_resolver_destroy(resolver);
        }
        if (coordinator) {
            nimcp_multi_swarm_destroy(coordinator);
        }
        nimcp_memory_cleanup();
    }

    /**
     * Helper: Create test swarm state
     */
    swarm_state_t create_test_swarm(uint64_t id, float priority = 0.5f) {
        swarm_state_t state = {0};
        state.swarm_id = id;
        state.priority = priority;
        state.is_active = true;
        state.mission_priority = NIMCP_MISSION_PRIORITY_MEDIUM;

        state.territory.min = {0.0, 0.0, 0.0};
        state.territory.max = {100.0, 100.0, 100.0};
        state.territory.priority = priority;

        return state;
    }
};

/*=============================================================================
 * PERFORMANCE REGRESSION TESTS
 *============================================================================*/

TEST_F(ConflictRegressionTest, PerformanceManyConflicts) {
    /**
     * WHAT: Test performance with many conflicts
     * WHY:  Ensure system scales with conflict count
     * HOW:  Create many swarms with overlapping resources, measure time
     */

    const uint32_t NUM_SWARMS = 50;
    swarm_state_t* swarms = new swarm_state_t[NUM_SWARMS];

    // Create swarms with overlapping resources
    for (uint32_t i = 0; i < NUM_SWARMS; i++) {
        swarms[i] = create_test_swarm(i + 1, 0.5f);
        swarms[i].resource_ids[0] = 100 + (i % 10);  // 10 different resources
        swarms[i].resource_count = 1;
    }

    uint64_t start = nimcp_time_now_us();

    // Detect conflicts
    conflict_t* conflicts = new conflict_t[1000];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, swarms, NUM_SWARMS, conflicts, 1000, &count);

    uint64_t end = nimcp_time_now_us();
    float elapsed_ms = (end - start) / 1000.0f;

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    printf("Detected %u conflicts from %u swarms in %.2f ms\n",
           count, NUM_SWARMS, elapsed_ms);

    // Performance threshold: Should handle 50 swarms in < 100ms
    EXPECT_LT(elapsed_ms, 100.0f);

    delete[] conflicts;
    delete[] swarms;
}

TEST_F(ConflictRegressionTest, PerformanceResolutionSpeed) {
    /**
     * WHAT: Test resolution speed
     * WHY:  Ensure resolutions are fast
     * HOW:  Create conflicts and measure resolution time
     */

    const uint32_t NUM_CONFLICTS = 100;

    // Register swarms
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Swarm%d", i);
        nimcp_swarm_identity_t* swarm = nimcp_swarm_identity_create(coordinator, name, 10);
        ASSERT_NE(swarm, nullptr);
        swarm->health_percentage = 0.5f + (i * 0.05f);
        nimcp_swarm_register(coordinator, swarm);
    }

    // Create conflicts
    conflict_t* conflicts = new conflict_t[NUM_CONFLICTS];
    for (uint32_t i = 0; i < NUM_CONFLICTS; i++) {
        conflicts[i].conflict_id = i + 1;
        conflicts[i].type = CONFLICT_TYPE_RESOURCE;
        conflicts[i].swarm_ids[0] = (i % 10) + 1;
        conflicts[i].swarm_ids[1] = ((i + 1) % 10) + 1;
        conflicts[i].swarm_count = 2;
        conflicts[i].resource_id = 200 + i;
    }

    uint64_t start = nimcp_time_now_us();

    // Resolve all conflicts
    for (uint32_t i = 0; i < NUM_CONFLICTS; i++) {
        resolution_result_t result;
        conflict_resolver_resolve(resolver, &conflicts[i],
                                 RESOLUTION_STRATEGY_PRIORITY_WINS, &result);
    }

    uint64_t end = nimcp_time_now_us();
    float elapsed_ms = (end - start) / 1000.0f;

    printf("Resolved %u conflicts in %.2f ms (%.2f us/conflict)\n",
           NUM_CONFLICTS, elapsed_ms, (elapsed_ms * 1000.0f) / NUM_CONFLICTS);

    // Performance threshold: < 1ms per conflict
    float ms_per_conflict = elapsed_ms / NUM_CONFLICTS;
    EXPECT_LT(ms_per_conflict, 1.0f);

    delete[] conflicts;
}

TEST_F(ConflictRegressionTest, NegotiationConvergenceTime) {
    /**
     * WHAT: Test negotiation convergence time
     * WHY:  Ensure negotiations converge quickly
     * HOW:  Run negotiation, measure convergence time
     */

    conflict_t conflict = {0};
    conflict.conflict_id = 1;
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = 1;
    conflict.swarm_ids[1] = 2;
    conflict.swarm_count = 2;

    uint64_t start = nimcp_time_now_us();

    // Start negotiation
    nimcp_result_t result = conflict_resolver_negotiate(resolver, &conflict, 10);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Simulate converging offers
    for (int round = 0; round < 10; round++) {
        float proposal[2];
        proposal[0] = 0.5f + (0.05f / (round + 1));
        proposal[1] = 1.0f - proposal[0];

        result = conflict_resolver_make_offer(resolver, conflict.conflict_id, 1, proposal, 2);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Check convergence
        bool converged = false;
        result = conflict_resolver_check_convergence(resolver, conflict.conflict_id, &converged);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        if (converged) {
            uint64_t end = nimcp_time_now_us();
            float elapsed_ms = (end - start) / 1000.0f;

            printf("Negotiation converged in round %d (%.2f ms)\n", round + 1, elapsed_ms);

            // Should converge within 10 rounds
            EXPECT_LE(round + 1, 10);
            break;
        }
    }
}

TEST_F(ConflictRegressionTest, MemoryUsageUnderLoad) {
    /**
     * WHAT: Test memory usage under load
     * WHY:  Ensure no memory leaks or excessive usage
     * HOW:  Create/destroy many conflicts, check memory
     */

    nimcp_memory_stats_t before, after;
    nimcp_memory_get_stats(&before);

    // Create and resolve many conflicts
    for (int iteration = 0; iteration < 10; iteration++) {
        swarm_state_t swarms[20];
        for (int i = 0; i < 20; i++) {
            swarms[i] = create_test_swarm(i + 1);
            swarms[i].resource_ids[0] = 400 + (i % 5);
            swarms[i].resource_count = 1;
        }

        conflict_t conflicts[100];
        uint32_t count = 0;

        conflict_resolver_detect(resolver, swarms, 20, conflicts, 100, &count);

        // Resolve conflicts
        for (uint32_t i = 0; i < count; i++) {
            resolution_result_t result;
            conflict_resolver_resolve(resolver, &conflicts[i],
                                     RESOLUTION_STRATEGY_FAIR_SHARE, &result);
        }
    }

    nimcp_memory_get_stats(&after);

    printf("Memory before: %zu bytes, after: %zu bytes, delta: %zd bytes\n",
           before.current_allocated, after.current_allocated,
           (ssize_t)(after.current_allocated - before.current_allocated));

    // Memory growth should be bounded
    size_t growth = after.current_allocated - before.current_allocated;
    EXPECT_LT(growth, 1024 * 1024);  // Less than 1MB growth
}

/*=============================================================================
 * CORRECTNESS REGRESSION TESTS
 *============================================================================*/

TEST_F(ConflictRegressionTest, ConsistentDetection) {
    /**
     * WHAT: Test detection consistency
     * WHY:  Ensure same input produces same output
     * HOW:  Run detection multiple times, verify consistency
     */

    swarm_state_t swarms[3];
    for (int i = 0; i < 3; i++) {
        swarms[i] = create_test_swarm(i + 1);
        swarms[i].resource_ids[0] = 500;
        swarms[i].resource_count = 1;
    }

    conflict_t conflicts1[10];
    uint32_t count1 = 0;

    conflict_resolver_detect(resolver, swarms, 3, conflicts1, 10, &count1);

    // Run again
    conflict_t conflicts2[10];
    uint32_t count2 = 0;

    conflict_resolver_detect(resolver, swarms, 3, conflicts2, 10, &count2);

    // Should detect same number of conflicts
    EXPECT_EQ(count1, count2);

    // Conflict types should match
    for (uint32_t i = 0; i < count1 && i < count2; i++) {
        EXPECT_EQ(conflicts1[i].type, conflicts2[i].type);
        EXPECT_EQ(conflicts1[i].swarm_count, conflicts2[i].swarm_count);
    }
}

TEST_F(ConflictRegressionTest, FairShareConsistency) {
    /**
     * WHAT: Test fair share allocation consistency
     * WHY:  Ensure allocations sum to 1.0
     * HOW:  Resolve many conflicts, verify allocations
     */

    // Register swarms with different priorities
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Swarm%d", i);
        nimcp_swarm_identity_t* swarm = nimcp_swarm_identity_create(coordinator, name, 10);
        ASSERT_NE(swarm, nullptr);
        swarm->health_percentage = 0.3f + (i * 0.15f);
        nimcp_swarm_register(coordinator, swarm);
    }

    // Test multiple conflicts
    for (int test = 0; test < 20; test++) {
        conflict_t conflict = {0};
        conflict.type = CONFLICT_TYPE_RESOURCE;
        conflict.swarm_count = 2 + (test % 3);  // 2-4 swarms

        for (uint32_t i = 0; i < conflict.swarm_count; i++) {
            conflict.swarm_ids[i] = (test + i) % 5 + 1;
        }

        resolution_result_t result;
        nimcp_result_t res = conflict_resolver_fair_share(resolver, &conflict, &result);

        ASSERT_EQ(res, NIMCP_SUCCESS);
        ASSERT_TRUE(result.success);

        // Verify allocation sums to ~1.0
        float total = 0.0f;
        for (uint32_t i = 0; i < result.term_count; i++) {
            total += result.terms[i];
            EXPECT_GE(result.terms[i], 0.0f);
            EXPECT_LE(result.terms[i], 1.0f);
        }

        EXPECT_NEAR(total, 1.0f, 0.01f) << "Test " << test << " failed";
    }
}

TEST_F(ConflictRegressionTest, StatisticsAccuracy) {
    /**
     * WHAT: Test statistics tracking accuracy
     * WHY:  Ensure stats reflect actual operations
     * HOW:  Perform operations, verify stats
     */

    conflict_stats_t initial_stats;
    conflict_resolver_get_stats(resolver, &initial_stats);

    // Register swarms
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Swarm%d", i);
        nimcp_swarm_identity_t* swarm = nimcp_swarm_identity_create(coordinator, name, 10);
        nimcp_swarm_register(coordinator, swarm);
    }

    // Create states with conflicts
    swarm_state_t swarms[4];
    for (int i = 0; i < 4; i++) {
        swarms[i] = create_test_swarm(i + 1);
        swarms[i].resource_ids[0] = 600 + (i % 2);
        swarms[i].resource_count = 1;
    }

    // Detect
    conflict_t conflicts[20];
    uint32_t count = 0;

    conflict_resolver_detect(resolver, swarms, 4, conflicts, 20, &count);

    // Resolve half
    uint32_t resolved_count = 0;
    for (uint32_t i = 0; i < count / 2; i++) {
        resolution_result_t result;
        nimcp_result_t res = conflict_resolver_resolve(
            resolver, &conflicts[i],
            RESOLUTION_STRATEGY_PRIORITY_WINS, &result);

        if (res == NIMCP_SUCCESS) {
            resolved_count++;
        }
    }

    // Check stats
    conflict_stats_t final_stats;
    conflict_resolver_get_stats(resolver, &final_stats);

    EXPECT_EQ(final_stats.total_conflicts - initial_stats.total_conflicts, count);
    EXPECT_GE(final_stats.conflicts_resolved - initial_stats.conflicts_resolved, resolved_count);
}

/*=============================================================================
 * EDGE CASE REGRESSION TESTS
 *============================================================================*/

TEST_F(ConflictRegressionTest, EmptySwarmList) {
    /**
     * WHAT: Test detection with empty swarm list
     * WHY:  Ensure graceful handling
     * HOW:  Call detect with 0 swarms
     */

    conflict_t conflicts[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, nullptr, 0, conflicts, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(ConflictRegressionTest, MaxConflictsReached) {
    /**
     * WHAT: Test behavior when max conflicts reached
     * WHY:  Ensure proper handling of capacity limits
     * HOW:  Create more conflicts than capacity
     */

    const uint32_t SMALL_CAPACITY = 5;
    swarm_state_t swarms[20];

    // Create many swarms with same resource
    for (int i = 0; i < 20; i++) {
        swarms[i] = create_test_swarm(i + 1);
        swarms[i].resource_ids[0] = 700;
        swarms[i].resource_count = 1;
    }

    conflict_t conflicts[SMALL_CAPACITY];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, swarms, 20, conflicts, SMALL_CAPACITY, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_LE(count, SMALL_CAPACITY);
}

TEST_F(ConflictRegressionTest, ZeroPrioritySwarms) {
    /**
     * WHAT: Test handling of zero-priority swarms
     * WHY:  Ensure no division by zero
     * HOW:  Create swarms with zero priority, resolve
     */

    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Zero1", 5);
    nimcp_swarm_identity_t* swarm2 = nimcp_swarm_identity_create(coordinator, "Zero2", 5);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    swarm1->health_percentage = 0.0f;
    swarm2->health_percentage = 0.0f;

    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    conflict_t conflict = {0};
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = swarm1->swarm_id;
    conflict.swarm_ids[1] = swarm2->swarm_id;
    conflict.swarm_count = 2;

    resolution_result_t result;
    nimcp_result_t res = conflict_resolver_fair_share(resolver, &conflict, &result);

    EXPECT_EQ(res, NIMCP_SUCCESS);
    EXPECT_TRUE(result.success);

    // Should still allocate fairly
    EXPECT_NEAR(result.terms[0] + result.terms[1], 1.0f, 0.01f);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
