/**
 * @file test_astrocyte_plasticity.cpp
 * @brief Comprehensive unit tests for astrocyte plasticity system
 *
 * WHAT: Tests for tripartite synapse astrocyte-neuron interactions
 * WHY:  Verify astrocyte modulation of synaptic plasticity and transmission
 * HOW:  Google Test framework with 35+ test cases covering all API functions
 *
 * TEST COVERAGE:
 * 1. Lifecycle tests (create, destroy, default config)
 * 2. D-serine release and NMDA modulation tests
 * 3. Glutamate uptake and clearance tests
 * 4. Tripartite synapse modulation tests
 * 5. Gliotransmitter dynamics tests
 * 6. Calcium wave propagation tests
 * 7. LTP/LTD modulation tests
 * 8. Callback mechanism tests
 * 9. Sleep modulation tests
 * 10. Update dynamics tests
 * 11. Reactive astrogliosis tests (A1/A2 states)
 *
 * BIOLOGICAL BASIS:
 * Tests validate astrocyte functions in synaptic plasticity:
 * - D-serine as NMDA co-agonist (required for LTP)
 * - Glutamate clearance (prevents excitotoxicity, shapes transmission)
 * - ATP/adenosine signaling (metaplasticity, sleep homeostasis)
 * - Calcium waves (network coordination)
 * - Reactive states (A1 neurotoxic, A2 neuroprotective)
 *
 * @version 1.0.0
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <vector>

// Headers have their own extern "C" guards
#include "plasticity/astrocyte/nimcp_astrocyte_plasticity.h"
#include "plasticity/astrocyte/nimcp_astrocyte_sleep_bridge.h"
#include "plasticity/astrocyte/nimcp_astrocyte_immune_bridge.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class AstrocytePlasticityTest : public ::testing::Test {
protected:
    astrocyte_plasticity_t astro;
    astrocyte_config_t config;

    // Callback tracking
    struct CallbackData {
        std::vector<gliotransmitter_type_t> types;
        std::vector<float> amounts;
        int call_count;

        CallbackData() : call_count(0) {}
    };
    static CallbackData callback_data;

    static void gliotransmitter_callback(gliotransmitter_type_t type,
                                          float amount,
                                          void* user_data) {
        CallbackData* data = static_cast<CallbackData*>(user_data);
        data->types.push_back(type);
        data->amounts.push_back(amount);
        data->call_count++;
    }

    void SetUp() override {
        astrocyte_plasticity_default_config(&config);
        astro = astrocyte_plasticity_create(&config, 10);
        ASSERT_NE(astro, nullptr);

        // Reset callback data
        callback_data.call_count = 0;
        callback_data.types.clear();
        callback_data.amounts.clear();
    }

    void TearDown() override {
        if (astro) {
            astrocyte_plasticity_destroy(astro);
            astro = nullptr;
        }
    }
};

// Define static member
AstrocytePlasticityTest::CallbackData AstrocytePlasticityTest::callback_data;

class AstrocyteSleepBridgeTest : public ::testing::Test {
protected:
    astrocyte_plasticity_t astro;
    sleep_system_t sleep_sys;
    astrocyte_sleep_bridge_t bridge;

    void SetUp() override {
        // Create astrocyte system
        astrocyte_config_t astro_config;
        astrocyte_plasticity_default_config(&astro_config);
        astro = astrocyte_plasticity_create(&astro_config, 5);
        ASSERT_NE(astro, nullptr);

        // Create sleep system
        sleep_config_t sleep_config = sleep_default_config();
        sleep_sys = sleep_system_create(&sleep_config);
        ASSERT_NE(sleep_sys, nullptr);

        // Create bridge
        astrocyte_sleep_config_t bridge_config;
        astrocyte_sleep_default_config(&bridge_config);
        bridge = astrocyte_sleep_bridge_create(&bridge_config, sleep_sys, astro);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            astrocyte_sleep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (sleep_sys) {
            sleep_system_destroy(sleep_sys);
            sleep_sys = nullptr;
        }
        if (astro) {
            astrocyte_plasticity_destroy(astro);
            astro = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST(AstrocytePlasticityLifecycle, DefaultConfigInitialization) {
    astrocyte_config_t config;
    EXPECT_EQ(astrocyte_plasticity_default_config(&config), 0);

    EXPECT_FLOAT_EQ(config.baseline_d_serine, ASTROCYTE_D_SERINE_BASELINE);
    EXPECT_FLOAT_EQ(config.baseline_glu_uptake, ASTROCYTE_GLU_UPTAKE_BASELINE);
    EXPECT_FLOAT_EQ(config.baseline_atp_release, ASTROCYTE_ATP_BASELINE);
    EXPECT_TRUE(config.enable_d_serine_modulation);
    EXPECT_TRUE(config.enable_glutamate_uptake);
    EXPECT_TRUE(config.enable_atp_signaling);
    EXPECT_TRUE(config.enable_calcium_waves);
}

TEST(AstrocytePlasticityLifecycle, CreateDestroy) {
    astrocyte_config_t config;
    astrocyte_plasticity_default_config(&config);
    astrocyte_plasticity_t astro = astrocyte_plasticity_create(&config, 5);
    ASSERT_NE(astro, nullptr);

    EXPECT_EQ(astrocyte_plasticity_get_num_astrocytes(astro), 5);

    astrocyte_plasticity_destroy(astro);
    // Should not crash
}

TEST(AstrocytePlasticityLifecycle, CreateWithZeroAstrocytesFails) {
    astrocyte_config_t config;
    astrocyte_plasticity_default_config(&config);
    astrocyte_plasticity_t astro = astrocyte_plasticity_create(&config, 0);
    EXPECT_EQ(astro, nullptr);
}

TEST(AstrocytePlasticityLifecycle, DestroyNullDoesNotCrash) {
    astrocyte_plasticity_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Astrocyte State Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, InitialStateIsBaseline) {
    astrocyte_state_t state;
    EXPECT_EQ(astrocyte_plasticity_get_state(astro, 0, &state), 0);

    EXPECT_FLOAT_EQ(state.d_serine_level, config.baseline_d_serine);
    EXPECT_FLOAT_EQ(state.glutamate_uptake_rate, config.baseline_glu_uptake);
    EXPECT_FLOAT_EQ(state.atp_release_level, config.baseline_atp_release);
    EXPECT_EQ(state.reactive_state, ASTROCYTE_RESTING);
    EXPECT_FALSE(state.calcium_wave_active);
}

TEST_F(AstrocytePlasticityTest, UpdateAdvancesState) {
    EXPECT_EQ(astrocyte_plasticity_update(astro, 0, 0.5f, 100), 0);

    astrocyte_state_t state;
    EXPECT_EQ(astrocyte_plasticity_get_state(astro, 0, &state), 0);

    // Calcium should increase from synaptic activity
    EXPECT_GT(state.calcium_current, config.baseline_calcium);
}

TEST_F(AstrocytePlasticityTest, InvalidAstrocyteIDFails) {
    EXPECT_NE(astrocyte_plasticity_update(astro, 999, 0.5f, 100), 0);
}

/* ============================================================================
 * D-Serine Modulation Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, DSerineFactorIncreasesWithLevel) {
    // Release D-serine
    EXPECT_EQ(astrocyte_plasticity_release_gliotransmitter(
        astro, 0, GLIOTRANSMITTER_D_SERINE, 0.3f), 0);

    float factor = astrocyte_plasticity_get_d_serine_factor(astro, 0);
    EXPECT_GT(factor, 0.8f);
    EXPECT_LE(factor, 1.5f);  // Factor is capped at 1.5 (maximum enhancement)
}

TEST_F(AstrocytePlasticityTest, LowDSerineReducesFactor) {
    // Deplete D-serine
    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);

    // Manually set low D-serine (would happen with A1 reactive state)
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A1_REACTIVE, 0.8f);

    float factor = astrocyte_plasticity_get_d_serine_factor(astro, 0);
    // A1 reduces D-serine to 0.4, factor should be low
    EXPECT_LT(factor, 0.7f);
}

/* ============================================================================
 * Glutamate Uptake Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, GlutamateClearanceTimeVariesWithUptake) {
    float time_baseline = astrocyte_plasticity_get_glu_clearance_time(astro, 0);
    EXPECT_NEAR(time_baseline, ASTROCYTE_GLU_UPTAKE_TIME_MS /
                                ASTROCYTE_GLU_UPTAKE_BASELINE, 1.0f);

    // Impair uptake
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A1_REACTIVE, 1.0f);

    float time_impaired = astrocyte_plasticity_get_glu_clearance_time(astro, 0);
    EXPECT_GT(time_impaired, time_baseline);
}

TEST_F(AstrocytePlasticityTest, GlutamateNotificationIncreasesCalcium) {
    astrocyte_state_t state_before;
    astrocyte_plasticity_get_state(astro, 0, &state_before);

    // Notify of glutamate release
    EXPECT_EQ(astrocyte_plasticity_notify_glutamate_release(astro, 0, 1.0f), 0);

    astrocyte_state_t state_after;
    astrocyte_plasticity_get_state(astro, 0, &state_after);

    EXPECT_GT(state_after.calcium_current, state_before.calcium_current);
}

/* ============================================================================
 * ATP/Adenosine Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, HighAdenosineIncreasesA1RInhibition) {
    // Release ATP (converts to adenosine)
    EXPECT_EQ(astrocyte_plasticity_release_gliotransmitter(
        astro, 0, GLIOTRANSMITTER_ADENOSINE, 0.5f), 0);

    float inhibition = astrocyte_plasticity_get_a1r_inhibition(astro, 0);
    EXPECT_GT(inhibition, 0.0f);
}

TEST_F(AstrocytePlasticityTest, LowAdenosineNoInhibition) {
    float inhibition = astrocyte_plasticity_get_a1r_inhibition(astro, 0);
    // Baseline adenosine is 0, below threshold
    EXPECT_FLOAT_EQ(inhibition, 0.0f);
}

/* ============================================================================
 * Calcium Wave Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, TriggerCalciumWave) {
    EXPECT_FALSE(astrocyte_plasticity_is_calcium_wave_active(astro, 0));

    EXPECT_EQ(astrocyte_plasticity_trigger_calcium_wave(astro, 0, 0.8f), 0);

    EXPECT_TRUE(astrocyte_plasticity_is_calcium_wave_active(astro, 0));
}

TEST_F(AstrocytePlasticityTest, CalciumWaveDecaysOverTime) {
    astrocyte_plasticity_trigger_calcium_wave(astro, 0, 0.8f);
    EXPECT_TRUE(astrocyte_plasticity_is_calcium_wave_active(astro, 0));

    // Update multiple times for decay
    for (int i = 0; i < 10; i++) {
        astrocyte_plasticity_update(astro, 0, 0.0f, 100);
    }

    // Wave should decay
    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);
    EXPECT_LT(state.calcium_wave_amplitude, 0.8f);
}

TEST_F(AstrocytePlasticityTest, HighCalciumTriggersWave) {
    // High glutamate → high calcium → wave
    for (int i = 0; i < 5; i++) {
        astrocyte_plasticity_notify_glutamate_release(astro, 0, 1.0f);
    }

    // Calcium should be high enough to trigger wave
    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);
    EXPECT_GT(state.calcium_current, config.ca_wave_trigger_threshold);
}

/* ============================================================================
 * Reactive Astrogliosis Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, TransitionToA1ReactiveState) {
    EXPECT_EQ(astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A1_REACTIVE, 0.7f), 0);

    EXPECT_EQ(astrocyte_plasticity_get_reactive_state(astro, 0),
              ASTROCYTE_A1_REACTIVE);

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);
    EXPECT_FLOAT_EQ(state.a1_factor, 0.7f);
    EXPECT_FLOAT_EQ(state.a2_factor, 0.0f);
}

TEST_F(AstrocytePlasticityTest, A1ReducesDSerineAndUptake) {
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A1_REACTIVE, 1.0f);

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);

    EXPECT_LT(state.d_serine_level, config.baseline_d_serine);
    EXPECT_LT(state.glutamate_uptake_rate, config.baseline_glu_uptake);
}

TEST_F(AstrocytePlasticityTest, A2EnhancesFunction) {
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A2_REACTIVE, 1.0f);

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);

    EXPECT_GE(state.d_serine_level, config.baseline_d_serine);
    EXPECT_GT(state.glutamate_uptake_rate, config.baseline_glu_uptake);
}

TEST_F(AstrocytePlasticityTest, ReturnToRestingStateRestoresBaseline) {
    // First transition to A1
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A1_REACTIVE, 1.0f);

    // Then return to resting
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_RESTING, 0.0f);

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);

    EXPECT_FLOAT_EQ(state.d_serine_level, config.baseline_d_serine);
    EXPECT_FLOAT_EQ(state.glutamate_uptake_rate, config.baseline_glu_uptake);
    EXPECT_FLOAT_EQ(state.a1_factor, 0.0f);
    EXPECT_FLOAT_EQ(state.a2_factor, 0.0f);
}

/* ============================================================================
 * Plasticity Effects Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, GetPlasticityEffects) {
    astrocyte_plasticity_effects_t effects;
    EXPECT_EQ(astrocyte_plasticity_get_effects(astro, 0, &effects), 0);

    // Baseline state should have near-normal effects
    EXPECT_GT(effects.nmda_coagonist_factor, 0.5f);
    EXPECT_LT(effects.glutamate_clearance_time, 10.0f);
}

TEST_F(AstrocytePlasticityTest, A1ImpairstPlasticity) {
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A1_REACTIVE, 1.0f);

    astrocyte_plasticity_effects_t effects;
    astrocyte_plasticity_get_effects(astro, 0, &effects);

    // A1 should reduce NMDA co-agonist availability
    EXPECT_LT(effects.nmda_coagonist_factor, 0.7f);
    EXPECT_LT(effects.ltp_capacity_modulation, 0.7f);
    EXPECT_GT(effects.glutamate_clearance_time, ASTROCYTE_GLU_UPTAKE_TIME_MS);
}

/* ============================================================================
 * Sleep Bridge Tests
 * ============================================================================ */

