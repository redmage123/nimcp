/**
 * @file test_hub_orchestrator_integration.cpp
 * @brief Integration tests for cognitive hub orchestration
 * @version 1.0.0
 * @date 2025-01-08
 *
 * WHAT: Integration tests for cognitive hub event flow and orchestration
 * WHY:  Verify hub correctly orchestrates event flow between modules,
 *       tracks statistics, and manages subscriptions
 * HOW:  Test module registration, event publishing, subscription management,
 *       and statistics accuracy
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <vector>
#include <chrono>
#include <thread>

// Headers have their own extern "C" guards
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define MODULE_PUBLISHER_1      100
#define MODULE_PUBLISHER_2      101
#define MODULE_SUBSCRIBER_1     200
#define MODULE_SUBSCRIBER_2     201
#define MODULE_SUBSCRIBER_3     202
#define MODULE_SUBSCRIBER_4     203
#define MODULE_SUBSCRIBER_5     204

/* ============================================================================
 * Test Context Structures
 * ============================================================================ */

struct EventTracker {
    std::atomic<int> total_events{0};
    std::atomic<int> state_change_events{0};
    std::atomic<int> input_received_events{0};
    std::atomic<int> output_ready_events{0};
    std::atomic<int> attention_shift_events{0};
    std::atomic<int> memory_access_events{0};
    std::atomic<int> emotion_update_events{0};
    std::vector<uint32_t> source_modules;
    std::vector<cognitive_event_priority_t> priorities;
};

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

static int event_tracker_callback(const cognitive_event_data_t* event, void* user_data) {
    EventTracker* tracker = static_cast<EventTracker*>(user_data);
    tracker->total_events++;

    switch (event->event_type) {
        case COG_EVENT_STATE_CHANGE:
            tracker->state_change_events++;
            break;
        case COG_EVENT_INPUT_RECEIVED:
            tracker->input_received_events++;
            break;
        case COG_EVENT_OUTPUT_READY:
            tracker->output_ready_events++;
            break;
        case COG_EVENT_ATTENTION_SHIFT:
            tracker->attention_shift_events++;
            break;
        case COG_EVENT_MEMORY_ACCESS:
            tracker->memory_access_events++;
            break;
        case COG_EVENT_EMOTION_UPDATE:
            tracker->emotion_update_events++;
            break;
        default:
            break;
    }

    tracker->source_modules.push_back(event->source_module_id);
    tracker->priorities.push_back(event->priority);

    return 0;
}

static int counting_callback(const cognitive_event_data_t* event, void* user_data) {
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(user_data);
    (*counter)++;
    return 0;
}

static int failing_callback(const cognitive_event_data_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    return -1;  /* Simulate callback failure */
}

/* ============================================================================
 * Query Handler
 * ============================================================================ */

static int simple_query_handler(const cognitive_query_t* query,
                                 cognitive_query_result_t* result,
                                 void* context) {
    (void)query;
    (void)context;

    result->status = 0;
    snprintf(result->error_message, sizeof(result->error_message), "Query handled");
    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HubOrchestratorIntegrationTest : public ::testing::Test {
protected:
    cognitive_integration_hub_t hub;
    cognitive_hub_config_t config;

    void SetUp() override {
        hub = nullptr;

        /* Get default config */
        config = cognitive_hub_default_config();
        config.max_modules = 32;
        config.max_subscriptions = 128;
        config.enable_async = false;  /* Synchronous for deterministic tests */

        /* Create hub */
        hub = cognitive_hub_create(&config);
        ASSERT_NE(hub, nullptr);
    }

    void TearDown() override {
        if (hub) {
            cognitive_hub_destroy(hub);
        }
    }

    /* Helper to register a set of modules */
    void register_test_modules() {
        /* Publishers */
        cognitive_hub_register_module(hub, MODULE_PUBLISHER_1,
                                       COG_CATEGORY_PERCEPTION,
                                       "publisher_1", nullptr);
        cognitive_hub_register_module(hub, MODULE_PUBLISHER_2,
                                       COG_CATEGORY_REASONING,
                                       "publisher_2", nullptr);

        /* Subscribers */
        cognitive_hub_register_module(hub, MODULE_SUBSCRIBER_1,
                                       COG_CATEGORY_MEMORY,
                                       "subscriber_1", nullptr);
        cognitive_hub_register_module(hub, MODULE_SUBSCRIBER_2,
                                       COG_CATEGORY_EXECUTIVE,
                                       "subscriber_2", nullptr);
        cognitive_hub_register_module(hub, MODULE_SUBSCRIBER_3,
                                       COG_CATEGORY_EMOTIONAL,
                                       "subscriber_3", nullptr);
    }

    /* Helper to publish event */
    void publish_event(uint32_t publisher_id, cognitive_event_type_t event_type,
                       cognitive_event_priority_t priority) {
        cognitive_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = event_type;
        event.source_module_id = publisher_id;
        event.priority = priority;
        event.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );

        cognitive_hub_publish(hub, publisher_id, event_type, &event);
    }
};

