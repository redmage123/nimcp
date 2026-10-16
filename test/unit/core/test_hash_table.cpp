/**
 * @file test_hash_table.cpp
 * @brief Unit tests for generic hash table implementation
 */

#include <gtest/gtest.h>
#include <cstring>
#include "utils/containers/nimcp_hash_table.h"

// Test fixture for hash table operations
class HashTableTest : public ::testing::Test {
   protected:
    hash_table_t* table = nullptr;

    void TearDown() override
    {
        if (table) {
            hash_table_destroy(table);
            table = nullptr;
        }
    }

    // Helper to create string table with default config
    void CreateDefaultStringTable()
    {
        hash_table_config_t config = {.initial_buckets = 16,
                                      .key_type = HASH_KEY_STRING,
                                      .hash_algorithm = HASH_ALG_FNV1A,
                                      .custom_hash_fn = nullptr,
                                      .custom_compare_fn = nullptr,
                                      .value_destructor = nullptr,
                                      .case_insensitive = false,
                                      .thread_safe = false};
        table = hash_table_create(&config);
    }

    // Helper to create integer table
    void CreateIntegerTable()
    {
        hash_table_config_t config = {.initial_buckets = 16,
                                      .key_type = HASH_KEY_UINT32,
                                      .hash_algorithm = HASH_ALG_MURMUR3,
                                      .custom_hash_fn = nullptr,
                                      .custom_compare_fn = nullptr,
                                      .value_destructor = nullptr,
                                      .case_insensitive = false,
                                      .thread_safe = false};
        table = hash_table_create(&config);
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

/**
 * WHAT: Test hash table creation with default config
 * WHY: Verify table can be created with NULL config
 */
TEST_F(HashTableTest, Create_DefaultConfig)
{
    table = hash_table_create(nullptr);
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(hash_table_size(table), 0);
}

/**
 * WHAT: Test hash table creation with custom config
 * WHY: Verify configuration is respected
 */
TEST_F(HashTableTest, Create_CustomConfig)
{
    hash_table_config_t config = {.initial_buckets = 256,
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_MURMUR3,
                                  .custom_hash_fn = nullptr,
                                  .custom_compare_fn = nullptr,
                                  .value_destructor = nullptr,
                                  .case_insensitive = false,
                                  .thread_safe = false};

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(hash_table_bucket_count(table), 256);
}

/**
 * WHAT: Test hash table destroy with null
 * WHY: Verify null safety (should not crash)
 */
TEST_F(HashTableTest, Destroy_Null)
{
    hash_table_destroy(nullptr);  // Should not crash
}

/**
 * WHAT: Test hash table destroy with entries
 * WHY: Verify all memory is freed
 */
TEST_F(HashTableTest, Destroy_WithEntries)
{
    CreateDefaultStringTable();

    int values[] = {1, 2, 3};
    hash_table_insert_string(table, "key1", &values[0], sizeof(int));
    hash_table_insert_string(table, "key2", &values[1], sizeof(int));
    hash_table_insert_string(table, "key3", &values[2], sizeof(int));

    // Destroy should free all entries
    hash_table_destroy(table);
    table = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// String Key Operations Tests
//=============================================================================

/**
 * WHAT: Test basic string insert and lookup
 * WHY: Verify fundamental hash table operations
 */
TEST_F(HashTableTest, StringKey_InsertLookup)
{
    CreateDefaultStringTable();

    int value = 42;
    ASSERT_TRUE(hash_table_insert_string(table, "test_key", &value, sizeof(int)));

    int* retrieved = static_cast<int*>(hash_table_lookup_string(table, "test_key"));
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 42);
}

/**
 * WHAT: Test string key not found
 * WHY: Verify lookup returns null for missing keys
 */
TEST_F(HashTableTest, StringKey_NotFound)
{
    CreateDefaultStringTable();

    void* result = hash_table_lookup_string(table, "nonexistent");
    EXPECT_EQ(result, nullptr);
}

/**
 * WHAT: Test string key update
 * WHY: Verify inserting with same key updates value
 */
TEST_F(HashTableTest, StringKey_Update)
{
    CreateDefaultStringTable();

    int value1 = 100;
    int value2 = 200;

    ASSERT_TRUE(hash_table_insert_string(table, "key", &value1, sizeof(int)));
    EXPECT_EQ(hash_table_size(table), 1);

    // Update with same key
    ASSERT_TRUE(hash_table_insert_string(table, "key", &value2, sizeof(int)));
    EXPECT_EQ(hash_table_size(table), 1);  // Size should not increase

    int* retrieved = static_cast<int*>(hash_table_lookup_string(table, "key"));
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 200);  // Should have new value
}

/**
 * WHAT: Test string key removal
 * WHY: Verify entries can be deleted
 */
TEST_F(HashTableTest, StringKey_Remove)
{
    CreateDefaultStringTable();

    int value = 42;
    ASSERT_TRUE(hash_table_insert_string(table, "key", &value, sizeof(int)));
    EXPECT_EQ(hash_table_size(table), 1);

    ASSERT_TRUE(hash_table_remove_string(table, "key"));
    EXPECT_EQ(hash_table_size(table), 0);

    void* retrieved = hash_table_lookup_string(table, "key");
    EXPECT_EQ(retrieved, nullptr);
}

/**
 * WHAT: Test removing nonexistent key
 * WHY: Verify remove returns false for missing keys
 */
TEST_F(HashTableTest, StringKey_RemoveNonexistent)
{
    CreateDefaultStringTable();

    EXPECT_FALSE(hash_table_remove_string(table, "nonexistent"));
}

/**
 * WHAT: Test multiple string insertions
 * WHY: Verify table can handle multiple entries
 */
TEST_F(HashTableTest, StringKey_MultipleInserts)
{
    CreateDefaultStringTable();

    const int count = 100;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        ASSERT_TRUE(hash_table_insert_string(table, key, &i, sizeof(int)));
    }

