/**
 * @file test_optimizers.cpp
 * @brief Unit tests for Optimizers Module (Phase TM-2)
 *
 * Tests cover:
 * - All optimizer types (SGD, Adam, AdamW, NAdam, RMSprop, AdaGrad)
 * - Momentum and Nesterov variants
 * - Parameter updates and convergence
 * - Gradient clipping
 * - Learning rate management
 * - Security integration
 * - Statistics tracking
 * - Edge cases
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <limits>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_optimizers.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

namespace {

constexpr float EPSILON = 1e-5f;
constexpr float CONVERGENCE_TOL = 1e-3f;

/**
 * @brief Test fixture for optimizer unit tests
 */
class OptimizersTest : public ::testing::Test {
protected:
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t memory_mgr = nullptr;

    void SetUp() override {
        // Initialize security context
        security_ctx = nimcp_sec_integration_create();
        if (security_ctx) {
            nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
            nimcp_sec_integration_init(security_ctx, &sec_cfg);
        }

        memory_mgr = nullptr;
    }

    void TearDown() override {
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    // Helper to compute simple quadratic loss gradient: d/dx (x^2) = 2x
    void computeQuadraticGradient(const float* params, float* gradients, size_t count) {
        for (size_t i = 0; i < count; i++) {
            gradients[i] = 2.0f * params[i];
        }
    }

    // Helper to run optimization on quadratic function
    float runQuadraticOptimization(nimcp_optimizer_context_t* ctx, size_t steps = 100) {
        float params[4] = {5.0f, -3.0f, 2.0f, -1.5f};
        float gradients[4];

        for (size_t i = 0; i < steps; i++) {
            computeQuadraticGradient(params, gradients, 4);
            nimcp_optimizer_step(ctx, params, gradients, 4);
        }

        // Return sum of squared params (loss)
        float loss = 0.0f;
        for (int i = 0; i < 4; i++) {
            loss += params[i] * params[i];
        }
        return loss;
    }
};

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(OptimizersTest, SGD_DefaultConfig) {
    nimcp_sgd_config_t config = nimcp_optimizer_sgd_default(0.01f);

    EXPECT_FLOAT_EQ(config.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(config.momentum, 0.0f);
    EXPECT_FALSE(config.nesterov);
    EXPECT_FLOAT_EQ(config.dampening, 0.0f);
    EXPECT_FLOAT_EQ(config.weight_decay, 0.0f);
}

TEST_F(OptimizersTest, Adam_DefaultConfig) {
    nimcp_adam_config_t config = nimcp_optimizer_adam_default(0.001f);

    EXPECT_FLOAT_EQ(config.learning_rate, 0.001f);
    EXPECT_FLOAT_EQ(config.beta1, 0.9f);
    EXPECT_FLOAT_EQ(config.beta2, 0.999f);
    EXPECT_FLOAT_EQ(config.epsilon, 1e-8f);
    EXPECT_FLOAT_EQ(config.weight_decay, 0.0f);
    EXPECT_FALSE(config.amsgrad);
}

TEST_F(OptimizersTest, RMSprop_DefaultConfig) {
    nimcp_rmsprop_config_t config = nimcp_optimizer_rmsprop_default(0.01f);

    EXPECT_FLOAT_EQ(config.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(config.alpha, 0.99f);
    EXPECT_FLOAT_EQ(config.epsilon, 1e-8f);
    EXPECT_FLOAT_EQ(config.weight_decay, 0.0f);
    EXPECT_FLOAT_EQ(config.momentum, 0.0f);
    EXPECT_FALSE(config.centered);
}

TEST_F(OptimizersTest, AdaGrad_DefaultConfig) {
    nimcp_adagrad_config_t config = nimcp_optimizer_adagrad_default(0.01f);

    EXPECT_FLOAT_EQ(config.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(config.lr_decay, 0.0f);
    EXPECT_FLOAT_EQ(config.weight_decay, 0.0f);
    EXPECT_FLOAT_EQ(config.initial_accumulator, 0.0f);
    EXPECT_FLOAT_EQ(config.epsilon, 1e-10f);
}

TEST_F(OptimizersTest, DefaultConfig_AllTypes) {
    for (int t = 0; t < NIMCP_OPTIMIZER_TYPE_COUNT; t++) {
        nimcp_optimizer_type_t type = static_cast<nimcp_optimizer_type_t>(t);
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(type);
        EXPECT_EQ(config.type, type);
    }
}

// ============================================================================
// Context Creation and Destruction Tests
// ============================================================================

TEST_F(OptimizersTest, CreateDestroy_SGD) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), config.params.sgd.learning_rate);
    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 0u);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, CreateDestroy_Adam) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), config.params.adam.learning_rate);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, CreateDestroy_AllTypes) {
    nimcp_optimizer_type_t types[] = {
        NIMCP_OPTIMIZER_SGD,
        NIMCP_OPTIMIZER_SGD_MOMENTUM,
        NIMCP_OPTIMIZER_NESTEROV,
        NIMCP_OPTIMIZER_ADAM,
        NIMCP_OPTIMIZER_ADAMW,
        NIMCP_OPTIMIZER_NADAM,
        NIMCP_OPTIMIZER_RMSPROP,
        NIMCP_OPTIMIZER_ADAGRAD
    };

    for (auto type : types) {
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(type);
        nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr) << "Failed to create optimizer type: " << (int)type;
        nimcp_optimizer_destroy(ctx);
    }
}

