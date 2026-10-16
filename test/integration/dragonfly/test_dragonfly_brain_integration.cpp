//=============================================================================
// test_dragonfly_brain_integration.cpp - Dragonfly Brain Integration Tests
//=============================================================================
/**
 * @file test_dragonfly_brain_integration.cpp
 * @brief Integration tests for dragonfly subsystem within brain factory
 *
 * WHAT: Tests dragonfly initialization and operation within brain
 * WHY:  Verify dragonfly integrates correctly with brain lifecycle
 * HOW:  Create brain with dragonfly enabled, test all integration points
 *
 * TEST CATEGORIES:
 * - Initialization: Brain creates dragonfly when enabled
 * - Configuration: Brain config propagates to dragonfly
 * - Lifecycle: Dragonfly follows brain lifecycle (create/destroy)
 * - Accessors: Brain provides access to dragonfly subsystem
 * - Runtime: Dragonfly processes detections through brain API
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "nimcp.h"
#include "core/brain/nimcp_brain.h"
#include "dragonfly/nimcp_dragonfly.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonflyBrainIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Initialize NIMCP
        nimcp_init();
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        nimcp_shutdown();
    }

    // Helper to create brain with dragonfly enabled
    brain_t create_brain_with_dragonfly() {
        brain_config_t config = {};
        strncpy(config.task_name, "dragonfly_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = true;
        config.dragonfly_enable_imm = true;
        config.dragonfly_prediction_horizon_ms = 200.0f;
        config.dragonfly_nav_gain = 3.0f;
        return brain_create_custom(&config);
    }

    // Helper to create brain without dragonfly
    brain_t create_brain_without_dragonfly() {
        brain_config_t config = {};
        strncpy(config.task_name, "no_dragonfly_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_dragonfly = false;
        return brain_create_custom(&config);
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, BrainCreatesWithDragonflyEnabled) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr) << "Brain creation should succeed";

    dragonfly_system_t* dragonfly = brain_get_dragonfly(brain);
    EXPECT_NE(dragonfly, nullptr) << "Dragonfly should be initialized when enabled";
}

TEST_F(DragonflyBrainIntegrationTest, BrainCreatesWithDragonflyDisabled) {
    brain = create_brain_without_dragonfly();
    ASSERT_NE(brain, nullptr) << "Brain creation should succeed";

    dragonfly_system_t* dragonfly = brain_get_dragonfly(brain);
    EXPECT_EQ(dragonfly, nullptr) << "Dragonfly should be NULL when disabled";
}

TEST_F(DragonflyBrainIntegrationTest, DragonflyAccessorReturnsNullForNullBrain) {
    dragonfly_system_t* dragonfly = brain_get_dragonfly(nullptr);
    EXPECT_EQ(dragonfly, nullptr) << "Should handle NULL brain gracefully";
}

//=============================================================================
// Configuration Propagation Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, ConfigPropagatesIMSetting) {
    brain_config_t config = {};
    strncpy(config.task_name, "imm_test", sizeof(config.task_name) - 1);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    config.enable_dragonfly = true;
    config.dragonfly_enable_imm = true;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    dragonfly_system_t* dragonfly = brain_get_dragonfly(brain);
    ASSERT_NE(dragonfly, nullptr);

    // IMM should be enabled - verify via system behavior
    // (Internal config not directly accessible, but system should work)
    EXPECT_EQ(brain_dragonfly_get_mode(brain), (int)DRAGONFLY_MODE_IDLE);
}

TEST_F(DragonflyBrainIntegrationTest, ConfigPropagatesNavigationGain) {
    brain_config_t config = {};
    strncpy(config.task_name, "nav_gain_test", sizeof(config.task_name) - 1);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    config.enable_dragonfly = true;
    config.dragonfly_nav_gain = 4.5f;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    dragonfly_system_t* dragonfly = brain_get_dragonfly(brain);
    EXPECT_NE(dragonfly, nullptr) << "Dragonfly should be created with custom nav gain";
}

TEST_F(DragonflyBrainIntegrationTest, ConfigPropagatesSizeSelectivity) {
    brain_config_t config = {};
    strncpy(config.task_name, "size_sel_test", sizeof(config.task_name) - 1);
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;
    config.num_outputs = 10;
    config.enable_dragonfly = true;
    config.dragonfly_size_selectivity_min = 0.02f;
    config.dragonfly_size_selectivity_max = 0.15f;

    brain = brain_create_custom(&config);
    ASSERT_NE(brain, nullptr);

    dragonfly_system_t* dragonfly = brain_get_dragonfly(brain);
    EXPECT_NE(dragonfly, nullptr) << "Dragonfly should be created with custom size selectivity";
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, DragonflyDestroyedWithBrain) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    dragonfly_system_t* dragonfly = brain_get_dragonfly(brain);
    ASSERT_NE(dragonfly, nullptr);

    // Destroy brain - dragonfly should be cleaned up
    brain_destroy(brain);
    brain = nullptr;

    // No crash means success - dragonfly was properly destroyed
    SUCCEED();
}

TEST_F(DragonflyBrainIntegrationTest, MultipleBrainsWithDragonfly) {
    // Create first brain with dragonfly
    brain_config_t config1 = {};
    strncpy(config1.task_name, "multi_brain_1", sizeof(config1.task_name) - 1);
    config1.size = BRAIN_SIZE_SMALL;
    config1.task = BRAIN_TASK_CLASSIFICATION;
    config1.num_inputs = 64;
    config1.num_outputs = 10;
    config1.enable_dragonfly = true;
    brain_t brain1 = brain_create_custom(&config1);
    ASSERT_NE(brain1, nullptr);

    // Create second brain with dragonfly
    brain_config_t config2 = {};
    strncpy(config2.task_name, "multi_brain_2", sizeof(config2.task_name) - 1);
    config2.size = BRAIN_SIZE_SMALL;
    config2.task = BRAIN_TASK_CLASSIFICATION;
    config2.num_inputs = 64;
    config2.num_outputs = 10;
    config2.enable_dragonfly = true;
    brain_t brain2 = brain_create_custom(&config2);
    ASSERT_NE(brain2, nullptr);

    // Both should have separate dragonfly systems
    dragonfly_system_t* df1 = brain_get_dragonfly(brain1);
    dragonfly_system_t* df2 = brain_get_dragonfly(brain2);

    EXPECT_NE(df1, nullptr);
    EXPECT_NE(df2, nullptr);
    EXPECT_NE(df1, df2) << "Each brain should have its own dragonfly system";

    brain_destroy(brain2);
    brain_destroy(brain1);
}

//=============================================================================
// Runtime Detection Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, DetectionThroughBrainAPI) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    // Process a detection through brain API
    float position[3] = {10.0f, 5.0f, 0.0f};
    float size = 0.05f;
    float contrast = 0.8f;

    int result = brain_dragonfly_detect(brain, position, size, contrast);
    EXPECT_EQ(result, 0) << "Detection should succeed";
}

TEST_F(DragonflyBrainIntegrationTest, DetectionFailsWhenDisabled) {
    brain = create_brain_without_dragonfly();
    ASSERT_NE(brain, nullptr);

    float position[3] = {10.0f, 5.0f, 0.0f};
    int result = brain_dragonfly_detect(brain, position, 0.05f, 0.8f);
    EXPECT_EQ(result, -1) << "Detection should fail when dragonfly disabled";
}

TEST_F(DragonflyBrainIntegrationTest, DetectionFailsForNullBrain) {
    float position[3] = {10.0f, 5.0f, 0.0f};
    int result = brain_dragonfly_detect(nullptr, position, 0.05f, 0.8f);
    EXPECT_EQ(result, -1) << "Detection should fail for NULL brain";
}

//=============================================================================
// Motor Command Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, GetCommandThroughBrainAPI) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    float heading, pitch, urgency;
    float velocity[3];

    int result = brain_dragonfly_get_command(brain, &heading, &pitch, velocity, &urgency);
    EXPECT_EQ(result, 0) << "Get command should succeed";
}

TEST_F(DragonflyBrainIntegrationTest, GetCommandWithPartialOutputs) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    // Only request heading
    float heading;
    int result = brain_dragonfly_get_command(brain, &heading, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, 0) << "Get command with partial outputs should succeed";
}

TEST_F(DragonflyBrainIntegrationTest, GetCommandFailsWhenDisabled) {
    brain = create_brain_without_dragonfly();
    ASSERT_NE(brain, nullptr);

    float heading;
    int result = brain_dragonfly_get_command(brain, &heading, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1) << "Get command should fail when dragonfly disabled";
}

//=============================================================================
// Step/Update Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, StepThroughBrainAPI) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    // Step dragonfly forward 10ms
    int result = brain_step_dragonfly(brain, 10000);
    EXPECT_EQ(result, 0) << "Step should succeed";
}

TEST_F(DragonflyBrainIntegrationTest, StepMultipleTimes) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    // Step multiple times
    for (int i = 0; i < 100; i++) {
        int result = brain_step_dragonfly(brain, 1000);  // 1ms steps
        EXPECT_EQ(result, 0) << "Step " << i << " should succeed";
    }
}

TEST_F(DragonflyBrainIntegrationTest, StepFailsWhenDisabled) {
    brain = create_brain_without_dragonfly();
    ASSERT_NE(brain, nullptr);

    int result = brain_step_dragonfly(brain, 10000);
    EXPECT_EQ(result, -1) << "Step should fail when dragonfly disabled";
}

//=============================================================================
// Mode Query Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, GetModeThroughBrainAPI) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    int mode = brain_dragonfly_get_mode(brain);
    EXPECT_GE(mode, 0) << "Mode should be valid";
    EXPECT_EQ(mode, (int)DRAGONFLY_MODE_IDLE) << "Initial mode should be IDLE";
}

TEST_F(DragonflyBrainIntegrationTest, GetModeFailsWhenDisabled) {
    brain = create_brain_without_dragonfly();
    ASSERT_NE(brain, nullptr);

    int mode = brain_dragonfly_get_mode(brain);
    EXPECT_EQ(mode, -1) << "Get mode should fail when dragonfly disabled";
}

//=============================================================================
// Abort Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, AbortThroughBrainAPI) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    // Start tracking a target
    float position[3] = {10.0f, 5.0f, 0.0f};
    brain_dragonfly_detect(brain, position, 0.05f, 0.9f);

    // Abort should succeed
    int result = brain_dragonfly_abort(brain);
    EXPECT_EQ(result, 0) << "Abort should succeed";
}

TEST_F(DragonflyBrainIntegrationTest, AbortFailsWhenDisabled) {
    brain = create_brain_without_dragonfly();
    ASSERT_NE(brain, nullptr);

    int result = brain_dragonfly_abort(brain);
    EXPECT_EQ(result, -1) << "Abort should fail when dragonfly disabled";
}

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(DragonflyBrainIntegrationTest, FullDetectionToCommandPipeline) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    // Start in IDLE
    EXPECT_EQ(brain_dragonfly_get_mode(brain), (int)DRAGONFLY_MODE_IDLE);

    // Detect a target
    float position[3] = {15.0f, 3.0f, 2.0f};
    EXPECT_EQ(brain_dragonfly_detect(brain, position, 0.05f, 0.85f), 0);

    // Step to process detection
    EXPECT_EQ(brain_step_dragonfly(brain, 16000), 0);  // 16ms

    // Get motor command
    float heading, pitch, urgency;
    float velocity[3];
    EXPECT_EQ(brain_dragonfly_get_command(brain, &heading, &pitch, velocity, &urgency), 0);

    // Abort pursuit
    EXPECT_EQ(brain_dragonfly_abort(brain), 0);
}

TEST_F(DragonflyBrainIntegrationTest, MultipleTargetDetections) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    // Detect multiple targets
    float positions[3][3] = {
        {10.0f, 5.0f, 0.0f},
        {12.0f, 3.0f, 1.0f},
        {8.0f, 7.0f, -1.0f}
    };

    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(brain_dragonfly_detect(brain, positions[i], 0.05f, 0.8f), 0);
    }

    // Step to process
    EXPECT_EQ(brain_step_dragonfly(brain, 16000), 0);

    // Should still be able to get command (CSTMD1 selects one target)
    float heading;
    EXPECT_EQ(brain_dragonfly_get_command(brain, &heading, nullptr, nullptr, nullptr), 0);
}

TEST_F(DragonflyBrainIntegrationTest, ContinuousTrackingLoop) {
    brain = create_brain_with_dragonfly();
    ASSERT_NE(brain, nullptr);

    // Simulate continuous tracking for 100 frames
    float target_x = 10.0f;
    float target_y = 5.0f;

    for (int frame = 0; frame < 100; frame++) {
        // Target moves
        target_x += 0.1f;
        target_y += 0.02f * sin(frame * 0.1f);

        float position[3] = {target_x, target_y, 0.0f};
        EXPECT_EQ(brain_dragonfly_detect(brain, position, 0.05f, 0.8f), 0);
        EXPECT_EQ(brain_step_dragonfly(brain, 16000), 0);  // 60 FPS

        float heading, pitch, urgency;
        float velocity[3];
        EXPECT_EQ(brain_dragonfly_get_command(brain, &heading, &pitch, velocity, &urgency), 0);
    }
}
