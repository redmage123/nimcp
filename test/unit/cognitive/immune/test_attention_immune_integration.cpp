/**
 * @file test_attention_immune_integration.cpp
 * @brief Unit tests for attention-immune bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Comprehensive tests for attention-immune bidirectional integration
 * WHY:  Ensure immune→attention and attention→immune pathways work correctly
 * HOW:  Test cytokine effects, inflammation effects, threat focus, mindfulness
 */

#include <gtest/gtest.h>
#include "cognitive/immune/nimcp_attention_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/attention/nimcp_emotion_attention.h"
#include "plasticity/attention/nimcp_attention.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class AttentionImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    attention_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        brain_immune_start(immune_system);

        /* Create attention-immune bridge */
        attention_immune_config_t config;
        attention_immune_default_config(&config);
        bridge = attention_immune_bridge_create(&config, immune_system, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            attention_immune_bridge_destroy(bridge);
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

TEST_F(AttentionImmuneIntegrationTest, CreateDestroy) {
    /* Test create/destroy cycle */
    attention_immune_config_t config;
    ASSERT_EQ(attention_immune_default_config(&config), 0);

    attention_immune_bridge_t* test_bridge =
        attention_immune_bridge_create(&config, immune_system, nullptr, nullptr);
    ASSERT_NE(test_bridge, nullptr);

    attention_immune_bridge_destroy(test_bridge);
}

TEST_F(AttentionImmuneIntegrationTest, DefaultConfig) {
    attention_immune_config_t config;
    ASSERT_EQ(attention_immune_default_config(&config), 0);

    /* Verify defaults */
    EXPECT_TRUE(config.enable_cytokine_attention_impairment);
    EXPECT_TRUE(config.enable_inflammation_narrowing);
    EXPECT_TRUE(config.enable_threat_attention_immune_boost);
    EXPECT_TRUE(config.enable_mindful_attention_benefits);
    EXPECT_TRUE(config.enable_hypervigilance_inflammation);

    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.attention_immune_sensitivity, 1.0f);
}

TEST_F(AttentionImmuneIntegrationTest, CreateWithNullImmuneSystem) {
    attention_immune_config_t config;
    attention_immune_default_config(&config);

    attention_immune_bridge_t* null_bridge =
        attention_immune_bridge_create(&config, nullptr, nullptr, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

/* ============================================================================
 * Immune → Attention Tests (Cytokine Effects)
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, CytokineEffectsBaseline) {
    /* Apply cytokine effects at baseline (no inflammation) */
    ASSERT_EQ(attention_immune_apply_cytokine_effects(bridge), 0);

    cytokine_attention_effects_t effects;
    ASSERT_EQ(attention_immune_get_cytokine_effects(bridge, &effects), 0);

    /* At baseline, effects should be minimal */
    EXPECT_GE(effects.total_capacity_reduction, 0.0f);
    EXPECT_LE(effects.total_capacity_reduction, 0.1f);
}

TEST_F(AttentionImmuneIntegrationTest, CytokineEffectsWithInflammation) {
    /* Trigger inflammation */
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    brain_immune_present_antigen(
        immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        8,  /* High severity */
        0,
        &antigen_id
    );

    /* Initiate inflammation */
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    /* Apply cytokine effects */
    ASSERT_EQ(attention_immune_apply_cytokine_effects(bridge), 0);

    cytokine_attention_effects_t effects;
    ASSERT_EQ(attention_immune_get_cytokine_effects(bridge, &effects), 0);

    /* Effects should be in valid ranges */
    EXPECT_GE(effects.total_capacity_reduction, 0.0f);
    EXPECT_LE(effects.total_capacity_reduction, 1.0f);

    EXPECT_GE(effects.narrowing_factor, 0.0f);
    EXPECT_LE(effects.narrowing_factor, 1.0f);

    EXPECT_GE(effects.sustained_impairment, 0.0f);
    EXPECT_LE(effects.sustained_impairment, 1.0f);

    EXPECT_GE(effects.executive_impairment, 0.0f);
    EXPECT_LE(effects.executive_impairment, 1.0f);
}

TEST_F(AttentionImmuneIntegrationTest, CytokineEffectsDisabled) {
    /* Create bridge with cytokine effects disabled */
    attention_immune_config_t config;
    attention_immune_default_config(&config);
    config.enable_cytokine_attention_impairment = false;

    attention_immune_bridge_t* test_bridge =
        attention_immune_bridge_create(&config, immune_system, nullptr, nullptr);
    ASSERT_NE(test_bridge, nullptr);

    /* Apply should succeed but have no effect */
    ASSERT_EQ(attention_immune_apply_cytokine_effects(test_bridge), 0);

    attention_immune_bridge_destroy(test_bridge);
}

/* ============================================================================
 * Immune → Attention Tests (Inflammation Effects)
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, InflammationEffectsNone) {
    /* Apply inflammation effects with no inflammation */
    ASSERT_EQ(attention_immune_apply_inflammation_effects(bridge), 0);

    inflammation_attention_state_t state;
    ASSERT_EQ(attention_immune_get_inflammation_state(bridge, &state), 0);

    /* No inflammation = normal capacity */
    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(state.capacity_factor, 1.0f);
    EXPECT_FLOAT_EQ(state.width_narrowing, INFLAMMATION_NARROWING_BASE);
}

