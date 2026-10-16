/**
 * @file test_surprise_bridges_integration.cpp
 * @brief Integration tests for Surprise Bridges (Society of Thought Phase 1)
 * @date 2026-01-27
 *
 * WHAT: Integration tests for FEP, GW, and Attention bridges working together
 * WHY:  Verify multi-bridge pipeline: amplifier -> bridges -> downstream effects
 * HOW:  GoogleTest fixture connecting all bridges to a shared amplifier
 *
 * SCENARIOS:
 * 1. Pipeline flow: PE -> amplifier -> all three bridges update
 * 2. FEP precision modulation tracks amplifier level
 * 3. GW broadcast respects cooldown across update cycles
 * 4. Attention boost/shift propagation through bridge update
 * 5. Cross-bridge: surprise level affects all bridges consistently
 * 6. Reset propagation across all bridges
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "cognitive/salience/nimcp_surprise_fep_bridge.h"
#include "cognitive/salience/nimcp_surprise_gw_bridge.h"
#include "cognitive/salience/nimcp_surprise_attention_bridge.h"
}

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class SurpriseBridgesIntegrationTest : public ::testing::Test {
protected:
    surprise_amplifier_t* amp = nullptr;
    surprise_fep_bridge_t* fep_bridge = nullptr;
    surprise_gw_bridge_t* gw_bridge = nullptr;
    surprise_att_bridge_t* att_bridge = nullptr;

    void SetUp() override {
        /* Create amplifier */
        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);
        ASSERT_NE(amp, nullptr);

        /* Create FEP bridge */
        surprise_fep_config_t fep_cfg = surprise_fep_bridge_default_config();
        fep_cfg.enable_bio_async = false;
        fep_cfg.enable_logging = false;
        fep_bridge = surprise_fep_bridge_create(&fep_cfg);
        ASSERT_NE(fep_bridge, nullptr);
        ASSERT_EQ(surprise_fep_bridge_connect_amplifier(fep_bridge, amp), 0);

        /* Create GW bridge */
        surprise_gw_config_t gw_cfg = surprise_gw_bridge_default_config();
        gw_cfg.enable_bio_async = false;
        gw_cfg.enable_logging = false;
        gw_bridge = surprise_gw_bridge_create(&gw_cfg);
        ASSERT_NE(gw_bridge, nullptr);
        ASSERT_EQ(surprise_gw_bridge_connect_amplifier(gw_bridge, amp), 0);

        /* Create Attention bridge */
        surprise_att_config_t att_cfg = surprise_att_bridge_default_config();
        att_cfg.enable_bio_async = false;
        att_cfg.enable_logging = false;
        att_bridge = surprise_att_bridge_create(&att_cfg);
        ASSERT_NE(att_bridge, nullptr);
        ASSERT_EQ(surprise_att_bridge_connect_amplifier(att_bridge, amp), 0);
    }

    void TearDown() override {
        if (att_bridge) { surprise_att_bridge_destroy(att_bridge); att_bridge = nullptr; }
        if (gw_bridge) { surprise_gw_bridge_destroy(gw_bridge); gw_bridge = nullptr; }
        if (fep_bridge) { surprise_fep_bridge_destroy(fep_bridge); fep_bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

/* ============================================================================
 * Pipeline Flow Tests
 * ============================================================================ */

/**
 * WHAT: FEP PE forwarding triggers amplifier, which updates all bridge effects
 * WHY:  This is the primary pipeline: FEP PE -> amplifier -> bridge effects
 * HOW:  Forward PE through FEP bridge, update all bridges, check effects
 */
TEST_F(SurpriseBridgesIntegrationTest, FepPeFlowsThroughPipeline) {
    /* Forward a large PE through the FEP bridge to the amplifier */
    int rc = surprise_fep_forward_pe(fep_bridge, 0.8f, 0x100);
    EXPECT_EQ(rc, 0);

    /* Verify the PE reached the amplifier */
    float amp_level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(amp_level, 0.0f);

    /* Update all bridges - they should reflect the amplifier state */
    surprise_fep_bridge_update(fep_bridge, 0.1f);
    surprise_gw_bridge_update(gw_bridge, 0.1f);
    surprise_att_bridge_update(att_bridge, 0.1f);

    /* FEP bridge should show precision boost */
    surprise_fep_effects_t fep_effects;
    surprise_fep_bridge_get_effects(fep_bridge, &fep_effects);
    EXPECT_GT(fep_effects.current_precision_boost, 1.0f);
    EXPECT_GT(fep_effects.integrated_surprise, 0.0f);

    /* GW bridge sensitivity should be reduced */
    float gw_sens = surprise_gw_get_sensitivity(gw_bridge);
    EXPECT_LT(gw_sens, 1.0f);

    /* Attention bridge sensitivity should be reduced */
    float att_sens = surprise_att_get_sensitivity(att_bridge);
    EXPECT_LT(att_sens, 1.0f);
}

/**
 * WHAT: Bayesian surprise also flows through the FEP bridge to amplifier
 * WHY:  KL divergence is an alternative surprise source
 * HOW:  Forward Bayesian event, verify amplifier responds
 */
TEST_F(SurpriseBridgesIntegrationTest, BayesianSurpriseFlowsToAmplifier) {
    int rc = surprise_fep_forward_bayesian(fep_bridge, 3.0f, 0x200);
    EXPECT_EQ(rc, 0);

    /* Amplifier should have registered the event */
    float amp_level = surprise_amplifier_get_current_level(amp);
    EXPECT_GT(amp_level, 0.0f);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(fep_bridge, &stats);
    EXPECT_EQ(stats.bayesian_events_forwarded, 1u);
}

/* ============================================================================
 * FEP Precision Modulation Integration
 * ============================================================================ */

/**
 * WHAT: FEP precision tracks amplifier surprise level through multiple cycles
 * WHY:  Precision should rise with surprise and fall as surprise decays
 * HOW:  Create high surprise, modulate, decay, modulate again
 */
TEST_F(SurpriseBridgesIntegrationTest, PrecisionTracksSurpriseLevel) {
    /* Spike surprise */
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);
    surprise_fep_bridge_update(fep_bridge, 0.1f);

    float high_precision = surprise_fep_get_precision_boost(fep_bridge);
    EXPECT_GT(high_precision, 1.0f);

    /* Let surprise decay */
    for (int i = 0; i < 50; i++) {
        surprise_amplifier_update(amp, 0.5f);
    }
    surprise_fep_bridge_update(fep_bridge, 0.1f);

    float low_precision = surprise_fep_get_precision_boost(fep_bridge);
    EXPECT_LT(low_precision, high_precision);
}

/* ============================================================================
 * GW Broadcast Integration
 * ============================================================================ */

/**
 * WHAT: GW broadcast with cooldown works across multiple update cycles
 * WHY:  Cooldown should decrement during updates and eventually allow new broadcasts
 * HOW:  Submit, verify cooldown blocks, advance time, submit again
 */
TEST_F(SurpriseBridgesIntegrationTest, GwBroadcastCooldownAcrossCycles) {
    /* First broadcast succeeds */
    int rc = surprise_gw_submit_broadcast(gw_bridge, 0.8f, 1, 0.5f, 0.3f);
    EXPECT_EQ(rc, 0);

    /* Immediate second is blocked by cooldown */
    rc = surprise_gw_submit_broadcast(gw_bridge, 0.9f, 2, 0.6f, 0.4f);
    EXPECT_EQ(rc, 0);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(gw_bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 1u);
    EXPECT_EQ(stats.broadcasts_cooled, 1u);

    /* Advance time in incremental steps past cooldown (1.0s) */
    for (int i = 0; i < 5; i++) {
        surprise_gw_bridge_update(gw_bridge, 0.3f);
    }

    /* Now broadcast should work */
    rc = surprise_gw_submit_broadcast(gw_bridge, 0.9f, 3, 0.6f, 0.4f);
    EXPECT_EQ(rc, 0);

    surprise_gw_bridge_get_stats(gw_bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 2u);
}

/**
 * WHAT: GW pending broadcasts are processed during update
 * WHY:  Broadcasts queue then process in the update cycle
 * HOW:  Submit broadcast, update, verify broadcast_won incremented
 */
TEST_F(SurpriseBridgesIntegrationTest, GwPendingProcessedOnUpdate) {
    surprise_gw_submit_broadcast(gw_bridge, 0.7f, 1, 0.4f, 0.3f);

    surprise_gw_effects_t effects;
    surprise_gw_bridge_get_effects(gw_bridge, &effects);
    EXPECT_TRUE(effects.broadcast_pending);

    surprise_gw_bridge_update(gw_bridge, 0.1f);

    surprise_gw_bridge_get_effects(gw_bridge, &effects);
    EXPECT_FALSE(effects.broadcast_pending);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(gw_bridge, &stats);
    EXPECT_EQ(stats.broadcasts_won, 1u);
}

/* ============================================================================
 * Attention Bridge Integration
 * ============================================================================ */

/**
 * WHAT: Attention boost from amplifier event decays over update cycles
 * WHY:  Boost should fade over time, returning attention to normal
 * HOW:  Apply boost, run update cycles, verify decay
 */
TEST_F(SurpriseBridgesIntegrationTest, AttentionBoostDecaysOverCycles) {
    surprise_att_apply_boost(att_bridge, 0.9f, 2.0f);

    surprise_att_effects_t initial;
    surprise_att_bridge_get_effects(att_bridge, &initial);
    float initial_boost = initial.current_attention_boost;
    EXPECT_GT(initial_boost, 0.0f);

    /* Run multiple update cycles */
    for (int i = 0; i < 30; i++) {
        surprise_att_bridge_update(att_bridge, 0.5f);
    }

    surprise_att_effects_t decayed;
    surprise_att_bridge_get_effects(att_bridge, &decayed);
    EXPECT_LT(decayed.current_attention_boost, initial_boost);
}

/**
 * WHAT: Attention shift is a one-shot event cleared on update
 * WHY:  Shift should only be active for one cycle
 * HOW:  Request shift, verify active, update, verify cleared
 */
TEST_F(SurpriseBridgesIntegrationTest, AttentionShiftOneShotCycle) {
    surprise_att_request_shift(att_bridge, 0.9f, 0x42);

    surprise_att_effects_t effects;
    surprise_att_bridge_get_effects(att_bridge, &effects);
    EXPECT_TRUE(effects.shift_active);
    EXPECT_EQ(effects.shift_target, 0x42u);

    /* First update clears the shift */
    surprise_att_bridge_update(att_bridge, 0.1f);

    surprise_att_bridge_get_effects(att_bridge, &effects);
    EXPECT_FALSE(effects.shift_active);

    /* Second shift request reactivates it */
    surprise_att_request_shift(att_bridge, 0.8f, 0x99);
    surprise_att_bridge_get_effects(att_bridge, &effects);
    EXPECT_TRUE(effects.shift_active);
    EXPECT_EQ(effects.shift_target, 0x99u);
}

/* ============================================================================
 * Cross-Bridge Consistency Tests
 * ============================================================================ */

/**
 * WHAT: All three bridges react consistently to the same amplifier state
 * WHY:  They share one amplifier so their sensitivity should correlate
 * HOW:  Create high surprise, update all bridges, verify all show modulation
 */
TEST_F(SurpriseBridgesIntegrationTest, AllBridgesReactToSameAmplifier) {
    /* Create high surprise */
    surprise_amplifier_on_prediction_error(amp, 0.95f, 0x100);

    /* Update all bridges */
    surprise_fep_bridge_update(fep_bridge, 0.1f);
    surprise_gw_bridge_update(gw_bridge, 0.1f);
    surprise_att_bridge_update(att_bridge, 0.1f);

    /* All should show modulation from surprise */
    float fep_boost = surprise_fep_get_precision_boost(fep_bridge);
    float gw_sens = surprise_gw_get_sensitivity(gw_bridge);
    float att_sens = surprise_att_get_sensitivity(att_bridge);

    EXPECT_GT(fep_boost, 1.0f);   /* Precision boosted above baseline */
    EXPECT_LT(gw_sens, 1.0f);     /* GW sensitivity reduced */
    EXPECT_LT(att_sens, 1.0f);    /* Attention sensitivity reduced */
}

/**
 * WHAT: After amplifier decays to zero, bridges return to baseline
 * WHY:  No surprise = no modulation
 * HOW:  Create surprise, decay to zero, update bridges, verify baseline
 */
TEST_F(SurpriseBridgesIntegrationTest, BridgesReturnToBaselineAfterDecay) {
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);

    /* Decay amplifier to zero */
    for (int i = 0; i < 200; i++) {
        surprise_amplifier_update(amp, 1.0f);
    }
    EXPECT_FLOAT_EQ(surprise_amplifier_get_current_level(amp), 0.0f);

    /* Update all bridges */
    surprise_fep_bridge_update(fep_bridge, 0.1f);
    surprise_gw_bridge_update(gw_bridge, 0.1f);
    surprise_att_bridge_update(att_bridge, 0.1f);

    /* FEP should be at baseline precision */
    float fep_boost = surprise_fep_get_precision_boost(fep_bridge);
    EXPECT_FLOAT_EQ(fep_boost, 1.0f);

    /* GW and Attention should be at full sensitivity */
    float gw_sens = surprise_gw_get_sensitivity(gw_bridge);
    EXPECT_FLOAT_EQ(gw_sens, 1.0f);

    float att_sens = surprise_att_get_sensitivity(att_bridge);
    EXPECT_FLOAT_EQ(att_sens, 1.0f);
}

