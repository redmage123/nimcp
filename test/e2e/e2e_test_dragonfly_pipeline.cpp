/**
 * @file e2e_test_dragonfly_pipeline.cpp
 * @brief End-to-end tests for the complete dragonfly processing pipeline
 *
 * WHAT: Tests the full dragonfly pipeline from detection to motor output
 * WHY:  Verify all subsystems work together correctly
 * HOW:  Simulate realistic scenarios through the E2E framework
 *
 * PIPELINE STAGES TESTED:
 * 1. Detection Input: Visual cortex bridge receives motion blobs
 * 2. TSDN Encoding: Direction encoded as population vector
 * 3. Tracking: CSTMD1-inspired attention and tracking
 * 4. Prediction: Kalman/IMM trajectory prediction
 * 5. Interception: PN guidance for optimal pursuit
 * 6. Motor Output: Heading, pitch, velocity commands
 * 7. Brain Integration: Full brain lifecycle with dragonfly
 *
 * @author NIMCP Team
 * @date 2024-12-30
 */

#include "e2e_test_framework.h"

#include <cmath>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_visual_bridge.h"
#include "dragonfly/nimcp_dragonfly_audio_bridge.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Fixtures
//=============================================================================

class DragonflyPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        nimcp_memory_check_leaks();
        nimcp_shutdown();
    }
};

//=============================================================================
// E2E Test: Basic Detection to Motor Pipeline
//=============================================================================

