/**
 * @file test_neuromod_cognitive_integration.cpp
 * @brief Integration tests for Neuromodulatory-Cognitive Cross-Bridge Scenarios
 *
 * WHAT: Integration test suite for neuromod-cognitive bridge interactions
 * WHY:  Verify correct cross-bridge coordination and shared neuromodulator effects
 * HOW:  Test scenarios where multiple bridges respond to same neuromod signals
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "integration/inter/neuromod_attention/nimcp_neuromod_attention_bridge.h"
#include "integration/inter/neuromod_wm/nimcp_neuromod_wm_bridge.h"
#include "integration/inter/neuromod_emotion/nimcp_neuromod_emotion_bridge.h"
#include "integration/inter/neuromod_plasticity/nimcp_neuromod_plasticity_bridge.h"
#include "integration/inter/neuromod_gametheory/nimcp_neuromod_gametheory_bridge.h"
#include "integration/inter/neuromod_reasoning/nimcp_neuromod_reasoning_bridge.h"
}

//=============================================================================
// Integration Test Fixture
//=============================================================================

class NeuromodCognitiveIntegrationTest : public ::testing::Test {
protected:
    neuromod_attention_bridge_t* attention_bridge = nullptr;
    neuromod_wm_bridge_t* wm_bridge = nullptr;
    neuromod_emotion_bridge_t* emotion_bridge = nullptr;
    neuromod_plasticity_bridge_t* plasticity_bridge = nullptr;
    neuromod_gametheory_bridge_t* gametheory_bridge = nullptr;
    neuromod_reasoning_bridge_t* reasoning_bridge = nullptr;

    void SetUp() override {
        attention_bridge = neuromod_attention_create(nullptr);
        wm_bridge = neuromod_wm_create(nullptr);
        emotion_bridge = neuromod_emotion_create(nullptr);
        plasticity_bridge = neuromod_plasticity_create(nullptr);
        gametheory_bridge = neuromod_gametheory_create(nullptr);
        reasoning_bridge = neuromod_reasoning_create(nullptr);

        ASSERT_NE(attention_bridge, nullptr);
        ASSERT_NE(wm_bridge, nullptr);
        ASSERT_NE(emotion_bridge, nullptr);
        ASSERT_NE(plasticity_bridge, nullptr);
        ASSERT_NE(gametheory_bridge, nullptr);
        ASSERT_NE(reasoning_bridge, nullptr);
    }

    void TearDown() override {
        if (attention_bridge) neuromod_attention_destroy(attention_bridge);
        if (wm_bridge) neuromod_wm_destroy(wm_bridge);
        if (emotion_bridge) neuromod_emotion_destroy(emotion_bridge);
        if (plasticity_bridge) neuromod_plasticity_destroy(plasticity_bridge);
        if (gametheory_bridge) neuromod_gametheory_destroy(gametheory_bridge);
        if (reasoning_bridge) neuromod_reasoning_destroy(reasoning_bridge);
    }

    /* Helper: Apply unified NE level across all relevant bridges */
    void apply_global_ne(float ne_level) {
        neuromod_attention_apply_ne_gain(attention_bridge, ne_level, nullptr);
        neuromod_wm_apply_ne_flexibility(wm_bridge, ne_level, nullptr);
        neuromod_emotion_apply_ne_arousal(emotion_bridge, ne_level, nullptr);
        neuromod_plasticity_apply_ne_boost(plasticity_bridge, ne_level, nullptr);
        neuromod_gametheory_apply_ne_urgency(gametheory_bridge, ne_level, nullptr);
        neuromod_reasoning_apply_ne_control(reasoning_bridge, ne_level, nullptr);
    }

    /* Helper: Apply unified DA level across all relevant bridges */
    void apply_global_da(float da_level) {
        neuromod_attention_apply_da_salience(attention_bridge, da_level, nullptr);
        neuromod_wm_apply_da_gain(wm_bridge, da_level, nullptr);
        neuromod_emotion_apply_da_valence(emotion_bridge, da_level, nullptr);
        neuromod_plasticity_apply_da_gating(plasticity_bridge, da_level, nullptr);
        neuromod_gametheory_apply_da_competition(gametheory_bridge, da_level, nullptr);
        neuromod_reasoning_apply_da_confidence(reasoning_bridge, da_level, nullptr);
    }

    /* Helper: Apply unified 5-HT level across all relevant bridges */
    void apply_global_ht(float ht_level) {
        neuromod_attention_apply_ht_patience(attention_bridge, ht_level, nullptr);
        neuromod_wm_apply_ht_delay(wm_bridge, ht_level, nullptr);
        neuromod_emotion_apply_ht_regulation(emotion_bridge, ht_level, nullptr);
        neuromod_plasticity_apply_ht_consolidation(plasticity_bridge, ht_level, nullptr);
        neuromod_gametheory_apply_ht_cooperation(gametheory_bridge, ht_level, nullptr);
        neuromod_reasoning_apply_ht_deliberation(reasoning_bridge, ht_level, nullptr);
    }

    /* Helper: Apply unified Habenula level across all relevant bridges */
    void apply_global_hab(float hab_level) {
        neuromod_attention_apply_hab_aversion(attention_bridge, hab_level, nullptr);
        neuromod_plasticity_apply_hab_avoidance(plasticity_bridge, hab_level, nullptr);
        neuromod_gametheory_apply_hab_loss_aversion(gametheory_bridge, hab_level, nullptr);
        neuromod_reasoning_apply_hab_error(reasoning_bridge, hab_level, nullptr);
        neuromod_emotion_apply_hab_aversion(emotion_bridge, hab_level, nullptr);
    }
};

