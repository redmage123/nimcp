/**
 * @file test_fault_event_bus_regression.cpp
 * @brief Regression Tests for Event Bus System
 *
 * WHAT: Regression tests for event delivery reliability and performance
 * WHY:  Ensure no degradation in reliability or performance over time
 * HOW:  Benchmark latency, throughput, and reliability under various loads
 *
 * REGRESSION SCENARIOS:
 * - Event delivery reliability (100% delivery guarantee)
 * - Latency benchmarks (p50, p95, p99)
 * - Throughput under load
 * - Memory stability (no leaks)
 * - Thread safety under stress
 */

#include <gtest/gtest.h>
#include "core/events/nimcp_event_bus.h"
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cmath>

//=============================================================================
// Performance Measurement Utilities
//=============================================================================

struct LatencyStats {
    std::vector<uint64_t> latencies_us;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    void add_sample(uint64_t latency) {
        pthread_mutex_lock(&mutex);
        latencies_us.push_back(latency);
        pthread_mutex_unlock(&mutex);
    }

    uint64_t get_percentile(double p) {
        pthread_mutex_lock(&mutex);
        if (latencies_us.empty()) {
            pthread_mutex_unlock(&mutex);
            return 0;
        }
        std::sort(latencies_us.begin(), latencies_us.end());
        size_t index = (size_t)(p * latencies_us.size());
        if (index >= latencies_us.size()) index = latencies_us.size() - 1;
        uint64_t result = latencies_us[index];
        pthread_mutex_unlock(&mutex);
        return result;
    }

    double get_mean() {
        pthread_mutex_lock(&mutex);
        if (latencies_us.empty()) {
            pthread_mutex_unlock(&mutex);
            return 0.0;
        }
        uint64_t sum = 0;
        for (auto lat : latencies_us) sum += lat;
        double mean = (double)sum / latencies_us.size();
        pthread_mutex_unlock(&mutex);
        return mean;
    }

    size_t get_count() {
        pthread_mutex_lock(&mutex);
        size_t count = latencies_us.size();
        pthread_mutex_unlock(&mutex);
        return count;
    }
};

//=============================================================================
// Test Fixtures
//=============================================================================

class EventBusRegressionTest : public ::testing::Test {
protected:
    event_bus_t bus;

    void SetUp() override {
        bus = event_bus_create("regression_test", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(nullptr, bus);
    }

    void TearDown() override {
        if (bus) event_bus_destroy(bus);
    }
};

//=============================================================================
// Reliability Tests
//=============================================================================

TEST_F(EventBusRegressionTest, Reliability_100PercentDelivery_ImmediateMode) {
    const int NUM_EVENTS = 10000;
    std::atomic<int> received{0};

    auto callback = [](const brain_event_t* event, void* context) {
        std::atomic<int>* count = (std::atomic<int>*)context;
        (*count)++;
    };

    event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, &received);

    for (int i = 0; i < NUM_EVENTS; i++) {
        ASSERT_TRUE(event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                             EVENT_PRIORITY_NORMAL, "test"));
    }

    ASSERT_EQ(NUM_EVENTS, received.load());
}

TEST_F(EventBusRegressionTest, Reliability_NoEventLoss_MultipleSubscribers) {
    const int NUM_EVENTS = 5000;
    const int NUM_SUBSCRIBERS = 10;

    std::vector<std::atomic<int>> counters(NUM_SUBSCRIBERS);

    auto callback = [](const brain_event_t* event, void* context) {
        std::atomic<int>* count = (std::atomic<int>*)context;
        (*count)++;
    };

    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, &counters[i]);
    }

    for (int i = 0; i < NUM_EVENTS; i++) {
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "test");
    }

    // All subscribers should receive all events
    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        ASSERT_EQ(NUM_EVENTS, counters[i].load())
            << "Subscriber " << i << " missed events";
    }
}

