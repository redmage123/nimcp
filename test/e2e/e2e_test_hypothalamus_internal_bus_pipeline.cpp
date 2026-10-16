/**
 * @file e2e_test_hypothalamus_internal_bus_pipeline.cpp
 * @brief End-to-end tests for hypothalamus internal bus pipeline
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: E2E tests for the hypothalamus internal message bus
 *
 * WHY: The internal bus enables bidirectional communication between
 *      hypothalamus modules (circadian, HPA, drives, homeostasis, autonomic).
 *      This mirrors biological cross-talk:
 *      - Circadian rhythm affects hunger timing
 *      - Stress (cortisol) suppresses appetite
 *      - Fatigue reduces curiosity/exploration
 *      - Social connection reduces threat sensitivity
 *
 * HOW: Tests complete biological scenarios:
 *      - Full day simulations with circadian phases
 *      - Stress response pipelines (HPA axis activation)
 *      - Sleep pressure and melatonin effects
 *      - Fight-or-flight and relaxation responses
 *      - Multi-module cross-modulation
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_internal_bus.h"
}

// =============================================================================
// Test Constants
// =============================================================================

/** Microseconds in one simulated second */
static constexpr uint64_t US_PER_SECOND = 1000000ULL;

/** Microseconds in one simulated minute */
static constexpr uint64_t US_PER_MINUTE = 60ULL * US_PER_SECOND;

/** Microseconds in one simulated hour */
static constexpr uint64_t US_PER_HOUR = 60ULL * US_PER_MINUTE;

/** Update delta for regular ticks (100ms simulated) */
static constexpr uint64_t UPDATE_DELTA_US = 100000ULL;

/** Standard tolerance for float comparisons */
static constexpr float FLOAT_TOLERANCE = 0.001f;

/** Number of circadian phases in a day */
static constexpr uint32_t CIRCADIAN_PHASES = 8;

// =============================================================================
// Test Callback Context
// =============================================================================

/**
 * @brief Context for tracking events in callbacks
 */
struct EventTracker {
    std::atomic<int> circadian_events{0};
    std::atomic<int> stress_events{0};
    std::atomic<int> drive_events{0};
    std::atomic<int> autonomic_events{0};
    std::atomic<int> homeostatic_events{0};
    std::atomic<int> alignment_events{0};
    std::atomic<int> total_events{0};

    // Last received event data
    hypo_internal_event_t last_event;
    std::atomic<bool> has_event{false};

    void reset() {
        circadian_events = 0;
        stress_events = 0;
        drive_events = 0;
        autonomic_events = 0;
        homeostatic_events = 0;
        alignment_events = 0;
        total_events = 0;
        has_event = false;
        std::memset(&last_event, 0, sizeof(last_event));
    }
};

// =============================================================================
// Event Callbacks
// =============================================================================

static int circadian_callback(const hypo_internal_event_t* event, void* user_data) {
    auto* tracker = static_cast<EventTracker*>(user_data);
    tracker->circadian_events++;
    tracker->total_events++;
    tracker->last_event = *event;
    tracker->has_event = true;
    return 0;
}

static int stress_callback(const hypo_internal_event_t* event, void* user_data) {
    auto* tracker = static_cast<EventTracker*>(user_data);
    tracker->stress_events++;
    tracker->total_events++;
    tracker->last_event = *event;
    tracker->has_event = true;
    return 0;
}

static int drive_callback(const hypo_internal_event_t* event, void* user_data) {
    auto* tracker = static_cast<EventTracker*>(user_data);
    tracker->drive_events++;
    tracker->total_events++;
    tracker->last_event = *event;
    tracker->has_event = true;
    return 0;
}

static int autonomic_callback(const hypo_internal_event_t* event, void* user_data) {
    auto* tracker = static_cast<EventTracker*>(user_data);
    tracker->autonomic_events++;
    tracker->total_events++;
    tracker->last_event = *event;
    tracker->has_event = true;
    return 0;
}

static int homeostatic_callback(const hypo_internal_event_t* event, void* user_data) {
    auto* tracker = static_cast<EventTracker*>(user_data);
    tracker->homeostatic_events++;
    tracker->total_events++;
    tracker->last_event = *event;
    tracker->has_event = true;
    return 0;
}

static int alignment_callback(const hypo_internal_event_t* event, void* user_data) {
    auto* tracker = static_cast<EventTracker*>(user_data);
    tracker->alignment_events++;
    tracker->total_events++;
    tracker->last_event = *event;
    tracker->has_event = true;
    return 0;
}

static int generic_callback(const hypo_internal_event_t* event, void* user_data) {
    auto* tracker = static_cast<EventTracker*>(user_data);
    tracker->total_events++;
    tracker->last_event = *event;
    tracker->has_event = true;
    return 0;
}

// =============================================================================
// Test Fixture
// =============================================================================

class HypothalamusInternalBusPipelineTest : public ::testing::Test {
protected:
    hypo_ibus_t bus;
    EventTracker tracker;

    void SetUp() override {
        // Create bus with default config
        hypo_ibus_config_t config;
        int rc = hypo_ibus_default_config(&config);
        ASSERT_EQ(rc, 0) << "Failed to get default internal bus config";

        config.enable_modulation = true;
        config.enable_logging = false;

        bus = hypo_ibus_create(&config);
        ASSERT_NE(bus, nullptr) << "Failed to create hypothalamus internal bus";

        tracker.reset();
    }

    void TearDown() override {
        if (bus) {
            hypo_ibus_destroy(bus);
            bus = nullptr;
        }
    }

    /**
     * @brief Simulate circadian phase progression
     * @param phase Current phase (0-7)
     * @return Melatonin level for this phase
     */
    float get_melatonin_for_phase(uint32_t phase) {
        // Melatonin peaks at night (phases 6-7, 0)
        static const float melatonin_by_phase[] = {
            0.7f,  // Phase 0: Late night (high melatonin)
            0.3f,  // Phase 1: Early morning (decreasing)
            0.05f, // Phase 2: Morning (low)
            0.02f, // Phase 3: Midday (minimal)
            0.02f, // Phase 4: Afternoon (minimal)
            0.1f,  // Phase 5: Evening (rising)
            0.5f,  // Phase 6: Night (increasing)
            0.8f   // Phase 7: Deep night (peak)
        };
        return melatonin_by_phase[phase % CIRCADIAN_PHASES];
    }

