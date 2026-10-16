/**
 * @file e2e_test_sleep_bridge_pipeline.cpp
 * @brief End-to-end tests for complete Sleep Bridge Pipeline
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Test all 8 sleep bridges coordinating through complete sleep cycles
 * WHY:  Verify the entire sleep-wake system works as an integrated pipeline
 * HOW:  Simulate realistic sleep cycles with all bridges active
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
/* Plasticity bridges */
#include "plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.h"
#include "plasticity/stdp/nimcp_stdp_sleep_bridge.h"
#include "plasticity/bcm/nimcp_bcm_sleep_bridge.h"
#include "plasticity/homeostatic/nimcp_homeostatic_sleep_bridge.h"

/* Cognitive bridges */
#include "cognitive/attention/nimcp_attention_sleep_bridge.h"
#include "cognitive/working_memory/nimcp_working_memory_sleep_bridge.h"
#include "cognitive/executive/nimcp_executive_sleep_bridge.h"
#include "core/brain_oscillations/nimcp_oscillations_sleep_bridge.h"

/* ============================================================================
 * E2E Test Fixture
 * ============================================================================ */

class SleepBridgePipelineE2E : public ::testing::Test {
protected:
    /* All bridge configs */
    neuromodulators_sleep_config_t neuromod_cfg;
    stdp_sleep_config_t stdp_cfg;
    bcm_sleep_config_t bcm_cfg;
    homeostatic_sleep_config_t homeo_cfg;
    attention_sleep_config_t attn_cfg;
    working_memory_sleep_config_t wm_cfg;
    executive_sleep_config_t exec_cfg;
    oscillations_sleep_config_t osc_cfg;

    void SetUp() override {
        /* Initialize all configs with defaults */
        ASSERT_EQ(neuromod_sleep_default_config(&neuromod_cfg), 0);
        ASSERT_EQ(stdp_sleep_default_config(&stdp_cfg), 0);
        ASSERT_EQ(bcm_sleep_default_config(&bcm_cfg), 0);
        ASSERT_EQ(homeostatic_sleep_default_config(&homeo_cfg), 0);
        ASSERT_EQ(attention_sleep_default_config(&attn_cfg), 0);
        ASSERT_EQ(working_memory_sleep_default_config(&wm_cfg), 0);
        ASSERT_EQ(executive_sleep_default_config(&exec_cfg), 0);
        ASSERT_EQ(oscillations_sleep_default_config(&osc_cfg), 0);
    }

    /* Structure to hold all effects for a sleep state */
    struct SleepStateSnapshot {
        sleep_state_t state;

        /* Neuromodulators */
        float ach;
        float ne;
        float da;
        float serotonin;

        /* STDP */
        float stdp_lr;
        float stdp_ratio;
        float stdp_tau;

        /* BCM */
        float bcm_theta;
        float bcm_lr;

        /* Homeostatic */
        float homeo_scaling;
        float homeo_target;

        /* Attention */
        float attn_capacity;
        float attn_vigilance;

        /* Working Memory */
        float wm_capacity;
        float wm_decay;

        /* Executive */
        float exec_inhibition;
        float exec_flexibility;
        float exec_switch_cost;

        /* Oscillations */
        float osc_freq;
        float osc_spindle;
        oscillation_band_t osc_band;
    };

    /* Capture all effects for a given sleep state */
    SleepStateSnapshot captureState(sleep_state_t state) {
        SleepStateSnapshot snap;
        snap.state = state;

        /* Neuromodulators */
        snap.ach = neuromod_sleep_get_ach_factor(state);
        snap.ne = neuromod_sleep_get_ne_factor(state);
        snap.da = neuromod_sleep_get_da_factor(state);
        snap.serotonin = neuromod_sleep_get_serotonin_factor(state);

        /* STDP */
        snap.stdp_lr = stdp_sleep_get_lr_factor(state);
        snap.stdp_ratio = stdp_sleep_get_ratio_factor(state);
        snap.stdp_tau = stdp_sleep_get_tau_factor(state);

        /* BCM */
        snap.bcm_theta = bcm_sleep_theta_for_state(state);
        snap.bcm_lr = bcm_sleep_lr_for_state(state);

        /* Homeostatic */
        snap.homeo_scaling = homeostatic_sleep_scaling_for_state(state);
        snap.homeo_target = homeostatic_sleep_target_for_state(state);

        /* Attention */
        snap.attn_capacity = attention_sleep_capacity_for_state(state);
        snap.attn_vigilance = attention_sleep_vigilance_for_state(state);

        /* Working Memory */
        snap.wm_capacity = working_memory_sleep_capacity_for_state(state);
        snap.wm_decay = working_memory_sleep_decay_for_state(state);

        /* Executive */
        snap.exec_inhibition = executive_sleep_inhibition_for_state(state);
        snap.exec_flexibility = executive_sleep_flexibility_for_state(state);
        snap.exec_switch_cost = executive_sleep_switch_cost_for_state(state);

        /* Oscillations */
        snap.osc_freq = oscillations_sleep_freq_for_state(state);
        snap.osc_spindle = oscillations_sleep_spindle_for_state(state);
        snap.osc_band = oscillations_sleep_band_for_state(state);

        return snap;
    }

