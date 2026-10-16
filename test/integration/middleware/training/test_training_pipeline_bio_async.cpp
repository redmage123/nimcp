/**
 * @file test_training_pipeline_bio_async.cpp
 * @brief Integration tests for complete training pipeline with bio-async
 *
 * Tests end-to-end training with:
 * - Optimizer
 * - Loss function
 * - LR scheduler
 * - Gradient manager
 * - Bio-async messaging between all components
 */

#include <gtest/gtest.h>
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "middleware/training/nimcp_lr_scheduler.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include <vector>
#include <cmath>

class TrainingPipelineBioAsyncTest : public ::testing::Test {
protected:
    nimcp_optimizer_context_t* optimizer = nullptr;
    nimcp_loss_context_t* loss_ctx = nullptr;
    bio_module_context_t test_module = nullptr;

    // Track received messages
    struct MessageStats {
        uint32_t optimizer_steps = 0;
        uint32_t loss_computed = 0;
        uint32_t gradient_computed = 0;
        uint32_t dopamine_signals = 0;
    } msg_stats;

    void SetUp() override {
        // Initialize bio-router
        bio_router_config_t cfg = {0}; bio_router_init(&cfg);

        // Register test module as observer
        bio_module_info_t test_info = {
            .module_id = BIO_MODULE_STP,
            .module_name = "integration_test",
            .inbox_capacity = 128,
            .user_data = this
        };
        test_module = bio_router_register_module(&test_info);
        ASSERT_NE(test_module, nullptr);

        // Subscribe to all training messages
        bio_router_register_handler(test_module, BIO_MSG_OPTIMIZER_STEP,
                                    handle_optimizer_step);
        bio_router_register_handler(test_module, BIO_MSG_LOSS_COMPUTED,
                                    handle_loss_computed);
        bio_router_register_handler(test_module, BIO_MSG_GRADIENT_COMPUTED,
                                    handle_gradient_computed);
        bio_router_register_handler(test_module, BIO_MSG_TRAINING_METRIC,
                                    handle_training_metric);
    }

    void TearDown() override {
        if (optimizer) {
            nimcp_optimizer_destroy(optimizer);
        }
        if (loss_ctx) {
            nimcp_loss_destroy(loss_ctx);
        }
        if (test_module) {
            bio_router_unregister_module(test_module);
        }
        bio_router_shutdown();
    }

    static nimcp_error_t handle_optimizer_step(
        const void* msg, size_t msg_size,
        nimcp_bio_promise_t promise, void* user_data)
    {
        auto* test = static_cast<TrainingPipelineBioAsyncTest*>(user_data);
        test->msg_stats.optimizer_steps++;
        return NIMCP_SUCCESS;
    }

    static nimcp_error_t handle_loss_computed(
        const void* msg, size_t msg_size,
        nimcp_bio_promise_t promise, void* user_data)
    {
        auto* test = static_cast<TrainingPipelineBioAsyncTest*>(user_data);
        test->msg_stats.loss_computed++;
        return NIMCP_SUCCESS;
    }

    static nimcp_error_t handle_gradient_computed(
        const void* msg, size_t msg_size,
        nimcp_bio_promise_t promise, void* user_data)
    {
        auto* test = static_cast<TrainingPipelineBioAsyncTest*>(user_data);
        test->msg_stats.gradient_computed++;
        return NIMCP_SUCCESS;
    }

    static nimcp_error_t handle_training_metric(
        const void* msg, size_t msg_size,
        nimcp_bio_promise_t promise, void* user_data)
    {
        auto* test = static_cast<TrainingPipelineBioAsyncTest*>(user_data);
        // Check if from DOPAMINE channel
        // (would need channel info in message)
        return NIMCP_SUCCESS;
    }