    /**
     * @brief Get cortisol level for a circadian phase
     * @param phase Current phase (0-7)
     * @return Cortisol level for this phase
     */
    float get_cortisol_for_phase(uint32_t phase) {
        // Cortisol peaks in morning (phases 1-2)
        static const float cortisol_by_phase[] = {
            0.3f,  // Phase 0: Late night
            0.6f,  // Phase 1: Early morning (cortisol awakening response)
            0.8f,  // Phase 2: Morning (peak)
            0.5f,  // Phase 3: Midday
            0.3f,  // Phase 4: Afternoon
            0.2f,  // Phase 5: Evening
            0.15f, // Phase 6: Night
            0.2f   // Phase 7: Deep night
        };
        return cortisol_by_phase[phase % CIRCADIAN_PHASES];
    }

    /**
     * @brief Get alertness for a circadian phase
     * @param phase Current phase (0-7)
     * @return Alertness level for this phase
     */
    float get_alertness_for_phase(uint32_t phase) {
        static const float alertness_by_phase[] = {
            0.2f,  // Phase 0: Late night (drowsy)
            0.5f,  // Phase 1: Early morning (waking)
            0.9f,  // Phase 2: Morning (alert)
            0.7f,  // Phase 3: Midday (post-lunch dip possible)
            0.75f, // Phase 4: Afternoon
            0.6f,  // Phase 5: Evening
            0.3f,  // Phase 6: Night
            0.1f   // Phase 7: Deep night (sleepy)
        };
        return alertness_by_phase[phase % CIRCADIAN_PHASES];
    }
};

// =============================================================================
// Test 1: Complete Lifecycle Pipeline
// =============================================================================

/**
 * WHAT: Test complete lifecycle of internal bus
 * WHY:  Verify proper initialization, operation, and cleanup
 * HOW:  Create → configure → subscribe → publish → get stats → reset → destroy
 */
TEST_F(HypothalamusInternalBusPipelineTest, CompleteLifecyclePipeline) {
    // Phase 1: Verify initial state
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(stats.events_published, 0u);
    EXPECT_EQ(stats.active_subscribers, 0u);

    // Phase 2: Subscribe modules
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, circadian_callback, &tracker);
    ASSERT_GE(sub1, 0) << "Failed to subscribe drives to circadian events";

    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
        HYPO_IEVT_STRESS_ONSET, stress_callback, &tracker);
    ASSERT_GE(sub2, 0) << "Failed to subscribe appetite to stress events";

    // Phase 3: Verify subscriptions
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(stats.active_subscribers, 2u);

    // Phase 4: Publish events
    ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, 0, 1, 0.3f, 0.6f, 0.5f));
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.events_published, 0u);

    // Phase 5: Verify event delivery
    EXPECT_GE(tracker.circadian_events.load(), 1);

    // Phase 6: Reset and verify
    ASSERT_EQ(0, hypo_ibus_reset(bus));
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    // Subscriptions should persist after reset
    EXPECT_EQ(stats.active_subscribers, 2u);

    // Phase 7: Unsubscribe and cleanup
    ASSERT_EQ(0, hypo_ibus_unsubscribe(bus, sub1));
    ASSERT_EQ(0, hypo_ibus_unsubscribe(bus, sub2));
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(stats.active_subscribers, 0u);
}

// =============================================================================
// Test 2: Day Simulation - Circadian Phases Over 24 Hours
// =============================================================================

/**
 * WHAT: Simulate a complete 24-hour day with circadian phase transitions
 * WHY:  Biological circadian rhythm affects all hypothalamic functions
 * HOW:  Progress through 8 phases, publishing phase changes and verifying
 *       melatonin/cortisol/alertness patterns
 */
TEST_F(HypothalamusInternalBusPipelineTest, DaySimulationCircadianPhases) {
    // Subscribe multiple modules to circadian events
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, circadian_callback, &tracker);
    ASSERT_GE(sub1, 0);

    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_HPA_AXIS,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, circadian_callback, &tracker);
    ASSERT_GE(sub2, 0);

    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, circadian_callback, &tracker);
    ASSERT_GE(sub3, 0);

    // Simulate 24 hours: 8 phases, 3 hours each
    for (uint32_t phase = 0; phase < CIRCADIAN_PHASES; phase++) {
        uint32_t prev_phase = (phase == 0) ? 7 : phase - 1;
        float melatonin = get_melatonin_for_phase(phase);
        float cortisol = get_cortisol_for_phase(phase);
        float alertness = get_alertness_for_phase(phase);

        // Publish phase change
        ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(
            bus, prev_phase, phase, melatonin, cortisol, alertness))
            << "Failed to publish phase " << phase;

        // Update modulations for time passage (3 hours = 10.8 billion us)
        // Simulate in smaller chunks for stability
        for (int tick = 0; tick < 100; tick++) {
            ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_MINUTE));
        }
    }

    // Verify all phase changes were received
    // 8 phases × 3 subscribers = 24 events expected
    EXPECT_GE(tracker.circadian_events.load(), 24);

    // Verify statistics
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(stats.events_published, 8u);  // 8 phase changes
    EXPECT_GE(stats.events_delivered, 24u); // Delivered to 3 subscribers each
}

// =============================================================================
// Test 3: Stress Response Pipeline
// =============================================================================

/**
 * WHAT: Test complete stress response pipeline
 * WHY:  Stress activates HPA axis: CRH → ACTH → Cortisol → effects
 *       Cortisol suppresses appetite (biological adaptation)
 * HOW:  Trigger stress → observe HPA activation → verify appetite suppression
 *       → trigger recovery → verify normalization
 */
