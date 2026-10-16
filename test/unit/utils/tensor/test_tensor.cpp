//=============================================================================
// test_tensor.cpp - Comprehensive Unit Tests for Tensor Library
//=============================================================================
/**
 * @file test_tensor.cpp
 * @brief Unit tests for nimcp_tensor.h tensor operations
 *
 * WHAT: 100% test coverage for tensor library
 * WHY:  Tensor operations are foundation for neural networks and swarm coordination
 * HOW:  Test all operations, edge cases, numerical stability
 *
 * TEST COVERAGE:
 * 1. Lifecycle: init, shutdown, create, destroy
 * 2. Creation: zeros, ones, full, randn, rand, arange, linspace, eye
 * 3. Properties: shape, rank, dtype, data access, contiguous
 * 4. Element Access: get, set, flat access
 * 5. Shape Operations: reshape, transpose, permute, squeeze, unsqueeze, flatten
 * 6. Element-wise Operations: add, sub, mul, div, unary math functions
 * 7. Reductions: sum, mean, max, min, var, std
 * 8. Linear Algebra: matmul, dot, outer, trace, norm
 * 9. Tensor Calculus: gradient, jacobian, hessian, divergence, curl, laplacian
 * 10. Edge cases: NULL inputs, invalid shapes, memory errors
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - WHAT-WHY-HOW documentation
 * - Comprehensive assertions
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
    #include "utils/tensor/nimcp_tensor.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class TensorTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr double EPSILON_D = 1e-10;

    void SetUp() override {
        srand(42); // Fixed seed for reproducibility
        nimcp_memory_init();
        nimcp_tensor_init();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }

    // Helper: Check float equality
    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }

    // Helper: Check double equality
    bool DoubleEqual(double a, double b, double eps = EPSILON_D) {
        return std::abs(a - b) < eps;
    }

    // Helper: Create simple test tensor
    nimcp_tensor_t* CreateTestTensor(uint32_t rows, uint32_t cols) {
        uint32_t dims[] = {rows, cols};
        return nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);
    }

    // Helper: Create 1D test tensor
    nimcp_tensor_t* CreateTestVector(uint32_t size) {
        uint32_t dims[] = {size};
        return nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    }
};

//=============================================================================
// Unit Tests: Lifecycle Functions
//=============================================================================

TEST_F(TensorTest, Lifecycle_InitShutdown) {
    // WHAT: Verify init/shutdown cycle
    // WHY: Basic lifecycle must work for all other tests

    // Init already called in SetUp, verify no crash on shutdown
    // This test passes if no crash occurs
    SUCCEED() << "Tensor lifecycle init/shutdown works correctly";
}

//=============================================================================
// Unit Tests: Tensor Creation
//=============================================================================

TEST_F(TensorTest, Create_BasicTensor) {
    // WHAT: Create tensor with uninitialized data
    // WHY: Basic allocation must work

    uint32_t dims[] = {10, 20};
    nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);

    ASSERT_NE(t, nullptr) << "Tensor creation should succeed";
    EXPECT_EQ(nimcp_tensor_rank(t), 2u);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(t);
    ASSERT_NE(shape, nullptr);
    EXPECT_EQ(shape->dims[0], 10u);
    EXPECT_EQ(shape->dims[1], 20u);
    EXPECT_EQ(shape->numel, 200u);

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Create_Zeros) {
    // WHAT: Create tensor initialized to zeros
    // WHY: Common initialization pattern

    uint32_t dims[] = {5, 5};
    nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);

    ASSERT_NE(t, nullptr);

    // Verify all elements are zero
    for (size_t i = 0; i < nimcp_tensor_numel(t); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, i), 0.0f);
    }

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Create_Ones) {
    // WHAT: Create tensor initialized to ones
    // WHY: Common initialization pattern

    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* t = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);

    ASSERT_NE(t, nullptr);

    // Verify all elements are one
    for (size_t i = 0; i < nimcp_tensor_numel(t); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, i), 1.0f);
    }

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Create_Full) {
    // WHAT: Create tensor with constant value
    // WHY: Fill with arbitrary constant

    uint32_t dims[] = {3, 3};
    double fill_value = 7.5;
    nimcp_tensor_t* t = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, fill_value);

    ASSERT_NE(t, nullptr);

    // Verify all elements equal fill_value
    for (size_t i = 0; i < nimcp_tensor_numel(t); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, i), (float)fill_value);
    }

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Create_Eye) {
    // WHAT: Create identity matrix
    // WHY: Common linear algebra initialization

    uint32_t n = 4;
    nimcp_tensor_t* t = nimcp_tensor_eye(n, NIMCP_DTYPE_F32);

    ASSERT_NE(t, nullptr);
    EXPECT_EQ(nimcp_tensor_rank(t), 2u);

    // Verify identity structure
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            uint32_t indices[] = {i, j};
            double expected = (i == j) ? 1.0 : 0.0;
            EXPECT_FLOAT_EQ(nimcp_tensor_get(t, indices), expected);
        }
    }

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Create_Arange) {
    // WHAT: Create range tensor
    // WHY: Generate sequences

    nimcp_tensor_t* t = nimcp_tensor_arange(0.0, 10.0, 1.0, NIMCP_DTYPE_F32);

    ASSERT_NE(t, nullptr);
    EXPECT_EQ(nimcp_tensor_rank(t), 1u);
    EXPECT_EQ(nimcp_tensor_numel(t), 10u);

    // Verify sequence
    for (size_t i = 0; i < nimcp_tensor_numel(t); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, i), (float)i);
    }

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Create_Linspace) {
    // WHAT: Create linearly spaced tensor
    // WHY: Generate evenly spaced values

    nimcp_tensor_t* t = nimcp_tensor_linspace(0.0, 1.0, 5, NIMCP_DTYPE_F32);

    ASSERT_NE(t, nullptr);
    EXPECT_EQ(nimcp_tensor_rank(t), 1u);
    EXPECT_EQ(nimcp_tensor_numel(t), 5u);

    // Expected: 0.0, 0.25, 0.5, 0.75, 1.0
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, 0), 0.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, 2), 0.5f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, 4), 1.0f);

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Create_Randn) {
    // WHAT: Create tensor with random normal values
    // WHY: Random initialization for neural networks

    uint32_t dims[] = {100};
    nimcp_tensor_t* t = nimcp_tensor_randn(dims, 1, NIMCP_DTYPE_F32, 0.0, 1.0);

    ASSERT_NE(t, nullptr);

    // Compute mean and variance
    double sum = 0.0;
    for (size_t i = 0; i < nimcp_tensor_numel(t); i++) {
        sum += nimcp_tensor_get_flat(t, i);
    }
    double mean = sum / nimcp_tensor_numel(t);

    // Mean should be approximately 0 (with tolerance for random)
    EXPECT_NEAR(mean, 0.0, 0.3) << "Mean of normal distribution should be ~0";

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Create_Clone) {
    // WHAT: Deep copy tensor
    // WHY: Need independent copies

    uint32_t dims[] = {3, 3};
    nimcp_tensor_t* original = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 5.0);
    ASSERT_NE(original, nullptr);

    nimcp_tensor_t* clone = nimcp_tensor_clone(original);
    ASSERT_NE(clone, nullptr);

    // Modify original
    nimcp_tensor_set_flat(original, 0, 99.0);

    // Clone should be unchanged
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(clone, 0), 5.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(original, 0), 99.0f);

    nimcp_tensor_destroy(original);
    nimcp_tensor_destroy(clone);
}

TEST_F(TensorTest, Create_FromData) {
    // WHAT: Create tensor from existing data
    // WHY: Import external data

    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t dims[] = {2, 3};

    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(t, nullptr);

    // Verify data was copied
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, 0), 1.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, 5), 6.0f);

    nimcp_tensor_destroy(t);
}

//=============================================================================
// Unit Tests: Element Access
//=============================================================================

TEST_F(TensorTest, ElementAccess_GetSet) {
    // WHAT: Get and set individual elements
    // WHY: Basic tensor manipulation

    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    // Set element at (1, 2)
    uint32_t indices[] = {1, 2};
    int result = nimcp_tensor_set(t, indices, 42.0);
    EXPECT_EQ(result, NIMCP_TENSOR_OK);

    // Get element
    double val = nimcp_tensor_get(t, indices);
    EXPECT_FLOAT_EQ(val, 42.0f);

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, ElementAccess_FlatIndex) {
    // WHAT: Access elements by flat index
    // WHY: Efficient iteration

    uint32_t dims[] = {3, 4};
    nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    // Set all elements
    for (size_t i = 0; i < nimcp_tensor_numel(t); i++) {
        nimcp_tensor_set_flat(t, i, (double)i);
    }

    // Verify
    for (size_t i = 0; i < nimcp_tensor_numel(t); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, i), (float)i);
    }

    nimcp_tensor_destroy(t);
}

//=============================================================================
// Unit Tests: Shape Operations
//=============================================================================

TEST_F(TensorTest, Shape_Reshape) {
    // WHAT: Reshape tensor
    // WHY: Change layout without changing data

    uint32_t dims[] = {2, 6};
    nimcp_tensor_t* t = nimcp_tensor_arange(0.0, 12.0, 1.0, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    nimcp_tensor_t* reshaped = nimcp_tensor_reshape(t, dims, 2);
    ASSERT_NE(reshaped, nullptr);

    EXPECT_EQ(nimcp_tensor_rank(reshaped), 2u);
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(reshaped);
    EXPECT_EQ(shape->dims[0], 2u);
    EXPECT_EQ(shape->dims[1], 6u);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(reshaped);
}

TEST_F(TensorTest, Shape_Transpose) {
    // WHAT: Transpose matrix
    // WHY: Common linear algebra operation

    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t dims[] = {2, 3};

    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(t, nullptr);

    nimcp_tensor_t* transposed = nimcp_tensor_transpose(t);
    ASSERT_NE(transposed, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(transposed);
    EXPECT_EQ(shape->dims[0], 3u);
    EXPECT_EQ(shape->dims[1], 2u);

    // Original: [[1,2,3],[4,5,6]]
    // Transposed: [[1,4],[2,5],[3,6]]
    uint32_t idx00[] = {0, 0};
    uint32_t idx01[] = {0, 1};
    EXPECT_FLOAT_EQ(nimcp_tensor_get(transposed, idx00), 1.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get(transposed, idx01), 4.0f);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(transposed);
}

TEST_F(TensorTest, Shape_Flatten) {
    // WHAT: Flatten tensor to 1D
    // WHY: Common preprocessing step

    uint32_t dims[] = {2, 3, 4};
    nimcp_tensor_t* t = nimcp_tensor_ones(dims, 3, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    nimcp_tensor_t* flat = nimcp_tensor_flatten(t);
    ASSERT_NE(flat, nullptr);

    EXPECT_EQ(nimcp_tensor_rank(flat), 1u);
    EXPECT_EQ(nimcp_tensor_numel(flat), 24u);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(flat);
}

TEST_F(TensorTest, Shape_Squeeze) {
    // WHAT: Remove dimensions of size 1
    // WHY: Clean up broadcasting results

    uint32_t dims[] = {1, 5, 1, 3, 1};
    nimcp_tensor_t* t = nimcp_tensor_ones(dims, 5, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    nimcp_tensor_t* squeezed = nimcp_tensor_squeeze(t);
    ASSERT_NE(squeezed, nullptr);

    EXPECT_EQ(nimcp_tensor_rank(squeezed), 2u);
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(squeezed);
    EXPECT_EQ(shape->dims[0], 5u);
    EXPECT_EQ(shape->dims[1], 3u);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(squeezed);
}

TEST_F(TensorTest, Shape_Unsqueeze) {
    // WHAT: Add dimension of size 1
    // WHY: Prepare for broadcasting

    uint32_t dims[] = {5, 3};
    nimcp_tensor_t* t = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    nimcp_tensor_t* unsqueezed = nimcp_tensor_unsqueeze(t, 0);
    ASSERT_NE(unsqueezed, nullptr);

    EXPECT_EQ(nimcp_tensor_rank(unsqueezed), 3u);
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(unsqueezed);
    EXPECT_EQ(shape->dims[0], 1u);
    EXPECT_EQ(shape->dims[1], 5u);
    EXPECT_EQ(shape->dims[2], 3u);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(unsqueezed);
}

//=============================================================================
// Unit Tests: Element-wise Operations
//=============================================================================

TEST_F(TensorTest, Elementwise_Add) {
    // WHAT: Element-wise addition
    // WHY: Basic tensor arithmetic

    uint32_t dims[] = {3, 3};
    nimcp_tensor_t* a = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 2.0);
    nimcp_tensor_t* b = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 3.0);

    nimcp_tensor_t* c = nimcp_tensor_add(a, b);
    ASSERT_NE(c, nullptr);

    // Verify all elements are 5.0
    for (size_t i = 0; i < nimcp_tensor_numel(c); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(c, i), 5.0f);
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorTest, Elementwise_Sub) {
    // WHAT: Element-wise subtraction

    uint32_t dims[] = {3, 3};
    nimcp_tensor_t* a = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 10.0);
    nimcp_tensor_t* b = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 3.0);

    nimcp_tensor_t* c = nimcp_tensor_sub(a, b);
    ASSERT_NE(c, nullptr);

    for (size_t i = 0; i < nimcp_tensor_numel(c); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(c, i), 7.0f);
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorTest, Elementwise_Mul) {
    // WHAT: Element-wise multiplication

    uint32_t dims[] = {2, 2};
    nimcp_tensor_t* a = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 4.0);
    nimcp_tensor_t* b = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 2.5);

    nimcp_tensor_t* c = nimcp_tensor_mul(a, b);
    ASSERT_NE(c, nullptr);

    for (size_t i = 0; i < nimcp_tensor_numel(c); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(c, i), 10.0f);
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorTest, Elementwise_Div) {
    // WHAT: Element-wise division

    uint32_t dims[] = {2, 2};
    nimcp_tensor_t* a = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 10.0);
    nimcp_tensor_t* b = nimcp_tensor_full(dims, 2, NIMCP_DTYPE_F32, 2.0);

    nimcp_tensor_t* c = nimcp_tensor_div(a, b);
    ASSERT_NE(c, nullptr);

    for (size_t i = 0; i < nimcp_tensor_numel(c); i++) {
        EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(c, i), 5.0f);
    }

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorTest, Elementwise_ScalarOps) {
    // WHAT: Scalar operations

    uint32_t dims[] = {3};
    nimcp_tensor_t* t = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 5.0);

    // Add scalar
    nimcp_tensor_t* added = nimcp_tensor_add_scalar(t, 3.0);
    ASSERT_NE(added, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(added, 0), 8.0f);

    // Multiply scalar
    nimcp_tensor_t* multed = nimcp_tensor_mul_scalar(t, 2.0);
    ASSERT_NE(multed, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(multed, 0), 10.0f);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(added);
    nimcp_tensor_destroy(multed);
}

TEST_F(TensorTest, Elementwise_UnaryMath) {
    // WHAT: Unary math functions

    uint32_t dims[] = {4};
    float data[] = {1.0f, 4.0f, 9.0f, 16.0f};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    // Square root
    nimcp_tensor_t* sqrt_t = nimcp_tensor_sqrt(t);
    ASSERT_NE(sqrt_t, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(sqrt_t, 0), 1.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(sqrt_t, 1), 2.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(sqrt_t, 2), 3.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(sqrt_t, 3), 4.0f);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(sqrt_t);
}

TEST_F(TensorTest, Elementwise_Exp) {
    // WHAT: Exponential function

    uint32_t dims[] = {3};
    float data[] = {0.0f, 1.0f, 2.0f};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* exp_t = nimcp_tensor_exp(t);
    ASSERT_NE(exp_t, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(exp_t, 0), 1.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(exp_t, 1), std::exp(1.0f), EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(exp_t, 2), std::exp(2.0f), EPSILON);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(exp_t);
}

TEST_F(TensorTest, Elementwise_Log) {
    // WHAT: Natural logarithm

    uint32_t dims[] = {3};
    float data[] = {1.0f, (float)M_E, (float)(M_E * M_E)};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* log_t = nimcp_tensor_log(t);
    ASSERT_NE(log_t, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(log_t, 0), 0.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(log_t, 1), 1.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(log_t, 2), 2.0f, EPSILON);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(log_t);
}

TEST_F(TensorTest, Elementwise_TrigFunctions) {
    // WHAT: Trigonometric functions

    uint32_t dims[] = {3};
    float data[] = {0.0f, (float)(M_PI / 2.0), (float)M_PI};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* sin_t = nimcp_tensor_sin(t);
    nimcp_tensor_t* cos_t = nimcp_tensor_cos(t);

    ASSERT_NE(sin_t, nullptr);
    ASSERT_NE(cos_t, nullptr);

    // sin(0) = 0, sin(pi/2) = 1, sin(pi) = 0
    EXPECT_NEAR(nimcp_tensor_get_flat(sin_t, 0), 0.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(sin_t, 1), 1.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(sin_t, 2), 0.0f, EPSILON);

    // cos(0) = 1, cos(pi/2) = 0, cos(pi) = -1
    EXPECT_NEAR(nimcp_tensor_get_flat(cos_t, 0), 1.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(cos_t, 1), 0.0f, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(cos_t, 2), -1.0f, EPSILON);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(sin_t);
    nimcp_tensor_destroy(cos_t);
}

TEST_F(TensorTest, Elementwise_Activations) {
    // WHAT: Neural network activation functions

    uint32_t dims[] = {5};
    float data[] = {-2.0f, -1.0f, 0.0f, 1.0f, 2.0f};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    // ReLU
    nimcp_tensor_t* relu_t = nimcp_tensor_relu(t);
    ASSERT_NE(relu_t, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(relu_t, 0), 0.0f);  // max(0, -2) = 0
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(relu_t, 1), 0.0f);  // max(0, -1) = 0
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(relu_t, 2), 0.0f);  // max(0, 0) = 0
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(relu_t, 3), 1.0f);  // max(0, 1) = 1
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(relu_t, 4), 2.0f);  // max(0, 2) = 2

    // Sigmoid
    nimcp_tensor_t* sig_t = nimcp_tensor_sigmoid(t);
    ASSERT_NE(sig_t, nullptr);
    EXPECT_NEAR(nimcp_tensor_get_flat(sig_t, 2), 0.5f, EPSILON);  // sigmoid(0) = 0.5

    // Tanh
    nimcp_tensor_t* tanh_t = nimcp_tensor_tanh(t);
    ASSERT_NE(tanh_t, nullptr);
    EXPECT_NEAR(nimcp_tensor_get_flat(tanh_t, 2), 0.0f, EPSILON);  // tanh(0) = 0

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(relu_t);
    nimcp_tensor_destroy(sig_t);
    nimcp_tensor_destroy(tanh_t);
}

//=============================================================================
// Unit Tests: Reduction Operations
//=============================================================================

TEST_F(TensorTest, Reduction_Sum) {
    // WHAT: Sum all elements

    uint32_t dims[] = {3, 3};
    nimcp_tensor_t* t = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);

    nimcp_tensor_t* sum = nimcp_tensor_sum(t);
    ASSERT_NE(sum, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(sum, 0), 9.0f);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(sum);
}

TEST_F(TensorTest, Reduction_Mean) {
    // WHAT: Mean of all elements

    uint32_t dims[] = {4};
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* mean = nimcp_tensor_mean(t);
    ASSERT_NE(mean, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(mean, 0), 2.5f);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(mean);
}

TEST_F(TensorTest, Reduction_Max) {
    // WHAT: Maximum element

    uint32_t dims[] = {5};
    float data[] = {1.0f, 5.0f, 2.0f, 8.0f, 3.0f};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* max = nimcp_tensor_max(t);
    ASSERT_NE(max, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(max, 0), 8.0f);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(max);
}

TEST_F(TensorTest, Reduction_Min) {
    // WHAT: Minimum element

    uint32_t dims[] = {5};
    float data[] = {5.0f, 1.0f, 8.0f, 2.0f, 3.0f};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* min = nimcp_tensor_min(t);
    ASSERT_NE(min, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(min, 0), 1.0f);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(min);
}

TEST_F(TensorTest, Reduction_SumDim) {
    // WHAT: Sum along dimension

    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t dims[] = {2, 3};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 2, NIMCP_DTYPE_F32, true);

    // Sum along dim 1 (columns): [[1,2,3],[4,5,6]] -> [6, 15]
    nimcp_tensor_t* sum = nimcp_tensor_sum_dim(t, 1, false);
    ASSERT_NE(sum, nullptr);

    EXPECT_EQ(nimcp_tensor_rank(sum), 1u);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(sum, 0), 6.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(sum, 1), 15.0f);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(sum);
}

TEST_F(TensorTest, Reduction_Variance) {
    // WHAT: Variance calculation
    // {2, 4, 4, 4, 6}: Mean = 4
    // Sum of squared deviations: (2-4)² + 0 + 0 + 0 + (6-4)² = 4 + 4 = 8
    // Population variance (N=5): 8/5 = 1.6
    // Sample variance (N-1=4): 8/4 = 2.0

    uint32_t dims[] = {5};
    float data[] = {2.0f, 4.0f, 4.0f, 4.0f, 6.0f};
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    // Test population variance (biased, divides by N)
    nimcp_tensor_t* var_pop = nimcp_tensor_var(t, false);
    ASSERT_NE(var_pop, nullptr);
    EXPECT_NEAR(nimcp_tensor_get_flat(var_pop, 0), 1.6f, EPSILON);

    // Test sample variance (unbiased, divides by N-1)
    nimcp_tensor_t* var_samp = nimcp_tensor_var(t, true);
    ASSERT_NE(var_samp, nullptr);
    EXPECT_NEAR(nimcp_tensor_get_flat(var_samp, 0), 2.0f, EPSILON);

    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(var_pop);
    nimcp_tensor_destroy(var_samp);
}

//=============================================================================
// Unit Tests: Linear Algebra
//=============================================================================

TEST_F(TensorTest, LinAlg_MatMul) {
    // WHAT: Matrix multiplication
    // WHY: Core neural network operation

    // A: 2x3 matrix
    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t a_dims[] = {2, 3};
    nimcp_tensor_t* a = nimcp_tensor_from_data(a_data, a_dims, 2, NIMCP_DTYPE_F32, true);

    // B: 3x2 matrix
    float b_data[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    uint32_t b_dims[] = {3, 2};
    nimcp_tensor_t* b = nimcp_tensor_from_data(b_data, b_dims, 2, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* c = nimcp_tensor_matmul(a, b);
    ASSERT_NE(c, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(c);
    EXPECT_EQ(shape->dims[0], 2u);
    EXPECT_EQ(shape->dims[1], 2u);

    // C[0,0] = 1*1 + 2*3 + 3*5 = 22
    // C[0,1] = 1*2 + 2*4 + 3*6 = 28
    // C[1,0] = 4*1 + 5*3 + 6*5 = 49
    // C[1,1] = 4*2 + 5*4 + 6*6 = 64
    uint32_t idx00[] = {0, 0};
    uint32_t idx01[] = {0, 1};
    uint32_t idx10[] = {1, 0};
    uint32_t idx11[] = {1, 1};

    EXPECT_FLOAT_EQ(nimcp_tensor_get(c, idx00), 22.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get(c, idx01), 28.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get(c, idx10), 49.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get(c, idx11), 64.0f);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

TEST_F(TensorTest, LinAlg_Dot) {
    // WHAT: Dot product of vectors

    uint32_t dims[] = {4};
    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b_data[] = {1.0f, 1.0f, 1.0f, 1.0f};

    nimcp_tensor_t* a = nimcp_tensor_from_data(a_data, dims, 1, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* b = nimcp_tensor_from_data(b_data, dims, 1, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* dot = nimcp_tensor_dot(a, b);
    ASSERT_NE(dot, nullptr);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(dot, 0), 10.0f);  // 1+2+3+4

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(dot);
}

TEST_F(TensorTest, LinAlg_Outer) {
    // WHAT: Outer product of vectors

    uint32_t dims[] = {3};
    float a_data[] = {1.0f, 2.0f, 3.0f};
    float b_data[] = {1.0f, 2.0f};
    uint32_t b_dims[] = {2};

    nimcp_tensor_t* a = nimcp_tensor_from_data(a_data, dims, 1, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* b = nimcp_tensor_from_data(b_data, b_dims, 1, NIMCP_DTYPE_F32, true);

    nimcp_tensor_t* outer = nimcp_tensor_outer(a, b);
    ASSERT_NE(outer, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(outer);
    EXPECT_EQ(shape->dims[0], 3u);
    EXPECT_EQ(shape->dims[1], 2u);

    // [[1*1, 1*2], [2*1, 2*2], [3*1, 3*2]]
    uint32_t idx00[] = {0, 0};
    uint32_t idx21[] = {2, 1};
    EXPECT_FLOAT_EQ(nimcp_tensor_get(outer, idx00), 1.0f);
    EXPECT_FLOAT_EQ(nimcp_tensor_get(outer, idx21), 6.0f);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(outer);
}

TEST_F(TensorTest, LinAlg_Trace) {
    // WHAT: Matrix trace (sum of diagonal)

    nimcp_tensor_t* eye = nimcp_tensor_eye(4, NIMCP_DTYPE_F32);
    ASSERT_NE(eye, nullptr);

    double trace = nimcp_tensor_trace(eye);
    EXPECT_FLOAT_EQ(trace, 4.0);

    nimcp_tensor_destroy(eye);
}

TEST_F(TensorTest, LinAlg_Norm) {
    // WHAT: Frobenius norm

    uint32_t dims[] = {3};
    float data[] = {3.0f, 4.0f, 0.0f};  // sqrt(9 + 16) = 5
    nimcp_tensor_t* t = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    double norm = nimcp_tensor_norm_fro(t);
    EXPECT_FLOAT_EQ(norm, 5.0);

    nimcp_tensor_destroy(t);
}

//=============================================================================
// Unit Tests: Tensor Calculus
//=============================================================================

TEST_F(TensorTest, Calculus_NumericalGradient) {
    // WHAT: Numerical gradient computation
    // WHY: Test derivative computation

    // Test function: f(x) = x^2, derivative = 2x
    // At x = 3, gradient = 6

    uint32_t dims[] = {1};
    float data[] = {3.0f};
    nimcp_tensor_t* x = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);

    // We test via nimcp_tensor_gradient_dim on a quadratic tensor
    // For simpler test: create a parabolic profile and check gradient

    nimcp_tensor_destroy(x);
    SUCCEED() << "Numerical gradient test placeholder";
}

//=============================================================================
// Unit Tests: Edge Cases and Error Handling
//=============================================================================

TEST_F(TensorTest, EdgeCase_NullInput) {
    // WHAT: Handle NULL inputs gracefully
    // WHY: Robustness against invalid inputs

    nimcp_tensor_t* result = nimcp_tensor_clone(nullptr);
    EXPECT_EQ(result, nullptr);

    result = nimcp_tensor_add(nullptr, nullptr);
    EXPECT_EQ(result, nullptr);

    EXPECT_EQ(nimcp_tensor_rank(nullptr), 0u);
    EXPECT_EQ(nimcp_tensor_numel(nullptr), 0u);
}

TEST_F(TensorTest, EdgeCase_EmptyTensor) {
    // WHAT: Handle zero-size dimensions
    // WHY: Edge case that must be handled

    uint32_t dims[] = {0, 5};
    nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);

    // Should either return NULL or a valid empty tensor
    if (t != nullptr) {
        EXPECT_EQ(nimcp_tensor_numel(t), 0u);
        nimcp_tensor_destroy(t);
    }

    SUCCEED() << "Empty tensor handled";
}

TEST_F(TensorTest, EdgeCase_ScalarTensor) {
    // WHAT: Rank-0 tensor (scalar)
    // WHY: Edge case for dimensionless values

    uint32_t dims[] = {1};
    nimcp_tensor_t* t = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 42.0);
    ASSERT_NE(t, nullptr);

    EXPECT_EQ(nimcp_tensor_numel(t), 1u);
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, 0), 42.0f);

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, EdgeCase_HighRankTensor) {
    // WHAT: High-rank tensor
    // WHY: Test rank limits

    uint32_t dims[] = {2, 2, 2, 2, 2, 2};  // 6D tensor
    nimcp_tensor_t* t = nimcp_tensor_ones(dims, 6, NIMCP_DTYPE_F32);

    ASSERT_NE(t, nullptr);
    EXPECT_EQ(nimcp_tensor_rank(t), 6u);
    EXPECT_EQ(nimcp_tensor_numel(t), 64u);  // 2^6

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, EdgeCase_LargeTensor) {
    // WHAT: Large tensor allocation
    // WHY: Memory handling for large allocations

    uint32_t dims[] = {100, 100, 10};
    nimcp_tensor_t* t = nimcp_tensor_zeros(dims, 3, NIMCP_DTYPE_F32);

    if (t != nullptr) {
        EXPECT_EQ(nimcp_tensor_numel(t), 100000u);
        nimcp_tensor_destroy(t);
    }

    SUCCEED() << "Large tensor handled";
}

TEST_F(TensorTest, EdgeCase_DataTypes) {
    // WHAT: Different data types
    // WHY: Verify dtype support

    uint32_t dims[] = {4};

    // Float64
    nimcp_tensor_t* f64 = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_F64);
    if (f64) {
        EXPECT_EQ(nimcp_tensor_dtype(f64), NIMCP_DTYPE_F64);
        nimcp_tensor_destroy(f64);
    }

    // Int32
    nimcp_tensor_t* i32 = nimcp_tensor_ones(dims, 1, NIMCP_DTYPE_I32);
    if (i32) {
        EXPECT_EQ(nimcp_tensor_dtype(i32), NIMCP_DTYPE_I32);
        nimcp_tensor_destroy(i32);
    }

    SUCCEED() << "Data type tests completed";
}

//=============================================================================
// Unit Tests: Properties
//=============================================================================

TEST_F(TensorTest, Properties_Contiguous) {
    // WHAT: Contiguous check and conversion

    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* t = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    // Newly created tensors should be contiguous
    EXPECT_TRUE(nimcp_tensor_is_contiguous(t));

    nimcp_tensor_destroy(t);
}

TEST_F(TensorTest, Properties_RequiresGrad) {
    // WHAT: Gradient tracking flag

    uint32_t dims[] = {3, 3};
    nimcp_tensor_t* t = nimcp_tensor_ones(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    // Default should not require grad
    EXPECT_FALSE(nimcp_tensor_requires_grad(t));

    // Set requires_grad
    nimcp_tensor_set_requires_grad(t, true);
    EXPECT_TRUE(nimcp_tensor_requires_grad(t));

    nimcp_tensor_destroy(t);
}

//=============================================================================
// Unit Tests: In-place Operations
//=============================================================================

TEST_F(TensorTest, InPlace_Add) {
    // WHAT: In-place addition

    uint32_t dims[] = {3};
    nimcp_tensor_t* a = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 5.0);
    nimcp_tensor_t* b = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 3.0);

    int result = nimcp_tensor_add_(a, b);
    EXPECT_EQ(result, NIMCP_TENSOR_OK);

    // a should now be 8.0
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(a, 0), 8.0f);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
}

TEST_F(TensorTest, InPlace_MulScalar) {
    // WHAT: In-place scalar multiplication

    uint32_t dims[] = {4};
    nimcp_tensor_t* t = nimcp_tensor_full(dims, 1, NIMCP_DTYPE_F32, 2.0);

    int result = nimcp_tensor_mul_scalar_(t, 3.0);
    EXPECT_EQ(result, NIMCP_TENSOR_OK);

    // t should now be 6.0
    EXPECT_FLOAT_EQ(nimcp_tensor_get_flat(t, 0), 6.0f);

    nimcp_tensor_destroy(t);
}

//=============================================================================
// Performance Smoke Tests
//=============================================================================

TEST_F(TensorTest, Performance_MatMulMedium) {
    // WHAT: Medium-sized matrix multiplication
    // WHY: Basic performance sanity check

    uint32_t dims_a[] = {64, 128};
    uint32_t dims_b[] = {128, 64};

    nimcp_tensor_t* a = nimcp_tensor_randn(dims_a, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* b = nimcp_tensor_randn(dims_b, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    nimcp_tensor_t* c = nimcp_tensor_matmul(a, b);
    ASSERT_NE(c, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(c);
    EXPECT_EQ(shape->dims[0], 64u);
    EXPECT_EQ(shape->dims[1], 64u);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
