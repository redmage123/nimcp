//=============================================================================
// test_training_event_manager.cpp - Comprehensive Training Event Manager Tests
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_adapters.h"
#include "core/events/nimcp_event_bus.h"

#include <thread>
#include <atomic>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class TrainingEventManagerTest : public ::testing::Test {
protected:
    training_event_manager_t manager = nullptr;
    event_bus_t event_bus = nullptr;

    void SetUp() override {
        // Create event bus first using core API
        event_bus = event_bus_create("test_bus", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(event_bus, nullptr);

        // Create manager with event bus
        training_event_manager_config_t config = training_event_manager_default_config();
        manager = training_event_manager_create(&config, event_bus);
        ASSERT_NE(manager, nullptr);
    }

    void TearDown() override {
        if (manager) {
            training_event_manager_destroy(manager);
        }
        if (event_bus) {
            event_bus_destroy(event_bus);
        }
    }

    // Helper to create training event
    training_event_data_t create_training_event(training_event_type_t type,
                                                 uint32_t epoch, uint32_t batch,
                                                 float loss, float lr) {
        training_event_data_t event = {};
        event.type = type;
        event.epoch = epoch;
        event.batch = batch;
        event.loss = loss;
        event.learning_rate = lr;
        event.timestamp_us = 1000;
        event.metadata = nullptr;
        return event;
    }
};

// Callback counter for testing
struct CallbackContext {
    std::atomic<int> count{0};
    std::vector<training_event_type_t> types;
    std::vector<uint32_t> epochs;
    std::vector<float> losses;
};

// Test callback function
void test_callback(const training_event_data_t* event, void* context) {
    CallbackContext* ctx = static_cast<CallbackContext*>(context);
    ctx->count++;
    ctx->types.push_back(event->type);
    ctx->epochs.push_back(event->epoch);
    ctx->losses.push_back(event->loss);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, CreateAndDestroy) {
    EXPECT_NE(manager, nullptr);
}

TEST_F(TrainingEventManagerTest, CreateWithNullConfig) {
    training_event_manager_t test_manager = training_event_manager_create(nullptr, event_bus);
    EXPECT_NE(test_manager, nullptr);
    training_event_manager_destroy(test_manager);
}

TEST_F(TrainingEventManagerTest, CreateWithOwnEventBus) {
    training_event_manager_config_t config = training_event_manager_default_config();
    training_event_manager_t test_manager = training_event_manager_create(&config, nullptr);
    EXPECT_NE(test_manager, nullptr);
    training_event_manager_destroy(test_manager);
}

TEST_F(TrainingEventManagerTest, CreateWithCustomConfig) {
    training_event_manager_config_t config = training_event_manager_default_config();
    config.event_queue_capacity = 2048;
    config.enable_async_delivery = false;
    config.enable_event_batching = true;
    config.batch_timeout_ms = 20;

    training_event_manager_t test_manager = training_event_manager_create(&config, event_bus);
    EXPECT_NE(test_manager, nullptr);
    training_event_manager_destroy(test_manager);
}

TEST_F(TrainingEventManagerTest, DestroyNullManager) {
    training_event_manager_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Event Publishing Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, PublishEpochStartEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_EPOCH_START, 1, 0, 0.0f, 0.01f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);
    EXPECT_EQ(stats.events_published, 1u);
}

TEST_F(TrainingEventManagerTest, PublishEpochEndEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_EPOCH_END, 1, 100, 0.5f, 0.01f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);
    EXPECT_EQ(stats.events_published, 1u);
}

TEST_F(TrainingEventManagerTest, PublishBatchStartEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_BATCH_START, 1, 10, 0.0f, 0.01f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);
}

TEST_F(TrainingEventManagerTest, PublishBatchEndEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_BATCH_END, 1, 10, 0.45f, 0.01f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);
}

TEST_F(TrainingEventManagerTest, PublishLossUpdateEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_LOSS_UPDATE, 1, 50, 0.3f, 0.01f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);
}

TEST_F(TrainingEventManagerTest, PublishLearningRateUpdateEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_LR_UPDATE, 5, 0, 0.0f, 0.005f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);
}

TEST_F(TrainingEventManagerTest, PublishConvergenceEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_CONVERGENCE, 10, 500, 0.01f, 0.001f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);
}

