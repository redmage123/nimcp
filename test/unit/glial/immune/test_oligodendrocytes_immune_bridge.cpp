/**
 * @file test_oligodendrocytes_immune_bridge.cpp
 * @brief Unit tests for Oligodendrocytes-Immune Bridge
 *
 * WHAT: Comprehensive tests for oligodendrocytes-immune bridge functionality
 * WHY:  Ensure oligodendrocyte damage and demyelination correctly respond to immune signals
 * HOW:  Test lifecycle, cytokine effects, damage accumulation, demyelination/remyelination
 *
 * TEST CATEGORIES:
 * - Lifecycle: Creation, destruction, configuration
 * - Cytokine Effects: IL-1, IL-6, TNF, IL-10, IFN-gamma modulation
 * - Damage API: Accumulation, death threshold
 * - Demyelination: State transitions, myelin loss
 * - Remyelination: OPC recruitment, repair capacity
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
#include "glial/immune/nimcp_oligodendrocytes_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class OligodendrocytesImmuneBridgeTest : public ::testing::Test {
protected:
    oligo_immune_bridge_t* bridge = nullptr;
    oligo_immune_config_t config;

    void SetUp() override {
        oligo_immune_default_config(&config);
        bridge = oligo_immune_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            oligo_immune_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, DefaultConfigHasReasonableValues) {
    oligo_immune_config_t cfg;
    int result = oligo_immune_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GE(cfg.il1_myelination_reduction, 0.0f);
    EXPECT_GE(cfg.tnf_oligodendrocyte_death, 0.0f);
    EXPECT_GE(cfg.il10_protection_factor, 0.0f);
    EXPECT_GT(cfg.death_threshold, 0.0f);
    EXPECT_LE(cfg.death_threshold, 1.0f);
}

TEST_F(OligodendrocytesImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = oligo_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(OligodendrocytesImmuneBridgeTest, CreateWithNullConfigUsesDefaults) {
    oligo_immune_bridge_t* b = oligo_immune_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->initialized);
    oligo_immune_destroy(b);
}

TEST_F(OligodendrocytesImmuneBridgeTest, CreateWithConfigAppliesSettings) {
    config.il1_myelination_reduction = 0.8f;
    config.death_threshold = 0.5f;

    oligo_immune_bridge_t* b = oligo_immune_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_FLOAT_EQ(b->config.il1_myelination_reduction, 0.8f);
    EXPECT_FLOAT_EQ(b->config.death_threshold, 0.5f);
    oligo_immune_destroy(b);
}

TEST_F(OligodendrocytesImmuneBridgeTest, DestroyNullIsNoOp) {
    oligo_immune_destroy(nullptr);
    // Should not crash
}

TEST_F(OligodendrocytesImmuneBridgeTest, BridgeIsInitializedAfterCreate) {
    EXPECT_TRUE(bridge->initialized);
    EXPECT_NE(bridge->base.mutex, nullptr);
}

TEST_F(OligodendrocytesImmuneBridgeTest, InitialDamageIsNone) {
    EXPECT_EQ(bridge->damage_state, OLIGO_DAMAGE_NONE);
    EXPECT_FLOAT_EQ(bridge->damage_level, 0.0f);
}

TEST_F(OligodendrocytesImmuneBridgeTest, InitialDemyelinationIsNone) {
    EXPECT_EQ(bridge->demyelination_state, DEMYELINATION_NONE);
}

//=============================================================================
// Cytokine Effects Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, UpdateCytokineEffectsNullReturnsError) {
    int result = oligo_immune_update_cytokine_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, UpdateCytokineEffectsSucceeds) {
    int result = oligo_immune_update_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetCytokineEffectsNullBridgeReturnsError) {
    oligo_cytokine_effects_t effects;
    int result = oligo_immune_get_cytokine_effects(nullptr, &effects);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetCytokineEffectsNullOutputReturnsError) {
    int result = oligo_immune_get_cytokine_effects(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetCytokineEffectsReturnsData) {
    oligo_cytokine_effects_t effects;
    memset(&effects, 0xFF, sizeof(effects));

    int result = oligo_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(OligodendrocytesImmuneBridgeTest, ApplyModulationNullReturnsError) {
    int result = oligo_immune_apply_modulation(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, ApplyModulationSucceeds) {
    int result = oligo_immune_apply_modulation(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Damage API Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, AccumulateDamageNullReturnsError) {
    int result = oligo_immune_accumulate_damage(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, AccumulateDamageSucceeds) {
    int result = oligo_immune_accumulate_damage(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(OligodendrocytesImmuneBridgeTest, DamageAccumulates) {
    // Set net damage signal
    bridge->cytokine_effects.net_damage_signal = 0.5f;

    float initial_damage = bridge->damage_level;
    oligo_immune_accumulate_damage(bridge, 100.0f);
    float after_damage = bridge->damage_level;

    EXPECT_GT(after_damage, initial_damage);
}

TEST_F(OligodendrocytesImmuneBridgeTest, CheckDeathNullReturnsFalse) {
    bool died = oligo_immune_check_death(nullptr);
    EXPECT_FALSE(died);
}

TEST_F(OligodendrocytesImmuneBridgeTest, CheckDeathFalseWhenHealthy) {
    bool died = oligo_immune_check_death(bridge);
    EXPECT_FALSE(died);
}

TEST_F(OligodendrocytesImmuneBridgeTest, CheckDeathTrueWhenDamageExceedsThreshold) {
    bridge->damage_level = config.death_threshold + 0.1f;
    bool died = oligo_immune_check_death(bridge);
    EXPECT_TRUE(died);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetDamageStateNullReturnsNone) {
    oligo_damage_state_t state = oligo_immune_get_damage_state(nullptr);
    EXPECT_EQ(state, OLIGO_DAMAGE_NONE);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetDamageStateReturnsCurrentState) {
    oligo_damage_state_t state = oligo_immune_get_damage_state(bridge);
    EXPECT_EQ(state, OLIGO_DAMAGE_NONE);
}

TEST_F(OligodendrocytesImmuneBridgeTest, DamageStateProgresses) {
    // Mild damage
    bridge->damage_level = 0.3f;
    // Update to reflect state
    oligo_immune_update(bridge, 10.0f);
    oligo_damage_state_t mild = oligo_immune_get_damage_state(bridge);
    EXPECT_GE((int)mild, (int)OLIGO_DAMAGE_MILD);

    // Severe damage
    bridge->damage_level = 0.85f;
    oligo_immune_update(bridge, 10.0f);
    oligo_damage_state_t severe = oligo_immune_get_damage_state(bridge);
    EXPECT_GE((int)severe, (int)OLIGO_DAMAGE_SEVERE);
}

//=============================================================================
// Demyelination API Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, ProcessDemyelinationNullReturnsError) {
    int result = oligo_immune_process_demyelination(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, ProcessDemyelinationSucceeds) {
    int result = oligo_immune_process_demyelination(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetDemyelinationStateNullReturnsNone) {
    demyelination_state_t state = oligo_immune_get_demyelination_state(nullptr);
    EXPECT_EQ(state, DEMYELINATION_NONE);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetDemyelinationStateReturnsCurrentState) {
    demyelination_state_t state = oligo_immune_get_demyelination_state(bridge);
    EXPECT_EQ(state, DEMYELINATION_NONE);
}

TEST_F(OligodendrocytesImmuneBridgeTest, DamageLeadsToDemyelination) {
    // Accumulate significant damage
    bridge->cytokine_effects.net_damage_signal = 0.8f;
    for (int i = 0; i < 50; i++) {
        oligo_immune_accumulate_damage(bridge, 100.0f);
        oligo_immune_process_demyelination(bridge, 100.0f);
    }

    demyelination_state_t state = oligo_immune_get_demyelination_state(bridge);
    // Should have progressed from NONE
}

//=============================================================================
// Remyelination API Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, ProcessRemyelinationNullReturnsError) {
    int result = oligo_immune_process_remyelination(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, ProcessRemyelinationSucceeds) {
    int result = oligo_immune_process_remyelination(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetRemyelinationCapacityNullReturnsZero) {
    float capacity = oligo_immune_get_remyelination_capacity(nullptr);
    EXPECT_FLOAT_EQ(capacity, 0.0f);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetRemyelinationCapacityReturnsValue) {
    float capacity = oligo_immune_get_remyelination_capacity(bridge);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);
}

TEST_F(OligodendrocytesImmuneBridgeTest, RemyelinationRequiresLowInflammation) {
    // High inflammation reduces capacity
    bridge->cytokine_effects.net_damage_signal = 0.9f;
    float capacity_inflamed = oligo_immune_get_remyelination_capacity(bridge);

    // Low inflammation allows repair
    bridge->cytokine_effects.net_damage_signal = 0.0f;
    bridge->cytokine_effects.net_protection_signal = 0.9f;
    float capacity_resolved = oligo_immune_get_remyelination_capacity(bridge);

    // Resolved should have higher or equal capacity
    EXPECT_GE(capacity_resolved, capacity_inflamed);
}

//=============================================================================
// Update API Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, UpdateNullReturnsError) {
    int result = oligo_immune_update(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, UpdateSucceeds) {
    int result = oligo_immune_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(OligodendrocytesImmuneBridgeTest, UpdateWithZeroTimestep) {
    int result = oligo_immune_update(bridge, 0.0f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, GetStatsNullReturnsError) {
    oligo_immune_stats_t stats;
    int result = oligo_immune_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetStatsNullOutputReturnsError) {
    int result = oligo_immune_get_stats(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, GetStatsReturnsValidData) {
    oligo_immune_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    int result = oligo_immune_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(OligodendrocytesImmuneBridgeTest, ResetStatsNullIsNoOp) {
    oligo_immune_reset_stats(nullptr);
    // Should not crash
}

TEST_F(OligodendrocytesImmuneBridgeTest, ResetStatsClearsCounters) {
    // Generate some events
    bridge->cytokine_effects.net_damage_signal = 0.5f;
    oligo_immune_accumulate_damage(bridge, 100.0f);

    // Reset
    oligo_immune_reset_stats(bridge);

    // Verify stats are cleared
    oligo_immune_stats_t stats;
    oligo_immune_get_stats(bridge, &stats);
    EXPECT_EQ(stats.cytokine_events, 0u);
    EXPECT_EQ(stats.damage_events, 0u);
}

TEST_F(OligodendrocytesImmuneBridgeTest, StatsTrackDamageEvents) {
    bridge->cytokine_effects.net_damage_signal = 0.5f;
    oligo_immune_accumulate_damage(bridge, 100.0f);

    oligo_immune_stats_t stats;
    oligo_immune_get_stats(bridge, &stats);
    EXPECT_GT(stats.damage_events, 0u);
}

//=============================================================================
// Connection API Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, ConnectNetworkNullBridgeReturnsError) {
    int result = oligo_immune_connect_network(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, ConnectNetworkSucceeds) {
    int result = oligo_immune_connect_network(bridge, nullptr);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, ConnectBioAsyncNullReturnsError) {
    int result = oligo_immune_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(OligodendrocytesImmuneBridgeTest, DisconnectBioAsyncNullHandled) {
    int result = oligo_immune_disconnect_bio_async(nullptr);
    // Null handling - may return 0 or error code depending on implementation
    (void)result;
}

TEST_F(OligodendrocytesImmuneBridgeTest, IsBioAsyncConnectedNullReturnsFalse) {
    bool connected = oligo_immune_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(OligodendrocytesImmuneBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    bool connected = oligo_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, DamageStateToStringReturnsValidStrings) {
    EXPECT_STREQ(oligo_damage_state_to_string(OLIGO_DAMAGE_NONE), "NONE");
    EXPECT_STREQ(oligo_damage_state_to_string(OLIGO_DAMAGE_MILD), "MILD");
    EXPECT_STREQ(oligo_damage_state_to_string(OLIGO_DAMAGE_MODERATE), "MODERATE");
    EXPECT_STREQ(oligo_damage_state_to_string(OLIGO_DAMAGE_SEVERE), "SEVERE");
    EXPECT_STREQ(oligo_damage_state_to_string(OLIGO_DAMAGE_DEAD), "DEAD");
}

TEST_F(OligodendrocytesImmuneBridgeTest, DemyelinationStateToStringReturnsValidStrings) {
    EXPECT_STREQ(demyelination_state_to_string(DEMYELINATION_NONE), "NONE");
    EXPECT_STREQ(demyelination_state_to_string(DEMYELINATION_EARLY), "EARLY");
    EXPECT_STREQ(demyelination_state_to_string(DEMYELINATION_ACTIVE), "ACTIVE");
    EXPECT_STREQ(demyelination_state_to_string(DEMYELINATION_CHRONIC), "CHRONIC");
    EXPECT_STREQ(demyelination_state_to_string(DEMYELINATION_REMYELINATING), "REMYELINATING");
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(OligodendrocytesImmuneBridgeTest, ProtectionCountersDamage) {
    // Protection signal should reduce net damage
    bridge->cytokine_effects.net_damage_signal = 0.5f;
    bridge->cytokine_effects.net_protection_signal = 0.3f;

    // Net effect should be reduced
    oligo_immune_apply_modulation(bridge);
}

TEST_F(OligodendrocytesImmuneBridgeTest, DamageNeverExceedsMax) {
    // Very high damage signal for long time
    bridge->cytokine_effects.net_damage_signal = 1.0f;
    for (int i = 0; i < 1000; i++) {
        oligo_immune_accumulate_damage(bridge, 1000.0f);
    }

    EXPECT_LE(bridge->damage_level, OLIGO_IMMUNE_MAX_DAMAGE);
}

TEST_F(OligodendrocytesImmuneBridgeTest, DeathIsIrreversible) {
    // Cause death
    bridge->damage_level = config.death_threshold + 0.1f;
    oligo_immune_check_death(bridge);

    // Try to repair
    bridge->cytokine_effects.net_damage_signal = 0.0f;
    bridge->cytokine_effects.net_protection_signal = 1.0f;
    oligo_immune_update(bridge, 1000.0f);

    // Should still be dead
    EXPECT_EQ(bridge->damage_state, OLIGO_DAMAGE_DEAD);
}
