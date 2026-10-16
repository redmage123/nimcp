/**
 * @file test_recovery_pool_regression.cpp
 * @brief Regression tests for recovery pool (6+ tests)
 *
 * TEST COVERAGE:
 * - Pool size requirements validation
 * - Allocation pattern stability
 * - Performance benchmarks
 * - Memory leak detection
 * - Backward compatibility
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_recovery_pool.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"

#include <vector>
#include <chrono>
#include <cstring>

//=============================================================================
// Test Fixture
//=============================================================================

class RecoveryPoolRegressionTest : public ::testing::Test {
protected:
    recovery_pool_t* pool;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_thread_init();
        pool = nullptr;
    }

    void TearDown() override {
        if (pool) {
            recovery_pool_destroy(pool);
            pool = nullptr;
        }
        recovery_pool_clear_error();
    }
};

//=============================================================================
// Test Group 1: Pool Size Requirements (2 tests)
//=============================================================================

TEST_F(RecoveryPoolRegressionTest, MinimumPoolSize) {
    // Regression: 256KB minimum should support basic recovery
    const size_t min_size = 256 * 1024;
    pool = recovery_pool_create(min_size);
    ASSERT_NE(pool, nullptr);

    // Should support at least 50 allocations of 4KB each
    const int min_allocations = 50;
    const size_t alloc_size = 4096;

    int successful_allocations = 0;
    for (int i = 0; i < min_allocations; i++) {
        void* ptr = recovery_pool_alloc(pool, alloc_size);
        if (ptr) {
            successful_allocations++;
        }
    }

    // Regression check: minimum allocations should succeed
    EXPECT_GE(successful_allocations, min_allocations);

    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_GE(stats.current_used_bytes, min_allocations * alloc_size);
}

TEST_F(RecoveryPoolRegressionTest, StandardPoolSize) {
    // Regression: 1MB standard pool should support typical recovery workflow
    const size_t standard_size = 1024 * 1024;
    pool = recovery_pool_create(standard_size);
    ASSERT_NE(pool, nullptr);

    // Simulate typical recovery workflow allocations
    struct workflow_allocations {
        void* checkpoint_header;      // 4KB
        void* checkpoint_data;        // 256KB
        void* diagnostics_context;    // 8KB
        void* diagnostics_snapshot;   // 64KB
        void* recovery_metadata;      // 2KB
        void* temp_buffers[10];       // 10 x 16KB = 160KB
    };

    workflow_allocations allocs = {};

    // Allocate workflow structures
    allocs.checkpoint_header = recovery_pool_alloc(pool, 4 * 1024);
    allocs.checkpoint_data = recovery_pool_alloc(pool, 256 * 1024);
    allocs.diagnostics_context = recovery_pool_alloc(pool, 8 * 1024);
    allocs.diagnostics_snapshot = recovery_pool_alloc(pool, 64 * 1024);
    allocs.recovery_metadata = recovery_pool_alloc(pool, 2 * 1024);

    for (int i = 0; i < 10; i++) {
        allocs.temp_buffers[i] = recovery_pool_alloc(pool, 16 * 1024);
    }

    // Regression check: all typical allocations should succeed
    EXPECT_NE(allocs.checkpoint_header, nullptr);
    EXPECT_NE(allocs.checkpoint_data, nullptr);
    EXPECT_NE(allocs.diagnostics_context, nullptr);
    EXPECT_NE(allocs.diagnostics_snapshot, nullptr);
    EXPECT_NE(allocs.recovery_metadata, nullptr);

    for (int i = 0; i < 10; i++) {
        EXPECT_NE(allocs.temp_buffers[i], nullptr);
    }

    // Total: ~510KB allocated, should fit in 1MB pool with alignment
    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_LT(stats.current_used_bytes, standard_size);
    EXPECT_FALSE(stats.pool_exhausted);
}

//=============================================================================
// Test Group 2: Allocation Pattern Stability (2 tests)
//=============================================================================

TEST_F(RecoveryPoolRegressionTest, AllocationAlignmentStability) {
    // Regression: All allocations must be 8-byte aligned
    pool = recovery_pool_create(64 * 1024);
    ASSERT_NE(pool, nullptr);

    // Test various allocation sizes
    const size_t test_sizes[] = {
        1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17,
        31, 32, 33, 63, 64, 65, 127, 128, 129,
        255, 256, 257, 511, 512, 513, 1023, 1024, 1025
    };

    for (size_t size : test_sizes) {
        void* ptr = recovery_pool_alloc(pool, size);
        ASSERT_NE(ptr, nullptr) << "Failed to allocate size " << size;

        // Regression check: 8-byte alignment
        uintptr_t addr = (uintptr_t)ptr;
        EXPECT_EQ(addr % 8, 0) << "Allocation of size " << size << " not 8-byte aligned";
    }
}

TEST_F(RecoveryPoolRegressionTest, AllocationOrderStability) {
    // Regression: Allocations should be contiguous and ordered
    pool = recovery_pool_create(64 * 1024);
    ASSERT_NE(pool, nullptr);

    // Allocate 10 blocks of 1024 bytes
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        void* ptr = recovery_pool_alloc(pool, 1024);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    // Regression check: pointers should be in ascending order
    for (size_t i = 1; i < ptrs.size(); i++) {
        uintptr_t prev_addr = (uintptr_t)ptrs[i - 1];
        uintptr_t curr_addr = (uintptr_t)ptrs[i];
        EXPECT_LT(prev_addr, curr_addr) << "Allocation " << i << " not after allocation " << (i-1);
    }

    // Regression check: spacing should be consistent (aligned size)
    for (size_t i = 1; i < ptrs.size(); i++) {
        uintptr_t prev_addr = (uintptr_t)ptrs[i - 1];
        uintptr_t curr_addr = (uintptr_t)ptrs[i];
        size_t spacing = curr_addr - prev_addr;

        // Spacing should be aligned size (1024 aligned to 1024)
        EXPECT_EQ(spacing, 1024) << "Spacing inconsistent at allocation " << i;
    }
}

//=============================================================================
// Test Group 3: Performance Benchmarks (2 tests)
//=============================================================================

TEST_F(RecoveryPoolRegressionTest, AllocationPerformance) {
    // Regression: 1000 allocations should complete in < 10ms
    pool = recovery_pool_create(1024 * 1024);
    ASSERT_NE(pool, nullptr);

    const int num_allocations = 1000;
    const size_t alloc_size = 512;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_allocations; i++) {
        void* ptr = recovery_pool_alloc(pool, alloc_size);
        ASSERT_NE(ptr, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Regression check: should complete in < 10ms
    EXPECT_LT(duration.count(), 10) << "1000 allocations took " << duration.count() << "ms (expected < 10ms)";

    // Verify all allocations succeeded
    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.total_allocations, num_allocations);
}

TEST_F(RecoveryPoolRegressionTest, ResetPerformance) {
    // Regression: Reset should complete in < 1ms even with many allocations
    pool = recovery_pool_create(1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Create many allocations
    const int num_allocations = 500;
    for (int i = 0; i < num_allocations; i++) {
        void* ptr = recovery_pool_alloc(pool, 1024);
        ASSERT_NE(ptr, nullptr);
    }

    // Measure reset time
    auto start = std::chrono::high_resolution_clock::now();
    ASSERT_TRUE(recovery_pool_reset(pool));
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Regression check: reset should complete in < 1ms
    EXPECT_LT(duration.count(), 1000) << "Reset took " << duration.count() << "us (expected < 1000us)";

    // Verify pool reset
    recovery_pool_stats_t stats;
    ASSERT_TRUE(recovery_pool_get_stats(pool, &stats));
    EXPECT_EQ(stats.current_used_bytes, 0);
    EXPECT_EQ(stats.allocation_count, 0);
}

//=============================================================================
// Test Group 4: Memory Leak Detection (1 test)
//=============================================================================

TEST_F(RecoveryPoolRegressionTest, NoMemoryLeaks) {
    // Regression: Multiple create/destroy cycles should not leak memory
    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    const int num_cycles = 100;
    for (int i = 0; i < num_cycles; i++) {
        // Create pool
        recovery_pool_t* test_pool = recovery_pool_create(64 * 1024);
        ASSERT_NE(test_pool, nullptr);

        // Use pool
        for (int j = 0; j < 10; j++) {
            void* ptr = recovery_pool_alloc(test_pool, 1024);
            ASSERT_NE(ptr, nullptr);
        }

        // Reset
        ASSERT_TRUE(recovery_pool_reset(test_pool));

        // Destroy pool
        recovery_pool_destroy(test_pool);
    }

    nimcp_memory_get_stats(&stats_after);

    // Regression check: memory usage should return to baseline
    // Allow small overhead for tracking structures
    size_t leaked = stats_after.current_allocated - stats_before.current_allocated;
    EXPECT_LT(leaked, 1024) << "Detected " << leaked << " bytes leaked after " << num_cycles << " cycles";
}

//=============================================================================
// Test Group 5: Backward Compatibility (1 test)
//=============================================================================

TEST_F(RecoveryPoolRegressionTest, APIBackwardCompatibility) {
    // Regression: Core API should remain stable
    // This test documents the expected API surface

    // Pool creation
    pool = recovery_pool_create(1024);
    ASSERT_NE(pool, nullptr);

    // Emergency mode
    EXPECT_TRUE(recovery_pool_enter_emergency_mode(pool));
    EXPECT_TRUE(recovery_pool_is_emergency_mode(pool));
    EXPECT_TRUE(recovery_pool_exit_emergency_mode(pool));

    // Allocation
    void* ptr1 = recovery_pool_alloc(pool, 100);
    ASSERT_NE(ptr1, nullptr);

    void* ptr2 = recovery_pool_calloc(pool, 10, 10);
    ASSERT_NE(ptr2, nullptr);

    recovery_pool_free(pool, ptr1);

    // Pool management
    EXPECT_TRUE(recovery_pool_has_space(pool, 100));
    EXPECT_GT(recovery_pool_get_available(pool), 0);

    recovery_pool_stats_t stats;
    EXPECT_TRUE(recovery_pool_get_stats(pool, &stats));

    EXPECT_TRUE(recovery_pool_reset(pool));

    // Validation
    EXPECT_TRUE(recovery_pool_validate(pool));

    // Global pool
    recovery_pool_set_global(pool);
    EXPECT_EQ(recovery_pool_get_global(), pool);
    recovery_pool_set_global(nullptr);

    // Error handling
    recovery_pool_clear_error();
    const char* error = recovery_pool_get_error();
    EXPECT_NE(error, nullptr);

    // Destruction
    recovery_pool_destroy(pool);
    pool = nullptr;

    // Regression check: All core API functions present and working
    SUCCEED() << "All core API functions remain stable";
}

//=============================================================================
// Summary
//=============================================================================

// Total tests: 8 (exceeds 6 requirement)
// Coverage:
// - Pool size requirements: 2 tests
// - Allocation pattern stability: 2 tests
// - Performance benchmarks: 2 tests
// - Memory leak detection: 1 test
// - Backward compatibility: 1 test
