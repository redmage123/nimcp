/**
 * @file test_homeostatic_immune_integration.cpp
 * @brief Unit tests for Homeostatic Plasticity-Immune Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Comprehensive test suite for homeostatic-immune bidirectional coupling
 * WHY:  Verify biological accuracy of immune-homeostasis interactions
 * HOW:  Test cytokine effects, inflammation disruption, instability triggers, recovery boosts
 */

#include <gtest/gtest.h>
#include "plasticity/immune/nimcp_homeostatic_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class HomeostaticImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    homeostatic_controller_t homeostatic_controller;
    homeostatic_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);

        /* Create homeostatic controller */
        homeostatic_config_t homeo_cfg = homeostatic_config_default();
        homeostatic_controller = homeostatic_controller_create(&homeo_cfg, 100);
        ASSERT_NE(homeostatic_controller, nullptr);

        /* Create bridge with defaults */
        bridge = homeostatic_immune_bridge_create(
            nullptr,
            immune_system,
            homeostatic_controller
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            homeostatic_immune_bridge_destroy(bridge);
        }
        if (homeostatic_controller) {
            homeostatic_controller_destroy(homeostatic_controller);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, DefaultConfigInitialization) {
    homeostatic_immune_config_t config;
    int result = homeostatic_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_homeostasis_modulation);
    EXPECT_TRUE(config.enable_inflammation_disruption);
    EXPECT_TRUE(config.enable_instability_immune_trigger);
    EXPECT_TRUE(config.enable_recovery_immune_boost);
    EXPECT_TRUE(config.enable_tnf_biphasic_effect);

    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.instability_trigger_sensitivity, 1.0f);

    EXPECT_GT(config.baseline_target_rate, 0.0f);
    EXPECT_GT(config.baseline_scaling_factor, 0.0f);
    EXPECT_GE(config.baseline_threshold, 0.0f);
}

TEST_F(HomeostaticImmuneTest, BridgeCreationWithNullConfig) {
    /* Bridge created in SetUp with null config - verify it works */
    EXPECT_NE(bridge, nullptr);

    /* Verify baseline parameters are initialized */
    float scaling = homeostatic_immune_get_current_scaling_factor(bridge);
    float target = homeostatic_immune_get_current_target_rate(bridge);
    float threshold = homeostatic_immune_get_current_threshold(bridge);

    EXPECT_GT(scaling, 0.0f);
    EXPECT_GT(target, 0.0f);
    EXPECT_GE(threshold, 0.0f);
}

TEST_F(HomeostaticImmuneTest, BridgeCreationWithCustomConfig) {
    homeostatic_immune_config_t config;
    homeostatic_immune_default_config(&config);

    /* Customize */
    config.baseline_scaling_factor = 1.5f;
    config.baseline_target_rate = 8.0f;
    config.baseline_threshold = 0.6f;

    homeostatic_immune_bridge_t* custom_bridge =
        homeostatic_immune_bridge_create(
            &config,
            immune_system,
            homeostatic_controller
        );
    ASSERT_NE(custom_bridge, nullptr);

    /* Verify custom baseline */
    float scaling = homeostatic_immune_get_current_scaling_factor(custom_bridge);
    float target = homeostatic_immune_get_current_target_rate(custom_bridge);
    float threshold = homeostatic_immune_get_current_threshold(custom_bridge);

    EXPECT_FLOAT_EQ(scaling, 1.5f);
    EXPECT_FLOAT_EQ(target, 8.0f);
    EXPECT_FLOAT_EQ(threshold, 0.6f);

    homeostatic_immune_bridge_destroy(custom_bridge);
}

TEST_F(HomeostaticImmuneTest, BridgeCreationFailsWithoutImmuneSystem) {
    homeostatic_immune_bridge_t* bad_bridge =
        homeostatic_immune_bridge_create(
            nullptr,
            nullptr,  /* No immune system */
            homeostatic_controller
        );
    EXPECT_EQ(bad_bridge, nullptr);
}

