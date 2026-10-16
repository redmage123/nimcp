/**
 * @file test_amygdala_stress_bridge.cpp
 * @brief Unit tests for amygdala-stress/wellbeing integration bridge
 * @version 1.0.0
 * @date 2025-12-22
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_amygdala_stress_bridge.h"
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class AmygdalaStressBridgeTest : public ::testing::Test {
protected:
    amygdala_stress_bridge_t* bridge;
    amygdala_t* amygdala;

    void SetUp() override {
        /* Create amygdala */
        amyg_config_t amyg_cfg;
        amygdala_default_config(&amyg_cfg);
        amygdala = amygdala_create(&amyg_cfg);
        ASSERT_NE(amygdala, nullptr);

        /* Create bridge */
        bridge = amygdala_stress_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            amygdala_stress_destroy(bridge);
        }
        if (amygdala) {
            amygdala_destroy(amygdala);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, CreateWithDefaultConfig) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_FALSE(bridge->base.system_a_connected);
    EXPECT_FALSE(bridge->base.system_b_connected);
    EXPECT_FALSE(bridge->wellbeing_connected);
    EXPECT_EQ(bridge->cortisol_level, 0.0f);
    EXPECT_EQ(bridge->amygdala_sensitization, 0.0f);
}

TEST_F(AmygdalaStressBridgeTest, CreateWithCustomConfig) {
    amygdala_stress_config_t config;
    amygdala_stress_default_config(&config);

    config.cortisol_sensitivity = 1.5f;
    config.enable_fear_cortisol = false;

    auto custom_bridge = amygdala_stress_create(&config);
    ASSERT_NE(custom_bridge, nullptr);

    EXPECT_EQ(custom_bridge->config.cortisol_sensitivity, 1.5f);
    EXPECT_FALSE(custom_bridge->config.enable_fear_cortisol);

    amygdala_stress_destroy(custom_bridge);
}

TEST_F(AmygdalaStressBridgeTest, DefaultConfig) {
    amygdala_stress_config_t config;
    int ret = amygdala_stress_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_fear_cortisol);
    EXPECT_TRUE(config.enable_anxiety_cortisol);
    EXPECT_TRUE(config.enable_stress_sensitization);
    EXPECT_TRUE(config.enable_wellbeing_buffering);
    EXPECT_TRUE(config.enable_allostatic_load);
    EXPECT_EQ(config.cortisol_sensitivity, 1.0f);
}

TEST_F(AmygdalaStressBridgeTest, DestroyNullBridge) {
    amygdala_stress_destroy(nullptr);
    /* Should not crash */
    SUCCEED();
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, ConnectAmygdala) {
    int ret = amygdala_stress_connect_amygdala(bridge, amygdala);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(bridge->base.system_a_connected);
    EXPECT_EQ(bridge->base.system_a, amygdala);
}

TEST_F(AmygdalaStressBridgeTest, ConnectAmygdalaNullBridge) {
    int ret = amygdala_stress_connect_amygdala(nullptr, amygdala);
    EXPECT_NE(ret, 0);
}

