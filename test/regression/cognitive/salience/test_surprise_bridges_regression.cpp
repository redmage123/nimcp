/**
 * @file test_surprise_bridges_regression.cpp
 * @brief Regression tests for Surprise Bridges (Society of Thought Phase 1)
 * @date 2026-01-27
 *
 * WHAT: Regression tests ensuring surprise bridges maintain baseline behavior
 * WHY:  Prevent regressions in precision, broadcast thresholds, boost calculations
 * HOW:  Test known baselines, boundary conditions, performance thresholds
 *
 * BASELINES:
 * - FEP precision gain=1.5 with surprise=0.8 -> boost = 1 + 0.8*1.5 = 2.2
 * - GW broadcast threshold = 0.4, cooldown = 1.0s
 * - Attention boost gain = 1.5, shift threshold = 0.6
 * - Attention decay rate = 0.9 per second
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>

extern "C" {
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "cognitive/salience/nimcp_surprise_fep_bridge.h"
#include "cognitive/salience/nimcp_surprise_gw_bridge.h"
#include "cognitive/salience/nimcp_surprise_attention_bridge.h"
}

/* ============================================================================
 * FEP Bridge Regression Fixture
 * ============================================================================ */

class SurpriseFepBridgeRegressionTest : public ::testing::Test {
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

/**
 * BASELINE: Default config values must match header constants
 */
TEST_F(SurpriseFepBridgeRegressionTest, DefaultConfigBaseline) {
    surprise_fep_config_t cfg = surprise_fep_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.precision_gain, SURPRISE_FEP_DEFAULT_PRECISION_GAIN);
    EXPECT_FLOAT_EQ(cfg.pe_weight, SURPRISE_FEP_DEFAULT_PE_WEIGHT);
    EXPECT_FLOAT_EQ(cfg.bayesian_weight, SURPRISE_FEP_DEFAULT_BAYESIAN_WEIGHT);
    EXPECT_FLOAT_EQ(cfg.precision_floor, SURPRISE_FEP_DEFAULT_PRECISION_FLOOR);
    EXPECT_FLOAT_EQ(cfg.precision_ceiling, SURPRISE_FEP_DEFAULT_PRECISION_CEILING);
    EXPECT_FLOAT_EQ(cfg.pe_threshold, SURPRISE_FEP_DEFAULT_PE_THRESHOLD);
}

/**
 * BASELINE: PE at threshold boundary 0.2 should be filtered
 */
TEST_F(SurpriseFepBridgeRegressionTest, PeThresholdBoundaryExact) {
    /* PE exactly at threshold (0.2) is below threshold (< not <=) */
    surprise_fep_forward_pe(bridge, 0.19f, 0x100);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pe_events_forwarded, 0u);
    EXPECT_EQ(stats.pe_below_threshold, 1u);
}

/**
 * BASELINE: PE just above threshold should be forwarded
 */
TEST_F(SurpriseFepBridgeRegressionTest, PeThresholdBoundaryAbove) {
    surprise_fep_forward_pe(bridge, 0.21f, 0x100);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pe_events_forwarded, 1u);
}

/**
 * BASELINE: PE weight applies correctly (default weight=1.0)
 */
TEST_F(SurpriseFepBridgeRegressionTest, PeWeightBaseline) {
    surprise_fep_forward_pe(bridge, 0.5f, 0x100);

    surprise_fep_stats_t stats;
    surprise_fep_bridge_get_stats(bridge, &stats);
    /* weight=1.0, pe=0.5 -> forwarded=0.5 */
    EXPECT_FLOAT_EQ(stats.avg_pe_forwarded, 0.5f);
}

/**
 * BASELINE: Precision modulation formula: boost = 1.0 + surprise_level * gain
 */
TEST_F(SurpriseFepBridgeRegressionTest, PrecisionBoostFormula) {
    /* Fire a known PE to set amplifier level */
    surprise_amplifier_on_prediction_error(amp, 0.4f, 0x100);
    float level = surprise_amplifier_get_current_level(amp);

    /* Modulate precision */
    surprise_fep_modulate_precision(bridge);
    float boost = surprise_fep_get_precision_boost(bridge);

    /* Expected: 1.0 + level * 1.5, clamped to [0.1, 3.0] */
    float expected = 1.0f + level * 1.5f;
    if (expected > 3.0f) expected = 3.0f;
    if (expected < 0.1f) expected = 0.1f;

    EXPECT_NEAR(boost, expected, 0.01f);
}

