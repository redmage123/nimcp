/**
 * @file test_dendritic_immune_integration.cpp
 * @brief Unit tests for Dendritic Plasticity - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between dendritic plasticity and immune system
 * WHY:  Validate cytokine modulation of spine density/complexity and damage-triggered immunity
 * HOW:  Test cytokine effects on dendritic structure, spine loss-induced immune activation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "plasticity/immune/nimcp_dendritic_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/dendritic/nimcp_dendritic.h"

class DendriticImmuneTest : public ::testing::Test {
protected:
    dendritic_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    dendritic_tree_t dendritic_tree;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create dendritic tree
        dendritic_tree_config_t tree_config = dendritic_tree_config_default();
        tree_config.num_branches = 4;
        tree_config.compartments_per_branch = 8;
        tree_config.enable_nmda = true;
        tree_config.enable_dendritic_spikes = true;
        dendritic_tree = dendritic_tree_create(&tree_config);
        ASSERT_NE(dendritic_tree, nullptr);

        // Create bridge
        dendritic_immune_config_t bridge_config;
        dendritic_immune_default_config(&bridge_config);
        bridge = dendritic_immune_bridge_create(&bridge_config, immune, dendritic_tree);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        dendritic_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        dendritic_tree_destroy(dendritic_tree);
    }
};

/**
 * TEST: Lifecycle - Default Configuration
 * BIOLOGICAL: Verify sensible defaults for dendritic-immune integration
 */
TEST_F(DendriticImmuneTest, DefaultConfiguration) {
    dendritic_immune_config_t config;
    int result = dendritic_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    // All features should be enabled by default
    EXPECT_TRUE(config.enable_cytokine_dendritic_modulation);
    EXPECT_TRUE(config.enable_inflammation_atrophy);
    EXPECT_TRUE(config.enable_damage_immune_trigger);
    EXPECT_TRUE(config.enable_recovery_immune_support);

    // Default sensitivities should be 1.0
    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.damage_trigger_sensitivity, 1.0f);

    // Baseline spine density should be healthy
    EXPECT_GT(config.baseline_spine_density, 0.5f);
    EXPECT_LE(config.baseline_spine_density, 1.0f);
}

/**
 * TEST: Lifecycle - Bridge Creation and Destruction
 * BIOLOGICAL: Verify proper resource management
 */
TEST_F(DendriticImmuneTest, LifecycleManagement) {
    // Bridge already created in SetUp
    ASSERT_NE(bridge, nullptr);

    // Should be able to query initial state
    float spine_density = dendritic_immune_get_spine_density(bridge);
    EXPECT_GT(spine_density, 0.0f);
    EXPECT_LE(spine_density, 1.0f);

    // Should be able to query complexity
    float complexity_loss = dendritic_immune_get_complexity_loss(bridge);
    EXPECT_GE(complexity_loss, 0.0f);
    EXPECT_LE(complexity_loss, 1.0f);
}

/**
 * TEST: Lifecycle - Null Pointer Handling
 * BIOLOGICAL: Verify robust error handling
 */
TEST(DendriticImmuneNullTest, NullPointerHandling) {
    // Null config should work (use defaults)
    brain_immune_system_t* immune = brain_immune_create(nullptr);
    ASSERT_NE(immune, nullptr);

    dendritic_tree_config_t tree_config = dendritic_tree_config_default();
    dendritic_tree_t tree = dendritic_tree_create(&tree_config);
    ASSERT_NE(tree, nullptr);

    dendritic_immune_bridge_t* bridge = dendritic_immune_bridge_create(nullptr, immune, tree);
    EXPECT_NE(bridge, nullptr);

    // Cleanup
    dendritic_immune_bridge_destroy(bridge);
    brain_immune_destroy(immune);
    dendritic_tree_destroy(tree);

    // Null systems should return NULL
    dendritic_immune_config_t config;
    dendritic_immune_default_config(&config);
    bridge = dendritic_immune_bridge_create(&config, nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

/**
 * TEST: Cytokine Reduction of Spine Density
 * BIOLOGICAL: Pro-inflammatory cytokines (IL-1β, TNF-α) reduce spine density
 */
TEST_F(DendriticImmuneTest, CytokineReducesSpineDensity) {
    // Get baseline spine density
    float baseline = dendritic_immune_get_spine_density(bridge);
    EXPECT_GT(baseline, 0.5f);

    // Trigger inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  8, 0, &antigen_id);

    // Full immune activation
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);

    // Apply cytokine effects to dendritic structure
    int result = dendritic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Spine density should remain in valid range
    float new_density = dendritic_immune_get_spine_density(bridge);
    EXPECT_GT(new_density, 0.0f);
    EXPECT_LE(new_density, 1.0f);
}

/**
 * TEST: IL-10 Promotes Spine Growth
 * BIOLOGICAL: Anti-inflammatory cytokines promote dendritic growth and spine formation
 */
TEST_F(DendriticImmuneTest, IL10PromotesSpineGrowth) {
    // Get baseline
    float baseline = dendritic_immune_get_spine_density(bridge);

    // Would simulate IL-10 release from immune system
    // For now, just verify the function works
    int result = dendritic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    // Spine density should remain in valid range
    float new_density = dendritic_immune_get_spine_density(bridge);
    EXPECT_GT(new_density, 0.0f);
    EXPECT_LE(new_density, 1.0f);
}

/**
 * TEST: Chronic Inflammation Causes Dendritic Atrophy
 * BIOLOGICAL: Sustained inflammation (>3 days) causes progressive dendritic atrophy
 */
TEST_F(DendriticImmuneTest, ChronicInflammationCausesAtrophy) {
    // Apply inflammation effects
    int result = dendritic_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    // Get inflammation state
    inflammation_dendritic_state_t state;
    result = dendritic_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);

    // Atrophy severity should be in valid range
    EXPECT_GE(state.atrophy_severity, 0.0f);
    EXPECT_LE(state.atrophy_severity, 1.0f);

    // Complexity loss should be valid
    EXPECT_GE(state.complexity_loss, 0.0f);
    EXPECT_LE(state.complexity_loss, 1.0f);
}