TEST_F(OptimizersTest, CreateWithNullConfig) {
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(nullptr, security_ctx, memory_mgr);
    EXPECT_EQ(ctx, nullptr);
}

// ============================================================================
// SGD Optimizer Tests
// ============================================================================

TEST_F(OptimizersTest, SGD_BasicStep) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f, 2.0f, 3.0f};
    float gradients[] = {0.1f, 0.2f, 0.3f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 3);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // params -= lr * gradients
    EXPECT_NEAR(params[0], 1.0f - 0.1f * 0.1f, EPSILON);
    EXPECT_NEAR(params[1], 2.0f - 0.1f * 0.2f, EPSILON);
    EXPECT_NEAR(params[2], 3.0f - 0.1f * 0.3f, EPSILON);

    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 1u);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, SGD_WithMomentum) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD_MOMENTUM);
    config.params.sgd.learning_rate = 0.1f;
    config.params.sgd.momentum = 0.9f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    // First step: velocity = gradient
    nimcp_optimizer_step(ctx, params, gradients, 1);
    float after_step1 = params[0];

    // Second step: velocity = momentum * velocity + gradient
    params[0] = 1.0f;  // Reset for comparison
    nimcp_optimizer_step(ctx, params, gradients, 1);
    float after_step2 = params[0];

    // With momentum, second step should have accumulated velocity
    EXPECT_NE(after_step1, after_step2);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, SGD_Nesterov) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_NESTEROV);
    config.params.sgd.learning_rate = 0.1f;
    config.params.sgd.momentum = 0.9f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {5.0f};
    float gradients[] = {2.0f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Nesterov should have look-ahead behavior
    EXPECT_LT(params[0], 5.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, SGD_WithWeightDecay) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;
    config.params.sgd.weight_decay = 0.01f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.0f};  // Zero gradient

    nimcp_optimizer_step(ctx, params, gradients, 1);

    // With weight decay, params should decrease even with zero gradient
    EXPECT_LT(params[0], 1.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, SGD_Convergence) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float loss = runQuadraticOptimization(ctx, 200);

    // Should converge to near zero for quadratic
    EXPECT_LT(loss, 0.1f);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Adam Optimizer Tests
// ============================================================================

