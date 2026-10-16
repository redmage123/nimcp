/**
 * @file test_swarm_drone_id_hash.cpp
 * @brief Unit tests for 64-bit drone/swarm ID hash table handling
 *
 * TEST COVERAGE:
 * - Verify 64-bit swarm IDs are stored without truncation
 * - Test IDs that differ only in the high 32 bits
 * - Test IDs that share the same low 32 bits
 * - Test registration, lookup, and unregistration with 64-bit IDs
 * - Test hash_table_insert_uint64 / lookup_uint64 / remove_uint64 directly
 *
 * WHAT WAS FIXED:
 * In nimcp_swarm_multi.c, the swarm_hash_table_* wrapper functions used
 * HASH_KEY_UINT32 and cast 64-bit swarm/drone IDs to uint32_t, which
 * caused different swarms whose IDs differ only in the upper 32 bits to
 * collide and overwrite each other's entries. For example:
 *   swarm_id=0x100000001 and swarm_id=0x200000001 both map to hash key 1.
 *
 * The fix changed HASH_KEY_UINT32 to HASH_KEY_UINT64 and replaced
 * hash_table_insert_uint32/lookup_uint32/remove_uint32 calls with
 * the new hash_table_insert_uint64/lookup_uint64/remove_uint64 functions.
 *
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_multi.h"
#include "utils/containers/nimcp_hash_table.h"

/* ============================================================================
 * Hash Table uint64 Direct Tests
 * ============================================================================ */

class HashTableUint64Test : public ::testing::Test {
protected:
    hash_table_t* table;

    void SetUp() override {
        hash_table_config_t config = {
            .initial_buckets = 64,
            .key_type = HASH_KEY_UINT64,
            .hash_algorithm = HASH_ALG_MURMUR3,
            .value_destructor = nullptr,
            .case_insensitive = false,
            .thread_safe = false
        };
        table = hash_table_create(&config);
        ASSERT_NE(table, nullptr);
    }

    void TearDown() override {
        if (table) {
            hash_table_destroy(table);
        }
    }
};

/**
 * Basic insert and lookup with uint64 key
 */
TEST_F(HashTableUint64Test, InsertAndLookup) {
    uint64_t key = 0xDEADBEEFCAFEBABEULL;
    uint32_t value = 42;

    bool inserted = hash_table_insert_uint64(table, key, &value, sizeof(value));
    EXPECT_TRUE(inserted);

    void* result = hash_table_lookup_uint64(table, key);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*(uint32_t*)result, 42u);
}

/**
 * Two keys that share the same low 32 bits but differ in high 32 bits.
 * Before the fix, these would collide because the high bits were truncated.
 */
TEST_F(HashTableUint64Test, KeysDifferOnlyInHighBits) {
    uint64_t key_a = 0x0000000100000001ULL;  /* low 32 bits = 1 */
    uint64_t key_b = 0x0000000200000001ULL;  /* low 32 bits = 1, high differs */

    uint32_t val_a = 111;
    uint32_t val_b = 222;

    EXPECT_TRUE(hash_table_insert_uint64(table, key_a, &val_a, sizeof(val_a)));
    EXPECT_TRUE(hash_table_insert_uint64(table, key_b, &val_b, sizeof(val_b)));

    /* Both must be independently retrievable */
    void* result_a = hash_table_lookup_uint64(table, key_a);
    void* result_b = hash_table_lookup_uint64(table, key_b);

    ASSERT_NE(result_a, nullptr) << "key_a lookup returned NULL - high bits lost";
    ASSERT_NE(result_b, nullptr) << "key_b lookup returned NULL - high bits lost";

    EXPECT_EQ(*(uint32_t*)result_a, 111u) << "key_a value was overwritten by key_b";
    EXPECT_EQ(*(uint32_t*)result_b, 222u) << "key_b value is wrong";
}

/**
 * Multiple keys with identical low 32 bits but all unique high 32 bits
 */
TEST_F(HashTableUint64Test, ManyKeysWithSameLow32) {
    const uint32_t count = 16;
    uint32_t values[count];

    for (uint32_t i = 0; i < count; i++) {
        uint64_t key = ((uint64_t)(i + 1) << 32) | 0x00000042ULL;
        values[i] = 1000 + i;
        EXPECT_TRUE(hash_table_insert_uint64(table, key, &values[i], sizeof(values[i])));
    }

    /* All 16 entries must be independently retrievable */
    EXPECT_EQ(hash_table_size(table), (size_t)count);

    for (uint32_t i = 0; i < count; i++) {
        uint64_t key = ((uint64_t)(i + 1) << 32) | 0x00000042ULL;
        void* result = hash_table_lookup_uint64(table, key);
        ASSERT_NE(result, nullptr) << "Key " << i << " lost";
        EXPECT_EQ(*(uint32_t*)result, 1000 + i);
    }
}

/**
 * Remove by uint64 key
 */
TEST_F(HashTableUint64Test, RemoveUint64) {
    uint64_t key = 0xAAAABBBBCCCCDDDDULL;
    uint32_t value = 99;

    hash_table_insert_uint64(table, key, &value, sizeof(value));
    EXPECT_NE(hash_table_lookup_uint64(table, key), nullptr);

    bool removed = hash_table_remove_uint64(table, key);
    EXPECT_TRUE(removed);

    EXPECT_EQ(hash_table_lookup_uint64(table, key), nullptr);
}

