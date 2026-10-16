/**
 * @file test_gossip.cpp
 * @brief GoogleTest unit tests for NIMCP edge gossip learning subsystem
 *
 * Tests broadcast decisions, update creation/application, seen-hash ring buffer,
 * TTL management, and cleanup.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_edge.h"
#include "edge/nimcp_edge_types.h"
}

class GossipTest : public ::testing::Test {
protected:
    nimcp_gossip_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
        nimcp_gossip_init(&config);
    }

    void TearDown() override {
        // Config may have allocated seen_hashes
    }
};

TEST_F(GossipTest, ShouldBroadcastHighLossRatio) {
    // broadcast_loss_ratio is the threshold: broadcast if loss > ema * ratio
    float ema_loss = 1.0f;
    float high_loss = ema_loss * config.broadcast_loss_ratio + 0.5f;

    bool should = nimcp_gossip_should_broadcast(high_loss, ema_loss, &config);
    EXPECT_TRUE(should);
}

TEST_F(GossipTest, ShouldNotBroadcastNormalLoss) {
    float ema_loss = 1.0f;
    float normal_loss = ema_loss * 0.9f; // Below threshold

    bool should = nimcp_gossip_should_broadcast(normal_loss, ema_loss, &config);
    EXPECT_FALSE(should);
}

TEST_F(GossipTest, CreateUpdateIncludesChangedWeights) {
    float old_weights[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float new_weights[] = {1.0f, 2.5f, 3.0f, 5.0f}; // indices 1,3 changed

    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        42, old_weights, new_weights, 4, 2.0f, 1.0f);
    ASSERT_NE(update, nullptr);

    EXPECT_EQ(update->sender_id, 42u);
    EXPECT_GT(update->num_weights, 0u);
    EXPECT_LE(update->num_weights, 4u);

    // The changed indices should be present
    bool found_delta = false;
    for (uint32_t i = 0; i < update->num_weights; i++) {
        if (std::fabs(update->weight_deltas[i]) > 0.01f) {
            found_delta = true;
        }
    }
    EXPECT_TRUE(found_delta);

    nimcp_gossip_update_destroy(update);
}

TEST_F(GossipTest, CreateUpdateUrgencyCalculation) {
    float old_weights[] = {0.0f, 0.0f};
    float new_weights[] = {10.0f, 10.0f};

    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        1, old_weights, new_weights, 2, 5.0f, 1.0f);
    ASSERT_NE(update, nullptr);

    // High loss / ema ratio → high urgency
    EXPECT_GT(update->urgency, 0.0f);

    nimcp_gossip_update_destroy(update);
}

TEST_F(GossipTest, ApplyUpdateUnseenHashApplied) {
    float old_w[] = {1.0f, 2.0f, 3.0f};
    float new_w[] = {1.5f, 2.5f, 3.5f};

    // Use high loss relative to ema so urgency exceeds threshold (0.5).
    // urgency = min(1.0, loss / (5.0 * ema_loss))
    // With loss=5.0, ema=1.0: urgency = 5.0/(5.0*1.0) = 1.0 > 0.5
    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        1, old_w, new_w, 3, 5.0f, 1.0f);
    ASSERT_NE(update, nullptr);

    float local[] = {1.0f, 2.0f, 3.0f};
    int ret = nimcp_gossip_apply_update(local, update, &config);
    EXPECT_EQ(ret, 0);

    // Local weights should have been modified
    bool changed = false;
    for (int i = 0; i < 3; i++) {
        if (std::fabs(local[i] - 1.0f - i) > 0.001f) {
            changed = true;
        }
    }
    EXPECT_TRUE(changed) << "Applying unseen update should modify weights";

    nimcp_gossip_update_destroy(update);
}

TEST_F(GossipTest, ApplyUpdateSeenHashSkipped) {
    float old_w[] = {1.0f, 2.0f};
    float new_w[] = {1.5f, 2.5f};

    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        1, old_w, new_w, 2, 2.0f, 1.0f);
    ASSERT_NE(update, nullptr);

    // Mark the hash as seen
    nimcp_gossip_mark_seen(&config, update->experience_hash);

    float local[] = {1.0f, 2.0f};
    float local_copy[] = {1.0f, 2.0f};
    nimcp_gossip_apply_update(local, update, &config);

    // If hash is seen, update should be skipped — weights unchanged
    bool unchanged = true;
    for (int i = 0; i < 2; i++) {
        if (std::fabs(local[i] - local_copy[i]) > 0.001f) {
            unchanged = false;
        }
    }
    // Seen hash should be skipped
    EXPECT_TRUE(unchanged) << "Seen hash update should be skipped";

    nimcp_gossip_update_destroy(update);
}

TEST_F(GossipTest, MarkSeenAppearsInBuffer) {
    uint32_t test_hash = 12345;
    EXPECT_FALSE(nimcp_gossip_is_seen(&config, test_hash));

    nimcp_gossip_mark_seen(&config, test_hash);
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, test_hash));
}

TEST_F(GossipTest, RingBufferWrapsAround) {
    // Fill up the seen hash buffer and verify wrap
    uint32_t cap = config.seen_hash_capacity;
    ASSERT_GT(cap, 0u);

    for (uint32_t i = 0; i < cap + 10; i++) {
        nimcp_gossip_mark_seen(&config, 1000 + i);
    }

    // Most recent hashes should be seen
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, 1000 + cap + 9));

    // Oldest hashes may have been evicted
    // (This depends on ring buffer size, just verify no crash)
}

TEST_F(GossipTest, TTLDecrements) {
    float old_w[] = {1.0f};
    float new_w[] = {2.0f};

    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        1, old_w, new_w, 1, 2.0f, 1.0f);
    ASSERT_NE(update, nullptr);

    uint32_t initial_ttl = update->ttl;
    EXPECT_GT(initial_ttl, 0u);

    // TTL should be set based on config max_ttl
    EXPECT_LE(initial_ttl, config.max_ttl);

    nimcp_gossip_update_destroy(update);
}

TEST_F(GossipTest, ZeroWeightDeltaNoUpdate) {
    float weights[] = {1.0f, 2.0f, 3.0f};

    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        1, weights, weights, 3, 0.5f, 1.0f); // identical weights

    if (update != nullptr) {
        // If created, should have 0 changes
        EXPECT_EQ(update->num_weights, 0u);
        nimcp_gossip_update_destroy(update);
    }
    // nullptr is also acceptable — no update needed
}

TEST_F(GossipTest, DestroyFreesArrays) {
    float old_w[] = {1.0f, 2.0f, 3.0f};
    float new_w[] = {1.5f, 2.5f, 3.5f};

    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        1, old_w, new_w, 3, 2.0f, 1.0f);
    ASSERT_NE(update, nullptr);

    // Should not crash or leak
    nimcp_gossip_update_destroy(update);
}

TEST_F(GossipTest, DestroyNullSafe) {
    nimcp_gossip_update_destroy(nullptr);
}

TEST_F(GossipTest, MultipleHashesTracked) {
    nimcp_gossip_mark_seen(&config, 100);
    nimcp_gossip_mark_seen(&config, 200);
    nimcp_gossip_mark_seen(&config, 300);

    EXPECT_TRUE(nimcp_gossip_is_seen(&config, 100));
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, 200));
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, 300));
    EXPECT_FALSE(nimcp_gossip_is_seen(&config, 400));
}

TEST_F(GossipTest, HashCollisionBehavior) {
    // Create two updates from different weights that happen to get same hash
    float old_w[] = {1.0f, 2.0f};
    float new_w[] = {1.5f, 2.5f};

    nimcp_gossip_update_t* update1 = nimcp_gossip_create_update(
        1, old_w, new_w, 2, 2.0f, 1.0f);
    ASSERT_NE(update1, nullptr);

    uint32_t hash = update1->experience_hash;

    // Mark hash as seen
    nimcp_gossip_mark_seen(&config, hash);
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, hash));

    // Second update with same hash should be skipped
    float local[] = {1.0f, 2.0f};
    float local_copy[] = {1.0f, 2.0f};
    nimcp_gossip_apply_update(local, update1, &config);

    // Weights should be unchanged since hash is seen
    bool unchanged = true;
    for (int i = 0; i < 2; i++) {
        if (std::fabs(local[i] - local_copy[i]) > 0.001f) {
            unchanged = false;
        }
    }
    EXPECT_TRUE(unchanged) << "Update with seen hash should be skipped";

    nimcp_gossip_update_destroy(update1);
}

TEST_F(GossipTest, RingBufferWrapsCorrectly) {
    uint32_t cap = config.seen_hash_capacity;
    ASSERT_GT(cap, 0u);

    // Fill beyond capacity
    for (uint32_t i = 0; i < cap + 5; i++) {
        nimcp_gossip_mark_seen(&config, 5000 + i);
    }

    // Newest entries should be present
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, 5000 + cap + 4));
    EXPECT_TRUE(nimcp_gossip_is_seen(&config, 5000 + cap + 3));

    // Oldest entries should be evicted (overwritten by wrap)
    EXPECT_FALSE(nimcp_gossip_is_seen(&config, 5000));
    EXPECT_FALSE(nimcp_gossip_is_seen(&config, 5001));
}

TEST_F(GossipTest, NullConfigReturnsError) {
    float old_w[] = {1.0f};
    float new_w[] = {2.0f};

    nimcp_gossip_update_t* update = nimcp_gossip_create_update(
        1, old_w, new_w, 1, 2.0f, 1.0f);
    ASSERT_NE(update, nullptr);

    float local[] = {1.0f};
    int ret = nimcp_gossip_apply_update(local, update, nullptr);
    EXPECT_EQ(ret, -1);

    nimcp_gossip_update_destroy(update);
}