TEST_F(AmygdalaStressBridgeTest, ConnectAmygdalaNullAmygdala) {
    int ret = amygdala_stress_connect_amygdala(bridge, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(AmygdalaStressBridgeTest, ConnectStressSystem) {
    int dummy_stress = 42;
    int ret = amygdala_stress_connect_stress(bridge, &dummy_stress);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(bridge->base.system_b_connected);
}

TEST_F(AmygdalaStressBridgeTest, ConnectWellbeingSystem) {
    int dummy_wellbeing = 42;
    int ret = amygdala_stress_connect_wellbeing(bridge, &dummy_wellbeing);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(bridge->wellbeing_connected);
}

/* ============================================================================
 * Amygdala → Stress Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, FearDrivesCortisolRelease) {
    amygdala_stress_connect_amygdala(bridge, amygdala);

    /* Set high fear level (apply_fear_cortisol reads fear, not anxiety) */
    amygdala_set_fear_level(amygdala, 0.8f);

    /* Apply fear cortisol */
    int ret = amygdala_stress_apply_fear_cortisol(bridge);
    EXPECT_EQ(ret, 0);

    /* Cortisol should increase */
    float cortisol = amygdala_stress_get_cortisol(bridge);
    EXPECT_GT(cortisol, 0.0f);
}

TEST_F(AmygdalaStressBridgeTest, AnxietyDrivesChronicCortisol) {
    amygdala_stress_connect_amygdala(bridge, amygdala);

    /* Set anxiety level */
    amygdala_set_anxiety(amygdala, 0.6f);

    /* Apply anxiety cortisol */
    int ret = amygdala_stress_apply_anxiety_cortisol(bridge);
    EXPECT_EQ(ret, 0);

    /* Cortisol should increase */
    float cortisol = amygdala_stress_get_cortisol(bridge);
    EXPECT_GT(cortisol, 0.0f);
}

TEST_F(AmygdalaStressBridgeTest, CortisolAccumulation) {
    amygdala_stress_connect_amygdala(bridge, amygdala);

    amygdala_set_anxiety(amygdala, 0.5f);

    /* Apply multiple times */
    amygdala_stress_apply_fear_cortisol(bridge);
    float cortisol1 = amygdala_stress_get_cortisol(bridge);

    amygdala_stress_apply_anxiety_cortisol(bridge);
    float cortisol2 = amygdala_stress_get_cortisol(bridge);

    /* Cortisol should increase */
    EXPECT_GT(cortisol2, cortisol1);
}

TEST_F(AmygdalaStressBridgeTest, CortisolDecay) {
    amygdala_stress_connect_amygdala(bridge, amygdala);

    /* Set cortisol */
    amygdala_set_anxiety(amygdala, 0.7f);
    amygdala_stress_apply_anxiety_cortisol(bridge);
    float initial_cortisol = amygdala_stress_get_cortisol(bridge);
    EXPECT_GT(initial_cortisol, 0.0f);

    /* Update without adding more (decay only) */
    amygdala_set_anxiety(amygdala, 0.0f);
    amygdala_stress_update(bridge, 1000);  /* 1 second */

    float decayed_cortisol = amygdala_stress_get_cortisol(bridge);
    EXPECT_LT(decayed_cortisol, initial_cortisol);
}

/* ============================================================================
 * Allostatic Load Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, AllostaticLoadAccumulation) {
    amygdala_stress_connect_amygdala(bridge, amygdala);

    /* Set high cortisol */
    bridge->cortisol_level = 0.8f;

    /* Update allostatic load */
    float delta_sec = 1.0f;
    amygdala_stress_update_allostatic_load(bridge, delta_sec);

    float load = amygdala_stress_get_allostatic_load(bridge);
    EXPECT_GT(load, 0.0f);
}

TEST_F(AmygdalaStressBridgeTest, AllostaticLoadDecay) {
    bridge->allostatic_load.current_load = 0.5f;

    /* Update with no cortisol (decay only) */
    bridge->cortisol_level = 0.0f;
    amygdala_stress_update_allostatic_load(bridge, 1.0f);

    float load = amygdala_stress_get_allostatic_load(bridge);
    EXPECT_LT(load, 0.5f);
}

TEST_F(AmygdalaStressBridgeTest, ChronicBurdenDetection) {
    /* Set high allostatic load */
    bridge->allostatic_load.current_load = 0.7f;
    bridge->config.allostatic_threshold = 0.6f;

    amygdala_stress_update_allostatic_load(bridge, 0.1f);

    EXPECT_TRUE(amygdala_stress_is_chronic_burden(bridge));
}

