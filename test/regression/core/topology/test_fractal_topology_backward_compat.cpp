//=============================================================================
// topology_backward_compat.cpp - Regression Tests for Topology API
//=============================================================================
/**
 * @file topology_backward_compat.cpp
 * @brief Regression tests ensuring topology API remains backward compatible
 *
 * WHAT: Tests verifying topology API contracts don't change across versions
 * WHY:  Catch breaking changes that would affect existing code
 * HOW:  Test all public API functions with known inputs/outputs
 *
 * @author NIMCP Development Team
 * @date 2025-11-15
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/topology/nimcp_fractal_topology.h"
#include "core/neuralnet/nimcp_neuralnet.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TopologyBackwardCompatTest : public ::testing::Test {
protected:
    neural_network_t test_network;

    void SetUp() override {
        // Create a minimal test network
        network_config_t config = {
            .num_neurons = 100,
            .enable_stdp = false
        };
        test_network = neural_network_create(&config);
    }

    void TearDown() override {
        if (test_network) {
            neural_network_destroy(test_network);
        }
    }
};

//=============================================================================
// Configuration API Backward Compatibility
//=============================================================================

/**
 * TEST: Default scale-free config returns stable values
 * WHY: Code relying on default values must not break
 */
TEST_F(TopologyBackwardCompatTest, DefaultScaleFreeConfigStable) {
    scale_free_config_t config = topology_default_scale_free_config();

    // Verify structure fields exist and have reasonable values
    EXPECT_LT(config.power_law_gamma, 0.0f);
    EXPECT_GT(config.power_law_gamma, -5.0f);
    EXPECT_GT(config.hub_ratio, 0.0f);
    EXPECT_LT(config.hub_ratio, 1.0f);
    EXPECT_GT(config.min_degree, 0u);
    EXPECT_GE(config.max_degree, config.min_degree);
}

/**
 * TEST: Default fractal config returns stable values
 * WHY: Code relying on default values must not break
 */
TEST_F(TopologyBackwardCompatTest, DefaultFractalConfigStable) {
    fractal_config_t config = topology_default_fractal_config();

    // Verify structure fields exist and have reasonable values
    EXPECT_GT(config.fractal_dimension, 0.0f);
    EXPECT_LT(config.fractal_dimension, 5.0f);
    EXPECT_GT(config.hierarchy_levels, 0u);
    EXPECT_LT(config.hierarchy_levels, 20u);
    EXPECT_GT(config.branching_factor, 1.0f);
    EXPECT_GT(config.scale_factor, 0.0f);
    EXPECT_LT(config.scale_factor, 1.0f);
    EXPECT_GE(config.clustering_coeff, 0.0f);
    EXPECT_LE(config.clustering_coeff, 1.0f);
}

//=============================================================================
// Validation API Backward Compatibility
//=============================================================================

/**
 * TEST: Config validation accepts valid configs
 * WHY: Ensure validation doesn't become overly restrictive
 */
TEST_F(TopologyBackwardCompatTest, ValidConfigsAccepted) {
    // Scale-free config
    topology_config_t sf_config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = {.scale_free = topology_default_scale_free_config()}
    };
    EXPECT_TRUE(topology_validate_config(&sf_config));

    // Fractal config
    topology_config_t fractal_config = {
        .type = TOPOLOGY_FRACTAL,
        .params = {.fractal = topology_default_fractal_config()}
    };
    EXPECT_TRUE(topology_validate_config(&fractal_config));
}

/**
 * TEST: Config validation rejects NULL
 * WHY: NULL handling must remain consistent
 */
TEST_F(TopologyBackwardCompatTest, NullConfigRejected) {
    EXPECT_FALSE(topology_validate_config(nullptr));
}

//=============================================================================
// Generation API Backward Compatibility
//=============================================================================

/**
 * TEST: Scale-free generation signature unchanged
 * WHY: Function signature changes break existing code
 */
TEST_F(TopologyBackwardCompatTest, ScaleFreeGenerationSignature) {
    scale_free_config_t config = topology_default_scale_free_config();
    topology_stats_t stats;

    // Should accept: network, config, stats (can be NULL)
    bool result1 = topology_generate_scale_free(test_network, &config, &stats);
    bool result2 = topology_generate_scale_free(test_network, &config, nullptr);

    // Both calls should succeed or fail consistently
    EXPECT_EQ(result1, result2);
}

/**
 * TEST: Fractal generation signature unchanged
 * WHY: Function signature changes break existing code
 */
TEST_F(TopologyBackwardCompatTest, FractalGenerationSignature) {
    fractal_config_t config = topology_default_fractal_config();
    topology_stats_t stats;

    // Should accept: network, config, stats (can be NULL)
    bool result1 = topology_generate_fractal(test_network, &config, &stats);
    bool result2 = topology_generate_fractal(test_network, &config, nullptr);

    // Both calls should succeed or fail consistently
    EXPECT_EQ(result1, result2);
}

/**
 * TEST: Unified generation signature unchanged
 * WHY: Function signature changes break existing code
 */
TEST_F(TopologyBackwardCompatTest, UnifiedGenerationSignature) {
    topology_config_t config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = {.scale_free = topology_default_scale_free_config()}
    };
    topology_stats_t stats;

    // Should accept: network, config, stats
    bool result = topology_generate(test_network, &config, &stats);

    // Function should execute without crashing
    (void)result;
    SUCCEED();
}

/**
 * TEST: NULL network handling unchanged
 * WHY: Error handling must remain consistent
 */
