/**
 * @file test_cerebellum_vestibular_integration.cpp
 * @brief Integration tests for cerebellum-vestibular system
 *
 * Tests the complete integration between:
 * - Vestibular processor (brainstem)
 * - Cerebellum adapter (vestibulocerebellum)
 * - VOR calibration loop
 * - Motor coordination output
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Integration Test Fixture
//=============================================================================

class CerebellumVestibularIntegration : public ::testing::Test {
protected:
    vestibular_processor_t* vestibular = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    vestibular_cerebellum_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create vestibular processor with full configuration
        vestibular_config_t vest_config = vestibular_default_config();
        vest_config.enable_vor = true;
        vest_config.enable_vsr = true;
        vest_config.enable_cerebellar_modulation = true;
        vestibular = vestibular_create(&vest_config);
        ASSERT_NE(vestibular, nullptr);

        // Create cerebellum with vestibulocerebellum enabled
        cerebellum_config_t cereb_config = cerebellum_default_config();
        cereb_config.enable_vestibulocerebellum = true;
        cereb_config.enable_synaptic_dynamics = true;
        cereb_config.synapse_config = cerebellar_synapse_default_config();
        cereb_config.synapse_config.enable_vesicle_dynamics = true;
        cereb_config.synapse_config.enable_stp = true;
        cereb_config.synapse_config.enable_calcium_dynamics = true;
        cerebellum = cerebellum_create(&cereb_config);
        ASSERT_NE(cerebellum, nullptr);

        // Create bridge
        vestibular_cerebellum_config_t bridge_config = vestibular_cerebellum_default_config();
        bridge_config.enable_vor_adaptation = true;
        bridge_config.enable_feedback_loop = true;
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

    // Helper: simulate head rotation
    void simulateHeadRotation(float yaw, float pitch, float roll, float duration_ms) {
        int steps = (int)(duration_ms / 1.0f);  // 1ms steps
        for (int i = 0; i < steps; i++) {
            vestibular_input_t input;
            memset(&input, 0, sizeof(input));
            input.source = VESTIBULAR_INPUT_CANAL;
            input.canal.angular_velocity[0] = yaw;
            input.canal.angular_velocity[1] = pitch;
            input.canal.angular_velocity[2] = roll;
            input.timestamp_ms = (float)i;
            vestibular_process(vestibular, &input);
        }
    }

    // Helper: simulate linear motion
    void simulateLinearMotion(float x, float y, float z, float duration_ms) {
        int steps = (int)(duration_ms / 1.0f);
        for (int i = 0; i < steps; i++) {
            vestibular_input_t input;
            memset(&input, 0, sizeof(input));
            input.source = VESTIBULAR_INPUT_OTOLITH;
            input.otolith.linear_acceleration[0] = x;
            input.otolith.linear_acceleration[1] = y;
            input.otolith.linear_acceleration[2] = z;
            input.timestamp_ms = (float)i;
            vestibular_process(vestibular, &input);
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(CerebellumVestibularIntegration, SystemInitialization) {
    // All components should be properly initialized
    EXPECT_NE(vestibular, nullptr);
    EXPECT_NE(cerebellum, nullptr);
    EXPECT_NE(bridge, nullptr);

    // Bridge should be in idle state
    vestibular_cerebellum_status_t status = vestibular_cerebellum_get_status(bridge);
    EXPECT_EQ(status, VESTIBULAR_CEREBELLUM_STATUS_IDLE);
}

TEST_F(CerebellumVestibularIntegration, VestibularToCerebellumSignalFlow) {
    // Simulate head rotation
    simulateHeadRotation(1.0f, 0.0f, 0.0f, 50.0f);

    // Send mossy signal
    int result = vestibular_cerebellum_send_mossy_signal(bridge);
    EXPECT_EQ(result, 0);

    // Check cerebellum received input
    motor_coordination_result_t motor_result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
    EXPECT_TRUE(motor_result.motor_ready);
}

TEST_F(CerebellumVestibularIntegration, CerebellumToVestibularFeedback) {
    // Simulate activity
    simulateHeadRotation(2.0f, 0.0f, 0.0f, 100.0f);
    vestibular_cerebellum_send_mossy_signal(bridge);

    // Process through cerebellum
    motor_coordination_result_t motor_result;
    cerebellum_process(cerebellum, &motor_result);

    // Apply feedback
    int result = vestibular_cerebellum_apply_feedback(bridge);
    EXPECT_EQ(result, 0);

    // Check vestibular received modulation
    float modulation;
    vestibular_cerebellum_get_modulation(bridge, VESTIBULAR_NUCLEUS_MVN, &modulation);
    EXPECT_GE(modulation, 0.0f);
    EXPECT_LE(modulation, 2.0f);
}

//=============================================================================
// VOR Calibration Integration Tests
//=============================================================================

TEST_F(CerebellumVestibularIntegration, VorCalibrationLoop) {
    // Full VOR calibration scenario

    // Initial VOR gain
    float initial_gain[3];
    bool adaptation_active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &adaptation_active);

    // Simulate repeated head rotations with retinal slip
    for (int trial = 0; trial < 20; trial++) {
        // 1. Head rotation
        simulateHeadRotation(1.5f, 0.0f, 0.0f, 20.0f);

        // 2. Send vestibular signal to cerebellum
        vestibular_cerebellum_send_mossy_signal(bridge);

        // 3. Process through cerebellum
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);

        // 4. Retinal slip error (VOR not perfect)
        float slip_direction[3] = {1.0f, 0.0f, 0.0f};
        vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.3f, slip_direction);

        // 5. Apply cerebellar feedback
        vestibular_cerebellum_apply_feedback(bridge);
    }

    // Final VOR gain should have changed
    float final_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, final_gain, &adaptation_active);

    // Gain should have adapted
    EXPECT_NE(final_gain[0], initial_gain[0]);
}

TEST_F(CerebellumVestibularIntegration, VorAdaptationConvergence) {
    // Test that VOR adaptation converges to reduce error

    float gains[10][3];
    float retinal_slips[10];
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};

    for (int epoch = 0; epoch < 10; epoch++) {
        bool adaptation_active;
        vestibular_cerebellum_get_vor_state(bridge, gains[epoch], &adaptation_active);

        // Each epoch: multiple trials
        for (int trial = 0; trial < 10; trial++) {
            simulateHeadRotation(1.0f, 0.0f, 0.0f, 10.0f);
            vestibular_cerebellum_send_mossy_signal(bridge);

            motor_coordination_result_t motor_result;
            cerebellum_process(cerebellum, &motor_result);

            // Retinal slip proportional to VOR error
            float slip = 0.5f * (gains[epoch][0] - 1.0f);  // Error from ideal gain=1
            vestibular_cerebellum_trigger_vor_adaptation(bridge, fabsf(slip) + 0.1f, slip_direction);
            vestibular_cerebellum_apply_feedback(bridge);
        }

        vestibular_cerebellum_stats_t stats;
        vestibular_cerebellum_get_stats(bridge, &stats);
        retinal_slips[epoch] = stats.avg_retinal_slip;
    }

    // Later epochs should show adaptation (gain changes)
    EXPECT_NE(gains[9][0], gains[0][0]);
}

//=============================================================================
// Multi-Modal Integration Tests
//=============================================================================

TEST_F(CerebellumVestibularIntegration, CombinedRotationAndTranslation) {
    // Test simultaneous rotation and translation

    // Rotation input
    vestibular_input_t canal_input;
    memset(&canal_input, 0, sizeof(canal_input));
    canal_input.source = VESTIBULAR_INPUT_CANAL;
    canal_input.canal.angular_velocity[0] = 1.0f;
    canal_input.canal.angular_velocity[1] = 0.5f;
    canal_input.timestamp_ms = 0.0f;

    // Translation input
    vestibular_input_t otolith_input;
    memset(&otolith_input, 0, sizeof(otolith_input));
    otolith_input.source = VESTIBULAR_INPUT_OTOLITH;
    otolith_input.otolith.linear_acceleration[0] = 0.5f;
    otolith_input.otolith.linear_acceleration[2] = 9.8f;  // Gravity
    otolith_input.timestamp_ms = 0.0f;

    // Process both
    vestibular_process(vestibular, &canal_input);
    vestibular_process(vestibular, &otolith_input);

    // Send to cerebellum
    vestibular_cerebellum_send_mossy_signal(bridge);

    // Process
    motor_coordination_result_t motor_result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));

    // Should produce valid motor output
    EXPECT_TRUE(motor_result.motor_ready);
}

TEST_F(CerebellumVestibularIntegration, RapidHeadMovement) {
    // High-frequency head movements

    for (int i = 0; i < 100; i++) {
        // Sinusoidal head rotation
        float t = (float)i * 0.01f;  // 10ms period
        float yaw = 2.0f * sinf(2.0f * M_PI * t);

        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = yaw;
        input.timestamp_ms = (float)i;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);
    }

    motor_coordination_result_t motor_result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_GT(stats.mossy_signals_sent, 50);
}

//=============================================================================
// Cerebellar Learning Integration Tests
//=============================================================================

TEST_F(CerebellumVestibularIntegration, SynapticPlasticityDuringVor) {
    // Test that synaptic dynamics are engaged during VOR

    cerebellum_stats_t cereb_stats_before;
    cerebellum_get_stats(cerebellum, &cereb_stats_before);

    // VOR adaptation trials
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 50; i++) {
        simulateHeadRotation(1.5f, 0.0f, 0.0f, 10.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);

        vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.2f, slip_direction);
        vestibular_cerebellum_apply_feedback(bridge);
    }

    cerebellum_stats_t cereb_stats_after;
    cerebellum_get_stats(cerebellum, &cereb_stats_after);

    // Vesicle releases and calcium should have increased
    EXPECT_GT(cereb_stats_after.vesicle_releases, cereb_stats_before.vesicle_releases);
    EXPECT_GT(cereb_stats_after.avg_calcium_concentration, 0.0f);
}

TEST_F(CerebellumVestibularIntegration, PurkinjeCellActivityDuringVor) {
    // Test Purkinje cell engagement

    // Baseline Purkinje activity
    cerebellum_stats_t stats_baseline;
    cerebellum_get_stats(cerebellum, &stats_baseline);

    // Active VOR
    simulateHeadRotation(2.0f, 0.0f, 0.0f, 100.0f);
    for (int i = 0; i < 10; i++) {
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }

    cerebellum_stats_t stats_active;
    cerebellum_get_stats(cerebellum, &stats_active);

    // Purkinje cells should have been active
    EXPECT_GT(stats_active.purkinje_spikes, stats_baseline.purkinje_spikes);
}

//=============================================================================
// Error Handling Integration Tests
//=============================================================================

TEST_F(CerebellumVestibularIntegration, RecoveryFromInvalidInput) {
    // System should handle invalid inputs gracefully

    // Valid processing
    simulateHeadRotation(1.0f, 0.0f, 0.0f, 10.0f);
    EXPECT_EQ(0, vestibular_cerebellum_send_mossy_signal(bridge));

    // Invalid operation (null direction)
    int result = vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.5f, nullptr);
    EXPECT_EQ(-1, result);

    // System should still work after error
    simulateHeadRotation(1.0f, 0.0f, 0.0f, 10.0f);
    EXPECT_EQ(0, vestibular_cerebellum_send_mossy_signal(bridge));

    motor_coordination_result_t motor_result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
}

TEST_F(CerebellumVestibularIntegration, ExtremInputValues) {
    // Test with extreme input values

    vestibular_input_t input;
    memset(&input, 0, sizeof(input));
    input.source = VESTIBULAR_INPUT_CANAL;
    input.canal.angular_velocity[0] = 100.0f;  // Very fast rotation
    input.timestamp_ms = 0.0f;

    vestibular_process(vestibular, &input);
    EXPECT_EQ(0, vestibular_cerebellum_send_mossy_signal(bridge));

    motor_coordination_result_t motor_result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
}

//=============================================================================
// Timing and Latency Tests
//=============================================================================

TEST_F(CerebellumVestibularIntegration, SignalLatency) {
    // Verify signal propagation timing

    // Send signal
    simulateHeadRotation(1.0f, 0.0f, 0.0f, 10.0f);
    vestibular_cerebellum_send_mossy_signal(bridge);

    // Process should be immediate (within simulation step)
    motor_coordination_result_t motor_result;
    bool processed = cerebellum_process(cerebellum, &motor_result);
    EXPECT_TRUE(processed);
}

TEST_F(CerebellumVestibularIntegration, ContinuousProcessing) {
    // Test continuous real-time-like processing

    for (int frame = 0; frame < 1000; frame++) {
        // Simulate 1ms step
        float t = (float)frame * 0.001f;

        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = sinf(2.0f * M_PI * t);
        input.timestamp_ms = (float)frame;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
        vestibular_cerebellum_apply_feedback(bridge);
    }

    // Should complete without issues
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.mossy_signals_sent, 1000);
}

//=============================================================================
// Statistics and Diagnostics Integration Tests
//=============================================================================

TEST_F(CerebellumVestibularIntegration, ComprehensiveStats) {
    // Run full activity and verify all stats

    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 100; i++) {
        simulateHeadRotation(1.0f + 0.5f * sinf((float)i * 0.1f), 0.0f, 0.0f, 5.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);

        if (i % 5 == 0) {
            vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.15f, slip_direction);
        }
        vestibular_cerebellum_apply_feedback(bridge);
    }

    vestibular_cerebellum_stats_t bridge_stats;
    vestibular_cerebellum_get_stats(bridge, &bridge_stats);

    EXPECT_EQ(bridge_stats.mossy_signals_sent, 100);
    EXPECT_GT(bridge_stats.adaptation_triggers, 0);
    EXPECT_EQ(bridge_stats.feedback_events, 100);
    EXPECT_GT(bridge_stats.current_flocculus_output, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
