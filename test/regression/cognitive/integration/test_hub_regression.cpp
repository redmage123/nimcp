/**
 * @file test_hub_regression.cpp
 * @brief Regression tests for cognitive integration hub
 * @date 2025-01-08
 *
 * WHAT: Regression tests for the cognitive integration hub
 * WHY: Ensure hub behavior remains consistent across code changes
 * HOW: Test event ordering, event delivery, callback stability, memory management
 *
 * Tests:
 * - EventOrderPreserved: Verify events are delivered in publication order
 * - NoEventLoss: Verify all published events are received
 * - CallbackStability: Verify callbacks work after subscribe/unsubscribe cycles
 * - MemoryLeakPrevention: Verify no crashes during repeated create/destroy
 * - ThreadSafetyRegression: Verify thread-safe event publishing
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <chrono>
#include <cstring>

#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HubRegressionTest : public ::testing::Test {
protected:
    cognitive_integration_hub_t hub = nullptr;

    void SetUp() override {
        cognitive_hub_config_t config = cognitive_hub_default_config();
        config.max_modules = 64;
        config.max_subscriptions = 256;
        config.enable_async = false;  // Use sync for deterministic testing
        hub = cognitive_hub_create(&config);
        ASSERT_NE(hub, nullptr) << "Failed to create cognitive hub";
    }

    void TearDown() override {
        if (hub) {
            cognitive_hub_destroy(hub);
            hub = nullptr;
        }
    }
};

/* ============================================================================
 * Callback Context Structures
 * ============================================================================ */

/**
 * @brief Context for tracking event sequence order
 */
struct SequenceTrackingContext {
    std::vector<uint32_t> received_sequences;
    std::mutex mutex;
};

/**
 * @brief Context for counting received events
 */
struct EventCountContext {
    std::atomic<uint64_t> event_count{0};
};

/**
 * @brief Context for thread safety testing
 */
struct ThreadSafetyContext {
    std::atomic<uint64_t> total_events{0};
    std::mutex event_mutex;
    std::vector<uint32_t> source_ids;
};

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

/**
 * @brief Callback that records sequence numbers
 */
static int sequence_tracking_callback(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    SequenceTrackingContext* ctx = static_cast<SequenceTrackingContext*>(user_data);

    // Payload contains sequence number
    if (event->payload && event->payload_size >= sizeof(uint32_t)) {
        uint32_t seq = *static_cast<const uint32_t*>(event->payload);
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->received_sequences.push_back(seq);
    }

    return 0;
}

/**
 * @brief Callback that counts events
 */
static int event_counting_callback(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    EventCountContext* ctx = static_cast<EventCountContext*>(user_data);
    ctx->event_count.fetch_add(1, std::memory_order_relaxed);

    return 0;
}

/**
 * @brief Callback for thread safety testing
 */
static int thread_safety_callback(const cognitive_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    ThreadSafetyContext* ctx = static_cast<ThreadSafetyContext*>(user_data);
    ctx->total_events.fetch_add(1, std::memory_order_relaxed);

    // Record source module ID
    {
        std::lock_guard<std::mutex> lock(ctx->event_mutex);
        ctx->source_ids.push_back(event->source_module_id);
    }

    return 0;
}

/* ============================================================================
 * Regression Tests
 * ============================================================================ */

/**
 * @brief Test that events are delivered in the order they were published
 *
 * WHAT: Verify event ordering is preserved
 * WHY: Subscribers may depend on event ordering for correct behavior
 * HOW: Publish 100 events with sequence numbers, verify received in order
 */
