/**
 * @file test_surprise_bridges.cpp
 * @brief Unit tests for Surprise Amplifier bridges (FEP, GW, Attention)
 * @date 2026-01-27
 *
 * WHAT: Unit tests for all three surprise amplifier bridges
 * WHY:  Verify lifecycle, connections, operations, statistics for each bridge
 * HOW:  GoogleTest with separate test suites per bridge
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
 * Surprise FEP Bridge Tests
 * ============================================================================ */

class SurpriseFepBridgeTest : public ::testing::Test {
protected:
    surprise_fep_bridge_t* bridge = nullptr;
    surprise_amplifier_t* amp = nullptr;

    void SetUp() override {
        surprise_fep_config_t cfg = surprise_fep_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_fep_bridge_create(&cfg);
        ASSERT_NE(bridge, nullptr);

        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);
        ASSERT_NE(amp, nullptr);

        ASSERT_EQ(surprise_fep_bridge_connect_amplifier(bridge, amp), 0);
    }

    void TearDown() override {
        if (bridge) { surprise_fep_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurpriseFepBridgeTest, DefaultConfig) {
    surprise_fep_config_t cfg = surprise_fep_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.precision_gain, 1.5f);
    EXPECT_FLOAT_EQ(cfg.pe_weight, 1.0f);
    EXPECT_FLOAT_EQ(cfg.pe_threshold, 0.2f);
    EXPECT_TRUE(cfg.enable_precision_modulation);
    EXPECT_TRUE(cfg.enable_pe_forwarding);
}

TEST_F(SurpriseFepBridgeTest, CreateWithNullConfig) {
    surprise_fep_bridge_t* b = surprise_fep_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    surprise_fep_bridge_destroy(b);
}

TEST_F(SurpriseFepBridgeTest, DestroyNull) {
    surprise_fep_bridge_destroy(nullptr);  /* Should not crash */
}

TEST_F(SurpriseFepBridgeTest, ForwardPeBelowThreshold) {
    /* PE = 0.1 < threshold 0.2 → should not forward */
    int rc = surprise_fep_forward_pe(bridge, 0.1f, 0x100);
    EXPECT_EQ(rc, 0);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pe_events_forwarded, 0u);
    EXPECT_EQ(stats.pe_below_threshold, 1u);
}

TEST_F(SurpriseFepBridgeTest, ForwardPeAboveThreshold) {
    /* PE = 0.5 > threshold 0.2 → should forward */
    int rc = surprise_fep_forward_pe(bridge, 0.5f, 0x100);
    EXPECT_EQ(rc, 0);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pe_events_forwarded, 1u);
    EXPECT_GT(stats.avg_pe_forwarded, 0.0f);
}

TEST_F(SurpriseFepBridgeTest, ForwardBayesian) {
    int rc = surprise_fep_forward_bayesian(bridge, 2.0f, 0x200);
    EXPECT_EQ(rc, 0);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.bayesian_events_forwarded, 1u);
}

TEST_F(SurpriseFepBridgeTest, ForwardBayesianNegative) {
    int rc = surprise_fep_forward_bayesian(bridge, -1.0f, 0x200);
    EXPECT_EQ(rc, NIMCP_SURPRISE_FEP_ERROR_INVALID_PARAM);
}

TEST_F(SurpriseFepBridgeTest, PrecisionModulation) {
    /* Fire a surprise event to set amplifier level */
    surprise_amplifier_on_prediction_error(amp, 0.8f, 0x100);

    /* Modulate precision */
    int rc = surprise_fep_modulate_precision(bridge);
    EXPECT_EQ(rc, 0);

    float boost = surprise_fep_get_precision_boost(bridge);
    EXPECT_GT(boost, 1.0f);  /* Should be boosted above baseline */
}

TEST_F(SurpriseFepBridgeTest, PrecisionBoostNull) {
    float boost = surprise_fep_get_precision_boost(nullptr);
    EXPECT_FLOAT_EQ(boost, 1.0f);  /* Default on NULL */
}

TEST_F(SurpriseFepBridgeTest, UpdateModulatesPrecision) {
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);

    int rc = surprise_fep_bridge_update(bridge, 0.1f);
    EXPECT_EQ(rc, 0);

    surprise_fep_effects_t effects;
    surprise_fep_bridge_get_effects(bridge, &effects);
    EXPECT_GT(effects.current_precision_boost, 1.0f);
    EXPECT_GT(effects.integrated_surprise, 0.0f);
}

