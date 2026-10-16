/**
 * @file e2e_test_hypothalamus_perception_pipeline.cpp
 * @brief End-to-end tests for Hypothalamus-Perception Bridge Pipeline
 *
 * WHAT: Complete end-to-end tests simulating realistic perception-drive integration
 * WHY:  Verify that drives properly modulate sensory processing and perception
 *       feeds back to drive anticipation, following biological principles
 * HOW:  Create complete pipelines with multi-step scenarios exercising all
 *       enhanced features (interoception, chemosensory, predictive coding,
 *       pain modulation, sleep gating, thermal salience)
 *
 * TEST SCENARIOS:
 * 1. Hungry Agent Pipeline - High hunger -> food smell -> anticipation -> satisfaction
 * 2. Threat Response Pipeline - Threat detected -> safety drive -> pain suppression -> heightened perception
 * 3. Sleep Wake Cycle - Transition through sleep stages, test perception gating
 * 4. Thermal Regulation Pipeline - Cold exposure -> thermal discomfort -> warm-seeking
 * 5. Predictive Coding Pipeline - Generate predictions -> receive input -> update errors -> free energy
 * 6. Pain-Stress-Endorphin Pipeline - Stress -> pain modulation -> endorphin release -> recovery
 * 7. Full Sensory Integration - Multiple modalities, all enhancements active
 * 8. Circadian Pattern - Day/night cycle affecting perception
 * 9. Homeostatic Regulation - Multiple drives competing for attention
 *
 * @version Phase 17.5: Enhanced Perception Integration E2E Tests
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_perception_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class HypothalamusPerceptionE2ETest : public ::testing::Test {
protected:
    hypo_drive_system_handle_t* drives;
    hypo_perception_bridge_t* perception_bridge;
    hypo_drive_config_t drive_config;
    hypo_perception_config_t perception_config;
    hypo_perception_enhanced_config_t enhanced_config;

    void SetUp() override {
        // Create drive system
        drive_config = hypo_drive_default_config();
        drives = hypo_drive_create(&drive_config);
        ASSERT_NE(nullptr, drives);

        // Get default perception config
        hypo_perception_bridge_default_config(&perception_config);

        // Create perception bridge
        perception_bridge = hypo_perception_bridge_create(drives, &perception_config);
        ASSERT_NE(nullptr, perception_bridge);

        // Apply enhanced configuration
        hypo_perception_bridge_default_enhanced_config(&enhanced_config);
        enhanced_config.enable_interoception = true;
        enhanced_config.enable_chemosensory = true;
        enhanced_config.enable_predictive_coding = true;
        enhanced_config.enable_pain_modulation = true;
        enhanced_config.enable_sleep_gating = true;
        enhanced_config.enable_thermal_salience = true;

        int result = hypo_perception_bridge_apply_enhanced_config(perception_bridge, &enhanced_config);
        ASSERT_EQ(0, result);
    }

    void TearDown() override {
        hypo_perception_bridge_destroy(perception_bridge);
        perception_bridge = nullptr;
        hypo_drive_destroy(drives);
        drives = nullptr;
    }

    // Helper: Simulate time passing with updates
    void simulate_time(uint64_t total_us, uint64_t step_us = 100000) {
        uint64_t elapsed = 0;
        while (elapsed < total_us) {
            hypo_drive_update(drives, step_us);
            hypo_perception_bridge_update(perception_bridge, step_us);
            elapsed += step_us;
        }
    }

    // Helper: Check perception modulation is valid
    void assert_valid_modulation(const hypo_perception_modulation_t& mod) {
        EXPECT_GE(mod.global_gain, 0.5f);
        EXPECT_LE(mod.global_gain, 2.0f);
        EXPECT_GE(mod.arousal_level, 0.0f);
        EXPECT_LE(mod.arousal_level, 1.0f);

        for (int i = 0; i < HYPO_SENSE_COUNT; i++) {
            EXPECT_GE(mod.modality_gains[i], 0.0f);
            EXPECT_LE(mod.modality_gains[i], 3.0f);
        }

        for (int i = 0; i < HYPO_STIM_COUNT; i++) {
            EXPECT_GE(mod.category_salience[i], 0.0f);
            EXPECT_LE(mod.category_salience[i], 2.0f);
        }
    }
};

// ============================================================================
// SCENARIO 1: HUNGRY AGENT PIPELINE
// ============================================================================
// High hunger -> food smell detected -> anticipation -> satisfaction

TEST_F(HypothalamusPerceptionE2ETest, HungryAgentPipeline) {
    // Phase 1: Agent becomes hungry over time
    // Simulate 4 hours of no eating - hunger builds
    simulate_time(4 * 3600 * 1000000ULL);

    // Get drive state - hunger should be elevated
    hypo_drive_state_t hunger_state;
    ASSERT_TRUE(hypo_drive_get_state(drives, HYPO_DRIVE_HUNGER, &hunger_state));
    float initial_hunger = hunger_state.level;
    EXPECT_GT(initial_hunger, 0.3f) << "Hunger should build over 4 hours";

    // Compute perception modulation
    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

    // Get modulation state
    hypo_perception_modulation_t mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &mod));
    assert_valid_modulation(mod);

    // Food salience should be boosted due to hunger
    float food_salience_before_smell = mod.category_salience[HYPO_STIM_FOOD];
    EXPECT_GT(food_salience_before_smell, mod.category_salience[HYPO_STIM_NEUTRAL])
        << "Food salience should be higher than neutral when hungry";

    // Phase 2: Agent smells food
    ASSERT_EQ(0, hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.8f));

    // Check chemosensory state
    hypo_chemosensory_state_t chem_state;
    ASSERT_EQ(0, hypo_perception_bridge_get_chemosensory_state(perception_bridge, &chem_state));
    EXPECT_TRUE(chem_state.food_detected) << "Food should be detected from pleasant smell";
    EXPECT_GT(chem_state.hunger_modulation, 0.0f)
        << "Hunger should modulate olfactory sensitivity";

    // Phase 3: Process sensory detection (visual food detection)
    hypo_sensory_detection_t detection = {
        .detected_category = HYPO_STIM_FOOD,
        .modality = HYPO_SENSE_VISUAL,
        .confidence = 0.9f,
        .intensity = 0.7f,
        .is_threat = false,
        .timestamp_us = 0
    };
    ASSERT_EQ(0, hypo_perception_bridge_process_detection(perception_bridge, &detection));

    // Check anticipation increased
    float anticipation = hypo_perception_bridge_get_anticipation(perception_bridge, HYPO_DRIVE_HUNGER);
    EXPECT_GT(anticipation, 0.3f) << "Anticipation should increase after detecting food";

    // Phase 4: Agent eats (satisfy hunger)
    // Process gustatory input (sweet taste of food)
    ASSERT_EQ(0, hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_SWEET, 0.8f));
    ASSERT_EQ(0, hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_UMAMI, 0.6f));

    // Satisfy the hunger drive
    float reward = hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 0.9f);
    EXPECT_GT(reward, 0.0f) << "Satisfying hunger should produce positive reward";

    // Simulate digestion time
    simulate_time(30 * 60 * 1000000ULL);

    // Phase 5: Verify satiation effects
    ASSERT_TRUE(hypo_drive_get_state(drives, HYPO_DRIVE_HUNGER, &hunger_state));
    EXPECT_LT(hunger_state.level, initial_hunger)
        << "Hunger should be reduced after eating";

    // Recompute modulation
    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &mod));

    // Food salience should be lower now that satiated
    EXPECT_LT(mod.category_salience[HYPO_STIM_FOOD], food_salience_before_smell)
        << "Food salience should decrease after eating";

    // Get statistics
    hypo_perception_bridge_stats_t stats;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &stats));
    EXPECT_GT(stats.olfactory_stimuli, 0u);
    EXPECT_GT(stats.gustatory_stimuli, 0u);
    EXPECT_GT(stats.detections_processed, 0u);
}

// ============================================================================
// SCENARIO 2: THREAT RESPONSE PIPELINE
// ============================================================================
// Threat detected -> safety drive -> pain suppression -> heightened perception

TEST_F(HypothalamusPerceptionE2ETest, ThreatResponsePipeline) {
    // Phase 1: Normal baseline state
    hypo_perception_bridge_set_arousal(perception_bridge, 0.3f);
    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

    hypo_perception_modulation_t baseline_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &baseline_mod));
    EXPECT_FALSE(baseline_mod.threat_priority);
    EXPECT_FALSE(baseline_mod.survival_mode);

    // Phase 2: Threat detected (danger smell + visual threat)
    ASSERT_EQ(0, hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_DANGER, 0.9f));

    hypo_sensory_detection_t threat_detection = {
        .detected_category = HYPO_STIM_THREAT,
        .modality = HYPO_SENSE_VISUAL,
        .confidence = 0.95f,
        .intensity = 0.85f,
        .is_threat = true,
        .timestamp_us = 0
    };
    ASSERT_EQ(0, hypo_perception_bridge_process_detection(perception_bridge, &threat_detection));

    // Phase 3: Arousal spikes due to threat
    hypo_perception_bridge_set_arousal(perception_bridge, 0.95f);
    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

    hypo_perception_modulation_t threat_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &threat_mod));

    // Verify threat response
    EXPECT_TRUE(threat_mod.threat_priority)
        << "Threat detection should trigger threat priority mode";
    EXPECT_GT(threat_mod.global_gain, baseline_mod.global_gain)
        << "Global sensory gain should increase under threat";
    EXPECT_GT(threat_mod.category_salience[HYPO_STIM_THREAT],
              baseline_mod.category_salience[HYPO_STIM_THREAT])
        << "Threat salience should spike";

    // Safety drive should activate
    float safety_anticipation = hypo_perception_bridge_get_anticipation(
        perception_bridge, HYPO_DRIVE_SAFETY);
    EXPECT_GT(safety_anticipation, 0.0f)
        << "Safety drive anticipation should activate on threat";

    // Phase 4: Stress-induced analgesia (acute stress reduces pain)
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(
        perception_bridge, 0.9f, false /* not chronic */));

    // Get pain modulation state
    hypo_pain_modulation_state_t pain_state;
    ASSERT_EQ(0, hypo_perception_bridge_get_pain_state(perception_bridge, &pain_state));
    EXPECT_TRUE(pain_state.in_acute_stress);
    EXPECT_GT(pain_state.analgesia_level, 0.0f)
        << "Acute stress should induce analgesia";
    EXPECT_LT(pain_state.pain_sensitivity, 1.0f)
        << "Pain sensitivity should be reduced during acute stress";

    // Test pain modulation
    float raw_pain = 0.6f;
    float modulated_pain = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_modulate_pain(perception_bridge, raw_pain, &modulated_pain));
    EXPECT_LT(modulated_pain, raw_pain)
        << "Pain should be suppressed during acute stress (stress-induced analgesia)";

    // Phase 5: Threat passes, return to normal
    hypo_perception_bridge_set_arousal(perception_bridge, 0.4f);
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.2f, false));

    simulate_time(5 * 60 * 1000000ULL);  // 5 minutes

    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));
    hypo_perception_modulation_t recovery_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &recovery_mod));

    EXPECT_FALSE(recovery_mod.survival_mode)
        << "Survival mode should deactivate after threat passes";
    EXPECT_LT(recovery_mod.arousal_level, threat_mod.arousal_level)
        << "Arousal should return toward baseline";

    // Verify statistics
    hypo_perception_bridge_stats_t stats;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &stats));
    EXPECT_GT(stats.pain_stimuli_modulated, 0u);
}

