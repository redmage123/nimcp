/**
 * @file test_surprise_amplifier_integration.cpp
 * @brief Integration tests for Surprise Amplifier (Society of Thought Phase 1)
 * @date 2026-01-27
 *
 * WHAT: Integration tests for surprise amplifier with connected subsystems
 * WHY:  Verify multi-signal processing, decay cycles, statistics convergence,
 *       and correct interaction between amplification, refractory, and routing
 * HOW:  GoogleTest fixture with realistic multi-event scenarios
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>
#include <thread>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
}

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class SurpriseAmplifierIntegrationTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
        cfg.refractory_period_ms = 0;   /* Disable refractory for integration tests */
        cfg.max_concurrent = 256;       /* Allow many events for history tests */
        cfg.enable_bio_async = false;    /* No bio-async router in test */
        cfg.enable_logging = false;      /* Reduce test noise */

        amp = surprise_amplifier_create(&cfg);
        ASSERT_NE(amp, nullptr);
    }

    void TearDown() override {
        if (amp) {
            surprise_amplifier_destroy(amp);
            amp = nullptr;
        }
    }
};

/* ============================================================================
 * Multi-Signal Processing
 * ============================================================================ */

/**
 * WHAT: Multiple signal types accumulate in statistics correctly
 * WHY:  In practice, all five signal types fire concurrently
 * HOW:  Fire one of each type, verify per-type counters
 */
TEST_F(SurpriseAmplifierIntegrationTest, AllSignalTypesAccumulate) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);
    surprise_amplifier_on_agent_conflict(amp, 0x100, 0.9f, 0x200, 0.8f, 0.7f);
    surprise_amplifier_on_hypothesis_invalidated(amp, 0.9f, 0.1f);
    surprise_amplifier_on_novelty(amp, 0.8f, 0x300);
    surprise_amplifier_on_bayesian_surprise(amp, 5.0f, 0x400);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);

    EXPECT_GE(stats.total_surprises, 3u);  /* At least 3 should exceed threshold */
    EXPECT_GE(stats.fep_triggered, 1u);
    EXPECT_GE(stats.hypothesis_triggered, 1u);
    EXPECT_GT(stats.avg_magnitude, 0.0f);
    EXPECT_GT(stats.max_magnitude, 0.0f);
    EXPECT_LE(stats.max_magnitude, 1.0f);
}

/**
 * WHAT: Surprise level represents peak of recent events
 * WHY:  Current level should reflect the strongest active signal
 * HOW:  Fire weak then strong, verify level matches stronger event
 */
TEST_F(SurpriseAmplifierIntegrationTest, PeakLevelTracking) {
    surprise_amplifier_on_prediction_error(amp, 0.3f, 0x100);
    float level_weak = surprise_amplifier_get_current_level(amp);

    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x200);
    float level_strong = surprise_amplifier_get_current_level(amp);

    EXPECT_GT(level_strong, level_weak);
}

/* ============================================================================
 * Decay and Update Cycle
 * ============================================================================ */

/**
 * WHAT: Update cycle decays surprise monotonically
 * WHY:  Biological adaptation: surprise fades without new stimuli
 * HOW:  Fire event, update repeatedly, verify monotonic decrease
 */
TEST_F(SurpriseAmplifierIntegrationTest, MonotonicDecay) {
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);

    float prev_level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(prev_level, 0.0f);

    for (int i = 0; i < 20; i++) {
        surprise_amplifier_update(amp, 0.5f);
        float curr_level = surprise_amplifier_get_current_level(amp);
        EXPECT_LE(curr_level, prev_level + 0.001f);  /* Allow float rounding */
        prev_level = curr_level;
    }
}

/**
 * WHAT: New event during decay raises level back up
 * WHY:  Fresh surprise should override decay
 * HOW:  Fire, decay partially, fire again, verify level increased
 */
TEST_F(SurpriseAmplifierIntegrationTest, NewEventOverridesDecay) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);

    /* Decay for a while */
    for (int i = 0; i < 10; i++) {
        surprise_amplifier_update(amp, 0.5f);
    }
    float decayed_level = surprise_amplifier_get_current_level(amp);

    /* Fire strong event */
    surprise_amplifier_on_prediction_error(amp, 0.95f, 0x200);
    float boosted_level = surprise_amplifier_get_current_level(amp);

    EXPECT_GT(boosted_level, decayed_level);
}

/* ============================================================================
 * History Ring Buffer
 * ============================================================================ */

/**
 * WHAT: History correctly wraps around ring buffer
 * WHY:  History is a fixed-size ring buffer (SURPRISE_HISTORY_SIZE=64)
 * HOW:  Fire more than 64 events, verify history returns correct count
 */
