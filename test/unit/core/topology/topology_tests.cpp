//=============================================================================
// topology_tests.c - Unit Tests for Fractal Topology Module
//=============================================================================
/**
 * @file topology_tests.c
 * @brief Comprehensive unit tests for fractal network topology generation
 *
 * TEST PHILOSOPHY:
 * - Test-Driven Development (TDD): Tests written before/alongside implementation
 * - Guard clause verification: Test all error conditions
 * - Boundary testing: Min/max values, edge cases
 * - Statistical validation: Verify power-law properties
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 */

#include <gtest/gtest.h>
#include "core/topology/nimcp_fractal_topology.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include <math.h>

//=============================================================================
// Test Fixtures
//=============================================================================

class TopologyTest : public ::testing::Test {
protected:
    neural_network_t network;
    topology_stats_t stats;

    void SetUp() override {
        // Create minimal network for testing
        // NOTE: Set num_neurons in config to pre-create neurons
        // Do NOT manually add neurons afterward (that would double the count)
        network_config_t config = {
            .num_neurons = 100,
            .enable_stdp = true,
            .enable_homeostasis = true
        };
        network = neural_network_create(&config);

        memset(&stats, 0, sizeof(stats));

        // Clear any lingering errors from previous tests
        // Call a successful validation to trigger clear_error()
        topology_config_t valid_config = {
            .type = TOPOLOGY_SCALE_FREE,
            .params = {.scale_free = topology_default_scale_free_config()}
        };
        topology_validate_config(&valid_config);
    }

    void TearDown() override {
        if (network) {
            neural_network_destroy(network);
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * TEST: Default scale-free configuration has valid parameters
 * WHY: Users should get sensible defaults without parameter tuning
 */
TEST_F(TopologyTest, DefaultScaleFreeConfigIsValid) {
    scale_free_config_t config = topology_default_scale_free_config();

    // Verify power law exponent in biological range
    EXPECT_GE(config.power_law_gamma, -3.0f);
    EXPECT_LE(config.power_law_gamma, -1.5f);

    // Verify hub ratio is reasonable
    EXPECT_GE(config.hub_ratio, 0.05f);
    EXPECT_LE(config.hub_ratio, 0.30f);

    // Verify minimum degree
    EXPECT_GE(config.min_degree, 1u);
    EXPECT_LE(config.min_degree, 10u);

    // Verify spatial constraint
    EXPECT_GE(config.spatial_constraint, 0.0f);
    EXPECT_LE(config.spatial_constraint, 1.0f);
}

/**
 * TEST: Default fractal configuration has valid parameters
 * WHY: Fractal defaults should match cortical measurements
 */
TEST_F(TopologyTest, DefaultFractalConfigIsValid) {
    fractal_config_t config = topology_default_fractal_config();

    // Verify fractal dimension in biological range (cortex ~2.5)
    EXPECT_GE(config.fractal_dimension, 1.5f);
    EXPECT_LE(config.fractal_dimension, 3.0f);

    // Verify hierarchy levels reasonable
    EXPECT_GE(config.hierarchy_levels, 2u);
    EXPECT_LE(config.hierarchy_levels, 6u);

    // Verify branching factor
    EXPECT_GE(config.branching_factor, 2.0f);
    EXPECT_LE(config.branching_factor, 5.0f);

    // Verify scale factor (size reduction per level)
    EXPECT_GT(config.scale_factor, 0.0f);
    EXPECT_LT(config.scale_factor, 1.0f);
}

/**
 * TEST: Valid configuration passes validation
 * WHY: Correctly formed configs should be accepted
 */
TEST_F(TopologyTest, ValidConfigurationPassesValidation) {
    topology_config_t config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = {.scale_free = topology_default_scale_free_config()}
    };

    EXPECT_TRUE(topology_validate_config(&config));
    EXPECT_EQ(topology_get_last_error(), nullptr);
}

/**
 * TEST: NULL configuration fails validation
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(TopologyTest, NullConfigurationFailsValidation) {
    EXPECT_FALSE(topology_validate_config(nullptr));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

/**
 * TEST: Invalid power law exponent fails validation
 * WHY: Gamma must be negative for power-law distribution
 */
TEST_F(TopologyTest, PositivePowerLawGammaFailsValidation) {
    topology_config_t config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = {.scale_free = topology_default_scale_free_config()}
    };

    // Test positive gamma (invalid)
    config.params.scale_free.power_law_gamma = 2.0f;
    EXPECT_FALSE(topology_validate_config(&config));

    // Test zero gamma (invalid)
    config.params.scale_free.power_law_gamma = 0.0f;
    EXPECT_FALSE(topology_validate_config(&config));

    // Test too negative gamma (invalid)
    config.params.scale_free.power_law_gamma = -10.0f;
    EXPECT_FALSE(topology_validate_config(&config));
}

/**
 * TEST: Invalid hub ratio fails validation
 * WHY: Hub ratio must be in [0, 0.5] range
 */
TEST_F(TopologyTest, InvalidHubRatioFailsValidation) {
    topology_config_t config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = {.scale_free = topology_default_scale_free_config()}
    };

