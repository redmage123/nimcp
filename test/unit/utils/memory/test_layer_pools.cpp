/**
 * @file test_layer_pools.cpp
 * @brief Unit tests for Layer Pools Memory System (Phase 3)
 *
 * WHAT: Test layer pools functionality across all layers
 * WHY:  Verify O(1) allocation and cross-layer operations
 * HOW:  Test each layer's pools, metrics, and edge cases
 *
 * PHASE: 3 (Cross-Layer Integration)
 *
 * TEST COVERAGE:
 * - Creation/destruction (4 tests)
 * - Cognitive layer pools (6 tests)
 * - Middleware layer pools (8 tests)
 * - Training layer pools (6 tests)
 * - Cross-layer operations (4 tests)
 * - Metrics and fairness (6 tests)
 * - Thread safety (4 tests)
 * - Edge cases and null safety (6 tests)
 * Total: 44 tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_layer_pools.h"

//=============================================================================
// Test Fixture
//=============================================================================

class LayerPoolsTest : public ::testing::Test {
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

//=============================================================================
// Creation/Destruction Tests (4 tests)
//=============================================================================

TEST_F(LayerPoolsTest, Create_WithDefaults)
{
    // SetUp already created with defaults
    EXPECT_NE(pools, nullptr);

    // Verify brain pools are accessible
    brain_pools_t bp = layer_pools_get_brain_pools(pools);
    EXPECT_NE(bp, nullptr);
}

TEST_F(LayerPoolsTest, Create_WithNullConfig)
{
    layer_pools_t p = layer_pools_create(nullptr, nullptr);
    EXPECT_NE(p, nullptr);
    layer_pools_destroy(p);
}

TEST_F(LayerPoolsTest, Create_WithExistingBrainPools)
{
    brain_pools_config_t bp_config = brain_pools_default_config();
    brain_pools_t bp = brain_pools_create(&bp_config);
    ASSERT_NE(bp, nullptr);

    layer_pools_config_t config = layer_pools_default_config();
    layer_pools_t p = layer_pools_create(&config, bp);
    EXPECT_NE(p, nullptr);

    // Verify same brain pools
    EXPECT_EQ(layer_pools_get_brain_pools(p), bp);

    layer_pools_destroy(p);
    brain_pools_destroy(bp);  // We still own it
}

TEST_F(LayerPoolsTest, Create_ConfigVariants)
{
    // Training config
    layer_pools_config_t train_config = layer_pools_training_config();
    layer_pools_t train_pools = layer_pools_create(&train_config, nullptr);
    EXPECT_NE(train_pools, nullptr);
    layer_pools_destroy(train_pools);

    // Inference config
    layer_pools_config_t infer_config = layer_pools_inference_config();
    layer_pools_t infer_pools = layer_pools_create(&infer_config, nullptr);
    EXPECT_NE(infer_pools, nullptr);
    layer_pools_destroy(infer_pools);
}

//=============================================================================
// Cognitive Layer Pool Tests (6 tests)
//=============================================================================

TEST_F(LayerPoolsTest, Cognitive_WorkspaceEntry_AcquireRelease)
{
    void* entry = layer_pools_acquire_workspace_entry(pools);
    ASSERT_NE(entry, nullptr);

    // Write to entry
    memset(entry, 0x42, 64);

    layer_pools_release_workspace_entry(pools, entry);
}

TEST_F(LayerPoolsTest, Cognitive_KnowledgeEntry_AcquireRelease)
{
    void* entry = layer_pools_acquire_knowledge_entry(pools);
    ASSERT_NE(entry, nullptr);

    memset(entry, 0x43, 64);

    layer_pools_release_knowledge_entry(pools, entry);
}

TEST_F(LayerPoolsTest, Cognitive_WorkingMemoryItem_AcquireRelease)
{
    size_t item_size = 256;
    void* item = layer_pools_acquire_working_memory_item(pools, item_size);
    ASSERT_NE(item, nullptr);

    memset(item, 0x44, item_size);

    layer_pools_release_working_memory_item(pools, item, item_size);
}

TEST_F(LayerPoolsTest, Cognitive_MultipleWorkspaceEntries)
{
    const int COUNT = 100;
    void* entries[COUNT];

    for (int i = 0; i < COUNT; i++) {
        entries[i] = layer_pools_acquire_workspace_entry(pools);
        ASSERT_NE(entries[i], nullptr) << "Failed at " << i;
    }

    for (int i = 0; i < COUNT; i++) {
        layer_pools_release_workspace_entry(pools, entries[i]);
    }
}

TEST_F(LayerPoolsTest, Cognitive_StatsTracking)
{
    void* entry1 = layer_pools_acquire_workspace_entry(pools);
    void* entry2 = layer_pools_acquire_workspace_entry(pools);

    layer_stats_t stats;
    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_COGNITIVE, &stats));
    EXPECT_GE(stats.total_acquires, 2);
    EXPECT_EQ(stats.current_in_use, 2);

    layer_pools_release_workspace_entry(pools, entry1);
    layer_pools_release_workspace_entry(pools, entry2);

    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_COGNITIVE, &stats));
    EXPECT_EQ(stats.current_in_use, 0);
}

TEST_F(LayerPoolsTest, Cognitive_WorkingMemoryVariableSizes)
{
    size_t sizes[] = {64, 256, 1024, 4096};

    for (size_t size : sizes) {
        void* item = layer_pools_acquire_working_memory_item(pools, size);
        ASSERT_NE(item, nullptr) << "Failed for size " << size;
        layer_pools_release_working_memory_item(pools, item, size);
    }
}

//=============================================================================
// Middleware Layer Pool Tests (8 tests)
//=============================================================================

TEST_F(LayerPoolsTest, Middleware_EventEntry_AcquireRelease)
{
    void* entry = layer_pools_acquire_event_entry(pools);
    ASSERT_NE(entry, nullptr);

    memset(entry, 0x45, 64);

    layer_pools_release_event_entry(pools, entry);
}

TEST_F(LayerPoolsTest, Middleware_PatternNode_AcquireRelease)
{
    void* node = layer_pools_acquire_pattern_node(pools);
    ASSERT_NE(node, nullptr);

    memset(node, 0x46, 64);

    layer_pools_release_pattern_node(pools, node);
}

TEST_F(LayerPoolsTest, Middleware_RouteNode_AcquireRelease)
{
    void* node = layer_pools_acquire_route_node(pools);
    ASSERT_NE(node, nullptr);

    memset(node, 0x47, 64);

    layer_pools_release_route_node(pools, node);
}

TEST_F(LayerPoolsTest, Middleware_FeatureBuffer_AcquireRelease)
{
    float* buffer = layer_pools_acquire_feature_buffer(pools, 128);
    ASSERT_NE(buffer, nullptr);

    for (int i = 0; i < 128; i++) {
        buffer[i] = (float)i * 0.1f;
    }

    layer_pools_release_feature_buffer(pools, buffer);
}

TEST_F(LayerPoolsTest, Middleware_SubscriberEntry_AcquireRelease)
{
    void* entry = layer_pools_acquire_subscriber_entry(pools);
    ASSERT_NE(entry, nullptr);

    layer_pools_release_subscriber_entry(pools, entry);
}

TEST_F(LayerPoolsTest, Middleware_MultipleEventEntries)
{
    const int COUNT = 500;
    void* entries[COUNT];

    for (int i = 0; i < COUNT; i++) {
        entries[i] = layer_pools_acquire_event_entry(pools);
        ASSERT_NE(entries[i], nullptr) << "Failed at " << i;
    }

    for (int i = 0; i < COUNT; i++) {
        layer_pools_release_event_entry(pools, entries[i]);
    }
}

TEST_F(LayerPoolsTest, Middleware_StatsTracking)
{
    void* event = layer_pools_acquire_event_entry(pools);
    void* pattern = layer_pools_acquire_pattern_node(pools);
    void* route = layer_pools_acquire_route_node(pools);

    layer_stats_t stats;
    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_MIDDLEWARE, &stats));
    EXPECT_GE(stats.total_acquires, 3);
    EXPECT_EQ(stats.current_in_use, 3);

    layer_pools_release_event_entry(pools, event);
    layer_pools_release_pattern_node(pools, pattern);
    layer_pools_release_route_node(pools, route);
}

TEST_F(LayerPoolsTest, Middleware_FeatureBufferSizes)
{
    size_t sizes[] = {16, 64, 256, 1024};

    for (size_t size : sizes) {
        float* buffer = layer_pools_acquire_feature_buffer(pools, size);
        ASSERT_NE(buffer, nullptr) << "Failed for size " << size;
        layer_pools_release_feature_buffer(pools, buffer);
    }
}

//=============================================================================
// Training Layer Pool Tests (6 tests)
//=============================================================================

TEST_F(LayerPoolsTest, Training_LearningSignal_AcquireRelease)
{
    void* signal = layer_pools_acquire_learning_signal(pools);
    ASSERT_NE(signal, nullptr);

    memset(signal, 0x48, 64);

    layer_pools_release_learning_signal(pools, signal);
}

TEST_F(LayerPoolsTest, Training_TargetPrediction_AcquireRelease)
{
    float* target = nullptr;
    float* prediction = nullptr;

    ASSERT_TRUE(layer_pools_acquire_target_prediction(pools, 100, &target, &prediction));
    ASSERT_NE(target, nullptr);
    ASSERT_NE(prediction, nullptr);

    // Initialize
    for (int i = 0; i < 100; i++) {
        target[i] = 1.0f;
        prediction[i] = 0.5f;
    }

    layer_pools_release_target_prediction(pools, target, prediction);
}

TEST_F(LayerPoolsTest, Training_GradientBuffer_AcquireRelease)
{
    float* gradient = layer_pools_acquire_gradient_buffer(pools, 512);
    ASSERT_NE(gradient, nullptr);

    for (int i = 0; i < 512; i++) {
        gradient[i] = 0.01f * i;
    }

    layer_pools_release_gradient_buffer(pools, gradient);
}

TEST_F(LayerPoolsTest, Training_BatchBuffer_AcquireRelease)
{
    void* batch = layer_pools_acquire_batch_buffer(pools, 32, 100);
    ASSERT_NE(batch, nullptr);

    memset(batch, 0x49, 32 * 100);

    layer_pools_release_batch_buffer(pools, batch, 32 * 100);
}

TEST_F(LayerPoolsTest, Training_StatsTracking)
{
    void* signal = layer_pools_acquire_learning_signal(pools);

    layer_stats_t stats;
    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_TRAINING, &stats));
    EXPECT_GE(stats.total_acquires, 1);

    layer_pools_release_learning_signal(pools, signal);
}

TEST_F(LayerPoolsTest, Training_MultipleLearningSignals)
{
    const int COUNT = 100;
    void* signals[COUNT];

    for (int i = 0; i < COUNT; i++) {
        signals[i] = layer_pools_acquire_learning_signal(pools);
        ASSERT_NE(signals[i], nullptr) << "Failed at " << i;
    }

    for (int i = 0; i < COUNT; i++) {
        layer_pools_release_learning_signal(pools, signals[i]);
    }
}

//=============================================================================
// Cross-Layer Operations Tests (4 tests)
//=============================================================================

TEST_F(LayerPoolsTest, CrossLayer_BrainPoolsIntegration)
{
    brain_pools_t bp = layer_pools_get_brain_pools(pools);
    ASSERT_NE(bp, nullptr);

    // Acquire from brain pools directly
    void* decision = brain_pools_acquire_decision(bp);
    EXPECT_NE(decision, nullptr);
    brain_pools_release_decision(bp, decision);
}

TEST_F(LayerPoolsTest, CrossLayer_Borrow)
{
    size_t borrowed = layer_pools_borrow(pools, LAYER_POOL_TRAINING, LAYER_POOL_MIDDLEWARE, 10);
    EXPECT_EQ(borrowed, 10);

    layer_pools_return(pools, LAYER_POOL_MIDDLEWARE, 10);
}

TEST_F(LayerPoolsTest, CrossLayer_Rebalance)
{
    // Acquire some entries to create utilization
    void* entries[10];
    for (int i = 0; i < 10; i++) {
        entries[i] = layer_pools_acquire_event_entry(pools);
    }

    EXPECT_TRUE(layer_pools_rebalance(pools));

    for (int i = 0; i < 10; i++) {
        layer_pools_release_event_entry(pools, entries[i]);
    }
}

TEST_F(LayerPoolsTest, CrossLayer_TransferTracking)
{
    layer_pools_borrow(pools, LAYER_POOL_COGNITIVE, LAYER_POOL_MIDDLEWARE, 5);
    layer_pools_borrow(pools, LAYER_POOL_TRAINING, LAYER_POOL_MIDDLEWARE, 3);

    layer_pools_metrics_t metrics;
    ASSERT_TRUE(layer_pools_get_metrics(pools, &metrics));
    EXPECT_EQ(metrics.cross_layer_transfers, 8);
}

//=============================================================================
// Metrics and Fairness Tests (6 tests)
//=============================================================================

TEST_F(LayerPoolsTest, Metrics_GetComprehensive)
{
    layer_pools_metrics_t metrics;
    ASSERT_TRUE(layer_pools_get_metrics(pools, &metrics));

    EXPECT_GE(metrics.uptime_ms, 0);
}

TEST_F(LayerPoolsTest, Metrics_PerLayerStats)
{
    layer_stats_t stats;

    EXPECT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_BRAIN, &stats));
    EXPECT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_MIDDLEWARE, &stats));
    EXPECT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_COGNITIVE, &stats));
    EXPECT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_TRAINING, &stats));
}

TEST_F(LayerPoolsTest, Metrics_FairnessIndex)
{
    fairness_metrics_t fairness;
    ASSERT_TRUE(layer_pools_get_fairness_metrics(pools, &fairness));

    // Jain's index should be between 0.25 and 1.0 for 4 layers
    EXPECT_GE(fairness.jains_fairness_index, 0.25f);
    EXPECT_LE(fairness.jains_fairness_index, 1.0f);
}

TEST_F(LayerPoolsTest, Metrics_CrossEntropy)
{
    cross_entropy_metrics_t ce;
    ASSERT_TRUE(layer_pools_get_cross_entropy_metrics(pools, &ce));

    EXPECT_GE(ce.efficiency, 0.0f);
    EXPECT_LE(ce.efficiency, 1.0f);
}

TEST_F(LayerPoolsTest, Metrics_Reset)
{
    void* entry = layer_pools_acquire_event_entry(pools);
    layer_pools_release_event_entry(pools, entry);

    layer_pools_reset_metrics(pools);

    layer_stats_t stats;
    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_MIDDLEWARE, &stats));
    EXPECT_EQ(stats.total_acquires, 0);
}

TEST_F(LayerPoolsTest, Metrics_IsPerformant)
{
    // Fresh pools should be performant
    EXPECT_TRUE(layer_pools_is_performant(pools));
}

//=============================================================================
// Thread Safety Tests (4 tests)
//=============================================================================

TEST_F(LayerPoolsTest, ThreadSafety_ConcurrentCognitiveAcquire)
{
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 50;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            void* entry = layer_pools_acquire_workspace_entry(pools);
            if (entry) {
                layer_pools_release_workspace_entry(pools, entry);
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);
}

TEST_F(LayerPoolsTest, ThreadSafety_ConcurrentMiddlewareAcquire)
{
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            void* entry = layer_pools_acquire_event_entry(pools);
            if (entry) {
                layer_pools_release_event_entry(pools, entry);
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);
}

TEST_F(LayerPoolsTest, ThreadSafety_ConcurrentTrainingAcquire)
{
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 50;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            void* signal = layer_pools_acquire_learning_signal(pools);
            if (signal) {
                layer_pools_release_learning_signal(pools, signal);
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);
}

TEST_F(LayerPoolsTest, ThreadSafety_ConcurrentMetricsRead)
{
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 50;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            layer_pools_metrics_t metrics;
            if (layer_pools_get_metrics(pools, &metrics)) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);
}

//=============================================================================
// Edge Cases and Null Safety Tests (6 tests)
//=============================================================================

TEST_F(LayerPoolsTest, NullSafety_DestroyNull)
{
    layer_pools_destroy(nullptr);  // Should not crash
}

TEST_F(LayerPoolsTest, NullSafety_AcquireFromNull)
{
    EXPECT_EQ(layer_pools_acquire_workspace_entry(nullptr), nullptr);
    EXPECT_EQ(layer_pools_acquire_event_entry(nullptr), nullptr);
    EXPECT_EQ(layer_pools_acquire_learning_signal(nullptr), nullptr);
}

TEST_F(LayerPoolsTest, NullSafety_ReleaseToNull)
{
    layer_pools_release_workspace_entry(nullptr, (void*)0x1234);
    layer_pools_release_event_entry(nullptr, (void*)0x1234);
    layer_pools_release_learning_signal(nullptr, (void*)0x1234);
    // Should not crash
}

TEST_F(LayerPoolsTest, NullSafety_ReleaseNullEntry)
{
    layer_pools_release_workspace_entry(pools, nullptr);
    layer_pools_release_event_entry(pools, nullptr);
    layer_pools_release_learning_signal(pools, nullptr);
    // Should not crash
}

TEST_F(LayerPoolsTest, NullSafety_MetricsFunctions)
{
    layer_pools_metrics_t metrics;
    layer_stats_t stats;
    fairness_metrics_t fairness;

    EXPECT_FALSE(layer_pools_get_metrics(nullptr, &metrics));
    EXPECT_FALSE(layer_pools_get_metrics(pools, nullptr));
    EXPECT_FALSE(layer_pools_get_layer_stats(nullptr, LAYER_POOL_COGNITIVE, &stats));
    EXPECT_FALSE(layer_pools_get_layer_stats(pools, LAYER_POOL_COUNT + 1, &stats));
    EXPECT_FALSE(layer_pools_get_fairness_metrics(nullptr, &fairness));
}

TEST_F(LayerPoolsTest, Utility_CalculateMemory)
{
    layer_pools_config_t config = layer_pools_default_config();
    size_t memory = layer_pools_calculate_memory(&config);
    EXPECT_GT(memory, 0);

    // NULL config should use defaults
    size_t memory_null = layer_pools_calculate_memory(nullptr);
    EXPECT_EQ(memory, memory_null);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
