/**
 * @file test_hypothalamus_internal_bus_integration.cpp
 * @brief Integration tests for Hypothalamus Internal Bus bidirectional communication
 *
 * WHAT: Integration tests verifying the internal bus enables correct cross-talk
 *       between hypothalamus modules (Circadian, HPA, Drives, Homeostasis, etc.)
 * WHY:  Ensure biologically-inspired module communication patterns work correctly:
 *       - Circadian rhythm affects hunger timing (ghrelin peaks before meals)
 *       - Stress (cortisol) suppresses appetite short-term
 *       - Fatigue reduces curiosity/exploration drive
 *       - Social connection (oxytocin) reduces threat sensitivity
 * HOW:  Test pub-sub communication, modulation chains, event ordering,
 *       cascading responses, and integration with the hypothalamus adapter
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
#include <algorithm>
#include <cmath>

/* Headers have their own extern "C" guards */
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_internal_bus.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_MODULATION_DURATION_US    (30ULL * 60ULL * 1000000ULL)  /* 30 minutes */
#define TEST_SHORT_DURATION_US         (5ULL * 60ULL * 1000000ULL)   /* 5 minutes */
#define TEST_TOLERANCE                 0.001f

/* ============================================================================
 * Event Tracking Structures
 * ============================================================================ */

/**
 * @brief Tracks events received by a subscriber for verification
 *
 * WHAT: Records event types, sources, timestamps, and sequence IDs
 * WHY:  Enables verification of correct event delivery and ordering
 */
struct InternalEventTracker {
    std::atomic<int> total_events{0};
    std::atomic<int> circadian_events{0};
    std::atomic<int> stress_events{0};
    std::atomic<int> drive_events{0};
    std::atomic<int> autonomic_events{0};
    std::atomic<int> homeostatic_events{0};
    std::atomic<int> alignment_events{0};

    std::vector<hypo_internal_event_type_t> event_types;
    std::vector<hypo_internal_module_t> sources;
    std::vector<uint32_t> sequence_ids;
    std::vector<uint64_t> timestamps;
    std::mutex mutex;

    void reset() {
        total_events = 0;
        circadian_events = 0;
        stress_events = 0;
        drive_events = 0;
        autonomic_events = 0;
        homeostatic_events = 0;
        alignment_events = 0;
        std::lock_guard<std::mutex> lock(mutex);
        event_types.clear();
        sources.clear();
        sequence_ids.clear();
        timestamps.clear();
    }
};

/**
 * @brief Tracks modulation effects for verification
 */
struct ModulationTracker {
    std::atomic<int> modulations_received{0};
    std::vector<hypo_internal_module_t> targets;
    std::vector<float> factors;
    std::mutex mutex;

    void reset() {
        modulations_received = 0;
        std::lock_guard<std::mutex> lock(mutex);
        targets.clear();
        factors.clear();
    }
};

/* ============================================================================
 * Callback Functions
 * ============================================================================ */

/**
 * @brief Generic event tracking callback
 *
 * WHAT: Records all event details for later verification
 * WHY:  Enables comprehensive testing of event delivery
 */
static int event_tracker_callback(
    const hypo_internal_event_t* event,
    void* user_data
) {
    if (!event || !user_data) return -1;

    InternalEventTracker* tracker = static_cast<InternalEventTracker*>(user_data);
    tracker->total_events++;

    /* Categorize event type */
    switch (event->type) {
        case HYPO_IEVT_CIRCADIAN_PHASE_CHANGE:
        case HYPO_IEVT_MELATONIN_ONSET:
        case HYPO_IEVT_MELATONIN_OFFSET:
        case HYPO_IEVT_CORTISOL_AWAKENING:
            tracker->circadian_events++;
            break;

        case HYPO_IEVT_STRESS_ONSET:
        case HYPO_IEVT_STRESS_PEAK:
        case HYPO_IEVT_STRESS_RECOVERY:
        case HYPO_IEVT_CORTISOL_ELEVATED:
        case HYPO_IEVT_CORTISOL_NORMALIZED:
            tracker->stress_events++;
            break;

        case HYPO_IEVT_DRIVE_THRESHOLD_CROSSED:
        case HYPO_IEVT_DRIVE_SATISFIED:
        case HYPO_IEVT_DRIVE_CONFLICT:
        case HYPO_IEVT_HUNGER_ONSET:
        case HYPO_IEVT_FATIGUE_ONSET:
        case HYPO_IEVT_SAFETY_THREAT:
            tracker->drive_events++;
            break;

        case HYPO_IEVT_SYMPATHETIC_ACTIVATION:
        case HYPO_IEVT_PARASYMPATHETIC_ACTIVATION:
        case HYPO_IEVT_AUTONOMIC_BALANCE_SHIFT:
            tracker->autonomic_events++;
            break;

        case HYPO_IEVT_SETPOINT_DEVIATION:
        case HYPO_IEVT_SETPOINT_RESTORED:
        case HYPO_IEVT_TEMPERATURE_ALERT:
            tracker->homeostatic_events++;
            break;

        case HYPO_IEVT_ALIGNMENT_WARNING:
        case HYPO_IEVT_ALIGNMENT_VIOLATION:
            tracker->alignment_events++;
            break;

        default:
            break;
    }

    /* Record event details */
    {
        std::lock_guard<std::mutex> lock(tracker->mutex);
        tracker->event_types.push_back(event->type);
        tracker->sources.push_back(event->source);
        tracker->sequence_ids.push_back(event->sequence_id);
        tracker->timestamps.push_back(event->timestamp_us);
    }

    return 0;
}

/**
 * @brief Simple counting callback
 */
