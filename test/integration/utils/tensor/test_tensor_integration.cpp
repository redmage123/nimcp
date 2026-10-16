//=============================================================================
// test_tensor_integration.cpp - Integration Tests for Einsum + Autodiff
//=============================================================================
/**
 * @file test_tensor_integration.cpp
 * @brief Integration tests for einsum and autodiff working together
 *
 * WHAT: Test that einsum forward computation integrates correctly with
 *       autodiff backward pass and numerical gradient checking
 * WHY:  Einsum + autodiff is the core compute-then-differentiate pipeline;
 *       verifying their integration prevents silent gradient corruption
 * HOW:  Compute forward results via einsum, run autodiff backward, verify
 *       gradient shapes and values using numerical gradient cross-checks
 *
 * TEST COVERAGE:
 * 1. Einsum forward + autodiff backward (pipeline smoke test)
 * 2. Gradient shape matches input shape for all einsum patterns
 * 3. Numerical gradient of einsum-based scalar function
 * 4. Einsum matmul gradient via numerical differentiation
 * 5. Einsum trace gradient via numerical differentiation
 * 6. Chained einsum operations with backward
 * 7. Gradient accumulation across multiple backward passes
 * 8. Multiple inputs with autodiff backward
 * 9. requires_grad integration with einsum pipeline
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TensorEinsumAutodiffIntegration : public ::testing::Test {
protected:
    static constexpr double EPSILON = 1e-4;
    // F32 numerical gradients use h=1e-3 (adaptive step), giving ~0.1-0.25 error
    static constexpr double GRAD_EPSILON = 0.3;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_tensor_init();
        nimcp_tensor_reset_stats();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }

    /**
     * @brief Helper: Create a 2x2 F32 matrix with given values
     */
    nimcp_tensor_t* Create2x2(double a, double b, double c, double d) {
        uint32_t dims[] = {2, 2};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        if (!t) return nullptr;
        nimcp_tensor_set_flat(t, 0, a);
        nimcp_tensor_set_flat(t, 1, b);
        nimcp_tensor_set_flat(t, 2, c);
        nimcp_tensor_set_flat(t, 3, d);
        return t;
    }

    /**
     * @brief Helper: Create a 1D F32 vector from array
     */
    nimcp_tensor_t* CreateVec(const float* data, uint32_t size) {
        uint32_t dims[] = {size};
        return nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);
    }
};

//=============================================================================
// Integration Tests: Einsum Forward + Autodiff Backward Pipeline
//=============================================================================