// ============================================================================
// SCENARIO 3: SLEEP WAKE CYCLE
// ============================================================================
// Transition through sleep stages, test perception gating at each

TEST_F(HypothalamusPerceptionE2ETest, SleepWakeCycle) {
    struct SleepStageTest {
        hypo_sleep_stage_t stage;
        float expected_gate_min;
        float expected_gate_max;
        bool weak_stim_passes;   // Low intensity, non-threat
        bool strong_stim_passes; // High intensity
        bool threat_passes;      // Threat stimulus
        bool name_passes;        // Own name
    };

    std::vector<SleepStageTest> stages = {
        {HYPO_SLEEP_AWAKE, 0.9f, 1.0f, true, true, true, true},
        {HYPO_SLEEP_DROWSY, 0.6f, 0.9f, true, true, true, true},
        {HYPO_SLEEP_LIGHT, 0.3f, 0.6f, false, true, true, true},
        {HYPO_SLEEP_DEEP, 0.0f, 0.2f, false, false, true, true},
        {HYPO_SLEEP_REM, 0.1f, 0.3f, false, false, true, true},
    };

    for (const auto& test : stages) {
        // Set sleep stage
        ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, test.stage))
            << "Failed to set sleep stage: " << hypo_sleep_stage_name(test.stage);

        // Get sleep gating state
        hypo_sleep_gating_state_t gating;
        ASSERT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(perception_bridge, &gating));
        EXPECT_EQ(test.stage, gating.current_stage);
        EXPECT_GE(gating.perception_gate, test.expected_gate_min)
            << "Gate too low for " << hypo_sleep_stage_name(test.stage);
        EXPECT_LE(gating.perception_gate, test.expected_gate_max)
            << "Gate too high for " << hypo_sleep_stage_name(test.stage);

        // Test weak stimulus (low intensity, non-threat)
        bool passes = false;
        ASSERT_EQ(0, hypo_perception_bridge_check_sleep_gate(
            perception_bridge, 0.2f, false, false, &passes));
        EXPECT_EQ(test.weak_stim_passes, passes)
            << "Weak stimulus check failed for " << hypo_sleep_stage_name(test.stage);

        // Test strong stimulus
        ASSERT_EQ(0, hypo_perception_bridge_check_sleep_gate(
            perception_bridge, 0.9f, false, false, &passes));
        EXPECT_EQ(test.strong_stim_passes, passes)
            << "Strong stimulus check failed for " << hypo_sleep_stage_name(test.stage);

        // Test threat stimulus (should bypass gating in all stages)
        ASSERT_EQ(0, hypo_perception_bridge_check_sleep_gate(
            perception_bridge, 0.5f, true, false, &passes));
        EXPECT_EQ(test.threat_passes, passes)
            << "Threat stimulus check failed for " << hypo_sleep_stage_name(test.stage);

        // Test own name (should bypass gating)
        ASSERT_EQ(0, hypo_perception_bridge_check_sleep_gate(
            perception_bridge, 0.4f, false, true, &passes));
        EXPECT_EQ(test.name_passes, passes)
            << "Name stimulus check failed for " << hypo_sleep_stage_name(test.stage);
    }

    // Complete sleep cycle simulation
    // Evening: Awake -> Drowsy
    ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_AWAKE));
    simulate_time(30 * 60 * 1000000ULL);  // 30 min

    ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_DROWSY));
    simulate_time(15 * 60 * 1000000ULL);  // 15 min

    // Early sleep: Light -> Deep (SWS)
    ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_LIGHT));
    simulate_time(20 * 60 * 1000000ULL);  // 20 min

    ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_DEEP));
    simulate_time(60 * 60 * 1000000ULL);  // 60 min deep sleep

    // REM cycle
    ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_REM));
    simulate_time(20 * 60 * 1000000ULL);  // 20 min REM

    // Wake up
    ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_AWAKE));

    // Verify final state
    hypo_sleep_gating_state_t final_gating;
    ASSERT_EQ(0, hypo_perception_bridge_get_sleep_gating_state(perception_bridge, &final_gating));
    EXPECT_EQ(HYPO_SLEEP_AWAKE, final_gating.current_stage);
    EXPECT_GT(final_gating.perception_gate, 0.9f)
        << "Perception gate should be fully open when awake";

    // Verify statistics
    hypo_perception_bridge_stats_t stats;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &stats));
    EXPECT_GT(stats.sleep_gate_checks, 0u);
}

