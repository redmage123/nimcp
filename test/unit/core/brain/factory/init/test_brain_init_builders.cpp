/**
 * @file test_brain_init_builders.cpp
 * @brief Unit tests for brain initialization builder functions
 *
 * WHAT: Tests for spike params, network config, and brain config/stats builders
 * WHY:  Ensure all builder functions create valid, properly initialized structures
 * HOW:  Use GoogleTest framework with NULL checks, range validation, and edge cases
 *
 * Functions tested:
 * - nimcp_brain_factory_build_spike_params()
 * - nimcp_brain_factory_build_base_network_config()
 * - nimcp_brain_factory_build_network_config()
 * - nimcp_brain_factory_init_brain_config()
 * - nimcp_brain_factory_init_brain_stats()
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <limits>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

//=============================================================================
// Test Constants
//=============================================================================

static const float FLOAT_EPSILON = 1e-6f;
static const float DEFAULT_K_FACTOR = 0.5f;
static const float DEFAULT_MIN_THRESHOLD = 0.0001f;
static const float DEFAULT_MAX_THRESHOLD = 10.0f;
static const uint32_t DEFAULT_ADAPTATION_WINDOW = 100;

//=============================================================================
// Helper Functions
//=============================================================================

// Mock task strategy for testing
// Headers have their own extern "C" guards
    static float mock_get_learning_rate() { return 0.01f; }

//=============================================================================
// Spike Parameters Builder Tests
//=============================================================================

/**
 * WHAT: Test spike params with typical sparsity value
 * WHY:  Verify basic functionality with common input
 */
TEST(BrainInitBuilders, SpikeParams_TypicalSparsity)
{
    float sparsity = 0.8f;
    adaptive_spike_params_t params = nimcp_brain_factory_build_spike_params(sparsity);

    EXPECT_NEAR(params.k_factor, DEFAULT_K_FACTOR, FLOAT_EPSILON);
    EXPECT_NEAR(params.sparsity_target, sparsity, FLOAT_EPSILON);
    EXPECT_EQ(params.encoding, SPIKE_ENCODING_INTEGER);
    EXPECT_TRUE(params.enable_soft_reset);
    EXPECT_TRUE(params.enable_adaptation);
    EXPECT_EQ(params.adaptation_window, DEFAULT_ADAPTATION_WINDOW);
    EXPECT_NEAR(params.min_threshold, DEFAULT_MIN_THRESHOLD, FLOAT_EPSILON);
    EXPECT_NEAR(params.max_threshold, DEFAULT_MAX_THRESHOLD, FLOAT_EPSILON);
}

/**
 * WHAT: Test spike params with minimum sparsity
 * WHY:  Verify handling of edge case (0.0 sparsity)
 */
TEST(BrainInitBuilders, SpikeParams_MinimumSparsity)
{
    float sparsity = 0.0f;
    adaptive_spike_params_t params = nimcp_brain_factory_build_spike_params(sparsity);

    EXPECT_NEAR(params.sparsity_target, 0.0f, FLOAT_EPSILON);
    EXPECT_NEAR(params.k_factor, DEFAULT_K_FACTOR, FLOAT_EPSILON);
}

/**
 * WHAT: Test spike params with maximum sparsity
 * WHY:  Verify handling of edge case (1.0 sparsity)
 */
TEST(BrainInitBuilders, SpikeParams_MaximumSparsity)
{
    float sparsity = 1.0f;
    adaptive_spike_params_t params = nimcp_brain_factory_build_spike_params(sparsity);

    EXPECT_NEAR(params.sparsity_target, 1.0f, FLOAT_EPSILON);
    EXPECT_NEAR(params.k_factor, DEFAULT_K_FACTOR, FLOAT_EPSILON);
}

/**
 * WHAT: Test spike params threshold range
 * WHY:  Verify min < max threshold
 */
TEST(BrainInitBuilders, SpikeParams_ThresholdRange)
{
    adaptive_spike_params_t params = nimcp_brain_factory_build_spike_params(0.8f);

    EXPECT_LT(params.min_threshold, params.max_threshold)
        << "Min threshold must be less than max threshold";
    EXPECT_GT(params.min_threshold, 0.0f)
        << "Min threshold must be positive";
}