TEST_F(TensorEinsumAutodiffIntegration, EinsumMatmul_ThenBackward_GradientShapes) {
    // WHAT: Run einsum matmul forward, then autodiff backward
    // WHY:  Verify the pipeline does not crash and produces correctly-shaped gradients
    //
    // Forward: C = einsum("ij,jk->ik", A, B) where A is 2x3, B is 3x2
    // Backward: dL/dA should be 2x3, dL/dB should be 3x2

    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t a_dims[] = {2, 3};
    nimcp_tensor_t* a = nimcp_tensor_from_data(a_data, a_dims, 2, NIMCP_DTYPE_F32, true);

    float b_data[] = {7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f};
    uint32_t b_dims[] = {3, 2};
    nimcp_tensor_t* b = nimcp_tensor_from_data(b_data, b_dims, 2, NIMCP_DTYPE_F32, true);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Forward pass via einsum
    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* c = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(c, nullptr);

    // Verify forward result shape
    const nimcp_tensor_shape_t* c_shape = nimcp_tensor_shape(c);
    EXPECT_EQ(c_shape->dims[0], 2u);
    EXPECT_EQ(c_shape->dims[1], 2u);

    // Backward pass - use c as output, a and b as inputs
    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {a, b};
    nimcp_tensor_t* gradients[2] = {nullptr, nullptr};

    int rc = nimcp_autodiff_backward(ctx, c, inputs, 2, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    // Gradient for A should have shape 2x3
    ASSERT_NE(gradients[0], nullptr);
    const nimcp_tensor_shape_t* grad_a_shape = nimcp_tensor_shape(gradients[0]);
    EXPECT_EQ(grad_a_shape->rank, 2u);
    EXPECT_EQ(grad_a_shape->dims[0], 2u);
    EXPECT_EQ(grad_a_shape->dims[1], 3u);

    // Gradient for B should have shape 3x2
    ASSERT_NE(gradients[1], nullptr);
    const nimcp_tensor_shape_t* grad_b_shape = nimcp_tensor_shape(gradients[1]);
    EXPECT_EQ(grad_b_shape->rank, 2u);
    EXPECT_EQ(grad_b_shape->dims[0], 3u);
    EXPECT_EQ(grad_b_shape->dims[1], 2u);

    // Cleanup
    nimcp_tensor_destroy(gradients[0]);
    nimcp_tensor_destroy(gradients[1]);
    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorEinsumAutodiffIntegration, EinsumTrace_ThenBackward_GradientShape) {
    // WHAT: Einsum trace produces scalar output, backward should give 2x2 gradient
    // WHY:  Trace is a common einsum pattern used in loss computation

    nimcp_tensor_t* a = Create2x2(1.0, 2.0, 3.0, 4.0);
    ASSERT_NE(a, nullptr);

    // Forward: trace via einsum
    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* trace = nimcp_tensor_einsum("ii->", tensors, 1);
    ASSERT_NE(trace, nullptr);

    // Verify trace value: 1 + 4 = 5
    EXPECT_NEAR(nimcp_tensor_get_flat(trace, 0), 5.0, EPSILON);

    // Backward
    nimcp_tensor_t* inputs[] = {a};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(nullptr, trace, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    ASSERT_NE(gradients[0], nullptr);
    const nimcp_tensor_shape_t* grad_shape = nimcp_tensor_shape(gradients[0]);
    EXPECT_EQ(grad_shape->rank, 2u);
    EXPECT_EQ(grad_shape->dims[0], 2u);
    EXPECT_EQ(grad_shape->dims[1], 2u);

    nimcp_tensor_destroy(gradients[0]);
    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(trace);
}

//=============================================================================
// Integration Tests: Numerical Gradient via Einsum-based Scalar Function
//=============================================================================

/**
 * @brief Scalar function that computes trace via einsum: f(A) = tr(A)
 *
 * WHAT: Wraps einsum "ii->" to compute trace as a scalar
 * WHY:  nimcp_tensor_numerical_gradient() needs a scalar function to differentiate
 */
static double einsum_trace_fn(const nimcp_tensor_t* x, void* ctx) {
    (void)ctx;
    nimcp_tensor_t* tensors[] = {const_cast<nimcp_tensor_t*>(x)};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ii->", tensors, 1);
    if (!result) return 0.0;
    double val = nimcp_tensor_get_flat(result, 0);
    nimcp_tensor_destroy(result);
    return val;
}

TEST_F(TensorEinsumAutodiffIntegration, NumericalGradient_EinsumTrace) {
    // WHAT: Compute gradient of trace(A) using numerical differentiation
    // WHY:  d(trace(A))/dA_ij = delta_ij (identity matrix)
    //       This verifies einsum+gradient integration produces correct analytical result

    nimcp_tensor_t* a = Create2x2(3.0, 7.0, 11.0, 13.0);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(einsum_trace_fn, a, 1e-5, nullptr);
    ASSERT_NE(grad, nullptr);

    // d(trace)/dA = I (identity matrix)
    // grad[0,0] = 1, grad[0,1] = 0, grad[1,0] = 0, grad[1,1] = 1
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 0), 1.0, GRAD_EPSILON);  // d/dA[0,0]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 1), 0.0, GRAD_EPSILON);  // d/dA[0,1]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 2), 0.0, GRAD_EPSILON);  // d/dA[1,0]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 3), 1.0, GRAD_EPSILON);  // d/dA[1,1]

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(grad);
}

/**
 * @brief Scalar function: f(A) = sum(einsum("ij->", A)) = sum of all elements
 */
static double einsum_sum_fn(const nimcp_tensor_t* x, void* ctx) {
    (void)ctx;
    nimcp_tensor_t* tensors[] = {const_cast<nimcp_tensor_t*>(x)};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij->", tensors, 1);
    if (!result) return 0.0;
    double val = nimcp_tensor_get_flat(result, 0);
    nimcp_tensor_destroy(result);
    return val;
}