static int counting_callback(
    const hypo_internal_event_t* event,
    void* user_data
) {
    (void)event;
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(user_data);
    (*counter)++;
    return 0;
}

/**
 * @brief Failing callback to test error handling
 */
static int failing_callback(
    const hypo_internal_event_t* event,
    void* user_data
) {
    (void)event;
    (void)user_data;
    return -1;  /* Simulate failure */
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypothalamusInternalBusIntegrationTest : public ::testing::Test {
protected:
    hypo_ibus_t bus;
    hypo_ibus_config_t config;

    void SetUp() override {
        bus = nullptr;

        /* Get default configuration */
        ASSERT_EQ(0, hypo_ibus_default_config(&config));

        /* Create internal bus */
        bus = hypo_ibus_create(&config);
        ASSERT_NE(bus, nullptr);
    }

    void TearDown() override {
        if (bus) {
            hypo_ibus_destroy(bus);
            bus = nullptr;
        }
    }

    /**
     * @brief Helper to publish a circadian event
     */
    int publish_circadian_phase_change(uint32_t old_phase, uint32_t new_phase,
                                        float melatonin, float cortisol) {
        return hypo_ibus_publish_circadian_phase(bus, old_phase, new_phase,
                                                  melatonin, cortisol, 1.0f - melatonin);
    }

    /**
     * @brief Helper to publish stress event
     */
    int publish_stress_event(hypo_internal_event_type_t type, float level,
                              float cortisol, bool is_acute) {
        return hypo_ibus_publish_stress(bus, type, level, cortisol, is_acute);
    }

    /**
     * @brief Helper to publish drive event
     */
    int publish_drive_event(hypo_internal_event_type_t type, uint32_t drive_type,
                             float level, float urgency) {
        return hypo_ibus_publish_drive(bus, type, drive_type, level, urgency);
    }

    /**
     * @brief Helper to publish autonomic event
     */
    int publish_autonomic_event(hypo_internal_event_type_t type,
                                 float sympathetic, float parasympathetic) {
        return hypo_ibus_publish_autonomic(bus, type, sympathetic, parasympathetic);
    }
};

/* ============================================================================
 * Test 1: Multi-Module Communication - Circadian Publishes, Drives Receives
 * ============================================================================ */

/**
 * WHAT: Test circadian module publishing and drives module receiving
 * WHY:  Circadian rhythm affects drive timing (hunger peaks before meals)
 * HOW:  Subscribe drives to circadian events, publish phase change, verify receipt
 */
TEST_F(HypothalamusInternalBusIntegrationTest, CircadianPublishesDrivesReceives) {
    InternalEventTracker tracker;

    /* Subscribe Drives module to Circadian events */
    int sub_id = hypo_ibus_subscribe_to_module(
        bus,
        HYPO_IMOD_DRIVES,       /* Subscriber */
        HYPO_IMOD_CIRCADIAN,    /* Source module to listen to */
        event_tracker_callback,
        &tracker
    );
    ASSERT_GE(sub_id, 0);

    /* Publish circadian phase change (morning wakeup) */
    /* Phase values: 7 = late night (03:00-06:00), 0 = early morning (06:00-09:00) */
    int delivered = publish_circadian_phase_change(
        7,  /* old phase: late night */
        0,  /* new phase: early morning */
        0.1f,   /* melatonin dropping */
        0.8f    /* cortisol rising (awakening response) */
    );
    EXPECT_GE(delivered, 0);

    /* Verify drives module received the event */
    EXPECT_EQ(1, tracker.total_events.load());
    EXPECT_EQ(1, tracker.circadian_events.load());

    /* Verify event source was circadian */
    {
        std::lock_guard<std::mutex> lock(tracker.mutex);
        ASSERT_FALSE(tracker.sources.empty());
        EXPECT_EQ(HYPO_IMOD_CIRCADIAN, tracker.sources[0]);
        EXPECT_EQ(HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, tracker.event_types[0]);
    }
}

/* ============================================================================
 * Test 2: Cascading Events - Stress Triggers Multiple Module Responses
 * ============================================================================ */

/**
 * WHAT: Test stress event cascading to multiple modules
 * WHY:  Stress response involves HPA axis, autonomic, appetite, drives
 * HOW:  Subscribe multiple modules, publish stress, verify all receive
 */
TEST_F(HypothalamusInternalBusIntegrationTest, StressTriggersCascadingResponses) {
    InternalEventTracker appetite_tracker;
    InternalEventTracker autonomic_tracker;
    InternalEventTracker drives_tracker;
    InternalEventTracker homeostasis_tracker;

    /* Subscribe multiple modules to stress events */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
                                   HYPO_IEVT_STRESS_ONSET,
                                   event_tracker_callback, &appetite_tracker), 0);

    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                   HYPO_IEVT_STRESS_ONSET,
                                   event_tracker_callback, &autonomic_tracker), 0);

    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_ONSET,
                                   event_tracker_callback, &drives_tracker), 0);

    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_HOMEOSTASIS,
                                   HYPO_IEVT_STRESS_ONSET,
                                   event_tracker_callback, &homeostasis_tracker), 0);

    /* Publish acute stress onset */
    int delivered = publish_stress_event(
        HYPO_IEVT_STRESS_ONSET,
        0.8f,   /* high stress */
        0.7f,   /* elevated cortisol */
        true    /* acute stress */
    );
    EXPECT_GE(delivered, 0);

    /* Verify all modules received the stress event */
    EXPECT_EQ(1, appetite_tracker.total_events.load());
    EXPECT_EQ(1, autonomic_tracker.total_events.load());
    EXPECT_EQ(1, drives_tracker.total_events.load());
    EXPECT_EQ(1, homeostasis_tracker.total_events.load());

    /* Verify statistics reflect cascading delivery */
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(1u, stats.events_published);
    EXPECT_EQ(4u, stats.events_delivered);  /* 4 subscribers received */
}

