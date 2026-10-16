/**
 * @file test_brain_training_bio_async.cpp
 * @brief Unit tests for bio-async integration in brain training module
 *
 * Tests bio-async message handling for training step requests, loss computation,
 * gradient computation, optimizer steps, and checkpoint requests.
 *
 * @author NIMCP Development Team
 * @date 2025-12-03
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "middleware/training/nimcp_brain_training_integration.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

class BrainTrainingBioAsyncTest : public ::testing::Test {
protected:
    nimcp_brain_training_ctx_t* training_ctx;
    bio_module_context_t sender_ctx;

    void SetUp() override {
        /* Initialize bio-async router */
        bio_router_config_t router_config = {};
        router_config.max_modules = 32;
        router_config.inbox_capacity = 64;
        router_config.worker_threads = 2;

        nimcp_error_t rc = bio_router_init(&router_config);
        ASSERT_EQ(rc, NIMCP_SUCCESS) << "Failed to initialize bio-router";

        /* Register a sender module so we can send messages */
        bio_module_info_t sender_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "TestSender",
            .inbox_capacity = 64,
            .user_data = nullptr
        };
        sender_ctx = bio_router_register_module(&sender_info);
        ASSERT_NE(sender_ctx, nullptr) << "Failed to register sender module";

        /* Create brain training context with bio-async */
        nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
        training_ctx = nimcp_brain_training_create(&config);
        ASSERT_NE(training_ctx, nullptr) << "Failed to create brain training context";

        /* Initialize with bio-async support */
        nimcp_result_t res = nimcp_brain_training_init(training_ctx, nullptr, nullptr);
        ASSERT_EQ(res, NIMCP_SUCCESS) << "Failed to initialize brain training";
    }

    void TearDown() override {
        if (training_ctx) {
            nimcp_brain_training_destroy(training_ctx);
            training_ctx = nullptr;
        }

        if (sender_ctx) {
            bio_router_unregister_module(sender_ctx);
            sender_ctx = nullptr;
        }

        bio_router_shutdown();
    }
};

/**
 * @test Verify bio-async module registration
 */
TEST_F(BrainTrainingBioAsyncTest, ModuleRegistration) {
    /* Verify router is initialized (module registration happens internally) */
    EXPECT_TRUE(bio_router_is_initialized());
    LOG_INFO("Brain training module is registered with bio-router");
}

/**
 * @test Test training step request message handling
 */
TEST_F(BrainTrainingBioAsyncTest, TrainingStepRequest) {
    /* Create training step request message */
    bio_msg_training_step_t request = {};
    request.header.type = BIO_MSG_TRAINING_STEP_REQUEST;
    request.header.source_module = BIO_MODULE_BRAIN;
    request.header.target_module = BIO_MODULE_TRAINING;
    request.header.timestamp_us = nimcp_time_now_us();
    request.batch_id = 1;
    request.batch_size = 32;
    request.learning_rate = 0.001f;
    request.optimizer_type = 1; // ADAM

    /* Send message asynchronously */
    nimcp_bio_promise_t promise = bio_router_send_async(
        sender_ctx,
        &request,
        sizeof(request),
        BIO_CHANNEL_DOPAMINE
    );

    ASSERT_NE(promise, nullptr) << "Failed to send training step request";

    /* Get future and wait for response */
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    bio_msg_training_step_t response = {};
    nimcp_error_t wait_result = nimcp_bio_future_wait(
        future,
        &response,
        1000 // 1 second timeout
    );

    ASSERT_EQ(wait_result, NIMCP_SUCCESS) << "Failed to receive training step response";

    /* Verify response */
    EXPECT_EQ(response.header.type, BIO_MSG_TRAINING_STEP_COMPLETE);
    EXPECT_EQ(response.batch_id, request.batch_id);
    EXPECT_EQ(response.batch_size, request.batch_size);
    EXPECT_FLOAT_EQ(response.learning_rate, request.learning_rate);

    /* Cleanup */
    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);

    LOG_INFO("Training step request handled successfully");
}

/**
 * @test Test loss computed message handling
 */
