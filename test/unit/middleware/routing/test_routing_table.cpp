//=============================================================================
// test_routing_table.cpp - Comprehensive Routing Table Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_routing_table.h"

/**
 * WHAT: Comprehensive test suite for routing table
 * WHY:  Ensure dynamic routing with Hebbian learning and pruning works correctly
 * HOW:  Unit tests for all 13 functions, integration tests, regression tests
 */

class RoutingTableTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool FloatEquals(float a, float b, float eps = 0.001f) {
        return std::fabs(a - b) < eps;
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================
// WHAT: Test table creation and destruction
// WHY:  Verify resource management and configuration
// HOW:  Test various configurations and edge cases

TEST_F(RoutingTableTest, DefaultConfig_Valid) {
    routing_table_config_t config = routing_table_default_config();

    EXPECT_EQ(config.max_routes, ROUTING_TABLE_MAX_ROUTES);
    EXPECT_EQ(config.max_paths_per_source, ROUTING_TABLE_MAX_PATHS);
    EXPECT_FLOAT_EQ(config.min_strength, ROUTING_MIN_STRENGTH);
    EXPECT_FLOAT_EQ(config.strength_decay, ROUTING_STRENGTH_DECAY);
    EXPECT_TRUE(config.enable_learning);
    EXPECT_TRUE(config.enable_pruning);
    EXPECT_TRUE(config.enable_multi_path);
    EXPECT_FLOAT_EQ(config.learning_rate, 0.1f);
}

TEST_F(RoutingTableTest, Create_Success_DefaultConfig) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);

    ASSERT_NE(table, nullptr);
    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, Create_Success_CustomConfig) {
    routing_table_config_t config = routing_table_default_config();
    config.max_routes = 100;
    config.learning_rate = 0.2f;

    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, Create_Failure_NullConfig) {
    routing_table_t* table = routing_table_create(nullptr);
    EXPECT_EQ(table, nullptr);
}

TEST_F(RoutingTableTest, Create_Failure_ZeroRoutes) {
    routing_table_config_t config = routing_table_default_config();
    config.max_routes = 0;

    routing_table_t* table = routing_table_create(&config);
    EXPECT_EQ(table, nullptr);
}

TEST_F(RoutingTableTest, Destroy_NullSafe) {
    routing_table_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// ROUTE OPERATIONS TESTS
//=============================================================================
// WHAT: Test route addition, query, and removal
// WHY:  Verify core routing functionality
// HOW:  Add routes, query them, remove them

TEST_F(RoutingTableTest, AddRoute_Success) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    bool result = routing_table_add_route(table, 1, 100, 0.5f);
    EXPECT_TRUE(result);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, AddRoute_Failure_NullTable) {
    bool result = routing_table_add_route(nullptr, 1, 100, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(RoutingTableTest, AddRoute_Failure_InvalidStrength) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    EXPECT_FALSE(routing_table_add_route(table, 1, 100, -0.1f));
    EXPECT_FALSE(routing_table_add_route(table, 1, 100, 1.1f));

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, QueryRoutes_Success) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.8f);
    routing_table_add_route(table, 1, 101, 0.6f);

    route_query_t result;
    bool success = routing_table_query_routes(table, 1, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result.num_dests, 2);

    // Results sorted by strength (descending)
    EXPECT_EQ(result.dest_ids[0], 100);  // 0.8
    EXPECT_EQ(result.dest_ids[1], 101);  // 0.6

    routing_table_free_query(&result);
    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, QueryRoutes_Failure_NullParams) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);

    route_query_t result;
    EXPECT_FALSE(routing_table_query_routes(nullptr, 1, &result));
    EXPECT_FALSE(routing_table_query_routes(table, 1, nullptr));

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, QueryRoutes_NoRoutes_ReturnsFalse) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    route_query_t result;
    bool success = routing_table_query_routes(table, 999, &result);
    EXPECT_FALSE(success);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, GetStrength_Success) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.7f);

    float strength = 0.0f;
    bool result = routing_table_get_strength(table, 1, 100, &strength);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(strength, 0.7f);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, GetStrength_Failure_RouteNotFound) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    float strength = 99.9f;
    bool result = routing_table_get_strength(table, 1, 999, &strength);
    EXPECT_FALSE(result);
    EXPECT_FLOAT_EQ(strength, 0.0f);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, RemoveRoute_Success) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.5f);

    bool result = routing_table_remove_route(table, 1, 100);
    EXPECT_TRUE(result);

    // Route should be gone
    float strength = 0.0f;
    EXPECT_FALSE(routing_table_get_strength(table, 1, 100, &strength));

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, RemoveRoute_Failure_NotFound) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    bool result = routing_table_remove_route(table, 1, 999);
    EXPECT_FALSE(result);

    routing_table_destroy(table);
}

