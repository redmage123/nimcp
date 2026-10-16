/**
 * @file test_hypothalamus_orchestrator_integration.cpp
 * @brief Integration tests for Hypothalamus Orchestrator with multiple bridges
 *
 * WHAT: Integration tests verifying the orchestrator correctly coordinates
 *       multiple hypothalamus bridges, manages event flow, and tracks state
 * WHY:  Ensure orchestrator properly mediates inter-bridge communication
 *       and maintains coherent drive state across the hypothalamus subsystem
 * HOW:  Test bridge registration, event publishing, subscription management,
 *       drive state aggregation, and stress response coordination
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>
#include <functional>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_BRIDGE_EMOTION     0x1001
#define TEST_BRIDGE_EXECUTIVE   0x1002
#define TEST_BRIDGE_ATTENTION   0x1003
#define TEST_BRIDGE_SLEEP       0x1004
#define TEST_BRIDGE_WELLBEING   0x1005

/* ============================================================================
 * Event Tracking Structures
 * ============================================================================ */

struct EventTracker {
    std::atomic<int> total_events{0};
    std::atomic<int> drive_activated_events{0};
    std::atomic<int> drive_satisfied_events{0};
    std::atomic<int> stress_events{0};
    std::atomic<int> homeostatic_alerts{0};
    std::vector<hypo_event_type_t> event_types;
    std::vector<hypo_bridge_type_t> source_bridges;
    std::mutex mutex;
};

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

static int event_tracker_callback(const hypo_event_data_t* event, void* user_data) {
    if (!event || !user_data) return -1;

    EventTracker* tracker = static_cast<EventTracker*>(user_data);
    tracker->total_events++;

    switch (event->event_type) {
        case HYPO_EVENT_DRIVE_ACTIVATED:
            tracker->drive_activated_events++;
            break;
        case HYPO_EVENT_DRIVE_SATISFIED:
            tracker->drive_satisfied_events++;
            break;
        case HYPO_EVENT_STRESS_RESPONSE:
            tracker->stress_events++;
            break;
        case HYPO_EVENT_HOMEOSTATIC_ALERT:
            tracker->homeostatic_alerts++;
            break;
        default:
            break;
    }

    {
        std::lock_guard<std::mutex> lock(tracker->mutex);
        tracker->event_types.push_back(event->event_type);
        tracker->source_bridges.push_back(event->source);
    }

    return 0;
}

static int counting_callback(const hypo_event_data_t* event, void* user_data) {
    (void)event;
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(user_data);
    (*counter)++;
    return 0;
}