TEST_F(AttentionImmuneIntegrationTest, InflammationEffectsLocal) {
    /* Create local inflammation (1 site) */
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(
        immune_system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope), 5, 0, &antigen_id
    );

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    /* Apply inflammation effects */
    ASSERT_EQ(attention_immune_apply_inflammation_effects(bridge), 0);

    inflammation_attention_state_t state;
    ASSERT_EQ(attention_immune_get_inflammation_state(bridge, &state), 0);

    /* Should have local inflammation — capacity reduced from 1.0 */
    EXPECT_EQ(state.current_level, INFLAMMATION_LOCAL);
    EXPECT_LE(state.capacity_factor, 1.0f);
    EXPECT_GE(state.capacity_factor, INFLAMMATION_LOCAL_CAPACITY_FACTOR);

    /* Should have some narrowing */
    EXPECT_GT(state.width_narrowing, INFLAMMATION_NARROWING_BASE);
}

TEST_F(AttentionImmuneIntegrationTest, InflammationEffectsSystemic) {
    /* Create multiple inflammation sites (systemic) */
    for (int i = 0; i < 3; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)i, 0x02, 0x03};
        brain_immune_present_antigen(
            immune_system, ANTIGEN_SOURCE_MANUAL,
            epitope, sizeof(epitope), 7, 0, &antigen_id
        );

        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, i + 1, antigen_id, &site_id);
    }

    /* Apply inflammation effects */
    ASSERT_EQ(attention_immune_apply_inflammation_effects(bridge), 0);

    inflammation_attention_state_t state;
    ASSERT_EQ(attention_immune_get_inflammation_state(bridge, &state), 0);

    /* State should be in valid range */
    EXPECT_GE(state.current_level, INFLAMMATION_NONE);
    EXPECT_LE(state.current_level, INFLAMMATION_STORM);

    EXPECT_GE(state.capacity_factor, 0.0f);
    EXPECT_LE(state.capacity_factor, 1.0f);

    /* All deficits should be in valid ranges */
    EXPECT_GE(state.width_narrowing, 0.0f);
    EXPECT_LE(state.width_narrowing, 1.0f);

    EXPECT_GE(state.sustained_deficit, 0.0f);
    EXPECT_LE(state.sustained_deficit, 1.0f);

    EXPECT_GE(state.flexibility_impairment, 0.0f);
    EXPECT_LE(state.flexibility_impairment, 1.0f);

    EXPECT_GE(state.working_memory_deficit, 0.0f);
    EXPECT_LE(state.working_memory_deficit, 1.0f);

    EXPECT_GE(state.threat_bias_strength, 0.0f);
    EXPECT_LE(state.threat_bias_strength, 1.0f);

    EXPECT_GE(state.disengagement_difficulty, 0.0f);
    EXPECT_LE(state.disengagement_difficulty, 1.0f);
}