TEST_F(TensorEinsumAutodiffIntegration, NumericalGradient_EinsumSumAll) {
    // WHAT: Gradient of sum(A) = all ones
    // WHY:  d(sum(A))/dA_ij = 1 for all i,j

    nimcp_tensor_t* a = Create2x2(1.0, 2.0, 3.0, 4.0);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(einsum_sum_fn, a, 1e-5, nullptr);
    ASSERT_NE(grad, nullptr);

    for (size_t i = 0; i < nimcp_tensor_numel(grad); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(grad, i), 1.0, GRAD_EPSILON)
            << "Sum gradient should be 1.0 at index " << i;
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(grad);
}

//=============================================================================
// Integration Tests: Einsum Matmul Gradient via Numerical Differentiation
//=============================================================================

/**
 * @brief Context for matmul gradient computation
 *
 * Stores the fixed B matrix so we can compute d/dA of sum(A @ B)
 */
struct MatmulGradCtx {
    nimcp_tensor_t* b;
};

/**
 * @brief Scalar function: f(A) = sum(einsum("ij,jk->ik", A, B))
 *
 * WHAT: Computes total sum of matmul result
 * WHY:  Provides a scalar objective for gradient computation w.r.t. A
 */
static double einsum_matmul_sum_fn(const nimcp_tensor_t* x, void* ctx) {
    MatmulGradCtx* mctx = static_cast<MatmulGradCtx*>(ctx);
    nimcp_tensor_t* tensors[] = {const_cast<nimcp_tensor_t*>(x), mctx->b};
    nimcp_tensor_t* product = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    if (!product) return 0.0;

    nimcp_tensor_t* total = nimcp_tensor_sum(product);
    double val = 0.0;
    if (total) {
        val = nimcp_tensor_get_flat(total, 0);
        nimcp_tensor_destroy(total);
    }
    nimcp_tensor_destroy(product);
    return val;
}

TEST_F(TensorEinsumAutodiffIntegration, NumericalGradient_EinsumMatmul_WrtA) {
    // WHAT: Gradient of sum(A @ B) with respect to A
    // WHY:  d(sum(A@B))/dA_ij = sum_k(B_jk) = row sum of B^T = column sum of B
    //
    // A = [[1,2],[3,4]], B = [[1,1],[1,1]]
    // sum(A@B) = sum([[3,3],[7,7]]) = 20
    // d/dA[i,j] = sum_k B[j,k]
    // For B = [[1,1],[1,1]]: d/dA = [[2,2],[2,2]]

    nimcp_tensor_t* a = Create2x2(1.0, 2.0, 3.0, 4.0);
    nimcp_tensor_t* b = Create2x2(1.0, 1.0, 1.0, 1.0);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    MatmulGradCtx ctx_data;
    ctx_data.b = b;

    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(
        einsum_matmul_sum_fn, a, 1e-5, &ctx_data);
    ASSERT_NE(grad, nullptr);

    // Each gradient element = sum of corresponding row in B
    // B row 0 sum = 2, B row 1 sum = 2
    // So d/dA = [[2,2],[2,2]]
    for (size_t i = 0; i < nimcp_tensor_numel(grad); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(grad, i), 2.0, GRAD_EPSILON)
            << "Matmul gradient mismatch at index " << i;
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(grad);
}

TEST_F(TensorEinsumAutodiffIntegration, NumericalGradient_EinsumMatmul_Asymmetric) {
    // WHAT: Gradient of sum(A @ B) where B is not all ones
    // WHY:  Test non-trivial gradient values
    //
    // A = [[1,0],[0,1]], B = [[2,3],[5,7]]
    // sum(A@B) = sum(B) = 17
    // d/dA[i,j] = sum_k B[j,k]
    //   d/dA[0,0] = B[0,0]+B[0,1] = 5
    //   d/dA[0,1] = B[1,0]+B[1,1] = 12
    //   d/dA[1,0] = B[0,0]+B[0,1] = 5
    //   d/dA[1,1] = B[1,0]+B[1,1] = 12

    nimcp_tensor_t* a = Create2x2(1.0, 0.0, 0.0, 1.0);
    nimcp_tensor_t* b = Create2x2(2.0, 3.0, 5.0, 7.0);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    MatmulGradCtx ctx_data;
    ctx_data.b = b;

    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(
        einsum_matmul_sum_fn, a, 1e-5, &ctx_data);
    ASSERT_NE(grad, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 0), 5.0, GRAD_EPSILON);   // d/dA[0,0]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 1), 12.0, GRAD_EPSILON);  // d/dA[0,1]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 2), 5.0, GRAD_EPSILON);   // d/dA[1,0]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 3), 12.0, GRAD_EPSILON);  // d/dA[1,1]

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(grad);
}

