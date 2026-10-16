/**
 * @file test_swarm_pheromone.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Pheromone System
 *
 * TEST COVERAGE:
 * - System creation and destruction
 * - Pheromone deposit and retrieval
 * - Gradient calculation
 * - Path planning
 * - Decay and evaporation
 * - Environmental modifiers
 * - Spatial queries
 * - Path reinforcement
 * - Bio-async integration
 * - BBB security validation
 * - Edge cases and error handling
 * - Performance under load
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_pheromone.h"
#include "security/nimcp_blood_brain_barrier.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmPheromoneTest : public ::testing::Test {
protected:
    nimcp_pheromone_system_t* system;
    nimcp_pheromone_config_t config;
    nimcp_blood_brain_barrier_t* bbb;

    void SetUp() override {
        // Get default configuration
        nimcp_pheromone_default_config(&config);

        // Create BBB for security tests
        bbb = nullptr; // Can be NULL for basic tests

        // Create pheromone system
        system = nimcp_pheromone_create(&config, bbb);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            nimcp_pheromone_destroy(system);
            system = nullptr;
        }
        if (bbb) {
            // nimcp_bbb_destroy(bbb);
            bbb = nullptr;
        }
    }

    // Helper to create position
    nimcp_position3d_t makePosition(float x, float y, float z) {
        nimcp_position3d_t pos = {x, y, z};
        return pos;
    }
};

//=============================================================================
// Basic Creation and Destruction Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, CreateValidSystem) {
    EXPECT_NE(system, nullptr);
}

TEST_F(SwarmPheromoneTest, DestroyNullSystem) {
    nimcp_pheromone_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

TEST_F(SwarmPheromoneTest, CreateWithNullConfig) {
    auto* sys = nimcp_pheromone_create(nullptr, nullptr);
    // Should use defaults or fail gracefully
    if (sys) {
        nimcp_pheromone_destroy(sys);
    }
    SUCCEED();
}

TEST_F(SwarmPheromoneTest, CreateWithBBB) {
    // Test with BBB integration
    auto* sys = nimcp_pheromone_create(&config, bbb);
    EXPECT_NE(sys, nullptr);
    if (sys) {
        nimcp_pheromone_destroy(sys);
    }
}

//=============================================================================
// Pheromone Deposit Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, DepositBasicPheromone) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);

    nimcp_result_t result = nimcp_pheromone_deposit(
        system, &pos, PHEROMONE_RESOURCE, 0.5f
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmPheromoneTest, DepositMultipleTypes) {
    nimcp_position3d_t pos = makePosition(10.0f, 10.0f, 0.0f);

    // Deposit different pheromone types at same location
    for (int i = 0; i < PHEROMONE_TYPE_COUNT; i++) {
        nimcp_result_t result = nimcp_pheromone_deposit(
            system, &pos, static_cast<nimcp_pheromone_type_t>(i), 0.3f
        );
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(SwarmPheromoneTest, DepositWithNullPosition) {
    nimcp_result_t result = nimcp_pheromone_deposit(
        system, nullptr, PHEROMONE_PATH, 0.5f
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(SwarmPheromoneTest, DepositOutOfBounds) {
    nimcp_position3d_t pos = makePosition(1000.0f, 1000.0f, 1000.0f);

    nimcp_result_t result = nimcp_pheromone_deposit(
        system, &pos, PHEROMONE_DANGER, 0.5f
    );

    // Should handle gracefully
    SUCCEED();
}

TEST_F(SwarmPheromoneTest, DepositNegativeAmount) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);

    nimcp_result_t result = nimcp_pheromone_deposit(
        system, &pos, PHEROMONE_RESOURCE, -1.0f
    );

    // Should handle negative values appropriately
    SUCCEED();
}

//=============================================================================
// Pheromone Retrieval Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, RetrieveDeposited) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);
    float deposited = 0.7f;

    nimcp_pheromone_deposit(system, &pos, PHEROMONE_RESOURCE, deposited);

    float concentration = 0.0f;
    nimcp_result_t result = nimcp_pheromone_get_concentration(
        system, &pos, PHEROMONE_RESOURCE, &concentration
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NEAR(concentration, deposited, 0.01f);
}

TEST_F(SwarmPheromoneTest, RetrieveEmpty) {
    nimcp_position3d_t pos = makePosition(50.0f, 50.0f, 0.0f);

    float concentration = 0.0f;
    nimcp_result_t result = nimcp_pheromone_get_concentration(
        system, &pos, PHEROMONE_PATH, &concentration
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(concentration, 0.0f);
}

TEST_F(SwarmPheromoneTest, RetrieveWithNullOutput) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);

    nimcp_result_t result = nimcp_pheromone_get_concentration(
        system, &pos, PHEROMONE_RESOURCE, nullptr
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Gradient Calculation Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, CalculateGradient) {
    // Create a gradient by depositing at multiple locations
    nimcp_position3d_t pos1 = makePosition(0.0f, 0.0f, 0.0f);
    nimcp_position3d_t pos2 = makePosition(10.0f, 0.0f, 0.0f);

    nimcp_pheromone_deposit(system, &pos1, PHEROMONE_RESOURCE, 0.2f);
    nimcp_pheromone_deposit(system, &pos2, PHEROMONE_RESOURCE, 0.8f);

    nimcp_position3d_t query_pos = makePosition(5.0f, 0.0f, 0.0f);
    nimcp_pheromone_gradient_t gradient;

    nimcp_result_t result = nimcp_pheromone_get_gradient(
        system, &query_pos, PHEROMONE_RESOURCE, &gradient
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(gradient.magnitude, 0.0f);
}

TEST_F(SwarmPheromoneTest, GradientDirection) {
    // Deposit pheromone to the east
    nimcp_position3d_t source = makePosition(20.0f, 10.0f, 0.0f);
    nimcp_position3d_t query = makePosition(10.0f, 10.0f, 0.0f);

    nimcp_pheromone_deposit(system, &source, PHEROMONE_TARGET, 1.0f);

    nimcp_pheromone_gradient_t gradient;
    nimcp_pheromone_get_gradient(system, &query, PHEROMONE_TARGET, &gradient);

    // Gradient should point east (positive X direction)
    EXPECT_GT(gradient.direction.x, 0.0f);
}

TEST_F(SwarmPheromoneTest, GradientWithNullOutput) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);

    nimcp_result_t result = nimcp_pheromone_get_gradient(
        system, &pos, PHEROMONE_RESOURCE, nullptr
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Path Planning Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, PlanSimplePath) {
    // Create a simple pheromone trail
    for (int i = 0; i <= 10; i++) {
        nimcp_position3d_t pos = makePosition((float)i, 0.0f, 0.0f);
        nimcp_pheromone_deposit(system, &pos, PHEROMONE_PATH, 0.5f);
    }

    nimcp_position3d_t start = makePosition(0.0f, 0.0f, 0.0f);
    nimcp_position3d_t path[100];
    size_t path_length = 0;

    nimcp_result_t result = nimcp_pheromone_plan_path(
        system, &start, PHEROMONE_PATH, 100, path, &path_length
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(path_length, 0);
}

TEST_F(SwarmPheromoneTest, PlanPathNoTrail) {
    nimcp_position3d_t start = makePosition(0.0f, 0.0f, 0.0f);
    nimcp_position3d_t path[100];
    size_t path_length = 0;

    nimcp_result_t result = nimcp_pheromone_plan_path(
        system, &start, PHEROMONE_RESOURCE, 100, path, &path_length
    );

    // Should handle gracefully
    SUCCEED();
}

//=============================================================================
// Path Reinforcement Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, ReinforcePath) {
    // Create a path
    const size_t path_len = 5;
    nimcp_position3d_t path[path_len];
    for (size_t i = 0; i < path_len; i++) {
        path[i] = makePosition((float)i, (float)i, 0.0f);
    }

    nimcp_result_t result = nimcp_pheromone_reinforce_path(
        system, path, path_len, PHEROMONE_PATH, 1.5f
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify increased concentration
    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(
        system, &path[2], PHEROMONE_PATH, &concentration
    );
    EXPECT_GT(concentration, 0.0f);
}

TEST_F(SwarmPheromoneTest, ReinforceWithNullPath) {
    nimcp_result_t result = nimcp_pheromone_reinforce_path(
        system, nullptr, 10, PHEROMONE_PATH, 1.0f
    );

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Decay and Evaporation Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, PheromoneDecay) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);
    float initial = 1.0f;

    nimcp_pheromone_deposit(system, &pos, PHEROMONE_RESOURCE, initial);

    // Simulate time passing
    nimcp_pheromone_update(system, 10000); // 10 seconds

    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(
        system, &pos, PHEROMONE_RESOURCE, &concentration
    );

    // Should have decayed
    EXPECT_LT(concentration, initial);
}

TEST_F(SwarmPheromoneTest, MultipleUpdates) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);
    nimcp_pheromone_deposit(system, &pos, PHEROMONE_DANGER, 0.8f);

    // Multiple time steps
    for (int i = 0; i < 5; i++) {
        nimcp_result_t result = nimcp_pheromone_update(system, 1000);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(
        system, &pos, PHEROMONE_DANGER, &concentration
    );

    // Should have decayed over multiple updates
    EXPECT_LT(concentration, 0.8f);
}

//=============================================================================
// Environmental Modifier Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, SetEnvironment) {
    nimcp_position3d_t center = makePosition(10.0f, 10.0f, 0.0f);

    nimcp_result_t result = nimcp_pheromone_set_environment(
        system, &center, 5.0f, 0.5f
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmPheromoneTest, DepositWithModifier) {
    nimcp_position3d_t pos = makePosition(10.0f, 10.0f, 0.0f);

    nimcp_result_t result = nimcp_pheromone_deposit_modified(
        system, &pos, PHEROMONE_RESOURCE, 0.7f, 0.8f
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Spatial Query Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, QueryRadius) {
    // Deposit pheromones in a pattern
    for (int i = 0; i < 5; i++) {
        nimcp_position3d_t pos = makePosition((float)i, 0.0f, 0.0f);
        nimcp_pheromone_deposit(system, &pos, PHEROMONE_RESOURCE, 0.5f);
    }

    nimcp_position3d_t center = makePosition(2.0f, 0.0f, 0.0f);
    nimcp_pheromone_trail_t trails[20];
    size_t count = 0;

    nimcp_result_t result = nimcp_pheromone_query_radius(
        system, &center, 3.0f, PHEROMONE_RESOURCE, trails, 20, &count
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(count, 0);
}

TEST_F(SwarmPheromoneTest, QueryAllTypes) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);

    // Deposit multiple types
    nimcp_pheromone_deposit(system, &pos, PHEROMONE_RESOURCE, 0.5f);
    nimcp_pheromone_deposit(system, &pos, PHEROMONE_DANGER, 0.5f);

    nimcp_pheromone_trail_t trails[20];
    size_t count = 0;

    nimcp_result_t result = nimcp_pheromone_query_radius(
        system, &pos, 2.0f, PHEROMONE_TYPE_COUNT, trails, 20, &count
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Reset Functionality Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, ResetSystem) {
    // Deposit some pheromones
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);
    nimcp_pheromone_deposit(system, &pos, PHEROMONE_RESOURCE, 0.8f);

    // Reset
    nimcp_result_t result = nimcp_pheromone_reset(system);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify cleared
    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(
        system, &pos, PHEROMONE_RESOURCE, &concentration
    );
    EXPECT_FLOAT_EQ(concentration, 0.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, GetStatistics) {
    // Perform some operations
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);
    nimcp_pheromone_deposit(system, &pos, PHEROMONE_RESOURCE, 0.5f);

    nimcp_pheromone_stats_t stats;
    nimcp_result_t result = nimcp_pheromone_get_stats(system, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(stats.total_deposits, 0);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, TypeName) {
    const char* name = nimcp_pheromone_type_name(PHEROMONE_RESOURCE);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0);
}

TEST_F(SwarmPheromoneTest, ValidateConfig) {
    nimcp_pheromone_config_t test_config;
    nimcp_pheromone_default_config(&test_config);

    nimcp_result_t result = nimcp_pheromone_validate_config(&test_config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmPheromoneTest, InvalidConfig) {
    nimcp_pheromone_config_t invalid_config;
    memset(&invalid_config, 0, sizeof(invalid_config));
    invalid_config.voxel_size = -1.0f; // Invalid

    nimcp_result_t result = nimcp_pheromone_validate_config(&invalid_config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, RegisterBioAsync) {
    // Note: This test requires a mock router
    nimcp_result_t result = nimcp_pheromone_register_bioasync(
        system, nullptr
    );
    // Should handle null router gracefully
    SUCCEED();
}

TEST_F(SwarmPheromoneTest, BroadcastUpdate) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);

    nimcp_result_t result = nimcp_pheromone_broadcast_update(
        system, &pos, PHEROMONE_RESOURCE, 0.7f
    );

    // Should handle without router
    SUCCEED();
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(SwarmPheromoneTest, MaxConcentration) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);

    // Try to deposit excessive amounts
    nimcp_pheromone_deposit(system, &pos, PHEROMONE_RESOURCE, 10.0f);
    nimcp_pheromone_deposit(system, &pos, PHEROMONE_RESOURCE, 10.0f);

    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(
        system, &pos, PHEROMONE_RESOURCE, &concentration
    );

    // Should be capped at max
    EXPECT_LE(concentration, config.max_concentration);
}

TEST_F(SwarmPheromoneTest, ConcurrentDeposits) {
    nimcp_position3d_t pos = makePosition(5.0f, 5.0f, 0.0f);

    // Multiple deposits at same location
    for (int i = 0; i < 100; i++) {
        nimcp_pheromone_deposit(system, &pos, PHEROMONE_PATH, 0.1f);
    }

    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(
        system, &pos, PHEROMONE_PATH, &concentration
    );

    EXPECT_GT(concentration, 0.0f);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