TEST_F(BrainTrainingBioAsyncTest, LossComputed) {
    /* Create loss computed message */
    bio_msg_loss_computed_t loss_msg = {};
    loss_msg.header.type = BIO_MSG_LOSS_COMPUTED;
    loss_msg.header.source_module = BIO_MODULE_BRAIN;
    loss_msg.header.target_module = BIO_MODULE_TRAINING;
    loss_msg.header.timestamp_us = nimcp_time_now_us();
    loss_msg.batch_id = 1;
    loss_msg.loss_value = 0.5f;
    loss_msg.loss_gradient = -0.1f;
    loss_msg.loss_type = 1; // MSE

    /* Send message */
    nimcp_bio_promise_t promise = bio_router_send_async(
        sender_ctx,
        &loss_msg,
        sizeof(loss_msg),
        BIO_CHANNEL_DOPAMINE
    );

    ASSERT_NE(promise, nullptr) << "Failed to send loss computed message";

    /* Wait for acknowledgment */
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    bio_msg_loss_computed_t response = {};
    nimcp_error_t wait_result = nimcp_bio_future_wait(
        future,
        &response,
        1000
    );

    ASSERT_EQ(wait_result, NIMCP_SUCCESS) << "Failed to receive loss acknowledgment";

    /* Verify response */
    EXPECT_EQ(response.header.type, BIO_MSG_LOSS_COMPUTED);
    EXPECT_EQ(response.batch_id, loss_msg.batch_id);
    EXPECT_FLOAT_EQ(response.loss_value, loss_msg.loss_value);

    /* Cleanup */
    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);

    /* Verify statistics updated */
    nimcp_training_session_stats_t stats = {};
    nimcp_brain_training_get_stats(training_ctx, &stats);
    EXPECT_GT(stats.total_loss, 0.0);
    EXPECT_GT(stats.avg_loss, 0.0);

    LOG_INFO("Loss computed message handled successfully");
}

/**
 * @test Test gradient computed message handling
 */
TEST_F(BrainTrainingBioAsyncTest, GradientComputed) {
    /* Create gradient computed message (simple structure for now) */
    struct gradient_msg {
        bio_message_header_t header;
        uint32_t gradient_count;
    } grad_msg = {};

    grad_msg.header.type = BIO_MSG_GRADIENT_COMPUTED;
    grad_msg.header.source_module = BIO_MODULE_BRAIN;
    grad_msg.header.target_module = BIO_MODULE_TRAINING;
    grad_msg.header.timestamp_us = nimcp_time_now_us();
    grad_msg.gradient_count = 1000;

    /* Send message */
    nimcp_bio_promise_t promise = bio_router_send_async(
        sender_ctx,
        &grad_msg,
        sizeof(grad_msg),
        BIO_CHANNEL_DOPAMINE
    );

    ASSERT_NE(promise, nullptr) << "Failed to send gradient computed message";

    /* Wait for acknowledgment */
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    uint8_t response_buf[256] = {};
    nimcp_error_t wait_result = nimcp_bio_future_wait(
        future,
        response_buf,
        1000
    );

    ASSERT_EQ(wait_result, NIMCP_SUCCESS) << "Failed to receive gradient acknowledgment";

    /* Cleanup */
    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);

    LOG_INFO("Gradient computed message handled successfully");
}

/**
 * @test Test optimizer step message handling
 */
TEST_F(BrainTrainingBioAsyncTest, OptimizerStep) {
    /* Create optimizer step message */
    struct optimizer_msg {
        bio_message_header_t header;
        uint32_t optimizer_id;
    } opt_msg = {};

    opt_msg.header.type = BIO_MSG_OPTIMIZER_STEP;
    opt_msg.header.source_module = BIO_MODULE_BRAIN;
    opt_msg.header.target_module = BIO_MODULE_TRAINING;
    opt_msg.header.timestamp_us = nimcp_time_now_us();
    opt_msg.optimizer_id = 1;

    /* Send message */
    nimcp_bio_promise_t promise = bio_router_send_async(
        sender_ctx,
        &opt_msg,
        sizeof(opt_msg),
        BIO_CHANNEL_DOPAMINE
    );

    ASSERT_NE(promise, nullptr) << "Failed to send optimizer step message";

    /* Wait for acknowledgment */
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    uint8_t response_buf[256] = {};
    nimcp_error_t wait_result = nimcp_bio_future_wait(
        future,
        response_buf,
        1000
    );

    ASSERT_EQ(wait_result, NIMCP_SUCCESS) << "Failed to receive optimizer acknowledgment";

    /* Cleanup */
    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);

    /* Verify statistics updated */
    nimcp_training_session_stats_t stats = {};
    nimcp_brain_training_get_stats(training_ctx, &stats);
    EXPECT_GT(stats.weight_updates, 0UL);

    LOG_INFO("Optimizer step message handled successfully");
}

/**
 * @test Test checkpoint request message handling
 */