TEST_F(OptimizersTest, Adam_BasicStep) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f, 2.0f};
    float gradients[] = {0.1f, 0.2f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Adam should update parameters
    EXPECT_NE(params[0], 1.0f);
    EXPECT_NE(params[1], 2.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, Adam_BiasCorrection) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    // First step has bias correction
    nimcp_optimizer_step(ctx, params, gradients, 1);
    float step1 = params[0];

    // Multiple steps should show bias correction effect diminishing
    for (int i = 0; i < 100; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    EXPECT_NE(step1, params[0]);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, Adam_AMSGrad) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.amsgrad = true;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // AMSGrad should work
    EXPECT_LT(params[0], 1.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, Adam_Convergence) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float loss = runQuadraticOptimization(ctx, 200);

    // Adam should converge
    EXPECT_LT(loss, 1.0f);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// AdamW Optimizer Tests
// ============================================================================

TEST_F(OptimizersTest, AdamW_DecoupledWeightDecay) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAMW);
    config.params.adamw.learning_rate = 0.1f;
    config.params.adamw.weight_decay = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.0f};  // Zero gradient

    float original = params[0];
    nimcp_optimizer_step(ctx, params, gradients, 1);

    // AdamW should apply decoupled weight decay
    EXPECT_LT(params[0], original);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, AdamW_VsAdam) {
    // Create Adam with L2 regularization
    nimcp_optimizer_config_t adam_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    adam_config.params.adam.learning_rate = 0.1f;
    adam_config.params.adam.weight_decay = 0.01f;

    // Create AdamW with decoupled weight decay
    nimcp_optimizer_config_t adamw_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAMW);
    adamw_config.params.adamw.learning_rate = 0.1f;
    adamw_config.params.adamw.weight_decay = 0.01f;

    nimcp_optimizer_context_t* adam_ctx = nimcp_optimizer_create(&adam_config, security_ctx, memory_mgr);
    nimcp_optimizer_context_t* adamw_ctx = nimcp_optimizer_create(&adamw_config, security_ctx, memory_mgr);

    ASSERT_NE(adam_ctx, nullptr);
    ASSERT_NE(adamw_ctx, nullptr);

    float adam_params[] = {1.0f};
    float adamw_params[] = {1.0f};
    float gradients[] = {0.5f};

    nimcp_optimizer_step(adam_ctx, adam_params, gradients, 1);
    nimcp_optimizer_step(adamw_ctx, adamw_params, gradients, 1);

    // They should produce different results due to different weight decay handling
    // (Though might be close, the implementation is different)
    // Just verify both work
    EXPECT_LT(adam_params[0], 1.0f);
    EXPECT_LT(adamw_params[0], 1.0f);

    nimcp_optimizer_destroy(adam_ctx);
    nimcp_optimizer_destroy(adamw_ctx);
}

// ============================================================================
// NAdam Optimizer Tests
// ============================================================================

TEST_F(OptimizersTest, NAdam_BasicStep) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_NADAM);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f, 2.0f};
    float gradients[] = {0.1f, 0.2f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // NAdam should update parameters
    EXPECT_NE(params[0], 1.0f);
    EXPECT_NE(params[1], 2.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, NAdam_Convergence) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_NADAM);
    config.params.adam.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float loss = runQuadraticOptimization(ctx, 200);

    // NAdam should converge
    EXPECT_LT(loss, 1.0f);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// RMSprop Optimizer Tests
// ============================================================================

TEST_F(OptimizersTest, RMSprop_BasicStep) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_RMSPROP);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f, 2.0f};
    float gradients[] = {0.1f, 0.2f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // RMSprop should update parameters
    EXPECT_NE(params[0], 1.0f);
    EXPECT_NE(params[1], 2.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, RMSprop_Centered) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_RMSPROP);
    config.params.rmsprop.centered = true;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_LT(params[0], 1.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, RMSprop_WithMomentum) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_RMSPROP);
    config.params.rmsprop.momentum = 0.9f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, RMSprop_Convergence) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_RMSPROP);
    config.params.rmsprop.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float loss = runQuadraticOptimization(ctx, 200);

    // RMSprop should converge
    EXPECT_LT(loss, 1.0f);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// AdaGrad Optimizer Tests