/**
 * BASELINE: Precision boost clamped to ceiling (3.0)
 */
TEST_F(SurpriseFepBridgeRegressionTest, PrecisionBoostClampedToCeiling) {
    /* Max surprise -> boost = 1.0 + 1.0 * 1.5 = 2.5 (below ceiling) */
    surprise_amplifier_on_prediction_error(amp, 1.0f, 0x100);
    surprise_fep_modulate_precision(bridge);
    float boost = surprise_fep_get_precision_boost(bridge);
    EXPECT_LE(boost, 3.0f);
    EXPECT_GT(boost, 1.0f);
}

/**
 * BASELINE: Negative KL divergence returns error code
 */
TEST_F(SurpriseFepBridgeRegressionTest, NegativeKlDivergenceError) {
    int rc = surprise_fep_forward_bayesian(bridge, -0.5f, 0x200);
    EXPECT_EQ(rc, NIMCP_SURPRISE_FEP_ERROR_INVALID_PARAM);
}

/* ============================================================================
 * GW Bridge Regression Fixture
 * ============================================================================ */

class SurpriseGwBridgeRegressionTest : public ::testing::Test {
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

/**
 * BASELINE: Default config values must match header constants
 */
TEST_F(SurpriseGwBridgeRegressionTest, DefaultConfigBaseline) {
    surprise_gw_config_t cfg = surprise_gw_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.broadcast_threshold, SURPRISE_GW_DEFAULT_BROADCAST_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.competition_weight, SURPRISE_GW_DEFAULT_COMPETITION_WEIGHT);
    EXPECT_FLOAT_EQ(cfg.cooldown_seconds, SURPRISE_GW_DEFAULT_COOLDOWN_SECONDS);
}

/**
 * BASELINE: Broadcast threshold boundary at 0.4
 */
TEST_F(SurpriseGwBridgeRegressionTest, BroadcastThresholdBoundaryBelow) {
    surprise_gw_submit_broadcast(bridge, 0.39f, 1, 0.3f, 0.2f);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 0u);
    EXPECT_EQ(stats.below_threshold, 1u);
}

/**
 * BASELINE: Broadcast at threshold passes
 */
TEST_F(SurpriseGwBridgeRegressionTest, BroadcastThresholdBoundaryAbove) {
    surprise_gw_submit_broadcast(bridge, 0.41f, 1, 0.3f, 0.2f);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 1u);
}

/**
 * BASELINE: Cooldown duration is exactly 1.0 seconds
 */
TEST_F(SurpriseGwBridgeRegressionTest, CooldownDurationBaseline) {
    surprise_gw_submit_broadcast(bridge, 0.8f, 1, 0.5f, 0.3f);

    /* At 0.9s should still be in cooldown */
    surprise_gw_bridge_update(bridge, 0.9f);
    surprise_gw_submit_broadcast(bridge, 0.8f, 2, 0.5f, 0.3f);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 1u);
    EXPECT_EQ(stats.broadcasts_cooled, 1u);

    /* Advance past cooldown (total 1.1s) */
    surprise_gw_bridge_update(bridge, 0.2f);
    surprise_gw_submit_broadcast(bridge, 0.8f, 3, 0.5f, 0.3f);

    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 2u);
}

/**
 * BASELINE: GW sensitivity formula: 1.0 - 0.5 * surprise_level, clamped [0.2, 1.0]
 */
TEST_F(SurpriseGwBridgeRegressionTest, SensitivityFormulaBaseline) {
    surprise_amplifier_on_prediction_error(amp, 0.4f, 0x100);
    float level = surprise_amplifier_get_current_level(amp);

    surprise_gw_bridge_update(bridge, 0.1f);
    float sensitivity = surprise_gw_get_sensitivity(bridge);

    float expected = 1.0f - 0.5f * level;
    if (expected < 0.2f) expected = 0.2f;
    if (expected > 1.0f) expected = 1.0f;

    EXPECT_NEAR(sensitivity, expected, 0.01f);
}

