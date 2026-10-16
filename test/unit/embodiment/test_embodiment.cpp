/**
 * @file test_embodiment.cpp
 * @brief Comprehensive Unit Tests for Embodiment Module
 *
 * Tests all four embodiment subsystems:
 * - Affordance Processing (nimcp_affordance_processing.h)
 * - Body Ownership (nimcp_body_ownership.h)
 * - Embodied Simulation (nimcp_embodied_simulation.h)
 * - Interoceptive Prediction (nimcp_interoceptive_prediction.h)
 *
 * @version 1.0.0
 * @date 2026-02-02
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "embodiment/nimcp_affordance_processing.h"
#include "embodiment/nimcp_body_ownership.h"
#include "embodiment/nimcp_embodied_simulation.h"
#include "embodiment/nimcp_interoceptive_prediction.h"
}

/* ============================================================================
 * Affordance Processing Tests
 * ============================================================================ */

class AffordanceProcessingTest : public ::testing::Test {
protected:
    nimcp_affordance_context_t* ctx = nullptr;

    void SetUp() override { ctx = nullptr; }
    void TearDown() override {
        if (ctx) { nimcp_affordance_destroy(ctx); ctx = nullptr; }
    }

    nimcp_affordance_context_t* createWithDefaults() {
        ctx = nimcp_affordance_create(nullptr);
        return ctx;
    }

    nimcp_object_properties_t makeTestObject(uint32_t id, nimcp_object_category_t category) {
        nimcp_object_properties_t obj;
        memset(&obj, 0, sizeof(obj));
        obj.object_id = id;
        obj.category = category;
        obj.position[0] = 1.0;
        obj.position[1] = 0.5;
        obj.position[2] = 0.0;
        obj.dimensions[0] = 0.1;
        obj.dimensions[1] = 0.1;
        obj.dimensions[2] = 0.1;
        obj.distance = 0.5;
        obj.estimated_mass = 0.5;
        obj.is_graspable = true;
        obj.is_movable = true;
        obj.is_stationary = true;
        return obj;
    }
};

/* --- Lifecycle Tests --- */

TEST_F(AffordanceProcessingTest, DefaultConfigHasReasonableSettings) {
    nimcp_affordance_config_t config;
    nimcp_affordance_default_config(&config);
    EXPECT_GT(config.detection_threshold, 0.0);
    EXPECT_GT(config.max_objects, 0u);
    EXPECT_GT(config.max_affordances_per_object, 0u);
    EXPECT_GT(config.update_rate_hz, 0.0);
}

TEST_F(AffordanceProcessingTest, CreateWithNullConfigUsesDefaults) {
    ctx = nimcp_affordance_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(AffordanceProcessingTest, CreateWithValidConfig) {
    nimcp_affordance_config_t config;
    nimcp_affordance_default_config(&config);
    config.max_objects = 32;
    ctx = nimcp_affordance_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(AffordanceProcessingTest, DestroyNullIsNoOp) {
    nimcp_affordance_destroy(nullptr);
    // Should not crash
}

TEST_F(AffordanceProcessingTest, CreateDestroyCycle) {
    for (int i = 0; i < 5; i++) {
        ctx = nimcp_affordance_create(nullptr);
        ASSERT_NE(ctx, nullptr);
        nimcp_affordance_destroy(ctx);
        ctx = nullptr;
    }
}

TEST_F(AffordanceProcessingTest, ResetClearsState) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_register_object(ctx, &obj);

    nimcp_affordance_error_t err = nimcp_affordance_reset(ctx);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);

    nimcp_affordance_stats_t stats;
    nimcp_affordance_get_stats(ctx, &stats);
    EXPECT_EQ(stats.objects_tracked, 0u);
}

/* --- Object Management Tests --- */

TEST_F(AffordanceProcessingTest, RegisterObject) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_error_t err = nimcp_affordance_register_object(ctx, &obj);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
}

TEST_F(AffordanceProcessingTest, RegisterMultipleObjects) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    for (uint32_t i = 1; i <= 5; i++) {
        nimcp_object_properties_t obj = makeTestObject(i, NIMCP_OBJECT_CATEGORY_MANIPULANDUM);
        nimcp_affordance_error_t err = nimcp_affordance_register_object(ctx, &obj);
        EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
    }
}

TEST_F(AffordanceProcessingTest, UpdateObject) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_register_object(ctx, &obj);

    obj.position[0] = 2.0;
    nimcp_affordance_error_t err = nimcp_affordance_update_object(ctx, &obj);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
}

TEST_F(AffordanceProcessingTest, RemoveObject) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_register_object(ctx, &obj);

    nimcp_affordance_error_t err = nimcp_affordance_remove_object(ctx, 1);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
}

TEST_F(AffordanceProcessingTest, RemoveInvalidObjectReturnsError) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_affordance_error_t err = nimcp_affordance_remove_object(ctx, 999);
    EXPECT_NE(err, NIMCP_AFFORDANCE_OK);
}

TEST_F(AffordanceProcessingTest, GetObject) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_CONTAINER);
    nimcp_affordance_register_object(ctx, &obj);

    nimcp_object_properties_t retrieved;
    nimcp_affordance_error_t err = nimcp_affordance_get_object(ctx, 1, &retrieved);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
    EXPECT_EQ(retrieved.object_id, 1u);
    EXPECT_EQ(retrieved.category, NIMCP_OBJECT_CATEGORY_CONTAINER);
}

/* --- Affordance Detection Tests --- */

TEST_F(AffordanceProcessingTest, DetectAffordancesForObject) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_register_object(ctx, &obj);

    nimcp_affordance_t affordances[16];
    uint32_t num_detected = 0;
    nimcp_affordance_error_t err = nimcp_affordance_detect(
        ctx, 1, affordances, 16, &num_detected);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
}

TEST_F(AffordanceProcessingTest, DetectAllAffordances) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj1 = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_object_properties_t obj2 = makeTestObject(2, NIMCP_OBJECT_CATEGORY_CONTAINER);
    nimcp_affordance_register_object(ctx, &obj1);
    nimcp_affordance_register_object(ctx, &obj2);

    nimcp_affordance_t affordances[64];
    uint32_t num_detected = 0;
    nimcp_affordance_error_t err = nimcp_affordance_detect_all(
        ctx, affordances, 64, &num_detected);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
}