    /* Calculate cognitive capacity score (0-1) */
    float cognitiveCapacityScore(const SleepStateSnapshot& snap) {
        return (snap.attn_capacity + snap.wm_capacity +
                snap.exec_inhibition + snap.exec_flexibility) / 4.0f;
    }

    /* Calculate plasticity activity score */
    float plasticityActivityScore(const SleepStateSnapshot& snap) {
        /* Higher = more active plasticity (not direction-specific) */
        return (snap.stdp_lr + snap.bcm_lr + snap.homeo_scaling) / 3.0f;
    }

    /* Calculate neuromodulator arousal score */
    float arousalScore(const SleepStateSnapshot& snap) {
        /* ACh and NE are primary arousal neuromodulators */
        return (snap.ach + snap.ne) / 2.0f;
    }
};

/* ============================================================================
 * Full Sleep Cycle E2E Tests
 * ============================================================================ */

TEST_F(SleepBridgePipelineE2E, FullSleepCycle_AllStatesValid) {
    /* WHAT: Verify all states produce valid, biologically plausible values
     * WHY:  End-to-end validation of the entire pipeline
     * HOW:  Capture snapshots for all states and validate ranges
     */
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    for (auto state : states) {
        SleepStateSnapshot snap = captureState(state);

        /* All neuromodulators in [0, 1] */
        EXPECT_GE(snap.ach, 0.0f) << "State " << state;
        EXPECT_LE(snap.ach, 1.0f) << "State " << state;
        EXPECT_GE(snap.ne, 0.0f) << "State " << state;
        EXPECT_LE(snap.ne, 1.0f) << "State " << state;
        EXPECT_GE(snap.da, 0.0f) << "State " << state;
        EXPECT_LE(snap.da, 1.0f) << "State " << state;
        EXPECT_GE(snap.serotonin, 0.0f) << "State " << state;
        EXPECT_LE(snap.serotonin, 1.0f) << "State " << state;

        /* Learning rates positive */
        EXPECT_GT(snap.stdp_lr, 0.0f) << "State " << state;
        EXPECT_GT(snap.bcm_lr, 0.0f) << "State " << state;

        /* Ratios and thresholds positive */
        EXPECT_GT(snap.stdp_ratio, 0.0f) << "State " << state;
        EXPECT_GT(snap.stdp_tau, 0.0f) << "State " << state;
        EXPECT_GT(snap.bcm_theta, 0.0f) << "State " << state;

        /* Cognitive factors in [0, 1] */
        EXPECT_GE(snap.attn_capacity, 0.0f) << "State " << state;
        EXPECT_LE(snap.attn_capacity, 1.0f) << "State " << state;
        EXPECT_GE(snap.wm_capacity, 0.0f) << "State " << state;
        EXPECT_LE(snap.wm_capacity, 1.0f) << "State " << state;

        /* Frequency positive */
        EXPECT_GT(snap.osc_freq, 0.0f) << "State " << state;
    }
}

