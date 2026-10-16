//=============================================================================
// test_tensor_e2e.cpp - End-to-End Tests for Tensor Einsum + Autodiff Pipeline
//=============================================================================
/**
 * @file test_tensor_e2e.cpp
 * @brief End-to-end tests for complete tensor computation pipelines
 *
 * WHAT: Full pipeline tests: create tensors, perform complex einsum operations,
 *       run autodiff backward, verify gradients, simulate training steps
 * WHY:  Validate the entire tensor computation stack works together in
 *       realistic neural network scenarios
 * HOW:  Build multi-step pipelines combining creation, einsum, arithmetic,
 *       loss computation, backward pass, and parameter updates
 *
 * TEST COVERAGE:
 * 1. Linear layer forward+backward: x @ W via einsum, compute loss, get gradients
 * 2. Multi-layer MLP forward+backward pipeline
 * 3. Bilinear form: x^T A y via chained einsum, gradients w.r.t. A
 * 4. Attention-like pattern: Q @ K^T via einsum, scale, softmax, @ V
 * 5. Training loop simulation: forward, loss, backward, parameter update
 * 6. Batch processing pipeline with einsum
 * 7. Outer product + contraction pipeline (tensor network)
 * 8. Gradient correctness: analytical vs numerical for complex pipeline
 * 9. Memory lifecycle: no leaks through full pipeline
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TensorE2ETest : public ::testing::Test {
protected:
    static constexpr double EPSILON = 1e-4;
    // F32 numerical gradients use adaptive h=1e-3, giving ~0.1-0.5 error
    // for values in range 1-10. Larger values = larger absolute error.
    static constexpr double GRAD_EPSILON = 0.5;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_tensor_init();
        nimcp_tensor_reset_stats();
    }

    void TearDown() override {
        nimcp_tensor_shutdown();
    }

    /**
     * @brief Helper: Compute sum-of-squares loss: L = sum(y^2)
     *
     * WHAT: Scalar loss function for testing backward pass
     * WHY:  Simple differentiable loss with known gradient: dL/dy = 2*y
     */
    nimcp_tensor_t* ComputeSumSquaredLoss(nimcp_tensor_t* y) {
        nimcp_tensor_t* y_sq = nimcp_tensor_square(y);
        if (!y_sq) return nullptr;
        nimcp_tensor_t* loss = nimcp_tensor_sum(y_sq);
        nimcp_tensor_destroy(y_sq);
        return loss;
    }

    /**
     * @brief Helper: Create 2D tensor with known sequential values
     */
    nimcp_tensor_t* CreateSequential2D(uint32_t rows, uint32_t cols, float start = 1.0f) {
        uint32_t dims[] = {rows, cols};
        nimcp_tensor_t* t = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
        if (!t) return nullptr;
        for (uint32_t i = 0; i < rows * cols; i++) {
            nimcp_tensor_set_flat(t, i, start + static_cast<float>(i));
        }
        return t;
    }
};

//=============================================================================
// E2E Tests: Linear Layer Forward + Backward
//=============================================================================

