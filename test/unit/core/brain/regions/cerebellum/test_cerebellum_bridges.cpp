/**
 * @file test_cerebellum_bridges.cpp
 * @brief Unit tests for Cerebellum integration bridges
 *
 * Tests:
 * - Cerebellum quantum bridge (timing optimization, trajectory evaluation)
 *
 * @version Phase B4: Cerebellum Brain Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/cerebellum/nimcp_cerebellum_quantum_bridge.h"

//=============================================================================
// Cerebellum Quantum Bridge Tests
//=============================================================================

class CerebellumQuantumBridgeTest : public ::testing::Test {
protected:
    cerebellum_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        cerebellum_quantum_config_t config = cerebellum_quantum_default_config();
        bridge = cerebellum_quantum_bridge_create(nullptr, &config);
    }

    void TearDown() override {
        if (bridge) {
            cerebellum_quantum_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(CerebellumQuantumBridgeTest, CreateWithNullConfig) {
    cerebellum_quantum_bridge_t* b = cerebellum_quantum_bridge_create(nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    cerebellum_quantum_bridge_destroy(b);
}

TEST_F(CerebellumQuantumBridgeTest, CreateWithConfig) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(CerebellumQuantumBridgeTest, DefaultConfigValues) {
    cerebellum_quantum_config_t config = cerebellum_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.timing_search_depth, 0);
    EXPECT_GT(config.trajectory_alternatives, 0);
    EXPECT_GT(config.max_grover_iterations, 0);
    EXPECT_GT(config.min_timing_confidence, 0.0f);
    EXPECT_TRUE(config.enable_interference);
    EXPECT_TRUE(config.use_superposition);
}

TEST_F(CerebellumQuantumBridgeTest, IsEnabled) {
    EXPECT_TRUE(cerebellum_quantum_bridge_is_enabled(bridge));
}

TEST_F(CerebellumQuantumBridgeTest, SetEnabled) {
    cerebellum_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(cerebellum_quantum_bridge_is_enabled(bridge));

    cerebellum_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(cerebellum_quantum_bridge_is_enabled(bridge));
}

//=============================================================================
// Timing Optimization Tests
//=============================================================================

TEST_F(CerebellumQuantumBridgeTest, OptimizeTiming) {
    quantum_timing_result_t result;

    int ret = cerebellum_quantum_optimize_timing(bridge, 100.0f, 20.0f, 100, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_timing, nullptr);
    EXPECT_GT(result.candidates_evaluated, 0);
    EXPECT_GT(result.search_speedup, 0.0f);
}

TEST_F(CerebellumQuantumBridgeTest, OptimizeTimingDisabled) {
    cerebellum_quantum_bridge_set_enabled(bridge, false);

    quantum_timing_result_t result;
    int ret = cerebellum_quantum_optimize_timing(bridge, 100.0f, 20.0f, 100, &result);
    EXPECT_EQ(ret, -1);  // Should fail when disabled
}

TEST_F(CerebellumQuantumBridgeTest, TimingWithinRange) {
    quantum_timing_result_t result;

    float target = 100.0f;
    float range = 20.0f;

    cerebellum_quantum_optimize_timing(bridge, target, range, 100, &result);

    if (result.best_timing) {
        float timing = result.best_timing->timing_ms;
        EXPECT_GE(timing, target - range);
        EXPECT_LE(timing, target + range);
    }
}

TEST_F(CerebellumQuantumBridgeTest, TimingScoreComponents) {
    quantum_timing_result_t result;
    cerebellum_quantum_optimize_timing(bridge, 100.0f, 20.0f, 100, &result);

    if (result.best_timing) {
        EXPECT_GE(result.best_timing->amplitude, 0.0f);
        EXPECT_LE(result.best_timing->amplitude, 1.0f);
        EXPECT_GE(result.best_timing->precision_score, 0.0f);
        EXPECT_LE(result.best_timing->precision_score, 1.0f);
        EXPECT_GE(result.best_timing->energy_cost, 0.0f);
        EXPECT_LE(result.best_timing->energy_cost, 1.0f);
        EXPECT_GE(result.best_timing->combined_score, 0.0f);
    }
}

//=============================================================================
// Trajectory Optimization Tests
//=============================================================================

TEST_F(CerebellumQuantumBridgeTest, OptimizeTrajectory) {
    float start_state[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float end_state[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    quantum_trajectory_result_t result;

    int ret = cerebellum_quantum_optimize_trajectory(
        bridge, start_state, end_state, 4, 500.0f, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_trajectory, nullptr);
    EXPECT_GT(result.trajectories_evaluated, 0);
}

TEST_F(CerebellumQuantumBridgeTest, TrajectoryFeasibility) {
    float start_state[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float end_state[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    quantum_trajectory_result_t result;

    // Use a generous max duration
    cerebellum_quantum_optimize_trajectory(
        bridge, start_state, end_state, 4, 1000.0f, &result);

    if (result.best_trajectory) {
        EXPECT_TRUE(result.best_trajectory->is_feasible);
        EXPECT_LE(result.best_trajectory->duration_ms, 1000.0f);
    }
}

TEST_F(CerebellumQuantumBridgeTest, TrajectoryProperties) {
    float start_state[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float end_state[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    quantum_trajectory_result_t result;

    cerebellum_quantum_optimize_trajectory(
        bridge, start_state, end_state, 4, 500.0f, &result);

    if (result.best_trajectory) {
        EXPECT_GT(result.best_trajectory->num_waypoints, 0);
        EXPECT_LE(result.best_trajectory->num_waypoints, 32);
        EXPECT_GE(result.best_trajectory->smoothness_score, 0.0f);
        EXPECT_LE(result.best_trajectory->smoothness_score, 1.0f);
        EXPECT_GE(result.best_trajectory->energy_efficiency, 0.0f);
        EXPECT_LE(result.best_trajectory->energy_efficiency, 1.0f);
    }
}

//=============================================================================
// Gain Optimization Tests
//=============================================================================

TEST_F(CerebellumQuantumBridgeTest, OptimizeGains) {
    float current_gains[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    quantum_gain_result_t result;

    int ret = cerebellum_quantum_optimize_gains(
        bridge, current_gains, 4, 0.5f, &result);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(result.best_gains, nullptr);
    EXPECT_GT(result.candidates_evaluated, 0);
}

TEST_F(CerebellumQuantumBridgeTest, GainsWithError) {
    float current_gains[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    quantum_gain_result_t result;

    // Test with significant error signal
    cerebellum_quantum_optimize_gains(
        bridge, current_gains, 4, 0.8f, &result);

    if (result.best_gains) {
        EXPECT_GT(result.best_gains->num_gains, 0);
        EXPECT_GE(result.best_gains->stability_score, 0.0f);
        EXPECT_LE(result.best_gains->stability_score, 1.0f);
    }
}

TEST_F(CerebellumQuantumBridgeTest, GainsInRange) {
    float current_gains[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    quantum_gain_result_t result;

    cerebellum_quantum_optimize_gains(
        bridge, current_gains, 4, 0.5f, &result);

    if (result.best_gains) {
        for (uint32_t i = 0; i < result.best_gains->num_gains; i++) {
            EXPECT_GE(result.best_gains->gains[i], 0.1f);
            EXPECT_LE(result.best_gains->gains[i], 2.0f);
        }
    }
}

//=============================================================================
// Program Evaluation Tests
//=============================================================================

TEST_F(CerebellumQuantumBridgeTest, EvaluatePrograms) {
    float program1[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float program2[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float program3[4] = {0.3f, 0.3f, 0.3f, 0.3f};

    const float* programs[3] = {program1, program2, program3};
    float scores[3];

    int ret = cerebellum_quantum_evaluate_programs(
        bridge, programs, 3, 4, scores);
    EXPECT_EQ(ret, 0);

    // All scores should be valid
    for (int i = 0; i < 3; i++) {
        EXPECT_GE(scores[i], 0.0f);
        EXPECT_LE(scores[i], 1.0f);
    }
}

TEST_F(CerebellumQuantumBridgeTest, SelectProgram) {
    float program1[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float program2[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float program3[4] = {0.3f, 0.3f, 0.3f, 0.3f};

    const float* programs[3] = {program1, program2, program3};
    uint32_t best_idx;
    float confidence;

    int ret = cerebellum_quantum_select_program(
        bridge, programs, 3, 4, &best_idx, &confidence);
    EXPECT_EQ(ret, 0);
    EXPECT_LT(best_idx, 3);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CerebellumQuantumBridgeTest, GetStats) {
    cerebellum_quantum_stats_t stats;
    int ret = cerebellum_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.timing_optimizations, 0);
}

TEST_F(CerebellumQuantumBridgeTest, StatsTrackOperations) {
    quantum_timing_result_t timing_result;
    cerebellum_quantum_optimize_timing(bridge, 100.0f, 20.0f, 50, &timing_result);
    cerebellum_quantum_optimize_timing(bridge, 100.0f, 20.0f, 50, &timing_result);

    float start[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float end[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    quantum_trajectory_result_t traj_result;
    cerebellum_quantum_optimize_trajectory(bridge, start, end, 4, 500.0f, &traj_result);

    cerebellum_quantum_stats_t stats;
    cerebellum_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.timing_optimizations, 2);
    EXPECT_EQ(stats.trajectory_optimizations, 1);
}

TEST_F(CerebellumQuantumBridgeTest, ResetStats) {
    quantum_timing_result_t result;
    cerebellum_quantum_optimize_timing(bridge, 100.0f, 20.0f, 50, &result);

    cerebellum_quantum_reset_stats(bridge);

    cerebellum_quantum_stats_t stats;
    cerebellum_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.timing_optimizations, 0);
}

TEST_F(CerebellumQuantumBridgeTest, GetConfig) {
    cerebellum_quantum_config_t config;
    int ret = cerebellum_quantum_get_config(bridge, &config);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enabled);
}

//=============================================================================
// Speedup Verification Tests
//=============================================================================

TEST_F(CerebellumQuantumBridgeTest, GroverSpeedup) {
    quantum_timing_result_t result;

    /* Search in timing space of 1000 alternatives */
    cerebellum_quantum_optimize_timing(bridge, 100.0f, 50.0f, 1000, &result);

    /* Grover provides sqrt(N) speedup */
    /* For N=1000, speedup should be approximately sqrt(1000) = 31.6 */
    EXPECT_GT(result.search_speedup, 20.0f);
}

