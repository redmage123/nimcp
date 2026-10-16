/**
 * @file test_physics_modules_regression.cpp
 * @brief Regression tests for Physics modules
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Regression tests to verify:
 * - API stability (function signatures, return values)
 * - Configuration defaults remain consistent
 * - Memory safety (no leaks, double-frees)
 * - Error handling consistency
 * - Backward compatibility with established behavior
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "physics/graphs/nimcp_graph_theory_bridge.h"
#include "physics/dynamics/nimcp_dynamical_systems.h"
#include "physics/geometry/nimcp_information_geometry_bridge.h"
#include "utils/containers/nimcp_graph.h"
}

//=============================================================================
// API Stability Regression Tests
//=============================================================================

/**
 * @test Verify graph theory error codes remain stable
 */
TEST(GraphTheoryAPIRegression, ErrorCodeValues) {
    // Error codes must remain stable for ABI compatibility
    EXPECT_EQ(GRAPH_THEORY_OK, 0);
    EXPECT_EQ(GRAPH_THEORY_ERROR_INVALID_PARAM, -1);
    EXPECT_EQ(GRAPH_THEORY_ERROR_ALLOC, -2);
    EXPECT_EQ(GRAPH_THEORY_ERROR_NOT_INIT, -3);
    EXPECT_EQ(GRAPH_THEORY_ERROR_ALREADY_INIT, -4);
    EXPECT_EQ(GRAPH_THEORY_ERROR_KG_WIRING, -5);
    EXPECT_EQ(GRAPH_THEORY_ERROR_EXCEPTION, -6);
    EXPECT_EQ(GRAPH_THEORY_ERROR_BIO_ASYNC, -7);
    EXPECT_EQ(GRAPH_THEORY_ERROR_GRAPH_INVALID, -8);
    EXPECT_EQ(GRAPH_THEORY_ERROR_COMPUTATION, -9);
    EXPECT_EQ(GRAPH_THEORY_ERROR_CONVERGENCE, -10);
    EXPECT_EQ(GRAPH_THEORY_ERROR_DIMENSION, -11);
    EXPECT_EQ(GRAPH_THEORY_ERROR_DISCONNECTED, -12);
    EXPECT_EQ(GRAPH_THEORY_ERROR_TIMEOUT, -13);
}

/**
 * @test Verify dynamical systems error codes remain stable
 */
TEST(DynamicalSystemsAPIRegression, ErrorCodeValues) {
    EXPECT_EQ(DYNSYS_OK, 0);
    EXPECT_EQ(DYNSYS_ERR_NULL_PTR, -1);
    EXPECT_EQ(DYNSYS_ERR_INVALID_DIM, -2);
    EXPECT_EQ(DYNSYS_ERR_DIVERGENCE, -3);
    EXPECT_EQ(DYNSYS_ERR_NOT_INITIALIZED, -4);
    EXPECT_EQ(DYNSYS_ERR_ALREADY_INITIALIZED, -5);
    EXPECT_EQ(DYNSYS_ERR_NO_MEMORY, -6);
    EXPECT_EQ(DYNSYS_ERR_COMPUTATION, -7);
    EXPECT_EQ(DYNSYS_ERR_INVALID_PARAMETER, -8);
}

/**
 * @test Verify centrality type enums remain stable
 */
TEST(GraphTheoryAPIRegression, CentralityTypeValues) {
    EXPECT_EQ(CENTRALITY_DEGREE, 0);
    EXPECT_EQ(CENTRALITY_BETWEENNESS, 1);
    EXPECT_EQ(CENTRALITY_CLOSENESS, 2);
    EXPECT_EQ(CENTRALITY_EIGENVECTOR, 3);
    EXPECT_EQ(CENTRALITY_PAGERANK, 4);
    EXPECT_EQ(CENTRALITY_KATZ, 5);
}

/**
 * @test Verify community algorithm enums remain stable
 */
