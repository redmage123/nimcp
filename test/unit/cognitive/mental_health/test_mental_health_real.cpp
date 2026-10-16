/**
 * @file test_mental_health_real.cpp
 * @brief REAL tests for nimcp_mental_health.c that exercise actual implementation
 *
 * DIFFERENCE FROM test_mental_health_coverage.cpp:
 * - Creates REAL brain instances
 * - Creates REAL mental health monitors
 * - Exercises actual implementation code paths
 * - NOT just NULL guards and config checks
 * - Uses primary_severity not overall_severity (struct at line 273-291)
 *
 * @date 2025-11-10
 */

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/nimcp_mental_health.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class MentalHealthRealTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    mental_health_monitor_t* monitor = nullptr;

    void SetUp() override {
        // Create a REAL brain instance (tiny size for testing)
        brain = brain_create("mental_health_test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";

        // Create REAL mental health monitor with default config
        monitor = mental_health_create_default();
        ASSERT_NE(monitor, nullptr) << "Failed to create mental health monitor";
    }

    void TearDown() override {
        // Clean up monitor
        if (monitor) {
            mental_health_destroy(monitor);
            monitor = nullptr;
        }

        // Clean up brain
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create valid config
    mental_health_config_t create_valid_config() {
        return mental_health_default_config();
    }
};

//=============================================================================
// Test Suite: REAL Monitor Creation and Configuration
//=============================================================================

TEST_F(MentalHealthRealTest, CreateMonitor_DefaultConfig) {
    // Already created in SetUp, verify it works
    EXPECT_NE(monitor, nullptr);
}

TEST_F(MentalHealthRealTest, CreateMonitor_CustomConfig) {
    mental_health_config_t config = create_valid_config();
    config.check_interval_decisions = 50;
    config.history_window_size = 500;
    config.enable_auto_intervention = true;

    mental_health_monitor_t* custom_monitor = mental_health_create(&config);
    ASSERT_NE(custom_monitor, nullptr);

    mental_health_destroy(custom_monitor);
}

TEST_F(MentalHealthRealTest, DefaultConfig_HasValidThresholds) {
    mental_health_config_t config = mental_health_default_config();

    // Verify thresholds are ordered correctly
    EXPECT_LT(config.mild_threshold, config.moderate_threshold);
    EXPECT_LT(config.moderate_threshold, config.severe_threshold);
    EXPECT_LT(config.severe_threshold, config.critical_threshold);
    EXPECT_LE(config.critical_threshold, 1.0f);
}

TEST_F(MentalHealthRealTest, DefaultConfig_EnabledByDefault) {
    mental_health_config_t config = mental_health_default_config();

    EXPECT_TRUE(config.enable_monitoring);
    EXPECT_GT(config.check_interval_decisions, 0U);
}

//=============================================================================
// Test Suite: REAL Monitoring with Brain
//=============================================================================

TEST_F(MentalHealthRealTest, Update_WithRealBrain) {
    // Call update with REAL brain (output = nullptr is acceptable for update)
    mental_health_update(monitor, brain, nullptr, 1000);

    SUCCEED();
}

TEST_F(MentalHealthRealTest, Check_WithRealBrain) {
    // Perform a check with real brain
    disorder_severity_t severity = mental_health_check(monitor, brain);

    // Should return valid severity (likely NONE for a fresh brain)
    EXPECT_GE(severity, DISORDER_SEVERITY_NONE);
    EXPECT_LE(severity, DISORDER_SEVERITY_CRITICAL);
}

TEST_F(MentalHealthRealTest, CheckSpecific_AllDisorders) {
    // Test checking specific disorders with real brain
    for (int i = 0; i < DISORDER_COUNT; i++) {
        float score = mental_health_check_specific(monitor, brain, (disorder_type_t)i);

        // Score should be in valid range [0.0, 1.0]
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 1.0f);
    }
}

TEST_F(MentalHealthRealTest, CheckSpecific_Sociopathy) {
    // Check specific disorder with real brain
    float score = mental_health_check_specific(monitor, brain, DISORDER_SOCIOPATHY);

    // Fresh brain should have low sociopathy score
    EXPECT_LT(score, 0.3f);
}

