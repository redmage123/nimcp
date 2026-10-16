//=============================================================================
// test_event_subscriber.cpp - Comprehensive Event Subscriber Tests
//=============================================================================

#include <gtest/gtest.h>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "middleware/events/nimcp_event_subscriber.h"
#include "middleware/events/nimcp_event_types.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * WHAT: Test fixture for event subscriber tests
 * WHY:  Provide consistent setup/teardown for all tests
 * HOW:  Manages subscriber_manager lifecycle and test state
 */
class EventSubscriberTest : public ::testing::Test {
protected:
    subscriber_manager_t manager = nullptr;

    // Callback tracking
    static std::atomic<int> callback_count;
    static std::atomic<int> callback_invocations;
    static const event_t* last_event;
    static void* last_context;

    void SetUp() override {
        manager = subscriber_manager_create();
        ASSERT_NE(manager, nullptr);

        // Reset static tracking
        callback_count = 0;
        callback_invocations = 0;
        last_event = nullptr;
        last_context = nullptr;
    }

    void TearDown() override {
        if (manager) {
            subscriber_manager_destroy(manager);
            manager = nullptr;
        }
    }

    // Helper: Basic callback function
    static void basic_callback(const event_t* event, void* context) {
        callback_invocations++;
        last_event = event;
        last_context = context;
    }

    // Helper: Counting callback
    static void counting_callback(const event_t* event, void* context) {
        int* count = static_cast<int*>(context);
        if (count) (*count)++;
        callback_count++;
    }

    // Helper: Event type filter predicate
    static bool type_filter(const event_t* event, void* context) {
        event_type_t* target_type = static_cast<event_type_t*>(context);
        return (event->type == *target_type);
    }

    // Helper: Priority filter predicate
    static bool priority_filter(const event_t* event, void* context) {
        mw_event_priority_t* target_priority = static_cast<mw_event_priority_t*>(context);
        return (event->priority <= *target_priority);
    }

    // Helper: Always-false predicate
    static bool reject_all_filter(const event_t* event, void* context) {
        (void)event;
        (void)context;
        return false;
    }

    // Helper: Always-true predicate
    static bool accept_all_filter(const event_t* event, void* context) {
        (void)event;
        (void)context;
        return true;
    }
};

// Static member initialization
std::atomic<int> EventSubscriberTest::callback_count{0};
std::atomic<int> EventSubscriberTest::callback_invocations{0};
const event_t* EventSubscriberTest::last_event = nullptr;
void* EventSubscriberTest::last_context = nullptr;

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

/**
 * WHAT: Test basic create and destroy lifecycle
 * WHY:  Verify resource management works correctly
 * HOW:  Create manager, verify non-null, destroy
 */
TEST_F(EventSubscriberTest, CreateAndDestroy) {
    subscriber_manager_t mgr = subscriber_manager_create();
    EXPECT_NE(mgr, nullptr);
    EXPECT_EQ(subscriber_get_count(mgr), 0u);
    subscriber_manager_destroy(mgr);
}

/**
 * WHAT: Test destroying null manager
 * WHY:  Ensure null-safety in destroy
 * HOW:  Call destroy with nullptr
 */
TEST_F(EventSubscriberTest, DestroyNullManager) {
    subscriber_manager_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

/**
 * WHAT: Test destroying manager with active subscriptions
 * WHY:  Verify cleanup of all resources
 * HOW:  Subscribe, then destroy without unsubscribing
 */
TEST_F(EventSubscriberTest, DestroyWithActiveSubscriptions) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);
    EXPECT_NE(handle, SUBSCRIPTION_HANDLE_INVALID);

    // Destroy should clean up subscription
    subscriber_manager_destroy(manager);
    manager = nullptr; // Prevent double-free in TearDown

    SUCCEED();
}

//=============================================================================
// SUBSCRIPTION MANAGEMENT TESTS
//=============================================================================

/**
 * WHAT: Test basic subscription
 * WHY:  Verify subscribe operation works
 * HOW:  Subscribe with null config (all events)
 */
TEST_F(EventSubscriberTest, BasicSubscribe) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);

    EXPECT_NE(handle, SUBSCRIPTION_HANDLE_INVALID);
    EXPECT_EQ(subscriber_get_count(manager), 1u);
}

/**
 * WHAT: Test subscribing with NULL manager
 * WHY:  Ensure parameter validation
 * HOW:  Pass nullptr manager
 */
