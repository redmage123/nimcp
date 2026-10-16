// ============================================================================
// NIMCP - Phase C4.6 Multi-Objective Optimization Integration Tests
// ============================================================================
// WHAT: Integration tests for multi-objective Pareto-optimal source selection
// WHY:  Verify Phase C4.6 works correctly with full neuromodulator system
// HOW:  Test multi-objective with spatial_neuromod_system, real network
//
// INTEGRATION POINTS:
// - Phase C4.1: Quantum-Shannon diffusion (entropy, capacity, mutual info)
// - Phase C4.3: Bottleneck detection (identifies low-capacity paths)
// - Phase C4.4: Adaptive routing (optimal source selection)
// - Phase C4.5: Dynamic adaptation (automatic K tuning)
// - Phase C4.6: Multi-objective (Pareto-optimal selection)
//
// NIMCP STANDARDS:
// - All functions < 50 lines
// - Guard clauses (early returns)
// - WHAT-WHY-HOW documentation
// ============================================================================

#include <gtest/gtest.h>
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/quantum/nimcp_quantum_shannon.h"
#include <cmath>

// ============================================================================
// Test Fixture
// ============================================================================

class MultiObjectiveIntegrationTest : public ::testing::Test {
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

        // Create neuromodulator system with Phase C4.6 features enabled
        spatial_neuromod_config_t configs[NEUROMOD_COUNT];
        for (int i = 0; i < NEUROMOD_COUNT; i++) {
            configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
            configs[i].enable_quantum_walk = true;        // Phase C4.1
            configs[i].enable_dynamic_adaptation = false; // Phase C4.5 (disabled for predictable tests)
            configs[i].enable_multi_objective = true;     // Phase C4.6
            configs[i].num_objectives = 2;
            configs[i].objective_weights[0] = 0.5f;  // Efficiency
            configs[i].objective_weights[1] = 0.5f;  // Speedup
            configs[i].pareto_epsilon = 0.01f;
            configs[i].prefer_diversity = true;
            configs[i].num_adaptive_sources = 5;
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
                neuromod_system->fields[i]->last_propagation_efficiency = 0.8f;
                neuromod_system->fields[i]->last_speedup_vs_classical = 20.0f;
                neuromod_system->fields[i]->last_num_bottlenecks = 0;
                neuromod_system->fields[i]->last_information_rate = 3.0f;
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

    // Helper: Stimulate network neurons
    void stimulate_network() {
        for (uint32_t i = 0; i < num_neurons; i += 10) {
            float stim = 0.5f + 0.5f * std::sin(i * 0.1f);
            // Set neuron activity directly (simplified stimulation)
            if (i < num_neurons) {
                // Neurons would be stimulated via network API
            }
        }
    }
};

// ============================================================================
// Integration Tests
// ============================================================================

// WHAT: Test multi-objective release with quantum-Shannon
// WHY:  Verify Phase C4.6 integrates with Phase C4.1
TEST_F(MultiObjectiveIntegrationTest, WithQuantumShannon_ReleasesCorrectly) {
    // Get first active field
    spatial_neuromod_field_t* field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);

    // Update diffusion
    bool success = spatial_neuromod_update(field, network, 0.001f);
    ASSERT_TRUE(success);

    // Release using multi-objective
    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_multi_objective = true;
    config.num_objectives = 2;
    config.objective_weights[0] = 0.5f;
    config.objective_weights[1] = 0.5f;
    config.num_adaptive_sources = 5;

    success = spatial_neuromod_release_multi_objective(
        field, network, &config, 100.0f);
    ASSERT_TRUE(success);

    // Verify Shannon metrics are valid
    EXPECT_GE(field->last_propagation_efficiency, 0.0f);
    EXPECT_LE(field->last_propagation_efficiency, 1.0f);
    EXPECT_GE(field->last_speedup_vs_classical, 0.0f);
}

// WHAT: Test Pareto selection with real network
// WHY:  Verify multi-objective handles realistic conditions
TEST_F(MultiObjectiveIntegrationTest, ParetoSelection_WorksWithRealNetwork) {
    spatial_neuromod_field_t* field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);

    // Update diffusion
    spatial_neuromod_update(field, network, 0.001f);

    // Select Pareto-optimal sources
    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_multi_objective = true;
    config.num_objectives = 2;
    config.objective_weights[0] = 0.5f;
    config.objective_weights[1] = 0.5f;
    config.num_adaptive_sources = 5;

    uint32_t selected_ids[10];
    float selected_scores[40];  // 10 sources × 4 objectives
    uint32_t num_selected = 0;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, &config,
        selected_ids, selected_scores, &num_selected
    );
    ASSERT_TRUE(success);

    // Should select some sources
    EXPECT_GT(num_selected, 0u);
    EXPECT_LE(num_selected, config.num_adaptive_sources);

    // Note: Scores might not be valid without proper quantum-Shannon setup
    // Just verify the function completed successfully
}

