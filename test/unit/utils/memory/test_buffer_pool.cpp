//=============================================================================
// test_buffer_pool.cpp - Buffer Pool Unit Tests
//=============================================================================
/**
 * @file test_buffer_pool.cpp
 * @brief Comprehensive unit tests for buffer pool with CoW + Memory Pool
 *
 * WHAT: Tests buffer pool combining CoW managers and memory pools
 * WHY:  Verify 10x memory savings for sparse channel allocation patterns
 * HOW:  Test CoW behavior, pool integration, statistics, thread safety
 *
 * TEST COVERAGE:
 * 1. Creation/destruction
 * 2. Buffer acquisition (integration, window, accumulator)
 * 3. CoW behavior (shared vs private)
 * 4. Memory pool integration
 * 5. Statistics tracking
 * 6. Channel reset
 * 7. Memory leak detection
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_buffer_pool.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BufferPoolTest : public ::testing::Test {
protected:
    nimcp_memory_stats_t baseline_stats;

    void SetUp() override {
        nimcp_memory_init();
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
        // Get baseline stats AFTER initialization
        nimcp_memory_get_stats(&baseline_stats);
    }

    void TearDown() override {
        // Release any exception held as "current" by the dispatch system
        nimcp_exception_clear_current();

        // Check that we're back to baseline (only infrastructure allocations remain)
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, baseline_stats.current_allocated)
            << "Memory leak detected: " << (stats.current_allocated - baseline_stats.current_allocated) << " bytes leaked";

        // Note: don't call nimcp_memory_cleanup() between tests as it
        // destroys subsystem state (exception handler mutex) causing deadlocks
    }

    // Helper: Create default config
    buffer_pool_config_t default_config() {
        return buffer_pool_default_config(
            100,   // fast_size
            500,   // medium_size
            2500,  // slow_size
            1000,  // max_channels
            100    // expected_active
        );
    }
};

//=============================================================================
// Creation/Destruction Tests
//=============================================================================

/**
 * WHAT: Test pool creation with valid config
 * WHY:  Verify basic pool initialization
 */
TEST_F(BufferPoolTest, Create_ValidConfig_Success) {
    buffer_pool_config_t config = default_config();
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);
    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test pool creation with NULL config
 * WHY:  Verify error handling
 */
