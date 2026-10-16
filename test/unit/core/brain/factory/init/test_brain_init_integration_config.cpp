/**
 * @file test_brain_init_integration_config.cpp
 * @brief Integration tests for brain initialization configuration workflows
 *
 * WHAT: End-to-end tests for complete brain configuration workflows
 * WHY:  Ensure all builder functions work together correctly
 * HOW:  Test realistic configuration scenarios and cross-function interactions
 *
 * Integration scenarios tested:
 * - Complete brain creation workflow
 * - Configuration consistency across all components
 * - Multi-size brain configuration
 * - Error recovery and edge cases
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain.h"
#include "nimcp.h"

//=============================================================================
// Test Constants
//=============================================================================

static const float FLOAT_EPSILON = 1e-6f;

//=============================================================================
// Helper Functions
//=============================================================================

// Headers have their own extern "C" guards
    static float integration_mock_learning_rate() { return 0.01f; }

/**
 * WHAT: Helper to create complete configuration
 * WHY:  Reusable setup for integration tests
 */
struct CompleteConfig {
    brain_config_t brain_config;
    brain_stats_t brain_stats;
    adaptive_network_config_t network_config;
    task_strategy_t strategy;

    CompleteConfig() {
        memset(&brain_config, 0, sizeof(brain_config));
        memset(&brain_stats, 0, sizeof(brain_stats));
        memset(&network_config, 0, sizeof(network_config));
        memset(&strategy, 0, sizeof(strategy));
        strategy.get_learning_rate = integration_mock_learning_rate;
    }

    ~CompleteConfig() {
        if (network_config.base_config.layer_sizes) {
            nimcp_free((void*)network_config.base_config.layer_sizes);
            network_config.base_config.layer_sizes = nullptr;
        }
    }
};

//=============================================================================
// Complete Workflow Tests
//=============================================================================

/**
 * WHAT: Test complete configuration workflow for TINY brain
 * WHY:  Verify end-to-end configuration for smallest size
 */
TEST(BrainInitIntegration, CompleteWorkflow_Tiny)
{
    CompleteConfig cfg;
    const char* task_name = "tiny_classifier";
    brain_size_t size = BRAIN_SIZE_TINY;
    uint32_t num_inputs = 5;
    uint32_t num_outputs = 3;

    // Step 1: Get neuron count and sparsity
    uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(size);
    float sparsity = nimcp_brain_factory_get_default_sparsity(size);

    // Step 2: Initialize brain config
    nimcp_brain_factory_init_brain_config(&cfg.brain_config, task_name, size,
                                         BRAIN_TASK_CLASSIFICATION, num_inputs,
                                         num_outputs, &cfg.strategy);

    // Step 3: Initialize brain stats
    nimcp_brain_factory_init_brain_stats(&cfg.brain_stats, task_name, size,
                                        num_inputs, cfg.brain_config.learning_rate);

    // Step 4: Build network config
    cfg.network_config = nimcp_brain_factory_build_network_config(
        num_inputs, num_outputs, num_neurons, sparsity, ODE_EULER);

    // Verify consistency across all configurations
    EXPECT_EQ(cfg.brain_config.size, size);
    EXPECT_EQ(cfg.brain_config.num_inputs, num_inputs);
    EXPECT_EQ(cfg.brain_config.num_outputs, num_outputs);
    EXPECT_NEAR(cfg.brain_config.sparsity_target, sparsity, FLOAT_EPSILON);

    EXPECT_EQ(cfg.brain_stats.size, size);
    EXPECT_EQ(cfg.brain_stats.num_neurons, num_neurons);

    EXPECT_EQ(cfg.network_config.base_config.input_size, num_inputs);
    EXPECT_EQ(cfg.network_config.base_config.output_size, num_outputs);
    EXPECT_EQ(cfg.network_config.base_config.num_neurons, num_neurons);
    EXPECT_NEAR(cfg.network_config.spike_params.sparsity_target, sparsity, FLOAT_EPSILON);
}

/**
 * WHAT: Test complete configuration workflow for MEDIUM brain
 * WHY:  Verify end-to-end configuration for standard size
 */
