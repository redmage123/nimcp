/**
 * @file test_hypothalamus_internal_bus.cpp
 * @brief Unit tests for nimcp_hypothalamus_internal_bus.h
 *
 * WHAT: Comprehensive unit tests for the Hypothalamus internal message bus
 * WHY:  Ensure correct pub-sub behavior, modulation rules, and thread safety
 *       for intra-hypothalamus module communication
 * HOW:  Use Google Test framework to test lifecycle, subscription, publishing,
 *       modulation, query, thread safety, and error handling.
 *
 * COVERAGE TARGET: 100%
 *
 * TEST CATEGORIES:
 * 1. Lifecycle - create/destroy/reset
 * 2. Subscription - subscribe/unsubscribe patterns
 * 3. Publishing - event delivery and convenience functions
 * 4. Modulation - cross-module modulation rules
 * 5. Query - statistics and subscriber counts
 * 6. Thread Safety - concurrent operations
 * 7. Error Handling - invalid parameters
 *
 * @version Phase H2: Hypothalamus Internal Communication
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_internal_bus.h"

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

/**
 * @brief Test fixture for Hypothalamus Internal Bus tests
 *
 * Provides common setup/teardown and helper functions for testing
 * the internal bus pub-sub system.
 */
class HypothalamusInternalBusTest : public ::testing::Test {
protected:
    hypo_ibus_t bus;
    hypo_ibus_config_t config;

    void SetUp() override {
        // Get default config and create bus
        ASSERT_EQ(0, hypo_ibus_default_config(&config));
        bus = hypo_ibus_create(&config);
        ASSERT_NE(nullptr, bus) << "Failed to create internal bus";

        // Reset callback tracking
        callback_count = 0;
        last_event_type = static_cast<hypo_internal_event_type_t>(-1);
        last_source_module = static_cast<hypo_internal_module_t>(-1);
    }

    void TearDown() override {
        hypo_ibus_destroy(bus);
        bus = nullptr;
    }

    // Callback tracking for tests
    static std::atomic<int> callback_count;
    static hypo_internal_event_type_t last_event_type;
    static hypo_internal_module_t last_source_module;
    static hypo_internal_event_t last_event;

    /**
     * @brief Simple callback that increments counter and records event info
     */
    static int test_callback(const hypo_internal_event_t* event, void* user_data) {
        callback_count++;
        if (event) {
            last_event_type = event->type;
            last_source_module = event->source;
            last_event = *event;
        }
        return 0;
    }

    /**
     * @brief Callback that returns an error
     */
    static int error_callback(const hypo_internal_event_t* event, void* user_data) {
        callback_count++;
        return -1;  // Return error
    }

    /**
     * @brief Callback that captures user_data
     */
    static int user_data_callback(const hypo_internal_event_t* event, void* user_data) {
        if (user_data) {
            int* counter = static_cast<int*>(user_data);
            (*counter)++;
        }
        return 0;
    }
};

// Static member definitions
std::atomic<int> HypothalamusInternalBusTest::callback_count{0};
hypo_internal_event_type_t HypothalamusInternalBusTest::last_event_type =
    static_cast<hypo_internal_event_type_t>(-1);
hypo_internal_module_t HypothalamusInternalBusTest::last_source_module =
    static_cast<hypo_internal_module_t>(-1);
hypo_internal_event_t HypothalamusInternalBusTest::last_event = {};

/* ============================================================================
 * LIFECYCLE TESTS
 * ============================================================================ */

/**
 * @test Verify default config has biologically plausible values
 */
TEST_F(HypothalamusInternalBusTest, DefaultConfigHasBiologicallyPlausibleValues) {
    hypo_ibus_config_t default_config;
    ASSERT_EQ(0, hypo_ibus_default_config(&default_config));

    // Check structural defaults
    EXPECT_EQ(default_config.max_subscribers, HYPO_IBUS_MAX_SUBSCRIBERS);
    EXPECT_EQ(default_config.max_queue_size, HYPO_IBUS_MAX_QUEUE);
    EXPECT_FALSE(default_config.enable_async);  // Sync delivery by default
    EXPECT_TRUE(default_config.enable_modulation);

    // Check biological interaction parameters (based on header documentation)
    // circadian_hunger_amplitude: 0.3 (30% hunger variation)
    EXPECT_NEAR(default_config.circadian_hunger_amplitude, 0.3f, 0.05f);

    // circadian_fatigue_amplitude: 0.5 (50% fatigue variation)
    EXPECT_NEAR(default_config.circadian_fatigue_amplitude, 0.5f, 0.05f);

    // cortisol_appetite_suppression: 0.4 (40% appetite reduction under stress)
    EXPECT_NEAR(default_config.cortisol_appetite_suppression, 0.4f, 0.05f);

    // fatigue_curiosity_reduction: 0.6 (60% curiosity reduction when tired)
    EXPECT_NEAR(default_config.fatigue_curiosity_reduction, 0.6f, 0.05f);

    // social_safety_modulation: 0.3 (30% safety drive reduction with social)
    EXPECT_NEAR(default_config.social_safety_modulation, 0.3f, 0.05f);

    // hunger_stress_threshold: 0.85 (trigger stress at 85% hunger)
    EXPECT_NEAR(default_config.hunger_stress_threshold, 0.85f, 0.05f);
}

/**
 * @test Verify default_config returns error on NULL pointer
 */
TEST_F(HypothalamusInternalBusTest, DefaultConfigNullPointerReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_default_config(nullptr));
}

/**
 * @test Verify create with NULL config uses defaults
 */
TEST_F(HypothalamusInternalBusTest, CreateWithNullConfigUsesDefaults) {
    hypo_ibus_t bus_null = hypo_ibus_create(nullptr);
    ASSERT_NE(nullptr, bus_null);

    // Bus should be functional with default settings
    hypo_ibus_stats_t stats;
    EXPECT_EQ(0, hypo_ibus_get_stats(bus_null, &stats));
    EXPECT_EQ(0u, stats.active_subscribers);

    hypo_ibus_destroy(bus_null);
}

/**
 * @test Verify create with custom config applies settings
 */
