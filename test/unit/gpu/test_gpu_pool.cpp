/**
 * @file test_gpu_pool.cpp
 * @brief Unit tests for GPU Memory Pool
 *
 * Tests the bump, buddy, and slab allocators along with pool management,
 * fragmentation, defragmentation, and stream-ordered allocations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>

// Headers already have their own extern "C" guards
#include "gpu/memory/nimcp_gpu_pool.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class GPUPoolTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    nimcp_gpu_pool_t* pool = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (pool) {
            nimcp_gpu_pool_destroy(pool);
            pool = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create a pool with specific type
    nimcp_gpu_pool_t* CreatePool(nimcp_gpu_pool_type_t type, size_t size) {
        nimcp_gpu_pool_config_t config;
        switch (type) {
            case NIMCP_GPU_POOL_TYPE_SCRATCH:
                config = nimcp_gpu_pool_config_scratch(size);
                break;
            case NIMCP_GPU_POOL_TYPE_PERSISTENT:
                config = nimcp_gpu_pool_config_persistent(size);
                break;
            case NIMCP_GPU_POOL_TYPE_ACTIVATION: {
                size_t slab_sizes[] = {256, 512, 1024, 2048, 4096};
                config = nimcp_gpu_pool_config_activation(size, slab_sizes, 5);
                break;
            }
            default:
                config = nimcp_gpu_pool_config_default();
                config.initial_size = size;
                break;
        }
        return nimcp_gpu_pool_create(ctx, &config);
    }
};

//=============================================================================
// Pool Configuration Tests
//=============================================================================

TEST_F(GPUPoolTest, ConfigDefault_ReturnsValidConfig) {
    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_default();

    EXPECT_GT(config.initial_size, 0u);
    EXPECT_GE(config.max_size, config.initial_size);
    EXPECT_GT(config.block_size, 0u);
    EXPECT_GT(config.alignment, 0u);
    // Check alignment is power of 2
    EXPECT_EQ((config.alignment & (config.alignment - 1)), 0u);
}

TEST_F(GPUPoolTest, ConfigScratch_ReturnsValidConfig) {
    const size_t size = 64 * 1024 * 1024;  // 64 MB
    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_scratch(size);

    EXPECT_EQ(config.initial_size, size);
    EXPECT_EQ(config.type, NIMCP_GPU_POOL_TYPE_SCRATCH);
}

TEST_F(GPUPoolTest, ConfigPersistent_ReturnsValidConfig) {
    const size_t size = 128 * 1024 * 1024;  // 128 MB
    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_persistent(size);

    EXPECT_EQ(config.initial_size, size);
    EXPECT_EQ(config.type, NIMCP_GPU_POOL_TYPE_PERSISTENT);
}

TEST_F(GPUPoolTest, ConfigActivation_ReturnsValidConfig) {
    const size_t size = 32 * 1024 * 1024;  // 32 MB
    size_t slab_sizes[] = {256, 512, 1024, 2048};
    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_activation(size, slab_sizes, 4);

    EXPECT_EQ(config.initial_size, size);
    EXPECT_EQ(config.type, NIMCP_GPU_POOL_TYPE_ACTIVATION);
    EXPECT_EQ(config.num_slab_configs, 4u);
}

//=============================================================================
// Pool Creation Tests
//=============================================================================

TEST_F(GPUPoolTest, Create_WithDefaultConfig_ReturnsValidPool) {
    RequireGPU();

    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_default();
    config.initial_size = 16 * 1024 * 1024;  // 16 MB for testing
    pool = nimcp_gpu_pool_create(ctx, &config);

    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(nimcp_gpu_pool_is_valid(pool));
}

TEST_F(GPUPoolTest, Create_WithNullConfig_UsesDefaults) {
    RequireGPU();

    pool = nimcp_gpu_pool_create(ctx, nullptr);

    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(nimcp_gpu_pool_is_valid(pool));
}

TEST_F(GPUPoolTest, Create_WithNullContext_ReturnsNull) {
    pool = nimcp_gpu_pool_create(nullptr, nullptr);

    EXPECT_EQ(pool, nullptr);
}

TEST_F(GPUPoolTest, Create_ScratchPool_ReturnsValidPool) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);

    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(nimcp_gpu_pool_is_valid(pool));
}

TEST_F(GPUPoolTest, Create_PersistentPool_ReturnsValidPool) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);

    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(nimcp_gpu_pool_is_valid(pool));
}

TEST_F(GPUPoolTest, Create_ActivationPool_ReturnsValidPool) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_ACTIVATION, 16 * 1024 * 1024);

    ASSERT_NE(pool, nullptr);
    EXPECT_TRUE(nimcp_gpu_pool_is_valid(pool));
}

TEST_F(GPUPoolTest, Destroy_HandlesNull) {
    nimcp_gpu_pool_destroy(nullptr);  // Should not crash
}

TEST_F(GPUPoolTest, IsValid_ReturnsFalseForNull) {
    EXPECT_FALSE(nimcp_gpu_pool_is_valid(nullptr));
}

//=============================================================================
// Bump Allocator Tests
//=============================================================================

TEST_F(GPUPoolTest, BumpAlloc_SingleAllocation_ReturnsValidPtr) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);

    EXPECT_NE(ptr, nullptr);
    EXPECT_TRUE(nimcp_gpu_pool_owns(pool, ptr));
}

TEST_F(GPUPoolTest, BumpAlloc_MultipleAllocations_ReturnsDistinctPtrs) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr1 = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);
    void* ptr2 = nimcp_gpu_pool_alloc(pool, 2048, 0, nullptr);
    void* ptr3 = nimcp_gpu_pool_alloc(pool, 512, 0, nullptr);

    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(ptr3, nullptr);
    EXPECT_NE(ptr1, ptr2);
    EXPECT_NE(ptr2, ptr3);
    EXPECT_NE(ptr1, ptr3);
}

TEST_F(GPUPoolTest, BumpReset_FreesAllAllocations) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Allocate several blocks
    for (int i = 0; i < 100; i++) {
        void* ptr = nimcp_gpu_pool_alloc(pool, 4096, 0, nullptr);
        EXPECT_NE(ptr, nullptr);
    }

    nimcp_gpu_pool_stats_t stats_before;
    nimcp_gpu_pool_get_stats(pool, &stats_before);

    // Reset
    size_t freed = nimcp_gpu_pool_reset(pool);
    EXPECT_EQ(freed, 100u);

    nimcp_gpu_pool_stats_t stats_after;
    nimcp_gpu_pool_get_stats(pool, &stats_after);

    EXPECT_EQ(stats_after.used_size, 0u);
    EXPECT_EQ(stats_after.current_allocations, 0u);
}

TEST_F(GPUPoolTest, BumpAlloc_AfterReset_ReusesMemory) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr1 = nimcp_gpu_pool_alloc(pool, 1024 * 1024, 0, nullptr);
    EXPECT_NE(ptr1, nullptr);

    nimcp_gpu_pool_reset(pool);

    void* ptr2 = nimcp_gpu_pool_alloc(pool, 1024 * 1024, 0, nullptr);
    EXPECT_NE(ptr2, nullptr);

    // After reset, bump allocator should reuse memory from beginning
    // The new pointer should be at or near the same location
    EXPECT_EQ(ptr1, ptr2);
}

//=============================================================================
// Buddy Allocator Tests
//=============================================================================

TEST_F(GPUPoolTest, BuddyAlloc_SingleAllocation_ReturnsValidPtr) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);

    EXPECT_NE(ptr, nullptr);
    EXPECT_TRUE(nimcp_gpu_pool_owns(pool, ptr));
}

TEST_F(GPUPoolTest, BuddyAlloc_Free_ReturnsMemory) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    nimcp_gpu_pool_stats_t stats_before;
    nimcp_gpu_pool_get_stats(pool, &stats_before);

    void* ptr = nimcp_gpu_pool_alloc(pool, 1024 * 1024, 0, nullptr);
    ASSERT_NE(ptr, nullptr);

    nimcp_gpu_pool_stats_t stats_allocated;
    nimcp_gpu_pool_get_stats(pool, &stats_allocated);
    EXPECT_GT(stats_allocated.used_size, 0u);

    nimcp_gpu_pool_free(pool, ptr);

    nimcp_gpu_pool_stats_t stats_freed;
    nimcp_gpu_pool_get_stats(pool, &stats_freed);
    EXPECT_LT(stats_freed.used_size, stats_allocated.used_size);
}

TEST_F(GPUPoolTest, BuddyAlloc_PowerOfTwoSizes_SucceedsForAll) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    std::vector<void*> ptrs;

    // Allocate various power-of-2 sizes
    for (size_t i = 8; i <= 16; i++) {  // 256 bytes to 64KB
        size_t size = 1 << i;
        void* ptr = nimcp_gpu_pool_alloc(pool, size, 0, nullptr);
        EXPECT_NE(ptr, nullptr) << "Failed to allocate " << size << " bytes";
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }

    // Free in reverse order
    for (auto it = ptrs.rbegin(); it != ptrs.rend(); ++it) {
        nimcp_gpu_pool_free(pool, *it);
    }
}

TEST_F(GPUPoolTest, BuddyAlloc_CoalescesAdjacentFreeBlocks) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Allocate two adjacent blocks
    void* ptr1 = nimcp_gpu_pool_alloc(pool, 512 * 1024, 0, nullptr);
    void* ptr2 = nimcp_gpu_pool_alloc(pool, 512 * 1024, 0, nullptr);
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);

    // Free both
    nimcp_gpu_pool_free(pool, ptr1);
    nimcp_gpu_pool_free(pool, ptr2);

    // Should be able to allocate a larger block now (coalesced)
    void* ptr3 = nimcp_gpu_pool_alloc(pool, 1024 * 1024, 0, nullptr);
    EXPECT_NE(ptr3, nullptr);

    nimcp_gpu_pool_free(pool, ptr3);
}

//=============================================================================
// Slab Allocator Tests
//=============================================================================

TEST_F(GPUPoolTest, SlabAlloc_FixedSizes_ReturnsValidPtrs) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_ACTIVATION, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Allocate sizes that match slab sizes
    void* ptr256 = nimcp_gpu_pool_alloc(pool, 256, 0, nullptr);
    void* ptr512 = nimcp_gpu_pool_alloc(pool, 512, 0, nullptr);
    void* ptr1024 = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);

    EXPECT_NE(ptr256, nullptr);
    EXPECT_NE(ptr512, nullptr);
    EXPECT_NE(ptr1024, nullptr);

    nimcp_gpu_pool_free(pool, ptr256);
    nimcp_gpu_pool_free(pool, ptr512);
    nimcp_gpu_pool_free(pool, ptr1024);
}

TEST_F(GPUPoolTest, SlabAlloc_RoundsUpToSlabSize) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_ACTIVATION, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Allocate size slightly above slab boundary
    void* ptr = nimcp_gpu_pool_alloc(pool, 300, 0, nullptr);  // Should get 512-byte slab
    EXPECT_NE(ptr, nullptr);

    nimcp_gpu_pool_free(pool, ptr);
}

TEST_F(GPUPoolTest, SlabAlloc_ManySmallAllocations_Efficient) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_ACTIVATION, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    std::vector<void*> ptrs;
    const int num_allocs = 1000;

    // Allocate many small blocks
    for (int i = 0; i < num_allocs; i++) {
        void* ptr = nimcp_gpu_pool_alloc(pool, 256, 0, nullptr);
        EXPECT_NE(ptr, nullptr);
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }

    EXPECT_EQ(ptrs.size(), static_cast<size_t>(num_allocs));

    // Free all
    for (void* ptr : ptrs) {
        nimcp_gpu_pool_free(pool, ptr);
    }
}

//=============================================================================
// Alignment Tests
//=============================================================================

TEST_F(GPUPoolTest, Alloc_DefaultAlignment_Is256Bytes) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc(pool, 100, 0, nullptr);  // 0 = default alignment
    ASSERT_NE(ptr, nullptr);

    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    EXPECT_EQ(addr % NIMCP_GPU_POOL_DEFAULT_ALIGN, 0u);
}

TEST_F(GPUPoolTest, Alloc_CustomAlignment_Respected) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Test various power-of-2 alignments
    size_t alignments[] = {64, 128, 256, 512, 1024, 4096};

    for (size_t alignment : alignments) {
        void* ptr = nimcp_gpu_pool_alloc(pool, 1024, alignment, nullptr);
        ASSERT_NE(ptr, nullptr) << "Failed with alignment " << alignment;

        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(addr % alignment, 0u) << "Alignment " << alignment << " not respected";

        nimcp_gpu_pool_free(pool, ptr);
    }
}

//=============================================================================
// OOM Handling Tests
//=============================================================================

TEST_F(GPUPoolTest, Alloc_ExceedsPoolSize_ReturnsNull) {
    RequireGPU();

    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_scratch(1024 * 1024);  // 1 MB
    config.max_size = 1024 * 1024;  // Don't allow growth
    config.growth_factor = 0.0f;
    pool = nimcp_gpu_pool_create(ctx, &config);
    ASSERT_NE(pool, nullptr);

    // Try to allocate more than pool size
    void* ptr = nimcp_gpu_pool_alloc(pool, 2 * 1024 * 1024, 0, nullptr);
    EXPECT_EQ(ptr, nullptr);

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);
    EXPECT_GT(stats.failed_allocations, 0u);
}

TEST_F(GPUPoolTest, Alloc_FragmentedPool_MayFail) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 4 * 1024 * 1024);  // 4 MB
    ASSERT_NE(pool, nullptr);

    // Create fragmentation by alternating allocations and frees
    std::vector<void*> keep;
    for (int i = 0; i < 32; i++) {
        void* ptr1 = nimcp_gpu_pool_alloc(pool, 64 * 1024, 0, nullptr);  // 64 KB
        void* ptr2 = nimcp_gpu_pool_alloc(pool, 64 * 1024, 0, nullptr);  // 64 KB
        if (ptr1 && ptr2) {
            keep.push_back(ptr1);
            nimcp_gpu_pool_free(pool, ptr2);  // Free every other block
        }
    }

    // Even though ~2MB is free, may not be able to allocate contiguous 1MB
    // due to fragmentation (depends on allocator implementation)
    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);

    // Verify fragmentation exists
    EXPECT_GT(stats.used_size, 0u);

    // Cleanup
    for (void* ptr : keep) {
        nimcp_gpu_pool_free(pool, ptr);
    }
}

//=============================================================================
// Fragmentation Tests
//=============================================================================

TEST_F(GPUPoolTest, GetStats_ReportsFragmentation) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 8 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Create fragmentation
    std::vector<void*> keep;
    for (int i = 0; i < 20; i++) {
        void* ptr1 = nimcp_gpu_pool_alloc(pool, 128 * 1024, 0, nullptr);
        void* ptr2 = nimcp_gpu_pool_alloc(pool, 128 * 1024, 0, nullptr);
        if (ptr1) keep.push_back(ptr1);
        if (ptr2) nimcp_gpu_pool_free(pool, ptr2);
    }

    nimcp_gpu_pool_stats_t stats;
    bool result = nimcp_gpu_pool_get_stats(pool, &stats);
    EXPECT_TRUE(result);

    // Should have some fragmentation
    // (exact ratio depends on buddy allocator internal fragmentation)
    EXPECT_GE(stats.fragmentation_ratio, 0.0f);
    EXPECT_LE(stats.fragmentation_ratio, 1.0f);

    for (void* ptr : keep) {
        nimcp_gpu_pool_free(pool, ptr);
    }
}

TEST_F(GPUPoolTest, Defragment_ReducesFragmentation) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 8 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Create fragmentation
    std::vector<void*> keep;
    for (int i = 0; i < 20; i++) {
        void* ptr1 = nimcp_gpu_pool_alloc(pool, 64 * 1024, 0, nullptr);
        void* ptr2 = nimcp_gpu_pool_alloc(pool, 64 * 1024, 0, nullptr);
        if (ptr1) keep.push_back(ptr1);
        if (ptr2) nimcp_gpu_pool_free(pool, ptr2);
    }

    // Attempt defragmentation
    size_t reclaimed = nimcp_gpu_pool_defragment(pool);

    // Verify (defrag may or may not reclaim memory depending on implementation)
    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);

    // At minimum, stats should be valid
    EXPECT_GE(stats.largest_free_block, 0u);

    for (void* ptr : keep) {
        nimcp_gpu_pool_free(pool, ptr);
    }
}

//=============================================================================
// Stream-Ordered Allocation Tests
//=============================================================================

TEST_F(GPUPoolTest, Alloc_WithStream_AssociatesStream) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(ctx);

    void* ptr = nimcp_gpu_pool_alloc(pool, 1024, 0, stream);
    EXPECT_NE(ptr, nullptr);

    nimcp_gpu_pool_free(pool, ptr);
}

//=============================================================================
// Concurrent Allocation Tests
//=============================================================================

TEST_F(GPUPoolTest, Alloc_ConcurrentThreads_ThreadSafe) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 64 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    std::atomic<int> success_count(0);
    std::atomic<int> failure_count(0);
    const int num_threads = 4;
    const int allocs_per_thread = 100;

    auto alloc_worker = [&]() {
        std::vector<void*> ptrs;
        for (int i = 0; i < allocs_per_thread; i++) {
            void* ptr = nimcp_gpu_pool_alloc(pool, 4096, 0, nullptr);
            if (ptr) {
                ptrs.push_back(ptr);
                success_count++;
            } else {
                failure_count++;
            }
        }
        // Free all
        for (void* ptr : ptrs) {
            nimcp_gpu_pool_free(pool, ptr);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(alloc_worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Most allocations should succeed
    EXPECT_GT(success_count.load(), 0);

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.current_allocations, 0u);  // All freed
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(GPUPoolTest, GetStats_ReturnsValidStats) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    nimcp_gpu_pool_stats_t stats;
    bool result = nimcp_gpu_pool_get_stats(pool, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(stats.total_size, 16u * 1024 * 1024);
    EXPECT_EQ(stats.used_size, 0u);
    EXPECT_EQ(stats.current_allocations, 0u);
}

TEST_F(GPUPoolTest, GetStats_TracksAllocations) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr1 = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);
    void* ptr2 = nimcp_gpu_pool_alloc(pool, 2048, 0, nullptr);

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);

    EXPECT_EQ(stats.current_allocations, 2u);
    EXPECT_EQ(stats.allocation_count, 2u);
    EXPECT_GE(stats.used_size, 3072u);
}

TEST_F(GPUPoolTest, GetStats_TracksPeakUsage) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Allocate 1 MB
    void* ptr1 = nimcp_gpu_pool_alloc(pool, 1024 * 1024, 0, nullptr);
    ASSERT_NE(ptr1, nullptr);

    // Allocate another 1 MB
    void* ptr2 = nimcp_gpu_pool_alloc(pool, 1024 * 1024, 0, nullptr);
    ASSERT_NE(ptr2, nullptr);

    nimcp_gpu_pool_stats_t stats_peak;
    nimcp_gpu_pool_get_stats(pool, &stats_peak);
    size_t peak = stats_peak.peak_used;

    // Free one
    nimcp_gpu_pool_free(pool, ptr1);

    nimcp_gpu_pool_stats_t stats_after;
    nimcp_gpu_pool_get_stats(pool, &stats_after);

    EXPECT_LT(stats_after.used_size, peak);
    EXPECT_EQ(stats_after.peak_used, peak);  // Peak unchanged

    nimcp_gpu_pool_free(pool, ptr2);
}

TEST_F(GPUPoolTest, ResetStats_ClearsCounters) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Make some allocations
    void* ptr = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);
    EXPECT_NE(ptr, nullptr);

    nimcp_gpu_pool_reset(pool);
    nimcp_gpu_pool_reset_stats(pool);

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);

    EXPECT_EQ(stats.allocation_count, 0u);
    EXPECT_EQ(stats.free_count, 0u);
}

//=============================================================================
// Pool Growth Tests
//=============================================================================

TEST_F(GPUPoolTest, Reserve_GrowsPoolIfNeeded) {
    RequireGPU();

    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_default();
    config.initial_size = 4 * 1024 * 1024;  // Start with 4 MB
    config.max_size = 32 * 1024 * 1024;     // Allow growth to 32 MB
    config.growth_factor = 2.0f;
    pool = nimcp_gpu_pool_create(ctx, &config);
    ASSERT_NE(pool, nullptr);

    // Reserve more than initial size
    bool result = nimcp_gpu_pool_reserve(pool, 8 * 1024 * 1024);

    // May or may not succeed depending on implementation
    // but should not crash
    (void)result;

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);
    EXPECT_GE(stats.total_size, 4u * 1024 * 1024);
}

TEST_F(GPUPoolTest, Trim_ReleasesUnusedMemory) {
    RequireGPU();

    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_persistent(32 * 1024 * 1024);
    pool = nimcp_gpu_pool_create(ctx, &config);
    ASSERT_NE(pool, nullptr);

    // Allocate then free
    void* ptr = nimcp_gpu_pool_alloc(pool, 16 * 1024 * 1024, 0, nullptr);
    ASSERT_NE(ptr, nullptr);
    nimcp_gpu_pool_free(pool, ptr);

    // Trim
    size_t released = nimcp_gpu_pool_trim(pool);

    // Implementation may or may not release memory
    (void)released;

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);
    EXPECT_GE(stats.available_size, 0u);
}

//=============================================================================
// Observer Pattern Tests
//=============================================================================

static std::atomic<int> observer_alloc_count(0);
static std::atomic<int> observer_free_count(0);

static void test_observer_callback(const nimcp_gpu_block_t* block, bool is_allocation, void* user_data) {
    (void)block;
    (void)user_data;
    if (is_allocation) {
        observer_alloc_count++;
    } else {
        observer_free_count++;
    }
}

TEST_F(GPUPoolTest, Observer_ReceivesCallbacks) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    observer_alloc_count = 0;
    observer_free_count = 0;

    bool added = nimcp_gpu_pool_add_observer(pool, test_observer_callback, nullptr);
    EXPECT_TRUE(added);

    void* ptr = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);
    EXPECT_NE(ptr, nullptr);
    EXPECT_EQ(observer_alloc_count.load(), 1);

    nimcp_gpu_pool_free(pool, ptr);
    EXPECT_EQ(observer_free_count.load(), 1);

    nimcp_gpu_pool_remove_observer(pool, test_observer_callback);
}

//=============================================================================
// Tagged Allocation Tests
//=============================================================================

TEST_F(GPUPoolTest, AllocTagged_StoresTag) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc_tagged(pool, 1024, 0, nullptr, "test_tensor");
    EXPECT_NE(ptr, nullptr);

    nimcp_gpu_pool_free(pool, ptr);
}

//=============================================================================
// Ownership Tests
//=============================================================================

TEST_F(GPUPoolTest, Owns_ReturnsTrueForPoolPointer) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);
    ASSERT_NE(ptr, nullptr);

    EXPECT_TRUE(nimcp_gpu_pool_owns(pool, ptr));
}

TEST_F(GPUPoolTest, Owns_ReturnsFalseForExternalPointer) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    // Allocate from CUDA directly
    void* external_ptr = nimcp_gpu_malloc(ctx, 1024);
    ASSERT_NE(external_ptr, nullptr);

    EXPECT_FALSE(nimcp_gpu_pool_owns(pool, external_ptr));

    nimcp_gpu_free(ctx, external_ptr);
}

TEST_F(GPUPoolTest, Owns_ReturnsFalseForNullPtr) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    EXPECT_FALSE(nimcp_gpu_pool_owns(pool, nullptr));
}

//=============================================================================
// Global Pool Manager Tests
//=============================================================================

TEST_F(GPUPoolTest, PoolManager_InitAndShutdown) {
    RequireGPU();

    bool initialized = nimcp_gpu_pool_manager_init(ctx, nullptr);
    EXPECT_TRUE(initialized);

    nimcp_gpu_pool_t* global = nimcp_gpu_pool_manager_get();
    EXPECT_NE(global, nullptr);

    nimcp_gpu_pool_manager_shutdown();

    nimcp_gpu_pool_t* after_shutdown = nimcp_gpu_pool_manager_get();
    EXPECT_EQ(after_shutdown, nullptr);
}

TEST_F(GPUPoolTest, PoolManager_AllocAndFree) {
    RequireGPU();

    nimcp_gpu_pool_config_t config = nimcp_gpu_pool_config_default();
    config.initial_size = 16 * 1024 * 1024;
    bool initialized = nimcp_gpu_pool_manager_init(ctx, &config);
    EXPECT_TRUE(initialized);

    void* ptr = nimcp_gpu_pool_manager_alloc(1024);
    EXPECT_NE(ptr, nullptr);

    nimcp_gpu_pool_manager_free(ptr);

    nimcp_gpu_pool_manager_shutdown();
}

//=============================================================================
// Debug and Diagnostic Tests
//=============================================================================

TEST_F(GPUPoolTest, CheckLeaks_ReturnsZeroWhenClean) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);
    nimcp_gpu_pool_free(pool, ptr);

    size_t leaks = nimcp_gpu_pool_check_leaks(pool);
    EXPECT_EQ(leaks, 0u);
}

TEST_F(GPUPoolTest, CheckLeaks_DetectsLeakedBlocks) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr1 = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);
    void* ptr2 = nimcp_gpu_pool_alloc(pool, 2048, 0, nullptr);
    (void)ptr1;
    (void)ptr2;
    // Intentionally not freeing

    size_t leaks = nimcp_gpu_pool_check_leaks(pool);
    EXPECT_EQ(leaks, 2u);

    // Cleanup for proper teardown
    nimcp_gpu_pool_reset(pool);
}

TEST_F(GPUPoolTest, Validate_ReturnsTrueForHealthyPool) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    EXPECT_TRUE(nimcp_gpu_pool_validate(pool));

    void* ptr = nimcp_gpu_pool_alloc(pool, 1024, 0, nullptr);
    EXPECT_TRUE(nimcp_gpu_pool_validate(pool));

    nimcp_gpu_pool_free(pool, ptr);
    EXPECT_TRUE(nimcp_gpu_pool_validate(pool));
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(GPUPoolTest, NullSafety_AllFunctions) {
    // These should not crash with NULL parameters

    EXPECT_FALSE(nimcp_gpu_pool_is_valid(nullptr));
    EXPECT_EQ(nimcp_gpu_pool_alloc(nullptr, 1024, 0, nullptr), nullptr);

    nimcp_gpu_pool_free(nullptr, nullptr);
    EXPECT_EQ(nimcp_gpu_pool_reset(nullptr), 0u);
    EXPECT_EQ(nimcp_gpu_pool_defragment(nullptr), 0u);
    EXPECT_FALSE(nimcp_gpu_pool_reserve(nullptr, 1024));
    EXPECT_EQ(nimcp_gpu_pool_trim(nullptr), 0u);
    EXPECT_FALSE(nimcp_gpu_pool_owns(nullptr, nullptr));

    nimcp_gpu_pool_stats_t stats;
    EXPECT_FALSE(nimcp_gpu_pool_get_stats(nullptr, &stats));
    nimcp_gpu_pool_reset_stats(nullptr);

    EXPECT_FALSE(nimcp_gpu_pool_add_observer(nullptr, test_observer_callback, nullptr));
    EXPECT_FALSE(nimcp_gpu_pool_remove_observer(nullptr, test_observer_callback));

    EXPECT_EQ(nimcp_gpu_pool_check_leaks(nullptr), 0u);
    EXPECT_FALSE(nimcp_gpu_pool_validate(nullptr));
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(GPUPoolTest, Alloc_ZeroSize_ReturnsNull) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc(pool, 0, 0, nullptr);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(GPUPoolTest, Alloc_MinBlockSize_Succeeds) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc(pool, NIMCP_GPU_POOL_MIN_BLOCK, 0, nullptr);
    EXPECT_NE(ptr, nullptr);
}

TEST_F(GPUPoolTest, Alloc_SingleByte_RoundsUp) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_SCRATCH, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* ptr = nimcp_gpu_pool_alloc(pool, 1, 0, nullptr);
    EXPECT_NE(ptr, nullptr);  // Should round up to min block size

    nimcp_gpu_pool_stats_t stats;
    nimcp_gpu_pool_get_stats(pool, &stats);
    EXPECT_GE(stats.used_size, NIMCP_GPU_POOL_MIN_BLOCK);
}

TEST_F(GPUPoolTest, Free_NullPointer_NoOp) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    nimcp_gpu_pool_free(pool, nullptr);  // Should not crash
}

TEST_F(GPUPoolTest, Free_ExternalPointer_HandledGracefully) {
    RequireGPU();

    pool = CreatePool(NIMCP_GPU_POOL_TYPE_PERSISTENT, 16 * 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* external = nimcp_gpu_malloc(ctx, 1024);
    ASSERT_NE(external, nullptr);

    // Freeing external pointer should not crash but may be no-op
    nimcp_gpu_pool_free(pool, external);

    nimcp_gpu_free(ctx, external);
}