/**
 * WHAT: Test spike params encoding type
 * WHY:  Verify default encoding is INTEGER (fastest)
 */
TEST(BrainInitBuilders, SpikeParams_EncodingType)
{
    adaptive_spike_params_t params = nimcp_brain_factory_build_spike_params(0.8f);

    EXPECT_EQ(params.encoding, SPIKE_ENCODING_INTEGER)
        << "Default encoding should be INTEGER for performance";
}

/**
 * WHAT: Test spike params adaptation settings
 * WHY:  Verify adaptation is enabled by default
 */
TEST(BrainInitBuilders, SpikeParams_AdaptationEnabled)
{
    adaptive_spike_params_t params = nimcp_brain_factory_build_spike_params(0.8f);

    EXPECT_TRUE(params.enable_adaptation)
        << "Adaptation should be enabled by default";
    EXPECT_GT(params.adaptation_window, 0u)
        << "Adaptation window must be positive";
}

/**
 * WHAT: Test spike params soft reset
 * WHY:  Verify soft reset is enabled for biological realism
 */
TEST(BrainInitBuilders, SpikeParams_SoftReset)
{
    adaptive_spike_params_t params = nimcp_brain_factory_build_spike_params(0.8f);

    EXPECT_TRUE(params.enable_soft_reset)
        << "Soft reset should be enabled for biological realism";
}

/**
 * WHAT: Test spike params with various sparsity values
 * WHY:  Verify function works across full range
 */
TEST(BrainInitBuilders, SpikeParams_VariousSparsityValues)
{
    float sparsity_values[] = {0.1f, 0.5f, 0.7f, 0.85f, 0.95f};

    for (float sparsity : sparsity_values) {
        adaptive_spike_params_t params = nimcp_brain_factory_build_spike_params(sparsity);
        EXPECT_NEAR(params.sparsity_target, sparsity, FLOAT_EPSILON)
            << "Sparsity should match input value";
    }
}

//=============================================================================
// Base Network Config Builder Tests
//=============================================================================

/**
 * WHAT: Test base network config with typical parameters
 * WHY:  Verify basic network configuration
 */
TEST(BrainInitBuilders, BaseNetworkConfig_Typical)
{
    uint32_t num_inputs = 10;
    uint32_t num_outputs = 5;
    uint32_t num_neurons = 100;
    ode_integration_method_t method = ODE_EULER;

    network_config_t config = nimcp_brain_factory_build_base_network_config(
        num_inputs, num_outputs, num_neurons, method);

    EXPECT_EQ(config.input_size, num_inputs);
    EXPECT_EQ(config.output_size, num_outputs);
    EXPECT_EQ(config.num_neurons, num_neurons);
    EXPECT_EQ(config.num_layers, 3u);
    EXPECT_EQ(config.integration_method, method);
    EXPECT_NE(config.layer_sizes, nullptr) << "Layer sizes should be allocated";

    if (config.layer_sizes) {
        EXPECT_EQ(config.layer_sizes[0], num_inputs);
        EXPECT_EQ(config.layer_sizes[1], num_neurons);
        EXPECT_EQ(config.layer_sizes[2], num_outputs);
        nimcp_free((void*)config.layer_sizes);
    }
}

/**
 * WHAT: Test base network config plasticity flags
 * WHY:  Verify learning mechanisms are enabled
 */
TEST(BrainInitBuilders, BaseNetworkConfig_PlasticityFlags)
{
    network_config_t config = nimcp_brain_factory_build_base_network_config(
        10, 5, 100, ODE_EULER);

    EXPECT_TRUE(config.enable_stdp) << "STDP should be enabled";
    EXPECT_TRUE(config.enable_hebbian) << "Hebbian learning should be enabled";
    EXPECT_TRUE(config.enable_oja) << "Oja's rule should be enabled";
    EXPECT_TRUE(config.enable_homeostasis) << "Homeostasis should be enabled";

    if (config.layer_sizes) {
        nimcp_free((void*)config.layer_sizes);
    }
}

