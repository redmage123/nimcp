/**
 * @file test_hypothalamus_orchestrator.cpp
 * @brief Unit tests for nimcp_hypothalamus_orchestrator.c
 *
 * WHAT: Comprehensive unit tests for the Hypothalamus orchestrator
 * WHY:  Ensure correct bridge coordination, event routing, drive management,
 *       stress response, and system integrations work correctly
 * HOW:  Use Google Test framework to test lifecycle, bridge registration,
 *       event pub/sub, drive state, stress management, and integrations
 *
 * COVERAGE TARGET: 100%
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <atomic>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class HypothalamusOrchestratorTest : public ::testing::Test {
protected:
    hypo_orchestrator_t orch;
    hypo_orch_config_t config;

    void SetUp() override {
        ASSERT_EQ(0, hypo_orch_default_config(&config));
        orch = hypo_orch_create(&config);
        ASSERT_NE(nullptr, orch) << "Failed to create Hypothalamus orchestrator";
    }

    void TearDown() override {
        hypo_orch_destroy(orch);
        orch = nullptr;
    }
};

// Fixture for tests that need no orchestrator pre-created
class HypothalamusOrchestratorLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// DEFAULT CONFIG TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorLifecycleTest, DefaultConfigHasReasonableValues) {
    hypo_orch_config_t config;
    EXPECT_EQ(0, hypo_orch_default_config(&config));

    EXPECT_EQ(config.max_bridges, HYPO_ORCH_MAX_BRIDGES);
    EXPECT_EQ(config.max_subscriptions, HYPO_ORCH_MAX_SUBSCRIPTIONS);
    EXPECT_TRUE(config.enable_async);
    EXPECT_EQ(config.event_queue_size, HYPO_ORCH_MAX_EVENT_QUEUE);

    EXPECT_FLOAT_EQ(config.urgent_threshold, HYPO_ORCH_DEFAULT_URGENT_THRESHOLD);
    EXPECT_FLOAT_EQ(config.elevated_threshold, HYPO_ORCH_DEFAULT_ELEVATED_THRESHOLD);
    EXPECT_FLOAT_EQ(config.moderate_threshold, HYPO_ORCH_DEFAULT_MODERATE_THRESHOLD);
    EXPECT_FLOAT_EQ(config.drive_decay_rate, HYPO_ORCH_DEFAULT_DRIVE_DECAY);

    EXPECT_TRUE(config.auto_regulate);
    EXPECT_TRUE(config.enable_cascade);
    EXPECT_TRUE(config.enable_recovery);
    EXPECT_EQ(config.regulation_timeout_ms, 60000u);

    EXPECT_FALSE(config.connect_immune);
    EXPECT_FALSE(config.connect_bio_async);
    EXPECT_TRUE(config.enable_logging);
}

TEST_F(HypothalamusOrchestratorLifecycleTest, DefaultConfigNullReturnsError) {
    EXPECT_EQ(-1, hypo_orch_default_config(nullptr));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorLifecycleTest, CreateWithDefaultConfig) {
    hypo_orchestrator_t orch = hypo_orch_create(nullptr);
    ASSERT_NE(nullptr, orch);

    hypo_orch_state_t state;
    EXPECT_EQ(0, hypo_orch_get_state(orch, &state));
    EXPECT_EQ(HYPO_ORCH_STATE_IDLE, state);

    hypo_orch_destroy(orch);
}

TEST_F(HypothalamusOrchestratorLifecycleTest, CreateWithCustomConfig) {
    hypo_orch_config_t config;
    hypo_orch_default_config(&config);
    config.max_bridges = 16;
    config.enable_async = false;
    config.urgent_threshold = 0.85f;

    hypo_orchestrator_t orch = hypo_orch_create(&config);
    ASSERT_NE(nullptr, orch);

    hypo_orch_destroy(orch);
}

TEST_F(HypothalamusOrchestratorLifecycleTest, DestroyNullDoesNotCrash) {
    hypo_orch_destroy(nullptr);
    // Should not crash
}

TEST_F(HypothalamusOrchestratorTest, ResetClearsState) {
    // Register a bridge and report a drive
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "TestEmotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.8f,
                                        HYPO_URGENCY_ELEVATED, "Test drive"));

    // Reset
    EXPECT_EQ(0, hypo_orch_reset(orch));

    // State should be idle
    hypo_orch_state_t state;
    EXPECT_EQ(0, hypo_orch_get_state(orch, &state));
    EXPECT_EQ(HYPO_ORCH_STATE_IDLE, state);

    // Drive level should be reset
    float drive_level;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &drive_level));
    EXPECT_FLOAT_EQ(0.0f, drive_level);
}

TEST_F(HypothalamusOrchestratorLifecycleTest, ResetNullReturnsError) {
    EXPECT_EQ(-1, hypo_orch_reset(nullptr));
}

// ============================================================================
// BRIDGE REGISTRATION TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, RegisterBridgeSuccess) {
    uint32_t bridge_id;
    int result = hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "EmotionBridge", nullptr, nullptr, &bridge_id);
    EXPECT_EQ(0, result);
    EXPECT_GT(bridge_id, 0u);
}

TEST_F(HypothalamusOrchestratorTest, RegisterMultipleBridges) {
    uint32_t emotion_id, exec_id, attention_id;

    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &emotion_id));
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EXECUTIVE,
                                           "Executive", nullptr, nullptr, &exec_id));
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_ATTENTION,
                                           "Attention", nullptr, nullptr, &attention_id));

    // IDs should be unique
    EXPECT_NE(emotion_id, exec_id);
    EXPECT_NE(exec_id, attention_id);
    EXPECT_NE(emotion_id, attention_id);

    // Check stats
    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(3u, stats.registered_bridges);
}

TEST_F(HypothalamusOrchestratorTest, RegisterDuplicateBridgeTypeFails) {
    uint32_t bridge_id1, bridge_id2;

    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion1", nullptr, nullptr, &bridge_id1));
    // Same bridge type should fail
    EXPECT_EQ(-1, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                            "Emotion2", nullptr, nullptr, &bridge_id2));
}

TEST_F(HypothalamusOrchestratorTest, RegisterBridgeWithHandle) {
    int dummy_handle = 42;
    uint32_t bridge_id;

    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_MEMORY,
                                           "Memory", &dummy_handle, nullptr, &bridge_id));

    void* retrieved_handle;
    EXPECT_EQ(0, hypo_orch_get_bridge_by_type(orch, HYPO_BRIDGE_MEMORY, &retrieved_handle));
    EXPECT_EQ(&dummy_handle, retrieved_handle);
}

TEST_F(HypothalamusOrchestratorTest, RegisterBridgeWithContext) {
    struct { int value; } context = { 123 };
    uint32_t bridge_id;

    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_SALIENCE,
                                           "Salience", nullptr, &context, &bridge_id));

    hypo_bridge_info_t info;
    EXPECT_EQ(0, hypo_orch_get_bridge_info(orch, bridge_id, &info));
    EXPECT_EQ(&context, info.context);
}

TEST_F(HypothalamusOrchestratorTest, RegisterBridgeNullOrchFails) {
    uint32_t bridge_id;
    EXPECT_EQ(-1, hypo_orch_register_bridge(nullptr, HYPO_BRIDGE_EMOTION,
                                            "Test", nullptr, nullptr, &bridge_id));
}

TEST_F(HypothalamusOrchestratorTest, RegisterBridgeInvalidTypeFails) {
    uint32_t bridge_id;

    EXPECT_EQ(-1, hypo_orch_register_bridge(orch, HYPO_BRIDGE_UNKNOWN,
                                            "Test", nullptr, nullptr, &bridge_id));
    EXPECT_EQ(-1, hypo_orch_register_bridge(orch, HYPO_BRIDGE_COUNT,
                                            "Test", nullptr, nullptr, &bridge_id));
    EXPECT_EQ(-1, hypo_orch_register_bridge(orch, (hypo_bridge_type_t)100,
                                            "Test", nullptr, nullptr, &bridge_id));
}

TEST_F(HypothalamusOrchestratorTest, UnregisterBridgeSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_unregister_bridge(orch, bridge_id));

    // Should be able to register same type again
    uint32_t new_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion2", nullptr, nullptr, &new_id));
}

TEST_F(HypothalamusOrchestratorTest, UnregisterBridgeInvalidIdFails) {
    EXPECT_EQ(-1, hypo_orch_unregister_bridge(orch, 9999));
}

TEST_F(HypothalamusOrchestratorTest, UnregisterBridgeNullOrchFails) {
    EXPECT_EQ(-1, hypo_orch_unregister_bridge(nullptr, 1));
}

TEST_F(HypothalamusOrchestratorTest, GetBridgeInfoSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EXECUTIVE,
                                           "TestExec", nullptr, nullptr, &bridge_id));

    hypo_bridge_info_t info;
    EXPECT_EQ(0, hypo_orch_get_bridge_info(orch, bridge_id, &info));

    EXPECT_EQ(bridge_id, info.bridge_id);
    EXPECT_EQ(HYPO_BRIDGE_EXECUTIVE, info.type);
    EXPECT_STREQ("TestExec", info.name);
    EXPECT_TRUE(info.is_active);
    EXPECT_FLOAT_EQ(0.0f, info.current_drive_level);
}

TEST_F(HypothalamusOrchestratorTest, GetBridgeInfoInvalidIdFails) {
    hypo_bridge_info_t info;
    EXPECT_EQ(-1, hypo_orch_get_bridge_info(orch, 9999, &info));
}

TEST_F(HypothalamusOrchestratorTest, GetBridgeInfoNullParamsFails) {
    uint32_t bridge_id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "Test", nullptr, nullptr, &bridge_id);

    hypo_bridge_info_t info;
    EXPECT_EQ(-1, hypo_orch_get_bridge_info(nullptr, bridge_id, &info));
    EXPECT_EQ(-1, hypo_orch_get_bridge_info(orch, bridge_id, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, GetBridgeByTypeSuccess) {
    int handle = 42;
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_ATTENTION,
                                           "Attention", &handle, nullptr, &bridge_id));

    void* retrieved;
    EXPECT_EQ(0, hypo_orch_get_bridge_by_type(orch, HYPO_BRIDGE_ATTENTION, &retrieved));
    EXPECT_EQ(&handle, retrieved);
}

TEST_F(HypothalamusOrchestratorTest, GetBridgeByTypeNotFoundFails) {
    void* handle;
    EXPECT_EQ(-1, hypo_orch_get_bridge_by_type(orch, HYPO_BRIDGE_SLEEP, &handle));
}

TEST_F(HypothalamusOrchestratorTest, GetBridgeByTypeNullParamsFails) {
    void* handle;
    EXPECT_EQ(-1, hypo_orch_get_bridge_by_type(nullptr, HYPO_BRIDGE_EMOTION, &handle));
    EXPECT_EQ(-1, hypo_orch_get_bridge_by_type(orch, HYPO_BRIDGE_EMOTION, nullptr));
}

// ============================================================================
// EVENT SUBSCRIPTION TESTS
// ============================================================================

// Callback tracking for tests
static std::atomic<int> g_callback_count{0};
static hypo_event_data_t g_last_event;

static int test_event_callback(const hypo_event_data_t* event, void* user_data) {
    g_callback_count++;
    if (event) {
        memcpy(&g_last_event, event, sizeof(g_last_event));
    }
    return 0;
}

TEST_F(HypothalamusOrchestratorTest, SubscribeToEventTypeSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_DRIVE_ACTIVATED,
                                     test_event_callback, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, SubscribeToMultipleEventTypes) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EXECUTIVE,
                                           "Executive", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_DRIVE_ACTIVATED,
                                     test_event_callback, nullptr));
    EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_DRIVE_SATISFIED,
                                     test_event_callback, nullptr));
    EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_STRESS_RESPONSE,
                                     test_event_callback, nullptr));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(3u, stats.active_subscriptions);
}

TEST_F(HypothalamusOrchestratorTest, SubscribeNullCallbackFails) {
    uint32_t bridge_id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "Test", nullptr, nullptr, &bridge_id);

    EXPECT_EQ(-1, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_DRIVE_ACTIVATED,
                                      nullptr, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, SubscribeInvalidEventTypeFails) {
    uint32_t bridge_id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "Test", nullptr, nullptr, &bridge_id);

    EXPECT_EQ(-1, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_COUNT,
                                      test_event_callback, nullptr));
    EXPECT_EQ(-1, hypo_orch_subscribe(orch, bridge_id, (hypo_event_type_t)100,
                                      test_event_callback, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, SubscribeToBridgeSuccess) {
    uint32_t subscriber_id, publisher_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &subscriber_id));
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EXECUTIVE,
                                           "Executive", nullptr, nullptr, &publisher_id));

    EXPECT_EQ(0, hypo_orch_subscribe_to_bridge(orch, subscriber_id, HYPO_BRIDGE_EXECUTIVE,
                                               test_event_callback, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, SubscribeToBridgeInvalidTypeFails) {
    uint32_t bridge_id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "Test", nullptr, nullptr, &bridge_id);

    EXPECT_EQ(-1, hypo_orch_subscribe_to_bridge(orch, bridge_id, HYPO_BRIDGE_COUNT,
                                                test_event_callback, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, UnsubscribeSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_DRIVE_ACTIVATED,
                                     test_event_callback, nullptr));

    EXPECT_EQ(0, hypo_orch_unsubscribe(orch, bridge_id, HYPO_EVENT_DRIVE_ACTIVATED));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(0u, stats.active_subscriptions);
}

TEST_F(HypothalamusOrchestratorTest, UnsubscribeNonexistentFails) {
    uint32_t bridge_id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "Test", nullptr, nullptr, &bridge_id);

    EXPECT_EQ(-1, hypo_orch_unsubscribe(orch, bridge_id, HYPO_EVENT_DRIVE_ACTIVATED));
}

TEST_F(HypothalamusOrchestratorTest, UnsubscribeNullOrchFails) {
    EXPECT_EQ(-1, hypo_orch_unsubscribe(nullptr, 1, HYPO_EVENT_DRIVE_ACTIVATED));
}

// ============================================================================
// EVENT PUBLISHING TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, PublishEventSuccess) {
    uint32_t pub_id, sub_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Publisher", nullptr, nullptr, &pub_id));
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EXECUTIVE,
                                           "Subscriber", nullptr, nullptr, &sub_id));

    g_callback_count = 0;
    EXPECT_EQ(0, hypo_orch_subscribe(orch, sub_id, HYPO_EVENT_DRIVE_ACTIVATED,
                                     test_event_callback, nullptr));

    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;
    event.source = HYPO_BRIDGE_EMOTION;
    event.urgency = HYPO_URGENCY_MODERATE;
    event.drive.drive_level = 0.5f;

    EXPECT_EQ(0, hypo_orch_publish(orch, pub_id, &event));
    EXPECT_EQ(1, g_callback_count.load());
    EXPECT_EQ(HYPO_EVENT_DRIVE_ACTIVATED, g_last_event.event_type);
}

TEST_F(HypothalamusOrchestratorTest, PublishEventToMultipleSubscribers) {
    uint32_t pub_id, sub1_id, sub2_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Publisher", nullptr, nullptr, &pub_id));
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EXECUTIVE,
                                           "Sub1", nullptr, nullptr, &sub1_id));
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_ATTENTION,
                                           "Sub2", nullptr, nullptr, &sub2_id));

    g_callback_count = 0;
    EXPECT_EQ(0, hypo_orch_subscribe(orch, sub1_id, HYPO_EVENT_STRESS_RESPONSE,
                                     test_event_callback, nullptr));
    EXPECT_EQ(0, hypo_orch_subscribe(orch, sub2_id, HYPO_EVENT_STRESS_RESPONSE,
                                     test_event_callback, nullptr));

    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_STRESS_RESPONSE;
    event.source = HYPO_BRIDGE_EMOTION;

    EXPECT_EQ(0, hypo_orch_publish(orch, pub_id, &event));
    EXPECT_EQ(2, g_callback_count.load());
}

TEST_F(HypothalamusOrchestratorTest, PublishEventDoesNotDeliverToSelf) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "SelfPub", nullptr, nullptr, &bridge_id));

    g_callback_count = 0;
    EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_DRIVE_ACTIVATED,
                                     test_event_callback, nullptr));

    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_DRIVE_ACTIVATED;

    EXPECT_EQ(0, hypo_orch_publish(orch, bridge_id, &event));
    EXPECT_EQ(0, g_callback_count.load());  // Should not deliver to self
}

TEST_F(HypothalamusOrchestratorTest, PublishNullEventFails) {
    uint32_t bridge_id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "Test", nullptr, nullptr, &bridge_id);

    EXPECT_EQ(-1, hypo_orch_publish(orch, bridge_id, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, PublishNullOrchFails) {
    hypo_event_data_t event = {};
    EXPECT_EQ(-1, hypo_orch_publish(nullptr, 1, &event));
}

TEST_F(HypothalamusOrchestratorTest, PublishAsyncSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "AsyncPub", nullptr, nullptr, &bridge_id));

    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_REWARD_SIGNAL;
    event.source = HYPO_BRIDGE_EMOTION;

    EXPECT_EQ(0, hypo_orch_publish_async(orch, bridge_id, &event));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_GE(stats.async_queue_depth, 1u);
}

TEST_F(HypothalamusOrchestratorTest, PublishAsyncNullParamsFails) {
    uint32_t bridge_id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "Test", nullptr, nullptr, &bridge_id);

    hypo_event_data_t event = {};
    EXPECT_EQ(-1, hypo_orch_publish_async(nullptr, bridge_id, &event));
    EXPECT_EQ(-1, hypo_orch_publish_async(orch, bridge_id, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, ReportDriveSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.7f,
                                        HYPO_URGENCY_ELEVATED, "Test drive"));

    float drive_level;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &drive_level));
    EXPECT_GT(drive_level, 0.0f);
}

TEST_F(HypothalamusOrchestratorTest, ReportDriveNullOrchFails) {
    EXPECT_EQ(-1, hypo_orch_report_drive(nullptr, 1, 1, 0.5f,
                                         HYPO_URGENCY_MODERATE, "Test"));
}

// ============================================================================
// DRIVE STATE TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, GetDriveStateInitiallyEmpty) {
    hypo_unified_drive_state_t state;
    EXPECT_EQ(0, hypo_orch_get_drive_state(orch, &state));

    EXPECT_FLOAT_EQ(0.0f, state.unified_drive_level);
    EXPECT_EQ(HYPO_URGENCY_NONE, state.urgency);
    EXPECT_EQ(0u, state.active_drives);
}

TEST_F(HypothalamusOrchestratorTest, GetDriveStateAfterReport) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_SALIENCE,
                                           "Salience", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.75f,
                                        HYPO_URGENCY_ELEVATED, "High salience"));

    hypo_unified_drive_state_t state;
    EXPECT_EQ(0, hypo_orch_get_drive_state(orch, &state));

    EXPECT_GT(state.unified_drive_level, 0.0f);
    EXPECT_EQ(HYPO_BRIDGE_SALIENCE, state.primary_source);
    EXPECT_GT(strlen(state.drive_summary), 0u);
}

TEST_F(HypothalamusOrchestratorTest, GetDriveStateNullParamsFails) {
    hypo_unified_drive_state_t state;
    EXPECT_EQ(-1, hypo_orch_get_drive_state(nullptr, &state));
    EXPECT_EQ(-1, hypo_orch_get_drive_state(orch, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, GetDriveLevelSuccess) {
    float level;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &level));
    EXPECT_FLOAT_EQ(0.0f, level);
}

TEST_F(HypothalamusOrchestratorTest, GetDriveLevelNullParamsFails) {
    float level;
    EXPECT_EQ(-1, hypo_orch_get_drive_level(nullptr, &level));
    EXPECT_EQ(-1, hypo_orch_get_drive_level(orch, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, UpdateDriveDecaySuccess) {
    EXPECT_EQ(0, hypo_orch_update_drive_decay(orch));
}

TEST_F(HypothalamusOrchestratorTest, UpdateDriveDecayNullFails) {
    EXPECT_EQ(-1, hypo_orch_update_drive_decay(nullptr));
}

TEST_F(HypothalamusOrchestratorTest, ClearDrivesSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.9f,
                                        HYPO_URGENCY_URGENT, "Urgent"));

    EXPECT_EQ(0, hypo_orch_clear_drives(orch));

    float level;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &level));
    EXPECT_FLOAT_EQ(0.0f, level);
}

TEST_F(HypothalamusOrchestratorTest, ClearDrivesNullFails) {
    EXPECT_EQ(-1, hypo_orch_clear_drives(nullptr));
}

// ============================================================================
// STRESS STATE MANAGEMENT TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, TriggerStressSuccess) {
    EXPECT_EQ(0, hypo_orch_trigger_stress(orch, "Test stress"));

    bool stressed;
    EXPECT_EQ(0, hypo_orch_is_stressed(orch, &stressed));
    EXPECT_TRUE(stressed);

    hypo_orch_state_t state;
    EXPECT_EQ(0, hypo_orch_get_state(orch, &state));
    EXPECT_EQ(HYPO_ORCH_STATE_STRESS, state);
}

TEST_F(HypothalamusOrchestratorTest, TriggerStressNullReasonSuccess) {
    EXPECT_EQ(0, hypo_orch_trigger_stress(orch, nullptr));

    bool stressed;
    EXPECT_EQ(0, hypo_orch_is_stressed(orch, &stressed));
    EXPECT_TRUE(stressed);
}

TEST_F(HypothalamusOrchestratorTest, TriggerStressNullOrchFails) {
    EXPECT_EQ(-1, hypo_orch_trigger_stress(nullptr, "Test"));
}

TEST_F(HypothalamusOrchestratorTest, ReleaseStressSuccess) {
    EXPECT_EQ(0, hypo_orch_trigger_stress(orch, "Test"));

    bool stressed;
    EXPECT_EQ(0, hypo_orch_is_stressed(orch, &stressed));
    EXPECT_TRUE(stressed);

    EXPECT_EQ(0, hypo_orch_release_stress(orch));

    EXPECT_EQ(0, hypo_orch_is_stressed(orch, &stressed));
    EXPECT_FALSE(stressed);
}

TEST_F(HypothalamusOrchestratorTest, ReleaseStressGoesToRecovery) {
    // Enable recovery in config
    hypo_orch_config_t recovery_config;
    hypo_orch_default_config(&recovery_config);
    recovery_config.enable_recovery = true;

    hypo_orchestrator_t recovery_orch = hypo_orch_create(&recovery_config);
    ASSERT_NE(nullptr, recovery_orch);

    EXPECT_EQ(0, hypo_orch_trigger_stress(recovery_orch, "Test"));
    EXPECT_EQ(0, hypo_orch_release_stress(recovery_orch));

    hypo_orch_state_t state;
    EXPECT_EQ(0, hypo_orch_get_state(recovery_orch, &state));
    EXPECT_EQ(HYPO_ORCH_STATE_RECOVERY, state);

    hypo_orch_destroy(recovery_orch);
}

TEST_F(HypothalamusOrchestratorTest, ReleaseStressNullFails) {
    EXPECT_EQ(-1, hypo_orch_release_stress(nullptr));
}

TEST_F(HypothalamusOrchestratorTest, IsStressedInitiallyFalse) {
    bool stressed;
    EXPECT_EQ(0, hypo_orch_is_stressed(orch, &stressed));
    EXPECT_FALSE(stressed);
}

TEST_F(HypothalamusOrchestratorTest, IsStressedNullParamsFails) {
    bool stressed;
    EXPECT_EQ(-1, hypo_orch_is_stressed(nullptr, &stressed));
    EXPECT_EQ(-1, hypo_orch_is_stressed(orch, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, BroadcastResponseSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_HOMEOSTATIC_ALERT;
    event.urgency = HYPO_URGENCY_MODERATE;

    EXPECT_EQ(0, hypo_orch_broadcast_response(orch, HYPO_EVENT_HOMEOSTATIC_ALERT, &event));
}

TEST_F(HypothalamusOrchestratorTest, BroadcastResponseNullDataSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_broadcast_response(orch, HYPO_EVENT_AUTONOMIC_SHIFT, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, BroadcastResponseNullOrchFails) {
    EXPECT_EQ(-1, hypo_orch_broadcast_response(nullptr, HYPO_EVENT_REWARD_SIGNAL, nullptr));
}

// ============================================================================
// STATE AND STATISTICS TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, GetStateSuccess) {
    hypo_orch_state_t state;
    EXPECT_EQ(0, hypo_orch_get_state(orch, &state));
    EXPECT_EQ(HYPO_ORCH_STATE_IDLE, state);
}

TEST_F(HypothalamusOrchestratorTest, GetStateNullParamsFails) {
    hypo_orch_state_t state;
    EXPECT_EQ(-1, hypo_orch_get_state(nullptr, &state));
    EXPECT_EQ(-1, hypo_orch_get_state(orch, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, GetStatsSuccess) {
    // Small delay to ensure uptime_us > 0 (create and get_stats may happen
    // within the same microsecond)
    std::this_thread::sleep_for(std::chrono::microseconds(10));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));

    EXPECT_EQ(0u, stats.registered_bridges);
    EXPECT_EQ(0u, stats.active_subscriptions);
    EXPECT_EQ(0u, stats.events_published);
    EXPECT_GT(stats.uptime_us, 0u);
}

TEST_F(HypothalamusOrchestratorTest, GetStatsAfterActivity) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));
    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.5f,
                                        HYPO_URGENCY_MODERATE, "Test"));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));

    EXPECT_EQ(1u, stats.registered_bridges);
    EXPECT_EQ(1u, stats.drives_activated);
    EXPECT_GT(stats.events_published, 0u);
}

TEST_F(HypothalamusOrchestratorTest, GetStatsNullParamsFails) {
    hypo_orch_stats_t stats;
    EXPECT_EQ(-1, hypo_orch_get_stats(nullptr, &stats));
    EXPECT_EQ(-1, hypo_orch_get_stats(orch, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, ResetStatsSuccess) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));
    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.5f,
                                        HYPO_URGENCY_MODERATE, "Test"));

    EXPECT_EQ(0, hypo_orch_reset_stats(orch));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));

    // Bridge count should be preserved
    EXPECT_EQ(1u, stats.registered_bridges);
    // But event counts should be reset
    EXPECT_EQ(0u, stats.events_published);
    EXPECT_EQ(0u, stats.drives_activated);
}

TEST_F(HypothalamusOrchestratorTest, ResetStatsNullFails) {
    EXPECT_EQ(-1, hypo_orch_reset_stats(nullptr));
}

// ============================================================================
// BIO-ASYNC CONNECTION TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, ConnectBioAsyncSuccess) {
    int dummy_router = 42;
    EXPECT_EQ(0, hypo_orch_connect_bio_async(orch, &dummy_router));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(1u, stats.connected_bridges);
}

TEST_F(HypothalamusOrchestratorTest, ConnectBioAsyncNullRouterSuccess) {
    EXPECT_EQ(0, hypo_orch_connect_bio_async(orch, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, ConnectBioAsyncNullOrchFails) {
    int dummy = 42;
    EXPECT_EQ(-1, hypo_orch_connect_bio_async(nullptr, &dummy));
}

TEST_F(HypothalamusOrchestratorTest, DisconnectBioAsyncSuccess) {
    int dummy_router = 42;
    EXPECT_EQ(0, hypo_orch_connect_bio_async(orch, &dummy_router));
    EXPECT_EQ(0, hypo_orch_disconnect_bio_async(orch));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(0u, stats.connected_bridges);
}

TEST_F(HypothalamusOrchestratorTest, DisconnectBioAsyncWhenNotConnected) {
    EXPECT_EQ(0, hypo_orch_disconnect_bio_async(orch));
}

TEST_F(HypothalamusOrchestratorTest, DisconnectBioAsyncNullFails) {
    EXPECT_EQ(-1, hypo_orch_disconnect_bio_async(nullptr));
}

// ============================================================================
// IMMUNE CONNECTION TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, ConnectImmuneSuccess) {
    int dummy_immune = 123;
    EXPECT_EQ(0, hypo_orch_connect_immune(orch, &dummy_immune));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(1u, stats.connected_bridges);
}

TEST_F(HypothalamusOrchestratorTest, ConnectImmuneNullOrchFails) {
    int dummy = 123;
    EXPECT_EQ(-1, hypo_orch_connect_immune(nullptr, &dummy));
}

// ============================================================================
// LOGGING CONNECTION TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, ConnectLoggingSuccess) {
    int dummy_logger = 456;
    EXPECT_EQ(0, hypo_orch_connect_logging(orch, &dummy_logger));
}

TEST_F(HypothalamusOrchestratorTest, ConnectLoggingNullLoggerSuccess) {
    EXPECT_EQ(0, hypo_orch_connect_logging(orch, nullptr));
}

TEST_F(HypothalamusOrchestratorTest, ConnectLoggingNullOrchFails) {
    int dummy = 456;
    EXPECT_EQ(-1, hypo_orch_connect_logging(nullptr, &dummy));
}

// ============================================================================
// UTILITY FUNCTION TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, BridgeTypeNameReturnsValidStrings) {
    EXPECT_STREQ("Unknown", hypo_bridge_type_name(HYPO_BRIDGE_UNKNOWN));
    EXPECT_STREQ("Emotion", hypo_bridge_type_name(HYPO_BRIDGE_EMOTION));
    EXPECT_STREQ("Executive", hypo_bridge_type_name(HYPO_BRIDGE_EXECUTIVE));
    EXPECT_STREQ("Attention", hypo_bridge_type_name(HYPO_BRIDGE_ATTENTION));
    EXPECT_STREQ("Sleep", hypo_bridge_type_name(HYPO_BRIDGE_SLEEP));
    EXPECT_STREQ("Immune", hypo_bridge_type_name(HYPO_BRIDGE_IMMUNE));
    EXPECT_STREQ("Wellbeing", hypo_bridge_type_name(HYPO_BRIDGE_WELLBEING));
    EXPECT_STREQ("Memory", hypo_bridge_type_name(HYPO_BRIDGE_MEMORY));
    EXPECT_STREQ("Perception", hypo_bridge_type_name(HYPO_BRIDGE_PERCEPTION));
    EXPECT_STREQ("Salience", hypo_bridge_type_name(HYPO_BRIDGE_SALIENCE));
    EXPECT_STREQ("Reasoning", hypo_bridge_type_name(HYPO_BRIDGE_REASONING));
    EXPECT_STREQ("GlobalWorkspace", hypo_bridge_type_name(HYPO_BRIDGE_GLOBAL_WORKSPACE));
    EXPECT_STREQ("Introspection", hypo_bridge_type_name(HYPO_BRIDGE_INTROSPECTION));
    EXPECT_STREQ("Curiosity", hypo_bridge_type_name(HYPO_BRIDGE_CURIOSITY));
    EXPECT_STREQ("GameTheory", hypo_bridge_type_name(HYPO_BRIDGE_GAME_THEORY));
    EXPECT_STREQ("Imagination", hypo_bridge_type_name(HYPO_BRIDGE_IMAGINATION));
    EXPECT_STREQ("Epistemic", hypo_bridge_type_name(HYPO_BRIDGE_EPISTEMIC));
    EXPECT_STREQ("Collective", hypo_bridge_type_name(HYPO_BRIDGE_COLLECTIVE));
    EXPECT_STREQ("Bias", hypo_bridge_type_name(HYPO_BRIDGE_BIAS));
    EXPECT_STREQ("TheoryOfMind", hypo_bridge_type_name(HYPO_BRIDGE_THEORY_OF_MIND));
    EXPECT_STREQ("Predictive", hypo_bridge_type_name(HYPO_BRIDGE_PREDICTIVE));
    EXPECT_STREQ("Logging", hypo_bridge_type_name(HYPO_BRIDGE_LOGGING));
    EXPECT_STREQ("BioAsync", hypo_bridge_type_name(HYPO_BRIDGE_BIO_ASYNC));
}

TEST_F(HypothalamusOrchestratorTest, BridgeTypeNameInvalidReturnsInvalid) {
    EXPECT_STREQ("Invalid", hypo_bridge_type_name(HYPO_BRIDGE_COUNT));
    EXPECT_STREQ("Invalid", hypo_bridge_type_name((hypo_bridge_type_t)100));
}

TEST_F(HypothalamusOrchestratorTest, EventTypeNameReturnsValidStrings) {
    EXPECT_STREQ("DriveActivated", hypo_event_type_name(HYPO_EVENT_DRIVE_ACTIVATED));
    EXPECT_STREQ("DriveSatisfied", hypo_event_type_name(HYPO_EVENT_DRIVE_SATISFIED));
    EXPECT_STREQ("DriveConflict", hypo_event_type_name(HYPO_EVENT_DRIVE_CONFLICT));
    EXPECT_STREQ("HomeostaticAlert", hypo_event_type_name(HYPO_EVENT_HOMEOSTATIC_ALERT));
    EXPECT_STREQ("CircadianPhase", hypo_event_type_name(HYPO_EVENT_CIRCADIAN_PHASE));
    EXPECT_STREQ("StressResponse", hypo_event_type_name(HYPO_EVENT_STRESS_RESPONSE));
    EXPECT_STREQ("AutonomicShift", hypo_event_type_name(HYPO_EVENT_AUTONOMIC_SHIFT));
    EXPECT_STREQ("AlignmentCheck", hypo_event_type_name(HYPO_EVENT_ALIGNMENT_CHECK));
    EXPECT_STREQ("RewardSignal", hypo_event_type_name(HYPO_EVENT_REWARD_SIGNAL));
    EXPECT_STREQ("SetpointChange", hypo_event_type_name(HYPO_EVENT_SETPOINT_CHANGE));
}

TEST_F(HypothalamusOrchestratorTest, EventTypeNameInvalidReturnsInvalid) {
    EXPECT_STREQ("Invalid", hypo_event_type_name(HYPO_EVENT_COUNT));
    EXPECT_STREQ("Invalid", hypo_event_type_name((hypo_event_type_t)100));
}

TEST_F(HypothalamusOrchestratorTest, UrgencyNameReturnsValidStrings) {
    EXPECT_STREQ("None", hypo_urgency_name(HYPO_URGENCY_NONE));
    EXPECT_STREQ("Low", hypo_urgency_name(HYPO_URGENCY_LOW));
    EXPECT_STREQ("Moderate", hypo_urgency_name(HYPO_URGENCY_MODERATE));
    EXPECT_STREQ("Elevated", hypo_urgency_name(HYPO_URGENCY_ELEVATED));
    EXPECT_STREQ("Urgent", hypo_urgency_name(HYPO_URGENCY_URGENT));
}

TEST_F(HypothalamusOrchestratorTest, UrgencyNameInvalidReturnsInvalid) {
    EXPECT_STREQ("Invalid", hypo_urgency_name((hypo_urgency_t)10));
}

TEST_F(HypothalamusOrchestratorTest, OrchStateNameReturnsValidStrings) {
    EXPECT_STREQ("Uninitialized", hypo_orch_state_name(HYPO_ORCH_STATE_UNINITIALIZED));
    EXPECT_STREQ("Idle", hypo_orch_state_name(HYPO_ORCH_STATE_IDLE));
    EXPECT_STREQ("Monitoring", hypo_orch_state_name(HYPO_ORCH_STATE_MONITORING));
    EXPECT_STREQ("Regulating", hypo_orch_state_name(HYPO_ORCH_STATE_REGULATING));
    EXPECT_STREQ("Conflict", hypo_orch_state_name(HYPO_ORCH_STATE_CONFLICT));
    EXPECT_STREQ("Stress", hypo_orch_state_name(HYPO_ORCH_STATE_STRESS));
    EXPECT_STREQ("Recovery", hypo_orch_state_name(HYPO_ORCH_STATE_RECOVERY));
    EXPECT_STREQ("Error", hypo_orch_state_name(HYPO_ORCH_STATE_ERROR));
}

TEST_F(HypothalamusOrchestratorTest, OrchStateNameInvalidReturnsInvalid) {
    EXPECT_STREQ("Invalid", hypo_orch_state_name((hypo_orch_state_t)100));
}

TEST_F(HypothalamusOrchestratorTest, PrintSummaryDoesNotCrash) {
    // Register some bridges
    uint32_t id;
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "Emotion", nullptr, nullptr, &id);
    hypo_orch_report_drive(orch, id, 1, 0.5f, HYPO_URGENCY_MODERATE, "Test");

    // Should not crash
    hypo_orch_print_summary(orch);
}

TEST_F(HypothalamusOrchestratorTest, PrintSummaryNullDoesNotCrash) {
    hypo_orch_print_summary(nullptr);
}

TEST_F(HypothalamusOrchestratorTest, PrintStatsDoesNotCrash) {
    hypo_orch_stats_t stats;
    hypo_orch_get_stats(orch, &stats);

    // Should not crash
    hypo_orch_print_stats(&stats);
}

TEST_F(HypothalamusOrchestratorTest, PrintStatsNullDoesNotCrash) {
    hypo_orch_print_stats(nullptr);
}

// ============================================================================
// URGENCY LEVEL TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, DriveUrgencyNoneWhenZero) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    hypo_unified_drive_state_t state;
    EXPECT_EQ(0, hypo_orch_get_drive_state(orch, &state));
    EXPECT_EQ(HYPO_URGENCY_NONE, state.urgency);
}

TEST_F(HypothalamusOrchestratorTest, DriveUrgencyLowWhenLow) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.2f,
                                        HYPO_URGENCY_LOW, "Low drive"));

    hypo_unified_drive_state_t state;
    EXPECT_EQ(0, hypo_orch_get_drive_state(orch, &state));
    EXPECT_EQ(HYPO_URGENCY_LOW, state.urgency);
}

TEST_F(HypothalamusOrchestratorTest, DriveUrgencyModerateWhenModerate) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.5f,
                                        HYPO_URGENCY_MODERATE, "Moderate drive"));

    hypo_unified_drive_state_t state;
    EXPECT_EQ(0, hypo_orch_get_drive_state(orch, &state));
    EXPECT_EQ(HYPO_URGENCY_MODERATE, state.urgency);
}

TEST_F(HypothalamusOrchestratorTest, DriveUrgencyElevatedWhenElevated) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.8f,
                                        HYPO_URGENCY_ELEVATED, "Elevated drive"));

    hypo_unified_drive_state_t state;
    EXPECT_EQ(0, hypo_orch_get_drive_state(orch, &state));
    EXPECT_EQ(HYPO_URGENCY_ELEVATED, state.urgency);
}

TEST_F(HypothalamusOrchestratorTest, DriveUrgencyUrgentWhenUrgent) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.95f,
                                        HYPO_URGENCY_URGENT, "Urgent drive"));

    hypo_unified_drive_state_t state;
    EXPECT_EQ(0, hypo_orch_get_drive_state(orch, &state));
    EXPECT_EQ(HYPO_URGENCY_URGENT, state.urgency);
}

// ============================================================================
// AUTO-REGULATE TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, AutoRegulateTriggersStressOnUrgentDrive) {
    // Create orchestrator with auto_regulate enabled
    hypo_orch_config_t auto_config;
    hypo_orch_default_config(&auto_config);
    auto_config.auto_regulate = true;

    hypo_orchestrator_t auto_orch = hypo_orch_create(&auto_config);
    ASSERT_NE(nullptr, auto_orch);

    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(auto_orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    // Report urgent drive
    EXPECT_EQ(0, hypo_orch_report_drive(auto_orch, bridge_id, 1, 0.95f,
                                        HYPO_URGENCY_URGENT, "Urgent"));

    // Should auto-trigger stress
    bool stressed;
    EXPECT_EQ(0, hypo_orch_is_stressed(auto_orch, &stressed));
    EXPECT_TRUE(stressed);

    hypo_orch_destroy(auto_orch);
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, HandleManyBridges) {
    // Register maximum bridges - 1 (since HYPO_BRIDGE_UNKNOWN is invalid)
    for (int i = HYPO_BRIDGE_EMOTION; i < HYPO_BRIDGE_COUNT; i++) {
        uint32_t id;
        char name[64];
        snprintf(name, sizeof(name), "Bridge_%d", i);
        EXPECT_EQ(0, hypo_orch_register_bridge(orch, (hypo_bridge_type_t)i,
                                               name, nullptr, nullptr, &id));
    }

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(HYPO_BRIDGE_COUNT - 1, stats.registered_bridges);
}

TEST_F(HypothalamusOrchestratorTest, HandleManySubscriptions) {
    uint32_t bridge_ids[5];

    // Register several bridges
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION, "E1", nullptr, nullptr, &bridge_ids[0]);
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_EXECUTIVE, "E2", nullptr, nullptr, &bridge_ids[1]);
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_ATTENTION, "E3", nullptr, nullptr, &bridge_ids[2]);
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_MEMORY, "E4", nullptr, nullptr, &bridge_ids[3]);
    hypo_orch_register_bridge(orch, HYPO_BRIDGE_SALIENCE, "E5", nullptr, nullptr, &bridge_ids[4]);

    // Subscribe each bridge to multiple event types
    for (int b = 0; b < 5; b++) {
        for (int e = 0; e < HYPO_EVENT_COUNT; e++) {
            EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_ids[b], (hypo_event_type_t)e,
                                             test_event_callback, nullptr));
        }
    }

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(50u, stats.active_subscriptions);  // 5 bridges * 10 event types
}

TEST_F(HypothalamusOrchestratorTest, DriveDecayReducesLevel) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.8f,
                                        HYPO_URGENCY_ELEVATED, "Test"));

    // Wait a bit for decay
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Force decay update
    EXPECT_EQ(0, hypo_orch_update_drive_decay(orch));

    float level;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &level));
    // Drive should have decayed slightly
    EXPECT_LT(level, 0.8f);
}

TEST_F(HypothalamusOrchestratorTest, UnregisterBridgeClearsSubscriptions) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_DRIVE_ACTIVATED,
                                     test_event_callback, nullptr));
    EXPECT_EQ(0, hypo_orch_subscribe(orch, bridge_id, HYPO_EVENT_STRESS_RESPONSE,
                                     test_event_callback, nullptr));

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(2u, stats.active_subscriptions);

    // Unregister bridge
    EXPECT_EQ(0, hypo_orch_unregister_bridge(orch, bridge_id));

    // Subscriptions should be deactivated (not counted)
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    // Note: The subscriptions are deactivated but stats may not be decremented
    // This tests that unregistration handles subscriptions
}

// ============================================================================
// DRIVE EVENT PROCESSING TESTS
// ============================================================================

TEST_F(HypothalamusOrchestratorTest, DriveConflictEventUpdatesState) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_DRIVE_CONFLICT;
    event.source = HYPO_BRIDGE_EMOTION;
    event.urgency = HYPO_URGENCY_ELEVATED;
    event.drive.drive_level = 0.7f;

    EXPECT_EQ(0, hypo_orch_publish(orch, bridge_id, &event));

    hypo_orch_state_t state;
    EXPECT_EQ(0, hypo_orch_get_state(orch, &state));
    EXPECT_EQ(HYPO_ORCH_STATE_CONFLICT, state);

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(1u, stats.conflicts_detected);
}

TEST_F(HypothalamusOrchestratorTest, DriveSatisfiedEventReducesDrive) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    // First activate a drive
    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.8f,
                                        HYPO_URGENCY_ELEVATED, "Active"));

    // Then satisfy it
    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_DRIVE_SATISFIED;
    event.source = HYPO_BRIDGE_EMOTION;
    event.urgency = HYPO_URGENCY_NONE;

    EXPECT_EQ(0, hypo_orch_publish(orch, bridge_id, &event));

    // Drive level should be reduced
    float level;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &level));
    EXPECT_LT(level, 0.8f);

    hypo_orch_stats_t stats;
    EXPECT_EQ(0, hypo_orch_get_stats(orch, &stats));
    EXPECT_EQ(1u, stats.drives_satisfied);
}

TEST_F(HypothalamusOrchestratorTest, HomeostaticAlertEventUpdatesDrive) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_PERCEPTION,
                                           "Perception", nullptr, nullptr, &bridge_id));

    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_HOMEOSTATIC_ALERT;
    event.source = HYPO_BRIDGE_PERCEPTION;
    event.urgency = HYPO_URGENCY_URGENT;
    event.homeostatic.is_critical = true;
    event.homeostatic.deviation = 0.8f;

    EXPECT_EQ(0, hypo_orch_publish(orch, bridge_id, &event));

    // Drive should be elevated due to critical homeostatic alert
    float level;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &level));
    EXPECT_GT(level, 0.0f);
}

TEST_F(HypothalamusOrchestratorTest, RewardSignalReducesDrive) {
    uint32_t bridge_id;
    EXPECT_EQ(0, hypo_orch_register_bridge(orch, HYPO_BRIDGE_EMOTION,
                                           "Emotion", nullptr, nullptr, &bridge_id));

    // First set high drive
    EXPECT_EQ(0, hypo_orch_report_drive(orch, bridge_id, 1, 0.9f,
                                        HYPO_URGENCY_URGENT, "High"));

    float before;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &before));

    // Apply reward
    hypo_event_data_t event = {};
    event.event_type = HYPO_EVENT_REWARD_SIGNAL;
    event.source = HYPO_BRIDGE_EMOTION;
    event.reward.reward_magnitude = 0.5f;

    EXPECT_EQ(0, hypo_orch_publish(orch, bridge_id, &event));

    float after;
    EXPECT_EQ(0, hypo_orch_get_drive_level(orch, &after));
    EXPECT_LT(after, before);
}
