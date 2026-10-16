/**
 * @file test_snn_medulla_bridge.cpp
 * @brief Unit tests for SNN-Medulla bridge
 *
 * Tests bidirectional integration between SNN and medulla oblongata:
 * - Lifecycle (create/destroy)
 * - Configuration
 * - Bio-async integration
 * - Arousal modulation
 * - Protection gating
 * - Circadian modulation
 * - Statistics
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "snn/bridges/nimcp_snn_medulla_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "core/medulla/nimcp_medulla.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SNNMedullaBridgeTest : public ::testing::Test {
protected:
    snn_medulla_bridge_t* bridge = nullptr;
    snn_network_t* snn = nullptr;
    medulla_t medulla = nullptr;

    void SetUp() override {
        // Create SNN
        snn_config_t snn_config;
        snn_config_default(&snn_config);
        snn_config.n_inputs = 10;
        snn_config.n_outputs = 5;
        snn = snn_network_create(&snn_config);

        // Create medulla - NOTE: medulla_default_config() returns struct, not pointer param
        medulla_config_t med_config = medulla_default_config();
        medulla = medulla_create(&med_config);
    }

    void TearDown() override {
        if (bridge) {
            snn_medulla_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (snn) {
            snn_network_destroy(snn);
            snn = nullptr;
        }
        if (medulla) {
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

// Test 1: Default config sets reasonable values
TEST_F(SNNMedullaBridgeTest, DefaultConfigSetsReasonableDefaults) {
    snn_medulla_config_t config;
    snn_medulla_config_default(&config);

    EXPECT_GT(config.arousal_min_rate_factor, 0.0f);
    EXPECT_GT(config.arousal_max_rate_factor, config.arousal_min_rate_factor);
    EXPECT_GT(config.arousal_excitability_gain, 0.0f);
    EXPECT_GT(config.protection_throttle_factor, 0.0f);
    EXPECT_LE(config.protection_throttle_factor, 1.0f);
    EXPECT_TRUE(config.enable_emergency_shutdown);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_GT(config.update_interval_ms, 0.0f);
}

// Test 2: Create with default config
TEST_F(SNNMedullaBridgeTest, CreateWithDefaultConfig) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);
}

// Test 3: Create with custom config
TEST_F(SNNMedullaBridgeTest, CreateWithCustomConfig) {
    snn_medulla_config_t config;
    snn_medulla_config_default(&config);
    config.arousal_max_rate_factor = 3.0f;
    config.enable_circadian_modulation = false;

    bridge = snn_medulla_bridge_create(&config, snn, medulla);
    ASSERT_NE(bridge, nullptr);
}

// Test 4: Create requires SNN
TEST_F(SNNMedullaBridgeTest, CreateRequiresSNN) {
    bridge = snn_medulla_bridge_create(nullptr, nullptr, medulla);
    EXPECT_EQ(bridge, nullptr);
}

// Test 5: Create allows null medulla
TEST_F(SNNMedullaBridgeTest, CreateAllowsNullMedulla) {
    bridge = snn_medulla_bridge_create(nullptr, snn, nullptr);
    EXPECT_NE(bridge, nullptr);
}

// Test 6: Destroy null is safe
TEST_F(SNNMedullaBridgeTest, DestroyNullIsSafe) {
    snn_medulla_bridge_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Bio-async Tests
//=============================================================================

// Test 7: Bio-async initially disconnected
TEST_F(SNNMedullaBridgeTest, BioAsyncInitiallyDisconnected) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(snn_medulla_bridge_is_bio_async_connected(bridge));
}

// Test 8: Bio-async connect attempt
TEST_F(SNNMedullaBridgeTest, BioAsyncConnect) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_medulla_bridge_connect_bio_async(bridge);
    // May fail if router unavailable - any non-crash is valid
    (void)ret;
}

// Test 9: Bio-async disconnect
TEST_F(SNNMedullaBridgeTest, BioAsyncDisconnect) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    snn_medulla_bridge_connect_bio_async(bridge);
    EXPECT_EQ(snn_medulla_bridge_disconnect_bio_async(bridge), 0);
    EXPECT_FALSE(snn_medulla_bridge_is_bio_async_connected(bridge));
}

//=============================================================================
// Update Tests
//=============================================================================

// Test 10: Update with medulla
TEST_F(SNNMedullaBridgeTest, UpdateWithMedulla) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_medulla_bridge_update(bridge, 10.0f), 0);
}

// Test 11: Update without medulla
TEST_F(SNNMedullaBridgeTest, UpdateWithoutMedulla) {
    bridge = snn_medulla_bridge_create(nullptr, snn, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_medulla_bridge_update(bridge, 10.0f), 0);
}

// Test 12: Multiple updates
TEST_F(SNNMedullaBridgeTest, MultipleUpdates) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(snn_medulla_bridge_update(bridge, 1.0f), 0);
    }
}

//=============================================================================
// Modulation Tests
//=============================================================================

// Test 13: Get combined modulation
TEST_F(SNNMedullaBridgeTest, GetCombinedModulation) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    // Update to compute modulation
    snn_medulla_bridge_update(bridge, 100.0f);

    float mod = snn_medulla_get_combined_modulation(bridge);
    EXPECT_GT(mod, 0.0f);
    EXPECT_LE(mod, 3.0f);
}

// Test 14: Activity restriction check
TEST_F(SNNMedullaBridgeTest, ActivityRestrictionCheck) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    // Initially should not be restricted
    bool restricted = snn_medulla_is_activity_restricted(bridge);
    // Value depends on medulla state, just ensure no crash
    (void)restricted;
}

//=============================================================================
// State Query Tests
//=============================================================================

// Test 15: Get state
TEST_F(SNNMedullaBridgeTest, GetState) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    snn_medulla_bridge_update(bridge, 100.0f);

    snn_medulla_state_t state;
    EXPECT_EQ(snn_medulla_bridge_get_state(bridge, &state), 0);
    EXPECT_GE(state.sync_count, 1u);
}

// Test 16: Get arousal
TEST_F(SNNMedullaBridgeTest, GetArousal) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    snn_medulla_bridge_update(bridge, 100.0f);

    float arousal = snn_medulla_get_arousal(bridge);
    // May be -1 if medulla not connected, or [0,1] if connected
    EXPECT_GE(arousal, -1.0f);
    EXPECT_LE(arousal, 1.0f);
}

// Test 17: Get protection level
TEST_F(SNNMedullaBridgeTest, GetProtectionLevel) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    protection_level_t level = snn_medulla_get_protection_level(bridge);
    EXPECT_GE((int)level, 0);
    EXPECT_LE((int)level, 5);
}

// Test 18: Get circadian phase
TEST_F(SNNMedullaBridgeTest, GetCircadianPhase) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    circadian_phase_t phase = snn_medulla_get_circadian_phase(bridge);
    EXPECT_GE((int)phase, 0);
    EXPECT_LE((int)phase, 7);
}

//=============================================================================
// Statistics Tests
//=============================================================================

// Test 19: Get stats initially zero
TEST_F(SNNMedullaBridgeTest, GetStatsInitiallyZero) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    uint32_t sync_count = 999;
    uint32_t emergency_count = 999;
    float avg_mod = 999.0f;

    EXPECT_EQ(snn_medulla_get_stats(bridge, &sync_count, &emergency_count, &avg_mod), 0);
    EXPECT_EQ(sync_count, 0u);
    EXPECT_EQ(emergency_count, 0u);
}

// Test 20: Stats increment with updates
TEST_F(SNNMedullaBridgeTest, StatsIncrementWithUpdates) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    // Run several updates
    for (int i = 0; i < 10; i++) {
        snn_medulla_bridge_update(bridge, 20.0f);
    }

    uint32_t sync_count = 0;
    snn_medulla_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_GT(sync_count, 0u);
}

// Test 21: Reset stats
TEST_F(SNNMedullaBridgeTest, ResetStats) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    // Generate some stats
    for (int i = 0; i < 10; i++) {
        snn_medulla_bridge_update(bridge, 20.0f);
    }

    // Reset
    snn_medulla_reset_stats(bridge);

    uint32_t sync_count = 999;
    snn_medulla_get_stats(bridge, &sync_count, nullptr, nullptr);
    EXPECT_EQ(sync_count, 0u);
}

//=============================================================================
// Edge Cases
//=============================================================================

// Test 22: Null bridge returns error
TEST_F(SNNMedullaBridgeTest, NullBridgeReturnsError) {
    EXPECT_EQ(snn_medulla_bridge_update(nullptr, 10.0f), -1);
    EXPECT_EQ(snn_medulla_bridge_get_state(nullptr, nullptr), -1);
    EXPECT_EQ(snn_medulla_get_stats(nullptr, nullptr, nullptr, nullptr), -1);
}

// Test 23: Get state with null output
TEST_F(SNNMedullaBridgeTest, GetStateNullOutput) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(snn_medulla_bridge_get_state(bridge, nullptr), 0);
}

// Test 24: Get stats with partial null outputs
TEST_F(SNNMedullaBridgeTest, GetStatsPartialNullOutputs) {
    bridge = snn_medulla_bridge_create(nullptr, snn, medulla);
    ASSERT_NE(bridge, nullptr);

    uint32_t sync_count = 0;
    EXPECT_EQ(snn_medulla_get_stats(bridge, &sync_count, nullptr, nullptr), 0);
}

// Test 25: Null bridge accessors return safe values
TEST_F(SNNMedullaBridgeTest, NullBridgeAccessorsReturnSafeValues) {
    EXPECT_LT(snn_medulla_get_arousal(nullptr), 0.0f);
    EXPECT_EQ(snn_medulla_get_protection_level(nullptr), PROTECTION_LEVEL_NORMAL);
    EXPECT_EQ(snn_medulla_get_circadian_phase(nullptr), CIRCADIAN_PHASE_MORNING);
    EXPECT_FLOAT_EQ(snn_medulla_get_combined_modulation(nullptr), 1.0f);
    EXPECT_FALSE(snn_medulla_is_activity_restricted(nullptr));
    EXPECT_FALSE(snn_medulla_bridge_is_bio_async_connected(nullptr));
}