//=============================================================================
// Integration Tests: Chained Einsum + Backward
//=============================================================================

TEST_F(TensorEinsumAutodiffIntegration, ChainedEinsum_OuterThenContract) {
    // WHAT: Chain outer product then contraction, verify gradient pipeline
    // WHY:  Multi-step einsum is common in attention and tensor networks
    //
    // Step 1: outer = einsum("i,j->ij", u, v)
    // Step 2: trace = einsum("ii->", outer)
    // Analytically: trace(u (x) v) = sum_i u_i * v_i = dot(u, v)

    float u_data[] = {2.0f, 3.0f};
    float v_data[] = {5.0f, 7.0f};
    uint32_t dims[] = {2};

    nimcp_tensor_t* u = nimcp_tensor_from_data(u_data, dims, 1, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* v = nimcp_tensor_from_data(v_data, dims, 1, NIMCP_DTYPE_F32, true);
    ASSERT_NE(u, nullptr);
    ASSERT_NE(v, nullptr);

    // Outer product
    nimcp_tensor_t* uv_tensors[] = {u, v};
    nimcp_tensor_t* outer = nimcp_tensor_einsum("i,j->ij", uv_tensors, 2);
    ASSERT_NE(outer, nullptr);

    // Trace of outer product = dot product
    nimcp_tensor_t* trace_tensors[] = {outer};
    nimcp_tensor_t* trace = nimcp_tensor_einsum("ii->", trace_tensors, 1);
    ASSERT_NE(trace, nullptr);

    // dot(u,v) = 2*5 + 3*7 = 10 + 21 = 31
    EXPECT_NEAR(nimcp_tensor_get_flat(trace, 0), 31.0, EPSILON);

    // Backward from trace: gradients w.r.t. outer matrix
    nimcp_tensor_t* backward_inputs[] = {outer};
    nimcp_tensor_t* backward_grads[1] = {nullptr};

    int rc = nimcp_autodiff_backward(nullptr, trace, backward_inputs, 1, backward_grads);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    ASSERT_NE(backward_grads[0], nullptr);
    EXPECT_EQ(nimcp_tensor_rank(backward_grads[0]), 2u);
    EXPECT_EQ(nimcp_tensor_numel(backward_grads[0]), 4u);

    nimcp_tensor_destroy(backward_grads[0]);
    nimcp_tensor_destroy(u);
    nimcp_tensor_destroy(v);
    nimcp_tensor_destroy(outer);
    nimcp_tensor_destroy(trace);
}

TEST_F(TensorEinsumAutodiffIntegration, EinsumDotProduct_NumericalGradient) {
    // WHAT: Numerical gradient of dot product via einsum
    // WHY:  d(u.v)/du_i = v_i - verify einsum correctly computes this

    struct DotCtx { nimcp_tensor_t* v; };

    auto dot_fn = [](const nimcp_tensor_t* u, void* ctx) -> double {
        DotCtx* dc = static_cast<DotCtx*>(ctx);
        nimcp_tensor_t* tensors[] = {const_cast<nimcp_tensor_t*>(u), dc->v};
        nimcp_tensor_t* result = nimcp_tensor_einsum("i,i->", tensors, 2);
        if (!result) return 0.0;
        double val = nimcp_tensor_get_flat(result, 0);
        nimcp_tensor_destroy(result);
        return val;
    };

    float u_data[] = {1.0f, 2.0f, 3.0f};
    float v_data[] = {4.0f, 5.0f, 6.0f};
    uint32_t dims[] = {3};

    nimcp_tensor_t* u = nimcp_tensor_from_data(u_data, dims, 1, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* v = nimcp_tensor_from_data(v_data, dims, 1, NIMCP_DTYPE_F32, true);
    ASSERT_NE(u, nullptr);
    ASSERT_NE(v, nullptr);

    DotCtx ctx_data;
    ctx_data.v = v;

    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(dot_fn, u, 1e-5, &ctx_data);
    ASSERT_NE(grad, nullptr);

    // d(u.v)/du_i = v_i
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 0), 4.0, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 1), 5.0, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 2), 6.0, GRAD_EPSILON);

    nimcp_tensor_destroy(u);
    nimcp_tensor_destroy(v);
    nimcp_tensor_destroy(grad);
}

