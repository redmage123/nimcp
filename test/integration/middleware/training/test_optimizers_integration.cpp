/**
 * @file test_optimizers_integration.cpp
 * @brief Integration tests for Optimizers Module (Phase TM-2)
 *
 * Tests cover:
 * - Integration with Loss Functions module
 * - Multi-epoch training workflows
 * - Convergence on optimization problems
 * - Parameter group management
 * - Statistics tracking across training runs
 * - Stress testing with large parameter counts
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/validation/nimcp_common.h"

namespace {

constexpr float EPSILON = 1e-5f;
constexpr float CONVERGENCE_TOL = 1e-2f;

/**
 * @brief Test fixture for optimizer integration tests
 */
class OptimizersIntegrationTest : public ::testing::Test {
protected:
    nimcp_sec_integration_t* security_ctx = nullptr;
    unified_mem_manager_t memory_mgr = nullptr;
    std::mt19937 rng{42};

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

    // Generate random data
    std::vector<float> generateRandomData(size_t count, float min_val = -1.0f, float max_val = 1.0f) {
        std::vector<float> data(count);
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    // Simple linear regression: y = Wx + b
    struct LinearModel {
        std::vector<float> weights;
        float bias;
        size_t input_size;

        LinearModel(size_t in_size) : input_size(in_size), bias(0.0f) {
            weights.resize(in_size, 0.1f);
        }

        float forward(const std::vector<float>& x) const {
            float result = bias;
            for (size_t i = 0; i < input_size && i < x.size(); i++) {
                result += weights[i] * x[i];
            }
            return result;
        }

        void backward(const std::vector<float>& x, float loss_grad,
                      std::vector<float>& weight_grads, float& bias_grad) const {
            bias_grad = loss_grad;
            for (size_t i = 0; i < input_size && i < x.size(); i++) {
                weight_grads[i] = loss_grad * x[i];
            }
        }
    };
};

// ============================================================================
// Integration with Loss Functions Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, OptimizerWithMSELoss) {
    // Create optimizer with higher learning rate for faster convergence
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    opt_config.params.adam.learning_rate = 0.5f;  // Higher LR for this simple test
    nimcp_optimizer_context_t* opt_ctx = nimcp_optimizer_create(&opt_config, security_ctx, memory_mgr);
    ASSERT_NE(opt_ctx, nullptr);

    // Create loss context
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    nimcp_loss_context_t* loss_ctx = nimcp_loss_create(&loss_config, security_ctx, memory_mgr);
    ASSERT_NE(loss_ctx, nullptr);

    // Simple optimization: minimize (x - 5)^2
    float params[] = {0.0f};
    float target = 5.0f;

    std::vector<float> losses;
    for (int i = 0; i < 200; i++) {
        // Forward
        nimcp_loss_result_t loss_result;
        nimcp_loss_forward(loss_ctx, params, &target, 1, 1, &loss_result);
        losses.push_back(loss_result.loss_value);

        // Backward
        float gradients[1];
        nimcp_loss_mse_grad(params, &target, gradients, 1);

        // Update
        nimcp_optimizer_step(opt_ctx, params, gradients, 1);
    }

    // Should converge close to target (allow wider tolerance)
    EXPECT_NEAR(params[0], 5.0f, 1.5f);

    // Loss should decrease over time
    EXPECT_LT(losses.back(), losses.front());

    nimcp_loss_destroy(loss_ctx);
    nimcp_optimizer_destroy(opt_ctx);
}

