/**
 * @file test_oscillations_immune_full_integration.cpp
 * @brief Comprehensive tests for brain oscillations-immune integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Full integration tests for bidirectional oscillations-immune coupling
 * WHY:  Ensure cytokines modulate oscillations correctly, abnormal oscillations trigger immune
 * HOW:  Unit tests covering lifecycle, immune→osc, osc→immune, and bidirectional updates
 */

#include <gtest/gtest.h>
#include "core/brain_oscillations/nimcp_oscillations_immune_bridge.h"
#include "core/brain_oscillations/nimcp_brain_oscillations.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_brain.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class OscillationsImmuneFullIntegrationTest : public ::testing::Test {
protected:
    /* Shared brain and oscillation analyzer (expensive to create) */
    static brain_t shared_brain;
    static brain_oscillation_analyzer_t* shared_osc_analyzer;

    /* Per-test immune system and bridge */
    brain_immune_system_t* immune_system;
    oscillations_immune_bridge_t* bridge;

    /* Convenience aliases for shared resources */
    brain_t brain;
    brain_oscillation_analyzer_t* osc_analyzer;

    static void SetUpTestSuite() {
        shared_brain = brain_create("oscillations_immune_full_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 100, 10);
        ASSERT_NE(shared_brain, (brain_t)0);
        shared_osc_analyzer = brain_oscillation_create(shared_brain, 500, 250);
        ASSERT_NE(shared_osc_analyzer, nullptr);
    }

    static void TearDownTestSuite() {
        if (shared_osc_analyzer) brain_oscillation_destroy(shared_osc_analyzer);
        if (shared_brain) brain_destroy(shared_brain);
        shared_osc_analyzer = nullptr;
        shared_brain = (brain_t)0;
    }

    void SetUp() override {
        brain = shared_brain;
        osc_analyzer = shared_osc_analyzer;

        /* Create immune system (per-test) */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);

        /* Create bridge (per-test) */
        oscillations_immune_config_t bridge_cfg;
        oscillations_immune_default_config(&bridge_cfg);
        bridge = oscillations_immune_bridge_create(&bridge_cfg, osc_analyzer, immune_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) oscillations_immune_bridge_destroy(bridge);
        if (immune_system) brain_immune_destroy(immune_system);
    }

    /* Helper: Simulate IL-1β cytokine release */
    /* Note: target_region=1 (not 0/broadcast) so cytokine stays undelivered
       and is visible to get_cytokine_concentration() in the bridge */
    void releaseIL1(float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system,
            BRAIN_CYTOKINE_IL1,
            1, /* source_cell */
            concentration,
            1, /* target_region (non-zero to avoid immediate delivery) */
            &cytokine_id
        );
    }

    /* Helper: Simulate TNF-α cytokine release */
    void releaseTNF(float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system,
            BRAIN_CYTOKINE_TNF,
            1,
            concentration,
            1,
            &cytokine_id
        );
    }

    /* Helper: Simulate IL-10 cytokine release */
    void releaseIL10(float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system,
            BRAIN_CYTOKINE_IL10,
            1,
            concentration,
            1,
            &cytokine_id
        );
    }

    /* Helper: Create inflammation site */
    void createInflammation(brain_inflammation_level_t level) {
        uint32_t antigen_id;
        brain_immune_present_antigen(
            immune_system,
            ANTIGEN_SOURCE_MANUAL,
            (const uint8_t*)"test_threat",
            11,
            8, /* severity */
            0,
            &antigen_id
        );

        uint32_t site_id;
        brain_immune_initiate_inflammation(
            immune_system,
            0, /* region */
            antigen_id,
            &site_id
        );

        /* Escalate to desired level (initiate already creates at LOCAL=1) */
        for (int i = 1; i < (int)level; i++) {
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
    }

    /* Helper: Record activity and build oscillation buffer */
    void buildOscillationBuffer(int num_samples) {
        for (int i = 0; i < num_samples; i++) {
            brain_oscillation_record_activity(osc_analyzer);
        }
    }
};