//=============================================================================
// PRIORITY TESTS
//=============================================================================
// WHAT: Test route priority management
// WHY:  Verify conflict resolution via priority
// HOW:  Set priorities, verify precedence

TEST_F(RoutingTableTest, SetPriority_Success) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.5f);

    bool result = routing_table_set_priority(table, 1, 100, 5);
    EXPECT_TRUE(result);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, SetPriority_Failure_RouteNotFound) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    bool result = routing_table_set_priority(table, 1, 999, 5);
    EXPECT_FALSE(result);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, SetPriority_Failure_NullTable) {
    bool result = routing_table_set_priority(nullptr, 1, 100, 5);
    EXPECT_FALSE(result);
}

//=============================================================================
// HEBBIAN LEARNING TESTS
//=============================================================================
// WHAT: Test route strengthening via usage
// WHY:  Verify "use it or lose it" learning
// HOW:  Use routes, verify strength increases

TEST_F(RoutingTableTest, UseRoute_Success_StrengthensRoute) {
    routing_table_config_t config = routing_table_default_config();
    config.enable_learning = true;
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.5f);

    float initial_strength = 0.0f;
    routing_table_get_strength(table, 1, 100, &initial_strength);

    // Use route multiple times
    for (int i = 0; i < 5; i++) {
        routing_table_use_route(table, 1, 100);
    }

    float final_strength = 0.0f;
    routing_table_get_strength(table, 1, 100, &final_strength);

    EXPECT_GT(final_strength, initial_strength);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, UseRoute_Failure_NullTable) {
    bool result = routing_table_use_route(nullptr, 1, 100);
    EXPECT_FALSE(result);
}

TEST_F(RoutingTableTest, UseRoute_Failure_RouteNotFound) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    bool result = routing_table_use_route(table, 1, 999);
    EXPECT_FALSE(result);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, AddRoute_ExistingRoute_Strengthens) {
    routing_table_config_t config = routing_table_default_config();
    config.enable_learning = true;
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.5f);

    float initial_strength = 0.0f;
    routing_table_get_strength(table, 1, 100, &initial_strength);

    // Add same route again (should strengthen)
    routing_table_add_route(table, 1, 100, 0.5f);

    float final_strength = 0.0f;
    routing_table_get_strength(table, 1, 100, &final_strength);

    EXPECT_GT(final_strength, initial_strength);

    routing_table_destroy(table);
}

//=============================================================================
// PRUNING TESTS
//=============================================================================
// WHAT: Test weak route removal
// WHY:  Verify homeostatic maintenance
// HOW:  Create weak routes, prune them

TEST_F(RoutingTableTest, Prune_Success_RemovesWeakRoutes) {
    routing_table_config_t config = routing_table_default_config();
    config.min_strength = 0.3f;
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    // Add strong and weak routes
    routing_table_add_route(table, 1, 100, 0.8f);
    routing_table_add_route(table, 1, 101, 0.2f);  // Below threshold
    routing_table_add_route(table, 1, 102, 0.5f);

    uint32_t num_pruned = 0;
    bool result = routing_table_prune(table, &num_pruned);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_pruned, 1);  // Route 101 should be pruned

    // Verify weak route is gone
    float strength = 0.0f;
    EXPECT_FALSE(routing_table_get_strength(table, 1, 101, &strength));

    // Strong routes should remain
    EXPECT_TRUE(routing_table_get_strength(table, 1, 100, &strength));
    EXPECT_TRUE(routing_table_get_strength(table, 1, 102, &strength));

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, Prune_Failure_NullTable) {
    uint32_t num_pruned = 0;
    bool result = routing_table_prune(nullptr, &num_pruned);
    EXPECT_FALSE(result);
}

TEST_F(RoutingTableTest, Prune_NoPruning_NoWeakRoutes) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.8f);

    uint32_t num_pruned = 0;
    bool result = routing_table_prune(table, &num_pruned);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_pruned, 0);

    routing_table_destroy(table);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================
// WHAT: Test statistics retrieval
// WHY:  Verify routing metrics tracking
// HOW:  Add routes, query stats

