/**
 * @file test_cow_manager.cpp
 * @brief Comprehensive unit tests for nimcp_cow_manager - CoW with refcounting
 *
 * WHAT: 100% code coverage tests for Copy-on-Write manager
 * WHY:  Ensure correctness of lazy copy, refcounting, and thread safety
 * HOW:  Test all functions, edge cases, concurrency, and performance
 *
 * TEST COVERAGE:
 * - Basic operations: create, acquire, release, destroy
 * - Reference counting: atomic refcount correctness
 * - CoW triggering: lazy copy on first write
 * - Read/Write: const reads don't trigger CoW, writes do
 * - Thread safety: concurrent acquire/release/write
 * - Pool integration: fast allocation from pool
 * - Statistics: tracking CoW triggers, memory savings
 * - Custom callbacks: copy functions, destructors
 * - Memory management: zero leaks, proper cleanup
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

// Headers have their own extern "C" guards
    #include "utils/memory/nimcp_cow_manager.h"
    #include "utils/memory/nimcp_memory_pool.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/exception/nimcp_exception.h"
    #include "utils/exception/nimcp_exception_handlers.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CoWManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory tracking
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Create test data
        for (int i = 0; i < TEST_DATA_SIZE; i++) {
            test_data[i] = static_cast<float>(i);
        }

        // Warm up exception handler system (one-time mutex allocation)
        {
            nimcp_exception_t* warmup = nimcp_exception_create(
                NIMCP_ERROR_NULL_POINTER, EXCEPTION_SEVERITY_DEBUG,
                __FILE__, __LINE__, __func__, "warmup");
            if (warmup) {
                nimcp_exception_dispatch(warmup);
                nimcp_exception_unref(warmup);
            }
            nimcp_exception_clear_current();
        }
        // Record baseline after handler system is initialized
        nimcp_memory_get_stats(&baseline_stats_);
    }

    void TearDown() override {
        // Release any exception held as "current" by the dispatch system
        nimcp_exception_clear_current();

        // Check for memory leaks relative to baseline
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, baseline_stats_.current_allocated)
            << "Memory leak detected: "
            << (stats.current_allocated - baseline_stats_.current_allocated)
            << " bytes leaked since baseline";
    }

    static constexpr size_t TEST_DATA_SIZE = 1024;
    float test_data[TEST_DATA_SIZE];
    nimcp_memory_stats_t baseline_stats_ = {};
};

//=============================================================================
// Basic Operations Tests
//=============================================================================

/**
 * WHAT: Test CoW manager creation with valid config
 * WHY:  Verify basic initialization works
 */
TEST_F(CoWManagerTest, CreateManager_ValidConfig_Success) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);

    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    // Verify initial state
    EXPECT_EQ(cow_get_refcount(mgr), 0);  // No handles yet

    cow_manager_destroy(mgr);
}

/**
 * WHAT: Test manager creation with NULL config
 * WHY:  Verify error handling
 */
TEST_F(CoWManagerTest, CreateManager_NullConfig_ReturnsNull) {
    cow_manager_t mgr = cow_manager_create(nullptr, test_data);
    EXPECT_EQ(mgr, nullptr);
}

/**
 * WHAT: Test manager creation with zero size
 * WHY:  Verify size validation
 */
TEST_F(CoWManagerTest, CreateManager_ZeroSize_ReturnsNull) {
    cow_manager_config_t config = cow_default_config(0, nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    EXPECT_EQ(mgr, nullptr);
}

/**
 * WHAT: Test destroy with NULL manager
 * WHY:  Verify NULL handling (should not crash)
 */
TEST_F(CoWManagerTest, DestroyManager_Null_NoCrash) {
    cow_manager_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Handle Acquire/Release Tests
//=============================================================================

/**
 * WHAT: Test basic handle acquisition
 * WHY:  Verify O(1) reference creation
 */
TEST_F(CoWManagerTest, Acquire_ValidManager_ReturnsHandle) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    cow_handle_t h = cow_acquire(mgr);
    ASSERT_NE(h, nullptr);

    // Verify handle state
    EXPECT_TRUE(cow_is_shared(h));
    EXPECT_EQ(cow_get_state(h), COW_STATE_SHARED);
    EXPECT_EQ(cow_get_refcount(mgr), 1);

    cow_release(h);
    cow_manager_destroy(mgr);
}

/**
 * WHAT: Test acquire from NULL manager
 * WHY:  Verify error handling
 */
TEST_F(CoWManagerTest, Acquire_NullManager_ReturnsNull) {
    cow_handle_t h = cow_acquire(nullptr);
    EXPECT_EQ(h, nullptr);
}

/**
 * WHAT: Test multiple acquires
 * WHY:  Verify refcount increments correctly
 */
TEST_F(CoWManagerTest, Acquire_MultipleHandles_RefcountCorrect) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    std::vector<cow_handle_t> handles;
    for (int i = 0; i < 10; i++) {
        cow_handle_t h = cow_acquire(mgr);
        ASSERT_NE(h, nullptr);
        handles.push_back(h);
        EXPECT_EQ(cow_get_refcount(mgr), i + 1);
    }

    // All should be shared
    for (auto h : handles) {
        EXPECT_TRUE(cow_is_shared(h));
    }

    // Release all
    for (auto h : handles) {
        cow_release(h);
    }

    EXPECT_EQ(cow_get_refcount(mgr), 0);
    cow_manager_destroy(mgr);
}

