/**
 * @file test_homeostasis_immune_integration.cpp
 * @brief Integration tests for immune-homeostasis system interaction
 *
 * WHAT: Tests full integration between immune system and homeostasis
 * WHY:  Verify immune events properly affect homeostatic regulation
 * HOW:  Integration scenarios with realistic immune-neural interactions
 */

#include <gtest/gtest.h>
#include <cstring>
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_core.h"
#include "core/neuralnet/nimcp_neuralnet_homeostasis.h"
#include "cognitive/immune/nimcp_brain_immune.h"

class ImmuneHomeostasisIntegrationTest : public ::testing::Test {
protected:
    neural_network_t network;
    brain_immune_system_t* immune_system;
    network_config_t config;

    void SetUp() override {
        // Create network
        memset(&config, 0, sizeof(config));
        config.num_neurons = 50;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.hebbian_rate = 0.001f;
        config.stdp_window = 20.0f;
        config.homeostatic_rate = 0.001f;
        config.target_activity = 0.5f;
        config.adaptation_rate = 0.01f;
        config.refractory_period = 5.0f;
        config.min_weight = 0.0f;
        config.max_weight = 1.0f;
        config.update_interval = 1;
        config.input_size = 50;
        config.output_size = 50;

        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        // Initialize neurons
        for (uint32_t i = 0; i < config.num_neurons; i++) {
            neuron_t* neuron = neural_network_get_neuron(network, i);
            ASSERT_NE(neuron, nullptr);

            neuron->homeostatic.target_activity = 0.5f;
            neuron->homeostatic.baseline_target_activity = 0.5f;
            neuron->homeostatic.time_scale = 1000.0f;
            neuron->homeostatic.strength = 0.1f;
            neuron->homeostatic.inflammation_modulation = 0.0f;
            neuron->homeostatic.cytokine_scaling_factor = 0.0f;
            neuron->homeostatic.metabolic_load = 0.0f;
            neuron->homeostatic.allostatic_load = 0.0f;
            neuron->homeostatic.inflammation_start = 0;
            neuron->avg_activity = 0.5f;
            neuron->plasticity_rate = 0.01f;
        }

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        // Connect systems
        neural_network_connect_immune_system(network, immune_system);
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
        if (network) {
            neural_network_destroy(network);
        }
    }
};

//=============================================================================
// Immune-Triggered Homeostatic Response Tests
//=============================================================================

TEST_F(ImmuneHomeostasisIntegrationTest, ImmuneInflammation_TriggersHomeostasisShift) {
    // WHAT: Immune inflammation should shift homeostatic set points
    // WHY:  Biological integration of immune and neural homeostasis

    neuron_t* neuron = neural_network_get_neuron(network, 10);
    float baseline = neuron->homeostatic.target_activity;

    // Simulate immune inflammation
    neural_network_apply_immune_inflammation(network, 0.6f, 0);

    // Verify homeostasis shifted
    EXPECT_GT(neuron->homeostatic.target_activity, baseline);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 0.6f);

    // Verify health metric affected
    float health = neural_network_compute_homeostatic_health(network, 10);
    EXPECT_LT(health, 1.0f);
}

TEST_F(ImmuneHomeostasisIntegrationTest, RegionalInflammation_AffectsLocalNeurons) {
    // WHAT: Regional inflammation should affect only targeted neurons
    // WHY:  Localized immune response should have localized neural effects

    uint32_t affected_id = 15;
    uint32_t unaffected_id = 25;

    neuron_t* affected = neural_network_get_neuron(network, affected_id);
    neuron_t* unaffected = neural_network_get_neuron(network, unaffected_id);

    float baseline_affected = affected->homeostatic.target_activity;
    float baseline_unaffected = unaffected->homeostatic.target_activity;

    // Apply regional inflammation
    neural_network_apply_immune_inflammation(network, 0.7f, affected_id);

    // Verify only target affected
    EXPECT_GT(affected->homeostatic.target_activity, baseline_affected);
    EXPECT_FLOAT_EQ(unaffected->homeostatic.target_activity, baseline_unaffected);
}

TEST_F(ImmuneHomeostasisIntegrationTest, InflammationResolution_RestoresHomeostasis) {
    // WHAT: IL-10 resolution should restore homeostatic baseline
    // WHY:  Complete immune-homeostasis cycle

    neuron_t* neuron = neural_network_get_neuron(network, 20);
    float baseline = neuron->homeostatic.baseline_target_activity;

    // Inflammation phase
    neural_network_apply_immune_inflammation(network, 0.8f, 0);
    EXPECT_GT(neuron->homeostatic.target_activity, baseline);

    // Resolution phase
    neural_network_apply_anti_inflammatory(network, 1.0f, 0);

    // Verify restoration (within tolerance)
    EXPECT_NEAR(neuron->homeostatic.target_activity, baseline, 0.01f);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 0.0f);
}