TEST_F(OptimizersIntegrationTest, OptimizerWithCrossEntropyLoss) {
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    opt_config.params.sgd.learning_rate = 0.1f;
    nimcp_optimizer_context_t* opt_ctx = nimcp_optimizer_create(&opt_config, security_ctx, memory_mgr);
    ASSERT_NE(opt_ctx, nullptr);

    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_CROSS_ENTROPY);
    nimcp_loss_context_t* loss_ctx = nimcp_loss_create(&loss_config, security_ctx, memory_mgr);
    ASSERT_NE(loss_ctx, nullptr);

    // Softmax logits for 3-class problem
    float logits[] = {0.0f, 0.0f, 0.0f};
    float targets[] = {1.0f, 0.0f, 0.0f};  // True class 0

    float initial_loss = 0.0f;
    float final_loss = 0.0f;

    for (int i = 0; i < 50; i++) {
        // Convert to probabilities
        float probs[3];
        nimcp_loss_softmax(logits, probs, 1, 3);

        // Compute loss
        nimcp_loss_result_t loss_result;
        nimcp_loss_forward(loss_ctx, probs, targets, 1, 3, &loss_result);

        if (i == 0) initial_loss = loss_result.loss_value;
        if (i == 49) final_loss = loss_result.loss_value;

        // Backward (softmax + CE gradient = probs - targets)
        float gradients[3];
        for (int j = 0; j < 3; j++) {
            gradients[j] = probs[j] - targets[j];
        }

        nimcp_optimizer_step(opt_ctx, logits, gradients, 3);
    }

    // Loss should decrease
    EXPECT_LT(final_loss, initial_loss);

    // Logit for true class should be highest
    EXPECT_GT(logits[0], logits[1]);
    EXPECT_GT(logits[0], logits[2]);

    nimcp_loss_destroy(loss_ctx);
    nimcp_optimizer_destroy(opt_ctx);
}

// ============================================================================
// Multi-Epoch Training Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, MultiEpochTraining_LinearRegression) {
    const size_t input_size = 5;
    const size_t num_samples = 100;
    const size_t num_epochs = 50;

    // Generate synthetic data: y = 2*x0 + 3*x1 - x2 + 0.5*x3 + noise
    std::vector<std::vector<float>> X(num_samples);
    std::vector<float> y(num_samples);

    std::normal_distribution<float> noise(0.0f, 0.1f);

    for (size_t i = 0; i < num_samples; i++) {
        X[i] = generateRandomData(input_size, -1.0f, 1.0f);
        y[i] = 2.0f * X[i][0] + 3.0f * X[i][1] - 1.0f * X[i][2] + 0.5f * X[i][3] + noise(rng);
    }

    // Create optimizer
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.learning_rate = 0.05f;
    nimcp_optimizer_context_t* opt_ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(opt_ctx, nullptr);

    // Initialize model weights
    std::vector<float> weights(input_size, 0.0f);
    float bias = 0.0f;

    std::vector<float> epoch_losses;

    for (size_t epoch = 0; epoch < num_epochs; epoch++) {
        float total_loss = 0.0f;

        for (size_t i = 0; i < num_samples; i++) {
            // Forward pass
            float pred = bias;
            for (size_t j = 0; j < input_size; j++) {
                pred += weights[j] * X[i][j];
            }

            // MSE loss
            float error = pred - y[i];
            total_loss += error * error;

            // Backward pass
            std::vector<float> weight_grads(input_size);
            float bias_grad = 2.0f * error / num_samples;
            for (size_t j = 0; j < input_size; j++) {
                weight_grads[j] = 2.0f * error * X[i][j] / num_samples;
            }

            // Create combined parameter and gradient arrays
            std::vector<float> params(input_size + 1);
            std::vector<float> grads(input_size + 1);
            for (size_t j = 0; j < input_size; j++) {
                params[j] = weights[j];
                grads[j] = weight_grads[j];
            }
            params[input_size] = bias;
            grads[input_size] = bias_grad;

            nimcp_optimizer_step(opt_ctx, params.data(), grads.data(), input_size + 1);

            // Update weights
            for (size_t j = 0; j < input_size; j++) {
                weights[j] = params[j];
            }
            bias = params[input_size];
        }

        epoch_losses.push_back(total_loss / num_samples);
    }

    // Loss should decrease over epochs
    EXPECT_LT(epoch_losses.back(), epoch_losses.front());

    // Weights should be close to true values
    EXPECT_NEAR(weights[0], 2.0f, 0.5f);
    EXPECT_NEAR(weights[1], 3.0f, 0.5f);
    EXPECT_NEAR(weights[2], -1.0f, 0.5f);

    nimcp_optimizer_destroy(opt_ctx);
}

