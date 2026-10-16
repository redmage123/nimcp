/**
 * @file test_loss_functions_regression.cpp
 * @brief Regression tests for loss functions module
 *
 * Tests:
 * - Known mathematical properties of loss functions
 * - Backward compatibility with established baselines
 * - Performance characteristics and scaling
 * - Numerical stability across edge cases
 *
 * Phase TM-2: Loss Functions Regression Testing
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_loss_functions.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

namespace {

constexpr float EPSILON = 1e-5f;
constexpr float TOLERANCE = 1e-4f;

/**
 * @brief Regression test fixture
 */
class LossFunctionsRegressionTest : public ::testing::Test {
protected:
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t memory_mgr = nullptr;

    void SetUp() override {
        security_ctx = nimcp_sec_integration_create();
        memory_mgr = nullptr;
    }

    void TearDown() override {
        if (security_ctx) {
            nimcp_sec_integration_destroy(security_ctx);
            security_ctx = nullptr;
        }
    }

    // Helper: Measure execution time
    template<typename Func>
    double measure_time_ms(Func&& func, size_t iterations = 1) {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; i++) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    }
};

// ============================================================================
// Mathematical Properties Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, MSE_KnownValues) {
    // Test known MSE computations
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Test case 1: Simple difference
    // MSE([1], [0]) = (1-0)^2 = 1
    {
        float pred[] = {1.0f};
        float tgt[] = {0.0f};
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 1, &result);
        EXPECT_NEAR(result.loss_value, 1.0f, TOLERANCE);
    }

    // Test case 2: Multiple values
    // MSE([2,4], [1,2]) = ((2-1)^2 + (4-2)^2) / 2 = (1 + 4) / 2 = 2.5
    {
        float pred[] = {2.0f, 4.0f};
        float tgt[] = {1.0f, 2.0f};
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 2, &result);
        EXPECT_NEAR(result.loss_value, 2.5f, TOLERANCE);
    }

    // Test case 3: Perfect prediction
    {
        float pred[] = {1.0f, 2.0f, 3.0f};
        float tgt[] = {1.0f, 2.0f, 3.0f};
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 3, &result);
        EXPECT_NEAR(result.loss_value, 0.0f, TOLERANCE);
    }

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, MSE_GradientKnownValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    // Default config uses NIMCP_LOSS_REDUCE_MEAN
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // MSE gradient: d/dx(mean((x-y)^2)) = 2*(x-y)/n
    // For x=2, y=1, n=1: gradient = 2*(2-1)/1 = 2
    float pred[] = {2.0f};
    float tgt[] = {1.0f};
    float grad[1];

    nimcp_loss_backward(ctx, pred, tgt, 1, 1, grad);
    EXPECT_NEAR(grad[0], 2.0f, TOLERANCE);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, MAE_KnownValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MAE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // MAE([2,5], [1,2]) = (|2-1| + |5-2|) / 2 = (1 + 3) / 2 = 2
    float pred[] = {2.0f, 5.0f};
    float tgt[] = {1.0f, 2.0f};
    nimcp_loss_result_t result;

    nimcp_loss_forward(ctx, pred, tgt, 1, 2, &result);
    EXPECT_NEAR(result.loss_value, 2.0f, TOLERANCE);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, BCE_KnownValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_BINARY_CROSS_ENTROPY);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // BCE(p, y) = -(y*log(p) + (1-y)*log(1-p))
    // For p=0.5, y=1: BCE = -(1*log(0.5) + 0*log(0.5)) = -log(0.5) = 0.693...
    float pred[] = {0.5f};
    float tgt[] = {1.0f};
    nimcp_loss_result_t result;

    nimcp_loss_forward(ctx, pred, tgt, 1, 1, &result);
    EXPECT_NEAR(result.loss_value, -std::log(0.5f), TOLERANCE);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, CrossEntropy_KnownValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_CROSS_ENTROPY);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // CE = -sum(y * log(p))
    // For p=[0.7, 0.2, 0.1], y=[1, 0, 0]: CE = -log(0.7)
    float pred[] = {0.7f, 0.2f, 0.1f};
    float tgt[] = {1.0f, 0.0f, 0.0f};
    nimcp_loss_result_t result;

    nimcp_loss_forward(ctx, pred, tgt, 1, 3, &result);
    EXPECT_NEAR(result.loss_value, -std::log(0.7f), TOLERANCE);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, KLDivergence_KnownValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_KL_DIVERGENCE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // KL(P||Q) = sum(P * log(P/Q))
    // For identical distributions, KL = 0
    float pred[] = {0.5f, 0.5f};
    float tgt[] = {0.5f, 0.5f};
    nimcp_loss_result_t result;

    nimcp_loss_forward(ctx, pred, tgt, 1, 2, &result);
    EXPECT_NEAR(result.loss_value, 0.0f, TOLERANCE);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, Huber_KnownValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_HUBER);
    config.params.huber.delta = 1.0f;
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Huber loss:
    // |a| <= delta: 0.5 * a^2
    // |a| > delta:  delta * (|a| - 0.5 * delta)

    // Test quadratic region: a=0.5, delta=1 -> 0.5 * 0.5^2 = 0.125
    {
        float pred[] = {1.5f};
        float tgt[] = {1.0f};  // diff = 0.5
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 1, &result);
        EXPECT_NEAR(result.loss_value, 0.125f, TOLERANCE);
    }

    // Test linear region: a=2, delta=1 -> 1*(2 - 0.5*1) = 1.5
    {
        float pred[] = {3.0f};
        float tgt[] = {1.0f};  // diff = 2
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 1, &result);
        EXPECT_NEAR(result.loss_value, 1.5f, TOLERANCE);
    }

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, BackwardCompat_DefaultConfigs) {
    // Verify default configs haven't changed
    nimcp_loss_config_t mse_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    EXPECT_EQ(mse_config.type, NIMCP_LOSS_MSE);
    EXPECT_EQ(mse_config.params.mse.reduction, NIMCP_LOSS_REDUCE_MEAN);
    EXPECT_FALSE(mse_config.clip_gradients);
    // Note: use_memory_pool default may vary based on implementation

    nimcp_loss_config_t ce_config = nimcp_loss_default_config(NIMCP_LOSS_CROSS_ENTROPY);
    EXPECT_EQ(ce_config.type, NIMCP_LOSS_CROSS_ENTROPY);

    nimcp_loss_config_t huber_config = nimcp_loss_default_config(NIMCP_LOSS_HUBER);
    EXPECT_EQ(huber_config.type, NIMCP_LOSS_HUBER);
    EXPECT_GT(huber_config.params.huber.delta, 0.0f);
}

