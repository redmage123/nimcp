/**
 * @file test_surface_manifold.cpp
 * @brief Unit Tests for Surface Manifold Mathematics
 *
 * Tests for manifold construction, metric tensor, and surface area computation.
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "core/geometry/nimcp_surface_manifold.h"
}

// Test tolerances
#define TOLERANCE 1e-5f

//=============================================================================
// Vector Utility Tests
//=============================================================================

class VectorUtilityTest : public ::testing::Test {};

TEST_F(VectorUtilityTest, Vec3Dot_Orthogonal) {
    surface_vec3_t a = {1.0f, 0.0f, 0.0f};
    surface_vec3_t b = {0.0f, 1.0f, 0.0f};
    float dot = surface_vec3_dot(&a, &b);
    EXPECT_NEAR(dot, 0.0f, TOLERANCE);
}

TEST_F(VectorUtilityTest, Vec3Dot_Parallel) {
    surface_vec3_t a = {1.0f, 0.0f, 0.0f};
    surface_vec3_t b = {2.0f, 0.0f, 0.0f};
    float dot = surface_vec3_dot(&a, &b);
    EXPECT_NEAR(dot, 2.0f, TOLERANCE);
}

TEST_F(VectorUtilityTest, Vec3Dot_NullA) {
    surface_vec3_t b = {1.0f, 0.0f, 0.0f};
    float dot = surface_vec3_dot(nullptr, &b);
    EXPECT_EQ(dot, 0.0f);
}

TEST_F(VectorUtilityTest, Vec3Cross_OrthogonalBasis) {
    surface_vec3_t x = {1.0f, 0.0f, 0.0f};
    surface_vec3_t y = {0.0f, 1.0f, 0.0f};
    surface_vec3_t result;
    surface_vec3_cross(&x, &y, &result);
    EXPECT_NEAR(result.x, 0.0f, TOLERANCE);
    EXPECT_NEAR(result.y, 0.0f, TOLERANCE);
    EXPECT_NEAR(result.z, 1.0f, TOLERANCE);
}

TEST_F(VectorUtilityTest, Vec3Magnitude_Unit) {
    surface_vec3_t v = {1.0f, 0.0f, 0.0f};
    float mag = surface_vec3_magnitude(&v);
    EXPECT_NEAR(mag, 1.0f, TOLERANCE);
}

TEST_F(VectorUtilityTest, Vec3Magnitude_General) {
    surface_vec3_t v = {3.0f, 4.0f, 0.0f};
    float mag = surface_vec3_magnitude(&v);
    EXPECT_NEAR(mag, 5.0f, TOLERANCE);
}

TEST_F(VectorUtilityTest, Vec3Normalize_Unit) {
    surface_vec3_t v = {3.0f, 0.0f, 0.0f};
    int ret = surface_vec3_normalize(&v);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(v.x, 1.0f, TOLERANCE);
    EXPECT_NEAR(surface_vec3_magnitude(&v), 1.0f, TOLERANCE);
}

TEST_F(VectorUtilityTest, Vec3Angle_Orthogonal) {
    surface_vec3_t a = {1.0f, 0.0f, 0.0f};
    surface_vec3_t b = {0.0f, 1.0f, 0.0f};
    float angle = surface_vec3_angle(&a, &b);
    EXPECT_NEAR(angle, M_PI / 2.0f, TOLERANCE);
}

TEST_F(VectorUtilityTest, Vec3Angle_Parallel) {
    surface_vec3_t a = {1.0f, 0.0f, 0.0f};
    surface_vec3_t b = {2.0f, 0.0f, 0.0f};
    float angle = surface_vec3_angle(&a, &b);
    EXPECT_NEAR(angle, 0.0f, TOLERANCE);
}

TEST_F(VectorUtilityTest, Vec3Distance_Origin) {
    surface_vec3_t a = {0.0f, 0.0f, 0.0f};
    surface_vec3_t b = {3.0f, 4.0f, 0.0f};
    float dist = surface_vec3_distance(&a, &b);
    EXPECT_NEAR(dist, 5.0f, TOLERANCE);
}

//=============================================================================
// Geometry Calculation Tests
//=============================================================================

class GeometryCalcTest : public ::testing::Test {};

TEST_F(GeometryCalcTest, ComputeChi_ValidParams) {
    float chi;
    int ret = surface_compute_chi(0.83f, 1.0f, &chi);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(chi, 0.83f, TOLERANCE);
}

TEST_F(GeometryCalcTest, ComputeChi_ZeroDistance) {
    float chi;
    int ret = surface_compute_chi(1.0f, 0.0f, &chi);
    EXPECT_EQ(ret, -1);  // Invalid
}

TEST_F(GeometryCalcTest, ComputeChi_NullOutput) {
    int ret = surface_compute_chi(1.0f, 1.0f, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(GeometryCalcTest, ComputeRho_ValidParams) {
    float rho;
    int ret = surface_compute_rho(0.5f, 1.0f, &rho);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(rho, 0.5f, TOLERANCE);
}

TEST_F(GeometryCalcTest, ComputeRho_ZeroParent) {
    float rho;
    int ret = surface_compute_rho(0.5f, 0.0f, &rho);
    EXPECT_EQ(ret, -1);
}

TEST_F(GeometryCalcTest, ComputeLambda_ValidParams) {
    float lambda;
    int ret = surface_compute_lambda(2.0f, 1.0f, &lambda);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(lambda, 2.0f, TOLERANCE);
}

//=============================================================================
// Manifold Creation Tests
//=============================================================================

class ManifoldCreationTest : public ::testing::Test {};

TEST_F(ManifoldCreationTest, Create_ValidCapacity) {
    surface_manifold_t* manifold = surface_manifold_create(16, 32);
    EXPECT_NE(manifold, nullptr);
    if (manifold) {
        EXPECT_EQ(manifold->capacity_charts, 16u);
        EXPECT_EQ(manifold->capacity_branch_points, 32u);
        EXPECT_EQ(manifold->num_charts, 0u);
        EXPECT_EQ(manifold->num_branch_points, 0u);
        surface_manifold_destroy(manifold);
    }
}

TEST_F(ManifoldCreationTest, Create_DefaultCapacity) {
    surface_manifold_t* manifold = surface_manifold_create(
        SURFACE_MAX_CHARTS, SURFACE_MAX_BRANCH_POINTS);
    EXPECT_NE(manifold, nullptr);
    if (manifold) {
        EXPECT_EQ(manifold->capacity_charts, (uint32_t)SURFACE_MAX_CHARTS);
        EXPECT_EQ(manifold->capacity_branch_points, (uint32_t)SURFACE_MAX_BRANCH_POINTS);
        surface_manifold_destroy(manifold);
    }
}

TEST_F(ManifoldCreationTest, Destroy_Null) {
    surface_manifold_destroy(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(ManifoldCreationTest, Reset_ValidManifold) {
    surface_manifold_t* manifold = surface_manifold_create(16, 32);
    ASSERT_NE(manifold, nullptr);

    manifold->num_charts = 5;  // Simulate added charts
    int ret = surface_manifold_reset(manifold);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(manifold->num_charts, 0u);

    surface_manifold_destroy(manifold);
}

TEST_F(ManifoldCreationTest, Reset_Null) {
    int ret = surface_manifold_reset(nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Chart Operations Tests
//=============================================================================

class ChartOperationsTest : public ::testing::Test {
protected:
    void SetUp() override {
        manifold = surface_manifold_create(16, 32);
        ASSERT_NE(manifold, nullptr);
    }

    void TearDown() override {
        if (manifold) {
            surface_manifold_destroy(manifold);
        }
    }

    surface_manifold_t* manifold;
};

TEST_F(ChartOperationsTest, AddChart_Valid) {
    uint32_t chart_id;
    int ret = surface_manifold_add_chart(manifold, 0, SURFACE_CHART_CYLINDRICAL, 0.1f, &chart_id);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(chart_id, 0u);
    EXPECT_EQ(manifold->num_charts, 1u);
}

TEST_F(ChartOperationsTest, AddChart_Multiple) {
    uint32_t id1, id2, id3;
    surface_manifold_add_chart(manifold, 0, SURFACE_CHART_CYLINDRICAL, 0.1f, &id1);
    surface_manifold_add_chart(manifold, 1, SURFACE_CHART_CONICAL, 0.2f, &id2);
    surface_manifold_add_chart(manifold, 2, SURFACE_CHART_PANTS, 0.15f, &id3);

    EXPECT_EQ(manifold->num_charts, 3u);
    EXPECT_EQ(id1, 0u);
    EXPECT_EQ(id2, 1u);
    EXPECT_EQ(id3, 2u);
}

TEST_F(ChartOperationsTest, AddChart_NullOutput) {
    int ret = surface_manifold_add_chart(manifold, 0, SURFACE_CHART_CYLINDRICAL, 0.1f, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(ChartOperationsTest, AddChart_NullManifold) {
    uint32_t chart_id;
    int ret = surface_manifold_add_chart(nullptr, 0, SURFACE_CHART_CYLINDRICAL, 0.1f, &chart_id);
    EXPECT_EQ(ret, -1);
}

TEST_F(ChartOperationsTest, ConnectCharts_Valid) {
    uint32_t id1, id2;
    surface_manifold_add_chart(manifold, 0, SURFACE_CHART_CYLINDRICAL, 0.1f, &id1);
    surface_manifold_add_chart(manifold, 1, SURFACE_CHART_CYLINDRICAL, 0.1f, &id2);

    int ret = surface_manifold_connect_charts(manifold, id1, id2, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(manifold->charts[id1].boundary_chart_ids[0], id2);
}

//=============================================================================
// Metric Tensor Tests
//=============================================================================

class MetricTensorTest : public ::testing::Test {};

TEST_F(MetricTensorTest, CylinderMetric_ValidRadius) {
    surface_cylinder_params_t params = {
        .radius = 0.5f,
        .length = 1.0f,
        .axis = {0.0f, 0.0f, 1.0f},
        .origin = {0.0f, 0.0f, 0.0f}
    };
    surface_sigma_t sigma = {0.5f, M_PI};
    surface_metric_tensor_t metric;

    int ret = surface_compute_metric_cylinder(&params, &sigma, &metric);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(metric.is_valid);
    // For cylinder: gamma = [[1, 0], [0, r^2]]
    EXPECT_NEAR(metric.gamma[0][0], 1.0f, TOLERANCE);
    EXPECT_NEAR(metric.gamma[0][1], 0.0f, TOLERANCE);
    EXPECT_NEAR(metric.gamma[1][0], 0.0f, TOLERANCE);
    EXPECT_NEAR(metric.gamma[1][1], 0.25f, TOLERANCE);  // r^2 = 0.25
    EXPECT_NEAR(metric.determinant, 0.25f, TOLERANCE);
}

TEST_F(MetricTensorTest, MetricDeterminant_Computed) {
    surface_metric_tensor_t metric = {};
    metric.gamma[0][0] = 2.0f;
    metric.gamma[0][1] = 0.0f;
    metric.gamma[1][0] = 0.0f;
    metric.gamma[1][1] = 3.0f;

    float det = surface_metric_determinant(&metric);
    EXPECT_NEAR(det, 6.0f, TOLERANCE);
}

TEST_F(MetricTensorTest, MetricInverse_Identity) {
    surface_metric_tensor_t metric = {};
    metric.gamma[0][0] = 1.0f;
    metric.gamma[0][1] = 0.0f;
    metric.gamma[1][0] = 0.0f;
    metric.gamma[1][1] = 1.0f;

    float inverse[2][2];
    int ret = surface_metric_inverse(&metric, inverse);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(inverse[0][0], 1.0f, TOLERANCE);
    EXPECT_NEAR(inverse[1][1], 1.0f, TOLERANCE);
}

//=============================================================================
// Surface Area Tests
//=============================================================================

class SurfaceAreaTest : public ::testing::Test {};

TEST_F(SurfaceAreaTest, CylinderArea_ValidParams) {
    surface_cylinder_params_t params = {
        .radius = 1.0f / (2.0f * M_PI),  // circumference = 1
        .length = 1.0f,
        .axis = {0.0f, 0.0f, 1.0f},
        .origin = {0.0f, 0.0f, 0.0f}
    };

    float area;
    int ret = surface_compute_cylinder_area(&params, &area);
    EXPECT_EQ(ret, 0);
    // Area = 2*pi*r*L = 2*pi * 1/(2*pi) * 1 = 1
    EXPECT_NEAR(area, 1.0f, TOLERANCE);
}

TEST_F(SurfaceAreaTest, ConeArea_ValidParams) {
    surface_cone_params_t params = {
        .radius_start = 1.0f,
        .radius_end = 1.0f,  // Actually a cylinder
        .length = 1.0f,
        .axis = {0.0f, 0.0f, 1.0f},
        .origin = {0.0f, 0.0f, 0.0f}
    };

    float area;
    int ret = surface_compute_cone_area(&params, &area);
    EXPECT_EQ(ret, 0);
    // When r1 = r2, cone is cylinder: area = 2*pi*r*L
    EXPECT_NEAR(area, 2.0f * M_PI, TOLERANCE);
}

//=============================================================================
// Nambu-Goto Action Tests
//=============================================================================

class NambuGotoTest : public ::testing::Test {
protected:
    void SetUp() override {
        manifold = surface_manifold_create(16, 32);
        ASSERT_NE(manifold, nullptr);
    }

    void TearDown() override {
        if (manifold) {
            surface_manifold_destroy(manifold);
        }
    }

    surface_manifold_t* manifold;
};

TEST_F(NambuGotoTest, Action_EmptyManifold) {
    manifold->total_surface_area = 0.0f;
    float action;
    int ret = surface_compute_nambu_goto_action(manifold, &action);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(action, 0.0f, TOLERANCE);
}

TEST_F(NambuGotoTest, Action_EqualsArea) {
    // Nambu-Goto action = T * surface_area, with T=1
    manifold->total_surface_area = 5.0f;
    float action;
    int ret = surface_compute_nambu_goto_action(manifold, &action);
    EXPECT_EQ(ret, 0);
    EXPECT_NEAR(action, 5.0f, TOLERANCE);
}

TEST_F(NambuGotoTest, Action_NullOutput) {
    int ret = surface_compute_nambu_goto_action(manifold, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Feynman Diagram Tests
//=============================================================================

class FeynmanDiagramTest : public ::testing::Test {
protected:
    void SetUp() override {
        manifold = surface_manifold_create(16, 32);
        ASSERT_NE(manifold, nullptr);
    }

    void TearDown() override {
        if (manifold) {
            surface_manifold_destroy(manifold);
        }
    }

    surface_manifold_t* manifold;
};

TEST_F(FeynmanDiagramTest, BranchToVertex_Valid) {
    surface_branch_point_t branch = {};
    branch.id = 42;
    branch.degree = 3;
    branch.link_ids[0] = 1;
    branch.link_ids[1] = 2;
    branch.link_ids[2] = 3;

    surface_feynman_vertex_t vertex;
    int ret = surface_map_branch_to_feynman(&branch, &vertex);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(vertex.id, 42u);
    EXPECT_EQ(vertex.valence, 3u);
    EXPECT_EQ(vertex.leg_ids[0], 1u);
}

TEST_F(FeynmanDiagramTest, ConstructDiagram_Empty) {
    surface_feynman_diagram_t diagram;
    int ret = surface_construct_feynman_diagram(manifold, &diagram);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(diagram.num_vertices, 0u);
}

TEST_F(FeynmanDiagramTest, DestroyDiagram_Null) {
    surface_feynman_diagram_destroy(nullptr);
    SUCCEED();  // Should not crash
}

//=============================================================================
// Solid Angle Tests
//=============================================================================

class SolidAngleTest : public ::testing::Test {};

TEST_F(SolidAngleTest, ComputeSolidAngle_TwoDirections) {
    surface_vec3_t directions[2] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };

    float solid_angle;
    int ret = surface_compute_solid_angle(directions, 2, &solid_angle);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(solid_angle, 0.0f);
}

TEST_F(SolidAngleTest, ComputeSolidAngle_SingleDirection) {
    surface_vec3_t directions[1] = {{1.0f, 0.0f, 0.0f}};

    float solid_angle;
    int ret = surface_compute_solid_angle(directions, 1, &solid_angle);
    EXPECT_EQ(ret, 0);
}

TEST_F(SolidAngleTest, ComputeSolidAngle_NullOutput) {
    surface_vec3_t directions[2] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    int ret = surface_compute_solid_angle(directions, 2, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Gaussian Quadrature Tests
//=============================================================================

class QuadratureTest : public ::testing::Test {};

TEST_F(QuadratureTest, GaussInit_Valid) {
    surface_gauss_quadrature_t quad;
    int ret = surface_gauss_init(&quad, 4);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(quad.num_points, 0u);
}

TEST_F(QuadratureTest, GaussInit_Null) {
    int ret = surface_gauss_init(nullptr, 4);
    EXPECT_EQ(ret, -1);
}
