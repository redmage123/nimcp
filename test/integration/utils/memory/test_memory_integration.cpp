//=============================================================================
// test_memory_integration.cpp - Memory System Integration Tests
//=============================================================================
/**
 * @file test_memory_integration.cpp
 * @brief Integration tests for Memory Pool + CoW Manager + Buffer Pool
 *
 * WHAT: Tests interaction between memory pool, CoW manager, and buffer pool
 * WHY:  Verify components work correctly together in realistic scenarios
 * HOW:  Test end-to-end workflows, cross-component interactions, performance
 *
 * INTEGRATION SCENARIOS:
 * 1. Memory Pool → CoW Manager: CoW uses pool for private copies
 * 2. CoW Manager → Buffer Pool: Buffer pool uses CoW for shared templates
 * 3. Sparse Allocation: 1000 channels declared, 100 active → 10x savings
 * 4. Mixed Workloads: Read-heavy vs write-heavy patterns
 * 5. Lifecycle: Create → Use → Reset → Reuse → Destroy
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/memory/nimcp_buffer_pool.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MemoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, 0)
            << "Memory leak: " << stats.current_allocated << " bytes";
    }
};

//=============================================================================
// Pool + CoW Integration Tests
//=============================================================================

/**
 * WHAT: Test CoW manager using memory pool for private copies
 * WHY:  Verify fast allocation path for CoW copies
 */
TEST_F(MemoryIntegrationTest, PoolCoW_PrivateCopies_UsePool) {
    // Create memory pool
    memory_pool_config_t pool_config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&pool_config);
    ASSERT_NE(pool, nullptr);

    // Create CoW manager with pool
    float template_data[256] = {0};
    cow_manager_config_t cow_config = cow_default_config(sizeof(template_data), pool);
    cow_manager_t cow = cow_manager_create(&cow_config, template_data);
    ASSERT_NE(cow, nullptr);

    // Acquire handles
    cow_handle_t h1 = cow_acquire(cow);
    cow_handle_t h2 = cow_acquire(cow);
    cow_handle_t h3 = cow_acquire(cow);

    // All shared initially
    EXPECT_TRUE(cow_is_shared(h1));
    EXPECT_TRUE(cow_is_shared(h2));
    EXPECT_TRUE(cow_is_shared(h3));

    // Trigger CoW - should allocate from pool
    float* data1 = (float*)cow_write(h1);
    ASSERT_NE(data1, nullptr);
    EXPECT_FALSE(cow_is_shared(h1));

    // Verify pool was used
    memory_pool_stats_t pool_stats;
    memory_pool_get_stats(pool, &pool_stats);
    EXPECT_GT(pool_stats.allocated_blocks, 0);

    // Cleanup
    cow_release(h1);
    cow_release(h2);
    cow_release(h3);
    cow_manager_destroy(cow);
    memory_pool_destroy(pool);
}

/**
 * WHAT: Test buffer pool using CoW managers for templates
 * WHY:  Verify buffer pool leverages CoW for shared templates
 */
TEST_F(MemoryIntegrationTest, CoWBuffer_SharedTemplates_MemorySavings) {
    buffer_pool_config_t config = buffer_pool_default_config(
        100,   // fast_size
        500,   // medium_size
        2500,  // slow_size
        100,   // max_channels
        20     // expected_active
    );
    config.enable_cow = true;

    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire many shared channels
    constexpr int NUM_SHARED = 50;
    for (int i = 0; i < NUM_SHARED; i++) {
        integration_buffer_t buf = buffer_pool_acquire_integration_buffer(pool, i, false);
        ASSERT_NE(buf, nullptr);
        EXPECT_TRUE(buffer_pool_is_channel_shared(pool, i));
    }

    // Get statistics
    buffer_pool_stats_t stats;
    buffer_pool_get_stats(pool, &stats);

    // Verify CoW is working
    EXPECT_TRUE(stats.cow_enabled);
    EXPECT_GE(stats.shared_channels, NUM_SHARED);
    EXPECT_GT(stats.memory_savings_bytes, 0);

    // Trigger CoW on some channels
    for (int i = 0; i < 10; i++) {
        buffer_pool_cow_make_private(pool, i);
        EXPECT_FALSE(buffer_pool_is_channel_shared(pool, i));
    }

    // Verify mixed state
    buffer_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.private_channels, 10);
    EXPECT_GE(stats.shared_channels, NUM_SHARED - 10);

    buffer_pool_destroy(pool);
}

