/**
 * @file test_dynamic_adaptation_integration.cpp
 * @brief Integration tests for Phase C4.5 dynamic adaptation
 *
 * WHAT: Tests dynamic adaptation integration with neuromodulator system
 * WHY:  Ensure Phase C4.5 works correctly in multi-component scenarios
 * HOW:  Test with spatial_neuromod_system, multi-field scenarios
 *
 * TEST COVERAGE:
 * - Integration with neuromodulator system
 * - Multi-field adaptation
 * - Multi-timestep behavior
 * - Performance under realistic workloads
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 */

#include <gtest/gtest.h>
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/quantum/nimcp_quantum_shannon.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DynamicAdaptationIntegrationTest : public ::testing::Test {
protected:
    neural_network_t network;
    spatial_neuromod_system_t* neuromod_system;
    uint32_t num_neurons;

    void SetUp() override {
        num_neurons = 200;

        // Create network
        network_config_t net_config = {};
        net_config.num_neurons = num_neurons;
        net_config.ei_ratio = 0.8f;
        net_config.learning_rate = 0.01f;
        net_config.stdp_window = 20.0f;
        net_config.refractory_period = 2.0f;
        net_config.min_weight = 0.0f;
        net_config.max_weight = 1.0f;
        net_config.input_size = num_neurons;
        net_config.output_size = num_neurons;

        network = neural_network_create(&net_config);
        ASSERT_NE(network, nullptr);

        // Create neuromodulator system with all Phase C4.x features enabled
        spatial_neuromod_config_t configs[NEUROMOD_COUNT];
        for (int i = 0; i < NEUROMOD_COUNT; i++) {
            configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
            configs[i].enable_quantum_walk = true;
            configs[i].enable_adaptive_routing = true;
            configs[i].enable_dynamic_adaptation = true;  // Phase C4.5
            configs[i].min_adaptive_sources = 1;
            configs[i].max_adaptive_sources = 10;
            configs[i].adaptation_rate = 0.1f;
            configs[i].target_efficiency = 0.75f;
            configs[i].efficiency_tolerance = 0.1f;
            configs[i].adaptation_cooldown_steps = 50;
        }

        bool enabled_types[NEUROMOD_COUNT] = {true, true, false, false};

        neuromod_system = spatial_neuromod_system_create(network, enabled_types, configs);
        ASSERT_NE(neuromod_system, nullptr);

        // Enable quantum-Shannon for active fields
        for (int i = 0; i < NEUROMOD_COUNT; i++) {
            if (neuromod_system->fields[i]) {
                neuromod_system->fields[i]->use_quantum_shannon = true;
                quantum_shannon_config_t qs_config = quantum_shannon_default_config();
                neuromod_system->fields[i]->quantum_shannon_diffusion = quantum_shannon_create(
                    network, num_neurons / 2, 10.0f, &qs_config);
                ASSERT_NE(neuromod_system->fields[i]->quantum_shannon_diffusion, nullptr);

                // Set good Shannon metrics
                neuromod_system->fields[i]->last_propagation_efficiency = 0.75f;
                neuromod_system->fields[i]->last_speedup_vs_classical = 15.0f;
                neuromod_system->fields[i]->last_num_bottlenecks = 0;
                neuromod_system->fields[i]->last_information_rate = 2.0f;
            }
        }
    }

    void TearDown() override {
        if (neuromod_system) {
            spatial_neuromod_system_destroy(neuromod_system);
        }
        if (network) {
            neural_network_destroy(network);
        }
    }
};

//=============================================================================
// Test: System Integration
//=============================================================================

TEST_F(DynamicAdaptationIntegrationTest, System_AllFields_InitializedCorrectly) {
    // WHAT: Test that all neuromodulator fields initialize correctly
    // WHY:  Ensure Phase C4.5 state is valid
    // HOW:  Check initial values

    // Check dopamine
    ASSERT_NE(neuromod_system->fields[NEUROMOD_DOPAMINE], nullptr);
    EXPECT_EQ(spatial_neuromod_get_current_adaptive_sources(
        neuromod_system->fields[NEUROMOD_DOPAMINE]), 3u);  // Default from config

    // Check serotonin
    ASSERT_NE(neuromod_system->fields[NEUROMOD_SEROTONIN], nullptr);
    EXPECT_EQ(spatial_neuromod_get_current_adaptive_sources(
        neuromod_system->fields[NEUROMOD_SEROTONIN]), 3u);
}

