/**
 * @file test_swarm_lifecycle.cpp
 * @brief Unit tests for swarm peer registry lifecycle — create, add, remove, sweep.
 *
 * WHAT: Test peer registry CRUD, heartbeat tracking, state transitions,
 *       dead peer sweep, and NULL safety.
 * WHY:  The peer registry is the foundation of the swarm runtime; bugs here
 *       cause devices to drop from the swarm silently.
 * HOW:  Google Test, forward-declare internal lifecycle functions.
 *
 * NOTE: nimcp_swarm_lifecycle.c defines its own local types for peer_entry
 *       and peer_registry. We forward-declare the functions and use opaque
 *       pointers where possible, or include the header types for compatible
 *       subset access.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_swarm_runtime_types.h"

/* Forward-declare internal lifecycle functions.
 * These are defined in nimcp_swarm_lifecycle.c but not in a public header.
 * The swarm_runtime_types.h defines nimcp_peer_registry_t and
 * nimcp_peer_entry_t which we use here. The lifecycle .c file defines
 * its own local versions, but since these are passed as pointers,
 * the linker resolves them by name. We must ensure the types used here
 * are layout-compatible with the .c file's local types for the fields
 * we access. */
nimcp_peer_registry_t*  nimcp_peer_registry_create(uint32_t capacity);
void                    nimcp_peer_registry_destroy(nimcp_peer_registry_t* registry);
int                     nimcp_peer_registry_add(nimcp_peer_registry_t* registry,
                            uint32_t device_id, const char* address,
                            uint16_t port, const nimcp_device_profile_t* profile);
int                     nimcp_peer_registry_remove(nimcp_peer_registry_t* registry,
                            uint32_t device_id);
nimcp_peer_entry_t*     nimcp_peer_registry_find(nimcp_peer_registry_t* registry,
                            uint32_t device_id);
int                     nimcp_peer_registry_update_heartbeat(
                            nimcp_peer_registry_t* registry, uint32_t device_id);
int                     nimcp_peer_registry_set_state(
                            nimcp_peer_registry_t* registry,
                            uint32_t device_id, nimcp_peer_state_t new_state);
uint32_t                nimcp_peer_registry_sweep(nimcp_peer_registry_t* registry,
                            uint32_t heartbeat_timeout_ms,
                            uint32_t dead_timeout_ms);
uint32_t                nimcp_peer_registry_get_active_count(
                            nimcp_peer_registry_t* registry);
uint32_t                nimcp_peer_registry_get_active_peers(
                            nimcp_peer_registry_t* registry,
                            uint32_t* out_ids, uint32_t max_count);
const char*             nimcp_peer_state_name(nimcp_peer_state_t state);
}

// ============================================================================
// Registry Lifecycle
// ============================================================================

TEST(SwarmLifecycle, CreateDestroyRegistry) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(16);
    ASSERT_NE(reg, nullptr);
    EXPECT_EQ(reg->count, 0u);
    EXPECT_EQ(reg->capacity, 16u);
    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, DestroyNull) {
    nimcp_peer_registry_destroy(NULL);
    SUCCEED() << "nimcp_peer_registry_destroy(NULL) did not crash";
}

TEST(SwarmLifecycle, CreateZeroCapacity) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(0);
    EXPECT_EQ(reg, nullptr) << "Zero capacity should return NULL";
}

// ============================================================================
// Add / Find / Remove
// ============================================================================

TEST(SwarmLifecycle, AddPeer) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    int rc = nimcp_peer_registry_add(reg, 100, "192.168.1.10", 9421, NULL);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(reg->count, 1u);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, FindPeer) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_registry_add(reg, 42, "10.0.0.1", 9421, NULL);

    nimcp_peer_entry_t* entry = nimcp_peer_registry_find(reg, 42);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->device_id, 42u);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, FindNonexistent) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_entry_t* entry = nimcp_peer_registry_find(reg, 999);
    EXPECT_EQ(entry, nullptr);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, RemovePeer) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_registry_add(reg, 1, "1.1.1.1", 9421, NULL);
    nimcp_peer_registry_add(reg, 2, "2.2.2.2", 9421, NULL);
    EXPECT_EQ(reg->count, 2u);

    int rc = nimcp_peer_registry_remove(reg, 1);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(reg->count, 1u);

    // Verify peer 2 is still there
    nimcp_peer_entry_t* entry = nimcp_peer_registry_find(reg, 2);
    EXPECT_NE(entry, nullptr);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, RemoveNonexistent) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    int rc = nimcp_peer_registry_remove(reg, 999);
    EXPECT_LT(rc, 0);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, DuplicateDeviceId) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_registry_add(reg, 1, "1.1.1.1", 9421, NULL);
    int rc = nimcp_peer_registry_add(reg, 1, "2.2.2.2", 9422, NULL);
    EXPECT_LT(rc, 0) << "Duplicate device_id should be rejected";
    EXPECT_EQ(reg->count, 1u);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, CapacityEnforcement) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(2);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_registry_add(reg, 1, "1.1.1.1", 9421, NULL);
    nimcp_peer_registry_add(reg, 2, "2.2.2.2", 9421, NULL);
    int rc = nimcp_peer_registry_add(reg, 3, "3.3.3.3", 9421, NULL);
    EXPECT_LT(rc, 0) << "Over-capacity add should fail";
    EXPECT_EQ(reg->count, 2u);

    nimcp_peer_registry_destroy(reg);
}

