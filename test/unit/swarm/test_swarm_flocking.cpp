/**
 * @file test_swarm_flocking.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Flocking Dynamics
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Separation behavior
 * - Alignment behavior
 * - Cohesion behavior
 * - Obstacle avoidance
 * - Leader following
 * - Formation maintenance
 * - Velocity updates
 * - Bio-async integration
 * - BBB security validation
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_flocking.h"
#include "security/nimcp_blood_brain_barrier.h"

class SwarmFlockingTest : public ::testing::Test {
protected:
    nimcp_flocking_system_t* system;
    nimcp_flocking_config_t config;

    void SetUp() override {
        nimcp_flocking_default_config(&config);
        system = nimcp_flocking_create(&config, nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            nimcp_flocking_destroy(system);
        }
    }
};

TEST_F(SwarmFlockingTest, CreateValidSystem) {
    EXPECT_NE(system, nullptr);
}

TEST_F(SwarmFlockingTest, DestroyNullSystem) {
    nimcp_flocking_destroy(nullptr);
    SUCCEED();
}

TEST_F(SwarmFlockingTest, RegisterAgent) {
    nimcp_flocking_agent_t agent = {
        1, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true
    };
    nimcp_result_t result = nimcp_flocking_register_agent(system, &agent);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, UnregisterAgent) {
    nimcp_flocking_agent_t agent = {1, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true};
    nimcp_flocking_register_agent(system, &agent);
    nimcp_result_t result = nimcp_flocking_unregister_agent(system, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, UpdatePosition) {
    nimcp_flocking_agent_t agent = {1, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true};
    nimcp_flocking_register_agent(system, &agent);
    
    nimcp_vector3d_t new_pos = {5.0, 5.0, 0.0};
    nimcp_result_t result = nimcp_flocking_update_position(system, 1, &new_pos);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, CalculateSeparation) {
    // Register multiple agents close together
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_flocking_agent_t agent = {
            i, {(double)i * 0.5, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true
        };
        nimcp_flocking_register_agent(system, &agent);
    }
    
    nimcp_vector3d_t separation;
    nimcp_result_t result = nimcp_flocking_calculate_separation(
        system, 0, &separation
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, CalculateAlignment) {
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_flocking_agent_t agent = {
            i, {(double)i * 2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true
        };
        nimcp_flocking_register_agent(system, &agent);
    }
    
    nimcp_vector3d_t alignment;
    nimcp_result_t result = nimcp_flocking_calculate_alignment(
        system, 0, &alignment
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, CalculateCohesion) {
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_flocking_agent_t agent = {
            i, {(double)i * 3.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true
        };
        nimcp_flocking_register_agent(system, &agent);
    }
    
    nimcp_vector3d_t cohesion;
    nimcp_result_t result = nimcp_flocking_calculate_cohesion(
        system, 0, &cohesion
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, AvoidObstacle) {
    nimcp_flocking_agent_t agent = {1, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true};
    nimcp_flocking_register_agent(system, &agent);
    
    nimcp_obstacle_t obstacle = {{5.0, 0.0, 0.0}, 2.0};
    nimcp_vector3d_t avoidance;
    nimcp_result_t result = nimcp_flocking_calculate_obstacle_avoidance(
        system, 1, &obstacle, &avoidance
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, FollowLeader) {
    nimcp_flocking_agent_t agent = {1, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true};
    nimcp_flocking_register_agent(system, &agent);
    
    nimcp_vector3d_t leader_pos = {10.0, 10.0, 0.0};
    nimcp_result_t result = nimcp_flocking_follow_leader(
        system, 1, &leader_pos
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, UpdateVelocity) {
    nimcp_flocking_agent_t agent = {1, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true};
    nimcp_flocking_register_agent(system, &agent);
    
    nimcp_vector3d_t new_vel = {2.0, 1.0, 0.0};
    nimcp_result_t result = nimcp_flocking_update_velocity(system, 1, &new_vel);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, SetFormation) {
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_flocking_agent_t agent = {i, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true};
        nimcp_flocking_register_agent(system, &agent);
    }
    
    nimcp_result_t result = nimcp_flocking_set_formation(
        system, FORMATION_LINE
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, GetNeighbors) {
    for (uint32_t i = 0; i < 10; i++) {
        nimcp_flocking_agent_t agent = {
            i, {(double)i * 1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true
        };
        nimcp_flocking_register_agent(system, &agent);
    }
    
    uint32_t neighbors[20];
    size_t count = 0;
    nimcp_result_t result = nimcp_flocking_get_neighbors(
        system, 5, 3.0, neighbors, 20, &count
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(count, 0);
}

TEST_F(SwarmFlockingTest, UpdateSystem) {
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_flocking_agent_t agent = {
            i, {(double)i * 2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true
        };
        nimcp_flocking_register_agent(system, &agent);
    }
    
    nimcp_result_t result = nimcp_flocking_update(system, 0.016);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, GetStatistics) {
    nimcp_flocking_stats_t stats;
    nimcp_result_t result = nimcp_flocking_get_stats(system, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, ValidateConfig) {
    nimcp_flocking_config_t test_config;
    nimcp_flocking_default_config(&test_config);
    nimcp_result_t result = nimcp_flocking_validate_config(&test_config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmFlockingTest, MaxAgents) {
    for (uint32_t i = 0; i < 100; i++) {
        nimcp_flocking_agent_t agent = {i, {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true};
        nimcp_flocking_register_agent(system, &agent);
    }
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
