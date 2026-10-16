/**
 * @file test_astrocyte_regression.cpp
 * @brief Regression tests for astrocyte-plasticity stability
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "plasticity/astrocyte/nimcp_astrocyte_plasticity.h"
#include "plasticity/astrocyte/nimcp_astrocyte_sleep_bridge.h"
#include "plasticity/astrocyte/nimcp_astrocyte_immune_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"

class AstrocyteRegressionTest : public ::testing::Test {
protected:
    astrocyte_plasticity_t astro;

    void SetUp() override {
        astrocyte_config_t config;
        astrocyte_plasticity_default_config(&config);
        astro = astrocyte_plasticity_create(&config, 20);
        ASSERT_NE(astro, nullptr);
    }

    void TearDown() override {
        if (astro) {
            astrocyte_plasticity_destroy(astro);
        }
    }
};

/* ============================================================================
 * Stability Tests
 * ============================================================================ */

TEST_F(AstrocyteRegressionTest, MultipleUpdatesStable) {
    // Run 1000 updates
    for (int i = 0; i < 1000; i++) {
        for (uint32_t j = 0; j < 20; j++) {
            EXPECT_EQ(astrocyte_plasticity_update(astro, j, 0.5f, 10), 0);
        }
    }

    // Check all astrocytes still valid
    for (uint32_t i = 0; i < 20; i++) {
        astrocyte_state_t state;
        EXPECT_EQ(astrocyte_plasticity_get_state(astro, i, &state), 0);
        EXPECT_GE(state.d_serine_level, 0.0f);
        EXPECT_LE(state.d_serine_level, 2.0f);
    }
}

TEST_F(AstrocyteRegressionTest, ReactiveStateTransitionStability) {
    astrocyte_reactive_state_t states[] = {
        ASTROCYTE_RESTING,
        ASTROCYTE_A1_REACTIVE,
        ASTROCYTE_A2_REACTIVE,
        ASTROCYTE_MIXED_REACTIVE,
        ASTROCYTE_RESTING
    };

    // Cycle through states multiple times
    for (int cycle = 0; cycle < 10; cycle++) {
        for (size_t s = 0; s < sizeof(states)/sizeof(states[0]); s++) {
            EXPECT_EQ(astrocyte_plasticity_set_reactive_state(
                astro, 0, states[s], 0.8f), 0);

            astrocyte_state_t state;
            astrocyte_plasticity_get_state(astro, 0, &state);
            EXPECT_EQ(state.reactive_state, states[s]);
        }
    }
}

TEST_F(AstrocyteRegressionTest, CalciumWavePropagationDoesNotCrash) {
    for (int i = 0; i < 100; i++) {
        uint32_t source = i % 20;
        astrocyte_plasticity_trigger_calcium_wave(astro, source, 0.9f);
        astrocyte_plasticity_update(astro, source, 0.0f, 10);
    }
}

TEST_F(AstrocyteRegressionTest, ExcessiveGlutamateDoesNotOverflow) {
    for (int i = 0; i < 100; i++) {
        astrocyte_plasticity_notify_glutamate_release(astro, 0, 10.0f);
    }

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);
    EXPECT_LE(state.calcium_current, 10.0f);  // Should be clamped
}

TEST_F(AstrocyteRegressionTest, GliotransmitterReleaseStability) {
    gliotransmitter_type_t types[] = {
        GLIOTRANSMITTER_D_SERINE,
        GLIOTRANSMITTER_ATP,
        GLIOTRANSMITTER_ADENOSINE,
        GLIOTRANSMITTER_GLUTAMATE
    };

    for (int i = 0; i < 100; i++) {
        for (size_t t = 0; t < sizeof(types)/sizeof(types[0]); t++) {
            EXPECT_EQ(astrocyte_plasticity_release_gliotransmitter(
                astro, i % 20, types[t], 0.5f), 0);
        }
    }
}

/* ============================================================================
 * Boundary Condition Tests
 * ============================================================================ */

TEST_F(AstrocyteRegressionTest, ZeroActivityDoesNotCrash) {
    for (int i = 0; i < 100; i++) {
        astrocyte_plasticity_update(astro, 0, 0.0f, 100);
    }

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);
    EXPECT_LT(state.calcium_current, 0.5f);
}

TEST_F(AstrocyteRegressionTest, MaxActivityDoesNotOverflow) {
    for (int i = 0; i < 100; i++) {
        astrocyte_plasticity_update(astro, 0, 1.0f, 100);
    }

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);
    // Should saturate, not overflow
    EXPECT_LT(state.calcium_current, 5.0f);
}

TEST_F(AstrocyteRegressionTest, RapidStateTransitions) {
    for (int i = 0; i < 1000; i++) {
        astrocyte_reactive_state_t state =
            (i % 2 == 0) ? ASTROCYTE_A1_REACTIVE : ASTROCYTE_A2_REACTIVE;
        astrocyte_plasticity_set_reactive_state(astro, 0, state, 1.0f);
    }

    astrocyte_state_t final_state;
    astrocyte_plasticity_get_state(astro, 0, &final_state);
    EXPECT_GE(final_state.d_serine_level, 0.0f);
}

/* ============================================================================
 * Bridge Regression Tests
 * ============================================================================ */

TEST(AstrocyteBridgeRegression, SleepBridgeMultipleUpdates) {
    astrocyte_config_t astro_config;
    astrocyte_plasticity_default_config(&astro_config);
    astrocyte_plasticity_t astro = astrocyte_plasticity_create(&astro_config, 5);
    ASSERT_NE(astro, nullptr);

    sleep_config_t sleep_config = sleep_default_config();
    sleep_system_t sleep_sys = sleep_system_create(&sleep_config);
    ASSERT_NE(sleep_sys, nullptr);

    astrocyte_sleep_config_t bridge_config;
    astrocyte_sleep_default_config(&bridge_config);
    astrocyte_sleep_bridge_t bridge =
        astrocyte_sleep_bridge_create(&bridge_config, sleep_sys, astro);
    ASSERT_NE(bridge, nullptr);

    // Cycle through sleep states
    sleep_state_t states[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    for (int cycle = 0; cycle < 10; cycle++) {
        for (size_t s = 0; s < sizeof(states)/sizeof(states[0]); s++) {
            sleep_enter_state(sleep_sys, states[s]);
            EXPECT_EQ(astrocyte_sleep_update(bridge), 0);
            EXPECT_EQ(astrocyte_sleep_apply_modulation(bridge), 0);
        }
    }

    astrocyte_sleep_bridge_destroy(bridge);
    sleep_system_destroy(sleep_sys);
    astrocyte_plasticity_destroy(astro);
}

TEST(AstrocyteBridgeRegression, ImmuneBridgeStability) {
    astrocyte_config_t astro_config;
    astrocyte_plasticity_default_config(&astro_config);
    astrocyte_plasticity_t astro = astrocyte_plasticity_create(&astro_config, 5);
    ASSERT_NE(astro, nullptr);

    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    brain_immune_system_t* brain_immune = brain_immune_create(&immune_config);
    ASSERT_NE(brain_immune, nullptr);

    astrocyte_immune_config_t bridge_config;
    astrocyte_immune_default_config(&bridge_config);
    astrocyte_immune_bridge_t* bridge =
        astrocyte_immune_bridge_create(&bridge_config, brain_immune, astro);
    ASSERT_NE(bridge, nullptr);

    // Run multiple updates
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(astrocyte_immune_bridge_update(bridge, 100), 0);
    }

    astrocyte_immune_bridge_destroy(bridge);
    brain_immune_destroy(brain_immune);
    astrocyte_plasticity_destroy(astro);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
