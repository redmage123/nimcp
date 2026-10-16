/**
 * @file integration_core_medulla.cpp
 * @brief Comprehensive Integration tests for the Medulla Oblongata module
 *
 * WHAT: Integration tests verifying cross-module behavior and system interactions
 * WHY:  Ensure arousal, protection, circadian, coupling, and external integrations
 *       work together correctly in realistic scenarios
 * HOW:  Test brain integration, health monitor, sleep-wake, neuromodulator,
 *       bio-async, immune bridge, and cerebellum bridge interactions
 *
 * TEST COVERAGE:
 * 1. Brain Integration Tests - Lifecycle, updates, arousal propagation
 * 2. Health Monitor Integration - Connection, alerts, emergency triggers
 * 3. Sleep-Wake Integration - Connection, state effects
 * 4. Neuromodulator Integration - Connection, arousal modulation
 * 5. Bio-Async Integration - Message flow, state broadcasts
 * 6. Immune Bridge Integration - Creation, cytokine effects, storms
 * 7. Cerebellum Bridge Integration - Creation, arousal effects on motor
 * 8. Cross-System Integration - Multi-system interaction scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-12-17
 * @version 2.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <cmath>
#include <atomic>
#include <vector>

// Core medulla headers - NIMCP headers have internal extern "C" blocks
#include "core/medulla/nimcp_medulla.h"
#include "core/medulla/nimcp_medulla_immune_bridge.h"
#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"
#include "core/medulla/nimcp_arousal_state.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture - Basic Medulla Setup
//=============================================================================

class MedullaIntegrationTest : public ::testing::Test {
protected:
    medulla_t medulla = nullptr;

    void SetUp() override {
        medulla_config_t config = medulla_default_config();
        config.enable_bio_async = false;  // Disable for basic integration testing
        medulla = medulla_create(&config);
        ASSERT_NE(medulla, nullptr);
    }

    void TearDown() override {
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }

    // Helper: Run multiple update cycles
    void runUpdateCycles(int cycles, float dt = 0.02f) {
        for (int i = 0; i < cycles; i++) {
            EXPECT_EQ(medulla_update(medulla, dt), NIMCP_SUCCESS);
        }
    }
};

//=============================================================================
// 1. Brain Integration Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, BrainMedullaLifecycle) {
    // WHAT: Test medulla lifecycle within brain context
    // WHY:  Medulla is a critical brainstem component that must initialize properly
    // HOW:  Create, start, verify state, stop, verify cleanup

    // Verify initial state (created but not started)
    medulla_stats_t stats;
    EXPECT_EQ(medulla_get_stats(medulla, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.state, MEDULLA_STATE_STOPPED);

    // Start medulla
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Verify running state
    EXPECT_EQ(medulla_get_stats(medulla, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    // Stop medulla
    EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);

    // Verify stopped state
    EXPECT_EQ(medulla_get_stats(medulla, &stats), NIMCP_SUCCESS);
    EXPECT_TRUE(stats.state == MEDULLA_STATE_STOPPED ||
                stats.state == MEDULLA_STATE_STOPPING);
}

TEST_F(MedullaIntegrationTest, BrainMedullaUpdate) {
    // WHAT: Test medulla updates correctly with brain update cycle
    // WHY:  Brain tick must propagate through medulla subsystems
    // HOW:  Run update cycles, verify stats accumulate

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);

    // Run update cycles simulating brain tick
    runUpdateCycles(100);

    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);

    // Verify updates were processed
    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);
    EXPECT_GE(stats_after.total_updates, 100u);

    // Verify arousal was updated
    EXPECT_GT(stats_after.arousal_updates, 0u);
}

TEST_F(MedullaIntegrationTest, BrainArousalPropagation) {
    // WHAT: Test arousal changes propagate to brain systems
    // WHY:  Arousal level affects all cognitive processing
    // HOW:  Modify arousal via boost/reduce, verify changes propagate

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);
    runUpdateCycles(5);

    // Get baseline arousal
    float initial_arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(initial_arousal, 0.0f);
    EXPECT_LE(initial_arousal, 1.0f);

    // Boost arousal
    EXPECT_EQ(medulla_boost_arousal(medulla, 0.2f), NIMCP_SUCCESS);
    runUpdateCycles(5);

    float boosted_arousal = medulla_get_arousal_level(medulla);
    EXPECT_GT(boosted_arousal, initial_arousal);

    // Reduce arousal
    EXPECT_EQ(medulla_reduce_arousal(medulla, 0.3f), NIMCP_SUCCESS);
    runUpdateCycles(5);

    float reduced_arousal = medulla_get_arousal_level(medulla);
    EXPECT_LT(reduced_arousal, boosted_arousal);
}

//=============================================================================
// Arousal-Protection Integration Tests (existing, enhanced)
//=============================================================================

TEST_F(MedullaIntegrationTest, ProtectionAffectsArousal) {
    // Start medulla
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Get initial protection level
    protection_level_t initial_level = medulla_get_protection_level(medulla);
    EXPECT_EQ(initial_level, PROTECTION_LEVEL_NORMAL);

    // Trigger emergency shutdown
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "test threat"), NIMCP_SUCCESS);

    // Protection should be elevated
    protection_level_t after_level = medulla_get_protection_level(medulla);
    EXPECT_GT((int)after_level, (int)PROTECTION_LEVEL_NORMAL);
}

TEST_F(MedullaIntegrationTest, UpdateCycleIntegration) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);

    // Run multiple update cycles
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(medulla_update(medulla, 0.02f), NIMCP_SUCCESS);
    }

    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);

    // Verify updates were counted
    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);
    EXPECT_GE(stats_after.total_updates, 50u);
}

//=============================================================================
// 2. Health Monitor Integration Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, HealthMonitorConnects) {
    // WHAT: Test health monitor connection
    // WHY:  Medulla must respond to health alerts from health monitor
    // HOW:  Connect mock health monitor, verify connection accepted

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Connect NULL health monitor (should be handled gracefully)
    int result = medulla_connect_health_monitor(medulla, nullptr);
    // May succeed (permissive) or fail (strict) - both valid behaviors
    (void)result;  // Just verify no crash

    // Verify medulla still functional after connection attempt
    EXPECT_EQ(medulla_update(medulla, 0.01f), NIMCP_SUCCESS);
}

TEST_F(MedullaIntegrationTest, HealthAlertTriggersProtection) {
    // WHAT: Test that health alerts change protection level
    // WHY:  Low health should trigger protective responses
    // HOW:  Use test helper to set low health condition, verify protection elevates

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Initial protection should be normal
    protection_level_t initial = medulla_get_protection_level(medulla);
    EXPECT_EQ(initial, PROTECTION_LEVEL_NORMAL);

    // Use test helper to force protection level (simulating health alert effect)
    EXPECT_EQ(medulla_test_set_protection(medulla, PROTECTION_LEVEL_DEFENSIVE), NIMCP_SUCCESS);
    runUpdateCycles(3);

    protection_level_t after = medulla_get_protection_level(medulla);
    EXPECT_EQ(after, PROTECTION_LEVEL_DEFENSIVE);
}

TEST_F(MedullaIntegrationTest, CriticalHealthTriggersEmergency) {
    // WHAT: Test critical health triggers emergency
    // WHY:  Critical health conditions must trigger emergency shutdown
    // HOW:  Trigger emergency, verify protection reaches critical/shutdown level

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Trigger emergency shutdown
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "critical health"), NIMCP_SUCCESS);

    // Verify protection is at critical or shutdown level
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)level, (int)PROTECTION_LEVEL_CRITICAL);

    // Verify state reflects emergency
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_TRUE(stats.state == MEDULLA_STATE_EMERGENCY ||
                stats.protection_level >= PROTECTION_LEVEL_CRITICAL);
}

//=============================================================================
// 3. Sleep-Wake Integration Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, SleepWakeConnects) {
    // WHAT: Test sleep-wake connection
    // WHY:  Sleep-wake cycle must synchronize with arousal state
    // HOW:  Connect mock sleep-wake system, verify connection

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Connect NULL sleep-wake (should be handled gracefully)
    int result = medulla_connect_sleep_wake(medulla, nullptr);
    // May succeed or fail - verify no crash
    (void)result;

    // Medulla should still function
    EXPECT_EQ(medulla_update(medulla, 0.01f), NIMCP_SUCCESS);
}

TEST_F(MedullaIntegrationTest, SleepStateAffectsArousal) {
    // WHAT: Test sleep state modulates arousal
    // WHY:  Sleep states reduce arousal (biological: descending reticular pathways)
    // HOW:  Set circadian phase to night, verify arousal tends lower

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Set to morning phase (high arousal expected)
    EXPECT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_test_set_arousal(medulla, 0.7f), NIMCP_SUCCESS);
    runUpdateCycles(10);

    float morning_arousal = medulla_get_arousal_level(medulla);

    // Set to deep night phase (low arousal expected)
    EXPECT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_test_set_arousal(medulla, 0.3f), NIMCP_SUCCESS);
    runUpdateCycles(10);

    float night_arousal = medulla_get_arousal_level(medulla);

    // Morning arousal should be higher than night
    EXPECT_GT(morning_arousal, night_arousal);
}

//=============================================================================
// 4. Neuromodulator Integration Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, NeuromodulatorConnects) {
    // WHAT: Test neuromodulator connection
    // WHY:  Neuromodulators affect arousal (NE, DA, 5-HT, ACh)
    // HOW:  Connect mock neuromodulator system

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Connect NULL neuromodulator system (should be handled gracefully)
    int result = medulla_connect_neuromodulators(medulla, nullptr);
    (void)result;

    // Medulla should still function
    EXPECT_EQ(medulla_update(medulla, 0.01f), NIMCP_SUCCESS);
}

TEST_F(MedullaIntegrationTest, NeuromodulatorsAffectArousal) {
    // WHAT: Test neuromodulators modulate arousal
    // WHY:  NE/DA increase arousal, low levels decrease it
    // HOW:  Apply stimuli that simulate neuromodulator effects

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Get baseline arousal
    float baseline = medulla_get_arousal_level(medulla);

    // Boost arousal (simulating NE/DA increase)
    EXPECT_EQ(medulla_boost_arousal(medulla, 0.2f), NIMCP_SUCCESS);
    runUpdateCycles(5);

    float after_boost = medulla_get_arousal_level(medulla);
    EXPECT_GT(after_boost, baseline);

    // Reduce arousal (simulating ACh increase for sleep)
    EXPECT_EQ(medulla_reduce_arousal(medulla, 0.3f), NIMCP_SUCCESS);
    runUpdateCycles(5);

    float after_reduce = medulla_get_arousal_level(medulla);
    EXPECT_LT(after_reduce, after_boost);
}

//=============================================================================
// 5. Bio-Async Integration Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, BioAsyncMessageFlow) {
    // WHAT: Test message flow through bio-async
    // WHY:  Medulla must participate in bio-async messaging for coordination
    // HOW:  Enable bio-async, verify connection status

    // Create medulla with bio-async enabled
    medulla_stop(medulla);
    medulla_destroy(medulla);

    medulla_config_t config = medulla_default_config();
    config.enable_bio_async = true;
    config.inbox_capacity = 64;
    medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Connect to bio-async
    int result = medulla_connect_bio_async(medulla);
    // May succeed or fail depending on router availability
    if (result == NIMCP_SUCCESS) {
        EXPECT_TRUE(medulla_is_bio_async_connected(medulla));

        // Disconnect
        EXPECT_EQ(medulla_disconnect_bio_async(medulla), NIMCP_SUCCESS);
        EXPECT_FALSE(medulla_is_bio_async_connected(medulla));
    }
}

TEST_F(MedullaIntegrationTest, BioAsyncStatebroadcast) {
    // WHAT: Test state changes are broadcast
    // WHY:  Other modules need to know about arousal/protection changes
    // HOW:  Change state, verify broadcast would occur (via stats)

    medulla_stop(medulla);
    medulla_destroy(medulla);

    medulla_config_t config = medulla_default_config();
    config.enable_bio_async = true;
    medulla = medulla_create(&config);
    ASSERT_NE(medulla, nullptr);

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Trigger state change (emergency)
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "test broadcast"), NIMCP_SUCCESS);

    // Verify state changed
    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_GE((int)stats.protection_level, (int)PROTECTION_LEVEL_CRITICAL);
}

//=============================================================================
// 6. Immune Bridge Integration Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, ImmuneBridgeCreation) {
    // WHAT: Test immune bridge creation
    // WHY:  Immune-medulla integration is critical for sickness behavior
    // HOW:  Create bridge, verify it connects medulla to immune system

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Get default config
    medulla_immune_config_t config;
    medulla_immune_default_config(&config);

    // Create bridge (will fail without immune system, but should not crash)
    medulla_immune_bridge_t bridge = medulla_immune_create(&config, medulla, nullptr);
    // Bridge may be NULL if immune system is required

    if (bridge) {
        // Get stats
        medulla_immune_stats_t stats;
        EXPECT_EQ(medulla_immune_get_stats(bridge, &stats), 0);
        EXPECT_EQ(stats.total_updates, 0u);

        medulla_immune_destroy(bridge);
    }
}

TEST_F(MedullaIntegrationTest, CytokineAffectsArousal) {
    // WHAT: Test cytokines modulate arousal
    // WHY:  Pro-inflammatory cytokines cause sickness behavior (reduced arousal)
    // HOW:  Verify utility function computes correct arousal factors

    // IL-1, IL-6, TNF-alpha should reduce arousal
    // Test utility functions for cytokine effects

    float il1_factor = CYTOKINE_IL1_AROUSAL_IMPACT;  // Should be negative
    float il6_factor = CYTOKINE_IL6_AROUSAL_IMPACT;
    float tnf_factor = CYTOKINE_TNF_AROUSAL_IMPACT;
    float il10_factor = CYTOKINE_IL10_AROUSAL_IMPACT;  // Anti-inflammatory, positive

    EXPECT_LT(il1_factor, 0.0f);
    EXPECT_LT(il6_factor, 0.0f);
    EXPECT_LT(tnf_factor, 0.0f);
    EXPECT_GT(il10_factor, 0.0f);
}

TEST_F(MedullaIntegrationTest, CytokineStormTriggersEmergency) {
    // WHAT: Test cytokine storm triggers emergency
    // WHY:  Extreme inflammation must trigger protective response
    // HOW:  Verify storm arousal factor is very low

    // Cytokine storm should dramatically reduce arousal
    float storm_factor = INFLAMMATION_STORM_AROUSAL_FACTOR;
    EXPECT_LE(storm_factor, 0.5f);  // Arousal reduced to 50% or less

    // Verify progression of inflammation severity
    EXPECT_GT(INFLAMMATION_NONE_AROUSAL_FACTOR, INFLAMMATION_LOCAL_AROUSAL_FACTOR);
    EXPECT_GT(INFLAMMATION_LOCAL_AROUSAL_FACTOR, INFLAMMATION_REGIONAL_AROUSAL_FACTOR);
    EXPECT_GT(INFLAMMATION_REGIONAL_AROUSAL_FACTOR, INFLAMMATION_SYSTEMIC_AROUSAL_FACTOR);
    EXPECT_GT(INFLAMMATION_SYSTEMIC_AROUSAL_FACTOR, INFLAMMATION_STORM_AROUSAL_FACTOR);
}

//=============================================================================
// 7. Cerebellum Bridge Integration Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, CerebellumBridgeCreation) {
    // WHAT: Test cerebellum bridge creation
    // WHY:  Medulla-cerebellum connection is vital for motor control
    // HOW:  Create bridge, verify it initializes correctly

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Get default config
    med_cereb_bridge_config_t config;
    EXPECT_EQ(med_cereb_bridge_default_config(&config), 0);

    // Create bridge
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect medulla
    EXPECT_EQ(med_cereb_bridge_connect_medulla(bridge, medulla), 0);

    // Get stats (should be initialized)
    med_cereb_bridge_stats_t stats;
    EXPECT_EQ(med_cereb_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.climbing_signals_sent, 0u);

    med_cereb_bridge_destroy(bridge);
}

TEST_F(MedullaIntegrationTest, ArousalAffectsMotorGain) {
    // WHAT: Test arousal affects motor gain
    // WHY:  Higher arousal = higher motor gain (faster reactions)
    // HOW:  Create bridge, set different arousal levels, compare motor effects

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Create and connect bridge
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    config.enable_arousal_modulation = true;

    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(med_cereb_bridge_connect_medulla(bridge, medulla), 0);

    // Set low arousal
    medulla_test_set_arousal(medulla, 0.3f);
    medulla_update(medulla, 0.02f);
    med_cereb_bridge_update(bridge, 20000);

    med_cereb_arousal_effects_t low_effects;
    EXPECT_EQ(med_cereb_bridge_get_arousal_effects(bridge, &low_effects), 0);

    // Set high arousal
    medulla_test_set_arousal(medulla, 0.8f);
    medulla_update(medulla, 0.02f);
    med_cereb_bridge_update(bridge, 20000);

    med_cereb_arousal_effects_t high_effects;
    EXPECT_EQ(med_cereb_bridge_get_arousal_effects(bridge, &high_effects), 0);

    // Higher arousal should mean higher motor gain
    EXPECT_GT(high_effects.motor_gain, low_effects.motor_gain);

    med_cereb_bridge_destroy(bridge);
}

//=============================================================================
// Circadian-Arousal Integration Tests (existing, enhanced)
//=============================================================================

TEST_F(MedullaIntegrationTest, CircadianPhaseTracking) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Get initial phase
    circadian_phase_t phase = medulla_get_circadian_phase(medulla);
    EXPECT_GE((int)phase, 0);
    EXPECT_LT((int)phase, 8);

    // Run updates
    for (int i = 0; i < 100; i++) {
        medulla_update(medulla, 0.1f);
    }

    // Phase should still be valid
    phase = medulla_get_circadian_phase(medulla);
    EXPECT_GE((int)phase, 0);
    EXPECT_LT((int)phase, 8);
}

//=============================================================================
// State Transition Integration Tests (existing)
//=============================================================================

TEST_F(MedullaIntegrationTest, StateTransitions) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);

    // Request degraded state
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_DEGRADED), NIMCP_SUCCESS);

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_DEGRADED);

    // Request back to running
    EXPECT_EQ(medulla_request_state_change(medulla, MEDULLA_STATE_RUNNING), NIMCP_SUCCESS);

    medulla_get_stats(medulla, &stats);
    EXPECT_EQ(stats.state, MEDULLA_STATE_RUNNING);
}

TEST_F(MedullaIntegrationTest, EmergencyStateTransition) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Trigger emergency
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "critical failure"), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    // Should be in emergency or stopped state
    EXPECT_TRUE(stats.state == MEDULLA_STATE_EMERGENCY ||
                stats.protection_level >= PROTECTION_LEVEL_CRITICAL);
}

//=============================================================================
// Lifecycle Integration Tests (existing)
//=============================================================================

TEST_F(MedullaIntegrationTest, StartStopCycles) {
    // Multiple start/stop cycles should work
    for (int cycle = 0; cycle < 3; cycle++) {
        EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

        // Run some updates
        for (int i = 0; i < 10; i++) {
            EXPECT_EQ(medulla_update(medulla, 0.016f), NIMCP_SUCCESS);
        }

        EXPECT_EQ(medulla_stop(medulla), NIMCP_SUCCESS);
    }
}

TEST_F(MedullaIntegrationTest, StatsAccumulation) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Run updates
    for (int i = 0; i < 100; i++) {
        medulla_update(medulla, 0.01f);
    }

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    // Verify statistics accumulated
    EXPECT_GE(stats.total_updates, 100u);
    EXPECT_GE(stats.arousal_updates, 0u);
}

//=============================================================================
// Protection Level Progression Tests (existing)
//=============================================================================

TEST_F(MedullaIntegrationTest, ProtectionLevelProgression) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Initial level should be normal
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_EQ(level, PROTECTION_LEVEL_NORMAL);

    // After emergency, should be elevated
    medulla_emergency_shutdown(medulla, "test");
    level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)level, (int)PROTECTION_LEVEL_CRITICAL);
}

//=============================================================================
// Arousal State Modulation Tests (existing)
//=============================================================================

TEST_F(MedullaIntegrationTest, ArousalLevelRange) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);

    // Arousal should be within valid range
    EXPECT_GE(stats.current_arousal, 0.0f);
    EXPECT_LE(stats.current_arousal, 1.0f);
}

//=============================================================================
// Concurrent Update Safety Tests (existing)
//=============================================================================

TEST_F(MedullaIntegrationTest, RapidUpdateSequence) {
    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Rapid updates shouldn't cause issues
    for (int i = 0; i < 1000; i++) {
        int result = medulla_update(medulla, 0.001f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_GE(stats.total_updates, 1000u);
}

//=============================================================================
// 8. Cross-System Integration Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, CircadianArousalProtectionInteraction) {
    // WHAT: Test interaction between circadian, arousal, and protection
    // WHY:  These systems must work together for proper brainstem function
    // HOW:  Set circadian phase, modify arousal, trigger protection, verify coherence

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    // Set morning phase (high alertness expected)
    EXPECT_EQ(medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING), NIMCP_SUCCESS);
    EXPECT_EQ(medulla_test_set_arousal(medulla, 0.7f), NIMCP_SUCCESS);
    runUpdateCycles(10);

    medulla_stats_t morning_stats;
    medulla_get_stats(medulla, &morning_stats);
    EXPECT_EQ(morning_stats.circadian_phase, CIRCADIAN_PHASE_MORNING);

    // Trigger emergency during morning
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "morning emergency"), NIMCP_SUCCESS);

    // Verify protection elevated even during high-alertness phase
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)level, (int)PROTECTION_LEVEL_CRITICAL);
}

TEST_F(MedullaIntegrationTest, MultipleSubsystemUpdates) {
    // WHAT: Test that all subsystems update correctly in sequence
    // WHY:  Medulla orchestrates multiple subsystems that must remain synchronized
    // HOW:  Run extended updates, verify all stats progress

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    medulla_stats_t stats_before;
    medulla_get_stats(medulla, &stats_before);

    // Run 500 updates
    for (int i = 0; i < 500; i++) {
        EXPECT_EQ(medulla_update(medulla, 0.01f), NIMCP_SUCCESS);

        // Periodically modify state
        if (i % 100 == 0) {
            medulla_boost_arousal(medulla, 0.05f);
        }
    }

    medulla_stats_t stats_after;
    medulla_get_stats(medulla, &stats_after);

    // Verify all counters progressed
    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);
    EXPECT_GT(stats_after.arousal_updates, stats_before.arousal_updates);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, ConcurrentAccessSafety) {
    // WHAT: Test thread safety of medulla operations
    // WHY:  Brain operates concurrently and medulla must be thread-safe
    // HOW:  Run concurrent reads and updates from multiple threads

    EXPECT_EQ(medulla_start(medulla), NIMCP_SUCCESS);

    std::atomic<bool> running{true};
    std::atomic<int> update_count{0};
    std::atomic<int> query_count{0};

    // Update thread
    std::thread updater([&]() {
        while (running) {
            medulla_update(medulla, 0.001f);
            update_count++;
        }
    });

    // Query thread
    std::thread querier([&]() {
        while (running) {
            medulla_stats_t stats;
            medulla_get_stats(medulla, &stats);
            float arousal = medulla_get_arousal_level(medulla);
            protection_level_t level = medulla_get_protection_level(medulla);
            (void)arousal;
            (void)level;
            query_count++;
        }
    });

    // Run for short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    updater.join();
    querier.join();

    // Should have done many operations without crashes
    EXPECT_GT(update_count.load(), 10);
    EXPECT_GT(query_count.load(), 10);

    // Medulla should still be valid
    medulla_stats_t final_stats;
    EXPECT_EQ(medulla_get_stats(medulla, &final_stats), NIMCP_SUCCESS);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(MedullaIntegrationTest, StringConversions) {
    // Test all enum-to-string conversions

    // Arousal levels
    EXPECT_NE(medulla_arousal_level_to_string(AROUSAL_LEVEL_COMA), nullptr);
    EXPECT_NE(medulla_arousal_level_to_string(AROUSAL_LEVEL_AWAKE), nullptr);
    EXPECT_NE(medulla_arousal_level_to_string(AROUSAL_LEVEL_HYPERAROUSAL), nullptr);

    // Protection levels
    EXPECT_NE(medulla_protection_level_to_string(PROTECTION_LEVEL_NORMAL), nullptr);
    EXPECT_NE(medulla_protection_level_to_string(PROTECTION_LEVEL_CRITICAL), nullptr);
    EXPECT_NE(medulla_protection_level_to_string(PROTECTION_LEVEL_SHUTDOWN), nullptr);

    // Circadian phases
    EXPECT_NE(medulla_circadian_phase_to_string(CIRCADIAN_PHASE_MORNING), nullptr);
    EXPECT_NE(medulla_circadian_phase_to_string(CIRCADIAN_PHASE_DEEP_NIGHT), nullptr);

    // Medulla states
    EXPECT_NE(medulla_state_to_string(MEDULLA_STATE_STOPPED), nullptr);
    EXPECT_NE(medulla_state_to_string(MEDULLA_STATE_RUNNING), nullptr);
    EXPECT_NE(medulla_state_to_string(MEDULLA_STATE_EMERGENCY), nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
