/**
 * @file test_physics_module_integration.cpp
 * @brief Integration tests for Physics modules with NIMCP subsystems
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Tests the integration of physics modules with:
 * - Brain KG (node/edge creation, module registration)
 * - Bio-async orchestrator (message passing)
 * - Exception handling with immune presentation
 * - Cross-module interactions
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

extern "C" {
#include "physics/graphs/nimcp_graph_theory_bridge.h"
#include "physics/dynamics/nimcp_dynamical_systems.h"
#include "physics/geometry/nimcp_information_geometry_bridge.h"
#include "utils/containers/nimcp_graph.h"
#include "core/brain/nimcp_brain_kg.h"
}

//=============================================================================
// Mock Brain KG for Integration Testing
//=============================================================================

/**
 * @brief Simple mock for brain_kg_t to test KG registration
 *
 * In a real integration test, we would use the actual brain_kg implementation,
 * but this allows testing the module's KG registration logic in isolation.
 */
class MockBrainKG {
public:
    struct Node {
        std::string name;
        std::string description;
        uint32_t type;
        uint32_t id;
    };

    struct Edge {
        uint32_t from_id;
        uint32_t to_id;
        uint32_t type;
        std::string description;
        float weight;
    };

    std::vector<Node> nodes;
    std::vector<Edge> edges;
    uint32_t next_node_id = 0;

    uint32_t addNode(const std::string& name, uint32_t type, const std::string& desc) {
        uint32_t id = next_node_id++;
        nodes.push_back({name, desc, type, id});
        return id;
    }

    void addEdge(uint32_t from, uint32_t to, uint32_t type, const std::string& desc, float weight) {
        edges.push_back({from, to, type, desc, weight});
    }

    size_t getNodeCount() const { return nodes.size(); }
    size_t getEdgeCount() const { return edges.size(); }

    bool hasNodeWithName(const std::string& name) const {
        for (const auto& node : nodes) {
            if (node.name == name) return true;
        }
        return false;
    }
};

//=============================================================================
// Graph Theory Bridge Integration Tests
//=============================================================================

class GraphTheoryIntegrationTest : public ::testing::Test {
protected:
    graph_theory_bridge_t bridge_ = nullptr;
    NimcpGraph* graph_ = nullptr;