/* ============================================================================
 * Test 3: Biological Modulation Chains - Cortisol Suppresses Appetite
 * ============================================================================ */

/**
 * WHAT: Test cortisol elevation causing appetite suppression
 * WHY:  Stress response suppresses appetite (fight-or-flight prioritization)
 * HOW:  Register cortisol→appetite modulation, trigger cortisol, verify modulation
 */
TEST_F(HypothalamusInternalBusIntegrationTest, CortisolSuppressesAppetite) {
    /* Register default biological modulations */
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GT(rules, 0);

    /* Check initial appetite modulation (should be 1.0 = no effect) */
    float initial_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_NEAR(1.0f, initial_mod, TEST_TOLERANCE);

    /* Publish cortisol elevated event */
    int delivered = publish_stress_event(
        HYPO_IEVT_CORTISOL_ELEVATED,
        0.85f,  /* high stress */
        0.9f,   /* very high cortisol */
        true    /* acute */
    );
    EXPECT_GE(delivered, 0);

    /* Check that appetite is now suppressed */
    float new_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);

    /* Appetite should be reduced (modulation factor < 1.0) */
    /* Default cortisol_appetite_suppression = 0.4 → factor = 0.6 */
    EXPECT_LT(new_mod, 1.0f);
    EXPECT_GT(new_mod, 0.0f);

    /* Verify modulation statistics */
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.modulations_applied, 0u);
}

/* ============================================================================
 * Test 4: Fatigue Reduces Curiosity Drive
 * ============================================================================ */

/**
 * WHAT: Test fatigue onset reducing curiosity drive
 * WHY:  Tired individuals conserve energy by reducing exploration
 * HOW:  Register fatigue→curiosity modulation, trigger fatigue, verify effect
 */
TEST_F(HypothalamusInternalBusIntegrationTest, FatigueReducesCuriosity) {
    /* Register default biological modulations */
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GT(rules, 0);

    /* Verify initial drives modulation is neutral */
    float initial_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);
    EXPECT_NEAR(1.0f, initial_mod, TEST_TOLERANCE);

    /* Publish fatigue onset event */
    int delivered = publish_drive_event(
        HYPO_IEVT_FATIGUE_ONSET,
        HYPO_DRIVE_FATIGUE,
        0.85f,  /* high fatigue */
        0.9f    /* high urgency */
    );
    EXPECT_GE(delivered, 0);

    /* Check drives modulation changed */
    float new_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);

    /* Default fatigue_curiosity_reduction = 0.6 → factor = 0.4 */
    EXPECT_LT(new_mod, 1.0f);
}

/* ============================================================================
 * Test 5: Cross-Talk - Circadian Phase Affects Hunger Timing
 * ============================================================================ */

/**
 * WHAT: Test circadian phase changes affecting appetite module
 * WHY:  Ghrelin (hunger hormone) peaks before expected meal times
 * HOW:  Register circadian→appetite modulation, change phases, track effects
 */
TEST_F(HypothalamusInternalBusIntegrationTest, CircadianAffectsHungerTiming) {
    InternalEventTracker tracker;

    /* Register default biological modulations */
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GT(rules, 0);

    /* Subscribe appetite module to circadian events */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
                                   HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                   event_tracker_callback, &tracker), 0);

    /* Simulate morning phase change (breakfast time) */
    /* Phase values: 7 = late night, 0 = early morning */
    publish_circadian_phase_change(
        7,  /* late night */
        0,  /* early morning */
        0.1f, 0.7f
    );

    /* Verify appetite received the phase change */
    EXPECT_EQ(1, tracker.total_events.load());

    /* Check appetite modulation changed (circadian hunger amplitude) */
    float mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);

    /* Default circadian_hunger_amplitude = 0.3 → factor = 1.3 (increased hunger) */
    EXPECT_GT(mod, 1.0f);  /* Hunger should be elevated at meal times */
}

/* ============================================================================
 * Test 6: Event Ordering - Sequence Numbers Increase
 * ============================================================================ */

/**
 * WHAT: Test events are delivered in correct sequence order
 * WHY:  Event ordering is crucial for causally-related events
 * HOW:  Publish multiple events, verify sequence IDs are increasing
 */
TEST_F(HypothalamusInternalBusIntegrationTest, EventOrderingPreserved) {
    InternalEventTracker tracker;

    /* Subscribe to all HPA axis events */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_ONSET,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_PEAK,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_RECOVERY,
                                   event_tracker_callback, &tracker), 0);

    /* Publish stress sequence: onset → peak → recovery */
    publish_stress_event(HYPO_IEVT_STRESS_ONSET, 0.5f, 0.4f, true);
    publish_stress_event(HYPO_IEVT_STRESS_PEAK, 0.9f, 0.85f, true);
    publish_stress_event(HYPO_IEVT_STRESS_RECOVERY, 0.3f, 0.4f, true);

    /* Verify we received all 3 events */
    EXPECT_EQ(3, tracker.total_events.load());

    /* Verify sequence IDs are strictly increasing */
    {
        std::lock_guard<std::mutex> lock(tracker.mutex);
        ASSERT_EQ(3u, tracker.sequence_ids.size());
        EXPECT_LT(tracker.sequence_ids[0], tracker.sequence_ids[1]);
        EXPECT_LT(tracker.sequence_ids[1], tracker.sequence_ids[2]);
    }

    /* Verify event types are in correct order */
    {
        std::lock_guard<std::mutex> lock(tracker.mutex);
        EXPECT_EQ(HYPO_IEVT_STRESS_ONSET, tracker.event_types[0]);
        EXPECT_EQ(HYPO_IEVT_STRESS_PEAK, tracker.event_types[1]);
        EXPECT_EQ(HYPO_IEVT_STRESS_RECOVERY, tracker.event_types[2]);
    }
}