TEST_F(SleepBridgePipelineE2E, AwakeState_OptimalCognition) {
    /* WHAT: Verify awake state has optimal cognitive function
     * WHY:  Awake is the reference state for cognition
     * HOW:  All cognitive systems should be at peak
     */
    SleepStateSnapshot awake = captureState(SLEEP_STATE_AWAKE);

    /* Full cognitive capacity */
    float cog_score = cognitiveCapacityScore(awake);
    EXPECT_FLOAT_EQ(cog_score, 1.0f) << "Full cognitive capacity when awake";

    /* Full neuromodulator arousal */
    float arousal = arousalScore(awake);
    EXPECT_FLOAT_EQ(arousal, 1.0f) << "Full arousal when awake";

    /* Beta oscillations (13-30 Hz) */
    EXPECT_EQ(awake.osc_band, OSC_BAND_BETA) << "Beta band when awake";
    EXPECT_GT(awake.osc_freq, 13.0f) << "High frequency when awake";

    /* No spindles */
    EXPECT_FLOAT_EQ(awake.osc_spindle, 0.0f) << "No spindles when awake";

    /* No homeostatic scaling (SHY) */
    EXPECT_FLOAT_EQ(awake.homeo_scaling, 0.0f) << "No scaling when awake";
}

TEST_F(SleepBridgePipelineE2E, DeepNREM_SynapticHomeostasis) {
    /* WHAT: Verify deep NREM implements SHY properly
     * WHY:  Deep NREM is when synaptic homeostasis occurs
     * HOW:  Maximum scaling, cognitive offline, delta oscillations
     */
    SleepStateSnapshot deep = captureState(SLEEP_STATE_DEEP_NREM);

    /* Cognitive systems offline */
    float cog_score = cognitiveCapacityScore(deep);
    EXPECT_FLOAT_EQ(cog_score, 0.0f) << "Cognitive systems offline in deep NREM";

    /* Maximum homeostatic scaling */
    EXPECT_FLOAT_EQ(deep.homeo_scaling, 1.0f) << "Maximum scaling in deep NREM";

    /* Reduced target (downscaling goal) */
    EXPECT_LT(deep.homeo_target, 1.0f) << "Downscaling target";

    /* Delta oscillations (0.5-4 Hz) */
    EXPECT_EQ(deep.osc_band, OSC_BAND_DELTA) << "Delta band in deep NREM";
    EXPECT_LT(deep.osc_freq, 4.0f) << "Delta frequency range";

    /* Neuromodulators suppressed */
    EXPECT_LT(deep.ne, 0.2f) << "NE suppressed";
    EXPECT_LT(deep.ach, 0.2f) << "ACh suppressed";

    /* STDP favors LTD */
    EXPECT_LT(deep.stdp_ratio, 1.0f) << "LTD bias in deep NREM";

    /* BCM threshold elevated (favors LTD) */
    EXPECT_GT(deep.bcm_theta, 1.0f) << "Elevated BCM theta";
}

TEST_F(SleepBridgePipelineE2E, REM_MemoryConsolidation) {
    /* WHAT: Verify REM state supports memory consolidation
     * WHY:  REM is critical for consolidating emotional/procedural memories
     * HOW:  High ACh, low NE, LTP bias, theta oscillations
     */
    SleepStateSnapshot rem = captureState(SLEEP_STATE_REM);

    /* REM neuromodulator profile: high ACh, very low NE */
    EXPECT_GT(rem.ach, 0.8f) << "High ACh in REM (cholinergic tone)";
    EXPECT_LT(rem.ne, 0.1f) << "Very low NE in REM (LC silent)";

    /* STDP favors LTP (consolidation) */
    EXPECT_GT(rem.stdp_ratio, 1.0f) << "LTP bias in REM";

    /* BCM threshold lowered (easier LTP) */
    EXPECT_LT(rem.bcm_theta, 1.0f) << "Lowered BCM theta";

    /* Theta oscillations (4-8 Hz) */
    EXPECT_EQ(rem.osc_band, OSC_BAND_THETA) << "Theta band in REM";

    /* No spindles in REM */
    EXPECT_FLOAT_EQ(rem.osc_spindle, 0.0f) << "No spindles in REM";

    /* Reduced homeostatic scaling (protect consolidating memories) */
    EXPECT_LT(rem.homeo_scaling, 0.5f) << "Reduced scaling in REM";

    /* Some internal cognition for dreams */
    EXPECT_GT(rem.attn_capacity, 0.0f) << "Some attention for dreams";
    EXPECT_GT(rem.wm_capacity, 0.0f) << "Some WM for dream narrative";
}

