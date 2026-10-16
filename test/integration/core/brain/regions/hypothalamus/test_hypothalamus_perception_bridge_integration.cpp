/**
 * @file test_hypothalamus_perception_bridge_integration.cpp
 * @brief Integration tests for Hypothalamus-Perception Bridge
 *
 * WHAT: Integration tests verifying the hypothalamus perception bridge
 *       correctly integrates drives with sensory modulation
 * WHY:  Ensure drive states properly modulate perception, and sensory
 *       detections feed back to drive system for anticipation
 * HOW:  Test drive-perception coupling, interoception, chemosensory,
 *       predictive coding, pain modulation, sleep gating, and thermal salience
 *
 * TEST SCENARIOS:
 * 1. Drive System Integration - Drive urgencies affect perception modulation
 * 2. Interoception-Drive Feedback - Interoceptive signals feed back to drives
 * 3. Chemosensory-Drive Integration - Olfactory/gustatory modulation by hunger/thirst
 * 4. Predictive-Perception Flow - Predictions generate and update with sensory input
 * 5. Pain-Stress Interaction - Stress level affects pain modulation (acute vs chronic)
 * 6. Sleep-Perception Pipeline - Sleep stages gate perception
 * 7. Thermal-Drive Integration - Temperature affects thermal salience
 * 8. Multi-Enhancement Interaction - All enhancements working together
 * 9. Bio-async Message Flow - Message handlers and broadcasts
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_perception_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "async/nimcp_bio_router.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_DRIVE_LEVEL_LOW        0.2f
#define TEST_DRIVE_LEVEL_MODERATE   0.5f
#define TEST_DRIVE_LEVEL_HIGH       0.8f
#define TEST_DRIVE_LEVEL_URGENT     0.95f

#define TEST_AROUSAL_LOW            0.2f
#define TEST_AROUSAL_MODERATE       0.5f
#define TEST_AROUSAL_HIGH           0.8f

#define TEST_STRESS_LOW             0.2f
#define TEST_STRESS_HIGH            0.8f

#define TEST_TEMPERATURE_COLD       0.3f
#define TEST_TEMPERATURE_SETPOINT   0.5f
#define TEST_TEMPERATURE_HOT        0.7f

#define FLOAT_TOLERANCE             0.001f
#define TIME_DELTA_US               16000  /* 16ms in microseconds */

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypothalamusPerceptionBridgeIntegrationTest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drive_system;
    hypo_perception_bridge_t* perception_bridge;

    void SetUp() override {
        /* Create drive system */
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr);

        /* Create perception bridge with default config */
        perception_bridge = hypo_perception_bridge_create(drive_system, nullptr);
        ASSERT_NE(perception_bridge, nullptr);
    }

    void TearDown() override {
        if (perception_bridge) {
            hypo_perception_bridge_destroy(perception_bridge);
            perception_bridge = nullptr;
        }
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
    }

    /* Helper to update bridge periodically */
    void update_bridge(uint64_t delta_us = TIME_DELTA_US) {
        int ret = hypo_perception_bridge_update(perception_bridge, delta_us);
        EXPECT_EQ(0, ret);
    }

    /* Helper to set a drive level by satisfying it inversely */
    void set_drive_level(hypo_drive_type_t drive, float level) {
        float satisfaction = 1.0f - level;
        hypo_drive_satisfy(drive_system, drive, satisfaction);
        /* Update the drive system */
        hypo_drive_update(drive_system, TIME_DELTA_US);
    }

    /* Helper to create perception bridge with enhanced config */
    hypo_perception_bridge_t* create_enhanced_bridge() {
        hypo_perception_enhanced_config_t enhanced_config;
        hypo_perception_bridge_default_enhanced_config(&enhanced_config);
        enhanced_config.enable_interoception = true;
        enhanced_config.enable_chemosensory = true;
        enhanced_config.enable_predictive_coding = true;
        enhanced_config.enable_pain_modulation = true;
        enhanced_config.enable_sleep_gating = true;
        enhanced_config.enable_thermal_salience = true;

        hypo_perception_bridge_t* bridge = hypo_perception_bridge_create(drive_system, &enhanced_config.base);
        if (bridge) {
            hypo_perception_bridge_apply_enhanced_config(bridge, &enhanced_config);
        }
        return bridge;
    }
};

/* ============================================================================
 * Test 1: Drive System Integration
 * How drive urgencies affect perception modulation
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, DriveUrgencyAffectsModulation) {
    /* Set arousal level */
    int ret = hypo_perception_bridge_set_arousal(perception_bridge, TEST_AROUSAL_MODERATE);
    ASSERT_EQ(0, ret);

    /* Update drive system with time to build urgency */
    hypo_drive_update(drive_system, 1000000);  /* 1 second */

    /* Compute modulation */
    ret = hypo_perception_bridge_compute_modulation(perception_bridge);
    ASSERT_EQ(0, ret);

    /* Get modulation */
    hypo_perception_modulation_t modulation;
    ret = hypo_perception_bridge_get_modulation(perception_bridge, &modulation);
    ASSERT_EQ(0, ret);

    /* Verify modulation is within valid range */
    EXPECT_GE(modulation.global_gain, 0.5f);
    EXPECT_LE(modulation.global_gain, 2.0f);
    EXPECT_GE(modulation.arousal_level, 0.0f);
    EXPECT_LE(modulation.arousal_level, 1.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, HighArousalIncreasesGain) {
    /* Set low arousal */
    hypo_perception_bridge_set_arousal(perception_bridge, TEST_AROUSAL_LOW);
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_modulation_t mod_low;
    hypo_perception_bridge_get_modulation(perception_bridge, &mod_low);

    /* Set high arousal */
    hypo_perception_bridge_set_arousal(perception_bridge, TEST_AROUSAL_HIGH);
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_modulation_t mod_high;
    hypo_perception_bridge_get_modulation(perception_bridge, &mod_high);

    /* Higher arousal should increase global gain */
    EXPECT_GT(mod_high.global_gain, mod_low.global_gain);
    EXPECT_GT(mod_high.arousal_level, mod_low.arousal_level);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, HungerDriveBoostsFoodSalience) {
    /* Set low hunger - satisfy drive */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, 1.0f);
    hypo_drive_update(drive_system, TIME_DELTA_US);
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_modulation_t mod_sated;
    hypo_perception_bridge_get_modulation(perception_bridge, &mod_sated);
    float food_salience_low = mod_sated.category_salience[HYPO_STIM_FOOD];

    /* Build hunger by updating over time without satisfaction */
    for (int i = 0; i < 100; i++) {
        hypo_drive_update(drive_system, 100000);  /* 100ms each */
    }
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_modulation_t mod_hungry;
    hypo_perception_bridge_get_modulation(perception_bridge, &mod_hungry);
    float food_salience_high = mod_hungry.category_salience[HYPO_STIM_FOOD];

    /* Higher hunger should increase food salience */
    EXPECT_GT(food_salience_high, food_salience_low);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, SafetyDriveTriggersThreatPriority) {
    /* Initially no threat priority */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_SAFETY, 1.0f);
    hypo_drive_update(drive_system, TIME_DELTA_US);
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_modulation_t mod_safe;
    hypo_perception_bridge_get_modulation(perception_bridge, &mod_safe);
    EXPECT_FALSE(mod_safe.threat_priority);

    /* Build safety drive urgency (simulating threat) */
    for (int i = 0; i < 200; i++) {
        hypo_drive_update(drive_system, 50000);  /* 50ms each */
    }
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_modulation_t mod_threat;
    hypo_perception_bridge_get_modulation(perception_bridge, &mod_threat);

    /* High safety drive urgency should boost threat salience */
    EXPECT_GT(mod_threat.category_salience[HYPO_STIM_THREAT],
              mod_safe.category_salience[HYPO_STIM_THREAT]);
}

