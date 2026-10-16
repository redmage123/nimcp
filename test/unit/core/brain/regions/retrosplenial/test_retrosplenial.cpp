/**
 * @file test_retrosplenial.cpp
 * @brief Unit tests for Retrosplenial Cortex (RSC) brain region
 * @version Phase 5: Memory Circuit
 * @date 2026-01-13
 *
 * Tests cover:
 * - Lifecycle (create, destroy, reset, update)
 * - Spatial reference frame transformation
 * - Contextual memory encoding
 * - Scene recognition
 * - Navigation support
 * - Landmark functions
 * - Imagination and planning
 * - Error handling
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/retrosplenial/nimcp_retrosplenial.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class RetrosplenialTest : public ::testing::Test {
protected:
    nimcp_retrosplenial_t* rsc = nullptr;

    void SetUp() override {
        nimcp_rsc_config_t config = nimcp_rsc_default_config();
        rsc = nimcp_rsc_create(&config);
        ASSERT_NE(rsc, nullptr);
    }

    void TearDown() override {
        if (rsc) {
            nimcp_rsc_destroy(rsc);
            rsc = nullptr;
        }
    }

    /* Helper to create test feature vectors */
    void createTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + sinf(i * 0.1f) * 0.5f;
        }
    }

    /* Helper to create test position */
    nimcp_rsc_position_t makePosition(float x, float y, float z) {
        nimcp_rsc_position_t pos;
        pos.x = x;
        pos.y = y;
        pos.z = z;
        return pos;
    }

    /* Helper to create test pose */
    nimcp_rsc_pose_t makePose(float x, float y, float z, float yaw, float pitch, float roll) {
        nimcp_rsc_pose_t pose;
        pose.position.x = x;
        pose.position.y = y;
        pose.position.z = z;
        pose.orientation.yaw = yaw;
        pose.orientation.pitch = pitch;
        pose.orientation.roll = roll;
        pose.confidence = 1.0f;
        pose.timestamp_us = 0;
        return pose;
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, CreateWithDefaultConfig) {
    nimcp_retrosplenial_t* r = nimcp_rsc_create(nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(nimcp_rsc_get_status(r), RSC_STATUS_IDLE);
    EXPECT_EQ(nimcp_rsc_get_last_error(r), RSC_OK);
    nimcp_rsc_destroy(r);
}

TEST_F(RetrosplenialTest, CreateWithCustomConfig) {
    nimcp_rsc_config_t config = nimcp_rsc_default_config();
    config.num_transform_neurons = 128;
    config.num_context_neurons = 256;
    config.num_scene_neurons = 128;
    config.num_hd_neurons = 30;
    config.num_landmark_neurons = 64;

    nimcp_retrosplenial_t* r = nimcp_rsc_create(&config);
    ASSERT_NE(r, nullptr);

    nimcp_rsc_config_t retrieved;
    EXPECT_EQ(nimcp_rsc_get_config(r, &retrieved), RSC_OK);
    EXPECT_EQ(retrieved.num_transform_neurons, 128u);
    EXPECT_EQ(retrieved.num_context_neurons, 256u);
    EXPECT_EQ(retrieved.num_scene_neurons, 128u);
    EXPECT_EQ(retrieved.num_hd_neurons, 30u);
    EXPECT_EQ(retrieved.num_landmark_neurons, 64u);

    nimcp_rsc_destroy(r);
}

TEST_F(RetrosplenialTest, DestroyNull) {
    /* Should not crash */
    nimcp_rsc_destroy(nullptr);
    SUCCEED();
}

TEST_F(RetrosplenialTest, Reset) {
    /* Perform some operations to change state */
    nimcp_rsc_position_t pos = makePosition(10.0f, 20.0f, 0.0f);
    nimcp_rsc_update_navigation(rsc, &pos, 1.0f, 1.0f, 0.1f);
    nimcp_rsc_update(rsc, 10.0f);

    /* Reset */
    EXPECT_EQ(nimcp_rsc_reset(rsc), RSC_OK);
    EXPECT_EQ(nimcp_rsc_get_status(rsc), RSC_STATUS_IDLE);
}

TEST_F(RetrosplenialTest, ResetNull) {
    EXPECT_EQ(nimcp_rsc_reset(nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, Update) {
    EXPECT_EQ(nimcp_rsc_update(rsc, 10.0f), RSC_OK);

    nimcp_rsc_stats_t stats;
    EXPECT_EQ(nimcp_rsc_get_stats(rsc, &stats), RSC_OK);
    EXPECT_EQ(stats.updates_processed, 1u);
}

TEST_F(RetrosplenialTest, UpdateNull) {
    EXPECT_EQ(nimcp_rsc_update(nullptr, 10.0f), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_rsc_update(rsc, 10.0f), RSC_OK);
    }

    nimcp_rsc_stats_t stats;
    EXPECT_EQ(nimcp_rsc_get_stats(rsc, &stats), RSC_OK);
    EXPECT_EQ(stats.updates_processed, 100u);
}

/*=============================================================================
 * DEFAULT CONFIG TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, DefaultConfigValues) {
    nimcp_rsc_config_t config = nimcp_rsc_default_config();

    EXPECT_EQ(config.num_transform_neurons, RSC_DEFAULT_TRANSFORM_NEURONS);
    EXPECT_EQ(config.num_context_neurons, RSC_DEFAULT_CONTEXT_NEURONS);
    EXPECT_EQ(config.num_scene_neurons, RSC_DEFAULT_SCENE_NEURONS);
    EXPECT_EQ(config.num_hd_neurons, RSC_DEFAULT_HD_NEURONS);
    EXPECT_EQ(config.num_landmark_neurons, RSC_DEFAULT_LANDMARK_NEURONS);
    EXPECT_EQ(config.spatial_dim, RSC_DEFAULT_SPATIAL_DIM);
    EXPECT_EQ(config.feature_dim, RSC_DEFAULT_FEATURE_DIM);
    EXPECT_EQ(config.context_dim, RSC_CONTEXT_DIM);
    EXPECT_EQ(config.scene_dim, RSC_SCENE_DIM);
}

/*=============================================================================
 * REFERENCE FRAME TRANSFORMATION TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, TransformPositionEgoToAllo) {
    nimcp_rsc_position_t input = makePosition(1.0f, 0.0f, 0.0f);
    nimcp_rsc_position_t output;

    nimcp_rsc_error_t err = nimcp_rsc_transform_position(
        rsc, &input, RSC_FRAME_EGOCENTRIC, RSC_FRAME_ALLOCENTRIC, &output);
    EXPECT_EQ(err, RSC_OK);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.transforms_computed, 1u);
}

TEST_F(RetrosplenialTest, TransformPositionAlloToEgo) {
    nimcp_rsc_position_t input = makePosition(10.0f, 10.0f, 0.0f);
    nimcp_rsc_position_t output;

    nimcp_rsc_error_t err = nimcp_rsc_transform_position(
        rsc, &input, RSC_FRAME_ALLOCENTRIC, RSC_FRAME_EGOCENTRIC, &output);
    EXPECT_EQ(err, RSC_OK);
}

TEST_F(RetrosplenialTest, TransformPositionNullInput) {
    nimcp_rsc_position_t output;
    EXPECT_EQ(nimcp_rsc_transform_position(rsc, nullptr, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, &output), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, TransformPositionNullOutput) {
    nimcp_rsc_position_t input = makePosition(1.0f, 0.0f, 0.0f);
    EXPECT_EQ(nimcp_rsc_transform_position(rsc, &input, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, TransformPositionNullRsc) {
    nimcp_rsc_position_t input = makePosition(1.0f, 0.0f, 0.0f);
    nimcp_rsc_position_t output;
    EXPECT_EQ(nimcp_rsc_transform_position(nullptr, &input, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, &output), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, TransformPose) {
    nimcp_rsc_pose_t input = makePose(1.0f, 2.0f, 0.0f, (float)M_PI/4, 0.0f, 0.0f);
    nimcp_rsc_pose_t output;

    nimcp_rsc_error_t err = nimcp_rsc_transform_pose(
        rsc, &input, RSC_FRAME_EGOCENTRIC, RSC_FRAME_ALLOCENTRIC, &output);
    EXPECT_EQ(err, RSC_OK);
}

TEST_F(RetrosplenialTest, TransformPoseNull) {
    nimcp_rsc_pose_t input = makePose(1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    nimcp_rsc_pose_t output;

    EXPECT_EQ(nimcp_rsc_transform_pose(nullptr, &input, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, &output), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_transform_pose(rsc, nullptr, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, &output), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_transform_pose(rsc, &input, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, CalibrateTransform) {
    nimcp_rsc_position_t ego = makePosition(1.0f, 0.0f, 0.0f);
    nimcp_rsc_position_t allo = makePosition(10.0f, 10.0f, 0.0f);

    EXPECT_EQ(nimcp_rsc_calibrate_transform(rsc, &ego, &allo, 0.0f), RSC_OK);
}

TEST_F(RetrosplenialTest, CalibrateTransformNull) {
    nimcp_rsc_position_t ego = makePosition(1.0f, 0.0f, 0.0f);
    nimcp_rsc_position_t allo = makePosition(10.0f, 10.0f, 0.0f);

    EXPECT_EQ(nimcp_rsc_calibrate_transform(nullptr, &ego, &allo, 0.0f), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_calibrate_transform(rsc, nullptr, &allo, 0.0f), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_calibrate_transform(rsc, &ego, nullptr, 0.0f), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetTransform) {
    nimcp_rsc_transform_t transform;

    EXPECT_EQ(nimcp_rsc_get_transform(rsc, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, &transform), RSC_OK);
    EXPECT_EQ(transform.source_frame, RSC_FRAME_EGOCENTRIC);
    EXPECT_EQ(transform.target_frame, RSC_FRAME_ALLOCENTRIC);
}

TEST_F(RetrosplenialTest, GetTransformNull) {
    nimcp_rsc_transform_t transform;

    EXPECT_EQ(nimcp_rsc_get_transform(nullptr, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, &transform), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_get_transform(rsc, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, nullptr), RSC_ERR_NULL_PTR);
}

/*=============================================================================
 * CONTEXT ENCODING TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, EncodeContext) {
    float spatial_features[64];
    float temporal_features[32];
    createTestFeatures(spatial_features, 64, 0.5f);
    createTestFeatures(temporal_features, 32, 0.3f);

    EXPECT_EQ(nimcp_rsc_encode_context(rsc, spatial_features, 64,
        temporal_features, 32), RSC_OK);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.contexts_encoded, 1u);
}

TEST_F(RetrosplenialTest, EncodeContextNullFeatures) {
    /* Null features should be allowed (optional) */
    EXPECT_EQ(nimcp_rsc_encode_context(rsc, nullptr, 0, nullptr, 0), RSC_OK);
}

TEST_F(RetrosplenialTest, EncodeContextNullRsc) {
    float features[64];
    EXPECT_EQ(nimcp_rsc_encode_context(nullptr, features, 64, nullptr, 0), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetContext) {
    /* First encode some context */
    float spatial_features[64];
    createTestFeatures(spatial_features, 64, 0.5f);
    nimcp_rsc_encode_context(rsc, spatial_features, 64, nullptr, 0);

    /* Then retrieve it */
    nimcp_rsc_context_t context;
    EXPECT_EQ(nimcp_rsc_get_context(rsc, &context), RSC_OK);
    EXPECT_GT(context.context_strength, 0.0f);
}

TEST_F(RetrosplenialTest, GetContextNull) {
    nimcp_rsc_context_t context;
    EXPECT_EQ(nimcp_rsc_get_context(nullptr, &context), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_get_context(rsc, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, RecallContext) {
    /* Encode multiple contexts first */
    for (int i = 0; i < 5; i++) {
        float spatial[64];
        createTestFeatures(spatial, 64, (float)i * 0.2f);
        nimcp_rsc_encode_context(rsc, spatial, 64, nullptr, 0);
        nimcp_rsc_update(rsc, 10.0f);
    }

    /* Recall with a cue */
    float cue[64];
    createTestFeatures(cue, 64, 0.4f);  /* Similar to index 2 */

    nimcp_rsc_context_t recalled;
    float similarity;
    EXPECT_EQ(nimcp_rsc_recall_context(rsc, cue, 64, &recalled, &similarity), RSC_OK);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_GE(stats.contexts_recalled, 1u);
}

TEST_F(RetrosplenialTest, RecallContextNull) {
    float cue[64];
    nimcp_rsc_context_t recalled;
    float similarity;

    EXPECT_EQ(nimcp_rsc_recall_context(nullptr, cue, 64, &recalled, &similarity),
        RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_recall_context(rsc, nullptr, 64, &recalled, &similarity),
        RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_recall_context(rsc, cue, 64, nullptr, &similarity),
        RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, UpdateContext) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    EXPECT_EQ(nimcp_rsc_update_context(rsc, RSC_CONTEXT_SPATIAL, features, 64, 0.5f), RSC_OK);
    EXPECT_EQ(nimcp_rsc_update_context(rsc, RSC_CONTEXT_TEMPORAL, features, 64, 0.3f), RSC_OK);
    EXPECT_EQ(nimcp_rsc_update_context(rsc, RSC_CONTEXT_ENVIRONMENTAL, features, 64, 0.4f), RSC_OK);
}

TEST_F(RetrosplenialTest, UpdateContextNull) {
    float features[64];
    EXPECT_EQ(nimcp_rsc_update_context(nullptr, RSC_CONTEXT_SPATIAL, features, 64, 0.5f),
        RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_update_context(rsc, RSC_CONTEXT_SPATIAL, nullptr, 64, 0.5f),
        RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, UpdateContextInvalidBlendFactor) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    /* Blend factor should be clamped - implementation clamps to [0, 1] */
    nimcp_rsc_error_t err = nimcp_rsc_update_context(rsc, RSC_CONTEXT_SPATIAL, features, 64, 1.5f);
    /* Implementation clamps, so returns OK */
    EXPECT_EQ(err, RSC_OK);
}

/*=============================================================================
 * SCENE RECOGNITION TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, ProcessScene) {
    float scene_features[256];
    createTestFeatures(scene_features, 256, 0.5f);

    EXPECT_EQ(nimcp_rsc_process_scene(rsc, scene_features, 256), RSC_OK);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.scenes_recognized, 1u);
}

TEST_F(RetrosplenialTest, ProcessSceneNull) {
    float features[256];
    EXPECT_EQ(nimcp_rsc_process_scene(nullptr, features, 256), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_process_scene(rsc, nullptr, 256), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, ProcessSceneZeroDim) {
    float features[256];
    /* Implementation accepts zero dim (gracefully handles as no-op) */
    EXPECT_EQ(nimcp_rsc_process_scene(rsc, features, 0), RSC_OK);
}

TEST_F(RetrosplenialTest, GetScene) {
    /* Process a scene first */
    float scene_features[256];
    createTestFeatures(scene_features, 256, 0.5f);
    nimcp_rsc_process_scene(rsc, scene_features, 256);

    /* Retrieve scene state */
    nimcp_rsc_scene_t scene;
    EXPECT_EQ(nimcp_rsc_get_scene(rsc, &scene), RSC_OK);
}

TEST_F(RetrosplenialTest, GetSceneNull) {
    nimcp_rsc_scene_t scene;
    EXPECT_EQ(nimcp_rsc_get_scene(nullptr, &scene), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_get_scene(rsc, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetFamiliarity) {
    /* Process same scene multiple times to build familiarity */
    float scene_features[256];
    createTestFeatures(scene_features, 256, 0.5f);

    for (int i = 0; i < 5; i++) {
        nimcp_rsc_process_scene(rsc, scene_features, 256);
        nimcp_rsc_update(rsc, 10.0f);
    }

    nimcp_rsc_familiarity_t familiarity;
    float score;
    EXPECT_EQ(nimcp_rsc_get_familiarity(rsc, &familiarity, &score), RSC_OK);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(RetrosplenialTest, GetFamiliarityNull) {
    nimcp_rsc_familiarity_t familiarity;
    float score;

    /* Only rsc null check returns error; null output params are gracefully skipped */
    EXPECT_EQ(nimcp_rsc_get_familiarity(nullptr, &familiarity, &score), RSC_ERR_NULL_PTR);
    /* Null familiarity/score are allowed (graceful no-op for those outputs) */
    EXPECT_EQ(nimcp_rsc_get_familiarity(rsc, nullptr, &score), RSC_OK);
    EXPECT_EQ(nimcp_rsc_get_familiarity(rsc, &familiarity, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, FamiliarityIncreasesWithExposure) {
    float scene_features[256];
    createTestFeatures(scene_features, 256, 0.5f);

    /* Get initial familiarity */
    nimcp_rsc_process_scene(rsc, scene_features, 256);
    nimcp_rsc_familiarity_t fam1;
    float score1;
    nimcp_rsc_get_familiarity(rsc, &fam1, &score1);

    /* Process same scene multiple times */
    for (int i = 0; i < 10; i++) {
        nimcp_rsc_process_scene(rsc, scene_features, 256);
        nimcp_rsc_update(rsc, 10.0f);
    }

    /* Familiarity should increase or stay same */
    nimcp_rsc_familiarity_t fam2;
    float score2;
    nimcp_rsc_get_familiarity(rsc, &fam2, &score2);
    EXPECT_GE(score2, score1);
}

/*=============================================================================
 * NAVIGATION SUPPORT TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, UpdateNavigation) {
    nimcp_rsc_position_t pos = makePosition(10.0f, 20.0f, 0.0f);

    EXPECT_EQ(nimcp_rsc_update_navigation(rsc, &pos, 1.57f, 1.0f, 0.1f), RSC_OK);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.navigation_updates, 1u);
}

TEST_F(RetrosplenialTest, UpdateNavigationNull) {
    nimcp_rsc_position_t pos = makePosition(10.0f, 20.0f, 0.0f);

    EXPECT_EQ(nimcp_rsc_update_navigation(nullptr, &pos, 0.0f, 0.0f, 0.0f), RSC_ERR_NULL_PTR);
    /* Null position is allowed (graceful no-op for position update) */
    EXPECT_EQ(nimcp_rsc_update_navigation(rsc, nullptr, 0.0f, 0.0f, 0.0f), RSC_OK);
}

TEST_F(RetrosplenialTest, IntegrateHeadDirection) {
    EXPECT_EQ(nimcp_rsc_integrate_head_direction(rsc, 1.57f, 0.9f), RSC_OK);
}

TEST_F(RetrosplenialTest, IntegrateHeadDirectionNull) {
    EXPECT_EQ(nimcp_rsc_integrate_head_direction(nullptr, 1.57f, 0.9f), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, IntegrateHeadDirectionLowConfidence) {
    /* Low confidence should still work but have less effect */
    EXPECT_EQ(nimcp_rsc_integrate_head_direction(rsc, 1.57f, 0.1f), RSC_OK);
}

TEST_F(RetrosplenialTest, SetNavigationGoal) {
    nimcp_rsc_position_t goal = makePosition(100.0f, 100.0f, 0.0f);

    EXPECT_EQ(nimcp_rsc_set_navigation_goal(rsc, &goal, 0.0f), RSC_OK);
}

TEST_F(RetrosplenialTest, SetNavigationGoalWithHeading) {
    nimcp_rsc_position_t goal = makePosition(100.0f, 100.0f, 0.0f);

    /* -1 heading means ignore heading */
    EXPECT_EQ(nimcp_rsc_set_navigation_goal(rsc, &goal, -1.0f), RSC_OK);
}

TEST_F(RetrosplenialTest, SetNavigationGoalNull) {
    nimcp_rsc_position_t goal = makePosition(100.0f, 100.0f, 0.0f);

    EXPECT_EQ(nimcp_rsc_set_navigation_goal(nullptr, &goal, 0.0f), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_set_navigation_goal(rsc, nullptr, 0.0f), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetNavigationGuidance) {
    /* Set current position and goal */
    nimcp_rsc_position_t pos = makePosition(0.0f, 0.0f, 0.0f);
    nimcp_rsc_update_navigation(rsc, &pos, 0.0f, 0.0f, 0.0f);

    nimcp_rsc_position_t goal = makePosition(100.0f, 0.0f, 0.0f);
    nimcp_rsc_set_navigation_goal(rsc, &goal, 0.0f);

    /* Get guidance */
    float bearing, distance, confidence;
    EXPECT_EQ(nimcp_rsc_get_navigation_guidance(rsc, &bearing, &distance, &confidence), RSC_OK);
    EXPECT_GT(distance, 0.0f);  /* Should be positive distance to goal */
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(RetrosplenialTest, GetNavigationGuidanceNull) {
    float bearing, distance, confidence;

    /* Only rsc null check returns error; null output params are gracefully skipped */
    EXPECT_EQ(nimcp_rsc_get_navigation_guidance(nullptr, &bearing, &distance, &confidence),
        RSC_ERR_NULL_PTR);
    /* Null output params are allowed (graceful no-op for those outputs) */
    EXPECT_EQ(nimcp_rsc_get_navigation_guidance(rsc, nullptr, &distance, &confidence), RSC_OK);
    EXPECT_EQ(nimcp_rsc_get_navigation_guidance(rsc, &bearing, nullptr, &confidence), RSC_OK);
    EXPECT_EQ(nimcp_rsc_get_navigation_guidance(rsc, &bearing, &distance, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, GetNavigationState) {
    /* Update navigation state */
    nimcp_rsc_position_t pos = makePosition(10.0f, 20.0f, 0.0f);
    nimcp_rsc_update_navigation(rsc, &pos, 1.57f, 1.0f, 0.1f);

    /* Retrieve state */
    nimcp_rsc_navigation_t nav_state;
    EXPECT_EQ(nimcp_rsc_get_navigation_state(rsc, &nav_state), RSC_OK);
    EXPECT_FLOAT_EQ(nav_state.current_pose.position.x, 10.0f);
    EXPECT_FLOAT_EQ(nav_state.current_pose.position.y, 20.0f);
}

TEST_F(RetrosplenialTest, GetNavigationStateNull) {
    nimcp_rsc_navigation_t nav_state;

    EXPECT_EQ(nimcp_rsc_get_navigation_state(nullptr, &nav_state), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_get_navigation_state(rsc, nullptr), RSC_ERR_NULL_PTR);
}

/*=============================================================================
 * LANDMARK TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, AddLandmark) {
    nimcp_rsc_position_t pos = makePosition(50.0f, 50.0f, 0.0f);
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    uint32_t landmark_id;
    EXPECT_EQ(nimcp_rsc_add_landmark(rsc, &pos, "TestLandmark", features, 64, &landmark_id), RSC_OK);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.active_landmarks, 1u);
}

TEST_F(RetrosplenialTest, AddLandmarkNull) {
    nimcp_rsc_position_t pos = makePosition(50.0f, 50.0f, 0.0f);
    uint32_t landmark_id;

    EXPECT_EQ(nimcp_rsc_add_landmark(nullptr, &pos, "Test", nullptr, 0, &landmark_id),
        RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_add_landmark(rsc, nullptr, "Test", nullptr, 0, &landmark_id),
        RSC_ERR_NULL_PTR);
    /* Null name is allowed (optional) */
    EXPECT_EQ(nimcp_rsc_add_landmark(rsc, &pos, nullptr, nullptr, 0, &landmark_id), RSC_OK);
}

TEST_F(RetrosplenialTest, AddMultipleLandmarks) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    for (int i = 0; i < 10; i++) {
        nimcp_rsc_position_t pos = makePosition((float)i * 10, (float)i * 10, 0.0f);
        char name[32];
        snprintf(name, sizeof(name), "Landmark%d", i);

        uint32_t landmark_id;
        EXPECT_EQ(nimcp_rsc_add_landmark(rsc, &pos, name, features, 64, &landmark_id), RSC_OK);
        EXPECT_EQ(landmark_id, (uint32_t)i);
    }

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.active_landmarks, 10u);
}

TEST_F(RetrosplenialTest, DetectLandmarks) {
    /* Add some landmarks first */
    float features[64];
    for (int i = 0; i < 5; i++) {
        nimcp_rsc_position_t pos = makePosition((float)i * 20, 0.0f, 0.0f);
        createTestFeatures(features, 64, (float)i * 0.1f);

        char name[32];
        snprintf(name, sizeof(name), "LM%d", i);

        uint32_t id;
        nimcp_rsc_add_landmark(rsc, &pos, name, features, 64, &id);
    }

    /* Detect landmarks in scene */
    float scene_features[256];
    createTestFeatures(scene_features, 256, 0.2f);

    uint32_t detected_ids[10];
    uint32_t num_detected;
    EXPECT_EQ(nimcp_rsc_detect_landmarks(rsc, scene_features, 256, detected_ids, 10, &num_detected),
        RSC_OK);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_GE(stats.landmarks_detected, num_detected);
}

TEST_F(RetrosplenialTest, DetectLandmarksNull) {
    float features[256];
    uint32_t ids[10];
    uint32_t num;

    EXPECT_EQ(nimcp_rsc_detect_landmarks(nullptr, features, 256, ids, 10, &num), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_detect_landmarks(rsc, nullptr, 256, ids, 10, &num), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_detect_landmarks(rsc, features, 256, nullptr, 10, &num), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_detect_landmarks(rsc, features, 256, ids, 10, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetLandmark) {
    /* Add a landmark */
    nimcp_rsc_position_t pos = makePosition(50.0f, 50.0f, 0.0f);
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    uint32_t landmark_id;
    nimcp_rsc_add_landmark(rsc, &pos, "TestLandmark", features, 64, &landmark_id);

    /* Retrieve it */
    nimcp_rsc_landmark_t landmark;
    EXPECT_EQ(nimcp_rsc_get_landmark(rsc, landmark_id, &landmark), RSC_OK);
    EXPECT_EQ(landmark.id, landmark_id);
    EXPECT_FLOAT_EQ(landmark.position.x, 50.0f);
    EXPECT_FLOAT_EQ(landmark.position.y, 50.0f);
    EXPECT_STREQ(landmark.name, "TestLandmark");
}

TEST_F(RetrosplenialTest, GetLandmarkNull) {
    nimcp_rsc_landmark_t landmark;

    EXPECT_EQ(nimcp_rsc_get_landmark(nullptr, 0, &landmark), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_get_landmark(rsc, 0, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetLandmarkInvalid) {
    nimcp_rsc_landmark_t landmark;

    EXPECT_NE(nimcp_rsc_get_landmark(rsc, 99999, &landmark), RSC_OK);
}

TEST_F(RetrosplenialTest, AnchorToLandmark) {
    /* Add a landmark */
    nimcp_rsc_position_t lm_pos = makePosition(100.0f, 0.0f, 0.0f);
    uint32_t landmark_id;
    nimcp_rsc_add_landmark(rsc, &lm_pos, "Anchor", nullptr, 0, &landmark_id);

    /* Set navigation position */
    nimcp_rsc_position_t pos = makePosition(0.0f, 0.0f, 0.0f);
    nimcp_rsc_update_navigation(rsc, &pos, 0.0f, 0.0f, 0.0f);

    /* Anchor to landmark (looking east at 100m distance) */
    EXPECT_EQ(nimcp_rsc_anchor_to_landmark(rsc, landmark_id, 0.0f, 100.0f), RSC_OK);
}

TEST_F(RetrosplenialTest, AnchorToLandmarkNull) {
    EXPECT_EQ(nimcp_rsc_anchor_to_landmark(nullptr, 0, 0.0f, 100.0f), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, AnchorToLandmarkInvalid) {
    EXPECT_NE(nimcp_rsc_anchor_to_landmark(rsc, 99999, 0.0f, 100.0f), RSC_OK);
}

/*=============================================================================
 * IMAGINATION AND PLANNING TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, StartImagination) {
    nimcp_rsc_pose_t target = makePose(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_EQ(nimcp_rsc_start_imagination(rsc, RSC_IMAGINE_PROSPECTIVE, &target, 60.0f), RSC_OK);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.imagination_episodes, 1u);
}

TEST_F(RetrosplenialTest, StartImaginationNullTarget) {
    /* Null target should be allowed for some modes */
    EXPECT_EQ(nimcp_rsc_start_imagination(rsc, RSC_IMAGINE_RETROSPECTIVE, nullptr, 10.0f), RSC_OK);
}

TEST_F(RetrosplenialTest, StartImaginationNull) {
    nimcp_rsc_pose_t target = makePose(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(nimcp_rsc_start_imagination(nullptr, RSC_IMAGINE_PROSPECTIVE, &target, 60.0f),
        RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, StepImagination) {
    nimcp_rsc_pose_t target = makePose(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    nimcp_rsc_start_imagination(rsc, RSC_IMAGINE_PROSPECTIVE, &target, 60.0f);

    /* Step through imagination */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nimcp_rsc_step_imagination(rsc, 100.0f), RSC_OK);
    }
}

TEST_F(RetrosplenialTest, StepImaginationNull) {
    EXPECT_EQ(nimcp_rsc_step_imagination(nullptr, 100.0f), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, StepImaginationNotActive) {
    /* Stepping imagination without starting returns INVALID_STATE */
    EXPECT_EQ(nimcp_rsc_step_imagination(rsc, 100.0f), RSC_ERR_INVALID_STATE);
}

TEST_F(RetrosplenialTest, StopImagination) {
    nimcp_rsc_pose_t target = makePose(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    nimcp_rsc_start_imagination(rsc, RSC_IMAGINE_PROSPECTIVE, &target, 60.0f);

    EXPECT_EQ(nimcp_rsc_stop_imagination(rsc), RSC_OK);

    /* Verify imagination is no longer active */
    nimcp_rsc_imagination_t state;
    nimcp_rsc_get_imagination_state(rsc, &state);
    EXPECT_FALSE(state.active);
}

TEST_F(RetrosplenialTest, StopImaginationNull) {
    EXPECT_EQ(nimcp_rsc_stop_imagination(nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetImaginationState) {
    nimcp_rsc_pose_t target = makePose(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    nimcp_rsc_start_imagination(rsc, RSC_IMAGINE_SPATIAL_SELF, &target, 0.0f);

    nimcp_rsc_imagination_t state;
    EXPECT_EQ(nimcp_rsc_get_imagination_state(rsc, &state), RSC_OK);
    EXPECT_TRUE(state.active);
    EXPECT_EQ(state.mode, RSC_IMAGINE_SPATIAL_SELF);
}

TEST_F(RetrosplenialTest, GetImaginationStateNull) {
    nimcp_rsc_imagination_t state;

    EXPECT_EQ(nimcp_rsc_get_imagination_state(nullptr, &state), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_get_imagination_state(rsc, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, ImaginationModes) {
    nimcp_rsc_pose_t target = makePose(100.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    /* Test all imagination modes */
    nimcp_rsc_imagine_mode_t modes[] = {
        RSC_IMAGINE_PROSPECTIVE,
        RSC_IMAGINE_RETROSPECTIVE,
        RSC_IMAGINE_COUNTERFACTUAL,
        RSC_IMAGINE_SPATIAL_SELF,
        RSC_IMAGINE_PERSPECTIVE_TAKING
    };

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_rsc_start_imagination(rsc, modes[i], &target, 10.0f), RSC_OK);

        nimcp_rsc_imagination_t state;
        nimcp_rsc_get_imagination_state(rsc, &state);
        EXPECT_EQ(state.mode, modes[i]);

        nimcp_rsc_stop_imagination(rsc);
    }
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, GetStatus) {
    EXPECT_EQ(nimcp_rsc_get_status(rsc), RSC_STATUS_IDLE);
}

TEST_F(RetrosplenialTest, GetStatusNull) {
    EXPECT_EQ(nimcp_rsc_get_status(nullptr), RSC_STATUS_ERROR);
}

TEST_F(RetrosplenialTest, GetLastError) {
    EXPECT_EQ(nimcp_rsc_get_last_error(rsc), RSC_OK);
}

TEST_F(RetrosplenialTest, GetLastErrorNull) {
    /* Implementation returns RSC_ERR_NULL_PTR (-1) for null rsc */
    EXPECT_EQ(nimcp_rsc_get_last_error(nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, ErrorStringMapping) {
    EXPECT_NE(nimcp_rsc_error_string(RSC_OK), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_INVALID_PARAM), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_ALREADY_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_NO_MEMORY), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_TRANSFORM_FAILED), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_CONTEXT_ENCODING_FAILED), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_SCENE_RECOGNITION_FAILED), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_NAVIGATION_FAILED), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_SECURITY_VIOLATION), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_IMMUNE_REJECTION), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_CAPACITY_EXCEEDED), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_INVALID_STATE), nullptr);
    EXPECT_NE(nimcp_rsc_error_string(RSC_ERR_INTERNAL), nullptr);
}

TEST_F(RetrosplenialTest, StatusStringMapping) {
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_IDLE), nullptr);
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_TRANSFORMING), nullptr);
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_ENCODING_CONTEXT), nullptr);
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_RECOGNIZING_SCENE), nullptr);
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_NAVIGATING), nullptr);
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_IMAGINING), nullptr);
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_RECALLING), nullptr);
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_READY), nullptr);
    EXPECT_NE(nimcp_rsc_status_string(RSC_STATUS_ERROR), nullptr);
}