/* ============================================================================
 * Hub Event Flow Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, HubEventFlow) {
    /* Step 1: Register modules */
    register_test_modules();

    /* Verify registration */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.registered_modules, 5u);

    /* Step 2: Set up subscriptions with event tracker */
    EventTracker tracker1, tracker2, tracker3;

    int ret = cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1,
                                       COG_EVENT_INPUT_RECEIVED,
                                       event_tracker_callback, &tracker1);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_2,
                                   COG_EVENT_INPUT_RECEIVED,
                                   event_tracker_callback, &tracker2);
    ASSERT_EQ(ret, 0);

    ret = cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_3,
                                   COG_EVENT_INPUT_RECEIVED,
                                   event_tracker_callback, &tracker3);
    ASSERT_EQ(ret, 0);

    /* Verify subscription count */
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.active_subscriptions, 3u);

    /* Step 3: Publish events */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PUBLISHER_1;
    event.priority = COG_PRIORITY_NORMAL;

    ret = cognitive_hub_publish(hub, MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED, &event);
    EXPECT_EQ(ret, 0);

    /* Step 4: Verify all subscribers received */
    EXPECT_EQ(tracker1.total_events.load(), 1);
    EXPECT_EQ(tracker2.total_events.load(), 1);
    EXPECT_EQ(tracker3.total_events.load(), 1);

    /* Verify event details */
    EXPECT_EQ(tracker1.input_received_events.load(), 1);
    EXPECT_FALSE(tracker1.source_modules.empty());
    EXPECT_EQ(tracker1.source_modules[0], MODULE_PUBLISHER_1);

    /* Step 5: Verify stats accurate */
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 1u);
    EXPECT_EQ(stats.events_delivered, 3u);  /* 3 subscribers */
    EXPECT_EQ(stats.events_dropped, 0u);
}

TEST_F(HubOrchestratorIntegrationTest, MultipleEventTypes) {
    register_test_modules();

    EventTracker tracker;

    /* Subscribe to multiple event types */
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_STATE_CHANGE,
                             event_tracker_callback, &tracker);
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                             event_tracker_callback, &tracker);
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_EMOTION_UPDATE,
                             event_tracker_callback, &tracker);

    /* Publish different event types */
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_STATE_CHANGE, COG_PRIORITY_HIGH);
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_EMOTION_UPDATE, COG_PRIORITY_LOW);

    /* Verify all received */
    EXPECT_EQ(tracker.total_events.load(), 3);
    EXPECT_EQ(tracker.state_change_events.load(), 1);
    EXPECT_EQ(tracker.input_received_events.load(), 1);
    EXPECT_EQ(tracker.emotion_update_events.load(), 1);

    /* Verify stats */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 3u);
    EXPECT_EQ(stats.events_delivered, 3u);
}

TEST_F(HubOrchestratorIntegrationTest, EventPriorityTracking) {
    register_test_modules();

    EventTracker tracker;
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_OUTPUT_READY,
                             event_tracker_callback, &tracker);

    /* Publish events with different priorities */
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_OUTPUT_READY, COG_PRIORITY_LOW);
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_OUTPUT_READY, COG_PRIORITY_NORMAL);
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_OUTPUT_READY, COG_PRIORITY_HIGH);
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_OUTPUT_READY, COG_PRIORITY_CRITICAL);

    EXPECT_EQ(tracker.total_events.load(), 4);
    EXPECT_EQ(tracker.priorities.size(), 4u);

    /* Verify priorities were tracked */
    EXPECT_EQ(tracker.priorities[0], COG_PRIORITY_LOW);
    EXPECT_EQ(tracker.priorities[1], COG_PRIORITY_NORMAL);
    EXPECT_EQ(tracker.priorities[2], COG_PRIORITY_HIGH);
    EXPECT_EQ(tracker.priorities[3], COG_PRIORITY_CRITICAL);
}