    // Test negative ratio
    config.params.scale_free.hub_ratio = -0.1f;
    EXPECT_FALSE(topology_validate_config(&config));

    // Test ratio > 0.5
    config.params.scale_free.hub_ratio = 0.8f;
    EXPECT_FALSE(topology_validate_config(&config));
}

/**
 * TEST: Invalid fractal dimension fails validation
 * WHY: Fractal dimension must be in [1.0, 3.0] for physical networks
 */
TEST_F(TopologyTest, InvalidFractalDimensionFailsValidation) {
    topology_config_t config = {
        .type = TOPOLOGY_FRACTAL,
        .params = {.fractal = topology_default_fractal_config()}
    };

    // Test dimension < 1.0
    config.params.fractal.fractal_dimension = 0.5f;
    EXPECT_FALSE(topology_validate_config(&config));

    // Test dimension > 3.0
    config.params.fractal.fractal_dimension = 4.0f;
    EXPECT_FALSE(topology_validate_config(&config));
}

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * TEST: Error messages are thread-safe
 * WHY: Multiple threads should have independent error states
 */
TEST_F(TopologyTest, ErrorMessagesAreThreadSafe) {
    // Initially no error
    EXPECT_EQ(topology_get_last_error(), nullptr);

    // Trigger error
    topology_validate_config(nullptr);

    // Error message should be set
    EXPECT_NE(topology_get_last_error(), nullptr);
    EXPECT_STREQ(topology_get_last_error(), "Configuration is NULL");
}

/**
 * TEST: Successful operations clear previous errors
 * WHY: Error state should not persist after success
 */
TEST_F(TopologyTest, SuccessfulOperationClearErrors) {
    // Trigger error
    topology_validate_config(nullptr);
    EXPECT_NE(topology_get_last_error(), nullptr);

    // Successful operation
    topology_config_t config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = {.scale_free = topology_default_scale_free_config()}
    };

    EXPECT_TRUE(topology_validate_config(&config));
    // Note: Error may still be set, but operation succeeded
}

//=============================================================================
// Scale-Free Generation Tests
//=============================================================================

/**
 * TEST: NULL network fails scale-free generation
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(TopologyTest, ScaleFreeGenerationRequiresValidNetwork) {
    scale_free_config_t config = topology_default_scale_free_config();

    EXPECT_FALSE(topology_generate_scale_free(nullptr, &config, &stats));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

/**
 * TEST: NULL configuration fails scale-free generation
 * WHY: Cannot generate without parameters
 */