// ============================================================================

TEST_F(OptimizersTest, AdaGrad_BasicStep) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAGRAD);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f, 2.0f};
    float gradients[] = {0.1f, 0.2f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // AdaGrad should update parameters
    EXPECT_NE(params[0], 1.0f);
    EXPECT_NE(params[1], 2.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, AdaGrad_AccumulatedGradients) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAGRAD);
    config.params.adagrad.learning_rate = 1.0f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {5.0f};
    float gradients[] = {1.0f};

    // Track step sizes
    float prev_params = params[0];
    std::vector<float> step_sizes;

    for (int i = 0; i < 10; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
        step_sizes.push_back(std::abs(params[0] - prev_params));
        prev_params = params[0];
    }

    // AdaGrad: step sizes should decrease over time (accumulated gradient squares)
    EXPECT_GT(step_sizes[0], step_sizes[9]);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, AdaGrad_LRDecay) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAGRAD);
    config.params.adagrad.learning_rate = 1.0f;
    config.params.adagrad.lr_decay = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {5.0f};
    float gradients[] = {1.0f};

    for (int i = 0; i < 10; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    // LR should have decayed
    EXPECT_LT(nimcp_optimizer_get_lr(ctx), 1.0f);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Learning Rate Management Tests
// ============================================================================

TEST_F(OptimizersTest, SetGetLR) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.1f);

    nimcp_optimizer_set_lr(ctx, 0.05f);
    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.05f);

    // Invalid LR should be ignored
    nimcp_optimizer_set_lr(ctx, -1.0f);
    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.05f);

    nimcp_optimizer_set_lr(ctx, 0.0f);
    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.05f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, StepCounter) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.1f};

    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 0u);

    nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 1u);

    nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 2u);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// State Reset Tests
// ============================================================================

TEST_F(OptimizersTest, ResetState_SGD_Momentum) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD_MOMENTUM);
    config.params.sgd.momentum = 0.9f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    // Build up momentum
    for (int i = 0; i < 5; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    float before_reset = params[0];

    // Reset and verify step count is cleared
    nimcp_optimizer_reset_state(ctx);
    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 0u);

    // After reset, next step should behave like first step
    params[0] = 1.0f;
    nimcp_optimizer_step(ctx, params, gradients, 1);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, ResetState_Adam) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    // Run several steps
    for (int i = 0; i < 10; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    nimcp_optimizer_reset_state(ctx);
    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 0u);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Gradient Clipping Tests
// ============================================================================

TEST_F(OptimizersTest, GradientNorm) {
    float gradients[] = {3.0f, 4.0f};
    float norm = nimcp_optimizer_gradient_norm(gradients, 2);
    EXPECT_NEAR(norm, 5.0f, EPSILON);
}

TEST_F(OptimizersTest, ClipByValue) {
    float gradients[] = {0.5f, 2.0f, -3.0f, 0.1f};
    float max_value = 1.0f;

    size_t clipped = nimcp_optimizer_clip_by_value(gradients, 4, max_value);

    EXPECT_EQ(clipped, 2u);
    EXPECT_FLOAT_EQ(gradients[0], 0.5f);
    EXPECT_FLOAT_EQ(gradients[1], 1.0f);
    EXPECT_FLOAT_EQ(gradients[2], -1.0f);
    EXPECT_FLOAT_EQ(gradients[3], 0.1f);
}

TEST_F(OptimizersTest, ClipByNorm) {
    float gradients[] = {3.0f, 4.0f};
    float max_norm = 1.0f;

    float original_norm = nimcp_optimizer_clip_by_norm(gradients, 2, max_norm);

    EXPECT_NEAR(original_norm, 5.0f, EPSILON);

    float new_norm = nimcp_optimizer_gradient_norm(gradients, 2);
    EXPECT_NEAR(new_norm, max_norm, EPSILON);

    // Direction preserved
    EXPECT_NEAR(gradients[1] / gradients[0], 4.0f / 3.0f, EPSILON);
}