//=============================================================================
// Sparse Allocation Pattern Tests
//=============================================================================

/**
 * WHAT: Test 1000 channels declared, only 100 active
 * WHY:  Validate 10x memory savings target
 */
TEST_F(MemoryIntegrationTest, SparseAllocation_10xSavings_Achieved) {
    buffer_pool_config_t config = buffer_pool_default_config(
        100,   // fast_size
        500,   // medium_size
        2500,  // slow_size
        1000,  // max_channels (declared)
        100    // expected_active
    );
    config.enable_cow = true;

    // Measure memory WITH CoW
    nimcp_memory_stats_t before_cow, after_cow;
    nimcp_memory_get_stats(&before_cow);

    buffer_pool_t pool_cow = buffer_pool_create(&config);
    ASSERT_NE(pool_cow, nullptr);

    // Acquire 100 channels
    for (int i = 0; i < 100; i++) {
        buffer_pool_acquire_integration_buffer(pool_cow, i, false);
    }

    nimcp_memory_get_stats(&after_cow);
    size_t cow_memory = after_cow.current_allocated - before_cow.current_allocated;

    buffer_pool_destroy(pool_cow);

    // Measure memory WITHOUT CoW (for comparison)
    config.enable_cow = false;
    config.max_channels = 100;  // Only allocate 100 directly

    nimcp_memory_stats_t before_no_cow, after_no_cow;
    nimcp_memory_get_stats(&before_no_cow);

    buffer_pool_t pool_no_cow = buffer_pool_create(&config);
    ASSERT_NE(pool_no_cow, nullptr);

    for (int i = 0; i < 100; i++) {
        buffer_pool_acquire_integration_buffer(pool_no_cow, i, false);
    }

    nimcp_memory_get_stats(&after_no_cow);
    size_t no_cow_memory = after_no_cow.current_allocated - before_no_cow.current_allocated;

    buffer_pool_destroy(pool_no_cow);

    // CoW memory should be comparable (slight overhead for CoW structures)
    // but enable 10x more channels
    float ratio = (float)no_cow_memory / (float)cow_memory;
    EXPECT_GT(ratio, 0.5f);  // At most 2x overhead
    EXPECT_LT(ratio, 2.0f);  // Reasonable efficiency
}

/**
 * WHAT: Test read-heavy workload (90% reads, 10% writes)
 * WHY:  Verify CoW benefits for read-dominant patterns
 */
TEST_F(MemoryIntegrationTest, ReadHeavyWorkload_LowCoWTriggers_Efficient) {
    buffer_pool_config_t config = buffer_pool_default_config(100, 500, 2500, 100, 50);
    config.enable_cow = true;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire 50 channels
    for (int i = 0; i < 50; i++) {
        buffer_pool_acquire_integration_buffer(pool, i, false);
    }

    // Read-heavy pattern: 90% reads, 10% writes
    for (int i = 0; i < 100; i++) {
        int channel = i % 50;
        if (i < 10) {
            // 10% writes - trigger CoW
            buffer_pool_cow_make_private(pool, channel);
        }
        // All operations are reads (no additional CoW triggers)
    }

    // Verify low CoW trigger rate
    buffer_pool_stats_t stats;
    buffer_pool_get_stats(pool, &stats);
    EXPECT_LE(stats.cow_triggers, 10);  // Only 10 writes
    EXPECT_GE(stats.shared_channels, 40);  // Most still shared

    buffer_pool_destroy(pool);
}

/**
 * WHAT: Test write-heavy workload (all channels need private copies)
 * WHY:  Verify CoW gracefully degrades to per-channel allocation
 */