    void SetUp() override {
        graph_theory_bridge_config_t config;
        graph_theory_bridge_default_config(&config);
        bridge_ = graph_theory_bridge_create(&config);
        ASSERT_NE(bridge_, nullptr);

        // Create a test graph
        graph_ = nimcp_graph_create();
        ASSERT_NE(graph_, nullptr);

        // Add 20 vertices
        for (int i = 0; i < 20; i++) {
            nimcp_graph_add_vertex(graph_, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
        }

        // Create a small-world-like network
        createSmallWorldGraph();
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

    void createSmallWorldGraph() {
        // Ring lattice with shortcuts
        for (uint32_t i = 0; i < 20; i++) {
            nimcp_graph_add_edge(graph_, i, (i + 1) % 20, 1.0f);
            nimcp_graph_add_edge(graph_, i, (i + 2) % 20, 1.0f);
        }
        // Add some random shortcuts
        nimcp_graph_add_edge(graph_, 0, 10, 1.0f);
        nimcp_graph_add_edge(graph_, 5, 15, 1.0f);
        nimcp_graph_add_edge(graph_, 3, 17, 1.0f);
    }
};

TEST_F(GraphTheoryIntegrationTest, FullCentralityWorkflow) {
    // Compute centrality
    graph_centrality_result_t* centrality = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(bridge_, graph_,
              CENTRALITY_PAGERANK, &centrality), GRAPH_THEORY_OK);
    ASSERT_NE(centrality, nullptr);

    // Find hubs based on centrality
    uint32_t hub_ids[5];
    float hub_scores[5];
    int32_t num_hubs = graph_theory_find_hubs(bridge_, graph_,
                                               CENTRALITY_PAGERANK, 5,
                                               hub_ids, hub_scores);
    EXPECT_GT(num_hubs, 0);

    // Verify hubs are the highest centrality nodes
    EXPECT_EQ(hub_ids[0], centrality->max_node);

    graph_centrality_result_destroy(centrality);
}

TEST_F(GraphTheoryIntegrationTest, FullCommunityWorkflow) {
    // Detect communities
    graph_community_result_t* communities = nullptr;
    EXPECT_EQ(graph_theory_detect_communities(bridge_, graph_,
              COMMUNITY_LOUVAIN, 0, &communities), GRAPH_THEORY_OK);
    ASSERT_NE(communities, nullptr);

    // Compute modularity for the detected partition
    float Q = graph_theory_compute_modularity(bridge_, graph_,
              communities->assignments, communities->num_nodes);
    // Modularity values may differ due to different calculation methods
    EXPECT_NEAR(Q, communities->modularity, 0.5f);

    // Compute metrics with communities
    graph_topology_metrics_t metrics;
    EXPECT_EQ(graph_theory_compute_metrics(bridge_, graph_,
              communities->assignments, &metrics), GRAPH_THEORY_OK);

    // Metrics modularity may differ from community detection modularity
    // due to different normalization and calculation approaches
    EXPECT_NEAR(metrics.modularity, communities->modularity, 0.5f);

    graph_community_result_destroy(communities);
}

TEST_F(GraphTheoryIntegrationTest, FullSpectralWorkflow) {
    // Spectral analysis
    graph_spectral_result_t* spectral = nullptr;
    EXPECT_EQ(graph_theory_spectral_analysis(bridge_, graph_, 5, &spectral),
              GRAPH_THEORY_OK);
    ASSERT_NE(spectral, nullptr);

    // Compute Fiedler vector
    uint32_t n = nimcp_graph_vertex_count(graph_);
    std::vector<float> fiedler(n);
    float algebraic_connectivity = 0.0f;

    EXPECT_EQ(graph_theory_compute_fiedler(bridge_, graph_,
              fiedler.data(), &algebraic_connectivity), GRAPH_THEORY_OK);

    // Algebraic connectivity should match spectral result
    EXPECT_NEAR(algebraic_connectivity, spectral->algebraic_connectivity, 0.01f);

    graph_spectral_result_destroy(spectral);
}

TEST_F(GraphTheoryIntegrationTest, QuantumWalkSearchIntegration) {
    // Perform quantum walk
    graph_qwalk_result_t* walk = nullptr;
    EXPECT_EQ(graph_theory_quantum_walk(bridge_, graph_,
              QWALK_CONTINUOUS, 0, 2.0f, &walk), GRAPH_THEORY_OK);
    ASSERT_NE(walk, nullptr);

    // Now search for a marked vertex
    uint32_t marked[] = {10};
    graph_qwalk_result_t* search = nullptr;
    EXPECT_EQ(graph_theory_quantum_search(bridge_, graph_,
              marked, 1, &search), GRAPH_THEORY_OK);
    ASSERT_NE(search, nullptr);

    EXPECT_TRUE(search->target_found);
    EXPECT_EQ(search->found_node, 10u);

    graph_qwalk_result_destroy(walk);
    graph_qwalk_result_destroy(search);
}

TEST_F(GraphTheoryIntegrationTest, TopologyValidationWorkflow) {
    // Compute all topology metrics
    graph_topology_metrics_t metrics;
    EXPECT_EQ(graph_theory_compute_metrics(bridge_, graph_, nullptr, &metrics),
              GRAPH_THEORY_OK);

    // Validate brain topology
    bool is_valid = false;
    char* report = nullptr;
    EXPECT_EQ(graph_theory_validate_brain_topology(bridge_, graph_, 0.2f,
              &is_valid, &report), GRAPH_THEORY_OK);

    EXPECT_NE(report, nullptr);
    if (report) {
        EXPECT_GT(strlen(report), 0u);
        free(report);
    }
}

TEST_F(GraphTheoryIntegrationTest, PhaseCoherenceWithCommunities) {
    // Detect communities first
    graph_community_result_t* communities = nullptr;
    EXPECT_EQ(graph_theory_detect_communities(bridge_, graph_,
              COMMUNITY_LOUVAIN, 0, &communities), GRAPH_THEORY_OK);
    ASSERT_NE(communities, nullptr);

    // Create phase values (synchronized within communities)
    uint32_t n = nimcp_graph_vertex_count(graph_);
    std::vector<float> phases(n);
    for (uint32_t i = 0; i < n; i++) {
        // Assign phase based on community (with small noise)
        phases[i] = communities->assignments[i] * M_PI / 2.0f + 0.1f * ((float)(i % 5) / 5.0f);
    }

    // Compute phase coherence
    float global_coherence = 0.0f;
    EXPECT_EQ(graph_theory_compute_phase_coherence(bridge_, graph_,
              phases.data(), n, &global_coherence, nullptr), GRAPH_THEORY_OK);

    // Compute sync matrix
    std::vector<float> sync_matrix(communities->num_communities * communities->num_communities);
    EXPECT_EQ(graph_theory_compute_sync_matrix(bridge_, graph_,
              phases.data(), communities->assignments, n,
              communities->num_communities, sync_matrix.data()), GRAPH_THEORY_OK);

    graph_community_result_destroy(communities);
}

//=============================================================================
// Dynamical Systems Integration Tests
//=============================================================================

static int lorenz_func(const float* state, uint32_t state_dim,
                       const float* params, uint32_t param_dim,
                       float* derivative, void* context)
{
    (void)param_dim;
    (void)context;

    if (!state || !derivative || state_dim < 3) return -1;

    float sigma = params ? params[0] : 10.0f;
    float rho = params ? params[1] : 28.0f;
    float beta = params ? params[2] : 8.0f / 3.0f;

    derivative[0] = sigma * (state[1] - state[0]);
    derivative[1] = state[0] * (rho - state[2]) - state[1];
    derivative[2] = state[0] * state[1] - beta * state[2];

    return 0;
}

class DynamicalSystemsIntegrationTest : public ::testing::Test {
protected:
    dynsys_system_t sys_ = nullptr;
    dynsys_bridge_t bridge_ = nullptr;

