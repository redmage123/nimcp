/**
 * @file test_surprise_amplifier_e2e.cpp
 * @brief End-to-end tests for Surprise Amplifier (Society of Thought Phase 1)
 * @date 2026-01-27
 *
 * WHAT: E2E tests simulating complete surprise amplifier workflows
 * WHY:  Verify the full lifecycle: create -> connect -> signal -> amplify ->
 *       decay -> query -> destroy under realistic conditions
 * HOW:  Simulate realistic cognitive scenarios with multiple signal sources,
 *       time-stepped updates, and full statistics verification
 *
 * SCENARIOS:
 * 1. Reasoning re-evaluation: hypothesis invalidation triggers attention shift
 * 2. Multi-agent debate: conflicting agents drive curiosity exploration
 * 3. Sustained novelty: continuous novel input maintains elevated surprise
 * 4. Full lifecycle: create/config/signal/update/query/reset/destroy
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
}

/* ============================================================================
 * E2E Fixture
 * ============================================================================ */

class SurpriseAmplifierE2ETest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;

    void SetUp() override {}

    void TearDown() override {
        if (amp) {
            surprise_amplifier_destroy(amp);
            amp = nullptr;
        }
    }

    /** Create amplifier with no refractory, no bio-async */
    surprise_amplifier_t* create_test_amp() {
        surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
        cfg.refractory_period_ms = 0;
        cfg.max_concurrent = 256;
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        return surprise_amplifier_create(&cfg);
    }
};

/* ============================================================================
 * Scenario 1: Reasoning Re-evaluation
 *
 * Simulates a reasoning chain where a hypothesis is formed, then invalidated
 * by new evidence, triggering surprise and executive re-evaluation.
 * ============================================================================ */

TEST_F(SurpriseAmplifierE2ETest, ReasoningReEvaluationScenario) {
    amp = create_test_amp();
    ASSERT_NE(amp, nullptr);

    /* Phase 1: Low surprise during normal reasoning */
    for (int step = 0; step < 10; step++) {
        surprise_amplifier_on_prediction_error(amp, 0.05f, 0x100);
        surprise_amplifier_update(amp, 0.1f);
    }

    float normal_level = surprise_amplifier_get_current_level(amp);
    EXPECT_LT(normal_level, 0.3f);  /* Should be low/zero during normal operation */

    /* Phase 2: Hypothesis invalidation - sudden surprise spike */
    surprise_amplifier_on_hypothesis_invalidated(amp, 0.95f, 0.05f);

    float spike_level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(spike_level, 0.5f);  /* Strong surprise */

    /* Verify event details */
    surprise_event_t event;
    surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(event.source, SURPRISE_SOURCE_HYPOTHESIS_INVALIDATED);
    EXPECT_GT(event.attention_boost, 0.5f);  /* Strong attention redirect */

    /* Verify executive interrupt was triggered */
    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.executive_interrupts, 1u);

    /* Phase 3: Gradual recovery */
    for (int step = 0; step < 50; step++) {
        surprise_amplifier_update(amp, 0.5f);
    }

    float recovered_level = surprise_amplifier_get_current_level(amp);
    EXPECT_LT(recovered_level, spike_level);  /* Decayed significantly */
}

/* ============================================================================
 * Scenario 2: Multi-Agent Debate
 *
 * Simulates multiple reasoning agents producing conflicting conclusions.
 * Each conflict should drive curiosity to explore alternatives.
 * ============================================================================ */