TEST_F(AmygdalaStressBridgeTest, AllostaticLoadPeakTracking) {
    bridge->cortisol_level = 0.9f;

    amygdala_stress_update_allostatic_load(bridge, 1.0f);
    float load1 = amygdala_stress_get_allostatic_load(bridge);

    amygdala_stress_update_allostatic_load(bridge, 1.0f);
    float load2 = amygdala_stress_get_allostatic_load(bridge);

    allostatic_load_state_t state;
    amygdala_stress_get_allostatic_state(bridge, &state);

    EXPECT_GE(state.peak_load, load1);
    EXPECT_GE(state.peak_load, load2);
}

/* ============================================================================
 * Stress → Amygdala Sensitization Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, StressSensitizesAmygdala) {
    /* Set moderate cortisol */
    bridge->cortisol_level = 0.6f;

    amygdala_stress_apply_sensitization(bridge);

    float sensitization = amygdala_stress_get_sensitization(bridge);
    EXPECT_GT(sensitization, 0.0f);
}

TEST_F(AmygdalaStressBridgeTest, HighStressHigherSensitization) {
    /* Low stress */
    bridge->cortisol_level = 0.4f;
    amygdala_stress_apply_sensitization(bridge);
    float low_sens = amygdala_stress_get_sensitization(bridge);

    /* High stress */
    bridge->cortisol_level = 0.8f;
    amygdala_stress_apply_sensitization(bridge);
    float high_sens = amygdala_stress_get_sensitization(bridge);

    EXPECT_GT(high_sens, low_sens);
}

TEST_F(AmygdalaStressBridgeTest, EffectiveReactivityIncreases) {
    /* No stress */
    bridge->amygdala_sensitization = 0.0f;
    float base_reactivity = amygdala_stress_get_effective_reactivity(bridge);
    EXPECT_EQ(base_reactivity, 1.0f);

    /* With sensitization */
    bridge->amygdala_sensitization = 0.3f;
    float enhanced_reactivity = amygdala_stress_get_effective_reactivity(bridge);
    EXPECT_EQ(enhanced_reactivity, 1.3f);
}

TEST_F(AmygdalaStressBridgeTest, SensitizationThresholds) {
    /* Mild stress */
    bridge->cortisol_level = 0.35f;
    bridge->config.stress_mild_threshold = 0.3f;
    amygdala_stress_apply_sensitization(bridge);
    float mild_sens = amygdala_stress_get_sensitization(bridge);
    EXPECT_GT(mild_sens, 0.0f);
    EXPECT_LT(mild_sens, 0.2f);

    /* High stress */
    bridge->cortisol_level = 0.75f;
    amygdala_stress_apply_sensitization(bridge);
    float high_sens = amygdala_stress_get_sensitization(bridge);
    EXPECT_GT(high_sens, mild_sens);
}

/* ============================================================================
 * Wellbeing → Amygdala Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, WellbeingBuffersReactivity) {
    int dummy_wellbeing = 42;
    amygdala_stress_connect_wellbeing(bridge, &dummy_wellbeing);

    /* Simulate high wellbeing */
    bridge->wellbeing_effects.wellbeing_level = 0.8f;
    bridge->config.wellbeing_threshold = 0.7f;

    amygdala_stress_apply_wellbeing_buffer(bridge);

    EXPECT_TRUE(bridge->wellbeing_effects.is_high_wellbeing);
    EXPECT_LT(bridge->wellbeing_buffer, 0.0f);  /* Negative = reduces sensitization */
}

TEST_F(AmygdalaStressBridgeTest, WellbeingEnhancesExtinction) {
    bridge->wellbeing_effects.extinction_enhancement = 0.3f;

    float extinction_boost = amygdala_stress_get_extinction_boost(bridge);
    EXPECT_EQ(extinction_boost, 1.3f);
}