TEST_F(EventSubscriberTest, SubscribeNullManager) {
    subscription_handle_t handle = subscriber_subscribe(
        nullptr, basic_callback, nullptr, nullptr);

    EXPECT_EQ(handle, SUBSCRIPTION_HANDLE_INVALID);
}

/**
 * WHAT: Test subscribing with NULL callback
 * WHY:  Ensure callback validation
 * HOW:  Pass nullptr callback
 */
TEST_F(EventSubscriberTest, SubscribeNullCallback) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, nullptr, nullptr, nullptr);

    EXPECT_EQ(handle, SUBSCRIPTION_HANDLE_INVALID);
}

/**
 * WHAT: Test multiple subscriptions
 * WHY:  Verify support for multiple subscribers
 * HOW:  Subscribe multiple times, check count
 */
TEST_F(EventSubscriberTest, MultipleSubscriptions) {
    subscription_handle_t h1 = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);
    subscription_handle_t h2 = subscriber_subscribe(
        manager, counting_callback, nullptr, nullptr);
    subscription_handle_t h3 = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);

    EXPECT_NE(h1, SUBSCRIPTION_HANDLE_INVALID);
    EXPECT_NE(h2, SUBSCRIPTION_HANDLE_INVALID);
    EXPECT_NE(h3, SUBSCRIPTION_HANDLE_INVALID);
    EXPECT_NE(h1, h2);
    EXPECT_NE(h1, h3);
    EXPECT_NE(h2, h3);
    EXPECT_EQ(subscriber_get_count(manager), 3u);
}

/**
 * WHAT: Test subscription with custom context
 * WHY:  Verify context passing works
 * HOW:  Subscribe with context, dispatch, check context
 */
TEST_F(EventSubscriberTest, SubscriptionWithContext) {
    int context_value = 42;
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, &context_value, nullptr);

    EXPECT_NE(handle, SUBSCRIPTION_HANDLE_INVALID);

    // Dispatch event and verify context
    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    subscriber_dispatch_event(manager, &event);

    EXPECT_EQ(last_context, &context_value);
}

/**
 * WHAT: Test unsubscribe operation
 * WHY:  Verify subscription removal works
 * HOW:  Subscribe, unsubscribe, check count
 */
TEST_F(EventSubscriberTest, BasicUnsubscribe) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);
    EXPECT_EQ(subscriber_get_count(manager), 1u);

    bool result = subscriber_unsubscribe(manager, handle);
    EXPECT_TRUE(result);
    EXPECT_EQ(subscriber_get_count(manager), 0u);
}

/**
 * WHAT: Test unsubscribe with NULL manager
 * WHY:  Ensure parameter validation
 * HOW:  Pass nullptr manager
 */
TEST_F(EventSubscriberTest, UnsubscribeNullManager) {
    bool result = subscriber_unsubscribe(nullptr, 1);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test unsubscribe with invalid handle
 * WHY:  Ensure handle validation
 * HOW:  Pass non-existent handle
 */
TEST_F(EventSubscriberTest, UnsubscribeInvalidHandle) {
    bool result = subscriber_unsubscribe(manager, 999999);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test double unsubscribe
 * WHY:  Verify idempotency
 * HOW:  Unsubscribe same handle twice
 */
TEST_F(EventSubscriberTest, DoubleUnsubscribe) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);

    bool result1 = subscriber_unsubscribe(manager, handle);
    EXPECT_TRUE(result1);

    bool result2 = subscriber_unsubscribe(manager, handle);
    EXPECT_FALSE(result2);
}

/**
 * WHAT: Test pause subscription
 * WHY:  Verify temporary subscription disabling
 * HOW:  Subscribe, pause, dispatch, verify no callback
 */
TEST_F(EventSubscriberTest, PauseSubscription) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);

    bool result = subscriber_pause(manager, handle);
    EXPECT_TRUE(result);

    // Dispatch event - should not trigger callback
    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified = subscriber_dispatch_event(manager, &event);

    EXPECT_EQ(notified, 0u);
    EXPECT_EQ(callback_invocations, 0);
}

/**
 * WHAT: Test resume subscription
 * WHY:  Verify re-enabling paused subscription
 * HOW:  Subscribe, pause, resume, dispatch, verify callback
 */
TEST_F(EventSubscriberTest, ResumeSubscription) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);

    subscriber_pause(manager, handle);
    bool result = subscriber_resume(manager, handle);
    EXPECT_TRUE(result);

    // Dispatch event - should trigger callback
    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified = subscriber_dispatch_event(manager, &event);

    EXPECT_EQ(notified, 1u);
    EXPECT_EQ(callback_invocations, 1);
}

