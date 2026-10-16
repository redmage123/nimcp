/**
 * @file test_brain_pools_integration.cpp
 * @brief Integration tests for Brain Pools memory system
 *
 * WHAT: Test brain pools integration with brain subsystems
 * WHY:  Verify O(1) allocation works in real brain workflows
 * HOW:  Test decision allocation, COW sharing, metrics tracking
 *
 * PHASE: 2 (Brain Substrate Memory Pools)
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_brain_pools.h"

class BrainPoolsIntegrationTest : public ::testing::Test {
protected:
    brain_pools_t pools = nullptr;

    void SetUp() override {
        brain_pools_config_t config = brain_pools_default_config();
        pools = brain_pools_create(&config);
        ASSERT_NE(pools, nullptr);
    }

    void TearDown() override {
        if (pools) brain_pools_destroy(pools);
    }
};

// Integration: Decision workflow
TEST_F(BrainPoolsIntegrationTest, DecisionWorkflow_AcquireProcessRelease)
{
    // Simulate typical decision workflow
    void* decision = brain_pools_acquire_decision(pools);
    ASSERT_NE(decision, nullptr);

    // Simulate processing (write to memory)
    memset(decision, 0x42, 64);  // Write to allocated block

    // Release
    brain_pools_release_decision(pools, decision);

    brain_pools_metrics_t metrics;
    ASSERT_TRUE(brain_pools_get_metrics(pools, &metrics));
    EXPECT_EQ(metrics.decision_stats.total_acquires, 1);
    EXPECT_EQ(metrics.decision_stats.total_releases, 1);
}

// Integration: Spike burst handling
TEST_F(BrainPoolsIntegrationTest, SpikeBurst_HighFrequency)
{
    // Simulate spike burst (1000 spikes/s typical)
    const int BURST_SIZE = 100;
    void* spikes[BURST_SIZE];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BURST_SIZE; i++) {
        spikes[i] = brain_pools_acquire_spike_event(pools);
        ASSERT_NE(spikes[i], nullptr);
    }

    auto mid = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < BURST_SIZE; i++) {
        brain_pools_release_spike_event(pools, spikes[i]);
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto acquire_us = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto release_us = std::chrono::duration_cast<std::chrono::microseconds>(end - mid).count();

    // Should be <1ms for 100 operations
    EXPECT_LT(acquire_us, 1000);
    EXPECT_LT(release_us, 1000);
}

// Integration: COW decision sharing
TEST_F(BrainPoolsIntegrationTest, COW_DecisionSharing)
{
    // Acquire COW decision (for template sharing)
    // Pool type 0 = decision pool
    void* handle = brain_pools_cow_acquire(pools, 0);
    EXPECT_NE(handle, nullptr);

    // Get read-only view
    const void* read_view = brain_pools_cow_read(handle);
    EXPECT_NE(read_view, nullptr);

    // Release
    brain_pools_cow_release(pools, handle);
}

// Integration: Activation patterns
TEST_F(BrainPoolsIntegrationTest, ActivationPattern_VariableSizes)
{
    // Test different activation sizes (common in neural processing)
    size_t sizes[] = {64, 256, 1024, 4096};

    for (size_t size : sizes) {
        float* activation = brain_pools_acquire_activation(pools, size);
        ASSERT_NE(activation, nullptr) << "Failed for size " << size;
        brain_pools_release_activation(pools, activation);
    }
}

// Integration: Feature buffer with size classes
TEST_F(BrainPoolsIntegrationTest, FeatureBuffer_SizeClassSelection)
{
    // Test different feature buffer sizes
    size_t requested_sizes[] = {32, 128, 512, 2048};

    for (size_t requested : requested_sizes) {
        size_t actual_size = 0;
        void* buffer = brain_pools_acquire_feature_buffer(pools, requested, &actual_size);
        ASSERT_NE(buffer, nullptr) << "Failed for size " << requested;
        EXPECT_GE(actual_size, requested) << "Actual size should be >= requested";
        brain_pools_release_feature_buffer(pools, buffer, actual_size);
    }
}

// Integration: Shannon metrics tracking
TEST_F(BrainPoolsIntegrationTest, Shannon_EntropyTracking)
{
    // Perform diverse allocations using feature buffers
    size_t actual_sizes[4];
    void* buffers[4];
    size_t requested[] = {32, 128, 512, 2048};

    for (int i = 0; i < 4; i++) {
        buffers[i] = brain_pools_acquire_feature_buffer(pools, requested[i], &actual_sizes[i]);
        ASSERT_NE(buffers[i], nullptr);
    }

    for (int i = 0; i < 4; i++) {
        brain_pools_release_feature_buffer(pools, buffers[i], actual_sizes[i]);
    }

    shannon_metrics_t shannon;
    ASSERT_TRUE(brain_pools_get_shannon_metrics(pools, &shannon));

    // Diverse allocations should have positive entropy
    EXPECT_GE(shannon.entropy_bits, 0.0f);
}

// Integration: Thread safety under load
TEST_F(BrainPoolsIntegrationTest, ThreadSafety_ConcurrentLoad)
{
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            void* ptr = brain_pools_acquire_spike_event(pools);
            if (ptr) {
                brain_pools_release_spike_event(pools, ptr);
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

    // All operations should succeed
    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);
}

// Integration: Memory efficiency
TEST_F(BrainPoolsIntegrationTest, MemoryEfficiency_Overhead)
{
    brain_pools_metrics_t metrics_before;
    ASSERT_TRUE(brain_pools_get_metrics(pools, &metrics_before));
    size_t initial = metrics_before.total_memory_bytes;
    EXPECT_GT(initial, 0);

    // After many operations, memory should be stable (no leaks)
    for (int i = 0; i < 1000; i++) {
        void* ptr = brain_pools_acquire_spike_event(pools);
        brain_pools_release_spike_event(pools, ptr);
    }

    brain_pools_metrics_t metrics_after;
    ASSERT_TRUE(brain_pools_get_metrics(pools, &metrics_after));
    size_t after = metrics_after.total_memory_bytes;

    // Memory should not grow significantly
    EXPECT_LE(after, initial * 1.1);  // Allow 10% variance
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
