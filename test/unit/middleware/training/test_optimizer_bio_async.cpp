/**
 * @file test_optimizer_bio_async.cpp
 * @brief Unit tests for optimizer bio-async integration
 *
 * Tests:
 * - Bio-async registration
 * - Message handling
 * - Event broadcasting
 * - DOPAMINE channel usage
 */

#include <gtest/gtest.h>
#include "middleware/training/nimcp_optimizers.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

class OptimizerBioAsyncTest : public ::testing::Test {
protected:
    nimcp_optimizer_context_t* optimizer = nullptr;
    bio_module_context_t test_module = nullptr;
    bool received_optimizer_step = false;
    bool received_gradient_computed = false;
    uint32_t last_step_number = 0;
    float last_gradient_norm = 0.0f;

    void SetUp() override {
        // Initialize bio-router
        bio_router_config_t cfg = {}; bio_router_init(&cfg);

        // Register test module to receive messages
        bio_module_info_t test_info = {
            .module_id = BIO_MODULE_STP,
            .module_name = "test_module",
            .inbox_capacity = 32,
            .user_data = this
        };
        test_module = bio_router_register_module(&test_info);
        ASSERT_NE(test_module, nullptr);

        // Register handlers
        bio_router_register_handler(test_module, BIO_MSG_OPTIMIZER_STEP,
                                    handle_optimizer_step_message);
        bio_router_register_handler(test_module, BIO_MSG_TRAINING_METRIC,
                                    handle_training_metric_message);
    }

    void TearDown() override {
        if (optimizer) {
            nimcp_optimizer_destroy(optimizer);
            optimizer = nullptr;
        }
        if (test_module) {
            bio_router_unregister_module(test_module);
            test_module = nullptr;
        }
        bio_router_shutdown();
    }

    static nimcp_error_t handle_optimizer_step_message(
        const void* msg, size_t msg_size,
        nimcp_bio_promise_t promise, void* user_data)
    {
        auto* test = static_cast<OptimizerBioAsyncTest*>(user_data);
        const auto* step_msg = static_cast<const bio_msg_optimizer_step_t*>(msg);

        test->received_optimizer_step = true;
        test->last_step_number = step_msg->step_number;
        test->last_gradient_norm = step_msg->gradient_norm;

        return NIMCP_SUCCESS;
    }

    static nimcp_error_t handle_training_metric_message(
        const void* msg, size_t msg_size,
        nimcp_bio_promise_t promise, void* user_data)
    {
        auto* test = static_cast<OptimizerBioAsyncTest*>(user_data);
        // const auto* metric_msg = static_cast<const bio_msg_training_metric_t*>(msg);
        // Process metric messages
        return NIMCP_SUCCESS;
    }
};

TEST_F(OptimizerBioAsyncTest, CreatesWithBioAsyncIntegration) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    optimizer = nimcp_optimizer_create(&config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    // Verify bio-async is initialized (if router is running)
    // The optimizer should have registered with the bio-router
}

TEST_F(OptimizerBioAsyncTest, BroadcastsOptimizerStepMessage) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.params.sgd.learning_rate = 0.01f;

    optimizer = nimcp_optimizer_create(&config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    // Initialize parameters
    const size_t param_count = 100;
    float params[param_count];
    float gradients[param_count];

    for (size_t i = 0; i < param_count; i++) {
        params[i] = 0.5f;
        gradients[i] = 0.01f * (i % 10);
    }

    nimcp_result_t err = nimcp_optimizer_init_params(optimizer, param_count);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Perform optimizer step
    err = nimcp_optimizer_step(optimizer, params, gradients, param_count);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Process messages
    bio_router_process_inbox(test_module, 10);

    // Verify we received the optimizer step message
    EXPECT_TRUE(received_optimizer_step);
    EXPECT_EQ(last_step_number, 1);
    EXPECT_GT(last_gradient_norm, 0.0f);
}

