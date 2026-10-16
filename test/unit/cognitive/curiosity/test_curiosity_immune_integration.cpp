/**
 * @file test_curiosity_immune_integration.cpp
 * @brief Unit tests for curiosity-immune system integration
 *
 * Tests bidirectional coupling between curiosity and brain immune system:
 * - Immune → Curiosity: Sickness behavior suppression
 * - Curiosity → Immune: Novelty-triggered vigilance
 */

#include <gtest/gtest.h>
#include <cstring>

// Include header that transitively includes C++ CUDA templates outside extern "C"
#include "cognitive/immune/nimcp_brain_immune.h"

// Headers have their own extern "C" guards
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/immune/nimcp_curiosity_immune_bridge.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

class CuriosityImmuneIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    curiosity_engine_t curiosity = nullptr;
    brain_immune_system_t* immune = nullptr;
    curiosity_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create parent brain with proper signature */
        brain = brain_create("test_curiosity", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 10, 2);
        ASSERT_NE(brain, nullptr);

        /* Create curiosity engine */
        curiosity = curiosity_engine_create(brain, "test_learner");
        ASSERT_NE(curiosity, nullptr);

        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        brain_immune_start(immune);

        /* Create bridge directly (avoids needing to access opaque curiosity struct) */
        curiosity_immune_config_t bridge_config;
        curiosity_immune_default_config(&bridge_config);
        bridge = curiosity_immune_bridge_create(&bridge_config, immune, curiosity);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            curiosity_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (curiosity) {
            curiosity_engine_destroy(curiosity);
            curiosity = nullptr;
        }
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(CuriosityImmuneIntegrationTest, ConnectImmune_Success) {
    /* WHAT: Test successful immune connection */
    int result = curiosity_connect_immune(curiosity, immune);
    EXPECT_EQ(result, 0);

    /* Verify suppression starts at 1.0 (no suppression) */
    float suppression = curiosity_get_immune_suppression(curiosity);
    EXPECT_FLOAT_EQ(suppression, 1.0f);

    /* Verify vigilance starts at 1.0 (no boost) */
    float vigilance = curiosity_get_novelty_vigilance_boost(curiosity);
    EXPECT_FLOAT_EQ(vigilance, 1.0f);
}

TEST_F(CuriosityImmuneIntegrationTest, ConnectImmune_NullEngine) {
    /* WHAT: Test connection with NULL engine */
    int result = curiosity_connect_immune(nullptr, immune);
    EXPECT_EQ(result, -1);
}

TEST_F(CuriosityImmuneIntegrationTest, ConnectImmune_NullImmuneSystem) {
    /* WHAT: Test connection with NULL immune system */
    int result = curiosity_connect_immune(curiosity, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(CuriosityImmuneIntegrationTest, DisconnectImmune_Success) {
    /* WHAT: Test successful disconnect */
    curiosity_connect_immune(curiosity, immune);
    int result = curiosity_disconnect_immune(curiosity);
    EXPECT_EQ(result, 0);

    /* After disconnect, queries return defaults */
    float suppression = curiosity_get_immune_suppression(curiosity);
    EXPECT_FLOAT_EQ(suppression, 1.0f);
}

TEST_F(CuriosityImmuneIntegrationTest, DisconnectImmune_NotConnected) {
    /* WHAT: Test disconnect when not connected (should be safe) */
    int result = curiosity_disconnect_immune(curiosity);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Sickness Behavior Tests (Immune → Curiosity)
//=============================================================================

TEST_F(CuriosityImmuneIntegrationTest, SicknessBehavior_CytokineSuppression) {
    /* WHAT: Test cytokine release suppresses curiosity */

    /* Present antigen to trigger immune response */
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03, 0x04};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 8, 1, &antigen_id);

    /* Activate immune cells */
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);

    /* Release pro-inflammatory cytokine (IL-1) */
    uint32_t cytokine_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 0.8f, 0, &cytokine_id);

    /* Simulate cytokine callback using immune struct's cytokines array */
    curiosity_immune_on_cytokine_release(bridge, &immune->cytokines[0]);

    /* Update bridge to apply suppression */
    curiosity_immune_bridge_update(bridge, 100);

    /* Verify curiosity is suppressed */
    float suppression = curiosity_immune_get_suppression_factor(bridge);
    EXPECT_LT(suppression, 1.0f) << "Cytokine should suppress curiosity";
    EXPECT_GT(suppression, 0.0f) << "Suppression should not be complete";
}