// ============================================================================
// Optimizer Comparison Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, CompareOptimizerConvergence) {
    // Compare convergence rates on simple quadratic
    nimcp_optimizer_type_t types[] = {
        NIMCP_OPTIMIZER_SGD,
        NIMCP_OPTIMIZER_SGD_MOMENTUM,
        NIMCP_OPTIMIZER_ADAM,
        NIMCP_OPTIMIZER_RMSPROP
    };

    struct Result {
        nimcp_optimizer_type_t type;
        float final_loss;
        uint64_t steps;
    };

    std::vector<Result> results;

    for (auto type : types) {
        nimcp_optimizer_config_t config = nimcp_optimizer_default_config(type);

        // Set comparable learning rates
        switch (type) {
            case NIMCP_OPTIMIZER_SGD:
            case NIMCP_OPTIMIZER_SGD_MOMENTUM:
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

        // Minimize f(x) = x^2 starting from x = 5
        float params[] = {5.0f};

        for (int i = 0; i < 100; i++) {
            float gradients[] = {2.0f * params[0]};  // d/dx (x^2) = 2x
            nimcp_optimizer_step(ctx, params, gradients, 1);
        }

        Result r;
        r.type = type;
        r.final_loss = params[0] * params[0];
        r.steps = nimcp_optimizer_get_step(ctx);
        results.push_back(r);

        nimcp_optimizer_destroy(ctx);
    }

    // All optimizers should converge
    for (const auto& r : results) {
        EXPECT_LT(r.final_loss, 1.0f) << "Optimizer " << nimcp_optimizer_type_name(r.type)
                                       << " did not converge";
    }
}

