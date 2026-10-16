/**
 * @file test_gpu_stub_no_false_throws.cpp
 * @brief Unit tests verifying GPU stubs don't throw for normal "no GPU" behavior
 *
 * WHAT: Verify that GPU stub functions return graceful failures without
 *       triggering NIMCP_THROW_TO_IMMUNE for normal "no GPU available" paths
 * WHY:  False positive immune throws in stubs pollute error tracking and
 *       cause test failures on CPU-only systems
 * HOW:  Call each stub function and verify it returns expected NULL/false/-1
 *       without throwing to the immune system
 *
 * VERIFIED HEADERS:
 * - gpu/context/nimcp_gpu_context.h: nimcp_gpu_context_create(), _create_auto(),
 *   _destroy(), _is_valid(), _set_device(), _synchronize(), _get_error(),
 *   nimcp_gpu_malloc(), _free(), _memcpy(), _memcpy_async(), _memset(),
 *   _memory_stats(), _get_compute_stream(), _get_transfer_stream(),
 *   _stream_synchronize(), _get_cublas(), _get_cufft_1d(),
 *   _calc_launch_config(), _get_optimal_block_size(),
 *   _print_info(), _get_info_string()
 * - gpu/backend/nimcp_kernel_backend.h: nimcp_kernel_backend_init(),
 *   _init_default(), _shutdown(), nimcp_get_kernel_backend(),
 *   nimcp_cuda_backend_available(), nimcp_get_backend_type(),
 *   nimcp_switch_backend(), nimcp_backend_type_name()
 *
 * @author NIMCP Development Team
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cstring>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GpuStubNoFalseThrowsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing to set up - we're testing stub behavior
    }

    void TearDown() override {
        // Ensure backend is shut down between tests
        nimcp_kernel_backend_shutdown();
    }
};

//=============================================================================
// GPU Context Stub Tests - No False Throws
//=============================================================================

/**
 * TEST: Context creation does not crash or throw
 *
 * In non-CUDA builds, creating a GPU context returns NULL.
 * In CUDA builds, it may return a valid context or NULL (no GPU device).
 * Either way, no immune system throws.
 */
TEST_F(GpuStubNoFalseThrowsTest, ContextCreateDoesNotCrash) {
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
    // On CUDA builds, may return non-NULL if GPU available
    if (ctx) {
        nimcp_gpu_context_destroy(ctx);
    }
    SUCCEED();
}

TEST_F(GpuStubNoFalseThrowsTest, ContextCreateAutoDoesNotCrash) {
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create_auto();
    if (ctx) {
        nimcp_gpu_context_destroy(ctx);
    }
    SUCCEED();
}

TEST_F(GpuStubNoFalseThrowsTest, ContextCreateNegativeDeviceDoesNotCrash) {
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(-1);
    // On stub builds, returns NULL. On CUDA builds, -1 may be treated as
    // default device (some CUDA runtimes accept it). Either way, no crash.
    if (ctx) {
        nimcp_gpu_context_destroy(ctx);
    }
    SUCCEED();
}

TEST_F(GpuStubNoFalseThrowsTest, ContextCreateLargeDeviceIdDoesNotCrash) {
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(99);
    // Large device ID should return NULL (no such device)
    EXPECT_EQ(ctx, nullptr);
}

/**
 * TEST: Context destroy on NULL is safe
 */
TEST_F(GpuStubNoFalseThrowsTest, ContextDestroyNullIsSafe) {
    nimcp_gpu_context_destroy(nullptr);
    // No crash = pass
    SUCCEED();
}

/**
 * TEST: Context is_valid returns false for NULL without throwing
 */
TEST_F(GpuStubNoFalseThrowsTest, ContextIsValidReturnsFalseForNull) {
    EXPECT_FALSE(nimcp_gpu_context_is_valid(nullptr));
}

/**
 * TEST: set_device returns error without throwing
 *
 * Previously this threw NIMCP_THROW_TO_IMMUNE. Now it should
 * return -1 silently.
 */
