/**
 * @file test_working_memory_substrate_bridge.cpp
 * @brief Unit tests for Working Memory-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-working memory integration including capacity factor,
 * decay rate modulation, refresh efficiency, and encoding strength.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/working_memory/nimcp_working_memory_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/nimcp_working_memory.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class WorkingMemorySubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    working_memory_t* working_memory = nullptr;
    wm_substrate_bridge_t* bridge = nullptr;
    wm_substrate_config_t config;

    void SetUp() override {
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        working_memory = working_memory_create();
        ASSERT_NE(working_memory, nullptr);

        wm_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            wm_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (working_memory) {
            working_memory_destroy(working_memory);
            working_memory = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    void createBridge() {
        bridge = wm_substrate_bridge_create(&config, substrate, working_memory);
        ASSERT_NE(bridge, nullptr);
    }

    void setSubstrateATP(float level) {
        if (substrate) {
            substrate_set_atp(substrate, level);
        }
    }

    void setSubstrateTemperature(float temp) {
        if (substrate) {
            substrate_set_temperature(substrate, temp);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, CreateWithValidInputs) {
    bridge = wm_substrate_bridge_create(&config, substrate, working_memory);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(WorkingMemorySubstrateBridgeTest, CreateWithNullConfig) {
    bridge = wm_substrate_bridge_create(nullptr, substrate, working_memory);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(WorkingMemorySubstrateBridgeTest, CreateWithNullSubstrate) {
    bridge = wm_substrate_bridge_create(&config, nullptr, working_memory);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(WorkingMemorySubstrateBridgeTest, CreateWithNullWorkingMemory) {
    bridge = wm_substrate_bridge_create(&config, substrate, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(WorkingMemorySubstrateBridgeTest, DestroyNull) {
    wm_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(WorkingMemorySubstrateBridgeTest, DestroyValid) {
    createBridge();
    wm_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, DefaultConfigEnablesModulations) {
    wm_substrate_config_t cfg;
    wm_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_capacity_modulation);
    EXPECT_TRUE(cfg.enable_decay_modulation);
    EXPECT_TRUE(cfg.enable_refresh_modulation);
}

TEST_F(WorkingMemorySubstrateBridgeTest, DefaultConfigHasReasonableSensitivity) {
    wm_substrate_config_t cfg;
    wm_substrate_default_config(&cfg);

    EXPECT_GT(cfg.atp_sensitivity, 0.0f);
    EXPECT_LE(cfg.atp_sensitivity, 2.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, DefaultConfigNullSafe) {
    int result = wm_substrate_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, BioAsyncInitiallyDisconnected) {
    createBridge();
    EXPECT_FALSE(wm_substrate_is_bio_async_connected(bridge));
}

TEST_F(WorkingMemorySubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = wm_substrate_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(WorkingMemorySubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = wm_substrate_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Initial Effects Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, InitialEffectsAreOptimal) {
    createBridge();

    EXPECT_FLOAT_EQ(wm_substrate_get_capacity_factor(bridge), 1.0f);
    EXPECT_FLOAT_EQ(wm_substrate_get_decay_mod(bridge), 1.0f);
    EXPECT_FLOAT_EQ(wm_substrate_get_refresh_efficiency(bridge), 1.0f);
    EXPECT_FLOAT_EQ(wm_substrate_get_encoding_strength(bridge), 1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, InitiallyNotImpaired) {
    createBridge();
    EXPECT_FALSE(wm_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Update Tests - Capacity Factor
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithFullATPMaintainsCapacity) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = wm_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float capacity = wm_substrate_get_capacity_factor(bridge);
    EXPECT_GE(capacity, 0.9f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithLowATPReducesCapacity) {
    createBridge();
    setSubstrateATP(0.4f);

    wm_substrate_update(bridge);

    float capacity = wm_substrate_get_capacity_factor(bridge);
    EXPECT_LT(capacity, 0.8f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithCriticalATPMinimalCapacity) {
    createBridge();
    setSubstrateATP(0.2f);

    wm_substrate_update(bridge);

    float capacity = wm_substrate_get_capacity_factor(bridge);
    EXPECT_LT(capacity, 0.5f);
}

/* ============================================================================
 * Update Tests - Decay Rate Modulation
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithNormalTempMaintainsDecay) {
    createBridge();
    setSubstrateTemperature(37.0f);

    wm_substrate_update(bridge);

    float decay = wm_substrate_get_decay_mod(bridge);
    EXPECT_FLOAT_EQ(decay, 1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithFeverIncreasesDecay) {
    createBridge();
    setSubstrateTemperature(39.0f);

    wm_substrate_update(bridge);

    float decay = wm_substrate_get_decay_mod(bridge);
    EXPECT_GT(decay, 1.0f);  // Faster forgetting
}

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithHypothermiaDecreasesDecay) {
    createBridge();
    setSubstrateTemperature(35.0f);

    wm_substrate_update(bridge);

    float decay = wm_substrate_get_decay_mod(bridge);
    EXPECT_LT(decay, 1.0f);  // Slower forgetting
}

/* ============================================================================
 * Update Tests - Refresh Efficiency
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithFullATPMaintainsRefresh) {
    createBridge();
    setSubstrateATP(1.0f);

    wm_substrate_update(bridge);

    float refresh = wm_substrate_get_refresh_efficiency(bridge);
    EXPECT_GE(refresh, 0.9f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithLowATPReducesRefresh) {
    createBridge();
    setSubstrateATP(0.35f);

    wm_substrate_update(bridge);

    float refresh = wm_substrate_get_refresh_efficiency(bridge);
    EXPECT_LT(refresh, 0.7f);
}

/* ============================================================================
 * Update Tests - Encoding Strength
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithFullATPMaintainsEncoding) {
    createBridge();
    setSubstrateATP(1.0f);

    wm_substrate_update(bridge);

    float encoding = wm_substrate_get_encoding_strength(bridge);
    EXPECT_GE(encoding, 0.9f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateWithLowATPReducesEncoding) {
    createBridge();
    setSubstrateATP(0.3f);

    wm_substrate_update(bridge);

    float encoding = wm_substrate_get_encoding_strength(bridge);
    /* Encoding strength has floor clamping to maintain baseline function.
     * Even with low ATP, expect reduced but not critically impaired */
    EXPECT_LT(encoding, 0.85f);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, ImpairedWithCriticalATP) {
    createBridge();
    setSubstrateATP(0.15f);

    wm_substrate_update(bridge);

    EXPECT_TRUE(wm_substrate_is_impaired(bridge));
}

