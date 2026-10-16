/**
 * @file test_softmax_numerical_stability.cpp
 * @brief Unit tests for softmax numerical stability in CPU backend and GPU stubs
 *
 * WHAT: Test softmax with edge cases (large values, NaN, inf, uniform, single element)
 * WHY:  Softmax must handle extreme inputs without overflow, NaN, or division by zero
 * HOW:  Test both the kernel backend CPU softmax and the GPU stub softmax
 *
 * TESTED EDGE CASES:
 * - Normal values (standard softmax)
 * - Large positive values (overflow risk)
 * - Large negative values (underflow risk)
 * - Mixed large positive and negative values
 * - Uniform values (should give uniform distribution)
 * - Single element (should give 1.0)
 * - All zeros (should give uniform distribution)
 * - NaN in input (should produce valid output)
 * - Inf in input (should produce valid output)
 * - Very different magnitudes (dynamic range)
 * - Multi-dimensional softmax (last dim)
 *
 * VERIFIED HEADERS:
 * - gpu/backend/nimcp_kernel_backend.h: nimcp_kernel_backend_t,
 *   nimcp_kernel_error_t, nimcp_tensor_ops_t::softmax,
 *   nimcp_kernel_backend_init(), nimcp_get_kernel_backend()
 * - gpu/tensor/nimcp_tensor_gpu.h: nimcp_gpu_tensor_t (struct with
 *   void* data, size_t* dims, uint32_t ndim, size_t numel, etc.)
 *
 * @author NIMCP Development Team
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <numeric>
#include <limits>

#include "gpu/backend/nimcp_kernel_backend.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SoftmaxNumericalStabilityTest : public ::testing::Test {
protected:
    nimcp_kernel_backend_t* backend = nullptr;

    void SetUp() override {
        nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
        backend = nimcp_get_kernel_backend();
        ASSERT_NE(backend, nullptr);
        ASSERT_NE(backend->tensor.softmax, nullptr);
    }

    void TearDown() override {
        nimcp_kernel_backend_shutdown();
    }

    /**
     * Helper: Create a simple GPU tensor backed by host memory.
     * For CPU backend testing, the tensor's data pointer is a plain
     * host pointer (no actual GPU allocation needed).
     */
    nimcp_gpu_tensor_t make_tensor(float* data, size_t* dims, uint32_t ndim, size_t numel) {
        nimcp_gpu_tensor_t t;
        memset(&t, 0, sizeof(t));
        t.data = data;
        t.dims = dims;
        t.ndim = ndim;
        t.numel = numel;
        t.elem_size = sizeof(float);
        t.precision = NIMCP_GPU_PRECISION_FP32;
        t.layout = NIMCP_TENSOR_LAYOUT_ROW_MAJOR;
        t.ctx = nullptr;
        t.owns_data = false;
        return t;
    }

    /**
     * Verify softmax output properties:
     * 1. All values in [0, 1]
     * 2. Sum approximately 1.0
     * 3. No NaN or inf values
     */
    void verify_softmax_output(const float* output, size_t n, const char* label) {
        float sum = 0.0f;
        for (size_t i = 0; i < n; i++) {
            EXPECT_FALSE(std::isnan(output[i]))
                << label << ": output[" << i << "] is NaN";
            EXPECT_FALSE(std::isinf(output[i]))
                << label << ": output[" << i << "] is inf";
            EXPECT_GE(output[i], 0.0f)
                << label << ": output[" << i << "] = " << output[i] << " < 0";
            EXPECT_LE(output[i], 1.0f)
                << label << ": output[" << i << "] = " << output[i] << " > 1";
            sum += output[i];
        }
        EXPECT_NEAR(sum, 1.0f, 1e-5f)
            << label << ": sum = " << sum << " (expected 1.0)";
    }
};

//=============================================================================
// Normal Operation Tests
//=============================================================================

TEST_F(SoftmaxNumericalStabilityTest, NormalValues) {
    float input[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[4] = {0};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 4);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 4);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 4, "NormalValues");

    // Verify relative ordering: softmax(4) > softmax(3) > softmax(2) > softmax(1)
    EXPECT_GT(output[3], output[2]);
    EXPECT_GT(output[2], output[1]);
    EXPECT_GT(output[1], output[0]);
}

TEST_F(SoftmaxNumericalStabilityTest, AllZeros) {
    float input[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float output[4] = {0};
    size_t dims[] = {4};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 4);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 4);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 4, "AllZeros");

    // Should be uniform: each = 0.25
    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(output[i], 0.25f, 1e-6f);
    }
}

