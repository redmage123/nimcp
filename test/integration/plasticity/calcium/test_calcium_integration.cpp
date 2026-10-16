/**
 * @file test_calcium_integration.cpp
 * @brief Integration tests for calcium dynamics with sleep and immune bridges
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "plasticity/calcium/nimcp_calcium_dynamics.h"
#include "plasticity/calcium/nimcp_calcium_sleep_bridge.h"
#include "plasticity/calcium/nimcp_calcium_immune_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CalciumSleepIntegrationTest : public ::testing::Test {
protected:
    calcium_dynamics_t calcium;
    sleep_system_t sleep_system;
    calcium_sleep_bridge_t bridge;

    void SetUp() override {
        calcium = calcium_create(nullptr);
        ASSERT_NE(calcium, nullptr);

        sleep_config_t sleep_config = sleep_default_config();
        sleep_system = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep_system, nullptr);

        bridge = calcium_sleep_bridge_create(nullptr, sleep_system, calcium);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) calcium_sleep_bridge_destroy(bridge);
        if (sleep_system) sleep_system_destroy(sleep_system);
        if (calcium) calcium_destroy(calcium);
    }
};

class CalciumImmuneIntegrationTest : public ::testing::Test {
protected:
    calcium_dynamics_t calcium;
    brain_immune_system_t* immune;
    calcium_immune_bridge_t* bridge;

    void SetUp() override {
        calcium = calcium_create(nullptr);
        ASSERT_NE(calcium, nullptr);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        bridge = calcium_immune_bridge_create(nullptr, immune, calcium);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) calcium_immune_bridge_destroy(bridge);
        if (immune) brain_immune_destroy(immune);
        if (calcium) calcium_destroy(calcium);
    }
};

/* ============================================================================
 * Sleep-Calcium Integration Tests (10 tests)
 * ============================================================================ */

TEST_F(CalciumSleepIntegrationTest, BridgeCreation) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CalciumSleepIntegrationTest, InfluxModulationAwake) {
    sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
    calcium_sleep_update(bridge);

    float influx_factor = calcium_sleep_get_influx_factor(bridge);
    EXPECT_FLOAT_EQ(influx_factor, CALCIUM_SLEEP_INFLUX_AWAKE);
}

TEST_F(CalciumSleepIntegrationTest, InfluxModulationDeepNREM) {
    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    calcium_sleep_update(bridge);

    float influx_factor = calcium_sleep_get_influx_factor(bridge);
    EXPECT_FLOAT_EQ(influx_factor, CALCIUM_SLEEP_INFLUX_DEEP_NREM);
}

TEST_F(CalciumSleepIntegrationTest, DecayModulationByState) {
    sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
    calcium_sleep_update(bridge);
    float decay_awake = calcium_sleep_get_decay_tau(bridge);

    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    calcium_sleep_update(bridge);
    float decay_deep = calcium_sleep_get_decay_tau(bridge);

    EXPECT_GT(decay_awake, decay_deep);  /* Faster decay during deep sleep */
}

TEST_F(CalciumSleepIntegrationTest, LearningRateModulation) {
    sleep_enter_state(sleep_system, SLEEP_STATE_AWAKE);
    calcium_sleep_update(bridge);
    float lr_awake = calcium_sleep_get_learning_rate(bridge, 0.01f);

    sleep_enter_state(sleep_system, SLEEP_STATE_DEEP_NREM);
    calcium_sleep_update(bridge);
    float lr_deep = calcium_sleep_get_learning_rate(bridge, 0.01f);

    EXPECT_GT(lr_awake, lr_deep);  /* Reduced learning during deep sleep */
}

TEST_F(CalciumSleepIntegrationTest, GetEffects) {
    sleep_enter_state(sleep_system, SLEEP_STATE_REM);
    calcium_sleep_update(bridge);

    calcium_sleep_effects_t effects;
    int ret = calcium_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(effects.influx_factor, CALCIUM_SLEEP_INFLUX_REM);
}

TEST_F(CalciumSleepIntegrationTest, AllSleepStates) {
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    for (auto state : states) {
        sleep_enter_state(sleep_system, state);
        calcium_sleep_update(bridge);

        float influx = calcium_sleep_get_influx_factor(bridge);
        EXPECT_GT(influx, 0.0f);
        EXPECT_LE(influx, 1.0f);
    }
}

TEST_F(CalciumSleepIntegrationTest, HelperFunctions) {
    float influx_awake = calcium_sleep_get_influx_factor_for_state(SLEEP_STATE_AWAKE);
    float influx_deep = calcium_sleep_get_influx_factor_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_FLOAT_EQ(influx_awake, CALCIUM_SLEEP_INFLUX_AWAKE);
    EXPECT_FLOAT_EQ(influx_deep, CALCIUM_SLEEP_INFLUX_DEEP_NREM);
}

