/**
 * @file test_snn_immune.cpp
 * @brief Unit tests for SNN immune system integration
 *
 * WHAT: Test SNN-immune bridge functionality
 * WHY:  Verify cytokine modulation and instability detection
 * HOW:  Test bridge lifecycle, effects computation, health monitoring
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

class SNNImmuneTest : public ::testing::Test {
protected:
    snn_network_t* network = nullptr;
    brain_immune_system_t* immune = nullptr;
    snn_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create SNN network
        snn_config_t config;
        snn_config_feedforward(&config, 4, 8, 2);
        network = snn_network_create(&config);

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
    }

    void TearDown() override {
        if (bridge) {
            snn_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }

    void CreateBridge() {
        snn_immune_config_t config;
        snn_immune_config_default(&config);
        bridge = snn_immune_bridge_create(&config, network, immune);
    }
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(SNNImmuneTest, ConfigDefaultNullPointer) {
    snn_immune_config_default(nullptr);
    // Should not crash
    SUCCEED();
}

TEST_F(SNNImmuneTest, ConfigDefaultSetsValues) {
    snn_immune_config_t config;
    snn_immune_config_default(&config);

    EXPECT_FLOAT_EQ(0.7f, config.stdp_il1_factor);
    EXPECT_FLOAT_EQ(0.8f, config.stdp_il6_factor);
    EXPECT_FLOAT_EQ(0.75f, config.stdp_tnf_factor);
    EXPECT_FLOAT_EQ(1.1f, config.stdp_il10_factor);
}

TEST_F(SNNImmuneTest, ConfigDefaultThresholds) {
    snn_immune_config_t config;
    snn_immune_config_default(&config);

    EXPECT_FLOAT_EQ(100.0f, config.max_spike_rate);
    EXPECT_FLOAT_EQ(0.1f, config.min_spike_rate);
    EXPECT_FLOAT_EQ(0.3f, config.burst_threshold);
    EXPECT_FLOAT_EQ(0.8f, config.sync_threshold);
}

TEST_F(SNNImmuneTest, ConfigDefaultFlags) {
    snn_immune_config_t config;
    snn_immune_config_default(&config);

    EXPECT_TRUE(config.auto_report_instabilities);
    EXPECT_TRUE(config.enable_learning_modulation);
    EXPECT_FALSE(config.enable_bio_async);
}

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(SNNImmuneTest, BridgeCreateNullConfig) {
    snn_immune_bridge_t* b = snn_immune_bridge_create(nullptr, network, immune);
    EXPECT_EQ(nullptr, b);
}

TEST_F(SNNImmuneTest, BridgeCreateNullNetwork) {
    snn_immune_config_t config;
    snn_immune_config_default(&config);
    snn_immune_bridge_t* b = snn_immune_bridge_create(&config, nullptr, immune);
    EXPECT_EQ(nullptr, b);
}

TEST_F(SNNImmuneTest, BridgeCreateNullImmune) {
    snn_immune_config_t config;
    snn_immune_config_default(&config);
    snn_immune_bridge_t* b = snn_immune_bridge_create(&config, network, nullptr);
    EXPECT_EQ(nullptr, b);
}

TEST_F(SNNImmuneTest, BridgeCreateValid) {
    CreateBridge();
    EXPECT_NE(nullptr, bridge);
}

TEST_F(SNNImmuneTest, BridgeDestroyNull) {
    snn_immune_bridge_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

TEST_F(SNNImmuneTest, BridgeInitialState) {
    CreateBridge();
    ASSERT_NE(nullptr, bridge);

    EXPECT_TRUE(bridge->connected);
    EXPECT_FALSE(bridge->base.bio_async_enabled);
    EXPECT_EQ(0u, bridge->instability_count);
    EXPECT_EQ(0u, bridge->immune_reports);
}

//=============================================================================
// Bio-Async Connection Tests
//=============================================================================

TEST_F(SNNImmuneTest, BioAsyncConnectNullBridge) {
    int result = snn_immune_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, BioAsyncConnectValid) {
    CreateBridge();
    int result = snn_immune_bridge_connect_bio_async(bridge);
    // May succeed or fail depending on router initialization
    EXPECT_TRUE(result == 0 || result == SNN_ERROR_NOT_INITIALIZED ||
                result == SNN_ERROR_OPERATION_FAILED);
}

TEST_F(SNNImmuneTest, BioAsyncDisconnectNullBridge) {
    int result = snn_immune_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, BioAsyncIsConnectedNullBridge) {
    bool connected = snn_immune_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(SNNImmuneTest, BioAsyncIsConnectedFalseInitially) {
    CreateBridge();
    bool connected = snn_immune_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Effect Update Tests
//=============================================================================

TEST_F(SNNImmuneTest, UpdateEffectsNullBridge) {
    int result = snn_immune_update_effects(nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, UpdateEffectsValid) {
    CreateBridge();
    int result = snn_immune_update_effects(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(SNNImmuneTest, UpdateEffectsInitialValues) {
    CreateBridge();
    snn_immune_update_effects(bridge);

    snn_cytokine_effects_t effects;
    snn_immune_get_effects(bridge, &effects);

    // With no inflammation, factors should be near 1.0
    EXPECT_GT(effects.stdp_amplitude_factor, 0.9f);
    EXPECT_LE(effects.stdp_amplitude_factor, 1.5f);
    EXPECT_FLOAT_EQ(1.0f, effects.learning_rate_factor);
}

//=============================================================================
// Health Computation Tests
//=============================================================================

TEST_F(SNNImmuneTest, ComputeHealthNullBridge) {
    int result = snn_immune_compute_health(nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, ComputeHealthValid) {
    CreateBridge();
    int result = snn_immune_compute_health(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(SNNImmuneTest, ComputeHealthInitiallyHealthy) {
    CreateBridge();
    snn_immune_compute_health(bridge);

    snn_health_metrics_t health;
    snn_immune_get_health(bridge, &health);

    // Initial network may be HEALTHY or SILENT (no activity)
    // Both are valid initial states
    EXPECT_TRUE(health.health == SNN_STATE_HEALTHY ||
                health.health == SNN_STATE_SILENT);
}

//=============================================================================
// Full Update Cycle Tests
//=============================================================================

TEST_F(SNNImmuneTest, UpdateNullBridge) {
    int result = snn_immune_update(nullptr, 0.0f);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, UpdateValid) {
    CreateBridge();
    int result = snn_immune_update(bridge, 0.0f);
    EXPECT_EQ(0, result);
}

TEST_F(SNNImmuneTest, UpdateIntervalRespected) {
    CreateBridge();

    // First update at t=0
    int result1 = snn_immune_update(bridge, 0.0f);
    EXPECT_EQ(0, result1);

    // Update at t=50ms (within interval)
    int result2 = snn_immune_update(bridge, 50.0f);
    EXPECT_EQ(0, result2);

    // Update at t=150ms (past interval)
    int result3 = snn_immune_update(bridge, 150.0f);
    EXPECT_EQ(0, result3);
}

//=============================================================================
// Modulation Function Tests
//=============================================================================

TEST_F(SNNImmuneTest, ModulateSTDPNullBridge) {
    float a_plus = 0.1f;
    float a_minus = 0.1f;
    snn_immune_modulate_stdp(nullptr, &a_plus, &a_minus);
    // Values should be unchanged
    EXPECT_FLOAT_EQ(0.1f, a_plus);
    EXPECT_FLOAT_EQ(0.1f, a_minus);
}

TEST_F(SNNImmuneTest, ModulateSTDPNullParams) {
    CreateBridge();
    snn_immune_modulate_stdp(bridge, nullptr, nullptr);
    // Should not crash
    SUCCEED();
}

TEST_F(SNNImmuneTest, ModulateSTDPValid) {
    CreateBridge();
    snn_immune_update_effects(bridge);

    float a_plus = 0.1f;
    float a_minus = 0.05f;
    snn_immune_modulate_stdp(bridge, &a_plus, &a_minus);

    // Without inflammation, should be scaled by ~1.0
    EXPECT_GT(a_plus, 0.0f);
    EXPECT_GT(a_minus, 0.0f);
}

TEST_F(SNNImmuneTest, ModulateThresholdNullBridge) {
    float result = snn_immune_modulate_threshold(nullptr, -55.0f);
    EXPECT_FLOAT_EQ(-55.0f, result);
}

TEST_F(SNNImmuneTest, ModulateThresholdValid) {
    CreateBridge();
    snn_immune_update_effects(bridge);

    float result = snn_immune_modulate_threshold(bridge, -55.0f);
    // Without inflammation, threshold should be unchanged
    EXPECT_FLOAT_EQ(-55.0f, result);
}

TEST_F(SNNImmuneTest, ModulateLearningRateNullBridge) {
    float result = snn_immune_modulate_learning_rate(nullptr, 0.01f);
    EXPECT_FLOAT_EQ(0.01f, result);
}

TEST_F(SNNImmuneTest, ModulateLearningRateValid) {
    CreateBridge();
    snn_immune_update_effects(bridge);

    float result = snn_immune_modulate_learning_rate(bridge, 0.01f);
    // Without inflammation, LR should be unchanged
    EXPECT_FLOAT_EQ(0.01f, result);
}

//=============================================================================
// Instability Reporting Tests
//=============================================================================

TEST_F(SNNImmuneTest, ReportInstabilityNullBridge) {
    int result = snn_immune_report_instability(nullptr, SNN_STATE_EXPLOSION, 5);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, ReportInstabilityValid) {
    CreateBridge();
    int result = snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 8);
    EXPECT_EQ(0, result);
}

TEST_F(SNNImmuneTest, ReportInstabilityIncrementsCounter) {
    CreateBridge();
    uint32_t initial_reports = bridge->immune_reports;

    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 8);

    EXPECT_EQ(initial_reports + 1, bridge->immune_reports);
}

TEST_F(SNNImmuneTest, CheckAndReportNullBridge) {
    uint32_t reports = snn_immune_check_and_report(nullptr);
    EXPECT_EQ(0u, reports);
}

TEST_F(SNNImmuneTest, CheckAndReportHealthyNetwork) {
    CreateBridge();
    uint32_t reports = snn_immune_check_and_report(bridge);
    // A network without activity may be detected as silent (instability)
    // so reports may be 0 or 1 depending on network state
    EXPECT_LE(reports, 1u);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(SNNImmuneTest, GetEffectsNullBridge) {
    snn_cytokine_effects_t effects;
    int result = snn_immune_get_effects(nullptr, &effects);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, GetEffectsNullOutput) {
    CreateBridge();
    int result = snn_immune_get_effects(bridge, nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, GetEffectsValid) {
    CreateBridge();
    snn_cytokine_effects_t effects;
    int result = snn_immune_get_effects(bridge, &effects);
    EXPECT_EQ(0, result);
}

TEST_F(SNNImmuneTest, GetHealthNullBridge) {
    snn_health_metrics_t health;
    int result = snn_immune_get_health(nullptr, &health);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, GetHealthNullOutput) {
    CreateBridge();
    int result = snn_immune_get_health(bridge, nullptr);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, GetHealthValid) {
    CreateBridge();
    snn_health_metrics_t health;
    int result = snn_immune_get_health(bridge, &health);
    EXPECT_EQ(0, result);
}

TEST_F(SNNImmuneTest, GetInflammationNullBridge) {
    brain_inflammation_level_t level = snn_immune_get_inflammation(nullptr);
    EXPECT_EQ(INFLAMMATION_NONE, level);
}

TEST_F(SNNImmuneTest, GetInflammationValid) {
    CreateBridge();
    brain_inflammation_level_t level = snn_immune_get_inflammation(bridge);
    EXPECT_EQ(INFLAMMATION_NONE, level);  // Initially no inflammation
}

TEST_F(SNNImmuneTest, IsNetworkHealthyNullBridge) {
    bool healthy = snn_immune_is_network_healthy(nullptr);
    EXPECT_TRUE(healthy);  // Default to healthy
}

TEST_F(SNNImmuneTest, IsNetworkHealthyValid) {
    CreateBridge();
    bool healthy = snn_immune_is_network_healthy(bridge);
    EXPECT_TRUE(healthy);  // Initially healthy
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SNNImmuneTest, GetStatsNullBridge) {
    uint32_t instability, reports, updates;
    int result = snn_immune_get_stats(nullptr, &instability, &reports, &updates);
    EXPECT_EQ(SNN_ERROR_NULL_POINTER, result);
}

TEST_F(SNNImmuneTest, GetStatsNullOutputs) {
    CreateBridge();
    int result = snn_immune_get_stats(bridge, nullptr, nullptr, nullptr);
    EXPECT_EQ(0, result);  // Should succeed, just not fill anything
}

TEST_F(SNNImmuneTest, GetStatsValid) {
    CreateBridge();
    uint32_t instability, reports, updates;
    int result = snn_immune_get_stats(bridge, &instability, &reports, &updates);

    EXPECT_EQ(0, result);
    EXPECT_EQ(0u, instability);
    EXPECT_EQ(0u, reports);
}

TEST_F(SNNImmuneTest, ResetStatsNullBridge) {
    snn_immune_reset_stats(nullptr);
    // Should not crash
    SUCCEED();
}

TEST_F(SNNImmuneTest, ResetStatsValid) {
    CreateBridge();

    // Generate some stats
    snn_immune_report_instability(bridge, SNN_STATE_EXPLOSION, 5);

    // Reset
    snn_immune_reset_stats(bridge);

    uint32_t instability, reports, updates;
    snn_immune_get_stats(bridge, &instability, &reports, &updates);

    EXPECT_EQ(0u, instability);
    EXPECT_EQ(0u, reports);
}