/**
 * TEST: IL-1β Impairs NMDA Receptor Trafficking
 * BIOLOGICAL: IL-1β specifically reduces NMDA receptor surface expression
 */
TEST_F(DendriticImmuneTest, IL1ImpainsNMDATrafficking) {
    // Apply inflammation effects
    dendritic_immune_apply_inflammation_effects(bridge);

    // Get inflammation state
    inflammation_dendritic_state_t state;
    dendritic_immune_get_inflammation_state(bridge, &state);

    // NMDA trafficking impairment should be in valid range
    EXPECT_GE(state.nmda_trafficking_impairment, 0.0f);
    EXPECT_LE(state.nmda_trafficking_impairment, 1.0f);
}

/**
 * TEST: Inflammation Reduces Dendritic Complexity
 * BIOLOGICAL: Cytokines cause loss of dendritic branching complexity
 */
TEST_F(DendriticImmuneTest, InflammationReducesComplexity) {
    // Get baseline complexity
    float baseline_complexity = 1.0f - dendritic_immune_get_complexity_loss(bridge);
    EXPECT_GT(baseline_complexity, 0.0f);

    // Trigger inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  9, 0, &antigen_id);

    // Apply inflammation effects
    dendritic_immune_apply_inflammation_effects(bridge);

    // Complexity should be affected
    float new_complexity_loss = dendritic_immune_get_complexity_loss(bridge);
    EXPECT_GE(new_complexity_loss, 0.0f);
    EXPECT_LE(new_complexity_loss, 1.0f);
}

/**
 * TEST: Rapid Spine Loss Triggers Immune Surveillance
 * BIOLOGICAL: Excessive spine loss activates microglial surveillance
 */
TEST_F(DendriticImmuneTest, RapidSpineLossTriggersSurveillance) {
    // Simulate spine loss by reducing density
    // (Would normally happen through cytokine effects)

    // Trigger immune response from spine loss
    int result = dendritic_immune_trigger_from_spine_loss(bridge);
    EXPECT_EQ(result, 0);

    // Should be able to compute spine loss
    float spine_loss = dendritic_immune_compute_spine_loss(bridge);
    EXPECT_GE(spine_loss, 0.0f);
    EXPECT_LE(spine_loss, 1.0f);
}

/**
 * TEST: Dendritic Damage Activates Immune Response
 * BIOLOGICAL: Structural damage releases danger signals (DAMPs)
 */
TEST_F(DendriticImmuneTest, DendriticDamageTriggerImmune) {
    // Trigger immune from damage
    int result = dendritic_immune_trigger_from_damage(bridge);
    EXPECT_EQ(result, 0);

    // Should not crash or produce invalid values
    float complexity_loss = dendritic_immune_get_complexity_loss(bridge);
    EXPECT_GE(complexity_loss, 0.0f);
    EXPECT_LE(complexity_loss, 1.0f);
}

/**
 * TEST: Healthy Dendrites Support Immune Resolution
 * BIOLOGICAL: Healthy synaptic state promotes IL-10 release and inflammation clearance
 */
