/**
 * @file test_protein_integration.cpp
 * @brief Integration tests for Protein Synthesis with Sleep and Immune
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
    #include "plasticity/protein/nimcp_protein_synthesis.h"
    #include "plasticity/protein/nimcp_protein_sleep_bridge.h"
    #include "plasticity/protein/nimcp_protein_immune_bridge.h"
    #include "cognitive/nimcp_sleep_wake.h"
    #include "cognitive/immune/nimcp_brain_immune.h"

class ProteinIntegrationTest : public ::testing::Test {
protected:
    protein_synthesis_system_t protein_system;
    sleep_system_t sleep_system;
    brain_immune_system_t* immune_system;
    protein_sleep_bridge_t sleep_bridge;
    protein_immune_bridge_t* immune_bridge;

    void SetUp() override {
        /* Create systems */
        protein_synthesis_config_t protein_config;
        protein_synthesis_default_config(&protein_config);
        protein_system = protein_synthesis_create(&protein_config);
        ASSERT_NE(protein_system, nullptr);

        sleep_config_t sleep_config = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep_system, nullptr);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Create bridges */
        protein_sleep_config_t sleep_bridge_config;
        protein_sleep_default_config(&sleep_bridge_config);
        sleep_bridge = protein_sleep_bridge_create(&sleep_bridge_config, sleep_system, protein_system);
        ASSERT_NE(sleep_bridge, nullptr);

        protein_immune_config_t immune_bridge_config;
        protein_immune_default_config(&immune_bridge_config);
        immune_bridge = protein_immune_bridge_create(&immune_bridge_config, immune_system, protein_system);
        ASSERT_NE(immune_bridge, nullptr);
    }

    void TearDown() override {
        if (immune_bridge) protein_immune_bridge_destroy(immune_bridge);
        if (sleep_bridge) protein_sleep_bridge_destroy(sleep_bridge);
        if (immune_system) brain_immune_destroy(immune_system);
        if (sleep_system) sleep_system_destroy(sleep_system);
        if (protein_system) protein_synthesis_destroy(protein_system);
    }
};

TEST_F(ProteinIntegrationTest, DeepSleepEnhancesConsolidation) {
    /* Set tag while awake */
    ASSERT_EQ(protein_synthesis_set_tag(protein_system, 1, 0.8f), 0);

    /* Enter deep sleep */
    ASSERT_TRUE(sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM));
    ASSERT_EQ(protein_sleep_update(sleep_bridge), 0);

    /* Synthesize during deep sleep */
    ASSERT_EQ(protein_synthesis_update(protein_system, 10000), 0);

    /* Pool should increase more than during wake */
    float pool = protein_synthesis_get_prp_pool(protein_system);
    EXPECT_GT(pool, 100.0f);

    /* Should be consolidation window */
    EXPECT_TRUE(protein_sleep_is_consolidation_window(sleep_bridge));
}

TEST_F(ProteinIntegrationTest, InflammationBlocksConsolidation) {
    /* Set tag */
    ASSERT_EQ(protein_synthesis_set_tag(protein_system, 1, 0.8f), 0);
    ASSERT_EQ(protein_synthesis_add_prps(protein_system, 500.0f), 0);

    /* Start immune and create inflammation */
    ASSERT_EQ(brain_immune_start(immune_system), 0);
    uint32_t site_id;
    ASSERT_EQ(brain_immune_initiate_inflammation(immune_system, 1, 0, &site_id), 0);
    ASSERT_EQ(brain_immune_escalate_inflammation(immune_system, site_id), 0);

    /* Update immune bridge */
    ASSERT_EQ(protein_immune_bridge_update(immune_bridge, 1000), 0);

    /* Synthesis should be impaired */
    EXPECT_TRUE(protein_immune_is_synthesis_impaired(immune_bridge));
}

TEST_F(ProteinIntegrationTest, SleepAndImmuneEffectsCombine) {
    /* Deep sleep should boost */
    ASSERT_TRUE(sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM));
    ASSERT_EQ(protein_sleep_update(sleep_bridge), 0);

    protein_sleep_effects_t sleep_effects;
    ASSERT_EQ(protein_sleep_get_effects(sleep_bridge, &sleep_effects), 0);
    EXPECT_GT(sleep_effects.synthesis_rate_factor, 1.0f);

    /* But inflammation should suppress */
    ASSERT_EQ(brain_immune_start(immune_system), 0);
    uint32_t site_id;
    ASSERT_EQ(brain_immune_initiate_inflammation(immune_system, 1, 0, &site_id), 0);
    ASSERT_EQ(protein_immune_bridge_update(immune_bridge, 1000), 0);

    EXPECT_TRUE(protein_immune_is_synthesis_impaired(immune_bridge));

    /* Net effect should be suppressed despite sleep */
    float reduction = protein_immune_get_synthesis_reduction(immune_bridge);
    EXPECT_GT(reduction, 0.0f);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
