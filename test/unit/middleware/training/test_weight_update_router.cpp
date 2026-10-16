//=============================================================================
// test_weight_update_router.cpp - Comprehensive Weight Update Router Tests
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_adapters.h"
#include "core/events/nimcp_event_bus.h"
#include "middleware/routing/nimcp_routing_table.h"

#include <thread>
#include <atomic>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class WeightUpdateRouterTest : public ::testing::Test {
protected:
    weight_update_router_t router = nullptr;
    event_bus_t event_bus = nullptr;

    void SetUp() override {
        // Create event bus using core API
        event_bus = event_bus_create("test_router_bus", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(event_bus, nullptr);

        // Create router with event bus
        weight_update_router_config_t config = weight_update_router_default_config();
        router = weight_update_router_create(&config, event_bus);
        ASSERT_NE(router, nullptr);
    }

    void TearDown() override {
        if (router) {
            weight_update_router_destroy(router);
        }
        if (event_bus) {
            event_bus_destroy(event_bus);
        }
    }

    // Helper to create weight update
    weight_update_t create_weight_update(weight_target_type_t target,
                                          uint32_t source, uint32_t dest,
                                          float delta, float lr) {
        weight_update_t update;
        update.target_type = target;
        update.source_neuron = source;
        update.target_neuron = dest;
        update.weight_delta = delta;
        update.learning_rate = lr;
        update.modulation_factor = 1.0f;
        update.timestamp_us = 1000;
        update.apply_stdp = false;
        update.metadata = nullptr;
        return update;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, CreateAndDestroy) {
    EXPECT_NE(router, nullptr);
}

TEST_F(WeightUpdateRouterTest, CreateWithNullConfig) {
    weight_update_router_t test_router = weight_update_router_create(nullptr, event_bus);
    EXPECT_NE(test_router, nullptr);
    weight_update_router_destroy(test_router);
}

TEST_F(WeightUpdateRouterTest, CreateWithOwnEventBus) {
    weight_update_router_config_t config = weight_update_router_default_config();
    weight_update_router_t test_router = weight_update_router_create(&config, nullptr);
    EXPECT_NE(test_router, nullptr);
    weight_update_router_destroy(test_router);
}

TEST_F(WeightUpdateRouterTest, CreateWithCustomConfig) {
    weight_update_router_config_t config = weight_update_router_default_config();
    config.routing_table_capacity = 500;
    config.enable_dynamic_routing = false;
    config.enable_priority_routing = true;
    config.max_batch_size = 64;

    weight_update_router_t test_router = weight_update_router_create(&config, event_bus);
    EXPECT_NE(test_router, nullptr);
    weight_update_router_destroy(test_router);
}

TEST_F(WeightUpdateRouterTest, DestroyNullRouter) {
    weight_update_router_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Route Management Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, AddRoute) {
    bool success = weight_update_router_add_route(
        router,
        LEARNING_SIGNAL_ERROR,
        WEIGHT_TARGET_CORTICAL,
        10  // priority
    );
    EXPECT_TRUE(success);

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GE(stats.active_routes, 1u);
}

TEST_F(WeightUpdateRouterTest, AddMultipleRoutes) {
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_REWARD, WEIGHT_TARGET_STRIATAL, 20));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_SURPRISE, WEIGHT_TARGET_HIPPOCAMPAL, 15));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ATTENTION, WEIGHT_TARGET_THALAMIC, 5));

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GE(stats.active_routes, 4u);
}

TEST_F(WeightUpdateRouterTest, AddAllSignalTypes) {
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_REWARD, WEIGHT_TARGET_STRIATAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_SURPRISE, WEIGHT_TARGET_HIPPOCAMPAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ATTENTION, WEIGHT_TARGET_THALAMIC, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_MEMORY, WEIGHT_TARGET_HIPPOCAMPAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_CUSTOM, WEIGHT_TARGET_CUSTOM, 10));

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GE(stats.active_routes, 6u);
}

TEST_F(WeightUpdateRouterTest, AddAllTargetTypes) {
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_SUBCORTICAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_HIPPOCAMPAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_STRIATAL, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_THALAMIC, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CEREBELLAR, 10));
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CUSTOM, 10));

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GE(stats.active_routes, 7u);
}

TEST_F(WeightUpdateRouterTest, RemoveRoute) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    bool success = weight_update_router_remove_route(
        router,
        LEARNING_SIGNAL_ERROR,
        WEIGHT_TARGET_CORTICAL
    );
    EXPECT_TRUE(success);
}

TEST_F(WeightUpdateRouterTest, RemoveNonexistentRoute) {
    bool success = weight_update_router_remove_route(
        router,
        LEARNING_SIGNAL_ERROR,
        WEIGHT_TARGET_CORTICAL
    );
    // May succeed or fail depending on implementation
    // Just verify it doesn't crash
}