TEST_F(BufferPoolTest, Create_NullConfig_ReturnsNull) {
    buffer_pool_t pool = buffer_pool_create(nullptr);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test pool creation with zero channels
 * WHY:  Verify input validation
 */
TEST_F(BufferPoolTest, Create_ZeroChannels_ReturnsNull) {
    buffer_pool_config_t config = default_config();
    config.max_channels = 0;
    buffer_pool_t pool = buffer_pool_create(&config);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test pool destruction with NULL
 * WHY:  Verify null safety
 */
TEST_F(BufferPoolTest, Destroy_Null_NoCrash) {
    buffer_pool_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Buffer Acquisition Tests
//=============================================================================

/**
 * WHAT: Test acquiring integration buffer
 * WHY:  Verify basic allocation
 */
TEST_F(BufferPoolTest, Acquire_IntegrationBuffer_Success) {
    buffer_pool_config_t config = default_config();
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    integration_buffer_t buf = buffer_pool_acquire_integration_buffer(pool, 0, false);
    EXPECT_NE(buf, nullptr);

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test acquiring sliding window
 * WHY:  Verify window allocation
 */
TEST_F(BufferPoolTest, Acquire_SlidingWindow_Success) {
    buffer_pool_config_t config = default_config();
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    sliding_window_t window = buffer_pool_acquire_sliding_window(pool, 0, false);
    EXPECT_NE(window, nullptr);

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test acquiring temporal accumulator
 * WHY:  Verify accumulator allocation
 */
TEST_F(BufferPoolTest, Acquire_TemporalAccumulator_Success) {
    buffer_pool_config_t config = default_config();
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    temporal_accumulator_t acc = buffer_pool_acquire_temporal_accumulator(pool, 0, false);
    EXPECT_NE(acc, nullptr);

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test acquiring buffer with NULL pool
 * WHY:  Verify error handling
 */
TEST_F(BufferPoolTest, Acquire_NullPool_ReturnsNull) {
    integration_buffer_t buf = buffer_pool_acquire_integration_buffer(nullptr, 0, false);
    EXPECT_EQ(buf, nullptr);
}

/**
 * WHAT: Test acquiring buffer with invalid channel
 * WHY:  Verify bounds checking
 */
TEST_F(BufferPoolTest, Acquire_InvalidChannel_ReturnsNull) {
    buffer_pool_config_t config = default_config();
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    integration_buffer_t buf = buffer_pool_acquire_integration_buffer(pool, 9999, false);
    EXPECT_EQ(buf, nullptr);

    buffer_pool_destroy(pool);
}

//=============================================================================
// CoW Behavior Tests
//=============================================================================

/**
 * WHAT: Test shared buffer acquisition (CoW enabled)
 * WHY:  Verify CoW creates shared references
 */
TEST_F(BufferPoolTest, CoW_SharedBuffer_IsShared) {
    buffer_pool_config_t config = default_config();
    config.enable_cow = true;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire buffers without requesting private copies
    integration_buffer_t buf1 = buffer_pool_acquire_integration_buffer(pool, 0, false);
    integration_buffer_t buf2 = buffer_pool_acquire_integration_buffer(pool, 1, false);

    EXPECT_NE(buf1, nullptr);
    EXPECT_NE(buf2, nullptr);

    // Both should be shared
    EXPECT_TRUE(buffer_pool_is_channel_shared(pool, 0));
    EXPECT_TRUE(buffer_pool_is_channel_shared(pool, 1));

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test private buffer acquisition triggers CoW
 * WHY:  Verify CoW copy-on-write behavior
 */
TEST_F(BufferPoolTest, CoW_PrivateBuffer_TriggersCoW) {
    buffer_pool_config_t config = default_config();
    config.enable_cow = true;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire shared buffer first
    integration_buffer_t buf = buffer_pool_acquire_integration_buffer(pool, 0, false);
    EXPECT_TRUE(buffer_pool_is_channel_shared(pool, 0));

    // Request private copy
    integration_buffer_t buf_private = buffer_pool_acquire_integration_buffer(pool, 0, true);
    EXPECT_NE(buf_private, nullptr);

    // Should now be private
    EXPECT_FALSE(buffer_pool_is_channel_shared(pool, 0));

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test manual CoW trigger
 * WHY:  Verify explicit make_private operation
 */
TEST_F(BufferPoolTest, CoW_MakePrivate_BecomesPrivate) {
    buffer_pool_config_t config = default_config();
    config.enable_cow = true;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire shared buffer
    buffer_pool_acquire_integration_buffer(pool, 0, false);
    EXPECT_TRUE(buffer_pool_is_channel_shared(pool, 0));

    // Manually make private
    bool result = buffer_pool_cow_make_private(pool, 0);
    EXPECT_TRUE(result);
    EXPECT_FALSE(buffer_pool_is_channel_shared(pool, 0));

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test CoW disabled mode (all private)
 * WHY:  Verify fallback to direct allocation
 */
TEST_F(BufferPoolTest, CoW_Disabled_AllPrivate) {
    buffer_pool_config_t config = default_config();
    config.enable_cow = false;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    integration_buffer_t buf1 = buffer_pool_acquire_integration_buffer(pool, 0, false);
    integration_buffer_t buf2 = buffer_pool_acquire_integration_buffer(pool, 1, false);

    EXPECT_NE(buf1, nullptr);
    EXPECT_NE(buf2, nullptr);

    // Should not be shared (CoW disabled)
    EXPECT_FALSE(buffer_pool_is_channel_shared(pool, 0));
    EXPECT_FALSE(buffer_pool_is_channel_shared(pool, 1));

    buffer_pool_destroy(pool);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test statistics tracking
 * WHY:  Verify stats collection
 */
TEST_F(BufferPoolTest, Stats_Tracking_Accurate) {
    buffer_pool_config_t config = default_config();
    config.enable_cow = true;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    buffer_pool_stats_t stats;

    // Acquire some buffers
    buffer_pool_acquire_integration_buffer(pool, 0, false);  // Shared
    buffer_pool_acquire_integration_buffer(pool, 1, false);  // Shared
    buffer_pool_acquire_integration_buffer(pool, 2, true);   // Private (triggers CoW)

    bool result = buffer_pool_get_stats(pool, &stats);
    EXPECT_TRUE(result);

    EXPECT_TRUE(stats.cow_enabled);
    EXPECT_GT(stats.cow_triggers, 0);  // At least one CoW trigger

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test memory savings calculation
 * WHY:  Verify CoW memory efficiency
 */
TEST_F(BufferPoolTest, Stats_MemorySavings_Calculated) {
    buffer_pool_config_t config = default_config();
    config.enable_cow = true;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire many shared channels
    for (size_t i = 0; i < 50; i++) {
        buffer_pool_acquire_integration_buffer(pool, i, false);
    }

    buffer_pool_stats_t stats;
    buffer_pool_get_stats(pool, &stats);

    // Should have many shared channels
    EXPECT_GT(stats.shared_channels, 0);
    // Memory savings should be significant
    EXPECT_GT(stats.memory_savings_bytes, 0);

    buffer_pool_destroy(pool);
}

//=============================================================================
// Reset Tests
//=============================================================================

/**
 * WHAT: Test pool reset
 * WHY:  Verify all channels released
 */
TEST_F(BufferPoolTest, Reset_ActiveChannels_Released) {
    buffer_pool_config_t config = default_config();
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire some channels
    buffer_pool_acquire_integration_buffer(pool, 0, false);
    buffer_pool_acquire_integration_buffer(pool, 1, false);
    buffer_pool_acquire_integration_buffer(pool, 2, false);

    // Reset pool
    size_t reset_count = buffer_pool_reset(pool);
    EXPECT_EQ(reset_count, 3);

    // Can reacquire channels
    integration_buffer_t buf = buffer_pool_acquire_integration_buffer(pool, 0, false);
    EXPECT_NE(buf, nullptr);

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test reset with NULL pool
 * WHY:  Verify null safety
 */
TEST_F(BufferPoolTest, Reset_NullPool_ReturnsZero) {
    size_t count = buffer_pool_reset(nullptr);
    EXPECT_EQ(count, 0);
}

//=============================================================================
// Memory Usage Tests
//=============================================================================

/**
 * WHAT: Test memory calculation
 * WHY:  Verify memory estimation
 */
TEST_F(BufferPoolTest, Memory_Calculate_Accurate) {
    buffer_pool_config_t config = default_config();

    // With CoW - should be much smaller
    config.enable_cow = true;
    size_t cow_memory = buffer_pool_calculate_memory(&config);

    // Without CoW - should be larger
    config.enable_cow = false;
    size_t no_cow_memory = buffer_pool_calculate_memory(&config);

    EXPECT_GT(no_cow_memory, cow_memory);
    EXPECT_GT(cow_memory, 0);
}

/**
 * WHAT: Test memory calculation with NULL
 * WHY:  Verify error handling
 */
TEST_F(BufferPoolTest, Memory_Calculate_Null_ReturnsZero) {
    size_t memory = buffer_pool_calculate_memory(nullptr);
    EXPECT_EQ(memory, 0);
}

//=============================================================================
// Concurrency Tests
//=============================================================================

/**
 * WHAT: Test concurrent buffer acquisition
 * WHY:  Verify thread safety
 */
TEST_F(BufferPoolTest, Concurrency_AcquireBuffers_ThreadSafe) {
    buffer_pool_config_t config = default_config();
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    constexpr int NUM_THREADS = 10;
    constexpr int CHANNELS_PER_THREAD = 10;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([pool, t, &success_count]() {
            for (int i = 0; i < CHANNELS_PER_THREAD; i++) {
                size_t channel_id = t * CHANNELS_PER_THREAD + i;
                integration_buffer_t buf = buffer_pool_acquire_integration_buffer(pool, channel_id, false);
                if (buf != nullptr) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * CHANNELS_PER_THREAD);

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test concurrent CoW triggers
 * WHY:  Verify thread-safe CoW operations
 */
TEST_F(BufferPoolTest, Concurrency_CoWTriggers_ThreadSafe) {
    buffer_pool_config_t config = default_config();
    config.enable_cow = true;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    constexpr int NUM_THREADS = 10;

    // Pre-acquire shared buffers
    for (int i = 0; i < NUM_THREADS; i++) {
        buffer_pool_acquire_integration_buffer(pool, i, false);
    }

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Trigger CoW concurrently
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([pool, t, &success_count]() {
            bool result = buffer_pool_cow_make_private(pool, t);
            if (result) {
                success_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS);

    buffer_pool_destroy(pool);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * WHAT: Test buffer pool with memory tracking
 * WHY:  Verify integration with nimcp_memory
 */
TEST_F(BufferPoolTest, Integration_MemoryTracking_Tracked) {
    nimcp_memory_stats_t before, after, final;
    nimcp_memory_get_stats(&before);

    buffer_pool_config_t config = default_config();
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    nimcp_memory_get_stats(&after);
    EXPECT_GT(after.current_allocated, before.current_allocated);

    buffer_pool_destroy(pool);

    nimcp_memory_get_stats(&final);
    EXPECT_EQ(final.current_allocated, before.current_allocated);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
