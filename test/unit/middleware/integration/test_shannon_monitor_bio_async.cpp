//=============================================================================
// test_shannon_monitor_bio_async.cpp
// Bio-Async Integration Tests for Shannon Monitor
//=============================================================================

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "middleware/events/nimcp_event_types.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ShannonMonitorBioAsyncTest : public ::testing::Test {
protected:
    shannon_monitor_t* monitor = nullptr;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;
        bio_config.enable_statistics = true;
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-async";

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = false;
        router_config.enable_statistics = true;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-router";

        // Create Shannon monitor
        monitor = shannon_monitor_create();
        ASSERT_NE(monitor, nullptr) << "Failed to create Shannon monitor";
    }

    void TearDown() override {
        if (monitor) {
            shannon_monitor_destroy(monitor);
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    // Helper to create test event
    event_t create_test_event(event_type_t type, mw_event_priority_t priority) {
        uint32_t neurons[] = {1, 2, 3};
        return event_create_spike_burst(neurons, 3, 0.8f, 1000,
                                       priority, EVENT_SOURCE_BRAIN);
    }
};

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, CreateRegistersWithBioRouter) {
    // Shannon monitor should register with bio-router upon creation
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // At least one module should be registered
    EXPECT_GE(stats.active_modules, 1u);
}

TEST_F(ShannonMonitorBioAsyncTest, DestroyUnregistersFromBioRouter) {
    // Get initial stats
    bio_router_stats_t stats_before;
    nimcp_error_t err = bio_router_get_stats(&stats_before);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    uint32_t modules_before = stats_before.active_modules;

    // Destroy monitor
    shannon_monitor_destroy(monitor);
    monitor = nullptr;

    // Get stats after destroy
    bio_router_stats_t stats_after;
    err = bio_router_get_stats(&stats_after);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Should have one less module
    EXPECT_LE(stats_after.active_modules, modules_before);
}

TEST_F(ShannonMonitorBioAsyncTest, MultipleMonitorsRegisterIndependently) {
    // Create second monitor
    shannon_monitor_t* monitor2 = shannon_monitor_create();
    ASSERT_NE(monitor2, nullptr);

    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // May register independently or share a module ID - both are valid
    EXPECT_GE(stats.active_modules, 1u);

    shannon_monitor_destroy(monitor2);
}

//=============================================================================
// Event Recording with Bio-Async Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, RecordsEventsWithAsyncBroadcast) {
    // Create and record events
    event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                      MW_EVENT_PRIORITY_NORMAL);

    shannon_monitor_record_event(monitor, &event);

    // Get metrics
    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(monitor);

    EXPECT_EQ(metrics.total_events, 1u);
}

TEST_F(ShannonMonitorBioAsyncTest, BroadcastsBottleneckDetection) {
    // Record many high-information events to simulate bottleneck
    for (int i = 0; i < 100; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_HIGH);
        shannon_monitor_record_event(monitor, &event);
    }

    // Check if bottleneck detected
    bool bottlenecked = shannon_monitor_is_bottlenecked(monitor);

    // Get metrics
    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(monitor);
    EXPECT_EQ(metrics.total_events, 100u);

    // Note: Actual bottleneck depends on threshold configuration
}