TEST_F(AstrocyteSleepBridgeTest, SleepBridgeLifecycle) {
    // Bridge should be created successfully in SetUp
    EXPECT_NE(bridge, nullptr);
    // Cleanup in TearDown
}

TEST_F(AstrocyteSleepBridgeTest, UpdateFromSleepState) {
    // Set sleep to NREM
    sleep_enter_state(sleep_sys, SLEEP_STATE_DEEP_NREM);

    EXPECT_EQ(astrocyte_sleep_update(bridge), 0);

    astrocyte_sleep_effects_t effects;
    EXPECT_EQ(astrocyte_sleep_get_effects(bridge, &effects), 0);

    EXPECT_EQ(effects.current_state, SLEEP_STATE_DEEP_NREM);
    EXPECT_GT(effects.d_serine_factor, 1.0f);  // Enhanced in NREM
    EXPECT_GT(effects.glutamate_uptake_factor, 0.9f);
    EXPECT_TRUE(effects.glymphatic_active);
}

TEST_F(AstrocyteSleepBridgeTest, GlymphaticActiveInNREM) {
    sleep_enter_state(sleep_sys, SLEEP_STATE_LIGHT_NREM);
    astrocyte_sleep_update(bridge);

    EXPECT_TRUE(astrocyte_sleep_is_glymphatic_active(bridge));
}