/* Static member definitions */
brain_t OscillationsImmuneFullIntegrationTest::shared_brain = (brain_t)0;
brain_oscillation_analyzer_t* OscillationsImmuneFullIntegrationTest::shared_osc_analyzer = nullptr;

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(OscillationsImmuneFullIntegrationTest, DefaultConfigValid) {
    oscillations_immune_config_t config;
    int result = oscillations_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_oscillation_modulation);
    EXPECT_TRUE(config.enable_inflammation_power_shift);
    EXPECT_TRUE(config.enable_oscillation_immune_trigger);
    EXPECT_TRUE(config.enable_abnormality_surveillance);
    EXPECT_TRUE(config.enable_il10_restoration);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_EQ(config.abnormality_sensitivity, 1.0f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, CreateBridgeSuccess) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(OscillationsImmuneFullIntegrationTest, CreateBridgeNullPointers) {
    oscillations_immune_bridge_t* null_bridge = oscillations_immune_bridge_create(
        nullptr, nullptr, nullptr);

    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(OscillationsImmuneFullIntegrationTest, DestroyBridgeNullSafe) {
    oscillations_immune_bridge_destroy(nullptr);
    /* Should not crash */
    SUCCEED();
}

TEST_F(OscillationsImmuneFullIntegrationTest, EstablishBaseline) {
    /* Build oscillation buffer first */
    buildOscillationBuffer(250);

    int result = oscillations_immune_establish_baseline(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Immune → Oscillations Tests (Cytokine Effects)
 * ============================================================================ */

TEST_F(OscillationsImmuneFullIntegrationTest, IL1IncreasesDeltalDecreasesGamma) {
    /* Release IL-1β */
    releaseIL1(0.8f);

    /* Apply cytokine effects */
    int result = oscillations_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Check effects */
    cytokine_oscillation_effects_t effects;
    oscillations_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.il1_delta_amplification, 1.0f);
    EXPECT_LT(effects.il1_gamma_suppression, 1.0f);
    EXPECT_GT(effects.total_delta_amplification, 1.0f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, TNFStrongGammaSuppression) {
    /* TNF-α has strongest gamma suppression */
    releaseTNF(0.9f);

    oscillations_immune_apply_cytokine_effects(bridge);

    cytokine_oscillation_effects_t effects;
    oscillations_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_LT(effects.tnf_gamma_suppression, 0.6f); /* Strong suppression */
    EXPECT_GT(effects.tnf_delta_amplification, 1.5f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, IL10RestorationEffect) {
    /* IL-10 should have restoration effect */
    releaseIL10(0.7f);

    oscillations_immune_apply_cytokine_effects(bridge);

    cytokine_oscillation_effects_t effects;
    oscillations_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.il10_restoration, 0.0f);
    EXPECT_LT(effects.il10_restoration, 1.0f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, MultiCytokineAggregation) {
    /* Release multiple pro-inflammatory cytokines */
    releaseIL1(0.5f);
    releaseTNF(0.6f);

    oscillations_immune_apply_cytokine_effects(bridge);

    cytokine_oscillation_effects_t effects;
    oscillations_immune_get_cytokine_effects(bridge, &effects);

    /* Total effects should be maximum (not additive) */
    EXPECT_GT(effects.total_delta_amplification, 1.4f);
    EXPECT_LE(effects.total_gamma_suppression, 0.7f);

    /* Network disruption should scale with cytokine burden */
    EXPECT_GT(effects.coherence_disruption, 0.0f);
    EXPECT_GT(effects.synchrony_disruption, 0.0f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, CytokineSensitivityTuning) {
    /* Create bridge with high sensitivity */
    oscillations_immune_config_t config;
    oscillations_immune_default_config(&config);
    config.cytokine_sensitivity = 2.0f;

    oscillations_immune_bridge_t* sensitive_bridge =
        oscillations_immune_bridge_create(&config, osc_analyzer, immune_system);
    ASSERT_NE(sensitive_bridge, nullptr);

    /* Release moderate IL-1β */
    releaseIL1(0.4f);

    oscillations_immune_apply_cytokine_effects(sensitive_bridge);

    cytokine_oscillation_effects_t effects;
    oscillations_immune_get_cytokine_effects(sensitive_bridge, &effects);

    /* Effects should be amplified by sensitivity */
    EXPECT_GT(effects.il1_delta_amplification, 1.3f);

    oscillations_immune_bridge_destroy(sensitive_bridge);
}

/* ============================================================================
 * Immune → Oscillations Tests (Inflammation Effects)
 * ============================================================================ */

TEST_F(OscillationsImmuneFullIntegrationTest, LocalInflammationMildEffects) {
    createInflammation(INFLAMMATION_LOCAL);

    int result = oscillations_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_oscillation_state_t state;
    oscillations_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_GT(state.delta_power_shift, 1.0f);
    EXPECT_LT(state.delta_power_shift, 1.3f); /* Mild increase */
    EXPECT_LT(state.gamma_power_shift, 1.0f);
    EXPECT_EQ(state.expected_state, COGNITIVE_STATE_RELAXED);
}

TEST_F(OscillationsImmuneFullIntegrationTest, SystemicInflammationStrongEffects) {
    createInflammation(INFLAMMATION_SYSTEMIC);

    oscillations_immune_apply_inflammation_effects(bridge);

    inflammation_oscillation_state_t state;
    oscillations_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.delta_power_shift, 1.8f); /* Strong increase */
    EXPECT_LT(state.gamma_power_shift, 0.6f); /* Strong suppression */
    EXPECT_GT(state.coherence_reduction, 0.4f);
    EXPECT_EQ(state.expected_state, COGNITIVE_STATE_DEEP_SLEEP);
}

TEST_F(OscillationsImmuneFullIntegrationTest, CytokineStormMaximalEffects) {
    createInflammation(INFLAMMATION_STORM);

    oscillations_immune_apply_inflammation_effects(bridge);

    inflammation_oscillation_state_t state;
    oscillations_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_STORM);
    EXPECT_GE(state.delta_power_shift, 2.5f); /* Maximal delta */
    EXPECT_LE(state.gamma_power_shift, 0.4f); /* Minimal gamma */
    EXPECT_GT(state.coherence_reduction, 0.6f);
    EXPECT_GT(state.synchrony_reduction, 0.5f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, InflammationStateShift) {
    createInflammation(INFLAMMATION_REGIONAL);

    oscillations_immune_apply_inflammation_effects(bridge);

    cognitive_state_t expected_state = oscillations_immune_compute_state_shift(bridge);

    EXPECT_EQ(expected_state, COGNITIVE_STATE_LIGHT_SLEEP);
}

/* ============================================================================
 * Oscillations → Immune Tests (Abnormality Detection)
 * ============================================================================ */

TEST_F(OscillationsImmuneFullIntegrationTest, DetectExcessiveDelta) {
    /* Build oscillation buffer */
    buildOscillationBuffer(250);

    /* Manually create excessive delta scenario by applying strong inflammation */
    createInflammation(INFLAMMATION_SYSTEMIC);
    oscillations_immune_apply_inflammation_effects(bridge);

    /* Detect abnormality */
    bool abnormal = oscillations_immune_detect_abnormality(bridge);

    oscillation_immune_trigger_t trigger;
    oscillations_immune_get_trigger_state(bridge, &trigger);

    /* Should detect excessive delta */
    EXPECT_TRUE(trigger.excessive_delta || trigger.suppressed_gamma);
}

TEST_F(OscillationsImmuneFullIntegrationTest, AbnormalityPersistenceTracking) {
    buildOscillationBuffer(250);

    /* Create condition for abnormality */
    createInflammation(INFLAMMATION_SYSTEMIC);
    oscillations_immune_apply_inflammation_effects(bridge);

    /* Detect abnormality multiple times */
    for (int i = 0; i < 5; i++) {
        oscillations_immune_detect_abnormality(bridge);
    }

    oscillation_immune_trigger_t trigger;
    oscillations_immune_get_trigger_state(bridge, &trigger);

    EXPECT_GT(trigger.consecutive_abnormal, 0);
}

TEST_F(OscillationsImmuneFullIntegrationTest, AbnormalityScoreWeighting) {
    buildOscillationBuffer(250);

    /* Create strong abnormality */
    createInflammation(INFLAMMATION_STORM);
    oscillations_immune_apply_inflammation_effects(bridge);

    oscillations_immune_detect_abnormality(bridge);

    float score = oscillations_immune_compute_abnormality_score(bridge);

    /* Score should be > 0 with abnormal oscillations */
    EXPECT_GT(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, TriggerImmuneFromPersistentAbnormality) {
    buildOscillationBuffer(250);

    /* Create abnormal condition */
    createInflammation(INFLAMMATION_SYSTEMIC);
    oscillations_immune_apply_inflammation_effects(bridge);

    /* Detect abnormality until persistence threshold */
    for (int i = 0; i < ABNORMALITY_PERSISTENCE_THRESHOLD + 1; i++) {
        oscillations_immune_detect_abnormality(bridge);
    }

    /* Trigger immune response */
    int result = oscillations_immune_trigger_from_abnormality(bridge);
    EXPECT_EQ(result, 0);

    oscillation_immune_trigger_t trigger;
    oscillations_immune_get_trigger_state(bridge, &trigger);

    EXPECT_TRUE(trigger.immune_surveillance_triggered);
    EXPECT_TRUE(trigger.antigen_presented);
    EXPECT_GT(trigger.antigen_id, 0);
}

TEST_F(OscillationsImmuneFullIntegrationTest, AbnormalitySeverityMapping) {
    buildOscillationBuffer(250);

    /* Create severe abnormality */
    createInflammation(INFLAMMATION_STORM);
    oscillations_immune_apply_inflammation_effects(bridge);

    for (int i = 0; i < ABNORMALITY_PERSISTENCE_THRESHOLD + 1; i++) {
        oscillations_immune_detect_abnormality(bridge);
    }

    oscillations_immune_trigger_from_abnormality(bridge);

    oscillation_immune_trigger_t trigger;
    oscillations_immune_get_trigger_state(bridge, &trigger);

    /* High abnormality score should map to high immune severity */
    EXPECT_GT(trigger.immune_severity, 5);
    EXPECT_LE(trigger.immune_severity, 10);
}

/* ============================================================================
 * Bidirectional Integration Tests
 * ============================================================================ */

TEST_F(OscillationsImmuneFullIntegrationTest, BidirectionalUpdate) {
    buildOscillationBuffer(250);

    /* Release cytokines */
    releaseIL1(0.6f);

    /* Create inflammation */
    createInflammation(INFLAMMATION_REGIONAL);

    /* Update bridge (both directions) */
    int result = oscillations_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    /* Check that both directions were processed */
    cytokine_oscillation_effects_t effects;
    oscillations_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_GT(effects.total_delta_amplification, 1.0f);

    inflammation_oscillation_state_t state;
    oscillations_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(state.current_level, INFLAMMATION_REGIONAL);
}

TEST_F(OscillationsImmuneFullIntegrationTest, IL10RestoresOscillations) {
    buildOscillationBuffer(250);

    /* Establish baseline */
    oscillations_immune_establish_baseline(bridge);

    /* Create inflammation */
    createInflammation(INFLAMMATION_REGIONAL);
    oscillations_immune_apply_inflammation_effects(bridge);

    /* Release IL-10 for restoration */
    releaseIL10(0.8f);

    int result = oscillations_immune_restore_with_il10(bridge, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(OscillationsImmuneFullIntegrationTest, IsModulatedDetection) {
    /* Initially not modulated */
    EXPECT_FALSE(oscillations_immune_is_modulated(bridge));

    /* Release cytokines */
    releaseIL1(0.7f);
    oscillations_immune_apply_cytokine_effects(bridge);

    /* Now should be modulated */
    EXPECT_TRUE(oscillations_immune_is_modulated(bridge));
}

TEST_F(OscillationsImmuneFullIntegrationTest, DeltaAmplificationQuery) {
    releaseIL1(0.5f);
    releaseTNF(0.6f);

    oscillations_immune_apply_cytokine_effects(bridge);

    float delta_amp = oscillations_immune_get_delta_amplification(bridge);
    EXPECT_GT(delta_amp, 1.0f);
    EXPECT_LE(delta_amp, 3.0f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, GammaSuppressionQuery) {
    releaseTNF(0.9f);

    oscillations_immune_apply_cytokine_effects(bridge);

    float gamma_supp = oscillations_immune_get_gamma_suppression(bridge);
    EXPECT_GE(gamma_supp, 0.3f);
    EXPECT_LT(gamma_supp, 1.0f);
}

/* ============================================================================
 * Edge Cases and Robustness Tests
 * ============================================================================ */

TEST_F(OscillationsImmuneFullIntegrationTest, NullPointerGuards) {
    EXPECT_EQ(oscillations_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(oscillations_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_FALSE(oscillations_immune_detect_abnormality(nullptr));
    EXPECT_EQ(oscillations_immune_trigger_from_abnormality(nullptr), -1);
    EXPECT_EQ(oscillations_immune_bridge_update(nullptr, 100), -1);
}

TEST_F(OscillationsImmuneFullIntegrationTest, DisabledFeaturesRespected) {
    /* Create bridge with all features disabled */
    oscillations_immune_config_t config;
    oscillations_immune_default_config(&config);
    config.enable_cytokine_oscillation_modulation = false;
    config.enable_inflammation_power_shift = false;
    config.enable_oscillation_immune_trigger = false;

    oscillations_immune_bridge_t* disabled_bridge =
        oscillations_immune_bridge_create(&config, osc_analyzer, immune_system);
    ASSERT_NE(disabled_bridge, nullptr);

    /* Release cytokines */
    releaseIL1(0.8f);

    /* Apply effects - should return 0 but not modify */
    EXPECT_EQ(oscillations_immune_apply_cytokine_effects(disabled_bridge), 0);

    cytokine_oscillation_effects_t effects;
    oscillations_immune_get_cytokine_effects(disabled_bridge, &effects);

    /* Effects should be minimal/zero */
    EXPECT_EQ(effects.total_delta_amplification, 0.0f);

    oscillations_immune_bridge_destroy(disabled_bridge);
}

TEST_F(OscillationsImmuneFullIntegrationTest, NoBaselineRestoration) {
    /* Try to restore without baseline */
    int result = oscillations_immune_restore_with_il10(bridge, 0.5f);
    EXPECT_EQ(result, -1); /* Should fail without baseline */
}

TEST_F(OscillationsImmuneFullIntegrationTest, ZeroCytokineConcentrations) {
    /* Apply effects with no cytokines */
    oscillations_immune_apply_cytokine_effects(bridge);

    cytokine_oscillation_effects_t effects;
    oscillations_immune_get_cytokine_effects(bridge, &effects);

    /* Should have neutral effects */
    EXPECT_LE(effects.total_delta_amplification, 1.1f);
    EXPECT_GE(effects.total_gamma_suppression, 0.9f);
}

TEST_F(OscillationsImmuneFullIntegrationTest, PreventDuplicateAntigenPresentation) {
    buildOscillationBuffer(250);

    /* Create abnormality */
    createInflammation(INFLAMMATION_SYSTEMIC);
    oscillations_immune_apply_inflammation_effects(bridge);

    /* Detect and trigger */
    for (int i = 0; i < ABNORMALITY_PERSISTENCE_THRESHOLD + 1; i++) {
        oscillations_immune_detect_abnormality(bridge);
    }

    oscillations_immune_trigger_from_abnormality(bridge);

    oscillation_immune_trigger_t trigger1;
    oscillations_immune_get_trigger_state(bridge, &trigger1);
    uint32_t first_antigen = trigger1.antigen_id;

    /* Try to trigger again */
    oscillations_immune_trigger_from_abnormality(bridge);

    oscillation_immune_trigger_t trigger2;
    oscillations_immune_get_trigger_state(bridge, &trigger2);

    /* Should not create duplicate antigen */
    EXPECT_EQ(trigger2.antigen_id, first_antigen);
}

/* ============================================================================
 * Statistical and Monitoring Tests
 * ============================================================================ */

TEST_F(OscillationsImmuneFullIntegrationTest, StatisticsTracking) {
    /* Perform various operations */
    releaseIL1(0.5f);
    oscillations_immune_apply_cytokine_effects(bridge);

    createInflammation(INFLAMMATION_LOCAL);
    oscillations_immune_apply_inflammation_effects(bridge);

    buildOscillationBuffer(250);
    for (int i = 0; i < ABNORMALITY_PERSISTENCE_THRESHOLD + 1; i++) {
        oscillations_immune_detect_abnormality(bridge);
    }
    oscillations_immune_trigger_from_abnormality(bridge);

    /* Update bridge */
    oscillations_immune_bridge_update(bridge, 100);

    /* Check statistics via internal state (would need getter) */
    EXPECT_GT(bridge->total_updates, 0);
    EXPECT_GT(bridge->cytokine_modulations, 0);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
