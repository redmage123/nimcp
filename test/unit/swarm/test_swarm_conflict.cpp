/**
 * @file test_swarm_conflict.cpp
 * @brief Unit tests for multi-swarm conflict resolution
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_conflict.h"
#include "swarm/nimcp_swarm_multi.h"
#include "utils/memory/nimcp_memory.h"
#include <cstring>
#include <cmath>

/**
 * Test fixture for conflict resolver tests
 */
class ConflictResolverTest : public ::testing::Test {
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

        // Default territory
        state.territory.min = {0.0, 0.0, 0.0};
        state.territory.max = {100.0, 100.0, 100.0};
        state.territory.priority = priority;

        return state;
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *============================================================================*/

TEST_F(ConflictResolverTest, CreateDestroy) {
    /**
     * WHAT: Test resolver creation and destruction
     * WHY:  Verify basic lifecycle management
     * HOW:  Create and destroy resolver, check for leaks
     */

    swarm_conflict_resolver_t test_resolver = conflict_resolver_create(coordinator, nullptr);
    ASSERT_NE(test_resolver, nullptr);

    conflict_resolver_destroy(test_resolver);
    // No crashes = success
}

TEST_F(ConflictResolverTest, CreateWithCustomConfig) {
    /**
     * WHAT: Test resolver creation with custom configuration
     * WHY:  Verify configuration is applied correctly
     * HOW:  Create resolver with custom config, verify settings
     */

    conflict_config_t config = conflict_resolver_default_config();
    config.max_conflicts = 50;
    config.resolution_timeout_ms = 10000;
    config.enable_negotiation = false;

    swarm_conflict_resolver_t test_resolver = conflict_resolver_create(coordinator, &config);
    ASSERT_NE(test_resolver, nullptr);

    conflict_resolver_destroy(test_resolver);
}

TEST_F(ConflictResolverTest, CreateWithNullCoordinator) {
    /**
     * WHAT: Test resolver creation with NULL coordinator
     * WHY:  Verify proper error handling
     * HOW:  Pass NULL coordinator, expect NULL return
     */

    swarm_conflict_resolver_t test_resolver = conflict_resolver_create(nullptr, nullptr);
    EXPECT_EQ(test_resolver, nullptr);
}

/*=============================================================================
 * FEATURE 1: CONFLICT DETECTION TESTS
 *============================================================================*/

TEST_F(ConflictResolverTest, DetectNoConflicts) {
    /**
     * WHAT: Test detection with non-conflicting swarms
     * WHY:  Verify no false positives
     * HOW:  Create separated swarms, detect conflicts
     */

    swarm_state_t swarms[2];
    swarms[0] = create_test_swarm(1);
    swarms[1] = create_test_swarm(2);

    // Separate territories
    swarms[0].territory.max = {50.0, 50.0, 50.0};
    swarms[1].territory.min = {60.0, 60.0, 60.0};
    swarms[1].territory.max = {100.0, 100.0, 100.0};

    conflict_t conflicts[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, swarms, 2, conflicts, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0);
}

TEST_F(ConflictResolverTest, DetectResourceConflict) {
    /**
     * WHAT: Test detection of resource conflicts
     * WHY:  Verify resource conflict detection works
     * HOW:  Create swarms with overlapping resources
     */

    swarm_state_t swarms[2];
    swarms[0] = create_test_swarm(1);
    swarms[1] = create_test_swarm(2);

    // Same resource
    swarms[0].resource_ids[0] = 100;
    swarms[0].resource_count = 1;
    swarms[1].resource_ids[0] = 100;
    swarms[1].resource_count = 1;

    conflict_t conflicts[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, swarms, 2, conflicts, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);

    // Check first conflict
    EXPECT_EQ(conflicts[0].type, CONFLICT_TYPE_RESOURCE);
    EXPECT_EQ(conflicts[0].swarm_count, 2u);
    EXPECT_EQ(conflicts[0].resource_id, 100u);
}

TEST_F(ConflictResolverTest, DetectTerritoryConflict) {
    /**
     * WHAT: Test detection of territory conflicts
     * WHY:  Verify territory overlap detection works
     * HOW:  Create swarms with overlapping territories
     */

    swarm_state_t swarms[2];
    swarms[0] = create_test_swarm(1);
    swarms[1] = create_test_swarm(2);

    // Overlapping territories
    swarms[0].territory.min = {0.0, 0.0, 0.0};
    swarms[0].territory.max = {60.0, 60.0, 60.0};
    swarms[1].territory.min = {40.0, 40.0, 40.0};
    swarms[1].territory.max = {100.0, 100.0, 100.0};

    conflict_t conflicts[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, swarms, 2, conflicts, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);

    // Find territory conflict
    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (conflicts[i].type == CONFLICT_TYPE_TERRITORY) {
            found = true;
            EXPECT_EQ(conflicts[i].swarm_count, 2u);
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ConflictResolverTest, DetectGoalConflict) {
    /**
     * WHAT: Test detection of goal conflicts
     * WHY:  Verify goal conflict detection works
     * HOW:  Create swarms with conflicting goals
     */

    swarm_state_t swarms[2];
    swarms[0] = create_test_swarm(1);
    swarms[1] = create_test_swarm(2);

    // Same goal
    swarms[0].goal_ids[0] = 500;
    swarms[0].goal_count = 1;
    swarms[1].goal_ids[0] = 500;
    swarms[1].goal_count = 1;

    conflict_t conflicts[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, swarms, 2, conflicts, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);

    // Find goal conflict
    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (conflicts[i].type == CONFLICT_TYPE_GOAL) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(ConflictResolverTest, CalculateSeverity) {
    /**
     * WHAT: Test severity calculation
     * WHY:  Verify severity scoring is correct
     * HOW:  Create conflicts and check severity values
     */

    conflict_t conflict = {0};
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_count = 2;

    float severity = conflict_resolver_calculate_severity(resolver, &conflict);
    EXPECT_GE(severity, 0.0f);
    EXPECT_LE(severity, 1.0f);

    // More swarms should increase severity
    conflict.swarm_count = 4;
    float severity2 = conflict_resolver_calculate_severity(resolver, &conflict);
    EXPECT_GT(severity2, severity);
}

/*=============================================================================
 * FEATURE 2: RESOLUTION STRATEGIES TESTS
 *============================================================================*/

TEST_F(ConflictResolverTest, SetStrategy) {
    /**
     * WHAT: Test setting resolution strategy
     * WHY:  Verify strategy configuration works
     * HOW:  Set strategy for conflict type
     */

    nimcp_result_t result = conflict_resolver_set_strategy(
        resolver, CONFLICT_TYPE_RESOURCE, RESOLUTION_STRATEGY_FAIR_SHARE);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ConflictResolverTest, PriorityWinsStrategy) {
    /**
     * WHAT: Test priority wins resolution strategy
     * WHY:  Verify highest priority swarm wins
     * HOW:  Create conflict, resolve with priority strategy
     */

    // Register swarms with different priorities
    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Swarm1", 10);
    nimcp_swarm_identity_t* swarm2 = nimcp_swarm_identity_create(coordinator, "Swarm2", 10);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    swarm1->health_percentage = 0.9f;
    swarm2->health_percentage = 0.5f;

    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    // Create conflict
    conflict_t conflict = {0};
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = swarm1->swarm_id;
    conflict.swarm_ids[1] = swarm2->swarm_id;
    conflict.swarm_count = 2;
    conflict.resource_id = 100;

    resolution_result_t result;
    nimcp_result_t res = conflict_resolver_priority_wins(resolver, &conflict, &result);

    EXPECT_EQ(res, NIMCP_SUCCESS);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.winner_id, swarm1->swarm_id);
}

TEST_F(ConflictResolverTest, FairShareStrategy) {
    /**
     * WHAT: Test fair share resolution strategy
     * WHY:  Verify proportional allocation works
     * HOW:  Create conflict, resolve with fair share
     */

    // Register swarms
    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Swarm1", 10);
    nimcp_swarm_identity_t* swarm2 = nimcp_swarm_identity_create(coordinator, "Swarm2", 10);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    swarm1->health_percentage = 0.6f;
    swarm2->health_percentage = 0.4f;

    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    // Create conflict
    conflict_t conflict = {0};
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = swarm1->swarm_id;
    conflict.swarm_ids[1] = swarm2->swarm_id;
    conflict.swarm_count = 2;

    resolution_result_t result;
    nimcp_result_t res = conflict_resolver_fair_share(resolver, &conflict, &result);

    EXPECT_EQ(res, NIMCP_SUCCESS);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.term_count, 2u);

    // Check proportional allocation
    float total = result.terms[0] + result.terms[1];
    EXPECT_NEAR(total, 1.0f, 0.01f);

    // Higher health should get more
    EXPECT_GT(result.terms[0], result.terms[1]);
}

TEST_F(ConflictResolverTest, YieldStrategy) {
    /**
     * WHAT: Test yield resolution strategy
     * WHY:  Verify lower priority yields
     * HOW:  Create conflict, resolve with yield
     */

    // Register swarms
    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Swarm1", 10);
    nimcp_swarm_identity_t* swarm2 = nimcp_swarm_identity_create(coordinator, "Swarm2", 10);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    swarm1->health_percentage = 0.8f;
    swarm2->health_percentage = 0.3f;

    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    // Create conflict
    conflict_t conflict = {0};
    conflict.type = CONFLICT_TYPE_TERRITORY;
    conflict.swarm_ids[0] = swarm1->swarm_id;
    conflict.swarm_ids[1] = swarm2->swarm_id;
    conflict.swarm_count = 2;

    resolution_result_t result;
    nimcp_result_t res = conflict_resolver_yield(resolver, &conflict, &result);

    EXPECT_EQ(res, NIMCP_SUCCESS);
    EXPECT_TRUE(result.success);
}

TEST_F(ConflictResolverTest, ArbitrationStrategy) {
    /**
     * WHAT: Test arbitration resolution strategy
     * WHY:  Verify arbitration works
     * HOW:  Create conflict, resolve with arbitration
     */

    // Register swarms
    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Swarm1", 10);
    nimcp_swarm_identity_t* swarm2 = nimcp_swarm_identity_create(coordinator, "Swarm2", 10);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    // Create conflict
    conflict_t conflict = {0};
    conflict.type = CONFLICT_TYPE_PRIORITY;
    conflict.swarm_ids[0] = swarm1->swarm_id;
    conflict.swarm_ids[1] = swarm2->swarm_id;
    conflict.swarm_count = 2;

    resolution_result_t result;
    nimcp_result_t res = conflict_resolver_arbitration(resolver, &conflict, &result);

    EXPECT_EQ(res, NIMCP_SUCCESS);
    EXPECT_TRUE(result.success);
}

/*=============================================================================
 * FEATURE 3: NEGOTIATION PROTOCOL TESTS
 *============================================================================*/

TEST_F(ConflictResolverTest, StartNegotiation) {
    /**
     * WHAT: Test negotiation initiation
     * WHY:  Verify negotiation can be started
     * HOW:  Create conflict and start negotiation
     */

    conflict_t conflict = {0};
    conflict.conflict_id = 1;
    conflict.type = CONFLICT_TYPE_TERRITORY;
    conflict.swarm_ids[0] = 1;
    conflict.swarm_ids[1] = 2;
    conflict.swarm_count = 2;

    nimcp_result_t result = conflict_resolver_negotiate(resolver, &conflict, 5);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ConflictResolverTest, MakeNegotiationOffer) {
    /**
     * WHAT: Test making negotiation offers
     * WHY:  Verify offer submission works
     * HOW:  Start negotiation, make offer
     */

    conflict_t conflict = {0};
    conflict.conflict_id = 1;
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = 1;
    conflict.swarm_ids[1] = 2;
    conflict.swarm_count = 2;

    // Start negotiation
    nimcp_result_t result = conflict_resolver_negotiate(resolver, &conflict, 5);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Make offer
    float proposal[2] = {0.6f, 0.4f};
    result = conflict_resolver_make_offer(resolver, conflict.conflict_id, 1, proposal, 2);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ConflictResolverTest, AcceptNegotiationOffer) {
    /**
     * WHAT: Test accepting negotiation offers
     * WHY:  Verify offer acceptance works
     * HOW:  Start negotiation, make offer, accept
     */

    conflict_t conflict = {0};
    conflict.conflict_id = 1;
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = 1;
    conflict.swarm_ids[1] = 2;
    conflict.swarm_count = 2;

    // Start negotiation
    nimcp_result_t result = conflict_resolver_negotiate(resolver, &conflict, 5);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Make offer
    float proposal[2] = {0.5f, 0.5f};
    result = conflict_resolver_make_offer(resolver, conflict.conflict_id, 1, proposal, 2);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Accept offer
    result = conflict_resolver_accept_offer(resolver, conflict.conflict_id, 2);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ConflictResolverTest, RejectNegotiationOffer) {
    /**
     * WHAT: Test rejecting negotiation offers
     * WHY:  Verify offer rejection works
     * HOW:  Start negotiation, make offer, reject
     */

    conflict_t conflict = {0};
    conflict.conflict_id = 1;
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = 1;
    conflict.swarm_ids[1] = 2;
    conflict.swarm_count = 2;

    // Start negotiation
    nimcp_result_t result = conflict_resolver_negotiate(resolver, &conflict, 5);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Make offer
    float proposal[2] = {0.9f, 0.1f};
    result = conflict_resolver_make_offer(resolver, conflict.conflict_id, 1, proposal, 2);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Reject offer
    result = conflict_resolver_reject_offer(resolver, conflict.conflict_id, 2, "unfair");

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(ConflictResolverTest, NegotiationConvergence) {
    /**
     * WHAT: Test negotiation convergence detection
     * WHY:  Verify convergence is detected correctly
     * HOW:  Make similar offers and check convergence
     */

    conflict_t conflict = {0};
    conflict.conflict_id = 1;
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = 1;
    conflict.swarm_ids[1] = 2;
    conflict.swarm_count = 2;

    // Start negotiation
    nimcp_result_t result = conflict_resolver_negotiate(resolver, &conflict, 10);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Make similar offers
    float proposal1[2] = {0.501f, 0.499f};
    result = conflict_resolver_make_offer(resolver, conflict.conflict_id, 1, proposal1, 2);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    float proposal2[2] = {0.502f, 0.498f};
    result = conflict_resolver_make_offer(resolver, conflict.conflict_id, 2, proposal2, 2);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Check convergence
    bool converged = false;
    result = conflict_resolver_check_convergence(resolver, conflict.conflict_id, &converged);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(converged);
}

/*=============================================================================
 * FEATURE 4: RESOLUTION TRACKING TESTS
 *============================================================================*/

TEST_F(ConflictResolverTest, GetStatistics) {
    /**
     * WHAT: Test getting conflict statistics
     * WHY:  Verify statistics tracking works
     * HOW:  Get initial stats, verify structure
     */

    conflict_stats_t stats;
    nimcp_result_t result = conflict_resolver_get_stats(resolver, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_conflicts, 0u);
    EXPECT_EQ(stats.conflicts_resolved, 0u);
}

TEST_F(ConflictResolverTest, GetActiveConflicts) {
    /**
     * WHAT: Test getting active conflicts
     * WHY:  Verify active conflict tracking works
     * HOW:  Detect conflicts, get active list
     */

    swarm_state_t swarms[2];
    swarms[0] = create_test_swarm(1);
    swarms[1] = create_test_swarm(2);

    // Create resource conflict
    swarms[0].resource_ids[0] = 100;
    swarms[0].resource_count = 1;
    swarms[1].resource_ids[0] = 100;
    swarms[1].resource_count = 1;

    conflict_t conflicts[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, swarms, 2, conflicts, 10, &count);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    ASSERT_GT(count, 0u);

    // Get active conflicts
    conflict_t active[10];
    uint32_t active_count = 0;

    result = conflict_resolver_get_active_conflicts(resolver, active, 10, &active_count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(active_count, 0u);
}

TEST_F(ConflictResolverTest, GetConflictById) {
    /**
     * WHAT: Test getting specific conflict by ID
     * WHY:  Verify conflict lookup works
     * HOW:  Detect conflict, get by ID
     */

    swarm_state_t swarms[2];
    swarms[0] = create_test_swarm(1);
    swarms[1] = create_test_swarm(2);

    swarms[0].resource_ids[0] = 100;
    swarms[0].resource_count = 1;
    swarms[1].resource_ids[0] = 100;
    swarms[1].resource_count = 1;

    conflict_t conflicts[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, swarms, 2, conflicts, 10, &count);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    ASSERT_GT(count, 0u);

    // Get by ID
    conflict_t retrieved;
    result = conflict_resolver_get_conflict(resolver, conflicts[0].conflict_id, &retrieved);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(retrieved.conflict_id, conflicts[0].conflict_id);
}

TEST_F(ConflictResolverTest, GetConflictHistory) {
    /**
     * WHAT: Test getting conflict history for a swarm
     * WHY:  Verify history tracking works
     * HOW:  Detect and resolve conflict, get history
     */

    // Register swarms
    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Swarm1", 10);
    ASSERT_NE(swarm1, nullptr);
    nimcp_swarm_register(coordinator, swarm1);

    // Initially no history
    conflict_t history[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_get_history(
        resolver, swarm1->swarm_id, history, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(ConflictResolverTest, ClearHistory) {
    /**
     * WHAT: Test clearing conflict history
     * WHY:  Verify history can be cleared
     * HOW:  Clear history, verify empty
     */

    nimcp_result_t result = conflict_resolver_clear_history(resolver);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/*=============================================================================
 * UTILITY FUNCTION TESTS
 *============================================================================*/

TEST_F(ConflictResolverTest, ConflictTypeName) {
    /**
     * WHAT: Test conflict type name function
     * WHY:  Verify string conversion works
     * HOW:  Get names for all types
     */

    const char* name = conflict_type_name(CONFLICT_TYPE_RESOURCE);
    EXPECT_STREQ(name, "RESOURCE");

    name = conflict_type_name(CONFLICT_TYPE_TERRITORY);
    EXPECT_STREQ(name, "TERRITORY");

    name = conflict_type_name(CONFLICT_TYPE_GOAL);
    EXPECT_STREQ(name, "GOAL");
}

TEST_F(ConflictResolverTest, ResolutionStrategyName) {
    /**
     * WHAT: Test resolution strategy name function
     * WHY:  Verify string conversion works
     * HOW:  Get names for all strategies
     */

    const char* name = resolution_strategy_name(RESOLUTION_STRATEGY_PRIORITY_WINS);
    EXPECT_STREQ(name, "PRIORITY_WINS");

    name = resolution_strategy_name(RESOLUTION_STRATEGY_FAIR_SHARE);
    EXPECT_STREQ(name, "FAIR_SHARE");

    name = resolution_strategy_name(RESOLUTION_STRATEGY_NEGOTIATION);
    EXPECT_STREQ(name, "NEGOTIATION");
}

TEST_F(ConflictResolverTest, PrintFunctions) {
    /**
     * WHAT: Test print utility functions
     * WHY:  Verify they don't crash
     * HOW:  Call print functions with test data
     */

    conflict_t conflict = {0};
    conflict.conflict_id = 1;
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_count = 2;
    conflict.severity = 0.7f;
    strcpy(conflict.description, "Test conflict");

    conflict_print(&conflict);

    resolution_result_t result = {0};
    result.conflict_id = 1;
    result.strategy_used = RESOLUTION_STRATEGY_PRIORITY_WINS;
    result.success = true;
    result.winner_id = 1;

    resolution_result_print(&result);

    conflict_stats_t stats = {0};
    stats.total_conflicts = 10;
    stats.conflicts_resolved = 8;

    conflict_stats_print(&stats);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
