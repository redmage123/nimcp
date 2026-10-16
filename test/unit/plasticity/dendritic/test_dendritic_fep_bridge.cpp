/**
 * @file test_dendritic_fep_bridge.cpp
 * @brief Unit tests for Dendritic-FEP bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive unit tests for Dendritic-FEP integration bridge
 * WHY:  Verify bidirectional FEP-Dendritic interaction for hierarchical predictions
 * HOW:  Test lifecycle, connections, FEP→Dendritic, Dendritic→FEP, state/stats
 */

#include <gtest/gtest.h>
#include "plasticity/dendritic/nimcp_dendritic_fep_bridge.h"

class DendriticFepBridgeTest : public ::testing::Test {
protected:
    dendritic_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        dendritic_fep_config_t config;
        dendritic_fep_bridge_default_config(&config);
        bridge = dendritic_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            dendritic_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(DendriticFepBridgeTest, CreateDestroy) {
    ASSERT_NE(nullptr, bridge);
}

TEST_F(DendriticFepBridgeTest, CreateWithNullConfig) {
    dendritic_fep_bridge_t* b = dendritic_fep_bridge_create(nullptr);
    ASSERT_NE(nullptr, b);
    dendritic_fep_bridge_destroy(b);
}

TEST_F(DendriticFepBridgeTest, DestroyNull) {
    dendritic_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(DendriticFepBridgeTest, DefaultConfig) {
    dendritic_fep_config_t config;
    int result = dendritic_fep_bridge_default_config(&config);
    EXPECT_EQ(0, result);

    EXPECT_GT(config.pe_nmda_gain, 0.0f);
    EXPECT_GT(config.precision_excitability_gain, 0.0f);
    EXPECT_TRUE(config.enable_pe_nmda_modulation);
    EXPECT_TRUE(config.enable_precision_gain_control);
}

TEST_F(DendriticFepBridgeTest, DefaultConfigNull) {
    int result = dendritic_fep_bridge_default_config(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(DendriticFepBridgeTest, ConnectFep) {
    fep_system_t* fep = nullptr;  // Mock FEP system
    int result = dendritic_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, ConnectFepNull) {
    int result = dendritic_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(DendriticFepBridgeTest, ConnectDendritic) {
    dendritic_tree_t tree = {0};  // Mock dendritic tree
    int result = dendritic_fep_bridge_connect_dendritic(bridge, tree);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, ConnectDendriticNull) {
    dendritic_tree_t tree = {0};
    int result = dendritic_fep_bridge_connect_dendritic(nullptr, tree);
    EXPECT_EQ(-1, result);
}

TEST_F(DendriticFepBridgeTest, Disconnect) {
    int result = dendritic_fep_bridge_disconnect(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, DisconnectNull) {
    int result = dendritic_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * FEP → Dendritic Direction Tests
 * ============================================================================ */

TEST_F(DendriticFepBridgeTest, ApplyPeNmdaModulationLow) {
    float pe = 0.3f;  // Low PE
    float modulation = dendritic_fep_apply_pe_nmda_modulation(bridge, pe);
    EXPECT_GT(modulation, 0.0f);
}

TEST_F(DendriticFepBridgeTest, ApplyPeNmdaModulationHigh) {
    float pe = 5.0f;  // High PE
    float modulation = dendritic_fep_apply_pe_nmda_modulation(bridge, pe);
    EXPECT_GT(modulation, 0.0f);
}

TEST_F(DendriticFepBridgeTest, ApplyPeNmdaModulationNull) {
    float modulation = dendritic_fep_apply_pe_nmda_modulation(nullptr, 1.0f);
    EXPECT_EQ(1.0f, modulation);
}

TEST_F(DendriticFepBridgeTest, ApplyPrecisionGainControlLow) {
    float precision = 0.3f;  // Low precision
    float gain = dendritic_fep_apply_precision_gain_control(bridge, precision);
    EXPECT_GE(gain, DENDRITIC_FEP_PRECISION_GAIN_MIN);
    EXPECT_LE(gain, DENDRITIC_FEP_PRECISION_GAIN_MAX);
}

TEST_F(DendriticFepBridgeTest, ApplyPrecisionGainControlHigh) {
    float precision = 1.8f;  // High precision
    float gain = dendritic_fep_apply_precision_gain_control(bridge, precision);
    EXPECT_GT(gain, 1.0f);
    EXPECT_LE(gain, DENDRITIC_FEP_PRECISION_GAIN_MAX);
}

TEST_F(DendriticFepBridgeTest, ApplyPrecisionGainControlNull) {
    float gain = dendritic_fep_apply_precision_gain_control(nullptr, 1.0f);
    EXPECT_EQ(1.0f, gain);
}

TEST_F(DendriticFepBridgeTest, ComputeCalciumBeliefUpdate) {
    float update = dendritic_fep_compute_calcium_belief_update(bridge);
    EXPECT_GE(update, 0.0f);
}

TEST_F(DendriticFepBridgeTest, ComputeCalciumBeliefUpdateNull) {
    float update = dendritic_fep_compute_calcium_belief_update(nullptr);
    EXPECT_EQ(0.0f, update);
}

TEST_F(DendriticFepBridgeTest, GetEffectiveNmdaConductance) {
    float base_conductance = 1.0f;
    float effective = dendritic_fep_get_effective_nmda_conductance(bridge, base_conductance);
    EXPECT_GT(effective, 0.0f);
}

TEST_F(DendriticFepBridgeTest, GetEffectiveNmdaConductanceNull) {
    float effective = dendritic_fep_get_effective_nmda_conductance(nullptr, 1.0f);
    EXPECT_EQ(1.0f, effective);
}

TEST_F(DendriticFepBridgeTest, GetEffectiveNmdaConductanceWithPe) {
    dendritic_fep_apply_pe_nmda_modulation(bridge, 3.0f);
    float base_conductance = 1.0f;
    float effective = dendritic_fep_get_effective_nmda_conductance(bridge, base_conductance);
    EXPECT_NE(effective, base_conductance);  // Should be modulated
}

/* ============================================================================
 * Dendritic → FEP Direction Tests
 * ============================================================================ */

TEST_F(DendriticFepBridgeTest, ReportDendriticSpike) {
    float spike_amplitude = 50.0f;  // mV
    int result = dendritic_fep_report_dendritic_spike(bridge, spike_amplitude);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, ReportDendriticSpikeNull) {
    int result = dendritic_fep_report_dendritic_spike(nullptr, 50.0f);
    EXPECT_EQ(-1, result);
}

TEST_F(DendriticFepBridgeTest, ReportDendriticSpikeSmall) {
    float spike_amplitude = 10.0f;  // Small spike
    int result = dendritic_fep_report_dendritic_spike(bridge, spike_amplitude);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, ReportDendriticSpikeLarge) {
    float spike_amplitude = 100.0f;  // Large spike
    int result = dendritic_fep_report_dendritic_spike(bridge, spike_amplitude);
    EXPECT_EQ(0, result);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(DendriticFepBridgeTest, Update) {
    int result = dendritic_fep_bridge_update(bridge, 10);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, UpdateNull) {
    int result = dendritic_fep_bridge_update(nullptr, 10);
    EXPECT_EQ(-1, result);
}

TEST_F(DendriticFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int result = dendritic_fep_bridge_update(bridge, 5);
        EXPECT_EQ(0, result);
    }
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(DendriticFepBridgeTest, GetState) {
    dendritic_fep_state_t state;
    int result = dendritic_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, GetStateNull) {
    dendritic_fep_state_t state;
    int result = dendritic_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(-1, result);

    result = dendritic_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(DendriticFepBridgeTest, GetStats) {
    dendritic_fep_stats_t stats;
    int result = dendritic_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, GetStatsNull) {
    dendritic_fep_stats_t stats;
    int result = dendritic_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(-1, result);

    result = dendritic_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(DendriticFepBridgeTest, StatsAfterOperations) {
    dendritic_fep_apply_pe_nmda_modulation(bridge, 4.0f);
    dendritic_fep_apply_precision_gain_control(bridge, 1.6f);
    dendritic_fep_report_dendritic_spike(bridge, 75.0f);
    dendritic_fep_bridge_update(bridge, 10);

    dendritic_fep_stats_t stats;
    int result = dendritic_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(DendriticFepBridgeTest, ConnectBioAsync) {
    int result = dendritic_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, ConnectBioAsyncNull) {
    int result = dendritic_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(DendriticFepBridgeTest, DisconnectBioAsync) {
    dendritic_fep_bridge_connect_bio_async(bridge);
    int result = dendritic_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(DendriticFepBridgeTest, DisconnectBioAsyncNull) {
    int result = dendritic_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(DendriticFepBridgeTest, IsBioAsyncConnected) {
    bool connected = dendritic_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);  // Not connected initially
}

TEST_F(DendriticFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = dendritic_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