TEST_F(HomeostaticImmuneTest, BridgeCreationFailsWithoutHomeostaticController) {
    homeostatic_immune_bridge_t* bad_bridge =
        homeostatic_immune_bridge_create(
            nullptr,
            immune_system,
            nullptr  /* No homeostatic controller */
        );
    EXPECT_EQ(bad_bridge, nullptr);
}

TEST_F(HomeostaticImmuneTest, BridgeDestruction) {
    homeostatic_immune_bridge_t* temp_bridge =
        homeostatic_immune_bridge_create(
            nullptr,
            immune_system,
            homeostatic_controller
        );
    ASSERT_NE(temp_bridge, nullptr);

    /* Should not crash */
    homeostatic_immune_bridge_destroy(temp_bridge);
    homeostatic_immune_bridge_destroy(nullptr);  /* Null is safe */
}

/* ============================================================================
 * Immune → Homeostasis: Cytokine Effects Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, ApplyCytokineEffectsBasic) {
    int result = homeostatic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Verify effects structure is updated */
    cytokine_homeostatic_effects_t effects;
    result = homeostatic_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    /* Initially should be zero (no cytokines yet) */
    EXPECT_GE(effects.homeostatic_disruption_level, 0.0f);
    EXPECT_LE(effects.homeostatic_disruption_level, 1.0f);
}

TEST_F(HomeostaticImmuneTest, TNFBiphasicEffectLowLevel) {
    /* Low TNF-α should enhance scaling */
    float low_tnf = 0.2f;
    float effect = homeostatic_immune_compute_tnf_biphasic(bridge, low_tnf);

    EXPECT_GT(effect, 0.0f);  /* Positive enhancement */
    EXPECT_LE(effect, CYTOKINE_TNF_SCALING_IMPACT);
}

TEST_F(HomeostaticImmuneTest, TNFBiphasicEffectHighLevel) {
    /* High TNF-α should suppress scaling */
    float high_tnf = 0.9f;
    float effect = homeostatic_immune_compute_tnf_biphasic(bridge, high_tnf);

    EXPECT_LT(effect, 0.0f);  /* Negative suppression */
    EXPECT_GE(effect, -CYTOKINE_TNF_SCALING_IMPACT);
}

TEST_F(HomeostaticImmuneTest, TNFBiphasicEffectOptimalRange) {
    /* Medium TNF-α should have minimal effect */
    float medium_tnf = 0.5f;
    float effect = homeostatic_immune_compute_tnf_biphasic(bridge, medium_tnf);

    EXPECT_NEAR(effect, 0.0f, 0.1f);  /* Near zero in optimal range */
}

TEST_F(HomeostaticImmuneTest, CytokineEffectsModulateScalingFactor) {
    /* Get initial scaling factor */
    float initial_scaling =
        homeostatic_immune_get_current_scaling_factor(bridge);

    /* Apply cytokine effects (even with zero cytokines, should complete) */
    int result = homeostatic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Verify scaling factor is still valid */
    float current_scaling =
        homeostatic_immune_get_current_scaling_factor(bridge);
    EXPECT_GT(current_scaling, 0.0f);
    EXPECT_LE(current_scaling, 2.0f);
}

TEST_F(HomeostaticImmuneTest, CytokineEffectsModulateTargetRate) {
    /* Get initial target rate */
    float initial_target =
        homeostatic_immune_get_current_target_rate(bridge);
    EXPECT_GT(initial_target, 0.0f);

    /* Apply cytokine effects */
    int result = homeostatic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Verify target rate is still valid */
    float current_target =
        homeostatic_immune_get_current_target_rate(bridge);
    EXPECT_GT(current_target, 0.0f);
    EXPECT_LE(current_target, 20.0f);
}

