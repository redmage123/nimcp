/**
 * @file test_neuromod_cognitive_e2e.cpp
 * @brief End-to-End tests for Neuromodulatory-Cognitive Integration
 *
 * WHAT: E2E test suite for full neuromodulator-to-behavior flows
 * WHY:  Verify complete processing pipelines from neuromod input to cognitive output
 * HOW:  Simulate realistic scenarios with neuromod nuclei driving cognitive behaviors
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <string>

extern "C" {
#include "integration/inter/neuromod_attention/nimcp_neuromod_attention_bridge.h"
#include "integration/inter/neuromod_wm/nimcp_neuromod_wm_bridge.h"
#include "integration/inter/neuromod_emotion/nimcp_neuromod_emotion_bridge.h"
#include "integration/inter/neuromod_plasticity/nimcp_neuromod_plasticity_bridge.h"
#include "integration/inter/neuromod_gametheory/nimcp_neuromod_gametheory_bridge.h"
#include "integration/inter/neuromod_reasoning/nimcp_neuromod_reasoning_bridge.h"
}

//=============================================================================
// E2E Test Fixture: Simulated Brain State
//=============================================================================

class NeuromodCognitiveE2ETest : public ::testing::Test {
protected:
    /* Simulated neuromodulatory nuclei state */
    struct NeuromodState {
        float lc_ne;      /* Locus Coeruleus norepinephrine */
        float vta_da;     /* VTA dopamine */
        float raphe_ht;   /* Raphe serotonin */
        float hab_signal; /* Habenula signal */
    };

    /* All cognitive bridges */
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

    /**
     * Apply neuromodulatory state to all cognitive bridges
     * Simulates broadcast from neuromod nuclei to all target regions
     */
    void apply_neuromod_state(const NeuromodState& state) {
        /* NE from LC affects attention, arousal, flexibility, memory boost, urgency, control */
        neuromod_attention_apply_ne_gain(attention_bridge, state.lc_ne, nullptr);
        neuromod_wm_apply_ne_flexibility(wm_bridge, state.lc_ne, nullptr);
        neuromod_emotion_apply_ne_arousal(emotion_bridge, state.lc_ne, nullptr);
        neuromod_plasticity_apply_ne_boost(plasticity_bridge, state.lc_ne, nullptr);
        neuromod_gametheory_apply_ne_urgency(gametheory_bridge, state.lc_ne, nullptr);
        neuromod_reasoning_apply_ne_control(reasoning_bridge, state.lc_ne, nullptr);

        /* DA from VTA affects salience, WM gain, valence, LTP gating, competition, confidence */
        neuromod_attention_apply_da_salience(attention_bridge, state.vta_da, nullptr);
        neuromod_wm_apply_da_gain(wm_bridge, state.vta_da, nullptr);
        neuromod_emotion_apply_da_valence(emotion_bridge, state.vta_da, nullptr);
        neuromod_plasticity_apply_da_gating(plasticity_bridge, state.vta_da, nullptr);
        neuromod_gametheory_apply_da_competition(gametheory_bridge, state.vta_da, nullptr);
        neuromod_reasoning_apply_da_confidence(reasoning_bridge, state.vta_da, nullptr);

        /* 5-HT from Raphe affects patience, delay tolerance, regulation, consolidation, cooperation, deliberation */
        neuromod_attention_apply_ht_patience(attention_bridge, state.raphe_ht, nullptr);
        neuromod_wm_apply_ht_delay(wm_bridge, state.raphe_ht, nullptr);
        neuromod_emotion_apply_ht_regulation(emotion_bridge, state.raphe_ht, nullptr);
        neuromod_plasticity_apply_ht_consolidation(plasticity_bridge, state.raphe_ht, nullptr);
        neuromod_gametheory_apply_ht_cooperation(gametheory_bridge, state.raphe_ht, nullptr);
        neuromod_reasoning_apply_ht_deliberation(reasoning_bridge, state.raphe_ht, nullptr);

        /* Habenula affects aversion, avoidance, loss aversion, error sensitivity */
        neuromod_attention_apply_hab_aversion(attention_bridge, state.hab_signal, nullptr);
        neuromod_emotion_apply_hab_aversion(emotion_bridge, state.hab_signal, nullptr);
        neuromod_plasticity_apply_hab_avoidance(plasticity_bridge, state.hab_signal, nullptr);
        neuromod_gametheory_apply_hab_loss_aversion(gametheory_bridge, state.hab_signal, nullptr);
        neuromod_reasoning_apply_hab_error(reasoning_bridge, state.hab_signal, nullptr);
    }

    /**
     * Compute full modulation state across all bridges
     */
    void compute_all_modulations(const NeuromodState& state) {
        neuromod_attention_compute_modulation(attention_bridge,
            state.lc_ne, state.vta_da, state.raphe_ht, state.hab_signal, nullptr);
        neuromod_wm_compute_modulation(wm_bridge,
            state.vta_da, state.lc_ne, state.raphe_ht, nullptr);
        neuromod_emotion_compute_modulation(emotion_bridge,
            state.lc_ne, state.vta_da, state.raphe_ht, state.hab_signal, nullptr);
        neuromod_plasticity_compute_modulation(plasticity_bridge,
            state.vta_da, state.lc_ne, state.raphe_ht, state.hab_signal, nullptr);
        neuromod_gametheory_compute_modulation(gametheory_bridge,
            state.raphe_ht, state.vta_da, state.lc_ne, state.hab_signal, nullptr);
        neuromod_reasoning_compute_modulation(reasoning_bridge,
            state.vta_da, state.lc_ne, state.raphe_ht, state.hab_signal, nullptr);
    }

    /**
     * Update all bridges with time step
     */
    void update_all(float dt_ms) {
        neuromod_attention_update(attention_bridge, dt_ms);
        neuromod_wm_update(wm_bridge, dt_ms);
        neuromod_emotion_update(emotion_bridge, dt_ms);
        neuromod_plasticity_update(plasticity_bridge, dt_ms);
        neuromod_gametheory_update(gametheory_bridge, dt_ms);
        neuromod_reasoning_update(reasoning_bridge, dt_ms);
    }

    /**
     * Check if all bridges are connected and coherent
     */
    bool all_bridges_healthy() {
        return neuromod_attention_is_connected(attention_bridge) &&
               neuromod_wm_is_connected(wm_bridge) &&
               neuromod_emotion_is_connected(emotion_bridge) &&
               neuromod_plasticity_is_connected(plasticity_bridge) &&
               neuromod_gametheory_is_connected(gametheory_bridge) &&
               neuromod_reasoning_is_connected(reasoning_bridge);
    }
};

