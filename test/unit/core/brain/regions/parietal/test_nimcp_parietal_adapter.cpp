/**
 * @file test_nimcp_parietal_adapter.cpp
 * @brief Unit tests for nimcp_parietal_adapter.c
 *
 * WHAT: Comprehensive unit tests for the parietal cortex adapter
 * WHY:  Ensure correct integration of somatosensory, spatial attention, and sensorimotor
 * HOW:  Use Google Test framework to test lifecycle, spatial processing, motor planning,
 *       coordinate transforms, and statistics tracking.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/parietal/nimcp_parietal_adapter.h"

// Test Fixture for Parietal Adapter
class ParietalAdapterTest : public ::testing::Test {
protected:
    parietal_adapter_t* adapter;
    parietal_cortex_config_t config;

    void SetUp() override {
        config = parietal_cortex_adapter_default_config();
        adapter = parietal_cortex_adapter_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create parietal adapter";
    }

    void TearDown() override {
        parietal_cortex_adapter_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to add a somatosensory input
    void add_touch_input(uint32_t region_id, float intensity) {
        parietal_cortex_somatosensory_input_t input;
        memset(&input, 0, sizeof(input));
        input.modality = PARIETAL_CORTEX_SOMATOSENSORY_TOUCH;
        input.body_region_id = region_id;
        input.intensity = intensity;
        input.timestamp_ms = 0.0;
        EXPECT_TRUE(parietal_cortex_add_somatosensory_input(adapter, &input));
    }

    // Helper to add a spatial target
    void add_target(uint32_t id, float x, float y, float z, float salience) {
        parietal_cortex_spatial_target_t target;
        memset(&target, 0, sizeof(target));
        target.target_id = id;
        target.position.x = x;
        target.position.y = y;
        target.position.z = z;
        target.salience = salience;
        target.frame = PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC;
        target.is_active = false;
        EXPECT_TRUE(parietal_cortex_add_spatial_target(adapter, &target));
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(ParietalAdapterTest, DefaultConfigHasReasonableValues) {
    parietal_cortex_config_t default_config = parietal_cortex_adapter_default_config();

    EXPECT_EQ(default_config.max_somatotopic_regions, PARIETAL_CORTEX_DEFAULT_MAX_SOMATOTOPIC_REGIONS);
    EXPECT_EQ(default_config.max_spatial_targets, PARIETAL_CORTEX_DEFAULT_MAX_SPATIAL_TARGETS);
    EXPECT_EQ(default_config.max_motor_plans, PARIETAL_CORTEX_DEFAULT_MAX_MOTOR_PLANS);
    EXPECT_EQ(default_config.receptive_field_size, PARIETAL_CORTEX_DEFAULT_RECEPTIVE_FIELD_SIZE);
    EXPECT_TRUE(default_config.enable_tactile_acuity);
    EXPECT_TRUE(default_config.enable_proprioception);
    EXPECT_TRUE(default_config.enable_reaching);
    EXPECT_TRUE(default_config.enable_grasping);
}

TEST_F(ParietalAdapterTest, CreateWithNullConfigUsesDefaults) {
    parietal_adapter_t* adapter_null = parietal_cortex_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);

    parietal_cortex_config_t retrieved;
    EXPECT_TRUE(parietal_cortex_get_config(adapter_null, &retrieved));
    EXPECT_EQ(retrieved.max_somatotopic_regions, PARIETAL_CORTEX_DEFAULT_MAX_SOMATOTOPIC_REGIONS);

    parietal_cortex_adapter_destroy(adapter_null);
}

TEST_F(ParietalAdapterTest, DestroyNullDoesNotCrash) {
    parietal_cortex_adapter_destroy(NULL);
    // Should not crash
}

TEST_F(ParietalAdapterTest, ResetClearsState) {
    // Add some inputs first
    add_touch_input(1, 0.8f);
    add_target(1, 0.5f, 0.5f, 0.0f, 0.9f);

    EXPECT_TRUE(parietal_cortex_adapter_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(parietal_cortex_get_status(adapter), PARIETAL_CORTEX_STATUS_IDLE);
    EXPECT_EQ(parietal_cortex_get_last_error(adapter), PARIETAL_CORTEX_ERROR_NONE);
}

TEST_F(ParietalAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(parietal_cortex_adapter_reset(NULL));
}

// ============================================================================
// SOMATOSENSORY PROCESSING TESTS
// ============================================================================

TEST_F(ParietalAdapterTest, AddSomatosensoryInputSuccess) {
    parietal_cortex_somatosensory_input_t input;
    memset(&input, 0, sizeof(input));
    input.modality = PARIETAL_CORTEX_SOMATOSENSORY_TOUCH;
    input.body_region_id = 1;
    input.intensity = 0.75f;
    input.timestamp_ms = 100.0;

    EXPECT_TRUE(parietal_cortex_add_somatosensory_input(adapter, &input));
    EXPECT_EQ(parietal_cortex_get_status(adapter), PARIETAL_CORTEX_STATUS_SOMATOSENSORY);
}

TEST_F(ParietalAdapterTest, GetBodyRegionActivation) {
    // Add touch input
    add_touch_input(42, 0.85f);

    // Get activation for that region
    float activation = parietal_cortex_get_body_region_activation(adapter, 42);
    EXPECT_FLOAT_EQ(activation, 0.85f);

    // Non-existent region should return 0
    activation = parietal_cortex_get_body_region_activation(adapter, 9999);
    EXPECT_FLOAT_EQ(activation, 0.0f);
}

TEST_F(ParietalAdapterTest, TwoPointDiscrimination) {
    // Add a body region
    add_touch_input(1, 0.5f);

    // Fingertip discrimination (should work at 2mm)
    EXPECT_TRUE(parietal_cortex_two_point_discrimination(adapter, 1, 3.0f));

    // Below threshold should fail
    EXPECT_FALSE(parietal_cortex_two_point_discrimination(adapter, 1, 1.0f));
}

TEST_F(ParietalAdapterTest, AddSomatosensoryInputNull) {
    EXPECT_FALSE(parietal_cortex_add_somatosensory_input(NULL, NULL));
    EXPECT_FALSE(parietal_cortex_add_somatosensory_input(adapter, NULL));
}

TEST_F(ParietalAdapterTest, MultipleModalityInputs) {
    // Test different sensory modalities
    parietal_cortex_somatosensory_input_t touch_input = {
        .modality = PARIETAL_CORTEX_SOMATOSENSORY_TOUCH,
        .body_region_id = 1,
        .intensity = 0.5f
    };
    parietal_cortex_somatosensory_input_t proprio_input = {
        .modality = PARIETAL_CORTEX_SOMATOSENSORY_PROPRIOCEPTION,
        .body_region_id = 2,
        .intensity = 0.7f
    };

    EXPECT_TRUE(parietal_cortex_add_somatosensory_input(adapter, &touch_input));
    EXPECT_TRUE(parietal_cortex_add_somatosensory_input(adapter, &proprio_input));

    // Check both regions are tracked
    EXPECT_FLOAT_EQ(parietal_cortex_get_body_region_activation(adapter, 1), 0.5f);
    EXPECT_FLOAT_EQ(parietal_cortex_get_body_region_activation(adapter, 2), 0.7f);
}

// ============================================================================
// SPATIAL ATTENTION TESTS
// ============================================================================

TEST_F(ParietalAdapterTest, AddSpatialTargetSuccess) {
    parietal_cortex_spatial_target_t target;
    memset(&target, 0, sizeof(target));
    target.target_id = 1;
    target.position.x = 0.5f;
    target.position.y = -0.3f;
    target.position.z = 0.0f;
    target.salience = 0.9f;
    target.frame = PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC;

    EXPECT_TRUE(parietal_cortex_add_spatial_target(adapter, &target));
}

TEST_F(ParietalAdapterTest, UpdateTargetPosition) {
    // Add target
    add_target(1, 0.5f, 0.5f, 0.0f, 0.8f);

    // Update position
    parietal_cortex_position_t new_pos = {0.7f, -0.2f, 0.0f};
    EXPECT_TRUE(parietal_cortex_update_target_position(adapter, 1, &new_pos));

    // Non-existent target should fail
    EXPECT_FALSE(parietal_cortex_update_target_position(adapter, 999, &new_pos));
}

TEST_F(ParietalAdapterTest, AttendToLocation) {
    // Add target
    add_target(1, 0.3f, 0.4f, 0.0f, 0.9f);

    // Attend to it
    EXPECT_TRUE(parietal_cortex_attend_to_location(adapter, 1, NULL));
    EXPECT_EQ(parietal_cortex_get_status(adapter), PARIETAL_CORTEX_STATUS_SPATIAL_ATTENTION);

    // Attend to position directly
    parietal_cortex_position_t pos = {0.0f, 0.0f, 0.0f};
    EXPECT_TRUE(parietal_cortex_attend_to_location(adapter, 0, &pos));
}

TEST_F(ParietalAdapterTest, CovertAttentionShift) {
    parietal_cortex_position_t new_focus = {0.2f, -0.1f, 0.0f};
    EXPECT_TRUE(parietal_cortex_covert_attention_shift(adapter, &new_focus, 50.0f));
}

TEST_F(ParietalAdapterTest, GetAttentionMap) {
    // Add some targets and attend
    add_target(1, 0.0f, 0.0f, 0.0f, 0.9f);
    parietal_cortex_attend_to_location(adapter, 1, NULL);

    parietal_cortex_attention_result_t result;
    EXPECT_TRUE(parietal_cortex_get_attention_map(adapter, &result));

    // Center should have highest attention
    // Attention map is 8x8, center is around index 27 (3,3) and 36 (4,4)
    float center_attention = result.attention_map[27];
    float edge_attention = result.attention_map[0];
    EXPECT_GT(center_attention, edge_attention);
}

// ============================================================================
// COORDINATE TRANSFORM TESTS
// ============================================================================

TEST_F(ParietalAdapterTest, TransformCoordinatesIdentity) {
    parietal_cortex_position_t input = {1.0f, 2.0f, 3.0f};
    parietal_cortex_position_t output;

    // Same frame should be near-identity
    EXPECT_TRUE(parietal_cortex_transform_coordinates(adapter, &input,
        PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC, PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC, &output));

    EXPECT_NEAR(output.x, input.x, 0.01f);
    EXPECT_NEAR(output.y, input.y, 0.01f);
    EXPECT_NEAR(output.z, input.z, 0.01f);
}

TEST_F(ParietalAdapterTest, TransformCoordinatesDifferentFrames) {
    parietal_cortex_position_t input = {1.0f, 0.0f, 0.0f};
    parietal_cortex_position_t output;

    EXPECT_TRUE(parietal_cortex_transform_coordinates(adapter, &input,
        PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC, PARIETAL_CORTEX_SPATIAL_FRAME_ALLOCENTRIC, &output));

    // Output should be different (transformed)
    // Initial transform is identity, so still same
    EXPECT_NEAR(output.x, input.x, 0.01f);
}

TEST_F(ParietalAdapterTest, TransformCoordinatesInvalidFrame) {
    parietal_cortex_position_t input = {1.0f, 0.0f, 0.0f};
    parietal_cortex_position_t output;

    EXPECT_FALSE(parietal_cortex_transform_coordinates(adapter, &input,
        (parietal_cortex_spatial_frame_t)99, PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC, &output));
}

// ============================================================================
// SENSORIMOTOR INTEGRATION TESTS
// ============================================================================

TEST_F(ParietalAdapterTest, PlanReachSuccess) {
    parietal_cortex_position_t target = {0.5f, 0.3f, 0.1f};
    parietal_cortex_motor_plan_t plan;

    EXPECT_TRUE(parietal_cortex_plan_reach(adapter, &target, 1 /* right hand */, &plan));
    EXPECT_EQ(plan.target_pos.x, target.x);
    EXPECT_EQ(plan.target_pos.y, target.y);
    EXPECT_EQ(plan.target_pos.z, target.z);
    EXPECT_FALSE(plan.requires_grasp);
    EXPECT_GT(plan.confidence, 0.0f);
}

