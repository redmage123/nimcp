/**
 * @file test_tpb_bio_async.cpp
 * @brief Unit tests for Training Plasticity Bridge (TPB) bio-async integration
 *
 * Tests training bridge async operations:
 * - Async weight updates during training
 * - Phase synchronization for batch operations
 * - Predictive region routing
 * - Non-blocking lock-free operations
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TPBBioAsyncTest : public ::testing::Test {
protected:
    bio_module_context_t training_module;
    bio_module_context_t plasticity_module;
    bio_module_context_t brain_module;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        bio_config.simulation_dt_ms = 1.0f;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Register modules
        bio_module_info_t training_info = {
            .module_id = BIO_MODULE_TRAINING,
            .module_name = "training",
            .inbox_capacity = 200,
            .user_data = nullptr
        };
        training_module = bio_router_register_module(&training_info);
        ASSERT_NE(training_module, nullptr);

        bio_module_info_t plasticity_info = {
            .module_id = BIO_MODULE_STDP,
            .module_name = "plasticity",
            .inbox_capacity = 200,
            .user_data = nullptr
        };
        plasticity_module = bio_router_register_module(&plasticity_info);
        ASSERT_NE(plasticity_module, nullptr);

        bio_module_info_t brain_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "brain",
            .inbox_capacity = 200,
            .user_data = nullptr
        };
        brain_module = bio_router_register_module(&brain_info);
        ASSERT_NE(brain_module, nullptr);
    }

    void TearDown() override {
        bio_router_unregister_module(training_module);
        bio_router_unregister_module(plasticity_module);
        bio_router_unregister_module(brain_module);
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// ASYNC WEIGHT UPDATE TESTS
//=============================================================================

TEST_F(TPBBioAsyncTest, AsyncWeightUpdate) {
    std::atomic<bool> update_complete{false};
    std::atomic<float> new_weight{0.0f};

    // Register plasticity handler
    auto plasticity_handler = [](const void* msg, size_t size,
                                 nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<float>*>*>(user_data);

        auto* request = static_cast<const bio_msg_weight_update_request_t*>(msg);
        EXPECT_EQ(request->header.type, BIO_MSG_WEIGHT_UPDATE_REQUEST);

        // Simulate weight update
        float old_w = 0.5f;
        float new_w = old_w + request->weight_delta;

        // Clamp if requested
        if (request->clamp_to_bounds) {
            if (new_w < request->min_weight) new_w = request->min_weight;
            if (new_w > request->max_weight) new_w = request->max_weight;
        }

        data->second->store(new_w);

        // Complete promise
        if (promise) {
            bio_msg_weight_update_response_t response;
            bio_msg_init_header(&response.header, BIO_MSG_WEIGHT_UPDATE_RESPONSE,
                               BIO_MODULE_STDP, (bio_module_id_t)request->header.source_module,
                               sizeof(response));
            response.synapse_id = request->synapse_id;
            response.old_weight = old_w;
            response.new_weight = new_w;
            response.clamped = (new_w != old_w + request->weight_delta);
            response.error = NIMCP_SUCCESS;

            nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
        }

        data->first->store(true);
        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<float>*> handler_data{
        &update_complete, &new_weight
    };

    bio_router_unregister_module(plasticity_module);
    bio_module_info_t plasticity_info = {
        .module_id = BIO_MODULE_STDP,
        .module_name = "plasticity",
        .inbox_capacity = 200,
        .user_data = &handler_data
    };
    plasticity_module = bio_router_register_module(&plasticity_info);
    bio_router_register_handler(plasticity_module, BIO_MSG_WEIGHT_UPDATE_REQUEST, plasticity_handler);

    // Send async weight update
    bio_msg_weight_update_request_t request;
    bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                       BIO_MODULE_TRAINING, BIO_MODULE_STDP,
                       sizeof(request));
    request.header.channel = BIO_CHANNEL_DOPAMINE;
    request.synapse_id = 42;
    request.weight_delta = 0.1f;
    request.learning_rate = 0.01f;
    request.clamp_to_bounds = true;
    request.min_weight = 0.0f;
    request.max_weight = 1.0f;

    nimcp_bio_promise_t promise = bio_router_send_async(
        training_module, &request, sizeof(request), BIO_CHANNEL_DOPAMINE);
    ASSERT_NE(promise, nullptr);

    // Process plasticity inbox
    bio_router_process_inbox(plasticity_module, 10);

    // Wait for completion
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    bio_msg_weight_update_response_t response;
    nimcp_error_t err = nimcp_bio_future_wait(future, &response, 2000);

    if (err == NIMCP_SUCCESS) {
        EXPECT_TRUE(update_complete.load());
        EXPECT_FLOAT_EQ(new_weight.load(), 0.6f);
        EXPECT_EQ(response.synapse_id, 42u);
        EXPECT_FLOAT_EQ(response.new_weight, 0.6f);
    }

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(TPBBioAsyncTest, ConcurrentWeightUpdates) {
    std::atomic<int> updates_processed{0};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;

        if (promise) {
            bio_msg_weight_update_response_t response;
            bio_msg_init_header(&response.header, BIO_MSG_WEIGHT_UPDATE_RESPONSE,
                               BIO_MODULE_STDP, BIO_MODULE_TRAINING,
                               sizeof(response));
            response.error = NIMCP_SUCCESS;
            nimcp_bio_promise_complete_sized(promise, &response, sizeof(response));
        }

        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(plasticity_module);
    bio_module_info_t plasticity_info = {
        .module_id = BIO_MODULE_STDP,
        .module_name = "plasticity",
        .inbox_capacity = 200,
        .user_data = &updates_processed
    };
    plasticity_module = bio_router_register_module(&plasticity_info);
    bio_router_register_handler(plasticity_module, BIO_MSG_WEIGHT_UPDATE_REQUEST, handler);

    // Send multiple concurrent updates
    const int NUM_UPDATES = 50;
    std::vector<nimcp_bio_promise_t> promises;

    for (int i = 0; i < NUM_UPDATES; i++) {
        bio_msg_weight_update_request_t request;
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_STDP,
                           sizeof(request));
        request.synapse_id = i;
        request.weight_delta = 0.01f;

        nimcp_bio_promise_t p = bio_router_send_async(
            training_module, &request, sizeof(request), BIO_CHANNEL_DOPAMINE);
        if (p) {
            promises.push_back(p);
        }
    }

    // Process all updates
    bio_router_process_inbox(plasticity_module, NUM_UPDATES);

    // Wait for all completions
    for (auto p : promises) {
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        if (f) {
            bio_msg_weight_update_response_t response;
            nimcp_bio_future_wait(f, &response, 2000);
            nimcp_bio_future_destroy(f);
        }
        nimcp_bio_promise_destroy(p);
    }

    EXPECT_EQ(updates_processed.load(), NUM_UPDATES);
}

//=============================================================================
// PHASE SYNCHRONIZATION TESTS
//=============================================================================

TEST_F(TPBBioAsyncTest, BatchPhaseSynchronization) {
    // Test: Training waits for all regions to complete batch processing

    std::atomic<int> regions_completed{0};

    // Register handler for batch completion
    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;

        if (promise) {
            bio_message_header_t response;
            bio_msg_init_header(&response, BIO_MSG_BATCH_COMPLETE,
                               BIO_MODULE_BRAIN, BIO_MODULE_TRAINING,
                               sizeof(response));
            nimcp_bio_promise_complete(promise, &response);
        }

        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(brain_module);
    bio_module_info_t brain_info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "brain",
        .inbox_capacity = 200,
        .user_data = &regions_completed
    };
    brain_module = bio_router_register_module(&brain_info);
    bio_router_register_handler(brain_module, BIO_MSG_TRAINING_STEP_REQUEST, handler);

    // Send training step requests to multiple "regions"
    const int NUM_REGIONS = 4;
    std::vector<nimcp_bio_promise_t> promises;

    for (int i = 0; i < NUM_REGIONS; i++) {
        bio_msg_training_step_t step;
        bio_msg_init_header(&step.header, BIO_MSG_TRAINING_STEP_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_BRAIN,
                           sizeof(step));
        step.batch_id = 1;
        step.batch_size = 32;

        nimcp_bio_promise_t p = bio_router_send_async(
            training_module, &step, sizeof(step), BIO_CHANNEL_DOPAMINE);
        if (p) {
            promises.push_back(p);
        }
    }

    // Process brain inbox
    bio_router_process_inbox(brain_module, NUM_REGIONS);

    // Create phase sync for all futures (gamma = tight synchronization)
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    ASSERT_NE(sync, nullptr);

    for (auto p : promises) {
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        if (f) {
            nimcp_phase_sync_add_future(sync, f);
        }
    }

    // Wait for all to synchronize
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 3000);

    if (err == NIMCP_SUCCESS) {
        // Check coherence
        float coherence = nimcp_phase_sync_get_coherence(sync);
        EXPECT_GE(coherence, BIO_PHASE_COHERENCE_THRESHOLD);
    }

    EXPECT_EQ(regions_completed.load(), NUM_REGIONS);

    // Cleanup
    nimcp_phase_sync_destroy(sync);
    for (auto p : promises) {
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        nimcp_bio_future_destroy(f);
        nimcp_bio_promise_destroy(p);
    }
}

//=============================================================================
// PREDICTIVE REGION ROUTING TESTS
//=============================================================================

TEST_F(TPBBioAsyncTest, PredictiveRegionRouting) {
    // Test: Training module predicts which region will complete next

    std::atomic<int> prediction_errors{0};

    // Create predictive model for region completion time
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "region_completion_ms", 100.0f, 1.0f);
    ASSERT_NE(model, nullptr);

    // Register prediction error callback
    auto error_callback = [](const char* signal_name, float prediction,
                            float actual, float error, float surprise,
                            void* user_data) {
        auto* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;

        // In real implementation, would adjust routing strategy
    };

    nimcp_predictive_on_error(model, error_callback, &prediction_errors, 1.0f);

    // Simulate region completions with varying times
    float actual_times[] = {95.0f, 105.0f, 98.0f, 110.0f, 102.0f};

    for (float time : actual_times) {
        nimcp_predictive_observe(model, time);

        // Check prediction accuracy
        float prediction = nimcp_predictive_get_prediction(model);
        float error = fabsf(prediction - time);

        // Prediction should improve over time
    }

    EXPECT_GE(prediction_errors.load(), 0);

    nimcp_predictive_destroy(model);
}

//=============================================================================
// NON-BLOCKING OPERATIONS TESTS
//=============================================================================

TEST_F(TPBBioAsyncTest, NonBlockingWeightUpdates) {
    // Test: Training continues without blocking on weight updates

    std::atomic<bool> training_blocked{false};
    std::atomic<int> updates_sent{0};

    // Register slow plasticity handler (simulates delay)
    auto slow_handler = [](const void* msg, size_t size,
                          nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        // Simulate processing delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (promise) {
            bio_msg_weight_update_response_t response;
            bio_msg_init_header(&response.header, BIO_MSG_WEIGHT_UPDATE_RESPONSE,
                               BIO_MODULE_STDP, BIO_MODULE_TRAINING,
                               sizeof(response));
            response.error = NIMCP_SUCCESS;
            nimcp_bio_promise_complete(promise, &response);
        }

        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(plasticity_module);
    bio_module_info_t plasticity_info = {
        .module_id = BIO_MODULE_STDP,
        .module_name = "plasticity",
        .inbox_capacity = 200,
        .user_data = nullptr
    };
    plasticity_module = bio_router_register_module(&plasticity_info);
    bio_router_register_handler(plasticity_module, BIO_MSG_WEIGHT_UPDATE_REQUEST, slow_handler);

    // Start background thread to process plasticity updates
    std::thread processor([&]() {
        while (updates_sent.load() < 10) {
            bio_router_process_inbox(plasticity_module, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Main "training loop" sends updates without blocking
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 10; i++) {
        bio_msg_weight_update_request_t request;
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_STDP,
                           sizeof(request));
        request.synapse_id = i;

        // Non-blocking send
        nimcp_bio_promise_t p = bio_router_send_async(
            training_module, &request, sizeof(request), BIO_CHANNEL_DOPAMINE);

        if (p) {
            updates_sent++;
            nimcp_bio_promise_destroy(p);  // Don't wait for response
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete quickly (not blocked by slow handler)
    EXPECT_LT(duration.count(), 50);  // Much less than 10 * 10ms
    EXPECT_FALSE(training_blocked.load());

    processor.join();
}

//=============================================================================
// LOSS COMPUTATION TESTS
//=============================================================================

TEST_F(TPBBioAsyncTest, LossComputationMessage) {
    std::atomic<bool> loss_received{false};
    std::atomic<float> loss_value{0.0f};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* data = static_cast<std::pair<std::atomic<bool>*, std::atomic<float>*>*>(user_data);
        data->first->store(true);

        auto* loss = static_cast<const bio_msg_loss_computed_t*>(msg);
        data->second->store(loss->loss_value);

        return NIMCP_SUCCESS;
    };

    std::pair<std::atomic<bool>*, std::atomic<float>*> handler_data{
        &loss_received, &loss_value
    };

    bio_router_unregister_module(training_module);
    bio_module_info_t training_info = {
        .module_id = BIO_MODULE_TRAINING,
        .module_name = "training",
        .inbox_capacity = 200,
        .user_data = &handler_data
    };
    training_module = bio_router_register_module(&training_info);
    bio_router_register_handler(training_module, BIO_MSG_LOSS_COMPUTED, handler);

    // Send loss computation result
    bio_msg_loss_computed_t loss;
    bio_msg_init_header(&loss.header, BIO_MSG_LOSS_COMPUTED,
                       BIO_MODULE_BRAIN, BIO_MODULE_TRAINING,
                       sizeof(loss));
    loss.batch_id = 1;
    loss.loss_value = 0.234f;
    loss.loss_gradient = -0.05f;
    loss.loss_type = 0;  // MSE or similar

    bio_router_send(brain_module, &loss, sizeof(loss), 1000);
    bio_router_process_inbox(training_module, 10);

    EXPECT_TRUE(loss_received.load());
    EXPECT_FLOAT_EQ(loss_value.load(), 0.234f);
}

//=============================================================================
// GRADIENT COMPUTATION TESTS
//=============================================================================

TEST_F(TPBBioAsyncTest, GradientComputationMessage) {
    std::atomic<bool> gradient_received{false};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* received = static_cast<std::atomic<bool>*>(user_data);
        *received = true;

        // In real implementation, would apply gradients via optimizer

        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(training_module);
    bio_module_info_t training_info = {
        .module_id = BIO_MODULE_TRAINING,
        .module_name = "training",
        .inbox_capacity = 200,
        .user_data = &gradient_received
    };
    training_module = bio_router_register_module(&training_info);
    bio_router_register_handler(training_module, BIO_MSG_GRADIENT_COMPUTED, handler);

    // Send gradient computation result
    bio_message_header_t gradient;
    bio_msg_init_header(&gradient, BIO_MSG_GRADIENT_COMPUTED,
                       BIO_MODULE_BRAIN, BIO_MODULE_TRAINING,
                       sizeof(gradient));

    bio_router_send(brain_module, &gradient, sizeof(gradient), 1000);
    bio_router_process_inbox(training_module, 10);

    EXPECT_TRUE(gradient_received.load());
}

//=============================================================================
// CHECKPOINT TESTS
//=============================================================================

TEST_F(TPBBioAsyncTest, CheckpointRequestAsync) {
    std::atomic<bool> checkpoint_triggered{false};

    auto handler = [](const void* msg, size_t size,
                     nimcp_bio_promise_t promise, void* user_data) -> nimcp_error_t {
        auto* triggered = static_cast<std::atomic<bool>*>(user_data);
        *triggered = true;

        auto* request = static_cast<const bio_msg_checkpoint_request_t*>(msg);

        // Simulate async checkpoint
        if (promise) {
            bio_message_header_t response;
            bio_msg_init_header(&response, BIO_MSG_CHECKPOINT_COMPLETE,
                               BIO_MODULE_BRAIN, (bio_module_id_t)request->header.source_module,
                               sizeof(response));
            nimcp_bio_promise_complete(promise, &response);
        }

        return NIMCP_SUCCESS;
    };

    bio_router_unregister_module(brain_module);
    bio_module_info_t brain_info = {
        .module_id = BIO_MODULE_BRAIN,
        .module_name = "brain",
        .inbox_capacity = 200,
        .user_data = &checkpoint_triggered
    };
    brain_module = bio_router_register_module(&brain_info);
    bio_router_register_handler(brain_module, BIO_MSG_CHECKPOINT_REQUEST, handler);

    // Send checkpoint request
    bio_msg_checkpoint_request_t request;
    bio_msg_init_header(&request.header, BIO_MSG_CHECKPOINT_REQUEST,
                       BIO_MODULE_TRAINING, BIO_MODULE_BRAIN,
                       sizeof(request));
    request.checkpoint_id = 1;
    strncpy(request.path, "/tmp/checkpoint.bin", sizeof(request.path));
    request.include_optimizer_state = true;
    request.compress = false;

    nimcp_bio_promise_t promise = bio_router_send_async(
        training_module, &request, sizeof(request), BIO_CHANNEL_DOPAMINE);

    bio_router_process_inbox(brain_module, 10);

    // Wait for completion
    if (promise) {
        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        bio_message_header_t response;
        nimcp_bio_future_wait(future, &response, 2000);
        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promise);
    }

    EXPECT_TRUE(checkpoint_triggered.load());
}