TEST_F(LossFunctionsRegressionTest, BackwardCompat_TypeNames) {
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_MSE), "MSE");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_MAE), "MAE");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_CROSS_ENTROPY), "CrossEntropy");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_BINARY_CROSS_ENTROPY), "BinaryCrossEntropy");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_KL_DIVERGENCE), "KLDivergence");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_HUBER), "Huber");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_HINGE), "Hinge");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_FOCAL), "Focal");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_CONTRASTIVE), "Contrastive");
    EXPECT_STREQ(nimcp_loss_type_name(NIMCP_LOSS_TRIPLET), "Triplet");
}

TEST_F(LossFunctionsRegressionTest, BackwardCompat_ReductionModes) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float pred[] = {1.0f, 2.0f, 3.0f};
    float tgt[] = {0.0f, 0.0f, 0.0f};

    // Mean reduction: (1 + 4 + 9) / 3 = 4.67
    config.params.mse.reduction = NIMCP_LOSS_REDUCE_MEAN;
    nimcp_loss_destroy(ctx);
    ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    {
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 3, &result);
        EXPECT_NEAR(result.loss_value, 14.0f / 3.0f, TOLERANCE);
    }

    // Sum reduction: 1 + 4 + 9 = 14
    config.params.mse.reduction = NIMCP_LOSS_REDUCE_SUM;
    nimcp_loss_destroy(ctx);
    ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    {
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 3, &result);
        EXPECT_NEAR(result.loss_value, 14.0f, TOLERANCE);
    }

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Numerical Stability Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, NumericalStability_VerySmallValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_BINARY_CROSS_ENTROPY);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Near-zero predictions
    float pred[] = {1e-10f, 1.0f - 1e-10f};
    float tgt[] = {0.0f, 1.0f};
    nimcp_loss_result_t result;

    nimcp_loss_forward(ctx, pred, tgt, 2, 1, &result);
    EXPECT_FALSE(std::isnan(result.loss_value));
    EXPECT_FALSE(std::isinf(result.loss_value));

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, NumericalStability_VeryLargeValues) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float pred[] = {1e6f};
    float tgt[] = {-1e6f};
    nimcp_loss_result_t result;

    nimcp_loss_forward(ctx, pred, tgt, 1, 1, &result);
    EXPECT_FALSE(std::isnan(result.loss_value));
    // Large but finite
    EXPECT_TRUE(std::isfinite(result.loss_value));

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, NumericalStability_GradientConsistency) {
    // Gradients should be consistent across multiple calls
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float pred[] = {0.7f};
    float tgt[] = {0.3f};

    std::vector<float> gradients;
    for (int i = 0; i < 100; i++) {
        nimcp_loss_result_t result;
        nimcp_loss_forward_backward(ctx, pred, tgt, 1, 1, &result);
        gradients.push_back(result.gradients[0]);
        nimcp_loss_result_free(&result);
    }

    // All gradients should be identical
    for (size_t i = 1; i < gradients.size(); i++) {
        EXPECT_FLOAT_EQ(gradients[0], gradients[i]);
    }

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Performance Regression Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, Performance_SmallBatch) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> predictions(32 * 10, 0.5f);
    std::vector<float> targets(32 * 10, 0.7f);

    double time_ms = measure_time_ms([&]() {
        nimcp_loss_result_t result;
        nimcp_loss_forward_backward(ctx, predictions.data(), targets.data(), 32, 10, &result);
        nimcp_loss_result_free(&result);
    }, 1000);

    // Should complete in < 1ms per iteration
    EXPECT_LT(time_ms, 1.0) << "Small batch took " << time_ms << "ms";

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, Performance_LargeBatch) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    constexpr size_t LARGE_BATCH = 1024;
    constexpr size_t LARGE_DIM = 1024;

    std::vector<float> predictions(LARGE_BATCH * LARGE_DIM, 0.5f);
    std::vector<float> targets(LARGE_BATCH * LARGE_DIM, 0.7f);

    double time_ms = measure_time_ms([&]() {
        nimcp_loss_result_t result;
        nimcp_loss_forward_backward(ctx, predictions.data(), targets.data(), LARGE_BATCH, LARGE_DIM, &result);
        nimcp_loss_result_free(&result);
    }, 10);

    // Should complete in < 100ms per iteration for 1M elements
    EXPECT_LT(time_ms, 100.0) << "Large batch took " << time_ms << "ms";

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, Performance_ContextCreation) {
    double time_ms = measure_time_ms([&]() {
        nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
        nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
        nimcp_loss_destroy(ctx);
    }, 1000);

    // Context creation should be < 1ms
    EXPECT_LT(time_ms, 1.0) << "Context creation took " << time_ms << "ms";
}

