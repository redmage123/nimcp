/**
 * @file test_introspection_immune_full_integration.cpp
 * @brief Comprehensive tests for Introspection-Immune Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Full integration tests for introspection-immune bridge
 * WHY:  Validate bidirectional coupling and biological accuracy
 * HOW:  Test all pathways: immune→introspection, introspection→immune
 */

#include <gtest/gtest.h>
#include "cognitive/immune/nimcp_introspection_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class IntrospectionImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    introspection_context_t introspection_ctx;
    introspection_immune_bridge_t* bridge;
    brain_t test_brain;

    void SetUp() override {
        /* Create brain for introspection context */
        test_brain = brain_create("test_brain", 128, 64);
        ASSERT_NE(test_brain, nullptr);

        /* Create introspection context */
        introspection_config_t intro_config = introspection_default_config();
        introspection_ctx = introspection_context_create(test_brain, &intro_config);
        ASSERT_NE(introspection_ctx, nullptr);

        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Create bridge (will be created in individual tests) */
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            introspection_immune_bridge_destroy(bridge);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
        if (introspection_ctx) {
            introspection_context_destroy(introspection_ctx);
        }
        if (test_brain) {
            brain_destroy(test_brain);
        }
    }

    /* Helper: Create bridge with default config */
    void CreateBridge() {
        introspection_immune_config_t config;
        introspection_immune_default_config(&config);
        bridge = introspection_immune_bridge_create(&config, immune_system, introspection_ctx);
        ASSERT_NE(bridge, nullptr);
    }

    /* Helper: Create bridge with custom config */
    void CreateBridgeWithConfig(const introspection_immune_config_t* config) {
        bridge = introspection_immune_bridge_create(config, immune_system, introspection_ctx);
        ASSERT_NE(bridge, nullptr);
    }

    /* Helper: Simulate cytokine release */
    void ReleaseCytokine(brain_cytokine_type_t type, float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(immune_system, type, 0, concentration, 0, &cytokine_id);
    }

    /* Helper: Simulate inflammation */
    void CreateInflammation(brain_inflammation_level_t level) {
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, 1, 0, &site_id);

        /* Escalate to desired level */
        for (int i = INFLAMMATION_LOCAL; i < level; i++) {
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, DefaultConfigInitialization) {
    introspection_immune_config_t config;
    int result = introspection_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_introspection_modulation);
    EXPECT_TRUE(config.enable_inflammation_phi_reduction);
    EXPECT_TRUE(config.enable_sickness_detection);
    EXPECT_TRUE(config.enable_pattern_immune_correlation);
    EXPECT_TRUE(config.enable_uncertainty_immune_coupling);

    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.sickness_detection_sensitivity, 1.0f);

    EXPECT_FLOAT_EQ(config.phi_sickness_threshold, PHI_SICKNESS_DETECTION_THRESHOLD);
    EXPECT_FLOAT_EQ(config.uncertainty_sickness_threshold, UNCERTAINTY_SICKNESS_THRESHOLD);
}

TEST_F(IntrospectionImmuneIntegrationTest, BridgeCreationSuccess) {
    CreateBridge();
    EXPECT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->enable_cytokine_introspection_modulation);
    EXPECT_TRUE(bridge->enable_inflammation_phi_reduction);
    EXPECT_EQ(bridge->total_updates, 0);
}

TEST_F(IntrospectionImmuneIntegrationTest, BridgeCreationNullInputs) {
    bridge = introspection_immune_bridge_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(IntrospectionImmuneIntegrationTest, BridgeDestruction) {
    CreateBridge();
    introspection_immune_bridge_destroy(bridge);
    bridge = nullptr; /* Prevent double-free in TearDown */
    /* No crash = success */
}

TEST_F(IntrospectionImmuneIntegrationTest, BridgeDestructionNullSafe) {
    introspection_immune_bridge_destroy(nullptr);
    /* No crash = success */
}

/* ============================================================================
 * Immune → Introspection: Cytokine Effects Tests
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, CytokineEffectsIL1Impairment) {
    CreateBridge();

    /* Release IL-1β */
    ReleaseCytokine(BRAIN_CYTOKINE_IL1, 0.8f);

    /* Apply cytokine effects */
    int result = introspection_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Check effects */
    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);

    /* IL-1β should impair metacognitive accuracy */
    EXPECT_LT(effects.il1_accuracy_impairment, 0.0f); /* Negative impact */
    EXPECT_GT(effects.consciousness_impairment_level, 0.0f);
    EXPECT_GT(effects.phi_reduction, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, CytokineEffectsIL6StrongImpairment) {
    CreateBridge();

    /* Release IL-6 (strongest metacognitive impairment) */
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 1.0f);

    introspection_immune_apply_cytokine_effects(bridge);

    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);

    /* IL-6 has strongest impact on prefrontal metacognitive circuits */
    EXPECT_LT(effects.il6_accuracy_impairment, effects.il1_accuracy_impairment);
    EXPECT_GT(effects.consciousness_impairment_level, 0.3f);
    EXPECT_GT(effects.phi_reduction, 0.2f);
}

