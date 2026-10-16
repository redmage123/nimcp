/**
 * @file test_extended_metaplasticity.cpp
 * @brief Unit Tests for Extended Metaplasticity Module
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include "plasticity/metaplasticity/nimcp_extended_metaplasticity.h"
#include "utils/time/nimcp_time.h"
#include <cmath>

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExtendedMetaplasticityTest : public ::testing::Test {
protected:
    extended_metaplasticity_config_t config;
    extended_metaplasticity_state_t* state;
    metaplasticity_controller_t controller;

    void SetUp() override {
        config = metaplasticity_config_default();
        state = nullptr;
        controller = nullptr;
    }

    void TearDown() override {
        if (state) {
            metaplasticity_state_destroy(state);
        }
        if (controller) {
            metaplasticity_controller_destroy(controller);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, DefaultConfigurationIsValid) {
    extended_metaplasticity_config_t cfg = metaplasticity_config_default();

    EXPECT_GT(cfg.baseline_tau_ms, 0.0f);
    EXPECT_GT(cfg.history_tau_ms, 0.0f);
    EXPECT_GT(cfg.history_size, 0);
    EXPECT_GT(cfg.min_theta, 0.0f);
    EXPECT_LT(cfg.min_theta, cfg.max_theta);
    EXPECT_TRUE(cfg.enable_sleep_reset);
    EXPECT_TRUE(cfg.enable_neuromodulator_shifts);
}

TEST_F(ExtendedMetaplasticityTest, FastConfigurationHasShorterTimescales) {
    extended_metaplasticity_config_t fast_cfg = metaplasticity_config_fast();
    extended_metaplasticity_config_t default_cfg = metaplasticity_config_default();

    EXPECT_LT(fast_cfg.baseline_tau_ms, default_cfg.baseline_tau_ms);
    EXPECT_LT(fast_cfg.history_tau_ms, default_cfg.history_tau_ms);
    EXPECT_GT(fast_cfg.da_sensitivity, default_cfg.da_sensitivity);
}

TEST_F(ExtendedMetaplasticityTest, SlowConfigurationHasLongerTimescales) {
    extended_metaplasticity_config_t slow_cfg = metaplasticity_config_slow();
    extended_metaplasticity_config_t default_cfg = metaplasticity_config_default();

    EXPECT_GT(slow_cfg.baseline_tau_ms, default_cfg.baseline_tau_ms);
    EXPECT_GT(slow_cfg.history_tau_ms, default_cfg.history_tau_ms);
    EXPECT_LT(slow_cfg.da_sensitivity, default_cfg.da_sensitivity);
}

TEST_F(ExtendedMetaplasticityTest, HippocampalConfigurationIsRewardSensitive) {
    extended_metaplasticity_config_t hipp_cfg = metaplasticity_config_hippocampal();

    EXPECT_GT(hipp_cfg.da_sensitivity, 1.5f);  // Very sensitive to dopamine
    EXPECT_GT(hipp_cfg.sleep_reset_strength, 1.0f);  // Enhanced consolidation
}

/* ============================================================================
 * State Lifecycle Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, StateCreationSucceeds) {
    state = metaplasticity_state_create(&config);

    ASSERT_NE(state, nullptr);
    EXPECT_FLOAT_EQ(state->theta_baseline, config.initial_theta_baseline);
    EXPECT_FLOAT_EQ(state->theta_effective, config.initial_theta_baseline);
    EXPECT_EQ(state->history_size, config.history_size);
}

TEST_F(ExtendedMetaplasticityTest, StateCreationWithNullConfigUsesDefaults) {
    state = metaplasticity_state_create(nullptr);

    ASSERT_NE(state, nullptr);
    EXPECT_GT(state->history_size, 0);
}

TEST_F(ExtendedMetaplasticityTest, StateDestructionDoesNotCrash) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    metaplasticity_state_destroy(state);
    state = nullptr;  // Prevent double-free

    SUCCEED();
}

TEST_F(ExtendedMetaplasticityTest, StateResetClearsHistory) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    // Modify state
    state->theta_effective = 5.0f;
    state->history_count = 10;

    // Reset
    metaplasticity_state_reset(state, &config);

    EXPECT_FLOAT_EQ(state->theta_effective, config.initial_theta_baseline);
    EXPECT_EQ(state->history_count, 0);
}

/* ============================================================================
 * Baseline Threshold Update Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, BaselineThresholdTracksActivitySquared) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float activity = 2.0f;
    float dt = 100.0f;

    // Update multiple times
    for (int i = 0; i < 100; i++) {
        metaplasticity_update_baseline(state, activity, dt, &config);
    }

    // Baseline should approach activity squared
    float expected_target = activity * activity;
    EXPECT_NEAR(state->theta_baseline_target, expected_target, 0.5f);
}

TEST_F(ExtendedMetaplasticityTest, BaselineThresholdClampsToMinMax) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    // Drive activity very high
    float activity = 100.0f;
    float dt = 1000.0f;

    for (int i = 0; i < 100; i++) {
        metaplasticity_update_baseline(state, activity, dt, &config);
    }

    EXPECT_LE(state->theta_baseline, config.max_theta);
    EXPECT_GE(state->theta_baseline, config.min_theta);
}

TEST_F(ExtendedMetaplasticityTest, BaselineAdaptsSlowlyWithLargeTau) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float initial_theta = state->theta_baseline;
    float activity = 3.0f;
    float dt = 10.0f;  // Small timestep

    metaplasticity_update_baseline(state, activity, dt, &config);

    // Should change very little in one small timestep
    EXPECT_NEAR(state->theta_baseline, initial_theta, 0.01f);
}

/* ============================================================================
 * Neuromodulator Shift Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, DopamineLowersThreshold) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    neuromodulator_levels_t neuromod = {0};
    neuromod.dopamine = 1.0f;  // High dopamine

    metaplasticity_apply_neuromodulator_shifts(state, &neuromod, &config);

    EXPECT_GT(state->da_shift, 0.0f);  // Positive shift means threshold reduction
}

TEST_F(ExtendedMetaplasticityTest, SerotoninRaisesThreshold) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    neuromodulator_levels_t neuromod = {0};
    neuromod.serotonin = 1.0f;  // High serotonin

    metaplasticity_apply_neuromodulator_shifts(state, &neuromod, &config);

    EXPECT_GT(state->serotonin_shift, 0.0f);  // Serotonin raises threshold
}

TEST_F(ExtendedMetaplasticityTest, NorepinephrineLowersThreshold) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    neuromodulator_levels_t neuromod = {0};
    neuromod.norepinephrine = 1.0f;

    metaplasticity_apply_neuromodulator_shifts(state, &neuromod, &config);

    EXPECT_GT(state->ne_shift, 0.0f);  // NE lowers threshold
}

TEST_F(ExtendedMetaplasticityTest, NeuromodulatorEffectsScaleWithSensitivity) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    neuromodulator_levels_t neuromod = {0};
    neuromod.dopamine = 1.0f;

    // High sensitivity
    config.da_sensitivity = 2.0f;
    metaplasticity_apply_neuromodulator_shifts(state, &neuromod, &config);
    float high_shift = state->da_shift;

    // Low sensitivity
    config.da_sensitivity = 0.5f;
    metaplasticity_apply_neuromodulator_shifts(state, &neuromod, &config);
    float low_shift = state->da_shift;

    EXPECT_GT(high_shift, low_shift);
}

/* ============================================================================
 * Sleep Reset Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, DeepNREMAppliesStrongReset) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    // Elevate threshold
    state->theta_effective = 5.0f;
    state->theta_baseline = 1.0f;

    // Apply deep NREM reset
    metaplasticity_apply_sleep_reset(state, SLEEP_STATE_DEEP_NREM, &config);

    // Threshold should move toward baseline
    EXPECT_LT(state->theta_effective, 5.0f);
    EXPECT_GT(state->sleep_reset_factor, 0.5f);
}

TEST_F(ExtendedMetaplasticityTest, AwakeStateNoReset) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float initial_theta = state->theta_effective;

    metaplasticity_apply_sleep_reset(state, SLEEP_STATE_AWAKE, &config);

    EXPECT_FLOAT_EQ(state->sleep_reset_factor, 0.0f);
}

TEST_F(ExtendedMetaplasticityTest, REMAppliesModerateReset) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    state->theta_effective = 5.0f;
    state->theta_baseline = 1.0f;

    metaplasticity_apply_sleep_reset(state, SLEEP_STATE_REM, &config);

    EXPECT_GT(state->sleep_reset_factor, 0.0f);
    EXPECT_LT(state->sleep_reset_factor, 1.0f);
    EXPECT_LT(state->theta_effective, 5.0f);
}

TEST_F(ExtendedMetaplasticityTest, SleepResetSavesPreSleepThreshold) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    state->theta_effective = 3.0f;
    state->current_sleep_state = SLEEP_STATE_AWAKE;

    // Transition to sleep
    metaplasticity_apply_sleep_reset(state, SLEEP_STATE_LIGHT_NREM, &config);

    EXPECT_FLOAT_EQ(state->pre_sleep_theta, 3.0f);
}

/* ============================================================================
 * History Buffer Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, HistoryBufferStoresActivity) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float activity = 2.0f;
    uint64_t timestamp = 1000;

    metaplasticity_update_history(state, activity, timestamp, &config);

    EXPECT_EQ(state->history_count, 1);
    EXPECT_FLOAT_EQ(state->history[0].activity_squared, activity * activity);
    EXPECT_EQ(state->history[0].timestamp_ms, timestamp);
}

TEST_F(ExtendedMetaplasticityTest, HistoryBufferCircularWrap) {
    config.history_size = 4;
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    // Add more entries than buffer size
    for (uint32_t i = 0; i < 10; i++) {
        metaplasticity_update_history(state, (float)i, i * 1000, &config);
    }

    // Should wrap around
    EXPECT_EQ(state->history_count, config.history_size);
    EXPECT_EQ(state->history_index, 10 % config.history_size);
}

/* ============================================================================
 * Effective Threshold Computation Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, EffectiveThresholdStartsAtBaseline) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float theta = metaplasticity_compute_effective_threshold(state, &config);

    EXPECT_NEAR(theta, config.initial_theta_baseline, 0.1f);
}

TEST_F(ExtendedMetaplasticityTest, EffectiveThresholdIncorporatesHistory) {
    config.enable_long_term_history = true;
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    /* Verify history is enabled */
    ASSERT_NE(state->history, nullptr);
    ASSERT_GT(state->history_size, 0u);

    /* Add high activity to history using realistic timestamps
     * Use current time as base so entries are "recent" for decay calculation */
    uint64_t now = nimcp_time_get_ms();
    for (int i = 0; i < 10; i++) {
        /* Recent entries (within last 10 seconds) */
        int result = metaplasticity_update_history(state, 5.0f, now - (10 - i) * 1000, &config);
        EXPECT_EQ(result, 0);
    }

    /* Verify history was actually added */
    ASSERT_EQ(state->history_count, 10u);

    float theta = metaplasticity_compute_effective_threshold(state, &config);

    /* History contribution: activity=5.0, squared=25.0
     * With 20% contribution factor: theta = 1.0 + 25.0 * 0.2 = 6.0 (before clamping)
     * With neuromod_factor = 1.0, should be > initial baseline */
    EXPECT_GT(theta, config.initial_theta_baseline);
}