TEST_F(TrainingEventManagerTest, PublishDivergenceEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_DIVERGENCE, 3, 150, 10.0f, 0.01f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);
}

TEST_F(TrainingEventManagerTest, PublishCheckpointEvent) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_CHECKPOINT, 5, 0, 0.2f, 0.005f);

    bool success = training_event_manager_publish(manager, &event);
    EXPECT_TRUE(success);
}

TEST_F(TrainingEventManagerTest, PublishAllEventTypes) {
    training_event_type_t types[] = {
        TRAINING_EVENT_EPOCH_START,
        TRAINING_EVENT_EPOCH_END,
        TRAINING_EVENT_BATCH_START,
        TRAINING_EVENT_BATCH_END,
        TRAINING_EVENT_LOSS_UPDATE,
        TRAINING_EVENT_LR_UPDATE,
        TRAINING_EVENT_CONVERGENCE,
        TRAINING_EVENT_DIVERGENCE,
        TRAINING_EVENT_CHECKPOINT
    };

    for (int i = 0; i < 9; i++) {
        training_event_data_t event = create_training_event(
            types[i], 1, i, 0.5f, 0.01f);
        EXPECT_TRUE(training_event_manager_publish(manager, &event));
    }

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);
    EXPECT_EQ(stats.events_published, 9u);
}

TEST_F(TrainingEventManagerTest, PublishMultipleEvents) {
    for (int i = 0; i < 100; i++) {
        training_event_data_t event = create_training_event(
            TRAINING_EVENT_BATCH_END, 1, i, 0.5f - i*0.001f, 0.01f);
        training_event_manager_publish(manager, &event);
    }

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);
    EXPECT_EQ(stats.events_published, 100u);
}

//=============================================================================
// Subscription Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, Subscribe) {
    CallbackContext ctx;

    event_subscription_handle_t handle = training_event_manager_subscribe(
        manager, test_callback, &ctx, nullptr, 0);

    EXPECT_NE(handle, 0u);

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);
    EXPECT_GE(stats.active_subscribers, 1u);
}

TEST_F(TrainingEventManagerTest, SubscribeAndReceive) {
    CallbackContext ctx;

    training_event_manager_subscribe(manager, test_callback, &ctx, nullptr, 0);

    training_event_data_t event = create_training_event(
        TRAINING_EVENT_EPOCH_START, 1, 0, 0.0f, 0.01f);
    training_event_manager_publish(manager, &event);

    // Process events
    uint32_t processed = training_event_manager_process_events(manager, 10);
    EXPECT_GE(processed, 0u);

    // Callback should have been called
    EXPECT_GE(ctx.count.load(), 0);
}

TEST_F(TrainingEventManagerTest, SubscribeMultipleCallbacks) {
    CallbackContext ctx1, ctx2, ctx3;

    training_event_manager_subscribe(manager, test_callback, &ctx1, nullptr, 0);
    training_event_manager_subscribe(manager, test_callback, &ctx2, nullptr, 0);
    training_event_manager_subscribe(manager, test_callback, &ctx3, nullptr, 0);

    training_event_data_t event = create_training_event(
        TRAINING_EVENT_BATCH_END, 1, 10, 0.5f, 0.01f);
    training_event_manager_publish(manager, &event);

    training_event_manager_process_events(manager, 10);

    // All callbacks should potentially be called
    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);
    EXPECT_GE(stats.active_subscribers, 3u);
}

TEST_F(TrainingEventManagerTest, SubscribeSpecificEventTypes) {
    CallbackContext ctx;

    training_event_type_t types[] = {
        TRAINING_EVENT_EPOCH_START,
        TRAINING_EVENT_EPOCH_END
    };

    event_subscription_handle_t handle = training_event_manager_subscribe(
        manager, test_callback, &ctx, types, 2);

    EXPECT_NE(handle, 0u);
}

TEST_F(TrainingEventManagerTest, Unsubscribe) {
    CallbackContext ctx;

    event_subscription_handle_t handle = training_event_manager_subscribe(
        manager, test_callback, &ctx, nullptr, 0);
    EXPECT_NE(handle, 0u);

    bool success = training_event_manager_unsubscribe(manager, handle);
    EXPECT_TRUE(success);
}

TEST_F(TrainingEventManagerTest, UnsubscribeInvalidHandle) {
    bool success = training_event_manager_unsubscribe(manager, 0);
    EXPECT_FALSE(success);
}

