/**
 * @file test_layer_pools_integration.cpp
 * @brief Integration tests for Layer Pools across NIMCP layers
 *
 * WHAT: Test layer pools integration across cognitive, middleware, training
 * WHY:  Verify cross-layer allocation works in real workflows
 * HOW:  Simulate real usage patterns across layers
 *
 * PHASE: 3 (Cross-Layer Integration)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_layer_pools.h"

class LayerPoolsIntegrationTest : public ::testing::Test {
protected:
    layer_pools_t pools = nullptr;

    void SetUp() override {
        layer_pools_config_t config = layer_pools_default_config();
        pools = layer_pools_create(&config, nullptr);
        ASSERT_NE(pools, nullptr);
    }

    void TearDown() override {
        if (pools) layer_pools_destroy(pools);
    }
};

// Integration: Cognitive workflow (workspace broadcast)
TEST_F(LayerPoolsIntegrationTest, Cognitive_WorkspaceBroadcast)
{
    // Simulate global workspace broadcast pattern
    const int NUM_SUBSCRIBERS = 10;

    // Acquire workspace entry for broadcast
    void* workspace_entry = layer_pools_acquire_workspace_entry(pools);
    ASSERT_NE(workspace_entry, nullptr);

    // Simulate subscriber deliveries
    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        void* subscriber = layer_pools_acquire_subscriber_entry(pools);
        ASSERT_NE(subscriber, nullptr);

        // Process and release
        layer_pools_release_subscriber_entry(pools, subscriber);
    }

    layer_pools_release_workspace_entry(pools, workspace_entry);

    // Verify metrics
    layer_stats_t cognitive_stats;
    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_COGNITIVE, &cognitive_stats));
    EXPECT_EQ(cognitive_stats.total_acquires, 1);
}

// Integration: Middleware event processing pipeline
TEST_F(LayerPoolsIntegrationTest, Middleware_EventPipeline)
{
    // Simulate event → pattern → route pipeline
    const int NUM_EVENTS = 50;

    for (int i = 0; i < NUM_EVENTS; i++) {
        // Step 1: Acquire event entry
        void* event = layer_pools_acquire_event_entry(pools);
        ASSERT_NE(event, nullptr);

        // Step 2: Pattern matching
        void* pattern = layer_pools_acquire_pattern_node(pools);
        ASSERT_NE(pattern, nullptr);

        // Step 3: Route decision
        void* route = layer_pools_acquire_route_node(pools);
        ASSERT_NE(route, nullptr);

        // Step 4: Feature extraction
        float* features = layer_pools_acquire_feature_buffer(pools, 64);
        ASSERT_NE(features, nullptr);

        // Release in reverse order
        layer_pools_release_feature_buffer(pools, features);
        layer_pools_release_route_node(pools, route);
        layer_pools_release_pattern_node(pools, pattern);
        layer_pools_release_event_entry(pools, event);
    }

    // Verify all released
    layer_stats_t middleware_stats;
    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_MIDDLEWARE, &middleware_stats));
    EXPECT_EQ(middleware_stats.current_in_use, 0);
}

// Integration: Training batch learning workflow
TEST_F(LayerPoolsIntegrationTest, Training_BatchLearning)
{
    const int BATCH_SIZE = 32;
    const int NUM_OUTPUTS = 10;

    // Acquire batch buffer
    void* batch = layer_pools_acquire_batch_buffer(pools, BATCH_SIZE, 100);
    ASSERT_NE(batch, nullptr);

    // For each example, acquire target/prediction
    for (int i = 0; i < BATCH_SIZE; i++) {
        float* target = nullptr;
        float* prediction = nullptr;

        ASSERT_TRUE(layer_pools_acquire_target_prediction(pools, NUM_OUTPUTS, &target, &prediction));

        // Simulate forward pass
        for (int j = 0; j < NUM_OUTPUTS; j++) {
            prediction[j] = 0.1f * j;
            target[j] = (j == i % NUM_OUTPUTS) ? 1.0f : 0.0f;
        }

        // Acquire gradient for backprop
        float* gradient = layer_pools_acquire_gradient_buffer(pools, 100);
        ASSERT_NE(gradient, nullptr);

        // Simulate backprop
        for (int j = 0; j < 100; j++) {
            gradient[j] = 0.01f;
        }

        // Release
        layer_pools_release_gradient_buffer(pools, gradient);
        layer_pools_release_target_prediction(pools, target, prediction);
    }

    layer_pools_release_batch_buffer(pools, batch, BATCH_SIZE * 100);
}

// Integration: Cross-layer workflow (training + middleware)
TEST_F(LayerPoolsIntegrationTest, CrossLayer_TrainingMiddleware)
{
    // Simulate learning signal from middleware event
    void* event = layer_pools_acquire_event_entry(pools);
    ASSERT_NE(event, nullptr);

    // Extract learning signal
    void* signal = layer_pools_acquire_learning_signal(pools);
    ASSERT_NE(signal, nullptr);

    // Feature extraction
    float* features = layer_pools_acquire_feature_buffer(pools, 32);
    ASSERT_NE(features, nullptr);

    // Process and release
    layer_pools_release_feature_buffer(pools, features);
    layer_pools_release_learning_signal(pools, signal);
    layer_pools_release_event_entry(pools, event);
}

// Integration: Brain pools interop
TEST_F(LayerPoolsIntegrationTest, BrainPools_Interop)
{
    brain_pools_t bp = layer_pools_get_brain_pools(pools);
    ASSERT_NE(bp, nullptr);

    // Use brain pools directly
    void* decision = brain_pools_acquire_decision(bp);
    ASSERT_NE(decision, nullptr);

    void* spike = brain_pools_acquire_spike_event(bp);
    ASSERT_NE(spike, nullptr);

    // Use layer pools feature buffer (delegates to brain pools)
    float* features = layer_pools_acquire_feature_buffer(pools, 128);
    ASSERT_NE(features, nullptr);

    // Release
    layer_pools_release_feature_buffer(pools, features);
    brain_pools_release_spike_event(bp, spike);
    brain_pools_release_decision(bp, decision);
}

// Integration: Concurrent multi-layer operations
TEST_F(LayerPoolsIntegrationTest, Concurrent_MultiLayer)
{
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 25;
    std::atomic<int> success_count{0};

    auto cognitive_worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            void* entry = layer_pools_acquire_workspace_entry(pools);
            if (entry) {
                layer_pools_release_workspace_entry(pools, entry);
                success_count++;
            }
        }
    };

    auto middleware_worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            void* entry = layer_pools_acquire_event_entry(pools);
            if (entry) {
                layer_pools_release_event_entry(pools, entry);
                success_count++;
            }
        }
    };

    auto training_worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            void* signal = layer_pools_acquire_learning_signal(pools);
            if (signal) {
                layer_pools_release_learning_signal(pools, signal);
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(cognitive_worker);
    threads.emplace_back(middleware_worker);
    threads.emplace_back(training_worker);
    threads.emplace_back(middleware_worker);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);
}

// Integration: Fairness under load
TEST_F(LayerPoolsIntegrationTest, Fairness_UnderLoad)
{
    // Create uneven load
    for (int i = 0; i < 100; i++) {
        void* event = layer_pools_acquire_event_entry(pools);
        layer_pools_release_event_entry(pools, event);
    }

    for (int i = 0; i < 10; i++) {
        void* workspace = layer_pools_acquire_workspace_entry(pools);
        layer_pools_release_workspace_entry(pools, workspace);
    }

    // Check fairness
    fairness_metrics_t fairness;
    ASSERT_TRUE(layer_pools_get_fairness_metrics(pools, &fairness));

    // Should still maintain reasonable fairness
    EXPECT_GE(fairness.jains_fairness_index, 0.25f);
}

// Integration: Memory stability
TEST_F(LayerPoolsIntegrationTest, Memory_Stability)
{
    layer_pools_metrics_t metrics_before;
    ASSERT_TRUE(layer_pools_get_metrics(pools, &metrics_before));

    // Heavy workload
    for (int i = 0; i < 1000; i++) {
        void* event = layer_pools_acquire_event_entry(pools);
        void* signal = layer_pools_acquire_learning_signal(pools);
        void* workspace = layer_pools_acquire_workspace_entry(pools);

        layer_pools_release_workspace_entry(pools, workspace);
        layer_pools_release_learning_signal(pools, signal);
        layer_pools_release_event_entry(pools, event);
    }

    layer_pools_metrics_t metrics_after;
    ASSERT_TRUE(layer_pools_get_metrics(pools, &metrics_after));

    // All should be released
    layer_stats_t middleware_stats;
    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_MIDDLEWARE, &middleware_stats));
    EXPECT_EQ(middleware_stats.current_in_use, 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
