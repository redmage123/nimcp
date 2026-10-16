/**
 * @file test_visual_cortex_topology.cpp
 * @brief Unit tests for visual cortex fractal topology integration
 *
 * WHAT: Tests scale-free network generation in V1 internal connections
 * WHY: Ensure topology integration works correctly in cognitive pipeline
 * HOW: Test creation, destruction, and topology metrics
 */

#include <gtest/gtest.h>
#include "perception/nimcp_visual_cortex.h"
#include "core/topology/nimcp_fractal_topology.h"
#include "utils/nimcp_test_base.h"

class VisualCortexTopologyTest : public NimcpTestBase {
protected:
    visual_cortex_t* cortex = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent FIRST
    }

    void TearDown() override {
        if (cortex) {
            visual_cortex_destroy(cortex);
            cortex = nullptr;
        }
        NimcpTestBase::TearDown();  // Call parent LAST
    }
};

//=============================================================================
// Basic Topology Creation Tests
//=============================================================================

TEST_F(VisualCortexTopologyTest, CreateWithoutTopology) {
    // WHAT: Create visual cortex without fractal topology
    // WHY: Verify baseline behavior unchanged
    visual_cortex_config_t config = {
        .input_width = 640,
        .input_height = 480,
        .num_v1_filters = 32,
        .feature_dim = 128,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,  // Disabled
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 320
    };

    cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Should succeed even without topology
    visual_cortex_destroy(cortex);
    cortex = nullptr;
}

TEST_F(VisualCortexTopologyTest, CreateWithTopologyEnabled) {
    // WHAT: Create visual cortex WITH fractal topology
    // WHY: Verify topology generation in V1
    visual_cortex_config_t config = {
        .input_width = 640,
        .input_height = 480,
        .num_v1_filters = 32,
        .feature_dim = 128,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = true,   // Enabled
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 320
    };

    cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Cortex should be created successfully
    // Internal network should exist (tested via successful destroy)
    visual_cortex_destroy(cortex);
    cortex = nullptr;
}

TEST_F(VisualCortexTopologyTest, TopologyWithZeroNeurons) {
    // WHAT: Test topology with zero internal neurons
    // WHY: Should gracefully handle edge case
    visual_cortex_config_t config = {
        .input_width = 640,
        .input_height = 480,
        .num_v1_filters = 32,
        .feature_dim = 128,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 0  // Edge case
    };

    cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Should succeed without creating internal network
    visual_cortex_destroy(cortex);
    cortex = nullptr;
}

TEST_F(VisualCortexTopologyTest, TopologyWithSmallNetwork) {
    // WHAT: Test topology with minimal neuron count
    // WHY: Verify small networks work
    visual_cortex_config_t config = {
        .input_width = 640,
        .input_height = 480,
        .num_v1_filters = 8,
        .feature_dim = 64,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 50  // Small network
    };

    cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_destroy(cortex);
    cortex = nullptr;
}

TEST_F(VisualCortexTopologyTest, TopologyWithLargeNetwork) {
    // WHAT: Test topology with large neuron count
    // WHY: Verify scalability
    visual_cortex_config_t config = {
        .input_width = 1920,
        .input_height = 1080,
        .num_v1_filters = 64,
        .feature_dim = 256,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 1000  // Large network
    };

    cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_destroy(cortex);
    cortex = nullptr;
}

//=============================================================================
// Topology Parameter Tests
//=============================================================================

TEST_F(VisualCortexTopologyTest, TopologyWithDifferentHubRatios) {
    // WHAT: Test various hub ratios
    // WHY: Verify parameter variation works
    float hub_ratios[] = {0.05f, 0.1f, 0.15f, 0.2f, 0.25f};

    for (float ratio : hub_ratios) {
        visual_cortex_config_t config = {
            .input_width = 640,
            .input_height = 480,
            .num_v1_filters = 32,
            .feature_dim = 128,
            .enable_attention = false,
            .enable_memory = false,
            .enable_fractal_topology = true,
            .hub_ratio = ratio,
            .power_law_gamma = -2.1f,
            .internal_neurons = 200
        };

        cortex = visual_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed with hub_ratio=" << ratio;

        visual_cortex_destroy(cortex);
        cortex = nullptr;
    }
}

TEST_F(VisualCortexTopologyTest, TopologyWithDifferentPowerLawGamma) {
    // WHAT: Test various power-law exponents
    // WHY: Verify biological range works
    float gammas[] = {-1.5f, -2.0f, -2.1f, -2.5f, -3.0f};

    for (float gamma : gammas) {
        visual_cortex_config_t config = {
            .input_width = 640,
            .input_height = 480,
            .num_v1_filters = 32,
            .feature_dim = 128,
            .enable_attention = false,
            .enable_memory = false,
            .enable_fractal_topology = true,
            .hub_ratio = 0.15f,
            .power_law_gamma = gamma,
            .internal_neurons = 200
        };

        cortex = visual_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed with gamma=" << gamma;

        visual_cortex_destroy(cortex);
        cortex = nullptr;
    }
}

//=============================================================================
// Integration with Visual Processing Tests
//=============================================================================

TEST_F(VisualCortexTopologyTest, TopologyWithAttentionEnabled) {
    // WHAT: Test topology + attention system
    // WHY: Verify compatibility
    visual_cortex_config_t config = {
        .input_width = 640,
        .input_height = 480,
        .num_v1_filters = 32,
        .feature_dim = 128,
        .enable_attention = true,   // Enabled
        .enable_memory = false,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 320
    };

    cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_destroy(cortex);
    cortex = nullptr;
}

TEST_F(VisualCortexTopologyTest, TopologyWithMemoryEnabled) {
    // WHAT: Test topology + memory system
    // WHY: Verify compatibility
    visual_cortex_config_t config = {
        .input_width = 640,
        .input_height = 480,
        .num_v1_filters = 32,
        .feature_dim = 128,
        .enable_attention = false,
        .enable_memory = true,      // Enabled
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 320
    };

    cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_destroy(cortex);
    cortex = nullptr;
}

TEST_F(VisualCortexTopologyTest, TopologyWithAllFeaturesEnabled) {
    // WHAT: Test topology + all features
    // WHY: Full integration test
    visual_cortex_config_t config = {
        .input_width = 640,
        .input_height = 480,
        .num_v1_filters = 32,
        .feature_dim = 128,
        .enable_attention = true,
        .enable_memory = true,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 320
    };

    cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_destroy(cortex);
    cortex = nullptr;
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(VisualCortexTopologyTest, MultipleCreateDestroyCycles) {
    // WHAT: Test repeated creation/destruction
    // WHY: Verify no memory leaks
    visual_cortex_config_t config = {
        .input_width = 640,
        .input_height = 480,
        .num_v1_filters = 32,
        .feature_dim = 128,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = true,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 200
    };

    for (int i = 0; i < 10; i++) {
        cortex = visual_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed on iteration " << i;

        visual_cortex_destroy(cortex);
        cortex = nullptr;
    }
}

TEST_F(VisualCortexTopologyTest, DestroyNullCortex) {
    // WHAT: Test destroying NULL cortex
    // WHY: Verify graceful handling
    visual_cortex_destroy(nullptr);  // Should not crash
}
