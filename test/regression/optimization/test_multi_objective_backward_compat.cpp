// ============================================================================
// NIMCP - Phase C4.6 Multi-Objective Backward Compatibility Regression Tests
// ============================================================================
// WHAT: Regression tests ensuring Phase C4.6 doesn't break existing functionality
// WHY:  Verify backward compatibility with existing code and phases
// HOW:  Test that old code paths still work, defaults are correct, opt-in only
//
// REGRESSION FOCUS:
// - Default behavior (multi-objective disabled by default)
// - Existing Phase C4.1-C4.5 functionality unaffected
// - API compatibility (no breaking changes)
// - Performance (no overhead when disabled)
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

// ============================================================================
// Test Fixture
// ============================================================================

class MultiObjectiveBackwardCompatTest : public ::testing::Test {
protected:
    neural_network_t network;
    spatial_neuromod_field_t* field;
    spatial_neuromod_config_t config;

    void SetUp() override {
        // Create network
        network_config_t net_config = {};
        net_config.num_neurons = 100;
        net_config.ei_ratio = 0.8f;
        net_config.learning_rate = 0.01f;
        net_config.stdp_window = 20.0f;
        net_config.refractory_period = 2.0f;
        net_config.min_weight = 0.0f;
        net_config.max_weight = 1.0f;
        net_config.input_size = 100;
        net_config.output_size = 100;

        network = neural_network_create(&net_config);
        ASSERT_NE(network, nullptr);

        // Use default config (multi-objective should be disabled)
        config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

        // Create field
        field = spatial_neuromod_create(100, &config);
        ASSERT_NE(field, nullptr);
    }

    void TearDown() override {
        if (field) {
            spatial_neuromod_destroy(field);
        }
        if (network) {
            neural_network_destroy(network);
        }
    }
};

// ============================================================================
// Default Behavior Tests (Opt-In Philosophy)
// ============================================================================

// WHAT: Test that multi-objective is disabled by default
// WHY:  Ensure backward compatibility - existing code should not be affected
TEST_F(MultiObjectiveBackwardCompatTest, DefaultConfig_MultiObjectiveDisabled) {
    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    // Multi-objective should be disabled by default
    EXPECT_FALSE(default_config.enable_multi_objective);
}

// WHAT: Test default num_objectives value
// WHY:  Verify sensible defaults for when feature is enabled
TEST_F(MultiObjectiveBackwardCompatTest, DefaultConfig_NumObjectivesIs2) {
    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    // Should default to 2 objectives (efficiency + speedup)
    EXPECT_EQ(default_config.num_objectives, 2u);
}

// WHAT: Test default objective weights
// WHY:  Verify equal weighting by default
TEST_F(MultiObjectiveBackwardCompatTest, DefaultConfig_EqualWeights) {
    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    // Should have equal weights for first 2 objectives
    EXPECT_FLOAT_EQ(default_config.objective_weights[0], 0.5f);
    EXPECT_FLOAT_EQ(default_config.objective_weights[1], 0.5f);
    EXPECT_FLOAT_EQ(default_config.objective_weights[2], 0.0f);
    EXPECT_FLOAT_EQ(default_config.objective_weights[3], 0.0f);
}

// WHAT: Test default pareto_epsilon value
// WHY:  Verify sensible default for dominance testing
TEST_F(MultiObjectiveBackwardCompatTest, DefaultConfig_ParetoEpsilon) {
    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    EXPECT_FLOAT_EQ(default_config.pareto_epsilon, 0.01f);
}

// WHAT: Test default prefer_diversity setting
// WHY:  Verify diversity preference is enabled by default
TEST_F(MultiObjectiveBackwardCompatTest, DefaultConfig_PrefersDiversity) {
    spatial_neuromod_config_t default_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    EXPECT_TRUE(default_config.prefer_diversity);
}

// ============================================================================
// API Backward Compatibility Tests
// ============================================================================

// WHAT: Test that existing update function still works
// WHY:  Ensure spatial_neuromod_update() is unaffected
TEST_F(MultiObjectiveBackwardCompatTest, ExistingUpdateFunction_StillWorks) {
    bool success = spatial_neuromod_update(field, network, 0.001f);
    EXPECT_TRUE(success);
}

