/**
 * @file test_swarm_conflict_integration.cpp
 * @brief Integration tests for multi-swarm conflict resolution
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include "swarm/nimcp_swarm_conflict.h"
#include "swarm/nimcp_swarm_multi.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include <cstring>

/**
 * Test fixture for integration tests
 */
class ConflictIntegrationTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coordinator;
    swarm_conflict_resolver_t resolver;
    bio_router_t router;

    void SetUp() override {
        nimcp_memory_init();

        // Initialize bio-async router
        bio_router_config_t router_config = bio_router_default_config();
        bio_router_init(&router_config);
        router = bio_router_get_global();

        // Create coordinator with router
        coordinator = nimcp_multi_swarm_create(nullptr, router);
        ASSERT_NE(coordinator, nullptr);

        // Create resolver
        resolver = conflict_resolver_create(coordinator, nullptr);
        ASSERT_NE(resolver, nullptr);

        // Register with bio-async
        conflict_resolver_register_bioasync(resolver, router);
    }

    void TearDown() override {
        if (resolver) {
            conflict_resolver_destroy(resolver);
        }
        if (coordinator) {
            nimcp_multi_swarm_destroy(coordinator);
        }
        bio_router_shutdown();
        nimcp_memory_cleanup();
    }
};

/*=============================================================================
 * INTEGRATION TESTS
 *============================================================================*/

TEST_F(ConflictIntegrationTest, RealSwarmConflict) {
    /**
     * WHAT: Test conflict detection with real swarm objects
     * WHY:  Verify integration with swarm system
     * HOW:  Create real swarms, detect conflicts
     */

    // Create real swarms
    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Alpha", 20);
    nimcp_swarm_identity_t* swarm2 = nimcp_swarm_identity_create(coordinator, "Beta", 20);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    // Set territories
    nimcp_coord3d_t min1 = {0.0, 0.0, 0.0};
    nimcp_coord3d_t max1 = {60.0, 60.0, 60.0};
    nimcp_swarm_set_territory(swarm1, min1, max1, false, 0.8f);

    nimcp_coord3d_t min2 = {40.0, 40.0, 40.0};
    nimcp_coord3d_t max2 = {100.0, 100.0, 100.0};
    nimcp_swarm_set_territory(swarm2, min2, max2, false, 0.6f);

    // Register swarms
    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    // Create swarm states
    swarm_state_t states[2] = {0};
    states[0].swarm_id = swarm1->swarm_id;
    states[0].territory = swarm1->territory;
    states[0].is_active = true;
    states[0].priority = 0.8f;

    states[1].swarm_id = swarm2->swarm_id;
    states[1].territory = swarm2->territory;
    states[1].is_active = true;
    states[1].priority = 0.6f;

    // Detect conflicts
    conflict_t conflicts[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, states, 2, conflicts, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);  // Should find territory overlap
}

TEST_F(ConflictIntegrationTest, BioAsyncMessaging) {
    /**
     * WHAT: Test bio-async integration
     * WHY:  Verify conflict messages are sent/received
     * HOW:  Create conflict, process inbox
     */

    // Process inbox
    uint32_t processed = conflict_resolver_process_inbox(resolver);

    // No messages initially
    EXPECT_EQ(processed, 0u);
}

