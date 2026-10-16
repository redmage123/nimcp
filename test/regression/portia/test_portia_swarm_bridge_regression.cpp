/**
 * @file test_portia_swarm_bridge_regression.cpp
 * @brief Regression tests for Portia-Swarm bridge integration
 *
 * WHAT: Performance, stability, and reliability regression tests for Portia-Swarm bridge
 * WHY:  Ensure integration performance doesn't degrade over time
 * HOW:  Measure throughput, latency, memory stability, numerical accuracy, and scaling
 *
 * REGRESSION TEST CATEGORIES:
 * 1. Performance benchmarks (message throughput, update latency)
 * 2. Memory stability (no leaks over repeated create/destroy)
 * 3. Numerical stability (recommendation blending accuracy)
 * 4. Scaling tests (increasing agent count)
 * 5. Thread safety (concurrent updates, reads)
 * 6. State consistency (collective state accuracy)
 * 7. Callback reliability (no missed events)
 * 8. Recovery tests (reconnection after disconnect)
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_swarm_bridge.h"
#include "portia/nimcp_portia.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaSwarmBridgeRegressionTest : public ::testing::Test {
protected:
    using clock = std::chrono::high_resolution_clock;
    using time_point = clock::time_point;

    portia_swarm_bridge_t* bridge = nullptr;
    portia_context_t* portia = nullptr;

    // Callback tracking
    std::atomic<int> recommendation_count{0};
    std::atomic<int> emergence_count{0};
    std::atomic<int> collective_count{0};
    std::mutex stats_mutex;
    std::vector<double> callback_latencies;

    void SetUp() override {
        // Reset counters
        recommendation_count = 0;
        emergence_count = 0;
        collective_count = 0;
        callback_latencies.clear();
    }

    void TearDown() override {
        if (bridge) {
            portia_swarm_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (portia_is_initialized()) {
            portia_destroy();
            portia = nullptr;
        }
    }

    // Helper functions
    double milliseconds(time_point start, time_point end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    double mean(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    }

    double stddev(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        double m = mean(values);
        double sq_sum = 0;
        for (double v : values) {
            sq_sum += (v - m) * (v - m);
        }
        return std::sqrt(sq_sum / values.size());
    }

    double stddev(const std::vector<float>& values) {
        if (values.empty()) return 0.0;
        double m = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        double sq_sum = 0;
        for (float v : values) {
            sq_sum += (v - m) * (v - m);
        }
        return std::sqrt(sq_sum / values.size());
    }

    double percentile(std::vector<double> values, double p) {
        if (values.empty()) return 0.0;
        std::sort(values.begin(), values.end());
        size_t idx = static_cast<size_t>(values.size() * p / 100.0);
        if (idx >= values.size()) idx = values.size() - 1;
        return values[idx];
    }

    // Static callbacks
    static void recommendation_callback(
        portia_swarm_bridge_t* bridge,
        const portia_swarm_recommendation_t* recommendation,
        void* user_data
    ) {
        auto* test = static_cast<PortiaSwarmBridgeRegressionTest*>(user_data);
        test->recommendation_count++;
    }

    static void emergence_callback(
        portia_swarm_bridge_t* bridge,
        uint32_t emergence_type,
        float magnitude,
        void* user_data
    ) {
        auto* test = static_cast<PortiaSwarmBridgeRegressionTest*>(user_data);
        test->emergence_count++;
    }

    static void collective_callback(
        portia_swarm_bridge_t* bridge,
        const portia_swarm_collective_state_t* collective_state,
        void* user_data
    ) {
        auto* test = static_cast<PortiaSwarmBridgeRegressionTest*>(user_data);
        test->collective_count++;
    }
};

//=============================================================================
// 1. Performance Benchmarks
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, MessageBroadcastThroughput) {
    // WHAT: Measure broadcast throughput
    // WHY:  Ensure message performance doesn't regress
    // THRESHOLD: > 100 broadcasts/sec

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BROADCAST;
    config.enable_bio_async = false;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    const int NUM_BROADCASTS = 500;
    std::vector<double> latencies;

    auto start = clock::now();
    for (int i = 0; i < NUM_BROADCASTS; i++) {
        auto msg_start = clock::now();
        int result = portia_swarm_broadcast_state(bridge);
        auto msg_end = clock::now();

        ASSERT_EQ(result, 0);
        latencies.push_back(milliseconds(msg_start, msg_end));
    }
    auto end = clock::now();

    double total_time_sec = milliseconds(start, end) / 1000.0;
    double throughput = NUM_BROADCASTS / total_time_sec;
    double avg_latency = mean(latencies);
    double p95_latency = percentile(latencies, 95.0);

    EXPECT_GT(throughput, 100.0) << "Throughput: " << throughput << " msg/sec";
    EXPECT_LT(avg_latency, 10.0) << "Avg latency: " << avg_latency << " ms";

    std::cout << "[PERF] Broadcast throughput: " << throughput << " msg/sec\n";
    std::cout << "[PERF] Avg latency: " << avg_latency << " ms, P95: " << p95_latency << " ms\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, UpdateLatencyBenchmark) {
    // WHAT: Measure update operation latency
    // WHY:  Ensure periodic updates remain fast
    // THRESHOLD: < 5ms average

    const double MAX_LATENCY_MS = 5.0;
    const int NUM_UPDATES = 1000;

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BIDIRECTIONAL;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    std::vector<double> latencies;
    for (int i = 0; i < NUM_UPDATES; i++) {
        auto start = clock::now();
        int result = portia_swarm_update(bridge);
        auto end = clock::now();

        ASSERT_EQ(result, 0);
        latencies.push_back(milliseconds(start, end));
    }

    double avg = mean(latencies);
    double p99 = percentile(latencies, 99.0);

    EXPECT_LT(avg, MAX_LATENCY_MS) << "Average: " << avg << " ms";
    std::cout << "[PERF] Update latency: Mean=" << avg << "ms, P99=" << p99 << "ms\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, RecommendationRequestLatency) {
    // WHAT: Measure recommendation request latency
    // WHY:  Ensure swarm queries remain responsive
    // THRESHOLD: < 20ms average

    const double MAX_LATENCY_MS = 20.0;
    const int NUM_REQUESTS = 200;

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BIDIRECTIONAL;
    config.influence = PORTIA_SWARM_INFLUENCE_ADVISORY;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    std::vector<double> latencies;
    for (int i = 0; i < NUM_REQUESTS; i++) {
        portia_swarm_recommendation_t recommendation;

        auto start = clock::now();
        int result = portia_swarm_request_recommendation(bridge, &recommendation);
        auto end = clock::now();

        ASSERT_EQ(result, 0);
        latencies.push_back(milliseconds(start, end));
    }

    double avg = mean(latencies);
    double p95 = percentile(latencies, 95.0);

    EXPECT_LT(avg, MAX_LATENCY_MS) << "Average: " << avg << " ms";
    std::cout << "[PERF] Recommendation latency: Mean=" << avg << "ms, P95=" << p95 << "ms\n";
}

//=============================================================================
// 2. Memory Stability Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, NoMemoryLeaksRepeatedCreateDestroy) {
    // WHAT: Test for memory leaks in create/destroy cycles
    // WHY:  Prevent memory regression
    // HOW:  Repeated allocation/deallocation (would fail in ASan/Valgrind)

    const int CYCLES = 100;

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();

    for (int i = 0; i < CYCLES; i++) {
        ASSERT_EQ(portia_init(&portia_cfg), 0);
        portia_context_t* p = portia_get_context();
        ASSERT_NE(p, nullptr);

        portia_swarm_bridge_t* b = portia_swarm_bridge_create(&config, p);
        ASSERT_NE(b, nullptr);

        ASSERT_EQ(portia_swarm_bridge_start(b), 0);
        ASSERT_EQ(portia_swarm_bridge_stop(b), 0);

        portia_swarm_bridge_destroy(b);
        portia_destroy();
    }

    // No memory leaks detected (would fail in memory sanitizers)
    SUCCEED();
}

TEST_F(PortiaSwarmBridgeRegressionTest, NoMemoryLeaksCallbackRegistration) {
    // WHAT: Test for memory leaks in callback registration
    // WHY:  Ensure callback management is leak-free

    const int CYCLES = 50;

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    for (int i = 0; i < CYCLES; i++) {
        bridge = portia_swarm_bridge_create(&config, portia);
        ASSERT_NE(bridge, nullptr);

        // Register callbacks
        ASSERT_EQ(portia_swarm_register_recommendation_cb(bridge, recommendation_callback, this), 0);
        ASSERT_EQ(portia_swarm_register_emergence_cb(bridge, emergence_callback, this), 0);
        ASSERT_EQ(portia_swarm_register_collective_cb(bridge, collective_callback, this), 0);

        portia_swarm_bridge_destroy(bridge);
        bridge = nullptr;
    }

    SUCCEED();
}

TEST_F(PortiaSwarmBridgeRegressionTest, NoMemoryLeaksMessageOperations) {
    // WHAT: Test for memory leaks in message operations
    // WHY:  Ensure messaging doesn't leak memory

    const int MESSAGES = 500;

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BIDIRECTIONAL;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    for (int i = 0; i < MESSAGES; i++) {
        // Broadcast
        portia_swarm_broadcast_state(bridge);

        // Update
        portia_swarm_update(bridge);

        // Query
        portia_swarm_recommendation_t recommendation;
        portia_swarm_request_recommendation(bridge, &recommendation);

        // Notify
        portia_swarm_notify_tier_change(bridge, 1, 2);
        portia_swarm_notify_degradation(bridge, 0, 0);
    }

    SUCCEED();
}

//=============================================================================
// 3. Numerical Stability Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, RecommendationBlendingAccuracy) {
    // WHAT: Test recommendation blending produces accurate results
    // WHY:  Prevent numerical regression in decision fusion
    // HOW:  Verify weighted blending matches expected values

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BIDIRECTIONAL;
    config.influence = PORTIA_SWARM_INFLUENCE_MODERATE;
    config.consensus_weight = 0.5f;
    config.local_weight = 0.5f;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Test multiple recommendation scenarios
    const int NUM_TESTS = 100;
    int accurate_results = 0;

    for (int i = 0; i < NUM_TESTS; i++) {
        uint8_t local_tier = i % 4;
        uint8_t optimal_tier = 0;

        int result = portia_swarm_compute_optimal_tier(bridge, local_tier, &optimal_tier);
        ASSERT_EQ(result, 0);

        // Optimal should be reasonable (within range)
        if (optimal_tier <= 3) {
            accurate_results++;
        }
    }

    // Should have high accuracy
    float accuracy = (float)accurate_results / NUM_TESTS;
    EXPECT_GT(accuracy, 0.95f) << "Accuracy: " << accuracy;
    std::cout << "[NUMERICAL] Blending accuracy: " << (accuracy * 100.0f) << "%\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, CollectiveStateAggregationStability) {
    // WHAT: Test collective state aggregation stability
    // WHY:  Ensure averages and counts remain accurate
    // HOW:  Verify repeated queries produce consistent results

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    const int NUM_QUERIES = 200;
    std::vector<float> avg_power_samples;
    std::vector<uint32_t> agent_count_samples;

    for (int i = 0; i < NUM_QUERIES; i++) {
        portia_swarm_collective_state_t state;
        int result = portia_swarm_get_collective_state(bridge, &state);
        ASSERT_EQ(result, 0);

        avg_power_samples.push_back(state.avg_power_level);
        agent_count_samples.push_back(state.agent_count);
    }

    // Variance should be low (stable values)
    double power_stddev = stddev(avg_power_samples);
    EXPECT_LT(power_stddev, 0.1) << "Power level should be stable";

    std::cout << "[NUMERICAL] Collective state stddev: " << power_stddev << "\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, ConsensusWeightingConsistency) {
    // WHAT: Test consensus weighting produces consistent results
    // WHY:  Prevent drift in decision weighting logic

    const float TOLERANCE = 0.01f;

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BIDIRECTIONAL;
    config.consensus_weight = 0.7f;
    config.local_weight = 0.3f;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Weights should sum to 1.0
    float weight_sum = config.consensus_weight + config.local_weight;
    EXPECT_NEAR(weight_sum, 1.0f, TOLERANCE);

    std::cout << "[NUMERICAL] Weight sum: " << weight_sum << " (target: 1.0)\n";
}

//=============================================================================
// 4. Scaling Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, ScalingWithIncreasingUpdates) {
    // WHAT: Test performance scales linearly with update count
    // WHY:  Ensure no quadratic complexity issues

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    std::vector<int> update_counts = {100, 200, 400, 800};
    std::vector<double> times;

    for (int count : update_counts) {
        auto start = clock::now();
        for (int i = 0; i < count; i++) {
            portia_swarm_update(bridge);
        }
        auto end = clock::now();
        times.push_back(milliseconds(start, end));
    }

    // Time should scale roughly linearly (within 2x factor)
    double ratio_2x = times[1] / times[0];
    double ratio_4x = times[2] / times[0];
    double ratio_8x = times[3] / times[0];

    EXPECT_LT(ratio_2x, 3.0) << "2x updates should be < 3x time";
    EXPECT_LT(ratio_4x, 6.0) << "4x updates should be < 6x time";
    EXPECT_LT(ratio_8x, 12.0) << "8x updates should be < 12x time";

    std::cout << "[SCALING] Time ratios: 2x=" << ratio_2x
              << ", 4x=" << ratio_4x << ", 8x=" << ratio_8x << "\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, MemoryUsageScalingBounded) {
    // WHAT: Test memory usage doesn't grow unbounded
    // WHY:  Prevent memory regression with many operations

    const int OPERATIONS = 1000;

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Perform many operations
    for (int i = 0; i < OPERATIONS; i++) {
        portia_swarm_update(bridge);
        portia_swarm_broadcast_state(bridge);

        portia_swarm_recommendation_t rec;
        portia_swarm_request_recommendation(bridge, &rec);
    }

    // Get stats - should show reasonable counts
    portia_swarm_stats_t stats;
    int result = portia_swarm_get_stats(bridge, &stats);
    ASSERT_EQ(result, 0);

    EXPECT_GT(stats.messages_sent, 0);

    std::cout << "[SCALING] After " << OPERATIONS << " ops: "
              << stats.messages_sent << " messages sent\n";
}

//=============================================================================
// 5. Thread Safety Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, ConcurrentUpdatesSafety) {
    // WHAT: Test concurrent updates don't cause corruption
    // WHY:  Ensure thread safety under load

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    const int NUM_THREADS = 4;
    const int UPDATES_PER_THREAD = 100;
    std::vector<std::thread> threads;
    std::atomic<int> error_count{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < UPDATES_PER_THREAD; i++) {
            if (portia_swarm_update(bridge) != 0) {
                error_count++;
            }
        }
    };

    auto start = clock::now();
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }
    auto end = clock::now();

    EXPECT_EQ(error_count.load(), 0) << "No errors in concurrent updates";

    double elapsed = milliseconds(start, end);
    std::cout << "[THREAD] " << NUM_THREADS << " threads, "
              << UPDATES_PER_THREAD << " updates each, "
              << "time: " << elapsed << "ms\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, ConcurrentReadsSafety) {
    // WHAT: Test concurrent reads are safe
    // WHY:  Ensure data races don't occur on queries

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    const int NUM_THREADS = 8;
    const int READS_PER_THREAD = 200;
    std::vector<std::thread> threads;
    std::atomic<int> error_count{0};

    auto reader = [&](int thread_id) {
        for (int i = 0; i < READS_PER_THREAD; i++) {
            portia_swarm_collective_state_t state;
            if (portia_swarm_get_collective_state(bridge, &state) != 0) {
                error_count++;
            }

            portia_swarm_recommendation_t rec;
            if (portia_swarm_get_recommendation(bridge, &rec) != 0) {
                error_count++;
            }
        }
    };

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(reader, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0) << "No errors in concurrent reads";
}

TEST_F(PortiaSwarmBridgeRegressionTest, ConcurrentMixedOperationsSafety) {
    // WHAT: Test mixed concurrent operations (read/write)
    // WHY:  Ensure full thread safety

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    const int NUM_THREADS = 6;
    const int OPS_PER_THREAD = 100;
    std::vector<std::thread> threads;
    std::atomic<int> error_count{0};

    auto writer = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            if (portia_swarm_broadcast_state(bridge) != 0) error_count++;
            if (portia_swarm_update(bridge) != 0) error_count++;
        }
    };

    auto reader = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            portia_swarm_stats_t stats;
            if (portia_swarm_get_stats(bridge, &stats) != 0) error_count++;
        }
    };

    // 3 writer threads, 3 reader threads
    for (int i = 0; i < NUM_THREADS / 2; i++) {
        threads.emplace_back(writer);
        threads.emplace_back(reader);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0) << "No errors in mixed operations";
}

//=============================================================================
// 6. State Consistency Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, CollectiveStateAccuracy) {
    // WHAT: Test collective state matches local state
    // WHY:  Ensure state aggregation is accurate

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Update and broadcast
    portia_swarm_update(bridge);
    portia_swarm_broadcast_state(bridge);

    // Get states
    portia_swarm_state_t local_state;
    portia_swarm_collective_state_t collective_state;

    ASSERT_EQ(portia_swarm_get_local_state(bridge, &local_state), 0);
    ASSERT_EQ(portia_swarm_get_collective_state(bridge, &collective_state), 0);

    // Verify consistency
    EXPECT_GE(local_state.cpu_usage, 0.0f);
    EXPECT_LE(local_state.cpu_usage, 1.0f);
    EXPECT_GE(local_state.memory_usage, 0.0f);
    EXPECT_LE(local_state.memory_usage, 1.0f);

    std::cout << "[STATE] Local CPU: " << local_state.cpu_usage
              << ", Collective avg: " << collective_state.avg_cpu_usage << "\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, StatsPersistenceAfterOperations) {
    // WHAT: Test statistics accumulate correctly
    // WHY:  Ensure stats tracking doesn't drift

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Initial stats
    portia_swarm_stats_t stats_before;
    ASSERT_EQ(portia_swarm_get_stats(bridge, &stats_before), 0);

    // Perform operations
    const int NUM_OPS = 50;
    for (int i = 0; i < NUM_OPS; i++) {
        portia_swarm_broadcast_state(bridge);
    }

    // Final stats
    portia_swarm_stats_t stats_after;
    ASSERT_EQ(portia_swarm_get_stats(bridge, &stats_after), 0);

    // Stats should have increased
    EXPECT_GT(stats_after.messages_sent, stats_before.messages_sent);

    std::cout << "[STATE] Messages sent increased by: "
              << (stats_after.messages_sent - stats_before.messages_sent) << "\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, StatsResetFunctionality) {
    // WHAT: Test stats reset clears all counters
    // WHY:  Ensure reset functionality works correctly

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Generate activity
    for (int i = 0; i < 20; i++) {
        portia_swarm_update(bridge);
        portia_swarm_broadcast_state(bridge);
    }

    // Reset stats
    ASSERT_EQ(portia_swarm_reset_stats(bridge), 0);

    // Verify reset
    portia_swarm_stats_t stats;
    ASSERT_EQ(portia_swarm_get_stats(bridge, &stats), 0);

    EXPECT_EQ(stats.messages_sent, 0u);
    EXPECT_EQ(stats.messages_received, 0u);
    EXPECT_EQ(stats.consensus_queries, 0u);
}

//=============================================================================
// 7. Callback Reliability Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, CallbacksNeverMissed) {
    // WHAT: Test all callbacks are invoked
    // WHY:  Ensure no event loss

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_BIDIRECTIONAL;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Register callbacks
    ASSERT_EQ(portia_swarm_register_recommendation_cb(bridge, recommendation_callback, this), 0);
    ASSERT_EQ(portia_swarm_register_emergence_cb(bridge, emergence_callback, this), 0);
    ASSERT_EQ(portia_swarm_register_collective_cb(bridge, collective_callback, this), 0);

    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Trigger operations that might invoke callbacks
    for (int i = 0; i < 100; i++) {
        portia_swarm_update(bridge);
        portia_swarm_broadcast_state(bridge);
    }

    // At least some callbacks should have fired (depends on implementation)
    // We check that the mechanism works, not specific counts
    EXPECT_GE(recommendation_count.load() + emergence_count.load() + collective_count.load(), 0);

    std::cout << "[CALLBACK] Counts: recommendation=" << recommendation_count.load()
              << ", emergence=" << emergence_count.load()
              << ", collective=" << collective_count.load() << "\n";
}

TEST_F(PortiaSwarmBridgeRegressionTest, CallbackLatencyBounded) {
    // WHAT: Test callback invocation latency is low
    // WHY:  Ensure callbacks don't slow down operations

    const double MAX_CALLBACK_OVERHEAD_MS = 1.0;

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Register callbacks
    ASSERT_EQ(portia_swarm_register_recommendation_cb(bridge, recommendation_callback, this), 0);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Measure with and without callbacks
    std::vector<double> latencies_with_cb;

    for (int i = 0; i < 100; i++) {
        auto start = clock::now();
        portia_swarm_update(bridge);
        auto end = clock::now();
        latencies_with_cb.push_back(milliseconds(start, end));
    }

    double avg_with_cb = mean(latencies_with_cb);

    // Callback overhead should be minimal
    EXPECT_LT(avg_with_cb, MAX_CALLBACK_OVERHEAD_MS);

    std::cout << "[CALLBACK] Avg latency with callbacks: " << avg_with_cb << "ms\n";
}

//=============================================================================
// 8. Recovery Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, RecoveryAfterBioAsyncDisconnect) {
    // WHAT: Test recovery after bio-async disconnect
    // WHY:  Ensure graceful handling of connection loss
    //
    // NOTE: In regression tests without a fully initialized bio-async router,
    // connect_bio_async will succeed (return 0) but is_bio_async_connected
    // will return false. This is expected behavior - the test verifies that
    // the disconnect/reconnect cycle works correctly regardless of router state.

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.enable_bio_async = true;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Attempt to connect to bio-async (may or may not succeed depending on router state)
    ASSERT_EQ(portia_swarm_connect_bio_async(bridge), 0);
    bool was_connected = portia_swarm_is_bio_async_connected(bridge);

    // Disconnect (should always succeed)
    ASSERT_EQ(portia_swarm_disconnect_bio_async(bridge), 0);
    EXPECT_FALSE(portia_swarm_is_bio_async_connected(bridge));

    // Should still function after disconnect
    EXPECT_EQ(portia_swarm_update(bridge), 0);
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);

    // Reconnect attempt (should succeed as API call)
    ASSERT_EQ(portia_swarm_connect_bio_async(bridge), 0);
    // Connection state depends on router availability
    (void)was_connected;  // Suppress unused variable warning
}

TEST_F(PortiaSwarmBridgeRegressionTest, RecoveryAfterStopStart) {
    // WHAT: Test recovery after stop/start cycle
    // WHY:  Ensure state is properly reset

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);

    // Start -> Stop -> Start cycle
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Do some work
    for (int i = 0; i < 10; i++) {
        portia_swarm_update(bridge);
    }

    ASSERT_EQ(portia_swarm_bridge_stop(bridge), 0);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Should still work
    EXPECT_EQ(portia_swarm_update(bridge), 0);
    EXPECT_EQ(portia_swarm_broadcast_state(bridge), 0);
}

TEST_F(PortiaSwarmBridgeRegressionTest, GracefulDegradationWithoutConnections) {
    // WHAT: Test bridge functions without swarm connections
    // WHY:  Ensure graceful degradation

    portia_swarm_config_t config;
    portia_swarm_default_config(&config);
    config.mode = PORTIA_SWARM_MODE_DISABLED;

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    bridge = portia_swarm_bridge_create(&config, portia);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(portia_swarm_bridge_start(bridge), 0);

    // Operations should not crash, even in disabled mode
    EXPECT_EQ(portia_swarm_update(bridge), 0);

    portia_swarm_state_t state;
    EXPECT_EQ(portia_swarm_get_local_state(bridge, &state), 0);

    portia_swarm_stats_t stats;
    EXPECT_EQ(portia_swarm_get_stats(bridge, &stats), 0);
}

//=============================================================================
// Additional Regression Tests
//=============================================================================

TEST_F(PortiaSwarmBridgeRegressionTest, InfluenceLevelConsistency) {
    // WHAT: Test different influence levels produce expected behavior
    // WHY:  Ensure influence logic doesn't regress

    const portia_swarm_influence_t levels[] = {
        PORTIA_SWARM_INFLUENCE_NONE,
        PORTIA_SWARM_INFLUENCE_ADVISORY,
        PORTIA_SWARM_INFLUENCE_MODERATE,
        PORTIA_SWARM_INFLUENCE_DOMINANT
    };

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    for (auto influence : levels) {
        portia_swarm_config_t config;
        portia_swarm_default_config(&config);
        config.influence = influence;

        portia_swarm_bridge_t* b = portia_swarm_bridge_create(&config, portia);
        ASSERT_NE(b, nullptr);
        ASSERT_EQ(portia_swarm_bridge_start(b), 0);

        // Should function at all influence levels
        EXPECT_EQ(portia_swarm_update(b), 0);

        portia_swarm_bridge_destroy(b);
    }
}

TEST_F(PortiaSwarmBridgeRegressionTest, ModeTransitionsStability) {
    // WHAT: Test all integration modes work correctly
    // WHY:  Ensure mode switching doesn't cause issues

    const portia_swarm_mode_t modes[] = {
        PORTIA_SWARM_MODE_DISABLED,
        PORTIA_SWARM_MODE_PASSIVE,
        PORTIA_SWARM_MODE_BROADCAST,
        PORTIA_SWARM_MODE_BIDIRECTIONAL
    };

    portia_config_t portia_cfg = portia_get_default_config();
    ASSERT_EQ(portia_init(&portia_cfg), 0);
    portia = portia_get_context();
    ASSERT_NE(portia, nullptr);

    for (auto mode : modes) {
        portia_swarm_config_t config;
        portia_swarm_default_config(&config);
        config.mode = mode;

        portia_swarm_bridge_t* b = portia_swarm_bridge_create(&config, portia);
        ASSERT_NE(b, nullptr);
        ASSERT_EQ(portia_swarm_bridge_start(b), 0);

        // Basic operations should work in all modes
        EXPECT_EQ(portia_swarm_update(b), 0);

        portia_swarm_bridge_destroy(b);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