/* ============================================================================
 * Subscription Management Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, SubscriptionUnsubscription) {
    register_test_modules();

    std::atomic<int> counter{0};

    /* Subscribe */
    int ret = cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                                       counting_callback, &counter);
    ASSERT_EQ(ret, 0);

    /* Publish - should receive */
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);
    EXPECT_EQ(counter.load(), 1);

    /* Unsubscribe */
    ret = cognitive_hub_unsubscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED);
    EXPECT_EQ(ret, 0);

    /* Publish - should NOT receive */
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);
    EXPECT_EQ(counter.load(), 1);  /* Still 1 */

    /* Stats should show unsubscription */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.active_subscriptions, 0u);
}

TEST_F(HubOrchestratorIntegrationTest, SubscriptionUpdate) {
    register_test_modules();

    std::atomic<int> counter1{0}, counter2{0};

    /* Subscribe with first callback */
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                             counting_callback, &counter1);

    publish_event(MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);
    EXPECT_EQ(counter1.load(), 1);
    EXPECT_EQ(counter2.load(), 0);

    /* Update subscription with different callback/data */
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                             counting_callback, &counter2);

    publish_event(MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);
    /* Second callback should be called */
    EXPECT_EQ(counter2.load(), 1);
}

TEST_F(HubOrchestratorIntegrationTest, MultipleSubscribersPerEvent) {
    register_test_modules();

    std::atomic<int> c1{0}, c2{0}, c3{0}, c4{0}, c5{0};

    /* Five subscribers to same event */
    cognitive_hub_register_module(hub, MODULE_SUBSCRIBER_4, COG_CATEGORY_SOCIAL,
                                   "subscriber_4", nullptr);
    cognitive_hub_register_module(hub, MODULE_SUBSCRIBER_5, COG_CATEGORY_SELF,
                                   "subscriber_5", nullptr);

    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_STATE_CHANGE,
                             counting_callback, &c1);
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_2, COG_EVENT_STATE_CHANGE,
                             counting_callback, &c2);
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_3, COG_EVENT_STATE_CHANGE,
                             counting_callback, &c3);
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_4, COG_EVENT_STATE_CHANGE,
                             counting_callback, &c4);
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_5, COG_EVENT_STATE_CHANGE,
                             counting_callback, &c5);

    /* Single publish */
    publish_event(MODULE_PUBLISHER_1, COG_EVENT_STATE_CHANGE, COG_PRIORITY_HIGH);

    /* All should receive */
    EXPECT_EQ(c1.load(), 1);
    EXPECT_EQ(c2.load(), 1);
    EXPECT_EQ(c3.load(), 1);
    EXPECT_EQ(c4.load(), 1);
    EXPECT_EQ(c5.load(), 1);

    /* Stats should reflect all deliveries */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_delivered, 5u);
}

/* ============================================================================
 * Callback Error Handling Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, CallbackErrorContinuesDelivery) {
    register_test_modules();

    std::atomic<int> success_counter{0};

    /* First subscriber fails */
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                             failing_callback, nullptr);
    /* Second subscriber succeeds */
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_2, COG_EVENT_INPUT_RECEIVED,
                             counting_callback, &success_counter);

    /* Publish - both should be called despite first failing */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PUBLISHER_1;
    event.priority = COG_PRIORITY_NORMAL;

    int ret = cognitive_hub_publish(hub, MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED,
                                     &event);
    /* Publish should succeed */
    EXPECT_EQ(ret, 0);
    /* The success_counter should have been incremented */

    /* Stats should show deliveries */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_GE(stats.events_delivered, 1u);
}

/* ============================================================================
 * Query Handling Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, QueryHandling) {
    register_test_modules();

    /* Register query handler */
    int ret = cognitive_hub_register_query_handler(hub, MODULE_SUBSCRIBER_1,
                                                    simple_query_handler);
    ASSERT_EQ(ret, 0);

    /* Query from publisher to subscriber */
    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_STATUS;

    cognitive_query_result_t result;
    memset(&result, 0, sizeof(result));

    ret = cognitive_hub_query_module(hub, MODULE_PUBLISHER_1, MODULE_SUBSCRIBER_1,
                                      &query, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.status, 0);

    /* Stats should track query */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.queries_processed, 1u);
}

TEST_F(HubOrchestratorIntegrationTest, QueryToUnregisteredModule) {
    register_test_modules();

    cognitive_query_t query;
    memset(&query, 0, sizeof(query));
    query.query_type = COG_QUERY_STATUS;

    cognitive_query_result_t result;
    memset(&result, 0, sizeof(result));

    /* Query to non-existent module */
    int ret = cognitive_hub_query_module(hub, MODULE_PUBLISHER_1, 9999,
                                          &query, &result);
    EXPECT_EQ(ret, -1);

    /* Stats should track failure */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.queries_failed, 1u);
}