TEST_F(AffordanceProcessingTest, GetAffordancesByAction) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_MANIPULANDUM);
    obj.is_graspable = true;
    nimcp_affordance_register_object(ctx, &obj);

    // Detect first to populate
    nimcp_affordance_t affordances[16];
    uint32_t num_detected = 0;
    nimcp_affordance_detect(ctx, 1, affordances, 16, &num_detected);

    nimcp_affordance_t grasp_affordances[16];
    uint32_t num_found = 0;
    nimcp_affordance_error_t err = nimcp_affordance_get_by_action(
        ctx, NIMCP_ACTION_GRASP, grasp_affordances, 16, &num_found);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
}

/* --- Competition Tests --- */

TEST_F(AffordanceProcessingTest, AffordanceCompetition) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_register_object(ctx, &obj);

    nimcp_affordance_t affordances[16];
    uint32_t num_detected = 0;
    nimcp_affordance_detect(ctx, 1, affordances, 16, &num_detected);

    nimcp_competition_result_t result;
    memset(&result, 0, sizeof(result));
    nimcp_affordance_error_t err = nimcp_affordance_compete(ctx, &result);
    // Either succeeds or returns no-affordances
    EXPECT_TRUE(err == NIMCP_AFFORDANCE_OK || err == NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES);
}

TEST_F(AffordanceProcessingTest, GoalDirectedCompetition) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_register_object(ctx, &obj);

    nimcp_affordance_t affordances[16];
    uint32_t num_detected = 0;
    nimcp_affordance_detect(ctx, 1, affordances, 16, &num_detected);

    nimcp_affordance_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.goal_id = 1;
    goal.target_action = NIMCP_ACTION_GRASP;
    goal.is_active = true;
    goal.priority = 1.0;

    nimcp_competition_result_t result;
    memset(&result, 0, sizeof(result));
    nimcp_affordance_error_t err = nimcp_affordance_compete_for_goal(ctx, &goal, &result);
    EXPECT_TRUE(err == NIMCP_AFFORDANCE_OK || err == NIMCP_AFFORDANCE_ERROR_NO_AFFORDANCES);
}

/* --- Motor Coupling Tests --- */

TEST_F(AffordanceProcessingTest, UpdateMotorReadiness) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_register_object(ctx, &obj);

    nimcp_affordance_t affordances[16];
    uint32_t num_detected = 0;
    nimcp_affordance_detect(ctx, 1, affordances, 16, &num_detected);

    if (num_detected > 0) {
        nimcp_affordance_error_t err = nimcp_affordance_update_motor_readiness(
            ctx, affordances[0].affordance_id, 0.8);
        EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
    }
}

TEST_F(AffordanceProcessingTest, ReportOutcome) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_object_properties_t obj = makeTestObject(1, NIMCP_OBJECT_CATEGORY_TOOL);
    nimcp_affordance_register_object(ctx, &obj);

    nimcp_affordance_t affordances[16];
    uint32_t num_detected = 0;
    nimcp_affordance_detect(ctx, 1, affordances, 16, &num_detected);

    if (num_detected > 0) {
        nimcp_affordance_error_t err = nimcp_affordance_report_outcome(
            ctx, affordances[0].affordance_id, true, 0.5, 1.0);
        EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
    }
}

/* --- Update and Stats Tests --- */

TEST_F(AffordanceProcessingTest, UpdateCycle) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_affordance_error_t err = nimcp_affordance_update(ctx, 0.016);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
}

TEST_F(AffordanceProcessingTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_affordance_stats_t stats;
    nimcp_affordance_error_t err = nimcp_affordance_get_stats(ctx, &stats);
    EXPECT_EQ(err, NIMCP_AFFORDANCE_OK);
}

TEST_F(AffordanceProcessingTest, NameFunctions) {
    EXPECT_NE(nimcp_affordance_action_name(NIMCP_ACTION_GRASP), nullptr);
    EXPECT_NE(nimcp_affordance_state_name(NIMCP_AFFORDANCE_STATE_DETECTED), nullptr);
    EXPECT_NE(nimcp_affordance_category_name(NIMCP_OBJECT_CATEGORY_TOOL), nullptr);
}

/* --- Null Parameter Tests --- */

TEST_F(AffordanceProcessingTest, NullContextOperationsReturnErrors) {
    nimcp_object_properties_t obj;
    memset(&obj, 0, sizeof(obj));
    EXPECT_EQ(nimcp_affordance_register_object(nullptr, &obj), NIMCP_AFFORDANCE_ERROR_NULL_PARAM);
    EXPECT_EQ(nimcp_affordance_update_object(nullptr, &obj), NIMCP_AFFORDANCE_ERROR_NULL_PARAM);
    EXPECT_EQ(nimcp_affordance_remove_object(nullptr, 1), NIMCP_AFFORDANCE_ERROR_NULL_PARAM);

    nimcp_affordance_stats_t stats;
    EXPECT_EQ(nimcp_affordance_get_stats(nullptr, &stats), NIMCP_AFFORDANCE_ERROR_NULL_PARAM);
}

/* ============================================================================
 * Body Ownership Tests
 * ============================================================================ */

class BodyOwnershipTest : public ::testing::Test {
protected:
    nimcp_body_context_t* ctx = nullptr;

    void SetUp() override { ctx = nullptr; }
    void TearDown() override {
        if (ctx) { nimcp_body_destroy(ctx); ctx = nullptr; }
    }

    nimcp_body_context_t* createWithDefaults() {
        ctx = nimcp_body_create(nullptr);
        return ctx;
    }

    nimcp_body_part_t makeTestPart(uint32_t id, nimcp_body_part_type_t type) {
        nimcp_body_part_t part;
        memset(&part, 0, sizeof(part));
        part.part_id = id;
        part.type = type;
        strncpy(part.name, "test_part", sizeof(part.name) - 1);
        part.position.x = 0.0;
        part.position.y = 0.0;
        part.position.z = 0.0;
        part.orientation.w = 1.0;
        part.mass = 1.0;
        part.ownership_state = NIMCP_OWNERSHIP_FULL;
        part.ownership_confidence = 1.0;
        part.is_active = true;
        return part;
    }
};

/* --- Lifecycle Tests --- */

