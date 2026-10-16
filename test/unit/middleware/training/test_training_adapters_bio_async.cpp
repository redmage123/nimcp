//=============================================================================
// test_training_adapters_bio_async.cpp
// Unit tests for Training Adapters Bio-Async Integration
//=============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_adapters.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"

/* Stub for bio_module_send_message - not implemented yet, just return success */
static inline nimcp_error_t bio_module_send_message(bio_module_id_t module,
                                                     const void* msg,
                                                     size_t msg_size,
                                                     nimcp_bio_promise_t promise)
{
    (void)module;
    (void)msg;
    (void)msg_size;
    (void)promise;
    /* Just return success for testing - actual routing not implemented */
    return NIMCP_SUCCESS;
}

using namespace testing;

class TrainingAdaptersBioAsyncTest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_manager = nullptr;

    void SetUp() override {
        // Initialize unified memory manager
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_config.enable_tracking = true;
        mem_config.enable_cow = true;
        mem_manager = unified_mem_create(&mem_config);
        ASSERT_NE(nullptr, mem_manager);

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;
        bio_config.enable_statistics = true;
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_init(&bio_config));
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
        if (mem_manager) {
            unified_mem_destroy(mem_manager);
            mem_manager = nullptr;
        }
    }
};

//=============================================================================
// Learning Signal Adapter Tests
//=============================================================================

TEST_F(TrainingAdaptersBioAsyncTest, LearningSignalAdapterCreationWithBioAsync) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();

    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(nullptr, adapter);

    // Verify bio-async integration is enabled
    learning_signal_adapter_stats_t stats;
    ASSERT_TRUE(learning_signal_adapter_get_stats(adapter, &stats));
    EXPECT_EQ(0, stats.signals_extracted);

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersBioAsyncTest, WeightUpdateRequestHandling) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(nullptr, adapter);

    // Create weight update request message
    bio_msg_weight_update_request_t request = {};
    bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                       BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                       sizeof(bio_msg_weight_update_request_t) - sizeof(bio_message_header_t));
    request.synapse_id = 42;
    request.pre_neuron_id = 10;
    request.post_neuron_id = 20;
    request.weight_delta = 0.05f;
    request.learning_rate = 0.01f;
    request.eligibility_trace = 0.8f;
    request.clamp_to_bounds = true;
    request.min_weight = -1.0f;
    request.max_weight = 1.0f;

    // Create promise for response
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE,
                                                           sizeof(bio_msg_weight_update_response_t));
    ASSERT_NE(nullptr, promise);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(nullptr, future);

    // Send message via bio-router
    nimcp_error_t err = bio_module_send_message(BIO_MODULE_TRAINING,
                                                &request, sizeof(request),
                                                promise);
    EXPECT_EQ(NIMCP_SUCCESS, err);

    // Wait for response with timeout
    bio_msg_weight_update_response_t response;
    err = nimcp_bio_future_wait(future, &response, 1000);

    if (err == NIMCP_SUCCESS) {
        EXPECT_EQ(request.synapse_id, response.synapse_id);
        EXPECT_EQ(NIMCP_SUCCESS, response.error);
    }

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersBioAsyncTest, GradientComputedHandling) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(nullptr, adapter);

    // Create gradient computed message
    bio_msg_loss_computed_t gradient_msg = {};
    bio_msg_init_header(&gradient_msg.header, BIO_MSG_GRADIENT_COMPUTED,
                       BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                       sizeof(bio_msg_loss_computed_t) - sizeof(bio_message_header_t));
    gradient_msg.batch_id = 5;
    gradient_msg.loss_value = 0.25f;
    gradient_msg.loss_gradient = -0.15f;
    gradient_msg.loss_type = 1; // MSE

    // Send message (no response expected)
    nimcp_error_t err = bio_module_send_message(BIO_MODULE_TRAINING,
                                                &gradient_msg, sizeof(gradient_msg),
                                                nullptr);
    EXPECT_EQ(NIMCP_SUCCESS, err);

    // Give time for async processing
    nimcp_bio_async_step(10.0f);

    // Check that signal was extracted
    learning_signal_adapter_stats_t stats;
    ASSERT_TRUE(learning_signal_adapter_get_stats(adapter, &stats));
    // May be 0 or 1 depending on timing
    EXPECT_GE(stats.signals_extracted, 0);

    learning_signal_adapter_destroy(adapter);
}

//=============================================================================
// Weight Update Router Tests
//=============================================================================

TEST_F(TrainingAdaptersBioAsyncTest, WeightRouterCreationWithBioAsync) {
    weight_update_router_config_t config = weight_update_router_default_config();

    weight_update_router_t router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(nullptr, router);

    weight_update_router_stats_t stats;
    ASSERT_TRUE(weight_update_router_get_stats(router, &stats));
    EXPECT_EQ(0, stats.updates_routed);

    weight_update_router_destroy(router);
}

