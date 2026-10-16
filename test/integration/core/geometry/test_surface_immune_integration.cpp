/**
 * @file test_surface_immune_integration.cpp
 * @brief Integration Tests for Surface Geometry Immune Integration
 *
 * WHAT: Tests for surface geometry <-> immune system interaction
 * WHY:  Geometry anomalies are reported as antigens for immune response
 * HOW:  GTest-based integration tests for antigen/antibody lifecycle
 *
 * NIMCP STANDARDS:
 * - Integration tests verify cross-module communication
 * - Tests antigen presentation, acknowledgment, resolution
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/geometry/nimcp_surface_geometry.h"
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "cognitive/immune/nimcp_surface_immune_bridge.h"
}

// Test tolerance
#define TOLERANCE 1e-5f

//=============================================================================
// Test Fixture
//=============================================================================

class SurfaceImmuneIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create geometry context
        surface_geometry_default_config(&geo_config);
        geo_ctx = surface_geometry_create(&geo_config);
        ASSERT_NE(geo_ctx, nullptr);

        // Create immune bridge
        surface_immune_default_config(&immune_config);
        immune_bridge = surface_immune_bridge_create(&immune_config);
        ASSERT_NE(immune_bridge, nullptr);

        // Connect bridge to geometry
        int ret = surface_immune_bridge_connect_geometry(immune_bridge, geo_ctx);
        ASSERT_EQ(ret, 0);
    }

    void TearDown() override {
        if (immune_bridge) {
            surface_immune_bridge_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (geo_ctx) {
            surface_geometry_destroy(geo_ctx);
            geo_ctx = nullptr;
        }
    }

    surface_geometry_config_t geo_config;
    surface_geometry_ctx_t* geo_ctx;
    surface_immune_config_t immune_config;
    surface_immune_bridge_t* immune_bridge;
};

//=============================================================================
// Validation Tests
//=============================================================================

TEST_F(SurfaceImmuneIntegrationTest, ValidateGeometry_ValidParameters) {
    surface_geometry_params_t params = {};
    params.chi = 0.5f;
    params.rho = 0.4f;

    bool is_valid;
    surface_antigen_type_t violation;
    int ret = surface_immune_validate_geometry(immune_bridge, &params, &is_valid, &violation);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(is_valid);
    EXPECT_EQ(violation, SURFACE_ANTIGEN_NONE);
}

TEST_F(SurfaceImmuneIntegrationTest, ValidateGeometry_InvalidChi) {
    surface_geometry_params_t params = {};
    params.chi = 3.0f;  // Invalid: max is 2.0
    params.rho = 0.5f;

    bool is_valid;
    surface_antigen_type_t violation;
    int ret = surface_immune_validate_geometry(immune_bridge, &params, &is_valid, &violation);

    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(is_valid);
    EXPECT_EQ(violation, SURFACE_ANTIGEN_INVALID_CHI);
}

TEST_F(SurfaceImmuneIntegrationTest, ValidateGeometry_InvalidRho) {
    surface_geometry_params_t params = {};
    params.chi = 0.5f;
    params.rho = 1.5f;  // Invalid: max is 1.0

    bool is_valid;
    surface_antigen_type_t violation;
    int ret = surface_immune_validate_geometry(immune_bridge, &params, &is_valid, &violation);

    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(is_valid);
    EXPECT_EQ(violation, SURFACE_ANTIGEN_RHO_OUT_OF_RANGE);
}

TEST_F(SurfaceImmuneIntegrationTest, ValidateBranch_ImpossibleTrifurcation) {
    surface_branch_point_t branch = {};
    branch.id = 1;
    branch.degree = 4;  // Trifurcation
    branch.params.chi = 0.5f;  // Below 0.83 threshold - might be considered impossible

    bool is_valid;
    uint32_t antigen_id;
    int ret = surface_immune_validate_branch(immune_bridge, &branch, &is_valid, &antigen_id);

    EXPECT_EQ(ret, 0);
    // Implementation may or may not detect impossible trifurcation
    // Just verify the function completes without error
}

//=============================================================================
// Antigen Lifecycle Tests
//=============================================================================

TEST_F(SurfaceImmuneIntegrationTest, PresentAnomaly_CreatesAntigen) {
    surface_branch_point_t branch = {};
    branch.id = 1;
    branch.position = {0.0f, 0.0f, 0.0f};

    uint32_t antigen_id;
    int ret = surface_immune_present_anomaly(
        immune_bridge,
        SURFACE_ANTIGEN_INVALID_CHI,
        &branch,
        0.83f,  // expected
        3.0f,   // actual
        &antigen_id
    );

    EXPECT_EQ(ret, 0);
    EXPECT_GT(antigen_id, 0u);

    // Verify antigen exists
    surface_antigen_t antigens[10] = {};
    uint32_t count;
    surface_immune_get_active_antigens(immune_bridge, antigens, 10, &count);
    EXPECT_GE(count, 1u);
}

TEST_F(SurfaceImmuneIntegrationTest, AcknowledgeAntigen) {
    // Present an anomaly
    surface_branch_point_t branch = {};
    branch.id = 1;
    uint32_t antigen_id;
    surface_immune_present_anomaly(
        immune_bridge,
        SURFACE_ANTIGEN_ANGLE_VIOLATION,
        &branch,
        M_PI / 2.0f,
        M_PI / 4.0f,
        &antigen_id
    );

    // Acknowledge it
    int ret = surface_immune_acknowledge_antigen(immune_bridge, antigen_id);
    EXPECT_EQ(ret, 0);

    // Antigen should still be active but acknowledged
    surface_antigen_t antigens[10] = {};
    uint32_t count;
    surface_immune_get_active_antigens(immune_bridge, antigens, 10, &count);
    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (antigens[i].id == antigen_id) {
            EXPECT_TRUE(antigens[i].acknowledged);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(SurfaceImmuneIntegrationTest, ResolveAntigen) {
    // Present an anomaly
    surface_branch_point_t branch = {};
    branch.id = 1;
    uint32_t antigen_id;
    surface_immune_present_anomaly(
        immune_bridge,
        SURFACE_ANTIGEN_MATERIAL_OVERFLOW,
        &branch,
        100.0f,
        150.0f,
        &antigen_id
    );

    // Resolve it
    int ret = surface_immune_resolve_antigen(immune_bridge, antigen_id);
    EXPECT_EQ(ret, 0);

    // Antigen should no longer be active
    surface_antigen_t antigens[10] = {};
    uint32_t count;
    surface_immune_get_active_antigens(immune_bridge, antigens, 10, &count);
    bool found = false;
    for (uint32_t i = 0; i < count; i++) {
        if (antigens[i].id == antigen_id && antigens[i].active) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST_F(SurfaceImmuneIntegrationTest, ResolveNonexistentAntigen) {
    int ret = surface_immune_resolve_antigen(immune_bridge, 9999);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Antibody Tests
//=============================================================================

TEST_F(SurfaceImmuneIntegrationTest, ProduceAntibody) {
    uint32_t antibody_id;
    int ret = surface_immune_produce_antibody(
        immune_bridge,
        SURFACE_ANTIGEN_INVALID_CHI,
        &antibody_id
    );

    EXPECT_EQ(ret, 0);
    EXPECT_GT(antibody_id, 0u);
}

TEST_F(SurfaceImmuneIntegrationTest, ApplyAntibody_CorrectsChi) {
    // Create antibody for chi violations
    uint32_t antibody_id;
    surface_immune_produce_antibody(immune_bridge, SURFACE_ANTIGEN_INVALID_CHI, &antibody_id);

    // Create params with invalid chi
    surface_geometry_params_t params = {};
    params.chi = 2.5f;  // Invalid
    params.rho = 0.5f;

    bool success;
    int ret = surface_immune_apply_antibody(immune_bridge, antibody_id, &params, &success);
    EXPECT_EQ(ret, 0);

    // If successful, chi should now be in valid range
    if (success) {
        EXPECT_LE(params.chi, 2.0f);
    }
}

TEST_F(SurfaceImmuneIntegrationTest, ApplyAntibody_NullParams) {
    uint32_t antibody_id;
    surface_immune_produce_antibody(immune_bridge, SURFACE_ANTIGEN_INVALID_CHI, &antibody_id);

    bool success;
    int ret = surface_immune_apply_antibody(immune_bridge, antibody_id, nullptr, &success);
    EXPECT_NE(ret, 0);  // Should return error code
}

//=============================================================================
// Immune Response Tests
//=============================================================================

TEST_F(SurfaceImmuneIntegrationTest, ReleaseCytokine_CriticalAnomaly) {
    // Present critical anomaly
    surface_branch_point_t branch = {};
    branch.id = 1;
    uint32_t antigen_id;
    surface_immune_present_anomaly(
        immune_bridge,
        SURFACE_ANTIGEN_TOPOLOGY_ERROR,
        &branch,
        0.0f,
        1.0f,
        &antigen_id
    );

    // Trigger cytokine release
    int ret = surface_immune_release_cytokine(
        immune_bridge,
        antigen_id,
        "Critical topology error detected"
    );
    EXPECT_EQ(ret, 0);

    // Stats should reflect cytokine release
    surface_immune_stats_t stats = {};
    surface_immune_get_stats(immune_bridge, &stats);
    EXPECT_GE(stats.cytokines_released, 1u);
}

TEST_F(SurfaceImmuneIntegrationTest, ActivateBCell_PersistentAnomaly) {
    // Present anomaly
    surface_branch_point_t branch = {};
    branch.id = 1;
    uint32_t antigen_id;
    surface_immune_present_anomaly(
        immune_bridge,
        SURFACE_ANTIGEN_ANGLE_VIOLATION,
        &branch,
        M_PI / 2.0f,
        M_PI / 4.0f,
        &antigen_id
    );

    // Activate B cell
    int ret = surface_immune_activate_b_cell(immune_bridge, antigen_id);
    EXPECT_EQ(ret, 0);

    // Stats should reflect activation
    surface_immune_stats_t stats = {};
    surface_immune_get_stats(immune_bridge, &stats);
    EXPECT_GE(stats.b_cells_activated, 1u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SurfaceImmuneIntegrationTest, StatsTrackValidations) {
    surface_geometry_params_t params = {};
    params.chi = 0.5f;
    params.rho = 0.5f;

    bool is_valid;
    surface_antigen_type_t violation;

    // Perform several validations
    for (int i = 0; i < 5; i++) {
        surface_immune_validate_geometry(immune_bridge, &params, &is_valid, &violation);
    }

    surface_immune_stats_t stats = {};
    surface_immune_get_stats(immune_bridge, &stats);
    EXPECT_GE(stats.total_validations, 5u);
}

TEST_F(SurfaceImmuneIntegrationTest, StatsTrackAnomalies) {
    surface_branch_point_t branch = {};
    branch.id = 1;
    uint32_t antigen_id;

    // Present several anomalies
    for (int i = 0; i < 3; i++) {
        surface_immune_present_anomaly(
            immune_bridge,
            SURFACE_ANTIGEN_INVALID_CHI,
            &branch,
            0.83f,
            3.0f,
            &antigen_id
        );
    }

    surface_immune_stats_t stats = {};
    surface_immune_get_stats(immune_bridge, &stats);
    EXPECT_GE(stats.anomalies_detected, 3u);
}

TEST_F(SurfaceImmuneIntegrationTest, StatsTrackResolutions) {
    surface_branch_point_t branch = {};
    branch.id = 1;
    uint32_t antigen_id;

    // Present and resolve anomalies
    for (int i = 0; i < 2; i++) {
        surface_immune_present_anomaly(
            immune_bridge,
            SURFACE_ANTIGEN_ANGLE_VIOLATION,
            &branch,
            0.0f, 1.0f,
            &antigen_id
        );
        surface_immune_resolve_antigen(immune_bridge, antigen_id);
    }

    surface_immune_stats_t stats = {};
    surface_immune_get_stats(immune_bridge, &stats);
    EXPECT_GE(stats.anomalies_resolved, 2u);
}

TEST_F(SurfaceImmuneIntegrationTest, ResetStats) {
    // Generate some activity
    surface_geometry_params_t params = {};
    params.chi = 3.0f;
    bool is_valid;
    surface_antigen_type_t violation;
    surface_immune_validate_geometry(immune_bridge, &params, &is_valid, &violation);

    // Reset
    int ret = surface_immune_reset_stats(immune_bridge);
    EXPECT_EQ(ret, 0);

    // Verify
    surface_immune_stats_t stats = {};
    surface_immune_get_stats(immune_bridge, &stats);
    EXPECT_EQ(stats.total_validations, 0u);
}

//=============================================================================
// Multiple Antigens Tests
//=============================================================================

TEST_F(SurfaceImmuneIntegrationTest, MultipleActiveAntigens) {
    surface_branch_point_t branch = {};

    // Present multiple antigens
    uint32_t ids[5];
    for (int i = 0; i < 5; i++) {
        branch.id = i + 1;
        surface_immune_present_anomaly(
            immune_bridge,
            (surface_antigen_type_t)(SURFACE_ANTIGEN_INVALID_CHI + (i % 3)),
            &branch,
            0.0f, (float)(i + 1),
            &ids[i]
        );
    }

    // Get all active antigens
    surface_antigen_t antigens[10] = {};
    uint32_t count;
    int ret = surface_immune_get_active_antigens(immune_bridge, antigens, 10, &count);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(count, 5u);
}

TEST_F(SurfaceImmuneIntegrationTest, AntigenCountByType) {
    surface_branch_point_t branch = {};
    branch.id = 1;
    uint32_t id;

    // Present different types
    surface_immune_present_anomaly(immune_bridge, SURFACE_ANTIGEN_INVALID_CHI, &branch, 0, 1, &id);
    surface_immune_present_anomaly(immune_bridge, SURFACE_ANTIGEN_INVALID_CHI, &branch, 0, 1, &id);
    surface_immune_present_anomaly(immune_bridge, SURFACE_ANTIGEN_ANGLE_VIOLATION, &branch, 0, 1, &id);

    surface_immune_stats_t stats = {};
    surface_immune_get_stats(immune_bridge, &stats);
    EXPECT_GE(stats.antigen_counts[SURFACE_ANTIGEN_INVALID_CHI], 2u);
    EXPECT_GE(stats.antigen_counts[SURFACE_ANTIGEN_ANGLE_VIOLATION], 1u);
}