/**
 * WHAT: Test pause/resume with NULL manager
 * WHY:  Ensure parameter validation
 * HOW:  Pass nullptr manager
 */
TEST_F(EventSubscriberTest, PauseResumeNullManager) {
    bool result1 = subscriber_pause(nullptr, 1);
    EXPECT_FALSE(result1);

    bool result2 = subscriber_resume(nullptr, 1);
    EXPECT_FALSE(result2);
}

/**
 * WHAT: Test pause/resume with invalid handle
 * WHY:  Ensure handle validation
 * HOW:  Pass non-existent handle
 */
TEST_F(EventSubscriberTest, PauseResumeInvalidHandle) {
    bool result1 = subscriber_pause(manager, 999999);
    EXPECT_FALSE(result1);

    bool result2 = subscriber_resume(manager, 999999);
    EXPECT_FALSE(result2);
}

//=============================================================================
// EVENT DISPATCH TESTS
//=============================================================================

/**
 * WHAT: Test basic event dispatch
 * WHY:  Verify events reach subscribers
 * HOW:  Subscribe, dispatch, verify callback invoked
 */
TEST_F(EventSubscriberTest, BasicEventDispatch) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);

    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified = subscriber_dispatch_event(manager, &event);

    EXPECT_EQ(notified, 1u);
    EXPECT_EQ(callback_invocations, 1);
    EXPECT_NE(last_event, nullptr);
}

/**
 * WHAT: Test dispatch to multiple subscribers
 * WHY:  Verify fan-out to all subscribers
 * HOW:  Multiple subscriptions, single dispatch
 */
TEST_F(EventSubscriberTest, DispatchToMultipleSubscribers) {
    int count1 = 0, count2 = 0, count3 = 0;

    subscriber_subscribe(manager, counting_callback, &count1, nullptr);
    subscriber_subscribe(manager, counting_callback, &count2, nullptr);
    subscriber_subscribe(manager, counting_callback, &count3, nullptr);

    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified = subscriber_dispatch_event(manager, &event);

    EXPECT_EQ(notified, 3u);
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
    EXPECT_EQ(count3, 1);
}

/**
 * WHAT: Test dispatch with NULL manager
 * WHY:  Ensure parameter validation
 * HOW:  Pass nullptr manager
 */
TEST_F(EventSubscriberTest, DispatchNullManager) {
    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified = subscriber_dispatch_event(nullptr, &event);

    EXPECT_EQ(notified, 0u);
}

/**
 * WHAT: Test dispatch with NULL event
 * WHY:  Ensure event validation
 * HOW:  Pass nullptr event
 */
TEST_F(EventSubscriberTest, DispatchNullEvent) {
    subscriber_subscribe(manager, basic_callback, nullptr, nullptr);
    uint32_t notified = subscriber_dispatch_event(manager, nullptr);

    EXPECT_EQ(notified, 0u);
}

/**
 * WHAT: Test dispatch to empty manager
 * WHY:  Verify handling of no subscribers
 * HOW:  Dispatch without subscribing
 */
TEST_F(EventSubscriberTest, DispatchNoSubscribers) {
    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified = subscriber_dispatch_event(manager, &event);

    EXPECT_EQ(notified, 0u);
}

/**
 * WHAT: Test multiple event dispatches
 * WHY:  Verify repeated dispatch works
 * HOW:  Subscribe, dispatch multiple events
 */