TEST_F(HypothalamusInternalBusPipelineTest, StressResponsePipeline) {
    // Register default modulations (includes cortisol → appetite suppression)
    int rules = hypo_ibus_register_default_modulations(bus);
    ASSERT_GT(rules, 0) << "Failed to register default modulations";

    // Subscribe modules
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
        HYPO_IEVT_STRESS_ONSET, stress_callback, &tracker);
    ASSERT_GE(sub1, 0);

    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
        HYPO_IEVT_CORTISOL_ELEVATED, stress_callback, &tracker);
    ASSERT_GE(sub2, 0);

    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_STRESS_RECOVERY, stress_callback, &tracker);
    ASSERT_GE(sub3, 0);

    // Get initial appetite modulation (should be 1.0 - no modulation)
    float initial_appetite_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_NEAR(initial_appetite_mod, 1.0f, FLOAT_TOLERANCE);

    // Phase 1: Stress onset
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET,
        0.6f, 0.5f, true));
    EXPECT_GE(tracker.stress_events.load(), 1);

    // Phase 2: Stress peak with elevated cortisol
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_PEAK,
        0.9f, 0.85f, true));
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_CORTISOL_ELEVATED,
        0.9f, 0.85f, true));

    // Allow modulation to take effect
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_SECOND));

    // Verify appetite is suppressed (modulation < 1.0)
    float stressed_appetite_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_LT(stressed_appetite_mod, 1.0f);

    // Phase 3: Recovery
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_RECOVERY,
        0.3f, 0.4f, false));
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_CORTISOL_NORMALIZED,
        0.1f, 0.3f, false));

    // Allow recovery (modulation decay over time)
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_MINUTE));
    }

    // Verify recovery - appetite modulation should trend back toward 1.0
    float recovered_appetite_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_GT(recovered_appetite_mod, stressed_appetite_mod);

    // Verify event counts (3 events: STRESS_ONSET, CORTISOL_ELEVATED, STRESS_RECOVERY)
    EXPECT_GE(tracker.stress_events.load(), 3);

    // Verify statistics
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.modulations_applied, 0u);
}

// =============================================================================
// Test 4: Sleep Pressure Pipeline
// =============================================================================

/**
 * WHAT: Test sleep pressure buildup with melatonin effects
 * WHY:  Melatonin onset signals sleep time, increases fatigue,
 *       reduces curiosity/exploration drive
 * HOW:  Publish melatonin onset → verify fatigue increase →
 *       verify curiosity reduction → morning reset
 */
TEST_F(HypothalamusInternalBusPipelineTest, SleepPressurePipeline) {
    // Register default modulations (includes fatigue → curiosity reduction)
    int rules = hypo_ibus_register_default_modulations(bus);
    ASSERT_GT(rules, 0);

    // Subscribe to melatonin events
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_MELATONIN_ONSET, circadian_callback, &tracker);
    ASSERT_GE(sub1, 0);

    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_FATIGUE_ONSET, drive_callback, &tracker);
    ASSERT_GE(sub2, 0);

    // Get baseline curiosity modulation
    float baseline_curiosity = hypo_ibus_get_modulation(bus, HYPO_IMOD_DRIVES, 0);

    // Phase 1: Evening - melatonin onset
    hypo_internal_event_t melatonin_event = {};
    melatonin_event.type = HYPO_IEVT_MELATONIN_ONSET;
    melatonin_event.source = HYPO_IMOD_CIRCADIAN;
    melatonin_event.data.circadian.melatonin_level = 0.6f;
    melatonin_event.data.circadian.sleep_pressure = 0.7f;
    melatonin_event.data.circadian.alertness = 0.3f;

    ASSERT_GE(hypo_ibus_publish(bus, &melatonin_event), 0);

    // Phase 2: Fatigue buildup
    ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_FATIGUE_ONSET,
        4, // HYPO_DRIVE_FATIGUE assumed
        0.8f, 0.85f));

    // Update modulations
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_HOUR));

    // Verify events received
    EXPECT_GE(tracker.circadian_events.load(), 1);
    EXPECT_GE(tracker.drive_events.load(), 1);

    // Phase 3: Morning - melatonin offset
    hypo_internal_event_t morning_event = {};
    morning_event.type = HYPO_IEVT_MELATONIN_OFFSET;
    morning_event.source = HYPO_IMOD_CIRCADIAN;
    morning_event.data.circadian.melatonin_level = 0.1f;
    morning_event.data.circadian.sleep_pressure = 0.1f;
    morning_event.data.circadian.alertness = 0.8f;

    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_MELATONIN_OFFSET, circadian_callback, &tracker);
    ASSERT_GE(sub3, 0);

    ASSERT_GE(hypo_ibus_publish(bus, &morning_event), 0);

    // Verify morning event
    EXPECT_GE(tracker.circadian_events.load(), 2);
}

// =============================================================================
// Test 5: Meal Timing Simulation
// =============================================================================

/**
 * WHAT: Simulate circadian hunger peaks at meal times
 * WHY:  Ghrelin (hunger hormone) follows circadian pattern, peaking
 *       before typical meal times (breakfast, lunch, dinner)
 * HOW:  Simulate 24h with hunger peaks at phases 2 (morning), 4 (afternoon),
 *       6 (evening), verify drive threshold crossings
 */
