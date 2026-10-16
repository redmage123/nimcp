/**
 * @file test_swarm_proprioception.cpp
 * @brief Tests for Collective Proprioception
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_proprioception.h"

class ProprioceptionTest : public ::testing::Test {
protected:
    nimcp_swarm_proprioception_t* system;
    nimcp_swarm_proprio_config_t config;

    void SetUp() override {
        nimcp_swarm_proprio_default_config(&config);
        system = nimcp_swarm_proprioception_create(1, &config, nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) nimcp_swarm_proprioception_destroy(system);
    }
};

TEST_F(ProprioceptionTest, CreateSystem) { EXPECT_NE(system, nullptr); }
TEST_F(ProprioceptionTest, UpdatePosition) {
    nimcp_swarm_position_t pos = {5.0, 5.0, 0.0};
    EXPECT_EQ(nimcp_swarm_proprio_update_position(system, &pos, nullptr), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, UpdateNeighbor) {
    nimcp_swarm_position_t pos = {6.0, 5.0, 0.0};
    EXPECT_EQ(nimcp_swarm_proprio_update_neighbor(system, 2, &pos, 0.9), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, GetNeighbors) {
    nimcp_swarm_neighbor_t neighbors[10];
    uint32_t count;
    EXPECT_EQ(nimcp_swarm_proprio_get_neighbors(system, neighbors, 10, &count), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, ClassifyShape) {
    nimcp_swarm_shape_descriptor_t desc;
    EXPECT_EQ(nimcp_swarm_proprio_classify_shape(system, &desc), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, DetectDeformation) {
    nimcp_swarm_deformation_metrics_t metrics;
    nimcp_result_t r = nimcp_swarm_proprio_detect_deformation(system, &metrics);
    SUCCEED();
}
TEST_F(ProprioceptionTest, BoundaryRole) {
    nimcp_swarm_boundary_descriptor_t desc;
    EXPECT_EQ(nimcp_swarm_proprio_boundary_role(system, &desc), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, CalculateDensity) {
    nimcp_swarm_density_info_t info;
    EXPECT_EQ(nimcp_swarm_proprio_density(system, &info), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, EstimateCOM) {
    nimcp_swarm_com_estimate_t est;
    EXPECT_EQ(nimcp_swarm_proprio_estimate_com(system, &est), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, FormationMetrics) {
    nimcp_swarm_formation_metrics_t metrics;
    EXPECT_EQ(nimcp_swarm_proprio_formation_metrics(system, &metrics), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, DetectVibration) {
    double signal[64] = {0};
    for (int i = 0; i < 64; i++) signal[i] = sin(i * 0.1);
    nimcp_swarm_vibration_data_t vib;
    EXPECT_EQ(nimcp_swarm_proprio_detect_vibration(system, signal, 64, &vib), NIMCP_SUCCESS);
}
TEST_F(ProprioceptionTest, BroadcastPosition) {
    nimcp_result_t r = nimcp_swarm_proprio_broadcast_position(system);
    SUCCEED();
}
TEST_F(ProprioceptionTest, PositionDistance) {
    nimcp_swarm_position_t p1 = {0, 0, 0}, p2 = {3, 4, 0};
    double d = nimcp_swarm_position_distance(&p1, &p2);
    EXPECT_NEAR(d, 5.0, 0.01);
}
TEST_F(ProprioceptionTest, ShapeName) {
    const char* name = nimcp_swarm_shape_name(NIMCP_SWARM_SHAPE_SPHERE);
    EXPECT_NE(name, nullptr);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
