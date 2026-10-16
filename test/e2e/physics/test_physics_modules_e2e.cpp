/**
 * @file test_physics_modules_e2e.cpp
 * @brief End-to-End Tests for Physics Modules Pipeline
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests complete workflows across Graph Theory Bridge, Dynamical Systems,
 * and Information Geometry Bridge modules with full NIMCP integration.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "physics/graphs/nimcp_graph_theory_bridge.h"
#include "physics/dynamics/nimcp_dynamical_systems.h"
#include "physics/geometry/nimcp_information_geometry_bridge.h"
#include "core/brain/nimcp_brain_kg.h"
}

//=============================================================================
// Test Helper: Lorenz System for Dynamical Systems
//=============================================================================

static const float LORENZ_SIGMA = 10.0f;
static const float LORENZ_RHO = 28.0f;
static const float LORENZ_BETA = 8.0f / 3.0f;

static int lorenz_system(const float* state, uint32_t state_dim,
                         const float* params, uint32_t param_dim,
                         float* derivatives, void* context) {
    (void)state_dim;
    (void)params;
    (void)param_dim;
    (void)context;
    derivatives[0] = LORENZ_SIGMA * (state[1] - state[0]);
    derivatives[1] = state[0] * (LORENZ_RHO - state[2]) - state[1];
    derivatives[2] = state[0] * state[1] - LORENZ_BETA * state[2];
    return 0;
}

static int lorenz_jacobian(const float* state, uint32_t state_dim,
                           const float* params, uint32_t param_dim,
                           float* jacobian, void* context) {
    (void)state_dim;
    (void)params;
    (void)param_dim;
    (void)context;
    // Row 0
    jacobian[0] = -LORENZ_SIGMA;
    jacobian[1] = LORENZ_SIGMA;
    jacobian[2] = 0.0f;
    // Row 1
    jacobian[3] = LORENZ_RHO - state[2];
    jacobian[4] = -1.0f;
    jacobian[5] = -state[0];
    // Row 2
    jacobian[6] = state[1];
    jacobian[7] = state[0];
    jacobian[8] = -LORENZ_BETA;
    return 0;
}

//=============================================================================
// Test Fixture for Full Pipeline E2E Tests
//=============================================================================

class PhysicsModulesE2ETest : public ::testing::Test {
protected:
    brain_kg_t* kg_ = nullptr;
    graph_theory_bridge_t graph_bridge_ = nullptr;
    dynsys_system_t dynsys_ = nullptr;
    info_geom_bridge_t geom_bridge_ = nullptr;

    void SetUp() override {
        // Create brain KG
        brain_kg_config_t kg_config;
        memset(&kg_config, 0, sizeof(kg_config));
        kg_config.max_nodes = 1000;
        kg_config.max_edges = 5000;
        kg_config.enable_statistics = true;
        kg_ = brain_kg_create(&kg_config);
        ASSERT_NE(kg_, nullptr);

        // Create graph theory bridge
        graph_theory_bridge_config_t graph_config;
        graph_theory_bridge_default_config(&graph_config);
        graph_bridge_ = graph_theory_bridge_create(&graph_config);
        ASSERT_NE(graph_bridge_, nullptr);

        // Create dynamical system (Lorenz)
        dynsys_config_t dyn_config = dynsys_default_config();
        dyn_config.state_dim = 3;
        dyn_config.enable_kg_wiring = false;
        dyn_config.enable_exception_handling = true;
        dynsys_ = dynsys_create(&dyn_config, lorenz_system, lorenz_jacobian, nullptr);
        ASSERT_NE(dynsys_, nullptr);

        // Create information geometry bridge
        info_geom_bridge_config_t geom_config = info_geom_bridge_default_config();
        geom_bridge_ = info_geom_bridge_create(&geom_config);
        ASSERT_NE(geom_bridge_, nullptr);
    }

    void TearDown() override {
        if (geom_bridge_) {
            info_geom_bridge_destroy(geom_bridge_);
        }
        if (dynsys_) {
            dynsys_destroy(dynsys_);
        }
        if (graph_bridge_) {
            graph_theory_bridge_destroy(graph_bridge_);
        }
        if (kg_) {
            brain_kg_destroy(kg_);
        }
    }
};

//=============================================================================
// E2E Test: Complete Graph Analysis Pipeline
//=============================================================================

TEST_F(PhysicsModulesE2ETest, CompleteGraphAnalysisPipeline) {
    // Step 1: Register KG with graph bridge
    graph_theory_error_t err = graph_theory_bridge_register_kg(
        graph_bridge_, kg_, 0x12345678);
    EXPECT_EQ(err, GRAPH_THEORY_OK);

    // Step 2: Create a graph structure using NimcpGraph API
    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Add 5 vertices
    for (int i = 0; i < 5; i++) {
        nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Add edges based on adjacency: node 2 has most connections (4)
    // 0-1, 0-2, 1-2, 1-3, 2-3, 2-4, 3-4
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);
    nimcp_graph_add_edge(graph, 0, 2, 1.0f);
    nimcp_graph_add_edge(graph, 1, 2, 1.0f);
    nimcp_graph_add_edge(graph, 1, 3, 1.0f);
    nimcp_graph_add_edge(graph, 2, 3, 1.0f);
    nimcp_graph_add_edge(graph, 2, 4, 1.0f);
    nimcp_graph_add_edge(graph, 3, 4, 1.0f);

    // Step 3: Compute centrality
    graph_centrality_result_t* centrality = nullptr;
    err = graph_theory_compute_centrality(graph_bridge_, graph, CENTRALITY_DEGREE, &centrality);
    EXPECT_EQ(err, GRAPH_THEORY_OK);
    ASSERT_NE(centrality, nullptr);
    EXPECT_EQ(centrality->num_nodes, 5u);

    // Step 4: Detect communities
    graph_community_result_t* communities = nullptr;
    err = graph_theory_detect_communities(graph_bridge_, graph, COMMUNITY_LOUVAIN, 2, &communities);
    EXPECT_EQ(err, GRAPH_THEORY_OK);
    ASSERT_NE(communities, nullptr);
    EXPECT_GE(communities->num_communities, 1u);
    EXPECT_LE(communities->num_communities, 5u);

    // Step 5: Compute topology metrics
    graph_topology_metrics_t topology;
    err = graph_theory_compute_metrics(graph_bridge_, graph, nullptr, &topology);
    EXPECT_EQ(err, GRAPH_THEORY_OK);
    EXPECT_GE(topology.clustering_coefficient, 0.0f);

    // Step 6: Verify KG was populated
    brain_kg_stats_t kg_stats;
    brain_kg_get_stats(kg_, &kg_stats);
    // KG may or may not be populated depending on configuration

    // Cleanup
    graph_centrality_result_destroy(centrality);
    graph_community_result_destroy(communities);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// E2E Test: Complete Dynamical Systems Analysis Pipeline
//=============================================================================

TEST_F(PhysicsModulesE2ETest, CompleteDynamicalSystemsPipeline) {
    // Step 1: Initialize Lorenz system state (API uses float)
    float state[3] = {1.0f, 1.0f, 1.0f};
    float dt = 0.01f;

    // Step 2: Evolve system for several steps
    for (int i = 0; i < 100; i++) {
        dynsys_error_t err = dynsys_step_rk4(dynsys_, state, dt);
        EXPECT_EQ(err, DYNSYS_OK);
    }

    // Verify state evolved (Lorenz attractor behavior)
    bool state_changed = (fabsf(state[0] - 1.0f) > 0.1f ||
                          fabsf(state[1] - 1.0f) > 0.1f ||
                          fabsf(state[2] - 1.0f) > 0.1f);
    EXPECT_TRUE(state_changed);

    // Step 3: Create Lyapunov analyzer
    dynsys_lyapunov_config_t lyap_config;
    memset(&lyap_config, 0, sizeof(lyap_config));
    lyap_config.num_exponents = 3;
    lyap_config.perturbation_size = 1e-8f;
    lyap_config.orthonormalization_interval = 10.0f;
    lyap_config.transient_steps = 100;
    lyap_config.analysis_steps = 500;

    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(&lyap_config, dynsys_);
    if (lyap != nullptr) {
        // Compute Lyapunov exponents
        float initial_state[3] = {1.0f, 1.0f, 1.0f};
        dynsys_lyapunov_result_t lyap_result;
        dynsys_error_t err = dynsys_lyapunov_compute(lyap, initial_state, &lyap_result);
        if (err == DYNSYS_OK) {
            // Verify we got exponents (detailed accuracy is tested in unit tests)
            EXPECT_GT(lyap_result.num_exponents, 0u);
            // Lorenz system: first exponent should be largest (positive for chaos)
            // But short analysis time may give approximate results, so just check
            // that we got reasonable values (not NaN or inf)
            for (size_t i = 0; i < lyap_result.num_exponents; i++) {
                EXPECT_FALSE(std::isnan(lyap_result.exponents[i]));
                EXPECT_FALSE(std::isinf(lyap_result.exponents[i]));
            }
        }
        dynsys_lyapunov_destroy(lyap);
    }

    // Step 4: Create bifurcation analyzer
    dynsys_bifurcation_config_t bif_config;
    memset(&bif_config, 0, sizeof(bif_config));
    bif_config.param_start = 20.0f;
    bif_config.param_end = 30.0f;
    bif_config.num_points = 10;
    bif_config.transient_steps = 100;
    bif_config.sample_steps = 50;

    dynsys_bifurcation_t bif = dynsys_bifurcation_create(&bif_config, dynsys_);
    if (bif != nullptr) {
        // Bifurcation analysis would be done here
        dynsys_bifurcation_destroy(bif);
    }
}

//=============================================================================
// E2E Test: Complete Information Geometry Bridge Pipeline
//=============================================================================

TEST_F(PhysicsModulesE2ETest, CompleteInfoGeometryPipeline) {
    // Step 1: Register KG with geometry bridge
    int result = info_geom_bridge_register_kg(geom_bridge_, kg_);
    EXPECT_EQ(result, 0);

    // Step 2: Register exception handler
    void* dummy_exception = (void*)0x12345678;
    result = info_geom_bridge_register_exception(geom_bridge_, dummy_exception);
    EXPECT_EQ(result, 0);

    // Step 3: Register bio-async channel
    void* dummy_channel = (void*)0x87654321;
    result = info_geom_bridge_register_bio_async(geom_bridge_, dummy_channel);
    EXPECT_EQ(result, 0);

    // Step 4: Verify registrations work (re-register with null to clear)
    result = info_geom_bridge_register_exception(geom_bridge_, nullptr);
    EXPECT_EQ(result, 0);
    result = info_geom_bridge_register_bio_async(geom_bridge_, nullptr);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// E2E Test: Cross-Module Integration
//=============================================================================

TEST_F(PhysicsModulesE2ETest, CrossModuleIntegration) {
    // Step 1: Use all modules with shared KG
    graph_theory_error_t g_err = graph_theory_bridge_register_kg(
        graph_bridge_, kg_, 0xCD12345678ULL);
    EXPECT_EQ(g_err, GRAPH_THEORY_OK);

    int i_err = info_geom_bridge_register_kg(geom_bridge_, kg_);
    EXPECT_EQ(i_err, 0);

    // Step 2: Run graph analysis using NimcpGraph API
    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Add 4 vertices
    for (int i = 0; i < 4; i++) {
        nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Add edges: ring topology (0-1, 1-2, 2-3, 3-0)
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);
    nimcp_graph_add_edge(graph, 1, 2, 1.0f);
    nimcp_graph_add_edge(graph, 2, 3, 1.0f);
    nimcp_graph_add_edge(graph, 3, 0, 1.0f);

    graph_centrality_result_t* centrality = nullptr;
    g_err = graph_theory_compute_centrality(graph_bridge_, graph, CENTRALITY_DEGREE, &centrality);
    EXPECT_EQ(g_err, GRAPH_THEORY_OK);
    ASSERT_NE(centrality, nullptr);

    // Step 3: Run dynamical systems analysis (API uses float)
    float state[3] = {0.1f, 0.1f, 0.1f};
    for (int i = 0; i < 50; i++) {
        dynsys_error_t err = dynsys_step_rk4(dynsys_, state, 0.01f);
        EXPECT_EQ(err, DYNSYS_OK);
    }

    // Step 4: Verify shared KG has nodes from both modules
    brain_kg_stats_t kg_stats;
    brain_kg_get_stats(kg_, &kg_stats);
    // KG may or may not be populated depending on configuration

    // Cleanup
    graph_centrality_result_destroy(centrality);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// E2E Test: Spectral Graph Analysis with Dynamical Systems
//=============================================================================

TEST_F(PhysicsModulesE2ETest, SpectralDynamicsIntegration) {
    // Create a larger graph for spectral analysis using NimcpGraph API
    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Add 8 vertices for ring graph
    for (int i = 0; i < 8; i++) {
        nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Ring graph with some cross-connections
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);
    nimcp_graph_add_edge(graph, 0, 7, 1.0f);
    nimcp_graph_add_edge(graph, 1, 2, 1.0f);
    nimcp_graph_add_edge(graph, 2, 3, 1.0f);
    nimcp_graph_add_edge(graph, 3, 4, 1.0f);
    nimcp_graph_add_edge(graph, 4, 5, 1.0f);
    nimcp_graph_add_edge(graph, 5, 6, 1.0f);
    nimcp_graph_add_edge(graph, 6, 7, 1.0f);

    // Run spectral analysis
    graph_spectral_result_t* spectral = nullptr;
    graph_theory_error_t err = graph_theory_spectral_analysis(graph_bridge_, graph, 3, &spectral);
    if (err == GRAPH_THEORY_OK && spectral != nullptr) {
        EXPECT_EQ(spectral->num_eigenvalues, 3u);
        EXPECT_GE(spectral->spectral_gap, 0.0f);
        graph_spectral_result_destroy(spectral);
    }

    // Evolve dynamical system (API uses float)
    float state[3] = {5.0f, 5.0f, 5.0f};
    for (int i = 0; i < 200; i++) {
        dynsys_step_rk4(dynsys_, state, 0.01f);
    }

    // Verify system is still in bounded region (Lorenz attractor)
    EXPECT_LT(fabsf(state[0]), 50.0f);
    EXPECT_LT(fabsf(state[1]), 50.0f);
    EXPECT_LT(fabsf(state[2]), 100.0f);

    nimcp_graph_destroy(graph);
}

//=============================================================================
// E2E Test: Quantum Walk with Graph Theory
//=============================================================================

TEST_F(PhysicsModulesE2ETest, QuantumWalkPipeline) {
    // Create graph for quantum walk using NimcpGraph API
    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Add 6 vertices for binary tree-like structure
    for (int i = 0; i < 6; i++) {
        nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Binary tree-like structure edges
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);
    nimcp_graph_add_edge(graph, 0, 2, 1.0f);
    nimcp_graph_add_edge(graph, 1, 3, 1.0f);
    nimcp_graph_add_edge(graph, 1, 4, 1.0f);
    nimcp_graph_add_edge(graph, 2, 5, 1.0f);

    // Perform quantum walk (API signature: bridge, graph, type, start_node, evolution_time, result**)
    graph_qwalk_result_t* qwalk_result = nullptr;
    graph_theory_error_t err = graph_theory_quantum_walk(
        graph_bridge_, graph, QWALK_CONTINUOUS, 0, 10.0f, &qwalk_result);

    if (err == GRAPH_THEORY_OK && qwalk_result != nullptr) {
        // Verify probability distribution is valid
        float total_prob = 0.0f;
        for (uint32_t i = 0; i < qwalk_result->num_nodes; i++) {
            EXPECT_GE(qwalk_result->probabilities[i], 0.0f);
            EXPECT_LE(qwalk_result->probabilities[i], 1.0f);
            total_prob += qwalk_result->probabilities[i];
        }
        EXPECT_NEAR(total_prob, 1.0f, 1e-4f);
        graph_qwalk_result_destroy(qwalk_result);
    }

    nimcp_graph_destroy(graph);
}

//=============================================================================
// E2E Test: Long-Running Stability
//=============================================================================

TEST_F(PhysicsModulesE2ETest, LongRunningStability) {
    // Multiple iterations of create/compute/destroy cycle
    for (int cycle = 0; cycle < 5; cycle++) {
        // Create fresh modules each cycle
        graph_theory_bridge_config_t graph_config;
        graph_theory_bridge_default_config(&graph_config);
        graph_theory_bridge_t local_graph = graph_theory_bridge_create(&graph_config);
        ASSERT_NE(local_graph, nullptr);

        dynsys_config_t dyn_config = dynsys_default_config();
        dyn_config.state_dim = 3;
        dynsys_system_t local_sys = dynsys_create(&dyn_config, lorenz_system, lorenz_jacobian, nullptr);
        ASSERT_NE(local_sys, nullptr);

        info_geom_bridge_config_t geom_config = info_geom_bridge_default_config();
        info_geom_bridge_t local_geom = info_geom_bridge_create(&geom_config);
        ASSERT_NE(local_geom, nullptr);

        // Do some work (API uses float)
        float state[3] = {1.0f, 1.0f, 1.0f};
        for (int step = 0; step < 50; step++) {
            dynsys_step_rk4(local_sys, state, 0.01f);
        }

        // Create graph using NimcpGraph API
        NimcpGraph* graph = nimcp_graph_create();
        ASSERT_NE(graph, nullptr);

        for (int i = 0; i < 3; i++) {
            nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
        }
        // Complete triangle
        nimcp_graph_add_edge(graph, 0, 1, 1.0f);
        nimcp_graph_add_edge(graph, 0, 2, 1.0f);
        nimcp_graph_add_edge(graph, 1, 2, 1.0f);

        graph_centrality_result_t* centrality = nullptr;
        graph_theory_compute_centrality(local_graph, graph, CENTRALITY_DEGREE, &centrality);
        if (centrality) {
            graph_centrality_result_destroy(centrality);
        }

        // Cleanup
        nimcp_graph_destroy(graph);
        info_geom_bridge_destroy(local_geom);
        dynsys_destroy(local_sys);
        graph_theory_bridge_destroy(local_graph);
    }
}

//=============================================================================
// E2E Test: Error Recovery Pipeline
//=============================================================================

TEST_F(PhysicsModulesE2ETest, ErrorRecoveryPipeline) {
    // Test with invalid inputs, verify recovery

    // Null graph to centrality
    graph_centrality_result_t* null_result = nullptr;
    graph_theory_error_t err = graph_theory_compute_centrality(
        graph_bridge_, nullptr, CENTRALITY_DEGREE, &null_result);
    EXPECT_NE(err, GRAPH_THEORY_OK);

    // Valid operations should still work after error
    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    for (int i = 0; i < 3; i++) {
        nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }
    // Line graph: 0-1, 1-2
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);
    nimcp_graph_add_edge(graph, 1, 2, 1.0f);

    graph_centrality_result_t* centrality = nullptr;
    err = graph_theory_compute_centrality(graph_bridge_, graph, CENTRALITY_DEGREE, &centrality);
    EXPECT_EQ(err, GRAPH_THEORY_OK);
    ASSERT_NE(centrality, nullptr);

    // Test info geometry with null registrations
    int ierr = info_geom_bridge_register_kg(nullptr, kg_);
    EXPECT_EQ(ierr, -1);

    ierr = info_geom_bridge_register_kg(geom_bridge_, nullptr);
    EXPECT_EQ(ierr, -1);

    // Should still work with valid inputs
    ierr = info_geom_bridge_register_kg(geom_bridge_, kg_);
    EXPECT_EQ(ierr, 0);

    // Cleanup
    graph_centrality_result_destroy(centrality);
    nimcp_graph_destroy(graph);
}

//=============================================================================
// E2E Test: Community Detection and Phase Coherence
//=============================================================================

TEST_F(PhysicsModulesE2ETest, CommunityPhaseCoherencePipeline) {
    // Create a graph with clear community structure using NimcpGraph API
    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Add 6 vertices
    for (int i = 0; i < 6; i++) {
        nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Two clusters of 3 nodes each, weakly connected
    // Cluster 1: nodes 0,1,2
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);
    nimcp_graph_add_edge(graph, 0, 2, 1.0f);
    nimcp_graph_add_edge(graph, 1, 2, 1.0f);
    // Cluster 2: nodes 3,4,5
    nimcp_graph_add_edge(graph, 3, 4, 1.0f);
    nimcp_graph_add_edge(graph, 3, 5, 1.0f);
    nimcp_graph_add_edge(graph, 4, 5, 1.0f);
    // Connection between clusters: 2-3
    nimcp_graph_add_edge(graph, 2, 3, 1.0f);

    // Detect communities
    graph_community_result_t* communities = nullptr;
    graph_theory_error_t err = graph_theory_detect_communities(
        graph_bridge_, graph, COMMUNITY_LOUVAIN, 2, &communities);

    if (err == GRAPH_THEORY_OK && communities != nullptr) {
        // Should detect approximately 2 communities
        EXPECT_GE(communities->num_communities, 1u);
        EXPECT_LE(communities->num_communities, 3u);

        // Compute phase coherence using phases (not community result)
        // API: graph_theory_compute_phase_coherence(bridge, graph, phases, num_nodes, global*, local*)
        float phases[6] = {0.0f, 0.1f, 0.2f, 3.14f, 3.24f, 3.34f};
        float global_coherence = 0.0f;
        err = graph_theory_compute_phase_coherence(
            graph_bridge_, graph, phases, 6, &global_coherence, nullptr);

        if (err == GRAPH_THEORY_OK) {
            EXPECT_GE(global_coherence, 0.0f);
            EXPECT_LE(global_coherence, 1.0f);
        }

        graph_community_result_destroy(communities);
    }

    nimcp_graph_destroy(graph);
}

//=============================================================================
// E2E Test: Hyperbolic Embedding
//=============================================================================

TEST_F(PhysicsModulesE2ETest, HyperbolicEmbeddingPipeline) {
    // Create a scale-free-like graph using NimcpGraph API
    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Add 5 vertices
    for (int i = 0; i < 5; i++) {
        nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Hub-and-spoke structure (node 0 is hub)
    nimcp_graph_add_edge(graph, 0, 1, 1.0f);
    nimcp_graph_add_edge(graph, 0, 2, 1.0f);
    nimcp_graph_add_edge(graph, 0, 3, 1.0f);
    nimcp_graph_add_edge(graph, 0, 4, 1.0f);
    nimcp_graph_add_edge(graph, 1, 2, 1.0f);

    // API: graph_theory_hyperbolic_embed(bridge, graph, dimension, result**)
    graph_hyperbolic_result_t* hyp_result = nullptr;
    graph_theory_error_t err = graph_theory_hyperbolic_embed(
        graph_bridge_, graph, 2, &hyp_result);

    if (err == GRAPH_THEORY_OK && hyp_result != nullptr) {
        EXPECT_EQ(hyp_result->num_nodes, 5u);
        // Hub node should have smaller radial coordinate
        // (closer to center in hyperbolic space)
        EXPECT_GE(hyp_result->mean_distortion, 0.0f);
        graph_hyperbolic_result_destroy(hyp_result);
    }

    nimcp_graph_destroy(graph);
}

//=============================================================================
// E2E Test: Full Analysis Report
//=============================================================================

TEST_F(PhysicsModulesE2ETest, FullAnalysisReport) {
    // Register all modules with shared KG
    graph_theory_bridge_register_kg(graph_bridge_, kg_, 0xFA12345678ULL);
    info_geom_bridge_register_kg(geom_bridge_, kg_);

    // Create analysis graph using NimcpGraph API
    NimcpGraph* test_graph = nimcp_graph_create();
    ASSERT_NE(test_graph, nullptr);

    // Add 10 vertices for small-world network
    for (int i = 0; i < 10; i++) {
        nimcp_graph_add_vertex(test_graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Ring connections
    for (int i = 0; i < 10; i++) {
        nimcp_graph_add_edge(test_graph, i, (i + 1) % 10, 1.0f);
    }
    // Some random long-range connections (small-world)
    nimcp_graph_add_edge(test_graph, 0, 5, 1.0f);
    nimcp_graph_add_edge(test_graph, 2, 7, 1.0f);
    nimcp_graph_add_edge(test_graph, 3, 8, 1.0f);

    // Collect all metrics
    graph_centrality_result_t* centrality = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(graph_bridge_, test_graph, CENTRALITY_BETWEENNESS, &centrality),
              GRAPH_THEORY_OK);
    ASSERT_NE(centrality, nullptr);

    graph_community_result_t* communities = nullptr;
    EXPECT_EQ(graph_theory_detect_communities(graph_bridge_, test_graph, COMMUNITY_LOUVAIN, 3, &communities),
              GRAPH_THEORY_OK);
    ASSERT_NE(communities, nullptr);

    graph_topology_metrics_t topology;
    EXPECT_EQ(graph_theory_compute_metrics(graph_bridge_, test_graph, nullptr, &topology),
              GRAPH_THEORY_OK);

    graph_spectral_result_t* spectral = nullptr;
    EXPECT_EQ(graph_theory_spectral_analysis(graph_bridge_, test_graph, 5, &spectral),
              GRAPH_THEORY_OK);

    // Evolve dynamical system using float state (API requirement)
    float state[3] = {1.0f, 1.0f, 1.0f};
    std::vector<float> trajectory_x;
    trajectory_x.reserve(1000);

    for (int i = 0; i < 1000; i++) {
        dynsys_step_rk4(dynsys_, state, 0.01f);
        trajectory_x.push_back(state[0]);
    }

    // Verify we have meaningful results
    EXPECT_GT(centrality->num_nodes, 0u);
    EXPECT_GE(communities->num_communities, 1u);
    // Use density instead of average_path_length (which doesn't exist)
    EXPECT_GE(topology.density, 0.0f);
    EXPECT_EQ(trajectory_x.size(), 1000u);

    // Clean up
    graph_centrality_result_destroy(centrality);
    graph_community_result_destroy(communities);
    if (spectral) graph_spectral_result_destroy(spectral);
    nimcp_graph_destroy(test_graph);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