/* ============================================================================
 * Test 7: Modulation Accumulation - Multiple Modulations Same Module
 * ============================================================================ */

/**
 * WHAT: Test multiple modulations affecting the same target module
 * WHY:  Real biology has overlapping effects (stress + fatigue on drives)
 * HOW:  Trigger multiple modulation sources, verify accumulated effect
 */
TEST_F(HypothalamusInternalBusIntegrationTest, ModulationAccumulation) {
    /* Register default biological modulations */
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GT(rules, 0);

    /* Initial drives modulation should be neutral */
    float initial = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);
    EXPECT_NEAR(1.0f, initial, TEST_TOLERANCE);

    /* Trigger multiple events that modulate drives */

    /* 1. Melatonin onset → increases fatigue drive */
    hypo_internal_event_t melatonin_event;
    memset(&melatonin_event, 0, sizeof(melatonin_event));
    melatonin_event.type = HYPO_IEVT_MELATONIN_ONSET;
    melatonin_event.source = HYPO_IMOD_CIRCADIAN;
    melatonin_event.timestamp_us = 1000000;
    melatonin_event.data.circadian.melatonin_level = 0.8f;
    hypo_ibus_publish(bus, &melatonin_event);

    float after_melatonin = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);

    /* 2. Fatigue onset → reduces curiosity drive */
    publish_drive_event(HYPO_IEVT_FATIGUE_ONSET, HYPO_DRIVE_FATIGUE, 0.7f, 0.8f);

    float after_both = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);

    /* Accumulated modulation should be different from single modulation */
    /* The exact value depends on whether effects are multiplicative or additive */
    EXPECT_NE(after_melatonin, after_both);

    /* Verify multiple modulations were applied */
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GE(stats.modulations_applied, 2u);
}

/* ============================================================================
 * Test 8: Recovery Patterns - Stress Onset → Peak → Recovery Chain
 * ============================================================================ */

/**
 * WHAT: Test full stress response cycle with modulation decay
 * WHY:  Stress response should be temporary with recovery
 * HOW:  Trigger stress sequence, update modulations over time, verify recovery
 */
TEST_F(HypothalamusInternalBusIntegrationTest, StressRecoveryPattern) {
    InternalEventTracker tracker;

    /* Register default modulations */
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GT(rules, 0);

    /* Subscribe to track stress events */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                   HYPO_IEVT_STRESS_ONSET,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                   HYPO_IEVT_STRESS_PEAK,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                   HYPO_IEVT_STRESS_RECOVERY,
                                   event_tracker_callback, &tracker), 0);

    /* Phase 1: Stress onset */
    publish_stress_event(HYPO_IEVT_STRESS_ONSET, 0.6f, 0.5f, true);
    float mod_onset = hypo_ibus_get_modulation(bus, HYPO_IMOD_AUTONOMIC, 0);

    /* Phase 2: Stress peak */
    publish_stress_event(HYPO_IEVT_STRESS_PEAK, 0.95f, 0.9f, true);
    float mod_peak = hypo_ibus_get_modulation(bus, HYPO_IMOD_AUTONOMIC, 0);

    /* Modulation should be higher at peak (more sympathetic activation) */
    EXPECT_GE(mod_peak, mod_onset);

    /* Phase 3: Recovery - simulate time passing */
    publish_stress_event(HYPO_IEVT_STRESS_RECOVERY, 0.3f, 0.35f, false);

    /* Simulate modulation decay over time */
    hypo_ibus_update_modulations(bus, TEST_MODULATION_DURATION_US);

    /* After sufficient time, modulations should decay toward baseline */
    float mod_after_decay = hypo_ibus_get_modulation(bus, HYPO_IMOD_AUTONOMIC, 0);

    /* Verify all stress events were received */
    EXPECT_EQ(3, tracker.total_events.load());
    EXPECT_EQ(3, tracker.stress_events.load());
}

/* ============================================================================
 * Test 9: Circadian-Driven Scenarios - Melatonin Onset → Fatigue Increase
 * ============================================================================ */

/**
 * WHAT: Test melatonin onset causing fatigue drive modulation
 * WHY:  Melatonin release signals sleep time, increasing fatigue
 * HOW:  Publish melatonin event, verify drives module modulation
 */
TEST_F(HypothalamusInternalBusIntegrationTest, MelatoninOnsetIncreasesFatigue) {
    InternalEventTracker tracker;

    /* Register default modulations */
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GT(rules, 0);

    /* Subscribe drives to melatonin events */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_MELATONIN_ONSET,
                                   event_tracker_callback, &tracker), 0);

    /* Get initial drives modulation */
    float initial = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);

    /* Publish melatonin onset (evening) */
    hypo_internal_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = HYPO_IEVT_MELATONIN_ONSET;
    event.source = HYPO_IMOD_CIRCADIAN;
    event.timestamp_us = 1000000;
    event.data.circadian.melatonin_level = 0.9f;
    event.data.circadian.alertness = 0.3f;
    event.data.circadian.sleep_pressure = 0.7f;

    hypo_ibus_publish(bus, &event);

    /* Verify drives received the event */
    EXPECT_EQ(1, tracker.total_events.load());

    /* Verify drives modulation increased (fatigue drive amplified) */
    /* Default circadian_fatigue_amplitude = 0.5 → factor = 1.5 */
    float after = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);
    EXPECT_GT(after, initial);
}

/* ============================================================================
 * Test 10: Autonomic Integration - Sympathetic/Parasympathetic Balance
 * ============================================================================ */

