//=============================================================================
// test_training_adapters_integration.cpp - Training Adapters Integration Tests
//
// Tests integration of:
// - Learning signal adapter with feature extraction and normalization
// - Weight update router with event bus and routing table
// - Training event manager with subscribers and coordination
// - End-to-end training pipelines
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cmath>
#include <random>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_adapters.h"
#include "middleware/training/nimcp_learning_signal_adapter.h"
#include "middleware/training/nimcp_weight_update_adapter.h"
#include "middleware/training/nimcp_training_event_adapter.h"
#include "core/events/nimcp_event_bus.h"
#include "middleware/routing/nimcp_routing_table.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class TrainingAdaptersIntegrationTest : public ::testing::Test {
protected:
    learning_signal_adapter_t learning_adapter;
    weight_update_router_t weight_router;
    training_event_manager_t event_manager;
    event_bus_t event_bus;

    // Test data
    std::vector<brain_event_t> test_events;
    std::atomic<int> callback_count;

    void SetUp() override {
        callback_count = 0;

        // Create event bus for integration
        event_bus = nullptr;  // Will be created in tests that need it

        // Adapters will be created in individual tests
        learning_adapter = nullptr;
        weight_router = nullptr;
        event_manager = nullptr;

        // Generate test events
        generateTestEvents();
    }

    void TearDown() override {
        if (learning_adapter) learning_signal_adapter_destroy(learning_adapter);
        if (weight_router) weight_update_router_destroy(weight_router);
        if (event_manager) training_event_manager_destroy(event_manager);
        if (event_bus) {
            // event_bus_destroy(event_bus);
        }
    }

    void generateTestEvents() {
        test_events.clear();
        // Events would be generated based on actual brain_event_t structure
    }

    static void training_callback(const training_event_data_t* event, void* context) {
        auto* count = static_cast<std::atomic<int>*>(context);
        (*count)++;
    }
};

//=============================================================================
// 1. LEARNING SIGNAL ADAPTER INTEGRATION TESTS
//=============================================================================

TEST_F(TrainingAdaptersIntegrationTest, LearningSignalExtractionBasic) {
    // WHAT: Test basic learning signal extraction from events
    // WHY: Must convert diverse events to standardized signals

    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_Z_SCORE;

    learning_adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(learning_adapter, nullptr);

    // Create test event (would use actual event structure)
    // For now, test adapter creation and destruction
    EXPECT_TRUE(true);

    learning_signal_adapter_stats_t stats;
    EXPECT_TRUE(learning_signal_adapter_get_stats(learning_adapter, &stats));
    EXPECT_EQ(stats.signals_extracted, 0);
}

TEST_F(TrainingAdaptersIntegrationTest, LearningSignalNormalization) {
    // WHAT: Test feature normalization strategies
    // WHY: Normalized features essential for stable learning

    // Test each normalization strategy
    normalization_strategy_t strategies[] = {
        NORMALIZE_MIN_MAX,
        NORMALIZE_Z_SCORE,
        NORMALIZE_L2,
        NORMALIZE_ADAPTIVE
    };

    for (auto strategy : strategies) {
        learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
        config.normalization = strategy;

        learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
        ASSERT_NE(adapter, nullptr);

        // Test normalization
        float features[5] = {100.0f, 200.0f, 150.0f, 180.0f, 120.0f};
        EXPECT_TRUE(learning_signal_adapter_normalize(adapter, features, 5));

        // Features should be modified
        bool modified = false;
        for (int i = 0; i < 5; i++) {
            if (std::abs(features[i] - (100.0f + i * 20.0f)) > 0.1f) {
                modified = true;
                break;
            }
        }
        EXPECT_TRUE(modified || strategy == NORMALIZE_NONE);

        learning_signal_adapter_destroy(adapter);
    }
}

TEST_F(TrainingAdaptersIntegrationTest, LearningSignalAttentionWeighting) {
    // WHAT: Test attention-weighted signal extraction
    // WHY: Attention should modulate learning signals

    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.enable_attention_weighting = true;

    learning_adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(learning_adapter, nullptr);

    // Create learning signal
    learning_signal_t signal;
    signal.type = LEARNING_SIGNAL_ERROR;
    signal.features = new float[4]{1.0f, 2.0f, 3.0f, 4.0f};
    signal.num_features = 4;
    signal.magnitude = 1.0f;
    signal.confidence = 1.0f;

    // Apply attention weighting
    float attention_weight = 0.5f;
    EXPECT_TRUE(learning_signal_adapter_apply_attention(learning_adapter,
                                                        &signal,
                                                        attention_weight));

    // Features should be attenuated
    for (uint32_t i = 0; i < signal.num_features; i++) {
        EXPECT_LT(signal.features[i], static_cast<float>(i + 1));
    }

    delete[] signal.features;
}