/* ============================================================================
 * Full Update Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, FullUpdateSucceeds) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float activity = 2.0f;
    float dt = 100.0f;
    neuromodulator_levels_t neuromod = {0};
    neuromod.dopamine = 0.5f;

    int result = metaplasticity_update(state, activity, &neuromod, dt, &config);

    EXPECT_EQ(result, 0);
}

TEST_F(ExtendedMetaplasticityTest, FullUpdateAppliesAllComponents) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float initial_baseline = state->theta_baseline;

    neuromodulator_levels_t neuromod = {0};
    neuromod.dopamine = 1.0f;

    // Update with activity
    for (int i = 0; i < 50; i++) {
        metaplasticity_update(state, 3.0f, &neuromod, 100.0f, &config);
    }

    // Baseline should have changed
    EXPECT_NE(state->theta_baseline, initial_baseline);
    // DA shift should be applied
    EXPECT_GT(state->da_shift, 0.0f);
}

/* ============================================================================
 * Query Function Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, GetThresholdReturnsEffective) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    state->theta_effective = 2.5f;

    EXPECT_FLOAT_EQ(metaplasticity_get_threshold(state), 2.5f);
}

TEST_F(ExtendedMetaplasticityTest, GetBaselineReturnsBaseline) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    state->theta_baseline = 1.5f;

    EXPECT_FLOAT_EQ(metaplasticity_get_baseline(state), 1.5f);
}

TEST_F(ExtendedMetaplasticityTest, WillInduceLTPWhenAboveThreshold) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    state->theta_effective = 2.0f;

    EXPECT_TRUE(metaplasticity_will_induce_ltp(state, 3.0f));
    EXPECT_FALSE(metaplasticity_will_induce_ltp(state, 1.0f));
}

TEST_F(ExtendedMetaplasticityTest, PlasticityRateScalesWithDistance) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    state->theta_effective = 2.0f;
    float base_rate = 0.01f;

    float rate_near = metaplasticity_get_plasticity_rate(state, 2.1f, base_rate);
    float rate_far = metaplasticity_get_plasticity_rate(state, 5.0f, base_rate);

    EXPECT_GT(rate_far, rate_near);
}

/* ============================================================================
 * Controller Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, ControllerCreationSucceeds) {
    controller = metaplasticity_controller_create(&config, 100);

    ASSERT_NE(controller, nullptr);
}

TEST_F(ExtendedMetaplasticityTest, ControllerUpdateAllSucceeds) {
    controller = metaplasticity_controller_create(&config, 10);
    ASSERT_NE(controller, nullptr);

    float activities[10];
    for (int i = 0; i < 10; i++) {
        activities[i] = (float)i / 10.0f;
    }

    neuromodulator_levels_t neuromod = {0};
    int result = metaplasticity_controller_update_all(controller, activities,
                                                       &neuromod, 100.0f);

    EXPECT_EQ(result, 0);
}

TEST_F(ExtendedMetaplasticityTest, ControllerGetStatsSucceeds) {
    controller = metaplasticity_controller_create(&config, 50);
    ASSERT_NE(controller, nullptr);

    metaplasticity_stats_t stats;
    int result = metaplasticity_controller_get_stats(controller, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.mean_theta_baseline, 0.0f);
}

TEST_F(ExtendedMetaplasticityTest, ControllerSetSleepStateAppliesReset) {
    controller = metaplasticity_controller_create(&config, 20);
    ASSERT_NE(controller, nullptr);

    int result = metaplasticity_controller_set_sleep_state(controller,
                                                            SLEEP_STATE_DEEP_NREM);

    EXPECT_EQ(result, 0);

    metaplasticity_stats_t stats;
    metaplasticity_controller_get_stats(controller, &stats);
    EXPECT_GT(stats.sleep_resets, 0);
}

/* ============================================================================
 * Module Management Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, ModuleInitializationSucceeds) {
    bool result = metaplasticity_module_init(&config);
    EXPECT_TRUE(result);

    metaplasticity_module_destroy();
}

TEST_F(ExtendedMetaplasticityTest, ModuleInitializationWithNullConfigSucceeds) {
    bool result = metaplasticity_module_init(nullptr);
    EXPECT_TRUE(result);

    metaplasticity_module_destroy();
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(ExtendedMetaplasticityTest, NullStateHandledGracefully) {
    float theta = metaplasticity_get_threshold(nullptr);
    EXPECT_GT(theta, 0.0f);  // Returns default

    bool ltp = metaplasticity_will_induce_ltp(nullptr, 5.0f);
    EXPECT_FALSE(ltp);  // Returns safe default
}

TEST_F(ExtendedMetaplasticityTest, ZeroTimestepSkipsUpdate) {
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    float initial_baseline = state->theta_baseline;

    metaplasticity_update_baseline(state, 5.0f, 0.0f, &config);

    EXPECT_FLOAT_EQ(state->theta_baseline, initial_baseline);
}

TEST_F(ExtendedMetaplasticityTest, DisabledHistorySkipsHistoryBuffer) {
    config.enable_long_term_history = false;
    state = metaplasticity_state_create(&config);
    ASSERT_NE(state, nullptr);

    metaplasticity_update_history(state, 5.0f, 1000, &config);

    // Should gracefully skip (no crash)
    SUCCEED();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
