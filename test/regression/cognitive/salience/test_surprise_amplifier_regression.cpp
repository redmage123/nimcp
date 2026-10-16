/**
 * @file test_surprise_amplifier_regression.cpp
 * @brief Regression tests for Surprise Amplifier (Society of Thought Phase 1)
 * @date 2026-01-27
 *
 * WHAT: Regression tests ensuring surprise amplifier maintains baseline behavior
 * WHY:  Prevent regressions in amplification accuracy, decay rates, and thresholds
 * HOW:  Test known baseline scenarios, boundary conditions, performance thresholds
 *
 * BASELINES (from Kim et al. 2026):
 * - Amplification at gain=2.0 should roughly double effective surprise
 * - Decay to zero within 100 seconds at rate 0.95
 * - Refractory period prevents events within 100ms
 * - All five source types produce valid events
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
}

/* ============================================================================
 * Regression Test Fixture
 * ============================================================================ */

class SurpriseAmplifierRegressionTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;

    void SetUp() override {
        surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
        cfg.refractory_period_ms = 0;
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;

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
 * Amplification Baseline Tests
 * ============================================================================ */

/**
 * BASELINE: Default gain (2.0) applied to 0.5 input produces magnitude = 1.0 (clamped)
 */
TEST_F(SurpriseAmplifierRegressionTest, AmplificationGainBaseline) {
    surprise_amplifier_on_prediction_error(amp, 0.5f, 0x100);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, 0);

    /* gain=2.0, input=0.5, weight=1.0 -> amplified=1.0 (clamped) */
    EXPECT_FLOAT_EQ(event.magnitude, 1.0f);
}

/**
 * BASELINE: Small input (0.2) with gain (2.0) produces magnitude=0.4 > threshold(0.3)
 */
TEST_F(SurpriseAmplifierRegressionTest, ThresholdBoundaryAbove) {
    surprise_amplifier_on_prediction_error(amp, 0.2f, 0x100);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(event.magnitude, 0.4f);
}

/**
 * BASELINE: Input 0.1 with gain 2.0 = 0.2 < threshold 0.3 -> no event
 */
TEST_F(SurpriseAmplifierRegressionTest, ThresholdBoundaryBelow) {
    surprise_amplifier_on_prediction_error(amp, 0.1f, 0x100);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_NOT_INITIALIZED);  /* No event stored */
}

/**
 * BASELINE: Conflict weight (1.5) amplifies inter-agent conflicts
 */
TEST_F(SurpriseAmplifierRegressionTest, ConflictWeightBaseline) {
    /* Two equally confident agents: min_conf=0.8, divergence=0.6 */
    /* conflict_strength = 0.8 * 0.6 * (1 - 0.0*0.5) = 0.48 */
    /* amplified = 0.48 * 1.5 * 2.0 = 1.44, clamped to 1.0 */
    surprise_amplifier_on_agent_conflict(amp, 0x100, 0.8f, 0x200, 0.8f, 0.6f);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(event.source, SURPRISE_SOURCE_INTER_AGENT_CONFLICT);
    EXPECT_GT(event.magnitude, 0.3f);  /* Above threshold */
}

/**
 * BASELINE: Hypothesis invalidation with large drop produces high surprise
 */
TEST_F(SurpriseAmplifierRegressionTest, HypothesisInvalidationBaseline) {
    /* prior=0.95, posterior=0.05 -> drop=0.9 */
    /* amplified = 0.9 * 1.3 * 2.0 = 2.34, clamped to 1.0 */
    surprise_amplifier_on_hypothesis_invalidated(amp, 0.95f, 0.05f);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(event.magnitude, 1.0f);  /* Should be clamped to max */
}

/**
 * BASELINE: Bayesian surprise normalizes KL divergence correctly
 */
TEST_F(SurpriseAmplifierRegressionTest, BayesianNormalizationBaseline) {
    /* KL=1.0 -> normalized = 1.0/(1+1.0) = 0.5 */
    /* amplified = 0.5 * 1.1 * 2.0 = 1.1, clamped to 1.0 */
    surprise_amplifier_on_bayesian_surprise(amp, 1.0f, 0x400);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(event.magnitude, 0.5f);
}

/* ============================================================================
 * Decay Rate Baseline Tests
 * ============================================================================ */

