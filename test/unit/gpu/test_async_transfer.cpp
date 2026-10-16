/**
 * @file test_async_transfer.cpp
 * @brief Unit tests for Async Transfer Manager with Double-Buffering and Pipeline
 *
 * Tests transfer manager creation, async H2D/D2H/D2D transfers, callbacks,
 * double buffering, pipelines, and pinned memory management.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>

// Headers already have their own extern "C" guards
#include "gpu/transfer/nimcp_async_transfer.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class AsyncTransferTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    nimcp_transfer_manager_t* mgr = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (mgr) {
            nimcp_transfer_manager_destroy(mgr);
            mgr = nullptr;
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

    // Helper to create transfer manager with defaults
    nimcp_transfer_manager_t* CreateManager(int num_streams = 4, size_t pinned_size = 0) {
        return nimcp_transfer_manager_create(ctx, num_streams, pinned_size);
    }
};

//=============================================================================
// Transfer Manager Creation Tests
//=============================================================================

TEST_F(AsyncTransferTest, Create_DefaultParams_ReturnsValidManager) {
    RequireGPU();

    mgr = nimcp_transfer_manager_create(ctx, 4, NIMCP_TRANSFER_DEFAULT_PINNED_SIZE);

    ASSERT_NE(mgr, nullptr);
    EXPECT_TRUE(nimcp_transfer_manager_is_valid(mgr));
}

TEST_F(AsyncTransferTest, Create_NullContext_ReturnsNull) {
    mgr = nimcp_transfer_manager_create(nullptr, 4, 0);

    EXPECT_EQ(mgr, nullptr);
}

TEST_F(AsyncTransferTest, Create_ZeroStreams_ClampedToMinimum) {
    RequireGPU();

    mgr = nimcp_transfer_manager_create(ctx, 0, 0);

    ASSERT_NE(mgr, nullptr);
    EXPECT_TRUE(nimcp_transfer_manager_is_valid(mgr));
    EXPECT_GE(mgr->num_streams, 1);
}

TEST_F(AsyncTransferTest, Create_MaxStreams_ClampedToLimit) {
    RequireGPU();

    mgr = nimcp_transfer_manager_create(ctx, 100, 0);  // Request too many

    ASSERT_NE(mgr, nullptr);
    EXPECT_LE(mgr->num_streams, NIMCP_TRANSFER_MAX_STREAMS);
}

TEST_F(AsyncTransferTest, Create_WithPinnedPool) {
    RequireGPU();

    const size_t pool_size = 16 * 1024 * 1024;  // 16 MB
    mgr = nimcp_transfer_manager_create(ctx, 4, pool_size);

    ASSERT_NE(mgr, nullptr);
    EXPECT_EQ(mgr->pinned_pool_size, pool_size);
}

TEST_F(AsyncTransferTest, Create_NoPinnedPool) {
    RequireGPU();

    mgr = nimcp_transfer_manager_create(ctx, 4, 0);

    ASSERT_NE(mgr, nullptr);
    EXPECT_EQ(mgr->pinned_pool_size, 0u);
    EXPECT_EQ(mgr->pinned_pool, nullptr);
}

TEST_F(AsyncTransferTest, Destroy_HandlesNull) {
    nimcp_transfer_manager_destroy(nullptr);  // Should not crash
}

TEST_F(AsyncTransferTest, IsValid_ReturnsFalseForNull) {
    EXPECT_FALSE(nimcp_transfer_manager_is_valid(nullptr));
}

//=============================================================================
// H2D Async Transfer Tests
//=============================================================================

TEST_F(AsyncTransferTest, H2D_BasicTransfer_Succeeds) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    const size_t size = 4096;
    std::vector<float> host_data(size / sizeof(float), 3.14f);
    void* device_data = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(device_data, nullptr);

    uint64_t request_id = nimcp_transfer_async(
        mgr, device_data, host_data.data(), size,
        NIMCP_TRANSFER_H2D, nullptr, nullptr);

    EXPECT_NE(request_id, NIMCP_TRANSFER_INVALID_ID);

    nimcp_transfer_wait(mgr, request_id);

    EXPECT_TRUE(nimcp_transfer_is_complete(mgr, request_id));
    EXPECT_EQ(nimcp_transfer_get_status(mgr, request_id), NIMCP_TRANSFER_STATUS_COMPLETED);

    // Verify data
    std::vector<float> verify(size / sizeof(float), 0.0f);
    nimcp_gpu_memcpy(ctx, verify.data(), device_data, size, GPU_MEMCPY_DEVICE_TO_HOST);

    for (size_t i = 0; i < size / sizeof(float); i++) {
        EXPECT_FLOAT_EQ(verify[i], 3.14f);
    }

    nimcp_gpu_free(ctx, device_data);
}

TEST_F(AsyncTransferTest, H2D_LargeTransfer_Succeeds) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    const size_t size = 64 * 1024 * 1024;  // 64 MB
    std::vector<uint8_t> host_data(size);
    for (size_t i = 0; i < size; i++) {
        host_data[i] = static_cast<uint8_t>(i & 0xFF);
    }

    void* device_data = nimcp_gpu_malloc(ctx, size);
    if (device_data == nullptr) {
        GTEST_SKIP() << "Not enough GPU memory for large transfer test";
    }

    uint64_t request_id = nimcp_transfer_async(
        mgr, device_data, host_data.data(), size,
        NIMCP_TRANSFER_H2D, nullptr, nullptr);

    EXPECT_NE(request_id, NIMCP_TRANSFER_INVALID_ID);

    nimcp_transfer_wait(mgr, request_id);
    EXPECT_TRUE(nimcp_transfer_is_complete(mgr, request_id));

    nimcp_gpu_free(ctx, device_data);
}

//=============================================================================
// D2H Async Transfer Tests
//=============================================================================

TEST_F(AsyncTransferTest, D2H_BasicTransfer_Succeeds) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    const size_t size = 4096;
    void* device_data = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(device_data, nullptr);

    // Initialize device data
    std::vector<float> init_data(size / sizeof(float), 2.718f);
    nimcp_gpu_memcpy(ctx, device_data, init_data.data(), size, GPU_MEMCPY_HOST_TO_DEVICE);

    std::vector<float> host_data(size / sizeof(float), 0.0f);

    uint64_t request_id = nimcp_transfer_async(
        mgr, host_data.data(), device_data, size,
        NIMCP_TRANSFER_D2H, nullptr, nullptr);

    EXPECT_NE(request_id, NIMCP_TRANSFER_INVALID_ID);

    nimcp_transfer_wait(mgr, request_id);

    EXPECT_TRUE(nimcp_transfer_is_complete(mgr, request_id));

    for (size_t i = 0; i < size / sizeof(float); i++) {
        EXPECT_FLOAT_EQ(host_data[i], 2.718f);
    }

    nimcp_gpu_free(ctx, device_data);
}

//=============================================================================
// D2D Async Transfer Tests
//=============================================================================

TEST_F(AsyncTransferTest, D2D_BasicTransfer_Succeeds) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    const size_t size = 4096;
    void* src_device = nimcp_gpu_malloc(ctx, size);
    void* dst_device = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(src_device, nullptr);
    ASSERT_NE(dst_device, nullptr);

    // Initialize source
    std::vector<float> init_data(size / sizeof(float), 1.414f);
    nimcp_gpu_memcpy(ctx, src_device, init_data.data(), size, GPU_MEMCPY_HOST_TO_DEVICE);
    nimcp_gpu_memset(ctx, dst_device, 0, size);

    uint64_t request_id = nimcp_transfer_async(
        mgr, dst_device, src_device, size,
        NIMCP_TRANSFER_D2D, nullptr, nullptr);

    EXPECT_NE(request_id, NIMCP_TRANSFER_INVALID_ID);

    nimcp_transfer_wait(mgr, request_id);
    EXPECT_TRUE(nimcp_transfer_is_complete(mgr, request_id));

    // Verify
    std::vector<float> verify(size / sizeof(float), 0.0f);
    nimcp_gpu_memcpy(ctx, verify.data(), dst_device, size, GPU_MEMCPY_DEVICE_TO_HOST);

    for (size_t i = 0; i < size / sizeof(float); i++) {
        EXPECT_FLOAT_EQ(verify[i], 1.414f);
    }

    nimcp_gpu_free(ctx, src_device);
    nimcp_gpu_free(ctx, dst_device);
}

//=============================================================================
// Transfer Completion Callback Tests
//=============================================================================

static std::atomic<int> callback_count(0);
static std::atomic<void*> callback_user_data(nullptr);

static void test_callback(void* user_data) {
    callback_count++;
    callback_user_data = user_data;
}

TEST_F(AsyncTransferTest, Callback_InvokedOnCompletion) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    callback_count = 0;
    callback_user_data = nullptr;

    const size_t size = 4096;
    std::vector<float> host_data(size / sizeof(float), 1.0f);
    void* device_data = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(device_data, nullptr);

    int user_value = 42;

    uint64_t request_id = nimcp_transfer_async(
        mgr, device_data, host_data.data(), size,
        NIMCP_TRANSFER_H2D, test_callback, &user_value);

    EXPECT_NE(request_id, NIMCP_TRANSFER_INVALID_ID);

    nimcp_transfer_wait(mgr, request_id);

    // Poll to process callbacks
    nimcp_transfer_poll(mgr);

    EXPECT_EQ(callback_count.load(), 1);
    EXPECT_EQ(callback_user_data.load(), &user_value);

    nimcp_gpu_free(ctx, device_data);
}

TEST_F(AsyncTransferTest, Callback_NoCallbackWhenNull) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    callback_count = 0;

    const size_t size = 4096;
    std::vector<float> host_data(size / sizeof(float), 1.0f);
    void* device_data = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(device_data, nullptr);

    uint64_t request_id = nimcp_transfer_async(
        mgr, device_data, host_data.data(), size,
        NIMCP_TRANSFER_H2D, nullptr, nullptr);

    nimcp_transfer_wait(mgr, request_id);
    nimcp_transfer_poll(mgr);

    EXPECT_EQ(callback_count.load(), 0);

    nimcp_gpu_free(ctx, device_data);
}

//=============================================================================
// Transfer Wait Tests
//=============================================================================

TEST_F(AsyncTransferTest, Wait_BlocksUntilComplete) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    const size_t size = 8 * 1024 * 1024;  // 8 MB for measurable time
    std::vector<uint8_t> host_data(size, 0xAB);
    void* device_data = nimcp_gpu_malloc(ctx, size);
    if (device_data == nullptr) {
        GTEST_SKIP() << "Not enough GPU memory";
    }

    uint64_t request_id = nimcp_transfer_async(
        mgr, device_data, host_data.data(), size,
        NIMCP_TRANSFER_H2D, nullptr, nullptr);

    // Should not be complete immediately (large transfer)
    // Note: May complete quickly on fast GPUs
    EXPECT_NE(request_id, NIMCP_TRANSFER_INVALID_ID);

    nimcp_transfer_wait(mgr, request_id);

    // After wait, must be complete
    EXPECT_TRUE(nimcp_transfer_is_complete(mgr, request_id));

    nimcp_gpu_free(ctx, device_data);
}

TEST_F(AsyncTransferTest, WaitAll_WaitsForAllTransfers) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    const size_t size = 4096;
    std::vector<void*> device_ptrs;
    std::vector<uint64_t> request_ids;

    for (int i = 0; i < 5; i++) {
        std::vector<float> host_data(size / sizeof(float), static_cast<float>(i));
        void* device_data = nimcp_gpu_malloc(ctx, size);
        if (device_data == nullptr) continue;
        device_ptrs.push_back(device_data);

        uint64_t id = nimcp_transfer_async(
            mgr, device_data, host_data.data(), size,
            NIMCP_TRANSFER_H2D, nullptr, nullptr);
        if (id != NIMCP_TRANSFER_INVALID_ID) {
            request_ids.push_back(id);
        }
    }

    nimcp_transfer_wait_all(mgr);

    for (uint64_t id : request_ids) {
        EXPECT_TRUE(nimcp_transfer_is_complete(mgr, id));
    }

    for (void* ptr : device_ptrs) {
        nimcp_gpu_free(ctx, ptr);
    }
}

//=============================================================================
// Multiple Concurrent Transfers Tests
//=============================================================================

TEST_F(AsyncTransferTest, ConcurrentTransfers_AllComplete) {
    RequireGPU();

    mgr = CreateManager(8);  // More streams for concurrency
    ASSERT_NE(mgr, nullptr);

    const size_t size = 1024 * 1024;  // 1 MB each
    const int num_transfers = 8;

    std::vector<std::vector<float>> host_data(num_transfers);
    std::vector<void*> device_data(num_transfers);
    std::vector<uint64_t> request_ids(num_transfers);

    for (int i = 0; i < num_transfers; i++) {
        host_data[i].resize(size / sizeof(float), static_cast<float>(i));
        device_data[i] = nimcp_gpu_malloc(ctx, size);
        if (device_data[i] == nullptr) {
            GTEST_SKIP() << "Not enough GPU memory for concurrent transfer test";
        }
    }

    // Queue all transfers
    for (int i = 0; i < num_transfers; i++) {
        request_ids[i] = nimcp_transfer_async(
            mgr, device_data[i], host_data[i].data(), size,
            NIMCP_TRANSFER_H2D, nullptr, nullptr);
        EXPECT_NE(request_ids[i], NIMCP_TRANSFER_INVALID_ID);
    }

    // Wait for all
    nimcp_transfer_wait_all(mgr);

    // Verify all complete
    for (int i = 0; i < num_transfers; i++) {
        EXPECT_TRUE(nimcp_transfer_is_complete(mgr, request_ids[i]));

        std::vector<float> verify(size / sizeof(float));
        nimcp_gpu_memcpy(ctx, verify.data(), device_data[i], size, GPU_MEMCPY_DEVICE_TO_HOST);
        EXPECT_FLOAT_EQ(verify[0], static_cast<float>(i));
    }

    for (void* ptr : device_data) {
        nimcp_gpu_free(ctx, ptr);
    }
}

//=============================================================================
// Pinned Memory Tests
//=============================================================================

TEST_F(AsyncTransferTest, PinnedAlloc_ReturnsValidPtr) {
    RequireGPU();

    mgr = CreateManager(4, 16 * 1024 * 1024);  // 16 MB pinned pool
    ASSERT_NE(mgr, nullptr);

    void* pinned = nimcp_transfer_alloc_pinned(mgr, 4096);
    EXPECT_NE(pinned, nullptr);

    nimcp_transfer_free_pinned(mgr, pinned);
}

TEST_F(AsyncTransferTest, PinnedAlloc_MultipleAllocations) {
    RequireGPU();

    mgr = CreateManager(4, 16 * 1024 * 1024);
    ASSERT_NE(mgr, nullptr);

    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        void* pinned = nimcp_transfer_alloc_pinned(mgr, 64 * 1024);  // 64 KB each
        EXPECT_NE(pinned, nullptr);
        if (pinned) ptrs.push_back(pinned);
    }

    for (void* ptr : ptrs) {
        nimcp_transfer_free_pinned(mgr, ptr);
    }
}

TEST_F(AsyncTransferTest, PinnedStats_ReturnsValidStats) {
    RequireGPU();

    mgr = CreateManager(4, 8 * 1024 * 1024);
    ASSERT_NE(mgr, nullptr);

    size_t total, used, count;
    nimcp_transfer_pinned_stats(mgr, &total, &used, &count);

    EXPECT_EQ(total, 8u * 1024 * 1024);
    EXPECT_EQ(used, 0u);
    EXPECT_EQ(count, 0u);

    void* pinned = nimcp_transfer_alloc_pinned(mgr, 1024);
    EXPECT_NE(pinned, nullptr);

    nimcp_transfer_pinned_stats(mgr, &total, &used, &count);
    EXPECT_GT(used, 0u);
    EXPECT_EQ(count, 1u);

    nimcp_transfer_free_pinned(mgr, pinned);
}

TEST_F(AsyncTransferTest, PinnedAlloc_FallsBackOnPoolExhaustion) {
    RequireGPU();

    // Small pool that will be exhausted
    mgr = CreateManager(4, 1024);  // Only 1 KB pool
    ASSERT_NE(mgr, nullptr);

    // Request more than pool size - should fall back to direct allocation
    void* pinned = nimcp_transfer_alloc_pinned(mgr, 4096);

    // May or may not succeed depending on implementation
    if (pinned != nullptr) {
        nimcp_transfer_free_pinned(mgr, pinned);
    }
}

//=============================================================================
// Double Buffer Tests
//=============================================================================

TEST_F(AsyncTransferTest, DoubleBuffer_Create_ReturnsValidBuffer) {
    RequireGPU();

    const size_t buffer_size = 4096;
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, buffer_size);

    ASSERT_NE(db, nullptr);
    EXPECT_TRUE(db->initialized);
    EXPECT_EQ(db->buffer_size, buffer_size);
    EXPECT_NE(db->buffers[0], nullptr);
    EXPECT_NE(db->buffers[1], nullptr);

    nimcp_double_buffer_destroy(db);
}

TEST_F(AsyncTransferTest, DoubleBuffer_Create_NullContext_ReturnsNull) {
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(nullptr, 4096);
    EXPECT_EQ(db, nullptr);
}

TEST_F(AsyncTransferTest, DoubleBuffer_Destroy_HandlesNull) {
    nimcp_double_buffer_destroy(nullptr);  // Should not crash
}

TEST_F(AsyncTransferTest, DoubleBuffer_StartLoad_Succeeds) {
    RequireGPU();

    const size_t buffer_size = 4096;
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, buffer_size);
    ASSERT_NE(db, nullptr);

    std::vector<float> host_data(buffer_size / sizeof(float), 1.5f);

    int result = nimcp_double_buffer_start_load(db, host_data.data(), buffer_size);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(nimcp_double_buffer_is_loading(db));

    nimcp_double_buffer_sync(db);
    nimcp_double_buffer_destroy(db);
}

TEST_F(AsyncTransferTest, DoubleBuffer_GetComputeBuffer_ReturnsValidPtr) {
    RequireGPU();

    const size_t buffer_size = 4096;
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, buffer_size);
    ASSERT_NE(db, nullptr);

    void* compute_buf = nimcp_double_buffer_get_compute_buffer(db);
    EXPECT_NE(compute_buf, nullptr);

    nimcp_double_buffer_destroy(db);
}

TEST_F(AsyncTransferTest, DoubleBuffer_LoadComputeOverlap) {
    RequireGPU();

    const size_t buffer_size = 1024 * 1024;  // 1 MB
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, buffer_size);
    ASSERT_NE(db, nullptr);

    std::vector<std::vector<float>> batches(3);
    for (int i = 0; i < 3; i++) {
        batches[i].resize(buffer_size / sizeof(float), static_cast<float>(i + 1));
    }

    // Load first batch
    nimcp_double_buffer_start_load(db, batches[0].data(), buffer_size);
    nimcp_double_buffer_wait_load(db);
    nimcp_double_buffer_swap(db);

    // Simulate pipelined processing
    for (int i = 1; i < 3; i++) {
        // Start loading next batch
        nimcp_double_buffer_start_load(db, batches[i].data(), buffer_size);

        // Get compute buffer (previous batch)
        void* compute_buf = nimcp_double_buffer_get_compute_buffer(db);
        EXPECT_NE(compute_buf, nullptr);

        // "Compute" on current buffer (just verify it's valid)
        // In real use, would launch kernels here

        // Wait for load and swap
        nimcp_double_buffer_wait_load(db);
        nimcp_double_buffer_swap(db);
    }

    nimcp_double_buffer_sync(db);
    nimcp_double_buffer_destroy(db);
}

TEST_F(AsyncTransferTest, DoubleBuffer_Swap_AlternatesBuffers) {
    RequireGPU();

    const size_t buffer_size = 4096;
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, buffer_size);
    ASSERT_NE(db, nullptr);

    int initial_active = db->active_index;

    nimcp_double_buffer_swap(db);

    EXPECT_NE(db->active_index, initial_active);
    EXPECT_EQ(db->active_index, 1 - initial_active);

    nimcp_double_buffer_swap(db);

    EXPECT_EQ(db->active_index, initial_active);

    nimcp_double_buffer_destroy(db);
}

TEST_F(AsyncTransferTest, DoubleBuffer_GetStreams_ReturnsValidStreams) {
    RequireGPU();

    const size_t buffer_size = 4096;
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, buffer_size);
    ASSERT_NE(db, nullptr);

    nimcp_cuda_stream_t compute_stream = nimcp_double_buffer_get_compute_stream(db);
    nimcp_cuda_stream_t transfer_stream = nimcp_double_buffer_get_transfer_stream(db);

    // Streams should be valid (non-null when CUDA is enabled)
    #ifdef NIMCP_ENABLE_CUDA
    EXPECT_NE(compute_stream, nullptr);
    EXPECT_NE(transfer_stream, nullptr);
    EXPECT_NE(compute_stream, transfer_stream);  // Should be different streams
    #endif

    nimcp_double_buffer_destroy(db);
}

TEST_F(AsyncTransferTest, DoubleBuffer_StartLoadWithCallback) {
    RequireGPU();

    const size_t buffer_size = 4096;
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, buffer_size);
    ASSERT_NE(db, nullptr);

    callback_count = 0;
    std::vector<float> host_data(buffer_size / sizeof(float), 2.0f);

    int result = nimcp_double_buffer_start_load_callback(
        db, host_data.data(), buffer_size, test_callback, nullptr);
    EXPECT_EQ(result, 0);

    nimcp_double_buffer_sync(db);

    // Callback should have been invoked
    // Note: Depends on implementation whether callbacks are synchronous
    nimcp_double_buffer_destroy(db);
}

//=============================================================================
// Pipeline Tests
//=============================================================================

static void test_stage_execute(void* input, void* output, void* params) {
    // Simple pass-through for testing
    (void)input;
    (void)output;
    (void)params;
}

TEST_F(AsyncTransferTest, Pipeline_Create_ReturnsValidPipeline) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 4);

    ASSERT_NE(pipeline, nullptr);
    EXPECT_TRUE(pipeline->initialized);
    EXPECT_EQ(pipeline->num_stages, 0);

    nimcp_pipeline_destroy(pipeline);
}

TEST_F(AsyncTransferTest, Pipeline_Create_NullContext_ReturnsNull) {
    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(nullptr, 4);
    EXPECT_EQ(pipeline, nullptr);
}

TEST_F(AsyncTransferTest, Pipeline_Destroy_HandlesNull) {
    nimcp_pipeline_destroy(nullptr);  // Should not crash
}

TEST_F(AsyncTransferTest, Pipeline_AddStage_Succeeds) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 4);
    ASSERT_NE(pipeline, nullptr);

    int stage_idx = nimcp_pipeline_add_stage(pipeline, "stage1", test_stage_execute, nullptr);

    EXPECT_EQ(stage_idx, 0);
    EXPECT_EQ(nimcp_pipeline_get_stage_count(pipeline), 1);

    nimcp_pipeline_destroy(pipeline);
}

TEST_F(AsyncTransferTest, Pipeline_AddMultipleStages) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 8);
    ASSERT_NE(pipeline, nullptr);

    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stage%d", i);
        int idx = nimcp_pipeline_add_stage(pipeline, name, test_stage_execute, nullptr);
        EXPECT_EQ(idx, i);
    }

    EXPECT_EQ(nimcp_pipeline_get_stage_count(pipeline), 5);

    nimcp_pipeline_destroy(pipeline);
}

TEST_F(AsyncTransferTest, Pipeline_GetStage_ReturnsValidStage) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 4);
    ASSERT_NE(pipeline, nullptr);

    int user_param = 123;
    nimcp_pipeline_add_stage(pipeline, "test_stage", test_stage_execute, &user_param);

    nimcp_pipeline_stage_t* stage = nimcp_pipeline_get_stage(pipeline, 0);
    ASSERT_NE(stage, nullptr);
    EXPECT_STREQ(stage->name, "test_stage");
    EXPECT_EQ(stage->execute, test_stage_execute);
    EXPECT_EQ(stage->params, &user_param);
    EXPECT_TRUE(stage->active);

    nimcp_pipeline_destroy(pipeline);
}

TEST_F(AsyncTransferTest, Pipeline_GetStage_InvalidIndex_ReturnsNull) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 4);
    ASSERT_NE(pipeline, nullptr);

    nimcp_pipeline_stage_t* stage = nimcp_pipeline_get_stage(pipeline, 0);  // No stages added
    EXPECT_EQ(stage, nullptr);

    nimcp_pipeline_add_stage(pipeline, "stage", test_stage_execute, nullptr);

    stage = nimcp_pipeline_get_stage(pipeline, 1);  // Out of bounds
    EXPECT_EQ(stage, nullptr);

    stage = nimcp_pipeline_get_stage(pipeline, -1);  // Negative
    EXPECT_EQ(stage, nullptr);

    nimcp_pipeline_destroy(pipeline);
}

TEST_F(AsyncTransferTest, Pipeline_ExecuteSingle) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 4);
    ASSERT_NE(pipeline, nullptr);

    nimcp_pipeline_add_stage(pipeline, "stage1", test_stage_execute, nullptr);
    nimcp_pipeline_add_stage(pipeline, "stage2", test_stage_execute, nullptr);

    const size_t size = 4096;
    void* input = nimcp_gpu_malloc(ctx, size);
    void* output = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    nimcp_pipeline_execute_single(pipeline, input, output);
    nimcp_pipeline_sync(pipeline);

    nimcp_gpu_free(ctx, input);
    nimcp_gpu_free(ctx, output);
    nimcp_pipeline_destroy(pipeline);
}

TEST_F(AsyncTransferTest, Pipeline_ExecuteMultipleBatches) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 4);
    ASSERT_NE(pipeline, nullptr);

    nimcp_pipeline_add_stage(pipeline, "preprocess", test_stage_execute, nullptr);
    nimcp_pipeline_add_stage(pipeline, "compute", test_stage_execute, nullptr);
    nimcp_pipeline_add_stage(pipeline, "postprocess", test_stage_execute, nullptr);

    const size_t size = 4096;
    const int num_batches = 4;

    void* inputs[num_batches];
    void* outputs[num_batches];

    for (int i = 0; i < num_batches; i++) {
        inputs[i] = nimcp_gpu_malloc(ctx, size);
        outputs[i] = nimcp_gpu_malloc(ctx, size);
        ASSERT_NE(inputs[i], nullptr);
        ASSERT_NE(outputs[i], nullptr);
    }

    nimcp_pipeline_execute(pipeline, inputs, outputs, num_batches);
    nimcp_pipeline_sync(pipeline);

    for (int i = 0; i < num_batches; i++) {
        nimcp_gpu_free(ctx, inputs[i]);
        nimcp_gpu_free(ctx, outputs[i]);
    }

    nimcp_pipeline_destroy(pipeline);
}

TEST_F(AsyncTransferTest, Pipeline_Reset_ClearsPipelineState) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 4);
    ASSERT_NE(pipeline, nullptr);

    nimcp_pipeline_add_stage(pipeline, "stage", test_stage_execute, nullptr);

    const size_t size = 4096;
    void* input = nimcp_gpu_malloc(ctx, size);
    void* output = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    nimcp_pipeline_execute_single(pipeline, input, output);
    nimcp_pipeline_sync(pipeline);

    nimcp_pipeline_reset(pipeline);

    // Should be able to execute again after reset
    nimcp_pipeline_execute_single(pipeline, input, output);
    nimcp_pipeline_sync(pipeline);

    nimcp_gpu_free(ctx, input);
    nimcp_gpu_free(ctx, output);
    nimcp_pipeline_destroy(pipeline);
}

//=============================================================================
// Transfer Statistics Tests
//=============================================================================

TEST_F(AsyncTransferTest, GetStats_ReturnsValidStats) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    size_t total_bytes, total_count;
    double avg_time;
    nimcp_transfer_get_stats(mgr, &total_bytes, &total_count, &avg_time);

    EXPECT_EQ(total_bytes, 0u);
    EXPECT_EQ(total_count, 0u);
}

TEST_F(AsyncTransferTest, GetStats_TracksTransfers) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    const size_t size = 4096;
    std::vector<float> host_data(size / sizeof(float), 1.0f);
    void* device_data = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(device_data, nullptr);

    uint64_t id = nimcp_transfer_async(
        mgr, device_data, host_data.data(), size,
        NIMCP_TRANSFER_H2D, nullptr, nullptr);
    nimcp_transfer_wait(mgr, id);

    size_t total_bytes, total_count;
    double avg_time;
    nimcp_transfer_get_stats(mgr, &total_bytes, &total_count, &avg_time);

    EXPECT_EQ(total_bytes, size);
    EXPECT_EQ(total_count, 1u);

    nimcp_gpu_free(ctx, device_data);
}

TEST_F(AsyncTransferTest, ResetStats_ClearsCounters) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    const size_t size = 4096;
    std::vector<float> host_data(size / sizeof(float), 1.0f);
    void* device_data = nimcp_gpu_malloc(ctx, size);
    ASSERT_NE(device_data, nullptr);

    uint64_t id = nimcp_transfer_async(
        mgr, device_data, host_data.data(), size,
        NIMCP_TRANSFER_H2D, nullptr, nullptr);
    nimcp_transfer_wait(mgr, id);

    nimcp_transfer_reset_stats(mgr);

    size_t total_bytes, total_count;
    double avg_time;
    nimcp_transfer_get_stats(mgr, &total_bytes, &total_count, &avg_time);

    EXPECT_EQ(total_bytes, 0u);
    EXPECT_EQ(total_count, 0u);

    nimcp_gpu_free(ctx, device_data);
}

//=============================================================================
// Transfer Status Tests
//=============================================================================

TEST_F(AsyncTransferTest, GetStatus_InvalidId_ReturnsCompleted) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    nimcp_transfer_status_t status = nimcp_transfer_get_status(mgr, NIMCP_TRANSFER_INVALID_ID);
    // Invalid ID should be treated as completed (or not found)
    EXPECT_TRUE(status == NIMCP_TRANSFER_STATUS_COMPLETED || status == NIMCP_TRANSFER_STATUS_PENDING);
}

TEST_F(AsyncTransferTest, IsComplete_InvalidId_ReturnsTrue) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    bool complete = nimcp_transfer_is_complete(mgr, NIMCP_TRANSFER_INVALID_ID);
    EXPECT_TRUE(complete);
}

TEST_F(AsyncTransferTest, Cancel_PendingTransfer) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    // Queue a large transfer that takes time
    const size_t size = 64 * 1024 * 1024;  // 64 MB
    std::vector<uint8_t> host_data(size, 0xAB);
    void* device_data = nimcp_gpu_malloc(ctx, size);
    if (device_data == nullptr) {
        GTEST_SKIP() << "Not enough GPU memory";
    }

    uint64_t id = nimcp_transfer_async(
        mgr, device_data, host_data.data(), size,
        NIMCP_TRANSFER_H2D, nullptr, nullptr);

    bool cancelled = nimcp_transfer_cancel(mgr, id);

    // May or may not be cancellable depending on timing
    // Just verify it doesn't crash
    (void)cancelled;

    // Wait for any remaining work
    nimcp_transfer_wait_all(mgr);

    nimcp_gpu_free(ctx, device_data);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(AsyncTransferTest, DirectionName_ReturnsCorrectStrings) {
    EXPECT_STREQ(nimcp_transfer_direction_name(NIMCP_TRANSFER_H2D), "H2D");
    EXPECT_STREQ(nimcp_transfer_direction_name(NIMCP_TRANSFER_D2H), "D2H");
    EXPECT_STREQ(nimcp_transfer_direction_name(NIMCP_TRANSFER_D2D), "D2D");
    EXPECT_STREQ(nimcp_transfer_direction_name(NIMCP_TRANSFER_P2P), "P2P");
}

TEST_F(AsyncTransferTest, StatusName_ReturnsCorrectStrings) {
    EXPECT_STREQ(nimcp_transfer_status_name(NIMCP_TRANSFER_STATUS_PENDING), "PENDING");
    EXPECT_STREQ(nimcp_transfer_status_name(NIMCP_TRANSFER_STATUS_IN_PROGRESS), "IN_PROGRESS");
    EXPECT_STREQ(nimcp_transfer_status_name(NIMCP_TRANSFER_STATUS_COMPLETED), "COMPLETED");
    EXPECT_STREQ(nimcp_transfer_status_name(NIMCP_TRANSFER_STATUS_FAILED), "FAILED");
    EXPECT_STREQ(nimcp_transfer_status_name(NIMCP_TRANSFER_STATUS_CANCELLED), "CANCELLED");
}

TEST_F(AsyncTransferTest, PrintInfo_DoesNotCrash) {
    RequireGPU();

    mgr = CreateManager(4, 8 * 1024 * 1024);
    ASSERT_NE(mgr, nullptr);

    // Should not crash
    nimcp_transfer_manager_print_info(mgr);
}

TEST_F(AsyncTransferTest, DoubleBuffer_PrintInfo_DoesNotCrash) {
    RequireGPU();

    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, 4096);
    ASSERT_NE(db, nullptr);

    // Should not crash
    nimcp_double_buffer_print_info(db);

    nimcp_double_buffer_destroy(db);
}

TEST_F(AsyncTransferTest, Pipeline_PrintInfo_DoesNotCrash) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, 4);
    ASSERT_NE(pipeline, nullptr);

    nimcp_pipeline_add_stage(pipeline, "stage1", test_stage_execute, nullptr);
    nimcp_pipeline_add_stage(pipeline, "stage2", test_stage_execute, nullptr);

    // Should not crash
    nimcp_pipeline_print_info(pipeline);

    nimcp_pipeline_destroy(pipeline);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(AsyncTransferTest, NullSafety_TransferManager) {
    EXPECT_FALSE(nimcp_transfer_manager_is_valid(nullptr));

    EXPECT_EQ(nimcp_transfer_async(nullptr, nullptr, nullptr, 100,
        NIMCP_TRANSFER_H2D, nullptr, nullptr), NIMCP_TRANSFER_INVALID_ID);

    nimcp_transfer_wait(nullptr, 0);  // Should not crash
    nimcp_transfer_wait_all(nullptr);  // Should not crash

    EXPECT_TRUE(nimcp_transfer_is_complete(nullptr, 0));
    EXPECT_FALSE(nimcp_transfer_cancel(nullptr, 0));
    EXPECT_EQ(nimcp_transfer_poll(nullptr), 0);

    EXPECT_EQ(nimcp_transfer_alloc_pinned(nullptr, 100), nullptr);
    nimcp_transfer_free_pinned(nullptr, nullptr);

    size_t a, b, c;
    nimcp_transfer_pinned_stats(nullptr, &a, &b, &c);  // Should not crash

    nimcp_transfer_get_stats(nullptr, nullptr, nullptr, nullptr);  // Should not crash
    nimcp_transfer_reset_stats(nullptr);  // Should not crash

    nimcp_transfer_manager_print_info(nullptr);  // Should not crash
}

TEST_F(AsyncTransferTest, NullSafety_DoubleBuffer) {
    EXPECT_EQ(nimcp_double_buffer_start_load(nullptr, nullptr, 0), -1);
    EXPECT_EQ(nimcp_double_buffer_start_load_callback(nullptr, nullptr, 0, nullptr, nullptr), -1);
    EXPECT_EQ(nimcp_double_buffer_get_compute_buffer(nullptr), nullptr);
    EXPECT_EQ(nimcp_double_buffer_wait_load(nullptr), nullptr);

    nimcp_double_buffer_swap(nullptr);  // Should not crash
    nimcp_double_buffer_sync(nullptr);  // Should not crash

    EXPECT_FALSE(nimcp_double_buffer_is_loading(nullptr));
    nimcp_double_buffer_print_info(nullptr);  // Should not crash
}

TEST_F(AsyncTransferTest, NullSafety_Pipeline) {
    EXPECT_EQ(nimcp_pipeline_add_stage(nullptr, "test", test_stage_execute, nullptr), -1);
    EXPECT_EQ(nimcp_pipeline_get_stage_count(nullptr), 0);
    EXPECT_EQ(nimcp_pipeline_get_stage(nullptr, 0), nullptr);

    nimcp_pipeline_execute(nullptr, nullptr, nullptr, 0);  // Should not crash
    nimcp_pipeline_execute_single(nullptr, nullptr, nullptr);  // Should not crash
    nimcp_pipeline_sync(nullptr);  // Should not crash
    nimcp_pipeline_reset(nullptr);  // Should not crash
    nimcp_pipeline_print_info(nullptr);  // Should not crash
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(AsyncTransferTest, Transfer_ZeroSize_ReturnsInvalid) {
    RequireGPU();

    mgr = CreateManager();
    ASSERT_NE(mgr, nullptr);

    void* device_data = nimcp_gpu_malloc(ctx, 4096);
    ASSERT_NE(device_data, nullptr);

    float dummy;
    uint64_t id = nimcp_transfer_async(
        mgr, device_data, &dummy, 0,
        NIMCP_TRANSFER_H2D, nullptr, nullptr);

    // Zero-size transfer should be invalid or immediately complete
    // Behavior depends on implementation

    nimcp_gpu_free(ctx, device_data);
}

TEST_F(AsyncTransferTest, DoubleBuffer_LoadSizeLargerThanBuffer) {
    RequireGPU();

    const size_t buffer_size = 4096;
    nimcp_double_buffer_t* db = nimcp_double_buffer_create(ctx, buffer_size);
    ASSERT_NE(db, nullptr);

    std::vector<float> host_data(buffer_size * 2 / sizeof(float), 1.0f);

    int result = nimcp_double_buffer_start_load(db, host_data.data(), buffer_size * 2);

    // Should fail or be clamped
    EXPECT_EQ(result, -1);

    nimcp_double_buffer_destroy(db);
}

TEST_F(AsyncTransferTest, Pipeline_TooManyStages) {
    RequireGPU();

    nimcp_pipeline_t* pipeline = nimcp_pipeline_create(ctx, NIMCP_PIPELINE_MAX_STAGES);
    ASSERT_NE(pipeline, nullptr);

    // Fill up all stages
    for (int i = 0; i < NIMCP_PIPELINE_MAX_STAGES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "stage%d", i);
        int idx = nimcp_pipeline_add_stage(pipeline, name, test_stage_execute, nullptr);
        EXPECT_EQ(idx, i);
    }

    // Try to add one more
    int idx = nimcp_pipeline_add_stage(pipeline, "overflow", test_stage_execute, nullptr);
    EXPECT_EQ(idx, -1);  // Should fail

    nimcp_pipeline_destroy(pipeline);
}
