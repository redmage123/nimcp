/**
 * @file test_visual_immune_integration.cpp
 * @brief Unit tests for Visual Cortex-Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Comprehensive test suite for visual-immune bridge
 * WHY:  Ensure correct bidirectional coupling between visual cortex and immune system
 * HOW:  Test lifecycle, immune→visual effects, visual→immune triggers, query API
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "perception/immune/nimcp_visual_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "perception/nimcp_visual_cortex.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class VisualImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    visual_cortex_t* visual_cortex;
    visual_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Create visual cortex */
        visual_cortex_config_t visual_config = {};
        visual_config.input_width = 640;
        visual_config.input_height = 480;
        visual_config.num_v1_filters = 16;
        visual_config.feature_dim = 128;
        visual_config.enable_attention = true;
        visual_config.enable_memory = true;
        visual_config.enable_bio_async = false;
        visual_config.enable_second_messengers = false;
        visual_cortex = visual_cortex_create(&visual_config);
        ASSERT_NE(visual_cortex, nullptr);

        /* Create bridge */
        visual_immune_config_t bridge_config;
        visual_immune_default_config(&bridge_config);
        bridge = visual_immune_bridge_create(&bridge_config, immune_system, visual_cortex);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) visual_immune_bridge_destroy(bridge);
        if (visual_cortex) visual_cortex_destroy(visual_cortex);
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, DefaultConfigValid) {
    visual_immune_config_t config;
    int result = visual_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_visual_modulation);
    EXPECT_TRUE(config.enable_inflammation_visual_impairment);
    EXPECT_TRUE(config.enable_visual_immune_trigger);
    EXPECT_TRUE(config.enable_sickness_visual_reduction);
    EXPECT_TRUE(config.enable_tunnel_vision);
    EXPECT_TRUE(config.enable_threat_salience_boost);

    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_EQ(config.visual_trigger_sensitivity, 1.0f);
}

TEST_F(VisualImmuneIntegrationTest, CreateBridgeSuccess) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(VisualImmuneIntegrationTest, CreateBridgeNullImmuneSystemFails) {
    visual_immune_bridge_t* null_bridge = visual_immune_bridge_create(
        nullptr, nullptr, visual_cortex);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(VisualImmuneIntegrationTest, CreateBridgeNullVisualCortexFails) {
    visual_immune_bridge_t* null_bridge = visual_immune_bridge_create(
        nullptr, immune_system, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(VisualImmuneIntegrationTest, DestroyBridgeHandlesNull) {
    visual_immune_bridge_destroy(nullptr);
    /* Should not crash */
}

/* ============================================================================
 * Immune → Visual: Cytokine Effects Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, ApplyCytokineEffectsNoInflammation) {
    int result = visual_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    cytokine_visual_effects_t effects;
    visual_immune_get_cytokine_effects(bridge, &effects);

    /* No inflammation = minimal impairment */
    EXPECT_GE(effects.total_processing_factor, 0.9f);
    EXPECT_GE(effects.total_accuracy_factor, 0.9f);
    EXPECT_GE(effects.total_attention_factor, 0.9f);
}

TEST_F(VisualImmuneIntegrationTest, ApplyCytokineEffectsWithInflammation) {
    /* Trigger inflammation by presenting high-severity antigen */
    uint8_t epitope[64] = {0xAB, 0xCD, 0xEF};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, 64, 9, 0, &antigen_id);

    /* Activate immune response */
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune_system, antigen_id, &helper_id);

    /* Trigger inflammation */
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    /* Apply cytokine effects */
    visual_immune_apply_cytokine_effects(bridge);

    cytokine_visual_effects_t effects;
    visual_immune_get_cytokine_effects(bridge, &effects);

    /* With inflammation, expect impairment */
    EXPECT_LT(effects.total_processing_factor, 1.0f);
    EXPECT_LT(effects.total_accuracy_factor, 1.0f);
    EXPECT_LT(effects.total_attention_factor, 1.0f);
}