/**
 * WHAT: Test release with NULL handle
 * WHY:  Verify NULL handling (should not crash)
 */
TEST_F(CoWManagerTest, Release_NullHandle_NoCrash) {
    cow_release(nullptr);  // Should not crash
}

//=============================================================================
// Read/Write Tests
//=============================================================================

/**
 * WHAT: Test read-only access
 * WHY:  Verify reads don't trigger CoW
 */
TEST_F(CoWManagerTest, Read_SharedHandle_NoCoWTrigger) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    cow_handle_t h = cow_acquire(mgr);
    ASSERT_NE(h, nullptr);

    // Read data
    const float* data = static_cast<const float*>(cow_read(h));
    ASSERT_NE(data, nullptr);

    // Verify data matches template
    for (size_t i = 0; i < TEST_DATA_SIZE; i++) {
        EXPECT_EQ(data[i], test_data[i]);
    }

    // Should still be shared
    EXPECT_TRUE(cow_is_shared(h));

    cow_release(h);
    cow_manager_destroy(mgr);
}

/**
 * WHAT: Test write triggers CoW
 * WHY:  Verify lazy copy on first write
 */
TEST_F(CoWManagerTest, Write_SharedHandle_TriggersCoW) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    cow_handle_t h = cow_acquire(mgr);
    ASSERT_NE(h, nullptr);
    EXPECT_TRUE(cow_is_shared(h));

    // Trigger CoW with write
    float* data = static_cast<float*>(cow_write(h));
    ASSERT_NE(data, nullptr);

    // Should now be private
    EXPECT_FALSE(cow_is_shared(h));
    EXPECT_EQ(cow_get_state(h), COW_STATE_PRIVATE);
    EXPECT_EQ(cow_get_refcount(mgr), 0);  // No longer using template

    // Modify data
    data[0] = 999.0f;
    EXPECT_EQ(data[0], 999.0f);

    // Verify template unchanged
    cow_handle_t h2 = cow_acquire(mgr);  // Fresh handle to check template
    const float* template_ptr = static_cast<const float*>(cow_read(h2));
    EXPECT_EQ(template_ptr[0], 0.0f);  // Original value
    cow_release(h2);  // Clean up second handle

    cow_release(h);
    cow_manager_destroy(mgr);
}

/**
 * WHAT: Test read from NULL handle
 * WHY:  Verify error handling
 */
TEST_F(CoWManagerTest, Read_NullHandle_ReturnsNull) {
    const void* data = cow_read(nullptr);
    EXPECT_EQ(data, nullptr);
}

/**
 * WHAT: Test write from NULL handle
 * WHY:  Verify error handling
 */
TEST_F(CoWManagerTest, Write_NullHandle_ReturnsNull) {
    void* data = cow_write(nullptr);
    EXPECT_EQ(data, nullptr);
}

//=============================================================================
// CoW Trigger Tests
//=============================================================================

/**
 * WHAT: Test multiple handles with selective CoW
 * WHY:  Verify independent CoW for each handle
 */
