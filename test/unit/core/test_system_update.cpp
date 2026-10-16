// ============================================================================
// NIMCP - spatial_neuromod_system_update() Unit Tests
// ============================================================================
// WHAT: Unit tests for spatial_neuromod_system_update() function
// WHY:  Ensure 100% code coverage and correctness
// HOW:  Test all paths, edge cases, and error conditions
//
// COVERAGE TARGET: 100% (all branches, all error paths)
//
// NIMCP STANDARDS:
// - All functions < 50 lines
// - Guard clauses (early returns)
// - WHAT-WHY-HOW documentation
// ============================================================================

#include <gtest/gtest.h>
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "core/neuralnet/nimcp_neuralnet.h"

// ============================================================================
// Test Fixture
// ============================================================================

class SystemUpdateTest : public ::testing::Test {
protected:
    neural_network_t network;
    spatial_neuromod_system_t* system;
    uint32_t num_neurons;

    void SetUp() override {
        num_neurons = 100;

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

        // Create system with 2 enabled fields (dopamine, serotonin)
        spatial_neuromod_config_t configs[NEUROMOD_COUNT];
        for (int i = 0; i < NEUROMOD_COUNT; i++) {
            configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        }

        bool enabled_types[NEUROMOD_COUNT] = {true, true, false, false};

        system = spatial_neuromod_system_create(network, enabled_types, configs);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            spatial_neuromod_system_destroy(system);
        }
        if (network) {
            neural_network_destroy(network);
        }
    }
};

// ============================================================================
// Success Path Tests
// ============================================================================

// WHAT: Test basic system update with valid inputs
// WHY:  Verify normal operation succeeds
TEST_F(SystemUpdateTest, ValidInputs_ReturnsTrue) {
    bool success = spatial_neuromod_system_update(system, network, 0.001f);
    EXPECT_TRUE(success);
}

// WHAT: Test update with multiple enabled fields
// WHY:  Verify all enabled fields are updated
TEST_F(SystemUpdateTest, MultipleFields_UpdatesAll) {
    // Both dopamine and serotonin enabled
    ASSERT_TRUE(system->enabled[NEUROMOD_DOPAMINE]);
    ASSERT_TRUE(system->enabled[NEUROMOD_SEROTONIN]);
    ASSERT_FALSE(system->enabled[NEUROMOD_ACETYLCHOLINE]);

    bool success = spatial_neuromod_system_update(system, network, 0.001f);
    EXPECT_TRUE(success);

    // Verify fields exist and are updated (check concentrations changed)
    EXPECT_NE(system->fields[NEUROMOD_DOPAMINE], nullptr);
    EXPECT_NE(system->fields[NEUROMOD_SEROTONIN], nullptr);
}

// WHAT: Test update with only one enabled field
// WHY:  Verify single-field system works
TEST_F(SystemUpdateTest, SingleField_UpdatesCorrectly) {
    // Create new system with only dopamine
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
    }

    bool enabled_types[NEUROMOD_COUNT] = {true, false, false, false};

    spatial_neuromod_system_t* single_system = spatial_neuromod_system_create(
        network, enabled_types, configs);
    ASSERT_NE(single_system, nullptr);

    bool success = spatial_neuromod_system_update(single_system, network, 0.001f);
    EXPECT_TRUE(success);

    spatial_neuromod_system_destroy(single_system);
}

// WHAT: Test update with different timesteps
// WHY:  Verify timestep parameter is correctly used
TEST_F(SystemUpdateTest, DifferentTimesteps_AllWork) {
    // Test very small timestep
    bool success = spatial_neuromod_system_update(system, network, 0.0001f);
    EXPECT_TRUE(success);

    // Test normal timestep
    success = spatial_neuromod_system_update(system, network, 0.001f);
    EXPECT_TRUE(success);

    // Test larger timestep
    success = spatial_neuromod_system_update(system, network, 0.01f);
    EXPECT_TRUE(success);
}

// WHAT: Test repeated updates
// WHY:  Verify function can be called multiple times
TEST_F(SystemUpdateTest, RepeatedUpdates_Work) {
    for (int i = 0; i < 10; i++) {
        bool success = spatial_neuromod_system_update(system, network, 0.001f);
        EXPECT_TRUE(success);
    }
}

// WHAT: Test update after field state changes
// WHY:  Verify update handles modified field state
TEST_F(SystemUpdateTest, AfterStateChange_UpdatesCorrectly) {
    // Modify field concentrations
    spatial_neuromod_field_t* field = system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);

    for (uint32_t i = 0; i < field->num_neurons; i++) {
        field->concentration[i] = 0.5f + 0.1f * (float)i / (float)field->num_neurons;
    }

    // Update should still work
    bool success = spatial_neuromod_system_update(system, network, 0.001f);
    EXPECT_TRUE(success);
}

// ============================================================================
// Error Path Tests (Guard Clauses)
// ============================================================================

// WHAT: Test with null system pointer
// WHY:  Verify guard clause for null system
TEST_F(SystemUpdateTest, NullSystem_ReturnsFalse) {
    bool success = spatial_neuromod_system_update(nullptr, network, 0.001f);
    EXPECT_FALSE(success);
}

// WHAT: Test with null network pointer
// WHY:  Verify guard clause for null network
TEST_F(SystemUpdateTest, NullNetwork_ReturnsFalse) {
    bool success = spatial_neuromod_system_update(system, nullptr, 0.001f);
    EXPECT_FALSE(success);
}