TEST(BrainInitIntegration, CompleteWorkflow_Medium)
{
    CompleteConfig cfg;
    brain_size_t size = BRAIN_SIZE_MEDIUM;
    uint32_t num_inputs = 20;
    uint32_t num_outputs = 10;

    uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(size);
    float sparsity = nimcp_brain_factory_get_default_sparsity(size);

    nimcp_brain_factory_init_brain_config(&cfg.brain_config, "medium_task", size,
                                         BRAIN_TASK_REGRESSION, num_inputs,
                                         num_outputs, &cfg.strategy);

    nimcp_brain_factory_init_brain_stats(&cfg.brain_stats, "medium_task", size,
                                        num_inputs, cfg.brain_config.learning_rate);

    cfg.network_config = nimcp_brain_factory_build_network_config(
        num_inputs, num_outputs, num_neurons, sparsity, ODE_RK4);

    // Verify RK4 integration method
    EXPECT_EQ(cfg.network_config.base_config.integration_method, ODE_RK4);
    EXPECT_EQ(cfg.brain_config.neuron_integration, ODE_EULER)
        << "Brain config should have Euler by default";
}

/**
 * WHAT: Test configuration consistency across all brain sizes
 * WHY:  Verify scalability and consistency
 */
TEST(BrainInitIntegration, AllSizes_ConfigurationConsistency)
{
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL,
                            BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};

    for (brain_size_t size : sizes) {
        CompleteConfig cfg;
        uint32_t num_inputs = 10;
        uint32_t num_outputs = 5;

        uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(size);
        float sparsity = nimcp_brain_factory_get_default_sparsity(size);

        nimcp_brain_factory_init_brain_config(&cfg.brain_config, "test", size,
                                             BRAIN_TASK_CLASSIFICATION, num_inputs,
                                             num_outputs, &cfg.strategy);

        nimcp_brain_factory_init_brain_stats(&cfg.brain_stats, "test", size,
                                            num_inputs, cfg.brain_config.learning_rate);

        cfg.network_config = nimcp_brain_factory_build_network_config(
            num_inputs, num_outputs, num_neurons, sparsity, ODE_EULER);

        // Verify all configs agree on dimensions
        EXPECT_EQ(cfg.brain_config.num_inputs, num_inputs);
        EXPECT_EQ(cfg.network_config.base_config.input_size, num_inputs);

        EXPECT_EQ(cfg.brain_config.num_outputs, num_outputs);
        EXPECT_EQ(cfg.network_config.base_config.output_size, num_outputs);

        // Verify sparsity consistency
        EXPECT_NEAR(cfg.brain_config.sparsity_target, sparsity, FLOAT_EPSILON);
        EXPECT_NEAR(cfg.network_config.spike_params.sparsity_target, sparsity, FLOAT_EPSILON);

        // Verify neuron count consistency
        EXPECT_EQ(cfg.brain_stats.num_neurons, num_neurons);
        EXPECT_EQ(cfg.network_config.base_config.num_neurons, num_neurons);
    }
}

/**
 * WHAT: Test learning rate propagation through configuration
 * WHY:  Verify learning rate flows correctly from strategy to all configs
 */
TEST(BrainInitIntegration, LearningRate_Propagation)
{
    CompleteConfig cfg;
    float expected_lr = 0.01f;  // From mock_get_learning_rate

    nimcp_brain_factory_init_brain_config(&cfg.brain_config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &cfg.strategy);

    nimcp_brain_factory_init_brain_stats(&cfg.brain_stats, "test", BRAIN_SIZE_MEDIUM,
                                        10, cfg.brain_config.learning_rate);

    // Verify learning rate is consistent
    EXPECT_NEAR(cfg.brain_config.learning_rate, expected_lr, FLOAT_EPSILON);
    EXPECT_NEAR(cfg.brain_stats.current_learning_rate, expected_lr, FLOAT_EPSILON);
}

/**
 * WHAT: Test spike parameters integration with network config
 * WHY:  Verify spike params are correctly embedded in network config
 */
TEST(BrainInitIntegration, SpikeParams_NetworkConfigIntegration)
{
    float sparsity = 0.85f;
    uint32_t num_neurons = 100;

    adaptive_network_config_t config = nimcp_brain_factory_build_network_config(
        10, 5, num_neurons, sparsity, ODE_EULER);

    // Verify spike params are correctly initialized
    EXPECT_NEAR(config.spike_params.sparsity_target, sparsity, FLOAT_EPSILON);
    EXPECT_NEAR(config.spike_params.k_factor, 0.5f, FLOAT_EPSILON);
    EXPECT_EQ(config.spike_params.encoding, SPIKE_ENCODING_INTEGER);
    EXPECT_TRUE(config.spike_params.enable_adaptation);

    // Verify spike params are consistent with network dimensions
    EXPECT_EQ(config.base_config.num_neurons, num_neurons);

    if (config.base_config.layer_sizes) {
        nimcp_free((void*)config.base_config.layer_sizes);
    }
}

