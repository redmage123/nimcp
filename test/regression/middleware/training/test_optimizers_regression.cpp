/**
 * @file test_optimizers_regression.cpp
 * @brief Regression tests for Optimizers Module (Phase TM-2)
 *
 * Tests cover:
 * - API stability and backwards compatibility
 * - Configuration defaults
 * - Numerical precision and determinism
 * - Performance baseline metrics
 * - Memory usage patterns
 * - Error handling consistency
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_optimizers.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

namespace {

constexpr float EPSILON = 1e-6f;
constexpr float REGRESSION_TOL = 1e-4f;

/**
 * @brief Test fixture for optimizer regression tests
 */
class OptimizersRegressionTest : public ::testing::Test {
protected:
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t memory_mgr = nullptr;

    void SetUp() override {
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
};

// ============================================================================
// API Stability Tests - Ensure API contracts are maintained
// ============================================================================

TEST_F(OptimizersRegressionTest, API_DefaultConfigStructure) {
    // Verify default config structure is consistent
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    // Type should match
    EXPECT_EQ(config.type, NIMCP_OPTIMIZER_ADAM);

    // Default values for Adam
    EXPECT_FLOAT_EQ(config.params.adam.learning_rate, 0.001f);
    EXPECT_FLOAT_EQ(config.params.adam.beta1, 0.9f);
    EXPECT_FLOAT_EQ(config.params.adam.beta2, 0.999f);
    EXPECT_FLOAT_EQ(config.params.adam.epsilon, 1e-8f);
    EXPECT_FLOAT_EQ(config.params.adam.weight_decay, 0.0f);
    EXPECT_FALSE(config.params.adam.amsgrad);

    // Clipping defaults
    EXPECT_FALSE(config.clip_gradients);
    EXPECT_FLOAT_EQ(config.gradient_clip_value, 1.0f);
    EXPECT_FLOAT_EQ(config.gradient_clip_norm, 0.0f);
}

TEST_F(OptimizersRegressionTest, API_SGDDefaultStructure) {
    nimcp_sgd_config_t sgd = nimcp_optimizer_sgd_default(0.1f);

    EXPECT_FLOAT_EQ(sgd.learning_rate, 0.1f);
    EXPECT_FLOAT_EQ(sgd.momentum, 0.0f);
    EXPECT_FALSE(sgd.nesterov);
    EXPECT_FLOAT_EQ(sgd.dampening, 0.0f);
    EXPECT_FLOAT_EQ(sgd.weight_decay, 0.0f);
}

TEST_F(OptimizersRegressionTest, API_RMSpropDefaultStructure) {
    nimcp_rmsprop_config_t rmsprop = nimcp_optimizer_rmsprop_default(0.01f);

    EXPECT_FLOAT_EQ(rmsprop.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(rmsprop.alpha, 0.99f);
    EXPECT_FLOAT_EQ(rmsprop.epsilon, 1e-8f);
    EXPECT_FLOAT_EQ(rmsprop.weight_decay, 0.0f);
    EXPECT_FLOAT_EQ(rmsprop.momentum, 0.0f);
    EXPECT_FALSE(rmsprop.centered);
}

TEST_F(OptimizersRegressionTest, API_AdaGradDefaultStructure) {
    nimcp_adagrad_config_t adagrad = nimcp_optimizer_adagrad_default(0.01f);

    EXPECT_FLOAT_EQ(adagrad.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(adagrad.lr_decay, 0.0f);
    EXPECT_FLOAT_EQ(adagrad.weight_decay, 0.0f);
    EXPECT_FLOAT_EQ(adagrad.initial_accumulator, 0.0f);
    EXPECT_FLOAT_EQ(adagrad.epsilon, 1e-10f);
}

TEST_F(OptimizersRegressionTest, API_TypeNames) {
    // Type names should remain stable
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_SGD), "SGD");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_SGD_MOMENTUM), "SGD+Momentum");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_NESTEROV), "Nesterov");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_ADAGRAD), "AdaGrad");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_RMSPROP), "RMSprop");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_ADAM), "Adam");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_ADAMW), "AdamW");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_NADAM), "NAdam");
    EXPECT_STREQ(nimcp_optimizer_type_name(NIMCP_OPTIMIZER_CUSTOM), "Custom");
}

