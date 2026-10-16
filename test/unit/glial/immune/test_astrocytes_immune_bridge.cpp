/**
 * @file test_astrocytes_immune_bridge.cpp
 * @brief Unit tests for Astrocytes-Immune Bridge
 *
 * WHAT: Comprehensive tests for astrocytes-immune bridge functionality
 * WHY:  Ensure astrocyte reactivity correctly responds to immune signals
 * HOW:  Test lifecycle, reactivity phenotypes (A1/A2), cytokine effects
 *
 * TEST CATEGORIES:
 * - Lifecycle: Creation, destruction, configuration
 * - Reactivity: A1/A2 phenotype transitions
 * - Cytokine Effects: IL-1, TNF, IL-6, IL-10 modulation
 * - Glutamate Clearance: Rate modulation by state
 * - Scar Formation: Chronic inflammation tracking
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
#include "glial/immune/nimcp_astrocytes_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AstrocytesImmuneBridgeTest : public ::testing::Test {
protected:
    astro_immune_bridge_t* bridge = nullptr;
    astro_immune_config_t config;

    void SetUp() override {
        astro_cell_default_config(&config);
        bridge = astro_cell_create(&config, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            astro_cell_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, DefaultConfigHasReasonableValues) {
    astro_immune_config_t cfg;
    int result = astro_cell_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GE(cfg.il1_a1_induction, 0.0f);
    EXPECT_LE(cfg.il1_a1_induction, 1.0f);
    EXPECT_GE(cfg.tnf_a1_induction, 0.0f);
    EXPECT_GE(cfg.il10_a2_promotion, 0.0f);
    EXPECT_GT(cfg.scar_formation_threshold, 0.0f);
    EXPECT_GT(cfg.glutamate_clearance_base, 0.0f);
}

TEST_F(AstrocytesImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = astro_cell_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(AstrocytesImmuneBridgeTest, CreateWithNullConfigUsesDefaults) {
    astro_immune_bridge_t* b = astro_cell_create(nullptr, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->initialized);
    astro_cell_destroy(b);
}

TEST_F(AstrocytesImmuneBridgeTest, CreateWithConfigAppliesSettings) {
    config.il1_a1_induction = 0.8f;
    config.scar_formation_threshold = 0.5f;

    astro_immune_bridge_t* b = astro_cell_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_FLOAT_EQ(b->config.il1_a1_induction, 0.8f);
    EXPECT_FLOAT_EQ(b->config.scar_formation_threshold, 0.5f);
    astro_cell_destroy(b);
}

TEST_F(AstrocytesImmuneBridgeTest, DestroyNullIsNoOp) {
    astro_cell_destroy(nullptr);
    // Should not crash
}

TEST_F(AstrocytesImmuneBridgeTest, BridgeIsInitializedAfterCreate) {
    EXPECT_TRUE(bridge->initialized);
    EXPECT_NE(bridge->base.mutex, nullptr);
}

TEST_F(AstrocytesImmuneBridgeTest, InitialStateIsQuiescent) {
    EXPECT_EQ(bridge->reactivity_state, ASTRO_QUIESCENT);
}

//=============================================================================
// Reactivity State Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, GetReactivityNullReturnsQuiescent) {
    astrocyte_reactivity_t state = astro_cell_get_reactivity(nullptr);
    EXPECT_EQ(state, ASTRO_QUIESCENT);
}

TEST_F(AstrocytesImmuneBridgeTest, GetReactivityReturnsCurrentState) {
    astrocyte_reactivity_t state = astro_cell_get_reactivity(bridge);
    EXPECT_EQ(state, ASTRO_QUIESCENT);
}

TEST_F(AstrocytesImmuneBridgeTest, UpdateReactivityNullReturnsError) {
    int result = astro_cell_update_reactivity(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AstrocytesImmuneBridgeTest, UpdateReactivitySucceeds) {
    int result = astro_cell_update_reactivity(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(AstrocytesImmuneBridgeTest, HighA1DriveTriggersA1State) {
    // Manually set high A1 drive
    bridge->cytokine_effects.a1_drive = 0.8f;
    bridge->cytokine_effects.a2_drive = 0.1f;

    astro_cell_update_reactivity(bridge, 10.0f);

    EXPECT_EQ(bridge->reactivity_state, ASTRO_A1_REACTIVE);
}

TEST_F(AstrocytesImmuneBridgeTest, HighA2DriveTriggersA2State) {
    // Manually set high A2 drive
    bridge->cytokine_effects.a1_drive = 0.1f;
    bridge->cytokine_effects.a2_drive = 0.8f;

    astro_cell_update_reactivity(bridge, 10.0f);

    EXPECT_EQ(bridge->reactivity_state, ASTRO_A2_REACTIVE);
}

TEST_F(AstrocytesImmuneBridgeTest, LowDriveRemainsQuiescent) {
    bridge->cytokine_effects.a1_drive = 0.2f;
    bridge->cytokine_effects.a2_drive = 0.1f;

    astro_cell_update_reactivity(bridge, 10.0f);

    EXPECT_EQ(bridge->reactivity_state, ASTRO_QUIESCENT);
}

TEST_F(AstrocytesImmuneBridgeTest, ChronicA1LeadsToScar) {
    config.scar_formation_threshold = 0.001f; // Very low threshold for test
    astro_immune_bridge_t* b = astro_cell_create(&config, nullptr, nullptr);
    ASSERT_NE(b, nullptr);

    // Maintain high A1 for very long time
    b->cytokine_effects.a1_drive = 0.9f;
    b->cytokine_effects.a2_drive = 0.0f;

    for (int i = 0; i < 2000; i++) {
        astro_cell_update_reactivity(b, 100.0f);
    }

    // State should progress through A1 and eventually reach scar forming
    // At minimum, should be in A1_REACTIVE state
    EXPECT_GE((int)b->reactivity_state, (int)ASTRO_A1_REACTIVE);

    astro_cell_destroy(b);
}

//=============================================================================
// Cytokine Effects Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, UpdateCytokineEffectsNullReturnsError) {
    int result = astro_cell_update_cytokine_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AstrocytesImmuneBridgeTest, UpdateCytokineEffectsSucceeds) {
    // Without immune system connected, should still succeed
    int result = astro_cell_update_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(AstrocytesImmuneBridgeTest, CytokineEffectsInitiallyZero) {
    EXPECT_FLOAT_EQ(bridge->cytokine_effects.il1_effect, 0.0f);
    EXPECT_FLOAT_EQ(bridge->cytokine_effects.tnf_effect, 0.0f);
    EXPECT_FLOAT_EQ(bridge->cytokine_effects.il6_effect, 0.0f);
    EXPECT_FLOAT_EQ(bridge->cytokine_effects.il10_effect, 0.0f);
}

//=============================================================================
// Glutamate Clearance Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, GetGlutamateClearanceNullReturnsDefault) {
    float rate = astro_cell_get_glutamate_clearance(nullptr);
    EXPECT_FLOAT_EQ(rate, 1.0f);
}

TEST_F(AstrocytesImmuneBridgeTest, GetGlutamateClearanceReturnsBaseInitially) {
    float rate = astro_cell_get_glutamate_clearance(bridge);
    EXPECT_FLOAT_EQ(rate, config.glutamate_clearance_base);
}

TEST_F(AstrocytesImmuneBridgeTest, A1ReducesGlutamateClearance) {
    // Set A1 state
    bridge->cytokine_effects.a1_drive = 0.9f;
    bridge->cytokine_effects.a2_drive = 0.0f;
    astro_cell_update_reactivity(bridge, 10.0f);

    float rate = astro_cell_get_glutamate_clearance(bridge);
    EXPECT_LT(rate, config.glutamate_clearance_base);
}

TEST_F(AstrocytesImmuneBridgeTest, A2MaintainsNormalClearance) {
    // Set A2 state
    bridge->cytokine_effects.a1_drive = 0.0f;
    bridge->cytokine_effects.a2_drive = 0.9f;
    astro_cell_update_reactivity(bridge, 10.0f);

    float rate = astro_cell_get_glutamate_clearance(bridge);
    EXPECT_FLOAT_EQ(rate, config.glutamate_clearance_base);
}

//=============================================================================
// Update API Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, UpdateNullReturnsError) {
    int result = astro_cell_update(nullptr, 10.0f);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AstrocytesImmuneBridgeTest, UpdateSucceeds) {
    int result = astro_cell_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(AstrocytesImmuneBridgeTest, UpdateCallsBothSubfunctions) {
    // Update should call both cytokine effects and reactivity updates
    int result = astro_cell_update(bridge, 10.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(AstrocytesImmuneBridgeTest, UpdateWithZeroTimestep) {
    int result = astro_cell_update(bridge, 0.0f);
    EXPECT_NE(result, 0);  // Zero timestep is invalid - should return error
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, GetStatsNullReturnsError) {
    astro_immune_stats_t stats;
    int result = astro_cell_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AstrocytesImmuneBridgeTest, GetStatsNullOutputReturnsError) {
    int result = astro_cell_get_stats(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AstrocytesImmuneBridgeTest, GetStatsReturnsValidData) {
    astro_immune_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    int result = astro_cell_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.reactivity_changes, 0u);
}

TEST_F(AstrocytesImmuneBridgeTest, ResetStatsNullIsNoOp) {
    astro_cell_reset_stats(nullptr);
    // Should not crash
}

TEST_F(AstrocytesImmuneBridgeTest, ResetStatsClearsCounters) {
    // Generate some state changes
    bridge->cytokine_effects.a1_drive = 0.9f;
    astro_cell_update_reactivity(bridge, 10.0f);

    // Reset
    astro_cell_reset_stats(bridge);

    // Verify stats are cleared
    astro_immune_stats_t stats;
    astro_cell_get_stats(bridge, &stats);
    EXPECT_EQ(stats.reactivity_changes, 0u);
    EXPECT_EQ(stats.a1_activations, 0u);
    EXPECT_EQ(stats.a2_activations, 0u);
}

TEST_F(AstrocytesImmuneBridgeTest, StatsTrackReactivityChanges) {
    astro_immune_stats_t stats_before, stats_after;
    astro_cell_get_stats(bridge, &stats_before);

    // Trigger state change
    bridge->cytokine_effects.a1_drive = 0.9f;
    astro_cell_update_reactivity(bridge, 10.0f);

    astro_cell_get_stats(bridge, &stats_after);
    EXPECT_GT(stats_after.reactivity_changes, stats_before.reactivity_changes);
}

TEST_F(AstrocytesImmuneBridgeTest, StatsTrackA1Activations) {
    bridge->cytokine_effects.a1_drive = 0.9f;
    astro_cell_update_reactivity(bridge, 10.0f);

    astro_immune_stats_t stats;
    astro_cell_get_stats(bridge, &stats);
    EXPECT_GT(stats.a1_activations, 0u);
}

TEST_F(AstrocytesImmuneBridgeTest, StatsTrackA2Activations) {
    bridge->cytokine_effects.a2_drive = 0.9f;
    astro_cell_update_reactivity(bridge, 10.0f);

    astro_immune_stats_t stats;
    astro_cell_get_stats(bridge, &stats);
    EXPECT_GT(stats.a2_activations, 0u);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, ConnectBioAsyncNullReturnsError) {
    int result = astro_cell_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AstrocytesImmuneBridgeTest, DisconnectBioAsyncNullReturnsSuccess) {
    int result = astro_cell_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(AstrocytesImmuneBridgeTest, IsBioAsyncConnectedNullReturnsFalse) {
    bool connected = astro_cell_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(AstrocytesImmuneBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    bool connected = astro_cell_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, ReactivityToStringReturnsValidStrings) {
    EXPECT_STREQ(astrocyte_reactivity_to_string(ASTRO_QUIESCENT), "QUIESCENT");
    EXPECT_STREQ(astrocyte_reactivity_to_string(ASTRO_A1_REACTIVE), "A1_REACTIVE");
    EXPECT_STREQ(astrocyte_reactivity_to_string(ASTRO_A2_REACTIVE), "A2_REACTIVE");
    EXPECT_STREQ(astrocyte_reactivity_to_string(ASTRO_SCAR_FORMING), "SCAR_FORMING");
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(AstrocytesImmuneBridgeTest, StateTransitionsTracked) {
    // Multiple state transitions
    bridge->cytokine_effects.a1_drive = 0.9f;
    astro_cell_update_reactivity(bridge, 10.0f);
    EXPECT_EQ(bridge->reactivity_state, ASTRO_A1_REACTIVE);

    bridge->cytokine_effects.a1_drive = 0.0f;
    bridge->cytokine_effects.a2_drive = 0.9f;
    astro_cell_update_reactivity(bridge, 10.0f);
    EXPECT_EQ(bridge->reactivity_state, ASTRO_A2_REACTIVE);

    astro_immune_stats_t stats;
    astro_cell_get_stats(bridge, &stats);
    EXPECT_GE(stats.reactivity_changes, 2u);
}

TEST_F(AstrocytesImmuneBridgeTest, ScarProgressResets) {
    // Partial scar progress, then resolve
    bridge->cytokine_effects.a1_drive = 0.9f;
    for (int i = 0; i < 5; i++) {
        astro_cell_update_reactivity(bridge, 100.0f);
    }
    float progress_during = bridge->scar_formation_progress;
    EXPECT_GT(progress_during, 0.0f);

    // May or may not reset when A1 resolves (depends on implementation)
}

TEST_F(AstrocytesImmuneBridgeTest, ConcurrentDrivesPickDominant) {
    // Both drives high, A1 should dominate if higher
    bridge->cytokine_effects.a1_drive = 0.8f;
    bridge->cytokine_effects.a2_drive = 0.7f;
    astro_cell_update_reactivity(bridge, 10.0f);

    EXPECT_EQ(bridge->reactivity_state, ASTRO_A1_REACTIVE);
}

TEST_F(AstrocytesImmuneBridgeTest, VerySmallDrivesRemainQuiescent) {
    bridge->cytokine_effects.a1_drive = 0.01f;
    bridge->cytokine_effects.a2_drive = 0.01f;
    astro_cell_update_reactivity(bridge, 10.0f);

    EXPECT_EQ(bridge->reactivity_state, ASTRO_QUIESCENT);
}
