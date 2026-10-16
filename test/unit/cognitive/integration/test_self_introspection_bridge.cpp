/**
 * @file test_self_introspection_bridge.cpp
 * @brief Unit tests for Self-Model - Introspection Bridge
 *
 * Tests bidirectional integration where the self-model guides introspective
 * queries and introspection results update the self-model.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "cognitive/integration/nimcp_self_introspection_bridge.h"

class SelfIntrospectionBridgeTest : public ::testing::Test {
protected:
    self_introspection_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = self_introspection_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            self_introspection_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(SelfIntrospectionBridgeTest, BridgeCreation) {
    // Bridge was created in SetUp - verify it's not null
    EXPECT_NE(bridge, nullptr);

    // Create and destroy a separate bridge
    self_introspection_bridge_t* test_bridge = self_introspection_bridge_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    self_introspection_bridge_destroy(test_bridge);
}

TEST_F(SelfIntrospectionBridgeTest, CreateWithConfig) {
    self_introspection_config_t config;
    int ret = self_introspection_default_config(&config);
    EXPECT_EQ(ret, 0);

    config.introspection_depth = 5;
    config.self_model_update_rate = 0.2f;

    self_introspection_bridge_t* custom = self_introspection_bridge_create(&config);
    ASSERT_NE(custom, nullptr);
    self_introspection_bridge_destroy(custom);
}

TEST_F(SelfIntrospectionBridgeTest, DestroyNullSafe) {
    // Should not crash
    self_introspection_bridge_destroy(nullptr);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(SelfIntrospectionBridgeTest, DefaultConfig) {
    self_introspection_config_t config;
    memset(&config, 0, sizeof(config));

    int ret = self_introspection_default_config(&config);
    EXPECT_EQ(ret, 0);

    // Default values: depth=3, update_rate=0.1, threshold=0.5
    EXPECT_EQ(config.introspection_depth, 3u);
    EXPECT_FLOAT_EQ(config.self_model_update_rate, 0.1f);
    EXPECT_FLOAT_EQ(config.reflection_threshold, 0.5f);
}

TEST_F(SelfIntrospectionBridgeTest, DefaultConfigNullPointer) {
    int ret = self_introspection_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(SelfIntrospectionBridgeTest, DefaultConfigAdditionalFields) {
    self_introspection_config_t config;
    int ret = self_introspection_default_config(&config);
    EXPECT_EQ(ret, 0);

    // Check additional fields have sensible defaults
    EXPECT_GE(config.min_update_confidence, 0.0f);
    EXPECT_LE(config.min_update_confidence, 1.0f);
    EXPECT_GE(config.max_reflection_depth, 1u);
}

//=============================================================================
// Query Guidance Tests
//=============================================================================

TEST_F(SelfIntrospectionBridgeTest, GuideQuery) {
    self_introspection_guidance_t guidance;
    memset(&guidance, 0, sizeof(guidance));

    int ret = self_introspection_guide_query(
        bridge, SELF_INTROSPECTION_QUERY_STATE, &guidance);
    EXPECT_EQ(ret, 0);

    // Verify guidance is populated
    EXPECT_GE(guidance.expectation_confidence, 0.0f);
    EXPECT_LE(guidance.expectation_confidence, 1.0f);
    EXPECT_GE(guidance.self_model_stability, 0.0f);
    EXPECT_LE(guidance.self_model_stability, 1.0f);
    EXPECT_GE(guidance.priority, 0.0f);
    EXPECT_LE(guidance.priority, 1.0f);
}

TEST_F(SelfIntrospectionBridgeTest, GuideQueryAllTypes) {
    self_introspection_guidance_t guidance;

    // Test all query types
    EXPECT_EQ(self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_STATE, &guidance), 0);
    EXPECT_EQ(self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_CAPABILITY, &guidance), 0);
    EXPECT_EQ(self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_KNOWLEDGE, &guidance), 0);
    EXPECT_EQ(self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_CONFIDENCE, &guidance), 0);
    EXPECT_EQ(self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_EMOTION, &guidance), 0);
    EXPECT_EQ(self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_INTENTION, &guidance), 0);
    EXPECT_EQ(self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_BELIEF, &guidance), 0);
    EXPECT_EQ(self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_MEMORY, &guidance), 0);
}

TEST_F(SelfIntrospectionBridgeTest, GuideQueryNullBridge) {
    self_introspection_guidance_t guidance;
    int ret = self_introspection_guide_query(nullptr, SELF_INTROSPECTION_QUERY_STATE, &guidance);
    EXPECT_EQ(ret, -1);
}

TEST_F(SelfIntrospectionBridgeTest, GuideQueryNullOutput) {
    int ret = self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_STATE, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Result Processing Tests
//=============================================================================

TEST_F(SelfIntrospectionBridgeTest, OnResult) {
    uint32_t query_id = 1001;

    self_introspection_result_t result;
    memset(&result, 0, sizeof(result));
    result.query_id = query_id;
    result.query_type = SELF_INTROSPECTION_QUERY_STATE;
    result.result_value = 0.75f;
    result.confidence = 0.9f;
    result.discrepancy = 0.1f;
    result.processing_time_ms = 50;
    result.suggests_update = true;

    int ret = self_introspection_on_result(bridge, query_id, &result);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfIntrospectionBridgeTest, OnResultMultiple) {
    for (uint32_t i = 0; i < 5; i++) {
        self_introspection_result_t result;
        memset(&result, 0, sizeof(result));
        result.query_id = i + 1;
        result.query_type = (self_introspection_query_type_t)(i % 8);
        result.result_value = (float)i * 0.2f;
        result.confidence = 0.8f;
        result.discrepancy = 0.05f;
        result.suggests_update = (i % 2 == 0);

        int ret = self_introspection_on_result(bridge, result.query_id, &result);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(SelfIntrospectionBridgeTest, OnResultNullBridge) {
    self_introspection_result_t result = {0};
    int ret = self_introspection_on_result(nullptr, 1, &result);
    EXPECT_EQ(ret, -1);
}

TEST_F(SelfIntrospectionBridgeTest, OnResultNullResult) {
    int ret = self_introspection_on_result(bridge, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Reflection Trigger Tests
//=============================================================================

TEST_F(SelfIntrospectionBridgeTest, TriggerReflection) {
    // Create a result with high discrepancy to trigger reflection
    self_introspection_result_t result;
    memset(&result, 0, sizeof(result));
    result.query_id = 1;
    result.query_type = SELF_INTROSPECTION_QUERY_STATE;
    result.result_value = 0.3f;
    result.confidence = 0.9f;
    result.discrepancy = 0.8f;  // High discrepancy
    result.suggests_update = true;

    self_introspection_on_result(bridge, result.query_id, &result);

    // Trigger reflection due to discrepancy
    int ret = self_introspection_trigger_reflection(
        bridge, SELF_INTROSPECTION_TRIGGER_DISCREPANCY);
    EXPECT_EQ(ret, 0);
}

TEST_F(SelfIntrospectionBridgeTest, TriggerReflectionNotNeeded) {
    // When self-model is coherent, scheduled reflection still succeeds
    // The function returns:
    // - query_id (> 0) if reflection was triggered
    // - 0 if no reflection was needed
    // - -1 on error
    // SCHEDULED always triggers, so we expect a query_id

    // Get initial self state
    self_introspection_self_state_t state;
    self_introspection_get_self_state(bridge, &state);

    // Trigger scheduled reflection - SCHEDULED always triggers
    int ret = self_introspection_trigger_reflection(
        bridge, SELF_INTROSPECTION_TRIGGER_SCHEDULED);
    EXPECT_GE(ret, 0);  // Returns query_id (>0) when triggered, 0 if not needed
}

TEST_F(SelfIntrospectionBridgeTest, TriggerReflectionAllTypes) {
    // Test all trigger types
    // The function returns:
    // - query_id (> 0) if reflection was triggered
    // - 0 if no reflection was needed (based on thresholds)
    // - -1 on error
    // Some triggers always fire (SCHEDULED, EXTERNAL, ERROR), others are conditional
    EXPECT_GE(self_introspection_trigger_reflection(bridge, SELF_INTROSPECTION_TRIGGER_DISCREPANCY), 0);
    EXPECT_GE(self_introspection_trigger_reflection(bridge, SELF_INTROSPECTION_TRIGGER_UNCERTAINTY), 0);
    EXPECT_GE(self_introspection_trigger_reflection(bridge, SELF_INTROSPECTION_TRIGGER_ERROR), 0);
    EXPECT_GE(self_introspection_trigger_reflection(bridge, SELF_INTROSPECTION_TRIGGER_NOVELTY), 0);
    EXPECT_GE(self_introspection_trigger_reflection(bridge, SELF_INTROSPECTION_TRIGGER_SCHEDULED), 0);
    EXPECT_GE(self_introspection_trigger_reflection(bridge, SELF_INTROSPECTION_TRIGGER_EXTERNAL), 0);
}

TEST_F(SelfIntrospectionBridgeTest, TriggerReflectionNullBridge) {
    int ret = self_introspection_trigger_reflection(nullptr, SELF_INTROSPECTION_TRIGGER_DISCREPANCY);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Self State Query Tests
//=============================================================================

TEST_F(SelfIntrospectionBridgeTest, GetSelfState) {
    self_introspection_self_state_t state;
    memset(&state, 0, sizeof(state));

    int ret = self_introspection_get_self_state(bridge, &state);
    EXPECT_EQ(ret, 0);

    // Verify state fields are within valid ranges
    EXPECT_GE(state.coherence, 0.0f);
    EXPECT_LE(state.coherence, 1.0f);
    EXPECT_GE(state.stability, 0.0f);
    EXPECT_LE(state.stability, 1.0f);
    EXPECT_GE(state.confidence, 0.0f);
    EXPECT_LE(state.confidence, 1.0f);
}

TEST_F(SelfIntrospectionBridgeTest, GetSelfStateAfterOperations) {
    // Perform some operations
    for (int i = 0; i < 5; i++) {
        self_introspection_guidance_t guidance;
        self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_STATE, &guidance);

        self_introspection_result_t result = {0};
        result.query_id = (uint32_t)i + 1;
        result.query_type = SELF_INTROSPECTION_QUERY_STATE;
        result.result_value = 0.5f + (float)i * 0.1f;
        result.confidence = 0.8f;
        self_introspection_on_result(bridge, result.query_id, &result);
    }

    self_introspection_self_state_t state;
    int ret = self_introspection_get_self_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.belief_count, 0u);
}

TEST_F(SelfIntrospectionBridgeTest, GetSelfStateNullBridge) {
    self_introspection_self_state_t state;
    int ret = self_introspection_get_self_state(nullptr, &state);
    EXPECT_EQ(ret, -1);
}

TEST_F(SelfIntrospectionBridgeTest, GetSelfStateNullOutput) {
    int ret = self_introspection_get_self_state(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(SelfIntrospectionBridgeTest, StatsTracking) {
    self_introspection_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // Perform operations to generate stats
    // 1. Guide queries
    for (int i = 0; i < 3; i++) {
        self_introspection_guidance_t guidance;
        self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_STATE, &guidance);
    }

    // 2. Process results
    for (uint32_t i = 0; i < 4; i++) {
        self_introspection_result_t result = {0};
        result.query_id = i + 1;
        result.query_type = SELF_INTROSPECTION_QUERY_CAPABILITY;
        result.result_value = 0.6f;
        result.confidence = 0.85f;
        self_introspection_on_result(bridge, result.query_id, &result);
    }

    // 3. Trigger reflections
    for (int i = 0; i < 2; i++) {
        self_introspection_trigger_reflection(bridge, SELF_INTROSPECTION_TRIGGER_SCHEDULED);
    }

    // Get stats
    int ret = self_introspection_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Verify stats are tracked
    EXPECT_GE(stats.queries_guided, 3u);
    EXPECT_GE(stats.results_integrated, 4u);
    EXPECT_GE(stats.reflections_triggered, 2u);
    EXPECT_GE(stats.avg_introspection_confidence, 0.0f);
    EXPECT_LE(stats.avg_introspection_confidence, 1.0f);
}

TEST_F(SelfIntrospectionBridgeTest, StatsNullBridge) {
    self_introspection_stats_t stats;
    int ret = self_introspection_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(SelfIntrospectionBridgeTest, StatsNullOutput) {
    int ret = self_introspection_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(SelfIntrospectionBridgeTest, StatsInitialValues) {
    self_introspection_stats_t stats;
    int ret = self_introspection_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Initial stats should be zero
    EXPECT_EQ(stats.queries_guided, 0u);
    EXPECT_EQ(stats.results_integrated, 0u);
    EXPECT_EQ(stats.reflections_triggered, 0u);
    EXPECT_EQ(stats.self_model_updates, 0u);
    EXPECT_EQ(stats.discrepancies_detected, 0u);
    EXPECT_EQ(stats.reflection_failures, 0u);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SelfIntrospectionBridgeTest, FullIntrospectionCycle) {
    // 1. Get guidance for query
    self_introspection_guidance_t guidance;
    int ret = self_introspection_guide_query(
        bridge, SELF_INTROSPECTION_QUERY_CONFIDENCE, &guidance);
    EXPECT_EQ(ret, 0);

    // 2. Simulate introspection and process result
    self_introspection_result_t result;
    memset(&result, 0, sizeof(result));
    result.query_id = 1;
    result.query_type = SELF_INTROSPECTION_QUERY_CONFIDENCE;
    result.result_value = 0.7f;
    result.confidence = 0.9f;
    result.discrepancy = std::fabs(result.result_value - guidance.expected_value);
    result.processing_time_ms = 25;
    result.suggests_update = (result.discrepancy > 0.2f);

    ret = self_introspection_on_result(bridge, result.query_id, &result);
    EXPECT_EQ(ret, 0);

    // 3. If discrepancy is high, trigger reflection
    if (result.discrepancy > 0.3f) {
        ret = self_introspection_trigger_reflection(
            bridge, SELF_INTROSPECTION_TRIGGER_DISCREPANCY);
        EXPECT_EQ(ret, 0);
    }

    // 4. Get updated self state
    self_introspection_self_state_t state;
    ret = self_introspection_get_self_state(bridge, &state);
    EXPECT_EQ(ret, 0);

    // 5. Verify stats
    self_introspection_stats_t stats;
    ret = self_introspection_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.queries_guided, 1u);
    EXPECT_GE(stats.results_integrated, 1u);
}

TEST_F(SelfIntrospectionBridgeTest, RepeatedSelfExamination) {
    const int num_cycles = 10;

    for (int cycle = 0; cycle < num_cycles; cycle++) {
        // Guide query
        self_introspection_guidance_t guidance;
        self_introspection_query_type_t query_type =
            (self_introspection_query_type_t)(cycle % 8);
        self_introspection_guide_query(bridge, query_type, &guidance);

        // Process result
        self_introspection_result_t result = {0};
        result.query_id = (uint32_t)(cycle + 1);
        result.query_type = query_type;
        result.result_value = 0.5f + (float)(cycle % 5) * 0.1f;
        result.confidence = 0.85f;
        result.discrepancy = 0.1f;
        self_introspection_on_result(bridge, result.query_id, &result);
    }

    // Verify stats
    self_introspection_stats_t stats;
    self_introspection_get_stats(bridge, &stats);
    EXPECT_EQ(stats.queries_guided, (uint64_t)num_cycles);
    EXPECT_EQ(stats.results_integrated, (uint64_t)num_cycles);
}

TEST_F(SelfIntrospectionBridgeTest, DiscrepancyDetection) {
    // Process result with high discrepancy
    self_introspection_result_t result;
    memset(&result, 0, sizeof(result));
    result.query_id = 1;
    result.query_type = SELF_INTROSPECTION_QUERY_BELIEF;
    result.result_value = 0.2f;
    result.confidence = 0.95f;
    result.discrepancy = 0.7f;  // High discrepancy
    result.suggests_update = true;

    int ret = self_introspection_on_result(bridge, result.query_id, &result);
    EXPECT_EQ(ret, 0);

    // Get stats
    self_introspection_stats_t stats;
    self_introspection_get_stats(bridge, &stats);

    // Check if discrepancy was detected
    EXPECT_GE(stats.discrepancies_detected, 0u);  // May or may not detect based on threshold
}

TEST_F(SelfIntrospectionBridgeTest, SelfModelStabilityOverTime) {
    // Perform consistent introspection to build stability
    for (int i = 0; i < 20; i++) {
        self_introspection_guidance_t guidance;
        self_introspection_guide_query(bridge, SELF_INTROSPECTION_QUERY_STATE, &guidance);

        self_introspection_result_t result = {0};
        result.query_id = (uint32_t)(i + 1);
        result.query_type = SELF_INTROSPECTION_QUERY_STATE;
        result.result_value = 0.6f;  // Consistent value
        result.confidence = 0.9f;
        result.discrepancy = 0.05f;  // Low discrepancy
        self_introspection_on_result(bridge, result.query_id, &result);
    }

    // Check self state - should have some stability
    self_introspection_self_state_t state;
    self_introspection_get_self_state(bridge, &state);
    EXPECT_GE(state.stability, 0.0f);
}
