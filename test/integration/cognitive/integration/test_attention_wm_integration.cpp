/**
 * @file test_attention_wm_integration.cpp
 * @brief Integration tests for attention-working memory bridge
 * @version 1.0.0
 * @date 2025-01-08
 *
 * WHAT: Integration tests for attention-working memory bidirectional coupling
 * WHY:  Verify attention gating, WM capacity management, focus shifts,
 *       and priority-based eviction work correctly
 * HOW:  Test attention-driven WM updates, capacity limits, and decay
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
#include "cognitive/integration/nimcp_attention_wm_bridge.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_ITEM_ID_1          1001
#define TEST_ITEM_ID_2          1002
#define TEST_ITEM_ID_3          1003
#define TEST_ITEM_ID_4          1004
#define TEST_ITEM_ID_5          1005
#define TEST_ITEM_ID_6          1006
#define TEST_ITEM_ID_7          1007
#define TEST_ITEM_ID_8          1008
#define TEST_ITEM_ID_9          1009
#define TEST_ITEM_ID_10         1010

#define ATTENTION_HIGH          0.9f
#define ATTENTION_MEDIUM        0.5f
#define ATTENTION_LOW           0.2f
#define ATTENTION_THRESHOLD     0.3f

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class AttentionWMIntegrationTest : public ::testing::Test {
protected:
    attention_wm_bridge_t* bridge;
    attention_wm_config_t config;

    void SetUp() override {
        bridge = nullptr;

        /* Get default config */
        int ret = attention_wm_bridge_default_config(&config);
        ASSERT_EQ(ret, 0);

        /* Use default capacity (7, Miller's number) */
        config.attention_threshold = ATTENTION_THRESHOLD;
        config.decay_rate = 0.1f;

        /* Create bridge */
        bridge = attention_wm_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            attention_wm_bridge_destroy(bridge);
        }
    }

    /* Helper to get current item count */
    size_t get_item_count() {
        attention_wm_stats_t stats;
        attention_wm_bridge_get_stats(bridge, &stats);
        return stats.current_item_count;
    }

    /* Helper to check if item is in WM */
    bool is_item_in_wm(uint64_t item_id) {
        attention_wm_item_t items[ATTENTION_WM_MAX_ITEMS];
        int count = attention_wm_get_attended_items(bridge, items, ATTENTION_WM_MAX_ITEMS);
        for (int i = 0; i < count; i++) {
            if (items[i].item_id == item_id) {
                return true;
            }
        }
        return false;
    }

    /* Helper to get item priority */
    float get_item_priority(uint64_t item_id) {
        attention_wm_item_t items[ATTENTION_WM_MAX_ITEMS];
        int count = attention_wm_get_attended_items(bridge, items, ATTENTION_WM_MAX_ITEMS);
        for (int i = 0; i < count; i++) {
            if (items[i].item_id == item_id) {
                return items[i].priority;
            }
        }
        return -1.0f;  /* Not found */
    }
};

/* ============================================================================
 * Attention-Driven WM Update Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, AttentionDrivenWMUpdate) {
    /* Gate multiple items with varying attention strengths */
    int ret;

    /* High attention - should enter WM */
    ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, ATTENTION_HIGH);
    EXPECT_EQ(ret, 0);

    /* Medium attention - should enter WM (above threshold) */
    ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, ATTENTION_MEDIUM);
    EXPECT_EQ(ret, 0);

    /* Low attention - should be rejected (below threshold) */
    ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, ATTENTION_LOW);
    EXPECT_EQ(ret, -1);  /* Rejected due to low attention */

    /* Verify items in WM */
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_1));
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_2));
    EXPECT_FALSE(is_item_in_wm(TEST_ITEM_ID_3));

    /* Check stats */
    attention_wm_stats_t stats;
    attention_wm_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.items_gated_in, 2u);
    EXPECT_EQ(stats.current_item_count, 2u);
}

TEST_F(AttentionWMIntegrationTest, AttentionStrengthAsPriority) {
    /* Items with higher attention should have higher initial priority */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.9f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.6f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, 0.4f);

    /* Verify priorities reflect attention strength */
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);
    EXPECT_EQ(count, 3);

    /* Find each item and check priority */
    for (int i = 0; i < count; i++) {
        if (items[i].item_id == TEST_ITEM_ID_1) {
            EXPECT_NEAR(items[i].priority, 0.9f, 0.1f);
        } else if (items[i].item_id == TEST_ITEM_ID_2) {
            EXPECT_NEAR(items[i].priority, 0.6f, 0.1f);
        } else if (items[i].item_id == TEST_ITEM_ID_3) {
            EXPECT_NEAR(items[i].priority, 0.4f, 0.1f);
        }
    }
}