TEST_F(HubRegressionTest, EventOrderPreserved) {
    const uint32_t PUBLISHER_ID = 1;
    const uint32_t SUBSCRIBER_ID = 2;
    const uint32_t NUM_EVENTS = 100;

    // Register publisher and subscriber modules
    int result = cognitive_hub_register_module(
        hub, PUBLISHER_ID, COG_CATEGORY_MEMORY, "publisher", nullptr
    );
    ASSERT_EQ(result, 0) << "Failed to register publisher module";

    result = cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_REASONING, "subscriber", nullptr
    );
    ASSERT_EQ(result, 0) << "Failed to register subscriber module";

    // Set up tracking context
    SequenceTrackingContext ctx;

    // Subscribe to state change events
    result = cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_STATE_CHANGE,
        sequence_tracking_callback, &ctx
    );
    ASSERT_EQ(result, 0) << "Failed to subscribe to events";

    // Publish events with sequence numbers
    for (uint32_t i = 0; i < NUM_EVENTS; i++) {
        uint32_t sequence = i;
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_STATE_CHANGE;
        event.source_module_id = PUBLISHER_ID;
        event.timestamp = static_cast<uint64_t>(i);
        event.priority = COG_PRIORITY_NORMAL;
        event.payload = &sequence;
        event.payload_size = sizeof(sequence);

        result = cognitive_hub_publish(hub, PUBLISHER_ID, COG_EVENT_STATE_CHANGE, &event);
        ASSERT_EQ(result, 0) << "Failed to publish event " << i;
    }

    // Verify all events received in order
    ASSERT_EQ(ctx.received_sequences.size(), NUM_EVENTS)
        << "Not all events were received";

    for (uint32_t i = 0; i < NUM_EVENTS; i++) {
        EXPECT_EQ(ctx.received_sequences[i], i)
            << "Event " << i << " received out of order, got " << ctx.received_sequences[i];
    }
}

/**
 * @brief Test that no events are lost during rapid publishing
 *
 * WHAT: Verify all published events are delivered
 * WHY: Event loss could cause inconsistent system state
 * HOW: Publish 1000 events rapidly, count received events
 */
TEST_F(HubRegressionTest, NoEventLoss) {
    const uint32_t PUBLISHER_ID = 1;
    const uint32_t SUBSCRIBER_ID = 2;
    const uint64_t NUM_EVENTS = 1000;

    // Register modules
    ASSERT_EQ(cognitive_hub_register_module(
        hub, PUBLISHER_ID, COG_CATEGORY_MEMORY, "publisher", nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_REASONING, "subscriber", nullptr
    ), 0);

    // Set up counting context
    EventCountContext ctx;

    // Subscribe to memory access events
    ASSERT_EQ(cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_MEMORY_ACCESS,
        event_counting_callback, &ctx
    ), 0);

    // Rapidly publish events
    for (uint64_t i = 0; i < NUM_EVENTS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_MEMORY_ACCESS;
        event.source_module_id = PUBLISHER_ID;
        event.timestamp = i;
        event.priority = COG_PRIORITY_NORMAL;
        event.payload = nullptr;
        event.payload_size = 0;

        int result = cognitive_hub_publish(hub, PUBLISHER_ID, COG_EVENT_MEMORY_ACCESS, &event);
        EXPECT_EQ(result, 0) << "Failed to publish event " << i;
    }

    // Verify all events received
    EXPECT_EQ(ctx.event_count.load(), NUM_EVENTS)
        << "Event loss detected: expected " << NUM_EVENTS
        << ", received " << ctx.event_count.load();

    // Verify stats match
    cognitive_hub_stats_t stats = {};
    ASSERT_EQ(cognitive_hub_get_stats(hub, &stats), 0);
    EXPECT_EQ(stats.events_published, NUM_EVENTS);
    EXPECT_EQ(stats.events_delivered, NUM_EVENTS);
    EXPECT_EQ(stats.events_dropped, 0ULL);
}

/**
 * @brief Test callback stability after subscribe/unsubscribe cycles
 *
 * WHAT: Verify callbacks work correctly after subscription changes
 * WHY: Dynamic subscription changes should not corrupt callback state
 * HOW: Subscribe/unsubscribe/subscribe same module, verify callbacks work
 */