static int failing_callback(const hypo_event_data_t* event, void* user_data) {
    (void)event;
    (void)user_data;
    return -1;  /* Simulate callback failure */
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypothalamusOrchestratorIntegrationTest : public ::testing::Test {
protected:
    hypo_orchestrator_t orch;
    hypo_orch_config_t config;

    void SetUp() override {
        orch = nullptr;

        /* Get default config */
        ASSERT_EQ(0, hypo_orch_default_config(&config));
        config.max_bridges = 32;
        config.max_subscriptions = 128;
        config.enable_async = false;  /* Synchronous for deterministic tests */

        /* Create orchestrator */
        orch = hypo_orch_create(&config);
        ASSERT_NE(orch, nullptr);
    }

    void TearDown() override {
        if (orch) {
            hypo_orch_destroy(orch);
            orch = nullptr;
        }
    }

    /* Helper to register a set of test bridges */
    void register_test_bridges() {
        uint32_t bridge_id;

        /* Register emotion bridge */
        int ret = hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                             "emotion_bridge",
                                             (void*)TEST_BRIDGE_EMOTION,
                                             nullptr, &bridge_id);
        ASSERT_EQ(0, ret);

        /* Register executive bridge */
        ret = hypo_orch_register_bridge(orch, HYPO_BRIDGE_EXECUTIVE,
                                         "executive_bridge",
                                         (void*)TEST_BRIDGE_EXECUTIVE,
                                         nullptr, &bridge_id);
        ASSERT_EQ(0, ret);

        /* Register attention bridge */
        ret = hypo_orch_register_bridge(orch, HYPO_BRIDGE_ATTENTION,
                                         "attention_bridge",
                                         (void*)TEST_BRIDGE_ATTENTION,
                                         nullptr, &bridge_id);
        ASSERT_EQ(0, ret);

        /* Register sleep bridge */
        ret = hypo_orch_register_bridge(orch, HYPO_BRIDGE_SLEEP,
                                         "sleep_bridge",
                                         (void*)TEST_BRIDGE_SLEEP,
                                         nullptr, &bridge_id);
        ASSERT_EQ(0, ret);

        /* Register wellbeing bridge */
        ret = hypo_orch_register_bridge(orch, HYPO_BRIDGE_WELLBEING,
                                         "wellbeing_bridge",
                                         (void*)TEST_BRIDGE_WELLBEING,
                                         nullptr, &bridge_id);
        ASSERT_EQ(0, ret);
    }

    /* Helper to publish a drive event */
    void publish_drive_event(uint32_t bridge_id, hypo_event_type_t event_type,
                             hypo_urgency_t urgency) {
        hypo_event_data_t event;
        memset(&event, 0, sizeof(event));
        event.event_type = event_type;
        event.source = HYPO_BRIDGE_EMOTION;
        event.urgency = urgency;
        event.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        event.drive.drive_type = 0;
        event.drive.drive_level = 0.7f;
        event.drive.deviation = 0.3f;
        event.drive.urgency_weight = 0.8f;
        snprintf(event.drive.description, sizeof(event.drive.description),
                 "Test drive event");

        hypo_orch_publish(orch, bridge_id, &event);
    }
};

/* ============================================================================
 * Orchestrator Lifecycle Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, CreateWithDefaultConfig) {
    /* Destroy existing orchestrator */
    hypo_orch_destroy(orch);

    /* Create with NULL config should use defaults */
    orch = hypo_orch_create(nullptr);
    ASSERT_NE(orch, nullptr);

    /* Get state - should be idle after creation */
    hypo_orch_state_t state;
    ASSERT_EQ(0, hypo_orch_get_state(orch, &state));
    EXPECT_EQ(HYPO_ORCH_STATE_IDLE, state);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, DestroyNullIsSafe) {
    /* Should not crash */
    hypo_orch_destroy(nullptr);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, ResetClearsState) {
    register_test_bridges();

    /* Report some drives */
    hypo_orch_report_drive(orch, 1, 0, 0.8f, HYPO_URGENCY_ELEVATED, "Test drive");

    /* Get initial stats */
    hypo_orch_stats_t stats_before;
    hypo_orch_get_stats(orch, &stats_before);
    EXPECT_GT(stats_before.registered_bridges, 0u);

    /* Reset */
    ASSERT_EQ(0, hypo_orch_reset(orch));

    /* Stats should be cleared but bridges should remain */
    hypo_orch_stats_t stats_after;
    hypo_orch_get_stats(orch, &stats_after);
    EXPECT_EQ(stats_after.registered_bridges, stats_before.registered_bridges);
}