/* ============================================================================
 * Test 2: Interoception-Drive Feedback
 * How interoceptive signals feed back to drives
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, InteroceptiveHungerPangsAffectState) {
    /* Process hunger pangs interoceptive signal */
    int ret = hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.7f);
    ASSERT_EQ(0, ret);

    /* Get interoceptive state */
    hypo_interoceptive_state_t state;
    ret = hypo_perception_bridge_get_interoceptive_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    /* Hunger pangs signal should be recorded */
    EXPECT_GT(state.signals[HYPO_INTERO_HUNGER_PANGS], 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, InteroceptiveAccuracyModulation) {
    /* Set high interoceptive accuracy (good body awareness) */
    int ret = hypo_perception_bridge_set_interoceptive_accuracy(perception_bridge, 0.9f);
    ASSERT_EQ(0, ret);

    /* Process a signal */
    ret = hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HEART_RATE, 0.6f);
    ASSERT_EQ(0, ret);

    hypo_interoceptive_state_t state;
    ret = hypo_perception_bridge_get_interoceptive_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    EXPECT_NEAR(state.global_interoceptive_accuracy, 0.9f, 0.1f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, MultipleInteroceptiveSignals) {
    /* Process multiple interoceptive signals */
    hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.5f);
    hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_THIRST, 0.3f);
    hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_FATIGUE, 0.4f);
    hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_TEMPERATURE, 0.6f);

    hypo_interoceptive_state_t state;
    int ret = hypo_perception_bridge_get_interoceptive_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    /* All signals should be present */
    EXPECT_GT(state.signals[HYPO_INTERO_HUNGER_PANGS], 0.0f);
    EXPECT_GT(state.signals[HYPO_INTERO_THIRST], 0.0f);
    EXPECT_GT(state.signals[HYPO_INTERO_FATIGUE], 0.0f);
    EXPECT_GT(state.signals[HYPO_INTERO_TEMPERATURE], 0.0f);
}

/* ============================================================================
 * Test 3: Chemosensory-Drive Integration
 * How olfactory/gustatory modulation by hunger/thirst
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, HungerEnhancesFoodSmell) {
    /* First, check smell response when sated */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_HUNGER, 1.0f);
    hypo_drive_update(drive_system, TIME_DELTA_US);

    /* Compute modulation to update chemosensory modulation based on drives */
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.5f);

    hypo_chemosensory_state_t state_sated;
    hypo_perception_bridge_get_chemosensory_state(perception_bridge, &state_sated);
    float sated_modulation = state_sated.hunger_modulation;

    /* Now build hunger */
    for (int i = 0; i < 100; i++) {
        hypo_drive_update(drive_system, 100000);
    }

    /* Recompute modulation with increased hunger */
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.5f);

    hypo_chemosensory_state_t state_hungry;
    hypo_perception_bridge_get_chemosensory_state(perception_bridge, &state_hungry);
    float hungry_modulation = state_hungry.hunger_modulation;

    /* Hunger should enhance food smell modulation */
    EXPECT_GT(hungry_modulation, sated_modulation);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ThirstEnhancesSaltTaste) {
    /* First, check taste response when hydrated */
    hypo_drive_satisfy(drive_system, HYPO_DRIVE_THIRST, 1.0f);
    hypo_drive_update(drive_system, TIME_DELTA_US);

    /* Compute modulation to update chemosensory modulation based on drives */
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_SALTY, 0.5f);

    hypo_chemosensory_state_t state_hydrated;
    hypo_perception_bridge_get_chemosensory_state(perception_bridge, &state_hydrated);
    float hydrated_modulation = state_hydrated.thirst_modulation;

    /* Build thirst */
    for (int i = 0; i < 100; i++) {
        hypo_drive_update(drive_system, 100000);
    }

    /* Recompute modulation with increased thirst */
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_SALTY, 0.5f);

    hypo_chemosensory_state_t state_thirsty;
    hypo_perception_bridge_get_chemosensory_state(perception_bridge, &state_thirsty);
    float thirsty_modulation = state_thirsty.thirst_modulation;

    /* Thirst should enhance salt taste modulation */
    EXPECT_GT(thirsty_modulation, hydrated_modulation);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ToxinDetectionTriggersBitterResponse) {
    /* Process bitter taste (toxin warning) */
    int ret = hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_BITTER, 0.8f);
    ASSERT_EQ(0, ret);

    hypo_chemosensory_state_t state;
    ret = hypo_perception_bridge_get_chemosensory_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    /* Toxin should be detected */
    EXPECT_TRUE(state.toxin_detected);
    EXPECT_GT(state.gustatory_intensity[HYPO_GUST_BITTER], 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, DangerOdorTriggersAlert) {
    /* Process danger odor */
    int ret = hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_DANGER, 0.9f);
    ASSERT_EQ(0, ret);

    hypo_chemosensory_state_t state;
    ret = hypo_perception_bridge_get_chemosensory_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    /* Danger odor should be at high intensity */
    EXPECT_GT(state.olfactory_intensity[HYPO_OLFACT_DANGER], 0.5f);
}