// ============================================================================
// SCENARIO 4: THERMAL REGULATION PIPELINE
// ============================================================================
// Cold exposure -> thermal discomfort -> warm-seeking behavior

TEST_F(HypothalamusPerceptionE2ETest, ThermalRegulationPipeline) {
    // Phase 1: Comfortable baseline (temperature at setpoint)
    ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, 0.5f));
    ASSERT_EQ(0, hypo_perception_bridge_set_ambient_temperature(perception_bridge, 0.5f));

    hypo_thermal_state_t baseline_thermal;
    ASSERT_EQ(0, hypo_perception_bridge_get_thermal_state(perception_bridge, &baseline_thermal));
    EXPECT_LT(baseline_thermal.thermal_discomfort, 0.2f)
        << "Thermal discomfort should be low at setpoint";
    EXPECT_FALSE(baseline_thermal.hypothermic_alert);
    EXPECT_FALSE(baseline_thermal.hyperthermic_alert);

    // Phase 2: Cold exposure (low ambient temperature)
    ASSERT_EQ(0, hypo_perception_bridge_set_ambient_temperature(perception_bridge, 0.2f));

    // Simulate 30 minutes in cold
    simulate_time(30 * 60 * 1000000ULL);

    // Core temperature drops
    ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, 0.35f));

    hypo_thermal_state_t cold_thermal;
    ASSERT_EQ(0, hypo_perception_bridge_get_thermal_state(perception_bridge, &cold_thermal));
    EXPECT_GT(cold_thermal.thermal_discomfort, baseline_thermal.thermal_discomfort)
        << "Cold exposure should increase thermal discomfort";
    EXPECT_GT(cold_thermal.warm_seeking, 0.3f)
        << "Warm-seeking motivation should increase when cold";
    EXPECT_LT(cold_thermal.cool_seeking, cold_thermal.warm_seeking)
        << "Cool-seeking should be lower than warm-seeking when cold";

    // Check for hypothermic alert at low temperatures
    ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, 0.15f));
    hypo_thermal_state_t hypothermic;
    ASSERT_EQ(0, hypo_perception_bridge_get_thermal_state(perception_bridge, &hypothermic));
    EXPECT_TRUE(hypothermic.hypothermic_alert)
        << "Should trigger hypothermic alert at very low core temperature";

    // Phase 3: Compute thermal salience boost
    float boost = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_compute_thermal_salience(perception_bridge, &boost));
    EXPECT_GT(boost, 1.0f) << "Thermal salience should be boosted when cold";
    EXPECT_LE(boost, 3.0f) << "Thermal salience boost should be within range";

    // Phase 4: Agent finds warmth
    ASSERT_EQ(0, hypo_perception_bridge_set_ambient_temperature(perception_bridge, 0.6f));

    // Simulate warming up over 20 minutes
    for (int i = 0; i < 20; i++) {
        float temp = 0.15f + (0.5f - 0.15f) * (i / 20.0f);
        ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, temp));
        simulate_time(60 * 1000000ULL);  // 1 minute
    }

    // Phase 5: Return to comfort
    ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, 0.5f));

    hypo_thermal_state_t warm_thermal;
    ASSERT_EQ(0, hypo_perception_bridge_get_thermal_state(perception_bridge, &warm_thermal));
    EXPECT_LT(warm_thermal.thermal_discomfort, cold_thermal.thermal_discomfort)
        << "Thermal discomfort should decrease after warming";
    EXPECT_LT(warm_thermal.warm_seeking, cold_thermal.warm_seeking)
        << "Warm-seeking motivation should decrease";
    EXPECT_FALSE(warm_thermal.hypothermic_alert);
    EXPECT_FALSE(warm_thermal.hyperthermic_alert);

    // Phase 6: Heat exposure test
    ASSERT_EQ(0, hypo_perception_bridge_set_ambient_temperature(perception_bridge, 0.85f));
    ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, 0.7f));

    hypo_thermal_state_t hot_thermal;
    ASSERT_EQ(0, hypo_perception_bridge_get_thermal_state(perception_bridge, &hot_thermal));
    EXPECT_GT(hot_thermal.cool_seeking, hot_thermal.warm_seeking)
        << "Cool-seeking should exceed warm-seeking when hot";

    // Very high temperature triggers hyperthermic alert
    ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, 0.85f));
    hypo_thermal_state_t hyperthermic;
    ASSERT_EQ(0, hypo_perception_bridge_get_thermal_state(perception_bridge, &hyperthermic));
    EXPECT_TRUE(hyperthermic.hyperthermic_alert)
        << "Should trigger hyperthermic alert at very high core temperature";

    // Verify statistics
    hypo_perception_bridge_stats_t stats;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &stats));
    EXPECT_GT(stats.thermal_updates, 0u);
}