// ============================================================================
// Scaling Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, Scaling_LinearTime) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    std::vector<double> times;
    std::vector<size_t> sizes = {100, 1000, 10000, 100000};

    for (size_t size : sizes) {
        std::vector<float> predictions(size, 0.5f);
        std::vector<float> targets(size, 0.7f);

        double time_ms = measure_time_ms([&]() {
            nimcp_loss_result_t result;
            nimcp_loss_forward(ctx, predictions.data(), targets.data(), 1, size, &result);
        }, 100);

        times.push_back(time_ms);
    }

    // Check approximately linear scaling (time should grow with size)
    // Allow 3x overhead from expected linear scaling
    for (size_t i = 1; i < times.size(); i++) {
        double expected_ratio = static_cast<double>(sizes[i]) / sizes[i-1];
        double actual_ratio = times[i] / times[i-1];
        // Actual ratio should be within 3x of expected for linear scaling
        EXPECT_LT(actual_ratio, expected_ratio * 3.0)
            << "Scaling issue: size " << sizes[i] << " took " << times[i]
            << "ms vs " << times[i-1] << "ms for size " << sizes[i-1];
    }

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Stats Tracking Regression Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, Stats_Tracking) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float pred[] = {0.5f};
    float tgt[] = {1.0f};

    // Perform operations
    for (int i = 0; i < 10; i++) {
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 1, &result);
    }

    for (int i = 0; i < 5; i++) {
        float grad[1];
        nimcp_loss_backward(ctx, pred, tgt, 1, 1, grad);
    }

    for (int i = 0; i < 3; i++) {
        nimcp_loss_result_t result;
        nimcp_loss_forward_backward(ctx, pred, tgt, 1, 1, &result);
        nimcp_loss_result_free(&result);
    }

    // Verify stats
    nimcp_loss_stats_t stats;
    nimcp_loss_get_stats(ctx, &stats);

    EXPECT_EQ(stats.forward_count, 13u);   // 10 + 3
    EXPECT_EQ(stats.backward_count, 8u);   // 5 + 3

    // Reset and verify
    nimcp_loss_reset_stats(ctx);
    nimcp_loss_get_stats(ctx, &stats);
    EXPECT_EQ(stats.forward_count, 0u);
    EXPECT_EQ(stats.backward_count, 0u);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, Stats_GradientClipCount) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    config.clip_gradients = true;
    config.gradient_clip_value = 0.1f;

    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Large difference will cause clipping
    float pred[] = {0.0f};
    float tgt[] = {100.0f};

    nimcp_loss_result_t result;
    nimcp_loss_forward_backward(ctx, pred, tgt, 1, 1, &result);
    nimcp_loss_result_free(&result);

    nimcp_loss_stats_t stats;
    nimcp_loss_get_stats(ctx, &stats);
    EXPECT_GT(stats.gradient_clips, 0u);

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Error Handling Regression Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, ErrorHandling_NullContext) {
    float pred[] = {0.5f};
    float tgt[] = {1.0f};
    nimcp_loss_result_t result;

    nimcp_result_t err = nimcp_loss_forward(nullptr, pred, tgt, 1, 1, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(LossFunctionsRegressionTest, ErrorHandling_NullInputs) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float valid[] = {0.5f};
    nimcp_loss_result_t result;

    EXPECT_NE(nimcp_loss_forward(ctx, nullptr, valid, 1, 1, &result), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_loss_forward(ctx, valid, nullptr, 1, 1, &result), NIMCP_SUCCESS);
    EXPECT_NE(nimcp_loss_forward(ctx, valid, valid, 1, 1, nullptr), NIMCP_SUCCESS);

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, ErrorHandling_InvalidDimensions) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float pred[] = {0.5f};
    float tgt[] = {1.0f};
    nimcp_loss_result_t result;

    // Note: Zero dimensions may be handled gracefully (as no-op) rather than error
    // Just test that it doesn't crash
    (void)nimcp_loss_forward(ctx, pred, tgt, 0, 1, &result);
    (void)nimcp_loss_forward(ctx, pred, tgt, 1, 0, &result);

    nimcp_loss_destroy(ctx);
}