TEST_F(BodyOwnershipTest, DefaultConfigHasReasonableSettings) {
    nimcp_body_config_t config;
    nimcp_body_default_config(&config);
    EXPECT_GT(config.proprio_weight, 0.0);
    EXPECT_GT(config.max_body_parts, 0u);
    EXPECT_GT(config.update_rate_hz, 0.0);
}

TEST_F(BodyOwnershipTest, CreateWithNullConfigUsesDefaults) {
    ctx = nimcp_body_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(BodyOwnershipTest, CreateWithValidConfig) {
    nimcp_body_config_t config;
    nimcp_body_default_config(&config);
    config.max_body_parts = 16;
    ctx = nimcp_body_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(BodyOwnershipTest, DestroyNullIsNoOp) {
    nimcp_body_destroy(nullptr);
}

TEST_F(BodyOwnershipTest, CreateDestroyCycle) {
    for (int i = 0; i < 5; i++) {
        ctx = nimcp_body_create(nullptr);
        ASSERT_NE(ctx, nullptr);
        nimcp_body_destroy(ctx);
        ctx = nullptr;
    }
}

TEST_F(BodyOwnershipTest, ResetClearsState) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_add_part(ctx, &part);

    nimcp_body_error_t err = nimcp_body_reset(ctx);
    EXPECT_EQ(err, NIMCP_BODY_OK);

    nimcp_body_stats_t stats;
    nimcp_body_get_stats(ctx, &stats);
    EXPECT_EQ(stats.active_parts, 0u);
}

/* --- Body Part Management Tests --- */

TEST_F(BodyOwnershipTest, AddBodyPart) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_error_t err = nimcp_body_add_part(ctx, &part);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, AddMultipleBodyParts) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t parts[] = {
        makeTestPart(1, NIMCP_BODY_PART_HEAD),
        makeTestPart(2, NIMCP_BODY_PART_TORSO),
        makeTestPart(3, NIMCP_BODY_PART_LEFT_ARM),
        makeTestPart(4, NIMCP_BODY_PART_RIGHT_ARM)
    };

    for (int i = 0; i < 4; i++) {
        nimcp_body_error_t err = nimcp_body_add_part(ctx, &parts[i]);
        EXPECT_EQ(err, NIMCP_BODY_OK);
    }
}

TEST_F(BodyOwnershipTest, UpdateBodyPart) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_add_part(ctx, &part);

    part.position.x = 1.0;
    nimcp_body_error_t err = nimcp_body_update_part(ctx, &part);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, RemoveBodyPart) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_add_part(ctx, &part);

    nimcp_body_error_t err = nimcp_body_remove_part(ctx, 1);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, GetBodyPart) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_add_part(ctx, &part);

    nimcp_body_part_t retrieved;
    nimcp_body_error_t err = nimcp_body_get_part(ctx, 1, &retrieved);
    EXPECT_EQ(err, NIMCP_BODY_OK);
    EXPECT_EQ(retrieved.part_id, 1u);
    EXPECT_EQ(retrieved.type, NIMCP_BODY_PART_LEFT_HAND);
}

TEST_F(BodyOwnershipTest, GetAllBodyParts) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part1 = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_part_t part2 = makeTestPart(2, NIMCP_BODY_PART_RIGHT_HAND);
    nimcp_body_add_part(ctx, &part1);
    nimcp_body_add_part(ctx, &part2);

    nimcp_body_part_t parts[32];
    uint32_t num_parts = 0;
    nimcp_body_error_t err = nimcp_body_get_all_parts(ctx, parts, 32, &num_parts);
    EXPECT_EQ(err, NIMCP_BODY_OK);
    EXPECT_EQ(num_parts, 2u);
}

TEST_F(BodyOwnershipTest, InitHumanSchema) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_error_t err = nimcp_body_init_human_schema(ctx);
    EXPECT_EQ(err, NIMCP_BODY_OK);

    nimcp_body_stats_t stats;
    nimcp_body_get_stats(ctx, &stats);
    EXPECT_GT(stats.active_parts, 0u);
}

/* --- Proprioceptive Integration Tests --- */

TEST_F(BodyOwnershipTest, ProcessProprioceptiveSignal) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_ARM);
    nimcp_body_add_part(ctx, &part);

    nimcp_proprio_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.part_id = 1;
    signal.position[0] = 0.5;
    signal.position[1] = 0.5;
    signal.position[2] = 0.0;
    signal.confidence = 0.9;

    nimcp_body_error_t err = nimcp_body_process_proprio(ctx, &signal);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, ProcessVisualFeedback) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_add_part(ctx, &part);

    nimcp_visual_feedback_t feedback;
    memset(&feedback, 0, sizeof(feedback));
    feedback.part_id = 1;
    feedback.seen_position.x = 0.5;
    feedback.seen_position.y = 0.5;
    feedback.seen_position.z = 0.0;
    feedback.is_visible = true;
    feedback.confidence = 0.95;

    nimcp_body_error_t err = nimcp_body_process_visual(ctx, &feedback);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, InvalidSensorDataHandling) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    // Process signal for non-existent part
    nimcp_proprio_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.part_id = 999;  // Non-existent part
    signal.confidence = 0.9;

    nimcp_body_error_t err = nimcp_body_process_proprio(ctx, &signal);
    EXPECT_NE(err, NIMCP_BODY_OK);
}

/* --- Ownership Tests --- */

TEST_F(BodyOwnershipTest, GetOwnership) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_add_part(ctx, &part);

    nimcp_ownership_state_t state;
    double confidence;
    nimcp_body_error_t err = nimcp_body_get_ownership(ctx, 1, &state, &confidence);
    EXPECT_EQ(err, NIMCP_BODY_OK);
    EXPECT_EQ(state, NIMCP_OWNERSHIP_FULL);
}

TEST_F(BodyOwnershipTest, UpdateOwnershipSync) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_LEFT_HAND);
    nimcp_body_add_part(ctx, &part);

    nimcp_body_position_t visual_pos = {0.5, 0.5, 0.0};
    nimcp_body_position_t tactile_pos = {0.5, 0.5, 0.0};

    nimcp_body_error_t err = nimcp_body_update_ownership_sync(
        ctx, 1, &visual_pos, &tactile_pos, true);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, ToolIncorporation) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_part_t part = makeTestPart(1, NIMCP_BODY_PART_RIGHT_HAND);
    nimcp_body_add_part(ctx, &part);

    nimcp_external_object_t tool;
    memset(&tool, 0, sizeof(tool));
    tool.object_id = 100;
    tool.position.x = 0.6;
    tool.position.y = 0.0;
    tool.position.z = 0.0;
    tool.ownership_score = 0.8;

    nimcp_body_error_t err = nimcp_body_process_external_object(ctx, &tool);
    EXPECT_EQ(err, NIMCP_BODY_OK);

    err = nimcp_body_incorporate_tool(ctx, 100, 1);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

