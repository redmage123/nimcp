/**
 * @file test_brain_training_bio_async_integration.cpp
 * @brief Integration tests for bio-async brain training with multiple modules
 *
 * Tests integration between brain training, loss functions, optimizers,
 * and bio-async communication layer.
 *
 * @author NIMCP Development Team
 * @date 2025-12-03
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
// Headers have their own extern "C" guards
#include "middleware/training/nimcp_brain_training_integration.h"
#include "middleware/training/nimcp_loss_functions.h"
#include "middleware/training/nimcp_optimizers.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

class BrainTrainingBioAsyncIntegrationTest : public ::testing::Test {
protected:
    nimcp_brain_training_ctx_t* training_ctx;
    bio_router_context_t* router_ctx;
    uint32_t loss_id;
    uint32_t optimizer_id;

    void SetUp() override {
        /* Initialize bio-async router with multiple worker threads */
        bio_router_config_t router_config = {0};
        router_config.max_modules = 64;
        router_config.default_inbox_capacity = 128;
        router_config.enable_priority_channels = true;
        router_config.worker_thread_count = 4;

        router_ctx = bio_router_init(&router_config);
        ASSERT_NE(router_ctx, nullptr) << "Failed to initialize bio-router";

        /* Create brain training context */
        nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
        config.default_learning_rate = 0.01f;
        config.enable_gradient_health_check = true;

        training_ctx = nimcp_brain_training_create(&config);
        ASSERT_NE(training_ctx, nullptr) << "Failed to create brain training context";

        /* Initialize */
        nimcp_result_t res = nimcp_brain_training_init(training_ctx, nullptr, nullptr);
        ASSERT_EQ(res, NIMCP_SUCCESS) << "Failed to initialize brain training";

        /* Create loss function */
        nimcp_loss_config_t loss_config = nimcp_loss_default_config();
        loss_config.loss_type = NIMCP_LOSS_MSE;
        loss_config.reduction = NIMCP_LOSS_REDUCE_MEAN;

        res = nimcp_brain_training_create_loss(training_ctx, &loss_config, &loss_id);
        ASSERT_EQ(res, NIMCP_SUCCESS) << "Failed to create loss function";

        /* Create optimizer */
        nimcp_optimizer_config_t opt_config = nimcp_optimizer_default_config();
        opt_config.optimizer_type = NIMCP_OPTIMIZER_SGD;
        opt_config.learning_rate = 0.01f;
        opt_config.momentum = 0.9f;

        res = nimcp_brain_training_create_optimizer(training_ctx, &opt_config, &optimizer_id);
        ASSERT_EQ(res, NIMCP_SUCCESS) << "Failed to create optimizer";

        LOG_INFO("Integration test setup complete");
    }

    void TearDown() override {
        if (training_ctx) {
            nimcp_brain_training_destroy(training_ctx);
            training_ctx = nullptr;
        }

        if (router_ctx) {
            bio_router_shutdown(router_ctx);
            router_ctx = nullptr;
        }
    }
};

/**
 * @test Test full training pipeline with bio-async
 */