TEST_F(CalciumSleepIntegrationTest, DecayTauHelpers) {
    float tau_awake = calcium_sleep_get_decay_tau_for_state(SLEEP_STATE_AWAKE);
    float tau_deep = calcium_sleep_get_decay_tau_for_state(SLEEP_STATE_DEEP_NREM);

    EXPECT_FLOAT_EQ(tau_awake, CALCIUM_SLEEP_DECAY_AWAKE);
    EXPECT_FLOAT_EQ(tau_deep, CALCIUM_SLEEP_DECAY_DEEP_NREM);
}

TEST_F(CalciumSleepIntegrationTest, ConfigurationOptions) {
    calcium_sleep_config_t config;
    calcium_sleep_default_config(&config);

    EXPECT_TRUE(config.enable_influx_modulation);
    EXPECT_TRUE(config.enable_decay_modulation);
    EXPECT_TRUE(config.enable_lr_modulation);
}

/* ============================================================================
 * Immune-Calcium Integration Tests (10 tests)
 * ============================================================================ */

TEST_F(CalciumImmuneIntegrationTest, BridgeCreation) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CalciumImmuneIntegrationTest, ApplyCytokineEffects) {
    int ret = calcium_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(ret, 0);

    cytokine_calcium_effects_t effects;
    calcium_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_FLOAT_EQ(effects.total_influx_modulation, 1.0f);  /* Neutral without cytokines */
}

TEST_F(CalciumImmuneIntegrationTest, ApplyInflammationEffects) {
    int ret = calcium_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(ret, 0);

    inflammation_calcium_state_t state;
    calcium_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
}

TEST_F(CalciumImmuneIntegrationTest, GetEffectiveInflux) {
    float influx = calcium_immune_get_effective_influx(bridge);
    EXPECT_GT(influx, 0.0f);
    EXPECT_LE(influx, 2.0f);
}

TEST_F(CalciumImmuneIntegrationTest, GetModulationState) {
    calcium_modulation_state_t modulation;
    int ret = calcium_immune_get_modulation_state(bridge, &modulation);
    EXPECT_EQ(ret, 0);

    EXPECT_GT(modulation.influx_modulation, 0.0f);
    EXPECT_GT(modulation.pump_modulation, 0.0f);
    EXPECT_GT(modulation.buffer_modulation, 0.0f);
}

TEST_F(CalciumImmuneIntegrationTest, DetectInstability) {
    /* Maintain very high calcium (excitotoxic) by re-setting each iteration
     * This simulates continuous calcium influx exceeding clearance */
    for (int i = 0; i < 200; i++) {
        calcium_set_concentration(calcium, 1.8f);
        calcium_update(calcium, 1.0f);
        calcium_immune_detect_instability(bridge);
    }

    calcium_instability_state_t state;
    calcium_immune_get_instability_state(bridge, &state);
    EXPECT_TRUE(state.excitotoxicity_detected);
}

TEST_F(CalciumImmuneIntegrationTest, DetectSynapticFailure) {
    /* Maintain very low calcium by re-setting each iteration
     * This simulates persistent synaptic failure with calcium below threshold */
    for (int i = 0; i < 1500; i++) {
        calcium_set_concentration(calcium, 0.03f);
        calcium_update(calcium, 1.0f);
        calcium_immune_detect_instability(bridge);
    }

    calcium_instability_state_t state;
    calcium_immune_get_instability_state(bridge, &state);
    EXPECT_TRUE(state.synaptic_failure_detected);
}

TEST_F(CalciumImmuneIntegrationTest, HealthyDynamics) {
    /* Keep calcium in healthy range */
    calcium_set_concentration(calcium, 0.5f);

    for (int i = 0; i < 100; i++) {
        calcium_update(calcium, 1.0f);
        calcium_immune_detect_instability(bridge);
    }

    calcium_instability_state_t state;
    calcium_immune_get_instability_state(bridge, &state);
    EXPECT_FALSE(state.excitotoxicity_detected);
    EXPECT_FALSE(state.synaptic_failure_detected);
}

TEST_F(CalciumImmuneIntegrationTest, BridgeUpdate) {
    int ret = calcium_immune_bridge_update(bridge, 10);
    EXPECT_EQ(ret, 0);
}

TEST_F(CalciumImmuneIntegrationTest, BioAsyncIntegration) {
    int ret = calcium_immune_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    ret = calcium_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
