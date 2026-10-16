/**
 * @file test_cognitive_modules_wiring.cpp
 * @brief Unit tests for cognitive module APIs used in brain_learn_vector
 *
 * WHAT: Verify each cognitive module's API is callable and struct-compatible
 * WHY:  brain_learn_vector calls 8 cognitive module APIs during training;
 *       if any struct layout or function signature changes, training breaks silently
 * HOW:  Test struct construction, create/destroy lifecycle, NULL safety
 *
 * Modules tested:
 * 1. RCOG (recursive cognition) — rcog_engine_create_goal, rcog_engine_process
 * 2. Ethics — action_context_t, ethics_engine_evaluate_action
 * 3. Engram (memory) — engram_system_create/destroy
 * 4. Theory of Mind — tom_observation_t, tom_observe, tom_update_self_model
 * 5. Dragonfly — dragonfly_detection_t, dragonfly_process_detection
 * 6. Portia — portia_is_initialized, portia_update
 * 7. Collective — collective_cognition_update
 * 8. Introspection — connectivity_health_config_t, introspection_assess_connectivity_health
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "portia/nimcp_portia.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/introspection/nimcp_connectivity_health.h"
}

// ============================================================================
// RCOG (Recursive Cognition) Wiring Tests
// ============================================================================

TEST(CognitiveWiring, RCOGGoalCreation) {
    rcog_goal_t goal = rcog_engine_create_goal("test query", RCOG_GOAL_REASONING);
    EXPECT_EQ(goal.type, RCOG_GOAL_REASONING);
    EXPECT_STREQ(goal.query, "test query");
}

TEST(CognitiveWiring, RCOGGoalAllTypes) {
    rcog_goal_type_t types[] = {
        RCOG_GOAL_QUESTION_ANSWERING, RCOG_GOAL_SUMMARIZATION,
        RCOG_GOAL_EXTRACTION, RCOG_GOAL_REASONING,
        RCOG_GOAL_PLANNING, RCOG_GOAL_ANALYSIS,
        RCOG_GOAL_GENERATION, RCOG_GOAL_TRANSLATION,
        RCOG_GOAL_VALIDATION, RCOG_GOAL_CUSTOM
    };
    for (auto type : types) {
        rcog_goal_t goal = rcog_engine_create_goal("test", type);
        EXPECT_EQ(goal.type, type) << "Failed for type " << (int)type;
    }
}

TEST(CognitiveWiring, RCOGProcessWithNullEngine) {
    rcog_goal_t goal = rcog_engine_create_goal("test", RCOG_GOAL_REASONING);
    rcog_process_result_t result;
    memset(&result, 0, sizeof(result));
    int rc = rcog_engine_process(NULL, &goal, &result);
    EXPECT_NE(rc, 0) << "rcog_engine_process(NULL) should fail gracefully";
}

TEST(CognitiveWiring, RCOGGoalEmptyQuery) {
    rcog_goal_t goal = rcog_engine_create_goal("", RCOG_GOAL_REASONING);
    EXPECT_EQ(goal.type, RCOG_GOAL_REASONING);
    // Empty query is valid — the engine decides how to handle it
}

TEST(CognitiveWiring, RCOGGoalNullQuery) {
    rcog_goal_t goal = rcog_engine_create_goal(NULL, RCOG_GOAL_REASONING);
    EXPECT_EQ(goal.type, RCOG_GOAL_REASONING);
    EXPECT_EQ(goal.query, nullptr);
}

TEST(CognitiveWiring, RCOGProcessResultZeroed) {
    rcog_process_result_t result;
    memset(&result, 0, sizeof(result));
    EXPECT_EQ(result.success, false);
    EXPECT_EQ(result.subtasks_created, 0u);
    EXPECT_EQ(result.processing_time_ms, 0u);
}

TEST(CognitiveWiring, RCOGEngineDestroyNull) {
    // NULL-safe destroy
    rcog_engine_destroy(NULL);
    SUCCEED() << "rcog_engine_destroy(NULL) did not crash";
}

TEST(CognitiveWiring, RCOGErrorNameCoverage) {
    EXPECT_STREQ(rcog_error_name(RCOG_OK), "OK");
    EXPECT_STREQ(rcog_error_name(RCOG_ERROR_NULL_POINTER), "NULL_POINTER");
    EXPECT_STREQ(rcog_error_name(RCOG_ERROR_MAX_DEPTH_EXCEEDED), "MAX_DEPTH_EXCEEDED");
}

// ============================================================================
// Ethics Wiring Tests
// ============================================================================

TEST(CognitiveWiring, EthicsActionContextFields) {
    action_context_t action;
    memset(&action, 0, sizeof(action));
    action.predicted_harm = 0.8f;
    action.fairness_violation = 0.3f;
    action.deception_level = 0.1f;
    action.autonomy_violation = 0.5f;
    action.privacy_violation = 0.2f;
    action.consent_violation = 0.4f;
    EXPECT_FLOAT_EQ(action.predicted_harm, 0.8f);
    EXPECT_FLOAT_EQ(action.fairness_violation, 0.3f);
    EXPECT_FLOAT_EQ(action.deception_level, 0.1f);
    EXPECT_FLOAT_EQ(action.autonomy_violation, 0.5f);
    EXPECT_FLOAT_EQ(action.privacy_violation, 0.2f);
    EXPECT_FLOAT_EQ(action.consent_violation, 0.4f);
}

TEST(CognitiveWiring, EthicsActionContextWithFeatures) {
    // Mirror the C code pattern in brain_learn_vector
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    action_context_t action;
    memset(&action, 0, sizeof(action));
    action.features = features;
    action.num_features = 10;
    action.predicted_harm = fminf(500.0f / 1000.0f, 1.0f); // loss=500
    action.fairness_violation = fminf(fabsf(features[0]), 1.0f);
    action.deception_level = fminf(fabsf(features[1]), 1.0f);
    action.autonomy_violation = fminf(fabsf(features[2]), 1.0f);
    action.privacy_violation = fminf(fabsf(features[3]), 1.0f);
    action.consent_violation = fminf(fabsf(features[4]), 1.0f);

    EXPECT_FLOAT_EQ(action.predicted_harm, 0.5f);
    EXPECT_EQ(action.num_features, 10u);
    EXPECT_EQ(action.features, features);
}

TEST(CognitiveWiring, EthicsActionContextZeroFeatures) {
    action_context_t action;
    memset(&action, 0, sizeof(action));
    // Zero features — the C code gates on num_features >= 5
    action.num_features = 0;
    action.features = NULL;
    EXPECT_EQ(action.num_features, 0u);
}

TEST(CognitiveWiring, EthicsEvaluateWithNullEngine) {
    action_context_t action;
    memset(&action, 0, sizeof(action));
    action.predicted_harm = 0.5f;
    // ethics_engine_evaluate_action takes ethics_engine_t (pointer typedef)
    // NULL engine should return a safe default
    ethics_evaluation_t result = ethics_engine_evaluate_action(NULL, &action);
    // Should not crash; result fields should have safe defaults
    (void)result;
    SUCCEED() << "ethics_engine_evaluate_action(NULL) did not crash";
}

TEST(CognitiveWiring, EthicsViolationTypeEnum) {
    EXPECT_EQ(ETHICS_VIOLATION_TYPE_NONE, 0);
    EXPECT_NE(ETHICS_VIOLATION_TYPE_HARM, 0);
    EXPECT_NE(ETHICS_VIOLATION_TYPE_GOLDEN_RULE, 0);
}

// ============================================================================
// Engram (Memory) Wiring Tests
// ============================================================================

TEST(CognitiveWiring, EngramSystemCreateDestroy) {
    engram_system_t* es = engram_system_create();
    ASSERT_NE(es, nullptr);
    EXPECT_EQ(es->active_count, 0u);
    engram_system_destroy(es);
}

TEST(CognitiveWiring, EngramDestroyNull) {
    engram_system_destroy(NULL);
    SUCCEED() << "engram_system_destroy(NULL) did not crash";
}

TEST(CognitiveWiring, EngramSystemResetWorks) {
    engram_system_t* es = engram_system_create();
    ASSERT_NE(es, nullptr);
    engram_system_reset(es);
    EXPECT_EQ(es->active_count, 0u);
    engram_system_destroy(es);
}

// ============================================================================
// Theory of Mind Wiring Tests
// ============================================================================

TEST(CognitiveWiring, TomObservationStructConstruction) {
    tom_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    float action_vec[10] = {1.0f, 0.5f};
    obs.action_vector = action_vec;
    obs.action_dim = 10;
    obs.verbal_context = "test label";
    obs.observed_emotion = TOM_EMOTION_JOY;
    EXPECT_EQ(obs.action_dim, 10u);
    EXPECT_EQ(obs.observed_emotion, TOM_EMOTION_JOY);
    EXPECT_STREQ(obs.verbal_context, "test label");
}

TEST(CognitiveWiring, TomEmotionEnumValues) {
    EXPECT_EQ(TOM_EMOTION_UNKNOWN, 0);
    EXPECT_NE(TOM_EMOTION_JOY, TOM_EMOTION_SADNESS);
    EXPECT_NE(TOM_EMOTION_SURPRISE, TOM_EMOTION_ANGER);
    // Verify COUNT is reasonable
    EXPECT_GE(TOM_EMOTION_COUNT, 8);
}

TEST(CognitiveWiring, TomObserveNullHandle) {
    tom_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    bool result = tom_observe(NULL, &obs);
    // Should return false (fail gracefully), not crash
    EXPECT_FALSE(result);
}

TEST(CognitiveWiring, TomUpdateSelfModelNullHandle) {
    float features[5] = {1.0f};
    bool result = tom_update_self_model(NULL, features, 5, "test", 0.9f);
    EXPECT_FALSE(result);
}

TEST(CognitiveWiring, TomEmotionToString) {
    const char* name = tom_emotion_to_string(TOM_EMOTION_JOY);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST(CognitiveWiring, TomBDIStateStructs) {
    tom_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.confidence = 0.8f;
    belief.is_false_belief = true;
    EXPECT_FLOAT_EQ(belief.confidence, 0.8f);
    EXPECT_TRUE(belief.is_false_belief);

    tom_desire_t desire;
    memset(&desire, 0, sizeof(desire));
    desire.intensity = 0.9f;
    EXPECT_FLOAT_EQ(desire.intensity, 0.9f);

    tom_intention_t intention;
    memset(&intention, 0, sizeof(intention));
    intention.likelihood = 0.7f;
    EXPECT_FLOAT_EQ(intention.likelihood, 0.7f);
}

// ============================================================================
// Dragonfly Wiring Tests
// ============================================================================

TEST(CognitiveWiring, DragonflyDetectionConstruction) {
    dragonfly_detection_t det;
    memset(&det, 0, sizeof(det));
    det.position[0] = 1.0f;
    det.position[1] = 2.0f;
    det.position[2] = 3.0f;
    det.size = 0.05f;
    det.contrast = 0.8f;
    det.motion_speed = 1.5f;
    det.timestamp_us = 1000000;
    det.id = 1;
    EXPECT_FLOAT_EQ(det.position[0], 1.0f);
    EXPECT_FLOAT_EQ(det.size, 0.05f);
    EXPECT_EQ(det.id, 1u);
}

TEST(CognitiveWiring, DragonflyProcessDetectionNullSystem) {
    dragonfly_detection_t det;
    memset(&det, 0, sizeof(det));
    det.position[0] = 1.0f;
    det.id = 1;
    int rc = dragonfly_process_detection(NULL, &det);
    EXPECT_NE(rc, 0) << "dragonfly_process_detection(NULL) should fail";
}

TEST(CognitiveWiring, DragonflySystemDestroyNull) {
    dragonfly_system_destroy(NULL);
    SUCCEED() << "dragonfly_system_destroy(NULL) did not crash";
}

// ============================================================================
// Portia Wiring Tests
// ============================================================================

TEST(CognitiveWiring, PortiaIsInitializedSafe) {
    // portia_is_initialized() should return false when not init'd
    bool init = portia_is_initialized();
    // We cannot guarantee it's false (another test might have init'd), but it should not crash
    (void)init;
    SUCCEED() << "portia_is_initialized() did not crash";
}

// ============================================================================
// Collective Cognition Wiring Tests
// ============================================================================

TEST(CognitiveWiring, CollectiveCognitionUpdateNull) {
    int rc = collective_cognition_update(NULL);
    EXPECT_NE(rc, 0) << "collective_cognition_update(NULL) should fail";
}

// ============================================================================
// Introspection Wiring Tests
// ============================================================================

TEST(CognitiveWiring, ConnectivityHealthConfigZeroed) {
    connectivity_health_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    // Zeroed config should not cause issues
    EXPECT_FLOAT_EQ(cfg.min_modularity, 0.0f);
    EXPECT_FLOAT_EQ(cfg.min_clustering_coefficient, 0.0f);
}

TEST(CognitiveWiring, ConnectivityHealthDefaultConfig) {
    connectivity_health_config_t cfg = connectivity_health_default_config();
    // Defaults should have reasonable non-zero values
    EXPECT_GT(cfg.min_modularity, 0.0f);
    EXPECT_GT(cfg.min_clustering_coefficient, 0.0f);
    EXPECT_GT(cfg.max_path_length, 0.0f);
}

TEST(CognitiveWiring, IntrospectionAssessWithNull) {
    // NULL introspection context should return safe defaults
    brain_connectivity_health_t health = introspection_assess_connectivity_health(NULL, NULL);
    // Should not crash; health should be zeroed or have safe values
    (void)health;
    SUCCEED() << "introspection_assess_connectivity_health(NULL, NULL) did not crash";
}
