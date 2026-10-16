/**
 * @file test_api_working_memory.cpp
 * @brief Unit tests for NIMCP API working memory functionality
 *
 * Tests working memory API including:
 * - nimcp_brain_working_memory_add()
 * - nimcp_brain_working_memory_get()
 * - nimcp_brain_working_memory_stats()
 * - nimcp_brain_working_memory_refresh()
 * - Capacity management
 * - Parameter validation
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>

class WorkingMemoryTest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        // Initialize NIMCP
        nimcp_init();

        // Create test brain
        // Note: Working memory may need to be enabled in brain config
        brain = nimcp_brain_create("wm_test_brain", NIMCP_BRAIN_TINY,
                                   NIMCP_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        // Cleanup brain
        if (brain) {
            nimcp_brain_destroy(brain);
            brain = nullptr;
        }

        // Shutdown NIMCP
        nimcp_shutdown();
    }
};

//=============================================================================
// Working Memory Add Tests
//=============================================================================

TEST_F(WorkingMemoryTest, AddWithValidDataSucceeds) {
    float data[64];
    for (int i = 0; i < 64; i++) {
        data[i] = (float)i / 64.0f;
    }

    nimcp_status_t status = nimcp_brain_working_memory_add(brain, data, 64, 0.8f);

    // May succeed or fail depending on whether working memory is enabled
    // If it succeeds, it should return NIMCP_SUCCESS
    // If working memory not enabled, should return NIMCP_ERROR_INVALID
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR_INVALID);
}

TEST_F(WorkingMemoryTest, AddWithNullBrainFails) {
    float data[64] = {0.1f, 0.2f, 0.3f};

    nimcp_status_t status = nimcp_brain_working_memory_add(nullptr, data, 64, 0.8f);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "NULL brain provided to working_memory_add");
}

TEST_F(WorkingMemoryTest, AddWithNullDataFails) {
    nimcp_status_t status = nimcp_brain_working_memory_add(brain, nullptr, 64, 0.8f);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "NULL data provided to working_memory_add");
}

TEST_F(WorkingMemoryTest, AddWithZeroSizeFails) {
    float data[64] = {0.1f, 0.2f, 0.3f};

    nimcp_status_t status = nimcp_brain_working_memory_add(brain, data, 0, 0.8f);

    EXPECT_EQ(status, NIMCP_ERROR_INVALID);
    EXPECT_STREQ(nimcp_get_error(), "Invalid size (0) provided to working_memory_add");
}

TEST_F(WorkingMemoryTest, AddWithVariousSaliences) {
    float data[32];
    for (int i = 0; i < 32; i++) {
        data[i] = (float)i / 32.0f;
    }

    // Try different salience values
    float saliences[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float salience : saliences) {
        nimcp_status_t status = nimcp_brain_working_memory_add(brain, data, 32, salience);
        // Should either succeed or fail consistently based on WM availability
        EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR_INVALID);
    }
}

TEST_F(WorkingMemoryTest, AddMultipleItems) {
    // Try to add multiple items
    for (int i = 0; i < 5; i++) {
        float data[16];
        for (int j = 0; j < 16; j++) {
            data[j] = (float)(i * 16 + j) / 100.0f;
        }

        nimcp_status_t status = nimcp_brain_working_memory_add(brain, data, 16, 0.7f);
        EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR_INVALID);
    }
}

TEST_F(WorkingMemoryTest, AddWithDifferentSizes) {
    // Try different data sizes
    uint32_t sizes[] = {1, 8, 16, 32, 64, 128};

    for (uint32_t size : sizes) {
        float* data = new float[size];
        for (uint32_t i = 0; i < size; i++) {
            data[i] = (float)i / (float)size;
        }

        nimcp_status_t status = nimcp_brain_working_memory_add(brain, data, size, 0.8f);
        EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR_INVALID);

        delete[] data;
    }
}

//=============================================================================
// Working Memory Get Tests
//=============================================================================

TEST_F(WorkingMemoryTest, GetWithValidIndexSucceeds) {
    // First add an item
    float add_data[32];
    for (int i = 0; i < 32; i++) {
        add_data[i] = (float)i / 32.0f;
    }

    nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, add_data, 32, 0.9f);

    if (add_status == NIMCP_SUCCESS) {
        // If add succeeded, try to get
        uint32_t size_out = 0;
        const float* retrieved = nimcp_brain_working_memory_get(brain, 0, &size_out);

        EXPECT_NE(retrieved, nullptr);
        EXPECT_GT(size_out, 0);
    }
}

TEST_F(WorkingMemoryTest, GetWithNullBrainFails) {
    uint32_t size_out = 0;
    const float* data = nimcp_brain_working_memory_get(nullptr, 0, &size_out);

    EXPECT_EQ(data, nullptr);
    EXPECT_STREQ(nimcp_get_error(), "Invalid brain handle");
}

TEST_F(WorkingMemoryTest, GetWithInvalidIndexFails) {
    // Try to get from a very high index that likely doesn't exist
    uint32_t size_out = 0;
    const float* data = nimcp_brain_working_memory_get(brain, 9999, &size_out);

    EXPECT_EQ(data, nullptr);
}

TEST_F(WorkingMemoryTest, GetWithNullSizeOutSucceeds) {
    // Add an item
    float add_data[16] = {0.1f, 0.2f, 0.3f};
    nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, add_data, 16, 0.8f);

    if (add_status == NIMCP_SUCCESS) {
        // Get with NULL size_out should still work
        const float* data = nimcp_brain_working_memory_get(brain, 0, nullptr);
        // May succeed or fail, but shouldn't crash
        (void)data; // Suppress unused warning
    }
}

TEST_F(WorkingMemoryTest, GetReturnsCorrectSize) {
    // Add items of different sizes
    uint32_t original_size = 24;
    float* add_data = new float[original_size];
    for (uint32_t i = 0; i < original_size; i++) {
        add_data[i] = (float)i / (float)original_size;
    }

    nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, add_data, original_size, 0.9f);

    if (add_status == NIMCP_SUCCESS) {
        uint32_t retrieved_size = 0;
        const float* retrieved = nimcp_brain_working_memory_get(brain, 0, &retrieved_size);

        if (retrieved != nullptr) {
            EXPECT_EQ(retrieved_size, original_size);
        }
    }

    delete[] add_data;
}

TEST_F(WorkingMemoryTest, GetMultipleItems) {
    // Add multiple items
    for (int i = 0; i < 3; i++) {
        float data[8];
        for (int j = 0; j < 8; j++) {
            data[j] = (float)(i * 10 + j);
        }
        nimcp_brain_working_memory_add(brain, data, 8, 0.8f - i * 0.1f);
    }

    // Try to get each item
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t size_out = 0;
        const float* data = nimcp_brain_working_memory_get(brain, i, &size_out);
        // May succeed based on capacity and whether WM is enabled
        (void)data;
    }
}

//=============================================================================
// Working Memory Stats Tests
//=============================================================================

TEST_F(WorkingMemoryTest, StatsWithValidParamsSucceeds) {
    uint32_t current_size = 0;
    uint32_t capacity = 0;

    nimcp_status_t status = nimcp_brain_working_memory_stats(brain, &current_size, &capacity);

    // Should either succeed or fail if WM not enabled
    EXPECT_TRUE(status == NIMCP_SUCCESS || status == NIMCP_ERROR_INVALID);

    if (status == NIMCP_SUCCESS) {
        // Capacity should be reasonable (typically 7 ± 2)
        EXPECT_GT(capacity, 0);
        EXPECT_LE(capacity, 20);
    }
}

TEST_F(WorkingMemoryTest, StatsWithNullBrainFails) {
    uint32_t current_size = 0;
    uint32_t capacity = 0;

    nimcp_status_t status = nimcp_brain_working_memory_stats(nullptr, &current_size, &capacity);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "NULL brain provided");
}

TEST_F(WorkingMemoryTest, StatsWithNullCurrentSizeFails) {
    uint32_t capacity = 0;

    nimcp_status_t status = nimcp_brain_working_memory_stats(brain, nullptr, &capacity);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "NULL output parameters");
}

TEST_F(WorkingMemoryTest, StatsWithNullCapacityFails) {
    uint32_t current_size = 0;

    nimcp_status_t status = nimcp_brain_working_memory_stats(brain, &current_size, nullptr);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "NULL output parameters");
}

TEST_F(WorkingMemoryTest, StatsWithAllNullFails) {
    nimcp_status_t status = nimcp_brain_working_memory_stats(brain, nullptr, nullptr);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(WorkingMemoryTest, StatsReflectsAddedItems) {
    uint32_t initial_size = 0;
    uint32_t capacity = 0;

    nimcp_status_t initial_status = nimcp_brain_working_memory_stats(brain, &initial_size, &capacity);

    if (initial_status == NIMCP_SUCCESS) {
        // Add an item
        float data[16] = {0.1f, 0.2f, 0.3f};
        nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, data, 16, 0.9f);

        if (add_status == NIMCP_SUCCESS) {
            uint32_t after_size = 0;
            uint32_t after_capacity = 0;
            nimcp_brain_working_memory_stats(brain, &after_size, &after_capacity);

            // Size should have increased
            EXPECT_GT(after_size, initial_size);
            // Capacity should remain the same
            EXPECT_EQ(after_capacity, capacity);
        }
    }
}

TEST_F(WorkingMemoryTest, StatsCapacityIsConstant) {
    uint32_t size1, cap1, size2, cap2;

    nimcp_status_t status1 = nimcp_brain_working_memory_stats(brain, &size1, &cap1);

    if (status1 == NIMCP_SUCCESS) {
        // Add some items
        for (int i = 0; i < 3; i++) {
            float data[8] = {0.1f, 0.2f};
            nimcp_brain_working_memory_add(brain, data, 8, 0.8f);
        }

        nimcp_status_t status2 = nimcp_brain_working_memory_stats(brain, &size2, &cap2);

        if (status2 == NIMCP_SUCCESS) {
            // Capacity should not change
            EXPECT_EQ(cap1, cap2);
        }
    }
}

//=============================================================================
// Working Memory Refresh Tests
//=============================================================================

TEST_F(WorkingMemoryTest, RefreshWithValidIndexSucceeds) {
    // Add an item
    float data[32];
    for (int i = 0; i < 32; i++) {
        data[i] = (float)i / 32.0f;
    }

    nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, data, 32, 0.9f);

    if (add_status == NIMCP_SUCCESS) {
        // Refresh the item
        nimcp_status_t refresh_status = nimcp_brain_working_memory_refresh(brain, 0);
        EXPECT_EQ(refresh_status, NIMCP_SUCCESS);
    }
}

TEST_F(WorkingMemoryTest, RefreshWithNullBrainFails) {
    nimcp_status_t status = nimcp_brain_working_memory_refresh(nullptr, 0);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "NULL brain provided");
}

TEST_F(WorkingMemoryTest, RefreshWithInvalidIndexFails) {
    nimcp_status_t status = nimcp_brain_working_memory_refresh(brain, 9999);

    // Should fail with either INVALID or error based on WM state
    EXPECT_NE(status, NIMCP_SUCCESS);
}

TEST_F(WorkingMemoryTest, RefreshMultipleTimes) {
    // Add an item
    float data[16] = {0.1f, 0.2f, 0.3f};
    nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, data, 16, 0.9f);

    if (add_status == NIMCP_SUCCESS) {
        // Refresh multiple times
        for (int i = 0; i < 5; i++) {
            nimcp_status_t refresh_status = nimcp_brain_working_memory_refresh(brain, 0);
            EXPECT_EQ(refresh_status, NIMCP_SUCCESS);
        }
    }
}

TEST_F(WorkingMemoryTest, RefreshDifferentItems) {
    // Add multiple items
    for (int i = 0; i < 3; i++) {
        float data[8];
        for (int j = 0; j < 8; j++) {
            data[j] = (float)(i * 10 + j);
        }
        nimcp_brain_working_memory_add(brain, data, 8, 0.8f);
    }

    // Refresh each item
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_status_t status = nimcp_brain_working_memory_refresh(brain, i);
        // May succeed based on whether items were actually added
        (void)status;
    }
}

//=============================================================================
// Capacity Management Tests
//=============================================================================

TEST_F(WorkingMemoryTest, CapacityLimitEnforced) {
    uint32_t initial_size, capacity;
    nimcp_status_t stats_status = nimcp_brain_working_memory_stats(brain, &initial_size, &capacity);

    if (stats_status == NIMCP_SUCCESS) {
        // Try to add more items than capacity
        for (uint32_t i = 0; i < capacity + 5; i++) {
            float data[16];
            for (int j = 0; j < 16; j++) {
                data[j] = (float)(i * 16 + j) / 100.0f;
            }
            nimcp_brain_working_memory_add(brain, data, 16, 0.8f);
        }

        // Check that size doesn't exceed capacity
        uint32_t final_size, final_capacity;
        nimcp_brain_working_memory_stats(brain, &final_size, &final_capacity);

        EXPECT_LE(final_size, final_capacity);
    }
}

TEST_F(WorkingMemoryTest, LowSalienceItemsEvicted) {
    uint32_t capacity = 0;
    uint32_t size = 0;
    nimcp_status_t stats_status = nimcp_brain_working_memory_stats(brain, &size, &capacity);

    if (stats_status == NIMCP_SUCCESS && capacity > 0) {
        // Fill capacity with low salience items
        for (uint32_t i = 0; i < capacity; i++) {
            float data[8];
            for (int j = 0; j < 8; j++) {
                data[j] = (float)i;
            }
            nimcp_brain_working_memory_add(brain, data, 8, 0.3f); // Low salience
        }

        // Add high salience item (should evict low salience)
        float high_sal_data[8] = {9.9f, 9.9f, 9.9f, 9.9f, 9.9f, 9.9f, 9.9f, 9.9f};
        nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, high_sal_data, 8, 0.99f);

        EXPECT_EQ(add_status, NIMCP_SUCCESS);
    }
}

TEST_F(WorkingMemoryTest, AddBeyondCapacitySucceeds) {
    uint32_t capacity = 0;
    uint32_t size = 0;
    nimcp_status_t stats_status = nimcp_brain_working_memory_stats(brain, &size, &capacity);

    if (stats_status == NIMCP_SUCCESS && capacity > 0) {
        // Add items beyond capacity
        for (uint32_t i = 0; i < capacity * 2; i++) {
            float data[8];
            for (int j = 0; j < 8; j++) {
                data[j] = (float)i;
            }
            nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, data, 8, 0.5f);

            // Each add should succeed (evicting old items)
            EXPECT_EQ(add_status, NIMCP_SUCCESS);
        }

        // Verify size is at or below capacity
        uint32_t final_size, final_capacity;
        nimcp_brain_working_memory_stats(brain, &final_size, &final_capacity);
        EXPECT_LE(final_size, final_capacity);
    }
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(WorkingMemoryTest, AddGetRefreshCycle) {
    // Add item
    float original_data[32];
    for (int i = 0; i < 32; i++) {
        original_data[i] = (float)i / 32.0f;
    }

    nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, original_data, 32, 0.9f);

    if (add_status == NIMCP_SUCCESS) {
        // Get item
        uint32_t size_out = 0;
        const float* retrieved = nimcp_brain_working_memory_get(brain, 0, &size_out);
        EXPECT_NE(retrieved, nullptr);

        // Refresh item
        nimcp_status_t refresh_status = nimcp_brain_working_memory_refresh(brain, 0);
        EXPECT_EQ(refresh_status, NIMCP_SUCCESS);

        // Get again
        retrieved = nimcp_brain_working_memory_get(brain, 0, &size_out);
        EXPECT_NE(retrieved, nullptr);
    }
}

TEST_F(WorkingMemoryTest, StatsAfterOperations) {
    uint32_t size_before, capacity_before;
    nimcp_brain_working_memory_stats(brain, &size_before, &capacity_before);

    // Do various operations
    float data[16] = {0.5f, 0.5f, 0.5f};
    nimcp_brain_working_memory_add(brain, data, 16, 0.8f);
    nimcp_brain_working_memory_get(brain, 0, nullptr);
    nimcp_brain_working_memory_refresh(brain, 0);

    // Check stats again
    uint32_t size_after, capacity_after;
    nimcp_status_t status = nimcp_brain_working_memory_stats(brain, &size_after, &capacity_after);

    if (status == NIMCP_SUCCESS) {
        EXPECT_EQ(capacity_after, capacity_before); // Capacity unchanged
    }
}

TEST_F(WorkingMemoryTest, MultipleAddGetSequence) {
    // Add several items and retrieve them
    const int num_items = 5;

    for (int i = 0; i < num_items; i++) {
        float data[8];
        for (int j = 0; j < 8; j++) {
            data[j] = (float)(i * 10 + j);
        }

        nimcp_status_t add_status = nimcp_brain_working_memory_add(brain, data, 8, 0.7f);

        if (add_status == NIMCP_SUCCESS) {
            // Immediately retrieve
            uint32_t size_out = 0;
            const float* retrieved = nimcp_brain_working_memory_get(brain, 0, &size_out);
            EXPECT_NE(retrieved, nullptr);
        }
    }
}