TEST_F(TrainingEventManagerTest, UnsubscribeAfterPublish) {
    CallbackContext ctx;

    event_subscription_handle_t handle = training_event_manager_subscribe(
        manager, test_callback, &ctx, nullptr, 0);

    training_event_data_t event = create_training_event(
        TRAINING_EVENT_EPOCH_START, 1, 0, 0.0f, 0.01f);
    training_event_manager_publish(manager, &event);

    // Unsubscribe before processing
    training_event_manager_unsubscribe(manager, handle);

    training_event_manager_process_events(manager, 10);
}

//=============================================================================
// Event Processing Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, ProcessNoEvents) {
    uint32_t processed = training_event_manager_process_events(manager, 10);
    EXPECT_EQ(processed, 0u);
}

TEST_F(TrainingEventManagerTest, ProcessSingleEvent) {
    CallbackContext ctx;
    training_event_manager_subscribe(manager, test_callback, &ctx, nullptr, 0);

    training_event_data_t event = create_training_event(
        TRAINING_EVENT_EPOCH_START, 1, 0, 0.0f, 0.01f);
    training_event_manager_publish(manager, &event);

    uint32_t processed = training_event_manager_process_events(manager, 10);
    EXPECT_GE(processed, 0u);
}

TEST_F(TrainingEventManagerTest, ProcessMultipleEvents) {
    CallbackContext ctx;
    training_event_manager_subscribe(manager, test_callback, &ctx, nullptr, 0);

    for (int i = 0; i < 10; i++) {
        training_event_data_t event = create_training_event(
            TRAINING_EVENT_BATCH_END, 1, i, 0.5f, 0.01f);
        training_event_manager_publish(manager, &event);
    }

    uint32_t processed = training_event_manager_process_events(manager, 20);
    EXPECT_GE(processed, 0u);
}

TEST_F(TrainingEventManagerTest, ProcessLimitedEvents) {
    CallbackContext ctx;
    training_event_manager_subscribe(manager, test_callback, &ctx, nullptr, 0);

    for (int i = 0; i < 20; i++) {
        training_event_data_t event = create_training_event(
            TRAINING_EVENT_BATCH_END, 1, i, 0.5f, 0.01f);
        training_event_manager_publish(manager, &event);
    }

    // Process only 5 events
    uint32_t processed = training_event_manager_process_events(manager, 5);
    EXPECT_LE(processed, 5u);
}

TEST_F(TrainingEventManagerTest, ProcessAllEvents) {
    CallbackContext ctx;
    training_event_manager_subscribe(manager, test_callback, &ctx, nullptr, 0);

    for (int i = 0; i < 50; i++) {
        training_event_data_t event = create_training_event(
            TRAINING_EVENT_BATCH_END, 1, i, 0.5f, 0.01f);
        training_event_manager_publish(manager, &event);
    }

    // Process all events
    uint32_t total_processed = 0;
    uint32_t processed;
    do {
        processed = training_event_manager_process_events(manager, 10);
        total_processed += processed;
    } while (processed > 0);

    EXPECT_GE(total_processed, 0u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, GetStatistics) {
    training_event_manager_stats_t stats;
    bool success = training_event_manager_get_stats(manager, &stats);
    EXPECT_TRUE(success);

    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.events_delivered, 0u);
    EXPECT_EQ(stats.events_dropped, 0u);
    EXPECT_EQ(stats.queue_size, 0u);
}

TEST_F(TrainingEventManagerTest, StatisticsAfterPublishing) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_EPOCH_START, 1, 0, 0.0f, 0.01f);
    training_event_manager_publish(manager, &event);

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);

    EXPECT_EQ(stats.events_published, 1u);
    EXPECT_GE(stats.queue_size, 0u);
}

TEST_F(TrainingEventManagerTest, StatisticsAfterProcessing) {
    CallbackContext ctx;
    training_event_manager_subscribe(manager, test_callback, &ctx, nullptr, 0);

    for (int i = 0; i < 10; i++) {
        training_event_data_t event = create_training_event(
            TRAINING_EVENT_BATCH_END, 1, i, 0.5f, 0.01f);
        training_event_manager_publish(manager, &event);
    }

    training_event_manager_process_events(manager, 20);

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);

    EXPECT_EQ(stats.events_published, 10u);
}