// ============================================================================
// SCENARIO 5: PREDICTIVE CODING PIPELINE
// ============================================================================
// Generate predictions -> receive sensory input -> update errors -> compute free energy

TEST_F(HypothalamusPerceptionE2ETest, PredictiveCodingPipeline) {
    // Phase 1: Generate baseline predictions
    // Agent expects food in environment (based on context/memory)
    ASSERT_EQ(0, hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.7f, 0.8f));

    // Agent expects no threat
    ASSERT_EQ(0, hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_THREAT_PRESENCE, 0.1f, 0.9f));

    // Agent expects social interaction
    ASSERT_EQ(0, hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_SOCIAL_INTERACTION, 0.5f, 0.6f));

    // Get predictive state
    hypo_predictive_state_t pred_state;
    ASSERT_EQ(0, hypo_perception_bridge_get_predictive_state(perception_bridge, &pred_state));
    EXPECT_FLOAT_EQ(0.7f, pred_state.predictions[HYPO_PRED_FOOD_PRESENCE]);
    EXPECT_FLOAT_EQ(0.1f, pred_state.predictions[HYPO_PRED_THREAT_PRESENCE]);
    EXPECT_FLOAT_EQ(0.8f, pred_state.precision[HYPO_PRED_FOOD_PRESENCE]);

    // Phase 2: Sensory input matches prediction (food found as expected)
    ASSERT_EQ(0, hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.75f));

    // Compute free energy
    float free_energy_matched = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_compute_free_energy(perception_bridge, &free_energy_matched));

    // Phase 3: Unexpected event (threat appears - high prediction error)
    ASSERT_EQ(0, hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_THREAT_PRESENCE, 0.8f));  // Actual = 0.8, predicted = 0.1

    float free_energy_surprised = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_compute_free_energy(perception_bridge, &free_energy_surprised));

    // Free energy should be higher when predictions violated
    EXPECT_GT(free_energy_surprised, free_energy_matched)
        << "Free energy should increase when predictions are violated (surprise)";

    // Get updated predictive state
    ASSERT_EQ(0, hypo_perception_bridge_get_predictive_state(perception_bridge, &pred_state));
    EXPECT_GT(fabsf(pred_state.prediction_errors[HYPO_PRED_THREAT_PRESENCE]),
              fabsf(pred_state.prediction_errors[HYPO_PRED_FOOD_PRESENCE]))
        << "Threat prediction error should be larger";

    // Phase 4: Update predictions based on new information
    // Now expect threat (update model of world)
    ASSERT_EQ(0, hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_THREAT_PRESENCE, 0.7f, 0.85f));

    // Observation matches new prediction
    ASSERT_EQ(0, hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_THREAT_PRESENCE, 0.72f));

    float free_energy_adapted = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_compute_free_energy(perception_bridge, &free_energy_adapted));

    // Free energy should decrease after adapting predictions
    EXPECT_LT(free_energy_adapted, free_energy_surprised)
        << "Free energy should decrease after updating predictions to match reality";

    // Phase 5: Complex scenario with multiple predictions
    // Temperature change prediction
    ASSERT_EQ(0, hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_TEMPERATURE_CHANGE, 0.3f, 0.7f));
    ASSERT_EQ(0, hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_TEMPERATURE_CHANGE, 0.35f));

    // Reward prediction
    ASSERT_EQ(0, hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_REWARD_AVAILABLE, 0.6f, 0.75f));
    ASSERT_EQ(0, hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_REWARD_AVAILABLE, 0.55f));

    float final_free_energy = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_compute_free_energy(perception_bridge, &final_free_energy));
    EXPECT_GE(final_free_energy, 0.0f) << "Free energy should be non-negative";

    // Verify active inference flag
    ASSERT_EQ(0, hypo_perception_bridge_get_predictive_state(perception_bridge, &pred_state));
    EXPECT_TRUE(pred_state.active_inference_enabled)
        << "Active inference should be enabled";

    // Verify statistics
    hypo_perception_bridge_stats_t stats;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &stats));
    EXPECT_GT(stats.predictions_generated, 0u);
    EXPECT_GT(stats.prediction_errors_updated, 0u);
    EXPECT_GT(stats.avg_free_energy, 0.0f);
}

// ============================================================================
// SCENARIO 6: PAIN-STRESS-ENDORPHIN PIPELINE
// ============================================================================
// Stress -> pain modulation -> endorphin release -> recovery