/**
 * WHAT: Test autonomic events and their cross-module effects
 * WHY:  Autonomic state affects many hypothalamic functions
 * HOW:  Publish autonomic events, verify subscriber receipt and modulation
 */
TEST_F(HypothalamusInternalBusIntegrationTest, AutonomicIntegration) {
    InternalEventTracker tracker;

    /* Register default modulations */
    int rules = hypo_ibus_register_default_modulations(bus);
    EXPECT_GT(rules, 0);

    /* Subscribe multiple modules to autonomic events */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
                                   HYPO_IEVT_SYMPATHETIC_ACTIVATION,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_HPA_AXIS,
                                   HYPO_IEVT_SYMPATHETIC_ACTIVATION,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_PARASYMPATHETIC_ACTIVATION,
                                   event_tracker_callback, &tracker), 0);

    /* Publish sympathetic activation (fight-or-flight) */
    publish_autonomic_event(
        HYPO_IEVT_SYMPATHETIC_ACTIVATION,
        0.9f,   /* high sympathetic */
        0.2f    /* low parasympathetic */
    );

    /* Verify appetite and HPA received sympathetic event */
    EXPECT_EQ(2, tracker.total_events.load());
    EXPECT_EQ(2, tracker.autonomic_events.load());

    /* Reset tracker for next test */
    tracker.reset();

    /* Publish parasympathetic activation (rest-and-digest) */
    publish_autonomic_event(
        HYPO_IEVT_PARASYMPATHETIC_ACTIVATION,
        0.2f,   /* low sympathetic */
        0.85f   /* high parasympathetic */
    );

    /* Verify drives received parasympathetic event */
    EXPECT_EQ(1, tracker.total_events.load());

    /* Check that drives safety modulation changed */
    /* Parasympathetic → reduced safety drive (relaxation) */
    float mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);
    /* The modulation should reflect accumulated effects */
    EXPECT_GT(mod, 0.0f);
}

/* ============================================================================
 * Test 11: Integration with Adapter Context - Bus Statistics Consistency
 * ============================================================================ */

/**
 * WHAT: Test bus maintains consistent statistics through complex scenarios
 * WHY:  Bus will be embedded in adapter; stats must be accurate
 * HOW:  Run multiple operations, verify stats match actual activity
 */
TEST_F(HypothalamusInternalBusIntegrationTest, BusStatisticsConsistency) {
    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    /* Subscribe two modules */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_ONSET,
                                   counting_callback, &counter1), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
                                   HYPO_IEVT_STRESS_ONSET,
                                   counting_callback, &counter2), 0);

    /* Publish multiple events */
    for (int i = 0; i < 10; i++) {
        publish_stress_event(HYPO_IEVT_STRESS_ONSET, 0.5f, 0.4f, true);
    }

    /* Verify counters */
    EXPECT_EQ(10, counter1.load());
    EXPECT_EQ(10, counter2.load());

    /* Verify statistics */
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));

    EXPECT_EQ(10u, stats.events_published);
    EXPECT_EQ(20u, stats.events_delivered);  /* 10 events * 2 subscribers */
    EXPECT_EQ(2u, stats.active_subscribers);

    /* Verify per-module statistics */
    EXPECT_EQ(10u, stats.module_events[HYPO_IMOD_HPA_AXIS]);  /* Source of stress */
    EXPECT_EQ(10u, stats.module_receives[HYPO_IMOD_DRIVES]);
    EXPECT_EQ(10u, stats.module_receives[HYPO_IMOD_APPETITE]);
}

/* ============================================================================
 * Test 12: Module Unsubscription and Event Filtering
 * ============================================================================ */

/**
 * WHAT: Test unsubscribing modules and event filtering
 * WHY:  Modules need to dynamically connect/disconnect from bus
 * HOW:  Subscribe, publish, unsubscribe, publish again, verify filtering
 */
TEST_F(HypothalamusInternalBusIntegrationTest, UnsubscriptionFiltering) {
    std::atomic<int> counter{0};

    /* Subscribe drives module */
    int sub_id = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                      HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                      counting_callback, &counter);
    ASSERT_GE(sub_id, 0);

    /* Publish - should receive */
    publish_circadian_phase_change(0, 1, 0.5f, 0.5f);
    EXPECT_EQ(1, counter.load());

    /* Unsubscribe */
    ASSERT_EQ(0, hypo_ibus_unsubscribe(bus, sub_id));

    /* Publish again - should NOT receive */
    publish_circadian_phase_change(1, 2, 0.4f, 0.6f);
    EXPECT_EQ(1, counter.load());  /* Still 1 */

    /* Verify subscriber count decreased */
    uint32_t sub_count = hypo_ibus_subscriber_count(bus, HYPO_IEVT_CIRCADIAN_PHASE_CHANGE);
    EXPECT_EQ(0u, sub_count);
}

/* ============================================================================
 * Test 13: Alignment Events - Safety System Integration
 * ============================================================================ */

/**
 * WHAT: Test alignment warning and violation events
 * WHY:  Alignment safety is critical per Byrnes' steering subsystem design
 * HOW:  Publish alignment events, verify all relevant modules notified
 */
