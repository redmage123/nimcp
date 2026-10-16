//=============================================================================
// test_event_bus.cpp - Event Bus Comprehensive Tests
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/events/nimcp_event_bus.h"
#include "middleware/events/nimcp_event_types.h"
#include "middleware/events/nimcp_event_queue.h"
#include "middleware/events/nimcp_event_subscriber.h"

#include <thread>
#include <atomic>
#include <vector>

//=============================================================================
// Test Fixtures
//=============================================================================

class EventBusTest : public ::testing::Test {
protected:
    event_bus_t bus = nullptr;

    void SetUp() override {
        event_bus_config_t config = event_bus_default_config();
        config.queue_capacity = 100;
        bus = event_bus_create(&config);
        ASSERT_NE(bus, nullptr);
    }

    void TearDown() override {
        if (bus) {
            event_bus_destroy(bus);
        }
    }
};

//=============================================================================
// Event Creation Tests
//=============================================================================

TEST_F(EventBusTest, CreateAndDestroyEventBus) {
    EXPECT_NE(bus, nullptr);
}

TEST_F(EventBusTest, CreateEventTypes) {
    // Spike burst event
    uint32_t neurons[] = {1, 2, 3, 4, 5};
    event_t spike = event_create_spike_burst(
        neurons, 5, 0.95f, 1000,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_BRAIN);

    EXPECT_EQ(spike.type, EVENT_TYPE_SPIKE_BURST);
    EXPECT_EQ(spike.data.spike_burst.num_neurons, 5u);
    EXPECT_FLOAT_EQ(spike.data.spike_burst.synchrony_score, 0.95f);

    // Pattern detected event
    event_t pattern = event_create_pattern_detected(
        42, 0.88f, 10, "test_pattern",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_PATTERN_DETECTOR);

    EXPECT_EQ(pattern.type, EVENT_TYPE_PATTERN_DETECTED);
    EXPECT_EQ(pattern.data.pattern_detected.pattern_id, 42u);
    EXPECT_FLOAT_EQ(pattern.data.pattern_detected.match_confidence, 0.88f);

    // Salience peak event
    event_t salience = event_create_salience_peak(
        0.9f, 0.7f, 0.8f, 0.6f,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_SALIENCE);

    EXPECT_EQ(salience.type, EVENT_TYPE_SALIENCE_PEAK);
    EXPECT_FLOAT_EQ(salience.data.salience_peak.salience_score, 0.9f);
    EXPECT_FLOAT_EQ(salience.data.salience_peak.novelty_score, 0.7f);
}

TEST_F(EventBusTest, EventCopyAndFree) {
    uint32_t neurons[] = {1, 2, 3};
    event_t original = event_create_spike_burst(
        neurons, 3, 0.9f, 500,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);

    event_t copy;
    EXPECT_TRUE(event_copy(&copy, &original));

    EXPECT_EQ(copy.type, original.type);
    EXPECT_EQ(copy.data.spike_burst.num_neurons,
              original.data.spike_burst.num_neurons);

    // Verify deep copy
    EXPECT_NE(copy.data.spike_burst.neuron_ids,
              original.data.spike_burst.neuron_ids);

    event_free(&copy);
    // Original should still be valid
    EXPECT_EQ(original.data.spike_burst.num_neurons, 3u);
}

//=============================================================================
// Event Queue Tests
//=============================================================================

TEST_F(EventBusTest, QueueEnqueueDequeue) {
    event_t event = event_create_salience_peak(
        0.8f, 0.6f, 0.7f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);

    EXPECT_TRUE(event_bus_publish(bus, &event));

    event_bus_stats_t stats;
    EXPECT_TRUE(event_bus_get_stats(bus, &stats));
    EXPECT_EQ(stats.events_published, 1u);
    EXPECT_EQ(stats.queue_size, 1u);

    // Process event
    uint32_t processed = event_bus_process_events(bus, 10);
    EXPECT_EQ(processed, 1u);

    EXPECT_TRUE(event_bus_get_stats(bus, &stats));
    EXPECT_EQ(stats.events_delivered, 1u);
    EXPECT_EQ(stats.queue_size, 0u);
}