/* ============================================================================
 * Bridge Registration Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, RegisterMultipleBridges) {
    register_test_bridges();

    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orch, &stats);
    EXPECT_EQ(5u, stats.registered_bridges);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, GetBridgeInfo) {
    uint32_t bridge_id;
    int ret = hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                         "test_emotion_bridge",
                                         (void*)0x1234,
                                         nullptr, &bridge_id);
    ASSERT_EQ(0, ret);

    hypo_bridge_info_t info;
    ret = hypo_orch_get_bridge_info(orch, bridge_id, &info);
    ASSERT_EQ(0, ret);

    EXPECT_EQ(HYPO_BRIDGE_EMOTION, info.type);
    EXPECT_STREQ("test_emotion_bridge", info.name);
    EXPECT_EQ((void*)0x1234, info.bridge_handle);
    EXPECT_TRUE(info.is_active);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, GetBridgeByType) {
    uint32_t bridge_id;
    void* test_handle = (void*)0xABCD;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_ATTENTION,
                               "attention_test",
                               test_handle,
                               nullptr, &bridge_id);

    void* found_handle = nullptr;
    int ret = hypo_orch_get_bridge_by_type(orch, HYPO_BRIDGE_ATTENTION, &found_handle);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(test_handle, found_handle);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, UnregisterBridge) {
    uint32_t bridge_id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_SLEEP,
                               "sleep_test",
                               nullptr,
                               nullptr, &bridge_id);

    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orch, &stats);
    uint32_t initial_count = stats.registered_bridges;

    int ret = hypo_orch_unregister_bridge(orch, bridge_id);
    EXPECT_EQ(0, ret);

    hypo_orch_get_stats(orch, &stats);
    EXPECT_EQ(initial_count - 1, stats.registered_bridges);
}

/* ============================================================================
 * Event Flow Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, EventFlowBetweenBridges) {
    register_test_bridges();

    /* Get bridge IDs */
    uint32_t emotion_id = 1;  /* First registered bridge */
    uint32_t executive_id = 2;

    /* Set up subscription with event tracker */
    EventTracker tracker;
    int ret = hypo_orch_subscribe(orch, executive_id,
                                   HYPO_EVENT_DRIVE_ACTIVATED,
                                   event_tracker_callback, &tracker);
    ASSERT_EQ(0, ret);

    /* Publish event from emotion bridge */
    hypo_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;
    event.source = HYPO_BRIDGE_EMOTION;
    event.urgency = HYPO_URGENCY_ELEVATED;
    event.timestamp = 1000000;
    event.drive.drive_type = 0;
    event.drive.drive_level = 0.75f;

    ret = hypo_orch_publish(orch, emotion_id, &event);
    EXPECT_EQ(0, ret);

    /* Verify subscriber received */
    EXPECT_EQ(1, tracker.total_events.load());
    EXPECT_EQ(1, tracker.drive_activated_events.load());
}

TEST_F(HypothalamusOrchestratorIntegrationTest, MultipleSubscribersReceiveEvent) {
    register_test_bridges();

    std::atomic<int> c1{0}, c2{0}, c3{0};

    /* Subscribe multiple bridges to same event type */
    hypo_orch_subscribe(orch, 2, HYPO_EVENT_STRESS_RESPONSE,
                         counting_callback, &c1);
    hypo_orch_subscribe(orch, 3, HYPO_EVENT_STRESS_RESPONSE,
                         counting_callback, &c2);
    hypo_orch_subscribe(orch, 4, HYPO_EVENT_STRESS_RESPONSE,
                         counting_callback, &c3);

    /* Publish stress event */
    hypo_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = HYPO_EVENT_STRESS_RESPONSE;
    event.source = HYPO_BRIDGE_EMOTION;
    event.urgency = HYPO_URGENCY_URGENT;
    event.timestamp = 1000000;
    event.stress.stress_level = 0.9f;
    event.stress.is_acute = true;

    hypo_orch_publish(orch, 1, &event);

    /* All subscribers should receive */
    EXPECT_EQ(1, c1.load());
    EXPECT_EQ(1, c2.load());
    EXPECT_EQ(1, c3.load());

    /* Verify stats */
    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orch, &stats);
    EXPECT_EQ(1u, stats.events_published);
    EXPECT_EQ(3u, stats.events_delivered);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, SubscriptionUnsubscription) {
    register_test_bridges();

    std::atomic<int> counter{0};

    /* Subscribe */
    int ret = hypo_orch_subscribe(orch, 2, HYPO_EVENT_DRIVE_SATISFIED,
                                   counting_callback, &counter);
    ASSERT_EQ(0, ret);

    /* Publish - should receive */
    hypo_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = HYPO_EVENT_DRIVE_SATISFIED;
    event.source = HYPO_BRIDGE_EMOTION;

    hypo_orch_publish(orch, 1, &event);
    EXPECT_EQ(1, counter.load());

    /* Unsubscribe */
    ret = hypo_orch_unsubscribe(orch, 2, HYPO_EVENT_DRIVE_SATISFIED);
    EXPECT_EQ(0, ret);

    /* Publish - should NOT receive */
    hypo_orch_publish(orch, 1, &event);
    EXPECT_EQ(1, counter.load());  /* Still 1 */
}

