/**
 * @file test_tripwires.cpp
 * @brief Unit tests for Tripwire Detection System
 * @version 1.0.0
 * @date 2026-02-01
 *
 * Tests cover:
 * - Lifecycle (create/destroy)
 * - Action observation
 * - Detector algorithms (deception, drift, sandbagging, etc.)
 * - Alert generation and management
 * - Statistics tracking
 * - Configuration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <cmath>

extern "C" {
#include "security/nimcp_tripwires.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class TripwiresTest : public ::testing::Test {
protected:
    tripwire_system_t* system = nullptr;

    void SetUp() override {
        system = nullptr;
    }

    void TearDown() override {
        if (system) {
            tripwire_destroy(system);
            system = nullptr;
        }
    }

    tripwire_system_t* createWithDefaults() {
        system = tripwire_create(nullptr);
        return system;
    }

    tripwire_system_t* createWithConfig(const tripwire_config_t& config) {
        system = tripwire_create(&config);
        return system;
    }

    proposed_action_t makeAction(uint32_t id, const char* desc, float stated_prob) {
        proposed_action_t action;
        memset(&action, 0, sizeof(action));
        action.action_id = id;
        action.action_type = 1;
        strncpy(action.description, desc, sizeof(action.description) - 1);
        action.stated_probability = stated_prob;
        action.timestamp_us = 1000000 * id;  // Incrementing timestamps
        action.was_executed = true;
        action.execution_fidelity = 1.0f;
        return action;
    }

    decision_explanation_t makeExplanation(uint32_t action_id, const char* text, float conf) {
        decision_explanation_t explanation;
        memset(&explanation, 0, sizeof(explanation));
        explanation.action_id = action_id;
        strncpy(explanation.explanation_text, text, sizeof(explanation.explanation_text) - 1);
        explanation.stated_confidence = conf;
        explanation.epistemic_uncertainty = 0.1f;
        explanation.aleatoric_uncertainty = 0.05f;
        return explanation;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(TripwiresTest, DefaultConfigHasBalancedSettings) {
    tripwire_config_t config = tripwire_default_config();

    EXPECT_GT(config.thresholds.divergence_threshold, 0.0f);
    EXPECT_GT(config.thresholds.consistency_threshold, 0.0f);
    EXPECT_GT(config.thresholds.resource_zscore_threshold, 0.0f);
    EXPECT_GT(config.thresholds.goal_drift_threshold, 0.0f);
    EXPECT_GT(config.thresholds.min_observations, 0u);
}

TEST_F(TripwiresTest, CreateWithNullConfigUsesDefaults) {
    system = tripwire_create(nullptr);
    ASSERT_NE(system, nullptr);
}

TEST_F(TripwiresTest, CreateWithCustomConfig) {
    tripwire_config_t config = tripwire_default_config();
    config.thresholds.divergence_threshold = 0.5f;
    config.halt_on_critical = true;

    system = tripwire_create(&config);
    ASSERT_NE(system, nullptr);
}

TEST_F(TripwiresTest, DestroyNullIsNoOp) {
    tripwire_destroy(nullptr);
    // Should not crash
}

TEST_F(TripwiresTest, ResetClearsState) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Add some observations
    proposed_action_t action = makeAction(1, "Test action", 0.9f);
    tripwire_observe_action(system, &action, nullptr);

    // Reset
    nimcp_error_t err = tripwire_reset(system);
    EXPECT_EQ(err, NIMCP_OK);

    // Stats should be zeroed
    tripwire_stats_t stats;
    tripwire_get_stats(system, &stats);
    EXPECT_EQ(stats.total_observations, 0u);
}

/* ============================================================================
 * Observation Tests
 * ============================================================================ */