TEST_F(CuriosityImmuneIntegrationTest, SicknessBehavior_InflammationSuppression) {
    /* WHAT: Test inflammation suppresses curiosity */

    /* Create inflammation site */
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 9, 1, &antigen_id);

    uint32_t site_id;
    brain_immune_initiate_inflammation(immune, 1, antigen_id, &site_id);

    /* Escalate to systemic inflammation */
    brain_immune_escalate_inflammation(immune, site_id);
    brain_immune_escalate_inflammation(immune, site_id);

    /* Verify inflammation is systemic */
    EXPECT_GE(immune->inflammation_sites[0].level, INFLAMMATION_SYSTEMIC);

    /* Simulate inflammation callback */
    curiosity_immune_on_inflammation(bridge, &immune->inflammation_sites[0]);

    /* Update bridge */
    curiosity_immune_bridge_update(bridge, 100);

    /* Verify strong suppression from systemic inflammation */
    float suppression = curiosity_immune_get_suppression_factor(bridge);
    EXPECT_LT(suppression, 0.7f) << "Systemic inflammation should strongly suppress";
}

TEST_F(CuriosityImmuneIntegrationTest, SicknessBehavior_Recovery) {
    /* WHAT: Test IL-10 aids curiosity recovery */

    /* First, induce sickness */
    uint32_t antigen_id, helper_id, cytokine_id;
    uint8_t epitope[] = {0x11, 0x22};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 7, 1, &antigen_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 0.9f, 0, &cytokine_id);

    curiosity_immune_on_cytokine_release(bridge, &immune->cytokines[0]);
    curiosity_immune_bridge_update(bridge, 100);

    float suppressed_level = curiosity_immune_get_suppression_factor(bridge);

    /* Now release IL-10 (anti-inflammatory) */
    uint32_t il10_id;
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL10, 0, 0.8f, 0, &il10_id);

    /* IL-10 should be anti-inflammatory */
    immune->cytokines[1].pro_inflammatory = false;

    curiosity_immune_on_cytokine_release(bridge, &immune->cytokines[1]);
    curiosity_immune_bridge_update(bridge, 100);

    /* Verify recovery (suppression should be less) */
    float recovered_level = curiosity_immune_get_suppression_factor(bridge);
    EXPECT_GT(recovered_level, suppressed_level) << "IL-10 should aid recovery";
}

TEST_F(CuriosityImmuneIntegrationTest, SicknessBehavior_ComputeSicknessLevel) {
    /* WHAT: Test sickness level computation from cytokines */

    /* Initially no sickness */
    float sickness = curiosity_immune_compute_sickness_level(bridge);
    EXPECT_FLOAT_EQ(sickness, 0.0f);

    /* Release multiple pro-inflammatory cytokines */
    uint32_t helper_id, cyt_id;
    uint8_t epitope[] = {0xFF};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 1, &antigen_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);

    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 0.6f, 0, &cyt_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, 0, 0.5f, 0, &cyt_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, 0, 0.4f, 0, &cyt_id);

    /* Recompute sickness */
    sickness = curiosity_immune_compute_sickness_level(bridge);
    EXPECT_GT(sickness, 0.0f) << "Multiple cytokines should increase sickness";
    EXPECT_LE(sickness, 1.0f) << "Sickness should be clamped to [0,1]";
}

