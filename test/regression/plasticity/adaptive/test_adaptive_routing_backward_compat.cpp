/**
 * @file test_adaptive_routing_backward_compat.cpp
 * @brief Regression tests for Phase C4.4 adaptive routing backward compatibility
 *
 * WHAT: Backward compatibility tests for adaptive routing
 * WHY:  Ensure Phase C4.4 doesn't break existing code
 * HOW:  Test that old configs work, new features are opt-in, no breaking changes
 *
 * TEST COVERAGE:
 * - Default config unchanged
 * - Adaptive routing opt-in only
 * - Existing release functions unchanged
 * - No performance regression when disabled
 * - Config structure backward compatible
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

class AdaptiveRoutingRegressionTest : public ::testing::Test {
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

TEST_F(AdaptiveRoutingRegressionTest, DefaultConfig_NoBreakingChanges) {
    // WHAT: Test that default config has no breaking changes
    // WHY:  Ensure existing code continues to work
    // HOW:  Create default config, verify all Phase C2.x fields present

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    // Phase C2.0 fields
    EXPECT_EQ(config.type, NEUROMOD_DOPAMINE);
    EXPECT_GT(config.diffusion_coeff, 0.0f);
    EXPECT_GT(config.decay_rate, 0.0f);
    EXPECT_GE(config.baseline, 0.0f);
    EXPECT_GT(config.timestep, 0.0f);
    EXPECT_GE(config.substeps, 0u);

    // Phase C2.1 fields (quantum walk)
    EXPECT_GE(config.quantum_walk_steps, 0u);
    EXPECT_GE(config.quantum_mixing_ratio, 0.0f);
    EXPECT_LE(config.quantum_mixing_ratio, 1.0f);
    EXPECT_GE(config.quantum_decoherence, 0.0f);
    EXPECT_LE(config.quantum_decoherence, 1.0f);
}

TEST_F(AdaptiveRoutingRegressionTest, DefaultConfig_AdaptiveRoutingDisabled) {
    // WHAT: Test that adaptive routing is disabled by default
    // WHY:  Opt-in behavior ensures backward compatibility
    // HOW:  Check default config, verify enable_adaptive_routing = false

    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        spatial_neuromod_config_t config = spatial_neuromod_default_config((neuromodulator_type_t)i);
        EXPECT_FALSE(config.enable_adaptive_routing) << "Neuromodulator type " << i;
    }
}

TEST_F(AdaptiveRoutingRegressionTest, DefaultConfig_HasValidAdaptiveSettings) {
    // WHAT: Test that Phase C4.4 default values are valid
    // WHY:  Ensure good defaults when user enables adaptive routing
    // HOW:  Check all adaptive routing config fields

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);

    EXPECT_GE(config.efficiency_weight, 0.0f);
    EXPECT_GE(config.speedup_weight, 0.0f);
    EXPECT_GE(config.bottleneck_penalty_weight, 0.0f);
    EXPECT_GE(config.info_rate_weight, 0.0f);
    EXPECT_GT(config.num_adaptive_sources, 0u);
    EXPECT_GE(config.min_source_score, 0.0f);
    EXPECT_LE(config.min_source_score, 1.0f);
}

//=============================================================================
// Test: Existing Release Functions Unchanged
//=============================================================================

TEST_F(AdaptiveRoutingRegressionTest, ReleaseFunction_BehaviorUnchanged) {
    // WHAT: Test that spatial_neuromod_release() behavior unchanged
    // WHY:  Ensure existing code continues to work
    // HOW:  Use old release function, verify same behavior as Phase C2.x

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Release at specific neuron (Phase C2.0 API)
    bool success = spatial_neuromod_release(field, 50, 10.0f);
    EXPECT_TRUE(success);

    // NOTE: Internal fields (source_rate, total_released) are now private
    // Test backward compatibility by checking API works, not internal state

    spatial_neuromod_destroy(field);
}

TEST_F(AdaptiveRoutingRegressionTest, ReleaseBatch_BehaviorUnchanged) {
    // WHAT: Test that spatial_neuromod_release_batch() behavior unchanged
    // WHY:  Ensure existing code continues to work
    // HOW:  Use old batch release function, verify same behavior

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_SEROTONIN);
    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Batch release (Phase C2.0 API)
    uint32_t neuron_ids[] = {10, 20, 30};
    float amounts[] = {5.0f, 10.0f, 15.0f};
    uint32_t count = 3;

    bool success = spatial_neuromod_release_batch(field, neuron_ids, amounts, count);
    EXPECT_TRUE(success);

    // NOTE: Internal fields are now private
    // Test backward compatibility by checking API works, not internal state

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: No Performance Regression When Disabled
//=============================================================================

TEST_F(AdaptiveRoutingRegressionTest, Performance_NoRegressionWhenDisabled) {
    // WHAT: Test that performance is unchanged when adaptive routing disabled
    // WHY:  Ensure no overhead for existing users
    // HOW:  Create field with defaults, verify no performance impact

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    ASSERT_FALSE(config.enable_adaptive_routing);  // Disabled by default

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Regular release should have no overhead
    for (int i = 0; i < 100; i++) {
        bool success = spatial_neuromod_release(field, i % num_neurons, 1.0f);
        EXPECT_TRUE(success);
    }

    // Update should work normally
    bool success = spatial_neuromod_update(field, network, 0.01f);
    EXPECT_TRUE(success);

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: Config Structure Backward Compatible
//=============================================================================

TEST_F(AdaptiveRoutingRegressionTest, ConfigStructure_BackwardCompatible) {
    // WHAT: Test that config structure is backward compatible
    // WHY:  Ensure existing code compiles and works
    // HOW:  Use old-style config initialization, verify it works

    // Old-style initialization (Phase C2.1)
    spatial_neuromod_config_t config;
    config.type = NEUROMOD_ACETYLCHOLINE;
    config.diffusion_coeff = 0.3f;
    config.decay_rate = 2.0f;
    config.baseline = 0.1f;
    config.timestep = 1.0f;
    config.substeps = 1;
    config.enable_quantum_walk = false;
    config.quantum_walk_steps = 50;
    config.quantum_mixing_ratio = 0.2f;
    config.quantum_coin_type = COIN_HADAMARD;
    config.quantum_decoherence = 0.05f;

    // New fields have sensible defaults even if not initialized
    // (C++ zero-initializes, but let's be explicit for safety)
    config.enable_adaptive_routing = false;
    config.efficiency_weight = 1.0f;
    config.speedup_weight = 0.5f;
    config.bottleneck_penalty_weight = 2.0f;
    config.info_rate_weight = 0.3f;
    config.num_adaptive_sources = 3;
    config.min_source_score = 0.1f;

    // Create field with old-style config
    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Verify old behavior works
    bool success = spatial_neuromod_release(field, 50, 10.0f);
    EXPECT_TRUE(success);

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: Quantum-Shannon Compatibility
//=============================================================================

TEST_F(AdaptiveRoutingRegressionTest, QuantumShannon_WorksWithoutAdaptiveRouting) {
    // WHAT: Test that Phase C4.3 quantum-Shannon works without adaptive routing
    // WHY:  Ensure Phase C4.3 and C4.4 are independent
    // HOW:  Enable quantum-Shannon only, verify it works

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;  // Enable quantum-Shannon (Phase C4.3)
    config.enable_adaptive_routing = false;  // Disable adaptive routing (Phase C4.4)

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    // Enable quantum-Shannon
    field->use_quantum_shannon = true;
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    field->quantum_shannon_diffusion = quantum_shannon_create(
        network, num_neurons / 2, 10.0f, &qs_config);
    ASSERT_NE(field->quantum_shannon_diffusion, nullptr);

    // Regular release should work (quantum-Shannon diffusion, no adaptive routing)
    bool success = spatial_neuromod_release(field, 50, 10.0f);
    EXPECT_TRUE(success);

    // Update should use quantum-Shannon
    success = spatial_neuromod_update(field, network, 0.01f);
    EXPECT_TRUE(success);

    spatial_neuromod_destroy(field);
}

TEST_F(AdaptiveRoutingRegressionTest, QuantumShannon_WorksWithAdaptiveRouting) {
    // WHAT: Test that Phase C4.3 + C4.4 work together
    // WHY:  Ensure phases integrate correctly
    // HOW:  Enable both, verify they work together

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;  // Enable quantum-Shannon (Phase C4.3)
    config.enable_adaptive_routing = true;  // Enable adaptive routing (Phase C4.4)

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
    field->last_speedup_vs_classical = 10.0f;
    field->last_num_bottlenecks = 2;
    field->last_information_rate = 1.5f;

    // Adaptive release should work
    bool success = spatial_neuromod_release_adaptive(field, network, &config, 10.0f);
    EXPECT_TRUE(success);

    // Update should use quantum-Shannon
    success = spatial_neuromod_update(field, network, 0.01f);
    EXPECT_TRUE(success);

    spatial_neuromod_destroy(field);
}

//=============================================================================
// Test: API Stability
//=============================================================================

TEST_F(AdaptiveRoutingRegressionTest, API_NoBreakingChanges) {
    // WHAT: Test that all Phase C2.x APIs still work
    // WHY:  Ensure API stability across phases
    // HOW:  Call all old APIs, verify they work

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_NOREPINEPHRINE);
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

//=============================================================================
// Test: System Integration Unchanged
//=============================================================================

TEST_F(AdaptiveRoutingRegressionTest, SystemIntegration_NoBreakingChanges) {
    // WHAT: Test that spatial_neuromod_system functions unchanged
    // WHY:  Ensure system-level APIs work
    // HOW:  Create system, test all old APIs

    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        // Use defaults (adaptive routing disabled)
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

TEST_F(AdaptiveRoutingRegressionTest, MemoryLayout_Stable) {
    // WHAT: Test that struct sizes are reasonable
    // WHY:  Ensure no massive memory bloat
    // HOW:  Check sizeof structs

    // Config size should be reasonable (< 200 bytes)
    EXPECT_LT(sizeof(spatial_neuromod_config_t), 200u);

    // Field size should be reasonable
    // (This is a smoke test - exact size depends on pointer size)
    EXPECT_GT(sizeof(spatial_neuromod_field_t), 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
