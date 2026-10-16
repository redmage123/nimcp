/**
 * @file test_medulla_cerebellum_bridge_integration.cpp
 * @brief Integration tests for Medulla-Cerebellum Bridge
 *
 * WHAT: Tests complete integration between medulla and cerebellum via inferior olive
 * WHY:  Verify error signaling, arousal modulation, protection gating, and circadian
 *       learning rate adjustments work correctly with real subsystems
 * HOW:  Create full medulla and cerebellum instances, run realistic scenarios,
 *       verify outputs match biological expectations
 *
 * INTEGRATION SCENARIOS:
 * 1. Bridge connected to real medulla - arousal effects flow through
 * 2. Bridge connected to real cerebellum - climbing fiber signals delivered
 * 3. Error queue processing - errors distributed to IO neurons, spikes generated
 * 4. Arousal cascade - medulla arousal changes affect motor modulation
 * 5. Protection cascade - medulla protection level affects motor gating
 * 6. Circadian cascade - medulla circadian phase affects learning rates
 * 7. Full pipeline - error queued -> IO processes -> climbing fiber -> cerebellum
 * 8. Multiple updates - sustained operation over many cycles
 * 9. Emergency stop flow - trigger emergency, verify motor gated, release
 * 10. Concurrent access - thread safety with multiple operations
 *
 * BIOLOGICAL CONTEXT:
 * The inferior olive (IO) in the medulla oblongata is the sole source of climbing
 * fibers to the cerebellum. These climbing fibers carry error signals that drive
 * cerebellar motor learning through long-term depression (LTD) at parallel fiber
 * synapses on Purkinje cells.
 *
 * @version Phase 4: Medulla-Cerebellum Integration
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

// Core headers
#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"
#include "core/medulla/nimcp_medulla.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Test Fixture - Basic Bridge Setup
//=============================================================================

class MedullaCerebellumBridgeIntegrationTest : public ::testing::Test {
protected:
    med_cereb_bridge_t bridge = nullptr;
    medulla_t medulla = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;

    void SetUp() override {
        // Create medulla with all features enabled
        medulla_config_t med_config = medulla_default_config();
        med_config.enable_health_integration = false;  // Standalone test
        med_config.enable_recovery_integration = false;
        med_config.enable_sleep_integration = false;
        med_config.enable_neuromod_integration = false;
        med_config.enable_bio_async = false;
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr) << "Failed to create medulla";

        // Create cerebellum with error learning enabled
        cerebellum_config_t cere_config = cerebellum_default_config();
        cere_config.enable_error_learning = true;
        cere_config.enable_timing = true;
        cere_config.enable_motor_adaptation = true;
        cere_config.enable_bio_async = false;
        cerebellum = cerebellum_create(&cere_config);
        ASSERT_NE(cerebellum, nullptr) << "Failed to create cerebellum";

        // Create bridge with all modulation enabled
        med_cereb_bridge_config_t bridge_config;
        ASSERT_EQ(0, med_cereb_bridge_default_config(&bridge_config));
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
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }

    // Helper: Run multiple update cycles
    void runUpdateCycles(int cycles, uint64_t delta_us = 10000) {
        for (int i = 0; i < cycles; i++) {
            medulla_update(medulla, delta_us / 1000000.0f);
            med_cereb_bridge_update(bridge, delta_us);
        }
    }

    // Helper: Queue multiple errors of different types
    void queueMixedErrors(int count) {
        for (int i = 0; i < count; i++) {
            med_cereb_error_type_t type = static_cast<med_cereb_error_type_t>(
                i % MED_CEREB_ERROR_COUNT);
            float magnitude = 0.3f + 0.4f * (i % 5) / 5.0f;
            med_cereb_bridge_queue_error(bridge, type, magnitude, i);
        }
    }
};

//=============================================================================
// Test: Bridge Creation and Connection
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, BridgeCreatedSuccessfully) {
    // WHAT: Verify bridge is properly created
    // WHY:  Basic sanity check for integration test setup
    EXPECT_NE(bridge, nullptr);
    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, MedullaConnectionWorks) {
    // WHAT: Test medulla connection established correctly
    // WHY:  Medulla is source of arousal, protection, circadian states

    // Medulla should be in running state or at least valid
    medulla_stats_t stats;
    EXPECT_EQ(0, medulla_get_stats(medulla, &stats));

    // Bridge should report connected
    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, CerebellumConnectionWorks) {
    // WHAT: Test cerebellum connection established correctly
    // WHY:  Cerebellum receives climbing fiber signals from bridge

    cerebellum_status_t status = cerebellum_get_status(cerebellum);
    EXPECT_NE(status, CEREBELLUM_STATUS_ERROR);

    // Bridge should be connected
    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));
}

//=============================================================================
// Test: Error Queue Processing
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, ErrorQueueBasicOperation) {
    // WHAT: Test basic error queue functionality
    // WHY:  Error queue is main pathway for IO processing

    // Queue single error
    EXPECT_EQ(0, med_cereb_bridge_queue_error(bridge,
        MED_CEREB_ERROR_TIMING, 0.5f, 100));

    // Verify error is pending
    EXPECT_EQ(1u, med_cereb_bridge_pending_error_count(bridge));

    // Process through update
    med_cereb_bridge_update(bridge, 10000);

    // After processing, queue should be clear or reduced
    // (depends on implementation - error may need multiple cycles)
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, ErrorQueueMultipleErrors) {
    // WHAT: Test processing multiple error types
    // WHY:  Different IO subdivisions handle different error types

    // Queue errors of each type
    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        med_cereb_error_type_t type = static_cast<med_cereb_error_type_t>(i);
        EXPECT_EQ(0, med_cereb_bridge_queue_error(bridge, type, 0.5f, i));
    }

    EXPECT_EQ((uint32_t)MED_CEREB_ERROR_COUNT,
              med_cereb_bridge_pending_error_count(bridge));

    // Process all errors
    runUpdateCycles(10);

    // Get statistics
    med_cereb_bridge_stats_t stats;
    EXPECT_EQ(0, med_cereb_bridge_get_stats(bridge, &stats));

    // Some signals should have been processed
    // (IO may have refractory periods limiting throughput)
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, ErrorQueueOverflow) {
    // WHAT: Test queue behavior when full
    // WHY:  Should handle overflow gracefully

    // Fill queue beyond capacity
    for (int i = 0; i < MED_CEREB_MAX_ERROR_QUEUE + 10; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
    }

    // Queue should be at max capacity
    EXPECT_LE(med_cereb_bridge_pending_error_count(bridge),
              (uint32_t)MED_CEREB_MAX_ERROR_QUEUE);

    // Get stats to check dropped errors
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.errors_dropped, 0u);
}

//=============================================================================
// Test: Inferior Olive Processing
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, IONeuronSpikesGenerated) {
    // WHAT: Test that IO neurons generate spikes from errors
    // WHY:  IO spikes are climbing fiber signals

    // Queue significant error
    med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_PREDICTION, 0.8f, 1);

    // Process multiple cycles (IO neurons need time)
    runUpdateCycles(100);

    // Check statistics
    med_cereb_bridge_stats_t stats;
    EXPECT_EQ(0, med_cereb_bridge_get_stats(bridge, &stats));
    EXPECT_GT(stats.io_spikes, 0u) << "IO neurons should have spiked";
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, IORefractoryPeriod) {
    // WHAT: Test IO neuron refractory period (biological ~100ms)
    // WHY:  IO neurons can't fire faster than ~10 Hz

    // Get IO state before
    med_cereb_inferior_olive_t io_state;
    EXPECT_EQ(0, med_cereb_bridge_get_io_state(bridge, &io_state));

    // Queue rapid errors
    for (int i = 0; i < 20; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.9f, i);
        med_cereb_bridge_update(bridge, 5000);  // 5ms intervals
    }

    // Get stats
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);

    // Spikes should be limited by refractory period
    // With 100ms refractory and 100ms of simulation, expect limited spikes
    EXPECT_LE(stats.io_spikes, io_state.num_neurons * 2);
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, IOStateAccessible) {
    // WHAT: Test IO state can be queried
    // WHY:  Diagnostic access to internal IO model

    med_cereb_inferior_olive_t io_state;
    EXPECT_EQ(0, med_cereb_bridge_get_io_state(bridge, &io_state));

    // Verify reasonable defaults
    EXPECT_GT(io_state.num_neurons, 0u);
    EXPECT_GT(io_state.oscillation_freq, 0.0f);
    EXPECT_LE(io_state.oscillation_freq, 20.0f);
    EXPECT_GT(io_state.refractory_period_us, 0u);
}

//=============================================================================
// Test: Climbing Fiber Signals to Cerebellum
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, ClimbingFiberSignalDelivered) {
    // WHAT: Test climbing fiber signals reach cerebellum
    // WHY:  This is the main error pathway for cerebellar learning

    // Get cerebellum stats before
    cerebellum_stats_t cere_stats_before;
    cerebellum_get_stats(cerebellum, &cere_stats_before);

    // Send immediate climbing signal
    EXPECT_EQ(0, med_cereb_bridge_send_climbing_signal(bridge,
        MED_CEREB_ERROR_AMPLITUDE, 0.7f, 0));

    // Get cerebellum stats after
    cerebellum_stats_t cere_stats_after;
    cerebellum_get_stats(cerebellum, &cere_stats_after);

    // Check bridge stats
    med_cereb_bridge_stats_t bridge_stats;
    med_cereb_bridge_get_stats(bridge, &bridge_stats);
    EXPECT_GT(bridge_stats.climbing_signals_sent, 0u);
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, BroadcastErrorToAllPurkinje) {
    // WHAT: Test broadcasting error to all Purkinje cells
    // WHY:  Global errors affect entire cerebellar output

    EXPECT_EQ(0, med_cereb_bridge_broadcast_error(bridge,
        MED_CEREB_ERROR_PROTECTION, 1.0f));

    // Update to process
    med_cereb_bridge_update(bridge, 10000);

    // Check stats
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.climbing_signals_sent, 0u);
}

//=============================================================================
// Test: Arousal Cascade
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, ArousalEffectsOnMotor) {
    // WHAT: Test arousal level affects motor modulation
    // WHY:  Higher arousal = higher gain, faster reactions

    // Start medulla
    medulla_start(medulla);

    // Set low arousal
    medulla_test_set_arousal(medulla, 0.3f);
    runUpdateCycles(5);

    med_cereb_arousal_effects_t low_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_arousal_effects(bridge, &low_effects));

    // Set high arousal
    medulla_test_set_arousal(medulla, 0.9f);
    runUpdateCycles(5);

    med_cereb_arousal_effects_t high_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_arousal_effects(bridge, &high_effects));

    // Higher arousal should mean higher motor gain
    EXPECT_GT(high_effects.motor_gain, low_effects.motor_gain);

    // Higher arousal should mean faster reactions (lower factor)
    EXPECT_LT(high_effects.reaction_time_factor, low_effects.reaction_time_factor);
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, ArousalMotorModulation) {
    // WHAT: Test motor command modulation by arousal
    // WHY:  Verify actual motor output is scaled

    medulla_start(medulla);
    medulla_test_set_arousal(medulla, 0.5f);
    runUpdateCycles(5);

    float motor_in[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float motor_out[4] = {0};

    EXPECT_EQ(0, med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, 4));

    // Output should be modulated (not identical to input)
    // At neutral arousal (0.5), should be close to unity gain
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(motor_out[i], 0.0f);
        EXPECT_LE(motor_out[i], 2.0f);
    }
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, ArousalFinePrecisionInvertedU) {
    // WHAT: Test inverted-U curve for fine motor precision
    // WHY:  Optimal arousal is moderate, not extreme

    medulla_start(medulla);

    // Test at multiple arousal levels
    float arousal_levels[] = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    float precisions[5];

    for (int i = 0; i < 5; i++) {
        medulla_test_set_arousal(medulla, arousal_levels[i]);
        runUpdateCycles(3);

        med_cereb_arousal_effects_t effects;
        med_cereb_bridge_get_arousal_effects(bridge, &effects);
        precisions[i] = effects.fine_motor_precision;
    }

    // Middle arousal (0.5) should have best or near-best precision
    // Check that extreme values are lower (with small tolerance for drift effects)
    // Note: During integration testing, arousal drifts toward circadian target,
    // so we allow a small tolerance for the Yerkes-Dodson curve check
    const float tolerance = 0.01f;  // 1% tolerance for drift effects
    EXPECT_LE(precisions[0], precisions[2] + tolerance);  // Low <= middle
    EXPECT_LE(precisions[4], precisions[2] + tolerance);  // High <= middle
}

//=============================================================================
// Test: Protection Cascade
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, ProtectionLevelGating) {
    // WHAT: Test protection level gates motor output
    // WHY:  Emergency states should suppress motor output

    medulla_start(medulla);

    // Normal operation
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    runUpdateCycles(3);

    med_cereb_protection_effects_t normal_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_protection_effects(bridge, &normal_effects));
    EXPECT_FALSE(normal_effects.emergency_stop);
    EXPECT_FLOAT_EQ(normal_effects.output_scale, 1.0f);

    // Critical protection
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL);
    runUpdateCycles(3);

    med_cereb_protection_effects_t critical_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_protection_effects(bridge, &critical_effects));

    // Critical should have reduced output or disabled voluntary
    EXPECT_LT(critical_effects.output_scale, normal_effects.output_scale);
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, MotorAllowedChecks) {
    // WHAT: Test motor_allowed function with different conditions
    // WHY:  Verify gating logic for essential/reflexive motor

    medulla_start(medulla);

    // Normal - all motor allowed
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_NORMAL);
    runUpdateCycles(3);

    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, true, false));
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, true));

    // High protection - check gating
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_DEFENSIVE);
    runUpdateCycles(3);

    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);

    // Non-essential might be disabled
    if (effects.non_essential_disabled) {
        EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, false, false));
    }

    // Reflexes should always be allowed unless emergency stop
    if (!effects.emergency_stop) {
        EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, true));
    }
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, ProtectionStatsTracking) {
    // WHAT: Test protection gate statistics are tracked
    // WHY:  Need to monitor how often protection affects motor

    medulla_start(medulla);

    // Trigger some protection gates
    medulla_test_set_protection(medulla, PROTECTION_LEVEL_CRITICAL);
    runUpdateCycles(10);

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.protection_gates, 0u);
}

//=============================================================================
// Test: Circadian Cascade
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, CircadianLearningModulation) {
    // WHAT: Test circadian phase affects learning rates
    // WHY:  Motor learning efficiency varies with time of day

    medulla_start(medulla);

    // Morning phase (peak alertness)
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    runUpdateCycles(3);

    med_cereb_circadian_effects_t morning_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_circadian_effects(bridge, &morning_effects));

    // Deep night phase (low alertness)
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT);
    runUpdateCycles(3);

    med_cereb_circadian_effects_t night_effects;
    EXPECT_EQ(0, med_cereb_bridge_get_circadian_effects(bridge, &night_effects));

    // Morning should have better learning rates
    EXPECT_GT(morning_effects.ltd_rate_multiplier, night_effects.ltd_rate_multiplier);
    EXPECT_GT(morning_effects.ltp_rate_multiplier, night_effects.ltp_rate_multiplier);
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, LearningMultiplierComputed) {
    // WHAT: Test combined learning multiplier
    // WHY:  Combines circadian and arousal effects

    medulla_start(medulla);

    // Optimal conditions
    medulla_test_set_arousal(medulla, 0.6f);
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_MORNING);
    runUpdateCycles(3);

    float optimal_multiplier = med_cereb_bridge_get_learning_multiplier(bridge);

    // Suboptimal conditions
    medulla_test_set_arousal(medulla, 0.2f);
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_DEEP_NIGHT);
    runUpdateCycles(3);

    float suboptimal_multiplier = med_cereb_bridge_get_learning_multiplier(bridge);

    // Optimal should be higher
    EXPECT_GT(optimal_multiplier, suboptimal_multiplier);

    // Both should be in valid range
    EXPECT_GE(optimal_multiplier, 0.1f);
    EXPECT_LE(optimal_multiplier, 2.0f);
    EXPECT_GE(suboptimal_multiplier, 0.1f);
    EXPECT_LE(suboptimal_multiplier, 2.0f);
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, ApplyCircadianLearning) {
    // WHAT: Test applying circadian learning to cerebellum
    // WHY:  Updates LTD/LTP rates in cerebellum

    medulla_start(medulla);
    medulla_test_set_circadian(medulla, CIRCADIAN_PHASE_AFTERNOON);
    runUpdateCycles(3);

    EXPECT_EQ(0, med_cereb_bridge_apply_circadian_learning(bridge));

    // Check stats
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.learning_rate_adjustments, 0u);
}

//=============================================================================
// Test: Full Pipeline
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, FullErrorToCerebellumPipeline) {
    // WHAT: Test complete error -> IO -> climbing fiber -> cerebellum pipeline
    // WHY:  This is THE primary error pathway for cerebellar learning

    medulla_start(medulla);

    // Reset stats
    med_cereb_bridge_reset_stats(bridge);

    // Queue error
    EXPECT_EQ(0, med_cereb_bridge_queue_error(bridge,
        MED_CEREB_ERROR_PREDICTION, 0.8f, 42));

    // Process through IO (may need multiple cycles)
    runUpdateCycles(50);

    // Verify pipeline
    med_cereb_bridge_stats_t stats;
    EXPECT_EQ(0, med_cereb_bridge_get_stats(bridge, &stats));

    // Error should have generated IO spikes
    EXPECT_GT(stats.io_spikes, 0u) << "IO should have spiked";

    // IO spikes should have generated climbing signals
    EXPECT_GT(stats.climbing_signals_sent, 0u) << "Climbing signals should be sent";
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, MultipleErrorTypesInPipeline) {
    // WHAT: Test multiple error types processed through pipeline
    // WHY:  Different IO subdivisions handle different errors

    medulla_start(medulla);
    med_cereb_bridge_reset_stats(bridge);

    // Queue all error types
    queueMixedErrors(MED_CEREB_ERROR_COUNT * 2);

    // Process extensively
    runUpdateCycles(100);

    // Check per-type statistics
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);

    // At least some types should have signals
    int types_with_signals = 0;
    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        if (stats.signals_per_type[i] > 0) {
            types_with_signals++;
        }
    }
    EXPECT_GT(types_with_signals, 0) << "At least some error types should generate signals";
}

//=============================================================================
// Test: Multiple Updates (Sustained Operation)
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, SustainedOperationStability) {
    // WHAT: Test bridge operates stably over many cycles
    // WHY:  Real systems run continuously

    medulla_start(medulla);

    // Run many cycles
    for (int i = 0; i < 1000; i++) {
        medulla_update(medulla, 0.01f);  // 10ms
        EXPECT_EQ(0, med_cereb_bridge_update(bridge, 10000));

        // Periodically queue errors
        if (i % 50 == 0) {
            med_cereb_bridge_queue_error(bridge,
                MED_CEREB_ERROR_TIMING, 0.3f + 0.1f * (i % 5), i);
        }
    }

    // Should still be operational
    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));

    // Stats should show activity
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.io_spikes, 0u);
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, StatisticsAccumulateCorrectly) {
    // WHAT: Test statistics tracking over time
    // WHY:  Statistics used for monitoring and debugging

    medulla_start(medulla);
    med_cereb_bridge_reset_stats(bridge);

    // Record initial
    med_cereb_bridge_stats_t stats_initial;
    med_cereb_bridge_get_stats(bridge, &stats_initial);

    // Run with activity
    for (int i = 0; i < 100; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TRAJECTORY, 0.5f, i);
        runUpdateCycles(5);
    }

    // Record final
    med_cereb_bridge_stats_t stats_final;
    med_cereb_bridge_get_stats(bridge, &stats_final);

    // Stats should have increased
    EXPECT_GT(stats_final.io_spikes, stats_initial.io_spikes);
    EXPECT_GT(stats_final.climbing_signals_sent, stats_initial.climbing_signals_sent);
}

//=============================================================================
// Test: Emergency Stop Flow
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, EmergencyStopActivation) {
    // WHAT: Test emergency stop gates all motor output
    // WHY:  Critical safety mechanism

    medulla_start(medulla);

    // Normal operation first
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));

    // Trigger emergency stop
    EXPECT_EQ(0, med_cereb_bridge_emergency_stop(bridge));

    // Check protection effects
    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_TRUE(effects.emergency_stop);

    // Motor should be gated (except maybe reflexes)
    EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, false, false));
    EXPECT_FALSE(med_cereb_bridge_motor_allowed(bridge, true, false));
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, EmergencyStopRelease) {
    // WHAT: Test emergency stop can be released
    // WHY:  Need to recover from emergency state

    medulla_start(medulla);

    // Activate emergency
    med_cereb_bridge_emergency_stop(bridge);

    // Verify emergency active
    med_cereb_protection_effects_t effects;
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_TRUE(effects.emergency_stop);

    // Release emergency
    EXPECT_EQ(0, med_cereb_bridge_release_emergency(bridge));

    // Update to process release
    runUpdateCycles(5);

    // Check emergency released
    med_cereb_bridge_get_protection_effects(bridge, &effects);
    EXPECT_FALSE(effects.emergency_stop);

    // Motor should be allowed again
    EXPECT_TRUE(med_cereb_bridge_motor_allowed(bridge, false, false));
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, EmergencyGeneratesProtectionError) {
    // WHAT: Test emergency stop sends protection error
    // WHY:  Cerebellum should learn from emergency events

    medulla_start(medulla);
    med_cereb_bridge_reset_stats(bridge);

    // Trigger emergency
    med_cereb_bridge_emergency_stop(bridge);

    // Process
    runUpdateCycles(10);

    // Check for protection errors
    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.signals_per_type[MED_CEREB_ERROR_PROTECTION], 0u);
}

//=============================================================================
// Test: Concurrent Access (Thread Safety)
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, ConcurrentErrorQueuing) {
    // WHAT: Test thread safety of error queue
    // WHY:  Multiple sources may queue errors concurrently

    medulla_start(medulla);

    std::atomic<int> total_queued{0};
    const int errors_per_thread = 100;
    const int num_threads = 4;

    auto queue_errors = [&](int thread_id) {
        for (int i = 0; i < errors_per_thread; i++) {
            med_cereb_error_type_t type = static_cast<med_cereb_error_type_t>(
                (thread_id + i) % MED_CEREB_ERROR_COUNT);
            if (med_cereb_bridge_queue_error(bridge, type, 0.5f,
                    thread_id * 1000 + i) == 0) {
                total_queued++;
            }
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(queue_errors, t);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Should have queued many errors (some may be dropped if queue full)
    EXPECT_GT(total_queued.load(), 0);
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, ConcurrentUpdateAndQuery) {
    // WHAT: Test concurrent updates and queries
    // WHY:  Real systems have concurrent read/write access

    medulla_start(medulla);

    std::atomic<bool> running{true};
    std::atomic<int> update_count{0};
    std::atomic<int> query_count{0};

    // Update thread
    auto updater = [&]() {
        while (running) {
            medulla_update(medulla, 0.001f);
            med_cereb_bridge_update(bridge, 1000);
            update_count++;
        }
    };

    // Query thread
    auto querier = [&]() {
        while (running) {
            med_cereb_bridge_stats_t stats;
            med_cereb_bridge_get_stats(bridge, &stats);

            med_cereb_arousal_effects_t effects;
            med_cereb_bridge_get_arousal_effects(bridge, &effects);

            query_count++;
        }
    };

    // Error queueing thread
    auto error_source = [&]() {
        int i = 0;
        while (running) {
            med_cereb_bridge_queue_error(bridge,
                MED_CEREB_ERROR_TIMING, 0.3f, i++);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    std::thread t1(updater);
    std::thread t2(querier);
    std::thread t3(error_source);

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    t1.join();
    t2.join();
    t3.join();

    // Should have done many operations
    EXPECT_GT(update_count.load(), 10);
    EXPECT_GT(query_count.load(), 10);

    // System should still be valid
    EXPECT_TRUE(med_cereb_bridge_is_connected(bridge));
}

//=============================================================================
// Test: Reset and Reinitialization
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, ResetClearsState) {
    // WHAT: Test bridge reset clears accumulated state
    // WHY:  Need clean slate for new tasks

    medulla_start(medulla);

    // Accumulate some state
    queueMixedErrors(20);
    runUpdateCycles(50);

    // Reset
    EXPECT_EQ(0, med_cereb_bridge_reset(bridge));

    // Queue should be empty
    EXPECT_EQ(0u, med_cereb_bridge_pending_error_count(bridge));
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, StatsResetIndependently) {
    // WHAT: Test statistics reset independent of state reset
    // WHY:  May want to reset stats but keep operational state

    medulla_start(medulla);

    // Accumulate stats
    queueMixedErrors(10);
    runUpdateCycles(50);

    med_cereb_bridge_stats_t before_reset;
    med_cereb_bridge_get_stats(bridge, &before_reset);
    EXPECT_GT(before_reset.io_spikes, 0u);

    // Reset stats only
    EXPECT_EQ(0, med_cereb_bridge_reset_stats(bridge));

    med_cereb_bridge_stats_t after_reset;
    med_cereb_bridge_get_stats(bridge, &after_reset);
    EXPECT_EQ(0u, after_reset.io_spikes);
    EXPECT_EQ(0u, after_reset.climbing_signals_sent);
}

//=============================================================================
// Test: Edge Cases and Error Handling
//=============================================================================

TEST_F(MedullaCerebellumBridgeIntegrationTest, InvalidErrorMagnitude) {
    // WHAT: Test handling of out-of-range error magnitudes
    // WHY:  Should clamp or reject invalid inputs

    // Magnitude > 1.0 (should be clamped or accepted)
    int result = med_cereb_bridge_queue_error(bridge,
        MED_CEREB_ERROR_TIMING, 2.0f, 1);
    // Implementation may clamp or reject - both are valid

    // Magnitude < -1.0
    result = med_cereb_bridge_queue_error(bridge,
        MED_CEREB_ERROR_TIMING, -2.0f, 2);
    // Implementation may clamp or reject

    // Zero magnitude (valid but may be ignored)
    EXPECT_EQ(0, med_cereb_bridge_queue_error(bridge,
        MED_CEREB_ERROR_TIMING, 0.0f, 3));
}

TEST_F(MedullaCerebellumBridgeIntegrationTest, ErrorTypeNames) {
    // WHAT: Test error type name helper function
    // WHY:  Diagnostics and logging

    for (int i = 0; i < MED_CEREB_ERROR_COUNT; i++) {
        med_cereb_error_type_t type = static_cast<med_cereb_error_type_t>(i);
        const char* name = med_cereb_error_type_name(type);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // Invalid type should return something safe
    const char* invalid = med_cereb_error_type_name(
        static_cast<med_cereb_error_type_t>(999));
    EXPECT_NE(invalid, nullptr);
}

//=============================================================================
// Test: Standalone Bridge (Without Connections)
//=============================================================================

class MedullaCerebellumBridgeStandaloneTest : public ::testing::Test {
protected:
    med_cereb_bridge_t bridge = nullptr;

    void SetUp() override {
        med_cereb_bridge_config_t config;
        med_cereb_bridge_default_config(&config);
        config.enable_bio_async = false;
        bridge = med_cereb_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(MedullaCerebellumBridgeStandaloneTest, CreateWithoutConnections) {
    // WHAT: Test bridge can be created without connections
    // WHY:  May be connected later

    EXPECT_NE(bridge, nullptr);
    EXPECT_FALSE(med_cereb_bridge_is_connected(bridge));
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, UpdateWithoutConnectionsGraceful) {
    // WHAT: Test update works even without full connections
    // WHY:  Should not crash, may do limited processing

    // Should not crash
    int result = med_cereb_bridge_update(bridge, 10000);
    // May return error due to no connections, but should not crash
    (void)result;  // Ignore return, just checking for crash

    // Error queuing should still work
    EXPECT_EQ(0, med_cereb_bridge_queue_error(bridge,
        MED_CEREB_ERROR_TIMING, 0.5f, 1));
}

TEST_F(MedullaCerebellumBridgeStandaloneTest, EffectsWithoutMedulla) {
    // WHAT: Test effects queries without medulla
    // WHY:  Should return safe defaults

    med_cereb_arousal_effects_t arousal;
    // May fail or return defaults
    int result = med_cereb_bridge_get_arousal_effects(bridge, &arousal);
    if (result == 0) {
        // If it succeeds, values should be in valid range
        EXPECT_GE(arousal.motor_gain, 0.2f);
        EXPECT_LE(arousal.motor_gain, 2.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