TEST(GraphTheoryAPIRegression, CommunityAlgoValues) {
    EXPECT_EQ(COMMUNITY_LOUVAIN, 0);
    EXPECT_EQ(COMMUNITY_SPECTRAL, 1);
    EXPECT_EQ(COMMUNITY_LABEL_PROP, 2);
    EXPECT_EQ(COMMUNITY_GIRVAN_NEWMAN, 3);
}

/**
 * @test Verify quantum walk type enums remain stable
 */
TEST(GraphTheoryAPIRegression, QWalkTypeValues) {
    EXPECT_EQ(QWALK_CONTINUOUS, 0);
    EXPECT_EQ(QWALK_DISCRETE, 1);
    EXPECT_EQ(QWALK_GROVER, 2);
}

/**
 * @test Verify bifurcation type enums remain stable
 */
TEST(DynamicalSystemsAPIRegression, BifurcationTypeValues) {
    EXPECT_EQ(BIFURCATION_NONE, 0);
    EXPECT_EQ(BIFURCATION_SADDLE_NODE, 1);
    EXPECT_EQ(BIFURCATION_TRANSCRITICAL, 2);
    EXPECT_EQ(BIFURCATION_PITCHFORK, 3);
    EXPECT_EQ(BIFURCATION_HOPF, 4);
    EXPECT_EQ(BIFURCATION_PERIOD_DOUBLING, 5);
    EXPECT_EQ(BIFURCATION_NEIMARK_SACKER, 6);
    EXPECT_EQ(BIFURCATION_HOMOCLINIC, 7);
    EXPECT_EQ(BIFURCATION_HETEROCLINIC, 8);
}

/**
 * @test Verify attractor type enums remain stable
 */
TEST(DynamicalSystemsAPIRegression, AttractorTypeValues) {
    EXPECT_EQ(ATTRACTOR_UNKNOWN, 0);
    EXPECT_EQ(ATTRACTOR_FIXED_POINT, 1);
    EXPECT_EQ(ATTRACTOR_LIMIT_CYCLE, 2);
    EXPECT_EQ(ATTRACTOR_TORUS, 3);
    EXPECT_EQ(ATTRACTOR_STRANGE, 4);
    EXPECT_EQ(ATTRACTOR_CHAOTIC_SADDLE, 5);
}

//=============================================================================
// Module Constants Regression Tests
//=============================================================================

TEST(GraphTheoryAPIRegression, ModuleConstants) {
    EXPECT_STREQ(GRAPH_THEORY_BRIDGE_MODULE_NAME, "graph_theory_bridge");
    EXPECT_EQ(GRAPH_THEORY_KG_MODULE_ID, 0x0470);
    EXPECT_EQ(GRAPH_THEORY_EXCEPTION_CATEGORY, 0x0047);
    EXPECT_EQ(GRAPH_THEORY_MAX_SPECTRAL_DIM, 1024);
    EXPECT_EQ(GRAPH_THEORY_MAX_NODES, 100000);
    EXPECT_FLOAT_EQ(GRAPH_THEORY_DEFAULT_TOLERANCE, 1e-6f);
}

TEST(DynamicalSystemsAPIRegression, ModuleConstants) {
    EXPECT_EQ(DYNSYS_MAX_STATE_DIM, 128);
    EXPECT_EQ(DYNSYS_MAX_EMBEDDING_DIM, 32);
    EXPECT_EQ(DYNSYS_MAX_LYAPUNOV, 16);
    EXPECT_EQ(DYNSYS_MAX_BIFURCATION_POINTS, 1024);
    EXPECT_FLOAT_EQ(DYNSYS_DEFAULT_DT, 0.001f);
    EXPECT_STREQ(DYNSYS_MODULE_NAME, "dynamical_systems");
    EXPECT_EQ(DYNSYS_KG_MODULE_ID, 0x0460);
}

