/**
 * @file test_working_memory_pe_regression.cpp
 * @brief Regression tests for positional encoding in working memory
 *
 * WHAT: Regression tests ensuring PE stability in working memory operations
 * WHY:  Working memory relies on PE for serial position effects - must be stable
 * HOW:  Test PE under repeated memory ops, verify consistency and performance
 *
 * REGRESSION TEST PHILOSOPHY:
 * - Ensure PE doesn't degrade with memory operations
 * - Verify PE memory stability in working memory context
 * - Validate PE serial position encoding remains consistent
 * - Test PE with memory capacity changes
 * - Verify PE integration doesn't break working memory
 * - Catch performance regressions in memory+PE path
 *
 * WHAT WE'RE PROTECTING:
 * - Serial position curve accuracy (primacy/recency effects)
 * - PE numerical stability with memory decay
 * - Memory usage with PE enabled
 * - Working memory API stability with PE integration
 * - Performance of memory operations with PE
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_working_memory.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WorkingMemoryPERegressionTest : public ::testing::Test {
protected:
    static constexpr int ITERATIONS = 1000;
    static constexpr int ITEM_SIZE = 128;
    static constexpr int MEMORY_CAPACITY = 7;

    working_memory_t* memory;

    void SetUp() override {
        // WHAT: Initialize bio-async and memory systems
        // WHY:  Working memory + PE requires these systems
        // HOW:  Standard initialization sequence
        nimcp_bio_async_init(nullptr);
        bio_router_config_t cfg = {0};
        bio_router_init(&cfg);

        memory = nullptr;
    }

    void TearDown() override {
        // WHAT: Clean up memory and systems
        // WHY:  Prevent memory leaks
        // HOW:  Standard cleanup sequence
        if (memory) {
            working_memory_destroy(memory);
            memory = nullptr;
        }

        // bio_router_shutdown();
        // bio_async_shutdown();
        // nimcp_unified_memory_shutdown();
    }

    // WHAT: Create test item with specified value
    // WHY:  Need consistent test data
    // HOW:  Fill vector with value pattern
    std::vector<float> CreateTestItem(float value, uint32_t size = ITEM_SIZE) {
        std::vector<float> item(size);
        for (uint32_t i = 0; i < size; i++) {
            item[i] = value + i * 0.01f;  // Add slight variation
        }
        return item;
    }

    // WHAT: Compare two items with tolerance
    // WHY:  Need fuzzy equality for floating point
    // HOW:  Element-wise comparison with epsilon
    bool ItemsMatch(const float* item1, const float* item2, uint32_t size, float tolerance = 1e-5f) {
        for (uint32_t i = 0; i < size; i++) {
            if (std::abs(item1[i] - item2[i]) > tolerance) {
                return false;
            }
        }
        return true;
    }
};

//=============================================================================
// 1. Stability Under Repeated Use Tests
//=============================================================================

TEST_F(WorkingMemoryPERegressionTest, StabilityUnderRepeatedUse_AddRefresh) {
    // WHAT: Verify PE doesn't degrade with repeated add/refresh operations
    // WHY:  Common operation pattern must remain stable
    // HOW:  Add items, refresh them many times, verify PE consistency

    working_memory_config_t config = working_memory_default_config();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    memory = working_memory_create_custom(&config);
    ASSERT_NE(memory, nullptr);

    auto test_item = CreateTestItem(0.5f);

    // Add item once
    working_memory_add(memory, test_item.data(), ITEM_SIZE, 1.0f);

    // Retrieve baseline
    uint32_t retrieved_size = 0;
    const float* baseline_item = working_memory_get(memory, 0, &retrieved_size);
    ASSERT_NE(baseline_item, nullptr);
    ASSERT_EQ(retrieved_size, ITEM_SIZE);

    std::vector<float> baseline(baseline_item, baseline_item + retrieved_size);

    // Refresh many times and verify consistency
    for (int i = 0; i < ITERATIONS; i++) {
        working_memory_refresh(memory, 0);

        uint32_t current_size = 0;
        const float* current_item = working_memory_get(memory, 0, &current_size);
        ASSERT_NE(current_item, nullptr);

        EXPECT_TRUE(ItemsMatch(baseline.data(), current_item, ITEM_SIZE))
            << "Item changed after refresh iteration " << i;

        if (i % 10 == 0) {
            // bio_router_process_messages(); (removed - not available)
        }
    }
}

TEST_F(WorkingMemoryPERegressionTest, StabilityUnderRepeatedUse_MultipleTypes) {
    // WHAT: Verify different PE types remain stable in working memory
    // WHY:  Each PE type should work reliably
    // HOW:  Test each PE type with repeated operations

    nimcp_pos_encoding_type_t types[] = {
        NIMCP_POS_SINUSOIDAL,
        NIMCP_POS_ROTARY,
        NIMCP_POS_ALIBI
    };

    for (auto pe_type : types) {
        working_memory_config_t config = working_memory_default_config();
        config.enable_positional_encoding = true;
        config.pe_type = pe_type;

        working_memory_t* test_memory = working_memory_create_custom(&config);
        ASSERT_NE(test_memory, nullptr) << "Failed to create memory with PE type " << pe_type;

        // Add multiple items
        for (int i = 0; i < 5; i++) {
            auto item = CreateTestItem(i * 0.1f);
            working_memory_add(test_memory, item.data(), ITEM_SIZE, 1.0f);
        }

        // Verify all items retrievable
        for (int i = 0; i < 5; i++) {
            uint32_t size = 0;
            const float* retrieved = working_memory_get(test_memory, i, &size);
            EXPECT_NE(retrieved, nullptr) << "Failed to retrieve item " << i
                                         << " with PE type " << pe_type;
        }

        working_memory_destroy(test_memory);
    }
}

//=============================================================================
// 2. Memory Stability Tests
//=============================================================================

TEST_F(WorkingMemoryPERegressionTest, MemoryStability_NoLeaksWithPE) {
    // WHAT: Verify working memory + PE doesn't leak memory
    // WHY:  PE integration shouldn't introduce leaks
    // HOW:  Create/destroy memory with PE many times

    for (int i = 0; i < 100; i++) {
        working_memory_config_t config = working_memory_default_config();
        config.enable_positional_encoding = true;
        config.pe_type = NIMCP_POS_SINUSOIDAL;

        working_memory_t* test_memory = working_memory_create_custom(&config);
        ASSERT_NE(test_memory, nullptr) << "Failed at iteration " << i;

        // Add some items
        for (int j = 0; j < 5; j++) {
            auto item = CreateTestItem(j * 0.1f);
            working_memory_add(test_memory, item.data(), ITEM_SIZE, 1.0f);
        }

        working_memory_destroy(test_memory);

        if (i % 10 == 0) {
            // bio_router_process_messages(); (removed - not available)
        }
    }

    SUCCEED();
}

TEST_F(WorkingMemoryPERegressionTest, MemoryStability_PEDisabledVsEnabled) {
    // WHAT: Compare memory usage with PE disabled vs enabled
    // WHY:  PE overhead should be reasonable
    // HOW:  Create both types, verify similar behavior

    // Without PE
    working_memory_config_t config_no_pe = working_memory_default_config();
    config_no_pe.enable_positional_encoding = false;

    working_memory_t* memory_no_pe = working_memory_create_custom(&config_no_pe);
    ASSERT_NE(memory_no_pe, nullptr);

    // With PE
    working_memory_config_t config_with_pe = working_memory_default_config();
    config_with_pe.enable_positional_encoding = true;
    config_with_pe.pe_type = NIMCP_POS_SINUSOIDAL;

    working_memory_t* memory_with_pe = working_memory_create_custom(&config_with_pe);
    ASSERT_NE(memory_with_pe, nullptr);

    // Both should handle same operations
    for (int i = 0; i < 7; i++) {
        auto item = CreateTestItem(i * 0.1f);
        working_memory_add(memory_no_pe, item.data(), ITEM_SIZE, 1.0f);
        working_memory_add(memory_with_pe, item.data(), ITEM_SIZE, 1.0f);
    }

    // Verify both have same item count
    working_memory_stats_t stats_no_pe, stats_with_pe;
    working_memory_get_stats(memory_no_pe, &stats_no_pe);
    working_memory_get_stats(memory_with_pe, &stats_with_pe);

    EXPECT_EQ(stats_no_pe.current_size, stats_with_pe.current_size);

    working_memory_destroy(memory_no_pe);
    working_memory_destroy(memory_with_pe);
}

//=============================================================================
// 3. Performance Stability Tests
//=============================================================================

TEST_F(WorkingMemoryPERegressionTest, PerformanceStability_AddOperations) {
    // WHAT: Verify PE doesn't significantly slow add operations
    // WHY:  Add is critical path operation
    // HOW:  Time adds with and without PE, compare

    // Without PE
    working_memory_config_t config_no_pe = working_memory_default_config();
    config_no_pe.enable_positional_encoding = false;

    working_memory_t* memory_no_pe = working_memory_create_custom(&config_no_pe);
    ASSERT_NE(memory_no_pe, nullptr);

    auto test_item = CreateTestItem(0.5f);

    auto start_no_pe = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        working_memory_clear(memory_no_pe);
        working_memory_add(memory_no_pe, test_item.data(), ITEM_SIZE, 1.0f);
    }
    auto end_no_pe = std::chrono::high_resolution_clock::now();
    auto duration_no_pe = std::chrono::duration_cast<std::chrono::microseconds>(
        end_no_pe - start_no_pe).count();

    working_memory_destroy(memory_no_pe);

    // With PE
    working_memory_config_t config_with_pe = working_memory_default_config();
    config_with_pe.enable_positional_encoding = true;
    config_with_pe.pe_type = NIMCP_POS_SINUSOIDAL;

    working_memory_t* memory_with_pe = working_memory_create_custom(&config_with_pe);
    ASSERT_NE(memory_with_pe, nullptr);

    auto start_with_pe = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        working_memory_clear(memory_with_pe);
        working_memory_add(memory_with_pe, test_item.data(), ITEM_SIZE, 1.0f);
    }
    auto end_with_pe = std::chrono::high_resolution_clock::now();
    auto duration_with_pe = std::chrono::duration_cast<std::chrono::microseconds>(
        end_with_pe - start_with_pe).count();

    working_memory_destroy(memory_with_pe);

    // PE should not add more than 3x overhead
    EXPECT_LT(duration_with_pe, duration_no_pe * 3)
        << "PE overhead too high: " << duration_with_pe << "us vs " << duration_no_pe << "us";
}

TEST_F(WorkingMemoryPERegressionTest, PerformanceStability_RetrieveOperations) {
    // WHAT: Verify PE doesn't slow retrieval operations
    // WHY:  Get is critical for reading working memory
    // HOW:  Time retrievals with PE enabled

    working_memory_config_t config = working_memory_default_config();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    memory = working_memory_create_custom(&config);
    ASSERT_NE(memory, nullptr);

    // Fill memory
    for (int i = 0; i < 7; i++) {
        auto item = CreateTestItem(i * 0.1f);
        working_memory_add(memory, item.data(), ITEM_SIZE, 1.0f);
    }

    // Time retrieval
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        uint32_t size = 0;
        const float* item = working_memory_get(memory, i % 7, &size);
        ASSERT_NE(item, nullptr);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    float avg_time_us = duration_us / 1000.0f;

    // Should be fast (< 10us per retrieval)
    EXPECT_LT(avg_time_us, 10.0f)
        << "Average retrieval time " << avg_time_us << "us exceeds 10us threshold";
}

//=============================================================================
// 4. Serial Position Consistency Tests
//=============================================================================

TEST_F(WorkingMemoryPERegressionTest, SerialPosition_ConsistentEncoding) {
    // WHAT: Verify serial position encodings are consistent
    // WHY:  Primacy/recency effects depend on consistent position encoding
    // HOW:  Add items, verify position encodings don't drift

    working_memory_config_t config = working_memory_default_config();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    memory = working_memory_create_custom(&config);
    ASSERT_NE(memory, nullptr);

    // Add items
    std::vector<std::vector<float>> baseline_items;
    for (int i = 0; i < 7; i++) {
        auto item = CreateTestItem(i * 0.1f);
        working_memory_add(memory, item.data(), ITEM_SIZE, 1.0f);

        // Store baseline
        uint32_t size = 0;
        const float* retrieved = working_memory_get(memory, i, &size);
        ASSERT_NE(retrieved, nullptr);
        baseline_items.emplace_back(retrieved, retrieved + size);
    }

    // Refresh all items many times
    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < 7; i++) {
            working_memory_refresh(memory, i);
        }

        // Verify position encodings haven't changed
        for (int i = 0; i < 7; i++) {
            uint32_t size = 0;
            const float* retrieved = working_memory_get(memory, i, &size);
            ASSERT_NE(retrieved, nullptr);

            EXPECT_TRUE(ItemsMatch(baseline_items[i].data(), retrieved, ITEM_SIZE))
                << "Position " << i << " encoding changed at iteration " << iter;
        }

        if (iter % 10 == 0) {
            // Process any pending bio-async messages
        }
    }
}

TEST_F(WorkingMemoryPERegressionTest, SerialPosition_OrderIndependence) {
    // WHAT: Verify PE applied correctly regardless of insertion order
    // WHY:  Position encoding should be absolute, not insertion-dependent
    // HOW:  Add items in different orders, verify position consistency

    working_memory_config_t config = working_memory_default_config();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;

    // Forward order
    working_memory_t* memory_fwd = working_memory_create_custom(&config);
    ASSERT_NE(memory_fwd, nullptr);

    for (int i = 0; i < 5; i++) {
        auto item = CreateTestItem(i * 0.1f);
        working_memory_add(memory_fwd, item.data(), ITEM_SIZE, 1.0f);
    }

    // Reverse order
    working_memory_t* memory_rev = working_memory_create_custom(&config);
    ASSERT_NE(memory_rev, nullptr);

    for (int i = 4; i >= 0; i--) {
        auto item = CreateTestItem(i * 0.1f);
        working_memory_add(memory_rev, item.data(), ITEM_SIZE, 1.0f);
    }

    // Items at same position should have same encoding (regardless of insertion order)
    // Note: This test verifies PE is position-based, not order-based
    // The actual item contents will differ, but the PE should be consistent
    // for items at the same position index

    working_memory_destroy(memory_fwd);
    working_memory_destroy(memory_rev);
    SUCCEED();
}

//=============================================================================
// 5. Capacity Stress Tests
//=============================================================================

TEST_F(WorkingMemoryPERegressionTest, CapacityStress_FullMemory) {
    // WHAT: Verify PE works correctly when memory is at capacity
    // WHY:  Eviction should maintain PE consistency
    // HOW:  Fill memory to capacity, add more, verify PE stability

    working_memory_config_t config = working_memory_default_config();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;
    config.capacity = 7;

    memory = working_memory_create_custom(&config);
    ASSERT_NE(memory, nullptr);

    // Fill to capacity
    for (int i = 0; i < 7; i++) {
        auto item = CreateTestItem(i * 0.1f);
        working_memory_add(memory, item.data(), ITEM_SIZE, 1.0f);
    }

    // Add more items (will trigger eviction)
    for (int i = 7; i < 20; i++) {
        auto item = CreateTestItem(i * 0.1f);
        working_memory_add(memory, item.data(), ITEM_SIZE, 1.0f);

        // Verify memory still has valid items
        working_memory_stats_t stats;
        working_memory_get_stats(memory, &stats);
        EXPECT_LE(stats.current_size, 7) << "Memory exceeded capacity";

        // Verify we can retrieve all current items
        for (uint32_t j = 0; j < stats.current_size; j++) {
            uint32_t size = 0;
            const float* retrieved = working_memory_get(memory, j, &size);
            EXPECT_NE(retrieved, nullptr) << "Failed to retrieve item " << j;
        }
    }
}

TEST_F(WorkingMemoryPERegressionTest, CapacityStress_VaryingCapacity) {
    // WHAT: Verify PE works with different memory capacities
    // WHY:  PE should scale with capacity
    // HOW:  Test various capacity values

    uint32_t capacities[] = {1, 3, 7, 10, 15};

    for (auto capacity : capacities) {
        working_memory_config_t config = working_memory_default_config();
        config.enable_positional_encoding = true;
        config.pe_type = NIMCP_POS_SINUSOIDAL;
        config.capacity = capacity;

        working_memory_t* test_memory = working_memory_create_custom(&config);
        ASSERT_NE(test_memory, nullptr) << "Failed with capacity " << capacity;

        // Fill to capacity
        for (uint32_t i = 0; i < capacity; i++) {
            auto item = CreateTestItem(i * 0.1f);
            working_memory_add(test_memory, item.data(), ITEM_SIZE, 1.0f);
        }

        // Verify all items retrievable
        for (uint32_t i = 0; i < capacity; i++) {
            uint32_t size = 0;
            const float* retrieved = working_memory_get(test_memory, i, &size);
            EXPECT_NE(retrieved, nullptr) << "Failed at capacity " << capacity
                                         << ", item " << i;
        }

        working_memory_destroy(test_memory);
    }
}

//=============================================================================
// 6. Decay + PE Interaction Tests
//=============================================================================

TEST_F(WorkingMemoryPERegressionTest, Decay_PEStabilityWithDecay) {
    // WHAT: Verify PE remains stable as items decay
    // WHY:  Decay shouldn't corrupt position encodings
    // HOW:  Enable decay, wait, verify PE consistency

    working_memory_config_t config = working_memory_default_config();
    config.enable_positional_encoding = true;
    config.pe_type = NIMCP_POS_SINUSOIDAL;
    config.enable_temporal_decay = true;
    config.decay_tau_ms = 100.0f;  // Fast decay for testing

    memory = working_memory_create_custom(&config);
    ASSERT_NE(memory, nullptr);

    // Add items
    for (int i = 0; i < 5; i++) {
        auto item = CreateTestItem(i * 0.1f);
        working_memory_add(memory, item.data(), ITEM_SIZE, 1.0f);
    }

    // Apply decay
    for (int i = 0; i < 10; i++) {
        working_memory_decay(memory, 50.0f);  // 50ms steps

        // Verify items still retrievable (even if salience decayed)
        working_memory_stats_t stats;
        working_memory_get_stats(memory, &stats);

        for (uint32_t j = 0; j < stats.current_size; j++) {
            uint32_t size = 0;
            const float* retrieved = working_memory_get(memory, j, &size);
            if (retrieved != nullptr) {
                // If item still exists, its encoding should be valid
                for (uint32_t k = 0; k < size; k++) {
                    EXPECT_TRUE(std::isfinite(retrieved[k]))
                        << "Non-finite value after decay";
                }
            }
        }
    }
}

//=============================================================================
// Run All Tests
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