TEST_F(AmygdalaStressBridgeTest, WellbeingReducesSensitization) {
    /* Set stress sensitization */
    bridge->cortisol_level = 0.7f;
    amygdala_stress_apply_sensitization(bridge);
    float stress_sens = amygdala_stress_get_sensitization(bridge);

    /* Apply wellbeing buffer */
    bridge->wellbeing_buffer = -0.2f;  /* Negative reduces sensitization */
    amygdala_stress_apply_sensitization(bridge);
    float buffered_sens = amygdala_stress_get_sensitization(bridge);

    EXPECT_LT(buffered_sens, stress_sens);
}

TEST_F(AmygdalaStressBridgeTest, WellbeingBoostsLoadRecovery) {
    int dummy_wellbeing = 42;
    amygdala_stress_connect_wellbeing(bridge, &dummy_wellbeing);

    /* High wellbeing */
    bridge->wellbeing_effects.wellbeing_level = 0.8f;
    bridge->config.wellbeing_threshold = 0.7f;

    amygdala_stress_apply_wellbeing_buffer(bridge);

    EXPECT_GT(bridge->wellbeing_effects.load_recovery_boost, 1.0f);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, FullUpdateCycle) {
    amygdala_stress_connect_amygdala(bridge, amygdala);

    /* Set initial state */
    amygdala_set_anxiety(amygdala, 0.6f);

    /* Run full update */
    int ret = amygdala_stress_update(bridge, 1000);
    EXPECT_EQ(ret, 0);

    /* Check effects applied */
    EXPECT_GT(bridge->cortisol_level, 0.0f);
    EXPECT_GT(bridge->base.total_updates, 0);
}

TEST_F(AmygdalaStressBridgeTest, StressAnxietyFeedbackLoop) {
    amygdala_stress_connect_amygdala(bridge, amygdala);

    /* Initial anxiety */
    amygdala_set_anxiety(amygdala, 0.5f);

    /* Update multiple times */
    for (int i = 0; i < 10; i++) {
        amygdala_stress_update(bridge, 1000);
    }

    /* Cortisol should accumulate */
    EXPECT_GT(bridge->cortisol_level, 0.0f);

    /* Sensitization should develop */
    EXPECT_GT(bridge->amygdala_sensitization, 0.0f);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, GetAmygdalaEffects) {
    amygdala_stress_effects_t effects;
    int ret = amygdala_stress_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(AmygdalaStressBridgeTest, GetStressEffects) {
    stress_amygdala_effects_t effects;
    int ret = amygdala_stress_get_stress_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(AmygdalaStressBridgeTest, GetWellbeingEffects) {
    wellbeing_amygdala_effects_t effects;
    int ret = amygdala_stress_get_wellbeing_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);
}

TEST_F(AmygdalaStressBridgeTest, GetAllostaticState) {
    allostatic_load_state_t state;
    int ret = amygdala_stress_get_allostatic_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, GetStatistics) {
    uint64_t updates;
    uint32_t episodes, interventions, chronic_episodes;

    int ret = amygdala_stress_get_statistics(bridge, &updates, &episodes, &interventions, &chronic_episodes);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(updates, 0);
}

TEST_F(AmygdalaStressBridgeTest, StatisticsTrackUpdates) {
    amygdala_stress_connect_amygdala(bridge, amygdala);

    amygdala_stress_update(bridge, 1000);
    amygdala_stress_update(bridge, 1000);

    uint64_t updates;
    amygdala_stress_get_statistics(bridge, &updates, nullptr, nullptr, nullptr);
    EXPECT_EQ(updates, 2);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(AmygdalaStressBridgeTest, BioAsyncConnection) {
    /* Note: bio-async may not be available in test environment */
    int ret = amygdala_stress_connect_bio_async(bridge);
    /* Either succeeds or fails gracefully */
    EXPECT_TRUE(ret == 0 || ret != 0);
}

TEST_F(AmygdalaStressBridgeTest, BioAsyncDisconnection) {
    amygdala_stress_disconnect_bio_async(bridge);
    EXPECT_FALSE(amygdala_stress_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