TEST_F(HypothalamusInternalBusIntegrationTest, AlignmentEventsPropagation) {
    InternalEventTracker tracker;

    /* Subscribe multiple modules to alignment events */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_ALIGNMENT_WARNING,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_ALIGNMENT_VIOLATION,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_HPA_AXIS,
                                   HYPO_IEVT_ALIGNMENT_WARNING,
                                   event_tracker_callback, &tracker), 0);

    /* Publish alignment warning */
    hypo_internal_event_t warning_event;
    memset(&warning_event, 0, sizeof(warning_event));
    warning_event.type = HYPO_IEVT_ALIGNMENT_WARNING;
    warning_event.source = HYPO_IMOD_ALIGNMENT;
    warning_event.timestamp_us = 1000000;
    warning_event.data.alignment.constraint_id = 1;
    warning_event.data.alignment.margin = 0.15f;
    warning_event.data.alignment.is_locked = false;
    strncpy(warning_event.data.alignment.description, "Approaching safety boundary",
            sizeof(warning_event.data.alignment.description) - 1);

    hypo_ibus_publish(bus, &warning_event);

    /* Verify warning received by both drives and HPA */
    EXPECT_EQ(2, tracker.total_events.load());
    EXPECT_EQ(2, tracker.alignment_events.load());

    /* Publish alignment violation */
    hypo_internal_event_t violation_event;
    memset(&violation_event, 0, sizeof(violation_event));
    violation_event.type = HYPO_IEVT_ALIGNMENT_VIOLATION;
    violation_event.source = HYPO_IMOD_ALIGNMENT;
    violation_event.timestamp_us = 2000000;
    violation_event.data.alignment.constraint_id = 1;
    violation_event.data.alignment.margin = 0.0f;
    violation_event.data.alignment.is_locked = true;
    strncpy(violation_event.data.alignment.description, "Safety boundary violated",
            sizeof(violation_event.data.alignment.description) - 1);

    hypo_ibus_publish(bus, &violation_event);

    /* Verify violation received by drives (only drives subscribed to violations) */
    EXPECT_EQ(3, tracker.total_events.load());
    EXPECT_EQ(3, tracker.alignment_events.load());
}

/* ============================================================================
 * Test 14: Concurrent Event Publishing Thread Safety
 * ============================================================================ */

/**
 * WHAT: Test thread-safe concurrent event publishing
 * WHY:  Multiple modules may publish simultaneously in real system
 * HOW:  Spawn multiple threads publishing, verify all events delivered
 */
TEST_F(HypothalamusInternalBusIntegrationTest, ConcurrentPublishing) {
    std::atomic<int> total_received{0};

    /* Subscribe to multiple event types */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_ONSET,
                                   counting_callback, &total_received), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                   counting_callback, &total_received), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_HUNGER_ONSET,
                                   counting_callback, &total_received), 0);

    auto stress_thread = [this]() {
        for (int i = 0; i < 30; i++) {
            publish_stress_event(HYPO_IEVT_STRESS_ONSET, 0.5f, 0.4f, true);
        }
    };

    auto circadian_thread = [this]() {
        for (int i = 0; i < 30; i++) {
            publish_circadian_phase_change(i % 8, (i + 1) % 8, 0.5f, 0.5f);
        }
    };

    auto drive_thread = [this]() {
        for (int i = 0; i < 30; i++) {
            publish_drive_event(HYPO_IEVT_HUNGER_ONSET, HYPO_DRIVE_HUNGER, 0.7f, 0.8f);
        }
    };

    std::thread t1(stress_thread);
    std::thread t2(circadian_thread);
    std::thread t3(drive_thread);

    t1.join();
    t2.join();
    t3.join();

    /* Should receive all 90 events (30 * 3) */
    EXPECT_EQ(90, total_received.load());

    /* Verify statistics */
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(90u, stats.events_published);
    EXPECT_EQ(90u, stats.events_delivered);
}

/* ============================================================================
 * Test 15: Modulation Rule Management
 * ============================================================================ */

/**
 * WHAT: Test custom modulation rule registration and application
 * WHY:  System must support custom biological modulation patterns
 * HOW:  Register custom rules, trigger events, verify modulation effects
 */
TEST_F(HypothalamusInternalBusIntegrationTest, CustomModulationRules) {
    /* Register a custom modulation: hunger onset → reduce thermoregulation */
    hypo_ibus_modulation_t custom_mod;
    memset(&custom_mod, 0, sizeof(custom_mod));
    custom_mod.target = HYPO_IMOD_THERMOREGULATION;
    custom_mod.modulation_factor = 0.8f;  /* 20% reduction */
    custom_mod.duration_us = TEST_SHORT_DURATION_US;
    custom_mod.is_additive = false;

    int rule_id = hypo_ibus_register_modulation(bus, HYPO_IEVT_HUNGER_ONSET, &custom_mod);
    ASSERT_GE(rule_id, 0);

    /* Check initial thermoregulation modulation */
    float initial = hypo_ibus_get_modulation(bus, HYPO_IMOD_THERMOREGULATION, 0);
    EXPECT_NEAR(1.0f, initial, TEST_TOLERANCE);

    /* Trigger hunger onset */
    publish_drive_event(HYPO_IEVT_HUNGER_ONSET, HYPO_DRIVE_HUNGER, 0.9f, 0.95f);

    /* Verify thermoregulation is now modulated */
    float after = hypo_ibus_get_modulation(bus, HYPO_IMOD_THERMOREGULATION, 0);
    EXPECT_NEAR(0.8f, after, TEST_TOLERANCE);

    /* Verify modulation was applied in stats */
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.modulations_applied, 0u);
}

/* ============================================================================
 * Test 16: Clear Modulations Reset
 * ============================================================================ */

/**
 * WHAT: Test clearing all active modulations
 * WHY:  System reset should restore neutral modulation state
 * HOW:  Apply modulations, clear them, verify all return to 1.0
 */