TEST_F(OptimizersTest, ClipByNorm_NoClipNeeded) {
    float gradients[] = {0.3f, 0.4f};  // norm = 0.5
    float max_norm = 1.0f;

    float original_norm = nimcp_optimizer_clip_by_norm(gradients, 2, max_norm);

    EXPECT_NEAR(original_norm, 0.5f, EPSILON);

    // No clipping should occur
    EXPECT_NEAR(gradients[0], 0.3f, EPSILON);
    EXPECT_NEAR(gradients[1], 0.4f, EPSILON);
}

TEST_F(OptimizersTest, OptimizerWithClipping) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;
    config.clip_gradients = true;
    config.gradient_clip_value = 0.5f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {0.0f};
    float gradients[] = {100.0f};  // Large gradient

    nimcp_optimizer_step(ctx, params, gradients, 1);

    // With clipping, parameter update should be bounded
    EXPECT_GE(params[0], -0.1f);  // lr * max_clip

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);
    EXPECT_GT(stats.gradient_clips, 0u);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(OptimizersTest, Statistics_Basic) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.5f};

    for (int i = 0; i < 10; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    nimcp_optimizer_stats_t stats;
    nimcp_result_t err = nimcp_optimizer_get_stats(ctx, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.step_count, 10u);
    EXPECT_GT(stats.total_gradient_norm, 0.0);
    EXPECT_GT(stats.avg_gradient_norm, 0.0);
    EXPECT_GT(stats.total_compute_time_ns, 0u);
    EXPECT_GT(stats.current_lr, 0.0);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, Statistics_Reset) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.5f};

    nimcp_optimizer_step(ctx, params, gradients, 1);

    nimcp_optimizer_reset_stats(ctx);

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);

    EXPECT_EQ(stats.step_count, 0u);
    EXPECT_DOUBLE_EQ(stats.total_gradient_norm, 0.0);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, Statistics_GradientNormTracking) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};

    // Different gradient magnitudes
    float small_grad[] = {0.1f};
    float large_grad[] = {10.0f};

    nimcp_optimizer_step(ctx, params, small_grad, 1);
    nimcp_optimizer_step(ctx, params, large_grad, 1);

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);

    EXPECT_NEAR(stats.min_gradient_norm, 0.1, 0.01);
    EXPECT_NEAR(stats.max_gradient_norm, 10.0, 0.1);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Parameter Group Tests
// ============================================================================

TEST_F(OptimizersTest, ParameterGroup_BasicStep) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f, 2.0f, 3.0f};
    float gradients[] = {0.1f, 0.2f, 0.3f};

    nimcp_param_group_t group = {
        .params = params,
        .gradients = gradients,
        .count = 3,
        .learning_rate = 0.0f,  // Use global LR
        .weight_decay = 0.0f
    };

    nimcp_result_t err = nimcp_optimizer_step_group(ctx, &group);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should use global LR
    EXPECT_NEAR(params[0], 1.0f - 0.1f * 0.1f, EPSILON);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, ParameterGroup_CustomLR) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    nimcp_param_group_t group = {
        .params = params,
        .gradients = gradients,
        .count = 1,
        .learning_rate = 0.5f,  // Custom LR for this group
        .weight_decay = 0.0f
    };

    nimcp_optimizer_step_group(ctx, &group);

    // Should use group-specific LR
    EXPECT_NEAR(params[0], 1.0f - 0.5f * 1.0f, EPSILON);

    // Global LR should be restored
    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.1f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, ZeroGrad) {
    float params[] = {1.0f, 2.0f, 3.0f};
    float gradients[] = {0.1f, 0.2f, 0.3f};

    nimcp_param_group_t group = {
        .params = params,
        .gradients = gradients,
        .count = 3,
        .learning_rate = 0.0f,
        .weight_decay = 0.0f
    };

    nimcp_optimizer_zero_grad(&group);

    EXPECT_FLOAT_EQ(gradients[0], 0.0f);
    EXPECT_FLOAT_EQ(gradients[1], 0.0f);
    EXPECT_FLOAT_EQ(gradients[2], 0.0f);
}