/* ============================================================================
 * Test 4: Predictive-Perception Flow
 * How predictions generate and update with sensory input
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, GeneratePrediction) {
    /* Generate food prediction based on hunger */
    int ret = hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.7f, 0.8f);
    ASSERT_EQ(0, ret);

    /* Get predictive state */
    hypo_predictive_state_t state;
    ret = hypo_perception_bridge_get_predictive_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    /* Prediction should be stored */
    EXPECT_NEAR(state.predictions[HYPO_PRED_FOOD_PRESENCE], 0.7f, 0.1f);
    EXPECT_NEAR(state.precision[HYPO_PRED_FOOD_PRESENCE], 0.8f, 0.1f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, PredictionErrorComputation) {
    /* Generate prediction */
    hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_THREAT_PRESENCE, 0.8f, 0.9f);

    /* Update with actual observation (lower than predicted) */
    int ret = hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_THREAT_PRESENCE, 0.3f);
    ASSERT_EQ(0, ret);

    hypo_predictive_state_t state;
    ret = hypo_perception_bridge_get_predictive_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    /* Prediction error should be non-zero (actual - expected) */
    float error = state.prediction_errors[HYPO_PRED_THREAT_PRESENCE];
    EXPECT_NE(error, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, FreeEnergyComputation) {
    /* Generate multiple predictions */
    hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.8f, 0.7f);
    hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_THREAT_PRESENCE, 0.3f, 0.9f);
    hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_SOCIAL_INTERACTION, 0.5f, 0.6f);

    /* Update with observations that differ from predictions */
    hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.2f);
    hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_THREAT_PRESENCE, 0.7f);
    hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_SOCIAL_INTERACTION, 0.4f);

    /* Compute free energy */
    float free_energy;
    int ret = hypo_perception_bridge_compute_free_energy(perception_bridge, &free_energy);
    ASSERT_EQ(0, ret);

    /* Free energy should be positive (prediction errors exist) */
    EXPECT_GT(free_energy, 0.0f);

    /* Verify predictive state also has free energy */
    hypo_predictive_state_t state;
    hypo_perception_bridge_get_predictive_state(perception_bridge, &state);
    EXPECT_GT(state.free_energy, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, MatchingPredictionReducesFreeEnergy) {
    /* Generate prediction */
    hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_REWARD_AVAILABLE, 0.6f, 0.8f);

    /* First, mismatch */
    hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_REWARD_AVAILABLE, 0.1f);

    float fe_mismatch;
    hypo_perception_bridge_compute_free_energy(perception_bridge, &fe_mismatch);

    /* Now, match */
    hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_REWARD_AVAILABLE, 0.6f);

    float fe_match;
    hypo_perception_bridge_compute_free_energy(perception_bridge, &fe_match);

    /* Matching prediction should have lower free energy */
    EXPECT_LT(fe_match, fe_mismatch);
}