/**
 * BASELINE: After 10 seconds of decay at rate 0.95, level should be ~0.60 of initial
 * Formula: level * 0.95^10 = level * 0.5987
 */
TEST_F(SurpriseAmplifierRegressionTest, DecayRateBaseline10s) {
    surprise_amplifier_on_prediction_error(amp, 0.5f, 0x100);
    float initial = surprise_amplifier_get_current_level(amp);

    for (int i = 0; i < 10; i++) {
        surprise_amplifier_update(amp, 1.0f);
    }
    float after_10s = surprise_amplifier_get_current_level(amp);

    float expected_ratio = powf(0.95f, 10.0f);  /* ~0.5987 */
    float actual_ratio = after_10s / initial;

    EXPECT_NEAR(actual_ratio, expected_ratio, 0.05f);
}

/**
 * BASELINE: Level reaches zero (< 0.001) within 200 update seconds
 */
TEST_F(SurpriseAmplifierRegressionTest, DecayToZeroBaseline) {
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);

    for (int i = 0; i < 200; i++) {
        surprise_amplifier_update(amp, 1.0f);
    }

    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

/* ============================================================================
 * Boost Calculation Baseline Tests
 * ============================================================================ */

/**
 * BASELINE: Attention boost = magnitude * attention_boost_factor (1.5)
 */
TEST_F(SurpriseAmplifierRegressionTest, AttentionBoostCalculation) {
    surprise_amplifier_on_prediction_error(amp, 0.2f, 0x100);

    surprise_event_t event;
    surprise_amplifier_get_last_event(amp, &event);

    /* magnitude=0.4, attention_factor=1.5 -> boost=0.6 */
    float expected_boost = event.magnitude * 1.5f;
    EXPECT_FLOAT_EQ(event.attention_boost, expected_boost);
}

/**
 * BASELINE: Curiosity boost = magnitude * curiosity_boost_factor (1.2)
 */
TEST_F(SurpriseAmplifierRegressionTest, CuriosityBoostCalculation) {
    surprise_amplifier_on_prediction_error(amp, 0.2f, 0x100);

    surprise_event_t event;
    surprise_amplifier_get_last_event(amp, &event);

    /* magnitude=0.4, curiosity_factor=1.2 -> boost=0.48 */
    float expected_boost = event.magnitude * 1.2f;
    EXPECT_FLOAT_EQ(event.curiosity_boost, expected_boost);
}

/* ============================================================================
 * Boundary Condition Tests
 * ============================================================================ */

/**
 * BOUNDARY: Maximum magnitude is clamped to 1.0
 */
TEST_F(SurpriseAmplifierRegressionTest, MagnitudeClampedToOne) {
    /* input=1.0, gain=2.0 -> amplified=2.0, clamped to 1.0 */
    surprise_amplifier_on_prediction_error(amp, 1.0f, 0x100);

    surprise_event_t event;
    surprise_amplifier_get_last_event(amp, &event);
    EXPECT_FLOAT_EQ(event.magnitude, 1.0f);
}

/**
 * BOUNDARY: Zero input produces no event
 */
TEST_F(SurpriseAmplifierRegressionTest, ZeroInputProducesNoEvent) {
    surprise_amplifier_on_prediction_error(amp, 0.0f, 0x100);

    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ERROR_NOT_INITIALIZED);
}

/**
 * BOUNDARY: Update with dt=0 is a no-op
 */
TEST_F(SurpriseAmplifierRegressionTest, ZeroDtNoOp) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);
    float before = surprise_amplifier_get_current_level(amp);

    surprise_amplifier_update(amp, 0.0f);
    float after = surprise_amplifier_get_current_level(amp);

    EXPECT_FLOAT_EQ(before, after);
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

/**
 * PERFORMANCE: 10000 events should complete within 500ms
 */
TEST_F(SurpriseAmplifierRegressionTest, PerformanceBulkEvents) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_amplifier_on_prediction_error(amp, 0.5f + (float)(i % 5) * 0.1f, 0x100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(elapsed, 500);
}

/**
 * PERFORMANCE: 10000 update cycles should complete within 200ms
 */
TEST_F(SurpriseAmplifierRegressionTest, PerformanceBulkUpdates) {
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        surprise_amplifier_update(amp, 0.001f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_LT(elapsed, 200);
}