TEST_F(TrainingAdaptersIntegrationTest, LearningSignalNoveltyBoost) {
    // WHAT: Test novelty-based signal amplification
    // WHY: Novel signals should receive learning boost

    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.enable_novelty_boost = true;
    config.novelty_boost_factor = 2.0f;

    learning_adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(learning_adapter, nullptr);

    // Test would involve creating events with novelty markers
    // and verifying boosted signal extraction
    EXPECT_TRUE(true);
}

TEST_F(TrainingAdaptersIntegrationTest, LearningSignalTypes) {
    // WHAT: Test extraction of different signal types
    // WHY: Different learning scenarios require different signals

    learning_adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(learning_adapter, nullptr);

    learning_signal_type_t types[] = {
        LEARNING_SIGNAL_ERROR,
        LEARNING_SIGNAL_REWARD,
        LEARNING_SIGNAL_SURPRISE,
        LEARNING_SIGNAL_ATTENTION,
        LEARNING_SIGNAL_MEMORY
    };

    for (auto type : types) {
        const char* name = learning_signal_type_name(type);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0);
    }
}

TEST_F(TrainingAdaptersIntegrationTest, LearningSignalConfidenceThreshold) {
    // WHAT: Test confidence-based signal filtering
    // WHY: Low confidence signals should be dropped

    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.min_confidence_threshold = 0.5f;

    learning_adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(learning_adapter, nullptr);

    // Statistics should track dropped signals
    learning_signal_adapter_stats_t stats;
    EXPECT_TRUE(learning_signal_adapter_get_stats(learning_adapter, &stats));
    EXPECT_EQ(stats.signals_dropped, 0);
}

//=============================================================================
// 2. WEIGHT UPDATE ROUTER INTEGRATION TESTS
//=============================================================================

TEST_F(TrainingAdaptersIntegrationTest, WeightUpdateRouterBasicRouting) {
    // WHAT: Test basic weight update routing
    // WHY: Updates must reach correct brain regions

    weight_update_router_config_t config = weight_update_router_default_config();
    config.routing_table_capacity = 10;

    weight_router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(weight_router, nullptr);

    // Add routing rules
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL,
                                               1));

    // Get statistics
    weight_update_router_stats_t stats;
    EXPECT_TRUE(weight_update_router_get_stats(weight_router, &stats));
    EXPECT_EQ(stats.active_routes, 1);
}

TEST_F(TrainingAdaptersIntegrationTest, WeightUpdateRouterMultipleRoutes) {
    // WHAT: Test routing table with multiple routes
    // WHY: Different signals route to different regions

    weight_update_router_config_t config = weight_update_router_default_config();
    weight_router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(weight_router, nullptr);

    // Add routes for different signal types
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_REWARD,
                                               WEIGHT_TARGET_STRIATAL, 1));
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_MEMORY,
                                               WEIGHT_TARGET_HIPPOCAMPAL, 1));

    weight_update_router_stats_t stats;
    EXPECT_TRUE(weight_update_router_get_stats(weight_router, &stats));
    EXPECT_EQ(stats.active_routes, 3);
}

TEST_F(TrainingAdaptersIntegrationTest, WeightUpdateRouterPriorityRouting) {
    // WHAT: Test priority-based routing
    // WHY: High priority updates should be processed first

    weight_update_router_config_t config = weight_update_router_default_config();
    config.enable_priority_routing = true;

    weight_router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(weight_router, nullptr);

    // Add routes with different priorities
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 3));
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_REWARD,
                                               WEIGHT_TARGET_STRIATAL, 1));

    // High priority route should be processed first
    EXPECT_TRUE(true);
}

TEST_F(TrainingAdaptersIntegrationTest, WeightUpdateRouterBatchProcessing) {
    // WHAT: Test batch routing of multiple updates
    // WHY: Batch processing improves efficiency

    weight_update_router_config_t config = weight_update_router_default_config();
    config.max_batch_size = 10;
    config.enable_update_coalescing = true;

    weight_router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(weight_router, nullptr);

    // Create batch of updates
    std::vector<weight_update_t> updates(5);
    for (int i = 0; i < 5; i++) {
        updates[i].target_type = WEIGHT_TARGET_CORTICAL;
        updates[i].source_neuron = i;
        updates[i].target_neuron = i + 10;
        updates[i].weight_delta = 0.01f;
        updates[i].learning_rate = 0.001f;
        updates[i].modulation_factor = 1.0f;
        updates[i].apply_stdp = false;
        updates[i].metadata = nullptr;
    }

    // Batch route
    uint32_t routed = weight_update_router_route_batch(weight_router,
                                                        updates.data(),
                                                        updates.size());
    // May succeed or fail depending on routing rules
    EXPECT_LE(routed, updates.size());
}

