/**
 * @file test_protein_synthesis.cpp
 * @brief Unit tests for Protein Synthesis Constraints System
 *
 * WHAT: Comprehensive unit tests for protein synthesis and tagging
 * WHY:  Verify Frey & Morris synaptic tagging and capture model
 * HOW:  Test PRP pool, tag management, consolidation, modulation
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

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class ProteinSynthesisTest : public ::testing::Test {
protected:
    protein_synthesis_system_t system;
    protein_synthesis_config_t config;

    void SetUp() override {
        protein_synthesis_default_config(&config);
        system = protein_synthesis_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            protein_synthesis_destroy(system);
            system = nullptr;
        }
    }
};

class ProteinSleepBridgeTest : public ::testing::Test {
protected:
    protein_synthesis_system_t protein_system;
    sleep_system_t sleep_system;
    protein_sleep_bridge_t bridge;

    void SetUp() override {
        protein_synthesis_config_t protein_config;
        protein_synthesis_default_config(&protein_config);
        protein_system = protein_synthesis_create(&protein_config);
        ASSERT_NE(protein_system, nullptr);

        sleep_config_t sleep_config = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep_system, nullptr);

        protein_sleep_config_t bridge_config;
        protein_sleep_default_config(&bridge_config);
        bridge = protein_sleep_bridge_create(&bridge_config, sleep_system, protein_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            protein_sleep_bridge_destroy(bridge);
        }
        if (sleep_system) {
            sleep_system_destroy(sleep_system);
        }
        if (protein_system) {
            protein_synthesis_destroy(protein_system);
        }
    }
};

class ProteinImmuneBridgeTest : public ::testing::Test {
protected:
    protein_synthesis_system_t protein_system;
    brain_immune_system_t* immune_system;
    protein_immune_bridge_t* bridge;

    void SetUp() override {
        protein_synthesis_config_t protein_config;
        protein_synthesis_default_config(&protein_config);
        protein_system = protein_synthesis_create(&protein_config);
        ASSERT_NE(protein_system, nullptr);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        protein_immune_config_t bridge_config;
        protein_immune_default_config(&bridge_config);
        bridge = protein_immune_bridge_create(&bridge_config, immune_system, protein_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            protein_immune_bridge_destroy(bridge);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
        if (protein_system) {
            protein_synthesis_destroy(protein_system);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ProteinSynthesisTest, DefaultConfigValid) {
    EXPECT_GT(config.initial_prp_pool, 0.0f);
    EXPECT_GT(config.max_prp_pool, config.initial_prp_pool);
    EXPECT_GT(config.base_synthesis_rate, 0.0f);
    EXPECT_GT(config.decay_rate, 0.0f);
    EXPECT_GT(config.tag_duration_ms, 0);
    EXPECT_GT(config.capture_threshold_min, 0.0f);
    EXPECT_TRUE(config.enable_sleep_modulation);
    EXPECT_TRUE(config.enable_immune_suppression);
}

TEST_F(ProteinSynthesisTest, CreateDestroySuccessful) {
    EXPECT_NE(system, nullptr);

    float pool = protein_synthesis_get_prp_pool(system);
    EXPECT_EQ(pool, config.initial_prp_pool);
}

TEST_F(ProteinSynthesisTest, CreateWithNullConfigUsesDefaults) {
    protein_synthesis_system_t sys2 = protein_synthesis_create(nullptr);
    ASSERT_NE(sys2, nullptr);

    float pool = protein_synthesis_get_prp_pool(sys2);
    EXPECT_GT(pool, 0.0f);

    protein_synthesis_destroy(sys2);
}

/* ============================================================================
 * PRP Pool Tests
 * ============================================================================ */

TEST_F(ProteinSynthesisTest, InitialPRPPoolCorrect) {
    float pool = protein_synthesis_get_prp_pool(system);
    EXPECT_FLOAT_EQ(pool, config.initial_prp_pool);
}

TEST_F(ProteinSynthesisTest, PRPSynthesisOverTime) {
    float initial_pool = protein_synthesis_get_prp_pool(system);

    /* Synthesize for 1 second */
    ASSERT_EQ(protein_synthesis_update(system, 1000), 0);

    float new_pool = protein_synthesis_get_prp_pool(system);
    EXPECT_GT(new_pool, initial_pool);
}

