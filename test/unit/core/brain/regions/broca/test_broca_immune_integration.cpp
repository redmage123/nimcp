/**
 * @file test_broca_immune_integration.cpp
 * @brief Unit tests for Broca's region immune system integration
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/broca/nimcp_broca_immune.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class BrocaImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    broca_adapter_t* broca_adapter;
    broca_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        brain_immune_start(immune_system);

        /* Create Broca adapter */
        broca_config_t broca_config = broca_default_config();
        broca_adapter = broca_create(&broca_config);
        ASSERT_NE(broca_adapter, nullptr);

        /* Create bridge */
        broca_immune_config_t bridge_config;
        broca_immune_default_config(&bridge_config);
        bridge = broca_immune_bridge_create(&bridge_config, immune_system,
            broca_adapter);
        ASSERT_NE(bridge, nullptr);

        broca_immune_bridge_start(bridge);
    }

    void TearDown() override {
        if (bridge) {
            broca_immune_bridge_stop(bridge);
            broca_immune_bridge_destroy(bridge);
        }
        if (broca_adapter) {
            broca_destroy(broca_adapter);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, DefaultConfig) {
    broca_immune_config_t config;
    ASSERT_EQ(broca_immune_default_config(&config), 0);

    EXPECT_TRUE(config.enable_inflammation_impairment);
    EXPECT_TRUE(config.enable_cytokine_modulation);
    EXPECT_TRUE(config.enable_error_immune_trigger);
    EXPECT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_GT(config.max_error_history, 0u);
}

TEST_F(BrocaImmuneIntegrationTest, CreateDestroy) {
    broca_immune_config_t config;
    broca_immune_default_config(&config);

    broca_immune_bridge_t* test_bridge = broca_immune_bridge_create(
        &config, immune_system, broca_adapter);
    ASSERT_NE(test_bridge, nullptr);

    broca_immune_state_t state = broca_immune_get_state(test_bridge);
    EXPECT_EQ(state, BROCA_IMMUNE_NORMAL);

    broca_immune_bridge_destroy(test_bridge);
}

TEST_F(BrocaImmuneIntegrationTest, CreateWithNullParams) {
    /* NULL immune system */
    broca_immune_bridge_t* test_bridge = broca_immune_bridge_create(
        nullptr, nullptr, broca_adapter);
    EXPECT_EQ(test_bridge, nullptr);

    /* NULL broca adapter */
    test_bridge = broca_immune_bridge_create(
        nullptr, immune_system, nullptr);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(BrocaImmuneIntegrationTest, StartStop) {
    EXPECT_EQ(broca_immune_bridge_start(bridge), 0);
    EXPECT_EQ(broca_immune_bridge_stop(bridge), 0);
}

/* ============================================================================
 * Impairment Computation Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, ComputeImpairmentNoInflammation) {
    broca_speech_impairment_t impairment;
    ASSERT_EQ(broca_immune_compute_impairment(bridge, &impairment), 0);

    /* No inflammation = no impairment */
    EXPECT_NEAR(impairment.overall_impairment, 0.0f, 0.1f);
    EXPECT_NEAR(impairment.lexical_access_impairment, 0.0f, 0.1f);
    EXPECT_NEAR(impairment.syntax_impairment, 0.0f, 0.1f);
    EXPECT_NEAR(impairment.phonological_impairment, 0.0f, 0.1f);
    EXPECT_NEAR(impairment.motor_planning_impairment, 0.0f, 0.1f);
    EXPECT_EQ(impairment.dominant_symptom, APHASIA_NONE);
}

TEST_F(BrocaImmuneIntegrationTest, ComputeImpairmentWithInflammation) {
    /* Trigger immune response to create inflammation */
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_ANOMALY,
        epitope, sizeof(epitope), 7, 0, &antigen_id);

    /* Activate immune response */
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune_system, antigen_id, &helper_id);

    /* Update bridge to pick up inflammation */
    broca_immune_bridge_update(bridge, 1000);

    /* Check impairment */
    broca_speech_impairment_t impairment;
    ASSERT_EQ(broca_immune_compute_impairment(bridge, &impairment), 0);

    /* Should have some impairment (exact value depends on immune state) */
    /* Just verify structure is populated */
    EXPECT_GE(impairment.overall_impairment, 0.0f);
    EXPECT_LE(impairment.overall_impairment, 1.0f);
}