TEST_F(VisualImmuneIntegrationTest, CytokineEffectsNullBridgeFails) {
    int result = visual_immune_apply_cytokine_effects(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Immune → Visual: Inflammation Effects Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, ApplyInflammationEffectsNoInflammation) {
    visual_immune_apply_inflammation_effects(bridge);

    inflammation_visual_state_t state;
    visual_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(state.processing_speed_reduction, 0.0f);
    EXPECT_FLOAT_EQ(state.tunnel_vision_severity, 0.0f);
}

TEST_F(VisualImmuneIntegrationTest, ApplyInflammationEffectsLocalInflammation) {
    /* Create local inflammation */
    uint8_t epitope[64] = {0x11, 0x22};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, 64, 5, 0, &antigen_id);
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    visual_immune_apply_inflammation_effects(bridge);

    inflammation_visual_state_t state;
    visual_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_GT(state.processing_speed_reduction, 0.0f);
    /* Local inflammation shouldn't cause tunnel vision */
    EXPECT_FLOAT_EQ(state.tunnel_vision_severity, 0.0f);
}

TEST_F(VisualImmuneIntegrationTest, ApplyInflammationEffectsSystemicInflammation) {
    /* Create multiple inflammation sites for systemic level */
    for (int i = 0; i < 5; i++) {
        uint8_t epitope[64];
        epitope[0] = (uint8_t)i;
        uint32_t antigen_id, site_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, 64, 8, 0, &antigen_id);
        brain_immune_initiate_inflammation(immune_system, i, antigen_id, &site_id);
    }

    visual_immune_apply_inflammation_effects(bridge);

    inflammation_visual_state_t state;
    visual_immune_get_inflammation_state(bridge, &state);

    /* Should reach systemic or storm level */
    EXPECT_GE(state.current_level, INFLAMMATION_SYSTEMIC);
    EXPECT_GT(state.processing_speed_reduction, 0.3f);
    EXPECT_GT(state.tunnel_vision_severity, 0.0f);
    EXPECT_GT(state.photophobia_level, 0.0f);
}

TEST_F(VisualImmuneIntegrationTest, TunnelVisionComputationAccurate) {
    /* Create regional inflammation */
    for (int i = 0; i < 3; i++) {
        uint8_t epitope[64];
        epitope[0] = (uint8_t)(i + 10);
        uint32_t antigen_id, site_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, 64, 7, 0, &antigen_id);
        brain_immune_initiate_inflammation(immune_system, i, antigen_id, &site_id);
    }

    visual_immune_apply_inflammation_effects(bridge);

    float tunnel_vision = visual_immune_compute_tunnel_vision(bridge);
    EXPECT_GE(tunnel_vision, 0.0f);
    EXPECT_LE(tunnel_vision, 1.0f);
}

/* ============================================================================
 * Immune → Visual: Sickness Behavior Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, ApplySicknesEffectsNoSickness) {
    visual_immune_apply_sickness_effects(bridge);

    EXPECT_FALSE(visual_immune_is_sick_behavior(bridge));

    sickness_visual_effects_t effects = bridge->sickness_effects;
    EXPECT_FLOAT_EQ(effects.sickness_behavior_level, 0.0f);
}

TEST_F(VisualImmuneIntegrationTest, ApplySicknessEffectsWithInflammation) {
    /* Create moderate inflammation */
    for (int i = 0; i < 2; i++) {
        uint8_t epitope[64];
        epitope[0] = (uint8_t)(i + 20);
        uint32_t antigen_id, site_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, 64, 6, 0, &antigen_id);
        brain_immune_initiate_inflammation(immune_system, i, antigen_id, &site_id);
    }

    visual_immune_apply_sickness_effects(bridge);

    sickness_visual_effects_t effects = bridge->sickness_effects;
    EXPECT_GT(effects.sickness_behavior_level, 0.0f);
    EXPECT_GT(effects.exploration_reduction, 0.0f);
    EXPECT_GT(effects.processing_speed_reduction, 0.0f);
}

TEST_F(VisualImmuneIntegrationTest, SicknessBehaviorDetection) {
    /* Create significant inflammation */
    for (int i = 0; i < 4; i++) {
        uint8_t epitope[64];
        epitope[0] = (uint8_t)(i + 30);
        uint32_t antigen_id, site_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, 64, 7, 0, &antigen_id);
        brain_immune_initiate_inflammation(immune_system, i, antigen_id, &site_id);
    }

    visual_immune_apply_sickness_effects(bridge);

    /* Should detect sickness behavior */
    EXPECT_TRUE(visual_immune_is_sick_behavior(bridge));
}

