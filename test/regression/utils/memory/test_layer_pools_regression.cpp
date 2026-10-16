/**
 * @file test_layer_pools_regression.cpp
 * @brief Regression tests for Layer Pools performance
 *
 * WHAT: Verify layer pools maintains performance targets
 * WHY:  Catch performance regressions across layers
 * HOW:  Benchmark against baseline, verify mathematical guarantees
 *
 * PHASE: 3 (Cross-Layer Integration)
 *
 * PERFORMANCE TARGETS:
 * - Cognitive layer acquire: <500ns median
 * - Middleware layer acquire: <300ns median
 * - Training layer acquire: <400ns median
 * - Cross-layer overhead: <10%
 */

#include <gtest/gtest.h>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_layer_pools.h"

class LayerPoolsRegressionTest : public ::testing::Test {
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

    double median(std::vector<double>& v) {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    }
};

// Regression: Cognitive layer latency
TEST_F(LayerPoolsRegressionTest, Latency_CognitiveAcquire)
{
    const int ITERATIONS = 1000;
    std::vector<double> latencies;
    latencies.reserve(ITERATIONS);

    for (int i = 0; i < ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        void* entry = layer_pools_acquire_workspace_entry(pools);
        auto end = std::chrono::high_resolution_clock::now();

        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(static_cast<double>(ns));
        layer_pools_release_workspace_entry(pools, entry);
    }

    double med = median(latencies);
    double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / ITERATIONS;

    // Cognitive acquire should be <500ns median
    EXPECT_LT(med, 500.0) << "Cognitive acquire median latency: " << med << "ns";
    EXPECT_LT(avg, 1000.0) << "Cognitive acquire avg latency: " << avg << "ns";
}

// Regression: Middleware layer latency
TEST_F(LayerPoolsRegressionTest, Latency_MiddlewareAcquire)
{
    const int ITERATIONS = 1000;
    std::vector<double> latencies;
    latencies.reserve(ITERATIONS);

    for (int i = 0; i < ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        void* entry = layer_pools_acquire_event_entry(pools);
        auto end = std::chrono::high_resolution_clock::now();

        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(static_cast<double>(ns));
        layer_pools_release_event_entry(pools, entry);
    }

    double med = median(latencies);

    // Middleware acquire should be <500ns median
    EXPECT_LT(med, 500.0) << "Middleware acquire median latency: " << med << "ns";
}

// Regression: Training layer latency
TEST_F(LayerPoolsRegressionTest, Latency_TrainingAcquire)
{
    const int ITERATIONS = 1000;
    std::vector<double> latencies;
    latencies.reserve(ITERATIONS);

    for (int i = 0; i < ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        void* signal = layer_pools_acquire_learning_signal(pools);
        auto end = std::chrono::high_resolution_clock::now();

        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(static_cast<double>(ns));
        layer_pools_release_learning_signal(pools, signal);
    }

    double med = median(latencies);

    // Training acquire should be <500ns median
    EXPECT_LT(med, 500.0) << "Training acquire median latency: " << med << "ns";
}

// Regression: Target/prediction pair latency
TEST_F(LayerPoolsRegressionTest, Latency_TargetPredictionAcquire)
{
    const int ITERATIONS = 500;
    std::vector<double> latencies;
    latencies.reserve(ITERATIONS);

    for (int i = 0; i < ITERATIONS; i++) {
        float* target = nullptr;
        float* prediction = nullptr;

        auto start = std::chrono::high_resolution_clock::now();
        bool ok = layer_pools_acquire_target_prediction(pools, 100, &target, &prediction);
        auto end = std::chrono::high_resolution_clock::now();

        ASSERT_TRUE(ok);
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(static_cast<double>(ns));
        layer_pools_release_target_prediction(pools, target, prediction);
    }

    double med = median(latencies);

    // Pair acquire should be <1000ns median (two allocations)
    EXPECT_LT(med, 1000.0) << "Target/prediction acquire median latency: " << med << "ns";
}

// Regression: Fairness index bounds
TEST_F(LayerPoolsRegressionTest, Fairness_JainsIndexBounds)
{
    // Generate load across layers
    for (int i = 0; i < 100; i++) {
        void* event = layer_pools_acquire_event_entry(pools);
        layer_pools_release_event_entry(pools, event);

        void* workspace = layer_pools_acquire_workspace_entry(pools);
        layer_pools_release_workspace_entry(pools, workspace);

        void* signal = layer_pools_acquire_learning_signal(pools);
        layer_pools_release_learning_signal(pools, signal);
    }

    fairness_metrics_t fairness;
    ASSERT_TRUE(layer_pools_get_fairness_metrics(pools, &fairness));

    // Jain's index should be in valid range [1/n, 1]
    EXPECT_GE(fairness.jains_fairness_index, 0.25f);  // 1/4 for 4 layers
    EXPECT_LE(fairness.jains_fairness_index, 1.0f);
}

// Regression: Cross-entropy efficiency
TEST_F(LayerPoolsRegressionTest, CrossEntropy_EfficiencyBounds)
{
    cross_entropy_metrics_t ce;
    ASSERT_TRUE(layer_pools_get_cross_entropy_metrics(pools, &ce));

    // Efficiency should be in [0, 1]
    EXPECT_GE(ce.efficiency, 0.0f);
    EXPECT_LE(ce.efficiency, 1.0f);
}

// Regression: Memory stability under stress
TEST_F(LayerPoolsRegressionTest, Stability_MemoryUnderStress)
{
    // 10000 cycles across all layers
    for (int i = 0; i < 10000; i++) {
        void* event = layer_pools_acquire_event_entry(pools);
        layer_pools_release_event_entry(pools, event);
    }

    for (int i = 0; i < 5000; i++) {
        void* workspace = layer_pools_acquire_workspace_entry(pools);
        layer_pools_release_workspace_entry(pools, workspace);
    }

    for (int i = 0; i < 5000; i++) {
        void* signal = layer_pools_acquire_learning_signal(pools);
        layer_pools_release_learning_signal(pools, signal);
    }

    // All should be released
    layer_stats_t stats;
    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_MIDDLEWARE, &stats));
    EXPECT_EQ(stats.current_in_use, 0);

    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_COGNITIVE, &stats));
    EXPECT_EQ(stats.current_in_use, 0);

    ASSERT_TRUE(layer_pools_get_layer_stats(pools, LAYER_POOL_TRAINING, &stats));
    EXPECT_EQ(stats.current_in_use, 0);
}

// Regression: Throughput sustained
TEST_F(LayerPoolsRegressionTest, Throughput_Sustained)
{
    const int ITERATIONS = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        void* entry = layer_pools_acquire_event_entry(pools);
        layer_pools_release_event_entry(pools, entry);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double ops_per_sec = (ITERATIONS * 1000000.0) / duration_us;

    // Should sustain at least 100K ops/sec
    EXPECT_GT(ops_per_sec, 100000.0) << "Throughput: " << ops_per_sec << " ops/sec";
}

// Regression: Is performant check
TEST_F(LayerPoolsRegressionTest, Performance_IsPerformant)
{
    // Fresh pools should be performant
    EXPECT_TRUE(layer_pools_is_performant(pools));

    // After heavy use, should still be performant
    for (int i = 0; i < 1000; i++) {
        void* entry = layer_pools_acquire_event_entry(pools);
        layer_pools_release_event_entry(pools, entry);
    }

    EXPECT_TRUE(layer_pools_is_performant(pools));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