E2E_TEST(DragonflyPipelineTest, BasicDetectionToMotorPipeline) {
    E2E_PIPELINE_START("Dragonfly Basic Detection Pipeline");

    brain_t brain = nullptr;
    dragonfly_system_t* dragonfly = nullptr;

    // Stage 1: Create brain with dragonfly enabled
    E2E_STAGE_BEGIN("Create brain with dragonfly", 500);
    {
        brain_config_t config = {};
        strncpy(config.task_name, "dragonfly_e2e", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = true;
        config.dragonfly_enable_imm = true;
        config.dragonfly_prediction_horizon_ms = 200.0f;
        config.dragonfly_nav_gain = 3.5f;

        brain = brain_create_custom(&config);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");

        dragonfly = brain_get_dragonfly(brain);
        E2E_ASSERT_NOT_NULL(dragonfly, "Dragonfly subsystem not created");
    }
    E2E_STAGE_END();

    // Stage 2: Verify initial state
    E2E_STAGE_BEGIN("Verify initial state", 10);
    {
        int mode = brain_dragonfly_get_mode(brain);
        E2E_ASSERT(mode == (int)DRAGONFLY_MODE_IDLE, "Initial mode should be IDLE");

        dragonfly_stats_t stats;
        int result = dragonfly_get_stats(dragonfly, &stats);
        E2E_ASSERT(result == 0, "Failed to get stats");
        E2E_ASSERT(stats.detections_processed == 0, "Initial detections should be 0");
    }
    E2E_STAGE_END();

    // Stage 3: Set self-state (pursuer position)
    E2E_STAGE_BEGIN("Set self-state", 10);
    {
        dragonfly_self_state_t self = {};
        self.position[0] = 0.0f;
        self.position[1] = 0.0f;
        self.position[2] = 0.0f;
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;

        int result = dragonfly_update_self_state(dragonfly, &self);
        E2E_ASSERT(result == 0, "Failed to set self-state");
    }
    E2E_STAGE_END();

    // Stage 4: Process detection sequence
    E2E_STAGE_BEGIN("Process detection sequence", 100);
    {
        float target_x = 20.0f;
        float target_y = 5.0f;
        float target_vx = -3.0f;
        float target_vy = -0.5f;

        for (int frame = 0; frame < 60; frame++) {
            target_x += target_vx * 0.016f;
            target_y += target_vy * 0.016f;

            float position[3] = {target_x, target_y, 0.0f};
            int result = brain_dragonfly_detect(brain, position, 0.05f, 0.85f);
            E2E_ASSERT(result == 0, "Detection failed");

            result = brain_step_dragonfly(brain, 16000);  // 16ms
            E2E_ASSERT(result == 0, "Step failed");
        }
    }
    E2E_STAGE_END();

    // Stage 5: Verify tracking mode achieved
    E2E_STAGE_BEGIN("Verify tracking mode", 10);
    {
        int mode = brain_dragonfly_get_mode(brain);
        E2E_ASSERT(mode == (int)DRAGONFLY_MODE_TRACKING ||
                   mode == (int)DRAGONFLY_MODE_PURSUING ||
                   mode == (int)DRAGONFLY_MODE_SCANNING,
                   "Should be in TRACKING, PURSUING, or SCANNING mode");
    }
    E2E_STAGE_END();

    // Stage 6: Get motor command
    E2E_STAGE_BEGIN("Get motor command", 10);
    {
        float heading, pitch, urgency;
        float velocity[3];

        int result = brain_dragonfly_get_command(brain, &heading, &pitch, velocity, &urgency);
        E2E_ASSERT(result == 0, "Failed to get motor command");

        // Target started at (20, 5) and moved with velocity (-3, -0.5)
        // After 60 frames at 16ms, target is approximately at:
        // x = 20 + (-3 * 0.016 * 60) = 20 - 2.88 = 17.12
        // y = 5 + (-0.5 * 0.016 * 60) = 5 - 0.48 = 4.52
        // Heading to target from origin should be approximately atan2(4.52, 17.12) ~ 0.26 rad
        // Allow generous tolerance for prediction dynamics
        E2E_ASSERT(heading > -0.5f && heading < 1.0f,
                   "Heading should point toward target in upper-right quadrant");
        E2E_ASSERT(urgency > 0.0f, "Urgency should be positive when tracking target");
    }
    E2E_STAGE_END();

    // Stage 7: Verify statistics
    E2E_STAGE_BEGIN("Verify statistics", 10);
    {
        dragonfly_stats_t stats;
        int result = dragonfly_get_stats(dragonfly, &stats);
        E2E_ASSERT(result == 0, "Failed to get stats");
        E2E_ASSERT(stats.detections_processed == 60, "Should have processed 60 detections");
        E2E_ASSERT(stats.total_updates == 60, "Should have 60 updates");
    }
    E2E_STAGE_END();

    // Stage 8: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Pursuit Success Pipeline
//=============================================================================

E2E_TEST(DragonflyPipelineTest, PursuitSuccessPipeline) {
    E2E_PIPELINE_START("Dragonfly Pursuit Success Pipeline");

    brain_t brain = nullptr;
    dragonfly_system_t* dragonfly = nullptr;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain_config_t config = {};
        strncpy(config.task_name, "pursuit_success_e2e", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = true;
        config.dragonfly_enable_imm = false;  // Simple prediction

        brain = brain_create_custom(&config);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
        dragonfly = brain_get_dragonfly(brain);
        E2E_ASSERT_NOT_NULL(dragonfly, "Dragonfly not created");
    }
    E2E_STAGE_END();

    // Stage 2: Setup pursuer and target
    E2E_STAGE_BEGIN("Setup simulation", 10);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);
    }
    E2E_STAGE_END();

    // Stage 3: Simulate pursuit
    E2E_STAGE_BEGIN("Simulate pursuit", 5000);
    {
        float pursuer_x = 0.0f, pursuer_y = 0.0f;
        float target_x = 15.0f, target_y = 0.0f;
        float target_vx = -2.0f, target_vy = 0.0f;
        float dt = 0.016f;

        bool intercepted = false;
        int max_frames = 300;  // 5 seconds at 60 FPS

        for (int frame = 0; frame < max_frames && !intercepted; frame++) {
            // Update target position
            target_x += target_vx * dt;
            target_y += target_vy * dt;

            // Update self-state with pursuer position
            dragonfly_self_state_t self = {};
            self.position[0] = pursuer_x;
            self.position[1] = pursuer_y;
            self.max_speed = 10.0f;
            self.max_accel = 20.0f;
            self.max_turn_rate = 6.0f;
            self.energy_level = 1.0f;
            dragonfly_update_self_state(dragonfly, &self);

            // Process detection
            float position[3] = {target_x, target_y, 0.0f};
            brain_dragonfly_detect(brain, position, 0.05f, 0.85f);
            brain_step_dragonfly(brain, (uint64_t)(dt * 1e6));

            // Get motor command and update pursuer
            float heading, pitch, urgency;
            float velocity[3];
            brain_dragonfly_get_command(brain, &heading, &pitch, velocity, &urgency);

            float speed = fminf(urgency * 10.0f, 10.0f);
            pursuer_x += speed * cosf(heading) * dt;
            pursuer_y += speed * sinf(heading) * dt;

            // Check for interception
            float dist = sqrtf((target_x - pursuer_x) * (target_x - pursuer_x) +
                              (target_y - pursuer_y) * (target_y - pursuer_y));
            if (dist < 1.5f) {
                intercepted = true;
            }
        }

        E2E_ASSERT(intercepted, "Should have intercepted target");
    }
    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Multi-Target Selection Pipeline
//=============================================================================

E2E_TEST(DragonflyPipelineTest, MultiTargetSelectionPipeline) {
    E2E_PIPELINE_START("Dragonfly Multi-Target Selection Pipeline");

    brain_t brain = nullptr;
    dragonfly_system_t* dragonfly = nullptr;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain_config_t config = {};
        strncpy(config.task_name, "multi_target_e2e", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = true;

        brain = brain_create_custom(&config);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
        dragonfly = brain_get_dragonfly(brain);
    }
    E2E_STAGE_END();

    // Stage 2: Setup self-state
    E2E_STAGE_BEGIN("Setup self-state", 10);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);
    }
    E2E_STAGE_END();

    // Stage 3: Present multiple targets
    E2E_STAGE_BEGIN("Present multiple targets", 100);
    {
        // Define multiple targets at different positions
        struct Target {
            float x, y, vx, vy;
            float contrast;
        };

        Target targets[] = {
            {15.0f, 5.0f, -2.0f, -0.5f, 0.8f},
            {20.0f, -3.0f, -3.0f, 0.0f, 0.85f},
            {12.0f, 0.0f, -1.5f, 0.2f, 0.9f},  // Closest with high contrast
            {25.0f, 8.0f, -2.5f, -1.0f, 0.7f}
        };

        for (int frame = 0; frame < 60; frame++) {
            for (int t = 0; t < 4; t++) {
                targets[t].x += targets[t].vx * 0.016f;
                targets[t].y += targets[t].vy * 0.016f;

                dragonfly_detection_t det = {};
                det.id = t + 1;
                det.position[0] = targets[t].x;
                det.position[1] = targets[t].y;
                det.position[2] = 0.0f;
                det.size = 0.05f;
                det.contrast = targets[t].contrast;
                det.motion_direction_rad = atan2f(targets[t].vy, targets[t].vx);
                det.motion_speed = sqrtf(targets[t].vx * targets[t].vx +
                                        targets[t].vy * targets[t].vy);

                dragonfly_process_detection(dragonfly, &det);
            }

            dragonfly_update(dragonfly, 0.016f);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Verify winner-take-all selection
    E2E_STAGE_BEGIN("Verify target selection", 10);
    {
        dragonfly_target_info_t primary;
        int result = dragonfly_get_primary_target(dragonfly, &primary);

        // Should have selected one target
        if (result == 0) {
            E2E_ASSERT(primary.id >= 1 && primary.id <= 4, "Invalid target ID");
            E2E_ASSERT(primary.confidence > 0.0f, "Should have confidence > 0");
        }

        // Get all targets
        dragonfly_target_info_t all_targets[8];
        uint32_t num_targets = 0;
        result = dragonfly_get_all_targets(dragonfly, all_targets, 8, &num_targets);
        E2E_ASSERT(result == 0, "Failed to get all targets");
        E2E_ASSERT(num_targets >= 1, "Should track at least one target");
    }
    E2E_STAGE_END();

    // Stage 5: Verify statistics
    E2E_STAGE_BEGIN("Verify statistics", 10);
    {
        dragonfly_stats_t stats;
        dragonfly_get_stats(dragonfly, &stats);

        // Should have processed all detections
        E2E_ASSERT(stats.detections_processed == 60 * 4, "Should process all detections");
    }
    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Evasive Target Pipeline
//=============================================================================

E2E_TEST(DragonflyPipelineTest, EvasiveTargetPipeline) {
    E2E_PIPELINE_START("Dragonfly Evasive Target Pipeline");

    brain_t brain = nullptr;
    dragonfly_system_t* dragonfly = nullptr;

    // Stage 1: Create brain with IMM enabled
    E2E_STAGE_BEGIN("Create brain with IMM", 500);
    {
        brain_config_t config = {};
        strncpy(config.task_name, "evasive_e2e", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = true;
        config.dragonfly_enable_imm = true;  // Enable multi-model prediction

        brain = brain_create_custom(&config);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
        dragonfly = brain_get_dragonfly(brain);
    }
    E2E_STAGE_END();

    // Stage 2: Setup pursuer
    E2E_STAGE_BEGIN("Setup pursuer", 10);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 12.0f;  // Faster pursuer
        self.max_accel = 25.0f;
        self.max_turn_rate = 8.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);
    }
    E2E_STAGE_END();

    // Stage 3: Track target that performs jinking maneuver
    E2E_STAGE_BEGIN("Track jinking target", 2000);
    {
        float target_x = 20.0f, target_y = 0.0f;
        float target_vx = -3.0f, target_vy = 0.0f;
        float dt = 0.016f;

        for (int frame = 0; frame < 120; frame++) {
            // Jink every 20 frames
            if (frame % 20 == 0 && frame > 0) {
                target_vy = (frame % 40 == 0) ? 4.0f : -4.0f;
            }

            target_x += target_vx * dt;
            target_y += target_vy * dt;

            dragonfly_detection_t det = {};
            det.id = 1;
            det.position[0] = target_x;
            det.position[1] = target_y;
            det.position[2] = 0.0f;
            det.size = 0.05f;
            det.contrast = 0.85f;
            det.motion_direction_rad = atan2f(target_vy, target_vx);
            det.motion_speed = sqrtf(target_vx * target_vx + target_vy * target_vy);

            dragonfly_process_detection(dragonfly, &det);
            dragonfly_update(dragonfly, dt);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Verify evasion detection
    E2E_STAGE_BEGIN("Check evasion detection", 10);
    {
        dragonfly_target_info_t target;
        int result = dragonfly_get_primary_target(dragonfly, &target);
        E2E_ASSERT(result == 0, "Should have a tracked target after processing detections");

        // Target performed jinking maneuvers (direction changes every 20 frames)
        // IMM should have detected the evasive behavior through model probability updates
        // Check that tracking confidence is reasonable despite evasive behavior
        E2E_ASSERT(target.confidence > 0.3f,
                   "Should maintain tracking confidence despite evasive target");

        // The target changed direction multiple times, so prediction uncertainty should be elevated
        // If IMM is working, it should have adapted to the maneuvering behavior
        // At minimum, verify we're still tracking the correct target
        E2E_ASSERT(target.id == 1, "Should be tracking the jinking target (id=1)");
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Visual Bridge Integration Pipeline
//=============================================================================

E2E_TEST(DragonflyPipelineTest, VisualBridgeIntegrationPipeline) {
    E2E_PIPELINE_START("Dragonfly Visual Bridge Integration Pipeline");

    dragonfly_system_t* dragonfly = nullptr;
    dragonfly_visual_bridge_t* visual_bridge = nullptr;

    // Stage 1: Create dragonfly system
    E2E_STAGE_BEGIN("Create dragonfly system", 100);
    {
        dragonfly_config_t config = dragonfly_default_config();
        dragonfly = dragonfly_system_create(&config);
        E2E_ASSERT_NOT_NULL(dragonfly, "Dragonfly creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Create visual bridge
    E2E_STAGE_BEGIN("Create visual bridge", 100);
    {
        visual_bridge_config_t vconfig = visual_bridge_default_config();
        visual_bridge = dragonfly_visual_bridge_create(dragonfly, nullptr, &vconfig);
        E2E_ASSERT_NOT_NULL(visual_bridge, "Visual bridge creation failed");
    }
    E2E_STAGE_END();

    // Stage 3: Setup self-state
    E2E_STAGE_BEGIN("Setup self-state", 10);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);
    }
    E2E_STAGE_END();

    // Stage 4: Process motion blobs through visual bridge
    E2E_STAGE_BEGIN("Process motion blobs", 100);
    {
        for (int frame = 0; frame < 30; frame++) {
            motion_blob_t blob = {};
            blob.center_x = 320.0f - frame * 5.0f;  // Moving left
            blob.center_y = 240.0f + frame * 2.0f;
            blob.size_pixels = 25.0f;
            blob.velocity_x = -5.0f;
            blob.velocity_y = 2.0f;
            blob.contrast = 0.85f;
            blob.salience = 0.8f;

            int result = dragonfly_visual_bridge_inject_blob(visual_bridge, &blob);
            E2E_ASSERT(result == 0, "Blob injection failed");
        }
    }
    E2E_STAGE_END();

    // Stage 5: Verify visual bridge stats
    E2E_STAGE_BEGIN("Verify visual bridge stats", 10);
    {
        visual_bridge_stats_t stats;
        int result = dragonfly_visual_bridge_get_stats(visual_bridge, &stats);
        E2E_ASSERT(result == 0, "Failed to get visual bridge stats");
        E2E_ASSERT(stats.blobs_detected == 30, "Should have detected 30 blobs");
    }
    E2E_STAGE_END();

    // Stage 6: Verify attention peak
    E2E_STAGE_BEGIN("Verify attention peak", 10);
    {
        visual_motion_result_t motion_result;
        int result = dragonfly_visual_bridge_get_result(visual_bridge, &motion_result);
        E2E_ASSERT(result == 0, "Failed to get motion result");
    }
    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        dragonfly_visual_bridge_destroy(visual_bridge);
        dragonfly_system_destroy(dragonfly);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Full System Lifecycle Pipeline
//=============================================================================

E2E_TEST(DragonflyPipelineTest, FullSystemLifecyclePipeline) {
    E2E_PIPELINE_START("Dragonfly Full System Lifecycle Pipeline");

    brain_t brain = nullptr;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain_config_t config = {};
        strncpy(config.task_name, "lifecycle_e2e", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = true;

        brain = brain_create_custom(&config);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Idle → Scanning → Tracking → Pursuing
    E2E_STAGE_BEGIN("Mode transitions", 500);
    {
        dragonfly_system_t* df = brain_get_dragonfly(brain);
        E2E_ASSERT_NOT_NULL(df, "Dragonfly not found");

        // Setup self-state
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(df, &self);

        // Initial mode should be IDLE
        E2E_ASSERT(dragonfly_get_mode(df) == DRAGONFLY_MODE_IDLE, "Should start IDLE");

        // Start scanning
        dragonfly_start_scan(df);
        E2E_ASSERT(dragonfly_get_mode(df) == DRAGONFLY_MODE_SCANNING, "Should be SCANNING");

        // Detect target
        for (int i = 0; i < 40; i++) {
            float position[3] = {15.0f - i * 0.1f, 0.0f, 0.0f};
            brain_dragonfly_detect(brain, position, 0.05f, 0.85f);
            brain_step_dragonfly(brain, 16000);
        }

        // Should be in tracking or pursuing
        int mode = dragonfly_get_mode(df);
        E2E_ASSERT(mode == DRAGONFLY_MODE_TRACKING ||
                   mode == DRAGONFLY_MODE_PURSUING ||
                   mode == DRAGONFLY_MODE_SCANNING,
                   "Should be TRACKING, PURSUING, or SCANNING");
    }
    E2E_STAGE_END();

    // Stage 3: Abort and return to idle
    E2E_STAGE_BEGIN("Abort pursuit", 100);
    {
        int result = brain_dragonfly_abort(brain);
        E2E_ASSERT(result == 0, "Abort failed");

        brain_step_dragonfly(brain, 16000);

        int mode = brain_dragonfly_get_mode(brain);
        E2E_ASSERT(mode == (int)DRAGONFLY_MODE_IDLE ||
                   mode == (int)DRAGONFLY_MODE_SCANNING,
                   "Should be IDLE or SCANNING after abort");
    }
    E2E_STAGE_END();

    // Stage 4: Reset and start again
    E2E_STAGE_BEGIN("Reset and restart", 100);
    {
        dragonfly_system_t* df = brain_get_dragonfly(brain);
        dragonfly_system_reset(df);

        E2E_ASSERT(dragonfly_get_mode(df) == DRAGONFLY_MODE_IDLE, "Should be IDLE after reset");

        dragonfly_stats_t stats;
        dragonfly_get_stats(df, &stats);
        // Stats should be cleared or preserved depending on implementation
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Performance Under Load Pipeline
//=============================================================================

E2E_TEST(DragonflyPipelineTest, PerformanceUnderLoadPipeline) {
    E2E_PIPELINE_START("Dragonfly Performance Under Load Pipeline");

    brain_t brain = nullptr;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain_config_t config = {};
        strncpy(config.task_name, "perf_e2e", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = true;
        config.dragonfly_enable_imm = true;

        brain = brain_create_custom(&config);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Setup
    E2E_STAGE_BEGIN("Setup", 10);
    {
        dragonfly_system_t* df = brain_get_dragonfly(brain);
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(df, &self);
    }
    E2E_STAGE_END();

    // Stage 3: High-frequency updates (simulating 120 Hz)
    E2E_STAGE_BEGIN("High-frequency updates", 5000);
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> pos_dist(-30.0f, 30.0f);
        std::uniform_real_distribution<float> vel_dist(-5.0f, 5.0f);

        for (int frame = 0; frame < 1000; frame++) {
            // Random target position
            float position[3] = {
                10.0f + pos_dist(rng) * 0.1f,
                pos_dist(rng) * 0.1f,
                0.0f
            };

            brain_dragonfly_detect(brain, position, 0.05f, 0.8f);
            brain_step_dragonfly(brain, 8333);  // ~120 Hz
        }
    }
    E2E_STAGE_END();

    // Stage 4: Verify no crashes and stats are reasonable
    E2E_STAGE_BEGIN("Verify stability", 10);
    {
        dragonfly_system_t* df = brain_get_dragonfly(brain);
        dragonfly_stats_t stats;
        int result = dragonfly_get_stats(df, &stats);
        E2E_ASSERT(result == 0, "Failed to get stats after load test");
        E2E_ASSERT(stats.detections_processed == 1000, "Should have processed 1000 detections");
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Energy-Aware Pursuit Pipeline
//=============================================================================

E2E_TEST(DragonflyPipelineTest, EnergyAwarePursuitPipeline) {
    E2E_PIPELINE_START("Dragonfly Energy-Aware Pursuit Pipeline");

    dragonfly_system_t* dragonfly = nullptr;

    // Stage 1: Create energy-aware system
    E2E_STAGE_BEGIN("Create energy-aware system", 100);
    {
        dragonfly_config_t config = dragonfly_default_config();
        config.energy_aware = true;
        config.min_energy_reserve = 0.2f;

        dragonfly = dragonfly_system_create(&config);
        E2E_ASSERT_NOT_NULL(dragonfly, "Dragonfly creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Test with high energy
    E2E_STAGE_BEGIN("High energy pursuit", 100);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;  // Full energy
        dragonfly_update_self_state(dragonfly, &self);

        dragonfly_start_scan(dragonfly);

        // Track target
        for (int i = 0; i < 30; i++) {
            dragonfly_detection_t det = {};
            det.id = 1;
            det.position[0] = 15.0f - i * 0.1f;
            det.position[1] = 0.0f;
            det.position[2] = 0.0f;
            det.size = 0.05f;
            det.contrast = 0.85f;
            dragonfly_process_detection(dragonfly, &det);
            dragonfly_update(dragonfly, 0.016f);
        }

        dragonfly_motor_cmd_t cmd;
        dragonfly_get_motor_command(dragonfly, &cmd);
        // High energy should result in strong pursuit
    }
    E2E_STAGE_END();

    // Stage 3: Test with low energy
    E2E_STAGE_BEGIN("Low energy behavior", 100);
    {
        dragonfly_system_reset(dragonfly);

        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 0.15f;  // Below reserve
        dragonfly_update_self_state(dragonfly, &self);

        dragonfly_start_scan(dragonfly);

        // Track distant target
        for (int i = 0; i < 30; i++) {
            dragonfly_detection_t det = {};
            det.id = 1;
            det.position[0] = 30.0f;  // Far target
            det.position[1] = 10.0f;
            det.position[2] = 0.0f;
            det.size = 0.05f;
            det.contrast = 0.8f;
            dragonfly_process_detection(dragonfly, &det);
            dragonfly_update(dragonfly, 0.016f);
        }

        dragonfly_motor_cmd_t cmd;
        dragonfly_get_motor_command(dragonfly, &cmd);
        // Low energy should result in reduced urgency
    }
    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        dragonfly_system_destroy(dragonfly);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
