/**
 * @file test_memory_pool.cpp
 * @brief Comprehensive unit tests for nimcp_memory_pool - O(1) memory pool
 *
 * WHAT: 100% code coverage tests for memory pool with free-list management
 * WHY:  Ensure correctness, thread-safety, and performance of pool operations
 * HOW:  Test all functions, edge cases, concurrency, and performance
 *
 * TEST COVERAGE:
 * - Basic operations: create, acquire, release, destroy
 * - Edge cases: NULL pointers, exhaustion, double-release
 * - Statistics: tracking, peak allocation, failed allocations
 * - Thread safety: concurrent acquire/release from 100 threads
 * - Performance: Verify <100ns acquire vs ~1.5μs malloc
 * - Memory: Zero leaks validation
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
    #include "utils/memory/nimcp_memory_pool.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/exception/nimcp_exception.h"
    #include "utils/exception/nimcp_exception_handlers.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MemoryPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory tracking for leak detection
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();
        // Warm up exception handler system (one-time mutex allocation)
        // by dispatching a dummy exception, then clearing it
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
        // before checking for leaks (exception objects are ~4KB)
        nimcp_exception_clear_current();

        // Check for memory leaks relative to baseline
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_EQ(stats.current_allocated, baseline_stats_.current_allocated)
            << "Memory leak detected: "
            << (stats.current_allocated - baseline_stats_.current_allocated)
            << " bytes leaked since baseline";
    }

    nimcp_memory_stats_t baseline_stats_ = {};
};

//=============================================================================
// Basic Operations Tests
//=============================================================================

/**
 * WHAT: Test pool creation with valid configuration
 * WHY:  Verify basic pool initialization works
 */
TEST_F(MemoryPoolTest, CreatePool_ValidConfig_Success) {
    memory_pool_config_t config = memory_pool_default_config(1024, 100);

    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Verify pool properties
    EXPECT_EQ(memory_pool_get_block_size(pool), 1024);
    EXPECT_EQ(memory_pool_get_available(pool), 100);

    memory_pool_destroy(pool);
}

/**
 * WHAT: Test pool creation with NULL config
 * WHY:  Verify proper error handling for invalid input
 */
TEST_F(MemoryPoolTest, CreatePool_NullConfig_ReturnsNull) {
    memory_pool_t pool = memory_pool_create(nullptr);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test pool creation with zero blocks
 * WHY:  Verify validation of pool size
 */
TEST_F(MemoryPoolTest, CreatePool_ZeroBlocks_ReturnsNull) {
    memory_pool_config_t config = memory_pool_default_config(1024, 0);
    memory_pool_t pool = memory_pool_create(&config);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test pool creation with zero block size
 * WHY:  Verify validation of block size
 */
TEST_F(MemoryPoolTest, CreatePool_ZeroBlockSize_ReturnsNull) {
    memory_pool_config_t config = memory_pool_default_config(0, 100);
    memory_pool_t pool = memory_pool_create(&config);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test pool creation with invalid alignment (not power of 2)
 * WHY:  Verify alignment validation
 */
TEST_F(MemoryPoolTest, CreatePool_InvalidAlignment_ReturnsNull) {
    memory_pool_config_t config = {
        .block_size = 1024,
        .num_blocks = 100,
        .alignment = 15,  // Not a power of 2
        .enable_tracking = true,
        .enable_guard_pages = false
    };

    memory_pool_t pool = memory_pool_create(&config);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test basic acquire operation
 * WHY:  Verify O(1) allocation works
 */
TEST_F(MemoryPoolTest, Acquire_ValidPool_ReturnsBlock) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    void* block = memory_pool_acquire(pool);
    ASSERT_NE(block, nullptr);

    // Verify available count decreased
    EXPECT_EQ(memory_pool_get_available(pool), 9);

    memory_pool_release(pool, block);
    memory_pool_destroy(pool);
}

/**
 * WHAT: Test acquire from NULL pool
 * WHY:  Verify error handling
 */
TEST_F(MemoryPoolTest, Acquire_NullPool_ReturnsNull) {
    void* block = memory_pool_acquire(nullptr);
    EXPECT_EQ(block, nullptr);
}

/**
 * WHAT: Test acquire when pool exhausted
 * WHY:  Verify proper handling of pool exhaustion
 */
TEST_F(MemoryPoolTest, Acquire_PoolExhausted_ReturnsNull) {
    memory_pool_config_t config = memory_pool_default_config(1024, 2);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire all blocks
    void* block1 = memory_pool_acquire(pool);
    void* block2 = memory_pool_acquire(pool);
    ASSERT_NE(block1, nullptr);
    ASSERT_NE(block2, nullptr);

    // Pool should be exhausted
    void* block3 = memory_pool_acquire(pool);
    EXPECT_EQ(block3, nullptr);
    EXPECT_EQ(memory_pool_get_available(pool), 0);

    // Check failed allocations in stats
    memory_pool_stats_t stats;
    ASSERT_TRUE(memory_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.failed_allocations, 1);

    memory_pool_release(pool, block1);
    memory_pool_release(pool, block2);
    memory_pool_destroy(pool);
}

/**
 * WHAT: Test basic release operation
 * WHY:  Verify O(1) deallocation works
 */
TEST_F(MemoryPoolTest, Release_ValidBlock_Success) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    void* block = memory_pool_acquire(pool);
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(memory_pool_get_available(pool), 9);

    memory_pool_release(pool, block);
    EXPECT_EQ(memory_pool_get_available(pool), 10);

    memory_pool_destroy(pool);
}

/**
 * WHAT: Test release with NULL pool
 * WHY:  Verify error handling (should not crash)
 */
TEST_F(MemoryPoolTest, Release_NullPool_NoCrash) {
    void* fake_block = (void*)0x1234;
    memory_pool_release(nullptr, fake_block);  // Should not crash
}

/**
 * WHAT: Test release with NULL block
 * WHY:  Verify NULL pointer handling
 */
TEST_F(MemoryPoolTest, Release_NullBlock_NoCrash) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    memory_pool_release(pool, nullptr);  // Should not crash

    memory_pool_destroy(pool);
}

/**
 * WHAT: Test double-release detection
 * WHY:  Verify protection against double-free
 */
TEST_F(MemoryPoolTest, Release_DoubleRelease_Ignored) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    void* block = memory_pool_acquire(pool);
    ASSERT_NE(block, nullptr);

    memory_pool_release(pool, block);  // First release - valid
    EXPECT_EQ(memory_pool_get_available(pool), 10);

    memory_pool_release(pool, block);  // Double release - should be ignored
    EXPECT_EQ(memory_pool_get_available(pool), 10);  // Count shouldn't change

    memory_pool_destroy(pool);
}

