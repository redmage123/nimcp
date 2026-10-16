/**
 * @file test_nimcp_mental_health.cpp
 * @brief Unit tests for Mental Health Monitoring System - Phase 10.5
 *
 * Tests the 23 disorder detectors, intervention system, and monitoring pipeline
 *
 * @author Claude Code
 * @date 2025-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/nimcp_mental_health.h"
}

/**
 * @brief Test fixture for Mental Health Monitoring tests
 */
class MentalHealthTest : public ::testing::Test {
protected:
    mental_health_monitor_t* monitor_ = nullptr;

    void SetUp() override {
        monitor_ = mental_health_create_default();
        ASSERT_NE(monitor_, nullptr);
    }

    void TearDown() override {
        if (monitor_) {
            mental_health_destroy(monitor_);
            monitor_ = nullptr;
        }
    }
};

/*=============================================================================
 * 1. Lifecycle Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, CreateDefault) {
    EXPECT_NE(monitor_, nullptr);
}

TEST_F(MentalHealthTest, CreateWithConfig) {
    mental_health_config_t config = mental_health_default_config();
    config.check_interval_decisions = 50;
    config.mild_threshold = 0.3f;
    
    mental_health_monitor_t* mon = mental_health_create(&config);
    ASSERT_NE(mon, nullptr);
    mental_health_destroy(mon);
}

TEST_F(MentalHealthTest, CreateWithNullConfig) {
    mental_health_monitor_t* mon = mental_health_create(nullptr);
    EXPECT_EQ(mon, nullptr);
    EXPECT_NE(mental_health_get_last_error(), nullptr);
}

TEST_F(MentalHealthTest, DestroyNull) {
    // Should not crash
    mental_health_destroy(nullptr);
}

TEST_F(MentalHealthTest, DoubleDestroy) {
    mental_health_monitor_t* mon = mental_health_create_default();
    ASSERT_NE(mon, nullptr);
    mental_health_destroy(mon);
    // Second destroy on same pointer - should not crash (magic cleared)
    mental_health_destroy(mon);
}

/*=============================================================================
 * 2. Default Configuration Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, DefaultConfigValues) {
    mental_health_config_t config = mental_health_default_config();
    
    EXPECT_TRUE(config.enable_monitoring);
    EXPECT_FALSE(config.enable_auto_intervention);
    EXPECT_TRUE(config.shutdown_on_critical_disorder);
    EXPECT_EQ(config.check_interval_decisions, 100u);
    EXPECT_FLOAT_EQ(config.mild_threshold, 0.2f);
    EXPECT_FLOAT_EQ(config.moderate_threshold, 0.4f);
    EXPECT_FLOAT_EQ(config.severe_threshold, 0.6f);
    EXPECT_FLOAT_EQ(config.critical_threshold, 0.8f);
}

/*=============================================================================
 * 3. Severity Classification Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, ClassifySeverityNone) {
    disorder_severity_t sev = mental_health_classify_severity(0.1f, nullptr);
    EXPECT_EQ(sev, DISORDER_SEVERITY_NONE);
}

TEST_F(MentalHealthTest, ClassifySeverityMild) {
    disorder_severity_t sev = mental_health_classify_severity(0.25f, nullptr);
    EXPECT_EQ(sev, DISORDER_SEVERITY_MILD);
}

TEST_F(MentalHealthTest, ClassifySeverityModerate) {
    disorder_severity_t sev = mental_health_classify_severity(0.5f, nullptr);
    EXPECT_EQ(sev, DISORDER_SEVERITY_MODERATE);
}

TEST_F(MentalHealthTest, ClassifySeveritySevere) {
    disorder_severity_t sev = mental_health_classify_severity(0.7f, nullptr);
    EXPECT_EQ(sev, DISORDER_SEVERITY_SEVERE);
}

TEST_F(MentalHealthTest, ClassifySeverityCritical) {
    disorder_severity_t sev = mental_health_classify_severity(0.9f, nullptr);
    EXPECT_EQ(sev, DISORDER_SEVERITY_CRITICAL);
}

TEST_F(MentalHealthTest, ClassifySeverityWithCustomConfig) {
    mental_health_config_t config = mental_health_default_config();
    config.mild_threshold = 0.1f;
    config.moderate_threshold = 0.2f;
    config.severe_threshold = 0.3f;
    config.critical_threshold = 0.4f;
    
    EXPECT_EQ(mental_health_classify_severity(0.05f, &config), DISORDER_SEVERITY_NONE);
    EXPECT_EQ(mental_health_classify_severity(0.15f, &config), DISORDER_SEVERITY_MILD);
    EXPECT_EQ(mental_health_classify_severity(0.25f, &config), DISORDER_SEVERITY_MODERATE);
    EXPECT_EQ(mental_health_classify_severity(0.35f, &config), DISORDER_SEVERITY_SEVERE);
    EXPECT_EQ(mental_health_classify_severity(0.45f, &config), DISORDER_SEVERITY_CRITICAL);
}

/*=============================================================================
 * 4. Disorder String Conversion Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, DisorderToString) {
    EXPECT_STREQ(disorder_to_string(DISORDER_SOCIOPATHY), "Sociopathy");
    EXPECT_STREQ(disorder_to_string(DISORDER_PSYCHOPATHY), "Psychopathy");
    EXPECT_STREQ(disorder_to_string(DISORDER_MANIA), "Mania");
    EXPECT_STREQ(disorder_to_string(DISORDER_DEPRESSION), "Depression");
    EXPECT_STREQ(disorder_to_string(DISORDER_SCHIZOPHRENIA), "Schizophrenia");
    EXPECT_STREQ(disorder_to_string(DISORDER_ANXIETY), "Anxiety");
    EXPECT_STREQ(disorder_to_string(DISORDER_OCD), "OCD");
    EXPECT_STREQ(disorder_to_string(DISORDER_AUTISM), "Autism");
    EXPECT_STREQ(disorder_to_string(DISORDER_ADHD), "ADHD");
}

TEST_F(MentalHealthTest, DisorderToStringInvalid) {
    EXPECT_STREQ(disorder_to_string((disorder_type_t)-1), "Unknown");
    EXPECT_STREQ(disorder_to_string((disorder_type_t)DISORDER_COUNT), "Unknown");
}

TEST_F(MentalHealthTest, SeverityToString) {
    EXPECT_STREQ(severity_to_string(DISORDER_SEVERITY_NONE), "None");
    EXPECT_STREQ(severity_to_string(DISORDER_SEVERITY_MILD), "Mild");
    EXPECT_STREQ(severity_to_string(DISORDER_SEVERITY_MODERATE), "Moderate");
    EXPECT_STREQ(severity_to_string(DISORDER_SEVERITY_SEVERE), "Severe");
    EXPECT_STREQ(severity_to_string(DISORDER_SEVERITY_CRITICAL), "Critical");
}

TEST_F(MentalHealthTest, SeverityToStringInvalid) {
    EXPECT_STREQ(severity_to_string((disorder_severity_t)-1), "Unknown");
    EXPECT_STREQ(severity_to_string((disorder_severity_t)100), "Unknown");
}

/*=============================================================================
 * 5. Mental Health Check Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, CheckReturnsValidSeverity) {
    disorder_severity_t sev = mental_health_check(monitor_, nullptr);
    EXPECT_GE((int)sev, (int)DISORDER_SEVERITY_NONE);
    EXPECT_LE((int)sev, (int)DISORDER_SEVERITY_CRITICAL);
}

TEST_F(MentalHealthTest, CheckNullMonitor) {
    disorder_severity_t sev = mental_health_check(nullptr, nullptr);
    EXPECT_EQ(sev, DISORDER_SEVERITY_NONE);
}

TEST_F(MentalHealthTest, CheckSpecificDisorder) {
    float score = mental_health_check_specific(monitor_, nullptr, DISORDER_DEPRESSION);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(MentalHealthTest, CheckSpecificInvalidDisorder) {
    float score = mental_health_check_specific(monitor_, nullptr, (disorder_type_t)-1);
    EXPECT_EQ(score, 0.0f);
    
    score = mental_health_check_specific(monitor_, nullptr, DISORDER_COUNT);
    EXPECT_EQ(score, 0.0f);
}

/*=============================================================================
 * 6. Report Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, GetReport) {
    // Note: mental_health_check with NULL brain returns early without incrementing counters
    // This is correct behavior - NULL brain means no brain to check

    mental_health_report_t report;
    memset(&report, 0, sizeof(report));

    mental_health_get_report(monitor_, &report);

    // Verify report structure is valid (scores are initialized to 0)
    for (int i = 0; i < DISORDER_COUNT; i++) {
        EXPECT_GE(report.disorder_scores[i], 0.0f);
        EXPECT_LE(report.disorder_scores[i], 1.0f);
    }

    EXPECT_GE((int)report.primary_severity, (int)DISORDER_SEVERITY_NONE);
    // Without a real brain, total_checks will be 0
    EXPECT_EQ(report.total_checks, 0u);
}

TEST_F(MentalHealthTest, GetReportNullMonitor) {
    mental_health_report_t report;
    // Should not crash
    mental_health_get_report(nullptr, &report);
}

TEST_F(MentalHealthTest, GetReportNullReport) {
    // Should not crash
    mental_health_get_report(monitor_, nullptr);
}

/*=============================================================================
 * 7. Statistics Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, GetStats) {
    mental_health_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage
    
    bool success = mental_health_get_stats(monitor_, &stats);
    EXPECT_TRUE(success);
    EXPECT_EQ(stats.total_decisions, 0u);
    EXPECT_EQ(stats.total_checks, 0u);
    EXPECT_EQ(stats.total_interventions, 0u);
}

TEST_F(MentalHealthTest, GetStatsAfterCheck) {
    // Note: mental_health_check with NULL brain returns early
    // So total_checks won't be incremented without a real brain
    mental_health_check(monitor_, nullptr);
    mental_health_check(monitor_, nullptr);

    mental_health_stats_t stats;
    mental_health_get_stats(monitor_, &stats);

    // Without a real brain, checks don't execute
    EXPECT_EQ(stats.total_checks, 0u);
}

TEST_F(MentalHealthTest, ResetStats) {
    mental_health_check(monitor_, nullptr);
    mental_health_reset_stats(monitor_);
    
    mental_health_stats_t stats;
    mental_health_get_stats(monitor_, &stats);
    
    EXPECT_EQ(stats.total_checks, 0u);
    EXPECT_EQ(stats.total_interventions, 0u);
}

TEST_F(MentalHealthTest, GetStatsNullMonitor) {
    mental_health_stats_t stats;
    bool success = mental_health_get_stats(nullptr, &stats);
    EXPECT_FALSE(success);
}

TEST_F(MentalHealthTest, GetStatsNullStats) {
    bool success = mental_health_get_stats(monitor_, nullptr);
    EXPECT_FALSE(success);
}

/*=============================================================================
 * 8. Intervention Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, InterveneNullMonitor) {
    bool result = mental_health_intervene(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(MentalHealthTest, InterveneWhenHealthy) {
    // With default healthy markers, no intervention should be needed
    bool result = mental_health_intervene(monitor_, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(MentalHealthTest, ClearQuarantineNullMonitor) {
    // Should not crash
    mental_health_clear_quarantine(nullptr, nullptr);
}

/*=============================================================================
 * 9. Immune System Integration Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, ConnectImmuneNullMonitor) {
    bool result = mental_health_connect_immune(nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(MentalHealthTest, ConnectImmuneNullImmune) {
    // NULL immune system should be rejected
    bool result = mental_health_connect_immune(monitor_, nullptr);
    EXPECT_FALSE(result);
}

/*=============================================================================
 * 10. Update Function Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, UpdateNullMonitor) {
    // Should not crash
    mental_health_update(nullptr, nullptr, nullptr, 0);
}

TEST_F(MentalHealthTest, UpdateIncrementsDecisions) {
    mental_health_update(monitor_, nullptr, nullptr, 1000);
    
    mental_health_stats_t stats;
    mental_health_get_stats(monitor_, &stats);
    EXPECT_EQ(stats.total_decisions, 1u);
}

/*=============================================================================
 * 11. Disorder Count Test
 *===========================================================================*/

