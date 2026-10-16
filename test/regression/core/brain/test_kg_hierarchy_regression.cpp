/**
 * @file test_kg_hierarchy_regression.cpp
 * @brief Regression tests for KG Hierarchy edge metadata API
 *
 * Tests edge cases and boundary conditions:
 * - Invalid node IDs (BRAIN_KG_INVALID_NODE, 0, UINT32_MAX)
 * - Boundary values for int32_t (INT32_MIN, INT32_MAX, 0)
 * - Empty and maximum-length keys
 * - Metadata persistence across multiple update cycles
 * - Key collision and overwrite behavior
 * - API contract stability
 * - Memory safety (no leaks, no corruption)
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <algorithm>
#include <climits>

#include "core/brain/nimcp_kg_hierarchy.h"
#include "core/brain/nimcp_brain_kg.h"

// ============================================================================
// Test Fixture
// ============================================================================

class KGHierarchyEdgeMetadataRegression : public ::testing::Test {
protected:
    brain_kg_t* kg;
    kg_hierarchy_t* hier;
    kg_hierarchy_config_t config;

    void SetUp() override {
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg_config.enable_integrity_checks = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);

        kg_hierarchy_default_config(&config);
        hier = nullptr;
    }

    void TearDown() override {
        if (hier) {
            kg_hierarchy_destroy(hier);
            hier = nullptr;
        }
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    brain_kg_node_id_t add_module(const char* name, brain_kg_node_type_t type) {
        return brain_kg_add_node(kg, name, type, "Test module");
    }

    // Create a simple two-node graph for focused edge metadata testing
    void create_simple_graph(brain_kg_node_id_t& a, brain_kg_node_id_t& b) {
        a = add_module("node_a", BRAIN_KG_NODE_CORE);
        b = add_module("node_b", BRAIN_KG_NODE_COGNITIVE);
        brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test_edge", 1.0f);
    }
};

// ============================================================================
// Boundary Value Tests - int32_t Range
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, BoundaryValueINT32MAX) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "max_val", INT32_MAX), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "max_val", &value), 0);
    EXPECT_EQ(value, INT32_MAX);
}

TEST_F(KGHierarchyEdgeMetadataRegression, BoundaryValueINT32MIN) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "min_val", INT32_MIN), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "min_val", &value), 0);
    EXPECT_EQ(value, INT32_MIN);
}

TEST_F(KGHierarchyEdgeMetadataRegression, BoundaryValueZero) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "zero_val", 0), 0);

    int32_t value = -1;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "zero_val", &value), 0);
    EXPECT_EQ(value, 0);
}

TEST_F(KGHierarchyEdgeMetadataRegression, BoundaryValueNegativeOne) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "neg_one", -1), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "neg_one", &value), 0);
    EXPECT_EQ(value, -1);
}

// ============================================================================
// Invalid Node ID Tests
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, InvalidNodeIDSetMetadata) {
    // Setting metadata with BRAIN_KG_INVALID_NODE should still succeed
    // because the metadata API doesn't validate node existence in KG
    brain_kg_node_id_t a = add_module("valid_node", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // The API stores metadata keyed by (from, to, key) without
    // checking if those nodes actually exist in the KG
    int result = kg_hierarchy_set_edge_metadata_int(
        hier, BRAIN_KG_INVALID_NODE, a, "test", 42);
    // This should succeed (no node validation in metadata layer)
    EXPECT_EQ(result, 0);

    int32_t value = 0;
    result = kg_hierarchy_get_edge_metadata_int(
        hier, BRAIN_KG_INVALID_NODE, a, "test", &value);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(value, 42);
}

TEST_F(KGHierarchyEdgeMetadataRegression, ZeroNodeIDsMetadata) {
    // Node ID 0 is valid in the KG system
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, 0, 0, "self_loop", 42), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, 0, 0, "self_loop", &value), 0);
    EXPECT_EQ(value, 42);
}

// ============================================================================
// Key Edge Cases
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, EmptyKeyReturnsError) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Empty key should fail
    int result = kg_hierarchy_set_edge_metadata_int(hier, a, b, "", 42);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyEdgeMetadataRegression, LongKeyTruncation) {
    // Keys longer than KG_EDGE_METADATA_KEY_LEN (64) are safely truncated on store.
    // However, get uses strcmp against the stored (truncated) key, so querying
    // with the original long key will NOT match. Must use truncated key to retrieve.
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Create a key that's 100 chars long (exceeds 64 limit)
    char long_key[101];
    memset(long_key, 'A', 100);
    long_key[100] = '\0';

    // Set should succeed (key gets truncated internally via strncpy)
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, long_key, 42), 0);

    // Get with the full 100-char key fails because strcmp compares
    // against the stored 63-char truncated key
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, long_key, &value), -1);

    // But get with the truncated key (63 chars) succeeds
    char truncated_key[64];
    memset(truncated_key, 'A', 63);
    truncated_key[63] = '\0';

    value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, truncated_key, &value), 0);
    EXPECT_EQ(value, 42);
}

TEST_F(KGHierarchyEdgeMetadataRegression, SingleCharKey) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, "x", 7), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, "x", &value), 0);
    EXPECT_EQ(value, 7);
}

TEST_F(KGHierarchyEdgeMetadataRegression, MaxLengthKey63Chars) {
    // Key exactly at the limit (63 chars + null = 64 bytes)
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    char key_63[64];
    memset(key_63, 'Z', 63);
    key_63[63] = '\0';

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, key_63, 63), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, key_63, &value), 0);
    EXPECT_EQ(value, 63);
}

// ============================================================================
// Null Parameter Regression Tests
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, SetNullHierarchy) {
    int result = kg_hierarchy_set_edge_metadata_int(nullptr, 1, 2, "key", 42);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyEdgeMetadataRegression, SetNullKey) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int result = kg_hierarchy_set_edge_metadata_int(hier, a, b, nullptr, 42);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyEdgeMetadataRegression, GetNullHierarchy) {
    int32_t value = 0;
    int result = kg_hierarchy_get_edge_metadata_int(nullptr, 1, 2, "key", &value);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyEdgeMetadataRegression, GetNullKey) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int32_t value = 0;
    int result = kg_hierarchy_get_edge_metadata_int(hier, a, b, nullptr, &value);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyEdgeMetadataRegression, GetNullValueOutput) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    kg_hierarchy_set_edge_metadata_int(hier, a, b, "key", 42);

    int result = kg_hierarchy_get_edge_metadata_int(hier, a, b, "key", nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Overwrite / Update Regression Tests
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, OverwriteValueMultipleTimes) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Overwrite the same key 10 times
    for (int32_t i = 0; i < 10; i++) {
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, a, b, "counter", i), 0);
    }

    // Final value should be 9
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "counter", &value), 0);
    EXPECT_EQ(value, 9);
}

TEST_F(KGHierarchyEdgeMetadataRegression, OverwriteDoesNotCreateDuplicates) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Set same key many times - should overwrite, not create duplicates
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, a, b, "dup_test", i), 0);
    }

    // Only 1 entry should exist for this key, value should be 99
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "dup_test", &value), 0);
    EXPECT_EQ(value, 99);

    // Adding a different key should still work (metadata not exhausted)
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "another_key", 42), 0);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "another_key", &value), 0);
    EXPECT_EQ(value, 42);
}

// ============================================================================
// Non-existent Key Regression Tests
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, GetNonexistentKeyReturnsErrorAndZero) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int32_t value = 999;  // Pre-fill with non-zero to verify it gets set to 0
    int result = kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "nonexistent", &value);
    EXPECT_EQ(result, -1);
    EXPECT_EQ(value, 0);  // Implementation sets *value = 0 on not-found
}

TEST_F(KGHierarchyEdgeMetadataRegression, GetFromNonexistentEdge) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Set metadata on (a, b) but query (b, a) - different edge
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "test_key", 42), 0);

    int32_t value = 999;
    int result = kg_hierarchy_get_edge_metadata_int(
        hier, b, a, "test_key", &value);
    EXPECT_EQ(result, -1);
    EXPECT_EQ(value, 0);
}

// ============================================================================
// Multiple Entries Regression Tests
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, ManyDistinctEntries) {
    // Add many distinct (edge, key) pairs to test dynamic array growth
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    const int NUM_ENTRIES = 128;
    // Use different from/to pairs to create distinct entries
    for (int i = 0; i < NUM_ENTRIES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        // Use different (from, to) pairs by varying from
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, (brain_kg_node_id_t)i, (brain_kg_node_id_t)(i + 1000),
            key, i * 10), 0)
            << "Failed at entry " << i;
    }

    // Verify all entries
    for (int i = 0; i < NUM_ENTRIES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        int32_t value = 0;
        EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
            hier, (brain_kg_node_id_t)i, (brain_kg_node_id_t)(i + 1000),
            key, &value), 0)
            << "Failed to get entry " << i;
        EXPECT_EQ(value, i * 10)
            << "Value mismatch at entry " << i;
    }
}

TEST_F(KGHierarchyEdgeMetadataRegression, ManyKeysOnSingleEdge) {
    // Add many different keys to the same edge
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    const int NUM_KEYS = 50;
    for (int i = 0; i < NUM_KEYS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "prop_%d", i);
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, a, b, key, i * 100), 0)
            << "Failed to set key " << i;
    }

    // Verify all keys
    for (int i = 0; i < NUM_KEYS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "prop_%d", i);
        int32_t value = 0;
        EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
            hier, a, b, key, &value), 0)
            << "Failed to get key " << i;
        EXPECT_EQ(value, i * 100)
            << "Value mismatch for key " << i;
    }
}

// ============================================================================
// API Contract Stability Tests
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, SetReturnsZeroOnSuccess) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // API contract: returns 0 on success
    int result = kg_hierarchy_set_edge_metadata_int(hier, a, b, "key", 42);
    EXPECT_EQ(result, 0);
}

TEST_F(KGHierarchyEdgeMetadataRegression, GetReturnsZeroOnSuccessNegOneOnFailure) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Set a value
    kg_hierarchy_set_edge_metadata_int(hier, a, b, "key", 42);

    // Success case: returns 0
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, "key", &value), 0);

    // Failure case: returns -1
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, "missing", &value), -1);
}

TEST_F(KGHierarchyEdgeMetadataRegression, ValuePointerNotModifiedOnNullError) {
    // When get fails due to null key, value pointer should not be written
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    int32_t sentinel = 12345;
    // Null key causes early return before any value write
    kg_hierarchy_get_edge_metadata_int(hier, a, b, nullptr, &sentinel);
    // The sentinel should be untouched because the function returns early
    EXPECT_EQ(sentinel, 12345);
}

// ============================================================================
// Metadata Persistence Across Hierarchy Operations
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, MetadataPersistsAcrossInvalidation) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "persist_test", 55), 0);

    // Invalidate hierarchy cache
    kg_hierarchy_invalidate(hier);

    // Metadata should still be accessible (it lives in hierarchy struct,
    // not in the cache that gets invalidated)
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "persist_test", &value), 0);
    EXPECT_EQ(value, 55);
}

TEST_F(KGHierarchyEdgeMetadataRegression, MetadataPersistsAcrossReadLockCycle) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "lock_test", 88), 0);

    // Lock and unlock
    EXPECT_EQ(kg_hierarchy_read_lock(hier), 0);
    EXPECT_EQ(kg_hierarchy_read_unlock(hier), 0);

    // Metadata should persist
    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "lock_test", &value), 0);
    EXPECT_EQ(value, 88);
}

// ============================================================================
// Key String Safety
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, KeyWithSpecialCharacters) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Keys with underscores, dots, hyphens
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "my.key-name_v2", 42), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "my.key-name_v2", &value), 0);
    EXPECT_EQ(value, 42);
}

TEST_F(KGHierarchyEdgeMetadataRegression, KeyWithSpaces) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a, b, "key with spaces", 99), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "key with spaces", &value), 0);
    EXPECT_EQ(value, 99);
}

TEST_F(KGHierarchyEdgeMetadataRegression, SimilarKeysAreDistinct) {
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Keys that differ only by case or trailing chars
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, "key", 1), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, "Key", 2), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, "KEY", 3), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, "key1", 4), 0);

    int32_t v1 = 0, v2 = 0, v3 = 0, v4 = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, "key", &v1), 0);
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, "Key", &v2), 0);
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, "KEY", &v3), 0);
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, "key1", &v4), 0);

    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v2, 2);
    EXPECT_EQ(v3, 3);
    EXPECT_EQ(v4, 4);
}

// ============================================================================
// Destroy Safety
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataRegression, DestroyAfterMetadataSetDoesNotLeak) {
    // This test verifies that destroying a hierarchy with metadata entries
    // does not cause memory leaks (run with ASAN/valgrind to verify)
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Add several metadata entries
    for (int i = 0; i < 50; i++) {
        char key[32];
        snprintf(key, sizeof(key), "leak_test_%d", i);
        kg_hierarchy_set_edge_metadata_int(hier, a, b, key, i);
    }

    // Destroy will free edge_metadata array
    kg_hierarchy_destroy(hier);
    hier = nullptr;  // Prevent double-free in TearDown
}

TEST_F(KGHierarchyEdgeMetadataRegression, CreateDestroyCreateWithMetadata) {
    // Create hierarchy, add metadata, destroy, create new one, add metadata again
    // Ensures clean lifecycle
    brain_kg_node_id_t a, b;
    create_simple_graph(a, b);

    // First lifecycle
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, "cycle", 1), 0);
    kg_hierarchy_destroy(hier);

    // Second lifecycle
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, "cycle", 2), 0);

    int32_t value = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(hier, a, b, "cycle", &value), 0);
    EXPECT_EQ(value, 2);
}