TEST_F(HomeostaticImmuneTest, CytokineEffectsModulateThreshold) {
    /* Get initial threshold */
    float initial_threshold =
        homeostatic_immune_get_current_threshold(bridge);
    EXPECT_GE(initial_threshold, 0.0f);

    /* Apply cytokine effects */
    int result = homeostatic_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Verify threshold is still valid */
    float current_threshold =
        homeostatic_immune_get_current_threshold(bridge);
    EXPECT_GE(current_threshold, 0.0f);
    EXPECT_LE(current_threshold, 1.0f);
}

/* ============================================================================
 * Immune → Homeostasis: Inflammation Effects Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, ApplyInflammationEffectsBasic) {
    int result = homeostatic_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    /* Verify inflammation state is updated */
    inflammation_homeostatic_state_t state;
    result = homeostatic_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);

    EXPECT_GE(state.scaling_disruption, 0.0f);
    EXPECT_LE(state.scaling_disruption, 1.0f);
}

TEST_F(HomeostaticImmuneTest, ChronicInflammationDetection) {
    /* Apply inflammation effects */
    int result = homeostatic_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_homeostatic_state_t state;
    homeostatic_immune_get_inflammation_state(bridge, &state);

    /* Initially should not be chronic */
    EXPECT_FALSE(state.is_chronic);
}

TEST_F(HomeostaticImmuneTest, InflammationDisruptionLevel) {
    /* Apply inflammation effects */
    homeostatic_immune_apply_inflammation_effects(bridge);

    inflammation_homeostatic_state_t state;
    homeostatic_immune_get_inflammation_state(bridge, &state);

    /* Check disruption metrics are in valid range */
    EXPECT_GE(state.scaling_disruption, 0.0f);
    EXPECT_LE(state.scaling_disruption, 1.0f);
    EXPECT_GE(state.setpoint_shift, 0.0f);
    EXPECT_LE(state.setpoint_shift, INFLAMMATION_SETPOINT_SHIFT_MAX);
    EXPECT_GE(state.adaptation_impairment, 0.0f);
    EXPECT_LE(state.adaptation_impairment, 1.0f);
}

TEST_F(HomeostaticImmuneTest, HomeostaticFailureDetection) {
    /* Initially should not have failure */
    bool failure = homeostatic_immune_is_homeostatic_failure(bridge);
    EXPECT_FALSE(failure);
}

TEST_F(HomeostaticImmuneTest, GetDisruptionLevel) {
    float disruption = homeostatic_immune_get_disruption_level(bridge);

    EXPECT_GE(disruption, 0.0f);
    EXPECT_LE(disruption, 1.0f);
}

/* ============================================================================
 * Homeostasis → Immune: Instability Trigger Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, DetectHyperexcitabilityNormal) {
    /* Normal firing rates (around target) */
    const uint32_t num_neurons = 100;
    float firing_rates[100];
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 5.0f;  /* Target rate */
    }

    float hyperexc = homeostatic_immune_detect_hyperexcitability(
        bridge, firing_rates, num_neurons
    );

    EXPECT_NEAR(hyperexc, 0.0f, 0.1f);  /* Should be near zero */
}

TEST_F(HomeostaticImmuneTest, DetectHyperexcitabilityElevated) {
    /* Elevated firing rates (2x target) */
    const uint32_t num_neurons = 100;
    float firing_rates[100];
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 10.0f;  /* 2x target rate */
    }

    float hyperexc = homeostatic_immune_detect_hyperexcitability(
        bridge, firing_rates, num_neurons
    );

    EXPECT_GT(hyperexc, 0.0f);  /* Should detect hyperexcitability */
    EXPECT_LE(hyperexc, 1.0f);
}

TEST_F(HomeostaticImmuneTest, DetectHyperexcitabilityExtreme) {
    /* Extreme firing rates (4x target) */
    const uint32_t num_neurons = 100;
    float firing_rates[100];
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 20.0f;  /* 4x target rate */
    }

    float hyperexc = homeostatic_immune_detect_hyperexcitability(
        bridge, firing_rates, num_neurons
    );

    EXPECT_GT(hyperexc, 0.5f);  /* Should be high */
    EXPECT_LE(hyperexc, 1.0f);
}

