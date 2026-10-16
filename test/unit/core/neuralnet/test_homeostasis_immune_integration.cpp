/**
 * @file test_homeostasis_immune_integration.cpp
 * @brief Unit tests for immune system integration with homeostasis
 *
 * WHAT: Tests immune-homeostasis integration features
 * WHY:  Verify inflammation, cytokines, and metabolic load affect homeostasis
 * HOW:  Unit test each integration function with edge cases
 */

#include <gtest/gtest.h>
#include <cstring>
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_core.h"
#include "core/neuralnet/nimcp_neuralnet_homeostasis.h"

class HomeostasisImmuneIntegrationTest : public ::testing::Test {
protected:
    neural_network_t network;
    network_config_t config;

    void SetUp() override {
        // Create minimal network config
        memset(&config, 0, sizeof(config));
        config.num_neurons = 10;
        config.input_size = 3;     // Required by validate_network_config
        config.output_size = 2;    // Required by validate_network_config
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

        // Create network
        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        // Initialize homeostatic parameters for all neurons
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
        }
    }

    void TearDown() override {
        if (network) {
            neural_network_destroy(network);
        }
    }
};

//=============================================================================
// Inflammation Effects Tests
//=============================================================================

TEST_F(HomeostasisImmuneIntegrationTest, ApplyInflammation_IncreasesTargetActivity) {
    // WHAT: Inflammation should increase homeostatic set point (fever analogy)
    // WHY:  Pro-inflammatory cytokines increase neural excitability

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float baseline = neuron->homeostatic.target_activity;

    bool result = neural_network_apply_immune_inflammation(network, 0.5f, 0);

    EXPECT_TRUE(result);
    EXPECT_GT(neuron->homeostatic.target_activity, baseline);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 0.5f);
    EXPECT_GT(neuron->homeostatic.inflammation_start, 0u);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyInflammation_ZeroLevel_NoEffect) {
    // WHAT: Zero inflammation should not change state

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float baseline = neuron->homeostatic.target_activity;

    bool result = neural_network_apply_immune_inflammation(network, 0.0f, 0);

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(neuron->homeostatic.target_activity, baseline);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 0.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyInflammation_MaxLevel_LargestIncrease) {
    // WHAT: Maximum inflammation should cause largest target activity increase

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float baseline = neuron->homeostatic.baseline_target_activity;

    bool result = neural_network_apply_immune_inflammation(network, 1.0f, 0);

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(neuron->homeostatic.target_activity, baseline * 1.5f);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 1.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyInflammation_InvalidLevel_ReturnsError) {
    // WHAT: Invalid inflammation levels should be rejected

    bool result1 = neural_network_apply_immune_inflammation(network, -0.1f, 0);
    bool result2 = neural_network_apply_immune_inflammation(network, 1.5f, 0);

    EXPECT_FALSE(result1);
    EXPECT_FALSE(result2);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyInflammation_SpecificRegion_AffectsOnlyTarget) {
    // WHAT: Region-specific inflammation should only affect target neuron

    bool result = neural_network_apply_immune_inflammation(network, 0.5f, 3);

    EXPECT_TRUE(result);

    neuron_t* affected = neural_network_get_neuron(network, 3);
    neuron_t* unaffected = neural_network_get_neuron(network, 5);

    EXPECT_FLOAT_EQ(affected->homeostatic.inflammation_modulation, 0.5f);
    EXPECT_FLOAT_EQ(unaffected->homeostatic.inflammation_modulation, 0.0f);
}

//=============================================================================
// Anti-Inflammatory Effects Tests
//=============================================================================