// ============================================================================
// Config Validation Tests
// ============================================================================

TEST_F(OptimizersTest, ValidateConfig_Valid) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    nimcp_result_t err = nimcp_optimizer_validate_config(&config);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(OptimizersTest, ValidateConfig_InvalidType) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.type = (nimcp_optimizer_type_t)999;

    nimcp_result_t err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(OptimizersTest, ValidateConfig_InvalidLR) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = -0.1f;

    nimcp_result_t err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(OptimizersTest, ValidateConfig_InvalidMomentum) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.momentum = 1.5f;  // > 1

    nimcp_result_t err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(OptimizersTest, ValidateConfig_InvalidBeta) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.beta1 = 1.0f;  // Must be < 1

    nimcp_result_t err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(OptimizersTest, ValidateConfig_NullConfig) {
    nimcp_result_t err = nimcp_optimizer_validate_config(nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Type Name Tests
// ============================================================================

TEST_F(OptimizersTest, TypeNames) {
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_SGD), "SGD");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_SGD_MOMENTUM), "SGD+Momentum");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_NESTEROV), "Nesterov");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_ADAM), "Adam");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_ADAMW), "AdamW");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_NADAM), "NAdam");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_RMSPROP), "RMSprop");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_ADAGRAD), "AdaGrad");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_CUSTOM), "Custom");
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(OptimizersTest, NullInputHandling) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t err = nimcp_optimizer_step(ctx, nullptr, nullptr, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, EmptyInputHandling) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[1] = {1.0f};
    float gradients[1] = {0.1f};

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, GetStatsNullContext) {
    nimcp_optimizer_stats_t stats;
    nimcp_result_t err = nimcp_optimizer_get_stats(nullptr, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(OptimizersTest, GetLRNullContext) {
    float lr = nimcp_optimizer_get_lr(nullptr);
    EXPECT_FLOAT_EQ(lr, 0.0f);
}

TEST_F(OptimizersTest, GetStepNullContext) {
    uint64_t step = nimcp_optimizer_get_step(nullptr);
    EXPECT_EQ(step, 0u);
}

TEST_F(OptimizersTest, IsRegisteredNullContext) {
    bool registered = nimcp_optimizer_is_registered(nullptr);
    EXPECT_FALSE(registered);
}

// ============================================================================
// Numerical Stability Tests
// ============================================================================

TEST_F(OptimizersTest, Adam_NumericalStability_LargeGradients) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1e6f};  // Large gradient

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should produce finite result
    EXPECT_TRUE(std::isfinite(params[0]));

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, Adam_NumericalStability_SmallGradients) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1e-10f};  // Very small gradient

    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should produce finite result
    EXPECT_TRUE(std::isfinite(params[0]));

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, RMSprop_NumericalStability) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_RMSPROP);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1e-15f};  // Extremely small gradient

    // Should not cause division by zero
    nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(std::isfinite(params[0]));

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Memory Management Tests
// ============================================================================

TEST_F(OptimizersTest, InitParams) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t err = nimcp_optimizer_init_params(ctx, 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, InitParams_InvalidInput) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t err = nimcp_optimizer_init_params(ctx, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);

    err = nimcp_optimizer_init_params(nullptr, 100);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Optimizer State Persistence Tests (H9)
// ============================================================================