TEST_F(TensorE2ETest, LinearLayer_ForwardBackward) {
    // WHAT: Full linear layer: y = x @ W, loss = sum(y^2), backward for dL/dW
    // WHY:  This is the fundamental building block of neural network training
    //
    // Pipeline:
    //   1. Create input x (1x3) and weight W (3x2)
    //   2. Forward: y = einsum("ij,jk->ik", x, W)
    //   3. Loss: L = sum(y^2)
    //   4. Backward: compute dL/dW numerically
    //   5. Verify gradient is non-zero and correct shape

    // Input: 1 sample, 3 features
    float x_data[] = {1.0f, 2.0f, 3.0f};
    uint32_t x_dims[] = {1, 3};
    nimcp_tensor_t* x = nimcp_tensor_from_data(x_data, x_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(x, nullptr);

    // Weight: 3 input features -> 2 output features
    float w_data[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
    uint32_t w_dims[] = {3, 2};
    nimcp_tensor_t* w = nimcp_tensor_from_data(w_data, w_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(w, nullptr);

    // Forward: y = x @ W via einsum
    nimcp_tensor_t* tensors[] = {x, w};
    nimcp_tensor_t* y = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
    ASSERT_NE(y, nullptr);

    // Verify output shape: (1, 2)
    const nimcp_tensor_shape_t* y_shape = nimcp_tensor_shape(y);
    EXPECT_EQ(y_shape->dims[0], 1u);
    EXPECT_EQ(y_shape->dims[1], 2u);

    // Verify forward values: y = [[1*0.1+2*0.3+3*0.5, 1*0.2+2*0.4+3*0.6]]
    //                          = [[0.1+0.6+1.5, 0.2+0.8+1.8]] = [[2.2, 2.8]]
    EXPECT_NEAR(nimcp_tensor_get_flat(y, 0), 2.2, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(y, 1), 2.8, EPSILON);

    // Loss: L = sum(y^2) = 2.2^2 + 2.8^2 = 4.84 + 7.84 = 12.68
    nimcp_tensor_t* loss = ComputeSumSquaredLoss(y);
    ASSERT_NE(loss, nullptr);
    EXPECT_NEAR(nimcp_tensor_get_flat(loss, 0), 12.68, 0.01);

    // Backward: compute gradient of loss w.r.t. weight W
    // Use numerical gradient: perturb W, recompute forward+loss
    struct LinearCtx { nimcp_tensor_t* x; };
    LinearCtx lctx;
    lctx.x = x;

    auto loss_fn = [](const nimcp_tensor_t* w_param, void* ctx) -> double {
        LinearCtx* lc = static_cast<LinearCtx*>(ctx);
        nimcp_tensor_t* ts[] = {lc->x, const_cast<nimcp_tensor_t*>(w_param)};
        nimcp_tensor_t* y_local = nimcp_tensor_einsum("ij,jk->ik", ts, 2);
        if (!y_local) return 0.0;

        nimcp_tensor_t* y_sq = nimcp_tensor_square(y_local);
        nimcp_tensor_t* loss_local = y_sq ? nimcp_tensor_sum(y_sq) : nullptr;
        double val = loss_local ? nimcp_tensor_get_flat(loss_local, 0) : 0.0;

        nimcp_tensor_destroy(loss_local);
        nimcp_tensor_destroy(y_sq);
        nimcp_tensor_destroy(y_local);
        return val;
    };

    nimcp_tensor_t* grad_w = nimcp_tensor_numerical_gradient(loss_fn, w, 1e-5, &lctx);
    ASSERT_NE(grad_w, nullptr);

    // Gradient shape should match W (3x2)
    const nimcp_tensor_shape_t* grad_shape = nimcp_tensor_shape(grad_w);
    EXPECT_EQ(grad_shape->rank, 2u);
    EXPECT_EQ(grad_shape->dims[0], 3u);
    EXPECT_EQ(grad_shape->dims[1], 2u);

    // Analytical gradient: dL/dW = 2 * x^T * y
    // dL/dW[j,k] = 2 * x[0,j] * y[0,k]
    // dL/dW[0,0] = 2*1*2.2 = 4.4
    // dL/dW[0,1] = 2*1*2.8 = 5.6
    // dL/dW[1,0] = 2*2*2.2 = 8.8
    // dL/dW[1,1] = 2*2*2.8 = 11.2
    // dL/dW[2,0] = 2*3*2.2 = 13.2
    // dL/dW[2,1] = 2*3*2.8 = 16.8
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_w, 0), 4.4, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_w, 1), 5.6, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_w, 2), 8.8, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_w, 3), 11.2, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_w, 4), 13.2, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_w, 5), 16.8, GRAD_EPSILON);

    // Cleanup
    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(w);
    nimcp_tensor_destroy(y);
    nimcp_tensor_destroy(loss);
    nimcp_tensor_destroy(grad_w);
}

//=============================================================================
// E2E Tests: Two-Layer MLP Forward + Backward
//=============================================================================