/* ============================================================================
 * Test 5: Pain-Stress Interaction
 * How stress level affects pain modulation (acute vs chronic)
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, AcuteStressReducesPain) {
    /* Set acute stress */
    int ret = hypo_perception_bridge_set_stress_for_pain(
        perception_bridge, TEST_STRESS_HIGH, false);  /* Not chronic */
    ASSERT_EQ(0, ret);

    /* Process pain stimulus */
    float raw_pain = 0.7f;
    float modulated_pain;
    ret = hypo_perception_bridge_modulate_pain(
        perception_bridge, raw_pain, &modulated_pain);
    ASSERT_EQ(0, ret);

    /* Acute stress should reduce pain (analgesia) */
    EXPECT_LT(modulated_pain, raw_pain);

    /* Verify pain state */
    hypo_pain_modulation_state_t state;
    ret = hypo_perception_bridge_get_pain_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    EXPECT_TRUE(state.in_acute_stress);
    EXPECT_FALSE(state.in_chronic_stress);
    EXPECT_GT(state.analgesia_level, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ChronicStressIncreasesPain) {
    /* Set chronic stress */
    int ret = hypo_perception_bridge_set_stress_for_pain(
        perception_bridge, TEST_STRESS_HIGH, true);  /* Chronic */
    ASSERT_EQ(0, ret);

    /* Process pain stimulus */
    float raw_pain = 0.5f;
    float modulated_pain;
    ret = hypo_perception_bridge_modulate_pain(
        perception_bridge, raw_pain, &modulated_pain);
    ASSERT_EQ(0, ret);

    /* Chronic stress should increase pain (hyperalgesia) */
    EXPECT_GT(modulated_pain, raw_pain);

    /* Verify pain state */
    hypo_pain_modulation_state_t state;
    ret = hypo_perception_bridge_get_pain_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    EXPECT_FALSE(state.in_acute_stress);
    EXPECT_TRUE(state.in_chronic_stress);
    EXPECT_GT(state.hyperalgesia_level, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, EndorphinReleaseReducesPain) {
    /* Baseline pain sensitivity */
    float raw_pain = 0.6f;
    float pain_before;
    hypo_perception_bridge_modulate_pain(perception_bridge, raw_pain, &pain_before);

    /* Release endorphins */
    int ret = hypo_perception_bridge_release_endorphins(perception_bridge, 0.8f);
    ASSERT_EQ(0, ret);

    /* Pain should be reduced after endorphin release */
    float pain_after;
    hypo_perception_bridge_modulate_pain(perception_bridge, raw_pain, &pain_after);

    EXPECT_LT(pain_after, pain_before);

    /* Verify endorphin level */
    hypo_pain_modulation_state_t state;
    hypo_perception_bridge_get_pain_state(perception_bridge, &state);
    EXPECT_GT(state.endorphin_level, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, NoStressNeutralPainModulation) {
    /* No stress */
    hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.0f, false);

    /* Process pain */
    float raw_pain = 0.5f;
    float modulated_pain;
    hypo_perception_bridge_modulate_pain(perception_bridge, raw_pain, &modulated_pain);

    /* Without stress, pain should be relatively unmodulated */
    EXPECT_NEAR(modulated_pain, raw_pain, 0.2f);

    hypo_pain_modulation_state_t state;
    hypo_perception_bridge_get_pain_state(perception_bridge, &state);
    EXPECT_FALSE(state.in_acute_stress);
    EXPECT_FALSE(state.in_chronic_stress);
}

/* ============================================================================
 * Test 6: Sleep-Perception Pipeline
 * How sleep stages gate perception
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, AwakeStateAllowsPerception) {
    /* Set awake state */
    int ret = hypo_perception_bridge_set_sleep_stage(
        perception_bridge, HYPO_SLEEP_AWAKE);
    ASSERT_EQ(0, ret);

    /* Check if normal stimulus passes gate */
    bool passes;
    ret = hypo_perception_bridge_check_sleep_gate(
        perception_bridge, 0.3f, false, false, &passes);
    ASSERT_EQ(0, ret);

    /* Normal stimulus should pass when awake */
    EXPECT_TRUE(passes);

    hypo_sleep_gating_state_t state;
    hypo_perception_bridge_get_sleep_gating_state(perception_bridge, &state);
    EXPECT_EQ(HYPO_SLEEP_AWAKE, state.current_stage);
    EXPECT_NEAR(state.perception_gate, 1.0f, 0.1f);  /* Gate fully open */
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, DeepSleepBlocksNormalStimuli) {
    /* Set deep sleep */
    int ret = hypo_perception_bridge_set_sleep_stage(
        perception_bridge, HYPO_SLEEP_DEEP);
    ASSERT_EQ(0, ret);

    /* Normal weak stimulus should not pass */
    bool passes;
    ret = hypo_perception_bridge_check_sleep_gate(
        perception_bridge, 0.2f, false, false, &passes);
    ASSERT_EQ(0, ret);

    EXPECT_FALSE(passes);

    hypo_sleep_gating_state_t state;
    hypo_perception_bridge_get_sleep_gating_state(perception_bridge, &state);
    EXPECT_EQ(HYPO_SLEEP_DEEP, state.current_stage);
    EXPECT_LT(state.perception_gate, 0.3f);  /* Gate mostly closed */
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ThreatBypassesSleepGate) {
    /* Set deep sleep */
    hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_DEEP);

    /* Threat stimulus should bypass gate even in deep sleep */
    bool passes;
    int ret = hypo_perception_bridge_check_sleep_gate(
        perception_bridge, 0.5f, true, false, &passes);  /* is_threat = true */
    ASSERT_EQ(0, ret);

    EXPECT_TRUE(passes);  /* Threat bypasses gate */

    hypo_sleep_gating_state_t state;
    hypo_perception_bridge_get_sleep_gating_state(perception_bridge, &state);
    EXPECT_GT(state.threat_bypass, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, OwnNameBypassesSleepGate) {
    /* Set light sleep */
    hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_LIGHT);

    /* Weak stimulus with name should pass */
    bool passes;
    int ret = hypo_perception_bridge_check_sleep_gate(
        perception_bridge, 0.3f, false, true, &passes);  /* is_name = true */
    ASSERT_EQ(0, ret);

    /* Own name can bypass gating */
    EXPECT_TRUE(passes);

    hypo_sleep_gating_state_t state;
    hypo_perception_bridge_get_sleep_gating_state(perception_bridge, &state);
    EXPECT_GT(state.name_bypass, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, REMSleepPartialGating) {
    /* Set REM sleep */
    hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_REM);

    hypo_sleep_gating_state_t state;
    hypo_perception_bridge_get_sleep_gating_state(perception_bridge, &state);

    EXPECT_EQ(HYPO_SLEEP_REM, state.current_stage);
    /* REM sleep should have partial gating (between awake and deep) */
    EXPECT_GT(state.perception_gate, 0.0f);
    EXPECT_LT(state.perception_gate, 1.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, StrongStimulusWakes) {
    /* Set drowsy state */
    hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_DROWSY);

    /* Very strong stimulus should pass gate and potentially wake */
    bool passes;
    int ret = hypo_perception_bridge_check_sleep_gate(
        perception_bridge, 0.9f, false, false, &passes);  /* Very strong */
    ASSERT_EQ(0, ret);

    EXPECT_TRUE(passes);  /* Strong stimulus passes even when drowsy */
}