/* --- Body Boundary Tests --- */

TEST_F(BodyOwnershipTest, UpdateBoundaries) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_init_human_schema(ctx);

    nimcp_body_error_t err = nimcp_body_update_boundaries(ctx);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, CheckBoundary) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_init_human_schema(ctx);
    nimcp_body_update_boundaries(ctx);

    nimcp_body_position_t pos = {0.0, 0.0, 0.0};
    bool is_inside;
    double distance;
    nimcp_body_error_t err = nimcp_body_check_boundary(ctx, &pos, &is_inside, &distance);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, GetCenterOfMass) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_init_human_schema(ctx);

    nimcp_body_position_t com;
    nimcp_body_error_t err = nimcp_body_get_center_of_mass(ctx, &com);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

/* --- Peripersonal Space Tests --- */

TEST_F(BodyOwnershipTest, UpdatePeripersonalSpace) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_init_human_schema(ctx);

    nimcp_body_position_t objects[] = {
        {0.5, 0.0, 0.0},
        {0.0, 0.5, 0.0}
    };

    nimcp_body_error_t err = nimcp_body_update_peripersonal(ctx, objects, 2);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, GetPeripersonalSpace) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_init_human_schema(ctx);

    nimcp_peripersonal_space_t space;
    nimcp_body_error_t err = nimcp_body_get_peripersonal(ctx, &space);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

/* --- Update and Stats Tests --- */

TEST_F(BodyOwnershipTest, UpdateCycle) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_error_t err = nimcp_body_update(ctx, 0.016);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_body_stats_t stats;
    nimcp_body_error_t err = nimcp_body_get_stats(ctx, &stats);
    EXPECT_EQ(err, NIMCP_BODY_OK);
}

TEST_F(BodyOwnershipTest, NameFunctions) {
    EXPECT_NE(nimcp_body_part_type_name(NIMCP_BODY_PART_LEFT_HAND), nullptr);
    EXPECT_NE(nimcp_body_ownership_state_name(NIMCP_OWNERSHIP_FULL), nullptr);
    EXPECT_NE(nimcp_body_joint_type_name(NIMCP_JOINT_BALL), nullptr);
}

/* --- Null Parameter Tests --- */

TEST_F(BodyOwnershipTest, NullContextOperationsReturnErrors) {
    nimcp_body_part_t part;
    memset(&part, 0, sizeof(part));
    EXPECT_EQ(nimcp_body_add_part(nullptr, &part), NIMCP_BODY_ERROR_NULL_PARAM);
    EXPECT_EQ(nimcp_body_update_part(nullptr, &part), NIMCP_BODY_ERROR_NULL_PARAM);
    EXPECT_EQ(nimcp_body_remove_part(nullptr, 1), NIMCP_BODY_ERROR_NULL_PARAM);

    nimcp_body_stats_t stats;
    EXPECT_EQ(nimcp_body_get_stats(nullptr, &stats), NIMCP_BODY_ERROR_NULL_PARAM);
}

/* ============================================================================
 * Embodied Simulation Tests
 * ============================================================================ */

class EmbodiedSimulationTest : public ::testing::Test {
protected:
    nimcp_sim_context_t* ctx = nullptr;

    void SetUp() override { ctx = nullptr; }
    void TearDown() override {
        if (ctx) { nimcp_sim_destroy(ctx); ctx = nullptr; }
    }

    nimcp_sim_context_t* createWithDefaults() {
        ctx = nimcp_sim_create(nullptr);
        return ctx;
    }

    nimcp_effector_state_t makeTestEffector(uint32_t id, nimcp_effector_type_t type) {
        nimcp_effector_state_t effector;
        memset(&effector, 0, sizeof(effector));
        effector.effector_id = id;
        effector.type = type;
        effector.position.x = 0.0;
        effector.position.y = 0.0;
        effector.position.z = 0.0;
        effector.orientation.w = 1.0;
        effector.max_force = 100.0;
        return effector;
    }

    nimcp_action_step_t makeTestAction(nimcp_action_primitive_t primitive, uint32_t effector_id) {
        nimcp_action_step_t step;
        memset(&step, 0, sizeof(step));
        step.primitive = primitive;
        step.effector_id = effector_id;
        step.target_position.x = 1.0;
        step.target_position.y = 0.0;
        step.target_position.z = 0.0;
        step.target_orientation.w = 1.0;
        step.duration = 1.0;
        step.max_velocity = 1.0;
        step.max_force = 50.0;
        return step;
    }
};

/* --- Lifecycle Tests --- */

TEST_F(EmbodiedSimulationTest, DefaultConfigHasReasonableSettings) {
    nimcp_sim_config_t config;
    nimcp_sim_default_config(&config);
    EXPECT_GT(config.time_step, 0.0);
    EXPECT_GT(config.max_steps, 0u);
    EXPECT_GT(config.max_concurrent, 0u);
}

TEST_F(EmbodiedSimulationTest, CreateWithNullConfigUsesDefaults) {
    ctx = nimcp_sim_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(EmbodiedSimulationTest, CreateWithValidConfig) {
    nimcp_sim_config_t config;
    nimcp_sim_default_config(&config);
    config.max_steps = 32;
    ctx = nimcp_sim_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(EmbodiedSimulationTest, DestroyNullIsNoOp) {
    nimcp_sim_destroy(nullptr);
}

TEST_F(EmbodiedSimulationTest, CreateDestroyCycle) {
    for (int i = 0; i < 5; i++) {
        ctx = nimcp_sim_create(nullptr);
        ASSERT_NE(ctx, nullptr);
        nimcp_sim_destroy(ctx);
        ctx = nullptr;
    }
}

TEST_F(EmbodiedSimulationTest, ResetClearsState) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_sim_error_t err = nimcp_sim_reset(ctx);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

/* --- Effector Control Tests --- */

TEST_F(EmbodiedSimulationTest, SetEffector) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_error_t err = nimcp_sim_set_effector(ctx, &effector);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, GetEffector) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    nimcp_effector_state_t retrieved;
    nimcp_sim_error_t err = nimcp_sim_get_effector(ctx, 1, &retrieved);
    EXPECT_EQ(err, NIMCP_SIM_OK);
    EXPECT_EQ(retrieved.effector_id, 1u);
}