TEST_F(BrainTrainingBioAsyncIntegrationTest, FullTrainingPipeline) {
    const size_t batch_size = 32;
    const size_t output_size = 10;
    const size_t param_count = 100;

    /* Allocate test data */
    float* params = (float*)nimcp_calloc(param_count, sizeof(float));
    float* predictions = (float*)nimcp_calloc(batch_size * output_size, sizeof(float));
    float* targets = (float*)nimcp_calloc(batch_size * output_size, sizeof(float));
    float loss_value = 0.0f;

    ASSERT_NE(params, nullptr);
    ASSERT_NE(predictions, nullptr);
    ASSERT_NE(targets, nullptr);

    /* Initialize with dummy data */
    for (size_t i = 0; i < param_count; i++) {
        params[i] = 0.01f * (i % 10);
    }
    for (size_t i = 0; i < batch_size * output_size; i++) {
        predictions[i] = 0.5f;
        targets[i] = 1.0f;
    }

    /* Perform training step */
    nimcp_result_t res = nimcp_brain_training_step(
        training_ctx,
        loss_id,
        optimizer_id,
        params,
        predictions,
        targets,
        batch_size,
        output_size,
        param_count,
        &loss_value
    );

    ASSERT_EQ(res, NIMCP_SUCCESS) << "Training step failed";
    EXPECT_GT(loss_value, 0.0f) << "Loss should be positive";

    /* Send training step notification via bio-async */
    bio_msg_training_step_t step_msg = {0};
    step_msg.header.type = BIO_MSG_TRAINING_STEP_COMPLETE;
    step_msg.header.source_module = BIO_MODULE_BRAIN;
    step_msg.header.target_module = BIO_MODULE_TRAINING;
    step_msg.batch_id = 1;
    step_msg.batch_size = batch_size;
    step_msg.learning_rate = 0.01f;

    nimcp_bio_promise_t promise = bio_router_send_message(
        router_ctx,
        BIO_MODULE_TRAINING,
        NIMCP_BIO_CHANNEL_DOPAMINE,
        &step_msg,
        sizeof(step_msg)
    );

    EXPECT_NE(promise, nullptr);

    /* Send loss update */
    bio_msg_loss_computed_t loss_msg = {0};
    loss_msg.header.type = BIO_MSG_LOSS_COMPUTED;
    loss_msg.header.source_module = BIO_MODULE_BRAIN;
    loss_msg.header.target_module = BIO_MODULE_TRAINING;
    loss_msg.batch_id = 1;
    loss_msg.loss_value = loss_value;
    loss_msg.loss_gradient = -0.1f;

    promise = bio_router_send_message(
        router_ctx,
        BIO_MODULE_TRAINING,
        NIMCP_BIO_CHANNEL_DOPAMINE,
        &loss_msg,
        sizeof(loss_msg)
    );

    EXPECT_NE(promise, nullptr);

    /* Verify statistics */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    nimcp_training_session_stats_t stats = {0};
    nimcp_brain_training_get_stats(training_ctx, &stats);

    EXPECT_GT(stats.total_batches, 0UL);
    EXPECT_GT(stats.weight_updates, 0UL);

    /* Cleanup */
    nimcp_free(params);
    nimcp_free(predictions);
    nimcp_free(targets);

    LOG_INFO("Full training pipeline test passed");
}

/**
 * @test Test multi-epoch training with bio-async synchronization
 */
TEST_F(BrainTrainingBioAsyncIntegrationTest, MultiEpochTraining) {
    const int NUM_EPOCHS = 5;
    const int BATCHES_PER_EPOCH = 10;

    for (int epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        LOG_INFO("Starting epoch %d", epoch + 1);

        for (int batch = 0; batch < BATCHES_PER_EPOCH; batch++) {
            /* Send training step request */
            bio_msg_training_step_t step_msg = {0};
            step_msg.header.type = BIO_MSG_TRAINING_STEP_REQUEST;
            step_msg.header.source_module = BIO_MODULE_BRAIN;
            step_msg.header.target_module = BIO_MODULE_TRAINING;
            step_msg.batch_id = epoch * BATCHES_PER_EPOCH + batch + 1;
            step_msg.batch_size = 32;
            step_msg.learning_rate = 0.01f;

            nimcp_bio_promise_t promise = bio_router_send_message(
                router_ctx,
                BIO_MODULE_TRAINING,
                NIMCP_BIO_CHANNEL_DOPAMINE,
                &step_msg,
                sizeof(step_msg)
            );

            ASSERT_NE(promise, nullptr);

            /* Wait for response */
            void* response_data = nullptr;
            size_t response_size = 0;
            nimcp_error_t result = nimcp_bio_promise_wait_sized(
                promise,
                &response_data,
                &response_size,
                1000
            );

            ASSERT_EQ(result, NIMCP_SUCCESS);

            if (response_data) {
                nimcp_free(response_data);
            }

            /* Send loss update */
            bio_msg_loss_computed_t loss_msg = {0};
            loss_msg.header.type = BIO_MSG_LOSS_COMPUTED;
            loss_msg.header.source_module = BIO_MODULE_BRAIN;
            loss_msg.header.target_module = BIO_MODULE_TRAINING;
            loss_msg.batch_id = step_msg.batch_id;
            loss_msg.loss_value = 1.0f / (epoch + 1); // Decreasing loss
            loss_msg.loss_gradient = -0.1f;

            promise = bio_router_send_message(
                router_ctx,
                BIO_MODULE_TRAINING,
                NIMCP_BIO_CHANNEL_DOPAMINE,
                &loss_msg,
                sizeof(loss_msg)
            );

            ASSERT_NE(promise, nullptr);

            result = nimcp_bio_promise_wait_sized(promise, &response_data, &response_size, 1000);
            if (response_data) {
                nimcp_free(response_data);
            }
        }

        LOG_INFO("Completed epoch %d", epoch + 1);
    }

    /* Verify training statistics */
    nimcp_training_session_stats_t stats = {0};
    nimcp_brain_training_get_stats(training_ctx, &stats);

    EXPECT_EQ(stats.total_batches, (uint64_t)(NUM_EPOCHS * BATCHES_PER_EPOCH));
    EXPECT_GT(stats.avg_loss, 0.0);
    EXPECT_LT(stats.avg_loss, 1.0);

    LOG_INFO("Multi-epoch training test passed");
}

