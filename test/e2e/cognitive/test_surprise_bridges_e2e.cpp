/**
 * @file test_surprise_bridges_e2e.cpp
 * @brief End-to-end tests for Surprise Bridges (Society of Thought Phase 1)
 * @date 2026-01-27
 *
 * WHAT: E2E tests simulating complete surprise bridge workflows
 * WHY:  Verify the full amplifier-bridge pipeline under realistic conditions
 * HOW:  Create amplifier + all bridges, run multi-step scenarios
 *
 * SCENARIOS:
 * 1. Reasoning surprise cascades through all bridges
 * 2. FEP-driven surprise with precision feedback
 * 3. Sustained activity with periodic GW broadcasts
 * 4. Full lifecycle: create/connect/process/reset/destroy
 * 5. Mixed signals stress test across all bridges
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "cognitive/salience/nimcp_surprise_fep_bridge.h"
#include "cognitive/salience/nimcp_surprise_gw_bridge.h"
#include "cognitive/salience/nimcp_surprise_attention_bridge.h"
}

/* ============================================================================
 * E2E Fixture
 * ============================================================================ */

class SurpriseBridgesE2ETest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_fep_bridge_t* fep_bridge = nullptr;
    surprise_gw_bridge_t* gw_bridge = nullptr;
    surprise_att_bridge_t* att_bridge = nullptr;

    void SetUp() override {}

    void TearDown() override {
        if (att_bridge) { surprise_att_bridge_destroy(att_bridge); att_bridge = nullptr; }
        if (gw_bridge) { surprise_gw_bridge_destroy(gw_bridge); gw_bridge = nullptr; }
        if (fep_bridge) { surprise_fep_bridge_destroy(fep_bridge); fep_bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }

    void CreateFullPipeline() {
        /* Create amplifier */
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);
        ASSERT_NE(amp, nullptr);

        /* Create and connect FEP bridge */
        surprise_fep_config_t fep_cfg = surprise_fep_bridge_default_config();
        fep_cfg.enable_bio_async = false;
        fep_cfg.enable_logging = false;
        fep_bridge = surprise_fep_bridge_create(&fep_cfg);
        ASSERT_NE(fep_bridge, nullptr);
        ASSERT_EQ(surprise_fep_bridge_connect_amplifier(fep_bridge, amp), 0);

        /* Create and connect GW bridge */
        surprise_gw_config_t gw_cfg = surprise_gw_bridge_default_config();
        gw_cfg.enable_bio_async = false;
        gw_cfg.enable_logging = false;
        gw_bridge = surprise_gw_bridge_create(&gw_cfg);
        ASSERT_NE(gw_bridge, nullptr);
        ASSERT_EQ(surprise_gw_bridge_connect_amplifier(gw_bridge, amp), 0);

        /* Create and connect Attention bridge */
        surprise_att_config_t att_cfg = surprise_att_bridge_default_config();
        att_cfg.enable_bio_async = false;
        att_cfg.enable_logging = false;
        att_bridge = surprise_att_bridge_create(&att_cfg);
        ASSERT_NE(att_bridge, nullptr);
        ASSERT_EQ(surprise_att_bridge_connect_amplifier(att_bridge, amp), 0);
    }

    /** Run one time step: update amplifier then all bridges */
    void StepAll(float dt) {
        surprise_amplifier_update(amp, dt);
        surprise_fep_bridge_update(fep_bridge, dt);
        surprise_gw_bridge_update(gw_bridge, dt);
        surprise_att_bridge_update(att_bridge, dt);
    }
};

/* ============================================================================
 * Scenario 1: Reasoning Surprise Cascade
 *
 * A hypothesis invalidation creates surprise that cascades through all bridges:
 * amplifier -> FEP precision boost -> GW broadcast -> attention shift
 * ============================================================================ */