TEST_F(OptimizersRegressionTest, API_TypeCount) {
    // Ensure type count is as expected (9 types: SGD, SGD_MOMENTUM, NESTEROV, ADAGRAD, RMSPROP, ADAM, ADAMW, NADAM, CUSTOM)
    EXPECT_EQ(NIMCP_OPTIMIZER_TYPE_COUNT, 9);
}

// ============================================================================
// Numerical Precision Tests - Ensure deterministic behavior
// ============================================================================

TEST_F(OptimizersRegressionTest, NumericalPrecision_SGD_SingleStep) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f, 2.0f, 3.0f};
    float gradients[] = {0.1f, 0.2f, 0.3f};

    nimcp_optimizer_step(ctx, params, gradients, 3);

    // Expected: params -= lr * gradients
    EXPECT_NEAR(params[0], 1.0f - 0.1f * 0.1f, REGRESSION_TOL);
    EXPECT_NEAR(params[1], 2.0f - 0.1f * 0.2f, REGRESSION_TOL);
    EXPECT_NEAR(params[2], 3.0f - 0.1f * 0.3f, REGRESSION_TOL);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersRegressionTest, NumericalPrecision_Adam_BiasCorrection) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.learning_rate = 0.1f;
    config.params.adam.beta1 = 0.9f;
    config.params.adam.beta2 = 0.999f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {1.0f};

    // First step
    nimcp_optimizer_step(ctx, params, gradients, 1);
    float after_step1 = params[0];

    // Reset and verify same result
    nimcp_optimizer_reset_state(ctx);
    params[0] = 1.0f;
    nimcp_optimizer_step(ctx, params, gradients, 1);

    EXPECT_NEAR(params[0], after_step1, REGRESSION_TOL);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersRegressionTest, NumericalPrecision_Determinism) {
    // Run same optimization twice, verify identical results
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.learning_rate = 0.01f;

    std::vector<float> results1, results2;

    for (int run = 0; run < 2; run++) {
        nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr);

        float params[] = {5.0f, -3.0f, 2.0f};

        for (int i = 0; i < 50; i++) {
            float gradients[] = {2.0f * params[0], 2.0f * params[1], 2.0f * params[2]};
            nimcp_optimizer_step(ctx, params, gradients, 3);
        }

        if (run == 0) {
            results1 = {params[0], params[1], params[2]};
        } else {
            results2 = {params[0], params[1], params[2]};
        }

        nimcp_optimizer_destroy(ctx);
    }

    // Results should be identical
    for (size_t i = 0; i < results1.size(); i++) {
        EXPECT_NEAR(results1[i], results2[i], REGRESSION_TOL);
    }
}

// ============================================================================
// Gradient Clipping Precision Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, GradientClipping_ByValue) {
    float gradients[] = {0.5f, 2.0f, -3.0f, 0.1f, 1.5f};
    float max_value = 1.0f;

    size_t clipped = nimcp_optimizer_clip_by_value(gradients, 5, max_value);

    EXPECT_EQ(clipped, 3u);  // 2.0, -3.0, 1.5 clipped

    EXPECT_FLOAT_EQ(gradients[0], 0.5f);
    EXPECT_FLOAT_EQ(gradients[1], 1.0f);
    EXPECT_FLOAT_EQ(gradients[2], -1.0f);
    EXPECT_FLOAT_EQ(gradients[3], 0.1f);
    EXPECT_FLOAT_EQ(gradients[4], 1.0f);
}

