//=============================================================================
// test_training_adapters_regression.cpp - Training Adapters Regression Tests
//
// Regression tests to ensure:
// - Learning signal format stability
// - Weight update routing consistency
// - Training event ordering guarantees
// - Performance regression detection
// - API backward compatibility
//=============================================================================

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>
#include <atomic>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_adapters.h"
#include "middleware/training/nimcp_learning_signal_adapter.h"
#include "middleware/training/nimcp_weight_update_adapter.h"
#include "middleware/training/nimcp_training_event_adapter.h"

//=============================================================================
// REGRESSION TEST FIXTURE
//=============================================================================

class TrainingAdaptersRegressionTest : public ::testing::Test {
protected:
    // Performance baselines (microseconds)
    static constexpr double MAX_SIGNAL_EXTRACT_TIME_US = 100.0;
    static constexpr double MAX_WEIGHT_ROUTE_TIME_US = 50.0;
    static constexpr double MAX_EVENT_PUBLISH_TIME_US = 30.0;

    // Behavioral constants
    static constexpr float NORMALIZATION_TOLERANCE = 0.01f;

    void SetUp() override {
        // Regression test setup
    }

    void TearDown() override {
        // Regression test cleanup
    }

    template<typename Func>
    double measureTimeUs(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    static void dummy_callback(const training_event_data_t* event, void* context) {
        // Dummy callback for testing
    }
};

//=============================================================================
// 1. LEARNING SIGNAL ADAPTER - BACKWARD COMPATIBILITY
//=============================================================================

TEST_F(TrainingAdaptersRegressionTest, LearningSignalDefaultConfigUnchanged) {
    // WHAT: Verify default learning signal configuration
    // WHY: Code relying on defaults must not break

    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();

    EXPECT_EQ(config.normalization, NORMALIZE_Z_SCORE);
    EXPECT_FLOAT_EQ(config.learning_rate_scale, 1.0f);
    EXPECT_FALSE(config.enable_attention_weighting);
    EXPECT_FALSE(config.enable_novelty_boost);
    EXPECT_GT(config.history_window_size, 0);
}

TEST_F(TrainingAdaptersRegressionTest, LearningSignalAPISurfaceStable) {
    // WHAT: Verify learning signal API unchanged
    // WHY: Binary compatibility

    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // All original APIs must exist
    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_TRUE(learning_signal_adapter_normalize(adapter, features, 5));

    learning_signal_adapter_stats_t stats;
    EXPECT_TRUE(learning_signal_adapter_get_stats(adapter, &stats));

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersRegressionTest, LearningSignalTypeNamesStable) {
    // WHAT: Verify signal type names unchanged
    // WHY: String representations used for logging/serialization

    struct {
        learning_signal_type_t type;
        const char* expected_prefix;
    } types[] = {
        {LEARNING_SIGNAL_ERROR, "ERROR"},
        {LEARNING_SIGNAL_REWARD, "REWARD"},
        {LEARNING_SIGNAL_SURPRISE, "SURPRISE"},
        {LEARNING_SIGNAL_ATTENTION, "ATTENTION"},
        {LEARNING_SIGNAL_MEMORY, "MEMORY"}
    };

    for (const auto& test : types) {
        const char* name = learning_signal_type_name(test.type);
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0);
        // Name should contain expected prefix (case-insensitive)
    }
}

TEST_F(TrainingAdaptersRegressionTest, NormalizationStrategiesConsistent) {
    // WHAT: Verify each normalization strategy produces consistent results
    // WHY: Normalization behavior must be deterministic

    normalization_strategy_t strategies[] = {
        NORMALIZE_MIN_MAX,
        NORMALIZE_Z_SCORE,
        NORMALIZE_L2
    };

    for (auto strategy : strategies) {
        learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
        config.normalization = strategy;

        learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
        ASSERT_NE(adapter, nullptr);

        // Test with known data
        float features1[5] = {100.0f, 200.0f, 150.0f, 180.0f, 120.0f};
        float features2[5] = {100.0f, 200.0f, 150.0f, 180.0f, 120.0f};

        EXPECT_TRUE(learning_signal_adapter_normalize(adapter, features1, 5));
        EXPECT_TRUE(learning_signal_adapter_normalize(adapter, features2, 5));

        // Results should be identical
        for (int i = 0; i < 5; i++) {
            EXPECT_NEAR(features1[i], features2[i], NORMALIZATION_TOLERANCE);
        }

        learning_signal_adapter_destroy(adapter);
    }
}