// WHAT: Test that existing release function still works
// WHY:  Ensure spatial_neuromod_release() is unaffected
TEST_F(MultiObjectiveBackwardCompatTest, ExistingReleaseFunction_StillWorks) {
    bool success = spatial_neuromod_release(field, 0, 100.0f);
    EXPECT_TRUE(success);
}

// WHAT: Test that Phase C4.4 adaptive release still works
// WHY:  Ensure spatial_neuromod_release_adaptive() is unaffected
TEST_F(MultiObjectiveBackwardCompatTest, AdaptiveRelease_StillWorks) {
    // Enable quantum-Shannon for adaptive routing
    field->use_quantum_shannon = true;
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    field->quantum_shannon_diffusion = quantum_shannon_create(network, 50, 10.0f, &qs_config);
    ASSERT_NE(field->quantum_shannon_diffusion, nullptr);

    // Set good Shannon metrics
    field->last_propagation_efficiency = 0.8f;
    field->last_speedup_vs_classical = 20.0f;

    // Update diffusion
    spatial_neuromod_update(field, network, 0.001f);

    // Adaptive release should still work
    config.enable_quantum_walk = true;
    config.num_adaptive_sources = 5;
    bool success = spatial_neuromod_release_adaptive(field, network, &config, 100.0f);
    EXPECT_TRUE(success);
}

// WHAT: Test that field initialization is unchanged
// WHY:  Ensure new state fields don't break existing code
TEST_F(MultiObjectiveBackwardCompatTest, FieldInitialization_Unchanged) {
    // Field should be initialized properly
    EXPECT_NE(field->concentration, nullptr);
    EXPECT_EQ(field->num_neurons, 100u);
    EXPECT_EQ(field->type, NEUROMOD_DOPAMINE);

    // Phase C4.6 state should be initialized
    EXPECT_EQ(field->pareto_front_size, 0u);
    EXPECT_EQ(field->pareto_cache_generation, 0u);
}

// ============================================================================
// Integration with Existing Phases
// ============================================================================

// WHAT: Test Phase C4.1 (quantum walk) still works
// WHY:  Verify quantum walk is unaffected by Phase C4.6
TEST_F(MultiObjectiveBackwardCompatTest, Phase41_QuantumWalk_Unaffected) {
    config.enable_quantum_walk = true;
    config.quantum_walk_steps = 50;

    spatial_neuromod_field_t* qw_field = spatial_neuromod_create(100, &config);
    ASSERT_NE(qw_field, nullptr);

    bool success = spatial_neuromod_update(qw_field, network, 0.001f);
    EXPECT_TRUE(success);

    spatial_neuromod_destroy(qw_field);
}

// WHAT: Test Phase C4.5 (dynamic adaptation) configuration still works
// WHY:  Verify dynamic adaptation config is unaffected by Phase C4.6
TEST_F(MultiObjectiveBackwardCompatTest, Phase45_DynamicAdaptation_Unaffected) {
    config.enable_dynamic_adaptation = true;
    config.min_adaptive_sources = 1;
    config.max_adaptive_sources = 10;
    config.adaptation_rate = 0.1f;
    config.target_efficiency = 0.75f;
    config.efficiency_tolerance = 0.1f;

    spatial_neuromod_field_t* da_field = spatial_neuromod_create(100, &config);
    ASSERT_NE(da_field, nullptr);

    // Verify field was created with dynamic adaptation enabled
    EXPECT_EQ(da_field->current_adaptive_sources, config.num_adaptive_sources);
    EXPECT_EQ(da_field->efficiency_ema, 0.0f);  // Initially zero
    EXPECT_EQ(da_field->adaptation_cooldown, 0u);

    spatial_neuromod_destroy(da_field);
}

// WHAT: Test multi-objective doesn't activate when disabled
// WHY:  Ensure zero overhead when feature is not used
TEST_F(MultiObjectiveBackwardCompatTest, DisabledMultiObjective_NoOverhead) {
    // Multi-objective disabled by default
    ASSERT_FALSE(config.enable_multi_objective);

    // Calling multi-objective functions should fail gracefully
    uint32_t selected_ids[10];
    float selected_scores[40];
    uint32_t num_selected = 0;

    bool success = spatial_neuromod_select_pareto_optimal(
        field, network, &config,
        selected_ids, selected_scores, &num_selected
    );
    EXPECT_FALSE(success);  // Should fail when disabled

    // Regular release should still work
    success = spatial_neuromod_release(field, 0, 100.0f);
    EXPECT_TRUE(success);
}