TEST_F(SurpriseAmplifierE2ETest, MultiAgentDebateScenario) {
    amp = create_test_amp();
    ASSERT_NE(amp, nullptr);

    /* Simulate 5 rounds of agent conflicts with varying intensity */
    struct ConflictRound {
        uint32_t agent_a;
        float conf_a;
        uint32_t agent_b;
        float conf_b;
        float divergence;
    };

    ConflictRound rounds[] = {
        {0x100, 0.7f, 0x200, 0.6f, 0.3f},  /* Mild disagreement */
        {0x100, 0.8f, 0x300, 0.8f, 0.5f},  /* Moderate: both confident */
        {0x200, 0.9f, 0x300, 0.9f, 0.8f},  /* Strong: highly confident, high divergence */
        {0x100, 0.5f, 0x200, 0.4f, 0.2f},  /* Weak: low confidence */
        {0x300, 0.95f, 0x400, 0.9f, 0.9f}, /* Very strong: near certain, maximal divergence */
    };

    std::vector<float> curiosity_boosts;

    for (const auto& round : rounds) {
        surprise_amplifier_on_agent_conflict(
            amp, round.agent_a, round.conf_a,
            round.agent_b, round.conf_b, round.divergence);

        surprise_event_t event;
        if (surprise_amplifier_get_last_event(amp, &event) == 0) {
            curiosity_boosts.push_back(event.curiosity_boost);
        }

        surprise_amplifier_update(amp, 0.2f);
    }

    /* Verify statistics */
    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.conflict_triggered, 1u);
    EXPECT_GT(stats.avg_curiosity_boost, 0.0f);

    /* Verify stronger conflicts produced higher curiosity */
    if (curiosity_boosts.size() >= 2) {
        float max_boost = *std::max_element(curiosity_boosts.begin(), curiosity_boosts.end());
        EXPECT_GT(max_boost, 0.3f);
    }
}

/* ============================================================================
 * Scenario 3: Sustained Novelty Stream
 *
 * Simulates continuous exposure to novel patterns, maintaining elevated
 * surprise with periodic updates.
 * ============================================================================ */

TEST_F(SurpriseAmplifierE2ETest, SustainedNoveltyStream) {
    amp = create_test_amp();
    ASSERT_NE(amp, nullptr);

    float avg_level = 0.0f;
    int sample_count = 0;

    for (int step = 0; step < 50; step++) {
        /* Novel input every step */
        surprise_amplifier_on_novelty(amp, 0.6f + 0.1f * sinf(step * 0.3f), 0x500 + step);
        surprise_amplifier_update(amp, 0.1f);

        float level = surprise_amplifier_get_current_level(amp);
        avg_level += level;
        sample_count++;
    }

    avg_level /= sample_count;

    /* Sustained novelty should maintain above-zero average surprise */
    EXPECT_GT(avg_level, 0.1f);

    /* Statistics should show many novelty events */
    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.novelty_triggered, 10u);
    EXPECT_EQ(stats.total_updates, 50u);
}

/* ============================================================================
 * Scenario 4: Full Lifecycle
 *
 * Tests the complete lifecycle: create -> configure -> connect -> signal ->
 * update -> query -> reset -> re-signal -> destroy
 * ============================================================================ */