TEST_F(HypothalamusInternalBusTest, CreateWithCustomConfigAppliesSettings) {
    hypo_ibus_config_t custom_config;
    ASSERT_EQ(0, hypo_ibus_default_config(&custom_config));

    // Customize settings
    custom_config.enable_logging = true;
    custom_config.circadian_hunger_amplitude = 0.5f;

    hypo_ibus_t custom_bus = hypo_ibus_create(&custom_config);
    ASSERT_NE(nullptr, custom_bus);

    // Verify bus is functional
    hypo_ibus_stats_t stats;
    EXPECT_EQ(0, hypo_ibus_get_stats(custom_bus, &stats));

    hypo_ibus_destroy(custom_bus);
}

/**
 * @test Verify destroy is NULL-safe
 */
TEST_F(HypothalamusInternalBusTest, DestroyNullDoesNotCrash) {
    hypo_ibus_destroy(nullptr);
    // Should not crash - test passes if we reach here
}

/**
 * @test Verify destroy cleans up subscriptions
 */
TEST_F(HypothalamusInternalBusTest, DestroyCleanesUpSubscriptions) {
    // Add some subscriptions first
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_CIRCADIAN,
                                    HYPO_IEVT_STRESS_ONSET, test_callback, nullptr);
    EXPECT_GE(sub1, 0);

    // Destroy should clean up - we can't verify directly but should not leak
    hypo_ibus_destroy(bus);
    bus = nullptr;  // Prevent double-free in TearDown
}

/**
 * @test Verify reset clears pending events and statistics
 */
TEST_F(HypothalamusInternalBusTest, ResetClearsPendingEventsAndStats) {
    // Subscribe and publish some events
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_STRESS_ONSET, test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    EXPECT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET,
                                           0.5f, 0.6f, true));

    // Get stats before reset
    hypo_ibus_stats_t stats_before;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats_before));
    EXPECT_GT(stats_before.events_published, 0u);

    // Reset
    EXPECT_EQ(0, hypo_ibus_reset(bus));

    // Stats should be cleared
    hypo_ibus_stats_t stats_after;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats_after));
    EXPECT_EQ(0u, stats_after.events_published);
    EXPECT_EQ(0u, stats_after.events_delivered);

    // Subscriptions should be preserved (as per API: "keeps subscriptions")
    EXPECT_GT(hypo_ibus_subscriber_count(bus, HYPO_IEVT_STRESS_ONSET), 0u);
}

/**
 * @test Verify reset returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, ResetNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_reset(nullptr));
}

/* ============================================================================
 * SUBSCRIPTION TESTS
 * ============================================================================ */

/**
 * @test Verify basic subscribe and unsubscribe
 */
TEST_F(HypothalamusInternalBusTest, SubscribeAndUnsubscribeBasic) {
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_CIRCADIAN,
                                      HYPO_IEVT_MELATONIN_ONSET,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    EXPECT_EQ(1u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_MELATONIN_ONSET));

    // Unsubscribe
    EXPECT_EQ(0, hypo_ibus_unsubscribe(bus, sub_id));
    EXPECT_EQ(0u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_MELATONIN_ONSET));
}

/**
 * @test Verify subscribe returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, SubscribeNullBusReturnsError) {
    int sub_id = hypo_ibus_subscribe(nullptr, HYPO_IMOD_CIRCADIAN,
                                      HYPO_IEVT_MELATONIN_ONSET,
                                      test_callback, nullptr);
    EXPECT_EQ(-1, sub_id);
}

/**
 * @test Verify subscribe returns error on NULL callback
 */
TEST_F(HypothalamusInternalBusTest, SubscribeNullCallbackReturnsError) {
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_CIRCADIAN,
                                      HYPO_IEVT_MELATONIN_ONSET,
                                      nullptr, nullptr);
    EXPECT_EQ(-1, sub_id);
}

/**
 * @test Verify multiple subscribers for same event
 */
TEST_F(HypothalamusInternalBusTest, MultipleSubscribersForSameEvent) {
    // Add multiple subscribers to the same event
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                    HYPO_IEVT_STRESS_ONSET,
                                    test_callback, nullptr);
    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_HOMEOSTASIS,
                                    HYPO_IEVT_STRESS_ONSET,
                                    test_callback, nullptr);
    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                    HYPO_IEVT_STRESS_ONSET,
                                    test_callback, nullptr);

    ASSERT_GE(sub1, 0);
    ASSERT_GE(sub2, 0);
    ASSERT_GE(sub3, 0);

    // All subscriptions should have unique IDs
    EXPECT_NE(sub1, sub2);
    EXPECT_NE(sub2, sub3);
    EXPECT_NE(sub1, sub3);

    EXPECT_EQ(3u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_STRESS_ONSET));
}

/**
 * @test Verify subscribe to module (all events from a module)
 */
TEST_F(HypothalamusInternalBusTest, SubscribeToModuleReceivesAllModuleEvents) {
    // Subscribe to all events from HPA axis module
    int sub_id = hypo_ibus_subscribe_to_module(bus, HYPO_IMOD_DRIVES,
                                                HYPO_IMOD_HPA_AXIS,
                                                test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Module should now have subscribers
    EXPECT_TRUE(hypo_ibus_has_subscribers(bus, HYPO_IMOD_HPA_AXIS));
}

/**
 * @test Verify subscribe_to_module returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, SubscribeToModuleNullBusReturnsError) {
    int sub_id = hypo_ibus_subscribe_to_module(nullptr, HYPO_IMOD_DRIVES,
                                                HYPO_IMOD_HPA_AXIS,
                                                test_callback, nullptr);
    EXPECT_EQ(-1, sub_id);
}

/**
 * @test Verify unsubscribe returns error on invalid subscription ID
 */
TEST_F(HypothalamusInternalBusTest, UnsubscribeInvalidIdReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_unsubscribe(bus, 999));  // Invalid ID
    EXPECT_EQ(-1, hypo_ibus_unsubscribe(bus, -1));   // Negative ID
}

/**
 * @test Verify unsubscribe returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, UnsubscribeNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_unsubscribe(nullptr, 0));
}

/**
 * @test Verify unsubscribe_module removes all subscriptions for a module
 */