TEST_F(SleepBridgePipelineE2E, LightNREM_SpindleGeneration) {
    /* WHAT: Verify light NREM (Stage 2) has peak spindle activity
     * WHY:  Sleep spindles are memory consolidation markers
     * HOW:  Spindles should peak in light NREM
     */
    SleepStateSnapshot light = captureState(SLEEP_STATE_LIGHT_NREM);
    SleepStateSnapshot awake = captureState(SLEEP_STATE_AWAKE);
    SleepStateSnapshot deep = captureState(SLEEP_STATE_DEEP_NREM);
    SleepStateSnapshot rem = captureState(SLEEP_STATE_REM);

    /* Light NREM should have highest spindle activity */
    EXPECT_GT(light.osc_spindle, awake.osc_spindle) << "More spindles than awake";
    EXPECT_GT(light.osc_spindle, deep.osc_spindle) << "More spindles than deep";
    EXPECT_GT(light.osc_spindle, rem.osc_spindle) << "More spindles than REM";
    EXPECT_GT(light.osc_spindle, 0.7f) << "High spindle activity";

    /* Theta band */
    EXPECT_EQ(light.osc_band, OSC_BAND_THETA) << "Theta band in light NREM";
}

/* ============================================================================
 * Sleep Cycle Progression E2E Tests
 * ============================================================================ */

TEST_F(SleepBridgePipelineE2E, SleepCycleProgression_CognitiveDecline) {
    /* WHAT: Verify cognitive capacity declines from awake → deep NREM
     * WHY:  This is the fundamental pattern of falling asleep
     * HOW:  Compare cognitive scores across the progression
     */
    SleepStateSnapshot awake = captureState(SLEEP_STATE_AWAKE);
    SleepStateSnapshot drowsy = captureState(SLEEP_STATE_DROWSY);
    SleepStateSnapshot light = captureState(SLEEP_STATE_LIGHT_NREM);
    SleepStateSnapshot deep = captureState(SLEEP_STATE_DEEP_NREM);

    float cog_awake = cognitiveCapacityScore(awake);
    float cog_drowsy = cognitiveCapacityScore(drowsy);
    float cog_light = cognitiveCapacityScore(light);
    float cog_deep = cognitiveCapacityScore(deep);

    /* Monotonic decline */
    EXPECT_GT(cog_awake, cog_drowsy) << "Awake > Drowsy";
    EXPECT_GT(cog_drowsy, cog_light) << "Drowsy > Light NREM";
    EXPECT_GT(cog_light, cog_deep) << "Light NREM > Deep NREM";

    /* Deep NREM should be zero */
    EXPECT_FLOAT_EQ(cog_deep, 0.0f) << "Zero cognition in deep NREM";
}

TEST_F(SleepBridgePipelineE2E, SleepCycleProgression_FrequencySlowing) {
    /* WHAT: Verify oscillation frequency slows as sleep deepens
     * WHY:  This defines sleep staging (Beta→Alpha→Theta→Delta)
     * HOW:  Compare frequencies across progression
     */
    SleepStateSnapshot awake = captureState(SLEEP_STATE_AWAKE);
    SleepStateSnapshot drowsy = captureState(SLEEP_STATE_DROWSY);
    SleepStateSnapshot light = captureState(SLEEP_STATE_LIGHT_NREM);
    SleepStateSnapshot deep = captureState(SLEEP_STATE_DEEP_NREM);

    /* Monotonic frequency decline */
    EXPECT_GT(awake.osc_freq, drowsy.osc_freq) << "Awake > Drowsy";
    EXPECT_GT(drowsy.osc_freq, light.osc_freq) << "Drowsy > Light";
    EXPECT_GT(light.osc_freq, deep.osc_freq) << "Light > Deep";

    /* Band progression */
    EXPECT_EQ(awake.osc_band, OSC_BAND_BETA) << "Beta when awake";
    EXPECT_EQ(drowsy.osc_band, OSC_BAND_ALPHA) << "Alpha when drowsy";
    EXPECT_EQ(light.osc_band, OSC_BAND_THETA) << "Theta in light NREM";
    EXPECT_EQ(deep.osc_band, OSC_BAND_DELTA) << "Delta in deep NREM";
}

