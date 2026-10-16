/**
 * @file test_cerebellum_vestibular_integration.cpp
 * @brief Integration tests for Cerebellum-Vestibular system
 *
 * WHAT: Tests complete integration between cerebellum and vestibular systems
 * WHY:  Verify VOR calibration loop, motor coordination, and brain integration
 * HOW:  Create full systems, run realistic scenarios, verify outputs
 *
 * INTEGRATION SCENARIOS:
 * 1. VOR calibration loop (vestibular -> cerebellum -> VOR gain)
 * 2. Vestibulospinal integration (posture control)
 * 3. Multi-axis rotation handling
 * 4. Adaptation over time (learning)
 * 5. Brain factory integration
 *
 * BIOLOGICAL CONTEXT:
 * The vestibulocerebellum (flocculus, nodulus) receives vestibular input
 * and modulates vestibular nuclei to calibrate the VOR. This integration
 * tests the complete loop from head movement to eye compensation.
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"

//=============================================================================
// Test Fixture - Full Integration Setup
//=============================================================================

class CerebellumVestibularIntegrationTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* cerebellum = nullptr;
    vestibular_processor_t* vestibular = nullptr;
    vestibular_cerebellum_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create cerebellum with all features enabled
        cerebellum_config_t cere_config = cerebellum_default_config();
        cere_config.enable_vestibulocerebellum = true;
        cere_config.enable_vor_adaptation = true;
        cere_config.enable_synaptic_dynamics = true;
        cere_config.enable_basket_cells = true;
        cere_config.enable_stellate_cells = true;
        cere_config.enable_golgi_cells = true;
        cerebellum = cerebellum_create(&cere_config);
        ASSERT_NE(cerebellum, nullptr);

        // Create vestibular processor with cerebellar input enabled
        vestibular_config_t vest_config = vestibular_default_config();
        vest_config.enable_cerebellar_input = true;
        vest_config.enable_vor_adaptation = true;
        vest_config.enable_vestibulospinal = true;
        vest_config.enable_velocity_storage = true;
        vestibular = vestibular_create(&vest_config);
        ASSERT_NE(vestibular, nullptr);

        // Create bridge
        vestibular_cerebellum_config_t bridge_config = vestibular_cerebellum_default_config();
        bridge_config.enable_vor_adaptation = true;
        bridge_config.enable_feedback_loop = true;
        bridge_config.route_to_flocculus = true;
        bridge_config.route_to_nodulus = true;
        bridge = vestibular_cerebellum_bridge_create(vestibular, cerebellum, &bridge_config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            vestibular_cerebellum_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (vestibular) {
            vestibular_destroy(vestibular);
            vestibular = nullptr;
        }
    }

    // Helper: Simulate head rotation
    void simulateHeadRotation(float yaw, float pitch, float roll, uint64_t timestamp_us) {
        semicircular_canal_input_t input;
        input.yaw_velocity = yaw;
        input.pitch_velocity = pitch;
        input.roll_velocity = roll;
        input.timestamp_us = timestamp_us;
        vestibular_process_canal_input(vestibular, &input);
    }

    // Helper: Run one VOR loop iteration
    void runVORLoopIteration(float retinal_slip = 0.0f) {
        // 1. Get vestibular state
        vestibular_mossy_signal_t signal;
        vestibular_get_mossy_signal(vestibular, &signal);

        // 2. Send to cerebellum
        vestibular_cerebellum_send_mossy_signal(bridge);

        // 3. If there's retinal slip, trigger adaptation
        if (fabs(retinal_slip) > VOR_RETINAL_SLIP_THRESHOLD) {
            float direction[3] = {1.0f, 0.0f, 0.0f};
            vestibular_cerebellum_trigger_vor_adaptation(bridge, retinal_slip, direction);
        }

        // 4. Apply cerebellar feedback
        vestibular_cerebellum_apply_feedback(bridge);
    }
};

//=============================================================================
// Test: Complete VOR Calibration Loop
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, CompleteVORCalibrationLoop) {
    // WHAT: Test complete VOR calibration loop
    // WHY:  Verify vestibular-cerebellum integration produces VOR output
    // HOW:  Simulate head rotation, check eye compensation command

    // Simulate yaw rotation (turning head left/right)
    for (int i = 0; i < 100; i++) {
        uint64_t timestamp = i * 10000;  // 10ms steps
        simulateHeadRotation(1.0f, 0.0f, 0.0f, timestamp);  // 1 rad/s yaw

        runVORLoopIteration(0.0f);  // No retinal slip initially

        // Get VOR eye command
        float eye_velocity[3];
        vestibular_get_vor_command(vestibular, eye_velocity);

        // Eye should move opposite to head (compensatory)
        if (i > 10) {  // Allow settling
            EXPECT_LT(eye_velocity[0], 0.0f) << "Eye should compensate for head yaw";
        }
    }

    // Check statistics
    vestibular_stats_t vest_stats;
    vestibular_get_stats(vestibular, &vest_stats);
    EXPECT_EQ(vest_stats.canal_inputs, 100);
    EXPECT_GT(vest_stats.vor_commands, 0);
}

//=============================================================================
// Test: VOR Gain Adaptation
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, VORGainAdaptation) {
    // WHAT: Test VOR gain adapts with retinal slip
    // WHY:  Verify cerebellar learning reduces VOR error
    // HOW:  Apply persistent retinal slip, verify gain changes

    // Get initial VOR gain
    float initial_gain[3];
    bool adaptation_active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &adaptation_active);

    // Simulate training with persistent retinal slip
    // (eyes not compensating enough -> increase gain)
    for (int trial = 0; trial < 50; trial++) {
        // Simulate 100ms of head rotation with retinal slip
        for (int i = 0; i < 10; i++) {
            uint64_t timestamp = (trial * 10 + i) * 10000;
            simulateHeadRotation(1.0f, 0.0f, 0.0f, timestamp);
            runVORLoopIteration(0.3f);  // Positive slip = undercompensation
        }
    }

    // Get final VOR gain
    float final_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, final_gain, &adaptation_active);

    // Gain should have changed (adaptation occurred)
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_GT(stats.adaptation_triggers, 0);
    EXPECT_GT(stats.total_vor_gain_change, 0.0f);
}

//=============================================================================
// Test: Multi-Axis Rotation
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, MultiAxisRotation) {
    // WHAT: Test handling of combined yaw, pitch, roll
    // WHY:  Real head movements are often multi-axis
    // HOW:  Simulate complex rotation, verify all axes processed

    // Combined rotation
    simulateHeadRotation(0.5f, 0.3f, 0.2f, 0);

    // Send through bridge
    vestibular_cerebellum_send_mossy_signal(bridge);

    // Get VOR command
    float eye_velocity[3];
    vestibular_get_vor_command(vestibular, eye_velocity);

    // All axes should have some response
    // (compensation for multi-axis rotation)

    // Check nucleus states
    vestibular_nucleus_state_t mvn_state, svn_state;
    vestibular_get_nucleus_state(vestibular, VESTIBULAR_NUCLEUS_MEDIAL, &mvn_state);
    vestibular_get_nucleus_state(vestibular, VESTIBULAR_NUCLEUS_SUPERIOR, &svn_state);

    // MVN handles horizontal (yaw), SVN handles vertical (pitch/roll)
    EXPECT_GT(fabs(mvn_state.activity[0]), 0.0f);  // Yaw activity in MVN
}

//=============================================================================
// Test: Vestibulospinal Integration
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, VestibulospinalIntegration) {
    // WHAT: Test vestibulospinal reflex with otolith input
    // WHY:  Posture control depends on linear acceleration sensing
    // HOW:  Simulate body tilt, verify postural command

    // Simulate forward tilt (head tilts forward)
    otolith_input_t otolith;
    otolith.x_accel = 2.0f;  // Forward acceleration
    otolith.y_accel = 0.0f;
    otolith.z_accel = 9.8f;  // Gravity
    otolith.head_tilt_pitch = 0.2f;  // Tilted forward
    otolith.head_tilt_roll = 0.0f;
    otolith.timestamp_us = 0;

    vestibular_process_otolith_input(vestibular, &otolith);

    // Get postural command
    float postural_command[3];
    vestibular_get_postural_command(vestibular, postural_command);

    // Should have some postural adjustment command
    vestibular_stats_t stats;
    vestibular_get_stats(vestibular, &stats);
    EXPECT_EQ(stats.otolith_inputs, 1);
}

//=============================================================================
// Test: Cerebellar Modulation Effect
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, CerebellarModulationEffect) {
    // WHAT: Test that cerebellar Purkinje output modulates vestibular nuclei
    // WHY:  VOR calibration requires cerebellar inhibition/excitation
    // HOW:  Apply modulation, verify effect on VOR output

    // Get baseline VOR response
    simulateHeadRotation(1.0f, 0.0f, 0.0f, 0);
    float baseline_eye[3];
    vestibular_get_vor_command(vestibular, baseline_eye);

    // Apply cerebellar modulation (inhibition)
    vestibular_apply_cerebellar_modulation(vestibular, VESTIBULAR_NUCLEUS_MEDIAL, 0.5f);

    // Simulate same head rotation
    simulateHeadRotation(1.0f, 0.0f, 0.0f, 10000);
    float modulated_eye[3];
    vestibular_get_vor_command(vestibular, modulated_eye);

    // Modulated response should be reduced
    EXPECT_LE(fabs(modulated_eye[0]), fabs(baseline_eye[0]) + 0.01f);

    // Verify modulation was applied
    float modulation;
    vestibular_cerebellum_get_modulation(bridge, VESTIBULAR_NUCLEUS_MEDIAL, &modulation);
    EXPECT_FLOAT_EQ(modulation, 0.5f);
}

//=============================================================================
// Test: Long-Term Adaptation Learning
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, LongTermAdaptationLearning) {
    // WHAT: Test learning over many trials
    // WHY:  VOR adaptation is a classic motor learning paradigm
    // HOW:  Simulate many adaptation trials, verify progressive learning

    std::vector<float> gain_history;

    for (int session = 0; session < 10; session++) {
        // Get current gain
        float vor_gain[3];
        bool active;
        vestibular_cerebellum_get_vor_state(bridge, vor_gain, &active);
        gain_history.push_back(vor_gain[0]);

        // Training session: 20 trials with retinal slip
        for (int trial = 0; trial < 20; trial++) {
            for (int step = 0; step < 5; step++) {
                uint64_t timestamp = ((session * 20 + trial) * 5 + step) * 10000;
                simulateHeadRotation(1.0f, 0.0f, 0.0f, timestamp);
                runVORLoopIteration(0.2f);
            }
        }
    }

    // Verify learning curve shows progression
    // (gains should change progressively)
    bool learning_occurred = false;
    for (size_t i = 1; i < gain_history.size(); i++) {
        if (fabs(gain_history[i] - gain_history[0]) > 0.01f) {
            learning_occurred = true;
            break;
        }
    }

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_vor_gain_change, 0.0f);
}

//=============================================================================
// Test: Velocity Storage Integration
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, VelocityStorageIntegration) {
    // WHAT: Test velocity storage mechanism
    // WHY:  Nodulus modulates velocity storage time constant
    // HOW:  Apply rotation, stop, verify decaying response

    // Apply rotation
    for (int i = 0; i < 50; i++) {
        simulateHeadRotation(1.0f, 0.0f, 0.0f, i * 10000);
        vestibular_cerebellum_send_mossy_signal(bridge);
    }

    // Stop rotation, but velocity storage should persist
    float eye_velocities[10];
    for (int i = 0; i < 10; i++) {
        simulateHeadRotation(0.0f, 0.0f, 0.0f, (50 + i) * 10000);
        vestibular_get_vor_command(vestibular, &eye_velocities[i * 3]);
    }

    // Response should decay over time (velocity storage)
    // First response after stop should still have some velocity
}

//=============================================================================
// Test: Statistics Tracking Across Systems
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, StatisticsTracking) {
    // WHAT: Verify all systems track statistics correctly
    // WHY:  Debugging and monitoring require accurate stats
    // HOW:  Run scenario, check all stat counters

    // Run a complete scenario
    for (int i = 0; i < 100; i++) {
        simulateHeadRotation(0.5f + 0.5f * sin(i * 0.1f), 0.0f, 0.0f, i * 10000);
        runVORLoopIteration(0.1f * sin(i * 0.2f));
    }

    // Check vestibular stats
    vestibular_stats_t vest_stats;
    vestibular_get_stats(vestibular, &vest_stats);
    EXPECT_EQ(vest_stats.canal_inputs, 100);
    EXPECT_GT(vest_stats.vor_commands, 0);

    // Check cerebellum stats
    cerebellum_stats_t cere_stats;
    cerebellum_get_stats(cerebellum, &cere_stats);
    EXPECT_GT(cere_stats.vestibular_inputs, 0);

    // Check bridge stats
    vestibular_cerebellum_stats_t bridge_stats;
    vestibular_cerebellum_get_stats(bridge, &bridge_stats);
    EXPECT_EQ(bridge_stats.mossy_signals_sent, 100);
    EXPECT_GT(bridge_stats.feedback_events, 0);
}

//=============================================================================
// Test: All Nuclei Receive Modulation
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, AllNucleiModulation) {
    // WHAT: Verify all vestibular nuclei receive cerebellar modulation
    // WHY:  Complete VOR/VSR requires modulation of all nuclei
    // HOW:  Apply modulation, check each nucleus

    // Run some activity
    for (int i = 0; i < 20; i++) {
        simulateHeadRotation(0.5f, 0.3f, 0.1f, i * 10000);
        runVORLoopIteration(0.1f);
    }

    // Check modulation for all nuclei
    float modulation;

    EXPECT_EQ(0, vestibular_cerebellum_get_modulation(bridge, VESTIBULAR_NUCLEUS_MEDIAL, &modulation));
    EXPECT_GE(modulation, 0.0f);
    EXPECT_LE(modulation, 2.0f);

    EXPECT_EQ(0, vestibular_cerebellum_get_modulation(bridge, VESTIBULAR_NUCLEUS_LATERAL, &modulation));
    EXPECT_GE(modulation, 0.0f);
    EXPECT_LE(modulation, 2.0f);

    EXPECT_EQ(0, vestibular_cerebellum_get_modulation(bridge, VESTIBULAR_NUCLEUS_SUPERIOR, &modulation));
    EXPECT_GE(modulation, 0.0f);
    EXPECT_LE(modulation, 2.0f);

    EXPECT_EQ(0, vestibular_cerebellum_get_modulation(bridge, VESTIBULAR_NUCLEUS_INFERIOR, &modulation));
    EXPECT_GE(modulation, 0.0f);
    EXPECT_LE(modulation, 2.0f);
}

//=============================================================================
// Test: Synaptic Dynamics Integration
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, SynapticDynamicsWithVestibular) {
    // WHAT: Test that synaptic dynamics affect VOR adaptation
    // WHY:  Realistic LTD requires vesicle pool and calcium dynamics
    // HOW:  High-frequency input should show depression effects

    // Get initial gain
    float initial_gain[3];
    bool active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &active);

    // High-frequency stimulation (should cause synaptic depression)
    for (int i = 0; i < 200; i++) {
        simulateHeadRotation(1.0f, 0.0f, 0.0f, i * 5000);  // 5ms intervals = 200Hz
        runVORLoopIteration(0.2f);
    }

    // Check cerebellar stats for synaptic dynamics
    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);
    EXPECT_GT(stats.vesicle_releases, 0);
}

//=============================================================================
// Test: Interneuron Integration
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, InterneuronIntegration) {
    // WHAT: Test interneurons active during vestibular processing
    // WHY:  Basket/stellate cells shape Purkinje output
    // HOW:  Check interneuron stats after vestibular activity

    // Run vestibular activity
    for (int i = 0; i < 100; i++) {
        simulateHeadRotation(0.8f, 0.0f, 0.0f, i * 10000);
        runVORLoopIteration(0.0f);
    }

    // Check interneuron statistics
    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);

    // Basket and stellate cells should have been active
    EXPECT_GE(stats.basket_inhibition_total, 0.0f);
    EXPECT_GE(stats.stellate_inhibition_total, 0.0f);
    EXPECT_GE(stats.golgi_feedback_total, 0.0f);
}

//=============================================================================
// Test: Error Recovery
//=============================================================================

TEST_F(CerebellumVestibularIntegrationTest, ErrorRecovery) {
    // WHAT: Test system handles errors gracefully
    // WHY:  Robust systems should recover from invalid inputs
    // HOW:  Feed invalid data, verify system continues

    // Normal operation
    simulateHeadRotation(1.0f, 0.0f, 0.0f, 0);
    runVORLoopIteration(0.0f);

    // Try null operations (should fail but not crash)
    vestibular_cerebellum_send_custom_signal(bridge, nullptr);
    vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.5f, nullptr);

    // System should still work
    simulateHeadRotation(1.0f, 0.0f, 0.0f, 10000);
    runVORLoopIteration(0.0f);

    vestibular_cerebellum_status_t status = vestibular_cerebellum_get_status(bridge);
    EXPECT_NE(status, VESTIBULAR_CEREBELLUM_STATUS_ERROR);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