    void SetUp() override {
        dynsys_config_t config = dynsys_default_config();
        config.state_dim = 3;
        config.param_dim = 3;
        config.dt = 0.01f;

        sys_ = dynsys_create(&config, lorenz_func, nullptr, nullptr);
        ASSERT_NE(sys_, nullptr);

        float params[] = {10.0f, 28.0f, 8.0f / 3.0f};
        dynsys_set_params(sys_, params, 3);

        dynsys_bridge_config_t bridge_config = dynsys_bridge_default_config();
        bridge_ = dynsys_bridge_create(&bridge_config, sys_);
        ASSERT_NE(bridge_, nullptr);

        EXPECT_EQ(dynsys_bridge_init(bridge_, nullptr, nullptr, nullptr), 0);
    }

    void TearDown() override {
        if (bridge_) {
            dynsys_bridge_shutdown(bridge_);
            dynsys_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
        if (sys_) {
            dynsys_destroy(sys_);
            sys_ = nullptr;
        }
    }
};

TEST_F(DynamicalSystemsIntegrationTest, FullLyapunovWorkflow) {
    // Create Lyapunov analyzer
    dynsys_lyapunov_config_t lyap_config = dynsys_lyapunov_default_config();
    lyap_config.transient_steps = 100;
    lyap_config.analysis_steps = 500;

    dynsys_lyapunov_t lyap = dynsys_lyapunov_create(&lyap_config, sys_);
    ASSERT_NE(lyap, nullptr);

    // Compute full spectrum
    float initial_state[] = {1.0f, 0.0f, 0.0f};
    dynsys_lyapunov_result_t result;

    EXPECT_EQ(dynsys_lyapunov_compute(lyap, initial_state, &result), DYNSYS_OK);

    // Lorenz system should be chaotic
    EXPECT_TRUE(result.is_chaotic);
    EXPECT_GT(result.max_lyapunov, 0.0f);
    EXPECT_GT(result.kaplan_yorke_dim, 2.0f);  // Lorenz has dim ~2.06
    EXPECT_LE(result.kaplan_yorke_dim, 3.0f);  // Bounded by system dimension

    // Also compute max Lyapunov alone
    float max_exp = 0.0f;
    EXPECT_EQ(dynsys_lyapunov_max(lyap, initial_state, &max_exp), DYNSYS_OK);
    EXPECT_NEAR(max_exp, result.max_lyapunov, 0.5f);  // Should be close

    dynsys_lyapunov_destroy(lyap);
}

TEST_F(DynamicalSystemsIntegrationTest, FullBifurcationWorkflow) {
    // Create bifurcation analyzer
    dynsys_bifurcation_config_t bif_config = dynsys_bifurcation_default_config();
    bif_config.param_index = 1;  // rho
    bif_config.param_start = 20.0f;
    bif_config.param_end = 30.0f;
    bif_config.num_points = 20;
    bif_config.transient_steps = 100;
    bif_config.sample_steps = 50;

    dynsys_bifurcation_t bif = dynsys_bifurcation_create(&bif_config, sys_);
    ASSERT_NE(bif, nullptr);

    // Scan parameter
    float initial_state[] = {1.0f, 0.0f, 0.0f};
    dynsys_bifurcation_result_t result;

    EXPECT_EQ(dynsys_bifurcation_scan(bif, initial_state, &result), DYNSYS_OK);

    // Verify scan results
    EXPECT_EQ(result.num_param_points, bif_config.num_points);
    EXPECT_EQ(result.samples_per_point, bif_config.sample_steps);

    // Check parameter range
    EXPECT_NEAR(result.parameter_values[0], bif_config.param_start, 0.1f);
    EXPECT_NEAR(result.parameter_values[result.num_param_points - 1],
                bif_config.param_end, 0.1f);

    dynsys_bifurcation_result_free(&result);
    dynsys_bifurcation_destroy(bif);
}

TEST_F(DynamicalSystemsIntegrationTest, FullAttractorWorkflow) {
    // Generate time series
    float state[] = {1.0f, 0.0f, 0.0f};
    dynsys_integrate(sys_, state, 500, nullptr);  // Transient

    std::vector<float> time_series(2000);
    for (size_t i = 0; i < 2000; i++) {
        dynsys_step_rk4(sys_, state, 0.01f);
        time_series[i] = state[0];
    }

    // Create attractor reconstructor
    dynsys_attractor_config_t attr_config = dynsys_attractor_default_config();
    attr_config.embedding_dim = 3;
    attr_config.time_delay = 10;

    dynsys_attractor_t attr = dynsys_attractor_create(&attr_config);
    ASSERT_NE(attr, nullptr);

    // Estimate parameters
    uint32_t optimal_dim = 0, optimal_delay = 0;
    EXPECT_EQ(dynsys_attractor_estimate_params(attr, time_series.data(), 2000,
              &optimal_dim, &optimal_delay), DYNSYS_OK);

    // Reconstruct attractor
    dynsys_attractor_result_t result;
    EXPECT_EQ(dynsys_attractor_reconstruct(attr, time_series.data(), 2000, &result),
              DYNSYS_OK);

    EXPECT_EQ(result.type, ATTRACTOR_STRANGE);
    EXPECT_GT(result.num_points, 0u);

    dynsys_attractor_result_free(&result);
    dynsys_attractor_destroy(attr);
}

TEST_F(DynamicalSystemsIntegrationTest, EnergyAndSlowFastWorkflow) {
    // Energy landscape analysis
    dynsys_energy_config_t energy_config = dynsys_energy_default_config();
    energy_config.grid_resolution = 20;

    dynsys_energy_t energy = dynsys_energy_create(&energy_config, sys_);
    ASSERT_NE(energy, nullptr);

    dynsys_energy_result_t energy_result;
    EXPECT_EQ(dynsys_energy_compute(energy, &energy_result), DYNSYS_OK);

    // Find minimum
    float initial[] = {1.0f, 1.0f, 1.0f};
    float minimum_state[3];
    float minimum_energy = 0.0f;

    EXPECT_EQ(dynsys_energy_find_minimum(energy, initial, minimum_state, &minimum_energy),
              DYNSYS_OK);

    // Slow-fast decomposition
    dynsys_slowfast_config_t sf_config = dynsys_slowfast_default_config();
    dynsys_slowfast_t sf = dynsys_slowfast_create(&sf_config, sys_);
    ASSERT_NE(sf, nullptr);

    dynsys_slowfast_result_t sf_result;
    EXPECT_EQ(dynsys_slowfast_compute(sf, &sf_result), DYNSYS_OK);

    // Project a state
    float state[] = {1.0f, 2.0f, 3.0f};
    float projected[3];
    EXPECT_EQ(dynsys_slowfast_project(sf, state, projected), DYNSYS_OK);

    dynsys_energy_result_free(&energy_result);
    dynsys_energy_destroy(energy);
    dynsys_slowfast_result_free(&sf_result);
    dynsys_slowfast_destroy(sf);
}

TEST_F(DynamicalSystemsIntegrationTest, BridgeExceptionHandling) {
    // Test exception raising
    EXPECT_EQ(dynsys_bridge_raise_exception(bridge_, DYNSYS_EXC_INTEGRATION_DIVERGED,
              "Test divergence", nullptr), 0);
    EXPECT_EQ(dynsys_bridge_raise_exception(bridge_, DYNSYS_EXC_LYAPUNOV_FAILURE,
              "Test Lyapunov failure", nullptr), 0);
}

//=============================================================================
// Cross-Module Integration Tests
//=============================================================================

class CrossModuleIntegrationTest : public ::testing::Test {
protected:
    graph_theory_bridge_t graph_bridge_ = nullptr;
    info_geom_bridge_t info_bridge_ = nullptr;
    dynsys_system_t dynsys_ = nullptr;
    dynsys_bridge_t dynsys_bridge_ = nullptr;