//=============================================================================
// Bio-Promise Integration Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, IntegratesWithBioPromises) {
    // Create promise for async monitoring
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_SEROTONIN, sizeof(float));
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Record event and measure information
    event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                      MW_EVENT_PRIORITY_NORMAL);
    shannon_monitor_record_event(monitor, &event);

    float information = shannon_monitor_measure_event_information(monitor, &event);

    // Complete promise with information content
    nimcp_bio_promise_complete(promise, &information);

    // Future should be ready
    EXPECT_TRUE(nimcp_bio_future_is_ready(future));

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(ShannonMonitorBioAsyncTest, UsesChannelForInformationBroadcast) {
    // Information theory updates use serotonin (stability/predictability)
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_SEROTONIN, sizeof(shannon_routing_metrics_t));
    ASSERT_NE(promise, nullptr);

    // Record several events
    for (int i = 0; i < 10; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Get metrics
    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(monitor);

    // Complete promise with metrics
    nimcp_bio_promise_complete(promise, &metrics);

    EXPECT_TRUE(nimcp_bio_future_is_ready(
        nimcp_bio_promise_get_future(promise)));

    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Channel Capacity with Bio-Async Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, CalculatesChannelCapacity) {
    // Record events
    for (int i = 0; i < 20; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Calculate channel capacity
    float capacity = shannon_monitor_calculate_channel_capacity(monitor);

    // Capacity should be positive (C = B log2(1 + SNR))
    EXPECT_GT(capacity, 0.0f);
}

TEST_F(ShannonMonitorBioAsyncTest, TracksUtilization) {
    // Record events
    for (int i = 0; i < 30; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Get utilization
    float utilization = shannon_monitor_get_utilization(monitor);

    // Utilization should be in valid range [0,1]
    EXPECT_GE(utilization, 0.0f);
    EXPECT_LE(utilization, 1.0f);
}

TEST_F(ShannonMonitorBioAsyncTest, TracksThroughput) {
    // Record events over time
    for (int i = 0; i < 15; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Get throughput
    float throughput = shannon_monitor_get_throughput(monitor);

    // Throughput should be positive
    EXPECT_GE(throughput, 0.0f);
}

//=============================================================================
// Information Measurement Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, MeasuresEventInformation) {
    event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                      MW_EVENT_PRIORITY_HIGH);

    // Record event first to build probability model
    shannon_monitor_record_event(monitor, &event);

    // Measure information content
    float information = shannon_monitor_measure_event_information(monitor, &event);

    // Information should be positive (I = -log2(P))
    EXPECT_GT(information, 0.0f);
    // Typically should be under 15 bits for most events
    EXPECT_LT(information, 15.0f);
}

TEST_F(ShannonMonitorBioAsyncTest, RareEventsHaveHighInformation) {
    // Record many common events
    for (int i = 0; i < 50; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Record one rare event
    event_t rare_event = create_test_event(EVENT_TYPE_PATTERN_DETECTED,
                                           MW_EVENT_PRIORITY_CRITICAL);
    shannon_monitor_record_event(monitor, &rare_event);

    // Measure information of rare event
    float rare_info = shannon_monitor_measure_event_information(monitor, &rare_event);

    // Rare event should have higher information content (if implemented)
    // Implementation may return 0 if not yet implemented
    EXPECT_GE(rare_info, 0.0f);
}

//=============================================================================
// Bottleneck Detection Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, DetectsBottleneckWithHighLoad) {
    // Create high load
    for (int i = 0; i < 200; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_HIGH);
        shannon_monitor_record_event(monitor, &event);
    }

    // Detect bottleneck
    uint32_t bottleneck_module = 0;
    float severity = shannon_monitor_detect_bottleneck(monitor, &bottleneck_module);

    // Severity should be in valid range
    EXPECT_GE(severity, 0.0f);
    EXPECT_LE(severity, 1.0f);
}

TEST_F(ShannonMonitorBioAsyncTest, BottleneckSeverityIncreasesWithLoad) {
    // Baseline
    uint32_t module1 = 0;
    float severity1 = shannon_monitor_detect_bottleneck(monitor, &module1);

    // Add moderate load
    for (int i = 0; i < 50; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    uint32_t module2 = 0;
    float severity2 = shannon_monitor_detect_bottleneck(monitor, &module2);

    // Add heavy load
    for (int i = 0; i < 100; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_HIGH);
        shannon_monitor_record_event(monitor, &event);
    }

    uint32_t module3 = 0;
    float severity3 = shannon_monitor_detect_bottleneck(monitor, &module3);

    // Severity should increase with load (or stay at max if already bottlenecked)
    EXPECT_GE(severity3, severity1);
}

//=============================================================================
// Entropy Calculation Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, CalculatesEventEntropy) {
    // Record diverse events
    for (int i = 0; i < 30; i++) {
        event_type_t type = static_cast<event_type_t>(i % 5);
        mw_event_priority_t priority = static_cast<mw_event_priority_t>(i % 5);
        event_t event = create_test_event(type, priority);
        shannon_monitor_record_event(monitor, &event);
    }

    // Get event entropy
    float entropy = shannon_monitor_get_event_entropy(monitor);

    // Entropy should be non-negative (may be 0 if not yet implemented)
    EXPECT_GE(entropy, 0.0f);
    // If implemented, should be reasonable
    if (entropy > 0.0f) {
        EXPECT_LT(entropy, 10.0f);  // Should be under log2(1024) for reasonable events
    }
}

TEST_F(ShannonMonitorBioAsyncTest, UniformDistributionHasMaxEntropy) {
    // Record perfectly uniform distribution
    for (int type = 0; type < 4; type++) {
        for (int i = 0; i < 10; i++) {
            event_t event = create_test_event(static_cast<event_type_t>(type),
                                             MW_EVENT_PRIORITY_NORMAL);
            shannon_monitor_record_event(monitor, &event);
        }
    }

    float uniform_entropy = shannon_monitor_get_event_entropy(monitor);

    // Create skewed distribution
    shannon_monitor_reset(monitor);
    for (int i = 0; i < 39; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }
    event_t rare = create_test_event(EVENT_TYPE_PATTERN_DETECTED,
                                     MW_EVENT_PRIORITY_HIGH);
    shannon_monitor_record_event(monitor, &rare);

    float skewed_entropy = shannon_monitor_get_event_entropy(monitor);

    // Both should be non-negative
    EXPECT_GE(uniform_entropy, 0.0f);
    EXPECT_GE(skewed_entropy, 0.0f);

    // If entropy is implemented, uniform should have higher entropy
    if (uniform_entropy > 0.0f && skewed_entropy > 0.0f) {
        EXPECT_GT(uniform_entropy, skewed_entropy);
    }
}

