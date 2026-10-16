//=============================================================================
// test_unified_memory.cpp - Unit Tests for Unified Memory Manager
//=============================================================================
/**
 * @file test_unified_memory.cpp
 * @brief Comprehensive unit tests for unified memory manager with CoW integration
 *
 * Tests cover:
 * - Manager creation and destruction
 * - Automatic strategy selection
 * - Object-level CoW allocations
 * - Page-level CoW allocations
 * - Direct pool allocations
 * - Cloning and sharing
 * - CoW triggering on write
 * - Snapshot creation and restoration
 * - Page pool integration
 * - Statistics tracking
 * - Thread safety
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class UnifiedMemoryTest : public ::testing::Test {
protected:
    unified_mem_manager_t manager_ = nullptr;

    void SetUp() override {
        unified_mem_config_t config = unified_mem_default_config();
        config.enable_tracking = true;
        manager_ = unified_mem_create(&config);
        ASSERT_NE(manager_, nullptr) << "Failed to create unified memory manager";
    }

    void TearDown() override {
        if (manager_) {
            unified_mem_destroy(manager_);
            manager_ = nullptr;
        }
    }

    // Helper to create test data
    std::vector<float> createTestData(size_t num_floats, float start_value = 0.0f) {
        std::vector<float> data(num_floats);
        for (size_t i = 0; i < num_floats; i++) {
            data[i] = start_value + static_cast<float>(i);
        }
        return data;
    }

    // Helper to verify data
    bool verifyData(const void* ptr, const std::vector<float>& expected) {
        const float* data = static_cast<const float*>(ptr);
        for (size_t i = 0; i < expected.size(); i++) {
            if (data[i] != expected[i]) return false;
        }
        return true;
    }
};

//=============================================================================
// Manager Creation Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, CreateManager_DefaultConfig) {
    unified_mem_manager_t mgr = unified_mem_create(nullptr);
    ASSERT_NE(mgr, nullptr);
    unified_mem_destroy(mgr);
}

TEST_F(UnifiedMemoryTest, CreateManager_CustomConfig) {
    unified_mem_config_t config = {
        .page_threshold = 32 * 1024,
        .object_pool_block_size = 0,
        .object_pool_num_blocks = 512,
        .page_pool_num_pages = 128,
        .default_strategy = UNIFIED_STRATEGY_AUTO,
        .enable_cow = true,
        .enable_tracking = true,
        .user_data = nullptr
    };

    unified_mem_manager_t mgr = unified_mem_create(&config);
    ASSERT_NE(mgr, nullptr);

    // Verify page pool was created
    size_t total_pages = 0, free_pages = 0;
    EXPECT_TRUE(unified_mem_get_page_pool_stats(mgr, &total_pages, &free_pages));
    EXPECT_EQ(total_pages, 128);
    EXPECT_EQ(free_pages, 128);

    unified_mem_destroy(mgr);
}

TEST_F(UnifiedMemoryTest, DefaultConfig_ValidValues) {
    unified_mem_config_t config = unified_mem_default_config();

    EXPECT_EQ(config.page_threshold, UNIFIED_MEM_PAGE_THRESHOLD);
    EXPECT_TRUE(config.enable_cow);
    EXPECT_TRUE(config.enable_tracking);
    EXPECT_EQ(config.default_strategy, UNIFIED_STRATEGY_AUTO);
}

//=============================================================================
// Strategy Selection Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, AutoStrategy_SmallAllocation_UsesObjectCoW) {
    // Small allocation (< page_threshold) should use object-level CoW
    const size_t size = 1024;  // 1KB
    unified_mem_request_t req = unified_mem_request(size, nullptr, true);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    EXPECT_EQ(unified_mem_get_strategy(h), UNIFIED_STRATEGY_OBJECT_COW);
    EXPECT_EQ(unified_mem_get_size(h), size);
    EXPECT_EQ(unified_mem_get_state(h), UNIFIED_STATE_SHARED);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, AutoStrategy_LargeAllocation_UsesPageCoW) {
    // Large allocation (>= page_threshold) should use page-level CoW
    const size_t size = 128 * 1024;  // 128KB
    unified_mem_request_t req = unified_mem_request(size, nullptr, true);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    EXPECT_EQ(unified_mem_get_strategy(h), UNIFIED_STRATEGY_PAGE_COW);
    EXPECT_EQ(unified_mem_get_size(h), size);
    EXPECT_EQ(unified_mem_get_state(h), UNIFIED_STATE_SHARED);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, AutoStrategy_NoCow_UsesPoolDirect) {
    // No CoW requested should use direct pool
    const size_t size = 4096;
    unified_mem_request_t req = unified_mem_request_direct(size);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    EXPECT_EQ(unified_mem_get_strategy(h), UNIFIED_STRATEGY_POOL_DIRECT);
    EXPECT_EQ(unified_mem_get_state(h), UNIFIED_STATE_DIRECT);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, ForcedStrategy_ObjectCoW) {
    const size_t size = 256 * 1024;  // Large, but force object CoW
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    EXPECT_EQ(unified_mem_get_strategy(h), UNIFIED_STRATEGY_OBJECT_COW);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, ForcedStrategy_PageCoW) {
    const size_t size = 1024;  // Small, but force page CoW
    unified_mem_request_t req = unified_mem_request_page_cow(size, nullptr);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    EXPECT_EQ(unified_mem_get_strategy(h), UNIFIED_STRATEGY_PAGE_COW);

    unified_mem_free(h);
}

//=============================================================================
// Allocation Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, Alloc_WithInitialData_ObjectCoW) {
    const size_t size = 4096;
    std::vector<float> data = createTestData(size / sizeof(float), 100.0f);

    unified_mem_request_t req = {
        .size = size,
        .initial_data = data.data(),
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    const void* ptr = unified_mem_read(h);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(verifyData(ptr, data));

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Alloc_WithInitialData_PageCoW) {
    const size_t size = 128 * 1024;
    std::vector<float> data = createTestData(size / sizeof(float), 200.0f);

    unified_mem_request_t req = unified_mem_request_page_cow(size, data.data());

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    const void* ptr = unified_mem_read(h);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(verifyData(ptr, data));

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Alloc_ZeroSize_ReturnsNull) {
    unified_mem_request_t req = unified_mem_request(0, nullptr, true);
    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    EXPECT_EQ(h, nullptr);
}

TEST_F(UnifiedMemoryTest, Alloc_NullManager_ReturnsNull) {
    unified_mem_request_t req = unified_mem_request(1024, nullptr, true);
    unified_mem_handle_t h = unified_mem_alloc(nullptr, &req);
    EXPECT_EQ(h, nullptr);
}

TEST_F(UnifiedMemoryTest, Alloc_NullRequest_ReturnsNull) {
    unified_mem_handle_t h = unified_mem_alloc(manager_, nullptr);
    EXPECT_EQ(h, nullptr);
}

//=============================================================================
// Read/Write Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, ReadWrite_ObjectCoW) {
    const size_t size = 4096;
    std::vector<float> data = createTestData(size / sizeof(float));

    unified_mem_request_t req = {
        .size = size,
        .initial_data = data.data(),
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Initially shared
    EXPECT_TRUE(unified_mem_is_shared(h));
    EXPECT_EQ(unified_mem_get_state(h), UNIFIED_STATE_SHARED);

    // Read should not trigger CoW
    const float* read_ptr = static_cast<const float*>(unified_mem_read(h));
    ASSERT_NE(read_ptr, nullptr);
    EXPECT_TRUE(unified_mem_is_shared(h));

    // Write should trigger CoW
    float* write_ptr = static_cast<float*>(unified_mem_write(h));
    ASSERT_NE(write_ptr, nullptr);
    EXPECT_FALSE(unified_mem_is_shared(h));
    EXPECT_EQ(unified_mem_get_state(h), UNIFIED_STATE_PRIVATE);

    // Modify data
    write_ptr[0] = 999.0f;

    // Read should return modified data
    read_ptr = static_cast<const float*>(unified_mem_read(h));
    EXPECT_FLOAT_EQ(read_ptr[0], 999.0f);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, ReadWrite_PageCoW) {
    const size_t size = 128 * 1024;
    std::vector<float> data = createTestData(size / sizeof(float));

    unified_mem_request_t req = unified_mem_request_page_cow(size, data.data());

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Initially shared
    EXPECT_TRUE(unified_mem_is_shared(h));

    // Write triggers CoW
    float* write_ptr = static_cast<float*>(unified_mem_write(h));
    ASSERT_NE(write_ptr, nullptr);

    // Modify data
    write_ptr[0] = 888.0f;

    // Read should return modified data
    const float* read_ptr = static_cast<const float*>(unified_mem_read(h));
    EXPECT_FLOAT_EQ(read_ptr[0], 888.0f);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, ReadWrite_Direct) {
    const size_t size = 4096;
    unified_mem_request_t req = unified_mem_request_direct(size);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Direct allocation is always writable
    EXPECT_FALSE(unified_mem_is_shared(h));
    EXPECT_EQ(unified_mem_get_state(h), UNIFIED_STATE_DIRECT);

    float* ptr = static_cast<float*>(unified_mem_write(h));
    ASSERT_NE(ptr, nullptr);
    ptr[0] = 777.0f;

    const float* read_ptr = static_cast<const float*>(unified_mem_read(h));
    EXPECT_FLOAT_EQ(read_ptr[0], 777.0f);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Read_NullHandle_ReturnsNull) {
    EXPECT_EQ(unified_mem_read(nullptr), nullptr);
}

TEST_F(UnifiedMemoryTest, Write_NullHandle_ReturnsNull) {
    EXPECT_EQ(unified_mem_write(nullptr), nullptr);
}

//=============================================================================
// Clone Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, Clone_ObjectCoW_SharesData) {
    const size_t size = 4096;
    std::vector<float> data = createTestData(size / sizeof(float));

    unified_mem_request_t req = {
        .size = size,
        .initial_data = data.data(),
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h1 = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h1, nullptr);

    unified_mem_handle_t h2 = unified_mem_clone(h1);
    ASSERT_NE(h2, nullptr);

    // Both should be shared
    EXPECT_TRUE(unified_mem_is_shared(h1));
    EXPECT_TRUE(unified_mem_is_shared(h2));

    // Both should have same data
    const float* ptr1 = static_cast<const float*>(unified_mem_read(h1));
    const float* ptr2 = static_cast<const float*>(unified_mem_read(h2));
    EXPECT_EQ(memcmp(ptr1, ptr2, size), 0);

    unified_mem_free(h2);
    unified_mem_free(h1);
}

TEST_F(UnifiedMemoryTest, Clone_ObjectCoW_WriteTriggersCoW) {
    const size_t size = 4096;
    std::vector<float> data = createTestData(size / sizeof(float));

    unified_mem_request_t req = {
        .size = size,
        .initial_data = data.data(),
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h1 = unified_mem_alloc(manager_, &req);
    unified_mem_handle_t h2 = unified_mem_clone(h1);
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);

    // Write to h1 - should trigger CoW
    float* ptr1 = static_cast<float*>(unified_mem_write(h1));
    ptr1[0] = 555.0f;

    // h1 should be private, h2 should still be shared
    EXPECT_FALSE(unified_mem_is_shared(h1));
    EXPECT_TRUE(unified_mem_is_shared(h2));

    // h2 should still have original data
    const float* ptr2 = static_cast<const float*>(unified_mem_read(h2));
    EXPECT_FLOAT_EQ(ptr2[0], data[0]);

    unified_mem_free(h2);
    unified_mem_free(h1);
}

TEST_F(UnifiedMemoryTest, Clone_PageCoW_SharesData) {
    const size_t size = 128 * 1024;
    std::vector<float> data = createTestData(size / sizeof(float));

    unified_mem_request_t req = unified_mem_request_page_cow(size, data.data());

    unified_mem_handle_t h1 = unified_mem_alloc(manager_, &req);
    unified_mem_handle_t h2 = unified_mem_clone(h1);
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);

    // Both should be shared
    EXPECT_TRUE(unified_mem_is_shared(h1));
    EXPECT_TRUE(unified_mem_is_shared(h2));

    unified_mem_free(h2);
    unified_mem_free(h1);
}

TEST_F(UnifiedMemoryTest, Clone_Direct_CopiesData) {
    const size_t size = 4096;
    unified_mem_request_t req = unified_mem_request_direct(size);

    unified_mem_handle_t h1 = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h1, nullptr);

    // Write some data
    float* ptr1 = static_cast<float*>(unified_mem_write(h1));
    ptr1[0] = 123.0f;

    // Clone
    unified_mem_handle_t h2 = unified_mem_clone(h1);
    ASSERT_NE(h2, nullptr);

    // Both should be direct (no sharing for direct allocations)
    EXPECT_EQ(unified_mem_get_state(h1), UNIFIED_STATE_DIRECT);
    EXPECT_EQ(unified_mem_get_state(h2), UNIFIED_STATE_DIRECT);

    // h2 should have copy of data
    const float* ptr2 = static_cast<const float*>(unified_mem_read(h2));
    EXPECT_FLOAT_EQ(ptr2[0], 123.0f);

    // Modifying h2 should not affect h1
    float* ptr2_write = static_cast<float*>(unified_mem_write(h2));
    ptr2_write[0] = 456.0f;

    ptr1 = static_cast<float*>(unified_mem_write(h1));
    EXPECT_FLOAT_EQ(ptr1[0], 123.0f);

    unified_mem_free(h2);
    unified_mem_free(h1);
}

TEST_F(UnifiedMemoryTest, Clone_NullHandle_ReturnsNull) {
    EXPECT_EQ(unified_mem_clone(nullptr), nullptr);
}

//=============================================================================
// Make Private Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, MakePrivate_ObjectCoW) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    EXPECT_TRUE(unified_mem_is_shared(h));

    EXPECT_TRUE(unified_mem_make_private(h));

    EXPECT_FALSE(unified_mem_is_shared(h));
    EXPECT_EQ(unified_mem_get_state(h), UNIFIED_STATE_PRIVATE);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, MakePrivate_AlreadyPrivate) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Make private twice
    EXPECT_TRUE(unified_mem_make_private(h));
    EXPECT_TRUE(unified_mem_make_private(h));  // Should succeed (no-op)

    unified_mem_free(h);
}

//=============================================================================
// Memory Savings Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, MemorySaved_SharedHandle) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Shared handle saves memory
    EXPECT_EQ(unified_mem_get_memory_saved(h), size);

    // After becoming private, no savings
    unified_mem_make_private(h);
    EXPECT_EQ(unified_mem_get_memory_saved(h), 0);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, MemorySaved_DirectHandle) {
    const size_t size = 4096;
    unified_mem_request_t req = unified_mem_request_direct(size);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Direct handle has no savings
    EXPECT_EQ(unified_mem_get_memory_saved(h), 0);

    unified_mem_free(h);
}

//=============================================================================
// Snapshot Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, Snapshot_ObjectCoW_Create) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    unified_mem_snapshot_t snap = unified_mem_snapshot_create(h);
    ASSERT_NE(snap, nullptr);

    unified_mem_snapshot_destroy(snap);
    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Snapshot_ObjectCoW_Restore) {
    const size_t size = 4096;
    std::vector<float> data = createTestData(size / sizeof(float), 100.0f);

    unified_mem_request_t req = {
        .size = size,
        .initial_data = data.data(),
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Create snapshot
    unified_mem_snapshot_t snap = unified_mem_snapshot_create(h);
    ASSERT_NE(snap, nullptr);

    // Modify data
    float* ptr = static_cast<float*>(unified_mem_write(h));
    ptr[0] = 999.0f;

    // Verify modification
    const float* read_ptr = static_cast<const float*>(unified_mem_read(h));
    EXPECT_FLOAT_EQ(read_ptr[0], 999.0f);

    // Restore from snapshot
    EXPECT_TRUE(unified_mem_snapshot_restore(h, snap));

    // Verify restoration
    read_ptr = static_cast<const float*>(unified_mem_read(h));
    EXPECT_FLOAT_EQ(read_ptr[0], data[0]);

    unified_mem_snapshot_destroy(snap);
    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Snapshot_PageCoW_Create) {
    const size_t size = 128 * 1024;
    unified_mem_request_t req = unified_mem_request_page_cow(size, nullptr);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    unified_mem_snapshot_t snap = unified_mem_snapshot_create(h);
    ASSERT_NE(snap, nullptr);

    unified_mem_snapshot_destroy(snap);
    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Snapshot_Direct_CreateAndRestore) {
    const size_t size = 4096;
    unified_mem_request_t req = unified_mem_request_direct(size);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Write initial data
    float* ptr = static_cast<float*>(unified_mem_write(h));
    ptr[0] = 100.0f;

    // Create snapshot
    unified_mem_snapshot_t snap = unified_mem_snapshot_create(h);
    ASSERT_NE(snap, nullptr);

    // Modify data
    ptr[0] = 200.0f;

    // Restore
    EXPECT_TRUE(unified_mem_snapshot_restore(h, snap));

    // Verify restoration
    const float* read_ptr = static_cast<const float*>(unified_mem_read(h));
    EXPECT_FLOAT_EQ(read_ptr[0], 100.0f);

    unified_mem_snapshot_destroy(snap);
    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Snapshot_DeltaBytes) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    unified_mem_snapshot_t snap = unified_mem_snapshot_create(h);
    ASSERT_NE(snap, nullptr);

    // No changes yet
    size_t delta = unified_mem_snapshot_get_delta_bytes(h, snap);
    EXPECT_EQ(delta, 0);

    // Make changes - actually modify the data
    char* data = static_cast<char*>(unified_mem_write(h));
    ASSERT_NE(data, nullptr);
    memset(data, 0xFF, size);  // Fill with 0xFF (different from zero-initialized)

    // Now there should be delta (all bytes changed)
    delta = unified_mem_snapshot_get_delta_bytes(h, snap);
    EXPECT_EQ(delta, size);

    unified_mem_snapshot_destroy(snap);
    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Snapshot_NullHandle_ReturnsNull) {
    EXPECT_EQ(unified_mem_snapshot_create(nullptr), nullptr);
}

TEST_F(UnifiedMemoryTest, Snapshot_Restore_WrongHandle_ReturnsFalse) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h1 = unified_mem_alloc(manager_, &req);
    unified_mem_handle_t h2 = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h1, nullptr);
    ASSERT_NE(h2, nullptr);

    unified_mem_snapshot_t snap = unified_mem_snapshot_create(h1);
    ASSERT_NE(snap, nullptr);

    // Try to restore wrong handle
    EXPECT_FALSE(unified_mem_snapshot_restore(h2, snap));

    unified_mem_snapshot_destroy(snap);
    unified_mem_free(h2);
    unified_mem_free(h1);
}

//=============================================================================
// Page Pool Integration Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, PagePool_Enable) {
    EXPECT_TRUE(unified_mem_enable_page_pool(manager_, 64));

    size_t total_pages = 0, free_pages = 0;
    EXPECT_TRUE(unified_mem_get_page_pool_stats(manager_, &total_pages, &free_pages));
    EXPECT_EQ(total_pages, 64);
    EXPECT_EQ(free_pages, 64);
}

TEST_F(UnifiedMemoryTest, PagePool_Disable) {
    unified_mem_enable_page_pool(manager_, 64);
    unified_mem_disable_page_pool(manager_);

    size_t total_pages = 0, free_pages = 0;
    EXPECT_FALSE(unified_mem_get_page_pool_stats(manager_, &total_pages, &free_pages));
}

TEST_F(UnifiedMemoryTest, PagePool_UsedForDirectAllocation) {
    // Enable page pool
    EXPECT_TRUE(unified_mem_enable_page_pool(manager_, 64));

    // Allocate a single page directly
    unified_mem_request_t req = unified_mem_request_direct(PAGE_COW_PAGE_SIZE);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    // Check pool usage
    size_t total_pages = 0, free_pages = 0;
    EXPECT_TRUE(unified_mem_get_page_pool_stats(manager_, &total_pages, &free_pages));
    EXPECT_EQ(free_pages, 63);  // One page used

    unified_mem_free(h);

    // Page should be returned to pool
    EXPECT_TRUE(unified_mem_get_page_pool_stats(manager_, &total_pages, &free_pages));
    EXPECT_EQ(free_pages, 64);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, Stats_InitialValues) {
    unified_mem_stats_t stats;
    EXPECT_TRUE(unified_mem_get_stats(manager_, &stats));

    EXPECT_EQ(stats.total_allocations, 0);
    EXPECT_EQ(stats.active_handles, 0);
    EXPECT_EQ(stats.cow_triggers, 0);
}

TEST_F(UnifiedMemoryTest, Stats_AfterAllocations) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    unified_mem_stats_t stats;
    EXPECT_TRUE(unified_mem_get_stats(manager_, &stats));

    EXPECT_EQ(stats.total_allocations, 1);
    EXPECT_EQ(stats.active_handles, 1);
    EXPECT_EQ(stats.object_cow_allocations, 1);
    EXPECT_EQ(stats.shared_handles, 1);

    unified_mem_free(h);

    EXPECT_TRUE(unified_mem_get_stats(manager_, &stats));
    EXPECT_EQ(stats.active_handles, 0);
}

TEST_F(UnifiedMemoryTest, Stats_CoWTriggers) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    unified_mem_stats_t stats;
    EXPECT_TRUE(unified_mem_get_stats(manager_, &stats));
    EXPECT_EQ(stats.cow_triggers, 0);

    // Trigger CoW
    unified_mem_write(h);

    EXPECT_TRUE(unified_mem_get_stats(manager_, &stats));
    EXPECT_EQ(stats.cow_triggers, 1);
    EXPECT_EQ(stats.private_handles, 1);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Stats_Reset) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    unified_mem_write(h);

    unified_mem_reset_stats(manager_);

    unified_mem_stats_t stats;
    EXPECT_TRUE(unified_mem_get_stats(manager_, &stats));
    EXPECT_EQ(stats.total_allocations, 0);
    EXPECT_EQ(stats.cow_triggers, 0);

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, Stats_NullManager_ReturnsFalse) {
    unified_mem_stats_t stats;
    EXPECT_FALSE(unified_mem_get_stats(nullptr, &stats));
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, ThreadSafety_ConcurrentAlloc) {
    const int num_threads = 4;
    const int allocs_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    auto alloc_func = [this, &success_count, allocs_per_thread]() {
        for (int i = 0; i < allocs_per_thread; i++) {
            unified_mem_request_t req = unified_mem_request(1024, nullptr, true);
            unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
            if (h) {
                success_count++;
                unified_mem_free(h);
            }
        }
    };

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(alloc_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * allocs_per_thread);
}

TEST_F(UnifiedMemoryTest, ThreadSafety_ConcurrentClone) {
    const size_t size = 4096;
    unified_mem_request_t req = {
        .size = size,
        .initial_data = nullptr,
        .strategy = UNIFIED_STRATEGY_OBJECT_COW,
        .enable_cow = true,
        .alignment = 0
    };

    unified_mem_handle_t master = unified_mem_alloc(manager_, &req);
    ASSERT_NE(master, nullptr);

    const int num_threads = 4;
    const int clones_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    auto clone_func = [master, &success_count, clones_per_thread]() {
        for (int i = 0; i < clones_per_thread; i++) {
            unified_mem_handle_t h = unified_mem_clone(master);
            if (h) {
                success_count++;
                unified_mem_free(h);
            }
        }
    };

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(clone_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * clones_per_thread);

    unified_mem_free(master);
}

TEST_F(UnifiedMemoryTest, ThreadSafety_ConcurrentReadWrite) {
    const size_t size = 64 * 1024;  // 64KB
    unified_mem_request_t req = unified_mem_request_page_cow(size, nullptr);

    unified_mem_handle_t master = unified_mem_alloc(manager_, &req);
    ASSERT_NE(master, nullptr);

    const int num_readers = 2;
    const int num_writers = 2;
    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};

    auto reader_func = [master, &stop, &read_count]() {
        while (!stop.load()) {
            const void* ptr = unified_mem_read(master);
            if (ptr) read_count++;
        }
    };

    auto writer_func = [master, &stop, &write_count]() {
        while (!stop.load()) {
            unified_mem_handle_t h = unified_mem_clone(master);
            if (h) {
                void* ptr = unified_mem_write(h);
                if (ptr) write_count++;
                unified_mem_free(h);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_readers; i++) {
        threads.emplace_back(reader_func);
    }
    for (int i = 0; i < num_writers; i++) {
        threads.emplace_back(writer_func);
    }

    // Run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(read_count.load(), 0);
    EXPECT_GT(write_count.load(), 0);

    unified_mem_free(master);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, LargeAllocation) {
    // Test 50MB allocation (typical weight matrix)
    const size_t size = 50 * 1024 * 1024;
    unified_mem_request_t req = unified_mem_request_page_cow(size, nullptr);

    unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
    ASSERT_NE(h, nullptr);

    EXPECT_EQ(unified_mem_get_strategy(h), UNIFIED_STRATEGY_PAGE_COW);
    EXPECT_EQ(unified_mem_get_size(h), size);

    // Write to first and last page
    float* ptr = static_cast<float*>(unified_mem_write(h));
    ASSERT_NE(ptr, nullptr);
    ptr[0] = 1.0f;
    ptr[(size / sizeof(float)) - 1] = 2.0f;

    unified_mem_free(h);
}

TEST_F(UnifiedMemoryTest, ManySmallAllocations) {
    const int num_allocs = 1000;
    std::vector<unified_mem_handle_t> handles;

    for (int i = 0; i < num_allocs; i++) {
        unified_mem_request_t req = unified_mem_request(256, nullptr, true);
        unified_mem_handle_t h = unified_mem_alloc(manager_, &req);
        ASSERT_NE(h, nullptr);
        handles.push_back(h);
    }

    unified_mem_stats_t stats;
    EXPECT_TRUE(unified_mem_get_stats(manager_, &stats));
    EXPECT_EQ(stats.active_handles, num_allocs);

    for (auto h : handles) {
        unified_mem_free(h);
    }
}

TEST_F(UnifiedMemoryTest, FreeAfterManagerDestroy) {
    unified_mem_config_t config = unified_mem_default_config();
    unified_mem_manager_t mgr = unified_mem_create(&config);
    ASSERT_NE(mgr, nullptr);

    unified_mem_request_t req = unified_mem_request(1024, nullptr, true);
    unified_mem_handle_t h = unified_mem_alloc(mgr, &req);
    ASSERT_NE(h, nullptr);

    // Destroy manager with active handle
    // (Should cleanup remaining handles)
    unified_mem_destroy(mgr);

    // Handle should be invalid but not crash
    // Note: This is undefined behavior but shouldn't crash
}

//=============================================================================
// Helper Function Tests
//=============================================================================

TEST_F(UnifiedMemoryTest, HelperFunctions_StrategyName) {
    EXPECT_STREQ(unified_mem_strategy_name(UNIFIED_STRATEGY_AUTO), "auto");
    EXPECT_STREQ(unified_mem_strategy_name(UNIFIED_STRATEGY_OBJECT_COW), "object_cow");
    EXPECT_STREQ(unified_mem_strategy_name(UNIFIED_STRATEGY_PAGE_COW), "page_cow");
    EXPECT_STREQ(unified_mem_strategy_name(UNIFIED_STRATEGY_POOL_DIRECT), "pool_direct");
    EXPECT_STREQ(unified_mem_strategy_name(UNIFIED_STRATEGY_MALLOC_DIRECT), "malloc_direct");
}

TEST_F(UnifiedMemoryTest, HelperFunctions_StateName) {
    EXPECT_STREQ(unified_mem_state_name(UNIFIED_STATE_SHARED), "shared");
    EXPECT_STREQ(unified_mem_state_name(UNIFIED_STATE_PRIVATE), "private");
    EXPECT_STREQ(unified_mem_state_name(UNIFIED_STATE_DIRECT), "direct");
    EXPECT_STREQ(unified_mem_state_name(UNIFIED_STATE_INVALID), "invalid");
}

TEST_F(UnifiedMemoryTest, HelperFunctions_RequestCreation) {
    unified_mem_request_t req1 = unified_mem_request(1024, nullptr, true);
    EXPECT_EQ(req1.size, 1024);
    EXPECT_EQ(req1.initial_data, nullptr);
    EXPECT_TRUE(req1.enable_cow);
    EXPECT_EQ(req1.strategy, UNIFIED_STRATEGY_AUTO);

    unified_mem_request_t req2 = unified_mem_request_direct(2048);
    EXPECT_EQ(req2.size, 2048);
    EXPECT_FALSE(req2.enable_cow);
    EXPECT_EQ(req2.strategy, UNIFIED_STRATEGY_POOL_DIRECT);

    std::vector<float> data(100);
    unified_mem_request_t req3 = unified_mem_request_page_cow(4096, data.data());
    EXPECT_EQ(req3.size, 4096);
    EXPECT_EQ(req3.initial_data, data.data());
    EXPECT_TRUE(req3.enable_cow);
    EXPECT_EQ(req3.strategy, UNIFIED_STRATEGY_PAGE_COW);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