TEST_F(HypothalamusPerceptionE2ETest, PainStressEndorphinPipeline) {
    // Phase 1: Baseline pain sensitivity
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.1f, false));

    hypo_pain_modulation_state_t baseline_pain;
    ASSERT_EQ(0, hypo_perception_bridge_get_pain_state(perception_bridge, &baseline_pain));
    float baseline_sensitivity = baseline_pain.pain_sensitivity;
    EXPECT_GT(baseline_sensitivity, 0.9f) << "Baseline pain sensitivity should be near 1.0";
    EXPECT_LT(baseline_pain.stress_level, 0.2f);
    EXPECT_FALSE(baseline_pain.in_acute_stress);
    EXPECT_FALSE(baseline_pain.in_chronic_stress);

    // Test baseline pain modulation
    float raw_pain = 0.5f;
    float modulated_baseline = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_modulate_pain(perception_bridge, raw_pain, &modulated_baseline));
    EXPECT_NEAR(modulated_baseline, raw_pain, 0.15f)
        << "Baseline pain modulation should be close to raw";

    // Phase 2: Acute stress response (fight-or-flight)
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.85f, false));

    hypo_pain_modulation_state_t acute_pain;
    ASSERT_EQ(0, hypo_perception_bridge_get_pain_state(perception_bridge, &acute_pain));
    EXPECT_TRUE(acute_pain.in_acute_stress);
    EXPECT_FALSE(acute_pain.in_chronic_stress);
    EXPECT_GT(acute_pain.analgesia_level, 0.3f)
        << "Acute stress should produce significant analgesia";
    EXPECT_LT(acute_pain.pain_sensitivity, baseline_sensitivity)
        << "Pain sensitivity should decrease during acute stress";

    // Test pain modulation during acute stress
    float modulated_acute = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_modulate_pain(perception_bridge, raw_pain, &modulated_acute));
    EXPECT_LT(modulated_acute, modulated_baseline)
        << "Pain should be reduced during acute stress (SIA)";

    // Phase 3: Endorphin release (e.g., from physical activity or reward)
    ASSERT_EQ(0, hypo_perception_bridge_release_endorphins(perception_bridge, 0.6f));

    hypo_pain_modulation_state_t endorphin_pain;
    ASSERT_EQ(0, hypo_perception_bridge_get_pain_state(perception_bridge, &endorphin_pain));
    EXPECT_GT(endorphin_pain.endorphin_level, acute_pain.endorphin_level)
        << "Endorphin level should increase after release";

    // Pain should be further reduced with endorphins
    float modulated_endorphin = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_modulate_pain(perception_bridge, raw_pain, &modulated_endorphin));
    EXPECT_LE(modulated_endorphin, modulated_acute)
        << "Endorphins should maintain or further reduce pain";

    // Phase 4: Recovery - stress subsides
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.2f, false));

    // Simulate recovery time
    simulate_time(30 * 60 * 1000000ULL);

    hypo_pain_modulation_state_t recovery_pain;
    ASSERT_EQ(0, hypo_perception_bridge_get_pain_state(perception_bridge, &recovery_pain));
    EXPECT_FALSE(recovery_pain.in_acute_stress)
        << "Should no longer be in acute stress after recovery";
    EXPECT_GT(recovery_pain.pain_sensitivity, acute_pain.pain_sensitivity)
        << "Pain sensitivity should return toward baseline";

    // Phase 5: Chronic stress scenario (different mechanism)
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.7f, true));

    hypo_pain_modulation_state_t chronic_pain;
    ASSERT_EQ(0, hypo_perception_bridge_get_pain_state(perception_bridge, &chronic_pain));
    EXPECT_FALSE(chronic_pain.in_acute_stress);
    EXPECT_TRUE(chronic_pain.in_chronic_stress);
    EXPECT_GT(chronic_pain.hyperalgesia_level, 0.0f)
        << "Chronic stress should produce hyperalgesia";
    EXPECT_GT(chronic_pain.pain_sensitivity, 1.0f)
        << "Chronic stress should increase pain sensitivity (hyperalgesia)";

    // Pain should be amplified in chronic stress
    float modulated_chronic = 0.0f;
    ASSERT_EQ(0, hypo_perception_bridge_modulate_pain(perception_bridge, raw_pain, &modulated_chronic));
    EXPECT_GT(modulated_chronic, raw_pain)
        << "Chronic stress should amplify pain (hyperalgesia)";

    // Verify statistics
    hypo_perception_bridge_stats_t stats;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &stats));
    EXPECT_GT(stats.pain_stimuli_modulated, 0u);
}

// ============================================================================
// SCENARIO 7: FULL SENSORY INTEGRATION
// ============================================================================
// Multiple modalities, all enhancements active, realistic scenario

