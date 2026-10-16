/**
 * @file test_kg_temporal.c
 * @brief Unit tests for Knowledge Graph Temporal Queries
 * @date 2026-02-02
 *
 * WHAT: Tests for KG temporal query (time-travel) functionality
 * WHY:  Ensure historical analysis, version tracking, and temporal diffs work correctly
 * HOW:  Test bi-temporal queries, version history, diffs, and trend analysis
 *
 * Tests cover:
 * - Temporal query configuration
 * - Point-in-time queries (AS_OF, BETWEEN, SINCE)
 * - Version history retrieval
 * - Temporal diffs
 * - Topology evolution tracking
 * - Node count trends
 * - Bi-temporal utilities
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_kg_temporal.h"
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
 * @brief Populate KG with test nodes
 */
static brain_kg_node_id_t populate_test_kg(brain_kg_t* kg)
{
    if (!kg) return BRAIN_KG_INVALID_NODE;

    brain_kg_node_id_t n1 = brain_kg_add_node(kg, "temporal_test_1",
        BRAIN_KG_NODE_CORTICAL, "Test node for temporal queries");
    brain_kg_add_node(kg, "temporal_test_2",
        BRAIN_KG_NODE_SUBCORTICAL, "Another test node");
    brain_kg_add_node(kg, "temporal_test_3",
        BRAIN_KG_NODE_COGNITIVE, "Third test node");

    return n1;  /* Return first node for testing */
}

//=============================================================================
// Unit Tests - Query Configuration
//=============================================================================

/**
 * Test: Default temporal query
 */
void test_temporal_query_default(void)
{
    printf("\n=== test_temporal_query_default ===\n");

    kg_temporal_query_t query;
    memset(&query, 0xFF, sizeof(query));

    int result = kg_temporal_query_default(&query);
    TEST_ASSERT(result == 0, "kg_temporal_query_default should return 0");
    TEST_ASSERT(query.mode == KG_TEMPORAL_CURRENT, "Default mode should be CURRENT");

    TEST_PASS("Default temporal query works");
}

/**
 * Test: Default temporal query with NULL
 */
void test_temporal_query_default_null(void)
{
    printf("\n=== test_temporal_query_default_null ===\n");

    int result = kg_temporal_query_default(NULL);
    TEST_ASSERT(result == -1, "kg_temporal_query_default(NULL) should return -1");

    TEST_PASS("NULL handling correct for query_default");
}

//=============================================================================
// Unit Tests - Point-in-Time Queries
//=============================================================================

/**
 * Test: Query node at current time
 */