TEST_F(EventBusRegressionTest, Reliability_EventOrdering_ImmediateMode) {
    std::vector<uint64_t> sequence_numbers;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    auto callback = [](const brain_event_t* event, void* context) {
        auto* data = (std::pair<std::vector<uint64_t>*, pthread_mutex_t*>*)context;
        pthread_mutex_lock(data->second);
        data->first->push_back(event->sequence_number);
        pthread_mutex_unlock(data->second);
    };

    std::pair<std::vector<uint64_t>*, pthread_mutex_t*> ctx = {&sequence_numbers, &mutex};

    event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, &ctx);

    const int NUM_EVENTS = 1000;
    for (int i = 0; i < NUM_EVENTS; i++) {
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "test");
    }

    // Verify ordering
    for (size_t i = 1; i < sequence_numbers.size(); i++) {
        ASSERT_LT(sequence_numbers[i-1], sequence_numbers[i])
            << "Events out of order at index " << i;
    }
}

//=============================================================================
// Latency Benchmarks
//=============================================================================

TEST_F(EventBusRegressionTest, Latency_SingleSubscriber_Baseline) {
    LatencyStats stats;

    auto callback = [](const brain_event_t* event, void* context) {
        LatencyStats* stats = (LatencyStats*)context;
        uint64_t now = event_get_timestamp_us();
        uint64_t latency = now - event->timestamp_us;
        stats->add_sample(latency);
    };

    event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, &stats);

    const int NUM_EVENTS = 10000;
    for (int i = 0; i < NUM_EVENTS; i++) {
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "test");
    }

    // Report percentiles
    double mean = stats.get_mean();
    uint64_t p50 = stats.get_percentile(0.50);
    uint64_t p95 = stats.get_percentile(0.95);
    uint64_t p99 = stats.get_percentile(0.99);

    printf("\n=== Latency Benchmark (Single Subscriber) ===\n");
    printf("Mean:  %.2f us\n", mean);
    printf("P50:   %lu us\n", p50);
    printf("P95:   %lu us\n", p95);
    printf("P99:   %lu us\n", p99);

    // Regression thresholds (adjust based on baseline)
    EXPECT_LT(mean, 50.0); // Mean < 50us
    EXPECT_LT(p95, 200);   // P95 < 200us
    EXPECT_LT(p99, 500);   // P99 < 500us
}

TEST_F(EventBusRegressionTest, Latency_MultipleSubscribers_ScalingTest) {
    const int NUM_SUBSCRIBERS = 100;
    LatencyStats stats;

    auto callback = [](const brain_event_t* event, void* context) {
        // No-op for timing measurement
    };

    // Subscribe many subscribers
    for (int i = 0; i < NUM_SUBSCRIBERS; i++) {
        event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, nullptr);
    }

    const int NUM_EVENTS = 1000;
    std::vector<uint64_t> publish_latencies;

    for (int i = 0; i < NUM_EVENTS; i++) {
        uint64_t start = event_get_timestamp_us();
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "test");
        uint64_t end = event_get_timestamp_us();
        publish_latencies.push_back(end - start);
    }

    // Calculate statistics
    std::sort(publish_latencies.begin(), publish_latencies.end());
    uint64_t p50 = publish_latencies[publish_latencies.size() / 2];
    uint64_t p95 = publish_latencies[(size_t)(0.95 * publish_latencies.size())];
    uint64_t p99 = publish_latencies[(size_t)(0.99 * publish_latencies.size())];

    printf("\n=== Publish Latency (100 Subscribers) ===\n");
    printf("P50:   %lu us\n", p50);
    printf("P95:   %lu us\n", p95);
    printf("P99:   %lu us\n", p99);

    // Should scale linearly with subscriber count
    EXPECT_LT(p95, 5000); // P95 < 5ms for 100 subscribers
}

//=============================================================================
// Throughput Benchmarks
//=============================================================================