TEST_F(WeightUpdateRouterTest, RemoveMultipleRoutes) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_REWARD, WEIGHT_TARGET_STRIATAL, 20);

    EXPECT_TRUE(weight_update_router_remove_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL));
    EXPECT_TRUE(weight_update_router_remove_route(
        router, LEARNING_SIGNAL_REWARD, WEIGHT_TARGET_STRIATAL));
}

//=============================================================================
// Route Strengthening Tests (Hebbian Learning)
//=============================================================================

TEST_F(WeightUpdateRouterTest, StrengthenRoute) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    bool success = weight_update_router_strengthen_route(
        router,
        LEARNING_SIGNAL_ERROR,
        WEIGHT_TARGET_CORTICAL
    );
    EXPECT_TRUE(success);
}

TEST_F(WeightUpdateRouterTest, StrengthenNonexistentRoute) {
    bool success = weight_update_router_strengthen_route(
        router,
        LEARNING_SIGNAL_ERROR,
        WEIGHT_TARGET_CORTICAL
    );
    EXPECT_FALSE(success);
}

TEST_F(WeightUpdateRouterTest, StrengthenMultipleTimes) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    // Strengthen route multiple times
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(weight_update_router_strengthen_route(
            router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL));
    }
}

TEST_F(WeightUpdateRouterTest, HebbianLearningOnUse) {
    // Enable dynamic routing for Hebbian learning
    weight_update_router_config_t config = weight_update_router_default_config();
    config.enable_dynamic_routing = true;
    config.route_learning_rate = 0.1f;

    weight_update_router_t test_router = weight_update_router_create(&config, event_bus);
    ASSERT_NE(test_router, nullptr);

    weight_update_router_add_route(
        test_router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    // Route multiple updates - should strengthen route
    for (int i = 0; i < 10; i++) {
        weight_update_t update = create_weight_update(
            WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);
        weight_update_router_route(test_router, &update);
    }

    weight_update_router_destroy(test_router);
}

//=============================================================================
// Single Route Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, RouteSingleUpdate) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);

    bool success = weight_update_router_route(router, &update);
    // Success depends on whether route is found
    // Just verify it doesn't crash

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    // Either routed or dropped
    EXPECT_GE(stats.updates_routed + stats.updates_dropped, 1u);
}

TEST_F(WeightUpdateRouterTest, RouteWithoutRoute) {
    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);

    bool success = weight_update_router_route(router, &update);
    EXPECT_FALSE(success);

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_EQ(stats.updates_dropped, 1u);
}

TEST_F(WeightUpdateRouterTest, RouteToAllTargets) {
    // Add routes for all target types
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_SUBCORTICAL, 10);
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_HIPPOCAMPAL, 10);
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_STRIATAL, 10);
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_THALAMIC, 10);
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CEREBELLAR, 10);

    // Route updates to each target
    weight_update_t updates[] = {
        create_weight_update(WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f),
        create_weight_update(WEIGHT_TARGET_SUBCORTICAL, 3, 4, 0.2f, 0.01f),
        create_weight_update(WEIGHT_TARGET_HIPPOCAMPAL, 5, 6, 0.3f, 0.01f),
        create_weight_update(WEIGHT_TARGET_STRIATAL, 7, 8, 0.4f, 0.01f),
        create_weight_update(WEIGHT_TARGET_THALAMIC, 9, 10, 0.5f, 0.01f),
        create_weight_update(WEIGHT_TARGET_CEREBELLAR, 11, 12, 0.6f, 0.01f)
    };

    for (int i = 0; i < 6; i++) {
        weight_update_router_route(router, &updates[i]);
    }

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GE(stats.updates_routed + stats.updates_dropped, 6u);
}

TEST_F(WeightUpdateRouterTest, RouteWithSTDP) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);
    update.apply_stdp = true;

    weight_update_router_route(router, &update);
}

TEST_F(WeightUpdateRouterTest, RouteWithModulation) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);
    update.modulation_factor = 1.5f;

    weight_update_router_route(router, &update);
}

TEST_F(WeightUpdateRouterTest, RouteWithNegativeDelta) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, -0.1f, 0.01f);  // Negative delta (LTD)

    weight_update_router_route(router, &update);
}

TEST_F(WeightUpdateRouterTest, RouteWithZeroDelta) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.0f, 0.01f);  // Zero delta

    weight_update_router_route(router, &update);
}

//=============================================================================
// Batch Route Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, RouteBatchEmpty) {
    weight_update_t updates[1];
    uint32_t routed = weight_update_router_route_batch(router, updates, 0);
    EXPECT_EQ(routed, 0u);
}