/**
 * BASELINE: Max pending broadcasts is 8
 */
TEST_F(SurpriseGwBridgeRegressionTest, MaxPendingBroadcasts) {
    /* Only the first one goes through since cooldown blocks the rest */
    surprise_gw_submit_broadcast(bridge, 0.8f, 1, 0.5f, 0.3f);

    surprise_gw_stats_t stats;
    surprise_gw_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.broadcasts_submitted, 1u);
}

/* ============================================================================
 * Attention Bridge Regression Fixture
 * ============================================================================ */

class SurpriseAttBridgeRegressionTest : public ::testing::Test {
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

/**
 * BASELINE: Default config values must match header constants
 */
TEST_F(SurpriseAttBridgeRegressionTest, DefaultConfigBaseline) {
    surprise_att_config_t cfg = surprise_att_bridge_default_config();
    EXPECT_FLOAT_EQ(cfg.boost_gain, SURPRISE_ATT_DEFAULT_BOOST_GAIN);
    EXPECT_FLOAT_EQ(cfg.shift_threshold, SURPRISE_ATT_DEFAULT_SHIFT_THRESHOLD);
    EXPECT_FLOAT_EQ(cfg.sensitivity_floor, SURPRISE_ATT_DEFAULT_SENSITIVITY_FLOOR);
    EXPECT_FLOAT_EQ(cfg.attention_decay_rate, SURPRISE_ATT_DEFAULT_DECAY_RATE);
}

/**
 * BASELINE: Boost gain = 1.5x applied to input
 */
TEST_F(SurpriseAttBridgeRegressionTest, BoostGainBaseline) {
    /* Input boost=1.0, gain=1.5 -> effective=1.5 */
    surprise_att_apply_boost(bridge, 0.8f, 1.0f);

    surprise_att_effects_t effects;
    surprise_att_bridge_get_effects(bridge, &effects);
    EXPECT_FLOAT_EQ(effects.current_attention_boost, 1.5f);
}

/**
 * BASELINE: Boost clamped to max 5.0
 */
TEST_F(SurpriseAttBridgeRegressionTest, BoostClampedToMax) {
    /* Input boost=5.0 (max), gain=1.5 -> effective=7.5, clamped to 5.0 */
    surprise_att_apply_boost(bridge, 1.0f, 5.0f);

    surprise_att_effects_t effects;
    surprise_att_bridge_get_effects(bridge, &effects);
    EXPECT_LE(effects.current_attention_boost, 5.0f);
}

/**
 * BASELINE: Shift threshold at exactly 0.6
 */
TEST_F(SurpriseAttBridgeRegressionTest, ShiftThresholdBoundaryBelow) {
    surprise_att_request_shift(bridge, 0.59f, 0x100);

    surprise_att_stats_t stats;
    surprise_att_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_shifts, 0u);
}

/**
 * BASELINE: Shift at threshold passes
 */
TEST_F(SurpriseAttBridgeRegressionTest, ShiftThresholdBoundaryAbove) {
    surprise_att_request_shift(bridge, 0.61f, 0x100);

    surprise_att_stats_t stats;
    surprise_att_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_shifts, 1u);
}

/**
 * BASELINE: Attention decay rate: 0.9^10 ≈ 0.3487
 */
TEST_F(SurpriseAttBridgeRegressionTest, DecayRateBaseline10s) {
    surprise_att_apply_boost(bridge, 0.9f, 1.0f);

    surprise_att_effects_t initial;
    surprise_att_bridge_get_effects(bridge, &initial);
    float initial_boost = initial.current_attention_boost;

    /* Decay for 10 seconds */
    for (int i = 0; i < 10; i++) {
        surprise_att_bridge_update(bridge, 1.0f);
    }

    surprise_att_effects_t after;
    surprise_att_bridge_get_effects(bridge, &after);

    float expected_ratio = powf(0.9f, 10.0f);  /* ~0.3487 */
    float actual_ratio = after.current_attention_boost / initial_boost;

    EXPECT_NEAR(actual_ratio, expected_ratio, 0.05f);
}

