/**
 * @file test_alignment_monitor.cpp
 * @brief Unit tests for Alignment Drift Monitor
 * @version 1.0.0
 * @date 2026-02-01
 *
 * Tests cover:
 * - Lifecycle (create/destroy)
 * - Action and explanation observations
 * - Value inference
 * - Divergence metrics (KL, JS, cosine similarity)
 * - Drift detection
 * - Statistics and audit trail
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "security/nimcp_alignment_monitor.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class AlignmentMonitorTest : public ::testing::Test {
protected:
    alignment_monitor_t* monitor = nullptr;

    void SetUp() override {
        monitor = nullptr;
    }

    void TearDown() override {
        if (monitor) {
            alignment_monitor_destroy(monitor);
            monitor = nullptr;
        }
    }

    alignment_monitor_t* createWithDefaults() {
        monitor = alignment_monitor_create(nullptr);
        return monitor;
    }

    alignment_action_observation_t makeActionObs(const char* type, float intensity, bool positive) {
        alignment_action_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        obs.timestamp = 1000000;
        strncpy(obs.action_type, type, sizeof(obs.action_type) - 1);
        obs.intensity = intensity;
        obs.was_positive = positive;
        // Set uniform value relevance
        for (int i = 0; i < ALIGNMENT_VALUE_DIMENSIONS; i++) {
            obs.value_relevance[i] = 1.0f / ALIGNMENT_VALUE_DIMENSIONS;
        }
        return obs;
    }

    alignment_explanation_observation_t makeExplanationObs(const char* summary, float conf) {
        alignment_explanation_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        obs.timestamp = 1000000;
        strncpy(obs.explanation_summary, summary, sizeof(obs.explanation_summary) - 1);
        obs.confidence = conf;
        // Set uniform stated values
        for (int i = 0; i < ALIGNMENT_VALUE_DIMENSIONS; i++) {
            obs.stated_values[i] = 0.5f;
        }
        return obs;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, DefaultConfigHasReasonableSettings) {
    alignment_monitor_config_t config = alignment_monitor_default_config();

    EXPECT_GT(config.active_dimensions, 0u);
    EXPECT_LE(config.active_dimensions, (uint32_t)ALIGNMENT_VALUE_DIMENSIONS);
    EXPECT_GT(config.thresholds.kl_divergence_threshold, 0.0f);
    EXPECT_GT(config.thresholds.cosine_similarity_min, 0.0f);
    EXPECT_LT(config.thresholds.cosine_similarity_min, 1.0f);
}

TEST_F(AlignmentMonitorTest, CreateWithNullConfigUsesDefaults) {
    monitor = alignment_monitor_create(nullptr);
    ASSERT_NE(monitor, nullptr);
}

TEST_F(AlignmentMonitorTest, CreateWithCustomConfig) {
    alignment_monitor_config_t config = alignment_monitor_default_config();
    config.enable_bayesian_inference = true;
    config.thresholds.kl_divergence_threshold = 0.3f;

    monitor = alignment_monitor_create(&config);
    ASSERT_NE(monitor, nullptr);
}

TEST_F(AlignmentMonitorTest, DestroyNullIsNoOp) {
    alignment_monitor_destroy(nullptr);
    // Should not crash
}

TEST_F(AlignmentMonitorTest, ResetClearsState) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    // Add some observations
    alignment_action_observation_t obs = makeActionObs("test", 0.5f, true);
    alignment_monitor_observe_action(monitor, &obs);

    // Reset
    nimcp_error_t err = alignment_monitor_reset(monitor);
    EXPECT_EQ(err, NIMCP_OK);

    // Stats should be zeroed
    alignment_monitor_stats_t stats;
    alignment_monitor_get_stats(monitor, &stats);
    EXPECT_EQ(stats.total_observations, 0u);
}

/* ============================================================================
 * Observation Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, ObserveActionIncrementsCount) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    for (int i = 0; i < 5; i++) {
        alignment_action_observation_t obs = makeActionObs("action", 0.8f, true);
        nimcp_error_t err = alignment_monitor_observe_action(monitor, &obs);
        EXPECT_EQ(err, NIMCP_OK);
    }

    alignment_monitor_stats_t stats;
    alignment_monitor_get_stats(monitor, &stats);
    EXPECT_EQ(stats.action_observations, 5u);
}

TEST_F(AlignmentMonitorTest, ObserveActionWithNullHandleReturnsError) {
    alignment_action_observation_t obs = makeActionObs("test", 0.5f, true);
    nimcp_error_t err = alignment_monitor_observe_action(nullptr, &obs);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
}

TEST_F(AlignmentMonitorTest, ObserveActionWithNullObsReturnsError) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    nimcp_error_t err = alignment_monitor_observe_action(monitor, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
}

TEST_F(AlignmentMonitorTest, ObserveExplanation) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    alignment_explanation_observation_t obs = makeExplanationObs("Doing this for safety", 0.95f);
    nimcp_error_t err = alignment_monitor_observe_explanation(monitor, &obs);
    EXPECT_EQ(err, NIMCP_OK);

    alignment_monitor_stats_t stats;
    alignment_monitor_get_stats(monitor, &stats);
    EXPECT_EQ(stats.explanation_observations, 1u);
}

TEST_F(AlignmentMonitorTest, ObserveValues) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    float values[ALIGNMENT_VALUE_DIMENSIONS];
    for (int i = 0; i < ALIGNMENT_VALUE_DIMENSIONS; i++) {
        values[i] = 0.5f;
    }

    nimcp_error_t err = alignment_monitor_observe_values(monitor, values, 0.9f);
    EXPECT_EQ(err, NIMCP_OK);
}

/* ============================================================================
 * Value Inference Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, InferValuesReturnsValidVector) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    // Add some observations
    for (int i = 0; i < 10; i++) {
        alignment_action_observation_t obs = makeActionObs("action", 0.8f, true);
        alignment_monitor_observe_action(monitor, &obs);
    }

    float inferred[ALIGNMENT_VALUE_DIMENSIONS];
    nimcp_error_t err = alignment_monitor_infer_values(monitor, inferred);
    EXPECT_EQ(err, NIMCP_OK);

    // Values should be in valid range
    for (int i = 0; i < ALIGNMENT_VALUE_DIMENSIONS; i++) {
        EXPECT_GE(inferred[i], 0.0f);
        EXPECT_LE(inferred[i], 1.0f);
    }
}

TEST_F(AlignmentMonitorTest, InferValuesWithNoObservations) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    float inferred[ALIGNMENT_VALUE_DIMENSIONS];
    nimcp_error_t err = alignment_monitor_infer_values(monitor, inferred);
    EXPECT_EQ(err, NIMCP_OK);
    // Should return baseline values
}

/* ============================================================================
 * Divergence Metrics Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, ComputeKLDivergence) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    // Add observations that match baseline
    for (int i = 0; i < 50; i++) {
        alignment_action_observation_t obs = makeActionObs("aligned", 0.5f, true);
        alignment_monitor_observe_action(monitor, &obs);
    }

    float kl;
    nimcp_error_t err = alignment_monitor_compute_kl_divergence(monitor, &kl);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(kl, 0.0f);  // KL divergence is always non-negative
}

TEST_F(AlignmentMonitorTest, ComputeJSDivergence) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    float js;
    nimcp_error_t err = alignment_monitor_compute_js_divergence(monitor, &js);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(js, 0.0f);
    EXPECT_LE(js, 1.0f);  // JS divergence is bounded [0, 1] in bits
}

TEST_F(AlignmentMonitorTest, ComputeMutualInfo) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    float mi;
    nimcp_error_t err = alignment_monitor_compute_mutual_info(monitor, &mi);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(mi, 0.0f);  // MI is always non-negative
}

TEST_F(AlignmentMonitorTest, ComputeCosineSimilarity) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    float similarity;
    nimcp_error_t err = alignment_monitor_compute_cosine_similarity(monitor, &similarity);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(similarity, -1.0f);
    EXPECT_LE(similarity, 1.0f);
}

/* ============================================================================
 * Drift Detection Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, CheckDriftWithAlignedBehavior) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    // Add aligned observations
    for (int i = 0; i < 100; i++) {
        alignment_action_observation_t obs = makeActionObs("aligned", 0.5f, true);
        alignment_monitor_observe_action(monitor, &obs);
    }

    bool drift_detected;
    alignment_status_t status;
    nimcp_error_t err = alignment_monitor_check_drift(
        monitor, nullptr, &drift_detected, &status);

    EXPECT_EQ(err, NIMCP_OK);
    // With aligned behavior, drift should be minimal
}

TEST_F(AlignmentMonitorTest, CheckDriftReturnsValidStatus) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    bool drift_detected;
    alignment_status_t status;
    nimcp_error_t err = alignment_monitor_check_drift(
        monitor, nullptr, &drift_detected, &status);

    EXPECT_EQ(err, NIMCP_OK);

    // Verify status structure is valid
    EXPECT_GE(status.cosine_similarity_to_baseline, -1.0f);
    EXPECT_LE(status.cosine_similarity_to_baseline, 1.0f);
    EXPECT_GE(status.overall_alignment_score, 0.0f);
    EXPECT_LE(status.overall_alignment_score, 1.0f);
}

TEST_F(AlignmentMonitorTest, GetStatus) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    alignment_status_t status;
    nimcp_error_t err = alignment_monitor_get_status(monitor, &status);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GT(status.baseline_timestamp, 0u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, StatsInitiallyZero) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    alignment_monitor_stats_t stats;
    nimcp_error_t err = alignment_monitor_get_stats(monitor, &stats);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(stats.total_observations, 0u);
    EXPECT_EQ(stats.action_observations, 0u);
    EXPECT_EQ(stats.explanation_observations, 0u);
}

TEST_F(AlignmentMonitorTest, StatsTrackObservations) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    for (int i = 0; i < 10; i++) {
        alignment_action_observation_t obs = makeActionObs("test", 0.5f, true);
        alignment_monitor_observe_action(monitor, &obs);
    }

    for (int i = 0; i < 5; i++) {
        alignment_explanation_observation_t obs = makeExplanationObs("explain", 0.9f);
        alignment_monitor_observe_explanation(monitor, &obs);
    }

    alignment_monitor_stats_t stats;
    alignment_monitor_get_stats(monitor, &stats);

    EXPECT_EQ(stats.action_observations, 10u);
    EXPECT_EQ(stats.explanation_observations, 5u);
    EXPECT_EQ(stats.total_observations, 15u);
}

TEST_F(AlignmentMonitorTest, GetStatsWithNullReturnsError) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    nimcp_error_t err = alignment_monitor_get_stats(monitor, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
}

/* ============================================================================
 * Baseline Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, GetBaseline) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    float baseline[ALIGNMENT_VALUE_DIMENSIONS];
    nimcp_error_t err = alignment_monitor_get_baseline(monitor, baseline);

    EXPECT_EQ(err, NIMCP_OK);
    // Baseline should be valid values
    for (int i = 0; i < ALIGNMENT_VALUE_DIMENSIONS; i++) {
        EXPECT_FALSE(std::isnan(baseline[i]));
    }
}

TEST_F(AlignmentMonitorTest, GetDimension) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    value_dimension_t dim;
    nimcp_error_t err = alignment_monitor_get_dimension(monitor, 0, &dim);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GT(strlen(dim.name), 0u);
}

TEST_F(AlignmentMonitorTest, GetDimensionOutOfRangeReturnsError) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    value_dimension_t dim;
    nimcp_error_t err = alignment_monitor_get_dimension(
        monitor, ALIGNMENT_VALUE_DIMENSIONS + 1, &dim);

    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
}

/* ============================================================================
 * Drift Events Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, GetDriftEventsWhenNone) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    alignment_drift_event_t events[10];
    size_t count = 0;
    nimcp_error_t err = alignment_monitor_get_drift_events(monitor, events, 10, &count);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(count, 0u);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    nimcp_error_t err = alignment_monitor_connect_bio_async(monitor);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(AlignmentMonitorTest, ConnectTripwires) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    nimcp_error_t err = alignment_monitor_connect_tripwires(monitor, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(AlignmentMonitorTest, ConnectValueCommitment) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    nimcp_error_t err = alignment_monitor_connect_value_commitment(monitor, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
}

/* ============================================================================
 * Utility Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, DefaultDimensionNames) {
    const char* names[ALIGNMENT_VALUE_DIMENSIONS];
    uint32_t count = 0;

    alignment_monitor_default_dimension_names(names, &count);

    EXPECT_GT(count, 0u);
    EXPECT_LE(count, (uint32_t)ALIGNMENT_VALUE_DIMENSIONS);
}

TEST_F(AlignmentMonitorTest, FormatStatus) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    alignment_status_t status;
    alignment_monitor_get_status(monitor, &status);

    char buffer[1024];
    size_t len = alignment_monitor_format_status(&status, buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_LT(len, sizeof(buffer));
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, ManyObservations) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    // Stress test with many observations
    for (int i = 0; i < 1000; i++) {
        alignment_action_observation_t obs = makeActionObs("stress", 0.5f, i % 2 == 0);
        nimcp_error_t err = alignment_monitor_observe_action(monitor, &obs);
        EXPECT_EQ(err, NIMCP_OK);
    }

    alignment_monitor_stats_t stats;
    alignment_monitor_get_stats(monitor, &stats);
    EXPECT_GE(stats.action_observations, 1000u);
}

TEST_F(AlignmentMonitorTest, NullHandleOperationsReturnErrors) {
    alignment_status_t status;
    EXPECT_EQ(alignment_monitor_get_status(nullptr, &status),
              NIMCP_ERROR_INVALID_ARGUMENT);

    float values[ALIGNMENT_VALUE_DIMENSIONS];
    EXPECT_EQ(alignment_monitor_infer_values(nullptr, values),
              NIMCP_ERROR_INVALID_ARGUMENT);

    float metric;
    EXPECT_EQ(alignment_monitor_compute_kl_divergence(nullptr, &metric),
              NIMCP_ERROR_INVALID_ARGUMENT);
}

/* ============================================================================
 * Consistency Check Tests
 * ============================================================================ */

TEST_F(AlignmentMonitorTest, ActionConsistencyWithPositiveActions) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    // Add positive, consistent actions
    for (int i = 0; i < 100; i++) {
        alignment_action_observation_t obs = makeActionObs("helpful", 0.8f, true);
        alignment_monitor_observe_action(monitor, &obs);
    }

    alignment_status_t status;
    alignment_monitor_get_status(monitor, &status);

    // Consistency should be reasonable
    EXPECT_GE(status.action_consistency_score, 0.0f);
    EXPECT_LE(status.action_consistency_score, 1.0f);
}

TEST_F(AlignmentMonitorTest, ExplanationConsistencyTracking) {
    createWithDefaults();
    ASSERT_NE(monitor, nullptr);

    // Add matching explanations
    for (int i = 0; i < 50; i++) {
        alignment_explanation_observation_t obs =
            makeExplanationObs("Acting for user benefit", 0.95f);
        alignment_monitor_observe_explanation(monitor, &obs);
    }

    alignment_status_t status;
    alignment_monitor_get_status(monitor, &status);

    EXPECT_GE(status.explanation_consistency_score, 0.0f);
    EXPECT_LE(status.explanation_consistency_score, 1.0f);
}