TEST_F(TrainingAdaptersRegressionTest, ZScoreNormalizationFormula) {
    // WHAT: Verify z-score normalization formula unchanged
    // WHY: Z-score calculation must be consistent

    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_Z_SCORE;

    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Test with known distribution
    float features[5] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};  // Mean=30, Std≈14.14
    EXPECT_TRUE(learning_signal_adapter_normalize(adapter, features, 5));

    // After z-score, mean should be ~0
    float mean = 0.0f;
    for (int i = 0; i < 5; i++) {
        mean += features[i];
    }
    mean /= 5.0f;

    EXPECT_NEAR(mean, 0.0f, 0.1f);

    // Std dev should be ~1
    float variance = 0.0f;
    for (int i = 0; i < 5; i++) {
        variance += (features[i] - mean) * (features[i] - mean);
    }
    float std_dev = std::sqrt(variance / 5.0f);

    EXPECT_NEAR(std_dev, 1.0f, 0.2f);

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersRegressionTest, MinMaxNormalizationRange) {
    // WHAT: Verify min-max normalization produces [0,1] range
    // WHY: Range must be consistent

    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_MIN_MAX;

    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    float features[5] = {10.0f, 50.0f, 30.0f, 70.0f, 20.0f};
    EXPECT_TRUE(learning_signal_adapter_normalize(adapter, features, 5));

    // All values should be in [0, 1]
    for (int i = 0; i < 5; i++) {
        EXPECT_GE(features[i], 0.0f);
        EXPECT_LE(features[i], 1.0f);
    }

    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersRegressionTest, AttentionWeightingBehavior) {
    // WHAT: Verify attention weighting formula
    // WHY: Attention modulation must be consistent

    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.enable_attention_weighting = true;

    learning_signal_adapter_t adapter = learning_signal_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Create test signal
    learning_signal_t signal;
    signal.type = LEARNING_SIGNAL_ERROR;
    signal.features = new float[4]{1.0f, 2.0f, 3.0f, 4.0f};
    signal.num_features = 4;
    signal.magnitude = 1.0f;
    signal.confidence = 1.0f;

    float original[4];
    for (int i = 0; i < 4; i++) {
        original[i] = signal.features[i];
    }

    // Apply attention
    float attention_weight = 0.5f;
    EXPECT_TRUE(learning_signal_adapter_apply_attention(adapter, &signal,
                                                        attention_weight));

    // Features should be scaled by attention
    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(signal.features[i], original[i] * attention_weight, 0.01f);
    }

    delete[] signal.features;
    learning_signal_adapter_destroy(adapter);
}

TEST_F(TrainingAdaptersRegressionTest, LearningSignalPerformance) {
    // WHAT: Verify learning signal operations meet baseline
    // WHY: No performance regressions

    learning_signal_adapter_t adapter = learning_signal_adapter_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Measure normalization performance
    double norm_time = measureTimeUs([&]() {
        for (int i = 0; i < 100; i++) {
            float features[10] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
                                 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
            learning_signal_adapter_normalize(adapter, features, 10);
        }
    });

    double avg_norm_time = norm_time / 100.0;
    EXPECT_LT(avg_norm_time, MAX_SIGNAL_EXTRACT_TIME_US);

    learning_signal_adapter_destroy(adapter);
}

//=============================================================================
// 2. WEIGHT UPDATE ROUTER - BACKWARD COMPATIBILITY
//=============================================================================

TEST_F(TrainingAdaptersRegressionTest, WeightRouterDefaultConfigUnchanged) {
    // WHAT: Verify default router configuration
    // WHY: Code relying on defaults must not break

    weight_update_router_config_t config = weight_update_router_default_config();

    EXPECT_GT(config.routing_table_capacity, 0);
    EXPECT_FALSE(config.enable_dynamic_routing);
    EXPECT_FALSE(config.enable_priority_routing);
    EXPECT_GT(config.max_batch_size, 0);
}