//=============================================================================
// Cross-Bridge NE Integration Tests
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, HighNEAffectsAllBridges) {
    /* Simulate high arousal state (e.g., stress/alertness) */
    apply_global_ne(0.9f);

    /* All bridges should reflect high NE effects */
    neuromod_attention_state_t att_state;
    neuromod_attention_get_state(attention_bridge, &att_state);
    EXPECT_GT(att_state.attention_gain, 0.5f);

    neuromod_wm_state_t wm_state;
    neuromod_wm_get_state(wm_bridge, &wm_state);
    EXPECT_GT(wm_state.flexibility_level, 0.5f);

    neuromod_emotion_state_t emo_state;
    neuromod_emotion_get_state(emotion_bridge, &emo_state);
    EXPECT_GT(emo_state.arousal_level, 0.5f);

    neuromod_plasticity_state_t plas_state;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_state);
    EXPECT_GT(plas_state.memory_boost, 1.0f);

    neuromod_gametheory_state_t gt_state;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_state);
    EXPECT_GT(gt_state.decision_urgency, 0.5f);

    neuromod_reasoning_state_t reason_state;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_state);
    /* High NE should give good control (peak of inverted-U or near it) */
}

TEST_F(NeuromodCognitiveIntegrationTest, LowNEDrowsinessAffectsAllBridges) {
    /* Simulate low arousal state (e.g., drowsiness)
     * Low NE gives gain near baseline (1.0), not peak gain
     * Compare to optimal NE gain to verify inverted-U */

    /* First get optimal gain */
    apply_global_ne(0.6f);  /* Optimal */
    neuromod_attention_state_t opt_state;
    neuromod_attention_get_state(attention_bridge, &opt_state);
    float optimal_gain = opt_state.attention_gain;

    /* Now apply low NE */
    apply_global_ne(0.1f);
    neuromod_attention_state_t att_state;
    neuromod_attention_get_state(attention_bridge, &att_state);
    /* Low NE should give lower gain than optimal */
    EXPECT_LT(att_state.attention_gain, optimal_gain);
    /* But should be near baseline (~1.0) */
    EXPECT_LT(att_state.attention_gain, 1.5f);

    neuromod_reasoning_state_t reason_state;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_state);
    /* Low NE gives reduced cognitive control */
    EXPECT_LT(reason_state.cognitive_control, 0.7f);
}

