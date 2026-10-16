/**
 * @file test_medulla_immune_bridge.cpp
 * @brief Unit tests for medulla-immune system bridge
 *
 * Tests the bidirectional integration between the medulla oblongata
 * and the brain immune system, including cytokine effects on arousal
 * and medulla state effects on immune function.
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_medulla_immune_bridge.h"
#include "core/medulla/nimcp_medulla.h"
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MedullaImmuneBridgeTest : public ::testing::Test {
protected:
    medulla_immune_bridge_t bridge = nullptr;
    medulla_t medulla = nullptr;
    brain_immune_system_t* immune = nullptr;

    void SetUp() override {
        // Create medulla with default config
        medulla_config_t medulla_config = medulla_default_config();
        medulla = medulla_create(&medulla_config);
        ASSERT_NE(medulla, nullptr);
        ASSERT_EQ(medulla_start(medulla), 0);

        // Create brain immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);
        ASSERT_EQ(brain_immune_start(immune), 0);
    }

    void TearDown() override {
        if (bridge) {
            medulla_immune_destroy(bridge);
            bridge = nullptr;
        }
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }
};

//=============================================================================
// Default Config Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, DefaultConfigSetsValidValues) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);

    // Verify sensible defaults
    EXPECT_TRUE(config.enable_immune_to_medulla);
    EXPECT_TRUE(config.enable_medulla_to_immune);
    EXPECT_TRUE(config.enable_circadian_modulation);

    EXPECT_GT(config.cytokine_sensitivity, 0.0f);
    EXPECT_LE(config.cytokine_sensitivity, 2.0f);
    EXPECT_GT(config.protection_coupling, 0.0f);
    EXPECT_GT(config.arousal_coupling, 0.0f);
    EXPECT_GT(config.circadian_coupling, 0.0f);

    EXPECT_GT(config.update_interval_ms, 0u);
}

//=============================================================================
// Creation Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, CreateWithNullMedulla) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);

    bridge = medulla_immune_create(&config, nullptr, immune);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MedullaImmuneBridgeTest, CreateWithNullImmune) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);

    bridge = medulla_immune_create(&config, medulla, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(MedullaImmuneBridgeTest, CreateWithNullConfig) {
    // Should use default config
    bridge = medulla_immune_create(nullptr, medulla, immune);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(MedullaImmuneBridgeTest, CreateSucceeds) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);

    bridge = medulla_immune_create(&config, medulla, immune);
    EXPECT_NE(bridge, nullptr);
}

//=============================================================================
// Destruction Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, DestroyNullBridge) {
    // Should not crash
    medulla_immune_destroy(nullptr);
}

TEST_F(MedullaImmuneBridgeTest, DestroyValidBridge) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    medulla_immune_destroy(bridge);
    bridge = nullptr;  // Prevent double destroy in TearDown
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, UpdateWithNullBridge) {
    int result = medulla_immune_update(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, UpdateSucceeds) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    int result = medulla_immune_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, MultipleUpdates) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 100; i++) {
        int result = medulla_immune_update(bridge);
        EXPECT_EQ(result, 0);
    }
}

//=============================================================================
// Immune -> Medulla Pathway Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, UpdateImmuneTOMedullaWithNullBridge) {
    int result = medulla_immune_update_immune_to_medulla(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, UpdateImmuneToMedullaSucceeds) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    medulla_cytokine_effects_t effects;
    int result = medulla_immune_update_immune_to_medulla(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, UpdateImmuneToMedullaEffectsInRange) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    medulla_cytokine_effects_t effects;
    int result = medulla_immune_update_immune_to_medulla(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Arousal modulation should be in valid range
    EXPECT_GE(effects.arousal_modulation, -1.0f);
    EXPECT_LE(effects.arousal_modulation, 1.0f);

    // Inflammation arousal factor should be positive
    EXPECT_GT(effects.inflammation_arousal_factor, 0.0f);
    EXPECT_LE(effects.inflammation_arousal_factor, 1.0f);
}

//=============================================================================
// Medulla -> Immune Pathway Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, UpdateMedullaToImmuneWithNullBridge) {
    int result = medulla_immune_update_medulla_to_immune(nullptr, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, UpdateMedullaToImmuneSucceeds) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    medulla_immune_effects_t effects;
    int result = medulla_immune_update_medulla_to_immune(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, UpdateMedullaToImmuneEffectsInRange) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    medulla_immune_effects_t effects;
    int result = medulla_immune_update_medulla_to_immune(bridge, &effects);
    EXPECT_EQ(result, 0);

    // All modulation factors should be positive
    EXPECT_GT(effects.arousal_immune_factor, 0.0f);
    EXPECT_GT(effects.protection_immune_factor, 0.0f);
    EXPECT_GT(effects.circadian_immune_factor, 0.0f);
    EXPECT_GT(effects.combined_immune_factor, 0.0f);
}

//=============================================================================
// Query Functions Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, GetCytokineEffectsWithNullBridge) {
    medulla_cytokine_effects_t effects;
    int result = medulla_immune_get_cytokine_effects(nullptr, &effects);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, GetCytokineEffectsWithNullOutput) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    int result = medulla_immune_get_cytokine_effects(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, GetCytokineEffectsSucceeds) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    // Do an update first
    medulla_immune_update(bridge);

    medulla_cytokine_effects_t effects;
    int result = medulla_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, GetImmuneEffectsWithNullBridge) {
    medulla_immune_effects_t effects;
    int result = medulla_immune_get_immune_effects(nullptr, &effects);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, GetImmuneEffectsSucceeds) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    // Do an update first
    medulla_immune_update(bridge);

    medulla_immune_effects_t effects;
    int result = medulla_immune_get_immune_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, InflammationArousalDecreaseWithLevel) {
    float none = medulla_immune_compute_inflammation_arousal(INFLAMMATION_NONE);
    float local = medulla_immune_compute_inflammation_arousal(INFLAMMATION_LOCAL);
    float regional = medulla_immune_compute_inflammation_arousal(INFLAMMATION_REGIONAL);
    float systemic = medulla_immune_compute_inflammation_arousal(INFLAMMATION_SYSTEMIC);
    float storm = medulla_immune_compute_inflammation_arousal(INFLAMMATION_STORM);

    // Arousal should decrease with inflammation level
    EXPECT_GE(none, local);
    EXPECT_GE(local, regional);
    EXPECT_GE(regional, systemic);
    EXPECT_GE(systemic, storm);

    // All values should be in valid range
    EXPECT_GT(none, 0.0f);
    EXPECT_LE(none, 1.0f);
    EXPECT_GT(storm, 0.0f);
    EXPECT_LE(storm, 1.0f);
}

TEST_F(MedullaImmuneBridgeTest, ProtectionImmuneIncreaseWithLevel) {
    float normal = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_NORMAL);
    float cautious = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_CAUTIOUS);
    float guarded = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_GUARDED);
    float defensive = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_DEFENSIVE);
    float critical = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_CRITICAL);

    // Immune activity should generally increase with protection level
    EXPECT_LE(normal, cautious);
    EXPECT_LE(cautious, guarded);
    EXPECT_LE(guarded, defensive);
    EXPECT_LE(defensive, critical);

    // All values should be positive
    EXPECT_GT(normal, 0.0f);
    EXPECT_GT(critical, 0.0f);
}

TEST_F(MedullaImmuneBridgeTest, CircadianImmuneDayNightDifference) {
    float morning = medulla_immune_compute_circadian_immune(CIRCADIAN_PHASE_MORNING);
    float afternoon = medulla_immune_compute_circadian_immune(CIRCADIAN_PHASE_AFTERNOON);
    float night = medulla_immune_compute_circadian_immune(CIRCADIAN_PHASE_NIGHT);
    float deep_night = medulla_immune_compute_circadian_immune(CIRCADIAN_PHASE_DEEP_NIGHT);

    // Day phases should have higher immune activity than night
    EXPECT_GT(morning, night);
    EXPECT_GT(afternoon, deep_night);

    // All values should be positive
    EXPECT_GT(morning, 0.0f);
    EXPECT_GT(night, 0.0f);
}

TEST_F(MedullaImmuneBridgeTest, CircadianAllPhasesValid) {
    // Test all circadian phases produce valid values
    circadian_phase_t phases[] = {
        CIRCADIAN_PHASE_EARLY_MORNING,
        CIRCADIAN_PHASE_MORNING,
        CIRCADIAN_PHASE_AFTERNOON,
        CIRCADIAN_PHASE_EVENING,
        CIRCADIAN_PHASE_LATE_EVENING,
        CIRCADIAN_PHASE_NIGHT,
        CIRCADIAN_PHASE_DEEP_NIGHT,
        CIRCADIAN_PHASE_PRE_DAWN
    };

    for (circadian_phase_t phase : phases) {
        float factor = medulla_immune_compute_circadian_immune(phase);
        EXPECT_GT(factor, 0.0f);
        EXPECT_LE(factor, 2.0f);
    }
}

//=============================================================================
// Stats Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, GetStatsWithNullBridge) {
    medulla_immune_stats_t stats;
    int result = medulla_immune_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, GetStatsWithNullStats) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    int result = medulla_immune_get_stats(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, GetStatsSucceeds) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    // Do some updates to accumulate stats
    for (int i = 0; i < 10; i++) {
        medulla_immune_update(bridge);
    }

    medulla_immune_stats_t stats;
    int result = medulla_immune_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_GE(stats.total_updates, 10u);
}

//=============================================================================
// Bio-Async Integration Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, ConnectBioAsyncWithNullBridge) {
    int result = medulla_immune_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, DisconnectBioAsyncWithNullBridge) {
    int result = medulla_immune_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, IsBioAsyncConnectedWithNullBridge) {
    bool connected = medulla_immune_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(MedullaImmuneBridgeTest, BioAsyncConnectDisconnectCycle) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    // Initially not connected
    EXPECT_FALSE(medulla_immune_is_bio_async_connected(bridge));

    // Connect (may fail if bio-async not available, that's OK)
    medulla_immune_connect_bio_async(bridge);

    // Disconnect
    medulla_immune_disconnect_bio_async(bridge);
    EXPECT_FALSE(medulla_immune_is_bio_async_connected(bridge));
}

//=============================================================================
// Feature Toggle Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, DisabledImmuneToMedulla) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    config.enable_immune_to_medulla = false;

    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    int result = medulla_immune_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, DisabledMedullaToImmune) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    config.enable_medulla_to_immune = false;

    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    int result = medulla_immune_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, DisabledCircadianModulation) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    config.enable_circadian_modulation = false;

    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    int result = medulla_immune_update(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Sensitivity Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, HighCytokineSensitivity) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    config.cytokine_sensitivity = 2.0f;  // Double sensitivity

    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    int result = medulla_immune_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(MedullaImmuneBridgeTest, LowCytokineSensitivity) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    config.cytokine_sensitivity = 0.5f;  // Half sensitivity

    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    int result = medulla_immune_update(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(MedullaImmuneBridgeTest, FullUpdateCycle) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    // Run a full update cycle
    for (int i = 0; i < 50; i++) {
        // Update medulla
        medulla_update(medulla, 0.02f);

        // Update bridge
        int result = medulla_immune_update(bridge);
        EXPECT_EQ(result, 0);
    }

    // Get final stats
    medulla_immune_stats_t stats;
    medulla_immune_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_updates, 50u);
}

TEST_F(MedullaImmuneBridgeTest, BothPathwaysActive) {
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);
    config.enable_immune_to_medulla = true;
    config.enable_medulla_to_immune = true;

    bridge = medulla_immune_create(&config, medulla, immune);
    ASSERT_NE(bridge, nullptr);

    // Run several updates
    for (int i = 0; i < 20; i++) {
        medulla_update(medulla, 0.05f);
        int result = medulla_immune_update(bridge);
        EXPECT_EQ(result, 0);
    }

    // Verify updates were counted (direction-specific counts may be 0 if no actual changes)
    medulla_immune_stats_t stats;
    medulla_immune_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_updates, 20u);
    // Note: immune_to_medulla_count and medulla_to_immune_count may be 0
    // if there are no actual cytokine effects to apply (normal baseline state)
}