TEST_F(IntrospectionImmuneIntegrationTest, CytokineEffectsTNFImpairment) {
    CreateBridge();

    /* Release TNF-α */
    ReleaseCytokine(BRAIN_CYTOKINE_TNF, 0.7f);

    introspection_immune_apply_cytokine_effects(bridge);

    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_LT(effects.tnf_accuracy_impairment, 0.0f);
    EXPECT_GT(effects.consciousness_impairment_level, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, CytokineEffectsIL10Restoration) {
    CreateBridge();

    /* Release IL-10 (anti-inflammatory) */
    ReleaseCytokine(BRAIN_CYTOKINE_IL10, 0.8f);

    introspection_immune_apply_cytokine_effects(bridge);

    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);

    /* IL-10 should restore clarity */
    EXPECT_GT(effects.il10_clarity_restoration, 0.0f); /* Positive impact */
}

TEST_F(IntrospectionImmuneIntegrationTest, CytokineEffectsCombinedProInflammatory) {
    CreateBridge();

    /* Release multiple pro-inflammatory cytokines */
    ReleaseCytokine(BRAIN_CYTOKINE_IL1, 0.6f);
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.7f);
    ReleaseCytokine(BRAIN_CYTOKINE_TNF, 0.5f);

    introspection_immune_apply_cytokine_effects(bridge);

    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);

    /* Combined effect should be significant */
    EXPECT_GT(effects.consciousness_impairment_level, 0.5f);
    EXPECT_GT(effects.phi_reduction, 0.3f);
    EXPECT_GT(effects.uncertainty_increase, 0.4f);
}

TEST_F(IntrospectionImmuneIntegrationTest, CytokineEffectsBalancedProAntiInflammatory) {
    CreateBridge();

    /* Release both pro and anti-inflammatory */
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.5f);
    ReleaseCytokine(BRAIN_CYTOKINE_IL10, 0.6f);

    introspection_immune_apply_cytokine_effects(bridge);

    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);

    /* IL-10 should partially offset IL-6 */
    EXPECT_LT(effects.consciousness_impairment_level, 0.5f);
}

TEST_F(IntrospectionImmuneIntegrationTest, CytokineEffectsDisabled) {
    /* Create bridge with cytokine modulation disabled */
    introspection_immune_config_t config;
    introspection_immune_default_config(&config);
    config.enable_cytokine_introspection_modulation = false;
    CreateBridgeWithConfig(&config);

    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 1.0f);

    int result = introspection_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0); /* Should return success but do nothing */

    /* Stats should not increase */
    EXPECT_EQ(bridge->cytokine_modulations, 0);
}

/* ============================================================================
 * Immune → Introspection: Inflammation Effects Tests
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, InflammationEffectsLocalLevel) {
    CreateBridge();

    /* Create local inflammation */
    CreateInflammation(INFLAMMATION_LOCAL);

    introspection_immune_apply_inflammation_effects(bridge);

    inflammation_consciousness_state_t state;
    introspection_immune_get_consciousness_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_GT(state.phi_reduction, 0.0f);
    EXPECT_LT(state.phi_reduction, 0.3f); /* Mild reduction for local */
}

TEST_F(IntrospectionImmuneIntegrationTest, InflammationEffectsSystemicLevel) {
    CreateBridge();

    /* Create systemic inflammation */
    CreateInflammation(INFLAMMATION_SYSTEMIC);

    introspection_immune_apply_inflammation_effects(bridge);

    inflammation_consciousness_state_t state;
    introspection_immune_get_consciousness_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.phi_reduction, 0.4f); /* Strong reduction for systemic */
    EXPECT_GT(state.metacognitive_accuracy_loss, 0.3f);
    EXPECT_GT(state.pattern_detection_impairment, 0.2f);
}

