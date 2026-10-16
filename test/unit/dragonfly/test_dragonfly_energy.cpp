/**
 * @file test_dragonfly_energy.cpp
 * @brief Unit tests for energy-optimal pursuit planning module
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_energy.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EnergyTest : public ::testing::Test {
protected:
    dragonfly_energy_t energy = nullptr;

    void SetUp() override {
        energy = dragonfly_energy_create(nullptr);
        ASSERT_NE(energy, nullptr);
    }

    void TearDown() override {
        if (energy) {
            dragonfly_energy_destroy(energy);
            energy = nullptr;
        }
    }

    interceptor_state_t make_self(float x, float y, float z) {
        interceptor_state_t self = {};
        self.position[0] = x;
        self.position[1] = y;
        self.position[2] = z;
        self.max_speed = 15.0f;
        self.max_accel = 10.0f;
        return self;
    }

    target_state_t make_target(float x, float y, float z, float vx, float vy, float vz) {
        target_state_t target = {};
        target.position[0] = x;
        target.position[1] = y;
        target.position[2] = z;
        target.velocity[0] = vx;
        target.velocity[1] = vy;
        target.velocity[2] = vz;
        target.confidence = 0.9f;
        return target;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(EnergyTest, DefaultConfig) {
    energy_config_t config = energy_default_config();
    EXPECT_GT(config.max_energy_j, 0.0f);
    EXPECT_GT(config.pursuit_power_w, config.rest_power_w);
    EXPECT_GT(config.reserve_fraction, 0.0f);
    EXPECT_LT(config.reserve_fraction, 1.0f);
}

TEST_F(EnergyTest, ValidateConfig) {
    energy_config_t config = energy_default_config();
    EXPECT_TRUE(energy_validate_config(&config));

    config.max_energy_j = 0.0f;
    EXPECT_FALSE(energy_validate_config(&config));

    config = energy_default_config();
    config.reserve_fraction = 1.5f;  // Invalid
    EXPECT_FALSE(energy_validate_config(&config));

    EXPECT_FALSE(energy_validate_config(nullptr));
}

TEST_F(EnergyTest, CreateWithCustomConfig) {
    energy_config_t config = energy_default_config();
    config.max_energy_j = 2000.0f;
    // Maintain power hierarchy: rest < hover < patrol < pursuit < max
    config.rest_power_w = 10.0f;
    config.hover_power_w = 30.0f;
    config.patrol_power_w = 60.0f;
    config.pursuit_power_w = 100.0f;
    config.max_power_w = 150.0f;

    dragonfly_energy_t custom = dragonfly_energy_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_energy_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EnergyTest, CreateAndDestroy) {
    dragonfly_energy_t e = dragonfly_energy_create(nullptr);
    ASSERT_NE(e, nullptr);
    dragonfly_energy_destroy(e);
}

TEST_F(EnergyTest, DestroyNull) {
    dragonfly_energy_destroy(nullptr);  // Should not crash
}

TEST_F(EnergyTest, Reset) {
    EXPECT_EQ(dragonfly_energy_reset(energy), 0);
}

//=============================================================================
// Budget Tests
//=============================================================================

TEST_F(EnergyTest, GetBudget) {
    energy_budget_t budget;
    EXPECT_EQ(dragonfly_energy_get_budget(energy, &budget), 0);
    EXPECT_GT(budget.current_energy_j, 0.0f);
    EXPECT_EQ(budget.state, ENERGY_STATE_FULL);
}

TEST_F(EnergyTest, SpendEnergy) {
    energy_budget_t before, after;
    dragonfly_energy_get_budget(energy, &before);

    EXPECT_EQ(dragonfly_energy_spend(energy, 100.0f, "test"), 0);

    dragonfly_energy_get_budget(energy, &after);
    EXPECT_LT(after.current_energy_j, before.current_energy_j);
}

TEST_F(EnergyTest, GainEnergy) {
    // First spend some energy
    dragonfly_energy_spend(energy, 500.0f, "test");

    energy_budget_t before, after;
    dragonfly_energy_get_budget(energy, &before);

    EXPECT_EQ(dragonfly_energy_gain(energy, 200.0f), 0);

    dragonfly_energy_get_budget(energy, &after);
    EXPECT_GT(after.current_energy_j, before.current_energy_j);
}

TEST_F(EnergyTest, EnergyStateTransitions) {
    energy_budget_t budget;

    // Start at full
    dragonfly_energy_get_budget(energy, &budget);
    EXPECT_EQ(budget.state, ENERGY_STATE_FULL);

    // Spend to get to low
    dragonfly_energy_spend(energy, budget.current_energy_j * 0.8f, "test");
    dragonfly_energy_get_budget(energy, &budget);
    EXPECT_NE(budget.state, ENERGY_STATE_FULL);
}

//=============================================================================
// Activity Cost Tests
//=============================================================================

TEST_F(EnergyTest, UpdateWithActivity) {
    energy_budget_t before, after;
    dragonfly_energy_get_budget(energy, &before);

    EXPECT_EQ(dragonfly_energy_update(energy, ACTIVITY_PURSUIT, 1.0f), 0);

    dragonfly_energy_get_budget(energy, &after);
    EXPECT_LT(after.current_energy_j, before.current_energy_j);
}

TEST_F(EnergyTest, DifferentActivityCosts) {
    energy_budget_t after_rest, after_pursuit;

    // Test resting
    dragonfly_energy_reset(energy);
    dragonfly_energy_update(energy, ACTIVITY_REST, 1.0f);
    dragonfly_energy_get_budget(energy, &after_rest);

    // Test pursuit
    dragonfly_energy_reset(energy);
    dragonfly_energy_update(energy, ACTIVITY_PURSUIT, 1.0f);
    dragonfly_energy_get_budget(energy, &after_pursuit);

    // Pursuit should cost more than rest
    EXPECT_GT(after_rest.current_energy_j, after_pursuit.current_energy_j);
}

//=============================================================================
// Pursuit Estimation Tests
//=============================================================================

TEST_F(EnergyTest, EstimatePursuitEnergy) {
    intercept_solution_t solution = {};
    solution.intercept_time_s = 3.0f;
    solution.feasibility = INTERCEPT_FEASIBLE;

    pursuit_energy_t estimate;
    EXPECT_EQ(dragonfly_energy_estimate_pursuit(energy, &solution, 0.05f, 0.7f, &estimate), 0);

    EXPECT_GT(estimate.estimated_energy_j, 0.0f);
    EXPECT_GT(estimate.pursuit_duration_s, 0.0f);
}

TEST_F(EnergyTest, EconomicallyViableCheck) {
    intercept_solution_t solution = {};
    solution.intercept_time_s = 0.5f;  // Quick catch
    solution.feasibility = INTERCEPT_FEASIBLE;

    pursuit_energy_t estimate;
    dragonfly_energy_estimate_pursuit(energy, &solution, 0.1f, 0.9f, &estimate);

    // Large prey with high success probability should be viable
    EXPECT_TRUE(estimate.economically_viable);
    EXPECT_GT(estimate.roi, 0.0f);
}

TEST_F(EnergyTest, NotViableIfLongPursuit) {
    intercept_solution_t solution = {};
    solution.intercept_time_s = 30.0f;  // Very long chase
    solution.feasibility = INTERCEPT_FEASIBLE;

    pursuit_energy_t estimate;
    dragonfly_energy_estimate_pursuit(energy, &solution, 0.01f, 0.3f, &estimate);

    // Small prey, low success, long chase should not be viable
    EXPECT_FALSE(estimate.economically_viable);
}

//=============================================================================
// Optimization Tests
//=============================================================================

TEST_F(EnergyTest, OptimizePursuit) {
    interceptor_state_t self = make_self(0, 0, 0);
    target_state_t target = make_target(100, 0, 0, 5, 0, 0);

    energy_optimization_t optimization;
    EXPECT_EQ(dragonfly_energy_optimize_pursuit(energy, &self, &target, 0.05f, &optimization), 0);

    // Should get a recommendation
    EXPECT_GE(optimization.optimal_speed, 0.0f);
    EXPECT_LE(optimization.optimal_speed, 1.0f);
}

TEST_F(EnergyTest, OptimizationRecommendsPursuit) {
    interceptor_state_t self = make_self(0, 0, 0);
    target_state_t target = make_target(50, 0, 0, 2, 0, 0);  // Easy target

    energy_optimization_t optimization;
    dragonfly_energy_optimize_pursuit(energy, &self, &target, 0.1f, &optimization);

    // Easy, nearby target should recommend pursuit
    EXPECT_TRUE(optimization.should_pursue);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EnergyTest, GetStats) {
    energy_stats_t stats;
    EXPECT_EQ(dragonfly_energy_get_stats(energy, &stats), 0);
}

TEST_F(EnergyTest, StatsAfterActivity) {
    dragonfly_energy_update(energy, ACTIVITY_PURSUIT, 1.0f);

    energy_stats_t stats;
    dragonfly_energy_get_stats(energy, &stats);
    // After one update, pursuits_attempted may still be 0 (no pursuit estimated)
    EXPECT_GE(stats.total_energy_spent_j, 0.0f);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(EnergyTest, NullPointerHandling) {
    EXPECT_EQ(dragonfly_energy_update(nullptr, ACTIVITY_REST, 1.0f), -1);
    EXPECT_EQ(dragonfly_energy_get_budget(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_energy_spend(nullptr, 100.0f, "test"), -1);
    EXPECT_EQ(dragonfly_energy_gain(nullptr, 100.0f), -1);
}

TEST_F(EnergyTest, InvalidDeltaTime) {
    EXPECT_EQ(dragonfly_energy_update(energy, ACTIVITY_REST, -1.0f), -1);
    EXPECT_EQ(dragonfly_energy_update(energy, ACTIVITY_REST, 0.0f), -1);
}