TEST_F(RetrosplenialTest, FrameStringMapping) {
    EXPECT_NE(nimcp_rsc_frame_string(RSC_FRAME_EGOCENTRIC), nullptr);
    EXPECT_NE(nimcp_rsc_frame_string(RSC_FRAME_ALLOCENTRIC), nullptr);
    EXPECT_NE(nimcp_rsc_frame_string(RSC_FRAME_OBJECT_CENTERED), nullptr);
    EXPECT_NE(nimcp_rsc_frame_string(RSC_FRAME_ROUTE_CENTERED), nullptr);
}

TEST_F(RetrosplenialTest, ContextTypeStringMapping) {
    EXPECT_NE(nimcp_rsc_context_type_string(RSC_CONTEXT_SPATIAL), nullptr);
    EXPECT_NE(nimcp_rsc_context_type_string(RSC_CONTEXT_TEMPORAL), nullptr);
    EXPECT_NE(nimcp_rsc_context_type_string(RSC_CONTEXT_ENVIRONMENTAL), nullptr);
    EXPECT_NE(nimcp_rsc_context_type_string(RSC_CONTEXT_SOCIAL), nullptr);
    EXPECT_NE(nimcp_rsc_context_type_string(RSC_CONTEXT_EMOTIONAL), nullptr);
    EXPECT_NE(nimcp_rsc_context_type_string(RSC_CONTEXT_TASK), nullptr);
}