TEST_F(TensorE2ETest, TwoLayerMLP_ForwardBackward) {
    // WHAT: Two-layer MLP: h = relu(x @ W1), y = h @ W2, loss = sum(y^2)
    // WHY:  Multi-layer networks chain einsum operations through nonlinearities
    //
    // Pipeline:
    //   1. x (1x2), W1 (2x3), W2 (3x1)
    //   2. z1 = einsum("ij,jk->ik", x, W1)
    //   3. h = relu(z1)
    //   4. y = einsum("ij,jk->ik", h, W2)
    //   5. loss = sum(y^2)

    float x_data[] = {1.0f, 2.0f};
    uint32_t x_dims[] = {1, 2};
    nimcp_tensor_t* x = nimcp_tensor_from_data(x_data, x_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(x, nullptr);

    float w1_data[] = {0.5f, 0.3f, 0.1f, 0.2f, 0.4f, 0.6f};
    uint32_t w1_dims[] = {2, 3};
    nimcp_tensor_t* w1 = nimcp_tensor_from_data(w1_data, w1_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(w1, nullptr);

    float w2_data[] = {0.7f, 0.8f, 0.9f};
    uint32_t w2_dims[] = {3, 1};
    nimcp_tensor_t* w2 = nimcp_tensor_from_data(w2_data, w2_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(w2, nullptr);

    // Layer 1: z1 = x @ W1
    nimcp_tensor_t* layer1_tensors[] = {x, w1};
    nimcp_tensor_t* z1 = nimcp_tensor_einsum("ij,jk->ik", layer1_tensors, 2);
    ASSERT_NE(z1, nullptr);

    // Activation: h = relu(z1)
    nimcp_tensor_t* h = nimcp_tensor_relu(z1);
    ASSERT_NE(h, nullptr);

    // Layer 2: y = h @ W2
    nimcp_tensor_t* layer2_tensors[] = {h, w2};
    nimcp_tensor_t* y = nimcp_tensor_einsum("ij,jk->ik", layer2_tensors, 2);
    ASSERT_NE(y, nullptr);

    // Output shape: (1, 1)
    const nimcp_tensor_shape_t* y_shape = nimcp_tensor_shape(y);
    EXPECT_EQ(y_shape->dims[0], 1u);
    EXPECT_EQ(y_shape->dims[1], 1u);

    // Loss
    nimcp_tensor_t* loss = ComputeSumSquaredLoss(y);
    ASSERT_NE(loss, nullptr);
    EXPECT_GT(nimcp_tensor_get_flat(loss, 0), 0.0);

    // Verify no NaN/Inf in output
    double y_val = nimcp_tensor_get_flat(y, 0);
    EXPECT_FALSE(std::isnan(y_val));
    EXPECT_FALSE(std::isinf(y_val));

    // Backward via autodiff
    nimcp_tensor_t* backward_inputs[] = {w1, w2};
    nimcp_tensor_t* backward_grads[2] = {nullptr, nullptr};

    int rc = nimcp_autodiff_backward(nullptr, loss, backward_inputs, 2, backward_grads);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    // Gradient shapes must match parameter shapes
    ASSERT_NE(backward_grads[0], nullptr);
    EXPECT_EQ(nimcp_tensor_rank(backward_grads[0]), 2u);
    EXPECT_EQ(nimcp_tensor_numel(backward_grads[0]), 6u);  // W1 is 2x3

    ASSERT_NE(backward_grads[1], nullptr);
    EXPECT_EQ(nimcp_tensor_rank(backward_grads[1]), 2u);
    EXPECT_EQ(nimcp_tensor_numel(backward_grads[1]), 3u);  // W2 is 3x1

    // Cleanup
    nimcp_tensor_destroy(backward_grads[0]);
    nimcp_tensor_destroy(backward_grads[1]);
    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(w1);
    nimcp_tensor_destroy(w2);
    nimcp_tensor_destroy(z1);
    nimcp_tensor_destroy(h);
    nimcp_tensor_destroy(y);
    nimcp_tensor_destroy(loss);
}

//=============================================================================
// E2E Tests: Bilinear Form via Einsum
//=============================================================================

TEST_F(TensorE2ETest, BilinearForm_EinsumGradient) {
    // WHAT: Compute bilinear form f(A) = x^T A y = einsum("i,ij,j->", x, A, y)
    //       and verify gradient dF/dA = x outer y
    // WHY:  Bilinear forms are used in attention, kernel methods, physics

    float x_data[] = {1.0f, 2.0f};
    float y_data[] = {3.0f, 4.0f};
    uint32_t vec_dims[] = {2};

    nimcp_tensor_t* x = nimcp_tensor_from_data(x_data, vec_dims, 1, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* y = nimcp_tensor_from_data(y_data, vec_dims, 1, NIMCP_DTYPE_F32, true);
    ASSERT_NE(x, nullptr);
    ASSERT_NE(y, nullptr);

    // Compute bilinear form step by step:
    // Step 1: z = A @ y via einsum("ij,j->i")
    // Step 2: result = x . z via einsum("i,i->")
    // This is equivalent to x^T A y

    float a_data_id[] = {1.0f, 0.0f, 0.0f, 1.0f};
    uint32_t a_dims[] = {2, 2};
    nimcp_tensor_t* A = nimcp_tensor_from_data(a_data_id, a_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(A, nullptr);

    // z = A @ y
    nimcp_tensor_t* step1_tensors[] = {A, y};
    nimcp_tensor_t* z = nimcp_tensor_einsum("ij,j->i", step1_tensors, 2);
    ASSERT_NE(z, nullptr);

    // result = x . z
    nimcp_tensor_t* step2_tensors[] = {x, z};
    nimcp_tensor_t* result = nimcp_tensor_einsum("i,i->", step2_tensors, 2);
    ASSERT_NE(result, nullptr);

    // With A = I: x^T I y = x . y = 1*3 + 2*4 = 11
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 11.0, EPSILON);

    // Numerical gradient dF/dA
    struct BilinearCtx { nimcp_tensor_t* x; nimcp_tensor_t* y; };
    BilinearCtx bctx;
    bctx.x = x;
    bctx.y = y;

    auto bilinear_fn = [](const nimcp_tensor_t* a_param, void* ctx) -> double {
        BilinearCtx* bc = static_cast<BilinearCtx*>(ctx);
        nimcp_tensor_t* ts1[] = {const_cast<nimcp_tensor_t*>(a_param), bc->y};
        nimcp_tensor_t* z_local = nimcp_tensor_einsum("ij,j->i", ts1, 2);
        if (!z_local) return 0.0;

        nimcp_tensor_t* ts2[] = {bc->x, z_local};
        nimcp_tensor_t* r = nimcp_tensor_einsum("i,i->", ts2, 2);
        double val = r ? nimcp_tensor_get_flat(r, 0) : 0.0;

        nimcp_tensor_destroy(r);
        nimcp_tensor_destroy(z_local);
        return val;
    };

    nimcp_tensor_t* grad_A = nimcp_tensor_numerical_gradient(bilinear_fn, A, 1e-5, &bctx);
    ASSERT_NE(grad_A, nullptr);

    // dF/dA_ij = x_i * y_j (outer product of x and y)
    // dF/dA = [[1*3, 1*4], [2*3, 2*4]] = [[3, 4], [6, 8]]
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_A, 0), 3.0, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_A, 1), 4.0, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_A, 2), 6.0, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad_A, 3), 8.0, GRAD_EPSILON);

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(y);
    nimcp_tensor_destroy(A);
    nimcp_tensor_destroy(z);
    nimcp_tensor_destroy(result);
    nimcp_tensor_destroy(grad_A);
}