TEST_F(SurpriseBridgesE2ETest, ReasoningSurpriseCascade) {
    CreateFullPipeline();

    /* Phase 1: Normal operation - low baseline */
    for (int i = 0; i < 5; i++) {
        surprise_amplifier_on_prediction_error(amp, 0.05f, 0x100);
        StepAll(0.1f);
    }

    float baseline_precision = surprise_fep_get_precision_boost(fep_bridge);
    float baseline_gw_sens = surprise_gw_get_sensitivity(gw_bridge);
    float baseline_att_sens = surprise_att_get_sensitivity(att_bridge);

    /* Phase 2: Hypothesis invalidation - surprise spike */
    surprise_amplifier_on_hypothesis_invalidated(amp, 0.95f, 0.05f);

    /* Apply surprise effects to bridges */
    surprise_event_t event;
    int rc = surprise_amplifier_get_last_event(amp, &event);
    EXPECT_EQ(rc, 0);

    /* Route through attention bridge */
    surprise_att_apply_boost(att_bridge, event.magnitude, event.attention_boost);
    surprise_att_request_shift(att_bridge, event.magnitude, 0x42);

    /* Route through GW bridge */
    surprise_gw_submit_broadcast(gw_bridge, event.magnitude, event.source,
                                  event.attention_boost, event.curiosity_boost);

    /* Update all bridges to compute effects */
    StepAll(0.1f);

    /* Verify cascade effects */
    float spike_precision = surprise_fep_get_precision_boost(fep_bridge);
    EXPECT_GT(spike_precision, baseline_precision);

    surprise_att_effects_t att_effects;
    surprise_att_bridge_get_effects(att_bridge, &att_effects);
    EXPECT_GT(att_effects.current_attention_boost, 0.0f);

    surprise_gw_stats_t gw_stats;
    surprise_gw_bridge_get_stats(gw_bridge, &gw_stats);
    EXPECT_GE(gw_stats.broadcasts_submitted, 1u);
    EXPECT_GE(gw_stats.broadcasts_won, 1u);

    /* Phase 3: Recovery - surprise decays */
    for (int i = 0; i < 50; i++) {
        StepAll(0.5f);
    }

    float recovered_precision = surprise_fep_get_precision_boost(fep_bridge);
    EXPECT_LT(recovered_precision, spike_precision);
}

/* ============================================================================
 * Scenario 2: FEP-Driven Surprise with Precision Feedback
 *
 * FEP prediction errors flow through the FEP bridge to the amplifier,
 * which then feeds back through the bridge as precision modulation.
 * ============================================================================ */

TEST_F(SurpriseBridgesE2ETest, FepDrivenPrecisionFeedbackLoop) {
    CreateFullPipeline();

    std::vector<float> precision_history;

    /* Run 20 cycles: forward PE -> amplifier -> bridge update -> read precision */
    for (int step = 0; step < 20; step++) {
        /* Forward a moderate PE every other step */
        if (step % 2 == 0) {
            surprise_fep_forward_pe(fep_bridge, 0.6f, 0x100 + step);
        }

        /* Update everything */
        StepAll(0.2f);

        /* Record precision */
        precision_history.push_back(surprise_fep_get_precision_boost(fep_bridge));
    }

    /* Precision should have been above baseline at some point */
    float max_precision = *std::max_element(precision_history.begin(), precision_history.end());
    EXPECT_GT(max_precision, 1.0f);

    /* Verify FEP stats */
    surprise_fep_stats_t fep_stats;
    surprise_fep_bridge_get_stats(fep_bridge, &fep_stats);
    EXPECT_GE(fep_stats.pe_events_forwarded, 5u);
    EXPECT_GE(fep_stats.total_updates, 20u);
}

/* ============================================================================
 * Scenario 3: Sustained Activity with Periodic GW Broadcasts
 *
 * Continuous surprise events with GW broadcasts at intervals (respecting cooldown).
 * ============================================================================ */

TEST_F(SurpriseBridgesE2ETest, SustainedActivityWithGwBroadcasts) {
    CreateFullPipeline();

    uint32_t broadcast_attempts = 0;
    float simulation_time = 0.0f;

    for (int step = 0; step < 100; step++) {
        float dt = 0.1f;
        simulation_time += dt;

        /* Periodic surprise events */
        if (step % 3 == 0) {
            surprise_amplifier_on_novelty(amp, 0.7f, 0x500 + step);
        }

        /* Try to broadcast every 5th step */
        if (step % 5 == 0) {
            float level = surprise_amplifier_get_current_level(amp);
            if (level > 0.0f) {
                surprise_gw_submit_broadcast(gw_bridge, level, 1, level * 1.5f, level * 1.2f);
                broadcast_attempts++;
            }
        }

        StepAll(dt);
    }

    surprise_gw_stats_t gw_stats;
    surprise_gw_bridge_get_stats(gw_bridge, &gw_stats);

    /* Should have some successful broadcasts */
    EXPECT_GT(gw_stats.broadcasts_submitted, 0u);

    /* Some should have been cooled (rapid attempts) */
    uint64_t total_attempts = gw_stats.broadcasts_submitted +
                               gw_stats.broadcasts_cooled +
                               gw_stats.below_threshold;
    EXPECT_GT(total_attempts, 0u);

    EXPECT_GE(gw_stats.total_updates, 100u);
}

/* ============================================================================
 * Scenario 4: Full Lifecycle
 *
 * Tests complete lifecycle: create -> connect -> process -> query ->
 * reset -> re-process -> destroy
 * ============================================================================ */

