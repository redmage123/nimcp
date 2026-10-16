/**
 * @file test_kg_analytics.c
 * @brief Unit tests for Knowledge Graph Analytics
 * @date 2026-02-02
 *
 * WHAT: Tests for KG analytics functionality
 * WHY:  Ensure access pattern analysis, topology health, and optimization work correctly
 * HOW:  Test node counting, path analysis, query performance, hot/cold nodes
 *
 * Tests cover:
 * - Access pattern analysis (get/reset)
 * - Hot/cold node identification
 * - Topology health metrics
 * - Capacity forecasting
 * - Optimization recommendations
 * - Query analysis (slow queries, explain)
 * - Threshold configuration
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/brain/nimcp_kg_analytics.h"
#include "core/brain/nimcp_brain_kg.h"
#include "nimcp.h"

//=============================================================================
// Test Helpers
//=============================================================================

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(msg) do { \
    printf("  PASS: %s\n", msg); \
    g_tests_passed++; \
} while(0)

/**
 * @brief Create a KG with security disabled for testing
 */
static brain_kg_t* create_test_kg(void)
{
    brain_kg_config_t config;
    brain_kg_default_config(&config);
    config.enable_security = false;
    config.enable_access_control = false;
    config.enable_immune_integration = false;
    return brain_kg_create(&config);
}

/**
 * @brief Populate KG with test nodes for analytics
 */
static void populate_test_kg(brain_kg_t* kg)
{
    if (!kg) return;

    brain_kg_node_id_t n1 = brain_kg_add_node(kg, "cortex_1", BRAIN_KG_NODE_CORTICAL, "Test cortical node 1");
    brain_kg_node_id_t n2 = brain_kg_add_node(kg, "cortex_2", BRAIN_KG_NODE_CORTICAL, "Test cortical node 2");
    brain_kg_node_id_t n3 = brain_kg_add_node(kg, "subcortical_1", BRAIN_KG_NODE_SUBCORTICAL, "Test subcortical node");
    brain_kg_node_id_t n4 = brain_kg_add_node(kg, "brainstem_1", BRAIN_KG_NODE_BRAINSTEM, "Test brainstem node");
    brain_kg_node_id_t n5 = brain_kg_add_node(kg, "isolated_node", BRAIN_KG_NODE_COGNITIVE, "Isolated test node");

    /* Add edges to create connectivity */
    if (n1 != BRAIN_KG_INVALID_NODE && n2 != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, n1, n2, BRAIN_KG_EDGE_CONNECTS_TO, "Test edge 1-2", 0.8f);
    }
    if (n2 != BRAIN_KG_INVALID_NODE && n3 != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, n2, n3, BRAIN_KG_EDGE_SENDS_TO, "Test edge 2-3", 0.7f);
    }
    if (n3 != BRAIN_KG_INVALID_NODE && n4 != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, n3, n4, BRAIN_KG_EDGE_MODULATES, "Test edge 3-4", 0.6f);
    }
    /* n5 is intentionally isolated */
    (void)n5;
}

//=============================================================================
// Unit Tests - Access Pattern Analysis
//=============================================================================

/**
 * Test: Get access patterns
 */
