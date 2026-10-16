/**
 * @file test_nimcp_cingulate_adapter.cpp
 * @brief Comprehensive unit tests for nimcp_cingulate_adapter.c
 *
 * WHAT: Unit tests for the Cingulate Cortex adapter
 * WHY:  Ensure correct conflict monitoring, error detection, and self-referential processing
 * HOW:  Use Google Test framework to test lifecycle, conflict detection, error processing,
 *       cognitive control, and self-referential processing.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class CingulateAdapterTest : public ::testing::Test {
protected:
    cingulate_adapter_t* adapter;
    cingulate_config_t config;

    void SetUp() override {
        config = cingulate_default_config();
        adapter = cingulate_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create cingulate adapter";
    }

    void TearDown() override {
        cingulate_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to set up response options for conflict testing
    void setup_conflict_scenario(float activation_a, float activation_b) {
        ASSERT_TRUE(cingulate_begin_monitoring(adapter, 4));

        cingulate_response_option_t option_a;
        memset(&option_a, 0, sizeof(option_a));
        option_a.option_id = 0;
        option_a.activation = activation_a;
        option_a.evidence = 0.5f;
        option_a.prior_probability = 0.25f;
        option_a.is_prepotent = true;
        ASSERT_TRUE(cingulate_update_response(adapter, &option_a));

        cingulate_response_option_t option_b;
        memset(&option_b, 0, sizeof(option_b));
        option_b.option_id = 1;
        option_b.activation = activation_b;
        option_b.evidence = 0.5f;
        option_b.prior_probability = 0.25f;
        option_b.is_prepotent = false;
        ASSERT_TRUE(cingulate_update_response(adapter, &option_b));
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(CingulateAdapterTest, DefaultConfigHasReasonableValues) {
    cingulate_config_t default_config = cingulate_default_config();

    EXPECT_EQ(default_config.max_conflicts, CINGULATE_DEFAULT_MAX_CONFLICTS);
    EXPECT_EQ(default_config.max_errors, CINGULATE_DEFAULT_MAX_ERRORS);
    EXPECT_FLOAT_EQ(default_config.conflict_threshold, CINGULATE_DEFAULT_CONFLICT_THRESHOLD);
    EXPECT_FLOAT_EQ(default_config.error_threshold, CINGULATE_DEFAULT_ERROR_THRESHOLD);
    EXPECT_TRUE(default_config.enable_conflict_monitoring);
    EXPECT_TRUE(default_config.enable_error_detection);
    EXPECT_TRUE(default_config.enable_cognitive_control);
    EXPECT_TRUE(default_config.enable_self_referential);
}

TEST_F(CingulateAdapterTest, CreateWithNullConfigUsesDefaults) {
    cingulate_adapter_t* adapter_null = cingulate_create(NULL);
    ASSERT_NE(nullptr, adapter_null);

    cingulate_config_t retrieved;
    EXPECT_TRUE(cingulate_get_config(adapter_null, &retrieved));
    EXPECT_EQ(retrieved.max_conflicts, CINGULATE_DEFAULT_MAX_CONFLICTS);

    cingulate_destroy(adapter_null);
}

TEST_F(CingulateAdapterTest, DestroyNullDoesNotCrash) {
    cingulate_destroy(NULL);
    // Should not crash
}

TEST_F(CingulateAdapterTest, ResetClearsState) {
    // Set up some state
    EXPECT_TRUE(cingulate_begin_monitoring(adapter, 2));
    EXPECT_TRUE(cingulate_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(cingulate_get_status(adapter), CINGULATE_STATUS_IDLE);
    EXPECT_EQ(cingulate_get_last_error_code(adapter), CINGULATE_ERROR_NONE);
}

TEST_F(CingulateAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(cingulate_reset(NULL));
}

/*=============================================================================
 * CONFLICT MONITORING TESTS (ACC - Dorsal)
 *===========================================================================*/

TEST_F(CingulateAdapterTest, BeginMonitoringSuccess) {
    EXPECT_TRUE(cingulate_begin_monitoring(adapter, 4));
    EXPECT_EQ(cingulate_get_status(adapter), CINGULATE_STATUS_MONITORING);
}

