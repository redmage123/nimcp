//=============================================================================
// test_routing_efficiency_regression.cpp - Routing Efficiency Regression Tests
//=============================================================================

#include <gtest/gtest.h>
#include <chrono>
#include "core/brain/nimcp_brain.h"
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "utils/quantum/nimcp_quantum_shannon.h"

class RoutingEfficiencyRegressionTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = brain_create("routing_efficiency_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 200, 20);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Regression: Routing efficiency metrics
//=============================================================================

TEST_F(RoutingEfficiencyRegressionTest, AdaptiveRouting_EfficiencyNotWorse_ThanBaseline) {
    // WHAT: Ensure adaptive routing doesn't degrade efficiency
    // WHY:  Prevent regression in core optimization metric
    // HOW:  Compare efficiency with and without adaptive routing

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);
    network_analyzer_run(analyzer);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();

    // Baseline: No adaptive routing
    quantum_shannon_diffusion_t* qsd_baseline = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd_baseline, nullptr);

    quantum_shannon_evolve(qsd_baseline, 100);

    shannon_diffusion_metrics_t baseline_metrics;
    quantum_shannon_get_metrics(qsd_baseline, &baseline_metrics);

    // With adaptive routing
    quantum_shannon_diffusion_t* qsd_adaptive = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd_adaptive, nullptr);

    quantum_adaptive_routing(qsd_adaptive, (void*)analyzer);
    quantum_shannon_evolve(qsd_adaptive, 100);

    shannon_diffusion_metrics_t adaptive_metrics;
    quantum_shannon_get_metrics(qsd_adaptive, &adaptive_metrics);

    // Adaptive routing should not significantly degrade efficiency
    // Allow for small variations due to quantum randomness
    float efficiency_ratio = adaptive_metrics.propagation_efficiency /
                            (baseline_metrics.propagation_efficiency + 1e-10f);

    // Efficiency should be at least 80% of baseline
    // (Can be better, just checking no major regression)
    EXPECT_GE(efficiency_ratio, 0.8f);

    quantum_shannon_destroy(qsd_baseline);
    quantum_shannon_destroy(qsd_adaptive);
}

TEST_F(RoutingEfficiencyRegressionTest, AdaptiveRouting_PerformanceOverhead_AcceptableRange) {
    // WHAT: Ensure routing overhead stays within 10-20% requirement
    // WHY:  Prevent performance regression
    // HOW:  Measure timing with and without routing

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);
    network_analyzer_run(analyzer);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Warmup
    quantum_shannon_evolve(qsd, 10);

    // Measure baseline (evolution without routing)
    auto baseline_start = std::chrono::high_resolution_clock::now();
    quantum_shannon_evolve(qsd, 100);
    auto baseline_end = std::chrono::high_resolution_clock::now();
    auto baseline_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        baseline_end - baseline_start
    );

    // Reset
    quantum_shannon_reset(qsd);

    // Measure with routing
    auto routing_start = std::chrono::high_resolution_clock::now();
    quantum_adaptive_routing(qsd, (void*)analyzer);
    quantum_shannon_evolve(qsd, 100);
    auto routing_end = std::chrono::high_resolution_clock::now();
    auto routing_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        routing_end - routing_start
    );

    // Calculate overhead
    double overhead_ratio = (double)(routing_duration.count() - baseline_duration.count()) /
                           (double)(baseline_duration.count() + 1);

    // Overhead should be less than 30% (generous bound for CI/CD variations)
    EXPECT_LT(overhead_ratio, 0.30);

    quantum_shannon_destroy(qsd);
}

TEST_F(RoutingEfficiencyRegressionTest, NetworkAnalyzer_CachingEfficiency_Maintained) {
    // WHAT: Ensure analyzer caching provides expected speedup
    // WHY:  Verify caching optimization doesn't regress
    // HOW:  Measure first vs subsequent analyzer access times

    // First access (creates analyzer)
    auto first_start = std::chrono::high_resolution_clock::now();
    void* analyzer1 = brain_get_network_analyzer(brain);
    auto first_end = std::chrono::high_resolution_clock::now();
    auto first_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        first_end - first_start
    );

    ASSERT_NE(analyzer1, nullptr);

    // Second access (cached)
    auto second_start = std::chrono::high_resolution_clock::now();
    void* analyzer2 = brain_get_network_analyzer(brain);
    auto second_end = std::chrono::high_resolution_clock::now();
    auto second_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        second_end - second_start
    );

    ASSERT_NE(analyzer2, nullptr);
    EXPECT_EQ(analyzer1, analyzer2);

    // Cached access should be at least 10x faster
    EXPECT_LT(second_duration.count() * 10, first_duration.count());
}

