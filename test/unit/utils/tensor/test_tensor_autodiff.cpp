//=============================================================================
// test_tensor_autodiff.cpp - Unit Tests for Autodiff Backward Pass
//=============================================================================
/**
 * @file test_tensor_autodiff.cpp
 * @brief Unit tests for nimcp_autodiff_backward() and related functions
 *
 * WHAT: Tests the automatic differentiation backward pass
 * WHY:  Autodiff is the core of neural network training; gradients must be correct
 * HOW:  Test with known tensor configurations and verify gradient shapes/values
 *
 * TEST COVERAGE:
 * 1. Simple scalar backward pass (no tape -> numerical fallback)
 * 2. Numerical gradient fallback for disconnected tensors
 * 3. Error cases: NULL output, NULL inputs, NULL gradients
 * 4. Gradient shape matches input shape
 * 5. Autodiff context lifecycle (create/destroy)
 * 6. requires_grad flag and gradient storage
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>

// Headers have their own extern "C" guards
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TensorAutodiffTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_tensor_init();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }
};

//=============================================================================
// Unit Tests: Autodiff Context Lifecycle
//=============================================================================

TEST_F(TensorAutodiffTest, ContextCreate) {
    // WHAT: Create and destroy autodiff context
    // WHY:  Basic lifecycle must work without leaks or crashes

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);
    nimcp_autodiff_destroy(ctx);
}

TEST_F(TensorAutodiffTest, ContextDestroyNull) {
    // WHAT: Destroying NULL context should be safe (no-op)
    // WHY:  Defensive programming; callers may have NULL

    nimcp_autodiff_destroy(nullptr);
    SUCCEED() << "Destroy of NULL context does not crash";
}

TEST_F(TensorAutodiffTest, ContextStartStop) {
    // WHAT: Start/stop recording should succeed
    // WHY:  Basic tape control must work

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    int rc = nimcp_autodiff_start(ctx);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    rc = nimcp_autodiff_stop(ctx);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    nimcp_autodiff_destroy(ctx);
}

TEST_F(TensorAutodiffTest, StartWithNullContext) {
    // WHAT: Start with NULL context should return error
    int rc = nimcp_autodiff_start(nullptr);
    EXPECT_EQ(rc, NIMCP_TENSOR_ERR_NULL);
}

TEST_F(TensorAutodiffTest, StopWithNullContext) {
    // WHAT: Stop with NULL context should return error
    int rc = nimcp_autodiff_stop(nullptr);
    EXPECT_EQ(rc, NIMCP_TENSOR_ERR_NULL);
}

//=============================================================================
// Unit Tests: Backward Pass - Numerical Fallback
//=============================================================================

TEST_F(TensorAutodiffTest, NumericalFallback_ScalarOutput) {
    // WHAT: Backward pass without tape falls back to numerical gradients
    // WHY:  When output IS the input (shared memory), numerical perturbation
    //       should detect the gradient as 1.0 for each element
    //
    // Setup: output = input (same tensor, so dOutput/dInput = identity)

    uint32_t dims[] = {3};
    nimcp_tensor_t* input = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 2.0);
    ASSERT_NE(input, nullptr);

    // Use input as its own output (trivial identity function)
    // The numerical fallback perturbs input and re-reads output.
    // Since output IS input, gradient should be 1.0 everywhere.
    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(ctx, input, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    ASSERT_NE(gradients[0], nullptr);
    EXPECT_EQ(nimcp_tensor_numel(gradients[0]), 3u);

    // Each element's gradient should be 1.0 (identity)
    for (size_t i = 0; i < nimcp_tensor_numel(gradients[0]); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(gradients[0], i), 1.0f, EPSILON);
    }

    nimcp_tensor_destroy(gradients[0]);
    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(input);
}

TEST_F(TensorAutodiffTest, NumericalFallback_DisconnectedTensors) {
    // WHAT: Gradients are zero when output is independent of input
    // WHY:  Numerical perturbation of input should not change output
    //
    // Setup: output and input are completely separate tensors

    uint32_t dims[] = {4};
    nimcp_tensor_t* input = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 3.0);
    nimcp_tensor_t* output = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 7.0);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(ctx, output, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    ASSERT_NE(gradients[0], nullptr);

    // All gradients should be zero (output doesn't depend on input)
    for (size_t i = 0; i < nimcp_tensor_numel(gradients[0]); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(gradients[0], i), 0.0f, EPSILON);
    }

    nimcp_tensor_destroy(gradients[0]);
    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(output);
}

TEST_F(TensorAutodiffTest, NumericalFallback_NullContext) {
    // WHAT: NULL context should trigger numerical fallback (not crash)
    // WHY:  The function documents ctx may be NULL for numerical-only mode

    uint32_t dims[] = {2};
    nimcp_tensor_t* input = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 1.0);
    ASSERT_NE(input, nullptr);

    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};

    // output = input (identity), ctx = NULL
    int rc = nimcp_autodiff_backward(nullptr, input, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    ASSERT_NE(gradients[0], nullptr);

    // Identity: gradient = 1.0
    for (size_t i = 0; i < nimcp_tensor_numel(gradients[0]); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(gradients[0], i), 1.0f, EPSILON);
    }

    nimcp_tensor_destroy(gradients[0]);
    nimcp_tensor_destroy(input);
}

//=============================================================================
// Unit Tests: Gradient Shape
//=============================================================================

TEST_F(TensorAutodiffTest, GradientShape_MatchesInput_1D) {
    // WHAT: Gradient tensor must have the same shape as the input tensor
    // WHY:  Each gradient element corresponds to an input element

    uint32_t dims[] = {5};
    nimcp_tensor_t* input = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(ctx, output, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    ASSERT_NE(gradients[0], nullptr);

    // Shape must match input
    const nimcp_tensor_shape_t* grad_shape = nimcp_tensor_shape(gradients[0]);
    const nimcp_tensor_shape_t* input_shape = nimcp_tensor_shape(input);
    ASSERT_NE(grad_shape, nullptr);
    ASSERT_NE(input_shape, nullptr);
    EXPECT_EQ(grad_shape->rank, input_shape->rank);
    EXPECT_EQ(grad_shape->dims[0], input_shape->dims[0]);
    EXPECT_EQ(grad_shape->numel, input_shape->numel);

    nimcp_tensor_destroy(gradients[0]);
    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(output);
}

TEST_F(TensorAutodiffTest, GradientShape_MatchesInput_2D) {
    // WHAT: Gradient shape matches 2D input
    uint32_t dims[] = {3, 4};
    nimcp_tensor_t* input = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(ctx, output, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    ASSERT_NE(gradients[0], nullptr);

    const nimcp_tensor_shape_t* grad_shape = nimcp_tensor_shape(gradients[0]);
    const nimcp_tensor_shape_t* input_shape = nimcp_tensor_shape(input);
    EXPECT_EQ(grad_shape->rank, 2u);
    EXPECT_EQ(grad_shape->dims[0], 3u);
    EXPECT_EQ(grad_shape->dims[1], 4u);
    EXPECT_EQ(grad_shape->numel, input_shape->numel);

    nimcp_tensor_destroy(gradients[0]);
    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(input);
    nimcp_tensor_destroy(output);
}

TEST_F(TensorAutodiffTest, GradientShape_MultipleInputs) {
    // WHAT: Each gradient matches its corresponding input shape
    // WHY:  Backward with multiple inputs produces one gradient per input

    uint32_t dims_a[] = {3};
    uint32_t dims_b[] = {2, 4};
    nimcp_tensor_t* input_a = nimcp_tensor_ones(dims_a, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* input_b = nimcp_tensor_ones(dims_b, 2, NIMCP_DTYPE_F32);
    nimcp_tensor_t* output = nimcp_tensor_ones(dims_a, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(input_a, nullptr);
    ASSERT_NE(input_b, nullptr);
    ASSERT_NE(output, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {input_a, input_b};
    nimcp_tensor_t* gradients[2] = {nullptr, nullptr};

    int rc = nimcp_autodiff_backward(ctx, output, inputs, 2, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    // Gradient 0 shape matches input_a
    ASSERT_NE(gradients[0], nullptr);
    EXPECT_EQ(nimcp_tensor_rank(gradients[0]), 1u);
    EXPECT_EQ(nimcp_tensor_numel(gradients[0]), 3u);

    // Gradient 1 shape matches input_b
    ASSERT_NE(gradients[1], nullptr);
    EXPECT_EQ(nimcp_tensor_rank(gradients[1]), 2u);
    const nimcp_tensor_shape_t* g1_shape = nimcp_tensor_shape(gradients[1]);
    EXPECT_EQ(g1_shape->dims[0], 2u);
    EXPECT_EQ(g1_shape->dims[1], 4u);

    nimcp_tensor_destroy(gradients[0]);
    nimcp_tensor_destroy(gradients[1]);
    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(input_a);
    nimcp_tensor_destroy(input_b);
    nimcp_tensor_destroy(output);
}

//=============================================================================
// Unit Tests: requires_grad and gradient storage
//=============================================================================

TEST_F(TensorAutodiffTest, RequiresGrad_StoresGradient) {
    // WHAT: When requires_grad is set, backward stores gradient on tensor
    // WHY:  This is how gradient accumulation works in training

    uint32_t dims[] = {3};
    nimcp_tensor_t* input = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 2.0);
    ASSERT_NE(input, nullptr);

    // Set requires_grad
    nimcp_tensor_set_requires_grad(input, true);
    EXPECT_TRUE(nimcp_tensor_requires_grad(input));

    // No stored gradient initially
    nimcp_tensor_t* grad_before = nimcp_tensor_grad(input);
    EXPECT_EQ(grad_before, nullptr);

    // Run backward (output = input, identity)
    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(nullptr, input, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    ASSERT_NE(gradients[0], nullptr);

    // After backward, gradient should be stored on input
    nimcp_tensor_t* stored_grad = nimcp_tensor_grad(input);
    EXPECT_NE(stored_grad, nullptr);

    nimcp_tensor_destroy(gradients[0]);
    nimcp_tensor_destroy(input);
}

TEST_F(TensorAutodiffTest, ZeroGrad) {
    // WHAT: nimcp_tensor_zero_grad clears accumulated gradient
    // WHY:  Must zero grads between training iterations

    uint32_t dims[] = {2};
    nimcp_tensor_t* input = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 1.0);
    ASSERT_NE(input, nullptr);
    nimcp_tensor_set_requires_grad(input, true);

    // Run backward to accumulate gradient
    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};
    int rc = nimcp_autodiff_backward(nullptr, input, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    nimcp_tensor_destroy(gradients[0]);

    // Gradient should exist
    EXPECT_NE(nimcp_tensor_grad(input), nullptr);

    // Zero grad
    rc = nimcp_tensor_zero_grad(input);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    nimcp_tensor_destroy(input);
}

//=============================================================================
// Unit Tests: Error Cases
//=============================================================================

TEST_F(TensorAutodiffTest, Error_NullOutput) {
    // WHAT: NULL output tensor should return NIMCP_TENSOR_ERR_NULL
    uint32_t dims[] = {3};
    nimcp_tensor_t* input = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(input, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(ctx, nullptr, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_ERR_NULL);

    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(input);
}

TEST_F(TensorAutodiffTest, Error_NullInputs) {
    // WHAT: NULL inputs array should return NIMCP_TENSOR_ERR_NULL
    uint32_t dims[] = {3};
    nimcp_tensor_t* output = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(output, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(ctx, output, nullptr, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_ERR_NULL);

    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(output);
}

TEST_F(TensorAutodiffTest, Error_ZeroNumInputs) {
    // WHAT: Zero num_inputs should return NIMCP_TENSOR_ERR_NULL
    uint32_t dims[] = {3};
    nimcp_tensor_t* output = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* input = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(output, nullptr);
    ASSERT_NE(input, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {input};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(ctx, output, inputs, 0, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_ERR_NULL);

    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(output);
    nimcp_tensor_destroy(input);
}

TEST_F(TensorAutodiffTest, Error_NullGradients) {
    // WHAT: NULL gradients output array should return NIMCP_TENSOR_ERR_NULL
    uint32_t dims[] = {3};
    nimcp_tensor_t* output = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* input = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(output, nullptr);
    ASSERT_NE(input, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {input};

    int rc = nimcp_autodiff_backward(ctx, output, inputs, 1, nullptr);
    EXPECT_EQ(rc, NIMCP_TENSOR_ERR_NULL);

    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(output);
    nimcp_tensor_destroy(input);
}

TEST_F(TensorAutodiffTest, Error_NullInputTensor) {
    // WHAT: NULL tensor in inputs array should return NIMCP_TENSOR_ERR_NULL
    uint32_t dims[] = {3};
    nimcp_tensor_t* output = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(output, nullptr);

    nimcp_autodiff_ctx_t* ctx = nimcp_autodiff_create();
    ASSERT_NE(ctx, nullptr);

    nimcp_tensor_t* inputs[] = {nullptr};
    nimcp_tensor_t* gradients[1] = {nullptr};

    int rc = nimcp_autodiff_backward(ctx, output, inputs, 1, gradients);
    EXPECT_EQ(rc, NIMCP_TENSOR_ERR_NULL);

    nimcp_autodiff_destroy(ctx);
    nimcp_tensor_destroy(output);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