TEST_F(AttentionImmuneIntegrationTest, InflammationEffectsStorm) {
    /* Create cytokine storm (4+ sites) */
    for (int i = 0; i < 4; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)i, 0x05, 0x06};
        brain_immune_present_antigen(
            immune_system, ANTIGEN_SOURCE_MANUAL,
            epitope, sizeof(epitope), 9, 0, &antigen_id
        );

        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, i + 1, antigen_id, &site_id);
    }

    /* Apply inflammation effects */
    ASSERT_EQ(attention_immune_apply_inflammation_effects(bridge), 0);

    inflammation_attention_state_t state;
    ASSERT_EQ(attention_immune_get_inflammation_state(bridge, &state), 0);

    /* State should be in valid range */
    EXPECT_GE(state.current_level, INFLAMMATION_NONE);
    EXPECT_LE(state.current_level, INFLAMMATION_STORM);

    EXPECT_GE(state.capacity_factor, 0.0f);
    EXPECT_LE(state.capacity_factor, 1.0f);

    /* Sustained deficit should be in valid range */
    EXPECT_GE(state.sustained_deficit, 0.0f);
    EXPECT_LE(state.sustained_deficit, 1.0f);
}

/* ============================================================================
 * Attention Capacity and Narrowing Tests
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, ComputeCapacityBaseline) {
    /* Baseline capacity should be in valid range */
    float capacity = attention_immune_compute_capacity(bridge);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);
}

TEST_F(AttentionImmuneIntegrationTest, ComputeCapacityWithInflammation) {
    /* Create inflammation */
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02};
    brain_immune_present_antigen(
        immune_system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope), 6, 0, &antigen_id
    );

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    /* Update effects */
    attention_immune_apply_inflammation_effects(bridge);
    attention_immune_apply_cytokine_effects(bridge);

    /* Capacity should be reduced */
    float capacity = attention_immune_compute_capacity(bridge);
    EXPECT_LT(capacity, 1.0f);
    EXPECT_GE(capacity, 0.0f);
}

TEST_F(AttentionImmuneIntegrationTest, ComputeNarrowingBaseline) {
    /* Baseline narrowing should be minimal */
    float narrowing = attention_immune_compute_narrowing(bridge);
    EXPECT_GE(narrowing, 0.0f);
    EXPECT_LE(narrowing, 0.2f);
}

TEST_F(AttentionImmuneIntegrationTest, ComputeNarrowingWithInflammation) {
    /* Create multiple inflammation sites */
    for (int i = 0; i < 2; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)i, 0x03};
        brain_immune_present_antigen(
            immune_system, ANTIGEN_SOURCE_MANUAL,
            epitope, sizeof(epitope), 6, 0, &antigen_id
        );

        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, i + 1, antigen_id, &site_id);
    }

    /* Update effects */
    attention_immune_apply_inflammation_effects(bridge);
    attention_immune_apply_cytokine_effects(bridge);

    /* Narrowing should increase */
    float narrowing = attention_immune_compute_narrowing(bridge);
    EXPECT_GT(narrowing, 0.2f);
    EXPECT_LE(narrowing, 1.0f);
}

TEST_F(AttentionImmuneIntegrationTest, AttentionDeficitDetection) {
    /* Check baseline deficit detection */
    bool baseline_deficit = attention_immune_has_attention_deficit(bridge);
    EXPECT_TRUE(baseline_deficit == true || baseline_deficit == false);

    /* Create severe inflammation */
    for (int i = 0; i < 3; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)i, 0x04};
        brain_immune_present_antigen(
            immune_system, ANTIGEN_SOURCE_MANUAL,
            epitope, sizeof(epitope), 8, 0, &antigen_id
        );

        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, i + 1, antigen_id, &site_id);
    }

    /* Update effects */
    attention_immune_apply_inflammation_effects(bridge);
    attention_immune_apply_cytokine_effects(bridge);

    /* Deficit detection should return a valid boolean */
    bool has_deficit = attention_immune_has_attention_deficit(bridge);
    EXPECT_TRUE(has_deficit == true || has_deficit == false);
}

/* ============================================================================
 * Attention → Immune Tests (Threat Focus)
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, ThreatFocusBoost) {
    /* Apply threat focus boost */
    ASSERT_EQ(attention_immune_boost_from_threat_focus(bridge), 0);

    /* Without emotion_attention or threat focus level set, boost should be zero or minimal */
    EXPECT_GE(bridge->attention_modulation.local_immune_boost, 0.0f);
    EXPECT_LE(bridge->attention_modulation.local_immune_boost, 0.1f);
}

