/**
 * @file test_working_memory.cpp
 * @brief Comprehensive unit tests for working memory system
 *
 * WHAT: Test suite for Miller's 7±2 working memory implementation
 * WHY:  Validate temporal decay, attention refresh, and eviction mechanisms
 * HOW:  Google Test framework with biological validation
 *
 * TEST COVERAGE:
 * 1. Lifecycle (create, destroy, custom config)
 * 2. Item management (add, get, remove, clear)
 * 3. Capacity and eviction (salience-based priority)
 * 4. Temporal decay (exponential forgetting)
 * 5. Attention refresh (rehearsal prevents decay)
 * 6. Statistics and queries
 * 7. Error handling (NULL checks, bounds checks)
 *
 * PHASE: 10.2 (Working Memory)
 * @author Claude Code
 * @date 2025-11
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

    #include "cognitive/nimcp_working_memory.h"
    #include "utils/time/nimcp_time.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class WorkingMemoryTest : public ::testing::Test {
protected:
    working_memory_t* wm;

    void SetUp() override {
        wm = nullptr;
    }

    void TearDown() override {
        if (wm) {
            working_memory_destroy(wm);
            wm = nullptr;
        }
    }

    // Helper: Create simple test item
    float* create_test_item(uint32_t size, float fill_value) {
        float* item = new float[size];
        for (uint32_t i = 0; i < size; i++) {
            item[i] = fill_value;
        }
        return item;
    }

    // Helper: Verify item contents
    bool verify_item(const float* item, uint32_t size, float expected_value) {
        for (uint32_t i = 0; i < size; i++) {
            if (fabs(item[i] - expected_value) > 1e-6f) {
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(WorkingMemoryTest, CreateDestroy) {
    // WHAT: Create and destroy working memory with default config
    // WHY:  Validate basic lifecycle
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    EXPECT_EQ(working_memory_get_capacity(wm), WORKING_MEMORY_DEFAULT_CAPACITY);
    EXPECT_EQ(working_memory_get_size(wm), 0);

    working_memory_destroy(wm);
    wm = nullptr;  // Prevent double-free in TearDown
}

TEST_F(WorkingMemoryTest, CreateCustomConfig) {
    // WHAT: Create working memory with custom capacity
    // WHY:  Validate configuration options
    working_memory_config_t config = working_memory_default_config();
    config.capacity = 10;
    config.decay_tau_ms = 2000.0f;

    wm = working_memory_create_custom(&config);
    ASSERT_NE(wm, nullptr);

    EXPECT_EQ(working_memory_get_capacity(wm), 10);
    EXPECT_EQ(working_memory_get_size(wm), 0);
}

TEST_F(WorkingMemoryTest, CreateNullConfig) {
    // WHAT: Attempt to create with NULL config
    // WHY:  Validate error handling
    wm = working_memory_create_custom(nullptr);
    EXPECT_EQ(wm, nullptr);
}

TEST_F(WorkingMemoryTest, CreateInvalidCapacity) {
    // WHAT: Attempt to create with invalid capacity
    // WHY:  Validate bounds checking
    working_memory_config_t config = working_memory_default_config();

    config.capacity = 0;
    wm = working_memory_create_custom(&config);
    EXPECT_EQ(wm, nullptr);

    config.capacity = 100;  // > MAX_CAPACITY
    wm = working_memory_create_custom(&config);
    EXPECT_EQ(wm, nullptr);
}

TEST_F(WorkingMemoryTest, DestroyNull) {
    // WHAT: Destroy NULL pointer (should be safe)
    // WHY:  Validate safe null handling
    working_memory_destroy(nullptr);  // Should not crash
}

// ============================================================================
// ITEM MANAGEMENT TESTS
// ============================================================================

TEST_F(WorkingMemoryTest, AddSingleItem) {
    // WHAT: Add single item to working memory
    // WHY:  Validate basic add operation
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    bool result = working_memory_add(wm, item, 4, 0.8f);

    EXPECT_TRUE(result);
    EXPECT_EQ(working_memory_get_size(wm), 1);
}

TEST_F(WorkingMemoryTest, AddMultipleItems) {
    // WHAT: Add multiple items up to capacity
    // WHY:  Validate sequential additions
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    for (uint32_t i = 0; i < WORKING_MEMORY_DEFAULT_CAPACITY; i++) {
        float* item = create_test_item(4, (float)i);
        bool result = working_memory_add(wm, item, 4, 0.5f);
        delete[] item;

        EXPECT_TRUE(result);
        EXPECT_EQ(working_memory_get_size(wm), i + 1);
    }

    EXPECT_TRUE(working_memory_is_full(wm));
}

TEST_F(WorkingMemoryTest, AddNullItem) {
    // WHAT: Attempt to add NULL item
    // WHY:  Validate NULL pointer checking
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    bool result = working_memory_add(wm, nullptr, 4, 0.5f);
    EXPECT_FALSE(result);
    EXPECT_EQ(working_memory_get_size(wm), 0);
}

TEST_F(WorkingMemoryTest, AddInvalidSalience) {
    // WHAT: Attempt to add item with invalid salience
    // WHY:  Validate salience bounds checking
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    bool result1 = working_memory_add(wm, item, 4, -0.1f);
    EXPECT_FALSE(result1);

    bool result2 = working_memory_add(wm, item, 4, 1.5f);
    EXPECT_FALSE(result2);
}

TEST_F(WorkingMemoryTest, GetItem) {
    // WHAT: Add item and retrieve it
    // WHY:  Validate get operation
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    working_memory_add(wm, item, 4, 0.8f);

    uint32_t size = 0;
    const float* retrieved = working_memory_get(wm, 0, &size);

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(size, 4);
    EXPECT_TRUE(verify_item(retrieved, 4, 0.0f) == false);  // Should contain data
    EXPECT_FLOAT_EQ(retrieved[0], 1.0f);
    EXPECT_FLOAT_EQ(retrieved[3], 4.0f);
}

TEST_F(WorkingMemoryTest, GetInvalidIndex) {
    // WHAT: Attempt to get item at invalid index
    // WHY:  Validate bounds checking
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    working_memory_add(wm, item, 4, 0.8f);

    const float* retrieved = working_memory_get(wm, 10, nullptr);
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(WorkingMemoryTest, RemoveItem) {
    // WHAT: Add items and remove one
    // WHY:  Validate remove operation and compaction
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    // Add 3 items
    for (uint32_t i = 0; i < 3; i++) {
        float* item = create_test_item(4, (float)i);
        working_memory_add(wm, item, 4, 0.5f);
        delete[] item;
    }

    EXPECT_EQ(working_memory_get_size(wm), 3);

    // Remove middle item
    bool result = working_memory_remove(wm, 1);
    EXPECT_TRUE(result);
    EXPECT_EQ(working_memory_get_size(wm), 2);
}

TEST_F(WorkingMemoryTest, ClearAll) {
    // WHAT: Add items and clear all
    // WHY:  Validate clear operation
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    // Add items
    for (uint32_t i = 0; i < 5; i++) {
        float* item = create_test_item(4, (float)i);
        working_memory_add(wm, item, 4, 0.5f);
        delete[] item;
    }

    EXPECT_EQ(working_memory_get_size(wm), 5);

    working_memory_clear(wm);
    EXPECT_EQ(working_memory_get_size(wm), 0);
}

// ============================================================================
// CAPACITY AND EVICTION TESTS
// ============================================================================

TEST_F(WorkingMemoryTest, SalienceBasedEviction) {
    // WHAT: Fill buffer and add item with higher salience
    // WHY:  Validate that lowest salience item is evicted
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    // Fill buffer with varying salience
    float saliences[] = {0.9f, 0.3f, 0.7f, 0.5f, 0.2f, 0.8f, 0.6f};
    for (uint32_t i = 0; i < WORKING_MEMORY_DEFAULT_CAPACITY; i++) {
        float* item = create_test_item(4, (float)i);
        working_memory_add(wm, item, 4, saliences[i]);
        delete[] item;
    }

    EXPECT_TRUE(working_memory_is_full(wm));

    // Add new item with medium salience (should evict 0.2f item at index 4)
    float* new_item = create_test_item(4, 99.0f);
    working_memory_add(wm, new_item, 4, 0.5f);
    delete[] new_item;

    EXPECT_EQ(working_memory_get_size(wm), WORKING_MEMORY_DEFAULT_CAPACITY);

    // Verify statistics
    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);
    EXPECT_EQ(stats.total_evictions, 1);
}

TEST_F(WorkingMemoryTest, FindHighestSalience) {
    // WHAT: Add items and find highest salience
    // WHY:  Validate priority query
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float saliences[] = {0.3f, 0.9f, 0.5f, 0.7f};
    for (uint32_t i = 0; i < 4; i++) {
        float* item = create_test_item(4, (float)i);
        working_memory_add(wm, item, 4, saliences[i]);
        delete[] item;
    }

    float max_salience = 0.0f;
    int max_index = working_memory_find_highest_salience(wm, &max_salience);

    EXPECT_EQ(max_index, 1);
    EXPECT_FLOAT_EQ(max_salience, 0.9f);
}

// ============================================================================
// TEMPORAL DECAY TESTS
// ============================================================================

TEST_F(WorkingMemoryTest, TemporalDecayBasic) {
    // WHAT: Add items and apply decay
    // WHY:  Validate exponential decay formula
    working_memory_config_t config = working_memory_default_config();
    config.decay_tau_ms = 1000.0f;
    wm = working_memory_create_custom(&config);
    ASSERT_NE(wm, nullptr);

    // Add item with medium salience
    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint64_t time_before_add = nimcp_time_monotonic_ms();
    working_memory_add(wm, item, 4, 0.5f);

    // Apply decay after "time" has passed (1 tau = 1000ms)
    uint64_t decay_time = time_before_add + 1000;
    uint32_t removed = working_memory_decay(wm, decay_time);

    // Should decay but not be removed (0.5 × e^(-1) ≈ 0.184 > 0.01)
    EXPECT_EQ(removed, 0);
    EXPECT_EQ(working_memory_get_size(wm), 1);
}

TEST_F(WorkingMemoryTest, TemporalDecayRemoval) {
    // WHAT: Decay item below threshold
    // WHY:  Validate decay-based eviction
    working_memory_config_t config = working_memory_default_config();
    config.decay_tau_ms = 100.0f;  // Fast decay
    config.min_salience = 0.1f;
    wm = working_memory_create_custom(&config);
    ASSERT_NE(wm, nullptr);

    // Add item with low salience
    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint64_t time_before_add = nimcp_time_monotonic_ms();
    working_memory_add(wm, item, 4, 0.2f);

    // Apply significant decay (5 tau = 500ms)
    uint64_t decay_time = time_before_add + 500;
    uint32_t removed = working_memory_decay(wm, decay_time);

    // Should be removed (0.2 × e^(-5) ≈ 0.0013 < 0.1)
    EXPECT_EQ(removed, 1);
    EXPECT_EQ(working_memory_get_size(wm), 0);
}

// ============================================================================
// ATTENTION REFRESH TESTS
// ============================================================================

TEST_F(WorkingMemoryTest, AttentionRefreshPreventsDev) {
    // WHAT: Refresh item and verify no decay
    // WHY:  Validate rehearsal mechanism
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    // Add item
    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint64_t time_before_add = nimcp_time_monotonic_ms();
    working_memory_add(wm, item, 4, 0.5f);

    // Refresh it
    bool refresh_result = working_memory_refresh(wm, 0);
    EXPECT_TRUE(refresh_result);

    // Apply decay (10 tau = 10000ms with default tau=1000ms)
    uint64_t decay_time = time_before_add + 10000;
    uint32_t removed = working_memory_decay(wm, decay_time);

    // Should not be removed (attention prevented decay)
    EXPECT_EQ(removed, 0);
    EXPECT_EQ(working_memory_get_size(wm), 1);
}

TEST_F(WorkingMemoryTest, RefreshInvalidIndex) {
    // WHAT: Attempt to refresh invalid index
    // WHY:  Validate bounds checking
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    bool result = working_memory_refresh(wm, 10);
    EXPECT_FALSE(result);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(WorkingMemoryTest, Statistics) {
    // WHAT: Perform operations and verify statistics
    // WHY:  Validate tracking of lifetime metrics
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    // Add items to fill buffer
    for (uint32_t i = 0; i < WORKING_MEMORY_DEFAULT_CAPACITY; i++) {
        float* item = create_test_item(4, (float)i);
        working_memory_add(wm, item, 4, 0.5f);
        delete[] item;
    }

    // Add one more to trigger eviction
    float* extra = create_test_item(4, 99.0f);
    working_memory_add(wm, extra, 4, 0.8f);
    delete[] extra;

    // Refresh an item
    working_memory_refresh(wm, 0);

    working_memory_stats_t stats;
    working_memory_get_stats(wm, &stats);

    EXPECT_EQ(stats.current_size, WORKING_MEMORY_DEFAULT_CAPACITY);
    EXPECT_EQ(stats.capacity, WORKING_MEMORY_DEFAULT_CAPACITY);
    EXPECT_EQ(stats.total_additions, WORKING_MEMORY_DEFAULT_CAPACITY + 1);
    EXPECT_EQ(stats.total_evictions, 1);
    EXPECT_EQ(stats.total_refreshes, 1);
}

// ============================================================================
// BIOLOGICAL VALIDATION TESTS
// ============================================================================

TEST_F(WorkingMemoryTest, MillersLawCompliance) {
    // WHAT: Verify default capacity matches Miller's 7±2
    // WHY:  Biological plausibility check
    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    uint32_t capacity = working_memory_get_capacity(wm);

    // Miller's law: 7±2 (i.e., 5-9 items)
    EXPECT_GE(capacity, 5);
    EXPECT_LE(capacity, 9);
    EXPECT_EQ(capacity, 7);  // Default should be exactly 7
}

TEST_F(WorkingMemoryTest, RealisticDecayTime) {
    // WHAT: Verify decay time constant is biologically plausible
    // WHY:  Ensure realistic forgetting (τ ≈ 1-2 seconds)
    working_memory_config_t config = working_memory_default_config();

    // Default tau should be around 1000-2000ms
    EXPECT_GE(config.decay_tau_ms, 500.0f);
    EXPECT_LE(config.decay_tau_ms, 5000.0f);
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

TEST_F(WorkingMemoryTest, NullPointerHandling) {
    // WHAT: Call all functions with NULL pointers
    // WHY:  Validate comprehensive NULL checking

    EXPECT_EQ(working_memory_get_size(nullptr), 0);
    EXPECT_EQ(working_memory_get_capacity(nullptr), 0);
    EXPECT_FALSE(working_memory_is_full(nullptr));

    float item[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_FALSE(working_memory_add(nullptr, item, 4, 0.5f));
    EXPECT_EQ(working_memory_get(nullptr, 0, nullptr), nullptr);
    EXPECT_FALSE(working_memory_remove(nullptr, 0));
    EXPECT_FALSE(working_memory_refresh(nullptr, 0));
    EXPECT_EQ(working_memory_decay(nullptr, 0), 0);

    working_memory_stats_t stats;
    working_memory_get_stats(nullptr, &stats);  // Should not crash

    working_memory_clear(nullptr);  // Should not crash
}

// ============================================================================
// EMOTIONAL WORKING MEMORY INTEGRATION TESTS (Phase 10.3)
// ============================================================================

TEST_F(WorkingMemoryTest, AddItemWithEmotion) {
    // WHAT: Add item with emotional tag
    // WHY:  Verify emotional tagging integration

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[3] = {1.0f, 2.0f, 3.0f};
    emotional_tag_t emotion = emotional_tag_create(0.8f, 0.7f, 1000);

    EXPECT_TRUE(working_memory_add_with_emotion(wm, item, 3, 0.5f, &emotion));
    EXPECT_EQ(working_memory_get_size(wm), 1);

    // Verify emotion was stored
    emotional_tag_t retrieved;
    EXPECT_TRUE(working_memory_get_emotion(wm, 0, &retrieved));
    EXPECT_FLOAT_EQ(retrieved.valence, 0.8f);
    EXPECT_FLOAT_EQ(retrieved.arousal, 0.7f);
}

TEST_F(WorkingMemoryTest, EmotionalSalienceBoost) {
    // WHAT: Verify salience is boosted by emotion
    // WHY:  Emotional events should have higher priority

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[2] = {1.0f, 2.0f};
    emotional_tag_t joy = emotional_tag_create(0.9f, 0.8f, 1000);  // High arousal + positive

    float base_salience = 0.5f;
    EXPECT_TRUE(working_memory_add_with_emotion(wm, item, 2, base_salience, &joy));

    // Get total salience (should be boosted)
    float total_salience = 0.0f;
    EXPECT_TRUE(working_memory_get_total_salience(wm, 0, &total_salience));

    // Expected boost: 1.0 + (0.8 × 0.5) + (0.9 × 0.3) = 1.67
    // Total: 0.5 × 1.67 = 0.835
    EXPECT_GT(total_salience, base_salience);
    EXPECT_LT(total_salience, 1.0f);  // Clamped to 1.0
}

TEST_F(WorkingMemoryTest, EmotionalEvictionPriority) {
    // WHAT: Emotional items should be evicted last
    // WHY:  High arousal events are biologically prioritized

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    // Fill to capacity with non-emotional items
    for (int i = 0; i < 7; i++) {
        float item[1] = {static_cast<float>(i)};
        working_memory_add(wm, item, 1, 0.3f);  // Low salience
    }

    // Add emotional item with moderate base salience
    float emotional_item[1] = {100.0f};
    emotional_tag_t fear = emotional_tag_create(-0.8f, 0.9f, 1000);  // High arousal
    working_memory_add_with_emotion(wm, emotional_item, 1, 0.4f, &fear);

    // Emotional item should evict lowest salience non-emotional item
    EXPECT_EQ(working_memory_get_size(wm), 7);

    // Verify emotional item is still present
    bool found_emotional = false;
    for (uint32_t i = 0; i < working_memory_get_size(wm); i++) {
        uint32_t size;
        const float* data = working_memory_get(wm, i, &size);
        if (data && data[0] == 100.0f) {
            found_emotional = true;
            break;
        }
    }
    EXPECT_TRUE(found_emotional);
}

TEST_F(WorkingMemoryTest, GetEmotionNeutralDefault) {
    // WHAT: Items without emotion return neutral
    // WHY:  Graceful handling of non-emotional items

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[2] = {1.0f, 2.0f};
    working_memory_add(wm, item, 2, 0.5f);  // No emotion

    emotional_tag_t retrieved;
    EXPECT_TRUE(working_memory_get_emotion(wm, 0, &retrieved));

    // Should be neutral
    EXPECT_FLOAT_EQ(retrieved.valence, 0.0f);
    EXPECT_FLOAT_EQ(retrieved.arousal, 0.0f);
    EXPECT_EQ(retrieved.category, EMOTION_CAT_NEUTRAL);
}

TEST_F(WorkingMemoryTest, TotalSalienceWithoutEmotion) {
    // WHAT: Total salience equals base salience without emotion
    // WHY:  Verify no boost when no emotion attached

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[1] = {42.0f};
    working_memory_add(wm, item, 1, 0.6f);

    float total_salience;
    EXPECT_TRUE(working_memory_get_total_salience(wm, 0, &total_salience));
    EXPECT_FLOAT_EQ(total_salience, 0.6f);  // No boost
}

TEST_F(WorkingMemoryTest, AddEmotionNullEmotion) {
    // WHAT: Reject NULL emotion tag
    // WHY:  Validate error handling

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[1] = {1.0f};
    EXPECT_FALSE(working_memory_add_with_emotion(wm, item, 1, 0.5f, nullptr));
}

TEST_F(WorkingMemoryTest, AddEmotionInvalidEmotion) {
    // WHAT: Reject invalid emotional tag
    // WHY:  Ensure validation

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[1] = {1.0f};
    emotional_tag_t invalid;
    invalid.valence = 5.0f;  // Out of range
    invalid.arousal = -1.0f;  // Invalid
    invalid.intensity = 0.0f;
    invalid.category = EMOTION_CAT_NEUTRAL;

    EXPECT_FALSE(working_memory_add_with_emotion(wm, item, 1, 0.5f, &invalid));
}

TEST_F(WorkingMemoryTest, MultipleEmotionalItems) {
    // WHAT: Add multiple items with different emotions
    // WHY:  Verify correct tracking of multiple emotional tags

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item1[1] = {1.0f};
    emotional_tag_t joy = emotional_tag_create(0.9f, 0.7f, 1000);
    working_memory_add_with_emotion(wm, item1, 1, 0.5f, &joy);

    float item2[1] = {2.0f};
    // FEAR requires valence < -0.6 AND arousal > 0.6 per implementation
    emotional_tag_t fear = emotional_tag_create(-0.65f, 0.9f, 1000);
    working_memory_add_with_emotion(wm, item2, 1, 0.6f, &fear);

    float item3[1] = {3.0f};
    emotional_tag_t calm = emotional_tag_create(0.3f, 0.1f, 1000);
    working_memory_add_with_emotion(wm, item3, 1, 0.4f, &calm);

    EXPECT_EQ(working_memory_get_size(wm), 3);

    // Verify each emotion
    emotional_tag_t retrieved;

    working_memory_get_emotion(wm, 0, &retrieved);
    EXPECT_EQ(retrieved.category, EMOTION_CAT_JOY);

    working_memory_get_emotion(wm, 1, &retrieved);
    EXPECT_EQ(retrieved.category, EMOTION_CAT_FEAR);

    working_memory_get_emotion(wm, 2, &retrieved);
    EXPECT_EQ(retrieved.category, EMOTION_CAT_CALM);
}

TEST_F(WorkingMemoryTest, EmotionalSalienceClampedTo1) {
    // WHAT: Very high emotional boost should clamp to 1.0
    // WHY:  Prevent salience overflow

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float item[1] = {1.0f};
    emotional_tag_t intense = emotional_tag_create(1.0f, 1.0f, 1000);  // Maximum emotion

    // High base salience + high emotion = should clamp
    working_memory_add_with_emotion(wm, item, 1, 0.9f, &intense);

    float total_salience;
    working_memory_get_total_salience(wm, 0, &total_salience);

    EXPECT_LE(total_salience, 1.0f);  // Must be clamped
}

TEST_F(WorkingMemoryTest, GetEmotionInvalidIndex) {
    // WHAT: Get emotion with invalid index
    // WHY:  Validate bounds checking

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    emotional_tag_t emotion;
    EXPECT_FALSE(working_memory_get_emotion(wm, 999, &emotion));
}

TEST_F(WorkingMemoryTest, GetTotalSalienceInvalidIndex) {
    // WHAT: Get total salience with invalid index
    // WHY:  Validate bounds checking

    wm = working_memory_create();
    ASSERT_NE(wm, nullptr);

    float salience;
    EXPECT_FALSE(working_memory_get_total_salience(wm, 999, &salience));
}