    EXPECT_EQ(hash_table_size(table), count);

    // Verify all values can be retrieved
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        int* value = static_cast<int*>(hash_table_lookup_string(table, key));
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, i);
    }
}

/**
 * WHAT: Test case-insensitive string keys
 * WHY: Verify case-insensitive mode works
 */
TEST_F(HashTableTest, StringKey_CaseInsensitive)
{
    hash_table_config_t config = {.initial_buckets = 16,
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_FNV1A,
                                  .custom_hash_fn = nullptr,
                                  .custom_compare_fn = nullptr,
                                  .value_destructor = nullptr,
                                  .case_insensitive = true,
                                  .thread_safe = false};
    table = hash_table_create(&config);

    int value = 42;
    ASSERT_TRUE(hash_table_insert_string(table, "TestKey", &value, sizeof(int)));

    // Should find with different case
    int* retrieved = static_cast<int*>(hash_table_lookup_string(table, "testkey"));
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 42);

    retrieved = static_cast<int*>(hash_table_lookup_string(table, "TESTKEY"));
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 42);
}

/**
 * WHAT: Test null string key handling
 * WHY: Verify null safety
 */
TEST_F(HashTableTest, StringKey_NullKey)
{
    CreateDefaultStringTable();

    int value = 42;
    EXPECT_FALSE(hash_table_insert_string(table, nullptr, &value, sizeof(int)));
    EXPECT_EQ(hash_table_lookup_string(table, nullptr), nullptr);
    EXPECT_FALSE(hash_table_remove_string(table, nullptr));
}

/**
 * WHAT: Test null value handling
 * WHY: Verify null value pointer is rejected
 */
TEST_F(HashTableTest, StringKey_NullValue)
{
    CreateDefaultStringTable();

    EXPECT_FALSE(hash_table_insert_string(table, "key", nullptr, sizeof(int)));
}

//=============================================================================
// Integer Key Operations Tests
//=============================================================================

/**
 * WHAT: Test uint32_t key insert and lookup
 * WHY: Verify integer key operations
 */
TEST_F(HashTableTest, IntegerKey_InsertLookup)
{
    CreateIntegerTable();

    int value = 42;
    ASSERT_TRUE(hash_table_insert_uint32(table, 12345, &value, sizeof(int)));

    int* retrieved = static_cast<int*>(hash_table_lookup_uint32(table, 12345));
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(*retrieved, 42);
}

/**
 * WHAT: Test integer key not found
 * WHY: Verify lookup returns null for missing keys
 */
TEST_F(HashTableTest, IntegerKey_NotFound)
{
    CreateIntegerTable();

    void* result = hash_table_lookup_uint32(table, 99999);
    EXPECT_EQ(result, nullptr);
}

/**
 * WHAT: Test integer key removal
 * WHY: Verify integer key deletion
 */
TEST_F(HashTableTest, IntegerKey_Remove)
{
    CreateIntegerTable();

    int value = 42;
    ASSERT_TRUE(hash_table_insert_uint32(table, 12345, &value, sizeof(int)));
    EXPECT_EQ(hash_table_size(table), 1);

    ASSERT_TRUE(hash_table_remove_uint32(table, 12345));
    EXPECT_EQ(hash_table_size(table), 0);

    void* retrieved = hash_table_lookup_uint32(table, 12345);
    EXPECT_EQ(retrieved, nullptr);
}