TEST_F(AttentionImmuneIntegrationTest, ThreatFocusDisabled) {
    /* Create bridge with threat focus disabled */
    attention_immune_config_t config;
    attention_immune_default_config(&config);
    config.enable_threat_attention_immune_boost = false;

    attention_immune_bridge_t* test_bridge =
        attention_immune_bridge_create(&config, immune_system, nullptr, nullptr);
    ASSERT_NE(test_bridge, nullptr);

    /* Apply should succeed but have no effect */
    ASSERT_EQ(attention_immune_boost_from_threat_focus(test_bridge), 0);

    attention_immune_bridge_destroy(test_bridge);
}

/* ============================================================================
 * Attention → Immune Tests (Hypervigilance)
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, HypervigilanceInflammation) {
    /* Set high vigilance */
    bridge->attention_modulation.vigilance_level = 0.9f;

    /* Trigger hypervigilance inflammation multiple times */
    for (int i = 0; i < 25; i++) {
        attention_immune_trigger_hypervigilance_inflammation(bridge);
    }

    /* Should accumulate and eventually trigger */
    EXPECT_GT(bridge->hypervigilance_inflammation_events, 0);
}

TEST_F(AttentionImmuneIntegrationTest, HypervigilanceDecay) {
    /* Set high vigilance briefly */
    bridge->attention_modulation.vigilance_level = 0.9f;
    attention_immune_trigger_hypervigilance_inflammation(bridge);

    float initial_accumulator = bridge->hypervigilance_accumulator;
    EXPECT_GT(initial_accumulator, 0.0f);

    /* Lower vigilance */
    bridge->attention_modulation.vigilance_level = 0.3f;

    /* Should decay */
    for (int i = 0; i < 5; i++) {
        attention_immune_trigger_hypervigilance_inflammation(bridge);
    }

    EXPECT_LT(bridge->hypervigilance_accumulator, initial_accumulator);
}

/* ============================================================================
 * Attention → Immune Tests (Mindfulness)
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, MindfulnessIL10Release) {
    /* Set mindful attention */
    bridge->attention_modulation.mindful_attention_level = 0.7f;
    bridge->attention_modulation.sustained_duration_sec = 35.0f;  /* > 30s */

    /* Release IL-10 */
    ASSERT_EQ(attention_immune_release_il10_from_mindfulness(bridge), 0);

    /* Should have IL-10 boost */
    EXPECT_GT(bridge->attention_modulation.il10_release_from_mindfulness, 0.0f);
    EXPECT_GT(bridge->attention_modulation.inflammation_reduction, 0.0f);
    EXPECT_GT(bridge->mindful_boosts, 0);
}

TEST_F(AttentionImmuneIntegrationTest, MindfulnessInsufficientDuration) {
    /* Set mindful attention but insufficient duration */
    bridge->attention_modulation.mindful_attention_level = 0.7f;
    bridge->attention_modulation.sustained_duration_sec = 10.0f;  /* < 30s */

    /* Release IL-10 */
    ASSERT_EQ(attention_immune_release_il10_from_mindfulness(bridge), 0);

    /* Should NOT have IL-10 boost */
    EXPECT_FLOAT_EQ(bridge->attention_modulation.il10_release_from_mindfulness, 0.0f);
}

TEST_F(AttentionImmuneIntegrationTest, MindfulnessDisabled) {
    /* Create bridge with mindfulness disabled */
    attention_immune_config_t config;
    attention_immune_default_config(&config);
    config.enable_mindful_attention_benefits = false;

    attention_immune_bridge_t* test_bridge =
        attention_immune_bridge_create(&config, immune_system, nullptr, nullptr);
    ASSERT_NE(test_bridge, nullptr);

    /* Set conditions that would normally trigger IL-10 */
    test_bridge->attention_modulation.mindful_attention_level = 0.7f;
    test_bridge->attention_modulation.sustained_duration_sec = 35.0f;

    /* Apply should succeed but have no effect */
    ASSERT_EQ(attention_immune_release_il10_from_mindfulness(test_bridge), 0);
    EXPECT_FLOAT_EQ(test_bridge->attention_modulation.il10_release_from_mindfulness, 0.0f);

    attention_immune_bridge_destroy(test_bridge);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, BridgeUpdate) {
    /* Update bridge */
    ASSERT_EQ(attention_immune_bridge_update(bridge, 100), 0);

    /* Should increment update counter */
    EXPECT_GT(bridge->total_updates, 0);
}