TEST_F(HypothalamusOrchestratorIntegrationTest, SubscribeToBridgeEvents) {
    register_test_bridges();

    EventTracker tracker;

    /* Subscribe to all events from emotion bridge */
    int ret = hypo_orch_subscribe_to_bridge(orch, 2, HYPO_BRIDGE_EMOTION,
                                             event_tracker_callback, &tracker);
    ASSERT_EQ(0, ret);

    /* Publish multiple event types from emotion bridge */
    hypo_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.source = HYPO_BRIDGE_EMOTION;

    event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;
    hypo_orch_publish(orch, 1, &event);

    event.event_type = HYPO_EVENT_STRESS_RESPONSE;
    hypo_orch_publish(orch, 1, &event);

    event.event_type = HYPO_EVENT_HOMEOSTATIC_ALERT;
    hypo_orch_publish(orch, 1, &event);

    /* Should receive all events from emotion bridge */
    EXPECT_GE(tracker.total_events.load(), 3);
}

/* ============================================================================
 * Drive State Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, ReportDriveUpdatesState) {
    register_test_bridges();

    /* Report drive from multiple bridges */
    hypo_orch_report_drive(orch, 1, 0, 0.6f, HYPO_URGENCY_MODERATE, "Hunger");
    hypo_orch_report_drive(orch, 2, 1, 0.4f, HYPO_URGENCY_LOW, "Thirst");
    hypo_orch_report_drive(orch, 3, 2, 0.8f, HYPO_URGENCY_ELEVATED, "Safety");

    /* Get unified drive state */
    hypo_unified_drive_state_t state;
    int ret = hypo_orch_get_drive_state(orch, &state);
    ASSERT_EQ(0, ret);

    /* Should have aggregated data */
    EXPECT_GT(state.active_drives, 0u);
    EXPECT_GT(state.unified_drive_level, 0.0f);
    EXPECT_LE(state.unified_drive_level, 1.0f);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, GetDriveLevel) {
    register_test_bridges();

    /* Report high drive */
    hypo_orch_report_drive(orch, 1, 0, 0.9f, HYPO_URGENCY_URGENT, "High drive");

    float drive_level;
    int ret = hypo_orch_get_drive_level(orch, &drive_level);
    ASSERT_EQ(0, ret);

    EXPECT_GT(drive_level, 0.0f);
    EXPECT_LE(drive_level, 1.0f);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, ClearDrives) {
    register_test_bridges();

    /* Report drives */
    hypo_orch_report_drive(orch, 1, 0, 0.8f, HYPO_URGENCY_ELEVATED, "Test");
    hypo_orch_report_drive(orch, 2, 1, 0.7f, HYPO_URGENCY_MODERATE, "Test");

    /* Clear drives */
    int ret = hypo_orch_clear_drives(orch);
    EXPECT_EQ(0, ret);

    /* Drive level should be reset */
    float drive_level;
    hypo_orch_get_drive_level(orch, &drive_level);
    EXPECT_LE(drive_level, 0.1f);  /* Should be near zero */
}