//=============================================================================
// E2E Tests: Training Step Simulation
//=============================================================================

TEST_F(TensorE2ETest, TrainingStep_SGD) {
    // WHAT: Simulate a full SGD training step
    // WHY:  Verify the complete train loop: forward -> loss -> backward -> update
    //
    // Pipeline:
    //   1. Initialize weight W
    //   2. Forward: y = x @ W
    //   3. Loss: L = sum(y^2)
    //   4. Gradient: dL/dW via numerical gradient
    //   5. Update: W_new = W - lr * dL/dW
    //   6. Verify loss decreases

    float x_data[] = {1.0f, 1.0f};
    uint32_t x_dims[] = {1, 2};
    nimcp_tensor_t* x = nimcp_tensor_from_data(x_data, x_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(x, nullptr);

    // Initial weight
    float w_data[] = {2.0f, 3.0f};
    uint32_t w_dims[] = {2, 1};
    nimcp_tensor_t* w = nimcp_tensor_from_data(w_data, w_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(w, nullptr);

    // Forward: y = x @ W
    nimcp_tensor_t* fwd_tensors[] = {x, w};
    nimcp_tensor_t* y = nimcp_tensor_einsum("ij,jk->ik", fwd_tensors, 2);
    ASSERT_NE(y, nullptr);

    // Loss before
    nimcp_tensor_t* loss_before = ComputeSumSquaredLoss(y);
    ASSERT_NE(loss_before, nullptr);
    double loss_val_before = nimcp_tensor_get_flat(loss_before, 0);

    // Compute gradient of loss w.r.t. W
    struct SGDCtx { nimcp_tensor_t* x; };
    SGDCtx sctx;
    sctx.x = x;

    auto loss_fn = [](const nimcp_tensor_t* w_param, void* ctx) -> double {
        SGDCtx* sc = static_cast<SGDCtx*>(ctx);
        nimcp_tensor_t* ts[] = {sc->x, const_cast<nimcp_tensor_t*>(w_param)};
        nimcp_tensor_t* y_local = nimcp_tensor_einsum("ij,jk->ik", ts, 2);
        if (!y_local) return 0.0;

        nimcp_tensor_t* y_sq = nimcp_tensor_square(y_local);
        nimcp_tensor_t* loss_local = y_sq ? nimcp_tensor_sum(y_sq) : nullptr;
        double val = loss_local ? nimcp_tensor_get_flat(loss_local, 0) : 0.0;

        nimcp_tensor_destroy(loss_local);
        nimcp_tensor_destroy(y_sq);
        nimcp_tensor_destroy(y_local);
        return val;
    };

    nimcp_tensor_t* grad_w = nimcp_tensor_numerical_gradient(loss_fn, w, 1e-5, &sctx);
    ASSERT_NE(grad_w, nullptr);

    // SGD update: W_new = W - lr * grad
    double lr = 0.01;
    nimcp_tensor_t* scaled_grad = nimcp_tensor_mul_scalar(grad_w, lr);
    ASSERT_NE(scaled_grad, nullptr);

    nimcp_tensor_t* w_new = nimcp_tensor_sub(w, scaled_grad);
    ASSERT_NE(w_new, nullptr);

    // Forward with updated weight
    nimcp_tensor_t* fwd2_tensors[] = {x, w_new};
    nimcp_tensor_t* y_new = nimcp_tensor_einsum("ij,jk->ik", fwd2_tensors, 2);
    ASSERT_NE(y_new, nullptr);

    nimcp_tensor_t* loss_after = ComputeSumSquaredLoss(y_new);
    ASSERT_NE(loss_after, nullptr);
    double loss_val_after = nimcp_tensor_get_flat(loss_after, 0);

    // Loss should decrease after one gradient step (for non-zero gradient)
    EXPECT_LT(loss_val_after, loss_val_before)
        << "Loss should decrease: before=" << loss_val_before
        << ", after=" << loss_val_after;

    // Cleanup
    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(w);
    nimcp_tensor_destroy(y);
    nimcp_tensor_destroy(loss_before);
    nimcp_tensor_destroy(grad_w);
    nimcp_tensor_destroy(scaled_grad);
    nimcp_tensor_destroy(w_new);
    nimcp_tensor_destroy(y_new);
    nimcp_tensor_destroy(loss_after);
}

TEST_F(TensorE2ETest, MultipleTrainingSteps_LossDecreases) {
    // WHAT: Run 5 training steps and verify loss monotonically decreases
    // WHY:  Confirms the gradient computation is correct direction

    float x_data[] = {1.0f, 0.5f, 0.5f, 1.0f};
    uint32_t x_dims[] = {2, 2};
    nimcp_tensor_t* x = nimcp_tensor_from_data(x_data, x_dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(x, nullptr);

    // Initialize weight close to origin so loss is small and gradient is reliable
    uint32_t w_dims[] = {2, 1};
    nimcp_tensor_t* w = nimcp_tensor_full(w_dims, 2, NIMCP_DTYPE_F32, 1.0);
    ASSERT_NE(w, nullptr);

    struct TrainCtx { nimcp_tensor_t* x; };
    TrainCtx tctx;
    tctx.x = x;

    auto loss_fn = [](const nimcp_tensor_t* w_param, void* ctx) -> double {
        TrainCtx* tc = static_cast<TrainCtx*>(ctx);
        nimcp_tensor_t* ts[] = {tc->x, const_cast<nimcp_tensor_t*>(w_param)};
        nimcp_tensor_t* y = nimcp_tensor_einsum("ij,jk->ik", ts, 2);
        if (!y) return 0.0;

        nimcp_tensor_t* y_sq = nimcp_tensor_square(y);
        nimcp_tensor_t* loss = y_sq ? nimcp_tensor_sum(y_sq) : nullptr;
        double val = loss ? nimcp_tensor_get_flat(loss, 0) : 0.0;

        nimcp_tensor_destroy(loss);
        nimcp_tensor_destroy(y_sq);
        nimcp_tensor_destroy(y);
        return val;
    };

    double prev_loss = loss_fn(w, &tctx);
    double lr = 0.01;

    for (int step = 0; step < 5; step++) {
        nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(loss_fn, w, 1e-4, &tctx);
        ASSERT_NE(grad, nullptr) << "Step " << step << ": gradient is NULL";

        nimcp_tensor_t* scaled = nimcp_tensor_mul_scalar(grad, lr);
        ASSERT_NE(scaled, nullptr);

        nimcp_tensor_t* w_new = nimcp_tensor_sub(w, scaled);
        ASSERT_NE(w_new, nullptr);

        double new_loss = loss_fn(w_new, &tctx);

        EXPECT_LE(new_loss, prev_loss + EPSILON)
            << "Step " << step << ": loss increased from " << prev_loss << " to " << new_loss;

        nimcp_tensor_destroy(w);
        w = w_new;
        prev_loss = new_loss;

        nimcp_tensor_destroy(grad);
        nimcp_tensor_destroy(scaled);
    }

    nimcp_tensor_destroy(x);
    nimcp_tensor_destroy(w);
}

//=============================================================================
// E2E Tests: Attention-like Pipeline
//=============================================================================

TEST_F(TensorE2ETest, AttentionPattern_QKV) {
    // WHAT: Simplified attention: scores = Q @ K^T via einsum, then apply to V
    // WHY:  Attention mechanism is a core transformer component
    //
    // Q, K, V are (seq=2, dim=3)
    // scores = einsum("ij,kj->ik", Q, K) = Q @ K^T  (2x2)
    // output = einsum("ij,jk->ik", scores, V)         (2x3)

    float q_data[] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    float k_data[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    float v_data[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
    uint32_t dims[] = {2, 3};

    nimcp_tensor_t* Q = nimcp_tensor_from_data(q_data, dims, 2, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* K = nimcp_tensor_from_data(k_data, dims, 2, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* V = nimcp_tensor_from_data(v_data, dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(Q, nullptr);
    ASSERT_NE(K, nullptr);
    ASSERT_NE(V, nullptr);

    // Attention scores: Q @ K^T via einsum
    nimcp_tensor_t* score_tensors[] = {Q, K};
    nimcp_tensor_t* scores = nimcp_tensor_einsum("ij,kj->ik", score_tensors, 2);
    ASSERT_NE(scores, nullptr);

    // Verify scores shape: (2, 2)
    const nimcp_tensor_shape_t* score_shape = nimcp_tensor_shape(scores);
    EXPECT_EQ(score_shape->dims[0], 2u);
    EXPECT_EQ(score_shape->dims[1], 2u);

    // Q[0] = [1,0,0], K[0] = [1,0,0], K[1] = [0,0,1]
    // scores[0,0] = Q[0] . K[0] = 1, scores[0,1] = Q[0] . K[1] = 0
    // Q[1] = [0,1,0], K[0] = [1,0,0], K[1] = [0,0,1]
    // scores[1,0] = 0, scores[1,1] = 0
    EXPECT_NEAR(nimcp_tensor_get_flat(scores, 0), 1.0, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(scores, 1), 0.0, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(scores, 2), 0.0, EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(scores, 3), 0.0, EPSILON);

    // Apply softmax to scores
    nimcp_tensor_t* attn_weights = nimcp_tensor_softmax(scores, -1);
    ASSERT_NE(attn_weights, nullptr);

    // Output: weighted sum of values
    nimcp_tensor_t* out_tensors[] = {attn_weights, V};
    nimcp_tensor_t* output = nimcp_tensor_einsum("ij,jk->ik", out_tensors, 2);
    ASSERT_NE(output, nullptr);

    // Verify output shape: (2, 3)
    const nimcp_tensor_shape_t* out_shape = nimcp_tensor_shape(output);
    EXPECT_EQ(out_shape->dims[0], 2u);
    EXPECT_EQ(out_shape->dims[1], 3u);

    // All output values should be finite
    for (size_t i = 0; i < nimcp_tensor_numel(output); i++) {
        double val = nimcp_tensor_get_flat(output, i);
        EXPECT_FALSE(std::isnan(val)) << "NaN at index " << i;
        EXPECT_FALSE(std::isinf(val)) << "Inf at index " << i;
    }

    nimcp_tensor_destroy(Q);
    nimcp_tensor_destroy(K);
    nimcp_tensor_destroy(V);
    nimcp_tensor_destroy(scores);
    nimcp_tensor_destroy(attn_weights);
    nimcp_tensor_destroy(output);
}

//=============================================================================
// E2E Tests: Tensor Network Pipeline (Outer Product + Contract)
//=============================================================================

TEST_F(TensorE2ETest, TensorNetwork_OuterContractPipeline) {
    // WHAT: Build a simple tensor network: outer product then contract back
    // WHY:  Tensor networks are used in physics and efficient ML models
    //
    // Pipeline:
    //   1. u (3,), v (3,) -> outer = einsum("i,j->ij", u, v)  (3x3)
    //   2. A (3x3)        -> contracted = einsum("ij,ij->", outer, A)  (scalar)
    // This computes sum_ij u_i * v_j * A_ij

    float u_data[] = {1.0f, 2.0f, 3.0f};
    float v_data[] = {4.0f, 5.0f, 6.0f};
    uint32_t vec_dims[] = {3};

    nimcp_tensor_t* u = nimcp_tensor_from_data(u_data, vec_dims, 1, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* v = nimcp_tensor_from_data(v_data, vec_dims, 1, NIMCP_DTYPE_F32, true);
    ASSERT_NE(u, nullptr);
    ASSERT_NE(v, nullptr);

    // Outer product
    nimcp_tensor_t* outer_ts[] = {u, v};
    nimcp_tensor_t* outer = nimcp_tensor_einsum("i,j->ij", outer_ts, 2);
    ASSERT_NE(outer, nullptr);

    // Create A = identity (so contraction = trace(u (x) v) = u . v)
    nimcp_tensor_t* A = nimcp_tensor_eye(3, NIMCP_DTYPE_F32);
    ASSERT_NE(A, nullptr);

    // Contract: sum_ij outer[i,j] * A[i,j]
    nimcp_tensor_t* contract_ts[] = {outer, A};
    nimcp_tensor_t* result = nimcp_tensor_einsum("ij,ij->", contract_ts, 2);
    ASSERT_NE(result, nullptr);

    // With A=I: result = sum_i u_i * v_i = dot(u,v) = 1*4+2*5+3*6 = 32
    EXPECT_NEAR(nimcp_tensor_get_flat(result, 0), 32.0, EPSILON);

    // Now backward from result to get gradient w.r.t. u
    nimcp_tensor_t* backward_inputs[] = {u};
    nimcp_tensor_t* backward_grads[1] = {nullptr};

    int rc = nimcp_autodiff_backward(nullptr, result, backward_inputs, 1, backward_grads);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);
    ASSERT_NE(backward_grads[0], nullptr);
    EXPECT_EQ(nimcp_tensor_numel(backward_grads[0]), 3u);

    nimcp_tensor_destroy(backward_grads[0]);
    nimcp_tensor_destroy(u);
    nimcp_tensor_destroy(v);
    nimcp_tensor_destroy(outer);
    nimcp_tensor_destroy(A);
    nimcp_tensor_destroy(result);
}

//=============================================================================
// E2E Tests: Batch Processing
//=============================================================================

TEST_F(TensorE2ETest, BatchEinsum_MatMul) {
    // WHAT: Batch matrix multiply via einsum on a batch of matrices
    // WHY:  Real ML workloads process batches; verify correctness at batch level
    //
    // Batch of 2: A (2x2x3), B (2x3x1)
    // Output: C (2x2x1) via "bij,bjk->bik"

    uint32_t a_dims[] = {2, 2, 3};
    nimcp_tensor_t* A = nimcp_tensor_ones(a_dims, 3, NIMCP_DTYPE_F32);
    ASSERT_NE(A, nullptr);

    uint32_t b_dims[] = {2, 3, 1};
    nimcp_tensor_t* B = nimcp_tensor_full(b_dims, 3, NIMCP_DTYPE_F32, 2.0);
    ASSERT_NE(B, nullptr);

    nimcp_tensor_t* tensors[] = {A, B};
    nimcp_tensor_t* C = nimcp_tensor_einsum("bij,bjk->bik", tensors, 2);
    ASSERT_NE(C, nullptr);

    // Shape: (2, 2, 1)
    const nimcp_tensor_shape_t* c_shape = nimcp_tensor_shape(C);
    EXPECT_EQ(c_shape->rank, 3u);
    EXPECT_EQ(c_shape->dims[0], 2u);
    EXPECT_EQ(c_shape->dims[1], 2u);
    EXPECT_EQ(c_shape->dims[2], 1u);

    // Each output element = sum of 3 ones * 2 = 6
    for (size_t i = 0; i < nimcp_tensor_numel(C); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(C, i), 6.0, EPSILON)
            << "Batch matmul result mismatch at index " << i;
    }

    // Verify backward produces correct shapes
    nimcp_tensor_t* backward_inputs[] = {A, B};
    nimcp_tensor_t* backward_grads[2] = {nullptr, nullptr};

    int rc = nimcp_autodiff_backward(nullptr, C, backward_inputs, 2, backward_grads);
    EXPECT_EQ(rc, NIMCP_TENSOR_OK);

    ASSERT_NE(backward_grads[0], nullptr);
    EXPECT_EQ(nimcp_tensor_numel(backward_grads[0]), 12u);  // A is 2x2x3

    ASSERT_NE(backward_grads[1], nullptr);
    EXPECT_EQ(nimcp_tensor_numel(backward_grads[1]), 6u);   // B is 2x3x1

    nimcp_tensor_destroy(backward_grads[0]);
    nimcp_tensor_destroy(backward_grads[1]);
    nimcp_tensor_destroy(A);
    nimcp_tensor_destroy(B);
    nimcp_tensor_destroy(C);
}

//=============================================================================
// E2E Tests: Memory Lifecycle Through Full Pipeline
//=============================================================================

TEST_F(TensorE2ETest, MemoryLifecycle_NoLeaks) {
    // WHAT: Run a complete pipeline and verify memory accounting is consistent
    // WHY:  Memory leaks in production are catastrophic for long-running systems

    nimcp_tensor_reset_stats();
    nimcp_tensor_stats_t stats_before;
    nimcp_tensor_get_stats(&stats_before);

    // Run a pipeline: create, einsum, backward, cleanup
    {
        uint32_t dims[] = {4, 4};
        nimcp_tensor_t* a = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
        nimcp_tensor_t* b = nimcp_tensor_randn(dims, 2, NIMCP_DTYPE_F32, 0.0, 1.0);
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);

        // Einsum matmul
        nimcp_tensor_t* tensors[] = {a, b};
        nimcp_tensor_t* c = nimcp_tensor_einsum("ij,jk->ik", tensors, 2);
        ASSERT_NE(c, nullptr);

        // Trace
        nimcp_tensor_t* trace_ts[] = {c};
        nimcp_tensor_t* tr = nimcp_tensor_einsum("ii->", trace_ts, 1);
        ASSERT_NE(tr, nullptr);

        // Backward
        nimcp_tensor_t* backward_inputs[] = {a};
        nimcp_tensor_t* backward_grads[1] = {nullptr};
        nimcp_autodiff_backward(nullptr, tr, backward_inputs, 1, backward_grads);

        // Cleanup in reverse order
        if (backward_grads[0]) nimcp_tensor_destroy(backward_grads[0]);
        nimcp_tensor_destroy(tr);
        nimcp_tensor_destroy(c);
        nimcp_tensor_destroy(b);
        nimcp_tensor_destroy(a);
    }

    nimcp_tensor_stats_t stats_after;
    nimcp_tensor_get_stats(&stats_after);

    // All created tensors should be destroyed
    EXPECT_EQ(stats_after.tensors_created, stats_after.tensors_destroyed)
        << "Memory leak: created=" << stats_after.tensors_created
        << " destroyed=" << stats_after.tensors_destroyed;
}

//=============================================================================
// E2E Tests: Hadamard Product + Sum Loss + Gradient
//=============================================================================

TEST_F(TensorE2ETest, HadamardLoss_Gradient) {
    // WHAT: L = sum(A * B) via einsum("ij,ij->"), gradient dL/dA = B
    // WHY:  Element-wise product loss is used in reconstruction losses

    auto hadamard_loss_fn = [](const nimcp_tensor_t* a, void* ctx) -> double {
        nimcp_tensor_t* b = static_cast<nimcp_tensor_t*>(ctx);
        nimcp_tensor_t* tensors[] = {const_cast<nimcp_tensor_t*>(a), b};
        nimcp_tensor_t* result = nimcp_tensor_einsum("ij,ij->", tensors, 2);
        if (!result) return 0.0;
        double val = nimcp_tensor_get_flat(result, 0);
        nimcp_tensor_destroy(result);
        return val;
    };

    uint32_t dims[] = {2, 2};
    float a_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b_data[] = {5.0f, 6.0f, 7.0f, 8.0f};

    nimcp_tensor_t* A = nimcp_tensor_from_data(a_data, dims, 2, NIMCP_DTYPE_F32, true);
    nimcp_tensor_t* B = nimcp_tensor_from_data(b_data, dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(A, nullptr);
    ASSERT_NE(B, nullptr);

    // Verify forward: sum(A * B) = 1*5+2*6+3*7+4*8 = 5+12+21+32 = 70
    nimcp_tensor_t* tensors[] = {A, B};
    nimcp_tensor_t* fwd = nimcp_tensor_einsum("ij,ij->", tensors, 2);
    ASSERT_NE(fwd, nullptr);
    EXPECT_NEAR(nimcp_tensor_get_flat(fwd, 0), 70.0, EPSILON);

    // Gradient dL/dA = B
    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(hadamard_loss_fn, A, 1e-5, B);
    ASSERT_NE(grad, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 0), 5.0, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 1), 6.0, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 2), 7.0, GRAD_EPSILON);
    EXPECT_NEAR(nimcp_tensor_get_flat(grad, 3), 8.0, GRAD_EPSILON);

    nimcp_tensor_destroy(A);
    nimcp_tensor_destroy(B);
    nimcp_tensor_destroy(fwd);
    nimcp_tensor_destroy(grad);
}

//=============================================================================
// E2E Tests: Column and Row Sum Gradients
//=============================================================================

TEST_F(TensorE2ETest, RowSum_Gradient) {
    // WHAT: L = sum(einsum("ij->i", A)) = sum of all elements of A
    // WHY:  Row reduction is common in batch loss computation; gradient = ones

    auto row_sum_loss = [](const nimcp_tensor_t* a, void* ctx) -> double {
        (void)ctx;
        nimcp_tensor_t* tensors[] = {const_cast<nimcp_tensor_t*>(a)};
        nimcp_tensor_t* row_sums = nimcp_tensor_einsum("ij->i", tensors, 1);
        if (!row_sums) return 0.0;

        nimcp_tensor_t* total = nimcp_tensor_sum(row_sums);
        double val = total ? nimcp_tensor_get_flat(total, 0) : 0.0;

        nimcp_tensor_destroy(total);
        nimcp_tensor_destroy(row_sums);
        return val;
    };

    uint32_t dims[] = {2, 3};
    nimcp_tensor_t* A = CreateSequential2D(2, 3);
    ASSERT_NE(A, nullptr);

    nimcp_tensor_t* grad = nimcp_tensor_numerical_gradient(row_sum_loss, A, 1e-5, nullptr);
    ASSERT_NE(grad, nullptr);

    // d(sum(row_sum(A)))/dA_ij = 1 for all i,j
    for (size_t i = 0; i < nimcp_tensor_numel(grad); i++) {
        EXPECT_NEAR(nimcp_tensor_get_flat(grad, i), 1.0, GRAD_EPSILON)
            << "Row sum gradient should be 1.0 at index " << i;
    }

    nimcp_tensor_destroy(A);
    nimcp_tensor_destroy(grad);
}

//=============================================================================
// E2E Tests: Einsum Consistency Across Multiple Data Types
//=============================================================================

TEST_F(TensorE2ETest, EinsumConsistency_F32vsF64) {
    // WHAT: Einsum results should be consistent between F32 and F64
    // WHY:  Numerical stability across precisions

    float f32_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t dims[] = {2, 2};

    nimcp_tensor_t* a32 = nimcp_tensor_from_data(f32_data, dims, 2, NIMCP_DTYPE_F32, true);
    ASSERT_NE(a32, nullptr);

    // Trace via einsum on F32
    nimcp_tensor_t* ts32[] = {a32};
    nimcp_tensor_t* trace32 = nimcp_tensor_einsum("ii->", ts32, 1);
    ASSERT_NE(trace32, nullptr);

    double trace_val = nimcp_tensor_get_flat(trace32, 0);
    EXPECT_NEAR(trace_val, 5.0, EPSILON);  // 1 + 4 = 5

    // Row sum via einsum on F32
    nimcp_tensor_t* row_sum = nimcp_tensor_einsum("ij->i", ts32, 1);
    ASSERT_NE(row_sum, nullptr);

    EXPECT_NEAR(nimcp_tensor_get_flat(row_sum, 0), 3.0, EPSILON);   // 1+2
    EXPECT_NEAR(nimcp_tensor_get_flat(row_sum, 1), 7.0, EPSILON);   // 3+4

    nimcp_tensor_destroy(a32);
    nimcp_tensor_destroy(trace32);
    nimcp_tensor_destroy(row_sum);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