TEST_F(WorkingMemorySubstrateBridgeTest, NotImpairedWithOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(37.0f);

    wm_substrate_update(bridge);

    EXPECT_FALSE(wm_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Get Effects Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, GetEffectsSuccess) {
    createBridge();

    wm_substrate_effects_t effects;
    int result = wm_substrate_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(effects.capacity_factor, 1.0f);
    EXPECT_FLOAT_EQ(effects.decay_rate_mod, 1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, GetEffectsNullBridge) {
    wm_substrate_effects_t effects;
    int result = wm_substrate_get_effects(nullptr, &effects);
    EXPECT_EQ(result, -1);
}

TEST_F(WorkingMemorySubstrateBridgeTest, GetEffectsNullOutput) {
    createBridge();
    int result = wm_substrate_get_effects(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Query API Null Safety Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, GetCapacityNullReturnsNegative) {
    EXPECT_FLOAT_EQ(wm_substrate_get_capacity_factor(nullptr), -1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, GetDecayNullReturnsNegative) {
    EXPECT_FLOAT_EQ(wm_substrate_get_decay_mod(nullptr), -1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, GetRefreshNullReturnsNegative) {
    EXPECT_FLOAT_EQ(wm_substrate_get_refresh_efficiency(nullptr), -1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, GetEncodingNullReturnsNegative) {
    EXPECT_FLOAT_EQ(wm_substrate_get_encoding_strength(nullptr), -1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, IsImpairedNullReturnsFalse) {
    EXPECT_FALSE(wm_substrate_is_impaired(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, InitialStatsAreZero) {
    createBridge();

    wm_substrate_stats_t stats;
    int result = wm_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateIncrementsCount) {
    createBridge();

    wm_substrate_update(bridge);
    wm_substrate_update(bridge);
    wm_substrate_update(bridge);

    wm_substrate_stats_t stats;
    wm_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_updates, 3u);
}

TEST_F(WorkingMemorySubstrateBridgeTest, GetStatsNullBridge) {
    wm_substrate_stats_t stats;
    int result = wm_substrate_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Configuration Modulation Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, DisabledCapacityModulationStaysOptimal) {
    config.enable_capacity_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    wm_substrate_update(bridge);

    EXPECT_FLOAT_EQ(wm_substrate_get_capacity_factor(bridge), 1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, DisabledDecayModulationStaysOptimal) {
    config.enable_decay_modulation = false;
    createBridge();
    setSubstrateTemperature(40.0f);

    wm_substrate_update(bridge);

    EXPECT_FLOAT_EQ(wm_substrate_get_decay_mod(bridge), 1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, DisabledRefreshModulationStaysOptimal) {
    config.enable_refresh_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    wm_substrate_update(bridge);

    EXPECT_FLOAT_EQ(wm_substrate_get_refresh_efficiency(bridge), 1.0f);
}

/* Encoding strength is always modulated based on metabolic/physical capacity,
 * so there's no separate enable_encoding_modulation flag */

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, UpdateNullReturnsError) {
    int result = wm_substrate_update(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(WorkingMemorySubstrateBridgeTest, CapacityNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    wm_substrate_update(bridge);

    float capacity = wm_substrate_get_capacity_factor(bridge);
    EXPECT_GE(capacity, 0.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, CapacityNeverAboveCeiling) {
    createBridge();
    setSubstrateATP(2.0f);

    wm_substrate_update(bridge);

    float capacity = wm_substrate_get_capacity_factor(bridge);
    EXPECT_LE(capacity, 1.0f);
}

TEST_F(WorkingMemorySubstrateBridgeTest, DecayModNeverNegative) {
    createBridge();
    setSubstrateTemperature(30.0f);  // Cold

    wm_substrate_update(bridge);

    float decay = wm_substrate_get_decay_mod(bridge);
    EXPECT_GE(decay, 0.0f);
}

/* Note: The bridge provides capacity_factor (0-1 float), not effective_capacity.
 * To compute effective capacity, multiply capacity_factor by the WM system's
 * configured capacity (typically 7). This is done by the working memory system itself,
 * not by the bridge. The bridge only provides the modulation factor. */

TEST_F(WorkingMemorySubstrateBridgeTest, FullCapacityFactorAtOptimalATP) {
    createBridge();
    setSubstrateATP(1.0f);

    wm_substrate_update(bridge);

    float capacity_factor = wm_substrate_get_capacity_factor(bridge);
    EXPECT_GE(capacity_factor, 0.9f);  // Near-full capacity
    // Effective capacity would be: capacity_factor * WM_NORMAL_CAPACITY (7)
    // ~6.3 - 7 items (Miller's 7±2)
}

TEST_F(WorkingMemorySubstrateBridgeTest, ReducedCapacityFactorWithLowATP) {
    createBridge();
    setSubstrateATP(0.4f);

    wm_substrate_update(bridge);

    float capacity_factor = wm_substrate_get_capacity_factor(bridge);
    EXPECT_LT(capacity_factor, 0.8f);  // Reduced capacity
    EXPECT_GT(capacity_factor, 0.2f);  // Not yet minimal
    // Effective capacity would be: capacity_factor * 7 ≈ 2-5 items
}

TEST_F(WorkingMemorySubstrateBridgeTest, MinimalCapacityFactorWithCriticalATP) {
    createBridge();
    setSubstrateATP(0.15f);

    wm_substrate_update(bridge);

    float capacity_factor = wm_substrate_get_capacity_factor(bridge);
    EXPECT_LE(capacity_factor, 0.5f);  // Critical impairment
    // Effective capacity would be: capacity_factor * 7 ≈ 1-3 items
}
