/**
 * @file test_dragonfly_system.cpp
 * @brief Unit tests for main dragonfly coordinator
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly.h"

class DragonflySystemTest : public ::testing::Test {
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

    // Helper to create self state
    dragonfly_self_state_t create_self_state(float x, float y, float z) {
        dragonfly_self_state_t self;
        memset(&self, 0, sizeof(self));
        self.position[0] = x;
        self.position[1] = y;
        self.position[2] = z;
        self.max_speed = 10.0f;
        self.max_accel = 5.0f;
        self.max_turn_rate = 3.14f;
        self.energy_level = 1.0f;
        return self;
    }

    // Helper to create detection
    dragonfly_detection_t create_detection(uint32_t id, float x, float y, float z) {
        dragonfly_detection_t det;
        memset(&det, 0, sizeof(det));
        det.id = id;
        det.position[0] = x;
        det.position[1] = y;
        det.position[2] = z;
        det.size = 0.05f;
        det.contrast = 0.8f;
        det.motion_direction_rad = 0.0f;
        det.motion_speed = 2.0f;
        det.timestamp_us = 1000000;
        return det;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonflySystemTest, DefaultConfig) {
    dragonfly_config_t config = dragonfly_default_config();

    EXPECT_GT(config.min_target_size, 0.0f);
    EXPECT_GT(config.max_target_distance, 0.0f);
    EXPECT_GT(config.abort_distance, config.max_target_distance);
    EXPECT_GT(config.intercept_threshold, 0.0f);
    EXPECT_GT(config.pursuit_timeout_s, 0.0f);
}

TEST_F(DragonflySystemTest, ValidateConfig) {
    dragonfly_config_t config = dragonfly_default_config();
    EXPECT_TRUE(dragonfly_validate_config(&config));

    // Invalid: null
    EXPECT_FALSE(dragonfly_validate_config(nullptr));

    // Invalid: negative min_target_size
    config = dragonfly_default_config();
    config.min_target_size = -1.0f;
    EXPECT_FALSE(dragonfly_validate_config(&config));

    // Invalid: abort_distance <= max_target_distance
    config = dragonfly_default_config();
    config.abort_distance = config.max_target_distance;
    EXPECT_FALSE(dragonfly_validate_config(&config));
}

TEST_F(DragonflySystemTest, CreateWithCustomConfig) {
    dragonfly_config_t config = dragonfly_default_config();
    config.pursuit_timeout_s = 60.0f;
    config.intercept_threshold = 1.0f;

    dragonfly_system_t* custom = dragonfly_system_create(&config);
    ASSERT_NE(custom, nullptr);

    dragonfly_config_t retrieved;
    EXPECT_EQ(dragonfly_get_config(custom, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.pursuit_timeout_s, 60.0f);
    EXPECT_FLOAT_EQ(retrieved.intercept_threshold, 1.0f);

    dragonfly_system_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonflySystemTest, CreateAndDestroy) {
    dragonfly_system_t* sys = dragonfly_system_create(nullptr);
    ASSERT_NE(sys, nullptr);
    dragonfly_system_destroy(sys);
}

TEST_F(DragonflySystemTest, DestroyNull) {
    dragonfly_system_destroy(nullptr);  // Should not crash
}

TEST_F(DragonflySystemTest, Reset) {
    // Process some data first
    dragonfly_detection_t det = create_detection(1, 10.0f, 5.0f, 0.0f);
    dragonfly_process_detection(system, &det);

    // Reset
    EXPECT_EQ(dragonfly_system_reset(system), 0);

    // Should be idle
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_IDLE);
}

//=============================================================================
// Mode and State Tests
//=============================================================================

TEST_F(DragonflySystemTest, InitialMode) {
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_IDLE);
}

TEST_F(DragonflySystemTest, StartScan) {
    EXPECT_EQ(dragonfly_start_scan(system), 0);
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_SCANNING);
}

TEST_F(DragonflySystemTest, GoIdle) {
    dragonfly_start_scan(system);
    EXPECT_EQ(dragonfly_go_idle(system), 0);
    EXPECT_EQ(dragonfly_get_mode(system), DRAGONFLY_MODE_IDLE);
}

TEST_F(DragonflySystemTest, HuntResultInitial) {
    EXPECT_EQ(dragonfly_get_hunt_result(system), DRAGONFLY_HUNT_IN_PROGRESS);
}

//=============================================================================
// Detection Processing Tests
//=============================================================================

TEST_F(DragonflySystemTest, ProcessDetection) {
    dragonfly_detection_t det = create_detection(1, 10.0f, 5.0f, 0.0f);
    EXPECT_EQ(dragonfly_process_detection(system, &det), 0);
}

TEST_F(DragonflySystemTest, ProcessMultipleDetections) {
    for (uint32_t i = 0; i < 5; i++) {
        dragonfly_detection_t det = create_detection(i, 10.0f + i, 5.0f, 0.0f);
        EXPECT_EQ(dragonfly_process_detection(system, &det), 0);
    }
}

TEST_F(DragonflySystemTest, FilterSmallTargets) {
    dragonfly_detection_t det = create_detection(1, 10.0f, 5.0f, 0.0f);
    det.size = 0.001f;  // Too small

    dragonfly_stats_t stats_before, stats_after;
    dragonfly_get_stats(system, &stats_before);

    dragonfly_process_detection(system, &det);

    dragonfly_get_stats(system, &stats_after);
    // Should still count as processed even if filtered
    EXPECT_EQ(stats_after.detections_processed, stats_before.detections_processed + 1);
}

TEST_F(DragonflySystemTest, FilterDistantTargets) {
    dragonfly_detection_t det = create_detection(1, 1000.0f, 500.0f, 0.0f);  // Very far

    // Should not crash, just filter
    EXPECT_EQ(dragonfly_process_detection(system, &det), 0);
}

//=============================================================================
// Self State Tests
//=============================================================================

TEST_F(DragonflySystemTest, UpdateSelfState) {
    dragonfly_self_state_t self = create_self_state(0.0f, 0.0f, 0.0f);
    EXPECT_EQ(dragonfly_update_self_state(system, &self), 0);
}

TEST_F(DragonflySystemTest, UpdateNullSelfState) {
    EXPECT_EQ(dragonfly_update_self_state(system, nullptr), -1);
}

//=============================================================================
// Update Cycle Tests
//=============================================================================

TEST_F(DragonflySystemTest, UpdateCycle) {
    EXPECT_EQ(dragonfly_update(system, 0.01f), 0);
}

TEST_F(DragonflySystemTest, UpdateInvalidDt) {
    EXPECT_EQ(dragonfly_update(system, 0.0f), -1);
    EXPECT_EQ(dragonfly_update(system, -1.0f), -1);
}

TEST_F(DragonflySystemTest, UpdateWithDetection) {
    // Set self state
    dragonfly_self_state_t self = create_self_state(0.0f, 0.0f, 0.0f);
    dragonfly_update_self_state(system, &self);

    // Start scanning
    dragonfly_start_scan(system);

    // Process detection
    dragonfly_detection_t det = create_detection(1, 10.0f, 5.0f, 0.0f);
    dragonfly_process_detection(system, &det);

    // Run update
    EXPECT_EQ(dragonfly_update(system, 0.01f), 0);
}

//=============================================================================
// Target Query Tests
//=============================================================================

TEST_F(DragonflySystemTest, GetAllTargetsEmpty) {
    dragonfly_target_info_t targets[8];
    uint32_t num_targets = 0;

    EXPECT_EQ(dragonfly_get_all_targets(system, targets, 8, &num_targets), 0);
    EXPECT_EQ(num_targets, 0u);
}

TEST_F(DragonflySystemTest, GetAllTargetsAfterDetection) {
    // Process some detections
    for (uint32_t i = 0; i < 3; i++) {
        dragonfly_detection_t det = create_detection(i + 1, 10.0f + i, 5.0f, 0.0f);
        dragonfly_process_detection(system, &det);
    }

    dragonfly_target_info_t targets[8];
    uint32_t num_targets = 0;

    EXPECT_EQ(dragonfly_get_all_targets(system, targets, 8, &num_targets), 0);
    EXPECT_GT(num_targets, 0u);
}

TEST_F(DragonflySystemTest, GetPrimaryTargetNoTarget) {
    dragonfly_target_info_t target;
    EXPECT_EQ(dragonfly_get_primary_target(system, &target), -1);
}

//=============================================================================
// TSDN Vector Tests
//=============================================================================

TEST_F(DragonflySystemTest, GetTSDNVector) {
    tsdn_vector_t vector;
    EXPECT_EQ(dragonfly_get_tsdn_vector(system, &vector), 0);
}

TEST_F(DragonflySystemTest, TSDNVectorAfterDetection) {
    // Process detection
    dragonfly_detection_t det = create_detection(1, 10.0f, 0.0f, 0.0f);  // At 0 degrees
    dragonfly_process_detection(system, &det);

    tsdn_vector_t vector;
    EXPECT_EQ(dragonfly_get_tsdn_vector(system, &vector), 0);
    // Vector should be valid (magnitude > 0 after detection)
}

//=============================================================================
// Prediction Tests
//=============================================================================

TEST_F(DragonflySystemTest, GetPrediction) {
    trajectory_prediction_t prediction;
    EXPECT_EQ(dragonfly_get_prediction(system, &prediction), 0);
}

//=============================================================================
// Intercept Solution Tests
//=============================================================================

TEST_F(DragonflySystemTest, GetInterceptSolution) {
    intercept_solution_t solution;
    EXPECT_EQ(dragonfly_get_intercept_solution(system, &solution), 0);
}

//=============================================================================
// Motor Command Tests
//=============================================================================

TEST_F(DragonflySystemTest, GetMotorCommand) {
    dragonfly_motor_cmd_t cmd;
    EXPECT_EQ(dragonfly_get_motor_command(system, &cmd), 0);
}

//=============================================================================
// Control Function Tests
//=============================================================================

TEST_F(DragonflySystemTest, LockTargetNoTarget) {
    EXPECT_EQ(dragonfly_lock_target(system, 999), -1);  // Non-existent target
}

TEST_F(DragonflySystemTest, StartPursuitNotTracking) {
    EXPECT_EQ(dragonfly_start_pursuit(system), -1);  // Not in TRACKING mode
}

TEST_F(DragonflySystemTest, AbortPursuitNotPursuing) {
    // Should still succeed (no-op)
    EXPECT_EQ(dragonfly_abort_pursuit(system), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonflySystemTest, GetStats) {
    dragonfly_stats_t stats;
    EXPECT_EQ(dragonfly_get_stats(system, &stats), 0);

    EXPECT_EQ(stats.detections_processed, 0u);
    EXPECT_EQ(stats.targets_tracked, 0u);
}

TEST_F(DragonflySystemTest, StatsTrackDetections) {
    dragonfly_detection_t det = create_detection(1, 10.0f, 5.0f, 0.0f);

    for (int i = 0; i < 5; i++) {
        dragonfly_process_detection(system, &det);
    }

    dragonfly_stats_t stats;
    dragonfly_get_stats(system, &stats);
    EXPECT_EQ(stats.detections_processed, 5u);
}

TEST_F(DragonflySystemTest, ResetStats) {
    // Process some data
    dragonfly_detection_t det = create_detection(1, 10.0f, 5.0f, 0.0f);
    dragonfly_process_detection(system, &det);

    // Reset stats
    EXPECT_EQ(dragonfly_reset_stats(system), 0);

    dragonfly_stats_t stats;
    dragonfly_get_stats(system, &stats);
    EXPECT_EQ(stats.detections_processed, 0u);
}

//=============================================================================
// Configuration Update Tests
//=============================================================================

TEST_F(DragonflySystemTest, SetConfig) {
    dragonfly_config_t config = dragonfly_default_config();
    config.pursuit_timeout_s = 45.0f;

    EXPECT_EQ(dragonfly_set_config(system, &config), 0);

    dragonfly_config_t retrieved;
    dragonfly_get_config(system, &retrieved);
    EXPECT_FLOAT_EQ(retrieved.pursuit_timeout_s, 45.0f);
}

TEST_F(DragonflySystemTest, SetInvalidConfig) {
    dragonfly_config_t config = dragonfly_default_config();
    config.min_target_size = -1.0f;  // Invalid

    EXPECT_EQ(dragonfly_set_config(system, &config), -1);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(DragonflySystemTest, ModeName) {
    EXPECT_STREQ(dragonfly_mode_name(DRAGONFLY_MODE_IDLE), "Idle");
    EXPECT_STREQ(dragonfly_mode_name(DRAGONFLY_MODE_SCANNING), "Scanning");
    EXPECT_STREQ(dragonfly_mode_name(DRAGONFLY_MODE_TRACKING), "Tracking");
    EXPECT_STREQ(dragonfly_mode_name(DRAGONFLY_MODE_PURSUING), "Pursuing");
    EXPECT_STREQ(dragonfly_mode_name(DRAGONFLY_MODE_INTERCEPTING), "Intercepting");
}

TEST_F(DragonflySystemTest, HuntResultName) {
    EXPECT_STREQ(dragonfly_hunt_result_name(DRAGONFLY_HUNT_IN_PROGRESS), "In Progress");
    EXPECT_STREQ(dragonfly_hunt_result_name(DRAGONFLY_HUNT_SUCCESS), "Success");
    EXPECT_STREQ(dragonfly_hunt_result_name(DRAGONFLY_HUNT_ESCAPED), "Escaped");
    EXPECT_STREQ(dragonfly_hunt_result_name(DRAGONFLY_HUNT_ABORTED), "Aborted");
    EXPECT_STREQ(dragonfly_hunt_result_name(DRAGONFLY_HUNT_TIMEOUT), "Timeout");
}

TEST_F(DragonflySystemTest, Version) {
    const char* version = dragonfly_version();
    ASSERT_NE(version, nullptr);
    EXPECT_STREQ(version, "1.0.0");
}

//=============================================================================
// Null Pointer Handling Tests
//=============================================================================

TEST_F(DragonflySystemTest, NullPointerHandling) {
    dragonfly_detection_t det = create_detection(1, 10.0f, 5.0f, 0.0f);
    dragonfly_self_state_t self = create_self_state(0.0f, 0.0f, 0.0f);
    dragonfly_motor_cmd_t cmd;
    dragonfly_target_info_t target;
    tsdn_vector_t vector;
    trajectory_prediction_t pred;
    intercept_solution_t sol;
    dragonfly_stats_t stats;
    dragonfly_config_t config;

    // Null system
    EXPECT_EQ(dragonfly_process_detection(nullptr, &det), -1);
    EXPECT_EQ(dragonfly_update_self_state(nullptr, &self), -1);
    EXPECT_EQ(dragonfly_update(nullptr, 0.01f), -1);
    EXPECT_EQ(dragonfly_get_motor_command(nullptr, &cmd), -1);
    EXPECT_EQ(dragonfly_get_primary_target(nullptr, &target), -1);
    EXPECT_EQ(dragonfly_get_tsdn_vector(nullptr, &vector), -1);
    EXPECT_EQ(dragonfly_get_prediction(nullptr, &pred), -1);
    EXPECT_EQ(dragonfly_get_intercept_solution(nullptr, &sol), -1);
    EXPECT_EQ(dragonfly_start_scan(nullptr), -1);
    EXPECT_EQ(dragonfly_lock_target(nullptr, 1), -1);
    EXPECT_EQ(dragonfly_start_pursuit(nullptr), -1);
    EXPECT_EQ(dragonfly_abort_pursuit(nullptr), -1);  // Null check
    EXPECT_EQ(dragonfly_go_idle(nullptr), -1);
    EXPECT_EQ(dragonfly_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_reset_stats(nullptr), -1);
    EXPECT_EQ(dragonfly_set_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_get_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_system_reset(nullptr), -1);

    // Null output params
    EXPECT_EQ(dragonfly_process_detection(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_motor_command(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_primary_target(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_tsdn_vector(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_prediction(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_intercept_solution(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_stats(system, nullptr), -1);
    EXPECT_EQ(dragonfly_set_config(system, nullptr), -1);
    EXPECT_EQ(dragonfly_get_config(system, nullptr), -1);
}