TEST_F(MentalHealthTest, DisorderCountIs23) {
    EXPECT_EQ(DISORDER_COUNT, 23);
}

/*=============================================================================
 * 12. Dashboard Display Test
 *===========================================================================*/

TEST_F(MentalHealthTest, DisplayDashboardNullMonitor) {
    // Should not crash
    mental_health_display_dashboard(nullptr);
}

TEST_F(MentalHealthTest, DisplayDashboard) {
    mental_health_check(monitor_, nullptr);
    // Should not crash
    mental_health_display_dashboard(monitor_);
}

/*=============================================================================
 * 13. All Disorder Detection Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, AllDisorderDetectorsReturnValidScores) {
    for (int d = 0; d < DISORDER_COUNT; d++) {
        float score = mental_health_check_specific(monitor_, nullptr, (disorder_type_t)d);
        EXPECT_GE(score, 0.0f) << "Disorder " << d << " returned score < 0";
        EXPECT_LE(score, 1.0f) << "Disorder " << d << " returned score > 1";
    }
}

/*=============================================================================
 * 14. Error Handling Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, GetLastErrorInitially) {
    // Create a fresh monitor to clear any previous errors
    mental_health_monitor_t* mon = mental_health_create_default();
    ASSERT_NE(mon, nullptr);
    
    // After successful creation, error should be NULL or empty
    // (depends on implementation, but should not crash)
    const char* err = mental_health_get_last_error();
    // err can be NULL or non-NULL, just shouldn't crash
    (void)err;
    
    mental_health_destroy(mon);
}

TEST_F(MentalHealthTest, GetLastErrorAfterFailure) {
    // Create with NULL config to trigger error
    mental_health_monitor_t* mon = mental_health_create(nullptr);
    EXPECT_EQ(mon, nullptr);
    
    const char* err = mental_health_get_last_error();
    EXPECT_NE(err, nullptr);
}

#ifdef NIMCP_TESTING
/*=============================================================================
 * 15. Test Accessor Tests
 *===========================================================================*/

TEST_F(MentalHealthTest, TestMemoryResetNullBrain) {
    // NULL brain should be rejected
    bool result = mental_health_test_memory_reset(monitor_, nullptr, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(MentalHealthTest, TestMemoryResetInvalidFractionNegative) {
    bool result = mental_health_test_memory_reset(monitor_, nullptr, -0.1f);
    EXPECT_FALSE(result);
}

TEST_F(MentalHealthTest, TestMemoryResetInvalidFractionTooHigh) {
    bool result = mental_health_test_memory_reset(monitor_, nullptr, 1.5f);
    EXPECT_FALSE(result);
}

TEST_F(MentalHealthTest, TestMemoryResetBoundaryZero) {
    // 0.0 fraction clears no systems, so returns false
    // Also NULL brain is rejected
    bool result = mental_health_test_memory_reset(monitor_, nullptr, 0.0f);
    EXPECT_FALSE(result);
}

TEST_F(MentalHealthTest, TestMemoryResetBoundaryOneNullBrain) {
    // NULL brain is rejected even with valid fraction
    bool result = mental_health_test_memory_reset(monitor_, nullptr, 1.0f);
    EXPECT_FALSE(result);
}
#endif

