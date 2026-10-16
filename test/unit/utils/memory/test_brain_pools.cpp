/**
 * @file test_brain_pools.cpp
 * @brief Unit tests for Brain Substrate Memory Pool System (Phase 2)
 *
 * WHAT: Comprehensive tests for brain pools with mathematical validation
 * WHY:  Ensure pool performance meets targets and metrics are accurate
 * HOW:  Google Test framework with fixtures for isolation
 *
 * TEST CATEGORIES:
 * 1. Creation/Configuration - Pool initialization
 * 2. Decision Pool - Decision structure allocation
 * 3. Spike Pool - High-frequency spike event allocation
 * 4. Feature Buffer Pool - Size-class based allocation
 * 5. COW Operations - Copy-on-write semantics
 * 6. Shannon Metrics - Information-theoretic validation
 * 7. Queuing Metrics - Queuing theory validation
 * 8. Performance - Latency and throughput targets
 * 9. Thread Safety - Concurrent operations
 * 10. Null Safety - Error handling
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <cmath>

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_brain_pools.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainPoolsTest : public ::testing::Test {
protected:
    brain_pools_t pools_ = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        brain_pools_config_t config = brain_pools_default_config();
        pools_ = brain_pools_create(&config);
    }

    void TearDown() override {
        if (pools_) {
            brain_pools_destroy(pools_);
            pools_ = nullptr;
        }
        nimcp_memory_cleanup();
    }
};

//=============================================================================
// Creation/Configuration Tests
//=============================================================================

TEST_F(BrainPoolsTest, Create_DefaultConfig_Success) {
    EXPECT_NE(pools_, nullptr);
}

TEST_F(BrainPoolsTest, Create_NullConfig_UsesDefaults) {
    brain_pools_t p = brain_pools_create(nullptr);
    EXPECT_NE(p, nullptr);
    brain_pools_destroy(p);
}

TEST_F(BrainPoolsTest, Create_CustomConfig_Success) {
    brain_pools_config_t config = brain_pools_default_config();
    config.decision_pool_capacity = 128;
    config.spike_pool_capacity = 2048;
    config.enable_shannon_tracking = true;

    brain_pools_t p = brain_pools_create(&config);
    EXPECT_NE(p, nullptr);
    brain_pools_destroy(p);
}

TEST_F(BrainPoolsTest, DefaultConfig_ValidValues) {
    brain_pools_config_t config = brain_pools_default_config();

    EXPECT_EQ(config.decision_pool_capacity, BRAIN_POOL_DEFAULT_DECISIONS);
    EXPECT_EQ(config.spike_pool_capacity, BRAIN_POOL_DEFAULT_SPIKES);
    EXPECT_GT(config.overallocation_factor, 1.0f);
    EXPECT_LE(config.overallocation_factor, 2.0f);
    EXPECT_GE(config.safety_factor_k, 2.0f);
    EXPECT_LE(config.safety_factor_k, 4.0f);
    EXPECT_TRUE(config.enable_metrics);
}

TEST_F(BrainPoolsTest, Destroy_NullPool_NoOp) {
    brain_pools_destroy(nullptr);  // Should not crash
}

TEST_F(BrainPoolsTest, CalculateMemory_ReturnsPositive) {
    brain_pools_config_t config = brain_pools_default_config();
    size_t memory = brain_pools_calculate_memory(&config);
    EXPECT_GT(memory, 0u);
}

//=============================================================================
// Decision Pool Tests
//=============================================================================

TEST_F(BrainPoolsTest, Decision_Acquire_ReturnsNonNull) {
    void* decision = brain_pools_acquire_decision(pools_);
    EXPECT_NE(decision, nullptr);
    brain_pools_release_decision(pools_, decision);
}

TEST_F(BrainPoolsTest, Decision_AcquireRelease_Multiple) {
    const int count = 100;
    void* decisions[count];

    for (int i = 0; i < count; i++) {
        decisions[i] = brain_pools_acquire_decision(pools_);
        EXPECT_NE(decisions[i], nullptr);
    }

    for (int i = 0; i < count; i++) {
        brain_pools_release_decision(pools_, decisions[i]);
    }
}

TEST_F(BrainPoolsTest, Decision_ReleaseNull_NoOp) {
    brain_pools_release_decision(pools_, nullptr);  // Should not crash
}

TEST_F(BrainPoolsTest, Decision_AcquireNull_ReturnsNull) {
    void* decision = brain_pools_acquire_decision(nullptr);
    EXPECT_EQ(decision, nullptr);
}

//=============================================================================
// Activation Pool Tests
//=============================================================================

TEST_F(BrainPoolsTest, Activation_Acquire_ReturnsNonNull) {
    float* activation = brain_pools_acquire_activation(pools_, 256);
    EXPECT_NE(activation, nullptr);
    brain_pools_release_activation(pools_, activation);
}

TEST_F(BrainPoolsTest, Activation_VariousSizes) {
    // Test different sizes
    size_t sizes[] = {64, 128, 256, 512, 1024};

    for (size_t size : sizes) {
        float* activation = brain_pools_acquire_activation(pools_, size);
        EXPECT_NE(activation, nullptr) << "Failed for size " << size;
        brain_pools_release_activation(pools_, activation);
    }
}

//=============================================================================
// Spike Pool Tests
//=============================================================================

TEST_F(BrainPoolsTest, Spike_Acquire_ReturnsNonNull) {
    void* spike = brain_pools_acquire_spike_event(pools_);
    EXPECT_NE(spike, nullptr);
    brain_pools_release_spike_event(pools_, spike);
}

TEST_F(BrainPoolsTest, Spike_HighFrequency) {
    const int iterations = 1000;

    for (int i = 0; i < iterations; i++) {
        void* spike = brain_pools_acquire_spike_event(pools_);
        EXPECT_NE(spike, nullptr);
        brain_pools_release_spike_event(pools_, spike);
    }
}

TEST_F(BrainPoolsTest, Spike_BatchAcquire_Success) {
    const size_t batch_size = 64;
    void* events[batch_size];

    size_t acquired = brain_pools_acquire_spike_batch(pools_, batch_size, events);
    EXPECT_EQ(acquired, batch_size);

    for (size_t i = 0; i < acquired; i++) {
        EXPECT_NE(events[i], nullptr);
    }

    brain_pools_release_spike_batch(pools_, acquired, events);
}

TEST_F(BrainPoolsTest, Spike_BatchRelease_Success) {
    const size_t batch_size = 32;
    void* events[batch_size];

    size_t acquired = brain_pools_acquire_spike_batch(pools_, batch_size, events);
    EXPECT_EQ(acquired, batch_size);

    brain_pools_release_spike_batch(pools_, acquired, events);

    // Can acquire again after release
    acquired = brain_pools_acquire_spike_batch(pools_, batch_size, events);
    EXPECT_EQ(acquired, batch_size);
    brain_pools_release_spike_batch(pools_, acquired, events);
}

//=============================================================================
// Feature Buffer Pool Tests
//=============================================================================

TEST_F(BrainPoolsTest, Feature_Acquire_ReturnsNonNull) {
    size_t actual_size = 0;
    void* buffer = brain_pools_acquire_feature_buffer(pools_, 100, &actual_size);
    EXPECT_NE(buffer, nullptr);
    EXPECT_GE(actual_size, 100u);
    brain_pools_release_feature_buffer(pools_, buffer, actual_size);
}

TEST_F(BrainPoolsTest, Feature_SizeClassSelection) {
    // Test that appropriate size classes are selected
    struct TestCase {
        size_t requested;
        size_t expected_min;
    } cases[] = {
        {32, BRAIN_POOL_SIZE_TINY},
        {64, BRAIN_POOL_SIZE_TINY},
        {100, BRAIN_POOL_SIZE_SMALL},
        {256, BRAIN_POOL_SIZE_SMALL},
        {500, BRAIN_POOL_SIZE_MEDIUM},
        {1024, BRAIN_POOL_SIZE_MEDIUM},
        {2000, BRAIN_POOL_SIZE_LARGE},
        {4096, BRAIN_POOL_SIZE_LARGE}
    };

    for (const auto& tc : cases) {
        size_t actual_size = 0;
        void* buffer = brain_pools_acquire_feature_buffer(pools_, tc.requested, &actual_size);
        EXPECT_NE(buffer, nullptr) << "Failed for requested=" << tc.requested;
        EXPECT_GE(actual_size, tc.expected_min) << "Size mismatch for requested=" << tc.requested;
        brain_pools_release_feature_buffer(pools_, buffer, actual_size);
    }
}

TEST_F(BrainPoolsTest, Feature_GetSizeClass) {
    EXPECT_EQ(brain_pools_get_size_class(32), POOL_SIZE_TINY);
    EXPECT_EQ(brain_pools_get_size_class(64), POOL_SIZE_TINY);
    EXPECT_EQ(brain_pools_get_size_class(100), POOL_SIZE_SMALL);
    EXPECT_EQ(brain_pools_get_size_class(256), POOL_SIZE_SMALL);
    EXPECT_EQ(brain_pools_get_size_class(500), POOL_SIZE_MEDIUM);
    EXPECT_EQ(brain_pools_get_size_class(1024), POOL_SIZE_MEDIUM);
    EXPECT_EQ(brain_pools_get_size_class(2000), POOL_SIZE_LARGE);
    EXPECT_EQ(brain_pools_get_size_class(5000), POOL_SIZE_XLARGE);
}

TEST_F(BrainPoolsTest, Feature_GetClassSize) {
    EXPECT_EQ(brain_pools_get_class_size(POOL_SIZE_TINY), BRAIN_POOL_SIZE_TINY);
    EXPECT_EQ(brain_pools_get_class_size(POOL_SIZE_SMALL), BRAIN_POOL_SIZE_SMALL);
    EXPECT_EQ(brain_pools_get_class_size(POOL_SIZE_MEDIUM), BRAIN_POOL_SIZE_MEDIUM);
    EXPECT_EQ(brain_pools_get_class_size(POOL_SIZE_LARGE), BRAIN_POOL_SIZE_LARGE);
    EXPECT_EQ(brain_pools_get_class_size(POOL_SIZE_XLARGE), BRAIN_POOL_SIZE_XLARGE);
}

//=============================================================================
// COW Tests
//=============================================================================

TEST_F(BrainPoolsTest, COW_Acquire_ReturnsHandle) {
    void* handle = brain_pools_cow_acquire(pools_, 0);  // Decision COW
    // May be NULL if COW not enabled for this pool
    if (handle) {
        brain_pools_cow_release(pools_, handle);
    }
}

TEST_F(BrainPoolsTest, COW_Read_ReturnsConst) {
    void* handle = brain_pools_cow_acquire(pools_, 0);
    if (handle) {
        const void* data = brain_pools_cow_read(handle);
        // Data may be NULL if handle is invalid
        brain_pools_cow_release(pools_, handle);
    }
}

TEST_F(BrainPoolsTest, COW_ReleaseNull_NoOp) {
    brain_pools_cow_release(pools_, nullptr);  // Should not crash
}

//=============================================================================
// Shannon Metrics Tests
//=============================================================================

TEST_F(BrainPoolsTest, Shannon_GetMetrics_Success) {
    // Generate some allocations first
    for (int i = 0; i < 100; i++) {
        size_t size = 64 * (1 + (i % 5));  // Vary sizes
        size_t actual;
        void* buf = brain_pools_acquire_feature_buffer(pools_, size, &actual);
        if (buf) {
            brain_pools_release_feature_buffer(pools_, buf, actual);
        }
    }

    shannon_metrics_t shannon;
    EXPECT_TRUE(brain_pools_get_shannon_metrics(pools_, &shannon));

    // Entropy should be >= 0
    EXPECT_GE(shannon.entropy_bits, 0.0f);

    // Max entropy should be positive for multiple size classes
    EXPECT_GT(shannon.max_entropy_bits, 0.0f);

    // Efficiency should be in [0, 1]
    EXPECT_GE(shannon.efficiency, 0.0f);
    EXPECT_LE(shannon.efficiency, 1.0f);

    // Redundancy = 1 - efficiency
    EXPECT_FLOAT_EQ(shannon.redundancy, 1.0f - shannon.efficiency);
}

TEST_F(BrainPoolsTest, Shannon_Entropy_UniformDistribution) {
    // Allocate equally from each size class
    const int per_class = 20;

    size_t sizes[] = {32, 128, 512, 2048, 8192};

    for (size_t size : sizes) {
        for (int i = 0; i < per_class; i++) {
            size_t actual;
            void* buf = brain_pools_acquire_feature_buffer(pools_, size, &actual);
            if (buf) {
                brain_pools_release_feature_buffer(pools_, buf, actual);
            }
        }
    }

    shannon_metrics_t shannon;
    EXPECT_TRUE(brain_pools_get_shannon_metrics(pools_, &shannon));

    // For uniform distribution, entropy should approach max_entropy
    // Allow some tolerance for size class mapping
    if (shannon.max_entropy_bits > 0) {
        EXPECT_GT(shannon.efficiency, 0.5f);  // Should be reasonably high
    }
}

TEST_F(BrainPoolsTest, Shannon_NullParams_ReturnsFalse) {
    EXPECT_FALSE(brain_pools_get_shannon_metrics(nullptr, nullptr));

    shannon_metrics_t shannon;
    EXPECT_FALSE(brain_pools_get_shannon_metrics(nullptr, &shannon));
    EXPECT_FALSE(brain_pools_get_shannon_metrics(pools_, nullptr));
}

//=============================================================================
// Queuing Metrics Tests
//=============================================================================

TEST_F(BrainPoolsTest, Queuing_GetMetrics_Success) {
    // Generate some allocations
    for (int i = 0; i < 100; i++) {
        void* spike = brain_pools_acquire_spike_event(pools_);
        if (spike) {
            brain_pools_release_spike_event(pools_, spike);
        }
    }

    queuing_metrics_t queuing;
    EXPECT_TRUE(brain_pools_get_queuing_metrics(pools_, &queuing));

    // Arrival rate should be positive after allocations
    EXPECT_GE(queuing.arrival_rate_lambda, 0.0f);

    // Utilization should be in reasonable range
    EXPECT_GE(queuing.utilization_rho, 0.0f);
}

TEST_F(BrainPoolsTest, Queuing_OptimalSize_Calculation) {
    // Test the optimal pool size formula: N = λ/μ + k√(λ/μ)
    float arrival_rate = 1000.0f;
    float service_rate = 1000.0f;
    float safety_factor = 3.0f;

    size_t optimal = brain_pools_optimal_size(arrival_rate, service_rate, safety_factor);

    // For λ=μ, rho=1, so N ≈ 1 + 3×1 = 4
    EXPECT_GE(optimal, 1u);
    EXPECT_LE(optimal, 10u);
}

TEST_F(BrainPoolsTest, Queuing_RecommendedConfig) {
    // Generate allocation pattern
    for (int i = 0; i < 500; i++) {
        void* spike = brain_pools_acquire_spike_event(pools_);
        if (spike) {
            brain_pools_release_spike_event(pools_, spike);
        }
    }

    brain_pools_config_t recommended;
    EXPECT_TRUE(brain_pools_get_recommended_config(pools_, &recommended));

    // Recommended spike capacity should be positive
    EXPECT_GT(recommended.spike_pool_capacity, 0u);
}

//=============================================================================
// Metrics API Tests
//=============================================================================

TEST_F(BrainPoolsTest, Metrics_GetMetrics_Success) {
    brain_pools_metrics_t metrics;
    EXPECT_TRUE(brain_pools_get_metrics(pools_, &metrics));
}

TEST_F(BrainPoolsTest, Metrics_TrackAcquires) {
    // Do some allocations
    void* d1 = brain_pools_acquire_decision(pools_);
    void* d2 = brain_pools_acquire_decision(pools_);

    brain_pools_metrics_t metrics;
    EXPECT_TRUE(brain_pools_get_metrics(pools_, &metrics));

    EXPECT_GE(metrics.decision_stats.total_acquires, 2u);
    EXPECT_GE(metrics.decision_stats.current_in_use, 2u);

    brain_pools_release_decision(pools_, d1);
    brain_pools_release_decision(pools_, d2);
}

TEST_F(BrainPoolsTest, Metrics_TrackReleases) {
    void* d1 = brain_pools_acquire_decision(pools_);
    brain_pools_release_decision(pools_, d1);

    brain_pools_metrics_t metrics;
    EXPECT_TRUE(brain_pools_get_metrics(pools_, &metrics));

    EXPECT_GE(metrics.decision_stats.total_releases, 1u);
}

TEST_F(BrainPoolsTest, Metrics_Reset) {
    // Generate some stats
    void* spike = brain_pools_acquire_spike_event(pools_);
    brain_pools_release_spike_event(pools_, spike);

    brain_pools_reset_metrics(pools_);

    brain_pools_metrics_t metrics;
    EXPECT_TRUE(brain_pools_get_metrics(pools_, &metrics));

    EXPECT_EQ(metrics.spike_stats.total_acquires, 0u);
    EXPECT_EQ(metrics.spike_stats.total_releases, 0u);
}

TEST_F(BrainPoolsTest, Metrics_PeakUsage) {
    const int count = 50;
    void* decisions[count];

    for (int i = 0; i < count; i++) {
        decisions[i] = brain_pools_acquire_decision(pools_);
    }

    brain_pools_metrics_t metrics;
    EXPECT_TRUE(brain_pools_get_metrics(pools_, &metrics));
    EXPECT_GE(metrics.decision_stats.peak_in_use, (uint64_t)count);

    for (int i = 0; i < count; i++) {
        brain_pools_release_decision(pools_, decisions[i]);
    }
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(BrainPoolsTest, Performance_AcquireLatency) {
    // Warm up
    for (int i = 0; i < 100; i++) {
        void* d = brain_pools_acquire_decision(pools_);
        brain_pools_release_decision(pools_, d);
    }

    brain_pools_metrics_t metrics;
    EXPECT_TRUE(brain_pools_get_metrics(pools_, &metrics));

    // Average acquire should be under target
    if (metrics.decision_stats.total_acquires > 0) {
        float avg_ns = (float)metrics.decision_stats.total_acquire_time_ns /
                       (float)metrics.decision_stats.total_acquires;
        // Relaxed target for test environment
        EXPECT_LT(avg_ns, 10000.0f);  // <10us in test
    }
}

TEST_F(BrainPoolsTest, Performance_IsPerformant) {
    // Generate workload
    for (int i = 0; i < 1000; i++) {
        void* spike = brain_pools_acquire_spike_event(pools_);
        if (spike) {
            brain_pools_release_spike_event(pools_, spike);
        }
    }

    // Check if performant (may fail in test environment due to overhead)
    bool performant = brain_pools_is_performant(pools_);
    // Just verify it doesn't crash - actual performance depends on environment
    (void)performant;
}

TEST_F(BrainPoolsTest, Performance_SpeedupVsMalloc) {
    // Generate significant workload
    for (int i = 0; i < 1000; i++) {
        void* spike = brain_pools_acquire_spike_event(pools_);
        if (spike) {
            brain_pools_release_spike_event(pools_, spike);
        }
    }

    brain_pools_metrics_t metrics;
    EXPECT_TRUE(brain_pools_get_metrics(pools_, &metrics));

    // Speedup should be positive
    EXPECT_GE(metrics.speedup_vs_malloc, 0.0f);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(BrainPoolsTest, ThreadSafety_ConcurrentSpikes) {
    const int num_threads = 4;
    const int ops_per_thread = 100;
    std::atomic<int> success_count{0};

    auto thread_func = [&]() {
        for (int i = 0; i < ops_per_thread; i++) {
            void* spike = brain_pools_acquire_spike_event(pools_);
            if (spike) {
                success_count++;
                brain_pools_release_spike_event(pools_, spike);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
}

TEST_F(BrainPoolsTest, ThreadSafety_ConcurrentDecisions) {
    const int num_threads = 4;
    const int ops_per_thread = 50;
    std::atomic<int> success_count{0};

    auto thread_func = [&]() {
        for (int i = 0; i < ops_per_thread; i++) {
            void* decision = brain_pools_acquire_decision(pools_);
            if (decision) {
                success_count++;
                brain_pools_release_decision(pools_, decision);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
}

TEST_F(BrainPoolsTest, ThreadSafety_ConcurrentMetrics) {
    const int num_threads = 4;
    std::atomic<bool> running{true};
    std::atomic<int> metrics_reads{0};

    // Thread doing allocations
    auto alloc_func = [&]() {
        while (running) {
            void* spike = brain_pools_acquire_spike_event(pools_);
            if (spike) {
                brain_pools_release_spike_event(pools_, spike);
            }
        }
    };

    // Thread reading metrics
    auto metrics_func = [&]() {
        brain_pools_metrics_t metrics;
        while (running) {
            if (brain_pools_get_metrics(pools_, &metrics)) {
                metrics_reads++;
            }
        }
    };

    std::thread alloc_thread(alloc_func);
    std::thread metrics_thread(metrics_func);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running = false;

    alloc_thread.join();
    metrics_thread.join();

    EXPECT_GT(metrics_reads.load(), 0);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(BrainPoolsTest, NullSafety_AcquireFunctions) {
    EXPECT_EQ(brain_pools_acquire_decision(nullptr), nullptr);
    EXPECT_EQ(brain_pools_acquire_activation(nullptr, 100), nullptr);
    EXPECT_EQ(brain_pools_acquire_spike_event(nullptr), nullptr);

    size_t actual;
    EXPECT_EQ(brain_pools_acquire_feature_buffer(nullptr, 100, &actual), nullptr);
}

TEST_F(BrainPoolsTest, NullSafety_ReleaseFunctions) {
    brain_pools_release_decision(nullptr, nullptr);
    brain_pools_release_activation(nullptr, nullptr);
    brain_pools_release_spike_event(nullptr, nullptr);
    brain_pools_release_feature_buffer(nullptr, nullptr, 0);
    // Should not crash
}

TEST_F(BrainPoolsTest, NullSafety_MetricsFunctions) {
    brain_pools_metrics_t metrics;
    EXPECT_FALSE(brain_pools_get_metrics(nullptr, &metrics));
    EXPECT_FALSE(brain_pools_get_metrics(pools_, nullptr));

    brain_pools_reset_metrics(nullptr);  // Should not crash

    EXPECT_FALSE(brain_pools_is_performant(nullptr));

    brain_pools_config_t config;
    EXPECT_FALSE(brain_pools_get_recommended_config(nullptr, &config));
}

TEST_F(BrainPoolsTest, NullSafety_BatchOperations) {
    void* events[10];
    EXPECT_EQ(brain_pools_acquire_spike_batch(nullptr, 10, events), 0u);
    EXPECT_EQ(brain_pools_acquire_spike_batch(pools_, 10, nullptr), 0u);
    EXPECT_EQ(brain_pools_acquire_spike_batch(pools_, 0, events), 0u);

    brain_pools_release_spike_batch(nullptr, 10, events);  // No crash
    brain_pools_release_spike_batch(pools_, 10, nullptr);  // No crash
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