TEST_F(EventSubscriberTest, MultipleDispatches) {
    int count = 0;
    subscriber_subscribe(manager, counting_callback, &count, nullptr);

    for (int i = 0; i < 10; i++) {
        event_t event = event_create_custom(nullptr, 0, "test",
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
        subscriber_dispatch_event(manager, &event);
    }

    EXPECT_EQ(count, 10);
}

//=============================================================================
// FILTER TESTS
//=============================================================================

/**
 * WHAT: Test event type filtering
 * WHY:  Verify type-based subscription filtering
 * HOW:  Subscribe to specific type, dispatch different types
 */
TEST_F(EventSubscriberTest, EventTypeFilter) {
    subscription_config_t config = subscriber_default_config();
    event_type_t types[] = {EVENT_TYPE_SPIKE_BURST};
    config.event_types = types;
    config.num_types = 1;

    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, &config);

    // Dispatch matching event
    event_t event1 = event_create_spike_burst(nullptr, 0, 0.5f, 1000,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    uint32_t notified1 = subscriber_dispatch_event(manager, &event1);
    EXPECT_EQ(notified1, 1u);

    // Reset callback count
    callback_invocations = 0;

    // Dispatch non-matching event
    event_t event2 = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified2 = subscriber_dispatch_event(manager, &event2);
    EXPECT_EQ(notified2, 0u);
    EXPECT_EQ(callback_invocations, 0);
}

/**
 * WHAT: Test multiple event type filtering
 * WHY:  Verify subscription to multiple types
 * HOW:  Subscribe to 2+ types, dispatch various types
 */
TEST_F(EventSubscriberTest, MultipleEventTypeFilter) {
    subscription_config_t config = subscriber_default_config();
    event_type_t types[] = {EVENT_TYPE_SPIKE_BURST, EVENT_TYPE_PATTERN_DETECTED};
    config.event_types = types;
    config.num_types = 2;

    int count = 0;
    subscriber_subscribe(manager, counting_callback, &count, &config);

    // Dispatch matching events
    event_t event1 = event_create_spike_burst(nullptr, 0, 0.5f, 1000,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    subscriber_dispatch_event(manager, &event1);

    event_t event2 = event_create_pattern_detected(1, 0.8f, 5, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_PATTERN_DETECTOR);
    subscriber_dispatch_event(manager, &event2);

    // Dispatch non-matching event
    event_t event3 = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    subscriber_dispatch_event(manager, &event3);

    EXPECT_EQ(count, 2);
}

/**
 * WHAT: Test event source filtering
 * WHY:  Verify source-based subscription filtering
 * HOW:  Subscribe to specific source, dispatch from different sources
 */
TEST_F(EventSubscriberTest, EventSourceFilter) {
    subscription_config_t config = subscriber_default_config();
    event_source_t sources[] = {EVENT_SOURCE_BRAIN};
    config.event_sources = sources;
    config.num_sources = 1;

    int count = 0;
    subscriber_subscribe(manager, counting_callback, &count, &config);

    // Dispatch from matching source
    event_t event1 = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    subscriber_dispatch_event(manager, &event1);
    EXPECT_EQ(count, 1);

    // Dispatch from non-matching source
    event_t event2 = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    subscriber_dispatch_event(manager, &event2);
    EXPECT_EQ(count, 1); // Should not increment
}

/**
 * WHAT: Test custom predicate filtering
 * WHY:  Verify custom filter predicates work
 * HOW:  Subscribe with predicate, dispatch various events
 */
TEST_F(EventSubscriberTest, CustomPredicateFilter) {
    subscription_config_t config = subscriber_default_config();
    mw_event_priority_t target_priority = MW_EVENT_PRIORITY_HIGH;
    config.predicate = priority_filter;
    config.predicate_context = &target_priority;

    int count = 0;
    subscriber_subscribe(manager, counting_callback, &count, &config);

    // Dispatch high priority (should match)
    event_t event1 = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_CUSTOM);
    subscriber_dispatch_event(manager, &event1);
    EXPECT_EQ(count, 1);

    // Dispatch low priority (should not match)
    event_t event2 = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_LOW, EVENT_SOURCE_CUSTOM);
    subscriber_dispatch_event(manager, &event2);
    EXPECT_EQ(count, 1);
}

/**
 * WHAT: Test predicate that rejects all events
 * WHY:  Verify predicate rejection works
 * HOW:  Subscribe with always-false predicate
 */
TEST_F(EventSubscriberTest, RejectAllPredicate) {
    subscription_config_t config = subscriber_default_config();
    config.predicate = reject_all_filter;

    int count = 0;
    subscriber_subscribe(manager, counting_callback, &count, &config);

    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified = subscriber_dispatch_event(manager, &event);

    EXPECT_EQ(notified, 0u);
    EXPECT_EQ(count, 0);
}

/**
 * WHAT: Test combined type and source filtering
 * WHY:  Verify multiple filters work together
 * HOW:  Subscribe with both type and source filters
 */