TEST_F(HypothalamusPerceptionE2ETest, FullSensoryIntegration) {
    // Scenario: Agent waking up, finding food, dealing with mild threat, eating

    // === MORNING WAKE-UP ===
    ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_AWAKE));
    hypo_perception_bridge_set_arousal(perception_bridge, 0.5f);

    // Set comfortable temperature
    ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, 0.5f));
    ASSERT_EQ(0, hypo_perception_bridge_set_ambient_temperature(perception_bridge, 0.5f));

    // Low stress baseline
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.15f, false));

    // Interoceptive signals: mild hunger
    ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.4f));

    // Simulate morning drive build-up
    simulate_time(2 * 3600 * 1000000ULL);

    // === FOOD SEARCH ===
    // Predict food presence
    ASSERT_EQ(0, hypo_perception_bridge_generate_prediction(
        perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.6f, 0.7f));

    // Compute modulation
    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

    hypo_perception_modulation_t search_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &search_mod));

    // Food salience should be boosted due to hunger
    EXPECT_GT(search_mod.category_salience[HYPO_STIM_FOOD], 0.3f);

    // === FOOD DETECTED ===
    // Smell food
    ASSERT_EQ(0, hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.7f));

    // Visual detection
    hypo_sensory_detection_t food_detection = {
        .detected_category = HYPO_STIM_FOOD,
        .modality = HYPO_SENSE_VISUAL,
        .confidence = 0.85f,
        .intensity = 0.6f,
        .is_threat = false,
        .timestamp_us = 0
    };
    ASSERT_EQ(0, hypo_perception_bridge_process_detection(perception_bridge, &food_detection));

    // Update prediction - food found
    ASSERT_EQ(0, hypo_perception_bridge_update_prediction_error(
        perception_bridge, HYPO_PRED_FOOD_PRESENCE, 0.9f));

    // Check anticipation
    float hunger_anticipation = hypo_perception_bridge_get_anticipation(
        perception_bridge, HYPO_DRIVE_HUNGER);
    EXPECT_GT(hunger_anticipation, 0.2f);

    // === MILD THREAT INTERRUPTION ===
    // Auditory threat detected
    hypo_sensory_detection_t threat = {
        .detected_category = HYPO_STIM_THREAT,
        .modality = HYPO_SENSE_AUDITORY,
        .confidence = 0.6f,
        .intensity = 0.5f,
        .is_threat = true,
        .timestamp_us = 0
    };
    ASSERT_EQ(0, hypo_perception_bridge_process_detection(perception_bridge, &threat));

    // Arousal increases
    hypo_perception_bridge_set_arousal(perception_bridge, 0.75f);

    // Mild stress response
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.5f, false));

    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));
    hypo_perception_modulation_t threat_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &threat_mod));

    // Both threat and food should be salient
    EXPECT_GT(threat_mod.category_salience[HYPO_STIM_THREAT], 0.0f);
    EXPECT_GT(threat_mod.category_salience[HYPO_STIM_FOOD], 0.0f);

    // === THREAT PASSES ===
    simulate_time(5 * 60 * 1000000ULL);

    // Arousal decreases
    hypo_perception_bridge_set_arousal(perception_bridge, 0.55f);
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.2f, false));

    // === EATING ===
    // Taste the food
    ASSERT_EQ(0, hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_SWEET, 0.7f));
    ASSERT_EQ(0, hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_UMAMI, 0.6f));
    ASSERT_EQ(0, hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_FAT, 0.4f));

    // Satisfy hunger
    float reward = hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 0.8f);
    EXPECT_GT(reward, 0.0f);

    // Interoceptive: fullness
    ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.1f));

    // === VERIFY FINAL STATE ===
    simulate_time(30 * 60 * 1000000ULL);

    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));
    hypo_perception_modulation_t final_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &final_mod));

    // Food salience should be reduced after eating
    EXPECT_LT(final_mod.category_salience[HYPO_STIM_FOOD], search_mod.category_salience[HYPO_STIM_FOOD])
        << "Food salience should decrease after eating";

    // Get comprehensive statistics
    hypo_perception_bridge_stats_t stats;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &stats));

    // All subsystems should have been active
    EXPECT_GT(stats.modulations_computed, 0u);
    EXPECT_GT(stats.detections_processed, 0u);
    EXPECT_GT(stats.interoceptive_signals, 0u);
    EXPECT_GT(stats.olfactory_stimuli, 0u);
    EXPECT_GT(stats.gustatory_stimuli, 0u);
    EXPECT_GT(stats.predictions_generated, 0u);
    EXPECT_GT(stats.prediction_errors_updated, 0u);

    // Print summary
    hypo_perception_bridge_print_summary(perception_bridge);
}

// ============================================================================
// SCENARIO 8: CIRCADIAN PATTERN
// ============================================================================
// Day/night cycle affecting perception

TEST_F(HypothalamusPerceptionE2ETest, CircadianPattern) {
    struct CircadianPhase {
        const char* name;
        int hour;
        float expected_arousal_min;
        float expected_arousal_max;
        hypo_sleep_stage_t typical_sleep_stage;
    };

    std::vector<CircadianPhase> phases = {
        {"Early Morning (6am)", 6, 0.3f, 0.6f, HYPO_SLEEP_DROWSY},
        {"Mid Morning (10am)", 10, 0.5f, 0.8f, HYPO_SLEEP_AWAKE},
        {"Afternoon (2pm)", 14, 0.4f, 0.7f, HYPO_SLEEP_AWAKE},
        {"Evening (7pm)", 19, 0.5f, 0.8f, HYPO_SLEEP_AWAKE},
        {"Night (11pm)", 23, 0.2f, 0.5f, HYPO_SLEEP_DROWSY},
        {"Deep Night (3am)", 3, 0.1f, 0.3f, HYPO_SLEEP_DEEP},
    };

    for (const auto& phase : phases) {
        // Set appropriate sleep stage for time of day
        ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(
            perception_bridge, phase.typical_sleep_stage));

        // Set arousal appropriate for time
        float arousal = (phase.expected_arousal_min + phase.expected_arousal_max) / 2.0f;
        hypo_perception_bridge_set_arousal(perception_bridge, arousal);

        // Compute modulation
        ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

        hypo_perception_modulation_t mod;
        ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &mod));

        // Verify arousal affects perception
        if (phase.typical_sleep_stage == HYPO_SLEEP_AWAKE) {
            EXPECT_GT(mod.global_gain, 0.8f)
                << "Global gain should be high during " << phase.name;
        } else if (phase.typical_sleep_stage == HYPO_SLEEP_DEEP) {
            EXPECT_LT(mod.global_gain, 1.0f)
                << "Global gain should be reduced during " << phase.name;
        }

        // Simulate 1 hour at this phase
        simulate_time(3600 * 1000000ULL);
    }

    // Simulate full 24-hour cycle with gradual transitions
    for (int hour = 0; hour < 24; hour++) {
        // Determine sleep/wake state
        bool awake = (hour >= 7 && hour <= 23);

        if (awake) {
            ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_AWAKE));

            // Arousal varies through day (dip in early afternoon)
            float arousal = 0.6f;
            if (hour >= 13 && hour <= 15) {
                arousal = 0.45f;  // Post-lunch dip
            } else if (hour >= 20) {
                arousal = 0.4f;  // Evening decline
            }
            hypo_perception_bridge_set_arousal(perception_bridge, arousal);
        } else {
            // Sleep stages through night
            if (hour <= 1 || hour >= 23) {
                ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_LIGHT));
            } else if (hour <= 4) {
                ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_DEEP));
            } else {
                ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_REM));
            }
            hypo_perception_bridge_set_arousal(perception_bridge, 0.1f);
        }

        ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

        // Simulate 1 hour
        simulate_time(3600 * 1000000ULL);
    }

    // Verify bridge survived full cycle
    hypo_perception_bridge_stats_t stats;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &stats));
    EXPECT_GT(stats.modulations_computed, 20u)
        << "Should have computed many modulations over 24 hours";
}

// ============================================================================
// SCENARIO 9: HOMEOSTATIC REGULATION
// ============================================================================
// Multiple drives competing for attention