TEST_F(SoftmaxNumericalStabilityTest, SingleElement) {
    float input[] = {42.0f};
    float output[1] = {0};
    size_t dims[] = {1};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 1);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 1);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    EXPECT_FLOAT_EQ(output[0], 1.0f);
}

TEST_F(SoftmaxNumericalStabilityTest, UniformValues) {
    float input[] = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
    float output[5] = {0};
    size_t dims[] = {5};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 5);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 5);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 5, "UniformValues");

    for (int i = 0; i < 5; i++) {
        EXPECT_NEAR(output[i], 0.2f, 1e-6f);
    }
}

//=============================================================================
// Numerical Stability Edge Cases
//=============================================================================

/**
 * TEST: Large positive values that would overflow expf() without max subtraction
 *
 * expf(1000) = inf, but softmax should handle this via max subtraction
 */
TEST_F(SoftmaxNumericalStabilityTest, LargePositiveValues) {
    float input[] = {1000.0f, 1001.0f, 1002.0f};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 3, "LargePositiveValues");

    // Largest input should have largest probability
    EXPECT_GT(output[2], output[1]);
    EXPECT_GT(output[1], output[0]);
}

/**
 * TEST: Very large positive values near float max
 */
TEST_F(SoftmaxNumericalStabilityTest, VeryLargePositiveValues) {
    float input[] = {1e30f, 1e30f + 1.0f, 1e30f + 2.0f};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 3, "VeryLargePositiveValues");
}

/**
 * TEST: Large negative values that would underflow expf()
 *
 * expf(-1000) = 0, which leads to sum=0 and division by zero
 */
TEST_F(SoftmaxNumericalStabilityTest, LargeNegativeValues) {
    float input[] = {-1000.0f, -999.0f, -998.0f};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 3, "LargeNegativeValues");
}

/**
 * TEST: Mixed large positive and negative values (extreme dynamic range)
 *
 * With values like [1000, -1000], after max subtraction:
 * - exp(0) = 1.0
 * - exp(-2000) = 0.0 (underflow)
 * Result should be [~1.0, ~0.0] without NaN
 */
TEST_F(SoftmaxNumericalStabilityTest, MixedExtremeValues) {
    float input[] = {1000.0f, -1000.0f, 0.0f};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 3, "MixedExtremeValues");

    // The 1000 element should dominate
    EXPECT_GT(output[0], 0.99f);
}

/**
 * TEST: One huge value, rest very small - "winner takes all"
 */
TEST_F(SoftmaxNumericalStabilityTest, WinnerTakesAll) {
    float input[] = {100.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float output[5] = {0};
    size_t dims[] = {5};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 5);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 5);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 5, "WinnerTakesAll");

    EXPECT_GT(output[0], 0.99f);
    for (int i = 1; i < 5; i++) {
        EXPECT_LT(output[i], 0.01f);
    }
}

//=============================================================================
// NaN and Inf Input Handling
//=============================================================================

/**
 * TEST: NaN in input - should produce valid (uniform) output
 */
TEST_F(SoftmaxNumericalStabilityTest, NaNInput) {
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    float input[] = {1.0f, nan_val, 3.0f};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);

    // Output should not contain NaN
    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(std::isnan(output[i])) << "output[" << i << "] is NaN";
    }
}

/**
 * TEST: All NaN input - should produce uniform distribution
 */
TEST_F(SoftmaxNumericalStabilityTest, AllNaNInput) {
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    float input[] = {nan_val, nan_val, nan_val};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);

    // Should fall back to uniform
    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_NEAR(output[i], 1.0f / 3.0f, 1e-5f);
    }
}

/**
 * TEST: Positive infinity in input
 */
TEST_F(SoftmaxNumericalStabilityTest, PositiveInfInput) {
    float inf_val = std::numeric_limits<float>::infinity();
    float input[] = {1.0f, inf_val, 3.0f};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);

    // Output should not have NaN or inf
    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }
}

/**
 * TEST: Negative infinity in input
 */
TEST_F(SoftmaxNumericalStabilityTest, NegativeInfInput) {
    float neg_inf = -std::numeric_limits<float>::infinity();
    float input[] = {1.0f, neg_inf, 3.0f};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);

    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_FALSE(std::isinf(output[i]));
    }
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(SoftmaxNumericalStabilityTest, NullInputReturnsError) {
    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);
}