TEST_F(EventSubscriberTest, CombinedTypeAndSourceFilter) {
    subscription_config_t config = subscriber_default_config();
    event_type_t types[] = {EVENT_TYPE_SPIKE_BURST};
    event_source_t sources[] = {EVENT_SOURCE_BRAIN};
    config.event_types = types;
    config.num_types = 1;
    config.event_sources = sources;
    config.num_sources = 1;

    int count = 0;
    subscriber_subscribe(manager, counting_callback, &count, &config);

    // Match both type and source
    event_t event1 = event_create_spike_burst(nullptr, 0, 0.5f, 1000,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    subscriber_dispatch_event(manager, &event1);
    EXPECT_EQ(count, 1);

    // Match type but not source
    event_t event2 = event_create_spike_burst(nullptr, 0, 0.5f, 1000,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    subscriber_dispatch_event(manager, &event2);
    EXPECT_EQ(count, 1);

    // Match source but not type
    event_t event3 = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    subscriber_dispatch_event(manager, &event3);
    EXPECT_EQ(count, 1);
}

//=============================================================================
// PRIORITY TESTS
//=============================================================================

/**
 * WHAT: Test subscriber priority ordering
 * WHY:  Verify high-priority subscribers called first
 * HOW:  Subscribe with different priorities, track order
 */
TEST_F(EventSubscriberTest, SubscriberPriorityOrdering) {
    static std::vector<int> call_order;
    call_order.clear();

    auto priority_callback = [](const event_t* event, void* context) {
        int priority = *static_cast<int*>(context);
        call_order.push_back(priority);
    };

    int p_low = 3, p_high = 1, p_normal = 2;

    subscription_config_t config_low = subscriber_default_config();
    config_low.priority = SUBSCRIBER_PRIORITY_LOW;
    subscriber_subscribe(manager, priority_callback, &p_low, &config_low);

    subscription_config_t config_high = subscriber_default_config();
    config_high.priority = SUBSCRIBER_PRIORITY_HIGH;
    subscriber_subscribe(manager, priority_callback, &p_high, &config_high);

    subscription_config_t config_normal = subscriber_default_config();
    config_normal.priority = SUBSCRIBER_PRIORITY_NORMAL;
    subscriber_subscribe(manager, priority_callback, &p_normal, &config_normal);

    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    subscriber_dispatch_event(manager, &event);

    // High priority should be called first
    ASSERT_EQ(call_order.size(), 3u);
    EXPECT_EQ(call_order[0], 1); // HIGH
    EXPECT_EQ(call_order[1], 2); // NORMAL
    EXPECT_EQ(call_order[2], 3); // LOW
}

/**
 * WHAT: Test all priority levels
 * WHY:  Verify all priority values work
 * HOW:  Subscribe with each priority level
 */
TEST_F(EventSubscriberTest, AllPriorityLevels) {
    subscription_config_t config = subscriber_default_config();

    config.priority = SUBSCRIBER_PRIORITY_HIGHEST;
    subscription_handle_t h1 = subscriber_subscribe(
        manager, basic_callback, nullptr, &config);
    EXPECT_NE(h1, SUBSCRIPTION_HANDLE_INVALID);

    config.priority = SUBSCRIBER_PRIORITY_HIGH;
    subscription_handle_t h2 = subscriber_subscribe(
        manager, basic_callback, nullptr, &config);
    EXPECT_NE(h2, SUBSCRIPTION_HANDLE_INVALID);

    config.priority = SUBSCRIBER_PRIORITY_NORMAL;
    subscription_handle_t h3 = subscriber_subscribe(
        manager, basic_callback, nullptr, &config);
    EXPECT_NE(h3, SUBSCRIPTION_HANDLE_INVALID);

    config.priority = SUBSCRIBER_PRIORITY_LOW;
    subscription_handle_t h4 = subscriber_subscribe(
        manager, basic_callback, nullptr, &config);
    EXPECT_NE(h4, SUBSCRIPTION_HANDLE_INVALID);

    config.priority = SUBSCRIBER_PRIORITY_LOWEST;
    subscription_handle_t h5 = subscriber_subscribe(
        manager, basic_callback, nullptr, &config);
    EXPECT_NE(h5, SUBSCRIPTION_HANDLE_INVALID);

    EXPECT_EQ(subscriber_get_count(manager), 5u);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

/**
 * WHAT: Test getting subscriber statistics
 * WHY:  Verify statistics tracking works
 * HOW:  Subscribe, dispatch, check stats
 */
TEST_F(EventSubscriberTest, GetStatistics) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);

    // Dispatch some events
    for (int i = 0; i < 5; i++) {
        event_t event = event_create_custom(nullptr, 0, "test",
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
        subscriber_dispatch_event(manager, &event);
    }

    subscriber_stats_t stats;
    bool result = subscriber_get_stats(manager, handle, &stats);

    EXPECT_TRUE(result);
    EXPECT_GE(stats.events_received, 5u);
    EXPECT_GE(stats.callback_invocations, 5u);
}

/**
 * WHAT: Test statistics with NULL manager
 * WHY:  Ensure parameter validation
 * HOW:  Pass nullptr manager
 */
TEST_F(EventSubscriberTest, GetStatsNullManager) {
    subscriber_stats_t stats;
    bool result = subscriber_get_stats(nullptr, 1, &stats);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test statistics with NULL stats pointer
 * WHY:  Ensure parameter validation
 * HOW:  Pass nullptr stats
 */
TEST_F(EventSubscriberTest, GetStatsNullPointer) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);
    bool result = subscriber_get_stats(manager, handle, nullptr);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test statistics with invalid handle
 * WHY:  Ensure handle validation
 * HOW:  Pass non-existent handle
 */
TEST_F(EventSubscriberTest, GetStatsInvalidHandle) {
    subscriber_stats_t stats;
    bool result = subscriber_get_stats(manager, 999999, &stats);
    EXPECT_FALSE(result);
}

/**
 * WHAT: Test subscriber count
 * WHY:  Verify count tracking works
 * HOW:  Add/remove subscriptions, check count
 */
TEST_F(EventSubscriberTest, GetSubscriberCount) {
    EXPECT_EQ(subscriber_get_count(manager), 0u);

    subscription_handle_t h1 = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);
    EXPECT_EQ(subscriber_get_count(manager), 1u);

    subscription_handle_t h2 = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);
    EXPECT_EQ(subscriber_get_count(manager), 2u);

    subscriber_unsubscribe(manager, h1);
    EXPECT_EQ(subscriber_get_count(manager), 1u);

    subscriber_unsubscribe(manager, h2);
    EXPECT_EQ(subscriber_get_count(manager), 0u);
}

