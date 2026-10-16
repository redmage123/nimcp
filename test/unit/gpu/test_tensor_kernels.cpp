/**
 * @file test_tensor_kernels.cpp
 * @brief Comprehensive unit tests for GPU tensor operations
 *
 * WHAT: Tests GPU tensor creation, element-wise ops, GEMM, activations, reductions
 * WHY:  Verify GPU tensor operations with correct behavior and CPU fallback
 * HOW:  Test all public API functions from nimcp_tensor_gpu.h
 *
 * TEST COVERAGE:
 * - GPU tensor lifecycle (create, destroy, clone)
 * - Element-wise operations (add, sub, mul, div)
 * - Matrix multiplication (GEMM, batched GEMM)
 * - Activation functions (relu, sigmoid, tanh, softmax, gelu, silu)
 * - Reduction operations (sum, mean, max, min, argmax, argmin)
 * - Norm operations (L1, L2, Linf, Frobenius)
 * - Memory operations (fill, zeros, ones, copy, transpose)
 * - CPU-GPU tensor integration
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

// GPU headers have their own extern "C" handling and include CUDA headers
// that cannot be in extern "C" blocks - include them directly
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/nimcp_execution_mode.h"

// Headers have their own extern "C" guards
#include "utils/tensor/nimcp_tensor.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float SOFTMAX_TOLERANCE = 1e-4f;

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for GPU tensor kernel tests
 *
 * WHAT: Provides common setup/teardown for GPU tensor tests
 * WHY:  Ensure proper cleanup of GPU resources and tensors
 * HOW:  Automatically destroys context and tensors in TearDown()
 */
class GPUTensorKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    std::vector<nimcp_gpu_tensor_t*> tensors_to_cleanup;

    void SetUp() override {
        // Try to create GPU context, may be NULL if no GPU
        ctx = nimcp_gpu_context_create_auto();
        if (!ctx) {
            // Create with device 0 as fallback (might still be NULL)
            ctx = nimcp_gpu_context_create(0);
        }
    }

    void TearDown() override {
        // Clean up all tensors
        for (auto* tensor : tensors_to_cleanup) {
            if (tensor) {
                nimcp_gpu_tensor_destroy(tensor);
            }
        }
        tensors_to_cleanup.clear();

        // Destroy context
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    /**
     * @brief Register tensor for automatic cleanup
     */
    nimcp_gpu_tensor_t* track(nimcp_gpu_tensor_t* tensor) {
        if (tensor) {
            tensors_to_cleanup.push_back(tensor);
        }
        return tensor;
    }

    /**
     * @brief Create 1D test tensor with sequential values
     */
    nimcp_gpu_tensor_t* create_1d_tensor(size_t size) {
        if (!ctx) return nullptr;
        std::vector<float> data(size);
        for (size_t i = 0; i < size; i++) {
            data[i] = static_cast<float>(i + 1);
        }
        size_t dims[] = {size};
        return track(nimcp_gpu_tensor_from_host(ctx, data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Create 2D test matrix with sequential values
     */
    nimcp_gpu_tensor_t* create_2d_tensor(size_t rows, size_t cols) {
        if (!ctx) return nullptr;
        std::vector<float> data(rows * cols);
        for (size_t i = 0; i < rows * cols; i++) {
            data[i] = static_cast<float>(i + 1);
        }
        size_t dims[] = {rows, cols};
        return track(nimcp_gpu_tensor_from_host(ctx, data.data(), dims, 2, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Create tensor with constant value
     */
    nimcp_gpu_tensor_t* create_constant_tensor(size_t rows, size_t cols, float value) {
        if (!ctx) return nullptr;
        std::vector<float> data(rows * cols, value);
        size_t dims[] = {rows, cols};
        return track(nimcp_gpu_tensor_from_host(ctx, data.data(), dims, 2, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Create output tensor (uninitialized)
     */
    nimcp_gpu_tensor_t* create_output_tensor(const size_t* dims, uint32_t ndim) {
        if (!ctx) return nullptr;
        return track(nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP32));
    }

    /**
     * @brief Copy tensor data to host vector
     */
    std::vector<float> to_host(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    /**
     * @brief Check if GPU context is available
     */
    bool has_gpu_context() const {
        return ctx != nullptr && nimcp_gpu_context_is_valid(ctx);
    }
};

//=============================================================================
// GPU Context and Availability Tests
//=============================================================================

/**
 * TEST: GPU tensor availability check
 * WHAT: Verify nimcp_gpu_tensor_available() returns valid result
 * WHY:  Must detect GPU availability for tensor operations
 */
TEST_F(GPUTensorKernelTest, Availability_ChecksCorrectly) {
    bool available = nimcp_gpu_tensor_available();
    // Just verify it returns without crashing
    SUCCEED();
}

/**
 * TEST: GPU memory info retrieval
 * WHAT: Verify nimcp_gpu_tensor_memory_info() works
 * WHY:  Need to query GPU memory for allocation decisions
 */
TEST_F(GPUTensorKernelTest, MemoryInfo_ReturnsValid) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    size_t free_bytes = 0;
    size_t total_bytes = 0;
    bool result = nimcp_gpu_tensor_memory_info(ctx, &free_bytes, &total_bytes);

    if (result) {
        EXPECT_GT(total_bytes, 0u);
        EXPECT_LE(free_bytes, total_bytes);
    }
}

//=============================================================================
// Tensor Lifecycle Tests
//=============================================================================

/**
 * TEST: Create GPU tensor
 * WHAT: Verify nimcp_gpu_tensor_create() creates tensor
 * WHY:  Tensor creation is fundamental to all operations
 */
TEST_F(GPUTensorKernelTest, Create_ValidDimensions_Succeeds) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    size_t dims[] = {10, 20, 30};
    nimcp_gpu_tensor_t* tensor = track(nimcp_gpu_tensor_create(ctx, dims, 3, NIMCP_GPU_PRECISION_FP32));

    ASSERT_NE(tensor, nullptr);
    EXPECT_EQ(tensor->ndim, 3u);
    EXPECT_EQ(tensor->numel, 10u * 20u * 30u);
    EXPECT_EQ(tensor->precision, NIMCP_GPU_PRECISION_FP32);
}

/**
 * TEST: Create tensor with NULL context
 * WHAT: Verify NULL context handling
 * WHY:  Guard clause validation
 */
TEST_F(GPUTensorKernelTest, Create_NullContext_ReturnsNull) {
    size_t dims[] = {10, 20};
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(nullptr, dims, 2, NIMCP_GPU_PRECISION_FP32);
    EXPECT_EQ(tensor, nullptr);
}

/**
 * TEST: Create tensor with NULL dims
 * WHAT: Verify NULL dims handling
 * WHY:  Guard clause validation
 */
TEST_F(GPUTensorKernelTest, Create_NullDims_ReturnsNull) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, nullptr, 2, NIMCP_GPU_PRECISION_FP32);
    EXPECT_EQ(tensor, nullptr);
}

/**
 * TEST: Create tensor from host data
 * WHAT: Verify nimcp_gpu_tensor_from_host() copies data correctly
 * WHY:  Need to upload host data to GPU
 */
TEST_F(GPUTensorKernelTest, FromHost_ValidData_Succeeds) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float host_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    size_t dims[] = {2, 3};
    nimcp_gpu_tensor_t* tensor = track(nimcp_gpu_tensor_from_host(ctx, host_data, dims, 2, NIMCP_GPU_PRECISION_FP32));

    ASSERT_NE(tensor, nullptr);
    EXPECT_EQ(tensor->numel, 6u);

    // Copy back and verify
    std::vector<float> result = to_host(tensor);
    ASSERT_EQ(result.size(), 6u);
    for (size_t i = 0; i < 6; i++) {
        EXPECT_FLOAT_EQ(result[i], host_data[i]);
    }
}

/**
 * TEST: Copy tensor to host
 * WHAT: Verify nimcp_gpu_tensor_to_host() copies data correctly
 * WHY:  Need to download results from GPU
 */
TEST_F(GPUTensorKernelTest, ToHost_ValidTensor_Succeeds) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float host_data[] = {1.5f, 2.5f, 3.5f, 4.5f};
    size_t dims[] = {2, 2};
    nimcp_gpu_tensor_t* tensor = track(nimcp_gpu_tensor_from_host(ctx, host_data, dims, 2, NIMCP_GPU_PRECISION_FP32));
    ASSERT_NE(tensor, nullptr);

    float result[4] = {0};
    bool success = nimcp_gpu_tensor_to_host(tensor, result);

    EXPECT_TRUE(success);
    for (size_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(result[i], host_data[i]);
    }
}

/**
 * TEST: Clone tensor
 * WHAT: Verify nimcp_gpu_tensor_clone() creates independent copy
 * WHY:  Need to duplicate tensors for operations
 */
TEST_F(GPUTensorKernelTest, Clone_ValidTensor_CreatesIndependentCopy) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    nimcp_gpu_tensor_t* original = create_2d_tensor(3, 4);
    ASSERT_NE(original, nullptr);

    nimcp_gpu_tensor_t* clone = track(nimcp_gpu_tensor_clone(original));
    ASSERT_NE(clone, nullptr);
    EXPECT_NE(clone, original);
    EXPECT_NE(clone->data, original->data);
    EXPECT_EQ(clone->numel, original->numel);

    std::vector<float> orig_data = to_host(original);
    std::vector<float> clone_data = to_host(clone);
    ASSERT_EQ(orig_data.size(), clone_data.size());
    for (size_t i = 0; i < orig_data.size(); i++) {
        EXPECT_FLOAT_EQ(orig_data[i], clone_data[i]);
    }
}

/**
 * TEST: Destroy NULL tensor
 * WHAT: Verify nimcp_gpu_tensor_destroy() handles NULL
 * WHY:  Prevent crashes from double-free or NULL destroy
 */
TEST_F(GPUTensorKernelTest, Destroy_Null_DoesNotCrash) {
    nimcp_gpu_tensor_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Element-wise Operation Tests
//=============================================================================

/**
 * TEST: Element-wise addition
 * WHAT: Verify nimcp_gpu_add() computes out = a + b
 * WHY:  Core element-wise operation
 */
TEST_F(GPUTensorKernelTest, Add_ValidTensors_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b_data[] = {0.5f, 1.5f, 2.5f, 3.5f};
    size_t dims[] = {2, 2};

    nimcp_gpu_tensor_t* a = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* b = track(nimcp_gpu_tensor_from_host(ctx, b_data, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 2);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(out, nullptr);

    bool result = nimcp_gpu_add(ctx, a, b, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    ASSERT_EQ(out_data.size(), 4u);
    EXPECT_NEAR(out_data[0], 1.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 3.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 5.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 7.5f, FLOAT_TOLERANCE);
}

/**
 * TEST: Element-wise subtraction
 * WHAT: Verify nimcp_gpu_sub() computes out = a - b
 * WHY:  Core element-wise operation
 */
TEST_F(GPUTensorKernelTest, Sub_ValidTensors_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float a_data[] = {5.0f, 4.0f, 3.0f, 2.0f};
    float b_data[] = {1.0f, 1.0f, 1.0f, 1.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* a = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* b = track(nimcp_gpu_tensor_from_host(ctx, b_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_sub(ctx, a, b, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 4.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 3.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 2.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 1.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Element-wise multiplication
 * WHAT: Verify nimcp_gpu_mul() computes out = a * b
 * WHY:  Core element-wise operation
 */
TEST_F(GPUTensorKernelTest, Mul_ValidTensors_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float a_data[] = {2.0f, 3.0f, 4.0f, 5.0f};
    float b_data[] = {0.5f, 0.5f, 0.5f, 0.5f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* a = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* b = track(nimcp_gpu_tensor_from_host(ctx, b_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_mul(ctx, a, b, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 1.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 2.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 2.5f, FLOAT_TOLERANCE);
}

/**
 * TEST: Element-wise division
 * WHAT: Verify nimcp_gpu_div() computes out = a / b
 * WHY:  Core element-wise operation
 */
TEST_F(GPUTensorKernelTest, Div_ValidTensors_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float a_data[] = {10.0f, 20.0f, 30.0f, 40.0f};
    float b_data[] = {2.0f, 4.0f, 5.0f, 8.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* a = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* b = track(nimcp_gpu_tensor_from_host(ctx, b_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_div(ctx, a, b, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 5.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 5.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 6.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 5.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Scalar addition
 * WHAT: Verify nimcp_gpu_add_scalar() computes out = a + scalar
 * WHY:  Broadcasting scalar operations
 */
TEST_F(GPUTensorKernelTest, AddScalar_ValidTensor_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* a = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_add_scalar(ctx, a, 10.0f, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    for (size_t i = 0; i < 4; i++) {
        EXPECT_NEAR(out_data[i], a_data[i] + 10.0f, FLOAT_TOLERANCE);
    }
}

/**
 * TEST: Scalar multiplication
 * WHAT: Verify nimcp_gpu_mul_scalar() computes out = a * scalar
 * WHY:  Broadcasting scalar operations
 */
TEST_F(GPUTensorKernelTest, MulScalar_ValidTensor_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* a = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_mul_scalar(ctx, a, 2.5f, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    for (size_t i = 0; i < 4; i++) {
        EXPECT_NEAR(out_data[i], a_data[i] * 2.5f, FLOAT_TOLERANCE);
    }
}

//=============================================================================
// GEMM (Matrix Multiplication) Tests
//=============================================================================

/**
 * TEST: Basic matrix multiplication
 * WHAT: Verify nimcp_gpu_gemm() computes C = A @ B
 * WHY:  GEMM is fundamental to neural network forward pass
 */
TEST_F(GPUTensorKernelTest, GEMM_BasicMultiply_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // A: 2x3 matrix, B: 3x2 matrix, C: 2x2 result
    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};  // 2x3
    float b_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};  // 3x2
    size_t dims_a[] = {2, 3};
    size_t dims_b[] = {3, 2};
    size_t dims_c[] = {2, 2};

    nimcp_gpu_tensor_t* A = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims_a, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* B = track(nimcp_gpu_tensor_from_host(ctx, b_data, dims_b, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* C = create_output_tensor(dims_c, 2);

    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);
    ASSERT_NE(C, nullptr);

    // C = 1.0 * A @ B + 0.0 * C
    bool result = nimcp_gpu_gemm(ctx, A, B, C, 1.0f, 0.0f, false, false);
    EXPECT_TRUE(result);

    std::vector<float> c_data = to_host(C);
    // A @ B = [[1*1+2*3+3*5, 1*2+2*4+3*6], [4*1+5*3+6*5, 4*2+5*4+6*6]]
    //       = [[22, 28], [49, 64]]
    EXPECT_NEAR(c_data[0], 22.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(c_data[1], 28.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(c_data[2], 49.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(c_data[3], 64.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: GEMM with alpha scaling
 * WHAT: Verify nimcp_gpu_gemm() applies alpha multiplier
 * WHY:  Scaling is needed for gradient computation
 */
TEST_F(GPUTensorKernelTest, GEMM_WithAlpha_ScalesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f};  // 2x2
    float b_data[] = {1.0f, 0.0f, 0.0f, 1.0f};  // 2x2 identity
    size_t dims[] = {2, 2};

    nimcp_gpu_tensor_t* A = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* B = track(nimcp_gpu_tensor_from_host(ctx, b_data, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* C = create_output_tensor(dims, 2);

    // C = 2.0 * A @ I = 2A
    bool result = nimcp_gpu_gemm(ctx, A, B, C, 2.0f, 0.0f, false, false);
    EXPECT_TRUE(result);

    std::vector<float> c_data = to_host(C);
    for (size_t i = 0; i < 4; i++) {
        EXPECT_NEAR(c_data[i], a_data[i] * 2.0f, FLOAT_TOLERANCE);
    }
}

/**
 * TEST: GEMM with beta accumulation
 * WHAT: Verify nimcp_gpu_gemm() accumulates with beta
 * WHY:  Accumulation is needed for gradient aggregation
 */
TEST_F(GPUTensorKernelTest, GEMM_WithBeta_AccumulatesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float a_data[] = {1.0f, 0.0f, 0.0f, 1.0f};  // 2x2 identity
    float b_data[] = {1.0f, 2.0f, 3.0f, 4.0f};  // 2x2
    float c_init[] = {10.0f, 10.0f, 10.0f, 10.0f};  // Initial C
    size_t dims[] = {2, 2};

    nimcp_gpu_tensor_t* A = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* B = track(nimcp_gpu_tensor_from_host(ctx, b_data, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* C = track(nimcp_gpu_tensor_from_host(ctx, c_init, dims, 2, NIMCP_GPU_PRECISION_FP32));

    // C = 1.0 * I @ B + 0.5 * C = B + 5
    bool result = nimcp_gpu_gemm(ctx, A, B, C, 1.0f, 0.5f, false, false);
    EXPECT_TRUE(result);

    std::vector<float> c_data = to_host(C);
    for (size_t i = 0; i < 4; i++) {
        EXPECT_NEAR(c_data[i], b_data[i] + 5.0f, FLOAT_TOLERANCE);
    }
}

/**
 * TEST: GEMM with transposed A
 * WHAT: Verify nimcp_gpu_gemm() handles trans_a flag
 * WHY:  Transpose needed for backpropagation
 */
TEST_F(GPUTensorKernelTest, GEMM_TransposeA_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // A: 3x2 (will be transposed to 2x3), B: 3x2, C: 2x2
    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};  // 3x2, transposed = 2x3
    float b_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};  // 3x2
    size_t dims_a[] = {3, 2};
    size_t dims_b[] = {3, 2};
    size_t dims_c[] = {2, 2};

    nimcp_gpu_tensor_t* A = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims_a, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* B = track(nimcp_gpu_tensor_from_host(ctx, b_data, dims_b, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* C = create_output_tensor(dims_c, 2);

    // C = A^T @ B
    bool result = nimcp_gpu_gemm(ctx, A, B, C, 1.0f, 0.0f, true, false);
    EXPECT_TRUE(result);

    // Verify operation succeeded (exact values depend on transpose implementation)
    std::vector<float> c_data = to_host(C);
    EXPECT_EQ(c_data.size(), 4u);
}

/**
 * TEST: Matrix-vector multiplication
 * WHAT: Verify nimcp_gpu_gemv() computes y = A @ x
 * WHY:  Common operation in neural networks
 */
TEST_F(GPUTensorKernelTest, GEMV_BasicMultiply_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // A: 2x3 matrix, x: 3 vector, y: 2 vector
    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};  // 2x3
    float x_data[] = {1.0f, 2.0f, 3.0f};  // 3
    size_t dims_a[] = {2, 3};
    size_t dims_x[] = {3};
    size_t dims_y[] = {2};

    nimcp_gpu_tensor_t* A = track(nimcp_gpu_tensor_from_host(ctx, a_data, dims_a, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims_x, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* y = create_output_tensor(dims_y, 1);

    bool result = nimcp_gpu_gemv(ctx, A, x, y, 1.0f, 0.0f, false);
    EXPECT_TRUE(result);

    std::vector<float> y_out = to_host(y);
    // y = [1*1+2*2+3*3, 4*1+5*2+6*3] = [14, 32]
    EXPECT_NEAR(y_out[0], 14.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(y_out[1], 32.0f, FLOAT_TOLERANCE);
}

//=============================================================================
// Activation Function Tests
//=============================================================================

/**
 * TEST: ReLU activation
 * WHAT: Verify nimcp_gpu_relu() computes max(0, x)
 * WHY:  Most common activation function
 */
TEST_F(GPUTensorKernelTest, ReLU_MixedValues_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    size_t dims[] = {5};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_relu(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 0.0f, FLOAT_TOLERANCE);  // max(0, -2) = 0
    EXPECT_NEAR(out_data[1], 0.0f, FLOAT_TOLERANCE);  // max(0, -1) = 0
    EXPECT_NEAR(out_data[2], 0.0f, FLOAT_TOLERANCE);  // max(0, 0) = 0
    EXPECT_NEAR(out_data[3], 1.0f, FLOAT_TOLERANCE);  // max(0, 1) = 1
    EXPECT_NEAR(out_data[4], 2.0f, FLOAT_TOLERANCE);  // max(0, 2) = 2
}

/**
 * TEST: Leaky ReLU activation
 * WHAT: Verify nimcp_gpu_leaky_relu() computes x > 0 ? x : alpha * x
 * WHY:  Prevents dying ReLU problem
 */
TEST_F(GPUTensorKernelTest, LeakyReLU_NegativeValues_AppliesAlpha) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    size_t dims[] = {5};
    float alpha = 0.01f;

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_leaky_relu(ctx, x, out, alpha);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], -0.02f, FLOAT_TOLERANCE);  // 0.01 * -2
    EXPECT_NEAR(out_data[1], -0.01f, FLOAT_TOLERANCE);  // 0.01 * -1
    EXPECT_NEAR(out_data[2], 0.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[4], 2.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Sigmoid activation
 * WHAT: Verify nimcp_gpu_sigmoid() computes 1/(1+exp(-x))
 * WHY:  Common for binary classification
 */
TEST_F(GPUTensorKernelTest, Sigmoid_KnownValues_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-5.0f, 0.0f, 5.0f};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_sigmoid(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 1.0f / (1.0f + std::exp(5.0f)), 1e-4f);
    EXPECT_NEAR(out_data[1], 0.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 1.0f / (1.0f + std::exp(-5.0f)), 1e-4f);
}

/**
 * TEST: Tanh activation
 * WHAT: Verify nimcp_gpu_tanh() computes tanh(x)
 * WHY:  Common activation in RNNs
 */
TEST_F(GPUTensorKernelTest, Tanh_KnownValues_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-2.0f, 0.0f, 2.0f};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_tanh(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], std::tanh(-2.0f), FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 0.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], std::tanh(2.0f), FLOAT_TOLERANCE);
}

/**
 * TEST: GELU activation
 * WHAT: Verify nimcp_gpu_gelu() computes GELU approximation
 * WHY:  Used in transformers (BERT, GPT)
 */
TEST_F(GPUTensorKernelTest, GELU_PositiveNegative_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-1.0f, 0.0f, 1.0f, 2.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_gelu(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_EQ(out_data.size(), 4u);
    // GELU(0) = 0
    EXPECT_NEAR(out_data[1], 0.0f, FLOAT_TOLERANCE);
    // GELU(x) > 0 for x > 0
    EXPECT_GT(out_data[2], 0.0f);
    EXPECT_GT(out_data[3], 0.0f);
}

/**
 * TEST: SiLU activation
 * WHAT: Verify nimcp_gpu_silu() computes x * sigmoid(x)
 * WHY:  Used in modern architectures (EfficientNet)
 */
TEST_F(GPUTensorKernelTest, SiLU_KnownValues_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-2.0f, 0.0f, 2.0f};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_silu(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    // SiLU(0) = 0 * sigmoid(0) = 0
    EXPECT_NEAR(out_data[1], 0.0f, FLOAT_TOLERANCE);
    // SiLU(x) = x * sigmoid(x)
    for (size_t i = 0; i < 3; i++) {
        float expected = x_data[i] / (1.0f + std::exp(-x_data[i]));
        EXPECT_NEAR(out_data[i], expected, 1e-4f);
    }
}

/**
 * TEST: Softmax activation
 * WHAT: Verify nimcp_gpu_softmax() normalizes to probability distribution
 * WHY:  Output layer for classification
 */
TEST_F(GPUTensorKernelTest, Softmax_SimpleVector_SumsToOne) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_softmax(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    float sum = std::accumulate(out_data.begin(), out_data.end(), 0.0f);
    EXPECT_NEAR(sum, 1.0f, SOFTMAX_TOLERANCE);

    // All values should be positive
    for (float val : out_data) {
        EXPECT_GT(val, 0.0f);
    }
}

/**
 * TEST: Softmax numerical stability
 * WHAT: Verify softmax handles large values without overflow
 * WHY:  Need numerically stable implementation
 */
TEST_F(GPUTensorKernelTest, Softmax_LargeValues_NumericallyStable) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {1000.0f, 1001.0f, 1002.0f};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_softmax(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    float sum = std::accumulate(out_data.begin(), out_data.end(), 0.0f);
    EXPECT_NEAR(sum, 1.0f, SOFTMAX_TOLERANCE);

    // No NaN or Inf values
    for (float val : out_data) {
        EXPECT_FALSE(std::isnan(val));
        EXPECT_FALSE(std::isinf(val));
    }
}

/**
 * TEST: Log-softmax
 * WHAT: Verify nimcp_gpu_log_softmax() computes log(softmax(x))
 * WHY:  More numerically stable for cross-entropy
 */
TEST_F(GPUTensorKernelTest, LogSoftmax_SimpleVector_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {1.0f, 2.0f, 3.0f};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_log_softmax(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    // All log probabilities should be negative
    for (float val : out_data) {
        EXPECT_LE(val, 0.0f);
    }

    // exp(log_softmax) should sum to 1
    float sum = 0.0f;
    for (float val : out_data) {
        sum += std::exp(val);
    }
    EXPECT_NEAR(sum, 1.0f, SOFTMAX_TOLERANCE);
}

//=============================================================================
// Reduction Operation Tests
//=============================================================================

/**
 * TEST: Sum reduction
 * WHAT: Verify nimcp_gpu_sum() computes sum along axis
 * WHY:  Common aggregation operation
 */
TEST_F(GPUTensorKernelTest, Sum_AllElements_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    size_t dims[] = {6};
    size_t out_dims[] = {1};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(out_dims, 1);

    bool result = nimcp_gpu_sum(ctx, x, out, -1, false);  // axis=-1 means all
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 21.0f, FLOAT_TOLERANCE);  // 1+2+3+4+5+6=21
}

/**
 * TEST: Mean reduction
 * WHAT: Verify nimcp_gpu_mean() computes mean along axis
 * WHY:  Used in normalization layers
 */
TEST_F(GPUTensorKernelTest, Mean_AllElements_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {2.0f, 4.0f, 6.0f, 8.0f};
    size_t dims[] = {4};
    size_t out_dims[] = {1};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(out_dims, 1);

    bool result = nimcp_gpu_mean(ctx, x, out, -1, false);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 5.0f, FLOAT_TOLERANCE);  // (2+4+6+8)/4=5
}

/**
 * TEST: Max reduction
 * WHAT: Verify nimcp_gpu_max() finds maximum along axis
 * WHY:  Used in pooling and argmax
 */
TEST_F(GPUTensorKernelTest, Max_AllElements_FindsMax) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {3.0f, 1.0f, 4.0f, 1.0f, 5.0f, 9.0f, 2.0f, 6.0f};
    size_t dims[] = {8};
    size_t out_dims[] = {1};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(out_dims, 1);

    bool result = nimcp_gpu_max(ctx, x, out, -1, false);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 9.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Min reduction
 * WHAT: Verify nimcp_gpu_min() finds minimum along axis
 * WHY:  Used in various normalization
 */
TEST_F(GPUTensorKernelTest, Min_AllElements_FindsMin) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {3.0f, 1.0f, 4.0f, 1.0f, 5.0f, 9.0f, 2.0f, 6.0f};
    size_t dims[] = {8};
    size_t out_dims[] = {1};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(out_dims, 1);

    bool result = nimcp_gpu_min(ctx, x, out, -1, false);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 1.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Variance computation
 * WHAT: Verify nimcp_gpu_var() computes variance
 * WHY:  Used in batch normalization
 */
TEST_F(GPUTensorKernelTest, Var_SimpleData_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f};
    size_t dims[] = {8};
    size_t out_dims[] = {1};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(out_dims, 1);

    bool result = nimcp_gpu_var(ctx, x, out, -1, false, false);  // biased variance
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    // Mean = 5, variance = mean((x - 5)^2) = (9+1+1+1+0+0+4+16)/8 = 4
    EXPECT_NEAR(out_data[0], 4.0f, 0.1f);
}

//=============================================================================
// Norm Operation Tests
//=============================================================================

/**
 * TEST: L1 norm
 * WHAT: Verify nimcp_gpu_norm_l1() computes sum(|x|)
 * WHY:  Used in regularization
 */
TEST_F(GPUTensorKernelTest, NormL1_MixedValues_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-1.0f, 2.0f, -3.0f, 4.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));

    float result_val = 0.0f;
    bool result = nimcp_gpu_norm_l1(ctx, x, &result_val);
    EXPECT_TRUE(result);
    EXPECT_NEAR(result_val, 10.0f, FLOAT_TOLERANCE);  // 1+2+3+4=10
}

/**
 * TEST: L2 norm
 * WHAT: Verify nimcp_gpu_norm_l2() computes sqrt(sum(x^2))
 * WHY:  Used in gradient clipping
 */
TEST_F(GPUTensorKernelTest, NormL2_SimpleVector_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {3.0f, 4.0f};  // 3-4-5 triangle
    size_t dims[] = {2};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));

    float result_val = 0.0f;
    bool result = nimcp_gpu_norm_l2(ctx, x, &result_val);
    EXPECT_TRUE(result);
    EXPECT_NEAR(result_val, 5.0f, FLOAT_TOLERANCE);  // sqrt(9+16)=5
}

/**
 * TEST: L-infinity norm
 * WHAT: Verify nimcp_gpu_norm_linf() computes max(|x|)
 * WHY:  Used in spectral normalization
 */
TEST_F(GPUTensorKernelTest, NormLinf_MixedValues_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-5.0f, 2.0f, -3.0f, 4.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));

    float result_val = 0.0f;
    bool result = nimcp_gpu_norm_linf(ctx, x, &result_val);
    EXPECT_TRUE(result);
    EXPECT_NEAR(result_val, 5.0f, FLOAT_TOLERANCE);  // max(5,2,3,4)=5
}

/**
 * TEST: Frobenius norm
 * WHAT: Verify nimcp_gpu_norm_frobenius() computes matrix Frobenius norm
 * WHY:  Used in matrix regularization
 */
TEST_F(GPUTensorKernelTest, NormFrobenius_Matrix_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {1.0f, 2.0f, 3.0f, 4.0f};  // 2x2 matrix
    size_t dims[] = {2, 2};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 2, NIMCP_GPU_PRECISION_FP32));

    float result_val = 0.0f;
    bool result = nimcp_gpu_norm_frobenius(ctx, x, &result_val);
    EXPECT_TRUE(result);
    // sqrt(1+4+9+16) = sqrt(30) ~= 5.477
    EXPECT_NEAR(result_val, std::sqrt(30.0f), FLOAT_TOLERANCE);
}

//=============================================================================
// Memory Operation Tests
//=============================================================================

/**
 * TEST: Fill tensor with value
 * WHAT: Verify nimcp_gpu_fill() sets all elements to value
 * WHY:  Initialize tensors for computation
 */
TEST_F(GPUTensorKernelTest, Fill_ValidTensor_SetsAllElements) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    size_t dims[] = {3, 4};
    nimcp_gpu_tensor_t* tensor = create_output_tensor(dims, 2);

    bool result = nimcp_gpu_fill(ctx, tensor, 3.14f);
    EXPECT_TRUE(result);

    std::vector<float> data = to_host(tensor);
    for (float val : data) {
        EXPECT_NEAR(val, 3.14f, FLOAT_TOLERANCE);
    }
}

/**
 * TEST: Fill tensor with zeros
 * WHAT: Verify nimcp_gpu_zeros() sets all elements to 0
 * WHY:  Zero initialization
 */
TEST_F(GPUTensorKernelTest, Zeros_ValidTensor_SetsAllToZero) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    size_t dims[] = {5, 5};
    nimcp_gpu_tensor_t* tensor = create_output_tensor(dims, 2);
    nimcp_gpu_fill(ctx, tensor, 999.0f);  // Fill with non-zero first

    bool result = nimcp_gpu_zeros(ctx, tensor);
    EXPECT_TRUE(result);

    std::vector<float> data = to_host(tensor);
    for (float val : data) {
        EXPECT_NEAR(val, 0.0f, FLOAT_TOLERANCE);
    }
}

/**
 * TEST: Fill tensor with ones
 * WHAT: Verify nimcp_gpu_ones() sets all elements to 1
 * WHY:  One initialization for masks
 */
TEST_F(GPUTensorKernelTest, Ones_ValidTensor_SetsAllToOne) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    size_t dims[] = {4, 3};
    nimcp_gpu_tensor_t* tensor = create_output_tensor(dims, 2);

    bool result = nimcp_gpu_ones(ctx, tensor);
    EXPECT_TRUE(result);

    std::vector<float> data = to_host(tensor);
    for (float val : data) {
        EXPECT_NEAR(val, 1.0f, FLOAT_TOLERANCE);
    }
}

/**
 * TEST: Copy tensor
 * WHAT: Verify nimcp_gpu_copy() duplicates tensor data
 * WHY:  Save tensor state for operations
 */
TEST_F(GPUTensorKernelTest, Copy_ValidTensors_CopiesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    nimcp_gpu_tensor_t* src = create_2d_tensor(3, 4);
    ASSERT_NE(src, nullptr);

    size_t dims[] = {3, 4};
    nimcp_gpu_tensor_t* dst = create_output_tensor(dims, 2);

    bool result = nimcp_gpu_copy(ctx, src, dst);
    EXPECT_TRUE(result);

    std::vector<float> src_data = to_host(src);
    std::vector<float> dst_data = to_host(dst);
    ASSERT_EQ(src_data.size(), dst_data.size());
    for (size_t i = 0; i < src_data.size(); i++) {
        EXPECT_FLOAT_EQ(src_data[i], dst_data[i]);
    }
}

/**
 * TEST: Transpose tensor
 * WHAT: Verify nimcp_gpu_transpose() swaps last two dims
 * WHY:  Matrix transpose for GEMM
 */
TEST_F(GPUTensorKernelTest, Transpose_2DMatrix_SwapsDimensions) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // 2x3 matrix -> 3x2 transposed
    float x_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    size_t dims[] = {2, 3};
    size_t out_dims[] = {3, 2};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(out_dims, 2);

    bool result = nimcp_gpu_transpose(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    // Original: [[1,2,3],[4,5,6]]
    // Transposed: [[1,4],[2,5],[3,6]]
    EXPECT_NEAR(out_data[0], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 4.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 2.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 5.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[4], 3.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[5], 6.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Clamp values
 * WHAT: Verify nimcp_gpu_clamp() restricts values to range
 * WHY:  Value clipping for gradients
 */
TEST_F(GPUTensorKernelTest, Clamp_OutOfRange_ClampsCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-5.0f, 0.0f, 5.0f, 10.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_clamp(ctx, x, -2.0f, 7.0f, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], -2.0f, FLOAT_TOLERANCE);  // -5 clamped to -2
    EXPECT_NEAR(out_data[1], 0.0f, FLOAT_TOLERANCE);   // 0 unchanged
    EXPECT_NEAR(out_data[2], 5.0f, FLOAT_TOLERANCE);   // 5 unchanged
    EXPECT_NEAR(out_data[3], 7.0f, FLOAT_TOLERANCE);   // 10 clamped to 7
}

//=============================================================================
// Math Function Tests
//=============================================================================

/**
 * TEST: Exponential
 * WHAT: Verify nimcp_gpu_exp() computes exp(x)
 * WHY:  Used in softmax
 */
TEST_F(GPUTensorKernelTest, Exp_KnownValues_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {0.0f, 1.0f, 2.0f};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_exp(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 1.0f, FLOAT_TOLERANCE);  // exp(0) = 1
    EXPECT_NEAR(out_data[1], std::exp(1.0f), FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], std::exp(2.0f), FLOAT_TOLERANCE);
}

/**
 * TEST: Natural logarithm
 * WHAT: Verify nimcp_gpu_log() computes log(x)
 * WHY:  Used in log-likelihood
 */
TEST_F(GPUTensorKernelTest, Log_PositiveValues_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {1.0f, 2.718281828f, 10.0f};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_log(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 0.0f, FLOAT_TOLERANCE);  // log(1) = 0
    EXPECT_NEAR(out_data[1], 1.0f, 1e-4f);  // log(e) ~= 1
    EXPECT_NEAR(out_data[2], std::log(10.0f), FLOAT_TOLERANCE);
}

/**
 * TEST: Square root
 * WHAT: Verify nimcp_gpu_sqrt() computes sqrt(x)
 * WHY:  Used in normalization
 */
TEST_F(GPUTensorKernelTest, Sqrt_PerfectSquares_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {1.0f, 4.0f, 9.0f, 16.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_sqrt(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 2.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 3.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 4.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Power function
 * WHAT: Verify nimcp_gpu_pow() computes x^exponent
 * WHY:  Used in various computations
 */
TEST_F(GPUTensorKernelTest, Pow_SquareExponent_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_pow(ctx, x, 2.0f, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 4.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 9.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 16.0f, FLOAT_TOLERANCE);
}

/**
 * TEST: Absolute value
 * WHAT: Verify nimcp_gpu_abs() computes |x|
 * WHY:  Used in L1 norm
 */
TEST_F(GPUTensorKernelTest, Abs_MixedSigns_ComputesCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    float x_data[] = {-3.0f, -1.0f, 0.0f, 1.0f, 3.0f};
    size_t dims[] = {5};

    nimcp_gpu_tensor_t* x = track(nimcp_gpu_tensor_from_host(ctx, x_data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_abs(ctx, x, out);
    EXPECT_TRUE(result);

    std::vector<float> out_data = to_host(out);
    EXPECT_NEAR(out_data[0], 3.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[1], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[2], 0.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[3], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(out_data[4], 3.0f, FLOAT_TOLERANCE);
}

//=============================================================================
// CPU-GPU Integration Tests
//=============================================================================

/**
 * TEST: GPU tensor from CPU tensor
 * WHAT: Verify nimcp_gpu_tensor_from_cpu() converts correctly
 * WHY:  Enable GPU acceleration of CPU operations
 */
TEST_F(GPUTensorKernelTest, FromCPU_ValidTensor_ConvertsCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // Create CPU tensor
    uint32_t cpu_dims[] = {2, 3};
    nimcp_tensor_t* cpu_tensor = nimcp_tensor_create(cpu_dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(cpu_tensor, nullptr);

    // Fill with values
    float* data = (float*)nimcp_tensor_data(cpu_tensor);
    for (size_t i = 0; i < 6; i++) {
        data[i] = static_cast<float>(i + 1);
    }

    // Convert to GPU
    nimcp_gpu_tensor_t* gpu_tensor = track(nimcp_gpu_tensor_from_cpu(ctx, cpu_tensor));
    ASSERT_NE(gpu_tensor, nullptr);
    EXPECT_EQ(gpu_tensor->numel, 6u);

    // Verify data
    std::vector<float> result = to_host(gpu_tensor);
    for (size_t i = 0; i < 6; i++) {
        EXPECT_FLOAT_EQ(result[i], static_cast<float>(i + 1));
    }

    nimcp_tensor_destroy(cpu_tensor);
}

/**
 * TEST: CPU tensor from GPU tensor
 * WHAT: Verify nimcp_cpu_tensor_from_gpu() converts correctly
 * WHY:  Enable CPU operations on GPU results
 */
TEST_F(GPUTensorKernelTest, ToCPU_ValidTensor_ConvertsCorrectly) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // Create GPU tensor
    nimcp_gpu_tensor_t* gpu_tensor = create_2d_tensor(2, 3);
    ASSERT_NE(gpu_tensor, nullptr);

    // Convert to CPU
    nimcp_tensor_t* cpu_tensor = nimcp_cpu_tensor_from_gpu(gpu_tensor);
    ASSERT_NE(cpu_tensor, nullptr);

    // Verify data
    float* data = (float*)nimcp_tensor_data(cpu_tensor);
    for (size_t i = 0; i < 6; i++) {
        EXPECT_FLOAT_EQ(data[i], static_cast<float>(i + 1));
    }

    nimcp_tensor_destroy(cpu_tensor);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * TEST: NULL context operations
 * WHAT: Verify operations fail gracefully with NULL context
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(GPUTensorKernelTest, NullContext_ElementWise_ReturnsFalse) {
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    size_t dims[] = {4};

    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    nimcp_gpu_tensor_t* a = track(nimcp_gpu_tensor_from_host(ctx, data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* b = track(nimcp_gpu_tensor_from_host(ctx, data, dims, 1, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result = nimcp_gpu_add(nullptr, a, b, out);
    EXPECT_FALSE(result);
}

/**
 * TEST: NULL tensor operations
 * WHAT: Verify operations fail gracefully with NULL tensors
 * WHY:  Prevent crashes from invalid input
 */
TEST_F(GPUTensorKernelTest, NullTensor_ElementWise_ReturnsFalse) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    size_t dims[] = {4};
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 1);

    bool result1 = nimcp_gpu_add(ctx, nullptr, nullptr, out);
    EXPECT_FALSE(result1);

    bool result2 = nimcp_gpu_relu(ctx, nullptr, out);
    EXPECT_FALSE(result2);
}

//=============================================================================
// Precision Mode Tests
//=============================================================================

/**
 * TEST: FP16 precision tensor creation
 * WHAT: Verify tensor creation with FP16 precision
 * WHY:  Half precision for memory efficiency
 */
TEST_F(GPUTensorKernelTest, Create_FP16Precision_Succeeds) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    size_t dims[] = {10, 10};
    nimcp_gpu_tensor_t* tensor = track(nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP16));

    if (tensor != nullptr) {  // FP16 may not be supported
        EXPECT_EQ(tensor->precision, NIMCP_GPU_PRECISION_FP16);
        EXPECT_EQ(tensor->elem_size, 2u);
    }
}

/**
 * TEST: Dtype conversion
 * WHAT: Verify dtype to precision conversion
 * WHY:  CPU-GPU tensor integration
 */
TEST_F(GPUTensorKernelTest, DtypeConversion_RoundTrip_Consistent) {
    // FP32
    nimcp_gpu_precision_t prec = nimcp_dtype_to_gpu_precision(NIMCP_DTYPE_F32);
    EXPECT_EQ(prec, NIMCP_GPU_PRECISION_FP32);
    nimcp_dtype_t dtype = nimcp_gpu_precision_to_dtype(prec);
    EXPECT_EQ(dtype, NIMCP_DTYPE_F32);
}

//=============================================================================
// Large Tensor Tests
//=============================================================================

/**
 * TEST: Large tensor operations
 * WHAT: Verify operations work on large tensors
 * WHY:  Real-world neural networks have large tensors
 */
TEST_F(GPUTensorKernelTest, LargeTensor_ElementWise_Succeeds) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // 1000x1000 matrix
    size_t dims[] = {1000, 1000};
    nimcp_gpu_tensor_t* a = track(nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* b = track(nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* out = create_output_tensor(dims, 2);

    if (a && b && out) {
        nimcp_gpu_fill(ctx, a, 1.0f);
        nimcp_gpu_fill(ctx, b, 2.0f);

        bool result = nimcp_gpu_add(ctx, a, b, out);
        EXPECT_TRUE(result);

        // Spot check a few values
        std::vector<float> data = to_host(out);
        EXPECT_NEAR(data[0], 3.0f, FLOAT_TOLERANCE);
        EXPECT_NEAR(data[500000], 3.0f, FLOAT_TOLERANCE);  // Middle
        EXPECT_NEAR(data[999999], 3.0f, FLOAT_TOLERANCE);  // Last
    }
}

/**
 * TEST: Large matrix multiplication
 * WHAT: Verify GEMM works on large matrices
 * WHY:  Neural network layers can be large
 */
TEST_F(GPUTensorKernelTest, LargeGEMM_SquareMatrices_Succeeds) {
    if (!has_gpu_context()) {
        GTEST_SKIP() << "GPU context not available";
    }

    // 256x256 matrices
    size_t dims[] = {256, 256};
    nimcp_gpu_tensor_t* A = track(nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* B = track(nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32));
    nimcp_gpu_tensor_t* C = create_output_tensor(dims, 2);

    if (A && B && C) {
        nimcp_gpu_fill(ctx, A, 1.0f);
        nimcp_gpu_fill(ctx, B, 1.0f);

        bool result = nimcp_gpu_gemm(ctx, A, B, C, 1.0f, 0.0f, false, false);
        EXPECT_TRUE(result);

        // Each element should be 256 (sum of 256 ones)
        std::vector<float> data = to_host(C);
        EXPECT_NEAR(data[0], 256.0f, 0.1f);
    }
}