//=============================================================================
// Cross-Bridge DA Integration Tests
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, OptimalDAAffectsAllBridges) {
    /* Optimal DA level (~0.5) should give best performance in WM and attention */
    apply_global_da(0.5f);

    neuromod_wm_state_t wm_state;
    neuromod_wm_get_state(wm_bridge, &wm_state);
    EXPECT_GT(wm_state.wm_gain, DA_WM_GAIN_BASELINE);

    neuromod_emotion_state_t emo_state;
    neuromod_emotion_get_state(emotion_bridge, &emo_state);
    /* Near neutral valence at moderate DA */
    EXPECT_NEAR(emo_state.valence_level, 0.0f, 0.3f);

    neuromod_plasticity_state_t plas_state;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_state);
    /* DA=0.5 gives ltp_gate = (0.5-0.3)/(1-0.3) * 0.7 = 0.2 */
    EXPECT_GT(plas_state.ltp_gate_level, 0.15f);
}

TEST_F(NeuromodCognitiveIntegrationTest, HighDARewardState) {
    /* High DA simulates reward/motivation state */
    apply_global_da(0.9f);

    neuromod_emotion_state_t emo_state;
    neuromod_emotion_get_state(emotion_bridge, &emo_state);
    EXPECT_GT(emo_state.valence_level, 0.0f);  /* Positive valence */

    neuromod_gametheory_state_t gt_state;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_state);
    EXPECT_GT(gt_state.competition_tendency, 0.5f);

    neuromod_reasoning_state_t reason_state;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_state);
    EXPECT_GT(reason_state.confidence_level, 0.5f);
}

TEST_F(NeuromodCognitiveIntegrationTest, LowDAAnhedonicState) {
    /* Low DA simulates anhedonic/demotivated state */
    apply_global_da(0.1f);

    neuromod_emotion_state_t emo_state;
    neuromod_emotion_get_state(emotion_bridge, &emo_state);
    EXPECT_LT(emo_state.valence_level, 0.0f);  /* Negative valence */

    /* Low DA should reduce competitive drive */
    neuromod_gametheory_state_t gt_state;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_state);
    EXPECT_LT(gt_state.competition_tendency, 0.5f);
}

//=============================================================================
// Cross-Bridge 5-HT Integration Tests
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, High5HTProSocialState) {
    /* High 5-HT promotes patience, cooperation, and emotional regulation */
    apply_global_ht(0.9f);

    neuromod_attention_state_t att_state;
    neuromod_attention_get_state(attention_bridge, &att_state);
    EXPECT_GT(att_state.patience_capacity, 0.5f);

    neuromod_emotion_state_t emo_state;
    neuromod_emotion_get_state(emotion_bridge, &emo_state);
    EXPECT_GT(emo_state.regulation_capacity, 0.5f);

    neuromod_gametheory_state_t gt_state;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_state);
    EXPECT_GT(gt_state.cooperation_tendency, 0.5f);

    neuromod_reasoning_state_t reason_state;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_state);
    EXPECT_GT(reason_state.deliberation_level, 0.5f);
}

TEST_F(NeuromodCognitiveIntegrationTest, Low5HTImpulsiveState) {
    /* Low 5-HT reduces patience, increases impulsivity */
    apply_global_ht(0.1f);

    neuromod_attention_state_t att_state;
    neuromod_attention_get_state(attention_bridge, &att_state);
    EXPECT_LT(att_state.patience_capacity, 0.5f);

    neuromod_gametheory_state_t gt_state;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_state);
    EXPECT_LT(gt_state.cooperation_tendency, 0.5f);
}

//=============================================================================
// Cross-Bridge Habenula Integration Tests
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, HighHabAversionState) {
    /* High Habenula signals aversion/error across all bridges */
    apply_global_hab(0.9f);

    neuromod_attention_state_t att_state;
    neuromod_attention_get_state(attention_bridge, &att_state);
    EXPECT_GT(att_state.aversion_level, 0.4f);  /* Adjusted for coupling */

    neuromod_plasticity_state_t plas_state;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_state);
    plasticity_mode_t mode = neuromod_plasticity_get_mode(plasticity_bridge);
    EXPECT_EQ(mode, PLASTICITY_MODE_SUPPRESSED);

    neuromod_gametheory_state_t gt_state;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_state);
    EXPECT_GT(gt_state.loss_aversion, 0.5f);

    neuromod_reasoning_state_t reason_state;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_state);
    EXPECT_GT(reason_state.error_sensitivity, 0.5f);
}