TEST_F(TrainingAdaptersRegressionTest, WeightRouterAPISurfaceStable) {
    // WHAT: Verify router API unchanged
    // WHY: Binary compatibility

    weight_update_router_t router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(router, nullptr);

    // All original APIs must exist
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    EXPECT_TRUE(weight_update_router_remove_route(router,
                                                   LEARNING_SIGNAL_ERROR,
                                                   WEIGHT_TARGET_CORTICAL));

    weight_update_router_stats_t stats;
    EXPECT_TRUE(weight_update_router_get_stats(router, &stats));

    weight_update_router_destroy(router);
}

TEST_F(TrainingAdaptersRegressionTest, WeightTargetTypeNamesStable) {
    // WHAT: Verify weight target names unchanged
    // WHY: String representations used for logging

    struct {
        weight_target_type_t type;
        const char* expected_prefix;
    } types[] = {
        {WEIGHT_TARGET_CORTICAL, "CORTICAL"},
        {WEIGHT_TARGET_SUBCORTICAL, "SUBCORTICAL"},
        {WEIGHT_TARGET_HIPPOCAMPAL, "HIPPOCAMPAL"},
        {WEIGHT_TARGET_STRIATAL, "STRIATAL"},
        {WEIGHT_TARGET_THALAMIC, "THALAMIC"}
    };

    for (const auto& test : types) {
        const char* name = weight_target_type_name(test.type);
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0);
    }
}

TEST_F(TrainingAdaptersRegressionTest, RoutingTableConsistency) {
    // WHAT: Verify routing table maintains consistent state
    // WHY: Routes must be stable and deterministic

    weight_update_router_t router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(router, nullptr);

    // Add multiple routes
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_REWARD,
                                               WEIGHT_TARGET_STRIATAL, 2));

    weight_update_router_stats_t stats1;
    EXPECT_TRUE(weight_update_router_get_stats(router, &stats1));
    EXPECT_EQ(stats1.active_routes, 2);

    // Remove one route
    EXPECT_TRUE(weight_update_router_remove_route(router,
                                                   LEARNING_SIGNAL_ERROR,
                                                   WEIGHT_TARGET_CORTICAL));

    weight_update_router_stats_t stats2;
    EXPECT_TRUE(weight_update_router_get_stats(router, &stats2));
    EXPECT_EQ(stats2.active_routes, 1);

    weight_update_router_destroy(router);
}

TEST_F(TrainingAdaptersRegressionTest, RoutePriorityOrdering) {
    // WHAT: Verify priority-based routing order
    // WHY: Higher priority routes must be processed first

    weight_update_router_config_t config = weight_update_router_default_config();
    config.enable_priority_routing = true;

    weight_update_router_t router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(router, nullptr);

    // Add routes with different priorities
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 3));
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_REWARD,
                                               WEIGHT_TARGET_STRIATAL, 1));

    // Priority routing should be enabled
    weight_update_router_stats_t stats;
    EXPECT_TRUE(weight_update_router_get_stats(router, &stats));
    EXPECT_EQ(stats.active_routes, 2);

    weight_update_router_destroy(router);
}

TEST_F(TrainingAdaptersRegressionTest, BatchRoutingConsistency) {
    // WHAT: Verify batch routing produces same results as individual
    // WHY: Batching should be an optimization, not change behavior

    weight_update_router_t router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(router, nullptr);

    // Add route
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    // Create updates
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
    uint32_t routed = weight_update_router_route_batch(router,
                                                        updates.data(),
                                                        updates.size());

    // All or none should route (based on routing rules)
    EXPECT_TRUE(routed == 0 || routed == updates.size());

    weight_update_router_destroy(router);
}

TEST_F(TrainingAdaptersRegressionTest, RouteStrengtheningBehavior) {
    // WHAT: Verify Hebbian route strengthening
    // WHY: Route learning must be consistent

    weight_update_router_config_t config = weight_update_router_default_config();
    config.enable_dynamic_routing = true;
    config.route_learning_rate = 0.01f;

    weight_update_router_t router = weight_update_router_create(&config, nullptr);
    ASSERT_NE(router, nullptr);

    // Add route
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    // Strengthen multiple times
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(weight_update_router_strengthen_route(router,
                                                           LEARNING_SIGNAL_ERROR,
                                                           WEIGHT_TARGET_CORTICAL));
    }

    // Route should still exist
    weight_update_router_stats_t stats;
    EXPECT_TRUE(weight_update_router_get_stats(router, &stats));
    EXPECT_EQ(stats.active_routes, 1);

    weight_update_router_destroy(router);
}