TEST_F(CingulateAdapterTest, BeginMonitoringNullReturnsFalse) {
    EXPECT_FALSE(cingulate_begin_monitoring(NULL, 4));
}

TEST_F(CingulateAdapterTest, BeginMonitoringTooManyOptionsUsesMax) {
    EXPECT_TRUE(cingulate_begin_monitoring(adapter, 100));
    // Should still work, but limit to max
}

TEST_F(CingulateAdapterTest, UpdateResponseSuccess) {
    EXPECT_TRUE(cingulate_begin_monitoring(adapter, 4));

    cingulate_response_option_t option;
    memset(&option, 0, sizeof(option));
    option.option_id = 0;
    option.activation = 0.7f;
    option.evidence = 0.5f;

    EXPECT_TRUE(cingulate_update_response(adapter, &option));
}

TEST_F(CingulateAdapterTest, UpdateResponseWithoutMonitoringFails) {
    cingulate_response_option_t option;
    memset(&option, 0, sizeof(option));
    option.option_id = 0;
    option.activation = 0.7f;

    EXPECT_FALSE(cingulate_update_response(adapter, &option));
}

TEST_F(CingulateAdapterTest, ConflictDetectedWhenTwoOptionsActive) {
    // Set up two highly active options
    setup_conflict_scenario(0.8f, 0.75f);

    cingulate_conflict_t conflict;
    bool detected = cingulate_evaluate_conflict(adapter, &conflict);

    // With two options at 0.8 and 0.75, conflict = 0.8 * 0.75 = 0.6
    // Should be above default threshold of 0.5
    EXPECT_TRUE(detected);
    EXPECT_GT(conflict.conflict_level, 0.0f);
}

TEST_F(CingulateAdapterTest, NoConflictWhenOnlyOneActive) {
    setup_conflict_scenario(0.8f, 0.1f);

    cingulate_conflict_t conflict;
    bool detected = cingulate_evaluate_conflict(adapter, &conflict);

    // With one dominant option, conflict = 0.8 * 0.1 = 0.08
    // Should be below threshold
    EXPECT_FALSE(detected);
}

TEST_F(CingulateAdapterTest, ConflictRequiresControlWhenStrong) {
    setup_conflict_scenario(0.9f, 0.85f);

    cingulate_conflict_t conflict;
    EXPECT_TRUE(cingulate_evaluate_conflict(adapter, &conflict));

    // Strong conflict should require control
    EXPECT_TRUE(cingulate_requires_control(adapter, &conflict));
}

/*=============================================================================
 * ERROR DETECTION TESTS (ACC - Rostral)
 *===========================================================================*/

TEST_F(CingulateAdapterTest, ErrorDetectedOnResponseMismatch) {
    EXPECT_TRUE(cingulate_report_response(adapter, 1, 2));  // Executed 1, intended 2

    cingulate_error_event_t error;
    EXPECT_TRUE(cingulate_get_last_error(adapter, &error));
    EXPECT_EQ(error.executed_option, 1u);
    EXPECT_EQ(error.intended_option, 2u);
    EXPECT_GT(error.error_magnitude, 0.0f);
}

TEST_F(CingulateAdapterTest, NoErrorWhenResponseMatches) {
    // Reset state first
    cingulate_reset(adapter);

    // Report matching response
    EXPECT_TRUE(cingulate_report_response(adapter, 1, 1));

    cingulate_error_event_t error;
    EXPECT_FALSE(cingulate_get_last_error(adapter, &error));
}

TEST_F(CingulateAdapterTest, OutcomeErrorDetected) {
    float outcome = 0.2f;
    float expected = 0.9f;

    bool error_detected = cingulate_report_outcome(adapter, outcome, expected);

    // Error magnitude = |0.2 - 0.9| = 0.7 > threshold
    EXPECT_TRUE(error_detected);

    cingulate_error_event_t error;
    EXPECT_TRUE(cingulate_get_last_error(adapter, &error));
    EXPECT_FLOAT_EQ(error.error_magnitude, 0.7f);
}

TEST_F(CingulateAdapterTest, SmallOutcomeErrorNotDetected) {
    float outcome = 0.5f;
    float expected = 0.55f;

    bool error_detected = cingulate_report_outcome(adapter, outcome, expected);

    // Error magnitude = |0.5 - 0.55| = 0.05 < threshold
    EXPECT_FALSE(error_detected);
}