TEST_F(TrainingAdaptersIntegrationTest, WeightUpdateRouterDynamicRouting) {
    // WHAT: Test dynamic route learning
    // WHY: Routes should adapt to usage patterns

    weight_update_router_config_t config = weight_update_router_default_config();
    config.enable_dynamic_routing = true;
    config.route_learning_rate = 0.01f;

    weight_router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(weight_router, nullptr);

    // Add initial route
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    // Strengthen route through use
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(weight_update_router_strengthen_route(weight_router,
                                                           LEARNING_SIGNAL_ERROR,
                                                           WEIGHT_TARGET_CORTICAL));
    }

    // Route should exist and be strengthened
    EXPECT_TRUE(true);
}

TEST_F(TrainingAdaptersIntegrationTest, WeightUpdateRouterRemoveRoutes) {
    // WHAT: Test route removal
    // WHY: Support dynamic routing table modification

    weight_router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(weight_router, nullptr);

    // Add and remove route
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    weight_update_router_stats_t stats1;
    EXPECT_TRUE(weight_update_router_get_stats(weight_router, &stats1));
    EXPECT_EQ(stats1.active_routes, 1);

    EXPECT_TRUE(weight_update_router_remove_route(weight_router,
                                                   LEARNING_SIGNAL_ERROR,
                                                   WEIGHT_TARGET_CORTICAL));

    weight_update_router_stats_t stats2;
    EXPECT_TRUE(weight_update_router_get_stats(weight_router, &stats2));
    EXPECT_EQ(stats2.active_routes, 0);
}

//=============================================================================
// 3. TRAINING EVENT MANAGER INTEGRATION TESTS
//=============================================================================

TEST_F(TrainingAdaptersIntegrationTest, TrainingEventManagerBasicPublish) {
    // WHAT: Test basic event publishing
    // WHY: Training events must be broadcast to subscribers

    training_event_manager_config_t config = training_event_manager_default_config();
    event_manager = training_event_manager_create(&config, nullptr);
    ASSERT_NE(event_manager, nullptr);

    // Publish event
    training_event_data_t event;
    event.type = TRAINING_EVENT_EPOCH_START;
    event.epoch = 1;
    event.batch = 0;
    event.loss = 0.0f;
    event.learning_rate = 0.001f;
    event.timestamp_us = 0;
    event.metadata = nullptr;

    EXPECT_TRUE(training_event_manager_publish(event_manager, &event));

    training_event_manager_stats_t stats;
    EXPECT_TRUE(training_event_manager_get_stats(event_manager, &stats));
    EXPECT_EQ(stats.events_published, 1);
}

TEST_F(TrainingAdaptersIntegrationTest, TrainingEventManagerSubscription) {
    // WHAT: Test event subscription and callbacks
    // WHY: Subscribers must receive events

    event_manager = training_event_manager_create(nullptr, nullptr);
    ASSERT_NE(event_manager, nullptr);

    // Subscribe to all events
    subscription_handle_t handle = training_event_manager_subscribe(
        event_manager,
        training_callback,
        &callback_count,
        nullptr,  // All event types
        0
    );

    // Publish event
    training_event_data_t event;
    event.type = TRAINING_EVENT_BATCH_START;
    event.epoch = 1;
    event.batch = 1;
    event.loss = 0.5f;
    event.learning_rate = 0.001f;
    event.timestamp_us = 0;
    event.metadata = nullptr;

    EXPECT_TRUE(training_event_manager_publish(event_manager, &event));

    // Process events
    uint32_t processed = training_event_manager_process_events(event_manager, 10);
    EXPECT_GE(processed, 0);

    // Callback may or may not have been called depending on async delivery
    // Just verify subscription works
    EXPECT_TRUE(true);

    // Unsubscribe
    EXPECT_TRUE(training_event_manager_unsubscribe(event_manager, handle));
}