TEST_F(EventBusTest, PriorityOrdering) {
    // Enqueue events with different priorities
    event_t low = event_create_salience_peak(0.1f, 0.1f, 0.1f, 0.1f,
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_SALIENCE);
    event_t high = event_create_salience_peak(0.9f, 0.9f, 0.9f, 0.9f,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_SALIENCE);
    event_t critical = event_create_salience_peak(1.0f, 1.0f, 1.0f, 1.0f,
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_SALIENCE);

    // Enqueue in wrong order
    EXPECT_TRUE(event_bus_publish(bus, &low));
    EXPECT_TRUE(event_bus_publish(bus, &high));
    EXPECT_TRUE(event_bus_publish(bus, &critical));

    // Should come out ordered by priority
    std::atomic<int> callback_count{0};
    std::vector<mw_event_priority_t> received_priorities;

    auto callback = [](const event_t* evt, void* ctx) {
        auto* priorities = static_cast<std::vector<mw_event_priority_t>*>(ctx);
        priorities->push_back(evt->priority);
    };

    event_bus_subscribe(bus, callback, &received_priorities, nullptr);

    event_bus_process_events(bus, 10);

    ASSERT_EQ(received_priorities.size(), 3u);
    EXPECT_EQ(received_priorities[0], MW_EVENT_PRIORITY_CRITICAL);
    EXPECT_EQ(received_priorities[1], MW_EVENT_PRIORITY_HIGH);
    EXPECT_EQ(received_priorities[2], MW_EVENT_PRIORITY_LOW);
}

//=============================================================================
// Subscriber Tests
//=============================================================================

TEST_F(EventBusTest, SubscribeAndReceive) {
    std::atomic<int> callback_count{0};

    auto callback = [](const event_t* evt, void* ctx) {
        auto* count = static_cast<std::atomic<int>*>(ctx);
        (*count)++;
    };

    subscription_handle_t handle = event_bus_subscribe(
        bus, callback, &callback_count, nullptr);

    EXPECT_NE(handle, SUBSCRIPTION_HANDLE_INVALID);

    // Publish event
    event_t event = event_create_salience_peak(
        0.8f, 0.7f, 0.6f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);

    event_bus_publish(bus, &event);
    event_bus_process_events(bus, 10);

    EXPECT_EQ(callback_count.load(), 1);
}

TEST_F(EventBusTest, MultipleSubscribers) {
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};
    std::atomic<int> count3{0};

    auto callback = [](const event_t* evt, void* ctx) {
        auto* count = static_cast<std::atomic<int>*>(ctx);
        (*count)++;
    };

    event_bus_subscribe(bus, callback, &count1, nullptr);
    event_bus_subscribe(bus, callback, &count2, nullptr);
    event_bus_subscribe(bus, callback, &count3, nullptr);

    event_t event = event_create_salience_peak(
        0.8f, 0.7f, 0.6f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);

    event_bus_publish(bus, &event);
    event_bus_process_events(bus, 10);

    // All three should receive
    EXPECT_EQ(count1.load(), 1);
    EXPECT_EQ(count2.load(), 1);
    EXPECT_EQ(count3.load(), 1);
}

TEST_F(EventBusTest, FilteredSubscription) {
    std::atomic<int> salience_count{0};
    std::atomic<int> pattern_count{0};

    auto callback = [](const event_t* evt, void* ctx) {
        auto* count = static_cast<std::atomic<int>*>(ctx);
        (*count)++;
    };

    // Subscribe only to salience events
    subscription_config_t salience_config = subscriber_default_config();
    event_type_t salience_types[] = {EVENT_TYPE_SALIENCE_PEAK};
    salience_config.event_types = salience_types;
    salience_config.num_types = 1;

    event_bus_subscribe(bus, callback, &salience_count, &salience_config);

    // Subscribe only to pattern events
    subscription_config_t pattern_config = subscriber_default_config();
    event_type_t pattern_types[] = {EVENT_TYPE_PATTERN_DETECTED};
    pattern_config.event_types = pattern_types;
    pattern_config.num_types = 1;

    event_bus_subscribe(bus, callback, &pattern_count, &pattern_config);

    // Publish different event types
    event_t salience = event_create_salience_peak(
        0.8f, 0.7f, 0.6f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);

    event_t pattern = event_create_pattern_detected(
        1, 0.9f, 5, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_PATTERN_DETECTOR);

    event_bus_publish(bus, &salience);
    event_bus_publish(bus, &pattern);
    event_bus_process_events(bus, 10);

    // Each subscriber should only receive their type
    EXPECT_EQ(salience_count.load(), 1);
    EXPECT_EQ(pattern_count.load(), 1);
}