/**
 * WHAT: Test multiple integer insertions
 * WHY: Verify table handles many integer keys
 */
TEST_F(HashTableTest, IntegerKey_MultipleInserts)
{
    CreateIntegerTable();

    const int count = 100;
    for (uint32_t i = 0; i < count; i++) {
        ASSERT_TRUE(hash_table_insert_uint32(table, i, &i, sizeof(uint32_t)));
    }

    EXPECT_EQ(hash_table_size(table), count);

    // Verify all values
    for (uint32_t i = 0; i < count; i++) {
        uint32_t* value = static_cast<uint32_t*>(hash_table_lookup_uint32(table, i));
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, i);
    }
}

/**
 * WHAT: Test integer key edge cases
 * WHY: Verify handling of 0 and max values
 */
TEST_F(HashTableTest, IntegerKey_EdgeCases)
{
    CreateIntegerTable();

    int val1 = 1, val2 = 2;
    ASSERT_TRUE(hash_table_insert_uint32(table, 0, &val1, sizeof(int)));
    ASSERT_TRUE(hash_table_insert_uint32(table, UINT32_MAX, &val2, sizeof(int)));

    int* retrieved1 = static_cast<int*>(hash_table_lookup_uint32(table, 0));
    ASSERT_NE(retrieved1, nullptr);
    EXPECT_EQ(*retrieved1, 1);

    int* retrieved2 = static_cast<int*>(hash_table_lookup_uint32(table, UINT32_MAX));
    ASSERT_NE(retrieved2, nullptr);
    EXPECT_EQ(*retrieved2, 2);
}

//=============================================================================
// Hash Algorithm Tests
//=============================================================================

/**
 * WHAT: Test FNV1A hash algorithm
 * WHY: Verify different hash algorithms work
 */
TEST_F(HashTableTest, HashAlgorithm_FNV1A)
{
    hash_table_config_t config = {.initial_buckets = 16,
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_FNV1A,
                                  .custom_hash_fn = nullptr,
                                  .custom_compare_fn = nullptr,
                                  .value_destructor = nullptr,
                                  .case_insensitive = false,
                                  .thread_safe = false};
    table = hash_table_create(&config);

    int value = 42;
    ASSERT_TRUE(hash_table_insert_string(table, "test", &value, sizeof(int)));

    int* retrieved = static_cast<int*>(hash_table_lookup_string(table, "test"));
    EXPECT_NE(retrieved, nullptr);
}

/**
 * WHAT: Test DJB2 hash algorithm
 * WHY: Verify alternative hash algorithm
 */
TEST_F(HashTableTest, HashAlgorithm_DJB2)
{
    hash_table_config_t config = {.initial_buckets = 16,
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_DJB2,
                                  .custom_hash_fn = nullptr,
                                  .custom_compare_fn = nullptr,
                                  .value_destructor = nullptr,
                                  .case_insensitive = false,
                                  .thread_safe = false};
    table = hash_table_create(&config);

    int value = 42;
    ASSERT_TRUE(hash_table_insert_string(table, "test", &value, sizeof(int)));

    int* retrieved = static_cast<int*>(hash_table_lookup_string(table, "test"));
    EXPECT_NE(retrieved, nullptr);
}

/**
 * WHAT: Test MurmurHash3 algorithm
 * WHY: Verify MurmurHash3 for better distribution
 */
TEST_F(HashTableTest, HashAlgorithm_Murmur3)
{
    hash_table_config_t config = {.initial_buckets = 16,
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_MURMUR3,
                                  .custom_hash_fn = nullptr,
                                  .custom_compare_fn = nullptr,
                                  .value_destructor = nullptr,
                                  .case_insensitive = false,
                                  .thread_safe = false};
    table = hash_table_create(&config);

    int value = 42;
    ASSERT_TRUE(hash_table_insert_string(table, "test", &value, sizeof(int)));

    int* retrieved = static_cast<int*>(hash_table_lookup_string(table, "test"));
    EXPECT_NE(retrieved, nullptr);
}

//=============================================================================
// Clear and Size Tests
//=============================================================================

/**
 * WHAT: Test clear operation
 * WHY: Verify table can be emptied
 */
