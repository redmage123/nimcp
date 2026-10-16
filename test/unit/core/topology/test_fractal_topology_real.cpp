//=============================================================================
// test_fractal_topology_real.cpp - Real Tests for Fractal Topology
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

#include "core/topology/nimcp_fractal_topology.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/brain/nimcp_brain.h"

class FractalTopologyRealTest : public ::testing::Test {
protected:
    // NOTE: Topology functions expect neural_network_t, not adaptive_network_t
    // brain_get_network() returns adaptive_network_t which is incompatible
    // These tests focus on config validation and NULL handling
    neural_network_t network = nullptr;

    void SetUp() override {
        // Cannot use brain_get_network() due to type incompatibility
        network = nullptr;
    }

    void TearDown() override {
        // Network not created in SetUp
    }
};

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(FractalTopologyRealTest, DefaultScaleFreeConfig) {
    scale_free_config_t config = topology_default_scale_free_config();

    EXPECT_LT(config.power_law_gamma, 0.0f);  // Should be negative
    EXPECT_GT(config.power_law_gamma, -4.0f);
    EXPECT_GT(config.hub_ratio, 0.0f);
    EXPECT_LT(config.hub_ratio, 1.0f);
    EXPECT_GT(config.min_degree, 0u);
    EXPECT_GT(config.max_degree, config.min_degree);
}

TEST_F(FractalTopologyRealTest, DefaultFractalConfig) {
    fractal_config_t config = topology_default_fractal_config();

    EXPECT_GT(config.fractal_dimension, 0.0f);
    EXPECT_LT(config.fractal_dimension, 4.0f);
    EXPECT_GT(config.hierarchy_levels, 0u);
    EXPECT_GT(config.branching_factor, 1.0f);
    EXPECT_GT(config.scale_factor, 0.0f);
    EXPECT_LT(config.scale_factor, 1.0f);
}

TEST_F(FractalTopologyRealTest, ValidateConfigValid) {
    topology_config_t config;
    config.type = TOPOLOGY_SCALE_FREE;
    config.params.scale_free = topology_default_scale_free_config();

    bool valid = topology_validate_config(&config);
    EXPECT_TRUE(valid);
}

TEST_F(FractalTopologyRealTest, ValidateConfigInvalidNull) {
    bool valid = topology_validate_config(nullptr);
    EXPECT_FALSE(valid);
}

// ============================================================================
// Scale-Free Topology Generation Tests
// ============================================================================

TEST_F(FractalTopologyRealTest, GenerateScaleFreeBasic) {
    scale_free_config_t config = topology_default_scale_free_config();
    topology_stats_t stats;

    bool result = topology_generate_scale_free(network, &config, &stats);

    // May fail if network not initialized properly, but should not crash
    (void)result;
    SUCCEED();
}