TEST_F(IntrospectionImmuneIntegrationTest, InflammationEffectsCytokineStorm) {
    CreateBridge();

    /* Create cytokine storm (maximum inflammation) */
    CreateInflammation(INFLAMMATION_STORM);

    introspection_immune_apply_inflammation_effects(bridge);

    inflammation_consciousness_state_t state;
    introspection_immune_get_consciousness_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_STORM);
    /* Storm should approach maximum Phi reduction */
    EXPECT_GT(state.phi_reduction, 0.6f);
    EXPECT_NEAR(state.phi_reduction, INFLAMMATION_PHI_REDUCTION_MAX, 0.1f);

    /* Severe metacognitive impairment */
    EXPECT_GT(state.metacognitive_accuracy_loss, 0.5f);
    EXPECT_GT(state.state_clarity_loss, 0.6f);
    EXPECT_GT(state.epistemic_uncertainty_increase, 0.7f);
}

TEST_F(IntrospectionImmuneIntegrationTest, InflammationEffectsPhiReductionComputation) {
    CreateBridge();

    CreateInflammation(INFLAMMATION_REGIONAL);
    introspection_immune_apply_inflammation_effects(bridge);

    float phi_reduction = introspection_immune_compute_phi_reduction(bridge);

    EXPECT_GT(phi_reduction, 0.0f);
    EXPECT_LE(phi_reduction, INFLAMMATION_PHI_REDUCTION_MAX);
}

TEST_F(IntrospectionImmuneIntegrationTest, InflammationEffectsUncertaintyIncrease) {
    CreateBridge();

    CreateInflammation(INFLAMMATION_SYSTEMIC);
    introspection_immune_apply_inflammation_effects(bridge);

    float uncertainty_increase = introspection_immune_compute_uncertainty_increase(bridge);

    EXPECT_GT(uncertainty_increase, 0.5f);
    EXPECT_LE(uncertainty_increase, 1.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, InflammationEffectsNoInflammation) {
    CreateBridge();

    /* No inflammation created */
    introspection_immune_apply_inflammation_effects(bridge);

    inflammation_consciousness_state_t state;
    introspection_immune_get_consciousness_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(state.phi_reduction, 0.0f);
    EXPECT_FLOAT_EQ(state.metacognitive_accuracy_loss, 0.0f);
}

/* ============================================================================
 * Introspection → Immune: Sickness Detection Tests
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, SicknessDetectionBaseline) {
    CreateBridge();

    /* Set baseline metrics */
    int result = introspection_immune_set_baseline(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_GT(bridge->baseline_phi, 0.0f);
    EXPECT_GT(bridge->baseline_uncertainty, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, SicknessDetectionPhiDrop) {
    CreateBridge();

    /* Set baseline */
    introspection_immune_set_baseline(bridge);

    /* Create inflammation to cause Phi drop */
    CreateInflammation(INFLAMMATION_SYSTEMIC);
    introspection_immune_apply_inflammation_effects(bridge);

    /* Detect sickness */
    introspection_immune_detect_sickness(bridge);

    /* Should detect sickness due to Phi drop */
    EXPECT_TRUE(introspection_immune_is_sickness_detected(bridge));
    EXPECT_GT(bridge->sickness_detection.sickness_confidence, 0.5f);
}

TEST_F(IntrospectionImmuneIntegrationTest, SicknessDetectionUncertaintyIncrease) {
    CreateBridge();

    introspection_immune_set_baseline(bridge);

    /* Release cytokines to increase uncertainty */
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 1.0f);
    ReleaseCytokine(BRAIN_CYTOKINE_TNF, 0.8f);
    introspection_immune_apply_cytokine_effects(bridge);

    introspection_immune_detect_sickness(bridge);

    /* Should detect sickness due to high uncertainty */
    EXPECT_TRUE(introspection_immune_is_sickness_detected(bridge));
}

TEST_F(IntrospectionImmuneIntegrationTest, SicknessDetectionMultipleIndicators) {
    CreateBridge();

    introspection_immune_set_baseline(bridge);

    /* Create both inflammation and cytokine effects */
    CreateInflammation(INFLAMMATION_SYSTEMIC);
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.9f);

    introspection_immune_apply_inflammation_effects(bridge);
    introspection_immune_apply_cytokine_effects(bridge);
    introspection_immune_detect_sickness(bridge);

    /* High confidence with multiple indicators */
    EXPECT_TRUE(introspection_immune_is_sickness_detected(bridge));
    EXPECT_GT(bridge->sickness_detection.sickness_confidence, 0.7f);
}