/**
 * WHAT: Test base network config scalability flags
 * WHY:  Verify BCM and eligibility traces are disabled by default
 */
TEST(BrainInitBuilders, BaseNetworkConfig_ScalabilityFlags)
{
    network_config_t config = nimcp_brain_factory_build_base_network_config(
        10, 5, 100, ODE_EULER);

    EXPECT_FALSE(config.enable_bcm)
        << "BCM should be disabled by default for scalability";
    EXPECT_FALSE(config.enable_eligibility)
        << "Eligibility traces should be disabled by default for scalability";

    if (config.layer_sizes) {
        nimcp_free((void*)config.layer_sizes);
    }
}

/**
 * WHAT: Test base network config with RK4 integration
 * WHY:  Verify different integration methods are supported
 */
TEST(BrainInitBuilders, BaseNetworkConfig_RK4Integration)
{
    network_config_t config = nimcp_brain_factory_build_base_network_config(
        10, 5, 100, ODE_RK4);

    EXPECT_EQ(config.integration_method, ODE_RK4);

    if (config.layer_sizes) {
        nimcp_free((void*)config.layer_sizes);
    }
}

/**
 * WHAT: Test base network config with minimal dimensions
 * WHY:  Verify handling of edge case (1 input, 1 output)
 */
TEST(BrainInitBuilders, BaseNetworkConfig_MinimalDimensions)
{
    network_config_t config = nimcp_brain_factory_build_base_network_config(
        1, 1, 10, ODE_EULER);

    EXPECT_EQ(config.input_size, 1u);
    EXPECT_EQ(config.output_size, 1u);
    EXPECT_NE(config.layer_sizes, nullptr);

    if (config.layer_sizes) {
        nimcp_free((void*)config.layer_sizes);
    }
}

/**
 * WHAT: Test base network config with large dimensions
 * WHY:  Verify handling of large networks
 */
TEST(BrainInitBuilders, BaseNetworkConfig_LargeDimensions)
{
    network_config_t config = nimcp_brain_factory_build_base_network_config(
        1000, 100, 5000, ODE_EULER);

    EXPECT_EQ(config.input_size, 1000u);
    EXPECT_EQ(config.output_size, 100u);
    EXPECT_EQ(config.num_neurons, 5000u);

    if (config.layer_sizes) {
        nimcp_free((void*)config.layer_sizes);
    }
}

/**
 * WHAT: Test base network config layer architecture
 * WHY:  Verify 3-layer architecture (input, hidden, output)
 */
TEST(BrainInitBuilders, BaseNetworkConfig_LayerArchitecture)
{
    network_config_t config = nimcp_brain_factory_build_base_network_config(
        20, 10, 50, ODE_EULER);

    EXPECT_EQ(config.num_layers, 3u) << "Should have 3 layers";
    EXPECT_NE(config.layer_sizes, nullptr);

    if (config.layer_sizes) {
        // Verify layer progression
        EXPECT_EQ(config.layer_sizes[0], config.input_size);
        EXPECT_EQ(config.layer_sizes[1], config.num_neurons);
        EXPECT_EQ(config.layer_sizes[2], config.output_size);
        nimcp_free((void*)config.layer_sizes);
    }
}

//=============================================================================
// Adaptive Network Config Builder Tests
//=============================================================================

/**
 * WHAT: Test adaptive network config with typical parameters
 * WHY:  Verify complete adaptive configuration
 */
TEST(BrainInitBuilders, AdaptiveNetworkConfig_Typical)
{
    adaptive_network_config_t config = nimcp_brain_factory_build_network_config(
        10, 5, 100, 0.8f, ODE_EULER);

    EXPECT_EQ(config.base_config.input_size, 10u);
    EXPECT_EQ(config.base_config.output_size, 5u);
    EXPECT_EQ(config.base_config.num_neurons, 100u);
    EXPECT_NEAR(config.spike_params.sparsity_target, 0.8f, FLOAT_EPSILON);
    EXPECT_FALSE(config.enable_sparsity) << "Sparsity disabled for untrained networks";

    if (config.base_config.layer_sizes) {
        nimcp_free((void*)config.base_config.layer_sizes);
    }
}