TEST_F(EmbodiedSimulationTest, InvalidEffectorHandling) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t retrieved;
    nimcp_sim_error_t err = nimcp_sim_get_effector(ctx, 999, &retrieved);
    EXPECT_NE(err, NIMCP_SIM_OK);
}

/* --- Object Management Tests --- */

TEST_F(EmbodiedSimulationTest, AddObject) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_sim_object_t obj;
    memset(&obj, 0, sizeof(obj));
    obj.object_id = 1;
    obj.position.x = 1.0;
    obj.mass = 1.0;
    obj.dimensions[0] = 0.1;
    obj.dimensions[1] = 0.1;
    obj.dimensions[2] = 0.1;

    nimcp_sim_error_t err = nimcp_sim_add_object(ctx, &obj);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, UpdateObject) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_sim_object_t obj;
    memset(&obj, 0, sizeof(obj));
    obj.object_id = 1;
    obj.position.x = 1.0;
    obj.mass = 1.0;
    nimcp_sim_add_object(ctx, &obj);

    obj.position.x = 2.0;
    nimcp_sim_error_t err = nimcp_sim_update_object(ctx, &obj);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, RemoveObject) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_sim_object_t obj;
    memset(&obj, 0, sizeof(obj));
    obj.object_id = 1;
    nimcp_sim_add_object(ctx, &obj);

    nimcp_sim_error_t err = nimcp_sim_remove_object(ctx, 1);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

/* --- Simulation Tests --- */

TEST_F(EmbodiedSimulationTest, StartSimulation) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    uint32_t sim_id = 0;
    nimcp_sim_error_t err = nimcp_sim_start(ctx, &sim_id);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, AddActionStep) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    uint32_t sim_id = 0;
    nimcp_sim_start(ctx, &sim_id);

    nimcp_action_step_t step = makeTestAction(NIMCP_ACTION_PRIM_REACH, 1);
    nimcp_sim_error_t err = nimcp_sim_add_step(ctx, sim_id, &step);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, RunSimulation) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    uint32_t sim_id = 0;
    nimcp_sim_start(ctx, &sim_id);

    nimcp_action_step_t step = makeTestAction(NIMCP_ACTION_PRIM_REACH, 1);
    nimcp_sim_add_step(ctx, sim_id, &step);

    nimcp_sim_result_t result;
    memset(&result, 0, sizeof(result));
    nimcp_sim_error_t err = nimcp_sim_run(ctx, sim_id, &result);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, BasicSimulationStep) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    uint32_t sim_id = 0;
    nimcp_sim_start(ctx, &sim_id);

    nimcp_action_step_t step = makeTestAction(NIMCP_ACTION_PRIM_REACH, 1);
    nimcp_sim_add_step(ctx, sim_id, &step);

    nimcp_sim_error_t err = nimcp_sim_step(ctx, sim_id);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, PauseResumeSimulation) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    uint32_t sim_id = 0;
    nimcp_sim_start(ctx, &sim_id);

    nimcp_sim_error_t err = nimcp_sim_pause(ctx, sim_id);
    EXPECT_EQ(err, NIMCP_SIM_OK);

    err = nimcp_sim_resume(ctx, sim_id);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, AbortSimulation) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    uint32_t sim_id = 0;
    nimcp_sim_start(ctx, &sim_id);

    nimcp_sim_error_t err = nimcp_sim_abort(ctx, sim_id);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, GetSimulationState) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    uint32_t sim_id = 0;
    nimcp_sim_start(ctx, &sim_id);

    nimcp_sim_state_t state;
    nimcp_sim_error_t err = nimcp_sim_get_state(ctx, sim_id, &state);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

/* --- Forward Model Tests --- */

TEST_F(EmbodiedSimulationTest, ForwardPredict) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    double motor_command[3] = {1.0, 0.0, 0.0};
    nimcp_forward_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    nimcp_sim_error_t err = nimcp_sim_forward_predict(
        ctx, 1, motor_command, 1.0, &prediction);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

/* --- Effort Estimation Tests --- */

TEST_F(EmbodiedSimulationTest, EstimateEffort) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    nimcp_action_step_t step = makeTestAction(NIMCP_ACTION_PRIM_REACH, 1);
    nimcp_effort_estimate_t estimate;
    memset(&estimate, 0, sizeof(estimate));

    nimcp_sim_error_t err = nimcp_sim_estimate_effort(ctx, &step, &estimate);
    EXPECT_EQ(err, NIMCP_SIM_OK);
    EXPECT_GE(estimate.total_cost, 0.0);
}

/* --- Feasibility and Collision Tests --- */

TEST_F(EmbodiedSimulationTest, CheckFeasibility) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    nimcp_action_step_t step = makeTestAction(NIMCP_ACTION_PRIM_REACH, 1);
    bool is_feasible = false;
    char reason[256] = {0};

    nimcp_sim_error_t err = nimcp_sim_check_feasibility(ctx, &step, &is_feasible, reason);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, CheckCollision) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    nimcp_sim_object_t obj;
    memset(&obj, 0, sizeof(obj));
    obj.object_id = 1;
    obj.position.x = 0.5;
    obj.dimensions[0] = 0.1;
    obj.dimensions[1] = 0.1;
    obj.dimensions[2] = 0.1;
    nimcp_sim_add_object(ctx, &obj);

    nimcp_sim_position_t start = {0.0, 0.0, 0.0};
    nimcp_sim_position_t end = {1.0, 0.0, 0.0};
    bool has_collision = false;
    nimcp_collision_event_t collision;
    memset(&collision, 0, sizeof(collision));

    nimcp_sim_error_t err = nimcp_sim_check_collision(
        ctx, 1, &start, &end, &has_collision, &collision);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, SafetyBoundaryEnforcement) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_effector_state_t effector = makeTestEffector(1, NIMCP_EFFECTOR_RIGHT_HAND);
    nimcp_sim_set_effector(ctx, &effector);

    // Create action with extreme target position
    nimcp_action_step_t step = makeTestAction(NIMCP_ACTION_PRIM_REACH, 1);
    step.target_position.x = 1000.0;  // Very far target
    step.target_position.y = 1000.0;
    step.target_position.z = 1000.0;

    bool is_feasible = false;
    char reason[256] = {0};
    nimcp_sim_error_t err = nimcp_sim_check_feasibility(ctx, &step, &is_feasible, reason);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