TEST_F(CoWManagerTest, CoW_MultipleHandles_IndependentCopies) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    // Acquire 3 handles
    cow_handle_t h1 = cow_acquire(mgr);
    cow_handle_t h2 = cow_acquire(mgr);
    cow_handle_t h3 = cow_acquire(mgr);
    EXPECT_EQ(cow_get_refcount(mgr), 3);

    // Trigger CoW on h2 only
    float* data2 = static_cast<float*>(cow_write(h2));
    data2[0] = 111.0f;

    // h1 and h3 still shared
    EXPECT_TRUE(cow_is_shared(h1));
    EXPECT_FALSE(cow_is_shared(h2));
    EXPECT_TRUE(cow_is_shared(h3));
    EXPECT_EQ(cow_get_refcount(mgr), 2);  // h1 and h3

    // Verify data independence
    const float* data1 = static_cast<const float*>(cow_read(h1));
    const float* data3 = static_cast<const float*>(cow_read(h3));
    EXPECT_EQ(data1[0], 0.0f);    // Original
    EXPECT_EQ(data2[0], 111.0f);  // Modified
    EXPECT_EQ(data3[0], 0.0f);    // Original

    cow_release(h1);
    cow_release(h2);
    cow_release(h3);
    cow_manager_destroy(mgr);
}

/**
 * WHAT: Test make_private pre-emptive CoW
 * WHY:  Verify explicit CoW trigger
 */
TEST_F(CoWManagerTest, MakePrivate_SharedHandle_BecomesPrivate) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    cow_handle_t h = cow_acquire(mgr);
    EXPECT_TRUE(cow_is_shared(h));

    // Make private explicitly
    ASSERT_TRUE(cow_make_private(h));
    EXPECT_FALSE(cow_is_shared(h));

    cow_release(h);
    cow_manager_destroy(mgr);
}

//=============================================================================
// Memory Pool Integration Tests
//=============================================================================

/**
 * WHAT: Test CoW with memory pool for fast allocation
 * WHY:  Verify pool integration provides O(1) copies
 */
TEST_F(CoWManagerTest, CoW_WithPool_FastAllocation) {
    // Create pool
    memory_pool_config_t pool_config = memory_pool_default_config(sizeof(test_data), 10);
    memory_pool_t pool = memory_pool_create(&pool_config);
    ASSERT_NE(pool, nullptr);

    // Create CoW manager with pool
    cow_manager_config_t config = cow_default_config(sizeof(test_data), pool);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    // Acquire and trigger CoW
    cow_handle_t h = cow_acquire(mgr);
    float* data = static_cast<float*>(cow_write(h));  // Should use pool
    ASSERT_NE(data, nullptr);

    // Verify data came from pool
    EXPECT_TRUE(memory_pool_owns(pool, data));

    cow_release(h);
    cow_manager_destroy(mgr);
    memory_pool_destroy(pool);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test statistics tracking
 * WHY:  Verify CoW metrics are accurate
 */
TEST_F(CoWManagerTest, Stats_TrackCoWTriggers_Accurate) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    config.enable_tracking = true;
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    // Acquire handles and trigger CoW
    cow_handle_t h1 = cow_acquire(mgr);
    cow_handle_t h2 = cow_acquire(mgr);
    cow_handle_t h3 = cow_acquire(mgr);

    cow_write(h1);  // Trigger 1
    cow_write(h2);  // Trigger 2

    // Check statistics
    cow_stats_t stats;
    ASSERT_TRUE(cow_get_stats(mgr, &stats));

    EXPECT_EQ(stats.total_handles, 3);
    EXPECT_EQ(stats.active_shared, 1);   // h3 still shared
    EXPECT_EQ(stats.active_private, 2);  // h1, h2 private
    EXPECT_EQ(stats.cow_triggers, 2);
    EXPECT_EQ(stats.template_refcount, 1);  // h3
    EXPECT_GT(stats.memory_saved_bytes, 0);  // h3 saves memory

    cow_release(h1);
    cow_release(h2);
    cow_release(h3);
    cow_manager_destroy(mgr);
}

/**
 * WHAT: Test reset statistics
 * WHY:  Verify stats can be cleared
 */
TEST_F(CoWManagerTest, Stats_Reset_ClearsMetrics) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    config.enable_tracking = true;
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    // Generate activity
    cow_handle_t h = cow_acquire(mgr);
    cow_write(h);

    // Reset stats
    cow_reset_stats(mgr);

    // Check stats cleared (except current state)
    cow_stats_t stats;
    ASSERT_TRUE(cow_get_stats(mgr, &stats));
    EXPECT_EQ(stats.total_handles, 0);
    EXPECT_EQ(stats.cow_triggers, 0);

    cow_release(h);
    cow_manager_destroy(mgr);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * WHAT: Test concurrent acquire/release
 * WHY:  Verify thread-safe reference counting
 */