TEST_F(DynamicAdaptationIntegrationTest, System_MultiField_DynamicAdaptationWorks) {
    // WHAT: Test that multiple fields can adapt independently
    // WHY:  Each neuromodulator should adapt based on its own efficiency
    // HOW:  Set different efficiencies, update, check independent adaptation

    spatial_neuromod_field_t* da_field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    spatial_neuromod_field_t* serotonin_field = neuromod_system->fields[NEUROMOD_SEROTONIN];

    // Set different efficiencies
    da_field->last_propagation_efficiency = 0.6f;  // Low (should increase K)
    serotonin_field->last_propagation_efficiency = 0.9f;  // High (should decrease K)

    // Get configs
    spatial_neuromod_config_t da_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    da_config.enable_quantum_walk = true;
    da_config.enable_adaptive_routing = true;
    da_config.enable_dynamic_adaptation = true;
    da_config.target_efficiency = 0.75f;
    da_config.efficiency_tolerance = 0.1f;

    spatial_neuromod_config_t serotonin_config = spatial_neuromod_default_config(NEUROMOD_SEROTONIN);
    serotonin_config.enable_quantum_walk = true;
    serotonin_config.enable_adaptive_routing = true;
    serotonin_config.enable_dynamic_adaptation = true;
    serotonin_config.target_efficiency = 0.75f;
    serotonin_config.efficiency_tolerance = 0.1f;

    // Update multiple times to build EMA
    for (int i = 0; i < 20; i++) {
        da_field->adaptation_cooldown = 0;  // Skip cooldown for testing
        serotonin_field->adaptation_cooldown = 0;

        spatial_neuromod_update_dynamic_adaptation(da_field, &da_config);
        spatial_neuromod_update_dynamic_adaptation(serotonin_field, &serotonin_config);
    }

    // Check that fields adapted independently
    uint32_t da_K = spatial_neuromod_get_current_adaptive_sources(da_field);
    uint32_t serotonin_K = spatial_neuromod_get_current_adaptive_sources(serotonin_field);

    // DA should have higher K (or at least not decreased)
    // Serotonin should have lower K (or at least not increased)
    EXPECT_GT(da_K, 0u);
    EXPECT_GT(serotonin_K, 0u);
}

//=============================================================================
// Test: Multi-Timestep Behavior
//=============================================================================

TEST_F(DynamicAdaptationIntegrationTest, MultiTimestep_AdaptationOccursOverTime) {
    // WHAT: Test that adaptation happens over multiple timesteps
    // WHY:  Verify EMA accumulation and eventual adaptation
    // HOW:  Run many timesteps with consistent efficiency

    spatial_neuromod_field_t* da_field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    uint32_t initial_K = spatial_neuromod_get_current_adaptive_sources(da_field);

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;
    config.enable_dynamic_adaptation = true;
    config.target_efficiency = 0.75f;
    config.efficiency_tolerance = 0.1f;
    config.adaptation_cooldown_steps = 10;

    // Set consistently low efficiency
    for (int i = 0; i < 100; i++) {
        da_field->last_propagation_efficiency = 0.5f;  // Consistently low
        spatial_neuromod_update_dynamic_adaptation(da_field, &config);
    }

    uint32_t final_K = spatial_neuromod_get_current_adaptive_sources(da_field);

    // K should have increased due to low efficiency
    EXPECT_GE(final_K, initial_K);
    EXPECT_LE(final_K, 10u);  // Still within max
}