// ============================================================================
// Parameter Group Integration Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, MultipleParameterGroups) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.params.adam.learning_rate = 0.01f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    // Two parameter groups with different learning rates
    float weights1[] = {1.0f, 2.0f, 3.0f};
    float weights2[] = {4.0f, 5.0f};
    float grads1[] = {0.1f, 0.1f, 0.1f};
    float grads2[] = {0.1f, 0.1f};

    nimcp_param_group_t group1 = {
        .params = weights1,
        .gradients = grads1,
        .count = 3,
        .learning_rate = 0.1f,   // High LR
        .weight_decay = 0.0f
    };

    nimcp_param_group_t group2 = {
        .params = weights2,
        .gradients = grads2,
        .count = 2,
        .learning_rate = 0.001f,  // Low LR
        .weight_decay = 0.0f
    };

    float w1_before = weights1[0];
    float w2_before = weights2[0];

    nimcp_optimizer_step_group(ctx, &group1);
    nimcp_optimizer_step_group(ctx, &group2);

    float w1_change = std::abs(weights1[0] - w1_before);
    float w2_change = std::abs(weights2[0] - w2_before);

    // Group 1 should have larger update due to higher LR
    EXPECT_GT(w1_change, w2_change * 10);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersIntegrationTest, ZeroGradBetweenBatches) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f, 2.0f};
    float gradients[] = {0.5f, 0.5f};

    nimcp_param_group_t group = {
        .params = params,
        .gradients = gradients,
        .count = 2,
        .learning_rate = 0.0f,
        .weight_decay = 0.0f
    };

    // Simulate batch training
    for (int batch = 0; batch < 5; batch++) {
        // Zero gradients
        nimcp_optimizer_zero_grad(&group);

        EXPECT_FLOAT_EQ(group.gradients[0], 0.0f);
        EXPECT_FLOAT_EQ(group.gradients[1], 0.0f);

        // Accumulate gradients
        group.gradients[0] = 0.1f * (batch + 1);
        group.gradients[1] = 0.2f * (batch + 1);

        nimcp_optimizer_step_group(ctx, &group);
    }

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Statistics Integration Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, StatisticsTrackingAcrossTraining) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    config.clip_gradients = true;
    config.gradient_clip_value = 1.0f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {10.0f};

    // Run training with varying gradient magnitudes
    for (int i = 0; i < 100; i++) {
        float grad_magnitude = (i % 10 == 0) ? 10.0f : 0.5f;  // Occasional large gradient
        float gradients[] = {grad_magnitude};
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);

    EXPECT_EQ(stats.step_count, 100u);
    EXPECT_GT(stats.gradient_clips, 0u);  // Should have clipped some gradients
    EXPECT_GT(stats.total_compute_time_ns, 0u);
    EXPECT_GT(stats.avg_gradient_norm, 0.0);
    EXPECT_GT(stats.max_gradient_norm, stats.min_gradient_norm);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersIntegrationTest, ResetAndRetrainStatistics) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {1.0f};
    float gradients[] = {0.1f};

    // First training phase
    for (int i = 0; i < 50; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    nimcp_optimizer_stats_t stats1;
    nimcp_optimizer_get_stats(ctx, &stats1);
    EXPECT_EQ(stats1.step_count, 50u);

    // Reset state (this resets step count as well as optimizer internal state)
    nimcp_optimizer_reset_state(ctx);

    nimcp_optimizer_stats_t stats2;
    nimcp_optimizer_get_stats(ctx, &stats2);
    // After reset_state, step_count should be 0
    EXPECT_EQ(nimcp_optimizer_get_step(ctx), 0u);

    // Second training phase
    for (int i = 0; i < 30; i++) {
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    nimcp_optimizer_stats_t stats3;
    nimcp_optimizer_get_stats(ctx, &stats3);
    EXPECT_EQ(stats3.step_count, 30u);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, StressTest_LargeParameterCount) {
    const size_t param_count = 100000;  // 100K parameters

    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    std::vector<float> params = generateRandomData(param_count, -1.0f, 1.0f);
    std::vector<float> gradients = generateRandomData(param_count, -0.1f, 0.1f);

    auto start = std::chrono::high_resolution_clock::now();

    // Run 10 steps with large parameter count
    for (int i = 0; i < 10; i++) {
        nimcp_result_t err = nimcp_optimizer_step(ctx, params.data(), gradients.data(), param_count);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 1 second for 10 steps)
    EXPECT_LT(duration.count(), 1000);

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);
    EXPECT_EQ(stats.step_count, 10u);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersIntegrationTest, StressTest_ManySteps) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.001f;  // Small LR for stability

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {100.0f};
    float gradients[] = {1.0f};

    // Run 10000 steps
    for (int i = 0; i < 10000; i++) {
        gradients[0] = 2.0f * params[0];  // Gradient of x^2
        nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 1);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    nimcp_optimizer_stats_t stats;
    nimcp_optimizer_get_stats(ctx, &stats);
    EXPECT_EQ(stats.step_count, 10000u);

    // Should have converged
    EXPECT_LT(std::abs(params[0]), 1.0f);

    nimcp_optimizer_destroy(ctx);
}

TEST_F(OptimizersIntegrationTest, StressTest_AllOptimizerTypes) {
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
        ASSERT_NE(ctx, nullptr) << "Failed for type: " << nimcp_optimizer_type_name(type);

        float params[] = {5.0f, -3.0f, 2.0f};
        float gradients[] = {1.0f, 1.0f, 1.0f};

        for (int i = 0; i < 100; i++) {
            nimcp_result_t err = nimcp_optimizer_step(ctx, params, gradients, 3);
            EXPECT_EQ(err, NIMCP_SUCCESS) << "Step failed for: " << nimcp_optimizer_type_name(type);
        }

        nimcp_optimizer_stats_t stats;
        nimcp_optimizer_get_stats(ctx, &stats);
        EXPECT_EQ(stats.step_count, 100u) << "Wrong step count for: " << nimcp_optimizer_type_name(type);

        nimcp_optimizer_destroy(ctx);
    }
}