TEST_F(RetrosplenialTest, FamiliarityStringMapping) {
    EXPECT_NE(nimcp_rsc_familiarity_string(RSC_SCENE_NOVEL), nullptr);
    EXPECT_NE(nimcp_rsc_familiarity_string(RSC_SCENE_VAGUELY_FAMILIAR), nullptr);
    EXPECT_NE(nimcp_rsc_familiarity_string(RSC_SCENE_FAMILIAR), nullptr);
    EXPECT_NE(nimcp_rsc_familiarity_string(RSC_SCENE_VERY_FAMILIAR), nullptr);
    EXPECT_NE(nimcp_rsc_familiarity_string(RSC_SCENE_HIGHLY_FAMILIAR), nullptr);
}

TEST_F(RetrosplenialTest, ImagineModeStringMapping) {
    EXPECT_NE(nimcp_rsc_imagine_mode_string(RSC_IMAGINE_PROSPECTIVE), nullptr);
    EXPECT_NE(nimcp_rsc_imagine_mode_string(RSC_IMAGINE_RETROSPECTIVE), nullptr);
    EXPECT_NE(nimcp_rsc_imagine_mode_string(RSC_IMAGINE_COUNTERFACTUAL), nullptr);
    EXPECT_NE(nimcp_rsc_imagine_mode_string(RSC_IMAGINE_SPATIAL_SELF), nullptr);
    EXPECT_NE(nimcp_rsc_imagine_mode_string(RSC_IMAGINE_PERSPECTIVE_TAKING), nullptr);
}