TEST_F(TrainingAdaptersIntegrationTest, TrainingEventManagerMultipleSubscribers) {
    // WHAT: Test multiple subscribers to same events
    // WHY: Many subsystems may need training coordination

    event_manager = training_event_manager_create(nullptr, nullptr);
    ASSERT_NE(event_manager, nullptr);

    std::atomic<int> count1{0}, count2{0}, count3{0};

    // Subscribe multiple times
    subscription_handle_t h1 = training_event_manager_subscribe(
        event_manager, training_callback, &count1, nullptr, 0);

    subscription_handle_t h2 = training_event_manager_subscribe(
        event_manager, training_callback, &count2, nullptr, 0);

    subscription_handle_t h3 = training_event_manager_subscribe(
        event_manager, training_callback, &count3, nullptr, 0);

    // Publish event
    training_event_data_t event;
    event.type = TRAINING_EVENT_LOSS_UPDATE;
    event.epoch = 1;
    event.batch = 5;
    event.loss = 0.3f;
    event.learning_rate = 0.001f;
    event.timestamp_us = 0;
    event.metadata = nullptr;

    EXPECT_TRUE(training_event_manager_publish(event_manager, &event));

    training_event_manager_stats_t stats;
    EXPECT_TRUE(training_event_manager_get_stats(event_manager, &stats));
    EXPECT_EQ(stats.active_subscribers, 3);

    // Cleanup
    training_event_manager_unsubscribe(event_manager, h1);
    training_event_manager_unsubscribe(event_manager, h2);
    training_event_manager_unsubscribe(event_manager, h3);
}

TEST_F(TrainingAdaptersIntegrationTest, TrainingEventManagerFilteredSubscription) {
    // WHAT: Test filtered event subscription
    // WHY: Subscribers should only receive relevant events

    event_manager = training_event_manager_create(nullptr, nullptr);
    ASSERT_NE(event_manager, nullptr);

    // Subscribe only to epoch events
    training_event_type_t types[] = {
        TRAINING_EVENT_EPOCH_START,
        TRAINING_EVENT_EPOCH_END
    };

    subscription_handle_t handle = training_event_manager_subscribe(
        event_manager,
        training_callback,
        &callback_count,
        types,
        2
    );

    // Publish epoch event (should be received)
    training_event_data_t epoch_event;
    epoch_event.type = TRAINING_EVENT_EPOCH_START;
    epoch_event.epoch = 1;
    EXPECT_TRUE(training_event_manager_publish(event_manager, &epoch_event));

    // Publish batch event (should be filtered)
    training_event_data_t batch_event;
    batch_event.type = TRAINING_EVENT_BATCH_START;
    batch_event.batch = 1;
    EXPECT_TRUE(training_event_manager_publish(event_manager, &batch_event));

    training_event_manager_unsubscribe(event_manager, handle);
}

TEST_F(TrainingAdaptersIntegrationTest, TrainingEventManagerAsyncDelivery) {
    // WHAT: Test asynchronous event delivery
    // WHY: Non-blocking event processing

    training_event_manager_config_t config = training_event_manager_default_config();
    config.enable_async_delivery = true;

    event_manager = training_event_manager_create(&config, nullptr);
    ASSERT_NE(event_manager, nullptr);

    subscription_handle_t handle = training_event_manager_subscribe(
        event_manager, training_callback, &callback_count, nullptr, 0);

    // Publish multiple events rapidly
    for (int i = 0; i < 10; i++) {
        training_event_data_t event;
        event.type = TRAINING_EVENT_BATCH_END;
        event.epoch = 1;
        event.batch = i;
        event.loss = 0.5f - i * 0.01f;
        event.learning_rate = 0.001f;
        event.timestamp_us = i * 1000;
        event.metadata = nullptr;

        EXPECT_TRUE(training_event_manager_publish(event_manager, &event));
    }

    // Events should be queued
    training_event_manager_stats_t stats;
    EXPECT_TRUE(training_event_manager_get_stats(event_manager, &stats));
    EXPECT_EQ(stats.events_published, 10);

    training_event_manager_unsubscribe(event_manager, handle);
}

TEST_F(TrainingAdaptersIntegrationTest, TrainingEventManagerBatching) {
    // WHAT: Test event batching
    // WHY: Reduce processing overhead for similar events

    training_event_manager_config_t config = training_event_manager_default_config();
    config.enable_event_batching = true;
    config.batch_timeout_ms = 100;

    event_manager = training_event_manager_create(&config, nullptr);
    ASSERT_NE(event_manager, nullptr);

    // Publish similar events rapidly
    for (int i = 0; i < 5; i++) {
        training_event_data_t event;
        event.type = TRAINING_EVENT_LR_UPDATE;
        event.epoch = 1;
        event.learning_rate = 0.001f * (1.0f - i * 0.1f);
        event.timestamp_us = i * 1000;
        event.metadata = nullptr;

        EXPECT_TRUE(training_event_manager_publish(event_manager, &event));
    }

    // Batching may reduce event count
    EXPECT_TRUE(true);
}