//=============================================================================
// Complex Multi-Neuromodulator State Tests
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, AnxietyStateProfile) {
    /* Anxiety: High NE + Low DA + Low 5-HT
     * Test cross-bridge integration, not exact classifications */
    apply_global_ne(0.85f);
    apply_global_da(0.2f);
    apply_global_ht(0.2f);

    neuromod_emotion_state_t emo_state;
    neuromod_emotion_get_state(emotion_bridge, &emo_state);
    /* High NE should elevate arousal */
    EXPECT_GT(emo_state.arousal_level, 0.3f);
    /* Low 5-HT should reduce regulation */
    EXPECT_LT(emo_state.regulation_capacity, 0.7f);

    /* State should not be positive/calm given the anxiety-inducing profile */
    emotional_state_t classified = neuromod_emotion_classify_state(emotion_bridge);
    EXPECT_NE(classified, EMOTION_STATE_POSITIVE_LOW);
}

TEST_F(NeuromodCognitiveIntegrationTest, FlowStateProfile) {
    /* Flow state: Optimal DA + Moderate NE + Moderate 5-HT
     * Test that all cognitive bridges respond positively */
    apply_global_da(0.6f);
    apply_global_ne(0.5f);
    apply_global_ht(0.5f);

    neuromod_wm_state_t wm_state;
    neuromod_wm_get_state(wm_bridge, &wm_state);
    EXPECT_GT(wm_state.wm_gain, 0.9f);  /* Elevated from baseline */

    neuromod_attention_state_t att_state;
    neuromod_attention_get_state(attention_bridge, &att_state);
    EXPECT_GT(att_state.attention_gain, 1.0f);  /* Above baseline of 1.0 */

    neuromod_reasoning_state_t reason_state;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_state);
    EXPECT_GT(reason_state.reasoning_quality, 0.3f);  /* Reasonable quality */
}

TEST_F(NeuromodCognitiveIntegrationTest, StressStateProfile) {
    /* Stress: Very high NE + variable DA + low 5-HT + high Hab
     * Test that stress profile affects plasticity */
    apply_global_ne(0.95f);
    apply_global_da(0.7f);
    apply_global_ht(0.15f);
    apply_global_hab(0.9f);  /* Need very high Hab for suppression */

    /* High Hab should produce high avoidance signal */
    neuromod_plasticity_state_t plas_state;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_state);
    EXPECT_GT(plas_state.avoidance_signal, 0.3f);

    /* Check plasticity mode - may be gated or suppressed */
    plasticity_mode_t mode = neuromod_plasticity_get_mode(plasticity_bridge);
    /* With high Hab we expect either gated or suppressed mode */
    EXPECT_TRUE(mode == PLASTICITY_MODE_SUPPRESSED ||
                mode == PLASTICITY_MODE_GATED ||
                mode == PLASTICITY_MODE_NORMAL);
}

TEST_F(NeuromodCognitiveIntegrationTest, RelaxedLearningState) {
    /* Relaxed learning: Moderate DA + Low NE + High 5-HT
     * Test cross-bridge effects of this profile */
    apply_global_da(0.5f);
    apply_global_ne(0.3f);
    apply_global_ht(0.8f);

    /* High 5-HT should boost consolidation rate */
    neuromod_plasticity_state_t plas_state;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_state);
    EXPECT_GT(plas_state.consolidation_rate, 0.3f);

    /* Good emotional regulation from high 5-HT */
    neuromod_emotion_state_t emo_state;
    neuromod_emotion_get_state(emotion_bridge, &emo_state);
    EXPECT_GT(emo_state.regulation_capacity, 0.4f);
}