TEST_F(ParietalAdapterTest, PlanGraspSuccess) {
    parietal_cortex_position_t target = {0.5f, 0.3f, 0.1f};
    parietal_cortex_motor_plan_t plan;

    EXPECT_TRUE(parietal_cortex_plan_grasp(adapter, &target, 0.05f /* 5cm object */, &plan));
    EXPECT_TRUE(plan.requires_grasp);
    EXPECT_GT(plan.grip_aperture, 0);
}

TEST_F(ParietalAdapterTest, ProcessIntegration) {
    // Add somatosensory input
    add_touch_input(1, 0.6f);

    // Add spatial target
    add_target(1, 0.4f, 0.2f, 0.0f, 0.8f);

    // Plan a reach
    parietal_cortex_position_t target = {0.4f, 0.2f, 0.0f};
    parietal_cortex_motor_plan_t plan;
    parietal_cortex_plan_reach(adapter, &target, 1, &plan);

    // Process integration
    parietal_cortex_integration_result_t result;
    EXPECT_TRUE(parietal_cortex_process_integration(adapter, &result));

    EXPECT_TRUE(result.has_touch_input);
    EXPECT_GT(result.active_body_regions, 0u);
    EXPECT_GT(result.motor_plan_count, 0u);
    EXPECT_TRUE(result.ready_for_execution);
}

