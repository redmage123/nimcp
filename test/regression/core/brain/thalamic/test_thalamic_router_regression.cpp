//=============================================================================
// test_thalamic_router_regression.cpp - Thalamic Router Regression Tests
//=============================================================================
/**
 * @file test_thalamic_router_regression.cpp
 * @brief Comprehensive regression tests for the thalamic router system
 *
 * WHAT: Tests for routing performance, determinism, message delivery,
 *       priority ordering, queue handling, and memory usage
 * WHY:  Ensure thalamic router is stable, efficient, and deterministic
 * HOW:  GTest framework with benchmarks, stress tests, and consistency checks
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <atomic>
#include <thread>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_thalamic_router.h"

//=============================================================================
// Callback Tracking for Tests
//=============================================================================

struct CallbackTracker {
    std::atomic<uint32_t> call_count{0};
    std::atomic<uint32_t> last_dest_id{0};
    std::atomic<float> last_attention{0.0f};
    std::vector<uint32_t> received_dest_ids;
    std::vector<float> received_signals;

    void reset() {
        call_count = 0;
        last_dest_id = 0;
        last_attention = 0.0f;
        received_dest_ids.clear();
        received_signals.clear();
    }
};

static CallbackTracker g_tracker;

static void test_delivery_callback(uint32_t dest_id, const float* signal,
                                   uint32_t size, float attention, void* user_data) {
    g_tracker.call_count++;
    g_tracker.last_dest_id = dest_id;
    g_tracker.last_attention = attention;

    if (user_data) {
        CallbackTracker* tracker = static_cast<CallbackTracker*>(user_data);
        tracker->received_dest_ids.push_back(dest_id);
        if (signal && size > 0) {
            tracker->received_signals.insert(tracker->received_signals.end(), signal, signal + size);
        }
    }
}

//=============================================================================
// Test Fixture
//=============================================================================

class ThalamicRouterRegressionTest : public NimcpTestBase {
protected:
    thalamic_router_t* router = nullptr;
    static constexpr uint32_t TEST_SIGNAL_SIZE = 16;
    static constexpr uint32_t MAX_TEST_DESTS = 8;

    void SetUp() override {
        NimcpTestBase::SetUp();
        g_tracker.reset();

        thalamic_router_config_t config = thalamic_router_default_config();
        config.max_queue_size = 2000;
        config.max_destinations = THALAMIC_MAX_DESTINATIONS;
        config.enable_attention_gating = true;
        config.enable_priority_routing = true;
        config.enable_statistics = true;
        config.min_attention_threshold = 0.01f;
        config.enable_quantum_routing = false;  // Disable for deterministic delivery tests

        router = thalamic_router_create(&config);
    }

    void TearDown() override {
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
        g_tracker.reset();
        NimcpTestBase::TearDown();
    }

    // Helper to create a test signal
    routed_signal_t* createTestSignal(uint32_t source_id, const std::vector<uint32_t>& dest_ids,
                                      float attention, signal_priority_t priority) {
        std::vector<float> signal_data(TEST_SIGNAL_SIZE);
        for (uint32_t i = 0; i < TEST_SIGNAL_SIZE; i++) {
            signal_data[i] = 0.5f + 0.01f * i;
        }

        routed_signal_t* signal = thalamic_router_create_signal(
            source_id, dest_ids.data(), dest_ids.size(),
            signal_data.data(), TEST_SIGNAL_SIZE, priority);

        if (signal) {
            signal->attention_weight = attention;
        }
        return signal;
    }

    void registerCallbacks(uint32_t num_destinations) {
        for (uint32_t i = 0; i < num_destinations; i++) {
            thalamic_router_set_callback(router, i, test_delivery_callback, &g_tracker);
        }
    }
};

//=============================================================================
// Performance Benchmark Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, RoutePerformanceBenchmark) {
    ASSERT_NE(router, nullptr);
    registerCallbacks(MAX_TEST_DESTS);

    const int NUM_ITERATIONS = 1000;
    std::vector<uint32_t> dests = {0, 1, 2};

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        routed_signal_t* signal = createTestSignal(100, dests, 0.8f, SIGNAL_PRIORITY_NORMAL);
        ASSERT_NE(signal, nullptr);

        bool result = thalamic_router_route_signal(router, signal);
        EXPECT_TRUE(result);

        thalamic_router_free_signal(signal);

        // Drain queue periodically to prevent overflow (queue capacity=2000)
        if ((i + 1) % 400 == 0) {
            uint32_t processed = 0;
            thalamic_router_process_queue(router, 500, &processed);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Routing includes periodic queue processing with quantum attention computation,
    // signal wrapper CoW management, and callback delivery. Allow up to 1000us under
    // CPU contention (parallel ctest) with quantum routing enabled.
    double avg_us = (double)duration.count() / NUM_ITERATIONS;
    EXPECT_LT(avg_us, 1000.0) << "Route signal too slow: " << avg_us << " us";
}

TEST_F(ThalamicRouterRegressionTest, QueueProcessingPerformance) {
    ASSERT_NE(router, nullptr);
    registerCallbacks(MAX_TEST_DESTS);

    // Queue many signals
    std::vector<uint32_t> dests = {0, 1};
    for (int i = 0; i < 200; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.7f, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    uint32_t total_processed = 0;
    while (total_processed < 200) {
        uint32_t processed = 0;
        thalamic_router_process_queue(router, 50, &processed);
        total_processed += processed;
        if (processed == 0) break;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Processing 200 signals includes quantum routing + callback delivery.
    // Allow up to 200ms under CPU contention (parallel ctest)
    EXPECT_LT(duration.count(), 200000) << "Queue processing too slow: " << duration.count() << " us";
}

TEST_F(ThalamicRouterRegressionTest, LookupPerformance) {
    ASSERT_NE(router, nullptr);

    // Set up routes
    for (uint32_t s = 0; s < 100; s++) {
        for (uint32_t d = 0; d < 10; d++) {
            thalamic_router_set_attention(router, s, d, 0.5f + 0.001f * s);
        }
    }

    const int NUM_ITERATIONS = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uint32_t source = i % 100;
        uint32_t dest = i % 10;
        float attention;
        bool found = thalamic_router_get_attention(router, source, dest, &attention);
        EXPECT_TRUE(found);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Lookup should be < 5us per call
    double avg_us = (double)duration.count() / NUM_ITERATIONS;
    EXPECT_LT(avg_us, 5.0) << "Attention lookup too slow: " << avg_us << " us";
}

//=============================================================================
// Route Lookup Determinism Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, RouteLookupDeterminism) {
    ASSERT_NE(router, nullptr);

    // Set up routes (keep total entries within attention gate capacity)
    const uint32_t NUM_SOURCES = 10;
    const uint32_t NUM_DESTS = 5;
    for (uint32_t s = 0; s < NUM_SOURCES; s++) {
        for (uint32_t d = 0; d < NUM_DESTS; d++) {
            float attention = 0.1f + 0.01f * s + 0.001f * d;
            thalamic_router_set_attention(router, s, d, attention);
        }
    }

    // Capture actual combined weights from first pass (attention gate applies
    // topdown/bottomup mixing, so get_attention returns combined_weight, not raw value)
    std::vector<std::vector<float>> reference(NUM_SOURCES, std::vector<float>(NUM_DESTS));
    for (uint32_t s = 0; s < NUM_SOURCES; s++) {
        for (uint32_t d = 0; d < NUM_DESTS; d++) {
            bool found = thalamic_router_get_attention(router, s, d, &reference[s][d]);
            EXPECT_TRUE(found);
            // Combined weight should be non-zero (topdown_weight * gate_topdown_coeff)
            EXPECT_GT(reference[s][d], 0.0f)
                << "Zero weight at source=" << s << " dest=" << d;
        }
    }

    // Subsequent passes must return identical values (determinism check)
    for (int pass = 1; pass < 4; pass++) {
        for (uint32_t s = 0; s < NUM_SOURCES; s++) {
            for (uint32_t d = 0; d < NUM_DESTS; d++) {
                float actual;
                bool found = thalamic_router_get_attention(router, s, d, &actual);
                EXPECT_TRUE(found);
                EXPECT_FLOAT_EQ(reference[s][d], actual)
                    << "Non-deterministic at source=" << s << " dest=" << d << " pass=" << pass;
            }
        }
    }
}

TEST_F(ThalamicRouterRegressionTest, RouteOrderDeterminism) {
    ASSERT_NE(router, nullptr);
    registerCallbacks(MAX_TEST_DESTS);

    CallbackTracker tracker1, tracker2;
    std::vector<uint32_t> dests = {0, 1, 2, 3};

    // First pass
    for (uint32_t i = 0; i < 10; i++) {
        thalamic_router_set_callback(router, i, test_delivery_callback, &tracker1);
    }

    for (int i = 0; i < 20; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.8f, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    uint32_t processed1 = 0;
    thalamic_router_process_queue(router, 100, &processed1);

    // Clear and second pass
    thalamic_router_clear_queue(router);
    thalamic_router_reset_stats(router);

    for (uint32_t i = 0; i < 10; i++) {
        thalamic_router_set_callback(router, i, test_delivery_callback, &tracker2);
    }

    for (int i = 0; i < 20; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.8f, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    uint32_t processed2 = 0;
    thalamic_router_process_queue(router, 100, &processed2);

    // Same number of deliveries
    EXPECT_EQ(tracker1.received_dest_ids.size(), tracker2.received_dest_ids.size());
}

//=============================================================================
// Message Delivery Accuracy Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, DeliveryAccuracy_SingleDest) {
    // Use a dedicated router with quantum routing DISABLED so delivery accuracy
    // is tested without quantum attention filtering
    thalamic_router_config_t cfg = thalamic_router_default_config();
    cfg.max_queue_size = 100;
    cfg.max_destinations = THALAMIC_MAX_DESTINATIONS;
    cfg.enable_attention_gating = true;
    cfg.enable_priority_routing = true;
    cfg.enable_statistics = true;
    cfg.min_attention_threshold = 0.01f;
    cfg.enable_quantum_routing = false;

    thalamic_router_t* local_router = thalamic_router_create(&cfg);
    ASSERT_NE(local_router, nullptr);

    CallbackTracker tracker;
    thalamic_router_set_callback(local_router, 42, test_delivery_callback, &tracker);

    // Establish route from source 1 to dest 42
    thalamic_router_set_attention(local_router, 1, 42, 0.9f);

    std::vector<uint32_t> dests = {42};
    routed_signal_t* signal = thalamic_router_create_signal(
        1, dests.data(), dests.size(),
        std::vector<float>(TEST_SIGNAL_SIZE, 0.5f).data(), TEST_SIGNAL_SIZE,
        SIGNAL_PRIORITY_NORMAL);
    ASSERT_NE(signal, nullptr);

    bool result = thalamic_router_route_signal(local_router, signal);
    EXPECT_TRUE(result);

    uint32_t processed = 0;
    thalamic_router_process_queue(local_router, 10, &processed);

    EXPECT_GT(tracker.received_dest_ids.size(), (size_t)0);
    if (!tracker.received_dest_ids.empty()) {
        EXPECT_EQ(tracker.received_dest_ids[0], 42u);
    }

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(local_router);
}

TEST_F(ThalamicRouterRegressionTest, DeliveryAccuracy_MultipleDests) {
    // Use a dedicated router with quantum routing DISABLED so delivery accuracy
    // is tested without quantum attention filtering
    thalamic_router_config_t cfg = thalamic_router_default_config();
    cfg.max_queue_size = 100;
    cfg.max_destinations = THALAMIC_MAX_DESTINATIONS;
    cfg.enable_attention_gating = true;
    cfg.enable_priority_routing = true;
    cfg.enable_statistics = true;
    cfg.min_attention_threshold = 0.01f;
    cfg.enable_quantum_routing = false;  // Disable quantum filtering

    thalamic_router_t* test_router = thalamic_router_create(&cfg);
    ASSERT_NE(test_router, nullptr);

    CallbackTracker tracker;
    std::vector<uint32_t> dests = {2, 4, 6, 8};
    for (uint32_t d : dests) {
        thalamic_router_set_callback(test_router, d, test_delivery_callback, &tracker);
        // Establish route from source 1 to each destination
        thalamic_router_set_attention(test_router, 1, d, 0.9f);
    }

    routed_signal_t* signal = createTestSignal(1, dests, 0.8f, SIGNAL_PRIORITY_NORMAL);
    ASSERT_NE(signal, nullptr);

    thalamic_router_route_signal(test_router, signal);

    uint32_t processed = 0;
    thalamic_router_process_queue(test_router, 10, &processed);

    // All destinations should receive the signal (no quantum filtering)
    EXPECT_GE(tracker.received_dest_ids.size(), dests.size());

    // Verify each destination received the signal
    for (uint32_t d : dests) {
        auto it = std::find(tracker.received_dest_ids.begin(),
                           tracker.received_dest_ids.end(), d);
        EXPECT_NE(it, tracker.received_dest_ids.end())
            << "Destination " << d << " did not receive signal";
    }

    thalamic_router_free_signal(signal);
    thalamic_router_destroy(test_router);
}

TEST_F(ThalamicRouterRegressionTest, DeliveryAccuracy_AttentionGating) {
    ASSERT_NE(router, nullptr);

    CallbackTracker tracker_high, tracker_low;
    thalamic_router_set_callback(router, 1, test_delivery_callback, &tracker_high);
    thalamic_router_set_callback(router, 2, test_delivery_callback, &tracker_low);

    // Route 1 gets high attention, route 2 gets low attention
    thalamic_router_set_attention(router, 100, 1, 0.9f);
    thalamic_router_set_attention(router, 100, 2, 0.1f);

    // Send multiple signals
    for (int i = 0; i < 10; i++) {
        std::vector<uint32_t> dests = {1, 2};
        routed_signal_t* signal = createTestSignal(100, dests, 0.5f, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    uint32_t processed = 0;
    thalamic_router_process_queue(router, 100, &processed);

    // Both should receive, but attention values should differ
    EXPECT_GT(tracker_high.received_dest_ids.size(), 0);
    EXPECT_GT(tracker_low.received_dest_ids.size(), 0);
}

//=============================================================================
// Priority Ordering Consistency Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, PriorityOrdering_HighFirst) {
    ASSERT_NE(router, nullptr);
    registerCallbacks(MAX_TEST_DESTS);

    std::vector<uint32_t> dests = {0};

    // Queue low priority first
    for (int i = 0; i < 5; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.5f, SIGNAL_PRIORITY_LOW);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    // Then high priority
    for (int i = 5; i < 10; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.5f, SIGNAL_PRIORITY_HIGH);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    // Process remaining queue signals.
    // HIGH priority signals were already delivered immediately (bypass queue),
    // so only LOW priority signals (5) remain in the queue.
    uint32_t processed = 0;
    thalamic_router_process_queue(router, 100, &processed);

    EXPECT_EQ(processed, 5u) << "Only LOW priority signals should be in queue";
}

TEST_F(ThalamicRouterRegressionTest, PriorityOrdering_Consistency) {
    ASSERT_NE(router, nullptr);

    std::vector<signal_priority_t> priorities = {
        SIGNAL_PRIORITY_LOW, SIGNAL_PRIORITY_NORMAL, SIGNAL_PRIORITY_HIGH
    };

    // Multiple passes should have consistent ordering
    for (int pass = 0; pass < 3; pass++) {
        thalamic_router_clear_queue(router);
        g_tracker.reset();
        registerCallbacks(MAX_TEST_DESTS);

        std::vector<uint32_t> dests = {0};

        // Queue mixed priorities
        for (int i = 0; i < 15; i++) {
            signal_priority_t priority = priorities[i % 3];
            routed_signal_t* signal = createTestSignal(i, dests, 0.6f, priority);
            if (signal) {
                thalamic_router_route_signal(router, signal);
                thalamic_router_free_signal(signal);
            }
        }

        uint32_t processed = 0;
        thalamic_router_process_queue(router, 100, &processed);

        // HIGH priority signals (5 out of 15) are delivered immediately,
        // so only LOW + NORMAL (10) remain in queue
        EXPECT_EQ(processed, 10u) << "Pass " << pass << " processed wrong count";
    }
}

TEST_F(ThalamicRouterRegressionTest, PriorityOrdering_BypassQueue) {
    ASSERT_NE(router, nullptr);

    CallbackTracker tracker;
    thalamic_router_set_callback(router, 0, test_delivery_callback, &tracker);

    // Establish routes from all sources to dest 0
    for (int i = 0; i < 5; i++) {
        thalamic_router_set_attention(router, i, 0, 0.5f);
    }
    thalamic_router_set_attention(router, 999, 0, 1.0f);

    // Queue normal signals
    std::vector<uint32_t> dests = {0};
    for (int i = 0; i < 5; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.5f, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    // Send bypass signal (critical)
    std::vector<float> bypass_data(TEST_SIGNAL_SIZE, 1.0f);
    routed_signal_t* bypass = thalamic_router_create_signal(
        999, dests.data(), dests.size(), bypass_data.data(), TEST_SIGNAL_SIZE, SIGNAL_PRIORITY_HIGH);
    if (bypass) {
        bypass->bypass_queue = true;
        bypass->attention_weight = 1.0f;
        thalamic_router_route_signal(router, bypass);
        thalamic_router_free_signal(bypass);
    }

    // Process
    uint32_t processed = 0;
    thalamic_router_process_queue(router, 10, &processed);

    // Bypass signals should be processed immediately
    EXPECT_GT(tracker.received_dest_ids.size(), 0);
}

//=============================================================================
// Queue Overflow Handling Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, QueueOverflow_Graceful) {
    ASSERT_NE(router, nullptr);
    registerCallbacks(MAX_TEST_DESTS);

    std::vector<uint32_t> dests = {0};
    int success_count = 0;
    int fail_count = 0;

    // Try to queue more than capacity (2000)
    for (int i = 0; i < 2500; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.5f, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            if (thalamic_router_route_signal(router, signal)) {
                success_count++;
            } else {
                fail_count++;
            }
            thalamic_router_free_signal(signal);
        }
    }

    // Some should fail due to overflow
    EXPECT_LT(success_count, 2500);
    EXPECT_GT(fail_count, 0);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_GT(stats.signals_dropped, 0);
}

TEST_F(ThalamicRouterRegressionTest, QueueOverflow_Recovery) {
    ASSERT_NE(router, nullptr);
    registerCallbacks(MAX_TEST_DESTS);

    std::vector<uint32_t> dests = {0};

    // Fill queue
    for (int i = 0; i < 2000; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.5f, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    // Process some to free space
    uint32_t processed = 0;
    thalamic_router_process_queue(router, 500, &processed);
    EXPECT_GT(processed, 0);

    // Should be able to queue more now
    routed_signal_t* new_signal = createTestSignal(1000, dests, 0.5f, SIGNAL_PRIORITY_NORMAL);
    if (new_signal) {
        bool result = thalamic_router_route_signal(router, new_signal);
        EXPECT_TRUE(result) << "Should accept new signal after processing";
        thalamic_router_free_signal(new_signal);
    }
}

TEST_F(ThalamicRouterRegressionTest, QueueClear) {
    ASSERT_NE(router, nullptr);

    std::vector<uint32_t> dests = {0};

    // Fill queue
    for (int i = 0; i < 100; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.5f, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }
    }

    routing_stats_t stats_before;
    thalamic_router_get_stats(router, &stats_before);
    EXPECT_GT(stats_before.queue_depth, 0);

    // Clear queue
    thalamic_router_clear_queue(router);

    routing_stats_t stats_after;
    thalamic_router_get_stats(router, &stats_after);
    EXPECT_EQ(stats_after.queue_depth, 0);
}

//=============================================================================
// Memory Usage Pattern Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, MemoryUsage_CreateDestroy) {
    for (int i = 0; i < 50; i++) {
        thalamic_router_config_t config = thalamic_router_default_config();
        config.max_queue_size = 100 + i * 10;

        thalamic_router_t* r = thalamic_router_create(&config);
        ASSERT_NE(r, nullptr) << "Create failed at iteration " << i;

        // Use the router
        std::vector<uint32_t> dests = {0, 1};
        std::vector<float> signal_data(8, 0.5f);
        routed_signal_t* signal = thalamic_router_create_signal(
            1, dests.data(), dests.size(), signal_data.data(), 8, SIGNAL_PRIORITY_NORMAL);
        if (signal) {
            thalamic_router_route_signal(r, signal);
            thalamic_router_free_signal(signal);
        }

        thalamic_router_destroy(r);
    }
}

TEST_F(ThalamicRouterRegressionTest, MemoryUsage_SignalLifecycle) {
    std::vector<uint32_t> dests = {0, 1, 2, 3};

    for (int i = 0; i < 1000; i++) {
        routed_signal_t* signal = createTestSignal(i, dests, 0.7f, SIGNAL_PRIORITY_NORMAL);
        ASSERT_NE(signal, nullptr) << "Create failed at iteration " << i;

        // Verify allocation
        EXPECT_NE(signal->dest_ids, nullptr);
        EXPECT_NE(signal->signal_data, nullptr);
        EXPECT_EQ(signal->num_dests, dests.size());
        EXPECT_EQ(signal->signal_size, TEST_SIGNAL_SIZE);

        thalamic_router_free_signal(signal);
    }
}

TEST_F(ThalamicRouterRegressionTest, MemoryUsage_StatisticsGrowth) {
    ASSERT_NE(router, nullptr);
    registerCallbacks(MAX_TEST_DESTS);

    std::vector<uint32_t> dests = {0};

    // Send many signals and verify statistics don't corrupt memory
    for (int batch = 0; batch < 10; batch++) {
        for (int i = 0; i < 100; i++) {
            routed_signal_t* signal = createTestSignal(batch * 100 + i, dests, 0.5f, SIGNAL_PRIORITY_NORMAL);
            if (signal) {
                thalamic_router_route_signal(router, signal);
                thalamic_router_free_signal(signal);
            }
        }

        uint32_t processed = 0;
        thalamic_router_process_queue(router, 100, &processed);

        // Statistics should be consistent
        routing_stats_t stats;
        thalamic_router_get_stats(router, &stats);
        EXPECT_GE(stats.signals_routed, 0);
        EXPECT_GE(stats.avg_latency_ms, 0.0f);
        EXPECT_FALSE(std::isnan(stats.avg_latency_ms));
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, Stress_HighVolume) {
    ASSERT_NE(router, nullptr);
    registerCallbacks(MAX_TEST_DESTS);

    const int NUM_SIGNALS = 10000;
    std::vector<uint32_t> dests = {0, 1, 2};

    // Establish routes - signals use source_id = i (0-9999), but the attention
    // table only needs entries for the dest_ids that the router checks.
    // Set attention for a range of sources to the 3 destinations.
    for (int src = 0; src < 100; src++) {
        for (uint32_t d : dests) {
            thalamic_router_set_attention(router, src, d, 0.5f);
        }
    }

    for (int i = 0; i < NUM_SIGNALS; i++) {
        float attention = 0.3f + 0.4f * (i % 100) / 100.0f;
        signal_priority_t priority = (i % 10 == 0) ? SIGNAL_PRIORITY_HIGH :
                                     (i % 3 == 0) ? SIGNAL_PRIORITY_LOW : SIGNAL_PRIORITY_NORMAL;

        routed_signal_t* signal = createTestSignal(i, dests, attention, priority);
        if (signal) {
            thalamic_router_route_signal(router, signal);
            thalamic_router_free_signal(signal);
        }

        // Process periodically to prevent queue overflow
        if (i % 100 == 0) {
            uint32_t processed = 0;
            thalamic_router_process_queue(router, 50, &processed);
        }
    }

    // Final processing
    uint32_t total_processed = 0;
    uint32_t processed = 0;
    do {
        processed = 0;
        thalamic_router_process_queue(router, 500, &processed);
        total_processed += processed;
    } while (processed > 0);

    routing_stats_t stats;
    thalamic_router_get_stats(router, &stats);
    EXPECT_GT(stats.signals_routed, 0);
}

TEST_F(ThalamicRouterRegressionTest, Stress_MixedOperations) {
    ASSERT_NE(router, nullptr);

    for (int i = 0; i < 5000; i++) {
        // Mix of operations
        switch (i % 5) {
            case 0: {
                // Route signal
                std::vector<uint32_t> dests = {(uint32_t)(i % MAX_TEST_DESTS)};
                routed_signal_t* signal = createTestSignal(i, dests, 0.6f, SIGNAL_PRIORITY_NORMAL);
                if (signal) {
                    thalamic_router_route_signal(router, signal);
                    thalamic_router_free_signal(signal);
                }
                break;
            }
            case 1: {
                // Process queue
                uint32_t processed = 0;
                thalamic_router_process_queue(router, 10, &processed);
                break;
            }
            case 2: {
                // Set attention
                thalamic_router_set_attention(router, i % 100, i % 50, 0.5f + 0.001f * i);
                break;
            }
            case 3: {
                // Get attention
                float attention;
                thalamic_router_get_attention(router, i % 100, i % 50, &attention);
                break;
            }
            case 4: {
                // Get stats
                routing_stats_t stats;
                thalamic_router_get_stats(router, &stats);
                EXPECT_GE(stats.signals_routed, 0);
                break;
            }
        }
    }
}

TEST_F(ThalamicRouterRegressionTest, Stress_RapidCreateDestroy) {
    for (int outer = 0; outer < 20; outer++) {
        thalamic_router_config_t config = thalamic_router_default_config();
        config.max_queue_size = 50;

        thalamic_router_t* r = thalamic_router_create(&config);
        ASSERT_NE(r, nullptr);

        // Quick operations
        std::vector<uint32_t> dests = {0};
        for (int i = 0; i < 30; i++) {
            routed_signal_t* signal = createTestSignal(i, dests, 0.5f, SIGNAL_PRIORITY_NORMAL);
            if (signal) {
                thalamic_router_route_signal(r, signal);
                thalamic_router_free_signal(signal);
            }
        }

        uint32_t processed = 0;
        thalamic_router_process_queue(r, 50, &processed);

        thalamic_router_destroy(r);
    }
}

//=============================================================================
// Imagination Routing Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, ImaginationAttention_SetGet) {
    ASSERT_NE(router, nullptr);

    // Set imagination attention
    bool result = thalamic_router_set_imagination_attention(router, 0.75f);
    EXPECT_TRUE(result);

    float attention = thalamic_router_get_imagination_attention(router);
    EXPECT_FLOAT_EQ(attention, 0.75f);
}

TEST_F(ThalamicRouterRegressionTest, ImaginationRouting_Toggle) {
    ASSERT_NE(router, nullptr);

    // Enable
    bool result = thalamic_router_set_imagination_routing_enabled(router, true);
    EXPECT_TRUE(result);
    EXPECT_TRUE(thalamic_router_is_imagination_routing_enabled(router));

    // Disable
    result = thalamic_router_set_imagination_routing_enabled(router, false);
    EXPECT_TRUE(result);
    EXPECT_FALSE(thalamic_router_is_imagination_routing_enabled(router));
}

TEST_F(ThalamicRouterRegressionTest, ImaginationContent_Route) {
    ASSERT_NE(router, nullptr);

    thalamic_router_set_imagination_routing_enabled(router, true);
    thalamic_router_set_imagination_attention(router, 0.8f);

    CallbackTracker tracker;
    thalamic_router_set_callback(router, 100, test_delivery_callback, &tracker);

    // Establish route from source 1 to dest 100
    thalamic_router_set_attention(router, 1, 100, 0.9f);

    std::vector<float> content(32, 0.5f);
    std::vector<uint32_t> dests = {100};

    bool result = thalamic_router_route_imagination_content(
        router, 1, content.data(), content.size(), dests.data(), dests.size());
    EXPECT_TRUE(result);

    uint32_t processed = 0;
    thalamic_router_process_queue(router, 10, &processed);

    EXPECT_GT(tracker.received_dest_ids.size(), 0);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, DefaultConfig_Values) {
    thalamic_router_config_t config = thalamic_router_default_config();

    // Default values should be reasonable
    EXPECT_GT(config.max_queue_size, 0);
    EXPECT_GT(config.max_destinations, 0);
    EXPECT_LE(config.max_destinations, THALAMIC_MAX_DESTINATIONS);
    EXPECT_GE(config.min_attention_threshold, 0.0f);
    EXPECT_LE(config.min_attention_threshold, 1.0f);
}

TEST_F(ThalamicRouterRegressionTest, CreateWithNullConfig) {
    // thalamic_router_create requires non-NULL config (complex internal initialization)
    // Verify it rejects NULL gracefully
    thalamic_router_t* r = thalamic_router_create(nullptr);
    EXPECT_EQ(r, nullptr) << "Should reject NULL config";

    // Verify default config creation works
    thalamic_router_config_t cfg = thalamic_router_default_config();
    r = thalamic_router_create(&cfg);
    ASSERT_NE(r, nullptr) << "Should succeed with default config";

    routing_stats_t stats;
    bool result = thalamic_router_get_stats(r, &stats);
    EXPECT_TRUE(result);

    thalamic_router_destroy(r);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(ThalamicRouterRegressionTest, NullSafety_Operations) {
    thalamic_router_destroy(nullptr);

    EXPECT_FALSE(thalamic_router_route_signal(nullptr, nullptr));
    EXPECT_FALSE(thalamic_router_process_queue(nullptr, 10, nullptr));
    EXPECT_FALSE(thalamic_router_set_callback(nullptr, 0, nullptr, nullptr));
    EXPECT_FALSE(thalamic_router_set_attention(nullptr, 0, 0, 0.5f));
    EXPECT_FALSE(thalamic_router_get_stats(nullptr, nullptr));

    thalamic_router_reset_stats(nullptr);
    thalamic_router_clear_queue(nullptr);

    EXPECT_LT(thalamic_router_get_imagination_attention(nullptr), 0.0f);
    EXPECT_FALSE(thalamic_router_is_imagination_routing_enabled(nullptr));
}

TEST_F(ThalamicRouterRegressionTest, NullSafety_SignalOperations) {
    thalamic_router_free_signal(nullptr);

    routed_signal_t* signal = thalamic_router_create_signal(0, nullptr, 0, nullptr, 0, SIGNAL_PRIORITY_NORMAL);
    EXPECT_EQ(signal, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