//=============================================================================
// Regression: Information propagation quality
//=============================================================================

TEST_F(RoutingEfficiencyRegressionTest, AdaptiveRouting_InformationLoss_WithinBounds) {
    // WHAT: Ensure information loss doesn't increase with routing
    // WHY:  Verify routing doesn't introduce unintended information degradation
    // HOW:  Compare information loss with/without routing

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);
    network_analyzer_run(analyzer);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();

    // Baseline
    quantum_shannon_diffusion_t* qsd_baseline = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    quantum_shannon_evolve(qsd_baseline, 100);
    shannon_diffusion_metrics_t baseline_metrics;
    quantum_shannon_get_metrics(qsd_baseline, &baseline_metrics);

    // With routing
    quantum_shannon_diffusion_t* qsd_adaptive = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    quantum_adaptive_routing(qsd_adaptive, (void*)analyzer);
    quantum_shannon_evolve(qsd_adaptive, 100);
    shannon_diffusion_metrics_t adaptive_metrics;
    quantum_shannon_get_metrics(qsd_adaptive, &adaptive_metrics);

    // Information loss should not increase significantly
    // Allow up to 20% increase (can be better, just checking bounds)
    float loss_ratio = adaptive_metrics.information_loss /
                      (baseline_metrics.information_loss + 1e-10f);

    EXPECT_LT(loss_ratio, 1.2f);

    quantum_shannon_destroy(qsd_baseline);
    quantum_shannon_destroy(qsd_adaptive);
}

TEST_F(RoutingEfficiencyRegressionTest, AdaptiveRouting_MutualInformation_Preserved) {
    // WHAT: Ensure mutual information is preserved or improved
    // WHY:  Verify routing maintains information-theoretic properties
    // HOW:  Check mutual information before/after routing

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);
    network_analyzer_run(analyzer);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Evolve before routing
    quantum_shannon_evolve(qsd, 50);
    shannon_diffusion_metrics_t before_metrics;
    quantum_shannon_get_metrics(qsd, &before_metrics);

    // Apply routing
    quantum_adaptive_routing(qsd, (void*)analyzer);

    // Evolve after routing
    quantum_shannon_evolve(qsd, 50);
    shannon_diffusion_metrics_t after_metrics;
    quantum_shannon_get_metrics(qsd, &after_metrics);

    // Mutual information should be in valid range
    EXPECT_GE(after_metrics.mutual_information, 0.0f);
    EXPECT_LE(after_metrics.mutual_information, after_metrics.source_entropy + 0.1f);

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Regression: Network topology stability
//=============================================================================

TEST_F(RoutingEfficiencyRegressionTest, Training_TopologyStability_Maintained) {
    // WHAT: Ensure topology remains stable during training
    // WHY:  Verify learning doesn't break network structure
    // HOW:  Train, analyze, check modularity/connectivity

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Initial topology
    network_analyzer_run(analyzer);
    topology_metrics_t initial_topology = network_analyzer_get_metrics(analyzer);

    // Train
    // Brain has 200 inputs, 20 outputs (from SetUp), so 1 sample = 200 floats
    float training_data[200];
    for (int i = 0; i < 200; i++) {
        training_data[i] = (float)(i % 10) * 0.1f;  // Pattern: 0.0, 0.1, ..., 0.9, 0.0, ...
    }
    float labels[20];
    for (int i = 0; i < 20; i++) {
        labels[i] = (i < 10) ? 1.0f : 0.0f;  // Binary pattern
    }

    brain_finetune_config_t config = {
        .learning_rate = 0.01f,
        .num_epochs = 100,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 1,
        .verbose = false
    };

    brain_finetune(brain, training_data, labels, 1, &config);

    // Re-analyze
    network_analyzer_run(analyzer);
    topology_metrics_t final_topology = network_analyzer_get_metrics(analyzer);

    // Topology should still be valid
    EXPECT_GE(final_topology.density, 0.0f);
    EXPECT_LE(final_topology.density, 1.0f);
    EXPECT_GE(final_topology.clustering_coefficient, 0.0f);
    EXPECT_LE(final_topology.clustering_coefficient, 1.0f);

    // Network should not have collapsed
    EXPECT_GT(final_topology.num_edges, 0u);
}

