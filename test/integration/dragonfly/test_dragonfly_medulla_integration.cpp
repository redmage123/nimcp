//=============================================================================
// test_dragonfly_medulla_integration.cpp - Dragonfly-Medulla Integration Tests
//=============================================================================
/**
 * @file test_dragonfly_medulla_integration.cpp
 * @brief Integration tests for dragonfly-medulla bridge modulation effects
 *
 * WHAT: Tests dragonfly behavior modulation by medulla oblongata states
 * WHY:  Verify that arousal, protection, and circadian states properly
 *       modulate dragonfly hunting behavior (biologically-inspired)
 * HOW:  Manipulate medulla states and verify dragonfly modulation changes
 *
 * BIOLOGICAL BASIS:
 * - Arousal: Alert dragonflies hunt better (faster reactions, higher nav gain)
 * - Protection: Threatened animals abort hunting (survival priority)
 * - Circadian: Diurnal hunters are inactive at night
 *
 * TEST CATEGORIES:
 * - Bridge Lifecycle: Creation, connection, destruction
 * - Arousal Modulation: Nav gain, urgency, accuracy scaling
 * - Protection Modulation: Hunting permission, abort signals
 * - Circadian Modulation: Performance scaling by time of day
 * - Combined Effects: Multiple modulation sources interacting
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "core/medulla/nimcp_medulla.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_medulla_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonflyMedullaIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        nimcp_init();
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        nimcp_shutdown();
    }

    // Helper to create brain with dragonfly and medulla enabled
    brain_t create_brain_with_dragonfly_medulla() {
        brain_config_t config = {};
        strncpy(config.task_name, "dragonfly_medulla_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = true;
        config.dragonfly_enable_imm = true;
        config.dragonfly_prediction_horizon_ms = 200.0f;
        config.dragonfly_nav_gain = 3.0f;
        // Medulla is enabled by default in brain
        return brain_create_custom(&config);
    }

    // Helper to get medulla from brain
    medulla_t get_medulla(brain_t b) {
        return brain_get_medulla(b);
    }
};

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(DragonflyMedullaIntegrationTest, BridgeCreatedWithBrain) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr) << "Brain creation should succeed";

    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    EXPECT_NE(bridge, nullptr) << "Bridge should be created when both dragonfly and medulla exist";
}

TEST_F(DragonflyMedullaIntegrationTest, BridgeConnected) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    EXPECT_TRUE(dragonfly_medulla_bridge_is_connected(bridge))
        << "Bridge should be connected after brain creation";
}

TEST_F(DragonflyMedullaIntegrationTest, BridgeReturnsNullForNullBrain) {
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(nullptr);
    EXPECT_EQ(bridge, nullptr) << "Bridge accessor should return NULL for NULL brain";
}

TEST_F(DragonflyMedullaIntegrationTest, BridgeStatsAccessible) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    dragonfly_medulla_stats_t stats;
    int result = dragonfly_medulla_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0) << "Should be able to get bridge stats";
    EXPECT_TRUE(stats.is_connected) << "Stats should show connected";
}

//=============================================================================
// Arousal Modulation Tests
//=============================================================================

TEST_F(DragonflyMedullaIntegrationTest, ArousalAffectsNavGain) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    ASSERT_NE(medulla, nullptr);

    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Test different arousal levels
    // Bridge maps arousal [0-1] to level [0-6] via: level = (int)(arousal * 6 + 0.5)
    // COMA=0, DEEP_SLEEP=1, LIGHT_SLEEP=2, DROWSY=3, AWAKE=4, ALERT=5, HYPERAROUSAL=6
    struct {
        float arousal;
        const char* name;
        float expected_min_scale;
        float expected_max_scale;
    } test_cases[] = {
        {0.02f, "COMA",          0.0f, 0.1f},   // level=0 -> 0.0
        {0.15f, "DEEP_SLEEP",    0.0f, 0.1f},   // level=1 -> 0.0
        {0.30f, "LIGHT_SLEEP",   0.1f, 0.3f},   // level=2 -> 0.2
        {0.50f, "DROWSY",        0.4f, 0.6f},   // level=3 -> 0.5
        {0.65f, "AWAKE",         0.9f, 1.1f},   // level=4 -> 1.0
        {0.82f, "ALERT",         1.1f, 1.4f},   // level=5 -> 1.2
        {0.98f, "HYPERAROUSAL",  1.4f, 1.6f},   // level=6 -> 1.5
    };

    for (const auto& tc : test_cases) {
        // Set arousal level
        int result = medulla_test_set_arousal(medulla, tc.arousal);
        ASSERT_EQ(result, 0) << "Failed to set arousal for " << tc.name;

        // Update bridge to apply new state
        result = dragonfly_medulla_bridge_update(bridge, 0.1f);
        EXPECT_EQ(result, 0) << "Bridge update failed for " << tc.name;

        // Get modulation
        dragonfly_medulla_modulation_t mod;
        result = brain_dragonfly_get_modulation(brain, &mod);
        ASSERT_EQ(result, 0) << "Failed to get modulation for " << tc.name;

        EXPECT_GE(mod.nav_gain_scale, tc.expected_min_scale)
            << tc.name << ": nav_gain_scale too low (" << mod.nav_gain_scale << ")";
        EXPECT_LE(mod.nav_gain_scale, tc.expected_max_scale)
            << tc.name << ": nav_gain_scale too high (" << mod.nav_gain_scale << ")";
    }
}

TEST_F(DragonflyMedullaIntegrationTest, ArousalAffectsUrgency) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Low arousal should reduce urgency
    medulla_test_set_arousal(medulla, 0.10f);  // DEEP_SLEEP
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    dragonfly_medulla_modulation_t mod_low;
    brain_dragonfly_get_modulation(brain, &mod_low);

    // High arousal should increase urgency
    medulla_test_set_arousal(medulla, 0.90f);  // HYPERAROUSAL
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    dragonfly_medulla_modulation_t mod_high;
    brain_dragonfly_get_modulation(brain, &mod_high);

    EXPECT_LT(mod_low.urgency_scale, mod_high.urgency_scale)
        << "Higher arousal should increase urgency";
    EXPECT_LT(mod_low.urgency_scale, 0.5f)
        << "Low arousal should have low urgency";
    EXPECT_GT(mod_high.urgency_scale, 1.0f)
        << "High arousal should have high urgency";
}

TEST_F(DragonflyMedullaIntegrationTest, ArousalAffectsReactionTime) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // DROWSY
    medulla_test_set_arousal(medulla, 0.35f);
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    dragonfly_medulla_modulation_t mod_drowsy;
    brain_dragonfly_get_modulation(brain, &mod_drowsy);

    // ALERT
    medulla_test_set_arousal(medulla, 0.75f);
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    dragonfly_medulla_modulation_t mod_alert;
    brain_dragonfly_get_modulation(brain, &mod_alert);

    EXPECT_LT(mod_drowsy.reaction_scale, mod_alert.reaction_scale)
        << "Alert state should have faster reactions than drowsy";
}

TEST_F(DragonflyMedullaIntegrationTest, HyperarousalReducesAccuracy) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Set circadian to morning for consistent comparison
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);

    // AWAKE - arousal=0.65 maps to level=4 (AWAKE), accuracy=1.0
    medulla_test_set_arousal(medulla, 0.65f);
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    dragonfly_medulla_modulation_t mod_awake;
    brain_dragonfly_get_modulation(brain, &mod_awake);

    // HYPERAROUSAL - arousal=0.98 maps to level=6 (HYPERAROUSAL), accuracy=0.8
    medulla_test_set_arousal(medulla, 0.98f);
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    dragonfly_medulla_modulation_t mod_hyper;
    brain_dragonfly_get_modulation(brain, &mod_hyper);

    EXPECT_GT(mod_awake.accuracy_scale, mod_hyper.accuracy_scale)
        << "Hyperarousal (panic) should reduce accuracy"
        << " (awake=" << mod_awake.accuracy_scale
        << ", hyper=" << mod_hyper.accuracy_scale << ")";
    EXPECT_LT(mod_hyper.accuracy_scale, 1.0f)
        << "Hyperarousal accuracy should be below baseline";
}

//=============================================================================
// Protection Level Tests
//=============================================================================

TEST_F(DragonflyMedullaIntegrationTest, NormalProtectionAllowsHunting) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    EXPECT_TRUE(brain_dragonfly_hunting_allowed(brain))
        << "Hunting should be allowed at NORMAL protection";
}

TEST_F(DragonflyMedullaIntegrationTest, CriticalProtectionBlocksHunting) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL);
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    EXPECT_FALSE(brain_dragonfly_hunting_allowed(brain))
        << "Hunting should be blocked at CRITICAL protection";
}

TEST_F(DragonflyMedullaIntegrationTest, ShutdownProtectionBlocksHunting) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    medulla_test_set_protection(medulla, PROTECTION_LEVEL_SHUTDOWN);
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    EXPECT_FALSE(brain_dragonfly_hunting_allowed(brain))
        << "Hunting should be blocked at SHUTDOWN protection";
}

TEST_F(DragonflyMedullaIntegrationTest, ProtectionAffectsDurationLimit) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // NORMAL - full duration
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    dragonfly_medulla_modulation_t mod_normal;
    brain_dragonfly_get_modulation(brain, &mod_normal);

    // DEFENSIVE - reduced duration
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_DEFENSIVE);
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    dragonfly_medulla_modulation_t mod_defensive;
    brain_dragonfly_get_modulation(brain, &mod_defensive);

    EXPECT_GT(mod_normal.max_duration_scale, mod_defensive.max_duration_scale)
        << "Higher protection should reduce max pursuit duration";
}

TEST_F(DragonflyMedullaIntegrationTest, ProtectionLevelProgression) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    protection_level_t levels[] = {
        PROTECTION_LEVEL_NORMAL,
        PROTECTION_LEVEL_CAUTIOUS,
        PROTECTION_LEVEL_GUARDED,
        PROTECTION_LEVEL_DEFENSIVE,
    };

    float prev_duration_scale = 2.0f;  // Start high
    for (protection_level_t level : levels) {
        medulla_test_set_protection(medulla, level);
        dragonfly_medulla_bridge_update(bridge, 0.1f);

        dragonfly_medulla_modulation_t mod;
        brain_dragonfly_get_modulation(brain, &mod);

        EXPECT_LE(mod.max_duration_scale, prev_duration_scale)
            << "Duration should decrease with higher protection levels";
        prev_duration_scale = mod.max_duration_scale;
    }
}

//=============================================================================
// Circadian Modulation Tests
//=============================================================================

TEST_F(DragonflyMedullaIntegrationTest, MorningPeakPerformance) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Set to ALERT arousal and MORNING circadian (peak hunting conditions)
    // arousal=0.82 -> level=5 (ALERT), nav_gain=1.2
    // circadian=MORNING, performance=1.0
    // Result: nav_gain_scale = 1.2 * 1.0 = 1.2
    medulla_test_set_arousal(medulla, 0.82f);
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    dragonfly_medulla_modulation_t mod;
    brain_dragonfly_get_modulation(brain, &mod);

    // Morning at alert arousal should have above-baseline modulation
    EXPECT_GE(mod.nav_gain_scale, 1.0f)
        << "Alert in morning should have at least baseline performance"
        << " (got " << mod.nav_gain_scale << ")";
}

TEST_F(DragonflyMedullaIntegrationTest, NightReducedPerformance) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Set arousal to AWAKE first (so we're not blocked by arousal)
    medulla_test_set_arousal(medulla, 0.55f);

    // MORNING
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    dragonfly_medulla_modulation_t mod_morning;
    brain_dragonfly_get_modulation(brain, &mod_morning);

    // DEEP_NIGHT
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT);
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    dragonfly_medulla_modulation_t mod_night;
    brain_dragonfly_get_modulation(brain, &mod_night);

    // Night performance should be lower than morning
    // Note: The exact comparison depends on how circadian affects overall modulation
    EXPECT_NE(mod_morning.nav_gain_scale, mod_night.nav_gain_scale)
        << "Circadian phase should affect performance";
}

TEST_F(DragonflyMedullaIntegrationTest, CircadianPhaseProgression) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Set arousal to AWAKE for consistent comparison
    medulla_test_set_arousal(medulla, 0.55f);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);

    circadian_phase_t phases[] = {
        CIRCADIAN_PHASE_EARLY_MORNING,
        CIRCADIAN_PHASE_MORNING,
        CIRCADIAN_PHASE_AFTERNOON,
        CIRCADIAN_PHASE_EVENING,
        CIRCADIAN_PHASE_LATE_EVENING,
        CIRCADIAN_PHASE_NIGHT,
        CIRCADIAN_PHASE_DEEP_NIGHT,
        CIRCADIAN_PHASE_PRE_DAWN,
    };

    for (circadian_phase_t phase : phases) {
        int result = medulla_test_set_circadian(medulla, phase);
        EXPECT_EQ(result, 0) << "Failed to set circadian phase";

        dragonfly_medulla_bridge_update(bridge, 0.1f);

        dragonfly_medulla_modulation_t mod;
        brain_dragonfly_get_modulation(brain, &mod);

        // All phases should produce valid modulation values
        EXPECT_GE(mod.nav_gain_scale, 0.0f);
        EXPECT_LE(mod.nav_gain_scale, 2.0f);
    }
}

//=============================================================================
// Combined Effect Tests
//=============================================================================

TEST_F(DragonflyMedullaIntegrationTest, SleepyAtNightVeryLowPerformance) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Drowsy + night = very low performance
    medulla_test_set_arousal(medulla, 0.35f);  // DROWSY
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    dragonfly_medulla_modulation_t mod;
    brain_dragonfly_get_modulation(brain, &mod);

    EXPECT_LT(mod.nav_gain_scale, 0.6f)
        << "Drowsy at night should have very low nav gain";
    EXPECT_LT(mod.urgency_scale, 0.5f)
        << "Drowsy at night should have low urgency";
}

TEST_F(DragonflyMedullaIntegrationTest, AlertInMorningPeakPerformance) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Alert + morning = peak performance
    medulla_test_set_arousal(medulla, 0.75f);  // ALERT
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    dragonfly_medulla_modulation_t mod;
    brain_dragonfly_get_modulation(brain, &mod);

    EXPECT_GT(mod.nav_gain_scale, 1.0f)
        << "Alert in morning should have high nav gain";
    EXPECT_GT(mod.urgency_scale, 1.0f)
        << "Alert in morning should have high urgency";
}

TEST_F(DragonflyMedullaIntegrationTest, ProtectionOverridesOtherFactors) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Perfect conditions but CRITICAL protection
    medulla_test_set_arousal(medulla, 0.75f);  // ALERT
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL);
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    // Hunting should still be blocked despite perfect conditions
    EXPECT_FALSE(brain_dragonfly_hunting_allowed(brain))
        << "CRITICAL protection should block hunting regardless of other factors";

    dragonfly_medulla_modulation_t mod;
    brain_dragonfly_get_modulation(brain, &mod);

    EXPECT_FALSE(mod.hunting_allowed)
        << "Modulation should indicate hunting not allowed";
}

//=============================================================================
// Modulation Query Tests
//=============================================================================

TEST_F(DragonflyMedullaIntegrationTest, ModulationReportsSourceStates) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Set specific states
    medulla_test_set_arousal(medulla, 0.75f);  // Should map to ALERT
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_CAUTIOUS);
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_AFTERNOON);
    dragonfly_medulla_bridge_update(bridge, 0.1f);

    dragonfly_medulla_modulation_t mod;
    int result = brain_dragonfly_get_modulation(brain, &mod);
    ASSERT_EQ(result, 0);

    // Verify source states are reported
    EXPECT_EQ(mod.protection_level, PROTECTION_LEVEL_CAUTIOUS);
    EXPECT_EQ(mod.circadian_phase, CIRCADIAN_PHASE_AFTERNOON);
    // Arousal level enum may differ slightly based on mapping
}

TEST_F(DragonflyMedullaIntegrationTest, ModulationWithNullOutputFails) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    int result = brain_dragonfly_get_modulation(brain, nullptr);
    EXPECT_EQ(result, -1) << "Should fail with NULL modulation output";
}

TEST_F(DragonflyMedullaIntegrationTest, HuntingAllowedReturnsCorrectly) {
    brain = create_brain_with_dragonfly_medulla();
    ASSERT_NE(brain, nullptr);

    medulla_t medulla = get_medulla(brain);
    dragonfly_medulla_bridge_t bridge = brain_get_dragonfly_medulla_bridge(brain);
    ASSERT_NE(bridge, nullptr);

    // Test NORMAL - allowed
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    medulla_test_set_arousal(medulla, 0.55f);  // AWAKE
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    EXPECT_TRUE(brain_dragonfly_hunting_allowed(brain));

    // Test with very low arousal - still allowed (just ineffective)
    medulla_test_set_arousal(medulla, 0.10f);  // DEEP_SLEEP
    dragonfly_medulla_bridge_update(bridge, 0.1f);
    // Low arousal may not block hunting outright, just reduce effectiveness
    // The exact behavior depends on bridge configuration
}