/**
 * WHAT: Test adaptive network config sparsity parameters
 * WHY:  Verify sparsity configuration is correct
 */
TEST(BrainInitBuilders, AdaptiveNetworkConfig_SparsityParams)
{
    adaptive_network_config_t config = nimcp_brain_factory_build_network_config(
        10, 5, 100, 0.85f, ODE_EULER);

    EXPECT_FALSE(config.enable_sparsity)
        << "Sparsity should be disabled for regression tests";
    EXPECT_NEAR(config.pruning_threshold, 0.01f, FLOAT_EPSILON);
    EXPECT_EQ(config.update_frequency, 100u);

    if (config.base_config.layer_sizes) {
        nimcp_free((void*)config.base_config.layer_sizes);
    }
}

/**
 * WHAT: Test adaptive network config with various sparsity targets
 * WHY:  Verify sparsity target is passed through correctly
 */
TEST(BrainInitBuilders, AdaptiveNetworkConfig_VariousSparsity)
{
    float sparsity_values[] = {0.7f, 0.8f, 0.85f, 0.9f};

    for (float sparsity : sparsity_values) {
        adaptive_network_config_t config = nimcp_brain_factory_build_network_config(
            10, 5, 100, sparsity, ODE_EULER);

        EXPECT_NEAR(config.spike_params.sparsity_target, sparsity, FLOAT_EPSILON)
            << "Sparsity target should match input";

        if (config.base_config.layer_sizes) {
            nimcp_free((void*)config.base_config.layer_sizes);
        }
    }
}

/**
 * WHAT: Test adaptive network config integration method
 * WHY:  Verify integration method is passed to base config
 */
TEST(BrainInitBuilders, AdaptiveNetworkConfig_IntegrationMethod)
{
    adaptive_network_config_t config_euler = nimcp_brain_factory_build_network_config(
        10, 5, 100, 0.8f, ODE_EULER);
    adaptive_network_config_t config_rk4 = nimcp_brain_factory_build_network_config(
        10, 5, 100, 0.8f, ODE_RK4);

    EXPECT_EQ(config_euler.base_config.integration_method, ODE_EULER);
    EXPECT_EQ(config_rk4.base_config.integration_method, ODE_RK4);

    if (config_euler.base_config.layer_sizes) {
        nimcp_free((void*)config_euler.base_config.layer_sizes);
    }
    if (config_rk4.base_config.layer_sizes) {
        nimcp_free((void*)config_rk4.base_config.layer_sizes);
    }
}

//=============================================================================
// Brain Config Init Tests
//=============================================================================

/**
 * WHAT: Test brain config initialization with typical parameters
 * WHY:  Verify complete brain configuration
 */
TEST(BrainInitBuilders, BrainConfig_Typical)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test_brain", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    EXPECT_EQ(config.size, BRAIN_SIZE_MEDIUM);
    EXPECT_EQ(config.task, BRAIN_TASK_CLASSIFICATION);
    EXPECT_EQ(config.num_inputs, 10u);
    EXPECT_EQ(config.num_outputs, 5u);
    EXPECT_NEAR(config.learning_rate, 0.01f, FLOAT_EPSILON);
    EXPECT_TRUE(config.enable_explanations);
    EXPECT_STREQ(config.task_name, "test_brain");
}

/**
 * WHAT: Test brain config sparsity target
 * WHY:  Verify sparsity matches brain size
 */
TEST(BrainInitBuilders, BrainConfig_SparsityTarget)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_LARGE,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    float expected_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE);
    EXPECT_NEAR(config.sparsity_target, expected_sparsity, FLOAT_EPSILON);
}

/**
 * WHAT: Test brain config ODE integration method
 * WHY:  Verify default is Euler for backward compatibility
 */
TEST(BrainInitBuilders, BrainConfig_ODEMethod)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    EXPECT_EQ(config.neuron_integration, ODE_EULER)
        << "Default should be Euler for backward compatibility";
}

/**
 * WHAT: Test brain config working memory defaults
 * WHY:  Verify Miller's 7±2 working memory capacity
 */