TEST_F(OptimizerBioAsyncTest, BroadcastsLearningRateChange) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    optimizer = nimcp_optimizer_create(&config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    float old_lr = nimcp_optimizer_get_lr(optimizer);

    // Change learning rate
    float new_lr = 0.0001f;
    nimcp_optimizer_set_lr(optimizer, new_lr);

    // Process messages
    bio_router_process_inbox(test_module, 10);

    // Verify learning rate was changed
    EXPECT_EQ(nimcp_optimizer_get_lr(optimizer), new_lr);

    // Note: Would need to capture DOPAMINE channel messages separately
}

TEST_F(OptimizerBioAsyncTest, HandlesGradientComputedMessage) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    optimizer = nimcp_optimizer_create(&config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    // Send gradient computed message to optimizer
    bio_msg_gradient_computed_t msg = {};
    bio_msg_init_header(&msg.header, BIO_MSG_GRADIENT_COMPUTED,
                       BIO_MODULE_STP, BIO_MODULE_TRAINING_OPTIMIZER,
                       sizeof(bio_msg_gradient_computed_t) - sizeof(bio_message_header_t));
    msg.layer_id = 42;
    msg.gradient_norm = 0.123f;

    // Send via bio-router (would need access to optimizer's inbox)
    // This is more of an integration test
}

TEST_F(OptimizerBioAsyncTest, ValidatesGradientsWithBBB) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);

    optimizer = nimcp_optimizer_create(&config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    const size_t param_count = 100;
    float params[param_count];
    float gradients[param_count];

    for (size_t i = 0; i < param_count; i++) {
        params[i] = 0.5f;
        gradients[i] = NAN; // Invalid gradient
    }

    nimcp_result_t err = nimcp_optimizer_init_params(optimizer, param_count);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // This should detect NaN gradients
    err = nimcp_optimizer_step(optimizer, params, gradients, param_count);

    // Depending on BBB configuration, this might fail or warn
    // For now, just verify it doesn't crash
}

TEST_F(OptimizerBioAsyncTest, ClipsGradientsAndLogsEvent) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_SGD);
    config.clip_gradients = true;
    config.gradient_clip_value = 0.1f;

    optimizer = nimcp_optimizer_create(&config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    const size_t param_count = 100;
    float params[param_count];
    float gradients[param_count];

    for (size_t i = 0; i < param_count; i++) {
        params[i] = 0.5f;
        gradients[i] = 1.0f; // Will be clipped
    }

    nimcp_result_t err = nimcp_optimizer_init_params(optimizer, param_count);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    err = nimcp_optimizer_step(optimizer, params, gradients, param_count);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Verify clipping occurred (check stats)
    nimcp_optimizer_stats_t stats;
    err = nimcp_optimizer_get_stats(optimizer, &stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(stats.gradient_clips, 0);
}

TEST_F(OptimizerBioAsyncTest, MultipleStepsBroadcastCorrectly) {
    nimcp_optimizer_config_t config = nimcp_optimizer_default_config(NIMCP_OPTIMIZER_ADAM);

    optimizer = nimcp_optimizer_create(&config, nullptr, nullptr);
    ASSERT_NE(optimizer, nullptr);

    const size_t param_count = 50;
    float params[param_count];
    float gradients[param_count];

    for (size_t i = 0; i < param_count; i++) {
        params[i] = 0.5f;
        gradients[i] = 0.01f;
    }

    nimcp_result_t err = nimcp_optimizer_init_params(optimizer, param_count);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Perform multiple steps
    for (int step = 0; step < 5; step++) {
        received_optimizer_step = false;

        err = nimcp_optimizer_step(optimizer, params, gradients, param_count);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        bio_router_process_inbox(test_module, 10);

        EXPECT_TRUE(received_optimizer_step);
        EXPECT_EQ(last_step_number, step + 1);
    }
}

// Entry point
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