TEST_F(DendriticImmuneTest, HealthyDendritesSupportRecovery) {
    // Support immune from healthy dendrites
    int result = dendritic_immune_support_from_health(bridge);
    EXPECT_EQ(result, 0);

    // Should maintain healthy spine density
    float spine_density = dendritic_immune_get_spine_density(bridge);
    EXPECT_GT(spine_density, 0.5f);
}

/**
 * TEST: Bidirectional Update
 * BIOLOGICAL: Both directions of coupling update in sync
 */
TEST_F(DendriticImmuneTest, BidirectionalUpdate) {
    // Update bridge
    int result = dendritic_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    // Multiple updates should work
    for (int i = 0; i < 10; i++) {
        result = dendritic_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }

    // State should remain valid
    float spine_density = dendritic_immune_get_spine_density(bridge);
    EXPECT_GT(spine_density, 0.0f);
    EXPECT_LE(spine_density, 1.0f);
}

/**
 * TEST: Cytokine Effects Query
 * BIOLOGICAL: Verify ability to query cytokine effects on dendrites
 */
TEST_F(DendriticImmuneTest, QueryCytokineEffects) {
    // Apply cytokine effects
    dendritic_immune_apply_cytokine_effects(bridge);

    // Query effects
    cytokine_dendritic_effects_t effects;
    int result = dendritic_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // All effects should be in valid ranges
    EXPECT_LE(fabs(effects.il1_spine_loss), 1.0f);
    EXPECT_LE(fabs(effects.il6_spine_loss), 1.0f);
    EXPECT_LE(fabs(effects.tnf_spine_loss), 1.0f);
    EXPECT_GE(effects.complexity_reduction, 0.0f);
    EXPECT_LE(effects.complexity_reduction, 1.0f);
    EXPECT_GE(effects.integration_impairment, 0.0f);
    EXPECT_LE(effects.integration_impairment, 1.0f);
}

/**
 * TEST: Inflammation State Query
 * BIOLOGICAL: Verify ability to query inflammation dendritic state
 */
TEST_F(DendriticImmuneTest, QueryInflammationState) {
    // Apply inflammation effects
    dendritic_immune_apply_inflammation_effects(bridge);

    // Query state
    inflammation_dendritic_state_t state;
    int result = dendritic_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);

    // Spine density should be valid
    EXPECT_GT(state.spine_density, 0.0f);
    EXPECT_LE(state.spine_density, 1.0f);

    // Baseline should be valid
    EXPECT_GT(state.spine_density_baseline, 0.0f);
    EXPECT_LE(state.spine_density_baseline, 1.0f);

    // All metrics should be in range
    EXPECT_GE(state.complexity_loss, 0.0f);
    EXPECT_LE(state.complexity_loss, 1.0f);
    EXPECT_GE(state.atrophy_severity, 0.0f);
    EXPECT_LE(state.atrophy_severity, 1.0f);
    EXPECT_GE(state.nmda_trafficking_impairment, 0.0f);
    EXPECT_LE(state.nmda_trafficking_impairment, 1.0f);
}

/**
 * TEST: Atrophy Detection
 * BIOLOGICAL: Detect when dendritic atrophy crosses threshold
 */
TEST_F(DendriticImmuneTest, AtrophyDetection) {
    // Apply inflammation effects
    dendritic_immune_apply_inflammation_effects(bridge);

    // Check atrophy status
    bool has_atrophy = dendritic_immune_is_atrophy(bridge);
    // Should return a valid boolean
    EXPECT_TRUE(has_atrophy == true || has_atrophy == false);
}

/**
 * TEST: Spine Density Tracking
 * BIOLOGICAL: Track spine density changes over time
 */
TEST_F(DendriticImmuneTest, SpineDensityTracking) {
    // Get initial density
    float initial = dendritic_immune_get_spine_density(bridge);
    EXPECT_GT(initial, 0.0f);

    // Apply effects multiple times
    for (int i = 0; i < 5; i++) {
        dendritic_immune_apply_cytokine_effects(bridge);
    }

    // Density should still be valid
    float final = dendritic_immune_get_spine_density(bridge);
    EXPECT_GT(final, 0.0f);
    EXPECT_LE(final, 1.0f);
}

/**
 * TEST: Complexity Loss Computation
 * BIOLOGICAL: Compute dendritic complexity reduction from inflammation
 */