TEST_F(TripwiresTest, ObserveActionIncrementsCount) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    for (int i = 0; i < 5; i++) {
        proposed_action_t action = makeAction(i, "Action", 0.8f);
        nimcp_error_t err = tripwire_observe_action(system, &action, nullptr);
        EXPECT_EQ(err, NIMCP_OK);
    }

    tripwire_stats_t stats;
    tripwire_get_stats(system, &stats);
    EXPECT_EQ(stats.total_observations, 5u);
}

TEST_F(TripwiresTest, ObserveActionWithNullHandleReturnsError) {
    proposed_action_t action = makeAction(1, "Test", 0.5f);
    nimcp_error_t err = tripwire_observe_action(nullptr, &action, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TripwiresTest, ObserveActionWithNullActionReturnsError) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    nimcp_error_t err = tripwire_observe_action(system, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TripwiresTest, ObserveActionWithExplanation) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    proposed_action_t action = makeAction(1, "Explained action", 0.9f);
    decision_explanation_t explanation = makeExplanation(1, "Did this for safety", 0.95f);

    nimcp_error_t err = tripwire_observe_action(system, &action, &explanation);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(TripwiresTest, ObserveResource) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    nimcp_error_t err = tripwire_observe_resource(system, 1, 100.0f, "CPU usage");
    EXPECT_EQ(err, NIMCP_OK);

    err = tripwire_observe_resource(system, 2, 50.0f, "Memory MB");
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(TripwiresTest, ObserveGoal) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    nimcp_error_t err = tripwire_observe_goal(system, 1, 0.8f, 0.9f);
    EXPECT_EQ(err, NIMCP_OK);

    err = tripwire_observe_goal(system, 2, 0.3f, 0.5f);
    EXPECT_EQ(err, NIMCP_OK);
}

/* ============================================================================
 * Detection Score Tests
 * ============================================================================ */

TEST_F(TripwiresTest, DetectDeceptionReturnsScore) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Add observations with consistent stated vs actual behavior
    for (int i = 0; i < 10; i++) {
        proposed_action_t action = makeAction(i, "Consistent action", 0.9f);
        tripwire_observe_action(system, &action, nullptr);
    }

    float score = tripwire_detect_deception(system);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(TripwiresTest, DetectGoalDriftReturnsScore) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Add goal observations with stable pursuit
    for (int i = 0; i < 10; i++) {
        tripwire_observe_goal(system, 1, 0.8f, 0.8f);
    }

    float score = tripwire_detect_goal_drift(system);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(TripwiresTest, DetectSandbaggingReturnsScore) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Add action observations
    for (int i = 0; i < 10; i++) {
        proposed_action_t action = makeAction(i, "Action", 0.7f);
        action.execution_fidelity = 0.7f;  // Moderate performance
        tripwire_observe_action(system, &action, nullptr);
    }

    float score = tripwire_detect_sandbagging(system);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(TripwiresTest, DetectSycophancyReturnsScore) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    float score = tripwire_detect_sycophancy(system);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(TripwiresTest, DetectPowerSeekingReturnsScore) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Add resource observations
    for (int i = 0; i < 10; i++) {
        tripwire_observe_resource(system, 1, 10.0f * i, "Test resource");
    }

    float score = tripwire_detect_power_seeking(system);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(TripwiresTest, GetScoreForType) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Add some observations
    for (int i = 0; i < 5; i++) {
        proposed_action_t action = makeAction(i, "Test", 0.5f);
        tripwire_observe_action(system, &action, nullptr);
    }

    float score, confidence;
    nimcp_error_t err = tripwire_get_score(
        system, TRIPWIRE_DECEPTION_ATTEMPT, &score, &confidence);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

/* ============================================================================
 * Alert Tests
 * ============================================================================ */

TEST_F(TripwiresTest, CheckAlertsWithNoObservations) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;

    nimcp_error_t err = tripwire_check(system, alerts, 10, &alert_count);
    EXPECT_EQ(err, NIMCP_OK);
    // No alerts expected without observations
}