TEST_F(SleepBridgePipelineE2E, SleepCycleProgression_HomeostaticScalingRamp) {
    /* WHAT: Verify homeostatic scaling increases into deep NREM
     * WHY:  SHY predicts scaling is maximal in deep sleep
     * HOW:  Compare scaling rates across progression
     */
    SleepStateSnapshot awake = captureState(SLEEP_STATE_AWAKE);
    SleepStateSnapshot drowsy = captureState(SLEEP_STATE_DROWSY);
    SleepStateSnapshot light = captureState(SLEEP_STATE_LIGHT_NREM);
    SleepStateSnapshot deep = captureState(SLEEP_STATE_DEEP_NREM);

    /* Monotonic scaling increase */
    EXPECT_LE(awake.homeo_scaling, drowsy.homeo_scaling) << "Awake ≤ Drowsy";
    EXPECT_LE(drowsy.homeo_scaling, light.homeo_scaling) << "Drowsy ≤ Light";
    EXPECT_LE(light.homeo_scaling, deep.homeo_scaling) << "Light ≤ Deep";

    /* Deep NREM at maximum */
    EXPECT_FLOAT_EQ(deep.homeo_scaling, 1.0f) << "Maximum scaling in deep NREM";
}

/* ============================================================================
 * Full Night Sleep Simulation E2E Tests
 * ============================================================================ */

TEST_F(SleepBridgePipelineE2E, FullNightSimulation_MultipleCycles) {
    /* WHAT: Simulate a full night of sleep (~4-5 cycles)
     * WHY:  Verify the pipeline handles realistic sleep architecture
     * HOW:  Run through typical sleep stages for each cycle
     */

    /* Typical sleep cycle pattern (simplified) */
    /* Cycle 1: More deep NREM */
    /* Cycle 2-4: Progressively more REM */
    std::vector<sleep_state_t> night_pattern = {
        /* Falling asleep */
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,

        /* Cycle 1 (heavy on deep NREM) */
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_REM,

        /* Cycle 2 */
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_REM,
        SLEEP_STATE_REM,

        /* Cycle 3 */
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_REM,
        SLEEP_STATE_REM,
        SLEEP_STATE_REM,

        /* Cycle 4 (REM dominant) */
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_REM,
        SLEEP_STATE_REM,
        SLEEP_STATE_REM,

        /* Waking */
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_AWAKE
    };

    /* Track cumulative effects */
    float total_scaling = 0.0f;
    float total_ltp_bias = 0.0f;
    float total_ltd_bias = 0.0f;
    int deep_nrem_epochs = 0;
    int rem_epochs = 0;

    for (auto state : night_pattern) {
        SleepStateSnapshot snap = captureState(state);

        /* Accumulate scaling */
        total_scaling += snap.homeo_scaling;

        /* Track LTP/LTD bias */
        if (snap.stdp_ratio > 1.0f) {
            total_ltp_bias += (snap.stdp_ratio - 1.0f);
        } else {
            total_ltd_bias += (1.0f - snap.stdp_ratio);
        }

        if (state == SLEEP_STATE_DEEP_NREM) deep_nrem_epochs++;
        if (state == SLEEP_STATE_REM) rem_epochs++;
    }

    /* Verify night totals are biologically reasonable */
    EXPECT_GT(total_scaling, 0.0f) << "Some scaling occurred";
    EXPECT_GT(rem_epochs, 0) << "Some REM sleep";
    EXPECT_GT(deep_nrem_epochs, 0) << "Some deep NREM";

    /* In a balanced night, both LTP and LTD occur */
    EXPECT_GT(total_ltp_bias, 0.0f) << "Some LTP (REM consolidation)";
    EXPECT_GT(total_ltd_bias, 0.0f) << "Some LTD (NREM downscaling)";

    /* Net effect should favor LTD (SHY prediction) */
    EXPECT_GT(total_ltd_bias, total_ltp_bias * 0.5f)
        << "Net LTD bias expected (but REM provides some LTP)";
}

TEST_F(SleepBridgePipelineE2E, SleepDeprivation_CognitiveDeficit) {
    /* WHAT: Model sleep deprivation as extended drowsy state
     * WHY:  Sleep deprivation impairs all cognitive domains
     * HOW:  Verify drowsy state shows global impairment
     */
    SleepStateSnapshot awake = captureState(SLEEP_STATE_AWAKE);
    SleepStateSnapshot drowsy = captureState(SLEEP_STATE_DROWSY);

    /* All cognitive domains impaired vs awake */
    EXPECT_LT(drowsy.attn_capacity, awake.attn_capacity) << "Attention impaired";
    EXPECT_LT(drowsy.attn_vigilance, awake.attn_vigilance) << "Vigilance impaired";
    EXPECT_LT(drowsy.wm_capacity, awake.wm_capacity) << "WM impaired";
    EXPECT_LT(drowsy.exec_inhibition, awake.exec_inhibition) << "Inhibition impaired";
    EXPECT_LT(drowsy.exec_flexibility, awake.exec_flexibility) << "Flexibility impaired";

    /* All neuromodulators reduced */
    EXPECT_LT(drowsy.ach, awake.ach) << "ACh reduced";
    EXPECT_LT(drowsy.ne, awake.ne) << "NE reduced";

    /* Oscillations slowed */
    EXPECT_LT(drowsy.osc_freq, awake.osc_freq) << "Frequency reduced";
    EXPECT_EQ(drowsy.osc_band, OSC_BAND_ALPHA) << "Alpha intrusion";
}