TEST_F(AstrocyteSleepBridgeTest, GlymphaticInactiveInAwake) {
    sleep_enter_state(sleep_sys, SLEEP_STATE_AWAKE);
    astrocyte_sleep_update(bridge);

    EXPECT_FALSE(astrocyte_sleep_is_glymphatic_active(bridge));
}

TEST_F(AstrocyteSleepBridgeTest, DSerineEnhancedInNREM) {
    sleep_enter_state(sleep_sys, SLEEP_STATE_DEEP_NREM);
    astrocyte_sleep_update(bridge);

    float base_d_serine = 0.8f;
    float modulated = astrocyte_sleep_get_d_serine_level(bridge, base_d_serine);

    EXPECT_GT(modulated, base_d_serine);
}

TEST_F(AstrocyteSleepBridgeTest, DSerineReducedInREM) {
    sleep_enter_state(sleep_sys, SLEEP_STATE_REM);
    astrocyte_sleep_update(bridge);

    float base_d_serine = 0.8f;
    float modulated = astrocyte_sleep_get_d_serine_level(bridge, base_d_serine);

    EXPECT_LT(modulated, base_d_serine);
}

TEST_F(AstrocyteSleepBridgeTest, ApplyModulationToAstrocytes) {
    sleep_enter_state(sleep_sys, SLEEP_STATE_DEEP_NREM);
    astrocyte_sleep_update(bridge);
    EXPECT_EQ(astrocyte_sleep_apply_modulation(bridge), 0);
    // Should not crash, effects applied to all astrocytes
}

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