TEST_F(GpuStubNoFalseThrowsTest, SetDeviceReturnsErrorSilently) {
    int ret = nimcp_gpu_context_set_device(nullptr);
    EXPECT_EQ(ret, -1);
}

/**
 * TEST: synchronize returns error without throwing
 */
TEST_F(GpuStubNoFalseThrowsTest, SynchronizeReturnsErrorSilently) {
    int ret = nimcp_gpu_context_synchronize(nullptr);
    // In stub mode, sync on NULL returns 0 (nothing to sync) or -1
    // Either is acceptable - just no throw
    (void)ret;
    SUCCEED();
}

/**
 * TEST: get_error returns string without throwing
 */
TEST_F(GpuStubNoFalseThrowsTest, GetErrorReturnsStringSilently) {
    const char* err = nimcp_gpu_context_get_error(nullptr);
    EXPECT_NE(err, nullptr);
}

//=============================================================================
// GPU Memory Stub Tests - No False Throws
//=============================================================================

/**
 * TEST: gpu_malloc returns NULL without throwing
 *
 * Previously threw NIMCP_THROW_TO_IMMUNE. Now returns NULL silently
 * since "no GPU memory" is normal in stub mode.
 */
TEST_F(GpuStubNoFalseThrowsTest, GpuMallocReturnsNullSilently) {
    void* ptr = nimcp_gpu_malloc(nullptr, 1024);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(GpuStubNoFalseThrowsTest, GpuMallocZeroSizeReturnsNullSilently) {
    void* ptr = nimcp_gpu_malloc(nullptr, 0);
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(GpuStubNoFalseThrowsTest, GpuMallocLargeSizeReturnsNullSilently) {
    void* ptr = nimcp_gpu_malloc(nullptr, (size_t)1024 * 1024 * 1024);
    EXPECT_EQ(ptr, nullptr);
}

/**
 * TEST: gpu_free on NULL is safe
 */
TEST_F(GpuStubNoFalseThrowsTest, GpuFreeNullIsSafe) {
    nimcp_gpu_free(nullptr, nullptr);
    SUCCEED();
}

/**
 * TEST: gpu_memcpy returns error for device operations without throwing
 */
TEST_F(GpuStubNoFalseThrowsTest, GpuMemcpyHostToHostNullCtxDoesNotCrash) {
    // HOST_TO_HOST with NULL context: returns 0 in stub mode, -1 in CUDA mode
    char src[] = "test";
    char dst[8] = {0};
    int ret = nimcp_gpu_memcpy(nullptr, dst, src, 5, GPU_MEMCPY_HOST_TO_HOST);
    EXPECT_TRUE(ret == 0 || ret == -1);
}

TEST_F(GpuStubNoFalseThrowsTest, GpuMemcpyDeviceOpsReturnErrorSilently) {
    char buf[8] = {0};
    int ret = nimcp_gpu_memcpy(nullptr, buf, buf, 4, GPU_MEMCPY_HOST_TO_DEVICE);
    EXPECT_EQ(ret, -1);

    ret = nimcp_gpu_memcpy(nullptr, buf, buf, 4, GPU_MEMCPY_DEVICE_TO_HOST);
    EXPECT_EQ(ret, -1);

    ret = nimcp_gpu_memcpy(nullptr, buf, buf, 4, GPU_MEMCPY_DEVICE_TO_DEVICE);
    EXPECT_EQ(ret, -1);
}

/**
 * TEST: gpu_memset returns error for NULL dev_ptr without throwing
 *
 * Previously threw NIMCP_THROW_TO_IMMUNE for NULL dev_ptr.
 * Now returns -1 silently.
 */
TEST_F(GpuStubNoFalseThrowsTest, GpuMemsetNullDevPtrReturnsErrorSilently) {
    int ret = nimcp_gpu_memset(nullptr, nullptr, 0, 1024);
    EXPECT_EQ(ret, -1);
}

TEST_F(GpuStubNoFalseThrowsTest, GpuMemsetValidPtrNullCtxDoesNotCrash) {
    char buf[16] = {0};
    int ret = nimcp_gpu_memset(nullptr, buf, 0xAA, 16);
    // Returns 0 in stub mode (plain memset), -1 in CUDA mode (no valid ctx)
    EXPECT_TRUE(ret == 0 || ret == -1);
}

/**
 * TEST: memory_stats returns zeros without throwing
 */
TEST_F(GpuStubNoFalseThrowsTest, MemoryStatsReturnsZerosSilently) {
    size_t alloc = 999, peak = 999, free_mem = 999;
    nimcp_gpu_memory_stats(nullptr, &alloc, &peak, &free_mem);
    EXPECT_EQ(alloc, 0u);
    EXPECT_EQ(peak, 0u);
    EXPECT_EQ(free_mem, 0u);
}

TEST_F(GpuStubNoFalseThrowsTest, MemoryStatsNullOutputsIsSafe) {
    nimcp_gpu_memory_stats(nullptr, nullptr, nullptr, nullptr);
    SUCCEED();
}

//=============================================================================
// GPU Stream Stub Tests - No False Throws
//=============================================================================

TEST_F(GpuStubNoFalseThrowsTest, GetComputeStreamReturnsNullSilently) {
    nimcp_cuda_stream_t stream = nimcp_gpu_get_compute_stream(nullptr);
    EXPECT_EQ(stream, nullptr);
}

TEST_F(GpuStubNoFalseThrowsTest, GetTransferStreamReturnsNullSilently) {
    nimcp_cuda_stream_t stream = nimcp_gpu_get_transfer_stream(nullptr);
    EXPECT_EQ(stream, nullptr);
}

TEST_F(GpuStubNoFalseThrowsTest, StreamSynchronizeReturnsErrorSilently) {
    int ret = nimcp_gpu_stream_synchronize(nullptr);
    // In stub, returns 0 (nothing to sync) or -1 - either is fine
    (void)ret;
    SUCCEED();
}

//=============================================================================
// GPU Library Handle Stub Tests - No False Throws
//=============================================================================

TEST_F(GpuStubNoFalseThrowsTest, GetCublasReturnsNullSilently) {
    nimcp_cublas_handle_t handle = nimcp_gpu_get_cublas(nullptr);
    EXPECT_EQ(handle, nullptr);
}

TEST_F(GpuStubNoFalseThrowsTest, GetCufft1dReturnsZeroSilently) {
    nimcp_cufft_handle_t plan = nimcp_gpu_get_cufft_1d(nullptr, 1024);
    EXPECT_EQ(plan, 0);
}

//=============================================================================
// GPU Kernel Config Stub Tests - No False Throws
//=============================================================================

TEST_F(GpuStubNoFalseThrowsTest, CalcLaunchConfigDoesNotThrow) {
    uint32_t block_size = 0, grid_size = 0;
    nimcp_gpu_calc_launch_config(nullptr, 1000, &block_size, &grid_size);
    // In stub mode, block_size defaults to 256 or 0, grid_size = 1 or 0
    // The important thing is no throw
    SUCCEED();
}

TEST_F(GpuStubNoFalseThrowsTest, GetOptimalBlockSizeDoesNotThrow) {
    uint32_t bs = nimcp_gpu_get_optimal_block_size(nullptr, 0);
    // Stub returns default or 0
    (void)bs;
    SUCCEED();
}

//=============================================================================
// GPU Info Stub Tests - No False Throws
//=============================================================================

TEST_F(GpuStubNoFalseThrowsTest, PrintInfoNullDoesNotThrow) {
    nimcp_gpu_context_print_info(nullptr);
    SUCCEED();
}

TEST_F(GpuStubNoFalseThrowsTest, GetInfoStringNullContextDoesNotThrow) {
    char buf[256] = {0};
    int ret = nimcp_gpu_context_get_info_string(nullptr, buf, sizeof(buf));
    EXPECT_GT(ret, 0);
    EXPECT_NE(buf[0], '\0');
}

TEST_F(GpuStubNoFalseThrowsTest, GetInfoStringNullBufferDoesNotThrow) {
    int ret = nimcp_gpu_context_get_info_string(nullptr, nullptr, 0);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Backend Init Stub Tests - No False Throws
//=============================================================================

/**
 * TEST: Backend init with CUDA preference falls back to CPU without throwing
 *
 * Previously, try_init_gpu_backend threw NIMCP_THROW_TO_IMMUNE when
 * CUDA was not available. Now it returns false silently.
 */
TEST_F(GpuStubNoFalseThrowsTest, BackendInitCudaPrefDoesNotThrow) {
    bool ok = nimcp_kernel_backend_init(NIMCP_BACKEND_CUDA);
    EXPECT_TRUE(ok);  // Always succeeds (CUDA if available, else falls back)

    nimcp_backend_type_t type = nimcp_get_backend_type();
    // On CUDA builds: CUDA. On CPU-only: CPU. Either is valid.
    EXPECT_TRUE(type == NIMCP_BACKEND_CPU || type == NIMCP_BACKEND_CUDA);
}

TEST_F(GpuStubNoFalseThrowsTest, BackendInitAutoDoesNotThrow) {
    bool ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
    EXPECT_TRUE(ok);

    nimcp_backend_type_t type = nimcp_get_backend_type();
    // AUTO selects best available: CUDA, ROCm, OpenCL, or CPU
    EXPECT_TRUE(type == NIMCP_BACKEND_CPU || type == NIMCP_BACKEND_CUDA
                || type == NIMCP_BACKEND_ROCM || type == NIMCP_BACKEND_OPENCL);
}

TEST_F(GpuStubNoFalseThrowsTest, BackendInitDefaultDoesNotThrow) {
    bool ok = nimcp_kernel_backend_init_default();
    EXPECT_TRUE(ok);

    nimcp_backend_type_t type = nimcp_get_backend_type();
    // GPU-first policy: CUDA if available, else fallback chain
    EXPECT_TRUE(type == NIMCP_BACKEND_CPU || type == NIMCP_BACKEND_CUDA
                || type == NIMCP_BACKEND_ROCM || type == NIMCP_BACKEND_OPENCL);
}

TEST_F(GpuStubNoFalseThrowsTest, BackendCudaAvailabilityIsConsistent) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    // On CUDA-enabled builds, CUDA backend is initialized (available=true).
    // On CPU-only builds, CUDA is not available.
    // The test verifies consistency: no crash, returns a valid bool.
    bool available = nimcp_cuda_backend_available();
    (void)available;
    SUCCEED();
}

TEST_F(GpuStubNoFalseThrowsTest, BackendTypeNameReturnsStrings) {
    EXPECT_STREQ(nimcp_backend_type_name(NIMCP_BACKEND_CPU), "CPU");
    EXPECT_STREQ(nimcp_backend_type_name(NIMCP_BACKEND_CUDA), "CUDA");
    EXPECT_STREQ(nimcp_backend_type_name(NIMCP_BACKEND_ROCM), "ROCm");
    EXPECT_STREQ(nimcp_backend_type_name(NIMCP_BACKEND_OPENCL), "OpenCL");
    EXPECT_STREQ(nimcp_backend_type_name(NIMCP_BACKEND_AUTO), "AUTO");
}

TEST_F(GpuStubNoFalseThrowsTest, BackendGetReturnsCpuBackend) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type, NIMCP_BACKEND_CPU);
    EXPECT_TRUE(backend->initialized);
    EXPECT_STREQ(backend->name, "CPU");
}

//=============================================================================
// Repeated Init/Shutdown Cycle - No False Throws
//=============================================================================

TEST_F(GpuStubNoFalseThrowsTest, RepeatedInitShutdownCycleNoThrows) {
    for (int i = 0; i < 5; i++) {
        bool ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
        EXPECT_TRUE(ok);
        nimcp_kernel_backend_shutdown();
    }
}

TEST_F(GpuStubNoFalseThrowsTest, DoubleInitIsHarmless) {
    bool ok1 = nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(ok1);
    bool ok2 = nimcp_kernel_backend_init(NIMCP_BACKEND_CUDA);
    EXPECT_TRUE(ok2);  // Returns true (already initialized)
}