TEST_F(SurpriseBridgesE2ETest, FullLifecycle) {
    CreateFullPipeline();

    /* Step 1: Verify initial state */
    EXPECT_FLOAT_EQ(surprise_fep_get_precision_boost(fep_bridge), 1.0f);
    EXPECT_FLOAT_EQ(surprise_gw_get_sensitivity(gw_bridge), 1.0f);
    EXPECT_FLOAT_EQ(surprise_att_get_sensitivity(att_bridge), 1.0f);

    /* Step 2: Process events through full pipeline */
    surprise_fep_forward_pe(fep_bridge, 0.8f, 0x100);
    surprise_fep_forward_bayesian(fep_bridge, 2.5f, 0x200);
    surprise_att_apply_boost(att_bridge, 0.9f, 1.5f);
    surprise_att_request_shift(att_bridge, 0.8f, 0x42);
    surprise_gw_submit_broadcast(gw_bridge, 0.8f, 1, 0.5f, 0.3f);

    /* Step 3: Update all */
    StepAll(0.1f);

    /* Step 4: Verify all bridges have state */
    surprise_fep_stats_t fep_stats;
    surprise_fep_bridge_get_stats(fep_bridge, &fep_stats);
    EXPECT_GT(fep_stats.pe_events_forwarded, 0u);
    EXPECT_GT(fep_stats.bayesian_events_forwarded, 0u);
    EXPECT_GT(fep_stats.total_updates, 0u);

    surprise_gw_stats_t gw_stats;
    surprise_gw_bridge_get_stats(gw_bridge, &gw_stats);
    EXPECT_GT(gw_stats.broadcasts_submitted, 0u);

    surprise_att_stats_t att_stats;
    surprise_att_bridge_get_stats(att_bridge, &att_stats);
    EXPECT_GT(att_stats.attention_boosts, 0u);
    EXPECT_GT(att_stats.attention_shifts, 0u);

    /* Step 5: Reset all bridges */
    EXPECT_EQ(surprise_fep_bridge_reset(fep_bridge), 0);
    EXPECT_EQ(surprise_gw_bridge_reset(gw_bridge), 0);
    EXPECT_EQ(surprise_att_bridge_reset(att_bridge), 0);

    /* Step 6: Verify clean state */
    surprise_fep_bridge_get_stats(fep_bridge, &fep_stats);
    EXPECT_EQ(fep_stats.pe_events_forwarded, 0u);
    EXPECT_EQ(fep_stats.total_updates, 0u);

    surprise_gw_bridge_get_stats(gw_bridge, &gw_stats);
    EXPECT_EQ(gw_stats.broadcasts_submitted, 0u);

    surprise_att_bridge_get_stats(att_bridge, &att_stats);
    EXPECT_EQ(att_stats.attention_boosts, 0u);

    /* Step 7: Re-process after reset */
    surprise_fep_forward_pe(fep_bridge, 0.5f, 0x300);
    StepAll(0.1f);

    surprise_fep_bridge_get_stats(fep_bridge, &fep_stats);
    EXPECT_EQ(fep_stats.pe_events_forwarded, 1u);

    /* Step 8: Destroy handled by TearDown */
}

/* ============================================================================
 * Scenario 5: Mixed Signal Stress Test
 *
 * Rapid mixed signals through all bridges simultaneously.
 * ============================================================================ */

TEST_F(SurpriseBridgesE2ETest, MixedSignalStressTest) {
    CreateFullPipeline();

    for (int i = 0; i < 200; i++) {
        float mag = 0.3f + 0.5f * (float)(i % 7) / 7.0f;

        switch (i % 6) {
            case 0:
                surprise_fep_forward_pe(fep_bridge, mag, 0x100 + i);
                break;
            case 1:
                surprise_fep_forward_bayesian(fep_bridge, mag * 3.0f, 0x200 + i);
                break;
            case 2:
                surprise_amplifier_on_prediction_error(amp, mag, 0x300 + i);
                break;
            case 3:
                surprise_att_apply_boost(att_bridge, mag, mag * 1.5f);
                break;
            case 4:
                surprise_att_request_shift(att_bridge, mag, 0x400 + i);
                break;
            case 5:
                surprise_gw_submit_broadcast(gw_bridge, mag, i, mag * 1.5f, mag * 1.2f);
                break;
        }

        /* Update every 3rd step */
        if (i % 3 == 0) {
            StepAll(0.05f);
        }
    }

    /* Final update */
    StepAll(0.1f);

    /* Verify all systems are still functioning */
    surprise_fep_stats_t fep_stats;
    surprise_fep_bridge_get_stats(fep_bridge, &fep_stats);
    EXPECT_GT(fep_stats.pe_events_forwarded + fep_stats.pe_below_threshold, 0u);
    EXPECT_GT(fep_stats.total_updates, 0u);

    surprise_gw_stats_t gw_stats;
    surprise_gw_bridge_get_stats(gw_bridge, &gw_stats);
    EXPECT_GT(gw_stats.total_updates, 0u);

    surprise_att_stats_t att_stats;
    surprise_att_bridge_get_stats(att_bridge, &att_stats);
    EXPECT_GT(att_stats.attention_boosts, 0u);
    EXPECT_GT(att_stats.total_updates, 0u);

    /* Amplifier should have processed events */
    surprise_amplifier_stats_t amp_stats = surprise_amplifier_get_stats(amp);
    EXPECT_GT(amp_stats.total_surprises, 0u);
}
