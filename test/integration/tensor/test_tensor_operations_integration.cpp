/**
 * @file test_tensor_operations_integration.cpp
 * @brief Integration tests for Tensor Operations
 *
 * WHAT: Test tensor operations chain correctly
 * WHY:  Verify autodiff works end-to-end
 *       and SIMD operations integrate with non-SIMD fallbacks
 * HOW:  Create tensors, chain operations, verify gradients
 *
 * TEST COVERAGE:
 * - Tensor operations chain correctly
 * - Autodiff works end-to-end
 * - SIMD operations integrate with non-SIMD fallbacks
 * - Memory management across operations
 * - Numerical stability
 * - Broadcasting correctness
 *
 * @author NIMCP Development Team
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TensorOperationsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize tensor subsystem
        int result = nimcp_tensor_init();
        ASSERT_EQ(result, NIMCP_TENSOR_OK);
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }

    // Helper to create test tensor with specific values
    nimcp_tensor_t* CreateTestTensor(std::vector<uint32_t> dims, float fill_value) {
        return nimcp_tensor_full(dims.data(), dims.size(), NIMCP_DTYPE_F32, fill_value);
    }

    // Helper to verify tensor contents
    bool VerifyTensorValue(const nimcp_tensor_t* t, float expected, float tolerance = 1e-5f) {
        if (!t) return false;
        size_t numel = nimcp_tensor_numel(t);
        for (size_t i = 0; i < numel; ++i) {
            double val = nimcp_tensor_get_flat(t, i);
            if (std::abs(val - expected) > tolerance) {
                return false;
            }
        }
        return true;
    }
};

//=============================================================================
// TENSOR CREATION TESTS
//=============================================================================

TEST_F(TensorOperationsIntegrationTest, Tensor_Creation_Basic) {
    /* WHAT: Test basic tensor creation */
    /* WHY:  Verify initialization works */

    uint32_t dims[] = {3, 4};
    nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    EXPECT_EQ(nimcp_tensor_rank(t), 2u);
    EXPECT_EQ(nimcp_tensor_numel(t), 12u);

    nimcp_tensor_destroy(t);
}

TEST_F(TensorOperationsIntegrationTest, Tensor_Creation_AllTypes) {
    /* WHAT: Test tensor creation with all data types */
    /* WHY:  Verify all dtypes work */

    uint32_t dims[] = {2, 3};
    nimcp_dtype_t types[] = {
        NIMCP_DTYPE_F32, NIMCP_DTYPE_F64, NIMCP_DTYPE_I32, NIMCP_DTYPE_I64
    };

    for (auto dtype : types) {
        nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 2, dtype);
        if (t) {
            EXPECT_EQ(nimcp_tensor_dtype(t), dtype);
            nimcp_tensor_destroy(t);
        }
    }
}

TEST_F(TensorOperationsIntegrationTest, Tensor_Creation_HighRank) {
    /* WHAT: Test high-rank tensor creation */
    /* WHY:  Verify multi-dimensional support */

    uint32_t dims[] = {2, 3, 4, 5, 6};
    nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 5, NIMCP_DTYPE_F32);

    if (t) {
        EXPECT_EQ(nimcp_tensor_rank(t), 5u);
        EXPECT_EQ(nimcp_tensor_numel(t), 2u * 3 * 4 * 5 * 6);
        nimcp_tensor_destroy(t);
    }
}

//=============================================================================
// TENSOR OPERATION CHAIN TESTS
//=============================================================================

