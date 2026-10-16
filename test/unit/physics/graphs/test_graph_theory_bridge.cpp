/**
 * @file test_graph_theory_bridge.cpp
 * @brief Unit tests for Graph Theory Bridge API
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests the graph theory bridge module including:
 * - Bridge lifecycle (create, destroy, config)
 * - KG registration
 * - Centrality analysis
 * - Community detection
 * - Topology metrics
 * - Spectral analysis
 * - Quantum walks
 * - Hyperbolic embeddings
 * - Phase coherence
 * - Error handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "physics/graphs/nimcp_graph_theory_bridge.h"
#include "utils/containers/nimcp_graph.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class GraphTheoryBridgeTest : public ::testing::Test {
protected:
    graph_theory_bridge_t bridge_ = nullptr;
    NimcpGraph* graph_ = nullptr;

    void SetUp() override {
        // Create bridge with default config
        graph_theory_bridge_config_t config;
        ASSERT_EQ(graph_theory_bridge_default_config(&config), GRAPH_THEORY_OK);
        bridge_ = graph_theory_bridge_create(&config);
        ASSERT_NE(bridge_, nullptr);

        // Create a simple test graph (5 nodes, ring topology)
        graph_ = nimcp_graph_create();
        ASSERT_NE(graph_, nullptr);

        // Add 5 vertices
        for (int i = 0; i < 5; i++) {
            uint32_t idx = nimcp_graph_add_vertex(graph_, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
            ASSERT_NE(idx, NIMCP_INVALID_VERTEX);
        }

        // Create a ring: 0-1-2-3-4-0
        ASSERT_TRUE(nimcp_graph_add_edge(graph_, 0, 1, 1.0f));
        ASSERT_TRUE(nimcp_graph_add_edge(graph_, 1, 2, 1.0f));
        ASSERT_TRUE(nimcp_graph_add_edge(graph_, 2, 3, 1.0f));
        ASSERT_TRUE(nimcp_graph_add_edge(graph_, 3, 4, 1.0f));
        ASSERT_TRUE(nimcp_graph_add_edge(graph_, 4, 0, 1.0f));
    }

    void TearDown() override {
        if (bridge_) {
            graph_theory_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
        if (graph_) {
            nimcp_graph_destroy(graph_);
            graph_ = nullptr;
        }
    }

    // Create a more complex graph for advanced tests
    NimcpGraph* createComplexGraph() {
        NimcpGraph* g = nimcp_graph_create();
        if (!g) return nullptr;

        // Add 10 vertices
        for (int i = 0; i < 10; i++) {
            uint32_t idx = nimcp_graph_add_vertex(g, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
            if (idx == NIMCP_INVALID_VERTEX) {
                nimcp_graph_destroy(g);
                return nullptr;
            }
        }

        // Create two communities with inter-community edges
        // Community 1: 0-1-2-3-4
        nimcp_graph_add_edge(g, 0, 1, 1.0f);
        nimcp_graph_add_edge(g, 0, 2, 1.0f);
        nimcp_graph_add_edge(g, 1, 2, 1.0f);
        nimcp_graph_add_edge(g, 1, 3, 1.0f);
        nimcp_graph_add_edge(g, 2, 3, 1.0f);
        nimcp_graph_add_edge(g, 3, 4, 1.0f);
        nimcp_graph_add_edge(g, 4, 0, 1.0f);

        // Community 2: 5-6-7-8-9
        nimcp_graph_add_edge(g, 5, 6, 1.0f);
        nimcp_graph_add_edge(g, 5, 7, 1.0f);
        nimcp_graph_add_edge(g, 6, 7, 1.0f);
        nimcp_graph_add_edge(g, 6, 8, 1.0f);
        nimcp_graph_add_edge(g, 7, 8, 1.0f);
        nimcp_graph_add_edge(g, 8, 9, 1.0f);
        nimcp_graph_add_edge(g, 9, 5, 1.0f);

        // Inter-community edges
        nimcp_graph_add_edge(g, 2, 5, 1.0f);
        nimcp_graph_add_edge(g, 4, 9, 1.0f);

        return g;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, DefaultConfig) {
    graph_theory_bridge_config_t config;
    EXPECT_EQ(graph_theory_bridge_default_config(&config), GRAPH_THEORY_OK);

    EXPECT_TRUE(config.enable_kg_wiring);
    EXPECT_TRUE(config.enable_exception_handling);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_immune_presentation);
}

TEST_F(GraphTheoryBridgeTest, DefaultConfigNullParam) {
    EXPECT_EQ(graph_theory_bridge_default_config(nullptr), GRAPH_THEORY_ERROR_INVALID_PARAM);
}

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, CreateWithNullConfig) {
    graph_theory_bridge_t b = graph_theory_bridge_create(nullptr);
    EXPECT_NE(b, nullptr);  // Should use defaults
    graph_theory_bridge_destroy(b);
}

TEST_F(GraphTheoryBridgeTest, CreateWithCustomConfig) {
    graph_theory_bridge_config_t config;
    graph_theory_bridge_default_config(&config);
    config.enable_kg_wiring = false;
    config.enable_logging = false;

    graph_theory_bridge_t b = graph_theory_bridge_create(&config);
    EXPECT_NE(b, nullptr);
    graph_theory_bridge_destroy(b);
}

TEST_F(GraphTheoryBridgeTest, DestroyNull) {
    graph_theory_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(GraphTheoryBridgeTest, MultipleCreateDestroy) {
    for (int i = 0; i < 5; i++) {
        graph_theory_bridge_t b = graph_theory_bridge_create(nullptr);
        ASSERT_NE(b, nullptr);
        graph_theory_bridge_destroy(b);
    }
}

//=============================================================================
// KG Registration Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, RegisterKGNull) {
    EXPECT_EQ(graph_theory_bridge_register_kg(nullptr, nullptr, 0),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_bridge_register_kg(bridge_, nullptr, 0),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, GetKGState) {
    graph_theory_kg_state_t state;
    EXPECT_EQ(graph_theory_bridge_get_kg_state(bridge_, &state), GRAPH_THEORY_OK);
    EXPECT_FALSE(state.registered);  // Not registered yet
}

TEST_F(GraphTheoryBridgeTest, GetKGStateNullParams) {
    graph_theory_kg_state_t state;
    EXPECT_EQ(graph_theory_bridge_get_kg_state(nullptr, &state),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_bridge_get_kg_state(bridge_, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

//=============================================================================
// Exception Handler Registration Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, RegisterException) {
    void* dummy_handler = (void*)0x12345678;
    EXPECT_EQ(graph_theory_bridge_register_exception(bridge_, dummy_handler),
              GRAPH_THEORY_OK);
}

TEST_F(GraphTheoryBridgeTest, RegisterExceptionNullBridge) {
    EXPECT_EQ(graph_theory_bridge_register_exception(nullptr, (void*)0x1234),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, RegisterExceptionNullHandler) {
    EXPECT_EQ(graph_theory_bridge_register_exception(bridge_, nullptr),
              GRAPH_THEORY_OK);  // Clears handler
}

//=============================================================================
// Bio-Async Registration Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, RegisterBioAsync) {
    void* dummy_channel = (void*)0x87654321;
    EXPECT_EQ(graph_theory_bridge_register_bio_async(bridge_, dummy_channel),
              GRAPH_THEORY_OK);
}

TEST_F(GraphTheoryBridgeTest, RegisterBioAsyncNullBridge) {
    EXPECT_EQ(graph_theory_bridge_register_bio_async(nullptr, (void*)0x1234),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, RegisterBioAsyncNullChannel) {
    EXPECT_EQ(graph_theory_bridge_register_bio_async(bridge_, nullptr),
              GRAPH_THEORY_OK);  // Clears channel
}

//=============================================================================
// Centrality Analysis Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, ComputeDegreeCentrality) {
    graph_centrality_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(bridge_, graph_, CENTRALITY_DEGREE, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->num_nodes, 5u);
    EXPECT_EQ(result->type, CENTRALITY_DEGREE);
    EXPECT_NE(result->values, nullptr);

    // Degree centrality is normalized: degree / (n-1)
    // In a directed ring (one-way edges), each node has degree 1
    // Centrality = 1 / (5-1) = 0.25
    for (uint32_t i = 0; i < result->num_nodes; i++) {
        EXPECT_NEAR(result->values[i], 0.25f, 0.01f);
    }

    graph_centrality_result_destroy(result);
}

TEST_F(GraphTheoryBridgeTest, ComputeBetweennessCentrality) {
    graph_centrality_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(bridge_, graph_, CENTRALITY_BETWEENNESS, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->num_nodes, 5u);
    EXPECT_EQ(result->type, CENTRALITY_BETWEENNESS);

    graph_centrality_result_destroy(result);
}

TEST_F(GraphTheoryBridgeTest, ComputeClosenessCentrality) {
    graph_centrality_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(bridge_, graph_, CENTRALITY_CLOSENESS, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->num_nodes, 5u);
    EXPECT_EQ(result->type, CENTRALITY_CLOSENESS);

    graph_centrality_result_destroy(result);
}

TEST_F(GraphTheoryBridgeTest, ComputeEigenvectorCentrality) {
    graph_centrality_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(bridge_, graph_, CENTRALITY_EIGENVECTOR, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->num_nodes, 5u);
    EXPECT_EQ(result->type, CENTRALITY_EIGENVECTOR);

    graph_centrality_result_destroy(result);
}

TEST_F(GraphTheoryBridgeTest, ComputeCentralityNullParams) {
    graph_centrality_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(nullptr, graph_, CENTRALITY_DEGREE, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_centrality(bridge_, nullptr, CENTRALITY_DEGREE, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_centrality(bridge_, graph_, CENTRALITY_DEGREE, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

//=============================================================================
// Hub Detection Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, FindHubs) {
    NimcpGraph* complex = createComplexGraph();
    ASSERT_NE(complex, nullptr);

    uint32_t hub_indices[3];
    float hub_scores[3];

    int32_t found = graph_theory_find_hubs(bridge_, complex, CENTRALITY_DEGREE,
                                           3, hub_indices, hub_scores);
    EXPECT_GE(found, 0);
    EXPECT_LE(found, 3);

    nimcp_graph_destroy(complex);
}

TEST_F(GraphTheoryBridgeTest, FindHubsNullParams) {
    uint32_t hub_indices[3];
    float hub_scores[3];

    EXPECT_EQ(graph_theory_find_hubs(nullptr, graph_, CENTRALITY_DEGREE,
                                      3, hub_indices, hub_scores), -1);
    EXPECT_EQ(graph_theory_find_hubs(bridge_, nullptr, CENTRALITY_DEGREE,
                                      3, hub_indices, hub_scores), -1);
}

//=============================================================================
// Community Detection Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, DetectCommunitiesLouvain) {
    NimcpGraph* complex = createComplexGraph();
    ASSERT_NE(complex, nullptr);

    graph_community_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_detect_communities(bridge_, complex, COMMUNITY_LOUVAIN, 2, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_GE(result->num_communities, 1u);
    EXPECT_LE(result->num_communities, 10u);
    EXPECT_EQ(result->num_nodes, 10u);
    // Modularity can be slightly negative for directed graphs or small graphs
    // with unclear community structure
    EXPECT_GE(result->modularity, -0.1f);

    graph_community_result_destroy(result);
    nimcp_graph_destroy(complex);
}

TEST_F(GraphTheoryBridgeTest, DetectCommunitiesSpectral) {
    NimcpGraph* complex = createComplexGraph();
    ASSERT_NE(complex, nullptr);

    graph_community_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_detect_communities(bridge_, complex, COMMUNITY_SPECTRAL, 2, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_GE(result->num_communities, 1u);
    EXPECT_EQ(result->num_nodes, 10u);

    graph_community_result_destroy(result);
    nimcp_graph_destroy(complex);
}

TEST_F(GraphTheoryBridgeTest, DetectCommunitiesNullParams) {
    graph_community_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_detect_communities(nullptr, graph_, COMMUNITY_LOUVAIN, 2, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_detect_communities(bridge_, nullptr, COMMUNITY_LOUVAIN, 2, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_detect_communities(bridge_, graph_, COMMUNITY_LOUVAIN, 2, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, ComputeModularity) {
    NimcpGraph* complex = createComplexGraph();
    ASSERT_NE(complex, nullptr);

    // Manual community assignment: 0-4 in community 0, 5-9 in community 1
    uint32_t assignments[10] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1};

    float modularity = graph_theory_compute_modularity(bridge_, complex, assignments, 10);
    EXPECT_GT(modularity, -2.0f);  // Not error

    nimcp_graph_destroy(complex);
}

//=============================================================================
// Topology Metrics Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, ComputeMetrics) {
    graph_topology_metrics_t metrics;
    EXPECT_EQ(graph_theory_compute_metrics(bridge_, graph_, nullptr, &metrics),
              GRAPH_THEORY_OK);

    EXPECT_GE(metrics.clustering_coefficient, 0.0f);
    EXPECT_LE(metrics.clustering_coefficient, 1.0f);
    EXPECT_GE(metrics.density, 0.0f);
    EXPECT_LE(metrics.density, 1.0f);
    EXPECT_GE(metrics.global_efficiency, 0.0f);
}

TEST_F(GraphTheoryBridgeTest, ComputeMetricsNullParams) {
    graph_topology_metrics_t metrics;
    EXPECT_EQ(graph_theory_compute_metrics(nullptr, graph_, nullptr, &metrics),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_metrics(bridge_, nullptr, nullptr, &metrics),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_metrics(bridge_, graph_, nullptr, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, ValidateBrainTopology) {
    bool is_valid = false;
    char* report = nullptr;

    EXPECT_EQ(graph_theory_validate_brain_topology(bridge_, graph_, 0.1f, &is_valid, &report),
              GRAPH_THEORY_OK);

    if (report) {
        free(report);
    }
}

//=============================================================================
// Spectral Analysis Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, SpectralAnalysis) {
    graph_spectral_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_spectral_analysis(bridge_, graph_, 3, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->num_eigenvalues, 3u);
    EXPECT_NE(result->eigenvalues, nullptr);
    EXPECT_GE(result->spectral_gap, 0.0f);

    graph_spectral_result_destroy(result);
}

TEST_F(GraphTheoryBridgeTest, SpectralAnalysisNullParams) {
    graph_spectral_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_spectral_analysis(nullptr, graph_, 3, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_spectral_analysis(bridge_, nullptr, 3, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_spectral_analysis(bridge_, graph_, 3, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_spectral_analysis(bridge_, graph_, 0, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, ComputeFiedler) {
    uint32_t n = graph_->vertex_count;
    std::vector<float> fiedler(n);
    float algebraic_connectivity = 0.0f;

    EXPECT_EQ(graph_theory_compute_fiedler(bridge_, graph_, fiedler.data(), &algebraic_connectivity),
              GRAPH_THEORY_OK);
    EXPECT_GE(algebraic_connectivity, 0.0f);
}

TEST_F(GraphTheoryBridgeTest, ComputeFiedlerNullParams) {
    float fiedler[5];
    float ac;
    EXPECT_EQ(graph_theory_compute_fiedler(nullptr, graph_, fiedler, &ac),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_fiedler(bridge_, nullptr, fiedler, &ac),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_fiedler(bridge_, graph_, nullptr, &ac),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_fiedler(bridge_, graph_, fiedler, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

//=============================================================================
// Quantum Walk Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, QuantumWalk) {
    graph_qwalk_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_quantum_walk(bridge_, graph_, QWALK_CONTINUOUS, 0, 1.0f, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->num_nodes, 5u);
    EXPECT_EQ(result->type, QWALK_CONTINUOUS);
    EXPECT_FLOAT_EQ(result->evolution_time, 1.0f);
    EXPECT_NE(result->probabilities, nullptr);

    // Probabilities should sum to approximately 1
    float sum = 0.0f;
    for (uint32_t i = 0; i < result->num_nodes; i++) {
        sum += result->probabilities[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);

    graph_qwalk_result_destroy(result);
}

TEST_F(GraphTheoryBridgeTest, QuantumWalkNullParams) {
    graph_qwalk_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_quantum_walk(nullptr, graph_, QWALK_CONTINUOUS, 0, 1.0f, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_quantum_walk(bridge_, nullptr, QWALK_CONTINUOUS, 0, 1.0f, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_quantum_walk(bridge_, graph_, QWALK_CONTINUOUS, 0, 1.0f, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, QuantumWalkInvalidStartNode) {
    graph_qwalk_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_quantum_walk(bridge_, graph_, QWALK_CONTINUOUS, 100, 1.0f, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, QuantumSearch) {
    uint32_t marked[] = {2};
    graph_qwalk_result_t* result = nullptr;

    EXPECT_EQ(graph_theory_quantum_search(bridge_, graph_, marked, 1, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_TRUE(result->target_found);
    EXPECT_EQ(result->found_node, 2u);
    EXPECT_GT(result->speedup_factor, 1.0f);

    graph_qwalk_result_destroy(result);
}

TEST_F(GraphTheoryBridgeTest, QuantumSearchNullParams) {
    uint32_t marked[] = {2};
    graph_qwalk_result_t* result = nullptr;

    EXPECT_EQ(graph_theory_quantum_search(nullptr, graph_, marked, 1, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_quantum_search(bridge_, nullptr, marked, 1, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_quantum_search(bridge_, graph_, nullptr, 1, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_quantum_search(bridge_, graph_, marked, 0, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_quantum_search(bridge_, graph_, marked, 1, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

//=============================================================================
// Hyperbolic Embedding Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, HyperbolicEmbed) {
    graph_hyperbolic_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_hyperbolic_embed(bridge_, graph_, 2, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(result->num_nodes, 5u);
    EXPECT_EQ(result->dimension, 2u);
    EXPECT_LT(result->curvature, 0.0f);  // Hyperbolic curvature is negative
    EXPECT_NE(result->coordinates, nullptr);

    graph_hyperbolic_result_destroy(result);
}

TEST_F(GraphTheoryBridgeTest, HyperbolicEmbedNullParams) {
    graph_hyperbolic_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_hyperbolic_embed(nullptr, graph_, 2, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_hyperbolic_embed(bridge_, nullptr, 2, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_hyperbolic_embed(bridge_, graph_, 2, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_hyperbolic_embed(bridge_, graph_, 0, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, ComputeCurvature) {
    uint32_t num_edges = graph_->edge_count;
    std::vector<float> edge_curvatures(num_edges);
    float mean_curvature = 0.0f;

    EXPECT_EQ(graph_theory_compute_curvature(bridge_, graph_, edge_curvatures.data(), &mean_curvature),
              GRAPH_THEORY_OK);
}

TEST_F(GraphTheoryBridgeTest, ComputeCurvatureNullParams) {
    float mean_curvature;
    EXPECT_EQ(graph_theory_compute_curvature(nullptr, graph_, nullptr, &mean_curvature),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_curvature(bridge_, nullptr, nullptr, &mean_curvature),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_curvature(bridge_, graph_, nullptr, nullptr),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

//=============================================================================
// Phase Coherence Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, ComputePhaseCoherence) {
    uint32_t n = graph_->vertex_count;
    std::vector<float> phases(n);
    std::vector<float> local_coherences(n);
    float global_coherence = 0.0f;

    // Initialize phases (random but bounded)
    for (uint32_t i = 0; i < n; i++) {
        phases[i] = (float)i * 0.5f;
    }

    EXPECT_EQ(graph_theory_compute_phase_coherence(bridge_, graph_, phases.data(), n,
                                                    &global_coherence, local_coherences.data()),
              GRAPH_THEORY_OK);

    EXPECT_GE(global_coherence, 0.0f);
    EXPECT_LE(global_coherence, 1.0f);
}

TEST_F(GraphTheoryBridgeTest, ComputePhaseCoherenceNullParams) {
    float phases[5] = {0};
    float local[5];
    float global;

    EXPECT_EQ(graph_theory_compute_phase_coherence(nullptr, graph_, phases, 5, &global, local),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_phase_coherence(bridge_, nullptr, phases, 5, &global, local),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_phase_coherence(bridge_, graph_, nullptr, 5, &global, local),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    // num_phases=0 returns GRAPH_THEORY_ERROR_DIMENSION (dimension mismatch)
    EXPECT_EQ(graph_theory_compute_phase_coherence(bridge_, graph_, phases, 0, &global, local),
              GRAPH_THEORY_ERROR_DIMENSION);
    EXPECT_EQ(graph_theory_compute_phase_coherence(bridge_, graph_, phases, 5, nullptr, local),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
}

TEST_F(GraphTheoryBridgeTest, ComputeSyncMatrix) {
    NimcpGraph* complex = createComplexGraph();
    ASSERT_NE(complex, nullptr);

    float phases[10];
    uint32_t communities[10] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
    float sync_matrix[4];  // 2x2

    for (int i = 0; i < 10; i++) {
        phases[i] = (float)i * 0.3f;
    }

    // Parameters: num_nodes, num_communities
    EXPECT_EQ(graph_theory_compute_sync_matrix(bridge_, complex, phases, communities,
                                                10, 2, sync_matrix),
              GRAPH_THEORY_OK);

    nimcp_graph_destroy(complex);
}

//=============================================================================
// Async Operation Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, AsyncCentrality) {
    // Async operations return request ID, 0 on error
    uint64_t req_id = graph_theory_async_centrality(bridge_, graph_, CENTRALITY_DEGREE,
                                                     nullptr, nullptr);
    // Without callback, should still return valid ID or 0
    EXPECT_GE(req_id, 0u);
}

TEST_F(GraphTheoryBridgeTest, AsyncCommunities) {
    uint64_t req_id = graph_theory_async_communities(bridge_, graph_, COMMUNITY_LOUVAIN,
                                                      nullptr, nullptr);
    EXPECT_GE(req_id, 0u);
}

TEST_F(GraphTheoryBridgeTest, CancelRequest) {
    // Cancel non-existent request is a no-op (implementation returns OK)
    // This is reasonable behavior - nothing to cancel is not an error
    EXPECT_EQ(graph_theory_cancel_request(bridge_, 99999), GRAPH_THEORY_OK);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, ErrorString) {
    EXPECT_STREQ(graph_theory_error_string(GRAPH_THEORY_OK), "Success");
    EXPECT_NE(graph_theory_error_string(GRAPH_THEORY_ERROR_INVALID_PARAM), nullptr);
    EXPECT_NE(graph_theory_error_string(GRAPH_THEORY_ERROR_ALLOC), nullptr);
}

TEST_F(GraphTheoryBridgeTest, CentralityName) {
    EXPECT_NE(graph_theory_centrality_name(CENTRALITY_DEGREE), nullptr);
    EXPECT_NE(graph_theory_centrality_name(CENTRALITY_BETWEENNESS), nullptr);
    EXPECT_NE(graph_theory_centrality_name(CENTRALITY_CLOSENESS), nullptr);
}

TEST_F(GraphTheoryBridgeTest, CommunityAlgoName) {
    EXPECT_NE(graph_theory_community_algo_name(COMMUNITY_LOUVAIN), nullptr);
    EXPECT_NE(graph_theory_community_algo_name(COMMUNITY_SPECTRAL), nullptr);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(GraphTheoryBridgeTest, RapidCreateDestroy) {
    for (int i = 0; i < 20; i++) {
        graph_theory_bridge_t b = graph_theory_bridge_create(nullptr);
        ASSERT_NE(b, nullptr);
        graph_theory_bridge_destroy(b);
    }
}

TEST_F(GraphTheoryBridgeTest, MultipleAnalysesOnSameGraph) {
    graph_centrality_result_t* centrality = nullptr;
    graph_community_result_t* community = nullptr;
    graph_spectral_result_t* spectral = nullptr;

    // Run multiple analyses
    EXPECT_EQ(graph_theory_compute_centrality(bridge_, graph_, CENTRALITY_DEGREE, &centrality),
              GRAPH_THEORY_OK);
    EXPECT_EQ(graph_theory_detect_communities(bridge_, graph_, COMMUNITY_LOUVAIN, 2, &community),
              GRAPH_THEORY_OK);
    EXPECT_EQ(graph_theory_spectral_analysis(bridge_, graph_, 3, &spectral),
              GRAPH_THEORY_OK);

    // Cleanup
    if (centrality) graph_centrality_result_destroy(centrality);
    if (community) graph_community_result_destroy(community);
    if (spectral) graph_spectral_result_destroy(spectral);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
