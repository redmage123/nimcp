/**
 * @file test_memory_consolidation_substrate_bridge.cpp
 * @brief Unit tests for Memory Consolidation-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-consolidation integration including consolidation rate,
 * protein synthesis, replay efficiency, and hippocampal-cortical transfer.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/memory/nimcp_memory_consolidation_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MemoryConsolidationSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    memory_consolidation_t* consolidation = nullptr;  // Use forward-declared type
    consolidation_substrate_bridge_t* bridge = nullptr;
    consolidation_substrate_config_t config;

    void SetUp() override {
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // Create a dummy consolidation pointer (opaque type)
        // Since the bridge doesn't actually call consolidation methods, we can use a dummy
        consolidation = (memory_consolidation_t*)0x1;  // Non-null dummy pointer

        consolidation_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            consolidation_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        // Don't destroy consolidation since it's a dummy pointer
        consolidation = nullptr;
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    void createBridge() {
        // Correct parameter order: config, consolidation, substrate
        bridge = consolidation_substrate_bridge_create(&config, consolidation, substrate);
        ASSERT_NE(bridge, nullptr);
    }

    void setSubstrateATP(float level) {
        if (substrate) {
            substrate_set_atp(substrate, level);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, CreateWithValidInputs) {
    // Correct parameter order: config, consolidation, substrate
    bridge = consolidation_substrate_bridge_create(&config, consolidation, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, CreateWithNullConfig) {
    // Correct parameter order: config, consolidation, substrate
    bridge = consolidation_substrate_bridge_create(nullptr, consolidation, substrate);
    EXPECT_EQ(bridge, nullptr);  // Should fail with NULL config
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, CreateWithNullSubstrate) {
    // Correct parameter order: config, consolidation, substrate
    bridge = consolidation_substrate_bridge_create(&config, consolidation, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, CreateWithNullConsolidation) {
    // Correct parameter order: config, consolidation, substrate
    bridge = consolidation_substrate_bridge_create(&config, nullptr, substrate);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, DestroyNull) {
    consolidation_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, DestroyValid) {
    createBridge();
    consolidation_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, DefaultConfigEnablesModulations) {
    consolidation_substrate_config_t cfg;
    consolidation_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_atp_modulation);
    EXPECT_TRUE(cfg.enable_stress_modulation);
    EXPECT_TRUE(cfg.enable_hypoxia_modulation);
    EXPECT_TRUE(cfg.enable_protein_synthesis);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, DefaultConfigHasReasonableSensitivity) {
    consolidation_substrate_config_t cfg;
    consolidation_substrate_default_config(&cfg);

    EXPECT_FLOAT_EQ(cfg.atp_sensitivity, CONSOLIDATION_SUBSTRATE_DEFAULT_ATP_SENSITIVITY);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, DefaultConfigNullSafe) {
    consolidation_substrate_default_config(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, BioAsyncInitiallyDisconnected) {
    createBridge();
    EXPECT_FALSE(consolidation_substrate_is_bio_async_connected(bridge));
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = consolidation_substrate_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);  // Implementation returns -1 on NULL
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = consolidation_substrate_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, -1);  // Implementation returns -1 on NULL
}

/* ============================================================================
 * Initial Effects Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, InitialEffectsAreOptimal) {
    createBridge();

    EXPECT_FLOAT_EQ(consolidation_substrate_get_consolidation_rate(bridge), 1.0f);
    EXPECT_FLOAT_EQ(consolidation_substrate_get_protein_synthesis_rate(bridge), 1.0f);
    EXPECT_FLOAT_EQ(consolidation_substrate_get_replay_efficiency(bridge), 1.0f);
    EXPECT_FLOAT_EQ(consolidation_substrate_get_transfer_rate(bridge), 1.0f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, InitiallyNotImpaired) {
    createBridge();
    EXPECT_FALSE(consolidation_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Update Tests - Consolidation Rate
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateWithFullATPMaintainsConsolidation) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = consolidation_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float rate = consolidation_substrate_get_consolidation_rate(bridge);
    EXPECT_GE(rate, 0.9f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateWithLowATPReducesConsolidation) {
    createBridge();
    setSubstrateATP(0.35f);

    consolidation_substrate_update(bridge);

    float rate = consolidation_substrate_get_consolidation_rate(bridge);
    EXPECT_LT(rate, 0.8f);
}

/* ============================================================================
 * Update Tests - Protein Synthesis Rate
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateWithOptimalATPMaintainsProteinSynthesis) {
    createBridge();
    setSubstrateATP(0.9f);

    consolidation_substrate_update(bridge);

    float rate = consolidation_substrate_get_protein_synthesis_rate(bridge);
    /* Protein synthesis depends on metabolic capacity (ATP + O2 + glucose).
     * With default O2 (0.97) and glucose (0.90), expect ~0.8 range */
    EXPECT_GE(rate, 0.75f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateWithCriticalATPReducesProteinSynthesis) {
    createBridge();
    setSubstrateATP(0.15f);

    consolidation_substrate_update(bridge);

    float rate = consolidation_substrate_get_protein_synthesis_rate(bridge);
    EXPECT_LT(rate, 0.5f);
}

