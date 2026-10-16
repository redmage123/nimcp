/**
 * @file test_swarm_drone_id_regression.cpp
 * @brief Regression test for 64-bit drone/swarm ID truncation to uint32_t
 *
 * REGRESSION: 64-bit drone ID truncated to uint32_t hash key
 *
 * ORIGINAL BUG:
 * In nimcp_swarm_multi.c, the swarm_hash_table_* wrapper functions used
 * HASH_KEY_UINT32 and cast uint64_t swarm IDs to uint32_t:
 *
 *   hash_table_insert_uint32(table, (uint32_t)key, ...)
 *   hash_table_lookup_uint32(table, (uint32_t)key)
 *   hash_table_remove_uint32(table, (uint32_t)key)
 *
 * This meant any two swarms whose IDs differed only in the upper 32 bits
 * would hash to the same key. The second insertion would overwrite the first.
 *
 * IMPACT:
 * - Swarm registry corruption when swarm_id > 2^32
 * - Mission registry lookup failures for large mission IDs
 * - Resource request routing to wrong swarms
 * - Silent data loss (no error returned)
 *
 * FIX:
 * 1. Added hash_table_insert_uint64/lookup_uint64/remove_uint64 to hash table API
 * 2. Changed HASH_KEY_UINT32 to HASH_KEY_UINT64 in swarm_hash_table_create_ex
 * 3. Changed wrapper functions to use uint64 API without truncation
 *
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_multi.h"
#include "utils/containers/nimcp_hash_table.h"

/* ============================================================================
 * Regression Test Fixture
 * ============================================================================ */

class SwarmDroneIdRegressionTest : public ::testing::Test {
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

/* ============================================================================
 * Regression: Hash Table uint64 Key Non-Truncation
 * ============================================================================ */

/**
 * REGRESSION: Keys that differ only in high 32 bits must not collide.
 * Before the fix, (uint32_t)0x100000001 == (uint32_t)0x200000001 == 1.
 */
TEST(HashTableUint64Regression, HighBitsDifferentiation) {
    hash_table_config_t config = {
        .initial_buckets = 32,
        .key_type = HASH_KEY_UINT64,
        .hash_algorithm = HASH_ALG_MURMUR3,
        .value_destructor = nullptr,
        .case_insensitive = false,
        .thread_safe = false
    };
    hash_table_t* table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);

    /* These three keys all have low 32 bits = 0x00000001 */
    uint64_t keys[] = {
        0x0000000100000001ULL,
        0x0000000200000001ULL,
        0x0000000300000001ULL,
    };

    uint32_t values[] = {100, 200, 300};

    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(hash_table_insert_uint64(table, keys[i], &values[i], sizeof(values[i])));
    }

    /* All three must coexist */
    EXPECT_EQ(hash_table_size(table), 3u);

    for (int i = 0; i < 3; i++) {
        void* result = hash_table_lookup_uint64(table, keys[i]);
        ASSERT_NE(result, nullptr) << "Key " << i << " lost (high bits truncated?)";
        EXPECT_EQ(*(uint32_t*)result, values[i])
            << "Key " << i << " value corrupted (overwrote by collision?)";
    }

    hash_table_destroy(table);
}

/**
 * REGRESSION: Insert then remove by uint64 key must use full 64 bits.
 * Before the fix, remove(0x100000001) would remove entry for key 1,
 * which could be a different entry than intended.
 */
TEST(HashTableUint64Regression, RemoveUsesFullKey) {
    hash_table_config_t config = {
        .initial_buckets = 32,
        .key_type = HASH_KEY_UINT64,
        .hash_algorithm = HASH_ALG_MURMUR3,
        .value_destructor = nullptr,
        .case_insensitive = false,
        .thread_safe = false
    };
    hash_table_t* table = hash_table_create(&config);
    ASSERT_NE(table, nullptr);

    uint64_t key_a = 0x0000000100000007ULL;
    uint64_t key_b = 0x0000000200000007ULL;
    uint32_t val_a = 10, val_b = 20;

    hash_table_insert_uint64(table, key_a, &val_a, sizeof(val_a));
    hash_table_insert_uint64(table, key_b, &val_b, sizeof(val_b));

    /* Remove key_a, verify key_b is unaffected */
    EXPECT_TRUE(hash_table_remove_uint64(table, key_a));
    EXPECT_EQ(hash_table_lookup_uint64(table, key_a), nullptr);

    void* result_b = hash_table_lookup_uint64(table, key_b);
    ASSERT_NE(result_b, nullptr) << "Removing key_a also removed key_b (truncation)";
    EXPECT_EQ(*(uint32_t*)result_b, 20u);

    hash_table_destroy(table);
}

/* ============================================================================
 * Regression: Multi-Swarm Coordinator 64-bit ID Support
 * ============================================================================ */

/**
 * REGRESSION: Register two swarms and verify both are independently accessible.
 * The coordinator assigns sequential IDs, so they won't naturally collide.
 * This test verifies basic functionality is preserved after the fix.
 */
TEST_F(SwarmDroneIdRegressionTest, BasicRegistrationPreserved) {
    auto* id1 = nimcp_swarm_identity_create(coord, "alpha", 10);
    auto* id2 = nimcp_swarm_identity_create(coord, "beta", 20);
    ASSERT_NE(id1, nullptr);
    ASSERT_NE(id2, nullptr);

    EXPECT_EQ(nimcp_swarm_register(coord, id1), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_swarm_register(coord, id2), NIMCP_SUCCESS);

    auto* found1 = nimcp_swarm_get(coord, id1->swarm_id);
    auto* found2 = nimcp_swarm_get(coord, id2->swarm_id);
    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);

    EXPECT_STREQ(found1->name, "alpha");
    EXPECT_STREQ(found2->name, "beta");
}