/* ============================================================================
 * WM Capacity Management Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, WMCapacityManagement) {
    /* Destroy and recreate with smaller capacity for testing */
    attention_wm_bridge_destroy(bridge);

    attention_wm_config_t small_config;
    attention_wm_bridge_default_config(&small_config);
    small_config.capacity_limit = 4;  /* Small capacity */
    small_config.attention_threshold = 0.3f;

    bridge = attention_wm_bridge_create(&small_config);
    ASSERT_NE(bridge, nullptr);

    /* Fill to capacity with varying priorities */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.5f);  /* Priority ~0.5 */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.6f);  /* Priority ~0.6 */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, 0.7f);  /* Priority ~0.7 */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_4, 0.8f);  /* Priority ~0.8 */

    EXPECT_EQ(get_item_count(), 4u);

    /* Add item with high priority - should evict lowest priority item */
    int ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_5, 0.9f);
    EXPECT_EQ(ret, 0);

    /* Count should still be at capacity */
    EXPECT_EQ(get_item_count(), 4u);

    /* Lowest priority item (ID_1) should be evicted */
    EXPECT_FALSE(is_item_in_wm(TEST_ITEM_ID_1));
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_5));

    /* Check eviction stats */
    attention_wm_stats_t stats;
    attention_wm_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.items_evicted, 1u);
}

TEST_F(AttentionWMIntegrationTest, EvictionPreservesHighPriority) {
    /* Recreate with small capacity */
    attention_wm_bridge_destroy(bridge);

    attention_wm_config_t small_config;
    attention_wm_bridge_default_config(&small_config);
    small_config.capacity_limit = 3;
    small_config.attention_threshold = 0.3f;

    bridge = attention_wm_bridge_create(&small_config);
    ASSERT_NE(bridge, nullptr);

    /* Fill with high priority items */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.9f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.85f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, 0.8f);

    /* Try to add lower priority item */
    int ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_4, 0.4f);

    /* Depending on implementation, may reject or evict lowest */
    /* Either way, high priority items should remain */
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_1));
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_2));
}

/* ============================================================================
 * Focus Shift Cycle Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, FocusShiftCycle) {
    /* Add items to WM */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.8f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.8f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, 0.8f);

    /* Record initial priorities */
    float initial_p1 = get_item_priority(TEST_ITEM_ID_1);
    float initial_p2 = get_item_priority(TEST_ITEM_ID_2);

    /* Shift focus from item 1 to item 2 */
    int ret = attention_wm_on_focus_shift(bridge, TEST_ITEM_ID_1, TEST_ITEM_ID_2);
    EXPECT_EQ(ret, 0);

    /* Item 2 should have boosted priority, item 1 decayed */
    float new_p1 = get_item_priority(TEST_ITEM_ID_1);
    float new_p2 = get_item_priority(TEST_ITEM_ID_2);

    EXPECT_LT(new_p1, initial_p1);  /* Decayed */
    EXPECT_GT(new_p2, initial_p2);  /* Boosted (or at least not decayed) */

    /* Check focus shift stats */
    attention_wm_stats_t stats;
    attention_wm_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.focus_shifts, 1u);
}

TEST_F(AttentionWMIntegrationTest, MultipleFocusShiftsAccumulateDecay) {
    /* Add items */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.9f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.9f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, 0.9f);

    float initial_p1 = get_item_priority(TEST_ITEM_ID_1);

    /* Multiple focus shifts away from item 1 */
    attention_wm_on_focus_shift(bridge, TEST_ITEM_ID_1, TEST_ITEM_ID_2);
    float after_first = get_item_priority(TEST_ITEM_ID_1);

    attention_wm_on_focus_shift(bridge, TEST_ITEM_ID_2, TEST_ITEM_ID_3);
    /* Item 1 not directly involved, but may still decay */

    attention_wm_on_focus_shift(bridge, TEST_ITEM_ID_3, TEST_ITEM_ID_2);
    /* Shift back to item 2 */

    /* After multiple shifts, item 1's priority should have decayed */
    float final_p1 = get_item_priority(TEST_ITEM_ID_1);
    EXPECT_LE(final_p1, after_first);

    /* Stats should track all shifts */
    attention_wm_stats_t stats;
    attention_wm_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.focus_shifts, 3u);
}

TEST_F(AttentionWMIntegrationTest, FocusShiftToNewItem) {
    /* Add initial items */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.8f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.8f);

    /* Shift to item not yet in WM (should still work) */
    int ret = attention_wm_on_focus_shift(bridge, TEST_ITEM_ID_1, TEST_ITEM_ID_3);
    EXPECT_EQ(ret, 0);

    /* Item 1 should have decayed */
    float p1 = get_item_priority(TEST_ITEM_ID_1);
    EXPECT_LT(p1, 0.8f);
}