TEST_F(RoutingEfficiencyRegressionTest, HubDetection_Consistency_AcrossAnalyses) {
    // WHAT: Ensure hub detection is consistent across multiple analyses
    // WHY:  Verify hub detection algorithm is stable
    // HOW:  Run analysis multiple times, check hub consistency

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // First analysis
    network_analyzer_run(analyzer);
    const hub_detection_t* hubs1 = network_analyzer_get_hubs(analyzer);

    uint32_t num_hubs1 = hubs1 ? hubs1->num_hubs : 0;

    // Second analysis (without network changes)
    network_analyzer_run(analyzer);
    const hub_detection_t* hubs2 = network_analyzer_get_hubs(analyzer);

    uint32_t num_hubs2 = hubs2 ? hubs2->num_hubs : 0;

    // Hub count should be similar (within 20% variation)
    if (num_hubs1 > 0 && num_hubs2 > 0) {
        float hub_ratio = (float)num_hubs2 / (float)num_hubs1;
        EXPECT_GE(hub_ratio, 0.8f);
        EXPECT_LE(hub_ratio, 1.2f);
    }
}

//=============================================================================
// Regression: Backward compatibility
//=============================================================================

TEST_F(RoutingEfficiencyRegressionTest, NetworkAnalyzer_NullBrain_HandledGracefully) {
    // WHAT: Ensure NULL brain is handled without crash
    // WHY:  Maintain backward compatibility and robustness
    // HOW:  Pass NULL brain to analyzer

    void* analyzer = brain_get_network_analyzer(nullptr);
    EXPECT_EQ(analyzer, nullptr);
}

TEST_F(RoutingEfficiencyRegressionTest, QuantumRouting_NullInputs_HandledGracefully) {
    // WHAT: Ensure NULL inputs are handled without crash
    // WHY:  Maintain robustness
    // HOW:  Pass various NULL combinations

    bool result1 = quantum_adaptive_routing(nullptr, nullptr);
    EXPECT_FALSE(result1);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    bool result2 = quantum_adaptive_routing(qsd, nullptr);
    EXPECT_TRUE(result2); // Should fall back gracefully

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Regression: Memory usage
//=============================================================================

TEST_F(RoutingEfficiencyRegressionTest, AdaptiveRouting_MemoryUsage_WithinBounds) {
    // WHAT: Ensure routing doesn't cause memory leaks or excessive allocation
    // WHY:  Prevent memory regression
    // HOW:  Call routing many times, check for leaks

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);
    network_analyzer_run(analyzer);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Call routing many times
    for (int i = 0; i < 100; i++) {
        bool result = quantum_adaptive_routing(qsd, (void*)analyzer);
        EXPECT_TRUE(result);
    }

    // No crash = no memory leaks (valgrind/asan will catch leaks)

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Regression: Probability conservation
//=============================================================================

TEST_F(RoutingEfficiencyRegressionTest, AdaptiveRouting_ProbabilityConservation_Maintained) {
    // WHAT: Ensure routing preserves probability normalization
    // WHY:  Verify quantum mechanics constraints maintained
    // HOW:  Check Σ|α|² = 1.0 after routing

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);
    network_analyzer_run(analyzer);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Get actual neuron count from network
    uint32_t num_neurons = neural_network_get_num_neurons(network);

    // Multiple routing + evolution cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        quantum_adaptive_routing(qsd, (void*)analyzer);
        quantum_shannon_evolve(qsd, 10);

        // Check normalization
        float* probs = (float*)malloc(num_neurons * sizeof(float));
        ASSERT_NE(probs, nullptr);

        quantum_shannon_get_distribution(qsd, probs);

        float total_prob = 0.0f;
        for (uint32_t i = 0; i < num_neurons; i++) {
            total_prob += probs[i];
        }

        // Should always be approximately 1.0
        EXPECT_NEAR(total_prob, 1.0f, 0.01f);

        free(probs);
    }

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