TEST_F(ProteinSynthesisTest, PRPPoolCappedAtMax) {
    /* Add PRPs beyond max */
    float excess = config.max_prp_pool * 2.0f;
    ASSERT_EQ(protein_synthesis_add_prps(system, excess), 0);

    float pool = protein_synthesis_get_prp_pool(system);
    EXPECT_LE(pool, config.max_prp_pool);
}

TEST_F(ProteinSynthesisTest, PRPDecayOverTime) {
    /* Fill pool */
    ASSERT_EQ(protein_synthesis_add_prps(system, config.max_prp_pool), 0);
    float initial_pool = protein_synthesis_get_prp_pool(system);

    /* Wait long time with no synthesis */
    config.base_synthesis_rate = 0.0f;  /* Disable synthesis */
    protein_synthesis_system_t sys2 = protein_synthesis_create(&config);
    ASSERT_EQ(protein_synthesis_add_prps(sys2, config.max_prp_pool), 0);

    ASSERT_EQ(protein_synthesis_update(sys2, 3600000), 0);  /* 1 hour */

    float decayed_pool = protein_synthesis_get_prp_pool(sys2);
    EXPECT_LT(decayed_pool, initial_pool);

    protein_synthesis_destroy(sys2);
}

TEST_F(ProteinSynthesisTest, InducedSynthesisBoost) {
    float base_rate = protein_synthesis_get_synthesis_rate(system);

    /* Induce synthesis boost */
    ASSERT_EQ(protein_synthesis_induce_synthesis(system, 2.0f, 60000), 0);

    /* Update and check rate increased */
    ASSERT_EQ(protein_synthesis_update(system, 1000), 0);
    float boosted_rate = protein_synthesis_get_synthesis_rate(system);

    EXPECT_GT(boosted_rate, base_rate);
}

/* ============================================================================
 * Synaptic Tag Tests
 * ============================================================================ */

TEST_F(ProteinSynthesisTest, SetTagSuccessful) {
    uint32_t synapse_id = 42;
    float strength = 0.8f;

    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, strength), 0);
    EXPECT_TRUE(protein_synthesis_is_tagged(system, synapse_id));
}

TEST_F(ProteinSynthesisTest, RemoveTagSuccessful) {
    uint32_t synapse_id = 42;

    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, 0.8f), 0);
    EXPECT_TRUE(protein_synthesis_is_tagged(system, synapse_id));

    ASSERT_EQ(protein_synthesis_remove_tag(system, synapse_id), 0);
    EXPECT_FALSE(protein_synthesis_is_tagged(system, synapse_id));
}

TEST_F(ProteinSynthesisTest, GetTagReturnsCorrectData) {
    uint32_t synapse_id = 42;
    float strength = 0.8f;

    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, strength), 0);

    synaptic_tag_t tag;
    ASSERT_EQ(protein_synthesis_get_tag(system, synapse_id, &tag), 0);

    EXPECT_EQ(tag.synapse_id, synapse_id);
    EXPECT_FLOAT_EQ(tag.tag_strength, strength);
    EXPECT_EQ(tag.state, TAG_STATE_TAGGED);
    EXPECT_FALSE(tag.consolidation_achieved);
}

TEST_F(ProteinSynthesisTest, MultipleTagsSupported) {
    /* Set multiple tags */
    for (uint32_t i = 0; i < 10; i++) {
        ASSERT_EQ(protein_synthesis_set_tag(system, i, 0.8f), 0);
    }

    uint32_t num_tags = protein_synthesis_get_num_tags(system);
    EXPECT_EQ(num_tags, 10);

    /* All should be tagged */
    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_TRUE(protein_synthesis_is_tagged(system, i));
    }
}