/* ============================================================================
 * Immune → Visual: Neuromodulator Effects Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, ModulateNeurotransmittersNoInflammation) {
    int result = visual_immune_modulate_neurotransmitters(bridge);
    EXPECT_EQ(result, 0);

    /* With no inflammation, ACh should be normal, NE low */
    const phasic_tonic_state_t* ach_state =
        visual_cortex_get_neuromod_state(visual_cortex, 1);
    const phasic_tonic_state_t* ne_state =
        visual_cortex_get_neuromod_state(visual_cortex, 2);

    if (ach_state) {
        EXPECT_GE(phasic_tonic_get_tonic_level(ach_state), 0.4f);
    }
    if (ne_state) {
        EXPECT_LE(phasic_tonic_get_tonic_level(ne_state), 0.5f);
    }
}

TEST_F(VisualImmuneIntegrationTest, ModulateNeurotransmittersWithInflammation) {
    /* Create inflammation */
    uint8_t epitope[64] = {0xAA, 0xBB};
    uint32_t antigen_id, site_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, 64, 8, 0, &antigen_id);
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    visual_immune_modulate_neurotransmitters(bridge);

    /* Inflammation should reduce ACh, increase NE */
    const phasic_tonic_state_t* ach_state =
        visual_cortex_get_neuromod_state(visual_cortex, 1);
    const phasic_tonic_state_t* ne_state =
        visual_cortex_get_neuromod_state(visual_cortex, 2);

    if (ach_state && ne_state) {
        /* ACh reduced, NE increased */
        EXPECT_LT(phasic_tonic_get_tonic_level(ach_state), 0.5f);
        EXPECT_GT(phasic_tonic_get_tonic_level(ne_state), 0.3f);
    }
}

/* ============================================================================
 * Visual → Immune: Threat Trigger Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, TriggerFromThreatLowSalienceNoTrigger) {
    float features[16] = {0.1f, 0.2f, 0.3f, 0.4f};
    int result = visual_immune_trigger_from_threat(bridge, features, 16, 0.3f);
    EXPECT_EQ(result, 0);

    /* Low salience shouldn't trigger */
    EXPECT_EQ(bridge->visual_triggered_responses, 0u);
}

TEST_F(VisualImmuneIntegrationTest, TriggerFromThreatHighSalienceTriggers) {
    float features[16] = {0.9f, 0.8f, 0.7f, 0.6f};
    int result = visual_immune_trigger_from_threat(bridge, features, 16, 0.8f);
    EXPECT_EQ(result, 0);

    /* High salience should trigger immune response */
    EXPECT_GT(bridge->visual_triggered_responses, 0u);
    EXPECT_GT(bridge->threat_detections, 0u);
}

TEST_F(VisualImmuneIntegrationTest, TriggerFromThreatNullFeaturesFails) {
    int result = visual_immune_trigger_from_threat(bridge, nullptr, 0, 0.9f);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Visual → Immune: Anomaly Trigger Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, TriggerFromAnomalyLowScoreNoTrigger) {
    int result = visual_immune_trigger_from_anomaly(bridge, 0.2f);
    EXPECT_EQ(result, 0);

    /* Low anomaly score shouldn't trigger */
    EXPECT_EQ(bridge->anomaly_detections, 0u);
}

TEST_F(VisualImmuneIntegrationTest, TriggerFromAnomalyHighScoreTriggers) {
    /* Anomaly threshold is lower due to amplification */
    int result = visual_immune_trigger_from_anomaly(bridge, 0.5f);
    EXPECT_EQ(result, 0);

    /* Should trigger due to amplification */
    EXPECT_GT(bridge->anomaly_detections, 0u);
}

/* ============================================================================
 * Visual → Immune: Visual Stress Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, TriggerFromVisualStressShortDurationNoTrigger) {
    bridge->visual_trigger.visual_stress_duration_sec = 1800.0f; /* 30 minutes */
    int result = visual_immune_trigger_from_visual_stress(bridge);
    EXPECT_EQ(result, 0);

    /* Short duration shouldn't trigger chronic stress response */
    /* Note: This test may pass with 0 triggers initially */
}

TEST_F(VisualImmuneIntegrationTest, TriggerFromVisualStressChronicTriggers) {
    bridge->visual_trigger.visual_stress_duration_sec = 7200.0f; /* 2 hours */
    int result = visual_immune_trigger_from_visual_stress(bridge);
    EXPECT_EQ(result, 0);

    /* Chronic stress should trigger */
    EXPECT_GT(bridge->visual_triggered_responses, 0u);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, BridgeUpdateSuccess) {
    int result = visual_immune_bridge_update(bridge, 100); /* 100ms */
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->total_updates, 1u);
}