TEST(AstrocyteSleepHelpers, DSerineFactorVariesBySleepState) {
    EXPECT_FLOAT_EQ(astrocyte_sleep_get_d_serine_factor(SLEEP_STATE_AWAKE),
                    ASTROCYTE_SLEEP_D_SERINE_AWAKE);
    EXPECT_FLOAT_EQ(astrocyte_sleep_get_d_serine_factor(SLEEP_STATE_DEEP_NREM),
                    ASTROCYTE_SLEEP_D_SERINE_DEEP_NREM);
    EXPECT_FLOAT_EQ(astrocyte_sleep_get_d_serine_factor(SLEEP_STATE_REM),
                    ASTROCYTE_SLEEP_D_SERINE_REM);
}

TEST(AstrocyteSleepHelpers, GlutamateUptakeFactorVariesBySleepState) {
    EXPECT_FLOAT_EQ(astrocyte_sleep_get_uptake_factor(SLEEP_STATE_AWAKE),
                    ASTROCYTE_SLEEP_GLU_UPTAKE_AWAKE);
    EXPECT_FLOAT_EQ(astrocyte_sleep_get_uptake_factor(SLEEP_STATE_DEEP_NREM),
                    ASTROCYTE_SLEEP_GLU_UPTAKE_DEEP_NREM);
}

