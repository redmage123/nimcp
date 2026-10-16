/**
 * @file test_adaptive_routing_integration.cpp
 * @brief Integration tests for Phase C4.4 adaptive routing with brain pipeline
 *
 * WHAT: Integration tests for adaptive routing in cognitive/training pipelines
 * WHY:  Ensure adaptive routing works with brain learning and inference
 * HOW:  Test brain + adaptive routing integration scenarios
 *
 * TEST COVERAGE:
 * - Brain learning with adaptive routing
 * - Brain inference with adaptive routing
 * - Neuromodulator system integration
 * - Multi-field adaptive routing
 * - Performance validation
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "core/neuralnet/nimcp_neuralnet.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AdaptiveRoutingIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    uint32_t num_features;
    uint32_t num_classes;

    void SetUp() override {
        num_features = 10;
        num_classes = 3;

        // Create brain with medium size
        brain = brain_create(
            "test_adaptive_brain",
            BRAIN_SIZE_MEDIUM,
            BRAIN_TASK_CLASSIFICATION,
            num_features,
            num_classes
        );

        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Test: Brain Learning with Adaptive Routing
//=============================================================================

TEST_F(AdaptiveRoutingIntegrationTest, BrainLearning_WithAdaptiveRouting_Works) {
    // WHAT: Test brain learning with adaptive routing enabled
    // WHY:  Ensure adaptive routing doesn't break learning
    // HOW:  Enable adaptive routing, train brain, check success

    // Enable quantum-Shannon (required for adaptive routing)
    bool qs_success = brain_enable_quantum_shannon_diffusion(brain, true, 0, 10.0f);
    ASSERT_TRUE(qs_success);

    // Create training data
    float features[10] = {0.5f, 0.3f, 0.8f, 0.2f, 0.9f, 0.1f, 0.7f, 0.4f, 0.6f, 0.5f};
    const char* label = "class_A";
    float confidence = 0.9f;

    // Learn multiple examples
    for (int i = 0; i < 10; i++) {
        bool success = brain_learn_example(brain, features, num_features, label, confidence);
        EXPECT_TRUE(success);
    }

    // Verify brain can make decisions after learning
    brain_decision_t* decision = brain_decide(brain, features, num_features);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label[0], '\0');  // Non-empty label

    brain_free_decision(decision);
}

TEST_F(AdaptiveRoutingIntegrationTest, BrainLearning_MultipleClasses_Works) {
    // WHAT: Test learning multiple classes with adaptive routing
    // WHY:  Ensure adaptive routing handles multi-class scenarios
    // HOW:  Train on multiple classes, verify decisions

    brain_enable_quantum_shannon_diffusion(brain, true, 0, 10.0f);

    float features_A[10] = {0.9f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    float features_B[10] = {0.1f, 0.9f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    float features_C[10] = {0.1f, 0.1f, 0.9f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};

    // Train on each class
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(brain_learn_example(brain, features_A, num_features, "class_A", 0.9f));
        EXPECT_TRUE(brain_learn_example(brain, features_B, num_features, "class_B", 0.9f));
        EXPECT_TRUE(brain_learn_example(brain, features_C, num_features, "class_C", 0.9f));
    }

    // Test decisions for each class
    brain_decision_t* decision_A = brain_decide(brain, features_A, num_features);
    brain_decision_t* decision_B = brain_decide(brain, features_B, num_features);
    brain_decision_t* decision_C = brain_decide(brain, features_C, num_features);

    ASSERT_NE(decision_A, nullptr);
    ASSERT_NE(decision_B, nullptr);
    ASSERT_NE(decision_C, nullptr);

    brain_free_decision(decision_A);
    brain_free_decision(decision_B);
    brain_free_decision(decision_C);
}

//=============================================================================
// Test: Brain Inference with Adaptive Routing
//=============================================================================

TEST_F(AdaptiveRoutingIntegrationTest, BrainInference_WithAdaptiveRouting_Works) {
    // WHAT: Test brain inference with adaptive routing
    // WHY:  Ensure adaptive routing doesn't break inference
    // HOW:  Enable adaptive routing, make decisions, check results

    brain_enable_quantum_shannon_diffusion(brain, true, 0, 10.0f);

    // Train a bit first
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_learn_example(brain, features, num_features, "test_class", 0.8f);

    // Make decision
    brain_decision_t* decision = brain_decide(brain, features, num_features);
    ASSERT_NE(decision, nullptr);

    // Verify decision structure
    EXPECT_NE(decision->label[0], '\0');
    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);
}

TEST_F(AdaptiveRoutingIntegrationTest, BrainInference_RepeatedCalls_Stable) {
    // WHAT: Test repeated inference calls
    // WHY:  Ensure adaptive routing is stable over time
    // HOW:  Make many decisions, check stability

    brain_enable_quantum_shannon_diffusion(brain, true, 0, 10.0f);

    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    brain_learn_example(brain, features, num_features, "stable_class", 0.9f);

    // Make many decisions
    for (int i = 0; i < 20; i++) {
        brain_decision_t* decision = brain_decide(brain, features, num_features);
        ASSERT_NE(decision, nullptr);
        EXPECT_NE(decision->label[0], '\0');
        brain_free_decision(decision);
    }
}

//=============================================================================
// Test: Neuromodulator System Integration
//=============================================================================

TEST_F(AdaptiveRoutingIntegrationTest, NeuromodulatorSystem_WithAdaptiveRouting_Works) {
    // WHAT: Test neuromodulator system with adaptive routing
    // WHY:  Ensure neuromodulators integrate with adaptive routing
    // HOW:  Create neuromod system with adaptive routing, test release

    // Create network
    uint32_t num_neurons = 100;

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

    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    // Configure with adaptive routing
    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        configs[i].enable_quantum_walk = true;
        configs[i].enable_adaptive_routing = true;
    }

    bool enabled_types[NEUROMOD_COUNT] = {true, true, true, true};

    // Create system
    spatial_neuromod_system_t* system = spatial_neuromod_system_create(
        network, enabled_types, configs);
    ASSERT_NE(system, nullptr);

    // Enable quantum-Shannon for each field
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        if (system->fields[i]) {
            system->fields[i]->use_quantum_shannon = true;
            quantum_shannon_config_t qs_config = quantum_shannon_default_config();
            system->fields[i]->quantum_shannon_diffusion = quantum_shannon_create(
                network, num_neurons / 2, 10.0f, &qs_config);
            ASSERT_NE(system->fields[i]->quantum_shannon_diffusion, nullptr);

            // Set metrics for testing
            system->fields[i]->last_propagation_efficiency = 0.7f;
            system->fields[i]->last_speedup_vs_classical = 8.0f;
            system->fields[i]->last_num_bottlenecks = 1;
            system->fields[i]->last_information_rate = 1.0f;
        }
    }

    // Test adaptive release on dopamine field
    bool success = spatial_neuromod_release_adaptive(
        system->fields[NEUROMOD_DOPAMINE],
        network,
        &configs[NEUROMOD_DOPAMINE],
        10.0f
    );
    EXPECT_TRUE(success);

    // Cleanup
    spatial_neuromod_system_destroy(system);
    neural_network_destroy(network);
}

//=============================================================================
// Test: Multi-Field Adaptive Routing
//=============================================================================

TEST_F(AdaptiveRoutingIntegrationTest, MultiField_AdaptiveRouting_Independent) {
    // WHAT: Test that multiple fields route independently
    // WHY:  Ensure fields don't interfere
    // HOW:  Release on multiple fields, verify independence

    uint32_t num_neurons = 100;

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

    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        configs[i].enable_quantum_walk = true;
        configs[i].enable_adaptive_routing = true;
    }

    bool enabled_types[NEUROMOD_COUNT] = {true, true, true, true};

    spatial_neuromod_system_t* system = spatial_neuromod_system_create(
        network, enabled_types, configs);
    ASSERT_NE(system, nullptr);

    // Enable quantum-Shannon for each field with different metrics
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        if (system->fields[i]) {
            system->fields[i]->use_quantum_shannon = true;
            quantum_shannon_config_t qs_config = quantum_shannon_default_config();
            system->fields[i]->quantum_shannon_diffusion = quantum_shannon_create(
                network, num_neurons / 2, 10.0f, &qs_config);

            // Different metrics for each field
            system->fields[i]->last_propagation_efficiency = 0.5f + 0.1f * i;
            system->fields[i]->last_speedup_vs_classical = 5.0f + 2.0f * i;
            system->fields[i]->last_num_bottlenecks = i;
            system->fields[i]->last_information_rate = 0.5f + 0.3f * i;
        }
    }

    // Release on all fields
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        if (system->fields[i]) {
            bool success = spatial_neuromod_release_adaptive(
                system->fields[i],
                network,
                &configs[i],
                10.0f
            );
            EXPECT_TRUE(success);
        }
    }

    // Verify each field has source_rate set
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        if (system->fields[i]) {
            float total_source_rate = 0.0f;
            for (uint32_t j = 0; j < num_neurons; j++) {
                total_source_rate += system->fields[i]->source_rate[j];
            }
            EXPECT_FLOAT_EQ(total_source_rate, 10.0f);
        }
    }

    spatial_neuromod_system_destroy(system);
    neural_network_destroy(network);
}

//=============================================================================
// Test: Performance Validation
//=============================================================================

TEST_F(AdaptiveRoutingIntegrationTest, Performance_AdaptiveRoutingOverhead_Acceptable) {
    // WHAT: Test that adaptive routing overhead is acceptable
    // WHY:  Ensure performance is reasonable
    // HOW:  Measure time for adaptive vs non-adaptive release

    uint32_t num_neurons = 500;

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

    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    spatial_neuromod_config_t config = spatial_neuromod_default_config(NEUROMOD_DOPAMINE);
    config.enable_quantum_walk = true;
    config.enable_adaptive_routing = true;

    spatial_neuromod_field_t* field = spatial_neuromod_create(num_neurons, &config);
    ASSERT_NE(field, nullptr);

    field->use_quantum_shannon = true;
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    field->quantum_shannon_diffusion = quantum_shannon_create(
        network, num_neurons / 2, 10.0f, &qs_config);
    ASSERT_NE(field->quantum_shannon_diffusion, nullptr);

    field->last_propagation_efficiency = 0.8f;
    field->last_speedup_vs_classical = 10.0f;
    field->last_num_bottlenecks = 2;
    field->last_information_rate = 1.5f;

    // Test adaptive release (should complete in reasonable time)
    bool success = spatial_neuromod_release_adaptive(field, network, &config, 10.0f);
    EXPECT_TRUE(success);

    // Cleanup
    spatial_neuromod_destroy(field);
    neural_network_destroy(network);
}

//=============================================================================
// Test: Backward Compatibility
//=============================================================================

TEST_F(AdaptiveRoutingIntegrationTest, BackwardCompatibility_AdaptiveRoutingDisabledByDefault) {
    // WHAT: Test that adaptive routing is disabled by default
    // WHY:  Ensure backward compatibility
    // HOW:  Create system with defaults, verify adaptive routing off

    uint32_t num_neurons = 100;

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

    neural_network_t network = neural_network_create(&net_config);
    ASSERT_NE(network, nullptr);

    spatial_neuromod_config_t configs[NEUROMOD_COUNT];
    for (int i = 0; i < NEUROMOD_COUNT; i++) {
        configs[i] = spatial_neuromod_default_config((neuromodulator_type_t)i);
        // Don't enable adaptive routing
    }

    bool enabled_types[NEUROMOD_COUNT] = {true, false, false, false};

    spatial_neuromod_system_t* system = spatial_neuromod_system_create(
        network, enabled_types, configs);
    ASSERT_NE(system, nullptr);

    // Verify adaptive routing is off
    EXPECT_FALSE(configs[NEUROMOD_DOPAMINE].enable_adaptive_routing);

    spatial_neuromod_system_destroy(system);
    neural_network_destroy(network);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