TEST_F(EventBusTest, Unsubscribe) {
    std::atomic<int> count{0};

    auto callback = [](const event_t* evt, void* ctx) {
        auto* count = static_cast<std::atomic<int>*>(ctx);
        (*count)++;
    };

    subscription_handle_t handle = event_bus_subscribe(
        bus, callback, &count, nullptr);

    // Publish and process
    event_t event = event_create_salience_peak(
        0.8f, 0.7f, 0.6f, 0.5f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);

    event_bus_publish(bus, &event);
    event_bus_process_events(bus, 10);
    EXPECT_EQ(count.load(), 1);

    // Unsubscribe
    EXPECT_TRUE(event_bus_unsubscribe(bus, handle));

    // Publish again
    event_bus_publish(bus, &event);
    event_bus_process_events(bus, 10);

    // Count should not increase
    EXPECT_EQ(count.load(), 1);
}

//=============================================================================
// Concurrency Tests
//=============================================================================

TEST_F(EventBusTest, ConcurrentPublish) {
    std::atomic<int> total_events{0};

    auto callback = [](const event_t* evt, void* ctx) {
        auto* count = static_cast<std::atomic<int>*>(ctx);
        (*count)++;
    };

    event_bus_subscribe(bus, callback, &total_events, nullptr);

    // Multiple threads publishing
    const int num_threads = 4;
    const int events_per_thread = 25;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, events_per_thread]() {
            for (int i = 0; i < events_per_thread; i++) {
                event_t event = event_create_salience_peak(
                    0.5f, 0.5f, 0.5f, 0.5f,
                    MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);
                event_bus_publish(this->bus, &event);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Process all events
    while (event_bus_process_events(bus, 32) > 0) {
        // Keep processing until queue empty
    }

    EXPECT_EQ(total_events.load(), num_threads * events_per_thread);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(EventBusTest, HighThroughput) {
    const int num_events = 1000;

    std::atomic<int> received{0};
    auto callback = [](const event_t* evt, void* ctx) {
        auto* count = static_cast<std::atomic<int>*>(ctx);
        (*count)++;
    };

    event_bus_subscribe(bus, callback, &received, nullptr);

    // Publish and process in batches to avoid overflow
    int total_processed = 0;
    for (int i = 0; i < num_events; i++) {
        event_t event = event_create_salience_peak(
            0.5f, 0.5f, 0.5f, 0.5f,
            static_cast<mw_event_priority_t>(i % 5),
            EVENT_SOURCE_SALIENCE);
        event_bus_publish(bus, &event);

        // Process every 50 events to avoid queue overflow
        if ((i + 1) % 50 == 0) {
            uint32_t processed = event_bus_process_events(bus, 100);
            total_processed += processed;
        }
    }

    // Process remaining events
    while (true) {
        uint32_t processed = event_bus_process_events(bus, 100);
        if (processed == 0) break;
        total_processed += processed;
    }

    EXPECT_EQ(total_processed, num_events);
    EXPECT_EQ(received.load(), num_events);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(EventBusTest, NullParameterHandling) {
    EXPECT_FALSE(event_bus_publish(nullptr, nullptr));
    EXPECT_FALSE(event_bus_publish(bus, nullptr));

    EXPECT_EQ(event_bus_subscribe(nullptr, nullptr, nullptr, nullptr),
              SUBSCRIPTION_HANDLE_INVALID);

    EXPECT_FALSE(event_bus_unsubscribe(nullptr, 1));
    EXPECT_FALSE(event_bus_unsubscribe(bus, SUBSCRIPTION_HANDLE_INVALID));

    EXPECT_EQ(event_bus_process_events(nullptr, 10), 0u);
}

TEST_F(EventBusTest, OverflowHandling) {
    // Create small capacity bus
    event_bus_destroy(bus);

    event_bus_config_t config = event_bus_default_config();
    config.queue_capacity = 5;
    config.overflow_policy = OVERFLOW_POLICY_DROP_OLDEST;
    bus = event_bus_create(&config);

    // Fill queue beyond capacity
    for (int i = 0; i < 10; i++) {
        event_t event = event_create_salience_peak(
            static_cast<float>(i) / 10.0f, 0.5f, 0.5f, 0.5f,
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_SALIENCE);
        event_bus_publish(bus, &event);
    }

    event_bus_stats_t stats;
    event_bus_get_stats(bus, &stats);

    // Should have dropped some events
    EXPECT_GT(stats.events_dropped, 0u);
    EXPECT_LE(stats.queue_size, 5u);
}