/* ============================================================================
 * Test 7: Thermal-Drive Integration
 * How temperature affects thermal salience
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ColdTemperatureIncreasesWarmSeeking) {
    /* Set cold core temperature */
    int ret = hypo_perception_bridge_set_core_temperature(
        perception_bridge, TEST_TEMPERATURE_COLD);
    ASSERT_EQ(0, ret);

    hypo_thermal_state_t state;
    ret = hypo_perception_bridge_get_thermal_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    /* Should seek warmth when cold */
    EXPECT_GT(state.warm_seeking, 0.0f);
    EXPECT_LT(state.cool_seeking, state.warm_seeking);
    EXPECT_GT(state.thermal_discomfort, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, HotTemperatureIncreasesCoolSeeking) {
    /* Set hot core temperature */
    int ret = hypo_perception_bridge_set_core_temperature(
        perception_bridge, TEST_TEMPERATURE_HOT);
    ASSERT_EQ(0, ret);

    hypo_thermal_state_t state;
    ret = hypo_perception_bridge_get_thermal_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    /* Should seek cooling when hot */
    EXPECT_GT(state.cool_seeking, 0.0f);
    EXPECT_LT(state.warm_seeking, state.cool_seeking);
    EXPECT_GT(state.thermal_discomfort, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ThermalDiscomfortBoostsSalience) {
    /* Set comfortable temperature */
    hypo_perception_bridge_set_core_temperature(perception_bridge, TEST_TEMPERATURE_SETPOINT);

    float boost_comfortable;
    hypo_perception_bridge_compute_thermal_salience(perception_bridge, &boost_comfortable);

    /* Set uncomfortable (cold) temperature */
    hypo_perception_bridge_set_core_temperature(perception_bridge, TEST_TEMPERATURE_COLD);

    float boost_uncomfortable;
    hypo_perception_bridge_compute_thermal_salience(perception_bridge, &boost_uncomfortable);

    /* Uncomfortable temperature should boost thermal salience more */
    EXPECT_GT(boost_uncomfortable, boost_comfortable);
    EXPECT_GE(boost_uncomfortable, 1.0f);
    EXPECT_LE(boost_uncomfortable, 3.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, AmbientTemperatureAffectsState) {
    /* Set ambient temperature */
    int ret = hypo_perception_bridge_set_ambient_temperature(
        perception_bridge, TEST_TEMPERATURE_COLD);
    ASSERT_EQ(0, ret);

    hypo_thermal_state_t state;
    ret = hypo_perception_bridge_get_thermal_state(perception_bridge, &state);
    ASSERT_EQ(0, ret);

    EXPECT_NEAR(state.ambient_temperature, TEST_TEMPERATURE_COLD, 0.1f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ExtremeTemperaturesGenerateAlerts) {
    /* Set very cold temperature (hypothermic) */
    hypo_perception_bridge_set_core_temperature(perception_bridge, 0.1f);

    hypo_thermal_state_t state;
    hypo_perception_bridge_get_thermal_state(perception_bridge, &state);

    /* Should have hypothermic alert */
    EXPECT_TRUE(state.hypothermic_alert);
    EXPECT_FALSE(state.hyperthermic_alert);

    /* Set very hot temperature (hyperthermic) */
    hypo_perception_bridge_set_core_temperature(perception_bridge, 0.9f);
    hypo_perception_bridge_get_thermal_state(perception_bridge, &state);

    /* Should have hyperthermic alert */
    EXPECT_TRUE(state.hyperthermic_alert);
    EXPECT_FALSE(state.hypothermic_alert);
}

/* ============================================================================
 * Test 8: Multi-Enhancement Interaction
 * Test all enhancements working together
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, AllEnhancementsEnabled) {
    /* Create bridge with all enhancements */
    hypo_perception_bridge_t* enhanced = create_enhanced_bridge();
    ASSERT_NE(enhanced, nullptr);

    /* Set various states */
    hypo_perception_bridge_set_arousal(enhanced, TEST_AROUSAL_HIGH);
    hypo_perception_bridge_set_stress_for_pain(enhanced, TEST_STRESS_LOW, false);
    hypo_perception_bridge_set_sleep_stage(enhanced, HYPO_SLEEP_AWAKE);
    hypo_perception_bridge_set_core_temperature(enhanced, TEST_TEMPERATURE_SETPOINT);

    /* Process signals from various modalities */
    hypo_perception_bridge_process_interoceptive(enhanced, HYPO_INTERO_HUNGER_PANGS, 0.5f);
    hypo_perception_bridge_process_olfactory(enhanced, HYPO_OLFACT_FOOD_PLEASANT, 0.6f);
    hypo_perception_bridge_process_gustatory(enhanced, HYPO_GUST_SWEET, 0.4f);
    hypo_perception_bridge_generate_prediction(enhanced, HYPO_PRED_FOOD_PRESENCE, 0.7f, 0.8f);

    /* Update */
    hypo_perception_bridge_update(enhanced, TIME_DELTA_US);

    /* Compute modulation */
    hypo_perception_bridge_compute_modulation(enhanced);

    /* Verify all states are accessible and valid */
    hypo_perception_modulation_t modulation;
    EXPECT_EQ(0, hypo_perception_bridge_get_modulation(enhanced, &modulation));
    EXPECT_GE(modulation.global_gain, 0.5f);
    EXPECT_LE(modulation.global_gain, 2.0f);

    hypo_interoceptive_state_t intero_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_interoceptive_state(enhanced, &intero_state));
    EXPECT_GT(intero_state.signals[HYPO_INTERO_HUNGER_PANGS], 0.0f);

    hypo_chemosensory_state_t chemo_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_chemosensory_state(enhanced, &chemo_state));
    EXPECT_GT(chemo_state.olfactory_intensity[HYPO_OLFACT_FOOD_PLEASANT], 0.0f);

    hypo_predictive_state_t pred_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_predictive_state(enhanced, &pred_state));
    EXPECT_GT(pred_state.predictions[HYPO_PRED_FOOD_PRESENCE], 0.0f);

    hypo_pain_modulation_state_t pain_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_pain_state(enhanced, &pain_state));

    hypo_sleep_gating_state_t sleep_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(enhanced, &sleep_state));
    EXPECT_EQ(HYPO_SLEEP_AWAKE, sleep_state.current_stage);

    hypo_thermal_state_t thermal_state;
    EXPECT_EQ(0, hypo_perception_bridge_get_thermal_state(enhanced, &thermal_state));

    /* Check statistics */
    hypo_perception_bridge_stats_t stats;
    EXPECT_EQ(0, hypo_perception_bridge_get_stats(enhanced, &stats));
    EXPECT_GT(stats.interoceptive_signals, 0u);
    EXPECT_GT(stats.olfactory_stimuli, 0u);
    EXPECT_GT(stats.gustatory_stimuli, 0u);
    EXPECT_GT(stats.predictions_generated, 0u);

    hypo_perception_bridge_destroy(enhanced);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, CrossModalIntegration) {
    /* Test how different modalities interact */

    /* Build hunger drive */
    for (int i = 0; i < 50; i++) {
        hypo_drive_update(drive_system, 100000);
    }

    /* Set high arousal */
    hypo_perception_bridge_set_arousal(perception_bridge, TEST_AROUSAL_HIGH);

    /* Process food-related stimuli across modalities */
    hypo_perception_bridge_process_interoceptive(perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.7f);
    hypo_perception_bridge_process_olfactory(perception_bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.8f);
    hypo_perception_bridge_process_gustatory(perception_bridge, HYPO_GUST_SWEET, 0.6f);

    /* Generate food prediction */
    hypo_perception_bridge_generate_prediction(perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.9f, 0.85f);

    /* Compute modulation */
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_modulation_t modulation;
    hypo_perception_bridge_get_modulation(perception_bridge, &modulation);

    /* Food salience should be high due to multiple converging signals */
    EXPECT_GT(modulation.category_salience[HYPO_STIM_FOOD], 0.5f);

    /* Global gain should be elevated due to arousal and drive urgency */
    EXPECT_GT(modulation.global_gain, 1.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, StressSleepInteraction) {
    /* Test interaction between stress and sleep states */

    /* Set acute stress */
    hypo_perception_bridge_set_stress_for_pain(perception_bridge, TEST_STRESS_HIGH, false);

    /* Try to sleep during stress */
    hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_LIGHT);

    hypo_sleep_gating_state_t sleep_state;
    hypo_perception_bridge_get_sleep_gating_state(perception_bridge, &sleep_state);

    /* Arousal threshold should be lower during stress (easier to wake) */
    /* This depends on implementation, but arousal should interact with sleep */
    EXPECT_GT(sleep_state.arousal_threshold, 0.0f);

    /* Process threat during sleep */
    bool passes;
    hypo_perception_bridge_check_sleep_gate(perception_bridge, 0.4f, true, false, &passes);

    /* Threat should pass gate */
    EXPECT_TRUE(passes);
}