/* ============================================================================
 * Stress Response Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, TriggerStressResponse) {
    register_test_bridges();

    EventTracker tracker;
    hypo_orch_subscribe(orch, 2, HYPO_EVENT_STRESS_RESPONSE,
                         event_tracker_callback, &tracker);

    /* Trigger stress */
    int ret = hypo_orch_trigger_stress(orch, "Test stressor");
    EXPECT_EQ(0, ret);

    /* Check orchestrator state */
    hypo_orch_state_t state;
    hypo_orch_get_state(orch, &state);
    EXPECT_EQ(HYPO_ORCH_STATE_STRESS, state);

    /* Check stress flag */
    bool in_stress;
    hypo_orch_is_stressed(orch, &in_stress);
    EXPECT_TRUE(in_stress);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, ReleaseStressResponse) {
    register_test_bridges();

    /* Trigger and release stress */
    hypo_orch_trigger_stress(orch, "Test");

    bool in_stress;
    hypo_orch_is_stressed(orch, &in_stress);
    EXPECT_TRUE(in_stress);

    int ret = hypo_orch_release_stress(orch);
    EXPECT_EQ(0, ret);

    hypo_orch_is_stressed(orch, &in_stress);
    EXPECT_FALSE(in_stress);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, StatisticsAccuracy) {
    register_test_bridges();

    std::atomic<int> counter{0};
    hypo_orch_subscribe(orch, 2, HYPO_EVENT_DRIVE_ACTIVATED,
                         counting_callback, &counter);
    hypo_orch_subscribe(orch, 3, HYPO_EVENT_DRIVE_ACTIVATED,
                         counting_callback, &counter);

    /* Publish multiple events */
    hypo_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;
    event.source = HYPO_BRIDGE_EMOTION;

    for (int i = 0; i < 10; i++) {
        hypo_orch_publish(orch, 1, &event);
    }

    /* Verify stats */
    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orch, &stats);

    EXPECT_EQ(5u, stats.registered_bridges);
    EXPECT_EQ(2u, stats.active_subscriptions);
    EXPECT_EQ(10u, stats.events_published);
    EXPECT_EQ(20u, stats.events_delivered);  /* 10 events * 2 subscribers */
    EXPECT_EQ(0u, stats.events_dropped);

    /* Counter should match */
    EXPECT_EQ(20, counter.load());
}