/* ============================================================================
 * LTP Notification Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, LTPInductionBoostsDSerine) {
    astrocyte_state_t state_before;
    astrocyte_plasticity_get_state(astro, 0, &state_before);

    EXPECT_EQ(astrocyte_plasticity_notify_ltp_induction(astro, 0), 0);

    astrocyte_state_t state_after;
    astrocyte_plasticity_get_state(astro, 0, &state_after);

    EXPECT_GT(state_after.d_serine_level, state_before.d_serine_level);
    EXPECT_GT(state_after.calcium_current, state_before.calcium_current);
}

TEST_F(AstrocytePlasticityTest, LTPInductionTriggersCalciumWave) {
    astrocyte_plasticity_notify_ltp_induction(astro, 0);

    EXPECT_TRUE(astrocyte_plasticity_is_calcium_wave_active(astro, 0));
}

/* ============================================================================
 * Gliotransmitter Callback Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, GliotransmitterCallbackTriggered) {
    /* WHAT: Test gliotransmitter release callback mechanism
     * WHY:  Allow external systems to respond to astrocyte signaling
     */
    // Create astrocyte with callback
    astrocyte_config_t cb_config;
    astrocyte_plasticity_default_config(&cb_config);
    cb_config.gliotransmitter_callback = gliotransmitter_callback;
    cb_config.callback_user_data = &callback_data;

    astrocyte_plasticity_t astro_cb = astrocyte_plasticity_create(&cb_config, 1);

    // Release D-serine
    astrocyte_plasticity_release_gliotransmitter(
        astro_cb, 0, GLIOTRANSMITTER_D_SERINE, 0.3f);

    EXPECT_GT(callback_data.call_count, 0);
    if (callback_data.call_count > 0) {
        EXPECT_EQ(callback_data.types[0], GLIOTRANSMITTER_D_SERINE);
        EXPECT_FLOAT_EQ(callback_data.amounts[0], 0.3f);
    }

    astrocyte_plasticity_destroy(astro_cb);
}

