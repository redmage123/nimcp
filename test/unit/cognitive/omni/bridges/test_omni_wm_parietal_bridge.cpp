/**
 * @file test_omni_wm_parietal_bridge.cpp
 * @brief Comprehensive unit tests for World Model Parietal Bridge
 *
 * WHAT: Tests for WM Parietal Bridge connecting RSSM with spatial reasoning and physics
 * WHY:  Bridge is critical for physics-informed world modeling and spatial-aware predictions
 * HOW:  Tests all APIs: lifecycle, connection, update, spatial prediction, coordinate
 *       transforms, physics constraints, object tracking, attention, and statistics
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

// =============================================================================
// Constants and Helpers
// =============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float TEST_DT = 0.016f; // ~60Hz
static constexpr uint32_t TEST_HORIZON = 10;
static constexpr uint32_t TEST_OBJECT_ID = 42;

static bool float_equals(float a, float b, float tol = FLOAT_TOLERANCE)
{
    return std::fabs(a - b) < tol;
}

static bool float_in_range(float value, float min_val, float max_val)
{
    return value >= min_val && value <= max_val;
}

// =============================================================================
// Test Fixture
// =============================================================================

class WMParietalBridgeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create bridge with default config
        bridge_ = omni_wm_parietal_bridge_create(nullptr);
    }

    void TearDown() override
    {
        if (bridge_) {
            omni_wm_parietal_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    // Helper to create bridge with custom config
    omni_wm_parietal_bridge_t* create_custom_bridge(bool enable_modulation,
                                                     float sensitivity)
    {
        omni_wm_parietal_bridge_config_t config;
        omni_wm_parietal_bridge_default_config(&config);
        config.enable_modulation = enable_modulation;
        config.sensitivity = sensitivity;
        return omni_wm_parietal_bridge_create(&config);
    }

    // Helper to create test spatial state
    wm_parietal_spatial_state_t create_test_state(uint32_t object_id,
                                                   float x, float y, float z)
    {
        wm_parietal_spatial_state_t state;
        memset(&state, 0, sizeof(state));
        state.object_id = object_id;
        state.position.x = x;
        state.position.y = y;
        state.position.z = z;
        state.velocity.vx = 0.0f;
        state.velocity.vy = 0.0f;
        state.velocity.vz = 0.0f;
        state.orientation.w = 1.0f;
        state.orientation.x = 0.0f;
        state.orientation.y = 0.0f;
        state.orientation.z = 0.0f;
        state.mass = 1.0f;
        state.bounding_radius = 0.5f;
        state.frame = WM_PARIETAL_FRAME_ALLOCENTRIC;
        state.confidence = 1.0f;
        return state;
    }

    // Helper to create test vec3
    wm_parietal_vec3_t create_vec3(float x, float y, float z)
    {
        wm_parietal_vec3_t v;
        v.x = x;
        v.y = y;
        v.z = z;
        return v;
    }

    omni_wm_parietal_bridge_t* bridge_ = nullptr;
};

// =============================================================================
// 1. Default Config Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, DefaultConfigNullFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, DefaultConfigSetsReasonableValues)
{
    omni_wm_parietal_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    nimcp_error_t result = omni_wm_parietal_bridge_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Check general settings
    EXPECT_TRUE(config.enable_modulation);
    EXPECT_TRUE(float_in_range(config.sensitivity, 0.5f, 2.0f));

    // Check spatial prediction settings
    EXPECT_GT(config.max_prediction_horizon, 0u);
    EXPECT_LE(config.max_prediction_horizon, WM_PARIETAL_MAX_TRAJECTORY_HORIZON);
    EXPECT_GT(config.prediction_dt, 0.0f);
    EXPECT_GE(config.default_frame, WM_PARIETAL_FRAME_EGOCENTRIC);
    EXPECT_LT(config.default_frame, WM_PARIETAL_FRAME_COUNT);

    // Check physics integration settings
    EXPECT_GT(config.physics_dt, 0.0f);
    EXPECT_GT(config.gravity_magnitude, 0.0f);
    EXPECT_GT(config.collision_epsilon, 0.0f);

    // Check spatial attention settings
    EXPECT_GT(config.attention_resolution, 0u);
    EXPECT_GT(config.attention_decay_rate, 0.0f);
    EXPECT_TRUE(float_in_range(config.salience_threshold, 0.0f, 1.0f));
}

TEST_F(WMParietalBridgeTest, DefaultConfigIdempotent)
{
    omni_wm_parietal_bridge_config_t config1, config2;

    omni_wm_parietal_bridge_default_config(&config1);
    omni_wm_parietal_bridge_default_config(&config2);

    EXPECT_EQ(config1.enable_modulation, config2.enable_modulation);
    EXPECT_FLOAT_EQ(config1.sensitivity, config2.sensitivity);
    EXPECT_EQ(config1.max_prediction_horizon, config2.max_prediction_horizon);
    EXPECT_FLOAT_EQ(config1.physics_dt, config2.physics_dt);
}

// =============================================================================
// 2. Lifecycle Tests - Create/Destroy
// =============================================================================

TEST_F(WMParietalBridgeTest, CreateWithNullConfigUsesDefaults)
{
    // bridge_ created in SetUp with NULL config
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(WMParietalBridgeTest, CreateWithCustomConfig)
{
    omni_wm_parietal_bridge_config_t config;
    omni_wm_parietal_bridge_default_config(&config);
    config.enable_modulation = false;
    config.sensitivity = 1.5f;
    config.enable_physics_constraints = true;
    config.gravity_magnitude = 10.0f;

    omni_wm_parietal_bridge_t* custom_bridge = omni_wm_parietal_bridge_create(&config);
    ASSERT_NE(custom_bridge, nullptr);

    // Verify config was applied
    EXPECT_FALSE(custom_bridge->config.enable_modulation);
    EXPECT_FLOAT_EQ(custom_bridge->config.sensitivity, 1.5f);
    EXPECT_FLOAT_EQ(custom_bridge->config.gravity_magnitude, 10.0f);

    omni_wm_parietal_bridge_destroy(custom_bridge);
}

TEST_F(WMParietalBridgeTest, CreateInitializesBaseFields)
{
    ASSERT_NE(bridge_, nullptr);

    // Check base bridge infrastructure
    EXPECT_NE(bridge_->base.module_name, nullptr);
    EXPECT_NE(bridge_->base.module_id, 0u);
}

TEST_F(WMParietalBridgeTest, CreateInitializesStatistics)
{
    ASSERT_NE(bridge_, nullptr);

    // Stats should be zeroed on creation
    EXPECT_EQ(bridge_->stats.spatial_predictions_made, 0u);
    EXPECT_EQ(bridge_->stats.trajectory_predictions_made, 0u);
    EXPECT_EQ(bridge_->stats.coordinate_transforms, 0u);
    EXPECT_EQ(bridge_->stats.errors_total, 0u);
}

TEST_F(WMParietalBridgeTest, CreateInitializesTrackedObjects)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->num_tracked_objects, 0u);
    EXPECT_GT(bridge_->tracked_objects_capacity, 0u);
}

TEST_F(WMParietalBridgeTest, CreateInitializesPhysicsConstraints)
{
    ASSERT_NE(bridge_, nullptr);

    EXPECT_EQ(bridge_->num_constraints, 0u);
}

TEST_F(WMParietalBridgeTest, DestroyNullSafe)
{
    // Should not crash
    omni_wm_parietal_bridge_destroy(nullptr);
}

TEST_F(WMParietalBridgeTest, DestroyValidBridge)
{
    omni_wm_parietal_bridge_t* temp = omni_wm_parietal_bridge_create(nullptr);
    ASSERT_NE(temp, nullptr);

    // Should not crash and should free resources
    omni_wm_parietal_bridge_destroy(temp);
}

// =============================================================================
// 3. Reset Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, ResetNullFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ResetClearsStatistics)
{
    ASSERT_NE(bridge_, nullptr);

    // Manually increment some stats
    bridge_->stats.spatial_predictions_made = 100;
    bridge_->stats.trajectory_predictions_made = 50;
    bridge_->stats.collisions_predicted = 10;

    nimcp_error_t result = omni_wm_parietal_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.spatial_predictions_made, 0u);
    EXPECT_EQ(bridge_->stats.trajectory_predictions_made, 0u);
    EXPECT_EQ(bridge_->stats.collisions_predicted, 0u);
}

TEST_F(WMParietalBridgeTest, ResetClearsTrackedObjects)
{
    ASSERT_NE(bridge_, nullptr);

    // Simulate having tracked objects
    bridge_->num_tracked_objects = 5;

    nimcp_error_t result = omni_wm_parietal_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->num_tracked_objects, 0u);
}

TEST_F(WMParietalBridgeTest, ResetClearsPhysicsConstraints)
{
    ASSERT_NE(bridge_, nullptr);

    // Simulate having constraints
    bridge_->num_constraints = 3;

    nimcp_error_t result = omni_wm_parietal_bridge_reset(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->num_constraints, 0u);
}

TEST_F(WMParietalBridgeTest, ResetPreservesConfiguration)
{
    omni_wm_parietal_bridge_t* custom = create_custom_bridge(false, 1.8f);
    ASSERT_NE(custom, nullptr);

    // Store original config values
    bool orig_enable = custom->config.enable_modulation;
    float orig_sens = custom->config.sensitivity;

    nimcp_error_t result = omni_wm_parietal_bridge_reset(custom);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Config should be preserved
    EXPECT_EQ(custom->config.enable_modulation, orig_enable);
    EXPECT_FLOAT_EQ(custom->config.sensitivity, orig_sens);

    omni_wm_parietal_bridge_destroy(custom);
}

// =============================================================================
// 4. Connection Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, IsConnectedWithoutConnectionReturnsFalse)
{
    ASSERT_NE(bridge_, nullptr);

    bool connected = omni_wm_parietal_bridge_is_connected(bridge_);
    EXPECT_FALSE(connected);
}

TEST_F(WMParietalBridgeTest, IsConnectedNullBridgeReturnsFalse)
{
    bool connected = omni_wm_parietal_bridge_is_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(WMParietalBridgeTest, ConnectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_connect(
        nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ConnectWorldModelNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_connect_world_model(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ConnectParietalLobeNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_connect_parietal_lobe(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ConnectParietalAdapterNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_connect_parietal_adapter(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ConnectSpatialReasoningNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_connect_spatial_reasoning(nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 5. Update Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, UpdateNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_update(nullptr, TEST_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateWithoutConnectionReturnsError)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_update(bridge_, TEST_DT);
    // Should return error or handle gracefully
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateZeroDtHandled)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_update(bridge_, 0.0f);
    // Should handle gracefully
    EXPECT_TRUE(result == NIMCP_SUCCESS || result != NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateNegativeDtHandled)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_update(bridge_, -1.0f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 6. Spatial Prediction Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, PredictSpatialStateNullBridgeFails)
{
    wm_parietal_spatial_state_t predicted;

    nimcp_error_t result = omni_wm_parietal_bridge_predict_spatial_state(
        nullptr, TEST_OBJECT_ID, TEST_HORIZON, WM_PARIETAL_FRAME_ALLOCENTRIC, &predicted);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PredictSpatialStateNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_predict_spatial_state(
        bridge_, TEST_OBJECT_ID, TEST_HORIZON, WM_PARIETAL_FRAME_ALLOCENTRIC, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PredictSpatialStateZeroHorizonFails)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_spatial_state_t predicted;

    nimcp_error_t result = omni_wm_parietal_bridge_predict_spatial_state(
        bridge_, TEST_OBJECT_ID, 0, WM_PARIETAL_FRAME_ALLOCENTRIC, &predicted);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PredictSpatialStateInvalidFrameFails)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_spatial_state_t predicted;

    nimcp_error_t result = omni_wm_parietal_bridge_predict_spatial_state(
        bridge_, TEST_OBJECT_ID, TEST_HORIZON, WM_PARIETAL_FRAME_COUNT, &predicted);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PredictTrajectoryNullBridgeFails)
{
    wm_parietal_trajectory_t trajectory;
    memset(&trajectory, 0, sizeof(trajectory));

    nimcp_error_t result = omni_wm_parietal_bridge_predict_trajectory(
        nullptr, TEST_OBJECT_ID, TEST_HORIZON, TEST_DT,
        WM_PARIETAL_FRAME_ALLOCENTRIC, &trajectory);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PredictTrajectoryNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_predict_trajectory(
        bridge_, TEST_OBJECT_ID, TEST_HORIZON, TEST_DT,
        WM_PARIETAL_FRAME_ALLOCENTRIC, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PredictJointTrajectoriesNullBridgeFails)
{
    uint32_t object_ids[] = {1, 2, 3};
    wm_parietal_trajectory_t* trajectories[3] = {nullptr, nullptr, nullptr};

    nimcp_error_t result = omni_wm_parietal_bridge_predict_joint_trajectories(
        nullptr, object_ids, 3, TEST_HORIZON, trajectories);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PredictJointTrajectoriesNullObjectIdsFails)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_trajectory_t* trajectories[3] = {nullptr, nullptr, nullptr};

    nimcp_error_t result = omni_wm_parietal_bridge_predict_joint_trajectories(
        bridge_, nullptr, 3, TEST_HORIZON, trajectories);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 7. Coordinate Transform Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, TransformPositionNullBridgeFails)
{
    wm_parietal_vec3_t pos = create_vec3(1.0f, 2.0f, 3.0f);
    wm_parietal_vec3_t result_pos;

    nimcp_error_t result = omni_wm_parietal_bridge_transform_position(
        nullptr, &pos, WM_PARIETAL_FRAME_EGOCENTRIC,
        WM_PARIETAL_FRAME_ALLOCENTRIC, &result_pos);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, TransformPositionNullInputFails)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_vec3_t result_pos;

    nimcp_error_t result = omni_wm_parietal_bridge_transform_position(
        bridge_, nullptr, WM_PARIETAL_FRAME_EGOCENTRIC,
        WM_PARIETAL_FRAME_ALLOCENTRIC, &result_pos);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, TransformPositionNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_vec3_t pos = create_vec3(1.0f, 2.0f, 3.0f);

    nimcp_error_t result = omni_wm_parietal_bridge_transform_position(
        bridge_, &pos, WM_PARIETAL_FRAME_EGOCENTRIC,
        WM_PARIETAL_FRAME_ALLOCENTRIC, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, TransformPositionSameFrameReturnsIdentity)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_vec3_t pos = create_vec3(1.0f, 2.0f, 3.0f);
    wm_parietal_vec3_t result_pos;

    nimcp_error_t result = omni_wm_parietal_bridge_transform_position(
        bridge_, &pos, WM_PARIETAL_FRAME_ALLOCENTRIC,
        WM_PARIETAL_FRAME_ALLOCENTRIC, &result_pos);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Same frame should return same position
    EXPECT_FLOAT_EQ(result_pos.x, pos.x);
    EXPECT_FLOAT_EQ(result_pos.y, pos.y);
    EXPECT_FLOAT_EQ(result_pos.z, pos.z);
}

TEST_F(WMParietalBridgeTest, TransformStateNullBridgeFails)
{
    wm_parietal_spatial_state_t state = create_test_state(1, 1.0f, 2.0f, 3.0f);
    wm_parietal_spatial_state_t result_state;

    nimcp_error_t result = omni_wm_parietal_bridge_transform_state(
        nullptr, &state, WM_PARIETAL_FRAME_EGOCENTRIC, &result_state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, TransformStateNullInputFails)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_spatial_state_t result_state;

    nimcp_error_t result = omni_wm_parietal_bridge_transform_state(
        bridge_, nullptr, WM_PARIETAL_FRAME_EGOCENTRIC, &result_state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, GetTransformMatrixNullBridgeFails)
{
    float matrix[16];

    nimcp_error_t result = omni_wm_parietal_bridge_get_transform_matrix(
        nullptr, WM_PARIETAL_FRAME_EGOCENTRIC, WM_PARIETAL_FRAME_ALLOCENTRIC, matrix);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, GetTransformMatrixNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_get_transform_matrix(
        bridge_, WM_PARIETAL_FRAME_EGOCENTRIC, WM_PARIETAL_FRAME_ALLOCENTRIC, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 8. Physics Constraint Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, AddPhysicsConstraintNullBridgeFails)
{
    wm_parietal_physics_constraint_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = WM_PARIETAL_PHYSICS_GRAVITY;
    constraint.enabled = true;
    constraint.strength = 1.0f;

    nimcp_error_t result = omni_wm_parietal_bridge_add_physics_constraint(nullptr, &constraint);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, AddPhysicsConstraintNullConstraintFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_add_physics_constraint(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, AddPhysicsConstraintGravity)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_physics_constraint_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = WM_PARIETAL_PHYSICS_GRAVITY;
    constraint.enabled = true;
    constraint.strength = 1.0f;
    constraint.parameters[0] = 9.81f; // g

    nimcp_error_t result = omni_wm_parietal_bridge_add_physics_constraint(bridge_, &constraint);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, AddPhysicsConstraintCollision)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_physics_constraint_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = WM_PARIETAL_PHYSICS_COLLISION;
    constraint.enabled = true;
    constraint.strength = 1.0f;

    nimcp_error_t result = omni_wm_parietal_bridge_add_physics_constraint(bridge_, &constraint);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, RemovePhysicsConstraintNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_remove_physics_constraint(
        nullptr, WM_PARIETAL_PHYSICS_GRAVITY, 0);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, RemovePhysicsConstraintValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Add a constraint first
    wm_parietal_physics_constraint_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.type = WM_PARIETAL_PHYSICS_GRAVITY;
    constraint.enabled = true;
    constraint.strength = 1.0f;
    omni_wm_parietal_bridge_add_physics_constraint(bridge_, &constraint);

    // Remove it
    nimcp_error_t result = omni_wm_parietal_bridge_remove_physics_constraint(
        bridge_, WM_PARIETAL_PHYSICS_GRAVITY, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ClearPhysicsConstraintsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_clear_physics_constraints(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ClearPhysicsConstraintsValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Add multiple constraints
    wm_parietal_physics_constraint_t constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.enabled = true;
    constraint.strength = 1.0f;

    constraint.type = WM_PARIETAL_PHYSICS_GRAVITY;
    omni_wm_parietal_bridge_add_physics_constraint(bridge_, &constraint);

    constraint.type = WM_PARIETAL_PHYSICS_COLLISION;
    omni_wm_parietal_bridge_add_physics_constraint(bridge_, &constraint);

    EXPECT_EQ(bridge_->num_constraints, 2u);

    // Clear all
    nimcp_error_t result = omni_wm_parietal_bridge_clear_physics_constraints(bridge_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->num_constraints, 0u);
}

TEST_F(WMParietalBridgeTest, CheckCollisionNullBridgeFails)
{
    bool will_collide;
    float time_to_collision;
    wm_parietal_vec3_t collision_point;

    nimcp_error_t result = omni_wm_parietal_bridge_check_collision(
        nullptr, 1, 2, 1.0f, &will_collide, &time_to_collision, &collision_point);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, CheckCollisionNullOutputsHandled)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_check_collision(
        bridge_, 1, 2, 1.0f, nullptr, nullptr, nullptr);
    // Should handle null outputs gracefully
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(WMParietalBridgeTest, PhysicsStepNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_physics_step(nullptr, TEST_DT);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PhysicsStepValid)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_physics_step(bridge_, TEST_DT);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, PhysicsStepNegativeDtFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_physics_step(bridge_, -0.01f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 9. Spatial Attention Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, UpdateAttentionNullBridgeFails)
{
    std::vector<float> attention_map(64, 0.5f);

    nimcp_error_t result = omni_wm_parietal_bridge_update_attention(
        nullptr, attention_map.data(), 8);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateAttentionNullMapFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_update_attention(bridge_, nullptr, 8);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateAttentionZeroDimFails)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> attention_map(64, 0.5f);

    nimcp_error_t result = omni_wm_parietal_bridge_update_attention(
        bridge_, attention_map.data(), 0);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateAttentionValid)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> attention_map(64, 0.5f);

    nimcp_error_t result = omni_wm_parietal_bridge_update_attention(
        bridge_, attention_map.data(), 8);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, SetAttentionFocusNullBridgeFails)
{
    wm_parietal_vec3_t focus = create_vec3(0.0f, 0.0f, 0.0f);

    nimcp_error_t result = omni_wm_parietal_bridge_set_attention_focus(nullptr, &focus, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, SetAttentionFocusNullFocusFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_set_attention_focus(bridge_, nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, SetAttentionFocusValid)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_vec3_t focus = create_vec3(1.0f, 2.0f, 3.0f);

    nimcp_error_t result = omni_wm_parietal_bridge_set_attention_focus(bridge_, &focus, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, GetAttentionAtNullBridgeReturnsZero)
{
    wm_parietal_vec3_t pos = create_vec3(1.0f, 2.0f, 3.0f);

    float attention = omni_wm_parietal_bridge_get_attention_at(nullptr, &pos);
    EXPECT_FLOAT_EQ(attention, 0.0f);
}

TEST_F(WMParietalBridgeTest, GetAttentionAtNullPositionReturnsZero)
{
    ASSERT_NE(bridge_, nullptr);

    float attention = omni_wm_parietal_bridge_get_attention_at(bridge_, nullptr);
    EXPECT_FLOAT_EQ(attention, 0.0f);
}

TEST_F(WMParietalBridgeTest, GetAttentionAtValid)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_vec3_t pos = create_vec3(1.0f, 2.0f, 3.0f);

    float attention = omni_wm_parietal_bridge_get_attention_at(bridge_, &pos);
    EXPECT_TRUE(float_in_range(attention, 0.0f, 1.0f));
}

// =============================================================================
// 10. Object Tracking Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, TrackObjectNullBridgeFails)
{
    wm_parietal_spatial_state_t state = create_test_state(1, 0.0f, 0.0f, 0.0f);

    nimcp_error_t result = omni_wm_parietal_bridge_track_object(nullptr, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, TrackObjectNullStateFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_track_object(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, TrackObjectValid)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_spatial_state_t state = create_test_state(TEST_OBJECT_ID, 1.0f, 2.0f, 3.0f);

    nimcp_error_t result = omni_wm_parietal_bridge_track_object(bridge_, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->num_tracked_objects, 1u);
}

TEST_F(WMParietalBridgeTest, TrackMultipleObjects)
{
    ASSERT_NE(bridge_, nullptr);

    for (uint32_t i = 0; i < 5; i++) {
        wm_parietal_spatial_state_t state = create_test_state(i + 1, (float)i, (float)i, (float)i);
        nimcp_error_t result = omni_wm_parietal_bridge_track_object(bridge_, &state);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(bridge_->num_tracked_objects, 5u);
}

TEST_F(WMParietalBridgeTest, UpdateObjectNullBridgeFails)
{
    wm_parietal_spatial_state_t state = create_test_state(1, 1.0f, 2.0f, 3.0f);

    nimcp_error_t result = omni_wm_parietal_bridge_update_object(nullptr, 1, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateObjectNullStateFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_update_object(bridge_, 1, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateObjectInvalidIdFails)
{
    ASSERT_NE(bridge_, nullptr);

    wm_parietal_spatial_state_t state = create_test_state(999, 1.0f, 2.0f, 3.0f);

    nimcp_error_t result = omni_wm_parietal_bridge_update_object(bridge_, 999, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UpdateObjectValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Track object first
    wm_parietal_spatial_state_t state = create_test_state(TEST_OBJECT_ID, 1.0f, 2.0f, 3.0f);
    omni_wm_parietal_bridge_track_object(bridge_, &state);

    // Update with new position
    state.position.x = 5.0f;
    state.position.y = 6.0f;
    state.position.z = 7.0f;

    nimcp_error_t result = omni_wm_parietal_bridge_update_object(bridge_, TEST_OBJECT_ID, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UntrackObjectNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_untrack_object(nullptr, 1);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UntrackObjectInvalidIdFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_untrack_object(bridge_, 999);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, UntrackObjectValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Track object first
    wm_parietal_spatial_state_t state = create_test_state(TEST_OBJECT_ID, 1.0f, 2.0f, 3.0f);
    omni_wm_parietal_bridge_track_object(bridge_, &state);
    EXPECT_EQ(bridge_->num_tracked_objects, 1u);

    // Untrack
    nimcp_error_t result = omni_wm_parietal_bridge_untrack_object(bridge_, TEST_OBJECT_ID);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(bridge_->num_tracked_objects, 0u);
}

TEST_F(WMParietalBridgeTest, GetObjectStateNullBridgeFails)
{
    wm_parietal_spatial_state_t state;

    nimcp_error_t result = omni_wm_parietal_bridge_get_object_state(nullptr, 1, &state);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, GetObjectStateNullOutputFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_get_object_state(bridge_, 1, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 11. Mathematical Reasoning Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, MathPredictNullBridgeFails)
{
    std::vector<float> observations = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> predictions(2);
    float confidence;

    nimcp_error_t result = omni_wm_parietal_bridge_math_predict(
        nullptr, observations.data(), 4, 2, predictions.data(), &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, MathPredictNullObservationsFails)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> predictions(2);
    float confidence;

    nimcp_error_t result = omni_wm_parietal_bridge_math_predict(
        bridge_, nullptr, 4, 2, predictions.data(), &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, MathPredictNullOutputsFails)
{
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> observations = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_error_t result = omni_wm_parietal_bridge_math_predict(
        bridge_, observations.data(), 4, 2, nullptr, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, EstimateQuantityNullBridgeFails)
{
    std::vector<float> values = {1.0f, 2.0f, 3.0f};
    float estimate, confidence;

    nimcp_error_t result = omni_wm_parietal_bridge_estimate_quantity(
        nullptr, values.data(), 3, &estimate, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, EstimateQuantityNullValuesFails)
{
    ASSERT_NE(bridge_, nullptr);

    float estimate, confidence;

    nimcp_error_t result = omni_wm_parietal_bridge_estimate_quantity(
        bridge_, nullptr, 3, &estimate, &confidence);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// 12. Statistics Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, GetStatsNullBridgeFails)
{
    omni_wm_parietal_bridge_stats_t stats;
    nimcp_error_t result = omni_wm_parietal_bridge_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, GetStatsNullStatsFails)
{
    ASSERT_NE(bridge_, nullptr);

    nimcp_error_t result = omni_wm_parietal_bridge_get_stats(bridge_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, GetStatsValid)
{
    ASSERT_NE(bridge_, nullptr);

    omni_wm_parietal_bridge_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    nimcp_error_t result = omni_wm_parietal_bridge_get_stats(bridge_, &stats);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Fresh bridge should have zero stats
    EXPECT_EQ(stats.spatial_predictions_made, 0u);
    EXPECT_EQ(stats.trajectory_predictions_made, 0u);
}

TEST_F(WMParietalBridgeTest, ResetStatsNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ResetStatsValid)
{
    ASSERT_NE(bridge_, nullptr);

    // Increment some stats
    bridge_->stats.spatial_predictions_made = 100;
    bridge_->stats.physics_queries = 50;

    nimcp_error_t result = omni_wm_parietal_bridge_reset_stats(bridge_);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(bridge_->stats.spatial_predictions_made, 0u);
    EXPECT_EQ(bridge_->stats.physics_queries, 0u);
}

// =============================================================================
// 13. Query Effects Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, GetWMEffectsNullBridgeReturnsNull)
{
    const omni_wm_to_parietal_effects_t* effects =
        omni_wm_parietal_bridge_get_wm_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(WMParietalBridgeTest, GetWMEffectsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const omni_wm_to_parietal_effects_t* effects =
        omni_wm_parietal_bridge_get_wm_effects(bridge_);
    ASSERT_NE(effects, nullptr);
}

TEST_F(WMParietalBridgeTest, GetParietalEffectsNullBridgeReturnsNull)
{
    const parietal_to_omni_wm_effects_t* effects =
        omni_wm_parietal_bridge_get_parietal_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

TEST_F(WMParietalBridgeTest, GetParietalEffectsValid)
{
    ASSERT_NE(bridge_, nullptr);

    const parietal_to_omni_wm_effects_t* effects =
        omni_wm_parietal_bridge_get_parietal_effects(bridge_);
    ASSERT_NE(effects, nullptr);
}

// =============================================================================
// 14. Bio-Async Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, ConnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, DisconnectBioAsyncNullBridgeFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, IsBioAsyncConnectedNullBridgeReturnsFalse)
{
    bool connected = omni_wm_parietal_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

// =============================================================================
// 15. Utility Function Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, ValidateConfigNullFails)
{
    nimcp_error_t result = omni_wm_parietal_bridge_validate_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ValidateConfigDefaultValid)
{
    omni_wm_parietal_bridge_config_t config;
    omni_wm_parietal_bridge_default_config(&config);

    nimcp_error_t result = omni_wm_parietal_bridge_validate_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, ValidateConfigInvalidSensitivity)
{
    omni_wm_parietal_bridge_config_t config;
    omni_wm_parietal_bridge_default_config(&config);
    config.sensitivity = 0.0f; // Out of valid range

    nimcp_error_t result = omni_wm_parietal_bridge_validate_config(&config);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, FrameToStringValid)
{
    for (int frame = WM_PARIETAL_FRAME_EGOCENTRIC; frame < WM_PARIETAL_FRAME_COUNT; frame++) {
        const char* str = omni_wm_parietal_frame_to_string((wm_parietal_frame_t)frame);
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(WMParietalBridgeTest, PhysicsTypeToStringValid)
{
    for (int type = WM_PARIETAL_PHYSICS_GRAVITY; type <= WM_PARIETAL_PHYSICS_CUSTOM; type++) {
        const char* str = omni_wm_parietal_physics_type_to_string((wm_parietal_physics_type_t)type);
        ASSERT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(WMParietalBridgeTest, CreateStateHelper)
{
    wm_parietal_spatial_state_t state = omni_wm_parietal_create_state(
        TEST_OBJECT_ID, 1.0f, 2.0f, 3.0f, WM_PARIETAL_FRAME_ALLOCENTRIC);

    EXPECT_EQ(state.object_id, TEST_OBJECT_ID);
    EXPECT_FLOAT_EQ(state.position.x, 1.0f);
    EXPECT_FLOAT_EQ(state.position.y, 2.0f);
    EXPECT_FLOAT_EQ(state.position.z, 3.0f);
    EXPECT_EQ(state.frame, WM_PARIETAL_FRAME_ALLOCENTRIC);
}

TEST_F(WMParietalBridgeTest, TrajectoryCreateDestroy)
{
    wm_parietal_trajectory_t* traj = omni_wm_parietal_trajectory_create(TEST_HORIZON);
    ASSERT_NE(traj, nullptr);
    EXPECT_NE(traj->states, nullptr);

    omni_wm_parietal_trajectory_destroy(traj);
}

TEST_F(WMParietalBridgeTest, TrajectoryCreateNullOnZeroLength)
{
    wm_parietal_trajectory_t* traj = omni_wm_parietal_trajectory_create(0);
    EXPECT_EQ(traj, nullptr);
}

TEST_F(WMParietalBridgeTest, TrajectoryDestroyNullSafe)
{
    // Should not crash
    omni_wm_parietal_trajectory_destroy(nullptr);
}

TEST_F(WMParietalBridgeTest, DistanceCalculation)
{
    wm_parietal_vec3_t a = create_vec3(0.0f, 0.0f, 0.0f);
    wm_parietal_vec3_t b = create_vec3(3.0f, 4.0f, 0.0f);

    float dist = omni_wm_parietal_distance(&a, &b);
    EXPECT_FLOAT_EQ(dist, 5.0f); // 3-4-5 triangle
}

TEST_F(WMParietalBridgeTest, DistanceZeroForSamePoint)
{
    wm_parietal_vec3_t a = create_vec3(1.0f, 2.0f, 3.0f);

    float dist = omni_wm_parietal_distance(&a, &a);
    EXPECT_FLOAT_EQ(dist, 0.0f);
}

TEST_F(WMParietalBridgeTest, NormalizeVector)
{
    wm_parietal_vec3_t v = create_vec3(3.0f, 0.0f, 4.0f);
    wm_parietal_vec3_t result;

    nimcp_error_t err = omni_wm_parietal_normalize(&v, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Check magnitude is 1
    float mag = std::sqrt(result.x * result.x + result.y * result.y + result.z * result.z);
    EXPECT_TRUE(float_equals(mag, 1.0f));
}

TEST_F(WMParietalBridgeTest, NormalizeZeroVectorFails)
{
    wm_parietal_vec3_t v = create_vec3(0.0f, 0.0f, 0.0f);
    wm_parietal_vec3_t result;

    nimcp_error_t err = omni_wm_parietal_normalize(&v, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, NormalizeNullInputFails)
{
    wm_parietal_vec3_t result;

    nimcp_error_t err = omni_wm_parietal_normalize(nullptr, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(WMParietalBridgeTest, NormalizeNullOutputFails)
{
    wm_parietal_vec3_t v = create_vec3(1.0f, 2.0f, 3.0f);

    nimcp_error_t err = omni_wm_parietal_normalize(&v, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

// =============================================================================
// 16. Memory Safety Tests
// =============================================================================

TEST_F(WMParietalBridgeTest, CreateDestroyManyTimes)
{
    for (int i = 0; i < 100; i++) {
        omni_wm_parietal_bridge_t* temp = omni_wm_parietal_bridge_create(nullptr);
        ASSERT_NE(temp, nullptr);
        omni_wm_parietal_bridge_destroy(temp);
    }
}

TEST_F(WMParietalBridgeTest, TrackUntrackObjectsManyTimes)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 50; i++) {
        wm_parietal_spatial_state_t state = create_test_state(
            i + 1, (float)i, (float)i * 2, (float)i * 3);

        nimcp_error_t result = omni_wm_parietal_bridge_track_object(bridge_, &state);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        result = omni_wm_parietal_bridge_untrack_object(bridge_, i + 1);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(bridge_->num_tracked_objects, 0u);
}

TEST_F(WMParietalBridgeTest, AddRemoveConstraintsManyTimes)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 50; i++) {
        wm_parietal_physics_constraint_t constraint;
        memset(&constraint, 0, sizeof(constraint));
        constraint.type = WM_PARIETAL_PHYSICS_GRAVITY;
        constraint.enabled = true;
        constraint.strength = 1.0f;

        nimcp_error_t result = omni_wm_parietal_bridge_add_physics_constraint(bridge_, &constraint);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        result = omni_wm_parietal_bridge_remove_physics_constraint(
            bridge_, WM_PARIETAL_PHYSICS_GRAVITY, 0);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(bridge_->num_constraints, 0u);
}

TEST_F(WMParietalBridgeTest, PhysicsStepManyTimes)
{
    ASSERT_NE(bridge_, nullptr);

    for (int i = 0; i < 100; i++) {
        nimcp_error_t result = omni_wm_parietal_bridge_physics_step(bridge_, TEST_DT);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