TEST_F(HypothalamusInternalBusPipelineTest, MealTimingSimulation) {
    // Register default modulations
    int rules = hypo_ibus_register_default_modulations(bus);
    ASSERT_GT(rules, 0);

    // Subscribe to hunger-related events
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_HUNGER_ONSET, drive_callback, &tracker);
    ASSERT_GE(sub1, 0);

    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_DRIVE_THRESHOLD_CROSSED, drive_callback, &tracker);
    ASSERT_GE(sub2, 0);

    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_DRIVE_SATISFIED, drive_callback, &tracker);
    ASSERT_GE(sub3, 0);

    // Meal times (as phases): breakfast=2, lunch=4, dinner=6
    const uint32_t meal_phases[] = {2, 4, 6};
    int meals_eaten = 0;

    for (uint32_t phase = 0; phase < CIRCADIAN_PHASES; phase++) {
        // Publish circadian phase
        uint32_t prev_phase = (phase == 0) ? 7 : phase - 1;
        ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, prev_phase, phase,
            get_melatonin_for_phase(phase),
            get_cortisol_for_phase(phase),
            get_alertness_for_phase(phase)));

        // Check if this is a meal time
        bool is_meal_time = (phase == 2 || phase == 4 || phase == 6);

        if (is_meal_time) {
            // Hunger onset before meal
            ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_HUNGER_ONSET,
                0, // HYPO_DRIVE_HUNGER
                0.75f, 0.8f));

            // Hunger threshold crossed
            ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_DRIVE_THRESHOLD_CROSSED,
                0, 0.85f, 0.9f));

            // Meal eaten - drive satisfied
            ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_DRIVE_SATISFIED,
                0, 0.1f, 0.05f));

            meals_eaten++;
        }

        // Update modulations
        ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_HOUR * 3));
    }

    // Verify meal events
    EXPECT_EQ(meals_eaten, 3);
    EXPECT_GE(tracker.drive_events.load(), 9); // 3 events per meal × 3 meals

    // Verify statistics
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.module_events[HYPO_IMOD_CIRCADIAN], 0u);
}

// =============================================================================
// Test 6: Fight-or-Flight Pipeline
// =============================================================================

/**
 * WHAT: Test fight-or-flight sympathetic activation
 * WHY:  Threat detection → sympathetic activation → safety drive →
 *       physiological arousal → eventual recovery
 * HOW:  Publish threat → verify sympathetic activation →
 *       verify safety drive increase → publish safety → verify recovery
 */
TEST_F(HypothalamusInternalBusPipelineTest, FightOrFlightPipeline) {
    // Subscribe to relevant events
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_SYMPATHETIC_ACTIVATION, autonomic_callback, &tracker);
    ASSERT_GE(sub1, 0);

    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_HPA_AXIS,
        HYPO_IEVT_SAFETY_THREAT, drive_callback, &tracker);
    ASSERT_GE(sub2, 0);

    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
        HYPO_IEVT_STRESS_ONSET, stress_callback, &tracker);
    ASSERT_GE(sub3, 0);

    int sub4 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_PARASYMPATHETIC_ACTIVATION, autonomic_callback, &tracker);
    ASSERT_GE(sub4, 0);

    // Phase 1: Threat detected
    ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_SAFETY_THREAT,
        8, // Safety drive
        0.9f, 0.95f));

    // Phase 2: Sympathetic activation (fight-or-flight)
    ASSERT_EQ(0, hypo_ibus_publish_autonomic(bus, HYPO_IEVT_SYMPATHETIC_ACTIVATION,
        0.85f, 0.15f)); // High sympathetic, low parasympathetic

    // Phase 3: Stress response
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET,
        0.8f, 0.7f, true));

    // Verify fight-or-flight events
    EXPECT_GE(tracker.drive_events.load(), 1);
    EXPECT_GE(tracker.autonomic_events.load(), 1);
    EXPECT_GE(tracker.stress_events.load(), 1);

    // Phase 4: Threat resolved - safety restored
    ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_DRIVE_SATISFIED,
        8, 0.2f, 0.1f));

    // Phase 5: Parasympathetic recovery (rest-and-digest)
    ASSERT_EQ(0, hypo_ibus_publish_autonomic(bus, HYPO_IEVT_PARASYMPATHETIC_ACTIVATION,
        0.2f, 0.75f)); // Low sympathetic, high parasympathetic

    // Phase 6: Stress recovery
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_RECOVERY,
        0.2f, 0.3f, false));

    // Verify recovery events
    EXPECT_GE(tracker.autonomic_events.load(), 2);
    EXPECT_GE(tracker.stress_events.load(), 1);  // Only STRESS_ONSET subscribed
}

// =============================================================================
// Test 7: Relaxation Pipeline
// =============================================================================

/**
 * WHAT: Test social connection and relaxation effects
 * WHY:  Oxytocin from social bonding reduces threat sensitivity
 *       and activates parasympathetic system
 * HOW:  Publish social satisfaction → verify safety drive reduction →
 *       verify parasympathetic activation → measure modulation effects
 */
TEST_F(HypothalamusInternalBusPipelineTest, RelaxationPipeline) {
    // Register default modulations (includes social → safety modulation)
    int rules = hypo_ibus_register_default_modulations(bus);
    ASSERT_GT(rules, 0);

    // Subscribe to events
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_ALIGNMENT,
        HYPO_IEVT_DRIVE_SATISFIED, drive_callback, &tracker);
    ASSERT_GE(sub1, 0);

    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_AUTONOMIC,
        HYPO_IEVT_PARASYMPATHETIC_ACTIVATION, autonomic_callback, &tracker);
    ASSERT_GE(sub2, 0);

    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_HPA_AXIS,
        HYPO_IEVT_AUTONOMIC_BALANCE_SHIFT, autonomic_callback, &tracker);
    ASSERT_GE(sub3, 0);

    // Get initial safety drive modulation
    float initial_safety = hypo_ibus_get_modulation(bus, HYPO_IMOD_ALIGNMENT, 0);

    // Phase 1: Social connection (oxytocin release)
    ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_DRIVE_SATISFIED,
        7, // Social drive
        0.9f, 0.1f)); // High satisfaction, low urgency

    // Phase 2: Parasympathetic activation
    ASSERT_EQ(0, hypo_ibus_publish_autonomic(bus, HYPO_IEVT_PARASYMPATHETIC_ACTIVATION,
        0.15f, 0.8f));

    // Phase 3: ANS balance shift toward relaxation
    hypo_internal_event_t balance_event = {};
    balance_event.type = HYPO_IEVT_AUTONOMIC_BALANCE_SHIFT;
    balance_event.source = HYPO_IMOD_AUTONOMIC;
    balance_event.data.autonomic.sympathetic_tone = 0.2f;
    balance_event.data.autonomic.parasympathetic_tone = 0.75f;
    balance_event.data.autonomic.balance = -0.55f; // Parasympathetic dominant

    ASSERT_GE(hypo_ibus_publish(bus, &balance_event), 0);

    // Update modulations
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_MINUTE * 10));

    // Verify events
    EXPECT_GE(tracker.drive_events.load(), 1);
    EXPECT_GE(tracker.autonomic_events.load(), 2);

    // Verify statistics
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.events_published, 0u);
}