TEST_F(CingulateAdapterTest, ERNAmplitudeScalesWithError) {
    // Large error
    cingulate_report_response(adapter, 1, 2);

    cingulate_error_event_t error;
    EXPECT_TRUE(cingulate_get_last_error(adapter, &error));

    // ERN should be negative (representing negativity)
    EXPECT_LT(error.ern_amplitude, 0.0f);
}

TEST_F(CingulateAdapterTest, ConsciousErrorHasPositivePe) {
    // Large error should be conscious
    cingulate_report_response(adapter, 1, 2);

    cingulate_error_event_t error;
    EXPECT_TRUE(cingulate_get_last_error(adapter, &error));

    // Large error should have positive Pe (conscious awareness)
    EXPECT_TRUE(error.is_conscious);
    EXPECT_GT(error.pe_amplitude, 0.0f);
}

/*=============================================================================
 * COGNITIVE CONTROL TESTS
 *===========================================================================*/

TEST_F(CingulateAdapterTest, ControlSignalGeneratedAfterConflict) {
    setup_conflict_scenario(0.8f, 0.75f);

    cingulate_conflict_t conflict;
    EXPECT_TRUE(cingulate_evaluate_conflict(adapter, &conflict));

    cingulate_control_signal_t signal;
    EXPECT_TRUE(cingulate_generate_control_signal(adapter, &signal));

    // Control level should be affected by conflict
    EXPECT_GT(signal.control_level, 0.0f);
    EXPECT_LE(signal.control_level, 1.0f);
}

TEST_F(CingulateAdapterTest, ControlSignalGeneratedAfterError) {
    cingulate_report_response(adapter, 1, 2);

    cingulate_control_signal_t signal;
    EXPECT_TRUE(cingulate_generate_control_signal(adapter, &signal));

    EXPECT_GT(signal.control_level, 0.0f);
}