//=============================================================================
// Integration Tests: Gradient Accumulation
//=============================================================================

TEST_F(TensorEinsumAutodiffIntegration, GradientAccumulation_MultipleBackward) {
    // WHAT: Running backward twice accumulates gradients on requires_grad tensors
    // WHY:  Gradient accumulation is essential for mini-batch training

    uint32_t dims[] = {2};
    nimcp_tensor_t* input = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 3.0);
    ASSERT_NE(input, nullptr);
    nimcp_tensor_set_requires_grad(input, true);

    // First backward pass (output = input, identity => gradient = 1.0)
    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* grads1[1] = {nullptr};

    int rc = nimcp_autodiff_backward(nullptr, input, inputs, 1, grads1);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    ASSERT_NE(grads1[0], nullptr);

    // Second backward pass - gradients should accumulate
    nimcp_tensor_t* grads2[1] = {nullptr};
    rc = nimcp_autodiff_backward(nullptr, input, inputs, 1, grads2);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    ASSERT_NE(grads2[0], nullptr);

    // The stored gradient should be accumulated (approx 1.0 + approx 1.0 ~ 2.0)
    // Numerical fallback for F32 has ~1e-3 per pass, so accumulated error ~2e-3
    nimcp_tensor_t* stored_grad = nimcp_tensor_grad(input);
    ASSERT_NE(stored_grad, nullptr);

    for (size_t i = 0; i < nimcp_tensor_numel(stored_grad); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(stored_grad, i), 2.0, 0.01)
            << "Accumulated gradient should be ~2.0 at index " << i;
    }

    nimcp_tensor_destroy(grads1[0]);
    nimcp_tensor_destroy(grads2[0]);
    nimcp_tensor_destroy(input);
}

TEST_F(TensorEinsumAutodiffIntegration, ZeroGrad_ResetsAccumulation) {
    // WHAT: zero_grad clears accumulated gradient, then new backward starts fresh
    // WHY:  Must reset between training epochs

    uint32_t dims[] = {3};
    nimcp_tensor_t* input = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 2.0);
    ASSERT_NE(input, nullptr);
    nimcp_tensor_set_requires_grad(input, true);

    // First backward
    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* grads1[1] = {nullptr};
    int rc = nimcp_autodiff_backward(nullptr, input, inputs, 1, grads1);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    nimcp_tensor_destroy(grads1[0]);

    // Zero grad
    rc = nimcp_tensor_zero_grad(input);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    // Second backward after zeroing
    nimcp_tensor_t* grads2[1] = {nullptr};
    rc = nimcp_autodiff_backward(nullptr, input, inputs, 1, grads2);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    nimcp_tensor_destroy(grads2[0]);

    // Stored gradient should be fresh 1.0, not accumulated 2.0
    // Note: zero_grad zeros the buffer, then backward adds 1.0, so result is 1.0
    nimcp_tensor_t* stored_grad = nimcp_tensor_grad(input);
    ASSERT_NE(stored_grad, nullptr);

    for (size_t i = 0; i < nimcp_tensor_numel(stored_grad); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(stored_grad, i), 1.0, EPSILON)
            << "After zero_grad + backward, gradient should be 1.0 at index " << i;
    }

    nimcp_tensor_destroy(input);
}

//=============================================================================
// Integration Tests: Einsum + Autodiff Context Lifecycle
//=============================================================================

