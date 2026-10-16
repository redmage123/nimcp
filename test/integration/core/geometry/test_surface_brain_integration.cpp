/**
 * @file test_surface_brain_integration.cpp
 * @brief Integration Tests for Surface Geometry Brain Integration
 *
 * WHAT: Tests for surface geometry integration with brain structures
 * WHY:  Surface optimization governs dendrite/axon branching geometry
 * HOW:  GTest-based integration tests for brain bridge
 *
 * NIMCP STANDARDS:
 * - Integration tests verify brain system communication
 * - Tests spine/axon geometry computation through bridge
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/geometry/nimcp_surface_geometry.h"
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "core/brain/bridges/nimcp_surface_geometry_bridge.h"
}

// Test tolerance
#define TOLERANCE 1e-5f

//=============================================================================
// Test Fixture
//=============================================================================

class SurfaceBrainIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create geometry context
        surface_geometry_default_config(&geo_config);
        geo_ctx = surface_geometry_create(&geo_config);
        ASSERT_NE(geo_ctx, nullptr);

        // Create brain bridge
        surface_geometry_bridge_default_config(&bridge_config);
        bridge = surface_geometry_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        // Connect bridge to geometry
        int ret = surface_geometry_bridge_connect_geometry(bridge, geo_ctx);
        ASSERT_EQ(ret, 0);
    }

    void TearDown() override {
        if (bridge) {
            surface_geometry_bridge_disconnect_geometry(bridge);
            surface_geometry_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (geo_ctx) {
            surface_geometry_destroy(geo_ctx);
            geo_ctx = nullptr;
        }
    }

    surface_geometry_config_t geo_config;
    surface_geometry_ctx_t* geo_ctx;
    surface_geometry_bridge_config_t bridge_config;
    surface_geometry_bridge_t* bridge;
};

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(SurfaceBrainIntegrationTest, BridgeConnectsToGeometry) {
    // Bridge requires BOTH geometry and brain region to be "connected"
    // After SetUp, only geometry is connected, so is_connected returns false
    // Connect a mock brain region to make bridge fully connected
    int dummy_brain = 42;  // Mock brain region pointer
    int ret = surface_geometry_bridge_connect_brain(bridge, &dummy_brain);
    EXPECT_EQ(ret, 0);

    bool connected = surface_geometry_bridge_is_connected(bridge);
    EXPECT_TRUE(connected);
}

TEST_F(SurfaceBrainIntegrationTest, DisconnectAndReconnect) {
    // First connect brain to make bridge active
    int dummy_brain = 42;
    surface_geometry_bridge_connect_brain(bridge, &dummy_brain);
    EXPECT_TRUE(surface_geometry_bridge_is_connected(bridge));

    // Disconnect geometry
    surface_geometry_bridge_disconnect_geometry(bridge);
    bool connected = surface_geometry_bridge_is_connected(bridge);
    EXPECT_FALSE(connected);  // Bridge inactive after geometry disconnect

    // Reconnect geometry
    int ret = surface_geometry_bridge_connect_geometry(bridge, geo_ctx);
    EXPECT_EQ(ret, 0);
    connected = surface_geometry_bridge_is_connected(bridge);
    EXPECT_TRUE(connected);  // Bridge active again
}

TEST_F(SurfaceBrainIntegrationTest, ConnectNull) {
    surface_geometry_bridge_t* new_bridge = surface_geometry_bridge_create(nullptr);
    ASSERT_NE(new_bridge, nullptr);

    int ret = surface_geometry_bridge_connect_geometry(new_bridge, nullptr);
    EXPECT_NE(ret, 0);  // Should return error code (NIMCP_ERROR_NULL_ARG or similar)

    surface_geometry_bridge_destroy(new_bridge);
}

//=============================================================================
// Spine Geometry Tests
//=============================================================================

TEST_F(SurfaceBrainIntegrationTest, ComputeSpineGeometry_Sprout) {
    // Thin spine on thick dendrite -> should be sprout
    float parent_diam = 2.0f;
    float spine_diam = 0.4f;  // rho = 0.2 < 0.6
    surface_vec3_t pos = {1.0f, 0.0f, 0.0f};

    spine_surface_geometry_t result = {};
    int ret = surface_geometry_bridge_compute_spine(
        bridge, parent_diam, spine_diam, &pos, &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_sprout);
    EXPECT_NEAR(result.optimal_angle, M_PI / 2.0f, 0.2f);
}

TEST_F(SurfaceBrainIntegrationTest, ComputeSpineGeometry_Branch) {
    // Thick spine -> should be branch, not sprout
    float parent_diam = 2.0f;
    float spine_diam = 1.5f;  // rho = 0.75 > 0.6
    surface_vec3_t pos = {1.0f, 0.0f, 0.0f};

    spine_surface_geometry_t result = {};
    int ret = surface_geometry_bridge_compute_spine(
        bridge, parent_diam, spine_diam, &pos, &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(result.is_sprout);
}

TEST_F(SurfaceBrainIntegrationTest, ComputeSpineGeometry_NullParams) {
    // Null position is handled gracefully (uses default)
    spine_surface_geometry_t result = {};
    int ret = surface_geometry_bridge_compute_spine(bridge, 2.0f, 0.4f, nullptr, &result);
    EXPECT_EQ(ret, 0);  // Null position is OK

    surface_vec3_t pos = {0, 0, 0};
    ret = surface_geometry_bridge_compute_spine(bridge, 2.0f, 0.4f, &pos, nullptr);
    EXPECT_NE(ret, 0);  // Null result should return error
}

//=============================================================================
// Axon Branch Geometry Tests
//=============================================================================

TEST_F(SurfaceBrainIntegrationTest, ComputeAxonBranch_Bifurcation) {
    float parent_diam = 2.0f;
    float child_diams[] = {1.2f, 1.0f};

    axon_branch_surface_geometry_t result = {};
    int ret = surface_geometry_bridge_compute_axon_branch(
        bridge, parent_diam, child_diams, 2, &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.degree, 3u);  // parent + 2 children = bifurcation
}

TEST_F(SurfaceBrainIntegrationTest, ComputeAxonBranch_Trifurcation) {
    float parent_diam = 2.0f;
    float child_diams[] = {1.0f, 0.9f, 0.8f};

    axon_branch_surface_geometry_t result = {};
    int ret = surface_geometry_bridge_compute_axon_branch(
        bridge, parent_diam, child_diams, 3, &result
    );
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.degree, 4u);  // parent + 3 children = trifurcation
}

TEST_F(SurfaceBrainIntegrationTest, ComputeAxonBranch_NullParams) {
    axon_branch_surface_geometry_t result = {};
    int ret = surface_geometry_bridge_compute_axon_branch(bridge, 2.0f, nullptr, 2, &result);
    EXPECT_NE(ret, 0);  // Should return error code

    float child_diams[] = {1.0f, 1.0f};
    ret = surface_geometry_bridge_compute_axon_branch(bridge, 2.0f, child_diams, 2, nullptr);
    EXPECT_NE(ret, 0);  // Should return error code
}

//=============================================================================
// Branch Computation Tests
//=============================================================================

TEST_F(SurfaceBrainIntegrationTest, ComputeBranch_ValidParams) {
    surface_branch_point_t branch = {};
    branch.degree = 3;
    branch.link_diameters[0] = 2.0f;
    branch.link_diameters[1] = 1.5f;
    branch.link_diameters[2] = 1.5f;

    surface_geometry_params_t result = {};
    int ret = surface_geometry_bridge_compute_branch(bridge, &branch, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(result.chi, 0.0f);
}

TEST_F(SurfaceBrainIntegrationTest, ComputeBranch_NullBridge) {
    surface_branch_point_t branch = {};
    surface_geometry_params_t result = {};
    int ret = surface_geometry_bridge_compute_branch(nullptr, &branch, &result);
    EXPECT_NE(ret, 0);  // Should return error code
}

//=============================================================================
// Optimization Tests
//=============================================================================

TEST_F(SurfaceBrainIntegrationTest, OptimizeNetworkViaBridge) {
    float terminals[3][3] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.5f, 0.866f, 0.0f}
    };

    surface_optimization_result_t result = {};
    int ret = surface_geometry_bridge_optimize(bridge, terminals, 3, 0.1f, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(result.surface_area, 0.0f);

    surface_optimization_result_free(&result);
}

TEST_F(SurfaceBrainIntegrationTest, OptimizeNetworkViaBridge_NullResult) {
    float terminals[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    int ret = surface_geometry_bridge_optimize(bridge, terminals, 3, 0.1f, nullptr);
    EXPECT_NE(ret, 0);  // Should return error code
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(SurfaceBrainIntegrationTest, ValidateGeometryViaBridge) {
    surface_geometry_params_t params = {};
    params.chi = 0.5f;
    params.rho = 0.5f;
    params.rho_threshold = 0.6f;
    params.steering_angle = static_cast<float>(M_PI / 2.0);

    surface_validation_result_t result = {};
    int ret = surface_geometry_bridge_validate(bridge, &params, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.is_valid);
}

TEST_F(SurfaceBrainIntegrationTest, ValidateGeometryViaBridge_NullParams) {
    surface_validation_result_t result = {};
    int ret = surface_geometry_bridge_validate(bridge, nullptr, &result);
    EXPECT_NE(ret, 0);  // Should return error code
}

//=============================================================================
// Broadcast Tests
//=============================================================================

TEST_F(SurfaceBrainIntegrationTest, BroadcastUpdate) {
    surface_geometry_params_t params = {};
    params.chi = 0.5f;
    params.rho = 0.3f;
    params.regime = SURFACE_REGIME_BRANCHING;
    int ret = surface_geometry_bridge_broadcast_update(bridge, &params);
    EXPECT_EQ(ret, 0);
}

TEST_F(SurfaceBrainIntegrationTest, BroadcastBranchFormed) {
    float position[3] = {1.0f, 2.0f, 3.0f};
    int ret = surface_geometry_bridge_broadcast_branch(
        bridge, SURFACE_BRANCH_BIFURCATION, position
    );
    EXPECT_EQ(ret, 0);
}

TEST_F(SurfaceBrainIntegrationTest, BroadcastAnomaly) {
    surface_branch_point_t branch = {};
    branch.id = 1;
    branch.degree = 3;
    int ret = surface_geometry_bridge_broadcast_anomaly(
        bridge, SURFACE_ERROR_CONSTRAINT_VIOLATION, &branch
    );
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SurfaceBrainIntegrationTest, TrackBridgeStats) {
    // Perform various operations
    float parent_diam = 2.0f;
    float spine_diam = 0.4f;
    surface_vec3_t pos = {0, 0, 0};
    spine_surface_geometry_t spine_result = {};
    surface_geometry_bridge_compute_spine(bridge, parent_diam, spine_diam, &pos, &spine_result);

    float child_diams[] = {1.0f, 1.0f};
    axon_branch_surface_geometry_t axon_result = {};
    surface_geometry_bridge_compute_axon_branch(bridge, 2.0f, child_diams, 2, &axon_result);

    surface_geometry_bridge_stats_t stats = {};
    int ret = surface_geometry_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    // Spine and axon computations both count toward total
    EXPECT_GE(stats.total_geometry_computations, 2u);
}

TEST_F(SurfaceBrainIntegrationTest, ResetBridgeStats) {
    // Generate some stats
    spine_surface_geometry_t result = {};
    surface_vec3_t pos = {0, 0, 0};
    surface_geometry_bridge_compute_spine(bridge, 2.0f, 0.4f, &pos, &result);

    // Reset
    int ret = surface_geometry_bridge_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    // Verify
    surface_geometry_bridge_stats_t stats = {};
    surface_geometry_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_geometry_computations, 0u);
}

TEST_F(SurfaceBrainIntegrationTest, GetBridgeConfig) {
    surface_geometry_bridge_config_t config = {};
    int ret = surface_geometry_bridge_get_config(bridge, &config);
    EXPECT_EQ(ret, 0);
}

TEST_F(SurfaceBrainIntegrationTest, SetBridgeConfig) {
    surface_geometry_bridge_config_t config = {};
    surface_geometry_bridge_default_config(&config);
    config.max_optimization_iterations = 500;

    int ret = surface_geometry_bridge_set_config(bridge, &config);
    EXPECT_EQ(ret, 0);
}

//=============================================================================
// Cross-Bridge Communication Tests
//=============================================================================

class CrossBridgeIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        geo_ctx = surface_geometry_create(nullptr);
        ASSERT_NE(geo_ctx, nullptr);

        brain_bridge = surface_geometry_bridge_create(nullptr);
        ASSERT_NE(brain_bridge, nullptr);

        surface_geometry_bridge_connect_geometry(brain_bridge, geo_ctx);
    }

    void TearDown() override {
        if (brain_bridge) {
            surface_geometry_bridge_disconnect_geometry(brain_bridge);
            surface_geometry_bridge_destroy(brain_bridge);
        }
        if (geo_ctx) surface_geometry_destroy(geo_ctx);
    }

    surface_geometry_ctx_t* geo_ctx;
    surface_geometry_bridge_t* brain_bridge;
};

TEST_F(CrossBridgeIntegrationTest, SpineGeometryUsesBackend) {
    // Compute spine geometry through bridge
    float parent_diam = 2.0f;
    float spine_diam = 0.4f;
    surface_vec3_t pos = {1.0f, 0.0f, 0.0f};

    spine_surface_geometry_t bridge_result = {};
    int ret1 = surface_geometry_bridge_compute_spine(
        brain_bridge, parent_diam, spine_diam, &pos, &bridge_result
    );

    // Compute directly through geometry context
    spine_surface_geometry_t direct_result = {};
    int ret2 = surface_compute_spine_geometry(
        geo_ctx, parent_diam, spine_diam, &pos, &direct_result
    );

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);

    // Results should be consistent
    EXPECT_EQ(bridge_result.is_sprout, direct_result.is_sprout);
    EXPECT_NEAR(bridge_result.optimal_angle, direct_result.optimal_angle, TOLERANCE);
}

TEST_F(CrossBridgeIntegrationTest, AxonBranchConsistency) {
    float parent_diam = 2.0f;
    float child_diams[] = {1.0f, 1.0f};

    axon_branch_surface_geometry_t bridge_result = {};
    int ret1 = surface_geometry_bridge_compute_axon_branch(
        brain_bridge, parent_diam, child_diams, 2, &bridge_result
    );

    axon_branch_surface_geometry_t direct_result = {};
    int ret2 = surface_compute_axon_branch_geometry(
        geo_ctx, parent_diam, child_diams, 2, &direct_result
    );

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);

    // Results should be consistent
    EXPECT_EQ(bridge_result.degree, direct_result.degree);
}

TEST_F(CrossBridgeIntegrationTest, ValidationConsistency) {
    surface_geometry_params_t params = {};
    params.chi = 0.5f;
    params.rho = 0.5f;
    params.rho_threshold = 0.6f;
    params.steering_angle = static_cast<float>(M_PI / 2.0);

    surface_validation_result_t bridge_result = {};
    int ret1 = surface_geometry_bridge_validate(brain_bridge, &params, &bridge_result);

    surface_validation_result_t direct_result = {};
    int ret2 = surface_validate_geometry(geo_ctx, &params, &direct_result);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);

    EXPECT_EQ(bridge_result.is_valid, direct_result.is_valid);
}