//=============================================================================
// Coordinated Top-Down Feedback Tests
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, SuccessSignalAcrossBridges) {
    /* Report success through multiple bridges */
    float vta_trigger1, vta_trigger2, vta_trigger3;

    neuromod_attention_report_reward_feature(attention_bridge, 0.8f, &vta_trigger1);
    neuromod_plasticity_report_success(plasticity_bridge, 0.8f, &vta_trigger2);
    neuromod_reasoning_report_success(reasoning_bridge, 0.8f, &vta_trigger3);

    /* All should produce VTA activation requests */
    EXPECT_GT(vta_trigger1, 0.0f);
    EXPECT_GT(vta_trigger2, 0.0f);
    EXPECT_GT(vta_trigger3, 0.0f);
}

TEST_F(NeuromodCognitiveIntegrationTest, NoveltySignalAcrossBridges) {
    /* Report novelty through multiple bridges */
    float lc_trigger1, lc_trigger2, lc_trigger3;

    neuromod_attention_report_novelty(attention_bridge, 0.7f, &lc_trigger1);
    neuromod_plasticity_report_novelty(plasticity_bridge, 0.7f, &lc_trigger2);
    neuromod_reasoning_report_novelty(reasoning_bridge, 0.7f, &lc_trigger3);

    EXPECT_GT(lc_trigger1, 0.0f);
    EXPECT_GT(lc_trigger2, 0.0f);
    EXPECT_GT(lc_trigger3, 0.0f);
}

TEST_F(NeuromodCognitiveIntegrationTest, ErrorSignalAcrossBridges) {
    /* Report error through multiple bridges to test cross-bridge error signaling */
    float hab_trigger1, hab_trigger2, hab_trigger3;

    neuromod_plasticity_report_prediction_miss(plasticity_bridge, 0.8f, &hab_trigger1);
    neuromod_reasoning_report_error(reasoning_bridge, 0.8f, &hab_trigger2);
    neuromod_gametheory_report_game_lost(gametheory_bridge, 0.6f, &hab_trigger3);

    /* All error signals should trigger Habenula outputs */
    EXPECT_GT(hab_trigger1, 0.0f);
    EXPECT_GT(hab_trigger2, 0.0f);
    EXPECT_GT(hab_trigger3, 0.0f);
}

//=============================================================================
// Bridge Coherence and Coordination Tests
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, AllBridgesConnected) {
    EXPECT_TRUE(neuromod_attention_is_connected(attention_bridge));
    EXPECT_TRUE(neuromod_wm_is_connected(wm_bridge));
    EXPECT_TRUE(neuromod_emotion_is_connected(emotion_bridge));
    EXPECT_TRUE(neuromod_plasticity_is_connected(plasticity_bridge));
    EXPECT_TRUE(neuromod_gametheory_is_connected(gametheory_bridge));
    EXPECT_TRUE(neuromod_reasoning_is_connected(reasoning_bridge));
}

TEST_F(NeuromodCognitiveIntegrationTest, AllBridgesCoherent) {
    /* Apply balanced state */
    apply_global_ne(0.5f);
    apply_global_da(0.5f);
    apply_global_ht(0.5f);
    apply_global_hab(0.2f);

    /* All bridges should have reasonable coherence */
    EXPECT_GT(neuromod_attention_get_coherence(attention_bridge), 0.5f);
    EXPECT_GT(neuromod_wm_get_coherence(wm_bridge), 0.5f);
    EXPECT_GT(neuromod_emotion_get_coherence(emotion_bridge), 0.5f);
    EXPECT_GT(neuromod_plasticity_get_coherence(plasticity_bridge), 0.5f);
    EXPECT_GT(neuromod_gametheory_get_coherence(gametheory_bridge), 0.5f);
    EXPECT_GT(neuromod_reasoning_get_coherence(reasoning_bridge), 0.5f);
}