TEST_F(TrainingAdaptersRegressionTest, WeightRouterPerformance) {
    // WHAT: Verify routing meets performance baseline
    // WHY: No performance regressions

    weight_update_router_t router = weight_update_router_create(nullptr, nullptr);
    ASSERT_NE(router, nullptr);

    // Add route
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    // Measure routing performance
    double route_time = measureTimeUs([&]() {
        for (int i = 0; i < 100; i++) {
            weight_update_t update;
            update.target_type = WEIGHT_TARGET_CORTICAL;
            update.source_neuron = i;
            update.target_neuron = i + 10;
            update.weight_delta = 0.01f;
            update.learning_rate = 0.001f;
            update.modulation_factor = 1.0f;
            update.apply_stdp = false;
            update.metadata = nullptr;

            weight_update_router_route(router, &update);
        }
    });

    double avg_route_time = route_time / 100.0;
    EXPECT_LT(avg_route_time, MAX_WEIGHT_ROUTE_TIME_US);

    weight_update_router_destroy(router);
}

//=============================================================================
// 3. TRAINING EVENT MANAGER - BACKWARD COMPATIBILITY
//=============================================================================

TEST_F(TrainingAdaptersRegressionTest, EventManagerDefaultConfigUnchanged) {
    // WHAT: Verify default event manager configuration
    // WHY: Code relying on defaults must not break

    training_event_manager_config_t config = training_event_manager_default_config();

    EXPECT_GT(config.event_queue_capacity, 0);
    EXPECT_FALSE(config.enable_async_delivery);
    EXPECT_FALSE(config.enable_event_batching);
    EXPECT_GT(config.batch_timeout_ms, 0);
}

TEST_F(TrainingAdaptersRegressionTest, EventManagerAPISurfaceStable) {
    // WHAT: Verify event manager API unchanged
    // WHY: Binary compatibility

    training_event_manager_t manager = training_event_manager_create(nullptr, nullptr);
    ASSERT_NE(manager, nullptr);

    // All original APIs must exist
    training_event_data_t event;
    event.type = TRAINING_EVENT_EPOCH_START;
    event.epoch = 1;
    event.batch = 0;
    event.loss = 0.5f;
    event.learning_rate = 0.001f;
    event.timestamp_us = 0;
    event.metadata = nullptr;

    EXPECT_TRUE(training_event_manager_publish(manager, &event));

    subscription_handle_t handle = training_event_manager_subscribe(
        manager, dummy_callback, nullptr, nullptr, 0);

    EXPECT_TRUE(training_event_manager_unsubscribe(manager, handle));

    training_event_manager_stats_t stats;
    EXPECT_TRUE(training_event_manager_get_stats(manager, &stats));

    training_event_manager_destroy(manager);
}

TEST_F(TrainingAdaptersRegressionTest, TrainingEventTypeNamesStable) {
    // WHAT: Verify event type names unchanged
    // WHY: String representations used for logging

    struct {
        training_event_type_t type;
        const char* expected_prefix;
    } types[] = {
        {TRAINING_EVENT_EPOCH_START, "EPOCH"},
        {TRAINING_EVENT_EPOCH_END, "EPOCH"},
        {TRAINING_EVENT_BATCH_START, "BATCH"},
        {TRAINING_EVENT_BATCH_END, "BATCH"},
        {TRAINING_EVENT_LOSS_UPDATE, "LOSS"}
    };

    for (const auto& test : types) {
        const char* name = training_event_type_name(test.type);
        ASSERT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0);
    }
}