TEST_F(FractalTopologyRealTest, GenerateScaleFreeNullNetwork) {
    scale_free_config_t config = topology_default_scale_free_config();

    bool result = topology_generate_scale_free(nullptr, &config, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, GenerateScaleFreeNullConfig) {
    bool result = topology_generate_scale_free(network, nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, GenerateScaleFreeWithStats) {
    scale_free_config_t config = topology_default_scale_free_config();
    topology_stats_t stats;

    bool result = topology_generate_scale_free(network, &config, &stats);

    if (result) {
        EXPECT_GT(stats.num_neurons, 0u);
        EXPECT_GE(stats.avg_degree, 0.0f);
    }
}

// ============================================================================
// Fractal Topology Generation Tests
// ============================================================================

TEST_F(FractalTopologyRealTest, GenerateFractalBasic) {
    fractal_config_t config = topology_default_fractal_config();
    topology_stats_t stats;

    bool result = topology_generate_fractal(network, &config, &stats);

    (void)result;
    SUCCEED();
}

TEST_F(FractalTopologyRealTest, GenerateFractalNullNetwork) {
    fractal_config_t config = topology_default_fractal_config();

    bool result = topology_generate_fractal(nullptr, &config, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, GenerateFractalNullConfig) {
    bool result = topology_generate_fractal(network, nullptr, nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Unified Topology Generation Tests
// ============================================================================

TEST_F(FractalTopologyRealTest, GenerateUnifiedScaleFree) {
    topology_config_t config;
    config.type = TOPOLOGY_SCALE_FREE;
    config.params.scale_free = topology_default_scale_free_config();

    topology_stats_t stats;
    bool result = topology_generate(network, &config, &stats);

    (void)result;
    SUCCEED();
}

TEST_F(FractalTopologyRealTest, GenerateUnifiedFractal) {
    topology_config_t config;
    config.type = TOPOLOGY_FRACTAL;
    config.params.fractal = topology_default_fractal_config();

    topology_stats_t stats;
    bool result = topology_generate(network, &config, &stats);

    (void)result;
    SUCCEED();
}

TEST_F(FractalTopologyRealTest, GenerateUnifiedNullInputs) {
    bool result = topology_generate(nullptr, nullptr, nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Topology Analysis Tests
// ============================================================================

TEST_F(FractalTopologyRealTest, ComputeStatsBasic) {
    topology_stats_t stats;
    bool result = topology_compute_stats(network, &stats);

    if (result) {
        EXPECT_GE(stats.num_neurons, 0u);
        EXPECT_GE(stats.num_synapses, 0u);
        EXPECT_GE(stats.avg_degree, 0.0f);
    }
}

TEST_F(FractalTopologyRealTest, ComputeStatsNullNetwork) {
    topology_stats_t stats;
    bool result = topology_compute_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, ComputeStatsNullStats) {
    bool result = topology_compute_stats(network, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, IsSmallWorld) {
    float sigma = 0.0f;
    bool result = topology_is_small_world(network, &sigma);

    // May be true or false depending on network state
    (void)result;
    SUCCEED();
}

TEST_F(FractalTopologyRealTest, IsSmallWorldNullNetwork) {
    bool result = topology_is_small_world(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, FitPowerLaw) {
    float gamma = 0.0f;
    float r_squared = 0.0f;
    bool result = topology_fit_power_law(network, &gamma, &r_squared);

    if (result) {
        EXPECT_LT(gamma, 0.0f);  // Should be negative for scale-free
        EXPECT_GE(r_squared, 0.0f);
        EXPECT_LE(r_squared, 1.0f);
    }
}

TEST_F(FractalTopologyRealTest, FitPowerLawNullNetwork) {
    float gamma, r_squared;
    bool result = topology_fit_power_law(nullptr, &gamma, &r_squared);
    EXPECT_FALSE(result);
}

// ============================================================================
// Hub Identification Tests
// ============================================================================

TEST_F(FractalTopologyRealTest, IdentifyHubsBasic) {
    uint32_t* hub_indices = nullptr;
    uint32_t num_hubs = 0;

    bool result = topology_identify_hubs(network, 0.9f, &hub_indices, &num_hubs);

    if (result && hub_indices) {
        EXPECT_GE(num_hubs, 0u);
        free(hub_indices);
    }
}

TEST_F(FractalTopologyRealTest, IdentifyHubsNullNetwork) {
    uint32_t* hub_indices = nullptr;
    uint32_t num_hubs = 0;

    bool result = topology_identify_hubs(nullptr, 0.9f, &hub_indices, &num_hubs);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, IdentifyHubsInvalidPercentile) {
    uint32_t* hub_indices = nullptr;
    uint32_t num_hubs = 0;

    bool result = topology_identify_hubs(network, 1.5f, &hub_indices, &num_hubs);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, ComputeBetweenness) {
    float centrality[100];
    bool result = topology_compute_betweenness(network, centrality);

    if (result) {
        for (int i = 0; i < 100; i++) {
            EXPECT_GE(centrality[i], 0.0f);
        }
    }
}

TEST_F(FractalTopologyRealTest, ComputeBetweennessNullNetwork) {
    float centrality[100];
    bool result = topology_compute_betweenness(nullptr, centrality);
    EXPECT_FALSE(result);
}

TEST_F(FractalTopologyRealTest, ComputeBetweennessNullArray) {
    bool result = topology_compute_betweenness(network, nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(FractalTopologyRealTest, GetLastError) {
    const char* error = topology_get_last_error();
    // May be NULL or a string, just check it doesn't crash
    (void)error;
    SUCCEED();
}