TEST_F(HypothalamusOrchestratorIntegrationTest, ResetStats) {
    register_test_bridges();

    /* Generate activity */
    hypo_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;
    event.source = HYPO_BRIDGE_EMOTION;

    for (int i = 0; i < 5; i++) {
        hypo_orch_publish(orch, 1, &event);
    }

    /* Reset stats */
    int ret = hypo_orch_reset_stats(orch);
    EXPECT_EQ(0, ret);

    /* Counters should be reset but bridges remain */
    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orch, &stats);
    EXPECT_EQ(0u, stats.events_published);
    EXPECT_EQ(0u, stats.events_delivered);
    EXPECT_EQ(5u, stats.registered_bridges);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, NullHandling) {
    /* Operations on null orchestrator should fail */
    int ret = hypo_orch_register_bridge(nullptr, HYPO_BRIDGE_EMOTION,
                                         "test", nullptr, nullptr, nullptr);
    EXPECT_EQ(-1, ret);

    hypo_orch_stats_t stats;
    ret = hypo_orch_get_stats(nullptr, &stats);
    EXPECT_EQ(-1, ret);

    hypo_orch_state_t state;
    ret = hypo_orch_get_state(nullptr, &state);
    EXPECT_EQ(-1, ret);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, SubscribeWithNullCallback) {
    register_test_bridges();

    int ret = hypo_orch_subscribe(orch, 2, HYPO_EVENT_DRIVE_ACTIVATED,
                                   nullptr, nullptr);
    EXPECT_EQ(-1, ret);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, CallbackErrorContinuesDelivery) {
    register_test_bridges();

    std::atomic<int> success_counter{0};

    /* First subscriber fails */
    hypo_orch_subscribe(orch, 2, HYPO_EVENT_DRIVE_ACTIVATED,
                         failing_callback, nullptr);
    /* Second subscriber succeeds */
    hypo_orch_subscribe(orch, 3, HYPO_EVENT_DRIVE_ACTIVATED,
                         counting_callback, &success_counter);

    /* Publish - both should be called despite first failing */
    hypo_event_data_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;
    event.source = HYPO_BRIDGE_EMOTION;

    hypo_orch_publish(orch, 1, &event);

    /* Success counter should have been incremented */
    EXPECT_EQ(1, success_counter.load());
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, StringConversions) {
    /* Test bridge type to string */
    const char* bridge_str = hypo_bridge_type_name(HYPO_BRIDGE_EMOTION);
    EXPECT_NE(bridge_str, nullptr);
    EXPECT_STRNE(bridge_str, "");

    bridge_str = hypo_bridge_type_name(HYPO_BRIDGE_EXECUTIVE);
    EXPECT_NE(bridge_str, nullptr);

    /* Test event type to string */
    const char* event_str = hypo_event_type_name(HYPO_EVENT_DRIVE_ACTIVATED);
    EXPECT_NE(event_str, nullptr);

    event_str = hypo_event_type_name(HYPO_EVENT_STRESS_RESPONSE);
    EXPECT_NE(event_str, nullptr);

    /* Test urgency to string */
    const char* urgency_str = hypo_urgency_name(HYPO_URGENCY_URGENT);
    EXPECT_NE(urgency_str, nullptr);

    /* Test state to string */
    const char* state_str = hypo_orch_state_name(HYPO_ORCH_STATE_IDLE);
    EXPECT_NE(state_str, nullptr);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(HypothalamusOrchestratorIntegrationTest, ConcurrentEventPublishing) {
    register_test_bridges();

    std::atomic<int> total_received{0};
    hypo_orch_subscribe(orch, 2, HYPO_EVENT_DRIVE_ACTIVATED,
                         counting_callback, &total_received);

    auto publish_thread = [this]() {
        for (int i = 0; i < 50; i++) {
            hypo_event_data_t event;
            memset(&event, 0, sizeof(event));
            event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;
            event.source = HYPO_BRIDGE_EMOTION;
            hypo_orch_publish(orch, 1, &event);
        }
    };

    std::thread t1(publish_thread);
    std::thread t2(publish_thread);
    std::thread t3(publish_thread);

    t1.join();
    t2.join();
    t3.join();

    /* Should receive all events */
    EXPECT_EQ(150, total_received.load());
}

TEST_F(HypothalamusOrchestratorIntegrationTest, ConcurrentDriveReporting) {
    register_test_bridges();

    std::atomic<int> success_count{0};

    auto report_thread = [this, &success_count]() {
        for (int i = 0; i < 50; i++) {
            int ret = hypo_orch_report_drive(orch, 1, i % 9, 0.5f + (i % 5) * 0.1f,
                                              HYPO_URGENCY_MODERATE, "Test");
            if (ret == 0) success_count++;
        }
    };

    std::thread t1(report_thread);
    std::thread t2(report_thread);

    t1.join();
    t2.join();

    EXPECT_EQ(100, success_count.load());

    /* Drive state should be queryable */
    hypo_unified_drive_state_t state;
    int ret = hypo_orch_get_drive_state(orch, &state);
    EXPECT_EQ(0, ret);
}

TEST_F(HypothalamusOrchestratorIntegrationTest, ConcurrentBridgeRegistration) {
    std::atomic<int> registered{0};

    auto register_thread = [this, &registered]() {
        for (int i = 0; i < 5; i++) {
            uint32_t bridge_id;
            char name[32];
            /* Use a hash of thread id for unique naming */
            std::hash<std::thread::id> hasher;
            size_t thread_hash = hasher(std::this_thread::get_id()) % 10000;
            snprintf(name, sizeof(name), "bridge_%zu_%d",
                     thread_hash, i);

            int ret = hypo_orch_register_bridge(orch,
                (hypo_bridge_type_t)((i % 10) + 1),
                name, nullptr, nullptr, &bridge_id);
            if (ret == 0) registered++;
        }
    };

    std::thread t1(register_thread);
    std::thread t2(register_thread);

    t1.join();
    t2.join();

    /* Some registrations should succeed */
    EXPECT_GT(registered.load(), 0);

    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orch, &stats);
    EXPECT_EQ(registered.load(), (int)stats.registered_bridges);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
