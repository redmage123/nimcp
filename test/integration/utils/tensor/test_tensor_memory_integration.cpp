//=============================================================================
// test_tensor_memory_integration.cpp - Integration Tests for Tensor Memory
//=============================================================================
/**
 * @file test_tensor_memory_integration.cpp
 * @brief Integration tests for tensor module with memory subsystem
 *
 * WHAT: Test tensor operations with memory management
 * WHY:  Ensure tensors work correctly with NIMCP memory pools
 * HOW:  Create large tensors, chain operations, verify memory accounting
 *
 * TEST COVERAGE:
 * 1. Memory pool integration
 * 2. Large tensor operations
 * 3. Chained operations (gradient descent)
 * 4. Neural network pipeline
 * 5. Memory cleanup
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
    #include "utils/tensor/nimcp_tensor.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class TensorIntegrationTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_tensor_init();
        nimcp_tensor_reset_stats();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }
};

//=============================================================================
// Integration Tests: Memory Management
//=============================================================================

TEST_F(TensorIntegrationTest, MemoryAccounting) {
    // WHAT: Verify memory is properly tracked
    // WHY:  Prevent memory leaks in production

    nimcp_tensor_stats_t stats_before, stats_after;
    nimcp_tensor_get_stats(&stats_before);

    // Create multiple tensors
    std::vector<nimcp_tensor_t*> tensors;
    for (int i = 0; i < 10; i++) {
        uint32_t dims[] = {100, 100};
        tensors.push_back(nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0));
    }

    nimcp_tensor_get_stats(&stats_after);
    EXPECT_EQ(stats_after.tensors_created, stats_before.tensors_created + 10);
    EXPECT_GT(stats_after.memory_current, stats_before.memory_current);

    // Destroy tensors
    for (auto t : tensors) {
        nimcp_tensor_destroy(t);
    }

    nimcp_tensor_get_stats(&stats_after);
    EXPECT_EQ(stats_after.tensors_destroyed, stats_after.tensors_created);
}

TEST_F(TensorIntegrationTest, LargeTensorOperations) {
    // WHAT: Operations on large tensors
    // WHY:  Verify scaling behavior

    // Create large tensors
    uint32_t dims[] = {256, 256};
    nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Matrix multiplication
    nimcp_tensor_t* c = nimcp_tensor_matmul(a, b);
    ASSERT_NE(c, nullptr);

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(c);
    EXPECT_EQ(shape->dims[0], 256u);
    EXPECT_EQ(shape->dims[1], 256u);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
}

//=============================================================================
// Integration Tests: Neural Network Pipeline
//=============================================================================

TEST_F(TensorIntegrationTest, SimpleForwardPass) {
    // WHAT: Simulate neural network forward pass
    // WHY:  Realistic use case

    // Input: batch=4, features=8
    uint32_t x_dims[] = {4, 8};
    nimcp_tensor_t* x = nimcp_tensor_randn(x_dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    ASSERT_NE(x, nullptr);

    // Weight: features_in=8, features_out=16
    uint32_t w1_dims[] = {8, 16};
    nimcp_tensor_t* w1 = nimcp_tensor_randn(w1_dims, 2, NIMCP_DTYPE_F32, 0.0, 0.1);
    ASSERT_NE(w1, nullptr);

    // Forward: h1 = relu(x @ w1)
    nimcp_tensor_t* z1 = nimcp_tensor_matmul(x, w1);
    ASSERT_NE(z1, nullptr);

    nimcp_tensor_t* h1 = nimcp_tensor_relu(z1);
    ASSERT_NE(h1, nullptr);

    // Verify shape
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(h1);
    EXPECT_EQ(shape->dims[0], 4u);  // batch
    EXPECT_EQ(shape->dims[1], 16u); // features_out

    // Second layer
    uint32_t w2_dims[] = {16, 4};
    nimcp_tensor_t* w2 = nimcp_tensor_randn(w2_dims, 2, NIMCP_DTYPE_F32, 0.0, 0.1);
    ASSERT_NE(w2, nullptr);

    nimcp_tensor_t* z2 = nimcp_tensor_matmul(h1, w2);
    ASSERT_NE(z2, nullptr);

    nimcp_tensor_t* output = nimcp_tensor_softmax(z2, -1);
    ASSERT_NE(output, nullptr);

    // Verify softmax sums to 1
    const nimcp_tensor_shape_t* out_shape = nimcp_tensor_shape(output);
    EXPECT_EQ(out_shape->dims[0], 4u);  // batch
    EXPECT_EQ(out_shape->dims[1], 4u);  // num_classes

    // Check row sums
    float* data = (float*)nimcp_tensor_data(output);
    for (uint32_t i = 0; i < 4; i++) {
        float sum = 0.0f;
        for (uint32_t j = 0; j < 4; j++) {
            sum += data[i * 4 + j];
        }
        EXPECT_NEAR(sum, 1.0f, EPSILON) << "Row " << i << " should sum to 1";
    }

    // Cleanup
    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(w1);
    nimcp_tensor_destroy(z1);
    nimcp_tensor_destroy(h1);
    nimcp_tensor_destroy(w2);
    nimcp_tensor_destroy(z2);
    nimcp_tensor_destroy(output);
}

TEST_F(TensorIntegrationTest, GradientDescentStep) {
    // WHAT: Simulate one gradient descent step
    // WHY:  Core training operation

    // Parameters
    uint32_t dims[] = {4, 4};
    nimcp_tensor_t* params = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    ASSERT_NE(params, nullptr);

    // Gradient
    nimcp_tensor_t* grad = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 0.1);
    ASSERT_NE(grad, nullptr);

    // Learning rate * gradient
    double lr = 0.01;
    nimcp_tensor_t* scaled_grad = nimcp_tensor_mul_scalar(grad, lr);
    ASSERT_NE(scaled_grad, nullptr);

    // Update: params = params - lr * grad
    nimcp_tensor_t* new_params = nimcp_tensor_sub(params, scaled_grad);
    ASSERT_NE(new_params, nullptr);

    // Verify params changed
    bool changed = false;
    for (size_t i = 0; i < 16; i++) {
        double old_val = nimcp_tensor_get_flat(params, i);
        double new_val = nimcp_tensor_get_flat(new_params, i);
        if (std::abs(old_val - new_val) > EPSILON) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed) << "Parameters should change after gradient step";

    nimcp_tensor_destroy(params);
    nimcp_tensor_destroy(grad);
    nimcp_tensor_destroy(scaled_grad);
    nimcp_tensor_destroy(new_params);
}

//=============================================================================
// Integration Tests: Tensor Calculus Pipeline
//=============================================================================

TEST_F(TensorIntegrationTest, NumericalGradientCheck) {
    // WHAT: Verify numerical gradient computation
    // WHY:  Used for gradient checking in ML

    // Create input
    uint32_t dims[] = {4};
    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_tensor_t* x = nimcp_tensor_from_data(data, dims, 1, NIMCP_DTYPE_F32, true);
    ASSERT_NE(x, nullptr);

    // Simple quadratic function: f(x) = sum(x^2)
    // Gradient: df/dx_i = 2 * x_i
    auto f = [](const nimcp_tensor_t* t, void* ctx) -> double {
        (void)ctx;
        double sum = 0.0;
        for (size_t i = 0; i < nimcp_tensor_numel(t); i++) {
            double v = nimcp_tensor_get_flat(t, i);
            sum += v * v;
        }
        return sum;
    };

    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(f, x, 1e-5, nullptr);
    ASSERT_NE(grad, nullptr);

    // Verify gradient: should be approximately 2*x
    // Use 1e-2 tolerance due to numerical derivative approximation
    for (size_t i = 0; i < 4; i++) {
        double expected = 2.0 * nimcp_tensor_get_flat(x, i);
        double actual = nimcp_tensor_get_flat(grad, i);
        EXPECT_NEAR(actual, expected, 0.02) << "Gradient at index " << i;
    }

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(grad);
}

TEST_F(TensorIntegrationTest, LaplacianHeatEquation) {
    // WHAT: Compute Laplacian for heat equation
    // WHY:  Scientific computing use case

    // Create 2D temperature field
    uint32_t dims[] = {16, 16};
    nimcp_tensor_t* temp = nimcp_tensor_zeros(dims, 2, NIMCP_DTYPE_F32);
    ASSERT_NE(temp, nullptr);

    // Set initial hot spot in center - use Gaussian-like distribution
    // to create smooth gradients (discrete Laplacian needs non-uniform values)
    for (uint32_t i = 0; i < 16; i++) {
        for (uint32_t j = 0; j < 16; j++) {
            // Distance from center (8, 8)
            double di = (double)i - 8.0;
            double dj = (double)j - 8.0;
            double dist_sq = di * di + dj * dj;
            // Gaussian temperature distribution
            double val = 100.0 * exp(-dist_sq / 8.0);
            uint32_t idx[] = {i, j};
            nimcp_tensor_set(temp, idx, val);
        }
    }

    // Compute Laplacian
    double spacing[] = {1.0, 1.0};
    nimcp_tensor_t* lap = nimcp_tensor_laplacian(temp, spacing);
    ASSERT_NE(lap, nullptr);

    // For a Gaussian, center Laplacian should be negative (heat flows out from peak)
    // Laplacian of Gaussian is negative at the center
    uint32_t center[] = {8, 8};
    double center_lap = nimcp_tensor_get(lap, center);
    EXPECT_LT(center_lap, 0.0) << "Heat should flow out of hot spot (Gaussian peak)";

    nimcp_tensor_destroy(temp);
    nimcp_tensor_destroy(lap);
}

//=============================================================================
// Integration Tests: Chained Operations
//=============================================================================

TEST_F(TensorIntegrationTest, ChainedTransformations) {
    // WHAT: Multiple chained tensor operations
    // WHY:  Test operation composition

    uint32_t dims[] = {8, 8};
    nimcp_tensor_t* t = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    ASSERT_NE(t, nullptr);

    // Chain: t -> transpose -> abs -> exp -> log -> softmax
    nimcp_tensor_t* t1 = nimcp_tensor_transpose(t);
    ASSERT_NE(t1, nullptr);

    nimcp_tensor_t* t2 = nimcp_tensor_abs(t1);
    ASSERT_NE(t2, nullptr);

    nimcp_tensor_t* t3 = nimcp_tensor_add_scalar(t2, 0.01);  // Avoid log(0)
    ASSERT_NE(t3, nullptr);

    nimcp_tensor_t* t4 = nimcp_tensor_exp(t3);
    ASSERT_NE(t4, nullptr);

    nimcp_tensor_t* t5 = nimcp_tensor_log(t4);
    ASSERT_NE(t5, nullptr);

    nimcp_tensor_t* t6 = nimcp_tensor_softmax(t5, -1);
    ASSERT_NE(t6, nullptr);

    // Verify final result is valid softmax output
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(t6);
    EXPECT_EQ(shape->dims[0], 8u);
    EXPECT_EQ(shape->dims[1], 8u);

    // Cleanup all intermediates
    nimcp_tensor_destroy(t);
    nimcp_tensor_destroy(t1);
    nimcp_tensor_destroy(t2);
    nimcp_tensor_destroy(t3);
    nimcp_tensor_destroy(t4);
    nimcp_tensor_destroy(t5);
    nimcp_tensor_destroy(t6);
}

TEST_F(TensorIntegrationTest, AttentionMechanism) {
    // WHAT: Scaled dot-product attention
    // WHY:  Transformer building block

    // Q, K, V: (batch=1, seq=4, dim=8)
    uint32_t dims[] = {4, 8};
    nimcp_tensor_t* Q = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* K = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* V = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    ASSERT_NE(Q, nullptr);
    ASSERT_NE(K, nullptr);
    ASSERT_NE(V, nullptr);

    // Attention
    nimcp_tensor_t* attn = nimcp_tensor_attention(Q, K, V, nullptr, 0.0);
    ASSERT_NE(attn, nullptr);

    // Output should have same shape as V
    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(attn);
    EXPECT_EQ(shape->dims[0], 4u);  // seq
    EXPECT_EQ(shape->dims[1], 8u);  // dim

    nimcp_tensor_destroy(Q);
    nimcp_tensor_destroy(K);
    nimcp_tensor_destroy(V);
    nimcp_tensor_destroy(attn);
}

//=============================================================================
// Integration Tests: Statistics Collection
//=============================================================================

TEST_F(TensorIntegrationTest, StatisticsTracking) {
    // WHAT: Verify operation statistics
    // WHY:  Performance monitoring

    nimcp_tensor_reset_stats();

    // Perform various operations
    uint32_t dims[] = {16, 16};
    nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
    nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);

    // Element-wise
    nimcp_tensor_t* c = nimcp_tensor_add(a, b);

    // Matrix mult
    nimcp_tensor_t* d = nimcp_tensor_matmul(a, b);

    // Reduction
    nimcp_tensor_t* e = nimcp_tensor_sum(a);

    nimcp_tensor_stats_t stats;
    nimcp_tensor_get_stats(&stats);

    EXPECT_GE(stats.tensors_created, 5u);
    EXPECT_GE(stats.ops_elementwise, 1u);
    EXPECT_GE(stats.ops_matmul, 1u);
    EXPECT_GE(stats.ops_reduction, 1u);

    nimcp_tensor_destroy(a);
    nimcp_tensor_destroy(b);
    nimcp_tensor_destroy(c);
    nimcp_tensor_destroy(d);
    nimcp_tensor_destroy(e);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
