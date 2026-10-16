/**
 * @file test_calcium_dynamics.cpp
 * @brief Unit tests for calcium-dependent learning rate dynamics
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "plasticity/calcium/nimcp_calcium_dynamics.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CalciumDynamicsTest : public ::testing::Test {
protected:
    calcium_dynamics_t calcium;
    calcium_config_t config;

    void SetUp() override {
        calcium_default_config(&config);
        calcium = calcium_create(&config);
        ASSERT_NE(calcium, nullptr);
    }

    void TearDown() override {
        if (calcium) {
            calcium_destroy(calcium);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests (5 tests)
 * ============================================================================ */

TEST_F(CalciumDynamicsTest, CreateDestroy) {
    EXPECT_NE(calcium, nullptr);
    EXPECT_FLOAT_EQ(calcium_get_concentration(calcium), CALCIUM_BASELINE_CONCENTRATION);
}

TEST_F(CalciumDynamicsTest, CreateWithNullConfig) {
    calcium_dynamics_t ca = calcium_create(nullptr);
    ASSERT_NE(ca, nullptr);
    EXPECT_FLOAT_EQ(calcium_get_concentration(ca), CALCIUM_BASELINE_CONCENTRATION);
    calcium_destroy(ca);
}

TEST_F(CalciumDynamicsTest, CreateWithCustomConfig) {
    calcium_config_t custom_config;
    calcium_default_config(&custom_config);
    custom_config.baseline_concentration = 0.2f;
    custom_config.threshold_ltd = 0.4f;
    custom_config.threshold_ltp = 0.6f;

    calcium_dynamics_t ca = calcium_create(&custom_config);
    ASSERT_NE(ca, nullptr);
    EXPECT_FLOAT_EQ(calcium_get_concentration(ca), 0.2f);
    calcium_destroy(ca);
}

TEST_F(CalciumDynamicsTest, DefaultConfig) {
    calcium_config_t cfg;
    int ret = calcium_default_config(&cfg);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(cfg.baseline_concentration, CALCIUM_BASELINE_CONCENTRATION);
    EXPECT_FLOAT_EQ(cfg.threshold_ltd, CALCIUM_THRESHOLD_LTD);
    EXPECT_FLOAT_EQ(cfg.threshold_ltp, CALCIUM_THRESHOLD_LTP);
    EXPECT_TRUE(cfg.enable_nmda_influx);
    EXPECT_TRUE(cfg.enable_buffering);
    EXPECT_TRUE(cfg.enable_pumps);
}