TEST_F(SurpriseAmplifierIntegrationTest, HistoryRingBufferWrap) {
    for (int i = 0; i < 100; i++) {
        surprise_amplifier_on_prediction_error(amp, 0.5f + (float)(i % 5) * 0.1f, 0x100 + i);
    }

    surprise_event_t events[SURPRISE_HISTORY_SIZE];
    uint32_t count = 0;
    int rc = surprise_amplifier_get_history(amp, events, SURPRISE_HISTORY_SIZE, &count);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(count, (uint32_t)SURPRISE_HISTORY_SIZE);

    /* All events should have valid timestamps */
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GT(events[i].timestamp_ns, 0u);
        EXPECT_GT(events[i].magnitude, 0.0f);
    }
}

/**
 * WHAT: History partial retrieval returns correct subset
 * WHY:  Callers may only want recent N events
 * HOW:  Fire 10 events, request 5, verify count=5
 */
TEST_F(SurpriseAmplifierIntegrationTest, HistoryPartialRetrieval) {
    for (int i = 0; i < 10; i++) {
        surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100 + i);
    }

    surprise_event_t events[5];
    uint32_t count = 0;
    int rc = surprise_amplifier_get_history(amp, events, 5, &count);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(count, 5u);
}

/* ============================================================================
 * Reset Integration
 * ============================================================================ */

/**
 * WHAT: Reset followed by new events works correctly
 * WHY:  Reset is used to restart without reallocating
 * HOW:  Fire events, reset, fire new events, verify fresh stats
 */
TEST_F(SurpriseAmplifierIntegrationTest, ResetThenNewEvents) {
    /* Phase 1: Fire events */
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);
    surprise_amplifier_on_novelty(amp, 0.7f, 0x200);

    surprise_amplifier_stats_t stats1 = surprise_amplifier_get_stats(amp);
    uint64_t total1 = stats1.total_surprises;
    EXPECT_GT(total1, 0u);

    /* Reset */
    surprise_amplifier_reset(amp);

    /* Phase 2: Fresh events */
    surprise_amplifier_on_hypothesis_invalidated(amp, 0.9f, 0.1f);

    surprise_amplifier_stats_t stats2 = surprise_amplifier_get_stats(amp);
    EXPECT_LT(stats2.total_surprises, total1);
    EXPECT_GE(stats2.hypothesis_triggered, 1u);
    EXPECT_EQ(stats2.fep_triggered, 0u);
}

/* ============================================================================
 * Executive Interrupt Threshold
 * ============================================================================ */

/**
 * WHAT: Executive interrupts are counted when magnitude exceeds threshold
 * WHY:  High surprise should trigger executive re-evaluation
 * HOW:  Fire high-magnitude event, check executive_interrupts in stats
 */
TEST_F(SurpriseAmplifierIntegrationTest, ExecutiveInterruptThreshold) {
    /* Default executive_interrupt_threshold = 0.7 */
    /* prediction_error=0.95, gain=2.0 -> amplified=1.9 (clamped to 1.0) > 0.7 */
    surprise_amplifier_on_prediction_error(amp, 0.95f, 0x100);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.executive_interrupts, 1u);
}

/**
 * WHAT: Low surprise does not trigger executive interrupt
 * WHY:  Only high surprise should interrupt executive processing
 * HOW:  Fire moderate event below interrupt threshold, verify no interrupt
 */
TEST_F(SurpriseAmplifierIntegrationTest, NoExecutiveInterruptBelowThreshold) {
    /* threshold=0.3, gain=2.0, input=0.25 -> amplified=0.5 */
    /* executive_interrupt_threshold=0.7, 0.5 < 0.7 -> no interrupt */
    surprise_amplifier_on_prediction_error(amp, 0.25f, 0x100);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_EQ(stats.executive_interrupts, 0u);
}

/* ============================================================================
 * Statistics Convergence
 * ============================================================================ */

/**
 * WHAT: Running average magnitude converges under steady input
 * WHY:  Statistics should be stable under constant input
 * HOW:  Fire many events at same magnitude, check avg approaches input
 */
TEST_F(SurpriseAmplifierIntegrationTest, AverageMagnitudeConverges) {
    for (int i = 0; i < 50; i++) {
        surprise_amplifier_on_prediction_error(amp, 0.5f, 0x100);
    }

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);

    /* With gain=2.0, input=0.5 -> amplified=1.0 (clamped) */
    /* Average should converge near 1.0 */
    EXPECT_GT(stats.avg_magnitude, 0.5f);
    EXPECT_LE(stats.avg_magnitude, 1.0f);
}