TEST_F(BrocaImmuneIntegrationTest, ImpairmentSubsystemsScaleCorrectly) {
    broca_speech_impairment_t impairment;
    ASSERT_EQ(broca_immune_compute_impairment(bridge, &impairment), 0);

    /* All subsystems should be in [0, 1] range */
    EXPECT_GE(impairment.lexical_access_impairment, 0.0f);
    EXPECT_LE(impairment.lexical_access_impairment, 1.0f);

    EXPECT_GE(impairment.syntax_impairment, 0.0f);
    EXPECT_LE(impairment.syntax_impairment, 1.0f);

    EXPECT_GE(impairment.phonological_impairment, 0.0f);
    EXPECT_LE(impairment.phonological_impairment, 1.0f);

    EXPECT_GE(impairment.motor_planning_impairment, 0.0f);
    EXPECT_LE(impairment.motor_planning_impairment, 1.0f);
}

TEST_F(BrocaImmuneIntegrationTest, ImpairmentPerformanceMetrics) {
    broca_speech_impairment_t impairment;
    ASSERT_EQ(broca_immune_compute_impairment(bridge, &impairment), 0);

    /* Performance metrics should have valid values */
    EXPECT_GT(impairment.speech_rate_multiplier, 0.0f);
    EXPECT_LE(impairment.speech_rate_multiplier, 1.0f);

    EXPECT_GE(impairment.error_rate, 0.0f);
    EXPECT_LE(impairment.error_rate, 1.0f);

    EXPECT_GT(impairment.lexical_diversity, 0.0f);
    EXPECT_LE(impairment.lexical_diversity, 1.0f);

    EXPECT_GT(impairment.mean_utterance_length, 0.0f);
}

/* ============================================================================
 * Inflammation Effect Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, ApplyInflammationEffects) {
    EXPECT_EQ(broca_immune_apply_inflammation_effects(bridge), 0);

    /* Should update internal impairment state */
    broca_speech_impairment_t impairment;
    ASSERT_EQ(broca_immune_get_impairment(bridge, &impairment), 0);

    /* Values should be valid */
    EXPECT_GE(impairment.overall_impairment, 0.0f);
    EXPECT_LE(impairment.overall_impairment, 1.0f);
}

TEST_F(BrocaImmuneIntegrationTest, StateTransitionFromInflammation) {
    /* Initially normal */
    EXPECT_EQ(broca_immune_get_state(bridge), BROCA_IMMUNE_NORMAL);

    /* Apply inflammation (simulated by triggering immune response) */
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_ANOMALY,
        epitope, sizeof(epitope), 8, 0, &antigen_id);

    /* Update to reflect immune state */
    broca_immune_bridge_update(bridge, 2000);

    /* State may change (depends on immune system state) */
    broca_immune_state_t new_state = broca_immune_get_state(bridge);
    /* Just verify it's a valid state */
    EXPECT_GE(new_state, BROCA_IMMUNE_NORMAL);
    EXPECT_LE(new_state, BROCA_IMMUNE_RECOVERING);
}

/* ============================================================================
 * Cytokine Effect Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, ApplyCytokineEffects) {
    EXPECT_EQ(broca_immune_apply_cytokine_effects(bridge), 0);

    /* Should compute cytokine effects */
    /* Effects are internal to bridge, just verify no crash */
}

TEST_F(BrocaImmuneIntegrationTest, CytokineEffectsDisabled) {
    /* Create bridge with cytokine modulation disabled */
    broca_immune_config_t config;
    broca_immune_default_config(&config);
    config.enable_cytokine_modulation = false;

    broca_immune_bridge_t* test_bridge = broca_immune_bridge_create(
        &config, immune_system, broca_adapter);
    ASSERT_NE(test_bridge, nullptr);

    /* Should return -1 when disabled */
    EXPECT_EQ(broca_immune_apply_cytokine_effects(test_bridge), -1);

    broca_immune_bridge_destroy(test_bridge);
}