TEST_F(TensorEinsumAutodiffIntegration, ContextLifecycle_StartStopBackward) {
    // WHAT: Create context, start recording, do einsum, stop, then backward
    // WHY:  Full lifecycle test for tape-based autodiff with einsum

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    int rc = nimcp_autodiff_start(ctx);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    // Einsum forward
    nimcp_tensor_t* a = Create2x2(1.0, 2.0, 3.0, 4.0);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* tensors[] = {a};
    nimcp_tensor_t* trace = nimcp_tensor_einsum("ii->", tensors, 1);
    ASSERT_NE(trace, nullptr);

    rc = nimcp_autodiff_stop(ctx);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    // Backward through context
    nimcp_tensor_t* inputs[] = {a};
    nimcp_tensor_t* gradients[1] = {nullptr};

    rc = nimcp_autodiff_backward(ctx, trace, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    ASSERT_NE(gradients[0], nullptr);

    // Gradient shape must match input
    EXPECT_EQ(nimcp_tensor_rank(gradients[0]), 2u);
    EXPECT_EQ(nimcp_tensor_numel(gradients[0]), 4u);

    nimcp_tensor_destroy(gradients[0]);
    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(trace);
}

//=============================================================================
// Integration Tests: Statistics Tracking Through Pipeline
//=============================================================================

TEST_F(TensorEinsumAutodiffIntegration, StatsTracking_EinsumAndBackward) {
    // WHAT: Verify operation counters increment through einsum + backward pipeline
    // WHY:  Performance monitoring requires accurate stats

    nimcp_tensor_reset_stats();

    nimcp_tensor_t* a = Create2x2(1.0, 2.0, 3.0, 4.0);
    nimcp_tensor_t* b = Create2x2(5.0, 6.0, 7.0, 8.0);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Einsum matmul (counts as contraction)
    nimcp_tensor_t* tensors[] = {a, b};
    nimcp_tensor_t* c = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(c, nullptr);

    // Backward (counts as calculus op)
    nimcp_tensor_t* inputs[] = {a};
    nimcp_tensor_t* gradients[1] = {nullptr};
    nimcp_autodiff_backward(nullptr, c, inputs, 1, gradients);

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);

    // Should have at least 3 tensors created (a, b, c) + any gradient tensors
    EXPECT_GE(stats.tensors_created, 3u);
    // Einsum matmul should count as contraction or matmul op
    EXPECT_GE(stats.ops_contraction + stats.ops_matmul, 1u);

    if (gradients[0]) nimcp_tensor_destroy(gradients[0]);
    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

//=============================================================================
// Integration Tests: Einsum Diagonal + Backward
//=============================================================================

/**
 * @brief Scalar function: f(A) = sum(diag(A)) via einsum "ii->i" then sum
 */
static double einsum_diag_sum_fn(const nimcp_tensor_t* x, void* ctx) {
    (void)ctx;
    nimcp_tensor_t* tensors[] = {const_cast<nimcp_tensor_t*>(x)};
    nimcp_tensor_t* diag = nimcp_tensor_einsum("ii->i", tensors, 1);
    if (!diag) return 0.0;

    nimcp_tensor_t* total = nimcp_tensor_sum(diag);
    double val = 0.0;
    if (total) {
        val = nimcp_tensor_get_flat(total, 0);
        nimcp_tensor_destroy(total);
    }
    nimcp_tensor_destroy(diag);
    return val;
}

TEST_F(TensorEinsumAutodiffIntegration, NumericalGradient_EinsumDiagonal) {
    // WHAT: Gradient of sum(diag(A)) should equal identity matrix
    // WHY:  sum(diag(A)) = trace(A), so d/dA_ij = delta_ij
    //       This tests the diagonal extraction + sum pipeline

    nimcp_tensor_t* a = Create2x2(10.0, 20.0, 30.0, 40.0);
    ASSERT_NE(a, nullptr);

    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(einsum_diag_sum_fn, a, 1e-5, nullptr);
    ASSERT_NE(grad, nullptr);

    // d(sum(diag(A)))/dA = I
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 0), 1.0, GRAD_EPSILON);  // [0,0]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 1), 0.0, GRAD_EPSILON);  // [0,1]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 2), 0.0, GRAD_EPSILON);  // [1,0]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 3), 1.0, GRAD_EPSILON);  // [1,1]

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(grad);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