/**
 * WHAT: Test layer sizes are properly allocated and initialized
 * WHY:  Verify memory management in network config builder
 */
TEST(BrainInitIntegration, NetworkConfig_LayerSizesAllocation)
{
    uint32_t num_inputs = 15;
    uint32_t num_outputs = 8;
    uint32_t num_neurons = 200;

    adaptive_network_config_t config = nimcp_brain_factory_build_network_config(
        num_inputs, num_outputs, num_neurons, 0.8f, ODE_EULER);

    ASSERT_NE(config.base_config.layer_sizes, nullptr)
        << "Layer sizes should be allocated";

    // Verify layer architecture
    EXPECT_EQ(config.base_config.num_layers, 3u);
    EXPECT_EQ(config.base_config.layer_sizes[0], num_inputs);
    EXPECT_EQ(config.base_config.layer_sizes[1], num_neurons);
    EXPECT_EQ(config.base_config.layer_sizes[2], num_outputs);

    // Test cleanup
    nimcp_free((void*)config.base_config.layer_sizes);
    config.base_config.layer_sizes = nullptr;
}

/**
 * WHAT: Test synapse calculation in brain stats
 * WHY:  Verify synapse count is consistent with neuron count and inputs
 */
TEST(BrainInitIntegration, BrainStats_SynapseConsistency)
{
    brain_size_t size = BRAIN_SIZE_MEDIUM;
    uint32_t num_inputs = 25;

    uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(size);

    brain_stats_t stats = {0};
    nimcp_brain_factory_init_brain_stats(&stats, "test", size, num_inputs, 0.01f);

    uint32_t expected_synapses = num_neurons * num_inputs;
    EXPECT_EQ(stats.num_synapses, expected_synapses);
    EXPECT_EQ(stats.num_active_synapses, expected_synapses)
        << "Initially all synapses are active";
}

/**
 * WHAT: Test task name propagation
 * WHY:  Verify task name is stored in both config and stats
 */
TEST(BrainInitIntegration, TaskName_Propagation)
{
    const char* task_name = "integration_test_task";
    CompleteConfig cfg;

    nimcp_brain_factory_init_brain_config(&cfg.brain_config, task_name, BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &cfg.strategy);

    nimcp_brain_factory_init_brain_stats(&cfg.brain_stats, task_name, BRAIN_SIZE_MEDIUM,
                                        10, cfg.brain_config.learning_rate);

    EXPECT_STREQ(cfg.brain_config.task_name, task_name);
    EXPECT_STREQ(cfg.brain_stats.task_name, task_name);
}

//=============================================================================
// Multi-Configuration Tests
//=============================================================================

/**
 * WHAT: Test multiple configurations with different parameters
 * WHY:  Verify independence of configuration instances
 */
TEST(BrainInitIntegration, MultipleConfigurations_Independence)
{
    // Create two different configurations
    CompleteConfig cfg1, cfg2;

    nimcp_brain_factory_init_brain_config(&cfg1.brain_config, "config1", BRAIN_SIZE_TINY,
                                         BRAIN_TASK_CLASSIFICATION, 5, 3, &cfg1.strategy);

    nimcp_brain_factory_init_brain_config(&cfg2.brain_config, "config2", BRAIN_SIZE_LARGE,
                                         BRAIN_TASK_REGRESSION, 20, 10, &cfg2.strategy);

    // Verify configs are independent
    EXPECT_EQ(cfg1.brain_config.size, BRAIN_SIZE_TINY);
    EXPECT_EQ(cfg2.brain_config.size, BRAIN_SIZE_LARGE);

    EXPECT_EQ(cfg1.brain_config.task, BRAIN_TASK_CLASSIFICATION);
    EXPECT_EQ(cfg2.brain_config.task, BRAIN_TASK_REGRESSION);

    EXPECT_STRNE(cfg1.brain_config.task_name, cfg2.brain_config.task_name);
}

/**
 * WHAT: Test sequential configuration creation
 * WHY:  Verify no state pollution between configs
 */