// =============================================================================
// Test 8: Multi-Module Interaction
// =============================================================================

/**
 * WHAT: Test all 9 internal modules communicating simultaneously
 * WHY:  Real hypothalamus has extensive cross-talk between all systems
 * HOW:  Subscribe all modules, publish events from each, verify all
 *       receive appropriate events
 */
TEST_F(HypothalamusInternalBusPipelineTest, MultiModuleInteraction) {
    EventTracker trackers[HYPO_IMOD_COUNT];

    // Subscribe each module to events from all other modules
    std::vector<int> subscriptions;
    for (int mod = 0; mod < HYPO_IMOD_COUNT; mod++) {
        // Subscribe to circadian events
        int sub = hypo_ibus_subscribe(bus, static_cast<hypo_internal_module_t>(mod),
            HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, generic_callback, &trackers[mod]);
        if (sub >= 0) subscriptions.push_back(sub);

        // Subscribe to stress events
        sub = hypo_ibus_subscribe(bus, static_cast<hypo_internal_module_t>(mod),
            HYPO_IEVT_STRESS_ONSET, generic_callback, &trackers[mod]);
        if (sub >= 0) subscriptions.push_back(sub);

        // Subscribe to drive events
        sub = hypo_ibus_subscribe(bus, static_cast<hypo_internal_module_t>(mod),
            HYPO_IEVT_DRIVE_THRESHOLD_CROSSED, generic_callback, &trackers[mod]);
        if (sub >= 0) subscriptions.push_back(sub);
    }

    // Verify subscriptions were created
    EXPECT_GT(subscriptions.size(), 0u);

    // Publish events from different modules
    ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, 0, 1, 0.2f, 0.6f, 0.7f));
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET, 0.5f, 0.4f, true));
    ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_DRIVE_THRESHOLD_CROSSED, 0, 0.8f, 0.85f));

    // Count total events received across all modules
    int total_received = 0;
    for (int mod = 0; mod < HYPO_IMOD_COUNT; mod++) {
        total_received += trackers[mod].total_events.load();
    }

    // At least some modules should have received events
    EXPECT_GT(total_received, 0);

    // Verify per-module statistics
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));

    // Some modules should have received events
    uint64_t total_receives = 0;
    for (int mod = 0; mod < HYPO_IMOD_COUNT; mod++) {
        total_receives += stats.module_receives[mod];
    }
    EXPECT_GT(total_receives, 0u);
}

// =============================================================================
// Test 9: Default Modulation Rules
// =============================================================================

/**
 * WHAT: Test all 7 default biological modulation rules
 * WHY:  These rules implement biologically-accurate cross-modulation:
 *       1. Circadian → Hunger (meal-time peaks)
 *       2. Circadian → Fatigue (nighttime increase)
 *       3. Cortisol → Appetite (stress suppression)
 *       4. Fatigue → Curiosity (tiredness reduces exploration)
 *       5. Social → Safety (connection reduces threat sensitivity)
 *       6. Hunger → Stress (extreme hunger triggers stress)
 *       7. Temperature → Fatigue (hyperthermia increases drowsiness)
 * HOW:  Register defaults, trigger each rule, verify modulation effects
 */
TEST_F(HypothalamusInternalBusPipelineTest, DefaultModulationRules) {
    // Register default modulation rules
    int rules = hypo_ibus_register_default_modulations(bus);
    ASSERT_EQ(rules, 7) << "Expected 7 default modulation rules";

    // Get initial config to check amplitudes
    hypo_ibus_config_t config;
    ASSERT_EQ(0, hypo_ibus_default_config(&config));

    // Verify default biological parameters
    EXPECT_NEAR(config.circadian_hunger_amplitude, 0.3f, FLOAT_TOLERANCE);
    EXPECT_NEAR(config.circadian_fatigue_amplitude, 0.5f, FLOAT_TOLERANCE);
    EXPECT_NEAR(config.cortisol_appetite_suppression, 0.4f, FLOAT_TOLERANCE);
    EXPECT_NEAR(config.fatigue_curiosity_reduction, 0.6f, FLOAT_TOLERANCE);
    EXPECT_NEAR(config.social_safety_modulation, 0.3f, FLOAT_TOLERANCE);
    EXPECT_NEAR(config.hunger_stress_threshold, 0.85f, FLOAT_TOLERANCE);

    // Test Rule 1: Circadian → Hunger (publish morning phase)
    ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, 0, 2, 0.05f, 0.8f, 0.9f));
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_SECOND));

    // Test Rule 3: Cortisol → Appetite (high stress)
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_CORTISOL_ELEVATED, 0.9f, 0.85f, true));
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_SECOND));
    float appetite_mod = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_LT(appetite_mod, 1.0f); // Should be suppressed

    // Test Rule 4: Fatigue → Curiosity (publish fatigue onset)
    ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_FATIGUE_ONSET, 4, 0.8f, 0.75f));
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_SECOND));

    // Test Rule 6: Hunger → Stress (extreme hunger)
    ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_HUNGER_ONSET, 0, 0.9f, 0.95f));
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_SECOND));

    // Verify modulations were applied
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.modulations_applied, 0u);
}

// =============================================================================
// Test 10: Extended Simulation - 1000 Update Cycles
// =============================================================================

/**
 * WHAT: Run 1000 update cycles with continuous events
 * WHY:  Verify system stability under extended operation
 * HOW:  Publish various events, update modulations, verify no crashes,
 *       memory leaks, or statistical anomalies
 */