/* ============================================================================
 * Update Tests - Replay Efficiency
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateWithFullATPMaintainsReplay) {
    createBridge();
    setSubstrateATP(1.0f);

    consolidation_substrate_update(bridge);

    float efficiency = consolidation_substrate_get_replay_efficiency(bridge);
    EXPECT_GE(efficiency, 0.9f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateWithLowATPReducesReplay) {
    createBridge();
    setSubstrateATP(0.25f);

    consolidation_substrate_update(bridge);

    float efficiency = consolidation_substrate_get_replay_efficiency(bridge);
    EXPECT_LT(efficiency, 0.6f);
}

/* ============================================================================
 * Update Tests - Transfer Rate
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateWithFullATPMaintainsTransfer) {
    createBridge();
    setSubstrateATP(1.0f);

    consolidation_substrate_update(bridge);

    float rate = consolidation_substrate_get_transfer_rate(bridge);
    /* Transfer depends on physical capacity (temperature, membrane, ion balance).
     * With normal defaults (~0.95 each), expect good but not max transfer */
    EXPECT_GE(rate, 0.5f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateWithLowATPReducesTransfer) {
    createBridge();
    setSubstrateATP(0.3f);

    consolidation_substrate_update(bridge);

    float rate = consolidation_substrate_get_transfer_rate(bridge);
    EXPECT_LT(rate, 0.7f);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, ImpairedWithCriticalATP) {
    createBridge();
    setSubstrateATP(0.1f);

    consolidation_substrate_update(bridge);

    EXPECT_TRUE(consolidation_substrate_is_impaired(bridge));
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, NotImpairedWithOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);

    consolidation_substrate_update(bridge);

    EXPECT_FALSE(consolidation_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Get Effects Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, GetEffectsSuccess) {
    createBridge();

    // consolidation_substrate_get_effects returns struct by value
    consolidation_substrate_effects_t effects = consolidation_substrate_get_effects(bridge);

    EXPECT_FLOAT_EQ(effects.consolidation_rate, 1.0f);
    EXPECT_FLOAT_EQ(effects.protein_synthesis_rate, 1.0f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, GetEffectsNullBridge) {
    // Returns default optimal effects on NULL
    consolidation_substrate_effects_t effects = consolidation_substrate_get_effects(nullptr);
    EXPECT_FLOAT_EQ(effects.consolidation_rate, 1.0f);
    EXPECT_FLOAT_EQ(effects.protein_synthesis_rate, 1.0f);
    EXPECT_FALSE(effects.is_impaired);
}

/* ============================================================================
 * Query API Null Safety Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, GetConsolidationRateNullReturnsOptimal) {
    // Implementation returns 1.0f on NULL, not -1.0f
    EXPECT_FLOAT_EQ(consolidation_substrate_get_consolidation_rate(nullptr), 1.0f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, GetProteinSynthesisNullReturnsOptimal) {
    // Implementation returns 1.0f on NULL, not -1.0f
    EXPECT_FLOAT_EQ(consolidation_substrate_get_protein_synthesis_rate(nullptr), 1.0f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, GetReplayEfficiencyNullReturnsOptimal) {
    // Implementation returns 1.0f on NULL, not -1.0f
    EXPECT_FLOAT_EQ(consolidation_substrate_get_replay_efficiency(nullptr), 1.0f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, GetTransferRateNullReturnsOptimal) {
    // Implementation returns 1.0f on NULL, not -1.0f
    EXPECT_FLOAT_EQ(consolidation_substrate_get_transfer_rate(nullptr), 1.0f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, IsImpairedNullReturnsFalse) {
    EXPECT_FALSE(consolidation_substrate_is_impaired(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, InitialStatsAreZero) {
    createBridge();

    // consolidation_substrate_get_stats returns struct by value
    consolidation_substrate_stats_t stats = consolidation_substrate_get_stats(bridge);

    EXPECT_EQ(stats.update_count, 0u);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateIncrementsCount) {
    createBridge();

    consolidation_substrate_update(bridge);
    consolidation_substrate_update(bridge);

    // consolidation_substrate_get_stats returns struct by value
    consolidation_substrate_stats_t stats = consolidation_substrate_get_stats(bridge);

    EXPECT_EQ(stats.update_count, 2u);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, GetStatsNullBridge) {
    // Returns zero-initialized stats on NULL
    consolidation_substrate_stats_t stats = consolidation_substrate_get_stats(nullptr);
    EXPECT_EQ(stats.update_count, 0u);
}

/* ============================================================================
 * Configuration Modulation Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, DisabledATPModulationStaysOptimal) {
    config.enable_atp_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    consolidation_substrate_update(bridge);

    EXPECT_FLOAT_EQ(consolidation_substrate_get_consolidation_rate(bridge), 1.0f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, DisabledProteinSynthesisStaysOptimal) {
    config.enable_protein_synthesis = false;
    createBridge();
    setSubstrateATP(0.1f);

    consolidation_substrate_update(bridge);

    EXPECT_FLOAT_EQ(consolidation_substrate_get_protein_synthesis_rate(bridge), 1.0f);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, UpdateNullReturnsError) {
    int result = consolidation_substrate_update(nullptr);
    /* Implementation returns -1 for NULL pointer errors */
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(MemoryConsolidationSubstrateBridgeTest, AllEffectsNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    consolidation_substrate_update(bridge);

    EXPECT_GE(consolidation_substrate_get_consolidation_rate(bridge), 0.0f);
    EXPECT_GE(consolidation_substrate_get_protein_synthesis_rate(bridge), 0.0f);
    EXPECT_GE(consolidation_substrate_get_replay_efficiency(bridge), 0.0f);
    EXPECT_GE(consolidation_substrate_get_transfer_rate(bridge), 0.0f);
}

TEST_F(MemoryConsolidationSubstrateBridgeTest, AllEffectsBoundedAbove) {
    createBridge();
    setSubstrateATP(2.0f);

    consolidation_substrate_update(bridge);

    EXPECT_LE(consolidation_substrate_get_consolidation_rate(bridge), 1.0f);
    EXPECT_LE(consolidation_substrate_get_protein_synthesis_rate(bridge), 1.0f);
    EXPECT_LE(consolidation_substrate_get_replay_efficiency(bridge), 1.0f);
    EXPECT_LE(consolidation_substrate_get_transfer_rate(bridge), 1.0f);
}