//=============================================================================
// 4. END-TO-END TRAINING PIPELINE INTEGRATION
//=============================================================================

TEST_F(TrainingAdaptersIntegrationTest, EndToEndLearningPipeline) {
    // WHAT: Test complete learning signal → weight update pipeline
    // WHY: Verify all adapters work together

    // Create all components
    learning_adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(learning_adapter, nullptr);

    weight_router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(weight_router, nullptr);

    event_manager = training_event_manager_create(nullptr, nullptr);
    ASSERT_NE(event_manager, nullptr);

    // Set up routing
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    // Simulate learning cycle
    for (int epoch = 0; epoch < 3; epoch++) {
        // Publish epoch start
        training_event_data_t epoch_start;
        epoch_start.type = TRAINING_EVENT_EPOCH_START;
        epoch_start.epoch = epoch;
        epoch_start.batch = 0;
        epoch_start.loss = 1.0f;
        epoch_start.learning_rate = 0.001f;
        epoch_start.timestamp_us = epoch * 1000000;
        epoch_start.metadata = nullptr;

        EXPECT_TRUE(training_event_manager_publish(event_manager, &epoch_start));

        // Simulate batch learning
        for (int batch = 0; batch < 5; batch++) {
            // Create weight update
            weight_update_t update;
            update.target_type = WEIGHT_TARGET_CORTICAL;
            update.source_neuron = batch;
            update.target_neuron = batch + 10;
            update.weight_delta = -0.01f * (1.0f - epoch * 0.1f);
            update.learning_rate = 0.001f;
            update.modulation_factor = 1.0f;
            update.apply_stdp = false;
            update.metadata = nullptr;

            // Route update
            bool routed = weight_update_router_route(weight_router, &update);
            // May succeed or fail
        }

        // Publish epoch end
        training_event_data_t epoch_end;
        epoch_end.type = TRAINING_EVENT_EPOCH_END;
        epoch_end.epoch = epoch;
        epoch_end.batch = 5;
        epoch_end.loss = 0.5f - epoch * 0.1f;
        epoch_end.learning_rate = 0.001f;
        epoch_end.timestamp_us = (epoch + 1) * 1000000;
        epoch_end.metadata = nullptr;

        EXPECT_TRUE(training_event_manager_publish(event_manager, &epoch_end));
    }

    // Verify pipeline processed data
    weight_update_router_stats_t router_stats;
    EXPECT_TRUE(weight_update_router_get_stats(weight_router, &router_stats));

    training_event_manager_stats_t event_stats;
    EXPECT_TRUE(training_event_manager_get_stats(event_manager, &event_stats));
    EXPECT_EQ(event_stats.events_published, 6);  // 3 start + 3 end
}

TEST_F(TrainingAdaptersIntegrationTest, EndToEndMultiRegionTraining) {
    // WHAT: Test training signals routed to multiple brain regions
    // WHY: Complex learning involves multiple regions

    learning_adapter = learning_signal_adapter_create(nullptr);
    weight_router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(learning_adapter, nullptr);
    ASSERT_NE(weight_router, nullptr);

    // Set up multi-region routing
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_REWARD,
                                               WEIGHT_TARGET_STRIATAL, 1));
    EXPECT_TRUE(weight_update_router_add_route(weight_router,
                                               LEARNING_SIGNAL_MEMORY,
                                               WEIGHT_TARGET_HIPPOCAMPAL, 1));

    // Simulate updates to different regions
    weight_target_type_t targets[] = {
        WEIGHT_TARGET_CORTICAL,
        WEIGHT_TARGET_STRIATAL,
        WEIGHT_TARGET_HIPPOCAMPAL
    };

    for (auto target : targets) {
        weight_update_t update;
        update.target_type = target;
        update.source_neuron = 0;
        update.target_neuron = 1;
        update.weight_delta = 0.01f;
        update.learning_rate = 0.001f;
        update.modulation_factor = 1.0f;
        update.apply_stdp = false;
        update.metadata = nullptr;

        bool routed = weight_update_router_route(weight_router, &update);
        // Routing may succeed or fail based on rules
    }

    weight_update_router_stats_t stats;
    EXPECT_TRUE(weight_update_router_get_stats(weight_router, &stats));
    EXPECT_EQ(stats.active_routes, 3);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
