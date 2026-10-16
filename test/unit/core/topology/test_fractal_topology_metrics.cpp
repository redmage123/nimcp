/**
 * Fractal Topology Graph Metrics Unit Tests
 *
 * WHAT: Comprehensive TDD tests for graph analysis metrics
 * WHY: Ensure mathematical correctness of topology analysis
 * HOW: Test each metric with known graph structures and expected values
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>

#include "core/topology/nimcp_fractal_topology.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class FractalTopologyMetricsTest : public ::testing::Test {
protected:
    neural_network_t network;

    void SetUp() override {
        // Create a small test network with proper configuration
        network_config_t config = {};
        config.num_neurons = 10;
        config.ei_ratio = 0.8f;
        config.learning_rate = 0.01f;
        config.hebbian_rate = 0.001f;
        config.stdp_window = 20.0f;
        config.homeostatic_rate = 0.0001f;
        config.target_activity = 0.1f;
        config.adaptation_rate = 0.01f;
        config.refractory_period = 2.0f;
        config.min_weight = -1.0f;
        config.max_weight = 1.0f;
        config.update_interval = 1;
        config.input_size = 10;
        config.output_size = 10;
        config.num_layers = 1;
        config.layer_sizes = nullptr;
        config.enable_stdp = false;
        config.enable_hebbian = false;
        config.enable_oja = false;
        config.enable_homeostasis = false;
        config.neuron_model = NEURON_MODEL_LIF;
        config.model_params = nullptr;
        config.integration_method = ODE_EULER;
        config.enable_bcm = false;
        config.enable_eligibility = false;

        network = neural_network_create(&config);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (network) {
            neural_network_destroy(network);
        }
    }

    // Helper: Create a simple triangle graph (3 nodes, all connected)
    void create_triangle_graph() {
        // Triangle: Node 0 <-> 1, 1 <-> 2, 2 <-> 0
        // Perfect clustering coefficient = 1.0
        neural_network_add_connection(network, 0, 1, 1.0f);
        neural_network_add_connection(network, 1, 0, 1.0f);
        neural_network_add_connection(network, 1, 2, 1.0f);
        neural_network_add_connection(network, 2, 1, 1.0f);
        neural_network_add_connection(network, 2, 0, 1.0f);
        neural_network_add_connection(network, 0, 2, 1.0f);
    }

    // Helper: Create a line graph (no triangles)
    void create_line_graph() {
        // Line: 0 -> 1 -> 2 (no cycles)
        // Clustering coefficient = 0.0
        neural_network_add_connection(network, 0, 1, 1.0f);
        neural_network_add_connection(network, 1, 2, 1.0f);
    }

    // Helper: Create a star graph (hub topology)
    void create_star_graph() {
        // Star: Node 0 at center, connected to all others
        // Central hub with no clustering among spokes
        for (uint32_t i = 1; i < 10; i++) {
            neural_network_add_connection(network, 0, i, 1.0f);
            neural_network_add_connection(network, i, 0, 1.0f);
        }
    }
};

//=============================================================================
// Clustering Coefficient Tests
//=============================================================================

/**
 * TEST: Clustering coefficient for perfect triangle
 * EXPECTED: 1.0 (all neighbors are connected)
 */
TEST_F(FractalTopologyMetricsTest, ClusteringCoefficient_Triangle_ReturnsOne) {
    create_triangle_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_FLOAT_EQ(stats.clustering_coefficient, 1.0f)
        << "Triangle graph should have perfect clustering";
}

/**
 * TEST: Clustering coefficient for line graph
 * EXPECTED: 0.0 (no triangles)
 */
TEST_F(FractalTopologyMetricsTest, ClusteringCoefficient_Line_ReturnsZero) {
    create_line_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_FLOAT_EQ(stats.clustering_coefficient, 0.0f)
        << "Line graph should have zero clustering";
}

