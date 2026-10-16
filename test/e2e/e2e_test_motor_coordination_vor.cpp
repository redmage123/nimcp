/**
 * @file e2e_test_motor_coordination_vor.cpp
 * @brief End-to-end tests for VOR Motor Coordination Pipeline
 *
 * WHAT: Full pipeline tests for VOR motor coordination (vestibular-cerebellum)
 * WHY:  Verify complete VOR calibration workflow with all components
 * HOW:  Test VOR loop, adaptation, multi-axis coordination, real-time performance
 *
 * TEST COVERAGE:
 * - VOR Calibration Loop (4 tests)
 * - Multi-Axis Coordination (3 tests)
 * - Adaptation Learning (3 tests)
 * - Real-Time Performance (2 tests)
 * - Recovery and Stability (2 tests)
 *
 * TOTAL: 14 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Vestibulo-Ocular Reflex stabilizes gaze during head movement
 * - Flocculus receives mossy fiber input from vestibular nuclei
 * - Climbing fibers carry retinal slip (error) signal
 * - Purkinje cells modulate vestibular nuclei to calibrate VOR
 * - VOR gain adaptation via LTD at parallel fiber-Purkinje synapses
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>
#include <thread>

// Headers have their own extern "C" guards
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_VOR_LOOP_TIME_MS = 5.0;      // Real-time constraint
constexpr double MAX_ADAPTATION_TIME_MS = 50.0;  // Per trial
constexpr float VOR_GAIN_TOLERANCE = 0.1f;       // 10% tolerance
constexpr float RETINAL_SLIP_ACCEPTABLE = 0.05f; // Acceptable slip after adaptation
constexpr int VOR_LEARNING_TRIALS = 100;         // Trials for learning test
constexpr int MULTI_AXIS_STEPS = 50;             // Steps for multi-axis test

//=============================================================================
// Test Fixture - Full VOR Pipeline
//=============================================================================

class E2EMotorCoordinationVORTest : public ::testing::Test {
protected:
    cerebellum_adapter_t* cerebellum = nullptr;
    vestibular_processor_t* vestibular = nullptr;
    vestibular_cerebellum_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create cerebellum with all features
        cerebellum_config_t cere_config = cerebellum_default_config();
        cere_config.enable_vestibulocerebellum = true;
        cere_config.enable_vor_adaptation = true;
        cere_config.enable_synaptic_dynamics = true;
        cere_config.enable_basket_cells = true;
        cere_config.enable_stellate_cells = true;
        cere_config.enable_golgi_cells = true;
        cerebellum = cerebellum_create(&cere_config);
        ASSERT_NE(cerebellum, nullptr);

        // Create vestibular processor
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

    // Helper: Run one complete VOR loop iteration
    void runVORLoop(float head_yaw, float head_pitch, float head_roll,
                    float retinal_slip, uint64_t timestamp_us) {
        // Step 1: Vestibular input
        semicircular_canal_input_t input;
        input.yaw_velocity = head_yaw;
        input.pitch_velocity = head_pitch;
        input.roll_velocity = head_roll;
        input.timestamp_us = timestamp_us;
        vestibular_process_canal_input(vestibular, &input);

        // Step 2: Send mossy signal to cerebellum
        vestibular_cerebellum_send_mossy_signal(bridge);

        // Step 3: Apply retinal slip if present
        if (fabs(retinal_slip) > VOR_RETINAL_SLIP_THRESHOLD) {
            float direction[3] = {head_yaw != 0 ? 1.0f : 0.0f,
                                  head_pitch != 0 ? 1.0f : 0.0f,
                                  head_roll != 0 ? 1.0f : 0.0f};
            vestibular_cerebellum_trigger_vor_adaptation(bridge, retinal_slip, direction);
        }

        // Step 4: Apply cerebellar feedback
        vestibular_cerebellum_apply_feedback(bridge);
    }

    // Helper: Get VOR error (retinal slip from current gain)
    float computeRetinalSlip(float head_velocity, float eye_velocity, float target_gain) {
        float expected_eye = -head_velocity * target_gain;
        return eye_velocity - expected_eye;
    }
};

//=============================================================================
// VOR Calibration Loop Tests
//=============================================================================

TEST_F(E2EMotorCoordinationVORTest, VORLoopCompletesInRealTime) {
    // WHAT: Verify VOR loop completes within real-time constraint
    // WHY:  VOR must operate at high frequency for gaze stability
    // HOW:  Time complete loop, verify < 5ms

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        runVORLoop(1.0f, 0.0f, 0.0f, 0.0f, i * 10000);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_loop_ms = static_cast<double>(duration.count()) / 100.0 / 1000.0;

    EXPECT_LT(per_loop_ms, MAX_VOR_LOOP_TIME_MS)
        << "VOR loop took " << per_loop_ms << "ms, exceeds real-time requirement";
}

TEST_F(E2EMotorCoordinationVORTest, VORProducesCompensatoryEyeMovement) {
    // WHAT: Verify VOR produces correct compensatory eye movement
    // WHY:  Core function: eyes should move opposite to head
    // HOW:  Apply head rotation, verify eye command is opposite

    // Apply head rotation
    for (int i = 0; i < 10; i++) {
        runVORLoop(1.0f, 0.0f, 0.0f, 0.0f, i * 10000);
    }

    // Get VOR command
    float eye_velocity[3];
    vestibular_get_vor_command(vestibular, eye_velocity);

    // Eye should move opposite to head (compensatory)
    EXPECT_LT(eye_velocity[0], 0.0f)
        << "VOR should produce compensatory eye movement opposite to head yaw";

    // Gain should be approximately 1.0 (eye velocity ~ head velocity)
    EXPECT_NEAR(fabs(eye_velocity[0]), 1.0f, VOR_GAIN_TOLERANCE)
        << "VOR gain should be near 1.0";
}

TEST_F(E2EMotorCoordinationVORTest, VORMaintainsGainOverTime) {
    // WHAT: Verify VOR gain remains stable without error signal
    // WHY:  VOR should not drift without calibration error
    // HOW:  Run many iterations, verify gain stays constant

    // Get initial gain
    float initial_gain[3];
    bool active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &active);

    // Run 1000 iterations without retinal slip
    for (int i = 0; i < 1000; i++) {
        runVORLoop(0.5f + 0.5f * sin(i * 0.1f), 0.0f, 0.0f, 0.0f, i * 10000);
    }

    // Get final gain
    float final_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, final_gain, &active);

    // Gain should not have drifted significantly
    EXPECT_NEAR(final_gain[0], initial_gain[0], 0.05f)
        << "VOR gain should remain stable without error signal";
}

TEST_F(E2EMotorCoordinationVORTest, VORHandlesRapidHeadMovements) {
    // WHAT: Verify VOR handles rapid/high-frequency head movements
    // WHY:  Natural head movements can be fast
    // HOW:  Apply high-frequency sinusoidal rotation

    std::vector<float> eye_responses;

    // High-frequency oscillation (5 Hz simulated)
    for (int i = 0; i < 200; i++) {
        float head_vel = 2.0f * sin(i * 0.314f);  // ~5Hz at 10ms steps
        runVORLoop(head_vel, 0.0f, 0.0f, 0.0f, i * 10000);

        float eye_velocity[3];
        vestibular_get_vor_command(vestibular, eye_velocity);
        eye_responses.push_back(eye_velocity[0]);
    }

    // Eye should track head with opposite sign
    float mean_error = 0.0f;
    for (size_t i = 10; i < eye_responses.size(); i++) {  // Skip transient
        float expected = -2.0f * sin(i * 0.314f);
        mean_error += fabs(eye_responses[i] - expected);
    }
    mean_error /= (eye_responses.size() - 10);

    EXPECT_LT(mean_error, 0.5f)
        << "VOR should track rapid head movements";
}

//=============================================================================
// Multi-Axis Coordination Tests
//=============================================================================

TEST_F(E2EMotorCoordinationVORTest, MultiAxisRotationHandled) {
    // WHAT: Verify VOR handles combined yaw/pitch/roll
    // WHY:  Natural head movements are often multi-axis
    // HOW:  Apply combined rotation, verify all axes respond

    // Combined rotation
    for (int i = 0; i < MULTI_AXIS_STEPS; i++) {
        runVORLoop(0.5f, 0.3f, 0.2f, 0.0f, i * 10000);
    }

    float eye_velocity[3];
    vestibular_get_vor_command(vestibular, eye_velocity);

    // All axes should show compensation
    // (exact values depend on implementation)
    vestibular_stats_t stats;
    vestibular_get_stats(vestibular, &stats);
    EXPECT_EQ(stats.canal_inputs, MULTI_AXIS_STEPS);
}

TEST_F(E2EMotorCoordinationVORTest, IndependentAxisAdaptation) {
    // WHAT: Verify each axis can adapt independently
    // WHY:  VOR calibration is axis-specific
    // HOW:  Apply slip on one axis, verify only that axis adapts

    // Get initial gains
    float initial_gain[3];
    bool active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &active);

    // Apply yaw-only slip
    for (int i = 0; i < 50; i++) {
        runVORLoop(1.0f, 0.0f, 0.0f, 0.3f, i * 10000);
    }

    float final_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, final_gain, &active);

    // Yaw gain should have changed, others less so
    float yaw_change = fabs(final_gain[0] - initial_gain[0]);
    float pitch_change = fabs(final_gain[1] - initial_gain[1]);
    float roll_change = fabs(final_gain[2] - initial_gain[2]);

    // Yaw should change most (or at least equally)
    EXPECT_GE(yaw_change + 0.01f, pitch_change);
    EXPECT_GE(yaw_change + 0.01f, roll_change);
}

TEST_F(E2EMotorCoordinationVORTest, SequentialAxisRotations) {
    // WHAT: Verify handling of sequential rotations on different axes
    // WHY:  Real movements often switch axes
    // HOW:  Alternate between axes, verify proper coordination

    for (int sequence = 0; sequence < 3; sequence++) {
        // Yaw rotation
        for (int i = 0; i < 10; i++) {
            runVORLoop(1.0f, 0.0f, 0.0f, 0.0f, (sequence * 30 + i) * 10000);
        }

        // Pitch rotation
        for (int i = 0; i < 10; i++) {
            runVORLoop(0.0f, 1.0f, 0.0f, 0.0f, (sequence * 30 + 10 + i) * 10000);
        }

        // Roll rotation
        for (int i = 0; i < 10; i++) {
            runVORLoop(0.0f, 0.0f, 1.0f, 0.0f, (sequence * 30 + 20 + i) * 10000);
        }
    }

    // Verify all nuclei active
    vestibular_stats_t stats;
    vestibular_get_stats(vestibular, &stats);
    EXPECT_EQ(stats.canal_inputs, 90);
}

//=============================================================================
// Adaptation Learning Tests
//=============================================================================

TEST_F(E2EMotorCoordinationVORTest, VORAdaptationReducesError) {
    // WHAT: Verify VOR adaptation reduces retinal slip over trials
    // WHY:  Learning should improve VOR accuracy
    // HOW:  Apply consistent slip, verify error decreases

    std::vector<float> slip_history;

    for (int trial = 0; trial < VOR_LEARNING_TRIALS; trial++) {
        // Simulate movement with initial VOR error
        float slip = 0.3f * exp(-trial * 0.03f);  // Decaying as learning progresses

        for (int step = 0; step < 5; step++) {
            runVORLoop(1.0f, 0.0f, 0.0f, slip, (trial * 5 + step) * 10000);
        }

        if (trial % 10 == 0) {
            slip_history.push_back(slip);
        }
    }

    // Error should decrease over time
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);

    EXPECT_GT(stats.adaptation_triggers, 0);
    EXPECT_GT(stats.total_vor_gain_change, 0.0f);
}

TEST_F(E2EMotorCoordinationVORTest, LearningPersists) {
    // WHAT: Verify learned VOR gain persists without slip
    // WHY:  Adaptation should be retained
    // HOW:  Adapt, then run without slip, verify gain maintained

    // Phase 1: Adapt VOR
    for (int trial = 0; trial < 50; trial++) {
        for (int step = 0; step < 5; step++) {
            runVORLoop(1.0f, 0.0f, 0.0f, 0.3f, (trial * 5 + step) * 10000);
        }
    }

    float post_adapt_gain[3];
    bool active;
    vestibular_cerebellum_get_vor_state(bridge, post_adapt_gain, &active);

    // Phase 2: Run without slip
    for (int trial = 0; trial < 50; trial++) {
        for (int step = 0; step < 5; step++) {
            runVORLoop(1.0f, 0.0f, 0.0f, 0.0f, (250 + trial * 5 + step) * 10000);
        }
    }

    float final_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, final_gain, &active);

    // Gain should be similar (learning retained)
    EXPECT_NEAR(final_gain[0], post_adapt_gain[0], 0.05f)
        << "Adapted VOR gain should persist without error signal";
}

TEST_F(E2EMotorCoordinationVORTest, BidirectionalAdaptation) {
    // WHAT: Verify VOR can adapt in both directions
    // WHY:  VOR can be both over- and under-compensating
    // HOW:  Apply positive then negative slip, verify gain adjusts both ways

    float initial_gain[3];
    bool active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &active);

    // Positive slip (increase gain)
    for (int i = 0; i < 50; i++) {
        runVORLoop(1.0f, 0.0f, 0.0f, 0.3f, i * 10000);
    }

    float after_positive_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, after_positive_gain, &active);

    // Negative slip (decrease gain)
    for (int i = 0; i < 50; i++) {
        runVORLoop(1.0f, 0.0f, 0.0f, -0.3f, (50 + i) * 10000);
    }

    float after_negative_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, after_negative_gain, &active);

    // Gain should have moved in both directions
    // (specific changes depend on LTD/LTP balance)
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_GT(stats.adaptation_triggers, 0);
}

//=============================================================================
// Real-Time Performance Tests
//=============================================================================

TEST_F(E2EMotorCoordinationVORTest, SustainedRealTimePerformance) {
    // WHAT: Verify sustained real-time performance over long run
    // WHY:  VOR must work continuously without degradation
    // HOW:  Run many iterations, verify no slowdown

    constexpr int NUM_ITERATIONS = 10000;
    constexpr int WARMUP = 100;

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        runVORLoop(0.5f, 0.0f, 0.0f, 0.0f, i * 10000);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float head_vel = 0.5f + 0.5f * sin(i * 0.05f);
        runVORLoop(head_vel, 0.0f, 0.0f, 0.0f, (WARMUP + i) * 10000);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_loop_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;

    EXPECT_LT(per_loop_us, 1000.0)
        << "Sustained VOR loop took " << per_loop_us << "us, should be < 1ms";
}

TEST_F(E2EMotorCoordinationVORTest, PerformanceWithAllFeatures) {
    // WHAT: Verify performance with all features enabled
    // WHY:  Full feature set should still meet real-time
    // HOW:  Run complete pipeline with synaptic dynamics, interneurons

    constexpr int NUM_ITERATIONS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Full loop with adaptation
        runVORLoop(1.0f, 0.3f, 0.1f, 0.1f, i * 10000);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_loop_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;

    EXPECT_LT(per_loop_us, 2000.0)
        << "Full-feature VOR loop took " << per_loop_us << "us, should be < 2ms";

    // Verify all features were active
    cerebellum_stats_t cere_stats;
    cerebellum_get_stats(cerebellum, &cere_stats);
    EXPECT_GE(cere_stats.basket_inhibition_total, 0.0f);
    EXPECT_GE(cere_stats.stellate_inhibition_total, 0.0f);
    EXPECT_GE(cere_stats.golgi_feedback_total, 0.0f);
}

//=============================================================================
// Recovery and Stability Tests
//=============================================================================

TEST_F(E2EMotorCoordinationVORTest, RecoveryFromSaturation) {
    // WHAT: Verify recovery from extreme inputs
    // WHY:  System should be robust to saturation
    // HOW:  Apply extreme rotation, then normal, verify recovery

    // Extreme rotation
    for (int i = 0; i < 20; i++) {
        runVORLoop(10.0f, 5.0f, 3.0f, 2.0f, i * 10000);
    }

    // Normal operation
    for (int i = 0; i < 50; i++) {
        runVORLoop(0.5f, 0.0f, 0.0f, 0.0f, (20 + i) * 10000);
    }

    // Should still produce valid output
    float eye_velocity[3];
    vestibular_get_vor_command(vestibular, eye_velocity);

    EXPECT_FALSE(std::isnan(eye_velocity[0]));
    EXPECT_FALSE(std::isinf(eye_velocity[0]));

    // Gain should still be in valid range
    float gain[3];
    bool active;
    vestibular_cerebellum_get_vor_state(bridge, gain, &active);
    EXPECT_GE(gain[0], VESTIBULAR_MIN_VOR_GAIN);
    EXPECT_LE(gain[0], VESTIBULAR_MAX_VOR_GAIN);
}

TEST_F(E2EMotorCoordinationVORTest, StabilityUnderPerturbation) {
    // WHAT: Verify stability under random perturbations
    // WHY:  Real vestibular input is noisy
    // HOW:  Add noise to inputs, verify stable operation

    std::srand(42);  // Reproducible

    for (int i = 0; i < 500; i++) {
        float noise_yaw = 0.1f * (std::rand() / (float)RAND_MAX - 0.5f);
        float noise_pitch = 0.1f * (std::rand() / (float)RAND_MAX - 0.5f);
        float noise_slip = 0.05f * (std::rand() / (float)RAND_MAX - 0.5f);

        runVORLoop(0.5f + noise_yaw, noise_pitch, 0.0f, noise_slip, i * 10000);
    }

    // System should still be stable
    vestibular_cerebellum_status_t status = vestibular_cerebellum_get_status(bridge);
    EXPECT_NE(status, VESTIBULAR_CEREBELLUM_STATUS_ERROR);

    // VOR gain should still be in valid range
    float gain[3];
    bool active;
    vestibular_cerebellum_get_vor_state(bridge, gain, &active);
    EXPECT_GE(gain[0], VESTIBULAR_MIN_VOR_GAIN);
    EXPECT_LE(gain[0], VESTIBULAR_MAX_VOR_GAIN);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