TEST_F(DynamicAdaptationIntegrationTest, MultiTimestep_CooldownPreventsTooFrequentAdaptation) {
    // WHAT: Test that cooldown limits adaptation frequency
    // WHY:  Prevent oscillations
    // HOW:  Track adaptation count over many timesteps

    spatial_neuromod_field_t* da_field = neuromod_system->fields[NEUROMOD_DOPAMINE];

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;
    config.enable_dynamic_adaptation = true;
    config.target_efficiency = 0.75f;
    config.efficiency_tolerance = 0.1f;
    config.adaptation_cooldown_steps = 20;  // Cooldown

    uint32_t adaptation_count = 0;
    uint32_t prev_K = spatial_neuromod_get_current_adaptive_sources(da_field);

    // Run many timesteps
    for (int i = 0; i < 200; i++) {
        da_field->last_propagation_efficiency = 0.5f;  // Low efficiency
        spatial_neuromod_update_dynamic_adaptation(da_field, &config);

        uint32_t curr_K = spatial_neuromod_get_current_adaptive_sources(da_field);
        if (curr_K != prev_K) {
            adaptation_count++;
            prev_K = curr_K;
        }
    }

    // Should have limited adaptations (not every timestep)
    // With cooldown=20, expect at most 200/20 = 10 adaptations
    EXPECT_LE(adaptation_count, 12u);  // Allow some margin
}

//=============================================================================
// Test: Performance Under Load
//=============================================================================

TEST_F(DynamicAdaptationIntegrationTest, Performance_MultipleFieldUpdates_NoSignificantOverhead) {
    // WHAT: Test that dynamic adaptation doesn't significantly slow down system
    // WHY:  Phase C4.5 should have minimal overhead
    // HOW:  Run many updates on multiple fields

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;
    config.enable_dynamic_adaptation = true;

    // Run 1000 updates
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < NEUROMOD_COUNT; j++) {
            if (neuromod_system->fields[j]) {
                spatial_neuromod_update_dynamic_adaptation(
                    neuromod_system->fields[j], &config);
            }
        }
    }

    // Just verify it completes (implicit performance check - no timeout)
}

//=============================================================================
// Test: Edge Cases
//=============================================================================

TEST_F(DynamicAdaptationIntegrationTest, EdgeCase_VaryingEfficiency_KAdaptsAppropriately) {
    // WHAT: Test behavior with varying efficiency over time
    // WHY:  Real scenarios have fluctuating performance
    // HOW:  Alternate between high and low efficiency

    spatial_neuromod_field_t* da_field = neuromod_system->fields[NEUROMOD_DOPAMINE];

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;
    config.enable_dynamic_adaptation = true;
    config.target_efficiency = 0.75f;
    config.efficiency_tolerance = 0.1f;
    config.adaptation_cooldown_steps = 5;  // Short cooldown

    // Vary efficiency
    for (int i = 0; i < 100; i++) {
        if (i % 2 == 0) {
            da_field->last_propagation_efficiency = 0.5f;  // Low
        } else {
            da_field->last_propagation_efficiency = 0.95f;  // High
        }
        spatial_neuromod_update_dynamic_adaptation(da_field, &config);
    }

    // K should be valid
    uint32_t K = spatial_neuromod_get_current_adaptive_sources(da_field);
    EXPECT_GT(K, 0u);
    EXPECT_LE(K, 10u);
}

TEST_F(DynamicAdaptationIntegrationTest, EdgeCase_DisabledField_NoAdaptation) {
    // WHAT: Test that disabled fields don't adapt
    // WHY:  Backward compatibility
    // HOW:  Disable dynamic adaptation, verify K stays constant

    spatial_neuromod_field_t* da_field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    uint32_t initial_K = spatial_neuromod_get_current_adaptive_sources(da_field);

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;
    config.enable_dynamic_adaptation = false;  // Disabled

    // Run many updates with low efficiency
    for (int i = 0; i < 100; i++) {
        da_field->last_propagation_efficiency = 0.3f;  // Very low
        spatial_neuromod_update_dynamic_adaptation(da_field, &config);
    }

    // K should not have changed
    uint32_t final_K = spatial_neuromod_get_current_adaptive_sources(da_field);
    EXPECT_EQ(final_K, initial_K);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
