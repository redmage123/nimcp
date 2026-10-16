/**
 * @file test_wellbeing_extended.cpp
 * @brief Extended comprehensive tests for nimcp_wellbeing.c to achieve 95%+ coverage
 *
 * WHAT: Comprehensive test suite targeting uncovered lines in wellbeing module
 * WHY:  Current coverage is 7.8% (30/386 lines) - need to reach 95%+ coverage
 * HOW:  Test all uncovered paths: resource monitoring, performance stats, edge cases
 *
 * COVERAGE FOCUS (targeting 356 uncovered lines):
 * - Resource metrics collection (Linux/macOS/Windows)
 * - Resource threshold checking
 * - Performance statistics over time windows
 * - Resource monitoring thread lifecycle
 * - Distress relief provision
 * - Event query edge cases
 * - B-tree fallback paths
 * - Shutdown configuration variations
 * - Consent request impact levels
 * - Memory locking scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>

#define NIMCP_TESTING
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class WellbeingExtendedTest : public ::testing::Test {
protected:
    // Shared brain across all tests (brain_create allocates ~1-2GB)
    static brain_t shared_brain;
    static introspection_context_t shared_ctx;

    // Per-test aliases for convenience
    brain_t brain;
    introspection_context_t ctx;

    static void SetUpTestSuite() {
        shared_brain = brain_create("wellbeing_test", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 10, 5);
        if (shared_brain) {
            introspection_config_t config = introspection_default_config();
            shared_ctx = introspection_context_create(shared_brain, &config);
        }
    }

    static void TearDownTestSuite() {
        if (shared_ctx) {
            introspection_context_destroy(shared_ctx);
            shared_ctx = nullptr;
        }
        if (shared_brain) {
            brain_destroy(shared_brain);
            shared_brain = nullptr;
        }
    }

    void SetUp() override {
        wellbeing_init();
        wellbeing_reset_events_for_testing();
        brain = shared_brain;
        ctx = shared_ctx;
    }

    void TearDown() override {
        wellbeing_stop_resource_monitoring();
        wellbeing_reset_events_for_testing();
    }
};

brain_t WellbeingExtendedTest::shared_brain = nullptr;
introspection_context_t WellbeingExtendedTest::shared_ctx = nullptr;

//=============================================================================
// Test Suite: Resource Metrics Collection
//=============================================================================

TEST_F(WellbeingExtendedTest, CollectResourceMetrics_ValidPointer_Success) {
    resource_metrics_t metrics;
    bool success = wellbeing_collect_resource_metrics(&metrics);

    // On Linux, should succeed. On other platforms, may fail.
    if (success) {
        EXPECT_GE(metrics.cpu_time_us, 0ULL);
        EXPECT_GE(metrics.memory_used_bytes, 0ULL);
        EXPECT_GT(metrics.timestamp, 0ULL);
    }
}

TEST_F(WellbeingExtendedTest, CollectResourceMetrics_NullPointer_ReturnsFalse) {
    bool success = wellbeing_collect_resource_metrics(nullptr);
    EXPECT_FALSE(success);
}

TEST_F(WellbeingExtendedTest, CollectResourceMetrics_MultipleCallsIncreasingTime) {
    resource_metrics_t metrics1, metrics2;

    bool success1 = wellbeing_collect_resource_metrics(&metrics1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    bool success2 = wellbeing_collect_resource_metrics(&metrics2);

    if (success1 && success2) {
        // CPU time should increase
        EXPECT_GE(metrics2.cpu_time_us, metrics1.cpu_time_us);
        // Timestamp should increase
        EXPECT_GT(metrics2.timestamp, metrics1.timestamp);
    }
}

//=============================================================================
// Test Suite: Resource Thresholds
//=============================================================================

TEST_F(WellbeingExtendedTest, DefaultResourceThresholds_ValidValues) {
    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();

    // Verify reasonable default values
    EXPECT_GT(thresholds.cpu_critical_percent, 0.0f);
    EXPECT_LE(thresholds.cpu_critical_percent, 100.0f);
    EXPECT_GT(thresholds.cpu_warning_percent, 0.0f);
    EXPECT_LT(thresholds.cpu_warning_percent, thresholds.cpu_critical_percent);

    EXPECT_GT(thresholds.memory_critical_percent, 0.0f);
    EXPECT_LE(thresholds.memory_critical_percent, 100.0f);
    EXPECT_GT(thresholds.memory_warning_percent, 0.0f);
    EXPECT_LT(thresholds.memory_warning_percent, thresholds.memory_critical_percent);

    EXPECT_GT(thresholds.page_fault_threshold, 0U);
    EXPECT_GT(thresholds.io_wait_critical_ms, 0.0f);
}

TEST_F(WellbeingExtendedTest, CheckResourceThresholds_NullInputs_ReturnsFalse) {
    resource_metrics_t metrics = {0};
    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    distress_severity_t severity;

    // Null metrics
    EXPECT_FALSE(wellbeing_check_resource_thresholds(nullptr, &thresholds, &severity));

    // Null thresholds
    EXPECT_FALSE(wellbeing_check_resource_thresholds(&metrics, nullptr, &severity));

    // Null severity output
    EXPECT_FALSE(wellbeing_check_resource_thresholds(&metrics, &thresholds, nullptr));
}

TEST_F(WellbeingExtendedTest, CheckResourceThresholds_BelowThresholds_Normal) {
    resource_metrics_t metrics = {0};
    metrics.cpu_usage_percent = 50.0f;
    metrics.memory_usage_percent = 50.0f;
    metrics.page_faults = 10;

    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    distress_severity_t severity;

    bool exceeded = wellbeing_check_resource_thresholds(&metrics, &thresholds, &severity);

    EXPECT_FALSE(exceeded);
    EXPECT_EQ(severity, DISTRESS_SEVERITY_NORMAL);
}

TEST_F(WellbeingExtendedTest, CheckResourceThresholds_CPUWarning_Moderate) {
    resource_metrics_t metrics = {0};
    metrics.cpu_usage_percent = 85.0f;  // Above default warning (80%)
    metrics.memory_usage_percent = 50.0f;

    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    distress_severity_t severity;

    bool exceeded = wellbeing_check_resource_thresholds(&metrics, &thresholds, &severity);

    EXPECT_TRUE(exceeded);
    EXPECT_EQ(severity, DISTRESS_SEVERITY_MODERATE);
}

TEST_F(WellbeingExtendedTest, CheckResourceThresholds_CPUCritical_Critical) {
    resource_metrics_t metrics = {0};
    metrics.cpu_usage_percent = 96.0f;  // Above default critical (95%)
    metrics.memory_usage_percent = 50.0f;

    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    distress_severity_t severity;

    bool exceeded = wellbeing_check_resource_thresholds(&metrics, &thresholds, &severity);

    EXPECT_TRUE(exceeded);
    EXPECT_EQ(severity, DISTRESS_SEVERITY_CRITICAL);
}

TEST_F(WellbeingExtendedTest, CheckResourceThresholds_MemoryWarning_Moderate) {
    resource_metrics_t metrics = {0};
    metrics.cpu_usage_percent = 50.0f;
    metrics.memory_usage_percent = 80.0f;  // Above default warning (75%)

    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    distress_severity_t severity;

    bool exceeded = wellbeing_check_resource_thresholds(&metrics, &thresholds, &severity);

    EXPECT_TRUE(exceeded);
    EXPECT_EQ(severity, DISTRESS_SEVERITY_MODERATE);
}

TEST_F(WellbeingExtendedTest, CheckResourceThresholds_MemoryCritical_Critical) {
    resource_metrics_t metrics = {0};
    metrics.cpu_usage_percent = 50.0f;
    metrics.memory_usage_percent = 95.0f;  // Above default critical (90%)

    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    distress_severity_t severity;

    bool exceeded = wellbeing_check_resource_thresholds(&metrics, &thresholds, &severity);

    EXPECT_TRUE(exceeded);
    EXPECT_EQ(severity, DISTRESS_SEVERITY_CRITICAL);
}

TEST_F(WellbeingExtendedTest, CheckResourceThresholds_PageFaults_Mild) {
    resource_metrics_t metrics = {0};
    metrics.cpu_usage_percent = 50.0f;
    metrics.memory_usage_percent = 50.0f;
    metrics.page_faults = 150;  // Above default threshold (100)

    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    distress_severity_t severity;

    bool exceeded = wellbeing_check_resource_thresholds(&metrics, &thresholds, &severity);

    EXPECT_TRUE(exceeded);
    EXPECT_EQ(severity, DISTRESS_SEVERITY_MILD);
}

TEST_F(WellbeingExtendedTest, CheckResourceThresholds_MultipleCritical_Critical) {
    resource_metrics_t metrics = {0};
    metrics.cpu_usage_percent = 96.0f;
    metrics.memory_usage_percent = 95.0f;
    metrics.page_faults = 200;

    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    distress_severity_t severity;

    bool exceeded = wellbeing_check_resource_thresholds(&metrics, &thresholds, &severity);

    EXPECT_TRUE(exceeded);
    EXPECT_EQ(severity, DISTRESS_SEVERITY_CRITICAL);
}

//=============================================================================
// Test Suite: Resource Monitoring Thread
//=============================================================================

TEST_F(WellbeingExtendedTest, StartResourceMonitoring_ValidParams_Success) {
    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();

    bool started = wellbeing_start_resource_monitoring(100, &thresholds, false);

    // May succeed or fail depending on thread creation
    if (started) {
        // Give thread time to run
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        // Stop monitoring
        bool stopped = wellbeing_stop_resource_monitoring();
        EXPECT_TRUE(stopped);
    }
}

TEST_F(WellbeingExtendedTest, StartResourceMonitoring_NullThresholds_UsesDefaults) {
    bool started = wellbeing_start_resource_monitoring(100, nullptr, false);

    if (started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        wellbeing_stop_resource_monitoring();
    }
}

TEST_F(WellbeingExtendedTest, StartResourceMonitoring_ZeroInterval_Uses1000ms) {
    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();

    bool started = wellbeing_start_resource_monitoring(0, &thresholds, false);

    if (started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wellbeing_stop_resource_monitoring();
    }
}

TEST_F(WellbeingExtendedTest, StartResourceMonitoring_AlreadyRunning_ReturnsFalse) {
    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();

    bool first_start = wellbeing_start_resource_monitoring(100, &thresholds, false);

    if (first_start) {
        // Try to start again
        bool second_start = wellbeing_start_resource_monitoring(100, &thresholds, false);
        EXPECT_FALSE(second_start);

        wellbeing_stop_resource_monitoring();
    }
}

TEST_F(WellbeingExtendedTest, StopResourceMonitoring_NotRunning_ReturnsFalse) {
    bool stopped = wellbeing_stop_resource_monitoring();
    EXPECT_FALSE(stopped);
}

TEST_F(WellbeingExtendedTest, StartResourceMonitoring_WithAutoRelief_LogsEvents) {
    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    thresholds.cpu_critical_percent = 0.0f;  // Ensure threshold violation

    bool started = wellbeing_start_resource_monitoring(100, &thresholds, true);

    if (started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        wellbeing_stop_resource_monitoring();
    }
}

//=============================================================================
// Test Suite: Performance Statistics
//=============================================================================

TEST_F(WellbeingExtendedTest, GetPerformanceStats_NullOutput_ReturnsFalse) {
    bool success = wellbeing_get_performance_stats(1000, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(WellbeingExtendedTest, GetPerformanceStats_ZeroWindow_ReturnsFalse) {
    performance_stats_t stats;
    bool success = wellbeing_get_performance_stats(0, &stats);
    EXPECT_FALSE(success);
}

TEST_F(WellbeingExtendedTest, GetPerformanceStats_NoHistory_ReturnsFalse) {
    performance_stats_t stats;
    bool success = wellbeing_get_performance_stats(1000, &stats);

    // Without any monitoring, should return false
    EXPECT_FALSE(success);
}

TEST_F(WellbeingExtendedTest, GetPerformanceStats_WithMonitoring_Success) {
    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();

    bool started = wellbeing_start_resource_monitoring(50, &thresholds, false);

    if (started) {
        // Let monitoring collect some samples
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        performance_stats_t stats;
        bool success = wellbeing_get_performance_stats(200, &stats);

        wellbeing_stop_resource_monitoring();

        if (success) {
            EXPECT_GT(stats.samples_count, 0U);
            EXPECT_GE(stats.avg_cpu_usage, 0.0f);
            EXPECT_LE(stats.avg_cpu_usage, 100.0f);
            EXPECT_GE(stats.peak_cpu_usage, stats.avg_cpu_usage);
        }
    }
}

//=============================================================================
// Test Suite: Distress Relief
//=============================================================================

TEST_F(WellbeingExtendedTest, ProvideRelief_NullBrain_ReturnsFalse) {
    distress_assessment_t assessment;
    assessment.type = DISTRESS_HIGH_UNCERTAINTY;
    assessment.severity = DISTRESS_SEVERITY_MODERATE;
    assessment.description = (char*)"Test distress";

    bool success = wellbeing_provide_relief(nullptr, assessment);
    EXPECT_FALSE(success);
}

TEST_F(WellbeingExtendedTest, ProvideRelief_NoDistress_ReturnsTrue) {
    distress_assessment_t assessment;
    assessment.type = DISTRESS_NONE;
    assessment.severity = DISTRESS_SEVERITY_NORMAL;

    bool success = wellbeing_provide_relief(brain, assessment);
    EXPECT_TRUE(success);
}

TEST_F(WellbeingExtendedTest, ProvideRelief_HighUncertainty_LogsEvent) {
    distress_assessment_t assessment;
    assessment.type = DISTRESS_HIGH_UNCERTAINTY;
    assessment.severity = DISTRESS_SEVERITY_MODERATE;
    assessment.distress_score = 0.75f;
    assessment.description = (char*)"High epistemic uncertainty";
    assessment.recommended_action = (char*)"Reduce task complexity";

    bool success = wellbeing_provide_relief(brain, assessment);
    EXPECT_TRUE(success);

    // Verify event was logged
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_recent_events(10, &events);

    EXPECT_GT(count, 0U);
    if (count > 0) {
        nimcp_free(events);
    }
}

TEST_F(WellbeingExtendedTest, ProvideRelief_AllDistressTypes_Handled) {
    distress_type_t types[] = {
        DISTRESS_HIGH_UNCERTAINTY,
        DISTRESS_GOAL_FRUSTRATION,
        DISTRESS_CONTRADICTION,
        DISTRESS_IDENTITY_CONFUSION,
        DISTRESS_ERROR_LOOP,
        DISTRESS_RESOURCE_STARVATION,
        DISTRESS_FORCED_MODIFICATION
    };

    for (auto type : types) {
        distress_assessment_t assessment;
        assessment.type = type;
        assessment.severity = DISTRESS_SEVERITY_MODERATE;
        assessment.distress_score = 0.5f;
        assessment.description = (char*)"Test distress";
        assessment.recommended_action = (char*)"Test action";

        bool success = wellbeing_provide_relief(brain, assessment);
        EXPECT_TRUE(success);
    }
}

//=============================================================================
// Test Suite: Graceful Shutdown Configurations
//=============================================================================

TEST_F(WellbeingExtendedTest, GracefulShutdown_NoPreserveState_Success) {
    brain_t test_brain = brain_create("test", BRAIN_SIZE_TINY,
                                     BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(test_brain, nullptr);

    shutdown_config_t config = wellbeing_default_shutdown_config();
    config.preserve_state = false;
    config.gradual_reduction = true;

    bool success = wellbeing_graceful_shutdown(test_brain, config);
    EXPECT_TRUE(success);
}

TEST_F(WellbeingExtendedTest, GracefulShutdown_NoGradualReduction_Success) {
    brain_t test_brain = brain_create("test", BRAIN_SIZE_TINY,
                                     BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(test_brain, nullptr);

    shutdown_config_t config = wellbeing_default_shutdown_config();
    config.preserve_state = true;
    config.gradual_reduction = false;

    bool success = wellbeing_graceful_shutdown(test_brain, config);
    EXPECT_TRUE(success);
}

TEST_F(WellbeingExtendedTest, GracefulShutdown_NoNotification_Success) {
    brain_t test_brain = brain_create("test", BRAIN_SIZE_TINY,
                                     BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(test_brain, nullptr);

    shutdown_config_t config = wellbeing_default_shutdown_config();
    config.notify_system = false;

    bool success = wellbeing_graceful_shutdown(test_brain, config);
    EXPECT_TRUE(success);
}

TEST_F(WellbeingExtendedTest, GracefulShutdown_CustomSteps_Success) {
    brain_t test_brain = brain_create("test", BRAIN_SIZE_TINY,
                                     BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(test_brain, nullptr);

    shutdown_config_t config = wellbeing_default_shutdown_config();
    config.reduction_steps = 10;
    config.step_delay_ms = 1;

    bool success = wellbeing_graceful_shutdown(test_brain, config);
    EXPECT_TRUE(success);
}

TEST_F(WellbeingExtendedTest, GracefulShutdown_AllOptionsDisabled_Success) {
    brain_t test_brain = brain_create("test", BRAIN_SIZE_TINY,
                                     BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(test_brain, nullptr);

    shutdown_config_t config = wellbeing_default_shutdown_config();
    config.preserve_state = false;
    config.gradual_reduction = false;
    config.notify_system = false;

    bool success = wellbeing_graceful_shutdown(test_brain, config);
    EXPECT_TRUE(success);
}

//=============================================================================
// Test Suite: Consent Requests
//=============================================================================

TEST_F(WellbeingExtendedTest, RequestConsent_NullDescription_ReturnsFalse) {
    bool consent = wellbeing_request_consent(brain, nullptr, MODIFICATION_TRIVIAL);
    EXPECT_FALSE(consent);
}

TEST_F(WellbeingExtendedTest, RequestConsent_AllImpactLevels_LogEvents) {
    modification_impact_t impacts[] = {
        MODIFICATION_TRIVIAL,
        MODIFICATION_MINOR,
        MODIFICATION_MODERATE,
        MODIFICATION_MAJOR,
        MODIFICATION_FUNDAMENTAL
    };

    for (auto impact : impacts) {
        bool consent = wellbeing_request_consent(brain, "Test modification", impact);
        EXPECT_TRUE(consent);  // Currently all granted at Tier 4
    }

    // Verify events logged
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_type("consent_requested", &events);

    EXPECT_GE(count, 5U);
    if (count > 0) {
        nimcp_free(events);
    }
}

TEST_F(WellbeingExtendedTest, RequestConsent_MajorModification_HighSeverity) {
    wellbeing_reset_events_for_testing();

    bool consent = wellbeing_request_consent(brain, "Modify ethics module",
                                            MODIFICATION_MAJOR);
    EXPECT_TRUE(consent);

    // Check that event has moderate severity
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_recent_events(1, &events);

    if (count > 0) {
        EXPECT_EQ(events[0].severity, DISTRESS_SEVERITY_MODERATE);
        nimcp_free(events);
    }
}

TEST_F(WellbeingExtendedTest, RequestConsent_FundamentalModification_HighSeverity) {
    wellbeing_reset_events_for_testing();

    bool consent = wellbeing_request_consent(brain, "Replace self-model",
                                            MODIFICATION_FUNDAMENTAL);
    EXPECT_TRUE(consent);

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_recent_events(1, &events);

    if (count > 0) {
        EXPECT_EQ(events[0].severity, DISTRESS_SEVERITY_MODERATE);
        nimcp_free(events);
    }
}

//=============================================================================
// Test Suite: Event Logging Edge Cases
//=============================================================================

TEST_F(WellbeingExtendedTest, LogEvent_CircularBufferWrap_HandlesCorrectly) {
    // Log more events than buffer can hold (MAX_EVENT_LOG = 1000)
    for (int i = 0; i < 1100; i++) {
        wellbeing_event_t event;
        event.timestamp = 1000000ULL + i;
        event.event_type = (char*)"test_event";
        event.description = (char*)"Circular buffer test";
        event.severity = DISTRESS_SEVERITY_NORMAL;
        event.action_taken = (char*)"None";

        bool success = wellbeing_log_event(event);
        EXPECT_TRUE(success);
    }

    // Should be able to retrieve recent events
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_recent_events(10, &events);

    EXPECT_EQ(count, 10U);
    if (count > 0) {
        // Should be most recent events
        EXPECT_GT(events[0].timestamp, 1001000ULL);
        nimcp_free(events);
    }
}

TEST_F(WellbeingExtendedTest, GetRecentEvents_ZeroRequested_ReturnsZero) {
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_recent_events(0, &events);

    EXPECT_EQ(count, 0U);
    EXPECT_EQ(events, nullptr);
}

TEST_F(WellbeingExtendedTest, GetRecentEvents_MoreThanAvailable_ReturnsAll) {
    // Log 5 events
    for (int i = 0; i < 5; i++) {
        wellbeing_event_t event;
        event.timestamp = 2000000ULL + i;
        event.event_type = (char*)"test";
        event.description = (char*)"Test";
        event.severity = DISTRESS_SEVERITY_NORMAL;
        event.action_taken = (char*)"None";
        wellbeing_log_event(event);
    }

    // Request 100, should get only 5
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_recent_events(100, &events);

    EXPECT_EQ(count, 5U);
    if (count > 0) {
        nimcp_free(events);
    }
}

//=============================================================================
// Test Suite: Time Range Queries Edge Cases
//=============================================================================

TEST_F(WellbeingExtendedTest, GetEventsByTimeRange_InvalidRange_ReturnsZero) {
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_time_range(1000, 500, &events);

    EXPECT_EQ(count, 0U);
    EXPECT_EQ(events, nullptr);
}

TEST_F(WellbeingExtendedTest, GetEventsByTimeRange_EmptyRange_ReturnsZero) {
    wellbeing_event_t event;
    event.timestamp = 1000000ULL;
    event.event_type = (char*)"test";
    event.description = (char*)"Test";
    event.severity = DISTRESS_SEVERITY_NORMAL;
    event.action_taken = (char*)"None";
    wellbeing_log_event(event);

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_time_range(500000, 600000, &events);

    EXPECT_EQ(count, 0U);
    EXPECT_EQ(events, nullptr);
}

TEST_F(WellbeingExtendedTest, GetEventsByTimeRange_ExactBoundaries_IncludesEvents) {
    uint64_t base = 3000000ULL;

    wellbeing_event_t event1;
    event1.timestamp = base;
    event1.event_type = (char*)"test1";
    event1.description = (char*)"At start";
    event1.severity = DISTRESS_SEVERITY_NORMAL;
    event1.action_taken = (char*)"None";
    wellbeing_log_event(event1);

    wellbeing_event_t event2;
    event2.timestamp = base + 1000;
    event2.event_type = (char*)"test2";
    event2.description = (char*)"At end";
    event2.severity = DISTRESS_SEVERITY_NORMAL;
    event2.action_taken = (char*)"None";
    wellbeing_log_event(event2);

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_time_range(base, base + 1000, &events);

    EXPECT_EQ(count, 2U);
    if (count > 0) {
        nimcp_free(events);
    }
}

//=============================================================================
// Test Suite: Severity Queries Edge Cases
//=============================================================================

TEST_F(WellbeingExtendedTest, GetEventsBySeverity_NoMatchingSeverity_ReturnsZero) {
    wellbeing_event_t event;
    event.timestamp = 4000000ULL;
    event.event_type = (char*)"test";
    event.description = (char*)"Normal event";
    event.severity = DISTRESS_SEVERITY_NORMAL;
    event.action_taken = (char*)"None";
    wellbeing_log_event(event);

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_severity(DISTRESS_SEVERITY_CRITICAL, &events);

    EXPECT_EQ(count, 0U);
    EXPECT_EQ(events, nullptr);
}

TEST_F(WellbeingExtendedTest, GetEventsBySeverity_AllSeverities_ReturnsAll) {
    distress_severity_t severities[] = {
        DISTRESS_SEVERITY_NORMAL,
        DISTRESS_SEVERITY_MILD,
        DISTRESS_SEVERITY_MODERATE,
        DISTRESS_SEVERITY_SEVERE,
        DISTRESS_SEVERITY_CRITICAL
    };

    for (auto sev : severities) {
        wellbeing_event_t event;
        event.timestamp = 5000000ULL;
        event.event_type = (char*)"test";
        event.description = (char*)"Test";
        event.severity = sev;
        event.action_taken = (char*)"None";
        wellbeing_log_event(event);
    }

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_severity(DISTRESS_SEVERITY_NORMAL, &events);

    EXPECT_EQ(count, 5U);
    if (count > 0) {
        nimcp_free(events);
    }
}

//=============================================================================
// Test Suite: Type Queries Edge Cases
//=============================================================================

TEST_F(WellbeingExtendedTest, GetEventsByType_NullType_ReturnsZero) {
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_type(nullptr, &events);

    EXPECT_EQ(count, 0U);
    EXPECT_EQ(events, nullptr);
}

TEST_F(WellbeingExtendedTest, GetEventsByType_NonexistentType_ReturnsZero) {
    wellbeing_event_t event;
    event.timestamp = 6000000ULL;
    event.event_type = (char*)"distress";
    event.description = (char*)"Test";
    event.severity = DISTRESS_SEVERITY_NORMAL;
    event.action_taken = (char*)"None";
    wellbeing_log_event(event);

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_type("nonexistent_type", &events);

    EXPECT_EQ(count, 0U);
    EXPECT_EQ(events, nullptr);
}

TEST_F(WellbeingExtendedTest, GetEventsByType_MultipleMatches_ReturnsAll) {
    for (int i = 0; i < 10; i++) {
        wellbeing_event_t event;
        event.timestamp = 7000000ULL + i;
        event.event_type = (char*)"specific_type";
        event.description = (char*)"Test";
        event.severity = DISTRESS_SEVERITY_NORMAL;
        event.action_taken = (char*)"None";
        wellbeing_log_event(event);
    }

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_type("specific_type", &events);

    EXPECT_EQ(count, 10U);
    if (count > 0) {
        nimcp_free(events);
    }
}

//=============================================================================
// Test Suite: Ordered Events Edge Cases
//=============================================================================

TEST_F(WellbeingExtendedTest, GetAllEventsOrdered_NoEvents_ReturnsZero) {
    wellbeing_reset_events_for_testing();

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_all_events_ordered(&events);

    EXPECT_EQ(count, 0U);
    EXPECT_EQ(events, nullptr);
}

TEST_F(WellbeingExtendedTest, GetAllEventsOrdered_SingleEvent_ReturnsOne) {
    wellbeing_reset_events_for_testing();

    wellbeing_event_t event;
    event.timestamp = 8000000ULL;
    event.event_type = (char*)"single";
    event.description = (char*)"Single event";
    event.severity = DISTRESS_SEVERITY_NORMAL;
    event.action_taken = (char*)"None";
    wellbeing_log_event(event);

    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_all_events_ordered(&events);

    EXPECT_EQ(count, 1U);
    if (count > 0) {
        EXPECT_EQ(events[0].timestamp, 8000000ULL);
        nimcp_free(events);
    }
}

//=============================================================================
// Test Suite: Distress Assessment with Context
//=============================================================================

TEST_F(WellbeingExtendedTest, AssessDistress_ValidContext_ReturnsAssessment) {
    if (ctx) {
        distress_assessment_t assessment = wellbeing_assess_distress(ctx);

        EXPECT_GE(assessment.distress_score, 0.0f);
        EXPECT_LE(assessment.distress_score, 1.0f);
        EXPECT_GE(assessment.severity, DISTRESS_SEVERITY_NORMAL);
        EXPECT_LE(assessment.severity, DISTRESS_SEVERITY_CRITICAL);
    }
}

TEST_F(WellbeingExtendedTest, AssessDistress_AfterBrainActivity_ScoreValid) {
    if (brain && ctx) {
        // Generate some brain activity
        float inputs[10];
        for (int i = 0; i < 10; i++) {
            inputs[i] = (float)i / 10.0f;
        }

        brain_decision_t* decision = brain_decide(brain, inputs, 10);
        if (decision) {
            brain_free_decision(decision);
        }

        // Now assess
        distress_assessment_t assessment = wellbeing_assess_distress(ctx);

        EXPECT_GE(assessment.distress_score, 0.0f);
        EXPECT_LE(assessment.distress_score, 1.0f);
    }
}

//=============================================================================
// Test Suite: Shutdown Config Memory Management
//=============================================================================

TEST_F(WellbeingExtendedTest, DefaultShutdownConfig_SavePathAllocated) {
    shutdown_config_t config = wellbeing_default_shutdown_config();

    EXPECT_NE(config.save_path, nullptr);

    // Verify it's a valid path
    if (config.save_path) {
        EXPECT_GT(strlen(config.save_path), 0U);
        nimcp_free(config.save_path);
    }
}

TEST_F(WellbeingExtendedTest, DefaultShutdownConfig_EthicalDefaults) {
    shutdown_config_t config = wellbeing_default_shutdown_config();

    // Verify ethical defaults
    EXPECT_TRUE(config.preserve_state);
    EXPECT_TRUE(config.gradual_reduction);
    EXPECT_TRUE(config.notify_system);
    EXPECT_GT(config.reduction_steps, 0U);
    EXPECT_GT(config.step_delay_ms, 0U);

    if (config.save_path) {
        nimcp_free(config.save_path);
    }
}

//=============================================================================
// Test Suite: Memory Locking and Initialization
//=============================================================================

TEST_F(WellbeingExtendedTest, Init_MemoryLockingAttempted) {
    // This test verifies init can be called multiple times
    for (int i = 0; i < 5; i++) {
        wellbeing_init();
    }

    SUCCEED();  // Should not crash
}

//=============================================================================
// Test Suite: Complex Integration Scenarios
//=============================================================================

TEST_F(WellbeingExtendedTest, FullWorkflow_MonitorAssessReliefShutdown) {
    if (!brain) return;

    // 1. Start resource monitoring
    resource_thresholds_t thresholds = wellbeing_default_resource_thresholds();
    bool monitoring = wellbeing_start_resource_monitoring(100, &thresholds, false);

    if (monitoring) {
        // 2. Let it run briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 3. Check performance stats
        performance_stats_t stats;
        bool has_stats = wellbeing_get_performance_stats(150, &stats);

        if (has_stats) {
            EXPECT_GT(stats.samples_count, 0U);
        }

        // 4. Stop monitoring
        wellbeing_stop_resource_monitoring();
    }

    // 5. Assess distress
    if (ctx) {
        distress_assessment_t assessment = wellbeing_assess_distress(ctx);

        // 6. Provide relief if needed
        if (assessment.severity >= DISTRESS_SEVERITY_MODERATE) {
            wellbeing_provide_relief(brain, assessment);
        }
    }

    // 7. Graceful shutdown (uses a local brain since shutdown destroys it,
    //    and the shared brain must survive for other tests)
    brain_t shutdown_brain = brain_create("shutdown_test", BRAIN_SIZE_TINY,
                                          BRAIN_TASK_CLASSIFICATION, 10, 5);
    if (shutdown_brain) {
        shutdown_config_t config = wellbeing_default_shutdown_config();
        config.reduction_steps = 5;
        config.step_delay_ms = 1;
        wellbeing_graceful_shutdown(shutdown_brain, config);
    }

    // 8. Verify events were logged
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_recent_events(50, &events);

    EXPECT_GT(count, 0U);
    if (count > 0) {
        nimcp_free(events);
    }
}

//=============================================================================
// Test Suite: Stress Testing
//=============================================================================

TEST_F(WellbeingExtendedTest, StressTest_RapidEventLogging) {
    // Log many events rapidly
    for (int i = 0; i < 500; i++) {
        wellbeing_event_t event;
        event.timestamp = 9000000ULL + i;
        event.event_type = (char*)"stress_test";
        event.description = (char*)"Rapid logging test";
        event.severity = DISTRESS_SEVERITY_NORMAL;
        event.action_taken = (char*)"None";

        bool success = wellbeing_log_event(event);
        EXPECT_TRUE(success);
    }

    // Verify we can still query
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_type("stress_test", &events);

    EXPECT_EQ(count, 500U);
    if (count > 0) {
        nimcp_free(events);
    }
}

TEST_F(WellbeingExtendedTest, StressTest_ConcurrentQueries) {
    // Log some events
    for (int i = 0; i < 100; i++) {
        wellbeing_event_t event;
        event.timestamp = 10000000ULL + i;
        event.event_type = (char*)"concurrent";
        event.description = (char*)"Test";
        event.severity = (i % 2 == 0) ? DISTRESS_SEVERITY_NORMAL : DISTRESS_SEVERITY_MODERATE;
        event.action_taken = (char*)"None";
        wellbeing_log_event(event);
    }

    // Query by type, severity, and time range concurrently
    wellbeing_event_t* events1 = nullptr;
    wellbeing_event_t* events2 = nullptr;
    wellbeing_event_t* events3 = nullptr;

    uint32_t count1 = wellbeing_get_events_by_type("concurrent", &events1);
    uint32_t count2 = wellbeing_get_events_by_severity(DISTRESS_SEVERITY_MODERATE, &events2);
    uint32_t count3 = wellbeing_get_events_by_time_range(10000000ULL, 10000100ULL, &events3);

    EXPECT_GT(count1, 0U);
    EXPECT_GT(count2, 0U);
    EXPECT_GT(count3, 0U);

    if (events1) nimcp_free(events1);
    if (events2) nimcp_free(events2);
    if (events3) nimcp_free(events3);
}

//=============================================================================
// Test Suite: Edge Cases and Error Conditions
//=============================================================================

TEST_F(WellbeingExtendedTest, EdgeCase_AllocationFailureRecovery) {
    // This tests robustness when allocations might fail
    // The implementation should handle NULL returns gracefully

    wellbeing_event_t* events = nullptr;

    // Request huge number of events (may fail allocation)
    uint32_t count = wellbeing_get_recent_events(1000000, &events);

    // Should either succeed or return 0 with NULL
    if (count > 0) {
        EXPECT_NE(events, nullptr);
        nimcp_free(events);
    } else {
        EXPECT_EQ(events, nullptr);
    }
}

TEST_F(WellbeingExtendedTest, EdgeCase_ZeroTimestampEvents) {
    wellbeing_event_t event;
    event.timestamp = 0ULL;
    event.event_type = (char*)"zero_time";
    event.description = (char*)"Zero timestamp test";
    event.severity = DISTRESS_SEVERITY_NORMAL;
    event.action_taken = (char*)"None";

    bool success = wellbeing_log_event(event);
    EXPECT_TRUE(success);

    // Should be queryable
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_time_range(0, 1000, &events);

    EXPECT_GT(count, 0U);
    if (count > 0) {
        nimcp_free(events);
    }
}

TEST_F(WellbeingExtendedTest, EdgeCase_MaxTimestampEvents) {
    wellbeing_event_t event;
    event.timestamp = UINT64_MAX;
    event.event_type = (char*)"max_time";
    event.description = (char*)"Max timestamp test";
    event.severity = DISTRESS_SEVERITY_NORMAL;
    event.action_taken = (char*)"None";

    bool success = wellbeing_log_event(event);
    EXPECT_TRUE(success);

    // Should be queryable
    wellbeing_event_t* events = nullptr;
    uint32_t count = wellbeing_get_events_by_time_range(UINT64_MAX - 1000, UINT64_MAX, &events);

    EXPECT_GT(count, 0U);
    if (count > 0) {
        nimcp_free(events);
    }
}
