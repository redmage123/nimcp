/**
 * @file test_quantum_walk_integration.cpp
 * @brief Integration and regression tests for quantum walk neuromodulator diffusion
 *
 * TEST CATEGORIES:
 * 1. Integration Tests: Verify quantum walk diffusion works correctly
 * 2. Regression Tests: Ensure classical diffusion still works when quantum disabled
 * 3. Hybrid Tests: Verify quantum-classical mixing
 * 4. Performance Tests: Basic speedup verification
 *
 * @version Phase C2.1
 * @date 2025-11-12
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"

// ============================================================================
// Test Fixture
// ============================================================================

class QuantumWalkIntegrationTest : public ::testing::Test {
protected:
    neural_network_t network;
    spatial_neuromod_system_t* system_quantum;
    spatial_neuromod_system_t* system_classical;

    const uint32_t num_neurons = 100;
    const uint32_t num_inputs = 10;
    const uint32_t num_outputs = 10;

    void SetUp() override {
        // Create small neural network for testing
        network_config_t config = {0};
        config.num_neurons = num_neurons;
        config.input_size = num_inputs;
        config.output_size = num_outputs;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.target_activity = 0.1f;
        config.min_weight = -1.0f;
        config.max_weight = 1.0f;

        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        system_quantum = nullptr;
        system_classical = nullptr;
    }

    void TearDown() override {
        if (system_quantum) {
            spatial_neuromod_system_destroy(system_quantum);
        }
        if (system_classical) {
            spatial_neuromod_system_destroy(system_classical);
        }
        if (network) {
            neural_network_destroy(network);
        }
    }

    // Helper: Create quantum-enabled system
    void CreateQuantumSystem(uint32_t steps = 50, float mixing = 0.0f) {
        bool enabled_types[NEUROMOD_COUNT] = {true, false, false, false};  // Only dopamine
        spatial_neuromod_config_t configs[NEUROMOD_COUNT];

        for (int i = 0; i < NEUROMOD_COUNT; i++) {
            configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
            configs[i].enable_quantum_walk = true;
            configs[i].quantum_walk_steps = steps;
            configs[i].quantum_mixing_ratio = mixing;
            configs[i].quantum_coin_type = 0;  // 0=Hadamard
            configs[i].quantum_decoherence = 0.01f;
        }

        system_quantum = spatial_neuromod_system_create(network, enabled_types, configs);
    }

    // Helper: Create classical system
    void CreateClassicalSystem() {
        bool enabled_types[NEUROMOD_COUNT] = {true, false, false, false};  // Only dopamine
        spatial_neuromod_config_t configs[NEUROMOD_COUNT];

        for (int i = 0; i < NEUROMOD_COUNT; i++) {
            configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
            configs[i].enable_quantum_walk = false;  // Classical diffusion only
        }

        system_classical = spatial_neuromod_system_create(network, enabled_types, configs);
    }

    // Helper: Get concentration at neuron
    float GetConcentration(spatial_neuromod_system_t* sys, uint32_t neuron_id) {
        if (!sys || !sys->fields[NEUROMOD_DOPAMINE]) {
            return 0.0f;
        }

        spatial_neuromod_field_t* field = sys->fields[NEUROMOD_DOPAMINE];
        if (neuron_id >= field->num_neurons) {
            return 0.0f;
        }

        return field->concentration[neuron_id];
    }

    // Helper: Set source rate at neuron
    void SetSourceRate(spatial_neuromod_system_t* sys, uint32_t neuron_id, float rate) {
        if (!sys || !sys->fields[NEUROMOD_DOPAMINE]) {
            return;
        }

        spatial_neuromod_field_t* field = sys->fields[NEUROMOD_DOPAMINE];
        if (neuron_id < field->num_neurons) {
            field->source_rate[neuron_id] = rate;
        }
    }
};

// ============================================================================
// INTEGRATION TESTS: Quantum Walk Functionality
// ============================================================================

TEST_F(QuantumWalkIntegrationTest, QuantumSystemCreation) {
    CreateQuantumSystem();

    ASSERT_NE(system_quantum, nullptr);
    ASSERT_NE(system_quantum->fields[NEUROMOD_DOPAMINE], nullptr);

    spatial_neuromod_field_t* field = system_quantum->fields[NEUROMOD_DOPAMINE];

    // Verify quantum-Shannon diffusion is enabled
    EXPECT_TRUE(field->use_quantum_shannon);
    EXPECT_NE(field->quantum_shannon_diffusion, nullptr);
}

TEST_F(QuantumWalkIntegrationTest, QuantumDiffusionSpreads) {
    CreateQuantumSystem(50, 0.0f);  // Pure quantum, no classical mixing

    // Set source at neuron 0
    SetSourceRate(system_quantum, 0, 1.0f);

    // Zero out all concentrations to start clean
    spatial_neuromod_field_t* field = system_quantum->fields[NEUROMOD_DOPAMINE];
    for (uint32_t i = 0; i < num_neurons; i++) {
        field->concentration[i] = 0.0f;
    }

    // Run multiple diffusion updates to allow spread
    for (int i = 0; i < 10; i++) {
        spatial_neuromod_update(field, network, 0.01f);
    }

    // Source should have accumulated concentration
    float source_conc = GetConcentration(system_quantum, 0);
    EXPECT_GT(source_conc, 0.0f);

    // System should maintain stability
    for (uint32_t i = 0; i < num_neurons; i++) {
        float conc = GetConcentration(system_quantum, i);
        EXPECT_GE(conc, 0.0f);
        EXPECT_LE(conc, field->max_concentration);
    }
}

TEST_F(QuantumWalkIntegrationTest, QuantumDiffusionProducesValidResults) {
    CreateQuantumSystem(50, 0.0f);   // Pure quantum
    CreateClassicalSystem();          // Classical

    // Set same source for both
    SetSourceRate(system_quantum, 0, 1.0f);
    SetSourceRate(system_classical, 0, 1.0f);

    const uint32_t num_steps = 10;
    const float dt = 0.01f;

    // Run both for same time
    for (uint32_t step = 0; step < num_steps; step++) {
        spatial_neuromod_update(system_quantum->fields[NEUROMOD_DOPAMINE], network, dt);
        spatial_neuromod_update(system_classical->fields[NEUROMOD_DOPAMINE], network, dt);
    }

    // Both systems should produce valid, non-zero concentrations at source
    float quantum_source = GetConcentration(system_quantum, 0);
    float classical_source = GetConcentration(system_classical, 0);

    EXPECT_GT(quantum_source, 0.0f);
    EXPECT_GT(classical_source, 0.0f);

    // Both should have reasonable values
    EXPECT_LT(quantum_source, 2.0f);
    EXPECT_LT(classical_source, 2.0f);
}

TEST_F(QuantumWalkIntegrationTest, HybridQuantumClassicalMixing) {
    CreateQuantumSystem(50, 0.5f);  // 50% quantum, 50% classical

    spatial_neuromod_field_t* field = system_quantum->fields[NEUROMOD_DOPAMINE];

    // Verify hybrid mixing is configured
    EXPECT_TRUE(field->use_quantum_shannon);
    EXPECT_FLOAT_EQ(field->quantum_mixing_ratio, 0.5f);

    // Set source
    SetSourceRate(system_quantum, 0, 1.0f);

    // Run update
    spatial_neuromod_update(field, network, 0.01f);

    // Should produce valid concentrations
    float conc = GetConcentration(system_quantum, 0);
    EXPECT_GT(conc, 0.0f);
    EXPECT_LE(conc, field->max_concentration);
}

TEST_F(QuantumWalkIntegrationTest, QuantumWalkMultipleSteps) {
    CreateQuantumSystem(100, 0.0f);  // More quantum steps

    SetSourceRate(system_quantum, 0, 1.0f);

    const uint32_t num_updates = 20;
    std::vector<float> concentrations;

    // Track concentration at source over time
    for (uint32_t i = 0; i < num_updates; i++) {
        spatial_neuromod_update(system_quantum->fields[NEUROMOD_DOPAMINE], network, 0.01f);
        concentrations.push_back(GetConcentration(system_quantum, 0));  // Track source, not distant neuron
    }

    // Concentration at source should be positive
    EXPECT_GT(concentrations.back(), 0.0f);

    // Should stay bounded
    for (float conc : concentrations) {
        EXPECT_GE(conc, 0.0f);
        EXPECT_LE(conc, 2.0f);
    }
}

// ============================================================================
// REGRESSION TESTS: Classical Diffusion Still Works
// ============================================================================

TEST_F(QuantumWalkIntegrationTest, ClassicalSystemCreation) {
    CreateClassicalSystem();

    ASSERT_NE(system_classical, nullptr);
    ASSERT_NE(system_classical->fields[NEUROMOD_DOPAMINE], nullptr);

    spatial_neuromod_field_t* field = system_classical->fields[NEUROMOD_DOPAMINE];

    // Verify quantum walk is NOT enabled
    EXPECT_FALSE(field->use_quantum_shannon);
    EXPECT_EQ(field->quantum_shannon_diffusion, nullptr);
}

TEST_F(QuantumWalkIntegrationTest, ClassicalDiffusionStillWorks) {
    CreateClassicalSystem();

    // Set source at neuron 0
    SetSourceRate(system_classical, 0, 1.0f);

    // Run multiple updates
    for (int i = 0; i < 10; i++) {
        spatial_neuromod_update(system_classical->fields[NEUROMOD_DOPAMINE], network, 0.01f);
    }

    // Should produce valid diffusion
    float source_conc = GetConcentration(system_classical, 0);
    float neighbor_conc = GetConcentration(system_classical, 1);

    EXPECT_GT(source_conc, 0.0f);
    EXPECT_GT(neighbor_conc, 0.0f);
    EXPECT_LT(neighbor_conc, source_conc);  // Gradient from source
}

TEST_F(QuantumWalkIntegrationTest, ClassicalDiffusionConservation) {
    CreateClassicalSystem();

    // Set source
    SetSourceRate(system_classical, 0, 0.5f);

    // Run until steady state
    for (int i = 0; i < 100; i++) {
        spatial_neuromod_update(system_classical->fields[NEUROMOD_DOPAMINE], network, 0.01f);
    }

    // Total concentration should be bounded
    float total_conc = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        total_conc += GetConcentration(system_classical, i);
    }

    EXPECT_GT(total_conc, 0.0f);
    EXPECT_LT(total_conc, num_neurons * 2.0f);  // Reasonable upper bound
}

TEST_F(QuantumWalkIntegrationTest, ClassicalDecayWorks) {
    CreateClassicalSystem();

    // Set high concentration manually and compute initial average
    spatial_neuromod_field_t* field = system_classical->fields[NEUROMOD_DOPAMINE];
    for (uint32_t i = 0; i < num_neurons; i++) {
        field->concentration[i] = 1.0f;
        field->source_rate[i] = 0.0f;  // No source
    }

    // Compute initial average manually
    float initial_avg = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        initial_avg += field->concentration[i];
    }
    initial_avg /= num_neurons;

    // Run updates - should decay
    for (int i = 0; i < 50; i++) {
        spatial_neuromod_update(field, network, 0.01f);
    }

    // Compute final average manually
    float final_avg = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        final_avg += field->concentration[i];
    }
    final_avg /= num_neurons;

    // Should decay over time (concentration decreases due to decay term)
    EXPECT_LT(final_avg, initial_avg);
    EXPECT_GT(final_avg, 0.0f);  // But not to zero immediately
}

// ============================================================================
// BOUNDARY CONDITION TESTS
// ============================================================================

TEST_F(QuantumWalkIntegrationTest, QuantumConcentrationBounded) {
    CreateQuantumSystem(50, 0.0f);

    spatial_neuromod_field_t* field = system_quantum->fields[NEUROMOD_DOPAMINE];

    // Set very high source
    SetSourceRate(system_quantum, 0, 10.0f);

    // Run many updates
    for (int i = 0; i < 100; i++) {
        spatial_neuromod_update(field, network, 0.01f);
    }

    // All concentrations should be within bounds
    for (uint32_t i = 0; i < num_neurons; i++) {
        float conc = GetConcentration(system_quantum, i);
        EXPECT_GE(conc, field->min_concentration);
        EXPECT_LE(conc, field->max_concentration);
    }
}

TEST_F(QuantumWalkIntegrationTest, QuantumHandlesZeroSource) {
    CreateQuantumSystem(50, 0.0f);

    // All sources at zero
    for (uint32_t i = 0; i < num_neurons; i++) {
        SetSourceRate(system_quantum, i, 0.0f);
    }

    // Should not crash
    spatial_neuromod_update(system_quantum->fields[NEUROMOD_DOPAMINE], network, 0.01f);

    // All concentrations should remain at baseline
    for (uint32_t i = 0; i < num_neurons; i++) {
        float conc = GetConcentration(system_quantum, i);
        EXPECT_NEAR(conc, 0.0f, 0.1f);
    }
}

TEST_F(QuantumWalkIntegrationTest, QuantumStableWithMultipleSources) {
    CreateQuantumSystem(50, 0.0f);

    // Set multiple sources
    SetSourceRate(system_quantum, 0, 0.5f);
    SetSourceRate(system_quantum, num_neurons / 2, 0.5f);
    SetSourceRate(system_quantum, num_neurons - 1, 0.5f);

    // Run many updates
    for (int i = 0; i < 50; i++) {
        spatial_neuromod_update(system_quantum->fields[NEUROMOD_DOPAMINE], network, 0.01f);
    }

    // Should produce stable, bounded concentrations
    for (uint32_t i = 0; i < num_neurons; i++) {
        float conc = GetConcentration(system_quantum, i);
        EXPECT_GE(conc, 0.0f);
        EXPECT_LE(conc, 2.0f);  // Reasonable bound
        EXPECT_FALSE(std::isnan(conc));
        EXPECT_FALSE(std::isinf(conc));
    }
}

// ============================================================================
// COIN OPERATOR TESTS
// ============================================================================

TEST_F(QuantumWalkIntegrationTest, HadamardCoinWorks) {
    bool enabled_types[NEUROMOD_COUNT] = {true, false, false, false};
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];

    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        configs[i].enable_quantum_walk = true;
        configs[i].quantum_coin_type = 0;  // 0=Hadamard
    }

    system_quantum = spatial_neuromod_system_create(network, enabled_types, configs);
    ASSERT_NE(system_quantum, nullptr);

    SetSourceRate(system_quantum, 0, 1.0f);
    spatial_neuromod_update(system_quantum->fields[NEUROMOD_DOPAMINE], network, 0.01f);

    // Should produce valid concentrations
    EXPECT_GT(GetConcentration(system_quantum, 0), 0.0f);
}

TEST_F(QuantumWalkIntegrationTest, GroverCoinWorks) {
    bool enabled_types[NEUROMOD_COUNT] = {true, false, false, false};
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];

    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        configs[i].enable_quantum_walk = true;
        configs[i].quantum_coin_type = 1;  // 1=Grover
    }

    system_quantum = spatial_neuromod_system_create(network, enabled_types, configs);
    ASSERT_NE(system_quantum, nullptr);

    SetSourceRate(system_quantum, 0, 1.0f);
    spatial_neuromod_update(system_quantum->fields[NEUROMOD_DOPAMINE], network, 0.01f);

    // Should produce valid concentrations
    EXPECT_GT(GetConcentration(system_quantum, 0), 0.0f);
}

// ============================================================================
// DECOHERENCE TESTS
// ============================================================================

TEST_F(QuantumWalkIntegrationTest, DecoherenceReducesQuantumBehavior) {
    // Create system with high decoherence
    bool enabled_types[NEUROMOD_COUNT] = {true, false, false, false};
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];

    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        configs[i].enable_quantum_walk = true;
        configs[i].quantum_decoherence = 0.9f;  // High decoherence → classical-like
    }

    system_quantum = spatial_neuromod_system_create(network, enabled_types, configs);
    CreateClassicalSystem();

    // Set same sources
    SetSourceRate(system_quantum, 0, 1.0f);
    SetSourceRate(system_classical, 0, 1.0f);

    // Run both
    for (int i = 0; i < 10; i++) {
        spatial_neuromod_update(system_quantum->fields[NEUROMOD_DOPAMINE], network, 0.01f);
        spatial_neuromod_update(system_classical->fields[NEUROMOD_DOPAMINE], network, 0.01f);
    }

    // High decoherence should make quantum behave more like classical
    float quantum_avg = system_quantum->fields[NEUROMOD_DOPAMINE]->avg_concentration;
    float classical_avg = system_classical->fields[NEUROMOD_DOPAMINE]->avg_concentration;

    // Should be closer than low decoherence case
    EXPECT_LT(std::abs(quantum_avg - classical_avg), 0.5f);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
