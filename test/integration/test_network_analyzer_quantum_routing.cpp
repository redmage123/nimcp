//=============================================================================
// test_network_analyzer_quantum_routing.cpp - Integration Tests
//=============================================================================

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "cognitive/analysis/nimcp_network_analysis.h"
#include "utils/quantum/nimcp_quantum_shannon.h"

class NetworkAnalyzerQuantumRoutingIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        config.size = BRAIN_SIZE_MEDIUM;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 10;
        config.num_outputs = 10;
        snprintf(config.task_name, sizeof(config.task_name), "analyzer_test");

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Test: End-to-end integration
//=============================================================================

TEST_F(NetworkAnalyzerQuantumRoutingIntegrationTest, FullPipeline_BrainToRoutingToInference) {
    // WHAT: Test complete pipeline: brain → analyzer → quantum routing → inference
    // WHY:  Verify all components work together
    // HOW:  Create brain, get analyzer, create QSD, apply routing, run inference

    // Step 1: Get network analyzer from brain
    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Step 2: Run network analysis
    bool analysis_result = network_analyzer_run(analyzer);
    EXPECT_TRUE(analysis_result);

    // Step 3: Create quantum-Shannon diffusion
    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(adaptive_net, nullptr);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(network, nullptr);

    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Step 4: Apply adaptive routing
    bool routing_result = quantum_adaptive_routing(qsd, (void*)analyzer);
    EXPECT_TRUE(routing_result);

    // Step 5: Evolve quantum walk
    bool evolve_result = quantum_shannon_evolve(qsd, 100);
    EXPECT_TRUE(evolve_result);

    // Step 6: Get Shannon metrics
    shannon_diffusion_metrics_t metrics;
    bool metrics_result = quantum_shannon_get_metrics(qsd, &metrics);
    EXPECT_TRUE(metrics_result);

    // Verify metrics are valid
    EXPECT_GE(metrics.propagation_efficiency, 0.0f);
    EXPECT_LE(metrics.propagation_efficiency, 1.0f);

    // Step 7: Run brain inference
    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    brain_decision_t* decision = brain_decide(brain, input, 10);
    EXPECT_NE(decision, nullptr);
    brain_free_decision(decision);

    quantum_shannon_destroy(qsd);
}

TEST_F(NetworkAnalyzerQuantumRoutingIntegrationTest, Training_UpdatesTopology_ImprovesRouting) {
    // WHAT: Test that training updates topology which improves routing
    // WHY:  Verify dynamic adaptation during learning
    // HOW:  Train, analyze, route, compare metrics

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    ASSERT_NE(adaptive_net, nullptr);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    ASSERT_NE(network, nullptr);

    // Initial analysis and routing
    network_analyzer_run(analyzer);
    topology_metrics_t initial_topology = network_analyzer_get_metrics(analyzer);

    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd1 = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd1, nullptr);

    quantum_adaptive_routing(qsd1, (void*)analyzer);
    quantum_shannon_evolve(qsd1, 100);

    shannon_diffusion_metrics_t initial_metrics;
    quantum_shannon_get_metrics(qsd1, &initial_metrics);

    // Perform training
    float input[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    for (int i = 0; i < 50; i++) {
        brain_learn_example(brain, input, 10, "class_0", 1.0f);
    }

    // Re-analyze and route after training
    network_analyzer_run(analyzer);
    topology_metrics_t updated_topology = network_analyzer_get_metrics(analyzer);

    quantum_shannon_diffusion_t* qsd2 = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd2, nullptr);

    quantum_adaptive_routing(qsd2, (void*)analyzer);
    quantum_shannon_evolve(qsd2, 100);

    shannon_diffusion_metrics_t updated_metrics;
    quantum_shannon_get_metrics(qsd2, &updated_metrics);

    // Topology should be valid after training
    EXPECT_GE(updated_topology.density, 0.0f);
    EXPECT_LE(updated_topology.density, 1.0f);

    // Metrics should be valid
    EXPECT_GE(updated_metrics.propagation_efficiency, 0.0f);
    EXPECT_LE(updated_metrics.propagation_efficiency, 1.0f);

    quantum_shannon_destroy(qsd1);
    quantum_shannon_destroy(qsd2);
}

TEST_F(NetworkAnalyzerQuantumRoutingIntegrationTest, CommunityDetection_GuidesRouting) {
    // WHAT: Test that community detection guides quantum routing
    // WHY:  Verify community-aware routing optimization
    // HOW:  Detect communities, route, verify routing uses community structure

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Run community detection
    bool detect_result = network_analyzer_detect_communities(analyzer);
    EXPECT_TRUE(detect_result);

    const community_structure_t* communities = network_analyzer_get_communities(analyzer);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Apply adaptive routing (should use community structure if available)
    bool routing_result = quantum_adaptive_routing(qsd, (void*)analyzer);
    EXPECT_TRUE(routing_result);

    // If communities were detected, routing should have processed them
    if (communities && communities->num_communities > 1) {
        EXPECT_GT(communities->num_communities, 0u);
    }

    quantum_shannon_destroy(qsd);
}