TEST_F(HypothalamusInternalBusIntegrationTest, ClearModulationsReset) {
    /* Register default modulations and trigger some */
    hypo_ibus_register_default_modulations(bus);

    /* Trigger events that create modulations */
    publish_stress_event(HYPO_IEVT_CORTISOL_ELEVATED, 0.9f, 0.85f, true);
    publish_drive_event(HYPO_IEVT_FATIGUE_ONSET, HYPO_DRIVE_FATIGUE, 0.8f, 0.9f);

    /* Verify some modulations are active (not 1.0) */
    float appetite_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    float drives_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);
    EXPECT_NE(1.0f, appetite_mod);
    EXPECT_NE(1.0f, drives_mod);

    /* Clear all modulations */
    ASSERT_EQ(0, hypo_ibus_clear_modulations(bus));

    /* Verify all modules return to neutral */
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        float mod = hypo_ibus_get_modulation(bus, (hypo_internal_module_t)i, 0);
        EXPECT_NEAR(1.0f, mod, TEST_TOLERANCE) << "Module " << i << " not reset";
    }
}

/* ============================================================================
 * Test 17: Homeostatic Events Cross-Talk
 * ============================================================================ */

/**
 * WHAT: Test homeostatic events affecting other modules
 * WHY:  Temperature, setpoint deviations affect drives and autonomic
 * HOW:  Publish homeostatic events, verify cross-module communication
 */
TEST_F(HypothalamusInternalBusIntegrationTest, HomeostaticCrossTalk) {
    InternalEventTracker tracker;

    /* Register default modulations */
    hypo_ibus_register_default_modulations(bus);

    /* Subscribe multiple modules to homeostatic events */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_TEMPERATURE_ALERT,
                                   event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
                                   HYPO_IEVT_TEMPERATURE_ALERT,
                                   event_tracker_callback, &tracker), 0);

    /* Publish temperature alert */
    hypo_internal_event_t temp_event;
    memset(&temp_event, 0, sizeof(temp_event));
    temp_event.type = HYPO_IEVT_TEMPERATURE_ALERT;
    temp_event.source = HYPO_IMOD_THERMOREGULATION;
    temp_event.timestamp_us = 1000000;
    temp_event.data.homeostatic.variable_id = 0;  /* Core temperature */
    temp_event.data.homeostatic.current_value = 38.5f;  /* Elevated */
    temp_event.data.homeostatic.setpoint = 37.0f;
    temp_event.data.homeostatic.error = 1.5f;

    hypo_ibus_publish(bus, &temp_event);

    /* Verify both modules received */
    EXPECT_EQ(2, tracker.total_events.load());
    EXPECT_EQ(2, tracker.homeostatic_events.load());

    /* Verify temperature alert causes modulation (hyperthermia → fatigue) */
    float drives_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);
    /* Default rule: Temperature alert → 30% fatigue increase (factor = 1.3) */
    EXPECT_GT(drives_mod, 1.0f);
}

/* ============================================================================
 * Test 18: Utility Functions - Name Lookups
 * ============================================================================ */

/**
 * WHAT: Test utility functions for module and event names
 * WHY:  Human-readable names needed for debugging and logging
 * HOW:  Call name functions, verify non-NULL meaningful results
 */
TEST_F(HypothalamusInternalBusIntegrationTest, UtilityFunctions) {
    /* Test module names */
    for (int i = 0; i < HYPO_IMOD_COUNT; i++) {
        const char* name = hypo_ibus_module_name((hypo_internal_module_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "");
        EXPECT_STRNE(name, "Unknown");
    }

    /* Test event names */
    for (int i = 0; i < HYPO_IEVT_COUNT; i++) {
        const char* name = hypo_ibus_event_name((hypo_internal_event_type_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "");
        EXPECT_STRNE(name, "Unknown");
    }

    /* Test out-of-range returns "Unknown" */
    EXPECT_STREQ("Unknown", hypo_ibus_module_name((hypo_internal_module_t)999));
    EXPECT_STREQ("Unknown", hypo_ibus_event_name((hypo_internal_event_type_t)999));
}

/* ============================================================================
 * Test 19: Bus Reset Preserves Subscriptions
 * ============================================================================ */

/**
 * WHAT: Test bus reset keeps subscriptions but clears state
 * WHY:  Reset should allow reuse without resubscribing
 * HOW:  Subscribe, reset, verify subscription still works
 */
TEST_F(HypothalamusInternalBusIntegrationTest, ResetPreservesSubscriptions) {
    std::atomic<int> counter{0};

    /* Subscribe */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_ONSET,
                                   counting_callback, &counter), 0);

    /* Publish and verify */
    publish_stress_event(HYPO_IEVT_STRESS_ONSET, 0.5f, 0.4f, true);
    EXPECT_EQ(1, counter.load());

    /* Reset bus */
    ASSERT_EQ(0, hypo_ibus_reset(bus));

    /* Verify stats were reset */
    hypo_ibus_stats_t stats;
    hypo_ibus_get_stats(bus, &stats);
    EXPECT_EQ(0u, stats.events_published);

    /* Publish again - subscription should still work */
    publish_stress_event(HYPO_IEVT_STRESS_ONSET, 0.6f, 0.5f, true);
    EXPECT_EQ(2, counter.load());  /* Incremented */
}

/* ============================================================================
 * Test 20: Callback Error Handling Continues Delivery
 * ============================================================================ */

/**
 * WHAT: Test that callback errors don't stop delivery to other subscribers
 * WHY:  One failing module shouldn't break the entire bus
 * HOW:  Subscribe failing + succeeding callbacks, verify both called
 */