TEST_F(OptimizersRegressionTest, GradientClipping_ByNorm) {
    float gradients[] = {3.0f, 4.0f};  // norm = 5
    float max_norm = 1.0f;

    float original_norm = nimcp_optimizer_clip_by_norm(gradients, 2, max_norm);

    EXPECT_NEAR(original_norm, 5.0f, EPSILON);

    // New norm should be max_norm
    float new_norm = std::sqrt(gradients[0] * gradients[0] + gradients[1] * gradients[1]);
    EXPECT_NEAR(new_norm, max_norm, EPSILON);

    // Ratio preserved
    EXPECT_NEAR(gradients[0] / gradients[1], 0.75f, EPSILON);
}

TEST_F(OptimizersRegressionTest, GradientNorm_Calculation) {
    float gradients[] = {3.0f, 4.0f, 0.0f};
    float norm = nimcp_optimizer_gradient_norm(gradients, 3);
    EXPECT_NEAR(norm, 5.0f, EPSILON);
}

// ============================================================================
// Configuration Validation Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, ConfigValidation_ValidConfigs) {
    nimcp_optimizer_type_t types[] = {
        NIMCP_OPTIMIZER_SGD,
        NIMCP_OPTIMIZER_ADAM,
        NIMCP_OPTIMIZER_RMSPROP,
        NIMCP_OPTIMIZER_ADAGRAD
    };

    for (auto type : types) {
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(type);
        nimcp_result_t err = nimcp_optimizer_validate_config(&config);
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed for type: " << nimcp_optimizer_type_name(type);
    }
}

TEST_F(OptimizersRegressionTest, ConfigValidation_InvalidLR) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.0f;

    nimcp_result_t err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);

    config.params.sgd.learning_rate = -0.1f;
    err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(OptimizersRegressionTest, ConfigValidation_InvalidBeta) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    // beta1 >= 1 is invalid
    config.params.adam.beta1 = 1.0f;
    nimcp_result_t err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // beta2 >= 1 is invalid
    config.params.adam.beta1 = 0.9f;
    config.params.adam.beta2 = 1.0f;
    err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // negative beta is invalid
    config.params.adam.beta1 = -0.1f;
    config.params.adam.beta2 = 0.999f;
    err = nimcp_optimizer_validate_config(&config);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, ErrorHandling_NullInputs) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Null params
    float gradients[] = {1.0f};
    nimcp_result_t err = nimcp_optimizer_step(ctx, nullptr, gradients, 1);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Null gradients
    float params[] = {1.0f};
    err = nimcp_optimizer_step(ctx, params, nullptr, 1);
    EXPECT_NE(err, NIMCP_SUCCESS);

    // Zero count
    err = nimcp_optimizer_step(ctx, params, gradients, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersRegressionTest, ErrorHandling_NullContext) {
    nimcp_result_t err = nimcp_optimizer_step(nullptr, nullptr, nullptr, 0);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_optimizer_stats_t stats;
    err = nimcp_optimizer_get_stats(nullptr, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(nullptr), 0.0f);
    EXPECT_EQ(nimcp_optimizer_get_step(nullptr), 0u);
}