TEST_F(BrainTrainingBioAsyncTest, CheckpointRequest) {
    /* Create checkpoint request message */
    bio_msg_checkpoint_request_t ckpt_req = {};
    ckpt_req.header.type = BIO_MSG_CHECKPOINT_REQUEST;
    ckpt_req.header.source_module = BIO_MODULE_BRAIN;
    ckpt_req.header.target_module = BIO_MODULE_TRAINING;
    ckpt_req.header.timestamp_us = nimcp_time_now_us();
    ckpt_req.checkpoint_id = 1;
    strncpy(ckpt_req.path, "/tmp/checkpoint_test.ckpt", sizeof(ckpt_req.path) - 1);
    ckpt_req.include_optimizer_state = true;
    ckpt_req.compress = false;

    /* Send message */
    nimcp_bio_promise_t promise = bio_router_send_async(
        sender_ctx,
        &ckpt_req,
        sizeof(ckpt_req),
        BIO_CHANNEL_SEROTONIN
    );

    ASSERT_NE(promise, nullptr) << "Failed to send checkpoint request";

    /* Wait for response */
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    bio_msg_checkpoint_request_t response = {};
    nimcp_error_t wait_result = nimcp_bio_future_wait(
        future,
        &response,
        1000
    );

    ASSERT_EQ(wait_result, NIMCP_SUCCESS) << "Failed to receive checkpoint response";

    /* Verify response */
    EXPECT_EQ(response.header.type, BIO_MSG_CHECKPOINT_COMPLETE);
    EXPECT_EQ(response.checkpoint_id, ckpt_req.checkpoint_id);
    EXPECT_STREQ(response.path, ckpt_req.path);

    /* Cleanup */
    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);

    LOG_INFO("Checkpoint request handled successfully");
}

/**
 * @test Test multiple concurrent messages
 */
TEST_F(BrainTrainingBioAsyncTest, ConcurrentMessages) {
    const int NUM_MESSAGES = 10;
    nimcp_bio_promise_t promises[NUM_MESSAGES];

    /* Send multiple training step requests */
    for (int i = 0; i < NUM_MESSAGES; i++) {
        bio_msg_training_step_t request = {};
        request.header.type = BIO_MSG_TRAINING_STEP_REQUEST;
        request.header.source_module = BIO_MODULE_BRAIN;
        request.header.target_module = BIO_MODULE_TRAINING;
        request.header.timestamp_us = nimcp_time_now_us();
        request.batch_id = i + 1;
        request.batch_size = 32;
        request.learning_rate = 0.001f;
        request.optimizer_type = 1;

        promises[i] = bio_router_send_async(
            sender_ctx,
            &request,
            sizeof(request),
            BIO_CHANNEL_DOPAMINE
        );

        ASSERT_NE(promises[i], nullptr) << "Failed to send message " << i;
    }

    /* Wait for all responses */
    int successful = 0;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promises[i]);
        if (!future) continue;

        uint8_t response_buf[256] = {};
        nimcp_error_t result = nimcp_bio_future_wait(
            future,
            response_buf,
            2000 // 2 second timeout for all
        );

        if (result == NIMCP_SUCCESS) {
            successful++;
        }
        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promises[i]);
    }

    EXPECT_EQ(successful, NUM_MESSAGES) << "Not all messages were processed successfully";
    LOG_INFO("Successfully processed %d concurrent messages", successful);
}

/**
 * @test Test convergence detection via bio-async
 */
TEST_F(BrainTrainingBioAsyncTest, ConvergenceDetection) {
    /* Send series of loss messages with decreasing values */
    for (int i = 0; i < 15; i++) {
        bio_msg_loss_computed_t loss_msg = {};
        loss_msg.header.type = BIO_MSG_LOSS_COMPUTED;
        loss_msg.header.source_module = BIO_MODULE_BRAIN;
        loss_msg.header.target_module = BIO_MODULE_TRAINING;
        loss_msg.header.timestamp_us = nimcp_time_now_us();
        loss_msg.batch_id = i + 1;
        loss_msg.loss_value = 0.001f; // Stable low loss
        loss_msg.loss_gradient = -0.0001f;
        loss_msg.loss_type = 1;

        nimcp_bio_promise_t promise = bio_router_send_async(
            sender_ctx,
            &loss_msg,
            sizeof(loss_msg),
            BIO_CHANNEL_DOPAMINE
        );

        ASSERT_NE(promise, nullptr);

        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        if (future) {
            uint8_t response_buf[256] = {};
            nimcp_bio_future_wait(future, response_buf, 1000);
            nimcp_bio_future_destroy(future);
        }
        nimcp_bio_promise_destroy(promise);
    }

    /* Check convergence status */
    bool converged = nimcp_brain_training_is_converged(training_ctx);
    EXPECT_TRUE(converged) << "Training should have converged after stable loss";

    LOG_INFO("Convergence detection test passed");
}

/**
 * @test Test module cleanup
 */
TEST_F(BrainTrainingBioAsyncTest, ModuleCleanup) {
    /* Verify router is initialized */
    EXPECT_TRUE(bio_router_is_initialized());

    /* Destroy context (will unregister) */
    nimcp_brain_training_destroy(training_ctx);
    training_ctx = nullptr;

    /* Router should still be initialized (other modules may exist) */
    EXPECT_TRUE(bio_router_is_initialized());

    LOG_INFO("Module cleanup test passed");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