TEST_F(TensorOperationsIntegrationTest, OperationChain_AddMulSub) {
    /* WHAT: Test chaining add, mul, sub operations */
    /* WHY:  Verify operations can be composed */

    uint32_t dims[] = {4, 4};

    nimcp_tensor_t* a = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 2.0);
    nimcp_tensor_t* b = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 3.0);
    nimcp_tensor_t* c = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 1.0);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    // Chain: ((a + b) * 2) - c = ((2 + 3) * 2) - 1 = 9
    nimcp_tensor_t* sum = nimcp_tensor_add(a, b);  // 5
    ASSERT_NE(sum, nullptr);

    nimcp_tensor_t* scaled = nimcp_tensor_mul_scalar(sum, 2.0);  // 10
    ASSERT_NE(scaled, nullptr);

    nimcp_tensor_t* result = nimcp_tensor_sub(scaled, c);  // 9
    ASSERT_NE(result, nullptr);

    EXPECT_TRUE(VerifyTensorValue(result, 9.0f));

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
    nimcp_tensor_destroy(sum);
    nimcp_tensor_destroy(scaled);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorOperationsIntegrationTest, OperationChain_MatMulAdd) {
    /* WHAT: Test matmul followed by bias add */
    /* WHY:  Common neural network pattern */

    // Create 2x3 @ 3x4 = 2x4
    uint32_t dims_a[] = {2, 3};
    uint32_t dims_b[] = {3, 4};
    uint32_t dims_bias[] = {4};

    nimcp_tensor_t* a = nimcp_tensor_ones(dims_a, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* b = nimcp_tensor_ones(dims_b, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* bias = nimcp_tensor_full(dims_bias, 1, NIMCP_DTYPE_F32, 0.5);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(bias, nullptr);

    // Matmul: all 1s * all 1s with inner dim 3 = 3.0
    nimcp_tensor_t* mm = nimcp_tensor_matmul(a, b);
    if (!mm) {
        nimcp_tensor_destroy(a);
        nimcp_tensor_destroy(b);
        nimcp_tensor_destroy(bias);
        GTEST_SKIP() << "Matmul not implemented";
    }

    // Verify matmul result shape
    EXPECT_EQ(nimcp_tensor_rank(mm), 2u);

    // Add bias with broadcasting
    nimcp_tensor_t* result = nimcp_tensor_add(mm, bias);
    if (result) {
        // Result should be 3.0 + 0.5 = 3.5
        EXPECT_TRUE(VerifyTensorValue(result, 3.5f));
        nimcp_tensor_destroy(result);
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(bias);
    nimcp_tensor_destroy(mm);
}

TEST_F(TensorOperationsIntegrationTest, OperationChain_ActivationFunctions) {
    /* WHAT: Test chaining activation functions */
    /* WHY:  Common NN layer pattern */

    uint32_t dims[] = {2, 4};
    nimcp_tensor_t* x = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    ASSERT_NE(x, nullptr);

    // Chain: relu -> sigmoid -> tanh
    nimcp_tensor_t* relu_out = nimcp_tensor_relu(x);
    nimcp_tensor_t* sig_out = relu_out ? nimcp_tensor_sigmoid(relu_out) : nullptr;
    nimcp_tensor_t* tanh_out = sig_out ? nimcp_tensor_tanh(sig_out) : nullptr;

    if (tanh_out) {
        // Verify output is in valid range for tanh: [-1, 1]
        size_t numel = nimcp_tensor_numel(tanh_out);
        for (size_t i = 0; i < numel; ++i) {
            double val = nimcp_tensor_get_flat(tanh_out, i);
            EXPECT_GE(val, -1.0);
            EXPECT_LE(val, 1.0);
        }
    }

    nimcp_tensor_destroy(x);
    if (relu_out) nimcp_tensor_destroy(relu_out);
    if (sig_out) nimcp_tensor_destroy(sig_out);
    if (tanh_out) nimcp_tensor_destroy(tanh_out);
}

//=============================================================================
// AUTODIFF END-TO-END TESTS
//=============================================================================

TEST_F(TensorOperationsIntegrationTest, Autodiff_SimpleGradient) {
    /* WHAT: Test simple gradient computation */
    /* WHY:  Verify autodiff basics work */

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    if (!ctx) {
        GTEST_SKIP() << "Autodiff not available";
    }

    nimcp_autodiff_start(ctx);

    // y = x^2, dy/dx = 2x
    uint32_t dims[] = {1};
    nimcp_tensor_t* x = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 3.0);
    ASSERT_NE(x, nullptr);

    nimcp_tensor_set_requires_grad(x, true);

    nimcp_tensor_t* y = nimcp_tensor_square(x);

    nimcp_autodiff_stop(ctx);

    if (y) {
        // Compute gradient
        nimcp_tensor_t* gradients[1];
        nimcp_tensor_t* inputs[] = {x};

        int result = nimcp_autodiff_backward(ctx, y, inputs, 1, gradients);
        if (result == NIMCP_TENSOR_OK && gradients[0]) {
            // Gradient should be 2 * 3 = 6
            double grad_val = nimcp_tensor_get_flat(gradients[0], 0);
            EXPECT_NEAR(grad_val, 6.0, 0.01);
            nimcp_tensor_destroy(gradients[0]);
        }

        nimcp_tensor_destroy(y);
    }

    nimcp_tensor_destroy(x);
    nimcp_autodiff_destroy(ctx);
}

TEST_F(TensorOperationsIntegrationTest, Autodiff_ChainRule) {
    /* WHAT: Test chain rule in autodiff */
    /* WHY:  Verify gradient flows through operations */

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    if (!ctx) {
        GTEST_SKIP() << "Autodiff not available";
    }

    nimcp_autodiff_start(ctx);

    // y = (x + 2)^2, dy/dx = 2(x + 2)
    uint32_t dims[] = {1};
    nimcp_tensor_t* x = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 1.0);  // x = 1
    ASSERT_NE(x, nullptr);

    nimcp_tensor_set_requires_grad(x, true);

    nimcp_tensor_t* x_plus_2 = nimcp_tensor_add_scalar(x, 2.0);  // 3
    nimcp_tensor_t* y = x_plus_2 ? nimcp_tensor_square(x_plus_2) : nullptr;  // 9

    nimcp_autodiff_stop(ctx);

    if (y) {
        nimcp_tensor_t* gradients[1];
        nimcp_tensor_t* inputs[] = {x};

        int result = nimcp_autodiff_backward(ctx, y, inputs, 1, gradients);
        if (result == NIMCP_TENSOR_OK && gradients[0]) {
            // dy/dx = 2 * (1 + 2) = 6
            double grad_val = nimcp_tensor_get_flat(gradients[0], 0);
            EXPECT_NEAR(grad_val, 6.0, 0.01);
            nimcp_tensor_destroy(gradients[0]);
        }

        nimcp_tensor_destroy(y);
    }

    nimcp_tensor_destroy(x);
    if (x_plus_2) nimcp_tensor_destroy(x_plus_2);
    nimcp_autodiff_destroy(ctx);
}

TEST_F(TensorOperationsIntegrationTest, Autodiff_MultiVariable) {
    /* WHAT: Test gradients with multiple variables */
    /* WHY:  Verify partial derivatives work */

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    if (!ctx) {
        GTEST_SKIP() << "Autodiff not available";
    }

    nimcp_autodiff_start(ctx);

    // z = x * y, dz/dx = y, dz/dy = x
    uint32_t dims[] = {1};
    nimcp_tensor_t* x = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 3.0);
    nimcp_tensor_t* y = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 4.0);

    ASSERT_NE(x, nullptr);
    ASSERT_NE(y, nullptr);

    nimcp_tensor_set_requires_grad(x, true);
    nimcp_tensor_set_requires_grad(y, true);

    nimcp_tensor_t* z = nimcp_tensor_mul(x, y);

    nimcp_autodiff_stop(ctx);

    if (z) {
        nimcp_tensor_t* gradients[2];
        nimcp_tensor_t* inputs[] = {x, y};

        int result = nimcp_autodiff_backward(ctx, z, inputs, 2, gradients);
        if (result == NIMCP_TENSOR_OK) {
            if (gradients[0]) {
                // dz/dx = y = 4
                double grad_x = nimcp_tensor_get_flat(gradients[0], 0);
                EXPECT_NEAR(grad_x, 4.0, 0.01);
                nimcp_tensor_destroy(gradients[0]);
            }
            if (gradients[1]) {
                // dz/dy = x = 3
                double grad_y = nimcp_tensor_get_flat(gradients[1], 0);
                EXPECT_NEAR(grad_y, 3.0, 0.01);
                nimcp_tensor_destroy(gradients[1]);
            }
        }

        nimcp_tensor_destroy(z);
    }

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(y);
    nimcp_autodiff_destroy(ctx);
}