TEST_F(OptimizersRegressionTest, ErrorHandling_NullConfig) {
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(nullptr, security_ctx, memory_mgr);
    EXPECT_EQ(ctx, nullptr);

    nimcp_result_t err = nimcp_optimizer_validate_config(nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, Statistics_StepCountAccumulation) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.1f};

    for (int i = 0; i < 100; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
        EXPECT_EQ(nimcp_optimizer_get_step(ctx), (uint64_t)(i + 1));
    }

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);
    EXPECT_EQ(stats.step_count, 100u);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersRegressionTest, Statistics_GradientTracking) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};

    // Steps with different gradient magnitudes
    float g1[] = {0.1f};
    float g2[] = {10.0f};
    float g3[] = {1.0f};

    nimcp_optimizer_step(ctx, params, g1, 1);
    nimcp_optimizer_step(ctx, params, g2, 1);
    nimcp_optimizer_step(ctx, params, g3, 1);

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);

    EXPECT_NEAR(stats.min_gradient_norm, 0.1, 0.01);
    EXPECT_NEAR(stats.max_gradient_norm, 10.0, 0.1);
    EXPECT_GT(stats.total_gradient_norm, 0.0);
    EXPECT_GT(stats.avg_gradient_norm, 0.0);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersRegressionTest, Statistics_LRTracking) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);
    EXPECT_NEAR(stats.current_lr, 0.1, 1e-6);

    nimcp_optimizer_set_lr(ctx, 0.05f);
    nimcp_optimizer_get_stats(ctx, &stats);
    EXPECT_NEAR(stats.current_lr, 0.05, 1e-6);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Performance Baseline Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, Performance_LargeUpdate) {
    const size_t param_count = 10000;

    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> params(param_count, 1.0f);
    std::vector<float> gradients(param_count, 0.1f);

    auto start = std::chrono::high_resolution_clock::now();

    // 100 steps with 10K params
    for (int i = 0; i < 100; i++) {
        nimcp_optimizer_step(ctx, params.data(), gradients.data(), param_count);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (< 500ms)
    EXPECT_LT(duration.count(), 500);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersRegressionTest, Performance_AllOptimizers) {
    const size_t param_count = 1000;
    const int num_steps = 50;

    nimcp_optimizer_type_t types[] = {
        NIMCP_OPTIMIZER_SGD,
        NIMCP_OPTIMIZER_SGD_MOMENTUM,
        NIMCP_OPTIMIZER_ADAM,
        NIMCP_OPTIMIZER_RMSPROP,
        NIMCP_OPTIMIZER_ADAGRAD
    };

    std::vector<float> params(param_count, 1.0f);
    std::vector<float> gradients(param_count, 0.1f);

    for (auto type : types) {
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(type);
        nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr) << "Failed for: " << nimcp_optimizer_type_name(type);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_steps; i++) {
            nimcp_optimizer_step(ctx, params.data(), gradients.data(), param_count);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Each optimizer should complete 50 steps in < 100ms
        EXPECT_LT(duration.count(), 100) << "Slow performance for: " << nimcp_optimizer_type_name(type);

        // Reset params for next optimizer
        std::fill(params.begin(), params.end(), 1.0f);

        nimcp_optimizer_destroy(ctx);
    }
}

// ============================================================================
// State Management Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, StateManagement_Reset) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {5.0f};
    float gradients[] = {1.0f};

    // Run some steps
    for (int i = 0; i < 10; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 10u);

    // Reset
    nimcp_optimizer_reset_state(ctx);
    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 0u);

    // Run more steps
    for (int i = 0; i < 5; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 5u);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersRegressionTest, StateManagement_LRChange) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.1f);

    nimcp_optimizer_set_lr(ctx, 0.05f);
    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.05f);

    // Invalid LR should be ignored
    nimcp_optimizer_set_lr(ctx, 0.0f);
    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.05f);

    nimcp_optimizer_set_lr(ctx, -0.1f);
    EXPECT_FLOAT_EQ(nimcp_optimizer_get_lr(ctx), 0.05f);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Convergence Baseline Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, Convergence_Quadratic) {
    // All optimizers should minimize x^2 reasonably
    nimcp_optimizer_type_t types[] = {
        NIMCP_OPTIMIZER_SGD,
        NIMCP_OPTIMIZER_ADAM,
        NIMCP_OPTIMIZER_RMSPROP
    };

    for (auto type : types) {
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(type);

        // Set reasonable learning rate for convergence
        switch (type) {
            case NIMCP_OPTIMIZER_SGD:
                config.params.sgd.learning_rate = 0.1f;
                break;
            case NIMCP_OPTIMIZER_ADAM:
                config.params.adam.learning_rate = 0.1f;
                break;
            case NIMCP_OPTIMIZER_RMSPROP:
                config.params.rmsprop.learning_rate = 0.1f;
                break;
            default:
                break;
        }

        nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr);

        float params[] = {10.0f};

        // Minimize x^2
        for (int i = 0; i < 200; i++) {
            float gradients[] = {2.0f * params[0]};
            nimcp_optimizer_step(ctx, params, gradients, 1);
        }

        // Should converge close to 0
        EXPECT_LT(std::abs(params[0]), 1.0f) << "Failed for: " << nimcp_optimizer_type_name(type);

        nimcp_optimizer_destroy(ctx);
    }
}