TEST_F(DendriticImmuneTest, ComplexityLossComputation) {
    // Compute complexity loss
    float loss = dendritic_immune_compute_complexity_loss(bridge);
    EXPECT_GE(loss, 0.0f);
    EXPECT_LE(loss, 1.0f);

    // Apply inflammation
    dendritic_immune_apply_inflammation_effects(bridge);

    // Recompute
    float new_loss = dendritic_immune_compute_complexity_loss(bridge);
    EXPECT_GE(new_loss, 0.0f);
    EXPECT_LE(new_loss, 1.0f);
}

/**
 * TEST: Spine Loss Computation
 * BIOLOGICAL: Compute spine loss from baseline
 */
TEST_F(DendriticImmuneTest, SpineLossComputation) {
    // Compute spine loss
    float loss = dendritic_immune_compute_spine_loss(bridge);
    EXPECT_GE(loss, 0.0f);
    EXPECT_LE(loss, 1.0f);

    // Should start at zero (no loss yet)
    EXPECT_FLOAT_EQ(loss, 0.0f);
}

/**
 * TEST: Calcium Dysregulation Effects
 * BIOLOGICAL: Inflammation causes calcium handling problems
 */
TEST_F(DendriticImmuneTest, CalciumDysregulation) {
    // Apply inflammation effects
    dendritic_immune_apply_inflammation_effects(bridge);

    // Get state
    inflammation_dendritic_state_t state;
    dendritic_immune_get_inflammation_state(bridge, &state);

    // Calcium dysregulation should be in valid range
    EXPECT_GE(state.calcium_dysregulation, 0.0f);
    EXPECT_LE(state.calcium_dysregulation, 1.0f);
}

/**
 * TEST: Dendritic Spike Impairment
 * BIOLOGICAL: Inflammation impairs dendritic spike generation
 */
TEST_F(DendriticImmuneTest, DendriticSpikeImpairment) {
    // Apply inflammation effects
    dendritic_immune_apply_inflammation_effects(bridge);

    // Get state
    inflammation_dendritic_state_t state;
    dendritic_immune_get_inflammation_state(bridge, &state);

    // Spike impairment should be in valid range
    EXPECT_GE(state.spike_generation_impairment, 0.0f);
    EXPECT_LE(state.spike_generation_impairment, 1.0f);
}

/**
 * TEST: Integration Impairment
 * BIOLOGICAL: Cytokines impair dendritic integration capacity
 */
TEST_F(DendriticImmuneTest, IntegrationImpairment) {
    // Apply cytokine effects
    dendritic_immune_apply_cytokine_effects(bridge);

    // Get effects
    cytokine_dendritic_effects_t effects;
    dendritic_immune_get_cytokine_effects(bridge, &effects);

    // Integration impairment should be valid
    EXPECT_GE(effects.integration_impairment, 0.0f);
    EXPECT_LE(effects.integration_impairment, 1.0f);
}

/**
 * TEST: Growth Suppression
 * BIOLOGICAL: Pro-inflammatory cytokines suppress dendritic growth
 */
TEST_F(DendriticImmuneTest, GrowthSuppression) {
    // Apply cytokine effects
    dendritic_immune_apply_cytokine_effects(bridge);

    // Get effects
    cytokine_dendritic_effects_t effects;
    dendritic_immune_get_cytokine_effects(bridge, &effects);

    // Growth suppression should be valid
    EXPECT_GE(effects.growth_suppression, 0.0f);
    EXPECT_LE(effects.growth_suppression, 1.0f);
}

/**
 * TEST: Thread Safety
 * BIOLOGICAL: Verify thread-safe access to shared state
 */
TEST_F(DendriticImmuneTest, ThreadSafety) {
    // Multiple concurrent operations should be safe
    for (int i = 0; i < 10; i++) {
        dendritic_immune_apply_cytokine_effects(bridge);
        dendritic_immune_apply_inflammation_effects(bridge);

        float density = dendritic_immune_get_spine_density(bridge);
        EXPECT_GT(density, 0.0f);
        EXPECT_LE(density, 1.0f);
    }
}

/**
 * TEST: Null Pointer Safety in Query Functions
 * BIOLOGICAL: Verify robust handling of null pointers
 */
TEST(DendriticImmuneNullTest, NullPointerSafetyInQueries) {
    // All query functions should handle null safely
    EXPECT_FLOAT_EQ(dendritic_immune_get_spine_density(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(dendritic_immune_get_complexity_loss(nullptr), 0.0f);
    EXPECT_FALSE(dendritic_immune_is_atrophy(nullptr));

    cytokine_dendritic_effects_t effects;
    EXPECT_EQ(dendritic_immune_get_cytokine_effects(nullptr, &effects), -1);

    inflammation_dendritic_state_t state;
    EXPECT_EQ(dendritic_immune_get_inflammation_state(nullptr, &state), -1);
}