//=============================================================================
// Cytokine-Plasticity Interaction Tests
//=============================================================================

TEST_F(ImmuneHomeostasisIntegrationTest, ProInflammatoryCytokines_ReducePlasticity) {
    // WHAT: Pro-inflammatory cytokines should reduce synaptic plasticity
    // WHY:  Immune activation temporarily impairs learning

    neuron_t* neuron = neural_network_get_neuron(network, 5);
    float baseline_plasticity = neuron->plasticity_rate;

    // Simulate pro-inflammatory state
    neural_network_modulate_scaling_rate(network, 5, -0.6f);
    neural_network_apply_immune_metabolic_load(network, 0.5f, 0);

    // Verify plasticity reduced
    EXPECT_LT(neuron->plasticity_rate, baseline_plasticity);
    EXPECT_FLOAT_EQ(neuron->homeostatic.cytokine_scaling_factor, -0.6f);
}

TEST_F(ImmuneHomeostasisIntegrationTest, AntiInflammatoryCytokines_RestorePlasticity) {
    // WHAT: Anti-inflammatory cytokines should restore plasticity
    // WHY:  Resolution phase normalizes learning capacity

    neuron_t* neuron = neural_network_get_neuron(network, 8);

    // Suppress plasticity
    neural_network_modulate_scaling_rate(network, 8, -0.7f);
    float suppressed_strength = neuron->homeostatic.strength;

    // Restore with anti-inflammatory
    neural_network_modulate_scaling_rate(network, 8, 0.4f);

    EXPECT_GT(neuron->homeostatic.strength, suppressed_strength);
}

//=============================================================================
// Metabolic Load Integration Tests
//=============================================================================

TEST_F(ImmuneHomeostasisIntegrationTest, ImmuneActivation_IncreasesMetabolicDemand) {
    // WHAT: Immune response should increase metabolic load
    // WHY:  Immune activation competes for neural resources

    neuron_t* neuron = neural_network_get_neuron(network, 12);
    float baseline_plasticity = neuron->plasticity_rate;

    // Simulate immune activation
    neural_network_apply_immune_metabolic_load(network, 0.7f, 0);

    // Verify metabolic impact
    EXPECT_FLOAT_EQ(neuron->homeostatic.metabolic_load, 0.7f);
    EXPECT_LT(neuron->plasticity_rate, baseline_plasticity);

    // Verify health impact
    float health = neural_network_compute_homeostatic_health(network, 12);
    EXPECT_LT(health, 0.8f);
}

TEST_F(ImmuneHomeostasisIntegrationTest, MetabolicLoad_CompoundsWithInflammation) {
    // WHAT: Metabolic load and inflammation should compound effects
    // WHY:  Multiple immune factors interact

    neuron_t* neuron = neural_network_get_neuron(network, 18);

    // Apply both factors
    neural_network_apply_immune_inflammation(network, 0.6f, 0);
    neural_network_apply_immune_metabolic_load(network, 0.5f, 0);

    // Verify compounded effects on health
    float health = neural_network_compute_homeostatic_health(network, 18);
    EXPECT_LT(health, 0.7f); // Significant health reduction
}

//=============================================================================
// Allostatic Load Accumulation Tests
//=============================================================================

TEST_F(ImmuneHomeostasisIntegrationTest, ChronicInflammation_AccumulatesAllostaticLoad) {
    // WHAT: Prolonged inflammation should accumulate allostatic burden
    // WHY:  Chronic immune activation damages neural health

    neuron_t* neuron = neural_network_get_neuron(network, 25);

    // Simulate chronic inflammation (many episodes with high severity)
    // Each episode: load_increment = level * (duration/1000) * 0.001
    // 50 episodes * 0.9 * 10.0 * 0.001 = 0.45 total load
    for (int i = 0; i < 50; i++) {
        neural_network_accumulate_allostatic_load(network, 25, 10000, 0.9f);
    }

    // Verify accumulation
    EXPECT_GT(neuron->homeostatic.allostatic_load, 0.1f);

    // Verify health degradation
    float health = neural_network_compute_homeostatic_health(network, 25);
    EXPECT_LT(health, 0.8f);
}