TEST_F(SurpriseFepBridgeTest, Reset) {
    surprise_fep_forward_pe(bridge, 0.5f, 0x100);
    surprise_fep_bridge_update(bridge, 0.1f);

    int rc = surprise_fep_bridge_reset(bridge);
    EXPECT_EQ(rc, 0);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pe_events_forwarded, 0u);
    EXPECT_EQ(stats.total_updates, 0u);

    surprise_fep_effects_t effects;
    surprise_fep_bridge_get_effects(bridge, &effects);
    EXPECT_FLOAT_EQ(effects.current_precision_boost, 1.0f);
}

TEST_F(SurpriseFepBridgeTest, ZeroDtNoOp) {
    int rc = surprise_fep_bridge_update(bridge, 0.0f);
    EXPECT_EQ(rc, 0);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * Surprise GW Bridge Tests
 * ============================================================================ */

class SurpriseGwBridgeTest : public ::testing::Test {
protected:
    surprise_gw_bridge_t* bridge = nullptr;
    surprise_amplifier_t* amp = nullptr;

    void SetUp() override {
        surprise_gw_config_t cfg = surprise_gw_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_gw_bridge_create(&cfg);
        ASSERT_NE(bridge, nullptr);

        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);
        ASSERT_NE(amp, nullptr);

        ASSERT_EQ(surprise_gw_bridge_connect_amplifier(bridge, amp), 0);
    }

    void TearDown() override {
        if (bridge) { surprise_gw_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurpriseGwBridgeTest, DefaultConfig) {
    surprise_gw_config_t cfg = surprise_gw_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.broadcast_threshold, 0.4f);
    EXPECT_FLOAT_EQ(cfg.competition_weight, 2.0f);
    EXPECT_FLOAT_EQ(cfg.cooldown_seconds, 1.0f);
    EXPECT_TRUE(cfg.enable_broadcast);
}

TEST_F(SurpriseGwBridgeTest, CreateWithNullConfig) {
    surprise_gw_bridge_t* b = surprise_gw_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    surprise_gw_bridge_destroy(b);
}

TEST_F(SurpriseGwBridgeTest, DestroyNull) {
    surprise_gw_bridge_destroy(nullptr);  /* Should not crash */
}

TEST_F(SurpriseGwBridgeTest, SubmitBroadcastBelowThreshold) {
    int rc = surprise_gw_submit_broadcast(bridge, 0.2f, 0, 0.1f, 0.1f);
    EXPECT_EQ(rc, 0);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 0u);
    EXPECT_EQ(stats.below_threshold, 1u);
}

TEST_F(SurpriseGwBridgeTest, SubmitBroadcastAboveThreshold) {
    int rc = surprise_gw_submit_broadcast(bridge, 0.8f, 1, 0.5f, 0.3f);
    EXPECT_EQ(rc, 0);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 1u);
    EXPECT_GT(stats.avg_broadcast_magnitude, 0.0f);
}

TEST_F(SurpriseGwBridgeTest, CooldownPreventsRapidBroadcast) {
    surprise_gw_submit_broadcast(bridge, 0.8f, 1, 0.5f, 0.3f);

    /* Second broadcast immediately → cooldown blocks it */
    surprise_gw_submit_broadcast(bridge, 0.9f, 2, 0.6f, 0.4f);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 1u);
    EXPECT_EQ(stats.broadcasts_cooled, 1u);
}

TEST_F(SurpriseGwBridgeTest, CooldownExpiresAfterTime) {
    surprise_gw_submit_broadcast(bridge, 0.8f, 1, 0.5f, 0.3f);

    /* Advance time past cooldown (1.0s default) */
    surprise_gw_bridge_update(bridge, 1.5f);

    /* Now should be able to broadcast again */
    surprise_gw_submit_broadcast(bridge, 0.9f, 2, 0.6f, 0.4f);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 2u);
}

TEST_F(SurpriseGwBridgeTest, UpdateProcessesPending) {
    surprise_gw_submit_broadcast(bridge, 0.8f, 1, 0.5f, 0.3f);

    /* Process pending broadcasts */
    surprise_gw_bridge_update(bridge, 0.1f);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_won, 1u);  /* Standalone mode: all win */
}

TEST_F(SurpriseGwBridgeTest, SensitivityModulation) {
    /* Fire surprise event to raise amplifier level */
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);

    /* Update to compute sensitivity */
    surprise_gw_bridge_update(bridge, 0.1f);

    float sensitivity = surprise_gw_get_sensitivity(bridge);
    EXPECT_LT(sensitivity, 1.0f);  /* Should be reduced when surprise is active */
    EXPECT_GT(sensitivity, 0.0f);
}