TEST_F(HashTableTest, Clear_RemovesAllEntries)
{
    CreateDefaultStringTable();

    // Insert multiple entries
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        hash_table_insert_string(table, key, &i, sizeof(int));
    }

    EXPECT_EQ(hash_table_size(table), 10);

    // Clear
    hash_table_clear(table);

    EXPECT_EQ(hash_table_size(table), 0);

    // Verify all keys are gone
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        EXPECT_EQ(hash_table_lookup_string(table, key), nullptr);
    }
}

/**
 * WHAT: Test clear on empty table
 * WHY: Verify clear handles empty table
 */
TEST_F(HashTableTest, Clear_EmptyTable)
{
    CreateDefaultStringTable();

    hash_table_clear(table);  // Should not crash
    EXPECT_EQ(hash_table_size(table), 0);
}

/**
 * WHAT: Test size tracking
 * WHY: Verify size is accurate through operations
 */
TEST_F(HashTableTest, Size_Tracking)
{
    CreateDefaultStringTable();

    EXPECT_EQ(hash_table_size(table), 0);

    int value = 1;
    hash_table_insert_string(table, "key1", &value, sizeof(int));
    EXPECT_EQ(hash_table_size(table), 1);

    hash_table_insert_string(table, "key2", &value, sizeof(int));
    EXPECT_EQ(hash_table_size(table), 2);

    hash_table_remove_string(table, "key1");
    EXPECT_EQ(hash_table_size(table), 1);

    hash_table_clear(table);
    EXPECT_EQ(hash_table_size(table), 0);
}

/**
 * WHAT: Test size with null table
 * WHY: Verify null safety
 */
TEST_F(HashTableTest, Size_NullTable)
{
    EXPECT_EQ(hash_table_size(nullptr), 0);
    EXPECT_EQ(hash_table_bucket_count(nullptr), 0);
}

//=============================================================================
// Iteration Tests
//=============================================================================

// Callback for iteration testing
struct IterationContext {
    int count = 0;
    int sum = 0;
};

static bool iteration_callback(const void* key, size_t key_size, void* value, size_t value_size,
                               void* user_data)
{
    (void) key;
    (void) key_size;
    (void) value_size;

    IterationContext* ctx = static_cast<IterationContext*>(user_data);
    ctx->count++;
    ctx->sum += *static_cast<int*>(value);
    return true;  // Continue iteration
}

/**
 * WHAT: Test iteration over all entries
 * WHY: Verify callback is called for each entry
 */
TEST_F(HashTableTest, Iterate_AllEntries)
{
    CreateDefaultStringTable();

    // Insert entries with known sum
    int sum = 0;
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        hash_table_insert_string(table, key, &i, sizeof(int));
        sum += i;
    }

    IterationContext ctx;
    hash_table_iterate(table, iteration_callback, &ctx);

    EXPECT_EQ(ctx.count, 10);
    EXPECT_EQ(ctx.sum, sum);
}

/**
 * WHAT: Test iteration early termination
 * WHY: Verify iteration can be stopped
 */
TEST_F(HashTableTest, Iterate_EarlyStop)
{
    CreateDefaultStringTable();

    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        hash_table_insert_string(table, key, &i, sizeof(int));
    }

    // Callback that stops after 5 entries
    int count = 0;
    auto stop_callback = [](const void* key, size_t key_size, void* value, size_t value_size,
                            void* user_data) -> bool {
        (void) key;
        (void) key_size;
        (void) value;
        (void) value_size;
        int* count_ptr = static_cast<int*>(user_data);
        (*count_ptr)++;
        return *count_ptr < 5;  // Stop after 5
    };

    hash_table_iterate(table, stop_callback, &count);
    EXPECT_EQ(count, 5);
}

/**
 * WHAT: Test iteration on empty table
 * WHY: Verify iteration handles empty table
 */