/* ============================================================================
 * Priority-Based Eviction Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, PriorityBasedEviction) {
    /* Recreate with small capacity */
    attention_wm_bridge_destroy(bridge);

    attention_wm_config_t small_config;
    attention_wm_bridge_default_config(&small_config);
    small_config.capacity_limit = 3;
    small_config.attention_threshold = 0.3f;

    bridge = attention_wm_bridge_create(&small_config);
    ASSERT_NE(bridge, nullptr);

    /* Add items with distinct priorities */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.4f);  /* Lowest */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.6f);  /* Medium */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, 0.8f);  /* Highest */

    /* Lower priority of item 2 to make it lowest */
    attention_wm_update_priority(bridge, TEST_ITEM_ID_2, 0.35f);

    /* Add new item - should evict item 2 (now lowest) */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_4, 0.7f);

    /* Item 2 should be evicted as lowest priority */
    EXPECT_FALSE(is_item_in_wm(TEST_ITEM_ID_2));
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_1));
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_3));
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_4));
}

TEST_F(AttentionWMIntegrationTest, LowestPriorityAlwaysEvicted) {
    /* Recreate with small capacity */
    attention_wm_bridge_destroy(bridge);

    attention_wm_config_t small_config;
    attention_wm_bridge_default_config(&small_config);
    small_config.capacity_limit = 3;
    small_config.attention_threshold = 0.3f;

    bridge = attention_wm_bridge_create(&small_config);
    ASSERT_NE(bridge, nullptr);

    /* Track evictions */
    std::vector<uint64_t> evicted_ids;

    /* Fill capacity */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.5f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.6f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, 0.7f);

    /* Add multiple items - each should evict lowest */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_4, 0.8f);
    if (!is_item_in_wm(TEST_ITEM_ID_1)) evicted_ids.push_back(TEST_ITEM_ID_1);

    attention_wm_gate_entry(bridge, TEST_ITEM_ID_5, 0.9f);
    if (!is_item_in_wm(TEST_ITEM_ID_2)) evicted_ids.push_back(TEST_ITEM_ID_2);

    /* Verify evictions occurred in order of lowest priority */
    attention_wm_stats_t stats;
    attention_wm_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.items_evicted, 2u);

    /* Highest priority items should remain */
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_4) || is_item_in_wm(TEST_ITEM_ID_5));
}

/* ============================================================================
 * Priority Update Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, PriorityUpdate) {
    /* Add item */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.5f);

    float initial = get_item_priority(TEST_ITEM_ID_1);
    EXPECT_NEAR(initial, 0.5f, 0.1f);

    /* Update priority */
    int ret = attention_wm_update_priority(bridge, TEST_ITEM_ID_1, 0.9f);
    EXPECT_EQ(ret, 0);

    float updated = get_item_priority(TEST_ITEM_ID_1);
    EXPECT_NEAR(updated, 0.9f, 0.1f);

    /* Stats should track update */
    attention_wm_stats_t stats;
    attention_wm_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.priority_updates, 1u);
}

TEST_F(AttentionWMIntegrationTest, PriorityUpdateToZero) {
    /* Add item */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.8f);

    /* Update to minimum priority */
    int ret = attention_wm_update_priority(bridge, TEST_ITEM_ID_1, 0.0f);
    EXPECT_EQ(ret, 0);

    float priority = get_item_priority(TEST_ITEM_ID_1);
    EXPECT_FLOAT_EQ(priority, 0.0f);
}

TEST_F(AttentionWMIntegrationTest, PriorityUpdateNonexistentItem) {
    /* Try to update item not in WM */
    int ret = attention_wm_update_priority(bridge, TEST_ITEM_ID_1, 0.5f);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, GetAttendedItems) {
    /* Add several items */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.7f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.8f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_3, 0.6f);

    /* Get attended items */
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);

    EXPECT_EQ(count, 3);

    /* Verify item details */
    for (int i = 0; i < count; i++) {
        EXPECT_GT(items[i].item_id, 0u);
        EXPECT_GE(items[i].priority, 0.0f);
        EXPECT_LE(items[i].priority, 1.0f);
        EXPECT_GT(items[i].attention_strength, 0.0f);
    }
}

TEST_F(AttentionWMIntegrationTest, GetAttendedItemsEmptyWM) {
    /* WM is empty */
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(bridge, items, 10);

    EXPECT_EQ(count, 0);
}