TEST_F(SurpriseAmplifierE2ETest, FullLifecycle) {
    /* Step 1: Custom configuration */
    surprise_amplifier_config_t cfg = surprise_amplifier_default_config();
    cfg.base_threshold = 0.2f;
    cfg.amplification_gain = 3.0f;
    cfg.refractory_period_ms = 0;
    cfg.enable_bio_async = false;
    cfg.enable_executive_interrupt = true;
    cfg.executive_interrupt_threshold = 0.5f;

    amp = surprise_amplifier_create(&cfg);
    ASSERT_NE(amp, nullptr);

    /* Step 2: Connect dummy subsystems */
    int dummy_fep = 1;
    EXPECT_EQ(surprise_amplifier_connect_fep(amp, &dummy_fep), 0);

    /* Step 3: Verify initial state */
    EXPECT_FLOAT_EQ(surprise_amplifier_get_current_level(amp), 0.0f);
    EXPECT_FALSE(surprise_amplifier_is_in_refractory(amp));
    EXPECT_FALSE(surprise_amplifier_is_bio_async_connected(amp));

    /* Step 4: Signal processing */
    surprise_amplifier_on_prediction_error(amp, 0.5f, 0x100);
    surprise_amplifier_on_bayesian_surprise(amp, 2.0f, 0x200);
    surprise_amplifier_on_hypothesis_invalidated(amp, 0.8f, 0.2f);

    /* Step 5: Query state */
    float level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(level, 0.0f);

    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GE(stats.total_surprises, 2u);
    EXPECT_GT(stats.avg_magnitude, 0.0f);
    EXPECT_GT(stats.max_magnitude, 0.0f);
    EXPECT_GT(stats.avg_attention_boost, 0.0f);
    EXPECT_GT(stats.avg_curiosity_boost, 0.0f);

    /* Step 6: Update cycle */
    for (int i = 0; i < 20; i++) {
        surprise_amplifier_update(amp, 0.5f);
    }

    float decayed = surprise_amplifier_get_current_level(amp);
    EXPECT_LT(decayed, level);

    /* Step 7: Event history */
    surprise_event_t events[10];
    uint32_t count = 0;
    surprise_amplifier_get_history(amp, events, 10, &count);
    EXPECT_GT(count, 0u);

    /* Step 8: Reset and verify clean state */
    EXPECT_EQ(surprise_amplifier_reset(amp), 0);
    EXPECT_FLOAT_EQ(surprise_amplifier_get_current_level(amp), 0.0f);

    surprise_amplifier_stats_t reset_stats = surprise_amplifier_get_stats(amp);
    EXPECT_EQ(reset_stats.total_surprises, 0u);

    /* Step 9: Re-signal after reset */
    surprise_amplifier_on_novelty(amp, 0.9f, 0x300);
    EXPECT_GT(surprise_amplifier_get_current_level(amp), 0.0f);

    /* Step 10: Destroy (implicit in TearDown) */
}

/* ============================================================================
 * Scenario 5: Mixed Signal Barrage
 *
 * Stress test with all signal types firing in rapid succession.
 * ============================================================================ */

TEST_F(SurpriseAmplifierE2ETest, MixedSignalBarrage) {
    amp = create_test_amp();
    ASSERT_NE(amp, nullptr);

    /* Fire 200 mixed signals */
    for (int i = 0; i < 200; i++) {
        float magnitude = 0.3f + 0.5f * (float)(i % 7) / 7.0f;

        switch (i % 5) {
            case 0:
                surprise_amplifier_on_prediction_error(amp, magnitude, 0x100);
                break;
            case 1:
                surprise_amplifier_on_agent_conflict(amp, 0x100, 0.8f, 0x200, 0.7f, magnitude);
                break;
            case 2:
                surprise_amplifier_on_hypothesis_invalidated(amp, 0.9f, 0.9f - magnitude);
                break;
            case 3:
                surprise_amplifier_on_novelty(amp, magnitude, 0x300);
                break;
            case 4:
                surprise_amplifier_on_bayesian_surprise(amp, magnitude * 3.0f, 0x400);
                break;
        }

        if (i % 3 == 0) {
            surprise_amplifier_update(amp, 0.05f);
        }
    }

    /* Verify system is still functioning */
    surprise_amplifier_stats_t stats = surprise_amplifier_get_stats(amp);
    EXPECT_GT(stats.total_surprises, 50u);
    EXPECT_GE(stats.fep_triggered, 1u);
    EXPECT_GE(stats.hypothesis_triggered, 1u);
    EXPECT_GE(stats.novelty_triggered, 1u);
    EXPECT_GE(stats.bayesian_triggered, 1u);
    EXPECT_GT(stats.avg_magnitude, 0.0f);
    EXPECT_LE(stats.max_magnitude, 1.0f);

    /* History should be full */
    surprise_event_t events[SURPRISE_HISTORY_SIZE];
    uint32_t count = 0;
    surprise_amplifier_get_history(amp, events, SURPRISE_HISTORY_SIZE, &count);
    EXPECT_EQ(count, (uint32_t)SURPRISE_HISTORY_SIZE);
}