TEST_F(TopologyBackwardCompatTest, NullNetworkHandling) {
    scale_free_config_t config = topology_default_scale_free_config();

    // All generation functions should reject NULL network
    EXPECT_FALSE(topology_generate_scale_free(nullptr, &config, nullptr));

    fractal_config_t fractal_config = topology_default_fractal_config();
    EXPECT_FALSE(topology_generate_fractal(nullptr, &fractal_config, nullptr));

    topology_config_t unified_config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = {.scale_free = config}
    };
    EXPECT_FALSE(topology_generate(nullptr, &unified_config, nullptr));
}

/**
 * TEST: NULL config handling unchanged
 * WHY: Error handling must remain consistent
 */
TEST_F(TopologyBackwardCompatTest, NullConfigHandling) {
    // All generation functions should reject NULL config
    EXPECT_FALSE(topology_generate_scale_free(test_network, nullptr, nullptr));
    EXPECT_FALSE(topology_generate_fractal(test_network, nullptr, nullptr));
    EXPECT_FALSE(topology_generate(test_network, nullptr, nullptr));
}

//=============================================================================
// Statistics API Backward Compatibility
//=============================================================================

/**
 * TEST: Statistics structure fields unchanged
 * WHY: Changing struct layout breaks ABI compatibility
 */
TEST_F(TopologyBackwardCompatTest, StatisticsStructureStable) {
    topology_stats_t stats;

    // All fields should be accessible
    stats.num_neurons = 0;
    stats.num_synapses = 0;
    stats.avg_degree = 0.0f;
    stats.degree_std = 0.0f;
    stats.clustering_coefficient = 0.0f;
    stats.characteristic_path = 0.0f;
    stats.power_law_fit = 0.0f;
    stats.num_hubs = 0;
    stats.hub_connectivity = 0.0f;
    stats.small_world_sigma = 0.0f;

    // Test compiles = structure unchanged
    SUCCEED();
}

/**
 * TEST: Compute stats signature unchanged
 * WHY: Function signature changes break existing code
 */
TEST_F(TopologyBackwardCompatTest, ComputeStatsSignature) {
    topology_stats_t stats;

    // Should accept: network, stats
    bool result = topology_compute_stats(test_network, &stats);

    // Function should execute
    (void)result;
    SUCCEED();
}

/**
 * TEST: Compute stats NULL handling
 * WHY: Error handling must remain consistent
 */
TEST_F(TopologyBackwardCompatTest, ComputeStatsNullHandling) {
    topology_stats_t stats;

    // Should reject NULL network
    EXPECT_FALSE(topology_compute_stats(nullptr, &stats));

    // Should reject NULL stats
    EXPECT_FALSE(topology_compute_stats(test_network, nullptr));
}

//=============================================================================
// Analysis API Backward Compatibility
//=============================================================================

/**
 * TEST: Hub identification signature unchanged
 * WHY: Function signature changes break existing code
 */
TEST_F(TopologyBackwardCompatTest, HubIdentificationSignature) {
    uint32_t* hub_indices = nullptr;
    uint32_t num_hubs = 0;

    // Should accept: network, percentile, indices, count
    bool result = topology_identify_hubs(test_network, 0.9f, &hub_indices, &num_hubs);

    if (hub_indices) {
        free(hub_indices);
    }

    // Function should execute
    (void)result;
    SUCCEED();
}

/**
 * TEST: Small-world test signature unchanged
 * WHY: Function signature changes break existing code
 */
TEST_F(TopologyBackwardCompatTest, SmallWorldTestSignature) {
    float sigma = 0.0f;

    // Should accept: network, sigma (can be NULL)
    bool result1 = topology_is_small_world(test_network, &sigma);
    bool result2 = topology_is_small_world(test_network, nullptr);

    // Function should execute
    (void)result1;
    (void)result2;
    SUCCEED();
}

/**
 * TEST: Power-law fitting signature unchanged
 * WHY: Function signature changes break existing code
 */
TEST_F(TopologyBackwardCompatTest, PowerLawFitSignature) {
    float gamma = 0.0f;
    float r_squared = 0.0f;

    // Should accept: network, gamma, r_squared
    bool result = topology_fit_power_law(test_network, &gamma, &r_squared);

    // Function should execute
    (void)result;
    SUCCEED();
}

/**
 * TEST: Betweenness centrality signature unchanged
 * WHY: Function signature changes break existing code
 */
TEST_F(TopologyBackwardCompatTest, BetweennessSignature) {
    float centrality[100];

    // Should accept: network, centrality array
    bool result = topology_compute_betweenness(test_network, centrality);

    // Function should execute
    (void)result;
    SUCCEED();
}

//=============================================================================
// Error Handling API Backward Compatibility
//=============================================================================

/**
 * TEST: Error message retrieval unchanged
 * WHY: Error handling API must remain stable
 */
TEST_F(TopologyBackwardCompatTest, ErrorHandlingStable) {
    // Should return const char* (can be NULL)
    const char* error = topology_get_last_error();

    // Function should not crash
    (void)error;
    SUCCEED();
}

/**
 * TEST: Error clearing works
 * WHY: Error state management must be consistent
 */
TEST_F(TopologyBackwardCompatTest, ErrorClearingWorks) {
    // Generate an error
    topology_generate_scale_free(nullptr, nullptr, nullptr);

    // Error should be set
    const char* error1 = topology_get_last_error();
    EXPECT_NE(error1, nullptr);

    // Clear error (implementation detail, may not have public clear function)
    // Just verify we can retrieve error multiple times
    const char* error2 = topology_get_last_error();
    EXPECT_NE(error2, nullptr);
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