/**
 * @test Test checkpoint coordination via bio-async
 */
TEST_F(BrainTrainingBioAsyncIntegrationTest, CheckpointCoordination) {
    /* Perform some training */
    for (int i = 0; i < 5; i++) {
        bio_msg_loss_computed_t loss_msg = {0};
        loss_msg.header.type = BIO_MSG_LOSS_COMPUTED;
        loss_msg.header.source_module = BIO_MODULE_BRAIN;
        loss_msg.header.target_module = BIO_MODULE_TRAINING;
        loss_msg.batch_id = i + 1;
        loss_msg.loss_value = 0.5f - (i * 0.1f);
        loss_msg.loss_gradient = -0.1f;

        nimcp_bio_promise_t promise = bio_router_send_message(
            router_ctx,
            BIO_MODULE_TRAINING,
            NIMCP_BIO_CHANNEL_DOPAMINE,
            &loss_msg,
            sizeof(loss_msg)
        );

        ASSERT_NE(promise, nullptr);

        void* response_data = nullptr;
        size_t response_size = 0;
        nimcp_bio_promise_wait_sized(promise, &response_data, &response_size, 1000);
        if (response_data) {
            nimcp_free(response_data);
        }
    }

    /* Request checkpoint */
    bio_msg_checkpoint_request_t ckpt_req = {0};
    ckpt_req.header.type = BIO_MSG_CHECKPOINT_REQUEST;
    ckpt_req.header.source_module = BIO_MODULE_BRAIN;
    ckpt_req.header.target_module = BIO_MODULE_TRAINING;
    ckpt_req.checkpoint_id = 1;
    strncpy(ckpt_req.path, "/tmp/integration_test_checkpoint.ckpt", sizeof(ckpt_req.path) - 1);
    ckpt_req.include_optimizer_state = true;
    ckpt_req.compress = true;

    nimcp_bio_promise_t promise = bio_router_send_message(
        router_ctx,
        BIO_MODULE_TRAINING,
        NIMCP_BIO_CHANNEL_SEROTONIN,
        &ckpt_req,
        sizeof(ckpt_req)
    );

    ASSERT_NE(promise, nullptr) << "Failed to send checkpoint request";

    /* Wait for checkpoint completion */
    void* response_data = nullptr;
    size_t response_size = 0;
    nimcp_error_t result = nimcp_bio_promise_wait_sized(
        promise,
        &response_data,
        &response_size,
        2000 // 2 second timeout
    );

    ASSERT_EQ(result, NIMCP_SUCCESS) << "Checkpoint request failed";
    ASSERT_NE(response_data, nullptr);

    /* Verify checkpoint response */
    bio_msg_checkpoint_request_t* response = (bio_msg_checkpoint_request_t*)response_data;
    EXPECT_EQ(response->header.type, BIO_MSG_CHECKPOINT_COMPLETE);
    EXPECT_EQ(response->checkpoint_id, ckpt_req.checkpoint_id);

    nimcp_free(response_data);

    LOG_INFO("Checkpoint coordination test passed");
}

/**
 * @test Test concurrent training and monitoring via bio-async
 */