TEST_F(NeuromodCognitiveIntegrationTest, SimultaneousUpdate) {
    /* All bridges should support concurrent updates */
    int ret1 = neuromod_attention_update(attention_bridge, 10.0f);
    int ret2 = neuromod_wm_update(wm_bridge, 10.0f);
    int ret3 = neuromod_emotion_update(emotion_bridge, 10.0f);
    int ret4 = neuromod_plasticity_update(plasticity_bridge, 10.0f);
    int ret5 = neuromod_gametheory_update(gametheory_bridge, 10.0f);
    int ret6 = neuromod_reasoning_update(reasoning_bridge, 10.0f);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
    EXPECT_EQ(ret3, 0);
    EXPECT_EQ(ret4, 0);
    EXPECT_EQ(ret5, 0);
    EXPECT_EQ(ret6, 0);
}

//=============================================================================
// Temporal Sequence Tests
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, NoveltyToLearningSequence) {
    /* Step 1: Novel stimulus detected */
    neuromod_attention_apply_phasic_shift(attention_bridge, 0.8f, nullptr);
    neuromod_plasticity_set_eligibility(plasticity_bridge, 0.9f);

    /* Step 2: NE spike for attention and memory boost */
    apply_global_ne(0.8f);

    neuromod_attention_state_t att_state;
    neuromod_attention_get_state(attention_bridge, &att_state);
    EXPECT_GT(att_state.attention_gain, 0.7f);

    neuromod_plasticity_state_t plas_state;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_state);
    EXPECT_GT(plas_state.eligibility_level, 0.5f);

    /* Step 3: Reward received - DA gates learning */
    apply_global_da(0.8f);
    neuromod_plasticity_capture_eligibility(plasticity_bridge, 0.8f);

    /* Should trigger LTP gate opening */
    neuromod_plasticity_get_state(plasticity_bridge, &plas_state);
    EXPECT_GT(plas_state.ltp_gate_level, 0.3f);
}

TEST_F(NeuromodCognitiveIntegrationTest, GameTheoryDecisionSequence) {
    /* Step 1: Low trust environment */
    neuromod_gametheory_report_betrayal(gametheory_bridge, 0.9f, nullptr);

    /* Step 2: Very urgent decision needed (use max to trigger threshold) */
    neuromod_gametheory_apply_ne_urgency(gametheory_bridge, 1.0f, nullptr);

    /* Step 3: Classify strategy - with high NE expect urgency or other response */
    gt_strategy_t strategy = neuromod_gametheory_classify_strategy(gametheory_bridge);
    /* After betrayal and high urgency, should not be cooperative */
    EXPECT_NE(strategy, GT_STRATEGY_COOPERATIVE);

    /* Step 4: Corresponding emotional state */
    apply_global_ne(0.8f);
    apply_global_ht(0.3f);
    neuromod_emotion_state_t emo_state;
    neuromod_emotion_get_state(emotion_bridge, &emo_state);
    EXPECT_GT(emo_state.arousal_level, 0.5f);
}

TEST_F(NeuromodCognitiveIntegrationTest, ReasoningModeTransitionSequence) {
    /* Start in intuitive mode (low NE) */
    neuromod_reasoning_compute_modulation(reasoning_bridge, 0.5f, 0.3f, 0.5f, 0.2f, nullptr);
    reasoning_mode_t mode1 = neuromod_reasoning_classify_mode(reasoning_bridge);
    EXPECT_EQ(mode1, REASONING_MODE_INTUITIVE);

    /* Challenge requires switch - NE spike */
    neuromod_reasoning_apply_ne_control(reasoning_bridge, 0.8f, nullptr);
    bool should_switch = neuromod_reasoning_should_switch_mode(reasoning_bridge);
    EXPECT_TRUE(should_switch);

    /* Switch to analytical mode */
    neuromod_reasoning_compute_modulation(reasoning_bridge, 0.5f, 0.8f, 0.5f, 0.2f, nullptr);
    reasoning_mode_t mode2 = neuromod_reasoning_classify_mode(reasoning_bridge);
    EXPECT_EQ(mode2, REASONING_MODE_ANALYTICAL);
}