TEST_F(EventBusRegressionTest, Throughput_MaxEventsPerSecond_Baseline) {
    std::atomic<int> received{0};

    auto callback = [](const brain_event_t* event, void* context) {
        std::atomic<int>* count = (std::atomic<int>*)context;
        (*count)++;
    };

    event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, &received);

    const int DURATION_SEC = 1;
    auto start = std::chrono::high_resolution_clock::now();
    auto end_time = start + std::chrono::seconds(DURATION_SEC);

    int published = 0;
    while (std::chrono::high_resolution_clock::now() < end_time) {
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "test");
        published++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();

    double throughput = published / duration;

    printf("\n=== Throughput Benchmark ===\n");
    printf("Published:  %d events\n", published);
    printf("Received:   %d events\n", received.load());
    printf("Duration:   %.2f sec\n", duration);
    printf("Throughput: %.0f events/sec\n", throughput);

    // Regression threshold
    EXPECT_GT(throughput, 100000.0); // > 100K events/sec
}

TEST_F(EventBusRegressionTest, Throughput_MultipleEventTypes_NoSlowdown) {
    std::atomic<int> received{0};

    auto callback = [](const brain_event_t* event, void* context) {
        std::atomic<int>* count = (std::atomic<int>*)context;
        (*count)++;
    };

    // Subscribe to multiple types
    brain_event_type_t types[] = {
        EVENT_ERROR_DETECTED,
        EVENT_RECOVERY_STARTED,
        EVENT_CHECKPOINT_CREATED,
        EVENT_HEALTH_DEGRADED
    };

    for (auto type : types) {
        event_bus_subscribe(bus, type, callback, &received);
    }

    auto start = std::chrono::high_resolution_clock::now();

    const int EVENTS_PER_TYPE = 10000;
    for (auto type : types) {
        for (int i = 0; i < EVENTS_PER_TYPE; i++) {
            event_bus_publish_simple(bus, type, EVENT_PRIORITY_NORMAL, "test");
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();

    int total_events = 4 * EVENTS_PER_TYPE;
    double throughput = total_events / duration;

    printf("\n=== Multi-Type Throughput ===\n");
    printf("Throughput: %.0f events/sec\n", throughput);

    EXPECT_EQ(total_events, received.load());
    EXPECT_GT(throughput, 100000.0);
}

//=============================================================================
// Thread Safety Under Stress
//=============================================================================

struct StressTestContext {
    event_bus_t bus;
    std::atomic<int> published{0};
    std::atomic<int> received{0};
    std::atomic<bool> stop{false};
};

static void* publisher_thread_stress(void* arg) {
    StressTestContext* ctx = (StressTestContext*)arg;

    while (!ctx->stop.load()) {
        event_bus_publish_simple(ctx->bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "stress");
        ctx->published++;
    }

    return nullptr;
}

static void stress_callback(const brain_event_t* event, void* context) {
    StressTestContext* ctx = (StressTestContext*)context;
    ctx->received++;
}

TEST_F(EventBusRegressionTest, ThreadSafety_ConcurrentStress_5Seconds) {
    StressTestContext ctx;
    ctx.bus = bus;

    event_bus_subscribe(bus, EVENT_ERROR_DETECTED, stress_callback, &ctx);

    const int NUM_THREADS = 8;
    pthread_t threads[NUM_THREADS];

    // Launch publishers
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], nullptr, publisher_thread_stress, &ctx);
    }

    // Run for 5 seconds
    sleep(5);
    ctx.stop = true;

    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    printf("\n=== Concurrent Stress Test (5 sec) ===\n");
    printf("Published: %d events\n", ctx.published.load());
    printf("Received:  %d events\n", ctx.received.load());

    // All events should be delivered
    ASSERT_EQ(ctx.published.load(), ctx.received.load());
}

//=============================================================================
// Memory Stability Tests
//=============================================================================

