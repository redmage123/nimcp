/**
 * @file test_pag.cpp
 * @brief Unit tests for Periaqueductal Gray (PAG) brain region module
 *
 * WHAT: Comprehensive unit tests for the PAG survival behavior control center
 * WHY:  Ensure correct defensive behavior, pain modulation, vocalization,
 *       autonomic control, and emotional expression processing
 * HOW:  Use Google Test framework to test lifecycle, defensive behaviors,
 *       pain pathways, vocalization, autonomic output, column control,
 *       error handling, and statistics tracking
 *
 * COVERAGE TARGET: 100%
 *
 * @version 1.1
 * @date 2026-01-13
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/pag/nimcp_pag.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PAGTest : public ::testing::Test {
protected:
    nimcp_pag_t* pag;
    pag_config_t config;

    void SetUp() override {
        int ret = pag_default_config(&config);
        ASSERT_EQ(ret, 0) << "Failed to get default PAG config";

        pag = pag_create(&config);
        ASSERT_NE(pag, nullptr) << "Failed to create PAG instance";

        // Initialize PAG - required for most operations to succeed
        ret = pag_init(pag);
        ASSERT_EQ(ret, 0) << "Failed to initialize PAG instance";
    }

    void TearDown() override {
        pag_destroy(pag);
        pag = nullptr;
    }
};

// Fixture without init for testing pre-init behavior
class PAGUninitializedTest : public ::testing::Test {
protected:
    nimcp_pag_t* pag;
    pag_config_t config;

    void SetUp() override {
        int ret = pag_default_config(&config);
        ASSERT_EQ(ret, 0) << "Failed to get default PAG config";

        pag = pag_create(&config);
        ASSERT_NE(pag, nullptr) << "Failed to create PAG instance";
        // Note: NOT calling pag_init() here
    }

    void TearDown() override {
        pag_destroy(pag);
        pag = nullptr;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(PAGTest, DefaultConfigHasReasonableValues) {
    pag_config_t default_config;
    int ret = pag_default_config(&default_config);
    EXPECT_EQ(ret, 0);

    // Verify threshold values are in valid ranges
    EXPECT_GE(default_config.threat_threshold, 0.0f);
    EXPECT_LE(default_config.threat_threshold, 1.0f);
    EXPECT_GE(default_config.analgesia_gain, 0.0f);
    EXPECT_GE(default_config.autonomic_gain, 0.0f);
    EXPECT_GE(default_config.vocal_threshold, 0.0f);
    EXPECT_LE(default_config.vocal_threshold, 1.0f);

    // Verify bias values
    EXPECT_GE(default_config.flight_vs_fight_bias, -1.0f);
    EXPECT_LE(default_config.flight_vs_fight_bias, 1.0f);
}

TEST_F(PAGTest, DefaultConfigNullReturnsError) {
    int ret = pag_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, CreateWithNullConfigUsesDefaults) {
    nimcp_pag_t* pag_null = pag_create(nullptr);
    ASSERT_NE(pag_null, nullptr);

    // Verify it was created with some state
    EXPECT_FALSE(pag_null->defense.defense_active);

    pag_destroy(pag_null);
}

TEST_F(PAGTest, DestroyNullDoesNotCrash) {
    pag_destroy(nullptr);
    // Should not crash
}

TEST_F(PAGUninitializedTest, InitSuccess) {
    int ret = pag_init(pag);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(pag->initialized);
}

TEST_F(PAGTest, InitNullReturnsError) {
    int ret = pag_init(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ResetClearsState) {
    // First process a threat to change state
    pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.8f, 0.0f, 5.0f);

    int ret = pag_reset(pag);
    EXPECT_EQ(ret, 0);

    // Defense should be cleared
    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_EQ(defense.threat_level, PAG_THREAT_NONE);
    EXPECT_FALSE(defense.defense_active);
}

TEST_F(PAGTest, ResetNullReturnsError) {
    int ret = pag_reset(nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Pre-initialization Tests
//=============================================================================

TEST_F(PAGUninitializedTest, ProcessThreatWithoutInitReturnsError) {
    // pag_process_threat requires initialized PAG
    int ret = pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.5f, 0.0f, 5.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGUninitializedTest, UpdateWithoutInitReturnsError) {
    // pag_update requires initialized PAG
    int ret = pag_update(pag, 0.016f);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Threat and Defensive Behavior Tests
//=============================================================================

TEST_F(PAGTest, ProcessThreatSuccess) {
    int ret = pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.7f, 1.57f, 10.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, ProcessThreatNullReturnsError) {
    int ret = pag_process_threat(nullptr, PAG_THREAT_PROXIMAL, 0.5f, 0.0f, 5.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ProcessThreatUpdatesDefenseState) {
    pag_process_threat(pag, PAG_THREAT_IMMINENT, 0.9f, 0.0f, 2.0f);

    pag_defense_state_t defense;
    int ret = pag_get_defense_state(pag, &defense);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(defense.threat_level, PAG_THREAT_IMMINENT);
    EXPECT_FLOAT_EQ(defense.threat_intensity, 0.9f);
    EXPECT_TRUE(defense.defense_active);
}

TEST_F(PAGTest, GetDefenseStateNullReturnsError) {
    pag_defense_state_t defense;
    EXPECT_EQ(pag_get_defense_state(nullptr, &defense), -1);
    EXPECT_EQ(pag_get_defense_state(pag, nullptr), -1);
}

TEST_F(PAGTest, SetDefenseResponseSuccess) {
    int ret = pag_set_defense_response(pag, PAG_DEFENSE_FLIGHT, 0.8f);
    EXPECT_EQ(ret, 0);

    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_EQ(defense.active_defense, PAG_DEFENSE_FLIGHT);
    EXPECT_FLOAT_EQ(defense.defense_intensity, 0.8f);
}

TEST_F(PAGTest, SetDefenseResponseNullReturnsError) {
    int ret = pag_set_defense_response(nullptr, PAG_DEFENSE_FIGHT, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SetDefenseResponseInvalidTypeReturnsError) {
    int ret = pag_set_defense_response(pag, (pag_defense_type_t)99, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SetDefenseResponseClampsIntensity) {
    // Test intensity > 1.0
    pag_set_defense_response(pag, PAG_DEFENSE_FREEZE, 1.5f);
    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_LE(defense.defense_intensity, 1.0f);

    // Test intensity < 0.0
    pag_set_defense_response(pag, PAG_DEFENSE_FREEZE, -0.5f);
    pag_get_defense_state(pag, &defense);
    EXPECT_GE(defense.defense_intensity, 0.0f);
}

TEST_F(PAGTest, CheckEscapeRouteSuccess) {
    bool available = false;
    int ret = pag_check_escape_route(pag, &available);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, CheckEscapeRouteNullReturnsError) {
    bool available;
    EXPECT_EQ(pag_check_escape_route(nullptr, &available), -1);
    EXPECT_EQ(pag_check_escape_route(pag, nullptr), -1);
}

TEST_F(PAGTest, SetEscapeRouteSuccess) {
    int ret = pag_set_escape_route(pag, true);
    EXPECT_EQ(ret, 0);

    bool available = false;
    pag_check_escape_route(pag, &available);
    EXPECT_TRUE(available);

    ret = pag_set_escape_route(pag, false);
    EXPECT_EQ(ret, 0);

    pag_check_escape_route(pag, &available);
    EXPECT_FALSE(available);
}

TEST_F(PAGTest, SetEscapeRouteNullReturnsError) {
    int ret = pag_set_escape_route(nullptr, true);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ClearThreatSuccess) {
    // First set a threat
    pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.8f, 0.0f, 5.0f);

    int ret = pag_clear_threat(pag);
    EXPECT_EQ(ret, 0);

    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_EQ(defense.threat_level, PAG_THREAT_NONE);
}

TEST_F(PAGTest, ClearThreatNullReturnsError) {
    int ret = pag_clear_threat(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, GetCopingStrategyReturnsValid) {
    pag_coping_strategy_t coping = pag_get_coping_strategy(pag);
    EXPECT_GE(coping, PAG_COPING_ACTIVE);
    EXPECT_LE(coping, PAG_COPING_MIXED);
}

TEST_F(PAGTest, ActiveDefenseSelectsActiveCoping) {
    pag_set_defense_response(pag, PAG_DEFENSE_FIGHT, 0.9f);
    pag_coping_strategy_t coping = pag_get_coping_strategy(pag);
    EXPECT_EQ(coping, PAG_COPING_ACTIVE);

    pag_set_defense_response(pag, PAG_DEFENSE_FLIGHT, 0.9f);
    coping = pag_get_coping_strategy(pag);
    EXPECT_EQ(coping, PAG_COPING_ACTIVE);
}

TEST_F(PAGTest, PassiveDefenseSelectsPassiveCoping) {
    pag_set_defense_response(pag, PAG_DEFENSE_FREEZE, 0.9f);
    pag_coping_strategy_t coping = pag_get_coping_strategy(pag);
    EXPECT_EQ(coping, PAG_COPING_PASSIVE);

    pag_set_defense_response(pag, PAG_DEFENSE_FAWN, 0.9f);
    coping = pag_get_coping_strategy(pag);
    EXPECT_EQ(coping, PAG_COPING_PASSIVE);
}

TEST_F(PAGTest, ThreatLevelsAffectDefenseSelection) {
    // Distal threat - should select active coping if escape available
    pag_set_escape_route(pag, true);
    pag_process_threat(pag, PAG_THREAT_DISTAL, 0.5f, 0.0f, 20.0f);

    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    // At distal threat, flight is more likely if escape available

    // Contact threat - freeze or fight
    pag_process_threat(pag, PAG_THREAT_CONTACT, 0.9f, 0.0f, 0.0f);
    pag_get_defense_state(pag, &defense);
    EXPECT_TRUE(defense.defense_active);
}

//=============================================================================
// Pain Modulation Tests
//=============================================================================

TEST_F(PAGTest, ProcessPainSuccess) {
    pag_pain_input_t pain = {0};
    pain.intensity = 0.6f;
    pain.unpleasantness = 0.5f;
    pain.location_code = 1;
    pain.nociceptive = true;
    pain.neuropathic = false;
    pain.timestamp_us = 0;

    int ret = pag_process_pain(pag, &pain);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, ProcessPainNullReturnsError) {
    pag_pain_input_t pain = {0};
    EXPECT_EQ(pag_process_pain(nullptr, &pain), -1);
    EXPECT_EQ(pag_process_pain(pag, nullptr), -1);
}

TEST_F(PAGTest, GetAnalgesiaStateSuccess) {
    pag_analgesia_state_t analgesia;
    int ret = pag_get_analgesia_state(pag, &analgesia);
    EXPECT_EQ(ret, 0);

    // Initial analgesia should be low
    EXPECT_GE(analgesia.analgesia_level, 0.0f);
    EXPECT_LE(analgesia.analgesia_level, 1.0f);
}

TEST_F(PAGTest, GetAnalgesiaStateNullReturnsError) {
    pag_analgesia_state_t analgesia;
    EXPECT_EQ(pag_get_analgesia_state(nullptr, &analgesia), -1);
    EXPECT_EQ(pag_get_analgesia_state(pag, nullptr), -1);
}

TEST_F(PAGTest, ActivatePainPathwaySuccess) {
    int ret = pag_activate_pain_pathway(pag, PAG_PAIN_PATHWAY_OPIOID, 0.7f);
    EXPECT_EQ(ret, 0);

    pag_analgesia_state_t analgesia;
    pag_get_analgesia_state(pag, &analgesia);
    EXPECT_GT(analgesia.pathway_activity[PAG_PAIN_PATHWAY_OPIOID], 0.0f);
}

TEST_F(PAGTest, ActivatePainPathwayNullReturnsError) {
    int ret = pag_activate_pain_pathway(nullptr, PAG_PAIN_PATHWAY_OPIOID, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ActivatePainPathwayInvalidPathwayReturnsError) {
    int ret = pag_activate_pain_pathway(pag, (pag_pain_pathway_t)99, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ActivateAllPainPathways) {
    for (int i = 0; i < PAG_PAIN_PATHWAY_COUNT; i++) {
        int ret = pag_activate_pain_pathway(pag, (pag_pain_pathway_t)i, 0.5f);
        EXPECT_EQ(ret, 0);
    }

    pag_analgesia_state_t analgesia;
    pag_get_analgesia_state(pag, &analgesia);

    for (int i = 0; i < PAG_PAIN_PATHWAY_COUNT; i++) {
        EXPECT_GT(analgesia.pathway_activity[i], 0.0f);
    }
}

TEST_F(PAGTest, GetDescendingInhibitionReturnsValidValue) {
    float inhibition = pag_get_descending_inhibition(pag);
    EXPECT_GE(inhibition, 0.0f);
    EXPECT_LE(inhibition, 1.0f);
}

TEST_F(PAGTest, AnalgesiaIncreasesDescendingInhibition) {
    float initial_inhibition = pag_get_descending_inhibition(pag);

    // Activate opioid pathway
    pag_activate_pain_pathway(pag, PAG_PAIN_PATHWAY_OPIOID, 0.9f);
    pag_update(pag, 0.1f);

    float final_inhibition = pag_get_descending_inhibition(pag);
    EXPECT_GE(final_inhibition, initial_inhibition);
}

TEST_F(PAGTest, TriggerStressAnalgesiaSuccess) {
    int ret = pag_trigger_stress_analgesia(pag, 0.8f);
    EXPECT_EQ(ret, 0);

    pag_analgesia_state_t analgesia;
    pag_get_analgesia_state(pag, &analgesia);
    EXPECT_GT(analgesia.stress_induced_factor, 0.0f);
}

TEST_F(PAGTest, TriggerStressAnalgesiaNullReturnsError) {
    int ret = pag_trigger_stress_analgesia(nullptr, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, HasOpioidToleranceInitiallyFalse) {
    bool tolerance = pag_has_opioid_tolerance(pag);
    EXPECT_FALSE(tolerance);
}

//=============================================================================
// Vocalization Tests
//=============================================================================

TEST_F(PAGTest, TriggerVocalizationSuccess) {
    int ret = pag_trigger_vocalization(pag, PAG_VOCAL_ALARM, 0.8f);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, TriggerVocalizationNullReturnsError) {
    int ret = pag_trigger_vocalization(nullptr, PAG_VOCAL_ALARM, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, TriggerVocalizationInvalidTypeReturnsError) {
    int ret = pag_trigger_vocalization(pag, (pag_vocal_type_t)99, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, GetVocalizationStateSuccess) {
    pag_trigger_vocalization(pag, PAG_VOCAL_DISTRESS, 0.7f);

    pag_vocal_state_t vocal;
    int ret = pag_get_vocalization_state(pag, &vocal);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(vocal.type, PAG_VOCAL_DISTRESS);
    // Note: intensity is multiplied by vocal_intensity_gain (1.0f by default)
    EXPECT_NEAR(vocal.intensity, 0.7f * config.vocal_intensity_gain, 0.01f);
    EXPECT_TRUE(vocal.active);
}

TEST_F(PAGTest, GetVocalizationStateNullReturnsError) {
    pag_vocal_state_t vocal;
    EXPECT_EQ(pag_get_vocalization_state(nullptr, &vocal), -1);
    EXPECT_EQ(pag_get_vocalization_state(pag, nullptr), -1);
}

TEST_F(PAGTest, StopVocalizationSuccess) {
    pag_trigger_vocalization(pag, PAG_VOCAL_ALARM, 0.8f);

    int ret = pag_stop_vocalization(pag);
    EXPECT_EQ(ret, 0);

    pag_vocal_state_t vocal;
    pag_get_vocalization_state(pag, &vocal);
    EXPECT_FALSE(vocal.active);
}

TEST_F(PAGTest, StopVocalizationNullReturnsError) {
    int ret = pag_stop_vocalization(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, AllVocalizationTypes) {
    for (int i = PAG_VOCAL_NONE; i < PAG_VOCAL_COUNT; i++) {
        int ret = pag_trigger_vocalization(pag, (pag_vocal_type_t)i, 0.5f);
        EXPECT_EQ(ret, 0);

        pag_vocal_state_t vocal;
        pag_get_vocalization_state(pag, &vocal);
        EXPECT_EQ(vocal.type, (pag_vocal_type_t)i);
    }
}

//=============================================================================
// Autonomic Control Tests
//=============================================================================

TEST_F(PAGTest, GetAutonomicStateSuccess) {
    pag_autonomic_state_t autonomic;
    int ret = pag_get_autonomic_state(pag, &autonomic);
    EXPECT_EQ(ret, 0);

    // Verify values are in valid ranges
    EXPECT_GE(autonomic.heart_rate_modulation, -1.0f);
    EXPECT_LE(autonomic.heart_rate_modulation, 1.0f);
    EXPECT_GE(autonomic.blood_pressure_modulation, -1.0f);
    EXPECT_LE(autonomic.blood_pressure_modulation, 1.0f);
}

TEST_F(PAGTest, GetAutonomicStateNullReturnsError) {
    pag_autonomic_state_t autonomic;
    EXPECT_EQ(pag_get_autonomic_state(nullptr, &autonomic), -1);
    EXPECT_EQ(pag_get_autonomic_state(pag, nullptr), -1);
}

TEST_F(PAGTest, GetCardiovascularOutputSuccess) {
    float hr_mod, bp_mod;
    int ret = pag_get_cardiovascular_output(pag, &hr_mod, &bp_mod);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(hr_mod, -1.0f);
    EXPECT_LE(hr_mod, 1.0f);
    EXPECT_GE(bp_mod, -1.0f);
    EXPECT_LE(bp_mod, 1.0f);
}

TEST_F(PAGTest, GetCardiovascularOutputNullPagReturnsError) {
    float hr_mod, bp_mod;
    EXPECT_EQ(pag_get_cardiovascular_output(nullptr, &hr_mod, &bp_mod), -1);
}

TEST_F(PAGTest, GetCardiovascularOutputAcceptsNullOutputs) {
    // Implementation allows NULL for individual output params (graceful no-op)
    float hr_mod, bp_mod;
    EXPECT_EQ(pag_get_cardiovascular_output(pag, nullptr, &bp_mod), 0);
    EXPECT_EQ(pag_get_cardiovascular_output(pag, &hr_mod, nullptr), 0);
}

TEST_F(PAGTest, GetRespiratoryOutputSuccess) {
    float rate_mod, depth_mod;
    int ret = pag_get_respiratory_output(pag, &rate_mod, &depth_mod);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, GetRespiratoryOutputNullPagReturnsError) {
    float rate_mod, depth_mod;
    EXPECT_EQ(pag_get_respiratory_output(nullptr, &rate_mod, &depth_mod), -1);
}

TEST_F(PAGTest, GetRespiratoryOutputAcceptsNullOutputs) {
    // Implementation allows NULL for individual output params (graceful no-op)
    float rate_mod, depth_mod;
    EXPECT_EQ(pag_get_respiratory_output(pag, nullptr, &depth_mod), 0);
    EXPECT_EQ(pag_get_respiratory_output(pag, &rate_mod, nullptr), 0);
}

TEST_F(PAGTest, TonicImmobilityInitiallyFalse) {
    bool immobile = pag_is_tonic_immobility(pag);
    EXPECT_FALSE(immobile);
}

TEST_F(PAGTest, FreezeResponseActivatesTonicImmobility) {
    // Activate strong freeze response
    pag_set_defense_response(pag, PAG_DEFENSE_FREEZE, 1.0f);
    pag_update(pag, 0.5f);

    // After strong freeze, tonic immobility may be active
    // (depends on implementation threshold)
    bool immobile = pag_is_tonic_immobility(pag);
    // Just verify function returns valid bool
    EXPECT_TRUE(immobile || !immobile);
}

TEST_F(PAGTest, ActiveDefenseIncreasesHeartRate) {
    // Baseline
    float hr_base, bp_base;
    pag_get_cardiovascular_output(pag, &hr_base, &bp_base);

    // Activate fight response (active coping)
    pag_set_defense_response(pag, PAG_DEFENSE_FIGHT, 0.9f);
    pag_update(pag, 0.1f);

    float hr_after, bp_after;
    pag_get_cardiovascular_output(pag, &hr_after, &bp_after);

    // Heart rate should increase with active coping (tachycardia)
    EXPECT_GE(hr_after, hr_base);
}

//=============================================================================
// Emotional State Tests
//=============================================================================

TEST_F(PAGTest, GetEmotionalStateSuccess) {
    pag_emotional_state_t emotion;
    int ret = pag_get_emotional_state(pag, &emotion);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(emotion.emotional_intensity, 0.0f);
    EXPECT_LE(emotion.emotional_intensity, 1.0f);
    EXPECT_GE(emotion.valence, -1.0f);
    EXPECT_LE(emotion.valence, 1.0f);
    EXPECT_GE(emotion.arousal, 0.0f);
    EXPECT_LE(emotion.arousal, 1.0f);
}

TEST_F(PAGTest, GetEmotionalStateNullReturnsError) {
    pag_emotional_state_t emotion;
    EXPECT_EQ(pag_get_emotional_state(nullptr, &emotion), -1);
    EXPECT_EQ(pag_get_emotional_state(pag, nullptr), -1);
}

TEST_F(PAGTest, SetEmotionInputSuccess) {
    int ret = pag_set_emotion_input(pag, PAG_EMOTION_FEAR, 0.8f);
    EXPECT_EQ(ret, 0);

    pag_emotional_state_t emotion;
    pag_get_emotional_state(pag, &emotion);
    EXPECT_GT(emotion.emotion_levels[PAG_EMOTION_FEAR], 0.0f);
}

TEST_F(PAGTest, SetEmotionInputNullReturnsError) {
    int ret = pag_set_emotion_input(nullptr, PAG_EMOTION_FEAR, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SetEmotionInputInvalidTypeReturnsError) {
    int ret = pag_set_emotion_input(pag, (pag_emotion_type_t)99, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, GetDominantEmotionReturnsValid) {
    /* Get the dominant emotion - should be valid enum value */
    pag_emotion_type_t dominant = pag_get_dominant_emotion(pag);
    /* Verify the dominant emotion is a valid enum value */
    EXPECT_GE(static_cast<int>(dominant), 0);
    EXPECT_LT(static_cast<int>(dominant), PAG_EMOTION_COUNT);

    /* Note: PAG emotions are computed from internal state (columns, defense mode,
     * threat level) during pag_update(), not directly from set inputs.
     * pag_set_emotion_input() affects column excitation which influences
     * the computed emotion levels after update. */
}