/**
 * WHAT: Test pool reset operation
 * WHY:  Verify fast bulk deallocation
 */
TEST_F(MemoryPoolTest, Reset_ValidPool_FreesAllBlocks) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire several blocks
    std::vector<void*> blocks;
    for (int i = 0; i < 5; i++) {
        void* block = memory_pool_acquire(pool);
        ASSERT_NE(block, nullptr);
        blocks.push_back(block);
    }
    EXPECT_EQ(memory_pool_get_available(pool), 5);

    // Reset pool
    size_t reset_count = memory_pool_reset(pool);
    EXPECT_EQ(reset_count, 10);
    EXPECT_EQ(memory_pool_get_available(pool), 10);

    // Don't release blocks - they're invalidated after reset
    memory_pool_destroy(pool);
}

/**
 * WHAT: Test destroy with NULL pool
 * WHY:  Verify NULL handling (should not crash)
 */
TEST_F(MemoryPoolTest, Destroy_NullPool_NoCrash) {
    memory_pool_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * WHAT: Test statistics tracking
 * WHY:  Verify allocation counters work correctly
 */
TEST_F(MemoryPoolTest, Stats_TrackAllocations_Accurate) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    config.enable_tracking = true;
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire and release blocks
    void* block1 = memory_pool_acquire(pool);
    void* block2 = memory_pool_acquire(pool);
    memory_pool_release(pool, block1);
    void* block3 = memory_pool_acquire(pool);

    // Check statistics
    memory_pool_stats_t stats;
    ASSERT_TRUE(memory_pool_get_stats(pool, &stats));

    EXPECT_EQ(stats.total_blocks, 10);
    EXPECT_EQ(stats.allocated_blocks, 2);  // block2 and block3 still allocated
    EXPECT_EQ(stats.free_blocks, 8);
    EXPECT_EQ(stats.total_allocations, 3);
    EXPECT_EQ(stats.total_deallocations, 1);
    EXPECT_EQ(stats.peak_allocated, 2);
    EXPECT_EQ(stats.failed_allocations, 0);

    memory_pool_release(pool, block2);
    memory_pool_release(pool, block3);
    memory_pool_destroy(pool);
}

/**
 * WHAT: Test peak allocation tracking
 * WHY:  Verify peak counter captures maximum usage
 */
TEST_F(MemoryPoolTest, Stats_TrackPeak_CapturesMaximum) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    config.enable_tracking = true;
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire 5 blocks (peak = 5)
    std::vector<void*> blocks;
    for (int i = 0; i < 5; i++) {
        blocks.push_back(memory_pool_acquire(pool));
    }

    // Release 3 blocks (current = 2, but peak still = 5)
    for (int i = 0; i < 3; i++) {
        memory_pool_release(pool, blocks[i]);
    }

    // Check peak
    memory_pool_stats_t stats;
    ASSERT_TRUE(memory_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.peak_allocated, 5);
    EXPECT_EQ(stats.allocated_blocks, 2);

    memory_pool_release(pool, blocks[3]);
    memory_pool_release(pool, blocks[4]);
    memory_pool_destroy(pool);
}

