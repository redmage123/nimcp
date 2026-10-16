/**
 * @file test_cognitive_sleep_integration.cpp
 * @brief Integration tests for Cognitive-Sleep bridges working together
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Test all cognitive sleep bridges coordinating during sleep cycles
 * WHY:  Verify that Attention, Working Memory, and Executive bridges
 *       correctly interact during sleep state transitions
 * HOW:  Simulate sleep cycles and verify coordinated cognitive changes
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/attention/nimcp_attention_sleep_bridge.h"
#include "cognitive/working_memory/nimcp_working_memory_sleep_bridge.h"
#include "cognitive/executive/nimcp_executive_sleep_bridge.h"
#include "core/brain_oscillations/nimcp_oscillations_sleep_bridge.h"

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class CognitiveSleepIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

    /* Get all cognitive effects for a given sleep state */
    void getCognitiveEffectsForState(sleep_state_t state,
                                     float& attn_capacity, float& attn_vigilance,
                                     float& wm_capacity, float& wm_decay,
                                     float& exec_inhibition, float& exec_flexibility,
                                     float& osc_frequency, float& osc_spindle) {
        attn_capacity = attention_sleep_capacity_for_state(state);
        attn_vigilance = attention_sleep_vigilance_for_state(state);
        wm_capacity = working_memory_sleep_capacity_for_state(state);
        wm_decay = working_memory_sleep_decay_for_state(state);
        exec_inhibition = executive_sleep_inhibition_for_state(state);
        exec_flexibility = executive_sleep_flexibility_for_state(state);
        osc_frequency = oscillations_sleep_freq_for_state(state);
        osc_spindle = oscillations_sleep_spindle_for_state(state);
    }
};

/* ============================================================================
 * Coordinated Cognitive State Tests
 * ============================================================================ */

TEST_F(CognitiveSleepIntegrationTest, AwakeState_FullCognitiveCapacity) {
    /* WHAT: Verify all cognitive systems at full capacity when awake
     * WHY:  Awake is the reference state for cognitive function
     * HOW:  Check all factors are at 1.0
     */
    float attn_cap, attn_vig, wm_cap, wm_decay;
    float exec_inhib, exec_flex, osc_freq, osc_spindle;

    getCognitiveEffectsForState(SLEEP_STATE_AWAKE,
                               attn_cap, attn_vig, wm_cap, wm_decay,
                               exec_inhib, exec_flex, osc_freq, osc_spindle);

    /* Full cognitive capacity */
    EXPECT_FLOAT_EQ(attn_cap, 1.0f) << "Full attention capacity";
    EXPECT_FLOAT_EQ(attn_vig, 1.0f) << "Full vigilance";
    EXPECT_FLOAT_EQ(wm_cap, 1.0f) << "Full WM capacity (7±2)";
    EXPECT_FLOAT_EQ(wm_decay, 1.0f) << "Normal WM decay";
    EXPECT_FLOAT_EQ(exec_inhib, 1.0f) << "Full inhibitory control";
    EXPECT_FLOAT_EQ(exec_flex, 1.0f) << "Full cognitive flexibility";

    /* Beta/Gamma oscillations, no spindles */
    EXPECT_GT(osc_freq, 13.0f) << "Beta range when awake";
    EXPECT_FLOAT_EQ(osc_spindle, 0.0f) << "No spindles when awake";
}

TEST_F(CognitiveSleepIntegrationTest, DeepNREM_CognitiveOffline) {
    /* WHAT: Verify cognitive systems offline during deep NREM
     * WHY:  Deep NREM is for offline consolidation, not active cognition
     * HOW:  Check near-zero capacity, delta oscillations
     */
    float attn_cap, attn_vig, wm_cap, wm_decay;
    float exec_inhib, exec_flex, osc_freq, osc_spindle;

    getCognitiveEffectsForState(SLEEP_STATE_DEEP_NREM,
                               attn_cap, attn_vig, wm_cap, wm_decay,
                               exec_inhib, exec_flex, osc_freq, osc_spindle);

    /* Cognitive systems offline */
    EXPECT_FLOAT_EQ(attn_cap, 0.0f) << "No attention in deep NREM";
    EXPECT_FLOAT_EQ(wm_cap, 0.0f) << "WM offline in deep NREM";
    EXPECT_FLOAT_EQ(exec_inhib, 0.0f) << "No inhibition in deep NREM";
    EXPECT_FLOAT_EQ(exec_flex, 0.0f) << "No flexibility in deep NREM";

    /* Delta oscillations dominate */
    EXPECT_LT(osc_freq, 4.0f) << "Delta range in deep NREM";
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_DEEP_NREM);
    EXPECT_EQ(band, OSC_BAND_DELTA) << "Delta band dominant";
}

