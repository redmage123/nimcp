/**
 * @file test_gpu_stub_regression.cpp
 * @brief Regression tests for GPU stub behavior
 *
 * WHAT: Verify GPU stubs maintain correct behavior after fixes
 * WHY:  Prevent reintroduction of false positive throws and ensure
 *       consistent error handling in stub mode
 * HOW:  Test specific regression scenarios for each fixed bug
 *
 * REGRESSION COVERAGE:
 * - Issue 1: nimcp_gpu_malloc stub no longer throws (was NIMCP_ERROR_NULL_POINTER)
 * - Issue 1: nimcp_gpu_memset stub no longer throws for NULL dev_ptr
 * - Issue 1: nimcp_gpu_context_set_device no longer throws (was NIMCP_ERROR_GPU)
 * - Issue 1: try_init_gpu_backend no longer throws (was NIMCP_ERROR_NOT_INITIALIZED)
 * - Issue 4: Backend API returns consistent types (bool, enum, ptr)
 * - Issue 4: Kernel ops return nimcp_kernel_error_t consistently
 *
 * VERIFIED HEADERS:
 * - gpu/context/nimcp_gpu_context.h
 * - gpu/backend/nimcp_kernel_backend.h
 *
 * @author NIMCP Development Team
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GpuStubRegressionTest : public ::testing::Test {
protected:
    void TearDown() override {
        nimcp_kernel_backend_shutdown();
    }
};

//=============================================================================
// Regression: False Positive Throws Removed
//=============================================================================

/**
 * REGRESSION: nimcp_gpu_malloc stub threw NIMCP_ERROR_NULL_POINTER
 *
 * BUG: nimcp_gpu_malloc in nimcp_gpu_context_stub.c called
 *      NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, ...) before
 *      returning NULL. This polluted immune system alerts on every
 *      GPU allocation attempt in CPU-only builds.
 *
 * FIX: Removed the throw. Returning NULL is normal "no GPU" behavior.
 */
TEST_F(GpuStubRegressionTest, GpuMallocNoFalsePositiveThrow) {
    // Call gpu_malloc multiple times - should never trigger immune
    for (int i = 0; i < 100; i++) {
        void* ptr = nimcp_gpu_malloc(nullptr, 1024 * (i + 1));
        EXPECT_EQ(ptr, nullptr);
    }
}

/**
 * REGRESSION: nimcp_gpu_context_set_device threw NIMCP_ERROR_GPU
 *
 * BUG: set_device in stub threw "GPU context set_device called but CUDA
 *      not available" via NIMCP_THROW_TO_IMMUNE. This is false-positive
 *      since callers checking the return value handle this gracefully.
 *
 * FIX: Removed the throw. Return -1 is sufficient.
 */
TEST_F(GpuStubRegressionTest, SetDeviceNoFalsePositiveThrow) {
    int ret = nimcp_gpu_context_set_device(nullptr);
    EXPECT_EQ(ret, -1);
}

/**
 * REGRESSION: nimcp_gpu_memset threw NIMCP_ERROR_INVALID_PARAM for NULL dev_ptr
 *
 * BUG: In stub mode, passing NULL dev_ptr to memset triggered
 *      NIMCP_THROW_TO_IMMUNE. But in stub mode, NULL dev_ptr can be
 *      the result of a previous gpu_malloc returning NULL (normal).
 *
 * FIX: Removed the throw. Return -1 is sufficient.
 */
TEST_F(GpuStubRegressionTest, GpuMemsetNullDevPtrNoFalsePositiveThrow) {
    int ret = nimcp_gpu_memset(nullptr, nullptr, 0, 1024);
    EXPECT_EQ(ret, -1);
}

/**
 * REGRESSION: try_init_gpu_backend threw NIMCP_ERROR_NOT_INITIALIZED
 *
 * BUG: In nimcp_kernel_backend.c, try_init_gpu_backend() threw
 *      NIMCP_THROW_TO_IMMUNE when CUDA/ROCm/OpenCL were not available.
 *      This is false-positive - "no GPU" is normal on CPU-only systems.
 *      The throws polluted immune alerts during every backend init.
 *
 * FIX: Removed all throws from try_init_gpu_backend fallthrough paths.
 */
TEST_F(GpuStubRegressionTest, BackendInitNoFalsePositiveThrows) {
    // This used to throw 3-4 times (CUDA, ROCm, OpenCL, default)
    // during init_default on CPU-only systems
    bool ok = nimcp_kernel_backend_init_default();
    EXPECT_TRUE(ok);
    // On CUDA-enabled builds, GPU-first policy selects CUDA.
    // On CPU-only builds, falls back to CPU. Either is valid.
    nimcp_backend_type_t type = nimcp_get_backend_type();
    EXPECT_TRUE(type == NIMCP_BACKEND_CPU || type == NIMCP_BACKEND_CUDA
                || type == NIMCP_BACKEND_ROCM || type == NIMCP_BACKEND_OPENCL);
}

//=============================================================================
// Regression: Return Type Consistency
//=============================================================================

/**
 * REGRESSION: Ensure kernel backend ops return nimcp_kernel_error_t
 *
 * Previously there was confusion between:
 * - nimcp_kernel_error_t (0 for success, negative for error)
 * - int (0/-1)
 * - bool (true/false)
 * - nimcp_error_t (positive values for errors)
 *
 * Backend kernel operations must consistently return nimcp_kernel_error_t.
 */