TEST_F(ParietalAdapterTest, GetNextMotorPlan) {
    // Plan multiple reaches
    parietal_cortex_position_t targets[] = {
        {0.3f, 0.3f, 0.0f},
        {0.5f, -0.2f, 0.1f}
    };
    parietal_cortex_motor_plan_t plan;

    for (int i = 0; i < 2; i++) {
        parietal_cortex_plan_reach(adapter, &targets[i], 1, &plan);
    }

    // Get them back
    EXPECT_TRUE(parietal_cortex_get_next_motor_plan(adapter, &plan));
    EXPECT_FLOAT_EQ(plan.target_pos.x, 0.3f);

    EXPECT_TRUE(parietal_cortex_get_next_motor_plan(adapter, &plan));
    EXPECT_FLOAT_EQ(plan.target_pos.x, 0.5f);

    // No more plans
    EXPECT_FALSE(parietal_cortex_get_next_motor_plan(adapter, &plan));
}

// ============================================================================
// TRAINING TESTS
// ============================================================================

TEST_F(ParietalAdapterTest, TrainTransform) {
    // Enable training
    parietal_cortex_config_t train_config = parietal_cortex_adapter_default_config();
    train_config.enable_training = true;
    parietal_adapter_t* train_adapter = parietal_cortex_adapter_create(&train_config);
    ASSERT_NE(nullptr, train_adapter);

    parietal_cortex_position_t input = {1.0f, 0.0f, 0.0f};
    parietal_cortex_position_t target = {0.0f, 1.0f, 0.0f};  // 90 degree rotation

    EXPECT_TRUE(parietal_cortex_train_transform(train_adapter,
        PARIETAL_CORTEX_SPATIAL_FRAME_EGOCENTRIC, PARIETAL_CORTEX_SPATIAL_FRAME_ALLOCENTRIC,
        &input, &target, 0.0f));

    parietal_cortex_stats_t stats;
    parietal_cortex_get_stats(train_adapter, &stats);
    EXPECT_GT(stats.training_iterations, 0u);

    parietal_cortex_adapter_destroy(train_adapter);
}