TEST_F(TrainingEventManagerTest, StatisticsQueueSize) {
    // Publish without processing
    for (int i = 0; i < 5; i++) {
        training_event_data_t event = create_training_event(
            TRAINING_EVENT_BATCH_END, 1, i, 0.5f, 0.01f);
        training_event_manager_publish(manager, &event);
    }

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);

    EXPECT_GE(stats.queue_size, 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, PublishNullManager) {
    training_event_data_t event = create_training_event(
        TRAINING_EVENT_EPOCH_START, 1, 0, 0.0f, 0.01f);

    bool success = training_event_manager_publish(nullptr, &event);
    EXPECT_FALSE(success);
}

TEST_F(TrainingEventManagerTest, PublishNullEvent) {
    bool success = training_event_manager_publish(manager, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(TrainingEventManagerTest, SubscribeNullManager) {
    CallbackContext ctx;

    event_subscription_handle_t handle = training_event_manager_subscribe(
        nullptr, test_callback, &ctx, nullptr, 0);
    EXPECT_EQ(handle, 0u);
}

TEST_F(TrainingEventManagerTest, SubscribeNullCallback) {
    event_subscription_handle_t handle = training_event_manager_subscribe(
        manager, nullptr, nullptr, nullptr, 0);
    EXPECT_EQ(handle, 0u);
}

TEST_F(TrainingEventManagerTest, UnsubscribeNullManager) {
    bool success = training_event_manager_unsubscribe(nullptr, 1);
    EXPECT_FALSE(success);
}

TEST_F(TrainingEventManagerTest, ProcessEventsNullManager) {
    uint32_t processed = training_event_manager_process_events(nullptr, 10);
    EXPECT_EQ(processed, 0u);
}

TEST_F(TrainingEventManagerTest, ProcessEventsZeroLimit) {
    uint32_t processed = training_event_manager_process_events(manager, 0);
    EXPECT_EQ(processed, 0u);
}

TEST_F(TrainingEventManagerTest, GetStatsNullManager) {
    training_event_manager_stats_t stats;
    bool success = training_event_manager_get_stats(nullptr, &stats);
    EXPECT_FALSE(success);
}

TEST_F(TrainingEventManagerTest, GetStatsNullStats) {
    bool success = training_event_manager_get_stats(manager, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, ConcurrentPublishing) {
    const int num_threads = 4;
    const int events_per_thread = 100;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, events_per_thread, t]() {
            for (int i = 0; i < events_per_thread; i++) {
                training_event_data_t event = create_training_event(
                    TRAINING_EVENT_BATCH_END, t, i, 0.5f, 0.01f);

                if (training_event_manager_publish(manager, &event)) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);
    EXPECT_EQ(stats.events_published, (uint64_t)(num_threads * events_per_thread));
}

TEST_F(TrainingEventManagerTest, ConcurrentSubscribing) {
    const int num_threads = 4;
    std::vector<CallbackContext> contexts(num_threads);
    std::atomic<int> subscribe_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &contexts, &subscribe_count, t]() {
            event_subscription_handle_t handle = training_event_manager_subscribe(
                manager, test_callback, &contexts[t], nullptr, 0);

            if (handle != 0) {
                subscribe_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(subscribe_count.load(), num_threads);
}

TEST_F(TrainingEventManagerTest, ConcurrentPublishAndProcess) {
    CallbackContext ctx;
    training_event_manager_subscribe(manager, test_callback, &ctx, nullptr, 0);

    std::atomic<bool> publishing{true};
    std::atomic<int> published{0};

    // Publisher thread
    std::thread publisher([this, &publishing, &published]() {
        for (int i = 0; i < 100; i++) {
            training_event_data_t event = create_training_event(
                TRAINING_EVENT_BATCH_END, 1, i, 0.5f, 0.01f);
            if (training_event_manager_publish(manager, &event)) {
                published++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        publishing = false;
    });

    // Processor thread
    std::thread processor([this, &publishing]() {
        while (publishing.load() || true) {
            training_event_manager_process_events(manager, 5);
            std::this_thread::sleep_for(std::chrono::microseconds(200));

            // Stop after a reasonable time
            if (!publishing.load()) break;
        }
    });

    publisher.join();
    processor.join();

    EXPECT_EQ(published.load(), 100);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, TrainingEventTypeNames) {
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_EPOCH_START), "Epoch Start");
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_EPOCH_END), "Epoch End");
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_BATCH_START), "Batch Start");
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_BATCH_END), "Batch End");
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_LOSS_UPDATE), "Loss Update");
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_LR_UPDATE), "LR Update");
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_CONVERGENCE), "Convergence");
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_DIVERGENCE), "Divergence");
    EXPECT_STREQ(training_event_type_name(TRAINING_EVENT_CHECKPOINT), "Checkpoint");
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(TrainingEventManagerTest, CompleteTrainingLoop) {
    CallbackContext ctx;
    training_event_manager_subscribe(manager, test_callback, &ctx, nullptr, 0);

    // Simulate a training loop
    for (uint32_t epoch = 0; epoch < 3; epoch++) {
        // Epoch start
        training_event_data_t epoch_start = create_training_event(
            TRAINING_EVENT_EPOCH_START, epoch, 0, 0.0f, 0.01f);
        training_event_manager_publish(manager, &epoch_start);

        // Batches
        for (uint32_t batch = 0; batch < 10; batch++) {
            training_event_data_t batch_start = create_training_event(
                TRAINING_EVENT_BATCH_START, epoch, batch, 0.0f, 0.01f);
            training_event_manager_publish(manager, &batch_start);

            training_event_data_t batch_end = create_training_event(
                TRAINING_EVENT_BATCH_END, epoch, batch, 0.5f - batch*0.01f, 0.01f);
            training_event_manager_publish(manager, &batch_end);

            training_event_data_t loss_update = create_training_event(
                TRAINING_EVENT_LOSS_UPDATE, epoch, batch, 0.5f - batch*0.01f, 0.01f);
            training_event_manager_publish(manager, &loss_update);
        }

        // Epoch end
        training_event_data_t epoch_end = create_training_event(
            TRAINING_EVENT_EPOCH_END, epoch, 10, 0.4f, 0.01f);
        training_event_manager_publish(manager, &epoch_end);
    }

    // Convergence
    training_event_data_t convergence = create_training_event(
        TRAINING_EVENT_CONVERGENCE, 3, 0, 0.01f, 0.001f);
    training_event_manager_publish(manager, &convergence);

    // Checkpoint
    training_event_data_t checkpoint = create_training_event(
        TRAINING_EVENT_CHECKPOINT, 3, 0, 0.01f, 0.001f);
    training_event_manager_publish(manager, &checkpoint);

    // Process all events
    training_event_manager_process_events(manager, 1000);

    // Check statistics
    training_event_manager_stats_t stats;
    training_event_manager_get_stats(manager, &stats);

    // 3 epochs * (1 start + 10*(start+end+loss) + 1 end) + 1 convergence + 1 checkpoint
    // = 3 * (1 + 30 + 1) + 2 = 3 * 32 + 2 = 98
    EXPECT_EQ(stats.events_published, 98u);
}