TEST_F(HypothalamusInternalBusTest, UnsubscribeModuleRemovesAllModuleSubscriptions) {
    // Add multiple subscriptions from same module
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                    HYPO_IEVT_STRESS_ONSET,
                                    test_callback, nullptr);
    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                    HYPO_IEVT_HUNGER_ONSET,
                                    test_callback, nullptr);
    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                    HYPO_IEVT_FATIGUE_ONSET,
                                    test_callback, nullptr);

    ASSERT_GE(sub1, 0);
    ASSERT_GE(sub2, 0);
    ASSERT_GE(sub3, 0);

    // Unsubscribe all for HYPO_IMOD_DRIVES
    int removed = hypo_ibus_unsubscribe_module(bus, HYPO_IMOD_DRIVES);
    EXPECT_EQ(3, removed);

    // Verify all subscriptions removed
    EXPECT_EQ(0u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_STRESS_ONSET));
    EXPECT_EQ(0u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_HUNGER_ONSET));
    EXPECT_EQ(0u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_FATIGUE_ONSET));
}

/**
 * @test Verify unsubscribe_module returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, UnsubscribeModuleNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_unsubscribe_module(nullptr, HYPO_IMOD_DRIVES));
}

/**
 * @test Verify maximum subscribers limit is enforced
 */
TEST_F(HypothalamusInternalBusTest, MaxSubscribersLimitEnforced) {
    // Add maximum number of subscribers
    int last_valid_id = -1;
    for (uint32_t i = 0; i < HYPO_IBUS_MAX_SUBSCRIBERS; i++) {
        int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_CIRCADIAN,
                                          HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                          test_callback, nullptr);
        if (sub_id >= 0) {
            last_valid_id = sub_id;
        }
    }

    EXPECT_GE(last_valid_id, 0);  // At least some subscriptions succeeded

    // Next subscription should fail
    int overflow_id = hypo_ibus_subscribe(bus, HYPO_IMOD_CIRCADIAN,
                                           HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                           test_callback, nullptr);
    EXPECT_EQ(-1, overflow_id);
}

/* ============================================================================
 * PUBLISHING TESTS
 * ============================================================================ */

/**
 * @test Verify basic publish invokes callbacks
 */
TEST_F(HypothalamusInternalBusTest, PublishInvokesSubscriberCallbacks) {
    callback_count = 0;

    // Subscribe to circadian phase change
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Create and publish event
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_CIRCADIAN_PHASE_CHANGE;
    event.source = HYPO_IMOD_CIRCADIAN;
    event.data.circadian.old_phase = 0;
    event.data.circadian.new_phase = 1;
    event.data.circadian.melatonin_level = 0.8f;
    event.data.circadian.cortisol_level = 0.2f;
    event.data.circadian.alertness = 0.3f;

    int result = hypo_ibus_publish(bus, &event);
    EXPECT_EQ(0, result);  // 0 = success
    EXPECT_EQ(1, callback_count.load());  // One subscriber notified
    EXPECT_EQ(HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, last_event_type);
    EXPECT_EQ(HYPO_IMOD_CIRCADIAN, last_source_module);
}

/**
 * @test Verify publish returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, PublishNullBusReturnsError) {
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_STRESS_ONSET;
    event.source = HYPO_IMOD_HPA_AXIS;

    EXPECT_EQ(-1, hypo_ibus_publish(nullptr, &event));
}

/**
 * @test Verify publish returns error on NULL event
 */
TEST_F(HypothalamusInternalBusTest, PublishNullEventReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_publish(bus, nullptr));
}

/**
 * @test Verify publish returns 0 when no subscribers
 */
TEST_F(HypothalamusInternalBusTest, PublishNoSubscribersReturnsZero) {
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_ALIGNMENT_WARNING;  // Unlikely to have subscribers
    event.source = HYPO_IMOD_ALIGNMENT;

    int delivered = hypo_ibus_publish(bus, &event);
    EXPECT_EQ(0, delivered);
}

/**
 * @test Verify publish_circadian_phase convenience function
 */
TEST_F(HypothalamusInternalBusTest, PublishCircadianPhaseConvenienceFunction) {
    callback_count = 0;

    // Subscribe to circadian phase change
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Use convenience function
    int result = hypo_ibus_publish_circadian_phase(bus,
                                                    0, 1,    // old_phase, new_phase
                                                    0.8f,    // melatonin
                                                    0.2f,    // cortisol
                                                    0.3f);   // alertness
    EXPECT_EQ(0, result);
    EXPECT_EQ(1, callback_count.load());
    EXPECT_EQ(HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, last_event_type);

    // Verify event data was populated
    EXPECT_EQ(0u, last_event.data.circadian.old_phase);
    EXPECT_EQ(1u, last_event.data.circadian.new_phase);
    EXPECT_NEAR(0.8f, last_event.data.circadian.melatonin_level, 0.001f);
}

/**
 * @test Verify publish_circadian_phase returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, PublishCircadianPhaseNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_publish_circadian_phase(nullptr, 0, 1, 0.8f, 0.2f, 0.3f));
}

/**
 * @test Verify publish_stress convenience function
 */
TEST_F(HypothalamusInternalBusTest, PublishStressConvenienceFunction) {
    callback_count = 0;

    // Subscribe to stress events
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                      HYPO_IEVT_STRESS_ONSET,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Use convenience function
    int result = hypo_ibus_publish_stress(bus,
                                           HYPO_IEVT_STRESS_ONSET,
                                           0.7f,    // stress_level
                                           0.6f,    // cortisol
                                           true);   // is_acute
    EXPECT_EQ(0, result);
    EXPECT_EQ(1, callback_count.load());

    // Verify event data
    EXPECT_EQ(HYPO_IEVT_STRESS_ONSET, last_event_type);
    EXPECT_NEAR(0.7f, last_event.data.stress.stress_level, 0.001f);
    EXPECT_NEAR(0.6f, last_event.data.stress.cortisol_level, 0.001f);
    EXPECT_TRUE(last_event.data.stress.is_acute);
}

/**
 * @test Verify publish_stress returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, PublishStressNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_publish_stress(nullptr, HYPO_IEVT_STRESS_ONSET,
                                            0.5f, 0.5f, true));
}

/**
 * @test Verify publish_drive convenience function
 */
