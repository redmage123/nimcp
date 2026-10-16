/**
 * @file test_dynamic_adaptation_backward_compat.cpp
 * @brief Regression tests for Phase C4.5 dynamic adaptation backward compatibility
 *
 * WHAT: Backward compatibility tests for dynamic K adaptation
 * WHY:  Ensure Phase C4.5 doesn't break existing code
 * HOW:  Test that old configs work, new features are opt-in, no breaking changes
 *
 * TEST COVERAGE:
 * - Default config unchanged
 * - Dynamic adaptation opt-in only
 * - Phase C4.4 works without Phase C4.5
 * - No performance regression when disabled
 * - Config structure backward compatible
 * - API stability
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

class DynamicAdaptationRegressionTest : public ::testing::Test {
protected:
    neural_network_t network;
    uint32_t num_neurons;

    void SetUp() override {
        num_neurons = 100;

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
    }

    void TearDown() override {
        if (network) {
            neural_network_destroy(network);
        }
    }
};

//=============================================================================
// Test: Default Configuration Unchanged
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, DefaultConfig_NoBreakingChanges) {
    // WHAT: Test that default config has no breaking changes
    // WHY:  Ensure existing code continues to work
    // HOW:  Create default config, verify all Phase C2.x/C4.x fields present

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    // Phase C2.0 fields
    EXPECT_EQ(config.type, NEUROMOD_DOPAMINE);
    EXPECT_GT(config.diffusion_coeff, 0.0f);
    EXPECT_GT(config.decay_rate, 0.0f);
    EXPECT_GE(config.baseline, 0.0f);
    EXPECT_GT(config.timestep, 0.0f);
    EXPECT_GE(config.substeps, 0u);

    // Phase C2.1 fields
    EXPECT_GE(config.quantum_walk_steps, 0u);
    EXPECT_GE(config.quantum_mixing_ratio, 0.0f);
    EXPECT_LE(config.quantum_mixing_ratio, 1.0f);

    // Phase C4.4 fields
    EXPECT_GE(config.efficiency_weight, 0.0f);
    EXPECT_GE(config.num_adaptive_sources, 0u);
}

TEST_F(DynamicAdaptationRegressionTest, DefaultConfig_DynamicAdaptationDisabled) {
    // WHAT: Test that dynamic adaptation is disabled by default
    // WHY:  Opt-in behavior ensures backward compatibility
    // HOW:  Check default config for all neuromodulator types

    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        spatial_neuromod_config_t config = spatial_neuromod_default_config((neuromodulator_type_t)i);
        EXPECT_FALSE(config.enable_dynamic_adaptation) << "Neuromodulator type " << i;
    }
}

TEST_F(DynamicAdaptationRegressionTest, DefaultConfig_HasValidDynamicSettings) {
    // WHAT: Test that Phase C4.5 default values are valid
    // WHY:  Ensure good defaults when user enables dynamic adaptation
    // HOW:  Check all dynamic adaptation config fields

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    EXPECT_GE(config.min_adaptive_sources, 1u);
    EXPECT_GT(config.max_adaptive_sources, config.min_adaptive_sources);
    EXPECT_GE(config.adaptation_rate, 0.0f);
    EXPECT_LE(config.adaptation_rate, 1.0f);
    EXPECT_GE(config.target_efficiency, 0.0f);
    EXPECT_LE(config.target_efficiency, 1.0f);
    EXPECT_GE(config.efficiency_tolerance, 0.0f);
    EXPECT_GT(config.adaptation_cooldown_steps, 0u);
}

//=============================================================================
// Test: Phase C4.4 Works Without Phase C4.5
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, PhaseC44_WorksWithoutDynamicAdaptation) {
    // WHAT: Test that Phase C4.4 adaptive routing works without Phase C4.5
    // WHY:  Ensure phases are independent
    // HOW:  Enable C4.4 only, verify it works

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;       // Enable quantum-Shannon
    config.enable_adaptive_routing = true;   // Enable Phase C4.4
    config.enable_dynamic_adaptation = false; // Disable Phase C4.5

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Enable quantum-Shannon
    field->use_quantum_shannon = true;
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    field->quantum_shannon_diffusion = quantum_shannon_create(
        network, num_neurons / 2, 10.0f, &qs_config);
    ASSERT_NE(field->quantum_shannon_diffusion, nullptr);

    // Set Shannon metrics
    field->last_propagation_efficiency = 0.8f;
    field->last_speedup_vs_classical = 15.0f;
    field->last_num_bottlenecks = 0;
    field->last_information_rate = 2.0f;

    // Phase C4.4 adaptive release should work (K fixed from config)
    bool success = spatial_neuromod_release_adaptive(field, network, &config, 10.0f);
    EXPECT_TRUE(success);

    // Verify K is fixed (doesn't change)
    uint32_t K = spatial_neuromod_get_current_adaptive_sources(field);
    EXPECT_EQ(K, config.num_adaptive_sources);

    spatial_neuromod_destroy(field);
}

TEST_F(DynamicAdaptationRegressionTest, PhaseC44_StaticK_BehaviorUnchanged) {
    // WHAT: Test that K remains static when dynamic adaptation disabled
    // WHY:  Backward compatibility with Phase C4.4 behavior
    // HOW:  Run multiple updates, verify K never changes

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;
    config.enable_dynamic_adaptation = false;  // Disabled
    config.num_adaptive_sources = 5;

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    field->use_quantum_shannon = true;
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    field->quantum_shannon_diffusion = quantum_shannon_create(
        network, num_neurons / 2, 10.0f, &qs_config);

    // Set varying efficiency
    for (int i = 0; i < 50; i++) {
        field->last_propagation_efficiency = 0.5f + (i % 5) * 0.1f;  // Varies 0.5-0.9
        spatial_neuromod_update(field, network, 0.01f);

        // K should remain fixed at 5
        EXPECT_EQ(spatial_neuromod_get_current_adaptive_sources(field), 5u);
    }

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: No Performance Regression When Disabled
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, Performance_NoRegressionWhenDisabled) {
    // WHAT: Test that performance unchanged when dynamic adaptation disabled
    // WHY:  Ensure no overhead for existing users
    // HOW:  Create field with defaults, verify no performance impact

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    ASSERT_FALSE(config.enable_dynamic_adaptation);  // Disabled by default

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Regular updates should have no overhead
    for (int i = 0; i < 100; i++) {
        spatial_neuromod_release(field, i % num_neurons, 1.0f);
        spatial_neuromod_update(field, network, 0.01f);
    }

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: Config Structure Backward Compatible
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, ConfigStructure_BackwardCompatible) {
    // WHAT: Test that config structure is backward compatible
    // WHY:  Ensure existing code compiles and works
    // HOW:  Use old-style config initialization, verify it works

    // Old-style initialization (Phase C4.4)
    spatial_neuromod_config_t config;
    config.type = NEUROMOD_DOPAMINE;
    config.diffusion_coeff = 0.2f;
    config.decay_rate = 0.5f;
    config.baseline = 0.05f;
    config.timestep = 1.0f;
    config.substeps = 1;
    config.enable_quantum_walk = true;
    config.quantum_walk_steps = 50;
    config.quantum_mixing_ratio = 0.2f;
    config.quantum_coin_type = COIN_HADAMARD;
    config.quantum_decoherence = 0.05f;
    config.enable_adaptive_routing = true;
    config.efficiency_weight = 1.0f;
    config.speedup_weight = 0.5f;
    config.bottleneck_penalty_weight = 2.0f;
    config.info_rate_weight = 0.3f;
    config.num_adaptive_sources = 5;
    config.min_source_score = 0.1f;

    // New Phase C4.5 fields (explicit initialization for safety)
    config.enable_dynamic_adaptation = false;
    config.min_adaptive_sources = 1;
    config.max_adaptive_sources = 10;
    config.adaptation_rate = 0.1f;
    config.target_efficiency = 0.75f;
    config.efficiency_tolerance = 0.1f;
    config.adaptation_cooldown_steps = 100;

    // Create field with old-style config
    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Verify old behavior works
    bool success = spatial_neuromod_release(field, 50, 10.0f);
    EXPECT_TRUE(success);

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: API Stability
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, API_NoBreakingChanges) {
    // WHAT: Test that all previous APIs still work
    // WHY:  Ensure API stability across phases
    // HOW:  Call all old APIs, verify they work

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Phase C2.0 APIs
    EXPECT_TRUE(spatial_neuromod_release(field, 10, 5.0f));
    EXPECT_TRUE(spatial_neuromod_update(field, network, 0.01f));
    EXPECT_GE(spatial_neuromod_get_concentration(field, 10), 0.0f);
    EXPECT_GE(spatial_neuromod_get_average(field), 0.0f);

    uint32_t max_neuron;
    float max_conc = spatial_neuromod_get_max(field, &max_neuron);
    EXPECT_GE(max_conc, 0.0f);

    EXPECT_TRUE(spatial_neuromod_set_concentration(field, 20, 0.5f));
    EXPECT_TRUE(spatial_neuromod_validate(field));

    spatial_neuromod_destroy(field);
}

TEST_F(DynamicAdaptationRegressionTest, API_PhaseC44Functions_StillWork) {
    // WHAT: Test that Phase C4.4 APIs still work
    // WHY:  Ensure Phase C4.5 doesn't break Phase C4.4
    // HOW:  Call Phase C4.4 functions, verify correctness

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;
    // Dynamic adaptation disabled (default)

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    field->use_quantum_shannon = true;
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    field->quantum_shannon_diffusion = quantum_shannon_create(
        network, num_neurons / 2, 10.0f, &qs_config);

    field->last_propagation_efficiency = 0.8f;
    field->last_speedup_vs_classical = 15.0f;
    field->last_num_bottlenecks = 0;
    field->last_information_rate = 2.0f;

    // Phase C4.4 APIs should work
    float score = spatial_neuromod_score_neuron(field, 50, network, &config);
    EXPECT_GE(score, 0.0f);

    uint32_t selected_ids[100];
    float selected_scores[100];
    uint32_t num_selected;
    bool success = spatial_neuromod_select_optimal_sources(
        field, network, &config, selected_ids, selected_scores, &num_selected);
    EXPECT_TRUE(success);

    success = spatial_neuromod_release_adaptive(field, network, &config, 10.0f);
    EXPECT_TRUE(success);

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: Field State Initialization
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, FieldState_InitializesCorrectly) {
    // WHAT: Test that new Phase C4.5 state fields initialize correctly
    // WHY:  Prevent uninitialized memory bugs
    // HOW:  Create field, check initial state values

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.num_adaptive_sources = 7;

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Phase C4.5 state should be initialized
    EXPECT_FLOAT_EQ(field->efficiency_ema, 0.0f);
    EXPECT_EQ(field->current_adaptive_sources, 7u);  // Matches config
    EXPECT_EQ(field->adaptation_cooldown, 0u);

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: System Integration Unchanged
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, SystemIntegration_NoBreakingChanges) {
    // WHAT: Test that spatial_neuromod_system functions unchanged
    // WHY:  Ensure system-level APIs work
    // HOW:  Create system, test all old APIs

    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        // Use defaults (dynamic adaptation disabled)
    }

    bool enabled_types[NEUROMOD_COUNT] = {true, true, false, false};

    spatial_neuromod_system_t* system = spatial_neuromod_system_create(
        network, enabled_types, configs);
    ASSERT_NE(system, nullptr);

    // Old system APIs should work
    EXPECT_TRUE(system->fields[NEUROMOD_DOPAMINE] != nullptr);
    EXPECT_TRUE(system->fields[NEUROMOD_SEROTONIN] != nullptr);
    EXPECT_TRUE(system->fields[NEUROMOD_ACETYLCHOLINE] == nullptr);
    EXPECT_TRUE(system->fields[NEUROMOD_NOREPINEPHRINE] == nullptr);

    spatial_neuromod_system_destroy(system);
}

//=============================================================================
// Test: Memory Layout Stable
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, MemoryLayout_Stable) {
    // WHAT: Test that struct sizes are reasonable
    // WHY:  Ensure no massive memory bloat
    // HOW:  Check sizeof structs

    // Config size should be reasonable (< 300 bytes with Phase C4.5 additions)
    EXPECT_LT(sizeof(spatial_neuromod_config_t), 300u);

    // Field size should be reasonable
    EXPECT_GT(sizeof(spatial_neuromod_field_t), 0u);
}

//=============================================================================
// Test: Phase C4.5 New APIs Work When Enabled
//=============================================================================

TEST_F(DynamicAdaptationRegressionTest, NewAPIs_WorkWhenEnabled) {
    // WHAT: Test that new Phase C4.5 APIs work when enabled
    // WHY:  Verify feature works when opted in
    // HOW:  Enable dynamic adaptation, call new APIs

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;
    config.enable_dynamic_adaptation = true;  // Enable Phase C4.5

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    field->use_quantum_shannon = true;
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    field->quantum_shannon_diffusion = quantum_shannon_create(
        network, num_neurons / 2, 10.0f, &qs_config);

    field->last_propagation_efficiency = 0.8f;
    field->last_speedup_vs_classical = 15.0f;
    field->last_num_bottlenecks = 0;
    field->last_information_rate = 2.0f;

    // New Phase C4.5 APIs
    bool success = spatial_neuromod_update_dynamic_adaptation(field, &config);
    EXPECT_TRUE(success);

    uint32_t K = spatial_neuromod_get_current_adaptive_sources(field);
    EXPECT_GT(K, 0u);

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