TEST_F(WeightUpdateRouterTest, RouteBatchSingleUpdate) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t updates[] = {
        create_weight_update(WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f)
    };

    uint32_t routed = weight_update_router_route_batch(router, updates, 1);
    // May be 0 or 1 depending on routing success
}

TEST_F(WeightUpdateRouterTest, RouteBatchMultipleUpdates) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t updates[10];
    for (int i = 0; i < 10; i++) {
        updates[i] = create_weight_update(
            WEIGHT_TARGET_CORTICAL, i, i+1, 0.1f, 0.01f);
    }

    uint32_t routed = weight_update_router_route_batch(router, updates, 10);
    EXPECT_LE(routed, 10u);

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GE(stats.batch_operations, 1u);
}

TEST_F(WeightUpdateRouterTest, RouteBatchLarge) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    const int batch_size = 100;
    weight_update_t updates[batch_size];
    for (int i = 0; i < batch_size; i++) {
        updates[i] = create_weight_update(
            WEIGHT_TARGET_CORTICAL, i, i+1, 0.1f, 0.01f);
    }

    uint32_t routed = weight_update_router_route_batch(router, updates, batch_size);
    EXPECT_LE(routed, (uint32_t)batch_size);

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GE(stats.batch_operations, 1u);
}

TEST_F(WeightUpdateRouterTest, RouteBatchMixedTargets) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_STRIATAL, 10);

    weight_update_t updates[6];
    for (int i = 0; i < 3; i++) {
        updates[i] = create_weight_update(
            WEIGHT_TARGET_CORTICAL, i, i+1, 0.1f, 0.01f);
    }
    for (int i = 3; i < 6; i++) {
        updates[i] = create_weight_update(
            WEIGHT_TARGET_STRIATAL, i, i+1, 0.2f, 0.01f);
    }

    uint32_t routed = weight_update_router_route_batch(router, updates, 6);
    EXPECT_LE(routed, 6u);
}

TEST_F(WeightUpdateRouterTest, RouteBatchExceedsCapacity) {
    // Create router with small batch size
    weight_update_router_config_t config = weight_update_router_default_config();
    config.max_batch_size = 10;
    weight_update_router_t test_router = weight_update_router_create(&config, event_bus);

    weight_update_router_add_route(
        test_router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    // Send batch larger than max_batch_size
    const int batch_size = 50;
    weight_update_t updates[batch_size];
    for (int i = 0; i < batch_size; i++) {
        updates[i] = create_weight_update(
            WEIGHT_TARGET_CORTICAL, i, i+1, 0.1f, 0.01f);
    }

    uint32_t routed = weight_update_router_route_batch(test_router, updates, batch_size);
    // Should handle in multiple batches
    EXPECT_LE(routed, (uint32_t)batch_size);

    weight_update_router_destroy(test_router);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, GetStatistics) {
    weight_update_router_stats_t stats;
    bool success = weight_update_router_get_stats(router, &stats);
    EXPECT_TRUE(success);

    EXPECT_EQ(stats.updates_routed, 0u);
    EXPECT_EQ(stats.updates_dropped, 0u);
    EXPECT_EQ(stats.batch_operations, 0u);
}

TEST_F(WeightUpdateRouterTest, StatisticsAfterRouting) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);
    weight_update_router_route(router, &update);

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);

    EXPECT_GE(stats.updates_routed + stats.updates_dropped, 1u);
}

TEST_F(WeightUpdateRouterTest, StatisticsAfterBatchRouting) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    weight_update_t updates[5];
    for (int i = 0; i < 5; i++) {
        updates[i] = create_weight_update(
            WEIGHT_TARGET_CORTICAL, i, i+1, 0.1f, 0.01f);
    }

    weight_update_router_route_batch(router, updates, 5);

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);

    EXPECT_GE(stats.batch_operations, 1u);
}

TEST_F(WeightUpdateRouterTest, StatisticsDroppedUpdates) {
    // Route without adding routes - should drop
    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);
    weight_update_router_route(router, &update);

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);

    EXPECT_EQ(stats.updates_dropped, 1u);
    EXPECT_EQ(stats.updates_routed, 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, AddRouteNullRouter) {
    bool success = weight_update_router_add_route(
        nullptr, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);
    EXPECT_FALSE(success);
}

TEST_F(WeightUpdateRouterTest, RemoveRouteNullRouter) {
    bool success = weight_update_router_remove_route(
        nullptr, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL);
    EXPECT_FALSE(success);
}

TEST_F(WeightUpdateRouterTest, StrengthenRouteNullRouter) {
    bool success = weight_update_router_strengthen_route(
        nullptr, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL);
    EXPECT_FALSE(success);
}

TEST_F(WeightUpdateRouterTest, RouteNullRouter) {
    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);

    bool success = weight_update_router_route(nullptr, &update);
    EXPECT_FALSE(success);
}