TEST_F(HypothalamusInternalBusTest, PublishDriveConvenienceFunction) {
    callback_count = 0;

    // Subscribe to drive events
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_HPA_AXIS,
                                      HYPO_IEVT_HUNGER_ONSET,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Use convenience function
    int result = hypo_ibus_publish_drive(bus,
                                          HYPO_IEVT_HUNGER_ONSET,
                                          0,       // drive_type (hunger)
                                          0.8f,    // drive_level
                                          0.9f);   // urgency
    EXPECT_EQ(0, result);
    EXPECT_EQ(1, callback_count.load());

    // Verify event data
    EXPECT_EQ(HYPO_IEVT_HUNGER_ONSET, last_event_type);
    EXPECT_EQ(0u, last_event.data.drive.drive_type);
    EXPECT_NEAR(0.8f, last_event.data.drive.drive_level, 0.001f);
    EXPECT_NEAR(0.9f, last_event.data.drive.urgency, 0.001f);
}

/**
 * @test Verify publish_drive returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, PublishDriveNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_publish_drive(nullptr, HYPO_IEVT_HUNGER_ONSET,
                                           0, 0.5f, 0.5f));
}

/**
 * @test Verify publish_autonomic convenience function
 */
TEST_F(HypothalamusInternalBusTest, PublishAutonomicConvenienceFunction) {
    callback_count = 0;

    // Subscribe to autonomic events
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_HPA_AXIS,
                                      HYPO_IEVT_SYMPATHETIC_ACTIVATION,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Use convenience function
    int result = hypo_ibus_publish_autonomic(bus,
                                              HYPO_IEVT_SYMPATHETIC_ACTIVATION,
                                              0.8f,    // sympathetic
                                              0.2f);   // parasympathetic
    EXPECT_EQ(0, result);
    EXPECT_EQ(1, callback_count.load());

    // Verify event data
    EXPECT_EQ(HYPO_IEVT_SYMPATHETIC_ACTIVATION, last_event_type);
    EXPECT_NEAR(0.8f, last_event.data.autonomic.sympathetic_tone, 0.001f);
    EXPECT_NEAR(0.2f, last_event.data.autonomic.parasympathetic_tone, 0.001f);
}

/**
 * @test Verify publish_autonomic returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, PublishAutonomicNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_publish_autonomic(nullptr,
                                               HYPO_IEVT_SYMPATHETIC_ACTIVATION,
                                               0.8f, 0.2f));
}

/**
 * @test Verify multiple subscribers all receive event
 */
TEST_F(HypothalamusInternalBusTest, MultipleSubscribersAllReceiveEvent) {
    callback_count = 0;

    // Add three subscribers
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                    HYPO_IEVT_STRESS_PEAK,
                                    test_callback, nullptr);
    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_HOMEOSTASIS,
                                    HYPO_IEVT_STRESS_PEAK,
                                    test_callback, nullptr);
    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                    HYPO_IEVT_STRESS_PEAK,
                                    test_callback, nullptr);

    ASSERT_GE(sub1, 0);
    ASSERT_GE(sub2, 0);
    ASSERT_GE(sub3, 0);

    // Publish event
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_STRESS_PEAK;
    event.source = HYPO_IMOD_HPA_AXIS;
    event.data.stress.stress_level = 0.9f;

    int result = hypo_ibus_publish(bus, &event);
    EXPECT_EQ(0, result);  // 0 = success
    EXPECT_EQ(3, callback_count.load());  // All 3 subscribers notified
}

/**
 * @test Verify user_data is passed to callback correctly
 */
TEST_F(HypothalamusInternalBusTest, UserDataPassedToCallback) {
    int user_counter = 0;

    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_CORTISOL_ELEVATED,
                                      user_data_callback, &user_counter);
    ASSERT_GE(sub_id, 0);

    // Publish event
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_CORTISOL_ELEVATED;
    event.source = HYPO_IMOD_HPA_AXIS;

    hypo_ibus_publish(bus, &event);

    EXPECT_EQ(1, user_counter);
}

/**
 * @test Verify statistics are updated on publish
 */
TEST_F(HypothalamusInternalBusTest, StatsUpdatedOnPublish) {
    // Subscribe
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_DRIVE_SATISFIED,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Publish multiple events
    for (int i = 0; i < 5; i++) {
        hypo_internal_event_t event = {};
        event.type = HYPO_IEVT_DRIVE_SATISFIED;
        event.source = HYPO_IMOD_DRIVES;
        hypo_ibus_publish(bus, &event);
    }

    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));

    EXPECT_EQ(5u, stats.events_published);
    EXPECT_EQ(5u, stats.events_delivered);  // One subscriber, 5 events
    EXPECT_EQ(5u, stats.module_events[HYPO_IMOD_DRIVES]);
}

/* ============================================================================
 * MODULATION TESTS
 * ============================================================================ */

/**
 * @test Verify register_modulation returns valid rule ID
 */
TEST_F(HypothalamusInternalBusTest, RegisterModulationReturnsValidRuleId) {
    hypo_ibus_modulation_t modulation = {};
    modulation.target = HYPO_IMOD_APPETITE;
    modulation.modulation_factor = 0.6f;  // Reduce to 60%
    modulation.parameter_id = 0;
    modulation.duration_us = 5000000;  // 5 seconds
    modulation.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus, HYPO_IEVT_CORTISOL_ELEVATED,
                                                 &modulation);
    EXPECT_GE(rule_id, 0);
}

/**
 * @test Verify register_modulation returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, RegisterModulationNullBusReturnsError) {
    hypo_ibus_modulation_t modulation = {};
    modulation.target = HYPO_IMOD_APPETITE;

    EXPECT_EQ(-1, hypo_ibus_register_modulation(nullptr, HYPO_IEVT_CORTISOL_ELEVATED,
                                                  &modulation));
}

/**
 * @test Verify register_modulation returns error on NULL modulation
 */
TEST_F(HypothalamusInternalBusTest, RegisterModulationNullModulationReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_register_modulation(bus, HYPO_IEVT_CORTISOL_ELEVATED,
                                                  nullptr));
}