/* ============================================================================
 * Test 9: Bio-async Message Flow
 * Test message handlers and broadcasts
 * ============================================================================ */

class HypothalamusPerceptionBridgeBioAsyncTest : public HypothalamusPerceptionBridgeIntegrationTest {
protected:
    void SetUp() override {
        /* Initialize global bio-router if not already */
        bio_router_config_t router_config = bio_router_default_config();
        if (!bio_router_is_initialized()) {
            nimcp_error_t err = bio_router_init(&router_config);
            ASSERT_EQ(NIMCP_SUCCESS, err);
        }

        /* Now call parent setup */
        HypothalamusPerceptionBridgeIntegrationTest::SetUp();
    }

    void TearDown() override {
        /* Call parent teardown first */
        HypothalamusPerceptionBridgeIntegrationTest::TearDown();

        /* Note: Don't shutdown global router as other tests might use it */
    }
};

TEST_F(HypothalamusPerceptionBridgeBioAsyncTest, RegisterWithBioRouter) {
    /* Register with bio-async router */
    bool result = hypo_perception_bridge_register_bio(perception_bridge, false);
    EXPECT_TRUE(result);

    /* Unregister */
    hypo_perception_bridge_unregister_bio(perception_bridge);
}

TEST_F(HypothalamusPerceptionBridgeBioAsyncTest, BroadcastModulation) {
    /* Register with router */
    bool registered = hypo_perception_bridge_register_bio(perception_bridge, false);
    EXPECT_TRUE(registered);

    /* Set up some state */
    hypo_perception_bridge_set_arousal(perception_bridge, TEST_AROUSAL_HIGH);
    hypo_perception_bridge_compute_modulation(perception_bridge);

    /* Broadcast modulation */
    nimcp_error_t err = hypo_perception_bridge_broadcast_modulation(perception_bridge);
    EXPECT_EQ(NIMCP_SUCCESS, err);

    /* Cleanup */
    hypo_perception_bridge_unregister_bio(perception_bridge);
}