/**
 * TEST: Clustering coefficient for star graph
 * EXPECTED: 0.0 (hub has no clustering among spokes)
 */
TEST_F(FractalTopologyMetricsTest, ClusteringCoefficient_Star_ReturnsZero) {
    create_star_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_FLOAT_EQ(stats.clustering_coefficient, 0.0f)
        << "Star graph should have zero clustering";
}

//=============================================================================
// Characteristic Path Length Tests
//=============================================================================

/**
 * TEST: Path length for triangle graph
 * EXPECTED: 1.0 (all nodes directly connected)
 */
TEST_F(FractalTopologyMetricsTest, CharacteristicPath_Triangle_ReturnsOne) {
    create_triangle_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_FLOAT_EQ(stats.characteristic_path, 1.0f)
        << "Triangle graph should have path length 1";
}

/**
 * TEST: Path length for line graph
 * EXPECTED: ~1.33 (average of paths: 1, 2)
 */
TEST_F(FractalTopologyMetricsTest, CharacteristicPath_Line_ReturnsAverage) {
    create_line_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    // Line: 0->1 (dist 1), 1->2 (dist 1), 0->2 (dist 2)
    // Average path for 3 nodes in line: (1 + 1 + 2) / 3 = 1.333
    EXPECT_NEAR(stats.characteristic_path, 1.333f, 0.01f)
        << "Line graph should have average path length ~1.33";
}

/**
 * TEST: Path length for star graph
 * EXPECTED: ~1.89 (most paths go through hub)
 */
TEST_F(FractalTopologyMetricsTest, CharacteristicPath_Star_ReturnsTwoHopAverage) {
    create_star_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    // Star with 10 nodes: center has dist 1 to all (9 paths)
    // Spokes have dist 2 to each other through center (36 paths)
    // Average: (9*1 + 36*2) / 45 = 81/45 = 1.8
    EXPECT_NEAR(stats.characteristic_path, 1.8f, 0.1f)
        << "Star graph should have path length ~1.8";
}

//=============================================================================
// Power-Law Fit Tests
//=============================================================================

/**
 * TEST: Power-law fit for uniform degree distribution
 * EXPECTED: Low R² (not scale-free)
 */
TEST_F(FractalTopologyMetricsTest, PowerLawFit_Triangle_LowRSquared) {
    create_triangle_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_LT(stats.power_law_fit, 0.5f)
        << "Triangle graph (uniform degree) should not fit power-law";
}

/**
 * TEST: Power-law fit for star graph
 * EXPECTED: High R² (hub creates power-law-like distribution)
 */
TEST_F(FractalTopologyMetricsTest, PowerLawFit_Star_HighRSquared) {
    create_star_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_GT(stats.power_law_fit, 0.7f)
        << "Star graph (hub topology) should fit power-law well";
}

//=============================================================================
// Hub Detection Tests
//=============================================================================

/**
 * TEST: Hub detection in star graph
 * EXPECTED: 1 hub (center node)
 */
TEST_F(FractalTopologyMetricsTest, HubDetection_Star_FindsOneHub) {
    create_star_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_EQ(stats.num_hubs, 1u)
        << "Star graph should have exactly 1 hub";
}

/**
 * TEST: Hub connectivity in star graph
 * EXPECTED: High hub connectivity (all paths through center)
 */
TEST_F(FractalTopologyMetricsTest, HubConnectivity_Star_HighValue) {
    create_star_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_GT(stats.hub_connectivity, 0.9f)
        << "Star graph should have very high hub connectivity";
}

/**
 * TEST: Hub detection in triangle
 * EXPECTED: 0 hubs (all nodes have equal degree)
 */
TEST_F(FractalTopologyMetricsTest, HubDetection_Triangle_FindsNoHubs) {
    create_triangle_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_EQ(stats.num_hubs, 0u)
        << "Triangle graph (uniform degree) should have no hubs";
}

//=============================================================================
// Small-World Sigma Tests
//=============================================================================