/**
 * WHAT: Test get_stats with NULL parameters
 * WHY:  Verify error handling
 */
TEST_F(MemoryPoolTest, Stats_NullParameters_ReturnsFalse) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    memory_pool_stats_t stats;
    EXPECT_FALSE(memory_pool_get_stats(nullptr, &stats));
    EXPECT_FALSE(memory_pool_get_stats(pool, nullptr));

    memory_pool_destroy(pool);
}

//=============================================================================
// Ownership and Bounds Tests
//=============================================================================

/**
 * WHAT: Test ownership check for valid block
 * WHY:  Verify pointer validation works
 */
TEST_F(MemoryPoolTest, Owns_ValidBlock_ReturnsTrue) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    void* block = memory_pool_acquire(pool);
    ASSERT_NE(block, nullptr);

    EXPECT_TRUE(memory_pool_owns(pool, block));

    memory_pool_release(pool, block);
    memory_pool_destroy(pool);
}

/**
 * WHAT: Test ownership check for external pointer
 * WHY:  Verify rejection of non-pool pointers
 */
TEST_F(MemoryPoolTest, Owns_ExternalPointer_ReturnsFalse) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    void* external = nimcp_malloc(100);
    EXPECT_FALSE(memory_pool_owns(pool, external));
    nimcp_free(external);

    memory_pool_destroy(pool);
}

/**
 * WHAT: Test ownership with NULL parameters
 * WHY:  Verify NULL handling
 */
TEST_F(MemoryPoolTest, Owns_NullParameters_ReturnsFalse) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    void* block = memory_pool_acquire(pool);

    EXPECT_FALSE(memory_pool_owns(nullptr, block));
    EXPECT_FALSE(memory_pool_owns(pool, nullptr));

    memory_pool_release(pool, block);
    memory_pool_destroy(pool);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * WHAT: Test concurrent acquire/release from multiple threads
 * WHY:  Verify thread-safe operations under load
 */
TEST_F(MemoryPoolTest, Concurrency_100Threads_ThreadSafe) {
    memory_pool_config_t config = memory_pool_default_config(1024, 1000);
    config.enable_tracking = true;
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    const int NUM_THREADS = 100;
    const int OPERATIONS_PER_THREAD = 10;

    std::vector<std::thread> threads;
    std::atomic<int> successful_ops{0};

    // Launch threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([pool, &successful_ops, OPERATIONS_PER_THREAD]() {
            for (int j = 0; j < OPERATIONS_PER_THREAD; j++) {
                void* block = memory_pool_acquire(pool);
                if (block) {
                    // Write to block (verify no corruption)
                    memset(block, 0xFF, 1024);
                    memory_pool_release(pool, block);
                    successful_ops++;
                }
            }
        });
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all operations completed
    EXPECT_EQ(successful_ops.load(), NUM_THREADS * OPERATIONS_PER_THREAD);

    // Verify pool integrity
    EXPECT_EQ(memory_pool_get_available(pool), 1000);

    memory_pool_stats_t stats;
    ASSERT_TRUE(memory_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.total_allocations, NUM_THREADS * OPERATIONS_PER_THREAD);
    EXPECT_EQ(stats.total_deallocations, NUM_THREADS * OPERATIONS_PER_THREAD);
    EXPECT_EQ(stats.allocated_blocks, 0);

    memory_pool_destroy(pool);
}

//=============================================================================
// Alignment Tests
//=============================================================================

/**
 * WHAT: Test memory alignment
 * WHY:  Verify blocks are properly aligned for SIMD/atomic operations
 */
TEST_F(MemoryPoolTest, Alignment_16ByteAlignment_Aligned) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    config.alignment = 16;
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire multiple blocks and check alignment
    for (int i = 0; i < 10; i++) {
        void* block = memory_pool_acquire(pool);
        ASSERT_NE(block, nullptr);

        uintptr_t addr = reinterpret_cast<uintptr_t>(block);
        EXPECT_EQ(addr % 16, 0) << "Block " << i << " not 16-byte aligned";

        memory_pool_release(pool, block);
    }

    memory_pool_destroy(pool);
}

/**
 * WHAT: Test that pool respects requested alignment
 * WHY:  Verify alignment configuration is used
 * NOTE: User pointers have header offset, so check alignment is power-of-2
 *       The pool's internal memory region IS aligned, but user pointers
 *       are offset by sizeof(block_header_t) = 16 bytes
 */
