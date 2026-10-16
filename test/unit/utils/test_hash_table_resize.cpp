/**
 * @file test_hash_table_resize.cpp
 * @brief Unit tests for hash table dynamic resizing (Tier 4 improvement H11)
 *
 * WHAT: Verify hash table auto-resizes when load factor exceeds 0.75
 * WHY: Without resizing, 256-bucket table degrades to O(n) at >200 entries
 * HOW: Insert large numbers of entries, verify bucket count grows and lookups succeed
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <chrono>

extern "C" {
#include "utils/containers/nimcp_hash_table.h"
}

#include "utils/nimcp_test_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class HashTableResizeTest : public NimcpTestBase {
protected:
    hash_table_t* table = nullptr;

    void TearDown() override {
        if (table) {
            hash_table_destroy(table);
            table = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Test 1: Resize triggers when load factor exceeds 0.75
//=============================================================================

TEST_F(HashTableResizeTest, ResizeTriggersOnHighLoadFactor) {
    // WHAT: Insert enough entries to trigger resize
    // WHY: Verify automatic resize happens at load factor > 0.75

    hash_table_config_t config = {};
    config.initial_buckets = 64;
    config.key_type = HASH_KEY_STRING;
    config.hash_algorithm = HASH_ALG_FNV1A;

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(hash_table_bucket_count(table), 64u);

    // Insert 49 entries (49/64 = 0.766 > 0.75) to trigger resize
    for (int i = 0; i < 49; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%04d", i);
        int value = i;
        EXPECT_TRUE(hash_table_insert_string(table, key, &value, sizeof(value)));
    }

    // Bucket count should have doubled (at least once)
    EXPECT_GT(hash_table_bucket_count(table), 64u);
    EXPECT_EQ(hash_table_size(table), 49u);
}

//=============================================================================
// Test 2: All entries survive resize
//=============================================================================

TEST_F(HashTableResizeTest, NoEntriesLostDuringResize) {
    // WHAT: Insert 1000+ entries and verify all are retrievable
    // WHY: Resize must rehash all entries correctly

    hash_table_config_t config = {};
    config.initial_buckets = 32;  // Small initial to force many resizes
    config.key_type = HASH_KEY_STRING;
    config.hash_algorithm = HASH_ALG_FNV1A;

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);

    const int NUM_ENTRIES = 1500;

    // Insert all entries
    for (int i = 0; i < NUM_ENTRIES; i++) {
        char key[64];
        snprintf(key, sizeof(key), "entry_%06d", i);
        EXPECT_TRUE(hash_table_insert_string(table, key, &i, sizeof(i)));
    }

    EXPECT_EQ(hash_table_size(table), (size_t)NUM_ENTRIES);

    // Verify EVERY entry is still retrievable
    for (int i = 0; i < NUM_ENTRIES; i++) {
        char key[64];
        snprintf(key, sizeof(key), "entry_%06d", i);
        int* val = (int*)hash_table_lookup_string(table, key);
        ASSERT_NE(val, nullptr) << "Entry missing after resize: " << key;
        EXPECT_EQ(*val, i) << "Wrong value for key: " << key;
    }

    // Should have resized multiple times from 32 buckets
    // 32 -> 64 -> 128 -> 256 -> 512 -> 1024 -> 2048
    EXPECT_GE(hash_table_bucket_count(table), 2048u);
}

//=============================================================================
// Test 3: O(1) lookup performance after resize
//=============================================================================

TEST_F(HashTableResizeTest, LookupPerformanceWithResize) {
    // WHAT: Verify lookup performance stays reasonable with 2000 entries
    // WHY: Dynamic resizing should maintain O(1) average case

    table = hash_table_create(nullptr);  // Default 256 buckets
    ASSERT_NE(table, nullptr);

    const int NUM_ENTRIES = 2000;

    // Insert entries
    for (int i = 0; i < NUM_ENTRIES; i++) {
        char key[64];
        snprintf(key, sizeof(key), "perf_key_%06d", i);
        EXPECT_TRUE(hash_table_insert_string(table, key, &i, sizeof(i)));
    }

    // Measure lookup time for 2000 lookups
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ENTRIES; i++) {
        char key[64];
        snprintf(key, sizeof(key), "perf_key_%06d", i);
        int* val = (int*)hash_table_lookup_string(table, key);
        ASSERT_NE(val, nullptr);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // With resizing, 2000 lookups should complete in well under 100ms
    // Without resizing (256 buckets, avg chain ~8), it would be much slower
    EXPECT_LT(duration_us, 100000) << "Lookup too slow: " << duration_us << " us for " << NUM_ENTRIES << " lookups";

    // Check chain statistics — max chain should be reasonable
    size_t max_chain = 0;
    float avg_chain = 0.0f;
    size_t empty_buckets = 0;
    hash_table_stats(table, &max_chain, &avg_chain, &empty_buckets);

    // With good resizing, avg chain should be near 1.0
    EXPECT_LT(avg_chain, 3.0f) << "Average chain length too high after resize";
    EXPECT_LT(max_chain, 10u) << "Max chain length too high after resize";
}

//=============================================================================
// Test 4: Integer keys survive resize
//=============================================================================

TEST_F(HashTableResizeTest, IntegerKeysSurviveResize) {
    // WHAT: Verify uint32 keys work correctly through resize
    // WHY: Resize must work for all key types

    hash_table_config_t config = {};
    config.initial_buckets = 16;
    config.key_type = HASH_KEY_UINT32;
    config.hash_algorithm = HASH_ALG_MURMUR3;

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);

    const int NUM_ENTRIES = 500;

    for (int i = 0; i < NUM_ENTRIES; i++) {
        uint32_t key = (uint32_t)i;
        EXPECT_TRUE(hash_table_insert_uint32(table, key, &i, sizeof(i)));
    }

    EXPECT_EQ(hash_table_size(table), (size_t)NUM_ENTRIES);

    // Verify all entries
    for (int i = 0; i < NUM_ENTRIES; i++) {
        int* val = (int*)hash_table_lookup_uint32(table, (uint32_t)i);
        ASSERT_NE(val, nullptr) << "Missing uint32 key: " << i;
        EXPECT_EQ(*val, i);
    }

    // Should have resized from 16 buckets
    EXPECT_GT(hash_table_bucket_count(table), 16u);
}

//=============================================================================
// Test 5: Bucket count doubles correctly
//=============================================================================

TEST_F(HashTableResizeTest, BucketCountDoublesOnResize) {
    // WHAT: Verify bucket count exactly doubles each time
    // WHY: Implementation should double, not use other growth factor

    hash_table_config_t config = {};
    config.initial_buckets = 8;
    config.key_type = HASH_KEY_STRING;
    config.hash_algorithm = HASH_ALG_FNV1A;

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(hash_table_bucket_count(table), 8u);

    // Insert 7 entries (7/8 = 0.875 > 0.75) — should trigger resize to 16
    for (int i = 0; i < 7; i++) {
        char key[32];
        snprintf(key, sizeof(key), "k%d", i);
        int value = i;
        hash_table_insert_string(table, key, &value, sizeof(value));
    }

    EXPECT_EQ(hash_table_bucket_count(table), 16u);

    // Insert more to trigger 16 -> 32 (need 13 entries: 13/16 = 0.8125)
    for (int i = 7; i < 13; i++) {
        char key[32];
        snprintf(key, sizeof(key), "k%d", i);
        int value = i;
        hash_table_insert_string(table, key, &value, sizeof(value));
    }

    EXPECT_EQ(hash_table_bucket_count(table), 32u);
}

//=============================================================================
// Test 6: Remove after resize
//=============================================================================

TEST_F(HashTableResizeTest, RemoveAfterResize) {
    // WHAT: Remove entries after table has been resized
    // WHY: Removal must work correctly with new bucket layout

    hash_table_config_t config = {};
    config.initial_buckets = 16;
    config.key_type = HASH_KEY_STRING;
    config.hash_algorithm = HASH_ALG_FNV1A;

    table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);

    // Insert enough to trigger resize
    const int NUM_ENTRIES = 100;
    for (int i = 0; i < NUM_ENTRIES; i++) {
        char key[32];
        snprintf(key, sizeof(key), "rm_%d", i);
        int value = i;
        hash_table_insert_string(table, key, &value, sizeof(value));
    }

    EXPECT_GT(hash_table_bucket_count(table), 16u);

    // Remove half the entries
    for (int i = 0; i < NUM_ENTRIES; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "rm_%d", i);
        EXPECT_TRUE(hash_table_remove_string(table, key));
    }

    EXPECT_EQ(hash_table_size(table), (size_t)(NUM_ENTRIES / 2));

    // Verify remaining entries
    for (int i = 1; i < NUM_ENTRIES; i += 2) {
        char key[32];
        snprintf(key, sizeof(key), "rm_%d", i);
        int* val = (int*)hash_table_lookup_string(table, key);
        ASSERT_NE(val, nullptr) << "Missing after remove: " << key;
        EXPECT_EQ(*val, i);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