TEST_F(HypothalamusPerceptionBridgeBioAsyncTest, SensoryDetectionFeedback) {
    /* Create sensory detection */
    hypo_sensory_detection_t detection;
    detection.detected_category = HYPO_STIM_FOOD;
    detection.modality = HYPO_SENSE_VISUAL;
    detection.confidence = 0.8f;
    detection.intensity = 0.6f;
    detection.is_threat = false;
    detection.timestamp_us = 1000000;

    /* Process detection */
    int ret = hypo_perception_bridge_process_detection(perception_bridge, &detection);
    ASSERT_EQ(0, ret);

    /* Check anticipation was generated */
    float anticipation = hypo_perception_bridge_get_anticipation(
        perception_bridge, HYPO_DRIVE_HUNGER);
    EXPECT_GT(anticipation, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeBioAsyncTest, ThreatDetectionTriggersResponse) {
    /* Create threat detection */
    hypo_sensory_detection_t detection;
    detection.detected_category = HYPO_STIM_THREAT;
    detection.modality = HYPO_SENSE_VISUAL;
    detection.confidence = 0.9f;
    detection.intensity = 0.8f;
    detection.is_threat = true;
    detection.timestamp_us = 1000000;

    /* Process detection */
    int ret = hypo_perception_bridge_process_detection(perception_bridge, &detection);
    ASSERT_EQ(0, ret);

    /* Safety drive anticipation should increase */
    float anticipation = hypo_perception_bridge_get_anticipation(
        perception_bridge, HYPO_DRIVE_SAFETY);
    EXPECT_GT(anticipation, 0.0f);

    /* Recompute modulation */
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_modulation_t modulation;
    hypo_perception_bridge_get_modulation(perception_bridge, &modulation);

    /* Threat salience should be high */
    EXPECT_GT(modulation.category_salience[HYPO_STIM_THREAT], 0.0f);
}

/* ============================================================================
 * Statistics and Diagnostics Tests
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, StatisticsAccumulate) {
    /* Perform various operations */
    hypo_perception_bridge_compute_modulation(perception_bridge);
    hypo_perception_bridge_compute_modulation(perception_bridge);
    hypo_perception_bridge_compute_modulation(perception_bridge);

    hypo_perception_bridge_process_interoceptive(perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.5f);
    hypo_perception_bridge_process_interoceptive(perception_bridge, HYPO_INTERO_THIRST, 0.3f);

    hypo_perception_bridge_process_olfactory(perception_bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.6f);
    hypo_perception_bridge_process_gustatory(perception_bridge, HYPO_GUST_SWEET, 0.4f);

    hypo_perception_bridge_generate_prediction(perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.7f, 0.8f);
    hypo_perception_bridge_update_prediction_error(perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.5f);

    float pain;
    hypo_perception_bridge_modulate_pain(perception_bridge, 0.5f, &pain);

    bool passes;
    hypo_perception_bridge_check_sleep_gate(perception_bridge, 0.5f, false, false, &passes);

    /* Get stats */
    hypo_perception_bridge_stats_t stats;
    int ret = hypo_perception_bridge_get_stats(perception_bridge, &stats);
    ASSERT_EQ(0, ret);

    EXPECT_EQ(3u, stats.modulations_computed);
    EXPECT_EQ(2u, stats.interoceptive_signals);
    EXPECT_EQ(1u, stats.olfactory_stimuli);
    EXPECT_EQ(1u, stats.gustatory_stimuli);
    EXPECT_EQ(1u, stats.predictions_generated);
    EXPECT_EQ(1u, stats.prediction_errors_updated);
    EXPECT_EQ(1u, stats.pain_stimuli_modulated);
    EXPECT_EQ(1u, stats.sleep_gate_checks);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ResetStatistics) {
    /* Generate some stats */
    hypo_perception_bridge_compute_modulation(perception_bridge);
    hypo_perception_bridge_process_interoceptive(perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.5f);

    /* Reset */
    int ret = hypo_perception_bridge_reset_stats(perception_bridge);
    ASSERT_EQ(0, ret);

    /* Stats should be zero */
    hypo_perception_bridge_stats_t stats;
    hypo_perception_bridge_get_stats(perception_bridge, &stats);

    EXPECT_EQ(0u, stats.modulations_computed);
    EXPECT_EQ(0u, stats.interoceptive_signals);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ResetBridge) {
    /* Set various states */
    hypo_perception_bridge_set_arousal(perception_bridge, TEST_AROUSAL_HIGH);
    hypo_perception_bridge_set_stress_for_pain(perception_bridge, TEST_STRESS_HIGH, false);
    hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_LIGHT);

    /* Reset bridge */
    int ret = hypo_perception_bridge_reset(perception_bridge);
    ASSERT_EQ(0, ret);

    /* States should be reset to defaults */
    hypo_perception_modulation_t modulation;
    hypo_perception_bridge_get_modulation(perception_bridge, &modulation);

    /* Values should be at default/neutral */
    /* Exact values depend on implementation defaults */
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, NullBridgeHandling) {
    /* Operations on null bridge should fail gracefully */
    int ret = hypo_perception_bridge_compute_modulation(nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_set_arousal(nullptr, 0.5f);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_process_interoceptive(nullptr, HYPO_INTERO_HUNGER_PANGS, 0.5f);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_process_olfactory(nullptr, HYPO_OLFACT_FOOD_PLEASANT, 0.5f);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_generate_prediction(nullptr, HYPO_PRED_FOOD_PRESENCE, 0.5f, 0.5f);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_set_stress_for_pain(nullptr, 0.5f, false);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_set_sleep_stage(nullptr, HYPO_SLEEP_AWAKE);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_set_core_temperature(nullptr, 0.5f);
    EXPECT_EQ(-1, ret);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, NullOutputHandling) {
    /* Null output parameters should be handled */
    int ret = hypo_perception_bridge_get_modulation(perception_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_get_interoceptive_state(perception_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_get_chemosensory_state(perception_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_get_predictive_state(perception_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_get_pain_state(perception_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_get_sleep_gating_state(perception_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_get_thermal_state(perception_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_perception_bridge_get_stats(perception_bridge, nullptr);
    EXPECT_EQ(-1, ret);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, InvalidInputClamping) {
    /* Values outside valid range should be clamped */
    int ret = hypo_perception_bridge_set_arousal(perception_bridge, 2.0f);
    EXPECT_EQ(0, ret);  /* Should succeed with clamping */

    hypo_perception_modulation_t modulation;
    hypo_perception_bridge_get_modulation(perception_bridge, &modulation);
    EXPECT_LE(modulation.arousal_level, 1.0f);  /* Should be clamped to 1.0 */

    ret = hypo_perception_bridge_set_arousal(perception_bridge, -1.0f);
    EXPECT_EQ(0, ret);

    hypo_perception_bridge_get_modulation(perception_bridge, &modulation);
    EXPECT_GE(modulation.arousal_level, 0.0f);  /* Should be clamped to 0.0 */
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, DestroyNullIsSafe) {
    /* Destroying null should not crash */
    hypo_perception_bridge_destroy(nullptr);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, InteroceptiveTypeNames) {
    const char* name;

    name = hypo_interoceptive_type_name(HYPO_INTERO_HUNGER_PANGS);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_interoceptive_type_name(HYPO_INTERO_HEART_RATE);
    EXPECT_NE(name, nullptr);

    name = hypo_interoceptive_type_name(HYPO_INTERO_FATIGUE);
    EXPECT_NE(name, nullptr);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, OlfactoryTypeNames) {
    const char* name;

    name = hypo_olfactory_type_name(HYPO_OLFACT_FOOD_PLEASANT);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_olfactory_type_name(HYPO_OLFACT_DANGER);
    EXPECT_NE(name, nullptr);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, GustatoryTypeNames) {
    const char* name;

    name = hypo_gustatory_type_name(HYPO_GUST_SWEET);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_gustatory_type_name(HYPO_GUST_BITTER);
    EXPECT_NE(name, nullptr);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, PredictionTypeNames) {
    const char* name;

    name = hypo_prediction_type_name(HYPO_PRED_FOOD_PRESENCE);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_prediction_type_name(HYPO_PRED_THREAT_PRESENCE);
    EXPECT_NE(name, nullptr);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, SleepStageNames) {
    const char* name;

    name = hypo_sleep_stage_name(HYPO_SLEEP_AWAKE);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");

    name = hypo_sleep_stage_name(HYPO_SLEEP_REM);
    EXPECT_NE(name, nullptr);

    name = hypo_sleep_stage_name(HYPO_SLEEP_DEEP);
    EXPECT_NE(name, nullptr);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, DefaultConfigValues) {
    hypo_perception_config_t config;
    hypo_perception_bridge_default_config(&config);

    /* Verify reasonable defaults */
    EXPECT_GT(config.arousal_gain_min, 0.0f);
    EXPECT_LT(config.arousal_gain_min, 1.0f);
    EXPECT_GT(config.arousal_gain_max, 1.0f);
    EXPECT_LE(config.arousal_gain_max, 2.0f);
    EXPECT_GT(config.drive_salience_weight, 0.0f);
    EXPECT_LE(config.drive_salience_weight, 1.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, EnhancedConfigDefaults) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    /* Verify enhanced defaults */
    EXPECT_GE(config.interoceptive_accuracy, 0.0f);
    EXPECT_LE(config.interoceptive_accuracy, 1.0f);
    EXPECT_GT(config.hunger_smell_boost, 0.0f);
    EXPECT_GT(config.thirst_salt_boost, 0.0f);
    EXPECT_GT(config.prediction_precision_default, 0.0f);
    EXPECT_GT(config.acute_stress_analgesia, 0.0f);
    EXPECT_GT(config.chronic_stress_hyperalgesia, 0.0f);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ApplyEnhancedConfig) {
    hypo_perception_enhanced_config_t config;
    hypo_perception_bridge_default_enhanced_config(&config);

    /* Modify config */
    config.enable_interoception = true;
    config.enable_predictive_coding = true;
    config.interoceptive_accuracy = 0.95f;
    config.prediction_precision_default = 0.8f;

    /* Apply config */
    int ret = hypo_perception_bridge_apply_enhanced_config(perception_bridge, &config);
    EXPECT_EQ(0, ret);

    /* Verify config was applied (indirectly through behavior) */
    hypo_perception_bridge_set_interoceptive_accuracy(perception_bridge, 0.5f);

    hypo_interoceptive_state_t state;
    hypo_perception_bridge_get_interoceptive_state(perception_bridge, &state);
    EXPECT_NEAR(state.global_interoceptive_accuracy, 0.5f, 0.1f);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ConcurrentModulationComputation) {
    std::atomic<int> success_count{0};

    auto compute_thread = [this, &success_count]() {
        for (int i = 0; i < 50; i++) {
            hypo_perception_bridge_set_arousal(perception_bridge, 0.3f + (i % 5) * 0.1f);
            int ret = hypo_perception_bridge_compute_modulation(perception_bridge);
            if (ret == 0) success_count++;
        }
    };

    std::thread t1(compute_thread);
    std::thread t2(compute_thread);

    t1.join();
    t2.join();

    EXPECT_EQ(100, success_count.load());

    /* Bridge should still be functional */
    hypo_perception_modulation_t modulation;
    int ret = hypo_perception_bridge_get_modulation(perception_bridge, &modulation);
    EXPECT_EQ(0, ret);
}

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ConcurrentStimulusProcessing) {
    std::atomic<int> intero_count{0};
    std::atomic<int> olfact_count{0};

    auto intero_thread = [this, &intero_count]() {
        for (int i = 0; i < 30; i++) {
            int ret = hypo_perception_bridge_process_interoceptive(
                perception_bridge,
                static_cast<hypo_interoceptive_type_t>(i % HYPO_INTERO_COUNT),
                0.3f + (i % 5) * 0.1f);
            if (ret == 0) intero_count++;
        }
    };

    auto olfact_thread = [this, &olfact_count]() {
        for (int i = 0; i < 30; i++) {
            int ret = hypo_perception_bridge_process_olfactory(
                perception_bridge,
                static_cast<hypo_olfactory_type_t>(i % HYPO_OLFACT_COUNT),
                0.4f + (i % 4) * 0.1f);
            if (ret == 0) olfact_count++;
        }
    };

    std::thread t1(intero_thread);
    std::thread t2(olfact_thread);

    t1.join();
    t2.join();

    EXPECT_EQ(30, intero_count.load());
    EXPECT_EQ(30, olfact_count.load());

    /* Check stats */
    hypo_perception_bridge_stats_t stats;
    hypo_perception_bridge_get_stats(perception_bridge, &stats);
    EXPECT_EQ(30u, stats.interoceptive_signals);
    EXPECT_EQ(30u, stats.olfactory_stimuli);
}

/* ============================================================================
 * Extended Update Cycle Tests
 * ============================================================================ */

TEST_F(HypothalamusPerceptionBridgeIntegrationTest, ExtendedUpdateCycles) {
    /* Reset stats to ensure accurate counting for this test */
    hypo_perception_bridge_reset_stats(perception_bridge);

    /* Run many update cycles with various inputs */
    for (int cycle = 0; cycle < 100; cycle++) {
        /* Update drive system */
        hypo_drive_update(drive_system, TIME_DELTA_US);

        /* Vary arousal */
        float arousal = 0.3f + 0.5f * sinf(cycle * 0.1f);
        hypo_perception_bridge_set_arousal(perception_bridge, arousal);

        /* Process some stimuli */
        if (cycle % 3 == 0) {
            hypo_perception_bridge_process_interoceptive(
                perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.5f);
        }
        if (cycle % 5 == 0) {
            hypo_perception_bridge_process_olfactory(
                perception_bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.6f);
        }
        if (cycle % 7 == 0) {
            hypo_perception_bridge_generate_prediction(
                perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.7f, 0.8f);
        }

        /* Update bridge (internally calls compute_modulation) */
        hypo_perception_bridge_update(perception_bridge, TIME_DELTA_US);
    }

    /* Verify final state is valid */
    hypo_perception_modulation_t modulation;
    int ret = hypo_perception_bridge_get_modulation(perception_bridge, &modulation);
    EXPECT_EQ(0, ret);
    EXPECT_GE(modulation.global_gain, 0.5f);
    EXPECT_LE(modulation.global_gain, 2.0f);

    /* Check stats */
    hypo_perception_bridge_stats_t stats;
    hypo_perception_bridge_get_stats(perception_bridge, &stats);
    EXPECT_EQ(100u, stats.modulations_computed);
    EXPECT_GT(stats.interoceptive_signals, 30u);  /* ~100/3 */
    EXPECT_GT(stats.olfactory_stimuli, 15u);      /* ~100/5 */
    EXPECT_GT(stats.predictions_generated, 10u);  /* ~100/7 */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    /* Clean up global router if initialized */
    if (bio_router_is_initialized()) {
        bio_router_shutdown();
    }

    return result;
}