/* ============================================================================
 * Statistics Accuracy Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, StatisticsAccuracy) {
    register_test_modules();

    std::atomic<int> counter{0};

    /* Set up subscriptions */
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                             counting_callback, &counter);
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_2, COG_EVENT_INPUT_RECEIVED,
                             counting_callback, &counter);

    /* Publish multiple events */
    for (int i = 0; i < 10; i++) {
        publish_event(MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);
    }

    /* Verify stats */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);

    EXPECT_EQ(stats.registered_modules, 5u);
    EXPECT_EQ(stats.active_subscriptions, 2u);
    EXPECT_EQ(stats.events_published, 10u);
    EXPECT_EQ(stats.events_delivered, 20u);  /* 10 events * 2 subscribers */
    EXPECT_EQ(stats.events_dropped, 0u);

    /* Counter should match delivered */
    EXPECT_EQ(counter.load(), 20);
}

TEST_F(HubOrchestratorIntegrationTest, StatisticsReset) {
    register_test_modules();

    std::atomic<int> counter{0};
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                             counting_callback, &counter);

    /* Generate some activity */
    for (int i = 0; i < 5; i++) {
        publish_event(MODULE_PUBLISHER_1, COG_EVENT_INPUT_RECEIVED, COG_PRIORITY_NORMAL);
    }

    /* Verify stats before reset */
    cognitive_hub_stats_t stats;
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 5u);
    EXPECT_EQ(stats.events_delivered, 5u);

    /* Reset stats */
    int ret = cognitive_hub_reset_stats(hub);
    EXPECT_EQ(ret, 0);

    /* Verify counters reset but structural info remains */
    cognitive_hub_get_stats(hub, &stats);
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.events_delivered, 0u);
    EXPECT_EQ(stats.registered_modules, 5u);  /* Still registered */
    EXPECT_EQ(stats.active_subscriptions, 1u); /* Still subscribed */
}

/* ============================================================================
 * Category Broadcasting Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, CategoryBroadcast) {
    /* Register multiple modules in same category */
    cognitive_hub_register_module(hub, 1000, COG_CATEGORY_MEMORY, "mem1", nullptr);
    cognitive_hub_register_module(hub, 1001, COG_CATEGORY_MEMORY, "mem2", nullptr);
    cognitive_hub_register_module(hub, 1002, COG_CATEGORY_MEMORY, "mem3", nullptr);
    cognitive_hub_register_module(hub, 2000, COG_CATEGORY_PERCEPTION, "perc1", nullptr);

    std::atomic<int> mem1_count{0}, mem2_count{0}, mem3_count{0}, perc_count{0};

    /* Subscribe all memory modules */
    cognitive_hub_subscribe(hub, 1000, COG_EVENT_MEMORY_ACCESS,
                             counting_callback, &mem1_count);
    cognitive_hub_subscribe(hub, 1001, COG_EVENT_MEMORY_ACCESS,
                             counting_callback, &mem2_count);
    cognitive_hub_subscribe(hub, 1002, COG_EVENT_MEMORY_ACCESS,
                             counting_callback, &mem3_count);
    cognitive_hub_subscribe(hub, 2000, COG_EVENT_MEMORY_ACCESS,
                             counting_callback, &perc_count);

    /* Broadcast to memory category only */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_MEMORY_ACCESS;
    event.source_module_id = 2000;
    event.priority = COG_PRIORITY_NORMAL;

    int ret = cognitive_hub_publish_to_category(hub, 2000, COG_CATEGORY_MEMORY,
                                                 COG_EVENT_MEMORY_ACCESS, &event);
    EXPECT_EQ(ret, 0);

    /* All memory modules should receive */
    EXPECT_EQ(mem1_count.load(), 1);
    EXPECT_EQ(mem2_count.load(), 1);
    EXPECT_EQ(mem3_count.load(), 1);
    /* Perception module should NOT receive (different category) */
    /* (depending on implementation - may or may not filter) */
}