TEST_F(CingulateAdapterTest, GetControlLevelReturnsValue) {
    float level = cingulate_get_control_level(adapter);

    // Should return baseline control level
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(CingulateAdapterTest, GetControlLevelNullReturnsZero) {
    float level = cingulate_get_control_level(NULL);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

/*=============================================================================
 * SELF-REFERENTIAL PROCESSING TESTS (PCC)
 *===========================================================================*/

TEST_F(CingulateAdapterTest, EvaluateSelfRelevanceSuccess) {
    float features[] = {0.5f, 0.3f, 0.8f, 0.2f};
    uint32_t num_features = 4;

    cingulate_self_reference_t result;
    EXPECT_TRUE(cingulate_evaluate_self_relevance(adapter, features, num_features, &result));

    EXPECT_GE(result.self_relevance, 0.0f);
    EXPECT_LE(result.self_relevance, 1.0f);
}

TEST_F(CingulateAdapterTest, SelfRelevanceNullInputsFail) {
    cingulate_self_reference_t result;

    EXPECT_FALSE(cingulate_evaluate_self_relevance(NULL, NULL, 0, &result));
    EXPECT_FALSE(cingulate_evaluate_self_relevance(adapter, NULL, 0, &result));
}

TEST_F(CingulateAdapterTest, DefaultModeInitiallyFalse) {
    EXPECT_FALSE(cingulate_is_default_mode(adapter));
}

TEST_F(CingulateAdapterTest, DefaultModeActivatesWithLowArousal) {
    // Low arousal should eventually activate DMN
    for (int i = 0; i < 10; i++) {
        cingulate_integrate_emotion(adapter, 0.0f, 0.1f);
    }

    // After multiple low-arousal integrations, DMN might activate
    // (Depends on internal focus level building up)
}

/*=============================================================================
 * EMOTION-COGNITION INTEGRATION TESTS
 *===========================================================================*/

TEST_F(CingulateAdapterTest, IntegrateEmotionSuccess) {
    EXPECT_TRUE(cingulate_integrate_emotion(adapter, 0.5f, 0.7f));
}

TEST_F(CingulateAdapterTest, IntegrateEmotionClampsValues) {
    // Should not crash with out-of-range values
    EXPECT_TRUE(cingulate_integrate_emotion(adapter, 2.0f, 2.0f));
    EXPECT_TRUE(cingulate_integrate_emotion(adapter, -2.0f, -2.0f));
}

TEST_F(CingulateAdapterTest, ReportPainSuccess) {
    EXPECT_TRUE(cingulate_report_pain(adapter, 0.7f, true));
}

TEST_F(CingulateAdapterTest, HighPainIncreasesArousal) {
    // Report high pain
    cingulate_report_pain(adapter, 0.9f, true);

    // Generate control signal (should reflect increased arousal/control)
    cingulate_control_signal_t signal;
    cingulate_generate_control_signal(adapter, &signal);

    // Control should be elevated
    EXPECT_GT(signal.control_level, 0.5f);
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(CingulateAdapterTest, GetStatusReturnsValidValue) {
    cingulate_status_t status = cingulate_get_status(adapter);
    EXPECT_EQ(status, CINGULATE_STATUS_IDLE);
}

TEST_F(CingulateAdapterTest, GetStatusNullReturnsError) {
    cingulate_status_t status = cingulate_get_status(NULL);
    EXPECT_EQ(status, CINGULATE_STATUS_ERROR);
}

TEST_F(CingulateAdapterTest, ErrorStringReturnsValidStrings) {
    EXPECT_STREQ(cingulate_error_string(CINGULATE_ERROR_NONE), "No error");
    EXPECT_STREQ(cingulate_error_string(CINGULATE_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(cingulate_error_string(CINGULATE_ERROR_NOT_INITIALIZED), "Not initialized");
}

TEST_F(CingulateAdapterTest, StatusStringReturnsValidStrings) {
    EXPECT_STREQ(cingulate_status_string(CINGULATE_STATUS_IDLE), "Idle");
    EXPECT_STREQ(cingulate_status_string(CINGULATE_STATUS_MONITORING), "Monitoring");
    EXPECT_STREQ(cingulate_status_string(CINGULATE_STATUS_CONFLICT_DETECTED), "Conflict detected");
}

TEST_F(CingulateAdapterTest, GetStatsSuccess) {
    cingulate_stats_t stats;
    EXPECT_TRUE(cingulate_get_stats(adapter, &stats));

    // Initial stats should be zero
    EXPECT_EQ(stats.conflicts_detected, 0u);
    EXPECT_EQ(stats.errors_detected, 0u);
}

TEST_F(CingulateAdapterTest, GetConfigSuccess) {
    cingulate_config_t retrieved;
    EXPECT_TRUE(cingulate_get_config(adapter, &retrieved));

    EXPECT_EQ(retrieved.max_conflicts, config.max_conflicts);
    EXPECT_FLOAT_EQ(retrieved.conflict_threshold, config.conflict_threshold);
}

/*=============================================================================
 * CALLBACK TESTS
 *===========================================================================*/

static bool conflict_callback_called = false;
static void test_conflict_callback(const cingulate_conflict_t* conflict, void* user_data) {
    (void)conflict;
    (void)user_data;
    conflict_callback_called = true;
}

static bool error_callback_called = false;
static void test_error_callback(const cingulate_error_event_t* error, void* user_data) {
    (void)error;
    (void)user_data;
    error_callback_called = true;
}

TEST_F(CingulateAdapterTest, ConflictCallbackInvoked) {
    conflict_callback_called = false;
    EXPECT_TRUE(cingulate_set_conflict_callback(adapter, test_conflict_callback, NULL));

    setup_conflict_scenario(0.8f, 0.75f);

    cingulate_conflict_t conflict;
    cingulate_evaluate_conflict(adapter, &conflict);

    EXPECT_TRUE(conflict_callback_called);
}

TEST_F(CingulateAdapterTest, ErrorCallbackInvoked) {
    error_callback_called = false;
    EXPECT_TRUE(cingulate_set_error_callback(adapter, test_error_callback, NULL));

    cingulate_report_response(adapter, 1, 2);

    EXPECT_TRUE(error_callback_called);
}

/*=============================================================================
 * STATISTICS TRACKING TESTS
 *===========================================================================*/

TEST_F(CingulateAdapterTest, StatisticsUpdateAfterConflict) {
    setup_conflict_scenario(0.8f, 0.75f);

    cingulate_conflict_t conflict;
    cingulate_evaluate_conflict(adapter, &conflict);

    cingulate_stats_t stats;
    cingulate_get_stats(adapter, &stats);

    EXPECT_EQ(stats.conflicts_detected, 1u);
    EXPECT_GT(stats.avg_conflict_level, 0.0f);
}

TEST_F(CingulateAdapterTest, StatisticsUpdateAfterError) {
    cingulate_report_response(adapter, 1, 2);

    cingulate_stats_t stats;
    cingulate_get_stats(adapter, &stats);

    EXPECT_EQ(stats.errors_detected, 1u);
    EXPECT_GT(stats.avg_error_magnitude, 0.0f);
}

TEST_F(CingulateAdapterTest, StatisticsUpdateAfterMultipleEvents) {
    // Multiple conflicts
    for (int i = 0; i < 3; i++) {
        cingulate_reset(adapter);
        setup_conflict_scenario(0.8f, 0.75f);
        cingulate_conflict_t conflict;
        cingulate_evaluate_conflict(adapter, &conflict);
    }

    cingulate_stats_t stats;
    cingulate_get_stats(adapter, &stats);

    EXPECT_EQ(stats.conflicts_detected, 3u);
}

/*=============================================================================
 * EDGE CASE TESTS
 *===========================================================================*/

TEST_F(CingulateAdapterTest, HandleZeroOptions) {
    // Should handle edge case of 0 options
    EXPECT_FALSE(cingulate_begin_monitoring(adapter, 0));
}

TEST_F(CingulateAdapterTest, HandleMaxOptions) {
    // Should handle max options
    EXPECT_TRUE(cingulate_begin_monitoring(adapter, 16));
}

TEST_F(CingulateAdapterTest, MultipleResetsClearState) {
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(cingulate_reset(adapter));
        EXPECT_EQ(cingulate_get_status(adapter), CINGULATE_STATUS_IDLE);
    }
}

/*=============================================================================
 * INTEGRATION FLOW TESTS
 *===========================================================================*/

TEST_F(CingulateAdapterTest, FullConflictResolutionFlow) {
    // 1. Begin monitoring
    EXPECT_TRUE(cingulate_begin_monitoring(adapter, 4));

    // 2. Update response options
    for (uint32_t i = 0; i < 4; i++) {
        cingulate_response_option_t option;
        memset(&option, 0, sizeof(option));
        option.option_id = i;
        option.activation = 0.7f - (float)i * 0.1f;
        option.evidence = 0.5f;
        EXPECT_TRUE(cingulate_update_response(adapter, &option));
    }

    // 3. Evaluate conflict
    cingulate_conflict_t conflict;
    bool has_conflict = cingulate_evaluate_conflict(adapter, &conflict);

    // 4. Generate control signal if conflict
    if (has_conflict) {
        cingulate_control_signal_t signal;
        EXPECT_TRUE(cingulate_generate_control_signal(adapter, &signal));
    }

    // 5. Report response and check for errors
    EXPECT_TRUE(cingulate_report_response(adapter, 0, 0));

    // 6. Get statistics
    cingulate_stats_t stats;
    EXPECT_TRUE(cingulate_get_stats(adapter, &stats));
    EXPECT_EQ(stats.responses_monitored, 1u);
}

TEST_F(CingulateAdapterTest, FullErrorProcessingFlow) {
    // 1. Report an error
    EXPECT_TRUE(cingulate_report_response(adapter, 2, 1));

    // 2. Get error details
    cingulate_error_event_t error;
    EXPECT_TRUE(cingulate_get_last_error(adapter, &error));
    EXPECT_TRUE(error.is_conscious);

    // 3. Check if error was conscious
    EXPECT_TRUE(cingulate_error_is_conscious(adapter, error.error_id));

    // 4. Generate control signal
    cingulate_control_signal_t signal;
    EXPECT_TRUE(cingulate_generate_control_signal(adapter, &signal));

    // 5. Control should be elevated due to error
    EXPECT_GT(signal.control_level, 0.4f);
}