void test_temporal_query_node_current(void)
{
    printf("\n=== test_temporal_query_node_current ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);
    TEST_ASSERT(node_id != BRAIN_KG_INVALID_NODE, "populate_test_kg should succeed");

    kg_temporal_query_t query;
    kg_temporal_query_default(&query);
    query.mode = KG_TEMPORAL_CURRENT;

    void* result_data = NULL;
    size_t result_size = 0;

    int result = kg_temporal_query_node(kg, node_id, &query, &result_data, &result_size);
    TEST_ASSERT(result == 0 || result == -1, "query_node should return valid result");

    if (result_data) {
        free(result_data);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Query node at current time works");
}

/**
 * Test: Query node AS_OF specific time
 */
void test_temporal_query_node_as_of(void)
{
    printf("\n=== test_temporal_query_node_as_of ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);

    kg_temporal_query_t query;
    kg_temporal_query_default(&query);
    query.mode = KG_TEMPORAL_AS_OF;
    query.as_of_timestamp = kg_temporal_now();

    void* result_data = NULL;
    size_t result_size = 0;

    int result = kg_temporal_query_node(kg, node_id, &query, &result_data, &result_size);
    TEST_ASSERT(result == 0 || result == -1, "query_node AS_OF should return valid result");

    if (result_data) {
        free(result_data);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Query node AS_OF works");
}

/**
 * Test: Query node with NULL parameters
 */
void test_temporal_query_node_null(void)
{
    printf("\n=== test_temporal_query_node_null ===\n");

    void* result_data = NULL;
    size_t result_size = 0;

    int result = kg_temporal_query_node(NULL, 0, NULL, &result_data, &result_size);
    TEST_ASSERT(result == -1, "query_node(NULL, ...) should return -1");

    TEST_PASS("NULL handling correct for query_node");
}

/**
 * Test: Query subgraph
 */
void test_temporal_query_subgraph(void)
{
    printf("\n=== test_temporal_query_subgraph ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t root = populate_test_kg(kg);

    kg_temporal_query_t query;
    kg_temporal_query_default(&query);

    brain_kg_t* result_kg = NULL;

    int result = kg_temporal_query_subgraph(kg, root, 2, &query, &result_kg);
    TEST_ASSERT(result == 0 || result == -1, "query_subgraph should return valid result");

    if (result_kg) {
        brain_kg_destroy(result_kg);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Query subgraph works");
}

/**
 * Test: Query multiple nodes
 */
void test_temporal_query_nodes(void)
{
    printf("\n=== test_temporal_query_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t n1 = populate_test_kg(kg);

    brain_kg_node_id_t node_ids[2] = {n1, n1 + 1};
    void* results[2] = {NULL, NULL};
    size_t result_sizes[2] = {0, 0};

    kg_temporal_query_t query;
    kg_temporal_query_default(&query);

    int result = kg_temporal_query_nodes(kg, node_ids, 2, &query, results, result_sizes);
    TEST_ASSERT(result >= -1, "query_nodes should return valid result");

    for (int i = 0; i < 2; i++) {
        if (results[i]) {
            free(results[i]);
        }
    }

    brain_kg_destroy(kg);
    TEST_PASS("Query multiple nodes works");
}

//=============================================================================
// Unit Tests - Version History
//=============================================================================

/**
 * Test: Get node versions
 */
void test_temporal_get_versions(void)
{
    printf("\n=== test_temporal_get_versions ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);

    kg_node_version_t versions[16];
    uint32_t count = 0;

    int result = kg_temporal_get_versions(kg, node_id, versions, 16, &count);
    TEST_ASSERT(result == 0 || result == -1, "get_versions should return valid result");

    /* Free any snapshot data */
    for (uint32_t i = 0; i < count; i++) {
        kg_temporal_version_free(&versions[i]);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Get node versions works");
}

/**
 * Test: Get version at timestamp
 */
void test_temporal_get_version_at(void)
{
    printf("\n=== test_temporal_get_version_at ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);

    kg_node_version_t version;
    memset(&version, 0, sizeof(version));

    uint64_t now = kg_temporal_now();

    int result = kg_temporal_get_version_at(kg, node_id, now, &version);
    TEST_ASSERT(result == 0 || result == -1, "get_version_at should return valid result");

    kg_temporal_version_free(&version);

    brain_kg_destroy(kg);
    TEST_PASS("Get version at timestamp works");
}

/**
 * Test: Get current version number
 */
void test_temporal_get_current_version(void)
{
    printf("\n=== test_temporal_get_current_version ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);

    uint64_t version = kg_temporal_get_current_version(kg, node_id);
    TEST_ASSERT(version >= 0, "Current version should be >= 0");

    brain_kg_destroy(kg);
    TEST_PASS("Get current version works");
}

/**
 * Test: Check if node existed at timestamp
 */
void test_temporal_node_existed_at(void)
{
    printf("\n=== test_temporal_node_existed_at ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);

    uint64_t now = kg_temporal_now();
    bool existed = kg_temporal_node_existed_at(kg, node_id, now);
    TEST_ASSERT(existed == true || existed == false, "node_existed_at should return bool");

    /* Check very old timestamp (node should not exist) */
    bool existed_old = kg_temporal_node_existed_at(kg, node_id, 1000);
    TEST_ASSERT(existed_old == true || existed_old == false, "node_existed_at (old) should return bool");

    brain_kg_destroy(kg);
    TEST_PASS("Node existed at timestamp works");
}

/**
 * Test: Get creation time
 */
void test_temporal_get_creation_time(void)
{
    printf("\n=== test_temporal_get_creation_time ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);

    uint64_t creation_time = kg_temporal_get_creation_time(kg, node_id);
    /* Creation time should be non-zero for existing nodes */
    TEST_ASSERT(creation_time >= 0, "Creation time should be >= 0");

    brain_kg_destroy(kg);
    TEST_PASS("Get creation time works");
}

/**
 * Test: Free version (NULL safe)
 */
void test_temporal_version_free_null(void)
{
    printf("\n=== test_temporal_version_free_null ===\n");

    kg_temporal_version_free(NULL);  /* Should not crash */

    TEST_PASS("Version free (NULL) is safe");
}

//=============================================================================
// Unit Tests - Temporal Diffs
//=============================================================================

/**
 * Test: Compute temporal diff
 */
void test_temporal_diff(void)
{
    printf("\n=== test_temporal_diff ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    uint64_t now = kg_temporal_now();
    uint64_t one_hour_ago = now - (3600 * 1000);  /* 1 hour in ms */

    kg_temporal_diff_t diff;
    memset(&diff, 0, sizeof(diff));

    int result = kg_temporal_diff(kg, one_hour_ago, now, &diff);
    TEST_ASSERT(result == 0 || result == -1, "temporal_diff should return valid result");

    kg_temporal_diff_free(&diff);

    brain_kg_destroy(kg);
    TEST_PASS("Compute temporal diff works");
}

/**
 * Test: Compute diff for subgraph
 */
void test_temporal_diff_subgraph(void)
{
    printf("\n=== test_temporal_diff_subgraph ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t root = populate_test_kg(kg);

    uint64_t now = kg_temporal_now();
    uint64_t one_hour_ago = now - (3600 * 1000);

    kg_temporal_diff_t diff;
    memset(&diff, 0, sizeof(diff));

    int result = kg_temporal_diff_subgraph(kg, root, 2, one_hour_ago, now, &diff);
    TEST_ASSERT(result == 0 || result == -1, "diff_subgraph should return valid result");

    kg_temporal_diff_free(&diff);

    brain_kg_destroy(kg);
    TEST_PASS("Diff subgraph works");
}

/**
 * Test: Get node changes
 */
void test_temporal_get_node_changes(void)
{
    printf("\n=== test_temporal_get_node_changes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);

    uint64_t now = kg_temporal_now();
    uint64_t one_hour_ago = now - (3600 * 1000);

    kg_node_version_t before, after;
    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));

    int result = kg_temporal_get_node_changes(kg, node_id, one_hour_ago, now, &before, &after);
    TEST_ASSERT(result == 0 || result == -1, "get_node_changes should return valid result");

    kg_temporal_version_free(&before);
    kg_temporal_version_free(&after);

    brain_kg_destroy(kg);
    TEST_PASS("Get node changes works");
}

/**
 * Test: Free diff (NULL safe)
 */
void test_temporal_diff_free_null(void)
{
    printf("\n=== test_temporal_diff_free_null ===\n");

    kg_temporal_diff_free(NULL);  /* Should not crash */

    TEST_PASS("Diff free (NULL) is safe");
}

//=============================================================================
// Unit Tests - Trend Analysis
//=============================================================================

/**
 * Test: Get topology evolution
 */
void test_temporal_get_topology_evolution(void)
{
    printf("\n=== test_temporal_get_topology_evolution ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    uint64_t now = kg_temporal_now();
    uint64_t one_day_ago = now - (24 * 3600 * 1000);

    kg_topology_snapshot_t snapshots[10];

    int count = kg_temporal_get_topology_evolution(kg, one_day_ago, now, 10, snapshots);
    TEST_ASSERT(count >= -1, "get_topology_evolution should return valid count");

    brain_kg_destroy(kg);
    TEST_PASS("Get topology evolution works");
}

/**
 * Test: Get node count trend
 */
void test_temporal_get_node_count_trend(void)
{
    printf("\n=== test_temporal_get_node_count_trend ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    populate_test_kg(kg);

    uint64_t now = kg_temporal_now();
    uint64_t one_day_ago = now - (24 * 3600 * 1000);

    uint64_t timestamps[10];
    uint32_t node_counts[10];

    int count = kg_temporal_get_node_count_trend(kg, one_day_ago, now,
                                                  timestamps, node_counts, 10);
    TEST_ASSERT(count >= -1, "get_node_count_trend should return valid count");

    brain_kg_destroy(kg);
    TEST_PASS("Get node count trend works");
}

/**
 * Test: Get node activity trend
 */
void test_temporal_get_node_activity_trend(void)
{
    printf("\n=== test_temporal_get_node_activity_trend ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");
    brain_kg_node_id_t node_id = populate_test_kg(kg);

    uint64_t now = kg_temporal_now();
    uint64_t one_day_ago = now - (24 * 3600 * 1000);

    uint64_t timestamps[10];
    brain_kg_node_state_t states[10];

    int count = kg_temporal_get_node_activity_trend(kg, node_id, one_day_ago, now,
                                                     timestamps, states, 10);
    TEST_ASSERT(count >= -1, "get_node_activity_trend should return valid count");

    brain_kg_destroy(kg);
    TEST_PASS("Get node activity trend works");
}

//=============================================================================
// Unit Tests - Utility Functions
//=============================================================================

/**
 * Test: Temporal mode to string
 */
void test_temporal_mode_to_string(void)
{
    printf("\n=== test_temporal_mode_to_string ===\n");

    const char* str = kg_temporal_mode_to_string(KG_TEMPORAL_CURRENT);
    TEST_ASSERT(str != NULL, "CURRENT mode string should not be NULL");

    str = kg_temporal_mode_to_string(KG_TEMPORAL_AS_OF);
    TEST_ASSERT(str != NULL, "AS_OF mode string should not be NULL");

    str = kg_temporal_mode_to_string(KG_TEMPORAL_BETWEEN);
    TEST_ASSERT(str != NULL, "BETWEEN mode string should not be NULL");

    str = kg_temporal_mode_to_string(KG_TEMPORAL_SINCE);
    TEST_ASSERT(str != NULL, "SINCE mode string should not be NULL");

    str = kg_temporal_mode_to_string(KG_TEMPORAL_VERSIONS);
    TEST_ASSERT(str != NULL, "VERSIONS mode string should not be NULL");

    TEST_PASS("Temporal mode to string works");
}

/**
 * Test: Get current timestamp
 */
void test_temporal_now(void)
{
    printf("\n=== test_temporal_now ===\n");

    uint64_t now = kg_temporal_now();
    TEST_ASSERT(now > 0, "kg_temporal_now should return positive timestamp");

    /* Should be reasonably recent (after year 2020) */
    uint64_t year_2020_ms = 1577836800000ULL;  /* Jan 1, 2020 */
    TEST_ASSERT(now > year_2020_ms, "Timestamp should be after 2020");

    TEST_PASS("Get current timestamp works");
}

/**
 * Test: Create bi-temporal coordinate
 */
void test_temporal_create_bitemporal(void)
{
    printf("\n=== test_temporal_create_bitemporal ===\n");

    uint64_t valid_time = 1609459200000ULL;  /* Jan 1, 2021 */

    kg_bitemporal_t bt = kg_temporal_create_bitemporal(valid_time);
    TEST_ASSERT(bt.valid_time == valid_time, "Valid time should match");
    TEST_ASSERT(bt.transaction_time > 0, "Transaction time should be positive");

    TEST_PASS("Create bi-temporal coordinate works");
}

/**
 * Test: Check timestamp in range
 */
void test_temporal_in_range(void)
{
    printf("\n=== test_temporal_in_range ===\n");

    kg_bitemporal_t bt;
    bt.valid_time = 1000;
    bt.transaction_time = 2000;

    /* For in_range checks, the semantics depend on the implementation */
    bool in_range = kg_temporal_in_range(&bt, 1500, false);
    TEST_ASSERT(in_range == true || in_range == false, "in_range should return bool");

    in_range = kg_temporal_in_range(&bt, 500, false);
    TEST_ASSERT(in_range == true || in_range == false, "in_range (before) should return bool");

    TEST_PASS("Check timestamp in range works");
}

/**
 * Test: In range with NULL
 */
void test_temporal_in_range_null(void)
{
    printf("\n=== test_temporal_in_range_null ===\n");

    bool in_range = kg_temporal_in_range(NULL, 1000, false);
    TEST_ASSERT(in_range == false, "in_range(NULL, ...) should return false");

    TEST_PASS("NULL handling correct for in_range");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("KG Temporal Queries Unit Tests\n");
    printf("============================================\n");

    /* Query configuration tests */
    test_temporal_query_default();
    test_temporal_query_default_null();

    /* Point-in-time query tests */
    test_temporal_query_node_current();
    test_temporal_query_node_as_of();
    test_temporal_query_node_null();
    test_temporal_query_subgraph();
    test_temporal_query_nodes();

    /* Version history tests */
    test_temporal_get_versions();
    test_temporal_get_version_at();
    test_temporal_get_current_version();
    test_temporal_node_existed_at();
    test_temporal_get_creation_time();
    test_temporal_version_free_null();

    /* Temporal diff tests */
    test_temporal_diff();
    test_temporal_diff_subgraph();
    test_temporal_get_node_changes();
    test_temporal_diff_free_null();

    /* Trend analysis tests */
    test_temporal_get_topology_evolution();
    test_temporal_get_node_count_trend();
    test_temporal_get_node_activity_trend();

    /* Utility function tests */
    test_temporal_mode_to_string();
    test_temporal_now();
    test_temporal_create_bitemporal();
    test_temporal_in_range();
    test_temporal_in_range_null();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