TEST(InfoGeomAPIRegression, ModuleConstants) {
    EXPECT_STREQ(INFO_GEOM_BRIDGE_MODULE_NAME, "info_geometry_bridge");
    EXPECT_EQ(INFO_GEOM_BRIDGE_KG_MODULE_ID, 0x0450);
}

//=============================================================================
// Configuration Defaults Regression Tests
//=============================================================================

TEST(GraphTheoryConfigRegression, DefaultConfigValues) {
    graph_theory_bridge_config_t config;
    graph_theory_bridge_default_config(&config);

    // These defaults should remain stable
    EXPECT_TRUE(config.enable_kg_wiring);
    EXPECT_TRUE(config.enable_exception_handling);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_immune_presentation);
    EXPECT_TRUE(config.enable_logging);
    EXPECT_EQ(config.default_centrality, CENTRALITY_PAGERANK);
    EXPECT_EQ(config.default_community_algo, COMMUNITY_LOUVAIN);
    EXPECT_FLOAT_EQ(config.pagerank_damping, 0.85f);
    EXPECT_FLOAT_EQ(config.tolerance, GRAPH_THEORY_DEFAULT_TOLERANCE);
}

TEST(DynamicalSystemsConfigRegression, DefaultConfigValues) {
    dynsys_config_t config = dynsys_default_config();

    EXPECT_EQ(config.state_dim, 3u);
    EXPECT_EQ(config.param_dim, 1u);
    EXPECT_FLOAT_EQ(config.dt, DYNSYS_DEFAULT_DT);
    EXPECT_TRUE(config.enable_logging);
    EXPECT_TRUE(config.enable_metrics);
    EXPECT_TRUE(config.enable_kg_wiring);
    EXPECT_TRUE(config.enable_exception_handling);
}

TEST(DynamicalSystemsConfigRegression, LyapunovDefaultValues) {
    dynsys_lyapunov_config_t config = dynsys_lyapunov_default_config();

    EXPECT_EQ(config.num_exponents, 3u);
    EXPECT_FLOAT_EQ(config.orthonormalization_interval, 1.0f);
    EXPECT_EQ(config.transient_steps, 1000u);
    EXPECT_EQ(config.analysis_steps, 10000u);
    EXPECT_FLOAT_EQ(config.perturbation_size, 1e-6f);
}

TEST(DynamicalSystemsConfigRegression, BifurcationDefaultValues) {
    dynsys_bifurcation_config_t config = dynsys_bifurcation_default_config();

    EXPECT_EQ(config.param_index, 0u);
    EXPECT_FLOAT_EQ(config.param_start, 0.0f);
    EXPECT_FLOAT_EQ(config.param_end, 4.0f);
    EXPECT_EQ(config.num_points, 200u);
    EXPECT_EQ(config.transient_steps, 500u);
    EXPECT_EQ(config.sample_steps, 100u);
    EXPECT_FLOAT_EQ(config.tolerance, 1e-4f);
}

TEST(DynamicalSystemsConfigRegression, AttractorDefaultValues) {
    dynsys_attractor_config_t config = dynsys_attractor_default_config();

    EXPECT_EQ(config.embedding_dim, 3u);
    EXPECT_EQ(config.time_delay, 10u);
    EXPECT_EQ(config.observable_index, 0u);
    EXPECT_EQ(config.num_samples, 10000u);
    EXPECT_TRUE(config.estimate_dimension);
    EXPECT_FLOAT_EQ(config.epsilon_min, 0.01f);
    EXPECT_FLOAT_EQ(config.epsilon_max, 1.0f);
}

TEST(InfoGeomConfigRegression, DefaultConfigValues) {
    info_geom_bridge_config_t config = info_geom_bridge_default_config();

    EXPECT_TRUE(config.enable_kg_wiring);
    EXPECT_TRUE(config.enable_exception_handling);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.enable_immune_presentation);
    EXPECT_TRUE(config.enable_logging);
}