TEST_F(HomeostaticImmuneTest, DetectHyperexcitabilityLow) {
    /* Low firing rates (below target) */
    const uint32_t num_neurons = 100;
    float firing_rates[100];
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 2.0f;  /* Below target */
    }

    float hyperexc = homeostatic_immune_detect_hyperexcitability(
        bridge, firing_rates, num_neurons
    );

    EXPECT_NEAR(hyperexc, 0.0f, 0.01f);  /* Should be zero (not hyperexcitable) */
}

TEST_F(HomeostaticImmuneTest, DetectScalingFailureSuccess) {
    /* Successful scaling */
    float failure = homeostatic_immune_detect_scaling_failure(bridge, true);

    EXPECT_NEAR(failure, 0.0f, 0.01f);
}

TEST_F(HomeostaticImmuneTest, DetectScalingFailureSingle) {
    /* Single failure */
    float failure = homeostatic_immune_detect_scaling_failure(bridge, false);

    EXPECT_GT(failure, 0.0f);
    EXPECT_LE(failure, 1.0f);
}

TEST_F(HomeostaticImmuneTest, DetectScalingFailureConsecutive) {
    /* Multiple consecutive failures */
    for (int i = 0; i < 5; i++) {
        homeostatic_immune_detect_scaling_failure(bridge, false);
    }

    float failure = homeostatic_immune_detect_scaling_failure(bridge, false);

    EXPECT_GT(failure, 0.5f);  /* Should be high after many failures */
    EXPECT_LE(failure, 1.0f);
}

TEST_F(HomeostaticImmuneTest, TriggerFromInstabilityLow) {
    /* Low instability (normal firing) */
    const uint32_t num_neurons = 100;
    float firing_rates[100];
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 5.0f;  /* Normal */
    }

    int result = homeostatic_immune_trigger_from_instability(
        bridge, firing_rates, num_neurons
    );

    EXPECT_EQ(result, 0);
}

TEST_F(HomeostaticImmuneTest, TriggerFromInstabilityHigh) {
    /* High instability (hyperexcitable) */
    const uint32_t num_neurons = 100;
    float firing_rates[100];
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 20.0f;  /* Very high */
    }

    /* Also add scaling failures */
    for (int i = 0; i < 5; i++) {
        homeostatic_immune_detect_scaling_failure(bridge, false);
    }

    int result = homeostatic_immune_trigger_from_instability(
        bridge, firing_rates, num_neurons
    );

    EXPECT_EQ(result, 0);
    /* Note: actual immune trigger depends on threshold */
}

/* ============================================================================
 * Homeostasis → Immune: Recovery Boost Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, BoostFromRecoveryUnstable) {
    /* Not stable - should not boost */
    int result = homeostatic_immune_boost_from_recovery(bridge, false);
    EXPECT_EQ(result, 0);
}

TEST_F(HomeostaticImmuneTest, BoostFromRecoveryStable) {
    /* Stable - should boost immune resolution */
    int result = homeostatic_immune_boost_from_recovery(bridge, true);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, BridgeUpdateBasic) {
    const uint32_t num_neurons = 100;
    float firing_rates[100];
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 5.0f;  /* Normal */
    }

    int result = homeostatic_immune_bridge_update(
        bridge,
        firing_rates,
        num_neurons,
        true,   /* is_stable */
        true,   /* scaling_success */
        100     /* delta_ms */
    );

    EXPECT_EQ(result, 0);
}

TEST_F(HomeostaticImmuneTest, BridgeUpdateWithInstability) {
    const uint32_t num_neurons = 100;
    float firing_rates[100];
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 20.0f;  /* Hyperexcitable */
    }

    int result = homeostatic_immune_bridge_update(
        bridge,
        firing_rates,
        num_neurons,
        false,  /* is_stable */
        false,  /* scaling_success */
        100     /* delta_ms */
    );

    EXPECT_EQ(result, 0);
}