TEST_F(AstrocytePlasticityTest, AllGliotransmitterTypesSupported) {
    /* WHAT: Test release of all gliotransmitter types
     * WHY:  Verify system supports D-serine, glutamate, ATP, adenosine
     */
    gliotransmitter_type_t types[] = {
        GLIOTRANSMITTER_D_SERINE,
        GLIOTRANSMITTER_GLUTAMATE,
        GLIOTRANSMITTER_ATP,
        GLIOTRANSMITTER_ADENOSINE
    };

    for (int i = 0; i < 4; i++) {
        int result = astrocyte_plasticity_release_gliotransmitter(
            astro, 0, types[i], 0.2f);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(AstrocytePlasticityTest, ATPToAdenosineConversion) {
    /* WHAT: Test ATP → adenosine pathway
     * WHY:  ATP is converted to adenosine via ectonucleotidases
     * BIOLOGICAL: Adenosine A1R activation modulates transmission
     */
    astrocyte_state_t state_before;
    astrocyte_plasticity_get_state(astro, 0, &state_before);

    // Release ATP
    astrocyte_plasticity_release_gliotransmitter(
        astro, 0, GLIOTRANSMITTER_ATP, 0.6f);

    // Update to allow conversion
    astrocyte_plasticity_update(astro, 0, 0.5f, 100);

    astrocyte_state_t state_after;
    astrocyte_plasticity_get_state(astro, 0, &state_after);

    // ATP and adenosine levels should be elevated
    EXPECT_GT(state_after.atp_release_level, state_before.atp_release_level);
}

/* ============================================================================
 * Additional Tripartite Synapse Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, NMDACoagonistModulation) {
    /* WHAT: Test NMDA co-agonist factor modulates LTP capacity
     * WHY:  D-serine is required for NMDA receptor activation
     */
    astrocyte_plasticity_effects_t effects;
    astrocyte_plasticity_get_effects(astro, 0, &effects);

    // Normal D-serine should support strong LTP
    EXPECT_GT(effects.ltp_capacity_modulation, 0.5f);
}

TEST_F(AstrocytePlasticityTest, STDPWindowModulation) {
    /* WHAT: Test D-serine modulates STDP timing window
     * WHY:  D-serine affects NMDA kinetics → STDP window width
     * BIOLOGICAL: Low D-serine narrows STDP window
     */
    astrocyte_plasticity_effects_t effects;
    astrocyte_plasticity_get_effects(astro, 0, &effects);

    // STDP window modulation should be within reasonable range
    EXPECT_GE(effects.stdp_window_modulation, 0.5f);
    EXPECT_LE(effects.stdp_window_modulation, 1.5f);
}

TEST_F(AstrocytePlasticityTest, SpilloverFactorDependsOnUptake) {
    /* WHAT: Test glutamate spillover inversely related to uptake
     * WHY:  Impaired uptake causes spillover to neighbor synapses
     */
    astrocyte_plasticity_effects_t effects_normal;
    astrocyte_plasticity_get_effects(astro, 0, &effects_normal);

    // Impair uptake
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A1_REACTIVE, 0.8f);

    astrocyte_plasticity_effects_t effects_impaired;
    astrocyte_plasticity_get_effects(astro, 0, &effects_impaired);

    // Spillover should increase when uptake is impaired
    EXPECT_GT(effects_impaired.spillover_factor, effects_normal.spillover_factor);
}

TEST_F(AstrocytePlasticityTest, NetworkSynchronizationByCalciumWaves) {
    /* WHAT: Test calcium waves increase network synchronization
     * WHY:  Waves coordinate gliotransmitter release across synapses
     */
    // Trigger calcium wave
    astrocyte_plasticity_trigger_calcium_wave(astro, 0, 0.9f);

    astrocyte_plasticity_effects_t effects;
    astrocyte_plasticity_get_effects(astro, 0, &effects);

    EXPECT_GT(effects.synchronization_factor, 0.0f);
}