TEST_F(SurpriseGwBridgeTest, SensitivityNullReturnsDefault) {
    float sensitivity = surprise_gw_get_sensitivity(nullptr);
    EXPECT_FLOAT_EQ(sensitivity, 1.0f);
}

TEST_F(SurpriseGwBridgeTest, Reset) {
    surprise_gw_submit_broadcast(bridge, 0.8f, 1, 0.5f, 0.3f);
    surprise_gw_bridge_update(bridge, 0.1f);

    int rc = surprise_gw_bridge_reset(bridge);
    EXPECT_EQ(rc, 0);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * Surprise Attention Bridge Tests
 * ============================================================================ */

class SurpriseAttBridgeTest : public ::testing::Test {
protected:
    surprise_att_bridge_t* bridge = nullptr;
    surprise_amplifier_t* amp = nullptr;

    void SetUp() override {
        surprise_att_config_t cfg = surprise_att_bridge_default_config();
        cfg.enable_bio_async = false;
        cfg.enable_logging = false;
        bridge = surprise_att_bridge_create(&cfg);
        ASSERT_NE(bridge, nullptr);

        surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
        amp_cfg.refractory_period_ms = 0;
        amp_cfg.max_concurrent = 256;
        amp_cfg.enable_bio_async = false;
        amp_cfg.enable_logging = false;
        amp = surprise_amplifier_create(&amp_cfg);
        ASSERT_NE(amp, nullptr);

        ASSERT_EQ(surprise_att_bridge_connect_amplifier(bridge, amp), 0);
    }

    void TearDown() override {
        if (bridge) { surprise_att_bridge_destroy(bridge); bridge = nullptr; }
        if (amp) { surprise_amplifier_destroy(amp); amp = nullptr; }
    }
};

TEST_F(SurpriseAttBridgeTest, DefaultConfig) {
    surprise_att_config_t cfg = surprise_att_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.boost_gain, 1.5f);
    EXPECT_FLOAT_EQ(cfg.shift_threshold, 0.6f);
    EXPECT_FLOAT_EQ(cfg.sensitivity_floor, 0.3f);
    EXPECT_TRUE(cfg.enable_attention_boost);
    EXPECT_TRUE(cfg.enable_attention_shift);
}

TEST_F(SurpriseAttBridgeTest, CreateWithNullConfig) {
    surprise_att_bridge_t* b = surprise_att_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    surprise_att_bridge_destroy(b);
}

TEST_F(SurpriseAttBridgeTest, DestroyNull) {
    surprise_att_bridge_destroy(nullptr);  /* Should not crash */
}

TEST_F(SurpriseAttBridgeTest, ApplyBoost) {
    int rc = surprise_att_apply_boost(bridge, 0.8f, 1.0f);
    EXPECT_EQ(rc, 0);

    surprise_att_effects_t effects;
    surprise_att_bridge_get_effects(bridge, &effects);
    EXPECT_GT(effects.current_attention_boost, 0.0f);
}

TEST_F(SurpriseAttBridgeTest, ApplyBoostTakesMax) {
    surprise_att_apply_boost(bridge, 0.5f, 0.5f);
    surprise_att_apply_boost(bridge, 0.8f, 1.0f);

    surprise_att_effects_t effects;
    surprise_att_bridge_get_effects(bridge, &effects);

    /* Second boost was larger, so it should dominate */
    EXPECT_GT(effects.current_attention_boost, 0.75f);
}

TEST_F(SurpriseAttBridgeTest, RequestShiftBelowThreshold) {
    int rc = surprise_att_request_shift(bridge, 0.3f, 0x100);
    EXPECT_EQ(rc, 0);

    surprise_att_stats_t stats;
    surprise_att_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_shifts, 0u);  /* Below threshold */
}

TEST_F(SurpriseAttBridgeTest, RequestShiftAboveThreshold) {
    int rc = surprise_att_request_shift(bridge, 0.9f, 0x100);
    EXPECT_EQ(rc, 0);

    surprise_att_effects_t effects;
    surprise_att_bridge_get_effects(bridge, &effects);
    EXPECT_TRUE(effects.shift_active);
    EXPECT_EQ(effects.shift_target, 0x100u);

    surprise_att_stats_t stats;
    surprise_att_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_shifts, 1u);
}