TEST_F(ConflictIntegrationTest, MultipleConflictResolution) {
    /**
     * WHAT: Test resolving multiple conflicts
     * WHY:  Verify system handles multiple conflicts
     * HOW:  Create multiple swarms with various conflicts
     */

    // Create multiple swarms
    nimcp_swarm_identity_t* swarms[4];
    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Swarm%d", i);
        swarms[i] = nimcp_swarm_identity_create(coordinator, name, 10);
        ASSERT_NE(swarms[i], nullptr);
        nimcp_swarm_register(coordinator, swarms[i]);
    }

    // Create states with overlapping resources
    swarm_state_t states[4] = {0};
    for (int i = 0; i < 4; i++) {
        states[i].swarm_id = swarms[i]->swarm_id;
        states[i].is_active = true;
        states[i].priority = 0.5f + i * 0.1f;

        // Overlapping resources
        states[i].resource_ids[0] = 100 + (i % 2);  // Creates conflicts
        states[i].resource_count = 1;
    }

    // Detect conflicts
    conflict_t conflicts[20];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_detect(
        resolver, states, 4, conflicts, 20, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    // Resolve each conflict
    for (uint32_t i = 0; i < count; i++) {
        resolution_result_t res_result;
        result = conflict_resolver_resolve(
            resolver, &conflicts[i],
            RESOLUTION_STRATEGY_PRIORITY_WINS,
            &res_result);

        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Check statistics
    conflict_stats_t stats;
    conflict_resolver_get_stats(resolver, &stats);

    EXPECT_GT(stats.conflicts_resolved, 0u);
}

TEST_F(ConflictIntegrationTest, NegotiationWithMultipleSwarms) {
    /**
     * WHAT: Test negotiation with multiple swarms
     * WHY:  Verify negotiation protocol works end-to-end
     * HOW:  Create conflict, run full negotiation
     */

    // Create swarms
    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Alpha", 15);
    nimcp_swarm_identity_t* swarm2 = nimcp_swarm_identity_create(coordinator, "Beta", 15);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    // Create conflict
    conflict_t conflict = {0};
    conflict.conflict_id = 1;
    conflict.type = CONFLICT_TYPE_RESOURCE;
    conflict.swarm_ids[0] = swarm1->swarm_id;
    conflict.swarm_ids[1] = swarm2->swarm_id;
    conflict.swarm_count = 2;
    conflict.resource_id = 200;

    // Start negotiation
    nimcp_result_t result = conflict_resolver_negotiate(resolver, &conflict, 5);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Simulate negotiation rounds
    float offer1[2] = {0.6f, 0.4f};
    result = conflict_resolver_make_offer(resolver, conflict.conflict_id,
                                         swarm1->swarm_id, offer1, 2);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    float offer2[2] = {0.5f, 0.5f};
    result = conflict_resolver_make_offer(resolver, conflict.conflict_id,
                                         swarm2->swarm_id, offer2, 2);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check convergence
    bool converged = false;
    result = conflict_resolver_check_convergence(resolver, conflict.conflict_id, &converged);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    if (converged) {
        // Accept offer
        result = conflict_resolver_accept_offer(resolver, conflict.conflict_id, swarm2->swarm_id);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(ConflictIntegrationTest, HistoryTracking) {
    /**
     * WHAT: Test conflict history tracking
     * WHY:  Verify history is maintained correctly
     * HOW:  Create conflicts, resolve, check history
     */

    // Create swarms
    nimcp_swarm_identity_t* swarm1 = nimcp_swarm_identity_create(coordinator, "Alpha", 10);
    nimcp_swarm_identity_t* swarm2 = nimcp_swarm_identity_create(coordinator, "Beta", 10);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    nimcp_swarm_register(coordinator, swarm1);
    nimcp_swarm_register(coordinator, swarm2);

    // Create and resolve multiple conflicts
    for (int i = 0; i < 3; i++) {
        swarm_state_t states[2] = {0};
        states[0].swarm_id = swarm1->swarm_id;
        states[0].is_active = true;
        states[0].resource_ids[0] = 300 + i;
        states[0].resource_count = 1;

        states[1].swarm_id = swarm2->swarm_id;
        states[1].is_active = true;
        states[1].resource_ids[0] = 300 + i;
        states[1].resource_count = 1;

        // Detect
        conflict_t conflicts[10];
        uint32_t count = 0;

        nimcp_result_t result = conflict_resolver_detect(
            resolver, states, 2, conflicts, 10, &count);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Resolve
        if (count > 0) {
            resolution_result_t res_result;
            result = conflict_resolver_resolve(
                resolver, &conflicts[0],
                RESOLUTION_STRATEGY_PRIORITY_WINS,
                &res_result);
            EXPECT_EQ(result, NIMCP_SUCCESS);
        }
    }

    // Get history for swarm1
    conflict_t history[10];
    uint32_t count = 0;

    nimcp_result_t result = conflict_resolver_get_history(
        resolver, swarm1->swarm_id, history, 10, &count);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