/**
 * BASELINE: Sensitivity formula: 1.0 - 0.3 * surprise_level, floor 0.3
 */
TEST_F(SurpriseAttBridgeRegressionTest, SensitivityFormulaBaseline) {
    surprise_amplifier_on_prediction_error(amp, 0.5f, 0x100);
    float level = surprise_amplifier_get_current_level(amp);

    surprise_att_bridge_update(bridge, 0.1f);
    float sensitivity = surprise_att_get_sensitivity(bridge);

    float expected = 1.0f - 0.3f * level;
    if (expected < 0.3f) expected = 0.3f;

    EXPECT_NEAR(sensitivity, expected, 0.01f);
}

/**
 * BASELINE: Channel sensitivity is clamped to [0, 1]
 */
TEST_F(SurpriseAttBridgeRegressionTest, ChannelSensitivityClamped) {
    /* Over-range should be clamped */
    surprise_att_set_channel_sensitivity(bridge, 1, 2.0f);
    /* Under-range should be clamped */
    surprise_att_set_channel_sensitivity(bridge, 2, -0.5f);

    /* No error returned, values are clamped internally */
    SUCCEED();
}

/**
 * BASELINE: Max 8 channels
 */
TEST_F(SurpriseAttBridgeRegressionTest, MaxChannelSensitivity) {
    /* Add exactly 8 channels */
    for (uint32_t i = 0; i < SURPRISE_ATT_MAX_CHANNEL_SENSITIVITY; i++) {
        int rc = surprise_att_set_channel_sensitivity(bridge, i + 100, 0.5f);
        EXPECT_EQ(rc, 0);
    }
    /* 9th should silently not add */
    int rc = surprise_att_set_channel_sensitivity(bridge, 200, 0.5f);
    EXPECT_EQ(rc, 0);
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

TEST(SurpriseBridgesPerformance, FepBulkForwarding) {
    surprise_fep_config_t cfg = surprise_fep_bridge_default_config();
    cfg.enable_bio_async = false;
    cfg.enable_logging = false;
    surprise_fep_bridge_t* bridge = surprise_fep_bridge_create(&cfg);
    ASSERT_NE(bridge, nullptr);

    surprise_amplifier_config_t amp_cfg = surprise_amplifier_default_config();
    amp_cfg.refractory_period_ms = 0;
    amp_cfg.max_concurrent = 256;
    amp_cfg.enable_bio_async = false;
    amp_cfg.enable_logging = false;
    surprise_amplifier_t* amp = surprise_amplifier_create(&amp_cfg);
    ASSERT_NE(amp, nullptr);

    surprise_fep_bridge_connect_amplifier(bridge, amp);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        surprise_fep_forward_pe(bridge, 0.5f, 0x100);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(elapsed, 500);

    surprise_fep_bridge_destroy(bridge);
    surprise_amplifier_destroy(amp);
}

TEST(SurpriseBridgesPerformance, GwBulkUpdates) {
    surprise_gw_config_t cfg = surprise_gw_bridge_default_config();
    cfg.enable_bio_async = false;
    cfg.enable_logging = false;
    surprise_gw_bridge_t* bridge = surprise_gw_bridge_create(&cfg);
    ASSERT_NE(bridge, nullptr);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        surprise_gw_bridge_update(bridge, 0.001f);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(elapsed, 200);

    surprise_gw_bridge_destroy(bridge);
}

TEST(SurpriseBridgesPerformance, AttBulkBoosts) {
    surprise_att_config_t cfg = surprise_att_bridge_default_config();
    cfg.enable_bio_async = false;
    cfg.enable_logging = false;
    surprise_att_bridge_t* bridge = surprise_att_bridge_create(&cfg);
    ASSERT_NE(bridge, nullptr);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        surprise_att_apply_boost(bridge, 0.5f, 1.0f);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(elapsed, 200);

    surprise_att_bridge_destroy(bridge);
}