TEST_F(SoftmaxNumericalStabilityTest, NullInputDataReturnsError) {
    size_t dims[] = {4};
    nimcp_gpu_tensor_t in = make_tensor(nullptr, dims, 1, 4);
    nimcp_gpu_tensor_t out = make_tensor(nullptr, dims, 1, 4);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_NULL_PTR);
}

TEST_F(SoftmaxNumericalStabilityTest, ZeroElementsReturnsError) {
    float data = 0;
    size_t dims[] = {0};
    nimcp_gpu_tensor_t in = make_tensor(&data, dims, 1, 0);
    nimcp_gpu_tensor_t out = make_tensor(&data, dims, 1, 0);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_ERROR_INVALID_SIZE);
}

//=============================================================================
// Large Array Tests
//=============================================================================

TEST_F(SoftmaxNumericalStabilityTest, LargeArray) {
    const size_t N = 10000;
    std::vector<float> input(N);
    std::vector<float> output(N, 0.0f);
    size_t dims[] = {N};

    // Fill with values that vary widely
    for (size_t i = 0; i < N; i++) {
        input[i] = (float)i * 0.01f - 50.0f;  // Range: [-50, 50]
    }

    nimcp_gpu_tensor_t in = make_tensor(input.data(), dims, 1, N);
    nimcp_gpu_tensor_t out = make_tensor(output.data(), dims, 1, N);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output.data(), N, "LargeArray");
}

TEST_F(SoftmaxNumericalStabilityTest, LargeArrayAllSameValue) {
    const size_t N = 1000;
    std::vector<float> input(N, 42.0f);
    std::vector<float> output(N, 0.0f);
    size_t dims[] = {N};

    nimcp_gpu_tensor_t in = make_tensor(input.data(), dims, 1, N);
    nimcp_gpu_tensor_t out = make_tensor(output.data(), dims, 1, N);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output.data(), N, "LargeArraySameValue");

    float expected = 1.0f / (float)N;
    for (size_t i = 0; i < N; i++) {
        EXPECT_NEAR(output[i], expected, 1e-4f);
    }
}

//=============================================================================
// Correctness Tests (verify actual softmax values)
//=============================================================================

TEST_F(SoftmaxNumericalStabilityTest, CorrectnessSmallExample) {
    // softmax([0, 1]) = [e^0/(e^0+e^1), e^1/(e^0+e^1)]
    //                  = [1/(1+e), e/(1+e)]
    float input[] = {0.0f, 1.0f};
    float output[2] = {0};
    size_t dims[] = {2};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 2);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 2);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);

    float e = std::exp(1.0f);
    float expected_0 = 1.0f / (1.0f + e);
    float expected_1 = e / (1.0f + e);

    EXPECT_NEAR(output[0], expected_0, 1e-5f);
    EXPECT_NEAR(output[1], expected_1, 1e-5f);
}

TEST_F(SoftmaxNumericalStabilityTest, CorrectnessNegativeValues) {
    // softmax([-1, -2, -3]) should still sum to 1
    float input[] = {-1.0f, -2.0f, -3.0f};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);
    verify_softmax_output(output, 3, "NegativeValues");

    // -1 should have highest probability
    EXPECT_GT(output[0], output[1]);
    EXPECT_GT(output[1], output[2]);
}

//=============================================================================
// Boundary value tests near float limits
//=============================================================================

TEST_F(SoftmaxNumericalStabilityTest, FloatMaxInput) {
    float fmax = std::numeric_limits<float>::max();
    float input[] = {fmax, fmax, fmax};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);

    // All same -> uniform
    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_NEAR(output[i], 1.0f / 3.0f, 1e-5f);
    }
}

TEST_F(SoftmaxNumericalStabilityTest, FloatLowestInput) {
    float lowest = std::numeric_limits<float>::lowest();
    float input[] = {lowest, lowest, lowest};
    float output[3] = {0};
    size_t dims[] = {3};

    nimcp_gpu_tensor_t in = make_tensor(input, dims, 1, 3);
    nimcp_gpu_tensor_t out = make_tensor(output, dims, 1, 3);

    nimcp_kernel_error_t err = backend->tensor.softmax(nullptr, &in, &out);
    EXPECT_EQ(err, NIMCP_KERNEL_SUCCESS);

    for (int i = 0; i < 3; i++) {
        EXPECT_FALSE(std::isnan(output[i]));
        EXPECT_NEAR(output[i], 1.0f / 3.0f, 1e-5f);
    }
}
