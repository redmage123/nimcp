/**
 * @file test_metabolic_plasticity.cpp
 * @brief Unit tests for metabolic plasticity constraints
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MetabolicPlasticityTest : public ::testing::Test {
protected:
    metabolic_plasticity_t* metabolic;
    metabolic_config_t config;

    void SetUp() override {
        metabolic_plasticity_default_config(&config);
        metabolic = metabolic_plasticity_create(&config);
        ASSERT_NE(metabolic, nullptr);
    }

    void TearDown() override {
        if (metabolic) {
            metabolic_plasticity_destroy(metabolic);
            metabolic = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, CreateDestroy) {
    // Tested in SetUp/TearDown
    SUCCEED();
}

TEST_F(MetabolicPlasticityTest, CreateWithNullConfig) {
    metabolic_plasticity_t* m = metabolic_plasticity_create(nullptr);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(metabolic_plasticity_get_atp_level(m), METABOLIC_ATP_INITIAL);
    metabolic_plasticity_destroy(m);
}

TEST_F(MetabolicPlasticityTest, DefaultConfig) {
    metabolic_config_t cfg;
    int ret = metabolic_plasticity_default_config(&cfg);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(cfg.initial_atp, METABOLIC_ATP_INITIAL);
    EXPECT_EQ(cfg.costs.ltp_cost, METABOLIC_COST_LTP_BASE);
    EXPECT_EQ(cfg.costs.ltd_cost, METABOLIC_COST_LTD_BASE);
    EXPECT_TRUE(cfg.enable_ltp_gating);
    EXPECT_TRUE(cfg.enable_ltd_gating);
}

TEST_F(MetabolicPlasticityTest, DefaultConfigNullParam) {
    int ret = metabolic_plasticity_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Initial State Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, InitialATPLevel) {
    float atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_FLOAT_EQ(atp, METABOLIC_ATP_INITIAL);
}

TEST_F(MetabolicPlasticityTest, InitialEnergyState) {
    energy_state_t state = metabolic_plasticity_get_energy_state(metabolic);
    EXPECT_EQ(state, ENERGY_STATE_HEALTHY);
}

TEST_F(MetabolicPlasticityTest, InitialLTPPermitted) {
    EXPECT_TRUE(metabolic_plasticity_can_ltp(metabolic));
}

TEST_F(MetabolicPlasticityTest, InitialLTDPermitted) {
    EXPECT_TRUE(metabolic_plasticity_can_ltd(metabolic));
}

TEST_F(MetabolicPlasticityTest, InitialATPState) {
    atp_pool_state_t state;
    int ret = metabolic_plasticity_get_atp_state(metabolic, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(state.current_atp, METABOLIC_ATP_INITIAL);
    EXPECT_FLOAT_EQ(state.max_capacity, METABOLIC_ATP_FULL_CAPACITY);
    EXPECT_TRUE(state.ltp_permitted);
    EXPECT_TRUE(state.ltd_permitted);
    EXPECT_EQ(state.state, ENERGY_STATE_HEALTHY);
}

/* ============================================================================
 * Energy Consumption Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, ConsumeLTPEvent) {
    float initial_atp = metabolic_plasticity_get_atp_level(metabolic);
    int ret = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    EXPECT_EQ(ret, 0);

    float new_atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_FLOAT_EQ(new_atp, initial_atp - METABOLIC_COST_LTP_BASE);
}

TEST_F(MetabolicPlasticityTest, ConsumeLTDEvent) {
    float initial_atp = metabolic_plasticity_get_atp_level(metabolic);
    int ret = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTD, 1.0f);
    EXPECT_EQ(ret, 0);

    float new_atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_FLOAT_EQ(new_atp, initial_atp - METABOLIC_COST_LTD_BASE);
}

TEST_F(MetabolicPlasticityTest, ConsumeLTPMoreExpensiveThanLTD) {
    // Create two separate metabolic systems
    metabolic_plasticity_t* m1 = metabolic_plasticity_create(&config);
    metabolic_plasticity_t* m2 = metabolic_plasticity_create(&config);

    metabolic_plasticity_consume_atp(m1, METABOLIC_EVENT_LTP, 1.0f);
    metabolic_plasticity_consume_atp(m2, METABOLIC_EVENT_LTD, 1.0f);

    float atp_after_ltp = metabolic_plasticity_get_atp_level(m1);
    float atp_after_ltd = metabolic_plasticity_get_atp_level(m2);

    EXPECT_LT(atp_after_ltp, atp_after_ltd);  // LTP consumes more

    metabolic_plasticity_destroy(m1);
    metabolic_plasticity_destroy(m2);
}

TEST_F(MetabolicPlasticityTest, ConsumeScaledMagnitude) {
    float initial_atp = metabolic_plasticity_get_atp_level(metabolic);
    int ret = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 0.5f);
    EXPECT_EQ(ret, 0);

    float new_atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_FLOAT_EQ(new_atp, initial_atp - (METABOLIC_COST_LTP_BASE * 0.5f));
}

TEST_F(MetabolicPlasticityTest, ConsumeMultipleEvents) {
    // Consume 10 LTP events
    for (int i = 0; i < 10; i++) {
        metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    }

    float expected_atp = METABOLIC_ATP_INITIAL - (10 * METABOLIC_COST_LTP_BASE);
    float actual_atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_FLOAT_EQ(actual_atp, expected_atp);
}

TEST_F(MetabolicPlasticityTest, ConsumeInvalidMagnitude) {
    int ret1 = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, -0.1f);
    EXPECT_EQ(ret1, -1);

    int ret2 = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.5f);
    EXPECT_EQ(ret2, -1);
}

/* ============================================================================
 * Energy Gating Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, LTPBlockedWhenBelowThreshold) {
    // Deplete ATP below LTP threshold
    metabolic_plasticity_restore_atp(metabolic, METABOLIC_LTP_THRESHOLD - 1.0f);

    EXPECT_FALSE(metabolic_plasticity_can_ltp(metabolic));

    // Attempting LTP should fail
    int ret = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(MetabolicPlasticityTest, LTDBlockedWhenBelowThreshold) {
    // Deplete ATP below LTD threshold
    metabolic_plasticity_restore_atp(metabolic, METABOLIC_LTD_THRESHOLD - 1.0f);

    EXPECT_FALSE(metabolic_plasticity_can_ltd(metabolic));

    // Attempting LTD should fail
    int ret = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTD, 1.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(MetabolicPlasticityTest, LTPAllowedAboveThreshold) {
    metabolic_plasticity_restore_atp(metabolic, METABOLIC_LTP_THRESHOLD + 10.0f);

    EXPECT_TRUE(metabolic_plasticity_can_ltp(metabolic));

    int ret = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(MetabolicPlasticityTest, LTDAllowedAboveThreshold) {
    metabolic_plasticity_restore_atp(metabolic, METABOLIC_LTD_THRESHOLD + 10.0f);

    EXPECT_TRUE(metabolic_plasticity_can_ltd(metabolic));

    int ret = metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTD, 1.0f);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Energy State Classification Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, EnergyStateHealthy) {
    metabolic_plasticity_restore_atp(metabolic, 80.0f);
    energy_state_t state = metabolic_plasticity_get_energy_state(metabolic);
    EXPECT_EQ(state, ENERGY_STATE_HEALTHY);
}

TEST_F(MetabolicPlasticityTest, EnergyStateDepleted) {
    metabolic_plasticity_restore_atp(metabolic, 60.0f);
    energy_state_t state = metabolic_plasticity_get_energy_state(metabolic);
    EXPECT_EQ(state, ENERGY_STATE_DEPLETED);
}

TEST_F(MetabolicPlasticityTest, EnergyStateCritical) {
    metabolic_plasticity_restore_atp(metabolic, 40.0f);
    energy_state_t state = metabolic_plasticity_get_energy_state(metabolic);
    EXPECT_EQ(state, ENERGY_STATE_CRITICAL);
}

TEST_F(MetabolicPlasticityTest, EnergyStateEmergency) {
    metabolic_plasticity_restore_atp(metabolic, 20.0f);
    energy_state_t state = metabolic_plasticity_get_energy_state(metabolic);
    EXPECT_EQ(state, ENERGY_STATE_EMERGENCY);
}

TEST_F(MetabolicPlasticityTest, ClassifyEnergyStateHelper) {
    EXPECT_EQ(metabolic_classify_energy_state(80.0f), ENERGY_STATE_HEALTHY);
    EXPECT_EQ(metabolic_classify_energy_state(60.0f), ENERGY_STATE_DEPLETED);
    EXPECT_EQ(metabolic_classify_energy_state(40.0f), ENERGY_STATE_CRITICAL);
    EXPECT_EQ(metabolic_classify_energy_state(20.0f), ENERGY_STATE_EMERGENCY);
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, RecoveryIncreasesATP) {
    // Deplete ATP
    metabolic_plasticity_restore_atp(metabolic, 50.0f);

    // Update for 1 second
    int ret = metabolic_plasticity_update(metabolic, 1000);
    EXPECT_EQ(ret, 0);

    // ATP should increase
    float atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_GT(atp, 50.0f);
}

TEST_F(MetabolicPlasticityTest, RecoveryRateCorrect) {
    metabolic_plasticity_restore_atp(metabolic, 50.0f);

    float base_rate = METABOLIC_RECOVERY_RATE_BASE +
                     METABOLIC_RECOVERY_RATE_GLYCOLYSIS +
                     METABOLIC_RECOVERY_RATE_ASTROCYTE;

    metabolic_plasticity_update(metabolic, 1000);  // 1 second

    float expected_atp = 50.0f + base_rate;
    float actual_atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_NEAR(actual_atp, expected_atp, 0.01f);
}

TEST_F(MetabolicPlasticityTest, RecoveryCappedAtMax) {
    metabolic_plasticity_restore_atp(metabolic, 95.0f);

    // Update for 10 seconds (should fully recover)
    metabolic_plasticity_update(metabolic, 10000);

    float atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_FLOAT_EQ(atp, METABOLIC_ATP_FULL_CAPACITY);
}

TEST_F(MetabolicPlasticityTest, SetRecoveryRate) {
    float new_rate = 5.0f;
    int ret = metabolic_plasticity_set_recovery_rate(metabolic, new_rate);
    EXPECT_EQ(ret, 0);

    float actual_rate = metabolic_plasticity_get_recovery_rate(metabolic);
    EXPECT_FLOAT_EQ(actual_rate, new_rate);
}

TEST_F(MetabolicPlasticityTest, GetRecoveryRate) {
    float base_rate = METABOLIC_RECOVERY_RATE_BASE +
                     METABOLIC_RECOVERY_RATE_GLYCOLYSIS +
                     METABOLIC_RECOVERY_RATE_ASTROCYTE;

    float rate = metabolic_plasticity_get_recovery_rate(metabolic);
    EXPECT_FLOAT_EQ(rate, base_rate);
}

TEST_F(MetabolicPlasticityTest, ManualRestore) {
    int ret = metabolic_plasticity_restore_atp(metabolic, 75.0f);
    EXPECT_EQ(ret, 0);

    float atp = metabolic_plasticity_get_atp_level(metabolic);
    EXPECT_FLOAT_EQ(atp, 75.0f);
}

TEST_F(MetabolicPlasticityTest, RestoreClampedToRange) {
    metabolic_plasticity_restore_atp(metabolic, 150.0f);
    EXPECT_FLOAT_EQ(metabolic_plasticity_get_atp_level(metabolic), METABOLIC_ATP_FULL_CAPACITY);

    metabolic_plasticity_restore_atp(metabolic, -10.0f);
    EXPECT_FLOAT_EQ(metabolic_plasticity_get_atp_level(metabolic), METABOLIC_ATP_MIN);
}

/* ============================================================================
 * Modulation Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, EffectiveLRScaledByATP) {
    float base_lr = 0.01f;

    // At full ATP: 100% LR
    metabolic_plasticity_restore_atp(metabolic, 100.0f);
    float lr1 = metabolic_plasticity_get_effective_lr(metabolic, base_lr);
    EXPECT_FLOAT_EQ(lr1, base_lr);

    // At 50% ATP: 50% LR
    metabolic_plasticity_restore_atp(metabolic, 50.0f);
    float lr2 = metabolic_plasticity_get_effective_lr(metabolic, base_lr);
    EXPECT_FLOAT_EQ(lr2, base_lr * 0.5f);

    // At 0% ATP: 0% LR
    metabolic_plasticity_restore_atp(metabolic, 0.0f);
    float lr3 = metabolic_plasticity_get_effective_lr(metabolic, base_lr);
    EXPECT_FLOAT_EQ(lr3, 0.0f);
}

TEST_F(MetabolicPlasticityTest, MagnitudeScaleLTP) {
    // Above LTP threshold: full magnitude
    metabolic_plasticity_restore_atp(metabolic, 80.0f);
    float scale1 = metabolic_plasticity_get_magnitude_scale(metabolic, METABOLIC_EVENT_LTP);
    EXPECT_GT(scale1, 0.5f);

    // Below LTP threshold: zero magnitude
    metabolic_plasticity_restore_atp(metabolic, 40.0f);
    float scale2 = metabolic_plasticity_get_magnitude_scale(metabolic, METABOLIC_EVENT_LTP);
    EXPECT_FLOAT_EQ(scale2, 0.0f);
}

TEST_F(MetabolicPlasticityTest, MagnitudeScaleLTD) {
    // Above LTD threshold: full magnitude
    metabolic_plasticity_restore_atp(metabolic, 80.0f);
    float scale1 = metabolic_plasticity_get_magnitude_scale(metabolic, METABOLIC_EVENT_LTD);
    EXPECT_GT(scale1, 0.7f);

    // Below LTD threshold: zero magnitude
    metabolic_plasticity_restore_atp(metabolic, 20.0f);
    float scale2 = metabolic_plasticity_get_magnitude_scale(metabolic, METABOLIC_EVENT_LTD);
    EXPECT_FLOAT_EQ(scale2, 0.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, StatsInitiallyZero) {
    metabolic_stats_t stats;
    int ret = metabolic_plasticity_get_stats(metabolic, &stats);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(stats.total_ltp_events, 0);
    EXPECT_EQ(stats.total_ltd_events, 0);
    EXPECT_EQ(stats.ltp_blocked_count, 0);
    EXPECT_EQ(stats.ltd_blocked_count, 0);
    EXPECT_FLOAT_EQ(stats.total_atp_consumed, 0.0f);
    EXPECT_FLOAT_EQ(stats.total_atp_recovered, 0.0f);
}

TEST_F(MetabolicPlasticityTest, StatsTrackLTPEvents) {
    metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);

    metabolic_stats_t stats;
    metabolic_plasticity_get_stats(metabolic, &stats);

    EXPECT_EQ(stats.total_ltp_events, 2);
}

TEST_F(MetabolicPlasticityTest, StatsTrackLTDEvents) {
    metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTD, 1.0f);
    metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTD, 1.0f);
    metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTD, 1.0f);

    metabolic_stats_t stats;
    metabolic_plasticity_get_stats(metabolic, &stats);

    EXPECT_EQ(stats.total_ltd_events, 3);
}

TEST_F(MetabolicPlasticityTest, StatsTrackBlockedEvents) {
    // Deplete ATP
    metabolic_plasticity_restore_atp(metabolic, 40.0f);

    // Try LTP (should be blocked)
    metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);

    metabolic_stats_t stats;
    metabolic_plasticity_get_stats(metabolic, &stats);

    EXPECT_EQ(stats.ltp_blocked_count, 1);
    EXPECT_EQ(stats.total_ltp_events, 1);
}

TEST_F(MetabolicPlasticityTest, StatsTrackATPConsumed) {
    metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);

    metabolic_stats_t stats;
    metabolic_plasticity_get_stats(metabolic, &stats);

    EXPECT_FLOAT_EQ(stats.total_atp_consumed, METABOLIC_COST_LTP_BASE);
}

TEST_F(MetabolicPlasticityTest, StatsTrackATPRecovered) {
    metabolic_plasticity_restore_atp(metabolic, 50.0f);
    metabolic_plasticity_update(metabolic, 1000);

    metabolic_stats_t stats;
    metabolic_plasticity_get_stats(metabolic, &stats);

    EXPECT_GT(stats.total_atp_recovered, 0.0f);
}

TEST_F(MetabolicPlasticityTest, StatsTrackMinATP) {
    metabolic_plasticity_restore_atp(metabolic, 30.0f);

    metabolic_stats_t stats;
    metabolic_plasticity_get_stats(metabolic, &stats);

    EXPECT_FLOAT_EQ(stats.min_atp_reached, 30.0f);
}

TEST_F(MetabolicPlasticityTest, StatsReset) {
    metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);

    int ret = metabolic_plasticity_reset_stats(metabolic);
    EXPECT_EQ(ret, 0);

    metabolic_stats_t stats;
    metabolic_plasticity_get_stats(metabolic, &stats);

    EXPECT_EQ(stats.total_ltp_events, 0);
    EXPECT_FLOAT_EQ(stats.total_atp_consumed, 0.0f);
}

TEST_F(MetabolicPlasticityTest, GetLTPBlockRate) {
    // Do 5 successful LTP, then deplete and do 5 blocked
    for (int i = 0; i < 5; i++) {
        metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    }

    metabolic_plasticity_restore_atp(metabolic, 40.0f);

    for (int i = 0; i < 5; i++) {
        metabolic_plasticity_consume_atp(metabolic, METABOLIC_EVENT_LTP, 1.0f);
    }

    float block_rate = metabolic_plasticity_get_ltp_block_rate(metabolic);
    EXPECT_FLOAT_EQ(block_rate, 0.5f);  // 5/10 = 50%
}

TEST_F(MetabolicPlasticityTest, GetAvgATP) {
    metabolic_plasticity_update(metabolic, 100);
    metabolic_plasticity_update(metabolic, 100);

    float avg = metabolic_plasticity_get_avg_atp(metabolic);
    EXPECT_GT(avg, 0.0f);
}

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, GetEventCost) {
    EXPECT_FLOAT_EQ(metabolic_get_event_cost(METABOLIC_EVENT_LTP), METABOLIC_COST_LTP_BASE);
    EXPECT_FLOAT_EQ(metabolic_get_event_cost(METABOLIC_EVENT_LTD), METABOLIC_COST_LTD_BASE);
    EXPECT_FLOAT_EQ(metabolic_get_event_cost(METABOLIC_EVENT_SPINE_GROWTH), METABOLIC_COST_SPINE_GROWTH);
    EXPECT_FLOAT_EQ(metabolic_get_event_cost(METABOLIC_EVENT_PROTEIN_SYNTH), METABOLIC_COST_PROTEIN_SYNTH);
}

TEST_F(MetabolicPlasticityTest, EnergyStateNameHelper) {
    EXPECT_STREQ(metabolic_energy_state_name(ENERGY_STATE_HEALTHY), "HEALTHY");
    EXPECT_STREQ(metabolic_energy_state_name(ENERGY_STATE_DEPLETED), "DEPLETED");
    EXPECT_STREQ(metabolic_energy_state_name(ENERGY_STATE_CRITICAL), "CRITICAL");
    EXPECT_STREQ(metabolic_energy_state_name(ENERGY_STATE_EMERGENCY), "EMERGENCY");
}

TEST_F(MetabolicPlasticityTest, EventTypeNameHelper) {
    EXPECT_STREQ(metabolic_event_type_name(METABOLIC_EVENT_LTP), "LTP");
    EXPECT_STREQ(metabolic_event_type_name(METABOLIC_EVENT_LTD), "LTD");
    EXPECT_STREQ(metabolic_event_type_name(METABOLIC_EVENT_SPINE_GROWTH), "SPINE_GROWTH");
    EXPECT_STREQ(metabolic_event_type_name(METABOLIC_EVENT_PROTEIN_SYNTH), "PROTEIN_SYNTH");
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(MetabolicPlasticityTest, NullPointerHandling) {
    EXPECT_FALSE(metabolic_plasticity_can_ltp(nullptr));
    EXPECT_FALSE(metabolic_plasticity_can_ltd(nullptr));
    EXPECT_EQ(metabolic_plasticity_get_atp_level(nullptr), 0.0f);
    EXPECT_EQ(metabolic_plasticity_get_energy_state(nullptr), ENERGY_STATE_EMERGENCY);
    EXPECT_EQ(metabolic_plasticity_consume_atp(nullptr, METABOLIC_EVENT_LTP, 1.0f), -1);
    EXPECT_EQ(metabolic_plasticity_update(nullptr, 1000), -1);
}

TEST_F(MetabolicPlasticityTest, ZeroDeltaUpdate) {
    float initial_atp = metabolic_plasticity_get_atp_level(metabolic);
    metabolic_plasticity_update(metabolic, 0);
    float final_atp = metabolic_plasticity_get_atp_level(metabolic);

    EXPECT_FLOAT_EQ(initial_atp, final_atp);
}

TEST_F(MetabolicPlasticityTest, DisableLTPGating) {
    config.enable_ltp_gating = false;
    metabolic_plasticity_t* m = metabolic_plasticity_create(&config);

    // Deplete ATP
    metabolic_plasticity_restore_atp(m, 10.0f);

    // LTP should still be permitted (gating disabled)
    EXPECT_TRUE(metabolic_plasticity_can_ltp(m));

    metabolic_plasticity_destroy(m);
}

TEST_F(MetabolicPlasticityTest, DisableLTDGating) {
    config.enable_ltd_gating = false;
    metabolic_plasticity_t* m = metabolic_plasticity_create(&config);

    // Deplete ATP
    metabolic_plasticity_restore_atp(m, 10.0f);

    // LTD should still be permitted (gating disabled)
    EXPECT_TRUE(metabolic_plasticity_can_ltd(m));

    metabolic_plasticity_destroy(m);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