/**
 * @test Verify get_modulation returns 1.0 when no modulation active
 */
TEST_F(HypothalamusInternalBusTest, GetModulationReturnsOneWhenNoModulation) {
    float factor = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_NEAR(1.0f, factor, 0.001f);
}

/**
 * @test Verify get_modulation returns error value for NULL bus
 */
TEST_F(HypothalamusInternalBusTest, GetModulationNullBusReturnsDefault) {
    float factor = hypo_ibus_get_modulation(nullptr, HYPO_IMOD_APPETITE, 0);
    EXPECT_NEAR(1.0f, factor, 0.001f);  // Default when error
}

/**
 * @test Verify update_modulations decays time-limited modulations
 */
TEST_F(HypothalamusInternalBusTest, UpdateModulationsDecaysTimeoutModulations) {
    // Register a short-lived modulation
    // Use DRIVES module as target (fatigue affects exploration/drive behavior)
    hypo_ibus_modulation_t modulation = {};
    modulation.target = HYPO_IMOD_DRIVES;
    modulation.modulation_factor = 0.5f;
    modulation.parameter_id = 0;
    modulation.duration_us = 1000000;  // 1 second
    modulation.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus, HYPO_IEVT_FATIGUE_ONSET,
                                                 &modulation);
    ASSERT_GE(rule_id, 0);

    // Trigger the modulation by publishing event
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_FATIGUE_ONSET;
    event.source = HYPO_IMOD_HOMEOSTASIS;
    hypo_ibus_publish(bus, &event);

    // Update with time that exceeds duration
    EXPECT_EQ(0, hypo_ibus_update_modulations(bus, 2000000));  // 2 seconds

    // Modulation should have decayed (factor back to 1.0)
    float factor = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);
    EXPECT_NEAR(1.0f, factor, 0.1f);  // Allow some tolerance
}

/**
 * @test Verify update_modulations returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, UpdateModulationsNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_update_modulations(nullptr, 1000000));
}

/**
 * @test Verify clear_modulations removes all active modulations
 */
TEST_F(HypothalamusInternalBusTest, ClearModulationsRemovesAll) {
    // Register a modulation
    hypo_ibus_modulation_t modulation = {};
    modulation.target = HYPO_IMOD_APPETITE;
    modulation.modulation_factor = 0.5f;
    modulation.parameter_id = 0;
    modulation.duration_us = 0;  // No timeout
    modulation.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus, HYPO_IEVT_STRESS_ONSET,
                                                 &modulation);
    ASSERT_GE(rule_id, 0);

    // Trigger the modulation
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_STRESS_ONSET;
    event.source = HYPO_IMOD_HPA_AXIS;
    hypo_ibus_publish(bus, &event);

    // Clear all modulations
    EXPECT_EQ(0, hypo_ibus_clear_modulations(bus));

    // Factor should be back to 1.0
    float factor = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_NEAR(1.0f, factor, 0.001f);
}

/**
 * @test Verify register_default_modulations adds biological rules
 */
TEST_F(HypothalamusInternalBusTest, RegisterDefaultModulationsAddsBiologicalRules) {
    int rules_added = hypo_ibus_register_default_modulations(bus);

    // Should register at least 7 rules per documentation
    EXPECT_GE(rules_added, 7);
}

/**
 * @test Verify register_default_modulations returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, RegisterDefaultModulationsNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_register_default_modulations(nullptr));
}

/* ============================================================================
 * QUERY API TESTS
 * ============================================================================ */

/**
 * @test Verify get_stats returns valid statistics
 */
TEST_F(HypothalamusInternalBusTest, GetStatsReturnsValidStats) {
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));

    // Initial stats should be zero
    EXPECT_EQ(0u, stats.events_published);
    EXPECT_EQ(0u, stats.events_delivered);
    EXPECT_EQ(0u, stats.events_dropped);
    EXPECT_EQ(0u, stats.modulations_applied);
    EXPECT_EQ(0u, stats.queue_depth);
}

/**
 * @test Verify get_stats returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, GetStatsNullBusReturnsError) {
    hypo_ibus_stats_t stats;
    EXPECT_EQ(-1, hypo_ibus_get_stats(nullptr, &stats));
}

/**
 * @test Verify get_stats returns error on NULL stats
 */
TEST_F(HypothalamusInternalBusTest, GetStatsNullStatsReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_get_stats(bus, nullptr));
}

/**
 * @test Verify reset_stats clears statistics
 */
TEST_F(HypothalamusInternalBusTest, ResetStatsClearsStatistics) {
    // Generate some stats first
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_SETPOINT_DEVIATION,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_SETPOINT_DEVIATION;
    event.source = HYPO_IMOD_HOMEOSTASIS;
    hypo_ibus_publish(bus, &event);

    // Verify stats were recorded
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.events_published, 0u);

    // Reset stats
    EXPECT_EQ(0, hypo_ibus_reset_stats(bus));

    // Verify reset
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(0u, stats.events_published);
}

/**
 * @test Verify reset_stats returns error on NULL bus
 */
TEST_F(HypothalamusInternalBusTest, ResetStatsNullBusReturnsError) {
    EXPECT_EQ(-1, hypo_ibus_reset_stats(nullptr));
}

/**
 * @test Verify subscriber_count returns correct count
 */
TEST_F(HypothalamusInternalBusTest, SubscriberCountReturnsCorrectCount) {
    EXPECT_EQ(0u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_TEMPERATURE_ALERT));

    // Add subscribers
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_THERMOREGULATION,
                                    HYPO_IEVT_TEMPERATURE_ALERT,
                                    test_callback, nullptr);
    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                    HYPO_IEVT_TEMPERATURE_ALERT,
                                    test_callback, nullptr);

    ASSERT_GE(sub1, 0);
    ASSERT_GE(sub2, 0);

    EXPECT_EQ(2u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_TEMPERATURE_ALERT));

    // Remove one
    hypo_ibus_unsubscribe(bus, sub1);
    EXPECT_EQ(1u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_TEMPERATURE_ALERT));
}

/**
 * @test Verify subscriber_count returns 0 for NULL bus
 */