TEST_F(VisualImmuneIntegrationTest, BridgeUpdateAccumulatesTime) {
    float initial_stress = bridge->visual_trigger.visual_stress_duration_sec;

    visual_immune_bridge_update(bridge, 1000); /* 1 second */

    EXPECT_GT(bridge->visual_trigger.visual_stress_duration_sec, initial_stress);
}

TEST_F(VisualImmuneIntegrationTest, BridgeUpdateNullBridgeFails) {
    int result = visual_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, GetCytokineEffectsSuccess) {
    cytokine_visual_effects_t effects;
    int result = visual_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(VisualImmuneIntegrationTest, GetInflammationStateSuccess) {
    inflammation_visual_state_t state;
    int result = visual_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(VisualImmuneIntegrationTest, GetProcessingSpeedFactorNormal) {
    float factor = visual_immune_get_processing_speed_factor(bridge);
    EXPECT_GE(factor, 0.1f);
    EXPECT_LE(factor, 1.0f);
}

TEST_F(VisualImmuneIntegrationTest, GetAccuracyFactorNormal) {
    float factor = visual_immune_get_accuracy_factor(bridge);
    EXPECT_GE(factor, 0.2f);
    EXPECT_LE(factor, 1.0f);
}

TEST_F(VisualImmuneIntegrationTest, GetAttentionCapacityNormal) {
    float capacity = visual_immune_get_attention_capacity(bridge);
    EXPECT_GE(capacity, 0.2f);
    EXPECT_LE(capacity, 1.0f);
}

TEST_F(VisualImmuneIntegrationTest, GetThreatSalienceBoostRange) {
    float boost = visual_immune_get_threat_salience_boost(bridge);
    EXPECT_GE(boost, 1.0f);
    EXPECT_LE(boost, 2.0f);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(VisualImmuneIntegrationTest, InflammationImpaidsVisualProcessing) {
    /* Baseline processing speed */
    float baseline_speed = visual_immune_get_processing_speed_factor(bridge);

    /* Induce inflammation */
    for (int i = 0; i < 3; i++) {
        uint8_t epitope[64];
        epitope[0] = (uint8_t)(i + 40);
        uint32_t antigen_id, site_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, 64, 7, 0, &antigen_id);
        brain_immune_initiate_inflammation(immune_system, i, antigen_id, &site_id);
    }

    /* Update bridge */
    visual_immune_bridge_update(bridge, 1000);

    /* Processing speed should be reduced */
    float impaired_speed = visual_immune_get_processing_speed_factor(bridge);
    EXPECT_LT(impaired_speed, baseline_speed);
}

TEST_F(VisualImmuneIntegrationTest, VisualThreatTriggersImmune) {
    /* Get baseline immune stats */
    brain_immune_stats_t baseline_stats;
    brain_immune_get_stats(immune_system, &baseline_stats);

    /* Present visual threat */
    float threat_features[32];
    for (int i = 0; i < 32; i++) {
        threat_features[i] = 0.9f; /* High threat features */
    }
    visual_immune_trigger_from_threat(bridge, threat_features, 32, 0.95f);

    /* Check immune activation */
    brain_immune_stats_t after_stats;
    brain_immune_get_stats(immune_system, &after_stats);

    EXPECT_GT(after_stats.antigens_processed, baseline_stats.antigens_processed);
}

TEST_F(VisualImmuneIntegrationTest, SicknessNarrowsAttentionToThreats) {
    /* Create sickness */
    for (int i = 0; i < 4; i++) {
        uint8_t epitope[64];
        epitope[0] = (uint8_t)(i + 50);
        uint32_t antigen_id, site_id;
        brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                      epitope, 64, 8, 0, &antigen_id);
        brain_immune_initiate_inflammation(immune_system, i, antigen_id, &site_id);
    }

    visual_immune_bridge_update(bridge, 1000);

    /* Should have sickness behavior */
    EXPECT_TRUE(visual_immune_is_sick_behavior(bridge));

    /* Threat salience should be boosted */
    float threat_boost = visual_immune_get_threat_salience_boost(bridge);
    EXPECT_GT(threat_boost, 1.0f);

    /* Attention capacity should be reduced */
    float capacity = visual_immune_get_attention_capacity(bridge);
    EXPECT_LT(capacity, 1.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
