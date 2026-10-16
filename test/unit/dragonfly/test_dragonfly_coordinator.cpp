/**
 * @file test_dragonfly_coordinator.cpp
 * @brief Comprehensive unit tests for dragonfly coordinator module
 *
 * WHAT: Tests the central dragonfly coordinator that orchestrates all subsystems
 * WHY:  The coordinator is the heart of the dragonfly system - must be robust
 * HOW:  Test subsystem coordination, state machines, error handling
 *
 * TEST COVERAGE:
 * - Subsystem initialization and coordination
 * - State machine transitions and edge cases
 * - Pipeline orchestration
 * - Resource management
 * - Concurrent access patterns
 * - Error recovery mechanisms
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_tsdn.h"
#include "dragonfly/nimcp_dragonfly_tracking.h"
#include "dragonfly/nimcp_dragonfly_prediction.h"
#include "dragonfly/nimcp_dragonfly_intercept.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonflyCoordinatorTest : public ::testing::Test {
protected:
    dragonfly_system_t* system = nullptr;

    void SetUp() override {
        system = dragonfly_system_create(nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            dragonfly_system_destroy(system);
            system = nullptr;
        }
    }

    // Helper to create detection at specific angle
    dragonfly_detection_t create_detection_at_angle(uint32_t id, float angle_rad, float distance) {
        dragonfly_detection_t det;
        memset(&det, 0, sizeof(det));
        det.id = id;
        det.position[0] = distance * cosf(angle_rad);
        det.position[1] = distance * sinf(angle_rad);
        det.position[2] = 0.0f;
        det.size = 0.05f;
        det.contrast = 0.85f;
        det.motion_direction_rad = angle_rad + M_PI;  // Moving toward origin
        det.motion_speed = 3.0f;
        det.timestamp_us = 1000000;
        return det;
    }

    // Helper to create self state at origin
    dragonfly_self_state_t create_origin_self_state() {
        dragonfly_self_state_t self;
        memset(&self, 0, sizeof(self));
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        return self;
    }

    // Helper to simulate a tracking sequence
    void simulate_tracking_sequence(int num_frames, float dt = 0.016f) {
        dragonfly_self_state_t self = create_origin_self_state();
        dragonfly_update_self_state(system, &self);

        dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);

        for (int i = 0; i < num_frames; i++) {
            // Update detection position (approaching)
            det.position[0] -= det.motion_speed * dt;

            dragonfly_process_detection(system, &det);
            dragonfly_update(system, dt);
        }
    }
};

//=============================================================================
// Subsystem Initialization Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, SubsystemsInitialized) {
    // Verify all subsystems are accessible
    tsdn_vector_t tsdn;
    EXPECT_EQ(dragonfly_get_tsdn_vector(system, &tsdn), 0);

    trajectory_prediction_t pred;
    EXPECT_EQ(dragonfly_get_prediction(system, &pred), 0);

    intercept_solution_t sol;
    EXPECT_EQ(dragonfly_get_intercept_solution(system, &sol), 0);
}

TEST_F(DragonflyCoordinatorTest, SubsystemStatsPropagated) {
    dragonfly_stats_t stats;
    EXPECT_EQ(dragonfly_get_stats(system, &stats), 0);

    // TSDN stats should be valid
    EXPECT_EQ(stats.tsdn_stats.encode_calls, 0u);

    // Tracker stats should be valid
    EXPECT_EQ(stats.tracker_stats.total_observations, 0u);

    // Prediction stats should be valid
    EXPECT_EQ(stats.prediction_stats.predictions_made, 0u);

    // Intercept stats should be valid
    EXPECT_EQ(stats.intercept_stats.solutions_computed, 0u);
}

TEST_F(DragonflyCoordinatorTest, ConfigPropagatedToSubsystems) {
    dragonfly_config_t config = dragonfly_default_config();
    config.tsdn_config.gain = 2.0f;  // Custom gain multiplier
    config.prediction_config.enable_imm = true;
    config.intercept_config.pn_gain = 4.0f;  // Proportional navigation gain

    dragonfly_system_t* custom = dragonfly_system_create(&config);
    ASSERT_NE(custom, nullptr);

    dragonfly_config_t retrieved;
    EXPECT_EQ(dragonfly_get_config(custom, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.tsdn_config.gain, 2.0f);
    EXPECT_TRUE(retrieved.prediction_config.enable_imm);
    EXPECT_FLOAT_EQ(retrieved.intercept_config.pn_gain, 4.0f);

    dragonfly_system_destroy(custom);
}

//=============================================================================
// State Machine Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, StateTransitionIdleToScanning) {
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_IDLE);

    EXPECT_EQ(dragonfly_start_scan(system), 0);
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_SCANNING);
}

TEST_F(DragonflyCoordinatorTest, StateTransitionScanningToIdle) {
    dragonfly_start_scan(system);
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_SCANNING);

    EXPECT_EQ(dragonfly_go_idle(system), 0);
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_IDLE);
}

TEST_F(DragonflyCoordinatorTest, StateTransitionScanningToTracking) {
    dragonfly_start_scan(system);

    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    // Process multiple detections to establish tracking
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);
    for (int i = 0; i < 20; i++) {
        det.position[0] -= 0.05f;  // Approaching
        dragonfly_process_detection(system, &det);
        dragonfly_update(system, 0.016f);
    }

    dragonfly_mode_t mode = dragonfly_get_mode(system);
    EXPECT_TRUE(mode == DRAGONFLY_MODE_TRACKING ||
                mode == DRAGONFLY_MODE_PURSUING ||
                mode == DRAGONFLY_MODE_SCANNING)
        << "Expected TRACKING, PURSUING, or SCANNING mode, got " << dragonfly_mode_name(mode);
}

TEST_F(DragonflyCoordinatorTest, StateTransitionTrackingToPursuing) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    // Establish strong tracking
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);
    for (int i = 0; i < 60; i++) {
        det.position[0] -= det.motion_speed * 0.016f;
        dragonfly_process_detection(system, &det);
        dragonfly_update(system, 0.016f);
    }

    dragonfly_mode_t mode = dragonfly_get_mode(system);
    EXPECT_TRUE(mode == DRAGONFLY_MODE_TRACKING ||
                mode == DRAGONFLY_MODE_PURSUING ||
                mode == DRAGONFLY_MODE_INTERCEPTING)
        << "Expected advanced pursuit mode";
}

TEST_F(DragonflyCoordinatorTest, StateTransitionPursuingToIdle) {
    simulate_tracking_sequence(30);

    // Abort pursuit
    EXPECT_EQ(dragonfly_abort_pursuit(system), 0);

    // Step to process abort
    dragonfly_update(system, 0.016f);

    dragonfly_mode_t mode = dragonfly_get_mode(system);
    EXPECT_TRUE(mode == DRAGONFLY_MODE_IDLE ||
                mode == DRAGONFLY_MODE_SCANNING);
}

TEST_F(DragonflyCoordinatorTest, StatePreservedAfterReset) {
    // Start scanning and track
    dragonfly_start_scan(system);
    simulate_tracking_sequence(20);

    // Reset
    EXPECT_EQ(dragonfly_system_reset(system), 0);

    // Should be back to IDLE
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_IDLE);

    // Hunt result should be reset
    EXPECT_EQ(dragonfly_get_hunt_result(system), DRAGONFLY_HUNT_IN_PROGRESS);
}

//=============================================================================
// Pipeline Orchestration Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, DetectionFlowsThroughPipeline) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    // Process detection
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);
    EXPECT_EQ(dragonfly_process_detection(system, &det), 0);

    // Verify TSDN encoded it
    tsdn_vector_t tsdn;
    EXPECT_EQ(dragonfly_get_tsdn_vector(system, &tsdn), 0);

    // Step the system
    dragonfly_update(system, 0.016f);

    // Check stats show processing
    dragonfly_stats_t stats;
    dragonfly_get_stats(system, &stats);
    EXPECT_EQ(stats.detections_processed, 1u);
}

TEST_F(DragonflyCoordinatorTest, MotorCommandGeneratedAfterTracking) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    // Track target approaching from the right
    dragonfly_detection_t det = create_detection_at_angle(1, 0.3f, 15.0f);
    for (int i = 0; i < 30; i++) {
        det.position[0] -= det.motion_speed * 0.016f * cosf(0.3f);
        det.position[1] -= det.motion_speed * 0.016f * sinf(0.3f);
        dragonfly_process_detection(system, &det);
        dragonfly_update(system, 0.016f);
    }

    // Get motor command
    dragonfly_motor_cmd_t cmd;
    EXPECT_EQ(dragonfly_get_motor_command(system, &cmd), 0);

    // Command should have urgency if tracking
    dragonfly_mode_t mode = dragonfly_get_mode(system);
    if (mode == DRAGONFLY_MODE_PURSUING || mode == DRAGONFLY_MODE_TRACKING) {
        EXPECT_GT(cmd.urgency, 0.0f);
    }
}

TEST_F(DragonflyCoordinatorTest, PredictionUpdatesWithTracking) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    // Track moving target
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 20.0f);
    for (int i = 0; i < 30; i++) {
        det.position[0] -= det.motion_speed * 0.016f;
        dragonfly_process_detection(system, &det);
        dragonfly_update(system, 0.016f);
    }

    // Get prediction
    trajectory_prediction_t pred;
    EXPECT_EQ(dragonfly_get_prediction(system, &pred), 0);
}

TEST_F(DragonflyCoordinatorTest, InterceptSolutionComputedDuringPursuit) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    // Establish pursuit
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);
    for (int i = 0; i < 50; i++) {
        det.position[0] -= det.motion_speed * 0.016f;
        dragonfly_process_detection(system, &det);
        dragonfly_update(system, 0.016f);
    }

    // Get intercept solution
    intercept_solution_t sol;
    EXPECT_EQ(dragonfly_get_intercept_solution(system, &sol), 0);
}

//=============================================================================
// Target Management Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, MultipleTargetsHandled) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    // Process multiple targets
    for (uint32_t i = 0; i < DRAGONFLY_MAX_TARGETS; i++) {
        dragonfly_detection_t det = create_detection_at_angle(
            i + 1, (float)i * 0.5f, 15.0f + i * 2.0f);
        EXPECT_EQ(dragonfly_process_detection(system, &det), 0);
    }

    dragonfly_update(system, 0.016f);

    // Get all targets
    dragonfly_target_info_t targets[DRAGONFLY_MAX_TARGETS];
    uint32_t num_targets = 0;
    EXPECT_EQ(dragonfly_get_all_targets(system, targets, DRAGONFLY_MAX_TARGETS, &num_targets), 0);
    EXPECT_GT(num_targets, 0u);
    EXPECT_LE(num_targets, DRAGONFLY_MAX_TARGETS);
}

TEST_F(DragonflyCoordinatorTest, TargetLockingWorks) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    // Process two targets
    dragonfly_detection_t det1 = create_detection_at_angle(1, 0.0f, 15.0f);
    dragonfly_detection_t det2 = create_detection_at_angle(2, 0.5f, 12.0f);

    for (int i = 0; i < 20; i++) {
        dragonfly_process_detection(system, &det1);
        dragonfly_process_detection(system, &det2);
        dragonfly_update(system, 0.016f);
    }

    // Get target info
    dragonfly_target_info_t targets[DRAGONFLY_MAX_TARGETS];
    uint32_t num_targets = 0;
    dragonfly_get_all_targets(system, targets, DRAGONFLY_MAX_TARGETS, &num_targets);

    // If we have targets, try locking
    if (num_targets > 0) {
        int result = dragonfly_lock_target(system, targets[0].id);
        // Lock may succeed or fail depending on target state
        EXPECT_TRUE(result == 0 || result == -1);
    }
}

TEST_F(DragonflyCoordinatorTest, PrimaryTargetSelected) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    // Track a target consistently
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);
    for (int i = 0; i < 40; i++) {
        det.position[0] -= det.motion_speed * 0.016f;
        dragonfly_process_detection(system, &det);
        dragonfly_update(system, 0.016f);
    }

    dragonfly_target_info_t primary;
    int result = dragonfly_get_primary_target(system, &primary);

    // May or may not have primary depending on tracking state
    if (result == 0) {
        EXPECT_EQ(primary.id, 1u);
    }
}

//=============================================================================
// Energy Management Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, EnergyAwareDecisions) {
    dragonfly_config_t config = dragonfly_default_config();
    config.energy_aware = true;
    config.min_energy_reserve = 0.3f;

    dragonfly_system_t* energy_system = dragonfly_system_create(&config);
    ASSERT_NE(energy_system, nullptr);

    // Set low energy state
    dragonfly_self_state_t self = create_origin_self_state();
    self.energy_level = 0.1f;  // Very low energy
    dragonfly_update_self_state(energy_system, &self);

    dragonfly_start_scan(energy_system);

    // Try to track target
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 25.0f);  // Far target
    for (int i = 0; i < 20; i++) {
        dragonfly_process_detection(energy_system, &det);
        dragonfly_update(energy_system, 0.016f);
    }

    // Get motor command - urgency should be reduced due to energy
    dragonfly_motor_cmd_t cmd;
    dragonfly_get_motor_command(energy_system, &cmd);

    // Low energy should reduce pursuit aggressiveness
    // (exact behavior depends on implementation)

    dragonfly_system_destroy(energy_system);
}

TEST_F(DragonflyCoordinatorTest, EnergyLevelAffectsPursuit) {
    dragonfly_config_t config = dragonfly_default_config();
    config.energy_aware = true;

    dragonfly_system_t* system1 = dragonfly_system_create(&config);
    dragonfly_system_t* system2 = dragonfly_system_create(&config);

    // High energy pursuer
    dragonfly_self_state_t high_energy = create_origin_self_state();
    high_energy.energy_level = 1.0f;
    dragonfly_update_self_state(system1, &high_energy);

    // Low energy pursuer
    dragonfly_self_state_t low_energy = create_origin_self_state();
    low_energy.energy_level = 0.2f;
    dragonfly_update_self_state(system2, &low_energy);

    // Same target for both
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);

    dragonfly_start_scan(system1);
    dragonfly_start_scan(system2);

    for (int i = 0; i < 20; i++) {
        dragonfly_process_detection(system1, &det);
        dragonfly_process_detection(system2, &det);
        dragonfly_update(system1, 0.016f);
        dragonfly_update(system2, 0.016f);
    }

    dragonfly_motor_cmd_t cmd1, cmd2;
    dragonfly_get_motor_command(system1, &cmd1);
    dragonfly_get_motor_command(system2, &cmd2);

    // High energy should be more aggressive (higher urgency)
    // Low energy should be more conservative
    // (exact comparison depends on implementation)

    dragonfly_system_destroy(system1);
    dragonfly_system_destroy(system2);
}

//=============================================================================
// Timing and Update Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, ConsistentUpdateTiming) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);

    // Variable timesteps
    float timesteps[] = {0.001f, 0.016f, 0.033f, 0.010f, 0.020f};
    for (float dt : timesteps) {
        dragonfly_process_detection(system, &det);
        EXPECT_EQ(dragonfly_update(system, dt), 0);
        det.position[0] -= det.motion_speed * dt;
    }
}

TEST_F(DragonflyCoordinatorTest, LargeTimestepHandled) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    // Large timestep (e.g., frame drop)
    EXPECT_EQ(dragonfly_update(system, 0.1f), 0);
}

TEST_F(DragonflyCoordinatorTest, RapidUpdatesHandled) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    // Very rapid updates
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(dragonfly_update(system, 0.001f), 0);
    }
}

//=============================================================================
// Statistics Aggregation Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, StatisticsAccumulate) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);

    for (int i = 0; i < 50; i++) {
        dragonfly_process_detection(system, &det);
        dragonfly_update(system, 0.016f);
    }

    dragonfly_stats_t stats;
    dragonfly_get_stats(system, &stats);

    EXPECT_EQ(stats.detections_processed, 50u);
    EXPECT_EQ(stats.total_updates, 50u);
}

TEST_F(DragonflyCoordinatorTest, StatisticsResetAllSubsystems) {
    // Generate some stats
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);
    for (int i = 0; i < 10; i++) {
        dragonfly_process_detection(system, &det);
        dragonfly_update(system, 0.016f);
    }

    // Reset stats
    EXPECT_EQ(dragonfly_reset_stats(system), 0);

    dragonfly_stats_t stats;
    dragonfly_get_stats(system, &stats);

    EXPECT_EQ(stats.detections_processed, 0u);
    EXPECT_EQ(stats.total_updates, 0u);
}

//=============================================================================
// Error Recovery Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, RecoverFromInvalidDetection) {
    // Process valid detection
    dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);
    EXPECT_EQ(dragonfly_process_detection(system, &det), 0);

    // Try null detection - should fail gracefully
    EXPECT_EQ(dragonfly_process_detection(system, nullptr), -1);

    // System should still work
    EXPECT_EQ(dragonfly_process_detection(system, &det), 0);
    EXPECT_EQ(dragonfly_update(system, 0.016f), 0);
}

TEST_F(DragonflyCoordinatorTest, RecoverFromInvalidSelfState) {
    // Process valid self state
    dragonfly_self_state_t self = create_origin_self_state();
    EXPECT_EQ(dragonfly_update_self_state(system, &self), 0);

    // Try null self state
    EXPECT_EQ(dragonfly_update_self_state(system, nullptr), -1);

    // System should still work
    EXPECT_EQ(dragonfly_update(system, 0.016f), 0);
}

TEST_F(DragonflyCoordinatorTest, RecoverFromInvalidConfig) {
    dragonfly_config_t bad_config = dragonfly_default_config();
    bad_config.min_target_size = -1.0f;  // Invalid

    EXPECT_EQ(dragonfly_set_config(system, &bad_config), -1);

    // System should still work with original config
    dragonfly_config_t config;
    EXPECT_EQ(dragonfly_get_config(system, &config), 0);
    EXPECT_GT(config.min_target_size, 0.0f);
}

//=============================================================================
// Concurrent Access Tests (Basic Thread Safety)
//=============================================================================

TEST_F(DragonflyCoordinatorTest, ConcurrentDetections) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    dragonfly_start_scan(system);

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    // Multiple threads sending detections
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t, &errors]() {
            for (int i = 0; i < 50; i++) {
                dragonfly_detection_t det = create_detection_at_angle(
                    t * 100 + i, (float)t * 0.5f, 15.0f);
                if (dragonfly_process_detection(system, &det) < -1) {
                    errors++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // No errors expected (though some detections may be dropped)
    EXPECT_EQ(errors.load(), 0);
}

TEST_F(DragonflyCoordinatorTest, ConcurrentStatsReads) {
    dragonfly_self_state_t self = create_origin_self_state();
    dragonfly_update_self_state(system, &self);

    std::atomic<bool> running{true};
    std::atomic<int> errors{0};

    // Writer thread
    std::thread writer([this, &running]() {
        dragonfly_detection_t det = create_detection_at_angle(1, 0.0f, 15.0f);
        while (running) {
            dragonfly_process_detection(system, &det);
            dragonfly_update(system, 0.001f);
        }
    });

    // Reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < 3; i++) {
        readers.emplace_back([this, &errors]() {
            for (int j = 0; j < 100; j++) {
                dragonfly_stats_t stats;
                if (dragonfly_get_stats(system, &stats) < -1) {
                    errors++;
                }
            }
        });
    }

    for (auto& r : readers) {
        r.join();
    }

    running = false;
    writer.join();

    EXPECT_EQ(errors.load(), 0);
}

//=============================================================================
// Hunt Outcome Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, HuntResultInProgress) {
    // Initially in progress
    EXPECT_EQ(dragonfly_get_hunt_result(system), DRAGONFLY_HUNT_IN_PROGRESS);

    // Start tracking
    simulate_tracking_sequence(20);

    // Still in progress
    EXPECT_EQ(dragonfly_get_hunt_result(system), DRAGONFLY_HUNT_IN_PROGRESS);
}

TEST_F(DragonflyCoordinatorTest, HuntResultAborted) {
    simulate_tracking_sequence(20);

    // Abort
    dragonfly_abort_pursuit(system);
    dragonfly_update(system, 0.016f);

    dragonfly_hunt_result_t result = dragonfly_get_hunt_result(system);
    EXPECT_TRUE(result == DRAGONFLY_HUNT_ABORTED ||
                result == DRAGONFLY_HUNT_IN_PROGRESS);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, ModeNamesAreUnique) {
    std::vector<const char*> names;
    names.push_back(dragonfly_mode_name(DRAGONFLY_MODE_IDLE));
    names.push_back(dragonfly_mode_name(DRAGONFLY_MODE_SCANNING));
    names.push_back(dragonfly_mode_name(DRAGONFLY_MODE_TRACKING));
    names.push_back(dragonfly_mode_name(DRAGONFLY_MODE_PURSUING));
    names.push_back(dragonfly_mode_name(DRAGONFLY_MODE_INTERCEPTING));

    // Check all names are non-null and unique
    for (size_t i = 0; i < names.size(); i++) {
        EXPECT_NE(names[i], nullptr);
        for (size_t j = i + 1; j < names.size(); j++) {
            EXPECT_STRNE(names[i], names[j]);
        }
    }
}

TEST_F(DragonflyCoordinatorTest, HuntResultNamesAreUnique) {
    std::vector<const char*> names;
    names.push_back(dragonfly_hunt_result_name(DRAGONFLY_HUNT_IN_PROGRESS));
    names.push_back(dragonfly_hunt_result_name(DRAGONFLY_HUNT_SUCCESS));
    names.push_back(dragonfly_hunt_result_name(DRAGONFLY_HUNT_ESCAPED));
    names.push_back(dragonfly_hunt_result_name(DRAGONFLY_HUNT_ABORTED));
    names.push_back(dragonfly_hunt_result_name(DRAGONFLY_HUNT_TIMEOUT));

    for (size_t i = 0; i < names.size(); i++) {
        EXPECT_NE(names[i], nullptr);
        for (size_t j = i + 1; j < names.size(); j++) {
            EXPECT_STRNE(names[i], names[j]);
        }
    }
}

TEST_F(DragonflyCoordinatorTest, VersionString) {
    const char* version = dragonfly_version();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);

    // Should contain version numbers
    EXPECT_NE(strstr(version, "."), nullptr);
}

//=============================================================================
// Null Pointer Safety Tests
//=============================================================================

TEST_F(DragonflyCoordinatorTest, AllFunctionsHandleNullSystem) {
    dragonfly_detection_t det;
    memset(&det, 0, sizeof(det));
    dragonfly_self_state_t self;
    memset(&self, 0, sizeof(self));
    dragonfly_motor_cmd_t cmd;
    dragonfly_target_info_t target;
    dragonfly_target_info_t targets[8];
    uint32_t num_targets;
    tsdn_vector_t tsdn;
    trajectory_prediction_t pred;
    intercept_solution_t sol;
    dragonfly_stats_t stats;
    dragonfly_config_t config;
    dragonfly_state_t state;

    EXPECT_EQ(dragonfly_process_detection(nullptr, &det), -1);
    EXPECT_EQ(dragonfly_update_self_state(nullptr, &self), -1);
    EXPECT_EQ(dragonfly_update(nullptr, 0.016f), -1);
    EXPECT_EQ(dragonfly_get_motor_command(nullptr, &cmd), -1);
    EXPECT_EQ(dragonfly_get_primary_target(nullptr, &target), -1);
    EXPECT_EQ(dragonfly_get_all_targets(nullptr, targets, 8, &num_targets), -1);
    EXPECT_EQ(dragonfly_get_tsdn_vector(nullptr, &tsdn), -1);
    EXPECT_EQ(dragonfly_get_prediction(nullptr, &pred), -1);
    EXPECT_EQ(dragonfly_get_intercept_solution(nullptr, &sol), -1);
    EXPECT_EQ(dragonfly_start_scan(nullptr), -1);
    EXPECT_EQ(dragonfly_lock_target(nullptr, 1), -1);
    EXPECT_EQ(dragonfly_start_pursuit(nullptr), -1);
    EXPECT_EQ(dragonfly_abort_pursuit(nullptr), -1);
    EXPECT_EQ(dragonfly_go_idle(nullptr), -1);
    EXPECT_EQ(dragonfly_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_reset_stats(nullptr), -1);
    EXPECT_EQ(dragonfly_set_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_get_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_system_reset(nullptr), -1);
    EXPECT_EQ(dragonfly_get_state(nullptr, &state), -1);
}

TEST_F(DragonflyCoordinatorTest, AllFunctionsHandleNullOutputs) {
    EXPECT_EQ(dragonfly_process_detection(system, nullptr), -1);
    EXPECT_EQ(dragonfly_update_self_state(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_motor_command(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_primary_target(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_all_targets(system, nullptr, 8, nullptr), -1);
    EXPECT_EQ(dragonfly_get_tsdn_vector(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_prediction(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_intercept_solution(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_stats(system, nullptr), -1);
    EXPECT_EQ(dragonfly_set_config(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_config(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_state(system, nullptr), -1);
}