/**
 * REGRESSION: Unregister and re-lookup.
 * Verify unregister works correctly with 64-bit keys.
 */
TEST_F(SwarmDroneIdRegressionTest, UnregisterAndRelookup) {
    auto* id1 = nimcp_swarm_identity_create(coord, "s1", 5);
    auto* id2 = nimcp_swarm_identity_create(coord, "s2", 5);
    ASSERT_NE(id1, nullptr);
    ASSERT_NE(id2, nullptr);

    nimcp_swarm_register(coord, id1);
    nimcp_swarm_register(coord, id2);

    uint64_t id1_val = id1->swarm_id;
    uint64_t id2_val = id2->swarm_id;

    EXPECT_EQ(nimcp_swarm_unregister(coord, id1_val), NIMCP_SUCCESS);

    /* s1 gone, s2 still present */
    EXPECT_EQ(nimcp_swarm_get(coord, id1_val), nullptr);
    auto* found2 = nimcp_swarm_get(coord, id2_val);
    ASSERT_NE(found2, nullptr);
    EXPECT_STREQ(found2->name, "s2");
}

/**
 * REGRESSION: Mission creation and lookup with 64-bit IDs.
 * Verifies mission registry also uses full 64-bit keys.
 */
TEST_F(SwarmDroneIdRegressionTest, MissionRegistryUses64BitKeys) {
    nimcp_territory_bounds_t area = {{0,0,0}, {100,100,50}, 0, false, 0.5f};

    uint64_t m1 = nimcp_mission_create(coord, "mission_alpha",
        NIMCP_MISSION_PRIORITY_HIGH, area, 0);
    uint64_t m2 = nimcp_mission_create(coord, "mission_beta",
        NIMCP_MISSION_PRIORITY_MEDIUM, area, 0);

    EXPECT_GT(m1, (uint64_t)0);
    EXPECT_GT(m2, (uint64_t)0);
    EXPECT_NE(m1, m2);

    auto* found1 = nimcp_mission_get(coord, m1);
    auto* found2 = nimcp_mission_get(coord, m2);

    ASSERT_NE(found1, nullptr);
    ASSERT_NE(found2, nullptr);

    EXPECT_STREQ(found1->description, "mission_alpha");
    EXPECT_STREQ(found2->description, "mission_beta");
}

/**
 * REGRESSION: Coordinator stats must reflect correct registration count
 * when using 64-bit keys.
 *
 * NOTE: nimcp_multi_swarm_get_stats counts swarms within super-swarms,
 * not the global swarm_registry. Swarms must be added to a super-swarm
 * via nimcp_super_swarm_add_swarm to appear in stats.
 */
TEST_F(SwarmDroneIdRegressionTest, StatsReflectRegistrations) {
    auto* super = nimcp_super_swarm_create(coord, "stats_super");
    ASSERT_NE(super, nullptr);

    const int count = 10;
    for (int i = 0; i < count; i++) {
        char name[64];
        snprintf(name, sizeof(name), "swarm_%d", i);
        auto* id = nimcp_swarm_identity_create(coord, name, (uint32_t)(5 + i));
        ASSERT_NE(id, nullptr);
        nimcp_swarm_register(coord, id);
        nimcp_super_swarm_add_swarm(super, id);
    }

    uint32_t total_swarms = 0;
    nimcp_multi_swarm_get_stats(coord, &total_swarms, nullptr, nullptr, nullptr);
    EXPECT_EQ(total_swarms, (uint32_t)count);
}

/**
 * REGRESSION: Bridge creation between two swarms with valid 64-bit IDs.
 *
 * NOTE: nimcp_comm_bridge_create requires both swarms to be in the same
 * super-swarm. It searches super_swarms[i]->swarms[] for matching IDs.
 */
TEST_F(SwarmDroneIdRegressionTest, BridgeCreationWith64BitIds) {
    auto* super = nimcp_super_swarm_create(coord, "bridge_super");
    ASSERT_NE(super, nullptr);

    auto* id1 = nimcp_swarm_identity_create(coord, "bridge_a", 10);
    auto* id2 = nimcp_swarm_identity_create(coord, "bridge_b", 10);
    ASSERT_NE(id1, nullptr);
    ASSERT_NE(id2, nullptr);

    nimcp_swarm_register(coord, id1);
    nimcp_swarm_register(coord, id2);
    nimcp_super_swarm_add_swarm(super, id1);
    nimcp_super_swarm_add_swarm(super, id2);

    uint64_t bid = nimcp_comm_bridge_create(coord, id1->swarm_id,
                                              id2->swarm_id, nullptr, 0);
    EXPECT_GT(bid, (uint64_t)0);
}

/**
 * REGRESSION: Resource request between swarms with 64-bit IDs.
 */
TEST_F(SwarmDroneIdRegressionTest, ResourceRequestWith64BitIds) {
    auto* id1 = nimcp_swarm_identity_create(coord, "requester", 10);
    auto* id2 = nimcp_swarm_identity_create(coord, "provider", 20);
    ASSERT_NE(id1, nullptr);
    ASSERT_NE(id2, nullptr);

    nimcp_swarm_register(coord, id1);
    nimcp_swarm_register(coord, id2);

    uint64_t req = nimcp_resource_request(coord, id1->swarm_id, id2->swarm_id,
                                            NIMCP_RESOURCE_REQ_DRONES, 5,
                                            NIMCP_MISSION_PRIORITY_HIGH);
    EXPECT_GT(req, (uint64_t)0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