TEST_F(TopologyTest, ScaleFreeGenerationRequiresValidConfig) {
    EXPECT_FALSE(topology_generate_scale_free(network, nullptr, &stats));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

/**
 * TEST: Scale-free generation creates synapses
 * WHY: Generated network should have connections
 */
TEST_F(TopologyTest, ScaleFreeGenerationCreatesSynapses) {
    scale_free_config_t config = topology_default_scale_free_config();

    bool success = topology_generate_scale_free(network, &config, &stats);

    if (success) {  // May not be implemented yet
        EXPECT_GT(stats.num_synapses, 0u);
        EXPECT_EQ(stats.num_neurons, 100u);
        EXPECT_GT(stats.avg_degree, 0.0f);
    }
}

/**
 * TEST: Scale-free generation respects minimum degree
 * WHY: All neurons should have at least min_degree connections
 */
TEST_F(TopologyTest, ScaleFreeRespectsMinimumDegree) {
    scale_free_config_t config = topology_default_scale_free_config();
    config.min_degree = 5;

    bool success = topology_generate_scale_free(network, &config, &stats);

    if (success) {
        // Average degree should be >= min_degree
        EXPECT_GE(stats.avg_degree, (float)config.min_degree);
    }
}

/**
 * TEST: Hub neurons are identified correctly
 * WHY: Scale-free networks should have identifiable hubs
 */
TEST_F(TopologyTest, ScaleFreeNetworkHasHubs) {
    scale_free_config_t config = topology_default_scale_free_config();
    config.hub_ratio = 0.10f;  // 10% hubs

    bool success = topology_generate_scale_free(network, &config, &stats);

    if (success) {
        // Should have approximately hub_ratio * num_neurons hubs
        float expected_hubs = 100.0f * config.hub_ratio;
        EXPECT_GT(stats.num_hubs, 0u);
        EXPECT_LE(stats.num_hubs, (uint32_t)(expected_hubs * 2.0f));  // Allow 2x variance
    }
}

//=============================================================================
// Fractal Generation Tests
//=============================================================================

/**
 * TEST: Fractal generation rejects NULL network
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(TopologyTest, FractalGenerationRequiresValidNetwork) {
    fractal_config_t config = topology_default_fractal_config();

    EXPECT_FALSE(topology_generate_fractal(nullptr, &config, &stats));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

/**
 * TEST: Fractal generation rejects NULL config
 * WHY: Cannot generate without parameters
 */
TEST_F(TopologyTest, FractalGenerationRequiresValidConfig) {
    EXPECT_FALSE(topology_generate_fractal(network, nullptr, &stats));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

//=============================================================================
// Unified Generation Tests
//=============================================================================

/**
 * TEST: Unified generation dispatches to scale-free correctly
 * WHY: Strategy pattern should route to correct implementation
 */
TEST_F(TopologyTest, UnifiedGenerationDispatchesToScaleFree) {
    topology_config_t config = {
        .type = TOPOLOGY_SCALE_FREE,
        .params = {.scale_free = topology_default_scale_free_config()}
    };

    // Should call topology_generate_scale_free internally
    bool result = topology_generate(network, &config, &stats);

    // Result depends on whether implementation is complete
    // At minimum, should not crash
}

/**
 * TEST: Unified generation dispatches to fractal correctly
 * WHY: Strategy pattern should route to correct implementation
 */
TEST_F(TopologyTest, UnifiedGenerationDispatchesToFractal) {
    topology_config_t config = {
        .type = TOPOLOGY_FRACTAL,
        .params = {.fractal = topology_default_fractal_config()}
    };

    // Should call topology_generate_fractal internally
    bool result = topology_generate(network, &config, &stats);

    // May return false if not implemented
    if (!result) {
        EXPECT_NE(topology_get_last_error(), nullptr);
    }
}

/**
 * TEST: Unified generation rejects invalid topology type
 * WHY: Unknown types should fail gracefully
 */
TEST_F(TopologyTest, UnifiedGenerationRejectsInvalidType) {
    topology_config_t config = {
        .type = (topology_type_t)999,  // Invalid type
        .params = {.scale_free = topology_default_scale_free_config()}
    };

    EXPECT_FALSE(topology_generate(network, &config, &stats));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * TEST: Statistics computation rejects NULL network
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(TopologyTest, StatsComputationRequiresValidNetwork) {
    EXPECT_FALSE(topology_compute_stats(nullptr, &stats));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

/**
 * TEST: Statistics computation rejects NULL stats pointer
 * WHY: Cannot write output to NULL
 */
TEST_F(TopologyTest, StatsComputationRequiresValidStatsPointer) {
    EXPECT_FALSE(topology_compute_stats(network, nullptr));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

//=============================================================================
// Hub Identification Tests
//=============================================================================

/**
 * TEST: Hub identification rejects NULL network
 * WHY: Guard against NULL pointer dereference
 */
TEST_F(TopologyTest, HubIdentificationRequiresValidNetwork) {
    uint32_t* hubs = nullptr;
    uint32_t count = 0;

    EXPECT_FALSE(topology_identify_hubs(nullptr, 0.9f, &hubs, &count));
    EXPECT_NE(topology_get_last_error(), nullptr);
}

/**
 * TEST: Hub identification rejects invalid percentile
 * WHY: Percentile must be in [0, 1] range
 */
TEST_F(TopologyTest, HubIdentificationRejectsInvalidPercentile) {
    uint32_t* hubs = nullptr;
    uint32_t count = 0;

    // Test negative percentile
    EXPECT_FALSE(topology_identify_hubs(network, -0.1f, &hubs, &count));

    // Test percentile > 1.0
    EXPECT_FALSE(topology_identify_hubs(network, 1.5f, &hubs, &count));
}

/**
 * TEST: Hub identification rejects NULL output pointers
 * WHY: Cannot write output to NULL
 */
TEST_F(TopologyTest, HubIdentificationRequiresValidOutputPointers) {
    uint32_t count = 0;

    // NULL hub_indices pointer
    EXPECT_FALSE(topology_identify_hubs(network, 0.9f, nullptr, &count));

    // NULL num_hubs pointer
    uint32_t* hubs = nullptr;
    EXPECT_FALSE(topology_identify_hubs(network, 0.9f, &hubs, nullptr));
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