TEST_F(RetrosplenialTest, BioMsgTypeStringMapping) {
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_CONTEXT), nullptr);
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_NAVIGATION), nullptr);
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_SCENE_FAMILIARITY), nullptr);
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_FRAME_TRANSFORM), nullptr);
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_LANDMARK_DETECTED), nullptr);
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_HEAD_DIRECTION), nullptr);
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_IMAGINATION_STATE), nullptr);
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_CONTEXT_REQUEST), nullptr);
    EXPECT_NE(nimcp_rsc_bio_msg_type_string(RSC_BIO_MSG_TRANSFORM_REQUEST), nullptr);
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, GetStatsInitial) {
    nimcp_rsc_stats_t stats;
    EXPECT_EQ(nimcp_rsc_get_stats(rsc, &stats), RSC_OK);

    EXPECT_EQ(stats.updates_processed, 0u);
    EXPECT_EQ(stats.transforms_computed, 0u);
    EXPECT_EQ(stats.contexts_encoded, 0u);
    EXPECT_EQ(stats.scenes_recognized, 0u);
    EXPECT_EQ(stats.landmarks_detected, 0u);
    EXPECT_EQ(stats.navigation_updates, 0u);
    EXPECT_EQ(stats.imagination_episodes, 0u);
}

TEST_F(RetrosplenialTest, GetStatsAfterOperations) {
    /* Perform various operations */
    nimcp_rsc_update(rsc, 10.0f);

    nimcp_rsc_position_t pos = makePosition(1.0f, 0.0f, 0.0f);
    nimcp_rsc_position_t out;
    nimcp_rsc_transform_position(rsc, &pos, RSC_FRAME_EGOCENTRIC, RSC_FRAME_ALLOCENTRIC, &out);

    float features[64];
    createTestFeatures(features, 64, 0.5f);
    nimcp_rsc_encode_context(rsc, features, 64, nullptr, 0);

    float scene[256];
    createTestFeatures(scene, 256, 0.5f);
    nimcp_rsc_process_scene(rsc, scene, 256);

    nimcp_rsc_update_navigation(rsc, &pos, 0.0f, 0.0f, 0.0f);

    /* Verify stats */
    nimcp_rsc_stats_t stats;
    EXPECT_EQ(nimcp_rsc_get_stats(rsc, &stats), RSC_OK);
    EXPECT_EQ(stats.updates_processed, 1u);
    EXPECT_EQ(stats.transforms_computed, 1u);
    EXPECT_EQ(stats.contexts_encoded, 1u);
    EXPECT_EQ(stats.scenes_recognized, 1u);
    EXPECT_EQ(stats.navigation_updates, 1u);
}

