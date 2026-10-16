/**
 * @file test_utils_hash_table.cpp
 * @brief Comprehensive unit tests for hash table utilities
 *
 * WHAT: 100% test coverage for nimcp_hash_table.c
 * WHY:  Hash tables are critical for concept storage, routing, and lookups
 * HOW:  Test all operations, key types, algorithms, edge cases
 *
 * TEST COVERAGE:
 * 1. hash_table_create() - creation with defaults and custom config
 * 2. hash_table_destroy() - cleanup and NULL safety
 * 3. hash_table_insert_string() - string key insertion
 * 4. hash_table_lookup_string() - string key retrieval
 * 5. hash_table_remove_string() - string key deletion
 * 6. hash_table_insert_uint32() - integer key insertion
 * 7. hash_table_lookup_uint32() - integer key retrieval
 * 8. hash_table_remove_uint32() - integer key deletion
 * 9. hash_table_size() - size tracking
 * 10. hash_table_clear() - bulk removal
 * 11. hash_table_iterate() - traversal
 * 12. hash_table_stats() - collision statistics
 * 13. Case-insensitive keys
 * 14. Hash algorithm variations
 * 15. Edge cases and error handling
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <string>

    #include "utils/containers/nimcp_hash_table.h"
    #include "utils/nimcp_test_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HashTableTest : public NimcpTestBase {
protected:
    hash_table_t* table = nullptr;

    void TearDown() override {
        if (table) {
            hash_table_destroy(table);
            table = nullptr;
        }

        NimcpTestBase::TearDown();  // Call parent TearDown last for global state cleanup
    }
};

//=============================================================================
// Unit Test 1: Create with default config
//=============================================================================

TEST_F(HashTableTest, Create_DefaultConfig) {
    // WHAT: Create hash table with NULL config (uses defaults)
    // WHY:  Test default initialization

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    EXPECT_EQ(hash_table_size(table), 0u);
    EXPECT_GT(hash_table_bucket_count(table), 0u);
    SUCCEED() << "Hash table created with default config";
}

//=============================================================================
// Unit Test 2: Create with custom config
//=============================================================================

TEST_F(HashTableTest, Create_CustomConfig) {
    // WHAT: Create hash table with specific configuration
    // WHY:  Test custom initialization

    hash_table_config_t config = {};
    config.initial_buckets = 128;
    config.key_type = HASH_KEY_STRING;
    config.hash_algorithm = HASH_ALG_FNV1A;
    config.case_insensitive = false;

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);

    EXPECT_EQ(hash_table_bucket_count(table), 128u);
    SUCCEED() << "Hash table created with custom config";
}

//=============================================================================
// Unit Test 3: String insert and lookup
//=============================================================================

TEST_F(HashTableTest, StringKeys_InsertAndLookup) {
    // WHAT: Insert and retrieve values with string keys
    // WHY:  Core string key functionality

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    int value1 = 42;
    int value2 = 99;

    EXPECT_TRUE(hash_table_insert_string(table, "key1", &value1, sizeof(value1)));
    EXPECT_TRUE(hash_table_insert_string(table, "key2", &value2, sizeof(value2)));

    EXPECT_EQ(hash_table_size(table), 2u);

    int* retrieved1 = (int*)hash_table_lookup_string(table, "key1");
    int* retrieved2 = (int*)hash_table_lookup_string(table, "key2");

    ASSERT_NE(retrieved1, nullptr);
    ASSERT_NE(retrieved2, nullptr);

    EXPECT_EQ(*retrieved1, 42);
    EXPECT_EQ(*retrieved2, 99);

    SUCCEED() << "String key insert and lookup works";
}

//=============================================================================
// Unit Test 4: String key removal
//=============================================================================

TEST_F(HashTableTest, StringKeys_Remove) {
    // WHAT: Remove entries by string key
    // WHY:  Test deletion functionality

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    int value = 123;
    hash_table_insert_string(table, "remove_me", &value, sizeof(value));

    EXPECT_EQ(hash_table_size(table), 1u);

    EXPECT_TRUE(hash_table_remove_string(table, "remove_me"));
    EXPECT_EQ(hash_table_size(table), 0u);

    EXPECT_EQ(hash_table_lookup_string(table, "remove_me"), nullptr);

    // Removing non-existent key should return false
    EXPECT_FALSE(hash_table_remove_string(table, "nonexistent"));

    SUCCEED() << "String key removal works";
}

//=============================================================================
// Unit Test 5: Update existing key
//=============================================================================

TEST_F(HashTableTest, StringKeys_Update) {
    // WHAT: Update value for existing key
    // WHY:  Test overwrite behavior

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    int value1 = 100;
    int value2 = 200;

    hash_table_insert_string(table, "key", &value1, sizeof(value1));
    hash_table_insert_string(table, "key", &value2, sizeof(value2));

    // Size should still be 1 (update, not insert)
    EXPECT_EQ(hash_table_size(table), 1u);

    int* retrieved = (int*)hash_table_lookup_string(table, "key");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 200);

    SUCCEED() << "String key update works";
}

//=============================================================================
// Unit Test 6: Integer keys (uint32)
//=============================================================================

TEST_F(HashTableTest, IntegerKeys_Basic) {
    // WHAT: Test uint32_t key operations
    // WHY:  Support integer keys

    hash_table_config_t config = {};
    config.initial_buckets = 64;
    config.key_type = HASH_KEY_UINT32;
    config.hash_algorithm = HASH_ALG_MURMUR3;

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);

    int value1 = 111;
    int value2 = 222;

    EXPECT_TRUE(hash_table_insert_uint32(table, 12345, &value1, sizeof(value1)));
    EXPECT_TRUE(hash_table_insert_uint32(table, 67890, &value2, sizeof(value2)));

    EXPECT_EQ(hash_table_size(table), 2u);

    int* retrieved1 = (int*)hash_table_lookup_uint32(table, 12345);
    int* retrieved2 = (int*)hash_table_lookup_uint32(table, 67890);

    ASSERT_NE(retrieved1, nullptr);
    ASSERT_NE(retrieved2, nullptr);

    EXPECT_EQ(*retrieved1, 111);
    EXPECT_EQ(*retrieved2, 222);

    EXPECT_TRUE(hash_table_remove_uint32(table, 12345));
    EXPECT_EQ(hash_table_size(table), 1u);

    SUCCEED() << "Integer key operations work";
}

//=============================================================================
// Unit Test 7: Case-insensitive string keys
//=============================================================================

TEST_F(HashTableTest, StringKeys_CaseInsensitive) {
    // WHAT: Test case-insensitive key matching
    // WHY:  Support case-insensitive lookups

    hash_table_config_t config = {};
    config.initial_buckets = 64;
    config.key_type = HASH_KEY_STRING;
    config.hash_algorithm = HASH_ALG_FNV1A;
    config.case_insensitive = true;

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);

    int value = 555;
    hash_table_insert_string(table, "ConcePT", &value, sizeof(value));

    // Should find with different case
    int* retrieved1 = (int*)hash_table_lookup_string(table, "concept");
    int* retrieved2 = (int*)hash_table_lookup_string(table, "CONCEPT");
    int* retrieved3 = (int*)hash_table_lookup_string(table, "ConcePT");

    ASSERT_NE(retrieved1, nullptr);
    ASSERT_NE(retrieved2, nullptr);
    ASSERT_NE(retrieved3, nullptr);

    EXPECT_EQ(*retrieved1, 555);
    EXPECT_EQ(*retrieved2, 555);
    EXPECT_EQ(*retrieved3, 555);

    SUCCEED() << "Case-insensitive keys work";
}

//=============================================================================
// Unit Test 8: Clear operation
//=============================================================================

TEST_F(HashTableTest, Clear_RemovesAllEntries) {
    // WHAT: Clear table removes all entries
    // WHY:  Bulk deletion functionality

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    int value = 0;
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        hash_table_insert_string(table, key, &value, sizeof(value));
    }

    EXPECT_EQ(hash_table_size(table), 10u);

    hash_table_clear(table);

    EXPECT_EQ(hash_table_size(table), 0u);
    EXPECT_EQ(hash_table_lookup_string(table, "key0"), nullptr);

    SUCCEED() << "Clear operation works";
}

//=============================================================================
// Unit Test 9: Iteration
//=============================================================================

TEST_F(HashTableTest, Iterate_TraversesAllEntries) {
    // WHAT: Iterate over all entries
    // WHY:  Process all stored values

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    // Insert 5 entries
    for (int i = 0; i < 5; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        hash_table_insert_string(table, key, &i, sizeof(i));
    }

    // Count entries via iteration
    int count = 0;
    auto callback = [](const void* key, size_t key_size, void* value, size_t value_size,
                       void* user_data) -> bool {
        int* counter = (int*)user_data;
        (*counter)++;
        return true; // Continue iteration
    };

    hash_table_iterate(table, callback, &count);

    EXPECT_EQ(count, 5);
    SUCCEED() << "Iteration works";
}

//=============================================================================
// Unit Test 10: Statistics
//=============================================================================

TEST_F(HashTableTest, Stats_CollisionMetrics) {
    // WHAT: Get collision statistics
    // WHY:  Monitor hash table health

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    // Insert entries
    for (int i = 0; i < 20; i++) {
        char key[32];
        snprintf(key, sizeof(key), "entry%d", i);
        int value = i;
        hash_table_insert_string(table, key, &value, sizeof(value));
    }

    size_t max_chain = 0;
    float avg_chain = 0.0f;
    size_t empty_buckets = 0;

    hash_table_stats(table, &max_chain, &avg_chain, &empty_buckets);

    // Basic sanity checks
    EXPECT_GT(max_chain, 0u);
    EXPECT_GT(avg_chain, 0.0f);

    SUCCEED() << "Statistics reporting works";
}

//=============================================================================
// Unit Test 11: Destroy with NULL is safe
//=============================================================================

TEST_F(HashTableTest, Destroy_NullIsSafe) {
    // WHAT: Destroying NULL table doesn't crash
    // WHY:  Defensive programming

    hash_table_destroy(nullptr);
    SUCCEED() << "Destroying NULL table is safe";
}

//=============================================================================
// Unit Test 12: Lookup non-existent key
//=============================================================================

TEST_F(HashTableTest, Lookup_NonExistentKey) {
    // WHAT: Lookup of missing key returns NULL
    // WHY:  Error handling

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    EXPECT_EQ(hash_table_lookup_string(table, "missing"), nullptr);
    EXPECT_EQ(hash_table_lookup_uint32(table, 9999), nullptr);

    SUCCEED() << "Non-existent key lookup returns NULL";
}

//=============================================================================
// Unit Test 13: Different hash algorithms
//=============================================================================

TEST_F(HashTableTest, HashAlgorithms_FNV1A_vs_DJB2) {
    // WHAT: Test different hash algorithms
    // WHY:  Verify algorithm selection works

    // FNV1A
    hash_table_config_t config1 = {};
    config1.initial_buckets = 64;
    config1.key_type = HASH_KEY_STRING;
    config1.hash_algorithm = HASH_ALG_FNV1A;

    hash_table_t* table1 = hash_table_create(&config1);
    ASSERT_NE(table1, nullptr);

    int value = 777;
    hash_table_insert_string(table1, "test", &value, sizeof(value));

    int* retrieved1 = (int*)hash_table_lookup_string(table1, "test");
    ASSERT_NE(retrieved1, nullptr);
    EXPECT_EQ(*retrieved1, 777);

    // DJB2
    hash_table_config_t config2 = {};
    config2.initial_buckets = 64;
    config2.key_type = HASH_KEY_STRING;
    config2.hash_algorithm = HASH_ALG_DJB2;

    hash_table_t* table2 = hash_table_create(&config2);
    ASSERT_NE(table2, nullptr);

    hash_table_insert_string(table2, "test", &value, sizeof(value));

    int* retrieved2 = (int*)hash_table_lookup_string(table2, "test");
    ASSERT_NE(retrieved2, nullptr);
    EXPECT_EQ(*retrieved2, 777);

    hash_table_destroy(table1);
    hash_table_destroy(table2);

    SUCCEED() << "Different hash algorithms work";
}

//=============================================================================
// Unit Test 14: Large table (stress test)
//=============================================================================

TEST_F(HashTableTest, StressTest_LargeTable) {
    // WHAT: Insert many entries
    // WHY:  Test scalability

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    const int NUM_ENTRIES = 1000;

    // Insert 1000 entries
    for (int i = 0; i < NUM_ENTRIES; i++) {
        char key[64];
        snprintf(key, sizeof(key), "entry_%d", i);
        hash_table_insert_string(table, key, &i, sizeof(i));
    }

    EXPECT_EQ(hash_table_size(table), (size_t)NUM_ENTRIES);

    // Verify random lookups
    int* val500 = (int*)hash_table_lookup_string(table, "entry_500");
    int* val999 = (int*)hash_table_lookup_string(table, "entry_999");

    ASSERT_NE(val500, nullptr);
    ASSERT_NE(val999, nullptr);

    EXPECT_EQ(*val500, 500);
    EXPECT_EQ(*val999, 999);

    SUCCEED() << "Large table (1000 entries) works";
}

//=============================================================================
// Unit Test 15: Empty table operations
//=============================================================================

TEST_F(HashTableTest, EmptyTable_Operations) {
    // WHAT: Operations on empty table
    // WHY:  Edge case handling

    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);

    EXPECT_EQ(hash_table_size(table), 0u);
    EXPECT_EQ(hash_table_lookup_string(table, "anything"), nullptr);
    EXPECT_FALSE(hash_table_remove_string(table, "anything"));

    // Clear empty table should be safe
    hash_table_clear(table);
    EXPECT_EQ(hash_table_size(table), 0u);

    // Iterate empty table should be safe
    int count = 0;
    auto callback = [](const void*, size_t, void*, size_t, void* user_data) -> bool {
        int* c = (int*)user_data;
        (*c)++;
        return true;
    };
    hash_table_iterate(table, callback, &count);
    EXPECT_EQ(count, 0);

    SUCCEED() << "Empty table operations are safe";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