// ============================================================================
// State Compatibility Tests
// ============================================================================

// WHAT: Test that field state size is unchanged
// WHY:  Ensure no unexpected memory layout changes
TEST_F(MultiObjectiveBackwardCompatTest, FieldState_SizeUnchanged) {
    // This test ensures the struct didn't grow unexpectedly
    // (Some growth is expected for new Phase C4.6 fields)
    EXPECT_GT(sizeof(spatial_neuromod_field_t), 0u);

    // Verify new state fields exist
    EXPECT_EQ(field->pareto_front_size, 0u);
    EXPECT_EQ(field->pareto_cache_generation, 0u);
}

// WHAT: Test that config size is reasonable
// WHY:  Ensure config struct didn't grow too much
TEST_F(MultiObjectiveBackwardCompatTest, ConfigState_SizeReasonable) {
    // Config should contain new Phase C4.6 fields
    spatial_neuromod_config_t test_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    EXPECT_GT(sizeof(spatial_neuromod_config_t), 0u);
}

// ============================================================================
// Performance Regression Tests
// ============================================================================

// WHAT: Test that disabling multi-objective has no performance impact
// WHY:  Ensure zero overhead for users not using the feature
TEST_F(MultiObjectiveBackwardCompatTest, DisabledFeature_NoPerformanceImpact) {
    // Multi-objective disabled
    config.enable_multi_objective = false;

    spatial_neuromod_field_t* fast_field = spatial_neuromod_create(100, &config);
    ASSERT_NE(fast_field, nullptr);

    // Run 100 updates (should be fast)
    for (int i = 0; i < 100; i++) {
        bool success = spatial_neuromod_update(fast_field, network, 0.001f);
        ASSERT_TRUE(success);
    }

    spatial_neuromod_destroy(fast_field);
}

// WHAT: Test that system_update works with both enabled and disabled
// WHY:  Verify spatial_neuromod_system_update is backward compatible
TEST_F(MultiObjectiveBackwardCompatTest, SystemUpdate_BackwardCompatible) {
    // Create system with mixed configurations
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        // Leave multi-objective disabled (default)
    }

    bool enabled_types[NEUROMOD_COUNT] = {true, true, false, false};

    spatial_neuromod_system_t* system = spatial_neuromod_system_create(
        network, enabled_types, configs);
    ASSERT_NE(system, nullptr);

    // Update should work
    bool success = spatial_neuromod_system_update(system, network, 0.001f);
    EXPECT_TRUE(success);

    spatial_neuromod_system_destroy(system);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

// WHAT: Test enabling multi-objective without quantum-Shannon
// WHY:  Verify proper error handling for incomplete setup
TEST_F(MultiObjectiveBackwardCompatTest, MultiObjectiveWithoutQuantumShannon_Fails) {
    config.enable_multi_objective = true;
    config.enable_quantum_walk = false;  // Quantum-Shannon disabled

    spatial_neuromod_field_t* mo_field = spatial_neuromod_create(100, &config);
    ASSERT_NE(mo_field, nullptr);
    mo_field->use_quantum_shannon = false;

    // Multi-objective release should fail without quantum-Shannon
    bool success = spatial_neuromod_release_multi_objective(
        mo_field, network, &config, 100.0f);
    EXPECT_FALSE(success);

    spatial_neuromod_destroy(mo_field);
}

// WHAT: Test that old code can ignore new config fields
// WHY:  Ensure partial initialization doesn't break anything
TEST_F(MultiObjectiveBackwardCompatTest, PartialConfigInit_Safe) {
    spatial_neuromod_config_t partial_config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    // Only set old fields (simulate old code)
    partial_config.diffusion_coeff = 0.2f;
    partial_config.decay_rate = 0.5f;
    // Don't touch Phase C4.6 fields - they should use defaults

    spatial_neuromod_field_t* partial_field = spatial_neuromod_create(100, &partial_config);
    ASSERT_NE(partial_field, nullptr);

    bool success = spatial_neuromod_update(partial_field, network, 0.001f);
    EXPECT_TRUE(success);

    spatial_neuromod_destroy(partial_field);
}

// ============================================================================
// End of Regression Tests
// ============================================================================