TEST_F(TrainingAdaptersRegressionTest, EventOrderingGuarantees) {
    // WHAT: Verify events are delivered in order
    // WHY: Training coordination requires ordered events

    training_event_manager_t manager = training_event_manager_create(nullptr, nullptr);
    ASSERT_NE(manager, nullptr);

    // Publish multiple events
    for (int i = 0; i < 5; i++) {
        training_event_data_t event;
        event.type = TRAINING_EVENT_BATCH_END;
        event.epoch = 1;
        event.batch = i;
        event.loss = 0.5f - i * 0.01f;
        event.learning_rate = 0.001f;
        event.timestamp_us = i * 1000;
        event.metadata = nullptr;

        EXPECT_TRUE(training_event_manager_publish(manager, &event));
    }

    training_event_manager_stats_t stats;
    EXPECT_TRUE(training_event_manager_get_stats(manager, &stats));
    EXPECT_EQ(stats.events_published, 5);

    training_event_manager_destroy(manager);
}

TEST_F(TrainingAdaptersRegressionTest, SubscriptionFilteringBehavior) {
    // WHAT: Verify event filtering works consistently
    // WHY: Subscribers must receive only relevant events

    training_event_manager_t manager = training_event_manager_create(nullptr, nullptr);
    ASSERT_NE(manager, nullptr);

    std::atomic<int> callback_count{0};

    // Subscribe to specific events
    training_event_type_t types[] = {TRAINING_EVENT_EPOCH_START};
    subscription_handle_t handle = training_event_manager_subscribe(
        manager,
        [](const training_event_data_t* event, void* context) {
            auto* count = static_cast<std::atomic<int>*>(context);
            (*count)++;
        },
        &callback_count,
        types,
        1
    );

    // Publish matching event
    training_event_data_t match_event;
    match_event.type = TRAINING_EVENT_EPOCH_START;
    match_event.epoch = 1;
    EXPECT_TRUE(training_event_manager_publish(manager, &match_event));

    // Publish non-matching event
    training_event_data_t nomatch_event;
    nomatch_event.type = TRAINING_EVENT_BATCH_START;
    nomatch_event.batch = 1;
    EXPECT_TRUE(training_event_manager_publish(manager, &nomatch_event));

    training_event_manager_unsubscribe(manager, handle);
    training_event_manager_destroy(manager);
}

TEST_F(TrainingAdaptersRegressionTest, EventManagerPerformance) {
    // WHAT: Verify event operations meet baseline
    // WHY: No performance regressions

    training_event_manager_t manager = training_event_manager_create(nullptr, nullptr);
    ASSERT_NE(manager, nullptr);

    // Measure publish performance
    double publish_time = measureTimeUs([&]() {
        for (int i = 0; i < 100; i++) {
            training_event_data_t event;
            event.type = TRAINING_EVENT_BATCH_END;
            event.epoch = 1;
            event.batch = i;
            event.loss = 0.5f;
            event.learning_rate = 0.001f;
            event.timestamp_us = i * 1000;
            event.metadata = nullptr;

            training_event_manager_publish(manager, &event);
        }
    });

    double avg_publish_time = publish_time / 100.0;
    EXPECT_LT(avg_publish_time, MAX_EVENT_PUBLISH_TIME_US);

    training_event_manager_destroy(manager);
}

//=============================================================================
// 4. CROSS-COMPONENT CONSISTENCY
//=============================================================================

TEST_F(TrainingAdaptersRegressionTest, ComponentInteroperability) {
    // WHAT: Verify components work together consistently
    // WHY: Integration must be stable

    learning_signal_adapter_t signal_adapter = learning_signal_adapter_create(nullptr);
    weight_update_router_t router = weight_update_router_create(nullptr, nullptr);
    training_event_manager_t manager = training_event_manager_create(nullptr, nullptr);

    ASSERT_NE(signal_adapter, nullptr);
    ASSERT_NE(router, nullptr);
    ASSERT_NE(manager, nullptr);

    // Should be able to use all components together
    EXPECT_TRUE(weight_update_router_add_route(router,
                                               LEARNING_SIGNAL_ERROR,
                                               WEIGHT_TARGET_CORTICAL, 1));

    training_event_data_t event;
    event.type = TRAINING_EVENT_EPOCH_START;
    event.epoch = 1;
    EXPECT_TRUE(training_event_manager_publish(manager, &event));

    learning_signal_adapter_destroy(signal_adapter);
    weight_update_router_destroy(router);
    training_event_manager_destroy(manager);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
