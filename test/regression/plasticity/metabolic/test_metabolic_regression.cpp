/**
 * @file test_metabolic_regression.cpp
 * @brief Regression tests for metabolic plasticity stability
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"
#include "plasticity/metabolic/nimcp_metabolic_sleep_bridge.h"
#include "plasticity/metabolic/nimcp_metabolic_immune_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Stability Tests
 * ============================================================================ */

class MetabolicRegressionTest : public ::testing::Test {
protected:
    metabolic_plasticity_t* metabolic;

    void SetUp() override {
        metabolic = metabolic_plasticity_create(nullptr);
        ASSERT_NE(metabolic, nullptr);
    }

    void TearDown() override {
        if (metabolic) {
            metabolic_plasticity_destroy(metabolic);
        }
    }
};

TEST_F(MetabolicRegressionTest, LongTermRecoveryStability) {
    // Deplete ATP
    metabolic_plasticity_restore_atp(metabolic, 30.0f);

    // Simulate long recovery period (100 seconds)
    for (int i = 0; i < 100; i++) {
        metabolic_plasticity_update(metabolic, 1000);
    }

    // Should eventually recover to full capacity
    float final_atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_FLOAT_EQ(final_atp, METABOLIC_ATP_FULL_CAPACITY);
}

TEST_F(MetabolicRegressionTest, RepeatedDepletionRecovery) {
    // Cycle 10 times: deplete, recover
    for (int cycle = 0; cycle < 10; cycle++) {
        // Deplete
        for (int i = 0; i < 10; i++) {
            metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
        }

        // Recover
        for (int i = 0; i < 20; i++) {
            metabolic_plasticity_update(metabolic, 1000);
        }
    }

    // Should be stable near full capacity
    float final_atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_GE(final_atp, 95.0f);
}

TEST_F(MetabolicRegressionTest, ConsistentGatingBehavior) {
    // Test gating at various ATP levels (should be deterministic)
    for (float atp_level = 0.0f; atp_level <= 100.0f; atp_level += 10.0f) {
        metabolic_plasticity_restore_atp(metabolic, atp_level);

        bool can_ltp = metabolic_plasticity_can_ltp(metabolic);
        bool can_ltd = metabolic_plasticity_can_ltd(metabolic);

        // Check consistency
        if (atp_level >= METABOLIC_LTP_THRESHOLD) {
            EXPECT_TRUE(can_ltp);
        } else {
            EXPECT_FALSE(can_ltp);
        }

        if (atp_level >= METABOLIC_LTD_THRESHOLD) {
            EXPECT_TRUE(can_ltd);
        } else {
            EXPECT_FALSE(can_ltd);
        }
    }
}

TEST_F(MetabolicRegressionTest, EnergyStateTransitions) {
    // Test all state transitions
    metabolic_plasticity_restore_atp(metabolic, 100.0f);
    EXPECT_EQ(metabolic_plasticity_get_energy_state(metabolic), ENERGY_STATE_HEALTHY);

    metabolic_plasticity_restore_atp(metabolic, 60.0f);
    EXPECT_EQ(metabolic_plasticity_get_energy_state(metabolic), ENERGY_STATE_DEPLETED);

    metabolic_plasticity_restore_atp(metabolic, 40.0f);
    EXPECT_EQ(metabolic_plasticity_get_energy_state(metabolic), ENERGY_STATE_CRITICAL);

    metabolic_plasticity_restore_atp(metabolic, 20.0f);
    EXPECT_EQ(metabolic_plasticity_get_energy_state(metabolic), ENERGY_STATE_EMERGENCY);

    // Reverse transitions
    metabolic_plasticity_restore_atp(metabolic, 40.0f);
    EXPECT_EQ(metabolic_plasticity_get_energy_state(metabolic), ENERGY_STATE_CRITICAL);

    metabolic_plasticity_restore_atp(metabolic, 60.0f);
    EXPECT_EQ(metabolic_plasticity_get_energy_state(metabolic), ENERGY_STATE_DEPLETED);

    metabolic_plasticity_restore_atp(metabolic, 80.0f);
    EXPECT_EQ(metabolic_plasticity_get_energy_state(metabolic), ENERGY_STATE_HEALTHY);
}