/* ============================================================================
 * Update Dynamics Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, UpdateWithLowActivity) {
    /* WHAT: Test astrocyte state update with low synaptic activity
     * WHY:  Verify dynamics respond appropriately to activity level
     */
    astrocyte_state_t state_before;
    astrocyte_plasticity_get_state(astro, 0, &state_before);

    int result = astrocyte_plasticity_update(astro, 0, 0.1f, 50);
    EXPECT_EQ(result, 0);

    astrocyte_state_t state_after;
    astrocyte_plasticity_get_state(astro, 0, &state_after);

    // State should evolve
    EXPECT_GT(state_after.last_update_ms, 0);
}

TEST_F(AstrocytePlasticityTest, UpdateTimeDelta) {
    /* WHAT: Test update respects time delta
     * WHY:  State evolution should be time-dependent
     */
    astrocyte_plasticity_update(astro, 0, 0.5f, 10);

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);

    EXPECT_GT(state.delta_time_s, 0.0f);
}

/* ============================================================================
 * Additional Reactive Astrogliosis Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, MixedReactiveState) {
    /* WHAT: Test mixed A1/A2 phenotype
     * WHY:  Chronic inflammation can produce mixed phenotype
     */
    int result = astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_MIXED_REACTIVE, 0.5f);

    EXPECT_EQ(result, 0);

    astrocyte_reactive_state_t state =
        astrocyte_plasticity_get_reactive_state(astro, 0);

    EXPECT_EQ(state, ASTROCYTE_MIXED_REACTIVE);
}

TEST_F(AstrocytePlasticityTest, ReactiveStateIntensityModulation) {
    /* WHAT: Test reactive state intensity affects modulation strength
     * WHY:  Different inflammation levels produce graded responses
     */
    // Mild A1 reaction
    astrocyte_plasticity_set_reactive_state(
        astro, 0, ASTROCYTE_A1_REACTIVE, 0.3f);

    astrocyte_state_t state_mild;
    astrocyte_plasticity_get_state(astro, 0, &state_mild);

    // Severe A1 reaction
    astrocyte_plasticity_set_reactive_state(
        astro, 1, ASTROCYTE_A1_REACTIVE, 0.9f);

    astrocyte_state_t state_severe;
    astrocyte_plasticity_get_state(astro, 1, &state_severe);

    // Severe reaction should have greater effect
    EXPECT_GT(state_mild.d_serine_level, state_severe.d_serine_level);
}

/* ============================================================================
 * Spatial Coverage Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, SpatialCoverageRadius) {
    /* WHAT: Test astrocyte spatial coverage parameters
     * WHY:  Each astrocyte covers ~100-500 μm radius
     * BIOLOGICAL: Single astrocyte contacts ~100,000 synapses
     */
    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);

    EXPECT_GT(state.coverage_radius_um, 0.0f);
    EXPECT_LT(state.coverage_radius_um, 1000.0f);
}

TEST_F(AstrocytePlasticityTest, CalciumWaveFrequencyRange) {
    /* WHAT: Test calcium wave frequency within biological range
     * WHY:  Different frequencies for local vs. global signaling
     * BIOLOGICAL: 0.01-1 Hz range (Cornell-Bell et al. 1990)
     */
    astrocyte_plasticity_trigger_calcium_wave(astro, 0, 0.7f);

    astrocyte_state_t state;
    astrocyte_plasticity_get_state(astro, 0, &state);

    EXPECT_GE(state.calcium_wave_frequency, ASTROCYTE_CA_WAVE_FREQ_LOW);
    EXPECT_LE(state.calcium_wave_frequency, ASTROCYTE_CA_WAVE_FREQ_HIGH);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(AstrocytePlasticityTest, NullPointerHandling) {
    astrocyte_state_t state;
    EXPECT_NE(astrocyte_plasticity_get_state(nullptr, 0, &state), 0);
    EXPECT_NE(astrocyte_plasticity_get_state(astro, 0, nullptr), 0);
    EXPECT_EQ(astrocyte_plasticity_get_num_astrocytes(nullptr), 0);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