TEST_F(HypothalamusPerceptionE2ETest, HomeostaticRegulation) {
    // Scenario: Agent has multiple competing needs

    // Build up multiple drives
    simulate_time(3 * 3600 * 1000000ULL);  // 3 hours

    // Set various interoceptive signals
    ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.5f));
    ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_THIRST, 0.6f));
    ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_FATIGUE, 0.4f));

    // Get interoceptive state
    hypo_interoceptive_state_t intero;
    ASSERT_EQ(0, hypo_perception_bridge_get_interoceptive_state(perception_bridge, &intero));
    EXPECT_GT(intero.signals[HYPO_INTERO_HUNGER_PANGS], 0.0f);
    EXPECT_GT(intero.signals[HYPO_INTERO_THIRST], 0.0f);
    EXPECT_GT(intero.signals[HYPO_INTERO_FATIGUE], 0.0f);

    // Compute modulation
    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

    hypo_perception_modulation_t multi_drive_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &multi_drive_mod));

    // Both food and water stimuli should be salient
    EXPECT_GT(multi_drive_mod.category_salience[HYPO_STIM_FOOD], 0.0f)
        << "Food salience should be elevated from hunger";
    EXPECT_GT(multi_drive_mod.category_salience[HYPO_STIM_WATER], 0.0f)
        << "Water salience should be elevated from thirst";

    // Determine priority drive
    hypo_drive_type_t priority = hypo_drive_get_priority(drives);

    // Satisfy the most urgent drive first
    if (priority == HYPO_DRIVE_THIRST) {
        // Thirst is most urgent - find and drink water
        hypo_sensory_detection_t water = {
            .detected_category = HYPO_STIM_WATER,
            .modality = HYPO_SENSE_VISUAL,
            .confidence = 0.9f,
            .intensity = 0.7f,
            .is_threat = false,
            .timestamp_us = 0
        };
        ASSERT_EQ(0, hypo_perception_bridge_process_detection(perception_bridge, &water));

        float thirst_anticipation = hypo_perception_bridge_get_anticipation(
            perception_bridge, HYPO_DRIVE_THIRST);
        EXPECT_GT(thirst_anticipation, 0.0f);

        // Drink water
        hypo_drive_satisfy(drives, HYPO_DRIVE_THIRST, 0.85f);

        // Thirst interoception decreases
        ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
            perception_bridge, HYPO_INTERO_THIRST, 0.1f));
    }

    simulate_time(10 * 60 * 1000000ULL);  // 10 minutes

    // Recompute - next priority should emerge
    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

    hypo_perception_modulation_t after_satisfy_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &after_satisfy_mod));

    // Previously satisfied drive's salience should decrease
    if (priority == HYPO_DRIVE_THIRST) {
        EXPECT_LT(after_satisfy_mod.category_salience[HYPO_STIM_WATER],
                  multi_drive_mod.category_salience[HYPO_STIM_WATER])
            << "Water salience should decrease after drinking";
    }

    // Now satisfy hunger
    hypo_sensory_detection_t food = {
        .detected_category = HYPO_STIM_FOOD,
        .modality = HYPO_SENSE_OLFACTORY,
        .confidence = 0.85f,
        .intensity = 0.6f,
        .is_threat = false,
        .timestamp_us = 0
    };
    ASSERT_EQ(0, hypo_perception_bridge_process_detection(perception_bridge, &food));

    // Eat
    ASSERT_EQ(0, hypo_perception_bridge_process_gustatory(perception_bridge, HYPO_GUST_UMAMI, 0.7f));
    hypo_drive_satisfy(drives, HYPO_DRIVE_HUNGER, 0.8f);

    // Update interoception
    ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.1f));

    simulate_time(30 * 60 * 1000000ULL);

    // Now fatigue might be priority
    priority = hypo_drive_get_priority(drives);

    // If fatigue is high, should affect perception
    if (priority == HYPO_DRIVE_FATIGUE) {
        // Transition toward sleep
        ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_DROWSY));
        hypo_perception_bridge_set_arousal(perception_bridge, 0.3f);

        ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));
        hypo_perception_modulation_t drowsy_mod;
        ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &drowsy_mod));

        EXPECT_LT(drowsy_mod.global_gain, multi_drive_mod.global_gain)
            << "Global sensory gain should decrease when drowsy";
    }

    // Verify homeostatic balance was maintained
    float urgencies[HYPO_DRIVE_COUNT];
    ASSERT_TRUE(hypo_drive_get_urgencies(drives, urgencies));

    // After satisfying hunger and thirst, their urgencies should be lower
    // Fatigue urgency might still be elevated
    float satisfied_sum = urgencies[HYPO_DRIVE_HUNGER] + urgencies[HYPO_DRIVE_THIRST];
    EXPECT_LT(satisfied_sum, 1.0f)
        << "Combined hunger+thirst urgency should be lower after satisfaction";
}

// ============================================================================
// ADDITIONAL TESTS: EDGE CASES AND ROBUSTNESS
// ============================================================================

TEST_F(HypothalamusPerceptionE2ETest, InteroceptiveAccuracyModulation) {
    // Test body awareness affects interoceptive processing

    // Low interoceptive accuracy (poor body awareness)
    ASSERT_EQ(0, hypo_perception_bridge_set_interoceptive_accuracy(perception_bridge, 0.3f));

    ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HUNGER_PANGS, 0.6f));

    hypo_interoceptive_state_t low_accuracy;
    ASSERT_EQ(0, hypo_perception_bridge_get_interoceptive_state(perception_bridge, &low_accuracy));
    EXPECT_FLOAT_EQ(0.3f, low_accuracy.global_interoceptive_accuracy);

    // High interoceptive accuracy (good body awareness)
    ASSERT_EQ(0, hypo_perception_bridge_set_interoceptive_accuracy(perception_bridge, 0.9f));

    ASSERT_EQ(0, hypo_perception_bridge_process_interoceptive(
        perception_bridge, HYPO_INTERO_HEART_RATE, 0.7f));

    hypo_interoceptive_state_t high_accuracy;
    ASSERT_EQ(0, hypo_perception_bridge_get_interoceptive_state(perception_bridge, &high_accuracy));
    EXPECT_FLOAT_EQ(0.9f, high_accuracy.global_interoceptive_accuracy);

    // Salience should be higher with better accuracy
    EXPECT_GT(high_accuracy.salience[HYPO_INTERO_HEART_RATE], 0.0f);
}