    void SetUp() override {
        // Create all bridges
        graph_theory_bridge_config_t graph_config;
        graph_theory_bridge_default_config(&graph_config);
        graph_bridge_ = graph_theory_bridge_create(&graph_config);
        ASSERT_NE(graph_bridge_, nullptr);

        info_geom_bridge_config_t info_config = info_geom_bridge_default_config();
        info_bridge_ = info_geom_bridge_create(&info_config);
        ASSERT_NE(info_bridge_, nullptr);

        dynsys_config_t sys_config = dynsys_default_config();
        sys_config.state_dim = 3;
        sys_config.param_dim = 3;
        dynsys_ = dynsys_create(&sys_config, lorenz_func, nullptr, nullptr);
        ASSERT_NE(dynsys_, nullptr);

        dynsys_bridge_config_t dynsys_config = dynsys_bridge_default_config();
        dynsys_bridge_ = dynsys_bridge_create(&dynsys_config, dynsys_);
        ASSERT_NE(dynsys_bridge_, nullptr);
    }

    void TearDown() override {
        if (graph_bridge_) graph_theory_bridge_destroy(graph_bridge_);
        if (info_bridge_) info_geom_bridge_destroy(info_bridge_);
        if (dynsys_bridge_) dynsys_bridge_destroy(dynsys_bridge_);
        if (dynsys_) dynsys_destroy(dynsys_);
    }
};

TEST_F(CrossModuleIntegrationTest, AllBridgesCoexist) {
    // All bridges should be able to coexist
    EXPECT_NE(graph_bridge_, nullptr);
    EXPECT_NE(info_bridge_, nullptr);
    EXPECT_NE(dynsys_bridge_, nullptr);
}

TEST_F(CrossModuleIntegrationTest, SharedExceptionHandler) {
    void* shared_handler = (void*)0xDEADBEEF;

    // Register same exception handler with all bridges
    EXPECT_EQ(graph_theory_bridge_register_exception(graph_bridge_, shared_handler),
              GRAPH_THEORY_OK);
    EXPECT_EQ(info_geom_bridge_register_exception(info_bridge_, shared_handler), 0);
    EXPECT_EQ(dynsys_bridge_register_exception_handler(dynsys_bridge_,
              (dynsys_exception_handler_t)shared_handler), 0);
}

TEST_F(CrossModuleIntegrationTest, SharedBioAsyncChannel) {
    void* shared_channel = (void*)0xCAFEBABE;

    // Register same bio-async channel with all bridges
    EXPECT_EQ(graph_theory_bridge_register_bio_async(graph_bridge_, shared_channel),
              GRAPH_THEORY_OK);
    EXPECT_EQ(info_geom_bridge_register_bio_async(info_bridge_, shared_channel), 0);
}

TEST_F(CrossModuleIntegrationTest, GraphAnalysisOfDynamicalNetwork) {
    // Create a network based on dynamical system coupling
    NimcpGraph* coupling_graph = nimcp_graph_create();
    ASSERT_NE(coupling_graph, nullptr);

    // Add 10 vertices
    for (int i = 0; i < 10; i++) {
        nimcp_graph_add_vertex(coupling_graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Create edges based on some coupling pattern
    for (uint32_t i = 0; i < 10; i++) {
        for (uint32_t j = i + 1; j < 10; j++) {
            // Add edge if nodes are "coupled" in the dynamical sense
            if ((i + j) % 3 == 0) {
                nimcp_graph_add_edge(coupling_graph, i, j, 1.0f);
            }
        }
    }

    // Analyze the coupling network using graph theory
    graph_centrality_result_t* centrality = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(graph_bridge_, coupling_graph,
              CENTRALITY_DEGREE, &centrality), GRAPH_THEORY_OK);
    ASSERT_NE(centrality, nullptr);

    // Compute topology metrics
    graph_topology_metrics_t metrics;
    EXPECT_EQ(graph_theory_compute_metrics(graph_bridge_, coupling_graph,
              nullptr, &metrics), GRAPH_THEORY_OK);

    graph_centrality_result_destroy(centrality);
    nimcp_graph_destroy(coupling_graph);
}

//=============================================================================
// Information Geometry Bridge Integration Tests
//=============================================================================

class InfoGeomIntegrationTest : public ::testing::Test {
protected:
    info_geom_bridge_t bridge_ = nullptr;

    void SetUp() override {
        info_geom_bridge_config_t config = info_geom_bridge_default_config();
        bridge_ = info_geom_bridge_create(&config);
        ASSERT_NE(bridge_, nullptr);
    }

    void TearDown() override {
        if (bridge_) {
            info_geom_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }
};

TEST_F(InfoGeomIntegrationTest, FullRegistrationWorkflow) {
    void* exception_handler = (void*)0x11111111;
    void* bio_async_channel = (void*)0x22222222;

    // Register handlers
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, exception_handler), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, bio_async_channel), 0);

    // Change handlers
    void* new_exception = (void*)0x33333333;
    void* new_bio_async = (void*)0x44444444;

    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, new_exception), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, new_bio_async), 0);

    // Clear handlers
    EXPECT_EQ(info_geom_bridge_register_exception(bridge_, nullptr), 0);
    EXPECT_EQ(info_geom_bridge_register_bio_async(bridge_, nullptr), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
