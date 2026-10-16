/**
 * @file test_attention_wm_bridge.cpp
 * @brief Unit tests for Attention-WorkingMemory Integration Bridge
 * @version 1.0.0
 * @date 2025-12-31
 */

#include <gtest/gtest.h>

#include "cognitive/integration/nimcp_attention_wm_bridge.h"

/**
 * @brief Test fixture for Attention-WM bridge tests
 */
class AttentionWMBridgeTest : public ::testing::Test {
protected:
    attention_wm_bridge_t* bridge;
    attention_wm_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, attention_wm_bridge_default_config(&config));
        bridge = attention_wm_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }

    void TearDown() override {
        if (bridge) {
            attention_wm_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

/**
 * @brief Test bridge creation and destruction
 */
TEST_F(AttentionWMBridgeTest, BridgeCreation) {
    // Bridge already created in SetUp, verify it's valid
    EXPECT_NE(nullptr, bridge);

    // Create another bridge with NULL config (uses defaults)
    attention_wm_bridge_t* bridge2 = attention_wm_bridge_create(nullptr);
    EXPECT_NE(nullptr, bridge2);
    attention_wm_bridge_destroy(bridge2);

    // Destroy NULL should be safe
    attention_wm_bridge_destroy(nullptr);
}

/**
 * @brief Test default configuration values (capacity=7, threshold=0.3)
 */
TEST_F(AttentionWMBridgeTest, DefaultConfig) {
    attention_wm_config_t default_config;
    ASSERT_EQ(0, attention_wm_bridge_default_config(&default_config));

    // Verify expected defaults: capacity=7 (Miller's number), threshold=0.3
    EXPECT_EQ(7u, default_config.capacity_limit);
    EXPECT_FLOAT_EQ(0.3f, default_config.attention_threshold);
    EXPECT_GT(default_config.decay_rate, 0.0f);
    EXPECT_LE(default_config.decay_rate, 1.0f);

    // NULL config should return error
    EXPECT_EQ(-1, attention_wm_bridge_default_config(nullptr));
}

/* ============================================================================
 * Attention -> WM Direction Tests
 * ============================================================================ */

/**
 * @brief Test gating entry - item with sufficient attention enters WM
 */
TEST_F(AttentionWMBridgeTest, GateEntry) {
    uint64_t item_id = 100;
    float attention_strength = 0.8f;  // Well above threshold (0.3)

    // Gate item into WM
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, item_id, attention_strength));

    // Verify item is in WM
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);
    EXPECT_EQ(1, count);
    EXPECT_EQ(item_id, items[0].item_id);
    EXPECT_FLOAT_EQ(attention_strength, items[0].attention_strength);

    // NULL bridge should fail
    EXPECT_EQ(-1, attention_wm_gate_entry(nullptr, item_id, attention_strength));
}

/**
 * @brief Test gating rejection - item below threshold is rejected
 */
TEST_F(AttentionWMBridgeTest, GateEntryRejected) {
    uint64_t item_id = 101;
    float attention_strength = 0.1f;  // Below threshold (0.3)

    // Attempt to gate item - should be rejected
    int result = attention_wm_gate_entry(bridge, item_id, attention_strength);
    EXPECT_EQ(-1, result);  // Rejected due to insufficient attention

    // Verify item is NOT in WM
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);
    EXPECT_EQ(0, count);
}

/**
 * @brief Test capacity eviction - at capacity, lowest priority is evicted
 */
TEST_F(AttentionWMBridgeTest, CapacityEviction) {
    // Fill WM to capacity (7 items)
    for (uint64_t i = 1; i <= 7; i++) {
        float attention = 0.4f + (i * 0.05f);  // Varying attention levels
        EXPECT_EQ(0, attention_wm_gate_entry(bridge, i, attention));
    }

    // Verify at capacity
    attention_wm_stats_t stats;
    EXPECT_EQ(0, attention_wm_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(7u, stats.current_item_count);

    // Add one more item with high attention - should evict lowest priority
    uint64_t new_item_id = 100;
    float high_attention = 0.95f;
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, new_item_id, high_attention));

    // Should still be at capacity (evicted one, added one)
    EXPECT_EQ(0, attention_wm_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(7u, stats.current_item_count);
    EXPECT_GT(stats.items_evicted, 0u);

    // Verify new item is present
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);
    EXPECT_EQ(7, count);

    bool found_new_item = false;
    for (int i = 0; i < count; i++) {
        if (items[i].item_id == new_item_id) {
            found_new_item = true;
            break;
        }
    }
    EXPECT_TRUE(found_new_item);
}

/* ============================================================================
 * WM Management Tests
 * ============================================================================ */

/**
 * @brief Test updating priority of WM item
 */