TEST_F(SurpriseAttBridgeTest, ShiftClearsAfterUpdate) {
    surprise_att_request_shift(bridge, 0.9f, 0x100);

    /* Update clears shift */
    surprise_att_bridge_update(bridge, 0.1f);

    surprise_att_effects_t effects;
    surprise_att_bridge_get_effects(bridge, &effects);
    EXPECT_FALSE(effects.shift_active);
}

TEST_F(SurpriseAttBridgeTest, BoostDecays) {
    surprise_att_apply_boost(bridge, 0.8f, 1.0f);

    surprise_att_effects_t before;
    surprise_att_bridge_get_effects(bridge, &before);

    /* Decay over time */
    for (int i = 0; i < 20; i++) {
        surprise_att_bridge_update(bridge, 0.5f);
    }

    surprise_att_effects_t after;
    surprise_att_bridge_get_effects(bridge, &after);
    EXPECT_LT(after.current_attention_boost, before.current_attention_boost);
}

TEST_F(SurpriseAttBridgeTest, SensitivityModulation) {
    surprise_amplifier_on_prediction_error(amp, 0.9f, 0x100);

    surprise_att_bridge_update(bridge, 0.1f);

    float sensitivity = surprise_att_get_sensitivity(bridge);
    EXPECT_LT(sensitivity, 1.0f);
    EXPECT_GE(sensitivity, 0.3f);  /* Floor */
}

TEST_F(SurpriseAttBridgeTest, SensitivityNullReturnsDefault) {
    float sensitivity = surprise_att_get_sensitivity(nullptr);
    EXPECT_FLOAT_EQ(sensitivity, 1.0f);
}

TEST_F(SurpriseAttBridgeTest, SetChannelSensitivity) {
    int rc = surprise_att_set_channel_sensitivity(bridge, 42, 0.7f);
    EXPECT_EQ(rc, 0);

    /* Set same channel again */
    rc = surprise_att_set_channel_sensitivity(bridge, 42, 0.5f);
    EXPECT_EQ(rc, 0);

    /* Set different channel */
    rc = surprise_att_set_channel_sensitivity(bridge, 99, 0.9f);
    EXPECT_EQ(rc, 0);
}

TEST_F(SurpriseAttBridgeTest, Reset) {
    surprise_att_apply_boost(bridge, 0.8f, 1.0f);
    surprise_att_request_shift(bridge, 0.9f, 0x100);
    surprise_att_bridge_update(bridge, 0.1f);

    int rc = surprise_att_bridge_reset(bridge);
    EXPECT_EQ(rc, 0);

    surprise_att_stats_t stats;
    surprise_att_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_boosts, 0u);
    EXPECT_EQ(stats.attention_shifts, 0u);
    EXPECT_EQ(stats.total_updates, 0u);

    surprise_att_effects_t effects;
    surprise_att_bridge_get_effects(bridge, &effects);
    EXPECT_FLOAT_EQ(effects.current_attention_boost, 0.0f);
    EXPECT_FLOAT_EQ(effects.current_sensitivity, 1.0f);
}

TEST_F(SurpriseAttBridgeTest, ZeroDtNoOp) {
    int rc = surprise_att_bridge_update(bridge, 0.0f);
    EXPECT_EQ(rc, 0);

    surprise_att_stats_t stats;
    surprise_att_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * Null Safety Tests (all bridges)
 * ============================================================================ */

TEST(SurpriseBridgesNullSafety, FepForwardPeNull) {
    int rc = surprise_fep_forward_pe(nullptr, 0.5f, 0x100);
    EXPECT_EQ(rc, -1);  /* FEP bridges return -1 for errors */
}

TEST(SurpriseBridgesNullSafety, FepModulatePrecisionNull) {
    int rc = surprise_fep_modulate_precision(nullptr);
    EXPECT_EQ(rc, -1);  /* FEP bridges return -1 for errors */
}

TEST(SurpriseBridgesNullSafety, GwSubmitBroadcastNull) {
    int rc = surprise_gw_submit_broadcast(nullptr, 0.8f, 0, 0.5f, 0.3f);
    EXPECT_EQ(rc, NIMCP_SURPRISE_GW_ERROR_NULL_POINTER);
}

TEST(SurpriseBridgesNullSafety, AttApplyBoostNull) {
    int rc = surprise_att_apply_boost(nullptr, 0.8f, 1.0f);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER);
}

TEST(SurpriseBridgesNullSafety, AttRequestShiftNull) {
    int rc = surprise_att_request_shift(nullptr, 0.9f, 0x100);
    EXPECT_EQ(rc, NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER);
}