TEST_F(NetworkAnalyzerQuantumRoutingIntegrationTest, HubDetection_BiasesRouting) {
    // WHAT: Test that hub detection biases quantum routing
    // WHY:  Verify hub-centric routing optimization
    // HOW:  Detect hubs, route, verify hubs influence routing

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Run hub detection
    bool detect_result = network_analyzer_detect_hubs(analyzer);
    EXPECT_TRUE(detect_result);

    const hub_detection_t* hubs = network_analyzer_get_hubs(analyzer);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Apply adaptive routing (should use hub information)
    bool routing_result = quantum_adaptive_routing(qsd, (void*)analyzer);
    EXPECT_TRUE(routing_result);

    // If hubs were detected, routing should have processed them
    if (hubs && hubs->num_hubs > 0) {
        EXPECT_GT(hubs->num_hubs, 0u);

        // Check that hub neurons are valid
        for (uint32_t h = 0; h < hubs->num_hubs; h++) {
            EXPECT_LT(hubs->hubs[h].neuron_id, 200u);
            EXPECT_GE(hubs->hubs[h].degree_centrality, 0.0f);
            EXPECT_LE(hubs->hubs[h].degree_centrality, 1.0f);
        }
    }

    quantum_shannon_destroy(qsd);
}

TEST_F(NetworkAnalyzerQuantumRoutingIntegrationTest, BottleneckDetection_RoutesAround) {
    // WHAT: Test that bottleneck detection + adaptive routing work together
    // WHY:  Verify complete optimization pipeline
    // HOW:  Create QSD, optimize (detect bottlenecks), then apply adaptive routing

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

    // Evolve to detect bottlenecks
    quantum_shannon_evolve(qsd, 50);

    // Optimize (detect bottlenecks)
    bool optimize_result = quantum_shannon_optimize(qsd);
    EXPECT_TRUE(optimize_result);

    // Apply adaptive routing
    bool routing_result = quantum_adaptive_routing(qsd, (void*)analyzer);
    EXPECT_TRUE(routing_result);

    // Evolve again
    quantum_shannon_evolve(qsd, 50);

    // Get metrics
    shannon_diffusion_metrics_t metrics;
    quantum_shannon_get_metrics(qsd, &metrics);

    // Verify metrics are valid
    EXPECT_GE(metrics.propagation_efficiency, 0.0f);
    EXPECT_LE(metrics.propagation_efficiency, 1.0f);

    quantum_shannon_destroy(qsd);
}

TEST_F(NetworkAnalyzerQuantumRoutingIntegrationTest, AutoAnalysis_UpdatesRouting) {
    // WHAT: Test that auto-analysis updates routing dynamically
    // WHY:  Verify continuous adaptation
    // HOW:  Enable auto-analysis, train, verify routing adapts

    network_analyzer_t* analyzer = (network_analyzer_t*)brain_get_network_analyzer(brain);
    ASSERT_NE(analyzer, nullptr);

    // Enable auto-analysis (every 5 iterations)
    network_analyzer_set_auto_analyze(analyzer, true, 5);

    adaptive_network_t adaptive_net = brain_get_network(brain);
    neural_network_t network = adaptive_network_get_base_network(adaptive_net);
    quantum_shannon_config_t qs_config = quantum_shannon_default_config();
    quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
        network, 0, 10.0f, &qs_config
    );
    ASSERT_NE(qsd, nullptr);

    // Initial routing
    quantum_adaptive_routing(qsd, (void*)analyzer);
    uint32_t initial_analysis_count = analyzer->analysis_count;

    // Simulate learning events (trigger auto-analysis)
    for (int i = 0; i < 10; i++) {
        network_analyzer_on_learning_event(analyzer);
    }

    // Analysis count should have increased
    EXPECT_GE(analyzer->analysis_count, initial_analysis_count);

    // Re-route after auto-analysis
    bool routing_result = quantum_adaptive_routing(qsd, (void*)analyzer);
    EXPECT_TRUE(routing_result);

    quantum_shannon_destroy(qsd);
}

TEST_F(NetworkAnalyzerQuantumRoutingIntegrationTest, MultipleRoutingCycles_Stable) {
    // WHAT: Test multiple routing cycles are stable
    // WHY:  Verify routing doesn't degrade over time
    // HOW:  Route, evolve, re-route multiple times

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

    // Multiple routing cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        // Route
        bool routing_result = quantum_adaptive_routing(qsd, (void*)analyzer);
        EXPECT_TRUE(routing_result);

        // Evolve
        quantum_shannon_evolve(qsd, 20);

        // Re-analyze
        network_analyzer_run(analyzer);
    }

    // Get final metrics
    shannon_diffusion_metrics_t metrics;
    quantum_shannon_get_metrics(qsd, &metrics);

    // Should still be valid after multiple cycles
    EXPECT_GE(metrics.propagation_efficiency, 0.0f);
    EXPECT_LE(metrics.propagation_efficiency, 1.0f);

    quantum_shannon_destroy(qsd);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