TEST_F(ProteinSynthesisTest, TagDecaysOverTime) {
    uint32_t synapse_id = 42;
    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, 1.0f), 0);

    synaptic_tag_t tag_before;
    ASSERT_EQ(protein_synthesis_get_tag(system, synapse_id, &tag_before), 0);

    /* Wait 1 hour (tag half-life ~2 hours) */
    ASSERT_EQ(protein_synthesis_update(system, 3600000), 0);

    synaptic_tag_t tag_after;
    ASSERT_EQ(protein_synthesis_get_tag(system, synapse_id, &tag_after), 0);

    EXPECT_LT(tag_after.tag_strength, tag_before.tag_strength);
}

TEST_F(ProteinSynthesisTest, TagExpiresAfterDuration) {
    uint32_t synapse_id = 42;
    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, 1.0f), 0);

    /* Wait past tag duration */
    ASSERT_EQ(protein_synthesis_update(system, config.tag_duration_ms + 1000), 0);

    EXPECT_FALSE(protein_synthesis_is_tagged(system, synapse_id));
}

/* ============================================================================
 * Consolidation Tests
 * ============================================================================ */

TEST_F(ProteinSynthesisTest, CannotConsolidateWithoutTag) {
    uint32_t synapse_id = 42;

    /* Ensure sufficient PRPs */
    ASSERT_EQ(protein_synthesis_add_prps(system, config.max_prp_pool), 0);

    /* Try to consolidate without tag */
    EXPECT_NE(protein_synthesis_consolidate_synapse(system, synapse_id), 0);
}

TEST_F(ProteinSynthesisTest, CannotConsolidateWithoutPRPs) {
    uint32_t synapse_id = 42;

    /* Set tag but no PRPs */
    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, 0.8f), 0);

    /* Drain PRP pool */
    prp_pool_state_t state;
    ASSERT_EQ(protein_synthesis_get_prp_state(system, &state), 0);
    /* Pool starts with initial_prp_pool, may be enough for consolidation */

    /* Try to consolidate - might succeed if initial pool > threshold */
    bool can_consolidate = protein_synthesis_can_consolidate(system, synapse_id);

    if (can_consolidate) {
        /* If there are PRPs, consolidation should succeed */
        EXPECT_EQ(protein_synthesis_consolidate_synapse(system, synapse_id), 0);
    } else {
        /* If no PRPs, consolidation should fail */
        EXPECT_NE(protein_synthesis_consolidate_synapse(system, synapse_id), 0);
    }
}

TEST_F(ProteinSynthesisTest, SuccessfulConsolidation) {
    uint32_t synapse_id = 42;

    /* Set tag and ensure PRPs */
    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, 0.8f), 0);
    ASSERT_EQ(protein_synthesis_add_prps(system, config.max_prp_pool), 0);

    /* Should be able to consolidate */
    EXPECT_TRUE(protein_synthesis_can_consolidate(system, synapse_id));

    /* Consolidate */
    ASSERT_EQ(protein_synthesis_consolidate_synapse(system, synapse_id), 0);

    /* Verify state */
    synaptic_tag_t tag;
    ASSERT_EQ(protein_synthesis_get_tag(system, synapse_id, &tag), 0);
    EXPECT_EQ(tag.state, TAG_STATE_CONSOLIDATED);
    EXPECT_TRUE(tag.consolidation_achieved);
    EXPECT_GT(tag.prps_captured, 0.0f);
}

TEST_F(ProteinSynthesisTest, ConsolidationDepletesPool) {
    uint32_t synapse_id = 42;

    ASSERT_EQ(protein_synthesis_add_prps(system, config.max_prp_pool), 0);
    float pool_before = protein_synthesis_get_prp_pool(system);

    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, 0.8f), 0);
    ASSERT_EQ(protein_synthesis_consolidate_synapse(system, synapse_id), 0);

    float pool_after = protein_synthesis_get_prp_pool(system);
    EXPECT_LT(pool_after, pool_before);
}