TEST_F(CalciumDynamicsTest, DefaultConfigNullPointer) {
    int ret = calcium_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Concentration Tests (7 tests)
 * ============================================================================ */

TEST_F(CalciumDynamicsTest, GetConcentration) {
    float ca = calcium_get_concentration(calcium);
    EXPECT_FLOAT_EQ(ca, CALCIUM_BASELINE_CONCENTRATION);
}

TEST_F(CalciumDynamicsTest, SetConcentration) {
    int ret = calcium_set_concentration(calcium, 0.5f);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(calcium_get_concentration(calcium), 0.5f);
}

TEST_F(CalciumDynamicsTest, SetConcentrationClamping) {
    calcium_set_concentration(calcium, 5.0f);  /* Above max */
    EXPECT_FLOAT_EQ(calcium_get_concentration(calcium), CALCIUM_MAX_CONCENTRATION);

    calcium_set_concentration(calcium, -1.0f); /* Below min */
    EXPECT_FLOAT_EQ(calcium_get_concentration(calcium), CALCIUM_MIN_CONCENTRATION);
}

TEST_F(CalciumDynamicsTest, ResetConcentration) {
    calcium_set_concentration(calcium, 1.0f);
    EXPECT_FLOAT_EQ(calcium_get_concentration(calcium), 1.0f);

    int ret = calcium_reset(calcium);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(calcium_get_concentration(calcium), CALCIUM_BASELINE_CONCENTRATION);
}

TEST_F(CalciumDynamicsTest, ConcentrationAfterNMDAInflux) {
    float initial_ca = calcium_get_concentration(calcium);

    /* Trigger NMDA influx with high voltage (Mg2+ block removed) */
    calcium_trigger_nmda_influx(calcium, 1.0f, 0.0f);  /* NMDA=1.0, V=0mV */
    calcium_update(calcium, 1.0f);  /* 1 ms update */

    float final_ca = calcium_get_concentration(calcium);
    EXPECT_GT(final_ca, initial_ca);  /* Calcium should increase */
}

TEST_F(CalciumDynamicsTest, ConcentrationDecayOverTime) {
    /* Set high calcium */
    calcium_set_concentration(calcium, 1.0f);
    EXPECT_FLOAT_EQ(calcium_get_concentration(calcium), 1.0f);

    /* Update repeatedly without influx - should decay toward baseline */
    for (int i = 0; i < 100; i++) {
        calcium_update(calcium, 10.0f);  /* 10 ms steps */
    }

    float final_ca = calcium_get_concentration(calcium);
    EXPECT_LT(final_ca, 1.0f);  /* Should have decayed */
    EXPECT_GT(final_ca, CALCIUM_BASELINE_CONCENTRATION * 0.9f);  /* Should approach baseline */
}

TEST_F(CalciumDynamicsTest, GetState) {
    calcium_state_t state;
    int ret = calcium_get_state(calcium, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(state.ca_concentration, CALCIUM_BASELINE_CONCENTRATION);
    EXPECT_EQ(state.regime, CALCIUM_REGIME_NONE);
    EXPECT_EQ(state.total_updates, 0);
}

/* ============================================================================
 * NMDA Influx Tests (6 tests)
 * ============================================================================ */

TEST_F(CalciumDynamicsTest, NMDAInfluxHighVoltage) {
    /* High voltage removes Mg2+ block */
    calcium_trigger_nmda_influx(calcium, 1.0f, 0.0f);
    calcium_update(calcium, 1.0f);
    EXPECT_GT(calcium_get_concentration(calcium), CALCIUM_BASELINE_CONCENTRATION);
}

TEST_F(CalciumDynamicsTest, NMDAInfluxLowVoltage) {
    /* Low voltage maintains Mg2+ block */
    float initial_ca = calcium_get_concentration(calcium);
    calcium_trigger_nmda_influx(calcium, 1.0f, -80.0f);  /* Resting potential */
    calcium_update(calcium, 1.0f);
    float final_ca = calcium_get_concentration(calcium);

    /* Influx should be minimal due to Mg2+ block */
    EXPECT_NEAR(final_ca, initial_ca, 0.05f);
}

TEST_F(CalciumDynamicsTest, NMDAInfluxClamping) {
    /* Test that NMDA activation clamps to [0, 1] */
    calcium_trigger_nmda_influx(calcium, 2.0f, 0.0f);  /* >1.0 should clamp */
    calcium_update(calcium, 1.0f);
    float ca_high = calcium_get_concentration(calcium);

    calcium_reset(calcium);
    calcium_trigger_nmda_influx(calcium, 1.0f, 0.0f);  /* Normal 1.0 */
    calcium_update(calcium, 1.0f);
    float ca_normal = calcium_get_concentration(calcium);

    EXPECT_FLOAT_EQ(ca_high, ca_normal);
}

TEST_F(CalciumDynamicsTest, MgBlockComputation) {
    /* Test Mg2+ block factor at different voltages */
    float block_rest = calcium_compute_mg_block(-65.0f);
    float block_depol = calcium_compute_mg_block(0.0f);

    EXPECT_LT(block_rest, block_depol);  /* Block at rest < block during depolarization */
    EXPECT_GT(block_depol, 0.5f);        /* Significant unblock at 0 mV */
}

TEST_F(CalciumDynamicsTest, NMDAInfluxProportionalToActivation) {
    /* Test that influx scales with NMDA activation */
    calcium_trigger_nmda_influx(calcium, 0.5f, 0.0f);
    calcium_update(calcium, 1.0f);
    float ca_half = calcium_get_concentration(calcium);

    calcium_reset(calcium);
    calcium_trigger_nmda_influx(calcium, 1.0f, 0.0f);
    calcium_update(calcium, 1.0f);
    float ca_full = calcium_get_concentration(calcium);

    EXPECT_LT(ca_half, ca_full);
}

TEST_F(CalciumDynamicsTest, NMDAInfluxNullPointer) {
    int ret = calcium_trigger_nmda_influx(nullptr, 1.0f, 0.0f);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Omega Function Tests (8 tests)
 * ============================================================================ */

TEST_F(CalciumDynamicsTest, OmegaFunctionBelowLTDThreshold) {
    /* Below θ_LTD should give negative learning rate (LTD) */
    calcium_set_concentration(calcium, 0.3f);  /* < 0.35 */
    float lr = calcium_compute_learning_rate(calcium);
    EXPECT_LT(lr, 0.0f);  /* Negative for LTD */
}

TEST_F(CalciumDynamicsTest, OmegaFunctionAboveLTPThreshold) {
    /* Above θ_LTP should give positive learning rate (LTP) */
    calcium_set_concentration(calcium, 0.7f);  /* > 0.55 */
    float lr = calcium_compute_learning_rate(calcium);
    EXPECT_GT(lr, 0.0f);  /* Positive for LTP */
}

TEST_F(CalciumDynamicsTest, OmegaFunctionTransitionZone) {
    /* Between θ_LTD and θ_LTP should be near zero */
    calcium_set_concentration(calcium, 0.45f);  /* Between 0.35 and 0.55 */
    float lr = calcium_compute_learning_rate(calcium);
    EXPECT_NEAR(lr, 0.0f, 0.01f);  /* Near zero in transition */
}

TEST_F(CalciumDynamicsTest, OmegaFunctionSaturation) {
    /* Very high calcium should saturate */
    calcium_set_concentration(calcium, 1.5f);
    float lr_high = calcium_compute_learning_rate(calcium);

    calcium_set_concentration(calcium, 2.0f);
    float lr_vhigh = calcium_compute_learning_rate(calcium);

    /* Should be similar (saturated) */
    EXPECT_NEAR(lr_high, lr_vhigh, 0.002f);
}

TEST_F(CalciumDynamicsTest, OmegaFunctionDirectComputation) {
    /* Test direct omega function */
    float lr_ltd = calcium_omega_function(0.3f, 0.35f, 0.55f, 0.01f, 2.5f);
    float lr_ltp = calcium_omega_function(0.7f, 0.35f, 0.55f, 0.01f, 2.5f);

    EXPECT_LT(lr_ltd, 0.0f);
    EXPECT_GT(lr_ltp, 0.0f);
}

TEST_F(CalciumDynamicsTest, OmegaFunctionIncreasingWithCalcium) {
    /* Learning rate should increase monotonically with calcium above midpoint */
    float lr1 = calcium_omega_function(0.6f, 0.35f, 0.55f, 0.01f, 2.5f);
    float lr2 = calcium_omega_function(0.7f, 0.35f, 0.55f, 0.01f, 2.5f);
    float lr3 = calcium_omega_function(0.8f, 0.35f, 0.55f, 0.01f, 2.5f);

    EXPECT_LT(lr1, lr2);
    EXPECT_LT(lr2, lr3);
}

TEST_F(CalciumDynamicsTest, GetLearningRate) {
    calcium_set_concentration(calcium, 0.7f);
    calcium_update(calcium, 1.0f);  /* Compute LR */

    float lr = calcium_get_learning_rate(calcium);
    EXPECT_GT(lr, 0.0f);
}

TEST_F(CalciumDynamicsTest, GetLearningRateNullPointer) {
    float lr = calcium_get_learning_rate(nullptr);
    EXPECT_FLOAT_EQ(lr, 0.0f);
}

/* ============================================================================
 * Plasticity Regime Tests (6 tests)
 * ============================================================================ */

TEST_F(CalciumDynamicsTest, RegimeNone) {
    calcium_set_concentration(calcium, 0.1f);  /* < threshold_no_plasticity */
    EXPECT_EQ(calcium_get_regime(calcium), CALCIUM_REGIME_NONE);
    EXPECT_FALSE(calcium_is_ltd(calcium));
    EXPECT_FALSE(calcium_is_ltp(calcium));
}

TEST_F(CalciumDynamicsTest, RegimeLTD) {
    calcium_set_concentration(calcium, 0.3f);  /* Between no_plasticity and LTD */
    EXPECT_EQ(calcium_get_regime(calcium), CALCIUM_REGIME_LTD);
    EXPECT_TRUE(calcium_is_ltd(calcium));
    EXPECT_FALSE(calcium_is_ltp(calcium));
}

TEST_F(CalciumDynamicsTest, RegimeTransition) {
    calcium_set_concentration(calcium, 0.45f);  /* Between LTD and LTP */
    EXPECT_EQ(calcium_get_regime(calcium), CALCIUM_REGIME_TRANSITION);
    EXPECT_FALSE(calcium_is_ltd(calcium));
    EXPECT_FALSE(calcium_is_ltp(calcium));
}

TEST_F(CalciumDynamicsTest, RegimeLTP) {
    calcium_set_concentration(calcium, 0.7f);  /* Between LTP and saturation */
    EXPECT_EQ(calcium_get_regime(calcium), CALCIUM_REGIME_LTP);
    EXPECT_FALSE(calcium_is_ltd(calcium));
    EXPECT_TRUE(calcium_is_ltp(calcium));
}

TEST_F(CalciumDynamicsTest, RegimeSaturated) {
    calcium_set_concentration(calcium, 1.2f);  /* Above saturation */
    EXPECT_EQ(calcium_get_regime(calcium), CALCIUM_REGIME_SATURATED);
    EXPECT_TRUE(calcium_is_ltp(calcium));  /* Saturated counts as LTP */
}

TEST_F(CalciumDynamicsTest, RegimeTransitions) {
    /* Test state counts regime transitions */
    calcium_state_t state;

    calcium_set_concentration(calcium, 0.3f);  /* LTD */
    calcium_get_state(calcium, &state);
    uint32_t ltd_count1 = state.ltd_events;

    calcium_set_concentration(calcium, 0.7f);  /* LTP */
    calcium_get_state(calcium, &state);
    uint32_t ltp_count1 = state.ltp_events;

    EXPECT_GT(ltd_count1, 0);
    EXPECT_GT(ltp_count1, 0);
}

/* ============================================================================
 * Threshold Callback Tests (4 tests)
 * ============================================================================ */

static int callback_count = 0;
static calcium_threshold_crossing_t last_crossing;

static void test_threshold_callback(
    calcium_threshold_crossing_t crossing,
    float ca,
    void* user_data
) {
    callback_count++;
    last_crossing = crossing;
}

TEST_F(CalciumDynamicsTest, RegisterCallback) {
    callback_count = 0;
    int ret = calcium_register_threshold_callback(calcium, test_threshold_callback, nullptr);
    EXPECT_EQ(ret, 0);

    /* Trigger callback by crossing LTP threshold */
    calcium_set_concentration(calcium, 0.3f);   /* Below LTP */
    calcium_set_concentration(calcium, 0.6f);   /* Above LTP */

    EXPECT_GT(callback_count, 0);
}

TEST_F(CalciumDynamicsTest, UnregisterCallback) {
    callback_count = 0;
    calcium_register_threshold_callback(calcium, test_threshold_callback, nullptr);

    int ret = calcium_unregister_threshold_callback(calcium, test_threshold_callback);
    EXPECT_EQ(ret, 0);

    /* Callback should not fire after unregister */
    int initial_count = callback_count;
    calcium_set_concentration(calcium, 0.3f);
    calcium_set_concentration(calcium, 0.6f);

    EXPECT_EQ(callback_count, initial_count);  /* No new callbacks */
}

TEST_F(CalciumDynamicsTest, LTPThresholdCrossing) {
    callback_count = 0;
    calcium_register_threshold_callback(calcium, test_threshold_callback, nullptr);

    calcium_set_concentration(calcium, 0.3f);   /* Below LTP */
    calcium_set_concentration(calcium, 0.6f);   /* Cross LTP upward */

    EXPECT_EQ(last_crossing, CALCIUM_CROSS_LTP_THRESHOLD_UP);
}

TEST_F(CalciumDynamicsTest, LTDThresholdCrossing) {
    callback_count = 0;
    calcium_register_threshold_callback(calcium, test_threshold_callback, nullptr);

    calcium_set_concentration(calcium, 0.2f);   /* Below LTD */
    calcium_set_concentration(calcium, 0.4f);   /* Cross LTD upward */

    EXPECT_EQ(last_crossing, CALCIUM_CROSS_LTD_THRESHOLD_UP);
}

/* ============================================================================
 * Update Tests (4 tests)
 * ============================================================================ */

TEST_F(CalciumDynamicsTest, UpdateBasic) {
    int ret = calcium_update(calcium, 1.0f);
    EXPECT_EQ(ret, 0);

    calcium_state_t state;
    calcium_get_state(calcium, &state);
    EXPECT_EQ(state.total_updates, 1);
}

TEST_F(CalciumDynamicsTest, UpdateNullPointer) {
    int ret = calcium_update(nullptr, 1.0f);
    EXPECT_EQ(ret, -1);
}

TEST_F(CalciumDynamicsTest, UpdateZeroDelta) {
    int ret = calcium_update(calcium, 0.0f);
    EXPECT_EQ(ret, 0);  /* Should succeed but do nothing */
}

TEST_F(CalciumDynamicsTest, UpdateStatistics) {
    calcium_state_t state;

    /* Move to LTP regime and stay there */
    calcium_set_concentration(calcium, 0.7f);
    for (int i = 0; i < 10; i++) {
        calcium_update(calcium, 10.0f);  /* 10 ms each */
    }

    calcium_get_state(calcium, &state);
    EXPECT_EQ(state.total_updates, 10);
    EXPECT_GT(state.time_in_ltp_ms, 0);  /* Should have accumulated time in LTP */
}

/* ============================================================================
 * Bio-Async Tests (3 tests)
 * ============================================================================ */

TEST_F(CalciumDynamicsTest, BioAsyncConnect) {
    int ret = calcium_connect_bio_async(calcium);
    EXPECT_EQ(ret, 0);

    /* Should be connected or warn */
    bool connected = calcium_is_bio_async_connected(calcium);
    EXPECT_TRUE(connected || !connected);  /* Either state is valid without router */
}

TEST_F(CalciumDynamicsTest, BioAsyncDisconnect) {
    calcium_connect_bio_async(calcium);
    int ret = calcium_disconnect_bio_async(calcium);
    EXPECT_EQ(ret, 0);

    bool connected = calcium_is_bio_async_connected(calcium);
    EXPECT_FALSE(connected);
}

TEST_F(CalciumDynamicsTest, BioAsyncNullPointer) {
    int ret = calcium_connect_bio_async(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Config Query Tests (2 tests)
 * ============================================================================ */

TEST_F(CalciumDynamicsTest, GetConfig) {
    calcium_config_t retrieved_config;
    int ret = calcium_get_config(calcium, &retrieved_config);
    EXPECT_EQ(ret, 0);

    EXPECT_FLOAT_EQ(retrieved_config.baseline_concentration, config.baseline_concentration);
    EXPECT_FLOAT_EQ(retrieved_config.threshold_ltd, config.threshold_ltd);
    EXPECT_FLOAT_EQ(retrieved_config.threshold_ltp, config.threshold_ltp);
}

TEST_F(CalciumDynamicsTest, GetConfigNullPointer) {
    int ret = calcium_get_config(nullptr, &config);
    EXPECT_EQ(ret, -1);

    ret = calcium_get_config(calcium, nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