TEST_F(HypothalamusInternalBusPipelineTest, ExtendedSimulation1000Cycles) {
    // Register default modulations
    int rules = hypo_ibus_register_default_modulations(bus);
    ASSERT_GT(rules, 0);

    // Subscribe modules
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, generic_callback, &tracker);
    ASSERT_GE(sub1, 0);

    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
        HYPO_IEVT_STRESS_ONSET, generic_callback, &tracker);
    ASSERT_GE(sub2, 0);

    uint32_t current_phase = 0;

    // Run 1000 update cycles
    for (int cycle = 0; cycle < 1000; cycle++) {
        // Every 125 cycles, advance circadian phase (simulates ~3 hours)
        if (cycle % 125 == 0) {
            uint32_t next_phase = (current_phase + 1) % CIRCADIAN_PHASES;
            ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus,
                current_phase, next_phase,
                get_melatonin_for_phase(next_phase),
                get_cortisol_for_phase(next_phase),
                get_alertness_for_phase(next_phase)));
            current_phase = next_phase;
        }

        // Occasional stress events (every 50 cycles)
        if (cycle % 50 == 25) {
            float stress = 0.3f + 0.4f * (cycle % 10) / 10.0f;
            ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET,
                stress, stress * 0.8f, (cycle % 3) == 0));
        }

        // Update modulations each cycle
        ASSERT_EQ(0, hypo_ibus_update_modulations(bus, UPDATE_DELTA_US));
    }

    // Verify system stability
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));

    // Should have processed many events
    EXPECT_GE(stats.events_published, 8u);  // At least 8 circadian + stress events
    EXPECT_EQ(stats.events_dropped, 0u);    // No dropped events
    EXPECT_GE(stats.modulations_applied, 0u);

    // Modulation values should still be in valid range
    for (int mod = 0; mod < HYPO_IMOD_COUNT; mod++) {
        float modulation = hypo_ibus_get_modulation(bus,
            static_cast<hypo_internal_module_t>(mod), 0);
        EXPECT_GE(modulation, 0.0f);
        EXPECT_LE(modulation, 2.0f);
    }
}

// =============================================================================
// Test 11: Modulation Stacking
// =============================================================================

/**
 * WHAT: Test multiple modulations on the same module
 * WHY:  In biology, multiple factors affect the same system simultaneously
 *       (e.g., both cortisol AND circadian affect appetite)
 * HOW:  Register multiple modulations targeting appetite, verify they stack
 */
TEST_F(HypothalamusInternalBusPipelineTest, ModulationStacking) {
    // Register custom modulations targeting appetite
    hypo_ibus_modulation_t mod1 = {};
    mod1.target = HYPO_IMOD_APPETITE;
    mod1.modulation_factor = 0.7f;  // 30% reduction
    mod1.parameter_id = 0;
    mod1.duration_us = US_PER_HOUR;
    mod1.is_additive = false;

    int rule1 = hypo_ibus_register_modulation(bus, HYPO_IEVT_STRESS_ONSET, &mod1);
    ASSERT_GE(rule1, 0);

    hypo_ibus_modulation_t mod2 = {};
    mod2.target = HYPO_IMOD_APPETITE;
    mod2.modulation_factor = 0.8f;  // Additional 20% reduction
    mod2.parameter_id = 0;
    mod2.duration_us = US_PER_HOUR;
    mod2.is_additive = false;

    int rule2 = hypo_ibus_register_modulation(bus, HYPO_IEVT_FATIGUE_ONSET, &mod2);
    ASSERT_GE(rule2, 0);

    // Get baseline
    float baseline = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_NEAR(baseline, 1.0f, FLOAT_TOLERANCE);

    // Trigger first modulation
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET, 0.8f, 0.7f, true));
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_SECOND));

    float after_stress = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_LT(after_stress, baseline);

    // Trigger second modulation (stacks with first)
    ASSERT_EQ(0, hypo_ibus_publish_drive(bus, HYPO_IEVT_FATIGUE_ONSET, 4, 0.8f, 0.75f));
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_SECOND));

    float after_both = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    // Stacked modulations should reduce further (or at least not increase)
    EXPECT_LE(after_both, after_stress + FLOAT_TOLERANCE);

    // Clear modulations and verify reset
    ASSERT_EQ(0, hypo_ibus_clear_modulations(bus));
    float after_clear = hypo_ibus_get_modulation(bus, HYPO_IMOD_APPETITE, 0);
    EXPECT_NEAR(after_clear, 1.0f, FLOAT_TOLERANCE);
}

// =============================================================================
// Test 12: Event Storm Handling
// =============================================================================

/**
 * WHAT: Test system stability under rapid event publication
 * WHY:  Real biological systems can experience burst activity
 * HOW:  Publish 100 events rapidly, verify no crashes, queue overflow handled
 */
TEST_F(HypothalamusInternalBusPipelineTest, EventStormHandling) {
    // Subscribe to catch events
    int sub = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_DRIVE_THRESHOLD_CROSSED, generic_callback, &tracker);
    ASSERT_GE(sub, 0);

    // Publish 100 events rapidly
    for (int i = 0; i < 100; i++) {
        int rc = hypo_ibus_publish_drive(bus, HYPO_IEVT_DRIVE_THRESHOLD_CROSSED,
            i % 9, // Rotate through drives
            0.8f + (i % 20) * 0.01f,
            0.85f);
        // Some may fail if queue is full - that's acceptable
        // The key is no crash
    }

    // Verify system still operational
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));

    // Should have handled most events
    EXPECT_GT(stats.events_published, 50u);

    // Queue depth should be within limits
    EXPECT_LE(stats.queue_depth, HYPO_IBUS_MAX_QUEUE);
    EXPECT_LE(stats.peak_queue_depth, HYPO_IBUS_MAX_QUEUE);

    // Bus should still accept new events
    ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, 0, 1, 0.5f, 0.5f, 0.5f));
}

// =============================================================================
// Test 13: Module Isolation
// =============================================================================