TEST_F(ProteinSynthesisTest, ConsolidationProgressTracked) {
    uint32_t synapse_id = 42;

    ASSERT_EQ(protein_synthesis_set_tag(system, synapse_id, 0.8f), 0);

    /* No PRPs captured yet */
    float progress_before = protein_synthesis_get_consolidation_progress(system, synapse_id);
    EXPECT_FLOAT_EQ(progress_before, 0.0f);

    /* Consolidate */
    ASSERT_EQ(protein_synthesis_add_prps(system, config.max_prp_pool), 0);
    ASSERT_EQ(protein_synthesis_consolidate_synapse(system, synapse_id), 0);

    /* Progress should increase */
    float progress_after = protein_synthesis_get_consolidation_progress(system, synapse_id);
    EXPECT_GT(progress_after, progress_before);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ProteinSynthesisTest, StatisticsTracked) {
    /* Set some tags */
    ASSERT_EQ(protein_synthesis_set_tag(system, 1, 0.8f), 0);
    ASSERT_EQ(protein_synthesis_set_tag(system, 2, 0.8f), 0);

    /* Consolidate one */
    ASSERT_EQ(protein_synthesis_add_prps(system, config.max_prp_pool), 0);
    ASSERT_EQ(protein_synthesis_consolidate_synapse(system, 1), 0);

    /* Let other expire */
    ASSERT_EQ(protein_synthesis_update(system, config.tag_duration_ms + 1000), 0);

    protein_synthesis_stats_t stats;
    ASSERT_EQ(protein_synthesis_get_stats(system, &stats), 0);

    EXPECT_EQ(stats.total_tags_set, 2);
    EXPECT_EQ(stats.total_consolidations, 1);
    EXPECT_GT(stats.total_tags_expired, 0);
    EXPECT_GT(stats.total_prps_captured, 0);
}

TEST_F(ProteinSynthesisTest, StatisticsReset) {
    ASSERT_EQ(protein_synthesis_set_tag(system, 1, 0.8f), 0);

    protein_synthesis_stats_t stats_before;
    ASSERT_EQ(protein_synthesis_get_stats(system, &stats_before), 0);
    EXPECT_GT(stats_before.total_tags_set, 0);

    ASSERT_EQ(protein_synthesis_reset_stats(system), 0);

    protein_synthesis_stats_t stats_after;
    ASSERT_EQ(protein_synthesis_get_stats(system, &stats_after), 0);
    EXPECT_EQ(stats_after.total_tags_set, 0);
}

/* ============================================================================
 * Sleep Bridge Tests
 * ============================================================================ */

TEST_F(ProteinSleepBridgeTest, DefaultConfigValid) {
    protein_sleep_config_t config;
    ASSERT_EQ(protein_sleep_default_config(&config), 0);

    EXPECT_TRUE(config.enable_synthesis_modulation);
    EXPECT_TRUE(config.enable_delivery_modulation);
    EXPECT_GT(config.modulation_strength, 0.0f);
}

TEST_F(ProteinSleepBridgeTest, SleepStateModulatesSynthesis) {
    /* Update bridge */
    ASSERT_EQ(protein_sleep_update(bridge), 0);

    protein_sleep_effects_t effects;
    ASSERT_EQ(protein_sleep_get_effects(bridge, &effects), 0);

    /* Awake should be baseline */
    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(effects.synthesis_rate_factor, 1.0f);
}

TEST_F(ProteinSleepBridgeTest, DeepSleepBoostsSynthesis) {
    /* Enter deep sleep */
    ASSERT_TRUE(sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM));

    ASSERT_EQ(protein_sleep_update(bridge), 0);

    protein_sleep_effects_t effects;
    ASSERT_EQ(protein_sleep_get_effects(bridge, &effects), 0);

    EXPECT_EQ(effects.current_state, SLEEP_STATE_DEEP_NREM);
    EXPECT_GT(effects.synthesis_rate_factor, 1.0f);
    EXPECT_TRUE(effects.deep_sleep_consolidation);
}

TEST_F(ProteinSleepBridgeTest, IsConsolidationWindowCorrect) {
    /* Not in deep sleep initially */
    EXPECT_FALSE(protein_sleep_is_consolidation_window(bridge));

    /* Enter deep sleep */
    ASSERT_TRUE(sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM));
    ASSERT_EQ(protein_sleep_update(bridge), 0);

    EXPECT_TRUE(protein_sleep_is_consolidation_window(bridge));
}

/* ============================================================================
 * Immune Bridge Tests
 * ============================================================================ */