//=============================================================================
// Novelty Vigilance Tests (Curiosity → Immune)
//=============================================================================

TEST_F(CuriosityImmuneIntegrationTest, NoveltyVigilance_HighCuriosityTriggers) {
    /* WHAT: Test high curiosity drive triggers immune vigilance */

    /* Set high curiosity baseline (simulating high novelty) */
    curiosity_set_baseline(curiosity, 0.85f);

    /* Update bridge to detect high curiosity */
    curiosity_immune_bridge_update(bridge, 100);

    /* Verify vigilance boost is active */
    float vigilance = curiosity_immune_get_vigilance_boost(bridge);
    EXPECT_GT(vigilance, 1.0f) << "High curiosity should boost immune vigilance";
    EXPECT_LE(vigilance, 1.5f) << "Vigilance boost should be capped";
}

TEST_F(CuriosityImmuneIntegrationTest, NoveltyVigilance_LowCuriosityNoBoost) {
    /* WHAT: Test low curiosity doesn't trigger vigilance */

    /* Set low curiosity */
    curiosity_set_baseline(curiosity, 0.2f);

    /* Update bridge */
    curiosity_immune_bridge_update(bridge, 100);

    /* Verify no vigilance boost */
    float vigilance = curiosity_immune_get_vigilance_boost(bridge);
    EXPECT_FLOAT_EQ(vigilance, 1.0f) << "Low curiosity should not boost vigilance";
}

TEST_F(CuriosityImmuneIntegrationTest, NoveltyVigilance_KnowledgeGapTrigger) {
    /* WHAT: Test knowledge gap triggers immune vigilance */

    /* Create large knowledge gap */
    knowledge_gap_t gap;
    memset(&gap, 0, sizeof(gap));
    strncpy(gap.topic, "unknown_threat", sizeof(gap.topic) - 1);
    gap.gap_size = 0.8f;
    gap.curiosity_intensity = 0.75f;
    gap.learning_potential = 0.9f;

    /* Trigger gap handler */
    int result = curiosity_immune_on_knowledge_gap(bridge, &gap);
    EXPECT_EQ(result, 0);

    /* Verify vigilance boost */
    float vigilance = curiosity_immune_get_vigilance_boost(bridge);
    EXPECT_GT(vigilance, 1.0f) << "Large knowledge gap should trigger vigilance";
}

TEST_F(CuriosityImmuneIntegrationTest, NoveltyVigilance_SmallGapNoTrigger) {
    /* WHAT: Test small knowledge gap doesn't trigger vigilance */

    /* Create small knowledge gap */
    knowledge_gap_t gap;
    memset(&gap, 0, sizeof(gap));
    strncpy(gap.topic, "familiar_topic", sizeof(gap.topic) - 1);
    gap.gap_size = 0.3f;
    gap.curiosity_intensity = 0.2f;

    /* Trigger gap handler */
    curiosity_immune_on_knowledge_gap(bridge, &gap);

    /* Verify no vigilance boost */
    float vigilance = curiosity_immune_get_vigilance_boost(bridge);
    EXPECT_FLOAT_EQ(vigilance, 1.0f) << "Small gap should not trigger vigilance";
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(CuriosityImmuneIntegrationTest, Config_DefaultValues) {
    /* WHAT: Test default configuration */
    curiosity_immune_config_t config;
    int result = curiosity_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(config.enable_sickness_behavior);
    EXPECT_TRUE(config.enable_inflammation_suppression);
    EXPECT_TRUE(config.enable_novelty_vigilance);
    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.novelty_sensitivity, 1.0f);
}