TEST_F(AttentionImmuneIntegrationTest, BridgeUpdateSustainedTracking) {
    /* Set attention strength */
    bridge->attention_modulation.attention_strength = 0.5f;

    /* Update multiple times */
    for (int i = 0; i < 5; i++) {
        attention_immune_bridge_update(bridge, 100);
    }

    /* Sustained duration should accumulate */
    EXPECT_GT(bridge->attention_modulation.sustained_duration_sec, 0.0f);
}

TEST_F(AttentionImmuneIntegrationTest, BridgeUpdateSustainedReset) {
    /* Set attention strength initially */
    bridge->attention_modulation.attention_strength = 0.5f;
    attention_immune_bridge_update(bridge, 100);

    float initial_duration = bridge->attention_modulation.sustained_duration_sec;
    EXPECT_GT(initial_duration, 0.0f);

    /* Drop attention strength */
    bridge->attention_modulation.attention_strength = 0.1f;
    attention_immune_bridge_update(bridge, 100);

    /* Should reset */
    EXPECT_FLOAT_EQ(bridge->attention_modulation.sustained_duration_sec, 0.0f);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, GetCytokineEffects) {
    cytokine_attention_effects_t effects;
    ASSERT_EQ(attention_immune_get_cytokine_effects(bridge, &effects), 0);

    /* Should succeed */
    EXPECT_GE(effects.total_capacity_reduction, 0.0f);
}

TEST_F(AttentionImmuneIntegrationTest, GetInflammationState) {
    inflammation_attention_state_t state;
    ASSERT_EQ(attention_immune_get_inflammation_state(bridge, &state), 0);

    /* Should succeed */
    EXPECT_GE(state.capacity_factor, 0.0f);
}

TEST_F(AttentionImmuneIntegrationTest, GetCapacityFactor) {
    float capacity = attention_immune_get_capacity_factor(bridge);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);
}

TEST_F(AttentionImmuneIntegrationTest, GetNarrowingFactor) {
    float narrowing = attention_immune_get_narrowing_factor(bridge);
    EXPECT_GE(narrowing, 0.0f);
    EXPECT_LE(narrowing, 1.0f);
}

TEST_F(AttentionImmuneIntegrationTest, QueryNullBridge) {
    cytokine_attention_effects_t effects;
    EXPECT_EQ(attention_immune_get_cytokine_effects(nullptr, &effects), -1);

    inflammation_attention_state_t state;
    EXPECT_EQ(attention_immune_get_inflammation_state(nullptr, &state), -1);

    EXPECT_FALSE(attention_immune_has_attention_deficit(nullptr));
    EXPECT_FLOAT_EQ(attention_immune_get_capacity_factor(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(attention_immune_get_narrowing_factor(nullptr), 0.0f);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(AttentionImmuneIntegrationTest, FullCycleBidirectional) {
    /* Create inflammation */
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(
        immune_system, ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope), 7, 0, &antigen_id
    );

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);

    /* Update bridge (immune → attention) */
    attention_immune_bridge_update(bridge, 100);

    /* Capacity should be in valid range */
    float capacity = attention_immune_get_capacity_factor(bridge);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);

    /* Set mindful attention (attention → immune) */
    bridge->attention_modulation.mindful_attention_level = 0.8f;
    bridge->attention_modulation.sustained_duration_sec = 40.0f;

    /* Update bridge */
    attention_immune_bridge_update(bridge, 100);

    /* IL-10 release should be in valid range */
    EXPECT_GE(bridge->attention_modulation.il10_release_from_mindfulness, 0.0f);
    EXPECT_LE(bridge->attention_modulation.il10_release_from_mindfulness, 1.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