TEST_F(HypothalamusPerceptionE2ETest, ChemosensoryToxinDetection) {
    // Test detection of toxic/spoiled food

    // Detect spoiled food smell
    ASSERT_EQ(0, hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_FOOD_UNPLEASANT, 0.8f));

    hypo_chemosensory_state_t chem;
    ASSERT_EQ(0, hypo_perception_bridge_get_chemosensory_state(perception_bridge, &chem));
    EXPECT_TRUE(chem.toxin_detected) << "Unpleasant food smell should trigger toxin detection";
    EXPECT_FALSE(chem.food_detected) << "Should not detect as edible food";

    // Bitter taste (poison warning)
    ASSERT_EQ(0, hypo_perception_bridge_process_gustatory(
        perception_bridge, HYPO_GUST_BITTER, 0.9f));

    ASSERT_EQ(0, hypo_perception_bridge_get_chemosensory_state(perception_bridge, &chem));
    EXPECT_TRUE(chem.toxin_detected) << "Bitter taste should reinforce toxin detection";
}

TEST_F(HypothalamusPerceptionE2ETest, ResetAndRecover) {
    // Test that bridge can be reset and continue functioning

    // Build up some state
    hypo_perception_bridge_set_arousal(perception_bridge, 0.8f);
    ASSERT_EQ(0, hypo_perception_bridge_set_sleep_stage(perception_bridge, HYPO_SLEEP_AWAKE));
    ASSERT_EQ(0, hypo_perception_bridge_set_core_temperature(perception_bridge, 0.6f));
    ASSERT_EQ(0, hypo_perception_bridge_set_stress_for_pain(perception_bridge, 0.5f, false));

    ASSERT_EQ(0, hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_FOOD_PLEASANT, 0.7f));

    simulate_time(60 * 1000000ULL);

    // Get stats before reset
    hypo_perception_bridge_stats_t before_reset;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &before_reset));
    EXPECT_GT(before_reset.olfactory_stimuli, 0u);

    // Reset the bridge
    ASSERT_EQ(0, hypo_perception_bridge_reset(perception_bridge));

    // Stats should be reset
    hypo_perception_bridge_stats_t after_reset;
    ASSERT_EQ(0, hypo_perception_bridge_get_stats(perception_bridge, &after_reset));
    EXPECT_EQ(0u, after_reset.olfactory_stimuli);

    // Bridge should still function
    ASSERT_EQ(0, hypo_perception_bridge_process_olfactory(
        perception_bridge, HYPO_OLFACT_DANGER, 0.5f));

    ASSERT_EQ(0, hypo_perception_bridge_compute_modulation(perception_bridge));

    hypo_perception_modulation_t mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &mod));
    assert_valid_modulation(mod);
}

TEST_F(HypothalamusPerceptionE2ETest, ConcurrentUpdates) {
    // Test thread safety with concurrent operations

    std::atomic<bool> running{true};
    std::atomic<int> errors{0};

    // Thread 1: Continuous updates
    std::thread update_thread([this, &running, &errors]() {
        while (running.load()) {
            if (hypo_perception_bridge_update(perception_bridge, 10000) != 0) {
                errors++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 2: Compute modulations
    std::thread modulation_thread([this, &running, &errors]() {
        while (running.load()) {
            if (hypo_perception_bridge_compute_modulation(perception_bridge) != 0) {
                errors++;
            }
            hypo_perception_modulation_t mod;
            hypo_perception_bridge_get_modulation(perception_bridge, &mod);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });

    // Thread 3: Process sensory inputs
    std::thread sensory_thread([this, &running, &errors]() {
        int cycle = 0;
        while (running.load()) {
            hypo_sensory_detection_t det = {
                .detected_category = static_cast<hypo_stim_category_t>(cycle % HYPO_STIM_COUNT),
                .modality = static_cast<hypo_sense_modality_t>(cycle % HYPO_SENSE_COUNT),
                .confidence = 0.5f,
                .intensity = 0.5f,
                .is_threat = false,
                .timestamp_us = 0
            };
            hypo_perception_bridge_process_detection(perception_bridge, &det);
            cycle++;
            std::this_thread::sleep_for(std::chrono::microseconds(150));
        }
    });

    // Run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    update_thread.join();
    modulation_thread.join();
    sensory_thread.join();

    EXPECT_EQ(0, errors.load()) << "Should have no errors during concurrent operations";

    // Verify bridge still functional
    hypo_perception_modulation_t final_mod;
    ASSERT_EQ(0, hypo_perception_bridge_get_modulation(perception_bridge, &final_mod));
    assert_valid_modulation(final_mod);
}

TEST_F(HypothalamusPerceptionE2ETest, StringUtilitiesWork) {
    // Verify all string utility functions work

    // Interoceptive types
    for (int i = 0; i < HYPO_INTERO_COUNT; i++) {
        const char* name = hypo_interoceptive_type_name(static_cast<hypo_interoceptive_type_t>(i));
        EXPECT_NE(nullptr, name);
        EXPECT_GT(strlen(name), 0u);
    }

    // Olfactory types
    for (int i = 0; i < HYPO_OLFACT_COUNT; i++) {
        const char* name = hypo_olfactory_type_name(static_cast<hypo_olfactory_type_t>(i));
        EXPECT_NE(nullptr, name);
        EXPECT_GT(strlen(name), 0u);
    }

    // Gustatory types
    for (int i = 0; i < HYPO_GUST_COUNT; i++) {
        const char* name = hypo_gustatory_type_name(static_cast<hypo_gustatory_type_t>(i));
        EXPECT_NE(nullptr, name);
        EXPECT_GT(strlen(name), 0u);
    }

    // Prediction types
    for (int i = 0; i < HYPO_PRED_COUNT; i++) {
        const char* name = hypo_prediction_type_name(static_cast<hypo_prediction_type_t>(i));
        EXPECT_NE(nullptr, name);
        EXPECT_GT(strlen(name), 0u);
    }

    // Sleep stages
    for (int i = 0; i < HYPO_SLEEP_COUNT; i++) {
        const char* name = hypo_sleep_stage_name(static_cast<hypo_sleep_stage_t>(i));
        EXPECT_NE(nullptr, name);
        EXPECT_GT(strlen(name), 0u);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