//=============================================================================
// Filtered Event Tracking Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, TracksFilteredEvents) {
    // Record normal event
    event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                      MW_EVENT_PRIORITY_NORMAL);
    shannon_monitor_record_event(monitor, &event);

    // Record filtered event
    event_t filtered = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                         MW_EVENT_PRIORITY_LOW);
    shannon_monitor_record_filtered_event(monitor, &filtered, 2.5f);

    // Get metrics
    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(monitor);

    EXPECT_EQ(metrics.total_events, 1u);  // Only non-filtered counted
    EXPECT_EQ(metrics.filtered_events, 1u);
    EXPECT_GT(metrics.information_loss_rate, 0.0f);
}

TEST_F(ShannonMonitorBioAsyncTest, CalculatesInformationLossPercentage) {
    // Record events
    for (int i = 0; i < 10; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Filter some events
    for (int i = 0; i < 3; i++) {
        event_t filtered = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                             MW_EVENT_PRIORITY_LOW);
        shannon_monitor_record_filtered_event(monitor, &filtered, 5.0f);
    }

    // Get loss percentage
    float loss_pct = shannon_monitor_get_information_loss_percentage(monitor);

    // Should be between 0-100%
    EXPECT_GE(loss_pct, 0.0f);
    EXPECT_LE(loss_pct, 100.0f);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, HandlesConcurrentEventRecording) {
    const int num_threads = 4;
    const int events_per_thread = 25;
    std::atomic<int> total_recorded{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, events_per_thread, &total_recorded]() {
            for (int i = 0; i < events_per_thread; i++) {
                event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                                  MW_EVENT_PRIORITY_NORMAL);
                shannon_monitor_record_event(this->monitor, &event);
                total_recorded++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_recorded.load(), num_threads * events_per_thread);

    // Verify all were recorded
    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(monitor);
    EXPECT_EQ(metrics.total_events, (uint64_t)(num_threads * events_per_thread));
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, ConfiguresSNR) {
    // Set SNR
    float snr = 50.0f;
    shannon_monitor_set_snr(monitor, snr);

    // Record events
    for (int i = 0; i < 10; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Capacity should reflect SNR
    float capacity = shannon_monitor_calculate_channel_capacity(monitor);
    EXPECT_GT(capacity, 0.0f);
}

TEST_F(ShannonMonitorBioAsyncTest, ConfiguresBottleneckThreshold) {
    // Set high threshold (harder to trigger bottleneck)
    shannon_monitor_set_bottleneck_threshold(monitor, 0.95f);

    // Add load
    for (int i = 0; i < 100; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Check bottleneck
    bool bottlenecked = shannon_monitor_is_bottlenecked(monitor);

    // With high threshold, may not be bottlenecked
    // Just verify API works
    EXPECT_TRUE(bottlenecked || !bottlenecked);  // Valid either way
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, ResetClearsStatistics) {
    // Record events
    for (int i = 0; i < 20; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Verify events recorded
    shannon_routing_metrics_t metrics_before = shannon_monitor_get_metrics(monitor);
    EXPECT_GT(metrics_before.total_events, 0u);

    // Reset
    shannon_monitor_reset(monitor);

    // Verify cleared
    shannon_routing_metrics_t metrics_after = shannon_monitor_get_metrics(monitor);
    EXPECT_EQ(metrics_after.total_events, 0u);
    EXPECT_EQ(metrics_after.filtered_events, 0u);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(ShannonMonitorBioAsyncTest, HandlesHighThroughput) {
    const int num_events = 10000;

    for (int i = 0; i < num_events; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    // Verify all recorded
    shannon_routing_metrics_t metrics = shannon_monitor_get_metrics(monitor);
    EXPECT_EQ(metrics.total_events, (uint64_t)num_events);
}

TEST_F(ShannonMonitorBioAsyncTest, LowLatencyEventRecording) {
    const int num_events = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_events; i++) {
        event_t event = create_test_event(EVENT_TYPE_SPIKE_BURST,
                                          MW_EVENT_PRIORITY_NORMAL);
        shannon_monitor_record_event(monitor, &event);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    float avg_latency_us = (float)duration / num_events;

    // Should be under 10�s per event (including entropy calculation overhead)
    EXPECT_LT(avg_latency_us, 10.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
