/**
 * @file test_nimcp_motor_quantum_bridge.cpp
 * @brief Unit tests for nimcp_motor_quantum_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Motor Cortex quantum bridge
 * WHY:  Ensure correct quantum trajectory optimization and program selection
 * HOW:  Use Google Test framework to test lifecycle, trajectory optimization,
 *       program selection, timing optimization, and statistics.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_quantum_bridge.h"
#include "core/brain/regions/motor/nimcp_motor_adapter.h"

// Test Fixture for Motor Quantum Bridge
class MotorQuantumBridgeTest : public ::testing::Test {
protected:
    motor_quantum_bridge_t* bridge;
    motor_adapter_t* motor;
    motor_quantum_config_t config;

    void SetUp() override {
        // Create motor adapter first (with bio-async disabled)
        motor_config_t motor_cfg = motor_default_config();
        motor_cfg.enable_bio_async = false;
        motor = motor_create(&motor_cfg);
        ASSERT_NE(nullptr, motor) << "Failed to create Motor adapter";

        // Create quantum bridge
        config = motor_quantum_default_config();
        bridge = motor_quantum_bridge_create(motor, &config);
        ASSERT_NE(nullptr, bridge) << "Failed to create Motor quantum bridge";
    }

    void TearDown() override {
        motor_quantum_bridge_destroy(bridge);
        bridge = nullptr;
        motor_destroy(motor);
        motor = nullptr;
    }
};

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, DefaultConfigHasReasonableValues) {
    motor_quantum_config_t default_config = motor_quantum_default_config();

    EXPECT_TRUE(default_config.enabled);
    EXPECT_EQ(default_config.trajectory_alternatives, 16u);
    EXPECT_EQ(default_config.program_search_depth, 100u);
    EXPECT_EQ(default_config.max_grover_iterations, 10u);
    EXPECT_FLOAT_EQ(default_config.min_trajectory_confidence, 0.5f);
    EXPECT_TRUE(default_config.enable_interference);
    EXPECT_TRUE(default_config.use_superposition);
    EXPECT_FLOAT_EQ(default_config.energy_weight, 0.3f);
    EXPECT_FLOAT_EQ(default_config.time_weight, 0.3f);
    EXPECT_FLOAT_EQ(default_config.accuracy_weight, 0.4f);
    EXPECT_EQ(default_config.seed, 42u);
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, CreateWithNullMotorReturnsNonNull) {
    // Should still create even without motor adapter
    motor_quantum_bridge_t* b = motor_quantum_bridge_create(NULL, &config);
    EXPECT_NE(nullptr, b);
    motor_quantum_bridge_destroy(b);
}

TEST_F(MotorQuantumBridgeTest, CreateWithNullConfigUsesDefaults) {
    motor_quantum_bridge_t* b = motor_quantum_bridge_create(motor, NULL);
    ASSERT_NE(nullptr, b);

    motor_quantum_config_t retrieved;
    EXPECT_EQ(motor_quantum_get_config(b, &retrieved), 0);
    EXPECT_TRUE(retrieved.enabled);
    EXPECT_EQ(retrieved.trajectory_alternatives, 16u);

    motor_quantum_bridge_destroy(b);
}

TEST_F(MotorQuantumBridgeTest, DestroyNullDoesNotCrash) {
    motor_quantum_bridge_destroy(NULL);
    // Should not crash
}

TEST_F(MotorQuantumBridgeTest, IsEnabledReturnsTrue) {
    EXPECT_TRUE(motor_quantum_bridge_is_enabled(bridge));
}

TEST_F(MotorQuantumBridgeTest, IsEnabledNullReturnsFalse) {
    EXPECT_FALSE(motor_quantum_bridge_is_enabled(NULL));
}

TEST_F(MotorQuantumBridgeTest, SetEnabledTogglesBehavior) {
    EXPECT_TRUE(motor_quantum_bridge_is_enabled(bridge));

    motor_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(motor_quantum_bridge_is_enabled(bridge));

    motor_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(motor_quantum_bridge_is_enabled(bridge));
}

TEST_F(MotorQuantumBridgeTest, SetEnabledNullDoesNotCrash) {
    motor_quantum_bridge_set_enabled(NULL, true);
    motor_quantum_bridge_set_enabled(NULL, false);
    // Should not crash
}

// ============================================================================
// TRAJECTORY OPTIMIZATION TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, OptimizeTrajectorySuccess) {
    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {1.0f, 1.0f, 0.0f};
    quantum_trajectory_result_t result;

    int ret = motor_quantum_optimize_trajectory(bridge, &start, &end, 500.0f, 8, &result);
    EXPECT_EQ(ret, 0);

    EXPECT_NE(result.best_trajectory, nullptr);
    EXPECT_GT(result.trajectories_evaluated, 0u);
    EXPECT_GE(result.satisfaction_probability, 0.0f);
    EXPECT_LE(result.satisfaction_probability, 1.0f);
    EXPECT_GT(result.optimization_speedup, 1.0f);  // Should have speedup
}

TEST_F(MotorQuantumBridgeTest, OptimizeTrajectoryWithManyAlternatives) {
    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {2.0f, 2.0f, 1.0f};
    quantum_trajectory_result_t result;

    int ret = motor_quantum_optimize_trajectory(bridge, &start, &end, 1000.0f, 16, &result);
    EXPECT_EQ(ret, 0);

    EXPECT_NE(result.best_trajectory, nullptr);
    EXPECT_LE(result.trajectories_evaluated, 16u);  // Limited by max_candidates
}

TEST_F(MotorQuantumBridgeTest, OptimizeTrajectoryNullParams) {
    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {1.0f, 1.0f, 0.0f};
    quantum_trajectory_result_t result;

    EXPECT_EQ(motor_quantum_optimize_trajectory(NULL, &start, &end, 500.0f, 8, &result), -1);
    EXPECT_EQ(motor_quantum_optimize_trajectory(bridge, NULL, &end, 500.0f, 8, &result), -1);
    EXPECT_EQ(motor_quantum_optimize_trajectory(bridge, &start, NULL, 500.0f, 8, &result), -1);
    EXPECT_EQ(motor_quantum_optimize_trajectory(bridge, &start, &end, 500.0f, 8, NULL), -1);
}

TEST_F(MotorQuantumBridgeTest, OptimizeTrajectoryDisabledReturnsFail) {
    motor_quantum_bridge_set_enabled(bridge, false);

    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {1.0f, 1.0f, 0.0f};
    quantum_trajectory_result_t result;

    EXPECT_EQ(motor_quantum_optimize_trajectory(bridge, &start, &end, 500.0f, 8, &result), -1);
}

TEST_F(MotorQuantumBridgeTest, OptimizeTrajectoryResultHasValidScores) {
    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {1.0f, 1.0f, 0.0f};
    quantum_trajectory_result_t result;

    int ret = motor_quantum_optimize_trajectory(bridge, &start, &end, 500.0f, 8, &result);
    ASSERT_EQ(ret, 0);

    quantum_trajectory_candidate_t* best = result.best_trajectory;
    ASSERT_NE(best, nullptr);

    EXPECT_GE(best->amplitude, 0.0f);
    EXPECT_LE(best->amplitude, 1.0f);
    EXPECT_GE(best->energy_cost, 0.0f);
    EXPECT_LE(best->energy_cost, 1.0f);
    EXPECT_GE(best->time_cost, 0.0f);
    EXPECT_GE(best->accuracy_score, 0.0f);
    EXPECT_LE(best->accuracy_score, 1.0f);
    EXPECT_TRUE(best->is_feasible);
}

// ============================================================================
// PATH OPTIMIZATION TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, OptimizePathSuccess) {
    quantum_trajectory_waypoint_t waypoints[3];
    memset(waypoints, 0, sizeof(waypoints));

    waypoints[0].position.x = 0.0f;
    waypoints[0].position.y = 0.0f;
    waypoints[0].time_ms = 0.0f;

    waypoints[1].position.x = 0.5f;
    waypoints[1].position.y = 0.5f;
    waypoints[1].time_ms = 250.0f;

    waypoints[2].position.x = 1.0f;
    waypoints[2].position.y = 1.0f;
    waypoints[2].time_ms = 500.0f;

    quantum_trajectory_result_t result;
    int ret = motor_quantum_optimize_path(bridge, waypoints, 3, &result);
    EXPECT_EQ(ret, 0);

    EXPECT_NE(result.best_trajectory, nullptr);
}

TEST_F(MotorQuantumBridgeTest, OptimizePathNullParams) {
    quantum_trajectory_waypoint_t waypoints[3];
    quantum_trajectory_result_t result;

    EXPECT_EQ(motor_quantum_optimize_path(NULL, waypoints, 3, &result), -1);
    EXPECT_EQ(motor_quantum_optimize_path(bridge, NULL, 3, &result), -1);
    EXPECT_EQ(motor_quantum_optimize_path(bridge, waypoints, 1, &result), -1);
    EXPECT_EQ(motor_quantum_optimize_path(bridge, waypoints, 3, NULL), -1);
}

// ============================================================================
// PROGRAM SELECTION TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, SelectProgramSuccess) {
    float skill_requirements[4] = {0.8f, 0.5f, 0.3f, 0.9f};
    quantum_program_result_t result;

    int ret = motor_quantum_select_program(bridge, skill_requirements, 4, 10, &result);
    EXPECT_EQ(ret, 0);

    EXPECT_NE(result.best_program, nullptr);
    EXPECT_GT(result.programs_evaluated, 0u);
    EXPECT_GE(result.satisfaction_probability, 0.0f);
    EXPECT_LE(result.satisfaction_probability, 1.0f);
}

TEST_F(MotorQuantumBridgeTest, SelectProgramNullSkillsStillWorks) {
    quantum_program_result_t result;

    int ret = motor_quantum_select_program(bridge, NULL, 0, 10, &result);
    EXPECT_EQ(ret, 0);  // Should work without skill requirements
}

TEST_F(MotorQuantumBridgeTest, SelectProgramNullResult) {
    float skill_requirements[4] = {0.8f, 0.5f, 0.3f, 0.9f};

    EXPECT_EQ(motor_quantum_select_program(NULL, skill_requirements, 4, 10, NULL), -1);
    EXPECT_EQ(motor_quantum_select_program(bridge, skill_requirements, 4, 10, NULL), -1);
}

TEST_F(MotorQuantumBridgeTest, SelectProgramDisabledReturnsFail) {
    motor_quantum_bridge_set_enabled(bridge, false);

    float skill_requirements[4] = {0.8f, 0.5f, 0.3f, 0.9f};
    quantum_program_result_t result;

    EXPECT_EQ(motor_quantum_select_program(bridge, skill_requirements, 4, 10, &result), -1);
}

TEST_F(MotorQuantumBridgeTest, SelectProgramResultHasValidScores) {
    float skill_requirements[4] = {0.8f, 0.5f, 0.3f, 0.9f};
    quantum_program_result_t result;

    int ret = motor_quantum_select_program(bridge, skill_requirements, 4, 10, &result);
    ASSERT_EQ(ret, 0);

    quantum_program_candidate_t* best = result.best_program;
    ASSERT_NE(best, nullptr);

    EXPECT_GE(best->amplitude, 0.0f);
    EXPECT_LE(best->amplitude, 1.0f);
    EXPECT_GE(best->skill_match, 0.0f);
    EXPECT_LE(best->skill_match, 1.0f);
    EXPECT_GE(best->complexity, 0.0f);
    EXPECT_LE(best->complexity, 1.0f);
    EXPECT_GT(best->program_id, 0u);
}

// ============================================================================
// TIMING OPTIMIZATION TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, OptimizeTimingSuccess) {
    float base_timing[4] = {100.0f, 150.0f, 200.0f, 250.0f};
    quantum_timing_result_t result;

    int ret = motor_quantum_optimize_timing(bridge, base_timing, 4, &result);
    EXPECT_EQ(ret, 0);

    EXPECT_NE(result.best_timing, nullptr);
    EXPECT_GT(result.patterns_evaluated, 0u);
    EXPECT_GE(result.optimization_score, 0.0f);
}

TEST_F(MotorQuantumBridgeTest, OptimizeTimingNullParams) {
    float base_timing[4] = {100.0f, 150.0f, 200.0f, 250.0f};
    quantum_timing_result_t result;

    EXPECT_EQ(motor_quantum_optimize_timing(NULL, base_timing, 4, &result), -1);
    EXPECT_EQ(motor_quantum_optimize_timing(bridge, NULL, 4, &result), -1);
    EXPECT_EQ(motor_quantum_optimize_timing(bridge, base_timing, 4, NULL), -1);
}

TEST_F(MotorQuantumBridgeTest, OptimizeTimingDisabledReturnsFail) {
    motor_quantum_bridge_set_enabled(bridge, false);

    float base_timing[4] = {100.0f, 150.0f, 200.0f, 250.0f};
    quantum_timing_result_t result;

    EXPECT_EQ(motor_quantum_optimize_timing(bridge, base_timing, 4, &result), -1);
}

TEST_F(MotorQuantumBridgeTest, OptimizeTimingResultHasValidScores) {
    float base_timing[4] = {100.0f, 150.0f, 200.0f, 250.0f};
    quantum_timing_result_t result;

    int ret = motor_quantum_optimize_timing(bridge, base_timing, 4, &result);
    ASSERT_EQ(ret, 0);

    quantum_timing_candidate_t* best = result.best_timing;
    ASSERT_NE(best, nullptr);

    EXPECT_GE(best->amplitude, 0.0f);
    EXPECT_LE(best->amplitude, 1.0f);
    EXPECT_GE(best->coordination_score, 0.0f);
    EXPECT_LE(best->coordination_score, 1.0f);
    EXPECT_GE(best->rhythm_score, 0.0f);
    EXPECT_LE(best->rhythm_score, 1.0f);
    EXPECT_EQ(best->num_phases, 4u);
}

// ============================================================================
// PARALLEL EVALUATION TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, ParallelEvaluateSuccess) {
    uint32_t program_ids[5] = {1, 2, 3, 4, 5};
    quantum_motor_vec3_t goal = {1.0f, 1.0f, 0.0f};
    quantum_program_result_t result;

    int ret = motor_quantum_parallel_evaluate(bridge, program_ids, 5, &goal, &result);
    EXPECT_EQ(ret, 0);

    EXPECT_NE(result.best_program, nullptr);
}

TEST_F(MotorQuantumBridgeTest, ParallelEvaluateNullParams) {
    uint32_t program_ids[5] = {1, 2, 3, 4, 5};
    quantum_motor_vec3_t goal = {1.0f, 1.0f, 0.0f};
    quantum_program_result_t result;

    EXPECT_EQ(motor_quantum_parallel_evaluate(NULL, program_ids, 5, &goal, &result), -1);
    EXPECT_EQ(motor_quantum_parallel_evaluate(bridge, NULL, 5, &goal, &result), -1);
    EXPECT_EQ(motor_quantum_parallel_evaluate(bridge, program_ids, 5, NULL, &result), -1);
    EXPECT_EQ(motor_quantum_parallel_evaluate(bridge, program_ids, 5, &goal, NULL), -1);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, GetStatsSuccess) {
    motor_quantum_stats_t stats;
    int ret = motor_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.trajectory_optimizations, 0u);
    EXPECT_EQ(stats.program_selections, 0u);
    EXPECT_EQ(stats.timing_optimizations, 0u);
}

TEST_F(MotorQuantumBridgeTest, GetStatsAfterOperations) {
    // Perform some optimizations
    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {1.0f, 1.0f, 0.0f};
    quantum_trajectory_result_t traj_result;
    motor_quantum_optimize_trajectory(bridge, &start, &end, 500.0f, 8, &traj_result);

    float skill_requirements[4] = {0.8f, 0.5f, 0.3f, 0.9f};
    quantum_program_result_t prog_result;
    motor_quantum_select_program(bridge, skill_requirements, 4, 10, &prog_result);

    float base_timing[4] = {100.0f, 150.0f, 200.0f, 250.0f};
    quantum_timing_result_t timing_result;
    motor_quantum_optimize_timing(bridge, base_timing, 4, &timing_result);

    motor_quantum_stats_t stats;
    int ret = motor_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(stats.trajectory_optimizations, 1u);
    EXPECT_EQ(stats.program_selections, 1u);
    EXPECT_EQ(stats.timing_optimizations, 1u);
    EXPECT_GT(stats.avg_trajectory_speedup, 0.0f);
}

TEST_F(MotorQuantumBridgeTest, GetStatsNullParams) {
    motor_quantum_stats_t stats;

    EXPECT_EQ(motor_quantum_get_stats(NULL, &stats), -1);
    EXPECT_EQ(motor_quantum_get_stats(bridge, NULL), -1);
    EXPECT_EQ(motor_quantum_get_stats(NULL, NULL), -1);
}

TEST_F(MotorQuantumBridgeTest, ResetStatsSuccess) {
    // Perform an optimization
    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {1.0f, 1.0f, 0.0f};
    quantum_trajectory_result_t result;
    motor_quantum_optimize_trajectory(bridge, &start, &end, 500.0f, 8, &result);

    motor_quantum_stats_t stats;
    motor_quantum_get_stats(bridge, &stats);
    EXPECT_GT(stats.trajectory_optimizations, 0u);

    motor_quantum_reset_stats(bridge);

    motor_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.trajectory_optimizations, 0u);
    EXPECT_EQ(stats.program_selections, 0u);
    EXPECT_EQ(stats.timing_optimizations, 0u);
}

TEST_F(MotorQuantumBridgeTest, ResetStatsNullDoesNotCrash) {
    motor_quantum_reset_stats(NULL);
    // Should not crash
}

TEST_F(MotorQuantumBridgeTest, GetConfigSuccess) {
    motor_quantum_config_t retrieved;
    int ret = motor_quantum_get_config(bridge, &retrieved);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(retrieved.enabled, config.enabled);
    EXPECT_EQ(retrieved.trajectory_alternatives, config.trajectory_alternatives);
    EXPECT_EQ(retrieved.max_grover_iterations, config.max_grover_iterations);
    EXPECT_FLOAT_EQ(retrieved.min_trajectory_confidence, config.min_trajectory_confidence);
}

TEST_F(MotorQuantumBridgeTest, GetConfigNullParams) {
    motor_quantum_config_t retrieved;

    EXPECT_EQ(motor_quantum_get_config(NULL, &retrieved), -1);
    EXPECT_EQ(motor_quantum_get_config(bridge, NULL), -1);
    EXPECT_EQ(motor_quantum_get_config(NULL, NULL), -1);
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(MotorQuantumBridgeTest, OptimizeTrajectoryZeroDistance) {
    quantum_motor_vec3_t same = {0.5f, 0.5f, 0.5f};
    quantum_trajectory_result_t result;

    // Start and end at same point
    int ret = motor_quantum_optimize_trajectory(bridge, &same, &same, 100.0f, 8, &result);
    EXPECT_EQ(ret, 0);

    // Should still work, even if trivial
    EXPECT_NE(result.best_trajectory, nullptr);
}

TEST_F(MotorQuantumBridgeTest, OptimizeTrajectoryLargeAlternatives) {
    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {10.0f, 10.0f, 10.0f};
    quantum_trajectory_result_t result;

    // Request more alternatives than max_candidates
    int ret = motor_quantum_optimize_trajectory(bridge, &start, &end, 1000.0f, 1000, &result);
    EXPECT_EQ(ret, 0);

    // Should be capped at max_candidates
    EXPECT_LE(result.trajectories_evaluated, config.trajectory_alternatives);
}

TEST_F(MotorQuantumBridgeTest, MultipleOptimizationsUpdateStats) {
    quantum_motor_vec3_t start = {0.0f, 0.0f, 0.0f};
    quantum_motor_vec3_t end = {1.0f, 1.0f, 0.0f};
    quantum_trajectory_result_t result;

    for (int i = 0; i < 5; i++) {
        motor_quantum_optimize_trajectory(bridge, &start, &end, 500.0f, 8, &result);
    }

    motor_quantum_stats_t stats;
    motor_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.trajectory_optimizations, 5u);
    EXPECT_GT(stats.avg_trajectory_speedup, 0.0f);
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
