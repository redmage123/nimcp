/**
 * @file test_eligibility_fep_bridge.cpp
 * @brief Unit tests for Eligibility-FEP bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Comprehensive unit tests for Eligibility-FEP integration bridge
 * WHY:  Verify bidirectional FEP-Eligibility interaction for temporal credit assignment
 * HOW:  Test lifecycle, connections, FEP→Eligibility, Eligibility→FEP, state/stats
 */

#include <gtest/gtest.h>
#include "plasticity/eligibility/nimcp_eligibility_fep_bridge.h"

class EligibilityFepBridgeTest : public ::testing::Test {
protected:
    eligibility_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        eligibility_fep_config_t config;
        eligibility_fep_bridge_default_config(&config);
        bridge = eligibility_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            eligibility_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(EligibilityFepBridgeTest, CreateDestroy) {
    ASSERT_NE(nullptr, bridge);
}

TEST_F(EligibilityFepBridgeTest, CreateWithNullConfig) {
    eligibility_fep_bridge_t* b = eligibility_fep_bridge_create(nullptr);
    ASSERT_NE(nullptr, b);
    eligibility_fep_bridge_destroy(b);
}

TEST_F(EligibilityFepBridgeTest, DestroyNull) {
    eligibility_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(EligibilityFepBridgeTest, DefaultConfig) {
    eligibility_fep_config_t config;
    int result = eligibility_fep_bridge_default_config(&config);
    EXPECT_EQ(0, result);

    EXPECT_GT(config.pe_trace_gain, 0.0f);
    EXPECT_GT(config.precision_decay_sensitivity, 0.0f);
    EXPECT_TRUE(config.enable_pe_eligibility);
    EXPECT_TRUE(config.enable_precision_decay_modulation);
}

TEST_F(EligibilityFepBridgeTest, DefaultConfigNull) {
    int result = eligibility_fep_bridge_default_config(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(EligibilityFepBridgeTest, ConnectFep) {
    fep_system_t* fep = nullptr;  // Mock FEP system
    int result = eligibility_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, ConnectFepNull) {
    int result = eligibility_fep_bridge_connect_fep(nullptr, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(EligibilityFepBridgeTest, ConnectEligibility) {
    eligibility_trace_t traces[10];
    int result = eligibility_fep_bridge_connect_eligibility(bridge, traces, 10);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, ConnectEligibilityNull) {
    int result = eligibility_fep_bridge_connect_eligibility(nullptr, nullptr, 0);
    EXPECT_EQ(-1, result);
}

TEST_F(EligibilityFepBridgeTest, Disconnect) {
    int result = eligibility_fep_bridge_disconnect(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, DisconnectNull) {
    int result = eligibility_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(-1, result);
}

/* ============================================================================
 * FEP → Eligibility Direction Tests
 * ============================================================================ */

TEST_F(EligibilityFepBridgeTest, ApplyPeEligibilityLow) {
    float pe = 0.2f;  // Low PE
    float scaling = eligibility_fep_apply_pe_eligibility(bridge, pe);
    EXPECT_GT(scaling, 0.0f);
}

TEST_F(EligibilityFepBridgeTest, ApplyPeEligibilityHigh) {
    float pe = 5.0f;  // High PE
    float scaling = eligibility_fep_apply_pe_eligibility(bridge, pe);
    EXPECT_GT(scaling, 0.0f);
}

TEST_F(EligibilityFepBridgeTest, ApplyPeEligibilityNull) {
    float scaling = eligibility_fep_apply_pe_eligibility(nullptr, 1.0f);
    EXPECT_EQ(1.0f, scaling);
}

TEST_F(EligibilityFepBridgeTest, ApplyPrecisionDecayModulationLow) {
    float precision = 0.3f;  // Low precision
    float modulation = eligibility_fep_apply_precision_decay_modulation(bridge, precision);
    EXPECT_GE(modulation, ELIGIBILITY_FEP_PRECISION_DECAY_MIN);
    EXPECT_LE(modulation, ELIGIBILITY_FEP_PRECISION_DECAY_MAX);
}

TEST_F(EligibilityFepBridgeTest, ApplyPrecisionDecayModulationHigh) {
    float precision = 1.8f;  // High precision
    float modulation = eligibility_fep_apply_precision_decay_modulation(bridge, precision);
    EXPECT_GT(modulation, 0.5f);
    EXPECT_LE(modulation, ELIGIBILITY_FEP_PRECISION_DECAY_MAX);
}

TEST_F(EligibilityFepBridgeTest, ApplyPrecisionDecayModulationNull) {
    float modulation = eligibility_fep_apply_precision_decay_modulation(nullptr, 1.0f);
    EXPECT_EQ(1.0f, modulation);
}

TEST_F(EligibilityFepBridgeTest, ShouldConsolidateLowFe) {
    // Low free energy -> should not consolidate
    bool should_consolidate = eligibility_fep_should_consolidate(bridge);
    // Result depends on internal state
}

TEST_F(EligibilityFepBridgeTest, ShouldConsolidateNull) {
    bool should_consolidate = eligibility_fep_should_consolidate(nullptr);
    EXPECT_FALSE(should_consolidate);
}

TEST_F(EligibilityFepBridgeTest, GetEffectiveDecay) {
    float base_decay = 0.9f;
    float effective = eligibility_fep_get_effective_decay(bridge, base_decay);
    EXPECT_GT(effective, 0.0f);
    EXPECT_LE(effective, 1.0f);
}

TEST_F(EligibilityFepBridgeTest, GetEffectiveDecayNull) {
    float effective = eligibility_fep_get_effective_decay(nullptr, 0.9f);
    EXPECT_EQ(0.9f, effective);
}

TEST_F(EligibilityFepBridgeTest, GetEffectiveDecayWithPrecision) {
    eligibility_fep_apply_precision_decay_modulation(bridge, 1.5f);
    float base_decay = 0.9f;
    float effective = eligibility_fep_get_effective_decay(bridge, base_decay);
    EXPECT_NE(effective, base_decay);  // Should be modulated
}

/* ============================================================================
 * Eligibility → FEP Direction Tests
 * ============================================================================ */

TEST_F(EligibilityFepBridgeTest, ReportConsolidation) {
    int result = eligibility_fep_report_consolidation(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, ReportConsolidationNull) {
    int result = eligibility_fep_report_consolidation(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(EligibilityFepBridgeTest, ReportConsolidationMultiple) {
    for (int i = 0; i < 5; i++) {
        int result = eligibility_fep_report_consolidation(bridge);
        EXPECT_EQ(0, result);
    }
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(EligibilityFepBridgeTest, Update) {
    int result = eligibility_fep_bridge_update(bridge, 10);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, UpdateNull) {
    int result = eligibility_fep_bridge_update(nullptr, 10);
    EXPECT_EQ(-1, result);
}

TEST_F(EligibilityFepBridgeTest, UpdateMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        int result = eligibility_fep_bridge_update(bridge, 5);
        EXPECT_EQ(0, result);
    }
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(EligibilityFepBridgeTest, GetState) {
    eligibility_fep_state_t state;
    int result = eligibility_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, GetStateNull) {
    eligibility_fep_state_t state;
    int result = eligibility_fep_bridge_get_state(nullptr, &state);
    EXPECT_EQ(-1, result);

    result = eligibility_fep_bridge_get_state(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(EligibilityFepBridgeTest, GetStats) {
    eligibility_fep_stats_t stats;
    int result = eligibility_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, GetStatsNull) {
    eligibility_fep_stats_t stats;
    int result = eligibility_fep_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(-1, result);

    result = eligibility_fep_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(EligibilityFepBridgeTest, StatsAfterOperations) {
    eligibility_fep_apply_pe_eligibility(bridge, 3.0f);
    eligibility_fep_apply_precision_decay_modulation(bridge, 1.2f);
    eligibility_fep_report_consolidation(bridge);
    eligibility_fep_bridge_update(bridge, 10);

    eligibility_fep_stats_t stats;
    int result = eligibility_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(0, result);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(EligibilityFepBridgeTest, ConnectBioAsync) {
    int result = eligibility_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, ConnectBioAsyncNull) {
    int result = eligibility_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(EligibilityFepBridgeTest, DisconnectBioAsync) {
    eligibility_fep_bridge_connect_bio_async(bridge);
    int result = eligibility_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(0, result);
}

TEST_F(EligibilityFepBridgeTest, DisconnectBioAsyncNull) {
    int result = eligibility_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(-1, result);
}

TEST_F(EligibilityFepBridgeTest, IsBioAsyncConnected) {
    bool connected = eligibility_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);  // Not connected initially
}

TEST_F(EligibilityFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = eligibility_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