// ============================================================================
// Heartbeat
// ============================================================================

TEST(SwarmLifecycle, HeartbeatUpdate) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_registry_add(reg, 1, "1.1.1.1", 9421, NULL);
    int rc = nimcp_peer_registry_update_heartbeat(reg, 1);
    EXPECT_EQ(rc, 0);

    nimcp_peer_entry_t* entry = nimcp_peer_registry_find(reg, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->missed_heartbeats, 0u);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, HeartbeatUnknownPeer) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    int rc = nimcp_peer_registry_update_heartbeat(reg, 999);
    EXPECT_LT(rc, 0);

    nimcp_peer_registry_destroy(reg);
}

// ============================================================================
// State Transitions
// ============================================================================

TEST(SwarmLifecycle, SetState) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_registry_add(reg, 1, "1.1.1.1", 9421, NULL);
    int rc = nimcp_peer_registry_set_state(reg, 1, NIMCP_PEER_ACTIVE);
    EXPECT_EQ(rc, 0);

    nimcp_peer_entry_t* entry = nimcp_peer_registry_find(reg, 1);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, NIMCP_PEER_ACTIVE);

    nimcp_peer_registry_destroy(reg);
}

// ============================================================================
// Active Count / Peers
// ============================================================================

TEST(SwarmLifecycle, GetActiveCount) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_registry_add(reg, 1, "1.1.1.1", 9421, NULL);
    nimcp_peer_registry_add(reg, 2, "2.2.2.2", 9421, NULL);
    nimcp_peer_registry_set_state(reg, 1, NIMCP_PEER_ACTIVE);
    nimcp_peer_registry_set_state(reg, 2, NIMCP_PEER_ACTIVE);

    uint32_t active = nimcp_peer_registry_get_active_count(reg);
    EXPECT_EQ(active, 2u);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, GetActivePeers) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(8);
    ASSERT_NE(reg, nullptr);

    nimcp_peer_registry_add(reg, 10, "1.1.1.1", 9421, NULL);
    nimcp_peer_registry_add(reg, 20, "2.2.2.2", 9421, NULL);
    nimcp_peer_registry_add(reg, 30, "3.3.3.3", 9421, NULL);
    nimcp_peer_registry_set_state(reg, 10, NIMCP_PEER_ACTIVE);
    nimcp_peer_registry_set_state(reg, 30, NIMCP_PEER_ACTIVE);
    // Peer 20 stays in JOINING — not active

    uint32_t ids[8];
    uint32_t count = nimcp_peer_registry_get_active_peers(reg, ids, 8);
    EXPECT_EQ(count, 2u);

    nimcp_peer_registry_destroy(reg);
}

TEST(SwarmLifecycle, GetActiveCountNull) {
    EXPECT_EQ(nimcp_peer_registry_get_active_count(NULL), 0u);
}

// ============================================================================
// State Names
// ============================================================================

TEST(SwarmLifecycle, PeerStateNames) {
    EXPECT_STREQ(nimcp_peer_state_name(NIMCP_PEER_UNKNOWN), "UNKNOWN");
    EXPECT_STREQ(nimcp_peer_state_name(NIMCP_PEER_ACTIVE), "ACTIVE");
    EXPECT_STREQ(nimcp_peer_state_name(NIMCP_PEER_SUSPECTED), "SUSPECTED");
    EXPECT_STREQ(nimcp_peer_state_name(NIMCP_PEER_DEAD), "DEAD");
    EXPECT_STREQ(nimcp_peer_state_name(NIMCP_PEER_BYZANTINE), "BYZANTINE");
}

// ============================================================================
// NULL Safety
// ============================================================================

TEST(SwarmLifecycle, NullRegistryAllFunctions) {
    EXPECT_LT(nimcp_peer_registry_add(NULL, 1, "1.1.1.1", 9421, NULL), 0);
    EXPECT_LT(nimcp_peer_registry_remove(NULL, 1), 0);
    EXPECT_EQ(nimcp_peer_registry_find(NULL, 1), nullptr);
    EXPECT_LT(nimcp_peer_registry_update_heartbeat(NULL, 1), 0);
    EXPECT_LT(nimcp_peer_registry_set_state(NULL, 1, NIMCP_PEER_ACTIVE), 0);
    EXPECT_EQ(nimcp_peer_registry_sweep(NULL, 1000, 5000), 0u);
    EXPECT_EQ(nimcp_peer_registry_get_active_count(NULL), 0u);
    SUCCEED();
}

// ============================================================================
// Rapid Add/Remove (stress)
// ============================================================================

TEST(SwarmLifecycle, RapidAddRemove) {
    nimcp_peer_registry_t* reg = nimcp_peer_registry_create(32);
    ASSERT_NE(reg, nullptr);

    for (uint32_t i = 0; i < 20; i++) {
        nimcp_peer_registry_add(reg, i + 1, "10.0.0.1", 9421, NULL);
    }
    EXPECT_EQ(reg->count, 20u);

    for (uint32_t i = 0; i < 10; i++) {
        nimcp_peer_registry_remove(reg, i + 1);
    }
    EXPECT_EQ(reg->count, 10u);

    // Remaining peers should still be findable
    for (uint32_t i = 10; i < 20; i++) {
        EXPECT_NE(nimcp_peer_registry_find(reg, i + 1), nullptr)
            << "Peer " << (i + 1) << " should still exist";
    }

    nimcp_peer_registry_destroy(reg);
}