TEST(BrainInitBuilders, BrainConfig_WorkingMemory)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    EXPECT_TRUE(config.enable_working_memory);
    EXPECT_EQ(config.working_memory_capacity, 7u) << "Miller's magic number";
    EXPECT_NEAR(config.working_memory_decay_tau_ms, 1000.0f, FLOAT_EPSILON);
}

/**
 * WHAT: Test brain config theory of mind defaults
 * WHY:  Verify social cognition is enabled
 */
TEST(BrainInitBuilders, BrainConfig_TheoryOfMind)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    EXPECT_TRUE(config.enable_theory_of_mind);
    EXPECT_TRUE(config.enable_empathy_responses);
    EXPECT_TRUE(config.enable_false_belief_tracking);
}

/**
 * WHAT: Test brain config mirror neurons defaults
 * WHY:  Verify social learning is configured
 */
TEST(BrainInitBuilders, BrainConfig_MirrorNeurons)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    EXPECT_TRUE(config.enable_mirror_neurons);
    EXPECT_EQ(config.mirror_neuron_count, 1000u);
    EXPECT_EQ(config.mirror_max_actions, 100u);
    EXPECT_EQ(config.mirror_max_agents, 10u);
    EXPECT_NEAR(config.mirror_learning_rate, 0.01f, FLOAT_EPSILON);
    EXPECT_NEAR(config.mirror_match_threshold, 0.7f, FLOAT_EPSILON);
}

/**
 * WHAT: Test brain config NULL pointer handling
 * WHY:  Verify graceful handling of NULL config
 */
TEST(BrainInitBuilders, BrainConfig_NullConfig)
{
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    // Should not crash
    nimcp_brain_factory_init_brain_config(nullptr, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);
}

/**
 * WHAT: Test brain config NULL strategy handling
 * WHY:  Verify graceful handling of NULL strategy
 */
TEST(BrainInitBuilders, BrainConfig_NullStrategy)
{
    brain_config_t config = {};

    // Should not crash
    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, nullptr);
}

/**
 * WHAT: Test brain config personality defaults
 * WHY:  Verify personality system is configured
 */
TEST(BrainInitBuilders, BrainConfig_Personality)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    EXPECT_TRUE(config.use_random_personality);
    EXPECT_NEAR(config.personality_trait_mean, 0.5f, FLOAT_EPSILON);
    EXPECT_NEAR(config.personality_trait_stddev, 0.15f, FLOAT_EPSILON);
    EXPECT_NEAR(config.female_probability, 1.0f, FLOAT_EPSILON);
}

/**
 * WHAT: Test brain config biological realism defaults
 * WHY:  Verify glial and oscillations settings
 */
TEST(BrainInitBuilders, BrainConfig_BiologicalRealism)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    EXPECT_TRUE(config.enable_glial);
    EXPECT_FALSE(config.enable_oscillations) << "Oscillations opt-in by default";
}

/**
 * WHAT: Test brain config quantum features
 * WHY:  Verify quantum annealing and walk are disabled by default
 */
TEST(BrainInitBuilders, BrainConfig_QuantumFeatures)
{
    brain_config_t config = {};
    task_strategy_t strategy = {};
    strategy.get_learning_rate = mock_get_learning_rate;

    nimcp_brain_factory_init_brain_config(&config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &strategy);

    EXPECT_FALSE(config.enable_quantum_annealing);
    EXPECT_FALSE(config.enable_quantum_walk_diffusion);
}

//=============================================================================
// Brain Stats Init Tests
//=============================================================================

/**
 * WHAT: Test brain stats initialization with typical parameters
 * WHY:  Verify stats are correctly populated
 */
TEST(BrainInitBuilders, BrainStats_Typical)
{
    brain_stats_t stats = {};

    nimcp_brain_factory_init_brain_stats(&stats, "test_brain", BRAIN_SIZE_MEDIUM,
                                        10, 0.01f);

    EXPECT_EQ(stats.size, BRAIN_SIZE_MEDIUM);
    uint32_t expected_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);
    EXPECT_EQ(stats.num_neurons, expected_neurons);
    EXPECT_EQ(stats.num_synapses, expected_neurons * 10);
    EXPECT_EQ(stats.num_active_synapses, stats.num_synapses);
    EXPECT_NEAR(stats.current_learning_rate, 0.01f, FLOAT_EPSILON);
    EXPECT_STREQ(stats.task_name, "test_brain");
}

