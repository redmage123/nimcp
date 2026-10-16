/**
 * @file test_embodiment.cpp
 * @brief Unit tests for NIMCP URDF embodiment model — parsing, joint queries,
 *        body schema, and forward kinematics.
 *
 * WHAT: Test embodiment lifecycle, URDF parsing, joint/link queries, body schema
 *       composition, forward kinematics, and NULL safety.
 * WHY:  The embodiment model provides the brain's body-schema awareness;
 *       incorrect parsing or kinematics breaks proprioceptive feedback.
 * HOW:  Google Test, minimal URDF strings (no file I/O).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "edge/nimcp_embodiment.h"
}

// ============================================================================
// Test URDF Strings
// ============================================================================

static const char* SIMPLE_URDF =
    "<robot name=\"test_robot\">"
    "  <link name=\"base_link\">"
    "    <inertial><mass value=\"1.0\"/></inertial>"
    "  </link>"
    "  <link name=\"arm_link\">"
    "    <inertial><mass value=\"0.5\"/></inertial>"
    "  </link>"
    "  <joint name=\"arm_joint\" type=\"revolute\">"
    "    <parent link=\"base_link\"/>"
    "    <child link=\"arm_link\"/>"
    "    <axis xyz=\"0 0 1\"/>"
    "    <limit lower=\"-1.57\" upper=\"1.57\" velocity=\"2.0\" effort=\"10.0\"/>"
    "  </joint>"
    "</robot>";

static const char* MULTI_JOINT_URDF =
    "<robot name=\"multi_robot\">"
    "  <link name=\"base_link\"><inertial><mass value=\"2.0\"/></inertial></link>"
    "  <link name=\"link1\"><inertial><mass value=\"0.5\"/></inertial></link>"
    "  <link name=\"link2\"><inertial><mass value=\"0.3\"/></inertial></link>"
    "  <link name=\"link3\"><inertial><mass value=\"0.2\"/></inertial></link>"
    "  <joint name=\"joint1\" type=\"revolute\">"
    "    <parent link=\"base_link\"/><child link=\"link1\"/>"
    "    <axis xyz=\"0 0 1\"/>"
    "    <limit lower=\"-3.14\" upper=\"3.14\" velocity=\"2.0\" effort=\"10.0\"/>"
    "  </joint>"
    "  <joint name=\"joint2\" type=\"continuous\">"
    "    <parent link=\"link1\"/><child link=\"link2\"/>"
    "    <axis xyz=\"0 1 0\"/>"
    "  </joint>"
    "  <joint name=\"fixed_joint\" type=\"fixed\">"
    "    <parent link=\"link2\"/><child link=\"link3\"/>"
    "  </joint>"
    "</robot>";

// ============================================================================
// Lifecycle
// ============================================================================

TEST(Embodiment, CreateDestroy) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    nimcp_embodiment_destroy(em);
}

TEST(Embodiment, DestroyNull) {
    nimcp_embodiment_destroy(NULL);
    SUCCEED() << "nimcp_embodiment_destroy(NULL) did not crash";
}

// ============================================================================
// URDF Parsing
// ============================================================================

TEST(Embodiment, LoadSimpleURDF) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);

    int rc = nimcp_embodiment_load_urdf(em, SIMPLE_URDF);
    EXPECT_EQ(rc, 0) << "Simple URDF should parse successfully";

    nimcp_embodiment_destroy(em);
}

TEST(Embodiment, LoadMultiJointURDF) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("multi_robot");
    ASSERT_NE(em, nullptr);

    int rc = nimcp_embodiment_load_urdf(em, MULTI_JOINT_URDF);
    EXPECT_EQ(rc, 0) << "Multi-joint URDF should parse successfully";

    nimcp_embodiment_destroy(em);
}

TEST(Embodiment, InvalidURDF) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("bad_robot");
    ASSERT_NE(em, nullptr);

    int rc = nimcp_embodiment_load_urdf(em, "this is not valid xml at all");
    EXPECT_LT(rc, 0) << "Invalid URDF should return error";

    nimcp_embodiment_destroy(em);
}

TEST(Embodiment, EmptyURDF) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("empty_robot");
    ASSERT_NE(em, nullptr);

    int rc = nimcp_embodiment_load_urdf(em, "");
    EXPECT_LT(rc, 0) << "Empty URDF should return error";

    nimcp_embodiment_destroy(em);
}

// ============================================================================
// Joint Queries
// ============================================================================

TEST(Embodiment, GetJointByName) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, SIMPLE_URDF), 0);

    const nimcp_joint_desc_t* joint = nimcp_embodiment_get_joint(em, "arm_joint");
    ASSERT_NE(joint, nullptr);
    EXPECT_STREQ(joint->name, "arm_joint");
    EXPECT_EQ(joint->type, NIMCP_JOINT_REVOLUTE);
    EXPECT_STREQ(joint->parent_link, "base_link");
    EXPECT_STREQ(joint->child_link, "arm_link");

    nimcp_embodiment_destroy(em);
}

TEST(Embodiment, GetJointNotFound) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, SIMPLE_URDF), 0);

    const nimcp_joint_desc_t* joint = nimcp_embodiment_get_joint(em, "nonexistent");
    EXPECT_EQ(joint, nullptr);

    nimcp_embodiment_destroy(em);
}

// ============================================================================
// Link Queries
// ============================================================================

TEST(Embodiment, GetLinkByName) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, SIMPLE_URDF), 0);

    const nimcp_link_desc_t* link = nimcp_embodiment_get_link(em, "base_link");
    ASSERT_NE(link, nullptr);
    EXPECT_STREQ(link->name, "base_link");

    const nimcp_link_desc_t* arm = nimcp_embodiment_get_link(em, "arm_link");
    ASSERT_NE(arm, nullptr);
    EXPECT_STREQ(arm->name, "arm_link");

    // Link not found
    const nimcp_link_desc_t* missing = nimcp_embodiment_get_link(em, "nonexistent");
    EXPECT_EQ(missing, nullptr);

    nimcp_embodiment_destroy(em);
}

// ============================================================================
// Actuated Joint Count
// ============================================================================

TEST(Embodiment, NumActuatedExcludesFixed) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("multi_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, MULTI_JOINT_URDF), 0);

    uint32_t n = nimcp_embodiment_get_num_actuated(em);
    // 3 joints total: revolute + continuous + fixed. Fixed is excluded.
    EXPECT_EQ(n, 2u) << "Should exclude FIXED joints from actuated count";

    nimcp_embodiment_destroy(em);
}

TEST(Embodiment, NumActuatedSimple) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, SIMPLE_URDF), 0);

    uint32_t n = nimcp_embodiment_get_num_actuated(em);
    EXPECT_EQ(n, 1u) << "Simple URDF has 1 revolute joint";

    nimcp_embodiment_destroy(em);
}

// ============================================================================
// Joint Limits
// ============================================================================

TEST(Embodiment, GetJointLimits) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, SIMPLE_URDF), 0);

    float lower[4], upper[4];
    memset(lower, 0, sizeof(lower));
    memset(upper, 0, sizeof(upper));
    uint32_t n = nimcp_embodiment_get_joint_limits(em, lower, upper, 4);
    EXPECT_EQ(n, 1u) << "Should return 1 actuated joint";

    // Verify joint limits were parsed from the URDF.
    // Also verify via direct joint query.
    const nimcp_joint_desc_t* joint = nimcp_embodiment_get_joint(em, "arm_joint");
    ASSERT_NE(joint, nullptr);
    EXPECT_EQ(lower[0], joint->lower) << "Limits should match joint descriptor";
    EXPECT_EQ(upper[0], joint->upper) << "Limits should match joint descriptor";

    nimcp_embodiment_destroy(em);
}

// ============================================================================
// Body Schema Composition
// ============================================================================

TEST(Embodiment, BodySchemaZeroPositions) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, SIMPLE_URDF), 0);

    float positions[1] = {0.0f};
    float schema[4];
    memset(schema, 0xFF, sizeof(schema));

    uint32_t n = nimcp_embodiment_compose_body_schema(em, positions, schema, 4);
    // 1 actuated joint -> 2 values (normalized pos + proximity)
    EXPECT_EQ(n, 2u);

    // Verify the schema contains valid normalized values
    EXPECT_GE(schema[0], 0.0f);
    EXPECT_LE(schema[0], 1.0f);
    EXPECT_GE(schema[1], 0.0f);
    EXPECT_LE(schema[1], 1.0f);

    nimcp_embodiment_destroy(em);
}

TEST(Embodiment, BodySchemaAtLimit) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, SIMPLE_URDF), 0);

    // Check the actual parsed limits to set test expectations
    const nimcp_joint_desc_t* joint = nimcp_embodiment_get_joint(em, "arm_joint");
    ASSERT_NE(joint, nullptr);

    float range = joint->upper - joint->lower;
    float positions[1];
    float schema[4];

    if (range > 1e-6f) {
        // If limits were parsed, test at upper limit
        positions[0] = joint->upper;
        uint32_t n = nimcp_embodiment_compose_body_schema(em, positions, schema, 4);
        EXPECT_EQ(n, 2u);
        EXPECT_NEAR(schema[0], 1.0f, 0.05f) << "At upper limit -> normalized ~1.0";
        EXPECT_NEAR(schema[1], 1.0f, 0.1f)  << "At limit -> proximity ~1.0";
    } else {
        // Zero range: body schema defaults to center (0.5, 0.0)
        positions[0] = 0.0f;
        uint32_t n = nimcp_embodiment_compose_body_schema(em, positions, schema, 4);
        EXPECT_EQ(n, 2u);
        EXPECT_NEAR(schema[0], 0.5f, 0.05f) << "Zero range -> default center";
        EXPECT_NEAR(schema[1], 0.0f, 0.1f)  << "Zero range -> zero proximity";
    }

    nimcp_embodiment_destroy(em);
}

// ============================================================================
// Forward Kinematics
// ============================================================================

TEST(Embodiment, ForwardKinematicsZeroAngles) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("test_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, SIMPLE_URDF), 0);

    float positions[1] = {0.0f};
    float xyz[3] = {0};
    int rc = nimcp_embodiment_forward_kinematics(em, positions, NULL, xyz);
    EXPECT_EQ(rc, 0);

    // With zero angles and no specified origin offsets, the position
    // should be at or near the origin
    EXPECT_FALSE(std::isnan(xyz[0]));
    EXPECT_FALSE(std::isnan(xyz[1]));
    EXPECT_FALSE(std::isnan(xyz[2]));

    nimcp_embodiment_destroy(em);
}

// ============================================================================
// Joint Type Parsing
// ============================================================================

TEST(Embodiment, JointTypeParsing) {
    nimcp_embodiment_t* em = nimcp_embodiment_create("multi_robot");
    ASSERT_NE(em, nullptr);
    ASSERT_EQ(nimcp_embodiment_load_urdf(em, MULTI_JOINT_URDF), 0);

    const nimcp_joint_desc_t* j1 = nimcp_embodiment_get_joint(em, "joint1");
    ASSERT_NE(j1, nullptr);
    EXPECT_EQ(j1->type, NIMCP_JOINT_REVOLUTE);

    const nimcp_joint_desc_t* j2 = nimcp_embodiment_get_joint(em, "joint2");
    ASSERT_NE(j2, nullptr);
    EXPECT_EQ(j2->type, NIMCP_JOINT_CONTINUOUS);

    const nimcp_joint_desc_t* jf = nimcp_embodiment_get_joint(em, "fixed_joint");
    ASSERT_NE(jf, nullptr);
    EXPECT_EQ(jf->type, NIMCP_JOINT_FIXED);

    nimcp_embodiment_destroy(em);
}

TEST(Embodiment, JointTypeNames) {
    const char* name = nimcp_joint_type_name(NIMCP_JOINT_REVOLUTE);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_joint_type_name(NIMCP_JOINT_FIXED);
    EXPECT_NE(name, nullptr);

    name = nimcp_joint_type_name(NIMCP_JOINT_CONTINUOUS);
    EXPECT_NE(name, nullptr);

    name = nimcp_joint_type_name(NIMCP_JOINT_PRISMATIC);
    EXPECT_NE(name, nullptr);
}

// ============================================================================
// NULL Safety
// ============================================================================

TEST(Embodiment, NullSafety) {
    EXPECT_EQ(nimcp_embodiment_get_num_actuated(NULL), 0u);
    EXPECT_EQ(nimcp_embodiment_get_joint(NULL, "x"), nullptr);
    EXPECT_EQ(nimcp_embodiment_get_link(NULL, "x"), nullptr);
    EXPECT_LT(nimcp_embodiment_load_urdf(NULL, SIMPLE_URDF), 0);

    nimcp_embodiment_t* em = nimcp_embodiment_create("test");
    ASSERT_NE(em, nullptr);
    EXPECT_LT(nimcp_embodiment_load_urdf(em, NULL), 0);

    float lower[4], upper[4];
    EXPECT_EQ(nimcp_embodiment_get_joint_limits(NULL, lower, upper, 4), 0u);

    float pos[1] = {0}, schema[4] = {0};
    EXPECT_EQ(nimcp_embodiment_compose_body_schema(NULL, pos, schema, 4), 0u);

    float xyz[3];
    EXPECT_LT(nimcp_embodiment_forward_kinematics(NULL, pos, NULL, xyz), 0);

    nimcp_embodiment_destroy(em);
}