TEST(BrainInitIntegration, SequentialConfigurations_NoStatePollution)
{
    std::vector<brain_size_t> sizes = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL,
                                       BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};

    for (size_t i = 0; i < sizes.size(); i++) {
        CompleteConfig cfg;
        brain_size_t size = sizes[i];

        uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(size);
        float sparsity = nimcp_brain_factory_get_default_sparsity(size);

        nimcp_brain_factory_init_brain_config(&cfg.brain_config, "test", size,
                                             BRAIN_TASK_CLASSIFICATION, 10, 5, &cfg.strategy);

        // Verify each config is independent
        EXPECT_EQ(cfg.brain_config.size, size);
        EXPECT_NEAR(cfg.brain_config.sparsity_target, sparsity, FLOAT_EPSILON);

        brain_stats_t stats = {0};
        nimcp_brain_factory_init_brain_stats(&stats, "test", size, 10, 0.01f);
        EXPECT_EQ(stats.num_neurons, num_neurons);
    }
}

//=============================================================================
// Advanced Integration Tests
//=============================================================================

/**
 * WHAT: Test integration method variants
 * WHY:  Verify different ODE methods work in network config
 */
TEST(BrainInitIntegration, IntegrationMethods_AllVariants)
{
    ode_integration_method_t methods[] = {ODE_EULER, ODE_RK2, ODE_RK4};

    for (ode_integration_method_t method : methods) {
        adaptive_network_config_t config = nimcp_brain_factory_build_network_config(
            10, 5, 100, 0.8f, method);

        EXPECT_EQ(config.base_config.integration_method, method)
            << "Integration method should be preserved";

        if (config.base_config.layer_sizes) {
            nimcp_free((void*)config.base_config.layer_sizes);
        }
    }
}

/**
 * WHAT: Test sparsity scaling with brain size
 * WHY:  Verify sparsity increases with network size
 */
TEST(BrainInitIntegration, Sparsity_ScalingWithSize)
{
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL,
                            BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};
    float prev_sparsity = 0.0f;

    for (brain_size_t size : sizes) {
        float sparsity = nimcp_brain_factory_get_default_sparsity(size);

        if (prev_sparsity > 0.0f) {
            EXPECT_GT(sparsity, prev_sparsity)
                << "Sparsity should increase with brain size";
        }
        prev_sparsity = sparsity;
    }
}

/**
 * WHAT: Test memory scaling with brain size
 * WHY:  Verify memory requirements scale reasonably
 */
TEST(BrainInitIntegration, Memory_ReasonableScaling)
{
    brain_size_t sizes[] = {BRAIN_SIZE_TINY, BRAIN_SIZE_SMALL,
                            BRAIN_SIZE_MEDIUM, BRAIN_SIZE_LARGE};

    for (size_t i = 1; i < 4; i++) {
        uint32_t neurons_small = nimcp_brain_factory_get_neuron_count(sizes[i-1]);
        uint32_t neurons_large = nimcp_brain_factory_get_neuron_count(sizes[i]);

        float scaling_factor = static_cast<float>(neurons_large) / neurons_small;

        EXPECT_GT(scaling_factor, 1.0f);
        EXPECT_LT(scaling_factor, 100.0f)
            << "Scaling should be reasonable (not exponential)";
    }
}

/**
 * WHAT: Test configuration with extreme input/output dimensions
 * WHY:  Verify handling of edge cases
 */
TEST(BrainInitIntegration, ExtremeDimensions_Handling)
{
    struct TestCase {
        uint32_t inputs;
        uint32_t outputs;
        const char* description;
    };

    TestCase cases[] = {
        {1, 1, "minimal"},
        {1000, 1, "many inputs, one output"},
        {1, 1000, "one input, many outputs"},
        {100, 100, "balanced large"}
    };

    for (const auto& tc : cases) {
        CompleteConfig cfg;
        uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);
        float sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);

        cfg.network_config = nimcp_brain_factory_build_network_config(
            tc.inputs, tc.outputs, num_neurons, sparsity, ODE_EULER);

        EXPECT_EQ(cfg.network_config.base_config.input_size, tc.inputs)
            << "Failed for case: " << tc.description;
        EXPECT_EQ(cfg.network_config.base_config.output_size, tc.outputs)
            << "Failed for case: " << tc.description;
    }
}

/**
 * WHAT: Test plasticity flags consistency
 * WHY:  Verify learning mechanisms are enabled correctly
 */
