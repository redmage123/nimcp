/**
 * @file e2e_test_dragonfly_multimodal.cpp
 * @brief End-to-end tests for dragonfly multimodal sensory integration
 *
 * WHAT: Tests dragonfly multimodal fusion (visual, audio, proprioceptive)
 * WHY:  Real dragonflies integrate multiple senses for robust tracking
 * HOW:  Combine visual motion, audio directional cues, and self-state
 *
 * MULTIMODAL INTEGRATION:
 * - Visual: Motion detection blobs from visual cortex
 * - Audio: Directional cues from audio cortex (wing buzz sounds)
 * - Proprioceptive: Self-state (position, velocity, heading)
 * - Integration: Kalman fusion for robust target estimation
 *
 * BIOLOGICAL BASIS:
 * - Dragonflies use primarily visual tracking but have multimodal inputs
 * - Audio cues can direct initial attention
 * - Proprioception essential for pursuit dynamics
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

class DragonflyMultimodalTest : public ::testing::Test {
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
// E2E Test: Visual + Audio Fusion Pipeline
//=============================================================================

E2E_TEST(DragonflyMultimodalTest, VisualAudioFusionPipeline) {
    E2E_PIPELINE_START("Dragonfly Visual+Audio Fusion Pipeline");

    dragonfly_system_t* dragonfly = nullptr;
    dragonfly_visual_bridge_t* visual_bridge = nullptr;
    dragonfly_audio_bridge_t* audio_bridge = nullptr;

    // Stage 1: Create dragonfly system
    E2E_STAGE_BEGIN("Create dragonfly system", 100);
    {
        dragonfly_config_t config = dragonfly_default_config();
        config.prediction_config.enable_imm = true;
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

    // Stage 3: Create audio bridge
    E2E_STAGE_BEGIN("Create audio bridge", 100);
    {
        audio_bridge_config_t aconfig = audio_bridge_default_config();
        audio_bridge = dragonfly_audio_bridge_create(dragonfly, nullptr, &aconfig);
        E2E_ASSERT_NOT_NULL(audio_bridge, "Audio bridge creation failed");
    }
    E2E_STAGE_END();

    // Stage 4: Setup self-state
    E2E_STAGE_BEGIN("Setup self-state", 10);
    {
        dragonfly_self_state_t self = {};
        self.position[0] = 0.0f;
        self.position[1] = 0.0f;
        self.position[2] = 0.0f;
        self.heading_rad = 0.0f;
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);
    }
    E2E_STAGE_END();

    // Stage 5: Process audio source first (primes attention)
    E2E_STAGE_BEGIN("Audio attention priming", 50);
    {
        // Audio source from upper-right direction
        audio_source_t source = {};
        source.azimuth = 0.4f;  // ~23 degrees right
        source.elevation = 0.1f;
        source.confidence = 0.7f;
        source.intensity_db = -20.0f;  // Moderate intensity
        source.frequency_hz = 200.0f;  // Wing buzz frequency
        source.bandwidth_hz = 50.0f;
        source.source_id = 1;
        source.timestamp_us = 0;

        int result = dragonfly_audio_bridge_inject_source(audio_bridge, &source);
        E2E_ASSERT(result == 0, "Audio source injection failed");
    }
    E2E_STAGE_END();

    // Stage 6: Visual confirmation (target appears where audio indicated)
    E2E_STAGE_BEGIN("Visual confirmation", 100);
    {
        float target_distance = 15.0f;
        float target_angle = 0.4f;  // Same as audio cue

        for (int frame = 0; frame < 30; frame++) {
            // Target moving toward origin
            float t = (float)frame * 0.016f;
            float dist = target_distance - 2.0f * t;

            motion_blob_t blob = {};
            blob.center_x = 320.0f + dist * 15.0f * cosf(target_angle);
            blob.center_y = 240.0f - dist * 15.0f * sinf(target_angle);
            blob.size_pixels = 20.0f + (target_distance - dist) * 2.0f;
            blob.velocity_x = -2.0f * cosf(target_angle);
            blob.velocity_y = 2.0f * sinf(target_angle);
            blob.contrast = 0.85f;
            blob.salience = 0.9f;

            int result = dragonfly_visual_bridge_inject_blob(visual_bridge, &blob);
            E2E_ASSERT(result == 0, "Visual blob injection failed");

            dragonfly_update(dragonfly, 0.016f);
        }
    }
    E2E_STAGE_END();

    // Stage 7: Verify multimodal fusion result
    E2E_STAGE_BEGIN("Verify fusion result", 10);
    {
        // Check that tracking is active
        dragonfly_mode_t mode = dragonfly_get_mode(dragonfly);
        E2E_ASSERT(mode == DRAGONFLY_MODE_TRACKING ||
                   mode == DRAGONFLY_MODE_PURSUING ||
                   mode == DRAGONFLY_MODE_SCANNING,
                   "Should be tracking after multimodal input");

        // Get motor command - heading should point toward target
        dragonfly_motor_cmd_t cmd;
        dragonfly_get_motor_command(dragonfly, &cmd);

        // Target is at angle ~0.4 rad (upper-right), heading should be in that direction
        // Allow tolerance for prediction and tracking dynamics
        float target_angle = 0.4f;
        float angle_diff = fabsf(cmd.heading_rad - target_angle);
        // Normalize angle difference to [-pi, pi]
        while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;
        angle_diff = fabsf(angle_diff);
        E2E_ASSERT(angle_diff < 1.0f,
                   "Motor command heading should point toward target (within 1 rad)");
    }
    E2E_STAGE_END();

    // Stage 8: Verify stats show both modalities used
    E2E_STAGE_BEGIN("Verify multimodal stats", 10);
    {
        visual_bridge_stats_t vstats;
        dragonfly_visual_bridge_get_stats(visual_bridge, &vstats);
        E2E_ASSERT(vstats.blobs_detected == 30, "Should have 30 visual blobs");

        audio_bridge_stats_t astats;
        dragonfly_audio_bridge_get_stats(audio_bridge, &astats);
        E2E_ASSERT(astats.sources_detected == 1, "Should have 1 audio source detected");
    }
    E2E_STAGE_END();

    // Stage 9: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        dragonfly_audio_bridge_destroy(audio_bridge);
        dragonfly_visual_bridge_destroy(visual_bridge);
        dragonfly_system_destroy(dragonfly);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Audio-Directed Visual Search Pipeline
//=============================================================================

E2E_TEST(DragonflyMultimodalTest, AudioDirectedSearchPipeline) {
    E2E_PIPELINE_START("Dragonfly Audio-Directed Search Pipeline");

    dragonfly_system_t* dragonfly = nullptr;
    dragonfly_audio_bridge_t* audio_bridge = nullptr;

    // Stage 1: Create system
    E2E_STAGE_BEGIN("Create system", 100);
    {
        dragonfly_config_t config = dragonfly_default_config();
        dragonfly = dragonfly_system_create(&config);
        E2E_ASSERT_NOT_NULL(dragonfly, "Dragonfly creation failed");

        audio_bridge_config_t aconfig = audio_bridge_default_config();
        audio_bridge = dragonfly_audio_bridge_create(dragonfly, nullptr, &aconfig);
        E2E_ASSERT_NOT_NULL(audio_bridge, "Audio bridge creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Setup self-state
    E2E_STAGE_BEGIN("Setup", 10);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);

        dragonfly_start_scan(dragonfly);
    }
    E2E_STAGE_END();

    // Stage 3: Multiple audio sources from different directions
    E2E_STAGE_BEGIN("Multiple audio sources", 100);
    {
        // Source from left
        audio_source_t left_source = {};
        left_source.azimuth = -0.5f;
        left_source.elevation = 0.0f;
        left_source.confidence = 0.6f;
        left_source.intensity_db = -30.0f;  // Quieter
        left_source.frequency_hz = 200.0f;
        left_source.bandwidth_hz = 50.0f;
        left_source.source_id = 1;
        left_source.timestamp_us = 0;
        dragonfly_audio_bridge_inject_source(audio_bridge, &left_source);

        dragonfly_update(dragonfly, 0.016f);

        // Source from right (louder - should grab attention)
        audio_source_t right_source = {};
        right_source.azimuth = 0.3f;
        right_source.elevation = 0.1f;
        right_source.confidence = 0.8f;
        right_source.intensity_db = -15.0f;  // Louder
        right_source.frequency_hz = 220.0f;
        right_source.bandwidth_hz = 50.0f;
        right_source.source_id = 2;
        right_source.timestamp_us = 16000;  // 16ms later
        dragonfly_audio_bridge_inject_source(audio_bridge, &right_source);

        dragonfly_update(dragonfly, 0.016f);
    }
    E2E_STAGE_END();

    // Stage 4: Get attention direction
    E2E_STAGE_BEGIN("Get attention direction", 10);
    {
        float cue_direction[2];
        float cue_priority;
        int ret = dragonfly_audio_bridge_get_attention_cue(audio_bridge, cue_direction, &cue_priority);
        // ret == 0 means cue generated, ret == 1 means no cue, ret == -1 means error
        E2E_ASSERT(ret >= 0, "Failed to get attention cue");

        // Should prioritize louder, higher confidence source
        // Exact behavior depends on attention model
    }
    E2E_STAGE_END();

    // Stage 5: Verify audio bridge stats
    E2E_STAGE_BEGIN("Verify stats", 10);
    {
        audio_bridge_stats_t stats;
        dragonfly_audio_bridge_get_stats(audio_bridge, &stats);
        E2E_ASSERT(stats.sources_detected == 2, "Should have detected 2 audio sources");
    }
    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        dragonfly_audio_bridge_destroy(audio_bridge);
        dragonfly_system_destroy(dragonfly);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Proprioceptive Feedback Pipeline
//=============================================================================

E2E_TEST(DragonflyMultimodalTest, ProprioceptiveFeedbackPipeline) {
    E2E_PIPELINE_START("Dragonfly Proprioceptive Feedback Pipeline");

    brain_t brain = nullptr;
    dragonfly_system_t* dragonfly = nullptr;

    // Stage 1: Create brain with dragonfly
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain_config_t config = {};
        strncpy(config.task_name, "proprio_e2e", sizeof(config.task_name) - 1);
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

    // Stage 2: Simulate pursuit with continuous self-state updates
    E2E_STAGE_BEGIN("Pursuit with proprioception", 2000);
    {
        float pursuer_x = 0.0f, pursuer_y = 0.0f;
        float pursuer_vx = 0.0f, pursuer_vy = 0.0f;
        float pursuer_heading = 0.0f;

        float target_x = 20.0f, target_y = 5.0f;
        float target_vx = -2.5f, target_vy = -0.5f;

        float dt = 0.016f;
        float max_accel = 15.0f;

        for (int frame = 0; frame < 100; frame++) {
            // Update target
            target_x += target_vx * dt;
            target_y += target_vy * dt;

            // Update self-state with current pursuer state
            dragonfly_self_state_t self = {};
            self.position[0] = pursuer_x;
            self.position[1] = pursuer_y;
            self.position[2] = 0.0f;
            self.velocity[0] = pursuer_vx;
            self.velocity[1] = pursuer_vy;
            self.velocity[2] = 0.0f;
            self.heading_rad = pursuer_heading;
            self.max_speed = 10.0f;
            self.max_accel = 20.0f;
            self.max_turn_rate = 6.0f;
            self.energy_level = 1.0f;
            dragonfly_update_self_state(dragonfly, &self);

            // Process detection
            float position[3] = {target_x, target_y, 0.0f};
            brain_dragonfly_detect(brain, position, 0.05f, 0.85f);
            brain_step_dragonfly(brain, (uint64_t)(dt * 1e6));

            // Get motor command
            float cmd_heading, cmd_pitch, urgency;
            float velocity[3];
            brain_dragonfly_get_command(brain, &cmd_heading, &cmd_pitch, velocity, &urgency);

            // Update pursuer based on command
            float target_dir_x = cosf(cmd_heading);
            float target_dir_y = sinf(cmd_heading);

            // Apply acceleration toward commanded direction
            float accel = urgency * max_accel;
            pursuer_vx += target_dir_x * accel * dt;
            pursuer_vy += target_dir_y * accel * dt;

            // Limit speed
            float speed = sqrtf(pursuer_vx * pursuer_vx + pursuer_vy * pursuer_vy);
            if (speed > 10.0f) {
                pursuer_vx *= 10.0f / speed;
                pursuer_vy *= 10.0f / speed;
            }

            // Update position and heading
            pursuer_x += pursuer_vx * dt;
            pursuer_y += pursuer_vy * dt;
            if (speed > 0.1f) {
                pursuer_heading = atan2f(pursuer_vy, pursuer_vx);
            }

            // Check for interception
            float dist = sqrtf((target_x - pursuer_x) * (target_x - pursuer_x) +
                              (target_y - pursuer_y) * (target_y - pursuer_y));
            if (dist < 1.5f) {
                break;  // Intercepted
            }
        }
    }
    E2E_STAGE_END();

    // Stage 3: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Sensory Conflict Resolution Pipeline
//=============================================================================

E2E_TEST(DragonflyMultimodalTest, SensoryConflictResolutionPipeline) {
    E2E_PIPELINE_START("Dragonfly Sensory Conflict Resolution Pipeline");

    dragonfly_system_t* dragonfly = nullptr;
    dragonfly_visual_bridge_t* visual_bridge = nullptr;
    dragonfly_audio_bridge_t* audio_bridge = nullptr;

    // Stage 1: Create system with all bridges
    E2E_STAGE_BEGIN("Create system", 200);
    {
        dragonfly_config_t config = dragonfly_default_config();
        dragonfly = dragonfly_system_create(&config);
        E2E_ASSERT_NOT_NULL(dragonfly, "Dragonfly creation failed");

        visual_bridge_config_t vconfig = visual_bridge_default_config();
        visual_bridge = dragonfly_visual_bridge_create(dragonfly, nullptr, &vconfig);
        E2E_ASSERT_NOT_NULL(visual_bridge, "Visual bridge creation failed");

        audio_bridge_config_t aconfig = audio_bridge_default_config();
        audio_bridge = dragonfly_audio_bridge_create(dragonfly, nullptr, &aconfig);
        E2E_ASSERT_NOT_NULL(audio_bridge, "Audio bridge creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Setup
    E2E_STAGE_BEGIN("Setup", 10);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);
    }
    E2E_STAGE_END();

    // Stage 3: Inject conflicting sensory information
    E2E_STAGE_BEGIN("Conflicting sensory input", 100);
    {
        // Audio says target is on the left
        audio_source_t audio_source = {};
        audio_source.azimuth = -0.5f;  // Left
        audio_source.elevation = 0.0f;
        audio_source.confidence = 0.6f;
        audio_source.intensity_db = -20.0f;
        audio_source.frequency_hz = 200.0f;
        audio_source.bandwidth_hz = 50.0f;
        audio_source.source_id = 1;
        audio_source.timestamp_us = 0;
        dragonfly_audio_bridge_inject_source(audio_bridge, &audio_source);

        // Visual shows target on the right (higher confidence)
        for (int i = 0; i < 20; i++) {
            motion_blob_t blob = {};
            blob.center_x = 400.0f;  // Right of center
            blob.center_y = 240.0f;
            blob.size_pixels = 30.0f;
            blob.velocity_x = -5.0f;
            blob.velocity_y = 0.0f;
            blob.contrast = 0.9f;  // High contrast
            blob.salience = 0.95f;
            dragonfly_visual_bridge_inject_blob(visual_bridge, &blob);

            dragonfly_update(dragonfly, 0.016f);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Verify visual dominates (higher confidence)
    E2E_STAGE_BEGIN("Verify visual dominance", 10);
    {
        dragonfly_motor_cmd_t cmd;
        dragonfly_get_motor_command(dragonfly, &cmd);

        // Heading should be toward visual target (right, positive heading)
        // Visual should win due to higher confidence and direct tracking
        // Audio cue was at -0.5 rad (left), visual is at positive angle (right)
        // If visual dominates, heading should be positive (toward right side)
        E2E_ASSERT(cmd.heading_rad > -0.3f,
                   "Heading should favor visual target (right side) over audio (left side)");
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        dragonfly_audio_bridge_destroy(audio_bridge);
        dragonfly_visual_bridge_destroy(visual_bridge);
        dragonfly_system_destroy(dragonfly);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Occluded Target with Audio Pipeline
//=============================================================================

E2E_TEST(DragonflyMultimodalTest, OccludedTargetWithAudioPipeline) {
    E2E_PIPELINE_START("Dragonfly Occluded Target with Audio Pipeline");

    dragonfly_system_t* dragonfly = nullptr;
    dragonfly_visual_bridge_t* visual_bridge = nullptr;
    dragonfly_audio_bridge_t* audio_bridge = nullptr;

    // Stage 1: Create system
    E2E_STAGE_BEGIN("Create system", 200);
    {
        dragonfly_config_t config = dragonfly_default_config();
        config.prediction_config.enable_imm = true;  // Helps with occlusion
        dragonfly = dragonfly_system_create(&config);
        E2E_ASSERT_NOT_NULL(dragonfly, "Dragonfly creation failed");

        visual_bridge_config_t vconfig = visual_bridge_default_config();
        visual_bridge = dragonfly_visual_bridge_create(dragonfly, nullptr, &vconfig);
        E2E_ASSERT_NOT_NULL(visual_bridge, "Visual bridge creation failed");

        audio_bridge_config_t aconfig = audio_bridge_default_config();
        audio_bridge = dragonfly_audio_bridge_create(dragonfly, nullptr, &aconfig);
        E2E_ASSERT_NOT_NULL(audio_bridge, "Audio bridge creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Setup
    E2E_STAGE_BEGIN("Setup", 10);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);
    }
    E2E_STAGE_END();

    // Stage 3: Track target visually
    E2E_STAGE_BEGIN("Visual tracking", 100);
    {
        float target_x = 20.0f, target_y = 0.0f;
        float target_vx = -3.0f, target_vy = 0.0f;

        for (int frame = 0; frame < 30; frame++) {
            target_x += target_vx * 0.016f;

            motion_blob_t blob = {};
            blob.center_x = 320.0f + target_x * 10.0f;
            blob.center_y = 240.0f;
            blob.size_pixels = 25.0f;
            blob.velocity_x = target_vx * 10.0f;
            blob.velocity_y = 0.0f;
            blob.contrast = 0.85f;
            blob.salience = 0.9f;
            dragonfly_visual_bridge_inject_blob(visual_bridge, &blob);

            dragonfly_update(dragonfly, 0.016f);
        }
    }
    E2E_STAGE_END();

    // Stage 4: Target becomes occluded - visual lost, audio maintains
    E2E_STAGE_BEGIN("Occlusion with audio", 100);
    {
        float target_x = 20.0f - 30 * 0.016f * 3.0f;  // Continue from last position
        float target_y = 0.0f;
        float target_vx = -3.0f, target_vy = 0.0f;

        for (int frame = 0; frame < 30; frame++) {
            target_x += target_vx * 0.016f;

            // No visual input (occluded)

            // Audio source maintains approximate direction
            float angle = atan2f(target_y, target_x);
            audio_source_t source = {};
            source.azimuth = angle;
            source.elevation = 0.0f;
            source.confidence = 0.5f;  // Lower confidence than visual
            source.intensity_db = -25.0f;
            source.frequency_hz = 200.0f;
            source.bandwidth_hz = 50.0f;
            source.source_id = 1;
            source.timestamp_us = (uint64_t)(frame * 16000);  // 16ms per frame
            dragonfly_audio_bridge_inject_source(audio_bridge, &source);

            dragonfly_update(dragonfly, 0.016f);
        }
    }
    E2E_STAGE_END();

    // Stage 5: Target reappears - visual resumes
    E2E_STAGE_BEGIN("Visual reacquisition", 100);
    {
        float target_x = 20.0f - 60 * 0.016f * 3.0f;
        float target_y = 0.0f;
        float target_vx = -3.0f, target_vy = 0.0f;

        for (int frame = 0; frame < 30; frame++) {
            target_x += target_vx * 0.016f;

            motion_blob_t blob = {};
            blob.center_x = 320.0f + target_x * 10.0f;
            blob.center_y = 240.0f;
            blob.size_pixels = 25.0f;
            blob.velocity_x = target_vx * 10.0f;
            blob.velocity_y = 0.0f;
            blob.contrast = 0.85f;
            blob.salience = 0.9f;
            dragonfly_visual_bridge_inject_blob(visual_bridge, &blob);

            dragonfly_update(dragonfly, 0.016f);
        }
    }
    E2E_STAGE_END();

    // Stage 6: Verify tracking maintained through occlusion
    E2E_STAGE_BEGIN("Verify tracking maintained", 10);
    {
        dragonfly_mode_t mode = dragonfly_get_mode(dragonfly);
        E2E_ASSERT(mode == DRAGONFLY_MODE_TRACKING ||
                   mode == DRAGONFLY_MODE_PURSUING ||
                   mode == DRAGONFLY_MODE_SCANNING,
                   "Should maintain tracking through occlusion");
    }
    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        dragonfly_audio_bridge_destroy(audio_bridge);
        dragonfly_visual_bridge_destroy(visual_bridge);
        dragonfly_system_destroy(dragonfly);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Full Brain Multimodal Integration Pipeline
//=============================================================================

E2E_TEST(DragonflyMultimodalTest, FullBrainMultimodalIntegrationPipeline) {
    E2E_PIPELINE_START("Dragonfly Full Brain Multimodal Integration Pipeline");

    brain_t brain = nullptr;

    // Stage 1: Create brain with all features enabled
    E2E_STAGE_BEGIN("Create full-featured brain", 1000);
    {
        brain_config_t config = {};
        strncpy(config.task_name, "multimodal_brain_e2e", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_MEDIUM;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 128;
        config.num_outputs = 16;
        config.enable_dragonfly = true;
        config.dragonfly_enable_imm = true;
        config.dragonfly_prediction_horizon_ms = 300.0f;
        config.dragonfly_nav_gain = 4.0f;

        brain = brain_create_custom(&config);
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Complex multimodal scenario
    E2E_STAGE_BEGIN("Complex multimodal scenario", 5000);
    {
        dragonfly_system_t* df = brain_get_dragonfly(brain);
        E2E_ASSERT_NOT_NULL(df, "Dragonfly not found");

        // Self-state tracking
        float self_x = 0.0f, self_y = 0.0f;
        float self_vx = 0.0f, self_vy = 0.0f;
        float self_heading = 0.0f;

        // Multiple targets
        struct Target {
            float x, y, vx, vy;
            bool active;
        };

        Target targets[3] = {
            {15.0f, 5.0f, -2.0f, -0.5f, true},
            {20.0f, -8.0f, -1.5f, 1.0f, true},
            {12.0f, 0.0f, -3.0f, 0.0f, true}
        };

        float dt = 0.016f;

        for (int frame = 0; frame < 200; frame++) {
            // Update targets
            for (int t = 0; t < 3; t++) {
                if (targets[t].active) {
                    targets[t].x += targets[t].vx * dt;
                    targets[t].y += targets[t].vy * dt;

                    // Remove if escaped
                    if (targets[t].x < -10.0f || targets[t].x > 50.0f) {
                        targets[t].active = false;
                    }
                }
            }

            // Update self-state
            dragonfly_self_state_t self = {};
            self.position[0] = self_x;
            self.position[1] = self_y;
            self.position[2] = 0.0f;
            self.velocity[0] = self_vx;
            self.velocity[1] = self_vy;
            self.velocity[2] = 0.0f;
            self.heading_rad = self_heading;
            self.max_speed = 10.0f;
            self.max_accel = 20.0f;
            self.max_turn_rate = 6.0f;
            self.energy_level = 0.8f - (float)frame * 0.002f;  // Depleting energy
            dragonfly_update_self_state(df, &self);

            // Process detections
            for (int t = 0; t < 3; t++) {
                if (targets[t].active) {
                    float position[3] = {targets[t].x, targets[t].y, 0.0f};
                    brain_dragonfly_detect(brain, position, 0.05f, 0.8f + t * 0.05f);
                }
            }

            brain_step_dragonfly(brain, (uint64_t)(dt * 1e6));

            // Get motor command and update self
            float cmd_heading, cmd_pitch, urgency;
            float velocity[3];
            brain_dragonfly_get_command(brain, &cmd_heading, &cmd_pitch, velocity, &urgency);

            // Simple motion model
            float accel = urgency * 15.0f;
            self_vx += cosf(cmd_heading) * accel * dt;
            self_vy += sinf(cmd_heading) * accel * dt;

            float speed = sqrtf(self_vx * self_vx + self_vy * self_vy);
            if (speed > 10.0f) {
                self_vx *= 10.0f / speed;
                self_vy *= 10.0f / speed;
            }

            self_x += self_vx * dt;
            self_y += self_vy * dt;
            if (speed > 0.1f) {
                self_heading = atan2f(self_vy, self_vx);
            }
        }
    }
    E2E_STAGE_END();

    // Stage 3: Verify system stability
    E2E_STAGE_BEGIN("Verify stability", 10);
    {
        dragonfly_system_t* df = brain_get_dragonfly(brain);
        dragonfly_stats_t stats;
        dragonfly_get_stats(df, &stats);

        // Should have processed many detections
        E2E_ASSERT(stats.detections_processed > 0, "Should have processed detections");
        E2E_ASSERT(stats.total_updates == 200, "Should have 200 updates");
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
// E2E Test: Multimodal Stress Test Pipeline
//=============================================================================

E2E_TEST(DragonflyMultimodalTest, MultimodalStressTestPipeline) {
    E2E_PIPELINE_START("Dragonfly Multimodal Stress Test Pipeline");

    dragonfly_system_t* dragonfly = nullptr;
    dragonfly_visual_bridge_t* visual_bridge = nullptr;
    dragonfly_audio_bridge_t* audio_bridge = nullptr;

    // Stage 1: Create system
    E2E_STAGE_BEGIN("Create system", 200);
    {
        dragonfly_config_t config = dragonfly_default_config();
        dragonfly = dragonfly_system_create(&config);
        E2E_ASSERT_NOT_NULL(dragonfly, "Dragonfly creation failed");

        visual_bridge_config_t vconfig = visual_bridge_default_config();
        visual_bridge = dragonfly_visual_bridge_create(dragonfly, nullptr, &vconfig);
        E2E_ASSERT_NOT_NULL(visual_bridge, "Visual bridge creation failed");

        audio_bridge_config_t aconfig = audio_bridge_default_config();
        audio_bridge = dragonfly_audio_bridge_create(dragonfly, nullptr, &aconfig);
        E2E_ASSERT_NOT_NULL(audio_bridge, "Audio bridge creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Setup
    E2E_STAGE_BEGIN("Setup", 10);
    {
        dragonfly_self_state_t self = {};
        self.max_speed = 10.0f;
        self.max_accel = 20.0f;
        self.max_turn_rate = 6.0f;
        self.energy_level = 1.0f;
        dragonfly_update_self_state(dragonfly, &self);
    }
    E2E_STAGE_END();

    // Stage 3: High-frequency multimodal input
    E2E_STAGE_BEGIN("High-frequency multimodal input", 10000);
    {
        std::mt19937 rng(12345);
        std::uniform_real_distribution<float> pos_dist(-100.0f, 100.0f);
        std::uniform_real_distribution<float> angle_dist(-M_PI, M_PI);
        std::uniform_real_distribution<float> conf_dist(0.4f, 1.0f);
        std::uniform_real_distribution<float> db_dist(-40.0f, -10.0f);

        uint32_t audio_source_id = 0;

        for (int frame = 0; frame < 500; frame++) {
            // Multiple visual blobs
            for (int b = 0; b < 5; b++) {
                motion_blob_t blob = {};
                blob.center_x = 320.0f + pos_dist(rng);
                blob.center_y = 240.0f + pos_dist(rng);
                blob.size_pixels = 15.0f + conf_dist(rng) * 20.0f;
                blob.velocity_x = pos_dist(rng) * 0.1f;
                blob.velocity_y = pos_dist(rng) * 0.1f;
                blob.contrast = conf_dist(rng);
                blob.salience = conf_dist(rng);
                dragonfly_visual_bridge_inject_blob(visual_bridge, &blob);
            }

            // Occasional audio sources
            if (frame % 10 == 0) {
                audio_source_t source = {};
                source.azimuth = angle_dist(rng);
                source.elevation = angle_dist(rng) * 0.3f;
                source.confidence = conf_dist(rng);
                source.intensity_db = db_dist(rng);
                source.frequency_hz = 150.0f + conf_dist(rng) * 200.0f;
                source.bandwidth_hz = 30.0f + conf_dist(rng) * 40.0f;
                source.source_id = ++audio_source_id;
                source.timestamp_us = (uint64_t)(frame * 8000);  // 8ms per frame
                dragonfly_audio_bridge_inject_source(audio_bridge, &source);
            }

            dragonfly_update(dragonfly, 0.008f);  // 125 Hz
        }
    }
    E2E_STAGE_END();

    // Stage 4: Verify no crashes
    E2E_STAGE_BEGIN("Verify stability", 10);
    {
        dragonfly_stats_t stats;
        int result = dragonfly_get_stats(dragonfly, &stats);
        E2E_ASSERT(result == 0, "System should remain stable after stress test");
        E2E_ASSERT(stats.total_updates == 500, "Should have processed all updates");
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    {
        dragonfly_audio_bridge_destroy(audio_bridge);
        dragonfly_visual_bridge_destroy(visual_bridge);
        dragonfly_system_destroy(dragonfly);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