TEST_F(OptimizersTest, SaveLoad_SGD_RoundTrip) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.05f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Run some steps to build up state
    float params[] = {5.0f, -3.0f};
    float gradients[] = {1.0f, -0.5f};
    for (int i = 0; i < 10; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 2);
    }

    uint64_t step_before = nimcp_optimizer_get_step(ctx);
    float lr_before = nimcp_optimizer_get_lr(ctx);

    // Save
    FILE* f = tmpfile();
    ASSERT_NE(f, nullptr);
    int save_ret = nimcp_optimizer_save(ctx, f);
    EXPECT_EQ(save_ret, 0);

    // Rewind and load into fresh context
    rewind(f);
    nimcp_optimizer_context_t* ctx2 = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx2, nullptr);

    int load_ret = nimcp_optimizer_load(ctx2, f);
    EXPECT_EQ(load_ret, 0);
    fclose(f);

    // Verify state restored
    EXPECT_EQ(nimcp_optimizer_get_step(ctx2), step_before);
    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx2), lr_before);

    nimcp_optimizer_destroy(ctx);
    nimcp_optimizer_destroy(ctx2);
}

TEST_F(OptimizersTest, SaveLoad_Adam_MomentumPreserved) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.learning_rate = 0.01f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Build up Adam m/v state with 50 steps
    float params[] = {5.0f, -3.0f, 2.0f, -1.5f};
    float gradients[4];
    for (int i = 0; i < 50; i++) {
        computeQuadraticGradient(params, gradients, 4);
        nimcp_optimizer_step(ctx, params, gradients, 4);
    }
    float params_after_50[4];
    memcpy(params_after_50, params, sizeof(params));

    // Save state
    FILE* f = tmpfile();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(nimcp_optimizer_save(ctx, f), 0);

    // Load into fresh context — must init params first to allocate buffers
    rewind(f);
    nimcp_optimizer_context_t* ctx_loaded = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx_loaded, nullptr);
    nimcp_optimizer_init_params(ctx_loaded, 4);
    EXPECT_EQ(nimcp_optimizer_load(ctx_loaded, f), 0);
    fclose(f);

    // Continue original for 10 more steps
    float params_orig[4];
    memcpy(params_orig, params_after_50, sizeof(params));
    for (int i = 0; i < 10; i++) {
        computeQuadraticGradient(params_orig, gradients, 4);
        nimcp_optimizer_step(ctx, params_orig, gradients, 4);
    }

    // Continue loaded for 10 more steps (should produce same result)
    float params_loaded[4];
    memcpy(params_loaded, params_after_50, sizeof(params));
    for (int i = 0; i < 10; i++) {
        computeQuadraticGradient(params_loaded, gradients, 4);
        nimcp_optimizer_step(ctx_loaded, params_loaded, gradients, 4);
    }

    // Both should produce identical results (same optimizer state)
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(params_orig[i], params_loaded[i])
            << "Mismatch at param[" << i << "]";
    }

    nimcp_optimizer_destroy(ctx);
    nimcp_optimizer_destroy(ctx_loaded);
}

TEST_F(OptimizersTest, SaveLoad_AllTypes_RoundTrip) {
    nimcp_optimizer_type_t types[] = {
        NIMCP_OPTIMIZER_SGD,
        NIMCP_OPTIMIZER_SGD_MOMENTUM,
        NIMCP_OPTIMIZER_NESTEROV,
        NIMCP_OPTIMIZER_ADAM,
        NIMCP_OPTIMIZER_ADAMW,
        NIMCP_OPTIMIZER_NADAM,
        NIMCP_OPTIMIZER_RMSPROP,
        NIMCP_OPTIMIZER_ADAGRAD
    };

    for (auto type : types) {
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(type);
        nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr) << "Failed to create type " << (int)type;

        // Run a few steps
        float params[] = {3.0f, -2.0f};
        float gradients[] = {0.5f, -0.3f};
        for (int i = 0; i < 5; i++) {
            nimcp_optimizer_step(ctx, params, gradients, 2);
        }

        // Save
        FILE* f = tmpfile();
        ASSERT_NE(f, nullptr);
        int save_ret = nimcp_optimizer_save(ctx, f);
        EXPECT_EQ(save_ret, 0) << "Save failed for type " << (int)type;

        // Load — init params first so buffers are allocated for restore
        rewind(f);
        nimcp_optimizer_context_t* ctx2 = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx2, nullptr);
        nimcp_optimizer_init_params(ctx2, 2);

        int load_ret = nimcp_optimizer_load(ctx2, f);
        EXPECT_EQ(load_ret, 0) << "Load failed for type " << (int)type;
        fclose(f);

        EXPECT_EQ(nimcp_optimizer_get_step(ctx2), nimcp_optimizer_get_step(ctx))
            << "Step mismatch for type " << (int)type;

        nimcp_optimizer_destroy(ctx);
        nimcp_optimizer_destroy(ctx2);
    }
}

