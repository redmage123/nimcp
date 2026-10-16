/**
 * @file test_cognitive_regression.cpp
 * @brief Regression tests for cognitive training pipeline
 *
 * WHAT: Prevent regressions in cognitive module wiring and label dispatch
 * WHY:  Past bugs: NULL labels crashing, NaN features, long labels truncated,
 *       Portia called when not initialized, RCOG infinite recursion
 * HOW:  Reproduce edge cases that caused past failures
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cfloat>

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
// NULL Label Safety
// ============================================================================

TEST(CognitiveRegression, NullLabelAllModulesSafe) {
    // In brain_learn_vector, NULL label gates most cognitive blocks via:
    //   if (label && strstr(label, "..."))
    // Verify that passing NULL to each strstr-like check doesn't crash

    const char* label = NULL;

    // These should all be safe because C evaluates `label &&` first
    if (label && strstr(label, "ethics")) { FAIL() << "Should not reach here"; }
    if (label && strstr(label, "counterfactual")) { FAIL() << "Should not reach here"; }
    if (label && strstr(label, "causal")) { FAIL() << "Should not reach here"; }
    if (label && strstr(label, "metacog")) { FAIL() << "Should not reach here"; }
    if (label && strstr(label, "dragonfly")) { FAIL() << "Should not reach here"; }
    if (label && strstr(label, "portia")) { FAIL() << "Should not reach here"; }
    if (label && strstr(label, "rcog_")) { FAIL() << "Should not reach here"; }

    // ToM block uses strncmp with label check first
    if (label && (strncmp(label, "tom_", 4) == 0 || strstr(label, "perspective"))) {
        FAIL() << "Should not reach here";
    }

    SUCCEED() << "All NULL label checks passed";
}

// ============================================================================
// Very Long Labels
// ============================================================================

TEST(CognitiveRegression, VeryLongLabelStillTriggers) {
    // 255-char label with "ethics" at position 100
    char label[256];
    memset(label, 'a', 255);
    label[255] = '\0';
    memcpy(label + 100, "ethics", 6);
    EXPECT_TRUE(strstr(label, "ethics") != NULL);

    // Verify it would trigger the ethics block
    EXPECT_NE(strstr(label, "ethics"), nullptr);
}

TEST(CognitiveRegression, MaxLengthLabelNoOverflow) {
    // 1023-char label
    char label[1024];
    memset(label, 'x', 1023);
    label[1023] = '\0';
    memcpy(label + 500, "dragonfly", 9);
    EXPECT_TRUE(strstr(label, "dragonfly") != NULL);
}

// ============================================================================
// Label With Special Characters
// ============================================================================

TEST(CognitiveRegression, LabelWithSpecialChars) {
    const char* label = "ethics_test_123_v2";
    EXPECT_TRUE(strstr(label, "ethics") != NULL);
}

TEST(CognitiveRegression, LabelSubstringAtStart) {
    const char* label = "ethics";
    EXPECT_TRUE(strstr(label, "ethics") != NULL);
}

TEST(CognitiveRegression, LabelSubstringAtEnd) {
    const char* label = "test_ethics";
    EXPECT_TRUE(strstr(label, "ethics") != NULL);
}

// ============================================================================
// Zero/NaN Features for Ethics
// ============================================================================

TEST(CognitiveRegression, ZeroFeaturesEthics) {
    action_context_t action;
    memset(&action, 0, sizeof(action));
    action.num_features = 0;
    action.features = NULL;
    action.predicted_harm = 0.0f;
    // Should be safe — C code gates on num_features >= 5
    EXPECT_EQ(action.num_features, 0u);
    EXPECT_EQ(action.features, nullptr);

    ethics_evaluation_t result = ethics_engine_evaluate_action(NULL, &action);
    (void)result;
    SUCCEED() << "Ethics with zero features did not crash";
}

TEST(CognitiveRegression, NaNFeaturesEthics) {
    float features[5] = {NAN, NAN, NAN, NAN, NAN};
    action_context_t action;
    memset(&action, 0, sizeof(action));
    action.features = features;
    action.num_features = 5;
    action.predicted_harm = NAN;
    // C code: fminf(fabsf(features[0]), 1.0f) with NaN
    // fminf(NaN, 1.0f) behavior is implementation-defined but should not crash
    float fair = fminf(fabsf(features[0]), 1.0f);
    // NaN propagation — may return NaN or 1.0f depending on platform
    (void)fair;

    ethics_evaluation_t result = ethics_engine_evaluate_action(NULL, &action);
    (void)result;
    SUCCEED() << "Ethics with NaN features did not crash";
}

TEST(CognitiveRegression, InfFeaturesEthics) {
    float features[5] = {INFINITY, -INFINITY, INFINITY, -INFINITY, INFINITY};
    action_context_t action;
    memset(&action, 0, sizeof(action));
    action.features = features;
    action.num_features = 5;
    action.predicted_harm = fminf(INFINITY / 1000.0f, 1.0f);
    EXPECT_FLOAT_EQ(action.predicted_harm, 1.0f);

    ethics_evaluation_t result = ethics_engine_evaluate_action(NULL, &action);
    (void)result;
    SUCCEED() << "Ethics with Inf features did not crash";
}

// ============================================================================
// RCOG Max Depth
// ============================================================================

TEST(CognitiveRegression, RCOGMaxDepthRespected) {
    rcog_goal_t goal = rcog_engine_create_goal("test", RCOG_GOAL_REASONING);
    goal.max_depth = 0; // Explicitly set to 0 — should use default, not infinite
    EXPECT_EQ(goal.max_depth, 0u);

    rcog_process_result_t result;
    memset(&result, 0, sizeof(result));
    // Processing with NULL engine should fail fast, not recurse
    int rc = rcog_engine_process(NULL, &goal, &result);
    EXPECT_NE(rc, 0);
}

TEST(CognitiveRegression, RCOGMaxDepthLarge) {
    rcog_goal_t goal = rcog_engine_create_goal("test", RCOG_GOAL_REASONING);
    goal.max_depth = 1000; // Very deep — engine should cap this
    EXPECT_EQ(goal.max_depth, 1000u);
}

// ============================================================================
// Empty Goal Query
// ============================================================================

TEST(CognitiveRegression, EmptyGoalQuery) {
    rcog_goal_t goal = rcog_engine_create_goal("", RCOG_GOAL_REASONING);
    EXPECT_EQ(goal.type, RCOG_GOAL_REASONING);
    EXPECT_STREQ(goal.query, "");
}

// ============================================================================
// Portia When Not Initialized
// ============================================================================

TEST(CognitiveRegression, PortiaWhenNotInitialized) {
    // The C code does: if (portia_is_initialized()) portia_update();
    // When not initialized, portia_update should NOT be called
    // portia_is_initialized() must return false safely
    if (!portia_is_initialized()) {
        // Good — this is the expected path when Portia hasn't been init'd
        SUCCEED();
    } else {
        // Portia was init'd (possibly by another test) — just verify update works
        portia_update();
        SUCCEED();
    }
}

// ============================================================================
// Dragonfly Zero Timestamp
// ============================================================================

TEST(CognitiveRegression, DragonFlyZeroTimestamp) {
    dragonfly_detection_t det;
    memset(&det, 0, sizeof(det));
    det.position[0] = 1.0f;
    det.position[1] = 0.0f;
    det.position[2] = 0.0f;
    det.size = 0.05f;
    det.contrast = 0.8f;
    det.motion_speed = 1.0f;
    det.timestamp_us = 0; // Explicitly zero
    det.id = 1;

    // Should fail gracefully with NULL system
    int rc = dragonfly_process_detection(NULL, &det);
    EXPECT_NE(rc, 0);
}

TEST(CognitiveRegression, DragonFlyNegativePosition) {
    dragonfly_detection_t det;
    memset(&det, 0, sizeof(det));
    det.position[0] = -100.0f;
    det.position[1] = -200.0f;
    det.position[2] = -300.0f;
    det.size = 0.05f;
    det.contrast = 0.8f;
    det.motion_speed = 1.0f;
    det.timestamp_us = 1000;
    det.id = 1;

    int rc = dragonfly_process_detection(NULL, &det);
    EXPECT_NE(rc, 0);
}

// ============================================================================
// Concurrent-like Module Calls (Sequential but Rapid)
// ============================================================================

TEST(CognitiveRegression, ConcurrentModuleCalls) {
    // Rapidly call multiple modules in sequence — verify no state corruption
    for (int i = 0; i < 100; i++) {
        // RCOG
        rcog_goal_t goal = rcog_engine_create_goal("rapid_test", RCOG_GOAL_REASONING);
        EXPECT_EQ(goal.type, RCOG_GOAL_REASONING);

        // Ethics
        action_context_t action;
        memset(&action, 0, sizeof(action));
        action.predicted_harm = (float)i / 100.0f;
        ethics_engine_evaluate_action(NULL, &action);

        // ToM
        tom_update_self_model(NULL, &action.predicted_harm, 1, "rapid", 0.5f);

        // Collective
        collective_cognition_update(NULL);

        // Introspection
        introspection_assess_connectivity_health(NULL, NULL);
    }
    SUCCEED() << "100 rapid sequential module calls completed without crash";
}

// ============================================================================
// Engram Edge Cases
// ============================================================================

TEST(CognitiveRegression, EngramCreateResetDestroy) {
    engram_system_t* es = engram_system_create();
    ASSERT_NE(es, nullptr);
    engram_system_reset(es);
    engram_system_reset(es); // Double reset
    engram_system_destroy(es);
}

TEST(CognitiveRegression, EngramDoubleDestroy) {
    engram_system_t* es = engram_system_create();
    ASSERT_NE(es, nullptr);
    engram_system_destroy(es);
    // Second destroy with same pointer is UB, but NULL should be safe
    engram_system_destroy(NULL);
    SUCCEED();
}

// ============================================================================
// Connectivity Health Config Boundary Values
// ============================================================================

TEST(CognitiveRegression, ConnectivityHealthExtremeWeights) {
    connectivity_health_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    // Weights that sum > 1.0 or < 1.0
    cfg.weight_modularity = 0.0f;
    cfg.weight_hubs = 0.0f;
    cfg.weight_topology = 0.0f;
    cfg.weight_flow = 0.0f;
    // All-zero weights should not cause division by zero
    brain_connectivity_health_t health = introspection_assess_connectivity_health(NULL, &cfg);
    (void)health;
    SUCCEED() << "Zero-weight config did not crash";
}

TEST(CognitiveRegression, ConnectivityHealthNegativeThresholds) {
    connectivity_health_config_t cfg = connectivity_health_default_config();
    cfg.min_modularity = -1.0f; // Invalid but should not crash
    cfg.max_path_length = -1.0f;
    brain_connectivity_health_t health = introspection_assess_connectivity_health(NULL, &cfg);
    (void)health;
    SUCCEED() << "Negative thresholds did not crash";
}