/**
 * WHAT: Test brain stats for different brain sizes
 * WHY:  Verify neuron count scaling
 */
TEST(BrainInitBuilders, BrainStats_AllSizes)
{
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL,
                            BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};

    for (brain_size_t size : sizes) {
        brain_stats_t stats = {};
        nimcp_brain_factory_init_brain_stats(&stats, "test", size, 10, 0.01f);

        uint32_t expected = nimcp_brain_factory_get_neuron_count(size);
        EXPECT_EQ(stats.num_neurons, expected)
            << "Neuron count should match for size " << size;
        EXPECT_EQ(stats.size, size);
    }
}

/**
 * WHAT: Test brain stats synapse calculation
 * WHY:  Verify num_synapses = num_neurons * num_inputs
 */
TEST(BrainInitBuilders, BrainStats_SynapseCalculation)
{
    brain_stats_t stats = {};
    uint32_t num_inputs = 20;

    nimcp_brain_factory_init_brain_stats(&stats, "test", BRAIN_SIZE_SMALL,
                                        num_inputs, 0.01f);

    uint32_t expected_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL);
    uint32_t expected_synapses = expected_neurons * num_inputs;

    EXPECT_EQ(stats.num_synapses, expected_synapses);
    EXPECT_EQ(stats.num_active_synapses, expected_synapses);
}

/**
 * WHAT: Test brain stats quantum annealing counter
 * WHY:  Verify counter is initialized to zero
 */
TEST(BrainInitBuilders, BrainStats_QuantumCounter)
{
    brain_stats_t stats = {};

    nimcp_brain_factory_init_brain_stats(&stats, "test", BRAIN_SIZE_MEDIUM, 10, 0.01f);

    EXPECT_EQ(stats.quantum_annealing_runs, 0u)
        << "Quantum annealing counter should start at 0";
}

/**
 * WHAT: Test brain stats with various learning rates
 * WHY:  Verify learning rate is stored correctly
 */
TEST(BrainInitBuilders, BrainStats_LearningRates)
{
    float learning_rates[] = {0.001f, 0.01f, 0.05f, 0.1f};

    for (float lr : learning_rates) {
        brain_stats_t stats = {};
        nimcp_brain_factory_init_brain_stats(&stats, "test", BRAIN_SIZE_MEDIUM, 10, lr);

        EXPECT_NEAR(stats.current_learning_rate, lr, FLOAT_EPSILON)
            << "Learning rate should be stored correctly";
    }
}

/**
 * WHAT: Test brain stats task name length
 * WHY:  Verify long names are truncated safely
 */
TEST(BrainInitBuilders, BrainStats_LongTaskName)
{
    brain_stats_t stats = {};
    // Task name buffer is 64 chars, so use a string longer than 63 chars to trigger truncation
    const char* long_name = "this_is_a_very_long_task_name_that_exceeds_the_64_character_buffer_limit_and_should_be_truncated";

    nimcp_brain_factory_init_brain_stats(&stats, long_name, BRAIN_SIZE_MEDIUM, 10, 0.01f);

    // Name should be truncated but null-terminated
    EXPECT_LT(strlen(stats.task_name), strlen(long_name));
    EXPECT_GT(strlen(stats.task_name), 0u);
    // Verify max length is 63 (64 - 1 for null terminator)
    EXPECT_LE(strlen(stats.task_name), 63u);
}

/**
 * WHAT: Test brain stats with edge case inputs
 * WHY:  Verify handling of minimum valid values
 */
TEST(BrainInitBuilders, BrainStats_EdgeCaseInputs)
{
    brain_stats_t stats = {};

    // Minimum inputs (1)
    nimcp_brain_factory_init_brain_stats(&stats, "test", BRAIN_SIZE_TINY, 1, 0.001f);

    uint32_t expected_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);
    EXPECT_EQ(stats.num_neurons, expected_neurons);
    EXPECT_EQ(stats.num_synapses, expected_neurons * 1);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