TEST_F(AttentionWMBridgeTest, UpdatePriority) {
    uint64_t item_id = 200;
    float initial_attention = 0.6f;

    // Gate item in
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, item_id, initial_attention));

    // Update priority
    float new_priority = 0.9f;
    EXPECT_EQ(0, attention_wm_update_priority(bridge, item_id, new_priority));

    // Verify priority was updated
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);
    EXPECT_EQ(1, count);
    EXPECT_FLOAT_EQ(new_priority, items[0].priority);

    // Check stats
    attention_wm_stats_t stats;
    EXPECT_EQ(0, attention_wm_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.priority_updates, 0u);

    // Update non-existent item should fail
    EXPECT_EQ(-1, attention_wm_update_priority(bridge, 999, 0.5f));

    // NULL bridge should fail
    EXPECT_EQ(-1, attention_wm_update_priority(nullptr, item_id, 0.5f));
}

/**
 * @brief Test focus shift behavior - boosts new focus, decays old
 */
TEST_F(AttentionWMBridgeTest, FocusShift) {
    // Add multiple items
    uint64_t item1 = 301;
    uint64_t item2 = 302;
    uint64_t item3 = 303;

    EXPECT_EQ(0, attention_wm_gate_entry(bridge, item1, 0.5f));
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, item2, 0.5f));
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, item3, 0.5f));

    // Get initial priorities
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);
    EXPECT_EQ(3, count);

    float old_priority_item1 = 0.0f;
    float old_priority_item2 = 0.0f;
    for (int i = 0; i < count; i++) {
        if (items[i].item_id == item1) old_priority_item1 = items[i].priority;
        if (items[i].item_id == item2) old_priority_item2 = items[i].priority;
    }

    // Shift focus from item1 to item2
    EXPECT_EQ(0, attention_wm_on_focus_shift(bridge, item1, item2));

    // Check stats
    attention_wm_stats_t stats;
    EXPECT_EQ(0, attention_wm_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.focus_shifts, 0u);

    // Get new priorities
    count = attention_wm_get_attended_items(bridge, items, 10);
    float new_priority_item1 = 0.0f;
    float new_priority_item2 = 0.0f;
    for (int i = 0; i < count; i++) {
        if (items[i].item_id == item1) new_priority_item1 = items[i].priority;
        if (items[i].item_id == item2) new_priority_item2 = items[i].priority;
    }

    // New focus should have boosted priority
    EXPECT_GT(new_priority_item2, old_priority_item2);
    // Old focus should have decayed priority
    EXPECT_LT(new_priority_item1, old_priority_item1);

    // NULL bridge should fail
    EXPECT_EQ(-1, attention_wm_on_focus_shift(nullptr, item1, item2));
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

/**
 * @brief Test getting attended items above threshold
 */
TEST_F(AttentionWMBridgeTest, GetAttendedItems) {
    // Add several items with varying attention
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, 1, 0.8f));
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, 2, 0.6f));
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, 3, 0.4f));

    // Get all attended items
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);
    EXPECT_EQ(3, count);

    // Verify item data
    for (int i = 0; i < count; i++) {
        EXPECT_GT(items[i].item_id, 0u);
        EXPECT_GE(items[i].priority, 0.0f);
        EXPECT_LE(items[i].priority, 1.0f);
        EXPECT_GE(items[i].attention_strength, config.attention_threshold);
    }

    // Test with limited max_count
    count = attention_wm_get_attended_items(bridge, items, 2);
    EXPECT_EQ(2, count);

    // NULL checks
    EXPECT_EQ(-1, attention_wm_get_attended_items(nullptr, items, 10));
    EXPECT_EQ(-1, attention_wm_get_attended_items(bridge, nullptr, 10));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

/**
 * @brief Test statistics tracking
 */
TEST_F(AttentionWMBridgeTest, StatsTracking) {
    attention_wm_stats_t stats;

    // Get initial stats
    EXPECT_EQ(0, attention_wm_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.items_gated_in);
    EXPECT_EQ(0u, stats.items_evicted);
    EXPECT_EQ(0u, stats.priority_updates);
    EXPECT_EQ(0u, stats.focus_shifts);
    EXPECT_EQ(0u, stats.current_item_count);

    // Gate in some items
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, 1, 0.5f));
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, 2, 0.6f));
    EXPECT_EQ(0, attention_wm_gate_entry(bridge, 3, 0.7f));

    // Check stats updated
    EXPECT_EQ(0, attention_wm_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(3u, stats.items_gated_in);
    EXPECT_EQ(3u, stats.current_item_count);

    // Update priority
    EXPECT_EQ(0, attention_wm_update_priority(bridge, 1, 0.8f));
    EXPECT_EQ(0, attention_wm_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.priority_updates);

    // Focus shift
    EXPECT_EQ(0, attention_wm_on_focus_shift(bridge, 1, 2));
    EXPECT_EQ(0, attention_wm_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.focus_shifts);

    // Verify average priority is computed
    EXPECT_GT(stats.avg_priority, 0.0f);
    EXPECT_LE(stats.avg_priority, 1.0f);

    // NULL checks
    EXPECT_EQ(-1, attention_wm_bridge_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, attention_wm_bridge_get_stats(bridge, nullptr));
}
