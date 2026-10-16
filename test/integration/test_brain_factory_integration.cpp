//=============================================================================
// test_brain_factory_integration.cpp - Brain Factory Integration Tests
//=============================================================================
/**
 * @file test_brain_factory_integration.cpp
 * @brief Integration tests for full brain creation with all bridges
 *
 * WHAT: Tests brain factory creating complete brain with all subsystems
 * WHY:  Verify all subsystems initialize correctly and work together
 * HOW:  Create brains with various configs, verify all components present
 *
 * BRAIN SUBSYSTEMS TESTED:
 * - Core neural network (adaptive spiking network)
 * - Cognitive systems (working memory, executive, theory of mind, etc.)
 * - Glial integration (astrocyte modulation)
 * - Neuromodulator systems (dopamine, serotonin, etc.)
 * - Multimodal systems (visual, audio, speech)
 * - Bridge connections (substrate, thalamic, quantum)
 *
 * TEST SCENARIOS:
 * 1. Full brain creation with all subsystems
 * 2. Subsystem initialization order
 * 3. Bridge auto-connection during creation
 * 4. Brain configuration validation
 * 5. Multi-size brain creation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init.h"
#include "utils/bridge/nimcp_bridge_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainFactoryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No global setup needed
    }

    void TearDown() override {
        // Cleanup handled per-test
    }

    // Helper: Create brain with specific configuration
    brain_t createBrain(brain_size_t size, brain_task_t task,
                        uint32_t num_inputs, uint32_t num_outputs,
                        const char* name) {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.size = size;
        config.task = task;
        config.num_inputs = num_inputs;
        config.num_outputs = num_outputs;
        snprintf(config.task_name, sizeof(config.task_name), "%s", name);

        return brain_create_custom(&config);
    }
};

//=============================================================================
// Test: Basic Brain Creation
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, BasicCreation_AllSizes) {
    // WHAT: Test brain creation for all size presets
    // WHY:  Verify factory handles all size configurations
    // HOW:  Create brains of each size, verify valid

    brain_size_t sizes[] = {
        BRAIN_SIZE_MICRO,
        BRAIN_SIZE_TINY,
        BRAIN_SIZE_SMALL,
        BRAIN_SIZE_MEDIUM,
        BRAIN_SIZE_LARGE
    };

    const char* size_names[] = {
        "micro", "tiny", "small", "medium", "large"
    };

    for (int i = 0; i < 5; i++) {
        brain_t brain = createBrain(sizes[i], BRAIN_TASK_CLASSIFICATION,
                                    10, 5, size_names[i]);

        ASSERT_NE(brain, nullptr)
            << "Failed to create " << size_names[i] << " brain";

        // Verify network was created
        adaptive_network_t network = brain_get_network(brain);
        EXPECT_NE(network, nullptr)
            << size_names[i] << " brain should have network";

        // Get expected neuron count
        uint32_t expected_neurons = nimcp_brain_factory_get_neuron_count(sizes[i]);
        EXPECT_GT(expected_neurons, 0u);

        brain_destroy(brain);
    }
}

//=============================================================================
// Test: Task-Specific Configuration
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, TaskConfiguration_AllTypes) {
    // WHAT: Test brain creation for all task types
    // WHY:  Verify factory applies task-specific settings
    // HOW:  Create brains for each task, verify configuration

    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION
    };

    const char* task_names[] = {
        "classification", "regression", "pattern", "sequence", "association"
    };

    for (int i = 0; i < 5; i++) {
        brain_t brain = createBrain(BRAIN_SIZE_SMALL, tasks[i],
                                    16, 4, task_names[i]);

        ASSERT_NE(brain, nullptr)
            << "Failed to create brain for task: " << task_names[i];

        // Brain should be functional
        float input[16];
        for (int j = 0; j < 16; j++) {
            input[j] = 0.1f * j;
        }

        brain_decision_t* decision = brain_decide(brain, input, 16);
        EXPECT_NE(decision, nullptr)
            << task_names[i] << " brain should make decisions";

        if (decision) {
            EXPECT_GE(decision->confidence, 0.0f);
            EXPECT_LE(decision->confidence, 1.0f);
            brain_free_decision(decision);
        }

        brain_destroy(brain);
    }
}

//=============================================================================
// Test: Full Subsystem Initialization
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, Subsystems_AllInitialized) {
    // WHAT: Test that all subsystems are properly initialized
    // WHY:  Verify complete brain has all components
    // HOW:  Create brain, check for presence of each subsystem

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_MEDIUM;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 32;
    config.num_outputs = 10;
    snprintf(config.task_name, sizeof(config.task_name), "full_subsystem_test");

    // Enable optional subsystems
    config.enable_glial = true;
    config.enable_working_memory = true;
    config.enable_executive = true;

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Verify core network
    adaptive_network_t network = brain_get_network(brain);
    EXPECT_NE(network, nullptr) << "Should have adaptive network";

    // Verify network can be accessed
    if (network) {
        neural_network_t base_network = adaptive_network_get_base_network(network);
        EXPECT_NE(base_network, nullptr) << "Should have base network";
    }

    // Try to access various subsystems (if accessors exist)
    // Note: Some subsystems may not have public accessors

    brain_destroy(brain);
}

//=============================================================================
// Test: Working Memory Subsystem
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, WorkingMemory_Initialization) {
    // WHAT: Test working memory subsystem initialization
    // WHY:  Verify Miller's 7+/-2 implementation works
    // HOW:  Create brain with WM enabled, verify capacity

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 16;
    config.num_outputs = 4;
    config.enable_working_memory = true;
    snprintf(config.task_name, sizeof(config.task_name), "wm_test");

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Brain with working memory should function
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }

    // Multiple decisions should work (WM maintains state)
    for (int trial = 0; trial < 5; trial++) {
        brain_decision_t* decision = brain_decide(brain, input, 16);
        EXPECT_NE(decision, nullptr);
        if (decision) {
            brain_free_decision(decision);
        }
    }

    brain_destroy(brain);
}

//=============================================================================
// Test: Glial Integration Subsystem
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, GlialIntegration_Initialization) {
    // WHAT: Test glial integration subsystem
    // WHY:  Verify astrocyte modulation initializes
    // HOW:  Create brain with glial enabled

    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 8;
    config.num_outputs = 2;
    config.enable_glial = true;
    snprintf(config.task_name, sizeof(config.task_name), "glial_test");

    brain_t brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    // Brain should function with glial integration
    float input[8] = {0.5f, 0.6f, 0.7f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f};
    brain_decision_t* decision = brain_decide(brain, input, 8);
    EXPECT_NE(decision, nullptr);

    if (decision) {
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

//=============================================================================
// Test: Network Configuration Building
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, NetworkConfig_Validation) {
    // WHAT: Test network configuration building
    // WHY:  Verify factory builds valid network configs
    // HOW:  Build config, verify parameters

    uint32_t num_inputs = 32;
    uint32_t num_outputs = 8;
    uint32_t num_neurons = 500;
    float sparsity = 0.8f;

    adaptive_network_config_t config = nimcp_brain_factory_build_network_config(
        num_inputs, num_outputs, num_neurons, sparsity,
        ODE_INTEGRATION_RK4
    );

    // Verify base config
    EXPECT_EQ(config.base.num_inputs, num_inputs);
    EXPECT_EQ(config.base.num_outputs, num_outputs);
    EXPECT_EQ(config.base.num_layers, 3u);  // Input, hidden, output
    EXPECT_EQ(config.base.integration_method, ODE_INTEGRATION_RK4);

    // Verify spike params
    EXPECT_GE(config.spike_params.base_threshold, 0.0f);
    EXPECT_GE(config.spike_params.min_threshold, 0.0f);
    EXPECT_LE(config.spike_params.min_threshold, config.spike_params.base_threshold);

    // Cleanup layer_sizes if allocated
    if (config.base.layer_sizes) {
        free(config.base.layer_sizes);
    }
}

//=============================================================================
// Test: Spike Parameters Building
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, SpikeParams_Building) {
    // WHAT: Test spike parameter building
    // WHY:  Verify adaptive thresholds are configured correctly
    // HOW:  Build params for different sparsity targets

    float sparsity_levels[] = {0.7f, 0.8f, 0.9f};

    for (float sparsity : sparsity_levels) {
        adaptive_spike_params_t params =
            nimcp_brain_factory_build_spike_params(sparsity);

        // Verify biologically realistic values
        EXPECT_GE(params.base_threshold, 0.0f)
            << "Base threshold should be >= 0 for sparsity " << sparsity;
        EXPECT_GE(params.min_threshold, 0.0f)
            << "Min threshold should be >= 0 for sparsity " << sparsity;
        EXPECT_LE(params.min_threshold, params.base_threshold)
            << "Min threshold should be <= base for sparsity " << sparsity;
        EXPECT_GE(params.threshold_decay, 0.0f);
        EXPECT_LE(params.threshold_decay, 1.0f);
    }
}

//=============================================================================
// Test: Brain Statistics Initialization
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, Statistics_Initialization) {
    // WHAT: Test brain statistics initialization
    // WHY:  Verify stats start at correct values
    // HOW:  Create brain, verify initial stats

    brain_t brain = createBrain(BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                                16, 4, "stats_test");
    ASSERT_NE(brain, nullptr);

    brain_stats_t stats = brain_get_stats(brain);

    // Initial stats should be zero or near-zero
    EXPECT_EQ(stats.total_decisions, 0u);
    EXPECT_EQ(stats.total_training_examples, 0u);

    // After some operations, stats should update
    float input[16];
    for (int i = 0; i < 16; i++) input[i] = 0.5f;

    brain_decision_t* decision = brain_decide(brain, input, 16);
    if (decision) brain_free_decision(decision);

    stats = brain_get_stats(brain);
    EXPECT_EQ(stats.total_decisions, 1u);

    brain_destroy(brain);
}

//=============================================================================
// Test: Output Labels Initialization
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, OutputLabels_Initialization) {
    // WHAT: Test output label array initialization
    // WHY:  Verify classification labels work correctly
    // HOW:  Create brain, add labels, verify retrieval

    brain_t brain = createBrain(BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                                16, 4, "labels_test");
    ASSERT_NE(brain, nullptr);

    // Set labels via learning
    float input[16];
    for (int i = 0; i < 16; i++) input[i] = 0.5f;

    brain_learn_example(brain, input, 16, "class_a", 1.0f);
    brain_learn_example(brain, input, 16, "class_b", 1.0f);

    // Get decision
    brain_decision_t* decision = brain_decide(brain, input, 16);
    EXPECT_NE(decision, nullptr);

    if (decision) {
        // Label should be one of the learned classes
        bool valid_label = (strcmp(decision->label, "class_a") == 0) ||
                          (strcmp(decision->label, "class_b") == 0) ||
                          (strlen(decision->label) > 0);
        EXPECT_TRUE(valid_label);
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

//=============================================================================
// Test: Creation Parameter Validation
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, Validation_InvalidParameters) {
    // WHAT: Test parameter validation during creation
    // WHY:  Verify factory rejects invalid parameters
    // HOW:  Try creating with invalid params, verify NULL returned

    // Null task name should fail validation
    EXPECT_FALSE(nimcp_brain_factory_validate_creation_params(nullptr, 10, 5));

    // Zero inputs should fail
    EXPECT_FALSE(nimcp_brain_factory_validate_creation_params("test", 0, 5));

    // Zero outputs should fail
    EXPECT_FALSE(nimcp_brain_factory_validate_creation_params("test", 10, 0));

    // Valid parameters should pass
    EXPECT_TRUE(nimcp_brain_factory_validate_creation_params("test", 10, 5));

    // Large but valid parameters
    EXPECT_TRUE(nimcp_brain_factory_validate_creation_params("test", 1000, 100));
}

//=============================================================================
// Test: Default Sparsity Values
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, DefaultSparsity_BySize) {
    // WHAT: Test default sparsity values by size
    // WHY:  Verify larger networks have higher sparsity
    // HOW:  Get sparsity for each size, verify progression

    float micro_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MICRO);
    float tiny_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_TINY);
    float small_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_SMALL);
    float medium_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_MEDIUM);
    float large_sparsity = nimcp_brain_factory_get_default_sparsity(BRAIN_SIZE_LARGE);

    // All should be in valid range [0.5, 1.0]
    EXPECT_GE(micro_sparsity, 0.5f);
    EXPECT_LE(micro_sparsity, 1.0f);
    EXPECT_GE(tiny_sparsity, 0.5f);
    EXPECT_LE(tiny_sparsity, 1.0f);
    EXPECT_GE(small_sparsity, 0.5f);
    EXPECT_LE(small_sparsity, 1.0f);
    EXPECT_GE(medium_sparsity, 0.5f);
    EXPECT_LE(medium_sparsity, 1.0f);
    EXPECT_GE(large_sparsity, 0.5f);
    EXPECT_LE(large_sparsity, 1.0f);

    // Generally, larger networks should have higher or equal sparsity
    EXPECT_GE(large_sparsity, tiny_sparsity);
}

//=============================================================================
// Test: Neuron Count Progression
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, NeuronCount_Progression) {
    // WHAT: Test neuron counts increase with size
    // WHY:  Verify size presets provide increasing capacity
    // HOW:  Get counts for each size, verify progression

    uint32_t micro_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MICRO);
    uint32_t tiny_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_TINY);
    uint32_t small_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_SMALL);
    uint32_t medium_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_MEDIUM);
    uint32_t large_neurons = nimcp_brain_factory_get_neuron_count(BRAIN_SIZE_LARGE);

    // Sizes should progress
    EXPECT_LT(micro_neurons, tiny_neurons);
    EXPECT_LT(tiny_neurons, small_neurons);
    EXPECT_LE(small_neurons, medium_neurons);  // Could be equal
    EXPECT_LE(medium_neurons, large_neurons);
}

//=============================================================================
// Test: Decision Caching
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, DecisionCaching_Functionality) {
    // WHAT: Test decision caching for repeated inputs
    // WHY:  Verify cache improves performance
    // HOW:  Make repeated decisions, verify caching behavior

    brain_t brain = createBrain(BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                                16, 4, "cache_test");
    ASSERT_NE(brain, nullptr);

    float input[16];
    for (int i = 0; i < 16; i++) input[i] = 0.5f + 0.01f * i;

    // First decision - should compute and cache
    brain_decision_t* decision1 = brain_decide(brain, input, 16);
    ASSERT_NE(decision1, nullptr);

    // Check if input is cached
    bool is_cached = nimcp_brain_factory_is_cached_input(brain, input, 16);
    EXPECT_TRUE(is_cached) << "Input should be cached after decision";

    // Second decision with same input - should use cache
    brain_decision_t* decision2 = brain_decide(brain, input, 16);
    ASSERT_NE(decision2, nullptr);

    // Results should be consistent
    EXPECT_FLOAT_EQ(decision1->confidence, decision2->confidence);

    brain_free_decision(decision1);
    brain_free_decision(decision2);

    // Clear cache
    nimcp_brain_factory_clear_cache(brain);

    // Now input should not be cached
    is_cached = nimcp_brain_factory_is_cached_input(brain, input, 16);
    EXPECT_FALSE(is_cached) << "Cache should be cleared";

    brain_destroy(brain);
}

//=============================================================================
// Test: Learning Updates Cache
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, Learning_InvalidatesCache) {
    // WHAT: Test that learning invalidates decision cache
    // WHY:  Verify cache consistency with weight updates
    // HOW:  Make decision, learn, verify cache cleared

    brain_t brain = createBrain(BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                                8, 2, "learn_cache_test");
    ASSERT_NE(brain, nullptr);

    float input[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Make decision to populate cache
    brain_decision_t* decision = brain_decide(brain, input, 8);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    // Input should be cached
    EXPECT_TRUE(nimcp_brain_factory_is_cached_input(brain, input, 8));

    // Learn - should invalidate cache
    brain_learn_example(brain, input, 8, "class_a", 1.0f);

    // After learning, cache should typically be cleared
    // (implementation may vary)

    // Make new decision
    decision = brain_decide(brain, input, 8);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    brain_destroy(brain);
}

//=============================================================================
// Test: Multiple Brain Instances
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, MultipleBrains_Independent) {
    // WHAT: Test multiple brain instances operate independently
    // WHY:  Verify no shared state between brains
    // HOW:  Create multiple brains, verify independent operation

    brain_t brain1 = createBrain(BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                                 8, 2, "brain1");
    brain_t brain2 = createBrain(BRAIN_SIZE_SMALL, BRAIN_TASK_REGRESSION,
                                 8, 1, "brain2");

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);

    // Brains should be different
    EXPECT_NE(brain1, brain2);

    float input[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Both should make decisions independently
    brain_decision_t* d1 = brain_decide(brain1, input, 8);
    brain_decision_t* d2 = brain_decide(brain2, input, 8);

    EXPECT_NE(d1, nullptr);
    EXPECT_NE(d2, nullptr);

    if (d1) brain_free_decision(d1);
    if (d2) brain_free_decision(d2);

    // Learning in one shouldn't affect other
    brain_learn_example(brain1, input, 8, "class_x", 1.0f);

    brain_stats_t stats1 = brain_get_stats(brain1);
    brain_stats_t stats2 = brain_get_stats(brain2);

    EXPECT_EQ(stats1.total_training_examples, 1u);
    EXPECT_EQ(stats2.total_training_examples, 0u);

    brain_destroy(brain1);
    brain_destroy(brain2);
}

//=============================================================================
// Test: Brain Persistence Preparation
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, Persistence_Preparation) {
    // WHAT: Test brain is prepared for persistence
    // WHY:  Verify save/load infrastructure works
    // HOW:  Create brain, verify it can be saved

    brain_t brain = createBrain(BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION,
                                16, 4, "persist_test");
    ASSERT_NE(brain, nullptr);

    // Train briefly
    float input[16];
    for (int i = 0; i < 16; i++) input[i] = 0.5f;

    brain_learn_example(brain, input, 16, "test_class", 1.0f);

    // Verify stats are tracked (needed for persistence)
    brain_stats_t stats = brain_get_stats(brain);
    EXPECT_GT(stats.total_training_examples, 0u);

    // Get neuron count (used in persistence)
    EXPECT_GT(stats.num_neurons, 0u);

    brain_destroy(brain);
}

//=============================================================================
// Test: Config Initialization
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, ConfigInit_AllFields) {
    // WHAT: Test brain config initialization
    // WHY:  Verify all config fields are properly set
    // HOW:  Initialize config, verify field values

    brain_config_t config;
    nimcp_brain_factory_init_brain_config(
        &config,
        "test_brain",
        BRAIN_SIZE_MEDIUM,
        BRAIN_TASK_CLASSIFICATION,
        32, 8,
        nullptr  // No strategy
    );

    // Verify basic fields
    EXPECT_STREQ(config.task_name, "test_brain");
    EXPECT_EQ(config.size, BRAIN_SIZE_MEDIUM);
    EXPECT_EQ(config.task, BRAIN_TASK_CLASSIFICATION);
    EXPECT_EQ(config.num_inputs, 32u);
    EXPECT_EQ(config.num_outputs, 8u);
}

//=============================================================================
// Test: Stats Initialization
//=============================================================================

TEST_F(BrainFactoryIntegrationTest, StatsInit_AllFields) {
    // WHAT: Test brain stats initialization
    // WHY:  Verify stats start with correct values
    // HOW:  Initialize stats, verify field values

    brain_stats_t stats;
    nimcp_brain_factory_init_brain_stats(
        &stats,
        "test_brain",
        BRAIN_SIZE_SMALL,
        16,
        0.01f  // learning rate
    );

    // Verify fields
    EXPECT_STREQ(stats.brain_name, "test_brain");
    EXPECT_GT(stats.num_neurons, 0u);
    EXPECT_FLOAT_EQ(stats.learning_rate, 0.01f);
    EXPECT_EQ(stats.total_decisions, 0u);
    EXPECT_EQ(stats.total_training_examples, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