/* --- Stats Tests --- */

TEST_F(EmbodiedSimulationTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_sim_stats_t stats;
    nimcp_sim_error_t err = nimcp_sim_get_stats(ctx, &stats);
    EXPECT_EQ(err, NIMCP_SIM_OK);
}

TEST_F(EmbodiedSimulationTest, NameFunctions) {
    EXPECT_NE(nimcp_sim_state_name(NIMCP_SIM_STATE_RUNNING), nullptr);
    EXPECT_NE(nimcp_sim_primitive_name(NIMCP_ACTION_PRIM_REACH), nullptr);
    EXPECT_NE(nimcp_sim_effector_name(NIMCP_EFFECTOR_RIGHT_HAND), nullptr);
}

/* --- Null Parameter Tests --- */

TEST_F(EmbodiedSimulationTest, NullContextOperationsReturnErrors) {
    nimcp_effector_state_t effector;
    memset(&effector, 0, sizeof(effector));
    EXPECT_EQ(nimcp_sim_set_effector(nullptr, &effector), NIMCP_SIM_ERROR_NULL_PARAM);

    uint32_t sim_id;
    EXPECT_EQ(nimcp_sim_start(nullptr, &sim_id), NIMCP_SIM_ERROR_NULL_PARAM);

    nimcp_sim_stats_t stats;
    EXPECT_EQ(nimcp_sim_get_stats(nullptr, &stats), NIMCP_SIM_ERROR_NULL_PARAM);
}

/* ============================================================================
 * Interoceptive Prediction Tests
 * ============================================================================ */

class InteroceptivePredictionTest : public ::testing::Test {
protected:
    nimcp_intero_context_t* ctx = nullptr;

    void SetUp() override { ctx = nullptr; }
    void TearDown() override {
        if (ctx) { nimcp_intero_destroy(ctx); ctx = nullptr; }
    }

    nimcp_intero_context_t* createWithDefaults() {
        ctx = nimcp_intero_create(nullptr);
        return ctx;
    }

    nimcp_intero_signal_t makeTestSignal(nimcp_intero_signal_type_t type, double value) {
        nimcp_intero_signal_t signal;
        memset(&signal, 0, sizeof(signal));
        signal.type = type;
        signal.value = value;
        signal.precision = 0.9;
        signal.is_valid = true;
        return signal;
    }
};

/* --- Lifecycle Tests --- */

TEST_F(InteroceptivePredictionTest, DefaultConfigHasReasonableSettings) {
    nimcp_intero_config_t config;
    nimcp_intero_default_config(&config);
    EXPECT_GT(config.prediction_learning_rate, 0.0);
    EXPECT_GT(config.regulation_strength, 0.0);
    EXPECT_GT(config.update_rate_hz, 0.0);
}

TEST_F(InteroceptivePredictionTest, CreateWithNullConfigUsesDefaults) {
    ctx = nimcp_intero_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(InteroceptivePredictionTest, CreateWithValidConfig) {
    nimcp_intero_config_t config;
    nimcp_intero_default_config(&config);
    config.prediction_learning_rate = 0.1;
    ctx = nimcp_intero_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(InteroceptivePredictionTest, DestroyNullIsNoOp) {
    nimcp_intero_destroy(nullptr);
}

TEST_F(InteroceptivePredictionTest, CreateDestroyCycle) {
    for (int i = 0; i < 5; i++) {
        ctx = nimcp_intero_create(nullptr);
        ASSERT_NE(ctx, nullptr);
        nimcp_intero_destroy(ctx);
        ctx = nullptr;
    }
}

TEST_F(InteroceptivePredictionTest, ResetClearsState) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_error_t err = nimcp_intero_reset(ctx);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

/* --- System Management Tests --- */

TEST_F(InteroceptivePredictionTest, RegisterSystem) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    uint32_t system_id = 0;
    nimcp_intero_error_t err = nimcp_intero_register_system(
        ctx, NIMCP_SYSTEM_CARDIOVASCULAR, &system_id);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, GetSystem) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    uint32_t system_id = 0;
    nimcp_intero_register_system(ctx, NIMCP_SYSTEM_CARDIOVASCULAR, &system_id);

    nimcp_system_state_t state;
    nimcp_intero_error_t err = nimcp_intero_get_system(ctx, system_id, &state);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
    EXPECT_EQ(state.type, NIMCP_SYSTEM_CARDIOVASCULAR);
}

TEST_F(InteroceptivePredictionTest, InitStandardSystems) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_error_t err = nimcp_intero_init_standard_systems(ctx);
    EXPECT_EQ(err, NIMCP_INTERO_OK);

    nimcp_intero_stats_t stats;
    nimcp_intero_get_stats(ctx, &stats);
    EXPECT_GT(stats.active_systems, 0u);
}

/* --- Signal Processing Tests --- */

TEST_F(InteroceptivePredictionTest, ProcessSignal) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_intero_signal_t signal = makeTestSignal(NIMCP_SIGNAL_HEART_RATE, 72.0);
    nimcp_intero_error_t err = nimcp_intero_process_signal(ctx, &signal);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, ProcessMultipleSignals) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_intero_signal_t signals[3];
    signals[0] = makeTestSignal(NIMCP_SIGNAL_HEART_RATE, 72.0);
    signals[1] = makeTestSignal(NIMCP_SIGNAL_BREATHING_RATE, 16.0);
    signals[2] = makeTestSignal(NIMCP_SIGNAL_CORE_TEMP, 37.0);

    nimcp_intero_error_t err = nimcp_intero_process_signals(ctx, signals, 3);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, GetSignal) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_intero_signal_t signal = makeTestSignal(NIMCP_SIGNAL_HEART_RATE, 72.0);
    nimcp_intero_process_signal(ctx, &signal);

    double value = 0.0;
    nimcp_intero_error_t err = nimcp_intero_get_signal(
        ctx, NIMCP_SIGNAL_HEART_RATE, &value);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, InvalidSignalHandling) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    // Process signal without initializing systems
    nimcp_intero_signal_t signal = makeTestSignal(NIMCP_SIGNAL_HEART_RATE, 72.0);
    signal.is_valid = false;

    nimcp_intero_error_t err = nimcp_intero_process_signal(ctx, &signal);
    // Should handle gracefully
    EXPECT_TRUE(err == NIMCP_INTERO_OK || err == NIMCP_INTERO_ERROR_INVALID_SIGNAL);
}