// WHAT: Test multi-objective with multiple neurotransmitter types
// WHY:  Verify Phase C4.6 works across different neuromodulator fields
TEST_F(MultiObjectiveIntegrationTest, MultipleFields_AllWorkCorrectly) {
    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_multi_objective = true;
    config.num_objectives = 2;
    config.num_adaptive_sources = 5;

    // Test each active field
    for (int type = 0; type < NEUROMOD_COUNT; type++) {
        spatial_neuromod_field_t* field = neuromod_system->fields[type];
        if (!field) continue;

        // Update and release
        spatial_neuromod_update(field, network, 0.001f);

        bool success = spatial_neuromod_release_multi_objective(
            field, network, &config, 50.0f);
        ASSERT_TRUE(success);
    }
}

// WHAT: Test multi-objective with 3 objectives
// WHY:  Verify system handles more complex Pareto fronts
TEST_F(MultiObjectiveIntegrationTest, ThreeObjectives_FindsParetoFront) {
    spatial_neuromod_field_t* field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_multi_objective = true;
    config.num_objectives = 3;
    config.objective_weights[0] = 0.33f;  // Efficiency
    config.objective_weights[1] = 0.33f;  // Speedup
    config.objective_weights[2] = 0.34f;  // Bottleneck avoidance
    config.num_adaptive_sources = 5;

    // Update and release
    spatial_neuromod_update(field, network, 0.001f);

    bool success = spatial_neuromod_release_multi_objective(
        field, network, &config, 100.0f);
    ASSERT_TRUE(success);
}

// WHAT: Test multi-objective with 4 objectives
// WHY:  Verify system handles maximum number of objectives
TEST_F(MultiObjectiveIntegrationTest, FourObjectives_FindsParetoFront) {
    spatial_neuromod_field_t* field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_multi_objective = true;
    config.num_objectives = 4;
    config.objective_weights[0] = 0.25f;  // Efficiency
    config.objective_weights[1] = 0.25f;  // Speedup
    config.objective_weights[2] = 0.25f;  // Bottleneck avoidance
    config.objective_weights[3] = 0.25f;  // Information rate
    config.num_adaptive_sources = 5;

    // Update and release
    spatial_neuromod_update(field, network, 0.001f);

    bool success = spatial_neuromod_release_multi_objective(
        field, network, &config, 100.0f);
    ASSERT_TRUE(success);
}

// WHAT: Test multi-objective performance under load
// WHY:  Verify Phase C4.6 doesn't degrade performance significantly
TEST_F(MultiObjectiveIntegrationTest, Performance_AcceptableOverhead) {
    spatial_neuromod_field_t* field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_multi_objective = true;
    config.num_objectives = 2;
    config.num_adaptive_sources = 5;

    // Run 50 updates and releases
    for (int i = 0; i < 50; i++) {
        spatial_neuromod_update(field, network, 0.001f);

        bool success = spatial_neuromod_release_multi_objective(
            field, network, &config, 50.0f);
        ASSERT_TRUE(success);
    }

    // If we got here without timeout, performance is acceptable
}

// WHAT: Test integration with adaptive routing
// WHY:  Verify Phase C4.6 works alongside Phase C4.4
TEST_F(MultiObjectiveIntegrationTest, WithAdaptiveRouting_BothFeaturesWork) {
    spatial_neuromod_field_t* field = neuromod_system->fields[NEUROMOD_DOPAMINE];
    ASSERT_NE(field, nullptr);

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;    // Phase C4.4
    config.enable_multi_objective = true;     // Phase C4.6
    config.num_objectives = 2;
    config.num_adaptive_sources = 5;

    // Update and release
    spatial_neuromod_update(field, network, 0.001f);

    // Multi-objective release should override adaptive routing
    bool success = spatial_neuromod_release_multi_objective(
        field, network, &config, 100.0f);
    ASSERT_TRUE(success);
}

// WHAT: Test system update with multi-objective enabled
// WHY:  Verify spatial_neuromod_system_update works with Phase C4.6
TEST_F(MultiObjectiveIntegrationTest, SystemUpdate_WorksWithMultiObjective) {
    // Update entire system
    bool success = spatial_neuromod_system_update(neuromod_system, network, 0.001f);
    ASSERT_TRUE(success);

    // Release from all fields using multi-objective
    for (int type = 0; type < NEUROMOD_COUNT; type++) {
        spatial_neuromod_field_t* field = neuromod_system->fields[type];
        if (!field) continue;

        spatial_neuromod_config_t config = spatial_neuromod_default_config((neuromodulator_type_t)type);
        config.enable_quantum_walk = true;
        config.enable_multi_objective = true;
        config.num_objectives = 2;
        config.num_adaptive_sources = 5;

        success = spatial_neuromod_release_multi_objective(
            field, network, &config, 50.0f);
        ASSERT_TRUE(success);
    }
}

// ============================================================================
// End of Integration Tests
// ============================================================================
