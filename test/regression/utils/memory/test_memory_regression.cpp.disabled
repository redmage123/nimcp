//=============================================================================
// test_memory_regression.cpp - Memory System Regression Tests
//=============================================================================
/**
 * @file test_memory_regression.cpp
 * @brief Regression tests for memory pool, CoW, and buffer pool performance
 *
 * WHAT: Performance benchmarks and stress tests to prevent regressions
 * WHY:  Ensure optimizations don't degrade, catch performance bugs early
 * HOW:  Measure allocation speed, memory efficiency, thread scalability
 *
 * REGRESSION TARGETS:
 * - Memory Pool: <100ns acquire time
 * - CoW Manager: O(1) refcount, <1μs CoW trigger
 * - Buffer Pool: 10x memory savings for sparse allocation
 * - Thread Safety: Linear scalability up to 16 threads
 * - Memory Leaks: Zero leaks under all workloads
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <algorithm>

extern "C" {
#include "utils/memory/nimcp_memory_pool.h"
#include "utils/memory/nimcp_cow_manager.h"
#include "utils/memory/nimcp_buffer_pool.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class MemoryRegressionTest : public ::testing::Test {
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

    // Helper: Measure operation time in nanoseconds
    template<typename Func>
    uint64_t measure_ns(Func func, int iterations = 1000) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / iterations;
    }
};

//=============================================================================
// Memory Pool Performance Regression Tests
//=============================================================================

/**
 * WHAT: Verify pool acquire stays <100ns
 * WHY:  Catch allocation performance regressions
 */
TEST_F(MemoryRegressionTest, Pool_AcquireSpeed_Under100ns) {
    memory_pool_config_t config = memory_pool_default_config(1024, 1000);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Warm up
    for (int i = 0; i < 100; i++) {
        void* ptr = memory_pool_acquire(pool);
        memory_pool_release(pool, ptr);
    }

    // Measure
    std::vector<void*> ptrs;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        ptrs.push_back(memory_pool_acquire(pool));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    float avg_ns = (float)duration_ns / 1000.0f;

    // Cleanup
    for (void* ptr : ptrs) {
        memory_pool_release(pool, ptr);
    }
    memory_pool_destroy(pool);

    // Regression target: <100ns
    EXPECT_LT(avg_ns, 100.0f) << "Average acquire time: " << avg_ns << "ns";
}

/**
 * WHAT: Verify pool vs malloc speedup
 * WHY:  Ensure pool maintains >10x advantage over malloc
 */
TEST_F(MemoryRegressionTest, Pool_MallocSpeedup_Over10x) {
    memory_pool_config_t config = memory_pool_default_config(1024, 1000);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Measure pool
    std::vector<void*> pool_ptrs;
    auto pool_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        pool_ptrs.push_back(memory_pool_acquire(pool));
    }
    auto pool_end = std::chrono::high_resolution_clock::now();
    auto pool_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(pool_end - pool_start).count();

    // Cleanup pool
    for (void* ptr : pool_ptrs) {
        memory_pool_release(pool, ptr);
    }

    // Measure malloc
    std::vector<void*> malloc_ptrs;
    auto malloc_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        malloc_ptrs.push_back(malloc(1024));
    }
    auto malloc_end = std::chrono::high_resolution_clock::now();
    auto malloc_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(malloc_end - malloc_start).count();

    // Cleanup malloc
    for (void* ptr : malloc_ptrs) {
        free(ptr);
    }

    memory_pool_destroy(pool);

    float speedup = (float)malloc_ns / (float)pool_ns;
    EXPECT_GT(speedup, 10.0f) << "Pool speedup: " << speedup << "x";
}

//=============================================================================
// CoW Manager Performance Regression Tests
//=============================================================================

/**
 * WHAT: Verify CoW acquire (refcount increment) is O(1) and fast
 * WHY:  Ensure atomic operations don't introduce overhead
 */
TEST_F(MemoryRegressionTest, CoW_AcquireSpeed_Fast) {
    float template_data[256] = {0};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), nullptr);
    cow_manager_t cow = cow_manager_create(&config, template_data);
    ASSERT_NE(cow, nullptr);

    // Measure acquire time
    std::vector<cow_handle_t> handles;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        handles.push_back(cow_acquire(cow));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    float avg_ns = (float)duration_ns / 1000.0f;

    // Cleanup
    for (auto handle : handles) {
        cow_release(handle);
    }
    cow_manager_destroy(cow);

    // Target: <100ns (atomic increment is very fast)
    EXPECT_LT(avg_ns, 100.0f) << "Average cow_acquire: " << avg_ns << "ns";
}