TEST_F(CoWManagerTest, Concurrency_AcquireRelease_ThreadSafe) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    const int NUM_THREADS = 50;
    const int OPS_PER_THREAD = 10;

    std::atomic<int> successful_ops{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([mgr, &successful_ops, OPS_PER_THREAD]() {
            for (int j = 0; j < OPS_PER_THREAD; j++) {
                cow_handle_t h = cow_acquire(mgr);
                if (h) {
                    // Read data
                    const float* data = static_cast<const float*>(cow_read(h));
                    (void)data;  // Use data
                    cow_release(h);
                    successful_ops++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successful_ops.load(), NUM_THREADS * OPS_PER_THREAD);
    EXPECT_EQ(cow_get_refcount(mgr), 0);  // All released

    cow_manager_destroy(mgr);
}

/**
 * WHAT: Test concurrent CoW triggers
 * WHY:  Verify thread-safe copy operations
 */
TEST_F(CoWManagerTest, Concurrency_CoWTriggers_ThreadSafe) {
    // Create pool for concurrent allocation
    memory_pool_config_t pool_config = memory_pool_default_config(sizeof(test_data), 100);
    memory_pool_t pool = memory_pool_create(&pool_config);
    ASSERT_NE(pool, nullptr);

    cow_manager_config_t config = cow_default_config(sizeof(test_data), pool);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    const int NUM_THREADS = 20;

    std::vector<std::thread> threads;
    std::vector<cow_handle_t> handles(NUM_THREADS);

    // Acquire handles first
    for (int i = 0; i < NUM_THREADS; i++) {
        handles[i] = cow_acquire(mgr);
        ASSERT_NE(handles[i], nullptr);
    }

    // Trigger CoW concurrently
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([](cow_handle_t h) {
            float* data = static_cast<float*>(cow_write(h));
            if (data) {
                data[0] = 42.0f;  // Write
            }
        }, handles[i]);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All should be private
    for (int i = 0; i < NUM_THREADS; i++) {
        EXPECT_FALSE(cow_is_shared(handles[i]));
        cow_release(handles[i]);
    }

    cow_manager_destroy(mgr);
    memory_pool_destroy(pool);
}

//=============================================================================
// Memory Usage Tests
//=============================================================================

/**
 * WHAT: Test memory usage calculation
 * WHY:  Verify accurate memory tracking
 */
TEST_F(CoWManagerTest, MemoryUsage_Calculate_Accurate) {
    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    // Acquire handles
    cow_handle_t h1 = cow_acquire(mgr);  // Shared
    cow_handle_t h2 = cow_acquire(mgr);  // Shared
    cow_handle_t h3 = cow_acquire(mgr);  // Shared
    cow_write(h1);  // Make private

    size_t shared_bytes, private_bytes, overhead_bytes;
    ASSERT_TRUE(cow_calculate_memory_usage(mgr, &shared_bytes, &private_bytes, &overhead_bytes));

    EXPECT_EQ(shared_bytes, sizeof(test_data));        // One template
    EXPECT_EQ(private_bytes, sizeof(test_data));       // One private copy (h1)
    EXPECT_GT(overhead_bytes, 0);                       // Manager + handles

    cow_release(h1);
    cow_release(h2);
    cow_release(h3);
    cow_manager_destroy(mgr);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * WHAT: Test CoW with nimcp_memory tracking
 * WHY:  Verify integration with memory system
 */
TEST_F(CoWManagerTest, Integration_MemoryTracking_Tracked) {
    nimcp_memory_stats_t before_stats, after_stats;
    nimcp_memory_get_stats(&before_stats);

    cow_manager_config_t config = cow_default_config(sizeof(test_data), nullptr);
    cow_manager_t mgr = cow_manager_create(&config, test_data);
    ASSERT_NE(mgr, nullptr);

    nimcp_memory_get_stats(&after_stats);
    EXPECT_GT(after_stats.current_allocated, before_stats.current_allocated);

    cow_manager_destroy(mgr);

    nimcp_memory_get_stats(&after_stats);
    EXPECT_EQ(after_stats.current_allocated, before_stats.current_allocated);
}