//=============================================================================
// Error String Regression Tests
//=============================================================================

TEST(GraphTheoryAPIRegression, ErrorStrings) {
    EXPECT_STREQ(graph_theory_error_string(GRAPH_THEORY_OK), "Success");
    EXPECT_STREQ(graph_theory_error_string(GRAPH_THEORY_ERROR_INVALID_PARAM), "Invalid parameter");
    EXPECT_STREQ(graph_theory_error_string(GRAPH_THEORY_ERROR_ALLOC), "Memory allocation failed");
    EXPECT_STREQ(graph_theory_error_string(GRAPH_THEORY_ERROR_NOT_INIT), "Bridge not initialized");
    EXPECT_STREQ(graph_theory_error_string(GRAPH_THEORY_ERROR_COMPUTATION), "Computation error");
}

TEST(GraphTheoryAPIRegression, CentralityNames) {
    EXPECT_STREQ(graph_theory_centrality_name(CENTRALITY_DEGREE), "degree");
    EXPECT_STREQ(graph_theory_centrality_name(CENTRALITY_BETWEENNESS), "betweenness");
    EXPECT_STREQ(graph_theory_centrality_name(CENTRALITY_CLOSENESS), "closeness");
    EXPECT_STREQ(graph_theory_centrality_name(CENTRALITY_EIGENVECTOR), "eigenvector");
    EXPECT_STREQ(graph_theory_centrality_name(CENTRALITY_PAGERANK), "PageRank");
    EXPECT_STREQ(graph_theory_centrality_name(CENTRALITY_KATZ), "Katz");
}

TEST(GraphTheoryAPIRegression, CommunityAlgoNames) {
    EXPECT_STREQ(graph_theory_community_algo_name(COMMUNITY_LOUVAIN), "Louvain");
    EXPECT_STREQ(graph_theory_community_algo_name(COMMUNITY_SPECTRAL), "spectral");
    EXPECT_STREQ(graph_theory_community_algo_name(COMMUNITY_LABEL_PROP), "label propagation");
    EXPECT_STREQ(graph_theory_community_algo_name(COMMUNITY_GIRVAN_NEWMAN), "Girvan-Newman");
}

TEST(DynamicalSystemsAPIRegression, ErrorStrings) {
    EXPECT_STREQ(dynsys_error_string(DYNSYS_OK), "Success");
    EXPECT_STREQ(dynsys_error_string(DYNSYS_ERR_NULL_PTR), "Null pointer");
    EXPECT_STREQ(dynsys_error_string(DYNSYS_ERR_INVALID_DIM), "Invalid dimension");
    EXPECT_STREQ(dynsys_error_string(DYNSYS_ERR_DIVERGENCE), "Numerical divergence");
    EXPECT_STREQ(dynsys_error_string(DYNSYS_ERR_NO_MEMORY), "Memory allocation failed");
}

TEST(DynamicalSystemsAPIRegression, BifurcationTypeNames) {
    EXPECT_STREQ(dynsys_bifurcation_type_name(BIFURCATION_NONE), "none");
    EXPECT_STREQ(dynsys_bifurcation_type_name(BIFURCATION_SADDLE_NODE), "saddle-node");
    EXPECT_STREQ(dynsys_bifurcation_type_name(BIFURCATION_HOPF), "Hopf");
    EXPECT_STREQ(dynsys_bifurcation_type_name(BIFURCATION_PERIOD_DOUBLING), "period-doubling");
}

TEST(DynamicalSystemsAPIRegression, AttractorTypeNames) {
    EXPECT_STREQ(dynsys_attractor_type_name(ATTRACTOR_UNKNOWN), "unknown");
    EXPECT_STREQ(dynsys_attractor_type_name(ATTRACTOR_FIXED_POINT), "fixed point");
    EXPECT_STREQ(dynsys_attractor_type_name(ATTRACTOR_LIMIT_CYCLE), "limit cycle");
    EXPECT_STREQ(dynsys_attractor_type_name(ATTRACTOR_STRANGE), "strange attractor");
}