// ============================================================================
// Learning Rate Management Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, LearningRateScheduleSimulation) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.1f;

    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {10.0f};
    float gradients[] = {1.0f};

    // Simulate step decay schedule
    for (int epoch = 0; epoch < 30; epoch++) {
        // Decay LR every 10 epochs
        if (epoch > 0 && epoch % 10 == 0) {
            float current_lr = nimcp_optimizer_get_lr(ctx);
            nimcp_optimizer_set_lr(ctx, current_lr * 0.1f);
        }

        // Run steps for this epoch
        for (int step = 0; step < 10; step++) {
            gradients[0] = 2.0f * params[0];
            nimcp_optimizer_step(ctx, params, gradients, 1);
        }
    }

    // LR should have decayed twice (0.1 -> 0.01 -> 0.001)
    EXPECT_NEAR(nimcp_optimizer_get_lr(ctx), 0.001f, 1e-6f);

    nimcp_optimizer_destroy(ctx);
}

// ============================================================================
// Gradient Clipping Integration Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, GradientClippingStabilizesTraining) {
    // Without clipping
    nimcp_optimizer_config_t config_noclip = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config_noclip.params.sgd.learning_rate = 1.0f;
    config_noclip.clip_gradients = false;

    // With clipping
    nimcp_optimizer_config_t config_clip = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config_clip.params.sgd.learning_rate = 1.0f;
    config_clip.clip_gradients = true;
    config_clip.gradient_clip_value = 1.0f;

    nimcp_optimizer_context_t* ctx_noclip = nimcp_optimizer_create(&config_noclip, security_ctx, memory_mgr);
    nimcp_optimizer_context_t* ctx_clip = nimcp_optimizer_create(&config_clip, security_ctx, memory_mgr);

    ASSERT_NE(ctx_noclip, nullptr);
    ASSERT_NE(ctx_clip, nullptr);

    float params_noclip[] = {1.0f};
    float params_clip[] = {1.0f};

    // Large gradient that could cause instability
    float large_gradient[] = {100.0f};

    nimcp_optimizer_step(ctx_noclip, params_noclip, large_gradient, 1);
    nimcp_optimizer_step(ctx_clip, params_clip, large_gradient, 1);

    // Clipped update should be smaller
    float update_noclip = std::abs(params_noclip[0] - 1.0f);
    float update_clip = std::abs(params_clip[0] - 1.0f);

    EXPECT_LT(update_clip, update_noclip);

    nimcp_optimizer_destroy(ctx_noclip);
    nimcp_optimizer_destroy(ctx_clip);
}

// ============================================================================
// State Reset Integration Tests
// ============================================================================

TEST_F(OptimizersIntegrationTest, ResetStateAllowsRetraining) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    nimcp_optimizer_context_t* ctx = nimcp_optimizer_create(&config, security_ctx, memory_mgr);
    ASSERT_NE(ctx, nullptr);

    float params[] = {5.0f};
    float gradients[] = {1.0f};

    // First training run
    for (int i = 0; i < 50; i++) {
        gradients[0] = 2.0f * params[0];
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    float after_first = params[0];

    // Reset and retrain
    nimcp_optimizer_reset_state(ctx);
    params[0] = 5.0f;  // Reset params too

    for (int i = 0; i < 50; i++) {
        gradients[0] = 2.0f * params[0];
        nimcp_optimizer_step(ctx, params, gradients, 1);
    }

    float after_second = params[0];

    // Both runs should produce similar results
    EXPECT_NEAR(after_first, after_second, 0.1f);

    nimcp_optimizer_destroy(ctx);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