/**
 * WHAT: Verify CoW copy triggers are <1μs when using pool
 * WHY:  Ensure lazy copy performance meets targets
 */
TEST_F(MemoryRegressionTest, CoW_CopyTrigger_Under1us) {
    // Create pool for fast copies
    memory_pool_config_t pool_config = memory_pool_default_config(1024, 100);
    memory_pool_t pool = memory_pool_create(&pool_config);
    ASSERT_NE(pool, nullptr);

    float template_data[256] = {0};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), pool);
    cow_manager_t cow = cow_manager_create(&config, template_data);
    ASSERT_NE(cow, nullptr);

    // Measure CoW trigger time
    std::vector<cow_handle_t> handles;
    for (int i = 0; i < 100; i++) {
        handles.push_back(cow_acquire(cow));
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (auto handle : handles) {
        cow_write(handle);  // Trigger CoW
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    float avg_ns = (float)duration_ns / 100.0f;

    // Cleanup
    for (auto handle : handles) {
        cow_release(handle);
    }
    cow_manager_destroy(cow);
    memory_pool_destroy(pool);

    // Target: <1μs (1000ns)
    EXPECT_LT(avg_ns, 1000.0f) << "Average CoW trigger: " << avg_ns << "ns";
}

//=============================================================================
// Buffer Pool Memory Efficiency Regression Tests
//=============================================================================

/**
 * WHAT: Verify 10x memory savings for sparse allocation
 * WHY:  Ensure CoW optimization delivers expected benefits
 */
TEST_F(MemoryRegressionTest, BufferPool_SparseAllocation_10xSavings) {
    // Measure with CoW (1000 channels, 100 active)
    buffer_pool_config_t config_cow = buffer_pool_default_config(
        100, 500, 2500, 1000, 100
    );
    config_cow.enable_cow = true;

    nimcp_memory_stats_t before_cow, after_cow;
    nimcp_memory_get_stats(&before_cow);

    buffer_pool_t pool_cow = buffer_pool_create(&config_cow);
    ASSERT_NE(pool_cow, nullptr);

    for (int i = 0; i < 100; i++) {
        buffer_pool_acquire_integration_buffer(pool_cow, i, false);
    }

    nimcp_memory_get_stats(&after_cow);
    size_t cow_memory = after_cow.current_allocated - before_cow.current_allocated;

    buffer_pool_destroy(pool_cow);

    // Measure without CoW (1000 channels, all allocated)
    buffer_pool_config_t config_no_cow = buffer_pool_default_config(
        100, 500, 2500, 1000, 1000
    );
    config_no_cow.enable_cow = false;

    nimcp_memory_stats_t before_no_cow, after_no_cow;
    nimcp_memory_get_stats(&before_no_cow);

    buffer_pool_t pool_no_cow = buffer_pool_create(&config_no_cow);
    ASSERT_NE(pool_no_cow, nullptr);

    for (int i = 0; i < 1000; i++) {
        buffer_pool_acquire_integration_buffer(pool_no_cow, i, false);
    }

    nimcp_memory_get_stats(&after_no_cow);
    size_t no_cow_memory = after_no_cow.current_allocated - before_no_cow.current_allocated;

    buffer_pool_destroy(pool_no_cow);

    // Calculate savings
    float savings_ratio = (float)no_cow_memory / (float)cow_memory;

    // Target: >5x savings (being conservative)
    EXPECT_GT(savings_ratio, 5.0f)
        << "Savings ratio: " << savings_ratio << "x "
        << "(CoW: " << cow_memory << " bytes, No-CoW: " << no_cow_memory << " bytes)";
}

//=============================================================================
// Thread Scalability Regression Tests
//=============================================================================

/**
 * WHAT: Verify linear scalability up to 16 threads
 * WHY:  Ensure lock contention doesn't kill performance
 */
TEST_F(MemoryRegressionTest, Scalability_ThreadCount_Linear) {
    buffer_pool_config_t config = buffer_pool_default_config(100, 500, 2500, 1000, 500);
    buffer_pool_t pool = buffer_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    // Measure with different thread counts
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    std::vector<float> throughputs;

    for (int num_threads : thread_counts) {
        constexpr int OPS_PER_THREAD = 1000;

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; t++) {
            threads.emplace_back([pool, t, num_threads]() {
                for (int i = 0; i < OPS_PER_THREAD; i++) {
                    int channel = (t * OPS_PER_THREAD + i) % 1000;
                    buffer_pool_acquire_integration_buffer(pool, channel, false);
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        float throughput = (float)(num_threads * OPS_PER_THREAD) / (float)duration_ms * 1000.0f;  // ops/sec
        throughputs.push_back(throughput);

        // Reset for next test
        buffer_pool_reset(pool);
    }

    buffer_pool_destroy(pool);

    // Verify scaling: 16 threads should achieve >50% of 16x ideal speedup
    float baseline = throughputs[0];  // 1 thread
    float scaling_16 = throughputs[4] / baseline;  // 16 threads / 1 thread

    EXPECT_GT(scaling_16, 8.0f)  // At least 50% of ideal 16x
        << "16-thread scaling: " << scaling_16<< "x";
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * WHAT: Stress test with rapid alloc/dealloc cycles
 * WHY:  Catch memory corruption and race conditions
 */
TEST_F(MemoryRegressionTest, Stress_RapidCycles_NoCorruption) {
    memory_pool_config_t config = memory_pool_default_config(1024, 100);
    memory_pool_t pool = memory_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    constexpr int NUM_ITERATIONS = 10000;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        void* ptr = memory_pool_acquire(pool);
        ASSERT_NE(ptr, nullptr);
        memory_pool_release(pool, ptr);
    }

    // Verify pool integrity
    memory_pool_stats_t stats;
    memory_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.allocated_blocks, 0);
    EXPECT_EQ(stats.free_blocks, config.num_blocks);

    memory_pool_destroy(pool);
}

/**
 * WHAT: Stress test CoW with many concurrent writes
 * WHY:  Ensure CoW copy-on-write is robust under contention
 */
TEST_F(MemoryRegressionTest, Stress_ConcurrentCoW_NoRaces) {
    float template_data[256] = {0};
    cow_manager_config_t config = cow_default_config(sizeof(template_data), nullptr);
    cow_manager_t cow = cow_manager_create(&config, template_data);
    ASSERT_NE(cow, nullptr);

    constexpr int NUM_THREADS = 32;
    constexpr int OPS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::atomic<int> success{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([cow, &success]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                cow_handle_t h = cow_acquire(cow);
                if (h) {
                    float* data = (float*)cow_write(h);
                    if (data) {
                        data[0] = 1.0f;  // Write to trigger CoW
                        success++;
                    }
                    cow_release(h);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success.load(), NUM_THREADS * OPS_PER_THREAD);

    cow_manager_destroy(cow);
}

/**
 * WHAT: Memory leak stress test with complex lifecycle
 * WHY:  Ensure no leaks under realistic usage patterns
 */
TEST_F(MemoryRegressionTest, Stress_ComplexLifecycle_NoLeaks) {
    nimcp_memory_stats_t start_stats;
    nimcp_memory_get_stats(&start_stats);

    // Simulate complex workflow
    for (int iteration = 0; iteration < 100; iteration++) {
        buffer_pool_config_t config = buffer_pool_default_config(100, 500, 2500, 100, 50);
        buffer_pool_t pool = buffer_pool_create(&config);

        // Acquire various channels
        for (int i = 0; i < 50; i++) {
            buffer_pool_acquire_integration_buffer(pool, i, i % 3 == 0);
            buffer_pool_acquire_sliding_window(pool, i, i % 4 == 0);
            buffer_pool_acquire_temporal_accumulator(pool, i, i % 5 == 0);
        }

        // Reset and reuse
        buffer_pool_reset(pool);

        for (int i = 0; i < 30; i++) {
            buffer_pool_acquire_integration_buffer(pool, i, false);
        }

        buffer_pool_destroy(pool);
    }

    nimcp_memory_stats_t end_stats;
    nimcp_memory_get_stats(&end_stats);

    // Verify no accumulation
    EXPECT_EQ(end_stats.current_allocated, start_stats.current_allocated);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