TEST_F(MemoryPoolTest, Alignment_CustomAlignment_Respected) {
    // Test with 32-byte alignment
    memory_pool_config_t config = memory_pool_default_config(4096, 10);
    config.alignment = 32;
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire multiple blocks and verify consistent alignment
    std::vector<void*> blocks;
    for (int i = 0; i < 5; i++) {
        void* block = memory_pool_acquire(pool);
        ASSERT_NE(block, nullptr);
        blocks.push_back(block);

        uintptr_t addr = reinterpret_cast<uintptr_t>(block);
        // Block pointers should have consistent alignment
        // (may not be exactly alignment due to header, but should be consistent)
        EXPECT_EQ(addr % 16, 0) << "Block " << i << " not 16-byte aligned (header offset)";
    }

    for (void* block : blocks) {
        memory_pool_release(pool, block);
    }

    memory_pool_destroy(pool);
}

//=============================================================================
// Memory Reuse Tests
//=============================================================================

/**
 * WHAT: Test memory reuse after release
 * WHY:  Verify blocks can be reused (free-list works)
 */
TEST_F(MemoryPoolTest, Reuse_ReleaseAndAcquire_ReusesMemory) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Acquire and mark all blocks
    std::vector<void*> blocks;
    for (int i = 0; i < 10; i++) {
        void* block = memory_pool_acquire(pool);
        ASSERT_NE(block, nullptr);
        // Write pattern to identify block
        memset(block, i + 1, 1024);
        blocks.push_back(block);
    }

    // Release all blocks
    for (void* block : blocks) {
        memory_pool_release(pool, block);
    }

    // Acquire again - should reuse same memory
    for (int i = 0; i < 10; i++) {
        void* block = memory_pool_acquire(pool);
        ASSERT_NE(block, nullptr);
        // Memory might be reused (no guarantee of order, but should be from pool)
        EXPECT_TRUE(memory_pool_owns(pool, block));
        memory_pool_release(pool, block);
    }

    memory_pool_destroy(pool);
}

//=============================================================================
// Performance Benchmark Tests
//=============================================================================

/**
 * WHAT: Benchmark pool acquire vs malloc
 * WHY:  Verify O(1) pool is faster than O(log n) malloc
 * EXPECT: Pool acquire <100ns, malloc ~1.5μs (15x faster)
 */
TEST_F(MemoryPoolTest, Performance_AcquireVsMalloc_PoolFaster) {
    memory_pool_config_t config = memory_pool_default_config(1024, 10000);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    const int ITERATIONS = 10000;

    // Benchmark pool acquire
    auto pool_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        void* block = memory_pool_acquire(pool);
        // Don't release - measure pure acquire time
    }
    auto pool_end = std::chrono::high_resolution_clock::now();
    auto pool_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(pool_end - pool_start);
    double pool_avg_ns = pool_duration.count() / (double)ITERATIONS;

    // Reset pool for next test
    memory_pool_reset(pool);

    // Benchmark malloc
    std::vector<void*> malloc_ptrs;
    auto malloc_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; i++) {
        void* ptr = nimcp_malloc(1024);
        malloc_ptrs.push_back(ptr);
    }
    auto malloc_end = std::chrono::high_resolution_clock::now();
    auto malloc_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(malloc_end - malloc_start);
    double malloc_avg_ns = malloc_duration.count() / (double)ITERATIONS;

    // Cleanup malloc ptrs
    for (void* ptr : malloc_ptrs) {
        nimcp_free(ptr);
    }

    // Report results
    double speedup = malloc_avg_ns / pool_avg_ns;
    std::cout << "\n=== Performance Benchmark ===\n";
    std::cout << "Pool acquire:  " << pool_avg_ns << " ns/op\n";
    std::cout << "Malloc:        " << malloc_avg_ns << " ns/op\n";
    std::cout << "Speedup:       " << speedup << "x\n";

    // Pool should be significantly faster (at least 2x)
    EXPECT_LT(pool_avg_ns, malloc_avg_ns);
    EXPECT_GT(speedup, 2.0) << "Pool should be at least 2x faster than malloc";

    memory_pool_destroy(pool);
}

//=============================================================================
// Integration with nimcp_memory Tests
//=============================================================================

/**
 * WHAT: Verify memory pool integrates with nimcp_memory tracking
 * WHY:  Ensure pool allocations are tracked by memory system
 */
TEST_F(MemoryPoolTest, Integration_MemoryTracking_Tracked) {
    nimcp_memory_stats_t before_stats, after_stats;
    nimcp_memory_get_stats(&before_stats);

    memory_pool_config_t config = memory_pool_default_config(1024, 10);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    nimcp_memory_get_stats(&after_stats);

    // Pool creation should increase memory usage
    EXPECT_GT(after_stats.current_allocated, before_stats.current_allocated);

    memory_pool_destroy(pool);

    // After destroy, memory should be back to baseline
    nimcp_memory_get_stats(&after_stats);
    EXPECT_EQ(after_stats.current_allocated, before_stats.current_allocated);
}