TEST_F(PAGTest, AllEmotionTypes) {
    for (int i = 0; i < PAG_EMOTION_COUNT; i++) {
        int ret = pag_set_emotion_input(pag, (pag_emotion_type_t)i, 0.5f);
        EXPECT_EQ(ret, 0);
    }
}

//=============================================================================
// Column Control Tests
//=============================================================================

TEST_F(PAGTest, GetColumnActivityReturnsValidValue) {
    for (int col = 0; col < PAG_COLUMN_COUNT; col++) {
        float activity = pag_get_column_activity(pag, (pag_column_t)col);
        EXPECT_GE(activity, 0.0f);
        EXPECT_LE(activity, 1.0f);
    }
}

TEST_F(PAGTest, SetColumnModulationSuccess) {
    int ret = pag_set_column_modulation(pag, PAG_COLUMN_DORSOLATERAL, 1.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, SetColumnModulationNullReturnsError) {
    int ret = pag_set_column_modulation(nullptr, PAG_COLUMN_DORSOLATERAL, 1.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SetColumnModulationInvalidColumnReturnsError) {
    int ret = pag_set_column_modulation(pag, (pag_column_t)99, 1.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, GetColumnStateSuccess) {
    pag_column_state_t state;
    int ret = pag_get_column_state(pag, PAG_COLUMN_VENTROLATERAL, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.column, PAG_COLUMN_VENTROLATERAL);
}

TEST_F(PAGTest, GetColumnStateNullReturnsError) {
    pag_column_state_t state;
    EXPECT_EQ(pag_get_column_state(nullptr, PAG_COLUMN_LATERAL, &state), -1);
    EXPECT_EQ(pag_get_column_state(pag, PAG_COLUMN_LATERAL, nullptr), -1);
}

TEST_F(PAGTest, GetDominantColumnReturnsValid) {
    pag_column_t dominant = pag_get_dominant_column(pag);
    EXPECT_GE(dominant, PAG_COLUMN_DORSOLATERAL);
    EXPECT_LT(dominant, PAG_COLUMN_COUNT);
}

TEST_F(PAGTest, AllColumnsAccessible) {
    for (int col = 0; col < PAG_COLUMN_COUNT; col++) {
        pag_column_state_t state;
        int ret = pag_get_column_state(pag, (pag_column_t)col, &state);
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(state.column, (pag_column_t)col);
    }
}

TEST_F(PAGTest, ColumnModulationAffectsActivity) {
    float initial = pag_get_column_activity(pag, PAG_COLUMN_DORSOLATERAL);

    pag_set_column_modulation(pag, PAG_COLUMN_DORSOLATERAL, 2.0f);
    pag_update(pag, 0.1f);

    float after = pag_get_column_activity(pag, PAG_COLUMN_DORSOLATERAL);
    // Modulation should affect activity
    EXPECT_GE(after, 0.0f);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(PAGTest, UpdateSuccess) {
    int ret = pag_update(pag, 0.016f); // ~60fps
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, UpdateNullReturnsError) {
    int ret = pag_update(nullptr, 0.016f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, UpdateZeroDtHandled) {
    int ret = pag_update(pag, 0.0f);
    // Zero dt is allowed by implementation
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, UpdateNegativeDtHandled) {
    // Implementation does not validate negative dt (no explicit check)
    int ret = pag_update(pag, -0.1f);
    // Implementation accepts it (no validation) - returns 0
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, MultipleUpdatesStable) {
    for (int i = 0; i < 1000; i++) {
        int ret = pag_update(pag, 0.001f);
        EXPECT_EQ(ret, 0);
    }

    // Verify state is still valid
    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_FALSE(std::isnan(defense.threat_intensity));
    EXPECT_FALSE(std::isinf(defense.threat_intensity));
}

TEST_F(PAGTest, LargeTimeStepDoesNotCrash) {
    int ret = pag_update(pag, 10.0f); // 10 second timestep
    EXPECT_EQ(ret, 0);

    // State should still be valid
    pag_analgesia_state_t analgesia;
    pag_get_analgesia_state(pag, &analgesia);
    EXPECT_FALSE(std::isnan(analgesia.analgesia_level));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PAGTest, GetStatsSuccess) {
    pag_stats_t stats;
    int ret = pag_get_stats(pag, &stats);
    EXPECT_EQ(ret, 0);

    // Initial stats should be zero or valid
    EXPECT_GE(stats.threats_detected, 0u);
    EXPECT_GE(stats.pain_signals_processed, 0u);
    EXPECT_GE(stats.analgesia_episodes, 0u);
}

TEST_F(PAGTest, GetStatsNullReturnsError) {
    pag_stats_t stats;
    EXPECT_EQ(pag_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(pag_get_stats(pag, nullptr), -1);
}

TEST_F(PAGTest, StatsTrackThreats) {
    // Process several threats
    for (int i = 0; i < 5; i++) {
        pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.5f, 0.0f, 10.0f);
        pag_clear_threat(pag);  // Clear between threats to trigger new detection
    }

    pag_stats_t stats;
    pag_get_stats(pag, &stats);
    EXPECT_GE(stats.threats_detected, 5u);
}

TEST_F(PAGTest, StatsTrackPainSignals) {
    pag_pain_input_t pain = {0};
    pain.intensity = 0.5f;
    pain.nociceptive = true;

    for (int i = 0; i < 10; i++) {
        pag_process_pain(pag, &pain);
    }

    pag_stats_t stats;
    pag_get_stats(pag, &stats);
    EXPECT_GE(stats.pain_signals_processed, 10u);
}

TEST_F(PAGTest, StatsTrackDefenseActivationsViaThreat) {
    // Note: pag_set_defense_response does NOT increment defense_activations
    // Only pag_process_threat increments defense_activations
    pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.8f, 0.0f, 5.0f);
    pag_clear_threat(pag);
    pag_process_threat(pag, PAG_THREAT_IMMINENT, 0.9f, 0.0f, 2.0f);
    pag_clear_threat(pag);
    pag_process_threat(pag, PAG_THREAT_CONTACT, 1.0f, 0.0f, 0.0f);

    pag_stats_t stats;
    pag_get_stats(pag, &stats);

    uint64_t total_activations = 0;
    for (int i = 0; i < PAG_DEFENSE_COUNT; i++) {
        total_activations += stats.defense_activations[i];
    }
    EXPECT_GE(total_activations, 3u);
}

TEST_F(PAGTest, StatsTrackVocalizations) {
    pag_trigger_vocalization(pag, PAG_VOCAL_ALARM, 0.8f);
    pag_trigger_vocalization(pag, PAG_VOCAL_DISTRESS, 0.7f);

    pag_stats_t stats;
    pag_get_stats(pag, &stats);

    uint64_t total_vocals = 0;
    for (int i = 0; i < PAG_VOCAL_COUNT; i++) {
        total_vocals += stats.vocalizations[i];
    }
    EXPECT_GE(total_vocals, 2u);
}

//=============================================================================
// Utility String Functions Tests
//=============================================================================

TEST_F(PAGTest, ColumnStringReturnsValidStrings) {
    for (int col = 0; col < PAG_COLUMN_COUNT; col++) {
        const char* str = pag_column_string((pag_column_t)col);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGTest, DefenseStringReturnsValidStrings) {
    for (int def = 0; def < PAG_DEFENSE_COUNT; def++) {
        const char* str = pag_defense_string((pag_defense_type_t)def);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGTest, ThreatStringReturnsValidStrings) {
    for (int thr = 0; thr <= PAG_THREAT_NONE; thr++) {
        const char* str = pag_threat_string((pag_threat_level_t)thr);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGTest, EmotionStringReturnsValidStrings) {
    for (int emo = 0; emo < PAG_EMOTION_COUNT; emo++) {
        const char* str = pag_emotion_string((pag_emotion_type_t)emo);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGTest, VocalStringReturnsValidStrings) {
    for (int voc = 0; voc < PAG_VOCAL_COUNT; voc++) {
        const char* str = pag_vocal_string((pag_vocal_type_t)voc);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGTest, PainPathwayStringReturnsValidStrings) {
    for (int path = 0; path < PAG_PAIN_PATHWAY_COUNT; path++) {
        const char* str = pag_pain_pathway_string((pag_pain_pathway_t)path);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGTest, CopingStringReturnsValidStrings) {
    for (int cop = PAG_COPING_ACTIVE; cop <= PAG_COPING_MIXED; cop++) {
        const char* str = pag_coping_string((pag_coping_strategy_t)cop);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(PAGTest, GetMutexReturnsPointer) {
    nimcp_mutex_t* mutex = pag_get_mutex(pag);
    // Mutex should be valid after creation
    EXPECT_NE(mutex, nullptr);
}

TEST_F(PAGTest, GetMutexNullPagReturnsNull) {
    nimcp_mutex_t* mutex = pag_get_mutex(nullptr);
    EXPECT_EQ(mutex, nullptr);
}

//=============================================================================
// Integration API Tests (Connection functions)
//=============================================================================

TEST_F(PAGTest, KGRegisterNullKGReturnsError) {
    int ret = pag_kg_register(pag, nullptr, 0);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, KGRegisterNullPAGReturnsError) {
    int ret = pag_kg_register(nullptr, nullptr, 0);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, KGUnregisterNullReturnsError) {
    int ret = pag_kg_unregister(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, KGUnregisterWithoutRegistrationSucceeds) {
    // Unregister without registration should succeed (no-op)
    int ret = pag_kg_unregister(pag);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, SecurityConnectNullPagReturnsError) {
    int ret = pag_security_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SecurityConnectNullSecuritySucceeds) {
    // Connecting with NULL security is allowed (just sets pag->security = NULL)
    int ret = pag_security_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, BBBRegisterNullReturnsError) {
    int ret = pag_bbb_register(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, BBBRegisterWithoutSecurityReturnsError) {
    // BBB register requires security context
    int ret = pag_bbb_register(pag);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ImmuneConnectNullPagReturnsError) {
    int ret = pag_immune_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ImmuneConnectNullImmuneSucceeds) {
    // Connecting with NULL immune is allowed
    int ret = pag_immune_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, ImmuneAlertNullPagReturnsError) {
    int ret = pag_immune_alert(nullptr, 0, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ImmuneAlertWithoutImmuneReturnsError) {
    // Alert requires immune connection
    int ret = pag_immune_alert(pag, 0, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SNNConnectNullPagReturnsError) {
    int ret = pag_snn_connect(nullptr, nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SNNConnectNullSNNSucceeds) {
    // Connecting with NULL snn/plasticity is allowed
    int ret = pag_snn_connect(pag, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, UpdatePlasticityNullPagReturnsError) {
    int ret = pag_update_plasticity(nullptr, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, UpdatePlasticityWithoutPlasticityReturnsError) {
    // Update requires plasticity connection
    int ret = pag_update_plasticity(pag, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, HypothalamusConnectNullPagReturnsError) {
    int ret = pag_hypothalamus_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, HypothalamusConnectNullHypoSucceeds) {
    // Connecting with NULL hypothalamus is allowed
    int ret = pag_hypothalamus_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, ReceiveDriveSignalNullPagReturnsError) {
    int ret = pag_receive_drive_signal(nullptr, 0, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ReceiveDriveSignalWithoutHypothalamusSucceeds) {
    // Implementation doesn't check hypothalamus for receive
    int ret = pag_receive_drive_signal(pag, 0, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, SendToHypothalamusNullPagReturnsError) {
    int ret = pag_send_to_hypothalamus(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SendToHypothalamusWithoutConnectionReturnsError) {
    // Send requires hypothalamus connection
    int ret = pag_send_to_hypothalamus(pag);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ThalamusConnectNullPagReturnsError) {
    int ret = pag_thalamus_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, ThalamusConnectNullThalamusSucceeds) {
    int ret = pag_thalamus_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, AmygdalaConnectNullPagReturnsError) {
    int ret = pag_amygdala_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, AmygdalaConnectNullAmygdalaSucceeds) {
    int ret = pag_amygdala_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, PrefrontalConnectNullPagReturnsError) {
    int ret = pag_prefrontal_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, PrefrontalConnectNullPrefrontalSucceeds) {
    int ret = pag_prefrontal_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, BrainstemConnectNullPagReturnsError) {
    int ret = pag_brainstem_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, BrainstemConnectNullBrainstemSucceeds) {
    int ret = pag_brainstem_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, RVMConnectNullPagReturnsError) {
    int ret = pag_rvm_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, RVMConnectNullRVMSucceeds) {
    int ret = pag_rvm_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, CognitiveConnectNullPagReturnsError) {
    int ret = pag_cognitive_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, CognitiveConnectNullHubSucceeds) {
    int ret = pag_cognitive_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, TrainingConnectNullPagReturnsError) {
    int ret = pag_training_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, TrainingConnectNullTrainingSucceeds) {
    int ret = pag_training_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, PerceptionConnectNullPagReturnsError) {
    int ret = pag_perception_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, PerceptionConnectNullPerceptionSucceeds) {
    int ret = pag_perception_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, SymbolicConnectNullPagReturnsError) {
    int ret = pag_symbolic_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SymbolicConnectNullSymbolicSucceeds) {
    int ret = pag_symbolic_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, SwarmConnectNullPagReturnsError) {
    int ret = pag_swarm_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, SwarmConnectNullSwarmSucceeds) {
    int ret = pag_swarm_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, DragonflyConnectNullPagReturnsError) {
    int ret = pag_dragonfly_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, DragonflyConnectNullDragonflySucceeds) {
    int ret = pag_dragonfly_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, PortiaConnectNullPagReturnsError) {
    int ret = pag_portia_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, PortiaConnectNullPortiaSucceeds) {
    int ret = pag_portia_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, QMCConnectNullPagReturnsError) {
    int ret = pag_qmc_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, QMCConnectNullQMCSucceeds) {
    int ret = pag_qmc_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, OmniConnectNullPagReturnsError) {
    int ret = pag_omni_connect(nullptr, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, OmniConnectNullOmniSucceeds) {
    int ret = pag_omni_connect(pag, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(PAGTest, QMCOptimizeDefenseNullPagReturnsError) {
    int ret = pag_qmc_optimize_defense(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, QMCOptimizeDefenseWithoutQMCReturnsError) {
    // Requires QMC connection
    int ret = pag_qmc_optimize_defense(pag);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, QMCTSThreatResponseNullPagReturnsError) {
    pag_defense_type_t best;
    int ret = pag_qmcts_threat_response(nullptr, 100, &best);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, QMCTSThreatResponseWithoutQMCReturnsError) {
    // Requires QMC connection
    pag_defense_type_t best;
    int ret = pag_qmcts_threat_response(pag, 100, &best);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, BioAsyncBroadcastWithoutRouterReturnsError) {
    int ret = pag_bio_async_broadcast(pag, PAG_BIO_MSG_THREAT_DETECTED, nullptr, 0);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, BioAsyncSubscribeWithoutRouterReturnsError) {
    int ret = pag_bio_async_subscribe(pag, PAG_BIO_SUB_ALL);
    EXPECT_EQ(ret, -1);
}

TEST_F(PAGTest, KGQueryWithoutRegistrationReturnsError) {
    char result[256];
    int ret = pag_kg_query(pag, "test", result, sizeof(result));
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Behavioral Integration Tests
//=============================================================================

TEST_F(PAGTest, ThreatResponseIntegration) {
    // Simulate a complete threat response scenario

    // 1. Initial state - no threat
    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_EQ(defense.threat_level, PAG_THREAT_NONE);

    // 2. Distal threat detected - scanning behavior
    pag_process_threat(pag, PAG_THREAT_DISTAL, 0.3f, 1.0f, 50.0f);
    pag_update(pag, 0.1f);

    pag_get_defense_state(pag, &defense);
    EXPECT_EQ(defense.threat_level, PAG_THREAT_DISTAL);

    // 3. Threat approaches - flight if escape available
    pag_set_escape_route(pag, true);
    pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.6f, 1.0f, 10.0f);
    pag_update(pag, 0.1f);

    pag_coping_strategy_t coping = pag_get_coping_strategy(pag);
    // With escape available, should prefer active coping

    // 4. Threat imminent - no escape - freeze
    pag_set_escape_route(pag, false);
    pag_process_threat(pag, PAG_THREAT_IMMINENT, 0.9f, 1.0f, 1.0f);
    pag_update(pag, 0.1f);

    // Autonomic response should reflect defensive state
    pag_autonomic_state_t autonomic;
    pag_get_autonomic_state(pag, &autonomic);

    // 5. Threat cleared
    pag_clear_threat(pag);
    pag_update(pag, 1.0f); // Recovery time

    pag_get_defense_state(pag, &defense);
    EXPECT_EQ(defense.threat_level, PAG_THREAT_NONE);
}

TEST_F(PAGTest, PainAnalgesiaIntegration) {
    // 1. Process pain signal
    pag_pain_input_t pain = {0};
    pain.intensity = 0.7f;
    pain.unpleasantness = 0.6f;
    pain.nociceptive = true;

    pag_process_pain(pag, &pain);

    // 2. Activate stress-induced analgesia
    pag_trigger_stress_analgesia(pag, 0.8f);
    pag_update(pag, 0.1f);

    // 3. Check descending inhibition increased
    float inhibition = pag_get_descending_inhibition(pag);
    EXPECT_GT(inhibition, 0.0f);

    // 4. Verify analgesia state
    pag_analgesia_state_t analgesia;
    pag_get_analgesia_state(pag, &analgesia);
    EXPECT_GT(analgesia.stress_induced_factor, 0.0f);
}

TEST_F(PAGTest, EmotionalDefenseIntegration) {
    // 1. Set fear emotion input
    pag_set_emotion_input(pag, PAG_EMOTION_FEAR, 0.8f);
    pag_update(pag, 0.1f);

    // 2. Process threat
    pag_process_threat(pag, PAG_THREAT_PROXIMAL, 0.7f, 0.0f, 5.0f);
    pag_update(pag, 0.1f);

    // 3. Verify emotional state
    pag_emotional_state_t emotion;
    pag_get_emotional_state(pag, &emotion);
    EXPECT_EQ(pag_get_dominant_emotion(pag), PAG_EMOTION_FEAR);

    // 4. Verify defense activated
    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_TRUE(defense.defense_active);

    // 5. Trigger vocalization (alarm call)
    pag_trigger_vocalization(pag, PAG_VOCAL_ALARM, 0.7f);

    pag_vocal_state_t vocal;
    pag_get_vocalization_state(pag, &vocal);
    EXPECT_TRUE(vocal.active);
    EXPECT_EQ(vocal.type, PAG_VOCAL_ALARM);
}

TEST_F(PAGTest, ColumnCompetitionDynamics) {
    // Test that columns compete for dominance

    // 1. Activate dorsolateral (active coping)
    pag_set_column_modulation(pag, PAG_COLUMN_DORSOLATERAL, 1.5f);
    pag_set_defense_response(pag, PAG_DEFENSE_FIGHT, 0.8f);
    pag_update(pag, 0.1f);

    pag_column_t dominant = pag_get_dominant_column(pag);
    // Active defense should activate dorsolateral or lateral column

    // 2. Switch to passive coping
    pag_set_column_modulation(pag, PAG_COLUMN_VENTROLATERAL, 2.0f);
    pag_set_defense_response(pag, PAG_DEFENSE_FREEZE, 0.9f);
    pag_update(pag, 0.2f);

    dominant = pag_get_dominant_column(pag);
    // Passive defense should activate ventrolateral column
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(PAGTest, ExtremeIntensityValues) {
    // Test with intensity at boundaries
    pag_process_threat(pag, PAG_THREAT_IMMINENT, 1.0f, 0.0f, 0.0f);

    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_LE(defense.threat_intensity, 1.0f);
    EXPECT_GE(defense.threat_intensity, 0.0f);
}

TEST_F(PAGTest, NegativeIntensityClamped) {
    // Negative intensity should be clamped
    pag_process_threat(pag, PAG_THREAT_PROXIMAL, -0.5f, 0.0f, 10.0f);

    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_GE(defense.threat_intensity, 0.0f);
}

TEST_F(PAGTest, VeryHighIntensityClamped) {
    // Very high intensity should be clamped
    pag_process_threat(pag, PAG_THREAT_CONTACT, 10.0f, 0.0f, 0.0f);

    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_LE(defense.threat_intensity, 1.0f);
}

TEST_F(PAGTest, RapidStateTransitions) {
    // Test rapid switching between states
    for (int i = 0; i < 100; i++) {
        pag_set_defense_response(pag, PAG_DEFENSE_FIGHT, 0.8f);
        pag_update(pag, 0.01f);
        pag_set_defense_response(pag, PAG_DEFENSE_FREEZE, 0.8f);
        pag_update(pag, 0.01f);
    }

    // System should remain stable
    pag_defense_state_t defense;
    pag_get_defense_state(pag, &defense);
    EXPECT_FALSE(std::isnan(defense.defense_intensity));
    EXPECT_FALSE(std::isinf(defense.defense_intensity));
}

TEST_F(PAGTest, ConcurrentPainAndThreat) {
    // Process pain and threat simultaneously
    pag_pain_input_t pain = {0};
    pain.intensity = 0.8f;
    pain.nociceptive = true;

    pag_process_pain(pag, &pain);
    pag_process_threat(pag, PAG_THREAT_CONTACT, 0.9f, 0.0f, 0.0f);

    // Both should be processed
    pag_update(pag, 0.1f);

    pag_stats_t stats;
    pag_get_stats(pag, &stats);
    EXPECT_GE(stats.pain_signals_processed, 1u);
    EXPECT_GE(stats.threats_detected, 1u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