TEST(BrainInitIntegration, PlasticityFlags_Consistency)
{
    adaptive_network_config_t config = nimcp_brain_factory_build_network_config(
        10, 5, 100, 0.8f, ODE_EULER);

    // Basic plasticity should be enabled
    EXPECT_TRUE(config.base_config.enable_stdp);
    EXPECT_TRUE(config.base_config.enable_hebbian);
    EXPECT_TRUE(config.base_config.enable_oja);
    EXPECT_TRUE(config.base_config.enable_homeostasis);

    // Expensive plasticity should be disabled
    EXPECT_FALSE(config.base_config.enable_bcm);
    EXPECT_FALSE(config.base_config.enable_eligibility);

    if (config.base_config.layer_sizes) {
        nimcp_free((void*)config.base_config.layer_sizes);
    }
}

/**
 * WHAT: Test brain config subsystem defaults
 * WHY:  Verify all cognitive subsystems have correct defaults
 */
TEST(BrainInitIntegration, CognitiveSubsystems_Defaults)
{
    CompleteConfig cfg;

    nimcp_brain_factory_init_brain_config(&cfg.brain_config, "test", BRAIN_SIZE_MEDIUM,
                                         BRAIN_TASK_CLASSIFICATION, 10, 5, &cfg.strategy);

    // Verify key subsystems are configured
    EXPECT_TRUE(cfg.brain_config.enable_working_memory);
    EXPECT_TRUE(cfg.brain_config.enable_theory_of_mind);
    EXPECT_TRUE(cfg.brain_config.enable_mirror_neurons);
    EXPECT_TRUE(cfg.brain_config.enable_explanations);
    EXPECT_TRUE(cfg.brain_config.enable_glial);

    // Verify opt-in features are disabled
    EXPECT_FALSE(cfg.brain_config.enable_oscillations);
    EXPECT_FALSE(cfg.brain_config.enable_quantum_annealing);
}

/**
 * WHAT: Test configuration with all task types
 * WHY:  Verify task type doesn't break configuration
 */
TEST(BrainInitIntegration, AllTaskTypes_Configuration)
{
    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION,
        BRAIN_TASK_CUSTOM
    };

    for (brain_task_t task : tasks) {
        CompleteConfig cfg;

        nimcp_brain_factory_init_brain_config(&cfg.brain_config, "test", BRAIN_SIZE_MEDIUM,
                                             task, 10, 5, &cfg.strategy);

        EXPECT_EQ(cfg.brain_config.task, task);
        EXPECT_EQ(cfg.brain_config.size, BRAIN_SIZE_MEDIUM);
    }
}

/**
 * WHAT: Test configuration data integrity
 * WHY:  Verify no memory corruption or data leaks
 */
TEST(BrainInitIntegration, DataIntegrity_NoCorruption)
{
    const int NUM_ITERATIONS = 100;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        CompleteConfig cfg;
        brain_size_t size = BRAIN_SIZE_MEDIUM;
        uint32_t num_neurons = nimcp_brain_factory_get_neuron_count(size);
        float sparsity = nimcp_brain_factory_get_default_sparsity(size);

        nimcp_brain_factory_init_brain_config(&cfg.brain_config, "integrity_test", size,
                                             BRAIN_TASK_CLASSIFICATION, 10, 5, &cfg.strategy);

        cfg.network_config = nimcp_brain_factory_build_network_config(
            10, 5, num_neurons, sparsity, ODE_EULER);

        // Verify data hasn't been corrupted
        EXPECT_EQ(cfg.brain_config.size, size);
        EXPECT_EQ(cfg.network_config.base_config.num_neurons, num_neurons);
        EXPECT_NE(cfg.network_config.base_config.layer_sizes, nullptr);
    }
}

/**
 * WHAT: Test thread-safe configuration creation
 * WHY:  Verify configurations can be created independently
 */
TEST(BrainInitIntegration, ThreadSafety_IndependentCreation)
{
    // Create multiple configs "simultaneously" (in sequence, simulating threads)
    const int NUM_CONFIGS = 10;
    std::vector<CompleteConfig*> configs;

    for (int i = 0; i < NUM_CONFIGS; i++) {
        CompleteConfig* cfg = new CompleteConfig();
        brain_size_t size = static_cast<brain_size_t>(i % 4);  // Cycle through sizes

        nimcp_brain_factory_init_brain_config(&cfg->brain_config, "test", size,
                                             BRAIN_TASK_CLASSIFICATION, 10, 5, &cfg->strategy);

        configs.push_back(cfg);
    }

    // Verify all configs are valid and independent
    for (int i = 0; i < NUM_CONFIGS; i++) {
        brain_size_t expected_size = static_cast<brain_size_t>(i % 4);
        EXPECT_EQ(configs[i]->brain_config.size, expected_size);
    }

    // Cleanup
    for (auto* cfg : configs) {
        delete cfg;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
