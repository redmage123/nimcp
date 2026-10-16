/**
 * @file test_nimcp_occipital_logic_bridge.cpp
 * @brief Unit tests for nimcp_occipital_logic_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Occipital-Logic bridge
 * WHY:  Ensure correct visual predicate grounding and inference
 * HOW:  Use Google Test framework to test predicates, inference, and stats
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_logic_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OccipitalLogicBridgeTest : public ::testing::Test {
protected:
    occipital_logic_bridge_t* bridge;
    occipital_adapter_t* occipital;
    occipital_logic_config_t config;
    occipital_config_t occipital_config;

    void SetUp() override {
        // Create occipital adapter
        occipital_config = occipital_default_config();
        occipital_config.image_width = 64;
        occipital_config.image_height = 64;
        occipital = occipital_create(&occipital_config);

        // Create logic bridge
        config = occipital_logic_default_config();
        bridge = occipital_logic_bridge_create(occipital, &config);
    }

    void TearDown() override {
        if (bridge) {
            occipital_logic_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (occipital) {
            occipital_destroy(occipital);
            occipital = nullptr;
        }
    }
};

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_logic_config_t default_config = occipital_logic_default_config();

    EXPECT_TRUE(default_config.enable_unary_predicates);
    EXPECT_TRUE(default_config.enable_binary_predicates);
    EXPECT_GT(default_config.detection_threshold, 0.0f);
    EXPECT_LT(default_config.detection_threshold, 1.0f);
}

TEST_F(OccipitalLogicBridgeTest, GetConfigReturnsCurrentConfig) {
    ASSERT_NE(nullptr, bridge);

    occipital_logic_config_t retrieved;
    EXPECT_EQ(0, occipital_logic_bridge_get_config(bridge, &retrieved));

    EXPECT_EQ(config.enable_unary_predicates, retrieved.enable_unary_predicates);
    EXPECT_EQ(config.enable_binary_predicates, retrieved.enable_binary_predicates);
}

TEST_F(OccipitalLogicBridgeTest, GetConfigWithNullBridgeFails) {
    occipital_logic_config_t retrieved;
    EXPECT_EQ(-1, occipital_logic_bridge_get_config(nullptr, &retrieved));
}

TEST_F(OccipitalLogicBridgeTest, GetConfigWithNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_logic_bridge_get_config(bridge, nullptr));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, CreateWithNullOccipitalReturnsNull) {
    // Bridge requires occipital lobe connection
    occipital_logic_bridge_t* standalone = occipital_logic_bridge_create(
        NULL, &config);
    EXPECT_EQ(nullptr, standalone);
}

TEST_F(OccipitalLogicBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_logic_bridge_t* default_bridge = occipital_logic_bridge_create(
        occipital, NULL);
    ASSERT_NE(nullptr, default_bridge);

    occipital_logic_config_t retrieved;
    EXPECT_EQ(0, occipital_logic_bridge_get_config(default_bridge, &retrieved));
    EXPECT_TRUE(retrieved.enable_unary_predicates);

    occipital_logic_bridge_destroy(default_bridge);
}

TEST_F(OccipitalLogicBridgeTest, DestroyNullDoesNotCrash) {
    occipital_logic_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OccipitalLogicBridgeTest, ResetBridgeSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_logic_bridge_reset(bridge));
}

TEST_F(OccipitalLogicBridgeTest, ResetNullBridgeFails) {
    EXPECT_EQ(-1, occipital_logic_bridge_reset(nullptr));
}

// ============================================================================
// PREDICATE TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, GroundPredicatesSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_logic_bridge_update(bridge);
    EXPECT_GE(occipital_logic_ground_predicates(bridge), 0);
}

TEST_F(OccipitalLogicBridgeTest, GroundPredicatesNullBridgeFails) {
    EXPECT_EQ(-1, occipital_logic_ground_predicates(nullptr));
}

TEST_F(OccipitalLogicBridgeTest, AssertPredicateSucceeds) {
    ASSERT_NE(nullptr, bridge);

    visual_predicate_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.type = PRED_OBJECT_PRESENT;
    pred.confidence = 0.9f;
    pred.truth_value = 0.9f;
    pred.object_a = 1;

    EXPECT_EQ(0, occipital_logic_assert_predicate(bridge, &pred));
}

TEST_F(OccipitalLogicBridgeTest, AssertPredicateNullBridgeFails) {
    visual_predicate_t pred;
    memset(&pred, 0, sizeof(pred));
    EXPECT_EQ(-1, occipital_logic_assert_predicate(nullptr, &pred));
}

TEST_F(OccipitalLogicBridgeTest, AssertPredicateNullPredFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_logic_assert_predicate(bridge, nullptr));
}

TEST_F(OccipitalLogicBridgeTest, GetPredicatesReturnsAsserted) {
    ASSERT_NE(nullptr, bridge);

    // Assert a test predicate
    visual_predicate_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.type = PRED_OBJECT_PRESENT;
    pred.confidence = 0.9f;
    pred.truth_value = 0.9f;
    pred.object_a = 5;

    EXPECT_EQ(0, occipital_logic_assert_predicate(bridge, &pred));

    // Retrieve predicates
    visual_predicate_t predicates[10];
    uint32_t count = 0;
    EXPECT_EQ(0, occipital_logic_get_predicates(bridge, predicates, 10, &count));
    EXPECT_GE(count, 1u);
}

TEST_F(OccipitalLogicBridgeTest, GetPredicatesNullBridgeFails) {
    visual_predicate_t predicates[10];
    uint32_t count = 0;
    EXPECT_EQ(-1, occipital_logic_get_predicates(nullptr, predicates, 10, &count));
}

TEST_F(OccipitalLogicBridgeTest, QueryPredicateReturnsValidResult) {
    ASSERT_NE(nullptr, bridge);

    // Assert a predicate
    visual_predicate_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.type = PRED_OBJECT_PRESENT;
    pred.confidence = 0.9f;
    pred.truth_value = 0.85f;
    pred.object_a = 5;

    occipital_logic_assert_predicate(bridge, &pred);

    // Query for it
    float truth_value = 0.0f;
    float confidence = 0.0f;
    int result = occipital_logic_query_predicate(bridge, PRED_OBJECT_PRESENT, 5, 0,
        &truth_value, &confidence);
    EXPECT_EQ(0, result);
}

TEST_F(OccipitalLogicBridgeTest, QueryPredicateNullBridgeFails) {
    float truth_value, confidence;
    EXPECT_EQ(-1, occipital_logic_query_predicate(nullptr, PRED_OBJECT_PRESENT, 1, 0,
        &truth_value, &confidence));
}

// ============================================================================
// INFERENCE TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, RunInferenceSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_logic_bridge_update(bridge);
    int inferences = occipital_logic_run_inference(bridge);
    EXPECT_GE(inferences, 0);
}

TEST_F(OccipitalLogicBridgeTest, RunInferenceNullBridgeFails) {
    EXPECT_EQ(-1, occipital_logic_run_inference(nullptr));
}

TEST_F(OccipitalLogicBridgeTest, GetInferencesSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_logic_bridge_update(bridge);
    occipital_logic_run_inference(bridge);

    logic_inference_result_t results[10];
    uint32_t count = 0;
    EXPECT_EQ(0, occipital_logic_get_inferences(bridge, results, 10, &count));
}

TEST_F(OccipitalLogicBridgeTest, GetInferencesNullBridgeFails) {
    logic_inference_result_t results[10];
    uint32_t count = 0;
    EXPECT_EQ(-1, occipital_logic_get_inferences(nullptr, results, 10, &count));
}

TEST_F(OccipitalLogicBridgeTest, ProveGoalSucceeds) {
    ASSERT_NE(nullptr, bridge);

    // Assert some predicates to work with
    visual_predicate_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.type = PRED_OBJECT_PRESENT;
    pred.confidence = 0.9f;
    pred.truth_value = 0.9f;
    pred.object_a = 1;
    occipital_logic_assert_predicate(bridge, &pred);

    // Try to prove a goal
    visual_predicate_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.type = PRED_OBJECT_PRESENT;
    goal.object_a = 1;

    bool provable = false;
    float confidence = 0.0f;
    EXPECT_EQ(0, occipital_logic_prove_goal(bridge, &goal, &provable, &confidence));
}

TEST_F(OccipitalLogicBridgeTest, ProveGoalNullBridgeFails) {
    visual_predicate_t goal;
    memset(&goal, 0, sizeof(goal));
    bool provable;
    float confidence;
    EXPECT_EQ(-1, occipital_logic_prove_goal(nullptr, &goal, &provable, &confidence));
}

// ============================================================================
// PROCESSING TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, UpdateSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_logic_bridge_update(bridge));
}

TEST_F(OccipitalLogicBridgeTest, UpdateNullBridgeFails) {
    EXPECT_EQ(-1, occipital_logic_bridge_update(nullptr));
}

TEST_F(OccipitalLogicBridgeTest, GetEffectsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_logic_bridge_update(bridge);

    occipital_logic_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    EXPECT_EQ(0, occipital_logic_bridge_get_effects(bridge, &effects));
}

TEST_F(OccipitalLogicBridgeTest, GetEffectsNullBridgeFails) {
    occipital_logic_effects_t effects;
    EXPECT_EQ(-1, occipital_logic_bridge_get_effects(nullptr, &effects));
}

TEST_F(OccipitalLogicBridgeTest, GetEffectsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_logic_bridge_get_effects(bridge, nullptr));
}

// ============================================================================
// CONNECTION TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, ConnectBrainNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_logic_connect_brain(bridge, nullptr));
}

TEST_F(OccipitalLogicBridgeTest, ConnectBrainNullBridgeFails) {
    EXPECT_EQ(-1, occipital_logic_connect_brain(nullptr, nullptr));
}

TEST_F(OccipitalLogicBridgeTest, ConnectNetworkNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_logic_connect_network(bridge, nullptr));
}

TEST_F(OccipitalLogicBridgeTest, ConnectNetworkNullBridgeFails) {
    EXPECT_EQ(-1, occipital_logic_connect_network(nullptr, nullptr));
}

TEST_F(OccipitalLogicBridgeTest, IsBrainConnectedInitiallyFalse) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_FALSE(occipital_logic_is_brain_connected(bridge));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, GetStatsInitiallyZero) {
    ASSERT_NE(nullptr, bridge);

    occipital_logic_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    EXPECT_EQ(0, occipital_logic_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(0ULL, stats.predicates_grounded);
    EXPECT_EQ(0ULL, stats.inferences_performed);
}

TEST_F(OccipitalLogicBridgeTest, GetStatsUpdatesAfterProcessing) {
    ASSERT_NE(nullptr, bridge);

    // Assert some predicates
    visual_predicate_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.type = PRED_OBJECT_PRESENT;
    pred.confidence = 0.9f;
    pred.truth_value = 0.9f;

    for (int i = 0; i < 5; i++) {
        pred.object_a = i;
        occipital_logic_assert_predicate(bridge, &pred);
    }

    occipital_logic_stats_t stats;
    EXPECT_EQ(0, occipital_logic_bridge_get_stats(bridge, &stats));
    EXPECT_GE(stats.predicates_asserted, 5ULL);
}

TEST_F(OccipitalLogicBridgeTest, GetStatsNullBridgeFails) {
    occipital_logic_stats_t stats;
    EXPECT_EQ(-1, occipital_logic_bridge_get_stats(nullptr, &stats));
}

TEST_F(OccipitalLogicBridgeTest, GetStatsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_logic_bridge_get_stats(bridge, nullptr));
}

TEST_F(OccipitalLogicBridgeTest, ResetStatsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    // Do some processing
    visual_predicate_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.type = PRED_OBJECT_PRESENT;
    pred.confidence = 0.9f;
    pred.truth_value = 0.9f;
    pred.object_a = 1;
    occipital_logic_assert_predicate(bridge, &pred);

    // Reset stats
    occipital_logic_bridge_reset_stats(bridge);

    occipital_logic_stats_t stats;
    EXPECT_EQ(0, occipital_logic_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0ULL, stats.predicates_asserted);
}

// ============================================================================
// BIO-ASYNC TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, RegisterBioAsyncNullRouterSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_logic_bridge_register_bio_async(bridge, nullptr));
}

TEST_F(OccipitalLogicBridgeTest, RegisterBioAsyncNullBridgeFails) {
    EXPECT_EQ(-1, occipital_logic_bridge_register_bio_async(nullptr, nullptr));
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(OccipitalLogicBridgeTest, RepeatedUpdatesDoNotLeak) {
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(0, occipital_logic_bridge_update(bridge));
    }

    SUCCEED();
}

TEST_F(OccipitalLogicBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        occipital_logic_bridge_t* temp = occipital_logic_bridge_create(
            occipital, &config);
        ASSERT_NE(nullptr, temp);
        occipital_logic_bridge_destroy(temp);
    }
    SUCCEED();
}

TEST_F(OccipitalLogicBridgeTest, AssertManyPredicatesSucceeds) {
    ASSERT_NE(nullptr, bridge);

    visual_predicate_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.type = PRED_OBJECT_PRESENT;
    pred.confidence = 0.9f;
    pred.truth_value = 0.9f;

    for (int i = 0; i < 100; i++) {
        pred.object_a = i;
        EXPECT_EQ(0, occipital_logic_assert_predicate(bridge, &pred));
    }

    visual_predicate_t predicates[150];
    uint32_t count = 0;
    EXPECT_EQ(0, occipital_logic_get_predicates(bridge, predicates, 150, &count));
    EXPECT_GE(count, 100u);
}