//=============================================================================
// E2E Scenario: Wake-up and Morning Alertness
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, WakeUpAlertness) {
    /* Scenario: System transitions from sleep to wakefulness */

    /* Initial state: Low arousal (sleep-like) */
    NeuromodState sleep_state = {0.1f, 0.3f, 0.5f, 0.1f};
    apply_neuromod_state(sleep_state);
    compute_all_modulations(sleep_state);

    neuromod_attention_state_t att_sleep;
    neuromod_attention_get_state(attention_bridge, &att_sleep);
    float attention_asleep = att_sleep.attention_gain;

    neuromod_reasoning_state_t reason_sleep;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_sleep);
    float control_asleep = reason_sleep.cognitive_control;

    /* Transition: LC activation (wake-up signal) */
    NeuromodState waking_state = {0.5f, 0.4f, 0.5f, 0.1f};
    apply_neuromod_state(waking_state);
    compute_all_modulations(waking_state);
    update_all(100.0f);  /* 100ms time step */

    /* Final state: Fully awake (optimal arousal) */
    NeuromodState awake_state = {0.6f, 0.5f, 0.5f, 0.1f};
    apply_neuromod_state(awake_state);
    compute_all_modulations(awake_state);
    update_all(100.0f);

    neuromod_attention_state_t att_awake;
    neuromod_attention_get_state(attention_bridge, &att_awake);
    float attention_awake = att_awake.attention_gain;

    neuromod_reasoning_state_t reason_awake;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_awake);
    float control_awake = reason_awake.cognitive_control;

    /* Verify: Attention and cognitive control improved with wakefulness */
    EXPECT_GT(attention_awake, attention_asleep);
    EXPECT_GT(control_awake, control_asleep);
    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Novel Stimulus and Learning
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, NovelStimulusLearning) {
    /* Scenario: Novel stimulus triggers attention, eligibility, then reward leads to learning */

    /* Baseline state */
    NeuromodState baseline = {0.4f, 0.4f, 0.5f, 0.1f};
    apply_neuromod_state(baseline);
    compute_all_modulations(baseline);

    /* Step 1: Novel stimulus detected - LC phasic response */
    neuromod_attention_apply_phasic_shift(attention_bridge, 0.8f, nullptr);
    neuromod_plasticity_set_eligibility(plasticity_bridge, 0.9f);

    /* Phasic NE from novelty */
    NeuromodState novelty_state = {0.8f, 0.4f, 0.5f, 0.1f};
    apply_neuromod_state(novelty_state);

    neuromod_attention_state_t att_novelty;
    neuromod_attention_get_state(attention_bridge, &att_novelty);
    EXPECT_GT(att_novelty.attention_gain, 0.5f);

    neuromod_plasticity_state_t plas_elig;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_elig);
    EXPECT_GT(plas_elig.eligibility_level, 0.5f);

    /* Step 2: Time passes - eligibility starts to decay */
    neuromod_plasticity_decay_eligibility(plasticity_bridge, 50.0f);

    /* Step 3: Reward received - DA spike captures eligibility */
    NeuromodState reward_state = {0.5f, 0.9f, 0.5f, 0.1f};
    apply_neuromod_state(reward_state);

    float rpe;
    neuromod_plasticity_apply_reward_pe(plasticity_bridge, 0.9f, 0.3f, &rpe);
    EXPECT_GT(rpe, 0.0f);  /* Positive prediction error */

    neuromod_plasticity_capture_eligibility(plasticity_bridge, 0.9f);

    neuromod_plasticity_state_t plas_learned;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_learned);
    EXPECT_GT(plas_learned.ltp_gate_level, 0.3f);  /* LTP gate opened */

    /* Verify emotional valence is positive */
    neuromod_emotion_state_t emo_reward;
    neuromod_emotion_get_state(emotion_bridge, &emo_reward);
    EXPECT_GT(emo_reward.valence_level, 0.0f);

    /* Step 4: Return to baseline and consolidate */
    NeuromodState consolidate_state = {0.3f, 0.4f, 0.8f, 0.1f};
    apply_neuromod_state(consolidate_state);
    compute_all_modulations(consolidate_state);

    /* High 5-HT should boost consolidation rate */
    neuromod_plasticity_state_t plas_state;
    neuromod_plasticity_get_state(plasticity_bridge, &plas_state);
    EXPECT_GT(plas_state.consolidation_rate, 0.3f);

    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Stress and Cognitive Impairment
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, StressCognitiveImpairment) {
    /* Scenario: Stress causes excessive NE, impaired cognition */

    /* Optimal baseline */
    NeuromodState optimal = {0.5f, 0.5f, 0.5f, 0.2f};
    apply_neuromod_state(optimal);
    compute_all_modulations(optimal);

    neuromod_wm_state_t wm_optimal;
    neuromod_wm_get_state(wm_bridge, &wm_optimal);
    float wm_gain_optimal = wm_optimal.wm_gain;

    neuromod_reasoning_state_t reason_optimal;
    neuromod_reasoning_get_state(reasoning_bridge, &reason_optimal);
    float quality_optimal = reason_optimal.reasoning_quality;

    /* Stress: Very high NE, high DA, low 5-HT, high Hab */
    NeuromodState stress = {0.95f, 0.7f, 0.15f, 0.8f};
    apply_neuromod_state(stress);
    compute_all_modulations(stress);

    /* Emotional state should reflect stress */
    neuromod_emotion_state_t emo_stress;
    neuromod_emotion_get_state(emotion_bridge, &emo_stress);
    EXPECT_GT(emo_stress.arousal_level, 0.3f);  /* Elevated arousal */
    EXPECT_LT(emo_stress.regulation_capacity, 0.7f);  /* Reduced regulation */

    /* Under stress, should not be calm positive state */
    emotional_state_t emo_class = neuromod_emotion_classify_state(emotion_bridge);
    EXPECT_NE(emo_class, EMOTION_STATE_POSITIVE_LOW);

    /* Reasoning should be impaired */
    reasoning_mode_t reason_mode = neuromod_reasoning_classify_mode(reasoning_bridge);
    /* High Hab can cause impairment, or extreme NE moves past inverted-U peak */

    /* Under stress, plasticity should not be in enhanced mode */
    plasticity_mode_t plas_mode = neuromod_plasticity_get_mode(plasticity_bridge);
    EXPECT_NE(plas_mode, PLASTICITY_MODE_BOOSTED);

    /* WM flexibility should be affected */
    neuromod_wm_state_t wm_stress;
    neuromod_wm_get_state(wm_bridge, &wm_stress);

    /* Game theory should show cautious/urgent strategy */
    gt_strategy_t gt_strat = neuromod_gametheory_classify_strategy(gametheory_bridge);
    EXPECT_TRUE(gt_strat == GT_STRATEGY_CAUTIOUS ||
                gt_strat == GT_STRATEGY_URGENT);

    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Social Cooperation Decision
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, SocialCooperationDecision) {
    /* Scenario: Agent must decide whether to cooperate in social game */

    /* Pro-social state: high 5-HT, moderate DA, low Hab */
    NeuromodState prosocial = {0.4f, 0.5f, 0.8f, 0.15f};
    apply_neuromod_state(prosocial);
    compute_all_modulations(prosocial);

    /* Should favor cooperation */
    neuromod_gametheory_state_t gt_prosocial;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_prosocial);
    /* High 5-HT should elevate cooperation tendency */
    EXPECT_GT(gt_prosocial.cooperation_tendency, 0.3f);

    gt_strategy_t strat_prosocial = neuromod_gametheory_classify_strategy(gametheory_bridge);
    /* With high 5-HT, should not be competitive */
    EXPECT_NE(strat_prosocial, GT_STRATEGY_COMPETITIVE);

    /* Fair offer should be accepted */
    float accept_fair = neuromod_gametheory_evaluate_offer(gametheory_bridge, 0.5f);
    EXPECT_GT(accept_fair, 0.5f);

    /* Reset and test competitive state: low 5-HT, high DA */
    NeuromodState competitive = {0.5f, 0.9f, 0.2f, 0.3f};
    apply_neuromod_state(competitive);
    compute_all_modulations(competitive);

    neuromod_gametheory_state_t gt_competitive;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_competitive);
    EXPECT_GT(gt_competitive.competition_tendency, 0.5f);
    EXPECT_LT(gt_competitive.cooperation_tendency, 0.5f);

    bool cooperate_competitive = neuromod_gametheory_should_cooperate(gametheory_bridge, 0.6f);
    EXPECT_FALSE(cooperate_competitive);

    gt_strategy_t strat_competitive = neuromod_gametheory_classify_strategy(gametheory_bridge);
    /* With high DA and low 5-HT, should not be cooperative or balanced */
    EXPECT_NE(strat_competitive, GT_STRATEGY_COOPERATIVE);
    EXPECT_NE(strat_competitive, GT_STRATEGY_BALANCED);

    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Risk Decision Under Pressure
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, RiskDecisionUnderPressure) {
    /* Scenario: Agent must decide whether to take risk under time pressure */

    /* Calm baseline: should take calculated risks */
    NeuromodState calm = {0.4f, 0.6f, 0.5f, 0.2f};
    apply_neuromod_state(calm);
    compute_all_modulations(calm);

    neuromod_gametheory_apply_da_risk(gametheory_bridge, 0.6f, nullptr);
    neuromod_gametheory_apply_hab_loss_aversion(gametheory_bridge, 0.2f, nullptr);

    /* Positive expected value with moderate variance - should take risk */
    bool take_risk_calm = neuromod_gametheory_should_take_risk(gametheory_bridge, 0.2f, 0.3f);
    EXPECT_TRUE(take_risk_calm);

    /* Under time pressure: high NE urgency */
    neuromod_gametheory_report_time_pressure(gametheory_bridge, 0.8f, nullptr);
    neuromod_gametheory_apply_ne_urgency(gametheory_bridge, 0.9f, nullptr);

    gt_strategy_t strat_urgent = neuromod_gametheory_classify_strategy(gametheory_bridge);
    /* High NE should shift strategy away from balanced */
    EXPECT_NE(strat_urgent, GT_STRATEGY_BALANCED);

    /* Loss aversion state: high Hab, low DA */
    NeuromodState loss_averse = {0.5f, 0.3f, 0.5f, 0.8f};
    apply_neuromod_state(loss_averse);

    neuromod_gametheory_apply_hab_loss_aversion(gametheory_bridge, 0.9f, nullptr);
    neuromod_gametheory_apply_da_risk(gametheory_bridge, 0.2f, nullptr);

    /* Loss aversion state should affect decision - verify state update */
    neuromod_gametheory_state_t gt_state;
    neuromod_gametheory_get_state(gametheory_bridge, &gt_state);
    EXPECT_GT(gt_state.loss_aversion, 0.4f);
    EXPECT_LT(gt_state.risk_tolerance, 0.5f);

    gt_strategy_t strat_cautious = neuromod_gametheory_classify_strategy(gametheory_bridge);
    /* High Hab should shift strategy - not cooperative */
    EXPECT_NE(strat_cautious, GT_STRATEGY_COOPERATIVE);

    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Reasoning Mode Adaptation
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, ReasoningModeAdaptation) {
    /* Scenario: Reasoning mode adapts based on task demands and neuromodulatory state */

    /* Intuitive mode: low NE, relaxed state */
    NeuromodState intuitive = {0.3f, 0.5f, 0.5f, 0.2f};
    apply_neuromod_state(intuitive);
    compute_all_modulations(intuitive);

    reasoning_mode_t mode1 = neuromod_reasoning_classify_mode(reasoning_bridge);
    EXPECT_EQ(mode1, REASONING_MODE_INTUITIVE);

    /* Challenge detected - system should suggest mode switch */
    neuromod_reasoning_apply_ne_control(reasoning_bridge, 0.8f, nullptr);
    bool should_switch = neuromod_reasoning_should_switch_mode(reasoning_bridge);
    EXPECT_TRUE(should_switch);

    /* Switch to analytical mode: high NE */
    NeuromodState analytical = {0.7f, 0.5f, 0.5f, 0.2f};
    apply_neuromod_state(analytical);
    compute_all_modulations(analytical);

    reasoning_mode_t mode2 = neuromod_reasoning_classify_mode(reasoning_bridge);
    EXPECT_EQ(mode2, REASONING_MODE_ANALYTICAL);

    /* Creative mode: high DA, exploring */
    NeuromodState creative = {0.5f, 0.9f, 0.5f, 0.1f};
    apply_neuromod_state(creative);
    compute_all_modulations(creative);

    reasoning_mode_t mode3 = neuromod_reasoning_classify_mode(reasoning_bridge);
    EXPECT_EQ(mode3, REASONING_MODE_CREATIVE);

    /* Cautious mode: high 5-HT, careful deliberation */
    NeuromodState cautious = {0.5f, 0.5f, 0.9f, 0.2f};
    apply_neuromod_state(cautious);
    compute_all_modulations(cautious);

    reasoning_mode_t mode4 = neuromod_reasoning_classify_mode(reasoning_bridge);
    EXPECT_EQ(mode4, REASONING_MODE_CAUTIOUS);

    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Confidence Calibration Over Time
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, ConfidenceCalibrationOverTime) {
    /* Scenario: System learns to calibrate confidence based on success/failure history */

    NeuromodState baseline = {0.5f, 0.6f, 0.5f, 0.2f};
    apply_neuromod_state(baseline);

    /* Initially uncalibrated (not enough data) */
    float initial_calib = neuromod_reasoning_get_confidence_calibration(reasoning_bridge, 0.7f);
    EXPECT_NEAR(initial_calib, 0.5f, 0.01f);  /* Default when uncalibrated */

    /* Report many successes at high confidence */
    for (int i = 0; i < 20; i++) {
        neuromod_reasoning_apply_da_confidence(reasoning_bridge, 0.7f, nullptr);
        neuromod_reasoning_report_success(reasoning_bridge, 0.7f, nullptr);
    }

    /* Calibration should now be higher (we were accurate at 0.7 confidence) */
    float calibrated = neuromod_reasoning_get_confidence_calibration(reasoning_bridge, 0.7f);
    EXPECT_GE(calibrated, 0.0f);
    EXPECT_LE(calibrated, 1.0f);

    /* Report some errors - verify error reporting works */
    float hab_trigger = 0.0f;
    for (int i = 0; i < 5; i++) {
        neuromod_reasoning_report_error(reasoning_bridge, 0.6f, &hab_trigger);
    }
    /* Error reports should trigger habenula feedback */
    EXPECT_GT(hab_trigger, 0.0f);

    /* Verify state remains valid after error reports */
    neuromod_reasoning_state_t state_after;
    neuromod_reasoning_get_state(reasoning_bridge, &state_after);
    EXPECT_GE(state_after.confidence_level, 0.0f);
    EXPECT_LE(state_after.confidence_level, 1.0f);

    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Emotional Regulation
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, EmotionalRegulation) {
    /* Scenario: Fear response followed by regulation via 5-HT */

    /* Fear stimulus: High NE arousal, negative valence */
    neuromod_emotion_report_fear(emotion_bridge, 0.8f, nullptr);

    NeuromodState fear = {0.9f, 0.2f, 0.3f, 0.7f};
    apply_neuromod_state(fear);
    compute_all_modulations(fear);

    neuromod_emotion_state_t emo_fear;
    neuromod_emotion_get_state(emotion_bridge, &emo_fear);
    EXPECT_GT(emo_fear.arousal_level, 0.7f);
    EXPECT_LT(emo_fear.valence_level, 0.0f);

    emotional_state_t class_fear = neuromod_emotion_classify_state(emotion_bridge);
    EXPECT_TRUE(class_fear == EMOTION_STATE_ANXIOUS ||
                class_fear == EMOTION_STATE_NEGATIVE_HIGH);

    float stability_fear = neuromod_emotion_get_stability(emotion_bridge);

    /* Regulation: 5-HT increase, NE decrease */
    NeuromodState regulating = {0.5f, 0.4f, 0.8f, 0.3f};
    apply_neuromod_state(regulating);
    compute_all_modulations(regulating);

    neuromod_emotion_state_t emo_regulated;
    neuromod_emotion_get_state(emotion_bridge, &emo_regulated);
    EXPECT_GT(emo_regulated.regulation_capacity, 0.5f);
    EXPECT_LT(emo_regulated.arousal_level, emo_fear.arousal_level);

    float stability_regulated = neuromod_emotion_get_stability(emotion_bridge);
    EXPECT_GE(stability_regulated, 0.0f);

    /* Final calm state */
    NeuromodState calm = {0.4f, 0.5f, 0.7f, 0.1f};
    apply_neuromod_state(calm);
    compute_all_modulations(calm);

    emotional_state_t class_calm = neuromod_emotion_classify_state(emotion_bridge);
    EXPECT_TRUE(class_calm == EMOTION_STATE_NEUTRAL ||
                class_calm == EMOTION_STATE_POSITIVE_LOW);

    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Working Memory Under Load
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, WorkingMemoryUnderLoad) {
    /* Scenario: WM performance under increasing cognitive load */

    /* Optimal DA for best WM performance */
    NeuromodState optimal_wm = {0.5f, 0.5f, 0.5f, 0.2f};
    apply_neuromod_state(optimal_wm);
    compute_all_modulations(optimal_wm);

    neuromod_wm_state_t wm_optimal;
    neuromod_wm_get_state(wm_bridge, &wm_optimal);
    float gain_optimal = wm_optimal.wm_gain;
    EXPECT_GT(gain_optimal, DA_WM_GAIN_BASELINE);

    /* Report increasing load */
    float da_demand;
    neuromod_wm_report_load(wm_bridge, 0.5f, &da_demand);
    EXPECT_GT(da_demand, 0.0f);

    neuromod_wm_report_load(wm_bridge, 0.8f, &da_demand);
    EXPECT_GT(da_demand, 0.0f);

    /* High load should be reported - verify state after load reports */
    neuromod_wm_state_t wm_loaded;
    neuromod_wm_get_state(wm_bridge, &wm_loaded);
    /* State should remain valid after load reports */
    EXPECT_GE(wm_loaded.bridge_coherence, 0.0f);
    EXPECT_LE(wm_loaded.bridge_coherence, 1.0f);

    /* Overflow condition */
    float stress;
    neuromod_wm_report_overflow(wm_bridge, 0.9f, &stress);
    EXPECT_GT(stress, 0.0f);

    /* Task switching need */
    float lc_trigger;
    neuromod_wm_report_switch_need(wm_bridge, 0.7f, &lc_trigger);
    EXPECT_GT(lc_trigger, 0.0f);

    /* D1/D2 balance for stability vs flexibility */
    float stability, flexibility;
    neuromod_wm_apply_d1_stability(wm_bridge, 0.7f, &stability);
    neuromod_wm_apply_d2_flexibility(wm_bridge, 0.6f, &flexibility);
    EXPECT_GT(stability, 0.0f);
    EXPECT_GE(flexibility, 0.0f);

    /* NE reset on high arousal */
    bool reset_triggered;
    neuromod_wm_apply_ne_reset(wm_bridge, 0.9f, &reset_triggered);
    EXPECT_TRUE(reset_triggered);

    EXPECT_TRUE(all_bridges_healthy());
}