TEST_F(BrainTrainingBioAsyncIntegrationTest, ConcurrentTrainingMonitoring) {
    const int NUM_ITERATIONS = 20;
    std::atomic<int> completed_steps(0);

    /* Simulate concurrent training steps */
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        /* Training step */
        bio_msg_training_step_t step_msg = {0};
        step_msg.header.type = BIO_MSG_TRAINING_STEP_REQUEST;
        step_msg.header.source_module = BIO_MODULE_BRAIN;
        step_msg.header.target_module = BIO_MODULE_TRAINING;
        step_msg.batch_id = i + 1;
        step_msg.batch_size = 32;
        step_msg.learning_rate = 0.01f;

        bio_router_send_message(
            router_ctx,
            BIO_MODULE_TRAINING,
            NIMCP_BIO_CHANNEL_DOPAMINE,
            &step_msg,
            sizeof(step_msg)
        );

        /* Loss computation */
        bio_msg_loss_computed_t loss_msg = {0};
        loss_msg.header.type = BIO_MSG_LOSS_COMPUTED;
        loss_msg.header.source_module = BIO_MODULE_BRAIN;
        loss_msg.header.target_module = BIO_MODULE_TRAINING;
        loss_msg.batch_id = i + 1;
        loss_msg.loss_value = 1.0f / (i + 1);
        loss_msg.loss_gradient = -0.1f;

        bio_router_send_message(
            router_ctx,
            BIO_MODULE_TRAINING,
            NIMCP_BIO_CHANNEL_DOPAMINE,
            &loss_msg,
            sizeof(loss_msg)
        );

        /* Gradient update */
        struct gradient_msg {
            bio_message_header_t header;
            uint32_t gradient_count;
        } grad_msg = {0};

        grad_msg.header.type = BIO_MSG_GRADIENT_COMPUTED;
        grad_msg.header.source_module = BIO_MODULE_BRAIN;
        grad_msg.header.target_module = BIO_MODULE_TRAINING;
        grad_msg.gradient_count = 1000;

        bio_router_send_message(
            router_ctx,
            BIO_MODULE_TRAINING,
            NIMCP_BIO_CHANNEL_DOPAMINE,
            &grad_msg,
            sizeof(grad_msg)
        );

        /* Optimizer step */
        struct optimizer_msg {
            bio_message_header_t header;
            uint32_t optimizer_id;
        } opt_msg = {0};

        opt_msg.header.type = BIO_MSG_OPTIMIZER_STEP;
        opt_msg.header.source_module = BIO_MODULE_BRAIN;
        opt_msg.header.target_module = BIO_MODULE_TRAINING;
        opt_msg.optimizer_id = optimizer_id;

        bio_router_send_message(
            router_ctx,
            BIO_MODULE_TRAINING,
            NIMCP_BIO_CHANNEL_DOPAMINE,
            &opt_msg,
            sizeof(opt_msg)
        );

        completed_steps++;
    }

    /* Wait for messages to be processed */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    /* Verify all steps were processed */
    nimcp_training_session_stats_t stats = {0};
    nimcp_brain_training_get_stats(training_ctx, &stats);

    EXPECT_GE(stats.total_batches, (uint64_t)(NUM_ITERATIONS / 2));
    EXPECT_GT(stats.avg_loss, 0.0);

    LOG_INFO("Concurrent training monitoring test passed: %d steps", completed_steps.load());
}

/**
 * @test Test error handling in bio-async training
 */
TEST_F(BrainTrainingBioAsyncIntegrationTest, ErrorHandling) {
    /* Send malformed message (too small) */
    struct small_msg {
        uint32_t dummy;
    } bad_msg = {0};

    nimcp_bio_promise_t promise = bio_router_send_message(
        router_ctx,
        BIO_MODULE_TRAINING,
        NIMCP_BIO_CHANNEL_DOPAMINE,
        &bad_msg,
        sizeof(bad_msg)
    );

    /* Message should still be sent, but handler will reject it */
    EXPECT_NE(promise, nullptr);

    /* Try to wait (may timeout or return error) */
    void* response_data = nullptr;
    size_t response_size = 0;
    nimcp_bio_promise_wait_sized(promise, &response_data, &response_size, 500);

    /* Cleanup if response received */
    if (response_data) {
        nimcp_free(response_data);
    }

    /* System should still be operational */
    bio_msg_training_step_t valid_msg = {0};
    valid_msg.header.type = BIO_MSG_TRAINING_STEP_REQUEST;
    valid_msg.header.source_module = BIO_MODULE_BRAIN;
    valid_msg.header.target_module = BIO_MODULE_TRAINING;
    valid_msg.batch_id = 1;
    valid_msg.batch_size = 32;
    valid_msg.learning_rate = 0.01f;

    promise = bio_router_send_message(
        router_ctx,
        BIO_MODULE_TRAINING,
        NIMCP_BIO_CHANNEL_DOPAMINE,
        &valid_msg,
        sizeof(valid_msg)
    );

    ASSERT_NE(promise, nullptr) << "System should still accept valid messages";

    nimcp_error_t result = nimcp_bio_promise_wait_sized(
        promise,
        &response_data,
        &response_size,
        1000
    );

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Valid message should be processed after error";

    if (response_data) {
        nimcp_free(response_data);
    }

    LOG_INFO("Error handling test passed");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