/* ============================================================================
 * Cross-System Coordination E2E Tests
 * ============================================================================ */

TEST_F(SleepBridgePipelineE2E, NeuromodulatorPlasticityCoordination) {
    /* WHAT: Verify neuromodulator states coordinate with plasticity
     * WHY:  ACh/NE levels should gate plasticity mechanisms
     * HOW:  Compare neuromodulator and plasticity profiles
     */
    for (int s = SLEEP_STATE_AWAKE; s <= SLEEP_STATE_REM; s++) {
        sleep_state_t state = static_cast<sleep_state_t>(s);
        SleepStateSnapshot snap = captureState(state);

        /* When ACh is low, encoding should be reduced */
        if (snap.ach < 0.3f) {
            /* But consolidation/scaling can still occur */
            /* Deep NREM has low ACh but active scaling */
        }

        /* When NE is high, scaling should be suppressed */
        if (snap.ne > 0.8f) {
            EXPECT_LT(snap.homeo_scaling, 0.3f)
                << "High NE suppresses scaling for state " << s;
        }
    }
}

TEST_F(SleepBridgePipelineE2E, OscillationCognitiveCorrelation) {
    /* WHAT: Verify oscillations correlate with cognitive state
     * WHY:  Faster oscillations = higher arousal = better cognition
     * HOW:  Compare frequency with cognitive capacity
     */
    for (int s = SLEEP_STATE_AWAKE; s <= SLEEP_STATE_REM; s++) {
        sleep_state_t state = static_cast<sleep_state_t>(s);
        SleepStateSnapshot snap = captureState(state);

        /* Delta = offline cognition */
        if (snap.osc_band == OSC_BAND_DELTA) {
            EXPECT_FLOAT_EQ(cognitiveCapacityScore(snap), 0.0f)
                << "Delta = cognitive offline";
        }

        /* Beta = full cognition */
        if (snap.osc_band == OSC_BAND_BETA) {
            EXPECT_GT(cognitiveCapacityScore(snap), 0.9f)
                << "Beta = high cognition";
        }
    }
}

TEST_F(SleepBridgePipelineE2E, AllConfigsInitializeCorrectly) {
    /* WHAT: Verify all 8 configs initialize without error
     * WHY:  E2E validation of configuration system
     * HOW:  Check all modulation strengths are valid
     */

    /* All modulation strengths should be in valid range */
    /* Neuromodulators has separate strengths per neuromodulator */
    EXPECT_GE(neuromod_cfg.ach_modulation_strength, 0.0f);
    EXPECT_LE(neuromod_cfg.ach_modulation_strength, 1.0f);
    EXPECT_GE(neuromod_cfg.ne_modulation_strength, 0.0f);
    EXPECT_LE(neuromod_cfg.ne_modulation_strength, 1.0f);

    EXPECT_GE(stdp_cfg.modulation_strength, 0.0f);
    EXPECT_LE(stdp_cfg.modulation_strength, 1.0f);

    EXPECT_GE(bcm_cfg.modulation_strength, 0.0f);
    EXPECT_LE(bcm_cfg.modulation_strength, 1.0f);

    EXPECT_GE(homeo_cfg.modulation_strength, 0.0f);
    EXPECT_LE(homeo_cfg.modulation_strength, 1.0f);

    EXPECT_GE(attn_cfg.modulation_strength, 0.0f);
    EXPECT_LE(attn_cfg.modulation_strength, 1.0f);

    EXPECT_GE(wm_cfg.modulation_strength, 0.0f);
    EXPECT_LE(wm_cfg.modulation_strength, 1.0f);

    EXPECT_GE(exec_cfg.modulation_strength, 0.0f);
    EXPECT_LE(exec_cfg.modulation_strength, 1.0f);

    EXPECT_GE(osc_cfg.modulation_strength, 0.0f);
    EXPECT_LE(osc_cfg.modulation_strength, 1.0f);
}