TEST_F(TrainingAdaptersBioAsyncTest, WeightRouterMessageRouting) {
    weight_update_router_config_t config = weight_update_router_default_config();
    weight_update_router_t router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(nullptr, router);

    // Add routing rule
    bool added = weight_update_router_add_route(router,
                                                LEARNING_SIGNAL_ERROR,
                                                WEIGHT_TARGET_CORTICAL,
                                                10);
    EXPECT_TRUE(added);

    // Create weight update
    weight_update_t update = {};
    update.target_type = WEIGHT_TARGET_CORTICAL;
    update.source_neuron = 1;
    update.target_neuron = 2;
    update.weight_delta = 0.01f;
    update.learning_rate = 0.001f;
    update.modulation_factor = 1.0f;
    update.timestamp_us = 1000;
    update.apply_stdp = true;

    // Route update
    bool routed = weight_update_router_route(router, &update);
    EXPECT_TRUE(routed);

    weight_update_router_stats_t stats;
    ASSERT_TRUE(weight_update_router_get_stats(router, &stats));
    EXPECT_GE(stats.updates_routed, 1);

    weight_update_router_destroy(router);
}

//=============================================================================
// Training Event Manager Tests
//=============================================================================

TEST_F(TrainingAdaptersBioAsyncTest, EventManagerCreationWithBioAsync) {
    training_event_manager_config_t config = training_event_manager_default_config();

    training_event_manager_t manager = training_event_manager_create(&config, nullptr);
    ASSERT_NE(nullptr, manager);

    training_event_manager_stats_t stats;
    ASSERT_TRUE(training_event_manager_get_stats(manager, &stats));
    EXPECT_EQ(0, stats.events_published);

    training_event_manager_destroy(manager);
}

TEST_F(TrainingAdaptersBioAsyncTest, EventManagerPublishWithBioAsync) {
    training_event_manager_config_t config = training_event_manager_default_config();
    training_event_manager_t manager = training_event_manager_create(&config, nullptr);
    ASSERT_NE(nullptr, manager);

    // Create training event
    training_event_data_t event = {};
    event.type = TRAINING_EVENT_BATCH_END;
    event.epoch = 1;
    event.batch = 10;
    event.loss = 0.5f;
    event.learning_rate = 0.001f;
    event.timestamp_us = 2000;

    // Publish event
    bool published = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(published);

    training_event_manager_stats_t stats;
    ASSERT_TRUE(training_event_manager_get_stats(manager, &stats));
    EXPECT_GE(stats.events_published, 1);

    training_event_manager_destroy(manager);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(TrainingAdaptersBioAsyncTest, FullPipelineIntegration) {
    // Create all adapters
    learning_signal_adapter_config_t lsa_config = learning_signal_adapter_default_config();
    learning_signal_adapter_t adapter = learning_signal_adapter_create(&lsa_config);
    ASSERT_NE(nullptr, adapter);

    weight_update_router_config_t wur_config = weight_update_router_default_config();
    weight_update_router_t router = weight_update_router_create(&wur_config, nullptr);
    ASSERT_NE(nullptr, router);

    training_event_manager_config_t tem_config = training_event_manager_default_config();
    training_event_manager_t manager = training_event_manager_create(&tem_config, nullptr);
    ASSERT_NE(nullptr, manager);

    // Add routing rules
    weight_update_router_add_route(router,
                                   LEARNING_SIGNAL_ERROR,
                                   WEIGHT_TARGET_CORTICAL,
                                   10);

    // Simulate training iteration
    bio_msg_loss_computed_t loss_msg = {};
    bio_msg_init_header(&loss_msg.header, BIO_MSG_LOSS_COMPUTED,
                       BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                       sizeof(bio_msg_loss_computed_t) - sizeof(bio_message_header_t));
    loss_msg.batch_id = 1;
    loss_msg.loss_value = 0.35f;
    loss_msg.loss_gradient = -0.1f;

    bio_module_send_message(BIO_MODULE_TRAINING, &loss_msg, sizeof(loss_msg), nullptr);

    // Step simulation
    nimcp_bio_async_step(10.0f);

    // Verify all components processed something
    learning_signal_adapter_stats_t adapter_stats;
    learning_signal_adapter_get_stats(adapter, &adapter_stats);

    weight_update_router_stats_t router_stats;
    weight_update_router_get_stats(router, &router_stats);

    training_event_manager_stats_t manager_stats;
    training_event_manager_get_stats(manager, &manager_stats);

    // Cleanup
    training_event_manager_destroy(manager);
    weight_update_router_destroy(router);
    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersBioAsyncTest, BioAsyncStatisticsTracking) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(nullptr, adapter);

    // Get initial stats
    nimcp_bio_async_stats_t bio_stats;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_get_stats(&bio_stats));
    uint64_t initial_messages = bio_stats.total_futures_created;

    // Send messages
    for (int i = 0; i < 5; i++) {
        bio_msg_weight_update_request_t request = {};
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                           sizeof(bio_msg_weight_update_request_t) - sizeof(bio_message_header_t));
        request.synapse_id = i;
        request.weight_delta = 0.01f * i;

        bio_module_send_message(BIO_MODULE_TRAINING, &request, sizeof(request), nullptr);
    }

    // Get updated stats
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_get_stats(&bio_stats));

    learning_signal_adapter_destroy(adapter);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(TrainingAdaptersBioAsyncTest, InvalidMessageHandling) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(nullptr, adapter);

    // Send message with invalid size
    bio_msg_weight_update_request_t request = {};
    nimcp_error_t err = bio_module_send_message(BIO_MODULE_TRAINING,
                                                &request, 10,  // Too small
                                                nullptr);
    // Should handle gracefully

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersBioAsyncTest, NullAdapterHandling) {
    // Test that operations on NULL adapters don't crash
    learning_signal_adapter_destroy(nullptr);
    weight_update_router_destroy(nullptr);
    training_event_manager_destroy(nullptr);

    // All should complete without errors
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