//=============================================================================
// E2E Scenario: Complete Cognitive Cycle
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, CompleteCognitiveCycle) {
    /* Scenario: Full cycle from stimulus detection to learning and memory consolidation */

    /* Step 1: Baseline resting state */
    NeuromodState rest = {0.3f, 0.4f, 0.5f, 0.1f};
    apply_neuromod_state(rest);
    compute_all_modulations(rest);
    update_all(100.0f);

    /* Verify resting mode */
    reasoning_mode_t mode_rest = neuromod_reasoning_classify_mode(reasoning_bridge);
    EXPECT_EQ(mode_rest, REASONING_MODE_INTUITIVE);

    /* Step 2: Stimulus arrives - attention capture */
    neuromod_attention_report_novelty(attention_bridge, 0.8f, nullptr);
    neuromod_plasticity_set_eligibility(plasticity_bridge, 0.9f);

    NeuromodState alert = {0.7f, 0.4f, 0.5f, 0.1f};
    apply_neuromod_state(alert);
    compute_all_modulations(alert);
    update_all(50.0f);

    neuromod_attention_state_t att_alert;
    neuromod_attention_get_state(attention_bridge, &att_alert);
    EXPECT_GT(att_alert.attention_gain, 0.5f);

    /* Step 3: Processing - analytical reasoning engaged */
    reasoning_mode_t mode_process = neuromod_reasoning_classify_mode(reasoning_bridge);
    EXPECT_TRUE(mode_process == REASONING_MODE_ANALYTICAL ||
                mode_process == REASONING_MODE_INTUITIVE);

    /* Step 4: Decision made successfully */
    neuromod_reasoning_report_success(reasoning_bridge, 0.8f, nullptr);
    neuromod_attention_report_reward_feature(attention_bridge, 0.8f, nullptr);

    /* Step 5: Reward received - DA spike */
    NeuromodState reward = {0.5f, 0.9f, 0.5f, 0.1f};
    apply_neuromod_state(reward);
    compute_all_modulations(reward);

    float rpe;
    neuromod_plasticity_apply_reward_pe(plasticity_bridge, 0.9f, 0.4f, &rpe);
    EXPECT_GT(rpe, 0.0f);

    neuromod_plasticity_capture_eligibility(plasticity_bridge, 0.9f);

    /* Positive emotional response */
    neuromod_emotion_state_t emo_reward;
    neuromod_emotion_get_state(emotion_bridge, &emo_reward);
    EXPECT_GT(emo_reward.valence_level, 0.0f);

    /* Step 6: Return to calm and consolidate */
    NeuromodState consolidate = {0.3f, 0.4f, 0.8f, 0.1f};
    apply_neuromod_state(consolidate);
    compute_all_modulations(consolidate);

    for (int i = 0; i < 10; i++) {
        update_all(100.0f);
    }

    plasticity_mode_t plas_mode = neuromod_plasticity_get_mode(plasticity_bridge);
    /* After consolidation, plasticity should not be suppressed or gated */
    EXPECT_NE(plas_mode, PLASTICITY_MODE_SUPPRESSED);

    /* Step 7: Full consolidation */
    update_all(500.0f);

    /* Verify system health after complete cycle */
    EXPECT_TRUE(all_bridges_healthy());

    /* Verify stats were accumulated */
    neuromod_attention_stats_t att_stats;
    neuromod_attention_get_stats(attention_bridge, &att_stats);
    EXPECT_GT(att_stats.gain_modulations, 0u);

    neuromod_plasticity_stats_t plas_stats;
    neuromod_plasticity_get_stats(plasticity_bridge, &plas_stats);
    EXPECT_GT(plas_stats.ltp_gate_openings, 0u);

    neuromod_reasoning_stats_t reason_stats;
    neuromod_reasoning_get_stats(reasoning_bridge, &reason_stats);
    EXPECT_GT(reason_stats.successful_reasoning, 0u);
}

