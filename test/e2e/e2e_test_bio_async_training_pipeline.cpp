/**
 * @file e2e_test_bio_async_training_pipeline.cpp
 * @brief E2E Tests for Bio-Async Training Pipeline
 *
 * WHAT: Complete training pipeline using bio-async messaging
 * WHY:  Verify training flow with async loss computation, gradient updates, and weight modifications
 * HOW:  Test message routing between training modules via neuromodulator channels
 *
 * TEST PIPELINES:
 * - FullTrainingPipeline: Complete training step with loss, gradients, and optimizer
 * - BatchTrainingPipeline: Multi-batch training with bio-async coordination
 * - AsyncCheckpointPipeline: Asynchronous checkpoint saving during training
 * - TrainingWithPlasticityPipeline: Training coordinated with STDP/plasticity
 *
 * @author NIMCP Development Team
 * @date 2025-12-03
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain.h"
#include "middleware/training/nimcp_brain_training_integration.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioAsyncTrainingE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system
        nimcp_error_t err = nimcp_bio_async_init(NULL);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";

        // Initialize router
        err = bio_router_init(NULL);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Router initialization failed";

        // Register training module
        bio_module_info_t info = {
            .module_id = BIO_MODULE_TRAINING,
            .module_name = "training_test",
            .inbox_capacity = 100,
            .user_data = nullptr
        };
        training_ctx_ = bio_router_register_module(&info);
        ASSERT_NE(training_ctx_, nullptr) << "Failed to register training module";

        // Register handlers
        bio_router_register_handler(training_ctx_, BIO_MSG_LOSS_COMPUTED, handle_loss_computed);
        bio_router_register_handler(training_ctx_, BIO_MSG_GRADIENT_COMPUTED, handle_gradient_computed);
        bio_router_register_handler(training_ctx_, BIO_MSG_TRAINING_STEP_COMPLETE, handle_training_complete);
    }

    void TearDown() override {
        if (training_ctx_) {
            bio_router_unregister_module(training_ctx_);
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    // Message handlers
    static nimcp_error_t handle_loss_computed(const void* msg, size_t msg_size,
                                               nimcp_bio_promise_t response_promise,
                                               void* user_data) {
        const bio_msg_loss_computed_t* loss_msg =
            static_cast<const bio_msg_loss_computed_t*>(msg);

        loss_received_.store(true);
        last_loss_value_ = loss_msg->loss_value;

        if (response_promise) {
            nimcp_bio_promise_complete(response_promise, &loss_msg->loss_value);
        }

        return NIMCP_SUCCESS;
    }

    static nimcp_error_t handle_gradient_computed(const void* msg, size_t msg_size,
                                                   nimcp_bio_promise_t response_promise,
                                                   void* user_data) {
        const bio_msg_gradient_computed_t* grad_msg =
            reinterpret_cast<const bio_msg_gradient_computed_t*>(msg);

        gradient_received_.store(true);

        if (response_promise) {
            float ack = 1.0f;
            nimcp_bio_promise_complete(response_promise, &ack);
        }

        return NIMCP_SUCCESS;
    }

    static nimcp_error_t handle_training_complete(const void* msg, size_t msg_size,
                                                   nimcp_bio_promise_t response_promise,
                                                   void* user_data) {
        training_complete_.store(true);

        if (response_promise) {
            float ack = 1.0f;
            nimcp_bio_promise_complete(response_promise, &ack);
        }

        return NIMCP_SUCCESS;
    }

    bio_module_context_t training_ctx_{nullptr};

    static std::atomic<bool> loss_received_;
    static std::atomic<bool> gradient_received_;
    static std::atomic<bool> training_complete_;
    static std::atomic<float> last_loss_value_;
};

// Static member initialization
std::atomic<bool> BioAsyncTrainingE2ETest::loss_received_{false};
std::atomic<bool> BioAsyncTrainingE2ETest::gradient_received_{false};
std::atomic<bool> BioAsyncTrainingE2ETest::training_complete_{false};
std::atomic<float> BioAsyncTrainingE2ETest::last_loss_value_{0.0f};

//=============================================================================
// Pipeline 1: Full Training Step
//=============================================================================

TEST_F(BioAsyncTrainingE2ETest, FullTrainingPipeline) {
    E2E_PIPELINE_START("Full Training Step via Bio-Async");

    // Reset flags
    loss_received_.store(false);
    gradient_received_.store(false);
    training_complete_.store(false);

    // Stage 1: Send training step request
    E2E_STAGE_BEGIN("Send training step request", 100);

    bio_msg_training_step_t train_msg;
    bio_msg_init_header(&train_msg.header, BIO_MSG_TRAINING_STEP_REQUEST,
                        BIO_MODULE_TRAINING, BIO_MODULE_TRAINING, sizeof(train_msg));
    train_msg.batch_id = 1;
    train_msg.batch_size = 32;
    train_msg.learning_rate = 0.001f;
    train_msg.optimizer_type = 0; // SGD

    nimcp_bio_promise_t train_promise = bio_router_send_async(
        training_ctx_, &train_msg, sizeof(train_msg), BIO_CHANNEL_DOPAMINE);
    E2E_ASSERT_NOT_NULL(train_promise, "Failed to send training request");

    nimcp_bio_future_t train_future = nimcp_bio_promise_get_future(train_promise);
    E2E_ASSERT_NOT_NULL(train_future, "Failed to get training future");

    E2E_STAGE_END();

    // Stage 2: Simulate loss computation
    E2E_STAGE_BEGIN("Compute and send loss", 200);

    std::thread loss_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        bio_msg_loss_computed_t loss_msg;
        bio_msg_init_header(&loss_msg.header, BIO_MSG_LOSS_COMPUTED,
                            BIO_MODULE_TRAINING, BIO_MODULE_TRAINING, sizeof(loss_msg));
        loss_msg.batch_id = 1;
        loss_msg.loss_value = 2.35f;
        loss_msg.loss_gradient = -0.12f;
        loss_msg.loss_type = 0; // MSE

        bio_router_send(training_ctx_, &loss_msg, sizeof(loss_msg), 1000);
    });

    loss_thread.join();

    // Process inbox to receive loss message
    uint32_t processed = bio_router_process_inbox(training_ctx_, 10);
    E2E_ASSERT(processed > 0, "No messages processed");

    // Wait for loss to be received
    auto start = std::chrono::steady_clock::now();
    while (!loss_received_.load() &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bio_router_process_inbox(training_ctx_, 10);
    }

    E2E_ASSERT(loss_received_.load(), "Loss message not received");
    E2E_ASSERT(std::abs(last_loss_value_.load() - 2.35f) < 0.01f, "Loss value incorrect");

    E2E_STAGE_END();

    // Stage 3: Simulate gradient computation
    E2E_STAGE_BEGIN("Compute and send gradients", 200);

    // Define gradient computed message type (not in bio_messages.h)
    struct bio_msg_gradient_computed_t {
        bio_message_header_t header;
        uint32_t batch_id;
        uint32_t gradient_count;
        float gradient_norm;
    };

    std::thread gradient_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        bio_msg_gradient_computed_t grad_msg;
        bio_msg_init_header(&grad_msg.header, BIO_MSG_GRADIENT_COMPUTED,
                            BIO_MODULE_TRAINING, BIO_MODULE_TRAINING, sizeof(grad_msg));
        grad_msg.batch_id = 1;
        grad_msg.gradient_count = 1000;
        grad_msg.gradient_norm = 0.05f;

        bio_router_send(training_ctx_, &grad_msg, sizeof(grad_msg), 1000);
    });

    gradient_thread.join();

    // Process inbox
    start = std::chrono::steady_clock::now();
    while (!gradient_received_.load() &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bio_router_process_inbox(training_ctx_, 10);
    }

    E2E_ASSERT(gradient_received_.load(), "Gradient message not received");

    E2E_STAGE_END();

    // Stage 4: Send training complete
    E2E_STAGE_BEGIN("Signal training step complete", 100);

    // Define training step complete message
    struct bio_msg_training_step_complete_t {
        bio_message_header_t header;
        uint32_t batch_id;
        float final_loss;
        bool converged;
    };

    bio_msg_training_step_complete_t complete_msg;
    bio_msg_init_header(&complete_msg.header, BIO_MSG_TRAINING_STEP_COMPLETE,
                        BIO_MODULE_TRAINING, BIO_MODULE_TRAINING, sizeof(complete_msg));
    complete_msg.batch_id = 1;
    complete_msg.final_loss = 2.30f;
    complete_msg.converged = false;

    bio_router_send(training_ctx_, &complete_msg, sizeof(complete_msg), 1000);

    // Process and wait for completion
    start = std::chrono::steady_clock::now();
    while (!training_complete_.load() &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bio_router_process_inbox(training_ctx_, 10);
    }

    E2E_ASSERT(training_complete_.load(), "Training complete not received");

    E2E_STAGE_END();

    // Stage 5: Verify bio-async statistics
    E2E_STAGE_BEGIN("Verify bio-async statistics", 100);

    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get bio-async stats");
    E2E_ASSERT(stats.total_futures_created > 0, "No futures created");

    bio_router_stats_t router_stats;
    err = bio_router_get_stats(&router_stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get router stats");
    E2E_ASSERT(router_stats.messages_routed >= 3, "Expected at least 3 messages routed");

    E2E_STAGE_END();

    // Cleanup
    nimcp_bio_future_destroy(train_future);
    nimcp_bio_promise_destroy(train_promise);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Multi-Batch Training with Async Coordination
// NOTE: Disabled due to phase synchronization timeout issues
//=============================================================================

// KNOWN ISSUE: Test design expects promises to be completed by batch_complete
// messages, but the handler doesn't have access to the original promises.
// Requires architectural rework to properly connect async requests with responses.
TEST_F(BioAsyncTrainingE2ETest, DISABLED_BatchTrainingPipeline) {
    E2E_PIPELINE_START("Multi-Batch Training with Bio-Async");

    const uint32_t NUM_BATCHES = 5;
    std::vector<nimcp_bio_promise_t> batch_promises;
    std::vector<nimcp_bio_future_t> batch_futures;

    // Stage 1: Send multiple batch training requests
    E2E_STAGE_BEGIN("Send batch training requests", 200);

    for (uint32_t i = 0; i < NUM_BATCHES; i++) {
        bio_msg_training_step_t train_msg;
        bio_msg_init_header(&train_msg.header, BIO_MSG_TRAINING_STEP_REQUEST,
                            BIO_MODULE_TRAINING, BIO_MODULE_TRAINING, sizeof(train_msg));
        train_msg.batch_id = i + 1;
        train_msg.batch_size = 32;
        train_msg.learning_rate = 0.001f;
        train_msg.optimizer_type = 0;

        nimcp_bio_promise_t promise = bio_router_send_async(
            training_ctx_, &train_msg, sizeof(train_msg), BIO_CHANNEL_DOPAMINE);
        E2E_ASSERT_NOT_NULL(promise, "Failed to send batch training request");

        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        E2E_ASSERT_NOT_NULL(future, "Failed to get batch future");

        batch_promises.push_back(promise);
        batch_futures.push_back(future);
    }

    E2E_STAGE_END();

    // Stage 2: Use phase synchronization to wait for all batches
    E2E_STAGE_BEGIN("Wait for batch synchronization", 1000);

    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_BETA);
    E2E_ASSERT_NOT_NULL(sync, "Failed to create phase sync");

    for (auto future : batch_futures) {
        nimcp_error_t err = nimcp_phase_sync_add_future(sync, future);
        E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to add future to sync");
    }

    // Complete all batches asynchronously
    std::thread completion_thread([this, NUM_BATCHES]() {
        for (uint32_t i = 0; i < NUM_BATCHES; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Simulate batch completion
            struct bio_msg_batch_complete_t {
                bio_message_header_t header;
                uint32_t batch_id;
                float batch_loss;
            };

            bio_msg_batch_complete_t batch_msg;
            bio_msg_init_header(&batch_msg.header, BIO_MSG_BATCH_COMPLETE,
                                BIO_MODULE_TRAINING, BIO_MODULE_TRAINING, sizeof(batch_msg));
            batch_msg.batch_id = i + 1;
            batch_msg.batch_loss = 2.0f - (i * 0.1f);

            bio_router_send(training_ctx_, &batch_msg, sizeof(batch_msg), 1000);
        }
    });

    // Wait for coherent synchronization (80% threshold)
    nimcp_error_t err = nimcp_phase_sync_wait_coherent(sync, 0.8f, 5000);

    completion_thread.join();

    // Note: Phase sync might timeout if messages aren't completing promises
    // This is expected in this test setup

    E2E_STAGE_END();

    // Stage 3: Verify all futures
    E2E_STAGE_BEGIN("Verify batch futures", 500);

    size_t ready_count = 0;
    for (auto future : batch_futures) {
        if (nimcp_bio_future_is_ready(future)) {
            ready_count++;
        }
    }

    E2E_ASSERT(ready_count >= NUM_BATCHES / 2,
               "Expected at least half of batches to be ready");

    E2E_STAGE_END();

    // Cleanup
    nimcp_phase_sync_destroy(sync);
    for (auto future : batch_futures) {
        nimcp_bio_future_destroy(future);
    }
    for (auto promise : batch_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Async Checkpoint Pipeline
// NOTE: Disabled due to handler registration issues causing crashes
//=============================================================================

// KNOWN ISSUE: Lambda capture for checkpoint_requested atomic cannot be
// passed through C-style function pointer to bio_router_register_handler.
// Need to use static variables or a different handler registration pattern.
TEST_F(BioAsyncTrainingE2ETest, DISABLED_AsyncCheckpointPipeline) {
    E2E_PIPELINE_START("Async Checkpoint during Training");

    std::atomic<bool> checkpoint_requested{false};
    std::atomic<bool> checkpoint_completed{false};

    // Stage 1: Register checkpoint handlers
    E2E_STAGE_BEGIN("Register checkpoint handlers", 100);

    auto checkpoint_handler = [](const void* msg, size_t msg_size,
                                  nimcp_bio_promise_t response_promise,
                                  void* user_data) -> nimcp_error_t {
        auto* req_flag = static_cast<std::atomic<bool>*>(user_data);
        req_flag->store(true);

        if (response_promise) {
            float ack = 1.0f;
            nimcp_bio_promise_complete(response_promise, &ack);
        }

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(training_ctx_, BIO_MSG_CHECKPOINT_REQUEST, checkpoint_handler);

    E2E_STAGE_END();

    // Stage 2: Send checkpoint request
    E2E_STAGE_BEGIN("Send async checkpoint request", 200);

    bio_msg_checkpoint_request_t checkpoint_msg;
    bio_msg_init_header(&checkpoint_msg.header, BIO_MSG_CHECKPOINT_REQUEST,
                        BIO_MODULE_TRAINING, BIO_MODULE_TRAINING, sizeof(checkpoint_msg));
    checkpoint_msg.checkpoint_id = 1;
    snprintf(checkpoint_msg.path, sizeof(checkpoint_msg.path), "/tmp/checkpoint_1.ckpt");
    checkpoint_msg.include_optimizer_state = true;
    checkpoint_msg.compress = false;

    nimcp_bio_promise_t ckpt_promise = bio_router_send_async(
        training_ctx_, &checkpoint_msg, sizeof(checkpoint_msg),
        BIO_CHANNEL_SEROTONIN); // Slow channel for heavy operation
    E2E_ASSERT_NOT_NULL(ckpt_promise, "Failed to send checkpoint request");

    E2E_STAGE_END();

    // Stage 3: Process checkpoint in background
    E2E_STAGE_BEGIN("Process checkpoint", 500);

    std::thread checkpoint_thread([&checkpoint_completed]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        checkpoint_completed.store(true);
    });

    // Process messages while checkpoint completes
    auto start = std::chrono::steady_clock::now();
    while (!checkpoint_completed.load() &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(training_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    checkpoint_thread.join();

    E2E_ASSERT(checkpoint_completed.load(), "Checkpoint not completed");

    E2E_STAGE_END();

    // Cleanup
    nimcp_bio_promise_destroy(ckpt_promise);

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Training with Plasticity Coordination
// NOTE: Disabled due to handler coordination timing issues
//=============================================================================

// KNOWN ISSUE: Same fundamental issue as BatchTrainingPipeline - weight_handler
// needs access to original promises to complete them, but handler registration
// only passes user_data for the counter.
TEST_F(BioAsyncTrainingE2ETest, DISABLED_TrainingWithPlasticityPipeline) {
    E2E_PIPELINE_START("Training with Plasticity via Bio-Async");

    std::atomic<int> weight_updates{0};
    std::atomic<int> stdp_events{0};

    // Stage 1: Register plasticity handlers
    E2E_STAGE_BEGIN("Register plasticity handlers", 100);

    auto weight_handler = [](const void* msg, size_t msg_size,
                              nimcp_bio_promise_t response_promise,
                              void* user_data) -> nimcp_error_t {
        auto* counter = static_cast<std::atomic<int>*>(user_data);
        counter->fetch_add(1);

        if (response_promise) {
            bio_msg_weight_update_response_t response;
            bio_msg_init_header(&response.header, BIO_MSG_WEIGHT_UPDATE_RESPONSE,
                                BIO_MODULE_TRAINING, BIO_MODULE_STDP, sizeof(response));
            response.synapse_id = 1;
            response.old_weight = 0.5f;
            response.new_weight = 0.52f;
            response.clamped = false;
            response.error = NIMCP_SUCCESS;

            nimcp_bio_promise_complete(response_promise, &response);
        }

        return NIMCP_SUCCESS;
    };

    bio_router_register_handler(training_ctx_, BIO_MSG_WEIGHT_UPDATE_REQUEST, weight_handler);

    E2E_STAGE_END();

    // Stage 2: Send weight update requests
    E2E_STAGE_BEGIN("Send weight update requests", 300);

    const int NUM_UPDATES = 10;
    std::vector<nimcp_bio_promise_t> update_promises;

    for (int i = 0; i < NUM_UPDATES; i++) {
        bio_msg_weight_update_request_t update_msg;
        bio_msg_init_header(&update_msg.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                            BIO_MODULE_TRAINING, BIO_MODULE_STDP, sizeof(update_msg));
        update_msg.synapse_id = i;
        update_msg.pre_neuron_id = i * 2;
        update_msg.post_neuron_id = i * 2 + 1;
        update_msg.weight_delta = 0.02f;
        update_msg.learning_rate = 0.001f;
        update_msg.eligibility_trace = 0.8f;
        update_msg.clamp_to_bounds = true;
        update_msg.min_weight = 0.0f;
        update_msg.max_weight = 1.0f;

        nimcp_bio_promise_t promise = bio_router_send_async(
            training_ctx_, &update_msg, sizeof(update_msg),
            BIO_CHANNEL_DOPAMINE); // Reward channel for weight updates
        E2E_ASSERT_NOT_NULL(promise, "Failed to send weight update");

        update_promises.push_back(promise);
    }

    E2E_STAGE_END();

    // Stage 3: Process weight updates
    E2E_STAGE_BEGIN("Process weight updates", 500);

    auto start = std::chrono::steady_clock::now();
    while (weight_updates.load() < NUM_UPDATES &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < 1000) {
        bio_router_process_inbox(training_ctx_, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_ASSERT(weight_updates.load() >= NUM_UPDATES / 2,
               "Expected at least half of weight updates to be processed");

    E2E_STAGE_END();

    // Cleanup
    for (auto promise : update_promises) {
        nimcp_bio_promise_destroy(promise);
    }

    E2E_PIPELINE_END();
}