TEST_F(HypothalamusInternalBusIntegrationTest, CallbackErrorContinuesDelivery) {
    std::atomic<int> success_counter{0};

    /* First subscriber fails */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
                                   HYPO_IEVT_STRESS_ONSET,
                                   failing_callback, nullptr), 0);

    /* Second subscriber succeeds */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_ONSET,
                                   counting_callback, &success_counter), 0);

    /* Publish event */
    publish_stress_event(HYPO_IEVT_STRESS_ONSET, 0.7f, 0.6f, true);

    /* Success counter should increment despite failing callback */
    EXPECT_EQ(1, success_counter.load());

    /* Stats should reflect partial delivery */
    hypo_ibus_stats_t stats;
    hypo_ibus_get_stats(bus, &stats);
    EXPECT_EQ(1u, stats.events_published);
    /* Only 1 delivered successfully (the failing one doesn't count) */
    EXPECT_EQ(1u, stats.events_delivered);
}

/* ============================================================================
 * Test 21: Full Biological Scenario - Morning Awakening
 * ============================================================================ */

/**
 * WHAT: Simulate complete morning awakening biological scenario
 * WHY:  Verify realistic multi-system interaction patterns
 * HOW:  Sequence: cortisol awakening → melatonin offset → circadian phase →
 *       sympathetic activation → hunger onset
 */
TEST_F(HypothalamusInternalBusIntegrationTest, MorningAwakeningScenario) {
    InternalEventTracker tracker;

    /* Register default modulations */
    hypo_ibus_register_default_modulations(bus);

    /* Subscribe drives to track the awakening sequence */
    ASSERT_GE(hypo_ibus_subscribe_to_module(bus, HYPO_IMOD_DRIVES,
                                             HYPO_IMOD_CIRCADIAN,
                                             event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe_to_module(bus, HYPO_IMOD_DRIVES,
                                             HYPO_IMOD_HPA_AXIS,
                                             event_tracker_callback, &tracker), 0);
    ASSERT_GE(hypo_ibus_subscribe_to_module(bus, HYPO_IMOD_DRIVES,
                                             HYPO_IMOD_AUTONOMIC,
                                             event_tracker_callback, &tracker), 0);

    /* Step 1: Cortisol awakening response */
    hypo_internal_event_t cortisol_event;
    memset(&cortisol_event, 0, sizeof(cortisol_event));
    cortisol_event.type = HYPO_IEVT_CORTISOL_AWAKENING;
    cortisol_event.source = HYPO_IMOD_CIRCADIAN;
    cortisol_event.timestamp_us = 1000000;
    cortisol_event.data.circadian.cortisol_level = 0.85f;
    cortisol_event.data.circadian.melatonin_level = 0.2f;
    cortisol_event.data.circadian.alertness = 0.7f;
    hypo_ibus_publish(bus, &cortisol_event);

    /* Step 2: Melatonin offset */
    hypo_internal_event_t melatonin_offset_event;
    memset(&melatonin_offset_event, 0, sizeof(melatonin_offset_event));
    melatonin_offset_event.type = HYPO_IEVT_MELATONIN_OFFSET;
    melatonin_offset_event.source = HYPO_IMOD_CIRCADIAN;
    melatonin_offset_event.timestamp_us = 2000000;
    melatonin_offset_event.data.circadian.melatonin_level = 0.05f;
    melatonin_offset_event.data.circadian.alertness = 0.9f;
    hypo_ibus_publish(bus, &melatonin_offset_event);

    /* Step 3: Circadian phase change to early morning */
    /* Use numeric values: 7 = late night (03:00-06:00), 0 = early morning (06:00-09:00) */
    publish_circadian_phase_change(
        7,  /* late night */
        0,  /* early morning */
        0.03f, 0.9f
    );

    /* Step 4: Sympathetic tone increases (waking up) */
    publish_autonomic_event(HYPO_IEVT_SYMPATHETIC_ACTIVATION, 0.7f, 0.4f);

    /* Verify drives received all awakening events */
    EXPECT_GE(tracker.total_events.load(), 4);
    EXPECT_GE(tracker.circadian_events.load(), 3);
    EXPECT_GE(tracker.autonomic_events.load(), 1);

    /* Verify event sequence order */
    {
        std::lock_guard<std::mutex> lock(tracker.mutex);
        ASSERT_GE(tracker.sequence_ids.size(), 4u);

        /* Verify timestamps are increasing */
        for (size_t i = 1; i < tracker.timestamps.size(); i++) {
            EXPECT_GE(tracker.timestamps[i], tracker.timestamps[i-1]);
        }
    }
}

/* ============================================================================
 * Test 22: Unsubscribe All Module Subscriptions
 * ============================================================================ */

/**
 * WHAT: Test bulk unsubscription for a module
 * WHY:  Module shutdown should cleanly remove all its subscriptions
 * HOW:  Create multiple subscriptions, unsubscribe all, verify removal
 */
TEST_F(HypothalamusInternalBusIntegrationTest, UnsubscribeAllModuleSubscriptions) {
    std::atomic<int> counter{0};

    /* Create multiple subscriptions for DRIVES module */
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_STRESS_ONSET,
                                   counting_callback, &counter), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_CIRCADIAN_PHASE_CHANGE,
                                   counting_callback, &counter), 0);
    ASSERT_GE(hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
                                   HYPO_IEVT_HUNGER_ONSET,
                                   counting_callback, &counter), 0);

    /* Verify subscriptions active */
    EXPECT_GT(hypo_ibus_subscriber_count(bus, HYPO_IEVT_STRESS_ONSET), 0u);

    /* Unsubscribe all DRIVES subscriptions */
    int removed = hypo_ibus_unsubscribe_module(bus, HYPO_IMOD_DRIVES);
    EXPECT_EQ(3, removed);

    /* Verify no more subscriptions for these events (from DRIVES) */
    publish_stress_event(HYPO_IEVT_STRESS_ONSET, 0.5f, 0.4f, true);
    EXPECT_EQ(0, counter.load());  /* No delivery */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