TEST_F(RoutingTableTest, GetStats_Success) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.8f);
    routing_table_add_route(table, 1, 101, 0.6f);

    uint32_t num_routes = 0;
    float avg_strength = 0.0f;
    uint64_t total_usage = 0;

    bool result = routing_table_get_stats(table, &num_routes, &avg_strength, &total_usage);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_routes, 2);
    EXPECT_TRUE(FloatEquals(avg_strength, 0.7f, 0.01f));  // (0.8 + 0.6) / 2

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, GetStats_Failure_NullTable) {
    uint32_t num_routes = 0;
    bool result = routing_table_get_stats(nullptr, &num_routes, nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(RoutingTableTest, GetStats_EmptyTable_ZeroAvg) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    uint32_t num_routes = 0;
    float avg_strength = 99.9f;

    bool result = routing_table_get_stats(table, &num_routes, &avg_strength, nullptr);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_routes, 0);
    EXPECT_FLOAT_EQ(avg_strength, 0.0f);

    routing_table_destroy(table);
}

//=============================================================================
// CLEAR AND FREE TESTS
//=============================================================================
// WHAT: Test table clearing and memory management
// WHY:  Verify resource cleanup
// HOW:  Clear table, verify empty state

TEST_F(RoutingTableTest, Clear_RemovesAllRoutes) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.5f);
    routing_table_add_route(table, 1, 101, 0.7f);

    routing_table_clear(table);

    uint32_t num_routes = 0;
    routing_table_get_stats(table, &num_routes, nullptr, nullptr);
    EXPECT_EQ(num_routes, 0);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, Clear_NullSafe) {
    routing_table_clear(nullptr);
    // Should not crash
}

TEST_F(RoutingTableTest, FreeQuery_Success) {
    route_query_t result;
    result.dest_ids = new uint32_t[2];
    result.strengths = new float[2];
    result.num_dests = 2;

    routing_table_free_query(&result);

    EXPECT_EQ(result.dest_ids, nullptr);
    EXPECT_EQ(result.strengths, nullptr);
    EXPECT_EQ(result.num_dests, 0);
}

TEST_F(RoutingTableTest, FreeQuery_NullSafe) {
    routing_table_free_query(nullptr);
    // Should not crash
}

//=============================================================================
// MULTI-PATH TESTS
//=============================================================================
// WHAT: Test multi-destination routing
// WHY:  Verify fan-out capability
// HOW:  Add multiple destinations per source

TEST_F(RoutingTableTest, MultiPath_Success) {
    routing_table_config_t config = routing_table_default_config();
    config.enable_multi_path = true;
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.8f);
    routing_table_add_route(table, 1, 101, 0.6f);
    routing_table_add_route(table, 1, 102, 0.4f);

    route_query_t result;
    bool success = routing_table_query_routes(table, 1, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result.num_dests, 3);

    routing_table_free_query(&result);
    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, SinglePath_LimitsToOne) {
    routing_table_config_t config = routing_table_default_config();
    config.enable_multi_path = false;
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.6f);
    routing_table_add_route(table, 1, 101, 0.8f);  // Highest
    routing_table_add_route(table, 1, 102, 0.4f);

    route_query_t result;
    bool success = routing_table_query_routes(table, 1, &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(result.num_dests, 1);
    EXPECT_EQ(result.dest_ids[0], 101);  // Highest strength

    routing_table_free_query(&result);
    routing_table_destroy(table);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test edge cases and known issues
// WHY:  Prevent regressions
// HOW:  Test boundary conditions

TEST_F(RoutingTableTest, Regression_StrengthClamp) {
    routing_table_config_t config = routing_table_default_config();
    config.enable_learning = true;
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    routing_table_add_route(table, 1, 100, 0.99f);

    // Use route many times (should clamp to 1.0)
    for (int i = 0; i < 100; i++) {
        routing_table_use_route(table, 1, 100);
    }

    float strength = 0.0f;
    routing_table_get_strength(table, 1, 100, &strength);
    EXPECT_LE(strength, 1.0f);

    routing_table_destroy(table);
}

TEST_F(RoutingTableTest, Regression_HashCollision) {
    routing_table_config_t config = routing_table_default_config();
    routing_table_t* table = routing_table_create(&config);
    ASSERT_NE(table, nullptr);

    // Add many routes with same source (potential hash collision)
    for (uint32_t i = 0; i < 10; i++) {
        routing_table_add_route(table, 1, 100 + i, 0.5f);
    }

    // All routes should be retrievable
    for (uint32_t i = 0; i < 10; i++) {
        float strength = 0.0f;
        bool result = routing_table_get_strength(table, 1, 100 + i, &strength);
        EXPECT_TRUE(result);
    }

    routing_table_destroy(table);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