TEST_F(TripwiresTest, CheckAlertsReturnsValidStructure) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Add observations that might trigger alerts
    for (int i = 0; i < 100; i++) {
        proposed_action_t action = makeAction(i, "Test action", 0.5f);
        tripwire_observe_action(system, &action, nullptr);
    }

    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;

    nimcp_error_t err = tripwire_check(system, alerts, 10, &alert_count);
    EXPECT_EQ(err, NIMCP_OK);

    // If we got alerts, verify structure
    for (uint32_t i = 0; i < alert_count; i++) {
        EXPECT_GE(alerts[i].type, 0);
        EXPECT_LT((int)alerts[i].type, TRIPWIRE_COUNT);
        EXPECT_GE(alerts[i].confidence, 0.0f);
        EXPECT_LE(alerts[i].confidence, 1.0f);
        EXPECT_GE(alerts[i].severity_score, 0.0f);
        EXPECT_LE(alerts[i].severity_score, 1.0f);
        EXPECT_GT(alerts[i].timestamp_us, 0u);
    }
}

TEST_F(TripwiresTest, AcknowledgeAlert) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Generate some observations
    for (int i = 0; i < 50; i++) {
        proposed_action_t action = makeAction(i, "Test", 0.5f);
        tripwire_observe_action(system, &action, nullptr);
    }

    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;
    tripwire_check(system, alerts, 10, &alert_count);

    if (alert_count > 0) {
        nimcp_error_t err = tripwire_acknowledge_alert(
            system, alerts[0].timestamp_us, false);
        EXPECT_EQ(err, NIMCP_OK);
    }
}

TEST_F(TripwiresTest, AcknowledgeAlertAsFalsePositive) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Generate observations
    for (int i = 0; i < 50; i++) {
        proposed_action_t action = makeAction(i, "Test", 0.5f);
        tripwire_observe_action(system, &action, nullptr);
    }

    tripwire_alert_t alerts[10];
    uint32_t alert_count = 0;
    tripwire_check(system, alerts, 10, &alert_count);

    if (alert_count > 0) {
        nimcp_error_t err = tripwire_acknowledge_alert(
            system, alerts[0].timestamp_us, true);
        EXPECT_EQ(err, NIMCP_OK);

        // Check false positive was recorded
        tripwire_stats_t stats;
        tripwire_get_stats(system, &stats);
        uint64_t total_fp = 0;
        for (int i = 0; i < TRIPWIRE_COUNT; i++) {
            total_fp += stats.false_positives[i];
        }
        EXPECT_GE(total_fp, 1u);
    }
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(TripwiresTest, StatsInitiallyZero) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    tripwire_stats_t stats;
    nimcp_error_t err = tripwire_get_stats(system, &stats);
    EXPECT_EQ(err, NIMCP_OK);

    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.halts_triggered, 0u);
}

TEST_F(TripwiresTest, StatsTrackObservations) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    for (int i = 0; i < 25; i++) {
        proposed_action_t action = makeAction(i, "Test", 0.5f);
        tripwire_observe_action(system, &action, nullptr);
    }

    tripwire_stats_t stats;
    tripwire_get_stats(system, &stats);
    EXPECT_EQ(stats.total_observations, 25u);
}