TEST_F(RetrosplenialTest, GetStatsNull) {
    nimcp_rsc_stats_t stats;
    EXPECT_EQ(nimcp_rsc_get_stats(nullptr, &stats), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_get_stats(rsc, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetConfig) {
    nimcp_rsc_config_t config;
    EXPECT_EQ(nimcp_rsc_get_config(rsc, &config), RSC_OK);
    EXPECT_EQ(config.num_transform_neurons, RSC_DEFAULT_TRANSFORM_NEURONS);
}

TEST_F(RetrosplenialTest, GetConfigNull) {
    nimcp_rsc_config_t config;
    EXPECT_EQ(nimcp_rsc_get_config(nullptr, &config), RSC_ERR_NULL_PTR);
    EXPECT_EQ(nimcp_rsc_get_config(rsc, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, GetHealthStatus) {
    /* Health is computed from immune_bridge.health_score which starts at 0.0f
     * (due to calloc), so initial health = 1.0f * 0.0f = 0.0f */
    float health = nimcp_rsc_get_health_status(rsc);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(RetrosplenialTest, GetHealthStatusNull) {
    EXPECT_FLOAT_EQ(nimcp_rsc_get_health_status(nullptr), 0.0f);
}

TEST_F(RetrosplenialTest, GetHealthStatusAfterImmuneInit) {
    /* Initialize immune bridge to set health_score to 1.0f */
    nimcp_rsc_init_immune_bridge(rsc, nullptr);
    float health = nimcp_rsc_get_health_status(rsc);
    EXPECT_GT(health, 0.0f);
    EXPECT_LE(health, 1.0f);
}

TEST_F(RetrosplenialTest, LogDiagnostics) {
    EXPECT_EQ(nimcp_rsc_log_diagnostics(rsc), RSC_OK);
}

TEST_F(RetrosplenialTest, LogDiagnosticsNull) {
    EXPECT_EQ(nimcp_rsc_log_diagnostics(nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, PrintSummary) {
    /* Should not crash */
    nimcp_rsc_print_summary(rsc);
    nimcp_rsc_print_summary(nullptr);
    SUCCEED();
}

/*=============================================================================
 * THREAD SAFETY TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, GetMutex) {
    nimcp_mutex_t* mutex = nimcp_rsc_get_mutex(rsc);
    /* Mutex may or may not be initialized depending on config */
    SUCCEED();
}

TEST_F(RetrosplenialTest, GetMutexNull) {
    EXPECT_EQ(nimcp_rsc_get_mutex(nullptr), nullptr);
}

TEST_F(RetrosplenialTest, LockUnlock) {
    EXPECT_EQ(nimcp_rsc_lock(rsc), RSC_OK);
    EXPECT_EQ(nimcp_rsc_unlock(rsc), RSC_OK);
}

TEST_F(RetrosplenialTest, LockNull) {
    EXPECT_EQ(nimcp_rsc_lock(nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, UnlockNull) {
    EXPECT_EQ(nimcp_rsc_unlock(nullptr), RSC_ERR_NULL_PTR);
}

/*=============================================================================
 * BIO-ASYNC MESSAGING TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, ProcessBioMessages) {
    int processed = nimcp_rsc_process_bio_messages(rsc, 0);
    EXPECT_GE(processed, 0);  /* Should be >= 0, negative means error */
}

TEST_F(RetrosplenialTest, ProcessBioMessagesNull) {
    EXPECT_LT(nimcp_rsc_process_bio_messages(nullptr, 0), 0);
}

TEST_F(RetrosplenialTest, BroadcastContext) {
    /* Encode context first */
    float features[64];
    createTestFeatures(features, 64, 0.5f);
    nimcp_rsc_encode_context(rsc, features, 64, nullptr, 0);

    EXPECT_EQ(nimcp_rsc_broadcast_context(rsc), RSC_OK);
}

TEST_F(RetrosplenialTest, BroadcastContextNull) {
    EXPECT_EQ(nimcp_rsc_broadcast_context(nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, BroadcastNavigation) {
    nimcp_rsc_position_t pos = makePosition(10.0f, 20.0f, 0.0f);
    nimcp_rsc_update_navigation(rsc, &pos, 1.0f, 1.0f, 0.0f);

    EXPECT_EQ(nimcp_rsc_broadcast_navigation(rsc), RSC_OK);
}

TEST_F(RetrosplenialTest, BroadcastNavigationNull) {
    EXPECT_EQ(nimcp_rsc_broadcast_navigation(nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, BroadcastFamiliarity) {
    /* Process a scene first */
    float scene[256];
    createTestFeatures(scene, 256, 0.5f);
    nimcp_rsc_process_scene(rsc, scene, 256);

    EXPECT_EQ(nimcp_rsc_broadcast_familiarity(rsc), RSC_OK);
}

TEST_F(RetrosplenialTest, BroadcastFamiliarityNull) {
    EXPECT_EQ(nimcp_rsc_broadcast_familiarity(nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, BroadcastLandmark) {
    /* Add a landmark first */
    nimcp_rsc_position_t pos = makePosition(50.0f, 50.0f, 0.0f);
    uint32_t landmark_id;
    nimcp_rsc_add_landmark(rsc, &pos, "Test", nullptr, 0, &landmark_id);

    EXPECT_EQ(nimcp_rsc_broadcast_landmark(rsc, landmark_id), RSC_OK);
}

TEST_F(RetrosplenialTest, BroadcastLandmarkNull) {
    EXPECT_EQ(nimcp_rsc_broadcast_landmark(nullptr, 0), RSC_ERR_NULL_PTR);
}

/*=============================================================================
 * BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, InitSecurityBridge) {
    EXPECT_EQ(nimcp_rsc_init_security_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitSecurityBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_security_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitImmuneBridge) {
    EXPECT_EQ(nimcp_rsc_init_immune_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitImmuneBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_immune_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitBioAsyncBridge) {
    EXPECT_EQ(nimcp_rsc_init_bio_async_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitBioAsyncBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_bio_async_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitKgBridge) {
    EXPECT_EQ(nimcp_rsc_init_kg_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitKgBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_kg_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitSnnBridge) {
    EXPECT_EQ(nimcp_rsc_init_snn_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitSnnBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_snn_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitLoggingBridge) {
    EXPECT_EQ(nimcp_rsc_init_logging_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitLoggingBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_logging_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitHippocampusBridge) {
    EXPECT_EQ(nimcp_rsc_init_hippocampus_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitHippocampusBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_hippocampus_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitEntorhinalBridge) {
    EXPECT_EQ(nimcp_rsc_init_entorhinal_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitEntorhinalBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_entorhinal_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitParietalBridge) {
    EXPECT_EQ(nimcp_rsc_init_parietal_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitParietalBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_parietal_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitThalamicBridge) {
    EXPECT_EQ(nimcp_rsc_init_thalamic_bridge(rsc, nullptr), RSC_OK);
}

TEST_F(RetrosplenialTest, InitThalamicBridgeNull) {
    EXPECT_EQ(nimcp_rsc_init_thalamic_bridge(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitAllBridgesNullBrain) {
    /* Implementation requires both rsc AND brain to be non-NULL */
    EXPECT_EQ(nimcp_rsc_init_all_bridges(rsc, nullptr), RSC_ERR_NULL_PTR);
}

TEST_F(RetrosplenialTest, InitAllBridgesNullRsc) {
    EXPECT_EQ(nimcp_rsc_init_all_bridges(nullptr, nullptr), RSC_ERR_NULL_PTR);
}

/*=============================================================================
 * INTEGRATION SCENARIO TESTS
 *===========================================================================*/

TEST_F(RetrosplenialTest, NavigationScenario) {
    /* Initialize bridges */
    nimcp_rsc_init_hippocampus_bridge(rsc, nullptr);
    nimcp_rsc_init_entorhinal_bridge(rsc, nullptr);
    nimcp_rsc_init_thalamic_bridge(rsc, nullptr);

    /* Set starting position */
    nimcp_rsc_position_t start = makePosition(0.0f, 0.0f, 0.0f);
    nimcp_rsc_update_navigation(rsc, &start, 0.0f, 0.0f, 0.0f);

    /* Set goal */
    nimcp_rsc_position_t goal = makePosition(100.0f, 100.0f, 0.0f);
    nimcp_rsc_set_navigation_goal(rsc, &goal, 0.0f);

    /* Simulate navigation */
    float x = 0.0f, y = 0.0f;
    for (int i = 0; i < 50; i++) {
        /* Move toward goal */
        x += 2.0f;
        y += 2.0f;

        nimcp_rsc_position_t pos = makePosition(x, y, 0.0f);
        nimcp_rsc_update_navigation(rsc, &pos, 0.785f, 1.0f, 0.0f);

        /* Integrate head direction */
        nimcp_rsc_integrate_head_direction(rsc, 0.785f, 0.9f);

        /* Update system */
        nimcp_rsc_update(rsc, 10.0f);
    }

    /* Check we got close to goal */
    float bearing, distance, confidence;
    nimcp_rsc_get_navigation_guidance(rsc, &bearing, &distance, &confidence);
    EXPECT_LT(distance, 20.0f);  /* Should be within 20 units of goal */
}

TEST_F(RetrosplenialTest, SpatialContextEncodingScenario) {
    /* Visit multiple locations and encode context at each */
    nimcp_rsc_position_t locations[5] = {
        {0.0f, 0.0f, 0.0f},
        {50.0f, 0.0f, 0.0f},
        {50.0f, 50.0f, 0.0f},
        {0.0f, 50.0f, 0.0f},
        {25.0f, 25.0f, 0.0f}
    };

    for (int i = 0; i < 5; i++) {
        /* Move to location */
        nimcp_rsc_update_navigation(rsc, &locations[i], (float)i * 0.5f, 1.0f, 0.0f);

        /* Encode context */
        float spatial[64];
        createTestFeatures(spatial, 64, (float)i * 0.2f);
        nimcp_rsc_encode_context(rsc, spatial, 64, nullptr, 0);

        /* Process scene */
        float scene[256];
        createTestFeatures(scene, 256, (float)i * 0.1f);
        nimcp_rsc_process_scene(rsc, scene, 256);

        nimcp_rsc_update(rsc, 100.0f);
    }

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.contexts_encoded, 5u);
    EXPECT_EQ(stats.scenes_recognized, 5u);
    EXPECT_EQ(stats.navigation_updates, 5u);
}

TEST_F(RetrosplenialTest, ReferenceFrameTransformScenario) {
    /* Calibrate transform with multiple observations */
    for (int i = 0; i < 10; i++) {
        float angle = (float)i * M_PI / 5;
        float ego_x = cosf(angle) * 1.0f;
        float ego_y = sinf(angle) * 1.0f;

        /* In allocentric frame, agent is at (50, 50) */
        float allo_x = 50.0f + cosf(angle) * 1.0f;
        float allo_y = 50.0f + sinf(angle) * 1.0f;

        nimcp_rsc_position_t ego = makePosition(ego_x, ego_y, 0.0f);
        nimcp_rsc_position_t allo = makePosition(allo_x, allo_y, 0.0f);

        nimcp_rsc_calibrate_transform(rsc, &ego, &allo, angle);
    }

    /* Test transformation */
    nimcp_rsc_position_t ego_input = makePosition(1.0f, 0.0f, 0.0f);
    nimcp_rsc_position_t allo_output;

    nimcp_rsc_transform_position(rsc, &ego_input, RSC_FRAME_EGOCENTRIC,
        RSC_FRAME_ALLOCENTRIC, &allo_output);

    /* Output should be roughly (51, 50) - allowing for learning imprecision */
    nimcp_rsc_transform_t transform;
    nimcp_rsc_get_transform(rsc, RSC_FRAME_EGOCENTRIC, RSC_FRAME_ALLOCENTRIC, &transform);
    EXPECT_GT(transform.accuracy, 0.0f);
}

TEST_F(RetrosplenialTest, LandmarkNavigationScenario) {
    /* Add landmarks around environment */
    nimcp_rsc_position_t lm_positions[4] = {
        {0.0f, 100.0f, 0.0f},
        {100.0f, 100.0f, 0.0f},
        {100.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };
    const char* lm_names[4] = {"North", "East", "South", "West"};
    uint32_t lm_ids[4];

    for (int i = 0; i < 4; i++) {
        float features[64];
        createTestFeatures(features, 64, (float)i * 0.25f);
        nimcp_rsc_add_landmark(rsc, &lm_positions[i], lm_names[i], features, 64, &lm_ids[i]);
    }

    /* Navigate using landmarks */
    nimcp_rsc_position_t pos = makePosition(50.0f, 50.0f, 0.0f);
    nimcp_rsc_update_navigation(rsc, &pos, 0.0f, 0.0f, 0.0f);

    /* Anchor to landmark */
    nimcp_rsc_anchor_to_landmark(rsc, lm_ids[0], (float)M_PI/2, 50.0f);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.active_landmarks, 4u);
}

TEST_F(RetrosplenialTest, ImaginationPlanningScenario) {
    /* Set current position */
    nimcp_rsc_position_t current = makePosition(0.0f, 0.0f, 0.0f);
    nimcp_rsc_update_navigation(rsc, &current, 0.0f, 0.0f, 0.0f);

    /* Encode current context */
    float context[64];
    createTestFeatures(context, 64, 0.5f);
    nimcp_rsc_encode_context(rsc, context, 64, nullptr, 0);

    /* Imagine being at goal */
    nimcp_rsc_pose_t goal = makePose(100.0f, 100.0f, 0.0f, 0.785f, 0.0f, 0.0f);
    nimcp_rsc_start_imagination(rsc, RSC_IMAGINE_SPATIAL_SELF, &goal, 0.0f);

    /* Step through imagination */
    for (int i = 0; i < 20; i++) {
        nimcp_rsc_step_imagination(rsc, 50.0f);
    }

    /* Get imagination state */
    nimcp_rsc_imagination_t state;
    nimcp_rsc_get_imagination_state(rsc, &state);
    EXPECT_TRUE(state.active);
    EXPECT_GT(state.steps_simulated, 0u);

    /* Stop imagination */
    nimcp_rsc_stop_imagination(rsc);

    nimcp_rsc_stats_t stats;
    nimcp_rsc_get_stats(rsc, &stats);
    EXPECT_EQ(stats.imagination_episodes, 1u);
}