//=============================================================================
// Null Parameter Handling Regression Tests
//=============================================================================

TEST(GraphTheoryBehaviorRegression, NullParameterHandling) {
    // These behaviors must remain consistent
    EXPECT_EQ(graph_theory_bridge_default_config(nullptr), GRAPH_THEORY_ERROR_INVALID_PARAM);
    // NULL config succeeds - uses default configuration
    EXPECT_NE(graph_theory_bridge_create(nullptr), nullptr);

    graph_theory_bridge_t bridge = graph_theory_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    graph_centrality_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(nullptr, nullptr, CENTRALITY_DEGREE, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);
    EXPECT_EQ(graph_theory_compute_centrality(bridge, nullptr, CENTRALITY_DEGREE, &result),
              GRAPH_THEORY_ERROR_INVALID_PARAM);

    graph_theory_bridge_destroy(bridge);
}

static int simple_system(const float* state, uint32_t state_dim,
                         const float* params, uint32_t param_dim,
                         float* derivative, void* context)
{
    (void)params;
    (void)param_dim;
    (void)context;
    for (uint32_t i = 0; i < state_dim; i++) {
        derivative[i] = -state[i];
    }
    return 0;
}

TEST(DynamicalSystemsBehaviorRegression, NullParameterHandling) {
    dynsys_config_t config = dynsys_default_config();

    EXPECT_EQ(dynsys_create(nullptr, simple_system, nullptr, nullptr), nullptr);
    EXPECT_EQ(dynsys_create(&config, nullptr, nullptr, nullptr), nullptr);

    dynsys_system_t sys = dynsys_create(&config, simple_system, nullptr, nullptr);
    ASSERT_NE(sys, nullptr);

    EXPECT_EQ(dynsys_step_rk4(nullptr, nullptr, 0.01f), DYNSYS_ERR_NULL_PTR);
    EXPECT_EQ(dynsys_step_rk4(sys, nullptr, 0.01f), DYNSYS_ERR_NULL_PTR);

    dynsys_destroy(sys);
}

TEST(InfoGeomBehaviorRegression, NullParameterHandling) {
    info_geom_bridge_t bridge = info_geom_bridge_create(nullptr);
    EXPECT_NE(bridge, nullptr);  // Should use defaults

    EXPECT_EQ(info_geom_bridge_register_kg(nullptr, nullptr), -1);
    EXPECT_EQ(info_geom_bridge_register_exception(nullptr, nullptr), -1);
    EXPECT_EQ(info_geom_bridge_register_bio_async(nullptr, nullptr), -1);

    info_geom_bridge_destroy(bridge);
}

//=============================================================================
// Memory Safety Regression Tests
//=============================================================================

TEST(GraphTheoryMemoryRegression, DestroyNullSafe) {
    graph_theory_bridge_destroy(nullptr);
    graph_centrality_result_destroy(nullptr);
    graph_community_result_destroy(nullptr);
    graph_spectral_result_destroy(nullptr);
    graph_qwalk_result_destroy(nullptr);
    graph_hyperbolic_result_destroy(nullptr);
    // Should not crash
}

TEST(DynamicalSystemsMemoryRegression, DestroyNullSafe) {
    dynsys_destroy(nullptr);
    dynsys_lyapunov_destroy(nullptr);
    dynsys_bifurcation_destroy(nullptr);
    dynsys_attractor_destroy(nullptr);
    dynsys_energy_destroy(nullptr);
    dynsys_slowfast_destroy(nullptr);
    dynsys_bridge_destroy(nullptr);
    // Should not crash
}

TEST(InfoGeomMemoryRegression, DestroyNullSafe) {
    info_geom_bridge_destroy(nullptr);
    // Should not crash
}