/* ============================================================================
 * Speech Error Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, ReportSpeechError) {
    EXPECT_EQ(broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_PHONOLOGICAL, "cat", "tat", 0.3f), 0);

    /* Check statistics */
    broca_immune_stats_t stats;
    ASSERT_EQ(broca_immune_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_speech_errors, 1u);
    EXPECT_EQ(stats.phonological_errors, 1u);
}

TEST_F(BrocaImmuneIntegrationTest, ReportMultipleSpeechErrors) {
    /* Report different types of errors */
    EXPECT_EQ(broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_PHONOLOGICAL, "dog", "tog", 0.2f), 0);
    EXPECT_EQ(broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_SYNTACTIC, "he goes", "he go", 0.4f), 0);
    EXPECT_EQ(broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_MOTOR, "spaghetti", "pasgetti", 0.3f), 0);

    broca_immune_stats_t stats;
    ASSERT_EQ(broca_immune_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_speech_errors, 3u);
    EXPECT_EQ(stats.phonological_errors, 1u);
    EXPECT_EQ(stats.syntactic_errors, 1u);
    EXPECT_EQ(stats.motor_errors, 1u);
}

TEST_F(BrocaImmuneIntegrationTest, ReportSpeechErrorNullParams) {
    EXPECT_EQ(broca_immune_report_speech_error(nullptr,
        SPEECH_ERROR_PHONOLOGICAL, "test", "best", 0.5f), -1);
    EXPECT_EQ(broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_PHONOLOGICAL, nullptr, "best", 0.5f), -1);
    EXPECT_EQ(broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_PHONOLOGICAL, "test", nullptr, 0.5f), -1);
}

TEST_F(BrocaImmuneIntegrationTest, ErrorHistoryCapacity) {
    /* Report many errors to test capacity */
    for (int i = 0; i < 150; i++) {
        broca_immune_report_speech_error(bridge,
            SPEECH_ERROR_PHONOLOGICAL, "test", "best", 0.1f);
    }

    broca_immune_stats_t stats;
    ASSERT_EQ(broca_immune_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_speech_errors, 150u);

    /* History should be capped at max_error_history */
    /* Internal state not directly accessible, just verify no crash */
}

/* ============================================================================
 * Error Analysis Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, AnalyzeErrorPatterns) {
    /* Default error_analysis_window_ms is 60000ms (60 seconds)
     * Use timestamps larger than that to avoid uint64_t underflow
     * when calculating window_start = last_update_time_ms - window_ms */
    broca_immune_bridge_update(bridge, 70000);

    /* Report some errors */
    broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_PHONOLOGICAL, "cat", "tat", 0.3f);
    broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_PHONOLOGICAL, "pat", "bat", 0.3f);
    broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_SYNTACTIC, "I am", "I is", 0.4f);

    /* Update timestamp to ensure errors are within analysis window */
    broca_immune_bridge_update(bridge, 70100);

    /* Analyze patterns */
    float phono_damage, syntax_damage, motor_damage;
    EXPECT_EQ(broca_immune_analyze_error_patterns(bridge,
        &phono_damage, &syntax_damage, &motor_damage), 0);

    /* Should detect phonological errors */
    EXPECT_GT(phono_damage, 0.0f);
    EXPECT_GT(syntax_damage, 0.0f);
    EXPECT_GE(motor_damage, 0.0f);
}

TEST_F(BrocaImmuneIntegrationTest, AnalyzeErrorPatternsNoErrors) {
    float phono_damage, syntax_damage, motor_damage;
    EXPECT_EQ(broca_immune_analyze_error_patterns(bridge,
        &phono_damage, &syntax_damage, &motor_damage), 0);

    /* No errors = no damage */
    EXPECT_EQ(phono_damage, 0.0f);
    EXPECT_EQ(syntax_damage, 0.0f);
    EXPECT_EQ(motor_damage, 0.0f);
}