TEST_F(MetabolicRegressionTest, StatisticsAccuracy) {
    // Reset stats
    metabolic_plasticity_reset_stats(metabolic);

    // Perform known operations
    const int ltp_count = 10;
    const int ltd_count = 5;

    for (int i = 0; i < ltp_count; i++) {
        metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    }

    for (int i = 0; i < ltd_count; i++) {
        metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTD, 1.0f);
    }

    // Check statistics
    metabolic_stats_t stats;
    metabolic_plasticity_get_stats(metabolic, &stats);

    EXPECT_EQ(stats.total_ltp_events, ltp_count);
    EXPECT_EQ(stats.total_ltd_events, ltd_count);

    float expected_consumed = (ltp_count * METABOLIC_COST_LTP_BASE) +
                             (ltd_count * METABOLIC_COST_LTD_BASE);
    EXPECT_FLOAT_EQ(stats.total_atp_consumed, expected_consumed);
}

TEST_F(MetabolicRegressionTest, NoNegativeATP) {
    // Attempt to over-deplete
    for (int i = 0; i < 100; i++) {
        metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    }

    float atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_GE(atp, METABOLIC_ATP_MIN);
}

TEST_F(MetabolicRegressionTest, NoExcessiveATP) {
    // Attempt to over-recover
    for (int i = 0; i < 100; i++) {
        metabolic_plasticity_update(metabolic, 10000);
    }

    float atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_LE(atp, METABOLIC_ATP_FULL_CAPACITY);
}

TEST_F(MetabolicRegressionTest, RecoveryRateConsistency) {
    float base_rate = METABOLIC_RECOVERY_RATE_BASE +
                     METABOLIC_RECOVERY_RATE_GLYCOLYSIS +
                     METABOLIC_RECOVERY_RATE_ASTROCYTE;

    // Set custom rate
    metabolic_plasticity_set_recovery_rate(metabolic, 5.0f);
    EXPECT_FLOAT_EQ(metabolic_plasticity_get_recovery_rate(metabolic), 5.0f);

    // Reset to base
    metabolic_plasticity_set_recovery_rate(metabolic, base_rate);
    EXPECT_FLOAT_EQ(metabolic_plasticity_get_recovery_rate(metabolic), base_rate);
}

TEST_F(MetabolicRegressionTest, EffectiveLRConsistency) {
    float base_lr = 0.01f;

    // Test at multiple ATP levels
    for (float atp = 0.0f; atp <= 100.0f; atp += 10.0f) {
        metabolic_plasticity_restore_atp(metabolic, atp);

        float effective_lr = metabolic_plasticity_get_effective_lr(metabolic, base_lr);
        float expected_lr = base_lr * (atp / METABOLIC_ATP_FULL_CAPACITY);

        EXPECT_FLOAT_EQ(effective_lr, expected_lr);
    }
}

/* ============================================================================
 * Sleep Bridge Regression Tests
 * ============================================================================ */

class MetabolicSleepRegressionTest : public ::testing::Test {
protected:
    metabolic_plasticity_t* metabolic;
    sleep_system_t sleep_system;
    metabolic_sleep_bridge_t bridge;

    void SetUp() override {
        sleep_config_t sleep_config = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_config);
        metabolic = metabolic_plasticity_create(nullptr);
        bridge = metabolic_sleep_bridge_create(nullptr, sleep_system, metabolic);

        ASSERT_NE(sleep_system, nullptr);
        ASSERT_NE(metabolic, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) metabolic_sleep_bridge_destroy(bridge);
        if (metabolic) metabolic_plasticity_destroy(metabolic);
        if (sleep_system) sleep_system_destroy(sleep_system);
    }
};

TEST_F(MetabolicSleepRegressionTest, SleepStateModulationConsistency) {
    // Test each sleep state produces expected modulation
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    float expected_recovery[] = {
        METABOLIC_SLEEP_RECOVERY_AWAKE,
        METABOLIC_SLEEP_RECOVERY_DROWSY,
        METABOLIC_SLEEP_RECOVERY_LIGHT_NREM,
        METABOLIC_SLEEP_RECOVERY_DEEP_NREM,
        METABOLIC_SLEEP_RECOVERY_REM
    };

    for (int i = 0; i < 5; i++) {
        sleep_enter_state(sleep_system, states[i]);
        metabolic_sleep_update(bridge);

        metabolic_sleep_effects_t effects;
        metabolic_sleep_get_effects(bridge, &effects);

        EXPECT_FLOAT_EQ(effects.recovery_rate_factor, expected_recovery[i]);
    }
}

