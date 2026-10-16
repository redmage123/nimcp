/**
 * @file test_neuromod_emotion_bridge.cpp
 * @brief Unit tests for Neuromodulatory-Emotion Inter-Layer Bridge
 *
 * WHAT: Test suite for neuromod_emotion_bridge
 * WHY:  Verify correct NE→arousal, DA→valence, 5-HT→regulation, Hab→aversion
 * HOW:  Unit tests for lifecycle, modulation, state classification, feedback
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "integration/inter/neuromod_emotion/nimcp_neuromod_emotion_bridge.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodEmotionBridgeTest : public ::testing::Test {
protected:
    neuromod_emotion_bridge_t* bridge = nullptr;

    void SetUp() override {
        neuromod_emotion_config_t config = neuromod_emotion_default_config();
        bridge = neuromod_emotion_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            neuromod_emotion_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(NeuromodEmotionCreateTest, CreateWithDefaultConfig) {
    neuromod_emotion_bridge_t* br = neuromod_emotion_create(nullptr);
    ASSERT_NE(br, nullptr);
    EXPECT_TRUE(neuromod_emotion_is_connected(br));
    neuromod_emotion_destroy(br);
}

TEST(NeuromodEmotionCreateTest, CreateWithCustomConfig) {
    neuromod_emotion_config_t config = neuromod_emotion_default_config();
    config.ne_arousal_coupling = 0.9f;
    config.da_valence_coupling = 0.8f;
    config.ht_regulation_coupling = 0.7f;
    config.enable_state_classification = true;

    neuromod_emotion_bridge_t* br = neuromod_emotion_create(&config);
    ASSERT_NE(br, nullptr);
    neuromod_emotion_destroy(br);
}

TEST(NeuromodEmotionCreateTest, DestroyNull) {
    neuromod_emotion_destroy(nullptr);
}

//=============================================================================
// NE-Arousal Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, ApplyNEArousalLow) {
    float arousal_out;
    int ret = neuromod_emotion_apply_ne_arousal(bridge, 0.2f, &arousal_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(arousal_out, 0.0f);
    EXPECT_LT(arousal_out, 0.5f);
}

TEST_F(NeuromodEmotionBridgeTest, ApplyNEArousalHigh) {
    float arousal_out;
    int ret = neuromod_emotion_apply_ne_arousal(bridge, 0.9f, &arousal_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(arousal_out, 0.5f);
}

//=============================================================================
// DA-Valence Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, ApplyDAValenceLow) {
    float valence_out;
    /* Low DA should produce negative valence */
    int ret = neuromod_emotion_apply_da_valence(bridge, 0.1f, &valence_out);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(valence_out, 0.0f);
}

TEST_F(NeuromodEmotionBridgeTest, ApplyDAValenceHigh) {
    float valence_out;
    /* High DA should produce positive valence */
    int ret = neuromod_emotion_apply_da_valence(bridge, 0.9f, &valence_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(valence_out, 0.0f);
}

TEST_F(NeuromodEmotionBridgeTest, ApplyDAValenceNeutral) {
    float valence_out;
    /* DA around 0.4 should be near neutral */
    int ret = neuromod_emotion_apply_da_valence(bridge, 0.4f, &valence_out);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(valence_out, 0.0f, 0.2f);
}

//=============================================================================
// 5-HT Regulation Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, ApplyHTRegulationLow) {
    float regulation_out;
    int ret = neuromod_emotion_apply_ht_regulation(bridge, 0.2f, &regulation_out);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(regulation_out, 0.5f);
}

TEST_F(NeuromodEmotionBridgeTest, ApplyHTRegulationHigh) {
    float regulation_out;
    int ret = neuromod_emotion_apply_ht_regulation(bridge, 0.8f, &regulation_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(regulation_out, 0.5f);
}

//=============================================================================
// Habenula Aversion Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, ApplyHabAversionLow) {
    float aversion_out;
    int ret = neuromod_emotion_apply_hab_aversion(bridge, 0.2f, &aversion_out);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(aversion_out, 0.3f);
}

TEST_F(NeuromodEmotionBridgeTest, ApplyHabAversionHigh) {
    float aversion_out;
    int ret = neuromod_emotion_apply_hab_aversion(bridge, 0.9f, &aversion_out);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(aversion_out, 0.3f);
}

//=============================================================================
// State Classification Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, ClassifyStateNeutral) {
    neuromod_emotion_compute_modulation(bridge, 0.5f, 0.5f, 0.5f, 0.2f, nullptr);
    emotional_state_t state = neuromod_emotion_classify_state(bridge);
    /* Should be neutral or balanced */
    EXPECT_TRUE(state == EMOTION_STATE_NEUTRAL ||
                state == EMOTION_STATE_POSITIVE_LOW ||
                state == EMOTION_STATE_NEGATIVE_LOW);
}