TEST_F(HomeostasisImmuneIntegrationTest, ApplyAntiInflammatory_ReducesInflammation) {
    // WHAT: IL-10 should reduce inflammation and restore baseline
    // WHY:  Anti-inflammatory cytokines aid resolution

    neuron_t* neuron = neural_network_get_neuron(network, 0);

    // Apply inflammation first
    neural_network_apply_immune_inflammation(network, 0.8f, 0);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 0.8f);

    // Apply anti-inflammatory
    bool result = neural_network_apply_anti_inflammatory(network, 0.5f, 0);

    EXPECT_TRUE(result);
    EXPECT_LT(neuron->homeostatic.inflammation_modulation, 0.8f);
    EXPECT_GE(neuron->homeostatic.inflammation_modulation, 0.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyAntiInflammatory_FullDose_ClearsInflammation) {
    // WHAT: High IL-10 concentration should fully resolve inflammation

    neuron_t* neuron = neural_network_get_neuron(network, 0);

    // Apply moderate inflammation
    neural_network_apply_immune_inflammation(network, 0.5f, 0);

    // Apply maximum anti-inflammatory
    bool result = neural_network_apply_anti_inflammatory(network, 1.0f, 0);

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 0.0f);
    EXPECT_EQ(neuron->homeostatic.inflammation_start, 0u);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyAntiInflammatory_ReducesMetabolicLoad) {
    // WHAT: IL-10 should also reduce metabolic load

    neuron_t* neuron = neural_network_get_neuron(network, 0);

    // Set metabolic load
    neural_network_apply_immune_metabolic_load(network, 0.6f, 0);
    EXPECT_FLOAT_EQ(neuron->homeostatic.metabolic_load, 0.6f);

    // Apply anti-inflammatory
    neural_network_apply_anti_inflammatory(network, 0.5f, 0);

    EXPECT_LT(neuron->homeostatic.metabolic_load, 0.6f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyAntiInflammatory_RestoresTargetActivity) {
    // WHAT: IL-10 should restore target activity toward baseline

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float baseline = neuron->homeostatic.baseline_target_activity;

    // Apply inflammation
    neural_network_apply_immune_inflammation(network, 0.8f, 0);
    float inflamed_target = neuron->homeostatic.target_activity;
    EXPECT_GT(inflamed_target, baseline);

    // Apply anti-inflammatory
    neural_network_apply_anti_inflammatory(network, 0.5f, 0);

    EXPECT_LT(neuron->homeostatic.target_activity, inflamed_target);
    EXPECT_GE(neuron->homeostatic.target_activity, baseline);
}

//=============================================================================
// Cytokine Scaling Modulation Tests
//=============================================================================

TEST_F(HomeostasisImmuneIntegrationTest, ModulateScalingRate_NegativeSlows) {
    // WHAT: Negative cytokine modulation should slow synaptic scaling
    // WHY:  Pro-inflammatory cytokines reduce plasticity

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float baseline_strength = neuron->homeostatic.strength;

    bool result = neural_network_modulate_scaling_rate(network, 0, -0.5f);

    EXPECT_TRUE(result);
    EXPECT_LT(neuron->homeostatic.strength, baseline_strength);
    EXPECT_FLOAT_EQ(neuron->homeostatic.cytokine_scaling_factor, -0.5f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ModulateScalingRate_PositiveSpeeds) {
    // WHAT: Positive cytokine modulation should speed synaptic scaling

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float baseline_strength = neuron->homeostatic.strength;

    bool result = neural_network_modulate_scaling_rate(network, 0, 0.5f);

    EXPECT_TRUE(result);
    EXPECT_GT(neuron->homeostatic.strength, baseline_strength);
    EXPECT_FLOAT_EQ(neuron->homeostatic.cytokine_scaling_factor, 0.5f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ModulateScalingRate_ClampedRange) {
    // WHAT: Scaling multiplier should be clamped to reasonable range

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    neuron->homeostatic.strength = 0.1f;

    // Extreme negative modulation
    neural_network_modulate_scaling_rate(network, 0, -1.0f);
    EXPECT_GE(neuron->homeostatic.strength, 0.01f); // Should be clamped
}

TEST_F(HomeostasisImmuneIntegrationTest, ModulateScalingRate_InvalidRange_ReturnsError) {
    // WHAT: Out-of-range modulation should be rejected

    bool result1 = neural_network_modulate_scaling_rate(network, 0, -1.5f);
    bool result2 = neural_network_modulate_scaling_rate(network, 0, 1.5f);

    EXPECT_FALSE(result1);
    EXPECT_FALSE(result2);
}

//=============================================================================
// Metabolic Load Tests
//=============================================================================

TEST_F(HomeostasisImmuneIntegrationTest, ApplyMetabolicLoad_ReducesPlasticity) {
    // WHAT: Metabolic load should reduce plasticity rate
    // WHY:  Immune activation competes for energy resources

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    neuron->plasticity_rate = 0.01f;
    float baseline = neuron->plasticity_rate;

    bool result = neural_network_apply_immune_metabolic_load(network, 0.5f, 0);

    EXPECT_TRUE(result);
    EXPECT_LT(neuron->plasticity_rate, baseline);
    EXPECT_FLOAT_EQ(neuron->homeostatic.metabolic_load, 0.5f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyMetabolicLoad_MaxLoad_SignificantReduction) {
    // WHAT: Maximum metabolic load should greatly reduce plasticity

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    neuron->plasticity_rate = 0.01f;

    bool result = neural_network_apply_immune_metabolic_load(network, 1.0f, 0);

    EXPECT_TRUE(result);
    EXPECT_LT(neuron->plasticity_rate, 0.006f); // 50% reduction
    EXPECT_GE(neuron->plasticity_rate, 0.001f); // Minimum clamp
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyMetabolicLoad_ClampedMinimum) {
    // WHAT: Plasticity rate should be clamped to minimum

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    neuron->plasticity_rate = 0.001f;

    neural_network_apply_immune_metabolic_load(network, 1.0f, 0);

    EXPECT_GE(neuron->plasticity_rate, 0.001f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ApplyMetabolicLoad_InvalidLevel_ReturnsError) {
    // WHAT: Invalid metabolic load should be rejected

    bool result1 = neural_network_apply_immune_metabolic_load(network, -0.1f, 0);
    bool result2 = neural_network_apply_immune_metabolic_load(network, 1.5f, 0);

    EXPECT_FALSE(result1);
    EXPECT_FALSE(result2);
}

//=============================================================================
// Allostatic Load Tests
//=============================================================================

TEST_F(HomeostasisImmuneIntegrationTest, AccumulateAllostaticLoad_IncreasesOverTime) {
    // WHAT: Chronic inflammation should accumulate allostatic load
    // WHY:  Long-term stress damages neural health

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    EXPECT_FLOAT_EQ(neuron->homeostatic.allostatic_load, 0.0f);

    bool result = neural_network_accumulate_allostatic_load(network, 0, 5000, 0.7f);

    EXPECT_TRUE(result);
    EXPECT_GT(neuron->homeostatic.allostatic_load, 0.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, AccumulateAllostaticLoad_ReducesHomeostasisStrength) {
    // WHAT: Allostatic load should impair homeostatic effectiveness

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float baseline_strength = neuron->homeostatic.strength;

    neural_network_accumulate_allostatic_load(network, 0, 10000, 0.8f);

    EXPECT_LT(neuron->homeostatic.strength, baseline_strength);
}

TEST_F(HomeostasisImmuneIntegrationTest, AccumulateAllostaticLoad_LongerDuration_MoreLoad) {
    // WHAT: Longer inflammation duration should cause more allostatic load

    neuron_t* neuron1 = neural_network_get_neuron(network, 0);
    neuron_t* neuron2 = neural_network_get_neuron(network, 1);

    neural_network_accumulate_allostatic_load(network, 0, 5000, 0.5f);
    neural_network_accumulate_allostatic_load(network, 1, 15000, 0.5f);

    EXPECT_GT(neuron2->homeostatic.allostatic_load, neuron1->homeostatic.allostatic_load);
}

TEST_F(HomeostasisImmuneIntegrationTest, AccumulateAllostaticLoad_InvalidLevel_ReturnsError) {
    // WHAT: Invalid inflammation level should be rejected

    bool result1 = neural_network_accumulate_allostatic_load(network, 0, 5000, -0.1f);
    bool result2 = neural_network_accumulate_allostatic_load(network, 0, 5000, 1.5f);

    EXPECT_FALSE(result1);
    EXPECT_FALSE(result2);
}

//=============================================================================
// Homeostatic Health Metric Tests
//=============================================================================

TEST_F(HomeostasisImmuneIntegrationTest, ComputeHealth_HealthyState_NearOne) {
    // WHAT: Healthy neuron should have health near 1.0

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    neuron->avg_activity = 0.5f;
    neuron->homeostatic.target_activity = 0.5f;

    float health = neural_network_compute_homeostatic_health(network, 0);

    EXPECT_GT(health, 0.9f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ComputeHealth_Inflammation_ReducesHealth) {
    // WHAT: Inflammation should reduce health metric

    neural_network_apply_immune_inflammation(network, 0.8f, 0);

    float health = neural_network_compute_homeostatic_health(network, 0);

    EXPECT_LT(health, 1.0f);
    EXPECT_GT(health, 0.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ComputeHealth_MetabolicLoad_ReducesHealth) {
    // WHAT: Metabolic load should reduce health metric

    neural_network_apply_immune_metabolic_load(network, 0.7f, 0);

    float health = neural_network_compute_homeostatic_health(network, 0);

    EXPECT_LT(health, 1.0f);
    EXPECT_GT(health, 0.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ComputeHealth_AllostaticLoad_ReducesHealth) {
    // WHAT: Allostatic load should exponentially reduce health
    // NOTE: Allostatic load accumulates slowly (factor 0.001) to model gradual health impact
    //       With duration=20000ms, level=0.9: load_increment = 0.9 * 20 * 0.001 = 0.018
    //       This produces allostatic_penalty = exp(-0.018) = 0.982, so health < 1.0

    neural_network_accumulate_allostatic_load(network, 0, 20000, 0.9f);

    float health = neural_network_compute_homeostatic_health(network, 0);

    // Verify health is reduced from baseline (should be < 1.0 due to allostatic load)
    EXPECT_LT(health, 1.0f);
    EXPECT_GT(health, 0.0f);

    // Accumulate more load to see significant reduction
    // After 5 more accumulations, total load ~ 0.108, penalty = exp(-0.108) ~ 0.898
    for (int i = 0; i < 5; i++) {
        neural_network_accumulate_allostatic_load(network, 0, 20000, 0.9f);
    }
    float health_chronic = neural_network_compute_homeostatic_health(network, 0);
    EXPECT_LT(health_chronic, 0.95f);  // Chronic inflammation should show more impact
}

TEST_F(HomeostasisImmuneIntegrationTest, ComputeHealth_MultipleFactors_CompoundEffect) {
    // WHAT: Multiple immune factors should compound to reduce health

    neuron_t* neuron = neural_network_get_neuron(network, 0);

    // Apply multiple stressors
    neural_network_apply_immune_inflammation(network, 0.7f, 0);
    neural_network_apply_immune_metabolic_load(network, 0.6f, 0);
    neural_network_accumulate_allostatic_load(network, 0, 10000, 0.7f);

    float health = neural_network_compute_homeostatic_health(network, 0);

    EXPECT_LT(health, 0.6f); // Significantly reduced
    EXPECT_GT(health, 0.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ComputeHealth_ClampedRange) {
    // WHAT: Health metric should always be in [0, 1] range

    // Apply extreme stressors
    neural_network_apply_immune_inflammation(network, 1.0f, 0);
    neural_network_apply_immune_metabolic_load(network, 1.0f, 0);
    neural_network_accumulate_allostatic_load(network, 0, 50000, 1.0f);

    float health = neural_network_compute_homeostatic_health(network, 0);

    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(HomeostasisImmuneIntegrationTest, ComputeHealth_InvalidNeuron_ReturnsZero) {
    // WHAT: Invalid neuron ID should return 0.0

    float health = neural_network_compute_homeostatic_health(network, 999);

    EXPECT_FLOAT_EQ(health, 0.0f);
}

//=============================================================================
// Immune System Connection Tests
//=============================================================================

TEST_F(HomeostasisImmuneIntegrationTest, ConnectImmuneSystem_Success) {
    // WHAT: Should successfully connect immune system

    void* mock_immune_system = (void*)0x12345678;

    bool result = neural_network_connect_immune_system(network,
        (brain_immune_system_t*)mock_immune_system);

    EXPECT_TRUE(result);
}

TEST_F(HomeostasisImmuneIntegrationTest, ConnectImmuneSystem_NullNetwork_ReturnsError) {
    // WHAT: Null network should be rejected

    void* mock_immune_system = (void*)0x12345678;

    bool result = neural_network_connect_immune_system(nullptr,
        (brain_immune_system_t*)mock_immune_system);

    EXPECT_FALSE(result);
}

//=============================================================================
// Integration Scenario Tests
//=============================================================================

TEST_F(HomeostasisImmuneIntegrationTest, Scenario_InflammationResolutionCycle) {
    // WHAT: Full inflammation → resolution cycle
    // WHY:  Verify complete immune-homeostasis interaction

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float baseline = neuron->homeostatic.target_activity;

    // Phase 1: Inflammation onset
    neural_network_apply_immune_inflammation(network, 0.7f, 0);
    EXPECT_GT(neuron->homeostatic.target_activity, baseline);
    EXPECT_GT(neuron->homeostatic.inflammation_start, 0u);

    // Phase 2: Metabolic load increases
    neural_network_apply_immune_metabolic_load(network, 0.6f, 0);
    EXPECT_FLOAT_EQ(neuron->homeostatic.metabolic_load, 0.6f);

    // Phase 3: Begin resolution with IL-10
    neural_network_apply_anti_inflammatory(network, 0.5f, 0);
    EXPECT_LT(neuron->homeostatic.inflammation_modulation, 0.7f);

    // Phase 4: Complete resolution
    neural_network_apply_anti_inflammatory(network, 1.0f, 0);
    EXPECT_FLOAT_EQ(neuron->homeostatic.inflammation_modulation, 0.0f);
    EXPECT_EQ(neuron->homeostatic.inflammation_start, 0u);
}

TEST_F(HomeostasisImmuneIntegrationTest, Scenario_ChronicInflammation) {
    // WHAT: Chronic inflammation accumulates allostatic load
    // WHY:  Verify long-term health degradation

    neuron_t* neuron = neural_network_get_neuron(network, 0);
    float initial_health = neural_network_compute_homeostatic_health(network, 0);

    // Sustained inflammation
    neural_network_apply_immune_inflammation(network, 0.6f, 0);

    // Accumulate load over time
    for (int i = 0; i < 5; i++) {
        neural_network_accumulate_allostatic_load(network, 0, 5000, 0.6f);
    }

    float final_health = neural_network_compute_homeostatic_health(network, 0);

    EXPECT_LT(final_health, initial_health);
    EXPECT_GT(neuron->homeostatic.allostatic_load, 0.0f);
}

// Main function
int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