/**
 * TEST: Small-world sigma for random graph
 * EXPECTED: ~1.0 (baseline)
 */
TEST_F(FractalTopologyMetricsTest, SmallWorldSigma_RandomGraph_NearOne) {
    // Create random connections with deterministic seed
    srand(12345);
    for (uint32_t i = 0; i < 20; i++) {
        uint32_t from = rand() % 10;
        uint32_t to = rand() % 10;
        if (from != to) {
            neural_network_add_connection(network, from, to, 1.0f);
        }
    }

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_NEAR(stats.small_world_sigma, 1.0f, 0.5f)
        << "Random graph should have sigma near 1.0";
}

/**
 * TEST: Small-world sigma for high-clustering, short-path graph
 * EXPECTED: > 1.0 (small-world property)
 */
TEST_F(FractalTopologyMetricsTest, SmallWorldSigma_HighClusteringLowPath_GreaterThanOne) {
    // Create clustered ring with shortcuts (Watts-Strogatz style)
    // This will have high clustering and low path length = small-world
    for (uint32_t i = 0; i < 10; i++) {
        // Ring connections
        neural_network_add_connection(network, i, (i + 1) % 10, 1.0f);
        neural_network_add_connection(network, i, (i + 2) % 10, 1.0f);
        // Shortcuts
        if (i % 3 == 0) {
            neural_network_add_connection(network, i, (i + 5) % 10, 1.0f);
        }
    }

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    EXPECT_GT(stats.small_world_sigma, 1.0f)
        << "Clustered ring with shortcuts should have small-world property";
}

//=============================================================================
// Betweenness Centrality Tests (Helper Functions)
//=============================================================================

/**
 * TEST: Betweenness centrality calculation
 * NOTE: This tests internal helper function used for hub_connectivity
 */
TEST_F(FractalTopologyMetricsTest, BetweennessCentrality_Star_CenterHighest) {
    create_star_graph();

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_compute_stats(network, &stats));

    // In star graph, center node has highest betweenness
    // All spoke-to-spoke paths go through center
    // hub_connectivity should reflect this
    EXPECT_GT(stats.hub_connectivity, 0.8f);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: Full analysis on scale-free network
 * EXPECTED: All metrics computed without errors
 */
TEST_F(FractalTopologyMetricsTest, FullAnalysis_ScaleFreeNetwork_AllMetricsComputed) {
    // Generate scale-free network
    scale_free_config_t config = topology_default_scale_free_config();
    config.hub_ratio = 0.2f;

    topology_config_t topo_config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = { .scale_free = config }
    };

    topology_stats_t stats = {};
    ASSERT_TRUE(topology_generate(network, &topo_config, &stats));

    // Verify all metrics are reasonable
    EXPECT_GT(stats.num_synapses, 0u);
    EXPECT_GE(stats.clustering_coefficient, 0.0f);
    EXPECT_LE(stats.clustering_coefficient, 1.0f);
    EXPECT_GT(stats.characteristic_path, 0.0f);
    EXPECT_GE(stats.power_law_fit, 0.0f);
    EXPECT_LE(stats.power_law_fit, 1.0f);
    EXPECT_GE(stats.small_world_sigma, 0.0f);
}

/**
 * TEST: Error handling for NULL network
 */
TEST_F(FractalTopologyMetricsTest, AnalyzeNetwork_NullNetwork_ReturnsFalse) {
    topology_stats_t stats = {};
    EXPECT_FALSE(topology_compute_stats(nullptr, &stats));

    const char* error = topology_get_last_error();
    EXPECT_NE(error, nullptr);
}

/**
 * TEST: Error handling for NULL stats
 */
TEST_F(FractalTopologyMetricsTest, AnalyzeNetwork_NullStats_ReturnsFalse) {
    EXPECT_FALSE(topology_compute_stats(network, nullptr));

    const char* error = topology_get_last_error();
    EXPECT_NE(error, nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
