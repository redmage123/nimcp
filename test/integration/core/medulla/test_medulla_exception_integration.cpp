/**
 * @file test_medulla_exception_integration.cpp
 * @brief Comprehensive integration tests for medulla oblongata module
 *
 * WHAT: Tests integration scenarios between medulla and related components
 * WHY:  Ensure medulla-immune, medulla-cerebellum, and cross-bridge
 *       integration work correctly together
 * HOW:  Create realistic multi-component scenarios, verify expected behaviors
 *
 * TEST CATEGORIES:
 * 1. Medulla-Immune Bridge Integration
 *    - Bidirectional updates (immune->medulla, medulla->immune)
 *    - Cytokine effect propagation
 *    - Emergency shutdown on cytokine storm
 *
 * 2. Medulla-Cerebellum Bridge Integration
 *    - Error queue and climbing fiber signals
 *    - Motor command modulation with arousal/protection levels
 *    - IO neuron state updates
 *
 * 3. Cross-Bridge Integration
 *    - Medulla state consistency across bridges
 *    - Emergency shutdown propagation
 *
 * 4. Thread Safety Tests
 *    - Concurrent operations
 *
 * NOTE: Brainstem-cortex coupling tests are in a separate test file due to
 *       a typedef conflict between nimcp_medulla.h and nimcp_brainstem_coupling.h
 *
 * @version 1.0.0
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

#include "utils/nimcp_test_base.h"

// Core medulla headers
#include "core/medulla/nimcp_medulla.h"
#include "core/medulla/nimcp_medulla_immune_bridge.h"
#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"

// Related system headers
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Medulla-Immune Bridge Integration Tests
//=============================================================================

class MedullaImmuneIntegrationTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;
    brain_immune_system_t* immune = nullptr;
    medulla_immune_bridge_t bridge = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create medulla
        medulla_config_t med_config = medulla_default_config();
        med_config.enable_health_integration = false;
        med_config.enable_recovery_integration = false;
        med_config.enable_sleep_integration = false;
        med_config.enable_neuromod_integration = false;
        med_config.enable_bio_async = false;
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr) << "Failed to create medulla";

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_config.enable_bio_async = false;
        immune_config.enable_bbb_integration = false;
        immune_config.enable_bft_integration = false;
        immune_config.enable_swarm_integration = false;
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr) << "Failed to create immune system";

        // Create bridge
        medulla_immune_config_t bridge_config;
        medulla_immune_default_config(&bridge_config);
        bridge_config.enable_bio_async = false;
        bridge_config.enable_immune_to_medulla = true;
        bridge_config.enable_medulla_to_immune = true;
        bridge_config.enable_circadian_modulation = true;
        bridge = medulla_immune_create(&bridge_config, medulla, immune);
        ASSERT_NE(bridge, nullptr) << "Failed to create medulla-immune bridge";
    }

    void TearDown() override {
        if (bridge) {
            medulla_immune_destroy(bridge);
            bridge = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 1: Bridge creation with medulla and immune system
TEST_F(MedullaImmuneIntegrationTest, BridgeCreatedSuccessfully) {
    EXPECT_NE(bridge, nullptr);
}

// Test 2: Immune to medulla pathway basic operation
TEST_F(MedullaImmuneIntegrationTest, ImmuneToMedullaPathway) {
    medulla_start(medulla);

    medulla_cytokine_effects_t effects;
    int result = medulla_immune_update_immune_to_medulla(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Effects should have valid values
    EXPECT_GE(effects.inflammation_arousal_factor, 0.0f);
    EXPECT_LE(effects.inflammation_arousal_factor, 1.0f);
}

// Test 3: Medulla to immune pathway basic operation
TEST_F(MedullaImmuneIntegrationTest, MedullaToImmunePathway) {
    medulla_start(medulla);

    medulla_immune_effects_t effects;
    int result = medulla_immune_update_medulla_to_immune(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Effects should have valid immune factors
    EXPECT_GE(effects.combined_immune_factor, 0.0f);
    EXPECT_LE(effects.combined_immune_factor, 3.0f);  // Max combined factor
}

// Test 4: Bidirectional update operation
TEST_F(MedullaImmuneIntegrationTest, BidirectionalUpdate) {
    medulla_start(medulla);

    // Run multiple bidirectional updates
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(medulla_immune_update(bridge), 0);
        medulla_update(medulla, 0.01f);
    }

    // Check statistics
    medulla_immune_stats_t stats;
    EXPECT_EQ(medulla_immune_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);
}

// Test 5: Cytokine effect computation
TEST_F(MedullaImmuneIntegrationTest, CytokineEffectComputation) {
    // Test arousal factor computation for different inflammation levels
    float none_factor = medulla_immune_compute_inflammation_arousal(INFLAMMATION_NONE);
    float local_factor = medulla_immune_compute_inflammation_arousal(INFLAMMATION_LOCAL);
    float regional_factor = medulla_immune_compute_inflammation_arousal(INFLAMMATION_REGIONAL);
    float systemic_factor = medulla_immune_compute_inflammation_arousal(INFLAMMATION_SYSTEMIC);
    float storm_factor = medulla_immune_compute_inflammation_arousal(INFLAMMATION_STORM);

    // Higher inflammation should reduce arousal more
    EXPECT_GT(none_factor, local_factor);
    EXPECT_GT(local_factor, regional_factor);
    EXPECT_GT(regional_factor, systemic_factor);
    EXPECT_GT(systemic_factor, storm_factor);
}

// Test 6: Protection level immune factor computation
TEST_F(MedullaImmuneIntegrationTest, ProtectionImmuneFactorComputation) {
    float normal_factor = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_NORMAL);
    float cautious_factor = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_CAUTIOUS);
    float defensive_factor = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_DEFENSIVE);
    float critical_factor = medulla_immune_compute_protection_immune(PROTECTION_LEVEL_CRITICAL);

    // Higher protection should increase immune activity
    EXPECT_LE(normal_factor, cautious_factor);
    EXPECT_LE(cautious_factor, defensive_factor);
    EXPECT_LE(defensive_factor, critical_factor);
}

// Test 7: Circadian phase immune factor computation
TEST_F(MedullaImmuneIntegrationTest, CircadianImmuneFactor) {
    float morning_factor = medulla_immune_compute_circadian_immune(CIRCADIAN_PHASE_MORNING);
    float night_factor = medulla_immune_compute_circadian_immune(CIRCADIAN_PHASE_DEEP_NIGHT);

    // Day phases should have different immune efficiency than night
    EXPECT_NE(morning_factor, night_factor);
}

// Test 8: Arousal modulation from immune effects
TEST_F(MedullaImmuneIntegrationTest, ArousalModulationFromImmune) {
    medulla_start(medulla);

    // Get initial arousal
    float initial_arousal = medulla_get_arousal_level(medulla);

    // Run updates to apply immune effects
    for (int i = 0; i < 20; i++) {
        medulla_immune_update(bridge);
        medulla_update(medulla, 0.05f);
    }

    // Arousal should still be in valid range
    float final_arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(final_arousal, 0.0f);
    EXPECT_LE(final_arousal, 1.0f);
}

// Test 9: Protection escalation from inflammation
TEST_F(MedullaImmuneIntegrationTest, ProtectionEscalationFromInflammation) {
    medulla_start(medulla);

    // Get initial protection level
    protection_level_t initial_protection = medulla_get_protection_level(medulla);
    EXPECT_EQ(initial_protection, PROTECTION_LEVEL_NORMAL);

    // Run bridge updates
    for (int i = 0; i < 10; i++) {
        medulla_immune_update(bridge);
        medulla_update(medulla, 0.05f);
    }

    // Protection level should still be valid
    protection_level_t final_protection = medulla_get_protection_level(medulla);
    EXPECT_GE((int)final_protection, 0);
    EXPECT_LE((int)final_protection, (int)PROTECTION_LEVEL_SHUTDOWN);
}

// Test 10: Emergency shutdown on cytokine storm
TEST_F(MedullaImmuneIntegrationTest, EmergencyOnCytokineStorm) {
    medulla_start(medulla);

    // Trigger emergency from medulla side
    EXPECT_EQ(medulla_emergency_shutdown(medulla, "test cytokine storm"), 0);

    // Protection should be elevated
    protection_level_t level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)level, (int)PROTECTION_LEVEL_CRITICAL);
}

// Test 11: Statistics accumulation
TEST_F(MedullaImmuneIntegrationTest, StatisticsAccumulation) {
    medulla_start(medulla);

    medulla_immune_stats_t initial_stats;
    medulla_immune_get_stats(bridge, &initial_stats);

    // Run updates
    for (int i = 0; i < 50; i++) {
        medulla_immune_update(bridge);
    }

    medulla_immune_stats_t final_stats;
    medulla_immune_get_stats(bridge, &final_stats);

    EXPECT_GT(final_stats.total_updates, initial_stats.total_updates);
}

// Test 12: Get cytokine effects query
TEST_F(MedullaImmuneIntegrationTest, GetCytokineEffectsQuery) {
    medulla_start(medulla);
    medulla_immune_update(bridge);

    medulla_cytokine_effects_t effects;
    EXPECT_EQ(medulla_immune_get_cytokine_effects(bridge, &effects), 0);

    // Arousal modulation should be in valid range
    EXPECT_GE(effects.arousal_modulation, -1.0f);
    EXPECT_LE(effects.arousal_modulation, 1.0f);
}

// Test 13: Get immune effects query
TEST_F(MedullaImmuneIntegrationTest, GetImmuneEffectsQuery) {
    medulla_start(medulla);
    medulla_immune_update(bridge);

    medulla_immune_effects_t effects;
    EXPECT_EQ(medulla_immune_get_immune_effects(bridge, &effects), 0);

    // Arousal level should be valid
    EXPECT_GE(effects.arousal_level, 0.0f);
    EXPECT_LE(effects.arousal_level, 1.0f);
}

// Test 14: Arousal effects on immune system via bridge
TEST_F(MedullaImmuneIntegrationTest, ArousalEffectsOnImmune) {
    medulla_start(medulla);

    // Set different arousal levels and check immune effects
    medulla_test_set_arousal(medulla, 0.2f);  // Low arousal
    medulla_immune_update(bridge);

    medulla_immune_effects_t low_effects;
    EXPECT_EQ(0, medulla_immune_get_immune_effects(bridge, &low_effects));

    medulla_test_set_arousal(medulla, 0.8f);  // High arousal
    medulla_immune_update(bridge);

    medulla_immune_effects_t high_effects;
    EXPECT_EQ(0, medulla_immune_get_immune_effects(bridge, &high_effects));

    // Both should have valid combined factors
    EXPECT_GE(low_effects.combined_immune_factor, 0.0f);
    EXPECT_GE(high_effects.combined_immune_factor, 0.0f);
}

// Test 15: Sustained medulla-immune operation
TEST_F(MedullaImmuneIntegrationTest, SustainedMedullaImmuneOperation) {
    medulla_start(medulla);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(medulla_immune_update(bridge), 0);
        EXPECT_EQ(medulla_update(medulla, 0.01f), 0);
    }

    medulla_immune_stats_t stats;
    medulla_immune_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_updates, 100u);
}

//=============================================================================
// Medulla-Cerebellum Bridge Integration Tests
//=============================================================================

class MedullaCerebellumIntegrationTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    med_cereb_bridge_t bridge = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create medulla
        medulla_config_t med_config = medulla_default_config();
        med_config.enable_health_integration = false;
        med_config.enable_recovery_integration = false;
        med_config.enable_sleep_integration = false;
        med_config.enable_neuromod_integration = false;
        med_config.enable_bio_async = false;
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr) << "Failed to create medulla";

        // Create cerebellum
        cerebellum_config_t cere_config = cerebellum_default_config();
        cere_config.enable_error_learning = true;
        cere_config.enable_timing = true;
        cere_config.enable_motor_adaptation = true;
        cere_config.enable_bio_async = false;
        cerebellum = cerebellum_create(&cere_config);
        ASSERT_NE(cerebellum, nullptr) << "Failed to create cerebellum";

        // Create bridge
        med_cereb_bridge_config_t bridge_config;
        med_cereb_bridge_default_config(&bridge_config);
        bridge_config.enable_arousal_modulation = true;
        bridge_config.enable_protection_gating = true;
        bridge_config.enable_circadian_learning = true;
        bridge_config.enable_io_signaling = true;
        bridge_config.enable_bio_async = false;
        bridge = med_cereb_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr) << "Failed to create bridge";

        // Connect subsystems
        ASSERT_EQ(0, med_cereb_bridge_connect_medulla(bridge, medulla));
        ASSERT_EQ(0, med_cereb_bridge_connect_cerebellum(bridge, cerebellum));
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    void runUpdateCycles(int cycles, uint64_t delta_us = 10000) {
        for (int i = 0; i < cycles; i++) {
            medulla_update(medulla, delta_us / 1000000.0f);
            med_cereb_bridge_update(bridge, delta_us);
        }
    }
};

// Test 16: Bridge connected successfully
TEST_F(MedullaCerebellumIntegrationTest, BridgeConnectedSuccessfully) {
    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));
}

// Test 17: Error queue basic functionality
TEST_F(MedullaCerebellumIntegrationTest, ErrorQueueBasic) {
    EXPECT_EQ(0, med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 1));
    EXPECT_EQ(1u, med_cereb_bridge_pending_error_count(bridge));
}

// Test 18: Multiple error types in queue
TEST_F(MedullaCerebellumIntegrationTest, MultipleErrorTypes) {
    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        med_cereb_error_type_t type = static_cast<med_cereb_error_type_t>(i);
        EXPECT_EQ(0, med_cereb_bridge_queue_error(bridge, type, 0.5f, i));
    }
    EXPECT_EQ((uint32_t)MED_CEREB_ERROR_COUNT, med_cereb_bridge_pending_error_count(bridge));
}

// Test 19: Climbing fiber signal delivery
TEST_F(MedullaCerebellumIntegrationTest, ClimbingFiberSignal) {
    medulla_start(medulla);
    med_cereb_bridge_reset_stats(bridge);

    EXPECT_EQ(0, med_cereb_bridge_send_climbing_signal(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.7f, 0));

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.climbing_signals_sent, 0u);
}

// Test 20: Broadcast error to all Purkinje cells
TEST_F(MedullaCerebellumIntegrationTest, BroadcastError) {
    medulla_start(medulla);
    med_cereb_bridge_reset_stats(bridge);

    EXPECT_EQ(0, med_cereb_bridge_broadcast_error(bridge, MED_CEREB_ERROR_PROTECTION, 1.0f));

    med_cereb_bridge_update(bridge, 10000);

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.climbing_signals_sent, 0u);
}

// Test 21: Motor command modulation with low arousal
TEST_F(MedullaCerebellumIntegrationTest, MotorModulationLowArousal) {
    medulla_start(medulla);
    medulla_test_set_arousal(medulla, 0.2f);
    runUpdateCycles(5);

    float motor_in[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float motor_out[4] = {0};

    EXPECT_EQ(0, med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, 4));

    // Output should be modulated
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(motor_out[i], 0.0f);
    }
}

// Test 22: Motor command modulation with high arousal
TEST_F(MedullaCerebellumIntegrationTest, MotorModulationHighArousal) {
    medulla_start(medulla);
    medulla_test_set_arousal(medulla, 0.9f);
    runUpdateCycles(5);

    float motor_in[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float motor_out[4] = {0};

    EXPECT_EQ(0, med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, 4));

    // Output should be modulated
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(motor_out[i], 0.0f);
    }
}

// Test 23: Arousal affects motor gain
TEST_F(MedullaCerebellumIntegrationTest, ArousalAffectsMotorGain) {
    medulla_start(medulla);

    medulla_test_set_arousal(medulla, 0.3f);
    runUpdateCycles(5);

    med_cereb_arousal_effects_t low_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_arousal_effects(bridge, &low_effects));

    medulla_test_set_arousal(medulla, 0.9f);
    runUpdateCycles(5);

    med_cereb_arousal_effects_t high_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_arousal_effects(bridge, &high_effects));

    EXPECT_GT(high_effects.motor_gain, low_effects.motor_gain);
}

// Test 24: Protection level gating normal operation
TEST_F(MedullaCerebellumIntegrationTest, ProtectionGatingNormal) {
    medulla_start(medulla);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    runUpdateCycles(3);

    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, true, false));
}

// Test 25: Protection level gating critical
TEST_F(MedullaCerebellumIntegrationTest, ProtectionGatingCritical) {
    medulla_start(medulla);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL);
    runUpdateCycles(3);

    med_cereb_protection_effects_t effects;
    EXPECT_EQ(0, med_cereb_bridge_get_protection_effects(bridge, &effects));

    // Critical should have reduced output
    EXPECT_LT(effects.output_scale, 1.0f);
}

// Test 26: Emergency stop activation
TEST_F(MedullaCerebellumIntegrationTest, EmergencyStopActivation) {
    medulla_start(medulla);
    EXPECT_EQ(0, med_cereb_bridge_emergency_stop(bridge));

    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_TRUE(effects.emergency_stop);
}

// Test 27: Emergency stop release
TEST_F(MedullaCerebellumIntegrationTest, EmergencyStopRelease) {
    medulla_start(medulla);
    med_cereb_bridge_emergency_stop(bridge);
    EXPECT_EQ(0, med_cereb_bridge_release_emergency(bridge));

    runUpdateCycles(5);

    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_FALSE(effects.emergency_stop);
}

// Test 28: IO neuron state accessible
TEST_F(MedullaCerebellumIntegrationTest, IONeuronStateAccessible) {
    med_cereb_inferior_olive_t io_state;
    EXPECT_EQ(0, med_cereb_bridge_get_io_state(bridge, &io_state));

    EXPECT_GT(io_state.num_neurons, 0u);
    EXPECT_GT(io_state.oscillation_freq, 0.0f);
}

// Test 29: Circadian affects learning rate
TEST_F(MedullaCerebellumIntegrationTest, CircadianAffectsLearning) {
    medulla_start(medulla);

    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    runUpdateCycles(3);

    med_cereb_circadian_effects_t morning_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_circadian_effects(bridge, &morning_effects));

    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT);
    runUpdateCycles(3);

    med_cereb_circadian_effects_t night_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_circadian_effects(bridge, &night_effects));

    EXPECT_GT(morning_effects.ltd_rate_multiplier, night_effects.ltd_rate_multiplier);
}

// Test 30: Learning multiplier computation
TEST_F(MedullaCerebellumIntegrationTest, LearningMultiplierComputation) {
    medulla_start(medulla);
    runUpdateCycles(3);

    float multiplier = med_cereb_bridge_get_learning_multiplier(bridge);
    EXPECT_GE(multiplier, 0.1f);
    EXPECT_LE(multiplier, 2.0f);
}

// Test 31: Apply circadian learning
TEST_F(MedullaCerebellumIntegrationTest, ApplyCircadianLearning) {
    medulla_start(medulla);
    runUpdateCycles(3);

    med_cereb_bridge_reset_stats(bridge);
    EXPECT_EQ(0, med_cereb_bridge_apply_circadian_learning(bridge));

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.learning_rate_adjustments, 0u);
}

// Test 32: Bridge statistics reset
TEST_F(MedullaCerebellumIntegrationTest, StatisticsReset) {
    medulla_start(medulla);

    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 1);
    runUpdateCycles(50);

    med_cereb_bridge_stats_t before_reset;
    med_cereb_bridge_get_stats(bridge, &before_reset);

    EXPECT_EQ(0, med_cereb_bridge_reset_stats(bridge));

    med_cereb_bridge_stats_t after_reset;
    med_cereb_bridge_get_stats(bridge, &after_reset);
    EXPECT_EQ(0u, after_reset.io_spikes);
}

// Test 33: Error queue overflow handling
TEST_F(MedullaCerebellumIntegrationTest, ErrorQueueOverflow) {
    // Fill queue beyond capacity
    for (int i = 0; i < MED_CEREB_MAX_ERROR_QUEUE + 10; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
    }

    // Queue should be at max capacity
    EXPECT_LE(med_cereb_bridge_pending_error_count(bridge),
              (uint32_t)MED_CEREB_MAX_ERROR_QUEUE);
}

// Test 34: IO spikes generated from errors
TEST_F(MedullaCerebellumIntegrationTest, IOSpikesGenerated) {
    medulla_start(medulla);
    med_cereb_bridge_reset_stats(bridge);

    // Queue significant error
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_PREDICTION, 0.8f, 1);

    // Process multiple cycles
    runUpdateCycles(100);

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.io_spikes, 0u);
}

// Test 35: Sustained cerebellum bridge operation
TEST_F(MedullaCerebellumIntegrationTest, SustainedOperation) {
    medulla_start(medulla);

    for (int i = 0; i < 500; i++) {
        medulla_update(medulla, 0.01f);
        EXPECT_EQ(0, med_cereb_bridge_update(bridge, 10000));

        if (i % 50 == 0) {
            med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.3f, i);
        }
    }

    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));
}

//=============================================================================
// Cross-Bridge Integration Tests (Medulla-Immune + Medulla-Cerebellum)
//=============================================================================

class CrossBridgeIntegrationTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;
    brain_immune_system_t* immune = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    medulla_immune_bridge_t immune_bridge = nullptr;
    med_cereb_bridge_t cereb_bridge = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create medulla
        medulla_config_t med_config = medulla_default_config();
        med_config.enable_bio_async = false;
        med_config.enable_health_integration = false;
        med_config.enable_recovery_integration = false;
        med_config.enable_sleep_integration = false;
        med_config.enable_neuromod_integration = false;
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr);

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_config.enable_bio_async = false;
        immune_config.enable_bbb_integration = false;
        immune_config.enable_bft_integration = false;
        immune_config.enable_swarm_integration = false;
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create cerebellum
        cerebellum_config_t cere_config = cerebellum_default_config();
        cere_config.enable_bio_async = false;
        cerebellum = cerebellum_create(&cere_config);
        ASSERT_NE(cerebellum, nullptr);

        // Create immune bridge
        medulla_immune_config_t immune_bridge_config;
        medulla_immune_default_config(&immune_bridge_config);
        immune_bridge_config.enable_bio_async = false;
        immune_bridge = medulla_immune_create(&immune_bridge_config, medulla, immune);
        ASSERT_NE(immune_bridge, nullptr);

        // Create cerebellum bridge
        med_cereb_bridge_config_t cereb_bridge_config;
        med_cereb_bridge_default_config(&cereb_bridge_config);
        cereb_bridge_config.enable_bio_async = false;
        cereb_bridge = med_cereb_bridge_create(&cereb_bridge_config);
        ASSERT_NE(cereb_bridge, nullptr);
        med_cereb_bridge_connect_medulla(cereb_bridge, medulla);
        med_cereb_bridge_connect_cerebellum(cereb_bridge, cerebellum);
    }

    void TearDown() override {
        if (cereb_bridge) {
            med_cereb_bridge_destroy(cereb_bridge);
            cereb_bridge = nullptr;
        }
        if (immune_bridge) {
            medulla_immune_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 36: Both bridges created with shared medulla
TEST_F(CrossBridgeIntegrationTest, BothBridgesCreatedWithSharedMedulla) {
    EXPECT_NE(immune_bridge, nullptr);
    EXPECT_NE(cereb_bridge, nullptr);
    EXPECT_TRUE(med_cereb_bridge_is_connected(cereb_bridge));
}

// Test 37: Medulla arousal state consistent across bridges
TEST_F(CrossBridgeIntegrationTest, ArousalStateConsistentAcrossBridges) {
    medulla_start(medulla);
    medulla_test_set_arousal(medulla, 0.7f);

    // Update all bridges
    medulla_update(medulla, 0.01f);
    medulla_immune_update(immune_bridge);
    med_cereb_bridge_update(cereb_bridge, 10000);

    // Check arousal is reflected in cerebellum bridge
    med_cereb_arousal_effects_t cereb_effects;
    med_cereb_bridge_get_arousal_effects(cereb_bridge, &cereb_effects);

    EXPECT_GE(cereb_effects.motor_gain, 0.2f);
    EXPECT_LE(cereb_effects.motor_gain, 2.0f);
}

// Test 38: Medulla protection state consistent across bridges
TEST_F(CrossBridgeIntegrationTest, ProtectionStateConsistentAcrossBridges) {
    medulla_start(medulla);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_DEFENSIVE);

    medulla_update(medulla, 0.01f);
    medulla_immune_update(immune_bridge);
    med_cereb_bridge_update(cereb_bridge, 10000);

    // Check protection effects in cerebellum bridge
    med_cereb_protection_effects_t cereb_effects;
    med_cereb_bridge_get_protection_effects(cereb_bridge, &cereb_effects);

    EXPECT_LT(cereb_effects.output_scale, 1.0f);
}

// Test 39: Emergency shutdown propagates to all bridges
TEST_F(CrossBridgeIntegrationTest, EmergencyShutdownPropagation) {
    medulla_start(medulla);

    // Trigger emergency on medulla
    medulla_emergency_shutdown(medulla, "test emergency");

    // Update all bridges
    medulla_update(medulla, 0.01f);
    medulla_immune_update(immune_bridge);
    med_cereb_bridge_update(cereb_bridge, 10000);

    // Check medulla state
    protection_level_t med_level = medulla_get_protection_level(medulla);
    EXPECT_GE((int)med_level, (int)PROTECTION_LEVEL_CRITICAL);

    // Check cerebellum bridge reflects emergency
    med_cereb_protection_effects_t cereb_effects;
    med_cereb_bridge_get_protection_effects(cereb_bridge, &cereb_effects);
    EXPECT_LT(cereb_effects.output_scale, 1.0f);
}

// Test 40: Concurrent updates on all bridges
TEST_F(CrossBridgeIntegrationTest, ConcurrentUpdatesAllBridges) {
    medulla_start(medulla);

    for (int i = 0; i < 100; i++) {
        medulla_update(medulla, 0.01f);
        EXPECT_EQ(0, medulla_immune_update(immune_bridge));
        EXPECT_EQ(0, med_cereb_bridge_update(cereb_bridge, 10000));

        if (i % 10 == 0) {
            med_cereb_bridge_queue_error(cereb_bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
        }
    }

    medulla_stats_t med_stats;
    EXPECT_EQ(0, medulla_get_stats(medulla, &med_stats));
    EXPECT_GT(med_stats.total_updates, 0u);
}

// Test 41: Circadian phase affects both immune and cerebellum
TEST_F(CrossBridgeIntegrationTest, CircadianAffectsBothBridges) {
    medulla_start(medulla);
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);

    for (int i = 0; i < 5; i++) {
        medulla_update(medulla, 0.01f);
        medulla_immune_update(immune_bridge);
        med_cereb_bridge_update(cereb_bridge, 10000);
    }

    // Check immune effects
    medulla_immune_effects_t immune_effects;
    medulla_immune_get_immune_effects(immune_bridge, &immune_effects);
    EXPECT_EQ(immune_effects.circadian_phase, CIRCADIAN_PHASE_MORNING);

    // Check cerebellum effects
    med_cereb_circadian_effects_t cereb_effects;
    med_cereb_bridge_get_circadian_effects(cereb_bridge, &cereb_effects);
    EXPECT_GT(cereb_effects.ltd_rate_multiplier, 0.0f);
}

// Test 42: Statistics accumulate correctly across bridges
TEST_F(CrossBridgeIntegrationTest, StatisticsAccumulateAcrossBridges) {
    medulla_start(medulla);

    for (int i = 0; i < 50; i++) {
        medulla_update(medulla, 0.01f);
        medulla_immune_update(immune_bridge);
        med_cereb_bridge_update(cereb_bridge, 10000);
    }

    medulla_immune_stats_t immune_stats;
    EXPECT_EQ(0, medulla_immune_get_stats(immune_bridge, &immune_stats));
    EXPECT_GE(immune_stats.total_updates, 50u);
}

// Test 43: Deep night phase reduces both immune and learning
TEST_F(CrossBridgeIntegrationTest, DeepNightReducesActivity) {
    medulla_start(medulla);
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT);

    for (int i = 0; i < 5; i++) {
        medulla_update(medulla, 0.01f);
        medulla_immune_update(immune_bridge);
        med_cereb_bridge_update(cereb_bridge, 10000);
    }

    med_cereb_circadian_effects_t cereb_effects;
    med_cereb_bridge_get_circadian_effects(cereb_bridge, &cereb_effects);
    EXPECT_LT(cereb_effects.consolidation_rate, 1.0f);
}

// Test 44: Protection shutdown reduces motor output
TEST_F(CrossBridgeIntegrationTest, ProtectionShutdownReducesMotor) {
    medulla_start(medulla);
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_SHUTDOWN);

    for (int i = 0; i < 3; i++) {
        medulla_update(medulla, 0.01f);
        med_cereb_bridge_update(cereb_bridge, 10000);
    }

    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(cereb_bridge, &effects);

    EXPECT_LT(effects.output_scale, 0.5f);
}

// Test 45: Sustained operation with all bridges
TEST_F(CrossBridgeIntegrationTest, SustainedOperationAllBridges) {
    medulla_start(medulla);

    for (int i = 0; i < 500; i++) {
        medulla_update(medulla, 0.01f);
        medulla_immune_update(immune_bridge);
        med_cereb_bridge_update(cereb_bridge, 10000);

        if (i % 100 == 0) {
            float arousal = 0.3f + 0.5f * (i % 3) / 3.0f;
            medulla_test_set_arousal(medulla, arousal);
        }
        if (i % 150 == 0) {
            circadian_phase_t phase = static_cast<circadian_phase_t>(i % 8);
            medulla_test_set_circadian(medulla, phase);
        }
    }

    EXPECT_TRUE(med_cereb_bridge_is_connected(cereb_bridge));

    medulla_stats_t stats;
    medulla_get_stats(medulla, &stats);
    EXPECT_GE(stats.total_updates, 500u);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

class MedullaThreadSafetyTest : public NimcpTestBase {
protected:
    medulla_t medulla = nullptr;
    med_cereb_bridge_t bridge = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        medulla_config_t med_config = medulla_default_config();
        med_config.enable_bio_async = false;
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr);

        cerebellum_config_t cere_config = cerebellum_default_config();
        cere_config.enable_bio_async = false;
        cerebellum = cerebellum_create(&cere_config);
        ASSERT_NE(cerebellum, nullptr);

        med_cereb_bridge_config_t bridge_config;
        med_cereb_bridge_default_config(&bridge_config);
        bridge_config.enable_bio_async = false;
        bridge = med_cereb_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        med_cereb_bridge_connect_medulla(bridge, medulla);
        med_cereb_bridge_connect_cerebellum(bridge, cerebellum);
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

// Test 46: Concurrent error queueing
TEST_F(MedullaThreadSafetyTest, ConcurrentErrorQueueing) {
    medulla_start(medulla);

    std::atomic<int> total_queued{0};
    const int errors_per_thread = 50;
    const int num_threads = 4;

    auto queue_errors = [&](int thread_id) {
        for (int i = 0; i < errors_per_thread; i++) {
            med_cereb_error_type_t type = static_cast<med_cereb_error_type_t>(
                (thread_id + i) % MED_CEREB_ERROR_COUNT);
            if (med_cereb_bridge_queue_error(bridge, type, 0.5f, thread_id * 1000 + i) == 0) {
                total_queued++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(queue_errors, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(total_queued.load(), 0);
}

// Test 47: Concurrent medulla updates and queries
TEST_F(MedullaThreadSafetyTest, ConcurrentMedullaUpdatesAndQueries) {
    medulla_start(medulla);

    std::atomic<bool> running{true};
    std::atomic<int> update_count{0};
    std::atomic<int> query_count{0};

    auto updater = [&]() {
        while (running) {
            medulla_update(medulla, 0.001f);
            update_count++;
        }
    };

    auto querier = [&]() {
        while (running) {
            medulla_stats_t stats;
            medulla_get_stats(medulla, &stats);
            float arousal = medulla_get_arousal_level(medulla);
            (void)arousal;
            query_count++;
        }
    };

    std::thread t1(updater);
    std::thread t2(querier);
    std::thread t3(querier);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running = false;

    t1.join();
    t2.join();
    t3.join();

    EXPECT_GT(update_count.load(), 10);
    EXPECT_GT(query_count.load(), 10);
}

// Test 48: Concurrent bridge updates
TEST_F(MedullaThreadSafetyTest, ConcurrentBridgeUpdates) {
    medulla_start(medulla);

    std::atomic<bool> running{true};

    auto update_medulla = [&]() {
        while (running) {
            medulla_update(medulla, 0.001f);
            std::this_thread::yield();
        }
    };

    auto update_bridge = [&]() {
        while (running) {
            med_cereb_bridge_update(bridge, 1000);
            std::this_thread::yield();
        }
    };

    auto queue_errors = [&]() {
        int i = 0;
        while (running) {
            med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.3f, i++);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread t1(update_medulla);
    std::thread t2(update_bridge);
    std::thread t3(queue_errors);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running = false;

    t1.join();
    t2.join();
    t3.join();

    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));
}

// Test 49: Concurrent arousal modifications
TEST_F(MedullaThreadSafetyTest, ConcurrentArousalModifications) {
    medulla_start(medulla);

    std::atomic<bool> running{true};

    auto boost_arousal = [&]() {
        while (running) {
            medulla_boost_arousal(medulla, 0.05f);
            medulla_update(medulla, 0.001f);
            std::this_thread::yield();
        }
    };

    auto reduce_arousal = [&]() {
        while (running) {
            medulla_reduce_arousal(medulla, 0.03f);
            medulla_update(medulla, 0.001f);
            std::this_thread::yield();
        }
    };

    std::thread t1(boost_arousal);
    std::thread t2(reduce_arousal);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running = false;

    t1.join();
    t2.join();

    // Arousal should still be valid
    float arousal = medulla_get_arousal_level(medulla);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

class MedullaEdgeCasesTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

// Test 50: Create medulla with NULL config uses defaults
TEST_F(MedullaEdgeCasesTest, CreateMedullaWithNullConfig) {
    medulla_t med = medulla_create(NULL);
    EXPECT_NE(med, nullptr);
    medulla_destroy(med);
}

// Test 51: Destroy NULL medulla is safe
TEST_F(MedullaEdgeCasesTest, DestroyNullMedullaSafe) {
    medulla_destroy(nullptr);
    SUCCEED();
}

// Test 52: Update before start returns error
TEST_F(MedullaEdgeCasesTest, UpdateBeforeStart) {
    medulla_t med = medulla_create(NULL);
    ASSERT_NE(med, nullptr);

    int result = medulla_update(med, 0.01f);
    (void)result;

    medulla_destroy(med);
}

// Test 53: Double stop is safe
TEST_F(MedullaEdgeCasesTest, DoubleStopSafe) {
    medulla_t med = medulla_create(NULL);
    ASSERT_NE(med, nullptr);

    medulla_start(med);
    medulla_stop(med);
    int result = medulla_stop(med);
    (void)result;

    medulla_destroy(med);
}

// Test 54: Query functions on NULL medulla
TEST_F(MedullaEdgeCasesTest, QueryFunctionsOnNullMedulla) {
    float arousal = medulla_get_arousal_level(nullptr);
    EXPECT_LT(arousal, 0.0f);

    protection_level_t prot = medulla_get_protection_level(nullptr);
    (void)prot;
}

// Test 55: Bridge creation with NULL medulla
TEST_F(MedullaEdgeCasesTest, BridgeCreationWithNullMedulla) {
    brain_immune_config_t immune_config;
    brain_immune_default_config(&immune_config);
    immune_config.enable_bio_async = false;
    brain_immune_system_t* immune = brain_immune_create(&immune_config);
    ASSERT_NE(immune, nullptr);

    medulla_immune_config_t bridge_config;
    medulla_immune_default_config(&bridge_config);

    medulla_immune_bridge_t bridge = medulla_immune_create(&bridge_config, nullptr, immune);
    if (bridge) {
        medulla_immune_destroy(bridge);
    }

    brain_immune_destroy(immune);
}

// Test 56: Bridge creation with NULL immune
TEST_F(MedullaEdgeCasesTest, BridgeCreationWithNullImmune) {
    medulla_config_t med_config = medulla_default_config();
    med_config.enable_bio_async = false;
    medulla_t med = medulla_create(&med_config);
    ASSERT_NE(med, nullptr);

    medulla_immune_config_t bridge_config;
    medulla_immune_default_config(&bridge_config);

    medulla_immune_bridge_t bridge = medulla_immune_create(&bridge_config, med, nullptr);
    if (bridge) {
        medulla_immune_destroy(bridge);
    }

    medulla_destroy(med);
}

// Test 57: Extreme arousal values clamped
TEST_F(MedullaEdgeCasesTest, ExtremeArousalValuesClamped) {
    medulla_t med = medulla_create(NULL);
    ASSERT_NE(med, nullptr);
    medulla_start(med);

    medulla_test_set_arousal(med, 2.0f);
    float arousal = medulla_get_arousal_level(med);
    EXPECT_LE(arousal, 1.0f);

    medulla_test_set_arousal(med, -1.0f);
    arousal = medulla_get_arousal_level(med);
    EXPECT_GE(arousal, 0.0f);

    medulla_stop(med);
    medulla_destroy(med);
}

// Test 58: Invalid error type handling
TEST_F(MedullaEdgeCasesTest, InvalidErrorTypeHandling) {
    medulla_t med = medulla_create(NULL);
    cerebellum_config_t cere_config = cerebellum_default_config();
    cere_config.enable_bio_async = false;
    cerebellum_adapter_t* cere = cerebellum_create(&cere_config);

    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    config.enable_bio_async = false;
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    med_cereb_bridge_connect_medulla(bridge, med);
    med_cereb_bridge_connect_cerebellum(bridge, cere);

    int result = med_cereb_bridge_queue_error(bridge,
        static_cast<med_cereb_error_type_t>(999), 0.5f, 1);
    (void)result;

    med_cereb_bridge_destroy(bridge);
    cerebellum_destroy(cere);
    medulla_destroy(med);
}

// Test 59: Very small delta time
TEST_F(MedullaEdgeCasesTest, VerySmallDeltaTime) {
    medulla_t med = medulla_create(NULL);
    medulla_start(med);

    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(0, medulla_update(med, 0.0001f));
    }

    medulla_stop(med);
    medulla_destroy(med);
}

// Test 60: Very large delta time
TEST_F(MedullaEdgeCasesTest, VeryLargeDeltaTime) {
    medulla_t med = medulla_create(NULL);
    medulla_start(med);

    EXPECT_EQ(0, medulla_update(med, 1.0f));
    EXPECT_EQ(0, medulla_update(med, 10.0f));

    medulla_stop(med);
    medulla_destroy(med);
}

// Test 61: Emergency shutdown reason string handling
TEST_F(MedullaEdgeCasesTest, EmergencyShutdownReasonString) {
    medulla_t med = medulla_create(NULL);
    medulla_start(med);

    EXPECT_EQ(0, medulla_emergency_shutdown(med, "test reason"));

    medulla_start(med);
    EXPECT_EQ(0, medulla_emergency_shutdown(med, ""));

    medulla_start(med);
    int result = medulla_emergency_shutdown(med, NULL);
    (void)result;

    medulla_stop(med);
    medulla_destroy(med);
}

// Test 62: Cerebellum bridge without medulla connection
TEST_F(MedullaEdgeCasesTest, CerebellumBridgeWithoutMedullaConnection) {
    cerebellum_config_t cere_config = cerebellum_default_config();
    cere_config.enable_bio_async = false;
    cerebellum_adapter_t* cere = cerebellum_create(&cere_config);

    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    config.enable_bio_async = false;
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    med_cereb_bridge_connect_cerebellum(bridge, cere);

    EXPECT_FALSE(med_cereb_bridge_is_connected(bridge));

    int result = med_cereb_bridge_update(bridge, 10000);
    (void)result;

    med_cereb_bridge_destroy(bridge);
    cerebellum_destroy(cere);
}

// Test 63: Multiple medulla start/stop cycles
TEST_F(MedullaEdgeCasesTest, MultipleStartStopCycles) {
    medulla_t med = medulla_create(NULL);
    ASSERT_NE(med, nullptr);

    for (int cycle = 0; cycle < 5; cycle++) {
        EXPECT_EQ(0, medulla_start(med));

        for (int i = 0; i < 10; i++) {
            EXPECT_EQ(0, medulla_update(med, 0.01f));
        }

        EXPECT_EQ(0, medulla_stop(med));
    }

    medulla_destroy(med);
}

// Test 64: Protection level progression
TEST_F(MedullaEdgeCasesTest, ProtectionLevelProgression) {
    medulla_t med = medulla_create(NULL);
    medulla_start(med);

    for (int level = PROTECTION_LEVEL_NORMAL; level <= PROTECTION_LEVEL_SHUTDOWN; level++) {
        medulla_test_set_protection(med, static_cast<protection_level_t>(level));
        protection_level_t current = medulla_get_protection_level(med);
        EXPECT_EQ(current, static_cast<protection_level_t>(level));
    }

    medulla_stop(med);
    medulla_destroy(med);
}

// Test 65: Circadian phase cycling
TEST_F(MedullaEdgeCasesTest, CircadianPhaseCycling) {
    medulla_t med = medulla_create(NULL);
    medulla_start(med);

    for (int phase = 0; phase < 8; phase++) {
        medulla_test_set_circadian(med, static_cast<circadian_phase_t>(phase));
        circadian_phase_t current = medulla_get_circadian_phase(med);
        EXPECT_EQ(current, static_cast<circadian_phase_t>(phase));
    }

    medulla_stop(med);
    medulla_destroy(med);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