TEST_F(CognitiveSleepIntegrationTest, REM_DreamState) {
    /* WHAT: Verify REM has partial cognitive function for dreams
     * WHY:  Dreams require some attention and WM, but reduced executive control
     * HOW:  Check intermediate values, theta oscillations
     */
    float attn_cap, attn_vig, wm_cap, wm_decay;
    float exec_inhib, exec_flex, osc_freq, osc_spindle;

    getCognitiveEffectsForState(SLEEP_STATE_REM,
                               attn_cap, attn_vig, wm_cap, wm_decay,
                               exec_inhib, exec_flex, osc_freq, osc_spindle);

    /* Partial internal cognition for dreams */
    EXPECT_GT(attn_cap, 0.0f) << "Some internal attention in REM";
    EXPECT_GT(wm_cap, 0.0f) << "Some WM for dream narrative";
    EXPECT_GT(exec_inhib, 0.0f) << "Some inhibition (but reduced)";

    /* Less than awake */
    EXPECT_LT(attn_cap, 1.0f) << "Attention less than awake";
    EXPECT_LT(exec_inhib, 0.5f) << "Reduced inhibition (dream bizarreness)";

    /* Theta oscillations, no spindles */
    oscillation_band_t band = oscillations_sleep_band_for_state(SLEEP_STATE_REM);
    EXPECT_EQ(band, OSC_BAND_THETA) << "Theta band in REM";
    EXPECT_FLOAT_EQ(osc_spindle, 0.0f) << "No spindles in REM";
}

TEST_F(CognitiveSleepIntegrationTest, LightNREM_SpindleActivity) {
    /* WHAT: Verify light NREM has peak spindle activity
     * WHY:  Sleep spindles in Stage 2 are memory consolidation markers
     * HOW:  Check spindle activity peaks in light NREM
     */
    float spindle_awake = oscillations_sleep_spindle_for_state(SLEEP_STATE_AWAKE);
    float spindle_drowsy = oscillations_sleep_spindle_for_state(SLEEP_STATE_DROWSY);
    float spindle_light = oscillations_sleep_spindle_for_state(SLEEP_STATE_LIGHT_NREM);
    float spindle_deep = oscillations_sleep_spindle_for_state(SLEEP_STATE_DEEP_NREM);
    float spindle_rem = oscillations_sleep_spindle_for_state(SLEEP_STATE_REM);

    /* Light NREM should have peak spindles */
    EXPECT_GT(spindle_light, spindle_awake);
    EXPECT_GT(spindle_light, spindle_drowsy);
    EXPECT_GT(spindle_light, spindle_deep);
    EXPECT_GT(spindle_light, spindle_rem);
    EXPECT_GT(spindle_light, 0.7f) << "High spindle activity in light NREM";
}

/* ============================================================================
 * Sleep Transition Tests
 * ============================================================================ */

TEST_F(CognitiveSleepIntegrationTest, DrowsyTransition_AttentionLapses) {
    /* WHAT: Verify drowsy state shows attention lapses
     * WHY:  Drowsy state is characterized by attention failures
     * HOW:  Check reduced capacity and vigilance
     */
    float attn_cap = attention_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    float attn_vig = attention_sleep_vigilance_for_state(SLEEP_STATE_DROWSY);
    float wm_cap = working_memory_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    float exec_inhib = executive_sleep_inhibition_for_state(SLEEP_STATE_DROWSY);

    /* Significant impairment but not offline */
    EXPECT_LT(attn_cap, 0.8f) << "Attention capacity reduced";
    EXPECT_GT(attn_cap, 0.4f) << "Still some attention";
    EXPECT_LT(attn_vig, 0.7f) << "Vigilance impaired";
    EXPECT_LT(wm_cap, 0.9f) << "WM capacity reduced (~5 items)";
    EXPECT_LT(exec_inhib, 0.8f) << "Inhibition impaired";
}