TEST_F(HubRegressionTest, CallbackStability) {
    const uint32_t PUBLISHER_ID = 1;
    const uint32_t SUBSCRIBER_ID = 2;
    const int NUM_CYCLES = 10;
    const int EVENTS_PER_CYCLE = 10;

    // Register modules
    ASSERT_EQ(cognitive_hub_register_module(
        hub, PUBLISHER_ID, COG_CATEGORY_EMOTIONAL, "publisher", nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_SELF, "subscriber", nullptr
    ), 0);

    EventCountContext ctx;
    uint64_t expected_total = 0;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        // Subscribe
        ASSERT_EQ(cognitive_hub_subscribe(
            hub, SUBSCRIBER_ID, COG_EVENT_EMOTION_UPDATE,
            event_counting_callback, &ctx
        ), 0) << "Failed to subscribe on cycle " << cycle;

        // Publish some events (should be received)
        for (int i = 0; i < EVENTS_PER_CYCLE; i++) {
            cognitive_event_data_t event = {};
            event.event_type = COG_EVENT_EMOTION_UPDATE;
            event.source_module_id = PUBLISHER_ID;
            event.priority = COG_PRIORITY_NORMAL;

            ASSERT_EQ(cognitive_hub_publish(
                hub, PUBLISHER_ID, COG_EVENT_EMOTION_UPDATE, &event
            ), 0);
            expected_total++;
        }

        // Unsubscribe
        ASSERT_EQ(cognitive_hub_unsubscribe(
            hub, SUBSCRIBER_ID, COG_EVENT_EMOTION_UPDATE
        ), 0) << "Failed to unsubscribe on cycle " << cycle;

        // Publish events (should NOT be received)
        for (int i = 0; i < EVENTS_PER_CYCLE; i++) {
            cognitive_event_data_t event = {};
            event.event_type = COG_EVENT_EMOTION_UPDATE;
            event.source_module_id = PUBLISHER_ID;
            event.priority = COG_PRIORITY_NORMAL;

            ASSERT_EQ(cognitive_hub_publish(
                hub, PUBLISHER_ID, COG_EVENT_EMOTION_UPDATE, &event
            ), 0);
            // Not incrementing expected_total - these should not be received
        }
    }

    // Verify only subscribed events were received
    EXPECT_EQ(ctx.event_count.load(), expected_total)
        << "Callback stability issue: expected " << expected_total
        << ", received " << ctx.event_count.load();
}

/**
 * @brief Test that repeated create/destroy cycles don't leak memory
 *
 * WHAT: Verify no memory leaks or crashes during hub lifecycle
 * WHY: Memory leaks degrade system performance over time
 * HOW: Create and destroy hub 100 times with module registration
 *
 * NOTE: This test doesn't assert on memory usage directly.
 * Run with valgrind to verify no leaks: valgrind --leak-check=full ./test_executable
 */
TEST_F(HubRegressionTest, MemoryLeakPrevention) {
    // First destroy the fixture hub
    cognitive_hub_destroy(hub);
    hub = nullptr;

    const int NUM_ITERATIONS = 100;
    const int MODULES_PER_HUB = 10;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        // Create hub with config
        cognitive_hub_config_t config = cognitive_hub_default_config();
        config.max_modules = 32;
        config.max_subscriptions = 128;
        config.enable_async = false;

        cognitive_integration_hub_t test_hub = cognitive_hub_create(&config);
        ASSERT_NE(test_hub, nullptr) << "Failed to create hub on iteration " << iter;

        // Register multiple modules
        for (int m = 0; m < MODULES_PER_HUB; m++) {
            char name[64];
            snprintf(name, sizeof(name), "module_%d_%d", iter, m);

            int result = cognitive_hub_register_module(
                test_hub,
                static_cast<uint32_t>(m + 1),
                static_cast<cognitive_category_t>(m % COG_CATEGORY_COUNT),
                name,
                nullptr
            );
            EXPECT_EQ(result, 0) << "Failed to register module " << m << " on iteration " << iter;
        }

        // Subscribe each module to events
        EventCountContext ctx;
        for (int m = 0; m < MODULES_PER_HUB; m++) {
            cognitive_hub_subscribe(
                test_hub,
                static_cast<uint32_t>(m + 1),
                static_cast<cognitive_event_type_t>(m % COG_EVENT_COUNT),
                event_counting_callback,
                &ctx
            );
        }

        // Unregister modules
        for (int m = 0; m < MODULES_PER_HUB; m++) {
            cognitive_hub_unregister_module(test_hub, static_cast<uint32_t>(m + 1));
        }

        // Destroy hub
        cognitive_hub_destroy(test_hub);
    }

    // If we got here without crashing, the test passes
    // Memory leak detection requires external tools like valgrind
    SUCCEED() << "Completed " << NUM_ITERATIONS << " create/destroy cycles without crash";

    // Recreate fixture hub for TearDown
    cognitive_hub_config_t config = cognitive_hub_default_config();
    hub = cognitive_hub_create(&config);
}

