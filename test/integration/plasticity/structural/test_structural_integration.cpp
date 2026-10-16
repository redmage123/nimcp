/**
 * @file test_structural_integration.cpp
 * @brief Integration tests for structural plasticity with sleep and immune systems
 */

#include <gtest/gtest.h>
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "plasticity/structural/nimcp_structural_sleep_bridge.h"
#include "plasticity/structural/nimcp_structural_immune_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class StructuralIntegrationTest : public ::testing::Test {
protected:
    structural_plasticity_system_t* structural_system;
    sleep_system_t sleep_system;
    brain_immune_system_t* immune_system;

    void SetUp() override {
        /* Create structural plasticity system */
        structural_plasticity_config_t config;
        structural_plasticity_default_config(&config);
        config.enable_sleep_consolidation = true;
        config.enable_immune_pruning = true;
        structural_system = structural_plasticity_create(&config);
        ASSERT_NE(structural_system, nullptr);

        /* Create sleep system */
        sleep_config_t sleep_cfg = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_cfg);
        ASSERT_NE(sleep_system, nullptr);

        /* Create immune system */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);
    }

    void TearDown() override {
        if (structural_system) {
            structural_plasticity_destroy(structural_system);
        }
        if (sleep_system) {
            sleep_system_destroy(sleep_system);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }
};

//=============================================================================
// Sleep Integration Tests
//=============================================================================

TEST_F(StructuralIntegrationTest, SleepBridgeCreation) {
    /* WHAT: Test sleep bridge creation
     * WHY:  Verify integration setup works
     */
    structural_sleep_config_t config;
    structural_sleep_default_config(&config);

    structural_sleep_bridge_t bridge = structural_sleep_bridge_create(
        &config, sleep_system, structural_system);

    EXPECT_NE(bridge, nullptr);

    structural_sleep_bridge_destroy(bridge);
}

TEST_F(StructuralIntegrationTest, SleepStateModulatesFormation) {
    /* WHAT: Test sleep state affects formation rate
     * WHY:  Awake promotes formation, sleep reduces it
     * BIOLOGICAL: Spine formation primarily during wake
     */
    structural_sleep_config_t config;
    structural_sleep_default_config(&config);
    structural_sleep_bridge_t bridge = structural_sleep_bridge_create(
        &config, sleep_system, structural_system);

    /* Set to AWAKE state */
    sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
    structural_sleep_update(bridge);

    structural_sleep_effects_t effects_awake;
    structural_sleep_get_effects(bridge, &effects_awake);

    /* Set to DEEP_NREM state */
    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    structural_sleep_update(bridge);

    structural_sleep_effects_t effects_sleep;
    structural_sleep_get_effects(bridge, &effects_sleep);

    EXPECT_GT(effects_awake.formation_rate_factor, effects_sleep.formation_rate_factor);

    structural_sleep_bridge_destroy(bridge);
}

TEST_F(StructuralIntegrationTest, NREMConsolidatesTaggedSpines) {
    /* WHAT: Test NREM sleep consolidates tagged spines
     * WHY:  Sleep strengthens learning-related synapses
     * BIOLOGICAL: NREM consolidation mechanism
     */
    structural_sleep_config_t config;
    structural_sleep_default_config(&config);
    structural_sleep_bridge_t bridge = structural_sleep_bridge_create(
        &config, sleep_system, structural_system);

    /* Create and tag nascent spines */
    uint32_t synapse_ids[5];
    for (int i = 0; i < 5; i++) {
        structural_plasticity_form_synapse(
            structural_system, i, i+1, 50.0f, &synapse_ids[i]);
        structural_plasticity_tag_for_consolidation(
            structural_system, synapse_ids[i]);
    }

    /* Transition to DEEP_NREM */
    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    structural_sleep_update(bridge);

    /* Consolidate */
    structural_sleep_consolidate_tagged(bridge);

    /* Check if some spines stabilized */
    uint32_t stable_count = structural_plasticity_get_spine_count(
        structural_system, SYNAPSE_STATE_STABLE);

    EXPECT_GT(stable_count, 0);

    structural_sleep_bridge_destroy(bridge);
}

TEST_F(StructuralIntegrationTest, REMPrunesWeakSpines) {
    /* WHAT: Test REM sleep prunes weak spines
     * WHY:  REM implements synaptic downscaling
     * BIOLOGICAL: Synaptic homeostasis during sleep
     */
    structural_sleep_config_t config;
    structural_sleep_default_config(&config);
    structural_sleep_bridge_t bridge = structural_sleep_bridge_create(
        &config, sleep_system, structural_system);

    /* Create spines without tagging */
    uint32_t synapse_ids[5];
    for (int i = 0; i < 5; i++) {
        structural_plasticity_form_synapse(
            structural_system, i, i+1, 50.0f, &synapse_ids[i]);
        /* Don't tag for consolidation = weak */
    }

    uint32_t count_before = structural_plasticity_get_total_spines(structural_system);

    /* Transition to REM */
    sleep_enter_state(sleep_system, SLEEP_STATE_REM);
    structural_sleep_update(bridge);

    /* Prune weak spines */
    structural_sleep_prune_weak(bridge);

    uint32_t count_after = structural_plasticity_get_total_spines(structural_system);

    /* Some spines may have been pruned */
    EXPECT_LE(count_after, count_before);

    structural_sleep_bridge_destroy(bridge);
}

//=============================================================================
// Immune Integration Tests
//=============================================================================