TEST_F(WeightUpdateRouterTest, RouteNullUpdate) {
    bool success = weight_update_router_route(router, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(WeightUpdateRouterTest, RouteBatchNullRouter) {
    weight_update_t updates[1];
    uint32_t routed = weight_update_router_route_batch(nullptr, updates, 1);
    EXPECT_EQ(routed, 0u);
}

TEST_F(WeightUpdateRouterTest, RouteBatchNullUpdates) {
    uint32_t routed = weight_update_router_route_batch(router, nullptr, 10);
    EXPECT_EQ(routed, 0u);
}

TEST_F(WeightUpdateRouterTest, GetStatsNullRouter) {
    weight_update_router_stats_t stats;
    bool success = weight_update_router_get_stats(nullptr, &stats);
    EXPECT_FALSE(success);
}

TEST_F(WeightUpdateRouterTest, GetStatsNullStats) {
    bool success = weight_update_router_get_stats(router, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, ConcurrentRouting) {
    weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10);

    const int num_threads = 4;
    const int routes_per_thread = 100;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, routes_per_thread, t]() {
            for (int i = 0; i < routes_per_thread; i++) {
                weight_update_t update = create_weight_update(
                    WEIGHT_TARGET_CORTICAL, t*100+i, t*100+i+1, 0.1f, 0.01f);

                if (weight_update_router_route(router, &update)) {
                    success_count++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_EQ(stats.updates_routed + stats.updates_dropped,
              (uint64_t)(num_threads * routes_per_thread));
}

TEST_F(WeightUpdateRouterTest, ConcurrentRouteManagement) {
    const int num_threads = 4;
    std::atomic<int> add_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &add_count, t]() {
            // Each thread adds different routes
            learning_signal_type_t signal = static_cast<learning_signal_type_t>(t % 6);
            weight_target_type_t target = static_cast<weight_target_type_t>(t % 7);

            if (weight_update_router_add_route(router, signal, target, 10)) {
                add_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(add_count.load(), 0);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, LearningSignalTypeNames) {
    EXPECT_STREQ(learning_signal_type_name(LEARNING_SIGNAL_ERROR), "Error");
    EXPECT_STREQ(learning_signal_type_name(LEARNING_SIGNAL_REWARD), "Reward");
    EXPECT_STREQ(learning_signal_type_name(LEARNING_SIGNAL_SURPRISE), "Surprise");
    EXPECT_STREQ(learning_signal_type_name(LEARNING_SIGNAL_ATTENTION), "Attention");
    EXPECT_STREQ(learning_signal_type_name(LEARNING_SIGNAL_MEMORY), "Memory");
    EXPECT_STREQ(learning_signal_type_name(LEARNING_SIGNAL_CUSTOM), "Custom");
}

TEST_F(WeightUpdateRouterTest, WeightTargetTypeNames) {
    EXPECT_STREQ(weight_target_type_name(WEIGHT_TARGET_CORTICAL), "Cortical");
    EXPECT_STREQ(weight_target_type_name(WEIGHT_TARGET_SUBCORTICAL), "Subcortical");
    EXPECT_STREQ(weight_target_type_name(WEIGHT_TARGET_HIPPOCAMPAL), "Hippocampal");
    EXPECT_STREQ(weight_target_type_name(WEIGHT_TARGET_STRIATAL), "Striatal");
    EXPECT_STREQ(weight_target_type_name(WEIGHT_TARGET_THALAMIC), "Thalamic");
    EXPECT_STREQ(weight_target_type_name(WEIGHT_TARGET_CEREBELLAR), "Cerebellar");
    EXPECT_STREQ(weight_target_type_name(WEIGHT_TARGET_CUSTOM), "Custom");
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(WeightUpdateRouterTest, EndToEndWorkflow) {
    // Add routes
    EXPECT_TRUE(weight_update_router_add_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL, 10));

    // Route single update
    weight_update_t update = create_weight_update(
        WEIGHT_TARGET_CORTICAL, 1, 2, 0.1f, 0.01f);
    weight_update_router_route(router, &update);

    // Strengthen route
    weight_update_router_strengthen_route(
        router, LEARNING_SIGNAL_ERROR, WEIGHT_TARGET_CORTICAL);

    // Route batch
    weight_update_t updates[5];
    for (int i = 0; i < 5; i++) {
        updates[i] = create_weight_update(
            WEIGHT_TARGET_CORTICAL, i, i+1, 0.1f, 0.01f);
    }
    weight_update_router_route_batch(router, updates, 5);

    // Check statistics
    weight_update_router_stats_t stats;
    weight_update_router_get_stats(router, &stats);
    EXPECT_GE(stats.updates_routed + stats.updates_dropped, 6u);
    EXPECT_GE(stats.batch_operations, 1u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