TEST_F(IntrospectionImmuneIntegrationTest, SicknessDetectionNoSickness) {
    CreateBridge();

    introspection_immune_set_baseline(bridge);

    /* No immune activity */
    introspection_immune_detect_sickness(bridge);

    EXPECT_FALSE(introspection_immune_is_sickness_detected(bridge));
    EXPECT_FLOAT_EQ(bridge->sickness_detection.sickness_confidence, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, SicknessDetectionReset) {
    CreateBridge();

    introspection_immune_set_baseline(bridge);

    /* Trigger sickness detection */
    CreateInflammation(INFLAMMATION_SYSTEMIC);
    introspection_immune_apply_inflammation_effects(bridge);
    introspection_immune_detect_sickness(bridge);
    EXPECT_TRUE(introspection_immune_is_sickness_detected(bridge));

    /* Reset detection */
    int result = introspection_immune_reset_sickness_detection(bridge);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(introspection_immune_is_sickness_detected(bridge));
    EXPECT_FLOAT_EQ(bridge->sickness_detection.sickness_confidence, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, SicknessDetectionDisabled) {
    introspection_immune_config_t config;
    introspection_immune_default_config(&config);
    config.enable_sickness_detection = false;
    CreateBridgeWithConfig(&config);

    introspection_immune_set_baseline(bridge);
    CreateInflammation(INFLAMMATION_STORM);
    introspection_immune_apply_inflammation_effects(bridge);

    int result = introspection_immune_detect_sickness(bridge);
    EXPECT_EQ(result, 0); /* Returns success but does nothing */

    /* Should not increment sickness detection counter */
    EXPECT_EQ(bridge->sickness_detections, 0);
}

/* ============================================================================
 * Pattern Correlation Tests
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, PatternImmuneCorrelation) {
    CreateBridge();

    /* Start immune system */
    brain_immune_start(immune_system);

    /* Present antigen to activate immune */
    uint8_t epitope[32] = {0x01, 0x02, 0x03};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL, epitope, 3, 5, 0, &antigen_id);

    /* Correlate patterns */
    int result = introspection_immune_correlate_patterns(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(IntrospectionImmuneIntegrationTest, PatternCorrelationDisabled) {
    introspection_immune_config_t config;
    introspection_immune_default_config(&config);
    config.enable_pattern_immune_correlation = false;
    CreateBridgeWithConfig(&config);

    int result = introspection_immune_correlate_patterns(bridge);
    EXPECT_EQ(result, 0); /* Returns success but does nothing */
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, BridgeUpdateFullCycle) {
    CreateBridge();

    introspection_immune_set_baseline(bridge);

    /* Create immune activity */
    CreateInflammation(INFLAMMATION_REGIONAL);
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.7f);

    /* Run full update */
    int result = introspection_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    /* Should have processed both directions */
    EXPECT_GT(bridge->total_updates, 0);
    EXPECT_GT(bridge->cytokine_modulations, 0);

    /* Introspection should be impaired */
    float phi_reduction = introspection_immune_get_phi_reduction(bridge);
    EXPECT_GT(phi_reduction, 0.0f);

    float accuracy_loss = introspection_immune_get_accuracy_loss(bridge);
    EXPECT_GT(accuracy_loss, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, BridgeUpdateMultipleCycles) {
    CreateBridge();

    introspection_immune_set_baseline(bridge);
    CreateInflammation(INFLAMMATION_SYSTEMIC);

    /* Run multiple updates */
    for (int i = 0; i < 10; i++) {
        introspection_immune_bridge_update(bridge, 100);
    }

    EXPECT_EQ(bridge->total_updates, 10);
}

TEST_F(IntrospectionImmuneIntegrationTest, BridgeUpdateNullBridge) {
    int result = introspection_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, QueryCytokineEffects) {
    CreateBridge();

    ReleaseCytokine(BRAIN_CYTOKINE_IL1, 0.5f);
    introspection_immune_apply_cytokine_effects(bridge);

    cytokine_introspection_effects_t effects;
    int result = introspection_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_GT(effects.consciousness_impairment_level, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, QueryConsciousnessState) {
    CreateBridge();

    CreateInflammation(INFLAMMATION_REGIONAL);
    introspection_immune_apply_inflammation_effects(bridge);

    inflammation_consciousness_state_t state;
    int result = introspection_immune_get_consciousness_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.current_level, INFLAMMATION_REGIONAL);
    EXPECT_GT(state.phi_reduction, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, QueryNullInputs) {
    CreateBridge();

    cytokine_introspection_effects_t effects;
    EXPECT_EQ(introspection_immune_get_cytokine_effects(nullptr, &effects), -1);
    EXPECT_EQ(introspection_immune_get_cytokine_effects(bridge, nullptr), -1);

    inflammation_consciousness_state_t state;
    EXPECT_EQ(introspection_immune_get_consciousness_state(nullptr, &state), -1);
    EXPECT_EQ(introspection_immune_get_consciousness_state(bridge, nullptr), -1);
}

/* ============================================================================
 * Biological Accuracy Tests
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, BiologicalAccuracyIL6PrefrontalImpairment) {
    CreateBridge();

    /* IL-6 specifically impairs prefrontal metacognitive circuits */
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.8f);
    introspection_immune_apply_cytokine_effects(bridge);

    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);

    /* IL-6 should have stronger impact than other cytokines */
    EXPECT_LT(effects.il6_accuracy_impairment, CYTOKINE_IL6_INTROSPECTION_IMPACT * 0.5f);
    EXPECT_GT(effects.consciousness_impairment_level, 0.2f);
}