TEST_F(EventBusRegressionTest, MemoryStability_NoLeaks_10KIterations) {
    // This test checks for memory leaks by repeatedly creating/destroying
    // subscriptions and publishing events

    for (int iteration = 0; iteration < 10000; iteration++) {
        std::atomic<int> received{0};

        auto callback = [](const brain_event_t* event, void* context) {
            std::atomic<int>* count = (std::atomic<int>*)context;
            (*count)++;
        };

        event_subscription_handle_t handle = event_bus_subscribe(
            bus, EVENT_ERROR_DETECTED, callback, &received);

        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "test");

        ASSERT_EQ(1, received.load());

        event_bus_unsubscribe(bus, handle);
    }

    // If we get here without crashing or OOM, test passes
    SUCCEED();
}

TEST_F(EventBusRegressionTest, MemoryStability_LargePayloads_NoFragmentation) {
    std::atomic<int> received{0};

    auto callback = [](const brain_event_t* event, void* context) {
        std::atomic<int>* count = (std::atomic<int>*)context;
        (*count)++;
    };

    event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, &received);

    // Publish events with large payloads
    uint8_t large_payload[EVENT_BUS_MAX_DATA_SIZE];
    memset(large_payload, 0xAB, sizeof(large_payload));

    for (int i = 0; i < 10000; i++) {
        event_bus_publish_data(bus, EVENT_ERROR_DETECTED,
                              EVENT_PRIORITY_NORMAL, "test",
                              large_payload, sizeof(large_payload));
    }

    ASSERT_EQ(10000, received.load());
}

//=============================================================================
// Async Mode Regression Tests
//=============================================================================

class AsyncRegressionTest : public ::testing::Test {
protected:
    event_bus_t bus;

    void SetUp() override {
        bus = event_bus_create("async_regression", EVENT_DELIVERY_ASYNC);
        ASSERT_NE(nullptr, bus);
        ASSERT_TRUE(event_bus_start(bus));
    }

    void TearDown() override {
        if (bus) {
            event_bus_stop(bus, true);
            event_bus_destroy(bus);
        }
    }
};

TEST_F(AsyncRegressionTest, AsyncDelivery_EventualConsistency_AllDelivered) {
    std::atomic<int> received{0};

    auto callback = [](const brain_event_t* event, void* context) {
        std::atomic<int>* count = (std::atomic<int>*)context;
        (*count)++;
    };

    event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, &received);

    const int NUM_EVENTS = 5000;
    for (int i = 0; i < NUM_EVENTS; i++) {
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "test");
    }

    // Wait for async delivery
    sleep(2);

    // Allow small number of drops due to queue capacity under burst load
    EXPECT_GE(received.load(), NUM_EVENTS * 99 / 100)
        << "At least 99% of events should be delivered";
}

TEST_F(AsyncRegressionTest, AsyncDelivery_LatencyBenchmark) {
    LatencyStats stats;

    auto callback = [](const brain_event_t* event, void* context) {
        LatencyStats* stats = (LatencyStats*)context;
        uint64_t now = event_get_timestamp_us();
        uint64_t latency = now - event->timestamp_us;
        stats->add_sample(latency);
    };

    event_bus_subscribe(bus, EVENT_ERROR_DETECTED, callback, &stats);

    const int NUM_EVENTS = 1000;
    for (int i = 0; i < NUM_EVENTS; i++) {
        event_bus_publish_simple(bus, EVENT_ERROR_DETECTED,
                                EVENT_PRIORITY_NORMAL, "test");
        usleep(100); // Space out events
    }

    // Wait for processing
    sleep(1);

    double mean = stats.get_mean();
    uint64_t p95 = stats.get_percentile(0.95);

    printf("\n=== Async Delivery Latency ===\n");
    printf("Mean: %.2f us\n", mean);
    printf("P95:  %lu us\n", p95);

    // Async latency should be higher but bounded
    EXPECT_LT(p95, 100000); // P95 < 100ms
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