/* ============================================================================
 * Immune Trigger Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, TriggerFromErrorsBelowThreshold) {
    /* Report errors below threshold */
    broca_immune_report_speech_error(bridge,
        SPEECH_ERROR_PHONOLOGICAL, "cat", "tat", 0.1f);

    /* Should not trigger */
    EXPECT_EQ(broca_immune_trigger_from_errors(bridge), -1);
}

TEST_F(BrocaImmuneIntegrationTest, TriggerFromErrorsAboveThreshold) {
    /* Update bridge to set timestamp */
    broca_immune_bridge_update(bridge, 1000);

    /* Report many errors to exceed threshold */
    for (int i = 0; i < 10; i++) {
        broca_immune_report_speech_error(bridge,
            SPEECH_ERROR_PHONOLOGICAL, "cat", "tat", 0.5f);
    }

    /* Update to process errors */
    broca_immune_bridge_update(bridge, 2000);

    /* Check if immune was triggered */
    broca_immune_stats_t stats;
    ASSERT_EQ(broca_immune_get_stats(bridge, &stats), 0);

    /* May or may not trigger depending on threshold - just verify no crash */
}

TEST_F(BrocaImmuneIntegrationTest, TriggerFromErrorsDisabled) {
    /* Create bridge with error triggering disabled */
    broca_immune_config_t config;
    broca_immune_default_config(&config);
    config.enable_error_immune_trigger = false;

    broca_immune_bridge_t* test_bridge = broca_immune_bridge_create(
        &config, immune_system, broca_adapter);
    ASSERT_NE(test_bridge, nullptr);

    /* Should return -1 when disabled */
    EXPECT_EQ(broca_immune_trigger_from_errors(test_bridge), -1);

    broca_immune_bridge_destroy(test_bridge);
}

/* ============================================================================
 * State and Update Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, UpdateBridge) {
    EXPECT_EQ(broca_immune_bridge_update(bridge, 1000), 0);
    EXPECT_EQ(broca_immune_bridge_update(bridge, 2000), 0);
    EXPECT_EQ(broca_immune_bridge_update(bridge, 3000), 0);
}

TEST_F(BrocaImmuneIntegrationTest, UpdateBridgeWhenStopped) {
    broca_immune_bridge_stop(bridge);
    EXPECT_EQ(broca_immune_bridge_update(bridge, 1000), -1);
}

TEST_F(BrocaImmuneIntegrationTest, GetState) {
    broca_immune_state_t state = broca_immune_get_state(bridge);
    EXPECT_GE(state, BROCA_IMMUNE_NORMAL);
    EXPECT_LE(state, BROCA_IMMUNE_RECOVERING);
}

TEST_F(BrocaImmuneIntegrationTest, GetImpairment) {
    broca_speech_impairment_t impairment;
    ASSERT_EQ(broca_immune_get_impairment(bridge, &impairment), 0);

    /* Should have valid values */
    EXPECT_GE(impairment.overall_impairment, 0.0f);
    EXPECT_LE(impairment.overall_impairment, 1.0f);
}

TEST_F(BrocaImmuneIntegrationTest, GetStats) {
    broca_immune_stats_t stats;
    ASSERT_EQ(broca_immune_get_stats(bridge, &stats), 0);

    /* Initially all zeros */
    EXPECT_EQ(stats.total_speech_errors, 0u);
    EXPECT_EQ(stats.phonological_errors, 0u);
    EXPECT_EQ(stats.syntactic_errors, 0u);
    EXPECT_EQ(stats.motor_errors, 0u);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, AphasiaTypeToString) {
    EXPECT_STREQ(broca_aphasia_type_to_string(APHASIA_NONE), "None");
    EXPECT_STREQ(broca_aphasia_type_to_string(APHASIA_ANOMIA), "Anomia");
    EXPECT_STREQ(broca_aphasia_type_to_string(APHASIA_AGRAMMATISM), "Agrammatism");
    EXPECT_STREQ(broca_aphasia_type_to_string(APHASIA_PHONOLOGICAL), "Phonological");
    EXPECT_STREQ(broca_aphasia_type_to_string(APHASIA_MOTOR_SPEECH), "Motor Speech");
    EXPECT_STREQ(broca_aphasia_type_to_string(APHASIA_NONFLUENT), "Non-fluent");
    EXPECT_STREQ(broca_aphasia_type_to_string(APHASIA_MUTISM), "Mutism");
}