TEST_F(ParietalAdapterTest, TrainReaching) {
    // Enable training
    parietal_cortex_config_t train_config = parietal_cortex_adapter_default_config();
    train_config.enable_training = true;
    parietal_adapter_t* train_adapter = parietal_cortex_adapter_create(&train_config);
    ASSERT_NE(nullptr, train_adapter);

    parietal_cortex_position_t target = {0.5f, 0.3f, 0.0f};
    parietal_cortex_motor_plan_t plan;
    parietal_cortex_plan_reach(train_adapter, &target, 1, &plan);

    // Simulate reach endpoint (with some error)
    parietal_cortex_position_t actual = {0.52f, 0.28f, 0.0f};
    EXPECT_TRUE(parietal_cortex_train_reaching(train_adapter, &plan, &actual, 0.0f));

    parietal_cortex_adapter_destroy(train_adapter);
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

static int g_motor_callback_count = 0;
static int g_attention_callback_count = 0;

static void test_motor_callback(const parietal_cortex_motor_plan_t* plan, void* user_data) {
    (void)plan;
    (void)user_data;
    g_motor_callback_count++;
}

static void test_attention_callback(const parietal_cortex_attention_result_t* attention, void* user_data) {
    (void)attention;
    (void)user_data;
    g_attention_callback_count++;
}

TEST_F(ParietalAdapterTest, MotorCallback) {
    g_motor_callback_count = 0;

    EXPECT_TRUE(parietal_cortex_set_motor_callback(adapter, test_motor_callback, NULL));

    parietal_cortex_position_t target = {0.5f, 0.3f, 0.0f};
    parietal_cortex_motor_plan_t plan;
    parietal_cortex_plan_reach(adapter, &target, 1, &plan);

    EXPECT_EQ(g_motor_callback_count, 1);
}

TEST_F(ParietalAdapterTest, AttentionCallback) {
    g_attention_callback_count = 0;

    EXPECT_TRUE(parietal_cortex_set_attention_callback(adapter, test_attention_callback, NULL));

    add_target(1, 0.5f, 0.5f, 0.0f, 0.9f);
    parietal_cortex_attend_to_location(adapter, 1, NULL);

    EXPECT_EQ(g_attention_callback_count, 1);
}

// ============================================================================
// STATUS AND DIAGNOSTICS TESTS
// ============================================================================

TEST_F(ParietalAdapterTest, StatusStrings) {
    EXPECT_STREQ(parietal_cortex_status_string(PARIETAL_CORTEX_STATUS_IDLE), "Idle");
    EXPECT_STREQ(parietal_cortex_status_string(PARIETAL_CORTEX_STATUS_SOMATOSENSORY), "Somatosensory processing");
    EXPECT_STREQ(parietal_cortex_status_string(PARIETAL_CORTEX_STATUS_SPATIAL_ATTENTION), "Spatial attention");
    EXPECT_STREQ(parietal_cortex_status_string(PARIETAL_CORTEX_STATUS_SENSORIMOTOR), "Sensorimotor integration");
    EXPECT_STREQ(parietal_cortex_status_string(PARIETAL_CORTEX_STATUS_ERROR), "Error");
}

TEST_F(ParietalAdapterTest, ErrorStrings) {
    EXPECT_STREQ(parietal_cortex_error_string(PARIETAL_CORTEX_ERROR_NONE), "No error");
    EXPECT_STREQ(parietal_cortex_error_string(PARIETAL_CORTEX_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(parietal_cortex_error_string(PARIETAL_CORTEX_ERROR_BUFFER_OVERFLOW), "Buffer overflow");
}

TEST_F(ParietalAdapterTest, GetStats) {
    // Generate some activity
    add_touch_input(1, 0.5f);
    add_target(1, 0.3f, 0.3f, 0.0f, 0.8f);
    parietal_cortex_attend_to_location(adapter, 1, NULL);

    parietal_cortex_stats_t stats;
    EXPECT_TRUE(parietal_cortex_get_stats(adapter, &stats));

    EXPECT_GT(stats.somatosensory_samples, 0u);
    EXPECT_GT(stats.spatial_targets_tracked, 0u);
    EXPECT_GT(stats.attention_shifts, 0u);
}

TEST_F(ParietalAdapterTest, GetConfig) {
    parietal_cortex_config_t retrieved;
    EXPECT_TRUE(parietal_cortex_get_config(adapter, &retrieved));

    EXPECT_EQ(retrieved.max_somatotopic_regions, config.max_somatotopic_regions);
    EXPECT_EQ(retrieved.enable_reaching, config.enable_reaching);
}

// ============================================================================
// SUB-MODULE ACCESS TESTS
// ============================================================================

TEST_F(ParietalAdapterTest, GetSubModules) {
    EXPECT_NE(nullptr, parietal_cortex_get_somatosensory_processor(adapter));
    EXPECT_NE(nullptr, parietal_cortex_get_spatial_attention_processor(adapter));
    EXPECT_NE(nullptr, parietal_cortex_get_sensorimotor_integrator(adapter));
}

TEST_F(ParietalAdapterTest, GetSubModulesNull) {
    EXPECT_EQ(nullptr, parietal_cortex_get_somatosensory_processor(NULL));
    EXPECT_EQ(nullptr, parietal_cortex_get_spatial_attention_processor(NULL));
    EXPECT_EQ(nullptr, parietal_cortex_get_sensorimotor_integrator(NULL));
}

// ============================================================================
// EDGE CASES AND ERROR HANDLING
// ============================================================================

TEST_F(ParietalAdapterTest, DisabledFeatures) {
    parietal_cortex_config_t minimal_config = parietal_cortex_adapter_default_config();
    minimal_config.enable_reaching = false;
    minimal_config.enable_grasping = false;
    minimal_config.enable_covert_attention = false;

    parietal_adapter_t* minimal = parietal_cortex_adapter_create(&minimal_config);
    ASSERT_NE(nullptr, minimal);

    // Reach should fail when disabled
    parietal_cortex_position_t target = {0.5f, 0.3f, 0.0f};
    parietal_cortex_motor_plan_t plan;
    EXPECT_FALSE(parietal_cortex_plan_reach(minimal, &target, 1, &plan));

    // Covert attention should fail when disabled
    parietal_cortex_position_t focus = {0.0f, 0.0f, 0.0f};
    EXPECT_FALSE(parietal_cortex_covert_attention_shift(minimal, &focus, 50.0f));

    parietal_cortex_adapter_destroy(minimal);
}

TEST_F(ParietalAdapterTest, NullInputHandling) {
    EXPECT_EQ(parietal_cortex_get_status(NULL), PARIETAL_CORTEX_STATUS_ERROR);
    EXPECT_EQ(parietal_cortex_get_last_error(NULL), PARIETAL_CORTEX_ERROR_INTERNAL);

    parietal_cortex_stats_t stats;
    EXPECT_FALSE(parietal_cortex_get_stats(NULL, &stats));
    EXPECT_FALSE(parietal_cortex_get_stats(adapter, NULL));

    parietal_cortex_config_t config_out;
    EXPECT_FALSE(parietal_cortex_get_config(NULL, &config_out));
    EXPECT_FALSE(parietal_cortex_get_config(adapter, NULL));
}
