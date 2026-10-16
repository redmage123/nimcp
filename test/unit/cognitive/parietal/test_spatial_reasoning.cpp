/**
 * @file test_spatial_reasoning.cpp
 * @brief Unit tests for NIMCP Spatial Reasoning
 *
 * Tests mental rotation, coordinate transformations, spatial indexing,
 * quaternion operations, and spatial attention.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_spatial_reasoning.h"

namespace {

//=============================================================================
// Test Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-5f;
constexpr float PI = 3.14159265358979f;

//=============================================================================
// Test Fixture
//=============================================================================

class SpatialReasoningTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        sr = spatial_reasoning_create();
        ASSERT_NE(sr, nullptr);
    }

    void TearDown() override
    {
        if (sr) {
            spatial_reasoning_destroy(sr);
            sr = nullptr;
        }
    }

    // Helper: create a simple spatial object
    spatial_object_t create_test_object(float x, float y, float z)
    {
        spatial_object_t obj;
        memset(&obj, 0, sizeof(obj));
        obj.id = 0;
        obj.position = vec3_create(x, y, z);
        obj.orientation = quaternion_identity();
        obj.vertices = nullptr;
        obj.num_vertices = 0;
        obj.bounding_radius = 1.0f;
        obj.user_data = nullptr;
        return obj;
    }

    // Helper: create triangle vertices
    void create_triangle(vec3_t* verts, float size)
    {
        verts[0] = vec3_create(0, size, 0);
        verts[1] = vec3_create(-size * 0.866f, -size * 0.5f, 0);
        verts[2] = vec3_create(size * 0.866f, -size * 0.5f, 0);
    }

    spatial_reasoning_t* sr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SpatialReasoningTest, CreateDefault)
{
    EXPECT_NE(sr, nullptr);
}

TEST_F(SpatialReasoningTest, CreateCustom)
{
    spatial_config_t config = spatial_default_config();
    config.matching_threshold = 0.95f;
    config.max_objects = 5000;

    spatial_reasoning_t* custom = spatial_reasoning_create_custom(&config);
    ASSERT_NE(custom, nullptr);
    spatial_reasoning_destroy(custom);
}

TEST_F(SpatialReasoningTest, CreateWithNullConfig)
{
    spatial_reasoning_t* created = spatial_reasoning_create_custom(nullptr);
    EXPECT_NE(created, nullptr);
    spatial_reasoning_destroy(created);
}

TEST_F(SpatialReasoningTest, DestroyNullSafe)
{
    spatial_reasoning_destroy(nullptr);
    // Should not crash
}

TEST_F(SpatialReasoningTest, DefaultConfig)
{
    spatial_config_t config = spatial_default_config();

    EXPECT_NEAR(config.rotation_rate_deg_ms, SPATIAL_ROTATION_RATE_DEG_MS, 0.001f);
    EXPECT_NEAR(config.matching_threshold, 0.9f, 0.01f);
    EXPECT_EQ(config.max_objects, SPATIAL_MAX_OBJECTS);
    EXPECT_TRUE(config.enable_attention);
}

TEST_F(SpatialReasoningTest, ValidateConfig)
{
    spatial_config_t valid = spatial_default_config();
    EXPECT_TRUE(spatial_validate_config(&valid));

    spatial_config_t invalid = valid;
    invalid.max_objects = 0;
    EXPECT_FALSE(spatial_validate_config(&invalid));

    invalid = valid;
    invalid.matching_threshold = 1.5f;
    EXPECT_FALSE(spatial_validate_config(&invalid));
}

//=============================================================================
// Vector Utility Tests
//=============================================================================

TEST_F(SpatialReasoningTest, Vec3Create)
{
    vec3_t v = vec3_create(1.0f, 2.0f, 3.0f);
    EXPECT_EQ(v.x, 1.0f);
    EXPECT_EQ(v.y, 2.0f);
    EXPECT_EQ(v.z, 3.0f);
}

TEST_F(SpatialReasoningTest, Vec3Add)
{
    vec3_t a = vec3_create(1.0f, 2.0f, 3.0f);
    vec3_t b = vec3_create(4.0f, 5.0f, 6.0f);
    vec3_t c = vec3_add(a, b);

    EXPECT_EQ(c.x, 5.0f);
    EXPECT_EQ(c.y, 7.0f);
    EXPECT_EQ(c.z, 9.0f);
}

TEST_F(SpatialReasoningTest, Vec3Sub)
{
    vec3_t a = vec3_create(5.0f, 7.0f, 9.0f);
    vec3_t b = vec3_create(1.0f, 2.0f, 3.0f);
    vec3_t c = vec3_sub(a, b);

    EXPECT_EQ(c.x, 4.0f);
    EXPECT_EQ(c.y, 5.0f);
    EXPECT_EQ(c.z, 6.0f);
}

TEST_F(SpatialReasoningTest, Vec3Scale)
{
    vec3_t v = vec3_create(2.0f, 3.0f, 4.0f);
    vec3_t s = vec3_scale(v, 2.0f);

    EXPECT_EQ(s.x, 4.0f);
    EXPECT_EQ(s.y, 6.0f);
    EXPECT_EQ(s.z, 8.0f);
}

TEST_F(SpatialReasoningTest, Vec3Dot)
{
    vec3_t a = vec3_create(1.0f, 2.0f, 3.0f);
    vec3_t b = vec3_create(4.0f, 5.0f, 6.0f);
    float dot = vec3_dot(a, b);

    EXPECT_NEAR(dot, 32.0f, FLOAT_TOLERANCE);  // 1*4 + 2*5 + 3*6
}

TEST_F(SpatialReasoningTest, Vec3Cross)
{
    vec3_t x = vec3_create(1.0f, 0.0f, 0.0f);
    vec3_t y = vec3_create(0.0f, 1.0f, 0.0f);
    vec3_t z = vec3_cross(x, y);

    EXPECT_NEAR(z.x, 0.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(z.y, 0.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(z.z, 1.0f, FLOAT_TOLERANCE);
}

TEST_F(SpatialReasoningTest, Vec3Length)
{
    vec3_t v = vec3_create(3.0f, 4.0f, 0.0f);
    float len = vec3_length(v);

    EXPECT_NEAR(len, 5.0f, FLOAT_TOLERANCE);
}

TEST_F(SpatialReasoningTest, Vec3Normalize)
{
    vec3_t v = vec3_create(3.0f, 4.0f, 0.0f);
    vec3_t n = vec3_normalize(v);

    EXPECT_NEAR(vec3_length(n), 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(n.x, 0.6f, FLOAT_TOLERANCE);
    EXPECT_NEAR(n.y, 0.8f, FLOAT_TOLERANCE);
}

TEST_F(SpatialReasoningTest, Vec3Distance)
{
    vec3_t a = vec3_create(0.0f, 0.0f, 0.0f);
    vec3_t b = vec3_create(3.0f, 4.0f, 0.0f);
    float dist = vec3_distance(a, b);

    EXPECT_NEAR(dist, 5.0f, FLOAT_TOLERANCE);
}

//=============================================================================
// Quaternion Tests
//=============================================================================

TEST_F(SpatialReasoningTest, QuaternionIdentity)
{
    quaternion_t q = quaternion_identity();

    EXPECT_EQ(q.w, 1.0f);
    EXPECT_EQ(q.x, 0.0f);
    EXPECT_EQ(q.y, 0.0f);
    EXPECT_EQ(q.z, 0.0f);
}

TEST_F(SpatialReasoningTest, QuaternionFromAxisAngle)
{
    vec3_t axis = vec3_create(0.0f, 0.0f, 1.0f);  // Z axis
    quaternion_t q = quaternion_from_axis_angle(axis, PI / 2);  // 90 degrees

    EXPECT_NEAR(q.w, cosf(PI / 4), FLOAT_TOLERANCE);
    EXPECT_NEAR(q.z, sinf(PI / 4), FLOAT_TOLERANCE);
}

TEST_F(SpatialReasoningTest, QuaternionNormalize)
{
    quaternion_t q = {2.0f, 0.0f, 0.0f, 0.0f};
    quaternion_t n = quaternion_normalize(q);

    float len = sqrtf(n.w * n.w + n.x * n.x + n.y * n.y + n.z * n.z);
    EXPECT_NEAR(len, 1.0f, FLOAT_TOLERANCE);
}

TEST_F(SpatialReasoningTest, QuaternionMultiply)
{
    vec3_t axis = vec3_create(0.0f, 0.0f, 1.0f);
    quaternion_t q1 = quaternion_from_axis_angle(axis, PI / 4);  // 45 degrees
    quaternion_t q2 = quaternion_from_axis_angle(axis, PI / 4);  // 45 degrees
    quaternion_t combined = quaternion_multiply(q1, q2);  // Should be 90 degrees

    // Apply to vector
    vec3_t v = vec3_create(1.0f, 0.0f, 0.0f);
    vec3_t rotated = quaternion_rotate_vector(combined, v);

    EXPECT_NEAR(rotated.x, 0.0f, 0.01f);
    EXPECT_NEAR(rotated.y, 1.0f, 0.01f);
}

TEST_F(SpatialReasoningTest, QuaternionRotateVector)
{
    vec3_t axis = vec3_create(0.0f, 0.0f, 1.0f);  // Z axis
    quaternion_t q = quaternion_from_axis_angle(axis, PI / 2);  // 90 degrees around Z

    vec3_t v = vec3_create(1.0f, 0.0f, 0.0f);  // X axis
    vec3_t rotated = quaternion_rotate_vector(q, v);

    // X should become Y after 90 degree rotation around Z
    EXPECT_NEAR(rotated.x, 0.0f, 0.01f);
    EXPECT_NEAR(rotated.y, 1.0f, 0.01f);
    EXPECT_NEAR(rotated.z, 0.0f, 0.01f);
}

TEST_F(SpatialReasoningTest, QuaternionAngleBetween)
{
    quaternion_t q1 = quaternion_identity();
    vec3_t axis = vec3_create(0.0f, 0.0f, 1.0f);
    quaternion_t q2 = quaternion_from_axis_angle(axis, PI / 2);

    // quaternion_angle_between returns degrees, not radians
    float angle = quaternion_angle_between(q1, q2);
    EXPECT_NEAR(angle, 90.0f, 1.0f);
}

//=============================================================================
// Mental Rotation Tests
//=============================================================================

TEST_F(SpatialReasoningTest, MentalRotateSameObject)
{
    spatial_object_t obj_a = create_test_object(0, 0, 0);
    spatial_object_t obj_b = create_test_object(0, 0, 0);

    rotation_result_t result = spatial_rotate_and_compare(sr, &obj_a, &obj_b);

    EXPECT_TRUE(result.is_match);
    EXPECT_GT(result.confidence, 0.5f);  // Reasonable confidence for identical objects
}

TEST_F(SpatialReasoningTest, MentalRotateProcessingTimeLinear)
{
    // Shepard paradigm: processing time should be linear with angle
    vec3_t axis = vec3_create(0.0f, 1.0f, 0.0f);  // Y axis

    spatial_object_t obj = create_test_object(0, 0, 0);
    uint64_t time1 = spatial_mental_rotate(sr, &obj, axis, 45.0f);
    uint64_t time2 = spatial_mental_rotate(sr, &obj, axis, 90.0f);

    // 90 degrees should take roughly twice as long as 45 degrees
    float ratio = (float)time2 / (float)time1;
    EXPECT_GT(ratio, 1.5f);
    EXPECT_LT(ratio, 2.5f);
}

TEST_F(SpatialReasoningTest, ShapeSimilarity)
{
    spatial_object_t obj_a = create_test_object(0, 0, 0);
    spatial_object_t obj_b = create_test_object(1, 1, 1);

    // Same shape at different positions
    float sim = spatial_shape_similarity(sr, &obj_a, &obj_b);
    EXPECT_GE(sim, 0.0f);
    EXPECT_LE(sim, 1.0f);
}

TEST_F(SpatialReasoningTest, MentalRotateNullHandling)
{
    spatial_object_t obj = create_test_object(0, 0, 0);
    vec3_t axis = vec3_create(0, 1, 0);

    rotation_result_t result = spatial_rotate_and_compare(nullptr, &obj, &obj);
    EXPECT_FALSE(result.is_match);

    result = spatial_rotate_and_compare(sr, nullptr, &obj);
    EXPECT_FALSE(result.is_match);
}

//=============================================================================
// Coordinate Transformation Tests
//=============================================================================

TEST_F(SpatialReasoningTest, EgoToAllocentric)
{
    observer_pose_t observer;
    observer.position = vec3_create(10.0f, 0.0f, 0.0f);
    observer.orientation = quaternion_identity();
    observer.heading = 0.0f;

    vec3_t local = vec3_create(5.0f, 0.0f, 0.0f);  // 5 units in front
    vec3_t world = spatial_ego_to_allocentric(sr, local, &observer);

    EXPECT_NEAR(world.x, 15.0f, 0.01f);  // 10 + 5
    EXPECT_NEAR(world.y, 0.0f, 0.01f);
}

TEST_F(SpatialReasoningTest, AllocentricToEgo)
{
    observer_pose_t observer;
    observer.position = vec3_create(10.0f, 0.0f, 0.0f);
    observer.orientation = quaternion_identity();
    observer.heading = 0.0f;

    vec3_t world = vec3_create(15.0f, 0.0f, 0.0f);
    vec3_t local = spatial_allocentric_to_ego(sr, world, &observer);

    EXPECT_NEAR(local.x, 5.0f, 0.01f);  // 15 - 10
}

TEST_F(SpatialReasoningTest, CoordinateTransformRoundTrip)
{
    observer_pose_t observer;
    observer.position = vec3_create(5.0f, 3.0f, 2.0f);
    observer.orientation = quaternion_from_axis_angle(vec3_create(0, 1, 0), 0.5f);
    observer.heading = 0.5f;

    vec3_t original = vec3_create(1.0f, 2.0f, 3.0f);
    vec3_t world = spatial_ego_to_allocentric(sr, original, &observer);
    vec3_t back = spatial_allocentric_to_ego(sr, world, &observer);

    EXPECT_NEAR(back.x, original.x, 0.01f);
    EXPECT_NEAR(back.y, original.y, 0.01f);
    EXPECT_NEAR(back.z, original.z, 0.01f);
}

TEST_F(SpatialReasoningTest, PoseToTransform)
{
    observer_pose_t pose;
    pose.position = vec3_create(1.0f, 2.0f, 3.0f);
    pose.orientation = quaternion_identity();
    pose.heading = 0.0f;

    spatial_transform_t transform = spatial_pose_to_transform(&pose);

    // Test translation component (column-major, translation in columns 12-14)
    EXPECT_NEAR(transform.m[12], 1.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(transform.m[13], 2.0f, FLOAT_TOLERANCE);
    EXPECT_NEAR(transform.m[14], 3.0f, FLOAT_TOLERANCE);
}

TEST_F(SpatialReasoningTest, TransformPoint)
{
    observer_pose_t pose;
    pose.position = vec3_create(10.0f, 0.0f, 0.0f);
    pose.orientation = quaternion_identity();
    pose.heading = 0.0f;

    spatial_transform_t transform = spatial_pose_to_transform(&pose);
    vec3_t point = vec3_create(5.0f, 0.0f, 0.0f);
    vec3_t result = spatial_transform_point(&transform, point);

    EXPECT_NEAR(result.x, 15.0f, 0.01f);
}

TEST_F(SpatialReasoningTest, TransformInverse)
{
    observer_pose_t pose;
    pose.position = vec3_create(5.0f, 3.0f, 2.0f);
    pose.orientation = quaternion_from_axis_angle(vec3_create(0, 1, 0), 0.3f);
    pose.heading = 0.3f;

    spatial_transform_t transform = spatial_pose_to_transform(&pose);
    spatial_transform_t inverse = spatial_transform_inverse(&transform);

    // Apply transform then inverse should give original point
    vec3_t original = vec3_create(1.0f, 2.0f, 3.0f);
    vec3_t transformed = spatial_transform_point(&transform, original);
    vec3_t back = spatial_transform_point(&inverse, transformed);

    EXPECT_NEAR(back.x, original.x, 0.01f);
    EXPECT_NEAR(back.y, original.y, 0.01f);
    EXPECT_NEAR(back.z, original.z, 0.01f);
}

//=============================================================================
// Spatial Indexing Tests
//=============================================================================

TEST_F(SpatialReasoningTest, AddObject)
{
    spatial_object_t obj = create_test_object(1.0f, 2.0f, 3.0f);
    uint32_t id = spatial_add_object(sr, &obj);

    EXPECT_GT(id, 0);
}

TEST_F(SpatialReasoningTest, AddMultipleObjects)
{
    for (int i = 0; i < 10; i++) {
        spatial_object_t obj = create_test_object((float)i, 0.0f, 0.0f);
        uint32_t id = spatial_add_object(sr, &obj);
        EXPECT_GT(id, 0);
    }

    spatial_stats_t stats;
    spatial_get_stats(sr, &stats);
    EXPECT_EQ(stats.objects_stored, 10);
}

TEST_F(SpatialReasoningTest, RemoveObject)
{
    spatial_object_t obj = create_test_object(1.0f, 2.0f, 3.0f);
    uint32_t id = spatial_add_object(sr, &obj);

    EXPECT_EQ(spatial_remove_object(sr, id), 0);
    EXPECT_NE(spatial_remove_object(sr, id), 0);  // Already removed
}

TEST_F(SpatialReasoningTest, UpdatePosition)
{
    spatial_object_t obj = create_test_object(1.0f, 2.0f, 3.0f);
    uint32_t id = spatial_add_object(sr, &obj);

    vec3_t new_pos = vec3_create(10.0f, 20.0f, 30.0f);
    EXPECT_EQ(spatial_update_position(sr, id, new_pos), 0);
}

TEST_F(SpatialReasoningTest, FindNearest)
{
    // Add objects at different positions
    spatial_object_t obj1 = create_test_object(0.0f, 0.0f, 0.0f);
    spatial_object_t obj2 = create_test_object(10.0f, 0.0f, 0.0f);
    spatial_object_t obj3 = create_test_object(5.0f, 0.0f, 0.0f);

    spatial_add_object(sr, &obj1);
    spatial_add_object(sr, &obj2);
    spatial_add_object(sr, &obj3);

    // Query near origin
    vec3_t query = vec3_create(1.0f, 0.0f, 0.0f);
    spatial_object_t* nearest = spatial_find_nearest(sr, query);

    ASSERT_NE(nearest, nullptr);
    EXPECT_NEAR(nearest->position.x, 0.0f, 0.01f);  // Should find obj1
}

TEST_F(SpatialReasoningTest, FindKNearest)
{
    // Add objects
    for (int i = 0; i < 10; i++) {
        spatial_object_t obj = create_test_object((float)i * 2, 0.0f, 0.0f);
        spatial_add_object(sr, &obj);
    }

    spatial_query_result_t* result = spatial_query_result_create(5);
    ASSERT_NE(result, nullptr);

    vec3_t query = vec3_create(0.0f, 0.0f, 0.0f);
    uint32_t found = spatial_find_k_nearest(sr, query, 5, result);

    EXPECT_EQ(found, 5);
    EXPECT_EQ(result->count, 5);

    spatial_query_result_destroy(result);
}

TEST_F(SpatialReasoningTest, FindInRadius)
{
    // Add objects in a line
    for (int i = 0; i < 10; i++) {
        spatial_object_t obj = create_test_object((float)i, 0.0f, 0.0f);
        spatial_add_object(sr, &obj);
    }

    spatial_query_result_t* result = spatial_query_result_create(20);
    ASSERT_NE(result, nullptr);

    vec3_t center = vec3_create(0.0f, 0.0f, 0.0f);
    uint32_t found = spatial_find_in_radius(sr, center, 3.5f, result);

    EXPECT_EQ(found, 4);  // Objects at 0, 1, 2, 3 are within radius 3.5

    spatial_query_result_destroy(result);
}

TEST_F(SpatialReasoningTest, QueryResultNullHandling)
{
    spatial_query_result_t* result = spatial_query_result_create(10);
    ASSERT_NE(result, nullptr);

    vec3_t query = vec3_create(0, 0, 0);
    EXPECT_EQ(spatial_find_k_nearest(nullptr, query, 5, result), 0);
    EXPECT_EQ(spatial_find_k_nearest(sr, query, 5, nullptr), 0);

    spatial_query_result_destroy(result);
    spatial_query_result_destroy(nullptr);  // Should be null-safe
}

//=============================================================================
// Spatial Attention Tests
//=============================================================================

TEST_F(SpatialReasoningTest, AttentionCreate)
{
    spatial_attention_t* attn = spatial_attention_create(sr, 10, 10);
    ASSERT_NE(attn, nullptr);
    spatial_attention_destroy(attn);
}

TEST_F(SpatialReasoningTest, AttentionSetFocus)
{
    spatial_attention_t* attn = spatial_attention_create(sr, 10, 10);
    ASSERT_NE(attn, nullptr);

    vec3_t focus = vec3_create(5.0f, 5.0f, 0.0f);
    EXPECT_EQ(spatial_attention_set_focus(attn, focus, 2.0f), 0);

    // Attention should be highest at focus point
    float attn_at_focus = spatial_attention_at(attn, focus);
    vec3_t away = vec3_create(0.0f, 0.0f, 0.0f);
    float attn_away = spatial_attention_at(attn, away);

    EXPECT_GT(attn_at_focus, attn_away);

    spatial_attention_destroy(attn);
}

TEST_F(SpatialReasoningTest, AttentionUpdate)
{
    spatial_attention_t* attn = spatial_attention_create(sr, 10, 10);
    ASSERT_NE(attn, nullptr);

    vec3_t focus = vec3_create(5.0f, 5.0f, 0.0f);
    spatial_attention_set_focus(attn, focus, 2.0f);

    float before = spatial_attention_at(attn, focus);
    EXPECT_EQ(spatial_attention_update(attn, 0.5f), 0);  // 50% decay
    float after = spatial_attention_at(attn, focus);

    EXPECT_LT(after, before);  // Attention should decay

    spatial_attention_destroy(attn);
}

TEST_F(SpatialReasoningTest, AttentionNullHandling)
{
    spatial_attention_destroy(nullptr);  // Should not crash

    vec3_t focus = vec3_create(0, 0, 0);
    EXPECT_NE(spatial_attention_set_focus(nullptr, focus, 1.0f), 0);
    EXPECT_EQ(spatial_attention_at(nullptr, focus), 0.0f);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(SpatialReasoningTest, SetInflammation)
{
    EXPECT_EQ(spatial_set_inflammation(sr, 0.5f), 0);
    EXPECT_NE(spatial_set_inflammation(nullptr, 0.5f), 0);
}

TEST_F(SpatialReasoningTest, SetFatigue)
{
    EXPECT_EQ(spatial_set_fatigue(sr, 0.5f), 0);
    EXPECT_NE(spatial_set_fatigue(nullptr, 0.5f), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SpatialReasoningTest, GetStats)
{
    // Perform operations
    spatial_object_t obj1 = create_test_object(0, 0, 0);
    spatial_object_t obj2 = create_test_object(1, 0, 0);
    spatial_add_object(sr, &obj1);
    spatial_rotate_and_compare(sr, &obj1, &obj2);

    spatial_stats_t stats;
    EXPECT_EQ(spatial_get_stats(sr, &stats), 0);

    EXPECT_GE(stats.rotations_performed, 1);
    EXPECT_GE(stats.objects_stored, 1);
}

TEST_F(SpatialReasoningTest, GetStatsNullHandling)
{
    spatial_stats_t stats;
    EXPECT_NE(spatial_get_stats(nullptr, &stats), 0);
    EXPECT_NE(spatial_get_stats(sr, nullptr), 0);
}

TEST_F(SpatialReasoningTest, ResetStats)
{
    spatial_object_t obj = create_test_object(0, 0, 0);
    spatial_add_object(sr, &obj);

    spatial_reset_stats(sr);

    spatial_stats_t stats;
    spatial_get_stats(sr, &stats);
    EXPECT_EQ(stats.rotations_performed, 0);
}

TEST_F(SpatialReasoningTest, ResetStatsNullSafe)
{
    spatial_reset_stats(nullptr);
    // Should not crash
}

}  // namespace