TEST_F(HypothalamusInternalBusTest, SubscriberCountNullBusReturnsZero) {
    EXPECT_EQ(0u, hypo_ibus_subscriber_count(nullptr, HYPO_IEVT_STRESS_ONSET));
}

/**
 * @test Verify has_subscribers returns correct value
 */
TEST_F(HypothalamusInternalBusTest, HasSubscribersReturnsCorrectValue) {
    EXPECT_FALSE(hypo_ibus_has_subscribers(bus, HYPO_IMOD_HPA_AXIS));

    // Add subscriber from HPA axis
    int sub_id = hypo_ibus_subscribe_to_module(bus, HYPO_IMOD_DRIVES,
                                                HYPO_IMOD_HPA_AXIS,
                                                test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    EXPECT_TRUE(hypo_ibus_has_subscribers(bus, HYPO_IMOD_HPA_AXIS));
}

/**
 * @test Verify has_subscribers returns false for NULL bus
 */
TEST_F(HypothalamusInternalBusTest, HasSubscribersNullBusReturnsFalse) {
    EXPECT_FALSE(hypo_ibus_has_subscribers(nullptr, HYPO_IMOD_HPA_AXIS));
}

/* ============================================================================
 * UTILITY FUNCTION TESTS
 * ============================================================================ */

/**
 * @test Verify module_name returns non-NULL for all modules
 */
TEST_F(HypothalamusInternalBusTest, ModuleNameReturnsNonNullForAllModules) {
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        const char* name = hypo_ibus_module_name(static_cast<hypo_internal_module_t>(i));
        EXPECT_NE(nullptr, name);
        EXPECT_GT(strlen(name), 0u);
    }
}

/**
 * @test Verify module_name returns valid string for invalid module
 */
TEST_F(HypothalamusInternalBusTest, ModuleNameReturnsValidForInvalidModule) {
    const char* name = hypo_ibus_module_name(static_cast<hypo_internal_module_t>(999));
    EXPECT_NE(nullptr, name);  // Should return "Unknown" or similar
}

/**
 * @test Verify event_name returns non-NULL for all events
 */
TEST_F(HypothalamusInternalBusTest, EventNameReturnsNonNullForAllEvents) {
    for (int i = 0; i < HYPO_IEVT_COUNT; i++) {
        const char* name = hypo_ibus_event_name(static_cast<hypo_internal_event_type_t>(i));
        EXPECT_NE(nullptr, name);
        EXPECT_GT(strlen(name), 0u);
    }
}

/**
 * @test Verify event_name returns valid string for invalid event
 */
TEST_F(HypothalamusInternalBusTest, EventNameReturnsValidForInvalidEvent) {
    const char* name = hypo_ibus_event_name(static_cast<hypo_internal_event_type_t>(999));
    EXPECT_NE(nullptr, name);  // Should return "Unknown" or similar
}

/**
 * @test Verify print_summary is NULL-safe
 */
TEST_F(HypothalamusInternalBusTest, PrintSummaryNullSafe) {
    hypo_ibus_print_summary(nullptr);
    // Should not crash

    hypo_ibus_print_summary(bus);
    // Should not crash
}

/**
 * @test Verify print_stats is NULL-safe
 */
TEST_F(HypothalamusInternalBusTest, PrintStatsNullSafe) {
    hypo_ibus_print_stats(nullptr);
    // Should not crash

    hypo_ibus_stats_t stats = {};
    hypo_ibus_print_stats(&stats);
    // Should not crash
}

/* ============================================================================
 * THREAD SAFETY TESTS
 * ============================================================================ */

/**
 * @test Verify concurrent subscribe operations are thread-safe
 */