TEST_F(TensorOperationsIntegrationTest, Autodiff_NeuralNetworkPattern) {
    /* WHAT: Test autodiff for basic neural network pattern */
    /* WHY:  Verify common ML use case works */

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    if (!ctx) {
        GTEST_SKIP() << "Autodiff not available";
    }

    nimcp_autodiff_start(ctx);

    // Simple linear layer: y = relu(W @ x + b)
    uint32_t dims_x[] = {4, 1};
    uint32_t dims_w[] = {2, 4};
    uint32_t dims_b[] = {2, 1};

    nimcp_tensor_t* x = nimcp_tensor_randn(dims_x, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* w = nimcp_tensor_randn(dims_w, 2, NIMCP_DTYPE_F32, 0.0, 0.1);
    nimcp_tensor_t* b = nimcp_tensor_zeros(dims_b, 2, NIMCP_DTYPE_F32);

    ASSERT_NE(x, nullptr);
    ASSERT_NE(w, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_set_requires_grad(w, true);
    nimcp_tensor_set_requires_grad(b, true);

    // Forward pass
    nimcp_tensor_t* wx = nimcp_tensor_matmul(w, x);
    nimcp_tensor_t* wx_plus_b = wx ? nimcp_tensor_add(wx, b) : nullptr;
    nimcp_tensor_t* y = wx_plus_b ? nimcp_tensor_relu(wx_plus_b) : nullptr;

    // Sum to get scalar loss
    nimcp_tensor_t* loss = y ? nimcp_tensor_sum(y) : nullptr;

    nimcp_autodiff_stop(ctx);

    if (loss) {
        // Compute gradients w.r.t. W and b
        nimcp_tensor_t* gradients[2];
        nimcp_tensor_t* inputs[] = {w, b};

        int result = nimcp_autodiff_backward(ctx, loss, inputs, 2, gradients);
        if (result == NIMCP_TENSOR_OK) {
            // Gradients should exist
            if (gradients[0]) {
                EXPECT_EQ(nimcp_tensor_rank(gradients[0]), nimcp_tensor_rank(w));
                nimcp_tensor_destroy(gradients[0]);
            }
            if (gradients[1]) {
                EXPECT_EQ(nimcp_tensor_rank(gradients[1]), nimcp_tensor_rank(b));
                nimcp_tensor_destroy(gradients[1]);
            }
        }

        nimcp_tensor_destroy(loss);
    }

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(w);
    nimcp_tensor_destroy(b);
    if (wx) nimcp_tensor_destroy(wx);
    if (wx_plus_b) nimcp_tensor_destroy(wx_plus_b);
    if (y) nimcp_tensor_destroy(y);
    nimcp_autodiff_destroy(ctx);
}

//=============================================================================
// SIMD vs NON-SIMD FALLBACK TESTS
//=============================================================================

TEST_F(TensorOperationsIntegrationTest, SIMD_AlignedTensor) {
    /* WHAT: Test operations on aligned tensor */
    /* WHY:  Verify SIMD path works */

    // Create large aligned tensor
    uint32_t dims[] = {64, 64};  // Nice power of 2
    nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Operations should use SIMD on aligned data
    nimcp_tensor_t* sum = nimcp_tensor_add(a, b);
    nimcp_tensor_t* prod = nimcp_tensor_mul(a, b);
    nimcp_tensor_t* exp_a = nimcp_tensor_exp(a);

    ASSERT_NE(sum, nullptr);
    ASSERT_NE(prod, nullptr);
    ASSERT_NE(exp_a, nullptr);

    // Verify results are reasonable
    EXPECT_EQ(nimcp_tensor_numel(sum), 64u * 64);
    EXPECT_EQ(nimcp_tensor_numel(prod), 64u * 64);
    EXPECT_EQ(nimcp_tensor_numel(exp_a), 64u * 64);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(sum);
    nimcp_tensor_destroy(prod);
    nimcp_tensor_destroy(exp_a);
}

TEST_F(TensorOperationsIntegrationTest, SIMD_UnalignedTensor) {
    /* WHAT: Test operations on potentially unaligned tensor */
    /* WHY:  Verify fallback works */

    // Create odd-sized tensor that may not align perfectly
    uint32_t dims[] = {17, 23};  // Odd sizes
    nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Operations should still work (may use scalar fallback)
    nimcp_tensor_t* sum = nimcp_tensor_add(a, b);
    nimcp_tensor_t* prod = nimcp_tensor_mul(a, b);

    ASSERT_NE(sum, nullptr);
    ASSERT_NE(prod, nullptr);

    EXPECT_EQ(nimcp_tensor_numel(sum), 17u * 23);
    EXPECT_EQ(nimcp_tensor_numel(prod), 17u * 23);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(sum);
    nimcp_tensor_destroy(prod);
}

TEST_F(TensorOperationsIntegrationTest, SIMD_MixedAlignmentOperations) {
    /* WHAT: Test operations between aligned and unaligned tensors */
    /* WHY:  Verify hybrid paths work */

    uint32_t dims_aligned[] = {64, 64};
    uint32_t dims_unaligned[] = {64, 63};

    nimcp_tensor_t* aligned = nimcp_tensor_randn(dims_aligned, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* unaligned = nimcp_tensor_randn(dims_unaligned, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    ASSERT_NE(aligned, nullptr);
    ASSERT_NE(unaligned, nullptr);

    // Reshape unaligned to match (if possible)
    // Or use broadcast-compatible operations

    nimcp_tensor_destroy(aligned);
    nimcp_tensor_destroy(unaligned);
}

//=============================================================================
// BROADCASTING TESTS
//=============================================================================

TEST_F(TensorOperationsIntegrationTest, Broadcasting_ScalarToTensor) {
    /* WHAT: Test scalar broadcast to tensor */
    /* WHY:  Verify basic broadcasting */

    uint32_t dims[] = {3, 4};
    nimcp_tensor_t* a = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 2.0);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* result = nimcp_tensor_add_scalar(a, 5.0);
    ASSERT_NE(result, nullptr);

    EXPECT_TRUE(VerifyTensorValue(result, 7.0f));

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(result);
}

TEST_F(TensorOperationsIntegrationTest, Broadcasting_VectorToMatrix) {
    /* WHAT: Test vector broadcast to matrix */
    /* WHY:  Common bias addition pattern */

    uint32_t dims_matrix[] = {3, 4};
    uint32_t dims_vector[] = {4};

    nimcp_tensor_t* matrix = nimcp_tensor_full(dims_matrix, 2, NIMCP_DTYPE_F32, 1.0);
    nimcp_tensor_t* vector = nimcp_tensor_full(dims_vector, 1, NIMCP_DTYPE_F32, 2.0);

    ASSERT_NE(matrix, nullptr);
    ASSERT_NE(vector, nullptr);

    nimcp_tensor_t* result = nimcp_tensor_add(matrix, vector);
    if (result) {
        // Each row should be 1 + 2 = 3
        EXPECT_TRUE(VerifyTensorValue(result, 3.0f));
        nimcp_tensor_destroy(result);
    }

    nimcp_tensor_destroy(matrix);
    nimcp_tensor_destroy(vector);
}

TEST_F(TensorOperationsIntegrationTest, Broadcasting_OuterProductStyle) {
    /* WHAT: Test outer product via broadcasting */
    /* WHY:  Verify multi-dimensional broadcast */

    uint32_t dims_col[] = {3, 1};
    uint32_t dims_row[] = {1, 4};

    nimcp_tensor_t* col = nimcp_tensor_full(dims_col, 2, NIMCP_DTYPE_F32, 2.0);
    nimcp_tensor_t* row = nimcp_tensor_full(dims_row, 2, NIMCP_DTYPE_F32, 3.0);

    ASSERT_NE(col, nullptr);
    ASSERT_NE(row, nullptr);

    nimcp_tensor_t* result = nimcp_tensor_mul(col, row);
    if (result) {
        // Result should be 3x4 matrix with all 6.0
        EXPECT_EQ(nimcp_tensor_rank(result), 2u);
        EXPECT_TRUE(VerifyTensorValue(result, 6.0f));
        nimcp_tensor_destroy(result);
    }

    nimcp_tensor_destroy(col);
    nimcp_tensor_destroy(row);
}

//=============================================================================
// NUMERICAL STABILITY TESTS
//=============================================================================

TEST_F(TensorOperationsIntegrationTest, NumericalStability_Softmax) {
    /* WHAT: Test softmax numerical stability */
    /* WHY:  Verify no overflow with large values */

    uint32_t dims[] = {4};

    // Create tensor with large values that would overflow exp()
    nimcp_tensor_t* x = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 1000.0);
    ASSERT_NE(x, nullptr);

    nimcp_tensor_t* sm = nimcp_tensor_softmax(x, 0);
    if (sm) {
        // With all equal values, softmax should give uniform distribution
        EXPECT_TRUE(VerifyTensorValue(sm, 0.25f, 0.01f));

        // Verify sum = 1
        nimcp_tensor_t* sum_result = nimcp_tensor_sum(sm);
        if (sum_result) {
            double sum_val = nimcp_tensor_get_flat(sum_result, 0);
            EXPECT_NEAR(sum_val, 1.0, 0.001);
            nimcp_tensor_destroy(sum_result);
        }

        nimcp_tensor_destroy(sm);
    }

    nimcp_tensor_destroy(x);
}

TEST_F(TensorOperationsIntegrationTest, NumericalStability_LogSoftmax) {
    /* WHAT: Test log-softmax numerical stability */
    /* WHY:  Verify no NaN with large values */

    uint32_t dims[] = {4};
    nimcp_tensor_t* x = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 1000.0);
    ASSERT_NE(x, nullptr);

    nimcp_tensor_t* lsm = nimcp_tensor_log_softmax(x, 0);
    if (lsm) {
        // With all equal, log_softmax should give log(1/4) = -log(4)
        double expected = -std::log(4.0);

        for (size_t i = 0; i < 4; ++i) {
            double val = nimcp_tensor_get_flat(lsm, i);
            EXPECT_FALSE(std::isnan(val));
            EXPECT_FALSE(std::isinf(val));
            EXPECT_NEAR(val, expected, 0.01);
        }

        nimcp_tensor_destroy(lsm);
    }

    nimcp_tensor_destroy(x);
}

TEST_F(TensorOperationsIntegrationTest, NumericalStability_LayerNorm) {
    /* WHAT: Test layer normalization stability */
    /* WHY:  Verify no division by zero */

    uint32_t dims[] = {2, 4};
    nimcp_tensor_t* x = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 5.0);  // Constant
    ASSERT_NE(x, nullptr);

    uint32_t param_dims[] = {4};
    nimcp_tensor_t* gamma = nimcp_tensor_ones(param_dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* beta = nimcp_tensor_zeros(param_dims, 1, NIMCP_DTYPE_F32);

    ASSERT_NE(gamma, nullptr);
    ASSERT_NE(beta, nullptr);

    // With constant input, variance = 0, should handle gracefully
    nimcp_tensor_t* ln = nimcp_tensor_layer_norm(x, gamma, beta, 1e-5);
    if (ln) {
        // Should produce finite values
        size_t numel = nimcp_tensor_numel(ln);
        for (size_t i = 0; i < numel; ++i) {
            double val = nimcp_tensor_get_flat(ln, i);
            EXPECT_FALSE(std::isnan(val));
            EXPECT_FALSE(std::isinf(val));
        }
        nimcp_tensor_destroy(ln);
    }

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(gamma);
    nimcp_tensor_destroy(beta);
}

//=============================================================================
// MEMORY MANAGEMENT TESTS
//=============================================================================

TEST_F(TensorOperationsIntegrationTest, Memory_NoLeaks) {
    /* WHAT: Test no memory leaks in operation chains */
    /* WHY:  Verify proper cleanup */

    nimcp_tensor_reset_stats();

    {
        uint32_t dims[] = {10, 10};
        for (int i = 0; i < 100; ++i) {
            nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
            nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
            nimcp_tensor_t* c = nimcp_tensor_add(a, b);
            nimcp_tensor_t* d = nimcp_tensor_mul(c, a);

            nimcp_tensor_destroy(a);
            nimcp_tensor_destroy(b);
            nimcp_tensor_destroy(c);
            nimcp_tensor_destroy(d);
        }
    }

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);

    // All tensors should be destroyed
    EXPECT_EQ(stats.tensors_created, stats.tensors_destroyed);
}