TEST_F(ImmuneHomeostasisIntegrationTest, AllostaticLoad_ImpairsHomeostasisCapacity) {
    // WHAT: Allostatic load should reduce homeostatic effectiveness
    // WHY:  Chronic stress damages homeostatic mechanisms

    neuron_t* neuron = neural_network_get_neuron(network, 30);
    float baseline_strength = neuron->homeostatic.strength;

    // Accumulate significant load
    neural_network_accumulate_allostatic_load(network, 30, 20000, 0.9f);

    // Verify impaired homeostasis
    EXPECT_LT(neuron->homeostatic.strength, baseline_strength);
    EXPECT_GT(neuron->homeostatic.allostatic_load, 0.0f);
}

//=============================================================================
// Complete Immune-Homeostasis Cycle Tests
//=============================================================================

TEST_F(ImmuneHomeostasisIntegrationTest, FullCycle_ThreatToResolution) {
    // WHAT: Complete threat detection → inflammation → resolution cycle
    // WHY:  Verify end-to-end immune-homeostasis integration

    neuron_t* neuron = neural_network_get_neuron(network, 35);
    float initial_health = neural_network_compute_homeostatic_health(network, 35);
    float baseline_target = neuron->homeostatic.baseline_target_activity;

    // Phase 1: Threat detection → inflammation
    neural_network_apply_immune_inflammation(network, 0.8f, 0);
    EXPECT_GT(neuron->homeostatic.target_activity, baseline_target);

    // Phase 2: Immune activation → metabolic load
    neural_network_apply_immune_metabolic_load(network, 0.6f, 0);
    float inflamed_health = neural_network_compute_homeostatic_health(network, 35);
    EXPECT_LT(inflamed_health, initial_health);

    // Phase 3: Cytokine modulation → reduced plasticity
    neural_network_modulate_scaling_rate(network, 35, -0.5f);
    float suppressed_strength = neuron->homeostatic.strength;

    // Phase 4: Threat cleared → anti-inflammatory
    neural_network_apply_anti_inflammatory(network, 0.7f, 0);
    EXPECT_LT(neuron->homeostatic.inflammation_modulation, 0.8f);

    // Phase 5: Resolution complete
    neural_network_apply_anti_inflammatory(network, 1.0f, 0);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 0.0f);

    // Phase 6: Verify recovery
    float final_health = neural_network_compute_homeostatic_health(network, 35);
    EXPECT_GT(final_health, inflamed_health);
}

TEST_F(ImmuneHomeostasisIntegrationTest, MultipleThreats_CumulativeEffect) {
    // WHAT: Multiple concurrent threats should have cumulative effects
    // WHY:  Realistic scenario with multiple immune challenges

    neuron_t* neuron = neural_network_get_neuron(network, 40);
    float initial_health = neural_network_compute_homeostatic_health(network, 40);

    // Multiple concurrent immune challenges
    neural_network_apply_immune_inflammation(network, 0.7f, 0);
    neural_network_apply_immune_metabolic_load(network, 0.6f, 0);
    neural_network_accumulate_allostatic_load(network, 40, 10000, 0.7f);

    // Verify cumulative health impact
    float stressed_health = neural_network_compute_homeostatic_health(network, 40);
    EXPECT_LT(stressed_health, 0.5f); // Significant degradation
    EXPECT_LT(stressed_health, initial_health);
}

TEST_F(ImmuneHomeostasisIntegrationTest, NetworkWide_ImmuneHomeostasisBalance) {
    // WHAT: Network-wide immune response with homeostatic compensation
    // WHY:  Test system-level balance

    // Apply network-wide inflammation
    neural_network_apply_immune_inflammation(network, 0.5f, 0);

    // Verify all neurons affected
    int affected_count = 0;
    for (uint32_t i = 0; i < 10; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (neuron->homeostatic.inflammation_modulation > 0.0f) {
            affected_count++;
        }
    }

    EXPECT_EQ(affected_count, 10); // All tested neurons affected

    // Apply network-wide resolution
    neural_network_apply_anti_inflammatory(network, 1.0f, 0);

    // Verify resolution
    int resolved_count = 0;
    for (uint32_t i = 0; i < 10; i++) {
        neuron_t* neuron = neural_network_get_neuron(network, i);
        if (neuron->homeostatic.inflammation_modulation < 0.1f) {
            resolved_count++;
        }
    }

    EXPECT_EQ(resolved_count, 10); // All resolved
}

// Main function
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