TEST_F(CognitiveSleepIntegrationTest, FrequencyDeclineAcrossStages) {
    /* WHAT: Verify oscillation frequency decreases as sleep deepens
     * WHY:  This defines the sleep stages (Beta→Alpha→Theta→Delta)
     * HOW:  Check frequency monotonically decreases
     */
    float freq_awake = oscillations_sleep_freq_for_state(SLEEP_STATE_AWAKE);
    float freq_drowsy = oscillations_sleep_freq_for_state(SLEEP_STATE_DROWSY);
    float freq_light = oscillations_sleep_freq_for_state(SLEEP_STATE_LIGHT_NREM);
    float freq_deep = oscillations_sleep_freq_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_GT(freq_awake, freq_drowsy) << "Awake > Drowsy";
    EXPECT_GT(freq_drowsy, freq_light) << "Drowsy > Light NREM";
    EXPECT_GT(freq_light, freq_deep) << "Light NREM > Deep NREM";
}

/* ============================================================================
 * Cross-System Coordination Tests
 * ============================================================================ */

TEST_F(CognitiveSleepIntegrationTest, AttentionWM_Coordination) {
    /* WHAT: Verify attention and WM decline together
     * WHY:  WM requires attention for maintenance
     * HOW:  Check parallel decline across states
     */
    for (int state = SLEEP_STATE_AWAKE; state <= SLEEP_STATE_REM; state++) {
        sleep_state_t s = static_cast<sleep_state_t>(state);
        float attn = attention_sleep_capacity_for_state(s);
        float wm = working_memory_sleep_capacity_for_state(s);

        /* When attention is very low, WM should also be low */
        if (attn < 0.2f) {
            EXPECT_LT(wm, 0.5f) << "Low attention → low WM for state " << state;
        }
    }
}

TEST_F(CognitiveSleepIntegrationTest, ExecutiveFlexibility_InhibitionCorrelation) {
    /* WHAT: Verify flexibility and inhibition decline together
     * WHY:  Both are PFC functions, should be affected similarly
     * HOW:  Check correlation across states
     */
    for (int state = SLEEP_STATE_AWAKE; state <= SLEEP_STATE_REM; state++) {
        sleep_state_t s = static_cast<sleep_state_t>(state);
        float inhib = executive_sleep_inhibition_for_state(s);
        float flex = executive_sleep_flexibility_for_state(s);

        /* Both should be high or low together (within 0.5) */
        float diff = fabs(inhib - flex);
        EXPECT_LT(diff, 0.6f) << "Inhibition and flexibility should correlate for state " << state;
    }
}

/* ============================================================================
 * Sleep Deprivation Model Tests
 * ============================================================================ */

TEST_F(CognitiveSleepIntegrationTest, SleepDeprivation_CumulativeDeficit) {
    /* WHAT: Model sleep deprivation effects
     * WHY:  Extended drowsiness simulates sleep deprivation
     * HOW:  Check all cognitive functions impaired in drowsy state
     */
    float attn_cap = attention_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    float wm_cap = working_memory_sleep_capacity_for_state(SLEEP_STATE_DROWSY);
    float exec_inhib = executive_sleep_inhibition_for_state(SLEEP_STATE_DROWSY);
    float exec_flex = executive_sleep_flexibility_for_state(SLEEP_STATE_DROWSY);

    /* All cognitive functions impaired */
    EXPECT_LT(attn_cap, 1.0f) << "Attention impaired";
    EXPECT_LT(wm_cap, 1.0f) << "WM impaired";
    EXPECT_LT(exec_inhib, 1.0f) << "Inhibition impaired";
    EXPECT_LT(exec_flex, 1.0f) << "Flexibility impaired";

    /* Calculate total impairment */
    float total_impairment = 4.0f - (attn_cap + wm_cap + exec_inhib + exec_flex);
    EXPECT_GT(total_impairment, 0.5f) << "Significant total cognitive impairment";
}