TEST_F(NeuromodEmotionBridgeTest, ClassifyStateAnxious) {
    /* High NE + low DA = anxious */
    neuromod_emotion_compute_modulation(bridge, 0.9f, 0.1f, 0.3f, 0.2f, nullptr);
    emotional_state_t state = neuromod_emotion_classify_state(bridge);
    EXPECT_TRUE(state == EMOTION_STATE_ANXIOUS ||
                state == EMOTION_STATE_NEGATIVE_HIGH);
}

TEST_F(NeuromodEmotionBridgeTest, ClassifyStatePositive) {
    /* Low NE + high DA = positive */
    neuromod_emotion_compute_modulation(bridge, 0.4f, 0.9f, 0.5f, 0.1f, nullptr);
    emotional_state_t state = neuromod_emotion_classify_state(bridge);
    EXPECT_TRUE(state == EMOTION_STATE_POSITIVE_LOW ||
                state == EMOTION_STATE_POSITIVE_HIGH);
}

TEST_F(NeuromodEmotionBridgeTest, StateNameMapping) {
    EXPECT_STREQ(neuromod_emotion_state_name(EMOTION_STATE_NEUTRAL), "Neutral");
    EXPECT_STREQ(neuromod_emotion_state_name(EMOTION_STATE_ANXIOUS), "Anxious");
    EXPECT_STREQ(neuromod_emotion_state_name(EMOTION_STATE_POSITIVE_HIGH), "Excited");
}

//=============================================================================
// Top-Down Feedback Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, ReportFear) {
    float lc_trigger;
    int ret = neuromod_emotion_report_fear(bridge, 0.8f, &lc_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(lc_trigger, 0.0f);
}

TEST_F(NeuromodEmotionBridgeTest, ReportRewardAnticipation) {
    float vta_trigger;
    int ret = neuromod_emotion_report_reward_anticipation(bridge, 0.7f, &vta_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(vta_trigger, 0.0f);
}

TEST_F(NeuromodEmotionBridgeTest, ReportConflict) {
    float ht_demand;
    int ret = neuromod_emotion_report_conflict(bridge, 0.6f, &ht_demand);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(ht_demand, 0.0f);
}

TEST_F(NeuromodEmotionBridgeTest, ReportDisappointment) {
    float hab_trigger;
    int ret = neuromod_emotion_report_disappointment(bridge, 0.7f, &hab_trigger);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(hab_trigger, 0.0f);
}

//=============================================================================
// Unified Modulation Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, ComputeModulation) {
    neuromod_emotion_state_t state;
    int ret = neuromod_emotion_compute_modulation(bridge, 0.5f, 0.5f, 0.5f, 0.3f, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.arousal_level, 0.0f);
    EXPECT_LE(state.arousal_level, 1.0f);
    EXPECT_GE(state.valence_level, -1.0f);
    EXPECT_LE(state.valence_level, 1.0f);
    EXPECT_GE(state.emotional_stability, 0.0f);
    EXPECT_LE(state.emotional_stability, 1.0f);
}

//=============================================================================
// Update and State Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, Update) {
    int ret = neuromod_emotion_update(bridge, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodEmotionBridgeTest, GetState) {
    neuromod_emotion_state_t state;
    int ret = neuromod_emotion_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(NeuromodEmotionBridgeTest, GetStats) {
    neuromod_emotion_apply_ne_arousal(bridge, 0.5f, nullptr);
    neuromod_emotion_report_fear(bridge, 0.6f, nullptr);

    neuromod_emotion_stats_t stats;
    int ret = neuromod_emotion_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.arousal_modulations, 0u);
}

TEST_F(NeuromodEmotionBridgeTest, ResetStats) {
    neuromod_emotion_apply_ne_arousal(bridge, 0.5f, nullptr);

    int ret = neuromod_emotion_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    neuromod_emotion_stats_t stats;
    neuromod_emotion_get_stats(bridge, &stats);
    EXPECT_EQ(stats.arousal_modulations, 0u);
}

//=============================================================================
// Diagnostics Tests
//=============================================================================

TEST_F(NeuromodEmotionBridgeTest, IsConnected) {
    EXPECT_TRUE(neuromod_emotion_is_connected(bridge));
    EXPECT_FALSE(neuromod_emotion_is_connected(nullptr));
}

TEST_F(NeuromodEmotionBridgeTest, GetStability) {
    float stability = neuromod_emotion_get_stability(bridge);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(NeuromodEmotionBridgeTest, GetCoherence) {
    float coherence = neuromod_emotion_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(NeuromodEmotionBridgeTest, PrintSummary) {
    neuromod_emotion_print_summary(bridge);
    neuromod_emotion_print_summary(nullptr);
}