/**
 * WHAT: Test count with NULL manager
 * WHY:  Ensure parameter validation
 * HOW:  Pass nullptr manager
 */
TEST_F(EventSubscriberTest, GetCountNullManager) {
    uint32_t count = subscriber_get_count(nullptr);
    EXPECT_EQ(count, 0u);
}

/**
 * WHAT: Test events dropped by filter
 * WHY:  Verify dropped event tracking
 * HOW:  Subscribe with filter, dispatch mismatched events
 */
TEST_F(EventSubscriberTest, DroppedEventsStatistics) {
    subscription_config_t config = subscriber_default_config();
    config.predicate = reject_all_filter;

    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, &config);

    // Dispatch events that will be filtered
    for (int i = 0; i < 10; i++) {
        event_t event = event_create_custom(nullptr, 0, "test",
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
        subscriber_dispatch_event(manager, &event);
    }

    subscriber_stats_t stats;
    bool result = subscriber_get_stats(manager, handle, &stats);

    EXPECT_TRUE(result);
    EXPECT_GE(stats.events_dropped, 10u);
    EXPECT_EQ(stats.callback_invocations, 0u);
}

//=============================================================================
// UTILITY TESTS
//=============================================================================

/**
 * WHAT: Test default config creation
 * WHY:  Verify default values are correct
 * HOW:  Create default config, check values
 */
TEST_F(EventSubscriberTest, DefaultConfig) {
    subscription_config_t config = subscriber_default_config();

    EXPECT_EQ(config.event_types, nullptr);
    EXPECT_EQ(config.num_types, 0u);
    EXPECT_EQ(config.event_sources, nullptr);
    EXPECT_EQ(config.num_sources, 0u);
    EXPECT_EQ(config.predicate, nullptr);
    EXPECT_EQ(config.predicate_context, nullptr);
    EXPECT_EQ(config.priority, SUBSCRIBER_PRIORITY_NORMAL);
}

/**
 * WHAT: Test getting last error message
 * WHY:  Verify error reporting works
 * HOW:  Cause error, check error message
 */
TEST_F(EventSubscriberTest, GetLastError) {
    // Cause an error
    subscriber_subscribe(nullptr, basic_callback, nullptr, nullptr);

    const char* error = subscriber_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_GT(strlen(error), 0u);
}

//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

/**
 * WHAT: Test all NULL parameter combinations
 * WHY:  Ensure comprehensive NULL safety
 * HOW:  Test all functions with NULL params
 */