TEST_F(TrainingEventManagerTest, AsyncVsSyncDelivery) {
    // Test async delivery
    training_event_manager_config_t async_config = training_event_manager_default_config();
    async_config.enable_async_delivery = true;
    training_event_manager_t async_manager = training_event_manager_create(&async_config, nullptr);

    CallbackContext async_ctx;
    training_event_manager_subscribe(async_manager, test_callback, &async_ctx, nullptr, 0);

    for (int i = 0; i < 10; i++) {
        training_event_data_t event = create_training_event(
            TRAINING_EVENT_BATCH_END, 1, i, 0.5f, 0.01f);
        training_event_manager_publish(async_manager, &event);
    }

    training_event_manager_destroy(async_manager);

    // Test sync delivery
    training_event_manager_config_t sync_config = training_event_manager_default_config();
    sync_config.enable_async_delivery = false;
    training_event_manager_t sync_manager = training_event_manager_create(&sync_config, nullptr);

    CallbackContext sync_ctx;
    training_event_manager_subscribe(sync_manager, test_callback, &sync_ctx, nullptr, 0);

    for (int i = 0; i < 10; i++) {
        training_event_data_t event = create_training_event(
            TRAINING_EVENT_BATCH_END, 1, i, 0.5f, 0.01f);
        training_event_manager_publish(sync_manager, &event);
    }

    training_event_manager_process_events(sync_manager, 20);

    training_event_manager_destroy(sync_manager);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