TEST(GraphTheoryMemoryRegression, CreateDestroyLoop) {
    // Test for memory leaks in create/destroy cycle
    for (int i = 0; i < 100; i++) {
        graph_theory_bridge_config_t config;
        graph_theory_bridge_default_config(&config);
        graph_theory_bridge_t bridge = graph_theory_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
        graph_theory_bridge_destroy(bridge);
    }
}

TEST(DynamicalSystemsMemoryRegression, CreateDestroyLoop) {
    for (int i = 0; i < 100; i++) {
        dynsys_config_t config = dynsys_default_config();
        dynsys_system_t sys = dynsys_create(&config, simple_system, nullptr, nullptr);
        ASSERT_NE(sys, nullptr);
        dynsys_destroy(sys);
    }
}

TEST(InfoGeomMemoryRegression, CreateDestroyLoop) {
    for (int i = 0; i < 100; i++) {
        info_geom_bridge_config_t config = info_geom_bridge_default_config();
        info_geom_bridge_t bridge = info_geom_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
        info_geom_bridge_destroy(bridge);
    }
}

//=============================================================================
// Behavioral Consistency Tests
//=============================================================================

TEST(GraphTheoryBehaviorRegression, CentralityResultConsistency) {
    graph_theory_bridge_t bridge = graph_theory_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    NimcpGraph* graph = nimcp_graph_create();
    ASSERT_NE(graph, nullptr);

    // Add 10 vertices
    for (int i = 0; i < 10; i++) {
        nimcp_graph_add_vertex(graph, (uint64_t)i, 0.0f, 0.0f, 0.0f, 0);
    }

    // Create a ring
    for (uint32_t i = 0; i < 10; i++) {
        nimcp_graph_add_edge(graph, i, (i + 1) % 10, 1.0f);
    }

    graph_centrality_result_t* result = nullptr;
    EXPECT_EQ(graph_theory_compute_centrality(bridge, graph, CENTRALITY_DEGREE, &result),
              GRAPH_THEORY_OK);
    ASSERT_NE(result, nullptr);

    // In a ring, all nodes have equal degree centrality
    float first_value = result->values[0];
    for (uint32_t i = 1; i < result->num_nodes; i++) {
        EXPECT_FLOAT_EQ(result->values[i], first_value);
    }

    graph_centrality_result_destroy(result);
    nimcp_graph_destroy(graph);
    graph_theory_bridge_destroy(bridge);
}

TEST(DynamicalSystemsBehaviorRegression, IntegrationEnergyConservation) {
    // For a harmonic oscillator, total energy should be conserved
    dynsys_config_t config = dynsys_default_config();
    config.state_dim = 2;
    config.param_dim = 1;
    config.dt = 0.001f;

    auto harmonic = [](const float* state, uint32_t state_dim,
                       const float* params, uint32_t param_dim,
                       float* derivative, void* context) -> int {
        (void)state_dim;
        (void)param_dim;
        (void)context;
        float omega = params ? params[0] : 1.0f;
        derivative[0] = state[1];
        derivative[1] = -omega * omega * state[0];
        return 0;
    };

    dynsys_system_t sys = dynsys_create(&config, harmonic, nullptr, nullptr);
    ASSERT_NE(sys, nullptr);

    float omega = 1.0f;
    dynsys_set_params(sys, &omega, 1);

    float state[] = {1.0f, 0.0f};  // Initial: x=1, v=0
    float initial_energy = 0.5f * (state[0] * state[0] + state[1] * state[1]);

    // Integrate for many steps
    for (int i = 0; i < 10000; i++) {
        dynsys_step_rk4(sys, state, 0.001f);
    }

    float final_energy = 0.5f * (state[0] * state[0] + state[1] * state[1]);

    // Energy should be conserved (within numerical tolerance)
    EXPECT_NEAR(final_energy, initial_energy, 0.01f);

    dynsys_destroy(sys);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