TEST_F(HashTableTest, Iterate_EmptyTable)
{
    CreateDefaultStringTable();

    IterationContext ctx;
    hash_table_iterate(table, iteration_callback, &ctx);

    EXPECT_EQ(ctx.count, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test collision statistics
 * WHY: Verify stats are calculated correctly
 */
TEST_F(HashTableTest, Stats_CollisionMetrics)
{
    CreateDefaultStringTable();

    // Insert entries
    for (int i = 0; i < 20; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        hash_table_insert_string(table, key, &i, sizeof(int));
    }

    size_t max_chain, empty_buckets;
    float avg_chain;
    hash_table_stats(table, &max_chain, &avg_chain, &empty_buckets);

    // With 20 entries in 16 buckets, expect some collisions
    EXPECT_GT(max_chain, 0);
    EXPECT_GT(avg_chain, 0.0f);
    EXPECT_LT(empty_buckets, 16);  // Some buckets should be filled
}

/**
 * WHAT: Test stats on empty table
 * WHY: Verify stats handle empty table
 */
TEST_F(HashTableTest, Stats_EmptyTable)
{
    CreateDefaultStringTable();

    size_t max_chain, empty_buckets;
    float avg_chain;
    hash_table_stats(table, &max_chain, &avg_chain, &empty_buckets);

    EXPECT_EQ(max_chain, 0);
    EXPECT_EQ(avg_chain, 0.0f);
    EXPECT_EQ(empty_buckets, 16);
}

/**
 * WHAT: Test stats with null table
 * WHY: Verify null safety
 */
TEST_F(HashTableTest, Stats_NullTable)
{
    size_t max_chain, empty_buckets;
    float avg_chain;

    hash_table_stats(nullptr, &max_chain, &avg_chain, &empty_buckets);

    EXPECT_EQ(max_chain, 0);
    EXPECT_EQ(avg_chain, 0.0f);
    EXPECT_EQ(empty_buckets, 0);
}

//=============================================================================
// Value Destructor Tests
//=============================================================================

// Track destructor calls
struct DestructorContext {
    int* call_count_ptr = nullptr;
};

static void test_destructor(void* value, size_t value_size)
{
    (void) value_size;
    DestructorContext* ctx = static_cast<DestructorContext*>(value);
    if (ctx->call_count_ptr) {
        (*ctx->call_count_ptr)++;
    }
}

/**
 * WHAT: Test value destructor is called on remove
 * WHY: Verify custom cleanup works
 */
TEST_F(HashTableTest, Destructor_OnRemove)
{
    hash_table_config_t config = {.initial_buckets = 16,
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_FNV1A,
                                  .custom_hash_fn = nullptr,
                                  .custom_compare_fn = nullptr,
                                  .value_destructor = test_destructor,
                                  .case_insensitive = false,
                                  .thread_safe = false};
    table = hash_table_create(&config);

    int call_count = 0;
    DestructorContext ctx;
    ctx.call_count_ptr = &call_count;
    hash_table_insert_string(table, "key", &ctx, sizeof(DestructorContext));

    hash_table_remove_string(table, "key");

    // Destructor should have been called once
    EXPECT_EQ(call_count, 1);
}

//=============================================================================
// Collision Handling Tests
//=============================================================================

/**
 * WHAT: Test handling of hash collisions
 * WHY: Verify chaining works correctly
 */
TEST_F(HashTableTest, Collisions_MultipleValues)
{
    // Create small table to force collisions
    hash_table_config_t config = {.initial_buckets = 4,  // Small to force collisions
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_FNV1A,
                                  .custom_hash_fn = nullptr,
                                  .custom_compare_fn = nullptr,
                                  .value_destructor = nullptr,
                                  .case_insensitive = false,
                                  .thread_safe = false};
    table = hash_table_create(&config);

    // Insert many entries
    const int count = 50;
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "collision_key_%d", i);
        ASSERT_TRUE(hash_table_insert_string(table, key, &i, sizeof(int)));
    }

    EXPECT_EQ(hash_table_size(table), count);

    // Verify all entries are retrievable despite collisions
    for (int i = 0; i < count; i++) {
        char key[32];
        snprintf(key, sizeof(key), "collision_key_%d", i);
        int* value = static_cast<int*>(hash_table_lookup_string(table, key));
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, i);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * WHAT: Test with large number of entries
 * WHY: Verify performance and correctness at scale
 */
TEST_F(HashTableTest, Stress_LargeTable)
{
    hash_table_config_t config = {.initial_buckets = 256,
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_MURMUR3,
                                  .custom_hash_fn = nullptr,
                                  .custom_compare_fn = nullptr,
                                  .value_destructor = nullptr,
                                  .case_insensitive = false,
                                  .thread_safe = false};
    table = hash_table_create(&config);

    const int count = 1000;
    for (int i = 0; i < count; i++) {
        char key[64];
        snprintf(key, sizeof(key), "large_table_key_%d", i);
        ASSERT_TRUE(hash_table_insert_string(table, key, &i, sizeof(int)));
    }

    EXPECT_EQ(hash_table_size(table), count);

    // Verify random access
    for (int i = 0; i < count; i += 10) {
        char key[64];
        snprintf(key, sizeof(key), "large_table_key_%d", i);
        int* value = static_cast<int*>(hash_table_lookup_string(table, key));
        ASSERT_NE(value, nullptr);
        EXPECT_EQ(*value, i);
    }
}