TEST_F(IntrospectionImmuneIntegrationTest, BiologicalAccuracyFeverStateConsciousnessReduction) {
    CreateBridge();

    /* Fever (cytokine storm) should severely reduce consciousness */
    CreateInflammation(INFLAMMATION_STORM);
    introspection_immune_apply_inflammation_effects(bridge);

    inflammation_consciousness_state_t state;
    introspection_immune_get_consciousness_state(bridge, &state);

    /* Severe Phi reduction in storm state */
    EXPECT_GT(state.phi_reduction, 0.5f);
    EXPECT_GT(state.state_clarity_loss, 0.5f);
}

TEST_F(IntrospectionImmuneIntegrationTest, BiologicalAccuracyMetacognitiveMonitoring) {
    CreateBridge();

    introspection_immune_set_baseline(bridge);

    /* Introspection should detect abnormal internal state */
    CreateInflammation(INFLAMMATION_SYSTEMIC);
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.9f);

    introspection_immune_apply_inflammation_effects(bridge);
    introspection_immune_apply_cytokine_effects(bridge);
    introspection_immune_detect_sickness(bridge);

    /* Self-awareness of sickness state */
    EXPECT_TRUE(introspection_immune_is_sickness_detected(bridge));
    EXPECT_GT(bridge->sickness_detection.sickness_confidence, 0.6f);
}

/* ============================================================================
 * Edge Cases and Error Handling
 * ============================================================================ */

TEST_F(IntrospectionImmuneIntegrationTest, EdgeCaseMaximumCytokineConcentration) {
    CreateBridge();

    /* Maximum cytokine concentration */
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 1.0f);
    ReleaseCytokine(BRAIN_CYTOKINE_TNF, 1.0f);
    ReleaseCytokine(BRAIN_CYTOKINE_IL1, 1.0f);

    introspection_immune_apply_cytokine_effects(bridge);

    /* Should not exceed maximum impairment */
    float phi_reduction = introspection_immune_get_phi_reduction(bridge);
    EXPECT_LE(phi_reduction, INFLAMMATION_PHI_REDUCTION_MAX);

    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_LE(effects.consciousness_impairment_level, 1.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, EdgeCaseZeroCytokineConcentration) {
    CreateBridge();

    /* Zero cytokine concentration */
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 0.0f);

    introspection_immune_apply_cytokine_effects(bridge);

    cytokine_introspection_effects_t effects;
    introspection_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_FLOAT_EQ(effects.consciousness_impairment_level, 0.0f);
}

TEST_F(IntrospectionImmuneIntegrationTest, EdgeCasePhiReductionClamping) {
    CreateBridge();

    /* Extreme inflammation */
    CreateInflammation(INFLAMMATION_STORM);
    ReleaseCytokine(BRAIN_CYTOKINE_IL6, 1.0f);

    introspection_immune_apply_inflammation_effects(bridge);
    introspection_immune_apply_cytokine_effects(bridge);

    /* Phi reduction should be clamped to max */
    float phi_reduction = introspection_immune_compute_phi_reduction(bridge);
    EXPECT_LE(phi_reduction, INFLAMMATION_PHI_REDUCTION_MAX);
    EXPECT_GE(phi_reduction, 0.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