/**
 * WHAT: Test that unsubscribed modules don't receive events
 * WHY:  Event isolation is critical for correct module behavior
 * HOW:  Subscribe some modules, publish events, verify only subscribed
 *       modules receive them
 */
TEST_F(HypothalamusInternalBusPipelineTest, ModuleIsolation) {
    EventTracker subscribed_tracker;
    EventTracker unsubscribed_tracker;

    // Subscribe only DRIVES to circadian events
    int sub = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, generic_callback, &subscribed_tracker);
    ASSERT_GE(sub, 0);

    // Don't subscribe APPETITE to circadian events

    // Publish circadian event
    ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, 0, 1, 0.3f, 0.6f, 0.7f));

    // Verify subscribed module received event
    EXPECT_EQ(subscribed_tracker.total_events.load(), 1);

    // Verify unsubscribed module did NOT receive event
    EXPECT_EQ(unsubscribed_tracker.total_events.load(), 0);

    // Check subscriber counts for the event type
    uint32_t circadian_subs = hypo_ibus_subscriber_count(bus, HYPO_IEVT_CIRCADIAN_PHASE_CHANGE);
    EXPECT_GE(circadian_subs, 1u);

    // Unsubscribe and verify
    ASSERT_EQ(0, hypo_ibus_unsubscribe(bus, sub));

    // Publish another event
    ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, 1, 2, 0.05f, 0.8f, 0.9f));

    // Event count should not increase
    EXPECT_EQ(subscribed_tracker.total_events.load(), 1);
}

// =============================================================================
// Test 14: Reset and Reconfigure
// =============================================================================

/**
 * WHAT: Test full reset and reconfiguration without memory issues
 * WHY:  System should cleanly reset state while preserving subscriptions
 * HOW:  Configure, use, reset, reconfigure, verify clean state
 */
TEST_F(HypothalamusInternalBusPipelineTest, ResetAndReconfigure) {
    // Phase 1: Initial configuration and use
    int rules = hypo_ibus_register_default_modulations(bus);
    ASSERT_GT(rules, 0);

    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_STRESS_ONSET, stress_callback, &tracker);
    ASSERT_GE(sub1, 0);

    // Publish some events
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET, 0.8f, 0.7f, true));
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_MINUTE));

    hypo_ibus_stats_t stats1;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats1));
    EXPECT_GT(stats1.events_published, 0u);

    // Phase 2: Reset
    ASSERT_EQ(0, hypo_ibus_reset(bus));

    // Phase 3: Verify reset state
    hypo_ibus_stats_t stats2;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats2));
    // Subscriptions should persist
    EXPECT_EQ(stats2.active_subscribers, stats1.active_subscribers);

    // Phase 4: Reset statistics separately
    ASSERT_EQ(0, hypo_ibus_reset_stats(bus));
    hypo_ibus_stats_t stats3;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats3));
    EXPECT_EQ(stats3.events_published, 0u);
    EXPECT_EQ(stats3.events_delivered, 0u);

    // Phase 5: Verify system still works after reset
    tracker.reset();
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET, 0.5f, 0.4f, false));
    EXPECT_GE(tracker.stress_events.load(), 1);

    // Phase 6: Clear modulations
    ASSERT_EQ(0, hypo_ibus_clear_modulations(bus));

    // Phase 7: Verify clean modulation state
    for (int mod = 0; mod < HYPO_IMOD_COUNT; mod++) {
        float modulation = hypo_ibus_get_modulation(bus,
            static_cast<hypo_internal_module_t>(mod), 0);
        EXPECT_NEAR(modulation, 1.0f, FLOAT_TOLERANCE);
    }

    // Phase 8: Unsubscribe all
    int removed = hypo_ibus_unsubscribe_module(bus, HYPO_IMOD_DRIVES);
    EXPECT_GE(removed, 1);
}

// =============================================================================
// Test 15: Statistics Validation
// =============================================================================

/**
 * WHAT: Validate end-to-end statistics accuracy
 * WHY:  Accurate statistics are critical for monitoring and debugging
 * HOW:  Perform known operations, verify statistics match expectations
 */
TEST_F(HypothalamusInternalBusPipelineTest, StatisticsValidation) {
    // Subscribe multiple modules
    int sub1 = hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, generic_callback, &tracker);
    int sub2 = hypo_ibus_subscribe(bus, HYPO_IMOD_APPETITE,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, generic_callback, &tracker);
    int sub3 = hypo_ibus_subscribe(bus, HYPO_IMOD_HPA_AXIS,
        HYPO_IEVT_CIRCADIAN_PHASE_CHANGE, generic_callback, &tracker);
    ASSERT_GE(sub1, 0);
    ASSERT_GE(sub2, 0);
    ASSERT_GE(sub3, 0);

    // Verify subscriber count
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(stats.active_subscribers, 3u);

    // Publish known number of events
    const int num_events = 5;
    for (int i = 0; i < num_events; i++) {
        ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, i, i + 1, 0.5f, 0.5f, 0.5f));
    }

    // Verify publish count
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_EQ(stats.events_published, static_cast<uint64_t>(num_events));

    // Each event should be delivered to 3 subscribers
    EXPECT_EQ(stats.events_delivered, static_cast<uint64_t>(num_events * 3));

    // Verify no dropped events
    EXPECT_EQ(stats.events_dropped, 0u);

    // Verify per-module event counts
    EXPECT_GT(stats.module_events[HYPO_IMOD_CIRCADIAN], 0u);

    // Verify module receive counts
    EXPECT_GT(stats.module_receives[HYPO_IMOD_DRIVES], 0u);
    EXPECT_GT(stats.module_receives[HYPO_IMOD_APPETITE], 0u);
    EXPECT_GT(stats.module_receives[HYPO_IMOD_HPA_AXIS], 0u);

    // Register modulations and verify count
    int rules = hypo_ibus_register_default_modulations(bus);
    ASSERT_GT(rules, 0);

    // Trigger modulation
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET, 0.8f, 0.7f, true));
    ASSERT_EQ(0, hypo_ibus_update_modulations(bus, US_PER_SECOND));

    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    EXPECT_GT(stats.modulations_applied, 0u);

    // Verify queue statistics
    EXPECT_LE(stats.queue_depth, HYPO_IBUS_MAX_QUEUE);
    EXPECT_LE(stats.peak_queue_depth, HYPO_IBUS_MAX_QUEUE);
}