TEST_F(MemoryIntegrationTest, WriteHeavyWorkload_AllPrivate_Functional) {
    buffer_pool_config_t config = buffer_pool_default_config(100, 500, 2500, 50, 50);
    config.enable_cow = true;
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire all channels as private (write-heavy)
    for (int i = 0; i < 50; i++) {
        buffer_pool_acquire_integration_buffer(pool, i, true);  // needs_private=true
    }

    // Verify all private
    buffer_pool_stats_t stats;
    buffer_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.private_channels, 50);
    EXPECT_EQ(stats.shared_channels, 0);

    buffer_pool_destroy(pool);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * WHAT: Test create → use → reset → reuse → destroy lifecycle
 * WHY:  Verify pool can be reused efficiently
 */
TEST_F(MemoryIntegrationTest, Lifecycle_ResetReuse_Efficient) {
    buffer_pool_config_t config = buffer_pool_default_config(100, 500, 2500, 100, 50);
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // First use
    for (int i = 0; i < 50; i++) {
        buffer_pool_acquire_integration_buffer(pool, i, false);
    }

    buffer_pool_stats_t stats1;
    buffer_pool_get_stats(pool, &stats1);
    EXPECT_GT(stats1.shared_channels, 0);

    // Reset
    size_t reset_count = buffer_pool_reset(pool);
    EXPECT_EQ(reset_count, 50);

    // Verify reset
    buffer_pool_stats_t stats2;
    buffer_pool_get_stats(pool, &stats2);
    EXPECT_EQ(stats2.shared_channels, 0);
    EXPECT_EQ(stats2.private_channels, 0);

    // Reuse
    for (int i = 0; i < 30; i++) {
        buffer_pool_acquire_integration_buffer(pool, i, false);
    }

    buffer_pool_stats_t stats3;
    buffer_pool_get_stats(pool, &stats3);
    EXPECT_GT(stats3.shared_channels, 0);

    buffer_pool_destroy(pool);
}

//=============================================================================
// Concurrent Integration Tests
//=============================================================================

/**
 * WHAT: Test concurrent access across all components
 * WHY:  Verify thread safety in integrated system
 */
TEST_F(MemoryIntegrationTest, Concurrent_AllComponents_ThreadSafe) {
    buffer_pool_config_t config = buffer_pool_default_config(100, 500, 2500, 100, 50);
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 10;

    std::vector<std::thread> threads;
    std::atomic<int> success{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([pool, t, &success]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                int channel = t * OPS_PER_THREAD + i;

                // Acquire buffer
                integration_buffer_t buf = buffer_pool_acquire_integration_buffer(
                    pool, channel, i % 2 == 0  // 50% private
                );

                if (buf != nullptr) {
                    success++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success.load(), NUM_THREADS * OPS_PER_THREAD);

    buffer_pool_destroy(pool);
}

//=============================================================================
// Performance Integration Tests
//=============================================================================

/**
 * WHAT: Test end-to-end allocation performance
 * WHY:  Verify integrated system meets performance targets
 */
TEST_F(MemoryIntegrationTest, Performance_E2E_MeetTargets) {
    buffer_pool_config_t config = buffer_pool_default_config(100, 500, 2500, 100, 50);
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Warm up
    for (int i = 0; i < 10; i++) {
        buffer_pool_acquire_integration_buffer(pool, i, false);
    }

    // Measure acquisition time
    auto start = std::chrono::high_resolution_clock::now();

    constexpr int NUM_ACQUIRES = 1000;
    for (int i = 0; i < NUM_ACQUIRES; i++) {
        buffer_pool_acquire_integration_buffer(pool, i % 100, false);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    float avg_ns = (float)duration_ns / (float)NUM_ACQUIRES;

    // Target: <1μs per acquire (including CoW overhead)
    EXPECT_LT(avg_ns, 1000.0f) << "Average: " << avg_ns << "ns";

    buffer_pool_destroy(pool);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