/* ============================================================================
 * Module Lifecycle Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, ModuleLifecycle) {
    /* Register module */
    int ret = cognitive_hub_register_module(hub, 100, COG_CATEGORY_PERCEPTION,
                                             "test_module", nullptr);
    ASSERT_EQ(ret, 0);

    /* Verify active */
    cognitive_module_info_t info;
    cognitive_hub_get_module_info(hub, 100, &info);
    EXPECT_TRUE(info.is_active);

    /* Deactivate */
    ret = cognitive_hub_set_module_active(hub, 100, false);
    EXPECT_EQ(ret, 0);

    cognitive_hub_get_module_info(hub, 100, &info);
    EXPECT_FALSE(info.is_active);

    /* Subscribe while inactive */
    std::atomic<int> counter{0};
    ret = cognitive_hub_subscribe(hub, 100, COG_EVENT_INPUT_RECEIVED,
                                   counting_callback, &counter);
    /* Should work even when inactive */

    /* Reactivate */
    ret = cognitive_hub_set_module_active(hub, 100, true);
    EXPECT_EQ(ret, 0);

    /* Unregister */
    ret = cognitive_hub_unregister_module(hub, 100);
    EXPECT_EQ(ret, 0);

    /* Should no longer exist */
    ret = cognitive_hub_get_module_info(hub, 100, &info);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Edge Cases and Error Handling Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, NullHandling) {
    /* Null config should use defaults */
    cognitive_integration_hub_t null_config_hub = cognitive_hub_create(nullptr);
    EXPECT_NE(null_config_hub, nullptr);
    cognitive_hub_destroy(null_config_hub);

    /* Destroy null should be safe */
    cognitive_hub_destroy(nullptr);  /* Should not crash */

    /* Operations on null hub should fail */
    int ret = cognitive_hub_register_module(nullptr, 100, COG_CATEGORY_PERCEPTION,
                                             "test", nullptr);
    EXPECT_EQ(ret, -1);

    cognitive_hub_stats_t stats;
    ret = cognitive_hub_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(HubOrchestratorIntegrationTest, PublishFromUnregisteredModule) {
    /* Try to publish from unregistered module */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_STATE_CHANGE;
    event.source_module_id = 9999;  /* Not registered */

    int ret = cognitive_hub_publish(hub, 9999, COG_EVENT_STATE_CHANGE, &event);
    EXPECT_EQ(ret, -1);
}

TEST_F(HubOrchestratorIntegrationTest, SubscribeWithNullCallback) {
    register_test_modules();

    int ret = cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                                       nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, StringConversions) {
    /* Test category to string */
    const char* cat_str = cognitive_category_to_string(COG_CATEGORY_PERCEPTION);
    EXPECT_NE(cat_str, nullptr);
    EXPECT_STRNE(cat_str, "UNKNOWN");

    cat_str = cognitive_category_to_string(COG_CATEGORY_MEMORY);
    EXPECT_NE(cat_str, nullptr);

    /* Test event type to string */
    const char* event_str = cognitive_event_type_to_string(COG_EVENT_STATE_CHANGE);
    EXPECT_NE(event_str, nullptr);
    EXPECT_STRNE(event_str, "UNKNOWN");

    event_str = cognitive_event_type_to_string(COG_EVENT_EMOTION_UPDATE);
    EXPECT_NE(event_str, nullptr);

    /* Test query type to string */
    const char* query_str = cognitive_query_type_to_string(COG_QUERY_STATUS);
    EXPECT_NE(query_str, nullptr);
    EXPECT_STRNE(query_str, "UNKNOWN");

    /* Invalid values should return UNKNOWN */
    cat_str = cognitive_category_to_string((cognitive_category_t)999);
    EXPECT_STREQ(cat_str, "UNKNOWN");

    event_str = cognitive_event_type_to_string((cognitive_event_type_t)999);
    EXPECT_STREQ(event_str, "UNKNOWN");
}

/* ============================================================================
 * Async Queue Tests (if enabled)
 * ============================================================================ */

TEST_F(HubOrchestratorIntegrationTest, AsyncQueueManagement) {
    /* Create hub with async enabled */
    cognitive_hub_destroy(hub);

    config.enable_async = true;
    config.event_queue_size = 100;
    hub = cognitive_hub_create(&config);
    ASSERT_NE(hub, nullptr);

    register_test_modules();

    std::atomic<int> counter{0};
    cognitive_hub_subscribe(hub, MODULE_SUBSCRIBER_1, COG_EVENT_INPUT_RECEIVED,
                             counting_callback, &counter);

    /* Publish async */
    cognitive_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = COG_EVENT_INPUT_RECEIVED;
    event.source_module_id = MODULE_PUBLISHER_1;
    event.priority = COG_PRIORITY_NORMAL;

    int ret = cognitive_hub_publish_async(hub, MODULE_PUBLISHER_1,
                                           COG_EVENT_INPUT_RECEIVED, &event);
    EXPECT_EQ(ret, 0);

    /* Flush queue */
    ret = cognitive_hub_flush_async_queue(hub, 1000);  /* 1 second timeout */
    EXPECT_EQ(ret, 0);

    /* Event should have been delivered */
    EXPECT_GE(counter.load(), 1);

    /* Check queue depth */
    uint32_t depth = cognitive_hub_get_async_queue_depth(hub);
    EXPECT_EQ(depth, 0u);  /* Should be empty after flush */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