TEST_F(OptimizersTest, SaveLoad_TypeMismatch_Fails) {
    // Save an Adam optimizer
    nimcp_optimizer_config_t adam_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    nimcp_optimizer_context_t* adam_ctx = nimcp_optimizer_create(&adam_config, security_ctx, memory_mgr);
    ASSERT_NE(adam_ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.1f};
    nimcp_optimizer_step(adam_ctx, params, gradients, 1);

    FILE* f = tmpfile();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(nimcp_optimizer_save(adam_ctx, f), 0);

    // Try to load into an SGD context — should fail
    rewind(f);
    nimcp_optimizer_config_t sgd_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* sgd_ctx = nimcp_optimizer_create(&sgd_config, security_ctx, memory_mgr);
    ASSERT_NE(sgd_ctx, nullptr);

    int load_ret = nimcp_optimizer_load(sgd_ctx, f);
    EXPECT_EQ(load_ret, -1) << "Loading Adam state into SGD should fail";

    fclose(f);
    nimcp_optimizer_destroy(adam_ctx);
    nimcp_optimizer_destroy(sgd_ctx);
}

TEST_F(OptimizersTest, SaveLoad_NullContext_Fails) {
    FILE* f = tmpfile();
    ASSERT_NE(f, nullptr);

    EXPECT_EQ(nimcp_optimizer_save(nullptr, f), -1);
    EXPECT_EQ(nimcp_optimizer_load(nullptr, f), -1);

    fclose(f);
}

TEST_F(OptimizersTest, SaveLoad_NullFile_Fails) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(nimcp_optimizer_save(ctx, nullptr), -1);
    EXPECT_EQ(nimcp_optimizer_load(ctx, nullptr), -1);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersTest, SaveLoad_TruncatedFile_Fails) {
    // Save a valid optimizer state
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.1f};
    nimcp_optimizer_step(ctx, params, gradients, 1);

    FILE* f = tmpfile();
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(nimcp_optimizer_save(ctx, f), 0);

    // Get file size, then truncate to half
    long file_size = ftell(f);
    rewind(f);

    // Create a truncated copy
    FILE* trunc = tmpfile();
    ASSERT_NE(trunc, nullptr);
    char buf[256];
    long half = file_size / 2;
    long written = 0;
    while (written < half) {
        size_t to_read = (size_t)((half - written) < 256 ? (half - written) : 256);
        size_t n = fread(buf, 1, to_read, f);
        if (n == 0) break;
        fwrite(buf, 1, n, trunc);
        written += (long)n;
    }
    rewind(trunc);

    // Loading from truncated file should fail
    nimcp_optimizer_context_t* ctx2 = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx2, nullptr);

    int load_ret = nimcp_optimizer_load(ctx2, trunc);
    EXPECT_EQ(load_ret, -1) << "Loading from truncated file should fail";

    fclose(f);
    fclose(trunc);
    nimcp_optimizer_destroy(ctx);
    nimcp_optimizer_destroy(ctx2);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