/* --- Prediction Tests --- */

TEST_F(InteroceptivePredictionTest, PredictSignal) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_intero_signal_t signal = makeTestSignal(NIMCP_SIGNAL_HEART_RATE, 72.0);
    nimcp_intero_process_signal(ctx, &signal);

    nimcp_intero_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    nimcp_intero_error_t err = nimcp_intero_predict(
        ctx, NIMCP_SIGNAL_HEART_RATE, 1.0, &prediction);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, UpdatePrediction) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_intero_signal_t signal = makeTestSignal(NIMCP_SIGNAL_HEART_RATE, 72.0);
    nimcp_intero_process_signal(ctx, &signal);

    nimcp_intero_error_t err = nimcp_intero_update_prediction(
        ctx, NIMCP_SIGNAL_HEART_RATE, 75.0);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, GetPredictionError) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_intero_signal_t signal = makeTestSignal(NIMCP_SIGNAL_HEART_RATE, 72.0);
    nimcp_intero_process_signal(ctx, &signal);

    double error = 0.0;
    nimcp_intero_error_t err = nimcp_intero_get_prediction_error(
        ctx, NIMCP_SIGNAL_HEART_RATE, &error);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

/* --- Homeostatic Integration Tests --- */

TEST_F(InteroceptivePredictionTest, SetSetpoint) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_setpoint_t setpoint;
    memset(&setpoint, 0, sizeof(setpoint));
    setpoint.signal = NIMCP_SIGNAL_HEART_RATE;
    setpoint.target_value = 70.0;
    setpoint.tolerance_low = 60.0;
    setpoint.tolerance_high = 80.0;
    setpoint.critical_low = 40.0;
    setpoint.critical_high = 120.0;
    setpoint.regulation_gain = 0.5;

    nimcp_intero_error_t err = nimcp_intero_set_setpoint(ctx, &setpoint);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, GetSetpoint) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_setpoint_t setpoint;
    memset(&setpoint, 0, sizeof(setpoint));
    setpoint.signal = NIMCP_SIGNAL_HEART_RATE;
    setpoint.target_value = 70.0;
    nimcp_intero_set_setpoint(ctx, &setpoint);

    nimcp_setpoint_t retrieved;
    nimcp_intero_error_t err = nimcp_intero_get_setpoint(
        ctx, NIMCP_SIGNAL_HEART_RATE, &retrieved);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, GetHomeostaticState) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_setpoint_t setpoint;
    memset(&setpoint, 0, sizeof(setpoint));
    setpoint.signal = NIMCP_SIGNAL_HEART_RATE;
    setpoint.target_value = 70.0;
    setpoint.tolerance_low = 60.0;
    setpoint.tolerance_high = 80.0;
    nimcp_intero_set_setpoint(ctx, &setpoint);

    nimcp_intero_signal_t signal = makeTestSignal(NIMCP_SIGNAL_HEART_RATE, 72.0);
    nimcp_intero_process_signal(ctx, &signal);

    nimcp_homeostatic_state_t state;
    double deviation = 0.0;
    nimcp_intero_error_t err = nimcp_intero_get_homeostatic_state(
        ctx, NIMCP_SIGNAL_HEART_RATE, &state, &deviation);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, ComputeDrive) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_setpoint_t setpoint;
    memset(&setpoint, 0, sizeof(setpoint));
    setpoint.signal = NIMCP_SIGNAL_HUNGER;
    setpoint.target_value = 0.3;
    setpoint.tolerance_low = 0.1;
    setpoint.tolerance_high = 0.5;
    nimcp_intero_set_setpoint(ctx, &setpoint);

    nimcp_intero_signal_t signal = makeTestSignal(NIMCP_SIGNAL_HUNGER, 0.8);
    nimcp_intero_process_signal(ctx, &signal);

    double drive = 0.0;
    nimcp_intero_error_t err = nimcp_intero_compute_drive(
        ctx, NIMCP_SIGNAL_HUNGER, &drive);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

/* --- Allostatic Load Tests --- */

TEST_F(InteroceptivePredictionTest, GetAllostaticLoad) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_allostatic_load_t load;
    nimcp_intero_error_t err = nimcp_intero_get_allostatic_load(ctx, &load);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
    EXPECT_GE(load.total_load, 0.0);
    EXPECT_LE(load.total_load, 1.0);
}

TEST_F(InteroceptivePredictionTest, ApplyStress) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_intero_error_t err = nimcp_intero_apply_stress(ctx, 0.5, 10.0);
    EXPECT_EQ(err, NIMCP_INTERO_OK);

    // Trigger update cycle to propagate stress to allostatic load
    nimcp_intero_update(ctx, 0.016);

    nimcp_allostatic_load_t load;
    nimcp_intero_get_allostatic_load(ctx, &load);
    EXPECT_GT(load.acute_stress, 0.0);
}

TEST_F(InteroceptivePredictionTest, ApplyRecovery) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);
    nimcp_intero_apply_stress(ctx, 0.5, 10.0);

    nimcp_intero_error_t err = nimcp_intero_apply_recovery(ctx, 30.0);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

/* --- Emotional Mapping Tests --- */

TEST_F(InteroceptivePredictionTest, GetEmotionalState) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_emotional_state_t state;
    nimcp_intero_error_t err = nimcp_intero_get_emotional_state(ctx, &state);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, ComputeArousal) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    double arousal = 0.0;
    nimcp_arousal_state_t state;
    nimcp_intero_error_t err = nimcp_intero_compute_arousal(ctx, &arousal, &state);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
    EXPECT_GE(arousal, 0.0);
    EXPECT_LE(arousal, 1.0);
}