TEST_F(MetabolicSleepRegressionTest, SleepPressureMonotonicity) {
    // Sleep pressure should increase as ATP decreases
    float prev_pressure = 0.0f;

    for (float atp = 100.0f; atp >= 20.0f; atp -= 10.0f) {
        metabolic_plasticity_restore_atp(metabolic, atp);
        metabolic_sleep_update(bridge);

        float pressure = metabolic_sleep_get_sleep_pressure(bridge);

        EXPECT_GE(pressure, prev_pressure);  // Should increase or stay same
        prev_pressure = pressure;
    }
}

TEST_F(MetabolicSleepRegressionTest, RepeatedSleepCycles) {
    // Simulate 10 sleep-wake cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Wake: deplete
        sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
        metabolic_sleep_update(bridge);
        for (int i = 0; i < 5; i++) {
            metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
        }

        // Sleep: recover
        sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
        metabolic_sleep_update(bridge);
        for (int i = 0; i < 10; i++) {
            metabolic_plasticity_update(metabolic, 1000);
        }
    }

    // Should be stable
    float final_atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_GE(final_atp, 80.0f);
}

/* ============================================================================
 * Immune Bridge Regression Tests
 * ============================================================================ */

class MetabolicImmuneRegressionTest : public ::testing::Test {
protected:
    metabolic_plasticity_t* metabolic;
    brain_immune_system_t* immune;
    metabolic_immune_bridge_t* bridge;

    void SetUp() override {
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        metabolic = metabolic_plasticity_create(nullptr);
        bridge = metabolic_immune_bridge_create(nullptr, immune, metabolic);

        ASSERT_NE(immune, nullptr);
        ASSERT_NE(metabolic, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) metabolic_immune_bridge_destroy(bridge);
        if (metabolic) metabolic_plasticity_destroy(metabolic);
        if (immune) brain_immune_destroy(immune);
    }
};

TEST_F(MetabolicImmuneRegressionTest, CytokineEffectsConsistency) {
    // Multiple applications should produce same effects
    for (int i = 0; i < 10; i++) {
        metabolic_immune_apply_cytokine_effects(bridge);

        cytokine_metabolic_effects_t effects;
        metabolic_immune_get_cytokine_effects(bridge, &effects);

        EXPECT_FLOAT_EQ(effects.il1_burden, CYTOKINE_IL1_METABOLIC_BURDEN);
        EXPECT_GT(effects.total_cost_multiplier, 0.0f);
    }
}

TEST_F(MetabolicImmuneRegressionTest, ImmuneCapacityMonotonicity) {
    // Immune capacity should decrease as ATP decreases
    float prev_capacity = 1.0f;

    for (float atp = 100.0f; atp >= 20.0f; atp -= 10.0f) {
        metabolic_plasticity_restore_atp(metabolic, atp);
        metabolic_immune_update_atp_effects(bridge);

        float capacity = metabolic_immune_get_immune_capacity(bridge);

        EXPECT_LE(capacity, prev_capacity);  // Should decrease or stay same
        prev_capacity = capacity;
    }
}

TEST_F(MetabolicImmuneRegressionTest, RepeatedBridgeUpdates) {
    // Many updates should be stable
    for (int i = 0; i < 100; i++) {
        metabolic_immune_bridge_update(bridge, 100);
    }

    // Should still function
    cytokine_metabolic_effects_t effects;
    int ret = metabolic_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(MetabolicImmuneRegressionTest, CostModulationBounds) {
    float base_cost = 3.0f;

    // Apply effects many times
    for (int i = 0; i < 10; i++) {
        metabolic_immune_apply_cytokine_effects(bridge);
        metabolic_immune_apply_inflammation_effects(bridge);
    }

    float effective_cost = metabolic_immune_get_effective_cost(bridge, base_cost);

    // Cost should be reasonable (not negative, not absurdly high)
    EXPECT_GE(effective_cost, 0.0f);
    EXPECT_LE(effective_cost, base_cost * 10.0f);  // At most 10x
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