// ============================================================================
// Memory Management Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, Memory_CreateDestroyRepeat) {
    // Repeated create/destroy should not leak memory
    for (int i = 0; i < 100; i++) {
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
        nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr);

        float params[] = {1.0f, 2.0f, 3.0f};
        float gradients[] = {0.1f, 0.2f, 0.3f};

        nimcp_optimizer_step(ctx, params, gradients, 3);

        nimcp_optimizer_destroy(ctx);
    }

    // If we got here without crash/leak, test passes
    SUCCEED();
}

TEST_F(OptimizersRegressionTest, Memory_LargeParameterInit) {
    const size_t large_count = 1000000;  // 1M parameters

    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> params(large_count, 1.0f);
    std::vector<float> gradients(large_count, 0.01f);

    // First step allocates internal state
    nimcp_result_t err = nimcp_optimizer_step(ctx, params.data(), gradients.data(), large_count);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Second step should work with allocated state
    err = nimcp_optimizer_step(ctx, params.data(), gradients.data(), large_count);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Gradient Clipping Integration Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, GradientClipping_Integration) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 1.0f;
    config.clip_gradients = true;
    config.gradient_clip_value = 0.5f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {0.0f};
    float gradients[] = {100.0f};

    nimcp_optimizer_step(ctx, params, gradients, 1);

    // Update should be bounded by clipped gradient
    EXPECT_LE(std::abs(params[0]), 0.5f + EPSILON);

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);
    EXPECT_GT(stats.gradient_clips, 0u);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(OptimizersRegressionTest, BackwardCompat_StatsStructure) {
    // Verify stats structure has expected fields
    nimcp_optimizer_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // These fields must exist and be accessible
    stats.step_count = 100;
    stats.total_gradient_norm = 50.0;
    stats.min_gradient_norm = 0.1;
    stats.max_gradient_norm = 10.0;
    stats.avg_gradient_norm = 1.0;
    stats.gradient_clips = 5;
    stats.total_param_update = 25.0;
    stats.current_lr = 0.01;
    stats.total_compute_time_ns = 1000000;
    stats.peak_memory_bytes = 4096;

    // Verify assignments worked
    EXPECT_EQ(stats.step_count, 100u);
    EXPECT_DOUBLE_EQ(stats.total_gradient_norm, 50.0);
    EXPECT_DOUBLE_EQ(stats.min_gradient_norm, 0.1);
    EXPECT_DOUBLE_EQ(stats.max_gradient_norm, 10.0);
    EXPECT_DOUBLE_EQ(stats.avg_gradient_norm, 1.0);
    EXPECT_EQ(stats.gradient_clips, 5u);
    EXPECT_DOUBLE_EQ(stats.total_param_update, 25.0);
    EXPECT_DOUBLE_EQ(stats.current_lr, 0.01);
    EXPECT_EQ(stats.total_compute_time_ns, 1000000u);
    EXPECT_EQ(stats.peak_memory_bytes, 4096u);
}

TEST_F(OptimizersRegressionTest, BackwardCompat_ParamGroupStructure) {
    // Verify param group structure
    nimcp_param_group_t group;
    memset(&group, 0, sizeof(group));

    float params[] = {1.0f};
    float grads[] = {0.1f};

    group.params = params;
    group.gradients = grads;
    group.count = 1;
    group.learning_rate = 0.01f;
    group.weight_decay = 0.001f;

    EXPECT_EQ(group.params, params);
    EXPECT_EQ(group.gradients, grads);
    EXPECT_EQ(group.count, 1u);
    EXPECT_FLOAT_EQ(group.learning_rate, 0.01f);
    EXPECT_FLOAT_EQ(group.weight_decay, 0.001f);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