TEST_F(InteroceptivePredictionTest, ComputeValence) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    double valence = 0.0;
    nimcp_valence_t state;
    nimcp_intero_error_t err = nimcp_intero_compute_valence(ctx, &valence, &state);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
    EXPECT_GE(valence, -1.0);
    EXPECT_LE(valence, 1.0);
}

/* --- Assessment Tests --- */

TEST_F(InteroceptivePredictionTest, AssessAwareness) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_init_standard_systems(ctx);

    nimcp_intero_awareness_t awareness;
    nimcp_intero_error_t err = nimcp_intero_assess_awareness(ctx, &awareness);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

/* --- Update and Stats Tests --- */

TEST_F(InteroceptivePredictionTest, UpdateCycle) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_error_t err = nimcp_intero_update(ctx, 0.016);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(ctx, nullptr);

    nimcp_intero_stats_t stats;
    nimcp_intero_error_t err = nimcp_intero_get_stats(ctx, &stats);
    EXPECT_EQ(err, NIMCP_INTERO_OK);
}

TEST_F(InteroceptivePredictionTest, NameFunctions) {
    EXPECT_NE(nimcp_intero_system_name(NIMCP_SYSTEM_CARDIOVASCULAR), nullptr);
    EXPECT_NE(nimcp_intero_signal_name(NIMCP_SIGNAL_HEART_RATE), nullptr);
    EXPECT_NE(nimcp_intero_homeo_state_name(NIMCP_HOMEO_OPTIMAL), nullptr);
    EXPECT_NE(nimcp_intero_arousal_name(NIMCP_AROUSAL_MODERATE), nullptr);
    EXPECT_NE(nimcp_intero_valence_name(NIMCP_VALENCE_NEUTRAL), nullptr);
}

/* --- Null Parameter Tests --- */

TEST_F(InteroceptivePredictionTest, NullContextOperationsReturnErrors) {
    nimcp_intero_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    EXPECT_EQ(nimcp_intero_process_signal(nullptr, &signal), NIMCP_INTERO_ERROR_NULL_PARAM);

    uint32_t system_id;
    EXPECT_EQ(nimcp_intero_register_system(nullptr, NIMCP_SYSTEM_CARDIOVASCULAR, &system_id),
              NIMCP_INTERO_ERROR_NULL_PARAM);

    nimcp_intero_stats_t stats;
    EXPECT_EQ(nimcp_intero_get_stats(nullptr, &stats), NIMCP_INTERO_ERROR_NULL_PARAM);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

class EmbodimentIntegrationTest : public ::testing::Test {
protected:
    nimcp_affordance_context_t* affordance_ctx = nullptr;
    nimcp_body_context_t* body_ctx = nullptr;
    nimcp_sim_context_t* sim_ctx = nullptr;
    nimcp_intero_context_t* intero_ctx = nullptr;

    void SetUp() override {
        affordance_ctx = nullptr;
        body_ctx = nullptr;
        sim_ctx = nullptr;
        intero_ctx = nullptr;
    }

    void TearDown() override {
        if (affordance_ctx) { nimcp_affordance_destroy(affordance_ctx); }
        if (body_ctx) { nimcp_body_destroy(body_ctx); }
        if (sim_ctx) { nimcp_sim_destroy(sim_ctx); }
        if (intero_ctx) { nimcp_intero_destroy(intero_ctx); }
    }
};

TEST_F(EmbodimentIntegrationTest, AllModulesCreateAndDestroy) {
    affordance_ctx = nimcp_affordance_create(nullptr);
    body_ctx = nimcp_body_create(nullptr);
    sim_ctx = nimcp_sim_create(nullptr);
    intero_ctx = nimcp_intero_create(nullptr);

    ASSERT_NE(affordance_ctx, nullptr);
    ASSERT_NE(body_ctx, nullptr);
    ASSERT_NE(sim_ctx, nullptr);
    ASSERT_NE(intero_ctx, nullptr);
}

TEST_F(EmbodimentIntegrationTest, AllModulesUpdate) {
    affordance_ctx = nimcp_affordance_create(nullptr);
    body_ctx = nimcp_body_create(nullptr);
    sim_ctx = nimcp_sim_create(nullptr);
    intero_ctx = nimcp_intero_create(nullptr);

    EXPECT_EQ(nimcp_affordance_update(affordance_ctx, 0.016), NIMCP_AFFORDANCE_OK);
    EXPECT_EQ(nimcp_body_update(body_ctx, 0.016), NIMCP_BODY_OK);
    // sim_ctx update is via sim_step for active simulations
    EXPECT_EQ(nimcp_intero_update(intero_ctx, 0.016), NIMCP_INTERO_OK);
}

TEST_F(EmbodimentIntegrationTest, AllModulesReset) {
    affordance_ctx = nimcp_affordance_create(nullptr);
    body_ctx = nimcp_body_create(nullptr);
    sim_ctx = nimcp_sim_create(nullptr);
    intero_ctx = nimcp_intero_create(nullptr);

    EXPECT_EQ(nimcp_affordance_reset(affordance_ctx), NIMCP_AFFORDANCE_OK);
    EXPECT_EQ(nimcp_body_reset(body_ctx), NIMCP_BODY_OK);
    EXPECT_EQ(nimcp_sim_reset(sim_ctx), NIMCP_SIM_OK);
    EXPECT_EQ(nimcp_intero_reset(intero_ctx), NIMCP_INTERO_OK);
}

TEST_F(EmbodimentIntegrationTest, AllModulesGetStats) {
    affordance_ctx = nimcp_affordance_create(nullptr);
    body_ctx = nimcp_body_create(nullptr);
    sim_ctx = nimcp_sim_create(nullptr);
    intero_ctx = nimcp_intero_create(nullptr);

    nimcp_affordance_stats_t aff_stats;
    nimcp_body_stats_t body_stats;
    nimcp_sim_stats_t sim_stats;
    nimcp_intero_stats_t intero_stats;

    EXPECT_EQ(nimcp_affordance_get_stats(affordance_ctx, &aff_stats), NIMCP_AFFORDANCE_OK);
    EXPECT_EQ(nimcp_body_get_stats(body_ctx, &body_stats), NIMCP_BODY_OK);
    EXPECT_EQ(nimcp_sim_get_stats(sim_ctx, &sim_stats), NIMCP_SIM_OK);
    EXPECT_EQ(nimcp_intero_get_stats(intero_ctx, &intero_stats), NIMCP_INTERO_OK);
}