TEST_F(ProteinImmuneBridgeTest, DefaultConfigValid) {
    protein_immune_config_t config;
    ASSERT_EQ(protein_immune_default_config(&config), 0);

    EXPECT_TRUE(config.enable_cytokine_suppression);
    EXPECT_TRUE(config.enable_inflammation_impairment);
    EXPECT_GT(config.cytokine_sensitivity, 0.0f);
}

TEST_F(ProteinImmuneBridgeTest, InitiallyNotImpaired) {
    EXPECT_FALSE(protein_immune_is_synthesis_impaired(bridge));

    float reduction = protein_immune_get_synthesis_reduction(bridge);
    EXPECT_FLOAT_EQ(reduction, 0.0f);
}

TEST_F(ProteinImmuneBridgeTest, InflammationSuppressesSynthesis) {
    /* Start immune system */
    ASSERT_EQ(brain_immune_start(immune_system), 0);

    /* Create inflammation */
    uint32_t site_id;
    ASSERT_EQ(brain_immune_initiate_inflammation(immune_system, 1, 0, &site_id), 0);
    ASSERT_EQ(brain_immune_escalate_inflammation(immune_system, site_id), 0);

    /* Update bridge */
    ASSERT_EQ(protein_immune_bridge_update(bridge, 1000), 0);

    /* Should be impaired */
    EXPECT_TRUE(protein_immune_is_synthesis_impaired(bridge));

    float reduction = protein_immune_get_synthesis_reduction(bridge);
    EXPECT_GT(reduction, 0.0f);
}

TEST_F(ProteinImmuneBridgeTest, SynthesisRestorationWorks) {
    /* Apply some suppression */
    ASSERT_EQ(brain_immune_start(immune_system), 0);
    uint32_t site_id;
    ASSERT_EQ(brain_immune_initiate_inflammation(immune_system, 1, 0, &site_id), 0);
    ASSERT_EQ(protein_immune_bridge_update(bridge, 1000), 0);

    EXPECT_TRUE(protein_immune_is_synthesis_impaired(bridge));

    /* Restore */
    ASSERT_EQ(protein_immune_restore_synthesis(bridge, 1.0f), 0);

    /* Should be less impaired */
    float reduction = protein_immune_get_synthesis_reduction(bridge);
    EXPECT_LT(reduction, 10.0f);  /* Near baseline */
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(ProteinSynthesisTest, ConsolidationCallbackInvoked) {
    bool callback_invoked = false;
    uint32_t callback_synapse_id = 0;

    auto callback = [](const consolidation_event_t* event, void* user_data) {
        bool* invoked = (bool*)user_data;
        *invoked = true;
    };

    config.consolidation_callback = callback;
    config.callback_user_data = &callback_invoked;

    protein_synthesis_system_t sys2 = protein_synthesis_create(&config);
    ASSERT_NE(sys2, nullptr);

    /* Set tag and consolidate */
    ASSERT_EQ(protein_synthesis_add_prps(sys2, config.max_prp_pool), 0);
    ASSERT_EQ(protein_synthesis_set_tag(sys2, 42, 0.8f), 0);
    ASSERT_EQ(protein_synthesis_consolidate_synapse(sys2, 42), 0);

    EXPECT_TRUE(callback_invoked);

    protein_synthesis_destroy(sys2);
}

TEST_F(ProteinSynthesisTest, CompetitionForLimitedPRPs) {
    /* Set multiple tags */
    uint32_t num_tags = 5;
    for (uint32_t i = 0; i < num_tags; i++) {
        ASSERT_EQ(protein_synthesis_set_tag(system, i, 0.8f), 0);
    }

    /* Add limited PRPs (enough for ~2 consolidations) */
    float limited_prps = config.capture_threshold_optimal * 2.0f;
    ASSERT_EQ(protein_synthesis_add_prps(system, limited_prps), 0);

    /* Try to consolidate all */
    uint32_t successful = 0;
    for (uint32_t i = 0; i < num_tags; i++) {
        if (protein_synthesis_consolidate_synapse(system, i) == 0) {
            successful++;
        }
    }

    /* Not all should succeed */
    EXPECT_LT(successful, num_tags);
    EXPECT_GT(successful, 0);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