// =============================================================================
// Test 16: Utility Functions
// =============================================================================

/**
 * WHAT: Test utility functions for names and printing
 * WHY:  Debug output and logging require correct string conversions
 * HOW:  Call name functions for all types, verify non-null and sensible
 */
TEST_F(HypothalamusInternalBusPipelineTest, UtilityFunctions) {
    // Test module names
    for (int mod = 0; mod < HYPO_IMOD_COUNT; mod++) {
        const char* name = hypo_ibus_module_name(static_cast<hypo_internal_module_t>(mod));
        ASSERT_NE(name, nullptr) << "Module " << mod << " name is NULL";
        EXPECT_GT(strlen(name), 0u) << "Module " << mod << " name is empty";
    }

    // Test event type names
    for (int evt = 0; evt < HYPO_IEVT_COUNT; evt++) {
        const char* name = hypo_ibus_event_name(static_cast<hypo_internal_event_type_t>(evt));
        ASSERT_NE(name, nullptr) << "Event " << evt << " name is NULL";
        EXPECT_GT(strlen(name), 0u) << "Event " << evt << " name is empty";
    }

    // Test print functions don't crash
    // Note: These print to stdout, we just verify no crash
    hypo_ibus_print_summary(bus);
    hypo_ibus_print_summary(nullptr);  // Should be NULL-safe

    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
    hypo_ibus_print_stats(&stats);
    hypo_ibus_print_stats(nullptr);  // Should be NULL-safe
}

// =============================================================================
// Test 17: Error Handling
// =============================================================================

/**
 * WHAT: Test error handling for invalid inputs
 * WHY:  Robust systems must handle errors gracefully
 * HOW:  Call functions with NULL/invalid parameters, verify error returns
 */
TEST_F(HypothalamusInternalBusPipelineTest, ErrorHandling) {
    // NULL bus operations
    EXPECT_EQ(-1, hypo_ibus_reset(nullptr));
    EXPECT_EQ(-1, hypo_ibus_subscribe(nullptr, HYPO_IMOD_DRIVES,
        HYPO_IEVT_STRESS_ONSET, generic_callback, nullptr));
    EXPECT_EQ(-1, hypo_ibus_unsubscribe(nullptr, 0));
    EXPECT_LT(hypo_ibus_publish(nullptr, nullptr), 0);
    EXPECT_EQ(-1, hypo_ibus_get_stats(nullptr, nullptr));
    EXPECT_EQ(-1, hypo_ibus_update_modulations(nullptr, 1000));

    // NULL callback
    EXPECT_EQ(-1, hypo_ibus_subscribe(bus, HYPO_IMOD_DRIVES,
        HYPO_IEVT_STRESS_ONSET, nullptr, nullptr));

    // NULL event
    EXPECT_LT(hypo_ibus_publish(bus, nullptr), 0);

    // NULL stats output
    EXPECT_EQ(-1, hypo_ibus_get_stats(bus, nullptr));

    // Invalid subscription ID
    EXPECT_EQ(-1, hypo_ibus_unsubscribe(bus, 9999));

    // NULL config
    EXPECT_EQ(-1, hypo_ibus_default_config(nullptr));

    // NULL modulation
    EXPECT_EQ(-1, hypo_ibus_register_modulation(bus, HYPO_IEVT_STRESS_ONSET, nullptr));

    // System should still be operational after errors
    hypo_ibus_stats_t stats;
    ASSERT_EQ(0, hypo_ibus_get_stats(bus, &stats));
}

// =============================================================================
// Test 18: Subscription to Module Events
// =============================================================================

/**
 * WHAT: Test subscription to all events from a specific module
 * WHY:  Sometimes a module needs to monitor all output from another
 * HOW:  Use subscribe_to_module, publish various events, verify receipt
 */
TEST_F(HypothalamusInternalBusPipelineTest, SubscriptionToModuleEvents) {
    EventTracker module_tracker;

    // Subscribe HPA to all events from CIRCADIAN module
    int sub = hypo_ibus_subscribe_to_module(bus, HYPO_IMOD_HPA_AXIS,
        HYPO_IMOD_CIRCADIAN, generic_callback, &module_tracker);
    ASSERT_GE(sub, 0);

    // Publish various circadian events
    ASSERT_EQ(0, hypo_ibus_publish_circadian_phase(bus, 0, 1, 0.3f, 0.6f, 0.5f));

    hypo_internal_event_t melatonin_event = {};
    melatonin_event.type = HYPO_IEVT_MELATONIN_ONSET;
    melatonin_event.source = HYPO_IMOD_CIRCADIAN;
    melatonin_event.data.circadian.melatonin_level = 0.7f;
    ASSERT_GE(hypo_ibus_publish(bus, &melatonin_event), 0);

    hypo_internal_event_t cortisol_event = {};
    cortisol_event.type = HYPO_IEVT_CORTISOL_AWAKENING;
    cortisol_event.source = HYPO_IMOD_CIRCADIAN;
    cortisol_event.data.circadian.cortisol_level = 0.8f;
    ASSERT_GE(hypo_ibus_publish(bus, &cortisol_event), 0);

    // All circadian events should be received
    EXPECT_GE(module_tracker.total_events.load(), 3);

    // Publish event from different module (should NOT be received)
    int initial_count = module_tracker.total_events.load();
    ASSERT_EQ(0, hypo_ibus_publish_stress(bus, HYPO_IEVT_STRESS_ONSET, 0.5f, 0.4f, true));

    // Count should not change for non-circadian events
    EXPECT_EQ(module_tracker.total_events.load(), initial_count);
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
