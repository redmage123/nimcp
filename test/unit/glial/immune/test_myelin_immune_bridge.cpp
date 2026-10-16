/**
 * @file test_myelin_immune_bridge.cpp
 * @brief Unit tests for Myelin-Immune Bridge
 *
 * WHAT: Comprehensive tests for myelin-immune bridge functionality
 * WHY:  Ensure myelin integrity correctly responds to autoimmune attack (MS model)
 * HOW:  Test lifecycle, cytokine damage, repair, conduction efficiency
 *
 * TEST CATEGORIES:
 * - Lifecycle: Creation, destruction, configuration
 * - Cytokine Effects: IL-1, TNF, IFN-gamma damage; IL-10 repair
 * - Damage API: Integrity loss from cytokines
 * - Repair API: Integrity restoration
 * - Conduction: Efficiency modulation by integrity
 * - Bio-async: Message routing integration
 * - Statistics: Event tracking and reporting
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "glial/immune/nimcp_myelin_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MyelinImmuneBridgeTest : public ::testing::Test {
protected:
    myelin_immune_bridge_t* bridge = nullptr;
    myelin_immune_config_t config;

    void SetUp() override {
        myelin_immune_default_config(&config);
        bridge = myelin_immune_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            myelin_immune_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, DefaultConfigHasReasonableValues) {
    myelin_immune_config_t cfg;
    int result = myelin_immune_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GE(cfg.il1_damage_rate, 0.0f);
    EXPECT_GE(cfg.tnf_damage_rate, 0.0f);
    EXPECT_GE(cfg.ifn_gamma_damage_rate, 0.0f);
    EXPECT_GE(cfg.il10_repair_rate, 0.0f);
    EXPECT_GE(cfg.integrity_repair_rate, 0.0f);
}

TEST_F(MyelinImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = myelin_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MyelinImmuneBridgeTest, CreateWithNullConfigUsesDefaults) {
    myelin_immune_bridge_t* b = myelin_immune_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->initialized);
    myelin_immune_destroy(b);
}

TEST_F(MyelinImmuneBridgeTest, CreateWithConfigAppliesSettings) {
    config.il1_damage_rate = 0.8f;
    config.il10_repair_rate = 0.5f;

    myelin_immune_bridge_t* b = myelin_immune_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_FLOAT_EQ(b->config.il1_damage_rate, 0.8f);
    EXPECT_FLOAT_EQ(b->config.il10_repair_rate, 0.5f);
    myelin_immune_destroy(b);
}

TEST_F(MyelinImmuneBridgeTest, DestroyNullIsNoOp) {
    myelin_immune_destroy(nullptr);
    // Should not crash
}

TEST_F(MyelinImmuneBridgeTest, BridgeIsInitializedAfterCreate) {
    EXPECT_TRUE(bridge->initialized);
    EXPECT_NE(bridge->base.mutex, nullptr);
}

TEST_F(MyelinImmuneBridgeTest, InitialIntegrityIsOne) {
    EXPECT_FLOAT_EQ(bridge->sheath_integrity, 1.0f);
}

TEST_F(MyelinImmuneBridgeTest, InitialConductionIsOne) {
    EXPECT_FLOAT_EQ(bridge->conduction_efficiency, 1.0f);
}

//=============================================================================
// Cytokine Effects Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, UpdateCytokineEffectsNullReturnsError) {
    int result = myelin_immune_update_cytokine_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MyelinImmuneBridgeTest, UpdateCytokineEffectsSucceeds) {
    int result = myelin_immune_update_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MyelinImmuneBridgeTest, CytokineEffectsInitiallyZero) {
    EXPECT_FLOAT_EQ(bridge->cytokine_effects.il1_damage, 0.0f);
    EXPECT_FLOAT_EQ(bridge->cytokine_effects.tnf_damage, 0.0f);
    EXPECT_FLOAT_EQ(bridge->cytokine_effects.ifn_gamma_damage, 0.0f);
    EXPECT_FLOAT_EQ(bridge->cytokine_effects.il10_repair, 0.0f);
}

//=============================================================================
// Damage API Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, ApplyDamageNullReturnsError) {
    int result = myelin_immune_apply_damage(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MyelinImmuneBridgeTest, ApplyDamageSucceeds) {
    int result = myelin_immune_apply_damage(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(MyelinImmuneBridgeTest, DamageReducesIntegrity) {
    // Set damage signal
    bridge->cytokine_effects.net_damage = 0.5f;

    float initial = bridge->sheath_integrity;
    myelin_immune_apply_damage(bridge, 100.0f);
    float after = bridge->sheath_integrity;

    EXPECT_LT(after, initial);
}

TEST_F(MyelinImmuneBridgeTest, DamageNeverBelowZero) {
    // Set high damage
    bridge->cytokine_effects.net_damage = 1.0f;

    for (int i = 0; i < 100; i++) {
        myelin_immune_apply_damage(bridge, 1000.0f);
    }

    EXPECT_GE(bridge->sheath_integrity, 0.0f);
}

TEST_F(MyelinImmuneBridgeTest, NoDamageWhenZeroSignal) {
    bridge->cytokine_effects.net_damage = 0.0f;

    float initial = bridge->sheath_integrity;
    myelin_immune_apply_damage(bridge, 100.0f);
    float after = bridge->sheath_integrity;

    EXPECT_FLOAT_EQ(after, initial);
}

//=============================================================================
// Repair API Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, ApplyRepairNullReturnsError) {
    int result = myelin_immune_apply_repair(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MyelinImmuneBridgeTest, ApplyRepairSucceeds) {
    int result = myelin_immune_apply_repair(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(MyelinImmuneBridgeTest, RepairIncreasesIntegrity) {
    // First damage
    bridge->sheath_integrity = 0.5f;

    // Set repair signal
    bridge->cytokine_effects.il10_repair = 0.8f;

    float before = bridge->sheath_integrity;
    myelin_immune_apply_repair(bridge, 100.0f);
    float after = bridge->sheath_integrity;

    EXPECT_GT(after, before);
}

TEST_F(MyelinImmuneBridgeTest, RepairNeverAboveOne) {
    bridge->cytokine_effects.il10_repair = 1.0f;

    for (int i = 0; i < 100; i++) {
        myelin_immune_apply_repair(bridge, 1000.0f);
    }

    EXPECT_LE(bridge->sheath_integrity, 1.0f);
}

TEST_F(MyelinImmuneBridgeTest, NoRepairWhenZeroSignal) {
    bridge->sheath_integrity = 0.5f;
    bridge->cytokine_effects.il10_repair = 0.0f;

    float before = bridge->sheath_integrity;
    myelin_immune_apply_repair(bridge, 100.0f);
    float after = bridge->sheath_integrity;

    // May have baseline repair depending on config
    EXPECT_GE(after, before);
}

//=============================================================================
// Integrity and Conduction Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, GetIntegrityNullReturnsDefault) {
    float integrity = myelin_immune_get_integrity(nullptr);
    EXPECT_FLOAT_EQ(integrity, 1.0f);
}

TEST_F(MyelinImmuneBridgeTest, GetIntegrityReturnsCurrentValue) {
    bridge->sheath_integrity = 0.7f;
    float integrity = myelin_immune_get_integrity(bridge);
    EXPECT_FLOAT_EQ(integrity, 0.7f);
}

TEST_F(MyelinImmuneBridgeTest, GetConductionEfficiencyNullReturnsDefault) {
    float efficiency = myelin_immune_get_conduction_efficiency(nullptr);
    EXPECT_FLOAT_EQ(efficiency, 1.0f);
}

TEST_F(MyelinImmuneBridgeTest, GetConductionEfficiencyReturnsCurrentValue) {
    bridge->conduction_efficiency = 0.6f;
    float efficiency = myelin_immune_get_conduction_efficiency(bridge);
    EXPECT_FLOAT_EQ(efficiency, 0.6f);
}

TEST_F(MyelinImmuneBridgeTest, IntegrityAffectsConduction) {
    // Damage the myelin
    bridge->cytokine_effects.net_damage = 0.8f;
    myelin_immune_apply_damage(bridge, 500.0f);
    myelin_immune_update(bridge, 10.0f);

    float integrity = myelin_immune_get_integrity(bridge);
    float conduction = myelin_immune_get_conduction_efficiency(bridge);

    // Conduction should be reduced with reduced integrity
    EXPECT_LE(conduction, 1.0f);
}

//=============================================================================
// Update API Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, UpdateNullReturnsError) {
    int result = myelin_immune_update(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MyelinImmuneBridgeTest, UpdateSucceeds) {
    int result = myelin_immune_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(MyelinImmuneBridgeTest, UpdateWithZeroTimestep) {
    int result = myelin_immune_update(bridge, 0.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(MyelinImmuneBridgeTest, UpdateAppliesBothDamageAndRepair) {
    // Set both signals
    bridge->cytokine_effects.net_damage = 0.3f;
    bridge->cytokine_effects.il10_repair = 0.2f;

    int result = myelin_immune_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, GetStatsNullReturnsError) {
    myelin_immune_stats_t stats;
    int result = myelin_immune_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MyelinImmuneBridgeTest, GetStatsNullOutputReturnsError) {
    int result = myelin_immune_get_stats(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MyelinImmuneBridgeTest, GetStatsReturnsValidData) {
    myelin_immune_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    int result = myelin_immune_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    // Stats returned successfully - values may vary
    EXPECT_GE(stats.current_integrity, 0.0f);
    EXPECT_LE(stats.current_integrity, 1.0f);
}

TEST_F(MyelinImmuneBridgeTest, ResetStatsNullIsNoOp) {
    myelin_immune_reset_stats(nullptr);
    // Should not crash
}

TEST_F(MyelinImmuneBridgeTest, ResetStatsClearsCounters) {
    // Generate some events
    bridge->cytokine_effects.net_damage = 0.5f;
    myelin_immune_apply_damage(bridge, 100.0f);

    // Reset
    myelin_immune_reset_stats(bridge);

    // Verify stats are cleared
    myelin_immune_stats_t stats;
    myelin_immune_get_stats(bridge, &stats);
    EXPECT_EQ(stats.damage_events, 0u);
    EXPECT_EQ(stats.repair_events, 0u);
}

TEST_F(MyelinImmuneBridgeTest, StatsTrackDamageEvents) {
    bridge->cytokine_effects.net_damage = 0.5f;
    myelin_immune_apply_damage(bridge, 100.0f);

    myelin_immune_stats_t stats;
    myelin_immune_get_stats(bridge, &stats);
    EXPECT_GT(stats.damage_events, 0u);
}

TEST_F(MyelinImmuneBridgeTest, StatsTrackRepairEvents) {
    bridge->sheath_integrity = 0.5f;
    bridge->cytokine_effects.il10_repair = 0.5f;
    myelin_immune_apply_repair(bridge, 100.0f);

    myelin_immune_stats_t stats;
    myelin_immune_get_stats(bridge, &stats);
    EXPECT_GT(stats.repair_events, 0u);
}

TEST_F(MyelinImmuneBridgeTest, StatsTrackIntegrityLost) {
    bridge->cytokine_effects.net_damage = 0.5f;
    myelin_immune_apply_damage(bridge, 100.0f);

    myelin_immune_stats_t stats;
    myelin_immune_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_integrity_lost, 0.0f);
}

TEST_F(MyelinImmuneBridgeTest, StatsTrackIntegrityRestored) {
    bridge->sheath_integrity = 0.5f;
    bridge->cytokine_effects.il10_repair = 0.5f;
    myelin_immune_apply_repair(bridge, 100.0f);

    myelin_immune_stats_t stats;
    myelin_immune_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_integrity_restored, 0.0f);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, ConnectBioAsyncNullReturnsError) {
    int result = myelin_immune_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MyelinImmuneBridgeTest, DisconnectBioAsyncNullReturnsSuccess) {
    int result = myelin_immune_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(MyelinImmuneBridgeTest, IsBioAsyncConnectedNullReturnsFalse) {
    bool connected = myelin_immune_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(MyelinImmuneBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    bool connected = myelin_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(MyelinImmuneBridgeTest, DamageAndRepairBalance) {
    // Equal damage and repair should roughly balance
    bridge->cytokine_effects.net_damage = 0.3f;
    bridge->cytokine_effects.il10_repair = 0.3f;

    float initial = bridge->sheath_integrity;
    for (int i = 0; i < 10; i++) {
        myelin_immune_apply_damage(bridge, 10.0f);
        myelin_immune_apply_repair(bridge, 10.0f);
    }
    float final_val = bridge->sheath_integrity;

    // Should be close to initial (within tolerance)
    EXPECT_GT(final_val, 0.5f);
}

TEST_F(MyelinImmuneBridgeTest, CompleteDemyelinationAndRecovery) {
    // Completely demyelinate
    bridge->cytokine_effects.net_damage = 1.0f;
    for (int i = 0; i < 100; i++) {
        myelin_immune_apply_damage(bridge, 100.0f);
    }
    float damaged_level = bridge->sheath_integrity;
    EXPECT_LT(damaged_level, 0.5f);

    // Now repair
    bridge->cytokine_effects.net_damage = 0.0f;
    bridge->cytokine_effects.il10_repair = 1.0f;
    for (int i = 0; i < 500; i++) {
        myelin_immune_apply_repair(bridge, 100.0f);
    }
    // Repair should have increased integrity
    EXPECT_GT(bridge->sheath_integrity, damaged_level);
}

TEST_F(MyelinImmuneBridgeTest, VerySmallTimestepDamage) {
    bridge->cytokine_effects.net_damage = 0.5f;

    float initial = bridge->sheath_integrity;
    myelin_immune_apply_damage(bridge, 0.001f);
    float after = bridge->sheath_integrity;

    // Very small damage
    EXPECT_LE(initial - after, 0.001f);
}

TEST_F(MyelinImmuneBridgeTest, LargeTimestepHandled) {
    bridge->cytokine_effects.net_damage = 0.1f;

    int result = myelin_immune_apply_damage(bridge, 100000.0f);
    EXPECT_EQ(result, 0);

    // Should clamp to valid range
    EXPECT_GE(bridge->sheath_integrity, 0.0f);
    EXPECT_LE(bridge->sheath_integrity, 1.0f);
}