TEST_F(CerebellumQuantumBridgeTest, SpeedupScalesWithSearchSize) {
    quantum_timing_result_t result100, result1000;

    cerebellum_quantum_optimize_timing(bridge, 100.0f, 50.0f, 100, &result100);
    cerebellum_quantum_optimize_timing(bridge, 100.0f, 50.0f, 1000, &result1000);

    // Larger search space should have larger speedup
    EXPECT_GT(result1000.search_speedup, result100.search_speedup);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

class CerebellumQuantumNullSafetyTest : public ::testing::Test {};

TEST_F(CerebellumQuantumNullSafetyTest, DestroyNull) {
    cerebellum_quantum_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(CerebellumQuantumNullSafetyTest, IsEnabledNull) {
    EXPECT_FALSE(cerebellum_quantum_bridge_is_enabled(nullptr));
}

TEST_F(CerebellumQuantumNullSafetyTest, OptimizeTimingNull) {
    quantum_timing_result_t result;
    EXPECT_EQ(cerebellum_quantum_optimize_timing(nullptr, 100.0f, 20.0f, 100, &result), -1);
}

TEST_F(CerebellumQuantumNullSafetyTest, OptimizeTrajectoryNull) {
    float start[4] = {0, 0, 0, 0};
    float end[4] = {1, 1, 1, 1};
    quantum_trajectory_result_t result;
    EXPECT_EQ(cerebellum_quantum_optimize_trajectory(nullptr, start, end, 4, 500.0f, &result), -1);
}

TEST_F(CerebellumQuantumNullSafetyTest, OptimizeGainsNull) {
    float gains[4] = {1, 1, 1, 1};
    quantum_gain_result_t result;
    EXPECT_EQ(cerebellum_quantum_optimize_gains(nullptr, gains, 4, 0.5f, &result), -1);
}

TEST_F(CerebellumQuantumNullSafetyTest, GetStatsNull) {
    cerebellum_quantum_stats_t stats;
    EXPECT_EQ(cerebellum_quantum_get_stats(nullptr, &stats), -1);
}

//=============================================================================
// Integration Test
//=============================================================================

class CerebellumQuantumIntegrationTest : public ::testing::Test {
protected:
    cerebellum_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = cerebellum_quantum_bridge_create(nullptr, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            cerebellum_quantum_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(CerebellumQuantumIntegrationTest, SimulateMotorCoordination) {
    /* Step 1: Optimize timing for movement */
    quantum_timing_result_t timing_result;
    int ret = cerebellum_quantum_optimize_timing(bridge, 200.0f, 50.0f, 500, &timing_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(timing_result.best_timing, nullptr);

    float optimal_timing = timing_result.best_timing->timing_ms;

    /* Step 2: Find optimal trajectory */
    float start_state[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float end_state[4] = {1.0f, 0.5f, 0.8f, 0.3f};

    quantum_trajectory_result_t traj_result;
    ret = cerebellum_quantum_optimize_trajectory(
        bridge, start_state, end_state, 4, optimal_timing * 2, &traj_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(traj_result.best_trajectory, nullptr);

    /* Step 3: Optimize motor gains based on error */
    float current_gains[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float error = 0.3f;  // 30% error

    quantum_gain_result_t gain_result;
    ret = cerebellum_quantum_optimize_gains(bridge, current_gains, 4, error, &gain_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(gain_result.best_gains, nullptr);

    /* Verify statistics */
    cerebellum_quantum_stats_t stats;
    cerebellum_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.timing_optimizations, 1);
    EXPECT_EQ(stats.trajectory_optimizations, 1);
    EXPECT_EQ(stats.gain_optimizations, 1);

    /* Verify speedup was achieved */
    EXPECT_GT(timing_result.search_speedup, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