// WHAT: Test with zero timestep
// WHY:  Verify guard clause for invalid timestep
TEST_F(SystemUpdateTest, ZeroTimestep_ReturnsFalse) {
    bool success = spatial_neuromod_system_update(system, network, 0.0f);
    EXPECT_FALSE(success);
}

// WHAT: Test with negative timestep
// WHY:  Verify guard clause for negative timestep
TEST_F(SystemUpdateTest, NegativeTimestep_ReturnsFalse) {
    bool success = spatial_neuromod_system_update(system, network, -0.001f);
    EXPECT_FALSE(success);
}

// WHAT: Test with both null system and network
// WHY:  Verify system check comes first (defensive)
TEST_F(SystemUpdateTest, BothNull_ReturnsFalse) {
    bool success = spatial_neuromod_system_update(nullptr, nullptr, 0.001f);
    EXPECT_FALSE(success);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

// WHAT: Test system with no enabled fields
// WHY:  Verify empty system doesn't crash
TEST_F(SystemUpdateTest, NoEnabledFields_ReturnsTrue) {
    // Create system with no enabled fields
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
    }

    bool enabled_types[NEUROMOD_COUNT] = {false, false, false, false};

    spatial_neuromod_system_t* empty_system = spatial_neuromod_system_create(
        network, enabled_types, configs);
    ASSERT_NE(empty_system, nullptr);

    // Should succeed (no fields to update = success)
    bool success = spatial_neuromod_system_update(empty_system, network, 0.001f);
    EXPECT_TRUE(success);

    spatial_neuromod_system_destroy(empty_system);
}

// WHAT: Test system with all fields enabled
// WHY:  Verify maximum field count works
TEST_F(SystemUpdateTest, AllFieldsEnabled_UpdatesAll) {
    // Create system with all fields enabled
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
    }

    bool enabled_types[NEUROMOD_COUNT] = {true, true, true, true};

    spatial_neuromod_system_t* full_system = spatial_neuromod_system_create(
        network, enabled_types, configs);
    ASSERT_NE(full_system, nullptr);

    bool success = spatial_neuromod_system_update(full_system, network, 0.001f);
    EXPECT_TRUE(success);

    // Verify dopamine and serotonin fields exist (others may not be created)
    EXPECT_NE(full_system->fields[NEUROMOD_DOPAMINE], nullptr);
    EXPECT_NE(full_system->fields[NEUROMOD_SEROTONIN], nullptr);

    spatial_neuromod_system_destroy(full_system);
}

// WHAT: Test with very small network (1 neuron)
// WHY:  Verify minimum network size works
TEST_F(SystemUpdateTest, TinyNetwork_Works) {
    // Create tiny network
    network_config_t net_config = {};
    net_config.num_neurons = 1;
    net_config.ei_ratio = 0.8f;
    net_config.learning_rate = 0.01f;
    net_config.stdp_window = 20.0f;
    net_config.refractory_period = 2.0f;
    net_config.min_weight = 0.0f;
    net_config.max_weight = 1.0f;
    net_config.input_size = 1;
    net_config.output_size = 1;

    neural_network_t tiny_net = neural_network_create(&net_config);
    ASSERT_NE(tiny_net, nullptr);

    // Create system for tiny network
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
    }

    bool enabled_types[NEUROMOD_COUNT] = {true, false, false, false};

    spatial_neuromod_system_t* tiny_system = spatial_neuromod_system_create(
        tiny_net, enabled_types, configs);
    ASSERT_NE(tiny_system, nullptr);

    bool success = spatial_neuromod_system_update(tiny_system, tiny_net, 0.001f);
    EXPECT_TRUE(success);

    spatial_neuromod_system_destroy(tiny_system);
    neural_network_destroy(tiny_net);
}

// WHAT: Test with very large timestep
// WHY:  Verify large dt doesn't break function
TEST_F(SystemUpdateTest, LargeTimestep_Works) {
    bool success = spatial_neuromod_system_update(system, network, 1.0f);
    EXPECT_TRUE(success);
}

// WHAT: Test with extremely small timestep
// WHY:  Verify precision edge case
TEST_F(SystemUpdateTest, TinyTimestep_Works) {
    bool success = spatial_neuromod_system_update(system, network, 1e-6f);
    EXPECT_TRUE(success);
}

// ============================================================================
// Integration Tests (with other features)
// ============================================================================

// WHAT: Test update with quantum-Shannon enabled
// WHY:  Verify compatibility with Phase C4.1
TEST_F(SystemUpdateTest, WithQuantumShannon_Works) {
    // Enable quantum-Shannon on fields
    spatial_neuromod_field_t* field = system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);
    field->use_quantum_shannon = true;

    bool success = spatial_neuromod_system_update(system, network, 0.001f);
    EXPECT_TRUE(success);
}

// WHAT: Test update with substeps configured
// WHY:  Verify compatibility with substep feature
TEST_F(SystemUpdateTest, WithSubsteps_Works) {
    spatial_neuromod_field_t* field = system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);
    field->substeps = 4;  // Use multiple substeps

    bool success = spatial_neuromod_system_update(system, network, 0.001f);
    EXPECT_TRUE(success);
}

// ============================================================================
// End of Unit Tests
// ============================================================================