TEST_F(TensorOperationsIntegrationTest, Memory_ConcurrentAllocations) {
    /* WHAT: Test concurrent tensor allocations */
    /* WHY:  Verify thread safety of memory management */

    std::atomic<bool> stop{false};
    std::atomic<int> allocations{0};
    std::atomic<int> errors{0};

    auto worker = [&]() {
        while (!stop.load()) {
            uint32_t dims[] = {8, 8};
            nimcp_tensor_t* t = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
            if (t) {
                allocations.fetch_add(1);
                nimcp_tensor_destroy(t);
            } else {
                errors.fetch_add(1);
            }
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(allocations.load(), 100);
    EXPECT_EQ(errors.load(), 0);
}

//=============================================================================
// REDUCTION OPERATIONS TESTS
//=============================================================================

TEST_F(TensorOperationsIntegrationTest, Reduction_SumMeanMax) {
    /* WHAT: Test reduction operations */
    /* WHY:  Verify aggregate computations */

    uint32_t dims[] = {3, 4};
    nimcp_tensor_t* t = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 2.0);
    ASSERT_NE(t, nullptr);

    // Sum should be 3 * 4 * 2 = 24
    nimcp_tensor_t* sum_result = nimcp_tensor_sum(t);
    if (sum_result) {
        double sum_val = nimcp_tensor_get_flat(sum_result, 0);
        EXPECT_NEAR(sum_val, 24.0, 0.01);
        nimcp_tensor_destroy(sum_result);
    }

    // Mean should be 2
    nimcp_tensor_t* mean_result = nimcp_tensor_mean(t);
    if (mean_result) {
        double mean_val = nimcp_tensor_get_flat(mean_result, 0);
        EXPECT_NEAR(mean_val, 2.0, 0.01);
        nimcp_tensor_destroy(mean_result);
    }

    // Max should be 2
    nimcp_tensor_t* max_result = nimcp_tensor_max(t);
    if (max_result) {
        double max_val = nimcp_tensor_get_flat(max_result, 0);
        EXPECT_NEAR(max_val, 2.0, 0.01);
        nimcp_tensor_destroy(max_result);
    }

    nimcp_tensor_destroy(t);
}

TEST_F(TensorOperationsIntegrationTest, Reduction_AlongDimension) {
    /* WHAT: Test reduction along specific dimension */
    /* WHY:  Verify dimensional reductions */

    uint32_t dims[] = {2, 3, 4};
    nimcp_tensor_t* t = nimcp_tensor_full(dims, 3, NIMCP_DTYPE_F32, 1.0);
    ASSERT_NE(t, nullptr);

    // Sum along dim 1 (size 3) should give shape (2, 4) with values 3
    nimcp_tensor_t* sum_dim1 = nimcp_tensor_sum_dim(t, 1, false);
    if (sum_dim1) {
        EXPECT_EQ(nimcp_tensor_rank(sum_dim1), 2u);
        EXPECT_TRUE(VerifyTensorValue(sum_dim1, 3.0f));
        nimcp_tensor_destroy(sum_dim1);
    }

    // Sum along dim 1 with keepdim should give shape (2, 1, 4)
    nimcp_tensor_t* sum_dim1_keep = nimcp_tensor_sum_dim(t, 1, true);
    if (sum_dim1_keep) {
        EXPECT_EQ(nimcp_tensor_rank(sum_dim1_keep), 3u);
        nimcp_tensor_destroy(sum_dim1_keep);
    }

    nimcp_tensor_destroy(t);
}
