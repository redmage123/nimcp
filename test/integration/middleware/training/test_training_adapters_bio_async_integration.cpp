//=============================================================================
// test_training_adapters_bio_async_integration.cpp
// Integration tests for Training Adapters Bio-Async
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_adapters.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "core/events/nimcp_event_bus.h"

class TrainingAdaptersIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize memory subsystem
        nimcp_memory_init();

        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = true;
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_init(&bio_config));

        // Initialize bio-router for message sending
        bio_router_config_t router_config = bio_router_default_config();
        bio_router_init(&router_config);

        // Register a sender module
        bio_module_info_t sender_info = {
            .module_id = BIO_MODULE_STP,
            .module_name = "TestSender",
            .inbox_capacity = 64,
            .user_data = nullptr
        };
        sender_module_ = bio_router_register_module(&sender_info);

        // Create event bus
        event_bus = event_bus_create("integration_test", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(nullptr, event_bus);
    }

    void TearDown() override {
        if (event_bus) {
            event_bus_destroy(event_bus);
        }
        if (sender_module_) {
            bio_router_unregister_module(sender_module_);
            sender_module_ = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        nimcp_memory_cleanup();
    }

    bio_module_context_t sender_module_ = nullptr;

    event_bus_t event_bus = nullptr;
};

TEST_F(TrainingAdaptersIntegrationTest, EndToEndTrainingPipeline) {
    // Create complete training adapter pipeline
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    weight_update_router_t router = weight_update_router_create(nullptr, event_bus);
    ASSERT_NE(nullptr, router);

    training_event_manager_t manager = training_event_manager_create(nullptr, event_bus);
    ASSERT_NE(nullptr, manager);

    // Setup routing
    weight_update_router_add_route(router,
                                   LEARNING_SIGNAL_ERROR,
                                   WEIGHT_TARGET_CORTICAL,
                                   100);

    // Simulate training batch
    for (int batch = 0; batch < 3; batch++) {
        // Publish batch start
        training_event_data_t batch_start = {};
        batch_start.type = TRAINING_EVENT_BATCH_START;
        batch_start.batch = batch;
        batch_start.epoch = 0;
        training_event_manager_publish(manager, &batch_start);

        // Simulate weight updates
        for (int i = 0; i < 10; i++) {
            weight_update_t update = {};
            update.target_type = WEIGHT_TARGET_CORTICAL;
            update.source_neuron = i;
            update.target_neuron = i + 10;
            update.weight_delta = 0.001f * (i + 1);
            update.learning_rate = 0.01f;
            update.modulation_factor = 1.0f;

            weight_update_router_route(router, &update);
        }

        // Publish batch end with loss
        training_event_data_t batch_end = {};
        batch_end.type = TRAINING_EVENT_BATCH_END;
        batch_end.batch = batch;
        batch_end.epoch = 0;
        batch_end.loss = 0.5f - batch * 0.1f;
        training_event_manager_publish(manager, &batch_end);
    }

    // Process events
    event_bus_flush(event_bus);

    // Verify statistics
    weight_update_router_stats_t router_stats;
    ASSERT_TRUE(weight_update_router_get_stats(router, &router_stats));
    EXPECT_GE(router_stats.updates_routed, 30);

    training_event_manager_stats_t manager_stats;
    ASSERT_TRUE(training_event_manager_get_stats(manager, &manager_stats));
    EXPECT_GE(manager_stats.events_published, 6);

    // Cleanup
    training_event_manager_destroy(manager);
    weight_update_router_destroy(router);
    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersIntegrationTest, MultiChannelCommunication) {
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    // Send messages via different channels
    for (int channel = 0; channel < BIO_CHANNEL_COUNT; channel++) {
        bio_msg_weight_update_request_t request = {};
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                           sizeof(request) - sizeof(bio_message_header_t));
        request.header.channel = (nimcp_bio_channel_type_t)channel;
        request.synapse_id = channel;
        request.weight_delta = 0.01f;

        nimcp_bio_promise_t promise = nimcp_bio_promise_create(
            (nimcp_bio_channel_type_t)channel,
            sizeof(bio_msg_weight_update_response_t));

        bio_router_send(sender_module_, &request, sizeof(request), 500);

        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        bio_msg_weight_update_response_t response;
        nimcp_bio_future_wait(future, &response, 500);

        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promise);
    }

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersIntegrationTest, ConcurrentMessageProcessing) {
    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(nullptr, adapter);

    const int NUM_MESSAGES = 100;
    nimcp_bio_promise_t promises[NUM_MESSAGES];
    nimcp_bio_future_t futures[NUM_MESSAGES];

    // Send many messages concurrently
    for (int i = 0; i < NUM_MESSAGES; i++) {
        bio_msg_weight_update_request_t request = {};
        bio_msg_init_header(&request.header, BIO_MSG_WEIGHT_UPDATE_REQUEST,
                           BIO_MODULE_TRAINING, BIO_MODULE_TRAINING,
                           sizeof(request) - sizeof(bio_message_header_t));
        request.synapse_id = i;
        request.weight_delta = 0.001f * i;

        promises[i] = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE,
                                               sizeof(bio_msg_weight_update_response_t));

        bio_router_send(sender_module_, &request, sizeof(request), 500);
        futures[i] = nimcp_bio_promise_get_future(promises[i]);
    }

    // Wait for all responses
    int completed = 0;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        bio_msg_weight_update_response_t response;
        nimcp_error_t err = nimcp_bio_future_wait(futures[i], &response, 1000);
        if (err == NIMCP_SUCCESS) {
            completed++;
        }
    }

    // Cleanup
    for (int i = 0; i < NUM_MESSAGES; i++) {
        nimcp_bio_future_destroy(futures[i]);
        nimcp_bio_promise_destroy(promises[i]);
    }

    EXPECT_GT(completed, NUM_MESSAGES / 2);  // At least half should complete

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersIntegrationTest, EventSubscriptionCallback) {
    training_event_manager_t manager = training_event_manager_create(nullptr, event_bus);
    ASSERT_NE(nullptr, manager);

    // Track callback invocations
    static int callback_count = 0;
    auto callback = [](const training_event_data_t* event, void* context) {
        callback_count++;
    };

    // Subscribe
    event_subscription_handle_t handle = training_event_manager_subscribe(
        manager, callback, nullptr, nullptr, 0);
    EXPECT_NE(INVALID_SUBSCRIPTION_HANDLE, handle);

    // Publish events
    for (int i = 0; i < 5; i++) {
        training_event_data_t event = {};
        event.type = TRAINING_EVENT_LOSS_UPDATE;
        event.loss = 0.1f * i;
        training_event_manager_publish(manager, &event);
    }

    event_bus_flush(event_bus);

    // Unsubscribe
    training_event_manager_unsubscribe(manager, handle);

    training_event_manager_destroy(manager);
}

TEST_F(TrainingAdaptersIntegrationTest, RoutingTableDynamicUpdates) {
    weight_update_router_t router = weight_update_router_create(nullptr, event_bus);
    ASSERT_NE(nullptr, router);

    // Add and remove routes dynamically
    for (int i = 0; i < 5; i++) {
        bool added = weight_update_router_add_route(router,
                                                    LEARNING_SIGNAL_ERROR,
                                                    (weight_target_type_t)i,
                                                    i * 10);
        EXPECT_TRUE(added);
    }

    // Route some updates
    for (int i = 0; i < 10; i++) {
        weight_update_t update = {};
        update.target_type = (weight_target_type_t)(i % 5);
        update.weight_delta = 0.01f;
        weight_update_router_route(router, &update);
    }

    // Remove routes
    for (int i = 0; i < 3; i++) {
        bool removed = weight_update_router_remove_route(router,
                                                         LEARNING_SIGNAL_ERROR,
                                                         (weight_target_type_t)i);
        EXPECT_TRUE(removed);
    }

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GT(stats.updates_routed, 0);

    weight_update_router_destroy(router);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