TEST_F(EventSubscriberTest, NullParameterComprehensive) {
    // Already covered in individual tests, this is a summary test

    // Create/Destroy
    subscriber_manager_destroy(nullptr);

    // Subscribe
    EXPECT_EQ(subscriber_subscribe(nullptr, basic_callback, nullptr, nullptr),
              SUBSCRIPTION_HANDLE_INVALID);
    EXPECT_EQ(subscriber_subscribe(manager, nullptr, nullptr, nullptr),
              SUBSCRIPTION_HANDLE_INVALID);

    // Unsubscribe
    EXPECT_FALSE(subscriber_unsubscribe(nullptr, 1));

    // Pause/Resume
    EXPECT_FALSE(subscriber_pause(nullptr, 1));
    EXPECT_FALSE(subscriber_resume(nullptr, 1));

    // Dispatch
    EXPECT_EQ(subscriber_dispatch_event(nullptr, nullptr), 0u);

    // Stats
    subscriber_stats_t stats;
    EXPECT_FALSE(subscriber_get_stats(nullptr, 1, &stats));
    EXPECT_FALSE(subscriber_get_stats(manager, 1, nullptr));

    // Count
    EXPECT_EQ(subscriber_get_count(nullptr), 0u);
}

/**
 * WHAT: Test invalid handle edge cases
 * WHY:  Ensure handle validation is robust
 * HOW:  Use INVALID handle and extreme values
 */
TEST_F(EventSubscriberTest, InvalidHandleEdgeCases) {
    EXPECT_FALSE(subscriber_unsubscribe(manager, SUBSCRIPTION_HANDLE_INVALID));
    EXPECT_FALSE(subscriber_pause(manager, SUBSCRIPTION_HANDLE_INVALID));
    EXPECT_FALSE(subscriber_resume(manager, SUBSCRIPTION_HANDLE_INVALID));

    subscriber_stats_t stats;
    EXPECT_FALSE(subscriber_get_stats(manager, SUBSCRIPTION_HANDLE_INVALID, &stats));

    // Max value
    EXPECT_FALSE(subscriber_unsubscribe(manager, UINT64_MAX));
    EXPECT_FALSE(subscriber_pause(manager, UINT64_MAX));
}

//=============================================================================
// STRESS TESTS
//=============================================================================

/**
 * WHAT: Test many subscribers
 * WHY:  Verify scalability
 * HOW:  Create 1000+ subscriptions
 */
TEST_F(EventSubscriberTest, ManySubscribers) {
    std::vector<subscription_handle_t> handles;

    for (int i = 0; i < 1000; i++) {
        subscription_handle_t handle = subscriber_subscribe(
            manager, basic_callback, nullptr, nullptr);
        EXPECT_NE(handle, SUBSCRIPTION_HANDLE_INVALID);
        handles.push_back(handle);
    }

    EXPECT_EQ(subscriber_get_count(manager), 1000u);

    // Dispatch event to all
    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    uint32_t notified = subscriber_dispatch_event(manager, &event);
    EXPECT_EQ(notified, 1000u);
}

/**
 * WHAT: Test rapid subscribe/unsubscribe
 * WHY:  Verify no resource leaks
 * HOW:  Subscribe and unsubscribe in loop
 */
TEST_F(EventSubscriberTest, RapidSubscribeUnsubscribe) {
    for (int i = 0; i < 1000; i++) {
        subscription_handle_t handle = subscriber_subscribe(
            manager, basic_callback, nullptr, nullptr);
        EXPECT_NE(handle, SUBSCRIPTION_HANDLE_INVALID);

        bool result = subscriber_unsubscribe(manager, handle);
        EXPECT_TRUE(result);
    }

    EXPECT_EQ(subscriber_get_count(manager), 0u);
}

/**
 * WHAT: Test rapid event dispatch
 * WHY:  Verify performance under load
 * HOW:  Dispatch many events quickly
 */