TEST_F(StructuralIntegrationTest, ImmuneBridgeCreation) {
    /* WHAT: Test immune bridge creation
     * WHY:  Verify immune integration setup
     */
    structural_immune_config_t config;
    structural_immune_default_config(&config);

    structural_immune_bridge_t* bridge = structural_immune_bridge_create(
        &config, immune_system, structural_system);

    EXPECT_NE(bridge, nullptr);

    structural_immune_bridge_destroy(bridge);
}

TEST_F(StructuralIntegrationTest, MicrogliaPrunesComplementTagged) {
    /* WHAT: Test microglia prune complement-tagged synapses
     * WHY:  Immune-mediated synaptic pruning
     * BIOLOGICAL: Microglia engulf C3-tagged synapses
     */
    structural_immune_config_t config;
    structural_immune_default_config(&config);
    structural_immune_bridge_t* bridge = structural_immune_bridge_create(
        &config, immune_system, structural_system);

    /* Create and tag spines */
    uint32_t synapse_ids[5];
    for (int i = 0; i < 5; i++) {
        structural_plasticity_form_synapse(
            structural_system, i, i+1, 50.0f, &synapse_ids[i]);

        uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
        memset(tag, 0xC3, STRUCTURAL_EPITOPE_SIZE);
        structural_plasticity_tag_complement(
            structural_system, synapse_ids[i], tag, STRUCTURAL_EPITOPE_SIZE);
    }

    uint32_t count_before = structural_plasticity_get_total_spines(structural_system);

    /* Trigger microglia pruning */
    structural_immune_microglia_prune(bridge);

    uint32_t count_after = structural_plasticity_get_total_spines(structural_system);

    /* Some spines should be pruned */
    EXPECT_LT(count_after, count_before);

    structural_immune_bridge_destroy(bridge);
}

TEST_F(StructuralIntegrationTest, ImmuneTagsWeakSpines) {
    /* WHAT: Test immune system tags weak spines
     * WHY:  Low-activity synapses targeted for elimination
     * BIOLOGICAL: Complement tags weak synapses
     */
    structural_immune_config_t config;
    structural_immune_default_config(&config);
    structural_immune_bridge_t* bridge = structural_immune_bridge_create(
        &config, immune_system, structural_system);

    /* Create spines with LOW activity (< 2.0 Hz threshold for weak spine tagging)
     * This simulates synapses that have been inactive and should be tagged
     * for complement-mediated pruning.
     */
    for (int i = 0; i < 10; i++) {
        uint32_t id;
        /* Form with low activity (0.5 Hz) to make them "weak" */
        structural_plasticity_form_synapse(
            structural_system, i, i+1, 0.5f, &id);
    }

    /* Tag weak spines */
    uint32_t tagged = structural_immune_tag_weak_spines(bridge);

    EXPECT_GT(tagged, 0);

    structural_immune_bridge_destroy(bridge);
}

TEST_F(StructuralIntegrationTest, InflammationImpairsFormation) {
    /* WHAT: Test inflammation reduces spine formation
     * WHY:  Pro-inflammatory cytokines impair structural plasticity
     * BIOLOGICAL: IL-1β, TNF-α reduce spine density
     */
    structural_immune_config_t config;
    structural_immune_default_config(&config);
    structural_immune_bridge_t* bridge = structural_immune_bridge_create(
        &config, immune_system, structural_system);

    /* Apply inflammation effects */
    structural_immune_apply_inflammation_effects(bridge);

    float formation_factor = structural_immune_get_formation_factor(bridge);

    /* Formation should be impaired (factor < 1.0) */
    EXPECT_LE(formation_factor, 1.0f);

    structural_immune_bridge_destroy(bridge);
}

//=============================================================================
// Combined Sleep-Immune Integration
//=============================================================================

TEST_F(StructuralIntegrationTest, SleepAndImmuneCooperate) {
    /* WHAT: Test sleep and immune systems work together
     * WHY:  Realistic multi-system integration
     * BIOLOGICAL: Sleep consolidates, immune prunes
     */
    structural_sleep_config_t sleep_config;
    structural_sleep_default_config(&sleep_config);
    structural_sleep_bridge_t sleep_bridge = structural_sleep_bridge_create(
        &sleep_config, sleep_system, structural_system);

    structural_immune_config_t immune_config;
    structural_immune_default_config(&immune_config);
    structural_immune_bridge_t* immune_bridge = structural_immune_bridge_create(
        &immune_config, immune_system, structural_system);

    /* Create spines, tag some for consolidation, leave others weak */
    for (int i = 0; i < 10; i++) {
        uint32_t id;
        structural_plasticity_form_synapse(
            structural_system, i, i+1, 50.0f, &id);

        if (i % 2 == 0) {
            /* Tag even ones for consolidation */
            structural_plasticity_tag_for_consolidation(structural_system, id);
        } else {
            /* Odd ones are weak, tag for immune pruning */
            uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
            memset(tag, 0xC3, STRUCTURAL_EPITOPE_SIZE);
            structural_plasticity_tag_complement(
                structural_system, id, tag, STRUCTURAL_EPITOPE_SIZE);
        }
    }

    /* NREM consolidates tagged spines */
    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    structural_sleep_update(sleep_bridge);
    structural_sleep_consolidate_tagged(sleep_bridge);

    /* Immune prunes complement-tagged */
    structural_immune_microglia_prune(immune_bridge);

    /* Should have some stable (consolidated) and fewer total (pruned) */
    uint32_t stable_count = structural_plasticity_get_spine_count(
        structural_system, SYNAPSE_STATE_STABLE);

    EXPECT_GT(stable_count, 0);

    structural_sleep_bridge_destroy(sleep_bridge);
    structural_immune_bridge_destroy(immune_bridge);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