/**
 * @brief Test thread safety of concurrent event publishing
 *
 * WHAT: Verify hub handles concurrent access correctly
 * WHY: Multi-threaded cognitive systems require thread-safe event routing
 * HOW: Spawn 4 threads, each publishes 100 events, verify no crashes and all received
 */
TEST_F(HubRegressionTest, ThreadSafetyRegression) {
    const uint32_t NUM_THREADS = 4;
    const uint32_t EVENTS_PER_THREAD = 100;
    const uint32_t SUBSCRIBER_ID = 100;

    // Register subscriber module
    ASSERT_EQ(cognitive_hub_register_module(
        hub, SUBSCRIBER_ID, COG_CATEGORY_EXECUTIVE, "subscriber", nullptr
    ), 0);

    // Register publisher modules (one per thread)
    for (uint32_t t = 0; t < NUM_THREADS; t++) {
        char name[64];
        snprintf(name, sizeof(name), "publisher_%u", t);
        ASSERT_EQ(cognitive_hub_register_module(
            hub, t + 1, COG_CATEGORY_MEMORY, name, nullptr
        ), 0);
    }

    // Set up thread safety context
    ThreadSafetyContext ctx;

    // Subscribe to decision events
    ASSERT_EQ(cognitive_hub_subscribe(
        hub, SUBSCRIBER_ID, COG_EVENT_DECISION_MADE,
        thread_safety_callback, &ctx
    ), 0);

    // Thread function for publishing events
    auto publish_events = [this](uint32_t publisher_id, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            cognitive_event_data_t event = {};
            event.event_type = COG_EVENT_DECISION_MADE;
            event.source_module_id = publisher_id;
            event.timestamp = static_cast<uint64_t>(i);
            event.priority = COG_PRIORITY_NORMAL;
            event.payload = nullptr;
            event.payload_size = 0;

            cognitive_hub_publish(hub, publisher_id, COG_EVENT_DECISION_MADE, &event);

            // Small delay to increase interleaving likelihood
            if (i % 10 == 0) {
                std::this_thread::yield();
            }
        }
    };

    // Spawn threads
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (uint32_t t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(publish_events, t + 1, EVENTS_PER_THREAD);
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all events were received
    const uint64_t expected_events = NUM_THREADS * EVENTS_PER_THREAD;
    EXPECT_EQ(ctx.total_events.load(), expected_events)
        << "Thread safety issue: expected " << expected_events
        << " events, received " << ctx.total_events.load();

    // Verify we received events from all publishers
    std::vector<uint32_t> event_counts(NUM_THREADS + 1, 0);
    {
        std::lock_guard<std::mutex> lock(ctx.event_mutex);
        for (uint32_t source_id : ctx.source_ids) {
            if (source_id > 0 && source_id <= NUM_THREADS) {
                event_counts[source_id]++;
            }
        }
    }

    for (uint32_t t = 1; t <= NUM_THREADS; t++) {
        EXPECT_EQ(event_counts[t], EVENTS_PER_THREAD)
            << "Publisher " << t << " events: expected " << EVENTS_PER_THREAD
            << ", received " << event_counts[t];
    }
}

/**
 * @brief Test that async queue handles high load without event loss
 *
 * WHAT: Verify async event queue handles burst traffic
 * WHY: Async mode must not lose events during high load
 * HOW: Enable async mode, publish burst of events, verify all delivered
 */