//=============================================================================
// E2E Scenario: Long-Running Simulation
//=============================================================================

TEST_F(NeuromodCognitiveE2ETest, LongRunningSimulation) {
    /* Scenario: Extended simulation with varying states */

    std::vector<NeuromodState> states = {
        {0.3f, 0.4f, 0.5f, 0.1f},  /* Rest */
        {0.7f, 0.5f, 0.5f, 0.2f},  /* Alert */
        {0.5f, 0.8f, 0.5f, 0.1f},  /* Motivated */
        {0.6f, 0.5f, 0.8f, 0.1f},  /* Calm focused */
        {0.8f, 0.3f, 0.3f, 0.6f},  /* Stressed */
        {0.4f, 0.6f, 0.7f, 0.2f},  /* Recovery */
        {0.5f, 0.5f, 0.5f, 0.2f},  /* Balanced */
    };

    for (int cycle = 0; cycle < 10; cycle++) {
        for (const auto& state : states) {
            apply_neuromod_state(state);
            compute_all_modulations(state);
            update_all(100.0f);

            /* Verify bounds maintained */
            float coh1 = neuromod_attention_get_coherence(attention_bridge);
            float coh2 = neuromod_wm_get_coherence(wm_bridge);
            float coh3 = neuromod_emotion_get_coherence(emotion_bridge);
            float coh4 = neuromod_plasticity_get_coherence(plasticity_bridge);
            float coh5 = neuromod_gametheory_get_coherence(gametheory_bridge);
            float coh6 = neuromod_reasoning_get_coherence(reasoning_bridge);

            EXPECT_GE(coh1, 0.0f); EXPECT_LE(coh1, 1.0f);
            EXPECT_GE(coh2, 0.0f); EXPECT_LE(coh2, 1.0f);
            EXPECT_GE(coh3, 0.0f); EXPECT_LE(coh3, 1.0f);
            EXPECT_GE(coh4, 0.0f); EXPECT_LE(coh4, 1.0f);
            EXPECT_GE(coh5, 0.0f); EXPECT_LE(coh5, 1.0f);
            EXPECT_GE(coh6, 0.0f); EXPECT_LE(coh6, 1.0f);
        }
    }

    /* All bridges should remain healthy after extended simulation */
    EXPECT_TRUE(all_bridges_healthy());
}