/**
 * Remove one of two keys that share the same low 32 bits
 */
TEST_F(HashTableUint64Test, RemoveOneOfCollision) {
    uint64_t key_a = 0x0000000100000005ULL;
    uint64_t key_b = 0x0000000200000005ULL;

    uint32_t val_a = 10, val_b = 20;

    hash_table_insert_uint64(table, key_a, &val_a, sizeof(val_a));
    hash_table_insert_uint64(table, key_b, &val_b, sizeof(val_b));

    /* Remove key_a, key_b should still be present */
    EXPECT_TRUE(hash_table_remove_uint64(table, key_a));

    EXPECT_EQ(hash_table_lookup_uint64(table, key_a), nullptr);
    void* result_b = hash_table_lookup_uint64(table, key_b);
    ASSERT_NE(result_b, nullptr);
    EXPECT_EQ(*(uint32_t*)result_b, 20u);
}

/**
 * Edge case: key = 0 should work
 */
TEST_F(HashTableUint64Test, ZeroKey) {
    uint64_t key = 0;
    uint32_t value = 77;

    EXPECT_TRUE(hash_table_insert_uint64(table, key, &value, sizeof(value)));
    void* result = hash_table_lookup_uint64(table, key);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*(uint32_t*)result, 77u);
}

/**
 * Edge case: key = UINT64_MAX
 */
TEST_F(HashTableUint64Test, MaxKey) {
    uint64_t key = UINT64_MAX;
    uint32_t value = 88;

    EXPECT_TRUE(hash_table_insert_uint64(table, key, &value, sizeof(value)));
    void* result = hash_table_lookup_uint64(table, key);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*(uint32_t*)result, 88u);
}

/* ============================================================================
 * Multi-Swarm Coordinator Swarm ID Tests (Integration-Level)
 * ============================================================================ */

class SwarmDroneIdTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coord;

    void SetUp() override {
        coord = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coord, nullptr);
    }

    void TearDown() override {
        if (coord) {
            nimcp_multi_swarm_destroy(coord);
        }
    }
};

/**
 * Register two swarms and verify each gets a unique 64-bit ID that
 * can be looked up without collision
 */
TEST_F(SwarmDroneIdTest, RegisterAndLookupByFullId) {
    auto* id1 = nimcp_swarm_identity_create(coord, "swarm_alpha", 10);
    auto* id2 = nimcp_swarm_identity_create(coord, "swarm_beta", 20);
    ASSERT_NE(id1, nullptr);
    ASSERT_NE(id2, nullptr);

    EXPECT_EQ(nimcp_swarm_register(coord, id1), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_swarm_register(coord, id2), NIMCP_SUCCESS);

    /* Look up each swarm by its full ID */
    auto* found1 = nimcp_swarm_get(coord, id1->swarm_id);
    auto* found2 = nimcp_swarm_get(coord, id2->swarm_id);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);

    EXPECT_STREQ(found1->name, "swarm_alpha");
    EXPECT_STREQ(found2->name, "swarm_beta");
}

/**
 * Verify that unregistering one swarm does not affect another
 */
TEST_F(SwarmDroneIdTest, UnregisterDoesNotAffectOther) {
    auto* id1 = nimcp_swarm_identity_create(coord, "s1", 10);
    auto* id2 = nimcp_swarm_identity_create(coord, "s2", 10);
    ASSERT_NE(id1, nullptr);
    ASSERT_NE(id2, nullptr);

    nimcp_swarm_register(coord, id1);
    nimcp_swarm_register(coord, id2);

    uint64_t s1_id = id1->swarm_id;
    uint64_t s2_id = id2->swarm_id;

    /* Unregister s1 */
    EXPECT_EQ(nimcp_swarm_unregister(coord, s1_id), NIMCP_SUCCESS);

    /* s1 should be gone, s2 should remain */
    EXPECT_EQ(nimcp_swarm_get(coord, s1_id), nullptr);
    EXPECT_NE(nimcp_swarm_get(coord, s2_id), nullptr);
}

/**
 * Register many swarms and verify all are independently accessible
 */
TEST_F(SwarmDroneIdTest, ManySwarmRegistrations) {
    const int count = 20;
    std::vector<uint64_t> ids;

    for (int i = 0; i < count; i++) {
        char name[64];
        snprintf(name, sizeof(name), "swarm_%d", i);
        auto* identity = nimcp_swarm_identity_create(coord, name, (uint32_t)(5 + i));
        ASSERT_NE(identity, nullptr);
        nimcp_swarm_register(coord, identity);
        ids.push_back(identity->swarm_id);
    }

    /* All 20 must be retrievable */
    for (int i = 0; i < count; i++) {
        auto* found = nimcp_swarm_get(coord, ids[i]);
        ASSERT_NE(found, nullptr) << "Swarm " << i << " (id=" << ids[i] << ") not found";
        char expected[64];
        snprintf(expected, sizeof(expected), "swarm_%d", i);
        EXPECT_STREQ(found->name, expected);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