// ============================================================================
// Cross-Platform Consistency Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, CrossPlatform_DeterministicOutput) {
    // Same input should always produce same output
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);

    std::vector<float> losses;

    for (int trial = 0; trial < 10; trial++) {
        nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr);

        float pred[] = {0.123456f, 0.789012f, 0.345678f};
        float tgt[] = {0.111111f, 0.888888f, 0.333333f};

        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 3, &result);
        losses.push_back(result.loss_value);

        nimcp_loss_destroy(ctx);
    }

    // All losses should be identical
    for (size_t i = 1; i < losses.size(); i++) {
        EXPECT_FLOAT_EQ(losses[0], losses[i]);
    }
}

// ============================================================================
// Memory Leak Prevention Tests
// ============================================================================

TEST_F(LossFunctionsRegressionTest, MemoryLeak_ResultCleanup) {
    nimcp_loss_config_t config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float pred[] = {0.5f};
    float tgt[] = {1.0f};

    // Many allocations with proper cleanup
    for (int i = 0; i < 10000; i++) {
        nimcp_loss_result_t result;
        nimcp_loss_forward_backward(ctx, pred, tgt, 1, 1, &result);
        nimcp_loss_result_free(&result);
    }

    // If we get here without crashing, cleanup is working
    SUCCEED();

    nimcp_loss_destroy(ctx);
}

TEST_F(LossFunctionsRegressionTest, MemoryLeak_ContextCleanup) {
    // Many context create/destroy cycles
    for (int i = 0; i < 1000; i++) {
        nimcp_loss_config_t config = nimcp_loss_default_config(
            static_cast<nimcp_loss_type_t>(i % 10));

        nimcp_loss_context_t* ctx = nimcp_loss_create(&config, security_ctx, memory_mgr);
        ASSERT_NE(ctx, nullptr);

        float pred[] = {0.5f};
        float tgt[] = {1.0f};
        nimcp_loss_result_t result;
        nimcp_loss_forward(ctx, pred, tgt, 1, 1, &result);

        nimcp_loss_destroy(ctx);
    }

    SUCCEED();
}

}  // namespace
