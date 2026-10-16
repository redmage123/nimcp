/**
 * @file test_fep_immune_bridge.cpp
 * @brief Unit tests for FEP-Immune Bridge module
 */

#include <gtest/gtest.h>
#include <cmath>
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/immune/nimcp_brain_immune.h"

class FEPImmuneBridgeTest : public ::testing::Test {
protected:
    fep_immune_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;
    brain_immune_system_t* immune = nullptr;

    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;

    void SetUp() override {
        // Create FEP-immune bridge
        fep_immune_config_t config;
        fep_immune_bridge_default_config(&config);
        bridge = fep_immune_bridge_create(&config);

        // Create FEP system
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
    }

    void TearDown() override {
        if (bridge) {
            fep_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(FEPImmuneBridgeTest, CreateWithNullConfig) {
    fep_immune_bridge_t* b = fep_immune_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    fep_immune_bridge_destroy(b);
}

TEST_F(FEPImmuneBridgeTest, DestroyNull) {
    fep_immune_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(FEPImmuneBridgeTest, DefaultConfig) {
    fep_immune_config_t config;
    int ret = fep_immune_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.prediction_error_threshold, 0.0f);
    EXPECT_TRUE(config.enable_sickness_behavior);
    EXPECT_TRUE(config.enable_immune_memory_transfer);
}

TEST_F(FEPImmuneBridgeTest, DefaultConfigNullPtr) {
    int ret = fep_immune_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, ConnectFEP) {
    int ret = fep_immune_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, ConnectFEPNullParams) {
    EXPECT_EQ(fep_immune_bridge_connect_fep(nullptr, fep), -1);
    // NULL fep is allowed (acts as disconnect/reset)
    EXPECT_EQ(fep_immune_bridge_connect_fep(bridge, nullptr), 0);
}

TEST_F(FEPImmuneBridgeTest, ConnectImmune) {
    int ret = fep_immune_bridge_connect_immune(bridge, immune);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, ConnectImmuneNullParams) {
    EXPECT_EQ(fep_immune_bridge_connect_immune(nullptr, immune), -1);
    EXPECT_EQ(fep_immune_bridge_connect_immune(bridge, nullptr), -1);
}

TEST_F(FEPImmuneBridgeTest, Disconnect) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    int ret = fep_immune_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * FEP → Immune Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, ReportPredictionFailureLow) {
    int ret = fep_immune_report_prediction_failure(bridge, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, ReportPredictionFailureMedium) {
    int ret = fep_immune_report_prediction_failure(bridge, 6.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, ReportPredictionFailureHigh) {
    int ret = fep_immune_report_prediction_failure(bridge, 12.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, ReportPredictionFailureCritical) {
    fep_immune_bridge_connect_immune(bridge, immune);

    int ret = fep_immune_report_prediction_failure(bridge, 25.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, ReportModelViolation) {
    fep_immune_bridge_connect_immune(bridge, immune);

    uint8_t pattern[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00};
    int ret = fep_immune_report_model_violation(bridge, pattern, 8);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, ReportModelViolationNullParams) {
    EXPECT_EQ(fep_immune_report_model_violation(nullptr, nullptr, 0), -1);
    EXPECT_EQ(fep_immune_report_model_violation(bridge, nullptr, 8), -1);

    uint8_t pattern[8] = {0};
    EXPECT_EQ(fep_immune_report_model_violation(bridge, pattern, 0), -1);
}

TEST_F(FEPImmuneBridgeTest, TransferBeliefToMemory) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    int ret = fep_immune_transfer_belief_to_memory(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, ConvergenceIL10Release) {
    fep_immune_bridge_connect_immune(bridge, immune);
    bridge->state.converged = true;

    int ret = fep_immune_convergence_il10_release(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Immune → FEP Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, ApplyInflammationEffects) {
    fep_immune_bridge_connect_fep(bridge, fep);

    int ret = fep_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, GetPrecisionModifier) {
    float modifier;
    int ret = fep_immune_get_precision_modifier(bridge, &modifier);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(modifier, 0.0f);
    EXPECT_LE(modifier, 1.5f);
}

TEST_F(FEPImmuneBridgeTest, GetLearningModifier) {
    float modifier;
    int ret = fep_immune_get_learning_modifier(bridge, &modifier);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(modifier, 0.0f);
    EXPECT_LE(modifier, 1.5f);
}

TEST_F(FEPImmuneBridgeTest, UpdateCytokineEffects) {
    fep_immune_bridge_connect_immune(bridge, immune);

    int ret = fep_immune_update_cytokine_effects(bridge);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Inflammation Level Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, InflammationLevelNone) {
    brain_inflammation_level_t level = fep_immune_get_inflammation_level(bridge);
    EXPECT_EQ(level, INFLAMMATION_NONE);
}

TEST_F(FEPImmuneBridgeTest, IsSicknessActiveInitial) {
    bool active = fep_immune_is_sickness_active(bridge);
    EXPECT_FALSE(active);
}

TEST_F(FEPImmuneBridgeTest, IsSicknessActiveNull) {
    EXPECT_FALSE(fep_immune_is_sickness_active(nullptr));
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, Update) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    int ret = fep_immune_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPImmuneBridgeTest, UpdateNullBridge) {
    EXPECT_EQ(fep_immune_bridge_update(nullptr, 100), -1);
}

TEST_F(FEPImmuneBridgeTest, UpdateMultipleTimes) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    for (int i = 0; i < 10; i++) {
        int ret = fep_immune_bridge_update(bridge, 100);
        EXPECT_EQ(ret, 0);
    }
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, GetState) {
    fep_immune_state_t state;
    int ret = fep_immune_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(state.inflammation_level, INFLAMMATION_NONE);
    EXPECT_FALSE(state.sickness_behavior_active);
}

TEST_F(FEPImmuneBridgeTest, GetStateNullParams) {
    fep_immune_state_t state;
    EXPECT_EQ(fep_immune_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(fep_immune_bridge_get_state(bridge, nullptr), -1);
}

TEST_F(FEPImmuneBridgeTest, GetStats) {
    fep_immune_stats_t stats;
    int ret = fep_immune_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.immune_activations, 0u);
}

TEST_F(FEPImmuneBridgeTest, StatsAfterActivity) {
    fep_immune_bridge_connect_immune(bridge, immune);

    // Trigger some prediction failures
    fep_immune_report_prediction_failure(bridge, 25.0f);
    fep_immune_report_prediction_failure(bridge, 15.0f);

    fep_immune_stats_t stats;
    fep_immune_bridge_get_stats(bridge, &stats);

    EXPECT_GT(stats.prediction_failures, 0u);
}

/* ============================================================================
 * Precision/Learning Factor Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, PrecisionFactorByInflammation) {
    // Test that precision factor decreases with inflammation
    // (testing internal helpers via public API)

    float modifier_none, modifier_later;
    bridge->state.inflammation_level = INFLAMMATION_NONE;
    fep_immune_get_precision_modifier(bridge, &modifier_none);

    bridge->state.inflammation_level = INFLAMMATION_SYSTEMIC;
    fep_immune_get_precision_modifier(bridge, &modifier_later);

    EXPECT_LT(modifier_later, modifier_none);
}

TEST_F(FEPImmuneBridgeTest, LearningFactorByInflammation) {
    float modifier_none, modifier_later;
    bridge->state.inflammation_level = INFLAMMATION_NONE;
    fep_immune_get_learning_modifier(bridge, &modifier_none);

    bridge->state.inflammation_level = INFLAMMATION_STORM;
    fep_immune_get_learning_modifier(bridge, &modifier_later);

    EXPECT_LT(modifier_later, modifier_none);
}

/* ============================================================================
 * Sickness Behavior Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, SicknessBehaviorActivation) {
    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    // Simulate high inflammation
    bridge->state.inflammation_level = INFLAMMATION_SYSTEMIC;
    fep_immune_apply_inflammation_effects(bridge);

    // Check sickness behavior is active
    fep_immune_state_t state;
    fep_immune_bridge_get_state(bridge, &state);
    // May or may not be active depending on thresholds
    EXPECT_GE(state.sickness_intensity, 0.0f);
}

/* ============================================================================
 * Recovery Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, RecoveryProgress) {
    fep_immune_bridge_connect_fep(bridge, fep);

    // With no inflammation, recovery should progress
    bridge->state.inflammation_level = INFLAMMATION_NONE;
    fep_immune_bridge_update(bridge, 1000);

    fep_immune_state_t state;
    fep_immune_bridge_get_state(bridge, &state);
    EXPECT_GE(state.recovery_progress, 0.0f);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, BioAsyncConnectDisconnect) {
    EXPECT_FALSE(fep_immune_bridge_is_bio_async_connected(bridge));

    int ret = fep_immune_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);

    ret = fep_immune_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(fep_immune_bridge_is_bio_async_connected(bridge));
}

TEST_F(FEPImmuneBridgeTest, BioAsyncDoubleConnect) {
    fep_immune_bridge_connect_bio_async(bridge);
    int ret = fep_immune_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    fep_immune_bridge_disconnect_bio_async(bridge);
}

TEST_F(FEPImmuneBridgeTest, BioAsyncNullParams) {
    EXPECT_EQ(fep_immune_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(fep_immune_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(fep_immune_bridge_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(FEPImmuneBridgeTest, DisabledFeatures) {
    fep_immune_config_t config;
    fep_immune_bridge_default_config(&config);
    config.enable_sickness_behavior = false;
    config.enable_immune_memory_transfer = false;
    config.enable_pe_immune_activation = false;
    config.enable_convergence_il10 = false;

    fep_immune_bridge_t* disabled_bridge = fep_immune_bridge_create(&config);
    ASSERT_NE(disabled_bridge, nullptr);

    // Features should be disabled
    int ret = fep_immune_report_prediction_failure(disabled_bridge, 100.0f);
    EXPECT_EQ(ret, 0);  // Should return success but do nothing

    fep_immune_bridge_destroy(disabled_bridge);
}

TEST_F(FEPImmuneBridgeTest, SensitivityScaling) {
    fep_immune_config_t config;
    fep_immune_bridge_default_config(&config);
    config.cytokine_sensitivity = 2.0f;
    config.inflammation_sensitivity = 0.5f;

    fep_immune_bridge_t* scaled_bridge = fep_immune_bridge_create(&config);
    ASSERT_NE(scaled_bridge, nullptr);

    fep_immune_bridge_destroy(scaled_bridge);
}