TEST_F(HomeostaticImmuneTest, BridgeUpdateMultipleCycles) {
    const uint32_t num_neurons = 100;
    float firing_rates[100];

    /* Run multiple update cycles */
    for (int cycle = 0; cycle < 10; cycle++) {
        /* Vary firing rates */
        for (uint32_t i = 0; i < num_neurons; i++) {
            firing_rates[i] = 5.0f + (float)(cycle % 3);
        }

        int result = homeostatic_immune_bridge_update(
            bridge,
            firing_rates,
            num_neurons,
            (cycle % 2 == 0),  /* Alternate stable/unstable */
            (cycle % 3 != 0),  /* Occasional failures */
            100
        );

        EXPECT_EQ(result, 0);
    }
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, GetCytokineEffects) {
    cytokine_homeostatic_effects_t effects;
    int result = homeostatic_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_GE(effects.homeostatic_disruption_level, 0.0f);
    EXPECT_LE(effects.homeostatic_disruption_level, 1.0f);
}

TEST_F(HomeostaticImmuneTest, GetInflammationState) {
    inflammation_homeostatic_state_t state;
    int result = homeostatic_immune_get_inflammation_state(bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_GE(state.scaling_disruption, 0.0f);
    EXPECT_LE(state.scaling_disruption, 1.0f);
}

TEST_F(HomeostaticImmuneTest, GetCurrentParameters) {
    float scaling = homeostatic_immune_get_current_scaling_factor(bridge);
    float target = homeostatic_immune_get_current_target_rate(bridge);
    float threshold = homeostatic_immune_get_current_threshold(bridge);

    EXPECT_GT(scaling, 0.0f);
    EXPECT_GT(target, 0.0f);
    EXPECT_GE(threshold, 0.0f);

    /* All should be in valid ranges */
    EXPECT_LE(scaling, 2.0f);
    EXPECT_LE(target, 20.0f);
    EXPECT_LE(threshold, 1.0f);
}

TEST_F(HomeostaticImmuneTest, GetCytokineEffectsNullBridge) {
    cytokine_homeostatic_effects_t effects;
    int result = homeostatic_immune_get_cytokine_effects(nullptr, &effects);
    EXPECT_EQ(result, -1);
}

TEST_F(HomeostaticImmuneTest, GetCytokineEffectsNullOutput) {
    int result = homeostatic_immune_get_cytokine_effects(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Edge Cases and Error Handling Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, ApplyCytokineEffectsNullBridge) {
    int result = homeostatic_immune_apply_cytokine_effects(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HomeostaticImmuneTest, ApplyInflammationEffectsNullBridge) {
    int result = homeostatic_immune_apply_inflammation_effects(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(HomeostaticImmuneTest, TriggerFromInstabilityNullBridge) {
    float rates[10] = {5.0f};
    int result = homeostatic_immune_trigger_from_instability(
        nullptr, rates, 10
    );
    EXPECT_EQ(result, -1);
}

TEST_F(HomeostaticImmuneTest, TriggerFromInstabilityNullRates) {
    int result = homeostatic_immune_trigger_from_instability(
        bridge, nullptr, 10
    );
    EXPECT_EQ(result, -1);
}

TEST_F(HomeostaticImmuneTest, BoostFromRecoveryNullBridge) {
    int result = homeostatic_immune_boost_from_recovery(nullptr, true);
    EXPECT_EQ(result, -1);
}

TEST_F(HomeostaticImmuneTest, BridgeUpdateNullBridge) {
    float rates[10] = {5.0f};
    int result = homeostatic_immune_bridge_update(
        nullptr, rates, 10, true, true, 100
    );
    EXPECT_EQ(result, -1);
}

TEST_F(HomeostaticImmuneTest, TNFBiphasicNullBridge) {
    float effect = homeostatic_immune_compute_tnf_biphasic(nullptr, 0.5f);
    EXPECT_FLOAT_EQ(effect, 0.0f);
}

TEST_F(HomeostaticImmuneTest, DetectHyperexcitabilityNullBridge) {
    float rates[10] = {5.0f};
    float hyperexc = homeostatic_immune_detect_hyperexcitability(
        nullptr, rates, 10
    );
    EXPECT_FLOAT_EQ(hyperexc, 0.0f);
}

TEST_F(HomeostaticImmuneTest, DetectHyperexcitabilityNullRates) {
    float hyperexc = homeostatic_immune_detect_hyperexcitability(
        bridge, nullptr, 10
    );
    EXPECT_FLOAT_EQ(hyperexc, 0.0f);
}

TEST_F(HomeostaticImmuneTest, DetectHyperexcitabilityZeroNeurons) {
    float rates[10] = {5.0f};
    float hyperexc = homeostatic_immune_detect_hyperexcitability(
        bridge, rates, 0
    );
    EXPECT_FLOAT_EQ(hyperexc, 0.0f);
}

TEST_F(HomeostaticImmuneTest, DetectScalingFailureNullBridge) {
    float failure = homeostatic_immune_detect_scaling_failure(nullptr, true);
    EXPECT_FLOAT_EQ(failure, 0.0f);
}

TEST_F(HomeostaticImmuneTest, GetCurrentParametersNullBridge) {
    float scaling = homeostatic_immune_get_current_scaling_factor(nullptr);
    float target = homeostatic_immune_get_current_target_rate(nullptr);
    float threshold = homeostatic_immune_get_current_threshold(nullptr);
    float disruption = homeostatic_immune_get_disruption_level(nullptr);

    /* Should return safe defaults */
    EXPECT_FLOAT_EQ(scaling, 1.0f);
    EXPECT_FLOAT_EQ(target, 5.0f);
    EXPECT_FLOAT_EQ(threshold, 0.5f);
    EXPECT_FLOAT_EQ(disruption, 0.0f);
}

TEST_F(HomeostaticImmuneTest, IsHomeostaticFailureNullBridge) {
    bool failure = homeostatic_immune_is_homeostatic_failure(nullptr);
    EXPECT_FALSE(failure);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(HomeostaticImmuneTest, FullBidirectionalCycle) {
    const uint32_t num_neurons = 100;
    float firing_rates[100];

    /* Start with normal activity */
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 5.0f;
    }

    /* Cycle 1: Normal state */
    homeostatic_immune_bridge_update(
        bridge, firing_rates, num_neurons, true, true, 100
    );

    float disruption1 = homeostatic_immune_get_disruption_level(bridge);
    EXPECT_GE(disruption1, 0.0f);

    /* Cycle 2: Introduce hyperexcitability */
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 15.0f;
    }

    homeostatic_immune_bridge_update(
        bridge, firing_rates, num_neurons, false, false, 100
    );

    /* Cycle 3: Recover */
    for (uint32_t i = 0; i < num_neurons; i++) {
        firing_rates[i] = 5.0f;
    }

    homeostatic_immune_bridge_update(
        bridge, firing_rates, num_neurons, true, true, 100
    );

    /* Should complete all cycles */
    float disruption3 = homeostatic_immune_get_disruption_level(bridge);
    EXPECT_GE(disruption3, 0.0f);
}

TEST_F(HomeostaticImmuneTest, ScalingFailureRecoverySequence) {
    /* Introduce scaling failures */
    for (int i = 0; i < 5; i++) {
        homeostatic_immune_detect_scaling_failure(bridge, false);
    }

    float failure_level = homeostatic_immune_detect_scaling_failure(
        bridge, false
    );
    EXPECT_GT(failure_level, 0.5f);

    /* Recover with successes */
    homeostatic_immune_detect_scaling_failure(bridge, true);

    float recovered_level = homeostatic_immune_detect_scaling_failure(
        bridge, true
    );
    EXPECT_NEAR(recovered_level, 0.0f, 0.1f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