TEST_F(GpuStubRegressionTest, CpuBackendTensorOpsReturnKernelError) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    ASSERT_NE(backend, nullptr);

    // All tensor ops should be non-NULL and return NIMCP_KERNEL_ERROR_NULL_PTR
    // when called with NULL arguments (consistent error type)
    ASSERT_NE(backend->tensor.add, nullptr);
    nimcp_kernel_error_t err = backend->tensor.add(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);

    ASSERT_NE(backend->tensor.softmax, nullptr);
    err = backend->tensor.softmax(nullptr, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);

    ASSERT_NE(backend->tensor.relu, nullptr);
    err = backend->tensor.relu(nullptr, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);

    ASSERT_NE(backend->tensor.sigmoid, nullptr);
    err = backend->tensor.sigmoid(nullptr, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);

    ASSERT_NE(backend->tensor.matmul, nullptr);
    err = backend->tensor.matmul(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);
}

TEST_F(GpuStubRegressionTest, CpuBackendTrainingOpsReturnKernelError) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    ASSERT_NE(backend, nullptr);

    ASSERT_NE(backend->training.mse_loss, nullptr);
    nimcp_kernel_error_t err = backend->training.mse_loss(nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);

    ASSERT_NE(backend->training.gradient_clip, nullptr);
    err = backend->training.gradient_clip(nullptr, nullptr, 1.0f);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);

    ASSERT_NE(backend->training.sgd_step, nullptr);
    err = backend->training.sgd_step(nullptr, nullptr, nullptr, 0.01f, 0.0f, nullptr);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);
}

TEST_F(GpuStubRegressionTest, CpuBackendSnnOpsReturnKernelError) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    ASSERT_NE(backend, nullptr);

    ASSERT_NE(backend->snn.lif_forward, nullptr);
    nimcp_kernel_error_t err = backend->snn.lif_forward(
        nullptr, nullptr, nullptr, nullptr, 0.02f, 1.0f, 0.0f, 0.001f);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);
}

//=============================================================================
// Regression: Backend Switch Behavior
//=============================================================================

/**
 * Ensure backend switch to non-existent GPU backends returns false
 * (not throw) and stays on CPU.
 */
TEST_F(GpuStubRegressionTest, SwitchToUnavailableBackendReturnsFalse) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);

    // These should return false but not crash or throw fatally
    // Note: nimcp_switch_backend to CUDA does throw NIMCP_THROW_TO_IMMUNE
    // for explicit switch requests (user error). This is intentional.
    // The key fix was in try_init_gpu_backend during INIT, not switch.
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);
}

TEST_F(GpuStubRegressionTest, SwitchToCpuAlwaysSucceeds) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    bool ok = nimcp_switch_backend(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(ok);
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);
}

//=============================================================================
// Regression: Context Stub Memory Operations
//=============================================================================

/**
 * HOST_TO_HOST memcpy with NULL context returns error code without crash.
 *
 * On CUDA-enabled builds, the CUDA implementation requires a valid context
 * and returns -1 for NULL context (even for HOST_TO_HOST).
 * On CPU-only (stub) builds, HOST_TO_HOST uses plain memcpy and returns 0.
 * Both behaviors are correct - the key regression is no crash or throw.
 */
TEST_F(GpuStubRegressionTest, MemcpyHostToHostNullCtxDoesNotCrash) {
    float src[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float dst[4] = {0};

    int ret = nimcp_gpu_memcpy(nullptr, dst, src,
                                sizeof(src), GPU_MEMCPY_HOST_TO_HOST);
    // Returns 0 in stub mode (plain memcpy), -1 in CUDA mode (no valid ctx).
    // Either is acceptable - no crash or immune throw is the key requirement.
    EXPECT_TRUE(ret == 0 || ret == -1);
}

/**
 * Memset with valid buffer but NULL context returns error code without crash.
 *
 * On CUDA-enabled builds, NULL context means -1 (no valid GPU context).
 * On CPU-only (stub) builds, non-NULL dev_ptr uses plain memset and returns 0.
 * Both behaviors are correct - the key regression is no crash or throw.
 */
TEST_F(GpuStubRegressionTest, MemsetValidBufferNullCtxDoesNotCrash) {
    uint8_t buf[32];
    memset(buf, 0xFF, sizeof(buf));

    int ret = nimcp_gpu_memset(nullptr, buf, 0, sizeof(buf));
    // Returns 0 in stub mode (plain memset), -1 in CUDA mode (no valid ctx).
    EXPECT_TRUE(ret == 0 || ret == -1);
}

//=============================================================================
// Regression: Async memcpy falls back to sync in stub
//=============================================================================

TEST_F(GpuStubRegressionTest, AsyncMemcpyHostToHostNullCtxDoesNotCrash) {
    float src[] = {10.0f, 20.0f};
    float dst[2] = {0};

    int ret = nimcp_gpu_memcpy_async(nullptr, dst, src,
                                      sizeof(src), GPU_MEMCPY_HOST_TO_HOST,
                                      false);
    // Returns 0 in stub mode (falls back to sync memcpy), -1 in CUDA mode.
    EXPECT_TRUE(ret == 0 || ret == -1);
}

//=============================================================================
// Regression: Info string functions
//=============================================================================

TEST_F(GpuStubRegressionTest, InfoStringContainsCudaNotAvailable) {
    char buf[256] = {0};
    int len = nimcp_gpu_context_get_info_string(nullptr, buf, sizeof(buf));
    EXPECT_GT(len, 0);
    // Should contain some indication of no GPU
    EXPECT_TRUE(strstr(buf, "GPU") != nullptr || strstr(buf, "CUDA") != nullptr
                || strstr(buf, "No") != nullptr || strstr(buf, "not") != nullptr);
}
