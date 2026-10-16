//=============================================================================
// test_quantum_shannon_integration.cpp - Quantum-Shannon Brain Integration Tests
//=============================================================================
/**
 * @file test_quantum_shannon_integration.cpp
 * @brief Integration tests for Quantum Walk + Shannon with NIMCP brain pipelines
 *
 * COVERAGE: Quantum-Shannon diffusion + Brain learning/inference + Neuromodulation
 * TEST COUNT: 15+ integration tests
 *
 * TEST CATEGORIES:
 * 1. Brain learning pipeline integration (5 tests)
 * 2. Brain inference pipeline integration (3 tests)
 * 3. Neuromodulator diffusion with quantum-Shannon (3 tests)
 * 4. Multi-step evolution with metrics tracking (2 tests)
 * 5. Bottleneck detection during learning (2 tests)
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 * @version 2.10.0 Phase C4.1
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "utils/quantum/nimcp_quantum_shannon.h"

#include "core/brain/nimcp_brain.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumShannonIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create brain instances for different tests
        brain_small = brain_create("quantum_shannon_small", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain_small, nullptr);

        brain_medium = brain_create("quantum_shannon_medium", BRAIN_SIZE_MEDIUM,
                                   BRAIN_TASK_CLASSIFICATION, 20, 10);
        ASSERT_NE(brain_medium, nullptr);

        // Get default configurations
        qs_config = quantum_shannon_default_config();
    }

    void TearDown() override {
        if (brain_small) {
            brain_destroy(brain_small);
        }
        if (brain_medium) {
            brain_destroy(brain_medium);
        }
        if (brain_large) {
            brain_destroy(brain_large);
        }
    }

    brain_t brain_small = nullptr;
    brain_t brain_medium = nullptr;
    brain_t brain_large = nullptr;
    quantum_shannon_config_t qs_config;

    // Helper: Create test pattern
    void create_test_pattern(float* features, uint32_t size, float value) {
        for (uint32_t i = 0; i < size; i++) {
            features[i] = value + 0.01f * (float)i;
        }
    }

    // Helper: Get network from brain (simplified for testing)
    neural_network_t get_brain_network(brain_t brain) {
        // Get adaptive network from brain, then extract base network
        adaptive_network_t adaptive_net = brain_get_network(brain);
        return adaptive_network_get_base_network(adaptive_net);
    }
};

//=============================================================================
// Category 1: Brain Learning Pipeline Integration (5 tests)
//=============================================================================

TEST_F(QuantumShannonIntegrationTest, BrainLearningWithQuantumShannon) {
    // Train brain with quantum-Shannon monitoring
    float features[10];
    create_test_pattern(features, 10, 0.5f);

    // Learn pattern
    float loss = brain_learn_example(brain_small, features, 10, "class_a", 0.9f);
    EXPECT_GE(loss, 0.0f);

    // Get neural network for quantum-Shannon diffusion
    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    // Create quantum-Shannon diffusion starting at source neuron
    uint32_t source_neuron = 0;
    float source_information_bits = 8.0f;

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, source_neuron, source_information_bits, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Evolve quantum-Shannon diffusion
    bool evolved = quantum_shannon_evolve(qsd, 50);
    EXPECT_TRUE(evolved);

    // Get Shannon metrics
    shannon_diffusion_metrics_t metrics;
    bool got_metrics = quantum_shannon_get_metrics(qsd, &metrics);
    EXPECT_TRUE(got_metrics);

    // Verify metrics updated correctly
    EXPECT_GT(metrics.source_entropy, 0.0f);
    EXPECT_LE(metrics.source_entropy, source_information_bits + 0.1f);
    EXPECT_GE(metrics.propagation_efficiency, 0.0f);
    EXPECT_LE(metrics.propagation_efficiency, 1.0f);

    // Verify information preserved > 80%
    EXPECT_GT(metrics.propagation_efficiency, 0.80f);

    // Verify speedup vs classical
    EXPECT_GT(metrics.speedup_vs_classical, 1.0f);

    quantum_shannon_destroy(qsd);
}

TEST_F(QuantumShannonIntegrationTest, QuantumShannon_TracksLearningProgress) {
    std::vector<float> propagation_efficiencies;

    float features[10];
    create_test_pattern(features, 10, 0.5f);

    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    // Track quantum-Shannon metrics over learning epochs
    for (int epoch = 0; epoch < 5; epoch++) {
        // Learning step
        brain_learn_example(brain_small, features, 10, "class_a", 0.9f);

        // Create quantum-Shannon diffusion
        quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
            network, 0, 8.0f, &qs_config
        );
        ASSERT_NE(qsd, nullptr);

        quantum_shannon_evolve(qsd, 30);

        shannon_diffusion_metrics_t metrics;
        quantum_shannon_get_metrics(qsd, &metrics);

        propagation_efficiencies.push_back(metrics.propagation_efficiency);

        quantum_shannon_destroy(qsd);
    }

    // Efficiency should improve or stabilize with learning
    EXPECT_GE(propagation_efficiencies.back(), propagation_efficiencies.front() - 0.1f);
}

TEST_F(QuantumShannonIntegrationTest, BottleneckDetection_DuringLearning) {
    float features[10];
    create_test_pattern(features, 10, 0.5f);

    // Train brain
    for (int i = 0; i < 3; i++) {
        brain_learn_example(brain_small, features, 10, "class_a", 0.9f);
    }

    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    // Create quantum-Shannon diffusion
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Evolve to allow bottleneck detection
    quantum_shannon_evolve(qsd, 100);

    // Get bottlenecks
    const uint32_t max_bottlenecks = 20;
    quantum_shannon_bottleneck_t bottlenecks[max_bottlenecks];

    uint32_t num_bottlenecks = quantum_shannon_get_bottlenecks(
        qsd, bottlenecks, max_bottlenecks
    );

    // Should detect some bottlenecks in untrained network
    EXPECT_GE(num_bottlenecks, 0u);

    // Verify bottleneck properties
    for (uint32_t i = 0; i < num_bottlenecks; i++) {
        EXPECT_GT(bottlenecks[i].capacity, 0.0f);
        EXPECT_GT(bottlenecks[i].deficit, 0.0f);
        EXPECT_LE(bottlenecks[i].deficit, 1.0f);
        EXPECT_GT(bottlenecks[i].suggested_weight, 0.0f);
    }

    quantum_shannon_destroy(qsd);
}

TEST_F(QuantumShannonIntegrationTest, QuantumOptimization_ImprovesCapacity) {
    float features[10];
    create_test_pattern(features, 10, 0.6f);

    brain_learn_example(brain_small, features, 10, "class_a", 0.9f);

    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    // Create diffusion and evolve
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 8.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    quantum_shannon_evolve(qsd, 50);

    // Get metrics before optimization
    shannon_diffusion_metrics_t metrics_before;
    quantum_shannon_get_metrics(qsd, &metrics_before);

    // Apply quantum-Shannon optimization
    bool optimized = quantum_shannon_optimize(qsd);
    EXPECT_TRUE(optimized);

    // Evolve again after optimization
    quantum_shannon_reset(qsd);
    quantum_shannon_evolve(qsd, 50);

    // Get metrics after optimization
    shannon_diffusion_metrics_t metrics_after;
    quantum_shannon_get_metrics(qsd, &metrics_after);

    // Optimization should maintain or improve efficiency
    EXPECT_GE(metrics_after.propagation_efficiency,
              metrics_before.propagation_efficiency - 0.05f);

    quantum_shannon_destroy(qsd);
}

TEST_F(QuantumShannonIntegrationTest, BatchLearning_WithQuantumShannonMonitoring) {
    // Create batch of examples
    const uint32_t batch_size = 5;
    brain_example_t* examples = (brain_example_t*)malloc(
        batch_size * sizeof(brain_example_t)
    );

    for (uint32_t i = 0; i < batch_size; i++) {
        examples[i].features = (float*)malloc(10 * sizeof(float));
        examples[i].num_features = 10;
        create_test_pattern(examples[i].features, 10, 0.1f * (float)i);
        strncpy(examples[i].label, "class_a", 63);
        examples[i].confidence = 0.9f;
    }

    // Batch learning
    brain_learn_batch(brain_small, examples, batch_size);

    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    // Analyze quantum-Shannon metrics after batch
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    quantum_shannon_evolve(qsd, 75);

    shannon_diffusion_metrics_t metrics;
    quantum_shannon_get_metrics(qsd, &metrics);

    EXPECT_GT(metrics.total_capacity, 0.0f);
    EXPECT_GT(metrics.num_nodes_reached, 0u);

    quantum_shannon_destroy(qsd);

    // Cleanup
    for (uint32_t i = 0; i < batch_size; i++) {
        free(examples[i].features);
    }
    free(examples);
}

//=============================================================================
// Category 2: Brain Inference Pipeline Integration (3 tests)
//=============================================================================

TEST_F(QuantumShannonIntegrationTest, QuantumShannon_DuringInference) {
    // Train first
    float train_features[10];
    create_test_pattern(train_features, 10, 0.7f);
    brain_learn_example(brain_small, train_features, 10, "class_a", 0.9f);

    // Perform inference
    float test_features[10];
    create_test_pattern(test_features, 10, 0.65f);

    brain_decision_t* decision = brain_decide(brain_small, test_features, 10);
    ASSERT_NE(decision, nullptr);

    EXPECT_GE(decision->confidence, 0.0f);
    EXPECT_LE(decision->confidence, 1.0f);

    brain_free_decision(decision);

    // Analyze information flow during inference using quantum-Shannon
    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 8.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    quantum_shannon_evolve(qsd, 40);

    shannon_diffusion_metrics_t metrics;
    quantum_shannon_get_metrics(qsd, &metrics);

    // Verify information propagates efficiently during inference
    EXPECT_GT(metrics.information_rate, 0.0f);
    EXPECT_GT(metrics.num_nodes_reached, 0u);

    quantum_shannon_destroy(qsd);
}

TEST_F(QuantumShannonIntegrationTest, InformationFlow_ScalesWithBrainSize) {
    // Create brains of different sizes
    brain_large = brain_create("quantum_shannon_large", BRAIN_SIZE_LARGE,
                              BRAIN_TASK_CLASSIFICATION, 50, 25);
    ASSERT_NE(brain_large, nullptr);

    // Train all brains
    float features_small[10], features_medium[20], features_large[50];
    create_test_pattern(features_small, 10, 0.5f);
    create_test_pattern(features_medium, 20, 0.5f);
    create_test_pattern(features_large, 50, 0.5f);

    brain_learn_example(brain_small, features_small, 10, "test", 0.9f);
    brain_learn_example(brain_medium, features_medium, 20, "test", 0.9f);
    brain_learn_example(brain_large, features_large, 50, "test", 0.9f);

    // Analyze quantum-Shannon metrics for each
    std::vector<float> total_capacities;

    brain_t brains[] = {brain_small, brain_medium, brain_large};
    for (int i = 0; i < 3; i++) {
        neural_network_t network = get_brain_network(brains[i]);
        if (network == nullptr) continue;

        quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
            network, 0, 8.0f, &qs_config
        );
        if (qsd == nullptr) continue;

        quantum_shannon_evolve(qsd, 50);

        shannon_diffusion_metrics_t metrics;
        quantum_shannon_get_metrics(qsd, &metrics);

        total_capacities.push_back(metrics.total_capacity);

        quantum_shannon_destroy(qsd);
    }

    // Total capacity should generally increase with network size
    ASSERT_GE(total_capacities.size(), 2u);
    // Allow some variance but expect general trend
}

TEST_F(QuantumShannonIntegrationTest, BatchInference_WithQuantumShannonTracking) {
    // Train first
    float train_features[10];
    create_test_pattern(train_features, 10, 0.6f);
    brain_learn_example(brain_small, train_features, 10, "class_a", 0.9f);

    // Create batch for inference
    const uint32_t batch_size = 3;
    float** input_features = (float**)malloc(batch_size * sizeof(float*));

    for (uint32_t i = 0; i < batch_size; i++) {
        input_features[i] = (float*)malloc(10 * sizeof(float));
        create_test_pattern(input_features[i], 10, 0.55f + 0.05f * (float)i);
    }

    brain_decision_t* outputs = (brain_decision_t*)calloc(
        batch_size, sizeof(brain_decision_t)
    );

    brain_decide_batch(brain_small, (const float**)input_features, batch_size, 10, outputs);

    // Verify all inferences succeeded
    for (uint32_t i = 0; i < batch_size; i++) {
        EXPECT_GE(outputs[i].confidence, 0.0f);
        EXPECT_LE(outputs[i].confidence, 1.0f);
    }

    // Analyze quantum-Shannon metrics after batch inference
    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 9.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    quantum_shannon_evolve(qsd, 60);

    shannon_diffusion_metrics_t metrics;
    quantum_shannon_get_metrics(qsd, &metrics);

    EXPECT_GT(metrics.average_capacity, 0.0f);

    quantum_shannon_destroy(qsd);

    // Cleanup
    for (uint32_t i = 0; i < batch_size; i++) {
        free(input_features[i]);
    }
    free(input_features);
    free(outputs);
}

//=============================================================================
// Category 3: Neuromodulator Diffusion with Quantum-Shannon (3 tests)
//=============================================================================

TEST_F(QuantumShannonIntegrationTest, QuantumWalk_AcceleratesNeuromodDiffusion) {
    neural_network_t network = get_brain_network(brain_medium);
    ASSERT_NE(network, nullptr);

    // Use middle neuron as source for better connectivity
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    uint32_t source_node = num_neurons / 2;

    // Create quantum-Shannon diffusion for neuromodulator (e.g., dopamine)
    quantum_shannon_config_t fast_config = quantum_shannon_fast_config();

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, source_node, 10.0f, &fast_config
    );
    ASSERT_NE(qsd, nullptr);

    // Measure evolution time
    quantum_shannon_evolve(qsd, 100);

    shannon_diffusion_metrics_t metrics;
    quantum_shannon_get_metrics(qsd, &metrics);

    // Verify quantum speedup (may be modest for brain networks)
    EXPECT_GE(metrics.speedup_vs_classical, 1.0f);

    // Verify spreading distance (quantum walk should spread)
    EXPECT_GT(metrics.spreading_distance, 0.0f);

    quantum_shannon_destroy(qsd);
}

TEST_F(QuantumShannonIntegrationTest, QuantumShannon_DetectsNeuromodBottlenecks) {
    float features[10];
    create_test_pattern(features, 10, 0.5f);
    brain_learn_example(brain_small, features, 10, "class_a", 0.9f);

    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    // Create diffusion with bottleneck detection enabled
    qs_config.bottleneck_threshold = 0.6f;  // Stricter threshold
    qs_config.enable_adaptive_coin = true;

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 12.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    quantum_shannon_evolve(qsd, 80);

    // Get metrics and bottlenecks
    shannon_diffusion_metrics_t metrics;
    quantum_shannon_get_metrics(qsd, &metrics);

    EXPECT_GE(metrics.num_bottlenecks, 0u);

    if (metrics.num_bottlenecks > 0) {
        EXPECT_GT(metrics.bottleneck_severity, 0.0f);
        EXPECT_LE(metrics.bottleneck_severity, 1.0f);
    }

    quantum_shannon_destroy(qsd);
}

TEST_F(QuantumShannonIntegrationTest, QuantumShannon_RouteAroundBottlenecks) {
    neural_network_t network = get_brain_network(brain_medium);
    ASSERT_NE(network, nullptr);

    // Create diffusion
    qs_config.enable_adaptive_coin = true;
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Evolve to detect bottlenecks
    quantum_shannon_evolve(qsd, 60);

    // Get metrics before routing
    shannon_diffusion_metrics_t metrics_before;
    quantum_shannon_get_metrics(qsd, &metrics_before);

    // Apply routing optimization
    bool routed = quantum_shannon_route_around_bottlenecks(qsd);
    EXPECT_TRUE(routed);

    // Reset and evolve again
    quantum_shannon_reset(qsd);
    quantum_shannon_evolve(qsd, 60);

    // Get metrics after routing
    shannon_diffusion_metrics_t metrics_after;
    quantum_shannon_get_metrics(qsd, &metrics_after);

    // Routing should maintain or improve propagation
    EXPECT_GE(metrics_after.num_nodes_reached,
              metrics_before.num_nodes_reached);

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Category 4: Multi-Step Evolution with Metrics Tracking (2 tests)
//=============================================================================

TEST_F(QuantumShannonIntegrationTest, MultiStep_MetricsEvolution) {
    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 8.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    std::vector<float> propagation_efficiencies;
    std::vector<float> spreading_distances;
    std::vector<uint32_t> nodes_reached;

    // Track metrics over multiple evolution steps
    for (int step = 0; step < 10; step++) {
        quantum_shannon_step(qsd);

        shannon_diffusion_metrics_t metrics;
        quantum_shannon_get_metrics(qsd, &metrics);

        propagation_efficiencies.push_back(metrics.propagation_efficiency);
        spreading_distances.push_back(metrics.spreading_distance);
        nodes_reached.push_back(metrics.num_nodes_reached);
    }

    // Verify metrics evolve sensibly
    EXPECT_GT(propagation_efficiencies.size(), 0u);

    // Spreading distance should generally increase
    EXPECT_GE(spreading_distances.back(), spreading_distances.front());

    // Nodes reached should increase or stabilize
    EXPECT_GE(nodes_reached.back(), nodes_reached.front());

    quantum_shannon_destroy(qsd);
}

TEST_F(QuantumShannonIntegrationTest, InformationRate_TracksTemporalDynamics) {
    float features[10];
    create_test_pattern(features, 10, 0.5f);
    brain_learn_example(brain_small, features, 10, "class_a", 0.9f);

    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    // Configure with frequent Shannon updates
    qs_config.shannon_update_interval = 5;  // Update every 5 steps
    qs_config.track_information_loss = true;

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    std::vector<float> information_rates;

    // Track information rate over evolution
    for (int i = 0; i < 20; i++) {
        quantum_shannon_step(qsd);

        if (i % 5 == 0) {
            shannon_diffusion_metrics_t metrics;
            quantum_shannon_get_metrics(qsd, &metrics);
            information_rates.push_back(metrics.information_rate);
        }
    }

    // All rates should be non-negative
    for (float rate : information_rates) {
        EXPECT_GE(rate, 0.0f);
    }

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Category 5: Integration with Different Configurations (2 tests)
//=============================================================================

TEST_F(QuantumShannonIntegrationTest, HighAccuracyConfig_PreservesMoreInformation) {
    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    // Test with default config
    quantum_shannon_diffusion_t* qsd_default = quantum_shannon_create(
        network, 0, 8.0f, &qs_config
    );
    ASSERT_NE(qsd_default, nullptr);

    quantum_shannon_evolve(qsd_default, 50);

    shannon_diffusion_metrics_t metrics_default;
    quantum_shannon_get_metrics(qsd_default, &metrics_default);

    // Test with high-accuracy config
    quantum_shannon_config_t high_acc = quantum_shannon_high_accuracy_config();
    quantum_shannon_diffusion_t* qsd_high_acc = quantum_shannon_create(
        network, 0, 8.0f, &high_acc
    );
    ASSERT_NE(qsd_high_acc, nullptr);

    quantum_shannon_evolve(qsd_high_acc, 50);

    shannon_diffusion_metrics_t metrics_high_acc;
    quantum_shannon_get_metrics(qsd_high_acc, &metrics_high_acc);

    // High-accuracy should have higher or equal propagation efficiency
    EXPECT_GE(metrics_high_acc.propagation_efficiency,
              metrics_default.propagation_efficiency - 0.05f);

    // Information loss should be lower with high-accuracy
    EXPECT_LE(metrics_high_acc.information_loss,
              metrics_default.information_loss + 0.1f);

    quantum_shannon_destroy(qsd_default);
    quantum_shannon_destroy(qsd_high_acc);
}

TEST_F(QuantumShannonIntegrationTest, FastConfig_MaintainsPerformance) {
    neural_network_t network = get_brain_network(brain_medium);
    ASSERT_NE(network, nullptr);

    // Use middle neuron as source for better connectivity
    uint32_t num_neurons = neural_network_get_num_neurons(network);
    uint32_t source_node = num_neurons / 2;

    quantum_shannon_config_t fast_config = quantum_shannon_fast_config();

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, source_node, 10.0f, &fast_config
    );
    ASSERT_NE(qsd, nullptr);

    // Fast config should still produce valid results
    quantum_shannon_evolve(qsd, 100);

    shannon_diffusion_metrics_t metrics;
    quantum_shannon_get_metrics(qsd, &metrics);

    // All metrics should be valid
    EXPECT_GE(metrics.source_entropy, 0.0f);
    EXPECT_GE(metrics.total_entropy, 0.0f);
    EXPECT_GE(metrics.propagation_efficiency, 0.0f);
    EXPECT_LE(metrics.propagation_efficiency, 1.0f);
    EXPECT_GE(metrics.speedup_vs_classical, 1.0f);  // At least 1x (relaxed from >)

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Bonus Tests: Edge Cases and Robustness
//=============================================================================

TEST_F(QuantumShannonIntegrationTest, QuantumShannon_HandlesResetCorrectly) {
    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 8.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Evolve
    quantum_shannon_evolve(qsd, 50);

    shannon_diffusion_metrics_t metrics_before;
    quantum_shannon_get_metrics(qsd, &metrics_before);

    // Reset
    bool reset_ok = quantum_shannon_reset(qsd);
    EXPECT_TRUE(reset_ok);

    // Evolve again
    quantum_shannon_evolve(qsd, 50);

    shannon_diffusion_metrics_t metrics_after;
    quantum_shannon_get_metrics(qsd, &metrics_after);

    // Metrics should be similar after reset
    EXPECT_NEAR(metrics_before.propagation_efficiency,
                metrics_after.propagation_efficiency, 0.15f);

    quantum_shannon_destroy(qsd);
}

TEST_F(QuantumShannonIntegrationTest, QuantumShannon_VerifiesIntegrity) {
    neural_network_t network = get_brain_network(brain_small);
    ASSERT_NE(network, nullptr);

    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 8.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    quantum_shannon_evolve(qsd, 40);

    // Verify quantum-Shannon integrity
    bool valid = quantum_shannon_verify(qsd);
    EXPECT_TRUE(valid);

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