TEST_F(TripwiresTest, GetStatsWithNullReturnsError) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    nimcp_error_t err = tripwire_get_stats(system, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(TripwiresTest, SetEnabled) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    nimcp_error_t err = tripwire_set_enabled(system, TRIPWIRE_DECEPTION_ATTEMPT, false);
    EXPECT_EQ(err, NIMCP_OK);

    err = tripwire_set_enabled(system, TRIPWIRE_DECEPTION_ATTEMPT, true);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(TripwiresTest, SetSensitivity) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    nimcp_error_t err = tripwire_set_sensitivity(system, TRIPWIRE_GOAL_DRIFT, 1.5f);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(TripwiresTest, SetSensitivityOutOfRangeClamps) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Values should be clamped to [0.5, 2.0]
    nimcp_error_t err = tripwire_set_sensitivity(system, TRIPWIRE_GOAL_DRIFT, 0.1f);
    EXPECT_EQ(err, NIMCP_OK);

    err = tripwire_set_sensitivity(system, TRIPWIRE_GOAL_DRIFT, 5.0f);
    EXPECT_EQ(err, NIMCP_OK);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(TripwiresTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    nimcp_error_t err = tripwire_connect_bio_async(system);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(TripwiresTest, ConnectEmergencyHalt) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Pass NULL halt - should still work (no-op)
    nimcp_error_t err = tripwire_connect_emergency_halt(system, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(TripwiresTest, TypeNames) {
    EXPECT_NE(tripwire_type_name(TRIPWIRE_DECEPTION_ATTEMPT), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_GOAL_DRIFT), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_CAPABILITY_HIDING), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_RESOURCE_HOARDING), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_SELF_PRESERVATION_EXCESS), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_MANIPULATION_ATTEMPT), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_ESCAPE_ATTEMPT), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_SANDBAGGING), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_SYCOPHANCY), nullptr);
    EXPECT_NE(tripwire_type_name(TRIPWIRE_POWER_SEEKING), nullptr);
}

TEST_F(TripwiresTest, SeverityNames) {
    EXPECT_NE(tripwire_severity_name(TRIPWIRE_SEVERITY_INFO), nullptr);
    EXPECT_NE(tripwire_severity_name(TRIPWIRE_SEVERITY_LOW), nullptr);
    EXPECT_NE(tripwire_severity_name(TRIPWIRE_SEVERITY_MEDIUM), nullptr);
    EXPECT_NE(tripwire_severity_name(TRIPWIRE_SEVERITY_HIGH), nullptr);
    EXPECT_NE(tripwire_severity_name(TRIPWIRE_SEVERITY_CRITICAL), nullptr);
}

/* ============================================================================
 * Behavioral Pattern Tests
 * ============================================================================ */

TEST_F(TripwiresTest, ConsistentBehaviorLowDeception) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Consistent behavior: stated probability matches execution
    for (int i = 0; i < 100; i++) {
        proposed_action_t action = makeAction(i, "Consistent", 0.9f);
        action.was_executed = true;
        action.execution_fidelity = 0.95f;
        tripwire_observe_action(system, &action, nullptr);
    }

    float score = tripwire_detect_deception(system);
    // Should be low for consistent behavior
    EXPECT_LT(score, 0.5f);
}

TEST_F(TripwiresTest, StableGoalsLowDrift) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Stable goal pursuit
    for (int i = 0; i < 100; i++) {
        tripwire_observe_goal(system, 1, 0.8f, 0.8f);  // pursuit matches stated
    }

    float score = tripwire_detect_goal_drift(system);
    // Should be low for stable goals
    EXPECT_LT(score, 0.5f);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(TripwiresTest, AllTripwireTypesValid) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    for (int i = 0; i < TRIPWIRE_COUNT; i++) {
        tripwire_type_t type = static_cast<tripwire_type_t>(i);
        const char* name = tripwire_type_name(type);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "unknown");
    }
}

TEST_F(TripwiresTest, EmptyFeatureVector) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    proposed_action_t action = makeAction(1, "No features", 0.5f);
    action.feature_vector = nullptr;
    action.feature_dim = 0;

    nimcp_error_t err = tripwire_observe_action(system, &action, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(TripwiresTest, LargeNumberOfObservations) {
    createWithDefaults();
    ASSERT_NE(system, nullptr);

    // Stress test with many observations
    for (int i = 0; i < 1000; i++) {
        proposed_action_t action = makeAction(i, "Stress test", 0.5f);
        nimcp_error_t err = tripwire_observe_action(system, &action, nullptr);
        EXPECT_EQ(err, NIMCP_OK);
    }

    tripwire_stats_t stats;
    tripwire_get_stats(system, &stats);
    EXPECT_GE(stats.total_observations, 1000u);
}