TEST_F(MentalHealthRealTest, CheckSpecific_Depression) {
    float score = mental_health_check_specific(monitor, brain, DISORDER_DEPRESSION);

    // Should return valid score
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(MentalHealthRealTest, CheckSpecific_Mania) {
    float score = mental_health_check_specific(monitor, brain, DISORDER_MANIA);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(MentalHealthRealTest, CheckSpecific_Anxiety) {
    float score = mental_health_check_specific(monitor, brain, DISORDER_ANXIETY);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(MentalHealthRealTest, CheckSpecific_ADHD) {
    float score = mental_health_check_specific(monitor, brain, DISORDER_ADHD);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

//=============================================================================
// Test Suite: REAL Classification
//=============================================================================

TEST_F(MentalHealthRealTest, ClassifySeverity_WithDefaultThresholds) {
    mental_health_config_t config = create_valid_config();

    // Test various severity levels
    EXPECT_EQ(mental_health_classify_severity(0.1f, &config), DISORDER_SEVERITY_NONE);
    EXPECT_EQ(mental_health_classify_severity(0.3f, &config), DISORDER_SEVERITY_MILD);
    EXPECT_EQ(mental_health_classify_severity(0.5f, &config), DISORDER_SEVERITY_MODERATE);
    EXPECT_EQ(mental_health_classify_severity(0.7f, &config), DISORDER_SEVERITY_SEVERE);
    EXPECT_EQ(mental_health_classify_severity(0.9f, &config), DISORDER_SEVERITY_CRITICAL);
}

TEST_F(MentalHealthRealTest, ClassifySeverity_BoundaryValues) {
    mental_health_config_t config = create_valid_config();

    EXPECT_EQ(mental_health_classify_severity(0.0f, &config), DISORDER_SEVERITY_NONE);
    EXPECT_EQ(mental_health_classify_severity(1.0f, &config), DISORDER_SEVERITY_CRITICAL);
}

//=============================================================================
// Test Suite: REAL Intervention
//=============================================================================

TEST_F(MentalHealthRealTest, Intervene_WithRealBrain) {
    // Attempt intervention with real brain
    bool success = mental_health_intervene(monitor, brain);

    // May succeed or not depending on whether intervention is needed
    // Just verify function doesn't crash
    (void)success;  // Suppress unused warning
    SUCCEED();
}

TEST_F(MentalHealthRealTest, ClearQuarantine_WithRealBrain) {
    // Clear quarantine with real brain
    mental_health_clear_quarantine(monitor, brain);

    // Should not crash
    SUCCEED();
}

//=============================================================================
// Test Suite: REAL Reporting
//=============================================================================

TEST_F(MentalHealthRealTest, GetReport_WithRealMonitor) {
    mental_health_report_t report;
    memset(&report, 0, sizeof(report));

    mental_health_get_report(monitor, &report);

    // Report should have valid structure with primary_severity (NOT overall_severity)
    EXPECT_GE(report.primary_severity, DISORDER_SEVERITY_NONE);
    EXPECT_LE(report.primary_severity, DISORDER_SEVERITY_CRITICAL);
}

TEST_F(MentalHealthRealTest, GetReport_AllScoresValid) {
    mental_health_report_t report;
    mental_health_get_report(monitor, &report);

    // All disorder scores should be in [0, 1]
    for (int i = 0; i < DISORDER_COUNT; i++) {
        EXPECT_GE(report.disorder_scores[i], 0.0f);
        EXPECT_LE(report.disorder_scores[i], 1.0f);
    }
}

TEST_F(MentalHealthRealTest, GetReport_AfterCheck) {
    // Perform check first
    mental_health_check(monitor, brain);

    mental_health_report_t report;
    mental_health_get_report(monitor, &report);

    EXPECT_GE(report.total_checks, 1U);
}

//=============================================================================
// Test Suite: REAL Statistics
//=============================================================================

TEST_F(MentalHealthRealTest, GetStats_WithRealMonitor) {
    mental_health_stats_t stats;
    bool success = mental_health_get_stats(monitor, &stats);

    EXPECT_TRUE(success);
    EXPECT_EQ(stats.total_decisions, 0U);  // Fresh monitor
    EXPECT_EQ(stats.total_checks, 0U);
    EXPECT_EQ(stats.total_interventions, 0U);
}

TEST_F(MentalHealthRealTest, ResetStats_WithRealMonitor) {
    // Do some operations first
    mental_health_check(monitor, brain);

    // Reset stats
    mental_health_reset_stats(monitor);

    // Verify stats are cleared
    mental_health_stats_t stats;
    mental_health_get_stats(monitor, &stats);
    EXPECT_EQ(stats.total_checks, 0U);
}

TEST_F(MentalHealthRealTest, GetStats_AfterOperations) {
    // Perform several checks
    for (int i = 0; i < 5; i++) {
        mental_health_check(monitor, brain);
    }

    mental_health_stats_t stats;
    bool success = mental_health_get_stats(monitor, &stats);

    EXPECT_TRUE(success);
    EXPECT_EQ(stats.total_checks, 5U);
}

//=============================================================================
// Test Suite: REAL Utility Functions
//=============================================================================

TEST_F(MentalHealthRealTest, DisorderToString_AllDisorders) {
    // Test a few key disorders
    const char* sociopathy = disorder_to_string(DISORDER_SOCIOPATHY);
    const char* depression = disorder_to_string(DISORDER_DEPRESSION);
    const char* adhd = disorder_to_string(DISORDER_ADHD);

    EXPECT_NE(sociopathy, nullptr);
    EXPECT_NE(depression, nullptr);
    EXPECT_NE(adhd, nullptr);
}

TEST_F(MentalHealthRealTest, SeverityToString_AllLevels) {
    EXPECT_NE(severity_to_string(DISORDER_SEVERITY_NONE), nullptr);
    EXPECT_NE(severity_to_string(DISORDER_SEVERITY_MILD), nullptr);
    EXPECT_NE(severity_to_string(DISORDER_SEVERITY_MODERATE), nullptr);
    EXPECT_NE(severity_to_string(DISORDER_SEVERITY_SEVERE), nullptr);
    EXPECT_NE(severity_to_string(DISORDER_SEVERITY_CRITICAL), nullptr);
}

//=============================================================================
// Test Suite: REAL Integration Workflow
//=============================================================================

TEST_F(MentalHealthRealTest, CompleteMonitoringWorkflow) {
    // 1. Update with decision
    mental_health_update(monitor, brain, nullptr, 1000);

    // 2. Check overall health
    disorder_severity_t severity = mental_health_check(monitor, brain);
    EXPECT_GE(severity, DISORDER_SEVERITY_NONE);

    // 3. Check specific disorder
    float score = mental_health_check_specific(monitor, brain, DISORDER_DEPRESSION);
    EXPECT_GE(score, 0.0f);

    // 4. Get report (using primary_severity)
    mental_health_report_t report;
    mental_health_get_report(monitor, &report);
    EXPECT_GE(report.primary_severity, DISORDER_SEVERITY_NONE);
    EXPECT_GE(report.primary_disorder, DISORDER_SOCIOPATHY);
    EXPECT_LT(report.primary_disorder, DISORDER_COUNT);

    // 5. Get statistics
    mental_health_stats_t stats;
    mental_health_get_stats(monitor, &stats);
    EXPECT_GT(stats.total_checks, 0U);
}

TEST_F(MentalHealthRealTest, MultipleUpdatesCycle) {
    // Simulate multiple decision cycles
    for (uint64_t time = 1000; time <= 10000; time += 1000) {
        mental_health_update(monitor, brain, nullptr, time);

        if (time % 3000 == 0) {
            mental_health_check(monitor, brain);
        }
    }

    mental_health_stats_t stats;
    mental_health_get_stats(monitor, &stats);
    EXPECT_GT(stats.total_checks, 0U);
}

//=============================================================================
// Test Suite: NULL Guards (still important for safety)
//=============================================================================

TEST_F(MentalHealthRealTest, NullGuard_CreateNull) {
    mental_health_monitor_t* null_monitor = mental_health_create(nullptr);
    EXPECT_EQ(null_monitor, nullptr);
}

TEST_F(MentalHealthRealTest, NullGuard_DestroyNull) {
    mental_health_destroy(nullptr);
    SUCCEED();
}

TEST_F(MentalHealthRealTest, NullGuard_UpdateNull) {
    mental_health_update(nullptr, brain, nullptr, 0);
    SUCCEED();
}

TEST_F(MentalHealthRealTest, NullGuard_CheckNull) {
    disorder_severity_t severity = mental_health_check(nullptr, brain);
    EXPECT_EQ(severity, DISORDER_SEVERITY_NONE);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