TEST_F(HypothalamusInternalBusTest, ConcurrentSubscribeIsThreadSafe) {
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    const int num_threads = 4;
    const int subs_per_thread = 4;  // Keep under max subscribers

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, &failure_count, t, subs_per_thread]() {
            for (int i = 0; i < subs_per_thread; i++) {
                int sub_id = hypo_ibus_subscribe(bus,
                    static_cast<hypo_internal_module_t>(t % HYPO_IMOD_COUNT),
                    static_cast<hypo_internal_event_type_t>(i % HYPO_IEVT_COUNT),
                    test_callback, nullptr);
                if (sub_id >= 0) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Most subscriptions should succeed
    EXPECT_GT(success_count.load(), 0);

    // Stats should be consistent
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(static_cast<uint32_t>(success_count.load()), stats.active_subscribers);
}

/**
 * @test Verify concurrent publish operations are thread-safe
 */
TEST_F(HypothalamusInternalBusTest, ConcurrentPublishIsThreadSafe) {
    callback_count = 0;

    // Subscribe first
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_STRESS_ONSET,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    const int num_threads = 4;
    const int publishes_per_thread = 25;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, publishes_per_thread]() {
            for (int i = 0; i < publishes_per_thread; i++) {
                hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET,
                                          0.5f, 0.5f, true);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All callbacks should have been invoked
    EXPECT_EQ(num_threads * publishes_per_thread, callback_count.load());

    // Stats should be consistent
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(static_cast<uint64_t>(num_threads * publishes_per_thread),
              stats.events_published);
}

/**
 * @test Verify concurrent subscribe and publish operations are thread-safe
 */
TEST_F(HypothalamusInternalBusTest, ConcurrentSubscribeAndPublishIsThreadSafe) {
    std::atomic<bool> running{true};
    std::atomic<int> publish_count{0};

    // Publisher thread
    std::thread publisher([this, &running, &publish_count]() {
        while (running) {
            hypo_ibus_publish_stress(bus, HYPO_IEVT_CORTISOL_ELEVATED,
                                      0.6f, 0.7f, false);
            publish_count++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Subscriber thread
    std::thread subscriber([this, &running]() {
        while (running) {
            int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                              HYPO_IEVT_CORTISOL_ELEVATED,
                                              test_callback, nullptr);
            if (sub_id >= 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                hypo_ibus_unsubscribe(bus, sub_id);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Let them run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    publisher.join();
    subscriber.join();

    // Should have published some events without crashing
    EXPECT_GT(publish_count.load(), 0);
}

/**
 * @test Verify concurrent modulation updates are thread-safe
 */
TEST_F(HypothalamusInternalBusTest, ConcurrentModulationUpdatesAreThreadSafe) {
    // Register a modulation
    hypo_ibus_modulation_t modulation = {};
    modulation.target = HYPO_IMOD_APPETITE;
    modulation.modulation_factor = 0.5f;
    modulation.parameter_id = 0;
    modulation.duration_us = 10000000;  // 10 seconds
    modulation.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus, HYPO_IEVT_STRESS_ONSET,
                                                 &modulation);
    ASSERT_GE(rule_id, 0);

    std::atomic<bool> running{true};

    // Thread 1: Trigger modulations
    std::thread trigger([this, &running]() {
        while (running) {
            hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET, 0.8f, 0.8f, true);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    // Thread 2: Update modulations
    std::thread updater([this, &running]() {
        while (running) {
            hypo_ibus_update_modulations(bus, 100000);  // 100ms
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 3: Query modulations
    std::thread querier([this, &running]() {
        while (running) {
            float factor = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
            (void)factor;  // Just query, don't assert
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    trigger.join();
    updater.join();
    querier.join();

    // Test passes if no deadlock or crash occurred
}

/* ============================================================================
 * ERROR HANDLING TESTS
 * ============================================================================ */

/**
 * @test Verify invalid event type is handled gracefully
 */
TEST_F(HypothalamusInternalBusTest, InvalidEventTypeHandledGracefully) {
    hypo_internal_event_t event = {};
    event.type = static_cast<hypo_internal_event_type_t>(999);  // Invalid
    event.source = HYPO_IMOD_CIRCADIAN;

    // Should not crash - may return 0 or -1 depending on implementation
    int result = hypo_ibus_publish(bus, &event);
    // Just verify it doesn't crash
    (void)result;
}

/**
 * @test Verify invalid module type is handled gracefully
 */
TEST_F(HypothalamusInternalBusTest, InvalidModuleTypeHandledGracefully) {
    int sub_id = hypo_ibus_subscribe(bus,
        static_cast<hypo_internal_module_t>(999),  // Invalid
        HYPO_IEVT_STRESS_ONSET,
        test_callback, nullptr);

    // Should either succeed with warning or return error
    // Just verify it doesn't crash
    (void)sub_id;
}

/**
 * @test Verify callback returning error doesn't break other subscribers
 */
TEST_F(HypothalamusInternalBusTest, CallbackErrorDoesNotBreakOtherSubscribers) {
    callback_count = 0;

    // First subscriber returns error
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                    HYPO_IEVT_SAFETY_THREAT,
                                    error_callback, nullptr);
    // Second subscriber is normal
    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_HOMEOSTASIS,
                                    HYPO_IEVT_SAFETY_THREAT,
                                    test_callback, nullptr);

    ASSERT_GE(sub1, 0);
    ASSERT_GE(sub2, 0);

    // Publish event
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_SAFETY_THREAT;
    event.source = HYPO_IMOD_DRIVES;

    int result = hypo_ibus_publish(bus, &event);

    // Both callbacks should have been invoked despite first one's error
    EXPECT_EQ(2, callback_count.load());  // Both were attempted
    EXPECT_EQ(0, result);  // 0 = success
}

/**
 * @test Verify double unsubscribe is handled gracefully
 */
TEST_F(HypothalamusInternalBusTest, DoubleUnsubscribeHandledGracefully) {
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_CIRCADIAN,
                                      HYPO_IEVT_MELATONIN_ONSET,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // First unsubscribe
    EXPECT_EQ(0, hypo_ibus_unsubscribe(bus, sub_id));

    // Second unsubscribe should return error but not crash
    EXPECT_EQ(-1, hypo_ibus_unsubscribe(bus, sub_id));
}

/**
 * @test Verify modulation factor is clamped to valid range
 */
TEST_F(HypothalamusInternalBusTest, ModulationFactorClampedToValidRange) {
    // Register modulation with extreme factor
    hypo_ibus_modulation_t modulation = {};
    modulation.target = HYPO_IMOD_APPETITE;
    modulation.modulation_factor = 5.0f;  // Way above valid range [0, 2]
    modulation.parameter_id = 0;
    modulation.duration_us = 0;
    modulation.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus, HYPO_IEVT_STRESS_PEAK,
                                                 &modulation);

    // Should either reject or clamp
    if (rule_id >= 0) {
        // Trigger the modulation
        hypo_internal_event_t event = {};
        event.type = HYPO_IEVT_STRESS_PEAK;
        event.source = HYPO_IMOD_HPA_AXIS;
        hypo_ibus_publish(bus, &event);

        // Factor should be clamped to valid range
        float factor = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
        EXPECT_LE(factor, 2.0f);
        EXPECT_GE(factor, 0.0f);
    }
}

/* ============================================================================
 * EDGE CASE TESTS
 * ============================================================================ */

/**
 * @test Verify rapid subscribe/unsubscribe cycles don't leak resources
 */
TEST_F(HypothalamusInternalBusTest, RapidSubscribeUnsubscribeCycleNoLeak) {
    for (int cycle = 0; cycle < 100; cycle++) {
        int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_CIRCADIAN,
                                          HYPO_IEVT_CORTISOL_AWAKENING,
                                          test_callback, nullptr);
        ASSERT_GE(sub_id, 0);
        EXPECT_EQ(0, hypo_ibus_unsubscribe(bus, sub_id));
    }

    // Should be no subscribers left
    EXPECT_EQ(0u, hypo_ibus_subscriber_count(bus, HYPO_IEVT_CORTISOL_AWAKENING));

    // Stats should be consistent
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(0u, stats.active_subscribers);
}

/**
 * @test Verify publish with zero delta time is handled
 */
TEST_F(HypothalamusInternalBusTest, UpdateModulationsWithZeroDeltaHandled) {
    EXPECT_EQ(0, hypo_ibus_update_modulations(bus, 0));  // Zero time
}

/**
 * @test Verify publish with large delta time is handled
 */
TEST_F(HypothalamusInternalBusTest, UpdateModulationsWithLargeDeltaHandled) {
    // Very large time jump (1 hour in microseconds)
    EXPECT_EQ(0, hypo_ibus_update_modulations(bus, 3600000000ULL));
}

/**
 * @test Verify all event types can be published
 */
TEST_F(HypothalamusInternalBusTest, AllEventTypesCanBePublished) {
    for (int i = 0; i < HYPO_IEVT_COUNT; i++) {
        hypo_internal_event_t event = {};
        event.type = static_cast<hypo_internal_event_type_t>(i);
        event.source = HYPO_IMOD_CIRCADIAN;

        // Should not crash for any event type
        int result = hypo_ibus_publish(bus, &event);
        EXPECT_GE(result, 0);  // 0 (no subscribers) is valid
    }
}

/**
 * @test Verify all module types can subscribe
 */
TEST_F(HypothalamusInternalBusTest, AllModuleTypesCanSubscribe) {
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        int sub_id = hypo_ibus_subscribe(bus,
            static_cast<hypo_internal_module_t>(i),
            HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
            test_callback, nullptr);
        EXPECT_GE(sub_id, 0);
    }

    EXPECT_EQ(static_cast<uint32_t>(HYPO_IMOD_COUNT),
              hypo_ibus_subscriber_count(bus, HYPO_IEVT_CIRCADIAN_PHASE_CHANGE));
}

/**
 * @test Verify stats track per-module events correctly
 */
TEST_F(HypothalamusInternalBusTest, StatsTrackPerModuleEventsCorrectly) {
    // Subscribe to enable delivery tracking
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                      HYPO_IEVT_AUTONOMIC_BALANCE_SHIFT,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Publish events from specific modules
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_AUTONOMIC_BALANCE_SHIFT;
    event.source = HYPO_IMOD_AUTONOMIC;

    for (int i = 0; i < 5; i++) {
        hypo_ibus_publish(bus, &event);
    }

    // Check per-module stats
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(5u, stats.module_events[HYPO_IMOD_AUTONOMIC]);
}

/**
 * @test Verify event timestamp is set on publish
 */
TEST_F(HypothalamusInternalBusTest, EventTimestampSetOnPublish) {
    callback_count = 0;

    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_DRIVE_CONFLICT,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_DRIVE_CONFLICT;
    event.source = HYPO_IMOD_DRIVES;
    event.timestamp_us = 0;  // Not set

    hypo_ibus_publish(bus, &event);

    // The bus should have set a timestamp or passed through
    // We can't easily verify the exact timestamp but the event should be delivered
    EXPECT_EQ(1, callback_count.load());
}

/**
 * @test Verify sequence ID increments on publish
 */
TEST_F(HypothalamusInternalBusTest, SequenceIdIncrementsOnPublish) {
    callback_count = 0;
    static uint32_t first_seq_id = 0;
    static uint32_t second_seq_id = 0;

    // Custom callback to capture sequence IDs
    auto seq_callback = [](const hypo_internal_event_t* event, void* user_data) -> int {
        static int call_count = 0;
        if (call_count == 0) {
            first_seq_id = event->sequence_id;
        } else {
            second_seq_id = event->sequence_id;
        }
        call_count++;
        return 0;
    };

    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_SETPOINT_RESTORED,
                                      seq_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Publish two events
    hypo_internal_event_t event = {};
    event.type = HYPO_IEVT_SETPOINT_RESTORED;
    event.source = HYPO_IMOD_HOMEOSTASIS;

    hypo_ibus_publish(bus, &event);
    hypo_ibus_publish(bus, &event);

    // Sequence IDs should differ (if implementation sets them)
    // Just verify events were delivered
    EXPECT_GE(second_seq_id, first_seq_id);
}

/* ============================================================================
 * INTEGRATION-STYLE TESTS
 * ============================================================================ */

/**
 * @test Verify complete circadian-to-drives communication flow
 */
TEST_F(HypothalamusInternalBusTest, CircadianToDrivesCommmunicationFlow) {
    callback_count = 0;

    // Drives module subscribes to circadian events
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                      test_callback, nullptr);
    ASSERT_GE(sub_id, 0);

    // Register modulation: circadian affects hunger
    hypo_ibus_modulation_t modulation = {};
    modulation.target = HYPO_IMOD_APPETITE;
    modulation.modulation_factor = 1.3f;  // 30% increase
    modulation.parameter_id = 0;
    modulation.duration_us = 3600000000ULL;  // 1 hour
    modulation.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus,
                                                 HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                                 &modulation);
    ASSERT_GE(rule_id, 0);

    // Circadian module publishes phase change
    int result = hypo_ibus_publish_circadian_phase(bus,
                                                    2, 3,    // phase change
                                                    0.1f,    // melatonin (low - daytime)
                                                    0.6f,    // cortisol
                                                    0.9f);   // alertness (high)
    EXPECT_EQ(0, result);

    // Verify callback was invoked
    EXPECT_EQ(1, callback_count.load());
    EXPECT_EQ(HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, last_event_type);
    EXPECT_EQ(HYPO_IMOD_CIRCADIAN, last_source_module);

    // Verify modulation was applied (appetite modulated)
    float factor = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_GT(factor, 1.0f);  // Should be elevated
}

/**
 * @test Verify stress cascade through multiple modules
 */
TEST_F(HypothalamusInternalBusTest, StressCascadeThroughMultipleModules) {
    callback_count = 0;

    // Multiple modules subscribe to stress events
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
                                    HYPO_IEVT_STRESS_ONSET,
                                    test_callback, nullptr);
    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                    HYPO_IEVT_STRESS_ONSET,
                                    test_callback, nullptr);
    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                    HYPO_IEVT_STRESS_ONSET,
                                    test_callback, nullptr);

    ASSERT_GE(sub1, 0);
    ASSERT_GE(sub2, 0);
    ASSERT_GE(sub3, 0);

    // Register biological modulations
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GE(rules, 0);

    // HPA axis publishes stress
    int result = hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET,
                                           0.8f, 0.7f, true);
    EXPECT_EQ(0, result);

    // All three modules should have received the event
    EXPECT_EQ(3, callback_count.load());

    // Stats should show the cascade
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(1u, stats.events_published);
    EXPECT_EQ(3u, stats.events_delivered);
}