TEST_F(CuriosityImmuneIntegrationTest, Config_CustomSensitivity) {
    /* WHAT: Test custom sensitivity configuration */
    curiosity_immune_config_t config;
    curiosity_immune_default_config(&config);

    /* Increase cytokine sensitivity */
    config.cytokine_sensitivity = 2.0f;

    /* Create bridge with custom config */
    curiosity_immune_bridge_t* custom_bridge = curiosity_immune_bridge_create(
        &config, immune, curiosity
    );
    ASSERT_NE(custom_bridge, nullptr);

    /* Release cytokine */
    uint32_t antigen_id, helper_id, cyt_id;
    uint8_t epitope[] = {0xAA};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 5, 1, &antigen_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 0.5f, 0, &cyt_id);

    curiosity_immune_on_cytokine_release(custom_bridge, &immune->cytokines[0]);

    /* With 2x sensitivity, effect should be stronger */
    float sickness = curiosity_immune_get_sickness_level(custom_bridge);
    EXPECT_GT(sickness, 0.0f) << "Custom sensitivity should amplify effect";

    curiosity_immune_bridge_destroy(custom_bridge);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(CuriosityImmuneIntegrationTest, EdgeCase_NullBridgeOperations) {
    /* WHAT: Test NULL bridge safety */
    EXPECT_EQ(curiosity_immune_bridge_update(nullptr, 100), -1);
    EXPECT_FLOAT_EQ(curiosity_immune_get_sickness_level(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(curiosity_immune_get_suppression_factor(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(curiosity_immune_get_vigilance_boost(nullptr), 1.0f);
}

TEST_F(CuriosityImmuneIntegrationTest, EdgeCase_MaximumSuppression) {
    /* WHAT: Test maximum suppression during cytokine storm */

    /* Create cytokine storm (many high-concentration cytokines) */
    uint32_t antigen_id, helper_id, cyt_id;
    uint8_t epitope[] = {0xFF, 0xEE};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 10, 1, &antigen_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);

    /* Release many cytokines at high concentration */
    for (int i = 0; i < 5; i++) {
        brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 1.0f, 0, &cyt_id);
        brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL6, 0, 1.0f, 0, &cyt_id);
        brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_TNF, 0, 1.0f, 0, &cyt_id);
    }

    /* Trigger callbacks */
    for (size_t i = 0; i < immune->cytokine_count; i++) {
        curiosity_immune_on_cytokine_release(bridge, &immune->cytokines[i]);
    }

    curiosity_immune_bridge_update(bridge, 100);

    /* Verify suppression is capped at minimum (MAX_CURIOSITY_SUPPRESSION = 0.1) */
    float suppression = curiosity_immune_get_suppression_factor(bridge);
    EXPECT_GE(suppression, 0.1f) << "Suppression should be capped at minimum";
    EXPECT_LE(suppression, 1.0f);
}

TEST_F(CuriosityImmuneIntegrationTest, Integration_BidirectionalCoupling) {
    /* WHAT: Test bidirectional effects work together */

    /* Start with high curiosity (triggers vigilance) */
    curiosity_set_baseline(curiosity, 0.9f);
    curiosity_immune_bridge_update(bridge, 100);

    float initial_vigilance = curiosity_immune_get_vigilance_boost(bridge);
    EXPECT_GT(initial_vigilance, 1.0f) << "High curiosity should boost vigilance";

    /* Then trigger immune response (should suppress curiosity) */
    uint32_t antigen_id, helper_id, cyt_id;
    uint8_t epitope[] = {0x99};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 8, 1, &antigen_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_release_cytokine(immune, BRAIN_CYTOKINE_IL1, 0, 0.7f, 0, &cyt_id);

    curiosity_immune_on_cytokine_release(bridge, &immune->cytokines[0]);
    curiosity_immune_bridge_update(bridge, 100);

    float suppression = curiosity_immune_get_suppression_factor(bridge);
    EXPECT_LT(suppression, 1.0f) << "Cytokine should suppress curiosity";

    /* Verify both effects are present */
    float final_vigilance = curiosity_immune_get_vigilance_boost(bridge);
    EXPECT_GT(final_vigilance, 0.0f) << "Vigilance should still exist";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