/* ============================================================================
 * Reset Propagation
 * ============================================================================ */

/**
 * WHAT: Resetting all bridges independently restores each to clean state
 * WHY:  Each bridge should reset without affecting others
 * HOW:  Use bridges, reset each, verify independent clean state
 */
TEST_F(SurpriseBridgesIntegrationTest, IndependentBridgeResets) {
    /* Use all bridges */
    surprise_fep_forward_pe(fep_bridge, 0.8f, 0x100);
    surprise_gw_submit_broadcast(gw_bridge, 0.8f, 1, 0.5f, 0.3f);
    surprise_att_apply_boost(att_bridge, 0.8f, 1.5f);

    /* Reset FEP bridge only */
    surprise_fep_bridge_reset(fep_bridge);

    surprise_fep_stats_t fep_stats;
    surprise_fep_bridge_get_stats(fep_bridge, &fep_stats);
    EXPECT_EQ(fep_stats.pe_events_forwarded, 0u);

    /* GW and Att should still have their stats */
    surprise_gw_stats_t gw_stats;
    surprise_gw_bridge_get_stats(gw_bridge, &gw_stats);
    EXPECT_EQ(gw_stats.broadcasts_submitted, 1u);

    surprise_att_stats_t att_stats;
    surprise_att_bridge_get_stats(att_bridge, &att_stats);
    EXPECT_EQ(att_stats.attention_boosts, 1u);
}