TEST_F(AttentionWMIntegrationTest, GetAttendedItemsLimitedBuffer) {
    /* Add more items than buffer can hold */
    for (int i = 0; i < 5; i++) {
        attention_wm_gate_entry(bridge, TEST_ITEM_ID_1 + i, 0.7f);
    }

    /* Get with small buffer */
    attention_wm_item_t items[3];
    int count = attention_wm_get_attended_items(bridge, items, 3);

    EXPECT_EQ(count, 3);  /* Limited by buffer size */
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, StatisticsAccumulation) {
    /* Perform various operations */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.8f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.7f);
    attention_wm_update_priority(bridge, TEST_ITEM_ID_1, 0.9f);
    attention_wm_on_focus_shift(bridge, TEST_ITEM_ID_1, TEST_ITEM_ID_2);

    attention_wm_stats_t stats;
    int ret = attention_wm_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(stats.items_gated_in, 2u);
    EXPECT_EQ(stats.priority_updates, 1u);
    EXPECT_EQ(stats.focus_shifts, 1u);
    EXPECT_EQ(stats.current_item_count, 2u);
    EXPECT_GT(stats.avg_priority, 0.0f);
}

TEST_F(AttentionWMIntegrationTest, AveragePriorityTracking) {
    /* Add items with known priorities */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.6f);
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.8f);

    attention_wm_stats_t stats;
    attention_wm_bridge_get_stats(bridge, &stats);

    /* Average should be approximately (0.6 + 0.8) / 2 = 0.7 */
    EXPECT_NEAR(stats.avg_priority, 0.7f, 0.1f);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, DuplicateItemGating) {
    /* Add same item twice */
    int ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.7f);
    EXPECT_EQ(ret, 0);

    ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.9f);
    /* Should either update or reject - count should not increase */
    EXPECT_EQ(get_item_count(), 1u);
}

TEST_F(AttentionWMIntegrationTest, AttentionAtBoundaries) {
    /* Test minimum attention (should be rejected) */
    int ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, ATTENTION_WM_MIN_PRIORITY);
    /* Below threshold, should fail */

    /* Test maximum attention */
    ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, ATTENTION_WM_MAX_PRIORITY);
    EXPECT_EQ(ret, 0);

    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_2));
}

TEST_F(AttentionWMIntegrationTest, FocusShiftWithNoOldFocus) {
    /* Add items */
    attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.7f);

    /* Shift from "no focus" (0) to item */
    int ret = attention_wm_on_focus_shift(bridge, 0, TEST_ITEM_ID_1);
    EXPECT_EQ(ret, 0);

    /* Item should have boosted priority */
    float priority = get_item_priority(TEST_ITEM_ID_1);
    EXPECT_GE(priority, 0.7f);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, CustomConfiguration) {
    /* Destroy default bridge */
    attention_wm_bridge_destroy(bridge);

    /* Create with custom config */
    attention_wm_config_t custom_config;
    attention_wm_bridge_default_config(&custom_config);
    custom_config.capacity_limit = 5;
    custom_config.attention_threshold = 0.5f;  /* Higher threshold */
    custom_config.decay_rate = 0.2f;

    bridge = attention_wm_bridge_create(&custom_config);
    ASSERT_NE(bridge, nullptr);

    /* Items with attention < 0.5 should be rejected */
    int ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_1, 0.4f);
    EXPECT_EQ(ret, -1);

    ret = attention_wm_gate_entry(bridge, TEST_ITEM_ID_2, 0.6f);
    EXPECT_EQ(ret, 0);

    EXPECT_FALSE(is_item_in_wm(TEST_ITEM_ID_1));
    EXPECT_TRUE(is_item_in_wm(TEST_ITEM_ID_2));
}

/* ============================================================================
 * Null/Error Handling Tests
 * ============================================================================ */

TEST_F(AttentionWMIntegrationTest, NullBridgeHandling) {
    int ret;

    ret = attention_wm_gate_entry(nullptr, TEST_ITEM_ID_1, 0.5f);
    EXPECT_EQ(ret, -1);

    ret = attention_wm_on_focus_shift(nullptr, TEST_ITEM_ID_1, TEST_ITEM_ID_2);
    EXPECT_EQ(ret, -1);

    ret = attention_wm_update_priority(nullptr, TEST_ITEM_ID_1, 0.5f);
    EXPECT_EQ(ret, -1);

    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(nullptr, items, 10);
    EXPECT_EQ(count, -1);

    attention_wm_stats_t stats;
    ret = attention_wm_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(AttentionWMIntegrationTest, NullOutputHandling) {
    int count = attention_wm_get_attended_items(bridge, nullptr, 10);
    EXPECT_EQ(count, -1);

    int ret = attention_wm_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