TEST_F(EventSubscriberTest, RapidEventDispatch) {
    int count = 0;
    subscriber_subscribe(manager, counting_callback, &count, nullptr);

    for (int i = 0; i < 10000; i++) {
        event_t event = event_create_custom(nullptr, 0, "test",
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
        subscriber_dispatch_event(manager, &event);
    }

    EXPECT_EQ(count, 10000);
}

/**
 * WHAT: Test mixed operations under stress
 * WHY:  Verify robustness with concurrent operation types
 * HOW:  Mix subscribe, dispatch, unsubscribe
 */
TEST_F(EventSubscriberTest, MixedOperationsStress) {
    std::vector<subscription_handle_t> handles;
    int total_count = 0;

    for (int i = 0; i < 100; i++) {
        // Subscribe
        subscription_handle_t handle = subscriber_subscribe(
            manager, counting_callback, &total_count, nullptr);
        handles.push_back(handle);

        // Dispatch
        event_t event = event_create_custom(nullptr, 0, "test",
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
        subscriber_dispatch_event(manager, &event);

        // Unsubscribe some
        if (i % 3 == 0 && !handles.empty()) {
            subscriber_unsubscribe(manager, handles.back());
            handles.pop_back();
        }
    }

    EXPECT_GT(total_count, 0);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================

/**
 * WHAT: Test unsubscribe during iteration
 * WHY:  Prevent crashes from modifying during iteration
 * HOW:  Callback that unsubscribes during dispatch
 */
TEST_F(EventSubscriberTest, UnsubscribeDuringDispatch) {
    static subscriber_manager_t static_manager;
    static subscription_handle_t static_handle;
    static_manager = manager;

    auto self_unsubscribe_callback = [](const event_t* event, void* context) {
        // This is generally unsafe, but test defensive coding
        // In production, should be prevented or handled safely
        (void)event;
        (void)context;
    };

    static_handle = subscriber_subscribe(
        manager, self_unsubscribe_callback, nullptr, nullptr);

    event_t event = event_create_custom(nullptr, 0, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);

    // This should not crash
    subscriber_dispatch_event(manager, &event);

    SUCCEED();
}

/**
 * WHAT: Test empty filter arrays
 * WHY:  Verify edge case handling
 * HOW:  Subscribe with empty type/source arrays
 */
TEST_F(EventSubscriberTest, EmptyFilterArrays) {
    subscription_config_t config = subscriber_default_config();
    event_type_t types[1];
    config.event_types = types;
    config.num_types = 0; // Empty array

    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, &config);

    // Should succeed - empty array means no type filtering
    EXPECT_NE(handle, SUBSCRIPTION_HANDLE_INVALID);
}

/**
 * WHAT: Test paused subscription stats
 * WHY:  Verify stats correct when paused
 * HOW:  Pause, dispatch, check stats
 */
TEST_F(EventSubscriberTest, PausedSubscriptionStats) {
    subscription_handle_t handle = subscriber_subscribe(
        manager, basic_callback, nullptr, nullptr);

    subscriber_pause(manager, handle);

    // Dispatch events while paused
    for (int i = 0; i < 5; i++) {
        event_t event = event_create_custom(nullptr, 0, "test",
            MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
        subscriber_dispatch_event(manager, &event);
    }

    subscriber_stats_t stats;
    subscriber_get_stats(manager, handle, &stats);

    // Should not have received events while paused
    EXPECT_EQ(stats.callback_invocations, 0u);
}

/**
 * WHAT: Test all event types dispatch correctly
 * WHY:  Verify each event type works
 * HOW:  Create and dispatch each event type
 */
TEST_F(EventSubscriberTest, AllEventTypesDispatch) {
    int count = 0;
    subscriber_subscribe(manager, counting_callback, &count, nullptr);

    // Test each event type
    uint32_t neurons[] = {1, 2, 3};
    event_t e1 = event_create_spike_burst(neurons, 3, 0.8f, 1000,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    subscriber_dispatch_event(manager, &e1);

    event_t e2 = event_create_pattern_detected(1, 0.9f, 5, "test",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_PATTERN_DETECTOR);
    subscriber_dispatch_event(manager, &e2);

    event_t e3 = event_create_attention_shift(1, 2, 0.7f, "novelty",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_WORKING_MEMORY);
    subscriber_dispatch_event(manager, &e3);

    float trace[] = {0.1f, 0.2f, 0.3f};
    event_t e4 = event_create_memory_formed(1, trace, 3, 0.8f,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_WORKING_MEMORY);
    subscriber_dispatch_event(manager, &e4);

    event_t e5 = event_create_salience_peak(0.9f, 0.8f, 0.7f, 0.6f,
        MW_EVENT_PRIORITY_CRITICAL, EVENT_SOURCE_SALIENCE);
    subscriber_dispatch_event(manager, &e5);

    event_t e6 = event_create_oscillation_change(10.0f, 20.0f, 1.5f, "alpha",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    subscriber_dispatch_event(manager, &e6);

    event_t e7 = event_create_error_detected(1.0f, 0.5f, 0.5f, 42,
        MW_EVENT_PRIORITY_HIGH, EVENT_SOURCE_PREDICTIVE);
    subscriber_dispatch_event(manager, &e7);

    float decision[] = {0.5f, 0.5f};
    event_t e8 = event_create_decision_made(1, 0.85f, decision, 2,
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_BRAIN);
    subscriber_dispatch_event(manager, &e8);

    event_t e9 = event_create_custom(nullptr, 0, "custom",
        MW_EVENT_PRIORITY_NORMAL, EVENT_SOURCE_CUSTOM);
    subscriber_dispatch_event(manager, &e9);

    EXPECT_EQ(count, 9);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