    void ProcessAllMessages() {
        bio_router_process_inbox(test_module, 1000);
        // Give time for async processing
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};

TEST_F(TrainingPipelineBioAsyncTest, SimpleTrainingLoopWithBioAsync) {
    // Setup: Create a simple linear regression problem
    // y = 2x + 3
    const size_t num_samples = 100;
    const size_t num_features = 1;
    const size_t num_params = 2; // weight and bias

    std::vector<float> X(num_samples);
    std::vector<float> y(num_samples);

    // Generate synthetic data
    for (size_t i = 0; i < num_samples; i++) {
        X[i] = static_cast<float>(i) / num_samples;
        y[i] = 2.0f * X[i] + 3.0f + (rand() % 100 - 50) / 1000.0f; // Add noise
    }

    // Initialize parameters
    float params[num_params] = {0.0f, 0.0f}; // [weight, bias]
    float gradients[num_params];

    // Create optimizer
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    opt_config.params.sgd.learning_rate = 0.01f;
    optimizer = nimcp_optimizer_create(&opt_config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    nimcp_result_t err = nimcp_optimizer_init_params(optimizer, num_params);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Create loss function
    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    loss_ctx = nimcp_loss_create(&loss_config, nullptr, nullptr);
    ASSERT_NE(loss_ctx, nullptr);

    err = nimcp_loss_init(loss_ctx);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Training loop
    const int num_epochs = 10;
    float prev_loss = INFINITY;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float epoch_loss = 0.0f;

        for (size_t i = 0; i < num_samples; i++) {
            // Forward pass: y_pred = w*x + b
            float prediction = params[0] * X[i] + params[1];
            float target = y[i];

            // Compute loss
            nimcp_loss_result_t loss_result;
            err = nimcp_loss_forward(loss_ctx, &prediction, &target, 1, 1, &loss_result);
            ASSERT_EQ(err, NIMCP_SUCCESS);
            epoch_loss += loss_result.loss_value;

            // Backward pass: compute gradients
            err = nimcp_loss_backward(loss_ctx, &prediction, &target, 1, 1, gradients);
            ASSERT_EQ(err, NIMCP_SUCCESS);

            // Optimizer step
            err = nimcp_optimizer_step(optimizer, params, gradients, num_params);
            ASSERT_EQ(err, NIMCP_SUCCESS);

            // Process bio-async messages periodically
            if (i % 10 == 0) {
                ProcessAllMessages();
            }
        }

        epoch_loss /= num_samples;

        // Process all pending messages
        ProcessAllMessages();

        // Verify loss is decreasing
        if (epoch > 0) {
            EXPECT_LT(epoch_loss, prev_loss) << "Loss should decrease over epochs";
        }
        prev_loss = epoch_loss;
    }

    // Verify we received bio-async messages
    EXPECT_GT(msg_stats.optimizer_steps, 0) << "Should receive optimizer step messages";

    // Verify parameters converged to reasonable values
    EXPECT_NEAR(params[0], 2.0f, 0.5f) << "Weight should converge to ~2.0";
    EXPECT_NEAR(params[1], 3.0f, 0.5f) << "Bias should converge to ~3.0";
}

TEST_F(TrainingPipelineBioAsyncTest, AllModulesCommunicateViaBioAsync) {
    // Create all training components
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);
    optimizer = nimcp_optimizer_create(&opt_config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    nimcp_loss_config_t loss_config = nimcp_loss_default_config(NIMCP_LOSS_MSE);
    loss_ctx = nimcp_loss_create(&loss_config, nullptr, nullptr);
    ASSERT_NE(loss_ctx, nullptr);

    // Simulate a training step
    const size_t param_count = 100;
    float params[param_count];
    float gradients[param_count];
    float predictions[param_count];
    float targets[param_count];

    for (size_t i = 0; i < param_count; i++) {
        params[i] = 0.5f;
        gradients[i] = 0.01f;
        predictions[i] = 0.5f;
        targets[i] = 0.6f;
    }

    // Initialize
    nimcp_optimizer_init_params(optimizer, param_count);

    // Forward pass
    nimcp_loss_result_t loss_result;
    nimcp_loss_forward(loss_ctx, predictions, targets, 1, param_count, &loss_result);

    // Backward pass
    nimcp_loss_backward(loss_ctx, predictions, targets, 1, param_count, gradients);

    // Optimizer step
    nimcp_optimizer_step(optimizer, params, gradients, param_count);

    // Process all messages
    ProcessAllMessages();

    // Verify message flow
    // (Exact counts depend on implementation)
    EXPECT_GT(msg_stats.optimizer_steps, 0);
}

TEST_F(TrainingPipelineBioAsyncTest, DOPAMINEChannelSignalsImprovements) {
    nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    opt_config.params.sgd.learning_rate = 0.1f;
    optimizer = nimcp_optimizer_create(&opt_config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    nimcp_optimizer_init_params(optimizer, 10);

    // Simulate improving gradients
    float params[10] = {0.5f};
    float gradients1[10], gradients2[10];

    for (int i = 0; i < 10; i++) {
        gradients1[i] = 1.0f;  // High gradient
        gradients2[i] = 0.1f;  // Lower gradient (improvement)
    }

    // First step with high gradients
    nimcp_optimizer_step(optimizer, params, gradients1, 10);
    ProcessAllMessages();

    // Second step with lower gradients (should trigger DOPAMINE)
    nimcp_optimizer_step(optimizer, params, gradients2, 10);
    ProcessAllMessages();

    // TODO: Verify DOPAMINE channel messages
    // Would need separate tracking for channel-specific messages
}

// Entry point
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