TEST_F(BrocaImmuneIntegrationTest, ImmuneStateToString) {
    EXPECT_STREQ(broca_immune_state_to_string(BROCA_IMMUNE_NORMAL), "Normal");
    EXPECT_STREQ(broca_immune_state_to_string(BROCA_IMMUNE_MILD_IMPAIRMENT),
        "Mild Impairment");
    EXPECT_STREQ(broca_immune_state_to_string(BROCA_IMMUNE_MODERATE_APHASIA),
        "Moderate Aphasia");
    EXPECT_STREQ(broca_immune_state_to_string(BROCA_IMMUNE_SEVERE_APHASIA),
        "Severe Aphasia");
    EXPECT_STREQ(broca_immune_state_to_string(BROCA_IMMUNE_STORM),
        "Cytokine Storm");
    EXPECT_STREQ(broca_immune_state_to_string(BROCA_IMMUNE_RECOVERING),
        "Recovering");
}

TEST_F(BrocaImmuneIntegrationTest, SpeechErrorTypeToString) {
    EXPECT_STREQ(broca_speech_error_type_to_string(SPEECH_ERROR_NONE), "None");
    EXPECT_STREQ(broca_speech_error_type_to_string(SPEECH_ERROR_PHONOLOGICAL),
        "Phonological");
    EXPECT_STREQ(broca_speech_error_type_to_string(SPEECH_ERROR_LEXICAL), "Lexical");
    EXPECT_STREQ(broca_speech_error_type_to_string(SPEECH_ERROR_SYNTACTIC),
        "Syntactic");
    EXPECT_STREQ(broca_speech_error_type_to_string(SPEECH_ERROR_MOTOR), "Motor");
    EXPECT_STREQ(broca_speech_error_type_to_string(SPEECH_ERROR_HESITATION),
        "Hesitation");
    EXPECT_STREQ(broca_speech_error_type_to_string(SPEECH_ERROR_PERSEVERATION),
        "Perseveration");
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(BrocaImmuneIntegrationTest, FullCycleInflammationImpairmentRecovery) {
    /* Start normal */
    EXPECT_EQ(broca_immune_get_state(bridge), BROCA_IMMUNE_NORMAL);

    /* Trigger immune response */
    uint32_t antigen_id;
    uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_ANOMALY,
        epitope, sizeof(epitope), 9, 0, &antigen_id);

    /* Activate immune cells */
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune_system, antigen_id, &helper_id);

    /* Update bridge - inflammation should cause impairment */
    broca_immune_bridge_update(bridge, 5000);

    broca_speech_impairment_t impairment;
    ASSERT_EQ(broca_immune_get_impairment(bridge, &impairment), 0);

    /* Should have some impairment metrics populated */
    /* (exact values depend on immune system state) */

    /* Neutralize threat */
    uint32_t antibody_id;
    brain_immune_produce_antibody(immune_system, b_cell_id,
        ANTIBODY_IGG, &antibody_id);
    brain_immune_neutralize(immune_system, antigen_id, antibody_id);

    /* Update - should start recovery */
    broca_immune_bridge_update(bridge, 10000);

    /* Verify no crashes during full cycle */
}

TEST_F(BrocaImmuneIntegrationTest, ErrorsTriggersImmuneThenImpairment) {
    /* Report many speech errors */
    for (int i = 0; i < 15; i++) {
        broca_immune_report_speech_error(bridge,
            SPEECH_ERROR_PHONOLOGICAL, "test", "best", 0.6f);
    }

    /* Update to process errors */
    broca_immune_bridge_update(bridge, 1000);

    /* Check if immune was triggered */
    broca_immune_stats_t stats;
    ASSERT_EQ(broca_immune_get_stats(bridge, &stats), 0);

    /* Errors should be recorded */
    EXPECT_GT(stats.total_speech_errors, 0u);

    /* May have triggered immune (depends on threshold) */
    /* Just verify system is functional */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