TEST(HubRegressionAsyncTest, AsyncQueueHighLoad) {
    const uint32_t PUBLISHER_ID = 1;
    const uint32_t SUBSCRIBER_ID = 2;
    const uint64_t NUM_EVENTS = 500;
    const uint32_t FLUSH_TIMEOUT_MS = 5000;

    // Create hub with async enabled
    cognitive_hub_config_t config = cognitive_hub_default_config();
    config.enable_async = true;
    config.event_queue_size = 2048;  // Large queue for burst

    cognitive_integration_hub_t async_hub = cognitive_hub_create(&config);
    ASSERT_NE(async_hub, nullptr);

    // Register modules
    ASSERT_EQ(cognitive_hub_register_module(
        async_hub, PUBLISHER_ID, COG_CATEGORY_PERCEPTION, "publisher", nullptr
    ), 0);

    ASSERT_EQ(cognitive_hub_register_module(
        async_hub, SUBSCRIBER_ID, COG_CATEGORY_MEMORY, "subscriber", nullptr
    ), 0);

    // Set up counting context
    EventCountContext ctx;

    // Subscribe to input events
    ASSERT_EQ(cognitive_hub_subscribe(
        async_hub, SUBSCRIBER_ID, COG_EVENT_INPUT_RECEIVED,
        event_counting_callback, &ctx
    ), 0);

    // Burst publish events (async)
    for (uint64_t i = 0; i < NUM_EVENTS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_INPUT_RECEIVED;
        event.source_module_id = PUBLISHER_ID;
        event.timestamp = i;
        event.priority = COG_PRIORITY_NORMAL;

        int result = cognitive_hub_publish_async(
            async_hub, PUBLISHER_ID, COG_EVENT_INPUT_RECEIVED, &event
        );
        EXPECT_EQ(result, 0) << "Failed to async publish event " << i;
    }

    // Flush the async queue
    int flush_result = cognitive_hub_flush_async_queue(async_hub, FLUSH_TIMEOUT_MS);
    EXPECT_EQ(flush_result, 0) << "Async queue flush timed out";

    // Allow additional time for delivery
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify all events delivered
    EXPECT_EQ(ctx.event_count.load(), NUM_EVENTS)
        << "Async event loss: expected " << NUM_EVENTS
        << ", received " << ctx.event_count.load();

    cognitive_hub_destroy(async_hub);
}

/**
 * @brief Test multiple subscribers receive same event
 *
 * WHAT: Verify broadcast delivery to multiple subscribers
 * WHY: Events should fan out to all interested subscribers
 * HOW: Register multiple subscribers, publish event, verify all receive it
 */
TEST_F(HubRegressionTest, MultipleSubscribersReceiveEvents) {
    const uint32_t PUBLISHER_ID = 1;
    const uint32_t NUM_SUBSCRIBERS = 5;
    const uint32_t NUM_EVENTS = 50;

    // Register publisher
    ASSERT_EQ(cognitive_hub_register_module(
        hub, PUBLISHER_ID, COG_CATEGORY_PERCEPTION, "publisher", nullptr
    ), 0);

    // Create contexts and register subscribers
    std::vector<EventCountContext> contexts(NUM_SUBSCRIBERS);

    for (uint32_t s = 0; s < NUM_SUBSCRIBERS; s++) {
        uint32_t sub_id = s + 10;
        char name[64];
        snprintf(name, sizeof(name), "subscriber_%u", s);

        ASSERT_EQ(cognitive_hub_register_module(
            hub, sub_id,
            static_cast<cognitive_category_t>(s % COG_CATEGORY_COUNT),
            name, nullptr
        ), 0);

        ASSERT_EQ(cognitive_hub_subscribe(
            hub, sub_id, COG_EVENT_OUTPUT_READY,
            event_counting_callback, &contexts[s]
        ), 0);
    }

    // Publish events
    for (uint32_t i = 0; i < NUM_EVENTS; i++) {
        cognitive_event_data_t event = {};
        event.event_type = COG_EVENT_OUTPUT_READY;
        event.source_module_id = PUBLISHER_ID;
        event.priority = COG_PRIORITY_NORMAL;

        ASSERT_EQ(cognitive_hub_publish(
            hub, PUBLISHER_ID, COG_EVENT_OUTPUT_READY, &event
        ), 0);
    }

    // Verify each subscriber received all events
    for (uint32_t s = 0; s < NUM_SUBSCRIBERS; s++) {
        EXPECT_EQ(contexts[s].event_count.load(), NUM_EVENTS)
            << "Subscriber " << s << " received " << contexts[s].event_count.load()
            << " events, expected " << NUM_EVENTS;
    }
}