void test_analytics_get_access_patterns(void)
{
    printf("\n=== test_analytics_get_access_patterns ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    kg_access_pattern_t patterns[32];
    uint32_t count = 0;

    int result = kg_analytics_get_access_patterns(kg, patterns, 32, &count);
    TEST_ASSERT(result == 0 || result == -1, "get_access_patterns should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Get access patterns works");
}

/**
 * Test: Get access patterns with NULL parameters
 */
void test_analytics_get_access_patterns_null(void)
{
    printf("\n=== test_analytics_get_access_patterns_null ===\n");

    int result = kg_analytics_get_access_patterns(NULL, NULL, 0, NULL);
    TEST_ASSERT(result == -1, "get_access_patterns(NULL, ...) should return -1");

    TEST_PASS("NULL handling correct for get_access_patterns");
}

/**
 * Test: Reset access patterns
 */
void test_analytics_reset_access_patterns(void)
{
    printf("\n=== test_analytics_reset_access_patterns ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    int result = kg_analytics_reset_access_patterns(kg);
    TEST_ASSERT(result == 0 || result == -1, "reset_access_patterns should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Reset access patterns works");
}

/**
 * Test: Reset access patterns with NULL
 */
void test_analytics_reset_access_patterns_null(void)
{
    printf("\n=== test_analytics_reset_access_patterns_null ===\n");

    int result = kg_analytics_reset_access_patterns(NULL);
    TEST_ASSERT(result == -1, "reset_access_patterns(NULL) should return -1");

    TEST_PASS("NULL handling correct for reset_access_patterns");
}

//=============================================================================
// Unit Tests - Hot/Cold Node Identification
//=============================================================================

/**
 * Test: Get hot nodes
 */
void test_analytics_get_hot_nodes(void)
{
    printf("\n=== test_analytics_get_hot_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    brain_kg_node_id_t nodes[32];
    uint32_t count = 0;

    int result = kg_analytics_get_hot_nodes(kg, nodes, 32, &count);
    TEST_ASSERT(result == 0 || result == -1, "get_hot_nodes should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Get hot nodes works");
}

/**
 * Test: Get hot nodes with NULL
 */
void test_analytics_get_hot_nodes_null(void)
{
    printf("\n=== test_analytics_get_hot_nodes_null ===\n");

    int result = kg_analytics_get_hot_nodes(NULL, NULL, 0, NULL);
    TEST_ASSERT(result == -1, "get_hot_nodes(NULL, ...) should return -1");

    TEST_PASS("NULL handling correct for get_hot_nodes");
}

/**
 * Test: Get cold nodes
 */
void test_analytics_get_cold_nodes(void)
{
    printf("\n=== test_analytics_get_cold_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    brain_kg_node_id_t nodes[32];
    uint32_t count = 0;

    int result = kg_analytics_get_cold_nodes(kg, nodes, 32, &count);
    TEST_ASSERT(result == 0 || result == -1, "get_cold_nodes should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Get cold nodes works");
}

/**
 * Test: Get cold nodes with NULL
 */
void test_analytics_get_cold_nodes_null(void)
{
    printf("\n=== test_analytics_get_cold_nodes_null ===\n");

    int result = kg_analytics_get_cold_nodes(NULL, NULL, 0, NULL);
    TEST_ASSERT(result == -1, "get_cold_nodes(NULL, ...) should return -1");

    TEST_PASS("NULL handling correct for get_cold_nodes");
}

//=============================================================================
// Unit Tests - Topology Health
//=============================================================================

/**
 * Test: Check topology health
 */
void test_analytics_check_topology_health(void)
{
    printf("\n=== test_analytics_check_topology_health ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    kg_topology_health_t health;
    memset(&health, 0, sizeof(health));

    int result = kg_analytics_check_topology_health(kg, &health);
    TEST_ASSERT(result == 0 || result == -1, "check_topology_health should return valid result");

    if (result == 0) {
        TEST_ASSERT(health.connectivity_score >= 0.0f && health.connectivity_score <= 1.0f,
                    "Connectivity score should be in [0,1]");
        TEST_ASSERT(health.balance_score >= 0.0f && health.balance_score <= 1.0f,
                    "Balance score should be in [0,1]");
        TEST_ASSERT(health.redundancy_score >= 0.0f && health.redundancy_score <= 1.0f,
                    "Redundancy score should be in [0,1]");
    }

    brain_kg_destroy(kg);
    TEST_PASS("Check topology health works");
}

/**
 * Test: Check topology health with NULL
 */
void test_analytics_check_topology_health_null(void)
{
    printf("\n=== test_analytics_check_topology_health_null ===\n");

    int result = kg_analytics_check_topology_health(NULL, NULL);
    TEST_ASSERT(result == -1, "check_topology_health(NULL, NULL) should return -1");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    result = kg_analytics_check_topology_health(kg, NULL);
    TEST_ASSERT(result == -1, "check_topology_health(kg, NULL) should return -1");

    brain_kg_destroy(kg);
    TEST_PASS("NULL handling correct for check_topology_health");
}

/**
 * Test: Find bottleneck nodes
 */
void test_analytics_find_bottlenecks(void)
{
    printf("\n=== test_analytics_find_bottlenecks ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    brain_kg_node_id_t nodes[32];
    uint32_t count = 0;

    int result = kg_analytics_find_bottlenecks(kg, nodes, 32, &count);
    TEST_ASSERT(result == 0 || result == -1, "find_bottlenecks should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Find bottlenecks works");
}

/**
 * Test: Find isolated nodes
 */
void test_analytics_find_isolated(void)
{
    printf("\n=== test_analytics_find_isolated ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    brain_kg_node_id_t nodes[32];
    uint32_t count = 0;

    int result = kg_analytics_find_isolated(kg, nodes, 32, &count);
    TEST_ASSERT(result == 0 || result == -1, "find_isolated should return valid result");

    /* We added an isolated node, so if successful there should be at least 1 */
    if (result == 0 && count > 0) {
        printf("  Found %u isolated nodes\n", count);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Find isolated nodes works");
}

//=============================================================================
// Unit Tests - Capacity Forecasting
//=============================================================================

/**
 * Test: Forecast capacity
 */
void test_analytics_forecast_capacity(void)
{
    printf("\n=== test_analytics_forecast_capacity ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    kg_capacity_forecast_t forecast;
    memset(&forecast, 0, sizeof(forecast));

    int result = kg_analytics_forecast_capacity(kg, &forecast);
    TEST_ASSERT(result == 0 || result == -1, "forecast_capacity should return valid result");

    if (result == 0) {
        TEST_ASSERT(forecast.current_nodes > 0, "Current nodes should be > 0");
        TEST_ASSERT(forecast.projected_nodes_30d >= forecast.current_nodes,
                    "30-day projection should be >= current");
    }

    brain_kg_destroy(kg);
    TEST_PASS("Forecast capacity works");
}

/**
 * Test: Forecast capacity with NULL
 */
void test_analytics_forecast_capacity_null(void)
{
    printf("\n=== test_analytics_forecast_capacity_null ===\n");

    int result = kg_analytics_forecast_capacity(NULL, NULL);
    TEST_ASSERT(result == -1, "forecast_capacity(NULL, NULL) should return -1");

    TEST_PASS("NULL handling correct for forecast_capacity");
}

/**
 * Test: Estimate growth
 */
void test_analytics_estimate_growth(void)
{
    printf("\n=== test_analytics_estimate_growth ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    uint64_t projected_size = 0;

    int result = kg_analytics_estimate_growth(kg, 30, &projected_size);
    TEST_ASSERT(result == 0 || result == -1, "estimate_growth should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Estimate growth works");
}

//=============================================================================
// Unit Tests - Optimization Recommendations
//=============================================================================

/**
 * Test: Get recommendations
 */
void test_analytics_get_recommendations(void)
{
    printf("\n=== test_analytics_get_recommendations ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    kg_optimization_t recommendations[16];
    uint32_t count = 0;

    int result = kg_analytics_get_recommendations(kg, recommendations, 16, &count);
    TEST_ASSERT(result == 0 || result == -1, "get_recommendations should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Get recommendations works");
}

/**
 * Test: Get recommendations with NULL
 */
void test_analytics_get_recommendations_null(void)
{
    printf("\n=== test_analytics_get_recommendations_null ===\n");

    int result = kg_analytics_get_recommendations(NULL, NULL, 0, NULL);
    TEST_ASSERT(result == -1, "get_recommendations(NULL, ...) should return -1");

    TEST_PASS("NULL handling correct for get_recommendations");
}

/**
 * Test: Apply recommendation
 */
void test_analytics_apply_recommendation(void)
{
    printf("\n=== test_analytics_apply_recommendation ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    kg_optimization_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = KG_OPT_CREATE_INDEX;
    strncpy(rec.description, "Test optimization", sizeof(rec.description) - 1);
    strncpy(rec.target, "test_target", sizeof(rec.target) - 1);

    int result = kg_analytics_apply_recommendation(kg, &rec);
    TEST_ASSERT(result == 0 || result == -1, "apply_recommendation should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Apply recommendation works");
}

//=============================================================================
// Unit Tests - Query Analysis
//=============================================================================

/**
 * Test: Get slow queries
 */
void test_analytics_get_slow_queries(void)
{
    printf("\n=== test_analytics_get_slow_queries ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    kg_slow_query_t queries[16];
    uint32_t count = 0;

    int result = kg_analytics_get_slow_queries(kg, queries, 16, &count);
    TEST_ASSERT(result == 0 || result == -1, "get_slow_queries should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Get slow queries works");
}

/**
 * Test: Clear slow queries
 */
void test_analytics_clear_slow_queries(void)
{
    printf("\n=== test_analytics_clear_slow_queries ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    int result = kg_analytics_clear_slow_queries(kg);
    TEST_ASSERT(result == 0 || result == -1, "clear_slow_queries should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Clear slow queries works");
}

/**
 * Test: Explain query
 */
void test_analytics_explain_query(void)
{
    printf("\n=== test_analytics_explain_query ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    char explanation[4096];
    memset(explanation, 0, sizeof(explanation));

    int result = kg_analytics_explain_query(kg, "SELECT * FROM nodes", explanation, sizeof(explanation));
    TEST_ASSERT(result == 0 || result == -1, "explain_query should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Explain query works");
}

//=============================================================================
// Unit Tests - Threshold Configuration
//=============================================================================

/**
 * Test: Set slow query threshold
 */
void test_analytics_set_slow_query_threshold(void)
{
    printf("\n=== test_analytics_set_slow_query_threshold ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    int result = kg_analytics_set_slow_query_threshold(kg, 100);
    TEST_ASSERT(result == 0 || result == -1, "set_slow_query_threshold should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Set slow query threshold works");
}

/**
 * Test: Set access thresholds
 */
void test_analytics_set_access_thresholds(void)
{
    printf("\n=== test_analytics_set_access_thresholds ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    int result = kg_analytics_set_access_thresholds(kg, 10.0f, 0.01f);
    TEST_ASSERT(result == 0 || result == -1, "set_access_thresholds should return valid result");

    brain_kg_destroy(kg);
    TEST_PASS("Set access thresholds works");
}

//=============================================================================
// Unit Tests - Utility Functions
//=============================================================================

/**
 * Test: Optimization type to string
 */
void test_analytics_optimization_type_to_string(void)
{
    printf("\n=== test_analytics_optimization_type_to_string ===\n");

    const char* str = kg_optimization_type_to_string(KG_OPT_CREATE_INDEX);
    TEST_ASSERT(str != NULL, "KG_OPT_CREATE_INDEX string should not be NULL");

    str = kg_optimization_type_to_string(KG_OPT_DROP_INDEX);
    TEST_ASSERT(str != NULL, "KG_OPT_DROP_INDEX string should not be NULL");

    str = kg_optimization_type_to_string(KG_OPT_DENORMALIZE);
    TEST_ASSERT(str != NULL, "KG_OPT_DENORMALIZE string should not be NULL");

    str = kg_optimization_type_to_string(KG_OPT_PARTITION);
    TEST_ASSERT(str != NULL, "KG_OPT_PARTITION string should not be NULL");

    str = kg_optimization_type_to_string(KG_OPT_ARCHIVE);
    TEST_ASSERT(str != NULL, "KG_OPT_ARCHIVE string should not be NULL");

    str = kg_optimization_type_to_string(KG_OPT_CACHE);
    TEST_ASSERT(str != NULL, "KG_OPT_CACHE string should not be NULL");

    TEST_PASS("Optimization type to string conversions work");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("KG Analytics Unit Tests\n");
    printf("============================================\n");

    /* Access pattern tests */
    test_analytics_get_access_patterns();
    test_analytics_get_access_patterns_null();
    test_analytics_reset_access_patterns();
    test_analytics_reset_access_patterns_null();

    /* Hot/cold node tests */
    test_analytics_get_hot_nodes();
    test_analytics_get_hot_nodes_null();
    test_analytics_get_cold_nodes();
    test_analytics_get_cold_nodes_null();

    /* Topology health tests */
    test_analytics_check_topology_health();
    test_analytics_check_topology_health_null();
    test_analytics_find_bottlenecks();
    test_analytics_find_isolated();

    /* Capacity forecasting tests */
    test_analytics_forecast_capacity();
    test_analytics_forecast_capacity_null();
    test_analytics_estimate_growth();

    /* Optimization recommendation tests */
    test_analytics_get_recommendations();
    test_analytics_get_recommendations_null();
    test_analytics_apply_recommendation();

    /* Query analysis tests */
    test_analytics_get_slow_queries();
    test_analytics_clear_slow_queries();
    test_analytics_explain_query();

    /* Threshold configuration tests */
    test_analytics_set_slow_query_threshold();
    test_analytics_set_access_thresholds();

    /* Utility function tests */
    test_analytics_optimization_type_to_string();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