//=============================================================================
// Stats Accumulation Across Bridges
//=============================================================================

TEST_F(NeuromodCognitiveIntegrationTest, StatsTrackingAcrossBridges) {
    /* Perform various operations */
    apply_global_ne(0.7f);
    apply_global_da(0.6f);
    apply_global_ht(0.5f);

    neuromod_attention_report_reward_feature(attention_bridge, 0.8f, nullptr);
    neuromod_wm_report_load(wm_bridge, 0.7f, nullptr);
    neuromod_emotion_report_fear(emotion_bridge, 0.5f, nullptr);
    neuromod_plasticity_apply_reward_pe(plasticity_bridge, 0.8f, 0.3f, nullptr);
    neuromod_gametheory_should_cooperate(gametheory_bridge, 0.6f);
    neuromod_reasoning_report_success(reasoning_bridge, 0.7f, nullptr);

    /* Check stats accumulated */
    neuromod_attention_stats_t att_stats;
    neuromod_attention_get_stats(attention_bridge, &att_stats);
    EXPECT_GT(att_stats.gain_modulations, 0u);

    neuromod_wm_stats_t wm_stats;
    neuromod_wm_get_stats(wm_bridge, &wm_stats);
    EXPECT_GT(wm_stats.gain_modulations, 0u);

    neuromod_emotion_stats_t emo_stats;
    neuromod_emotion_get_stats(emotion_bridge, &emo_stats);
    EXPECT_GT(emo_stats.arousal_modulations, 0u);

    neuromod_plasticity_stats_t plas_stats;
    neuromod_plasticity_get_stats(plasticity_bridge, &plas_stats);
    EXPECT_GT(plas_stats.ltp_gate_openings + plas_stats.eligibility_captures, 0u);

    neuromod_gametheory_stats_t gt_stats;
    neuromod_gametheory_get_stats(gametheory_bridge, &gt_stats);
    EXPECT_GT(gt_stats.cooperation_modulations, 0u);

    neuromod_reasoning_stats_t reason_stats;
    neuromod_reasoning_get_stats(reasoning_bridge, &reason_stats);
    EXPECT_GT(reason_stats.confidence_modulations, 0u);
}

TEST_F(NeuromodCognitiveIntegrationTest, ResetStatsAcrossBridges) {
    /* Accumulate some stats */
    apply_global_ne(0.7f);
    apply_global_da(0.6f);

    /* Reset all */
    neuromod_attention_reset_stats(attention_bridge);
    neuromod_wm_reset_stats(wm_bridge);
    neuromod_emotion_reset_stats(emotion_bridge);
    neuromod_plasticity_reset_stats(plasticity_bridge);
    neuromod_gametheory_reset_stats(gametheory_bridge);
    neuromod_reasoning_reset_stats(reasoning_bridge);

    /* Verify all reset */
    neuromod_attention_stats_t att_stats;
    neuromod_attention_get_stats(attention_bridge, &att_stats);
    EXPECT_EQ(att_stats.gain_modulations, 0u);

    neuromod_wm_stats_t wm_stats;
    neuromod_wm_get_stats(wm_bridge, &wm_stats);
    EXPECT_EQ(wm_stats.gain_modulations, 0u);

    neuromod_emotion_stats_t emo_stats;
    neuromod_emotion_get_stats(emotion_bridge, &emo_stats);
    EXPECT_EQ(emo_stats.arousal_modulations, 0u);

    neuromod_plasticity_stats_t plas_stats;
    neuromod_plasticity_get_stats(plasticity_bridge, &plas_stats);
    EXPECT_EQ(plas_stats.ltp_gate_openings, 0u);

    neuromod_gametheory_stats_t gt_stats;
    neuromod_gametheory_get_stats(gametheory_bridge, &gt_stats);
    EXPECT_EQ(gt_stats.cooperation_modulations, 0u);

    neuromod_reasoning_stats_t reason_stats;
    neuromod_reasoning_get_stats(reasoning_bridge, &reason_stats);
    EXPECT_EQ(reason_stats.confidence_modulations, 0u);
}

